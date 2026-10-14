`timescale 1ns/1ps
module sim_PipelineCPU;
    reg clk = 0, rst = 1;

    PipelineCPU dut(.clk(clk), .rst(rst));

    always #5 clk = ~clk;

    initial begin
        // reset
        #20 rst = 0;

        // 跑足够多拍，让它执行到最后自旋
        repeat (200) @(posedge clk);

        $display("t0=%h t1=%h t2=%h t3=%h t4=%h t5=%h t6=%h t7=%h t8=%h",
            dut.u_rf.rf[8], dut.u_rf.rf[9], dut.u_rf.rf[10], dut.u_rf.rf[11],
            dut.u_rf.rf[12], dut.u_rf.rf[13], dut.u_rf.rf[14], dut.u_rf.rf[15], dut.u_rf.rf[24]);

        $display("s0=%h s1=%h s2=%h s3=%h s4=%h s5=%h s6=%h s7=%h",
            dut.u_rf.rf[16], dut.u_rf.rf[17], dut.u_rf.rf[18], dut.u_rf.rf[19],
            dut.u_rf.rf[20], dut.u_rf.rf[21], dut.u_rf.rf[22], dut.u_rf.rf[23]);

        if (dut.u_rf.rf[8]  === 32'h0000_0001 &&  // t0
            dut.u_rf.rf[9]  === 32'h0000_0002 &&  // t1
            dut.u_rf.rf[10] === 32'h0000_0003 &&  // t2
            dut.u_rf.rf[11] === 32'h0000_0002 &&  // t3
            dut.u_rf.rf[14] === 32'h0000_0001 &&  // t6
            dut.u_rf.rf[16] === 32'h0000_0003 &&  // s0
            dut.u_rf.rf[17] === 32'h0000_0004 &&  // s1 (load-use stall ok)
            dut.u_rf.rf[18] === 32'h0000_1234 &&  // s2
            dut.u_rf.rf[19] === 32'h0000_BEEF &&  // s3 (ORI zero-ext ok)
            dut.u_rf.rf[21] === 32'h0000_5678 &&  // s5 (branch taken ok)
            dut.u_rf.rf[20] === 32'h0000_0000 &&  // s4 must stay 0 (flush+skip ok)
            dut.u_rf.rf[23] === 32'h0000_0003 &&  // s7 (mem[0])
            dut.u_rf.rf[24] === 32'h0000_0005 &&  // t8 (second load-use stall ok)
            dut.u_rf.rf[22] === 32'h0000_0005     // s6 (mem[8])
        ) begin
            $display("TEST PASS: pipeline hazards/forwarding/branch/jump/ori all OK");
        end else begin
            $display("TEST FAIL");
        end

        $finish;
    end
endmodule
