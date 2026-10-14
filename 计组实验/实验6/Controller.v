module Controller (
    input [5:0] Op,
    input [5:0] Funct,
    input Zero,
    output MemtoReg,
    output MemWrite,
    output PCSrc,
    output ALUSrc,
    output RegDst,
    output RegWrite,
    output Jump,
    output [2:0] ALUControl
);
    wire [1:0] ALUOp;
    wire       Branch;

    MainDec MainDec_1 (
        .Op       (Op),
        .MemtoReg (MemtoReg),
        .MemWrite (MemWrite),
        .Branch   (Branch),
        .ALUSrc   (ALUSrc),
        .RegDst   (RegDst),
        .RegWrite (RegWrite),
        .Jump     (Jump),
        .ALUOp    (ALUOp)
    );

    ALUDec ALUDec_1 (
        .Funct      (Funct),
        .ALUOp      (ALUOp),
        .ALUControl (ALUControl)
    );

    assign PCSrc = Branch & Zero;
endmodule
