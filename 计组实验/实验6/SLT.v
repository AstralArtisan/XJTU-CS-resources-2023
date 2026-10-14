module SLT(
    output reg [31:0] F,
    input [31:0] A, B,
    input EN
);
    always@(*) begin
        if(!EN) begin
            F = (A < B) ? 32'b1 : 32'b0;
        end else begin
            F = 32'bz;
        end
    end
endmodule
