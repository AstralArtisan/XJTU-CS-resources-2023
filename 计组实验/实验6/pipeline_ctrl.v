// ----------------------------
// 数据前推：EX/MA 与 MA/WB -> EX
// ----------------------------
module ForwardingUnitPipe(
    input  wire        EX_MA_RegWrite,
    input  wire        MA_WB_RegWrite,
    input  wire [4:0]  EX_MA_WA,        // EX/MA 写回目的寄存器号
    input  wire [4:0]  MA_WB_WA,        // MA/WB 写回目的寄存器号
    input  wire [4:0]  ID_EX_Rs,        // ID/EX 指令的 rs 字段
    input  wire [4:0]  ID_EX_Rt,        // ID/EX 指令的 rt 字段
    output reg  [1:0]  FwdA,
    output reg  [1:0]  FwdB
);
    always @(*) begin
        FwdA = 2'b00;
        FwdB = 2'b00;

        if (EX_MA_RegWrite && (EX_MA_WA != 5'd0) && (EX_MA_WA == ID_EX_Rs))
            FwdA = 2'b10;
        else if (MA_WB_RegWrite && (MA_WB_WA != 5'd0) && (MA_WB_WA == ID_EX_Rs))
            FwdA = 2'b01;

        if (EX_MA_RegWrite && (EX_MA_WA != 5'd0) && (EX_MA_WA == ID_EX_Rt))
            FwdB = 2'b10;
        else if (MA_WB_RegWrite && (MA_WB_WA != 5'd0) && (MA_WB_WA == ID_EX_Rt))
            FwdB = 2'b01;
    end
endmodule

// ----------------------------
// store 数据前推：EX/MA 的 store 写数据来自 MA/WB 写回值
// ----------------------------
module StoreForwardUnit(
    input  wire        MA_WB_RegWrite,
    input  wire [4:0]  MA_WB_WA,     // MA/WB 写回目的寄存器号
    input  wire [4:0]  EX_MA_Rt,     // EX/MA 指令的 rt 字段 - store 用
    output wire        Sel_MA_WB
);
    assign Sel_MA_WB = MA_WB_RegWrite && (MA_WB_WA != 5'd0) && (MA_WB_WA == EX_MA_Rt);
endmodule

// ----------------------------
// 冒险检测：load-use + 控制冒险 flush
// ----------------------------
module HazardUnit(
    input  wire        ID_EX_MemtoReg,
    input  wire [4:0]  ID_EX_Rt,        // ID/EX 的 rt 字段（load 的目的寄存器号）
    input  wire [4:0]  FI_ID_Rs,
    input  wire [4:0]  FI_ID_Rt,
    input  wire        BranchTaken,
    input  wire        ID_Jump,          // ID 阶段检测到 jump（用于 flush 前段）
    output reg         PCEn,
    output reg         FI_ID_En,
    output reg         Flush_FI_ID,
    output reg         Flush_ID_EX
);
    wire LoadUse = ID_EX_MemtoReg && ((ID_EX_Rt == FI_ID_Rs) || (ID_EX_Rt == FI_ID_Rt));

    always @(*) begin
        PCEn       = 1'b1;
        FI_ID_En   = 1'b1;
        Flush_FI_ID= 1'b0;
        Flush_ID_EX= 1'b0;

        // load-use：stall PC 和 FI/ID，同时在 ID/EX 注入 bubble
        if (LoadUse) begin
            PCEn      = 1'b0;
            FI_ID_En  = 1'b0;
            Flush_ID_EX = 1'b1;
        end

        // 控制冒险：分支生效或 jump，flush 前两级
        if (BranchTaken) begin
            Flush_FI_ID = 1'b1;
            Flush_ID_EX = 1'b1;
        end

        if (ID_Jump) begin
            Flush_FI_ID = 1'b1;
            Flush_ID_EX = 1'b1;
        end
    end
endmodule
