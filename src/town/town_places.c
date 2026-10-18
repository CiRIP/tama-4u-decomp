/*
 * The town: the place menu and its places (park and plaza, sports ground, lottery, school and
 * clubs, Tama-Depa floors, matchmaking, transformation booth), jobs, outings with their diary
 * pages, and the street encounters.
 *
 * Code 24110A6..241488C, .rodata from 2541848.
 */
#include "proto/town/town_places.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/chara/chara_draw.h"
#include "proto/food/food_meals.h"
#include "proto/shop/shop_reform.h"
#include "proto/shop/shop_browser.h"
#include "proto/diary/diary_notebook.h"
#include "proto/pet/pet_toilet.h"
#include "proto/anim/anim_scripts.h"
#include "proto/evo/evo_counters.h"
#include "proto/evo/evo_special.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/ui/ui_message.h"
#include "proto/pet/pet_gone.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/ir/ir_receive.h"
#include "proto/ir/ir_driver.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_menu.h"
#include "proto/town/town_street.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/owner/owner_profile.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/game/game_menu.h"
#include "proto/game/game_training.h"
#include "proto/comm/comm_outcome.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * F28 = the town ("おでかけ"): たまデパ (department store: buy / sell), こうえん (park: ひろば
 * square encounters and うんどうじょう sports ground), がっこう (school: ぶかつ club
 * activities), おみあいじょ (matchmaking parlour), たまのうえん (farm), であいストリート
 * (encounter street), part-time jobs (アルバイト: patrol, shop keeping, ...) with the
 * job record job, the lottery, and the "おせわマーク" care-mark achievements. Most
 * scenes are played through the generated animation zone (func_247xxxx..func_248xxxx)
 * with speech lines chara_text(0, 100, n) shown by ui_show_message.
 *
 * Award care-mark achievement / unlock flag k ("おせわマーク デザイン ゲットしました!")
 * the first time only: set the flag, save, jingle 0x2F and a 4-line message
 * (ui_message_box with D_2541FA8..).
 */
void town_award_care_mark(int k)
{
    if (pet_get_unlock_flag(k) != 0)
        return;
    snd_wait_end();
    pet_set_unlock_flag(k, 1);
    save_game();
    snd_play(0x2f);
    ui_message_box(D_2541FA8, D_2541FB8, D_2541FC4, D_2541FD8, 8);
}

/*
 * Select the second character for job animation k (characters 0x52/0x46/0x48/0x3D) and
 * return the park background gfx_bg_park. No caller in this firmware.
 */
u8 *job_select_chara(u8 k)
{
    switch (k) {
    case 1:
        chara_select_other_by_set(0x52);
        break;
    case 2:
        chara_select_other_by_set(0x46);
        break;
    case 3:
        chara_select_other_by_set(0x48);
        break;
    default:
        chara_select_other_by_set(0x3d);
        break;
    }
    return gfx_bg_park;
}

/*
 * Part-time job record job (adult pet away at a job): x000 = current job (0 none,
 * 1..4), x001[0] = shifts worked this job, x001[job] = total experience per job (cap 32),
 * x006 = minute counter, x008 = "job over" flag. Jobs only run between 7:00 and 18:59
 * (job_check_hours); job_minute_tick = the per-minute job tick.
 *
 * Clear the current job (the experience is kept; job_clear_all clears it too).
 */
void job_clear(void)
{
    job.kind = 0;
    job.minutes = 0;
    job.hours[0] = 0;
    job.finished = 0;
}

/*
 * Clear the job state and the per-job experience counters.
 */
void job_clear_all(void)
{
    int i;

    job.kind = 0;
    job.minutes = 0;
    job.hours[0] = 0;
    job.finished = 0;
    for (i = 1; i <= 4; i++)
        job.hours[i] = 0;
}

/*
 * Start job a (1..4): job.kind = a, counters cleared.
 */
void job_start(int a)
{
    job.kind = a;
    job.minutes = 0;
    job.hours[0] = 0;
    job.finished = 0;
}

/*
 * Job finished (x008 == 1) and waiting to be collected?
 */
int job_is_finished(void)
{
    if (job.kind != 0 && job.finished == 1)
        return 1;
    return 0;
}

/*
 * Jobs only run 7:00..18:59: outside these hours the job ends (x008 = 1).
 */
u8 job_check_hours(void)
{
    u8 t;
    struct Job *p;

    t = rtc_get_hour();
    rtc_get_minute();
    p = &job;
    if (p->kind != 0) {
        if (!(t >= 7 && t <= 18)) {
            p->finished = 1;
            return 1;
        }
    }
    return 0;
}

/*
 * Per-minute job work (adults): every 60 minutes +250 gotchi points and one hour
 * (x001[0], ends after 30), experience of the job type +1 (max 32); the job ends when the
 * pet's happiness or hunger reaches 0. Returns 1 when the job is over.
 */
int job_minute_tick(void)
{
    struct Job *p;
    u8 *q;

    rtc_get_hour();
    rtc_get_minute();
    if (save.pet.life_stage <= 3)
        return 0;
    p = &job;
    if (p->kind != 0) {
        if (p->finished == 1)
            return 1;
        if (job_check_hours() != 0)
            return 1;
        job.minutes++;
        if (job.minutes > 59) {
            job.minutes = 0;
            pet_add_points(250);
            job.hours[0]++;
            if (job.hours[0] > 30) {
                job.finished = 1;
                return 1;
            }
            q = (u8 *)&job + job.kind;
            if (q[1] <= 31)
                q[1]++;
        }
        if (save.pet.happiness == 0 || save.pet.hunger == 0) {
            job.finished = 1;
            return 1;
        }
    }
    return 0;
}

/*
 * Returns -1 (stub).
 */
int job_reward_item(void)
{
    rand();
    return -1;
}

/*
 * Write the "worked at job n" picture-diary page (diary_compose_draft, events 3000..3003 for job
 * types 1..4, random variant) and, when it was written, draw it (diary_assign_partner_pics /
 * diary_render_store).
 */
void job_diary_page(void)
{
    int r;

    snd_wait_end_b(0);
    snd_wait_end_b(1);
    switch (job.kind) {
    case 1:
        snd_wait_end_b(2);
        snd_wait_end_b(3);
        r = diary_compose_draft((u8 *)diary_draft, D_2541FDC, D_2541FFC, D_2541FE8, D_2541FE8, D_2541FE8, D_2541FDC, 3000, util_rand_except(3, 3));
        break;
    case 2:
        snd_wait_end_b(2);
        snd_wait_end_b(3);
        r = diary_compose_draft((u8 *)diary_draft, D_2541FDC, D_2541FFC, D_2541FE8, D_2541FE8, D_2541FE8, D_2541FDC, 3001, util_rand_except(3, 3));
        break;
    case 3:
        snd_wait_end_b(2);
        snd_wait_end_b(3);
        r = diary_compose_draft((u8 *)diary_draft, D_2541FDC, D_2541FFC, D_2541FE8, D_2541FE8, D_2541FE8, D_2541FDC, 3002, util_rand_except(3, 3));
        break;
    default:
        snd_wait_end_b(2);
        snd_wait_end_b(3);
        r = diary_compose_draft((u8 *)diary_draft, D_2541FDC, D_2541FFC, D_2541FE8, D_2541FE8, D_2541FE8, D_2541FDC, 3003, util_rand_except(3, 3));
        break;
    }
    if (r == 0) {
        diary_snapshot_images((u32 *)diary_images, food_party_table_pic_chara(), r);
        diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
        snd_wait_end();
        diary_render_store(diary_draft, diary_images);
    }
}

/* callers use the void prototype; the definition returns int */
/*
 * End of a part-time job (a = 1: collected by the player, or interrupted: unclear;
 * otherwise the job-end scene job_diary_page runs and "どうする? かえる/かえらない" asks
 * whether to go home): the diary page (events 3000..3003), and when the job's experience
 * passed 29 shifts a reward item (job_reward_item, a stub returning -1 in this firmware,
 * stored with item_add_to_inventory; the "おつかれさま ... これをどうぞ" lines D_25418C8..), then
 * the diary notice diary_writing_scene.
 */
int job_end(u8 a)
{
    int sel = 1;
    int n, v;
    struct Job *e;

    if (a != sel) {
        job_diary_page();
        if (btn_c_presses) {
            diary_writing_scene();
            ui_timeout = 10;
            return 1;
        }
        tmr_t5_standard_tick();
        ui_timeout = 10;
        sel = 0;
        switch (ui_yes_no(D_2542010, D_254201C, D_2542024, 16, &sel)) {
        case 1:
            switch (sel) {
            case 0:
                break;
            case 1:
                return 1;
            }
            break;
        case 2:
            return 1;
        case 3:
            return 1;
        }
    }
    switch (job.kind) {
    case 1:
        n = 7;
        break;
    case 2:
        n = 0xb;
        break;
    case 3:
        n = 0xf;
        break;
    default:
        n = 0x13;
    }
    ((int (*)(int))snd_wait_end_b)(0);
    snd_wait_end_b(1);
    if (job.hours[0] == 0) {
        snd_wait_end_b(2);
        snd_wait_end_b(3);
        v = -1;
        switch (n) {
        case 7:
            v = 3000;
            break;
        case 0xb:
            v = 3001;
            break;
        case 0xf:
            v = 3002;
            break;
        case 0x13:
            v = 3003;
            break;
        }
        if (v > 0) {
            v = diary_compose_draft((u8 *)diary_draft, D_2541FDC, D_2541FFC, D_2541FE8, D_2541FE8, D_2541FE8, D_2541FDC, v, util_rand_except(3, 3));
            if (v == 0) {
                diary_snapshot_images((u32 *)diary_images, food_party_table_pic_chara(), v);
                diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
                snd_wait_end();
                diary_render_store(diary_draft, diary_images);
            }
        }
        job_clear();
    } else {
        snd_wait_end_b(2);
        snd_wait_end_b(3);
        e = &job;
        if (e->hours[e->kind] > 0x1d) {
            int k;
            u32 t;
            e->hours[e->kind] = 0;
            k = job_reward_item();
            t = anim_pic_slots[0][0];
            item_load_pictures(anim_pic_slots[0], (struct TamagoFile *)k, 1);
            anim_pic_slots[0][0] = (const u8 *)t;
            snd_wait_end();
            item_add_to_inventory((struct TamagoFile *)k);
        }
        v = -1;
        switch (n) {
        case 7:
            v = 3000;
            break;
        case 0xb:
            v = 3001;
            break;
        case 0xf:
            v = 3002;
            break;
        case 0x13:
            v = 3003;
            break;
        }
        if (v > 0) {
            v = diary_compose_draft((u8 *)diary_draft, D_2541FDC, D_2541FFC, D_2541FE8, D_2541FE8, D_2541FE8, D_2541FDC, v, util_rand_except(3, 3));
            if (v == 0) {
                diary_snapshot_images((u32 *)diary_images, food_party_table_pic_chara(), v);
                diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
                snd_wait_end();
                diary_render_store(diary_draft, diary_images);
            }
        }
        job_clear();
        anim_clear_pic_slots2();
    }
    anim_happy();
    snd_wait_end();
    save_game();
    diary_writing_scene();
    return 1;
}

/*
 * Activity k done often enough (club activity?): counters pet.activity0_count..D_00012C4, counted
 * by F30 pet_count_activity: more than 9 for 0 and 3, more than 1 for 1 and 2.
 * pet_activity_done_count = how many, pet_activity_done_bool(k) the same as 0/1.
 */
u8 pet_activity_done(u8 k)
{
    switch (k) {
    case 0:
        if (save.pet.activity0_count > 9)
            return 1;
        return 0;
    case 1:
        if (save.pet.shop_count > 1)
            return 1;
        return 0;
    case 2:
        if (save.pet.tamamori_count > 1)
            return 1;
        return 0;
    case 3:
        if (save.pet.restaurant_count > 9)
            return 1;
        return 0;
    }
    return 0;
}

/*
 * Number of activities with enough experience (pet_activity_done).
 */
u8 pet_activity_done_count(void)
{
    u8 n = 0;

    if (pet_activity_done(0))
        n++;
    if (pet_activity_done(1))
        n++;
    if (pet_activity_done(2))
        n++;
    if (pet_activity_done(3))
        n++;
    return n;
}

/*
 * pet_activity_done as 0/1.
 */
int pet_activity_done_bool(int k)
{
    switch (k) {
    case 0: {
        u8 x = pet_activity_done(0);
        if (x != 0)
            return 1;
        return 0;
    }
    case 1: {
        u8 x = pet_activity_done(1);
        if (x != 0)
            return 1;
        return 0;
    }
    case 2: {
        u8 x = pet_activity_done(2);
        if (x != 0)
            return 1;
        return 0;
    }
    case 3: {
        u8 x = pet_activity_done(3);
        if (x != 0)
            return 1;
        return 0;
    }
    }
    return 0;
}

/*
 * Animation with the empty picture D_2258DF4 in game slot 2 (anim_travel_walk/func_2472838).
 */
void anim_place_enter_blank(const u8 *p)
{
    anim_pic_slots[2][1] = D_2258DF4;
    anim_travel_walk();
    anim_travel_walk_short(p);
}

/*
 * Encounter street: meet a random character from table t (n entries, never the same as
 * last time, never the companion already adopted, pet.companion_sprite_set): load its sprite, +1
 * happiness; characters 44..48 are the "tama pets" whose friendship item (slot id + 39) is
 * counted up to 3, after which a special-form pet (stage 4) is asked "つれていく?" (take it
 * along?) and the tama pet becomes its companion pet.companion_sprite_set (its sprite set, drawn
 * beside the pet by anim_draw_companion; flag id + 17).
 */
int town_street_encounter(struct EncounterEntry *t, int n)
{
    int r;

again:
    town_last_street_chara = util_rand_except(n, town_last_street_chara);
    if (save.pet.companion_sprite_set != 0 && t[town_last_street_chara].sprite_id == save.pet.companion_sprite_set)
        goto again;
    util_rand_except(t[town_last_street_chara].line_count, t[town_last_street_chara].line_count);
    chara_select_other_by_set(t[town_last_street_chara].sprite_id);
    if ((u16)(t[town_last_street_chara].sprite_id - 44) <= 4) {
        r = t[town_last_street_chara].sprite_id + 39;
        item_acquire(r);
        item_get_count(r);
        pet_add_happiness(1);
        if (item_get_count(r) > 2 && save.pet.companion_sprite_set == 0 && save.pet.life_stage == 4) {
            if (ui_confirm_yes_no(D_2542094, 0)) {
                save.pet.companion_sprite_set = t[town_last_street_chara].sprite_id;
                pet_set_unlock_flag(t[town_last_street_chara].sprite_id + 17, 1);
            } else if (ui_timeout == 0) {
                save.pet.companion_sprite_set = 0;
                save_game();
                return 0;
            }
        }
        save_game();
    }
    visit_load_partner_pics(1);
    anim_happy();
    return 1;
}

/*
 * Pick a random purchasable built-in item across the four shop banks D_2541D20[i] (bank
 * ids) with the filters D_2541D24[i] and make it the shop's featured item (loaded with
 * item_bank_record).
 */
void shop_pick_featured_item(void)
{
    u32 i;
    int n = 0;
    int k, best, t;

    for (i = 0; i <= 3; i++)
        n += item_bank_count(D_2541D20[i], D_2541D24[i]);
    n = util_rand_except(n, n);
    for (i = 0; i <= 3; i++) {
        k = D_2541D20[i];
        best = item_bank_next_occupied(k, item_bank_tbl[k].slot_count + item_bank_tbl[k].builtin_count, D_2541D24[i]);
        for (;;) {
            if (n == 0)
                goto done;
            n--;
            t = item_bank_next_occupied(k, best, D_2541D24[i]);
            if (t < best)
                break;
            best = t;
        }
    }
done:
    item_bank_record(k, best);
}

/*
 * Returns 0 (stub entry of a callback table).
 */
int town_park_event_nobody(void)
{
    return 0;
}

/*
 * Park square: a character chosen by the hour (tables town_park_charas_morning 7-10h, town_park_charas_midday 11-15h,
 * town_park_charas_evening 16-18h, town_park_charas_night otherwise; {sprite id, first line, line count}) greets us
 * with one of its lines (text group 100).
 */
int town_park_greeting(void)
{
    u8 m;
    int n;
    struct EncounterEntry *t;
    struct EncounterEntry *p;
    int w;

    m = rtc_get_hour();
    rtc_get_day();
    if (m >= 7 && m <= 10) {
        t = (struct EncounterEntry *)town_park_charas_morning;
        n = 2;
    } else if (m >= 11 && m <= 15) {
        t = (struct EncounterEntry *)town_park_charas_midday;
        n = 2;
    } else if (m >= 16 && m <= 18) {
        t = (struct EncounterEntry *)town_park_charas_evening;
        n = 2;
    } else {
        t = (struct EncounterEntry *)town_park_charas_night;
        n = 2;
    }
    { int k = rand() % n; p = (struct EncounterEntry *)(k * 4 + (int)t); }
    chara_select_other_by_set(p->sprite_id);
    w = rand() % p->line_count + p->first_line;
    anim_park_guest_enter();
    ui_show_message(0x16, 0, chara_text(0, 100, w));
    anim_park_guest_happy();
    anim_park_guest_leave();
    return 1;
}

/*
 * Give the player built-in item k (item_builtin_records[k], the table of item records in ROM;
 * stored with item_add_to_inventory).
 */
void item_give_builtin(int k)
{
    struct TamagoFile *p = item_builtin_records[k];

    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 1;
    snd_wait_end();
    if (item_add_to_inventory(p) == 0)
        ui_timeout = 10;
}

/* per-file alias: town_visitor_table is defined (as data) later in the merged file */
struct VisitorEntry {
    u16 chara_id;
    u32 gift_item;
    u32 greeting_line;
    u32 gift_line;
};
extern const struct VisitorEntry town_visitor_table[];


/*
 * Character id -> its entry in the table town_visitor_table (16-byte rows: id, value, x, y); *x/*y
 * filled; 0x80 if the id is not listed (asm).
 */
int town_visitor_entry(u16 id, int *x, int *y)
{
    int i;

    for (i = 0;; i++) {
        if (town_visitor_table[i].chara_id == 0xffff)
            return 0x80;
        if (town_visitor_table[i].chara_id == id) {
            *x = town_visitor_table[i].greeting_line;
            *y = town_visitor_table[i].gift_line;
            return town_visitor_table[i].gift_item;
        }
    }
}

/*
 * List the unlocked characters (except id skip) into ids[] with the three values
 * town_visitor_entry (asm) gives for each in va / vb / vc (0x80 = not available: fall back to
 * the character's family x006). Returns the count.
 */
int town_list_unlocked_visitors(u16 *ids, u32 *va, u32 *vb, u32 *vc, u16 skip)
{
    u32 x, y;
    int n = 0;
    int i = 0;
    int j;
    u16 id;
    u16 v;

    for (;; i++) {
        if (chara_table[i].id == 0xffff)
            return n;
        if (chara_table[i].unlock_flag == 0xffff)
            continue;
        if (!pet_get_unlock_flag(chara_table[i].unlock_flag))
            continue;
        id = chara_table[i].id;
        v = town_visitor_entry(id, (int)&x, (int)&y);
        if (id == skip)
            continue;
        if (v == 0x80) {
            id = chara_table[i].family;
            v = town_visitor_entry(id, (int)&x, (int)&y);
            if (id == skip)
                continue;
            if (v == 0x80)
                continue;
        }
        for (j = 0; j < n; j++) {
            if (ids[j] == id) {
                j = -1;
                break;
            }
        }
        if (j == -1)
            continue;
        ids[j] = id;
        va[j] = v;
        vb[j] = x;
        vc[j] = y;
        n++;
    }
}


/*
 * List helper for the visitors (asm).
 */
int town_park_event_friend(void)
{
    u16 ids[13];
    u32 va[13];
    u32 vb[13];
    u32 vc[13];
    int i, n, m, flag;
    u8 *t;

    for (i = 0; i <= 12; i++) {
        ids[i] = 0xffff;
        va[i] = -1;
        vb[i] = -1;
        vc[i] = -1;
    }
    n = town_list_unlocked_visitors(ids, va, vb, vc, 0xffff);
    m = town_list_unlocked_visitors(ids, va, vb, vc, chara_family(save.pet.x000));
    flag = 0;
    if (n > 11) {
        if (rand() % 100 < 30)
            flag = 1;
        else
            flag = 0;
    }
    m = rand() % m;
    chara_select_other_by_id(ids[m]);
    if (flag == 0) {
        anim_park_guest_enter();
        t = chara_text(0, 100, vb[m]);
        ui_show_message(0x16, 0, t);
        anim_visitor_list_event();
    } else {
        anim_park_guest_enter();
        t = chara_text(0, 100, vc[m]);
        ui_show_message(0x16, 0, t);
        item_load_pictures(anim_pic_slots[0], (struct TamagoFile *)item_builtin_records[va[m]], 0xf);
        anim_visitor_list_end(anim_pic_slots[0][0]);
        item_give_builtin(va[m]);
    }
    anim_park_guest_happy();
    anim_park_guest_leave();
    return 1;
}

/*
 * Park square event 3: the pet meets its own contact character (a friend, idol, rival or
 * mentor tied to the pet's character id; table town_park_contacts: {character id, contact
 * sprite, first line of text group 100, line count}) and talks; first sets the
 * park-meeting flag of the special-form routes 5 / 6 / 7 (evo_note_park_meeting).
 * Characters 0x12 / 0x7D5 / 0x7D6 get the second scene variant (mode 1).
 */
int town_park_meet_contact(void)
{
    int mode;
    int i;
    u16 k;
    const struct ParkContact *e;
    u16 a;
    u8 b;
    int c;
    u8 w;

    evo_note_park_meeting();
    k = save.pet.x000;
    if (k == 0x12 || k == 0x7d5 || k == 0x7d6)
        mode = 1;
    else
        mode = 0;
    for (i = 0;; i++) {
        if (town_park_contacts[i].chara_id == k)
            break;
        if (town_park_contacts[i].chara_id == 0xffff)
            break;
    }
    e = &town_park_contacts[i];
    a = e->visitor_sprite;
    b = e->first_line;
    c = e->line_count;
    w = b + rand() % c;
    switch (mode) {
    case 0:
        chara_select_other_by_set(a);
        anim_park_guest_enter();
        ui_show_message(0x16, 0, chara_text(0, 100, w));
        anim_park_guest_happy();
        anim_park_guest_leave();
        break;
    case 1:
        anim_park_guest2_enter();
        ui_show_message(0x17, 0, chara_text(0, 100, w));
        anim_park_guest2_happy();
        anim_park_guest2_leave();
        break;
    }
    return 1;
}

/*
 * Sell ("うる"): "どれをうる?" choose among the three inventories item_inv_clothes / item_inv_accessories /
 * item_inv_tools (clothes / accessories / play tools, with counts, greyed when empty; menu
 * ui_three_entry_menu), pick an item (item_inv_picker), "これをうる? はい/いいえ", then the item is
 * consumed and its sell price (item_price) added. 1 sold, 2 back, 3 cancelled / timeout.
 */
int shop_sell(void)
{
    int sel = 0;
    int t;
    int x;
    int a, b, c;
    int r;
    u32 s;

again:
    ui_new_screen();
    a = item_inv_usable_count(&item_inv_clothes);
    b = item_inv_usable_count(&item_inv_accessories);
    c = item_inv_usable_count(&item_inv_tools);
    switch ((u32)ui_three_entry_menu((int)D_254223C, (int)item_inv_clothes.name, a ? 0 : 3, (int)item_inv_accessories.name,
                              b ? 0 : 3, (int)item_inv_tools.name, c ? 0 : 3, 0, (int)&sel)) {
    case 1:
        break;
    case 2:
        return 2;
    case 3:
        return 3;
    default:
        return 1;
    }
    for (;;) {
        t = -1;
        shop_set_discount_mode(-1);
        r = item_inv_picker(&item_inventories[D_2541D28[sel]], 0, &t);
        shop_set_discount_mode(0);
        if (r == -1)
            goto again;
        if (r == -2)
            return 3;
        x = 1;
        ui_timeout = 10;
        switch (s = ui_yes_no(D_254224C, D_254225C, D_2542264, 16, &x)) {
        case 1:
            if (x != 0)
                continue;
            item_consume(r);
            pet_add_points((u16)item_price(item_record_by_index(r)));
            anim_shopkeeper_sell();
            save_game();
            return 1;
        case 2:
            continue;
        case 3:
            return s;
        default:
            continue;
        }
    }
}

const u8 D_254184C[] = _("もっといっしょにい\nてくれたら､こんな\nけつだんをせずにすんだかも\n…こんどあえたらそのときは\nもっともっとあそぼうね……");
const u8 D_25418C8[] = _("アルバイトのぼしゅうがあ\nるよ!キミもやっちゃう?");
const u8 D_25418FC[] = _("いまはアルバイトの\nぼしゅうがないっスよ↓");
const u8 D_2541928[] = _("キミにはアルバイトは\nまだはやいみたいっスね↓");
const u8 D_2541958[] = _("こまってるたまごっちがい\nたらたすけてあげてぱと!");
const u8 D_254198C[] = _("パトロールおつかれぱと!\nまたよろしくぱと☼");
const u8 D_25419BC[] = _("パトロールおつかれぱと!\nこれをどうぞぱと☼");
const u8 D_25419EC[] = _("がんばってくれたごほうび\nに､これもあげるぱと♪");
const u8 D_2541A20[] = _("うってうってうりまくって\nくださいませ～♪");
const u8 D_2541A4C[] = _("みせばんおつかれさま～\nまたおねがいしま～す♥");
const u8 D_2541A7C[] = _("みせばんおつかれさま～\nこれをさしあげま～す♥");
const u8 D_2541AAC[] = _("がんばったごほうびに\nこちらもさしあげま～す☼");
const u8 D_2541ADC[] = _("ステキにコーディネート\nしてあげてね★");
const u8 D_2541B04[] = _("みせばんおつかれさま★\nまたおねがいね♪");
const u8 D_2541B30[] = _("みせばんおつかれさま★\nよかったらこれどうぞ♪");
const u8 D_2541B60[] = _("がんばったごほうびに\nこれもあげるわ★");
const u8 D_2541B88[] = _("えがおでせっきゃく\nおねがいね♪");
const u8 D_2541BAC[] = _("おつかれさま♥\nまたおねがいね!");
const u8 D_2541BD0[] = _("おつかれさま♥\nこれどうぞ♪");
const u8 D_2541BF0[] = _("がんばったごほうびにこれ\nどうぞ♪またおねがいね!");
const u8 D_2541C24[] = _("おめでとー!!!\nエコカーをプレゼント!");
const u8 D_2541C50[] = _("エコバッグを\nプレゼントだよー");
const u8 D_2541C70[] = _("エコスコップを\nプレゼントだよー");
const u8 *const D_2541C94[] = {
    D_254184C, D_25418C8, D_25418FC, D_2541928,
    D_2541958, D_254198C, D_25419BC, D_25419EC,
    D_2541A20, D_2541A4C, D_2541A7C, D_2541AAC,
    D_2541ADC, D_2541B04, D_2541B30, D_2541B60,
    D_2541B88, D_2541BAC, D_2541BD0, D_2541BF0,
    D_2541C24, D_2541C50, D_2541C70
};
const u8 D_2541CF0[] = { 0x70, 0x70, 0x70, 0x70 };
const struct EncounterEntry town_park_charas_morning[] = { { 0x3e, 3, 4 }, { 0x41, 7, 4 } };
const struct EncounterEntry town_park_charas_midday[] = { { 0x2d, 11, 4 }, { 0x42, 15, 4 } };
const struct EncounterEntry town_park_charas_evening[] = { { 0x3d, 19, 4 }, { 0x40, 23, 4 } };
const struct EncounterEntry town_park_charas_night[] = { { 0x2d, 27, 4 }, { 0x3f, 31, 4 } };
const struct ShopKeeper shop_keepers[] = { { 0x48, 0, 1 }, { 0x4a, 2, 1 }, { 0x49, 1, 1 } };
const u8 D_2541D20[] = { 2, 3, 5, 4 };
const u8 D_2541D24[] = { 0x33, 0x34, 0x29, 0x2a };
const u8 D_2541D28[] = { 2, 3, 1, 0 };
const u8 D_2541D2C[] = _("たまデパ");
const u8 D_2541D38[] = _("こうえん");
const u8 D_2541D44[] = _("がっこう");
const u8 D_2541D50[] = _("おみあいじょ");
const u8 D_2541D60[] = _("たまのうえん");
const u8 D_2541D70[] = _("であいストリート");
/*
 * town_places (in this data block): the town places (16 entries of 20 bytes: name text,
 * availability callback x004 (-1 hidden, 0..2 shown), enter callbacks x008/x00C/x010);
 * entries 8..13 are the downloadable places of category 8 (town_menu).
 */
const struct PlaceEntry town_places[] = {
    { D_2541D2C, (int (*)(void))town_always_open_b, (u8 (*)(void))town_enter_tamadepa, shop_floor_menu, 0 },
    { D_2541D38, (int (*)(void))town_always_open, town_enter_park, (u8 (*)(void))town_park_menu, 0 },
    { D_2541D44, (int (*)(void))town_always_open_c, (u8 (*)(void))town_enter_school, (u8 (*)(void))town_school, 0 },
    { D_2541D50, (int (*)(void))town_always_open_d, (u8 (*)(void))town_enter_matchmaking, (u8 (*)(void))town_matchmaking, 0 },
    { D_2541D60, town_farm_state_cb, (u8 (*)(void))town_enter_farm, (u8 (*)(void))town_farm_visit, 0 },
    { D_2541D70, town_street_state_cb, (u8 (*)(void))town_enter_street, (u8 (*)(void))town_street_visit, 0 },
    { 0 },
    { 0 },
    { 0, (int (*)(void))town_app_slot0_state, (u8 (*)(void))town_enter_nopic, 0, 0 },
    { 0, (int (*)(void))town_app_slot1_state, (u8 (*)(void))town_enter_nopic, 0, 0 },
    { 0 },
    { 0 },
    { 0, (int (*)(void))town_app_slot2_state, (u8 (*)(void))town_enter_nopic, 0, 0 },
    { 0, (int (*)(void))town_app_slot3_state, (u8 (*)(void))town_enter_nopic, 0, 0 },
    { 0 },
    { 0 }
};
const u8 D_2541EC4[] = _("ゲームコーナー");
const u8 D_2541ED4[] = _("リフォーム");
const u8 D_2541EE0[] = _("アイテムショップ");
const u8 D_2541EF4[] = _("たまファッション");
const struct PlaceEntry shop_floor_menu_tbl[] = {
    { D_2541EC4, (int (*)(void))town_always_open, (u8 (*)(void))town_depa_floor_enter, (u8 (*)(void))town_game_corner, town_place_leave },
    { D_2541ED4, (int (*)(void))town_always_open, (u8 (*)(void))town_depa_floor_enter, (u8 (*)(void))town_reform_shop, town_place_leave },
    { D_2541EE0, (int (*)(void))town_always_open, (u8 (*)(void))town_depa_floor_enter, (u8 (*)(void))town_item_shop, town_place_leave },
    { D_2541EF4, (int (*)(void))town_always_open, (u8 (*)(void))town_depa_floor_enter, (u8 (*)(void))town_fashion_shop, town_place_leave }
};
const u8 D_2541F58[] = _("ようふく");
const u8 D_2541F64[] = _("アクセサリー");
const u8 D_2541F74[] = _("あそびどうぐ");
const struct BankListEntry shop_bank_list[] = {
    { D_2541F58, 0x13, 0x33 },
    { D_2541F64, 0x14, 0x34 },
    { D_2541F74, 0x15, 0x2a }
};
const u8 D_2541FA8[] = _("おせわマーク");
const u8 D_2541FB8[] = _("デザイン");
const u8 D_2541FC4[] = _("ゲットしました!");
const u8 D_2541FD8[] = _("");
const u8 D_2541FDC[] = _("     ");
const u8 D_2541FE8[] = _("         ");
const u8 D_2541FFC[] = _("        ");
const u8 D_2542010[] = _("どうする?");
const u8 D_254201C[] = _("かえる");
const u8 D_2542024[] = _("かえらない");
const u8 D_2542030[] = _("つぼみ☼こうえん");
const u8 D_2542044[] = _("たまもりショップ");
const u8 D_2542058[] = _("♪カフェ♪");
struct PlaceListMenu {
    u32 count;
    int (*x004)(int);
    struct {
        const u8 *count;
        u32 x004;
    } entries[5];
};
const struct PlaceListMenu D_2542064 = {
    4, pet_activity_done_bool,
    { { D_2542030, 0 }, { D_2541D2C, 0 }, { D_2542044, 0 }, { D_2542058, 0 }, { 0, 0 } }
};
const u8 D_2542094[] = _("つれていく?");
/* character id -> x004 (0x80 = not listed), x008, x00c (town_visitor_entry) */
const struct VisitorEntry town_visitor_table[] = {
    { 0x0010, 0x5e, 0x23, 0x24 },
    { 0x0011, 0x7b, 0x25, 0x26 },
    { 0x0012, 0x0e, 0x27, 0x28 },
    { 0x0014, 0x2e, 0x29, 0x2a },
    { 0x0013, 0x0d, 0x2b, 0x2c },
    { 0x03eb, 0x0c, 0x2d, 0x2e },
    { 0x03ea, 0x45, 0x2f, 0x30 },
    { 0x03ed, 0x2d, 0x31, 0x32 },
    { 0x03e9, 0x7f, 0x33, 0x34 },
    { 0x03ec, 0x7a, 0x35, 0x36 },
    { 0x0015, 0x7c, 0xe1, 0xe2 },
    { 0x03ee, 0x5f, 0xe3, 0xe4 },
    { 0xffff, 0x80, 0x00, 0x00 }
};
const struct ParkContact town_park_contacts[] = {
    { 0x0010, 0x24, 0x37, 1 },
    { 0x07d1, 0x24, 0x38, 1 },
    { 0x07d2, 0x24, 0x39, 1 },
    { 0x0011, 0x4b, 0x3a, 1 },
    { 0x07d3, 0x4b, 0x3b, 1 },
    { 0x07d4, 0x4b, 0x3c, 1 },
    { 0x0013, 0x44, 0x3d, 1 },
    { 0x07d7, 0x44, 0x3e, 1 },
    { 0x07d8, 0x44, 0x3f, 1 },
    { 0x0012, 0x00, 0x40, 1 },
    { 0x07d5, 0x00, 0x41, 1 },
    { 0x07d6, 0x00, 0x42, 1 },
    { 0x0014, 0x1e, 0x43, 1 },
    { 0x07d9, 0x1e, 0x44, 1 },
    { 0x03eb, 0x50, 0x45, 1 },
    { 0x0bbc, 0x50, 0x46, 1 },
    { 0x0bbd, 0x50, 0x47, 1 },
    { 0x03ec, 0x2d, 0x48, 1 },
    { 0x0bbe, 0x2d, 0x49, 1 },
    { 0x03e9, 0x47, 0x4a, 1 },
    { 0x0bb9, 0x47, 0x4b, 1 },
    { 0x0bba, 0x47, 0x4c, 1 },
    { 0x03ea, 0x0c, 0x4d, 1 },
    { 0x0bbb, 0x0c, 0x4e, 1 },
    { 0x03ed, 0x0f, 0x4f, 1 },
    { 0x0bbf, 0x0f, 0x50, 1 },
    { 0x0bc1, 0x2d, 0xe5, 1 },
    { 0x0015, 0x2e, 0xe6, 1 },
    { 0x07da, 0x2e, 0xe7, 1 },
    { 0x0bc2, 0x0c, 0xe8, 1 },
    { 0x03ee, 0x23, 0xe9, 1 },
    { 0x0bc0, 0x23, 0xea, 1 },
    { 0xffff, 0x0c, 0x51, 1 }
};
const u8 D_254223C[] = _("どれをうる?");
const u8 D_254224C[] = _("これをうる?");
const u8 D_254225C[] = _("はい");
const u8 D_2542264[] = _("いいえ");
const u8 D_254226C[] = _("なにをする?");
const u8 D_254227C[] = _("かう");
const u8 D_2542284[] = _("うる");

/*
 * Tama Depa: one of three shopkeepers (shop_keepers) greets, then "なにをする? かう/うる"
 * (dialog D_254226C..): buy through the shop browser shop_browser(shop_bank_list = the shop's
 * bank list, ...) or sell (shop_sell).
 */
int shop_tama_depa(void)
{
    int sel = 0;
    int t;
    u8 k;
    const struct ShopKeeper *e;
    u16 a;
    int b;
    int r;

    ui_new_screen();
    k = rand() % 3;
    e = &shop_keepers[k];
    a = e->sprite_id;
    b = e->first_line;
    chara_select_other_by_set(a);
    anim_park_guest_enter();
    ui_show_message(0x16, 0, chara_text(0, 100, b));
again:
    switch (ui_yes_no(D_254226C, D_254227C, D_2542284, 16, &sel)) {
    case 1:
        switch (sel) {
        case 0:
            t = -1;
            shop_set_discount_mode(2);
            r = shop_browser(shop_bank_list, k, 1, &t, anim_shopkeeper_buy, 0);
            shop_set_discount_mode(0);
            switch (r) {
            case 0:
                return 3;
            case 1:
            case 2:
                shop_set_discount_mode(0);
                return 1;
            case 3:
            case 4:
                goto again;
            case 5:
            case 6:
            default:
                return 3;
            }
        case 1:
            r = shop_sell();
            if (r != 2)
                return r;
            goto again;
        }
    case 2:
        return 2;
    case 3:
        return 3;
    default:
        shop_set_discount_mode(0);
        return 1;
    }
}

/*
 * Lottery (くじ): three animations, 10 % first prize (flag 0x15, picture 0x21B4044), 30 %
 * second (0x16), else third (0x14); the flag is an achievement, lines 0x52..0x56.
 */
int town_lottery(void)
{
    u8 r;
    int img;
    int id;

    anim_lottery_enter();
    ui_show_message(0x18, 0, chara_text(0, 100, 0x52));
    anim_lottery_draw();
    ui_show_message(0x18, 0, chara_text(0, 100, 0x53));
    r = rand() % 100;
    if (r <= 9) {
        img = 0x21b4044;
        id = 0x15;
        ui_show_message(0x18, 0, chara_text(0, 100, 0x54));
    } else if (r <= 39) {
        img = 0x21b4434;
        id = 0x16;
        ui_show_message(0x18, 0, chara_text(0, 100, 0x55));
    } else {
        img = 0x21b3e40;
        id = 0x14;
        ui_show_message(0x18, 0, chara_text(0, 100, 0x56));
    }
    pet_set_unlock_flag(id, 1);
    anim_lottery_result(img);
    anim_lottery_prize((const u8 *)img);
    anim_lottery_leave();
    return 1;
}

extern int (*T_84498[])(void) __asm__("D_0084498");
/*
 * Park square (ひろば) event picker by hour / day: night (0-6, 22-23 h) = nobody or a
 * visitor (handlers 0 / 2 / 3); the 10th, 20th and 30th between 10 and 14 h = the
 * once-a-day event 4 (flag 0x44), 7-8 h once a day = event 5 (flag 0x4A); otherwise 10 %
 * nothing, 20 % (adults) a relative, else a marriage candidate (chara_list_marriage_candidates found one)
 * or an encounter (album friends, random). T_84498 = the six handlers in RAM
 * (D_0084498); returns the handler's result (0..3).
 */
int town_park_square_event(void)
{
    u16 buf[14];
    u32 a0;
    u8 a1, ok;
    int v;
    int n;

    a0 = rtc_get_hour();
    a1 = rtc_get_day();
    ui_new_screen();
    ok = chara_list_marriage_candidates(buf, save.pet.x000);
    switch (a0) {
    case 0 ... 6:
    case 22:
    case 23:
        v = rand() % 100;
        if (ok != 0) {
            if (save.pet.life_stage > 2) {
                if (v < 30)
                    n = 0;
                else if (v < 60)
                    n = 3;
                else
                    n = 2;
            } else {
                if (v < 50)
                    n = 0;
                else
                    n = 2;
            }
        } else {
            n = 0;
            if (save.pet.life_stage > 2 && v >= 50)
                n = 3;
        }
        break;
    default:
        if (a1 == 10 || a1 == 20 || a1 == 30) {
            switch (a0) {
            case 10 ... 14:
                if (pet_get_unlock_flag(0x44) == 0) {
                    pet_set_unlock_flag(0x44, 1);
                    n = 5;
                    goto done;
                }
            }
        }
        switch (a0) {
        case 7 ... 8:
            if (pet_get_unlock_flag(0x4a) == 0) {
                pet_set_unlock_flag(0x4a, 1);
                n = 4;
                goto done;
            }
        }
        if (rand() % 100 < 10)
            n = 0;
        else if (save.pet.life_stage >= 3 && save.pet.life_stage <= 4 && rand() % 100 < 20)
            n = 3;
        else if (ok == 0)
            n = 1;
        else if (rand() % 100 < 50)
            n = 2;
        else
            n = 1;
        break;
    }
done:
    return T_84498[n]();
}

/*
 * Sports ground: the training mini game game_training (-1 cancelled, 0 fail, 1..4+ ranks)
 * pays 20 / 60 / 100 / 140 / 200 Gotchi points with hearts (happiness) and a weight loss
 * of 2..10.
 */
void town_sports_ground(void)
{
    ui_new_screen();
    anim_sports_enter();
    ui_show_message(0x19, 0, chara_text(0, 100, 0x57));
    anim_sports_start();
    switch (game_training()) {
    case -1:
        return;
    case 0:
        ui_show_message(0x19, 0, chara_text(0, 100, 0x5a));
        anim_sports_lose();
        break;
    case 1:
        ui_show_message(0x19, 0, chara_text(0, 100, 0x59));
        pet_add_happiness(1);
        pet_sub_weight(2);
        anim_sports_points(0x14);
        anim_sports_ok();
        pet_add_points(0x14);
        break;
    case 2:
        ui_show_message(0x19, 0, chara_text(0, 100, 0x59));
        pet_add_happiness(1);
        pet_sub_weight(4);
        anim_sports_points(0x3c);
        anim_sports_ok();
        pet_add_points(0x3c);
        break;
    case 3:
        ui_show_message(0x19, 0, chara_text(0, 100, 0x59));
        pet_add_happiness(1);
        pet_sub_weight(6);
        anim_sports_points(0x64);
        anim_sports_ok();
        pet_add_points(0x64);
        break;
    case 4:
        ui_show_message(0x19, 0, chara_text(0, 100, 0x59));
        pet_add_happiness(2);
        pet_sub_weight(8);
        anim_sports_points(0x8c);
        anim_sports_win();
        pet_add_points(0x8c);
        break;
    default:
        ui_show_message(0x19, 0, chara_text(0, 100, 0x58));
        pet_add_happiness(3);
        pet_sub_weight(10);
        anim_sports_points(0xc8);
        anim_sports_prize();
        pet_add_points(0xc8);
        break;
    }
    save_game();
}

/*
 * Park menu "どっちにいく? ひろば/うんどうじょう" (dialog D_25422C0): the square runs
 * town_park_square_event (result -1 or +1 happiness), the sports ground town_sports_ground; the
 * free-running timer-2 counter seeds the RNG (srand).
 */
int town_park_menu(void)
{
    int sel = 0;

again:
    srand(T16_TC2);
    switch (ui_yes_no(D_25422C0, D_25422D0, D_25422D8, 16, &sel)) {
    case 1:
        switch (sel) {
        case 0:
            switch ((u32)town_park_square_event()) {
            case 0:
                anim_park_enter();
                anim_sad_park();
                pet_sub_happiness(1);
                break;
            case 1:
                anim_happy();
                pet_add_happiness(1);
                break;
            case 2:
                goto again;
            default:
                return 0;
            }
            save_game();
            return 1;
        case 1:
            town_sports_ground();
            goto again;
        }
        break;
    case 2:
        return 0;
    case 3:
        return 0;
    }
}

/*
 * Map special character ids 0x16/0x1A/0x29 to 0x57/0x58/0x59 (inverse of chara_canonical_id).
 */
int chara_id_alias(u8 c)
{
    switch (c) {
    case 0x16:
        return 0x57;
    case 0x1a:
        return 0x58;
    case 0x29:
        return 0x59;
    }
    return c;
}

/*
 * Shuffle n halfwords.
 */
void util_shuffle_u16(u16 *buf, int n)
{
    u16 j, i, k, cnt;
    u16 t;
    int r;

    if (n <= 1)
        return;
    j = n;
    if (n == 2) {
        r = rand() & 1;
        cnt = 1;
        if (r == 0)
            return;
    } else
        cnt = j;
    for (k = 0; k < cnt; k++) {
        j = util_rand_except(n, j);
        i = util_rand_except(n, j);
        t = buf[j];
        buf[j] = buf[i];
        buf[i] = t;
    }
}

/*
 * The pet's life story, when all four clover leaves are filled (pet.clover_leaves bits 0-3):
 * the baby pet.baby_id, the child pet.child_id (with an extra scene when all three habits
 * are learned), the adult pet.adult_id, then the special form pet.special_id or the adult's
 * ending; played by the see-off (pet_see_off) and after the marriage choice.
 */
void pet_life_story(void)
{
    if ((save.pet.clover_leaves & 0xf) != 0xf)
        return;
    ui_new_screen();
    chara_select_other_by_id(save.pet.baby_id);
    anim_life_baby();
    chara_select_other_by_id(save.pet.child_id);
    anim_life_child();
    if ((save.pet.training[0] & 0xffffff) == 0xffffff) {
        chara_select_other_by_id(save.pet.child_id);
        anim_life_child_habits();
    }
    chara_select_other_by_id(save.pet.adult_id);
    anim_life_adult();
    if (save.pet.special_id != 0) {
        chara_select_other_by_id(save.pet.special_id);
        anim_life_special();
    } else {
        chara_select_other_by_id(save.pet.adult_id);
        anim_life_adult_end();
    }
}

/*
 * Matchmaking (おみあい) candidates: pick the candidate species (six per gender:
 * town_match_species_a for family != 0, town_match_species_b otherwise, excluding our own species / its twin
 * id, with special cases for some pets), shuffle, present the first three with their
 * proposal lines (text group 4).
 */
u8 town_match_candidates(u8 *p)
{
    u16 *buf = (u16 *)p;
    int i, j;

    if ((u8)(btn_a_presses | btn_b_presses)) {
        btn_a_presses = 0;
        btn_b_presses = 0;
    }
    if (btn_c_presses) {
        ui_timeout = 10;
        return 0;
    }
    if (pet_get_gender()) {
        for (j = i = 0; i <= 5; i++) {
            if ((chara_family(save.pet.x000) == 0x3ed && town_match_species_a[i] == 0x14)
                || (chara_family(save.pet.x000) == 0x3eb && town_match_species_a[i] == 0x15)
                || (save.pet.x000 == 0x1b59 && town_match_species_a[i] == 0x10)
                || (save.pet.x000 == 0x1b5a && town_match_species_a[i] == 0x11)
                || (save.pet.x000 == 0x1b5b && town_match_species_a[i] == 0x12)
                || (save.pet.x000 == 0x1b5c && town_match_species_a[i] == 0x13))
                continue;
            buf[j] = town_match_species_a[i];
            j++;
        }
    } else {
        for (j = i = 0; i <= 5; i++) {
            if ((chara_family(save.pet.x000) == 0x14 && town_match_species_b[i] == 0x3ed)
                || (chara_family(save.pet.x000) == 0x15 && town_match_species_b[i] == 0x3eb)
                || (save.pet.x000 == 0x1389 && town_match_species_b[i] == 0x3e9)
                || (save.pet.x000 == 0x138a && town_match_species_b[i] == 0x3ea)
                || (save.pet.x000 == 0x138b && town_match_species_b[i] == 0x3eb)
                || (save.pet.x000 == 0x138c && town_match_species_b[i] == 0x3ec))
                continue;
            buf[j] = town_match_species_b[i];
            j++;
        }
    }
    util_shuffle_u16(buf, j);
    for (i = 0; i <= 2; i++) {
        chara_select_other_by_id(buf[i]);
        anim_matchmaking_candidate();
        ui_show_message(0x15, 0, chara_text(buf[i], 4, 0));
        chara_select_other_by_id(buf[i]);
        anim_matchmaking_candidate_leave();
    }
    anim_matchmaking_candidate2();
    ui_show_message(0x14, 0, chara_text(0, 100, 0xb2));
    return 1;
}

/*
 * Partner choice screen (three candidates with pictures, A/B/C): "おみあい する/しない";
 * a choice leads to the wedding (anim_wedding, the life story pet_life_story) and the new generation
 * (pet_pick_variant_flag, pet_start_next_generation, anim_new_egg).
 */
u8 town_match_choose(u8 *p)
{
    u8 arr[3];
    int res;
    u8 j;
    u8 sel;
    u8 id;
    int i;

    sel = 0;
again:
    j = 0;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_20C3964);
    for (i = 0; i <= 2; i++) {
        arr[i] = gfx_object_create(0, 0, (u16)(i * 48 + 0x20), 0, 0, 0);
        gfx_object_create(0, 0, (u16)(i * 48 + 0x20), 0, 0, 0);
        gfx_object_create(0, 0, (u16)(i * 48 + 0x20), 0, 0, 0);
        chara_draw_any(arr[i], i * 32 + 0x20, i & 1, 0, i * 40 + 0x17, 0x36,
                     chara_index_by_id(((u16 *)p)[i]));
    }
    id = gfx_object_create(sel * 40 + 0x12, 0x27, 0x80, 0, 0, D_21A8F14);
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    tmr_t5_start_timeout(500);
    do {
        if (btn_a_presses) {
            sel++;
            if (sel > 2)
                sel = 0;
            gfx_object_move_dirty(id, sel * 40 + 0x12, 0x27);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            res = 1;
            if (ui_yes_no(D_2542300, D_254230C, D_2542314, 0x10, &res) == 1 && res == 0) {
                chara_select_other_by_id(((u16 *)p)[sel]);
                anim_wedding();
                pet_life_story();
                chara_select_other_by_id(((u16 *)p)[sel]);
                anim_wedding_end();
                pet_pick_variant_flag();
                pet_start_next_generation();
                rtc_seconds = 0;
                ui_main_icon = 0;
                anim_new_egg();
                rtc_seconds = 0;
                return 1;
            }
            if (ui_timeout == 0)
                break;
            goto again;
        }
        if (btn_c_presses) {
            ui_timeout = 10;
            sys_idle_seconds = 0;
            return 0;
        }
        if (tmr_flags & 0x20) {
            tmr_flags &= ~0x20;
            for (i = 0; i <= 2; i++)
                chara_draw_any(arr[i], i * 32 + 0x20, (i ^ j) & 1, 0, i * 40 + 0x17, 0x36,
                             chara_index_by_id(((u16 *)p)[i]));
            func_00009A6();
            j++;
        }
        sys_idle_tick();
    } while (ui_timeout);
    if (ui_timeout == 0) {
        sys_idle_seconds = 10;
        return 0;
    }
    ui_timeout = 10;
    sys_idle_seconds = 0;
    return 1;
}

/*
 * The matchmaking parlour entry: adults older than 3 days only (animations, candidate
 * list town_match_candidates, choice town_match_choose); younger ones get a "come back later" scene,
 * babies/children just +1 happiness.
 */
int town_matchmaking(void)
{
    u8 buf[36];

    ui_new_screen();
    if (save.pet.life_stage <= 2 || save.pet.age_days <= 3) {
        anim_matchmaking_parlour();
        pet_add_happiness(1);
        save_game();
        anim_happy();
        return 1;
    }
    anim_matchmaking_enter();
    ui_show_message(0x14, 0, chara_text(0, 100, 0x9f));
    anim_matchmaking_intro();
    if (town_match_candidates(buf) == 0)
        return 1;
    if (town_match_choose(buf) != 0) {
        save_game();
        return 1;
    }
    return 0;
}

const u8 D_25422C0[] = _("どっちにいく?");
const u8 D_25422D0[] = _("ひろば");
const u8 D_25422D8[] = _("うんどうじょう");
const u16 town_match_species_a[] = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15 };
const u16 town_match_species_b[] = { 0x3e9, 0x3ea, 0x3eb, 0x3ec, 0x3ed, 0x3ee };
const u8 D_2542300[] = _("おみあい");
const u8 D_254230C[] = _("する");
const u8 D_2542314[] = _("しない");

/*
 * Transformation ("へんしん") availability: certain characters (0x10, 0x11, 0x15, 0x17...)
 * can transform when they own a specific item (item_inv_find_owned_id(inventory, item id) >= 0;
 * 0x19 needs > 2999 points; 0x59 needs P06 high (test input) or more than 9 friends) ->
 * the id of the transformed form (0x30..0x3C, 0x5A), 0 = not now, -1 = this character
 * never transforms.
 */
int chara_transform_target(void)
{
    switch (pet_get_id()) {
    case 0x10:
        if (item_inv_find_owned_id(&item_inv_accessories, 0x2712) >= 0)
            return 0x30;
        break;
    case 0x11:
        if (item_inv_find_owned_id(&item_inv_tools, 0x271B) >= 0)
            return 0x31;
        break;
    case 0x15:
        if (item_inv_find_owned_id(&item_inv_accessories, 0x2AFA) >= 0)
            return 0x32;
        break;
    case 0x17:
        if (item_inv_find_owned_id(&item_inv_accessories, 0x2AFB) >= 0)
            return 0x33;
        break;
    case 0x19:
        if (save.pet.gotchi_points > 0xbb7)
            return 0x34;
        break;
    case 0x1f:
        if (item_inv_find_owned_id(&item_inv_clothes, 0x2711) >= 0)
            return 0x35;
        break;
    case 0x20:
        if (item_inv_find_owned_id(&item_inv_clothes, 0x2712) >= 0)
            return 0x36;
        break;
    case 0x21:
        if (item_inv_find_owned_id(&item_inv_accessories, 0x2711) >= 0)
            return 0x37;
        break;
    case 0x22:
        if (item_inv_find_owned_id(&item_inv_accessories, 0x2713) >= 0)
            return 0x38;
        break;
    case 0x23:
        if (item_inv_find_owned_id(&item_inv_accessories, 0x2AF9) >= 0)
            return 0x39;
        break;
    case 0x26:
        if (item_inv_find_owned_id(&item_inv_clothes, 0x2AF9) >= 0)
            return 0x3a;
        break;
    case 0x28:
        if (item_inv_find_owned_id(&item_inv_daily, 0x2EE1) >= 0)
            return 0x3b;
        break;
    case 0x2d:
        if (item_inv_find_owned_id(&item_inv_tools, 0x2711) >= 0)
            return 0x3c;
        break;
    case 0x59:
        if ((P0_P0D & 0x40) == 0)
            return 0x5a;
        if (friend_count() > 9)
            return 0x5a;
        break;
    default:
        return -1;
    }
    return 0;
}

/*
 * Transform / untransform by item (chara_transform_target): a transformed character (id 0x30..0x3C
 * or 0x5A) is asked "へんしんかいじょ?" (undo?) and reverts (sprite set reset, pet.idle_pose =
 * 0xFFF forces a reload, pet_select_sprite_set loads the sprites); otherwise "へんしんする?"
 * (ui_confirm_yes_no) sets the new id, resets the worn-item flags pet.worn_accessory/9 and plays the
 * reveal with +2 hearts.
 */
int chara_transform(void)
{
    int k = chara_transform_target();
    u16 *p;

    if (k < 0)
        return 1;
    if ((u16)(save.pet.x000 - 0x30) <= 12 || save.pet.x000 == 0x5a)
        k = save.pet.x000;
    if (k != 0 && save.pet.x000 == k) {
        if (ui_confirm_yes_no(D_2542444, 0)) {
            save.pet.x082 = 0;
            save.pet.x062 = 0;
            save.pet.idle_pose = 0xfff;
            save.pet.x000 = pet_get_id();
            pet_select_sprite_set(save.pet.x000);
        }
    } else if (k != 0 && ui_confirm_yes_no(D_2542458, 0)) {
        p = &save.pet.x000;
        save.pet.x082 = 0;
        save.pet.x062 = 0;
        save.pet.worn_accessory = 0;
        save.pet.worn_clothes = 0;
        save.pet.idle_pose = 0xfff;
        *p = k;
        pet_select_sprite_set(*p);
        pet_add_happiness(2);
        visit_load_partner_pics(1);
        anim_happy();
    }
    return 1;
}

/*
 * Random choice between a and b (50%).
 */
int util_pick_one_of_two(int a, int b)
{
    int t = util_rand_except(100, 100);

    if (t <= 49)
        return a;
    return b;
}

const u8 D_2542444[] = _("へんしんかいじょ?");
const u8 D_2542458[] = _("へんしんする?");

/*
 * Outing record outing (away mode): x000 = where the pet is: 0 home, 1 the game-centre stay
 * (open 10:00-14:59, 50 points an hour), 2..6 the school, one of its five clubs picked by
 * outing_pick_destination with 20 % each (2 inventions, 3 fashion, 4 cooking, 5 arts,
 * 6 sports: diary pages 4000, 4001, 4004, 4003, 4002). No firmware code sets x000 from 0.
 *
 * Select the companion character / picture of story k (2..).
 */
int outing_story_chara(u8 k)
{
    int r;

    switch (k) {
    case 1:
        break;
    case 2:
        chara_select_third_by_set(0x41);
        anim_pic_slots[2][2] = D_21A1664;
        break;
    case 3:
        chara_select_third_by_set(0x3f);
        anim_pic_slots[2][2] = D_21A264C;
        break;
    case 4:
        chara_select_third_by_set(0x42);
        break;
    case 5:
        chara_select_third_by_set(0x43);
        anim_pic_slots[2][2] = D_21A392C;
        break;
    case 0:
    case 6:
    default:
        chara_select_third_by_set(0x40);
        anim_pic_slots[2][2] = D_21A392C;
        break;
    }
    return r;
}

const u8 D_2542484[] = {
    0x04, 0x6c, 0x04, 0x6c, 0x04, 0x6c, 0x04, 0x6c, 0x04, 0x6c,
    0x04, 0x6c, 0x04, 0x6c, 0x78, 0x36, 0x63, 0x04, 0x6c, 0x00
};

/* callers use the void prototype_2412D80; the definition returns int */
/*
 * Return from the away mode outing (the school, or the game-centre stay): write the diary
 * page when the pet comes back (diary_compose_draft: pages 4000..4004 by school club; with a
 * partner along 2002 or 2003 (school / nursery with the partner) instead). Jobs are job,
 * not this.
 */
int outing_return_diary(void)
{
    int b;
    u16 c;
    int t;

    snd_wait_end_b(2);
    snd_wait_end_b(3);
    if (save.pet.life_stage <= 3) {
        if (visit_partner.mode == 2) {
            if (diary_compose_draft((u8 *)diary_draft, D_2541FDC, D_2541FFC, D_2541FE8, D_2541FE8, (u8 *)((int)D_2542484), (u8 *)&save.pet.owner_name, 0x7d3, util_rand_except(2, 2)) == 0) {
                diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
                snd_wait_end();
                diary_render_store(diary_draft, diary_images);
            }
        }
    } else {
        anim_pic_slots[3][0] = 0;
        switch (util_rand_except(7, 7)) {
        case 2:
            b = 4000;
            c = util_rand_except(5, 5);
            t = outing_story_chara(outing.destination);
            anim_pic_slots[3][0] = (const u8 *)*chara_sprite_set64_pose2;
            break;
        case 3:
            b = 4001;
            c = util_rand_except(15, 15);
            t = outing_story_chara(outing.destination);
            anim_pic_slots[3][0] = (const u8 *)*chara_sprite_outing3_pose2;
            break;
        case 4:
            b = 4004;
            c = util_rand_except(5, 5);
            t = outing_story_chara(outing.destination);
            anim_pic_slots[3][0] = (const u8 *)*chara_sprite_set65_pose2;
            break;
        case 5:
            b = 4003;
            c = util_rand_except(10, 10);
            t = outing_story_chara(outing.destination);
            anim_pic_slots[3][0] = (const u8 *)*chara_sprite_set66_pose2;
            break;
        case 6:
            b = 4002;
            c = util_rand_except(5, 5);
            anim_pic_slots[2][2] = D_21A3230;
            anim_pic_slots[3][0] = (const u8 *)*chara_sprite_set63_pose2;
            break;
        }
        if (visit_partner.mode == 2) {
            b = 2002;
            c = 0;
        }
        if (diary_compose_draft((u8 *)diary_draft, ir_partner_name, D_2541FFC, D_2541FE8, D_2541FE8, friend_event.x004, (u8 *)&save.pet.owner_name, b, c) == 0) {
            diary_snapshot_images((u32 *)diary_images, t, 0);
            diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
            snd_wait_end();
            diary_render_store(diary_draft, diary_images);
        }
    }
    diary_writing_scene();
    visit_load_partner_pics(1);
    anim_happy();
    outing.destination = 0;
    outing_clear_counters();
    snd_wait_end();
    save_game();
    return 1;
}

/*
 * Choose a random destination 2..6 for the away mode outing (20% each).
 */
void outing_pick_destination(void)
{
    u8 t;

    if (outing.destination == 1)
        return;
    t = util_rand_except(100, 100);
    if (t < 20)
        outing.destination = 2;
    else if (t < 40)
        outing.destination = 3;
    else if (t < 60)
        outing.destination = 4;
    else if (t < 80)
        outing.destination = 5;
    else
        outing.destination = 6;
    util_rand_except(100, 100);
}

/*
 * Away mode set-up (outing / visit_partner outing) with the scene character.
 */
void outing_setup(void)
{
    int b;
    u16 c;
    int t;

    util_rand_except(100, 100);
    outing_text_number(2);
    anim_pic_slots[3][0] = 0;
    if (outing.destination == 2) {
        t = outing_story_chara(2);
        b = 4000;
        c = util_rand_except(5, 5);
        anim_pic_slots[3][0] = (const u8 *)*chara_sprite_set64_pose2;
    }
    if (visit_partner.mode == 2) {
        b = 2002;
        c = 0;
    }
    if (diary_compose_draft((u8 *)diary_draft, ir_partner_name, D_2541FFC, D_2541FE8, D_2541FE8, friend_event.x004, (u8 *)&save.pet.owner_name, b, c) == 0) {
        diary_snapshot_images((u32 *)diary_images, t, 0);
        diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
        snd_wait_end();
        diary_render_store(diary_draft, diary_images);
    }
}

/*
 * The "back from the outing" handler: away mode (outing) progress check and
 * animation; returns 0 when finished.
 */
u8 outing_return_handler(void)
{
    int b;
    u16 c;
    int t;

    outing_pick_destination();
    snd_wait_end_b(2);
    snd_wait_end_b(3);
    if (visit_load_partner_pics(0) != 0 && (save.pet.life_stage != 4 || visit_partner.life_stage != 4)) {
        if (visit_partner.mode == 2) {
            if (diary_compose_draft((u8 *)diary_draft, D_2541FDC, D_2541FFC, D_2541FE8, D_2541FE8, (u8 *)((int)D_2541FE8), (u8 *)&save.pet.owner_name, 0x7d3, 0) == 0) {
                diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
                snd_wait_end();
                diary_render_store(diary_draft, diary_images);
            }
        }
        return 1;
    }
    if (outing.destination == 1) {
        if (visit_partner.mode == 2) {
            if (diary_compose_draft((u8 *)diary_draft, D_2541FDC, D_2541FFC, D_2541FE8, D_2541FE8, (u8 *)((int)D_2541FE8), (u8 *)&save.pet.owner_name, 0x7d3, 1) == 0) {
                diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
                snd_wait_end();
                diary_render_store(diary_draft, diary_images);
            }
        }
        if ((u8)(btn_a_presses | btn_b_presses)) {
            btn_a_presses = 0;
            btn_b_presses = 0;
        }
        if (btn_c_presses != 0)
            return 0;
        return 1;
    }
    if (visit_load_partner_pics(0) == 0 && util_rand_except(100, 100) > 80)
        outing_setup();
    anim_pic_slots[3][0] = 0;
    switch (outing.destination) {
    case 2:
        b = 4000;
        c = util_rand_except(5, 5);
        t = outing_story_chara(outing.destination);
        anim_pic_slots[3][0] = (const u8 *)*chara_sprite_set64_pose2;
        break;
    case 3:
        b = 4001;
        c = util_rand_except(15, 15);
        t = outing_story_chara(outing.destination);
        anim_pic_slots[3][0] = (const u8 *)*chara_sprite_outing3_pose2;
        break;
    case 4:
        b = 4004;
        c = util_rand_except(5, 5);
        t = outing_story_chara(outing.destination);
        anim_pic_slots[3][0] = (const u8 *)*chara_sprite_set65_pose2;
        break;
    case 5:
        b = 4003;
        c = util_rand_except(10, 10);
        t = outing_story_chara(outing.destination);
        anim_pic_slots[3][0] = (const u8 *)*chara_sprite_set66_pose2;
        break;
    case 6:
        b = 4002;
        c = util_rand_except(5, 5);
        anim_pic_slots[2][2] = D_21A3230;
        anim_pic_slots[3][0] = (const u8 *)*chara_sprite_set63_pose2;
        break;
    }
    if (visit_partner.mode == 2) {
        b = 2002;
        c = 0;
    }
    if (diary_compose_draft((u8 *)diary_draft, ir_partner_name, D_2541FFC, D_2541FE8, D_2541FE8, friend_event.x004, (u8 *)&save.pet.owner_name, b, c) == 0) {
        diary_snapshot_images((u32 *)diary_images, t, 0);
        diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
        snd_wait_end();
        diary_render_store(diary_draft, diary_images);
    }
    return 1;
}

/*
 * Away mode outing screen (a = 1: forced end): wrapper of outing_return_handler with the
 * "どうする? かえる/かえらない" (go home?) question.
 */
int outing_screen(u8 a)
{
    int ret = 1;
    int sel = 1;

    if (a != 0) {
        snd_wait_end_b(0);
        snd_wait_end_b(1);
        outing_return_diary();
        return ret;
    }
    snd_wait_end_b(0);
    snd_wait_end_b(1);
    if (outing_return_handler() == 0) {
        diary_writing_scene();
        return 1;
    }
    tmr_t5_standard_tick();
    ui_timeout = 10;
    sel = 0;
    switch (ui_yes_no(D_2542010, D_254201C, D_2542024, 16, &sel)) {
    case 1:
        switch (sel) {
        case 0:
            outing_return_diary();
            return 1;
        case 1:
            return 1;
        }
        break;
    case 2:
        return 1;
    case 3:
        return 1;
    }
}

/*
 * Text number for the away-mode destination outing.destination and phase a.
 */
int outing_text_number(u8 a)
{
    int r;
    u16 v;

    switch (outing.destination) {
    case 2:
        switch (a) {
        case 0: r = 0x20; break;
        case 1: v = util_rand_except(100, 100) % 100; r = v < 50 ? 0x21 : 0x22; break;
        case 2:
        default: r = 0x23; break;
        }
        break;
    case 3:
        switch (a) {
        case 0: r = 0x1c; break;
        case 1: v = util_rand_except(100, 100) % 100; r = v < 50 ? 0x1d : 0x1e; break;
        case 2:
        default: r = 0x1f; break;
        }
        break;
    case 4:
        switch (a) {
        case 0: r = 0x18; break;
        case 1: v = util_rand_except(100, 100) % 100; r = v < 50 ? 0x19 : 0x1a; break;
        case 2:
        default: r = 0x1b; break;
        }
        break;
    case 5:
        switch (a) {
        case 0: r = 0x14; break;
        case 1: v = util_rand_except(100, 100) % 100; r = v < 50 ? 0x15 : 0x16; break;
        case 2:
        default: r = 0x17; break;
        }
        break;
    case 0:
    case 1:
    case 6:
    default:
        v = util_rand_except(100, 100) % 100;
        r = v < 50 ? 0x24 : 0x25;
        break;
    }
    return r;
}

const struct ClubLimits town_club_limits[] = { { 1, 2, 4 }, { 1, 1, 1 }, { 1, 3, 6 } };
const u8 D_25424F8[][5] = {
    { 2, 2, 2, 2, 2 },
    { 1, 0, 1, 2, 0 },
    { 0, 1, 0, 0, 0 },
    { 0, 2, 0, 1, 1 },
    { 2, 0, 0, 1, 1 },
    { 2, 0, 1, 1, 0 },
    { 0, 1, 1, 0, 2 },
    { 1, 1, 0, 2, 0 },
    { 1, 1, 2, 0, 0 },
    { 0, 1, 2, 1, 0 },
    { 0, 0, 1, 0, 0 },
    { 0, 0, 0, 0, 1 },
    { 0, 0, 0, 1, 0 },
    { 0, 1, 0, 0, 1 },
    { 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
const u8 D_2542548[] = { 8, 9, 10, 11, 12 };
const u8 D_2542550[] = _("じっけんキット\0");
const u8 D_2542564[] = _("メイクボックス\0");
const u8 D_2542578[] = _("サッカーボール\0");
const u8 D_254258C[] = _("かみねんど\0");
const u8 D_254259C[] = _("フライパン\0");
const u8 D_25425AC[] = _("ひっきようぐ");
const u16 town_club_lines[] = { 0x85, 0x89, 0x8d, 0x91, 0x95 };

/*
 * School club activities (ぶかつ: 0 はつめいぶ invention, 1 ファッションぶ fashion, 2 うんどうぶ
 * sports, 3 げいじゅつぶ art, 4 りょうりぶ cooking; their tools じっけんキット / メイクボックス /
 * サッカーボール / かみねんど / フライパン / ひっきようぐ). Per club three counters at
 * pet + 0x208 + k*3: [0] joined, [1] practice sessions, [2] achievements; +0x217+k =
 * club mastered.
 *
 * The club's background picture k (0..4).
 */
const u8 *town_club_background(int k)
{
    switch (k) {
    default:
    case 0:
        return D_20BCC74;
    case 1:
        return D_20BDE9C;
    case 2:
        return D_20BF0C4;
    case 3:
        return D_20C02EC;
    case 4:
        return D_20C1514;
    }
}

/*
 * Load the teacher sprite for club a and phase b (chara_select_other_by_set): b = 0 picks by progress
 * (the sum of the 3-byte record at pet + 0x208 + 3a), 1 / 2 are fixed variants.
 */
void town_club_teacher_load(int a, int b)
{
    u8 *p;
    int n;
    int v;

    p = save.pet.club_records[a];
    n = p[0] + p[1] + p[2];
    switch (a) {
    case 1:
    default:
        v = 0x4f;
        break;
    case 2:
        v = 0x50;
        break;
    case 4:
        v = 0x53;
        break;
    case 0:
        switch ((u32)b) {
        case 0:
            break;
        case 1:
            v = 0x4e;
            goto out;
        case 2:
            v = 0x4d;
            goto out;
        }
        if (n == 0)
            v = 0x4e;
        else if (n == 1)
            v = 0x4d;
        else
            v = 0x4e - (rand() & 1);
        break;
    case 3:
        switch ((u32)b) {
        case 0:
            break;
        case 1:
            v = 0x52;
            goto out;
        case 2:
            v = 0x51;
            goto out;
        }
        if (n == 0)
            v = 0x52;
        else if (n == 1)
            v = 0x51;
        else
            v = 0x52 - (rand() & 1);
        break;
    }
out:
    chara_select_other_by_set(v);
}

/*
 * The teacher's line: text 100/0x84 with the character's name s spliced into its name
 * gap.
 */
void town_club_teacher_line(u8 *s)
{
    u8 buf[132];
    u8 *p = buf;
    u8 *q = chara_text(0, 100, 0x84);
    u8 a, b;

    for (;;) {
        a = q[0];
        if (a != 4)
            break;
        b = q[1];
        if (b != 0x6c)
            break;
        *p++ = a;
        *p++ = b;
        q += 2;
    }
    strcpy((char *)p, (char *)s);
    p += strlen((char *)s);
    *p++ = 4;
    *p++ = 0x6c;
    strcpy((char *)p, (char *)q);
    chara_select_other_by_set(0x5c);
    ui_show_message(0x1a, 0, buf);
}

/*
 * Club k "not enough" scene (the needed club tool is missing): line b, the teacher's line
 * with name c, -1 happiness.
 */
void town_club_missing_tool(int a, u16 b, u8 *c)
{
    const u8 *p = town_club_background(a);

    anim_clear_pic_slot(0);
    anim_clear_pic_slot(1);
    town_club_teacher_load(a, 1);
    anim_club_teacher(p);
    ui_show_message(0x1c, p, chara_text(0, 100, b));
    anim_club_not_ready(p);
    town_club_teacher_line(c);
    anim_sad();
    pet_sub_happiness(1);
}

/*
 * Club k practice animation (one per club: anim_club_practice_invention, anim_club_practice_fashion, anim_club_practice_sports...).
 */
void town_club_practice_anim(int k)
{
    const u8 *p = town_club_background(k);

    anim_clear_pic_slot(0);
    anim_clear_pic_slot(1);
    town_club_teacher_load(k, 0);
    anim_club_teacher(p);
    switch (k) {
    default:
    case 0:
        anim_club_practice_invention();
        break;
    case 1:
        anim_club_practice_fashion();
        break;
    case 2:
        anim_club_practice_sports();
        break;
    case 3:
        anim_club_practice_art();
        break;
    case 4:
        anim_club_practice_cooking();
        break;
    }
}

/*
 * Club k practice / progress scene (+1 happiness).
 */
void town_club_progress_scene(int k, u8 *s)
{
    const u8 *p = town_club_background(k);

    anim_pic_slots[0][0] = D_21A6F58;
    anim_pic_slots[0][1] = D_21A6AAC;
    town_club_teacher_load(k, 2);
    anim_club_teacher(p);
    ui_show_message(0x1c, p, chara_text(0, 100, (u8)town_club_lines[k] + 1));
    anim_club_lesson(p);
    town_club_teacher_line(s);
    anim_happy();
    pet_add_happiness(1);
}

/*
 * First visit of lesson k.
 */
void town_club_first_visit(int k)
{
    const u8 *p = town_club_background(k);

    anim_clear_pic_slot(0);
    anim_clear_pic_slot(1);
    town_club_teacher_load(k, 1);
    anim_club_teacher(p);
    ui_show_message(0x1c, p, chara_text(0, 100, (u8)town_club_lines[k]));
    anim_club_lesson(p);
    town_club_teacher_line(town_club_common_tool_name);
    anim_happy();
    pet_add_happiness(1);
}

/*
 * One club visit (club k): join on the first visit (town_club_first_visit), practise (needs the
 * club tool, item 0x1B) up to the limit from the character's talent table
 * D_25424F8[class][k] / town_club_limits, then the achievement counter (needs the club's own
 * tool D_2542548[k]); reaching its limit awards the club mastery (+5 hearts). Missing
 * tools give the "not enough" scene town_club_missing_tool.
 */
void town_club_visit(int k)
{
    u8 m, cnt, lim;
    u8 *p;
    u8 *e;
    u8 *r;
    const u8 *q;

    e = save.pet.club_records[k];
    if (e[0] == 0) {
        town_club_first_visit(k);
        e[0] = 1;
    } else {
        m = D_25424F8[chara_club_class(save.pet.x000)][k];
        cnt = save.pet.club_records[k][1];
        lim = town_club_limits[m].practice_limit;
        if (cnt < lim) {
            if (item_inv_find_owned_id(&item_inv_tools, 0x1b) < 0) {
                town_club_missing_tool(k, town_club_lines[k] + 2, town_club_common_tool_name);
            } else {
                town_club_progress_scene(k, cnt + 1 == lim ? town_club_tool_names[k] : town_club_common_tool_name);
                if (save.pet.club_records[k][1] != 0xff)
                    save.pet.club_records[k][1]++;
            }
        } else if (item_inv_find_owned_id(&item_inv_tools, D_2542548[k]) < 0) {
            town_club_missing_tool(k, town_club_lines[k] + 3, town_club_tool_names[k]);
        } else {
            town_club_practice_anim(k);
            if (save.pet.club_records[k][2] != 0xff)
                save.pet.club_records[k][2]++;
            if (save.pet.club_records[k][2] == town_club_limits[m].achievement_limit) {
                q = town_club_background(k);
                anim_pic_slots[0][0] = town_club_backgrounds[k];
                anim_club_mastered(q);
                save.pet.club_mastered[k] = 1;
                pet_add_happiness(5);
            }
            town_club_teacher_line(town_club_tool_names[k]);
            anim_happy();
            pet_add_happiness(1);
        }
    }
    save_game();
}

/*
 * "ぶかつをえらんでね" (choose a club) list of 5 (D_2542674).
 */
int town_club_choose(u32 *out)
{
    u32 v = 0;
    int r;

    ui_new_screen();
    r = ui_paged_list(D_2542674, 5, (u8 *)town_club_menu_tbl, 0, &v);
    if (r == 1)
        *out = v;
    return r;
}

/*
 * The school: closed outside 7:00-16:59 (line 0x83, -1 happiness), babies/children are
 * sent home (0x82), else choose a club (town_club_choose) and visit it (town_club_visit).
 */
int town_school(void)
{
    u32 tmp;
    u8 k;
    int r;
    int ret;
    u8 *p;

    k = rtc_get_hour();
    ui_new_screen();
    if (k < 7 || k > 16) {
        chara_select_other_by_set(0x4c);
        anim_school_closed(D_20BBA4C);
        p = chara_text(0, 0x64, 0x83);
        ui_show_message(0x1b, 0, p);
        pet_sub_happiness(1);
        save_game();
        anim_sad();
        ret = 1;
    } else if (save.pet.life_stage <= 2) {
        chara_select_other_by_set(0x5b);
        anim_school_closed(D_20BBA4C);
        p = chara_text(0, 0x64, 0x82);
        ui_show_message(0x1a, 0, p);
        pet_add_happiness(1);
        save_game();
        anim_happy();
        ret = 1;
    } else {
        chara_select_other_by_set(0x5c);
        anim_school_closed(D_20BBA4C);
        p = chara_text(0, 0x64, 0x81);
        ui_show_message(0x1a, 0, p);
        r = town_club_choose(&tmp);
        if (r == 3) {
            ret = 0;
        } else {
            if (r == 1)
                town_club_visit(tmp);
            ret = r != 2;
        }
    }
    return ret;
}

const u8 D_2542604[] = _("はつめいぶ");
const u8 D_2542610[] = _("ファッションぶ");
const u8 D_2542620[] = _("うんどうぶ");
const u8 D_254262C[] = _("げいじゅつぶ");
const u8 D_254263C[] = _("りょうりぶ\0");
const struct ClubListEntry town_club_menu_tbl[] = {
    { D_2542604, 0 },
    { D_2542610, 0 },
    { D_2542620, 0 },
    { D_254262C, 0 },
    { D_254263C, 0 }
};
const u8 D_2542674[] = _("ぶかつをえらんでね");

/*
 * Travel scene to a town place: background p (0 = the generic one), b = whether the
 * lottery prize car is owned (flag 0x15: the car sprite 0x21B4044 instead of the
 * placeholder), c = the vehicle variant / kind 0..3; anim_travel_walk / anim_travel_ride /
 * anim_travel_ride2 are the walk / ride animations, anim_travel_walk_short the arrival. 1 = cancelled
 * with C. town_enter_park / town_enter_school / town_enter_tamadepa / town_enter_matchmaking / town_enter_nopic /
 * town_enter_nopic_b are the per-place "pre" hooks of the town menu (Tama Depa, park, school,
 * parlour, ...).
 */
u8 town_travel_scene(const u8 *p, u8 b, int c)
{
    int k;
    int z = 0;
    u32 v;

    visit_load_partner_pics(z);
    if (p != z) {
        if (c == 3)
            k = 5;
        else if (c == 2)
            k = 3;
        else if (c == 1)
            k = 2;
        else
            k = 0;
        v = (u32)D_21B4044;
        if (b == 0)
            v = (u32)D_2258DF4;
        anim_pic_slots[2][1] = (const u8 *)v;
    } else {
        k = c == 5 ? 6 : 4;
    }
    switch (k) {
    case 4:
        anim_travel_ride();
        break;
    case 6:
        anim_travel_ride2();
        break;
    case 0: case 1: case 2: case 3: case 5:
    default:
        anim_travel_walk();
        break;
    }
    if ((u8)(btn_a_presses | btn_b_presses)) {
        btn_a_presses = 0;
        btn_b_presses = 0;
    }
    if (btn_c_presses != 0)
        return 1;
    switch (k) {
    case 0: case 1: case 2: case 5:
        anim_travel_walk_short(p);
        break;
    case 3: case 4:
        break;
    }
    if ((u8)(btn_a_presses | btn_b_presses)) {
        btn_a_presses = 0;
        btn_b_presses = 0;
    }
    if (btn_c_presses != 0)
        return 1;
    return 0;
}

/*
 * Game center entrance message (text 100/0x9B).
 */
int town_game_center_greeting(void)
{
    int v0;

    anim_game_center_enter();
    v0 = chara_text(0, 0x64, 0x9b);
    ui_show_message(0x13, 0, v0);
    return 0;
}

/*
 * Enter place picture D_20B83D4.
 */
u8 town_enter_park(void)
{
    if (visit_partner.mode == 2)
        ((int (*)(int))snd_wait_end_b)(0);
    return town_travel_scene(D_20B83D4, 0, 0);
}

/*
 * Enter place picture 0x20BA824.
 */
int town_enter_school(void)
{
    snd_wait_end_b(0);
    return town_travel_scene(D_20BA824, 0, 0);
}

/*
 * Enter place picture 0x20B04DC (variant by flag 0x15).
 */
int town_enter_tamadepa(void)
{
    return town_travel_scene(D_20B04DC, pet_get_unlock_flag(0x15), 0);
}

/*
 * Tama Depa floor menu shop_floor_menu_tbl (4 entries: ゲームコーナー game corner / リフォーム
 * redecorating / アイテムショップ / たまファッション) with the welcome line 100/0x9A.
 */
u8 shop_floor_menu(void)
{
    ui_new_screen();
    town_car_slot_pic();
    ui_show_message(0x1d, 0, chara_text(0, 100, 0x9a));
    return ui_place_menu_run(shop_floor_menu_tbl, 4);
}

/*
 * Enter place picture 0x20C273C.
 */
int town_enter_matchmaking(void)
{
    return town_travel_scene(D_20C273C, 0, 0);
}

/*
 * Enter place without picture (kind 4).
 */
int town_enter_nopic(void)
{
    return town_travel_scene(0, 0, 4);
}

/*
 * Enter place without picture; returns 0/1.
 */
int town_enter_nopic_b(void)
{
    u8 x = town_travel_scene(0, 0, 0);

    return x != 0;
}

/*
 * Empty function (stub, `ret` only; falls off the end without a result).
 */
u8 town_hook_stub_a(int a)
{
}

/*
 * town_hook_stub_a(1).
 */
int town_hook_stub_call(void)
{
    return town_hook_stub_a(1);
}

/*
 * Returns 0 = "always available" (availability callback of the town menu table); so do
 * town_always_open_b .. town_always_open_d.
 */
int town_always_open(void)
{
    return 0;
}

/*
 * Returns 0x0 (stub).
 */
int town_always_open_b(void)
{
    return 0;
}

/*
 * Returns 0x0 (stub).
 */
int town_always_open_c(void)
{
    return 0;
}

int town_always_open_d(void)
{
    return 0;
}

/*
 * App slot availability: 0 when slot k of item bank / download category 8 (the four
 * 0x8000-byte "4Uアプリ" downloaded place slots, sectors 0x7A-0x7B = 0x2720000..) holds an
 * item, -1 (hidden) when erased. town_app_slot0_state / town_app_slot1_state / town_app_slot2_state /
 * town_app_slot3_state = slots 0..3 for the table.
 */
int town_app_slot_state(u8 k)
{
    u8 x = item_bank_has(8, k, 0);

    return (x != 0) - 1;
}

/*
 * town_app_slot_state(0).
 */
void town_app_slot0_state(void)
{
    town_app_slot_state(0);
}

/*
 * Is downloaded place slot 1 present?
 */
void town_app_slot1_state(void)
{
    town_app_slot_state(1);
}

/*
 * Is downloaded place slot 2 present?
 */
void town_app_slot2_state(void)
{
    town_app_slot_state(2);
}

/*
 * Is downloaded place slot 3 present?
 */
void town_app_slot3_state(void)
{
    town_app_slot_state(3);
}

/*
 * Game slot 2 picture: prize picture D_21B4044 if flag 0x15, else empty.
 */
void town_car_slot_pic(void)
{
    u8 t = pet_get_unlock_flag(0x15);
    u32 v = (u32)D_21B4044;

    if (t == 0)
        v = (u32)D_2258DF4;
    anim_pic_slots[2][1] = (const u8 *)v;
}

/*
 * The town menu table town_places: 16 entries of {x000 name, x004 availability callback (-1
 * hidden, 0 open, 1 greyed, 2 "???????" locked), x008 pre hook (travel scene, non-zero =
 * abort), x00c handler, x010 post hook}: 0 たまデパ, 1 こうえん, 2 がっこう, 3 おみあいじょ,
 * 4 たまのうえん (F43 town_farm_state_cb/2509020/250944E), 5 であいストリート (town_street_state_cb/
 * 2509034/250913A), 6-7 unused, 8 / 9 / 12 / 13 = the downloaded APP slots 0..3 (name
 * taken from the item record, no handler: see town_menu), 10-11 / 14-15 unused.
 *
 * Availability of entry k: calls its callback x004 (no callback = -1).
 */
int town_place_state(int k)
{
    int (*f)(void) = town_places[k].x004;

    if (f == 0)
        return -1;
    return f();
}

/*
 * Town menu entry k -> downloaded app slot: 8 / 9 / 12 / 13 -> 0..3, else -1.
 */
int town_entry_app_slot(int k)
{
    int r = -1;

    if (k >= 8 && k <= 13) {
        switch (k) {
        case 8:
            r = 0;
            break;
        case 9:
            r = 1;
            break;
        case 12:
            r = 2;
            break;
        case 13:
            r = 3;
            break;
        default:
            r = -1;
            break;
        }
    }
    return r;
}

/*
 * Next visible town menu entry after k (a locked entry 4 jumps to 6 and sets town_locked_jump).
 */
int town_next_visible_entry(int k)
{
    int r;

    for (;;) {
        if (++k > 15)
            k = 0;
        r = town_place_state(k);
        if (r == 2) {
            if (k == 4) {
                town_locked_jump = 1;
                k = 6;
            }
            break;
        }
        if (r >= 0)
            break;
    }
    return k;
}

/*
 * Number of available town places.
 */
u8 town_place_count(void)
{
    int n = 0;
    int i;
    int r;

    for (i = 0; i <= 15; i++) {
        r = town_place_state(i);
        if (r >= 0 && r <= 2)
            n++;
    }
    return n;
}

/*
 * Draw page a of the town menu / map (places with their pictures and names; pages: 0-3
 * the four places, 4-5 farm/street, 6-7 and 8-9 the app slots whose names are read from
 * the bank-8 item records at +0x5E), greyed entries in colour 0x34, locked ones as
 * "???????", and the page-down arrow.
 */
void town_menu_draw_page(int a)
{
    int i;
    u8 tmp[12];
    int j;
    u8 c;
    u16 w;
    int n;
    int A, B;
    int k, m;
    u8 *p;
    int x, y;
    int o;
    u32 t;

    o = 0;
    A = 0;
    B = 0;
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    if (a <= 3) {
        n = 4;
        i = 0;
    } else if (a <= 5) {
        n = 2;
        i = 4;
    } else if (a <= 7) {
        n = 2;
        i = 8;
    } else if (a == 9) {
        n = 2;
        i = 12;
    } else if (a <= 9) {
        n = 2;
        i = 12;
    } else {
        x = town_place_state(8);
        y = town_place_state(9);
        if ((x == 0 || x == 1) && (y == 0 || y == 1)) {
            n = 2;
            i = 8;
        } else {
            n = 2;
            i = 9;
        }
    }
    j = 0;
    do {
    switch (town_place_state(i)) {
    case 0:
    case 1:
        if (town_place_state(i) == 0) {
            w = 0x30;
            c = gfx_object_create(0x18, 0x20, w, 0, 0, 0);
        } else {
            w = 0x34;
            c = gfx_object_create(0x18, 0x20, w, 0, 3, 0);
        }
        t = town_places[i].name;
        if (t != 0) {
            gfx_draw_text_line(c, 0x18, j * 16 + 0x20, w, t, 0, 8);
        } else {
            if (a == 6 && A == 0)
                B = 0;
            if (a == 6 && A == 1)
                B = 1;
            if (a == 7 && A == 0)
                B = 0;
            if (a == 7 && A == 1)
                B = 1;
            if (a == 9 && A == 0)
                B = 2;
            if (a == 9 && A == 1)
                B = 3;
            if (a == 8 && A == 0)
                B = 2;
            if (a == 8 && A == 1)
                B = 3;
            for (m = k = 0; k <= 3; k++) {
                p = item_bank_slot(8, k);
                if (p != 0 && *(u16 *)p != 0xffff) {
                    if (m == B) {
                        gfx_draw_text_line(c, 0x18, j * 16 + 0x20, w, (int)(p + 0x5e), 0, 8);
                        break;
                    }
                    m++;
                }
            }
            A = 1;
        }
        j++;
        n = (u8)(n - 1);
        break;
    case 2:
        c = gfx_object_create(0x18, 0x20, 0x34, 0, 3, 0);
        gfx_draw_text_line(c, 0x18, j * 16 + 0x20, 0x34, (int)D_25426BC, 0, 8);
        n = 0;
        break;
    }
    i++;
    } while (n != 0 && i <= 15);
    if (town_place_count() > 4) {
        if (a <= 3) {
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
        } else {
            o = town_locked_jump == 1 ? 0 : 1;
            if (a == 4) {
                if (town_place_count() >= o + 6)
                    gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
                else
                    gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
            } else if (a == 6) {
                if (town_place_count() >= o + 8)
                    gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
                else
                    gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
            } else if (a == 7) {
                if (town_place_count() >= o + 8)
                    gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
                else
                    gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
            } else if (a == 8) {
                if (town_place_count() >= o + 10)
                    gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
                else
                    gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
            } else
                gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
        }
    }
}

/*
 * THE TOWN MENU / map ("おでかけ", "まち"): the places of the table town_places. A = next
 * entry, B = choose, C / timeout = leave. For the built-in places the table's pre hook
 * (travel scene), handler and post hook run. For an APP entry (8 / 9 / 12 / 13, handler
 * NULL: the downloaded places of download category 8, 4 slots of 32 KB) the "いく?" (go?)
 * dialog (D_25426CC) offers go / back / delete ("ほんとうに?" then the item is erased from
 * bank 8 by rewriting its sector), and "go" does the following - this is NATIVE CODE
 * EXECUTION FROM A DOWNLOADED ITEM:
 *     fn = item_bank_slot(8, slot);            item record of the app (TAMAGO 0x94 / 2 / 2)
 *     dl_install_program(fn);                       copy it into the boot sectors at 0x2000000
 *     entry = BE u32 at fn[0xAE..0xB1] + 0x2000000;
 *     if (entry() != 0) { ... dl_chara_affection_update(0x2000000) ... }   run it; on a non-zero
 *     result the character's "got it" animation, the downloaded-character bookkeeping
 *     dl_chara_affection_update and a save.
 * The app is therefore an S1C33 binary linked for 0x2000000 + its offset inside the
 * TAMAGO file, called as a normal function without arguments; it is accepted on the way
 * in by comm_validate_record on the exchange record that carried it (magic, size, checksum, the
 * table-driven 8-byte signature) and the type/category bytes only, no cryptography. Apps
 * arrive over NFC from the smartphone (SNEP PUT -> nfc_snep_store_fragment -> item_store_in_bank -> bank
 * 8). None is installed in this dump. The download pack has 9 distinct ones, the
 * "おでかけ" OUTINGS (ids 1001..1014, entries 0x2000204 / 0x200043E): animation programs
 * built on the API block of F8a; 7 of them carry their prizes as TAMAGO item files inside
 * the image and give them with api_store_item before returning non-zero
 * (notes/findings/tamago.md).
 */
u8 town_menu(void)
{
    u8 k;
    u8 cur, n;
    u8 h;
    int F;
    u32 v;
    int r, i, q;
    u16 a2;
    u8 *p;
    const struct PlaceEntry *e;
    u8 (*f)(void);
    u32 fn;
    u8 *s;

    k = 0;
    F = 0;
    town_locked_jump = 0;
    cur = town_next_visible_entry(16);
again:
    town_place_count();
    ui_new_screen();
    town_menu_draw_page(k);
    if (k < 4)
        h = gfx_object_create(6, (k % 4) * 16 + 0x22, 0x20, 0, 0, D_2104714);
    else
        h = gfx_object_create(6, ((k - 4) % 2) * 16 + 0x22, 0x20, 0, 0, D_2104714);
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    for (;;) {
        if (btn_a_presses != 0) {
            k++;
            n = town_next_visible_entry(cur);
            if (n == 0)
                k = 0;
            cur = n;
            if (k == 4 && town_locked_jump == 1)
                F = 1;
            if (k == 5 && town_locked_jump == 1)
                k = 6;
            if (k == 5)
                town_locked_jump = 0;
            if (k == 0 || k == 4 || k == 6 || k == 8)
                goto again;
            if (k < 4)
                gfx_object_move_dirty(h, 6, (k % 4) * 16 + 0x22);
            else if (k < 6)
                gfx_object_move_dirty(h, 6, ((k - 4) % 2) * 16 + 0x22);
            else
                gfx_object_move_dirty(h, 6, ((k - 6) % 2) * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            ui_timeout = 10;
            if (town_place_state(cur) != 0) {
                snd_error_beep();
                btn_b_presses = 0;
                ui_timeout = 10;
                continue;
            }
            if (sys_battery_low() != 0)
                return 1;
            if (cur > 7) {
                v = 0;
                switch ((u32)ui_use_no_delete((int)D_25426CC, 0, &v)) {
                case 1:
                    switch (v) {
                    case 0:
                        break;
                    case 1:
                        goto again;
                    case 2:
                        if (ui_confirm_really() != 0) {
                            u8 a0;
                            snd_wait_end();
                            a0 = item_bank_tbl[8].first_sector;
                            a2 = item_bank_tbl[8].slot_size;
                            r = town_entry_app_slot(cur);
                            if (r < 0)
                                goto clr;
                            q = a0 + (r >> 1);
                            p = item_bank_slot(8, r);
                            if (*(u16 *)p == 0xffff)
                                goto clr;
                            flash_backup_sector(q);
                            flash_restore_sector_except(q, p, a2);
                            snd_wait_end();
                            save_game();
                            n = town_next_visible_entry(cur);
                            if (n == 0) {
                                i = cur;
                                for (;;) {
                                    if ((u32)town_place_state(i) <= 1)
                                        break;
                                    i--;
                                }
                                n = i;
                                k--;
                            }
                            if (k == 5 && F == 1) {
                                k = 4;
                                n = k;
                            } else if (k == 5)
                                n = k;
                            cur = n;
                            goto again;
                        }
                        if (ui_timeout == 0)
                            return 1;
                    clr:
                        btn_b_presses = 0;
                        btn_c_presses = 0;
                        goto again;
                    }
                    break;
                case 2:
                    goto again;
                case 3:
                    return 1;
                }
            }
            f = town_places[cur].x008;
            if (f != 0 && f() != 0)
                goto again;
            town_place_aborted = 0;
            e = &town_places[cur];
            if (e->x00c != 0) {
                if (e->x00c() == 0)
                    goto L30;
                if (e->x010 != 0)
                    e->x010();
                if (town_place_aborted == 1) {
                    ui_timeout = 0;
                    return 1;
                }
                ui_timeout = 10;
                return 1;
            }
            fn = (u32)item_bank_slot(8, town_entry_app_slot(cur));
            if (*(u16 *)fn == 0xffff)
                continue;
            dl_install_program((struct TamagoFile *)fn);
            s = (u8 *)fn;
            fn = s[0xb1];
            fn += s[0xb0] << 8;
            fn += s[0xaf] << 16;
            fn += s[0xae] << 24;
            fn += FLASH_DL_PROGRAM;
            if (((int (*)(void))fn)() != 0) {
                anim_happy();
                dl_chara_affection_update(FLASH_DL_PROGRAM);
                snd_wait_end();
                save_game();
                ui_timeout = 10;
                return 1;
            }
        L30:
            if (ui_timeout == 0) {
                sys_idle_seconds = 10;
                return 0;
            }
            ui_timeout = 10;
            sys_idle_seconds = 0;
            goto again;
        }
        if (btn_c_presses != 0)
            break;
        sys_idle_tick();
        if (ui_timeout == 0)
            break;
    }
    return 0;
}

/*
 * Availability callback of entry k of a place table t (no callback = -1).
 */
int ui_place_entry_avail(struct PlaceEntry *t, int k)
{
    int (*f)(void) = t[k].x004;

    if (f == 0)
        return -1;
    return f();
}

/*
 * Next available entry after k in table t of n entries.
 */
int ui_place_next_avail(struct PlaceEntry *t, int k, int n)
{
    do {
        if (++k >= n)
            k = 0;
    } while (ui_place_entry_avail(t, k) < 0);
    return k;
}

/*
 * Draw page b of a 4-entry place list t.
 */
void ui_place_draw_page(struct PlaceEntry *t, int b, int c)
{
    int flag = 0;
    int i, k, h;
    u8 a;

    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    for (i = 0; i <= 3; i++) {
        k = b * 4 + i;
        switch (ui_place_entry_avail(t, k)) {
        case 0:
            h = 0x30;
            a = gfx_object_create(0x18, 0x20, h, 0, 0, 0);
            if (t[k].name != 0)
                gfx_draw_text_line(a, 0x18, i * 16 + 0x20, h, t[k].name, 0, 8);
            break;
        case 1:
            h = 0x34;
            a = gfx_object_create(0x18, 0x20, h, 0, 3, 0);
            if (t[k].name != 0)
                gfx_draw_text_line(a, 0x18, i * 16 + 0x20, 0x34, t[k].name, 0, 8);
            break;
        case 2:
            a = gfx_object_create(0x18, 0x20, 0x34, 0, 3, 0);
            gfx_draw_text_line(a, 0x18, i * 16 + 0x20, 0x34, (int)D_25426BC, 0, 8);
            break;
        }
        if (ui_place_next_avail(t, k, c) < k)
            flag = 1;
    }
    if (flag) {
        if (b)
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
    } else
        gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
}

/*
 * Generic 4-per-page menu driver over a table p of b {name, availability, pre, handler,
 * post} rows like the town menu's (used for the Tama Depa floors shop_floor_menu_tbl, the notebooks
 * ui_notebook_menu_tbl (F53) and others): ui_place_entry_avail = availability, ui_place_next_avail = next available
 * row, ui_place_draw_page draws a page. A moves, B runs the entry's callbacks, C back. Battery is
 * checked at every redraw (sys_battery_low -> shutdown). Returns 1 when a handler ran, 0 on
 * C / timeout (sys_idle_seconds = 10 marks the timeout).
 */
#define T ((struct PlaceEntry *)p)
u8 ui_place_menu_run(u8 *p, int b)
{
    u32 k = 0;
    u8 n;
    u8 a;
    u8 c;

again:
    if (sys_battery_low() == 1)
        sys_power_down();
    ui_new_screen();
    ui_place_draw_page(T, k >> 2, b);
    a = gfx_object_create(6, (k & 3) * 16 + 0x22, 0x20, 0, 0, 0x2104714);
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    for (;;) {
        if (btn_a_presses != 0) {
            n = ui_place_next_avail(T, k, b);
            if (k >> 2 != n >> 2) {
                k = n;
                goto again;
            }
            k = n;
            gfx_object_move_dirty(a, 6, (n & 3) * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            ui_timeout = 10;
            if (ui_place_entry_avail(T, k) != 0) {
                snd_error_beep();
                btn_b_presses = 0;
                ui_timeout = 10;
                continue;
            }
            if (T[k].x008 != 0 && T[k].x008() != 0)
                goto again;
            if (T[k].x00c != 0 && T[k].x00c() != 0) {
                if (T[k].x010 != 0)
                    T[k].x010();
                ui_timeout = 10;
                return 1;
            }
            c = ui_timeout;
            if (c == 0) {
                sys_idle_seconds = 10;
                break;
            }
            ui_timeout = 10;
            sys_idle_seconds = 0;
            goto again;
        }
        if (btn_c_presses != 0)
            break;
        sys_idle_tick();
        if (ui_timeout == 0)
            break;
    }
    return 0;
}

/*
 * Tama Depa: the entrance hook of all four floors (shop_floor_menu_tbl: ゲームコーナー,
 * リフォーム, アイテムショップ, たまファッション): the floor-entrance scene
 * anim_tamadepa_floor_enter.
 */
int town_depa_floor_enter(void)
{
    ui_new_screen();
    town_car_slot_pic();
    anim_tamadepa_floor_enter();
    return 0;
}

/*
 * Game corner: entrance line 0x9B (town_game_center_greeting), then the game menu game_menu
 * (built-in and downloaded games).
 */
int town_game_corner(void)
{
    town_game_center_greeting();
    return game_menu();
}

const u8 D_25426BC[] = _("???????");
const u8 D_25426CC[] = _("いく?");

/*
 * Town place: shop 1 (entrance animation, text 100/0x9C, shop shop_reform_living).
 */
int town_reform_shop(void)
{
    ui_new_screen();
    anim_shop1_enter();
    ui_show_message(0x12, 0, chara_text(0, 100, 0x9c));
    return shop_reform_living() ? 1 : 0;
}

/*
 * Town place: shop 2 (picture gfx_bg_tama_depa, text 100/0x9D, shop shop_play_tools).
 */
int town_item_shop(void)
{
    ui_new_screen();
    town_car_slot_pic();
    anim_shop2_enter(&gfx_bg_tama_depa);
    ui_show_message(0x1d, 0, chara_text(0, 100, 0x9d));
    return shop_play_tools() ? 1 : 0;
}

/*
 * Town place: shop 3 (picture D_20B71AC, text 100/0x9E, shop shop_fashion).
 */
int town_fashion_shop(void)
{
    ui_new_screen();
    town_car_slot_pic();
    anim_shop3_enter(D_20B71AC);
    ui_show_message(0x11, 0, chara_text(0, 100, 0x9e));
    return shop_fashion() ? 1 : 0;
}

/*
 * After a place visit: default leaving animation unless the visit ended the outing
 * (town_place_aborted).
 */
void town_place_leave(void)
{
    if (town_place_aborted != 1)
        anim_happy();
}

/*
 * The RAM variables this file defines.
 */
u8 town_place_aborted;
struct Job job;
u32 diary_images;
u32 town_locked_jump DSTRAM;
u8 town_last_street_chara = 0xFF;
int (*D_0084498[6])(void) = {
    town_park_event_nobody, town_park_greeting, town_park_event_friend,
    town_park_meet_contact, shop_tama_depa, town_lottery,
};
const u8 *town_club_tool_names[5] = {
    D_2542550, D_2542564, D_2542578, D_254258C, D_254259C,
};
u8 *town_club_common_tool_name = (u8 *)D_25425AC;
const u8 *D_00844C8[5] = {
    D_21B4A30, D_21B4C34, D_21B4E34, D_21B5028, D_21B522C,
};
const u8 *town_club_backgrounds[5] = {
    D_20F4768, D_20F4848, D_20F4928, D_20F4A04, D_20F4AE4,
};
