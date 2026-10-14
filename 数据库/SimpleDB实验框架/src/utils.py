import json


def print_ast(ast, title="AST Structure"):
    """
    格式化打印 AST (抽象语法树) 结构。
    使用 JSON 缩进格式，方便调试和观察 SQL 解析结果。
    """
    print(f"\n=== {title} ===")
    # ensure_ascii=False 允许正确显示中文
    # default=str 处理 AST 中可能包含的非标准 JSON 对象 (如自定义类实例)
    print(json.dumps(ast, indent=4, default=str, ensure_ascii=False))
    print("=" * (len(title) + 8))


def print_plan(operator, indent=0, prefix=""):
    """
    递归打印查询执行计划树 (Execution Plan Tree)。

    依赖算子类必须实现以下接口:
    1. get_info() -> dict: 返回算子内部参数 (如过滤条件、表名等)。
    2. get_children() -> list[Operator]: 返回子算子列表。
    """
    if not operator:
        return

    # 1. 准备当前节点的打印信息
    space = "  " * indent
    op_type = operator.__class__.__name__

    # 获取算子内部详情
    info_dict = {}
    if hasattr(operator, "get_info"):
        info_dict = operator.get_info()

    # 格式化参数显示: [Key: Val, ...]
    info_str = ""
    if info_dict:
        items = [f"{k}: {v}" for k, v in info_dict.items()]
        info_str = f"[{', '.join(items)}]"

    # 打印当前行: 缩进 + 前缀 + 类型 + 参数
    print(f"{space}{prefix}{op_type} {info_str}")

    # 2. 递归处理子节点
    if hasattr(operator, "get_children"):
        children = operator.get_children()
        count = len(children)

        for i, child in enumerate(children):
            # 生成树状结构的视觉前缀
            # 单一子节点用 "|- ", Join 的左右子节点用 "L- " 和 "R- " 区分
            if count == 1:
                new_prefix = "|- "
            elif i == 0:
                new_prefix = "L- "
            else:
                new_prefix = "R- "

            print_plan(child, indent + 2, new_prefix)

    # [Compatibility] 兼容未实现 get_children 接口的旧式算子
    elif hasattr(operator, "child"):
        print_plan(operator.child, indent + 2, "|- ")
