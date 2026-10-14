module ALU(
    output reg [31:0] F,    // 输出结果
    output ZF, CF, OF, SF, PF,  // 所有标志位
    input [31:0] A, B,     // 运算数
    input [3:0] OP         // 运算选择
);

    parameter SIZE = 32;  // 运算位数
    
    // 功能定义
    parameter ALU_AND = 4'b0000;  // 按位与
    parameter ALU_OR  = 4'b0001;  // 按位或
    parameter ALU_XOR = 4'b0010;  // 按位异或
    parameter ALU_NOR = 4'b0011;  // 按位或非
    parameter ALU_ADD = 4'b0100;  // 算术加法
    parameter ALU_SUB = 4'b0101;  // 算术减法
    parameter ALU_SLT = 4'b0110;  // 比较，若A<B，则输出1;否则输出0
    parameter ALU_SLL = 4'b0111;  // 逻辑左移，B逻辑左移A所指定的位数
    
    // 中间变量
    wire [7:0] EN;           // 运算使能信号
    wire [31:0] Fw, Fa;      // 线网输出F
    wire CF_temp;            // 临时进位标志
    wire C;                  // 最高位进位
    
    // 数据流描述
    assign Fa = A & B;  // 按位与
    
    // 行为描述
    always@(*) begin
        case(OP)
            ALU_AND: F <= Fa;        // 按位与
            ALU_OR : F <= A | B;     // 按位或
            ALU_XOR: F <= A ^ B;     // 按位异或
            ALU_NOR: F <= ~(A | B);  // 按位或非
            default: F <= Fw;        // 其他操作使用模块输出
        endcase
    end
    
    // 结构化描述
    // 3-8译码器，生成片选EN信号
	Decoder38 decoder38_1(OP[2:0], EN);
    
    // 算术加法
    ADD add_1(Fw, CF_temp, A, B, EN[4]);
    
    // 算术减法
    SUB sub_1(Fw, CF_temp, A, B, EN[5]);
    
    // 比较，若A<B，则输出1;否则输出0
    SLT slt_1(Fw, A, B, EN[6]);
    
    // 逻辑左移，B逻辑左移A所指定的位数
    SLL sll_1(Fw, A, B, EN[7]);
 
	assign C = (OP == ALU_ADD) ? CF_temp :
				(OP == ALU_SUB) ? ~CF_temp : 1'b0;
    // 标志位计算（从第一个代码移植）
    assign ZF = (F == 0);                            // 零标志位
    assign CF = (OP == ALU_ADD || OP == ALU_SUB) ? CF_temp : 1'b0;  // 进借位标志位
    assign OF = A[31] ^ B[31] ^ F[31] ^ CF;           // 溢出标志位
    assign SF = F[31];                               // 符号标志位
    assign PF = ~^F;                                  // 奇偶标志位
endmodule
