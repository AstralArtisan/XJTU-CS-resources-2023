// 常用组合模块汇总

module mux2 #(parameter W=32)(input wire s, input wire [W-1:0] a,b, output wire [W-1:0] y);
    assign y = s ? b : a;
endmodule

module mux3 #(parameter W=32)(input wire [1:0] s, input wire [W-1:0] a,b,c, output wire [W-1:0] y);
    assign y = (s==2'b00)?a : (s==2'b01)?b : c;
endmodule

module signext(input wire [15:0] in, output wire [31:0] out);
    assign out = {{16{in[15]}}, in};
endmodule

module zeroext(input wire [15:0] in, output wire [31:0] out);
    assign out = {16'b0, in};
endmodule

module shl2_32(input wire [31:0] in, output wire [31:0] out);
    assign out = {in[29:0], 2'b00};
endmodule

module shl2_26(input wire [25:0] in, output wire [27:0] out);
    assign out = {in, 2'b00};
endmodule

module add32(input wire [31:0] a,b, output wire [31:0] y);
    assign y = a + b;
endmodule
