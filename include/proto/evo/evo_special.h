/* Declarations of src/evo/evo_special.c: its types, variables and functions. */
#ifndef PROTO_EVO_EVO_SPECIAL_H
#define PROTO_EVO_EVO_SPECIAL_H

#include "structs.h"


void evo_reset_special_counters(void);
u8 evo_cond_fat(void);
u8 evo_cond_thin(void);
u8 evo_cond_snacks(void);
u8 evo_cond_park_meeting(void);
u8 evo_cond_games20(void);
u8 evo_cond_item6(void);
u8 evo_cond_item15(void);
int item_owned_slot_by_id(u8 kind, u16 id);
u8 evo_cond_baths(u8);
u8 evo_cond_met(void);
u8 evo_cond_trait0(u8);
u8 evo_cond_trait1(u8);
u8 evo_cond_unhappy(void);
u8 evo_cond_happy_clubs(u8);
u8 evo_slot_unused(u8);
void evo_clear_slots(void);
u8 evo_check_special(u8);
void evo_check_special_mask(u8);
u8 evo_choose_next(void);
u8 evo_route_lost(void);
void evo_revert_special_form(void);
void evo_evolve_now(void);
void evo_evolve_now_twice(void);
void evo_stage_change_reset(void);

#endif
