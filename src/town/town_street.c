/*
 * The street and the farm: street encounters and friendships, farm planting and harvest, outing
 * timing checks, and the fountain with its honesty test.
 *
 * Code 2508FEE..2509EA0, .rodata from 25426D4.
 */
#include "proto/town/town_street.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/chara/chara_draw.h"
#include "proto/town/town_places.h"
#include "proto/shop/shop_reform.h"
#include "proto/anim/anim_scripts.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_message.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/comm/comm_outcome.h"
#include "proto/comm/comm_checks.h"
#include "flash.h"
#include "libc.h"

/*
 * Town entry 4 "たまのうえん": "???" locked (2) below 20 communications (pet.comm_count),
 * open (0) from 20.
 */
int town_farm_state_cb(void)
{
    int r;

    if (save.pet.comm_count > 0x13)
        r = 0;
    else
        r = 2;
    return r;
}

/*
 * Town entry 5 "であいストリート": hidden (-1) below 20 communications (pet.comm_count),
 * "???" locked (2) from 20, open (0) from 50.
 */
int town_street_state_cb(void)
{
    if (save.pet.comm_count <= 19)
        return -1;
    if (save.pet.comm_count > 49)
        return 0;
    return 2;
}

int g_town_travel_scene(void *, int, int) __asm__("town_travel_scene");
/*
 * Enter a place without picture (kind 5).
 */
int town_enter_farm(void)
{
    int r = g_town_travel_scene(0, 0, 5);

    if (r)
        r = 1;
    return r;
}

/*
 * Enter a place without picture (kind 5).
 */
int town_enter_street(void)
{
    int r = g_town_travel_scene(0, 0, 5);

    if (r)
        r = 1;
    return r;
}

/*
 * F29 = であいストリート (encounter street), たまのうえん (the farm), the game centre state
 * and the fountain.
 *
 * town_pick_street_chara: pick the street character met (sprites 96..101) by chance (thirds of
 * util_rand_except(100, 100)) and by our gender (pet_get_gender == 0 -> 99..101, else 96..98),
 * select it as the visitor (chara_select_other_by_set) and return its index 0..5.
 */
u8 town_pick_street_chara(void)
{
    u8 r;
    u16 id;
    int v = util_rand_except(100, 100);

    if (v <= 32) {
        if (pet_get_gender() == 0) {
            id = 99;
            r = 3;
        } else {
            id = 96;
            r = 0;
        }
    } else if (v <= 65) {
        if (pet_get_gender() == 0) {
            id = 100;
            r = 4;
        } else {
            id = 97;
            r = 1;
        }
    } else {
        if (pet_get_gender() == 0) {
            r = 5;
            id = 101;
        } else {
            r = 2;
            id = 98;
        }
    }
    chara_select_other_by_set(id);
    return r;
}


/*
 * Select street character k (0..5 -> character ids 0x60..0x65) as the visitor/partner
 * sprite set (chara_select_other_by_set).
 */
void town_select_street_chara(u8 k)
{
    u16 n;

    switch (k) {
    case 0:
        n = 0x60;
        break;
    case 1:
        n = 0x61;
        break;
    case 2:
        n = 0x62;
        break;
    case 3:
        n = 0x63;
        break;
    case 4:
        n = 0x64;
        break;
    case 5:
    default:
        n = 0x65;
        break;
    }
    chara_select_other_by_set(n);
}

/*
 * Bump the friendship counter pet.street_friendship[k] of street character k (cap 11).
 */
void town_street_friendship_add(u8 k)
{
    save.pet.street_friendship[k]++;
    if (save.pet.street_friendship[k] > 11)
        save.pet.street_friendship[k] = 11;
}

/*
 * Clear the six street friendship counters pet.street_friendship.
 */
void town_street_friendship_clear(void)
{
    int i;

    for (i = 0; i <= 5; i++)
        save.pet.street_friendship[i] = 0;
}

const u8 D_25426F0[] = _("けっこんする?");
const u8 D_2542700[] = _("する");
const u8 D_2542708[] = _("しない");

/*
 * ENCOUNTER STREET visit: meet a street character (town_pick_street_chara); babies/children just play
 * (anim_street_play); adults progress the friendship by its counter: 0 first meeting, 2, 5,
 * 8 = dates with its lines (text group 100, five lines per character from 0x3E + 5k),
 * 11 = the proposal "けっこんする? する/しない" (marry?) -> wedding (pet_life_story), new
 * generation (pet_pick_variant_flag / pet_start_next_generation / anim_new_egg); other values = a random chat.
 * Each meeting counts (town_street_friendship_add) and saves.
 */
int town_street_visit(void)
{
    u8 i;
    u8 v;
    int sel;
    int t;
    int z = 0;

    if ((u8)(btn_a_presses | btn_b_presses) != z) {
        btn_a_presses = z;
        btn_b_presses = z;
    }
    if (btn_c_presses != 0) {
        ui_timeout = 10;
        return 0;
    }
    i = town_pick_street_chara();
    v = save.pet.street_friendship[i];
    if (save.pet.life_stage <= 2) {
        anim_street_play();
    } else {
        switch (v) {
        case 0:
            anim_street_meet();
            ui_show_message(0x20, 0, chara_text(0, 100, i * 5 - 0x3e));
            anim_street_happy();
            break;
        case 2:
            anim_street_meet();
            ui_show_message(0x20, 0, chara_text(0, 100, i * 5 - 0x3d));
            anim_street_wedding();
            break;
        case 5:
            anim_street_meet();
            ui_show_message(0x20, 0, chara_text(0, 100, i * 5 - 0x3c));
            anim_street_wedding_party();
            break;
        case 8:
            anim_street_meet();
            ui_show_message(0x20, 0, chara_text(0, 100, i * 5 - 0x3b));
            anim_street_i_love_you();
            break;
        case 11:
            anim_street_meet();
            ui_show_message(0x20, 0, chara_text(0, 100, i * 5 - 0x3a));
            sel = 1;
            switch (ui_yes_no(D_25426F0, D_2542700, D_2542708, 16, &sel)) {
            case 1:
                if (sel != 0)
                    goto done;
                anim_wedding();
                pet_life_story();
                town_select_street_chara(i);
                anim_wedding_end();
                pet_pick_variant_flag();
                pet_start_next_generation();
                rtc_seconds = 0;
                ui_main_icon = 0;
                anim_new_egg();
                rtc_seconds = 0;
                return 1;
            case 2:
                return 1;
            case 3:
                ui_timeout = 0;
                return 0;
            }
            goto tail;
        case -1:
        default:
            t = util_rand_except(100, 100);
            if (t <= 0x20)
                anim_street_play();
            else if (t <= 0x41)
                anim_street_friendship();
            else
                anim_street_love();
            break;
        }
        anim_street_leave();
        town_street_friendship_add(i);
    }
tail:
    anim_happy();
done:
    pet_add_happiness(1);
    save_after_sound();
    return 1;
}

/*
 * FARM (たまのうえん): pet.farm_stage = growth stage 0..4, pet.farm_tended = tended today,
 * pet.farm_neglect_days = days without care (4 = withered).
 *
 * town_farm_midnight: the midnight tick: clear "tended today"; the days-without-care count is
 * reset when the field was tended or is empty, else incremented (max 4).
 */
void town_farm_midnight(void)
{
    if (save.pet.farm_tended != 0)
        save.pet.farm_neglect_days = 0;
    else if (save.pet.farm_stage == 0)
        save.pet.farm_neglect_days = 0;
    else {
        save.pet.farm_neglect_days++;
        if (save.pet.farm_neglect_days > 4)
            save.pet.farm_neglect_days = 4;
    }
    save.pet.farm_tended = 0;
}

/*
 * What happens on a farm visit: 0 already tended today, 1 planting, 2 growing (one stage
 * per day), 3 harvest (stage 4 -> back to 0), 4 withered (4 days without care).
 */
u8 town_farm_event(void)
{
    u8 r;

    if (save.pet.farm_tended != 0)
        r = 0;
    else if (save.pet.farm_neglect_days > 3)
        r = 4;
    else if (save.pet.farm_stage <= 3) {
        save.pet.farm_tended = 1;
        save.pet.farm_stage++;
        if (save.pet.farm_stage == 1)
            r = 1;
        else
            r = 2;
    } else {
        save.pet.farm_stage = 0;
        save.pet.farm_tended = 0;
        r = 3;
    }
    return r;
}

/*
 * The plant picture for farm stage k (5 = ripe, 6 / 7 the two crops, 8 withered).
 */
u8 *town_farm_plant_pic(u8 k)
{
    const u8 *p = D_2137380;

    switch (k) {
    case 0:
        p = D_2258DF4;
        break;
    case 1:
        p = D_2135E9C;
        break;
    case 2:
        p = D_213618C;
        break;
    case 3:
        p = D_2136480;
        break;
    case 4:
        p = D_2136778;
        break;
    case 5:
        p = D_2136A74;
        break;
    case 6:
        p = D_2136D78;
        break;
    case 7:
        p = D_213707C;
        break;
    case 8:
        p = D_2137380;
        break;
    }
    return p;
}

const u8 D_2542768[3] = { 0x5b, 0x5c, 0x5d };
const u8 D_254276C[5] = { 0x75, 0x76, 0x77, 0x78, 0x79 };

/*
 * Farm visit (town_farm_event): already tended -> a farmer (sprites 0x4A / 0x66 / 0x67)
 * chats (one of nine lines); planting / growing / harvest animations; the harvest gives a
 * random crop (util_rand_except(0x50) < 30: one of three D_2542768 items, else one of five
 * D_254276C) stored with item_add_to_inventory; withered -> the sad scene, -1 heart, reset.
 */
int town_farm_visit(void)
{
    const u8 *loc[3];
    int t;
    u8 v;
    int k;
    const u8 *p;
    struct TamagoFile **q;
    int z = 0;
    int n;

    if ((u8)(btn_a_presses | btn_b_presses) != z) {
        btn_a_presses = z;
        btn_b_presses = z;
    }
    if (btn_c_presses != 0) {
        ui_timeout = 10;
        return 0;
    }
    z = town_farm_event();
    switch (z) {
    case 0:
        t = util_rand_except(100, 100);
        if (t <= 0x20) {
            chara_select_other_by_set(0x4a);
            v = t % 3 - 0x41;
        } else if (t <= 0x41) {
            chara_select_other_by_set(0x66);
            v = t % 3 - 0x44;
        } else {
            chara_select_other_by_set(0x67);
            v = t % 3 - 0x47;
        }
        anim_pic_slots[0][1] = (const u8 *)((u32)town_farm_plant_pic(save.pet.farm_stage + 1));
        anim_farm_farmer();
        anim_pic_slots[0][0] = anim_pic_slots[0][1];
        ui_show_message(0x1e, 0, chara_text(0, 100, v));
        goto tail;
    case 1:
        chara_select_other_by_set(0x67);
        anim_pic_slots[0][1] = (const u8 *)((u32)town_farm_plant_pic(0));
        anim_farm_farmer();
        anim_pic_slots[0][0] = anim_pic_slots[0][1];
        ui_show_message(0x1e, 0, chara_text(0, 100, 0xb7));
        anim_farm_plant();
        goto tail;
    case 2:
        anim_pic_slots[0][0] = (const u8 *)((u32)town_farm_plant_pic(save.pet.farm_stage));
        anim_pic_slots[0][1] = (const u8 *)((u32)town_farm_plant_pic(save.pet.farm_stage + 1));
        anim_farm_walk();
        anim_farm_grow();
        goto tail;
    case 3:
        anim_pic_slots[0][0] = (const u8 *)((u32)town_farm_plant_pic(5));
        anim_farm_walk();
        n = 0x50;
        if (util_rand_except(n, n) < 0x1e) {
            anim_pic_slots[0][0] = (const u8 *)((u32)town_farm_plant_pic(6));
            t = util_rand_except(100, 100);
            p = &D_2542768[t % 3];
        } else {
            anim_pic_slots[0][0] = (const u8 *)((u32)town_farm_plant_pic(7));
            t = util_rand_except(100, 100);
            p = &D_254276C[t % 5];
        }
        q = &item_builtin_records[*p];
        item_load_pictures(loc, *q, 3);
        anim_farm_harvest(loc[0]);
        item_add_to_inventory(*q);
    tail:
        anim_happy();
        pet_add_happiness(1);
        break;
    case 4:
        anim_pic_slots[0][0] = (const u8 *)((u32)town_farm_plant_pic(save.pet.farm_stage + 1));
        anim_pic_slots[0][1] = (const u8 *)((u32)town_farm_plant_pic(8));
        chara_select_other_by_set(0x66);
        anim_farm_withered();
        ui_show_message(0x1f, 0, chara_text(0, 100, 0xb8));
        anim_sad();
        pet_sub_happiness(1);
        save.pet.farm_stage = 0;
        save.pet.farm_tended = 0;
        save.pet.farm_neglect_days = 0;
        break;
    }
    save_after_sound();
    return 1;
}

/*
 * Returns a (stub).
 */
int func_25096D0(u16 p0)
{
    return p0;
}

/*
 * Outing record outing (the "away mode"): x000 = where the character is (0 home, 1 the
 * game centre, 2..6 a school club, see F28), x001 = minute counter, x002 = 1 "it is time
 * to come home".
 *
 * outing_clear_counters: clear its counters x001 / x002.
 */
void outing_clear_counters(void)
{
    outing.minutes = 0;
    outing.come_home = 0;
}

/*
 * End the outing: clear outing.
 */
void outing_end(void)
{
    outing.destination = 0;
    outing.minutes = 0;
    outing.come_home = 0;
}

/*
 * Is it time to come home (away and outing.come_home == 1)?
 */
int outing_time_to_return(void)
{
    if (outing.destination != 0 && outing.come_home == 1)
        return 1;
    return 0;
}

/*
 * Opening hours check: the game centre (type 1) only between 10:00 and 14:59, outings
 * between 7:00 and 15:59; outside them x002 = 1 (time to come home).
 */
u8 outing_hours_check(void)
{
    u8 t;
    u8 s;

    t = rtc_get_hour();
    rtc_get_minute();
    s = outing.destination;
    if (s != 0) {
        if (s == 1) {
            if (t < 10 || t > 14) {
                outing.come_home = s;
                return s;
            }
        } else {
            if (t < 7 || t > 15) {
                outing.come_home = 1;
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Per-minute tick of the outing outing: at the game centre (type 1) every hour costs 50
 * points, and the visit ends when the pet has fewer than 50 points; then the opening hours
 * check.
 */
int outing_minute_tick(void)
{
    u8 s = outing.destination;

    if (s == 1) {
        outing.minutes++;
        if (outing.minutes > 59) {
            outing.minutes = 0;
            if (save.pet.gotchi_points <= 49) {
                outing.come_home = s;
                return s;
            }
            pet_spend_points(50);
        }
    }
    if (outing_hours_check())
        rtc_get_hour();
    return 0;
}

/*
 * Returns 0x1 (stub).
 */
int func_25097AA(void)
{
    return 1;
}

/*
 * A/B/C pressed during the fountain question: returns 3/2/1.
 */
int town_fountain_button(void)
{
    if (btn_a_presses != 0) {
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        return 3;
    }
    if (btn_b_presses != 0) {
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        return 2;
    }
    if (btn_c_presses != 0) {
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        return 1;
    }
    return 0;
}

/*
 * Fountain result (gold/silver/normal?): points, happiness, weight. The result k is always
 * 0 (only the normal one: happiness + 1); the other branches never run.
 */
int town_fountain_result(void)
{
    int k = 0;

    if (k == 1) {
        pet_add_points(60);
        pet_add_happiness(2);
        pet_sub_weight(2);
    } else if (k == 0) {
        pet_add_happiness(1);
    } else {
        snd_wait_end();
        return 0;
    }
    snd_wait_end();
    save_game();
    return 1;
}

/*
 * Four-line question (a0..a3) with a two-way choice on lines 3 / 4 (cursor i, A toggles):
 * returns the chosen line (0 / 1), -1 on C / timeout. Used by the fountain honesty test.
 */
int ui_question_four_lines_choice(int a0, int a1, int a2, int a3, u8 i)
{
    u8 a;
    u8 c;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x20, 0x10, a0, 0, 8);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x30, 0x10, a1, 0, 8);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x40, 0x10, a2, 0, 8);
    a = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x50, 0x10, a3, 0, 8);
    c = gfx_object_create(6, (i << 4) + 0x42, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    do {
        if (btn_a_presses != 0) {
            ui_timeout = 10;
            i = i + 1;
            if (i > 1)
                i = 0;
            gfx_object_move_dirty(c, 6, (i << 4) + 0x42);
            func_00009A6();
            btn_a_presses = 0;
        }
        if (btn_b_presses != 0) {
            btn_a_presses = 1;
            btn_b_presses = 1;
            btn_c_presses = 1;
            snd_wait_end();
            return i;
        }
        if (btn_c_presses != 0)
            break;
        sys_idle_tick();
    } while (ui_timeout != 0);
    return -1;
}

/*
 * Fountain event helper (asm).
 */
int item_find_same_kind(int a, u8 n, int t, int out)
{
    struct TamagoFile *p = a;
    int i;

    for (i = 0; i < n; i++)
        if (((struct TamagoFile **)t)[i]->x04e == p->x04e)
            if (((struct TamagoFile **)t)[i]->x04f == p->x04f)
                if (((struct TamagoFile **)t)[i]->sub_kind == p->sub_kind)
                    if (((struct TamagoFile **)t)[i]->x05a == p->x05a)
                        if (((struct TamagoFile **)t)[i]->x05b == p->x05b) {
                            if (((u8 *)out)[i] == 0xff)
                                return 2;
                            return 0;
                        }
    return 1;
}

/*
 * Owned item id as a record usable by the fountain event (*out).
 */
int town_fountain_owned_item(int id, struct TamagoFile **out)
{
    int n;
    struct TamagoFile *p;

    n = item_get_count(id);
    if (n != 0) {
        p = item_record_by_index(id);
        if (p->x000_h != 0xffff) {
            *out = p;
            if ((*out)->x04e == 0x81 || (*out)->x04e == 1) {
                switch ((*out)->x04f) {
                case 2:
                    if (D_2535F50[save.pet.worn_accessory] == id)
                        n--;
                    break;
                case 3:
                    if (D_2535F68[save.pet.worn_clothes] == id)
                        n--;
                    break;
                }
                if (n == 0)
                    n = -1;
            }
        } else {
            n = 0;
        }
    }
    return n;
}

extern const u8 D_25427FC[];
/*
 * Fountain: pick two owned items for the "dropped item" question (asm).
 */
u8 town_fountain_pick_items(int a, int b)
{
    int e;
    u8 w0[1];
    u8 w1[1];
    u8 n, k;
    int i;
    u32 j;
    int r;

    {
    const struct Inventory *t = &item_inv_clothes;
    k = 0;
    int list[(n = t[0].count + t[1].count, n = n + t[-2].count)];
    u8 flags[n];
    u8 c0, c1;

    for (i = 0; i < item_inv_clothes.count; i++) {
        r = town_fountain_owned_item(item_inv_clothes.indexes[i], (struct TamagoFile **)&e);
        if (r != 0) {
            list[k] = e;
            flags[k] = r;
            k++;
        }
    }
    for (i = 0; i < item_inv_accessories.count; i++) {
        r = town_fountain_owned_item(item_inv_accessories.indexes[i], (struct TamagoFile **)&e);
        if (r != 0) {
            list[k] = e;
            flags[k] = r;
            k++;
        }
    }
    for (i = 0; i < item_inv_daily.count; i++) {
        r = town_fountain_owned_item(item_inv_daily.indexes[i], (struct TamagoFile **)&e);
        if (r != 0) {
            list[k] = e;
            flags[k] = r;
            k++;
        }
    }
    if (k == 0 || k >= n)
        return 0;
    c0 = c1 = 0;
    for (j = 0; j < 1; j++) {
        n = D_25427FC[j];
        e = item_builtin_records[n];
        switch (item_find_same_kind(e, k, (int)list, (int)flags)) {
        case 0:
            w0[c0] = n;
            c0++;
            break;
        case 1:
            w1[c1] = n;
            c1++;
            break;
        }
    }
    if (c0 == 0 || c1 == 0)
        return 0;
    n = w0[util_rand_except(c0, c0)];
    *(int *)a = item_builtin_records[n];
    n = w1[util_rand_except(c1, c1)];
    *(int *)b = item_builtin_records[n];
    return 1;
    }
}

/*
 * The honesty test at the fountain ("あなたが おとしたのは" = what you dropped was ...):
 * our item record a and a tempting item b (town_fountain_pick_items picks them) are shown in random
 * order; choosing the honest answer (a) earns +2 hearts and item b.
 */
void town_fountain_honesty_test(struct TamagoFile *a, int b0)
{
    struct TamagoFile *b = (struct TamagoFile *)b0;
    struct TamagoFile *p, *q;
    const u8 *t1[7], *t2[7], *t3[7], *t4[7];
    int f;
    int r;

    if (util_rand_except(2, 2) != 0) {
        p = b;
        q = a;
        f = 0;
    } else {
        p = a;
        q = b;
        f = 1;
    }
    if (p->x04f == 3) {
        item_load_pictures(t1, p, 7);
        anim_pic_slots[4][1] = (const u8 *)t1[6];
    } else
        item_load_pictures(&anim_pic_slots[4][1], p, 1);
    if (q->x04f == 3) {
        item_load_pictures(t2, q, 7);
        anim_pic_slots[4][0] = (const u8 *)t2[6];
    } else
        item_load_pictures(&anim_pic_slots[4][0], q, 1);
    r = ui_question_four_lines_choice((int)D_2542800, (int)D_254280C, (int)&p->name, (int)&q->name, 0);
    if (r == -1)
        return;
    if ((f == 1 && r == 0) || (f == 0 && r == 1)) {
        pet_add_happiness(2);
        item_add_to_inventory(b);
        save_game();
        return;
    }
    if (a->x04f == 3) {
        item_load_pictures(t3, a, 7);
        anim_pic_slots[4][0] = (const u8 *)t3[6];
    } else
        item_load_pictures(&anim_pic_slots[4][0], a, 1);
    if (b->x04f == 3) {
        item_load_pictures(t4, b, 7);
        anim_pic_slots[4][1] = (const u8 *)t4[6];
    } else
        item_load_pictures(&anim_pic_slots[4][1], b, 1);
}

const u8 D_2542788[] = _("メリーゴーランド");
const u8 D_254279C[] = _("おばけやしき");
const u8 D_25427AC[] = _("コーヒーカップ");
const u8 D_25427BC[] = _("かんらんしゃ");
/* unreferenced: a list of the four attractions (name, 0) with a count and a callback */
struct AttractionListMenu {
    u32 count;
    void (*x004)(void);
    struct {
        const u8 *count;
        u32 x004;
    } entries[5];
};
const struct AttractionListMenu D_25427CC = {
    4, func_25097AA,
    { { D_2542788, 0 }, { D_254279C, 0 }, { D_25427AC, 0 }, { D_25427BC, 0 }, { 0, 0 } },
};
const u8 D_25427FC[4] = { 0, 0, 0, 0 };
const u8 D_2542800[] = _("あなたが");
const u8 D_254280C[] = _("おとしたのは");
const u8 D_254281C[] = _("10$なげいれる?");

/*
 * Fountain visit: about 10 % the honesty test (town_fountain_honesty_test), else "10$なげいれる?" (throw in
 * 10$?): pays 10 points for +5..10 on one random stat of the five (pet_add_traits applies the
 * vector).
 */
int town_fountain_visit(void)
{
    u8 buf[5];
    struct TamagoFile *a;
    struct TamagoFile *b;
    u16 k;
    int n;

    if (util_rand_except(100, 100) > 0x5a && town_fountain_pick_items((int)&a, (int)&b) != 0) {
        town_fountain_honesty_test(a, (int)b);
        return 1;
    }
    if ((u8)(btn_a_presses | btn_b_presses)) {
        btn_a_presses = 0;
        btn_b_presses = 0;
    }
    if (btn_c_presses != 0)
        return 0;
    if (ui_confirm_yes_no(D_254281C, 0) != 0) {
        if (pet_can_pay(10) == 0)
            return 1;
        pet_spend_points(10);
        k = util_rand_except(10, 10) % 5;
        n = (u16)(util_rand_except(10, 10) % 6) + 5;
        util_rand_except(100, 100);
        memset(buf, 0, 5);
        switch (k) {
        case 0:
            buf[0] = n;
            break;
        case 1:
            buf[1] += n;
            break;
        case 2:
            buf[2] += n;
            break;
        case 3:
            buf[3] += n;
            break;
        case 4:
        default:
            buf[4] += n;
            break;
        }
        pet_add_traits(buf);
        pet_add_happiness(1);
        snd_wait_end();
        save_game();
        return 1;
    } else {
        (void)ui_timeout;
        return 1;
    }
}

/*
 * The RAM variables this file defines.
 */
struct Outing outing;
