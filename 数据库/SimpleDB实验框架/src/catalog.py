import csv
import os


class Catalog:
    """
    元数据管理器 (Catalog)
    职责：
    1. 管理数据库数据目录，提供物理文件路径查找。
    2. 缓存与提供表结构 (Schema) 信息。
    3. 解析 SQL 表引用 (处理 table AS alias)。
    4. 解析列归属 (处理 col, table.col, alias.col)。
    5. 管理运行时索引 (注册与查找)。
    """

    def __init__(self, data_dir: str = "data"):
        self.data_dir = data_dir
        # 索引注册表: { "table_real_name.column_name": IndexInstance }
        self.indexes = {}
        # Schema 缓存: { "table_real_name": ["id", "name", ...] }
        self._schema_cache = {}

    def get_table_path(self, table_name: str) -> str:
        """根据表名获取 CSV 文件的绝对/相对路径"""
        if table_name.endswith(".csv"):
            filename = table_name
        else:
            filename = f"{table_name}.csv"
        return os.path.join(self.data_dir, filename)

    def get_table_columns(self, table_name: str) -> list[str]:
        """
        读取 CSV 表头获取列名 (Lazy Load + Cache)。
        """
        if table_name in self._schema_cache:
            return self._schema_cache[table_name]

        file_path = self.get_table_path(table_name)
        if not os.path.exists(file_path):
            raise FileNotFoundError(f"Table file not found: {file_path}")

        try:
            with open(file_path, encoding="utf-8") as f:
                reader = csv.reader(f)
                headers = next(reader)
                cols = [h.strip() for h in headers]
                self._schema_cache[table_name] = cols
                return cols
        except Exception as e:
            print(f"[Catalog Warning] Failed to read schema for {table_name}: {e}")
            return []

    def get_table_size(self, table_name: str) -> float:
        """
        获取表文件的大小 (Bytes)。
        优化器使用该数值估算 IO 代价。
        """
        file_path = self.get_table_path(table_name)
        try:
            return os.path.getsize(file_path)
        except OSError:
            # 如果文件不存在或无法读取，返回无穷大，避免优化器误选
            return float("inf")

    def resolve_table_info(self, table_ref) -> tuple[str, str]:
        """
        解析 mo-sql-parsing 库生成的 FROM 子句对象。

        参数:
        table_ref: 可能是字符串 (直接表名) 或字典 (包含 'value' 和 'name' 别名)。

        返回:
        (real_table_name, alias)
        """
        # Case 1: 字典格式 {'value': 'student', 'name': 's'}
        if isinstance(table_ref, dict):
            real_name = table_ref.get("value")
            alias = table_ref.get("name")
            if not isinstance(real_name, str):
                raise ValueError(f"Invalid table name: {real_name}")
            # 如果没有别名，别名默认为真名
            return real_name, (alias if alias else real_name)

        # Case 2: 字符串格式 'student'
        elif isinstance(table_ref, str):
            return table_ref, table_ref

        else:
            raise ValueError(f"Unknown table ref type: {type(table_ref)}")

    def resolve_column_owner(self, col_ref: str, current_aliases: dict) -> str:
        """
        解析列归属：判断一个列名属于当前查询范围内的哪个表(别名)。

        参数:
        col_ref: 列名 (例如 "id" 或 "s.id")
        current_aliases: 当前有效的别名映射 {alias: real_name}

        返回:
        owner_alias: 该列所属的表别名。
        """
        # Case 1: 显式指定了别名 (e.g., "s.id")
        if "." in col_ref:
            prefix, _ = col_ref.split(".", 1)
            if prefix in current_aliases:
                return prefix
            # 如果前缀不是已知别名，可能 SQL 语义错误，暂且忽略等待后续处理
            pass

        # Case 2: 未指定别名 (e.g., "id") -> 模糊匹配
        # 需要遍历所有涉及的表，看哪个表里有这个列
        matches = []
        for alias, real_name in current_aliases.items():
            cols = self.get_table_columns(real_name)
            if col_ref in cols:
                matches.append(alias)

        if len(matches) == 1:
            return matches[0]
        elif len(matches) > 1:
            raise ValueError(f"Ambiguous column '{col_ref}', found in tables: {matches}")

        raise ValueError(f"Column '{col_ref}' not found in any table scope.")

    def register_index(self, table_name: str, column_name: str, index_instance):
        """注册内存中的索引实例"""
        key = f"{table_name}.{column_name}"
        self.indexes[key] = index_instance

    def get_index(self, table_name: str, column_name: str):
        """查找索引实例"""
        key = f"{table_name}.{column_name}"
        return self.indexes.get(key)
