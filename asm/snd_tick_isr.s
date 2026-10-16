/*
 * T16 ch.4 compare-B interrupt (vector 46), the sound sequencer tick: saves %r0..%r15 and
 * the multiply registers AHR / ALR (in D_0084434, as the C code it calls may use
 * them), runs snd_sequencer_tick, restores, reti.
 */
    .section .text
    .align 1
    .global snd_tick_isr
snd_tick_isr:
    pushn %r15
    xld.w %r4,D_0084434
    ld.w %r5,%ahr
    ld.w [%r4]+,%r5
    ld.w %r5,%alr
    ld.w [%r4],%r5
    xcall snd_sequencer_tick
    xld.w %r4,D_0084434
    ld.w %r5,[%r4]+
    ld.w %ahr,%r5
    ld.w %r5,[%r4]
    ld.w %alr,%r5
    popn %r15
    reti
