/*
 * Storing photos in the album: free entry search, writing a record, and the 'storage full: pick
 * a photo to erase' dialog.
 *
 * Code 24154DC..2415804, .rodata from 255C258.
 */
#include "proto/album/album_store.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/diary/diary_notebook.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"

/*
 * Storage full: list the 6 saved pictures/letters (category 10 at 0x2770000, 6 x 0x2800
 * bytes) with texts D_255C25C.., let the user pick one to overwrite. Returns the slot or
 * -1.
 */
int item_pick_slot_to_overwrite(void)
{
    int sel;
    int cur;
    u8 *img;
    int i, p, o, q, k;
    u8 c, c2, t;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    t = gfx_object_create(8, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_centered(t, 0x20, 0x10, D_255C25C, 0, 9);
    t = gfx_object_create(8, 0x2f, 0x10, 0, 0, 0);
    gfx_draw_text_centered(t, 0x2f, 0x10, D_255C26C, 0, 9);
    t = gfx_object_create(8, 0x3e, 0x10, 0, 0, 0);
    gfx_draw_text_centered(t, 0x3e, 0x10, D_255C280, 0, 9);
    t = gfx_object_create(8, 0x4d, 0x10, 0, 0, 0);
    gfx_draw_text_centered(t, 0x4d, 0x10, D_255C290, 0, 9);
    func_00009A6();
    flash_backup_sector(0x7f);
    tmr_delay_ms_t5(2000);
    cur = album_prev_photo(0);
redraw:
    k = save.pet.photo_slots[cur] - 1;
    img = (u8 *)(k * 0x2800 + 0x2770000);
    ui_new_screen();
    c = gfx_object_create(0, 0x1c, 0, 0, 0, (const u8 *)img);
    c2 = gfx_object_create(0x72, 0x58, 0xb0, 0, 0, D_21059A4);
    if (cur > album_prev_photo(cur))
        gfx_object_set_image(c2, 0xb0, 0, D_21059A4);
    else
        gfx_object_set_image(c2, 0xb0, 1, D_21059A4);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    do {
        if (btn_a_presses != 0) {
            if (cur > 0)
                cur--;
            else
                cur = 5;
            k = save.pet.photo_slots[cur] - 1;
            img = (u8 *)(k * 0x2800 + 0x2770000);
            gfx_object_set_image(c, 0, 0, img);
            if (cur > album_prev_photo(cur))
                gfx_object_set_image(c2, 0xb0, 0, D_21059A4);
            else
                gfx_object_set_image(c2, 0xb0, 1, D_21059A4);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            sel = 1;
            switch (ui_yes_no(D_255C2A0, D_255C2AC, D_255C2B4, 0x10, &sel)) {
            case 1:
                if (sel != 0)
                    goto redraw;
                if (ui_confirm_really() != 0) {
                    snd_wait_end();
                    flash_restore_sector_except(0x7f, img, 0x2800);
                    for (i = cur; i <= 4; i++) {
                        p = (int)&save.pet;
                        o = 0xb4;
                        q = 0xb5;
                        *(u8 *)(i + p + o + 2) = *(u8 *)(i + p + q + 2);
                    }
                    save.pet.photo_slots[i] = 0;
                    return k;
                }
                if (ui_timeout == 0)
                    return -1;
                btn_b_presses = 0;
                btn_c_presses = 0;
                goto redraw;
            case 2:
                return -1;
            case 3:
                return -1;
            }
            goto redraw;
        }
        if (btn_c_presses != 0)
            return -1;
        sys_idle_tick();
    } while (ui_timeout != 0);
    return -1;
}

const u8 D_255C25C[] = _("これいじょう");
const u8 D_255C26C[] = _("ほぞんできないよ!");
const u8 D_255C280[] = _("どれをけすか");
const u8 D_255C290[] = _("えらんでね!");
const u8 D_255C2A0[] = _("どうする?");
const u8 D_255C2AC[] = _("けす");
const u8 D_255C2B4[] = _("けさない");

/*
 * Index of the first empty entry (0) of pet.x0B6[5].
 */
int album_free_photo_entry(void)
{
    int i;

    for (i = 0; i <= 4; i++)
        if (save.pet.photo_slots[i] == 0)
            break;
    return i;
}

/*
 * Store item/letter p into a free slot of category 10 (0x2770000, 0x2800-byte slots),
 * asking which one to replace when full (item_pick_slot_to_overwrite). Returns 1 if cancelled.
 */
int album_store_record(struct TamagoFile *p)
{
    int r = flash_first_erased_record(D_2770000, 0x2800, 6);

    if (r < 0) {
        r = item_pick_slot_to_overwrite();
        if (r < 0)
            return 1;
    }
    save.pet.photo_slots[album_free_photo_entry()] = r + 1;
    save_game();
    flash_program_record(p, D_2770000, 0x2800, r);
    return 0;
}
