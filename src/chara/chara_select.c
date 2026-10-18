/*
 * town_visit_place, then becoming a character, sprite-set selection for the pet and the other
 * characters, new game / new egg, the stage changes baby -> child -> adult and the next
 * generation.
 *
 * Open: whether 2509FD0 and town_visit_place (2509FD4) are the end of town_street.
 *
 * Code 2509FD0..250B316, .rodata from 255B4D8. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/chara/chara_select.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/pet/pet_bath.h"
#include "proto/food/food_meals.h"
#include "proto/room/room_window.h"
#include "proto/town/town_places.h"
#include "proto/diary/diary_notebook.h"
#include "proto/pet/pet_toilet.h"
#include "proto/anim/anim_scripts.h"
#include "proto/evo/evo_counters.h"
#include "proto/evo/evo_special.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/pet/pet_gone.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/ir/ir_receive.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_menu.h"
#include "proto/town/town_street.h"
#include "proto/save/save_flash.h"
#include "proto/pet/pet_care_room.h"
#include "proto/pet/pet_care.h"
#include "proto/owner/owner_profile.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/comm/comm_outcome.h"
#include "proto/comm/comm_checks.h"
#include "libc.h"
#include "hw.h"

/*
 * chara_table (in this data block): the character table, 46-byte rows (x000 sprite set, x002
 * character id, x006 family, x008 kind, x00F stage, x011 base weight, x012 hunger period,
 * x013 happiness period, x014 sickness %, x015/x016 wake/sleep hours, x017 value,
 * x01E/x01F/x020 codes, x02A unlock flag, x02C ...), terminated by id 0xFFFF.
 */
const struct CharaEntry chara_table[46] = {
    { 0, 0, 0, 0, 0, 0, 0, { 20, 20, 20, 20, 20 },
      1, 15, 5, 3, 5, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 1, 1, 0, 0, 1, 0, 0, { 20, 20, 20, 20, 20 },
      1, 15, 5, 3, 5, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 2, 2, 0, 0, 2, 1, 0, { 20, 20, 20, 20, 20 },
      1, 15, 5, 3, 5, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 3, 3, 0, 0, 3, 1, 0, { 20, 20, 20, 20, 20 },
      1, 15, 5, 3, 5, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 4, 4, 0, 0, 4, 0, 0, { 20, 20, 20, 20, 20 },
      2, 20, 5, 60, 60, 40, 7, 20, 0, 4, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 5, 5, 0, 0, 5, 0, 0, { 20, 20, 20, 20, 20 },
      2, 20, 5, 60, 45, 40, 7, 20, 0, 4, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 6, 6, 0, 0, 6, 0, 0, { 20, 20, 20, 20, 20 },
      2, 20, 5, 60, 60, 40, 7, 20, 0, 4, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 7, 7, 0, 0, 7, 0, 0, { 20, 20, 20, 20, 20 },
      2, 20, 5, 60, 45, 40, 7, 20, 0, 4, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 8, 10, 0, 0, 10, 1, 0, { 20, 20, 20, 20, 20 },
      2, 20, 5, 60, 60, 40, 7, 20, 0, 4, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 9, 11, 0, 0, 11, 1, 0, { 20, 20, 20, 20, 20 },
      2, 20, 5, 60, 45, 40, 7, 20, 0, 4, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 10, 12, 0, 0, 12, 1, 0, { 20, 20, 20, 20, 20 },
      2, 20, 5, 60, 60, 40, 7, 20, 0, 4, 0, 1, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 11, 13, 0, 0, 13, 1, 0, { 20, 20, 20, 20, 20 },
      2, 20, 5, 60, 45, 40, 7, 20, 0, 4, 0, 2, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 12, 16, 0, 0, 16, 0, 0, { 50, 40, 50, 30, 40 },
      3, 50, 15, 95, 35, 30, 7, 21, 0, 1, 0, 0, 0, 11, 23, 0, 2, 15018, 2, 15018, 29, 1 },
    { 13, 17, 0, 0, 17, 0, 0, { 40, 30, 40, 50, 50 },
      3, 50, 15, 50, 70, 20, 7, 21, 0, 2, 0, 0, 0, 5, 18, 3, 2, 15005, 2, 15005, 30, 3 },
    { 14, 18, 0, 0, 18, 0, 2, { 50, 50, 40, 30, 40 },
      3, 50, 15, 70, 60, 80, 7, 22, 0, 1, 0, 0, 0, 7, 20, 6, 2, 15010, 0, 15005, 31, 7 },
    { 15, 19, 0, 0, 19, 0, 2, { 40, 50, 50, 40, 30 },
      3, 50, 15, 70, 50, 40, 7, 22, 0, 1, 0, 0, 1, 7, 19, 9, 2, 15013, 2, 15013, 32, 6 },
    { 16, 20, 0, 0, 20, 0, 0, { 40, 40, 50, 40, 40 },
      3, 50, 15, 80, 40, 80, 7, 20, 0, 1, 0, 0, 2, 3, 6, 12, 2, 15018, 0, 15002, 33, 10 },
    { 17, 21, 0, 0, 21, 0, 0, { 50, 50, 50, 50, 50 },
      3, 50, 15, 50, 50, 60, 7, 20, 0, 1, 0, 0, 3, 7, 17, 30, 2, 15018, 0, 15002, 39, 1 },
    { 18, 1001, 0, 0, 1001, 1, 3, { 40, 50, 40, 40, 40 },
      3, 50, 15, 90, 70, 60, 7, 21, 0, 1, 0, 0, 0, 10, 10, 14, 2, 15011, 2, 15011, 34, 2 },
    { 19, 1002, 0, 0, 1002, 1, 1, { 50, 50, 30, 40, 40 },
      3, 50, 15, 90, 60, 80, 7, 21, 0, 1, 0, 0, 4, 2, 14, 17, 2, 15015, 0, 15003, 35, 8 },
    { 20, 1003, 0, 0, 1003, 1, 1, { 30, 40, 40, 50, 50 },
      3, 50, 15, 90, 35, 30, 7, 21, 0, 1, 0, 0, 5, 9, 10, 19, 2, 15016, 0, 15010, 36, 4 },
    { 21, 1004, 0, 0, 1004, 1, 4, { 30, 40, 50, 50, 40 },
      3, 50, 15, 70, 50, 40, 7, 22, 0, 1, 0, 3, 6, 12, 29, 22, 2, 15004, 2, 15004, 37, 5 },
    { 22, 1005, 0, 0, 1005, 1, 2, { 40, 50, 30, 50, 40 },
      3, 50, 15, 50, 80, 20, 7, 22, 0, 3, 0, 0, 7, 12, 6, 24, 2, 15006, 1, 15011, 38, 9 },
    { 23, 1006, 0, 0, 1006, 1, 2, { 50, 50, 50, 50, 50 },
      3, 50, 15, 60, 60, 50, 7, 21, 0, 1, 0, 0, 8, 3, 30, 14, 2, 15006, 1, 15011, 40, 13 },
    { 24, 2001, 0, 0, 16, 0, 1, { 50, 40, 50, 30, 40 },
      4, 50, 15, 95, 35, 60, 7, 22, 0, 0, 0, 0, 0, 11, 23, 1, 2, 15017, 4, 15010, 29, 1 },
    { 25, 2002, 0, 0, 16, 0, 3, { 50, 40, 50, 30, 40 },
      4, 50, 15, 50, 30, 80, 7, 22, 0, 0, 0, 0, 0, 11, 23, 2, 2, 15011, 1, 15015, 29, 1 },
    { 26, 2003, 0, 0, 17, 0, 1, { 40, 30, 40, 50, 50 },
      4, 50, 15, 40, 90, 20, 7, 20, 0, 0, 0, 0, 0, 5, 18, 4, 2, 15007, 2, 15007, 30, 3 },
    { 27, 2004, 0, 0, 17, 0, 3, { 40, 30, 40, 50, 50 },
      4, 50, 15, 70, 50, 60, 7, 21, 0, 0, 0, 0, 0, 5, 18, 5, 2, 15012, 0, 15014, 30, 3 },
    { 28, 2005, 0, 0, 18, 0, 4, { 50, 50, 40, 30, 40 },
      4, 50, 15, 60, 50, 80, 7, 21, 0, 0, 0, 0, 0, 7, 20, 7, 2, 15016, 0, 15013, 31, 7 },
    { 29, 2006, 0, 0, 18, 0, 2, { 50, 50, 40, 30, 40 },
      4, 50, 15, 80, 80, 20, 7, 22, 0, 0, 0, 0, 0, 7, 20, 8, 2, 15009, 1, 15007, 31, 7 },
    { 30, 2007, 0, 0, 19, 0, 2, { 40, 50, 50, 40, 30 },
      4, 50, 15, 90, 70, 20, 7, 22, 0, 0, 0, 0, 0, 7, 19, 10, 2, 15010, 1, 15008, 32, 6 },
    { 31, 2008, 0, 0, 19, 0, 0, { 40, 50, 50, 40, 30 },
      4, 50, 15, 70, 35, 40, 7, 22, 0, 0, 0, 0, 0, 7, 19, 11, 2, 15006, 2, 15006, 32, 6 },
    { 32, 2009, 0, 0, 20, 0, 2, { 40, 40, 50, 40, 40 },
      4, 50, 15, 80, 60, 60, 7, 22, 0, 0, 0, 0, 0, 3, 6, 13, 2, 15009, 0, 15009, 33, 10 },
    { 33, 2010, 0, 0, 21, 0, 2, { 50, 30, 50, 40, 30 },
      4, 50, 15, 50, 50, 60, 7, 22, 0, 0, 0, 0, 0, 7, 17, 10, 2, 15009, 0, 15009, 39, 1 },
    { 34, 3001, 0, 0, 1001, 1, 1, { 40, 50, 40, 40, 40 },
      4, 50, 15, 90, 50, 60, 7, 20, 0, 0, 0, 0, 0, 10, 10, 15, 2, 15008, 2, 15008, 34, 2 },
    { 35, 3002, 0, 0, 1001, 1, 1, { 40, 50, 40, 40, 40 },
      4, 50, 15, 70, 70, 30, 7, 21, 0, 0, 0, 0, 0, 10, 10, 16, 2, 15015, 0, 15011, 34, 2 },
    { 36, 3003, 0, 0, 1002, 1, 4, { 50, 50, 30, 40, 40 },
      4, 50, 15, 90, 50, 80, 7, 21, 0, 0, 0, 0, 0, 2, 14, 18, 2, 15004, 1, 15014, 35, 8 },
    { 37, 3004, 0, 0, 1003, 1, 1, { 30, 40, 50, 50, 50 },
      4, 50, 15, 70, 50, 20, 7, 20, 0, 0, 0, 0, 0, 9, 10, 20, 2, 15006, 1, 15005, 36, 4 },
    { 38, 3005, 0, 0, 1003, 1, 0, { 30, 40, 40, 50, 50 },
      4, 50, 15, 50, 50, 60, 7, 22, 0, 0, 0, 0, 0, 9, 10, 21, 2, 15009, 1, 15002, 36, 4 },
    { 39, 3006, 0, 0, 1004, 1, 1, { 30, 40, 50, 50, 40 },
      4, 50, 15, 70, 60, 20, 7, 22, 0, 0, 0, 0, 0, 12, 29, 23, 2, 15017, 1, 15012, 37, 5 },
    { 40, 3007, 0, 0, 1005, 1, 2, { 40, 50, 30, 50, 40 },
      4, 50, 15, 90, 50, 60, 7, 21, 0, 0, 0, 0, 0, 12, 6, 25, 2, 15014, 2, 15014, 38, 9 },
    { 41, 3008, 0, 0, 1006, 1, 2, { 50, 50, 50, 50, 50 },
      4, 50, 15, 60, 60, 50, 7, 21, 0, 0, 0, 0, 0, 3, 30, 25, 2, 15014, 2, 15014, 40, 13 },
    { 42, 3009, 0, 0, 1004, 1, 2, { 50, 50, 50, 50, 50 },
      4, 50, 15, 70, 50, 20, 7, 20, 0, 0, 0, 0, 0, 12, 29, 27, 2, 15014, 2, 15014, 37, 5 },
    { 43, 3010, 0, 0, 1002, 1, 2, { 50, 50, 50, 50, 50 },
      4, 50, 15, 70, 50, 20, 7, 21, 0, 0, 0, 0, 0, 2, 14, 26, 2, 15014, 2, 15014, 35, 8 },
    { 104, 9999, 61, 0, 0, 1, 0, { 100, 100, 100, 100, 100 },
      3, 50, 15, 60, 60, 60, 7, 20, 0, 0, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
    { 0xffff, 0xffff, 0, 0, 0, 0, 0, { 100, 100, 100, 100, 100 },
      3, 50, 15, 60, 60, 0, 7, 19, 0, 0, 0, 0, 0, 0, 0, 37, 0xffff, 0, 0xffff, 0, 0xffff, 0 },
};
/* built-in character ids in the debug character cycle (test_debug_set_character), ended by 0xFFFF */
const u16 chara_builtin_ids[20] = {
    1, 3, 6, 7, 12, 13, 21, 16, 17, 18, 19, 20, 1006, 1001, 1002, 1003, 1004, 1005, 5001, 0xffff,
};

/*
 * Returns 1 (stub).
 */
int func_2509FD0(void)
{
    return 1;
}

/*
 * Visit town place a directly (its enter callbacks x008/x00C/x010), with town_visit_flag set
 * meanwhile.
 */
void town_visit_place(int a)
{
    u8 k = a;

    town_visit_begin();
    if (town_places[k].x008 == 0 || town_places[k].x008() == 0) {
        if (town_places[k].x00c != 0 && town_places[k].x00c() != 0) {
            if (town_places[k].x010 != 0)
                town_places[k].x010();
            town_visit_end();
            ui_timeout = 10;
            return;
        }
    }
    town_visit_end();
}

/*
 * Become character a (growth, transformation): set pet.x000/x002 (table row or
 * downloaded record), life stage, base weight, timers and the character-dependent values.
 */
void pet_become_character(u16 a)
{
    const struct CharaEntry *t;
    struct DlCharaRecord *e;
    u16 i = 0;

    if (a <= 5000) {
        save.pet.x000 = a;
        t = chara_table;
        for (;;) {
            if (t[i].id == save.pet.x000) {
                save.pet.chara_row = i;
                break;
            }
            if (t[i].id == 0xffff) {
                save.pet.chara_row = 0;
                break;
            }
            i++;
        }
        save.pet.life_stage = chara_table[save.pet.chara_row].stage;
        if (save.pet.weight < chara_table[save.pet.chara_row].base_weight)
            save.pet.weight = chara_table[save.pet.chara_row].base_weight;
        if (save.pet.hunger_timer == 0)
            save.pet.hunger_timer = chara_table[save.pet.chara_row].hunger_period;
        if (save.pet.happiness_timer == 0)
            save.pet.happiness_timer = chara_table[save.pet.chara_row].happiness_period;
        pet_select_sprite_set(chara_table[save.pet.chara_row].sprite_set);
        save.pet.gender = chara_table[save.pet.chara_row].gender;
    } else {
        save.pet.x000 = a;
        save.pet.chara_row = 5001;
        do {
            e = dl_chara_record(i);
            if (e != (struct DlCharaRecord *)-1 && e->id == save.pet.x000) {
                save.pet.chara_row = i + 5001;
                break;
            }
            i++;
        } while (i <= 7);
        e = dl_chara_record(save.pet.chara_row + 0x77);
        save.pet.life_stage = e->stage;
        if (save.pet.weight < e->base_weight)
            save.pet.weight = e->base_weight;
        if (save.pet.hunger_timer == 0)
            save.pet.hunger_timer = e->hunger_period;
        if (save.pet.happiness_timer == 0)
            save.pet.happiness_timer = e->happiness_period;
        pet_select_sprite_set(e->sprite_set);
        memset(save.pet.traits, 0, 5);
        save.pet.dominant_trait = 0;
        save.pet.gender = e->gender;
    }
}

/*
 * Set the pet's character id a and table row without resetting the stats.
 */
void pet_set_character_id(u16 a)
{
    u16 i;
    const struct CharaEntry *t;
    struct DlCharaRecord *q;

    if (a <= 5000) {
        save.pet.x000 = a;
        t = chara_table;
        for (i = 0;; i++) {
            if (t[i].id == save.pet.x000) {
                save.pet.chara_row = i;
                break;
            }
            if (t[i].id == 0xffff) {
                save.pet.chara_row = 0;
                break;
            }
        }
        pet_select_sprite_set(chara_table[save.pet.chara_row].sprite_set);
    } else {
        save.pet.x000 = a;
        save.pet.chara_row = 5001;
        for (i = 0; i <= 7; i++) {
            q = dl_chara_record(i);
            if (q != (struct DlCharaRecord *)-1 && q->id == save.pet.x000) {
                save.pet.chara_row = i + 5001;
                break;
            }
        }
        pet_select_sprite_set(dl_chara_record(save.pet.chara_row + 0x77)->sprite_set);
    }
}

/*
 * Select character a as the "second character" for animations (by id).
 */
void chara_select_pet_sprites_by_id(s16 a)
{
    int i;
    int r;
    const struct CharaEntry *t;
    struct DlCharaRecord *q;

    if (a <= 5000) {
        t = chara_table;
        for (i = 0;; i++) {
            if (t[i].id == a) {
                r = (u16)i;
                break;
            }
            if (t[i].id == 0xffff) {
                r = 0;
                break;
            }
        }
        chara_pet_sprite_set = chara_table[r].sprite_set;
        save.pet.chara_row = r;
    } else {
        r = 5001;
        for (i = 0; i <= 7; i++) {
            q = dl_chara_record(i);
            if (q != (struct DlCharaRecord *)-1 && q->id == a) {
                r = (u16)(i + 5001);
                break;
            }
        }
        chara_pet_sprite_set = dl_chara_record(r + 0x77)->sprite_set;
        save.pet.chara_row = r;
    }
}

/*
 * Select the pet's sprite set by sprite-set id.
 */
void pet_select_sprite_set(u16 id)
{
    struct DlCharaRecord *p;
    int i;
    u16 r;

    if (save.pet.x000 <= 5000) {
        for (i = 0; ; i++) {
            if (chara_table[i].sprite_set == id) {
                r = i;
                break;
            }
            if (chara_table[i].sprite_set == 0xffff) {
                r = 0;
                break;
            }
        }
    } else {
        r = 5001;
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->sprite_set == id) {
                r = i + 5001;
                break;
            }
        }
    }
    chara_pet_sprite_set = id;
    save.pet.chara_row = r;
}

/*
 * Select the visitor character by sprite-set id (chara_other_sprite_set / chara_other_row).
 */
void chara_select_other_by_set(u16 id)
{
    int i;
    u16 r;

    for (i = 0; ; i++) {
        if (chara_table[i].sprite_set == id) {
            r = i;
            break;
        }
        if (chara_table[i].sprite_set == 0xffff) {
            r = 0;
            break;
        }
    }
    chara_other_sprite_set = id;
    chara_other_row = r;
}

/*
 * Select the third character by sprite-set id (chara_third_sprite_set / chara_third_row).
 */
void chara_select_third_by_set(u16 id)
{
    int i;
    u16 r;

    for (i = 0; ; i++) {
        if (chara_table[i].sprite_set == id) {
            r = i;
            break;
        }
        if (chara_table[i].sprite_set == 0xffff) {
            r = 0;
            break;
        }
    }
    chara_third_sprite_set = id;
    chara_third_row = r;
}

/*
 * Select the fourth character by sprite-set id (chara_fourth_sprite_set / chara_fourth_row).
 */
void chara_select_fourth_by_set(u16 id)
{
    int i;
    u16 r;

    for (i = 0; ; i++) {
        if (chara_table[i].sprite_set == id) {
            r = i;
            break;
        }
        if (chara_table[i].sprite_set == 0xffff) {
            r = 0;
            break;
        }
    }
    chara_fourth_sprite_set = id;
    chara_fourth_row = r;
}

/*
 * Select the "other" character by id (table chara_table: x002 = id, x000 = sprite set);
 * unknown ids select entry 0. Returns its x008 field.
 */
u8 chara_select_other_by_id(u16 id)
{
    u16 i;
    u16 r;
    struct CharaEntry *t;

    for (i = 0, t = chara_table; ; i++) {
        if (t[i].id == id) {
            r = i;
            break;
        }
        if (t[i].id == 0xffff) {
            r = 0;
            break;
        }
    }
    chara_other_sprite_set = chara_table[r].sprite_set;
    chara_other_row = r;
    return chara_table[r].gender;
}

/*
 * Select the third character by character id (chara_third_sprite_set / chara_third_row); returns its x008.
 */
u8 chara_select_third_by_id(u16 id)
{
    u16 i;
    u16 r;
    struct CharaEntry *t;

    for (i = 0, t = chara_table; ; i++) {
        if (t[i].id == id) {
            r = i;
            break;
        }
        if (t[i].id == 0xffff) {
            r = 0;
            break;
        }
    }
    chara_third_sprite_set = chara_table[r].sprite_set;
    chara_third_row = r;
    return chara_table[r].gender;
}

/*
 * Select the fourth character by character id (chara_fourth_sprite_set / chara_fourth_row); returns its x008.
 */
u8 chara_select_fourth_by_id(u16 id)
{
    u16 i;
    u16 r;
    struct CharaEntry *t;

    for (i = 0, t = chara_table; ; i++) {
        if (t[i].id == id) {
            r = i;
            break;
        }
        if (t[i].id == 0xffff) {
            r = 0;
            break;
        }
    }
    chara_fourth_sprite_set = chara_table[r].sprite_set;
    chara_fourth_row = r;
    return chara_table[r].gender;
}

/*
 * Select the character of the fifth slot by id (chara_fifth_sprite_set = sprite set,
 * chara_fifth_row = table index).
 */
void chara_select_fifth_by_id(u16 id)
{
    int i;
    u16 r;
    struct CharaEntry *t;

    for (i = 0, t = chara_table; ; i++) {
        if (t[i].id == id) {
            r = i;
            break;
        }
        if (t[i].id == 0xffff) {
            r = 0;
            break;
        }
    }
    chara_fifth_sprite_set = chara_table[r].sprite_set;
    chara_fifth_row = r;
}

/*
 * Select the character of the fifth slot (chara_fifth_sprite_set / chara_fifth_row) by
 * sprite-set id.
 */
void chara_select_fifth_by_set(u16 id)
{
    int i;
    u16 r;

    for (i = 0; ; i++) {
        if (chara_table[i].sprite_set == id) {
            r = i;
            break;
        }
        if (chara_table[i].sprite_set == 0xffff) {
            r = 0;
            break;
        }
    }
    chara_fifth_sprite_set = id;
    chara_fifth_row = r;
}

/*
 * Reset the pet state for a new egg/generation: age, counters, flags, calls, poops,
 * items, worn accessories, events...
 */
void pet_reset_for_new_egg(void)
{
    save.pet.age_minutes = 0;
    save.pet.kitchen_dirt = 0;
    save.pet.toilet_dirt = 0;
    save.pet.living_dirt = 0;
    save.pet.bedroom_dirt = 0;
    save.pet.bath_dirt = 0;
    save.pet.praise_timer = 0;
    pet_praise_expired = 0;
    save.pet.status = 0;
    save.pet.sick_count = 0;
    save.pet.decline_minutes = 0;
    save.pet.starve_minutes = 0;
    save.pet.ten_day_minutes = 0;
    save.pet.ten_day_event_due = 0;
    save.pet.gone_minutes = 0;
    save.pet.neglect_minutes = 0;
    save.pet.hunger_timer = 3;
    save.pet.happiness_timer = 5;
    save.pet.hungry_call_timer = 15;
    save.pet.unhappy_call_timer = 15;
    rtc_seconds = 0;
    save.pet.whim_call_timer = 0;
    save.pet.poop_count = 0;
    pet_poops_drawn = 0;
    save.pet.toilet_timer = 0;
    save.pet.hunger_loss_parity = 0;
    save.pet.age_days = 0;
    save.pet.hunger = 0;
    save.pet.meals_today = 0;
    save.pet.happiness = 0;
    save.pet.clover_leaves = 0;
    save.pet.clover1_id_hi = 0;
    save.pet.clover2_id_hi = 0;
    save.pet.clover1_id_lo = 0;
    save.pet.clover2_id_lo = 0;
    save.pet.clover1_kind = 0;
    save.pet.clover2_kind = 0;
    save.pet.x062 = 1;
    save.pet.x0cc = 0;
    save.pet.x082 = 0;
    save.pet.companion_sprite_set = 0;
    save.pet.calls = 0;
    save.pet.weight = 0;
    save.pet.x040 = 0;
    save.pet.x087 = 0;
    save.pet.care_mistakes = 0;
    save.pet.worn_accessory = 0;
    save.pet.worn_clothes = 0;
    save.pet.toilet_count = 0;
    save.pet.game_count = 0;
    save.pet.meal_count = 0;
    save.pet.x148 = 0;
    save.pet.baby_id = 0;
    save.pet.child_id = 0;
    save.pet.adult_id = 0;
    save.pet.special_id = 0;
    save.pet.discipline_flags = 0;
    save.pet.revive_talks = 0;
    memset(&save.pet.training[0], 0, 3);
    memset(save.pet.club_records, 0, 15);
    memset(save.pet.club_mastered, 0, 5);
    evo_reset_counters_all();
    save.pet.evo_route = 0;
    save.pet.park_meeting = 0;
    save.pet.evo_bath_count = 0;
    save.pet.evo_pending_days = 0;
    save.pet.met_orenetchi = 0;
    save.pet.special_evo_item = 0;
    save.pet.special_evo_slots[0] = 0;
    save.pet.special_evo_slots[1] = 0;
    save.pet.special_evo_slots[2] = 0;
    save.pet.special_evo_slots[3] = 0;
    town_street_friendship_clear();
}

void pet_new_game_init(void)
{
    int i;
    int z = 0, k;

    save.pet.gotchi_points = z;
    k = 1;
    save.pet.generation = k;
    save.pet.owner_bday_day = k;
    save.pet.owner_bday_month = k;
    save.pet.weather_today = z;
    sys_backlight_level = 4;
    sys_set_backlight(4);
    save.pet.life_stage = z;
    pet_reset_for_new_egg();
    snd_enabled = k;
    save.pet.room_designs[0] = 10;
    for (i = 0; i < 16; i += 2) {
        int p = (int)&save.pet;
        *(u8 *)(i + p + 0x70) = 4;
        *(u8 *)(i + p + 0x71) = 0x6c;
    }
    for (i = 0; i < 25; i++) {
        int p = (int)&save.pet;
        int o = 0x44;
        *(u8 *)(i + p + o + 3) = 0;
    }
    dl_chara_affection_update(0xff);
    pet_clear_traits(0);
    pet_clear_growth_items();
    memset(&save.pet.x13d, 0, 8);
    save.pet.profile_age = 100;
    save.pet.baby_id = 0;
    save.pet.child_id = 0;
    save.pet.adult_id = 0;
    save.pet.special_id = 0;
    evo_reset_counters_all();
    town_street_friendship_clear();
}

/*
 * New egg: random egg 1 or 3 (50%), stage 0, clear marks and counters.
 */
void pet_new_egg_random(void)
{
    u16 k;

    if (rand() % 100 <= 49)
        k = 1;
    else
        k = 3;
    pet_become_character(k);
    save.pet.life_stage = 0;
    dl_chara_affection_update(0xff);
    pet_clear_growth_items();
    memset(&save.pet.x13d, 0, 8);
    save_game();
    ui_main_icon = 0;
    anim_new_egg();
}

/*
 * Baby -> child: character 1 becomes 6 or 7, character 3 becomes 12 or 13, depending on
 * the happiness meter (> 9).
 */
void evo_baby_to_child(void)
{
    switch (save.pet.x000) {
    case 1:
        pet_become_character(save.pet.happiness > 9 ? 6 : 7);
        break;
    case 3:
        pet_become_character(save.pet.happiness > 9 ? 12 : 13);
        break;
    }
}

/*
 * Pick a downloaded character that can follow character a (growth into a downloaded
 * character), 0 if none.
 */
u16 evo_pick_adult_boy(u16 a)
{
    u16 list[10];
    u16 ids[8];
    int i;
    u16 n;
    u16 r = 0;
    struct DlCharaRecord *e;

    for (i = r; i <= 7; i++)
        ids[i] = 0xffff;
    for (i = 0; i <= 7; i++) {
        e = dl_chara_record(i);
        if (e != (struct DlCharaRecord *)-1 && e->gender == 0 && save.pet.dl_affection[i] > 2)
            ids[i] = i;
    }
    util_shuffle_u16(ids, 8);
    for (i = 0; i <= 7; i++) {
        if (ids[i] != 0xffff)
            return dl_chara_record(ids[i])->id;
    }
    for (n = i = 0; i <= 9; i++)
        list[i] = 0;
    if (save.pet.comm_count > 9) {
        if (save.pet.growth_item_counts[1] > 2) {
            r = 0x15;
            list[n] = r;
            n++;
        }
        if (save.pet.traits[3] > 9 && save.pet.happiness > 0x13) {
            r = 0x15;
            list[n] = r;
            n++;
        }
    }
    if (save.pet.traits[0] > 9 && save.pet.care_mistakes <= 4) {
        r = 0x10;
        list[n] = r;
        n++;
    }
    if (save.pet.traits[4] > 9 && save.pet.weight > 0x3b) {
        r = 0x11;
        list[n] = r;
        n++;
    }
    if (save.pet.growth_item_counts[0] > 4) {
        r = 0x12;
        list[n] = r;
        n++;
    }
    if (save.pet.traits[2] > 9 && save.pet.care_mistakes <= 4) {
        r = 0x13;
        list[n] = r;
        n++;
    }
    if (n > 1) {
        for (i = 0; i <= 8; i++)
            util_shuffle_u16(list, n);
        r = list[0];
    }
    return r;
}

/*
 * Variant of evo_pick_adult_boy for code x.
 */
u16 evo_pick_adult_girl(u16 x)
{
    u16 a[10];
    u16 b[8];
    u16 r = 0;
    int i;
    u16 n;
    struct DlCharaRecord *s;

    for (i = r; i < 8; i++)
        b[i] = 0xffff;
    for (i = 0; i < 8; i++) {
        s = dl_chara_record(i);
        if (s != (struct DlCharaRecord *)-1 && s->gender == 1 && save.pet.dl_affection[i] > 2)
            b[i] = i;
    }
    util_shuffle_u16(b, 8);
    for (i = 0; i < 8; i++) {
        if (b[i] != 0xffff)
            return dl_chara_record(b[i])->id;
    }
    n = i = 0;
    for (; i < 10; i++)
        a[i] = 0;
    if (save.pet.comm_count > 4) {
        if (save.pet.growth_item_counts[0] > 2) {
            r = 0x3ee;
            a[n] = r;
            n++;
        }
        if (save.pet.traits[4] > 5 && save.pet.happiness > 0x13) {
            r = 0x3ee;
            a[n] = r;
            n++;
        }
    }
    if (save.pet.traits[3] > 9 && save.pet.care_mistakes <= 4) {
        r = 0x3eb;
        a[n] = r;
        n++;
    }
    if (save.pet.traits[0] > 0x13) {
        r = 0x3ea;
        a[n] = r;
        n++;
    }
    if (save.pet.weight > 0x45) {
        r = 0x3ed;
        a[n] = r;
        n++;
    }
    if (save.pet.traits[1] > 0x13) {
        r = 0x3e9;
        a[n] = r;
        n++;
    }
    if (save.pet.traits[2] > 9 && save.pet.care_mistakes <= 4) {
        r = 0x3ec;
        a[n] = r;
        n++;
    }
    if (n > 1) {
        for (i = 0; i < 9; i++)
            util_shuffle_u16(a, n);
        r = a[0];
    }
    return r;
}

/*
 * Next character from k by care mistakes x00F (<= 2 / <= 7 / more), e.g. 6 -> 0x10/0x11/0x12.
 */
u16 evo_next_by_care_boy(u16 k)
{
    u16 r = 0;

    switch (k) {
    case 6:
        if (save.pet.care_mistakes <= 2)
            r = 0x10;
        else if (save.pet.care_mistakes <= 7)
            r = 0x11;
        else
            r = 0x12;
        break;
    case 7:
        if (save.pet.care_mistakes <= 2)
            r = 0x13;
        else if (save.pet.care_mistakes <= 7)
            r = 0x14;
        else
            r = 0x12;
        break;
    }
    return r;
}

/*
 * Next character for 0xC / 0xD by care mistakes (0x3EA..0x3EC...).
 */
u16 evo_next_by_care_girl(void)
{
    int id = 0;

    switch (save.pet.x000) {
    case 12:
        if (save.pet.care_mistakes <= 2)
            id = 1003;
        else if (save.pet.care_mistakes <= 7)
            id = 1002;
        else
            id = 1005;
        break;
    case 13:
        if (save.pet.care_mistakes <= 2)
            id = 1004;
        else if (save.pet.care_mistakes <= 7)
            id = 1001;
        else
            id = 1005;
        break;
    }
    return id;
}

/*
 * Growth step child -> adult (6 / 7 and 12 / 13 -> a stage-3 adult): a downloaded follow-up
 * if any, else by care mistakes; the ten-day count restarts.
 */
void evo_child_to_adult(void)
{
    u16 k;

    switch (save.pet.x000) {
    case 6:
    case 7:
        k = evo_pick_adult_boy(save.pet.x000);
        if (k == 0)
            k = evo_next_by_care_boy(save.pet.x000);
        pet_become_character(k);
        break;
    case 12:
    case 13:
        k = evo_pick_adult_girl(save.pet.x000);
        if (k == 0)
            k = evo_next_by_care_girl();
        pet_become_character(k);
        break;
    }
    save.pet.ten_day_minutes = 0;
    save.pet.ten_day_event_due = 0;
}

/*
 * Stage change (called by main): egg -> hatch (new generation), baby -> child (evo_baby_to_child),
 * child -> adult (evo_child_to_adult), with the matching animations, points and resets. (The
 * adult <-> special form changes are evo_evolve_now's.)
 */
void evo_stage_change(void)
{
    struct PetState *p;
    struct PetState *q;
    int n;

    chara_select_other_by_id(save.pet.x000);
    switch (save.pet.life_stage) {
    case 0:
        pet_start_next_generation();
        save.pet.family_continues = 1;
        save.pet.life_stage = 1;
        anim_hatch();
        if (save.comm_book_notice_mode == 1)
            save.comm_book_notice_mode = 2;
        comm_notebook_unlock_check();
        p = &save.pet;
        save.comm_book_notice_mode = 0;
        save.pet.baby_id = p->x000;
        save.pet.child_id = 0;
        save.pet.adult_id = 0;
        save.pet.special_id = 0;
        p->care_mistakes = 0;
        if (p->hunger == 0)
            p->calls |= 1;
        if (save.pet.happiness == 0)
            save.pet.calls |= 2;
        if (save.pet.calls != 0 && rtc_seconds < 0x3c)
            rtc_seconds = 0x3c;
        break;
    case 1:
        anim_growth_start();
        evo_baby_to_child();
        anim_growth_end();
        anim_clear_pic_slots2();
        evo_stage_bonus_points = 300;
        n = 300;
        pet_add_points(n);
        anim_points_gain(n);
        anim_happy();
        save.pet.child_id = save.pet.x000;
        break;
    case 2:
        anim_growth_start();
        evo_child_to_adult();
        q = &save.pet;
        save.pet.x148 = 0;
        anim_growth_end();
        anim_clear_pic_slots2();
        evo_stage_bonus_points = 500;
        n = 500;
        pet_add_points(n);
        anim_points_gain(n);
        anim_happy();
        if (q->status & 2)
            q->status &= 0x7d;
        save.pet.meals_today = 0;
        pet_clear_traits(1);
        pet_clear_growth_items();
        save.pet.adult_id = save.pet.x000;
        break;
    }
    save.pet.toilet_count = 0;
    save.pet.game_count = 0;
    save.pet.meal_count = 0;
    save.pet.x087 = 0;
    save.pet.sick_count = 0;
    friend_loop_noop_b();
    if (save.pet.training[2] != 0xff && save.pet.cleaning_timer != 0) {
        save.pet.cleaning_timer = 0;
        REG8(&pet_request_bits) &= ~4;
    }
    if (save.pet.training[1] != 0xff && save.pet.bath_timer != 0) {
        save.pet.bath_timer = 0;
        REG8(&pet_request_bits) &= ~2;
    }
    save_game();
}

/*
 * Pending stage change (growth), by the current stage pet.life_stage: 0 -> pet_start_next_generation,
 * stage 1, character noted in pet.baby_id; 1 -> evo_baby_to_child, 300 points, pet.child_id; 2 ->
 * evo_child_to_adult, 500 points, pet.adult_id. Then pet_select_sprite_set(previous character) and the
 * per-stage counters are reset; pet.x062 = 1.
 */
void evo_stage_change_silent(void)
{
    u8 k = save.pet.x000_b;
    struct PetState *p;

    save.pet.x062 = 1;
    save.pet.x082 = 0;
    switch (save.pet.life_stage) {
    case 0:
        pet_start_next_generation();
        p = &save.pet;
        save.pet.family_continues = 1;
        save.pet.life_stage = 1;
        save.pet.baby_id = p->x000;
        save.pet.child_id = 0;
        save.pet.adult_id = 0;
        save.pet.special_id = 0;
        p->care_mistakes = 0;
        break;
    case 1:
        evo_baby_to_child();
        pet_add_points(300);
        save.pet.child_id = save.pet.x000;
        break;
    case 2:
        evo_child_to_adult();
        pet_add_points(500);
        if (save.pet.status & 2)
            save.pet.status &= 0x7d;
        save.pet.meals_today = 0;
        evo_reset_counters_all();
        pet_clear_traits(1);
        pet_clear_growth_items();
        save.pet.adult_id = save.pet.x000;
        break;
    }
    pet_select_sprite_set(k);
    save.pet.toilet_count = 0;
    save.pet.game_count = 0;
    save.pet.meal_count = 0;
    save.pet.x087 = 0;
    save.pet.sick_count = 0;
    save.pet.x148 = 0;
    friend_loop_noop_b();
    if (save.pet.training[2] != 0xff && save.pet.cleaning_timer != 0) {
        save.pet.cleaning_timer = 0;
        pet_request_bits &= ~4;
    }
    if (save.pet.training[1] != 0xff && save.pet.bath_timer != 0) {
        save.pet.bath_timer = 0;
        pet_request_bits &= ~2;
    }
    save_game();
}

/*
 * Pick the new egg kind (1 or 3) from the previous generation.
 */
int pet_pick_egg_kind(void)
{
    int r;

    if (save.pet.generation > 1) {
        if (rand() % 100 <= 49)
            r = 1;
        else
            r = 3;
    } else if (pet_get_gender()) {
        if (rand() % 100 <= 69)
            r = 1;
        else
            r = 3;
    } else {
        if (rand() % 100 <= 69)
            r = 3;
        else
            r = 1;
    }
    return r;
}

/*
 * Start the next generation egg (pet.family_continues: new generation): select the egg character,
 * reset the state (pet_reset_for_new_egg), carry over generation data.
 */
void pet_start_next_generation(void)
{
    int k;

    pet_life_paused = 1;
    if (save.pet.family_continues)
        k = save.pet.next_egg_kind;
    else
        k = rand() % 100 > 50 ? 2 : 0;
    switch (k) { case 0: chara_select_other_by_id(1); break; case 1: chara_select_other_by_id(1); break; case 2: chara_select_other_by_id(3); break; default: chara_select_other_by_id(3); break; }
    pet_reset_for_new_egg();
    if (save.pet.family_continues) {
        if (++save.pet.generation > 99)
            save.pet.generation = 99;
    }
    switch (k) { case 0: pet_become_character(1); break; case 1: pet_become_character(1); break; case 2: pet_become_character(3); break; default: pet_become_character(3); break; }
    friend_loop_noop_a();
    friend_loop_noop_b();
    pet_clear_traits(1);
    pet_clear_activities();
    job_clear_all();
    dl_chara_affection_update(0xff);
    pet_clear_growth_items();
    memset(&save.pet.x13d, 0, 8);
    rtc_seconds = 0;
    save.pet.life_stage = 0;
    evo_reset_counters_all();
    save_game();
    pet_life_paused = 0;
}

/*
 * Start a new egg of kind k (1 or 3) as a new generation (pet.generation = generation, max
 * 99); clears the friend/visit state.
 */
void pet_start_new_egg(int k)
{
    pet_life_paused = 1;
    switch (k) { case 0: chara_select_other_by_id(1); break; case 1: chara_select_other_by_id(1); break; case 2: chara_select_other_by_id(3); break; default: chara_select_other_by_id(3); break; }
    pet_reset_for_new_egg();
    if (++save.pet.generation > 99)
        save.pet.generation = 99;
    switch (k) { case 0: pet_become_character(1); break; case 1: pet_become_character(1); break; case 2: pet_become_character(3); break; default: pet_become_character(3); break; }
    pet_life_paused = 0;
    friend_loop_noop_a();
    friend_loop_noop_b();
    pet_clear_traits(1);
    pet_clear_activities();
    job_clear_all();
    dl_chara_affection_update(0xff);
    pet_clear_growth_items();
    memset(&save.pet.x13d, 0, 8);
    save_game();
    rtc_seconds = 60;
}

/*
 * Register item p use for babies/children (growth counters by category).
 */
void evo_count_item_use_young(int a, struct TamagoFile *p)
{
    if (save.pet.life_stage > 2)
        return;
    if (pet_get_gender() == 0) {
        if (a == 2)
            return;
        if (item_type(p) != 0x81)
            return;
        switch (item_category(p)) {
        case 1:
            if (item_sub_kind(p) != 1)
                return;
            if (item_id(p) != 2)
                return;
            if (save.pet.growth_item_counts[0] > 4)
                return;
            save.pet.growth_item_counts[0]++;
            break;
        case 4:
            if (item_sub_kind(p) != 1)
                return;
            if (item_id(p) != 0x420)
                return;
            if (save.pet.growth_item_counts[1] > 2)
                return;
            save.pet.growth_item_counts[1]++;
            break;
        }
    } else {
        if (a == 2)
            return;
        if (item_type(p) != 0x81)
            return;
        if (item_category(p) != 1)
            return;
        if (item_sub_kind(p) != 2)
            return;
        if (item_id(p) != 0x3f6)
            return;
        if (save.pet.growth_item_counts[0] > 2)
            return;
        save.pet.growth_item_counts[0]++;
    }
}

/*
 * The RAM variables this file defines.
 */
u16 evo_stage_bonus_points;
volatile u16 chara_pet_sprite_set;
u16 chara_other_sprite_set;
u16 chara_other_row;
u16 chara_third_sprite_set;
u16 chara_third_row;
u16 chara_fourth_sprite_set;
u16 chara_fourth_row;
u16 chara_fifth_sprite_set;
u16 chara_fifth_row;
u8 rtc_seconds;
