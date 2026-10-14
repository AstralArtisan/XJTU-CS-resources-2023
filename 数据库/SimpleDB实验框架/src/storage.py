MISSING_COLUMN = object()


class Tuple:
    """
    定义数据的基本单位。
    为了简化，直接使用 Python 的 dict，key 为列名，value 为值。
    """

    def __init__(self, data: dict):
        self.data = data

    def __repr__(self):
        return str(self.data)

    def get(self, column_name):
        if column_name in self.data:
            return self.data[column_name]
        suffix = f".{column_name}"
        matches = [k for k in self.data.keys() if k.endswith(suffix)]

        if len(matches) == 1:
            return self.data[matches[0]]
        elif len(matches) > 1:
            raise ValueError(f"Ambiguous column name '{column_name}', matches: {matches}")

        return MISSING_COLUMN

    def merge(self, other: "Tuple") -> "Tuple":
        """
        合并两个 Tuple。
        """
        new_data = self.data.copy()
        new_data.update(other.data)
        return Tuple(new_data)


class Operator:
    """抽象基类，所有算子（包括 Scan）都应继承它"""

    def __iter__(self):
        return self

    def __next__(self):
        return self.next()

    def next(self):
        raise NotImplementedError

    def reset(self):
        raise NotImplementedError
