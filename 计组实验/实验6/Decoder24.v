module Decoder24(
    output reg [3:0] CS_i,
    input  [1:0] Sel
);
    always @(*) begin
        case(Sel)
            2'b00: CS_i = 4'b0001;
            2'b01: CS_i = 4'b0010;
            2'b10: CS_i = 4'b0100;
            2'b11: CS_i = 4'b1000;
        endcase
    end
endmodule
