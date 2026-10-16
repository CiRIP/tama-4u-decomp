/*
 * Debug menu entry: set the pet to a chosen character and stage.
 *
 * Code 250E980..250E980, .rodata from 255C8C8. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/test/test_debug_chara.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/evo/evo_special.h"
#include "proto/comm/comm_menu.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/dl/dl_chara.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"

/*
 * Debug "デバックセット": cycle through all characters (built-in table chara_builtin_ids and the
 * downloaded ones) with A, B sets the pet to the shown character with a stage-matching
 * age; with the P06 debug strap held all souvenirs are given; resets counters and saves.
 */
void test_debug_set_character(void)
{
    u8 buf[4];
    u16 i;
    u8 h;
    int s;
    u8 cnt;
    int j, n, k;
    struct DlCharaRecord *e;
    struct PetState *p;

    i = 0;
    cnt = 0;
    save.pet.x000 = chara_builtin_ids[0];
    pet_become_character(save.pet.x000);
    h = gfx_object_create(0x30, 0x3b, 0x40, 0, 0, (const u8 *)chara_sprite_sets[chara_pet_sprite_set].pose0);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    for (;;) {
        p = &save.pet;
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            i++;
            if (chara_builtin_ids[i] <= 5000) {
                p->x000 = chara_builtin_ids[i];
                if (p->x000 == 0xffff)
                    i = 0;
                p->hunger_timer = 0;
                p->happiness_timer = 0;
                p->x000 = chara_builtin_ids[i];
                pet_become_character(p->x000);
                gfx_object_set_image(h, 0x40, 0, (const u8 *)chara_sprite_sets[chara_pet_sprite_set].pose0);
                cnt = 0;
            } else {
                if (dl_chara_count() == 0) {
                    i = 0;
                    p->x000 = chara_builtin_ids[0];
                } else if (cnt < dl_chara_count()) {
                    for (j = 0, n = 0; j <= 7; j++) {
                        e = dl_chara_record(j);
                        if ((int)e != -1) {
                            if (n == cnt) {
                                p->x000 = e->id;
                                cnt++;
                                i--;
                                s = j;
                                break;
                            }
                            n++;
                        }
                    }
                } else {
                    cnt = 0;
                    i = 0;
                    p->x000 = chara_builtin_ids[0];
                }
                p->hunger_timer = 0;
                p->happiness_timer = 0;
                pet_become_character(p->x000);
                if (p->x000 <= 5000)
                    gfx_object_set_image(h, 0x40, 0, (const u8 *)chara_sprite_sets[chara_pet_sprite_set].pose0);
                else
                    gfx_object_set_image(h, 0x40, 0, (const u8 *)dl_chara_pose_pic(s, 1));
            }
            func_00009A6();
        }
        if (btn_b_presses != 0) {
            btn_b_presses = 0;
            break;
        }
        if (btn_c_presses != 0) {
            btn_c_presses = 0;
            break;
        }
        sys_idle_tick();
    }
    switch (save.pet.life_stage) {
    case 1:
        save.pet.age_minutes = 1;
        break;
    case 2:
        save.pet.age_minutes = 60;
        break;
    case 3:
    default:
        save.pet.age_minutes = 0x16bc;
        break;
    }
    if (sys_debug_strap_count == 8) {
        save.pet.clover_leaves = 15;
        pet_add_happiness(0x14);
    } else {
        save.pet.clover_leaves = 0;
        save.pet.happiness = 0;
        pet_add_happiness(1);
        save.pet.clover1_id_hi = 0;
        save.pet.clover1_id_lo = 0;
        save.pet.clover2_id_hi = 0;
        save.pet.clover2_id_lo = 0;
        save.pet.clover1_kind = 0;
        save.pet.clover2_kind = 0;
    }
    k = 0;
    evo_stage_change_reset();
    save.pet.age_days = (save.pet.age_minutes + 0x59f) / 0x5a0;
    save.pet.owner_id = rand();
    for (; k < 100; k++) {
        if (save.pet.owner_id != 0)
            break;
        save.pet.owner_id = rand();
    }
    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 1;
    pet_clear_traits(1);
    pet_clear_growth_items();
    save.pet.baby_id = 0;
    save.pet.child_id = 0;
    save.pet.adult_id = 0;
    save.pet.special_id = 0;
    switch (save.pet.life_stage) {
    case 1:
        save.pet.baby_id = save.pet.x000;
        break;
    case 2:
        save.pet.baby_id = pet_get_gender() == 0 ? 1 : 3;
        save.pet.child_id = save.pet.x000;
        break;
    case 3:
        if (pet_get_gender() == 0) {
            save.pet.baby_id = 1;
            save.pet.child_id = 6;
        } else {
            save.pet.baby_id = 3;
            save.pet.child_id = 12;
        }
        save.pet.adult_id = save.pet.x000;
        break;
    }
    snd_wait_end();
    save_game();
}
