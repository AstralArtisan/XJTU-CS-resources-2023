module MultiCycleCPU (
    input clk,
    input reset
);

    // 状态编码
    parameter FETCH      = 3'b000;
    parameter DECODE     = 3'b001;
    parameter EXECUTE    = 3'b010;
    parameter MEM_ACCESS = 3'b011;
    parameter WRITE_BACK = 3'b100;

    reg [2:0] state, next_state;

    // PC 与指令通路
    reg  [31:0] pc_next;
    wire [31:0] PC_out;
    wire [31:0] instruction;
    reg  [31:0] IR;

    // 数据通路信号
    wire [31:0] read_data1, read_data2;
    wire [31:0] alu_result, mem_read_data;
    wire [31:0] sign_ext_imm;
    wire [31:0] alu_input_b;
    wire [31:0] write_back_data;
    wire [4:0]  write_reg;
    wire [3:0]  alu_control;
    wire        zero;
    reg  [31:0] mdr; // Memory Data Register for lw

    // 控制信号（未分阶段）
    wire regdst_raw, alusrc_raw, memtoreg_raw;
    wire regwrite_raw, memread_raw, memwrite_raw, branch_raw, jump_raw;
    wire [1:0] aluop_raw;

    // 各阶段使能
    wire regwrite_en;
    wire memread_en;
    wire memwrite_en;

    ProgramCounter PC (
        .clk    (clk),
        .reset  (reset),
        .PC_in  (pc_next),
        .PC_out (PC_out)
    );

    InstructionMemory IM (
        .Address    (PC_out),
        .Instruction(instruction)
    );

    RegFile RF(
        .CLK (clk),
        .WE3 (regwrite_en),
        .RA1 (IR[25:21]),
        .RA2 (IR[20:16]),
        .WA3 (write_reg),
        .WD3 (write_back_data),
        .RD1 (read_data1),
        .RD2 (read_data2)
    );

    DataMemory DM (
        .clk      (clk),
        .MemRead  (memread_en),
        .MemWrite (memwrite_en),
        .Address  (alu_result),
        .WriteData(read_data2),
        .ReadData (mem_read_data)
    );

    ControlUnit CU(
        .Opcode   (IR[31:26]),
        .RegDst   (regdst_raw),
        .ALUSrc   (alusrc_raw),
        .MemtoReg (memtoreg_raw),
        .RegWrite (regwrite_raw),
        .MemRead  (memread_raw),
        .MemWrite (memwrite_raw),
        .Branch   (branch_raw),
        .Jump     (jump_raw),
        .ALUOp    (aluop_raw)
    );

    ALUControlUnit ALUCtrl(
        .ALUOp     (aluop_raw),
        .Funct     (IR[5:0]),
        .ALUControl(alu_control)
    );

    assign sign_ext_imm    = (IR[31:26] == 6'b001101) ?
                              {16'b0, IR[15:0]} :
                              {{16{IR[15]}}, IR[15:0]};
    assign alu_input_b     = alusrc_raw ? sign_ext_imm : read_data2;
    assign write_reg       = regdst_raw ? IR[15:11] : IR[20:16];
    assign write_back_data = memtoreg_raw ? mdr : alu_result;

    assign regwrite_en = (state == WRITE_BACK) ? regwrite_raw : 1'b0;
    assign memread_en  = (state == MEM_ACCESS) ? memread_raw  : 1'b0;
    assign memwrite_en = (state == MEM_ACCESS) ? memwrite_raw : 1'b0;

    ALUWrapper alu (
        .A          (read_data1),
        .B          (alu_input_b),
        .ALUControl (alu_control),
        .Result     (alu_result),
        .Zero       (zero)
    );

    // PC更新逻辑：取指阶段自增，执行阶段处理跳转/分支
    always @(*) begin
        pc_next = PC_out;
        case (state)
            FETCH: begin
                pc_next = PC_out + 4;
            end
            EXECUTE: begin
                if (jump_raw) begin
                    pc_next = {PC_out[31:28], IR[25:0], 2'b00};
                end else if (branch_raw && zero) begin
                    pc_next = PC_out + (sign_ext_imm << 2);
                end
            end
            default: begin
                pc_next = PC_out;
            end
        endcase
    end

    // 状态与IR寄存
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            state <= FETCH;
            IR    <= 32'b0;
            mdr   <= 32'b0;
        end else begin
            state <= next_state;
            if (state == FETCH) begin
                IR <= instruction;
            end
            if (state == MEM_ACCESS && memread_en) begin
                mdr <= mem_read_data; // latch data for lw
            end
        end
    end

    // 下一状态逻辑
    always @(*) begin
        case (state)
            FETCH:  next_state = DECODE;
            DECODE: next_state = EXECUTE;
            EXECUTE: begin
                case (IR[31:26])
                    6'b100011,
                    6'b101011: next_state = MEM_ACCESS; // LW, SW
                    6'b000100: next_state = FETCH;      // BEQ
                    6'b001101: next_state = WRITE_BACK; // ORI
                    6'b001000: next_state = WRITE_BACK; // ADDI
                    6'b000000: next_state = WRITE_BACK; // R-type
                    6'b000010: next_state = FETCH;      // J
                    default:   next_state = FETCH;
                endcase
            end
            MEM_ACCESS: begin
                if (IR[31:26] == 6'b100011) begin
                    next_state = WRITE_BACK; // LW
                end else begin
                    next_state = FETCH;  // SW
                end
            end
            WRITE_BACK: next_state = FETCH;
            default:    next_state = FETCH;
        endcase
    end
endmodule
