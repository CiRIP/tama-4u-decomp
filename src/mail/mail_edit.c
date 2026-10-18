/*
 * Friend messages: the message card, loading and editing a message.
 *
 * Code 2518760..251926E, .rodata from 255D5C0.
 */
#include "proto/mail/mail_edit.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/diary/diary_notebook.h"
#include "proto/comm/comm_menu.h"
#include "proto/txt/txt_input.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"
#include "libc.h"

u8 mail_count_for_friend(int id)
{
    u8 *p;
    int n, i;

    p = (u8 *)mail_records;
    n = i = 0;
    for (; i < 100; i++, p += 0x64) {
        if (*(int *)p == id && *(int *)(p + 4) >= 0)
            n++;
    }
    return n;
}

/*
 * Message record editing helper (k = 0: load a message into buf).
 */
int mail_card(u8 k, int a, int b, int c, int d)
{
    u8 buf[0x50];
    int i;
    int r;
    int ret;
    int j;

    if (k == 0) {
        mail_load_message(a, (int)buf);
        if (save.mail_unread_pending == 1) {
            if (c == -1)
                mail_draw_card(a, b, 0xff, (int)buf, d);
            else
                mail_draw_card(a, b, c, (int)buf, d);
        } else if (mail_count_for_friend(b) > 1)
            mail_draw_card(a, b, c, (int)buf, d);
        else
            mail_draw_card(a, b, 0xff, (int)buf, d);
        ret = -1;
        goto out;
    }
    i = 0;
    r = -1;
    for (; i <= 0x3f; i++) {
        if (((u8 *)(i + a))[0x1c] != 0) {
            r = 0;
            break;
        }
    }
    if (r == 0) {
        for (;;) {
            mail_load_message(a, (int)buf);
            mail_draw_card(a, b, 0xff, (int)buf, d);
            btn_a_presses = 1;
            btn_b_presses = 0;
            btn_c_presses = 0;
            ui_timeout = 30;
            for (;;) {
                if (ui_timeout == 0) {
                    ui_timeout = 0;
                    sys_display_state = 2;
                    sys_set_backlight(0);
                    sys_apply_backlight();
                    ret = -2;
                    goto out;
                }
                if (btn_a_presses)
                    ;
                if (btn_b_presses)
                    break;
                if (btn_c_presses) {
                    btn_c_presses = 0;
                    ui_timeout = 10;
                    ret = -1;
                    goto out;
                }
                sys_idle_tick();
            }
            btn_b_presses = 0;
            r = mail_edit_dialog(0);
            if (r == 0)
                break;
            if (r == -2) {
                ret = r;
                goto out;
            }
            sys_idle_tick();
        }
    }
    for (;;) {
        mail_load_message(a, (int)buf);
        r = mail_edit_message((int)buf, d);
        if (r != 0) {
            if (r == -1) {
                ret = r;
                goto out;
            }
            if (((s8)save.pet.calls < 0 && pet_sleep_state() != 3) || (save.pet.calls & 0x70)) {
                sys_idle_seconds = 0;
                sys_display_state = 0;
                ret = -2;
                goto out;
            }
            sys_display_state = 2;
            sys_set_backlight(0);
            sys_apply_backlight();
            ret = -2;
            goto out;
        }
        for (j = 0; j <= 0x3f; j++)
            ((u8 *)(j + a))[0x1c] = buf[j];
        ret = 0;
        goto out;
    }
out:
    return ret;
}

/*
 * F52 = the MESSAGE editor / card for the friend mail (F53 mail_card uses it).
 *
 * Edit the message at a (0x50 bytes: 4 editable lines of 16 characters plus a fifth line
 * that is only shown; b = the character picture): A moves down a line (past the last
 * line: save when something was written and changed, else the stop-editing question), B
 * opens the text editor txt_input(4, 6..9) for the line, C goes up a line (on the
 * first line "へんしゅうやめる? はい/いいえ" = stop editing?, mail_edit_dialog(1)). Returns 0
 * saved, -1 cancelled, -2 timeout / display off. Blank pairs are normalised to D_255D5C8.
 */
int mail_edit_message(int a, int b)
{
    u8 tmp[17];
    u8 buf[5][16];
    u8 kept[5][16];
    u8 sel;
    int i;
    int r = 0;
    int redraw;
    s16 y;
    u8 *p;
    u8 *q;
    const u8 *img;
    int n;
    u8 k;

    memcpy(buf, (void *)a, 0x50);
    sel = 0;
    for (i = 0; i < 0x40; i += 2) {
        p = (u8 *)buf + i;
        if (memcmp(p, D_255D5C4, 2) == 0)
            memcpy(p, D_255D5C8, 2);
    }
    btn_clear();
    memcpy(kept, buf, 0x50);
again:
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_212393C);
    y = 0x1a;
    switch (save.pet.life_stage) {
    case 1:
    case 2:
    case 3:
        y -= 4;
        break;
    case 4:
        y += 1;
        break;
    }
    gfx_object_create(-2, y, 0x10, 0, 0x10, (const u8 *)b);
    img = D_21237E0;
    gfx_object_create(0x19 - (img[0] >> 1), 0x26 - (img[1] >> 1), 0x40, 0, 0, (const u8 *)img);
    for (i = 0; i <= 4; i++) {
        q = &mail_card_line_objs[i];
        *q = gfx_object_create(0, 0, 0x50, 0, 0, 0);
    }
    redraw = 1;
    for (;;) {
        if (sys_display_state != 0) {
            r = -2;
            goto out;
        }
        if (redraw) {
            for (i = 0; i <= 4; i++) {
                p = buf[i];
                gfx_object_set_flags(mail_card_line_objs[i], (i == sel && i < 4) ? 0x2000 : 0);
                gfx_draw_text_line(mail_card_line_objs[i], 0x35, i * 11 + 0x21, 0x50, (int)p, 1, 8);
            }
            func_00009A6();
            redraw = 0;
        }
        if (btn_a_presses != 0) {
            if (sel > 2) {
                for (i = 0; i < 0x40; i += 2)
                    if (memcmp((u8 *)buf + i, D_255D5C8, 2) != 0)
                        break;
                if (i < 0x40) {
                    if (memcmp(kept, buf, 0x50) != 0) {
                        r = 0;
                        memcpy((void *)a, buf, 0x50);
                        goto out;
                    }
                }
                r = -1;
                goto cancel;
            } else {
                sel++;
                btn_a_presses = 0;
                redraw = 1;
            }
        }
        if (btn_b_presses != 0) {
            p = buf[sel];
            memset(tmp, 0, 0x11);
            memcpy(tmp, p, 0x10);
            switch (sel) {
            case 1:
                k = 7;
                break;
            case 2:
                k = 8;
                break;
            case 3:
                k = 9;
                break;
            default:
                k = 6;
            }
            n = txt_input(4, k, (int)tmp);
            if (n > 0) {
                memcpy(p, tmp, 0x10);
                if (sel <= 2)
                    sel++;
                else
                    btn_a_presses = 1;
            } else if (n < 0) {
                ui_timeout = 0;
                r = -2;
                goto out;
            }
            btn_b_presses = 0;
            goto again;
        }
        if (btn_c_presses != 0) {
            if (sel == 0) {
            cancel:
                r = mail_edit_dialog(1);
                if (r == -2)
                    goto out;
                if (r == 0) {
                    r = -1;
                    goto out;
                }
                btn_clear();
                goto again;
            }
            sel--;
            btn_c_presses = 0;
            redraw = 1;
        }
        sys_idle_tick();
    }
out:
    btn_clear();
    return r;
}

/*
 * mail_draw_card_line(line, text, record) (asm): draw one line of the message card.
 */
void mail_draw_card_line(u8 k, int tbl, int p)
{
    u8 buf[20];
    int i;
    u8 *src, *d;

    for (i = 0; i < 20; i++)
        buf[i] = 0;
    for (i = 0; i < 16; i++) {
        d = buf;
        src = (u8 *)((k << 4) + tbl);
        d[i] = src[i];
    }
    if (((int *)p)[1] <= 0)
        i = 0;
    else
        i = 0x32;
    gfx_draw_text_line(mail_card_line_objs[k], 0x35 - i, k * 11 + 0x21, 0x50, (int)buf, 1, 8);
}

/*
 * Draw a message card: record a (its kind at +4: <= 0 = our own message with the
 * character picture e, positioned by the life stage; else a friend's with friend b's
 * portrait from the portrait sectors), page arrow c (0xFF none), text d. The five line
 * sprites are mail_card_line_objs[].
 */
void mail_draw_card(int a, int b, u8 c, int d, int e)
{
    const u8 *p;
    u8 *img;
    u8 n;
    u8 bank;
    int i;

    ui_new_screen();
    if (*(int *)(a + 4) <= 0) {
        gfx_object_create(0, 0x1c, 0, 0, 0, D_212393C);
        if (save.pet.life_stage == 4)
            gfx_object_create(-2, 0x1b, 0x10, 0, 0x10, (const u8 *)e);
        else
            gfx_object_create(-2, 0x16, 0x10, 0, 0x10, (const u8 *)e);
        if (*(int *)(a + 4) < 0) {
            p = D_21237E0;
            gfx_object_create(0x19 - (p[0] >> 1), 0x26 - (p[1] >> 1), 0x40, 0, 0, (const u8 *)p);
        }
        if (c != 0xff) {
            if (c == 0)
                gfx_object_create(0x74, 0x59, 0x48, 1, 0, D_21059A4);
            else
                gfx_object_create(0x74, 0x59, 0x48, 0, 0, D_21059A4);
        }
        for (i = 0; i <= 4; i++)
            mail_card_line_objs[i] = gfx_object_create(0x35, i * 11 + 0x21, 0x50, 0, 0, 0);
    } else {
        gfx_object_set_flags(gfx_object_create(0, 0x1c, 0, 0, 0, D_212393C), 0x10);
        n = save.friend_list[b].met_flags[1] - 1;
        bank = n / 17 + FLASH_SEC_PORTRAITS;
        n = n % 17;
        img = flash_sector_addr(bank) + n * FRIEND_PORTRAIT_SIZE;
        gfx_object_create(0x52, 0x5e - img[1], 0x10, 0, 0, (const u8 *)img);
        if (c != 0xff) {
            if (c == 0)
                gfx_object_create(0x74, 0x59, 0x48, 1, 0, D_21059A4);
            else
                gfx_object_create(0x74, 0x59, 0x48, 0, 0, D_21059A4);
        }
        for (i = 0; i <= 4; i++)
            mail_card_line_objs[i] = gfx_object_create(4, i * 11 + 0x21, 0x50, 0, 0, 0);
    }
    for (i = 0; i <= 4; i++)
        mail_draw_card_line(i, d, a);
    func_00009A6();
}

/*
 * mail_load_message(record, buf) (asm): load message record a into buf, unpacking its text for
 * display.
 */
void mail_load_message(int a, int b)
{
    int i, j;

    for (i = 0, j = 0; i < 0x40; i++, j++)
        ((u8 *)b)[j] = ((u8 *)(i + a))[0x1c];
    for (i = 0, j = 0x40; i < 0x10; i++, j++)
        ((u8 *)b)[j] = ((u8 *)(i + a))[8];
}

/*
 * Message editing dialogs: k = 0 "メッセへんしゅう? する/しない" (edit the message?, 10 s),
 * k = 1 "へんしゅうやめる? はい/いいえ" (stop editing?, 30 s). Returns 0 = first option,
 * -1 second / C, -2 timeout.
 */
int mail_edit_dialog(u8 k)
{
    u8 sel;
    u8 b;
    u8 t;
    u8 n;

    switch (k) {
    case 0:
        ui_new_screen();
        sel = 1;
        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
        t = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_line(t, 10, 0x20, 0x10, (int)D_255D5CC, 0, 9);
        t = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_line(t, 0x18, 0x40, 0x10, (int)D_255D5E0, 0, 8);
        t = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_line(t, 0x18, 0x50, 0x10, (int)D_255D5E8, 0, 8);
        b = gfx_object_create(6, 0x52, 0x20, 0, 0, D_2104714);
        func_00009A6();
        break;
    case 1:
        ui_new_screen();
        sel = 1;
        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
        t = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_centered(t, 0x20, 0x10, D_255D5F0, 0, 9);
        t = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_line(t, 0x18, 0x40, 0x10, (int)D_255D604, 0, 8);
        t = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_line(t, 0x18, 0x50, 0x10, (int)D_255D60C, 0, 8);
        b = gfx_object_create(6, 0x52, 0x20, 0, 0, D_2104714);
        func_00009A6();
        break;
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    n = k == 1 ? 30 : 10;
    ui_timeout = n;
    for (;;) {
        if (btn_a_presses) {
            ui_timeout = n;
            sel++;
            if (sel > 1)
                sel = 0;
            gfx_object_move_dirty(b, 6, sel * 16 + 0x42);
            func_00009A6();
            btn_a_presses = 0;
        }
        if (btn_b_presses) {
            if (sel != 0)
                break;
            btn_a_presses = 1;
            btn_b_presses = 1;
            btn_c_presses = 1;
            snd_wait_end();
            return sel;
        }
        if (btn_c_presses)
            break;
        sys_idle_tick();
        if (ui_timeout == 0)
            return -2;
    }
    return -1;
}

const u8 D_255D5C4[] = _("");
const u8 D_255D5C8[] = _(" ");
const u8 D_255D5CC[] = _("メッセへんしゅう?");
const u8 D_255D5E0[] = _("する");
const u8 D_255D5E8[] = _("しない");
const u8 D_255D5F0[] = _("へんしゅうやめる?");
const u8 D_255D604[] = _("はい");
const u8 D_255D60C[] = _("いいえ");

/*
 * Previous friend index before k that is used (wraps).
 */
int friend_prev_used(int k)
{
    if (k > 29)
        k = 0;
    do {
        k--;
        if (k < 0)
            k = friend_count() - 1;
    } while (save.friend_list[k].x000_b == 0);
    return k;
}

/*
 * Friend i's picture record (picture banks 9..11, 0xE80 bytes per friend) or a default.
 */
int friend_picture_record(int i)
{
    u8 *r;
    u8 k, a;

    if (save.friend_list[i].met_flags[1] != 0) {
        k = save.friend_list[i].met_flags[1] - 1;
        a = k / 17 + FLASH_SEC_PORTRAITS;
        k = k % 17;
        r = flash_sector_addr(a) + k * 0xe80;
        if (*(int *)r == -1)
            r = *friend_portrait_placeholder;
    } else {
        chara_select_other_by_id((&save.friend_list[0].gender)[i * 0x16]);
        r = (u8 *)chara_sprite_sets[chara_other_sprite_set].pose13;
    }
    return (int)r;
}

/*
 * Friend page a: picture, name and profile texts.
 */
void friend_draw_page(u8 a)
{
    u8 buf2[17];
    u8 tmp[12];
    u8 buf1[17];
    u8 *p;
    u8 *q;
    struct PictureSize *img;

    snd_wait_end();
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    if (save.friend_list[a].met_flags[0] & 2) {
        if (save.friend_list[a].gender != 0) {
            img = (struct PictureSize *)D_20F4CEC;
            gfx_object_create(0x5d - (img->width >> 1), 0x3c - (img->height >> 1), 0x10, 0, 0, (const u8 *)img);
        } else {
            img = (struct PictureSize *)D_20F4BC4;
            gfx_object_create(0x5d - (img->width >> 1), 0x3c - (img->height >> 1), 0x10, 0, 0, (const u8 *)img);
        }
        friend_draw_portrait(a, 0x2d, 0x50, 0x20);
    } else {
        gfx_object_create(0, 0x1c, 0x10, 0, 0, D_2124B50);
    }
    p = buf1;
    memset(p, 0, 16);
    memcpy(p, save.friend_list[a].name, 16);
    memset(buf2, 0, 17);
    memcpy(buf2, p, 16);
    q = tmp;
    q[3] = gfx_object_create(0, 0, 0x40, 0, 0, 0);
    gfx_draw_text_line(q[3], 0x10, 0x51, 0x40, (int)buf2, 0, 8);
}

/*
 * The RAM variables this file defines.
 */
u8 mail_card_line_objs[5];
