#include "qcompiler/ir.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace qcompiler {

namespace {

bool is_temp_name(const std::string &name) {
    if (name.size() < 2 || name[0] != 't') {
        return false;
    }
    for (std::size_t i = 1; i < name.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
            return false;
        }
    }
    return true;
}

void tally_use(const std::string &name, std::unordered_map<std::string, int> &uses) {
    if (is_temp_name(name)) {
        uses[name] += 1;
    }
}

std::unordered_map<std::string, int> count_temp_uses(const std::vector<IRInstruction> &ins) {
    std::unordered_map<std::string, int> uses;
    for (const auto &i : ins) {
        tally_use(i.lhs, uses);
        tally_use(i.rhs, uses);
    }
    return uses;
}

} // namespace

std::string symbol_kind_name(SymbolKind kind) {
    switch (kind) {
    case SymbolKind::GlobalScalar:
        return "global-scalar";
    case SymbolKind::GlobalArray:
        return "global-array";
    case SymbolKind::LocalScalar:
        return "local-scalar";
    case SymbolKind::LocalArray:
        return "local-array";
    case SymbolKind::ParamScalar:
        return "param-scalar";
    case SymbolKind::ParamArray:
        return "param-array";
    case SymbolKind::Temp:
        return "temp";
    }
    return "unknown";
}

std::string instruction_to_qtac(const IRInstruction &instruction) {
    std::ostringstream out;
    switch (instruction.op) {
    case IROp::Label:
        out << "LABEL " << instruction.dst << ";";
        break;
    case IROp::Goto:
        out << "GOTO " << instruction.dst << ";";
        break;
    case IROp::If:
        out << "IF " << instruction.lhs << ' ' << instruction.op_symbol << ' ' << instruction.rhs
            << " THEN " << instruction.label_true << " ELSE " << instruction.label_false << ";";
        break;
    case IROp::Par:
        out << "PAR " << instruction.lhs << ";";
        break;
    case IROp::Call:
        out << instruction.dst << " = CALL " << instruction.callee << ", " << instruction.arg_count << ";";
        break;
    case IROp::Return:
        out << "RETURN " << instruction.lhs << ";";
        break;
    case IROp::Binary:
        out << instruction.dst << " = " << instruction.lhs << ' ' << instruction.op_symbol << ' '
            << instruction.rhs << ";";
        break;
    case IROp::Move:
        out << instruction.dst << " = " << instruction.lhs << ";";
        break;
    case IROp::Load:
        out << instruction.dst << " = M[" << instruction.lhs << "];";
        break;
    case IROp::Store:
        out << "M[" << instruction.lhs << "] = " << instruction.rhs << ";";
        break;
    case IROp::Nop:
        out << "NOP;";
        break;
    }
    return out.str();
}

std::string program_to_qtac(const IRProgram &program) {
    std::ostringstream out;
    for (const auto &global : program.globals) {
        out << 'i' << global.size_bytes << ' ' << global.name << ";\n";
    }
    for (const auto &function : program.functions) {
        out << "define " << function.name << '(';
        for (std::size_t i = 0; i < function.params.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << function.params[i];
        }
        out << "){\n";
        for (const auto &instruction : function.instructions) {
            out << instruction_to_qtac(instruction) << '\n';
        }
        out << "}\n";
    }
    for (const auto &instruction : program.main) {
        out << instruction_to_qtac(instruction) << '\n';
    }
    return out.str();
}

std::string program_to_symtab(const IRProgram &program) {
    std::ostringstream out;
    out << "name\tkind\tfunction\tsize\n";
    for (const auto &symbol : program.symbols) {
        out << symbol.name << '\t' << symbol_kind_name(symbol.kind) << '\t';
        out << (symbol.function.empty() ? "<global>" : symbol.function) << '\t';
        out << symbol.size_bytes << '\n';
    }
    return out.str();
}

std::vector<IRInstruction> elide_temp_moves(const std::vector<IRInstruction> &instructions) {
    const auto uses = count_temp_uses(instructions);
    std::vector<IRInstruction> result;
    result.reserve(instructions.size());
    for (std::size_t i = 0; i < instructions.size(); ++i) {
        const IRInstruction &cur = instructions[i];
        const bool produces_value = cur.op == IROp::Load || cur.op == IROp::Call;
        if (produces_value && is_temp_name(cur.dst) && i + 1 < instructions.size()) {
            const IRInstruction &nxt = instructions[i + 1];
            const auto it = uses.find(cur.dst);
            const bool used_once = it != uses.end() && it->second == 1;
            if (used_once && nxt.op == IROp::Move && nxt.lhs == cur.dst) {
                IRInstruction merged = cur;
                merged.dst = nxt.dst;
                result.push_back(merged);
                ++i;
                continue;
            }
        }
        result.push_back(cur);
    }
    return result;
}

IRProgram elide_temp_moves_program(IRProgram program) {
    for (auto &function : program.functions) {
        function.instructions = elide_temp_moves(function.instructions);
    }
    program.main = elide_temp_moves(program.main);
    return program;
}

// 局部公共子表达式消除（local CSE）。
//
// 以基本块为单位工作：LABEL 是块入口，GOTO/IF/RETURN/CALL 是块出口（参见
// 素材中“基本块单入口单出口”的定义）。块内维护一张“可用表达式”表，记录每个已
// 计算过的算术二元运算 (lhs op rhs) 及其结果临时量。再次遇到同一表达式且其操作
// 数在块内未被重新定值时，直接复用先前结果：删除这条冗余的 Binary 指令，并把它
// 的目标临时量在后续指令中重命名为先前结果。临时量是单赋值的，且本后端为每个临
// 时量分配独立栈槽，故跨块重命名安全。地址计算 (address_calc) 不入表，原因见下。
std::vector<IRInstruction> eliminate_common_subexpressions(const std::vector<IRInstruction> &instructions) {
    struct AvailExpr {
        std::string lhs;
        std::string rhs;
        std::string op;
        std::string dst;
    };

    std::unordered_map<std::string, std::string> rename; // 被删临时量 -> 规范临时量（函数级）
    std::vector<AvailExpr> available;                     // 当前基本块内的可用表达式

    auto apply_rename = [&rename](std::string &name) {
        const auto it = rename.find(name);
        if (it != rename.end()) {
            name = it->second;
        }
    };
    auto kill_uses_of = [&available](const std::string &name) {
        available.erase(std::remove_if(available.begin(), available.end(),
                                       [&name](const AvailExpr &e) {
                                           return e.lhs == name || e.rhs == name;
                                       }),
                        available.end());
    };

    std::vector<IRInstruction> result;
    result.reserve(instructions.size());
    for (const IRInstruction &original : instructions) {
        IRInstruction cur = original;

        // 重命名“使用”位置（操作数）；目标位置是定值，不重命名。
        switch (cur.op) {
        case IROp::If:
        case IROp::Store:
        case IROp::Binary:
            apply_rename(cur.lhs);
            apply_rename(cur.rhs);
            break;
        case IROp::Par:
        case IROp::Return:
        case IROp::Move:
        case IROp::Load:
            apply_rename(cur.lhs);
            break;
        default:
            break;
        }

        if (cur.op == IROp::Label) { // 块入口：清空可用表达式
            available.clear();
            result.push_back(cur);
            continue;
        }

        // 仅对“算术”二元运算做 CSE。地址计算 (address_calc) 的临时量会被
        // T_LDR2/T_STR2 模板与相邻 Load/Store 融合、内联到寻址里而不落栈槽，
        // 复用它作为基址会读到未初始化的槽位，故排除在外，交给融合模板处理。
        if (cur.op == IROp::Binary && is_temp_name(cur.dst) && !cur.address_calc) {
            const AvailExpr *hit = nullptr;
            for (const AvailExpr &e : available) {
                if (e.op == cur.op_symbol && e.lhs == cur.lhs && e.rhs == cur.rhs) {
                    hit = &e;
                    break;
                }
            }
            if (hit != nullptr) { // 冗余：复用先前结果，删除本指令
                rename[cur.dst] = hit->dst;
                continue;
            }
            available.push_back({cur.lhs, cur.rhs, cur.op_symbol, cur.dst});
            result.push_back(cur);
            continue;
        }

        // 定值会使引用该名字的可用表达式失效（如 i 被重新赋值后 arr+i 不再有效）。
        if ((cur.op == IROp::Move || cur.op == IROp::Load || cur.op == IROp::Call) && !cur.dst.empty()) {
            kill_uses_of(cur.dst);
        }
        result.push_back(cur);

        if (cur.op == IROp::Goto || cur.op == IROp::If || cur.op == IROp::Return || cur.op == IROp::Call) {
            available.clear(); // 块出口
        }
    }
    return result;
}

std::vector<IRInstruction> optimize_basic(const std::vector<IRInstruction> &instructions) {
    std::vector<IRInstruction> result;
    bool unreachable_until_label = false;
    for (const auto &instruction : instructions) {
        if (instruction.op == IROp::Label) {
            unreachable_until_label = false;
            result.push_back(instruction);
            continue;
        }
        if (unreachable_until_label) {
            continue;
        }
        if (instruction.op == IROp::Nop) {
            continue;
        }
        if (instruction.op == IROp::Move && instruction.dst == instruction.lhs) {
            continue;
        }
        result.push_back(instruction);
        if (instruction.op == IROp::Goto || instruction.op == IROp::Return) {
            unreachable_until_label = true;
        }
    }
    return result;
}

IRProgram optimize_program_basic(IRProgram program) {
    for (auto &function : program.functions) {
        function.instructions = eliminate_common_subexpressions(function.instructions);
        function.instructions = optimize_basic(function.instructions);
    }
    program.main = eliminate_common_subexpressions(program.main);
    program.main = optimize_basic(program.main);
    return program;
}

} // namespace qcompiler
