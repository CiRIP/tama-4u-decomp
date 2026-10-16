/* Functions defined in src/lib/snd/snd_ch_pitch.c. */
#ifndef PROTO_LIB_SND_SND_CH_PITCH_H
#define PROTO_LIB_SND_SND_CH_PITCH_H

#include "structs.h"

void snd_set_pitch_0(u16, u16);
void snd_set_pitch_1(u16, u16);
void snd_set_pitch_2(u16, u16);
void snd_set_pitch_3(u16, u16);
void snd_set_pitch(int sel, u16 a, u16 b);

#endif
