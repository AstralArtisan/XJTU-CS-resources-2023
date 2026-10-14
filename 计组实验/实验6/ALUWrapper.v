module ALUWrapper(
    input  [31:0] A,
    input  [31:0] B,
    input  [3:0]  ALUControl,   // 直接接 ALUControl
    output [31:0] Result,
    output        Zero
);
    wire ZF, CF, OF, SF, PF;

    ALU u_alu(
        .F (Result),
        .ZF(ZF),
        .CF(CF),
        .OF(OF),
        .SF(SF),
        .PF(PF),
        .A (A),
        .B (B),
        .OP(ALUControl)
    );

    assign Zero = ZF;   // 分支比较用的 Zero
endmodule
