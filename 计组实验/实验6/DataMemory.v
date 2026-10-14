module DataMemory(
    input        clk,
    input        MemRead,
    input        MemWrite,
    input  [31:0] Address,
    input  [31:0] WriteData,
    output [31:0] ReadData
);
    wire [11:0] word_addr = Address[13:2];  // 假设按字对齐

    RAM_4Kx32 u_ram(
        .Data_out(ReadData),
        .Addr    (word_addr),
        .Rst     (1'b0),
        .RE      (MemRead),
        .WE      (MemWrite),
        .CS      (MemRead | MemWrite),
        .CLK     (clk),
        .Data_in (WriteData)
    );
endmodule
