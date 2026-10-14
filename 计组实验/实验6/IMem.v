module IMem(
    input  [5:0]  A,
    output [31:0] RD
);
    reg [31:0] rom [0:63];

    integer i;
    initial begin
        for (i = 0; i < 64; i = i + 1) begin
            rom[i] = 32'b0;
        end
        // 0. addi $t0,$zero,1
        // 1. addi $t1,$zero,2
        // 2. add $t2,$t0,$t1 // 需要对前两条 addi 的结果前递
        // 3. sub $t3,$t2,$t0 // 需要对 t2 前递
        // 4. and $t4,$t2,$t3
        // 5. or $t5,$t4,$t1
        // 6. slt $t6,$t0,$t1 // t6=1
        // 7. sw $t2,0($zero) // mem[0]=3
        // 8. add $t7,$t0,$t1 // t7=3
        // 9. sw $t7,4($zero) // 紧跟 store，测 store-data 前递 mem[1]=3
        // 10. lw $s0,4($zero) // s0=3
        // 11. add $s1,$s0,$t0 // 紧跟 use，测 load-use stall s1=4
        // 12. beq $s1,$zero,+2 // not taken，后面两条应执行
        // 13. addi $s2,$zero,0x1234 // s2=0x1234
        // 14. ori $s3,$zero,0xBEEF // 零扩展 s3=0x0000BEEF
        // 15. beq $s2,$s2,+1 // taken：跳过 16，仅执行 17
        // 16. addi $s4,$zero,0x7777 // 必须被 flush（不应执行）
        // 17. addi $s5,$zero,0x5678 // 分支目标：s5=0x5678
        // 18. j 20 // Jump 在 ID 生效：跳过 19
        // 19. addi $s4,$zero,0x9999 // 必须被 jump 跳过（不应执行）
        // 20. lw $s7,0($zero) // s7=mem[0]=3
        // 21. add $t8,$s7,$t1 // 紧跟 use，测第二处 load-use stall t8=5
        // 22. sw $t8,8($zero) // mem[2]=5
        // 23. lw $s6,8($zero) // s6=5（验证 mem[2]）
        // 24. j 24 // 自旋

        rom[0]  = 32'h20080001;
        rom[1]  = 32'h20090002;
        rom[2]  = 32'h01095020;
        rom[3]  = 32'h01485822;
        rom[4]  = 32'h014B6024;
        rom[5]  = 32'h01896825;
        rom[6]  = 32'h0109702A;
        rom[7]  = 32'hAC0A0000;
        rom[8]  = 32'h01097820;
        rom[9]  = 32'hAC0F0004;
        rom[10] = 32'h8C100004;
        rom[11] = 32'h02088820;
        rom[12] = 32'h12200002;
        rom[13] = 32'h20121234;
        rom[14] = 32'h3413BEEF;
        rom[15] = 32'h12520001;
        rom[16] = 32'h20147777;
        rom[17] = 32'h20155678;
        rom[18] = 32'h08000014;
        rom[19] = 32'h20149999;
        rom[20] = 32'h8C170000;
        rom[21] = 32'h02E9C020;
        rom[22] = 32'hAC180008;
        rom[23] = 32'h8C160008;
        rom[24] = 32'h08000018;

    end

    assign RD = rom[A];
endmodule
