/*
 * The outcome of a communication: greeting (profile card, fortune), gift, proposal / marriage,
 * app, game rounds, photo; the idle-touch event; friend book update; and the flash helpers used
 * while building records.
 *
 * Code 2527918..25294CE, .rodata from 2562140.
 */
#include "proto/comm/comm_outcome.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/town/town_places.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/evo/evo_counters.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_message.h"
#include "proto/comm/comm_record_pics.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/comm/comm_flash_buffers.h"
#include "proto/ir/ir_packets.h"
#include "proto/ir/ir_driver.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/test/test_debug_chara.h"
#include "proto/spot/spot_events.h"
#include "proto/pet/pet_care.h"
#include "proto/spot/spot_reader.h"
#include "proto/game/game_timing.h"
#include "proto/game/game_builtin.h"
#include "proto/comm/comm_checks.h"
#include "proto/nfc/nfc_session.h"
#include "proto/comm/comm_exchange.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

struct ItemKey { u8 category; u8 subkind; u16 item_id; u32 d; };
struct BuiltinItemKeyEntry { u8 category; u8 subkind; u16 item_id; struct TamagoFile *record; };
struct BuiltinItemRecEntry { u8 *key_record; struct TamagoFile *record; };
int item_find_builtin_match(int a, struct TamagoFile **out)
{
    u8 *p = (u8 *)a;
    struct ItemKey k;
    struct BuiltinItemKeyEntry *t1;
    struct BuiltinItemRecEntry *t2;
    u8 *r;
    u8 *q;
    int i;

    k.category = p[0x4f];
    k.subkind = p[0x51];
    k.item_id = (p[0x5a] << 8) + p[0x5b];
    t1 = (struct BuiltinItemKeyEntry *)0x23d8cb0;
    for (i = 0; i <= 63; i++) {
        if (t1[i].category == k.category) {
            if (t1[i].subkind == k.subkind) {
                if (t1[i].item_id == k.item_id) {
                    *out = t1[i].record;
                    return 1;
                }
            }
        }
    }
    t2 = (struct BuiltinItemRecEntry *)0x23d8eb0;
    q = p + 0x5a;
    for (i = 0; i <= 63; i++) {
        r = t2[i].key_record;
        if (r[0x4f] == k.category) {
            if (r[0x51] == k.subkind) {
                if (*q == r[0x5a]) {
                    if (p[0x5b] == r[0x5b]) {
                        *out = t2[i].record;
                        return 1;
                    }
                }
            }
        }
    }
    t2 = (struct BuiltinItemRecEntry *)0x23d90b0;
    q = p + 0x5a;
    for (i = 0; i <= 63; i++) {
        r = t2[i].key_record;
        if (r[0x4f] == k.category) {
            if (r[0x51] == k.subkind) {
                if (*q == r[0x5a]) {
                    if (p[0x5b] == r[0x5b]) {
                        *out = t2[i].record;
                        return 1;
                    }
                }
            }
        }
    }
    t2 = (struct BuiltinItemRecEntry *)0x23d90b0;
    q = p + 0x5a;
    for (i = 0; i <= 63; i++) {
        r = (u8 *)t2[i].record;
        if (r[0x4f] == k.category) {
            if (r[0x51] == k.subkind) {
                if (*q == r[0x5a]) {
                    if (p[0x5b] == r[0x5b]) {
                        *out = r;
                        return 2;
                    }
                }
            }
        }
    }
    return 0;
}

/*
 * F66 = what happens AFTER a Tamagotchi-to-Tamagotchi NFC exchange (the records are built
 * and swapped by F70b): show the partner, apply the result of each session type
 * (nfc_session.x003), update the friend book (ともだちてちょう), store received items, save.
 * Also the small flash / button / friend-list helpers used by F70b.
 *
 * comm_take_gift_item, session result (item gift): take the item carried by exchange record x
 * (section 3, comm_attached_file) into our inventory: item_find_builtin_match must accept it (1), its
 * images are loaded (item_load_pictures -> anim_pic_slots) and shown, it is stored with item_add_to_inventory
 * (a failure shows the inventory-full flow), then pet_add_happiness(1) (+1 happiness).
 * comm_storing_gift = 1 while storing.
 */
u8 comm_take_gift_item(struct ExchangeRecord *x)
{
    struct TamagoFile *p;
    u8 r;

    r = item_find_builtin_match((int)comm_attached_file(x), &p);
    if (r != 1)
        return 0;
    item_load_pictures((void *)anim_pic_slots, p, 15);
    anim_comm_item_received(anim_pic_slots[0][0]);
    comm_exchange_kind = 0;
    btn_unmask(7);
    comm_storing_gift = r;
    if (item_add_to_inventory(p) != 0) {
        comm_storing_gift = 0;
        anim_clear_pic_slots2();
        anim_happy();
    }
    comm_storing_gift = 0;
    pet_add_happiness(1);
    return 1;
}

/*
 * Communication message screens k: 0 = "あなたのなまえが とうろくされたよ" (your name has
 * been registered), 1 = "きせつのよていが いっぱいだよ! けしたいよていを えらんでね" (the
 * seasonal schedule is full, choose one to delete).
 */
void comm_show_msg(int k)
{
    switch (k) {
    case 0:
        ui_message_box(D_2562148, D_256215C, D_2562144, D_2562144, 16);
        break;
    case 1:
        ui_message_box(D_2562170, D_2562184, D_2562194, D_25621A8, 0);
        break;
    }
}

/*
 * Extract the partner's profile from exchange record b: its 16-byte owner name (0x60)
 * into a, and the first 4 bytes of the profile section (directory entry 0xE4 + 8 = the
 * owner_profile_bytes bytes: [0] gender 0..2, [1] mood 0..6, [2] age? 0..99, [3] 0..12, unclear:
 * birth month or star sign) into out, each set to 0 when out of range.
 */
void comm_get_profile(u8 *out, void *a, u8 *b)
{
    u8 tmp[4];

    memcpy(a, b + 0x60, 16);
    memcpy(tmp, b + *(u16 *)(b + 0xe4) + 8, 4);
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
    out[3] = tmp[3];
    if (out[0] > 2)
        out[0] = 0;
    if (out[1] > 6)
        out[1] = 0;
    if (out[2] > 99)
        out[2] = 0;
    if (out[3] > 12)
        out[3] = 0;
}

/*
 * Same as comm_get_profile for the peer's RAM record comm_rx_record (greeting mode).
 */
void comm_get_peer_profile(u8 *out, void *a)
{
    u8 tmp[4];
    u8 *p;

    memcpy(a, comm_rx_record + 0x60, 16);
    p = comm_rx_record;
    memcpy(tmp, p + *(u16 *)(p + 0xe4) + 8, 4);
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
    out[3] = tmp[3];
    if (out[0] > 2)
        out[0] = 0;
    if (out[1] > 6)
        out[1] = 0;
    if (out[2] > 99)
        out[2] = 0;
    if (out[3] > 12)
        out[3] = 0;
}

/*
 * Gender text k into dst: 1 = おんなのこ (girl), 2 = おとこのこ (boy), else blank.
 */
void owner_gender_text(u8 k, char *dst)
{
    const char *s;

    if (k == 1)
        s = D_25621B4;
    else if (k == 2)
        s = D_25621C0;
    else
        s = D_25621CC;
    strcpy(dst, s);
}

/*
 * The mood icon image for mood k (table owner_mood_icons of 7 pointers); 0 for k > 6.
 */
int owner_mood_icon(u8 k)
{
    int t[7];

    memcpy(t, owner_mood_icons, 0x1c);
    if (k > 6)
        return 0;
    return t[k];
}

const u8 D_2562144[] = _("");
const u8 D_2562148[] = _("あなたのなまえが");
const u8 D_256215C[] = _("とうろくされたよ");
const u8 D_2562170[] = _("きせつのよていが");
const u8 D_2562184[] = _("いっぱいだよ!");
const u8 D_2562194[] = _("けしたいよていを");
const u8 D_25621A8[] = _("えらんでね");
const char D_25621B4[] = _("おんなのこ");
const char D_25621C0[] = _("おとこのこ");
const char D_25621CC[] = _(" ");
extern u8 D_20F37D0[];
extern u8 D_20F3A68[];
extern u8 D_20F3D00[];
extern u8 D_20F3F98[];
extern u8 D_20F4230[];
extern u8 D_20F29F0[];
const u8 *const owner_mood_icons[7] = {
    D_20F37D0, D_20F37D0, D_20F3A68, D_20F3D00, D_20F3F98, D_20F4230, D_20F29F0,
};
const char D_25621EC[] = _("さいこー!");
const char D_25621F8[] = _("いいかんじ♪");
const char D_2562208[] = _("やるきまんまん");
const char D_2562218[] = _("ふつう");
const char D_2562220[] = _("いまいち");
const char D_256222C[] = _("しょんぼり");

/*
 * Mood label k into dst: 1..6 = さいこー! / いいかんじ♪ / やるきまんまん / ふつう / いまいち /
 * しょんぼり, else blank (not a star sign).
 */
void owner_mood_text(u8 k, char *dst)
{
    const char *s;

    switch (k) {
    case 1:
        s = D_25621EC;
        break;
    case 2:
        s = D_25621F8;
        break;
    case 3:
        s = D_2562208;
        break;
    case 4:
        s = D_2562218;
        break;
    case 5:
        s = D_2562220;
        break;
    case 6:
        s = D_256222C;
        break;
    default:
        s = D_25621CC;
        break;
    }
    strcpy(dst, s);
}

/*
 * Profile exchange result: draw the partner's profile card from the greeting record
 * (comm_get_peer_profile into a): frame (a plain one when gender and mood are both 0), mood icon,
 * name, and unless empty the gender and mood labels (age and the 4th byte are not shown).
 */
void comm_draw_profile_card(struct OwnerProfile *a)
{
    u8 b18[18];
    char b17[17];
    u8 b11[11];
    u8 flag[1];
    u8 *p;
    char *s;

    memset(b18, 0, 0x12);
    memset(a, 0, 4);
    comm_get_peer_profile((u8 *)a, b18);
    flag[0] = (a->gender == 0 && a->mood == 0);
    ui_new_screen();
    memset(b11, 0, 0xb);
    b11[0] = gfx_object_create(0, 0x1c, 0, 0, 0, (const u8 *)(flag[0] ? 0x20fd2dc : 0x2104794));
    p = b11;
    p[2] = gfx_object_create(0x4d, 0x38, 0x30, 0, 0, (const u8 *)owner_mood_icon(a->mood));
    s = b17;
    memset(s, 0, 0x11);
    memcpy(s, b18, 0x10);
    p[3] = gfx_object_create(0, 0, 0x40, 0, 0, 0);
    gfx_draw_text_line(p[3], 9, 0x23, 0x40, (int)s, 0, 8);
    if (!flag[0]) {
        memset(s, 0, 0x11);
        owner_gender_text(a->gender, s);
        p[4] = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_line(p[4], 9, 0x38, 0x40, (int)s, 0, 5);
        memset(s, 0, 0x11);
        owner_mood_text(a->mood, s);
        p[5] = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_line(p[5], 9, 0x4d, 0x40, (int)s, 1, 7);
    }
    func_00009A6();
}

/*
 * Communication info screen a: 0 = "ともだちてちょうを こうしんしたよ!" (friend book
 * updated), 1 = "うらないちゅう…" (fortune in progress, sound 0x13).
 */
void comm_info_screen(int a)
{
    switch (a) {
    case 0:
        anim_scene_room(3);
        anim_draw_pic(0, D_2104794, 0, 0x40, 0x24);
        anim_draw_text(1, D_2562254, 0, 0x10, 0x1c);
        anim_draw_text(2, D_2562268, 0, 0x16, 0x2c);
        func_00009A6();
        comm_friendbook_updated = 0;
        break;
    case 1:
        anim_scene_room(3);
        anim_draw_pic(0, D_2104794, 0, 0x40, 0x24);
        anim_draw_pic(1, D_215D6F8, 0, 0x64, 0x35);
        anim_draw_text(2, D_256227C, 0, 0x16, 0x18);
        anim_queue_sound(0x13);
        func_00009A6();
        break;
    }
}

/*
 * Two-choice dialog after a profile exchange: "あいしょうを うらなう? はい / いいえ" (tell
 * the compatibility fortune?): 0 yes, 1 no, 2 cancelled, 3 timeout.
 */
s8 comm_ask_fortune(void)
{
    int x = 0;

    switch ((u32)ui_question_two_lines(D_2562290, D_25622A0, D_25622AC, D_25622B4, 16, &x)) {
    case 1:
        switch (x) {
        case 0:
            return 0;
        case 1:
            return 1;
        }
        break;
    case 2:
        return 2;
    case 3:
    default:
        return 3;
    }
}

/*
 * "Numerology" for the compatibility fortune, for our record (a == 0, comm_tx_record) or the
 * partner's (comm_rx_record): count the name characters (pairs whose second byte is not 0x6C =
 * blank) + gender + (mood - 1), reduce to one digit by repeated digit sums -> *out
 * (pet.fortune_name_len..D_0001227 keep the intermediate values).
 */
void comm_fortune_number(u8 a, struct OwnerProfile *p, u8 *out)
{
    u8 buf[18];
    u16 n;
    int i;

    *out = 0;
    memset(buf, 0, 0x12);
    memset(p, 0, 4);
    if (a == 0)
        comm_get_profile((u8 *)p, buf, (u8 *)comm_tx_record);
    else
        comm_get_profile((u8 *)p, buf, comm_rx_record);
    n = 0;
    for (i = 0; i < 16; i += 2) {
        if (buf[i + 1] != 0x6c && buf[i] != 0)
            n++;
    }
    save.pet.fortune_name_len = n;
    switch (p->gender) {
    case 0: n = 0; break;
    case 1: n = 1; break;
    case 2: n = 2; break;
    default: n = 0; break;
    }
    save.pet.fortune_gender = n;
    n = p->mood ? p->mood - 1 : 0;
    save.pet.fortune_mood = n;
    n = save.pet.fortune_name_len + save.pet.fortune_gender + save.pet.fortune_mood;
    for (;;) {
        if (n <= 9)
            break;
        n = n / 10 + n % 10;
        save.pet.fortune_digit = n;
    }
    save.pet.fortune_digit = n;
    *out = n;
}

const u8 D_2562254[] = _("ともだちてちょうを");
const u8 D_2562268[] = _("こうしんしたよ!");
const u8 D_256227C[] = _("うらないちゅう…");
const u8 D_2562290[] = _("あいしょうを");
const u8 D_25622A0[] = _("うらなう?");
const u8 D_25622AC[] = _("はい");
const u8 D_25622B4[] = _("いいえ");
const u8 D_25622BC[] = _("…かも!");


/*
 * Show the compatibility fortune result: the digit difference a (0..9; 0, 1-2, 3-4, 5-6,
 * 7, 8+) picks one of 6 texts of text group 0x66 (chara_text), shown as two lines
 * followed by "…かも!" ("...maybe!") over fixed pictures.
 */
void comm_show_fortune(u8 a)
{
    int n;
    u8 *p;

    n = 1;
    switch (a) {
    case 0: break;
    case 1: case 2: n += 1; break;
    case 3: case 4: n += 2; break;
    case 5: case 6: n += 3; break;
    case 7: n += 4; break;
    case 8:
    default: n += 5; break;
    }
    p = chara_text(0, 0x66, n);
    anim_scene_room(9);
    anim_draw_pic(0, D_2104794, 0, 0x40, 0x24);
    anim_draw_pic(1, D_214E930, 0, 0x49, 0x33);
    anim_draw_pic(2, D_215D4D8, 0, 0x64, 0x35);
    p = (u8 *)anim_draw_text(3, p, 0, 0x10, 0xc);
    anim_draw_text(4, p, 0, 0x10, 0x1c);
    anim_draw_text(5, D_25622BC, 0, 0x10, 0x2c);
    func_00009A6();
}

/*
 * Session type 2 (greeting / profile exchange) outcome: show the partner's profile card
 * for 10 ticks of 100 ms (ui_wait_ticks_hold_a, restarted while A is held), unless cancelled (C or
 * timeout) offer the compatibility fortune (both numerology digits, their difference ->
 * comm_show_fortune), clear the mail queue comm_mail_queue, comm_friendbook_updated = 2, update the friend book
 * (friend_book_update).
 */
void comm_outcome_greeting(void)
{
    struct OwnerProfile a[2];
    struct OwnerProfile b;
    u8 c, d;
    s8 r;
    u8 diff;
    int i;

    comm_draw_profile_card(a);
    ui_wait_ticks_hold_a(10);
    if (btn_c_presses == 0 && ui_timeout != 0) {
        r = comm_ask_fortune();
        if (r == 0) {
            comm_info_screen(1);
            comm_fortune_number(r, &b, &c);
            comm_fortune_number(1, &b, &d);
            if (d > c)
                diff = d - c;
            else
                diff = c - d;
            ui_wait_ticks(3);
            comm_show_fortune(diff);
            ui_wait_ticks_hold_a(10);
        }
    }
    for (i = 0; i <= 2; i++)
        ((u8 *)&comm_mail_queue)[i] = 0xff;
    comm_friendbook_updated = 2;
    friend_book_update();
    ui_wait_ticks(3);
}


/*
 * comm_load_section_pics(dst, list, n) = collect up to n picture pointers from a rendered-image
 * section into the game variable table dst (list: u16 marker (0xFFFF = none), u16, u16
 * count, then count x {u16 length, bitmap}); dst[i] = bitmap pointer, the rest 0; returns
 * the count. This is how the partner's rendered poses (section 1 at directory entry 0xE2)
 * become anim_pic_slots (asm).
 */
int comm_load_section_pics(u32 *dst, u8 *list, int n)
{
    int cnt, i;
    u16 len;

    if (list == 0)
        return 0;
    if (*(u16 *)list == 0xffff)
        return 0;
    cnt = *(u16 *)(list + 4);
    list += 6;
    if (cnt > n)
        cnt = n;
    for (i = 0; i < cnt; i++) {
        len = *(u16 *)list;
        list += 2;
        dst[i] = (u32)list;
        list += len;
    }
    for (; i < n; i++)
        dst[i] = 0;
    return cnt;
}

/*
 * Session type 5 (gift / item exchange) outcome. Giver (initiator): the partner's pictures
 * from the received section at entry 0xE2 (comm_load_section_pics), "give" animation
 * (anim_gift_given), every 5th gift (pet.gift_count) a 500-point bonus (anim_points_gain shows it,
 * pet_add_points adds it), exchange kind comm_exchange_kind = 4 (giving side, the value the IR code
 * uses), comm_item_exchange_result(4, ...) marks our item as given, and if the partner sent something
 * back comm_take_gift_item stores it (else a plain ending animation). Receiver (the idle toy):
 * wait the sync delay (header 0x76, in seconds: tmr_t5_wait_100ms(x76 * 10)), "receive"
 * animation, comm_exchange_kind = 5 (receiving side) while comm_item_exchange_result(5, ...) stores the item,
 * then 0. Then friend book (friend_book_update) and mail handling (comm_store_peer_mail).
 */
void comm_outcome_gift(void)
{
    int r;
    u8 v;

    comm_rx_record = (u8 *)&comm_rx_phase2;
    if (nfc_session.x002 == 1) {
        comm_load_section_pics(anim_pic_slots, (u8 *)&comm_rx_phase2 + comm_rx_phase2.sect_poses, 3);
        anim_gift_given();
        if (++save.pet.gift_count > 4) {
            anim_clear_pic_slots2();
            anim_gift_bonus();
            anim_points_gain(500);
            pet_add_points(500);
            save.pet.gift_count = 0;
        }
        comm_is_initiator = 1;
        comm_exchange_kind = 4;
        comm_item_exchange_result(4, (struct ExchangeRecord *)(FLASH_COMM_TX + 0x800), &comm_rx_phase2);
        v = comm_take_gift_item((struct ExchangeRecord *)(FLASH_COMM_TX + 0x800));
        if (v == 0) {
            *(u8 *)&comm_exchange_kind = v;
            anim_clear_pic_slots2();
            anim_happy();
            pet_add_happiness(1);
        }
    } else {
        if (*(u16 *)(comm_rx_record + 0x76) != 0) {
            anim_comm_done();
            tmr_t5_wait_100ms(*(u16 *)(comm_rx_record + 0x76) * 10);
        }
        pet_add_happiness(1);
        nfc_session.item_refused = 0;
        comm_is_initiator = 0;
        comm_exchange_kind = 5;
        r = comm_item_exchange_result(5, (struct ExchangeRecord *)(FLASH_COMM_TX + 0x800), &comm_rx_phase2);
        if (r != 0)
            r = 1;
        comm_exchange_kind = 0;
        if (r == 1 && nfc_session.item_refused == 0)
            anim_happy();
    }
    friend_book_update();
    comm_store_peer_mail();
}

/*
 * Marriage happened (end of the session type 6 outcome): reset the character's life
 * timers / counters (x0bc, x01c, x01e, pet_poops_drawn, pet.hunger_loss_parity), roll the next generation's
 * traits (pet_pick_variant_flag: 1 or 3 by the x004 flag and chance, pet_start_next_generation), ui_main_icon = 0,
 * anim_new_egg, comm_book_notice_mode = 1 = "new generation pending" (a new egg) (MED).
 */
void evo_after_marriage(void)
{
    save.pet.living_dirt = 0;
    save.pet.poop_count = 0;
    pet_poops_drawn = 0;
    save.pet.toilet_timer = 0;
    save.pet.hunger_loss_parity = 0;
    pet_pick_variant_flag();
    pet_start_next_generation();
    ui_main_icon = 0;
    anim_new_egg();
    save.comm_book_notice_mode = 1;
}

/*
 * Proposal refusal screens k: 0 = "まだプロポーズ できないよ!" (cannot propose yet), 1 =
 * "このともだちには プロポーズ できないよ!" (not to this friend), 2 = "このともだちからは
 * プロポーズ できないよ" (not from this friend).
 */
void comm_proposal_refused_msg(int k)
{
    switch (k) {
    case 0:
        ui_message_box(D_25622EC, D_25622FC, D_2562144, D_2562144, 16);
        break;
    case 1:
        ui_message_box(D_256230C, D_2562334, D_25622FC, 0, 8);
        break;
    case 2:
        ui_message_box(D_2562320, D_2562334, D_25622FC, 0, 8);
        break;
    }
}

extern const u16 comm_forbidden_pairs[][2];


/*
 * comm_can_marry() = may the two characters marry? (session type 6, called by
 * comm_outcome_marriage; not a photo check.) Compares our phase-2 record (0x27C0800) with the
 * partner's (0x27D0800): same gender byte 0x5F (pet_get_gender) -> refusal by role (the
 * accepting initiator gets 2 "このともだちからは", the proposing target 1
 * "このともだちには"); either life stage 0x7B <= 2 -> refusal 0 (not adult); the
 * partner's 0x86 != 0 -> refused; our species group 0x88 paired with theirs in the table
 * comm_forbidden_pairs (symmetric pairs {0x10, 0x1B59}, {0x11, 0x1B5A}, ..., {0x3EB, 0x138B},
 * {0x15, 0x3EB}: a character and its downloadable twin, probably) -> refused (2 for the
 * initiator, 1 for the target). Returns 2 = refused (message shown with comm_proposal_refused_msg),
 * 0 = ok.
 */
u8 comm_can_marry(void)
{
    u16 list[21];
    int i;
    int r;
    u8 n;

    for (i = 0; i <= 20; i++)
        list[i] = 0;
    comm_tx_record = (FLASH_COMM_TX + 0x800);
    comm_rx_record = (u8 *)&comm_rx_phase2;
    if (comm_rx_phase2.gender == *(u8 *)(FLASH_COMM_TX + 0x85F)) {
        if (nfc_session.x002 == 1)
            comm_proposal_refused_msg(2);
        else
            comm_proposal_refused_msg(1);
        return 2;
    }
    if (comm_rx_record[0x7b] <= 2 || ((u8 *)comm_tx_record)[0x7b] <= 2) {
        comm_proposal_refused_msg(0);
        return 2;
    }
    n = 0;
    for (i = 0; i <= 20; i++) {
        if (*(u16 *)((u8 *)comm_tx_record + 0x88) == comm_forbidden_pairs[i][0]) {
            list[n] = comm_forbidden_pairs[i][1];
            n++;
        }
    }
    r = 0;
    if (comm_rx_record[0x86] != 0)
        r = 2;
    if (r == 0 && n != 0) {
        for (i = 0; i < n; i++) {
            if (*(u16 *)(comm_rx_record + 0x88) == list[i]) {
                r = 2;
                break;
            }
        }
    }
    if (r == 2) {
        if (nfc_session.x002 == 1)
            comm_proposal_refused_msg(r);
        else
            comm_proposal_refused_msg(1);
    }
    return r;
}

/*
 * Session type 6 (proposal / marriage) outcome (nfc_session.x003 == 6 in comm_run_outcome;
 * reached from menu choice 5 only): if comm_can_marry allows it, the accepting side (role 1
 * = initiator, うける) shows the partner's poses, the proposal animation (anim_proposal),
 * the proposer's proposal text (section 5 at directory entry 0xEA, via
 * ui_show_message(0xB, ...)) and anim_wedding_couple; the proposing side (role 2 = target, する)
 * waits the sync delay and plays anim_just_married. Both then run pet_life_story and
 * evo_after_marriage (new generation).
 */
void comm_outcome_marriage(void)
{
    u8 *q;

    if (comm_can_marry() != 2) {
        comm_rx_record = (u8 *)&comm_rx_phase2;
        if (nfc_session.x002 == 1) {
            q = (u8 *)&comm_rx_phase2 + comm_rx_phase2.sect_poses;
            comm_load_section_pics(anim_pic_slots, q, 3);
            anim_proposal();
            q = (u8 *)&comm_rx_phase2 + *(u16 *)(comm_rx_record + 0xea);
            ui_show_message(0xb, 0, q + 8);
            anim_wedding_couple();
        } else {
            if (*(u16 *)(comm_rx_record + 0x76) != 0) {
                anim_comm_done();
                tmr_t5_wait_100ms(*(u16 *)(comm_rx_record + 0x76) * 10);
            }
            q = (u8 *)&comm_rx_phase2;
            q += *(u16 *)(comm_rx_record + 0xe2);
            comm_load_section_pics(anim_pic_slots, q, 3);
            anim_just_married();
        }
        pet_life_story();
        evo_after_marriage();
    }
}

/*
 * Session type 8 (app download link) outcome on the target, i.e. the toy in "4Uアプリ"
 * (menu choice 6): store the item of the received record (comm_record_item -> item_receive,
 * any kind of item, apps included, not only lottery / stamp items), with the "name
 * registered" message (comm_show_msg(0)) first when the record version 0x84 is 3.
 */
void comm_outcome_app(void)
{
    if (comm_rx_phase2.version == 3)
        comm_show_msg(0);
    item_receive((struct TamagoFile *)comm_record_item(&comm_rx_phase2));
    pet_add_happiness(1);
    anim_happy();
}

/*
 * "もういちど つうしんするよ" (communicating once more) screen (D_2562394 / D_25623A0),
 * for k == 0 only.
 */
void comm_retry_msg(u8 k)
{
    if (k == 0) {
        anim_scene_room(3);
        anim_draw_pic(0, D_2104794, 0, 0x40, 0x24);
        anim_draw_text(1, D_2562394, 0, 0x28, 0x1c);
        anim_draw_text(2, D_25623A0, 0, 0x1c, 0x2c);
        func_00009A6();
    }
}

/*
 * Session type 3 (communication / connection game, round 1) outcome: each side plays the
 * mini game (game_timing_round: intro animations + game_timing_play; the initiator with the
 * partner's poses, the target after the sync delay) and keeps its score in
 * nfc_session.score_initiator (initiator) / x01c (target); the scores are exchanged in header field
 * 0xB0 of the next session (type 4, comm_outcome_game2).
 */
void comm_outcome_game1(void)
{
    u8 m;
    int r;

    comm_rx_record = (u8 *)&comm_rx_phase2;
    m = nfc_session.x002;
    if (m == 1) {
        comm_load_section_pics(anim_pic_slots, comm_rx_phase2.sect_poses + (u8 *)&comm_rx_phase2, 3);
        nfc_session.score_initiator = game_timing_round(m);
    } else {
        if (*(u16 *)(comm_rx_record + 0x76) != 0) {
            anim_comm_done();
            tmr_t5_wait_100ms(*(u16 *)(comm_rx_record + 0x76) * 10);
            anim_good_result();
            r = game_timing_round(0);
        } else
            r = game_timing_round(0);
        nfc_session.score_target = r;
    }
}


/*
 * Session type 4 (communication game, round 2; dispatched for nfc_session.x003 == 4, not 6)
 * outcome: both scores (ours from our record 0xB0, theirs from the received 0xB0) are
 * summed into x01e and game_timing_result_anim(role, total) shows the result and awards the prize
 * level (0..4 at > 0x27 / 0x4F / 0x77 / 0x8B). Then friend book and mail.
 */
void comm_outcome_game2(void)
{
    u8 m;

    comm_tx_record = (FLASH_COMM_TX + 0x800);
    comm_rx_record = (u8 *)&comm_rx_phase2;
    m = nfc_session.x002;
    if (m == 1) {
        comm_load_section_pics(anim_pic_slots, comm_rx_phase2.sect_poses + (u8 *)&comm_rx_phase2, 3);
        nfc_session.score_initiator = *(u16 *)(comm_tx_record + 0xb0);
        nfc_session.score_target = *(u16 *)(comm_rx_record + 0xb0);
        nfc_session.score_total = nfc_session.score_initiator + nfc_session.score_target;
        game_timing_result_anim(m, nfc_session.score_total);
    } else {
        if (*(u16 *)(comm_rx_record + 0x76) != 0) {
            anim_comm_done();
            tmr_t5_wait_100ms(*(u16 *)(comm_rx_record + 0x76) * 10);
        }
        nfc_session.score_initiator = *(u16 *)(comm_rx_record + 0xb0);
        nfc_session.score_target = *(u16 *)(comm_tx_record + 0xb0);
        nfc_session.score_total = nfc_session.score_initiator + nfc_session.score_target;
        game_timing_result_anim(0, nfc_session.score_total);
    }
    friend_book_update();
    comm_store_peer_mail();
}

/*
 * Session type 7 (commemorative photo; dispatched for nfc_session.x003 == 7, not marriage)
 * outcome: partner poses, photo animation (anim_photo_session on the initiator), then
 * album_make_photo(role, theirs, ours) merges the two picture sets and stores the photo
 * record (0x2770000 + k x 0x2800 album slots); pet_add_happiness(1), anim_happy, friend
 * book, mail.
 */
void comm_outcome_photo(void)
{
    comm_tx_record = (u32)(FLASH_COMM_TX + 0x800);
    comm_rx_record = (u8 *)&comm_rx_phase2;
    if (nfc_session.x002 == 1) {
        comm_load_section_pics(anim_pic_slots, comm_rx_phase2.sect_poses + (u8 *)&comm_rx_phase2, 3);
        anim_photo_session();
        album_make_photo(0, (void *)comm_rx_record, (void *)comm_tx_record);
    } else {
        if (*(u16 *)(comm_rx_record + 0x76) != 0) {
            anim_comm_done();
            tmr_t5_wait_100ms(*(u16 *)(comm_rx_record + 0x76) * 10);
            anim_good_result();
        }
        album_make_photo(1, (void *)comm_rx_record, (void *)comm_tx_record);
    }
    pet_add_happiness(1);
    anim_happy();
    friend_book_update();
    comm_store_peer_mail();
}

const u8 D_25622EC[] = _("まだプロポーズ");
const u8 D_25622FC[] = _("できないよ!");
const u8 D_256230C[] = _("このともだちには");
const u8 D_2562320[] = _("このともだちからは");
const u8 D_2562334[] = _("プロポーズ");
/* pairs of character ids (both orders), ended by {0xFFFF, 0xFFFF} */
const u16 comm_forbidden_pairs[21][2] = {
    { 16, 7001 }, { 7001, 16 }, { 17, 7002 }, { 7002, 17 }, { 18, 7003 }, { 7003, 18 },
    { 19, 7004 }, { 7004, 19 }, { 20, 1005 }, { 1005, 20 }, { 1001, 5001 }, { 5001, 1001 },
    { 1002, 5002 }, { 5002, 1002 }, { 1003, 5003 }, { 5003, 1003 }, { 1004, 5004 },
    { 5004, 1004 }, { 21, 1003 }, { 1003, 21 }, { 0xffff, 0xffff },
};
const u8 D_2562394[] = _("もういちど");
const u8 D_25623A0[] = _("つうしんするよ");


/*
 * After a successful NFC exchange (called by F69): leave the NFC timing bracket
 * (comm_exclusive_timing_off), run the outcome handler of the session type nfc_session.x003 (= record
 * byte 0x4C; 0 / 1 nothing, 2 greeting comm_outcome_greeting, 3/4 game comm_outcome_game1 /
 * comm_outcome_game2, 5 gift comm_outcome_gift, 6 marriage comm_outcome_marriage, 7 photo comm_outcome_photo, 8 app
 * download comm_outcome_app on the target only, nothing on the debug sender; the handlers
 * update the friend book), note the partner's character id 0x74 (evo_flag_met_chara: ids 20 /
 * 2009 set pet.met_orenetchi for adults), save (save_after_sound), 10 s menu timeout.
 */
int comm_run_outcome(void)
{
    u16 v;

    comm_exclusive_timing_off();
    room_view = 0;
    if (nfc_session.x003 == 2) {
        v = *(u16 *)(comm_rx_record + 0x74);
    } else {
        comm_outcome_record = FLASH_COMM_RX;
        v = *(u16 *)(comm_outcome_record + 0x74);
    }
    switch (nfc_session.x003) {
    case 0:
    case 1:
        break;
    case 2:
        comm_outcome_greeting();
        break;
    case 5:
        comm_outcome_gift();
        break;
    case 6:
        comm_outcome_marriage();
        break;
    case 8:
        if (nfc_session.x002 == 2)
            comm_outcome_app();
        break;
    case 3:
        comm_outcome_game1();
        break;
    case 4:
        comm_outcome_game2();
        break;
    case 7:
        comm_outcome_photo();
        break;
    }
    evo_flag_met_chara(v);
    save_after_sound();
    anim_busy = 0;
    ui_timeout = 10;
    return 0;
}

/*
 * The idle-touch event (notes/README.md item 8): called from nfc_idle_touch when an RF
 * field woke the idle toy but no Tamagotchi answered within 2 s (peer 0xF0EF, i.e. a
 * phone or reader: nfc_idle_target_session result 4); neither an outing event nor part of session
 * type 3. Picks an event keyed by the day of the month (rtc_get_day): days 3, 13, 23, 30,
 * 31 -> kind 0 (a gift item, game_score_reward(game_slot_screen())), 5/8/15/18/25/28 -> kind 1 (the
 * quiz game_quiz), 9/19/29 -> kind 2 (spot_fortune_event), each with a 50% roll of 100,
 * else one of three generic events pet_event_idle_random / pet_event_phrase / spot_profile_talk by a second
 * roll; pet.comm_traveller_due set (every 10th visit, comm_count_tenth) forces spot_traveller_souvenir. Ends with
 * comm_count_and_save (the communication count pet.comm_count + 1, save), session type cleared and
 * NFC detection re-armed (nfc_wait_field_on). Nothing about the reader is read or kept. The
 * three generic events and the game are also Type 2 spot kinds 1-4 (spot_reader). (MED)
 */
void comm_idle_touch_event(void)
{
    int x = 0;
    int y;
    int n;
    int i;
    u8 k = rtc_get_day();

    if (k == 3 || k == 13 || k == 23 || k == 30 || k == 31) {
        x = 1;
        y = 0;
    } else if (k == 5 || k == 8 || k == 15 || k == 18 || k == 25 || k == 28) {
        x = 1;
        y = 1;
    } else if (k == 9 || k == 19 || k == 29) {
        x = 1;
        y = 2;
    }
    if (save.pet.comm_traveller_due != 0)
        x = 2;
    if (x == 1) {
        n = util_rand_except(100, 100);
        if (n <= 0x31) {
            if (y == 0)
                game_score_reward(((int (*)(void))game_slot_screen)());
            else if (y == 1)
                game_quiz();
            else if (y == 2)
                spot_fortune_event();
        } else {
            n++;
            for (i = 0; i < n % 10 + 1; i++)
                n = util_rand_except(100, 100);
            if (n <= 0x20)
                pet_event_idle_random();
            else if (n <= 0x41)
                pet_event_phrase();
            else
                spot_profile_talk();
        }
    } else if (x == 2) {
        spot_traveller_souvenir();
    } else {
        n = util_rand_except(100, 100);
        if (n <= 0x20)
            pet_event_idle_random();
        else if (n <= 0x41)
            pet_event_phrase();
        else
            spot_profile_talk();
    }
    comm_count_and_save();
    {
        u8 z = 0;
        nfc_session.x000_b = z;
        nfc_session.x003 = z;
    }
    nfc_wait_field_on();
}

/*
 * NFC session cancelled by the user? Button C pressed (btn_c_presses == 1) -> nfc_result = 4
 * and return 1, else 0.
 */
u8 comm_user_cancelled(void)
{
    u8 v = btn_c_presses;

    if (v == 1) {
        nfc_result = 4;
        return v;
    }
    return 0;
}

/*
 * NFC: short busy delay of 0x5DC0 (24000) iterations of a 4-instruction loop (nop, add,
 * cmp, jrle), roughly 3-5 ms at 30 MHz depending on flash wait states (pause between
 * retries; estimate, not measured).
 */
void nfc_short_delay(void)
{
    int i;
    for (i = 0; i < 0x5dc0; i++)
        asm("nop");
}

/*
 * NFC: put the front-end to sleep (RfOff), power it down (nfc_hw_power_off) and mark it off
 * (x001 = 2).
 */
void nfc_power_off(void)
{
    nfc_sleep_field_off_set();
    nfc_hw_power_off();
    nfc_session.x001 = 2;
}

/*
 * Apply the Gotchi-point transfer described in exchange record p (session type 5 only):
 * byte 0xCF = 1 adds the amount 0xD0 to our Gotchi points (pet_add_points), 2 subtracts it
 * (pet_spend_points).
 */
void comm_apply_points(struct GiftPoints *p)
{
    u8 c;

    if (nfc_session.x003 == 5) {
        if (nfc_session.x002 == 1) {
            c = p->points_mode;
            if (c == 0)
                return;
        } else {
            c = p->points_mode;
            if (c == 0)
                return;
        }
        {
            u32 a = p->points;
            if (c == 1)
                pet_add_points(a);
            else if (c == 2)
                pet_spend_points(a);
        }
    }
}

/*
 * Mask the button interrupts (port inputs 0..2, pINT_EK01_EP03 bits 0-2) selected by mask
 * a (bit 0 = A, bit 1 = B, bit 2 = C); btn_unmask unmasks them.
 */
void btn_mask(int a)
{
    a &= 7;
    INT_EK01_EP03 &= ~a;
}

/*
 * Re-enable the button interrupts selected by mask v, clearing their pending flags first
 * (writing 1 clears an ITC flag).
 */
void btn_unmask(int v)
{
    v &= 7;
    INT_FK01_FP03 = v | INT_FK01_FP03;
    INT_EK01_EP03 = v | INT_EK01_EP03;
}

/*
 * Clear the button counters and wait k ticks of 100 ms with halt (btn_wait_press_or_timeout).
 */
void ui_wait_ticks(u8 k)
{
    anim_busy = 0;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_wait_press_or_timeout(k);
}

/*
 * Like ui_wait_ticks but restarts the wait while button A is pressed.
 */
void ui_wait_ticks_hold_a(u8 k)
{
    anim_busy = 0;
again:
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_wait_press_or_timeout(k);
    if (btn_a_presses != 0)
        goto again;
}







/*
 * Erase the NFC transfer areas in flash: sectors 0x84..0x85 (0x27C0000 tx and 0x27D0000
 * rx, 64 KB each).
 */
int comm_erase_banks(void)
{
    int v0;

    snd_wait_before_flash();
    sys_irq_disable();
    v0 = func_0000A54(FLASH_BASE, FLASH_SEC_COMM_TX, FLASH_SEC_COMM_RX);
    sys_irq_enable();
    return v0;
}

/*
 * Flash helper of the exchange: fill n bytes of flash at dst with the halfword v, after
 * waiting for the sound to finish (snd_wait_before_flash), interrupts masked (sys_irq_disable /
 * sys_irq_enable). 0 ok, -1 on a flash error. Bug (verified): the -1 return skips
 * sys_irq_enable, so interrupts stay disabled (no button, timer, RTC or NFC handler runs;
 * `halt` loops spin) until a later flash call succeeds; the callers ignore the result.
 * Only a flash program timeout or a non-erased cell can trigger it (the banks are erased
 * first), so it should not happen in practice. flash_program_bytes has the same bug.
 */
int flash_fill(u32 n, u32 dst, u16 v)
{
    u16 w[1];
    u32 i;
    u32 off;
    u8 *p;

    w[0] = v;
    p = (u8 *)dst;
    snd_wait_before_flash();
    sys_irq_disable();
    for (i = 0, off = 0; i < n / 2; i++, off += 2) {
        if (func_0000B52((u16 *)(p + off), w[0], FLASH_BASE) != 0)
            return -1;
    }
    sys_irq_enable();
    return 0;
}

/*
 * Fill n bytes of RAM at dst with the halfword v.
 */
int util_fill16(u32 n, u8 *dst, u16 v)
{
    u32 i;
    u32 off;

    for (i = 0, off = 0; i < n / 2; i++, off += 2)
        *(u16 *)(dst + off) = v;
    return 0;
}

/*
 * Program n bytes from src into flash at dst (halfword writes with the IRAM routine
 * func_0000B52) after waiting for the sound (snd_wait_before_flash), interrupts masked. Returns -1
 * on a flash error, then with interrupts still disabled (same bug as flash_fill).
 */
int flash_program_bytes(u32 n, u32 dst, u8 *src)
{
    u8 buf[4];
    u32 i;
    u32 off;

    snd_wait_before_flash();
    memset(buf, 0, 4);
    sys_irq_disable();
    for (i = 0, off = 0; i < n / 2; i++, off += 2) {
        if (func_0000B52((u16 *)(dst + off), *(u16 *)(src + off), FLASH_BASE) != 0)
            return -1;
    }
    sys_irq_enable();
    return 0;
}

/*
 * Program n bytes from the halfword buffer src into flash at dst after waiting for the
 * sound (snd_wait_before_flash), interrupts masked. Returns 0 ok, else the IRAM flash error.
 */
int flash_program_halfwords(u32 dst, u16 *src, int n)
{
    int i;
    int r = 0;
    u32 d;

    snd_wait_before_flash();
    sys_irq_disable();
    d = dst;
    for (i = r; i < n / 2; i++) {
        r = func_0000B52((u16 *)d, src[i], FLASH_BASE);
        if (r != 0)
            break;
        d += 2;
    }
    sys_irq_enable();
    return r;
}

/*
 * Return the address of the item file attached to exchange record p: the data start of
 * section 3 (directory entry 0xE6), p + p->x0e6 + 8; remember p in comm_outcome_record.
 */
int comm_record_item(struct ExchangeRecord *p)
{
    comm_outcome_record = (u32)p;
    return (int)p + p->sect_item + 8;
}

/*
 * Spin 65535 times (the test of T16 ch.4 CTL4 == 1 inside only adds a nop: the loop does
 * not exit when the tick stops), wait for the sound to finish (snd_wait_end), then
 * save_game. anim_busy = 1 while running: tells the button handlers not to start
 * key-click sounds (probably because the sound ISR and its data are in the NOR flash).
 */
void save_after_sound(void)
{
    int i;

    anim_busy = 1;
    for (i = 0; i <= 0xfffe; i++) {
        if (T16_CTL4 == 1)
            __asm__("nop");
    }
    snd_wait_end();
    save_game();
    anim_busy = 0;
}

/*
 * Spin 65535 times (the CTL4 == 1 test inside does not end the loop early), then wait for
 * the sound to finish (snd_wait_end). Called before NOR flash operations (probably
 * because the sound ISR and its data are in the flash).
 */
void snd_wait_before_flash(void)
{
    int i;

    for (i = 0; i <= 0xfffe; i++) {
        if (T16_CTL4 == 1)
            __asm__("nop");
    }
    snd_wait_end();
}

/*
 * comm_same_peer(id, name) = is the received friend the stored peer? 0 when id (u16, the
 * record's owner id at 0x72) and the 16-byte name match the peer remembered by
 * comm_remember_peer in comm_phase1_peer, else 2: guards that the phase-2 record of an exchange comes
 * from the same Tamagotchi as phase 1 (asm).
 */
int comm_same_peer(u16 id, const u8 *name)
{
    const u8 *s = name;
    int hi = id >> 8;
    u8 lo = id;
    u8 i;

    for (i = 0; i <= 15; i++)
        if (comm_phase1_peer.name[i] != s[i])
            return 2;
    if (comm_phase1_peer.owner_id_hi != hi)
        return 2;
    if (comm_phase1_peer.owner_id_lo != lo)
        return 2;
    return 0;
}

/*
 * Remember the current peer: its owner id v (the record's 0x72, which comm_same_peer
 * compares; big endian in comm_phase1_peer.owner_id_hi/x002) and its 16-byte name (comm_phase1_peer.name).
 */
void comm_remember_peer(u16 v, u8 *src)
{
    u8 i;
    u8 hi = v >> 8;

    comm_phase1_peer.owner_id_hi = hi;
    comm_phase1_peer.owner_id_lo = v;
    for (i = 0; i <= 15; i++)
        comm_phase1_peer.name[i] = src[i];
}

/*
 * friend_lookup_or_add(id, name, flag) = friend-list lookup/insert (friend_list, 31 records of
 * 0x16 bytes; see F16 friend_find_or_add): an existing record with the same owner id and name
 * gets x003 = flag and its met-by bit (x014 |= 1 in the greeting session type 2, else
 * |= 2), comm_friend_is_new = 0; otherwise the first free record is filled (x014 = 1 or 2, x015 =
 * 0), comm_friend_is_new = 1. Returns the index, or 31 when no record is free (asm).
 */
u8 friend_lookup_or_add(u16 id, const u8 *name, u8 flag)
{
    const u8 *s = name;
    int hi = id >> 8;
    u8 lo = id;
    u8 k, j, diff;
    struct Friend *e;

    comm_friend_is_new = 0;
    for (k = 0; k <= 30; k++) {
        if (save.friend_list[k].x000_b != 0) {
            diff = 0;
            for (j = 0; j <= 15; j++)
                if (save.friend_list[k].name[j] != s[j])
                    diff++;
            if (diff == 0 && save.friend_list[k].x001 == hi) {
                e = save.friend_list;
                if (e[k].owner_id_lo == lo) {
                    e[k].gender = flag;
                    if (nfc_session.x003 == 2)
                        e[k].met_flags[0] |= 1;
                    else
                        save.friend_list[k].met_flags[0] |= 2;
                    return k;
                }
            }
        }
    }
    comm_friend_is_new = 1;
    for (k = 0; k <= 30; k++) {
        if (save.friend_list[k].x000_b == 0) {
            save.friend_list[k].x001 = hi;
            save.friend_list[k].owner_id_lo = lo;
            for (j = 0; j <= 15; j++)
                save.friend_list[k].name[j] = s[j];
            save.friend_list[k].gender = flag;
            e = save.friend_list;
            if (nfc_session.x003 == 2)
                e[k].met_flags[0] = 1;
            else
                save.friend_list[k].met_flags[0] = 2;
            save.friend_list[k].met_flags[1] = 0;
            return k;
        }
    }
    return k;
}

/*
 * Portrait of friend k: its stored 0xE80-byte record in the portrait sectors (slot s =
 * x015 - 1, 17 records per 64 KB sector: sector 9 + s / 17); the placeholder image
 * D_215DBE8 when the current partner comm_partner_slot is new (comm_friend_is_new) or in slot 30+, or the
 * record is erased.
 */
u8 *friend_portrait(int k)
{
    u8 a;
    u8 b;
    u8 *r;
    u8 c = comm_partner_slot;

    r = D_215DBE8;
    if (c > 29 || comm_friend_is_new != 0)
        return r;
    a = save.friend_list[k].met_flags[1] - 1;
    b = a / 0x11 + FLASH_SEC_PORTRAITS;
    a = a % 0x11;
    r = flash_sector_addr(b) + a * 0xe80;
    if (*(u32 *)r == 0xffffffff)
        r = D_215DBE8;
    return r;
}


/*
 * After a session, update the friend book with the partner of this exchange: look it up
 * / add it (friend_lookup_or_add by owner id + name + family byte; the greeting session type 2
 * uses the fields of the record received in RAM, comm_rx_packet.rec), then file
 * its portrait: type 2 erases sector 13 (0x2050000) and copies there 0xE00 bytes of the
 * stored portrait (friend_portrait) + the 0x80-byte profile from the RAM record's section;
 * the other types point at section 0 of the received record in flash (0x27D0808 + entry
 * 0xE0). The location goes to nfc_session.friend_data. For a real slot (< 30) friend_portrait_backup /
 * friend_portrait_store store it in the portrait sectors and in type 2 the "friend book updated"
 * screen is shown. New friends get x000 = 0xFF. anim_busy = 1 meanwhile (no key-click
 * sounds).
 */
void friend_book_update(void)
{
    u8 *q;
    int t;

    anim_busy = 1;
    snd_wait_before_flash();
    if (nfc_session.x003 == 2) {
        q = comm_rx_record;
        t = friend_lookup_or_add(comm_rx_packet.rec.owner_id, comm_rx_packet.rec.name, comm_rx_packet.rec.gender);
        comm_partner_slot = t;
        q = q + comm_rx_packet.rec.sect_profile + 8;
        snd_wait_before_flash();
        sys_irq_disable();
        func_0000A54(FLASH_BASE, FLASH_SEC_TEMP_RECORD, FLASH_SEC_TEMP_RECORD);
        flash_program_bytes(0xe00, FLASH_TEMP_RECORD, friend_portrait(comm_partner_slot));
        flash_program_bytes(0x80, (FLASH_TEMP_RECORD + 0xE00), q);
        q = (u8 *)FLASH_TEMP_RECORD;
        nfc_session.friend_data = (u32)q;
        sys_irq_enable();
    } else {
        int o;
        comm_partner_slot = friend_lookup_or_add(comm_rx_phase2.owner_id, comm_rx_phase2.name, comm_rx_phase2.gender);
        o = comm_rx_phase2.sect_portrait;
        q = (u8 *)&comm_rx_phase2 + 8 + o;
        nfc_session.friend_data = (u32)q;
    }
    if (comm_partner_slot <= 0x1d) {
        if (nfc_session.x003 == 2)
            comm_info_screen(0);
        if (comm_friend_is_new != 0)
            save.friend_list[comm_partner_slot].x000_b = -1;
        snd_wait_before_flash();
        friend_portrait_backup(comm_partner_slot);
        friend_portrait_store(comm_partner_slot, q - 0x118);
    } else {
        if (comm_friend_is_new != 0)
            save.friend_list[comm_partner_slot].x000_b = -1;
    }
    anim_busy = 0;
}

/*
 * After a session: store the received friend data (nfc_session.friend_data - 0x118) for friend
 * comm_partner_slot = 29 (friend_portrait_backup / friend_portrait_store). Called (func_25039AA) when a friend is
 * deleted (friend_delete) while friend_book_state.new_friend_waiting marks a new friend waiting for a free slot
 * (comm_result_wait_book opens the friend book right after the session). x010 is then the
 * portrait + profile staged in flash sector 13 at 0x2050000 by friend_book_update (greeting) or
 * the record received at 0x27D0808 + offset: in that flow nothing else rewrites sector 13.
 */
void comm_store_pending_friend(void)
{
    u8 tmp[8];
    u32 p;

    if (friend_book_state.new_friend_waiting == 0)
        return;
    comm_partner_slot = 29;
    p = nfc_session.friend_data;
    friend_book_state.deleted_count++;
    snd_wait_before_flash();
    friend_portrait_backup(comm_partner_slot);
    friend_portrait_store(comm_partner_slot, (u8 *)(p - 0x118));
    friend_book_state.new_friend_waiting = 0;
}

/*
 * Debug/test helper (no caller): make up a random friend, the same steps as the greeting
 * path of friend_book_update with random data (util_rand_except(a, b) = random number): a random
 * owner id s and a random 4-glyph name d[] (pairs 04 xx), added to the friend list
 * friend_list with comm_partner_slot = friend_lookup_or_add(s, d, family 1); a random profile b[] (gender
 * 0/1, mood 0..5, 0..99, month 0..11), five 16-byte strings of (4, n) pairs and a (month,
 * day) pair. Then erase flash sector 0xD (0x2050000) and write the friend's 0xE00-byte
 * portrait (friend_portrait, the placeholder for a new friend) at 0x2050000 and the 0x80
 * profile bytes at 0x2050E00; nfc_session.friend_data remembers the address. For a real slot
 * (< 30) friend_portrait_backup / friend_portrait_store file the portrait in the friend portrait sectors;
 * new friends get x000 = 0xFF. snd_wait_before_flash (wait for the sound) / save_after_sound (save)
 * bracket it. Fable read this as the creation of a new character record; the code makes a
 * friend (see notes/OPEN.md item 6 for the sector 0xD users).
 */
void comm_make_random_friend(void)
{
    u8 a[2];
    u8 buf[0x100];
    u8 b[4];
    u8 c[5][16];
    u8 d[16];
    u8 *p;
    u16 s;
    u8 *t;
    int i;

    snd_wait_before_flash();
    memset(buf, 0, 0x100);
    memset(b, 0, 4);
    memset(c, 0, 0x50);
    memset(d, 0, 0x10);
    s = util_rand_except(1000, 1000) + util_rand_except(2000, 2000);
    d[0] = 4;
    d[1] = (util_rand_except(100, 100) + 1) % 30 + 1;
    d[2] = 4;
    d[3] = (util_rand_except(200, 200) + 1) % 50 + 1;
    d[4] = 4;
    d[5] = (util_rand_except(100, 100) + 1) % 30 + 1;
    d[6] = 4;
    d[7] = (util_rand_except(200, 200) + 1) % 50 + 1;
    comm_partner_slot = friend_lookup_or_add(s, d, 1);
    p = buf;
    b[0] = (u32)(util_rand_except(100, 100) + 1) % 2;
    b[1] = (util_rand_except(200, 100) + 1) % 6;
    b[2] = (util_rand_except(100, 100) + 1) % 100;
    b[3] = (util_rand_except(200, 200) + 1) % 12;
    memcpy(p, b, 4);
    p += 4;
    for (i = 0; i < 5; i++) {
        c[i][0] = 4;
        c[i][1] = (util_rand_except(100, 100) + 1) % 30 + 1;
        c[i][2] = 4;
        c[i][3] = (util_rand_except(200, 200) + 1) % 50 + 1;
    }
    memcpy(p, c, 0x50);
    p += 0x50;
    a[0] = (util_rand_except(200, 100) + 1) % 12;
    a[1] = (util_rand_except(100, 100) + 1) % 31;
    memcpy(p, a, 2);
    t = buf;
    snd_wait_before_flash();
    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_TEMP_RECORD, FLASH_SEC_TEMP_RECORD);
    flash_program_bytes(0xe00, FLASH_TEMP_RECORD, friend_portrait(comm_partner_slot));
    flash_program_bytes(0x80, (FLASH_TEMP_RECORD + 0xE00), t);
    t = (u8 *)FLASH_TEMP_RECORD;
    nfc_session.friend_data = (u32)t;
    sys_irq_enable();
    if (comm_partner_slot <= 29) {
        if (comm_friend_is_new)
            save.friend_list[comm_partner_slot].x000_b = 0xff;
        friend_portrait_backup(comm_partner_slot);
        friend_portrait_store(comm_partner_slot, t - 0x118);
    } else {
        if (comm_friend_is_new)
            save.friend_list[comm_partner_slot].x000_b = 0xff;
    }
    save_after_sound();
}

/*
 * Pick pet.next_egg_kind = 1 or 3 at random (70/30), inverted when pet.gender is set.
 */
void pet_pick_variant_flag(void)
{
    u8 r;

    if (save.pet.gender == 0) {
        if (rand() % 100 > 30)
            r = 3;
        else
            r = 1;
    } else {
        if (rand() % 100 > 30)
            r = 1;
        else
            r = 3;
    }
    save.pet.next_egg_kind = r;
}

/*
 * Species group ("family" / base id) of character id: for ids <= 3999 the u16 at +6 of
 * its chara_table table entry (looked up by chara_index_by_id); ids above that (downloaded
 * characters) are returned unchanged, i.e. are their own group.
 */
int chara_family(u16 id)
{
    const u16 *p;

    if (id > 3999)
        return id;
    p = &chara_table[chara_index_by_id(id)].id;
    return p[2];
}

/*
 * The RAM variables this file defines.
 */
u16 comm_storing_gift;
u8 *comm_rx_record;
u32 comm_outcome_record;
struct PeerId comm_phase1_peer;
