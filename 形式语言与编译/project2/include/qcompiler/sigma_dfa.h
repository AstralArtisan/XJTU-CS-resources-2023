#pragma once

#include "qcompiler/ir.h"

#include <array>
#include <string>
#include <vector>

namespace qcompiler {

enum class TemplateKind {
    Label,
    Goto,
    Return,
    If,
    IfLabel,
    Aop,
    Move,
    Ldr0,
    Ldr2,
    Str0,
    Str2,
    ParCall,
    Nop
};

std::string template_kind_name(TemplateKind kind);

struct TemplateMatch {
    TemplateKind kind = TemplateKind::Nop;
    std::vector<const IRInstruction *> insns;
    int meta = 0;
};

struct SigmaDfa {
    static const SigmaDfa &instance();
    std::string to_text() const;

    std::vector<std::string> states;
    std::vector<std::string> accept_states;
    std::vector<std::string> transitions;
    std::vector<std::string> alphabet;
};

std::vector<TemplateMatch> match_templates(const std::vector<IRInstruction> &ir);

std::array<int, 13> template_histogram(const std::vector<TemplateMatch> &matches);

} // namespace qcompiler
