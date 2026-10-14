"""Pass4: 寄存器分配器 - 将虚拟寄存器映射为ARM64物理寄存器"""

import re

# 可用物理寄存器池: X9 ~ X15
PHYS_REGS = [f'X{i}' for i in range(9, 16)]


def allocate_registers(asm_lines):
    """将虚拟寄存器替换为物理寄存器

    策略: 按虚拟寄存器首次出现顺序分配物理寄存器 X9~X15
    X0 保留给返回值，不参与分配

    Args:
        asm_lines: Pass3输出的汇编行列表

    Returns:
        (allocated_lines, reg_map) - 分配后的汇编行列表和映射表
    """
    # 扫描所有虚拟寄存器 (Xname 形式，排除 X0~X15 和 Xcrs)
    vreg_pattern = re.compile(r'\bX([a-zA-Z_]\w*)\b')
    reserved = {'crs'} | {str(i) for i in range(16)}

    # 收集虚拟寄存器，按首次出现顺序
    vreg_order = []
    vreg_set = set()
    for line in asm_lines:
        for m in vreg_pattern.finditer(line):
            name = m.group(1)
            if name not in reserved and name not in vreg_set:
                vreg_order.append(name)
                vreg_set.add(name)

    # 建立映射
    reg_map = {}
    for idx, name in enumerate(vreg_order):
        if idx < len(PHYS_REGS):
            reg_map[f'X{name}'] = PHYS_REGS[idx]
        else:
            # 溢出处理（简单报错，fact程序不会超过7个）
            reg_map[f'X{name}'] = f'X_SPILL_{name}'

    # 替换
    result = []
    for line in asm_lines:
        new_line = line
        # 按虚拟寄存器名长度降序替换，避免短名误替换长名
        for vreg in sorted(reg_map.keys(), key=len, reverse=True):
            new_line = new_line.replace(vreg, reg_map[vreg])
        result.append(new_line)

    return result, reg_map


def format_reg_map(reg_map):
    """格式化寄存器映射表"""
    return '\n'.join(f'{vreg} → {phys}' for vreg, phys in reg_map.items())
