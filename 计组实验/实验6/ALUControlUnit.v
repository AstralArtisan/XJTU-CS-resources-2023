module ALUControlUnit(
    input  [1:0] ALUOp,
    input  [5:0] Funct,
    output [3:0] ALUControl
);
    wire [2:0] alu_ctl3;
    reg  [3:0] alu_ctl4;

    ALUDec u_ad(
        .Funct(Funct),
        .ALUOp(ALUOp),
        .ALUControl(alu_ctl3)
    );

    // Map 3-bit decoder output to the 4-bit ALU opcodes used by ALU.v
    always @(*) begin
        case (alu_ctl3)
            3'b000: alu_ctl4 = 4'b0000; // and
            3'b001: alu_ctl4 = 4'b0001; // or
            3'b010: alu_ctl4 = 4'b0100; // add
            3'b011: alu_ctl4 = 4'b0010; // xor
            3'b100: alu_ctl4 = 4'b0011; // nor
            3'b101: alu_ctl4 = 4'b0111; // sll
            3'b110: alu_ctl4 = 4'b0101; // sub
            3'b111: alu_ctl4 = 4'b0110; // slt
            default: alu_ctl4 = 4'b0000;
        endcase
    end

    assign ALUControl = alu_ctl4;
endmodule
