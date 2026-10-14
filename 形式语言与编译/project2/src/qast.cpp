#include "qcompiler/qast.h"

#include "qcompiler/slr.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace qcompiler {

namespace {

struct ExprResult {
    std::vector<IRInstruction> code;
    std::string place;
};

struct CondResult {
    std::vector<IRInstruction> code;
    std::string lhs;
    std::string op;
    std::string rhs;
};

struct TranslationContext {
    IRProgram program;
    std::unordered_map<std::string, SymbolKind> globals;
    std::unordered_map<std::string, SymbolKind> locals;
    std::unordered_map<std::string, int> functions;
    int current_function_index = -1;
    std::string current_function_name;
    int temp_id = 0;
    int label_id = 0;

    bool in_function() const {
        return current_function_index >= 0;
    }

    std::string make_temp() {
        const std::string name = "t" + std::to_string(temp_id++);
        program.symbols.push_back(Symbol{name, SymbolKind::Temp, in_function() ? current_function_name : "<main>", 8});
        return name;
    }

    std::string make_label() {
        return "l" + std::to_string(label_id++);
    }

    void add_symbol(const std::string &name, SymbolKind kind, int size_bytes) {
        auto &scope = in_function() ? locals : globals;
        if (scope.find(name) != scope.end()) {
            throw std::runtime_error("semantic error: duplicate symbol '" + name + "'");
        }
        if (!in_function() && functions.find(name) != functions.end()) {
            throw std::runtime_error("semantic error: symbol conflicts with function '" + name + "'");
        }
        program.symbols.push_back(Symbol{name, kind, in_function() ? current_function_name : "", size_bytes});
        scope[name] = kind;
    }

    void declare_function(const std::string &name, int arity) {
        if (functions.find(name) != functions.end()) {
            throw std::runtime_error("semantic error: duplicate function '" + name + "'");
        }
        if (globals.find(name) != globals.end()) {
            throw std::runtime_error("semantic error: function conflicts with global symbol '" + name + "'");
        }
        functions[name] = arity;
    }

    std::optional<SymbolKind> find_symbol(const std::string &name) const {
        const auto local = locals.find(name);
        if (local != locals.end()) {
            return local->second;
        }
        const auto global = globals.find(name);
        if (global != globals.end()) {
            return global->second;
        }
        return std::nullopt;
    }

    void require_symbol(const std::string &name) const {
        if (!find_symbol(name).has_value()) {
            throw std::runtime_error("semantic error: undefined symbol '" + name + "'");
        }
    }

    void require_array(const std::string &name) const {
        const auto kind = find_symbol(name);
        if (!kind.has_value()) {
            throw std::runtime_error("semantic error: undefined array '" + name + "'");
        }
        if (*kind != SymbolKind::GlobalArray && *kind != SymbolKind::LocalArray && *kind != SymbolKind::ParamArray) {
            throw std::runtime_error("semantic error: symbol '" + name + "' is not an array");
        }
    }

    void require_scalar_lvalue(const std::string &name) const {
        const auto kind = find_symbol(name);
        if (!kind.has_value()) {
            throw std::runtime_error("semantic error: undefined assignment target '" + name + "'");
        }
        if (*kind == SymbolKind::GlobalArray || *kind == SymbolKind::LocalArray || *kind == SymbolKind::ParamArray) {
            throw std::runtime_error("semantic error: cannot assign directly to array '" + name + "'");
        }
    }

    void require_function_call(const std::string &name, int arity) const {
        const auto found = functions.find(name);
        if (found == functions.end()) {
            throw std::runtime_error("semantic error: undefined function '" + name + "'");
        }
        if (found->second != arity) {
            throw std::runtime_error("semantic error: function '" + name + "' expects " +
                                     std::to_string(found->second) + " arguments, got " +
                                     std::to_string(arity));
        }
        if (arity > 8) {
            throw std::runtime_error("semantic error: function '" + name + "' exceeds ARM64 argument limit");
        }
    }
};

struct QastValue {
    std::string symbol;
    std::string text;
    std::string type;
    std::string name;
    bool is_array = false;
    int size_bytes = 8;
    std::vector<QastNode> nodes;
    std::vector<QastNode> params;
    std::vector<QastNode> args;
    QastNode node;
};

struct InputToken {
    std::string symbol;
    QastValue value;
    std::size_t start_pos = 0;
    std::size_t end_pos = 0;
};

enum ProductionIndex {
    kProgramBlock = 1,
    kItemsItemItems = 2,
    kItemsEmpty = 3,
    kItemDeclaration = 4,
    kItemFunction = 5,
    kItemStatement = 6,
    kDeclaration = 7,
    kDeclSuffixArray = 8,
    kDeclSuffixScalar = 9,
    kFunction = 10,
    kFunctionHeader = 11,
    kTypeInt = 12,
    kTypeVoid = 13,
    kParamsParamParams = 14,
    kParamsEmpty = 15,
    kParam = 16,
    kParamSuffixArray = 17,
    kParamSuffixScalar = 18,
    kBlock = 19,
    kBlockItemsBlockItemBlockItems = 20,
    kBlockItemsEmpty = 21,
    kBlockItemDeclaration = 22,
    kBlockItemStatement = 23,
    kStatementEmpty = 24,
    kStatementExpr = 25,
    kStatementIf = 26,
    kStatementIfElse = 27,
    kStatementWhile = 28,
    kStatementReturnExpr = 29,
    kStatementReturnEmpty = 30,
    kStatementBlock = 31,
    kExprScalarAssign = 32,
    kExprArrayAssign = 33,
    kExprScalarPlusAssign = 34,
    kExprArrayPlusAssign = 35,
    kExprAdd = 36,
    kAddPlus = 37,
    kAddMinus = 38,
    kAddMul = 39,
    kMulTimes = 40,
    kMulDivide = 41,
    kMulUnary = 42,
    kUnaryMinus = 43,
    kUnaryPrimary = 44,
    kPrimaryNumber = 45,
    kPrimaryId = 46,
    kPrimaryCall = 47,
    kPrimaryArrayLoad = 48,
    kPrimaryArrayRef = 49,
    kPrimaryParenExpr = 50,
    kArgsExprArgsTail = 51,
    kArgsEmpty = 52,
    kArgsTailCommaArgs = 53,
    kArgsTailEmpty = 54,
    kCondOr = 55,
    kCondAnd = 56,
    kAndCondAnd = 57,
    kAndCondNot = 58,
    kNotCondBang = 59,
    kNotCondParen = 60,
    kNotCondRel = 61,
    kRelCondCompare = 62,
    kRelCondExpr = 63,
    kRelOpLt = 64,
    kRelOpLe = 65,
    kRelOpGt = 66,
    kRelOpGe = 67,
    kRelOpEq = 68,
    kRelOpNe = 69,
};

QastNode make_node(std::string kind) {
    QastNode node;
    node.kind = std::move(kind);
    return node;
}

QastNode make_value_node(std::string kind, std::string value) {
    QastNode node;
    node.kind = std::move(kind);
    node.value = std::move(value);
    return node;
}

void append_nodes(std::vector<QastNode> &target, const std::vector<QastNode> &source) {
    target.insert(target.end(), source.begin(), source.end());
}

void append_code(std::vector<IRInstruction> &target, const std::vector<IRInstruction> &source) {
    target.insert(target.end(), source.begin(), source.end());
}

std::string token_terminal(TokenType type) {
    switch (type) {
    case TokenType::End:
        return "$";
    case TokenType::Identifier:
        return "ID";
    case TokenType::Number:
        return "NUMBER";
    case TokenType::KwInt:
        return "INT";
    case TokenType::KwVoid:
        return "VOID";
    case TokenType::KwIf:
        return "IF";
    case TokenType::KwElse:
        return "ELSE";
    case TokenType::KwWhile:
        return "WHILE";
    case TokenType::KwReturn:
        return "RETURN";
    case TokenType::LBrace:
        return "LBRACE";
    case TokenType::RBrace:
        return "RBRACE";
    case TokenType::LParen:
        return "LPAREN";
    case TokenType::RParen:
        return "RPAREN";
    case TokenType::LBracket:
        return "LBRACKET";
    case TokenType::RBracket:
        return "RBRACKET";
    case TokenType::Semicolon:
        return "SEMI";
    case TokenType::Comma:
        return "COMMA";
    case TokenType::Plus:
        return "PLUS";
    case TokenType::Minus:
        return "MINUS";
    case TokenType::Star:
        return "STAR";
    case TokenType::Slash:
        return "SLASH";
    case TokenType::Assign:
        return "ASSIGN";
    case TokenType::PlusAssign:
        return "PLUS_ASSIGN";
    case TokenType::Less:
        return "LT";
    case TokenType::LessEqual:
        return "LE";
    case TokenType::Greater:
        return "GT";
    case TokenType::GreaterEqual:
        return "GE";
    case TokenType::EqualEqual:
        return "EQ";
    case TokenType::NotEqual:
        return "NE";
    case TokenType::Bang:
        return "BANG";
    case TokenType::AndAnd:
        return "AND";
    case TokenType::OrOr:
        return "OR";
    }
    return token_type_name(type);
}

bool is_type_token(TokenType type) {
    return type == TokenType::KwInt || type == TokenType::KwVoid;
}

bool is_relation_token(TokenType type) {
    return type == TokenType::Less || type == TokenType::LessEqual || type == TokenType::Greater ||
           type == TokenType::GreaterEqual || type == TokenType::EqualEqual || type == TokenType::NotEqual;
}

std::string invert_relation(const std::string &op) {
    if (op == "==") {
        return "!=";
    }
    if (op == "!=") {
        return "==";
    }
    if (op == "<") {
        return ">=";
    }
    if (op == "<=") {
        return ">";
    }
    if (op == ">") {
        return "<=";
    }
    if (op == ">=") {
        return "<";
    }
    throw std::runtime_error("cannot invert relation operator: " + op);
}

std::string make_array_address(TranslationContext &context,
                               std::vector<IRInstruction> &code,
                               const std::string &base,
                               const std::string &index) {
    const std::string temp = context.make_temp();
    IRInstruction calc;
    calc.op = IROp::Binary;
    calc.dst = temp;
    calc.lhs = base;
    calc.rhs = index;
    calc.op_symbol = "+";
    calc.address_calc = true;
    code.push_back(calc);
    return temp;
}

CondResult materialize_binary_condition(const CondResult &left,
                                        const CondResult &right,
                                        bool is_and,
                                        TranslationContext &context) {
    CondResult result;
    const std::string temp = context.make_temp();
    const std::string eval_right = context.make_label();
    const std::string true_label = context.make_label();
    const std::string false_label = context.make_label();
    const std::string end_label = context.make_label();

    append_code(result.code, left.code);
    if (is_and) {
        result.code.push_back(IRInstruction{IROp::If, "", left.lhs, left.rhs, left.op, eval_right, false_label});
        result.code.push_back(IRInstruction{IROp::Label, eval_right});
        append_code(result.code, right.code);
        result.code.push_back(IRInstruction{IROp::If, "", right.lhs, right.rhs, right.op, true_label, false_label});
    } else {
        result.code.push_back(IRInstruction{IROp::If, "", left.lhs, left.rhs, left.op, true_label, eval_right});
        result.code.push_back(IRInstruction{IROp::Label, eval_right});
        append_code(result.code, right.code);
        result.code.push_back(IRInstruction{IROp::If, "", right.lhs, right.rhs, right.op, true_label, false_label});
    }
    result.code.push_back(IRInstruction{IROp::Label, true_label});
    result.code.push_back(IRInstruction{IROp::Move, temp, "1"});
    result.code.push_back(IRInstruction{IROp::Goto, end_label});
    result.code.push_back(IRInstruction{IROp::Label, false_label});
    result.code.push_back(IRInstruction{IROp::Move, temp, "0"});
    result.code.push_back(IRInstruction{IROp::Label, end_label});
    result.lhs = temp;
    result.op = "!=";
    result.rhs = "0";
    return result;
}

class RecursiveQastParser {
public:
    explicit RecursiveQastParser(const std::vector<Token> &tokens) : tokens_(tokens) {}

    QastNode parse() {
        expect(TokenType::LBrace, "'{'");
        std::vector<QastNode> items;
        while (!check(TokenType::RBrace)) {
            items.push_back(parse_item());
        }
        expect(TokenType::RBrace, "'}'");
        expect(TokenType::End, "end of file");
        QastNode program = make_node("Program");
        program.children = std::move(items);
        return program;
    }

private:
    const Token &peek(std::size_t offset = 0) const {
        if (pos_ + offset >= tokens_.size()) {
            return tokens_.back();
        }
        return tokens_[pos_ + offset];
    }

    bool check(TokenType type, std::size_t offset = 0) const {
        return peek(offset).type == type;
    }

    Token expect(TokenType type, const std::string &expected) {
        if (!check(type)) {
            const Token &token = peek();
            throw std::runtime_error("recursive parse error at " + std::to_string(token.line) + ":" +
                                     std::to_string(token.column) + ": expected " + expected +
                                     ", got " + token_type_name(token.type));
        }
        return tokens_[pos_++];
    }

    bool match(TokenType type) {
        if (!check(type)) {
            return false;
        }
        ++pos_;
        return true;
    }

    QastNode parse_item() {
        if (is_type_token(peek().type)) {
            const Token type_token = tokens_[pos_++];
            const Token id = expect(TokenType::Identifier, "identifier");
            if (match(TokenType::LParen)) {
                return parse_function_after_header(type_token, id);
            }
            return parse_declaration_after_name(type_token, id);
        }
        return parse_statement();
    }

    QastNode parse_block_item() {
        if (is_type_token(peek().type)) {
            const Token type_token = tokens_[pos_++];
            const Token id = expect(TokenType::Identifier, "identifier");
            if (check(TokenType::LParen)) {
                const Token &token = peek();
                throw std::runtime_error("recursive parse error at " + std::to_string(token.line) + ":" +
                                         std::to_string(token.column) +
                                         ": nested function definitions are not supported");
            }
            return parse_declaration_after_name(type_token, id);
        }
        return parse_statement();
    }

    QastNode parse_declaration_after_name(const Token &type_token, const Token &id) {
        QastNode declaration = make_value_node("Declaration", id.lexeme);
        declaration.type = type_token.lexeme;
        if (match(TokenType::LBracket)) {
            const Token size = expect(TokenType::Number, "array size");
            expect(TokenType::RBracket, "']'");
            declaration.is_array = true;
            declaration.size_bytes = std::stoi(size.lexeme) * 8;
        }
        expect(TokenType::Semicolon, "';'");
        return declaration;
    }

    QastNode parse_function_after_header(const Token &type_token, const Token &id) {
        QastNode function = make_value_node("Function", id.lexeme);
        function.type = type_token.lexeme;
        while (!check(TokenType::RParen)) {
            function.children.push_back(parse_param());
        }
        expect(TokenType::RParen, "')'");
        function.children.push_back(parse_block());
        return function;
    }

    QastNode parse_param() {
        const Token type_token = expect(TokenType::KwInt, "'int'");
        const Token id = expect(TokenType::Identifier, "parameter name");
        QastNode param = make_value_node("Param", id.lexeme);
        param.type = type_token.lexeme;
        if (match(TokenType::LBracket)) {
            expect(TokenType::RBracket, "']'");
            param.is_array = true;
        }
        expect(TokenType::Semicolon, "';'");
        return param;
    }

    QastNode parse_block() {
        expect(TokenType::LBrace, "'{'");
        QastNode block = make_node("Block");
        while (!check(TokenType::RBrace)) {
            block.children.push_back(parse_block_item());
        }
        expect(TokenType::RBrace, "'}'");
        return block;
    }

    QastNode parse_statement() {
        if (match(TokenType::Semicolon)) {
            return make_node("EmptyStatement");
        }
        if (check(TokenType::LBrace)) {
            return parse_block();
        }
        if (match(TokenType::KwIf)) {
            expect(TokenType::LParen, "'('");
            QastNode condition = parse_condition();
            expect(TokenType::RParen, "')'");
            QastNode statement = make_node("If");
            statement.children.push_back(std::move(condition));
            statement.children.push_back(parse_statement());
            if (match(TokenType::KwElse)) {
                statement.kind = "IfElse";
                statement.children.push_back(parse_statement());
            }
            return statement;
        }
        if (match(TokenType::KwWhile)) {
            expect(TokenType::LParen, "'('");
            QastNode statement = make_node("While");
            statement.children.push_back(parse_condition());
            expect(TokenType::RParen, "')'");
            statement.children.push_back(parse_statement());
            return statement;
        }
        if (match(TokenType::KwReturn)) {
            QastNode statement = make_node("Return");
            if (!check(TokenType::Semicolon)) {
                statement.children.push_back(parse_expression());
            }
            expect(TokenType::Semicolon, "';'");
            return statement;
        }
        QastNode statement = make_node("ExprStatement");
        statement.children.push_back(parse_expression());
        expect(TokenType::Semicolon, "';'");
        return statement;
    }

    QastNode parse_expression() {
        if (check(TokenType::Identifier) &&
            (check(TokenType::Assign, 1) || check(TokenType::PlusAssign, 1))) {
            const Token id = tokens_[pos_++];
            const Token op = tokens_[pos_++];
            QastNode assignment = make_value_node("Assign", id.lexeme);
            assignment.op = op.lexeme;
            assignment.children.push_back(parse_expression());
            return assignment;
        }
        if (check(TokenType::Identifier) && check(TokenType::LBracket, 1)) {
            const std::size_t close = matching_bracket(pos_ + 1, TokenType::LBracket, TokenType::RBracket);
            if (tokens_[close + 1].type == TokenType::Assign || tokens_[close + 1].type == TokenType::PlusAssign) {
                const Token id = tokens_[pos_++];
                expect(TokenType::LBracket, "'['");
                QastNode index = parse_expression();
                expect(TokenType::RBracket, "']'");
                const Token op = tokens_[pos_++];
                QastNode assignment = make_value_node("ArrayAssign", id.lexeme);
                assignment.op = op.lexeme;
                assignment.children.push_back(std::move(index));
                assignment.children.push_back(parse_expression());
                return assignment;
            }
        }
        return parse_add();
    }

    QastNode parse_add() {
        QastNode expr = parse_mul();
        while (check(TokenType::Plus) || check(TokenType::Minus)) {
            const Token op = tokens_[pos_++];
            QastNode binary = make_node("Binary");
            binary.op = op.lexeme;
            binary.children.push_back(std::move(expr));
            binary.children.push_back(parse_mul());
            expr = std::move(binary);
        }
        return expr;
    }

    QastNode parse_mul() {
        QastNode expr = parse_unary();
        while (check(TokenType::Star) || check(TokenType::Slash)) {
            const Token op = tokens_[pos_++];
            QastNode binary = make_node("Binary");
            binary.op = op.lexeme;
            binary.children.push_back(std::move(expr));
            binary.children.push_back(parse_unary());
            expr = std::move(binary);
        }
        return expr;
    }

    QastNode parse_unary() {
        if (match(TokenType::Minus)) {
            QastNode unary = make_node("Unary");
            unary.op = "-";
            unary.children.push_back(parse_unary());
            return unary;
        }
        return parse_primary();
    }

    QastNode parse_primary() {
        if (check(TokenType::Number)) {
            return make_value_node("Number", tokens_[pos_++].lexeme);
        }
        if (check(TokenType::Identifier)) {
            const Token id = tokens_[pos_++];
            if (match(TokenType::LParen)) {
                QastNode call = make_value_node("Call", id.lexeme);
                call.children = parse_args();
                expect(TokenType::RParen, "')'");
                return call;
            }
            if (match(TokenType::LBracket)) {
                if (match(TokenType::RBracket)) {
                    return make_value_node("ArrayRef", id.lexeme);
                }
                QastNode load = make_value_node("ArrayLoad", id.lexeme);
                load.children.push_back(parse_expression());
                expect(TokenType::RBracket, "']'");
                return load;
            }
            return make_value_node("Identifier", id.lexeme);
        }
        if (match(TokenType::LParen)) {
            QastNode expr = parse_expression();
            expect(TokenType::RParen, "')'");
            return expr;
        }
        const Token &token = peek();
        throw std::runtime_error("recursive parse error at " + std::to_string(token.line) + ":" +
                                 std::to_string(token.column) + ": expected expression");
    }

    std::vector<QastNode> parse_args() {
        std::vector<QastNode> args;
        if (check(TokenType::RParen)) {
            return args;
        }
        args.push_back(parse_expression());
        while (match(TokenType::Comma)) {
            if (check(TokenType::RParen)) {
                break;
            }
            args.push_back(parse_expression());
        }
        return args;
    }

    QastNode parse_condition() {
        QastNode condition = parse_and_condition();
        while (match(TokenType::OrOr)) {
            QastNode node = make_node("Or");
            node.children.push_back(std::move(condition));
            node.children.push_back(parse_and_condition());
            condition = std::move(node);
        }
        return condition;
    }

    QastNode parse_and_condition() {
        QastNode condition = parse_not_condition();
        while (match(TokenType::AndAnd)) {
            QastNode node = make_node("And");
            node.children.push_back(std::move(condition));
            node.children.push_back(parse_not_condition());
            condition = std::move(node);
        }
        return condition;
    }

    QastNode parse_not_condition() {
        if (match(TokenType::Bang)) {
            QastNode node = make_node("Not");
            node.children.push_back(parse_not_condition());
            return node;
        }
        if (check(TokenType::LParen) && !parenthesized_expression_is_relation_left()) {
            expect(TokenType::LParen, "'('");
            QastNode condition = parse_condition();
            expect(TokenType::RParen, "')'");
            return condition;
        }
        return parse_relation_condition();
    }

    QastNode parse_relation_condition() {
        QastNode left = parse_expression();
        if (is_relation_token(peek().type)) {
            const Token op = tokens_[pos_++];
            QastNode relation = make_node("Rel");
            relation.op = op.lexeme;
            relation.children.push_back(std::move(left));
            relation.children.push_back(parse_expression());
            return relation;
        }
        QastNode truth = make_node("Truth");
        truth.children.push_back(std::move(left));
        return truth;
    }

    bool parenthesized_expression_is_relation_left() const {
        const std::size_t close = matching_bracket(pos_, TokenType::LParen, TokenType::RParen);
        return is_relation_token(tokens_[close + 1].type);
    }

    std::size_t matching_bracket(std::size_t open_pos, TokenType open, TokenType close) const {
        int depth = 0;
        for (std::size_t i = open_pos; i < tokens_.size(); ++i) {
            if (tokens_[i].type == open) {
                ++depth;
            } else if (tokens_[i].type == close) {
                --depth;
                if (depth == 0) {
                    return i;
                }
            }
        }
        const Token &token = tokens_[open_pos];
        throw std::runtime_error("recursive parse error at " + std::to_string(token.line) + ":" +
                                 std::to_string(token.column) + ": unmatched bracket");
    }

    const std::vector<Token> &tokens_;
    std::size_t pos_ = 0;
};

class SlrQastParser {
public:
    explicit SlrQastParser(const std::vector<Token> &tokens)
        : tokens_(tokens), table_(build_qcompiler_slr_table()) {}

    QastNode parse() {
        std::vector<int> states{0};
        std::vector<QastValue> values;
        while (true) {
            const InputToken lookahead = read_lookahead();
            const auto action = table_.action(states.back(), lookahead.symbol);
            if (!action.has_value()) {
                error_expected(states.back(), lookahead);
            }
            if (action->kind == SlrActionKind::Shift) {
                states.push_back(action->target);
                values.push_back(lookahead.value);
                raw_pos_ = lookahead.end_pos;
                continue;
            }
            if (action->kind == SlrActionKind::Reduce) {
                const SlrProduction &production = table_.productions.at(static_cast<std::size_t>(action->target));
                std::vector<QastValue> rhs;
                for (std::size_t i = 0; i < production.rhs.size(); ++i) {
                    rhs.push_back(std::move(values.back()));
                    values.pop_back();
                    states.pop_back();
                }
                std::reverse(rhs.begin(), rhs.end());
                QastValue lhs = reduce(action->target, rhs);
                lhs.symbol = production.lhs;
                const auto next_state = table_.go_to(states.back(), production.lhs);
                if (!next_state.has_value()) {
                    throw std::runtime_error("internal SLR error: missing GOTO for " + production.lhs);
                }
                states.push_back(*next_state);
                values.push_back(std::move(lhs));
                continue;
            }
            return values.empty() ? make_node("Program") : values.back().node;
        }
    }

private:
    InputToken read_lookahead() const {
        const Token &token = tokens_.at(raw_pos_);
        QastValue value;
        value.symbol = token_terminal(token.type);
        value.text = token.lexeme;
        value.name = token.lexeme;
        const std::size_t end_pos = token.type == TokenType::End ? raw_pos_ : raw_pos_ + 1;
        return InputToken{value.symbol, std::move(value), raw_pos_, end_pos};
    }

    QastValue reduce(int production_index, const std::vector<QastValue> &rhs) {
        QastValue value;
        switch (production_index) {
        case kProgramBlock:
            value.node = make_node("Program");
            value.node.children = rhs[1].nodes;
            break;
        case kItemsItemItems:
            if (!rhs[0].node.kind.empty()) {
                value.nodes.push_back(rhs[0].node);
            }
            append_nodes(value.nodes, rhs[1].nodes);
            break;
        case kItemsEmpty:
            break;
        case kItemDeclaration:
        case kItemFunction:
        case kItemStatement:
        case kBlockItemDeclaration:
        case kBlockItemStatement:
        case kStatementBlock:
            value.node = rhs[0].node;
            break;
        case kDeclaration:
            value.node = make_value_node("Declaration", rhs[1].name);
            value.node.type = rhs[0].type;
            value.node.is_array = rhs[2].is_array;
            value.node.size_bytes = rhs[2].size_bytes;
            break;
        case kDeclSuffixArray:
            value.is_array = true;
            value.size_bytes = std::stoi(rhs[1].text) * 8;
            break;
        case kDeclSuffixScalar:
            value.is_array = false;
            value.size_bytes = 8;
            break;
        case kFunction:
            value.node = rhs[0].node;
            value.node.children.push_back(rhs[1].node);
            break;
        case kFunctionHeader:
            value.node = make_value_node("Function", rhs[1].name);
            value.node.type = rhs[0].type;
            value.node.children = rhs[3].params;
            break;
        case kTypeInt:
            value.type = "int";
            break;
        case kTypeVoid:
            value.type = "void";
            break;
        case kParamsParamParams:
            value.params = rhs[0].params;
            append_nodes(value.params, rhs[1].params);
            break;
        case kParamsEmpty:
            break;
        case kParam:
            value.node = make_value_node("Param", rhs[1].name);
            value.node.type = "int";
            value.node.is_array = rhs[2].is_array;
            value.params.push_back(value.node);
            break;
        case kParamSuffixArray:
            value.is_array = true;
            break;
        case kParamSuffixScalar:
            value.is_array = false;
            break;
        case kBlock:
            value.node = make_node("Block");
            value.node.children = rhs[1].nodes;
            break;
        case kBlockItemsBlockItemBlockItems:
            if (!rhs[0].node.kind.empty()) {
                value.nodes.push_back(rhs[0].node);
            }
            append_nodes(value.nodes, rhs[1].nodes);
            break;
        case kBlockItemsEmpty:
            break;
        case kStatementEmpty:
            value.node = make_node("EmptyStatement");
            break;
        case kStatementExpr:
            value.node = make_node("ExprStatement");
            value.node.children.push_back(rhs[0].node);
            break;
        case kStatementIf:
            value.node = make_node("If");
            value.node.children = {rhs[2].node, rhs[4].node};
            break;
        case kStatementIfElse:
            value.node = make_node("IfElse");
            value.node.children = {rhs[2].node, rhs[4].node, rhs[6].node};
            break;
        case kStatementWhile:
            value.node = make_node("While");
            value.node.children = {rhs[2].node, rhs[4].node};
            break;
        case kStatementReturnExpr:
            value.node = make_node("Return");
            value.node.children.push_back(rhs[1].node);
            break;
        case kStatementReturnEmpty:
            value.node = make_node("Return");
            break;
        case kExprScalarAssign:
        case kExprScalarPlusAssign:
            value.node = make_value_node("Assign", rhs[0].name);
            value.node.op = production_index == kExprScalarAssign ? "=" : "+=";
            value.node.children.push_back(rhs[2].node);
            break;
        case kExprArrayAssign:
        case kExprArrayPlusAssign:
            value.node = make_value_node("ArrayAssign", rhs[0].name);
            value.node.op = production_index == kExprArrayAssign ? "=" : "+=";
            value.node.children = {rhs[2].node, rhs[5].node};
            break;
        case kExprAdd:
        case kAddMul:
        case kMulUnary:
        case kUnaryPrimary:
        case kCondAnd:
        case kAndCondNot:
        case kNotCondRel:
            value.node = rhs[0].node;
            break;
        case kNotCondParen:
            value.node = rhs[1].node;
            break;
        case kAddPlus:
        case kAddMinus:
        case kMulTimes:
        case kMulDivide:
            value.node = make_node("Binary");
            value.node.op = rhs[1].text;
            value.node.children = {rhs[0].node, rhs[2].node};
            break;
        case kUnaryMinus:
            value.node = make_node("Unary");
            value.node.op = "-";
            value.node.children.push_back(rhs[1].node);
            break;
        case kPrimaryNumber:
            value.node = make_value_node("Number", rhs[0].text);
            break;
        case kPrimaryId:
            value.node = make_value_node("Identifier", rhs[0].name);
            break;
        case kPrimaryCall:
            value.node = make_value_node("Call", rhs[0].name);
            value.node.children = rhs[2].args;
            break;
        case kPrimaryArrayLoad:
            value.node = make_value_node("ArrayLoad", rhs[0].name);
            value.node.children.push_back(rhs[2].node);
            break;
        case kPrimaryArrayRef:
            value.node = make_value_node("ArrayRef", rhs[0].name);
            break;
        case kPrimaryParenExpr:
            value.node = rhs[1].node;
            break;
        case kArgsExprArgsTail:
            value.args.push_back(rhs[0].node);
            append_nodes(value.args, rhs[1].args);
            break;
        case kArgsEmpty:
        case kArgsTailEmpty:
            break;
        case kArgsTailCommaArgs:
            value.args = rhs[1].args;
            break;
        case kCondOr:
            value.node = make_node("Or");
            value.node.children = {rhs[0].node, rhs[2].node};
            break;
        case kAndCondAnd:
            value.node = make_node("And");
            value.node.children = {rhs[0].node, rhs[2].node};
            break;
        case kNotCondBang:
            value.node = make_node("Not");
            value.node.children.push_back(rhs[1].node);
            break;
        case kRelCondCompare:
            value.node = make_node("Rel");
            value.node.op = rhs[1].text;
            value.node.children = {rhs[0].node, rhs[2].node};
            break;
        case kRelCondExpr:
            value.node = make_node("Truth");
            value.node.children.push_back(rhs[0].node);
            break;
        case kRelOpLt:
        case kRelOpLe:
        case kRelOpGt:
        case kRelOpGe:
        case kRelOpEq:
        case kRelOpNe:
            value.text = rhs[0].text;
            break;
        default:
            break;
        }
        return value;
    }

    [[noreturn]] void error_expected(int state, const InputToken &lookahead) const {
        std::ostringstream expected;
        bool first = true;
        for (const auto &entry : table_.actions) {
            if (entry.first.first != state) {
                continue;
            }
            if (!first) {
                expected << ", ";
            }
            first = false;
            expected << entry.first.second;
        }
        const Token &token = tokens_.at(lookahead.start_pos);
        throw std::runtime_error("SLR QAST parse error at " + std::to_string(token.line) + ":" +
                                 std::to_string(token.column) + ": got " + lookahead.symbol +
                                 ", expected one of [" + expected.str() + "]");
    }

    const std::vector<Token> &tokens_;
    SlrTable table_;
    std::size_t raw_pos_ = 0;
};

class QastTranslator {
public:
    IRProgram translate(const QastNode &root) {
        if (root.kind != "Program") {
            throw std::runtime_error("QAST translation expects Program root");
        }
        for (const auto &child : root.children) {
            translate_top_level(child);
        }
        return context_.program;
    }

private:
    void translate_top_level(const QastNode &node) {
        if (node.kind == "Declaration") {
            translate_declaration(node);
            return;
        }
        if (node.kind == "Function") {
            translate_function(node);
            return;
        }
        append_code(context_.program.main, translate_statement(node));
    }

    void translate_declaration(const QastNode &node) {
        if (node.type == "void") {
            throw std::runtime_error("parse error: variables cannot have void type");
        }
        const SymbolKind kind =
            context_.in_function()
                ? (node.is_array ? SymbolKind::LocalArray : SymbolKind::LocalScalar)
                : (node.is_array ? SymbolKind::GlobalArray : SymbolKind::GlobalScalar);
        context_.add_symbol(node.value, kind, node.size_bytes);
        if (!context_.in_function()) {
            context_.program.globals.push_back(Global{node.value, node.size_bytes, kind});
        }
    }

    void translate_function(const QastNode &node) {
        if (context_.in_function()) {
            throw std::runtime_error("parse error: nested function definitions are not supported");
        }
        std::vector<QastNode> params;
        const QastNode *block = nullptr;
        for (const auto &child : node.children) {
            if (child.kind == "Param") {
                params.push_back(child);
            } else if (child.kind == "Block") {
                block = &child;
            }
        }
        if (block == nullptr) {
            throw std::runtime_error("QAST function missing block: " + node.value);
        }

        context_.program.functions.push_back(FunctionIR{node.value, node.type, {}, {}});
        context_.current_function_index = static_cast<int>(context_.program.functions.size()) - 1;
        context_.current_function_name = node.value;
        context_.locals.clear();
        context_.declare_function(node.value, static_cast<int>(params.size()));
        for (const auto &param : params) {
            context_.program.functions.back().params.push_back(param.value);
            context_.add_symbol(param.value, param.is_array ? SymbolKind::ParamArray : SymbolKind::ParamScalar, 8);
        }

        FunctionIR &function = context_.program.functions.back();
        function.instructions.push_back(IRInstruction{IROp::Label, function.name});
        append_code(function.instructions, translate_block(*block));
        if (function.instructions.empty() || function.instructions.back().op != IROp::Return) {
            function.instructions.push_back(IRInstruction{IROp::Return, "", "0"});
        }
        context_.current_function_index = -1;
        context_.current_function_name.clear();
        context_.locals.clear();
    }

    std::vector<IRInstruction> translate_block(const QastNode &node) {
        std::vector<IRInstruction> code;
        for (const auto &child : node.children) {
            if (child.kind == "Declaration") {
                translate_declaration(child);
            } else {
                append_code(code, translate_statement(child));
            }
        }
        return code;
    }

    std::vector<IRInstruction> translate_statement(const QastNode &node) {
        if (node.kind == "EmptyStatement") {
            return {};
        }
        if (node.kind == "Block") {
            return translate_block(node);
        }
        if (node.kind == "ExprStatement") {
            return translate_expr(node.children.at(0)).code;
        }
        if (node.kind == "If") {
            return translate_if(node, false);
        }
        if (node.kind == "IfElse") {
            return translate_if(node, true);
        }
        if (node.kind == "While") {
            return translate_while(node);
        }
        if (node.kind == "Return") {
            std::vector<IRInstruction> code;
            if (node.children.empty()) {
                code.push_back(IRInstruction{IROp::Return, "", "0"});
                return code;
            }
            const ExprResult value = translate_expr(node.children.at(0));
            code = value.code;
            code.push_back(IRInstruction{IROp::Return, "", value.place});
            return code;
        }
        throw std::runtime_error("unsupported QAST statement: " + node.kind);
    }

    std::vector<IRInstruction> translate_if(const QastNode &node, bool has_else) {
        const CondResult condition = translate_condition(node.children.at(0));
        std::vector<IRInstruction> code = condition.code;
        const std::string then_label = context_.make_label();
        const std::string else_or_end = context_.make_label();
        code.push_back(
            IRInstruction{IROp::If, "", condition.lhs, condition.rhs, condition.op, then_label, else_or_end});
        code.push_back(IRInstruction{IROp::Label, then_label});
        append_code(code, translate_statement(node.children.at(1)));
        if (!has_else) {
            code.push_back(IRInstruction{IROp::Label, else_or_end});
            return code;
        }
        const std::string end_label = context_.make_label();
        code.push_back(IRInstruction{IROp::Goto, end_label});
        code.push_back(IRInstruction{IROp::Label, else_or_end});
        append_code(code, translate_statement(node.children.at(2)));
        code.push_back(IRInstruction{IROp::Label, end_label});
        return code;
    }

    std::vector<IRInstruction> translate_while(const QastNode &node) {
        std::vector<IRInstruction> code;
        const std::string condition_label = context_.make_label();
        const std::string body_label = context_.make_label();
        const std::string end_label = context_.make_label();
        code.push_back(IRInstruction{IROp::Label, condition_label});
        const CondResult condition = translate_condition(node.children.at(0));
        append_code(code, condition.code);
        code.push_back(IRInstruction{IROp::If, "", condition.lhs, condition.rhs, condition.op, body_label, end_label});
        code.push_back(IRInstruction{IROp::Label, body_label});
        append_code(code, translate_statement(node.children.at(1)));
        code.push_back(IRInstruction{IROp::Goto, condition_label});
        code.push_back(IRInstruction{IROp::Label, end_label});
        return code;
    }

    ExprResult translate_expr(const QastNode &node) {
        if (node.kind == "Number") {
            ExprResult result;
            result.place = node.value;
            return result;
        }
        if (node.kind == "Identifier") {
            context_.require_symbol(node.value);
            ExprResult result;
            result.place = node.value;
            return result;
        }
        if (node.kind == "ArrayRef") {
            context_.require_array(node.value);
            ExprResult result;
            result.place = node.value;
            return result;
        }
        if (node.kind == "ArrayLoad") {
            return translate_array_load(node);
        }
        if (node.kind == "Call") {
            return translate_call(node);
        }
        if (node.kind == "Binary") {
            return translate_binary(node);
        }
        if (node.kind == "Unary") {
            return translate_unary(node);
        }
        if (node.kind == "Assign") {
            return translate_scalar_assign(node);
        }
        if (node.kind == "ArrayAssign") {
            return translate_array_assign(node);
        }
        throw std::runtime_error("unsupported QAST expression: " + node.kind);
    }

    ExprResult translate_scalar_assign(const QastNode &node) {
        context_.require_scalar_lvalue(node.value);
        const ExprResult rhs = translate_expr(node.children.at(0));
        ExprResult result;
        result.code = rhs.code;
        if (node.op == "+=") {
            const std::string temp = context_.make_temp();
            result.code.push_back(IRInstruction{IROp::Binary, temp, node.value, rhs.place, "+"});
            result.code.push_back(IRInstruction{IROp::Move, node.value, temp});
            result.place = temp;
        } else {
            result.code.push_back(IRInstruction{IROp::Move, node.value, rhs.place});
            result.place = rhs.place;
        }
        return result;
    }

    ExprResult translate_array_assign(const QastNode &node) {
        context_.require_array(node.value);
        const ExprResult index = translate_expr(node.children.at(0));
        ExprResult result;
        append_code(result.code, index.code);
        const std::string address = make_array_address(context_, result.code, node.value, index.place);
        if (node.op == "+=") {
            const std::string original = context_.make_temp();
            result.code.push_back(IRInstruction{IROp::Load, original, address});
            const ExprResult rhs = translate_expr(node.children.at(1));
            append_code(result.code, rhs.code);
            const std::string temp = context_.make_temp();
            result.code.push_back(IRInstruction{IROp::Binary, temp, original, rhs.place, "+"});
            result.code.push_back(IRInstruction{IROp::Store, "", address, temp});
            result.place = temp;
        } else {
            const ExprResult rhs = translate_expr(node.children.at(1));
            append_code(result.code, rhs.code);
            result.code.push_back(IRInstruction{IROp::Store, "", address, rhs.place});
            result.place = rhs.place;
        }
        return result;
    }

    ExprResult translate_binary(const QastNode &node) {
        const ExprResult left = translate_expr(node.children.at(0));
        const ExprResult right = translate_expr(node.children.at(1));
        ExprResult result;
        append_code(result.code, left.code);
        append_code(result.code, right.code);
        result.place = context_.make_temp();
        result.code.push_back(IRInstruction{IROp::Binary, result.place, left.place, right.place, node.op});
        return result;
    }

    ExprResult translate_unary(const QastNode &node) {
        const ExprResult operand = translate_expr(node.children.at(0));
        ExprResult result = operand;
        result.place = context_.make_temp();
        result.code.push_back(IRInstruction{IROp::Binary, result.place, "0", operand.place, "-"});
        return result;
    }

    ExprResult translate_call(const QastNode &node) {
        context_.require_function_call(node.value, static_cast<int>(node.children.size()));
        ExprResult result;
        std::vector<ExprResult> args;
        for (const auto &arg_node : node.children) {
            args.push_back(translate_expr(arg_node));
        }
        for (const auto &arg : args) {
            append_code(result.code, arg.code);
        }
        for (const auto &arg : args) {
            result.code.push_back(IRInstruction{IROp::Par, "", arg.place});
        }
        result.place = context_.make_temp();
        IRInstruction call;
        call.op = IROp::Call;
        call.dst = result.place;
        call.callee = node.value;
        call.arg_count = static_cast<int>(args.size());
        result.code.push_back(call);
        return result;
    }

    ExprResult translate_array_load(const QastNode &node) {
        context_.require_array(node.value);
        ExprResult result = translate_expr(node.children.at(0));
        const std::string address = make_array_address(context_, result.code, node.value, result.place);
        result.place = context_.make_temp();
        result.code.push_back(IRInstruction{IROp::Load, result.place, address});
        return result;
    }

    CondResult translate_condition(const QastNode &node) {
        if (node.kind == "Rel") {
            const ExprResult left = translate_expr(node.children.at(0));
            const ExprResult right = translate_expr(node.children.at(1));
            CondResult result;
            append_code(result.code, left.code);
            append_code(result.code, right.code);
            result.lhs = left.place;
            result.op = node.op;
            result.rhs = right.place;
            return result;
        }
        if (node.kind == "Truth") {
            const ExprResult expr = translate_expr(node.children.at(0));
            CondResult result;
            result.code = expr.code;
            result.lhs = expr.place;
            result.op = "!=";
            result.rhs = "0";
            return result;
        }
        if (node.kind == "Not") {
            CondResult result = translate_condition(node.children.at(0));
            result.op = invert_relation(result.op);
            return result;
        }
        if (node.kind == "And" || node.kind == "Or") {
            const CondResult left = translate_condition(node.children.at(0));
            const CondResult right = translate_condition(node.children.at(1));
            return materialize_binary_condition(left, right, node.kind == "And", context_);
        }
        throw std::runtime_error("unsupported QAST condition: " + node.kind);
    }

    TranslationContext context_;
};

void write_qast_text(const QastNode &node, int depth, std::ostringstream &out) {
    out << std::string(static_cast<std::size_t>(depth) * 2, ' ') << node.kind;
    if (!node.value.empty()) {
        out << " name=" << node.value;
    }
    if (!node.type.empty()) {
        out << " type=" << node.type;
    }
    if (!node.op.empty()) {
        out << " op=" << node.op;
    }
    if (node.is_array) {
        out << " array=true";
    }
    if (node.size_bytes != 8 || node.is_array) {
        out << " size=" << node.size_bytes;
    }
    out << '\n';
    for (const auto &child : node.children) {
        write_qast_text(child, depth + 1, out);
    }
}

} // namespace

QastNode parse_qast_recursive_descent(const std::vector<Token> &tokens) {
    return RecursiveQastParser(tokens).parse();
}

QastNode parse_qast_slr(const std::vector<Token> &tokens) {
    return SlrQastParser(tokens).parse();
}

IRProgram qast_to_ir(const QastNode &root) {
    return QastTranslator().translate(root);
}

std::string qast_to_text(const QastNode &root) {
    std::ostringstream out;
    write_qast_text(root, 0, out);
    return out.str();
}

} // namespace qcompiler
