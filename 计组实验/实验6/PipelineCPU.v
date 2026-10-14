`timescale 1ns/1ps

module PipelineCPU(
    input wire clk,
    input wire rst
);
    // ============================================================
    // FI: Fetch Instruction (取指，PC+4 修正)
    // ============================================================
    wire        PCEn;
    wire [31:0] PC, PCNext, NPC1_FI;     // NPC1 = PC + 4
    wire [31:0] FI_IR;                   // FI stage instruction

    add32 u_add_npc1(.a(PC), .b(32'd4), .y(NPC1_FI));

    ProgramCounterEn  u_pc(.clk(clk), .rst(rst), .en(PCEn), .next_pc(PCNext), .pc(PC));
    InstructionMemory u_im(.Address(PC), .Instruction(FI_IR));

    // ----------------------------
    // FI/ID 级间寄存器
    //   IR   : 指令寄存器
    //   NPC1 : 顺序下一条地址
    // ----------------------------
    wire FI_ID_En, Flush_FI_ID;
    reg  [31:0] FI_ID_IR;
    reg  [31:0] FI_ID_NPC1;

    always @(posedge clk) begin
        if (rst) begin
            FI_ID_IR   <= 32'b0;
            FI_ID_NPC1 <= 32'b0;
        end else if (FI_ID_En) begin
            if (Flush_FI_ID) begin
                FI_ID_IR   <= 32'b0;
                FI_ID_NPC1 <= 32'b0;
            end else begin
                FI_ID_IR   <= FI_IR;
                FI_ID_NPC1 <= NPC1_FI;
            end
        end
    end

    // ============================================================
    // ID: Instruction Decode (译码 + 读寄存器)
    // ============================================================
    wire [5:0] Op    = FI_ID_IR[31:26];
    wire [4:0] rs    = FI_ID_IR[25:21];
    wire [4:0] rt    = FI_ID_IR[20:16];
    wire [4:0] rd    = FI_ID_IR[15:11];
    wire [5:0] Funct = FI_ID_IR[5:0];
    wire [15:0] Imm16 = FI_ID_IR[15:0];
    wire [25:0] JIdx  = FI_ID_IR[25:0];

    // RF 读出
    wire [31:0] RF_Rs, RF_Rt;

    // WB 写回信号
    wire        WB_WE;
    wire [4:0]  WB_WA;
    wire [31:0] WB_WD;

    RegFile u_rf(
        .CLK (clk),
        .WE3 (WB_WE),
        .RA1 (rs),
        .RA2 (rt),
        .WA3 (WB_WA),
        .WD3 (WB_WD),
        .RD1 (RF_Rs),
        .RD2 (RF_Rt)
    );

    // WB->ID Forwarding (解决 WB 写回与 ID 读取在同一拍的 RAW 冒险)
    wire [31:0] RF_Rs_Fwd = (WB_WE && (WB_WA != 0) && (WB_WA == rs)) ? WB_WD : RF_Rs;
    wire [31:0] RF_Rt_Fwd = (WB_WE && (WB_WA != 0) && (WB_WA == rt)) ? WB_WD : RF_Rt;

    // 控制信号
    wire CU_RegWrite, CU_RegDst, CU_ALUSrc, CU_MemWrite, CU_MemtoReg;
    wire CU_Branch, CU_Jump;
    wire [1:0] CU_ALUOp;

    ControlUnit u_cu(
        .Opcode   (Op),
        .RegDst   (CU_RegDst),
        .ALUSrc   (CU_ALUSrc),
        .MemtoReg (CU_MemtoReg),
        .RegWrite (CU_RegWrite),
        .MemRead  (),             // unused
        .MemWrite (CU_MemWrite),
        .Branch   (CU_Branch),
        .Jump     (CU_Jump),
        .ALUOp    (CU_ALUOp)
    );

    // Imm32：16 -> 32 扩展（ORI/ANDI 用零扩展，其余符号扩展）
    wire ImmZero_ID = (Op == 6'b001101) /*ORI*/ || (Op == 6'b001100) /*ANDI*/;
    wire [31:0] Imm32_ID_sign, Imm32_ID_zero, Imm32_ID;
    signext u_se (.in(Imm16), .out(Imm32_ID_sign));
    zeroext u_ze(.in(Imm16), .out(Imm32_ID_zero));
    assign Imm32_ID = ImmZero_ID ? Imm32_ID_zero : Imm32_ID_sign;

    // NPC3：跳转目标地址
    wire [27:0] JIdx_sl2;
    shl2_26 u_jsh(.in(JIdx), .out(JIdx_sl2));
    wire [31:0] NPC3_ID = {FI_ID_NPC1[31:28], JIdx_sl2};

    // ----------------------------
    // ID/EX 级间寄存器
    //   IR, NPC1, Rs, Rt, Imm32
    // ----------------------------
    wire Flush_ID_EX;
    reg  [31:0] ID_EX_IR;
    reg  [31:0] ID_EX_NPC1;
    reg  [31:0] ID_EX_Rs;
    reg  [31:0] ID_EX_Rt;
    reg  [31:0] ID_EX_Imm32;
    reg  [31:0] ID_EX_NPC3; 

    reg         ID_EX_RegWrite, ID_EX_RegDst, ID_EX_ALUSrc, ID_EX_MemWrite, ID_EX_MemtoReg;
    reg         ID_EX_Branch, ID_EX_Jump;
    reg  [1:0]  ID_EX_ALUOp;

    always @(posedge clk) begin
        if (rst) begin
            ID_EX_IR      <= 0;
            ID_EX_NPC1    <= 0;
            ID_EX_Rs      <= 0;
            ID_EX_Rt      <= 0;
            ID_EX_Imm32   <= 0;
            ID_EX_NPC3    <= 0;

            ID_EX_RegWrite<= 0; ID_EX_RegDst<=0; ID_EX_ALUSrc<=0; ID_EX_MemWrite<=0; ID_EX_MemtoReg<=0;
            ID_EX_Branch  <= 0; ID_EX_Jump  <= 0; ID_EX_ALUOp <= 0;
        end else begin
            if (Flush_ID_EX) begin
                // bubble：控制清零，数据清零
                ID_EX_RegWrite<=0; ID_EX_MemWrite<=0; ID_EX_MemtoReg<=0;
                ID_EX_Branch  <=0; ID_EX_Jump    <=0; ID_EX_RegDst  <=0; ID_EX_ALUSrc<=0; ID_EX_ALUOp<=2'b00;

                ID_EX_IR    <=0; ID_EX_NPC1<=0; ID_EX_Rs<=0; ID_EX_Rt<=0; ID_EX_Imm32<=0; ID_EX_NPC3<=0;
            end else begin
                ID_EX_IR    <= FI_ID_IR;
                ID_EX_NPC1  <= FI_ID_NPC1;
                ID_EX_Rs    <= RF_Rs_Fwd;
                ID_EX_Rt    <= RF_Rt_Fwd;
                ID_EX_Imm32 <= Imm32_ID;
                ID_EX_NPC3  <= NPC3_ID;

                ID_EX_RegWrite<= CU_RegWrite;
                ID_EX_RegDst  <= CU_RegDst;
                ID_EX_ALUSrc  <= CU_ALUSrc;
                ID_EX_MemWrite<= CU_MemWrite;
                ID_EX_MemtoReg<= CU_MemtoReg;
                ID_EX_Branch  <= CU_Branch;
                ID_EX_Jump    <= CU_Jump;
                ID_EX_ALUOp   <= CU_ALUOp;
            end
        end
    end

    // ============================================================
    // EX: Execute (ALU 运算 + 分支目标 NPC2 计算)
    // ============================================================
    wire [5:0] EX_Op   = ID_EX_IR[31:26];
    wire [4:0] EX_rs   = ID_EX_IR[25:21];
    wire [4:0] EX_rt   = ID_EX_IR[20:16];
    wire [4:0] EX_rd   = ID_EX_IR[15:11];
    wire [5:0] EX_Funct= ID_EX_IR[5:0];

    wire [3:0] EX_ALUCtrl;

    ALUControlUnit u_aluctrl(
        .ALUOp     (ID_EX_ALUOp),
        .Funct     (EX_Funct),
        .ALUControl(EX_ALUCtrl)
    );

    // ID 阶段已完成扩展，这里直接使用流水化的 Imm32
    wire [31:0] Imm32_EX = ID_EX_Imm32;

    // forwarding
    wire [1:0] FwdA, FwdB;
    wire        EX_MA_RegWrite, MA_WB_RegWrite;
    wire [4:0]  EX_MA_WA, MA_WB_WA;

    ForwardingUnitPipe u_fwd(
        .EX_MA_RegWrite(EX_MA_RegWrite),
        .MA_WB_RegWrite(MA_WB_RegWrite),
        .EX_MA_WA      (EX_MA_WA),
        .MA_WB_WA      (MA_WB_WA),
        .ID_EX_Rs      (EX_rs),
        .ID_EX_Rt      (EX_rt),
        .FwdA          (FwdA),
        .FwdB          (FwdB)
    );

    wire [31:0] EX_MA_ALUOut;   
    wire [31:0] MA_WB_Value;    

    wire [31:0] EX_A = (FwdA==2'b10) ? EX_MA_ALUOut :
                       (FwdA==2'b01) ? MA_WB_Value  :
                                       ID_EX_Rs;

    wire [31:0] EX_Rt_Fwd = (FwdB==2'b10) ? EX_MA_ALUOut :
                            (FwdB==2'b01) ? MA_WB_Value  :
                                            ID_EX_Rt;

    wire [31:0] EX_B = ID_EX_ALUSrc ? Imm32_EX : EX_Rt_Fwd;

    wire [31:0] ALUOut_EX;
    wire EX_Zero;
    ALUWrapper u_alu(.ALUControl(EX_ALUCtrl), .A(EX_A), .B(EX_B), .Result(ALUOut_EX), .Zero(EX_Zero));

    // Branch 判定提前到 EX 级
    wire BranchTaken_EX = (ID_EX_Branch && EX_Zero);

    // NPC2 = NPC1 + (Imm32<<2)
    wire [31:0] Imm32_sl2_EX;
    shl2_32 u_sh2(.in(Imm32_EX), .out(Imm32_sl2_EX));
    wire [31:0] NPC2_EX;
    add32 u_add_npc2(.a(ID_EX_NPC1), .b(Imm32_sl2_EX), .y(NPC2_EX));

    // 写回目的寄存器号
    wire [4:0] WA_EX = ID_EX_RegDst ? EX_rd : EX_rt;

    // ============================================================
    // EX/MA 级间寄存器
    //   IR, Rt, NPC2, NPC3, ALUOut
    //   （控制信号流水化）
    // ============================================================
    reg  [31:0] EX_MA_IR;
    reg  [31:0] EX_MA_Rt;        // store 写数据
    reg  [31:0] EX_MA_NPC2;
    reg  [31:0] EX_MA_NPC3;
    reg  [31:0] EX_MA_ALUOut_r;

    reg         EX_MA_MemWrite, EX_MA_MemtoReg, EX_MA_Branch, EX_MA_Jump, EX_MA_RegWrite_r;
    reg         EX_MA_Zero;
    reg  [4:0]  EX_MA_WA_r;
    reg  [4:0]  EX_MA_rtnum;     // store-forward 需要 rt 号

    always @(posedge clk) begin
        if (rst) begin
            EX_MA_IR<=0; EX_MA_Rt<=0; EX_MA_NPC2<=0; EX_MA_NPC3<=0; EX_MA_ALUOut_r<=0;
            EX_MA_MemWrite<=0; EX_MA_MemtoReg<=0; EX_MA_Branch<=0; EX_MA_Jump<=0; EX_MA_RegWrite_r<=0;
            EX_MA_Zero<=0; EX_MA_WA_r<=0; EX_MA_rtnum<=0;
        end else begin
            EX_MA_IR      <= ID_EX_IR;
            EX_MA_Rt      <= EX_Rt_Fwd;      // store data before ALUSrc
            EX_MA_NPC2    <= NPC2_EX;
            EX_MA_NPC3    <= ID_EX_NPC3;
            EX_MA_ALUOut_r<= ALUOut_EX;

            EX_MA_MemWrite<= ID_EX_MemWrite;
            EX_MA_MemtoReg<= ID_EX_MemtoReg;
            EX_MA_Branch  <= ID_EX_Branch;
            EX_MA_Jump    <= ID_EX_Jump;
            EX_MA_RegWrite_r <= ID_EX_RegWrite;
            EX_MA_Zero    <= EX_Zero;
            EX_MA_WA_r    <= WA_EX;
            EX_MA_rtnum   <= EX_rt;
        end
    end

    assign EX_MA_RegWrite = EX_MA_RegWrite_r;
    assign EX_MA_WA       = EX_MA_WA_r;
    assign EX_MA_ALUOut   = EX_MA_ALUOut_r;

    // store-data forwarding
    wire StoreFromWB;
    StoreForwardUnit u_sfw(
        .MA_WB_RegWrite(MA_WB_RegWrite),
        .MA_WB_WA      (MA_WB_WA),
        .EX_MA_Rt      (EX_MA_rtnum),
        .Sel_MA_WB     (StoreFromWB)
    );
    wire [31:0] MA_StoreWD = StoreFromWB ? MA_WB_Value : EX_MA_Rt;

    // ============================================================
    // MA: Memory Access (数据存储器读写)
    // ============================================================
    wire [31:0] MEMOut_MA;
    DataMemory u_dm(
        .clk      (clk),
        .MemRead  (EX_MA_MemtoReg),
        .MemWrite (EX_MA_MemWrite),
        .Address  (EX_MA_ALUOut_r),
        .WriteData(MA_StoreWD),
        .ReadData (MEMOut_MA)
    );

    // 控制冒险：Jump 在 ID 级生效（PCNext 直接使用 CU_Jump/NPC3_ID）

    // ============================================================
    // MA/WB 级间寄存器
    //   IR, ALUOut, MEMOut
    // ============================================================
    reg  [31:0] MA_WB_IR;
    reg  [31:0] MA_WB_ALUOut;
    reg  [31:0] MA_WB_MEMOut;
    reg         MA_WB_MemtoReg, MA_WB_RegWrite_r;
    reg  [4:0]  MA_WB_WA_r;

    always @(posedge clk) begin
        if (rst) begin
            MA_WB_IR<=0; MA_WB_ALUOut<=0; MA_WB_MEMOut<=0;
            MA_WB_MemtoReg<=0; MA_WB_RegWrite_r<=0; MA_WB_WA_r<=0;
        end else begin
            MA_WB_IR      <= EX_MA_IR;
            MA_WB_ALUOut  <= EX_MA_ALUOut_r;
            MA_WB_MEMOut  <= MEMOut_MA;
            MA_WB_MemtoReg<= EX_MA_MemtoReg;
            MA_WB_RegWrite_r <= EX_MA_RegWrite_r;
            MA_WB_WA_r    <= EX_MA_WA_r;
        end
    end

    assign MA_WB_RegWrite = MA_WB_RegWrite_r;
    assign MA_WB_WA       = MA_WB_WA_r;

    // ============================================================
    // WB: Write Back (写回 RF)
    // ============================================================
    wire [31:0] WB_ALUOut = MA_WB_ALUOut;
    wire [31:0] WB_MEMOut = MA_WB_MEMOut;
    wire [31:0] WB_Value  = MA_WB_MemtoReg ? WB_MEMOut : WB_ALUOut;

    assign MA_WB_Value = WB_Value;

    assign WB_WE = MA_WB_RegWrite_r;
    assign WB_WA = MA_WB_WA_r;
    assign WB_WD = WB_Value;

    // ============================================================
    // Hazard + PCNext 选择
    // ============================================================
    HazardUnit u_hzd(
        .ID_EX_MemtoReg(ID_EX_MemtoReg),
        .ID_EX_Rt      (ID_EX_IR[20:16]),
        .FI_ID_Rs      (rs),
        .FI_ID_Rt      (rt),
        .BranchTaken   (BranchTaken_EX),
        .ID_Jump       (CU_Jump),
        .PCEn          (PCEn),
        .FI_ID_En      (FI_ID_En),
        .Flush_FI_ID   (Flush_FI_ID),
        .Flush_ID_EX   (Flush_ID_EX)
    );

    // next PC: J > taken-branch > pc+4
    assign PCNext =
        CU_Jump        ? NPC3_ID :
        BranchTaken_EX ? NPC2_EX :
                         NPC1_FI;

endmodule
