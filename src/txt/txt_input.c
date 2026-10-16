/*
 * Text input: the character-grid keyboard (hiragana, katakana, symbols, latin), cursor and
 * confirmation, used for names and messages.
 *
 * Code 24FE82A..24FF5A2, .rodata from 253A4D4.
 */
#include "proto/txt/txt_input.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/room/room_main.h"
#include "proto/sys/sys_boot.h"
#include "proto/comm/comm_menu.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "libc.h"
#include "hw.h"

const u8 D_253A4D8[] = _("");
const u8 D_253A4DC[] = _(" ");

/*
 * Text input (the keyboard): the prompt picture b (0..9, pictures 0x2100BC8.., e.g.
 * "なまえをいれてね"), the edited text c of up to 8 (7) characters (txt_input_max_len).
 * A moves to the next place (past the last one: txt_input_confirm "これでけってい?", or
 * "ちゃんと もじをいれてね!" when nothing was entered), B opens the character grid
 * (txt_input_pick_char), C goes back. Used by the name entry of the set-up, the profile
 * editor (F47) and the message editor (F52) (asm).
 */
int txt_input(int a, u8 b, int c)
{
    u8 buf[16];
    int img;
    u8 h;
    int again;
    int t;
    int n;
    int r;
    s8 *cur;
    u8 i, y;
    s8 k;

    again = 0;
    txt_input_charset = a;
    txt_input_prompt = b;
    txt_input_max_len = 8;
    switch (b) {
    case 0:
        img = 0x2100bc8;
        break;
    case 1:
        img = 0x2100e18;
        txt_input_max_len = 7;
        break;
    case 2:
        img = 0x2101068;
        break;
    case 3:
        img = 0x21012b8;
        break;
    case 4:
        img = 0x2101508;
        break;
    case 5:
        img = 0x2101758;
        txt_input_max_len = 7;
        break;
    case 6:
        img = 0x21019a8;
        break;
    case 7:
        img = 0x2101bf8;
        break;
    case 8:
        img = 0x2101e48;
        break;
    case 9:
        img = 0x2102098;
        break;
    default:
        return 0;
    }
    memset(buf, 0, 0x10);
    memcpy(buf, (void *)c, txt_input_max_len * 2);
    {
    int sv;
    switch (txt_input_charset) {
    case 8:
    case 12:
    case 16:
    case 20:
    case 24:
    case 28:
    case 32:
        sv = 5;
        break;
    case 36:
        sv = 6;
        break;
    default:
        sv = 25;
        break;
    }
    txt_input_page_count = sv;
    }
    memset(txt_input_char_objs, 0, 8);
    txt_input_pos = 0;
    btn_clear();
redraw:
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, txt_input_max_len == 7 ? 0x21022e8 : 0x20fe79c);
    h = gfx_object_create(0, 0x1c, 0x20, 0, 0, 0);
    gfx_object_create(6, 0x1f, 0x40, 0, 0, img);
    for (i = 0, y = 12; i < txt_input_max_len; i++) {
        txt_input_char_objs[i] = gfx_object_create(y, 0x2f, 0x50, 0, 0, 0);
        txt_set_glyph(txt_input_char_objs[i], 0x50, (int)&buf[i * 2], 0);
        y += 13;
    }
    func_00009A6();
    n = 0;
    t = 0;
    txt_input_hold_count = 0;
    tmr_t5_start_timeout(500);
    cur = &txt_input_pos;
loop:
    if (sys_display_state != 0) {
        if (b != 0)
            goto fail;
        T16_CTL0 = b;
        T16_CTL1 = b;
        T16_CTL3 = b;
        T16_CTL5 = b;
        sys_display_off();
        if (sys_display_state != 0) {
            do
                asm volatile("slp");
            while (sys_display_state != 0);
        }
        if (sys_battery_check_due != 0) {
            sys_battery_low();
            sys_battery_check_due = 0;
        }
        if (sys_battery_level <= 0x2e3)
            sys_power_down();
        tmr_t5_start_timeout(500);
    }
    if (tmr_flags & 0x20) {
        n++;
        tmr_flags &= ~0x20;
        if (!(P0_P0D & 1)) {
            if (++txt_input_hold_count > 2) {
                if (t == 0) {
                    t = 1;
                    tmr_t5_start_timeout(0x4b);
                }
                btn_a_presses++;
                sys_idle_seconds = 0;
                txt_input_hold_count = 2;
                snd_play(1);
            }
        } else {
            txt_input_hold_count = 0;
            if (t != 0) {
                t = 0;
                tmr_t5_start_timeout(500);
            }
        }
    }
    if (btn_a_presses != 0) {
        if (t == 0) {
            T16_CTL5 = 2;
            T16_CTL5 = 1;
        }
        btn_a_presses = 0;
        gfx_object_set_flags(txt_input_char_objs[*cur], 0);
        txt_set_glyph(txt_input_char_objs[*cur], 0x50, (int)&buf[*cur * 2], 0);
        n = 0;
        again = 0;
        if (++*cur >= txt_input_max_len) {
            r = txt_input_confirm(b, (int)buf);
            if (r > 0) {
                if (b == 0) {
                    for (k = 14; k >= 0; k -= 2) {
                        if (n == 0 && memcmp(&buf[k], D_253A4D8, 2) != 0) {
                            n = 1;
                            continue;
                        }
                        if (n != 0 && memcmp(&buf[k], D_253A4D8, 2) == 0)
                            memcpy(&buf[k], (void *)D_253A4DC, 2);
                    }
                }
                memcpy((void *)c, buf, txt_input_max_len * 2);
                goto out;
            }
            if (r == 0 || (r < 0 && b == 0)) {
                *cur = txt_input_max_len - 1;
                goto redraw;
            }
            goto out;
        }
    }
    if (btn_b_presses != 0) {
        btn_b_presses = 0;
        gfx_object_set_flags(txt_input_char_objs[*cur], 0x2000);
        txt_set_glyph(txt_input_char_objs[*cur], 0x50, (int)&buf[*cur * 2], 0);
        func_00009A6();
        r = txt_input_pick_char((int)buf, h);
        if (r > 0)
            btn_a_presses = 1;
        else if (r < 0)
            goto out;
        again = 0;
        goto redraw;
    }
    if (btn_c_presses != 0) {
        btn_c_presses = 0;
        gfx_object_set_flags(txt_input_char_objs[txt_input_pos], 0);
        txt_set_glyph(txt_input_char_objs[txt_input_pos], 0x50, (int)&buf[txt_input_pos * 2], 0);
        n = 0;
        if (--txt_input_pos < 0) {
            txt_input_pos = 0;
            if (b == 0 && again == 0) {
                again = 1;
                goto draw;
            }
            if (b == 0)
                memcpy((void *)c, buf, txt_input_max_len * 2);
            r = 0;
            goto out;
        }
    }
draw:
    gfx_object_set_flags(txt_input_char_objs[*cur], (n & 1) << 13);
    txt_set_glyph(txt_input_char_objs[*cur], 0x50, (int)&buf[*cur * 2], 0);
    func_00009A6();
    sys_idle_tick();
    goto loop;
fail:
    r = -1;
out:
    btn_clear();
    return r;
}

extern const u8 D_253A57C[];
extern const u8 D_253A588[];
extern const u8 D_253A59C[];
extern const u8 D_253A5B0[];
extern const u8 D_253A5B8[];
/*
 * Text input: the end-of-input confirmation "これでけってい?" (はい / いいえ), or the
 * "enter some characters" notice when the text is empty (asm).
 */
int txt_input_confirm(u8 a, int b)
{
    int r;
    u8 h;
    u8 *s;
    int ok;
    u8 i;
    u8 sel;
    u8 t;

    ok = 1;
    r = 0;
    if (a == r) {
        s = (u8 *)b;
        for (i = 0; i < txt_input_max_len; i++, s += 2) {
            if (s[1] != 0x6c && s[1] != 0)
                goto bad;
        }
    test:
        if (ok != 0) {
            ui_new_screen();
            gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
            t = gfx_object_create(0, 0, 0, 0, 0, 0);
            gfx_draw_text_line(t, 0x28, 0x30, 0x10, (int)D_253A57C, 0, 4);
            t = gfx_object_create(0, 0, 0, 0, 0, 0);
            gfx_draw_text_line(t, 0x11, 0x40, 0x10, (int)D_253A588, 0, 8);
            func_00009A6();
            btn_clear();
            for (;;) {
                if (sys_display_state != 0) {
                    if (a != 0)
                        goto fail;
                    T16_CTL0 = a;
                    T16_CTL1 = a;
                    T16_CTL3 = a;
                    T16_CTL5 = a;
                    sys_display_off();
                    if (sys_display_state != 0) {
                        do
                            asm volatile("slp");
                        while (sys_display_state != 0);
                    }
                    if (sys_battery_check_due != 0) {
                        sys_battery_low();
                        sys_battery_check_due = 0;
                    }
                    if (sys_battery_level <= 0x2e3)
                        sys_power_down();
                    tmr_t5_start_timeout(500);
                }
                if (btn_a_presses != 0 || btn_b_presses != 0 || btn_c_presses != 0)
                    goto out;
                sys_idle_tick();
            }
        }
        goto confirm;
    bad:
        ok = 0;
        goto test;
    }
confirm:
    sel = 0;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    t = gfx_object_create(0, 0, 0, 0, 0, 0);
    txt_draw_centered(t, 0x20, 0x10, (u8 *)D_253A59C, 0, 9);
    t = gfx_object_create(0, 0, 0, 0, 0, 0);
    txt_render_line(t, 0x18, 0x40, 0x10, (u8 *)D_253A5B0, 0, 8);
    t = gfx_object_create(0, 0, 0, 0, 0, 0);
    txt_render_line(t, 0x18, 0x50, 0x10, (u8 *)D_253A5B8, 0, 8);
    h = gfx_object_create(6, 0x42, 0x20, 0, 0, 0x2104714);
    func_00009A6();
    btn_clear();
    for (;;) {
        if (sys_display_state != 0) {
            if (a != 0)
                goto fail2;
            T16_CTL0 = a;
            T16_CTL1 = a;
            T16_CTL3 = a;
            T16_CTL5 = a;
            sys_display_off();
            if (sys_display_state != 0) {
                do
                    asm volatile("slp");
                while (sys_display_state != 0);
            }
            if (sys_battery_check_due != 0) {
                sys_battery_low();
                sys_battery_check_due = 0;
            }
            if (sys_battery_level <= 0x2e3)
                sys_power_down();
            goto next;
        fail2:
            r = -1;
            goto out;
        }
    next:
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            sel++;
            if (sel > 1)
                sel = 0;
            gfx_object_move_dirty(h, 6, sel * 16 + 0x42);
            func_00009A6();
        }
        if (btn_b_presses != 0) {
            snd_wait_end();
            if (sel == 0)
                r = 1;
            goto out;
        }
        if (btn_c_presses != 0)
            goto out;
        sys_idle_tick();
    }
fail:
    r = -1;
out:
    btn_clear();
    return r;
}

/*
 * Text input: the character grid (hiragana, symbols, pictographs, katakana pages,
 * txt_input_load_grid); returns the picked character (asm).
 */
int txt_input_pick_char(int a, u8 b)
{
    int r;
    u8 k;
    int t;
    u8 i, x, v;

    r = 0;
    k = 0;
    gfx_object_set_image(b, 0x20, 0, (const u8 *)(txt_input_max_len == 7 ? 0x21034fc : 0x20ff9b0));
    txt_input_load_grid(k);
    i = 0;
    x = 0x10;
    for (; i <= 7; i++) {
        txt_input_grid_objs[i] = gfx_object_create(x, 0x46, 0x50, 0, 0, 0);
        txt_set_glyph(txt_input_grid_objs[i], 0x50, (int)&txt_input_grid_chars[i * 2], 0);
        x += *(u8 *)0x2147244;
    }
    func_00009A6();
    t = 0;
    txt_input_hold_count = t;
    tmr_t5_start_timeout(500);
    btn_clear();
    for (;;) {
        if (sys_display_state != 0) {
            if (txt_input_prompt != 0)
                goto fail;
            T16_CTL0 = 0;
            T16_CTL1 = 0;
            T16_CTL3 = 0;
            T16_CTL5 = 0;
            sys_display_off();
            if (sys_display_state != 0) {
                do
                    asm volatile("slp");
                while (sys_display_state != 0);
            }
            if (sys_battery_check_due != 0) {
                sys_battery_low();
                sys_battery_check_due = 0;
            }
            if (sys_battery_level <= 0x2e3)
                sys_power_down();
            tmr_t5_start_timeout(500);
            goto next;
        fail:
            r = -1;
            goto out;
        }
    next:
        if (tmr_flags & 0x20) {
            tmr_flags &= ~0x20;
            if (!(P0_P0D & 1)) {
                if (++txt_input_hold_count > 2) {
                    if (t == 0) {
                        t = 1;
                        tmr_t5_start_timeout(0x4b);
                    }
                    btn_a_presses++;
                    sys_idle_seconds = 0;
                    txt_input_hold_count = 2;
                    snd_play(1);
                }
            } else {
                txt_input_hold_count = 0;
                if (t != 0) {
                    t = 0;
                    tmr_t5_start_timeout(500);
                }
            }
        }
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            k++;
            if (txt_input_page_count <= k)
                k = 0;
            txt_input_load_grid(k);
            for (i = 0; i <= 7; i++)
                txt_set_glyph(txt_input_grid_objs[i], 0x50, (int)&txt_input_grid_chars[i * 2], 0);
            func_00009A6();
        }
        if (btn_b_presses != 0) {
            snd_wait_end();
            btn_b_presses = 0;
            if (((u8 *)txt_input_grid_chars)[1] != 0xfe) {
                r = txt_input_cursor_loop(a);
                if (r != 0)
                    goto out;
            } else {
                txt_input_pos = txt_input_max_len;
                r = 1;
                goto out;
            }
        }
        if (btn_c_presses != 0)
            goto out;
        sys_idle_tick();
    }
out:
    btn_clear();
    return r;
}

extern const u8 D_253A5C0[];
int txt_input_cursor_loop(int a)
{
    int r;
    int t;
    u8 *p;
    u8 *q;
    int blink;
    u8 i;
    u8 f;

    r = 0;
    p = (u8 *)txt_input_grid_chars;
    blink = 0;
    t = 0;
    txt_input_hold_count = 0;
    tmr_t5_start_timeout(500);
    btn_clear();
loop:
    if (sys_display_state != 0) {
        f = txt_input_prompt;
        if (f != 0) {
            r = -1;
            goto out;
        }
        T16_CTL0 = f;
        T16_CTL1 = f;
        T16_CTL3 = f;
        T16_CTL5 = f;
        sys_display_off();
        while (sys_display_state != 0)
            asm volatile("slp");
        if (sys_battery_check_due != 0) {
            sys_battery_low();
            sys_battery_check_due = 0;
        }
        if (sys_battery_level <= 0x2e3)
            sys_power_down();
        tmr_t5_start_timeout(500);
    }
    if (tmr_flags & 0x20) {
        blink++;
        tmr_flags &= ~0x20;
        if (!(P0_P0D & 1)) {
            if (++txt_input_hold_count > 2) {
                if (t == 0) {
                    t = 1;
                    tmr_t5_start_timeout(0x4b);
                }
                btn_a_presses++;
                sys_idle_seconds = 0;
                txt_input_hold_count = 2;
                snd_play(1);
            }
        } else {
            txt_input_hold_count = 0;
            if (t != 0) {
                t = 0;
                tmr_t5_start_timeout(500);
            }
        }
    }
    if (btn_a_presses != 0) {
        btn_a_presses = 0;
        blink = 0;
        if (t == 0) {
            T16_CTL5 = 2;
            T16_CTL5 = 1;
        }
        p += 2;
        if (memcmp(p, D_253A5C0, 2) == 0)
            p = (u8 *)txt_input_grid_chars;
        q = p;
        for (i = 0; i <= 7; i++) {
            txt_set_glyph(txt_input_grid_objs[i], 0x50, (int)q, 0);
            q += 2;
            if (memcmp(q, D_253A5C0, 2) == 0)
                q = (u8 *)txt_input_grid_chars;
        }
    }
    if (btn_b_presses != 0) {
        memcpy((u8 *)a + txt_input_pos * 2, p, 2);
        r = 1;
        goto out;
    }
    if (btn_c_presses != 0) {
        gfx_object_set_flags(txt_input_grid_objs[0], 0);
        for (i = 0; i <= 7; i++)
            txt_set_glyph(txt_input_grid_objs[i], 0x50, (int)&txt_input_grid_chars[i * 2], 0);
        func_00009A6();
        goto out;
    }
    gfx_object_set_flags(txt_input_grid_objs[0], (blink & 1) << 13);
    txt_set_glyph(txt_input_grid_objs[0], 0x50, (int)p, 0);
    func_00009A6();
    sys_idle_tick();
    goto loop;
out:
    btn_clear();
    return r;
}

const u8 D_253A57C[] = _("ちゃんと");
const u8 D_253A588[] = _("もじをいれてね!");
const u8 D_253A59C[] = _("これでけってい?");
const u8 D_253A5B0[] = _("はい");
const u8 D_253A5B8[] = _("いいえ");
const u8 D_253A5C0[] = _("{FFFE}");
/* the same keyboard grids in the other code pages (high byte 08..24) */
const char D_253A5C4[] = _("{08E0}{08E1}{08E2}{08E3}{08E4}{08E5}{086C}{08E6}{08E7}{08E8}{08E9}{08EA}{08EB}{086C}{08EC}{08ED}{08EE}{08EF}{08F0}{08F1}{086C}{08F2}{08F3}{08F4}{08F5}{08F6}{08F7}{086C}{08F8}{08F9}{086C}{FFFE}");
const char D_253A608[] = _("{0851}{0852}{0853}{0854}{0855}{0856}{086C}{0857}{0858}{0859}{085A}{085B}{085C}{086C}{085D}{085E}{085F}{0860}{0861}{0862}{086C}{0863}{0864}{0865}{0866}{0867}{0868}{086C}{0869}{086A}{086B}{086C}{FFFE}");
const char D_253A64C[] = _("{086E}{086F}{0870}{0871}{0872}{0873}{086C}{0874}{0875}{0876}{086D}{086C}{FFFE}");
const char D_253A668[] = _("{08D0}{08D1}{08D2}{08D3}{08D4}{08D5}{086C}{08D6}{08D7}{08D8}{08D9}{08DA}{08DB}{086C}{08DC}{08DD}{08DE}{08DF}{086C}{FFFE}");
const char D_253A694[] = _("{08FE}{086C}{086C}{086C}{086C}{086C}{086C}{086C}{FFFE}");
const char D_253A6A8[] = _("{0CE0}{0CE1}{0CE2}{0CE3}{0CE4}{0CE5}{0C6C}{0CE6}{0CE7}{0CE8}{0CE9}{0CEA}{0CEB}{0C6C}{0CEC}{0CED}{0CEE}{0CEF}{0CF0}{0CF1}{0C6C}{0CF2}{0CF3}{0CF4}{0CF5}{0CF6}{0CF7}{0C6C}{0CF8}{0CF9}{0C01}{0C02}{0C03}{0C04}{0C6C}{0C05}{0C06}{0C07}{0C08}{0C09}{0C0A}{0C6C}{0C0B}{0C0C}{0C0D}{0C0E}{0C0F}{0C10}{0C6C}{FFFE}");
const char D_253A710[] = _("{0C51}{0C52}{0C53}{0C54}{0C55}{0C56}{0C6C}{0C57}{0C58}{0C59}{0C5A}{0C5B}{0C5C}{0C6C}{0C5D}{0C5E}{0C5F}{0C60}{0C61}{0C62}{0C6C}{0C63}{0C64}{0C65}{0C66}{0C67}{0C68}{0C6C}{0C69}{0C6A}{0C6B}{0C6C}{FFFE}");
const char D_253A754[] = _("{0C6E}{0C6F}{0C70}{0C71}{0C72}{0C73}{0C6C}{0C74}{0C75}{0C76}{0C6D}{0C6C}{FFFE}");
const char D_253A770[] = _("{0CD0}{0CD1}{0CD2}{0CD3}{0CD4}{0CD5}{0C6C}{0CD6}{0CD7}{0CD8}{0CD9}{0CDA}{0CDB}{0C6C}{0CDC}{0CDD}{0CDE}{0CDF}{0C6C}{FFFE}");
const char D_253A79C[] = _("{0CFE}{0C6C}{0C6C}{0C6C}{0C6C}{0C6C}{0C6C}{0C6C}{FFFE}");
const char D_253A7B0[] = _("{10E0}{10E1}{10E2}{10E3}{10E4}{10E5}{106C}{10E6}{10E7}{10E8}{10E9}{10EA}{10EB}{106C}{10EC}{10ED}{10EE}{10EF}{10F0}{10F1}{106C}{10F2}{10F3}{10F4}{10F5}{10F6}{10F7}{106C}{10F8}{10F9}{1001}{1002}{1003}{1004}{106C}{1005}{1006}{1007}{106C}{FFFE}");
const char D_253A804[] = _("{1051}{1052}{1053}{1054}{1055}{1056}{106C}{1057}{1058}{1059}{105A}{105B}{105C}{106C}{105D}{105E}{105F}{1060}{1061}{1062}{106C}{1063}{1064}{1065}{1066}{1067}{1068}{106C}{1069}{106A}{106B}{106C}{FFFE}");
const char D_253A848[] = _("{106E}{106F}{1070}{1071}{1072}{1073}{106C}{1074}{1075}{1076}{106D}{106C}{FFFE}");
const char D_253A864[] = _("{10D0}{10D1}{10D2}{10D3}{10D4}{10D5}{106C}{10D6}{10D7}{10D8}{10D9}{10DA}{10DB}{106C}{10DC}{10DD}{10DE}{10DF}{106C}{FFFE}");
const char D_253A890[] = _("{10FE}{106C}{106C}{106C}{106C}{106C}{106C}{106C}{FFFE}");
const char D_253A8A4[] = _("{14E0}{1401}{14E1}{14E2}{14E3}{14E4}{146C}{1402}{14E5}{14E6}{14E7}{14E8}{1403}{146C}{14E9}{14EA}{14EB}{14EC}{14ED}{1404}{146C}{14EE}{1405}{14EF}{14F0}{14F1}{14F2}{146C}{14F3}{14F4}{1406}{1407}{14F5}{14F6}{146C}{14F7}{14F8}{14F9}{146C}{FFFE}");
const char D_253A8F8[] = _("{1451}{1452}{1453}{1454}{1455}{1456}{146C}{1457}{1458}{1459}{145A}{145B}{145C}{146C}{145D}{145E}{145F}{1460}{1461}{1462}{146C}{1463}{1464}{1465}{1466}{1467}{1468}{146C}{1469}{146A}{146B}{146C}{FFFE}");
const char D_253A93C[] = _("{146E}{146F}{1470}{1471}{1472}{1473}{146C}{1474}{1475}{1476}{146D}{146C}{FFFE}");
const char D_253A958[] = _("{14D0}{14D1}{14D2}{14D3}{14D4}{14D5}{146C}{14D6}{14D7}{14D8}{14D9}{14DA}{14DB}{146C}{14DC}{14DD}{14DE}{14DF}{146C}{FFFE}");
const char D_253A984[] = _("{14FE}{146C}{146C}{146C}{146C}{146C}{146C}{146C}{FFFE}");
const char D_253A998[] = _("{18E0}{1801}{1802}{1803}{1804}{18E1}{186C}{18E2}{1805}{18E3}{18E4}{1806}{1807}{186C}{18E5}{18E6}{18E7}{18E8}{1808}{18E9}{186C}{18EA}{18EB}{18EC}{18ED}{18EE}{1809}{186C}{180A}{180B}{18EF}{18F0}{18F1}{18F2}{186C}{18F3}{18F4}{180C}{18F5}{18F6}{18F7}{186C}{18F8}{18F9}{186C}{FFFE}");
const char D_253A9F8[] = _("{1851}{1852}{1853}{1854}{1855}{1856}{186C}{1857}{1858}{1859}{185A}{185B}{185C}{186C}{185D}{185E}{185F}{1860}{1861}{1862}{186C}{1863}{1864}{1865}{1866}{1867}{1868}{186C}{1869}{186A}{186B}{186C}{FFFE}");
const char D_253AA3C[] = _("{186E}{186F}{1870}{1871}{1872}{1873}{186C}{1874}{1875}{1876}{186D}{186C}{FFFE}");
const char D_253AA58[] = _("{18D0}{18D1}{18D2}{18D3}{18D4}{18D5}{186C}{18D6}{18D7}{18D8}{18D9}{18DA}{18DB}{186C}{18DC}{18DD}{18DE}{18DF}{186C}{FFFE}");
const char D_253AA84[] = _("{18FE}{186C}{186C}{186C}{186C}{186C}{186C}{186C}{FFFE}");
const char D_253AA98[] = _("{1CE0}{1C01}{1CE1}{1CE2}{1CE3}{1CE4}{1C6C}{1CE5}{1CE6}{1CE7}{1CE8}{1CE9}{1CEA}{1C6C}{1CEB}{1CEC}{1CED}{1CEE}{1C02}{1CEF}{1C6C}{1CF0}{1CF1}{1CF2}{1CF3}{1CF4}{1C03}{1C6C}{1CF5}{1CF6}{1CF7}{1CF8}{1CF9}{1C6C}{FFFE}");
const char D_253AAE0[] = _("{1C51}{1C52}{1C53}{1C54}{1C55}{1C56}{1C6C}{1C57}{1C58}{1C59}{1C5A}{1C5B}{1C5C}{1C6C}{1C5D}{1C5E}{1C5F}{1C60}{1C61}{1C62}{1C6C}{1C63}{1C64}{1C65}{1C66}{1C67}{1C68}{1C6C}{1C69}{1C6A}{1C6B}{1C6C}{FFFE}");
const char D_253AB24[] = _("{1C6E}{1C6F}{1C70}{1C71}{1C72}{1C73}{1C6C}{1C74}{1C75}{1C76}{1C6D}{1C6C}{FFFE}");
const char D_253AB40[] = _("{1CD0}{1CD1}{1CD2}{1CD3}{1CD4}{1CD5}{1C6C}{1CD6}{1CD7}{1CD8}{1CD9}{1CDA}{1CDB}{1C6C}{1CDC}{1CDD}{1CDE}{1CDF}{1C6C}{FFFE}");
const char D_253AB6C[] = _("{1CFE}{1C6C}{1C6C}{1C6C}{1C6C}{1C6C}{1C6C}{1C6C}{FFFE}");
const char D_253AB80[] = _("{20E0}{2001}{20E1}{20E2}{2002}{20E3}{206C}{20E4}{2003}{20E5}{20E6}{20E7}{20E8}{206C}{20E9}{20EA}{20EB}{2004}{20EC}{20ED}{206C}{2005}{20EE}{2006}{20EF}{20F0}{20F1}{206C}{20F2}{2007}{20F3}{20F4}{20F5}{20F6}{206C}{20F7}{20F8}{20F9}{2008}{2009}{206C}{FFFE}");
const char D_253ABD8[] = _("{2051}{2052}{2053}{2054}{2055}{2056}{206C}{2057}{2058}{2059}{205A}{205B}{205C}{206C}{205D}{205E}{205F}{2060}{2061}{2062}{206C}{2063}{2064}{2065}{2066}{2067}{2068}{206C}{2069}{206A}{206B}{206C}{FFFE}");
const char D_253AC1C[] = _("{206E}{206F}{2070}{2071}{2072}{2073}{206C}{2074}{2075}{2076}{206D}{206C}{FFFE}");
const char D_253AC38[] = _("{20D0}{20D1}{20D2}{20D3}{20D4}{20D5}{206C}{20D6}{20D7}{20D8}{20D9}{20DA}{20DB}{206C}{20DC}{20DD}{20DE}{20DF}{206C}{FFFE}");
const char D_253AC64[] = _("{20FE}{206C}{206C}{206C}{206C}{206C}{206C}{206C}{FFFE}");
const char D_253AC78[] = _("{2401}{2402}{2403}{2404}{2405}{2406}{246C}{2407}{2408}{2409}{240A}{240B}{240C}{246C}{240D}{240E}{240F}{2410}{2411}{2412}{246C}{2413}{2414}{2415}{2416}{2417}{2418}{246C}{2419}{241A}{241B}{241C}{241D}{241E}{246C}{241F}{2420}{2421}{246C}{FFFE}");
const char D_253ACCC[] = _("{24E0}{24E1}{24E2}{24E3}{24E4}{24E5}{246C}{24E6}{24E7}{24E8}{24E9}{24EA}{24EB}{246C}{24EC}{24ED}{24EE}{24EF}{24F0}{24F1}{246C}{24F2}{24F3}{24F4}{24F5}{24F6}{24F7}{246C}{24F8}{24F9}{246C}{FFFE}");
const char D_253AD10[] = _("{2451}{2452}{2453}{2454}{2455}{2456}{246C}{2457}{2458}{2459}{245A}{245B}{245C}{246C}{245D}{245E}{245F}{2460}{2461}{2462}{246C}{2463}{2464}{2465}{2466}{2467}{2468}{246C}{2469}{246A}{246B}{246C}{FFFE}");
const char D_253AD54[] = _("{246E}{246F}{2470}{2471}{2472}{2473}{246C}{2474}{2475}{2476}{246D}{246C}{FFFE}");
const char D_253AD70[] = _("{24D0}{24D1}{24D2}{24D3}{24D4}{24D5}{246C}{24D6}{24D7}{24D8}{24D9}{24DA}{24DB}{246C}{24DC}{24DD}{24DE}{24DF}{246C}{FFFE}");
const char D_253AD9C[] = _("{24FE}{246C}{246C}{246C}{246C}{246C}{246C}{246C}{FFFE}");
const char D_253ADB0[] = _("あいうえお ぁぃぅぇぉ {FFFE}");
const char D_253ADCC[] = _("かきくけこ がぎぐげご {FFFE}");
const char D_253ADE8[] = _("さしすせそ ざじずぜぞ {FFFE}");
const char D_253AE04[] = _("たちつてと っだぢづでど {FFFE}");
const char D_253AE24[] = _("なにぬねの なにぬねの {FFFE}");
const char D_253AE40[] = _("はひふへほ ばびぶべぼ ぱぴぷぺぽ {FFFE}");
const char D_253AE68[] = _("まみむめも まみむめも {FFFE}");
const char D_253AE84[] = _("やゆよ ゃゅょ {FFFE}");
const char D_253AE98[] = _("らりるれろ らりるれろ {FFFE}");
const char D_253AEB4[] = _("わをん わをん {FFFE}");
const char D_253AEC8[] = _("ー～…､｡( )｢｣.・! ?&⬤✝♥☼ ★@♪╬↑↓ →←$ {FFFE}");
const char D_253AF0C[] = _("{D0}{D1}{D2}{D3}{D4}{D5} {D6}{D7}{D8}{D9}{DA}{DB} {DC}{DD}{DE}{DF} {FFFE}");
const char D_253AF38[] = _("アイウエオ ァィゥェォ{C7} {FFFE}");
const char D_253AF58[] = _("カキクケコ ガギグゲゴ {FFFE}");
const char D_253AF74[] = _("サシスセソ ザジズゼゾ {FFFE}");
const char D_253AF90[] = _("タチツテト ッダヂヅデド {FFFE}");
const char D_253AFB0[] = _("ナニヌネノ ナニヌネノ {FFFE}");
const char D_253AFCC[] = _("ハヒフヘホ バビブベボ パピプペポ {FFFE}");
const char D_253AFF4[] = _("マミムメモ マミムメモ {FFFE}");
const char D_253B010[] = _("ヤユヨ ャュョ {FFFE}");
const char D_253B024[] = _("ラリルレロ ラリルレロ {FFFE}");
const char D_253B040[] = _("ワヲン ワヲン {FFFE}");
const char D_253B054[] = _("123456 7890 {FFFE}");
const char D_253B070[] = _("ABCDEF GHIJKL MNOPQR STUVWX YZ {FFFE}");
const char D_253B0B4[] = _("{FE}       {FFFE}");

extern const char D_253A5C4[];

extern const char D_253A608[];

extern const char D_253A64C[];

extern const char D_253A668[];

extern const char D_253A694[];
extern const char D_253A6A8[];
extern const char D_253A710[];
extern const char D_253A754[];
extern const char D_253A770[];
extern const char D_253A79C[];
extern const char D_253A7B0[];
extern const char D_253A804[];
extern const char D_253A848[];
extern const char D_253A864[];
extern const char D_253A890[];
extern const char D_253A8A4[];
extern const char D_253A8F8[];
extern const char D_253A93C[];
extern const char D_253A958[];
extern const char D_253A984[];
extern const char D_253A998[];
extern const char D_253A9F8[];
extern const char D_253AA3C[];
extern const char D_253AA58[];
extern const char D_253AA84[];
extern const char D_253AA98[];
extern const char D_253AAE0[];
extern const char D_253AB24[];
extern const char D_253AB40[];
extern const char D_253AB6C[];
extern const char D_253AB80[];
extern const char D_253ABD8[];
extern const char D_253AC1C[];
extern const char D_253AC38[];
extern const char D_253AC64[];
extern const char D_253AC78[];
extern const char D_253ACCC[];
extern const char D_253AD10[];
extern const char D_253AD54[];
extern const char D_253AD70[];
extern const char D_253AD9C[];
extern const char D_253ADB0[];
extern const char D_253ADCC[];
extern const char D_253ADE8[];
extern const char D_253AE04[];
extern const char D_253AE24[];
extern const char D_253AE40[];
extern const char D_253AE68[];
extern const char D_253AE84[];
extern const char D_253AE98[];
extern const char D_253AEB4[];
extern const char D_253AEC8[];
extern const char D_253AF0C[];
extern const char D_253AF38[];
extern const char D_253AF58[];
extern const char D_253AF74[];
extern const char D_253AF90[];
extern const char D_253AFB0[];
extern const char D_253AFCC[];
extern const char D_253AFF4[];
extern const char D_253B010[];
extern const char D_253B024[];
extern const char D_253B040[];
extern const char D_253B054[];
extern const char D_253B070[];
extern const char D_253B0B4[];
/*
 * Text input: the character table a (0..4) of the keyboard layout txt_input_charset
 * (8, 12, 16, 20, 24, ...; tables D_253A5C4...).
 */
void txt_input_load_grid(u8 a)
{
    const char *s = 0;

    switch (txt_input_charset) {
    case 8:
        switch (a) {
        case 0: s = D_253A5C4; break;
        case 1: s = D_253A608; break;
        case 2: s = D_253A64C; break;
        case 3: s = D_253A668; break;
        case 4: s = D_253A694; break;
        }
        break;
    case 12:
        switch (a) {
        case 0: s = D_253A6A8; break;
        case 1: s = D_253A710; break;
        case 2: s = D_253A754; break;
        case 3: s = D_253A770; break;
        case 4: s = D_253A79C; break;
        }
        break;
    case 16:
        switch (a) {
        case 0: s = D_253A7B0; break;
        case 1: s = D_253A804; break;
        case 2: s = D_253A848; break;
        case 3: s = D_253A864; break;
        case 4: s = D_253A890; break;
        }
        break;
    case 20:
        switch (a) {
        case 0: s = D_253A8A4; break;
        case 1: s = D_253A8F8; break;
        case 2: s = D_253A93C; break;
        case 3: s = D_253A958; break;
        case 4: s = D_253A984; break;
        }
        break;
    case 24:
        switch (a) {
        case 0: s = D_253A998; break;
        case 1: s = D_253A9F8; break;
        case 2: s = D_253AA3C; break;
        case 3: s = D_253AA58; break;
        case 4: s = D_253AA84; break;
        }
        break;
    case 28:
        switch (a) {
        case 0: s = D_253AA98; break;
        case 1: s = D_253AAE0; break;
        case 2: s = D_253AB24; break;
        case 3: s = D_253AB40; break;
        case 4: s = D_253AB6C; break;
        }
        break;
    case 32:
        switch (a) {
        case 0: s = D_253AB80; break;
        case 1: s = D_253ABD8; break;
        case 2: s = D_253AC1C; break;
        case 3: s = D_253AC38; break;
        case 4: s = D_253AC64; break;
        }
        break;
    case 36:
        switch (a) {
        case 0: s = D_253AC78; break;
        case 1: s = D_253ACCC; break;
        case 2: s = D_253AD10; break;
        case 3: s = D_253AD54; break;
        case 4: s = D_253AD70; break;
        case 5: s = D_253AD9C; break;
        }
        break;
    default:
        switch (a) {
        case 0: s = D_253ADB0; break;
        case 1: s = D_253ADCC; break;
        case 2: s = D_253ADE8; break;
        case 3: s = D_253AE04; break;
        case 4: s = D_253AE24; break;
        case 5: s = D_253AE40; break;
        case 6: s = D_253AE68; break;
        case 7: s = D_253AE84; break;
        case 8: s = D_253AE98; break;
        case 9: s = D_253AEB4; break;
        case 10: s = D_253AEC8; break;
        case 11: s = D_253AF0C; break;
        case 12: s = D_253AF38; break;
        case 13: s = D_253AF58; break;
        case 14: s = D_253AF74; break;
        case 15: s = D_253AF90; break;
        case 16: s = D_253AFB0; break;
        case 17: s = D_253AFCC; break;
        case 18: s = D_253AFF4; break;
        case 19: s = D_253B010; break;
        case 20: s = D_253B024; break;
        case 21: s = D_253B040; break;
        case 22: s = D_253B054; break;
        case 23: s = D_253B070; break;
        case 24: s = D_253B0B4; break;
        }
        break;
    }
    txt_input_grid_len = (int)strlen(s) >> 1;
    strcpy(txt_input_grid_chars, s);
}

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 txt_input_charset DSTRAM;
u8 txt_input_char_objs[8] DSTRAM;
u8 txt_input_grid_objs[8] DSTRAM;
u8 txt_input_max_len DSTRAM;
s8 txt_input_pos DSTRAM;
u8 txt_input_page_count DSTRAM;
u8 txt_input_grid_len DSTRAM;
char txt_input_grid_chars[101] DSTRAM;
u8 txt_input_hold_count DSTRAM;
u8 txt_input_prompt DSTRAM;
