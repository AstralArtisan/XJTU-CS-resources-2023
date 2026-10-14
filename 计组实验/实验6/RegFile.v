`define DATA_WIDTH 32

module RegFile
    #(parameter ADDR_SIZE = 5)
    (input CLK, WE3,
     input [ADDR_SIZE-1:0] RA1, RA2, WA3,
     input [`DATA_WIDTH-1:0] WD3,
     output [`DATA_WIDTH-1:0] RD1, RD2);

    // 32×32 寄存器堆，初始清零
    reg [`DATA_WIDTH-1:0] rf[2 ** ADDR_SIZE-1:0];

    integer i;
    initial begin
        for (i = 0; i < (1 << ADDR_SIZE); i = i + 1)
            rf[i] = {`DATA_WIDTH{1'b0}};
    end

    // 数据写入，保持 $zero 不可写
    always @(posedge CLK)
        if (WE3 && (WA3 != 0)) rf[WA3] <= WD3;

    // 数据读出
    assign RD1 = (RA1 != 0) ? rf[RA1] : {`DATA_WIDTH{1'b0}};
    assign RD2 = (RA2 != 0) ? rf[RA2] : {`DATA_WIDTH{1'b0}};

endmodule
