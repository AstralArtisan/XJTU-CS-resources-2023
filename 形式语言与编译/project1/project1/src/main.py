"""QTAC-to-ARM64 代码生成器 - 主程序入口

用法:
    python -m src.main < test/fact.qtac > output/fact_asm.s
    python -m src.main --debug < test/fact.qtac
    python -m src.main test/fact.qtac -o output/fact_asm.s
"""

import sys
import os
import argparse

# 确保项目根目录在路径中
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from src.lexer import tokenize, format_tokens
from src.matcher import match_templates, format_templates
from src.codegen import generate_arm, format_asm
from src.regalloc import allocate_registers, format_reg_map


def main():
    parser = argparse.ArgumentParser(description='QTAC-to-ARM64 Code Generator')
    parser.add_argument('input', nargs='?', help='Input QTAC file (default: stdin)')
    parser.add_argument('-o', '--output', help='Output file (default: stdout)')
    parser.add_argument('--debug', action='store_true', help='Print intermediate results')
    args = parser.parse_args()

    # 读取输入
    if args.input:
        with open(args.input, 'r', encoding='utf-8') as f:
            source = f.read()
    else:
        source = sys.stdin.read()

    # Pass 1: 词法分析
    tokens = tokenize(source)
    if args.debug:
        print("=" * 60)
        print("Pass 1: 词法分析 (Lexical Analysis)")
        print("=" * 60)
        print(format_tokens(tokens))
        print()

    # Pass 2: σ-DFA 模板匹配
    templates = match_templates(tokens)
    if args.debug:
        print("=" * 60)
        print("Pass 2: σ-DFA 模板匹配 (Template Matching)")
        print("=" * 60)
        print(format_templates(templates))
        print()

    # Pass 3: ARM 代码生成 (虚拟寄存器)
    asm_lines = generate_arm(templates)
    if args.debug:
        print("=" * 60)
        print("Pass 3: ARM代码生成 - 虚拟寄存器 (Virtual Registers)")
        print("=" * 60)
        print(format_asm(asm_lines))
        print()

    # Pass 4: 寄存器分配
    final_lines, reg_map = allocate_registers(asm_lines)
    if args.debug:
        print("=" * 60)
        print("Pass 4: 寄存器分配 (Register Allocation)")
        print("=" * 60)
        print("映射表:")
        print(format_reg_map(reg_map))
        print()
        print("最终输出:")

    output = format_asm(final_lines)

    # 输出结果
    if args.output:
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(output + '\n')
        if args.debug:
            print(output)
        print(f"Output written to {args.output}", file=sys.stderr)
    else:
        print(output)


if __name__ == '__main__':
    main()
