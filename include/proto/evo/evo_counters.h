/* Functions defined in src/evo/evo_counters.c. */
#ifndef PROTO_EVO_EVO_COUNTERS_H
#define PROTO_EVO_EVO_COUNTERS_H

#include "structs.h"

void evo_count_item_use(struct TamagoFile *);
void evo_count_game(void);
void evo_count_bath(void);
void evo_flag_met_chara(u16);
void evo_note_park_meeting(void);
void evo_daily_countdown(void);
void evo_reset_counters(void);
void evo_reset_counters_all(void);

#endif
