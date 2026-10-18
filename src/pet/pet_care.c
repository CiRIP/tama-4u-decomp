/*
 * The care menu (praise, scold, talk, comfort), the sitter (call, questions, return, takeover)
 * and the request overlays drawn on the room (sitter, toilet, bath, cleaning, praise).
 *
 * Code 250F506..25109EE, .rodata from 255CA60.
 */
#include "proto/pet/pet_care.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/pet/pet_bath.h"
#include "proto/room/room_window.h"
#include "proto/anim/anim_scripts.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_message.h"
#include "proto/pet/pet_gone.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care_room.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * A child's habit learned (stage 2): the training counters D_1348[3] (toilet / bath /
 * cleaning, counted by pet_toilet / pet_bath / pet_cleaning) against their thresholds
 * (toilet 2..6 by character, bath and cleaning 2); one that reaches it becomes 0xFF
 * (learned) and the medal scene plays with its picture (poop D_20F44C8 / bathtub D_20F45A8 /
 * toy box D_20F4688). Also ends the praise window pet.praise_timer.
 */
int pet_habit_learned(void)
{
    int r = 0;
    u8 n = 0;
    int i;
    u8 *p;
    u32 kept;

    if (save.pet.life_stage == 2) {
        for (i = 0; i < 3; i++) {
            if (save.pet.training[i] == 0xff)
                continue;
            switch (i) {
            case 0:
                switch (save.pet.x000) {
                case 6:
                    n = 2;
                    break;
                case 7:
                    n = 5;
                    break;
                case 13:
                    n = 3;
                    break;
                case 12:
                    n = 1;
                    break;
                }
                break;
            case 1:
            case 2:
                n = 1;
                break;
            }
            n++;
            p = (u8 *)&save.pet + i;
            if (p[0x1ac] >= n) {
                p[0x1ac] = 0xff;
                kept = anim_pic_slots[0][0];
                switch (i) {
                case 0:
                    anim_pic_slots[0][0] = D_20F44C8;
                    break;
                case 1:
                    anim_pic_slots[0][0] = D_20F45A8;
                    break;
                case 2:
                    anim_pic_slots[0][0] = D_20F4688;
                    break;
                }
                if (sys_display_state)
                    sys_display_wake(1);
                anim_habit_learned();
                anim_pic_slots[0][0] = (const u8 *)kept;
                r = 1;
            }
        }
    }
    if (save.pet.discipline_flags & 2)
        save.pet.discipline_flags ^= 2;
    save.pet.praise_timer = 0;
    pet_praise_expired = 0;
    if (r)
        save_game();
    return r;
}

/* the room decoration lists of the sitter scenes (room scene steps, struct RoomStep) */
const struct RoomStep room_scene_sitter[] = {
    { 0, 0, 64, 50 }, { 9, 0, 56, 50 }, { 6, 0, 56, 50 }, { 7, 0, 56, 50 },
    { 2, 0x10, 75, 32 }, { 9, 0x10, 99, 50 }, { 8, 0, 96, 51 }, { 2, 0, 81, 33 },
    { 8, 0, 60, 51 }, { 0, 0, 63, 50 }, { 9, 0x10, 63, 50 }, { 2, 0x10, 75, 31 },
    { 9, 0x10, 103, 50 }, { 8, 0, 104, 50 }, { 7, 0, 103, 50 }, { 9, 0, 95, 50 },
    { 8, 0, 95, 51 }, { 9, 0, 95, 50 }, { 8, 0, 95, 51 }, { 2, 0, 95, 50 },
    { 8, 0, 96, 51 }, { 9, 0, 96, 50 }, { 8, 0, 96, 51 }, { 9, 0, 96, 50 },
    { 5, 0, 95, 50 }, { 5, 0, 96, 50 }, { 3, 0, 94, 50 }, { 3, 0x10, 92, 50 },
    { 3, 0, 94, 50 }, { 3, 0, 89, 50 }, { 3, 0, 84, 50 }, { 6, 0, 84, 50 },
    { 3, 0, 84, 50 }, { 6, 0, 84, 50 }, { 0xa, 0, 84, 50 }, { 0, 0, 85, 50 },
    { 0xa, 0, 84, 50 }, { 0, 0, 85, 50 }, { 9, 0, 84, 50 }, { 0, 0, 84, 50 },
    { 9, 0, 76, 50 }, { 0xff, 0, 0, 0 },
};
const struct RoomStep room_scene_toilet_pending[] = {
    { 0, 0, 65, 50 }, { 0, 0, 63, 50 }, { 0, 0, 65, 50 }, { 0, 0, 63, 50 },
    { 9, 0x10, 76, 50 }, { 0xa, 0, 76, 50 }, { 9, 0, 55, 50 }, { 9, 0x10, 61, 50 },
    { 0xa, 0x10, 65, 50 }, { 0xa, 0, 65, 50 }, { 0xa, 0x10, 65, 50 }, { 0xff, 0, 0, 0 },
};
const struct RoomStep room_scene_bath_pending[] = {
    { 0, 0, 65, 50 }, { 0, 0, 63, 50 }, { 0, 0, 65, 50 }, { 0, 0, 63, 50 },
    { 9, 0x10, 76, 50 }, { 0xc, 0, 73, 50 }, { 6, 0, 73, 50 }, { 0xc, 0, 73, 50 },
    { 3, 0, 63, 50 }, { 3, 0x10, 63, 50 }, { 3, 0, 63, 50 }, { 0xff, 0, 0, 0 },
};
const struct RoomStep room_scene_clean_pending[] = {
    { 3, 0, 64, 50 }, { 3, 0, 64, 49 }, { 0xff, 0, 0, 0 },
};
const struct RoomStep room_scene_praise_window[] = {
    { 0xe, 0, 80, 53 }, { 0xd, 0, 80, 53 }, { 0xe, 0, 80, 53 }, { 0xd, 0, 80, 53 },
    { 0xe, 0, 80, 53 }, { 0xd, 0, 80, 53 }, { 0xe, 0, 80, 53 }, { 0xd, 0, 80, 53 },
    { 0xff, 0, 0, 0 },
};
const struct RoomStep room_scene_toilet_call[] = {
    { 0, 0, 64, 50 }, { 0, 0x10, 67, 50 }, { 0, 0, 64, 50 }, { 0, 0x10, 67, 50 },
    { 0, 0, 64, 50 }, { 2, 0, 64, 50 }, { 0, 0, 64, 50 }, { 2, 0, 64, 50 },
    { 0xff, 0, 0, 0 },
};
const struct RoomStep room_scene_bath_call[] = {
    { 0, 0, 64, 50 }, { 0, 0x10, 67, 50 }, { 0, 0, 64, 50 }, { 0, 0x10, 67, 50 },
    { 0, 0, 64, 50 }, { 2, 0, 64, 50 }, { 0, 0, 64, 50 }, { 2, 0, 64, 50 },
    { 0xff, 0, 0, 0 },
};
const struct RoomStep room_scene_clean_call[] = {
    { 0, 0, 64, 50 }, { 0, 0x10, 67, 50 }, { 0, 0, 64, 50 }, { 0, 0x10, 67, 50 },
    { 0, 0, 64, 50 }, { 2, 0, 64, 50 }, { 0, 0, 64, 50 }, { 2, 0, 64, 50 },
    { 0xff, 0, 0, 0 },
};
const u8 D_255CCE4[] = _("ほめる");
const u8 D_255CCEC[] = _("しかる");
const u8 D_255CCF4[] = _("はなしかける");
const u8 D_255CD04[] = _("いたわり");
const u8 D_255CD10[] = _("たまシッター");
const u8 *const pet_care_menu_texts[] = { D_255CCE4, D_255CCEC, D_255CCF4, D_255CD04, D_255CD10 };

/*
 * F44 = the CARE menu ("おせわ"): ほめる praise / しかる scold / はなしかける talk /
 * いたわり comfort / たまシッター the Tama-sitter (babysitter). pet.discipline_flags = what the
 * character just did (bit 0 refused something, bit 1 did something praiseworthy),
 * pet.praise_timer / pet.praise_timer / pet_praise_expired = pending care events, pet_sitter_state = the sitter
 * (x000 active, x001 "come back now").
 *
 * The care menu (five entries, four per page; the sitter entry greyed unless the
 * character is a child or older, fed, happy and healthy). Growth step changes while in
 * the menu are reconciled (pet_care_poops_at_entry). Returns 1 when an action ran.
 */
int pet_care_menu(void)
{
    u32 tbl[5];
    u8 page;
    u8 last;
    int ret;
    u8 sel;
    int redraw;
    u8 total = 5;
    u8 per = 4;
    u8 cur;
    u8 i;
    int y;
    struct PetState *p;

    memcpy(tbl, pet_care_menu_texts, 20);
    sel = 0;
    page = 0;
    last = 0;
    redraw = 1;
    ret = 0;
    pet_care_poops_at_entry = save.pet.poop_count;
    pet_care_entry = save.pet.praise_timer != 0 ? 1 : 0;
    btn_clear();
again:
    ui_timeout = 10;
loop:
    if (redraw) {
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
        if (total > per)
            gfx_object_create(0x72, 0x58, 0x10, page < (total - 1) / per ? 0 : 1, 0, 0x21059a4);
        for (redraw = i = 0; i < per; i++) {
            if (sel + i >= total)
                break;
            if (sel == 4 && !(save.pet.life_stage > 1 && save.pet.hunger != 0 && save.pet.happiness != 0 && save.pet.status == 0)) {
                cur = gfx_object_create(0, 0, 0x24, 0, 3, 0);
                gfx_draw_text_line(cur, 0x18, i * 16 + 0x20, 0x24, tbl[sel + i], 0, 6);
            } else {
                cur = gfx_object_create(0, 0, 0, 0, 0, 0);
                gfx_draw_text_line(cur, 0x18, i * 16 + 0x20, 0x20, tbl[sel + i], 0, 6);
            }
        }
        y = (sel % per) * 16 + 0x22;
        redraw = 0;
        cur = gfx_object_create(6, y, 0x30, 0, 0, 0x2104714);
        func_00009A6();
    }
    if (btn_a_presses) {
        sel = sel < total - 1 ? sel + 1 : 0;
        page = sel / per;
        if (last != page) {
            last = page;
            redraw = 1;
        } else {
            gfx_object_move_dirty(cur, 6, (sel % per) * 16 + 0x22);
            func_00009A6();
        }
        btn_a_presses = 0;
        ui_timeout = 10;
    }
    if (btn_b_presses) {
        switch (sel) {
        case 0:
            pet_care_praise();
            break;
        case 1:
            pet_care_scold();
            break;
        case 2:
            pet_care_talk();
            break;
        case 3:
            pet_care_comfort();
            break;
        case 4:
            if (pet_care_poops_at_entry != save.pet.poop_count) {
                if (save.pet.poop_count > 5 && pet_care_poops_at_entry <= 5)
                    pet_get_sick();
                pet_poops_drawn = save.pet.poop_count;
            }
            p = &save.pet;
            if (save.pet.life_stage > 1 && p->hunger != 0 && p->happiness != 0 && p->status == 0) {
                pet_sitter_call();
                break;
            }
            snd_error_beep();
            btn_b_presses = 0;
            goto again;
        }
        ui_timeout = 10;
        ret = 1;
    } else if (btn_c_presses == 0) {
        sys_idle_tick();
        if (ui_timeout != 0)
            goto loop;
    }
    btn_clear();
    return ret;
}

/*
 * PRAISE: refused while any status flag x011 is set (sick, dirty, collapsed...:
 * anim_refuse, -1 happiness); after a good deed (pet.discipline_flags bit 1) the happy scene and +5
 * happiness (and the pending care event pet_habit_learned), otherwise a puzzled scene. Clears
 * the deed flags and the pending care events, saves.
 */
void pet_care_praise(void)
{
    if (save.pet.status != 0) {
        anim_refuse();
        pet_sub_happiness(1);
        return;
    }
    if (save.pet.discipline_flags & 2) {
        anim_praise();
        pet_add_happiness(5);
        if (save.pet.praise_timer != 0 || pet_care_entry != 0)
            pet_habit_learned();
    } else {
        anim_puzzled();
    }
    save.pet.discipline_flags = 0;
    save.pet.praise_timer = 0;
    pet_praise_expired = 0;
    save_game();
}

/*
 * SCOLD: on a collapsed / departing character (x011 & 4) it caps the sick / starving
 * timers x012 / x006 at 0x8AC and sets x011 bit 4 (the pet leaves: the "gone" step of the
 * departure ladder, the pet-gone screen follows); with another status
 * flag it is refused like praise (-1); otherwise after a refusal (pet.discipline_flags bit 0) the
 * scolding works (+4 happiness), after a good deed it backfires (-5, and the pending care
 * event pet_habit_learned), else -1. Ends the pending discipline call (pet.discipline_flags, x024,
 * pet_praise_expired cleared); saves.
 */
void pet_care_scold(void)
{
    if (save.pet.status != 0) {
        if (save.pet.status & 4) {
            if (save.pet.decline_minutes > 0x86f)
                save.pet.decline_minutes = 0x8ac;
            if (save.pet.starve_minutes > 0x86f)
                save.pet.starve_minutes = 0x8ac;
            save.pet.gone_minutes = 0;
            save.pet.status |= 0x10;
            pet_clear_call(8);
        } else {
            anim_refuse();
            pet_sub_happiness(1);
        }
    } else {
        if (save.pet.discipline_flags & 1) {
            anim_scold_collapsed();
            pet_add_happiness(4);
        } else if (save.pet.discipline_flags & 2) {
            anim_scold();
            pet_sub_happiness(5);
            if (save.pet.praise_timer != 0 || pet_care_entry != 0)
                pet_habit_learned();
        } else {
            anim_scold();
            pet_sub_happiness(1);
        }
        save.pet.discipline_flags = 0;
        save.pet.praise_timer = 0;
        pet_praise_expired = 0;
    }
    save_game();
}

/*
 * TALK: a collapsed character needs three tries (pet.revive_talks), then says its line 0xB and
 * RECOVERS (all sick / neglect state cleared, x011 = 0); otherwise with a pending care
 * event the event scene (pet_habit_learned), for a child that has not learned a habit yet
 * (pet.training[0] / pet.training[2] / pet.training[1] != 0xFF) while that
 * request is pending (toilet urge, 18:00 cleaning, 19:00 bath) the special talks, else one
 * of two chat lines (text groups 2 / 3).
 */
void pet_care_talk(void)
{
    u8 *p;

    if (save.pet.status != 0) {
        if (save.pet.status & 4) {
            if (save.pet.revive_talks <= 1) {
                anim_collapsed_talk();
                save.pet.revive_talks++;
                return;
            } else {
                pet_clear_call(8);
                p = chara_text(save.pet.x000, 0xb, 0);
                anim_pet_revive();
                ui_show_message(7, 0, p);
            }
            anim_pet_revive_end();
            anim_happy();
            save.pet.revive_talks = 0;
            room_view = 0;
            save.pet.x040 = 0;
            save.pet.weather_today = 0;
            save.pet.starve_minutes = 0;
            save.pet.x087 = 0;
            save.pet.decline_minutes = 0;
            save.pet.sick_count = 0;
            if (save.pet.status & 2)
                save.pet.meals_today = 0;
            save.pet.status = 0;
            save.pet.neglect_minutes = 0;
            if (save.pet.praise_timer == 0)
                pet_habit_learned();
            return;
        }
        anim_refuse();
        pet_sub_happiness(1);
        return;
    }
    if (save.pet.praise_timer != 0 || pet_care_entry != 0) {
        anim_puzzled();
        pet_habit_learned();
    } else if (save.pet.training[0] != 0xff && save.pet.life_stage == 2 && save.pet.toilet_timer != 0) {
        anim_talk_toilet_hint();
    } else if (save.pet.training[2] != 0xff && save.pet.life_stage == 2 && save.pet.cleaning_timer != 0) {
        anim_talk_cleaning_hint();
    } else if (save.pet.training[1] != 0xff && save.pet.life_stage == 2 && save.pet.bath_timer != 0) {
        anim_talk_bath_hint();
    } else {
        p = chara_text(save.pet.x000, (rand() & 1) ? 2 : 3, 0);
        ui_show_message(7, 0, p);
        anim_happy();
    }
    save.pet.discipline_flags = 0;
    save.pet.praise_timer = 0;
    pet_praise_expired = 0;
    save_game();
}

/*
 * COMFORT (いたわり): cures a sick / messy character (x011 bits 0, 1, 6 = 0x43 set, not
 * collapsed / gone = 0x14 clear): clears x040, the nursing scene; the 0x80 variant only
 * clears that bit and needs a second round; otherwise (with the RTC interrupt masked,
 * 0x300277 bit 2) timers and flags reset, +1 happiness, pet_habit_learned when x024 is clear,
 * save. Nothing to do -> the puzzled scene.
 */
void pet_care_comfort(void)
{
    u8 f = save.pet.status;

    if ((f & 0x14) == 0 && (f & 0x43) != 0) {
        save.pet.x040 = 0;
        anim_comfort();
        if (save.pet.status & 0x80) {
            save.pet.status &= ~0x80;
        } else {
            INT_EP47_ERTC_EAD &= ~4;
            anim_clear_pic_slots2();
            anim_happy();
            if (save.pet.decline_minutes <= 0x86f)
                save.pet.decline_minutes = 0;
            if ((save.pet.status & 2) || save.pet.neglect_minutes > 0x10df) {
                save.pet.meals_today = 0;
                save.pet.neglect_minutes = 0;
            }
            save.pet.status = 0;
            pet_add_happiness(1);
            room_view = 0;
            if (save.pet.praise_timer == 0)
                pet_habit_learned();
            pet_clear_call(8);
            save_game();
            INT_EP47_ERTC_EAD |= 4;
        }
    } else {
        anim_refuse();
    }
}

/*
 * TAMA-SITTER: only between 7 and 16 h (pet_sitter_hours; otherwise "たまシッターをよべるのは
 * ひるのあいだだけだよ"); the sitter arrives (text line 0xA0), "あずける?" (leave the pet
 * with her? pet_sitter_question) -> pet_sitter_state.active = 1, the care timers are cleared and the menu
 * closes.
 */
void pet_sitter_call(void)
{
    u32 tbl[4];
    u8 *p;
    int a;

    p = chara_text(0, 100, 0xa0);
    memcpy(tbl, pet_sitter_hours_msg, 16);
    save.pet.discipline_flags &= ~1;
    if (pet_sitter_hours() != 0) {
        anim_sitter_arrive();
        ui_show_message(0xe, 0, p);
        if (pet_sitter_question() != 0) {
            pet_sitter_state.due = 0;
            pet_sitter_state.active = 1;
            ui_timeout = 0;
            save.pet.toilet_timer = 0;
            save.pet.cleaning_timer = 0;
            save.pet.bath_timer = 0;
            pet_clear_call(4);
        }
    } else {
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
        gfx_object_create(9, 0x40, 0x10, 0, 0, D_214D924);
        gfx_object_create(0x66, 0x40, 0x20, 0, 0, D_214D9B8);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x20, 0x30, (u8 *)tbl[0], 0, 9);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x30, 0x30, (u8 *)tbl[1], 0, 9);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x40, 0x30, (u8 *)tbl[2], 0, 9);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x50, 0x30, (u8 *)tbl[3], 0, 9);
        func_00009A6();
        btn_clear();
        ui_timeout = 10;
        for (;;) {
            if (btn_a_presses != 0)
                break;
            if (btn_b_presses != 0)
                break;
            if (btn_c_presses != 0)
                break;
            if (ui_timeout == 0)
                break;
            sys_idle_tick();
        }
    }
    btn_clear();
}

/*
 * Babysitter question ("あずける?" leave the pet?): yes/no.
 */
u8 pet_sitter_question(void)
{
    u32 tbl[3];
    u8 r = 0;
    int sel = 0;
    u8 a;
    int b;

    memcpy(tbl, pet_sitter_question_msg, 12);
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    a = gfx_object_create(6, 0x42, 0x10, 0, 0, D_2104714);
    b = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_centered(b, 0x20, 0x20, (u8 *)tbl[0], 0, 9);
    b = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_line(b, 0x18, 0x40, 0x20, tbl[1], 0, 2);
    b = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_line(b, 0x18, 0x50, 0x20, tbl[2], 0, 3);
    func_00009A6();
    btn_clear();
    ui_timeout = 10;
    do {
        if (btn_a_presses != 0) {
            sel = sel == 0;
            gfx_object_move_dirty(a, 6, sel * 16 + 0x42);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            r = sel == 0;
            if (pet_care_poops_at_entry != save.pet.poop_count) {
                if (save.pet.poop_count > 5 && pet_care_poops_at_entry <= 5)
                    pet_get_sick();
                pet_poops_drawn = save.pet.poop_count;
            }
            if (r == 0)
                break;
            if (save.pet.hunger != 0 && save.pet.happiness != 0 && save.pet.status == 0)
                break;
            snd_error_beep();
            r = 0;
            btn_b_presses = 0;
            ui_timeout = 10;
        }
        if (btn_c_presses != 0)
            break;
        sys_idle_tick();
    } while (ui_timeout != 0);
    btn_clear();
    return r;
}

/*
 * The sitter hands the pet back. While she is active and not due (x001 == 0): "いま
 * たまシッターに おせわをおねがいしてるよ" and pet_sitter_takeover_question (take over the care?), 0 when
 * declined. Then one of 16 report lines (text group 100, line 0xA1 + k) with its scene,
 * and pet_sitter_state.active = 0; returns 1.
 */
int pet_sitter_return(void)
{
    u8 *s[4];
    int a;
    u8 k;
    u8 *t;

    memcpy(s, pet_sitter_busy_msg, 16);
    if (pet_sitter_state.due == 0) {
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x20, 0x30, s[0], 0, 9);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x30, 0x30, s[1], 0, 9);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x40, 0x30, s[2], 0, 9);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x50, 0x30, s[3], 0, 9);
        func_00009A6();
        btn_clear();
        ui_timeout = 10;
        for (;;) {
            if (btn_a_presses != 0 || btn_b_presses != 0 || btn_c_presses != 0 || ui_timeout == 0)
                break;
            sys_idle_tick();
        }
        if (pet_sitter_takeover_question() == 0)
            return 0;
    }
    k = rand() % 16;
    t = chara_text(0, 100, k - 0x5f);
    if (k < 14) {
        anim_sitter_report();
        ui_show_message(15, 0, t);
    } else {
        anim_sitter_report2();
        ui_show_message(16, 0, t);
    }
    anim_sitter_leave();
    pet_sitter_state.active = 0;
    return 1;
}

/*
 * Sitter question "たまシッターと おせわをこうたい する/しない" (take over the care from the
 * sitter?): 1 for する, 0 for しない (the default), C or timeout.
 */
u8 pet_sitter_takeover_question(void)
{
    u32 tbl[4];
    u8 r = 0;
    int sel = 1;
    int h;
    u8 a;
    int b;

    memcpy(tbl, pet_sitter_takeover_msg, 16);
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    h = 0x10;
    a = gfx_object_create(6, 0x52, h, 0, 0, D_2104714);
    b = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_line(b, h, 0x20, 0x20, tbl[0], 0, 7);
    b = gfx_object_create(0, 0, 0, 0, 0, 0);
    h = 0x10;
    gfx_draw_text_line(b, h, 0x30, 0x20, tbl[1], 0, 8);
    b = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_line(b, 0x18, 0x40, 0x20, tbl[2], 0, 2);
    b = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_line(b, 0x18, 0x50, 0x20, tbl[3], 0, 3);
    func_00009A6();
    btn_clear();
    ui_timeout = 10;
    do {
        if (btn_a_presses != 0) {
            sel = sel == 0;
            gfx_object_move_dirty(a, 6, sel * 16 + 0x42);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            r = sel == 0 ? 1 : 0;
            break;
        }
        if (btn_c_presses != 0)
            break;
        sys_idle_tick();
    } while (ui_timeout != 0);
    btn_clear();
    return r;
}

/*
 * Sitter hours: 1 between 7 and 16 h; outside them a running babysitting is marked due
 * (pet_sitter_state.due = 1).
 */
u8 pet_sitter_hours(void)
{
    u8 m = rtc_get_hour();

    if (m >= 7 && m <= 16)
        return 1;
    if (pet_sitter_state.active != 0)
        pet_sitter_state.due = 1;
    return 0;
}

/*
 * 1 when the sitter is active and due (pet_sitter_state.active and x001).
 */
int pet_sitter_due(void)
{
    int r;

    goto start;
zero:
    r = 0;
    goto out;
start:
    if (pet_sitter_state.active == 0)
        goto zero;
    if (pet_sitter_state.due == 0)
        goto zero;
    r = 1;
out:
    return r;
}

/*
 * Care flags reset when the pet is healthy (x011 == 0): pet.discipline_flags cleared, a pending x024
 * event moved to pet_praise_expired.
 */
void pet_care_flags_reset(void)
{
    if (save.pet.status == 0) {
        save.pet.discipline_flags = 0;
        if (save.pet.praise_timer != 0)
            pet_praise_expired = 1;
        save.pet.praise_timer = 0;
    }
}

/*
 * The Tama-sitter scene overlay: sprites moved over two path tables (41 positions
 * room_sitter_path_a / room_sitter_path_b; the sitter, sprite set 94, and the pictures
 * 0x21A0A58 / 0x214DA4C) for frame a (0xFF = create the sprites); main draws it with the
 * room scene room_scene_sitter, chosen while the pet is with the sitter (pet_sitter_state).
 */
struct PathStep {
    s8 pose;
    u8 flags;
    s8 x;
    s8 y;
};
extern const struct PathStep room_sitter_path_a[41];
extern const struct PathStep room_sitter_path_b[41];
extern u8 *D_AF9C[][15] __asm__("chara_sprite_sets");
void room_sitter_overlay(u8 a)
{
    struct PathStep t1[41];
    struct PathStep t2[41];
    struct GfxObject *e;
    struct PathStep *t;
    u8 *p;
    u8 c;
    s16 x;

    memcpy(t1, room_sitter_path_a, 0xa4);
    memcpy(t2, room_sitter_path_b, 0xa4);
    if (a == 0xff) {
        t = &t2[0];
        p = (u8 *)D_21A0A58;
        c = room_overlay_base_a[gfx_object_count].palette_base + room_overlay_base_a[gfx_object_count].image[2];
        room_overlay_objs.obj0 = gfx_object_create(t->x - (p[0] >> 1), t->y - (p[1] >> 1) + 0x1c, c, t->pose, t->flags, (int)p);
        p = (u8 *)D_214DA4C;
        c = room_overlay_base_a[gfx_object_count].palette_base + room_overlay_base_a[gfx_object_count].image[2];
        room_overlay_objs.obj1 = gfx_object_create(-50 - (p[0] >> 1), 0x40 - (p[1] >> 1), c, 0, 0, (int)p);
        room_overlay_objs.obj2 = gfx_object_create(-50 - (p[0] >> 1), 0x40 - (p[1] >> 1), c, 0, 0x10, (int)p);
        p = D_AF9C[94][t1[0].pose];
        c = room_overlay_base_a[gfx_object_count].palette_base + room_overlay_base_a[gfx_object_count].image[2];
        room_overlay_objs.obj3 = gfx_object_create(t1[0].x - (p[0] >> 1), t1[0].y - (p[1] >> 1) + 0x1c, c, t1[0].pose, t1[0].flags, (int)p);
    } else {
        t = &t2[a];
        p = (u8 *)D_21A0A58;
        gfx_object_move_dirty(room_overlay_objs.obj0, t->x - (p[0] >> 1), t->y - (p[1] >> 1) + 0x1c);
        if (a == 0x1e || a == 0x20)
            x = 0x45;
        else
            x = -50;
        p = (u8 *)D_214DA4C;
        gfx_object_move_dirty(room_overlay_objs.obj1, x - (p[0] >> 1), 0x40 - (p[1] >> 1));
        if (a == 0x1e || a == 0x20)
            x = 0x61;
        else
            x = -50;
        gfx_object_move_dirty(room_overlay_objs.obj2, x - (p[0] >> 1), 0x40 - (p[1] >> 1));
        t = &t1[a];
        p = D_AF9C[94][t->pose];
        gfx_object_set_flags(room_overlay_objs.obj3, t->flags);
        gfx_object_move_dirty(room_overlay_objs.obj3, t->x - (p[0] >> 1), t->y - (p[1] >> 1) + 0x1c);
    }
}

/*
 * Overlay of the toilet-request room scenes (room_scene_toilet_pending / _call, while the
 * toilet urge runs): the thought bubble 0x2164EAC with the toilet medal D_20F44C8
 * (a == 0xFF creates the two sprites).
 */
void room_toilet_request_overlay(u8 a)
{
    struct GfxObject *e;
    u8 c;
    u8 *p;

    if (a == 0xff) {
        p = (u8 *)D_2164EAC;
        e = &room_overlay_base_b[gfx_object_count];
        c = e->palette_base + e->image[2];
        room_overlay_objs.obj0 = gfx_object_create(0x27 - (p[0] >> 1), 0x35 - (p[1] >> 1), c, 0, 0, (const u8 *)p);
        p = (u8 *)D_20F44C8;
        e = &room_overlay_base_b[gfx_object_count];
        c = e->palette_base + e->image[2];
        room_overlay_objs.obj1 = gfx_object_create(0x27 - (p[0] >> 1), 0x34 - (p[1] >> 1), c, 0, 0, (const u8 *)p);
    }
}

/*
 * Overlay of the bath-request room scenes (room_scene_bath_pending / _call, while the 19:00
 * bath request runs): the thought bubble with the bath medal D_20F45A8 (a == 0xFF creates
 * the two sprites).
 */
void room_bath_request_overlay(u8 a)
{
    struct GfxObject *e;
    u8 c;
    u8 *p;

    if (a == 0xff) {
        p = (u8 *)D_2164EAC;
        e = &room_overlay_base_c[gfx_object_count];
        c = e->palette_base + e->image[2];
        room_overlay_objs.obj0 = gfx_object_create(0x27 - (p[0] >> 1), 0x35 - (p[1] >> 1), c, 0, 0, (const u8 *)p);
        p = (u8 *)D_20F45A8;
        e = &room_overlay_base_c[gfx_object_count];
        c = e->palette_base + e->image[2];
        room_overlay_objs.obj1 = gfx_object_create(0x27 - (p[0] >> 1), 0x34 - (p[1] >> 1), c, 0, 0, (const u8 *)p);
    }
}

extern struct GfxObject *const room_overlay_base_d;
/*
 * Overlay sprites of the cleaning-request room scenes: the dirt pictures 0x214DC90 /
 * 0x214DD1C toggled by the frame parity while the cleaning habit is not learned
 * (pet.training[2]), and the thought bubble 0x2164EAC with the toy-box medal 0x20F4688
 * (asm).
 */
void room_cleaning_request_overlay(u8 a)
{
    struct GfxObject *e;
    u8 c;
    u8 b = a;
    u8 *p;
    int r;

    if (save.pet.training[2] != 0xff) {
        if (b == 0xff) {
            p = (u8 *)0x214dc90;
            e = &room_overlay_base_d[gfx_object_count];
            c = e->palette_base + e->image[2];
            room_overlay_objs.obj0 = gfx_object_create(0x29 - (p[0] >> 1), 0x38 - (p[1] >> 1), c, 0, 0x10, (int)p);
            p = (u8 *)0x2164eac;
            e = &room_overlay_base_d[gfx_object_count];
            c = e->palette_base + e->image[2];
            r = gfx_object_create(0x60 - (p[0] >> 1), 0x30 - (p[1] >> 1), c, 0, 0x10, (int)p);
            room_overlay_objs.obj1 = r;
            p = (u8 *)0x20f4688;
            e = &room_overlay_base_d[gfx_object_count];
            c = e->palette_base + e->image[2];
            room_overlay_objs.obj2 = gfx_object_create(0x60 - (p[0] >> 1), 0x2f - (p[1] >> 1), c, 0, 0, (int)p);
        } else {
            p = (u8 *)((b & 1) ? 0x214dd1c : 0x214dc90);
            c = gfx_objects[room_overlay_objs.obj0].palette_base;
            gfx_object_set_image(room_overlay_objs.obj0, c, 0, p);
        }
    } else if (b == 0xff) {
        p = (u8 *)0x2164eac;
        e = &room_overlay_base_d[gfx_object_count];
        r = 0;
        c = e->palette_base + e->image[2];
        room_overlay_objs.obj0 = gfx_object_create(0x27 - (p[0] >> 1), 0x35 - (p[1] >> 1), c, r, r, (int)p);
        p = (u8 *)0x20f4688;
        e = &room_overlay_base_d[gfx_object_count];
        c = e->palette_base + e->image[2];
        room_overlay_objs.obj1 = gfx_object_create(0x27 - (p[0] >> 1), 0x34 - (p[1] >> 1), c, r, r, (int)p);
    }
}

/*
 * Overlay of the praise-window room scene: the speech bubble 0x217F9B4 with the fixed text
 * "できたよ!" over the pet (asm).
 */
void room_praise_overlay(u8 a)
{
    u8 buf[] = "\x04\x45\x04\x07\x04\x10\x04\x26\x04\x5c";
    struct GfxObject *e;
    u8 c;
    u8 *p;
    int z;

    if (a == 0xff) {
        z = 0;
        p = (u8 *)0x217f9b4;
        e = &room_overlay_base_e[gfx_object_count];
        c = e->palette_base + e->image[2];
        room_overlay_objs.obj0 = gfx_object_create(0x2e - (p[0] >> 1), 0x35 - (p[1] >> 1), c, z, z, (int)p);
        p = (u8 *)0x2091748;
        e = &room_overlay_base_e[gfx_object_count];
        c = e->palette_base + e->image[2];
        room_overlay_objs.obj1 = gfx_object_create(z, z, z, z, z, z);
        a = room_overlay_objs.obj1;
        txt_render_line(a, 0x1d - (p[0] >> 1), 0x32 - (p[1] >> 1), c, buf, 1, 5);
    } else {
        p = (u8 *)0x2091748;
        a = room_overlay_objs.obj1;
        c = gfx_objects[a].palette_base;
        txt_render_line(a, 0x1d - (p[0] >> 1), 0x32 - (p[1] >> 1), c, buf, 1, 5);
    }
}

const u8 D_255CD48[] = _("たまシッターを");
const u8 D_255CD58[] = _("よべるのは");
const u8 D_255CD64[] = _("ひるのあいだ");
const u8 D_255CD74[] = _("だけだよ");
const u8 *const pet_sitter_hours_msg[] = { D_255CD48, D_255CD58, D_255CD64, D_255CD74 };
const u8 D_255CD90[] = _("あずける?");
const u8 D_255CD9C[] = _("はい");
const u8 D_255CDA4[] = _("いいえ");
const u8 *const pet_sitter_question_msg[] = { D_255CD90, D_255CD9C, D_255CDA4 };
const u8 D_255CDB8[] = _("いま");
const u8 D_255CDC0[] = _("たまシッターに");
const u8 D_255CDD0[] = _("おせわをおねがい");
const u8 D_255CDE4[] = _("してるよ");
const u8 *const pet_sitter_busy_msg[] = { D_255CDB8, D_255CDC0, D_255CDD0, D_255CDE4 };
const u8 D_255CE00[] = _("たまシッターと");
const u8 D_255CE10[] = _("おせわをこうたい");
const u8 D_255CE24[] = _("する");
const u8 D_255CE2C[] = _("しない");
const u8 *const pet_sitter_takeover_msg[] = { D_255CE00, D_255CE10, D_255CE24, D_255CE2C };
const struct PathStep room_sitter_path_a[] = {
    { 0, 0x10, 25, 50 }, { 0, 0x10, 25, 50 }, { 9, 0x10, 26, 50 }, { 2, 0x10, 25, 50 },
    { 0, 0x10, 25, 50 }, { 0, 0x10, 26, 50 }, { 0, 0x10, 25, 50 }, { 2, 0x10, 26, 50 },
    { 0, 0x10, 27, 50 }, { 9, 0, 15, 50 }, { 0, 0, 21, 50 }, { 9, 0, 30, 50 },
    { 9, 0x10, 26, 50 }, { 9, 0x10, 28, 50 }, { 9, 0x10, 26, 50 }, { 0, 0x10, 27, 50 },
    { 9, 0x10, 27, 50 }, { 0, 0x10, 27, 50 }, { 9, 0x10, 28, 50 }, { 0, 0x10, 27, 50 },
    { 9, 0x10, 28, 50 }, { 0, 0x10, 27, 50 }, { 9, 0x10, 27, 50 }, { 0, 0x10, 27, 50 },
    { 0, 0x10, 25, 50 }, { 0, 0x10, 24, 50 }, { 9, 0x10, 25, 50 }, { 0, 0, 32, 50 },
    { 0, 0, 24, 50 }, { 8, 0x10, 36, 50 }, { 0, 0x10, 53, 50 }, { 9, 0x10, 53, 50 },
    { 0, 0x10, 52, 50 }, { 9, 0x10, 53, 50 }, { 2, 0x10, 49, 50 }, { 0, 0x10, 49, 50 },
    { 2, 0x10, 49, 50 }, { 0, 0x10, 49, 50 }, { 8, 0, 50, 49 }, { 9, 0, 40, 50 },
    { 0, 0x10, 25, 50 },
};
const struct PathStep room_sitter_path_b[] = {
    { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 },
    { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 },
    { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -11, 56 }, { 0, 0, -1, 55 },
    { 0, 0, 28, 25 }, { 0, 0, 28, 26 }, { 0, 0, 28, 25 }, { 0, 0, 62, 56 },
    { 0, 0, 62, 56 }, { 0, 0, 62, 56 }, { 0, 0, 62, 56 }, { 0, 0, 62, 56 },
    { 0, 0, 62, 56 }, { 0, 0, 62, 56 }, { 0, 0, 62, 56 }, { 0, 0, 62, 56 },
    { 0, 0, 62, 56 }, { 0, 0, 62, 56 }, { 0, 0, 62, 56 }, { 0, 0, 0, 56 },
    { 0, 0, -8, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 },
    { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 },
    { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 }, { 0, 0, -50, 56 },
    { 0, 0, -50, 56 },
};
struct GfxObject *const room_overlay_base_a = gfx_objects - 1;
struct GfxObject *const room_overlay_base_b = gfx_objects - 1;
struct GfxObject *const room_overlay_base_c = gfx_objects - 1;
struct GfxObject *const room_overlay_base_d = gfx_objects - 1;
const u8 D_255CF9C[] = _("できたよ!");
struct GfxObject *const room_overlay_base_e = gfx_objects - 1;

/*
 * The RAM variables this file defines.
 */
struct Sitter pet_sitter_state;
u8 pet_poops_drawn;
u8 pet_praise_expired;
struct OverlayObjs room_overlay_objs DSTRAM;
u8 pet_care_poops_at_entry DSTRAM;
u8 pet_care_entry DSTRAM;
