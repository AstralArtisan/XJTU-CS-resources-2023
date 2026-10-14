module RAM_4Kx32(
    output [31:0] Data_out,      // 32位数据输出
    input [11:0] Addr,           // 12位地址线
    input Rst,                   // 复位信号
    input RE,                    // 读使能
    input WE,                    // 写使能
    input CS,                    //片选
    input CLK,                   // 时钟
    input [31:0] Data_in         // 32位数据输入
);

    parameter Addr_Width = 12;   // 参数化地址线宽
    parameter Data_Width = 32;   // 参数化数据线宽
    
    wire [3:0] CS_i;             // 4位片选信号组
    
    // 2-4译码器：用地址最高2位生成4个片选信号
    Decoder24 decoder_inst(CS_i, Addr[Addr_Width-1:Addr_Width-2]);
    
    // 4个RAM_1Kx16模块，每个分为高16位和低16位两部分
    // CS0组：地址范围 0x000-0x3FF
    RAM_1Kx16 CS0_H_16bit(
        .Data_out(Data_out[31:16]), 
        .Addr(Addr[9:0]), 
        .Rst(Rst), .RE(RE), .WE(WE), .CS(CS_i[0]), .CLK(CLK), 
        .Data_in(Data_in[31:16])
    );
    RAM_1Kx16 CS0_L_16bit(
        .Data_out(Data_out[15:0]), 
        .Addr(Addr[9:0]), 
        .Rst(Rst), .RE(RE), .WE(WE), .CS(CS_i[0]), .CLK(CLK), 
        .Data_in(Data_in[15:0])
    );
    
    // CS1组：地址范围 0x400-0x7FF
    RAM_1Kx16 CS1_H_16bit(
        .Data_out(Data_out[31:16]), 
        .Addr(Addr[9:0]), 
        .Rst(Rst), .RE(RE), .WE(WE), .CS(CS_i[1]), .CLK(CLK), 
        .Data_in(Data_in[31:16])
    );
    RAM_1Kx16 CS1_L_16bit(
        .Data_out(Data_out[15:0]), 
        .Addr(Addr[9:0]), 
        .Rst(Rst), .RE(RE), .WE(WE), .CS(CS_i[1]), .CLK(CLK), 
        .Data_in(Data_in[15:0])
    );
    
    // CS2组：地址范围 0x800-0xBFF
    RAM_1Kx16 CS2_H_16bit(
        .Data_out(Data_out[31:16]), 
        .Addr(Addr[9:0]), 
        .Rst(Rst), .RE(RE), .WE(WE), .CS(CS_i[2]), .CLK(CLK), 
        .Data_in(Data_in[31:16])
    );
    RAM_1Kx16 CS2_L_16bit(
        .Data_out(Data_out[15:0]), 
        .Addr(Addr[9:0]), 
        .Rst(Rst), .RE(RE), .WE(WE), .CS(CS_i[2]), .CLK(CLK), 
        .Data_in(Data_in[15:0])
    );
    
    // CS3组：地址范围 0xC00-0xFFF
    RAM_1Kx16 CS3_H_16bit(
        .Data_out(Data_out[31:16]), 
        .Addr(Addr[9:0]), 
        .Rst(Rst), .RE(RE), .WE(WE), .CS(CS_i[3]), .CLK(CLK), 
        .Data_in(Data_in[31:16])
    );
    RAM_1Kx16 CS3_L_16bit(
        .Data_out(Data_out[15:0]), 
        .Addr(Addr[9:0]), 
        .Rst(Rst), .RE(RE), .WE(WE), .CS(CS_i[3]), .CLK(CLK), 
        .Data_in(Data_in[15:0])
    );
    
endmodule
