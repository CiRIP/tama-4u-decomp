/*
 * The owner's profile editor and the friend profile view.
 *
 * Code 251149E..2512CBC, .rodata from 255D0A4.
 */
#include "proto/owner/owner_profile.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/comm/comm_menu.h"
#include "proto/txt/txt_input.h"
#include "proto/owner/owner_mood.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/mail/mail_edit.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"
#include "libc.h"

/*
 * F47 = the OWNER PROFILE ("プロフィール") editor and viewer. The profile is owner_profile_bytes[4]
 * = {gender 0 unset / 1 girl / 2 boy, mood 1..6 (さいこー! .. しょんぼり), age 0..99,
 * star sign 1..12 (おひつじざ .. うおざ)} + pet.owner_profile_texts = five 16-character texts (name
 * of the owner's choice and four free lines; 0x6C = blank), the same bytes that travel
 * in section 2 of the NFC exchange record and are shown on a friend's card. The work
 * copy lives at owner_work_copy (0x54 bytes), the character record keeps a copy at
 * pet + 0x158.
 *
 * Profile editor: three pages (A cycles; B edits): 0 gender (おんなのこ/おとこのこ) +
 * mood (owner_edit_page0), 1 birthday (fixed, the owner's pet.owner_bday_month/E0), age, star sign and
 * text 1 (owner_edit_page1), 2 texts 2..5 (owner_edit_page2). Pages are drawn by owner_draw_page
 * with the "empty" placeholders D_255D1D0; each edit saves. Returns -1 on timeout.
 */
int owner_editor(void)
{
    u8 flags[4];
    u8 buf[0x54];
    int r = 0;
    u8 sel = 0;
    int redraw = 1;
    u8 i;
    u8 j;
    u8 c;

    owner_work_copy = (u32)buf;
    btn_clear();
    ui_timeout = 10;
    do {
        ((u8 *)owner_work_copy)[0] = save.pet.profile_gender;
        ((u8 *)owner_work_copy)[1] = save.pet.profile_mood;
        ((u8 *)owner_work_copy)[2] = save.pet.profile_age;
        ((u8 *)owner_work_copy)[3] = save.pet.profile_sign;
        memcpy((u8 *)owner_work_copy + 4, save.pet.owner_profile_texts, 0x50);
        if (((u8 *)owner_work_copy)[0] > 2)
            ((u8 *)owner_work_copy)[0] = 0;
        if (((u8 *)owner_work_copy)[1] > 6)
            ((u8 *)owner_work_copy)[1] = 0;
        if (((u8 *)owner_work_copy)[3] > 12)
            ((u8 *)owner_work_copy)[3] = 0;
        for (i = 0; i <= 4; i++) {
            flags[0] = 1;
            for (j = 0; j <= 15; j++) {
                c = ((u8 *)owner_work_copy + i * 16)[j + 4];
                if (c != 0 && c != 0x6c) {
                    flags[0] = 0;
                    break;
                }
            }
            if (flags[0])
                memcpy((u8 *)owner_work_copy + i * 16 + 4, (u8 *)D_255D1D0, strlen((char *)D_255D1D0));
        }
        owner_empty_pages((int)flags);
        if (redraw) {
            owner_draw_page(sel, flags[sel], 1, 0xff);
            redraw = 0;
        }
        if (btn_a_presses != 0) {
            sel = sel > 1 ? 0 : sel + 1;
            redraw = 1;
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            switch (sel) {
            case 0:
                r = owner_edit_page0();
                break;
            case 1:
                r = owner_edit_page1();
                break;
            case 2:
                r = owner_edit_page2();
                break;
            }
            if (r > 0) {
                snd_wait_end();
                save_game();
            } else if (r < 0)
                goto out;
            redraw = 1;
            btn_b_presses = 0;
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
 * Which profile pages are still empty, into a[0..2]: [0] gender and mood unset, [1] age
 * invalid / sign unset / text 1 blank, [2] all of texts 2..5 blank.
 */
void owner_empty_pages(int a)
{
    u8 *out = (u8 *)a;
    u8 i;
    u8 j;
    u8 *p;

    for (i = 0; i <= 2; i++, out++) {
        switch (i) {
        case 0:
            p = (u8 *)owner_work_copy;
            *out = p[0] == 0 && p[1] == 0;
            break;
        case 1:
            p = (u8 *)owner_work_copy;
            *out = p[2] > 0x63 && p[3] == 0;
            if (*out)
                *out = memcmp((u8 *)owner_work_copy + 4, D_255D1D0, 16) == 0;
            break;
        case 2:
            for (j = 1; j <= 4; j++) {
                *out = memcmp((u8 *)owner_work_copy + j * 16 + 4, D_255D1D0, 16) == 0;
                if (!*out)
                    return;
            }
            break;
        }
    }
}

/*
 * Draw profile page a (b = page empty -> the "fill me in" background, c = page arrow,
 * d = the text field being edited, highlighted with flag 0x2000): page 0 mood icon
 * (owner_mood_icon_value), owner name pet.owner_name, gender / mood labels (owner_gender_labels / owner_mood_label_tbl);
 * page 1 birthday (pet.owner_bday_month/D_00011E0) digits (owner_digit_pic = digit images), age, star
 * sign (owner_star_sign_labels), text 1; page 2 texts 2..5. Sprite ids in owner_page_objs.
 */
void owner_draw_page(u8 a, u8 b, u8 c, u8 d)
{
    u8 e = save.pet.owner_bday_month;
    u8 f = save.pet.owner_bday_day;
    u8 buf[17];

    ui_new_screen();
    memset(&owner_page_objs, 0, 11);
    switch (a) {
    case 0:
        owner_page_objs.background = gfx_object_create(0, 0x1c, 0, 0, 0, (const u8 *)(b != 0 ? 0x20fd2dc : (((u8 *)owner_work_copy)[1] == 0 ? 0x20fc0c8 : 0x2104794)));
        owner_page_objs.arrow = gfx_object_create(0x72, 0x58, 0x20, 0, 0, (const u8 *)(c != 0 ? 0x21059a4 : 0));
        owner_page_objs.mood_icon = gfx_object_create(0x4d, 0x38, 0x30, 0, 0, (const u8 *)owner_mood_icon_value(((u8 *)owner_work_copy)[1]));
        memset(buf, 0, 0x11);
        memcpy(buf, save.pet.owner_name, 0x10);
        owner_page_objs.name = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_line(owner_page_objs.name, 9, 0x23, 0x40, (int)buf, 0, 8);
        if (b == 0) {
            owner_page_objs.gender = gfx_object_create(0, 0, 0, 0, 0, 0);
            gfx_draw_text_line(owner_page_objs.gender, 9, 0x38, 0x40, (int)owner_gender_labels[*(u8 *)owner_work_copy], 0, 5);
            owner_page_objs.mood = gfx_object_create(0, 0, 0, 0, 0, 0);
            gfx_draw_text_line(owner_page_objs.mood, 9, 0x4d, 0x40, (int)owner_mood_label_tbl[((u8 *)owner_work_copy)[1]], 1, 7);
        }
        break;
    case 1:
        owner_page_objs.background = gfx_object_create(0, 0x1c, 0, 0, 0, (const u8 *)(b != 0 ? 0x20f6608 : 0x20f53e4));
        owner_page_objs.arrow = gfx_object_create(0x72, 0x58, 0x20, 0, 0, (const u8 *)(c != 0 ? 0x21059a4 : 0));
        owner_page_objs.mood_icon = gfx_object_create(0x18, 0x22, 0x30, 0, 0, (const u8 *)(e > 9 ? owner_digit_pic(1) : 0));
        owner_page_objs.name = gfx_object_create(0x21, 0x22, 0x30, 0, 0, (const u8 *)owner_digit_pic(e % 10));
        owner_page_objs.gender = gfx_object_create(0x3a, 0x22, 0x30, 0, 0, (const u8 *)(f > 9 ? owner_digit_pic(f / 10) : 0));
        owner_page_objs.mood = gfx_object_create(0x43, 0x22, 0x30, 0, 0, (const u8 *)owner_digit_pic(f % 10));
        if (b == 0) {
            if (((u8 *)owner_work_copy)[2] <= 99) {
                int t = ((u8 *)owner_work_copy)[2] > 9 ? owner_digit_pic(((u8 *)owner_work_copy)[2] / 10) : 0;
                owner_page_objs.age_tens = gfx_object_create(0x18, 0x32, 0x30, 0, 0, (const u8 *)t);
                owner_page_objs.age_ones = gfx_object_create(0x21, 0x32, 0x30, 0, 0, (const u8 *)owner_digit_pic(((u8 *)owner_work_copy)[2] % 10));
                owner_page_objs.age_unit = gfx_object_create(0x2b, 0x32, 0x30, 0, 0, D_20F5384);
            } else {
                owner_page_objs.age_tens = gfx_object_create(0x18, 0x32, 0x30, 0, 0, 0);
                owner_page_objs.age_ones = gfx_object_create(0x21, 0x32, 0x30, 0, 0, 0);
                owner_page_objs.age_unit = gfx_object_create(0x2b, 0x32, 0x30, 0, 0, 0);
            }
            owner_page_objs.sign = gfx_object_create(0, 0, 0, 0, 0, 0);
            gfx_draw_text_line(owner_page_objs.sign, 0x19, 0x41, 0x30, (int)owner_star_sign_labels[((u8 *)owner_work_copy)[3]], 0, 8);
            memset(buf, 0, 0x11);
            memcpy(buf, (u8 *)owner_work_copy + 4, 0x10);
            owner_page_objs.text1 = gfx_object_create(0, 0, 0, 0, 0, 0);
            if (d == 0)
                gfx_object_set_flags(owner_page_objs.text1, 0x2000);
            gfx_draw_text_line(owner_page_objs.text1, 0x19, 0x51, 0x30, (int)buf, 0, 7);
        }
        break;
    case 2:
        owner_page_objs.background = gfx_object_create(0, 0x1c, 0, 0, 0, (const u8 *)(b != 0 ? 0x20f9c78 : 0x20f8a54));
        owner_page_objs.arrow = gfx_object_create(0x72, 0x58, 0x20, 1, 0, (const u8 *)(c != 0 ? 0x21059a4 : 0));
        if (b == 0) {
            memset(buf, 0, 0x11);
            memcpy(buf, (u8 *)owner_work_copy + 0x14, 0x10);
            owner_page_objs.mood_icon = gfx_object_create(0, 0, 0, 0, 0, 0);
            if (d == 1)
                gfx_object_set_flags(owner_page_objs.mood_icon, 0x2000);
            gfx_draw_text_line(owner_page_objs.mood_icon, 0x19, 0x21, 0x30, (int)buf, 0, 8);
            memset(buf, 0, 0x11);
            memcpy(buf, (u8 *)owner_work_copy + 0x24, 0x10);
            owner_page_objs.name = gfx_object_create(0, 0, 0, 0, 0, 0);
            if (d == 2)
                gfx_object_set_flags(owner_page_objs.name, 0x2000);
            gfx_draw_text_line(owner_page_objs.name, 0x19, 0x31, 0x30, (int)buf, 0, 8);
            memset(buf, 0, 0x11);
            memcpy(buf, (u8 *)owner_work_copy + 0x34, 0x10);
            owner_page_objs.gender = gfx_object_create(0, 0, 0, 0, 0, 0);
            if (d == 3)
                gfx_object_set_flags(owner_page_objs.gender, 0x2000);
            gfx_draw_text_line(owner_page_objs.gender, 0x19, 0x41, 0x30, (int)buf, 0, 8);
            memset(buf, 0, 0x11);
            memcpy(buf, (u8 *)owner_work_copy + 0x44, 0x10);
            owner_page_objs.mood = gfx_object_create(0, 0, 0, 0, 0, 0);
            if (d == 4)
                gfx_object_set_flags(owner_page_objs.mood, 0x2000);
            gfx_draw_text_line(owner_page_objs.mood, 0x19, 0x51, 0x30, (int)buf, 0, 7);
        }
        break;
    }
    func_00009A6();
}

/*
 * Profile page 0 editor: A moves between gender / mood (or, while editing, steps the
 * value), B enters / confirms a field (blinking via the timer-5 flag), C leaves the field
 * / the page. Returns 1 = changed, -1 = timeout.
 */
int owner_edit_page0(void)
{
    int r = 0;
    int val;
    int max;
    int f6;
    int f7;
    int f8;
    int sel;
    int redraw;
    u8 c;
    int i;

    sel = r;
    redraw = 1;
    f6 = 1;
    f7 = 1;
    f8 = 1;
    if (*(u8 *)owner_work_copy != 2)
        *(u8 *)owner_work_copy = 1;
    owner_draw_page(0, 0, 0, 0xff);
    btn_clear();
    tmr_t5_start_timeout(500);
    ui_timeout = 10;
    do {
        if (tmr_flags & 0x20) {
            tmr_flags &= ~0x20;
            if (f7 == 0) {
                f6 = f6 ? 0 : 1;
                redraw = 1;
            }
        }
        if (btn_a_presses != 0) {
            if (f7 != 0) {
                sel = sel > 0 ? 0 : sel + 1;
                f6 = 1;
                f8 = 1;
            } else {
                val = val >= max ? 1 : val + 1;
            }
            btn_a_presses = 0;
            ui_timeout = 10;
            redraw = 1;
        }
        if (btn_b_presses != 0) {
            if (f7 != 0) {
                f6 = 0;
                f7 = 0;
                redraw = 1;
            } else {
                r = 1;
                switch (sel) {
                case 0:
                    *(u8 *)owner_work_copy = val;
                    save.pet.profile_gender = *(u8 *)owner_work_copy;
                    max = 6;
                    break;
                case 1:
                    ((u8 *)owner_work_copy)[1] = val;
                    *(u8 *)&save.pet.profile_mood = ((u8 *)owner_work_copy)[1];
                    break;
                }
                if (sel > 1)
                    goto out;
                f7 = 1;
                f8 = 1;
                redraw = 1;
                f6 = 1;
            }
            btn_b_presses = 0;
            ui_timeout = 10;
        }
        if (btn_c_presses != 0) {
            if (f7 != 0)
                goto out;
            f6 = 1;
            f7 = 1;
            redraw = 1;
            switch (sel) {
            case 0:
                val = *(u8 *)owner_work_copy;
                break;
            case 1:
                val = ((u8 *)owner_work_copy)[1];
                break;
            }
            f8 = 1;
            btn_c_presses = 0;
            ui_timeout = 10;
        }
        if (f8 != 0) {
            switch (sel) {
            case 0:
                val = *(u8 *)owner_work_copy;
                max = 2;
                c = ((u8 *)owner_work_copy)[1];
                break;
            case 1:
                val = ((u8 *)owner_work_copy)[1];
                if (val == 0)
                    val = 1;
                max = 6;
                c = val;
                break;
            }
            gfx_object_set_image(owner_page_objs.background, 0, 0, (const u8 *)(c != 0 ? 0x2104794 : 0x20fc0c8));
            f8 = 0;
        }
        if (redraw != 0) {
            if (f7 != 0) {
                for (i = 0; i <= 1; i++) {
                    switch (i) {
                    case 0:
                        c = i == sel ? val : *(u8 *)owner_work_copy;
                        gfx_object_set_flags(owner_page_objs.gender, (i == sel && f6) ? 0x2000 : 0);
                        gfx_draw_text_line(owner_page_objs.gender, 9, 0x38, 0x40, (int)owner_gender_labels[c], 0, 5);
                        break;
                    case 1:
                        c = i == sel ? val : ((u8 *)owner_work_copy)[1];
                        gfx_object_set_image(owner_page_objs.mood_icon, 0x30, 0, (const u8 *)owner_mood_icon_value(c));
                        gfx_draw_text_line(owner_page_objs.name, 9, 0x23, 0x40, (int)save.pet.owner_name, 0, 8);
                        gfx_object_set_flags(owner_page_objs.mood, (i == sel && f6) ? 0x2000 : 0);
                        gfx_draw_text_line(owner_page_objs.mood, 9, 0x4d, 0x40, (int)owner_mood_label_tbl[c], 1, 7);
                        break;
                    }
                }
            } else {
                switch (sel) {
                case 0:
                    gfx_object_set_flags(owner_page_objs.gender, f6 ? 0x2000 : 0);
                    gfx_draw_text_line(owner_page_objs.gender, 9, 0x38, 0x40, (int)owner_gender_labels[val], 0, 5);
                    break;
                case 1:
                    gfx_object_set_image(owner_page_objs.mood_icon, 0x30, 0, (const u8 *)owner_mood_icon_value(val));
                    gfx_draw_text_line(owner_page_objs.name, 9, 0x23, 0x40, (int)save.pet.owner_name, 0, 8);
                    gfx_object_set_flags(owner_page_objs.mood, f6 ? 0x2000 : 0);
                    gfx_draw_text_line(owner_page_objs.mood, 9, 0x4d, 0x40, (int)owner_mood_label_tbl[val], 1, 7);
                    break;
                }
            }
            func_00009A6();
            redraw = 0;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
    r = -1;
out:
    btn_clear();
    return r;
}

/*
 * Profile page 1 editor: age tens / ones, star sign, text 1 via the text editor
 * txt_input(4, 1, ..).
 */
int owner_edit_page1(void)
{
    int r = 0;
    int val;
    int max;
    int f6;
    int f7;
    int f8;
    int sel;
    int redraw;
    int t;
    u8 c;
    int i;

    sel = r;
    redraw = 1;
    f6 = 1;
    f7 = 1;
    f8 = 1;
    if (((u8 *)owner_work_copy)[2] > 99)
        ((u8 *)owner_work_copy)[2] = 0;
    if ((u8)(((u8 *)owner_work_copy)[3] - 1) > 11)
        ((u8 *)owner_work_copy)[3] = 1;
    owner_draw_page(1, 0, 0, 0xff);
    btn_clear();
    tmr_t5_start_timeout(500);
    ui_timeout = 10;
    do {
    top:
        if (tmr_flags & 0x20) {
            tmr_flags &= ~0x20;
            if (f7 == 0) {
                f6 = f6 ? 0 : 1;
                redraw = 1;
            }
        }
        if (btn_a_presses != 0) {
            if (f7 != 0) {
                sel = sel > 2 ? 0 : sel + 1;
                f6 = 1;
                f8 = 1;
            } else {
                val = val >= max ? (sel > 1 ? 1 : 0) : val + 1;
            }
            btn_a_presses = 0;
            ui_timeout = 10;
            redraw = 1;
        }
        if (btn_b_presses != 0) {
            if (f7 != 0) {
                f7 = 0;
                if (sel == 3)
                    goto top;
                tmr_t5_start_timeout(500);
            } else {
                r = 1;
                switch (sel) {
                case 0:
                    ((u8 *)owner_work_copy)[2] = val * 10 + ((u8 *)owner_work_copy)[2] % 10;
                    save.pet.profile_age = ((u8 *)owner_work_copy)[2];
                    break;
                case 1:
                    ((u8 *)owner_work_copy)[2] = ((u8 *)owner_work_copy)[2] - ((u8 *)owner_work_copy)[2] % 10 + val;
                    save.pet.profile_age = ((u8 *)owner_work_copy)[2];
                    break;
                case 2:
                    ((u8 *)owner_work_copy)[3] = val;
                    save.pet.profile_sign = ((u8 *)owner_work_copy)[3];
                    break;
                case 3:
                    r = txt_input(4, 1, owner_work_copy + 4);
                    if (r > 0) {
                        memcpy(save.pet.owner_profile_texts, (u8 *)owner_work_copy + 4, 16);
                        goto redraw3;
                    }
                    if (r == 0) {
                    redraw3:
                        owner_draw_page(1, 0, 0, 0);
                    } else {
                        ui_timeout = 0;
                    }
                    break;
                }
                if (sel > 3 || r < 0)
                    goto out;
                f7 = 1;
                f8 = 1;
            }
            btn_b_presses = 0;
            ui_timeout = 10;
            redraw = 1;
            f6 = 1;
        }
        if (btn_c_presses != 0) {
            if (f7 != 0)
                goto out;
            f6 = 1;
            f7 = 1;
            redraw = 1;
            switch (sel) {
            case 0:
                val = ((u8 *)owner_work_copy)[2] / 10;
                break;
            case 1:
                val = ((u8 *)owner_work_copy)[2] % 10;
                break;
            case 2:
                val = ((u8 *)owner_work_copy)[3];
                break;
            }
            btn_c_presses = 0;
            ui_timeout = 10;
        }
        if (f8 != 0) {
            switch (sel) {
            case 0:
                val = ((u8 *)owner_work_copy)[2] / 10;
                max = 9;
                break;
            case 1:
                val = ((u8 *)owner_work_copy)[2] % 10;
                max = 9;
                break;
            case 2:
                val = ((u8 *)owner_work_copy)[3];
                max = 12;
                break;
            case 3:
                val = 0;
                max = 0;
                break;
            }
            f8 = 0;
        }
        if (redraw != 0) {
            if (f7 != 0) {
                for (i = 0; i <= 3; i++) {
                    switch (i) {
                    case 0:
                        c = i == sel ? val : ((u8 *)owner_work_copy)[2] / 10;
                        t = (int)D_255D0A8;
                        if (c != 0)
                            t = owner_digit_pic(c);
                        gfx_object_set_flags(owner_page_objs.age_tens, i == sel ? 0x2000 : 0);
                        gfx_object_set_image(owner_page_objs.age_tens, 0x30, 0, t);
                        break;
                    case 1:
                        c = i == sel ? val : ((u8 *)owner_work_copy)[2] % 10;
                        gfx_object_set_flags(owner_page_objs.age_ones, i == sel ? 0x2000 : 0);
                        gfx_object_set_image(owner_page_objs.age_ones, 0x30, 0, (const u8 *)owner_digit_pic(c));
                        break;
                    case 2:
                        c = i == sel ? val : ((u8 *)owner_work_copy)[3];
                        gfx_object_set_flags(owner_page_objs.sign, i == sel ? 0x2000 : 0);
                        gfx_draw_text_line(owner_page_objs.sign, 0x19, 0x41, 0x30, (int)owner_star_sign_labels[c], 0, 8);
                        break;
                    case 3:
                        gfx_object_set_flags(owner_page_objs.text1, i == sel ? 0x2000 : 0);
                        gfx_draw_text_line(owner_page_objs.text1, 0x19, 0x51, 0x30, (int)((u8 *)owner_work_copy + 4), 0, 7);
                        break;
                    }
                }
            } else {
                switch (sel) {
                case 0:
                    t = (int)D_255D0A8;
                    if (val != 0)
                        t = owner_digit_pic(val);
                    gfx_object_set_flags(owner_page_objs.age_tens, f6 ? 0x2000 : 0);
                    gfx_object_set_image(owner_page_objs.age_tens, 0x30, 0, t);
                    break;
                case 1:
                    gfx_object_set_flags(owner_page_objs.age_ones, f6 ? 0x2000 : 0);
                    gfx_object_set_image(owner_page_objs.age_ones, 0x30, 0, (const u8 *)owner_digit_pic(val));
                    break;
                case 2:
                    gfx_object_set_flags(owner_page_objs.sign, f6 ? 0x2000 : 0);
                    gfx_draw_text_line(owner_page_objs.sign, 0x19, 0x41, 0x30, (int)owner_star_sign_labels[val], 0, 8);
                    break;
                }
            }
            func_00009A6();
            redraw = 0;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
    r = -1;
out:
    ((u8 *)owner_work_copy)[2] = save.pet.profile_age;
    btn_clear();
    return r;
}

/*
 * Profile page 2 editor: texts 2..5, each via the text editor txt_input(4, 2..5, ..)
 * and copied to pet.owner_profile_texts when changed. Returns 1 = changed, -1 = timeout.
 */
int owner_edit_page2(void)
{
    int r = 0;
    int n;
    int sel;
    int redraw;
    int f6 = 1;
    int i;

    sel = r;
    redraw = 1;
    owner_draw_page(2, 0, 0, 0xff);
    btn_clear();
    tmr_t5_start_timeout(500);
    ui_timeout = 10;
    do {
        if (tmr_flags & 0x20)
            tmr_flags &= ~0x20;
        if (btn_a_presses != 0) {
            sel = sel > 2 ? 0 : sel + 1;
            btn_a_presses = 0;
            ui_timeout = 10;
            redraw = 1;
        }
        if (btn_b_presses != 0) {
            switch (sel) {
            case 0:
                n = txt_input(4, 2, owner_work_copy + 0x14);
                if (n > 0) {
                    memcpy(save.pet.owner_profile_texts + 0x10, (u8 *)owner_work_copy + 0x14, 16);
                    r = 1;
                }
                break;
            case 1:
                n = txt_input(4, 3, owner_work_copy + 0x24);
                if (n > 0) {
                    memcpy(save.pet.owner_profile_texts + 0x20, (u8 *)owner_work_copy + 0x24, 16);
                    r = 1;
                }
                break;
            case 2:
                n = txt_input(4, 4, owner_work_copy + 0x34);
                if (n > 0) {
                    memcpy(save.pet.owner_profile_texts + 0x30, (u8 *)owner_work_copy + 0x34, 16);
                    r = 1;
                }
                break;
            case 3:
                n = txt_input(4, 5, owner_work_copy + 0x44);
                if (n > 0) {
                    memcpy(save.pet.owner_profile_texts + 0x40, (u8 *)owner_work_copy + 0x44, 16);
                    r = 1;
                }
                break;
            }
            if (n < 0) {
                ui_timeout = 0;
                break;
            }
            owner_draw_page(2, 0, 0, sel + 1);
            if (sel > 3)
                sel = 0;
            btn_b_presses = 0;
            ui_timeout = 10;
            redraw = 1;
        }
        if (btn_c_presses != 0) {
            btn_c_presses = 0;
            ui_timeout = 10;
            goto out;
        }
        if (redraw != 0) {
            for (i = 0; i <= 3; i++) {
                switch (i) {
                case 0:
                    gfx_object_set_flags(owner_page_objs.mood_icon, (i == sel && f6) ? 0x2000 : 0);
                    gfx_draw_text_line(owner_page_objs.mood_icon, 0x19, 0x21, 0x30, (int)((u8 *)owner_work_copy + 0x14), 0, 8);
                    break;
                case 1:
                    gfx_object_set_flags(owner_page_objs.name, (i == sel && f6) ? 0x2000 : 0);
                    gfx_draw_text_line(owner_page_objs.name, 0x19, 0x31, 0x30, (int)((u8 *)owner_work_copy + 0x24), 0, 8);
                    break;
                case 2:
                    gfx_object_set_flags(owner_page_objs.gender, (i == sel && f6) ? 0x2000 : 0);
                    gfx_draw_text_line(owner_page_objs.gender, 0x19, 0x41, 0x30, (int)((u8 *)owner_work_copy + 0x34), 0, 8);
                    break;
                case 3:
                    gfx_object_set_flags(owner_page_objs.mood, (i == sel && f6) ? 0x2000 : 0);
                    gfx_draw_text_line(owner_page_objs.mood, 0x19, 0x51, 0x30, (int)((u8 *)owner_work_copy + 0x44), 0, 7);
                    break;
                }
            }
            func_00009A6();
            redraw = 0;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
    r = -1;
out:
    btn_clear();
    return r;
}

/*
 * Mood icon k: value t[k] of the 7-entry table owner_mood_icon_tbl (0 for k > 6).
 */
int owner_mood_icon_value(u8 k)
{
    int t[7];

    memcpy(t, owner_mood_icon_tbl, 0x1c);
    if (k > 6)
        return 0;
    return t[k];
}

/* blank digit picture (8 x 14) */
const u8 D_255D0A8[] = {
    0x08, 0x0e, 0x02, 0x00, 0x01, 0xff, 0x07, 0xe0, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};
const u8 D_255D0EC[] = _(" ");
const u8 D_255D0F0[] = _("おんなのこ");
const u8 D_255D0FC[] = _("おとこのこ");
const u8 D_255D108[] = _("さいこー!");
const u8 D_255D114[] = _("いいかんじ♪");
const u8 D_255D124[] = _("やるきまんまん");
const u8 D_255D134[] = _("ふつう");
const u8 D_255D13C[] = _("いまいち");
const u8 D_255D148[] = _("しょんぼり");
const u8 D_255D154[] = _("おひつじざ");
const u8 D_255D160[] = _("おうしざ");
const u8 D_255D16C[] = _("ふたござ");
const u8 D_255D178[] = _("かにざ");
const u8 D_255D180[] = _("ししざ");
const u8 D_255D188[] = _("おとめざ");
const u8 D_255D194[] = _("てんびんざ");
const u8 D_255D1A0[] = _("さそりざ");
const u8 D_255D1AC[] = _("いてざ");
const u8 D_255D1B4[] = _("やぎざ");
const u8 D_255D1BC[] = _("みずがめざ");
const u8 D_255D1C8[] = _("うおざ");
const u8 D_255D1D0[] = _("        ");
extern u8 D_20F37D0[];
extern u8 D_20F3A68[];
extern u8 D_20F3D00[];
extern u8 D_20F3F98[];
extern u8 D_20F4230[];
extern u8 D_20F29F0[];
const u8 *const owner_mood_icon_tbl[7] = {
    D_20F37D0, D_20F37D0, D_20F3A68, D_20F3D00, D_20F3F98, D_20F4230, D_20F29F0,
};

extern const u8 D_2128020[], D_2127DBC[], D_2127E00[], D_2127E44[], D_2127E88[];
extern const u8 D_2127ECC[], D_2127F10[], D_2127F54[], D_2127F98[], D_2127FDC[];
/*
 * Digit image k (0..9: the 10 pictures D_2128020, D_2127DBC ..; D_255D0A8 otherwise),
 * used for the birthday on profile page 1.
 */
const u8 *owner_digit_pic(u8 idx)
{
    const u8 *tbl[10] = {
        D_2128020, D_2127DBC, D_2127E00, D_2127E44, D_2127E88,
        D_2127ECC, D_2127F10, D_2127F54, D_2127F98, D_2127FDC,
    };
    if (idx > 9)
        return D_255D0A8;
    return tbl[idx];
}

/*
 * View friend a's profile: the 0x54 profile bytes and the 2-byte birthday are read from
 * the friend's portrait record (friend_picture_record(a) + 0xE00 / + 0xE54), the name from the
 * friend list; three pages drawn by friend_profile_draw_page ("????????" for unset fields).
 */
int friend_profile_view(int a)
{
    u8 s[0x14];
    u8 b[4];
    u8 flags[4];
    u8 buf[0x54];
    int r = 0;
    u8 sel = 0;
    int redraw = 1;
    int e;
    u8 i;
    u8 j;
    u8 c;

    owner_work_copy = (u32)buf;
    e = friend_picture_record(a);
    memcpy((u8 *)owner_work_copy, (u8 *)e + 0xe00, 0x54);
    memcpy(b, (u8 *)e + 0xe54, 2);
    memset(s, 0, 0x11);
    memcpy(s, save.friend_list[a].name, 0x10);
    if (((u8 *)owner_work_copy)[0] > 2)
        ((u8 *)owner_work_copy)[0] = 0;
    if (((u8 *)owner_work_copy)[1] > 6)
        ((u8 *)owner_work_copy)[1] = 0;
    if (((u8 *)owner_work_copy)[3] > 12)
        ((u8 *)owner_work_copy)[3] = 0;
    for (i = 0; i <= 4; i++) {
        flags[0] = 1;
        for (j = 0; j <= 15; j++) {
            c = ((u8 *)owner_work_copy + i * 16)[j + 4];
            if (c != 0 && c != 0x6c) {
                flags[0] = 0;
                break;
            }
        }
        if (flags[0])
            memcpy((u8 *)owner_work_copy + i * 16 + 4, (u8 *)D_255D1D0, strlen((char *)D_255D1D0));
    }
    btn_clear();
    btn_b_presses = 1;
    ui_timeout = 10;
    do {
        owner_empty_pages((int)flags);
        if (redraw) {
            friend_profile_draw_page(sel, flags[sel], s, b);
            redraw = 0;
        }
        if (btn_a_presses != 0) {
            sel = sel > 1 ? 0 : sel + 1;
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
 * Draw page a of a friend's profile (friend_profile_view), birthday from t.
 */
void friend_profile_draw_page(u8 a, int b, const u8 *src, const u8 *t)
{
    u8 e = t[0];
    u8 f = t[1];
    u8 buf[17];

    b = 0;
    memset(&owner_page_objs, b, 11);
    ui_new_screen();
    switch (a) {
    case 0:
        owner_page_objs.background = gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
        owner_page_objs.arrow = gfx_object_create(0x72, 0x58, 0x20, 0, 0, D_21059A4);
        owner_page_objs.mood_icon = gfx_object_create(0x4d, 0x38, 0x30, 0, 0, (const u8 *)owner_mood_icon_value(((u8 *)owner_work_copy)[1]));
        memset(buf, 0, 0x11);
        memcpy(buf, src, 0x10);
        owner_page_objs.name = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_line(owner_page_objs.name, 9, 0x23, 0x40, (int)buf, 0, 8);
        owner_page_objs.gender = gfx_object_create(0, 0, 0, 0, 0, 0);
        b = *(u8 *)owner_work_copy != 0 ? (int)owner_gender_labels[*(u8 *)owner_work_copy] : (int)D_255D228;
        gfx_draw_text_line(owner_page_objs.gender, 9, 0x38, 0x40, b, 0, 5);
        owner_page_objs.mood = gfx_object_create(0, 0, 0, 0, 0, 0);
        b = ((u8 *)owner_work_copy)[1] != 0 ? (int)owner_mood_label_tbl[((u8 *)owner_work_copy)[1]] : (int)D_255D228;
        gfx_draw_text_line(owner_page_objs.mood, 9, 0x4d, 0x40, b, 1, 7);
        break;
    case 1:
        owner_page_objs.background = gfx_object_create(0, 0x1c, 0, 0, 0, D_20F53E4);
        owner_page_objs.arrow = gfx_object_create(0x72, 0x58, 0x20, 0, 0, D_21059A4);
        owner_page_objs.mood_icon = gfx_object_create(0x18, 0x22, 0x30, 0, 0, (const u8 *)(e > 9 ? owner_digit_pic(1) : 0));
        owner_page_objs.name = gfx_object_create(0x21, 0x22, 0x30, 0, 0, (const u8 *)owner_digit_pic(e % 10));
        owner_page_objs.gender = gfx_object_create(0x3a, 0x22, 0x30, 0, 0, (const u8 *)(f > 9 ? owner_digit_pic(f / 10) : 0));
        owner_page_objs.mood = gfx_object_create(0x43, 0x22, 0x30, 0, 0, (const u8 *)owner_digit_pic(f % 10));
        if (((u8 *)owner_work_copy)[2] <= 99) {
            int v = ((u8 *)owner_work_copy)[2] > 9 ? owner_digit_pic(((u8 *)owner_work_copy)[2] / 10) : 0;
            owner_page_objs.age_tens = gfx_object_create(0x18, 0x32, 0x30, 0, 0, (const u8 *)v);
            owner_page_objs.age_ones = gfx_object_create(0x21, 0x32, 0x30, 0, 0, (const u8 *)owner_digit_pic(((u8 *)owner_work_copy)[2] % 10));
            owner_page_objs.age_unit = gfx_object_create(0x2b, 0x32, 0x30, 0, 0, D_20F5384);
        } else {
            int x = 0x18;
            owner_page_objs.age_tens = gfx_object_create(x, 0x32, 0x30, 0xc, 0, (const u8 *)&D_2090E0C);
            x += *(u8 *)&D_2090E0C + 1;
            owner_page_objs.age_ones = gfx_object_create(x, 0x32, 0x30, 0xc, 0, (const u8 *)&D_2090E0C);
            x += *(u8 *)&D_2090E0C + 1;
            owner_page_objs.age_unit = gfx_object_create(x, 0x32, 0x30, 0, 0, D_20F5384);
        }
        owner_page_objs.sign = gfx_object_create(0, 0, 0, 0, 0, 0);
        b = ((u8 *)owner_work_copy)[3] != 0 ? (int)owner_star_sign_labels[((u8 *)owner_work_copy)[3]] : (int)D_255D228;
        gfx_draw_text_line(owner_page_objs.sign, 0x19, 0x41, 0x30, b, 0, 8);
        memset(buf, 0, 0x11);
        memcpy(buf, (u8 *)owner_work_copy + 4, 0x10);
        owner_page_objs.text1 = gfx_object_create(0, 0, 0, 0, 0, 0);
        b = memcmp(buf, D_255D1D0, 14) == 0 ? (int)D_255D228 : (int)buf;
        gfx_draw_text_line(owner_page_objs.text1, 0x19, 0x51, 0x30, b, 0, 7);
        break;
    case 2:
        owner_page_objs.background = gfx_object_create(0, 0x1c, 0, 0, 0, D_20F8A54);
        owner_page_objs.arrow = gfx_object_create(0x72, 0x58, 0x20, 1, 0, D_21059A4);
        memset(buf, 0, 0x11);
        memcpy(buf, (u8 *)owner_work_copy + 0x14, 0x10);
        owner_page_objs.mood_icon = gfx_object_create(0, 0, 0, 0, 0, 0);
        b = memcmp(buf, D_255D1D0, 16) == 0 ? (int)D_255D228 : (int)buf;
        gfx_draw_text_line(owner_page_objs.mood_icon, 0x19, 0x21, 0x30, b, 0, 8);
        memset(buf, 0, 0x11);
        memcpy(buf, (u8 *)owner_work_copy + 0x24, 0x10);
        owner_page_objs.name = gfx_object_create(0, 0, 0, 0, 0, 0);
        b = memcmp(buf, D_255D1D0, 16) == 0 ? (int)D_255D228 : (int)buf;
        gfx_draw_text_line(owner_page_objs.name, 0x19, 0x31, 0x30, b, 0, 8);
        memset(buf, 0, 0x11);
        memcpy(buf, (u8 *)owner_work_copy + 0x34, 0x10);
        owner_page_objs.gender = gfx_object_create(0, 0, 0, 0, 0, 0);
        b = memcmp(buf, D_255D1D0, 16) == 0 ? (int)D_255D228 : (int)buf;
        gfx_draw_text_line(owner_page_objs.gender, 0x19, 0x41, 0x30, b, 0, 8);
        memset(buf, 0, 0x11);
        memcpy(buf, (u8 *)owner_work_copy + 0x44, 0x10);
        owner_page_objs.mood = gfx_object_create(0, 0, 0, 0, 0, 0);
        b = memcmp(buf, D_255D1D0, 14) == 0 ? (int)D_255D228 : (int)buf;
        gfx_draw_text_line(owner_page_objs.mood, 0x19, 0x51, 0x30, b, 0, 7);
        break;
    }
    func_00009A6();
}

const u8 D_255D228[] = _("????????");

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u32 owner_work_copy DSTRAM;
struct ProfilePageObjs owner_page_objs DSTRAM;
const u8 *owner_gender_labels[3] = { D_255D0EC, D_255D0F0, D_255D0FC };
const u8 *owner_mood_label_tbl[7] = {
    D_255D0EC, D_255D108, D_255D114, D_255D124, D_255D134, D_255D13C, D_255D148,
};
const u8 *owner_star_sign_labels[13] = {
    D_255D0EC, D_255D154, D_255D160, D_255D16C, D_255D178, D_255D180, D_255D188,
    D_255D194, D_255D1A0, D_255D1AC, D_255D1B4, D_255D1BC, D_255D1C8,
};
