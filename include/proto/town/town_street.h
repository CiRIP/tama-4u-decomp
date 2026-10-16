/* Declarations of src/town/town_street.c: its types, variables and functions. */
#ifndef PROTO_TOWN_TOWN_STREET_H
#define PROTO_TOWN_TOWN_STREET_H

#include "structs.h"

struct Outing {
    u8 destination;
    u8 minutes;
    u8 come_home;
};

extern struct Outing outing;

extern const u32 D_2542710;
extern const u32 D_2542744;
extern const u32 D_2542774;
extern const u32 D_2542830;
extern const u8 D_2542800[];
extern const u8 D_254280C[];
extern const u8 D_254281C[];
extern const u8 D_25426F0[];
extern const u8 D_2542700[];
extern const u8 D_2542708[];
extern const u8 D_2542768[];
extern const u8 D_254276C[];

int town_farm_state_cb(void);
int town_street_state_cb(void);
int town_enter_farm(void);
int town_enter_street(void);
u8 town_pick_street_chara(void);
void town_select_street_chara(u8);
void town_street_friendship_add(u8);
void town_street_friendship_clear(void);
int town_street_visit(void);
void town_farm_midnight(void);
u8 town_farm_event(void);
u8 *town_farm_plant_pic(u8);
int town_farm_visit(void);
int func_25096D0(u16 p0);
void outing_clear_counters(void);
void outing_end(void);
int outing_time_to_return(void);
u8 outing_hours_check(void);
int outing_minute_tick(void);
int func_25097AA(void);
int town_fountain_button(void);
int town_fountain_result(void);
int ui_question_four_lines_choice(int, int, int, int, u8);
int item_find_same_kind(int, u8, int, int);
int town_fountain_owned_item(int, struct TamagoFile **);
u8 town_fountain_pick_items(int, int);
void town_fountain_honesty_test(struct TamagoFile *, int);
int town_fountain_visit(void);

#endif
