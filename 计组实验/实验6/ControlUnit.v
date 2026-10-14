module ControlUnit(
    input  [5:0] Opcode,
    output RegDst,
    output ALUSrc,
    output MemtoReg,
    output RegWrite,
    output MemRead,
    output MemWrite,
    output Branch,
    output Jump,
    output [1:0] ALUOp
);
    MainDec u_md(
        .Op      (Opcode),
        .MemtoReg(MemtoReg),
        .MemWrite(MemWrite),
        .Branch  (Branch),
        .ALUSrc  (ALUSrc),
        .RegDst  (RegDst),
        .RegWrite(RegWrite),
        .Jump    (Jump),
        .ALUOp   (ALUOp)
    );

    // 在只实现 lw/sw 的情况下：读内存 <=> MemtoReg = 1
    assign MemRead = MemtoReg;
endmodule
