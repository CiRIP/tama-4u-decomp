/*
 * Generic menus and dialogs: framed and plain menus, the message box, two-line questions, yes /
 * no and use / no / delete choices, a three-entry menu and a paged list.
 *
 * Code 25068E2..2507C06, .rodata from 253EFB0.
 */
#include "proto/ui/ui_menu.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/room/room_main.h"
#include "proto/ir/ir_driver.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"

/*
 * F23 = the generic menu and dialog drivers used by every menu in the game.
 * Menu table: {int count; int (*cb)(int idx); {label, handler} entries[]}; cb(idx) returns
 * 0 = entry disabled (greyed; error sound when chosen), 1 = enabled, 2 = hidden (skipped
 * when moving the cursor), 3 = drawn as a locked icon. Four entries per page, cursor sprite
 * 0x2104714, page arrows 0x21059A4. The caller runs entries[idx].handler itself when the
 * driver returns 1 (see F7 comm_main_menu, F28, F53).
 *
 * ui_menu_draw: draw the page holding entry *q (page *q / 4) of menu a0, mode 0
 * layout: entry texts, greyed entries, page arrows, cursor.
 */
struct MenuEntry { u32 label; u32 handler; };
struct MenuTableView { int count; u32 (*f)(int); struct MenuEntry entries[1]; };
#define P ((struct MenuTableView *)a0)
void ui_menu_draw(struct MenuTable *a0, int *q)
{
    int page;
    int i;
    int idx;

    page = *q / 4;
    ui_new_screen();
    i = 0;
    gfx_object_create(i, 0x1c, i, i, i, 0x2104794);
    for (; i <= 3; i++) {
        if (page * 4 + i >= P->count)
            continue;
        idx = page * 4 + i;
        if (P->entries[idx].label == 0)
            break;
        switch (P->f(idx)) {
        case 0:
            ui_label_obj = gfx_object_create(0x18, 0x20, 0x34, 0, 3, 0);
            gfx_draw_text_line(ui_label_obj, 0x18, i * 16 + 0x20, 0x34, P->entries[idx].label, 0, 8);
            break;
        case 1:
            ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
            gfx_draw_text_line(ui_label_obj, 0x18, i * 16 + 0x20, 0x30, P->entries[idx].label, 0, 8);
            break;
        case 2:
            continue;
        case 3:
            gfx_object_create(0x18, 0x20, 0x38, 0, 0, 0x20fe4f0);
            break;
        }
    }
    if ((P->count - 1) / 4 > 0) {
        if (page == (P->count - 1) / 4)
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, 0x21059a4);
        else if (page == 0)
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, 0x21059a4);
        else {
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, 0x21059a4);
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, 0x21059a4);
        }
    }
    ui_cursor_obj = gfx_object_create(6, *q % 4 * 16 + 0x22, 0x20, 0, 0, 0x2104714);
    func_00009A6();
}

struct MenuEntryB { u32 label; u32 handler; };
struct MenuTableFramed { int count; u32 (*f)(int); struct MenuEntryB entries[1]; };
/*
 * Mode 1 layout of the list menu (variant of ui_menu_draw): the same with the banner
 * "ひとりのおうちに しゅうごうしよう!!" (let's gather at one house) and a picture frame - the
 * communication-game invitation menu.
 */
void ui_menu_draw_framed(struct MenuTable *a0, int *q)
{
    int i;
    int idx;
    int z;
    const u8 *s;
    struct MenuTableFramed *p = (struct MenuTableFramed *)a0;
    int page = *q / 4;

    ui_new_screen();
    z = 0;
    gfx_object_create(z, 0x1c, z, z, z, D_2104794);
    s = D_2163F38;
    gfx_object_create(0x41 - (s[0] >> 1), 0x4d - (s[1] >> 1), 0x10, 0, 0, (const u8 *)s);
    ui_label_obj = gfx_object_create(0, 0, 0x50, 0, 0, 0);
    gfx_draw_text_centered(ui_label_obj, 0x44, 0x50, D_253EFB4, 2, 9);
    ui_label_obj = gfx_object_create(0, 0, 0x54, 0, 0, 0);
    gfx_draw_text_centered(ui_label_obj, 0x50, 0x54, D_253EFC8, 2, 0xc);
    for (i = 0; i <= 3; i++) {
        idx = page * 4 + i;
        if (idx >= p->count)
            continue;
        if (p->entries[idx].label == 0)
            break;
        switch (p->f(idx)) {
        case 0:
            ui_label_obj = gfx_object_create(0x18, 0x20, 0x34, 0, 3, 0);
            gfx_draw_text_line(ui_label_obj, 0x18, i * 16 + 0x20, 0x34, p->entries[idx].label, 0, 8);
            break;
        case 1:
            ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
            gfx_draw_text_line(ui_label_obj, 0x18, i * 16 + 0x20, 0x30, p->entries[idx].label, 0, 8);
            break;
        case 2:
            continue;
        case 3:
            gfx_object_create(0x18, 0x20, 0x38, 0, 0, D_20FE4F0);
            break;
        }
    }
    if ((p->count - 1) / 4 > 0) {
        if (page == (p->count - 1) / 4)
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
        else if (page == 0)
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
        else {
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
        }
    }
    ui_cursor_obj = gfx_object_create(6, *q % 4 * 16 + 0x22, 0x20, 0, 0, D_2104714);
    func_00009A6();
}

extern void snd_error_beep(void) ;
/*
 * THE MENU DRIVER: menu table m (see the top of the file), *pos = cursor entry, timeout =
 * seconds of inactivity (timer 5 periodic tick), mode 0 = plain layout (ui_menu_draw),
 * 1 = framed layout (ui_menu_draw_framed). A = next entry (hidden ones skipped, wraps, redraws on
 * a page change), B = choose (cb(idx) == 0 -> refusal sound and stay), C = cancel. Returns
 * 1 chosen (*pos = index), 2 cancelled, 3 timed out.
 */
int ui_menu_run(struct MenuTable *m, int *pos, u8 timeout, u8 mode)
{
    int redraw;

    tmr_t5_standard_tick();
    ui_timeout = timeout;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    if (mode == 0)
        ui_menu_draw(m, pos);
    else if (mode == 1)
        ui_menu_draw_framed(m, pos);

    for (;;) {
        if (btn_a_presses != 0) {
            redraw = 0;
            btn_a_presses = 0;
            for (; *pos < (int)m->count; (*pos)++) {
                if (m->fn(*pos + 1) != 2)
                    break;
                if ((*pos & 3) == 0)
                    redraw = 1;
            }
            (*pos)++;
            if ((*pos & 3) == 0)
                redraw = 1;
            if (*pos >= m->count) {
                *pos = 0;
                redraw = 1;
            }
            if (redraw) {
                if (mode == 0)
                    ui_menu_draw(m, pos);
                else if (mode == 1)
                    ui_menu_draw_framed(m, pos);
            }
            gfx_object_move_dirty(ui_cursor_obj, 6, *pos % 4 * 16 + 0x22);
            func_00009A6();
            ui_timeout = 10;
        } else if (btn_b_presses != 0) {
            redraw = m->fn(*pos);
            if (redraw == 0) {
                snd_error_beep();
                btn_b_presses = redraw;
                ui_timeout = 10;
            } else {
                btn_b_presses = 0;
                ui_timeout = 10;
                return 1;
            }
        } else if (btn_c_presses != 0) {
            btn_c_presses = 0;
            ui_timeout = 10;
            return 2;
        } else {
            sys_idle_tick();
            if (ui_timeout == 0)
                return 3;
        }
    }
}

/*
 * Run list menu a (10 s timeout). Used by the communication menus etc.
 */
u32 ui_menu_run_plain(const struct MenuTable *a, int *b)
{
    return ui_menu_run((struct MenuTable *)a, b, 10, 0);
}

/*
 * Run list menu a with the framed drawing.
 */
void ui_menu_run_framed(struct MenuTable *a, int *b, u8 c)
{
    ui_menu_run(a, b, c, 1);
}

/*
 * Message box: up to four centred text lines a..d at vertical offset y; waits 10 s for any
 * button (returns 0) or times out (returns 3).
 */
int ui_message_box(const u8 *a, const u8 *b, const u8 *c, const u8 *d, int y)
{
    tmr_t5_standard_tick();
    ui_timeout = 10;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    if (a != 0) {
        ui_label_obj = gfx_object_create(0, 0, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, y + 0x20, 0x30, a, 0, 9);
    }
    if (b != 0) {
        ui_label_obj = gfx_object_create(0, 0, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, y + 0x30, 0x30, b, 0, 9);
    }
    if (c != 0) {
        ui_label_obj = gfx_object_create(0, 0, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, y + 0x40, 0x30, c, 0, 9);
    }
    if (d != 0) {
        ui_label_obj = gfx_object_create(0, 0, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, y + 0x50, 0x30, d, 0, 9);
    }
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    for (;;) {
        if ((u8)(btn_a_presses | btn_b_presses | btn_c_presses))
            break;
        sys_idle_tick();
        if (ui_timeout == 0) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            return 3;
        }
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 0;
}

/*
 * Two-line question (a, b) with two options (c, d): A toggles *f (0/1), B chooses (*f =
 * index). Returns 1 chosen, 2 cancelled (C), 3 timeout.
 */
int ui_question_two_lines(const void *a, const void *b, const void *c, const void *d, int y, int *f)
{
    int r;
    int *p = f;

    *p &= 1;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    if (a != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, 0x20, 0x30, (u8 *)a, 0, 9);
    }
    if (b != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, 0x30, 0x30, (u8 *)b, 0, 9);
    }
    if (c != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x34, 0, 0, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x30, 0x34, (int)c, 0, 8);
    }
    if (d != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x34, 0, 0, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x40, 0x34, (int)d, 0, 8);
    }
    ui_cursor_obj = gfx_object_create(6, ((*p + 1) % 4) * 16 + y + 0x22, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses) {
            if (++*p > 1)
                *p = 0;
            gfx_object_move_dirty(ui_cursor_obj, 6, ((*p + 1) % 4) * 16 + y + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses)
            goto ok;
        if (btn_c_presses) {
            btn_c_presses = 0;
            ui_timeout = 10;
            r = 2;
            goto ret;
        }
        sys_idle_tick();
        if (ui_timeout)
            continue;
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        r = 3;
ret:
        return r;
    }
ok:
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 1;
}

/*
 * The "はい/いいえ" yes/no box used everywhere: one-line title a with the two options b and c
 * (option 0 = the first), cursor in *f, A toggles. Returns 1 chosen (*f), 2 cancelled,
 * 3 timeout. The one-line-title variant of ui_question_two_lines.
 */
u32 ui_yes_no(const u8 *a, const u8 *b, const u8 *c, int y, int *f)
{
    int r;
    int *p = f;

    *f &= 1;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    if (a != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, 0x20, 0x30, a, 0, 9);
    }
    if (b != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x30, 0x34, (int)b, 0, 8);
    }
    if (c != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x40, 0x34, (int)c, 0, 8);
    }
    ui_cursor_obj = gfx_object_create(6, ((*p + 1) % 4) * 16 + y + 0x22, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses) {
            if (++*p > 1)
                *p = 0;
            gfx_object_move_dirty(ui_cursor_obj, 6, ((*p + 1) % 4) * 16 + y + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses)
            goto ok;
        if (btn_c_presses) {
            btn_c_presses = 0;
            ui_timeout = 10;
            r = 2;
            goto ret;
        }
        sys_idle_tick();
        if (ui_timeout)
            continue;
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        r = 3;
ret:
        return r;
    }
ok:
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 1;
}

/*
 * Item question a with the three fixed options はい / いいえ / けす (yes / no / delete):
 * *f = choice 0..2; returns 1 chosen, 2 cancelled, 3 timeout.
 */
int ui_use_no_delete(int a, int y, u32 *f)
{
    int r;
    int y2 = y;
    u32 *f2 = f;

    *f &= 1;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_centered(ui_label_obj, y + 0x20, 0x30, (u8 *)a, 0, 9);
    ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_line(ui_label_obj, 0x18, y + 0x30, 0x34, (int)D_253EFE0, 0, 8);
    ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_line(ui_label_obj, 0x18, y + 0x40, 0x34, (int)D_253EFE8, 0, 8);
    ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_line(ui_label_obj, 0x18, y + 0x50, 0x34, (int)D_253EFF0, 0, 8);
    ui_cursor_obj = gfx_object_create(6, ((*f + 1) & 3) * 16 + y + 0x22, 0x20, 0, 0, 0x2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses == 0)
            goto done;
        if (++*f2 > 2)
            *f2 = 0;
        gfx_object_move_dirty(ui_cursor_obj, 6, ((*f2 + 1) & 3) * 16 + y2 + 0x22);
        func_00009A6();
        btn_a_presses = 0;
        ui_timeout = 10;
done:
        if (btn_b_presses)
            goto ok;
        if (btn_c_presses) {
            btn_c_presses = 0;
            ui_timeout = 10;
            r = 2;
            goto ret;
        }
        sys_idle_tick();
        if (ui_timeout == 0)
            goto cont;
        continue;
cont:
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        r = 3;
ret:
        return r;
    }
ok:
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 1;
}

const u8 D_253EFB4[] = _("ひとりのおうちに");
const u8 D_253EFC8[] = _("しゅうごうしよう!!");
const u8 D_253EFE0[] = _("はい");
const u8 D_253EFE8[] = _("いいえ");
const u8 D_253EFF0[] = _("けす");

/*
 * Question with two text lines and two answers (a..d), result in *f.
 */
int ui_question_two_lines_b(int a, int b, int c, int d, int y, u32 *f)
{
    int r;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    if (a != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, 0x20, 0x30, (u8 *)a, 0, 9);
    }
    if (b != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x34, 0, 0, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x30, 0x34, b, 0, 8);
    }
    if (c != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x34, 0, 0, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x40, 0x34, c, 0, 8);
    }
    if (d != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x34, 0, 0, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x50, 0x34, d, 0, 8);
    }
    ui_cursor_obj = gfx_object_create(6, ((*f + 1) & 3) * 16 + y + 0x22, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses) {
            if (++*f > 2)
                *f = 0;
            gfx_object_move_dirty(ui_cursor_obj, 6, ((*f + 1) & 3) * 16 + y + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses)
            goto ok;
        if (btn_c_presses) {
            btn_c_presses = 0;
            ui_timeout = 10;
            r = 2;
            goto ret;
        }
        sys_idle_tick();
        if (ui_timeout)
            continue;
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        r = 3;
ret:
        return r;
    }
ok:
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 1;
}

/*
 * Three-entry menu with per-entry enable flags (greyed when 3) and pictures.
 */
int ui_three_entry_menu(int a, int b, u16 c, int d, u16 e, int g, u16 h, int y, int *f)
{
    u8 flag[3];
    int i;
    int r;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    i = 3;
    while (i--)
        flag[i] = 0;
    if (a != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, 0x20, 0x30, (u8 *)a, 0, 9);
    }
    if (b != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x34, 0, c, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x30, 0x34, b, 0, 8);
        if ((c & 7) != 3)
            flag[0] = 1;
    }
    if (d != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x38, 0, e, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x40, 0x38, d, 0, 8);
        if ((e & 7) != 3)
            flag[1] = 1;
    }
    if (g != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x3c, 0, h, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x50, 0x3c, g, 0, 8);
        if ((h & 7) != 3)
            flag[2] = 1;
    }
    ui_cursor_obj = gfx_object_create(6, (*f + 1) % 4 * 16 + y + 0x22, 0x40, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses) {
            if (++*f > 2)
                *f = 0;
            gfx_object_move_dirty(ui_cursor_obj, 6, (*f + 1) % 4 * 16 + y + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            if (flag[*f] == 0) {
                snd_error_beep();
                btn_b_presses = 0;
                ui_timeout = 10;
                continue;
            }
            goto ok;
        }
        if (btn_c_presses) {
            btn_c_presses = 0;
            ui_timeout = 10;
            r = 2;
            goto ret;
        }
        sys_idle_tick();
        if (ui_timeout)
            continue;
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        r = 3;
ret:
        return r;
    }
ok:
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 1;
}

struct ListRow {
    int label;
    u16 value;
};
/*
 * Paged selection list a (n entries, table tbl) with a cursor; *out = chosen entry.
 */
int ui_paged_list(const u8 *a, int n, u8 *tbl, int y, u32 *out)
{
    u8 flag[3];
    int i;
    int p0, p1;
    u16 h0, h1, h2;
    int page, npages, cur;
    int sel, k, p2;
    int z;

    npages = (n + 2) / 3;
    cur = *out;
top:
    i = 3;
    while (i--)
        flag[i] = 0;
    page = cur / 3;
    sel = cur % 3;
    k = page * 3;
    if (k < n) {
        p0 = ((struct ListRow *)tbl)[k].label;
        h0 = ((struct ListRow *)tbl)[k].value;
        k++;
    } else {
        p0 = h0 = 0;
    }
    if (k < n) {
        p1 = ((struct ListRow *)tbl)[k].label;
        h1 = ((struct ListRow *)tbl)[k].value;
        k++;
    } else {
        p1 = h1 = 0;
    }
    if (k < n) {
        p2 = ((struct ListRow *)tbl)[k].label;
        h2 = ((struct ListRow *)tbl)[k].value;
    } else {
        p2 = h2 = 0;
    }
    ui_new_screen();
    z = 0;
    gfx_object_create(z, 0x1c, z, z, z, D_2104794);
    if (a != (u8 *)z) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_centered(ui_label_obj, 0x20, 0x30, a, 0, 9);
    }
    if (p0 != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x34, 0, h0, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x30, 0x34, p0, 0, 8);
        if ((h0 & 7) != 3)
            flag[0] = 1;
    }
    if (p1 != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x38, 0, h1, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x40, 0x38, p1, 0, 8);
        if ((h1 & 7) != 3)
            flag[1] = 1;
    }
    if (p2 != 0) {
        ui_label_obj = gfx_object_create(0x18, 0x20, 0x3c, 0, h2, 0);
        gfx_draw_text_line(ui_label_obj, 0x18, y + 0x50, 0x3c, p2, 0, 8);
        if ((h2 & 7) != 3)
            flag[2] = 1;
    }
    if (page + 1 >= npages) {
        if (page != 0)
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, 0x21059a4);
    } else
        gfx_object_create(0x72, 0x58, 0x40, 0, 0, 0x21059a4);
    ui_cursor_obj = gfx_object_create(6, sel * 16 + y + 0x32, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses) {
            btn_a_presses = 0;
            ui_timeout = 10;
next:
            sel++;
            if (sel > 2) {
                if (npages == 1)
                    sel = 0;
                else {
                    page++;
                    cur += 3;
                    if (page >= npages)
                        cur = 0;
                    goto top;
                }
            }
            if (sel == 0 && p0 == 0)
                goto next;
            if (sel == 1 && p1 == 0)
                goto next;
            if (sel == 2 && p2 == 0)
                goto next;
            gfx_object_move_dirty(ui_cursor_obj, 6, sel * 16 + y + 0x32);
            func_00009A6();
        }
        if (btn_b_presses) {
            btn_b_presses = 0;
            ui_timeout = 10;
            if (flag[sel])
                goto ok;
            snd_error_beep();
            continue;
        }
        if (btn_c_presses) {
            btn_c_presses = 0;
            ui_timeout = 10;
            return 2;
        }
        sys_idle_tick();
        if (ui_timeout)
            continue;
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        return 3;
    }
ok:
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    *out = page * 3 + sel;
    return 1;
}

/*
 * The RAM variables this file defines.
 */
volatile u8 ui_timeout;
u8 ui_cursor_obj DSTRAM;
u8 ui_label_obj DSTRAM;
