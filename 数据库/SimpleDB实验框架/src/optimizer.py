from collections.abc import Callable
from typing import Any

from .catalog import Catalog
from .operators import (
    CsvScan,
    FilterOperator,
    IndexNestedLoopJoinOperator,
    NestedLoopJoinOperator,
    Operator,
    ProjectOperator,
)
from .storage import MISSING_COLUMN, Tuple


class WhereConverter:
    """
    辅助类：将 WHERE 子句的 AST (字典结构) 转换为可执行的 Python 函数。
    """

    @staticmethod
    def build_predicate(where_clause: dict) -> Callable[[Tuple], bool]:
        """
        构建谓词函数。
        返回的函数接收一个 Tuple，返回 True (保留) 或 False (丢弃)。
        """
        if not where_clause:
            return lambda _: True

        def predicate(t: Tuple) -> bool:
            return WhereConverter._evaluate(where_clause, t)

        return predicate

    @staticmethod
    def format_condition(node: Any) -> str:
        """
        将 AST 节点格式化为易读的字符串 (用于执行计划打印)。
        例如: {'eq': ['age', 20]} -> "(age = 20)"
        """
        if isinstance(node, dict):
            op, args = list(node.items())[0]
            arg_strs = [WhereConverter.format_condition(arg) for arg in args]
            op_map = {"eq": "=", "neq": "!=", "gt": ">", "lt": "<", "gte": ">=", "lte": "<=", "and": "AND", "or": "OR"}
            symbol = op_map.get(op.lower(), op.upper())
            if len(arg_strs) == 2:
                return f"({arg_strs[0]} {symbol} {arg_strs[1]})"
            else:
                return f" {symbol} ".join([f"({s})" for s in arg_strs])
        if isinstance(node, str):
            return node
        return str(node)

    @staticmethod
    def _evaluate(node: Any, t: Tuple) -> Any:
        if isinstance(node, dict):
            op, args = list(node.items())[0]
            if isinstance(args, list):
                eval_args = [WhereConverter._evaluate(arg, t) for arg in args]
            else:
                eval_args = [WhereConverter._evaluate(args, t)]
            return WhereConverter._apply_op(op, eval_args)
        if isinstance(node, str):
            val = t.get(node)
            if val is not MISSING_COLUMN:
                return val
            return node
        return node

    @staticmethod
    def _apply_op(op: str, args: list) -> bool:
        op = op.lower()
        left = args[0]
        right = args[1] if len(args) > 1 else None

        if op == "eq":
            return left == right
        if op == "neq":
            return left != right
        if op == "gt":
            return left > right
        if op == "lt":
            return left < right
        if op == "gte":
            return left >= right
        if op == "lte":
            return left <= right
        if op == "and":
            return all(args)
        if op == "or":
            return any(args)
        return False


class QueryOptimizer:
    """
    查询优化器 (Query Optimizer)
    职责：基于规则 (RBO) 和代价 (CBO) 将 AST 转换为最优的物理执行计划树。
    """

    def __init__(self, catalog: Catalog):
        self.catalog = catalog
        self.enable_optimization = True

    def plan_query(self, ast: dict) -> Operator | None:
        """
        [入口方法] 生成物理执行计划。

        流程:
        1. 语义分析: 解析表名与别名。
        2. 逻辑优化: 谓词下推 (Predicate Pushdown)。
        3. 物理优化:
           - 扫描方式选择 (IndexScan vs SeqScan)。
           - 连接顺序交换 (Join Reordering)。
           - 连接算法选择 (IndexJoin vs NestedLoopJoin)。
        """
        if not ast:
            return None

        # 1. 准备元数据
        from_clause = ast.get("from")
        table_refs = from_clause if isinstance(from_clause, list) else [from_clause]

        aliases = {}
        for ref in table_refs:
            r, a = self.catalog.resolve_table_info(ref)
            aliases[a] = r

        # 2. 谓词拆分 (逻辑优化)
        where_clause = ast.get("where")
        tbl_preds, join_preds = self._distribute_predicates(where_clause, aliases)

        # 3. 构建扫描层 (Selection Pushdown)
        leaf_ops = []
        for ref in table_refs:
            _, alias = self.catalog.resolve_table_info(ref)
            preds = tbl_preds.get(alias, [])
            op = self._create_optimized_leaf(ref, preds)
            leaf_ops.append(op)

        # 4. 构建连接树 (Join Tree Construction)
        if not leaf_ops:
            return None

        root = leaf_ops[0]
        remaining_join_preds = list(join_preds)

        for i in range(1, len(leaf_ops)):
            right_op = leaf_ops[i]
            # 核心优化：连接顺序与算法选择
            root = self._create_optimized_join(root, right_op, join_preds)

        # 5. 最终清理 (Residual Filter)
        # 处理那些无法下推到 Join 内部的复杂条件
        if remaining_join_preds:
            final_where = {"and": remaining_join_preds} if len(remaining_join_preds) > 1 else remaining_join_preds[0]
            predicate_func = WhereConverter.build_predicate(final_where)
            predicate_str = WhereConverter.format_condition(final_where)
            root = FilterOperator(root, predicate_func, predicate_str)

        # 6. 投影 (Project)
        root = self._apply_projection(root, ast.get("select"))

        return root

    def _create_optimized_leaf(self, table_ref, predicates) -> Operator:
        """
        构建叶子节点。

        优化策略 (Selection Pushdown):
        尝试利用索引 (IndexScan) 替代全表扫描 (CsvScan)。
        如果命中索引，相关的过滤条件将被索引访问替代，不再需要 Filter 算子。
        """
        real_name, alias = self.catalog.resolve_table_info(table_ref)

        chosen_op = None
        used_pred_index = -1  # 标记被索引使用的条件索引

        # ==================================
        # TODO: 步骤 1 - 尝试索引匹配
        # ==================================
        # 任务提示:
        # 1. 遍历 predicates 列表。
        # 2. 调用 self._find_index_match(real_name, alias, pred) 检查是否命中索引。
        # 3. 如果命中 (index_inst 不为 None):
        #    - 创建 IndexScanOperator。
        #    - 记录 used_pred_index (避免该条件被重复执行)。
        #    - 打印优化日志 (可选)。
        #    - break (通常只利用一个索引)。
        if self.enable_optimization:
            # [在此处填入代码]
            pass

        # ==================================
        # 步骤 2 - 回退逻辑 (Fallback)
        # ==================================
        # 如果未命中索引，回退到全表扫描 (CsvScan)。

        if chosen_op is None:
            file_path = self.catalog.get_table_path(real_name)
            chosen_op = CsvScan(file_path, real_name, alias)

        # ==================================
        # 步骤 3 - 剩余条件过滤 (Residual Predicates)
        # ==================================
        # 如果有条件未被索引覆盖，需要在 Scan 之上添加 Filter 算子。

        if used_pred_index == -1:
            remaining_preds = predicates
        else:
            # ==================================
            # TODO: 收集剩余条件
            # ==================================
            # 过滤掉已经作为 Index Key 使用的条件。
            pass

        # 构建 Filter 算子
        if remaining_preds:
            final_where = {"and": remaining_preds} if len(remaining_preds) > 1 else remaining_preds[0]
            predicate_func = WhereConverter.build_predicate(final_where)
            predicate_str = WhereConverter.format_condition(final_where)
            chosen_op = FilterOperator(chosen_op, predicate_func, predicate_str)

        return chosen_op

    def _create_optimized_join(self, op1: Operator, op2: Operator, join_preds: list) -> Operator:
        """
        构建连接算子。

        优化策略:
        1. Index Nested Loop Join (INLJ): 利用右表的索引加速查找。
        2. Join Reordering: 始终保持"小表驱动大表" (Left Deep Tree)。
        """

        # 0. 无连接条件 -> 笛卡尔积
        if not join_preds:
            return NestedLoopJoinOperator(op1, op2)

        if self.enable_optimization:
            # ==================================
            # TODO: 步骤 1 - 安全性检查
            # ==================================
            # 并非所有算子都能作为 INLJ 的内表 (右表)。
            # 例如，Filter(Scan) 如果没有物化，通常无法直接利用原始索引。
            # 在此简化实现中，只有纯 CsvScan 才能安全地作为被驱动表。

            can_op2_be_inner = False  # [请修改此处代码]
            can_op1_be_inner = False  # [请修改此处代码]

            # ==================================
            # TODO: 步骤 2 - 尝试策略 A (正常顺序 INLJ)
            # ==================================
            # 检查: op2 是否有索引且 join_preds 中包含对应的等值条件?
            # 1. 调用 self._try_match_inlj(outer=op1, inner=op2, ...)
            # 2. 如果成功返回算子，直接使用。

            if can_op2_be_inner:
                # [在此处填入代码]
                pass

            # ==================================
            # TODO: 步骤 3 - 尝试策略 B (交换顺序 INLJ)
            # ==================================
            # 检查: op1 是否有索引?
            # 1. 尝试交换左右顺序: outer=op2, inner=op1
            # 2. 如果成功，返回交换后的 INLJ 算子。

            if can_op1_be_inner:
                # [在此处填入代码]
                pass

            # ==================================
            # TODO: 步骤 4 - 策略 C (兜底 NLJ + 大小表优化)
            # ==================================
            # 如果没有索引可用，退化为 NestedLoopJoin。
            # 优化: 比较 op1 和 op2 的预估大小 (Catalog.get_table_size)。
            # 始终让较小的表在左侧 (作为驱动表)。

            # [在此处填入代码]

        return NestedLoopJoinOperator(op1, op2)

    def _try_match_inlj(self, outer_op: Operator, inner_op: Operator, join_preds: list) -> Operator | None:
        """
        尝试匹配索引连接 (Index Nested Loop Join)。
        分析 Join 条件，看是否存在 `Outer.col = Inner.IndexedCol` 的模式。
        """
        # 仅支持 CsvScan 作为内表 (简化实现)
        if not isinstance(inner_op, CsvScan):
            return None

        inner_alias = inner_op.alias
        inner_real_name = inner_op.table_name

        temp_aliases = {inner_alias: inner_real_name}

        def get_pure_name(raw_col):
            s = str(raw_col)
            return s.split(".", 1)[1] if "." in s else s

        for pred in join_preds:
            if "eq" not in pred:
                continue
            args = pred["eq"]
            left_arg, right_arg = str(args[0]), str(args[1])

            target_col_ref = None
            outer_join_col = None

            # 尝试解析: Left = Inner.Col
            try:
                if self.catalog.resolve_column_owner(left_arg, temp_aliases) == inner_alias:
                    target_col_ref, outer_join_col = left_arg, right_arg
            except ValueError:
                pass

            # 尝试解析: Inner.Col = Right
            if not target_col_ref:
                try:
                    if self.catalog.resolve_column_owner(right_arg, temp_aliases) == inner_alias:
                        target_col_ref, outer_join_col = right_arg, left_arg
                except ValueError:
                    pass

            # 检查该列是否存在索引
            if target_col_ref:
                pure_col = get_pure_name(target_col_ref)
                idx = self.catalog.get_index(inner_real_name, pure_col)
                if idx and outer_join_col:
                    return IndexNestedLoopJoinOperator(outer_op, idx, outer_join_col, inner_alias)

        return None

    def _find_index_match(self, real_table_name: str, alias: str, where_clause: dict):
        """
        检查单表 WHERE 条件是否匹配该表的任何索引。
        仅支持简单的等值查询 (EQ)。
        """
        if not where_clause or list(where_clause.keys())[0] != "eq":
            return None, None

        op_type = "eq"
        args = where_clause[op_type]

        for i, arg in enumerate(args):
            if not isinstance(arg, str):
                continue

            # 处理别名: alias.col -> col
            search_key = arg
            if alias and arg.startswith(f"{alias}."):
                search_key = arg[len(alias) + 1 :]

            idx = self.catalog.get_index(real_table_name, search_key)
            if idx:
                target_val = args[1 - i]
                return idx, target_val
        return None, None

    # ==========================================
    #  辅助方法
    # ==========================================

    def _distribute_predicates(self, where_clause, table_aliases):
        """
        [作业核心方法]
        负责【逻辑优化】：将 WHERE 子句拆分成"单表过滤"和"连接过滤"。

        参数:
            where_clause: AST 中的 where 部分 (字典或 None)。
            table_aliases: 当前查询涉及的所有表别名 (dict_keys)。

        返回:
            table_preds: 字典 {alias: [cond1, cond2]}，分配给各表的过滤条件。
            join_preds: 列表 [cond3, ...], 必须在 Join 后执行的条件。

        任务提示:
        1. 初始化返回结构。
        2. 处理空条件 (None) 的情况。
        3. 【关键思考】处理 "OR" 条件:
           如果 where_clause 顶层包含 "or" (例如 "A or B")，
           简单的下推可能会导致数据丢失。建议将包含 OR 的复杂条件统一留给 join_preds。
        4. 分配条件:
           - 遍历每个子条件。
           - 分析该条件涉及哪些表 (使用 self._extract_columns 和 self.catalog.resolve_column_owner)。
           - 如果只涉及一张表 -> 放入 table_preds (Selection Pushdown)。
           - 否则 -> 放入 join_preds。
        """
        # ==========================
        # TODO: 请在此处实现谓词分发逻辑
        # ==========================
        # 你的代码应该涵盖初始化、条件遍历分发等所有逻辑。

        # 定义初始结构
        table_preds = {alias: [] for alias in table_aliases}
        join_preds = []

        # ... (请完成剩余逻辑) ...

        # [Default Fallback]
        # 以下为无优化的朴素实现，确保框架可以在无优化的情况下运行。
        # ★★★ 实现完上述任务后，请务必删除或注释掉下方代码 ★★★
        if where_clause:
            join_preds.append(where_clause)
        return table_preds, join_preds

    def _extract_columns(self, expr, collected):
        """递归提取表达式中涉及的所有列名"""
        if isinstance(expr, str):
            collected.add(expr)
        elif isinstance(expr, dict):
            for v in expr.values():
                if isinstance(v, list):
                    for item in v:
                        self._extract_columns(item, collected)
                else:
                    self._extract_columns(v, collected)

    def _apply_projection(self, root: Operator, cols) -> Operator:
        """
        在执行计划顶部添加 Project 算子。
        """
        should_project = True
        target_columns = []

        if not cols or cols == "*":
            should_project = False
        else:
            col_list = cols if isinstance(cols, list) else [cols]
            for c in col_list:
                if isinstance(c, dict) and "all_columns" in c:
                    should_project = False
                    break
                elif isinstance(c, dict) and "value" in c:
                    target_columns.append(c["value"])
                else:
                    target_columns.append(str(c))

        if should_project and target_columns:
            return ProjectOperator(root, target_columns)
        return root
