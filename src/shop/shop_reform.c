/*
 * Gotchi Reform: browsing and paying for room designs, the room-design tables used by every
 * scene, and the generic do / yes-no confirmation screens it uses.
 *
 * Code 241489E..24152BE, .rodata from 255BD48.
 */
#include "proto/shop/shop_reform.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/shop/shop_browser.h"
#include "proto/anim/anim_scripts.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"

/*
 * F32 = small confirmation screens used everywhere, and the GOTCHI REFORM (room
 * redecorating) browsers. Room designs are items in the background banks (room table
 * room_tbl[room] = {label, bank, filter}; the current design of each room is
 * pet.weather_now[2 + room]).
 *
 * "title + する/しない" question screen: question p, answers D_255BEEC/D_255BEF4 (する /
 * しない), hand cursor on answer b (ui_dialog_cursor_obj = cursor object). ui_two_choice_run waits for
 * the answer.
 */
void ui_question_do_screen(int p, u8 b)
{
    u8 a;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_centered(a, 0x20, 0x10, (u8 *)p, 0, 9);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x40, 0x10, (int)D_255BEEC, 0, 8);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x50, 0x10, (int)D_255BEF4, 0, 8);
    ui_dialog_cursor_obj = gfx_object_create(6, b * 16 + 0x42, 0x20, 0, 0, D_2104714);
    func_00009A6();
}

/*
 * "title + はい/いいえ" question screen: question p, answers D_255BEFC/D_255BF04 (はい /
 * いいえ), cursor on b.
 */
void ui_question_yes_screen(int p, u8 b)
{
    u8 a;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_centered(a, 0x20, 0x10, (u8 *)p, 0, 9);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x40, 0x10, (int)D_255BEFC, 0, 8);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x50, 0x10, (int)D_255BF04, 0, 8);
    ui_dialog_cursor_obj = gfx_object_create(6, b * 16 + 0x42, 0x20, 0, 0, D_2104714);
    func_00009A6();
}

/*
 * Set the dialog cursor object ui_dialog_cursor_obj.
 */
void ui_set_dialog_cursor(int a)
{
    ui_dialog_cursor_obj = a;
}

/*
 * Run a two-choice dialog: A toggles, B confirms: 1 = first answer, 0 = second answer / C
 * / timeout.
 */
u8 ui_two_choice_run(u8 a)
{
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses != 0) {
            a = a + 1;
            if (a >= 2)
                a = 0;
            gfx_object_move_dirty(ui_dialog_cursor_obj, 6, a * 16 + 0x42);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            ui_timeout = 10;
            if (a == 0)
                return 1;
            goto fail;
        }
        if (btn_c_presses != 0) {
            ui_timeout = 10;
            goto fail;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
fail:
    return 0;
}

/*
 * Yes/no question a with the cursor on b, drawn and answered (1 = yes): the "ほんとうに?"
 * box of all the delete flows.
 */
u8 ui_confirm_yes_no(const u8 *a, u8 b)
{
    ui_question_yes_screen((int)a, b);
    return ui_two_choice_run(b);
}

/*
 * Yes/no dialog screen with a two-line question (p, q).
 */
void ui_question_two_line_screen(int p, int q, u8 b)
{
    u8 a;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_centered(a, 0x20, 0x10, (u8 *)p, 0, 9);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_centered(a, 0x30, 0x10, (u8 *)q, 0, 9);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x40, 0x10, (int)D_255BEFC, 0, 8);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x50, 0x10, (int)D_255BF04, 0, 8);
    ui_dialog_cursor_obj = gfx_object_create(6, b * 16 + 0x42, 0x20, 0, 0, D_2104714);
    func_00009A6();
}

/*
 * Yes/no question with two lines (ui_question_two_line_screen); 1 = yes.
 */
int ui_yes_no_two_lines(int a, int b, u8 c)
{
    ui_question_two_line_screen(a, b, c);
    return ui_two_choice_run(c);
}

/*
 * Can the pet pay n Gotchi points? 1 if so; if not, show the "not enough points" picture
 * 0x210D65C with the error sound (12) for 3 s and return 0.
 */
u8 pet_can_pay(u16 n)
{
    if (save.pet.gotchi_points >= n)
        return 1;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_210D65C);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 3;
    snd_play(12);
    do {
        if ((u8)(btn_a_presses | btn_b_presses | btn_c_presses))
            break;
        sys_idle_tick();
    } while (ui_timeout != 0);
    ui_timeout = 10;
    return 0;
}

/*
 * Ask "リフォームする?" (D_255BF0C) and pay the price id if possible (pet_can_pay).
 * Returns 1 when paid.
 */
int shop_reform_pay(u16 id)
{
    if (ui_confirm_yes_no(D_255BF0C, 1)) {
        if (pet_can_pay(id)) {
            pet_spend_points(id);
            return 1;
        }
    }
    return 0;
}

/*
 * Next available design after b for room a (room_tbl[a]: x004 = item bank, x008 = also
 * offer the built-in designs): the next index that exists (item_bank_has), skipping the
 * design in use (pet.weather_now[a + 2]); for the living room (a == 0) it is skipped only when
 * it is the one stored in sector 0x76.
 */
int room_next_design(int a, int b)
{
    int k = room_tbl[a].bank;
    int f = room_tbl[a].flags;

    for (;;) {
        b++;
        if (f) {
            if (b >= item_bank_tbl[k].slot_count + item_bank_tbl[k].builtin_count)
                b = 0;
        } else {
            if (b >= item_bank_tbl[k].slot_count)
                b = 0;
        }
        if (b == save.pet.room_designs[a]) {
            if (a != 0)
                continue;
            if (item_same_identity((int)flash_sector_addr(FLASH_SEC_DEFAULT_BG), item_bank_record(k, b)) != 0)
                continue;
        }
        if (item_bank_has(k, b, f))
            return b;
    }
}

/*
 * Design browser for room a: one card per design with its price (item_draw_card), a badge
 * on the designs stored in the bank (downloaded ones, index below the bank count), sale
 * marks by the discount mode (shop_get_discount_mode). A = next design (room_next_design). B on a stored
 * design: the 3-entry menu "リフォームする?" (do it / no / delete; delete = confirmation
 * ui_confirm_really, then the scratch-sector rewrite flash_backup_sector / flash_restore_sector_except); on a
 * built-in design: "リフォームする? はい/いいえ"; then the price check (pet_can_pay)
 * and payment (pet_spend_points). Returns the chosen design (+100 for the variants of bank
 * 6), -1 cancelled (C) or timeout. The caller stores the item (item_add_to_inventory).
 */
int shop_reform_browser(int a)
{
    struct TamagoFile *it;
    u8 n;
    u8 first;
    char res = -1;
    u32 m;
    u32 w;
    int b;
    int k = room_tbl[a].bank;
    u8 cur;
    u8 z = 0;
    u32 sel;

    btn_b_presses = z;
    btn_c_presses = z;
    cur = room_next_design(a, item_bank_tbl[k].slot_count + item_bank_tbl[k].builtin_count);
    if (k == 6) {
        n = z;
        first = cur;
    } else {
        n = z;
        first = cur;
    }
again:
    if (z <= n) {
        it = item_bank_record(k, cur);
        item_draw_card(it, 1);
        if (cur < item_bank_tbl[k].slot_count)
            gfx_object_create(0x69, 0x21, 0x60, 0, 0, 0x214dfe4);
        switch (shop_get_discount_mode()) {
        case 1:
            gfx_object_create(0x4f, 0x32, 0x70, 0, 0, 0x219a690);
            break;
        case 2:
            gfx_object_create(0x4f, 0x32, 0x70, 0, 0, 0x2194f18);
            break;
        }
        if (cur < room_next_design(a, cur))
            gfx_object_create(0x72, 0x58, 0x80, 0, 0, 0x21059a4);
        else
            gfx_object_create(0x72, 0x58, 0x80, 1, 0, 0x21059a4);
        func_00009A6();
        res = -1;
    }
    ui_timeout = 10;
    btn_a_presses = 0;
    for (;;) {
        if (k == 6) {
            if (z > n) {
                if (btn_a_presses) {
                    n = n + 1;
                    goto again;
                }
            } else if (btn_a_presses) {
                cur = room_next_design(a, cur);
                if (cur != first)
                    goto again;
                n = 0;
                goto again;
            }
        } else if (btn_a_presses) {
            cur = room_next_design(a, cur);
            goto again;
        }
        if (btn_b_presses) {
            if (cur < item_bank_tbl[k].slot_count && res < 0) {
                sel = 1;
                switch ((u32)ui_use_no_delete((int)D_255BF0C, 0, &sel)) {
                case 1:
                    switch (sel) {
                    case 0:
                        m = item_price(it);
                        if (pet_can_pay(m)) {
                            pet_spend_points(m);
                            return cur;
                        }
                        break;
                    case 1:
                        break;
                    case 2:
                        if (ui_confirm_really()) {
                            int k2 = room_tbl[a].bank;
                            snd_wait_end();
                            w = item_bank_tbl[k2].slot_size;
                            b = item_bank_tbl[k2].first_sector + (w * cur >> 16);
                            flash_backup_sector(b);
                            flash_restore_sector_except(b, it, w);
                            snd_wait_end();
                            save_game();
                            cur = room_next_design(a, cur);
                            goto clear;
                        }
                        if (*(volatile u8 *)&ui_timeout == 0)
                            return -1;
                        btn_b_presses = 0;
                        btn_c_presses = 0;
                        goto again;
                    }
                    break;
                case 2:
                    goto again;
                case 3:
                    return -1;
                }
            } else if (ui_confirm_yes_no(D_255BF0C, 1)) {
                u32 m2 = item_price(it);
                if (pet_can_pay(m2)) {
                    int v;
                    pet_spend_points(m2);
                    v = res + 100;
                    if (res >= 0)
                        return v;
                    return cur;
                }
            } else
                res--;
            if (ui_timeout == 0)
                return -1;
        clear:
            btn_b_presses = 0;
            btn_c_presses = 0;
            goto again;
        }
        if (btn_c_presses)
            return -1;
        sys_idle_tick();
        if (ui_timeout == 0)
            return -1;
    }
}

/*
 * Redecorate the living room (room 0): sale mode (discount mode 2, shop_set_discount_mode) when
 * flag 0x17 is set; browse (shop_reform_browser), then the makeover animation (anim_reform_preview old
 * design room_living_designs[pet.room_designs[0]], anim_reform_done new one), store the design item
 * (item_add_to_inventory), remember it in pet.room_designs[0], +5 happiness (pet_add_happiness), apply the item's
 * attribute bytes (item_apply_attributes), save.
 */
int shop_reform_living(void)
{
    int i;
    int n;
    u8 t;
    u8 *q;
    struct TamagoFile *s;

    ui_new_screen();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    i = 0;
    if (pet_get_unlock_flag(0x17) != 0)
        shop_set_discount_mode(2);
    ui_timeout = 10;
    q = (u8 *)&save.pet + i;
    t = q[0x66];
    n = shop_reform_browser(i);
    shop_set_discount_mode(0);
    if (n < 0)
        return 0;
    if (q[0xbc] > 3) {
        q[0xbc] = 0;
        anim_reform_living();
    }
    s = item_bank_record(room_tbl[i].bank, n);
    anim_reform_preview((const u8 *)room_living_designs[t]);
    anim_reform_done((int)s + 0x204);
    snd_wait_end();
    item_add_to_inventory(s);
    save.pet.room_designs[i] = n;
    pet_add_happiness(5);
    item_apply_attributes(s);
    snd_wait_end();
    save_game();
    return 1;
}

/*
 * a-th living-room design: the downloaded ones (item id > 100) in the first 10 slots of
 * the bank first, then the built-in ones D_255C30C[] from the ROM item table item_builtin_records.
 */
struct TamagoFile *room_living_design_at(int a)
{
    u32 k = room_tbl[0].bank;
    int i;
    int j;
    struct TamagoFile *p;

    for (i = 0; i <= 9; i++) {
        p = item_bank_record(k, i);
        if (p == (struct TamagoFile *)-1)
            break;
        if (item_get_id(p) > 100) {
            if (a == 0)
                return p;
            a--;
        }
    }
    for (j = 0; j < item_bank_tbl[k].builtin_count; j++) {
        if (a == 0)
            return item_builtin_records[D_255C30C[j]];
        a--;
    }
    return (struct TamagoFile *)-1;
}

/*
 * Number of living-room designs (as listed by room_living_design_at).
 */
int room_living_design_count(void)
{
    u8 unused[12];
    struct TamagoFile *p;
    int n = 0;
    int k = room_tbl[0].bank;
    int i;

    for (i = 0; i <= 9; i++) {
        p = item_bank_record(k, i);
        if (p == (struct TamagoFile *)-1)
            break;
        if (item_get_id(p) > 100)
            n++;
    }
    return n + item_bank_tbl[k].builtin_count;
}

/*
 * The design browser of shop_reform_browser for the living room (room 0), started a entries in.
 */
int shop_reform_browser_living(u8 a)
{
    int s = 0;
    int k = room_tbl[0].bank;
    int cur;
    int i;
    struct TamagoFile *it;
    u32 sel;
    int b;
    u32 w;
    u32 n;

    btn_b_presses = s;
    btn_c_presses = s;
    cur = room_next_design(s, item_bank_tbl[k].slot_count + item_bank_tbl[k].builtin_count);
    for (i = 0; i < a - 1; i++)
        cur = room_next_design(s, cur);
again:
    it = item_bank_record(k, cur);
    item_draw_card(it, 1);
    if (cur < item_bank_tbl[k].slot_count)
        gfx_object_create(0x69, 0x21, 0x60, 0, 0, 0x214dfe4);
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    for (;;) {
        if (btn_a_presses)
            btn_a_presses = 0;
        if (btn_b_presses) {
            if (cur < item_bank_tbl[k].slot_count) {
                sel = 1;
                switch ((u32)ui_use_no_delete((int)D_255BF0C, 0, &sel)) {
                case 1:
                    switch (sel) {
                    case 0:
                        goto buy;
                    case 1:
                        break;
                    case 2:
                        if (ui_confirm_really()) {
                            int k2 = room_tbl[s].bank;
                            snd_wait_end();
                            b = item_bank_tbl[k2].first_sector;
                            w = item_bank_tbl[k2].slot_size;
                            b += w * cur >> 16;
                            flash_backup_sector(b);
                            flash_restore_sector_except(b, it, w);
                            cur = room_next_design(s, cur);
                            goto clear;
                        }
                        if (ui_timeout == 0)
                            goto fail;
                        btn_b_presses = 0;
                        btn_c_presses = 0;
                        goto again;
                    }
                    break;
                case 2:
                    goto again;
                case 3:
                    return -1;
                }
            } else if (ui_confirm_yes_no(D_255BF0C, 1)) {
            buy:
                n = item_price(it);
                if (pet_can_pay(n)) {
                    pet_spend_points(n);
                    return cur;
                }
            }
            if (ui_timeout == 0)
                return -1;
        clear:
            btn_b_presses = 0;
            btn_c_presses = s;
            goto again;
        }
        if (btn_c_presses) {
        fail:
            return -1;
        }
        sys_idle_tick();
        if (ui_timeout == 0)
            return -1;
    }
}

const u8 D_255BD4C[] = _("リビング");
const u8 D_255BD58[] = _("キッチン");
const u8 D_255BD64[] = _("トイレ");
const u8 D_255BD6C[] = _("おふろ");
const u8 D_255BD74[] = _("しんしつ");
/*
 * room_tbl (in this data block): the rooms (12-byte entries: label, x004 item bank, x008
 * "also offers the built-in designs" flag) used by room_next_design / shop_reform_browser.
 */
const struct BankListEntry room_tbl[5] = {
    { D_255BD4C, 6, 0x1f },
    { D_255BD58, 11, 0x1f },
    { D_255BD64, 12, 0x1f },
    { D_255BD6C, 13, 0x1f },
    { D_255BD74, 14, 0x1f },
};
/* the room designs: pictures by pet.room_designs[0] .. x06A (FLASH_DEFAULT_BG + 0x204: in the save area) */
extern u8 D_20D5BE0[];
extern u8 D_20CEEF0[];
extern u8 D_20D6E08[];
extern u8 D_20D8030[];
extern u8 D_20D9258[];
extern u8 D_20D0118[];
extern u8 D_20D1340[];
extern u8 D_20D2568[];
extern u8 D_20D3790[];
const u32 room_living_designs[25] = {
    (u32)(FLASH_DEFAULT_BG + 0x204), (u32)(FLASH_DEFAULT_BG + 0x204), (u32)(FLASH_DEFAULT_BG + 0x204), (u32)(FLASH_DEFAULT_BG + 0x204), (u32)(FLASH_DEFAULT_BG + 0x204),
    (u32)(FLASH_DEFAULT_BG + 0x204), (u32)(FLASH_DEFAULT_BG + 0x204), (u32)(FLASH_DEFAULT_BG + 0x204), (u32)(FLASH_DEFAULT_BG + 0x204), (u32)(FLASH_DEFAULT_BG + 0x204),
    (u32)D_20C8204, (u32)D_20D5BE0, (u32)D_20CEEF0, (u32)D_20D6E08, (u32)D_20D8030,
    (u32)D_20D9258, (u32)D_20D0118, (u32)D_20D1340, (u32)D_20D2568, (u32)D_20D3790,
    (u32)D_20C8204, (u32)D_20C8204, (u32)D_20C8204, (u32)D_20C8204, (u32)D_20C8204,
};
const u32 room_kitchen_designs[8] = {
    (u32)D_20C942C, (u32)D_20C942C, (u32)D_20C942C, (u32)D_20C942C,
    (u32)D_20A97EC, (u32)D_20AAA14, (u32)D_20A97EC, (u32)D_20AAA14,
};
const u32 room_toilet_designs[4] = { (u32)D_20CA650, (u32)D_20CA650, (u32)D_20CA650, (u32)D_20CA650 };
const u32 room_bath_designs[4] = { (u32)D_20CB878, (u32)D_20CB878, (u32)D_20CB878, (u32)D_20CB878 };
const u32 room_bedroom_designs[4] = { (u32)D_20CCAA0, (u32)D_20CCAA0, (u32)D_20CCAA0, (u32)D_20CCAA0 };
const u32 room_kitchen_bg_pics[8] = { (u32)D_217C6D8, (u32)D_218A668, (u32)D_218A668, 0, 0, 0, 0, 0 };
const u32 room_kitchen_fg_pics[8] = {
    (u32)D_218A7E0, (u32)D_218A7E0, (u32)D_218A7E0, (u32)D_218A7E0,
    (u32)D_217C9CC, (u32)D_217C9CC, (u32)D_217C9CC, (u32)D_217C9CC,
};
const u32 room_bath_extra_pics[4] = { 0, 0, 0, 0 };
const u32 room_bed_pics[4] = { (u32)D_217A848, (u32)D_217A848, (u32)D_217A848, (u32)D_217A848 };
/* unreferenced: the room design bytes and (probably) the number of designs per room */
u8 *const D_255BED0[5] = {
    &save.pet.room_designs[0], &save.pet.room_designs[1], &save.pet.room_designs[2], &save.pet.room_designs[3], &save.pet.room_designs[4],
};
const u8 D_255BEE4[5] = { 25, 4, 4, 4, 4 };
const u8 D_255BEEC[] = _("する");
const u8 D_255BEF4[] = _("しない");
const u8 D_255BEFC[] = _("はい");
const u8 D_255BF04[] = _("いいえ");
const u8 D_255BF0C[] = _("リフォームする?");

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 ui_dialog_cursor_obj DSTRAM;
