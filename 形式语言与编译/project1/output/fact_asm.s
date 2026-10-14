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
