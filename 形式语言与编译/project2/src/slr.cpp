#include "qcompiler/slr.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace qcompiler {

namespace {

struct Item {
    int production = 0;
    int dot = 0;

    bool operator<(const Item &other) const {
        if (production != other.production) {
            return production < other.production;
        }
        return dot < other.dot;
    }
};

using ItemSet = std::set<Item>;
using StringSet = std::set<std::string>;

bool is_nonterminal(const std::unordered_set<std::string> &nonterminals, const std::string &symbol) {
    return nonterminals.find(symbol) != nonterminals.end();
}

bool add_all(StringSet &target, const StringSet &source) {
    bool changed = false;
    for (const auto &value : source) {
        changed = target.insert(value).second || changed;
    }
    return changed;
}

std::vector<SlrProduction> qcompiler_grammar() {
    return {
        {"S'", {"Program"}},
        {"Program", {"LBRACE", "Items", "RBRACE"}},
        {"Items", {"Item", "Items"}},
        {"Items", {}},
        {"Item", {"Declaration"}},
        {"Item", {"Function"}},
        {"Item", {"Statement"}},
        {"Declaration", {"Type", "ID", "DeclSuffix", "SEMI"}},
        {"DeclSuffix", {"LBRACKET", "NUMBER", "RBRACKET"}},
        {"DeclSuffix", {}},
        {"Function", {"FunctionHeader", "Block"}},
        {"FunctionHeader", {"Type", "ID", "LPAREN", "Params", "RPAREN"}},
        {"Type", {"INT"}},
        {"Type", {"VOID"}},
        {"Params", {"Param", "Params"}},
        {"Params", {}},
        {"Param", {"INT", "ID", "ParamSuffix", "SEMI"}},
        {"ParamSuffix", {"LBRACKET", "RBRACKET"}},
        {"ParamSuffix", {}},
        {"Block", {"LBRACE", "BlockItems", "RBRACE"}},
        {"BlockItems", {"BlockItem", "BlockItems"}},
        {"BlockItems", {}},
        {"BlockItem", {"Declaration"}},
        {"BlockItem", {"Statement"}},
        {"Statement", {"SEMI"}},
        {"Statement", {"Expr", "SEMI"}},
        {"Statement", {"IF", "LPAREN", "Cond", "RPAREN", "Statement"}},
        {"Statement", {"IF", "LPAREN", "Cond", "RPAREN", "Statement", "ELSE", "Statement"}},
        {"Statement", {"WHILE", "LPAREN", "Cond", "RPAREN", "Statement"}},
        {"Statement", {"RETURN", "Expr", "SEMI"}},
        {"Statement", {"RETURN", "SEMI"}},
        {"Statement", {"Block"}},
        {"Expr", {"ID", "ASSIGN", "Expr"}},
        {"Expr", {"ID", "LBRACKET", "Expr", "RBRACKET", "ASSIGN", "Expr"}},
        {"Expr", {"ID", "PLUS_ASSIGN", "Expr"}},
        {"Expr", {"ID", "LBRACKET", "Expr", "RBRACKET", "PLUS_ASSIGN", "Expr"}},
        {"Expr", {"Add"}},
        {"Add", {"Add", "PLUS", "Mul"}},
        {"Add", {"Add", "MINUS", "Mul"}},
        {"Add", {"Mul"}},
        {"Mul", {"Mul", "STAR", "Unary"}},
        {"Mul", {"Mul", "SLASH", "Unary"}},
        {"Mul", {"Unary"}},
        {"Unary", {"MINUS", "Unary"}},
        {"Unary", {"Primary"}},
        {"Primary", {"NUMBER"}},
        {"Primary", {"ID"}},
        {"Primary", {"ID", "LPAREN", "Args", "RPAREN"}},
        {"Primary", {"ID", "LBRACKET", "Expr", "RBRACKET"}},
        {"Primary", {"ID", "LBRACKET", "RBRACKET"}},
        {"Primary", {"LPAREN", "Expr", "RPAREN"}},
        {"Args", {"Expr", "ArgsTail"}},
        {"Args", {}},
        {"ArgsTail", {"COMMA", "Args"}},
        {"ArgsTail", {}},
        {"Cond", {"Cond", "OR", "AndCond"}},
        {"Cond", {"AndCond"}},
        {"AndCond", {"AndCond", "AND", "NotCond"}},
        {"AndCond", {"NotCond"}},
        {"NotCond", {"BANG", "NotCond"}},
        {"NotCond", {"LPAREN", "Cond", "RPAREN"}},
        {"NotCond", {"RelCond"}},
        {"RelCond", {"Expr", "RelOp", "Expr"}},
        {"RelCond", {"Expr"}},
        {"RelOp", {"LT"}},
        {"RelOp", {"LE"}},
        {"RelOp", {"GT"}},
        {"RelOp", {"GE"}},
        {"RelOp", {"EQ"}},
        {"RelOp", {"NE"}},
    };
}

std::unordered_set<std::string> collect_nonterminals(const std::vector<SlrProduction> &productions) {
    std::unordered_set<std::string> nonterminals;
    for (const auto &production : productions) {
        nonterminals.insert(production.lhs);
    }
    return nonterminals;
}

std::unordered_map<std::string, std::vector<int>> productions_by_lhs(const std::vector<SlrProduction> &productions) {
    std::unordered_map<std::string, std::vector<int>> by_lhs;
    for (int i = 0; i < static_cast<int>(productions.size()); ++i) {
        by_lhs[productions[i].lhs].push_back(i);
    }
    return by_lhs;
}

std::unordered_map<std::string, StringSet> compute_first(const std::vector<SlrProduction> &productions,
                                                         const std::unordered_set<std::string> &nonterminals) {
    std::unordered_map<std::string, StringSet> first;
    for (const auto &production : productions) {
        first[production.lhs];
        for (const auto &symbol : production.rhs) {
            if (!is_nonterminal(nonterminals, symbol)) {
                first[symbol].insert(symbol);
            }
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &production : productions) {
            if (production.rhs.empty()) {
                changed = first[production.lhs].insert("ε").second || changed;
                continue;
            }
            bool nullable_prefix = true;
            for (const auto &symbol : production.rhs) {
                for (const auto &value : first[symbol]) {
                    if (value != "ε") {
                        changed = first[production.lhs].insert(value).second || changed;
                    }
                }
                if (first[symbol].find("ε") == first[symbol].end()) {
                    nullable_prefix = false;
                    break;
                }
            }
            if (nullable_prefix) {
                changed = first[production.lhs].insert("ε").second || changed;
            }
        }
    }
    return first;
}

StringSet first_of_sequence(const std::vector<std::string> &sequence,
                            std::size_t start,
                            const std::unordered_map<std::string, StringSet> &first) {
    StringSet result;
    if (start >= sequence.size()) {
        result.insert("ε");
        return result;
    }
    bool nullable_prefix = true;
    for (std::size_t i = start; i < sequence.size(); ++i) {
        const auto it = first.find(sequence[i]);
        if (it == first.end()) {
            result.insert(sequence[i]);
            nullable_prefix = false;
            break;
        }
        for (const auto &value : it->second) {
            if (value != "ε") {
                result.insert(value);
            }
        }
        if (it->second.find("ε") == it->second.end()) {
            nullable_prefix = false;
            break;
        }
    }
    if (nullable_prefix) {
        result.insert("ε");
    }
    return result;
}

std::unordered_map<std::string, StringSet> compute_follow(
    const std::vector<SlrProduction> &productions,
    const std::unordered_set<std::string> &nonterminals,
    const std::unordered_map<std::string, StringSet> &first) {
    std::unordered_map<std::string, StringSet> follow;
    for (const auto &nonterminal : nonterminals) {
        follow[nonterminal];
    }
    follow["S'"].insert("$");

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &production : productions) {
            for (std::size_t i = 0; i < production.rhs.size(); ++i) {
                const std::string &symbol = production.rhs[i];
                if (!is_nonterminal(nonterminals, symbol)) {
                    continue;
                }
                const StringSet suffix_first = first_of_sequence(production.rhs, i + 1, first);
                StringSet without_epsilon;
                for (const auto &value : suffix_first) {
                    if (value != "ε") {
                        without_epsilon.insert(value);
                    }
                }
                changed = add_all(follow[symbol], without_epsilon) || changed;
                if (suffix_first.find("ε") != suffix_first.end()) {
                    changed = add_all(follow[symbol], follow[production.lhs]) || changed;
                }
            }
        }
    }
    return follow;
}

ItemSet closure(const ItemSet &items,
                const std::vector<SlrProduction> &productions,
                const std::unordered_set<std::string> &nonterminals,
                const std::unordered_map<std::string, std::vector<int>> &by_lhs) {
    ItemSet result = items;
    bool changed = true;
    while (changed) {
        changed = false;
        std::vector<Item> snapshot(result.begin(), result.end());
        for (const auto &item : snapshot) {
            const auto &production = productions[item.production];
            if (item.dot >= static_cast<int>(production.rhs.size())) {
                continue;
            }
            const std::string &next = production.rhs[item.dot];
            if (!is_nonterminal(nonterminals, next)) {
                continue;
            }
            const auto found = by_lhs.find(next);
            if (found == by_lhs.end()) {
                continue;
            }
            for (int production_index : found->second) {
                changed = result.insert(Item{production_index, 0}).second || changed;
            }
        }
    }
    return result;
}

ItemSet go_to(const ItemSet &items,
              const std::string &symbol,
              const std::vector<SlrProduction> &productions,
              const std::unordered_set<std::string> &nonterminals,
              const std::unordered_map<std::string, std::vector<int>> &by_lhs) {
    ItemSet shifted;
    for (const auto &item : items) {
        const auto &production = productions[item.production];
        if (item.dot < static_cast<int>(production.rhs.size()) && production.rhs[item.dot] == symbol) {
            shifted.insert(Item{item.production, item.dot + 1});
        }
    }
    return closure(shifted, productions, nonterminals, by_lhs);
}

std::string action_to_string(const SlrAction &action) {
    switch (action.kind) {
    case SlrActionKind::Shift:
        return "s" + std::to_string(action.target);
    case SlrActionKind::Reduce:
        return "r" + std::to_string(action.target);
    case SlrActionKind::Accept:
        return "acc";
    }
    return "?";
}

bool is_shift_reduce_pair(const SlrAction &left, const SlrAction &right) {
    return (left.kind == SlrActionKind::Shift && right.kind == SlrActionKind::Reduce) ||
           (left.kind == SlrActionKind::Reduce && right.kind == SlrActionKind::Shift);
}

bool should_prefer_shift_for_conflict(const std::string &terminal) {
    return terminal == "ELSE" || terminal == "RPAREN";
}

void set_action(std::map<std::pair<int, std::string>, SlrAction> &actions,
                std::vector<std::string> &conflicts,
                std::vector<std::string> &resolved_conflicts,
                int state,
                const std::string &terminal,
                const SlrAction &action) {
    const auto key = std::make_pair(state, terminal);
    const auto found = actions.find(key);
    if (found == actions.end() || action_to_string(found->second) == action_to_string(action)) {
        actions[key] = action;
        return;
    }

    if (should_prefer_shift_for_conflict(terminal) && is_shift_reduce_pair(found->second, action)) {
        const SlrAction chosen = found->second.kind == SlrActionKind::Shift ? found->second : action;
        resolved_conflicts.push_back("state " + std::to_string(state) + ", terminal " + terminal + ": " +
                                     action_to_string(found->second) + " vs " + action_to_string(action) +
                                     " -> " + action_to_string(chosen));
        actions[key] = chosen;
        return;
    }

    conflicts.push_back("state " + std::to_string(state) + ", terminal " + terminal + ": " +
                        action_to_string(found->second) + " vs " + action_to_string(action));
}

SlrTable build_table() {
    SlrTable table;
    table.productions = qcompiler_grammar();
    const auto nonterminals = collect_nonterminals(table.productions);
    const auto by_lhs = productions_by_lhs(table.productions);
    const auto first = compute_first(table.productions, nonterminals);
    const auto follow = compute_follow(table.productions, nonterminals, first);

    std::set<std::string> symbols;
    for (const auto &production : table.productions) {
        for (const auto &symbol : production.rhs) {
            symbols.insert(symbol);
        }
    }

    std::vector<ItemSet> states;
    std::map<ItemSet, int> state_ids;
    std::queue<int> pending;
    const ItemSet start = closure(ItemSet{Item{0, 0}}, table.productions, nonterminals, by_lhs);
    states.push_back(start);
    state_ids[start] = 0;
    pending.push(0);

    while (!pending.empty()) {
        const int state_id = pending.front();
        pending.pop();
        for (const auto &symbol : symbols) {
            const ItemSet next = go_to(states[state_id], symbol, table.productions, nonterminals, by_lhs);
            if (next.empty()) {
                continue;
            }
            auto inserted = state_ids.emplace(next, static_cast<int>(states.size()));
            if (inserted.second) {
                states.push_back(next);
                pending.push(inserted.first->second);
            }
            const int next_id = inserted.first->second;
            if (is_nonterminal(nonterminals, symbol)) {
                table.gotos[{state_id, symbol}] = next_id;
            } else {
                set_action(table.actions,
                           table.conflicts,
                           table.resolved_conflicts,
                           state_id,
                           symbol,
                           SlrAction{SlrActionKind::Shift, next_id});
            }
        }

        for (const auto &item : states[state_id]) {
            const auto &production = table.productions[item.production];
            if (item.dot != static_cast<int>(production.rhs.size())) {
                continue;
            }
            if (item.production == 0) {
                set_action(table.actions,
                           table.conflicts,
                           table.resolved_conflicts,
                           state_id,
                           "$",
                           SlrAction{SlrActionKind::Accept, 0});
                continue;
            }
            const auto follow_it = follow.find(production.lhs);
            if (follow_it == follow.end()) {
                continue;
            }
            for (const auto &terminal : follow_it->second) {
                set_action(table.actions,
                           table.conflicts,
                           table.resolved_conflicts,
                           state_id,
                           terminal,
                           SlrAction{SlrActionKind::Reduce, item.production});
            }
        }
    }

    table.state_count = static_cast<int>(states.size());
    return table;
}

} // namespace

std::optional<SlrAction> SlrTable::action(int state, const std::string &terminal) const {
    const auto found = actions.find({state, terminal});
    if (found == actions.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<int> SlrTable::go_to(int state, const std::string &nonterminal) const {
    const auto found = gotos.find({state, nonterminal});
    if (found == gotos.end()) {
        return std::nullopt;
    }
    return found->second;
}

const SlrTable &build_qcompiler_slr_table() {
    static const SlrTable table = build_table();
    if (!table.conflicts.empty()) {
        throw std::runtime_error("unresolved SLR conflicts remain");
    }
    return table;
}

SlrTableSummary build_qsort_slr_table() {
    const SlrTable &table = build_qcompiler_slr_table();
    SlrTableSummary summary;
    summary.productions = table.productions;
    summary.state_count = table.state_count;
    summary.action_count = static_cast<int>(table.actions.size());
    summary.goto_count = static_cast<int>(table.gotos.size());
    summary.conflicts = table.conflicts;
    summary.resolved_conflicts = table.resolved_conflicts;
    for (const auto &entry : table.actions) {
        summary.action_entries.push_back("ACTION[" + std::to_string(entry.first.first) + ", " +
                                         entry.first.second + "] = " + action_to_string(entry.second));
    }
    for (const auto &entry : table.gotos) {
        summary.goto_entries.push_back("GOTO[" + std::to_string(entry.first.first) + ", " +
                                       entry.first.second + "] = " + std::to_string(entry.second));
    }
    return summary;
}

std::string slr_table_to_text(const SlrTableSummary &summary) {
    std::ostringstream out;
    out << "QCompiler C-route SLR(1) table\n";
    out << "states: " << summary.state_count << '\n';
    out << "actions: " << summary.action_count << '\n';
    out << "gotos: " << summary.goto_count << '\n';
    out << "unresolved conflicts: " << summary.conflicts.size() << '\n';
    out << "resolved conflicts: " << summary.resolved_conflicts.size() << '\n';
    for (const auto &conflict : summary.conflicts) {
        out << "  unresolved: " << conflict << '\n';
    }
    for (const auto &conflict : summary.resolved_conflicts) {
        out << "  resolved: " << conflict << '\n';
    }

    out << "\naction table:\n";
    for (const auto &entry : summary.action_entries) {
        out << entry << '\n';
    }
    out << "\ngoto table:\n";
    for (const auto &entry : summary.goto_entries) {
        out << entry << '\n';
    }
    out << "\nproductions:\n";
    for (std::size_t i = 0; i < summary.productions.size(); ++i) {
        out << i << ": " << summary.productions[i].lhs << " -> ";
        if (summary.productions[i].rhs.empty()) {
            out << "ε";
        } else {
            for (std::size_t j = 0; j < summary.productions[i].rhs.size(); ++j) {
                if (j != 0) {
                    out << ' ';
                }
                out << summary.productions[i].rhs[j];
            }
        }
        out << '\n';
    }
    return out.str();
}

} // namespace qcompiler
