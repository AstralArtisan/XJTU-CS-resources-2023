import csv
import os
import sys
import time
import unicodedata

from src.executor import Executor
from src.storage import Tuple
from src.utils import print_ast, print_plan

# 配置数据目录
DATA_DIR = "data"


def print_welcome():
    """打印系统启动 Banner"""
    print("=" * 60)
    print("SimpleDB: A Simple Database Implementation in Python")
    print("Type 'exit' or 'quit' to leave.")
    print("Type '.tables' to see available tables.")
    print("=" * 60)


def get_display_width(s: str) -> int:
    """
    计算字符串在终端显示的宽度。
    对于东亚字符 (Fullwidth/Wide)，宽度计为 2，其他计为 1。
    """
    width = 0
    for ch in s:
        if unicodedata.east_asian_width(ch) in ("F", "W", "A"):
            width += 2
        else:
            width += 1
    return width


def pad_string(s: str, width: int) -> str:
    """对字符串进行右侧空格填充，以适配指定的显示宽度"""
    current_width = get_display_width(s)
    padding_len = max(0, width - current_width)
    return s + " " * padding_len


def print_table(results: list[Tuple]):
    """
    格式化打印查询结果。
    自动计算每一列的最大显示宽度以确保对齐 (支持中文字符)。
    """
    if not results:
        print("(Empty set)")
        return

    # 获取表头
    headers = list(results[0].data.keys())

    def fmt_val(val):
        return "NULL" if val is None else str(val)

    # 初始化列宽为表头宽度
    col_widths = {h: get_display_width(h) for h in headers}

    # 扫描数据，更新最大列宽
    for row in results:
        for col in headers:
            val = fmt_val(row.data.get(col))
            col_widths[col] = max(col_widths[col], get_display_width(val))

    # 生成分割线
    separator = "-" * (sum(col_widths.values()) + 3 * (len(headers) - 1))

    # 打印表头
    print(separator)
    header_str = " | ".join([pad_string(h, col_widths[h]) for h in headers])
    print(header_str)
    print(separator)

    # 打印数据行
    for row in results:
        values = []
        for h in headers:
            val = fmt_val(row.data.get(h))
            values.append(pad_string(val, col_widths[h]))
        print(" | ".join(values))


def save_to_csv(results: list[Tuple], filename: str = "result.csv"):
    """
    将查询结果全量保存到 CSV 文件中，方便查看大数据集。
    """
    if not results:
        # 创建空文件
        with open(filename, "w", newline="", encoding="utf-8") as f:
            pass
        return

    try:
        headers = list(results[0].data.keys())
        with open(filename, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(headers)
            for row in results:
                values = [row.data.get(h) for h in headers]
                writer.writerow(values)

        # 仅在系统层面提示，不干扰主输出流
        # print(f"   [System] Result saved to '{filename}'")
        # (注：根据需要决定是否保留这行 print，或者保留在 main 中提示)

    except Exception as e:
        print(f"   [System] Failed to save csv: {e}")


def handle_meta_command(cmd: str, executor: Executor):
    """处理系统元命令 (.tables, .opt, .create_index)"""
    if cmd == ".tables":
        print("Available Tables:")
        if os.path.exists(DATA_DIR):
            files = [f for f in os.listdir(DATA_DIR) if f.endswith(".csv")]
            for f in files:
                print(f"  - {f}")
        else:
            print(f"  Directory '{DATA_DIR}' not found.")

    elif cmd.startswith(".create_index"):
        parts = cmd.split()
        if len(parts) != 3:
            print("Usage: .create_index <table> <column>")
            return
        table, col = parts[1], parts[2]
        executor.create_index(table, col)

    elif cmd.startswith(".opt"):
        parts = cmd.split()
        if len(parts) != 2 or parts[1].lower() not in ["on", "off"]:
            print("Usage: .opt [on|off]")
            return
        should_enable = parts[1].lower() == "on"
        executor.set_optimization(should_enable)

    else:
        print(f"Unknown meta command: {cmd}")


def main():
    executor = Executor(data_dir=DATA_DIR)
    print_welcome()

    input_buffer = []
    while True:
        try:
            # 获取用户输入
            # flush=True 确保提示符立即显示
            if not input_buffer:
                prompt = "simpledb> "
            else:
                prompt = "   ...> "  # 提示用户正在多行输入

            print(prompt, end="", flush=True)

            line = sys.stdin.readline()

            # 处理空输入
            if not line:
                break

            line = line.strip()

            if not input_buffer:
                if not line:
                    continue

                # 处理退出命令
                if line.lower() in ["exit", "quit"]:
                    print("Bye!")
                    break

                # 处理元命令 (如 .tables)
                if line.startswith("."):
                    handle_meta_command(line, executor)
                    continue

                # 处理清屏
                if line.lower() in ["clear", "cls"]:
                    os.system("cls" if os.name == "nt" else "clear")
                    continue
            if line:
                input_buffer.append(line)

            full_sql_check = " ".join(input_buffer).strip()

            if full_sql_check.endswith(";"):
                final_sql = full_sql_check.rstrip(";")
                # 3. 执行 SQL (计时)
                start_time = time.time()
                results = executor.execute(final_sql)
                duration = time.time() - start_time

                # --- Debug Print ---
                # 教学提示：请观察这里的 AST 结构，这能帮你完成 optimizer.py 的任务！
                print("\n--- [Debug] AST (Abstract Syntax Tree) ---")
                if executor.last_ast:
                    # 使用 print_ast 美化输出字典
                    print_ast(executor.last_ast)
                print("------------------------------------------\n")
                print("\n------------- Execution Plan -------------")
                if executor.last_plan:
                    print_plan(executor.last_plan)
                else:
                    print("(No execution plan generated)")
                print("---------------------------------------------\n")
                # 4. 打印结果
                save_to_csv(results, "result.csv")
                MAX_PRINT_ROWS = 30
                if len(results) > MAX_PRINT_ROWS:
                    print(f"\n[Note] Result too large ({len(results)} rows). Showing first {MAX_PRINT_ROWS} rows only.")
                    print("[Note] Check 'result.csv' for full output.\n")
                    print_table(results[:MAX_PRINT_ROWS])  # 只传切片
                    print("...")
                else:
                    # 结果少，正常打印
                    print_table(results)
                print(f"\n({len(results)} rows)")
                print(f"Time: {duration:.4f}s")
                print()
                input_buffer = []

        except KeyboardInterrupt:
            print("\nUse 'exit' to quit.")
            continue
        except Exception as e:
            # 捕获并显示运行时错误 (e.g. SQL Syntax Error, Missing Table)
            print(f"Error: {e}")
            print()


if __name__ == "__main__":
    main()
