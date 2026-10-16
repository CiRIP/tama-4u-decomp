    .section .text
    .align 1
    .global ir_decode_bytes
ir_decode_bytes:
    ext ir_edge_count2@h
    ext ir_edge_count2@m
    ld.w %r7,ir_edge_count2@l
    ext tmr_t5_ticks@h
    ext tmr_t5_ticks@m
    ld.w %r4,tmr_t5_ticks@l
    ext ir_port_ptr@h
    ext ir_port_ptr@m
    ld.w %r14,ir_port_ptr@l
    ld.w %r10,[%r14]
    ext ir_edge_count@h
    ext ir_edge_count@m
    ld.w %r12,ir_edge_count@l
    ld.w %r5,0x0
    ld.w %r8,0x0
    ld.w %r9,0x0
    ld.w %r11,0x2
.L2504B86:
    ld.w %r13,0x0
.L2504B88:
    ld.w %r14,[%r4]
    cmp %r14,0x0
    jreq .L2504B88
    ld.w %r14,0x0
    ld.w [%r4],%r14
    ld.w %r14,[%r12]
    cmp %r14,0x0
    jreq .L2504BAA
    ld.w %r14,0x0
    ld.w [%r12],%r14
    add %r11,0x1
    cmp %r11,0x8
    jrgt .L2504BB8
    ld.h %r14,[%r7]
    cmp %r14,0x18
    jrgt .L2504BB8
    jp .L2504B88
.L2504BAA:
    add %r13,0x1
    cmp %r13,0x3
    jreq .L2504BB2
    jp .L2504B88
.L2504BB2:
    ld.h %r14,[%r7]
    cmp %r14,0xd
    jrgt .L2504BC4
.L2504BB8:
    ld.w %r4,0x5
    ext ir_result@h
    ext ir_result@m
    ld.w %r14,ir_result@l
    ld.h [%r14],%r4
    jp .L2504C36
.L2504BC4:
    ld.w %r11,0x0
    ext ir_edge_count2@h
    ext ir_edge_count2@m
    ld.w %r14,ir_edge_count2@l
    ld.h [%r14],%r11
    ld.w %r11,0x3
    ld.w %r13,0x0
.L2504BD2:
    ld.w %r14,[%r4]
    cmp %r14,0x0
    jreq .L2504BD2
    ld.w %r14,0x0
    ld.w [%r4],%r14
    ld.w %r14,[%r12]
    cmp %r14,0x0
    jrgt .L2504BEA
    add %r11,0x1
    cmp %r11,0x18
    jrgt .L2504C20
    jp .L2504BD2
.L2504BEA:
    add %r13,0x1
    cmp %r13,0x1
    jreq .L2504BF2
    jp .L2504BD2
.L2504BF2:
    cmp %r11,0xa
    jrgt .L2504BFE
    srl %r5,0x1
    xand %r5,0x7f
    jp .L2504C04
.L2504BFE:
    srl %r5,0x1
    xoor %r5,0x80
.L2504C04:
    add %r8,0x1
    cmp %r8,0x8
    jreq .L2504C0E
    ld.w %r11,0x1
    jp .L2504B86
.L2504C0E:
    ld.b [%r6],%r5
    add %r6,0x1
    add %r9,0x1
    cmp %r9,0x14
    jreq .L2504C2C
    ld.w %r5,0x0
    ld.w %r8,0x0
    ld.w %r11,0x1
    jp .L2504B86
.L2504C20:
    ld.w %r4,0x6
    ext ir_result@h
    ext ir_result@m
    ld.w %r14,ir_result@l
    ld.h [%r14],%r4
    jp .L2504C36
.L2504C2C:
    ext ir_result@h
    ext ir_result@m
    ld.w %r14,ir_result@l
    ld.h [%r14],%r9
    jp .L2504C36
.L2504C36:
    ld.w %r5,0x0
    ext ir_result@h
    ext ir_result@m
    ld.w %r4,ir_result@l
    ld.uh %r4,[%r4]
    ld.uh %r4,%r4
    ret
