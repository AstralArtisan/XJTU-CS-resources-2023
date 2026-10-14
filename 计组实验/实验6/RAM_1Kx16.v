module RAM_1Kx16(Data_out, Addr, Rst, RE, WE, CS, CLK, Data_in);
    parameter Addr_Width = 10;  //参数化地址线宽
    parameter Data_Width = 16;  //参数化数据线宽
    parameter SIZE = 2 ** Addr_Width;  //参数化大小1024
    output reg [Data_Width-1:0] Data_out;//数据输出
    input [Addr_Width-1:0] Addr;         //地址数据
    input Rst; //复位信号
    input RE;  //读使能信号
    input WE;  //写使能信号
    input CS;  //使能/片选信号
    input CLK; //时钟信号
    input [Data_Width-1:0] Data_in;  //数据输入

    integer i;//临时变量，用于for循环
    reg [Data_Width-1:0] RAM [SIZE-1:0]; //1024*16的RAM

    //always @(posedge CLK) begin //时钟同步
    always @(*) begin  //异步
        casex({CS, Rst, RE, WE})
            4'bx1xx : for(i = 0;i <= SIZE-1;i = i+1) RAM[i] = 0; 
            4'b1010 : Data_out <= RAM[Addr];  //读数据
            4'b1001 : RAM[Addr] <= Data_in;   //写数据
            default : Data_out = 16'bz;
        endcase
    end
endmodule
