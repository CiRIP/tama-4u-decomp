/* Declarations of src/chara/chara_select.c: its types, variables and functions. */
#ifndef PROTO_CHARA_CHARA_SELECT_H
#define PROTO_CHARA_CHARA_SELECT_H

#include "structs.h"

struct CharaEntry {
    u16 sprite_set;
    u16 id;
    u8 x004;
    u8 x005;
    u16 family;
    u8 gender;
    u8 x009;
    u8 trait_caps[5];
    u8 stage;
    u8 x010;
    u8 base_weight;
    u8 hunger_period;
    u8 happiness_period;
    u8 sick_percent;
    u8 wake_hour;
    u8 sleep_hour;
    u8 charm;
    u8 fit_class;
    u8 x019;
    u16 own_accessory;
    u16 default_outfit;
    u8 bday_month;
    u8 bday_day;
    u16 class_code;
    u16 x022;
    u16 x024;
    u16 x026;
    u16 x028;
    u16 unlock_flag;
    u8 club_class;
};

extern u16 evo_stage_bonus_points;
extern volatile u16 chara_pet_sprite_set;
extern u16 chara_other_sprite_set;
extern u16 chara_other_row;
extern u16 chara_third_sprite_set;
extern u16 chara_third_row;
extern u16 chara_fourth_sprite_set;
extern u16 chara_fourth_row;
extern u16 chara_fifth_sprite_set;
extern u16 chara_fifth_row;
extern u8 rtc_seconds;

extern const struct CharaEntry chara_table[];
extern const u16 chara_builtin_ids[];

int func_2509FD0(void);
void town_visit_place(int);
void pet_become_character(u16);
void pet_set_character_id(u16);
void chara_select_pet_sprites_by_id(s16);
void pet_select_sprite_set(u16);
void chara_select_other_by_set(u16);
void chara_select_third_by_set(u16);
void chara_select_fourth_by_set(u16);
u8 chara_select_other_by_id(u16);
u8 chara_select_third_by_id(u16);
u8 chara_select_fourth_by_id(u16);
void chara_select_fifth_by_id(u16);
void chara_select_fifth_by_set(u16);
void pet_reset_for_new_egg(void);
void pet_new_game_init(void);
void pet_new_egg_random(void);
void evo_baby_to_child(void);
u16 evo_pick_adult_boy(u16);
u16 evo_pick_adult_girl(u16);
u16 evo_next_by_care_boy(u16);
u16 evo_next_by_care_girl(void);
void evo_child_to_adult(void);
void evo_stage_change(void);
void evo_stage_change_silent(void);
int pet_pick_egg_kind(void);
void pet_start_next_generation(void);
void pet_start_new_egg(int);
void evo_count_item_use_young(int, struct TamagoFile *);

#endif
