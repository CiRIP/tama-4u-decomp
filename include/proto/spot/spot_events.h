/* Declarations of src/spot/spot_events.c: its types, variables and functions. */
#ifndef PROTO_SPOT_SPOT_EVENTS_H
#define PROTO_SPOT_SPOT_EVENTS_H

#include "structs.h"


extern const u8 D_255C8D0[];
extern const u32 D_255C980;
extern const u32 D_255C994;
extern void (*const anim_idle_random_table[])(void);
extern const u16 pet_event_phrase_lines[][25];
extern const u8 D_255C8EC[];
extern const u8 D_255C8F0[];
extern const u8 D_255C918[];
extern const u8 D_255C938[];
extern const u8 D_255C960[];

void comm_count_tenth(void);
u8 item_souvenir_flag(u8);
void spot_traveller_souvenir(void);
void spot_fortune_line(void);
void spot_fortune_event(void);
void pet_event_idle_random(void);
void spot_profile_talk(void);
void pet_event_phrase(void);
void game_score_reward(int);

#endif
