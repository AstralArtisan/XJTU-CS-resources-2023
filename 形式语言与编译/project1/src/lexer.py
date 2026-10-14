"""Pass1: QTAC 词法分析器 - 将QTAC源代码解析为记号流"""

import re

# 记号类型
KEYWORDS = {'LABEL', 'GOTO', 'IF', 'THEN', 'ELSE', 'PAR', 'CALL', 'RETURN', 'M'}
ROP_SET = {'<', '<=', '>', '>=', '==', '!='}
AOP_SET = {'+', '-', '*', '/'}

# 记号模式（按优先级排列）
TOKEN_PATTERNS = [
    ('WHITESPACE', r'\s+'),
    ('ROP', r'<=|>=|==|!=|<|>'),
    ('AOP', r'[+\-*/]'),
    ('ASSIGN', r'='),
    ('SEMI', r';'),
    ('LBRACKET', r'\['),
    ('RBRACKET', r'\]'),
    ('COMMA', r','),
    ('NUMBER', r'[0-9]+'),
    ('IDENT', r'[a-zA-Z_][a-zA-Z0-9_]*'),
]

MASTER_PATTERN = '|'.join(f'(?P<{name}>{pattern})' for name, pattern in TOKEN_PATTERNS)
TOKEN_RE = re.compile(MASTER_PATTERN)


def classify_ident(word):
    """分类标识符：关键字、标号、临时变量、普通变量"""
    if word in KEYWORDS:
        return ('KEY', word)
    # 标号: l开头+数字 (l0, l1, ...) 或 L_数字
    if re.match(r'^l\d+$', word) or re.match(r'^L_\d+$', word):
        return ('l', word)
    # 临时变量: t开头+数字 (t1, t2, ...) 或 T_数字
    if re.match(r'^t\d+$', word) or re.match(r'^T_\d+$', word):
        return ('temp', word)
    # 普通变量名
    return ('d', word)


def tokenize(source):
    """将QTAC源代码文本解析为记号流

    Args:
        source: QTAC源代码字符串

    Returns:
        list of (type, value) tuples
    """
    tokens = []
    for m in TOKEN_RE.finditer(source):
        kind = m.lastgroup
        value = m.group()
        if kind == 'WHITESPACE':
            continue
        elif kind == 'NUMBER':
            tokens.append(('k', value))
        elif kind == 'ROP':
            tokens.append(('rop', value))
        elif kind == 'AOP':
            tokens.append(('aop', value))
        elif kind == 'ASSIGN':
            tokens.append(('=', '_'))
        elif kind == 'SEMI':
            tokens.append((';', '_'))
        elif kind == 'LBRACKET':
            tokens.append(('[', '_'))
        elif kind == 'RBRACKET':
            tokens.append((']', '_'))
        elif kind == 'COMMA':
            tokens.append((',', '_'))
        elif kind == 'IDENT':
            tokens.append(classify_ident(value))
    return tokens


def format_tokens(tokens):
    """格式化记号流为可读字符串"""
    return ' '.join(f'({t},{v})' for t, v in tokens)
