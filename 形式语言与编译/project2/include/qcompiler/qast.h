#pragma once

#include "qcompiler/ir.h"
#include "qcompiler/token.h"

#include <string>
#include <vector>

namespace qcompiler {

struct QastNode {
    std::string kind;
    std::string value;
    std::string type;
    std::string op;
    bool is_array = false;
    int size_bytes = 8;
    std::vector<QastNode> children;
};

QastNode parse_qast_recursive_descent(const std::vector<Token> &tokens);
QastNode parse_qast_slr(const std::vector<Token> &tokens);
IRProgram qast_to_ir(const QastNode &root);
std::string qast_to_text(const QastNode &root);

} // namespace qcompiler
