# abc_openEuler@aarch64(ARMv8)
        .arch armv8-a
        .file   "abc.c"
        .text
        .section        .rodata
        .align  3
.LC0:
        .string "c = %d\n"
        .text
        .align  2
        .global main
        .type   main, %function
main:
.LFB0:
        .cfi_startproc
        stp     x29, x30, [sp, -32]!
        .cfi_def_cfa_offset 32
        .cfi_offset 29, -32
        .cfi_offset 30, -24
        add     x29, sp, 0
        .cfi_def_cfa_register 29
        mov     w0, 1
        str     w0, [x29, 28]
        mov     w0, 2
        str     w0, [x29, 24]
        ldr     w1, [x29, 28]
        ldr     w0, [x29, 24]
        add     w0, w1, w0
        str     w0, [x29, 20]
        adrp    x0, .LC0
        add     x0, x0, :lo12:.LC0
        ldr     w1, [x29, 20]
        bl      printf
        mov     w0, 0
        ldp     x29, x30, [sp], 32
        .cfi_restore 30
        .cfi_restore 29
        .cfi_def_cfa 31, 0
        ret
        .cfi_endproc
.LFE0:
        .size   main, .-main
        .ident  "GCC: (GNU) 7.3.0"
        .section        .note.GNU-stack,"",@progbits