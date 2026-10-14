# σ-DFA 设计文档

## 1. 输入符号集 Σ

DFA的输入符号为Pass1词法分析器输出的记号类型：

| 符号 | 含义 | 示例 |
|------|------|------|
| `KEY:LABEL` | 关键字LABEL | LABEL |
| `KEY:GOTO` | 关键字GOTO | GOTO |
| `KEY:IF` | 关键字IF | IF |
| `KEY:THEN` | 关键字THEN | THEN |
| `KEY:ELSE` | 关键字ELSE | ELSE |
| `KEY:RETURN` | 关键字RETURN | RETURN |
| `KEY:PAR` | 关键字PAR | PAR |
| `KEY:CALL` | 关键字CALL | CALL |
| `KEY:M` | 关键字M | M |
| `d` | 普通变量 | n, a |
| `temp` | 临时变量 | t1, t2 |
| `l` | 标号 | l0, l1 |
| `k` | 立即数 | 2, 1 |
| `rop` | 关系运算符 | <, >= |
| `aop` | 算术运算符 | +, -, * |
| `=` | 赋值符号 | = |
| `;` | 分号 | ; |
| `[` | 左方括号 | [ |
| `]` | 右方括号 | ] |
| `,` | 逗号 | , |

其中 `d`, `temp` 统称为 `q`（变量类操作数），`q` 和 `k` 统称为 `a`（通用操作数）。

## 2. 状态集 Q

| 状态 | 含义 |
|------|------|
| S0 | 初始状态 |
| S_LABEL | 已读入 LABEL |
| S_LABEL_L | 已读入 LABEL l |
| S_GOTO | 已读入 GOTO |
| S_GOTO_B | 已读入 GOTO b |
| S_RETURN | 已读入 RETURN |
| S_RETURN_A | 已读入 RETURN a |
| S_IF | 已读入 IF |
| S_IF_Q | 已读入 IF q |
| S_IF_ROP | 已读入 IF q rop |
| S_IF_A | 已读入 IF q rop a |
| S_IF_THEN | 已读入 IF q rop a THEN |
| S_IF_L1 | 已读入 IF q rop a THEN l1 |
| S_IF_ELSE | 已读入 IF q rop a THEN l1 ELSE |
| S_IF_L2 | 已读入 IF q rop a THEN l1 ELSE l2 |
| S_Q | 已读入 q（变量/临时变量） |
| S_Q_EQ | 已读入 q = |
| S_Q_EQ_A | 已读入 q = a |
| S_Q_EQ_A_AOP | 已读入 q = a aop |
| S_Q_EQ_A_AOP_A | 已读入 q = a aop a |
| S_Q_EQ_CALL | 已读入 q = CALL |
| S_Q_EQ_CALL_D | 已读入 q = CALL d |
| S_Q_EQ_CALL_COMMA | 已读入 q = CALL d , |
| S_Q_EQ_CALL_K | 已读入 q = CALL d , k |
| S_Q_EQ_M | 已读入 q = M |
| S_Q_EQ_M_LB | 已读入 q = M [ |
| S_Q_EQ_M_B | 已读入 q = M [ b |
| S_Q_EQ_M_RB | 已读入 q = M [ b ] |
| S_M | 已读入 M |
| S_M_LB | 已读入 M [ |
| S_M_B | 已读入 M [ b |
| S_M_RB | 已读入 M [ b ] |
| S_M_EQ | 已读入 M [ b ] = |
| S_M_EQ_Q | 已读入 M [ b ] = q |
| S_PAR | 已读入 PAR |
| S_PAR_A | 已读入 PAR a |

### 接受状态

| 接受状态 | 识别的模板 |
|----------|-----------|
| ACCEPT_LABEL | T_LABEL: `LABEL l ;` |
| ACCEPT_GOTO | T_GOTO: `GOTO b ;` |
| ACCEPT_RETURN | T_RETURN: `RETURN a ;` |
| ACCEPT_IF | T_IF / T_IF_LABEL |
| ACCEPT_AOP | T_AOP: `q = q aop a ;` |
| ACCEPT_MOVE | T_MOVE: `q = a ;` |
| ACCEPT_LDR0 | T_LDR0: `q = M[b] ;` |
| ACCEPT_STR0 | T_STR0: `M[b] = q ;` |
| ACCEPT_PAR_CALL | T_PAR_CALL: `(PAR a ;)* q = CALL d, k ;` |

## 3. 状态转移表 DELTA[Q][Σ]

核心状态转移（`-` 表示错误/不可达）：

| 当前状态 | KEY:LABEL | KEY:GOTO | KEY:IF | KEY:RETURN | KEY:PAR | KEY:M | KEY:THEN | KEY:ELSE | KEY:CALL | q(d/temp) | l | k | rop | aop | = | ; | [ | ] | , |
|----------|-----------|----------|--------|------------|---------|-------|----------|----------|----------|-----------|---|---|-----|-----|---|---|---|---|---|
| **S0** | S_LABEL | S_GOTO | S_IF | S_RETURN | S_PAR | S_M | - | - | - | S_Q | - | - | - | - | - | - | - | - | - |
| **S_LABEL** | - | - | - | - | - | - | - | - | - | - | S_LABEL_L | - | - | - | - | - | - | - | - |
| **S_LABEL_L** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | **ACC_LABEL** | - | - | - |
| **S_GOTO** | - | - | - | - | - | - | - | - | - | S_GOTO_B | S_GOTO_B | - | - | - | - | - | - | - | - |
| **S_GOTO_B** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | **ACC_GOTO** | - | - | - |
| **S_RETURN** | - | - | - | - | - | - | - | - | - | S_RETURN_A | - | S_RETURN_A | - | - | - | - | - | - | - |
| **S_RETURN_A** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | **ACC_RETURN** | - | - | - |
| **S_IF** | - | - | - | - | - | - | - | - | - | S_IF_Q | - | - | - | - | - | - | - | - | - |
| **S_IF_Q** | - | - | - | - | - | - | - | - | - | - | - | - | S_IF_ROP | - | - | - | - | - | - |
| **S_IF_ROP** | - | - | - | - | - | - | - | - | - | S_IF_A | - | S_IF_A | - | - | - | - | - | - | - |
| **S_IF_A** | - | - | - | - | - | - | S_IF_THEN | - | - | - | - | - | - | - | - | - | - | - | - |
| **S_IF_THEN** | - | - | - | - | - | - | - | - | - | - | S_IF_L1 | - | - | - | - | - | - | - | - |
| **S_IF_L1** | - | - | - | - | - | - | - | S_IF_ELSE | - | - | - | - | - | - | - | - | - | - | - |
| **S_IF_ELSE** | - | - | - | - | - | - | - | - | - | - | S_IF_L2 | - | - | - | - | - | - | - | - |
| **S_IF_L2** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | **ACC_IF** | - | - | - |
| **S_Q** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | S_Q_EQ | - | - | - | - |
| **S_Q_EQ** | - | - | - | - | - | S_Q_EQ_M | - | - | S_Q_EQ_CALL | S_Q_EQ_A | - | S_Q_EQ_A | - | - | - | - | - | - | - |
| **S_Q_EQ_A** | - | - | - | - | - | - | - | - | - | - | - | - | - | S_Q_EQ_A_AOP | - | **ACC_MOVE** | - | - | - |
| **S_Q_EQ_A_AOP** | - | - | - | - | - | - | - | - | - | S_Q_EQ_A_AOP_A | - | S_Q_EQ_A_AOP_A | - | - | - | - | - | - | - |
| **S_Q_EQ_A_AOP_A** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | **ACC_AOP** | - | - | - |
| **S_Q_EQ_CALL** | - | - | - | - | - | - | - | - | - | S_Q_EQ_CALL_D | - | - | - | - | - | - | - | - | - |
| **S_Q_EQ_CALL_D** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | S_Q_EQ_CALL_COMMA |
| **S_Q_EQ_CALL_COMMA** | - | - | - | - | - | - | - | - | - | - | - | S_Q_EQ_CALL_K | - | - | - | - | - | - | - |
| **S_Q_EQ_CALL_K** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | **ACC_PAR_CALL** | - | - | - |
| **S_Q_EQ_M** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | S_Q_EQ_M_LB | - | - |
| **S_Q_EQ_M_LB** | - | - | - | - | - | - | - | - | - | S_Q_EQ_M_B | S_Q_EQ_M_B | - | - | - | - | - | - | - | - |
| **S_Q_EQ_M_B** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | S_Q_EQ_M_RB | - |
| **S_Q_EQ_M_RB** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | **ACC_LDR0** | - | - | - |
| **S_M** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | S_M_LB | - | - |
| **S_M_LB** | - | - | - | - | - | - | - | - | - | S_M_B | S_M_B | - | - | - | - | - | - | - | - |
| **S_M_B** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | S_M_RB | - |
| **S_M_RB** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | S_M_EQ | - | - | - | - |
| **S_M_EQ** | - | - | - | - | - | - | - | - | - | S_M_EQ_Q | - | - | - | - | - | - | - | - | - |
| **S_M_EQ_Q** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | **ACC_STR0** | - | - | - |
| **S_PAR** | - | - | - | - | - | - | - | - | - | S_PAR_A | - | S_PAR_A | - | - | - | - | - | - | - |
| **S_PAR_A** | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | **→S0**(收集) | - | - | - |

## 4. 初始状态和接受状态

- 初始状态: **S0**
- 接受状态: 所有 `ACC_*` 状态（见上表中加粗标记）
- 到达接受状态后自动回到 S0 开始下一轮匹配

## 5. 附加信息管理

DFA在状态转移过程中需要收集附加信息（σ信息）：

| 模板 | 收集的附加信息 |
|------|---------------|
| T_LABEL | 标号名 l |
| T_GOTO | 跳转目标 b |
| T_RETURN | 返回值 a, a的类型(q/k) |
| T_IF / T_IF_LABEL | 比较操作数 q, rop, a, 目标标号 l1, l2, (l3) |
| T_AOP | 目标 q1, 源操作数 q2, 运算符 aop, 操作数 a, a的类型 |
| T_MOVE | 目标 q, 源 a, a的类型 |
| T_LDR0 | 目标 q, 地址 b |
| T_STR0 | 地址 b, 源 q |
| T_PAR_CALL | 目标 q, 函数名 d, 参数个数 k, 参数列表 pars |

T_IF_LABEL 的特殊处理：当 T_IF 到达接受状态后，执行1步向前看(lookahead)。若下一条是 `LABEL l3 ;` 且 l3 等于 l1 或 l2，则合并为 T_IF_LABEL 模板，消耗额外3个记号。否则保持为 T_IF。
