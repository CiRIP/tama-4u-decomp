    .section .text
    .align 1
    .global tmr_t5_tick_handler
tmr_t5_tick_handler:
    ext tmr_t5_ticks@h
    ext tmr_t5_ticks@m
    ld.w %r2,tmr_t5_ticks@l
    ld.w %r1,[%r2]
    add %r1,0x1
    ld.w [%r2],%r1
    xld.w %r2,0x300284
    xld.w %r1,0x80
    ld.h [%r2],%r1
    popn %r2
    reti
    nop
