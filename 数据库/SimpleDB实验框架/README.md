# SimpleDB: A Python-based Relational Database Kernel

**SimpleDB** 是一个用于教学的关系型数据库内核实现。它采用标准的 **火山模型 (Volcano Model)** 架构，支持 SQL 解析、查询计划生成、算子执行以及基于规则和代价的查询优化 (RBO/CBO)。

通过本项目，你将实现数据库的核心组件，深入理解 SQL 文本如何经过解析、优化和执行最终生成查询结果的过程，并验证查询优化对系统性能的影响。

---

## 🚀 1. 环境准备 (Getting Started)

在开始编码之前，请完成以下环境配置。

### 1.1 安装依赖

本项目依赖 `mo-sql-parsing` 库进行 SQL 解析。请确保使用 Python 3.10 或更高版本。

```bash
pip install -r requirements.txt
```

### 1.2 生成测试数据

项目提供了一个数据生成脚本。**首次运行前必须执行此步骤**，以初始化数据库文件。

```bash
python datagen.py
```

> 执行后将在 `data/` 目录下生成三个文件：`student.csv` (500行), `score.csv` (5000行), `course.csv` (50行)。

### 1.3 启动数据库

运行入口脚本进入交互式终端：

```bash
python main.py
```

- 输入 `.tables` 查看当前表列表。
- 输入 `exit` 退出程序。
- SQL 语句支持以分号 `;` 结尾，或通过输入空行（连续回车）结束。

---

## 📂 2. 项目结构 (Project Structure)

核心代码位于 `src/` 目录下。请关注标记为 **[TODO]** 的文件并完成相应的代码实现。

```text
SimpleDB/
├── data/                  # 存放 .csv 数据文件
├── src/
│   ├── __init__.py
│   ├── catalog.py         # [只读] 元数据管理 (负责表结构解析、统计信息获取)
│   ├── executor.py        # [只读] 执行器 (负责驱动查询计划的执行)
│   ├── index.py           # [TODO] 哈希索引构建逻辑 (任务 2)
│   ├── operators.py       # [TODO] 物理算子实现 (任务 1)
│   ├── optimizer.py       # [TODO] 查询优化器实现 (任务 3)
│   ├── parser.py          # [只读] SQL 解析器封装
│   ├── storage.py         # [只读] 基础数据结构 Tuple
│   └── utils.py           # [只读] 工具函数 (用于打印 AST 和执行计划)
├── main.py                # [只读] 程序入口 (REPL 交互环境)
├── datagen.py             # 数据生成脚本
├── requirements.txt       # 依赖描述文件
└── test.sql               # 测试用例集
```

---

## 🧠 3. 核心原理 (Core Concepts)

**在编码前，请理解 SimpleDB 的执行机制与数据流转逻辑。**

### 3.1 火山模型 (Volcano Model)

SimpleDB 采用火山模型（Volcano Model）。这是一种**基于拉取（Pull-based）**的执行方式，其核心在于**控制流自顶向下，数据流自底向上**。

整个查询计划被构建为一棵由算子（Operator）组成的树。数据流转的物理路径如下：

1.  **驱动层 (Executor)**：
    执行器位于树的顶端。它不直接处理数据，而是通过迭代（如 `for` 循环）向根算子请求数据。

2.  **算子层 (Operators)**：
    当上层算子被调用时，它会调用下层算子的接口请求数据。
    - **控制流向下传递**：请求从 Root 算子层层传递至 Leaf（叶子）算子。
    - **数据流向上传递**：Leaf 算子从磁盘读取数据封装为 `Tuple`，通过 `return` 返回给上层。上层算子处理（如过滤、投影）后，继续返回给更上层。

3.  **存储层 (Storage)**：
    位于最底层的 Scan 算子负责直接读取 CSV 文件。

### 3.2 算子即迭代器 (Operators as Iterators)

在 Python 实现中，每个算子本质上都是一个**迭代器 (Iterator)**。

这意味着你不仅可以通过 `.next()` 单步获取数据，更可以通过标准的 `for` 循环或其他迭代工具（如 `list()`）来消耗算子产生的数据流。

在实现 `operators.py` 时，需遵循以下规范：

- **实现 `__iter__`**: 返回 `self` 即可。
- **实现 `next`**:
  - 这是算子的核心逻辑。
  - 每次调用仅处理并返回**一行**数据 (`Tuple`)。
  - 当没有更多数据时，必须抛出 `StopIteration` 异常，以通知上层停止拉取。
- **非阻塞式执行**: 算子不应一次性读取所有数据（除非是构建哈希表等特殊情况），而应“按需生产”，以支持处理大于内存的数据集。

---

## 🛠️ 4. 开发任务 (Development Tasks)

请按照以下阶段顺序完成开发。每个阶段完成后，使用 `test.sql` 中的用例进行验证。

### 🟢 Phase 1: 基础算子 (Basic Operators)

**目标文件**: `src/operators.py`

实现火山模型的基础执行逻辑：

1.  **ProjectOperator.next()**: 实现投影逻辑，仅返回 SQL 中指定的列。
2.  **FilterOperator.next()**: 实现过滤逻辑。循环读取子节点数据，直到发现符合 `WHERE` 条件的行或子节点数据耗尽。
3.  **NestedLoopJoinOperator.next()**: 实现嵌套循环连接。需要维护左表当前的行 (`current_left_tuple`) 作为状态，遍历右表进行匹配。

**✅ 验证方式**:
完成`test.sql`中的 Phase 1 实验.

---

### 🟡 Phase 2: 索引构建 (Index Construction)

**目标文件**: `src/index.py`

实现哈希索引以提升查询效率：

1.  **HashIndex.build()**: 遍历输入的 Scan 算子，在内存中构建哈希表。
    - _注意_：哈希表的 Key 为列值，Value 应为 `list[Tuple]`，以处理哈希冲突（多行数据具有相同 Key）的情况。

**✅ 验证方式**:
完成`test.sql`中的 Phase 2 实验.

---

### 🔴 Phase 3: 查询优化器 (Query Optimizer)

**目标文件**: `src/optimizer.py`

本阶段的目标是实现基于规则 (RBO) 和代价 (CBO) 的优化，并通过对比实验验证性能提升。

**核心任务**:

1.  **\_distribute_predicates**:
    - 实现选择下推。将 `WHERE` 子句拆解，尽早过滤数据。
2.  **\_create_optimized_leaf**:
    - 识别 `col = val` 条件，用 `IndexScan` 替代全表扫描。
3.  **\_create_optimized_join**:
    - **连接优化**: 无论 SQL 中 `FROM` 的顺序如何，确保总是“小表驱动大表”（小表在外层循环）。
    - **利用已有索引优化连接 (INLJ)**: 如果内层表（大表）有索引，利用索引加速连接，避免全表扫描。

**✅ 验证方式**:
完成`test.sql`中的 Phase 3 实验.

---

## 💡 使用指南 (Usage Guide)

### 常用命令

- `.tables`: 列出当前所有表。
- `.create_index [table] [col]`: 在指定表的列上构建哈希索引。
- `.opt on` / `.opt off`: 开启或关闭优化器。
- `exit`: 退出程序。

### 调试输出说明

查询执行后，系统会输出以下调试信息：

1.  **AST**: SQL 解析后的抽象语法树（JSON 格式），用于辅助编写优化器逻辑。
2.  **Execution Plan**: 最终的算子树结构。检查是否生成了 `IndexScan` 或 `Index Nested Loop Join` 以确认优化器是否生效。

---

## 📎 附录：核心数据结构与接口 (Appendix)

在开发过程中，你将频繁与以下数据结构和接口交互。

### 1. Tuple (`src/storage.py`)

`Tuple` 是系统内部传递数据的标准单位。它不仅是对字典的封装，还提供了 Join 和别名查找所需的辅助方法。

```python
class Tuple:
    def __init__(self, data: dict):
        # 内部存储格式: {"TableName.ColumnName": Value}
        # 例如: {"student.id": 1, "student.name": "Alice", "s.id": 1}
        self.data = data

    def get(self, field: str):
        """
        获取列值。支持自动处理表名限定。
        假设当前 Tuple 包含 {"student.id": 1, ...}
        - t.get("student.id") -> 返回 1
        - t.get("id")         -> 如果无歧义，也返回 1
        """
        ...

    def merge(self, other: 'Tuple') -> 'Tuple':
        """
        [Join 算子核心] 将当前 Tuple 与另一个 Tuple 合并。
        返回一个新的 Tuple 对象，包含两者的所有列。
        """
        ...
```

### 2. Operator (`src/operators.py`)

这是所有算子的基类。为了确保你的算子能被 `Executor` 正确执行并能被 `utils.print_plan` 正确打印，需要以下接口(大部分已实现)：

- **`__iter__(self)`**: 返回 `self`。
- **`next(self) -> Tuple`**:
  - 每次调用返回**一行**结果。
  - **关键**: 不要一次性计算所有结果存列表，必须是用 `yield` 或保存状态的迭代器模式，否则内存会溢出。
  - 数据耗尽时抛出 `StopIteration`。
- **`reset(self)`**:
  - **[NLJ 算子必读]** 将算子状态重置到开头。
  - 在嵌套循环连接中，内表（右表）会被多次遍历，每次外表换行时，都需要调用右表的 `reset()`。
- **`get_children(self) -> list`**:
  - 返回子算子列表。用于打印执行计划树。
- **`get_info(self) -> dict`**:
  - 返回算子的内部参数（如 Filter 的条件 string，Join 的类型）。用于打印执行计划详情。

### 3. HashIndex (`src/index.py`)

你需要构建的哈希索引应采用以下内存结构，以处理非唯一键（Non-Unique Keys）的情况：

```python
# 字典结构示例
# Key: 索引列的值
# Value: 包含该值的 Tuple 列表 (List[Tuple])
self.index_map = {
    101: [Tuple(row_A), Tuple(row_B)],  # 两个学生都在 101 班
    102: [Tuple(row_C)]
}
```

### 4. AST 结构全解 (`src/optimizer.py`)

SQL 解析器会将 SQL 语句转换为嵌套字典（JSON 风格）。在编写优化器时，你需要解析这些结构。

你也可以运行 main.py 直接输入 SQL 语句查看解析结果，尽可能多尝试一些 SQL 语句，有助于你着手编程。

**SQL 示例**:

```sql
SELECT name FROM student AS s, score
WHERE s.id = score.sid AND s.age > 20
```

**对应的 AST 结构**:

```python
{
  "select": [{ "value": "name" }],

  "from": [
    { "value": "student", "name": "s" }, // 带别名: value是真名, name是别名
    "score" // 无别名: 直接是字符串
  ],

  "where": {
    "and": [
      {
        "eq": ["s.id", "score.sid"] // 连接条件
      },
      {
        "gt": ["s.age", 20] // 过滤条件
      }
    ]
  }
}
```

- **注意**: 所有的列名引用在 AST 中都是字符串，所有的字面量（如 `20`）也是直接的值。
