"""Pass2: σ-DFA 模板匹配器 - 对记号流进行模板匹配"""

from src.dfa_table import *


def _tok_type(token):
    """获取记号的DFA输入符号类型"""
    t, v = token
    if t == 'KEY':
        return f'KEY:{v}'
    return t


def _is_var_or_temp(token):
    """判断是否为变量或临时变量(即q类型)"""
    return token[0] in ('d', 'temp')


def _is_operand(token):
    """判断是否为操作数(q或k)"""
    return token[0] in ('d', 'temp', 'k')


def _is_addr(token):
    """判断是否为地址(q或l)"""
    return token[0] in ('d', 'temp', 'l')


def match_templates(tokens):
    """对记号流进行σ-DFA模板匹配

    Args:
        tokens: Pass1输出的记号流 [(type, value), ...]

    Returns:
        list of (template_name, info_dict) tuples
    """
    results = []
    i = 0
    par_args = []  # 收集PAR参数

    while i < len(tokens):
        tok = tokens[i]
        sym = _tok_type(tok)

        # ---- LABEL l ; ----
        if sym == 'KEY:LABEL':
            if i + 2 < len(tokens) and tokens[i+1][0] == 'l' and tokens[i+2] == (';', '_'):
                label_tok = tokens[i+1]
                results.append(('T_LABEL', {'l': label_tok}))
                i += 3
                continue

        # ---- GOTO b ; ----
        if sym == 'KEY:GOTO':
            if i + 2 < len(tokens) and _is_addr(tokens[i+1]) and tokens[i+2] == (';', '_'):
                results.append(('T_GOTO', {'b': tokens[i+1]}))
                i += 3
                continue

        # ---- RETURN a ; ----
        if sym == 'KEY:RETURN':
            if i + 2 < len(tokens) and _is_operand(tokens[i+1]) and tokens[i+2] == (';', '_'):
                a_tok = tokens[i+1]
                a_kind = 'q' if _is_var_or_temp(a_tok) else 'k'
                results.append(('T_RETURN', {'a': a_tok, 'a_kind': a_kind}))
                i += 3
                continue

        # ---- IF q rop a THEN l1 ELSE l2 ; ----
        if sym == 'KEY:IF':
            if (i + 8 < len(tokens)
                and _is_var_or_temp(tokens[i+1])
                and tokens[i+2][0] == 'rop'
                and _is_operand(tokens[i+3])
                and _tok_type(tokens[i+4]) == 'KEY:THEN'
                and tokens[i+5][0] == 'l'
                and _tok_type(tokens[i+6]) == 'KEY:ELSE'
                and tokens[i+7][0] == 'l'
                and tokens[i+8] == (';', '_')):
                q_tok = tokens[i+1]
                rop_tok = tokens[i+2]
                a_tok = tokens[i+3]
                l1_tok = tokens[i+5]
                l2_tok = tokens[i+7]
                # 向前看: 是否紧跟 LABEL l3 ; 且 l3 == l1 或 l3 == l2
                if (i + 11 < len(tokens)
                    and _tok_type(tokens[i+9]) == 'KEY:LABEL'
                    and tokens[i+10][0] == 'l'
                    and tokens[i+11] == (';', '_')):
                    l3_tok = tokens[i+10]
                    if l3_tok[1] == l1_tok[1] or l3_tok[1] == l2_tok[1]:
                        which = 'l1' if l3_tok[1] == l1_tok[1] else 'l2'
                        results.append(('T_IF_LABEL', {
                            'q': q_tok, 'rop': rop_tok, 'a': a_tok,
                            'l1': l1_tok, 'l2': l2_tok, 'l3': l3_tok,
                            'l3_eq': which,
                        }))
                        i += 12
                        continue
                # 普通 T_IF
                results.append(('T_IF', {
                    'q': q_tok, 'rop': rop_tok, 'a': a_tok,
                    'l1': l1_tok, 'l2': l2_tok,
                }))
                i += 9
                continue

        # ---- PAR a ; ----
        if sym == 'KEY:PAR':
            if i + 2 < len(tokens) and _is_operand(tokens[i+1]) and tokens[i+2] == (';', '_'):
                par_args.append(tokens[i+1])
                i += 3
                continue

        # ---- M[b] = q ; (store) ----
        if sym == 'KEY:M':
            if (i + 6 < len(tokens)
                and tokens[i+1] == ('[', '_')
                and _is_addr(tokens[i+2])
                and tokens[i+3] == (']', '_')
                and tokens[i+4] == ('=', '_')
                and _is_var_or_temp(tokens[i+5])
                and tokens[i+6] == (';', '_')):
                results.append(('T_STR0', {'b': tokens[i+2], 'q': tokens[i+5]}))
                i += 7
                continue

        # ---- q = ... patterns (assignment, arithmetic, CALL, memory load) ----
        if _is_var_or_temp(tok):
            if i + 1 < len(tokens) and tokens[i+1] == ('=', '_'):
                # q = CALL d , k ;
                if (i + 6 < len(tokens)
                    and _tok_type(tokens[i+2]) == 'KEY:CALL'
                    and tokens[i+3][0] == 'd'
                    and tokens[i+4] == (',', '_')
                    and tokens[i+5][0] == 'k'
                    and tokens[i+6] == (';', '_')):
                    results.append(('T_PAR_CALL', {
                        'q': tok, 'func': tokens[i+3], 'argc': tokens[i+5],
                        'pars': list(par_args),
                    }))
                    par_args = []
                    i += 7
                    continue

                # q = M[b] ;  (memory load)
                if (i + 6 < len(tokens)
                    and _tok_type(tokens[i+2]) == 'KEY:M'
                    and tokens[i+3] == ('[', '_')
                    and _is_addr(tokens[i+4])
                    and tokens[i+5] == (']', '_')
                    and tokens[i+6] == (';', '_')):
                    results.append(('T_LDR0', {'q': tok, 'b': tokens[i+4]}))
                    i += 7
                    continue

                # q = q2 aop a ;  (arithmetic) or q = a ;  (move)
                if i + 2 < len(tokens) and _is_operand(tokens[i+2]):
                    # Check for arithmetic: q = q2 aop a ;
                    if (i + 4 < len(tokens)
                        and tokens[i+3][0] == 'aop'
                        and _is_operand(tokens[i+4])
                        and i + 5 < len(tokens)
                        and tokens[i+5] == (';', '_')):
                        a_tok = tokens[i+4]
                        a_kind = 'q' if _is_var_or_temp(a_tok) else 'k'
                        results.append(('T_AOP', {
                            'q1': tok, 'q2': tokens[i+2],
                            'aop': tokens[i+3], 'a': a_tok,
                            'a_kind': a_kind,
                        }))
                        i += 6
                        continue

                    # q = a ;  (move)
                    if i + 3 < len(tokens) and tokens[i+3] == (';', '_'):
                        a_tok = tokens[i+2]
                        a_kind = 'q' if _is_var_or_temp(a_tok) else 'k'
                        results.append(('T_MOVE', {
                            'q': tok, 'a': a_tok, 'a_kind': a_kind,
                        }))
                        i += 4
                        continue

        # Fallback: skip unrecognized token
        i += 1

    return results


def format_template(name, info):
    """格式化单个模板匹配结果"""
    if name == 'T_LABEL':
        return f"(T_LABEL, ({info['l'][0]},{info['l'][1]}))"
    elif name == 'T_GOTO':
        b = info['b']
        return f"(T_GOTO, ({b[0]},{b[1]}))"
    elif name == 'T_RETURN':
        a = info['a']
        return f"(T_RETURN, a is {info['a_kind']}, ({a[0]},{a[1]}))"
    elif name == 'T_IF':
        q, rop, a = info['q'], info['rop'], info['a']
        l1, l2 = info['l1'], info['l2']
        return (f"(T_IF, ({q[0]},{q[1]}), ({rop[0]},{rop[1]}), "
                f"({a[0]},{a[1]}), ({l1[0]},{l1[1]}), ({l2[0]},{l2[1]}))")
    elif name == 'T_IF_LABEL':
        q, rop, a = info['q'], info['rop'], info['a']
        l1, l2 = info['l1'], info['l2']
        eq = info['l3_eq']
        return (f"(T_IF_LABEL, {eq}=l3, ({q[0]},{q[1]}), ({rop[0]},{rop[1]}), "
                f"({a[0]},{a[1]}), ({l1[0]},{l1[1]}), ({l2[0]},{l2[1]}))")
    elif name == 'T_AOP':
        q1, q2, aop, a = info['q1'], info['q2'], info['aop'], info['a']
        return (f"(T_AOP, a is {info['a_kind']}, ({q1[0]},{q1[1]}), "
                f"({q2[0]},{q2[1]}), ({aop[0]},{aop[1]}), ({a[0]},{a[1]}))")
    elif name == 'T_MOVE':
        q, a = info['q'], info['a']
        return f"(T_MOVE, a is {info['a_kind']}, ({q[0]},{q[1]}), ({a[0]},{a[1]}))"
    elif name == 'T_LDR0':
        q, b = info['q'], info['b']
        return f"(T_LDR0, ({q[0]},{q[1]}), ({b[0]},{b[1]}))"
    elif name == 'T_STR0':
        b, q = info['b'], info['q']
        return f"(T_STR0, ({b[0]},{b[1]}), ({q[0]},{q[1]}))"
    elif name == 'T_PAR_CALL':
        q, func, argc = info['q'], info['func'], info['argc']
        pars = info['pars']
        par_str = ', '.join(f'({p[0]},{p[1]})' for p in pars)
        return (f"(T_PAR_CALL, ({q[0]},{q[1]}), ({func[0]},{func[1]}), "
                f"({argc[0]},{argc[1]}), pars=[{par_str}])")
    return f"({name}, {info})"


def format_templates(templates):
    """格式化模板匹配结果列表"""
    return '\n'.join(format_template(name, info) for name, info in templates)
