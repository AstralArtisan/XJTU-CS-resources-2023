#include "qcompiler/sigma_dfa.h"

#include <sstream>
#include <stdexcept>

namespace qcompiler {

namespace {

bool addr_then_load(const IRInstruction &binary, const IRInstruction &load) {
    return binary.address_calc && load.op == IROp::Load && load.lhs == binary.dst;
}

bool addr_then_store(const IRInstruction &binary, const IRInstruction &store) {
    return binary.address_calc && store.op == IROp::Store && store.lhs == binary.dst;
}

bool if_then_fall_through_label(const IRInstruction &branch, const IRInstruction &label) {
    if (branch.op != IROp::If || label.op != IROp::Label) {
        return false;
    }
    return label.dst == branch.label_true || label.dst == branch.label_false;
}

} // namespace

std::string template_kind_name(TemplateKind kind) {
    switch (kind) {
    case TemplateKind::Label:
        return "T_LABEL";
    case TemplateKind::Goto:
        return "T_GOTO";
    case TemplateKind::Return:
        return "T_RETURN";
    case TemplateKind::If:
        return "T_IF";
    case TemplateKind::IfLabel:
        return "T_IF_LABEL";
    case TemplateKind::Aop:
        return "T_AOP";
    case TemplateKind::Move:
        return "T_MOVE";
    case TemplateKind::Ldr0:
        return "T_LDR0";
    case TemplateKind::Ldr2:
        return "T_LDR2";
    case TemplateKind::Str0:
        return "T_STR0";
    case TemplateKind::Str2:
        return "T_STR2";
    case TemplateKind::ParCall:
        return "T_PAR_CALL";
    case TemplateKind::Nop:
        return "T_NOP";
    }
    return "?";
}

std::vector<TemplateMatch> match_templates(const std::vector<IRInstruction> &ir) {
    std::vector<TemplateMatch> out;
    std::vector<const IRInstruction *> par_buffer;
    const std::size_t n = ir.size();

    for (std::size_t i = 0; i < n; ++i) {
        const IRInstruction &cur = ir[i];

        if (cur.op == IROp::Par) {
            par_buffer.push_back(&cur);
            continue;
        }
        if (cur.op == IROp::Call) {
            if (par_buffer.size() > 8) {
                throw std::runtime_error("sigma-dfa: more than 8 PAR before CALL");
            }
            TemplateMatch match;
            match.kind = TemplateKind::ParCall;
            match.insns = par_buffer;
            match.insns.push_back(&cur);
            out.push_back(std::move(match));
            par_buffer.clear();
            continue;
        }
        if (!par_buffer.empty()) {
            throw std::runtime_error("sigma-dfa: PAR not followed by CALL");
        }

        if (cur.op == IROp::If && i + 1 < n && if_then_fall_through_label(cur, ir[i + 1])) {
            TemplateMatch match;
            match.kind = TemplateKind::IfLabel;
            match.insns = {&cur, &ir[i + 1]};
            match.meta = ir[i + 1].dst == cur.label_true ? 1 : 2;
            out.push_back(std::move(match));
            ++i;
            continue;
        }

        if (cur.op == IROp::Binary && cur.address_calc && i + 1 < n) {
            if (addr_then_load(cur, ir[i + 1])) {
                TemplateMatch match;
                match.kind = TemplateKind::Ldr2;
                match.insns = {&cur, &ir[i + 1]};
                out.push_back(std::move(match));
                ++i;
                continue;
            }
            if (addr_then_store(cur, ir[i + 1])) {
                TemplateMatch match;
                match.kind = TemplateKind::Str2;
                match.insns = {&cur, &ir[i + 1]};
                out.push_back(std::move(match));
                ++i;
                continue;
            }
        }

        TemplateMatch match;
        match.insns = {&cur};
        switch (cur.op) {
        case IROp::Label:
            match.kind = TemplateKind::Label;
            break;
        case IROp::Goto:
            match.kind = TemplateKind::Goto;
            break;
        case IROp::Return:
            match.kind = TemplateKind::Return;
            break;
        case IROp::If:
            match.kind = TemplateKind::If;
            break;
        case IROp::Binary:
            match.kind = TemplateKind::Aop;
            break;
        case IROp::Move:
            match.kind = TemplateKind::Move;
            break;
        case IROp::Load:
            match.kind = TemplateKind::Ldr0;
            break;
        case IROp::Store:
            match.kind = TemplateKind::Str0;
            break;
        case IROp::Nop:
            match.kind = TemplateKind::Nop;
            break;
        case IROp::Par:
        case IROp::Call:
            throw std::runtime_error("sigma-dfa: unreachable PAR/CALL");
        }
        out.push_back(std::move(match));
    }

    if (!par_buffer.empty()) {
        throw std::runtime_error("sigma-dfa: trailing PAR without CALL");
    }
    return out;
}

std::array<int, 13> template_histogram(const std::vector<TemplateMatch> &matches) {
    std::array<int, 13> hist{};
    for (const auto &match : matches) {
        hist[static_cast<std::size_t>(match.kind)] += 1;
    }
    return hist;
}

const SigmaDfa &SigmaDfa::instance() {
    static const SigmaDfa dfa = [] {
        SigmaDfa d;
        d.alphabet = {
            "L  (Label)",   "G  (Goto)",     "R  (Return)",       "I  (If)",
            "M  (Move)",    "A  (Binary)",   "D  (Binary addr)",  "LD (Load)",
            "ST (Store)",   "P  (Par)",      "C  (Call)",         "N  (Nop)",
        };
        d.states = {
            "S0       initial",       "S_IF     after I, lookahead",
            "S_ADDR   after D, lookahead", "S_PAR_n  after n PAR (1..8)",
        };
        d.accept_states = {
            "ACC_T_LABEL    (Label)",
            "ACC_T_GOTO     (Goto)",
            "ACC_T_RETURN   (Return)",
            "ACC_T_IF       (If, no fall-through label)",
            "ACC_T_IF_LABEL (If; Label where label is one of If branches)",
            "ACC_T_AOP      (Binary non-addr, or addr without matching Load/Store)",
            "ACC_T_MOVE     (Move)",
            "ACC_T_LDR0     (Load whose address is not from preceding Binary)",
            "ACC_T_LDR2     (Binary addr; Load using that address)",
            "ACC_T_STR0     (Store whose address is not from preceding Binary)",
            "ACC_T_STR2     (Binary addr; Store using that address)",
            "ACC_T_PAR_CALL ((Par)^n; Call, 0 <= n <= 8)",
            "ACC_T_NOP      (Nop)",
        };
        d.transitions = {
            "S0       --L---> ACC_T_LABEL",
            "S0       --G---> ACC_T_GOTO",
            "S0       --R---> ACC_T_RETURN",
            "S0       --I---> S_IF",
            "S_IF     --L (label name in {If.l1, If.l2})--> ACC_T_IF_LABEL",
            "S_IF     --other--> ACC_T_IF",
            "S0       --M---> ACC_T_MOVE",
            "S0       --A---> ACC_T_AOP",
            "S0       --D---> S_ADDR",
            "S_ADDR   --LD (Load.addr == Binary.dst)--> ACC_T_LDR2",
            "S_ADDR   --ST (Store.addr == Binary.dst)--> ACC_T_STR2",
            "S_ADDR   --other--> ACC_T_AOP",
            "S0       --LD--> ACC_T_LDR0",
            "S0       --ST--> ACC_T_STR0",
            "S0       --P---> S_PAR_1",
            "S_PAR_n  --P (n<8)--> S_PAR_(n+1)",
            "S_PAR_n  --C---> ACC_T_PAR_CALL",
            "S0       --N---> ACC_T_NOP",
        };
        return d;
    }();
    return dfa;
}

std::string SigmaDfa::to_text() const {
    std::ostringstream out;
    out << "QCompiler ARM backend sigma-DFA (Q2ARM template recognizer)\n";
    out << "alphabet size: " << alphabet.size() << '\n';
    out << "states (excluding accept): " << states.size() << '\n';
    out << "accept states: " << accept_states.size() << '\n';
    out << "transitions: " << transitions.size() << '\n';

    out << "\nalphabet (each IR instruction is mapped to one symbol):\n";
    for (const auto &sym : alphabet) {
        out << "  " << sym << '\n';
    }
    out << "\nstates:\n";
    for (const auto &s : states) {
        out << "  " << s << '\n';
    }
    out << "\naccept states -> Q2ARM templates:\n";
    for (const auto &a : accept_states) {
        out << "  " << a << '\n';
    }
    out << "\ntransitions:\n";
    for (const auto &t : transitions) {
        out << "  " << t << '\n';
    }
    return out.str();
}

} // namespace qcompiler
