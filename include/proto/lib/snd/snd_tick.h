/* Functions defined in src/lib/snd/snd_tick.c. */
#ifndef PROTO_LIB_SND_SND_TICK_H
#define PROTO_LIB_SND_SND_TICK_H

#include "structs.h"

void snd_tick_start(u16);
void snd_tick_timer_stop(void);
void snd_tick_ack(void);

#endif
