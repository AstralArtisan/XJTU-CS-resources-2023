"""Pass3: ARM64 代码生成器 - 将模板记号流转换为ARM64汇编(虚拟寄存器)"""

# 关系运算符到ARM64条件码的映射
ROP_TO_JROP = {
    '<': 'B.LT', '<=': 'B.LE', '>': 'B.GT',
    '>=': 'B.GE', '==': 'B.EQ', '!=': 'B.NE',
}

# 取反映射
ROP_TO_JROP_NEG = {
    '<': 'B.GE', '<=': 'B.GT', '>': 'B.LE',
    '>=': 'B.LT', '==': 'B.NE', '!=': 'B.EQ',
}

# 算术运算符到ARM64指令的映射 (寄存器版本)
AOP_TO_XAOP = {'+': 'ADD', '-': 'SUB', '*': 'MUL', '/': 'SDIV'}

# 算术运算符到ARM64指令的映射 (立即数版本)
AOP_TO_KAOP = {'+': 'ADD', '-': 'SUB'}


def _vreg(token):
    """将变量/临时变量记号转换为虚拟寄存器名"""
    return f'X{token[1]}'


def generate_arm(templates):
    """将模板记号流转换为ARM64汇编指令(虚拟寄存器)

    Args:
        templates: Pass2输出的模板列表 [(name, info), ...]

    Returns:
        list of ARM64 assembly lines
    """
    lines = []

    for name, info in templates:
        if name == 'T_LABEL':
            lines.append(f"{info['l'][1]}:")

        elif name == 'T_GOTO':
            b = info['b']
            if b[0] == 'l':
                lines.append(f"B {b[1]};")
            else:
                lines.append(f"BR {_vreg(b)};")

        elif name == 'T_RETURN':
            a = info['a']
            if info['a_kind'] == 'q':
                lines.append(f"MOV X0, {_vreg(a)}; RET;")
            else:
                lines.append(f"MOV X0, #{a[1]}; RET;")

        elif name == 'T_IF':
            q, rop_val, a = info['q'], info['rop'][1], info['a']
            l1, l2 = info['l1'][1], info['l2'][1]
            if a[0] == 'k':
                cmp_operand = f"#{a[1]}"
            else:
                cmp_operand = _vreg(a)
            jrop = ROP_TO_JROP[rop_val]
            lines.append(f"CMP {_vreg(q)}, {cmp_operand}; {jrop} {l1}; B {l2};")

        elif name == 'T_IF_LABEL':
            q, rop_val, a = info['q'], info['rop'][1], info['a']
            l1, l2 = info['l1'][1], info['l2'][1]
            l3 = info['l3'][1]
            if a[0] == 'k':
                cmp_operand = f"#{a[1]}"
            else:
                cmp_operand = _vreg(a)
            if info['l3_eq'] == 'l2':
                # l3 == l2: CMP Xq, Xa; Jrop l1; l2:
                jrop = ROP_TO_JROP[rop_val]
                lines.append(f"CMP {_vreg(q)}, {cmp_operand}; {jrop} {l1}; {l2}:")
            else:
                # l3 == l1: CMP Xq, Xa; Jrop' l2; l1:
                jrop_neg = ROP_TO_JROP_NEG[rop_val]
                lines.append(f"CMP {_vreg(q)}, {cmp_operand}; {jrop_neg} {l2}; {l1}:")

        elif name == 'T_AOP':
            q1, q2, aop_val, a = info['q1'], info['q2'], info['aop'][1], info['a']
            if info['a_kind'] == 'k' and aop_val in AOP_TO_KAOP:
                instr = AOP_TO_KAOP[aop_val]
                lines.append(f"{instr} {_vreg(q1)}, {_vreg(q2)}, #{a[1]};")
            else:
                instr = AOP_TO_XAOP[aop_val]
                if info['a_kind'] == 'q':
                    lines.append(f"{instr} {_vreg(q1)}, {_vreg(q2)}, {_vreg(a)};")
                else:
                    # MUL/SDIV don't support immediates, but handle gracefully
                    lines.append(f"{instr} {_vreg(q1)}, {_vreg(q2)}, {_vreg(a)};")

        elif name == 'T_MOVE':
            q, a = info['q'], info['a']
            if info['a_kind'] == 'q':
                lines.append(f"MOV {_vreg(q)}, {_vreg(a)};")
            else:
                lines.append(f"MOV {_vreg(q)}, #{a[1]};")

        elif name == 'T_LDR0':
            q, b = info['q'], info['b']
            if b[0] in ('d', 'temp'):
                lines.append(f"LDR {_vreg(q)}, [{_vreg(b)}];")
            else:
                lines.append(f"LDR {_vreg(q)}, {b[1]};")

        elif name == 'T_STR0':
            b, q = info['b'], info['q']
            if b[0] in ('d', 'temp'):
                lines.append(f"STR {_vreg(q)}, [{_vreg(b)}];")
            else:
                lines.append(f"ADR Xcrs, {b[1]}; STR {_vreg(q)}, [Xcrs];")

        elif name == 'T_PAR_CALL':
            q = info['q']
            func = info['func']
            pars = info['pars']
            # 参数传递: PAR args -> X0..X7
            for idx, p in enumerate(pars):
                if p[0] == 'k':
                    lines.append(f"MOV X{idx}, #{p[1]};")
                else:
                    lines.append(f"MOV X{idx}, {_vreg(p)};")
            lines.append(f"BL {func[1]};")
            lines.append(f"MOV {_vreg(q)}, X0;")

    return lines


def format_asm(lines):
    """格式化ARM64汇编输出"""
    return '\n'.join(lines)
