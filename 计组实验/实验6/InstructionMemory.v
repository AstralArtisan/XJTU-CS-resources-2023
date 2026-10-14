module InstructionMemory(
    input  [31:0] Address,
    output [31:0] Instruction
);
    // 取word地址（按字对齐）
    wire [5:0] word_addr = Address[7:2];  // 例：64 条指令

    IMem u_imem(
        .A (word_addr),
        .RD(Instruction)
    );
endmodule
