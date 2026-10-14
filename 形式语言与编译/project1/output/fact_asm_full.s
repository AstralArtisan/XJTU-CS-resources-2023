.global _start

.section .text

_start:
    MOV X10, #5         // n = 5
    MOV X11, #1         // a = 1
    BL my_func
    MOV X8, #93         // sys_exit
    SVC #0

my_func:
    // 代码生成器输出 (fact程序)
l0:
    MOV X9, #2;
    CMP X10, X9; B.GE l2; l1:
    MOV X0, X11; RET;
    B l3;
l2:
    SUB X12, X10, #1;
    MUL X13, X10, X11;
    MOV X10, X12;
    MOV X11, X13;
    B l0;
l3:
    B l3;
