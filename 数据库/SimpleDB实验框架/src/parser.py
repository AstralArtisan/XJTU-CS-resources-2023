from mo_sql_parsing import parse


def parse_sql(sql_str: str):
    """
    使用 mo-sql-parsing 将 SQL 字符串转换为字典格式的 AST (抽象语法树)。

    Example:
        Input: "SELECT id, name FROM student WHERE age > 20"
        Output: {
            'select': [{'value': 'id'}, {'value': 'name'}],
            'from': 'student',
            'where': {'gt': ['age', 20]}
        }
    """
    try:
        result = parse(sql_str)
        if isinstance(result, list):
            return result[0]
        return result
    except Exception as e:
        print(f"SQL Parsing Error: {e}")
        return None
