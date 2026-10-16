/*
 * The care menu of the room (toilet, bath / teeth, cleaning locker, closet) and the cleaning
 * action.
 *
 * Code 250F1EE..250F4F2, .rodata from 255CA0C.
 */
#include "proto/pet/pet_care_room.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/chara/chara_draw.h"
#include "proto/pet/pet_bath.h"
#include "proto/pet/pet_toilet.h"
#include "proto/anim/anim_scripts.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"

/*
 * "Care" menu of the room (トイレ toilet, おふろ・はみがき bath / tooth brushing,
 * おそうじロッカー cleaning locker, クローゼット closet), with pages; the bath/toilet clean
 * the poops and the dirty state.
 */
int pet_care_room_menu(void)
{
    u32 tbl[4];
    int ret = 0;
    u8 z = 0;
    volatile u8 *w;
    u8 page;
    u8 sel;
    u8 last;
    u8 room;
    int redraw;
    u8 total = 4;
    u8 per = 4;
    u8 cur;
    u8 i;
    int idx;

    memcpy(tbl, pet_care_room_menu_labels, 16);
    page = 0;
    sel = 0;
    last = 0;
    room = save.pet.poop_count;
    redraw = 1;
    btn_clear();
    ui_timeout = 10;
again:
    w = &ui_timeout;
    if (redraw) {
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
        if (total > per)
            gfx_object_create(0x72, 0x58, 0x10, page < (total - 1) / per ? 0 : 1, z, D_21059A4);
        for (i = 0; i < per; i++) {
            idx = page * per + i;
            if (idx >= total)
                break;
            cur = gfx_object_create(0, 0, 0, 0, 0, 0);
            gfx_draw_text_line(cur, 0x18, i * 16 + 0x20, 0x20, tbl[idx], 0, 8);
        }
        cur = gfx_object_create(6, sel * 16 + 0x22, 0x30, z, z, D_2104714);
        func_00009A6();
        redraw = 0;
    }
    if (btn_a_presses) {
        btn_a_presses = 0;
        *w = 10;
        sel = sel < per - 1 ? sel + 1 : 0;
        page = sel / per;
        if (last != page) {
            last = page;
            redraw = 1;
        } else {
            gfx_object_move_dirty(cur, 6, sel * 16 + 0x22);
            func_00009A6();
        }
    }
    if (btn_b_presses) {
        ret = 1;
        switch (sel) {
        case 0:
            if (save.pet.poop_count != room || pet_poop_overflow != 0) {
                pet_poop_overflow = z;
                if (save.pet.poop_count > 5 && room <= 5)
                    pet_get_sick();
                ret = -1;
            } else {
                pet_toilet();
            }
            break;
        case 1:
            if (save.pet.status != 0) {
                snd_play(0xc);
                btn_b_presses = z;
                *w = 10;
                redraw = 1;
                goto again;
            }
            pet_bath();
            break;
        case 2:
            pet_cleaning();
            break;
        case 3:
            if (ui_wardrobe_menu_cleared() == 0) {
                *w = 10;
                redraw = 1;
                goto again;
            }
            break;
        }
        *w = 10;
    } else if (btn_c_presses == 0) {
        sys_idle_tick();
        if (*w != 0)
            goto again;
    }
    btn_clear();
    return ret;
}

/*
 * Cleaning: the おそうじロッカー entry of the care menu (case 2; a trained pet also does it
 * by itself, pet_care_event_dispatch): clears the mess levels of the five rooms x0BC..x0C0
 * (living room = days without tidying, kitchen, toilet, bath, bedroom), ends the 18:00
 * cleaning request pet.cleaning_timer, counts a child's cleaning training
 * pet.training[2] and opens the praise window (300 s).
 */
void pet_cleaning(void)
{
    int f = 0;

    if (save.pet.cleaning_timer != 0)
        f = 1;
    save.pet.kitchen_dirt = 0;
    save.pet.toilet_dirt = 0;
    save.pet.living_dirt = 0;
    save.pet.bedroom_dirt = 0;
    save.pet.bath_dirt = 0;
    anim_cleaning();
    if (f) {
        if (save.pet.life_stage == 2 && save.pet.training[2] != 0xff)
            save.pet.training[2]++;
        save.pet.discipline_flags |= 2;
        save.pet.praise_timer = 300;
        save.pet.cleaning_timer = 0;
        pet_cleaning_expired = 0;
        pet_praise_expired = 0;
    }
    pet_add_happiness(1);
    save_game();
}

/*
 * Accessory helper ui_wardrobe_menu with the buttons cleared.
 */
int ui_wardrobe_menu_cleared(void)
{
    int v0;

    v0 = ui_wardrobe_menu();
    btn_clear();
    return v0;
}

const u8 D_255CA10[] = _("トイレ");
const u8 D_255CA18[] = _("おふろ・はみがき");
const u8 D_255CA2C[] = _("おそうじロッカー");
const u8 D_255CA40[] = _("クローゼット");
const u8 *const pet_care_room_menu_labels[4] = { D_255CA10, D_255CA18, D_255CA2C, D_255CA40 };

/*
 * The RAM variables this file defines.
 */
u8 pet_cleaning_expired;
u8 pet_poop_overflow DSTRAM;
