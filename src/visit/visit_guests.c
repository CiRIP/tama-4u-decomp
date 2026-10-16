/*
 * Visitors at the house: the garden visitor who buys an item, the travelling merchant who sells
 * one, and the scheduled visitor, with the item helpers they need.
 *
 * Code 2507FBE..2508B14, .rodata from 2540F5C.
 */
#include "proto/visit/visit_guests.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/room/room_window.h"
#include "proto/shop/shop_reform.h"
#include "proto/shop/shop_browser.h"
#include "proto/anim/anim_scripts.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_message.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"

/*
 * F25 = GARDEN VISITORS (にわ): a passing character asks to buy one of our items (a
 * specific one by name or one of a category, paying five times its value when it is
 * what they wanted), a travelling merchant sells built-in goods, scheduled visitors
 * bring a riddle / letter with a reward, and the pet digs up things ("ふりふりふりっ☼
 * なにかありそう" - something's here; snowman in winter).
 *
 * Copy the five stat affinities (trait bytes 0xB8..0xBC) of item record p to out[5].
 */
void item_copy_affinities(struct TamagoFile *p, u8 *out)
{
    s8 i;

    for (i = 0; i <= 4; i++)
        out[i] = p->trait_add[i];
}

/*
 * Copy the item's name (+0x5E, up to 10 chars) of record p to out; returns its length
 * (0 when all 10 are used).
 */
s8 item_copy_name(struct TamagoFile *p, u8 *out)
{
    struct {
        u8 pad[0x5c];
        u8 name[10];
    } *s;
    s8 i;

    i = 0;
    s = (void *)((u8 *)p + 2);
    for (; i <= 9; i++) {
        if (s->name[i] == 0)
            return i;
        out[i] = s->name[i];
    }
    return 0;
}

/*
 * Index of the strongest of the 5 affinities / trait values v (random among ties).
 */
s8 item_strongest_affinity(u8 *v)
{
    s8 idx[8];
    s8 i;
    s8 n;
    s8 best;
    u8 max;

    best = 0;
    max = 0;
    for (i = 0; i <= 4; i++) {
        if (v[i] > max) {
            max = v[i];
            best = i;
        }
    }
    n = 0;
    for (i = 0; i <= 4; i++) {
        if (v[i] == max) {
            idx[n] = i;
            n++;
        }
    }
    if (n > 1)
        best = idx[(s8)util_rand_except(n, n)];
    return best;
}

/*
 * Price the visitor pays for item a: its sell value (item_price_by_index), x5 when it is the
 * requested item (b == 0: a == e) or has the requested affinity in the requested category
 * (b == 1: d == its strongest affinity and c == f).
 */
u16 visit_garden_item_price(int a, u8 b, u8 c, u8 d, int e, int f)
{
    u8 buf[8];
    u16 v;
    s8 r;

    v = item_price_by_index(a);
    item_copy_affinities(item_record_by_index(a), buf);
    r = item_strongest_affinity(buf);
    if (b == 0) {
        if (a == e)
            v *= 5;
    } else if (b == 1) {
        if (d == r && c == f)
            v *= 5;
    }
    return v;
}

/*
 * Pick a random owned item across the four inventories starting at base (item_inventories_four:
 * おどうぐ / ようふく / れいぞうこ / アクセサリー) -> *out = its item id; returns how many
 * inventories had something.
 */
int item_pick_random_owned(const struct Inventory *base, int *out)
{
    s16 idx[4];
    int v[4];
    s16 n, i;
    u16 k;
    s16 j;
    const struct Inventory *p;
    int c;

    n = 0;
    for (i = 0; i <= 3; i++) {
        v[i] = item_inv_usable_count(&item_inventories_four[i]);
        if (v[i] > 0) {
            idx[n] = i;
            n++;
        }
    }
    if (n != 0) {
        k = util_rand_except(n, n);
        j = idx[k];
        p = &base[j];
        k = util_rand_except(v[j], v[j]);
        c = item_inv_next_usable(p, p->count);
        for (i = 0; i < k; i++)
            c = item_inv_next_usable(p, c);
        *out = p->indexes[c];
    }
    return n;
}

/*
 * Delete item id (slot a of bank b, table item_bank_slots_four {slot size, sector}) from its flash
 * bank through the scratch-sector rewrite (flash_backup_sector / flash_restore_sector_except) and drop one
 * ownership count.
 */
void item_delete_from_bank(int id, u32 a, int b)
{
    struct TamagoFile *p = item_record_by_index(id);
    int m, c;

    c = item_bank_slots_four[b].sector;
    m = item_bank_slots_four[b].slot_size;
    btn_b_presses = 0;
    btn_c_presses = 0;
    snd_wait_end();
    c += (m * a) >> 16;
    flash_backup_sector(c);
    flash_restore_sector_except(c, p, m);
    item_consume(id);
}

#define X4(p) ((p)->indexes)
#define NX(i, v) item_inv_next_usable(p[i], v)
#define DRAW(m) gfx_object_create(0x72, 0x58, 0x30, m, 0, D_21059A4)
#define JUMP(i)                                      \
    {                                                \
        k = i;                                       \
        *q = -1;                                \
        *q = NX(i, p[i]->count);                 \
    }
/*
 * Browse the four inventories a[0..3] as one list with item cards (A = next item, crossing
 * into the next non-empty inventory; *q = cursor, < 0 = start at the first
 * non-empty one), B = "ほんとうに?" (ui_confirm_really) then returns the item id with
 * *o = inventory number; -1 on C / timeout / nothing owned. d is not used.
 */
int item_browse_inventories(const struct Inventory *a, u32 *q, int o)
{
    int n[4];
    const struct Inventory *p[4];
    int flag = 1;
    int k;
    s8 c;

    p[0] = a++;
    p[1] = a++;
    p[2] = a++;
    p[3] = a;
    n[0] = item_inv_usable_count(p[0]);
    n[1] = item_inv_usable_count(p[1]);
    n[2] = item_inv_usable_count(p[2]);
    n[3] = item_inv_usable_count(p[3]);
    if (n[0] == 0 && n[1] == 0 && n[2] == 0 && n[3] == 0) {
        snd_error_beep();
        btn_a_presses = flag;
        btn_c_presses = flag;
        return -1;
    }
    if (n[0] + n[1] + n[2] + n[3] == 1)
        flag = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    if ((int)*q < 0) {
        if (n[0] > 0) {
            k = 0;
            *q = NX(0, p[0]->count);
        } else if (n[1] > 0) {
            k = 1;
            *q = NX(1, p[1]->count);
        } else if (n[2] > 0) {
            k = 2;
            *q = NX(2, p[2]->count);
        } else {
            k = 3;
            *q = NX(3, p[3]->count);
        }
    }
again:
    if (k == 0)
        item_draw_card_by_index(X4(p[0])[*q], shop_get_discount_mode());
    else if (k == 1)
        item_draw_card_by_index(X4(p[1])[*q], shop_get_discount_mode());
    else if (k == 2)
        item_draw_card_by_index(X4(p[2])[*q], shop_get_discount_mode());
    else
        item_draw_card_by_index(X4(p[3])[*q], shop_get_discount_mode());
    if (k == 0) {
        if ((int)*q < NX(0, *q)) {
            if (flag)
                DRAW(0);
        } else if (n[1] > 0 || n[2] > 0) {
            if (flag)
                DRAW(0);
        } else if (flag)
            DRAW(1);
    } else if (k == 1) {
        if ((int)*q < NX(1, *q)) {
            if (flag)
                DRAW(0);
        } else if (n[2] > 0) {
            if (flag)
                DRAW(0);
        } else if (flag)
            DRAW(1);
    } else if (k == 2) {
        if ((int)*q < NX(2, *q)) {
            if (flag)
                DRAW(0);
        } else if (n[3] > 0) {
            if (flag)
                DRAW(0);
        } else if (flag)
            DRAW(1);
    } else {
        if ((int)*q < NX(3, *q)) {
            if (flag)
                DRAW(0);
        } else if (flag)
            DRAW(1);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            ui_timeout = 10;
            if (flag) {
                if (k == 0) {
                    if ((int)*q < NX(0, *q))
                        *q = NX(0, *q);
                    else if (n[1] > 0) JUMP(1)
                    else if (n[2] > 0) JUMP(2)
                    else if (n[3] > 0) JUMP(3)
                    else
                        *q = NX(0, *q);
                } else if (k == 1) {
                    if ((int)*q < NX(1, *q))
                        *q = NX(1, *q);
                    else if (n[2] > 0) JUMP(2)
                    else if (n[3] > 0) JUMP(3)
                    else if (n[0] > 0) JUMP(0)
                    else
                        *q = NX(1, *q);
                } else if (k == 2) {
                    if ((int)*q < NX(2, *q))
                        *q = NX(2, *q);
                    else if (n[3] > 0) JUMP(3)
                    else if (n[0] > 0) JUMP(0)
                    else if (n[1] > 0) JUMP(1)
                    else
                        *q = NX(2, *q);
                } else {
                    if ((int)*q < NX(3, *q))
                        *q = NX(3, *q);
                    else if (n[0] > 0) JUMP(0)
                    else if (n[1] > 0) JUMP(1)
                    else if (n[2] > 0) JUMP(2)
                    else
                        *q = NX(3, *q);
                }
            }
            goto again;
        }
        if (btn_b_presses != 0) {
            btn_b_presses = 0;
            ui_timeout = 10;
            if (k == 0)
                c = X4(p[0])[*q];
            else if (k == 1)
                c = X4(p[1])[*q];
            else if (k == 2)
                c = X4(p[2])[*q];
            else
                c = X4(p[3])[*q];
            if (ui_confirm_really() != 0) {
                *(int *)o = k;
                ui_timeout = 10;
                return c;
            }
            if (ui_timeout == 0)
                return -1;
            goto again;
        }
        if (btn_c_presses != 0)
            return -1;
        sys_idle_tick();
    } while (ui_timeout != 0);
    return -1;
}

/*
 * A garden visitor (one of three, visit_garden_visitor_ids[k]): mode 0 asks for a named owned item
 * (line visit_garden_lines[14 + 7k] with the name spliced in), mode 1 for a category with an
 * affinity ("あたまがよくなるおどうぐを さがしているんじゃが?" etc., D_25416F0), mode 2 (only
 * with the seasonal flower pet.window_flower) hands over a flower item visit_garden_flower_items[2k + m]. For
 * modes 0/1 "なにかうる?" (sell something?) opens the browser over the four inventories
 * item_inventories_four (おどうぐ tools, ようふく clothes, れいぞうこ fridge, アクセサリー accessories)
 * and the chosen item is sold at visit_garden_item_price's price.
 */
void visit_garden_visitor(void)
{
    u8 buf[44];
    int w34;
    int sel;
    int t3c;
    int w40;
    const u8 *st44;
    u8 *s;
    s8 i;
    int sum;
    int n;
    u16 k;
    u16 mode;
    int extra;
    struct TamagoFile *p;
    u16 t;
    u16 m;
    int o;

    ui_new_screen();
    sum = i = 0;
    for (; i <= 3; i++)
        sum += item_inv_usable_count(&item_inventories_four[i]);
    if (save.pet.window_flower != 0 && sum != 0)
        mode = util_rand_except(100, 100) % 3;
    else if (sum != 0)
        mode = util_rand_except(100, 100) & 1;
    else if (save.pet.window_flower != 0)
        mode = 2;
    else
        return;
    k = util_rand_except(100, 100) % 3;
    chara_select_other_by_set(visit_garden_visitor_ids[k][0]);
    o = k * 7 + 14;
    extra = 0;
    if (mode == 0) {
        s = visit_garden_lines[o];
        item_pick_random_owned(item_inventories_four, &w34);
        p = item_record_by_index(w34);
        i = item_copy_name(p, buf);
        for (; i <= 0x1f; i++, s++) {
            if (*s == 0)
                break;
            buf[i] = *s;
        }
        buf[i] = 0;
    } else if (mode == 1) {
        m = util_rand_except(100, 100) % 5;
        extra = D_25416F0[m];
    }
    visit_load_partner_pics(0);
    t3c = -1;
    if (mode <= 1) {
        sel = 1;
        switch (ui_yes_no(D_2541728, D_2541738, D_2541740, 16, &sel)) {
        case 1:
            switch (sel) {
            case 0:
                n = item_browse_inventories(item_inventories_four, &t3c, &w40);
                if (n < 0)
                    return;
                t = visit_garden_item_price(n, mode, k, extra, w34, w40);
                item_consume(n);
                visit_load_partner_pics(0);
                pet_add_points(t);
                break;
            case 1:
                return;
            }
            break;
        case 2:
        case 3:
            return;
        }
    } else {
        m = util_rand_except(100, 100) & 1;
        p = (struct TamagoFile *)item_builtin_records[visit_garden_flower_items[k * 2 + m]];
        item_load_pictures(&st44, p, 1);
        item_add_to_inventory(p);
        anim_pic_slots[4][1] = D_2258DF4;
    }
    anim_clear_pic_slots2();
    pet_add_happiness(2);
    save_game();
}

/*
 * Browse the merchant's six built-in goods a[] (indexes into item_builtin_records, record
 * item_builtin_records[a[*b]]) with their cards; B returns the record, 0 on C / timeout.
 */
int visit_merchant_browse(int *a, int *b)
{
    int show = 1;
    int step = 1;
    int t;
    int k;

again:
    t = item_builtin_records[a[*b]];
    item_draw_card((struct TamagoFile *)t, 1);
    if (show) {
        if (*b <= 4)
            gfx_object_create(0x72, 0x58, 0x30, 0, 0, D_21059A4);
        else
            gfx_object_create(0x72, 0x58, 0x30, 1, 0, D_21059A4);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses != 0) {
            if (step != 0) {
                k = *b;
                k += step;
                *b = k;
                if (k > 5)
                    *b = 0;
                goto again;
            }
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            ui_timeout = 10;
            return t;
        }
        if (btn_c_presses != 0)
            break;
        sys_idle_tick();
    } while (ui_timeout != 0);
    return 0;
}

/*
 * Position of record x among the owned items of inventory q (item_same_identity compares ids),
 * -1 if absent.
 */
int item_owned_position(const struct Inventory *q, int x)
{
    int i;

    for (i = 0; i < q->count; i++) {
        if (item_get_count(q->indexes[i]) == 0)
            continue;
        if (item_same_identity(x, item_record_by_index(q->indexes[i])))
            return i;
    }
    return -1;
}

/*
 * Same lookup, returning the item code of the owned item of list q matching record x and
 * adding one more copy of it (item_acquire: the partner's share when a partner is
 * present); 0 if absent.
 */
int item_code_and_add_copy(const struct Inventory *q, int x)
{
    int k;

    if (q != 0) {
        k = item_owned_position(q, x);
        if (k >= 0) {
            k = q->indexes[k];
            item_acquire(k);
            return k;
        }
    }
    return 0;
}

/*
 * The travelling merchant: browse visit_merchant_goods (*a = position), "おかいあげ?" (buy? yes/no),
 * affordability, store, pay (a partner gets a copy), purchase animation anim_merchant_browse.
 * 1 bought.
 */
u8 visit_merchant_purchase(int *a)
{
    int sel;
    struct TamagoFile *p;
    u8 r = 0;

    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    for (;;) {
        p = (struct TamagoFile *)visit_merchant_browse((int *)((int)visit_merchant_goods), a);
        visit_load_partner_pics(0);
        if (p == 0)
            goto ret;
        sel = 1;
        switch (ui_yes_no(D_2541748, D_2541738, D_2541740, 16, &sel)) {
        case 1:
            switch (sel) {
            case 0:
                break;
            case 1:
                continue;
            }
            break;
        case 2:
            continue;
        case 3:
            return r;
        }
        if (pet_can_pay(item_price(p))) {
            if (visit_partner.mode == 2)
                anim_pic_slots[2][1] = D_217C5A0;
            btn_a_presses = 1;
            btn_b_presses = 1;
            btn_c_presses = 1;
            snd_wait_end();
            if (item_add_to_inventory(p)) {
                pet_spend_points(item_price(p));
                if (visit_partner.mode == 2)
                    item_code_and_add_copy(&item_inventories_four[2], (int)p);
                save_game();
                r = 1;
                anim_merchant_browse();
            }
        }
        ui_timeout = 10;
    ret:
        return r;
    }
}

/*
 * The travelling merchant's visit: animation, message (text 100/0x5B), room_idle = 0 (no
 * auto-sleep), the purchase dialog visit_merchant_purchase, reaction animation.
 */
void visit_merchant(void)
{
    int v;

    room_idle = 0;
    ui_new_screen();
    room_visitor_scene = 1;
    anim_merchant_arrive();
    ui_show_message(3, 0, chara_text(0, 100, 0x5b));
    v = 0;
    if (visit_merchant_purchase(&v) == 0) {
        room_visitor_scene = 0;
    } else {
        anim_merchant_sold();
        room_visitor_scene = 0;
    }
}

/*
 * Scheduled visitor (one of five characters, visit_scheduled_charas): for babies/children (life stage
 * <= 2) just a greeting (line visit_scheduled_lines[k][3]); otherwise one of three requests (lines
 * [k][0..2], rows visit_scheduled_requests: kind 0 = a question about school club v, fulfilled
 * when the club is mastered (D_00013B0[v + 3] = pet.club_mastered[v], pet.x217..), 1 = a chat, 2 = a show-and-tell of picture x004[j]); fulfilled
 * -> thanks line [k][4] and x014[j] Gotchi points, else line [k][5] and x01a points.
 * Pays with the money animation anim_points_gain_outdoor.
 */
void visit_scheduled_visitor(void)
{
    u16 k;
    u16 j;
    int c;
    u32 v;
    const u8 *kept;

    ui_new_screen();
    k = util_rand_except(100, 100) % 5;
    chara_select_other_by_set(visit_scheduled_charas[k][0]);
    room_visitor_scene = 1;
    anim_visitor_arrive();
    if (save.pet.life_stage <= 2) {
        ui_show_message(0, 0, chara_text(0, 100, visit_scheduled_lines[k][3]));
        k = 0;
    } else {
        j = util_rand_except(100, 100) % 3;
        ui_show_message(0, 0, chara_text(0, 100, visit_scheduled_lines[k][j]));
        kept = anim_pic_slots[0][0];
        c = visit_scheduled_requests[k].kinds[j];
        v = visit_scheduled_requests[k].club;
        switch (c) {
        case 0:
            if (save.pet.club_mastered[v])
                anim_pic_slots[0][0] = (const u8 *)visit_scheduled_requests[k].pics[j];
            else
                anim_pic_slots[0][0] = (const u8 *)visit_scheduled_requests[k].unowned_pic;
            anim_visitor_ask_item();
            break;
        case 1:
            anim_visitor_chat();
            break;
        case 2:
            anim_pic_slots[0][0] = (const u8 *)visit_scheduled_requests[k].pics[j];
            anim_visitor_show();
            break;
        }
        if (save.pet.club_mastered[v]) {
            anim_pic_slots[0][0] = (const u8 *)(c == 0 ? 0x217c5a0 : 0x21918a8);
            anim_visitor_pleased();
            ui_show_message(0, 0, chara_text(0, 100, visit_scheduled_lines[k][4]));
            k = visit_scheduled_requests[k].reward_points[j];
        } else {
            anim_pic_slots[0][0] = D_21918A8;
            anim_visitor_let_down();
            ui_show_message(1, 0, chara_text(0, 100, visit_scheduled_lines[k][5]));
            k = visit_scheduled_requests[k].consolation;
        }
        anim_points_gain_outdoor(k);
        anim_pic_slots[0][0] = kept;
    }
    pet_add_happiness(1);
    pet_add_points(k);
    save_game();
    anim_happy_outdoor();
    room_visitor_scene = 0;
}

const s8 D_2540F60[] = {
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15
};
const s8 D_2540F74[] = {
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24
};
const s8 D_2540F84[] = {
    0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43,
    0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d,
    0x4e, 0x4f, 0x50
};
const s8 D_2540F9C[] = {
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
    0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38
};
const u8 D_2540FB0[] = _("おどうぐ");
const u8 D_2540FBC[] = _("ようふく");
const u8 D_2540FC8[] = _("れいぞうこ");
const u8 D_2540FD4[] = _("アクセサリー");
const struct Inventory item_inventories_four[] = {
    { D_2540FB0, D_2540F84, 23 },
    { D_2540FBC, D_2540F74, 15 },
    { D_2540FC8, D_2540F60, 20 },
    { D_2540FD4, D_2540F9C, 20 }
};
const struct BankLayout item_bank_slots_four[] = {
    { 0x2000, 0x70, 0x19 },
    { 0x4000, 0x66, 0x0a },
    { 0x0800, 0x65, 0x16 },
    { 0x2000, 0x6b, 0x14 }
};
const u8 D_2541024[] = _("ふりふりふりっ☼\n(なにかありそう☼)");
const u8 D_254104C[] = _("ふりふりふりっ☼(ゆきだ\nるまつくろうよ☼)");
const u8 D_254107C[] = _("ここになにか\nあるみたいキャン☼");
const u8 D_25410A0[] = _("ゆきだるまつくるといいこ\nとあるかもキャン☼");
const u8 D_25410D0[] = _("スリンスリン～@(ここに\nなにかありそう☼)");
const u8 D_2541100[] = _("スリンスリン～@(ゆきだ\nるまつくろうよ～☼)");
const u8 D_2541130[] = _("あんあーん☼(ここに\nなにかあるかも☼)");
const u8 D_254115C[] = _("あんあーん☼(ゆきだるま\nつくりたい～☼)");
const u8 D_2541188[] = _("チャピチャピ～☼\n(なにかあるかも～☼)");
const u8 D_25411B4[] = _("チャピチャピ～☼(ゆきだ\nるまつくってみて～☼)");
const u8 D_25411E8[] = _("れいぞうこにいれておいて\nね★おいしくめしあがれ♪");
const u8 D_254121C[] = _("オマケするからふたりで\nなかよくたべてね♥");
const u8 D_2541248[] = _("これを♪カフェ♪にもって\nきて!オマケするわよ★");
const u8 D_254127C[] = _("レストランでやすくなる\nクーポンをあげるわ♪");
const u8 D_25412AC[] = _("をさがしているんじゃが?");
const u8 D_25412C8[] = _("あたまがよくなるおどうぐ\nをさがしているんじゃが?");
const u8 D_25412FC[] = _("おしゃれなおどうぐを\nさがしているんじゃが?");
const u8 D_254132C[] = _("ステキなおどうぐ\nをさがしているんじゃが?");
const u8 D_2541358[] = _("ちからがつくおどうぐを\nさがしているんじゃが?");
const u8 D_2541388[] = _("グルメなおどうぐ\nをさがしているんじゃが?");
const u8 D_25413B4[] = _("このにわはせかいいち\nのにわだぞい!");
const u8 D_25413DC[] = _("がほしいんだけどー?");
const u8 D_25413F4[] = _("かしこくみえるようふくか\nアクセのオススメあるー?");
const u8 D_2541428[] = _("おしゃれなようふくか\nアクセのオススメあるー?");
const u8 D_2541458[] = _("カリスマせいバツグンの\nようふくかアクセあるー?");
const u8 D_254148C[] = _("ちからみなぎるようふくか\nアクセのオススメあるー?");
const u8 D_25414C0[] = _("グルメっぽいようふくか\nアクセのオススメあるー?");
const u8 D_25414F4[] = _("ガーデニング\nちょースゴーい★★★");
const u8 D_2541518[] = _("があるってきいたんだけど");
const u8 D_2541534[] = _("あたまがよくなるオススメ\nりょうりはあるかい?");
const u8 D_2541564[] = _("おしゃれなオススメ\nりょうりはあるかい?");
const u8 D_2541590[] = _("みんなににんきのオススメ\nりょうりはあるかい?");
const u8 D_25415C0[] = _("ちからがでるオススメ\nりょうりはあるかい?");
const u8 D_25415EC[] = _("おいしさバッチリなオスス\nメりょうりはあるかい?");
const u8 D_2541620[] = _("こ､このにわは…!\nなんと…すばらしい…");
const u8 *const visit_garden_lines[] = {
    D_2541024, D_254104C, D_254107C, D_25410A0,
    D_25410D0, D_2541100, D_2541130, D_254115C,
    D_2541188, D_25411B4, D_25411E8, D_254121C,
    D_2541248, D_254127C, D_25412AC, D_25412C8,
    D_25412FC, D_254132C, D_2541358, D_2541388,
    D_25413B4, D_25413DC, D_25413F4, D_2541428,
    D_2541458, D_254148C, D_25414C0, D_25414F4,
    D_2541518, D_2541534, D_2541564, D_2541590,
    D_25415C0, D_25415EC, D_2541620
};
const u32 visit_garden_flower_items[6] = { 0 };
const u8 D_25416F0[] = { 0, 1, 2, 4, 3 };
const int visit_merchant_goods[12] = { 0x59, 0x53, 0x56, 0x66, 0x74, 0x72 };
const u8 D_2541728[] = _("なにかうる?");
const u8 D_2541738[] = _("はい");
const u8 D_2541740[] = _("いいえ");
const u8 D_2541748[] = _("おかいあげ?");
const u16 visit_scheduled_charas[][2] = {
    { 0x2e, 0 },
    { 0x2f, 0 },
    { 0x30, 0 },
    { 0x31, 0 },
    { 0x32, 0 }
};
const u8 visit_scheduled_lines[][6] = {
    { 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61 },
    { 0x62, 0x63, 0x64, 0x65, 0x66, 0x67 },
    { 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d },
    { 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73 },
    { 0x74, 0x75, 0x76, 0x77, 0x78, 0x79 }
};
extern u8 D_2190C44[];
extern u8 D_2190E3C[];
extern u8 D_21910E0[];
extern u8 D_21912F8[];
extern u8 D_2191628[];
extern u8 D_2191760[];
extern u8 D_21A5E90[];
const struct VisitorRequest visit_scheduled_requests[] = {
    { { 0, 0, 0, 0 }, { D_21903E4, D_21908BC, D_21906C4 }, D_21A5E90, { 2000, 1000, 500 }, 50, 0 },
    { { 0, 0, 0, 0 }, { D_2190AC4, D_2190C44, D_2190E3C }, D_2161E4C, { 2000, 1000, 500 }, 50, 1 },
    { { 2, 1, 2, 0 }, { D_21A71AC, 0, D_21A5538 }, D_21A5E90, { 2000, 1000, 500 }, 50, 2 },
    { { 0, 1, 0, 0 }, { D_21910E0, 0, D_21912F8 }, D_21A5E90, { 2000, 1000, 500 }, 50, 3 },
    { { 0, 0, 0, 0 }, { D_21914E8, D_2191628, D_2191760 }, D_2161BE0, { 2000, 1000, 500 }, 50, 4 }
};

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u16 visit_garden_visitor_ids[3][2] = { { 0x33 }, { 0x5A }, { 0x4D } };
