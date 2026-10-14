#include "qcompiler/arm64.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace qcompiler {

struct Arm64Generator::Context {
    std::string function_name;
    std::string exit_label;
    std::unordered_map<std::string, int> offsets;
    std::unordered_map<std::string, SymbolKind> kinds;
    int frame_size = 0;
    bool is_main = false;
};

namespace {

bool all_digits(const std::string &value, std::size_t start) {
    if (start >= value.size()) {
        return false;
    }
    for (std::size_t i = start; i < value.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

bool looks_like_label_name(const std::string &name) {
    return name.size() > 1 && name[0] == 'l' && all_digits(name, 1);
}

bool is_identifier_operand(const std::string &name) {
    if (name.empty()) {
        return false;
    }
    if (!(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) {
        return false;
    }
    for (char c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
            return false;
        }
    }
    return true;
}

void append_line(std::string &out, const std::string &line) {
    out += line;
    out += '\n';
}

template <typename ContextT>
void add_slot(ContextT &ctx, const std::string &name, int size_bytes, SymbolKind kind = SymbolKind::LocalScalar) {
    if (name.empty() || ctx.offsets.find(name) != ctx.offsets.end()) {
        return;
    }
    const int size = std::max(8, size_bytes);
    ctx.frame_size += size;
    ctx.offsets[name] = ctx.frame_size;
    ctx.kinds[name] = kind;
}

std::unordered_set<std::string> collect_labels(const std::vector<IRInstruction> &instructions) {
    std::unordered_set<std::string> labels;
    for (const auto &ins : instructions) {
        if (ins.op == IROp::Label || ins.op == IROp::Goto) {
            labels.insert(ins.dst);
        }
        if (ins.op == IROp::If) {
            labels.insert(ins.label_true);
            labels.insert(ins.label_false);
        }
    }
    return labels;
}

} // namespace

std::string Arm64Generator::generate(const IRProgram &program) const {
    std::string out;
    append_line(out, ".section .data");
    append_line(out, ".align 3");
    for (const auto &global : program.globals) {
        append_line(out, global.name + ":");
        append_line(out, "    .zero " + std::to_string(global.size_bytes));
    }
    append_line(out, "");
    append_line(out, ".section .text");
    append_line(out, ".global _start");
    for (const auto &function : program.functions) {
        emit_function(program, function, out);
        append_line(out, "");
    }
    emit_start(program, out);
    return out;
}

void Arm64Generator::emit_function(const IRProgram &program, const FunctionIR &function, std::string &out) const {
    Context ctx;
    ctx.function_name = function.name;
    ctx.exit_label = ".L_" + function.name + "_exit";
    ctx.is_main = false;

    for (const auto &symbol : program.symbols) {
        if (symbol.function == function.name) {
            add_slot(ctx, symbol.name, symbol.size_bytes, symbol.kind);
        }
    }
    const auto labels = collect_labels(function.instructions);
    for (const auto &ins : function.instructions) {
        auto add_operand = [&](const std::string &name) {
            if (!is_identifier_operand(name) || is_global_name(program, name) || labels.count(name) != 0 ||
                looks_like_label_name(name)) {
                return;
            }
            add_slot(ctx, name, 8, SymbolKind::Temp);
        };
        add_operand(ins.dst);
        add_operand(ins.lhs);
        add_operand(ins.rhs);
    }
    ctx.frame_size = align16(ctx.frame_size);

    append_line(out, function.name + ":");
    append_line(out, "    stp x29, x30, [sp, #-16]!");
    append_line(out, "    mov x29, sp");
    if (ctx.frame_size > 0) {
        append_line(out, "    sub sp, sp, #" + std::to_string(ctx.frame_size));
    }
    for (std::size_t i = 0; i < function.params.size() && i < 8; ++i) {
        store_operand(program, function.params[i], "x" + std::to_string(i), ctx, out);
    }

    std::vector<IRInstruction> body;
    body.reserve(function.instructions.size());
    for (const auto &ins : function.instructions) {
        if (ins.op == IROp::Label && ins.dst == function.name) {
            continue;
        }
        body.push_back(ins);
    }
    for (const auto &match : match_templates(body)) {
        emit_template(program, match, ctx, out);
    }

    append_line(out, ctx.exit_label + ":");
    if (ctx.frame_size > 0) {
        append_line(out, "    add sp, sp, #" + std::to_string(ctx.frame_size));
    }
    append_line(out, "    ldp x29, x30, [sp], #16");
    append_line(out, "    ret");
}

void Arm64Generator::emit_start(const IRProgram &program, std::string &out) const {
    Context ctx;
    ctx.function_name = "<main>";
    ctx.exit_label = ".L_start_exit";
    ctx.is_main = true;

    for (const auto &symbol : program.symbols) {
        if (symbol.function == "<main>") {
            add_slot(ctx, symbol.name, symbol.size_bytes, symbol.kind);
        }
    }
    const auto labels = collect_labels(program.main);
    for (const auto &ins : program.main) {
        auto add_operand = [&](const std::string &name) {
            if (!is_identifier_operand(name) || is_global_name(program, name) || labels.count(name) != 0 ||
                looks_like_label_name(name)) {
                return;
            }
            add_slot(ctx, name, 8, SymbolKind::Temp);
        };
        add_operand(ins.dst);
        add_operand(ins.lhs);
        add_operand(ins.rhs);
    }
    ctx.frame_size = align16(ctx.frame_size);

    append_line(out, "_start:");
    append_line(out, "    stp x29, x30, [sp, #-16]!");
    append_line(out, "    mov x29, sp");
    if (ctx.frame_size > 0) {
        append_line(out, "    sub sp, sp, #" + std::to_string(ctx.frame_size));
    }
    for (const auto &match : match_templates(program.main)) {
        emit_template(program, match, ctx, out);
    }

    append_line(out, ctx.exit_label + ":");
    if (ctx.frame_size > 0) {
        append_line(out, "    add sp, sp, #" + std::to_string(ctx.frame_size));
    }
    append_line(out, "    ldp x29, x30, [sp], #16");
    append_line(out, "    mov x0, #0");
    append_line(out, "    mov x8, #93");
    append_line(out, "    svc #0");
}

void Arm64Generator::emit_template(const IRProgram &program,
                                   const TemplateMatch &match,
                                   Context &ctx,
                                   std::string &out) const {
    auto addr_into = [&](const IRInstruction &binary, const std::string &dst_reg) {
        load_operand(program, binary.lhs, "x9", ctx, out);
        if (is_immediate(binary.rhs)) {
            const int scaled = std::stoi(binary.rhs) * 8;
            append_line(out, "    add " + dst_reg + ", x9, #" + std::to_string(scaled));
        } else {
            load_operand(program, binary.rhs, "x10", ctx, out);
            append_line(out, "    lsl x10, x10, #3");
            append_line(out, "    add " + dst_reg + ", x9, x10");
        }
    };

    switch (match.kind) {
    case TemplateKind::Label: {
        append_line(out, match.insns[0]->dst + ":");
        break;
    }
    case TemplateKind::Goto: {
        append_line(out, "    b " + match.insns[0]->dst);
        break;
    }
    case TemplateKind::Return: {
        load_operand(program, match.insns[0]->lhs, "x0", ctx, out);
        append_line(out, "    b " + ctx.exit_label);
        break;
    }
    case TemplateKind::If: {
        const IRInstruction &ins = *match.insns[0];
        load_operand(program, ins.lhs, "x9", ctx, out);
        if (is_immediate(ins.rhs)) {
            append_line(out, "    cmp x9, #" + ins.rhs);
        } else {
            load_operand(program, ins.rhs, "x10", ctx, out);
            append_line(out, "    cmp x9, x10");
        }
        append_line(out, "    " + branch_for(ins.op_symbol) + " " + ins.label_true);
        append_line(out, "    b " + ins.label_false);
        break;
    }
    case TemplateKind::IfLabel: {
        const IRInstruction &branch = *match.insns[0];
        const IRInstruction &label = *match.insns[1];
        load_operand(program, branch.lhs, "x9", ctx, out);
        if (is_immediate(branch.rhs)) {
            append_line(out, "    cmp x9, #" + branch.rhs);
        } else {
            load_operand(program, branch.rhs, "x10", ctx, out);
            append_line(out, "    cmp x9, x10");
        }
        if (match.meta == 2) {
            append_line(out, "    " + branch_for(branch.op_symbol) + " " + branch.label_true);
        } else {
            append_line(out, "    " + branch_for_negated(branch.op_symbol) + " " + branch.label_false);
        }
        append_line(out, label.dst + ":");
        break;
    }
    case TemplateKind::Aop: {
        const IRInstruction &ins = *match.insns[0];
        if (ins.address_calc) {
            std::string tmp = "x11";
            addr_into(ins, tmp);
            store_operand(program, ins.dst, tmp, ctx, out);
            break;
        }
        load_operand(program, ins.lhs, "x9", ctx, out);
        if (ins.op_symbol == "+" || ins.op_symbol == "-") {
            const bool imm_fits = is_immediate(ins.rhs) && std::stoi(ins.rhs) >= 0 && std::stoi(ins.rhs) <= 4095;
            if (imm_fits) {
                append_line(out, std::string("    ") + (ins.op_symbol == "+" ? "add" : "sub") +
                                     " x11, x9, #" + ins.rhs);
            } else {
                load_operand(program, ins.rhs, "x10", ctx, out);
                append_line(out, std::string("    ") + (ins.op_symbol == "+" ? "add" : "sub") +
                                     " x11, x9, x10");
            }
        } else if (ins.op_symbol == "*") {
            load_operand(program, ins.rhs, "x10", ctx, out);
            append_line(out, "    mul x11, x9, x10");
        } else if (ins.op_symbol == "/") {
            load_operand(program, ins.rhs, "x10", ctx, out);
            append_line(out, "    sdiv x11, x9, x10");
        } else {
            throw std::runtime_error("unsupported arithmetic operator: " + ins.op_symbol);
        }
        store_operand(program, ins.dst, "x11", ctx, out);
        break;
    }
    case TemplateKind::Move: {
        const IRInstruction &ins = *match.insns[0];
        load_operand(program, ins.lhs, "x9", ctx, out);
        store_operand(program, ins.dst, "x9", ctx, out);
        break;
    }
    case TemplateKind::Ldr0: {
        const IRInstruction &ins = *match.insns[0];
        load_operand(program, ins.lhs, "x9", ctx, out);
        append_line(out, "    ldr x10, [x9]");
        store_operand(program, ins.dst, "x10", ctx, out);
        break;
    }
    case TemplateKind::Ldr2: {
        const IRInstruction &addr = *match.insns[0];
        const IRInstruction &load = *match.insns[1];
        load_operand(program, addr.lhs, "x9", ctx, out);
        if (is_immediate(addr.rhs)) {
            const int scaled = std::stoi(addr.rhs) * 8;
            append_line(out, "    ldr x10, [x9, #" + std::to_string(scaled) + "]");
        } else {
            load_operand(program, addr.rhs, "x11", ctx, out);
            append_line(out, "    ldr x10, [x9, x11, lsl #3]");
        }
        store_operand(program, load.dst, "x10", ctx, out);
        break;
    }
    case TemplateKind::Str0: {
        const IRInstruction &ins = *match.insns[0];
        load_operand(program, ins.lhs, "x9", ctx, out);
        load_operand(program, ins.rhs, "x10", ctx, out);
        append_line(out, "    str x10, [x9]");
        break;
    }
    case TemplateKind::Str2: {
        const IRInstruction &addr = *match.insns[0];
        const IRInstruction &store = *match.insns[1];
        load_operand(program, addr.lhs, "x9", ctx, out);
        if (is_immediate(addr.rhs)) {
            const int scaled = std::stoi(addr.rhs) * 8;
            load_operand(program, store.rhs, "x10", ctx, out);
            append_line(out, "    str x10, [x9, #" + std::to_string(scaled) + "]");
        } else {
            load_operand(program, addr.rhs, "x11", ctx, out);
            load_operand(program, store.rhs, "x10", ctx, out);
            append_line(out, "    str x10, [x9, x11, lsl #3]");
        }
        break;
    }
    case TemplateKind::ParCall: {
        const std::size_t count = match.insns.size() - 1;
        for (std::size_t i = 0; i < count && i < 8; ++i) {
            load_operand(program, match.insns[i]->lhs, "x" + std::to_string(i), ctx, out);
        }
        const IRInstruction &call = *match.insns.back();
        append_line(out, "    bl " + call.callee);
        if (!call.dst.empty()) {
            store_operand(program, call.dst, "x0", ctx, out);
        }
        break;
    }
    case TemplateKind::Nop:
        append_line(out, "    nop");
        break;
    }
}

void Arm64Generator::load_operand(const IRProgram &program,
                                  const std::string &operand,
                                  const std::string &reg,
                                  const Context &ctx,
                                  std::string &out) const {
    if (operand.empty()) {
        append_line(out, "    mov " + reg + ", #0");
        return;
    }
    if (is_immediate(operand)) {
        append_line(out, "    mov " + reg + ", #" + operand);
        return;
    }
    if (is_global_array(program, operand)) {
        append_line(out, "    adr " + reg + ", " + operand);
        return;
    }
    if (is_global_scalar(program, operand)) {
        append_line(out, "    adr x16, " + operand);
        append_line(out, "    ldr " + reg + ", [x16]");
        return;
    }
    const auto it = ctx.offsets.find(operand);
    if (it == ctx.offsets.end()) {
        throw std::runtime_error("unknown operand in ARM generation: " + operand);
    }
    const auto kind = ctx.kinds.find(operand);
    if (kind != ctx.kinds.end() && kind->second == SymbolKind::LocalArray) {
        append_line(out, "    sub " + reg + ", x29, #" + std::to_string(it->second));
        return;
    }
    append_line(out, "    ldur " + reg + ", [x29, #-" + std::to_string(it->second) + "]");
}

void Arm64Generator::store_operand(const IRProgram &program,
                                   const std::string &name,
                                   const std::string &reg,
                                   const Context &ctx,
                                   std::string &out) const {
    if (is_global_name(program, name)) {
        append_line(out, "    adr x15, " + name);
        append_line(out, "    str " + reg + ", [x15]");
        return;
    }
    const auto it = ctx.offsets.find(name);
    if (it == ctx.offsets.end()) {
        throw std::runtime_error("unknown destination in ARM generation: " + name);
    }
    const auto kind = ctx.kinds.find(name);
    if (kind != ctx.kinds.end() && kind->second == SymbolKind::LocalArray) {
        throw std::runtime_error("cannot store directly to local array: " + name);
    }
    append_line(out, "    stur " + reg + ", [x29, #-" + std::to_string(it->second) + "]");
}

bool Arm64Generator::is_immediate(const std::string &value) const {
    if (value.empty()) {
        return false;
    }
    if (value[0] == '-') {
        return all_digits(value, 1);
    }
    return all_digits(value, 0);
}

bool Arm64Generator::is_global_name(const IRProgram &program, const std::string &name) const {
    return std::any_of(program.globals.begin(), program.globals.end(), [&](const Global &global) {
        return global.name == name;
    });
}

bool Arm64Generator::is_global_array(const IRProgram &program, const std::string &name) const {
    return std::any_of(program.globals.begin(), program.globals.end(), [&](const Global &global) {
        return global.name == name && global.kind == SymbolKind::GlobalArray;
    });
}

bool Arm64Generator::is_global_scalar(const IRProgram &program, const std::string &name) const {
    return std::any_of(program.globals.begin(), program.globals.end(), [&](const Global &global) {
        return global.name == name && global.kind == SymbolKind::GlobalScalar;
    });
}

std::string Arm64Generator::branch_for(const std::string &op) const {
    if (op == "==") return "b.eq";
    if (op == "!=") return "b.ne";
    if (op == "<")  return "b.lt";
    if (op == "<=") return "b.le";
    if (op == ">")  return "b.gt";
    if (op == ">=") return "b.ge";
    throw std::runtime_error("unsupported relation operator: " + op);
}

std::string Arm64Generator::branch_for_negated(const std::string &op) const {
    if (op == "==") return "b.ne";
    if (op == "!=") return "b.eq";
    if (op == "<")  return "b.ge";
    if (op == "<=") return "b.gt";
    if (op == ">")  return "b.le";
    if (op == ">=") return "b.lt";
    throw std::runtime_error("unsupported relation operator: " + op);
}

int Arm64Generator::align16(int value) const {
    if (value == 0) {
        return 0;
    }
    return ((value + 15) / 16) * 16;
}

} // namespace qcompiler
