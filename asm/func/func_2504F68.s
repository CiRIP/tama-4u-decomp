    .section .text
    .align 1
    .global ir_carrier_burst
ir_carrier_burst:
    ext tmr_t5_ticks@h
    ext tmr_t5_ticks@m
    ld.w %r7,tmr_t5_ticks@l
    ld.w %r5,0x0
    ld.w [%r7],%r5
    xld.w %r8,0x3007ae
    ext ir_port_ptr@h
    ext ir_port_ptr@m
    ld.w %r9,ir_port_ptr@l
    ld.w %r8,[%r9]
    ld.w %r9,0x20
.L2504F82:
    ld.w %r4,[%r7]
    cmp %r4,0x0
    jreq .L2504F82
    ld.w [%r7],%r5
    ld.b [%r8],%r9
.L2504F8C:
    ld.w %r4,[%r7]
    cmp %r4,0x0
    jreq .L2504F8C
    ld.w [%r7],%r5
    ld.b [%r8],%r5
    sub %r6,0x1
    jrne .L2504F82
    ret
    nop
