/*
 * Meals: the food menu (meal or snack from the inventory), eating with the like / dislike
 * reactions, the restaurant visit, and the birthday / party dinners with guests and the kitchen
 * styles.
 *
 * Code 240DB16..240F184, .rodata from 25391D8.
 */
#include "proto/food/food_meals.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/room/room_draw.h"
#include "proto/town/town_places.h"
#include "proto/shop/shop_reform.h"
#include "proto/shop/shop_browser.h"
#include "proto/anim/anim_scripts.h"
#include "proto/evo/evo_counters.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_message.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care.h"
#include "proto/owner/owner_profile.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"

/*
 * F9 = FOOD: the meal / snack menus (ごはん / おやつ: れいぞうこ fridge, レストラン,
 * カフェレストラン, たまベーカリー, たまカフェ, たまともごはん / たまともおやつ = cooked by a
 * Tama-friend), eating animations, the party meal on the owner's / a character's birthday
 * (パーティごはん / バースデーケーキ), and the restaurant staff lines (D_25391DC.. "I'll make
 * it" / "can't cook today" per character). Eating goes through food_eat_meal (meal) /
 * food_eat_snack (snack) for fridge items and food_eat_restaurant_meal / food_eat_restaurant_snack for restaurant
 * food, which also feed the partner, count the meals, fill a clover leaf with a liked item
 * (pet_fill_clover_leaf) and bump the downloaded characters' affection (dl_chara_affection_update).
 *
 * food_share_pics: copy the pictures (the food) of anim_pic_slots row 0 into the partner rows 3
 * (and 4 when k > 1, a shared meal; else the placeholders anim_clear_pic_slot(12..14)) and
 * reload the partner set (visit_load_partner_pics(0)) for the eating animation.
 */
void food_share_pics(u8 k)
{
    int i;

    for (i = 0; i <= 2; i++)
        anim_pic_slots[3][i] = anim_pic_slots[0][i];
    if (k > 1) {
        for (i = 0; i <= 2; i++)
            anim_pic_slots[4][i] = anim_pic_slots[0][i];
    } else {
        for (i = 0; i <= 2; i++)
            anim_clear_pic_slot(i + 12);
    }
    visit_load_partner_pics(0);
}

/*
 * Set the kitchen design pet.room_designs[1] (= pet.room_designs[1], the キッチン room's style;
 * the restaurant meals set a venue style here and restore the kitchen after) = k and load
 * its background / foreground pair (room_kitchen_bg_pics / room_kitchen_fg_pics ->
 * anim_scene_bg / anim_scene_fg).
 */
void room_set_kitchen_design(int k)
{
    save.pet.room_designs[1] = k;
    anim_scene_bg = room_kitchen_bg_pics[save.pet.room_designs[1]];
    anim_scene_fg = room_kitchen_fg_pics[save.pet.room_designs[1]];
}

/*
 * Is today the owner's birthday (pet.owner_bday_month month / owner_bday_day[0] day) and the party not
 * held yet (flag 0x43)? 1 yes, -1 no, 0 already celebrated.
 */
int food_owner_birthday_today(void)
{
    u8 a, b;

    if (pet_get_unlock_flag(0x43) == 1)
        return 0;
    a = rtc_get_month();
    b = rtc_get_day();
    if (save.pet.owner_bday_month != a)
        return -1;
    if (save.pet.owner_bday_day == b)
        return 1;
    return -1;
}

/*
 * Is today a character's birthday (flag 0x42 = already celebrated -> 0)? 1 = our own
 * (its family's day from chara_find_by_codes, or the downloaded character's from dl_pet_by_codes),
 * 3 = some other character's, -1 none.
 */
int food_chara_birthday_today(void)
{
    u8 a;
    u8 b;
    u16 k;
    int i;

    if (pet_get_unlock_flag(0x42) == 1)
        return 0;
    a = rtc_get_month();
    b = rtc_get_day();
    k = save.pet.x000;
    if (k > 0x1388) {
        if (dl_pet_by_codes(a, b) == save.pet.x000)
            return 1;
    }
    for (i = 0;; i++) {
        if (chara_table[i].id == save.pet.x000) {
            k = chara_table[i].family;
            break;
        }
        if (chara_table[i].id == 0xffff)
            break;
    }
    if (chara_find_by_codes(a, b, 3) == k)
        return 1;
    if (chara_find_by_codes(a, b, 3) >= 0)
        return 3;
    return -1;
}

/*
 * Kitchen design index of a meal scene: 7 (party meal) or 6. No caller in this firmware.
 */
int food_meal_kitchen_style_b(void)
{
    return (int)food_birthday_info.kind >= 0 ? 7 : 6;
}

/*
 * Kitchen design index of the restaurant meal scenes (for room_set_kitchen_design): 5 (party
 * meal) or 4.
 */
u8 food_meal_kitchen_style(void)
{
    return (int)food_birthday_info.kind >= 0 ? 5 : 4;
}

/*
 * Party table picture: 0x20AAA14 on the owner's birthday (food_owner_birthday_today), else 0x20A97EC.
 */
int food_party_table_pic(void)
{
    return food_owner_birthday_today() >= 0 ? 0x20aaa14 : 0x20a97ec;
}

/*
 * The same for a character's birthday (food_chara_birthday_today).
 */
int food_party_table_pic_chara(void)
{
    return food_chara_birthday_today() >= 0 ? 0x20aaa14 : 0x20a97ec;
}

/*
 * Order the guest list (asm): make its first n entries usable guests (built-in ids
 * <= 2000, not our own family x006) by swapping in later entries.
 */
void food_order_guest_list(u16 *list, int n)
{
    u16 *p = list;
    u16 fam = save.pet.x000;
    int i, j;
    u16 t;

    for (j = 0;; j++) {
        if (chara_table[j].id == save.pet.x000) {
            fam = chara_table[j].family;
            break;
        }
        if (chara_table[j].id == 0xffff)
            break;
    }
    for (i = 0; i < n; p++, i++) {
        if (*p > 2000 || *p == fam) {
            for (j = i + 1; j <= 0x20; j++) {
                if (list[j] > 2000 || list[j] == fam)
                    continue;
                t = *p;
                *p = list[j];
                list[j] = t;
                break;
            }
        }
    }
}

/*
 * Party guests for the birthday: a shuffled list of the adult characters (ids <= 2000)
 * of other families than ours, album-collected (raised) families first; a = how many
 * (1..3) to load into the three guest sprite sets (characters 2..4: chara_select_other_by_id /
 * chara_select_third_by_id / chara_select_fourth_by_id; 9999 = none).
 */
void food_load_party_guests(int a)
{
    u16 list[33];
    const struct CharaEntry *p;
    u16 id;
    int n;
    int j;
    u16 t;

    p = chara_table;
    id = save.pet.x000;
    n = 0;
    memset(list, 0, sizeof(list));
    for (; p->id != 0xffff; p++) {
        if (p->id == save.pet.x000) {
            id = p->family;
            break;
        }
    }
    for (p = chara_table; p->id <= 2000; p++) {
        if (p->stage == 3 && p->id != id)
            list[n++] = p->id;
    }
    util_shuffle_u16(list, n);
    for (n = 0; list[n] != 0; n++) {
        if (pet_get_unlock_flag(chara_family_index(list[n]) + 0x1d) == 0) {
            for (j = n + 1; list[j] != 0; j++) {
                if (pet_get_unlock_flag(chara_family_index(list[j]) + 0x1d) != 0) {
                    t = list[n];
                    list[n] = list[j];
                    list[j] = t;
                    break;
                }
            }
        }
    }
    switch (a) {
    case 1:
        chara_select_other_by_id(list[0]);
        chara_select_third_by_id(9999);
        chara_select_fourth_by_id(9999);
        break;
    case 2:
        chara_select_other_by_id(list[0]);
        chara_select_third_by_id(list[1]);
        chara_select_fourth_by_id(9999);
        break;
    case 3:
        chara_select_other_by_id(list[0]);
        chara_select_third_by_id(list[1]);
        chara_select_fourth_by_id(list[2]);
        break;
    default:
        chara_select_other_by_id(9999);
        chara_select_third_by_id(9999);
        chara_select_fourth_by_id(9999);
        break;
    }
}

/*
 * Load up to three marriage-candidate characters (chara_list_marriage_candidates, shuffled, ordered by
 * food_order_guest_list) as meal companions, by how many families have been raised (album_count:
 * 0 none, 1 one, 2 two, more = three).
 */
void food_load_candidate_guests(void)
{
    u16 buf[34];
    int n = 0;
    int i;

    chara_select_other_by_id(9999);
    chara_select_third_by_id(9999);
    chara_select_fourth_by_id(9999);
    memset(buf, n, 0x42);
    switch (album_count()) {
    case 0:
        break;
    case 1:
        n = chara_list_marriage_candidates(buf, save.pet.x000);
        food_order_guest_list(buf, 1);
        if (n > 0) {
            if (buf[0] >= 1 && buf[0] <= 2000)
                chara_select_other_by_id(buf[0]);
        }
        break;
    case 2:
        n = chara_list_marriage_candidates(buf, save.pet.x000);
        if (n > 1) {
            util_shuffle_u16(buf, n);
            food_order_guest_list(buf, 2);
            for (i = 0; i < 2; i++) {
                if (buf[i] >= 1 && buf[i] <= 2000)
                    chara_select_other_by_id(buf[i]);
            }
        } else {
            if (buf[0] >= 1 && buf[0] <= 2000)
                chara_select_other_by_id(buf[0]);
        }
        break;
    default:
        n = chara_list_marriage_candidates(buf, save.pet.x000);
        util_shuffle_u16(buf, n);
        food_order_guest_list(buf, 3);
        if (buf[0] >= 1 && buf[0] <= 2000)
            chara_select_other_by_id(buf[0]);
        if (buf[1] >= 1 && buf[1] <= 2000)
            chara_select_third_by_id(buf[1]);
        if (n > 2) {
            if (buf[2] >= 1 && buf[2] <= 2000)
                chara_select_fourth_by_id(buf[2]);
        }
        break;
    }
}

/*
 * Party / meal scene with guests: on the owner's birthday (food_birthday_info.kind == 1) three
 * guests and the party animation anim_birthday_party; otherwise one candidate guest
 * (anim_party_guest) or alone (anim_party_alone). food_party_state = {kind, characters at the table,
 * variant}. Returns 1 alone / 2 with guests.
 */
u8 food_party_scene(const u8 *p)
{
    u16 buf[54];
    int r;

    rtc_get_month();
    rtc_get_day();
    album_count();
    food_load_candidate_guests();
    r = food_birthday_info.kind;
    if (r == 1) {
        food_load_party_guests(3);
        anim_birthday_party();
        food_party_state.kind = r;
        food_party_state.table_charas = 4;
        food_party_state.variant = r;
        return 2;
    }
    r = chara_list_marriage_candidates(buf, save.pet.x000);
    if (r == 0) {
        anim_party_alone(p);
        food_party_state.kind = r;
        food_party_state.table_charas = 1;
        food_party_state.variant = 1;
        return 1;
    }
    util_shuffle_u16(buf, r);
    food_order_guest_list(buf, 1);
    if (buf[0] >= 1 && buf[0] <= 2000) {
        chara_select_other_by_id(buf[0]);
        anim_party_guest(p);
        food_party_state.kind = 0;
        food_party_state.table_charas = 2;
        food_party_state.variant = 2;
        return 2;
    }
    anim_party_alone(p);
    food_party_state.kind = 0;
    food_party_state.table_charas = 1;
    food_party_state.variant = 1;
    return 1;
}

/*
 * The same with two more cases: a neglected character (x011 & 2) refuses (anim_refuse_food_table);
 * on another character's birthday (food_birthday_info.kind == 3, chara_birthday_on on the date
 * food_birthday_info.month / x009) that character's family is invited (anim_party_extra). Owner's
 * birthday: three guests with anim_party_neglected. Returns 1 alone / 2 with guests.
 */
u8 food_party_scene_b(const u8 *p)
{
    u16 buf[34];
    int r;
    u16 id;
    int i;

    rtc_get_month();
    rtc_get_day();
    album_count();
    if (save.pet.status & 2) {
        anim_refuse_food_table(p);
        return;
    }
    food_load_candidate_guests();
    r = food_birthday_info.kind;
    if (r == 1) {
        food_load_party_guests(3);
        anim_party_neglected();
        food_party_state.kind = r;
        food_party_state.table_charas = 4;
        food_party_state.variant = r;
        return 2;
    }
    if (food_birthday_info.kind == 3) {
        id = chara_birthday_on(food_birthday_info.month, food_birthday_info.day);
        for (i = 0;; i++) {
            if (chara_table[i].id == id) {
                id = chara_table[i].family;
                break;
            }
            if (chara_table[i].id == 0xffff)
                break;
        }
        chara_select_other_by_id(id);
        chara_select_third_by_id(9999);
        chara_select_fourth_by_id(9999);
        anim_party_extra();
        food_party_state.kind = 1;
        food_party_state.table_charas = 4;
        food_party_state.variant = 1;
        return 2;
    }
    r = chara_list_marriage_candidates(buf, save.pet.x000);
    if (r == 0) {
        anim_party_alone(p);
        food_party_state.kind = r;
        food_party_state.table_charas = 1;
        food_party_state.variant = 1;
        return 1;
    }
    util_shuffle_u16(buf, r);
    food_order_guest_list(buf, 1);
    if (buf[0] >= 1 && buf[0] <= 2000) {
        chara_select_other_by_id(buf[0]);
        anim_party_guest(p);
        food_party_state.kind = 0;
        food_party_state.table_charas = 2;
        food_party_state.variant = 2;
        return 2;
    }
    anim_party_alone(p);
    food_party_state.kind = 0;
    food_party_state.table_charas = 1;
    food_party_state.variant = 1;
    return 1;
}

/*
 * Party dish screen: "パーティごはん" (a == 0) or "バースデーケーキ" (a != 0) with its
 * pictures; B = -1 (chosen), C / timeout 0.
 */
int food_party_dish_screen(int a)
{
    const u8 *s;
    u8 n;

    btn_clear();
    if (a == 0) {
        anim_pic_slots[3][1] = D_21944F0;
        anim_pic_slots[3][2] = D_2194854;
        anim_pic_slots[4][0] = D_2194BB8;
        anim_clear_pic_slot(0xd);
        anim_clear_pic_slot(0xe);
        anim_clear_pic_slot(0xf);
        anim_pic_slots[4][0] = D_21944F0;
        anim_pic_slots[4][1] = D_2194854;
        anim_pic_slots[4][2] = D_2194BB8;
    } else {
        anim_pic_slots[3][1] = D_2195134;
        anim_pic_slots[3][2] = D_2195494;
        anim_pic_slots[4][0] = D_21957F4;
        anim_clear_pic_slot(0xd);
        anim_clear_pic_slot(0xe);
        anim_clear_pic_slot(0xf);
        anim_pic_slots[4][0] = D_2195134;
        anim_pic_slots[4][1] = D_2195494;
        anim_pic_slots[4][2] = D_21957F4;
    }
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    gfx_object_create(0x3f - ((anim_pic_slots[3][1])[0] >> 1), 0x32 - ((anim_pic_slots[3][1])[1] >> 1), 0x10, 0, 0, anim_pic_slots[3][1]);
    s = a == 0 ? D_253A488 : D_253A498;
    n = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_centered(n, 0x49, 0x30, s, 0, 9);
    func_00009A6();
    ui_timeout = 10;
    btn_clear();
    for (;;) {
        if (btn_a_presses) {
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses)
            break;
        if (btn_c_presses) {
            ui_timeout = 10;
            return 0;
        }
        sys_idle_tick();
        if (ui_timeout == 0)
            return 0;
    }
    ui_timeout = 10;
    return -1;
}

/*
 * Eat MEAL item slot a from the fridge: refused when full (pet_can_eat == 0 ->
 * anim_refuse_food, returns 0); 1 in 10 the character turns the food down (anim_food_refused,
 * pet.discipline_flags bit 0, returns -1); the reaction by the pet's preference item_pet_preference_by_index (1 = likes
 * it, also offers the item to pet_fill_clover_leaf(1); 2 = dislikes it; other = neutral; one
 * animation each), then the item effects (item_apply_effects_by_index hunger etc.) and the logs
 * (evo_count_item_use_young care log, evo_count_item_use stats, item_apply_attributes diary). Returns 1 eaten.
 */
int food_eat_meal(int a)
{
    if (pet_can_eat() != 0) {
        if (rand() % 10 == 0) {
            anim_food_refused();
            save.pet.discipline_flags |= 1;
            return -1;
        }
        if (save.pet.x082 <= 3)
            save.pet.x082 = 0;
        switch (item_pet_preference_by_index(a)) {
        case 2:
            anim_eat_disliked();
            break;
        case 1:
            anim_eat_liked();
            pet_fill_clover_leaf(1, item_record_by_index(a));
            break;
        default:
            anim_eat_neutral();
            break;
        }
    } else {
        anim_refuse_food();
        return 0;
    }
    item_apply_effects_by_index(a);
    evo_count_item_use_young(5, item_record_by_index(a));
    evo_count_item_use(item_record_by_index(a));
    item_apply_attributes(item_record_by_index(a));
    return 1;
}

extern int rand(void);
/*
 * Eat SNACK item slot id: a neglected character (x011 & 2) refuses; when full, snacks
 * with a hunger effect (item_hunger_effect) are refused; 1 in 10 turned down as for meals; meal
 * counter pet_count_meal; the same preference reactions (pet_fill_clover_leaf(2) for 1) and effects;
 * for preference 1 the happiness changes by 4 - the item's richness (item_happiness_effect).
 */
int food_eat_snack(int id)
{
    u8 r;
    s8 d;

    if (save.pet.status & 2) {
        anim_refuse_food();
        return 0;
    }
    r = pet_can_eat();
    if (r == 0 && (s8)item_hunger_effect(item_record_by_index(id)) != 0) {
        anim_refuse_food();
        return r;
    }
    if (rand() % 10 == 0) {
        anim_food_refused();
        save.pet.discipline_flags |= 1;
        return -1;
    }
    if (save.pet.x082 <= 3)
        save.pet.x082 = 0;
    pet_count_meal();
    switch (item_pet_preference_by_index(id)) {
    case 2:
        anim_eat_disliked();
        break;
    case 1:
        anim_eat_liked();
        pet_fill_clover_leaf(2, item_record_by_index(id));
        break;
    default:
        anim_eat_neutral();
        break;
    }
    item_apply_effects_by_index(id);
    evo_count_item_use_young(5, (struct TamagoFile *)(item_record_by_index(id)));
    evo_count_item_use((struct TamagoFile *)(item_record_by_index(id)));
    if (item_pet_preference_by_index(id) == 1) {
        d = 4 - item_happiness_effect(item_record_by_index(id));
        if (d >= 0)
            pet_add_happiness(d);
        else
            pet_sub_happiness(-d);
    }
    item_apply_attributes((struct TamagoFile *)(item_record_by_index(id)));
    return 1;
}

/*
 * Returns 1 (stub).
 */
int func_240E606(void)
{
    return 1;
}

/*
 * Returns 1 (stub).
 */
int func_240E60A(void)
{
    return 1;
}

/*
 * Eat restaurant MEAL item a in scene p (table scene, room_set_kitchen_design): b == 1 = the party
 * meal (anim_party_meal, +3 happiness, hunger +6 = full); otherwise the same reaction /
 * effect sequence as the fridge, with the guest sprites anim_pic_slot_3_0.. when guests are
 * present. Returns 1 eaten, 0 refused (full: anim_refuse_food_table).
 */
u8 food_eat_restaurant_meal(int a, int b, const u8 *p)
{
    u8 t;

    if (b == 1) {
        t = save.pet.room_designs[1];
        room_set_kitchen_design(food_meal_kitchen_style());
        anim_party_meal();
        pet_add_happiness(3);
        pet_feed(6);
        save.pet.room_designs[1] = t;
        anim_happy();
        return 1;
    }
    t = save.pet.room_designs[1];
    room_set_kitchen_design(food_meal_kitchen_style());
    if (food_party_state.variant > 1) {
        anim_pic_slots[4][0] = anim_pic_slots[3][0];
        anim_pic_slots[4][1] = anim_pic_slots[3][1];
        anim_pic_slots[4][2] = anim_pic_slots[3][2];
    } else {
        anim_pic_slots[4][0] = D_2258DF4;
        anim_pic_slots[4][1] = D_2258DF4;
        anim_pic_slots[4][2] = D_2258DF4;
    }
    if (pet_can_eat()) {
        if (save.pet.x082 <= 3)
            save.pet.x082 = 0;
        switch (item_pet_preference((struct TamagoFile *)a)) {
        case 2:
            anim_eat_table_disliked(p);
            break;
        case 1:
            anim_eat_table_liked(p);
            save.pet.room_designs[1] = t;
            pet_fill_clover_leaf(1, (struct TamagoFile *)a);
            break;
        default:
            anim_eat_table_neutral(p);
            break;
        }
    } else {
        anim_refuse_food_table(p);
        save.pet.room_designs[1] = t;
        return 0;
    }
    item_apply_effects((struct TamagoFile *)a);
    evo_count_item_use_young(5, (struct TamagoFile *)a);
    evo_count_item_use((struct TamagoFile *)a);
    item_apply_attributes((struct TamagoFile *)a);
    save.pet.room_designs[1] = t;
    return 1;
}

/*
 * Returns 1 (stub).
 */
int func_240E744(void)
{
    return 1;
}

/*
 * The SNACK variant of food_eat_restaurant_meal: refusals as food_eat_snack, meal counter
 * pet_count_meal, activity counter pet_count_activity(3), happiness by the item's richness.
 */
u8 food_eat_restaurant_snack(int a, int b, const u8 *p)
{
    u8 t;
    s8 d;

    if (b == 1) {
        t = save.pet.room_designs[1];
        room_set_kitchen_design(food_meal_kitchen_style());
        anim_party_meal();
        save.pet.room_designs[1] = t;
        anim_happy();
        return 1;
    }
    t = save.pet.room_designs[1];
    room_set_kitchen_design(food_meal_kitchen_style());
    if (save.pet.status & 2) {
        anim_refuse_food_table(p);
        save.pet.room_designs[1] = t;
        return 0;
    }
    if (pet_can_eat() == 0 && (s8)item_hunger_effect((struct TamagoFile *)a) != 0) {
        anim_refuse_food_table(p);
        save.pet.room_designs[1] = t;
        return 0;
    }
    if (save.pet.x082 <= 3)
        save.pet.x082 = 0;
    if (food_party_state.variant > 1) {
        anim_pic_slots[4][0] = anim_pic_slots[3][0];
        anim_pic_slots[4][1] = anim_pic_slots[3][1];
        anim_pic_slots[4][2] = anim_pic_slots[3][2];
    } else {
        anim_pic_slots[4][0] = D_2258DF4;
        anim_pic_slots[4][1] = D_2258DF4;
        anim_pic_slots[4][2] = D_2258DF4;
    }
    pet_count_meal();
    switch (item_pet_preference((struct TamagoFile *)a)) {
    case 2:
        anim_eat_table_disliked(p);
        save.pet.room_designs[1] = t;
        break;
    case 1:
        anim_eat_table_liked(p);
        save.pet.room_designs[1] = t;
        pet_fill_clover_leaf(2, (struct TamagoFile *)a);
        break;
    default:
        anim_eat_table_neutral(p);
        save.pet.room_designs[1] = t;
        break;
    }
    pet_count_activity(3);
    item_apply_effects((struct TamagoFile *)a);
    evo_count_item_use_young(5, (struct TamagoFile *)a);
    evo_count_item_use((struct TamagoFile *)a);
    if (item_pet_preference((struct TamagoFile *)a) == 1) {
        d = 4 - item_happiness_effect((struct TamagoFile *)a);
        if (d >= 0)
            pet_add_happiness(d);
        else
            pet_sub_happiness(-d);
    }
    item_apply_attributes((struct TamagoFile *)a);
    save.pet.room_designs[1] = t;
    return 1;
}

/* this file called chara_id_alias without its prototype (argument passed as int) */
/*
 * Pick a random raised family (flags 0x1D..0x2B for the species groups 0x10..0x1E, mapped
 * to a character by chara_id_alias; not the current character) as the second character,
 * the Tama-friend cook (chara_select_other_by_id). Returns 0 if none, else 1.
 */
int food_pick_friend_cook(void)
{
    u8 buf[20];
    int n = 0;
    int i;
    int k;

    for (i = 0x10; i <= 0x1e; i++) {
        if (!pet_get_unlock_flag(i + 0xd))
            continue;
        if (pet_get_id() == i)
            continue;
        k = chara_id_alias(i);
        if (k == pet_get_id())
            continue;
        buf[n] = k;
        n++;
    }
    if (n == 0)
        return 0;
    util_rand_shuffle(buf, n);
    chara_select_other_by_id(buf[0]);
    return 1;
}

struct FoodVenue {
    const u8 *picture;
    u32 label;
    int x08;
    u8 bank;
    u8 filter;
    u8 kind;
    u8 pad;
};
extern const struct FoodVenue food_venues_meals[];
extern const struct FoodVenue food_venues_snacks[];
extern const u8 D_253A4AC[];
int f_2413B08(const u8 *, int, int) __asm__("town_travel_scene");
#define T ((struct FoodVenue *)a)
/*
 * Restaurant visit: table a = the two venues (food_venues_meals meals: レストラン / たまともごはん,
 * food_venues_snacks snacks: たまベーカリー / たまカフェ / たまともおやつ; rows {picture, ?, bank,
 * filter, kind: 1 restaurant meal, 2 / 5 travel variants, 4 restaurant snack}), b = start
 * row. Travel there (town_travel_scene); on a birthday (food_owner_birthday_today / food_chara_birthday_today, flags
 * 0x42 / 0x43 once a day) the party scene and the party menu replace the normal order;
 * otherwise food_meal_or_snack_choice asks ごはん / おやつ, the bank browser item_bank_browser shows the
 * dishes over the download slots item_bank_tbl ("たべる?" with the three-way eat / no /
 * delete box for downloaded items - the delete rewrites its flash sector -, the sale badge
 * when flag 0x16), affordability check, eat (food_eat_restaurant_meal / food_eat_restaurant_snack), pay. Returns 1
 * ate, 0 back, -1 timeout.
 */
int food_restaurant_visit(int a, u8 b)
{
    int w0;
    u32 sel;
    struct TamagoFile *it;
    u8 v;
    int v2;
    u8 mode;
    const u8 *img;
    u8 k;
    s8 s;
    int ra;
    int rb;
    int f;
    u8 t;
    int x2;
    u32 x0;
    u32 n;
    struct FoodVenue *e;

    mode = 0;
    img = D_20A97EC;
    k = b;
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (k <= 1) {
            w0 = -1;
            it = 0;
            ui_timeout = 10;
            e = (struct FoodVenue *)(k * 16 + a);
            switch (e->kind) {
            case 2:
                f = 1;
                break;
            case 5:
                f = 2;
                break;
            default:
                f = 0;
                break;
            }
            if (f_2413B08(e->picture, 0, f) != 0)
                return 0;
            if (T[k].kind == 1) {
                mode = 0;
                food_birthday_info.party_kind = -1;
                food_birthday_info.kind = -1;
                food_birthday_info.month = 0;
                food_birthday_info.day = 0;
                ra = food_owner_birthday_today();
                rb = food_chara_birthday_today();
                if (ra > 0) {
                    mode = 1;
                    food_birthday_info.party_kind = 1;
                    food_birthday_info.kind = ra;
                    food_birthday_info.month = rtc_get_month();
                    food_birthday_info.day = rtc_get_day();
                    pet_set_unlock_flag(0x43, mode);
                } else if (rb > 0) {
                    mode = 1;
                    food_birthday_info.party_kind = 0;
                    food_birthday_info.kind = rb;
                    food_birthday_info.month = rtc_get_month();
                    food_birthday_info.day = rtc_get_day();
                    pet_set_unlock_flag(0x42, mode);
                } else if (ra == 0 || rb == 0) {
                    food_birthday_info.kind = 0;
                }
                if (ra <= -1) {
                    img = D_20A97EC;
                    if (rb <= -1)
                        goto skip;
                }
                img = D_20AAA14;
            skip:

                t = save.pet.room_designs[1];
                room_set_kitchen_design(food_meal_kitchen_style());
                anim_restaurant_arrive(img);
                ui_show_message(0xd, img, chara_text(0, 100, 0x99));
                anim_restaurant_order(img);
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                ui_timeout = 10;
                if ((u8)(btn_a_presses | btn_b_presses)) {
                    btn_a_presses = 0;
                    btn_b_presses = 0;
                }
                if (btn_c_presses)
                    goto L13;
                if (food_birthday_info.party_kind == 1 && food_birthday_info.kind == 1)
                    v = food_party_scene(img);
                else
                    v = food_party_scene_b(img);
                save.pet.room_designs[1] = t;
            }
        L4:
            if (mode == 1) {
                if (food_party_dish_screen(food_birthday_info.party_kind) == 0)
                    return ui_timeout ? 0 : -1;
                goto L17;
            }
        L16:
            s = food_meal_or_snack_choice(k);
            if (s == -1)
                goto L19;
            if (s == -2)
                goto L20;
            k = s;
            a = k == 0 ? (int)food_venues_meals : (int)food_venues_snacks;
            w0 = -1;
            for (;;) {
        L31:
            if (T[k].kind == 1 || T[k].kind == 4) {
                if (pet_get_unlock_flag(0x16))
                    shop_set_discount_mode(2);
            }
            it = (struct TamagoFile *)item_bank_browser(T[k].x08, T[k].bank, 1, &w0);
            if (it) {
                food_share_pics(v);
                if (w0 < item_bank_tbl[T[k].x08].slot_count) {
                    sel = 1;
                    switch ((u32)ui_use_no_delete((int)D_253A4AC, 0, &sel)) {
                    case 1:
                        switch (sel) {
                        case 0:
                            break;
                        case 1:
                            goto L31;
                        case 2:
                            if (ui_confirm_really()) {
                                x2 = item_bank_tbl[T[k].x08].first_sector;
                                x0 = item_bank_tbl[T[k].x08].slot_size;
                                snd_wait_end();
                                flash_backup_sector(x2 + (x0 * w0 >> 16));
                                flash_restore_sector_except(x2 + (x0 * w0 >> 16), it, x0);
                                snd_wait_end();
                                save_game();
                                w0 = item_bank_next_occupied(T[k].x08, w0, T[k].bank);
                            } else if (ui_timeout == 0) {
                                shop_set_discount_mode(0);
                                return -1;
                            }
                            btn_b_presses = 0;
                            btn_c_presses = 0;
                            goto L31;
                        }
                        break;
                    case 2:
                        goto L19;
                    case 3:
                        shop_set_discount_mode(0);
                        return -1;
                    }
                }
            } else {
                if (btn_c_presses)
                    goto L16;
                shop_set_discount_mode(0);
                return ui_timeout ? v2 : -1;
            }
        L17:
            if (mode == 1) {
                switch (T[k].kind) {
                case 1:
                    v2 = food_eat_restaurant_meal((int)it, mode, img);
                    if (v2)
                        dl_chara_affection_update((int)it);
                    break;
                case 4:
                    v2 = food_eat_restaurant_snack((int)it, mode, img);
                    if (v2)
                        dl_chara_affection_update((int)it);
                    break;
                }
                break;
            }
                n = item_price(it);
                if (pet_can_pay(n) == 0)
                    continue;
                switch (T[k].kind) {
                case 1:
                    v2 = food_eat_restaurant_meal((int)it, mode, img);
                    if (v2)
                        dl_chara_affection_update((int)it);
                    break;
                case 4:
                    v2 = food_eat_restaurant_snack((int)it, mode, img);
                    if (v2)
                        dl_chara_affection_update((int)it);
                    break;
                }
            L40:
                if (v2 == 0)
                    goto L31;
                pet_spend_points(n);
            }
        L36:

            if (it)
                goto L4;
            break;
        }
        if (btn_c_presses) {
            btn_c_presses = 0;
            goto L19;
        }
        sys_idle_tick();
    } while (ui_timeout);
    if (ui_timeout)
        ui_timeout = 10;
    sys_idle_seconds = 0;
    shop_set_discount_mode(0);
    if (ui_timeout)
        return 1;
    return -1;
L13:
    save.pet.room_designs[1] = t;
    return 0;
L20:
    shop_set_discount_mode(0);
    return -1;
L19:
    shop_set_discount_mode(0);
    return 0;
}

/*
 * Food menu entry k available? (0 = fridge always, 1/2 = restaurants once hatched.)
 */
u8 food_menu_entry_available(int k)
{
    switch (k) {
    case 0:
        return 1;
    case 1:
    case 2:
        return save.pet.life_stage != 0;
    }
    return 0;
}

/*
 * THE FOOD MENU (table food_menu_tbl: れいぞうこ fridge / カフェレストラン): the fridge lists the
 * owned food (index list food_fridge_inventory via item_inv_picker); choosing a dish asks "たべる?" (eat?
 * / no / delete) except for the default food (id 1), eats it (meal food_eat_meal / snack
 * food_eat_snack), consumes one unit (two with a partner, item_consume_shared) and bumps the
 * affection of downloaded characters whose favourite it is. The restaurants go through
 * food_restaurant_visit. Returns 1 when something was eaten.
 */
extern const struct Inventory food_fridge_inventory;
u8 food_menu(void)
{
    struct TamagoFile *it;
    int h;
    u32 sel;
    u8 ret;
    u8 cur;
    u8 img;
    u8 cnt;
    u8 m;
    int i;
    u8 a;
    int n, r;
    int flag;

    ret = 0;
    cur = 0;
again:
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    for (i = 0; i < 2; i++) {
        if (food_menu_entry_available(i)) {
            u32 p = food_menu_tbl[i].label;
            if (p == 0)
                break;
            a = gfx_object_create(0x18, 0x20, i * 4 + 0x30, 0, 0, 0);
            gfx_draw_text_line(a, 0x18, i * 16 + 0x20, i * 4 + 0x30, p, 0, 8);
        } else {
            u32 p = food_menu_tbl[i].label;
            if (p == 0)
                break;
            a = gfx_object_create(0x18, 0x20, i * 4 + 0x30, 0, 3, 0);
            gfx_draw_text_line(a, 0x18, i * 16 + 0x20, i * 4 + 0x30, p, 0, 8);
        }
    }
    cnt = i;
    img = gfx_object_create(6, cur * 16 + 0x22, 0x20, 0, 0, 0x2104714);
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
loop:
    if (btn_a_presses) {
        cur++;
        if (cur >= cnt)
            cur = 0;
        gfx_object_move_dirty(img, 6, cur * 16 + 0x22);
        func_00009A6();
        btn_a_presses = 0;
        ui_timeout = 10;
    }
    if (btn_b_presses) {
        ui_timeout = 10;
        if (food_menu_entry_available(cur) == 0) {
            snd_error_beep();
            btn_b_presses = 0;
            ui_timeout = 10;
            goto loop;
        }
        if (food_menu_tbl[cur].venues != 0) {
            if (cur == 1)
                m = 0;
            else if (cur == 2)
                m = 1;
            r = food_restaurant_visit(food_menu_tbl[cur].venues, m);
            if (r > 0) {
                ret = 1;
                goto out;
            }
            if (r == 0)
                goto again;
            goto out;
        }
        h = -1;
    next:
        n = item_inv_picker(&food_fridge_inventory, 0, &h);
        if (n >= 0) {
            flag = 0;
            it = item_record_by_index(n);
            food_share_pics(1);
            if (item_get_id(it) != 1) {
                sel = 1;
                switch ((u32)ui_use_no_delete((int)D_253A4AC, flag, &sel)) {
                case 1:
                    switch (sel) {
                    case 0:
                        break;
                    case 1:
                        goto next;
                    case 2:
                        if (ui_confirm_really()) {
                            item_set_count(n, 0);
                            snd_wait_end();
                            save_game();
                            if (item_inv_usable_count(&food_fridge_inventory) == 0)
                                goto again;
                            h = item_inv_next_usable(&food_fridge_inventory, h);
                        } else if (ui_timeout == 0) {
                            shop_set_discount_mode(0);
                            goto out;
                        }
                        btn_b_presses = 0;
                        btn_c_presses = 0;
                        goto next;
                    }
                    break;
                case 2:
                    goto next;
                case 3:
                    shop_set_discount_mode(0);
                    goto out;
                }
            }
            ret = 0;
            if (it->sub_kind == 1)
                r = food_eat_meal(n);
            else
                r = food_eat_snack(n);
            if (r == -1)
                return ret;
            if (r == 1) {
                ret = 1;
                flag = 1;
            }
            if (ret)
                dl_chara_affection_update((int)it);
            if (item_get_id(it) != 1 && flag && item_consume_shared(n)) {
                if (item_inv_usable_count(&food_fridge_inventory) == 0)
                    goto again;
                h = item_inv_next_usable(&food_fridge_inventory, h);
            }
        } else if (n == -2)
            return ret;
        if (n >= 0)
            goto next;
        goto again;
    }
    if (btn_c_presses == 0) {
        sys_idle_tick();
        if (ui_timeout)
            goto loop;
    }
out:
    return ret;
}

/*
 * "ごはん / おやつ" (meal / snack) two-entry choice (cursor starts on a != 0): 0 / 1, -1 on
 * C, -2 on timeout.
 */
s8 food_meal_or_snack_choice(u8 a)
{
    struct LabelPair c;
    int sel;
    s8 r;
    u8 n;

    c = food_meal_snack_labels;
    sel = a != 0;
    r = -1;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    gfx_draw_text_line(gfx_object_create(0, 0, 0, 0, 0, 0), 0x18, 0x20, 0x10, c.first, 0, 3);
    gfx_draw_text_line(gfx_object_create(0, 0, 0, 0, 0, 0), 0x18, 0x30, 0x10, c.second, 0, 3);
    n = gfx_object_create(6, sel * 16 + 0x22, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses) {
            sel = !sel;
            gfx_object_move_dirty(n, 6, sel * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            r = sel;
            break;
        }
        if (btn_c_presses)
            break;
        sys_idle_tick();
        if (ui_timeout == 0) {
            r = -2;
            break;
        }
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return r;
}

const u8 D_25391DC[] = _("わかりました!!\nがんばってつくります☼");
const u8 D_2539208[] = _("まかせてくれ★\nつくってやるぜ!!");
const u8 D_253922C[] = _("しょうちしました\nさっそくおつくりします");
const u8 D_2539258[] = _("りょうりもげいじゅつ☼\nぼくにおまかせだよ!!");
const u8 D_2539288[] = _("わかったぞう!でもぼくも\nたべたくなっちゃうぞう☼");
const u8 D_25392BC[] = _("まかせるだっち☼でも\nぼくもたべたいだっち♥");
const u8 D_25392EC[] = _("まかせてくれ!!\nバッチリきめるぜ★");
const u8 D_2539314[] = _("そんなこともあろうかと\nじゅんびしてるでござる!");
const u8 D_2539348[] = _("ぽくがおいしい\nごちそうつくるのぽ★");
const u8 D_2539370[] = _("わかったでしょ!おれ\nじぶんでつくってみるし★");
const u8 D_25393A0[] = _("ぼくにつくってもらえる\nなんてしあわせだね!!");
const u8 D_25393D0[] = _("まかせてころ♥\nごちそうするころ☼");
const u8 D_25393F4[] = _("ワルでビターなあじを\nおしえてやるぜ★");
const u8 D_253941C[] = _("ボクがヒーローらしく★\nかっこよくつくります!!");
const u8 D_2539450[] = _("ちょっとまってね★\nすぐにじゅんびするよ♥");
const u8 D_253947C[] = _("ごちそう?がっはっは\nよし､まかせとけ!");
const u8 D_25394A8[] = _("わかった!すてきな\nごちそうをつくるよ♪");
const u8 D_25394D4[] = _("よし､うでによりをかけて\nつくらせてもらいます!");
const u8 D_2539508[] = _("まかせてよ!\nおいしいのつくるよ★");
const u8 D_253952C[] = _("がんぱってつくるから\nちょっとまっててだっぺ!");
const u8 D_253955C[] = _("ピポパポピポ♪\n(ちょっとまってろ♪)");
const u8 D_2539584[] = _("すごいのをつくるから\nきたいするでヤンス!");
const u8 D_25395B0[] = _("はつめいがいそがしくて\nきょうはつくれません…");
const u8 D_25395E0[] = _("わるい｡きょうはノリが\nよくなくてつくれないぜ!");
const u8 D_2539614[] = _("きょうはあいにく\nおつくりできません…");
const u8 D_253963C[] = _("いま､えをかいてるから\nつくれないんだ…");
const u8 D_2539668[] = _("きょうのごはんは\nもうたべちゃったぞう～♥");
const u8 D_2539694[] = _("いまからおんせんにはいる\nから､つくれないだっち!");
const u8 D_25396C8[] = _("テレビでサッカーを\nみるんだ\nつくれないな!");
const u8 D_25396F8[] = _("しゅぎょうがいそがしくて\nつくれないでござる↓");
const u8 D_2539728[] = _("スノボがいそがしくて\nきょうはつくれないのぽ…");
const u8 D_2539758[] = _("あそぶよていあるし!\nつくれないでしょ!");
const u8 D_2539784[] = _("ぼく､きょうはしゅくだい\nするからつくれないよ");
const u8 D_25397B4[] = _("ねむたいころ…\nきょうはつくれないころ～");
const u8 D_25397E0[] = _("ちょっと…きょうは…\nつ､つくれないです…");
const u8 D_253980C[] = _("これからさつえいなんです\nまたこんどきてください★");
const u8 D_2539840[] = _("これからコンテストなんだ\nまたこんどきてよね★");
const u8 D_2539870[] = _("ごちそう?きょうはそんな\nきぶんじゃねえな!");
const u8 D_25398A0[] = _("いどうとしょかんのしゅっ\nぱつなんだ…またきてね!");
const u8 D_25398D4[] = _("すみません…きょうはいそ\nがしくてつくれないんです");
const u8 D_2539908[] = _("ごめん､これからサンタク\nロっちとまちあわせなんだ");
const u8 D_253993C[] = _("これからこきょうにかえる\nからつくれないんだっぺ");
const u8 D_2539970[] = _("ピポプポパピポ↓\n(きょうはつくれない↓)");
const u8 D_253999C[] = _("きょうはいそがしいから\nつくれないでヤンス…↓");
const u8 D_25399CC[] = _("わたしにまかせて♥\nはりきっちゃう!!");
const u8 D_25399F4[] = _("オーケー★オーケー!!\nミーにまかせて♪");
const u8 D_2539A20[] = _("チョベリグスイーツなら\nまかせてちょんまげ♥");
const u8 D_2539A50[] = _("ちゃまに\nまかせてくださいです～♥");
const u8 D_2539A74[] = _("ぐるぐるスイーツ@\nつくっちゃう!!");
const u8 D_2539A9C[] = _("はーい♥\nすぐにじゅんびするぺろ");
const u8 D_2539AC0[] = _("わらわが\nうでによりをかけるじょよ");
const u8 D_2539AE4[] = _("まかせてくださいですわ～\nオーホッホ♥");
const u8 D_2539B0C[] = _("おいし～い\nスイーツをつくるぴち♪");
const u8 D_2539B30[] = _("おはなみたいにステキな\nスイーツをつくるね♥");
const u8 D_2539B60[] = _("ゴーゴー!!たのしく\nクッキングー★ゴー!!");
const u8 D_2539B90[] = _("おいしいスイーツ\nつくってあげるね♥");
const u8 D_2539BB8[] = _("ふわふわのスイーツ\nごちそうするわた♪");
const u8 D_2539BE0[] = _("おいしいスイーツ\nごちそうするなちゅ♥");
const u8 D_2539C08[] = _("うわさのスイーツを\nごちそうするだわさ♥");
const u8 D_2539C34[] = _("こころをこめておいしい\nスイーツをつくりますね♥");
const u8 D_2539C68[] = _("とくべつなスイーツを\nつくってあげるわ★");
const u8 D_2539C94[] = _("いいわ､まっててね!\nごちそうしてあげる★");
const u8 D_2539CC0[] = _("わかったぴょん♪\nみみっちにおまかせぴょん");
const u8 D_2539CEC[] = _("わかりました､がんばって\nつくりますね★");
const u8 D_2539D18[] = _("ももね､がんばって\nつくるのね～↑");
const u8 D_2539D3C[] = _("おまかせください♥\nすぐによういしますわ!");
const u8 D_2539D68[] = _("わたくしがすばらしい\nメニューをよういしますわ");
const u8 D_2539D98[] = _("まかせて!ギガキュン♥な\nスイーツをよういするよ!");
const u8 D_2539DCC[] = _("ごめんなさい…\nきょうはつくれないの↓");
const u8 D_2539DF4[] = _("ソーリー!きょうは\nクッキングできないんだ…");
const u8 D_2539E24[] = _("ガビーン!きょうは\nつくれないよ↓またこんど");
const u8 D_2539E54[] = _("きょうはつくれません…\nごめんなさいです～@");
const u8 D_2539E84[] = _("ざんねん↓きょうは\nつくれないのよね…@");
const u8 D_2539EB0[] = _("きょうはつくれないぺろ\nごめんぺろ～↓");
const u8 D_2539ED8[] = _("すまぬ!きょうは\nつくれないじょよ…");
const u8 D_2539F00[] = _("きょうはつくれませんわ↓\nごめんあそばせ…");
const u8 D_2539F2C[] = _("きぶんがのらなくて…\nきょうはつくれないぴち");
const u8 D_2539F5C[] = _("きょうはつくれないの…\nごめんね↓");
const u8 D_2539F80[] = _("きょうはつくれないよー!\nゴメン★でもファイト\n!");
const u8 D_2539FB4[] = _("あげ?ゴメンね…\nきょうはつくれないんだ…");
const u8 D_2539FE0[] = _("ごめんね…きょうは\nごちそうできないわた～");
const u8 D_253A00C[] = _("ごめんなちゅ…きょうは\nつくれないなちゅ～");
const u8 D_253A038[] = _("きょうはいそがしくて\nつくれないんだわさ!");
const u8 D_253A064[] = _("ごめんなさい↓きょうは\nつくってあげられません…");
const u8 D_253A098[] = _("きょうはつくれないわ\nごめんなさいね…");
const u8 D_253A0C0[] = _("わるいけど､きょうは\nいそがしいのよ");
const u8 D_253A0E8[] = _("いまけいさんドリルを\nしててつくれないぴょん!");
const u8 D_253A118[] = _("ごめんなさい､きょうは\nつくれないんです…");
const u8 D_253A144[] = _("きょうはつくれないのね↓\nまたきてほしいのね");
const u8 D_253A174[] = _("ごめんなさい､これから\nおしろにもどらないと…");
const u8 D_253A1A4[] = _("ごめんなさい…これから\nでかけてしまいますのっ!");
const u8 D_253A1D8[] = _("いまマジいそがしいから\nムリ～↓");
/* unreferenced: the restaurant staff lines D_25391DC .. D_253A1D8 */
const u8 *const D_253A1FC[92] = {
    D_25391DC, D_2539208, D_253922C, D_2539258, D_2539288, D_25392BC,
    D_25392EC, D_2539314, D_2539348, D_2539370, D_25393A0, D_25393D0,
    D_25393F4, D_253941C, D_2539450, D_253947C, D_25394A8, D_25394D4,
    D_2539508, D_253952C, D_253955C, D_2539584, D_25395B0, D_25395E0,
    D_2539614, D_253963C, D_2539668, D_2539694, D_25396C8, D_25396F8,
    D_2539728, D_2539758, D_2539784, D_25397B4, D_25397E0, D_253980C,
    D_2539840, D_2539870, D_25398A0, D_25398D4, D_2539908, D_253993C,
    D_2539970, D_253999C, D_25399CC, D_25399F4, D_2539A20, D_2539A50,
    D_2539A74, D_2539A9C, D_2539AC0, D_2539AE4, D_2539B0C, D_2539B30,
    D_2539B60, D_2539B90, D_2539BB8, D_2539BE0, D_2539C08, D_2539C34,
    D_2539C68, D_2539C94, D_2539CC0, D_2539CEC, D_2539D18, D_2539D3C,
    D_2539D68, D_2539D98, D_2539DCC, D_2539DF4, D_2539E24, D_2539E54,
    D_2539E84, D_2539EB0, D_2539ED8, D_2539F00, D_2539F2C, D_2539F5C,
    D_2539F80, D_2539FB4, D_2539FE0, D_253A00C, D_253A038, D_253A064,
    D_253A098, D_253A0C0, D_253A0E8, D_253A118, D_253A144, D_253A174,
    D_253A1A4, D_253A1D8,
};
const u8 D_253A36C[] = _("レストラン");
const u8 D_253A378[] = _("たまともごはん");
extern u8 D_20A85C4[];
const struct FoodVenue food_venues_meals[4] = {
    { D_20A85C4, (u32)D_253A36C, 0, 2, 2, 1, 0 },
    { D_20BBA4C, (u32)D_253A378, 0, 3, 4, 2, 0 },
};
const u8 D_253A3C8[] = _("たまベーカリー");
const u8 D_253A3D8[] = _("たまカフェ");
const u8 D_253A3E4[] = _("たまともおやつ");
const struct FoodVenue food_venues_snacks[4] = {
    { D_20BBA4C, (u32)D_253A3C8, 1, 12, 2, 3, 0 },
    { D_20BBA4C, (u32)D_253A3D8, 1, 13, 3, 4, 0 },
    { D_20BBA4C, (u32)D_253A3E4, 1, 14, 4, 5, 0 },
};
const u8 D_253A434[] = _("れいぞうこ");
const u8 D_253A440[] = _("カフェレストラン");
/* the food menu: れいぞうこ (the owned food) / カフェレストラン (the meals food_venues_meals) */
const struct FoodMenuEntry food_menu_tbl[2] = {
    { (u32)D_253A434, 0 },
    { (u32)D_253A440, (u32)food_venues_meals },
};
const s8 D_253A464[22] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
};
const struct Inventory food_fridge_inventory = { D_253A434, D_253A464, 22 };
const u8 D_253A488[] = _("パーティごはん");
const u8 D_253A498[] = _("バースデーケーキ");
const u8 D_253A4AC[] = _("たべる?");
const u8 D_253A4B8[] = _("ごはん");
const u8 D_253A4C0[] = _("おやつ");
const struct LabelPair food_meal_snack_labels = { (u32)D_253A4B8, (u32)D_253A4C0 };

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
struct PartyState food_party_state DSTRAM;
struct PartyInfo food_birthday_info DSTRAM;
