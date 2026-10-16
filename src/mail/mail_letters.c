/*
 * Letters: letters from friends and the farewell letter, the letter box with item letters and
 * their descriptions, read flags, and the coupon box.
 *
 * Code 241AF9A..241B8A2, .rodata from 255F2D4.
 */
#include "proto/mail/mail_letters.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/shop/shop_browser.h"
#include "proto/diary/diary_notebook.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"

/*
 * F55 = HAPPY MAIL (ハッピーメール): letters from friends stored as items in bank 9
 * (item_bank_tbl[9]: x003 slots of x000 bytes from sector x002: 40 slots of 1 KB at
 * 0x2740000; text at +0x200, read marks = flags 0..0x27) and the letter / coupon items of
 * the inventory item_inv_stamps.
 *
 * mail_farewell_letter: the farewell letter scene from a friend who left, the text lines
 * D_255F5AC.. ("もっといっぱいかまって ほしかった…… さみしかった…… こんどあえたら
 * そのときはもっともっと あそぼうね……") shown around the pet.
 */
void mail_farewell_letter(void)
{
    anim_scene_room(0xc);
    anim_draw_pic(0, D_2105A24, 0, 0x40, 0x24);
    anim_draw_text(1, D_255F5AC, 1, 10, 0x40);
    anim_draw_text(2, D_255F5BC, 1, 10, 0x35);
    anim_draw_text(3, D_255F5D4, 1, 10, 0x2a);
    anim_draw_text(4, D_255F5E4, 1, 10, 0x1f);
    anim_draw_text(5, D_255F5F8, 1, 4, 0);
    anim_draw_text(6, D_255F5FC, 1, 10, 0x14);
    anim_draw_text(7, D_255F610, 1, 10, 9);
    anim_draw_pet(8, 3, 0, 0x68, 0x1c);
    anim_draw_pic(0xb, D_2106C34, 0, 0x68, 3);
    anim_busy = 0;
    func_00009A6();
}

/*
 * Show letter item p: three pictures already loaded from its record (anim_pic_slots) and its
 * text lines (at +0x200, lines separated by 0xFF).
 */
void mail_show_letter_item(struct TamagoFile *p)
{
    u8 *img;
    u8 *s;
    int i, y;
    u8 a;

    item_load_stamp_pictures();
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2092D60);
    gfx_object_create(0x16 - ((anim_pic_slots[2][1])[0] >> 1), 0x4a - ((anim_pic_slots[2][1])[1] >> 1), 0x10, 0, 0, anim_pic_slots[2][1]);
    gfx_object_create(0x40 - ((anim_pic_slots[2][2])[0] >> 1), 0x4a - ((anim_pic_slots[2][2])[1] >> 1), 0x40, 0, 0, anim_pic_slots[2][2]);
    gfx_object_create(0x6b - ((anim_pic_slots[3][0])[0] >> 1), 0x4a - ((anim_pic_slots[3][0])[1] >> 1), 0x70, 0, 0, anim_pic_slots[3][0]);
    s = (u8 *)p + 0x200;
    for (i = 0; *s != 0; i++) {
        y = i * 11 + 0x1e;
        a = gfx_object_create(7, y, 0xa0, 0, 0, 0);
        s = (u8 *)gfx_draw_text_line(a, 7, y, 0xa0, (int)s, 1, 0xd);
        if (*s == 0xff)
            s++;
    }
    func_00009A6();
}

/*
 * An item's description card p: its picture (item_load_pictures), its name (0x5A) and its text
 * (+0x64), or the name at 0x5E and the text at +0x200 for 0x81 / category-6 letters.
 */
void item_description_card(struct TamagoFile *p)
{
    const u8 *img;
    u8 *s;
    int i, y;
    u8 a;
    s16 w;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2105A24);
    item_load_pictures(anim_pic_slots[0], p, 1);
    img = anim_pic_slots[0][0];
    w = img[1];
    w = 0x31 - (w >> 1 & 0x7fff);
    gfx_object_create(0x58, w, 0x10, 0, 0, (const u8 *)img);
    a = gfx_object_create(7, 0x2b, 0x30, 0, 0, 0);
    if (p->x04e == 0x81) {
        if (p->x04f == 6) {
            gfx_draw_text_line(a, 7, 0x20, 0x30, (int)&p->name, 1, 9);
            s = (u8 *)p + 0x200;
            goto loop;
        }
    }
    gfx_draw_text_line(a, 7, 0x20, 0x30, (int)&p->x05a, 1, 9);
    s = &p->legacy_text;
loop:
    for (i = 0; *s != 0; i++) {
        y = i * 11 + 0x2b;
        a = gfx_object_create(7, y, 0x30, 0, 0, 0);
        s = (u8 *)gfx_draw_text_line(a, 7, y, 0x30, (int)s, 1, 0xd);
        if (*s == 0xff)
            s++;
    }
    func_00009A6();
}

/*
 * Letter from friend k: friend picture, decorations, one of eight random template texts
 * mail_letter_templates[1..8] (D_255F2D8..: "このあいだはアリガトウ♥ ツーしんは たのしかったよ♪
 * またあそぼうね～" ...) and the friend's name.
 */
void mail_letter_from_friend(int k)
{
    u8 *s;
    int i, y;
    u8 a;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_208C218);
    friend_draw_portrait(k, 0x60, 0x5c, 0x10);
    gfx_object_create(0x47, 0x1e, 0x58, 0, 0, D_2196134);
    gfx_object_create(0x45, 0x58, 0x58, 0, 0, D_2196134);
    s = (u8 *)mail_letter_templates[rand() % 8 + 1];
    for (i = 0; *s != 0; i++) {
        y = i * 11 + 0x20;
        a = gfx_object_create(7, y, 0x30, 0, 0, 0);
        s = (u8 *)gfx_draw_text_line(a, 7, y, 0x30, (int)s, 1, 0xd);
        if (*s == 0xff)
            s++;
    }
    a = gfx_object_create(0x1c, 0x57, 0x30, 0, 0, 0);
    gfx_draw_text_line(a, 0x1c, 0x57, 0x30, (int)&save.friend_list[k].name[0], 1, 5);
    func_00009A6();
}

/*
 * Read mark of letter slot a (flags 0..0x27 of the achievement flag bitmap); mail_set_letter_read
 * sets it.
 */
int mail_letter_read_flag(int a)
{
    if ((u32)a > 0x27)
        return 0;
    return pet_get_unlock_flag(a);
}

/*
 * Set the letter read flag a.
 */
void mail_set_letter_read(int a, u8 b)
{
    if ((u32)a <= 0x27)
        pet_set_unlock_flag(a, b);
}

/* not const: mail_unread_letter_count/241B448 reload the slot count in their loops */
/*
 * Number of unread downloaded letters (category 9, item_bank_tbl[9].slot_count slots).
 */
int mail_unread_letter_count(void)
{
    int n = 0;
    int i;

    for (i = 0; i < item_bank_tbl[9].slot_count; i++) {
        if (*(int *)item_bank_slot(9, i) == -1)
            continue;
        if (mail_letter_read_flag(i) == 0)
            n++;
    }
    return n;
}

/*
 * Number of stored downloaded letters (category 9: 40 slots of 1 KB at 0x2740000).
 */
int mail_letter_count(void)
{
    int n = 0;
    int i;

    for (i = 0; i < item_bank_tbl[9].slot_count; i++)
        if (*(int *)item_bank_slot(9, i) != -1)
            n++;
    return n;
}

/*
 * Next stored letter after k.
 */
int mail_next_letter(int k)
{
    do {
        if (++k >= item_bank_tbl[9].slot_count)
            k = 0;
    } while (*(int *)item_bank_slot(9, k) == -1);
    return k;
}

/*
 * Letter box (てがみ) browser: envelope + sender name per slot, A = next, B = "よむ? /
 * no / delete" (read shows item_description_card and marks the slot read; delete rewrites the
 * sector), C / timeout -1 (0 when the last letter was deleted).
 */
int mail_letter_box(void)
{
    u32 out;
    struct TamagoFile *p;
    int idx;
    int more;
    u8 a;
    int c, w;

    more = 1;
    switch (mail_letter_count()) {
    case 0:
        snd_error_beep();
        btn_a_presses = 1;
        btn_c_presses = 1;
        return -1;
    case 1:
        more = 0;
    }
    idx = (u8)mail_next_letter(item_bank_tbl[9].slot_count);
draw:
    p = (struct TamagoFile *)item_bank_slot(9, idx);
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    gfx_object_create(0x34, 0x24, 0x10, 0, 0, D_208DA68);
    a = gfx_object_create(0, 0x41, 0x30, 0, 0, 0);
    item_draw_name(p, a, 0x41, 0x30);
    if (more != 0) {
        if (idx < mail_next_letter(idx))
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
        else
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    for (;;) {
        if (btn_a_presses != 0) {
            if (more != 0) {
                idx = (u8)mail_next_letter(idx);
                goto draw;
            }
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            out = 0;
            switch ((u32)ui_use_no_delete((int)D_255F624, 0, &out)) {
            case 2:
                goto draw;
            case 3:
                return -1;
            case 1:
                switch (out) {
                case 0:
                    item_description_card(p);
                    mail_set_letter_read(idx, 1);
                    btn_a_presses = 0;
                    btn_b_presses = 0;
                    btn_c_presses = 0;
                    ui_timeout = 10;
                    for (;;) {
                        if (btn_a_presses != 0) {
                            ui_timeout = 10;
                            btn_a_presses = 0;
                        }
                        if (btn_b_presses != 0) {
                            ui_timeout = 10;
                            goto draw;
                        }
                        if (btn_c_presses != 0) {
                            ui_timeout = 10;
                            goto draw;
                        }
                        sys_idle_tick();
                        if (ui_timeout == 0)
                            return -1;
                    }
                case 1:
                    goto draw;
                case 2:
                    if (ui_confirm_really() != 0) {
                        c = item_bank_tbl[9].first_sector;
                        w = item_bank_tbl[9].slot_size;
                        snd_wait_end();
                        c += (u32)(w * idx) >> 16;
                        flash_backup_sector(c);
                        flash_restore_sector_except(c, p, w);
                        if (item_bank_count(9, 0) > 0)
                            return -1;
                        return 0;
                    }
                    if (ui_timeout == 0)
                        return -1;
                    btn_b_presses = 0;
                    btn_c_presses = 0;
                    goto draw;
                }
            }
        }
        if (btn_c_presses != 0) {
            ui_timeout = 10;
            return -1;
        }
        if (ui_timeout == 0)
            return -1;
        sys_idle_tick();
    }
}

/*
 * Returns 0 (callback stub).
 */
int func_241B738(void)
{
    return 0;
}

/*
 * The mail_letter_box browser for the letter / coupon items of the inventory list item_inv_stamps
 * (item 0x52): read shows mail_show_letter_item, delete clears the ownership count.
 */
int mail_coupon_box(void)
{
    int a;
    u32 b;

top:
    ui_new_screen();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    a = -1;
    if (item_inv_usable_count(&item_inv_stamps) == 0) {
        snd_error_beep();
        btn_b_presses = 0;
        ui_timeout = 10;
        return -1;
    }
    if (ui_timeout == 0)
        return 0;
    goto again;
set:
    ui_timeout = 10;
again:
    if (item_inv_picker(&item_inv_stamps, 0, &a) < 0)
        goto fail;
    b = 0;
    switch ((u32)ui_use_no_delete((int)D_255F624, 0, &b)) {
    case 2:
        goto again;
    case 1:
        switch (b) {
        case 0:
            mail_show_letter_item(item_record_by_index(0x52));
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            ui_timeout = 10;
            for (;;) {
                if (btn_a_presses) {
                    ui_timeout = 10;
                    btn_a_presses = 0;
                }
                if (btn_b_presses || btn_c_presses)
                    goto set;
                sys_idle_tick();
                if (ui_timeout == 0)
                    return 0;
            }
        case 1:
            goto again;
        case 2:
            if (ui_confirm_really()) {
                item_set_count(0x52, 0);
                btn_b_presses = 0;
                btn_c_presses = 0;
                goto top;
            }
            if (ui_timeout == 0)
                return 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            goto again;
        }
        return -1;
    case 3:
    default:
        goto fail;
    }
fail:
    return -1;
}

const u8 D_255F2D8[] = _("このあいだはアリガトウ♥\nツーしんは\nたのしかったよ♪\nまたあそぼうね～");
const u8 D_255F324[] = _("はあ～い@@さいきんきに\nなるアイテムがあるんだけ\nど､2つもってた\nりしないかな～?\0");
const u8 D_255F380[] = _("やっほー↑げんきしてる?\nまたこんどあそびたいなっ\nておもっててがみ\nをおくったよ★");
const u8 D_255F3D8[] = _("ひさしぶり!こんどおしゃ\nべりしながらプロフこうか\nんしようね!↑↑\nやくそくだよ★");
const u8 D_255F430[] = _("ハロー♪ハロー♪♪\nあさのこうえんでしかやっ\nてないおみせのウ\nワサ､しってる?");
const u8 D_255F484[] = _("こんど､ふたりのあいしょ\nううらないしたいな～♥♥\nどんなけっかがで\nるかたのしみ♪");
const u8 D_255F4DC[] = _("ねぇねぇ!きょうはなにし\nてるの?よかったらまたあ\nそびたいなって､\nおもってるんだ★\0");
const u8 D_255F538[] = _("いっしょにしゃしんをとっ\nて､ステキなおもいでをの\nこしたいんだけど\nどうかな?");
const u8 *const mail_letter_templates[8] = {
    D_255F2D8, D_255F324, D_255F380, D_255F3D8, D_255F430, D_255F484, D_255F4DC, D_255F538,
};
const u8 D_255F5AC[] = _("あそぼうね……");
const u8 D_255F5BC[] = _("そのときはもっともっと");
const u8 D_255F5D4[] = _("こんどあえたら");
const u8 D_255F5E4[] = _("さみしかった……");
const u8 D_255F5F8[] = _("");
const u8 D_255F5FC[] = _("ってほしかった……");
const u8 D_255F610[] = _("もっといっぱいかま");
const u8 D_255F624[] = _("よむ?");
const u8 D_255F62C[] = _("てがみ");
const u8 D_255F634[] = _("ハッピーメール");
/* unreferenced */
const u8 *const D_255F644[2] = { D_255F62C, D_255F634 };

/*
 * Letter box screen wrapper (mail_letter_box) with the "no letters" sound.
 */
int mail_letter_box_screen(void)
{
    ui_new_screen();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    if (mail_letter_box()) {
        snd_error_beep();
        btn_b_presses = 0;
        ui_timeout = 10;
    } else if (ui_timeout == 0) {
        return 1;
    }
    ui_timeout = 10;
    if (btn_c_presses != 0)
        return 0;
    sys_idle_tick();
    if (ui_timeout == 0)
        return 0;
    return 0;
}

