`timescale 1ns/1ps

module sim_MultiCycleCPU;
    reg clk   = 0;
    reg reset = 1;

    // DUT
    MultiCycleCPU dut (
        .clk  (clk),
        .reset(reset)
    );

    // 时钟
    always #5 clk = ~clk;

    // 复位和仿真流程（IMem 已预置 sw/lw + beq/j 测试）
    initial begin
        #2 reset = 1;
        #8 reset = 0;  // 释放复位

        // 运行若干周期（足够执行到跳转自旋）
        repeat (80) @(posedge clk);

        $display("t=%0t ns: $t0=%0h $t1=%0h $t2=%0h $t3=%0h $t4=%0h $t5=%0h $t6=%0h",
                 $time, dut.RF.rf[8], dut.RF.rf[9], dut.RF.rf[10], dut.RF.rf[11], dut.RF.rf[12], dut.RF.rf[13], dut.RF.rf[14]);

        // 期望：
        // 1) sw/lw 成功：$t6 == 5（从内存读回），$t5 == 5
        // 2) beq 生效：跳过将 $t2=DEAD
        // 3) 分支目标：$t3=5A5A
        // 4) 跳转目标：$t4=0000_BEEF
        if (dut.RF.rf[13] === 32'h0000_0005 &&
            dut.RF.rf[14] === 32'h0000_0005 &&
            dut.RF.rf[10] !== 32'hDEAD &&
            dut.RF.rf[11] === 32'h5A5A &&
            dut.RF.rf[12] === 32'h0000_BEEF) begin
            $display("TEST PASS: sw/lw + beq/j 功能正常");
        end else begin
            $display("TEST FAIL: 预期 $t5=$t6=5, $t2!=DEAD, $t3=5A5A, $t4=0000_BEEF. 实际 $t5=%0h $t6=%0h $t2=%0h $t3=%0h $t4=%0h",
                     dut.RF.rf[13], dut.RF.rf[14], dut.RF.rf[10], dut.RF.rf[11], dut.RF.rf[12]);
        end
        $finish;
    end
endmodule
