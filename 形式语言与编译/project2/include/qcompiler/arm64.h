#pragma once

#include "qcompiler/ir.h"
#include "qcompiler/sigma_dfa.h"

#include <string>

namespace qcompiler {

class Arm64Generator {
public:
    std::string generate(const IRProgram &program) const;

private:
    struct Context;

    void emit_function(const IRProgram &program, const FunctionIR &function, std::string &out) const;
    void emit_start(const IRProgram &program, std::string &out) const;
    void emit_template(const IRProgram &program,
                       const TemplateMatch &match,
                       Context &ctx,
                       std::string &out) const;
    void load_operand(const IRProgram &program,
                      const std::string &operand,
                      const std::string &reg,
                      const Context &ctx,
                      std::string &out) const;
    void store_operand(const IRProgram &program,
                       const std::string &name,
                       const std::string &reg,
                       const Context &ctx,
                       std::string &out) const;
    bool is_immediate(const std::string &value) const;
    bool is_global_name(const IRProgram &program, const std::string &name) const;
    bool is_global_array(const IRProgram &program, const std::string &name) const;
    bool is_global_scalar(const IRProgram &program, const std::string &name) const;
    std::string branch_for(const std::string &op) const;
    std::string branch_for_negated(const std::string &op) const;
    int align16(int value) const;
};

} // namespace qcompiler
