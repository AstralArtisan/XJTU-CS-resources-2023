from typing import Any

from .storage import Operator, Tuple


class HashIndex:
    """
    基于内存哈希表的简易索引。

    原理:
    利用 Python 的 dict (哈希表) 存储列值到数据行的映射。
    结构: { column_value: [Tuple1, Tuple2, ...] }

    注意：
    因为一个具体的列值（如 age=20）可能对应多行数据，
    所以字典的 Value 必须是一个列表 (List[Tuple])。
    """

    def __init__(self):
        # 核心存储结构
        self.index_map: dict[Any, list[Tuple]] = {}
        # 元数据
        self.indexed_column: str | None = None
        self.table_name: str | None = None

    def build(self, source_operator: Operator, full_column_name: str, table_name: str):
        """
        [作业核心方法]
        构建索引：遍历数据源，建立哈希映射。

        参数:
            source_operator: 数据源算子 (通常是 CsvScan)，你需要遍历它。
            full_column_name: 需要索引的完整列名 (例如 "student.id")。
            table_name: 表名 (仅作记录)。

        实现提示:
        1. 清空旧数据 (self.index_map = {})。
        2. 重置算子。
        3. 遍历算子中的每一行。
        4. 从行中获取目标列的值。
        5. 将该行添加到 self.index_map[key] 的列表中。
           注意：如果 key 第一次出现，需要先初始化一个空列表。
        """
        print(f"Building Hash Index on {full_column_name} ...")
        self.indexed_column = full_column_name
        self.table_name = table_name
        self.index_map = {}

        # ==========================
        # TODO: 请在此处实现索引构建逻辑
        # ==========================
        # 提示：不要忘记处理 key 重复的情况 (Hash Collision)
        raise NotImplementedError("TODO: 实现 HashIndex.build")

        print(f"Index built! Size: {len(self.index_map)} keys.")

    def search(self, value: Any) -> list[Tuple]:
        """
        [作业核心方法]
        查找索引。

        参数:
            value: 要查找的目标值 (例如 WHERE id = 1 中的 1)。

        返回:
            List[Tuple]: 匹配该值的所有行。如果没有匹配，返回空列表 []。
        """
        # ==========================
        # TODO: 请在此处实现索引查找逻辑
        # ==========================
        raise NotImplementedError("TODO: 实现 HashIndex.search")
