#pragma once

#include <string>
#include <utility>
#include <vector>

namespace qcompiler {

enum class SymbolKind {
    GlobalScalar,
    GlobalArray,
    LocalScalar,
    LocalArray,
    ParamScalar,
    ParamArray,
    Temp
};

struct Symbol {
    std::string name;
    SymbolKind kind = SymbolKind::LocalScalar;
    std::string function;
    int size_bytes = 8;
};

struct Global {
    std::string name;
    int size_bytes = 8;
    SymbolKind kind = SymbolKind::GlobalArray;
};

enum class IROp {
    Label,
    Goto,
    If,
    Par,
    Call,
    Return,
    Binary,
    Move,
    Load,
    Store,
    Nop
};

struct IRInstruction {
    IROp op = IROp::Nop;
    std::string dst;
    std::string lhs;
    std::string rhs;
    std::string op_symbol;
    std::string label_true;
    std::string label_false;
    std::string callee;
    int arg_count = 0;
    bool address_calc = false;

    IRInstruction() = default;
    IRInstruction(IROp op_value,
                  std::string dst_value = {},
                  std::string lhs_value = {},
                  std::string rhs_value = {},
                  std::string op_symbol_value = {},
                  std::string label_true_value = {},
                  std::string label_false_value = {},
                  std::string callee_value = {},
                  int arg_count_value = 0,
                  bool address_calc_value = false)
        : op(op_value),
          dst(std::move(dst_value)),
          lhs(std::move(lhs_value)),
          rhs(std::move(rhs_value)),
          op_symbol(std::move(op_symbol_value)),
          label_true(std::move(label_true_value)),
          label_false(std::move(label_false_value)),
          callee(std::move(callee_value)),
          arg_count(arg_count_value),
          address_calc(address_calc_value) {}
};

struct FunctionIR {
    std::string name;
    std::string return_type;
    std::vector<std::string> params;
    std::vector<IRInstruction> instructions;
};

struct IRProgram {
    std::vector<Global> globals;
    std::vector<FunctionIR> functions;
    std::vector<IRInstruction> main;
    std::vector<Symbol> symbols;
};

std::string symbol_kind_name(SymbolKind kind);
std::string instruction_to_qtac(const IRInstruction &instruction);
std::string program_to_qtac(const IRProgram &program);
std::string program_to_symtab(const IRProgram &program);
std::vector<IRInstruction> elide_temp_moves(const std::vector<IRInstruction> &instructions);
IRProgram elide_temp_moves_program(IRProgram program);
std::vector<IRInstruction> eliminate_common_subexpressions(const std::vector<IRInstruction> &instructions);
std::vector<IRInstruction> optimize_basic(const std::vector<IRInstruction> &instructions);
IRProgram optimize_program_basic(IRProgram program);

} // namespace qcompiler
