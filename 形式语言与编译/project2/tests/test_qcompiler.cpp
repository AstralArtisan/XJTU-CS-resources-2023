#include "qcompiler/arm64.h"
#include "qcompiler/ir.h"
#include "qcompiler/lexer.h"
#include "qcompiler/parser.h"
#include "qcompiler/qast.h"
#include "qcompiler/sigma_dfa.h"
#include "qcompiler/slr.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace qcompiler;

namespace {

std::string read_fixture() {
    std::ifstream in("tests/fixtures/qsort.ql", std::ios::binary);
    if (!in) {
        in.open("../tests/fixtures/qsort.ql", std::ios::binary);
    }
    assert(in && "qsort fixture must be readable");
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool contains(const std::string &text, const std::string &needle) {
    return text.find(needle) != std::string::npos;
}

void lexer_test() {
    Lexer lexer(read_fixture());
    const auto tokens = lexer.tokenize();
    bool saw_while = false;
    bool saw_return = false;
    bool saw_plus_assign = false;
    for (const auto &token : tokens) {
        saw_while = saw_while || token.type == TokenType::KwWhile;
        saw_return = saw_return || token.type == TokenType::KwReturn;
        saw_plus_assign = saw_plus_assign || token.type == TokenType::PlusAssign;
    }
    assert(saw_while);
    assert(saw_return);
    assert(!saw_plus_assign);
}

IRProgram parse_fixture() {
    Lexer lexer(read_fixture());
    Parser parser(lexer.tokenize());
    return parser.parse_program();
}

IRProgram parse_source(const std::string &source) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    return parser.parse_program();
}

std::vector<Token> source_tokens(const std::string &source) {
    Lexer lexer(source);
    return lexer.tokenize();
}

std::vector<Token> fixture_tokens() {
    Lexer lexer(read_fixture());
    return lexer.tokenize();
}

void qtac_test() {
    const IRProgram program = parse_fixture();
    assert(program.globals.size() == 1);
    assert(program.functions.size() == 3);
    assert(!program.main.empty());
    const std::string qtac = program_to_qtac(program);
    assert(contains(qtac, "i80 a;"));
    assert(contains(qtac, "define partition(arr, low, high)"));
    assert(contains(qtac, "CALL qsort, 3"));
    assert(contains(qtac, "IF"));
    assert(contains(qtac, "M["));
}

void arm_test() {
    const IRProgram program = parse_fixture();
    const std::string asm_text = Arm64Generator{}.generate(program);
    assert(contains(asm_text, ".global _start"));
    assert(contains(asm_text, "partition:"));
    assert(contains(asm_text, "bl qsort"));
    assert(contains(asm_text, "ldr x10, [x9, x11, lsl #3]"));
    assert(contains(asm_text, "str x10, [x9, x11, lsl #3]"));
    assert(contains(asm_text, "cmp x9"));
}

void slr_test() {
    const auto table = build_qsort_slr_table();
    assert(table.state_count > 0);
    assert(table.conflicts.empty());
    assert(table.resolved_conflicts.size() == 2);
    assert(!table.action_entries.empty());
    assert(!table.goto_entries.empty());
    const std::string text = slr_table_to_text(table);
    assert(contains(text, "unresolved conflicts: 0"));
    assert(contains(text, "resolved conflicts: 2"));
    assert(contains(text, "ACTION["));
    assert(contains(text, "GOTO["));
    assert(contains(text, "Statement -> WHILE LPAREN Cond RPAREN Statement"));
    assert(contains(text, "Statement -> IF LPAREN Cond RPAREN Statement ELSE Statement"));
    assert(contains(text, "Expr -> ID ASSIGN Expr"));
    assert(contains(text, "RelCond -> Expr RelOp Expr"));
}

void else_and_boolean_test() {
    const std::string source = R"(
{
int a[1];
void f(int x;) {
    int y;
    if (!(x < 1) || x == 2) y = 1; else y = 2;
    while (x < 3 && y != 0) x = x + 1;
    return;
}
f(0,);
}
)";
    const std::string qtac = program_to_qtac(parse_source(source));
    assert(contains(qtac, "IF x >= 1"));
    assert(contains(qtac, "IF x == 2"));
    assert(contains(qtac, "IF x < 3"));
    assert(contains(qtac, "IF y != 0"));
    assert(contains(qtac, "y = 1"));
    assert(contains(qtac, "y = 2"));
}

void invalid_syntax_test() {
    bool failed = false;
    try {
        parse_source("{ int a[1] void f() { return; } }");
    } catch (const std::exception &ex) {
        failed = contains(ex.what(), "SLR parse error");
    }
    assert(failed);
}

void semantic_validation_test() {
    auto fails_with = [](const std::string &source, const std::string &needle) {
        try {
            parse_source(source);
        } catch (const std::exception &ex) {
            return contains(ex.what(), needle);
        }
        return false;
    };

    assert(fails_with("{ void f() { x = 1; } f(); }", "undefined assignment target"));
    assert(fails_with("{ void f() { int x; int x; } f(); }", "duplicate symbol"));
    assert(fails_with("{ void f(int x;) { return; } f(); }", "expects 1 arguments"));
    assert(fails_with("{ void f() { return; } foo(); }", "undefined function"));
    assert(fails_with("{ void f() { int x; x[0] = 1; } f(); }", "is not an array"));
}

void arm_addressing_test() {
    const std::string source = R"(
{
int g;
void f() {
    int b[2];
    int x;
    g = 3;
    x = g;
    b[0] = x;
}
f();
}
)";
    const std::string asm_text = Arm64Generator{}.generate(parse_source(source));
    assert(contains(asm_text, "adr x15, g"));
    assert(contains(asm_text, "adr x16, g"));
    assert(contains(asm_text, "ldr x9, [x16]"));
    assert(contains(asm_text, "sub x9, x29, #"));
    assert(contains(asm_text, "str x10, [x9, #0]"));
}

void optimizer_test() {
    IRInstruction move;
    move.op = IROp::Move;
    move.dst = "x";
    move.lhs = "x";
    IRInstruction label;
    label.op = IROp::Label;
    label.dst = "l0";
    const auto optimized = optimize_basic({move, label});
    assert(optimized.size() == 1);
    assert(optimized[0].op == IROp::Label);
}

void cse_test() {
    auto binary = [](std::string dst, std::string lhs, std::string op, std::string rhs, bool addr) {
        IRInstruction i;
        i.op = IROp::Binary;
        i.dst = std::move(dst);
        i.lhs = std::move(lhs);
        i.rhs = std::move(rhs);
        i.op_symbol = std::move(op);
        i.address_calc = addr;
        return i;
    };
    auto move = [](std::string dst, std::string lhs) {
        IRInstruction i;
        i.op = IROp::Move;
        i.dst = std::move(dst);
        i.lhs = std::move(lhs);
        return i;
    };
    auto label = [](std::string name) {
        IRInstruction i;
        i.op = IROp::Label;
        i.dst = std::move(name);
        return i;
    };

    // 算术公共子表达式：第二个 low-1 被消除，其使用被重命名到 t0。
    const auto arith = eliminate_common_subexpressions(
        {binary("t0", "low", "-", "1", false), move("i", "t0"),
         binary("t1", "low", "-", "1", false), move("j", "t1")});
    assert(arith.size() == 3);
    assert(arith[2].op == IROp::Move && arith[2].dst == "j" && arith[2].lhs == "t0");

    // 地址计算不参与 CSE（交给 T_LDR2/T_STR2 融合，复用会读到未物化的槽位）。
    IRInstruction load;
    load.op = IROp::Load;
    load.dst = "x";
    load.lhs = "t0";
    IRInstruction store;
    store.op = IROp::Store;
    store.lhs = "t1";
    store.rhs = "x";
    const auto addr = eliminate_common_subexpressions(
        {binary("t0", "arr", "+", "i", true), load, binary("t1", "arr", "+", "i", true), store});
    assert(addr.size() == 4);

    // 基本块边界阻断跨块合并：中间的 LABEL 让两个 low-1 都保留。
    const auto across = eliminate_common_subexpressions(
        {binary("t0", "low", "-", "1", false), move("i", "t0"), label("l0"),
         binary("t1", "low", "-", "1", false), move("j", "t1")});
    assert(across.size() == 5);

    // 操作数被重新定值后失效：i 改写后，arr... 这里用 low 改写来验证 kill。
    const auto killed = eliminate_common_subexpressions(
        {binary("t0", "low", "-", "1", false), move("low", "t0"),
         binary("t1", "low", "-", "1", false)});
    assert(killed.size() == 3); // 第二个 low-1 不能复用，因为 low 已被改写
}

void cse_fixture_test() {
    // 端到端：优化后程序仍含三个函数与全部控制流，且 partition 里两次 low-1
    // 中的第二次被消除（材料里 i 与 j 都初始化为 low-1）。
    const IRProgram program = parse_fixture();
    const IRProgram optimized = optimize_program_basic(program);
    const std::string before = program_to_qtac(program);
    const std::string after = program_to_qtac(optimized);
    auto count = [](const std::string &text, const std::string &needle) {
        int n = 0;
        for (std::size_t p = text.find(needle); p != std::string::npos; p = text.find(needle, p + 1)) {
            ++n;
        }
        return n;
    };
    assert(count(before, "= low - 1;") == 2);
    assert(count(after, "= low - 1;") == 1);
    // 优化后汇编仍能生成且保留函数与数组寻址融合。
    const std::string opt_asm = Arm64Generator{}.generate(optimized);
    assert(contains(opt_asm, "partition:"));
    assert(contains(opt_asm, "ldr x10, [x9, x11, lsl #3]"));
}

void qast_route_test() {
    const auto tokens = fixture_tokens();
    const QastNode route_a = parse_qast_recursive_descent(tokens);
    const QastNode route_b = parse_qast_slr(tokens);
    const std::string text_a = qast_to_text(route_a);
    const std::string text_b = qast_to_text(route_b);

    for (const char *needle : {"Function name=swap",
                               "Function name=partition",
                               "Function name=qsort",
                               "ArrayLoad",
                               "Call name=qsort",
                               "If",
                               "While"}) {
        assert(contains(text_a, needle));
        assert(contains(text_b, needle));
    }

    const std::string qtac_a = program_to_qtac(qast_to_ir(route_a));
    const std::string qtac_b = program_to_qtac(qast_to_ir(route_b));
    for (const char *needle : {"i80 a;",
                               "define partition(arr, low, high)",
                               "CALL qsort, 3",
                               "IF",
                               "M["}) {
        assert(contains(qtac_a, needle));
        assert(contains(qtac_b, needle));
    }
}

void qast_boolean_route_test() {
    const std::string source = R"(
{
int a[1];
void f(int x;) {
    int y;
    if (!(x < 1) || x == 2) y = 1; else y = 2;
    while (x < 3 && y != 0) x = x + 1;
    return;
}
f(0,);
}
)";
    const auto tokens = source_tokens(source);
    const std::string qtac_a = program_to_qtac(qast_to_ir(parse_qast_recursive_descent(tokens)));
    const std::string qtac_b = program_to_qtac(qast_to_ir(parse_qast_slr(tokens)));
    for (const char *needle : {"IF x >= 1", "IF x == 2", "IF x < 3", "IF y != 0", "y = 1", "y = 2"}) {
        assert(contains(qtac_a, needle));
        assert(contains(qtac_b, needle));
    }
}

void sigma_dfa_test() {
    const IRProgram program = parse_fixture();
    int total_ir = 0;
    int total_matches = 0;
    std::array<int, 13> sum{};
    for (const auto &function : program.functions) {
        total_ir += static_cast<int>(function.instructions.size());
        const auto matches = match_templates(function.instructions);
        total_matches += static_cast<int>(matches.size());
        const auto hist = template_histogram(matches);
        for (std::size_t i = 0; i < hist.size(); ++i) {
            sum[i] += hist[i];
        }
    }
    total_ir += static_cast<int>(program.main.size());
    const auto main_matches = match_templates(program.main);
    total_matches += static_cast<int>(main_matches.size());
    const auto main_hist = template_histogram(main_matches);
    for (std::size_t i = 0; i < main_hist.size(); ++i) {
        sum[i] += main_hist[i];
    }

    assert(total_matches < total_ir);
    assert(sum[static_cast<std::size_t>(TemplateKind::IfLabel)] >= 1);
    assert(sum[static_cast<std::size_t>(TemplateKind::Ldr2)] >= 1);
    assert(sum[static_cast<std::size_t>(TemplateKind::Str2)] >= 1);
    assert(sum[static_cast<std::size_t>(TemplateKind::ParCall)] >= 1);
    assert(sum[static_cast<std::size_t>(TemplateKind::Label)] >= 1);
    assert(sum[static_cast<std::size_t>(TemplateKind::Return)] >= 1);

    const std::string text = SigmaDfa::instance().to_text();
    assert(contains(text, "sigma-DFA"));
    assert(contains(text, "T_IF_LABEL"));
    assert(contains(text, "T_LDR2"));
    assert(contains(text, "T_STR2"));
    assert(contains(text, "T_PAR_CALL"));
}

} // namespace

int main() {
    lexer_test();
    qtac_test();
    arm_test();
    slr_test();
    else_and_boolean_test();
    invalid_syntax_test();
    semantic_validation_test();
    arm_addressing_test();
    optimizer_test();
    cse_test();
    cse_fixture_test();
    qast_route_test();
    qast_boolean_route_test();
    sigma_dfa_test();
    std::cout << "qcompiler tests passed\n";
    return 0;
}
