#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace qcompiler {

struct SlrProduction {
    std::string lhs;
    std::vector<std::string> rhs;
};

struct SlrTableSummary {
    std::vector<SlrProduction> productions;
    int state_count = 0;
    int action_count = 0;
    int goto_count = 0;
    std::vector<std::string> conflicts;
    std::vector<std::string> resolved_conflicts;
    std::vector<std::string> action_entries;
    std::vector<std::string> goto_entries;
};

enum class SlrActionKind {
    Shift,
    Reduce,
    Accept
};

struct SlrAction {
    SlrActionKind kind = SlrActionKind::Shift;
    int target = 0;
};

struct SlrTable {
    std::vector<SlrProduction> productions;
    std::map<std::pair<int, std::string>, SlrAction> actions;
    std::map<std::pair<int, std::string>, int> gotos;
    std::vector<std::string> conflicts;
    std::vector<std::string> resolved_conflicts;
    int state_count = 0;

    std::optional<SlrAction> action(int state, const std::string &terminal) const;
    std::optional<int> go_to(int state, const std::string &nonterminal) const;
};

const SlrTable &build_qcompiler_slr_table();
SlrTableSummary build_qsort_slr_table();
std::string slr_table_to_text(const SlrTableSummary &summary);

} // namespace qcompiler
