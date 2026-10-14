module SLL(
    output reg [31:0] F,
    input [31:0] A, B,
    input EN
);
    always@(*) begin
        if(!EN) begin
            F = B << A[4:0];  // 只使用A的低5位（最多移31位）
        end else begin
            F = 32'bz;
        end
    end
endmodule
