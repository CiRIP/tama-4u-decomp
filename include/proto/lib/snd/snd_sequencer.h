/* Declarations of src/lib/snd/snd_sequencer.c: the sound sequencer. */
#ifndef PROTO_LIB_SND_SND_SEQUENCER_H
#define PROTO_LIB_SND_SND_SEQUENCER_H

#include "structs.h"

/* A voice of the sequencer (see snd_sequencer_tick). */
struct SndVoice {
    u8 state;               /* 0 free, 1 loaded / paused, 2 playing, 3 finished */
    u8 countdown;           /* ticks left in the current event */
    u8 transpose;           /* semitones (signed) */
    u8 ended;               /* the last playing voice reached its end: call the callback */
    const u16 *periods;     /* the note period table, past its 0x3301 magic */
    const u8 *sequence;     /* the next event, past the sequence's 0x3302 magic */
};

/* snd_end_callback when there is none */
#define SND_NO_CALLBACK ((void (*)(int))-1)

extern u32 snd_saved_psr;           /* PSR saved by snd_irq_save_disable */
extern u32 D_0084434[2];
extern s8 snd_voice_notes[4];       /* the note sounding per voice, -1 = none */
extern void (*snd_end_callback)(int);
extern struct SndVoice snd_voices[4];

void snd_channel_restart(int);
void snd_preset_timers(int);
void snd_tick_isr(void);
void snd_irq_save_disable(void);
void snd_irq_restore(void);
int snd_voice_note(int);
void snd_set_voice_note(int, int);
void snd_sequencer_tick(void);
void snd_sequencer_init(int);
void snd_tick_stop(void);
void snd_set_end_callback(void (*)(int));
int snd_voice_load(int, const u16 *, const u16 *, int, int);
int snd_voice_start(int);
int snd_voice_pause(int);
int snd_voice_stop(int);
int snd_voice_state(int);

#endif
