/*
 * Events of an idle touch or Touch Spot: the traveller and his souvenir, the fortune cookie,
 * random idle scenes, profile talk, phrases, and the score rewards.
 *
 * Code 250ECDC..250F122, .rodata from 255C8CC.
 */
#include "proto/spot/spot_events.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/anim/anim_scripts.h"
#include "proto/ui/ui_text_banner.h"
#include "proto/ui/ui_message.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/owner/owner_profile.h"
#include "proto/spot/spot_reader.h"
#include "proto/comm/comm_outcome.h"
#include "flash.h"
#include "libc.h"

/*
 * Communication counter pet.comm_count_mod10 (9 -> pet.comm_traveller_due = 1, wraps after 9).
 */
void comm_count_tenth(void)
{
    save.pet.comm_count_mod10++;
    if (save.pet.comm_count_mod10 == 9)
        save.pet.comm_traveller_due = 1;
    else if (save.pet.comm_count_mod10 > 9)
        save.pet.comm_count_mod10 = 0;
}

/*
 * The traveller's souvenirs 0x29..0x2C (エッフェルとう, まほうのランプ, じゆうのめがみ,
 * こおりのちょうこく) are kept as the flags 0x19..0x1C instead of items: set the flag and
 * return 1; 0 for any other item.
 */
u8 item_souvenir_flag(u8 k)
{
    u8 r = 1;

    switch (k) {
    case 0x29:
        pet_set_unlock_flag(0x19, 1);
        break;
    case 0x2a:
        pet_set_unlock_flag(0x1a, 1);
        break;
    case 0x2b:
        pet_set_unlock_flag(0x1b, 1);
        break;
    case 0x2c:
        pet_set_unlock_flag(0x1c, 1);
        break;
    default:
        r = 0;
        break;
    }
    return r;
}

/*
 * The traveller's visit (an idle-touch event, forced every 10th communication by
 * pet.comm_traveller_due): text 100/0xE0 ("グッドアフタヌーン! これはおみやげ! ...
 * ボン・ボヤージュ★") and one of six souvenirs D_255C8D0 (a flag, item_souvenir_flag, or an
 * item), +1 happiness.
 */
void spot_traveller_souvenir(void)
{
    u32 buf[3];
    u8 k;
    struct TamagoFile **pp;
    int r;

    anim_traveller_arrive();
    ui_show_message(0x21, 0, chara_text(0, 100, 0xe0));
    r = util_rand_except(100, 100);
    k = D_255C8D0[r % 6];
    pp = &item_builtin_records[k];
    item_load_pictures((void *)buf, *pp, 3);
    anim_traveller_gift(buf[0]);
    if (item_souvenir_flag(k) == 0)
        item_add_to_inventory(*pp);
    anim_happy();
    pet_add_happiness(1);
    save.pet.comm_traveller_due = 0;
    save_after_sound();
}

/*
 * A random fortune line 1..99 of text group 0x69 ("ミラクルハッピー! なんでもチャレンジ" ...)
 * with the decorated text animation ui_text_banner.
 */
void spot_fortune_line(void)
{
    ui_text_banner(chara_text(0, 0x69, util_rand_except(99, 99) + 1));
}

/*
 * The fortune event (idle touch on days 9 / 19 / 29, and the Touch Spot reader): after a
 * key press a fortune line (spot_fortune_line), or with 8 % (v 51..58) the jackpot
 * "★おおあたり★" (anim_fortune_jackpot) with the item うらないクッキー (item_fortune_cookie);
 * +1 happiness.
 */
void spot_fortune_event(void)
{
    u32 buf[3];
    int v;
    int f;

    v = util_rand_except(100, 100);
    f = 0;
    if (v >= 0x33 && v <= 0x3a)
        f = 1;
    if (f != 1) {
        anim_fortune_start();
        anim_fortune_press();
        ui_wait_ticks(10);
        if (btn_a_presses + btn_b_presses + btn_c_presses != 0) {
            anim_fortune_open();
            spot_fortune_line();
            ui_wait_ticks(10);
            anim_happy();
            pet_add_happiness(1);
            save_after_sound();
        }
    } else {
        anim_fortune_start();
        anim_fortune_press();
        ui_wait_ticks(10);
        if (btn_a_presses + btn_b_presses + btn_c_presses != 0) {
            anim_fortune_open();
            item_load_pictures((void *)buf, (struct TamagoFile *)item_fortune_cookie, 3);
            anim_fortune_jackpot(buf[0]);
            item_add_to_inventory((struct TamagoFile *)item_fortune_cookie);
            anim_happy();
            pet_add_happiness(1);
            save_after_sound();
        }
    }
}

/*
 * Random idle animation among 5 (anim_idle_random_table), then the default end animation.
 */
void pet_event_idle_random(void)
{
    void (*fn[5])(void);

    memcpy(fn, anim_idle_random_table, 0x14);
    fn[rand() % 5]();
    anim_happy();
}

const u8 D_255C8D0[6] = { 0x29, 0x2a, 0x2b, 0x2c, 0x7d, 0x0b };
void (*const anim_idle_random_table[5])(void) = {
    anim_idle_random_0, anim_idle_random_1, anim_idle_random_2, anim_idle_random_3, anim_idle_random_4,
};
const u8 D_255C8EC[] = _(" ");
const u8 D_255C8F0[] = _("あなたのとくいなもの､みてみたいです@");
const u8 D_255C918[] = _("なりたいものになれるといいね♪");
const u8 D_255C938[] = _("さいきんきになるたべものはなんですか?");
const u8 D_255C960[] = _("こっそりすきなひとをおしえて!");

/*
 * The pet repeats one of the owner's five profile strings (pet.owner_profile_texts + k * 16)
 * inside a sentence of its own text groups 6..9, or asks one of the four default questions
 * below when that string is blank.
 */
void spot_profile_talk(void)
{
    u8 buf[20];
    u8 out[168];
    const u8 *s;
    int i;
    u8 k;

    s = D_255C8EC;
    k = rand() % 5;
    memset(out, 0, 0xa5);
    for (i = 0; i < 13; i++)
        strcat((char *)out, (char *)s);
    memset(buf, 0, 0x11);
    memcpy(buf, save.pet.owner_profile_texts + k * 16, 16);
    for (i = 14; i >= 0; i -= 2) {
        if (memcmp(buf + i, s, 2) != 0)
            break;
        buf[i] = 0;
    }
    if (strlen((char *)buf) != 0) {
        strcat((char *)out, (char *)buf);
        switch (k) {
        case 0:
        case 4:
            strcat((char *)out, (char *)chara_text(save.pet.x000, 9, 0));
            break;
        case 1:
            strcat((char *)out, (char *)chara_text(save.pet.x000, 8, 0));
            break;
        case 2:
            strcat((char *)out, (char *)chara_text(save.pet.x000, 7, 0));
            break;
        case 3:
            strcat((char *)out, (char *)chara_text(save.pet.x000, 6, 0));
            break;
        }
    } else {
        switch (k) {
        case 0:
        case 4:
            strcat((char *)out, (char *)D_255C8F0);
            break;
        case 1:
            strcat((char *)out, (char *)D_255C918);
            break;
        case 2:
            strcat((char *)out, (char *)D_255C938);
            break;
        case 3:
            strcat((char *)out, (char *)D_255C960);
            break;
        }
    }
    strcat((char *)out, (char *)s);
    ui_show_message(2, 0, out);
    anim_happy();
}

/*
 * Random phrase from the 25-entry table of the pet's kind (pet_event_phrase_lines) shown with an
 * animation.
 */
void pet_event_phrase(void)
{
    u16 tbl[50];
    int n;
    u8 i;
    u32 s0, s1;

    memcpy(tbl, pet_event_phrase_lines, 100);
    n = pet_get_gender() ? 25 : 0;
    i = n + rand() % 25;
    switch (tbl[i]) {
    case 12:
        chara_select_other_by_id(0x10);
        break;
    case 15:
        chara_select_other_by_id(0x13);
        break;
    case 16:
        chara_select_other_by_id(0x14);
        break;
    case 22:
        chara_select_other_by_id(0x3ed);
        break;
    default:
        chara_other_sprite_set = tbl[i];
        chara_other_row = 0;
        break;
    }
    s0 = anim_pic_slots[0][0];
    s1 = anim_pic_slots[0][1];
    anim_pic_slots[0][0] = D_2154050;
    anim_pic_slots[0][1] = D_215D918;
    anim_phrase_guest();
    ui_show_message(0xc, 0, chara_text(0, 0x65, i));
    anim_phrase_guest_end();
    anim_pic_slots[0][0] = (const u8 *)s0;
    anim_pic_slots[0][1] = (const u8 *)s1;
    anim_happy();
}

const u16 pet_event_phrase_lines[2][25] = {
    { 94, 94, 94, 58, 58, 48, 49, 50, 45, 45, 45, 67, 93, 69, 22, 22, 83, 75, 73, 45,
      69, 77, 73, 45, 69 },
    { 94, 94, 94, 58, 58, 58, 46, 47, 45, 45, 45, 61, 75, 78, 16, 71, 12, 12, 80, 75,
      84, 15, 72, 45, 45 },
};

/*
 * Reward for a score a (e.g. game/job result: > 199 -> 300 points and a prize item
 * item_builtin_records[18], ...).
 */
void game_score_reward(int a)
{
    u32 kept[15];
    u16 v;
    u8 k;
    struct TamagoFile *p;

    if (a > 0) {
        if (a > 199) {
            k = 3;
            v = 300;
            p = item_builtin_records[18];
            pet_set_unlock_flag(0x17, 1);
        } else if (a > 129) {
            k = 2;
            v = 300;
            p = item_builtin_records[19];
            pet_set_unlock_flag(0x18, 1);
        } else if (a > 49) {
            k = 1;
            v = 200;
            p = item_builtin_records[19];
            pet_set_unlock_flag(0x18, 1);
        } else {
            k = 1;
            v = 50;
            p = item_builtin_records[20];
        }
        anim_points_gain(v);
        memcpy(kept, anim_pic_slots, 0x3c);
        item_load_pictures(anim_pic_slots[0], p, 15);
        anim_receive_item();
        memcpy(anim_pic_slots, kept, 0x3c);
        if (v == 50)
            item_add_to_inventory(p);
        pet_add_happiness(k);
        pet_add_points(v);
        save_game();
        anim_happy();
    } else if (a == 0) {
        anim_sad();
    }
}
