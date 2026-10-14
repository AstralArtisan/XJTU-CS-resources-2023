"""
运行全部测试并生成 HTML 测试报告。

用法（在项目根目录执行）：
    python run_tests.py
"""
import subprocess
import sys
import os

ROOT = os.path.dirname(__file__)
SRC = os.path.join(ROOT, "src")
REPORT = os.path.join(ROOT, "docs", "reports", "测试报告.html")


def main():
    print("=" * 52)
    print("  西交食堂评价系统 — 自动化测试")
    print("=" * 52)

    # 检查 pytest-html 是否安装
    try:
        import pytest_html  # noqa
    except ImportError:
        print("[提示] 正在安装 pytest-html...")
        subprocess.run([sys.executable, "-m", "pip", "install", "pytest-html", "-q"], check=True)

    cmd = [
        sys.executable, "-m", "pytest",
        "src/backend/tests/",
        "--html", REPORT,
        "--self-contained-html",
        "-v",
        "--tb=short",
        "--no-header",
    ]

    env = os.environ.copy()
    env["PYTHONPATH"] = SRC

    result = subprocess.run(cmd, cwd=ROOT, env=env)

    print()
    if result.returncode == 0:
        print(f"✓ 全部测试通过！报告已生成：{REPORT}")
    else:
        print(f"✗ 存在失败用例，请查看报告：{REPORT}")

    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
