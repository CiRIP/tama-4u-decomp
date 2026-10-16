    .section .text
    .align 1
    .global ir_detect_start
ir_detect_start:
    ld.w %r11,0x0
    ld.w %r13,0x0
    ld.w %r14,0x0
    ext tmr_t5_ticks@h
    ext tmr_t5_ticks@m
    ld.w %r4,tmr_t5_ticks@l
    ld.w [%r4],%r14
    ext ir_edge_count@h
    ext ir_edge_count@m
    ld.w %r12,ir_edge_count@l
    ext ir_edge_count2@h
    ext ir_edge_count2@m
    ld.w %r5,ir_edge_count2@l
.L2504A62:
    ld.w %r14,[%r5]
    cmp %r14,0x0
    jrgt .L2504A8E
    ld.w %r14,[%r4]
    cmp %r14,0x0
    jreq .L2504A62
    ld.w %r14,[%r5]
    cmp %r14,0x0
    jrgt .L2504A8E
    ld.w %r14,0x0
    ld.w [%r4],%r14
    add %r11,0x1
    xcmp %r11,0x1f4
    jrgt .L2504A82
    jp .L2504A62
.L2504A82:
    ld.w %r4,0x1
    ext ir_rx_status@h
    ext ir_rx_status@m
    ld.w %r14,ir_rx_status@l
    ld.h [%r14],%r4
    jp .L2504B56
.L2504A8E:
    ld.w %r11,0x1
    ld.w %r13,0x0
    ld.w %r14,0x0
    ext tmr_t5_ticks@h
    ext tmr_t5_ticks@m
    ld.w %r4,tmr_t5_ticks@l
    ld.w [%r4],%r14
    ext ir_edge_count@h
    ext ir_edge_count@m
    ld.w %r12,ir_edge_count@l
    ld.w %r14,0x0
    ld.w [%r12],%r14
    ext ir_edge_count2@h
    ext ir_edge_count2@m
    ld.w %r5,ir_edge_count2@l
.L2504AAC:
    ld.w %r14,[%r4]
    cmp %r14,0x0
    jreq .L2504AAC
    ld.w %r14,0x0
    ld.w [%r4],%r14
    ld.w %r14,[%r12]
    cmp %r14,0x0
    jreq .L2504AD2
    ld.w %r14,0x0
    ld.w [%r12],%r14
    add %r11,0x1
    xcmp %r11,0x62
    jrgt .L2504AEA
    ld.h %r14,[%r5]
    xcmp %r14,0x171
    jrgt .L2504AE6
    jp .L2504AAC
.L2504AD2:
    add %r13,0x1
    cmp %r13,0x3
    jreq .L2504ADA
    jp .L2504AAC
.L2504ADA:
    ld.h %r14,[%r5]
    xcmp %r14,0x168
    jrgt .L2504AF8
    ld.w %r4,0x2
    jp .L2504AEE
.L2504AE6:
    ld.w %r4,0x10
    jp .L2504AEE
.L2504AEA:
    ld.w %r4,0x11
    jp .L2504AEE
.L2504AEE:
    ext ir_rx_status@h
    ext ir_rx_status@m
    ld.w %r14,ir_rx_status@l
    ld.h [%r14],%r4
    jp .L2504B56
.L2504AF8:
    ld.w %r14,0x0
    ext tmr_t5_ticks@h
    ext tmr_t5_ticks@m
    ld.w %r4,tmr_t5_ticks@l
    ld.w [%r4],%r14
    ext ir_edge_count@h
    ext ir_edge_count@m
    ld.w %r12,ir_edge_count@l
    ld.w [%r12],%r14
    ld.w %r11,0x0
    ext ir_edge_count2@h
    ext ir_edge_count2@m
    ld.w %r14,ir_edge_count2@l
    ld.h [%r14],%r11
    ld.w %r11,0x1
    ld.w %r13,0x0
.L2504B18:
    ld.w %r14,[%r4]
    cmp %r14,0x0
    jreq .L2504B18
    ld.w %r14,0x0
    ld.w [%r4],%r14
    ld.w %r14,[%r12]
    cmp %r14,0x0
    jrgt .L2504B30
    add %r11,0x1
    cmp %r11,0x19
    jrgt .L2504B3E
    jp .L2504B18
.L2504B30:
    add %r13,0x1
    cmp %r13,0x1
    jreq .L2504B38
    jp .L2504B18
.L2504B38:
    cmp %r11,0x18
    jrgt .L2504B3E
    jp .L2504B4A
.L2504B3E:
    ld.w %r4,0x3
    ext ir_rx_status@h
    ext ir_rx_status@m
    ld.w %r14,ir_rx_status@l
    ld.h [%r14],%r4
    jp .L2504B56
.L2504B4A:
    ld.w %r4,0x0
    ext ir_rx_status@h
    ext ir_rx_status@m
    ld.w %r14,ir_rx_status@l
    ld.h [%r14],%r4
    jp .L2504B56
.L2504B56:
    ld.w %r5,0x0
    ext ir_rx_status@h
    ext ir_rx_status@m
    ld.w %r4,ir_rx_status@l
    ld.uh %r4,[%r4]
    ld.uh %r4,%r4
    ret
