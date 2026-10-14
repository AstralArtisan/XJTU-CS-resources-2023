#include "qcompiler/parser.h"

#include "qcompiler/slr.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace qcompiler {

namespace {

struct ParamInfo {
    std::string name;
    bool array = false;
};

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

struct SemanticValue {
    std::string symbol;
    std::string text;
    std::string type;
    std::string name;
    bool is_array = false;
    int size_bytes = 8;
    std::vector<ParamInfo> params;
    std::vector<ExprResult> args;
    std::vector<IRInstruction> code;
    ExprResult expr;
    CondResult condition;
    int function_index = -1;
};

struct InputToken {
    std::string symbol;
    SemanticValue value;
    std::size_t start_pos = 0;
    std::size_t end_pos = 0;
};

// Production indexes mirror qcompiler_grammar() in slr.cpp.
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

void append_code(std::vector<IRInstruction> &target, const std::vector<IRInstruction> &source) {
    target.insert(target.end(), source.begin(), source.end());
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

class SlrSemanticParser {
public:
    explicit SlrSemanticParser(std::vector<Token> tokens)
        : tokens_(std::move(tokens)), table_(build_qcompiler_slr_table()) {}

    IRProgram parse() {
        std::vector<int> states{0};
        std::vector<SemanticValue> values;
        while (true) {
            const InputToken lookahead = read_lookahead();
            const auto action = table_.action(states.back(), lookahead.symbol);
            if (!action.has_value()) {
                error_expected(states.back(), lookahead);
            }
            if (action->kind == SlrActionKind::Shift) {
                states.push_back(action->target);
                values.push_back(lookahead.value);
                consume_lookahead(lookahead);
                continue;
            }
            if (action->kind == SlrActionKind::Reduce) {
                const SlrProduction &production = table_.productions.at(static_cast<std::size_t>(action->target));
                std::vector<SemanticValue> rhs;
                for (std::size_t i = 0; i < production.rhs.size(); ++i) {
                    rhs.push_back(std::move(values.back()));
                    values.pop_back();
                    states.pop_back();
                }
                std::reverse(rhs.begin(), rhs.end());
                SemanticValue lhs = reduce(action->target, rhs);
                lhs.symbol = production.lhs;
                const auto next_state = table_.go_to(states.back(), production.lhs);
                if (!next_state.has_value()) {
                    throw std::runtime_error("internal SLR error: missing GOTO for " + production.lhs);
                }
                states.push_back(*next_state);
                values.push_back(std::move(lhs));
                continue;
            }
            context_.program.main = values.empty() ? std::vector<IRInstruction>{} : values.back().code;
            return context_.program;
        }
    }

private:
    InputToken read_lookahead() const {
        const Token &token = tokens_.at(raw_pos_);
        SemanticValue value;
        value.symbol = token_terminal(token.type);
        value.text = token.lexeme;
        value.name = token.lexeme;
        const std::size_t end_pos = token.type == TokenType::End ? raw_pos_ : raw_pos_ + 1;
        return InputToken{value.symbol, std::move(value), raw_pos_, end_pos};
    }

    void consume_lookahead(const InputToken &lookahead) {
        raw_pos_ = lookahead.end_pos;
    }

    SemanticValue reduce(int production_index, const std::vector<SemanticValue> &rhs) {
        SemanticValue value;
        switch (production_index) {
        case kProgramBlock:
            value.code = rhs[1].code;
            break;
        case kItemsItemItems:
            value.code = rhs[0].code;
            append_code(value.code, rhs[1].code);
            break;
        case kItemsEmpty:
        case kItemDeclaration:
        case kItemFunction:
        case kBlockItemDeclaration:
            break;
        case kItemStatement:
        case kBlockItemStatement:
        case kStatementBlock:
            value.code = rhs[0].code;
            break;
        case kDeclaration:
            reduce_declaration(rhs[0], rhs[1], rhs[2]);
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
            reduce_function(rhs[0], rhs[1]);
            break;
        case kFunctionHeader:
            value.function_index = reduce_function_header(rhs[0], rhs[1], rhs[3]);
            break;
        case kTypeInt:
            value.type = "int";
            break;
        case kTypeVoid:
            value.type = "void";
            break;
        case kParamsParamParams:
            value.params = rhs[0].params;
            value.params.insert(value.params.end(), rhs[1].params.begin(), rhs[1].params.end());
            break;
        case kParamsEmpty:
            break;
        case kParam:
            value.params.push_back(ParamInfo{rhs[1].name, rhs[2].is_array});
            break;
        case kParamSuffixArray:
            value.is_array = true;
            break;
        case kParamSuffixScalar:
            value.is_array = false;
            break;
        case kBlock:
            value.code = rhs[1].code;
            break;
        case kBlockItemsBlockItemBlockItems:
            value.code = rhs[0].code;
            append_code(value.code, rhs[1].code);
            break;
        case kBlockItemsEmpty:
        case kStatementEmpty:
            break;
        case kStatementExpr:
            value.code = rhs[0].expr.code;
            break;
        case kStatementIf:
            value.code = reduce_if_without_else(rhs[2].condition, rhs[4].code);
            break;
        case kStatementIfElse:
            value.code = reduce_if_with_else(rhs[2].condition, rhs[4].code, rhs[6].code);
            break;
        case kStatementWhile:
            value.code = reduce_while(rhs[2].condition, rhs[4].code);
            break;
        case kStatementReturnExpr:
            value.code = rhs[1].expr.code;
            value.code.push_back(IRInstruction{IROp::Return, "", rhs[1].expr.place});
            break;
        case kStatementReturnEmpty:
            value.code.push_back(IRInstruction{IROp::Return, "", "0"});
            break;
        case kExprScalarAssign:
            value.expr = reduce_scalar_assign(rhs[0].name, rhs[2].expr, false);
            break;
        case kExprArrayAssign:
            value.expr = reduce_array_assign(rhs[0].name, rhs[2].expr, rhs[5].expr, false);
            break;
        case kExprScalarPlusAssign:
            value.expr = reduce_scalar_assign(rhs[0].name, rhs[2].expr, true);
            break;
        case kExprArrayPlusAssign:
            value.expr = reduce_array_assign(rhs[0].name, rhs[2].expr, rhs[5].expr, true);
            break;
        case kExprAdd:
        case kAddMul:
        case kMulUnary:
        case kUnaryPrimary:
            value.expr = rhs[0].expr;
            break;
        case kAddPlus:
        case kAddMinus:
        case kMulTimes:
        case kMulDivide:
            value.expr = reduce_binary_expr(rhs[0].expr, rhs[2].expr, rhs[1].text);
            break;
        case kUnaryMinus:
            value.expr = reduce_unary_minus(rhs[1].expr);
            break;
        case kPrimaryNumber:
            value.expr.place = rhs[0].text;
            break;
        case kPrimaryId:
            context_.require_symbol(rhs[0].name);
            value.expr.place = rhs[0].name;
            break;
        case kPrimaryCall:
            value.expr = reduce_call(rhs[0].name, rhs[2].args);
            break;
        case kPrimaryArrayLoad:
            value.expr = reduce_array_load(rhs[0].name, rhs[2].expr);
            break;
        case kPrimaryArrayRef:
            context_.require_array(rhs[0].name);
            value.expr.place = rhs[0].name;
            break;
        case kPrimaryParenExpr:
            value.expr = rhs[1].expr;
            break;
        case kArgsExprArgsTail:
            value.args.push_back(rhs[0].expr);
            value.args.insert(value.args.end(), rhs[1].args.begin(), rhs[1].args.end());
            break;
        case kArgsEmpty:
            break;
        case kArgsTailCommaArgs:
            value.args = rhs[1].args;
            break;
        case kArgsTailEmpty:
            break;
        case kCondOr:
            value.condition = materialize_binary_condition(rhs[0].condition, rhs[2].condition, false, context_);
            break;
        case kCondAnd:
        case kAndCondNot:
        case kNotCondRel:
        case kRelCondExpr:
            value.condition = rhs[0].condition;
            break;
        case kAndCondAnd:
            value.condition = materialize_binary_condition(rhs[0].condition, rhs[2].condition, true, context_);
            break;
        case kNotCondBang:
            value.condition = rhs[1].condition;
            value.condition.op = invert_relation(value.condition.op);
            break;
        case kNotCondParen:
            value.condition = rhs[1].condition;
            break;
        case kRelCondCompare:
            value.condition = reduce_relation(rhs[0].expr, rhs[1].text, rhs[2].expr);
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

    ExprResult reduce_scalar_assign(const std::string &name, const ExprResult &rhs, bool plus_assign) {
        context_.require_scalar_lvalue(name);
        ExprResult result;
        result.code = rhs.code;
        if (plus_assign) {
            const std::string temp = context_.make_temp();
            result.code.push_back(IRInstruction{IROp::Binary, temp, name, rhs.place, "+"});
            result.code.push_back(IRInstruction{IROp::Move, name, temp});
            result.place = temp;
        } else {
            result.code.push_back(IRInstruction{IROp::Move, name, rhs.place});
            result.place = rhs.place;
        }
        return result;
    }

    ExprResult reduce_array_assign(const std::string &name,
                                   const ExprResult &index,
                                   const ExprResult &rhs,
                                   bool plus_assign) {
        context_.require_array(name);
        ExprResult result;
        append_code(result.code, index.code);
        const std::string address = make_array_address(context_, result.code, name, index.place);
        if (plus_assign) {
            const std::string original = context_.make_temp();
            result.code.push_back(IRInstruction{IROp::Load, original, address});
            append_code(result.code, rhs.code);
            const std::string temp = context_.make_temp();
            result.code.push_back(IRInstruction{IROp::Binary, temp, original, rhs.place, "+"});
            result.code.push_back(IRInstruction{IROp::Store, "", address, temp});
            result.place = temp;
        } else {
            append_code(result.code, rhs.code);
            result.code.push_back(IRInstruction{IROp::Store, "", address, rhs.place});
            result.place = rhs.place;
        }
        return result;
    }

    ExprResult reduce_binary_expr(const ExprResult &left, const ExprResult &right, const std::string &op) {
        ExprResult result;
        append_code(result.code, left.code);
        append_code(result.code, right.code);
        result.place = context_.make_temp();
        result.code.push_back(IRInstruction{IROp::Binary, result.place, left.place, right.place, op});
        return result;
    }

    ExprResult reduce_unary_minus(const ExprResult &operand) {
        ExprResult result = operand;
        result.place = context_.make_temp();
        result.code.push_back(IRInstruction{IROp::Binary, result.place, "0", operand.place, "-"});
        return result;
    }

    ExprResult reduce_call(const std::string &name, const std::vector<ExprResult> &args) {
        context_.require_function_call(name, static_cast<int>(args.size()));
        ExprResult result;
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
        call.callee = name;
        call.arg_count = static_cast<int>(args.size());
        result.code.push_back(call);
        return result;
    }

    ExprResult reduce_array_load(const std::string &name, const ExprResult &index) {
        context_.require_array(name);
        ExprResult result = index;
        const std::string address = make_array_address(context_, result.code, name, index.place);
        result.place = context_.make_temp();
        result.code.push_back(IRInstruction{IROp::Load, result.place, address});
        return result;
    }

    CondResult reduce_relation(const ExprResult &left, const std::string &op, const ExprResult &right) {
        CondResult result;
        append_code(result.code, left.code);
        append_code(result.code, right.code);
        result.lhs = left.place;
        result.op = op;
        result.rhs = right.place;
        return result;
    }

    void reduce_declaration(const SemanticValue &type_value,
                            const SemanticValue &id_value,
                            const SemanticValue &suffix_value) {
        if (type_value.type == "void") {
            throw std::runtime_error("parse error: variables cannot have void type");
        }
        const SymbolKind kind =
            context_.in_function()
                ? (suffix_value.is_array ? SymbolKind::LocalArray : SymbolKind::LocalScalar)
                : (suffix_value.is_array ? SymbolKind::GlobalArray : SymbolKind::GlobalScalar);
        context_.add_symbol(id_value.name, kind, suffix_value.size_bytes);
        if (!context_.in_function()) {
            context_.program.globals.push_back(Global{id_value.name, suffix_value.size_bytes, kind});
        }
    }

    int reduce_function_header(const SemanticValue &type_value,
                               const SemanticValue &id_value,
                               const SemanticValue &params_value) {
        if (context_.in_function()) {
            throw std::runtime_error("parse error: nested function definitions are not supported");
        }
        context_.program.functions.push_back(FunctionIR{id_value.name, type_value.type, {}, {}});
        context_.current_function_index = static_cast<int>(context_.program.functions.size()) - 1;
        context_.current_function_name = id_value.name;
        context_.locals.clear();
        context_.declare_function(id_value.name, static_cast<int>(params_value.params.size()));
        for (const auto &param : params_value.params) {
            context_.program.functions.back().params.push_back(param.name);
            context_.add_symbol(param.name, param.array ? SymbolKind::ParamArray : SymbolKind::ParamScalar, 8);
        }
        return context_.current_function_index;
    }

    void reduce_function(const SemanticValue &header_value, const SemanticValue &block_value) {
        if (header_value.function_index < 0 ||
            header_value.function_index >= static_cast<int>(context_.program.functions.size())) {
            throw std::runtime_error("internal SLR error: invalid function header");
        }
        FunctionIR &function = context_.program.functions[static_cast<std::size_t>(header_value.function_index)];
        function.instructions.push_back(IRInstruction{IROp::Label, function.name});
        append_code(function.instructions, block_value.code);
        if (function.instructions.empty() || function.instructions.back().op != IROp::Return) {
            function.instructions.push_back(IRInstruction{IROp::Return, "", "0"});
        }
        context_.current_function_index = -1;
        context_.current_function_name.clear();
        context_.locals.clear();
    }

    std::vector<IRInstruction> reduce_if_without_else(const CondResult &condition,
                                                      const std::vector<IRInstruction> &then_code) {
        std::vector<IRInstruction> code = condition.code;
        const std::string then_label = context_.make_label();
        const std::string end_label = context_.make_label();
        code.push_back(IRInstruction{IROp::If, "", condition.lhs, condition.rhs, condition.op, then_label, end_label});
        code.push_back(IRInstruction{IROp::Label, then_label});
        append_code(code, then_code);
        code.push_back(IRInstruction{IROp::Label, end_label});
        return code;
    }

    std::vector<IRInstruction> reduce_if_with_else(const CondResult &condition,
                                                   const std::vector<IRInstruction> &then_code,
                                                   const std::vector<IRInstruction> &else_code) {
        std::vector<IRInstruction> code = condition.code;
        const std::string then_label = context_.make_label();
        const std::string else_label = context_.make_label();
        const std::string end_label = context_.make_label();
        code.push_back(IRInstruction{IROp::If, "", condition.lhs, condition.rhs, condition.op, then_label, else_label});
        code.push_back(IRInstruction{IROp::Label, then_label});
        append_code(code, then_code);
        code.push_back(IRInstruction{IROp::Goto, end_label});
        code.push_back(IRInstruction{IROp::Label, else_label});
        append_code(code, else_code);
        code.push_back(IRInstruction{IROp::Label, end_label});
        return code;
    }

    std::vector<IRInstruction> reduce_while(const CondResult &condition, const std::vector<IRInstruction> &body_code) {
        std::vector<IRInstruction> code;
        const std::string condition_label = context_.make_label();
        const std::string body_label = context_.make_label();
        const std::string end_label = context_.make_label();
        code.push_back(IRInstruction{IROp::Label, condition_label});
        append_code(code, condition.code);
        code.push_back(IRInstruction{IROp::If, "", condition.lhs, condition.rhs, condition.op, body_label, end_label});
        code.push_back(IRInstruction{IROp::Label, body_label});
        append_code(code, body_code);
        code.push_back(IRInstruction{IROp::Goto, condition_label});
        code.push_back(IRInstruction{IROp::Label, end_label});
        return code;
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
        throw std::runtime_error("SLR parse error at " + std::to_string(token.line) + ":" +
                                 std::to_string(token.column) + ": got " + lookahead.symbol +
                                 ", expected one of [" + expected.str() + "]");
    }

    std::vector<Token> tokens_;
    SlrTable table_;
    TranslationContext context_;
    std::size_t raw_pos_ = 0;
};

} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

IRProgram Parser::parse_program() {
    return SlrSemanticParser(tokens_).parse();
}

} // namespace qcompiler
