import csv
from datetime import datetime

from .storage import Operator, Tuple


class CsvScan(Operator):
    """
    存储层算子：负责读取 CSV 文件。
    它位于火山模型 (Volcano Model) 的最底层。
    """

    def __init__(self, file_path, table_name, alias=None):
        super().__init__()
        self.file_path = file_path
        self.table_name = table_name
        self.alias = alias if alias else table_name
        self.file = None
        self.reader = None
        self._open_file()

    def _open_file(self):
        if self.file:
            self.file.close()
        self.file = open(self.file_path, encoding="utf-8")
        self.reader = csv.DictReader(self.file)

    def reset(self):
        # 优化策略: 如果文件句柄仍有效，使用 seek(0) 避免系统调用的开销
        if self.file and not self.file.closed:
            self.file.seek(0)
            # DictReader 需要重新建立，因为它会消耗第一行作为 Header
            self.reader = csv.DictReader(self.file)
        else:
            self._open_file()

    def close(self):
        if self.file:
            self.file.close()
            self.file = None

    def __iter__(self):
        return self

    def _convert_value(self, v):
        """
        辅助方法：尝试将字符串值转换为具体的 Python 类型 (Int/Float/Date)。
        优先级：None > Int > Float > Date > String
        """
        if v is None or v.strip() == "":
            return None

        try:
            return int(v)
        except ValueError:
            pass

        try:
            return float(v)
        except ValueError:
            pass

        try:
            return datetime.strptime(v, "%Y-%m-%d").date()
        except ValueError:
            pass

        return v

    def next(self):
        assert self.reader is not None

        try:
            row_dict = next(self.reader)
        except StopIteration:
            raise StopIteration from None

        converted_data = {}
        for k, v in row_dict.items():
            full_key = f"{self.alias}.{k}"
            converted_data[full_key] = self._convert_value(v)

        return Tuple(converted_data)

    def get_children(self):
        return []

    def get_info(self):
        return {"Table": self.table_name, "Alias": self.alias}


class ProjectOperator(Operator):
    def __init__(self, child: Operator, columns: list):
        """
        投影算子 (Project / Projection)
        职责：对流经的数据进行列裁剪，只保留指定的列。
        """
        self.child = child
        self.columns = columns

    def next(self):
        """
        [作业核心方法]
        实现投影逻辑。

        提示:
        1. 从 child 获取下一行数据 (Tuple)。
        2. 如果 columns 为空或包含 "*"，则不进行裁剪，直接返回。
        3. 否则，创建一个新字典，提取指定列的数据。
        4. 返回新的 Tuple 对象。
        """
        # ==========================
        # TODO: 请在此处实现 Project 逻辑
        # ==========================
        raise NotImplementedError("TODO: 实现 ProjectOperator.next")

    def reset(self):
        self.child.reset()

    def get_children(self):
        return [self.child]

    def get_info(self):
        return {"Columns": self.columns}


class FilterOperator(Operator):
    def __init__(self, child: Operator, condition_func, rule_repr):
        """
        过滤算子 (Filter / Selection)
        职责：根据条件函数过滤数据流。

        参数:
        child: 上游算子
        condition_func: 谓词函数 (Lambda)，输入 Tuple 返回 bool。
        rule_repr: 条件的字符串表示，用于执行计划打印。
        """
        self.child = child
        self.condition_func = condition_func
        self.rule_repr = rule_repr

    def next(self):
        """
        [作业核心方法]
        实现过滤逻辑。

        算法提示:
        Filter 算子不仅是检查当前行，而是要"找到下一行符合条件的数据"。
        如果从 child 拿到的行不符合条件，你不能直接返回 None，
        而是必须继续向 child 索取下一行，直到：
        1. 找到符合条件的行 -> Return
        2. child 抛出 StopIteration -> Raise StopIteration
        """
        # ==========================
        # TODO: 请在此处实现 Filter 逻辑 (注意循环)
        # ==========================
        raise NotImplementedError("TODO: 实现 FilterOperator.next")

    def reset(self):
        self.child.reset()

    def get_children(self):
        return [self.child]

    def get_info(self):
        return {"Condition": self.rule_repr if self.rule_repr else "Func(...)"}


class LimitOperator(Operator):
    """
    限制算子 (Limit)
    职责：仅返回前 N 行数据，多余的截断。
    """

    def __init__(self, child: Operator, limit: int):
        super().__init__()
        self.child = child
        self.limit = limit
        self.count = 0

    def next(self) -> Tuple:
        if self.count >= self.limit:
            raise StopIteration from None

        tup = self.child.next()
        self.count += 1
        return tup

    def reset(self):
        self.count = 0
        self.child.reset()

    def get_children(self):
        return [self.child]

    def get_info(self):
        return {"Limit": self.limit}


class NestedLoopJoinOperator(Operator):
    """
    嵌套循环连接算子 (Nested Loop Join)
    这是最基础的 Join 算法，通常作为其他优化算法的基准或兜底策略。
    """

    def __init__(self, left: Operator, right: Operator):
        self.left = left
        self.right = right
        # 缓存当前的左表行，用于与右表的每一行进行拼接
        self.current_left_tuple = None

    def next(self) -> Tuple:
        """
        [作业核心方法]
        实现 Nested Loop Join 的迭代逻辑。

        算法伪代码:
        Loop:
            1. 如果 self.current_left_tuple 为空:
               - 从 left 获取下一行
               - 如果 left 结束，则 Join 结束 (Raise StopIteration)

            2. 尝试从 right 获取下一行 (right_tuple):
               - 成功: 返回 merge(left_tuple, right_tuple)
               - 失败 (StopIteration):
                 - reset right
                 - 将 current_left_tuple 置为 None (触发步骤 1 取下一行左表数据)
                 - Continue 循环
        """
        # ==================================
        # TODO: 请在此处实现 NLJ 的 next 逻辑
        # ==================================
        raise NotImplementedError("TODO: 实现 NestedLoopJoinOperator.next")

    def reset(self):
        self.left.reset()
        self.right.reset()
        self.current_left_tuple = None

    def get_children(self):
        return [self.left, self.right]

    def get_info(self):
        return {"Type": "NestedLoop (Cartesian)"}


class IndexScanOperator(Operator):
    """
    索引扫描算子 (Index Scan)
    职责：利用 HashIndex 直接定位数据，避免全表扫描。
    """

    def __init__(self, index_instance, value_to_search, table_alias: str):
        """
        参数:
        index_instance: 构建好的 HashIndex 对象
        value_to_search: 查找目标值 (WHERE col = val)
        table_alias: 表别名 (用于重命名结果列)
        """
        self.index = index_instance
        self.value = value_to_search
        self.table_name = index_instance.table_name
        self.alias = table_alias
        self.results: list[Tuple] = []
        self._iter = None

    def next(self):
        # 懒加载 (Lazy Load): 首次调用时才查询索引
        if self._iter is None:
            # 1. 查索引
            raw_tuples = self.index.search(self.value)

            # 2. 别名处理 (Renaming)
            # 如果查询使用了别名 (e.g., SELECT * FROM student AS s)，
            # 我们需要将数据中的 key 从 "student.name" 改为 "s.name"。
            if self.alias == self.index.table_name:
                self.results = raw_tuples
            else:
                renamed_tuples = []
                for t in raw_tuples:
                    new_data = {}
                    for k, v in t.data.items():
                        suffix = k.split(".", 1)[1] if "." in k else k
                        new_key = f"{self.alias}.{suffix}"
                        new_data[new_key] = v
                    renamed_tuples.append(Tuple(new_data))
                self.results = renamed_tuples

            self._iter = iter(self.results)

        try:
            return next(self._iter)
        except StopIteration:
            raise StopIteration from None

    def reset(self):
        self._iter = None

    def get_children(self):
        return []

    def get_info(self):
        return {"Table": self.table_name, "Alias": self.alias, "Index Lookup": self.value}


class IndexNestedLoopJoinOperator(Operator):
    """
    索引嵌套循环连接 (Index Nested Loop Join, INLJ)

    原理:
    对于左表 (Outer) 的每一行，利用右表 (Inner) 的索引查找匹配行。
    时间复杂度从 O(N*M) 降低到 O(N*1) (假设哈希查找为 O(1))。
    """

    def __init__(self, left_op: Operator, right_index, left_join_col: str, right_alias: str):
        self.left = left_op
        self.right_index = right_index
        self.left_join_col = left_join_col  # 左表关联列名
        self.right_alias = right_alias  # 右表别名

        self._left_iter = None
        self._current_left_tuple = None
        self._current_right_matches = []  # 缓存索引查找结果
        self._match_index = 0

    def next(self):
        if self._left_iter is None:
            self._left_iter = iter(self.left)

        while True:
            # 1. 如果当前左行对应的右表匹配项还有剩余，继续输出
            if self._match_index < len(self._current_right_matches):
                assert self._current_left_tuple is not None, "Logic Error: Left tuple is None but matches exist!"
                right_tuple = self._current_right_matches[self._match_index]
                self._match_index += 1
                return self._current_left_tuple.merge(right_tuple)

            # 2. 取下一行左表数据
            try:
                self._current_left_tuple = next(self._left_iter)
            except StopIteration:
                raise StopIteration from None

            # 3. 利用左表的值查右表索引
            val_to_search = self._current_left_tuple.get(self.left_join_col)

            if val_to_search is None:
                self._current_right_matches = []
            else:
                raw_matches = self.right_index.search(val_to_search)

                # 4. 运行时重命名 (Runtime Renaming)
                # 索引内存储的是原始表名 (e.g. "student.id")，需要改为当前别名 (e.g. "s2.id")
                self._current_right_matches = []
                for t in raw_matches:
                    new_data = {}
                    for k, v in t.data.items():
                        suffix = k.split(".")[-1] if "." in k else k
                        new_key = f"{self.right_alias}.{suffix}"
                        new_data[new_key] = v
                    self._current_right_matches.append(Tuple(new_data))

            self._match_index = 0

    def reset(self):
        self.left.reset()
        self._left_iter = None
        self._current_right_matches = []
        self._match_index = 0

    def get_children(self):
        return [self.left]

    def get_info(self):
        return {
            "Join Type": "Index Join (INLJ)",
            "Left Col": self.left_join_col,
            "Right Table": f"{self.right_alias} (Indexed)",
        }
