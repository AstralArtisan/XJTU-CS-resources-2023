module MainDec (
    input [5:0] Op,
    output MemtoReg,
    output MemWrite,
    output Branch,
    output ALUSrc,
    output RegDst,
    output RegWrite,
    output Jump,
    output [1:0] ALUOp
);
    reg [8:0] Controls;

    assign {RegWrite, RegDst, ALUSrc, Branch, MemWrite, MemtoReg, Jump, ALUOp} = Controls;

    always @(*) begin
        case (Op)
            6'b000000: Controls <= 9'b110000010; // RTYPE
            6'b100011: Controls <= 9'b101001000; // LW
            6'b101011: Controls <= 9'b001010000; // SW
            6'b000100: Controls <= 9'b000100001; // BEQ
            6'b001000: Controls <= 9'b101000000; // ADDI
            6'b001101: Controls <= 9'b101000011; // ORI
            6'b000010: Controls <= 9'b000000100; // J
            default:   Controls <= 9'bxxxxxxxxx; // illegal Op
        endcase
    end
endmodule
