/*
 * Sound library: the sequencer (voices, notes, tick interrupt, end callback).
 *
 * A voice plays a sequence on one tone channel (T16 ch.0..3). The game only uses voices 0
 * and 1, with the same melody and the second output inverted: the two outputs drive the
 * piezo from both ends (notes/findings/sound.md).
 */
#include "proto/lib/snd/snd_sequencer.h"
#include "proto/lib/snd/snd_tick.h"
#include "proto/lib/snd/snd_ch_on.h"
#include "proto/lib/snd/snd_ch_off.h"
#include "proto/lib/snd/snd_ch_pitch.h"
#include "proto/lib/snd/snd_ch_restart.h"
#include "hw.h"

/*
 * Sound channel ch: restart its timer.
 */
void snd_channel_restart(int ch)
{
    switch (ch) {
    case 0:
        snd_restart_ch0();
        break;
    case 1:
        snd_restart_ch1();
        break;
    case 2:
        snd_restart_ch2();
        break;
    case 3:
        snd_restart_ch3();
        break;
    }
}

/*
 * Sound: preset (counter to 0, output to its initial level) the tone timers selected by bit
 * mask m (bit n = timer / channel n, 0..3). The sequencer tick passes the voices that
 * started a new note in this tick, so a new note starts at a fresh period and TM0 / TM1
 * (same note, one output inverted) restart together in antiphase. Tied notes are not
 * preset.
 */
void snd_preset_timers(int m)
{
    if (m & 1)
        T16_CTL0_B0 |= 2;
    if (m & 2)
        T16_CTL1_B0 |= 2;
    if (m & 4)
        T16_CTL2_B0 |= 2;
    if (m & 8)
        T16_CTL3_B0 |= 2;
}

/*
 * Disable interrupts (PSR = 0), saving the PSR in snd_saved_psr for snd_irq_restore. The
 * pairs do not nest: an inner pair re-enables interrupts.
 */
void snd_irq_save_disable(void)
{
    u32 psr;

    asm volatile("ld.w %0,%%psr" : "=r"(psr));
    snd_saved_psr = psr;
    asm volatile("ld.w %%psr,%0" : : "r"(0));
}

/*
 * Restore the PSR (interrupt enable) saved by snd_irq_save_disable.
 */
void snd_irq_restore(void)
{
    asm volatile("ld.w %%psr,%0" : : "r"(snd_saved_psr));
}

/*
 * The note voice a is sounding now (1..0x3D after transposition, -1 = none / rest). The
 * sequencer tick compares it with the next event to detect a tie.
 */
int snd_voice_note(int a)
{
    return snd_voice_notes[a];
}

/*
 * Set the note voice a is sounding (0xFF after a load and on rests). See snd_voice_note.
 */
void snd_set_voice_note(int a, int b)
{
    snd_voice_notes[a] = b;
}

/* the length in ticks of event c: its bits 7:6 */
static u8 event_ticks(int c)
{
    switch (c >> 6) {
    case 0:
        return 4;
    case 1:
        return 8;
    case 2:
        return 0x10;
    default:
        return 0x20;
    }
}

/*
 * One sequencer tick, run by the T16 ch.4 interrupt (snd_tick_isr) every 7.78 ms.
 * A voice (struct SndVoice): state (0 free, 1 loaded / paused, 2 playing, 3 finished),
 * ticks left in the current event, transposition (signed semitones), an "ended" flag, the
 * period table (past its 0x3301 magic) and the event pointer (past the 0x3302 magic of the
 * sequence).
 * A sequence is one byte per event, 0 = end; bits 7:6 = length 4 / 8 / 16 / 32 ticks;
 * bits 5:0 = note 1..0x3D (C3..C8; period = table[note - 1], tone CRB = period - 1,
 * CRA = period/2 - 1), 0x3F = rest, 0x3E = a gap of length/4 ticks. An event followed by
 * a 0x3E byte is shortened by (that byte >> 6) + 1 ticks (the data only uses 0x3E itself:
 * the note gives its last tick to a 1-tick gap). A note equal to the one sounding
 * (snd_voice_note, compared before transposition) is a tie: no retrigger.
 * Per tick: acknowledge the interrupt; voices in state 1 / 3 are kept silent (1000 /
 * 2000); a playing voice counts down or takes its next event; at the end byte it goes to
 * state 3, and when no voice is left in state 2 its "ended" flag is set. Then the timers
 * of the voices that began a new note are preset together (snd_preset_timers), and the end
 * callback is called for each voice whose flag is set.
 */
void snd_sequencer_tick(void)
{
    int i, j;
    u8 d;
    int mask;
    int c;
    int note;
    u16 period;
    u8 st;

    snd_tick_ack();
    mask = 0;
    for (i = 0; i <= 3; i++) {
        struct SndVoice *v = &snd_voices[i];

        st = v->state;
        if (st == 0)
            continue;
        v->ended = 0;
        if (st == 3 || st == 1) {
            snd_set_pitch(i, 1000, 2000);
            continue;
        }
        if (v->countdown != 0) {
            v->countdown--;
            continue;
        }
        c = *v->sequence;
        note = c & 0x3f;
        if (note != 0x3e && snd_voice_note(i) == note) {
            /* a tie: the note goes on */
            v->countdown = event_ticks(c) - 1;
            v->sequence++;
            continue;
        }
        if (c == 0) {
            v->state = 3;
            v->countdown = 0;
            d = 4;
            for (j = 0; j <= 3; j++)
                if (snd_voices[j].state != 2)
                    d--;
            if (d == 0)
                v->ended = 1;
            continue;
        }
        v->sequence++;
        d = event_ticks(c);
        if (note == 0x3e)
            d >>= 2;
        if ((*v->sequence & 0x3f) == 0x3e)
            d = (u8)(d - (*v->sequence >> 6) - 1);
        v->countdown = d - 1;
        if (note > 0x3d) {
            snd_set_pitch(i, 1000, 2000);
            snd_set_voice_note(i, 0xff);
            continue;
        }
        note = (s8)(note + v->transpose);
        if (note <= 0)
            note = 1;
        if (note > 0x3d)
            note = 0x3d;
        snd_set_voice_note(i, note);
        period = v->periods[note - 1];
        snd_set_pitch(i, period - 1, (period >> 1) - 1);
        mask |= 1 << i;
    }
    snd_preset_timers(mask);
    if (snd_end_callback != SND_NO_CALLBACK) {
        for (i = 0; i <= 3; i++) {
            if (snd_voices[i].state != 0 && snd_voices[i].ended == 1) {
                snd_voices[i].ended = 0;
                snd_end_callback(i);
            }
        }
    }
}

/*
 * Sound library init: no callback, all 4 voices free, then start the tick on T16 ch.4 with
 * compare B = clk (snd_tick_start; 0xE3 = 7.78 ms at 30 MHz). Called by snd_init, which
 * then stops the tick until a sound is played.
 */
void snd_sequencer_init(int clk)
{
    int i;

    snd_end_callback = SND_NO_CALLBACK;
    for (i = 0; i <= 3; i++) {
        snd_voices[i].state = 0;
        snd_voices[i].countdown = 0;
    }
    snd_tick_start(clk);
}

/*
 * Stop the sound sequencer tick (snd_tick_timer_stop). Library API that nothing calls: the
 * game stops the tick by writing CTL4 = 0 (snd_end_callback_fn, snd_init).
 */
void snd_tick_stop(void)
{
    snd_tick_timer_stop();
}

/*
 * Set the end callback (called by the sequencer tick with the voice number when the last
 * playing voice reaches its end byte; SND_NO_CALLBACK = none). The game sets
 * snd_end_callback_fn (snd_init).
 */
void snd_set_end_callback(void (*f)(int))
{
    snd_end_callback = f;
}

/*
 * Load voice ch with the period table a (u16 0x3301 + 61 periods), the sequence b (u16
 * 0x3302 + event bytes, see snd_sequencer_tick), transposition tr semitones and output
 * mode e (1 = inverted). Returns -1 for ch > 3, -3 for tr < -24 (the upper test compares
 * ch with 24 instead of tr: a bug, harmless since ch <= 3 and the game passes tr = 0), -4
 * for a bad magic. Otherwise stops the voice, turns its tone channel on silent (1000 /
 * 2000), state 1 (loaded, waiting for snd_voice_start), current note 0xFF, returns 0.
 * Only caller: snd_play (voices 0 and 1).
 */
int snd_voice_load(int ch, const u16 *a, const u16 *b, int tr, int e)
{
    if ((u32)ch > 3)
        return -1;
    if (tr < -24 || ch > 24)
        return -3;
    if (*a != 0x3301)
        return -4;
    snd_voices[ch].periods = a + 1;
    if (*b != 0x3302)
        return -4;
    snd_voices[ch].sequence = (const u8 *)(b + 1);
    snd_voice_stop(ch);
    snd_voices[ch].transpose = tr;
    snd_ch_on(ch, 1000, 2000, e);
    snd_voices[ch].state = 1;
    snd_set_voice_note(ch, 0xff);
    return 0;
}

/*
 * Start voice ch (state 2, countdown 0, timer PRESET + PRUN via snd_channel_restart); the
 * next tick reads its first event. -1 bad voice, -2 not loaded.
 */
int snd_voice_start(int ch)
{
    if ((u32)ch > 3)
        return -1;
    if (snd_voices[ch].state == 0)
        return -2;
    snd_voices[ch].state = 2;
    snd_voices[ch].countdown = 0;
    snd_channel_restart(ch);
    return 0;
}

/*
 * Pause voice ch (back to state 1: kept silent, position kept). Nothing calls it.
 */
int snd_voice_pause(int ch)
{
    if ((u32)ch > 3)
        return -1;
    if (snd_voices[ch].state == 0)
        return -2;
    snd_voices[ch].state = 1;
    return 0;
}

/*
 * Stop voice ch: tone channel off (snd_ch_off: pin back to GPIO, timer and its clock off),
 * state 0 (free).
 */
int snd_voice_stop(int ch)
{
    if ((u32)ch > 3)
        return -1;
    snd_ch_off(ch);
    snd_voices[ch].state = 0;
    snd_voices[ch].countdown = 0;
    return 0;
}

/*
 * State of voice ch (0 free, 1 loaded / paused, 2 playing, 3 finished), -1 for a bad
 * voice.
 */
int snd_voice_state(int ch)
{
    if ((u32)ch > 3)
        return -1;
    return snd_voices[ch].state;
}

/* The DST-RAM variables of the sound library. */
u32 snd_saved_psr DSTRAM;
u32 D_0084434[2] DSTRAM;               /* AHR / ALR saved by the tick interrupt (asm/snd_tick_isr.s) */
s8 snd_voice_notes[4] DSTRAM;
void (*snd_end_callback)(int);
struct SndVoice snd_voices[4] DSTRAM;
