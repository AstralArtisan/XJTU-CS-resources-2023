"""自动化测试 - 验证代码生成器各pass的输出"""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from src.lexer import tokenize, format_tokens
from src.matcher import match_templates, format_templates
from src.codegen import generate_arm, format_asm
from src.regalloc import allocate_registers

FACT_QTAC = "LABEL l0; t1=2; IF n<t1 THEN l1 ELSE l2; LABEL l1; RETURN a; GOTO l3; LABEL l2; t2=n-1; t3=n*a; n=t2; a=t3; GOTO l0; LABEL l3;"

def test_pass1():
    """测试Pass1词法分析"""
    tokens = tokenize(FACT_QTAC)
    assert tokens[0] == ('KEY', 'LABEL')
    assert tokens[1] == ('l', 'l0')
    assert tokens[2] == (';', '_')
    assert tokens[3] == ('temp', 't1')
    assert tokens[4] == ('=', '_')
    assert tokens[5] == ('k', '2')
    assert ('rop', '<') in tokens
    assert ('aop', '-') in tokens
    assert ('aop', '*') in tokens
    print("Pass1 OK")

def test_pass2():
    """测试Pass2模板匹配"""
    tokens = tokenize(FACT_QTAC)
    templates = match_templates(tokens)
    names = [t[0] for t in templates]
    assert names == [
        'T_LABEL', 'T_MOVE', 'T_IF_LABEL', 'T_RETURN', 'T_GOTO',
        'T_LABEL', 'T_AOP', 'T_AOP', 'T_MOVE', 'T_MOVE',
        'T_GOTO', 'T_LABEL',
    ], f"Got: {names}"
    # 验证T_IF_LABEL合并
    if_label = templates[2]
    assert if_label[0] == 'T_IF_LABEL'
    assert if_label[1]['l3_eq'] == 'l1'
    print("Pass2 OK")

def test_pass3():
    """测试Pass3 ARM代码生成(虚拟寄存器)"""
    tokens = tokenize(FACT_QTAC)
    templates = match_templates(tokens)
    asm = generate_arm(templates)
    expected = [
        "l0:",
        "MOV Xt1, #2;",
        "CMP Xn, Xt1; B.GE l2; l1:",
        "MOV X0, Xa; RET;",
        "B l3;",
        "l2:",
        "SUB Xt2, Xn, #1;",
        "MUL Xt3, Xn, Xa;",
        "MOV Xn, Xt2;",
        "MOV Xa, Xt3;",
        "B l0;",
        "l3:",
    ]
    assert asm == expected, f"Mismatch:\nGot:      {asm}\nExpected: {expected}"
    print("Pass3 OK")

def test_pass4():
    """测试Pass4寄存器分配"""
    tokens = tokenize(FACT_QTAC)
    templates = match_templates(tokens)
    asm = generate_arm(templates)
    final, reg_map = allocate_registers(asm)
    expected = [
        "l0:",
        "MOV X9, #2;",
        "CMP X10, X9; B.GE l2; l1:",
        "MOV X0, X11; RET;",
        "B l3;",
        "l2:",
        "SUB X12, X10, #1;",
        "MUL X13, X10, X11;",
        "MOV X10, X12;",
        "MOV X11, X13;",
        "B l0;",
        "l3:",
    ]
    assert final == expected, f"Mismatch:\nGot:      {final}\nExpected: {expected}"
    assert reg_map['Xt1'] == 'X9'
    assert reg_map['Xn'] == 'X10'
    assert reg_map['Xa'] == 'X11'
    assert reg_map['Xt2'] == 'X12'
    assert reg_map['Xt3'] == 'X13'
    print("Pass4 OK")

if __name__ == '__main__':
    test_pass1()
    test_pass2()
    test_pass3()
    test_pass4()
    print("\nAll tests passed!")
