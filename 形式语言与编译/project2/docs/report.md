# QCompiler 仓库代码与功能报告

## 1. 项目概述

本仓库实现了课程项目中的简易编译器 QCompiler。项目采用 C++17 编写，通过 CMake 构建，核心目标是把 QL 子集程序编译为 QAST、QTAC、符号表、SLR 分析表和 ARM64 汇编。当前实现支持指导书中的 A/B/C 三条路线：A 路线使用递归下降构造 QAST，B 路线使用 SLR(1) 构造 QAST，C 路线使用 SLR(1) 制导翻译直接生成 IR/QTAC。

仓库内置的示例程序是 `tests/fixtures/qsort.ql`，实现快速排序相关函数：`swap`、`partition`、`qsort`，以及全局数组 `a[10]` 和顶层调用 `qsort(a[], 0, 9,)`。编译器运行后会在 `out/qsort.*` 生成默认 C 路线产物，也可生成 `out/qsortA.*`、`out/qsortB.*`、`out/qsortC.*` 用于三路线对照。

## 2. 工程结构

- `include/qcompiler/`：公共头文件，定义 token、IR、SLR 表、Parser 和 ARM64 生成器接口。
- `src/lexer.cpp`：词法分析器实现。
- `src/slr.cpp`：QL 子集文法、LR(0) 项目集、FIRST/FOLLOW、SLR ACTION/GOTO 表构造。
- `src/parser.cpp`：表驱动 SLR parser 和语法制导翻译。
- `src/qast.cpp`：A/B 路线 QAST 构造、QAST 文本输出和 QAST 到 IR 翻译。
- `src/ir.cpp`：QTAC 文本输出、符号表输出和基础优化。
- `src/arm64.cpp`：QTAC/IR 到 ARM64 汇编的模板式生成。
- `src/main.cpp`：命令行入口，串联词法、语法、IR、优化和汇编输出。
- `tests/test_qcompiler.cpp`：基于 `assert` 的回归测试集合。
- `out/`：当前 qsort 示例的阶段输出，包括 tokens、SLR 表、QTAC、符号表和汇编。

## 3. 构建与运行方式

标准构建流程如下：

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
build/Debug/qcompiler.exe tests/fixtures/qsort.ql --out out/qsort
build/Debug/qcompiler.exe tests/fixtures/qsort.ql --route A --out out/qsortA
build/Debug/qcompiler.exe tests/fixtures/qsort.ql --route B --out out/qsortB
build/Debug/qcompiler.exe tests/fixtures/qsort.ql --route C --out out/qsortC
```

在非 Visual Studio 生成器下，可执行文件位置可能是 `build/qcompiler` 或 `build/qcompiler.exe`。项目也可使用 `g++ -std=c++17 -Wall -Wextra -pedantic` 编译测试目标和 CLI 目标，用于补充 MSVC 之外的告警检查。

## 4. 编译主流程

`src/main.cpp` 的入口支持默认 C 路线和显式路线选择：

```bash
qcompiler <input.ql> [--route A|B|C] --out <output-prefix>
```

执行流程为：

1. 读取输入 QL 源文件。
2. `Lexer::tokenize()` 输出 token 序列。
3. 根据 `--route` 选择 A/B/C 编译路线；未传参数时默认 C。
4. A/B 路线输出 `.qast` 并经 QAST 翻译为 `IRProgram`；C 路线直接生成 `IRProgram`。
5. `optimize_program_basic()` 对 IR 做保守优化。
6. `Arm64Generator::generate()` 分别生成未优化和优化后的 ARM64 汇编。
7. 写出 `.tokens`、`.qtac`、`.symtab`、`.s`、`.opt.s`；B/C 额外写出 `.slr`。

该流程让每个阶段都有可检查文件，便于实验报告、调试和目标机验证。

## 5. 词法分析模块

词法定义位于 `include/qcompiler/token.h`，支持：

- 标识符、整数常量和文件结束 token。
- 关键字：`int`、`void`、`if`、`else`、`while`、`return`。
- 分隔符：`{}`、`()`、`[]`、`;`、`,`。
- 运算符：`+`、`-`、`*`、`/`、`=`、`+=`、`<`、`<=`、`>`、`>=`、`==`、`!=`、`!`、`&&`、`||`。

`src/lexer.cpp` 还处理空白、行注释 `//`、块注释 `/* ... */`，并记录 token 的行列号。错误信息带有位置，例如非法字符或未闭合块注释会抛出 `lex error at line:column`。`normalize_source()` 会把部分常见排版字符标准化，例如长横线转为 `-`，乘号转为 `*`，降低从 PDF 或文档复制源码时的输入风险。

## 6. SLR 文法与分析表

`src/slr.cpp` 中的 `qcompiler_grammar()` 是编译器语法核心。当前文法覆盖：

- 程序块 `{ Items }`。
- 全局/局部变量声明，包括数组声明。
- 函数定义、形参列表和块语句。
- 空语句、表达式语句、`if`、`if/else`、`while`、`return`。
- 标量赋值、数组元素赋值、`+=`。
- 加减乘除、单目负号、括号表达式。
- 函数调用、数组读取、数组实参写法 `arr[]`。
- 条件表达式：关系比较、`!`、`&&`、`||`。

SLR 表构造过程包括：

1. 收集非终结符。
2. 按左部组织产生式。
3. 计算 FIRST 集和 FOLLOW 集。
4. 构造 LR(0) closure/goto 项目集规范族。
5. 根据项目集生成 ACTION/GOTO。
6. 输出可读的表摘要。

当前 `out/qsort.slr` 的统计为：

- states: 125
- actions: 821
- gotos: 199
- unresolved conflicts: 0
- resolved conflicts: 2

两个已消解冲突分别是括号表达式在 `RPAREN` 上选择移进，以及 dangling-else 在 `ELSE` 上选择移进。后者使 `else` 绑定到最近的 `if`，符合常见语言规则。

## 7. Parser 与语义动作

`src/parser.cpp` 的 `SlrSemanticParser` 是 C 路线表驱动 parser。它维护状态栈和语义值栈，读取 lookahead 后执行移进、归约或接受。归约时根据产生式编号调用相应语义动作。为降低维护风险，代码用 `ProductionIndex` 给产生式编号命名，并注明这些编号必须与 `qcompiler_grammar()` 保持一致。

`src/qast.cpp` 提供 A/B 两条 QAST 路线。A 路线使用手写递归下降 parser，表达式采用分层优先级解析以避开左递归；B 路线复用同一张 SLR 表，在归约时构造 QAST。两条路线随后共享 QAST 到 IR 翻译器。

语义上下文 `TranslationContext` 负责：

- 记录全局符号、局部符号和函数签名。
- 生成临时变量 `t0`、`t1` 和标签 `l0`、`l1`。
- 检查重复定义、未定义符号、数组/标量误用、函数参数个数错误。
- 把函数、全局变量和临时变量写入 `IRProgram`。

表达式归约生成 `ExprResult`，包含前置 IR 指令和结果位置。条件归约生成 `CondResult`，包含条件计算指令、左右操作数和关系运算符。`if`、`if/else`、`while` 通过标签和 `IF/GOTO` 指令生成控制流。

重要语义行为包括：

- 标量赋值生成 `Move`，数组读写通过地址计算加 `Load`/`Store`。
- 数组地址计算按 8 字节元素宽度缩放下标。
- 函数调用先生成参数表达式，再生成 `PAR` 和 `CALL`。
- 函数若没有显式 `return`，自动补 `RETURN 0`。
- `!` 条件通过反转关系运算符实现。
- `&&`、`||` 会物化为临时布尔值，再统一作为关系条件处理。

## 8. IR、QTAC 与符号表

IR 定义位于 `include/qcompiler/ir.h`。核心结构包括：

- `IRProgram`：全局变量、函数列表、顶层 main 指令和符号表。
- `FunctionIR`：函数名、返回类型、参数名和函数指令。
- `IRInstruction`：统一表示标签、跳转、条件跳转、参数、调用、返回、二元运算、移动、加载、存储和 NOP。

`src/ir.cpp` 提供三类输出：

- `program_to_qtac()`：输出 QTAC 文本。
- `program_to_symtab()`：输出符号名、类别、所属函数、大小。
- `instruction_to_qtac()`：把单条 IR 指令格式化为可读四地址码。

当前符号类别包括全局标量、全局数组、局部标量、局部数组、标量参数、数组参数和临时变量。数组和标量默认按 8 字节整数处理。

## 9. 基础优化

优化位于 `src/ir.cpp`，函数为 `optimize_basic()` 和 `optimize_program_basic()`。当前优化保持保守：

- 删除 `NOP`。
- 删除 `x = x` 这类自赋值。
- 删除无条件跳转或返回之后、下一标签之前的不可达指令。

优化后的程序只用于生成 `out/qsort.opt.s`。原始 QTAC 和未优化汇编仍保留，便于对照。

## 10. ARM64 汇编生成

`src/arm64.cpp` 使用模板式方式从 IR 生成 ARM64 汇编。输出分为 `.data` 和 `.text` 两段：

- `.data` 为全局变量分配 `.zero <size>`。
- `.text` 声明 `_start`，并输出每个函数。

函数生成逻辑：

- 使用 `x29` 作为帧指针，保存/恢复 `x29`、`x30`。
- 扫描符号和临时变量，为局部变量、参数和临时值分配栈槽。
- 栈帧大小按 16 字节对齐。
- 前 8 个参数使用 `x0` 到 `x7`。
- 函数返回值放在 `x0`。

指令映射要点：

- `If` 生成 `cmp` 加条件分支，如 `b.lt`、`b.ge`。
- `Call` 先装载 pending 参数，再 `bl <callee>`。
- `Binary` 支持 `+`、`-`、`*`、`/`，分别映射到 `add/sub/mul/sdiv`。
- 地址计算使用 `address_calc` 标记，对数组下标乘以 8。
- 全局数组操作使用 `adr` 取地址。
- 全局标量读写通过全局地址再 `ldr/str`。
- 局部数组作为基址处理，禁止直接整体赋值。

`_start` 运行顶层 main 指令后通过 Linux 系统调用 `exit` 退出：`x8 = 93`，`svc #0`。

## 11. 测试覆盖情况

测试集中在 `tests/test_qcompiler.cpp`。当前覆盖：

- 词法分析能识别 qsort 中的关键字和运算符。
- qsort 能解析为包含 1 个全局变量、3 个函数和非空 main 指令的 IR。
- QTAC 中包含全局数组、函数定义、递归调用、条件跳转和内存访问。
- ARM64 输出包含 `_start`、函数标签、递归调用、加载/存储和比较指令。
- SLR 表无未解决冲突，且输出 ACTION/GOTO。
- `if/else`、`!`、`&&`、`||` 的条件语义可生成预期 QTAC。
- 语法错误会报告 SLR parse error。
- 语义错误覆盖未定义变量、重复符号、调用参数个数不匹配、未定义函数、非数组下标访问。
- ARM64 地址生成覆盖全局标量和局部数组。
- 优化器覆盖删除自赋值的基础行为。

测试命令：

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## 12. 阶段输出说明

以 `tests/fixtures/qsort.ql` 为输入，执行：

```bash
build/Debug/qcompiler.exe tests/fixtures/qsort.ql --out out/qsort
build/Debug/qcompiler.exe tests/fixtures/qsort.ql --route A --out out/qsortA
build/Debug/qcompiler.exe tests/fixtures/qsort.ql --route B --out out/qsortB
build/Debug/qcompiler.exe tests/fixtures/qsort.ql --route C --out out/qsortC
```

会生成：

- `out/qsort.tokens`：token 序列及源码行列位置。
- `out/qsort.slr`：SLR productions、ACTION 表、GOTO 表和冲突摘要。
- `out/qsort.qtac`：未优化 QTAC。
- `out/qsort.symtab`：符号表。
- `out/qsort.s`：未优化 ARM64 汇编。
- `out/qsort.opt.s`：基础优化后的 ARM64 汇编。
- `out/qsortA.qast`、`out/qsortB.qast`：A/B 路线 QAST 文本输出。
- `out/qsortA.*`、`out/qsortB.*`、`out/qsortC.*`：三路线对照输出。

这些文件是当前实现是否满足课程阶段输出要求的主要证据。

## 13. 鲲鹏环境验证方式

本仓库在 Windows 本地完成了编译器自身构建和测试。ARM64 汇编需在鲲鹏或兼容 ARM64 Linux 环境上继续验证。推荐步骤：

```bash
as -o qsort.o out/qsort.s
ld -o qsort qsort.o
./qsort
echo $?
```

若程序正常退出，说明 `_start`、函数调用、栈帧、分支和全局数据段在目标机可执行。当前 QL 子集没有输出运行时，qsort 示例主要验证编译链路和运行稳定性；若需观察排序结果，需要扩展运行时输出或用调试器检查全局数组。

## 14. 当前已满足能力

从代码和测试看，当前仓库已经具备：

- 可复现构建：CMake + C++17。
- A/B/C 三路线可切换：递归下降 QAST、SLR-QAST、SLR 制导翻译。
- 表驱动 SLR parser：有完整 ACTION/GOTO 输出。
- 词法、语法、语义、IR、优化、ARM64 后端的完整链路。
- qsort 示例的阶段产物。
- 对常见语法和语义错误的失败测试。
- 对 if/else、while、函数调用、数组访问、递归调用和条件组合的支持。
- 对 Windows 本地和 g++ 编译告警面的基础验证。

## 15. 已知限制与后续改进

当前实现面向课程 QL 子集和 qsort 示例，仍有以下限制：

- 整数统一按 8 字节处理，未实现多种整数宽度。
- 只支持一维数组，不支持复杂聚合类型。
- 函数参数最多按 ARM64 前 8 个寄存器处理，超过 8 个会报错。
- 优化器只做局部保守清理，没有常量传播、公共子表达式删除、活跃变量分析或寄存器分配。
- ARM64 后端使用简单栈槽分配，没有图着色寄存器分配。
- 没有标准库、输入输出运行时和排序结果打印功能。
- 鲲鹏目标机上的汇编、链接和运行截图仍需后续补充。

后续若继续完善，建议优先补充：ARM64 目标机自动化验证脚本、运行时输出支持、更多负例测试、寄存器分配优化和更完整的指导书验收清单。

## 16. AI 使用说明

本项目开发过程中使用 AI 辅助梳理指导书要求、搭建工程结构、实现编译器各阶段、编写测试、修复语义和后端问题，并生成本文档。最终代码仍以仓库中的构建、测试和阶段输出作为可验证依据。
