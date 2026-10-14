#pragma once

#include "qcompiler/ir.h"
#include "qcompiler/token.h"

#include <vector>

namespace qcompiler {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    IRProgram parse_program();

private:
    std::vector<Token> tokens_;
};

} // namespace qcompiler
