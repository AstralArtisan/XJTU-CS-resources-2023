# 鲲鹏 ARM64 排序功能验证脚本（GNU gdb 批处理）。
#
# 生成的可执行文件把全局数组 a 在 .data 段清零后调用 qsort，自身退出码恒为 0，
# 不足以证明排序正确。本脚本在 _start 入口把 a 预填为一组乱序整数，在 qsort
# 返回点读回内存，比对是否升序。
#
# 用法：gdb -batch -x tests/verify_sort.gdb ./qsort
set pagination off
python
import gdb, re
gdb.execute("break _start")
gdb.execute("run")
vals = [7, 3, 9, 1, 5, 8, 2, 6, 4, 0]
addr = int(gdb.parse_and_eval("(long)&a"))
for k, v in enumerate(vals):
    gdb.execute("set {long}(%d) = %d" % (addr + 8 * k, v))
before = [int(gdb.parse_and_eval("*(long*)(%d)" % (addr + 8 * k))) for k in range(10)]
print("BEFORE: " + str(before))
# qsort 是递归函数，不能直接断在 qsort 返回；改为断在 _start 里 bl qsort 的下一条指令。
dis = gdb.execute("disassemble _start", to_string=True)
bl_addr = None
for line in dis.splitlines():
    if "bl" in line and "qsort" in line:
        m = re.search(r"0x[0-9a-fA-F]+", line)
        if m:
            bl_addr = int(m.group(0), 16)
            break
if bl_addr is None:
    raise RuntimeError("could not locate 'bl qsort' in _start")
gdb.execute("break *0x%x" % (bl_addr + 4))
gdb.execute("continue")
after = [int(gdb.parse_and_eval("*(long*)(%d)" % (addr + 8 * k))) for k in range(10)]
print("AFTER:  " + str(after))
print("SORT_OK" if after == sorted(before) else "SORT_FAIL")
gdb.execute("continue")
end
