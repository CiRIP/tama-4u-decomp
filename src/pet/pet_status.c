/*
 * The pet status pages: hearts, profile, marks and clover.
 *
 * Code 2510E24..2511372, .rodata from 255D024.
 */
#include "proto/pet/pet_status.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/chara/chara_draw.h"
#include "proto/evo/evo_special.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"

/*
 * Submenu (entry 0 of ui_status_menu) with a two-choice cursor.
 */
int ui_status_browser(void)
{
    int r = 0;
    u8 sel = 0;
    int redraw = 1;

    btn_a_presses = sel;
    btn_b_presses = 1;
    btn_c_presses = sel;
    ui_timeout = 10;
    do {
        if (redraw) {
            switch (sel) {
            case 0:
                ui_status_page_hearts(1);
                break;
            case 1:
                ui_status_page_profile(1);
                break;
            case 2:
                ui_status_page_marks(1);
                break;
            case 3:
                pet_status_page_clover(1);
                break;
            }
            redraw = 0;
        }
        if (btn_a_presses != 0) {
            sel = sel > 2 ? 0 : sel + 1;
            redraw = 1;
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_c_presses != 0)
            goto out;
        sys_idle_tick();
    } while (ui_timeout != 0);
    r = -1;
out:
    btn_clear();
    return r;
}

/*
 * F46 = the STATUS pages (キャラパラメータ); pet_status_pages(k) dispatches page k:
 * 0 = hunger / happiness hearts with the character (ui_status_page_hearts: hunger x005 icons,
 * happiness x00c in fifths, the character drawn undressed and without state flags),
 * 1 = name, gender, age pet.age_days, weight pet.weight, Gotchi points (ui_status_page_profile), 2 = care
 * marks (bytes 0x1AC..0x1AE == 0xFF) and the five club masteries 0x217..0x21B
 * (ui_status_page_marks), 3 = the four-leaf clover (pet.clover_leaves; the full clover when
 * all four are set): the names of the three liked items that filled leaves (+0x1AF / +0x1C3
 * / +0x1D7) and "まいにちおせわ" (daily care) for the fourth, "???????" when empty
 * (pet_status_page_clover). The leaves unlock nothing; all four are needed for the album
 * registration and the life story.
 */
void pet_status_pages(u8 k)
{
    switch (k) {
    case 0:
        ui_status_page_hearts(0);
        break;
    case 1:
        ui_status_page_profile(0);
        break;
    case 2:
        ui_status_page_marks(0);
        break;
    case 3:
        pet_status_page_clover(0);
        break;
    }
}

/*
 * Status page with up to 21 entries.
 */
void ui_status_page_hearts(u8 a)
{
    u32 tbl[21];
    u8 i;
    u8 b;
    u16 s0;
    u8 s1, s2, s3, s4;

    memcpy(tbl, ui_status_happiness_pics, 0x54);
    i = 0;
    ui_new_screen();
    gfx_object_create(i, 0x1c, i, i, i, D_20E3204);
    if (a != 0)
        gfx_object_create(0x72, 0x58, 0x10, 0, 0, D_21059A4);
    for (i = 0; i < save.pet.hunger; i++)
        gfx_object_create(i * 13 + 0x2d, 0x21, 0x20, 0, 0, D_20EF348);
    i = save.pet.happiness != 0 ? (save.pet.happiness + 4) / 5 : 0;
    gfx_object_create(0x43, 0x2f, 0x30, 0, 0, (const u8 *)tbl[i]);
    gfx_object_create(0x43, 0x2f, 0x60, 0, 0, (const u8 *)(save.pet.life_stage <= 2 ? 0x20e441c : 0x20e4918));
    s0 = save.pet.x000;
    s1 = room_view;
    s2 = save.pet.worn_accessory;
    s3 = save.pet.worn_clothes;
    s4 = save.pet.x062;
    room_view = 0;
    save.pet.worn_accessory = 0;
    save.pet.worn_clothes = 0;
    save.pet.x062 = 0;
    i = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_object_create(0, 0, 0, 0, 0, 0);
    chara_draw_pet(i, 0x70, save.pet.happiness > 0x59 ? 2 : 0, 0, 0x5c, 0x2d);
    room_view = s1;
    save.pet.worn_accessory = s2;
    save.pet.worn_clothes = s3;
    save.pet.x062 = s4;
    pet_set_character_id(s0);
    func_00009A6();
    anim_busy = 0;
}

/*
 * Status page (background 0x20EF3A0).
 */
void ui_status_page_profile(u8 a)
{
    u8 c;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_20EF3A0);
    if (a != 0)
        gfx_object_create(0x72, 0x58, 0x10, 0, 0, D_21059A4);
    c = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_line(c, 10, 0x20, 0x20, (int)chara_text(save.pet.x000, 1, 0), 0, 9);
    c = pet_get_gender();
    gfx_object_create(10, 0x31, 0x30, 0, 0, (const u8 *)(c == 0 ? 0x20f4bc4 : 0x20f4cec));
    gfx_draw_number(0x5d, 0x31, 0x40, save.pet.age_days);
    gfx_draw_number(0x5d, 0x41, 0x40, save.pet.weight);
    gfx_draw_number(0x5d, 0x52, 0x40, save.pet.gotchi_points);
    func_00009A6();
}

/*
 * Status page with pet values (background 0x20F05C0).
 */
void ui_status_page_marks(u8 a)
{
    u8 *p = (u8 *)&save.pet;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_20F05C0);
    if (a != 0)
        gfx_object_create(0x72, 0x58, 0x10, 0, 0, D_21059A4);
    if (p[0x1ac] == 0xff)
        gfx_object_create(5, 0x2b, 0x20, 0, 0, D_20F44C8);
    if (p[0x1ad] == 0xff)
        gfx_object_create(0x1b, 0x2b, 0x30, 0, 0, D_20F45A8);
    if (p[0x1ae] == 0xff)
        gfx_object_create(0x31, 0x2b, 0x40, 0, 0, D_20F4688);
    if (p[0x217] != 0)
        gfx_object_create(5, 0x4a, 0x50, 0, 0, D_20F4768);
    if (p[0x218] != 0)
        gfx_object_create(0x1b, 0x4a, 0x60, 0, 0, D_20F4848);
    if (p[0x219] != 0)
        gfx_object_create(0x31, 0x4a, 0x70, 0, 0, D_20F4928);
    if (p[0x21a] != 0)
        gfx_object_create(0x47, 0x4a, 0x80, 0, 0, D_20F4A04);
    if (p[0x21b] != 0)
        gfx_object_create(0x5d, 0x4a, 0x90, 0, 0, D_20F4AE4);
    func_00009A6();
}

/*
 * Status page with the pet's name/values (default text D_255D07C).
 */
void pet_status_page_clover(u8 a)
{
    u8 buf[0x14];
    const u8 *def = D_255D07C;
    u8 *base = (u8 *)&save.pet;
    u8 i;
    const u8 *src;
    u8 c;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_20F17D8);
    if (save.pet.clover_leaves == 0xf)
        gfx_object_create(0x26, 0x27, 0x10, 0, 0, D_20F2C88);
    if (a != 0)
        gfx_object_create(0x72, 0x58, 0x20, 1, 0, D_21059A4);
    for (i = 0; i <= 3; i++) {
        src = def;
        if ((save.pet.clover_leaves >> i) & 1) {
            switch (1 << i) {
            case 1:
                src = base + 0x1af;
                break;
            case 2:
                src = base + 0x1c3;
                break;
            case 4:
                src = base + 0x1d7;
                break;
            case 8:
                src = D_255D090;
                break;
            }
            memset(buf, 0, 0x14);
            memcpy(buf, src, 0x12);
            c = gfx_object_create(0, 0, 0, 0, 0, 0);
            gfx_draw_text_line(c, 7, i * 16 + 0x21, 0x30, (int)src, 0, 9);
        }
    }
    func_00009A6();
}

extern u8 D_20E4E14[];
extern u8 D_20E530C[];
extern u8 D_20E5808[];
extern u8 D_20E5D04[];
extern u8 D_20E6200[];
extern u8 D_20E6700[];
extern u8 D_20E6C04[];
extern u8 D_20E710C[];
extern u8 D_20E7AF8[];
extern u8 D_20E84E8[];
extern u8 D_20E8EE0[];
extern u8 D_20E98D8[];
extern u8 D_20EA2D4[];
extern u8 D_20EACD4[];
extern u8 D_20EB6DC[];
extern u8 D_20EC0E4[];
extern u8 D_20ECAF0[];
extern u8 D_20ED500[];
extern u8 D_20EDF14[];
extern u8 D_20EE92C[];
const u8 *const ui_status_happiness_pics[21] = {
    0, D_20E4E14, D_20E530C, D_20E5808, D_20E5D04, D_20E6200, D_20E6700, D_20E6C04,
    D_20E710C, D_20E7AF8, D_20E84E8, D_20E8EE0, D_20E98D8, D_20EA2D4, D_20EACD4, D_20EB6DC,
    D_20EC0E4, D_20ECAF0, D_20ED500, D_20EDF14, D_20EE92C,
};
const u8 D_255D07C[] = _("???????{00}{00}");
const u8 D_255D090[] = _("まいにちおせわ{00}{00}");
