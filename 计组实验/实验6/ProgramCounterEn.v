// ProgramCounter with enable
module ProgramCounterEn(
    input  wire        clk,
    input  wire        rst,
    input  wire        en,
    input  wire [31:0] next_pc,
    output wire [31:0] pc
);
    wire [31:0] pc_out;
    wire [31:0] pc_in = en ? next_pc : pc_out;

    ProgramCounter u_pc(
        .clk    (clk),
        .reset  (rst),
        .PC_in  (pc_in),
        .PC_out (pc_out)
    );

    assign pc = pc_out;
endmodule
