import os

from .catalog import Catalog
from .index import HashIndex
from .operators import CsvScan
from .optimizer import QueryOptimizer
from .parser import parse_sql


class Executor:
    """
    执行器 (Executor)
    职责：
    1. 协调各组件 (Catalog, Optimizer) 工作。
    2. 接收 SQL 文本，驱动解析、优化与执行流程。
    3. 管理 DDL 操作 (如索引创建)。
    """

    def __init__(self, data_dir: str = "data"):
        self.catalog = Catalog(data_dir)
        self.optimizer = QueryOptimizer(self.catalog)
        # 缓存上一次的执行状态 (用于调试输出)
        self.last_plan = None
        self.last_ast = None

    def execute(self, sql: str) -> list:
        """
        执行 SQL 查询。

        流程:
        SQL Text -> AST -> Execution Plan (Optimized) -> Iterator Execution
        """
        # 1. 解析 (Parsing)
        self.last_ast = parse_sql(sql)
        if not self.last_ast:
            return []

        # 2. 规划与优化 (Planning & Optimization)
        self.last_plan = self.optimizer.plan_query(self.last_ast)

        if self.last_plan is None:
            return []

        # 3. 执行 (Execution)
        # 驱动算子树进行迭代，获取所有结果
        try:
            results = list(self.last_plan)
            return results
        except Exception as e:
            print(f"Runtime Error: {e}")
            return []

    def set_optimization(self, enable: bool):
        """开关优化器"""
        self.optimizer.enable_optimization = enable
        state = "ENABLED" if enable else "DISABLED"
        print(f"   [System] Optimizer is now {state}.")

    def create_index(self, table_name: str, column_name: str):
        """
        处理 .create_index 命令。
        注意：DDL 操作不经过优化器，直接操作底层存储和 Catalog。
        """
        # 1. 校验表文件是否存在
        file_path = self.catalog.get_table_path(table_name)
        if not os.path.exists(file_path):
            print(f"Error: Table '{table_name}' not found.")
            return

        # 2. 构建索引
        # 使用原始的 CsvScan 遍历全表数据 (绕过优化器，避免循环依赖)
        scan_op = CsvScan(file_path, table_name)
        index = HashIndex()
        full_col_name = f"{table_name}.{column_name}"

        try:
            # 执行构建 (Build Phase)
            index.build(scan_op, full_col_name, table_name)

            # 注册到 Catalog
            self.catalog.register_index(table_name, column_name, index)
            print(f"Index created on {full_col_name}")

        except Exception as e:
            print(f"Failed to create index: {e}")
