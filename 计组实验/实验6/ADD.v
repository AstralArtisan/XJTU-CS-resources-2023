module ADD(
    output reg [31:0] F,
    output reg CF,
    input [31:0] A, B,
    input EN
);
    always@(*) begin
        if(!EN) begin
            {CF, F} = A + B;
        end else begin
            F = 32'bz;
            CF = 1'bz;
        end
    end
endmodule
