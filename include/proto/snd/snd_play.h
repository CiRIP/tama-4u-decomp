/* Declarations of src/snd/snd_play.c: its types, variables and functions. */
#ifndef PROTO_SND_SND_PLAY_H
#define PROTO_SND_SND_PLAY_H

#include "structs.h"

/* sound: snd_finished = 1 when the last sound has finished (snd_end_callback_fn, polled by
 * snd_wait_end); snd_period_table = the period table in use (snd_period_table_main, snd_init) */
extern volatile u32 snd_finished;
extern const u16 *snd_period_table;

extern const u8 *const snd_bank[];

void snd_end_callback_fn(int);
void snd_init(void);
void snd_play(u8);
void snd_wait_end(void);
void snd_error_beep(void);

#endif
