/*
 * The pet's core state and the character data it reads: character table look-ups (class,
 * gender, birthday, family, texts), happiness, hunger, weight, points, sickness, poop and
 * sleep, the per-minute and per-clock-event updates, unlock flags, item counts, the album
 * register, random helpers, and the big tables of character and game texts (fortunes, tips,
 * mail templates). Mixed but one stretch: its anchors run from 2405AFC to 2408068.
 *
 * Code 2405882..240883A, .rodata from 2542844.
 */
#include "proto/pet/pet_core.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/room/room_main.h"
#include "proto/chara/chara_draw.h"
#include "proto/pet/pet_bath.h"
#include "proto/food/food_meals.h"
#include "proto/room/room_draw.h"
#include "proto/room/room_window.h"
#include "proto/town/town_places.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/evo/evo_counters.h"
#include "proto/evo/evo_special.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/pet/pet_gone.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_menu.h"
#include "proto/town/town_street.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care.h"
#include "proto/spot/spot_bingo_menu.h"
#include "proto/pet/pet_status.h"
#include "proto/owner/owner_profile.h"
#include "proto/ui/ui_settings.h"
#include "proto/spot/spot_reader.h"
#include "proto/comm/comm_outcome.h"
#include "flash.h"
#include "libc.h"

/*
 * Number of clover leaves filled (bits 0..3 of pet.clover_leaves, status page 3).
 */
int pet_clover_leaf_count(void)
{
    int n = 0;
    int i;

    for (i = 0; i <= 3; i++)
        if ((save.pet.clover_leaves >> i) & 1)
            n++;
    return n;
}

/*
 * Draw the number v right-aligned ending at x (9-pixel digits D_208EDB0), one display
 * object per digit.
 */
void gfx_draw_number(u8 x, u8 y, u8 z, int v)
{
    int d;

    for (d = 1; d <= v || (d == 1 && v == 0); d *= 10) {
        gfx_object_create(x, y, z, v / d % 10, 0, D_208EDB0);
        x -= 9;
    }
}

/*
 * Same as gfx_draw_number with the digit font D_2142E38.
 */
void gfx_draw_number_b(u8 x, u8 y, u8 z, int v)
{
    int d;

    for (d = 1; d <= v || (d == 1 && v == 0); d *= 10) {
        gfx_object_create(x, y, z, v / d % 10, 0, D_2142E38);
        x -= 9;
    }
}

/*
 * F30 = the pet: state of the current Tamagotchi and its life simulation.
 * pet (struct PetState, 0x248 bytes, saved by save_game) = the current
 * character; fields understood so far:
 *   x000 character id (<= 5000 built-in, row x002 of the character table chara_table;
 *        > 5000 downloaded character, record dl_chara_record(x002 + 0x77), which has the
 *        same fields shifted by one byte),
 *   x005 hunger hearts (0..5, 6 = overfed), x006 minutes starving (0x870 = departure),
 *   x008 minutes until the next hunger heart is lost (reload: table x012),
 *   x009 minutes left to answer the hungry call, x00A call flags (see pet_raise_call),
 *   x00B per-day counter bumped after meals by pet_count_meal (stage > 2, cap 15) and
 *   cleared at midnight, x00C happiness meter (cap 30 baby/child, 100 later),
 *   x00D minutes until happiness decays (table x013), x00E minutes left on the unhappy
 *   call, x00F care mistakes (pet_care_mistake), x010 sickness count (the 5th starts the
 *   departure), x011 state flags (1 sick, 2 neglected for 3 days, 4 collapsed (end of a
 *   departure ladder), 8 / 0x10 the last departure stages, 0x40 departure after bad care
 *   or the 5th sickness, 0x80 sick animation variant), x012 departure counter (the same
 *   ladder as x006: 0x870 = 36 h), x014 age in minutes, x018 Gotchi points (money, max
 *   99999), x01C poop count + x037[6] poop types, x01E toilet urge in seconds (armed after
 *   every second hunger heart lost: 900 child, 30 baby, 300 otherwise; at 0 a poop unless
 *   toilet-trained), x067..x06A chosen places/backgrounds, x06C/x06D worn accessory /
 *   clothes (item slots through D_2535F50 / D_2535F68), x06E pet companion sprite set
 *   (anim_draw_companion), x08C.. album days, x0AC[10] bitmap of 0x4E flags
 *   (pet_get_unlock_flag: 0x1D..0x3C raised characters, 0x3D.. seasonal decorations,
 *   0x46..0x49 the icon-bar themes 1..4, see ui_theme_nth), x0B6[6] photo album, x11A..
 *   the five stats, +0x1AF/+0x1C3/+0x1D7 the names of the liked items that filled the
 *   clover leaves pet.clover_leaves bits 1/2/4 (see pet_fill_clover_leaf).
 * Related globals: pet.life_stage life stage (0 egg, 1 baby, 2 child, 3 adult, 4 special
 * form of an adult, 5 a downloaded character's stage byte; notes/README.md point 15),
 * pet.weight weight (food adds, games subtract, floor = table x011), pet.age_days age in days,
 * owner_bday_day/D_00011E1 the owner's birthday day/month, owner_bday_day[3..] 2-bit ownership
 * counts of the 0x61 item slots (item_get_count get / item_set_count set / item_acquire add /
 * item_consume remove), pet.idle_pose current idle pose (sprite set), pet.whim_call_timer timer of
 * call 4, pet_alert_call_timer[0] timer of call 8, visit_partner = the partner / visitor record (x014 ==
 * 2: a partner is present).
 * The character table chara_table (46-byte rows): x000 sprite set, x002 id, x006 family,
 * x008 gender byte (see pet_get_gender), x00F stage (see pet.life_stage), x011 base weight, x012
 * hunger period, x013 happiness period, x014 sickness %, x015/x016 wake/sleep hours, x017
 * value, x01E/x01F birthday month/day, x020 0..31 code, x02A unlock flag; the 0xFFFF id
 * ends it.
 */

/*
 * Is character id unlocked/available? Looks the id up in the character table chara_table
 * (46-byte entries, x002 = character id, 0xFFFF terminates) and tests its unlock flag
 * x02A in the achievement/flag bitmap (pet_get_unlock_flag). Ids > 5000 (downloaded
 * characters): 0.
 */
int chara_is_unlocked(u16 id)
{
    int i;
    u16 x;

    if (id <= 5000) {
        i = 0;
        for (;;) {
            x = chara_table[i].id;
            if (x == id)
                break;
            if (x == 0xffff)
                return 0;
            i++;
        }
        if (pet_get_unlock_flag(chara_table[i].unlock_flag))
            return 1;
    } else
        return 0;
    return 1;
}

/*
 * List the ids of all characters whose unlock flag is set into out[].
 */
void chara_list_unlocked(u16 *out)
{
    int i;

    for (i = 0; ; i++) {
        if (chara_table[i].unlock_flag != 0xffff) {
            if (pet_get_unlock_flag(chara_table[i].unlock_flag))
                *out++ = chara_table[i].id;
        } else if (chara_table[i].id == 0xffff)
            break;
    }
}

/*
 * List into out[] the unlocked adult (x00F == 3) characters of a different family (x006)
 * than id - the marriage candidates. Returns the count.
 */
int chara_list_marriage_candidates(u16 *out, u16 id)
{
    const struct CharaEntry *p;
    int n = 0;
    u16 k = id;

    for (p = chara_table; p->id != 0xffff; p++) {
        if (p->id == id) {
            k = p->family;
            break;
        }
    }
    for (p = chara_table; p->id != 0xffff; p++) {
        if (p->unlock_flag != 0xffff && pet_get_unlock_flag(p->unlock_flag) && p->family != k && p->stage == 3) {
            *out++ = p->id;
            n++;
        }
    }
    return n;
}

/*
 * Icon-bar themes (ui_theme_names: ノーマル / ピンクノート / パープルアラビア / ブルースター /
 * きゃんでぃすかい, chosen in the settings, ui_settings_theme): theme 0 is always available,
 * themes 1..4 need the flags 0x46..0x49; the current one pet.icon_theme is skipped.
 *
 * ui_theme_nth: the n-th available theme.
 */
int ui_theme_nth(int n)
{
    int i;

    for (i = 0; i <= 4; i++) {
        if (i == save.pet.icon_theme)
            continue;
        if (i == 0 || pet_get_unlock_flag(i + 0x45)) {
            if (n == 0)
                return i;
            n--;
        }
    }
    return i;
}

/*
 * Number of unlocked extra icon-bar themes (flags 0x46..0x49 set).
 */
int ui_theme_count(void)
{
    int n = 0;
    int i;

    for (i = 0x46; i <= 0x49; i++)
        if (pet_get_unlock_flag(i))
            n++;
    return n;
}

/*
 * Draw the list of the available icon-bar themes of ui_theme_nth (names
 * ui_theme_names[i]).
 */
void ui_theme_draw_list(void)
{
    int i;
    int n;
    u8 a;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    n = 0;
    for (i = 0; i <= 4; i++) {
        if (i == save.pet.icon_theme)
            continue;
        if (i != 0 && pet_get_unlock_flag(i + 0x45) == 0)
            continue;
        a = gfx_object_create(0x18, 0x20, 0, 0, 0, 0);
        gfx_draw_text_line(a, 0x18, n * 16 + 0x20, 0x20, (int)ui_theme_names[i], 0, 8);
        n++;
    }
}

/*
 * Three-entry menu ui_status_menu_labels = キャラパラメータ / プロフィール / せってい (character
 * parameters / profile / settings): A moves the hand cursor, B runs entry 0
 * (ui_status_browser), 1 (owner_editor) or 2 (ui_settings_menu), C or timeout leaves.
 */
void ui_status_menu(void)
{
    int tbl[3];
    u8 b;
    u8 sel;
    u8 i;
    u8 a;

    memcpy(tbl, ui_status_menu_labels, 12);
    sel = 0;
again:
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    for (i = 0; i <= 2; i++) {
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_line(a, 0x18, i * 16 + 0x20, 0x10, tbl[i], 0, 8);
    }
    b = gfx_object_create(6, sel * 16 + 0x22, 0x30, 0, 0, D_2104714);
    func_00009A6();
    btn_clear();
    ui_timeout = 10;
    do {
        if (btn_a_presses) {
            sel = sel > 1 ? 0 : sel + 1;
            gfx_object_move_dirty(b, 6, sel * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            switch (sel) {
            case 0:
                if (ui_status_browser() < 0) {
                    ui_timeout = 0;
                    return;
                }
                break;
            case 1:
                if (owner_editor() < 0) {
                    ui_timeout = 0;
                    return;
                }
                break;
            case 2:
                if (ui_settings_menu() < 0) {
                    ui_timeout = 0;
                    return;
                }
                break;
            }
            goto again;
        }
        if (btn_c_presses)
            break;
        sys_idle_tick();
    } while (ui_timeout);
    btn_clear();
}

/*
 * Drop the ownership of item slots 0x53..0x5C (the temporary / event items): their counts
 * = 0.
 */
void item_drop_event_items(void)
{
    int i;

    for (i = 0x53; i <= 0x5c; i++)
        item_set_count(i, 0);
}

/*
 * Empty function (stub, `ret` only).
 */
void func_2405CF6(void)
{
}

/*
 * Raise a "call" (attention request, shown by the attention icon and a beep) of kind k in
 * the call flags pet.x00A:
 *   1 = hungry (x009 = 15 minutes to answer before it counts as a care mistake; for
 *       stage > 2 the idle pose pet.idle_pose = 1, the old one saved in pet.saved_idle_pose),
 *   2 = unhappy (x00E = 15 minutes; stage > 2: pose 0x92 or 0xDB at random),
 *   4 = a whim/discipline call, only when no other call is pending (pet.whim_call_timer = 15
 *       minutes; raised at fixed hours by pet_clock_events),
 *   8 = the alert call (raised on sickness, too many poops, neglect, the departure ladders
 *       and after a care mistake; clears the timer pet_alert_call_timer[0]),
 *   0x80 = a stage change is due (egg, baby at 60 minutes, child at 2940 minutes; main
 *       performs it): clears calls 1 and 2, and if it was already pending evo_stage_change_silent,
 *   others just set the bit (0x10 owner's birthday, 0x20 Christmas 12/25, 0x40 a
 *   character's birthday).
 * Suppressed while a visitor is present (visit_partner.mode != 0; with a partner, x014 == 2,
 * the calls 0x10/0x40 still pass), while away (outing: only 0x80, which then also
 * clears call 4) and during job. Returns 1 for a new call 4 or a partner call
 * 0x10/0x40, else 0.
 */
u8 pet_raise_call(u8 k)
{
    if (visit_partner.mode == 2 && (k == 0x10 || k == 0x40)) {
        save.pet.calls |= k;
        return 1;
    }
    if (visit_partner.mode != 0)
        return 0;
    if (outing.destination != 0) {
        if (k != 0x80)
            return 0;
        pet_clear_call(1);
        pet_clear_call(2);
        pet_clear_call(4);
        if (save.pet.calls & 0x80)
            evo_stage_change_silent();
        save.pet.calls |= 0x80;
        return 0;
    }
    if (job.kind != 0)
        return 0;
    switch (k) {
    case 1:
        if (save.pet.calls & 1)
            break;
        pet_clear_call(4);
        save.pet.calls |= 1;
        save.pet.hungry_call_timer = 0xf;
        rtc_seconds = 0;
        if (save.pet.life_stage <= 2)
            break;
        save.pet.saved_idle_pose = save.pet.idle_pose;
        save.pet.idle_pose = 1;
        return 0;
    case 2:
        if (save.pet.calls & 2)
            break;
        pet_clear_call(4);
        save.pet.calls |= 2;
        save.pet.unhappy_call_timer = 0xf;
        rtc_seconds = 0;
        if (save.pet.life_stage <= 2)
            break;
        save.pet.saved_idle_pose = save.pet.idle_pose;
        if (rand() % 100 < 0x32)
            save.pet.idle_pose = 0x92;
        else
            save.pet.idle_pose = 0xdb;
        return 0;
    case 4:
        if (save.pet.calls != 0)
            break;
        save.pet.calls |= 4;
        save.pet.whim_call_timer = 0xf;
        return 1;
    case 0x80:
        pet_clear_call(1);
        pet_clear_call(2);
        if (save.pet.calls & 0x80)
            evo_stage_change_silent();
        save.pet.calls |= 0x80;
        return 0;
    case 8:
        if (save.pet.calls & 8)
            break;
        save.pet.calls |= 8;
        save.pet.alert_call_timer = 0;
        sys_debug_fast_forward = 0;
        break;
    default:
        save.pet.calls |= k;
        break;
    }
    return 0;
}

/*
 * Choose the idle pose pet.idle_pose of an adult from its call timers: the character's base
 * pose (id - 0x10, or - 0x12 for ids > 60) when the hungry window x009 is longer than the
 * unhappy one x00e, else one of three alternative poses 73 apart (the "unhappy" poses).
 */
void pet_choose_adult_pose(void)
{
    int k;

    if (save.pet.hungry_call_timer > save.pet.unhappy_call_timer) {
        if (save.pet.x000 > 60)
            save.pet.idle_pose = save.pet.x000 - 0x12;
        else
            save.pet.idle_pose = save.pet.x000 - 0x10;
    } else {
        k = 73;
        if (save.pet.x000 > 60)
            save.pet.idle_pose = save.pet.x000 + (rand() % 3 + 1) * k - 0x12;
        else
            save.pet.idle_pose = save.pet.x000 + k * (rand() % 3 + 1) - 0x10;
    }
}

/*
 * Clear call k (see pet_raise_call) and its timer: 1 hungry (x009) and 2 unhappy (x00e)
 * restore the saved idle pose pet.saved_idle_pose, 4 (pet.whim_call_timer), 8 (pet_alert_call_timer[0]).
 */
void pet_clear_call(u8 k)
{
    switch (k) {
    case 1:
        if (save.pet.calls & 1) {
            if (save.pet.idle_pose <= 0x48)
                save.pet.idle_pose = save.pet.saved_idle_pose;
            save.pet.calls &= ~1;
        }
        save.pet.hungry_call_timer = 0;
        break;
    case 2:
        if (save.pet.calls & 2) {
            if (save.pet.idle_pose > 0x48)
                save.pet.idle_pose = save.pet.saved_idle_pose;
            save.pet.calls &= ~2;
        }
        save.pet.unhappy_call_timer = 0;
        break;
    case 4:
        save.pet.calls &= ~4;
        save.pet.whim_call_timer = 0;
        break;
    case 8:
        save.pet.calls &= ~8;
        save.pet.alert_call_timer = 0;
        break;
    default:
        save.pet.calls &= ~k;
        break;
    }
}

/*
 * Low byte of the character id for stages > 2 (adult, special form, downloaded), else 0.
 */
int pet_id_low_byte_grown(void)
{
    u8 a = save.pet.x000_b;

    if (save.pet.life_stage <= 2)
        return 0;
    return a;
}

/*
 * Current character id (pet.x000).
 */
u16 pet_get_id(void)
{
    return save.pet.x000;
}

/*
 * Gender byte of the current character (table field x008; for downloaded characters the
 * x007 of their record): the status page draws the gender icon from it (F46) and F29
 * picks the street characters of the other gender with it. Sent in TAMAGO packets
 * (+0x5F).
 */
u8 pet_get_gender(void)
{
    if (save.pet.x000 <= 5000)
        return chara_table[save.pet.chara_row].gender;
    return dl_chara_record((u8)save.pet.chara_row + 0x77)->gender;
}

/*
 * Item inventory: ownership count (0..3) of item slot n (n <= 0x60), 2 bits per slot
 * packed in pet.item_counts.
 */
int item_get_count(u32 n)
{
    if (n > 0x60)
        return 0;
    return (save.pet.item_counts[n >> 2] >> ((n & 3) * 2)) & 3;
}

/*
 * Spare count of item slot n: the ownership count minus one when the item (record via
 * item_record_by_index, type 0x81 or 1) is the accessory (category 2, slot x06c through
 * D_2535F50) or the clothes (category 3, slot x06d through D_2535F68) currently worn.
 */
int item_spare_count(int n)
{
    struct TamagoFile *p;
    u16 *h;
    int r = item_get_count(n);

    if (r != 0) {
        h = (u16 *)item_record_by_index(n);
        if (*h != 0xffff) {
            p = (struct TamagoFile *)h;
            if (p->x04e == 0x81 || p->x04e == 1) {
                switch (p->x04f) {
                case 2:
                    if (D_2535F50[save.pet.worn_accessory] == n)
                        r--;
                    break;
                case 3:
                    if (D_2535F68[save.pet.worn_clothes] == n)
                        r--;
                    break;
                }
            }
        }
    }
    return r;
}

/*
 * Item inventory: set the ownership count of slot n to v (0..3).
 */
void item_set_count(u32 n, int v)
{
    int s;

    if (n > 0x60 || v > 3 || v < 0)
        return;
    s = (n & 3) * 2;
    save.pet.item_counts[n >> 2] = (save.pet.item_counts[n >> 2] & ~(3 << s)) | ((v & 3) << s);
}

/*
 * Item inventory: acquire one item n (max 3). Returns 1 if it already had three.
 */
int item_acquire(u32 n)
{
    int t;

    if (n > 0x60)
        return 0;
    t = item_get_count(n);
    if (t > 2)
        return 1;
    t++;
    item_set_count(n, t);
    return 0;
}

/*
 * Item inventory: consume one item n. Returns 1 when the last one was used.
 */
int item_consume(u32 n)
{
    int t;

    if (n > 0x60)
        return 0;
    t = item_get_count(n);
    if (t != 0) {
        t--;
        item_set_count(n, t);
        if (t == 0)
            return 1;
    }
    return 0;
}

/*
 * Like item_consume, but consumes two when a partner is present (visit_partner.mode == 2:
 * both eat).
 */
int item_consume_shared(u32 n)
{
    int t;

    if (n > 0x60)
        return 0;
    t = item_get_count(n);
    if (t == 0)
        goto zero;
    t--;
    if (visit_partner.mode == 2 && t > 0)
        t--;
    item_set_count(n, t);
    if (t == 0)
        return 1;
zero:
    return 0;
}

/*
 * Achievement / unlock flag n (0..0x4D) of the bitmap pet.x0AC[10] (unlocked
 * characters / places / items, events seen...).
 */
u8 pet_get_unlock_flag(int n)
{
    if (n < 0 || n > 0x4d)
        return 0;
    return (save.pet.unlock_flags[n / 8] >> (n & 7)) & 1;
}

/*
 * Set flag n of the bitmap pet.x0AC to v (asm).
 */
void pet_set_unlock_flag(int n, u8 v)
{
    if ((u32)n <= 0x4d) {
        save.pet.unlock_flags[n / 8] &= ~(1 << (n & 7));
        save.pet.unlock_flags[n / 8] |= (v ? 1 : 0) << (n & 7);
    }
}

/*
 * Make happy by a (stage > 0, a != 0): clears the unhappy call, starts the happiness decay
 * timer x00D from the character table if it was 0, adds a to the happiness meter x00C (cap
 * 30 for baby/child, 100 for stage > 2). Returns 1 when the cap is hit.
 */
int pet_add_happiness(u8 a)
{
    struct DlCharaRecord *s;
    u8 r = 0;
    u8 lim;

    if (a != 0 && save.pet.life_stage != 0) {
        pet_clear_call(2);
        if (save.pet.x000 <= 0x1388) {
            if (save.pet.happiness_timer == 0)
                save.pet.happiness_timer = chara_table[save.pet.chara_row].happiness_period;
        } else {
            s = dl_chara_record(save.pet.chara_row + 0x77);
            if (save.pet.happiness_timer == 0)
                save.pet.happiness_timer = s->happiness_period;
        }
        save.pet.x087 = 0;
        lim = 30;
        if (save.pet.life_stage > 2)
            lim = 100;
        save.pet.happiness += a;
        if (save.pet.happiness >= lim) {
            save.pet.happiness = lim;
            r = 1;
        }
    }
    return r;
}

/*
 * Happiness meter x00C minus n (stops at 0; returns 1 when it reached 0).
 */
u8 pet_sub_happiness(u8 n)
{
    if (save.pet.happiness <= n) {
        save.pet.happiness = 0;
        return 1;
    }
    save.pet.happiness -= n;
    return 0;
}

/*
 * Value chara_value_table[a - 1].
 */
u32 chara_value_lookup(int a)
{
    return chara_value_table[a - 1];
}

/*
 * Partner's wish list: while a partner is present (visit_partner.mode == 2, x003 == 4)
 * remember up to two distinct type-0x81 items p (id bytes 0x5A/0x5B + kind 0x50) in
 * visit_partner.wish1_id_hi.. / visit_wish2_id_lo.., counting them in visit_wish_count (probably the "items the
 * visitor liked").
 */
void visit_note_partner_wish(struct TamagoFile *p)
{
    if (visit_partner.mode != 2)
        return;
    if (item_partner_preference(p) != 1)
        return;
    if (visit_partner.life_stage != 4)
        return;
    if (visit_partner.wish_count > 2)
        return;
    if (p == 0)
        return;
    if (p->x04e != 0x81)
        return;
    if (visit_partner.wish1_id_hi == p->x05a && visit_partner.wish1_id_lo == p->x05b && visit_wish1_kind == p->kind)
        return;
    if (visit_partner.wish2_id_hi == p->x05a && visit_wish2_id_lo == p->x05b && visit_wish2_kind == p->kind)
        return;
    if (visit_partner.wish_count == 0) {
        visit_partner.wish1_id_hi = p->x05a;
        visit_partner.wish1_id_lo = p->x05b;
        visit_wish1_kind = p->kind;
    } else if (visit_partner.wish_count == 1) {
        visit_partner.wish2_id_hi = p->x05a;
        visit_wish2_id_lo = p->x05b;
        visit_wish2_kind = p->kind;
    }
    visit_wish_count++;
}

/*
 * Fill a leaf of the four-leaf clover (status page 3) with the item file p the pet likes
 * (item_pet_preference_raw(p) == 1, type 0x81, stage > 2): the first three distinct liked
 * items fill the leaves pet.clover_leaves bits 1, 2, 4 (the item's id / kind kept in
 * pet.clover1_id_hi.., its 20-byte name at 0x5E copied to pet + 0x1AF / 0x1C3 / 0x1D7);
 * a = 8 is the fourth leaf, daily care (pet_try_daily_care_leaf). Plays the animation
 * anim_clover_leaf_get with the leaf's pictures (D_20F3640.., put in anim_pic_slots), +10
 * happiness, save; all four filled -> album_register registers the character and
 * anim_clover_complete plays the completion scene; the life story at the see-off
 * (pet_life_story) needs all four. Nothing is unlocked. Returns 1 when a leaf was shown.
 */
u8 pet_fill_clover_leaf(int a0, struct TamagoFile *p)
{
    struct { u32 a, b; } s;
    u8 a = a0;
    u8 r;
    u8 *buf;
    struct PetState *g;

    if (a != 8) {
        if (item_pet_preference_raw(p) != 1)
            goto zero;
    }
    r = 0;
    buf = 0;
    g = &save.pet;
    if (save.pet.life_stage > 2) {
        if (a != 8) {
            if (p == 0)
                return r;
            if (p->x04e != 0x81)
                return r;
            switch (a) {
            case 1:
            case 2:
            case 4:
                if ((save.pet.clover_leaves & 1) && save.pet.clover1_id_hi == p->x05a && save.pet.clover1_id_lo == p->x05b
                    && save.pet.clover1_kind == p->kind)
                    return r;
                if ((save.pet.clover_leaves & 2) && save.pet.clover2_id_hi == p->x05a && save.pet.clover2_id_lo == p->x05b
                    && save.pet.clover2_kind == p->kind)
                    return r;
                if (!(save.pet.clover_leaves & 1)) {
                    a = 1;
                    save.pet.clover1_id_hi = p->x05a;
                    save.pet.clover1_id_lo = p->x05b;
                    save.pet.clover1_kind = p->kind;
                } else if (!(save.pet.clover_leaves & 2)) {
                    a = 2;
                    save.pet.clover2_id_hi = p->x05a;
                    save.pet.clover2_id_lo = p->x05b;
                    save.pet.clover2_kind = p->kind;
                } else if (!(save.pet.clover_leaves & 4)) {
                    a = 4;
                } else
                    return r;
            }
        }
        s.a = anim_pic_slots[0][0];
        s.b = anim_pic_slots[0][1];
        switch (a) {
        case 1:
            anim_pic_slots[0][0] = D_20F3640;
            anim_pic_slots[0][1] = D_20F3180;
            buf = (u8 *)g + 0x1af;
            r = 1;
            break;
        case 2:
            anim_pic_slots[0][0] = D_20F36A4;
            anim_pic_slots[0][1] = D_20F32B0;
            buf = (u8 *)g + 0x1c3;
            r = 1;
            break;
        case 4:
            anim_pic_slots[0][0] = D_20F3708;
            anim_pic_slots[0][1] = D_20F33E0;
            buf = (u8 *)g + 0x1d7;
            r = 1;
            break;
        case 8:
            anim_pic_slots[0][0] = D_20F376C;
            anim_pic_slots[0][1] = D_20F3510;
            r = 1;
            break;
        }
        if (buf) {
            memset(buf, 0, 0x14);
            memcpy(buf, &p->name, 0x14);
        }
        if (r)
            anim_clover_leaf_get();
        if (!(a & save.pet.clover_leaves)) {
            save.pet.clover_leaves |= a;
            pet_add_happiness(10);
            snd_wait_end();
            save_game();
            if ((save.pet.clover_leaves & 0xf) == 0xf) {
                album_register();
                anim_clover_complete();
            }
        }
    }
    anim_pic_slots[0][0] = (const u8 *)s.a;
    anim_pic_slots[0][1] = (const u8 *)s.b;
    return r;
zero:
    return 0;
}

/*
 * Clear bits 0..2 of pet.x0cd.
 */
void pet_clear_album_bits(void)
{
    save.pet.x0cd &= ~7;
}

/*
 * Character id aliases: 0x57/0x58/0x59 -> 0x16/0x1A/0x29 (the same character under
 * another id).
 */
int chara_canonical_id(u8 c)
{
    switch (c) {
    case 0x57:
        return 0x16;
    case 0x58:
        return 0x1a;
    case 0x59:
        return 0x29;
    }
    return c;
}

/*
 * Map an adult (ids 0x10..0x15 まめっち.., 0x3E9..0x3EE めめっち..) or one of its special
 * forms (0x7D1.., 0xBB9..) to its family index 0..11 in the raised-characters flags
 * (0x1D + index); 0xFF if none.
 */
u8 chara_family_index(u16 a)
{
    switch (a) {
    case 0x10: return 0;
    case 0x11: return 1;
    case 0x12: return 2;
    case 0x13: return 3;
    case 0x14: return 4;
    case 0x15: return 10;
    case 0x3e9: return 5;
    case 0x3ea: return 6;
    case 0x3eb: return 7;
    case 0x3ec: return 8;
    case 0x3ed: return 9;
    case 0x3ee: return 11;
    case 0x7d1: return 0;
    case 0x7d2: return 0;
    case 0x7d3: return 1;
    case 0x7d4: return 1;
    case 0x7d5: return 2;
    case 0x7d6: return 2;
    case 0x7d7: return 3;
    case 0x7d8: return 3;
    case 0x7d9: return 4;
    case 0x7da: return 10;
    case 0xbb9: return 5;
    case 0xbba: return 5;
    case 0xbbb: return 6;
    case 0xbbc: return 7;
    case 0xbbd: return 7;
    case 0xbbe: return 8;
    case 0xbbf: return 9;
    case 0xbc0: return 11;
    case 0xbc1: return 8;
    case 0xbc2: return 6;
    }
    return 0xff;
}

/*
 * All four clover leaves pet.clover_leaves & 0xF filled (see pet_fill_clover_leaf)?
 */
int pet_clover_complete(void)
{
    if ((save.pet.clover_leaves & 0xf) == 0xf)
        return 1;
    return 0;
}

/*
 * Register the current character in the raised-characters book / album (flash sector
 * 0x37, 80 records of 0x1000 bytes: slots 0..11 for the built-in families, 12..79 for
 * downloaded characters): finds its record or a free one, rewrites the sector without it
 * (flash_backup_sector / flash_restore_sector_except), programs a 32-byte header {u16 group (or id), u16 age in
 * days pet.age_days, u16 gender pet_get_gender, 0xFFFF, u32 serial = highest + 1, name[0x14]}
 * and at +0x20 the character snapshot written by chara_render_poses_flash (field list D_2542AF8);
 * stores pet.age_days at pet + 0x8C + index and sets the family's flag 0x1D + index.
 * Returns 1 when newly registered, 0 if it was there (updated) or no slot is free.
 */
u8 album_register(void)
{
    struct {
        u16 a, b, c, d;
        u32 e;
        u8 data[0x14];
    } st;
    int v;
    u8 s1, s2, s3;
    u8 ret;
    int found;
    int empty;
    u32 max;
    u16 k, k2;
    u8 *p;
    int i;
    u16 *s;
    u16 *d;
    u32 j;

    ret = 1;
    v = chara_canonical_id(pet_get_id());
    if (v != 0) {    p = flash_sector_addr(0x37);
    max = 0;
    empty = 0xffff;
    found = 0xffff;
    if (save.pet.x000 <= 5000) {
        k = chara_family(save.pet.x000);
        for (i = 0; i <= 11; i++, p += 0x1000) {
            if (*(u16 *)p == k)
                found = (u16)i;
            if (*(u16 *)p == 0xffff && empty == 0xffff)
                empty = (u16)i;
        }
    } else {
        k = save.pet.x000;
        p += 0xc000;
        for (i = 12; i <= 0x4f; i++, p += 0x1000) {
            if (*(u16 *)p == k)
                found = (u16)i;
            if (*(u16 *)p == 0xffff && empty == 0xffff)
                empty = (u16)i;
        }
    }
    p = flash_sector_addr(0x37);
    for (i = 0; i <= 0x4f; i++, p += 0x1000) {
        if (*(u32 *)(p + 8) != 0xffffffff) {
            if (max == 0xffffffff)
                max = *(u32 *)(p + 8);
            else if (max < *(u32 *)(p + 8))
                max = *(u32 *)(p + 8);
        }
    }
    if (found == 0xffff && empty == 0xffff)
        return 0;
    if (found != 0xffff) {
        empty = found;
        ret = 0;
    }
    k2 = save.pet.x000;
    if (k2 > 5000)
        v = chara_family_index(chara_album_key(k2));
    else
        v = chara_family_index(k2);
    s1 = save.pet.x062;
    save.pet.x062 = 0;
    s2 = save.pet.x0cc;
    save.pet.x0cc = 0;
    s3 = save.pet.x148;
    save.pet.x148 = 0;
    p = flash_sector_addr(0x37);
    p += empty << 12;
    flash_backup_sector(empty / 16 + 0x37);
    flash_restore_sector_except(empty / 16 + 0x37, p, 0x1000);
    st.a = k;
    st.b = save.pet.age_days;
    st.c = pet_get_gender();
    st.e = max + 1;
    st.d = 0xffff;
    strncpy((char *)st.data, (char *)chara_text(k, 1, 0), 0x14);
    s = (u16 *)&st;
    sys_irq_disable();
    d = (u16 *)p;
    for (j = 0; j <= 15; j++) {
        func_0000B52(d, *s, 0x2000000);
        s++;
        d++;
    }
    sys_irq_enable();
    chara_render_poses_flash((int)D_2542AF8, (struct PictureSetHdr *)(p + 0x20));
    if (v != 0xff) {
        u8 *q = (u8 *)&save.pet + v;
        q[0x8c] = save.pet.age_days;
        pet_set_unlock_flag(v + 0x1d, 1);
    }
    save.pet.x062 = s1;
    save.pet.x0cc = s2;
    save.pet.x148 = s3;
    pet_clear_album_bits();
    save_game();
    return ret;
    }
    return 0;
}

/*
 * Re-snapshot an already registered character: update the album picture of the current
 * character when its family flag is set.
 */
int album_update_snapshot(void)
{
    int k;
    u8 *b;
    int t;
    u8 *q;

    t = chara_canonical_id(pet_get_id());
    if (t != 0) {
        if (pet_get_unlock_flag(chara_family_index(t) + 0x1d) == 1) {
            b = flash_sector_addr(0x37);
            b += k << 12;
            flash_backup_sector(k / 16 + 0x37);
            flash_restore_sector_except(k / 16 + 0x37, b, 0x1000);
            chara_render_poses_flash((int)D_2542AFC, (struct PictureSetHdr *)b);
            q = (u8 *)&save.pet + k;
            q[0x8c] = save.pet.age_days;
            pet_clear_album_bits();
            save_game();
            return 1;
        }
    }
    return 0;
}

/*
 * Number of raised characters (album flags 0x1D..0x3C set: families collected).
 */
int album_count(void)
{
    int n = 0;
    int i;

    for (i = 0x1d; i <= 0x3c; i++)
        if (pet_get_unlock_flag(i))
            n++;
    return n;
}

/*
 * Like album_count without counting the current character's family.
 */
int album_count_others(void)
{
    int n = album_count();
    u16 t = pet_get_id();

    if (t != 0 && pet_get_unlock_flag(chara_family_index(t) + 0x1d))
        n--;
    return n;
}

/*
 * Feed: clears the hungry call, resets the starvation counter x006 (unless past 0x86F),
 * starts the hunger timer x008 from the table if it was 0, adds a to the hunger hearts
 * x005 (> 5 -> 6 = overfed); with a partner present (visit_partner.mode == 2) the partner
 * eats too (visit_partner.hunger). Returns 0 if already full.
 */
int pet_feed(u8 a)
{
    struct DlCharaRecord *s;
    struct PetState *p;

    if (save.pet.hunger > 5 || a == 0)
        return 0;
    pet_clear_call(1);
    if (save.pet.x000 <= 0x1388) {
        if (save.pet.starve_minutes <= 0x86f)
            save.pet.starve_minutes = 0;
        if (save.pet.hunger_timer == 0)
            save.pet.hunger_timer = chara_table[save.pet.chara_row].hunger_period;
    } else {
        s = dl_chara_record(save.pet.chara_row + 0x77);
        if (save.pet.starve_minutes <= 0x86f)
            save.pet.starve_minutes = 0;
        if (save.pet.hunger_timer == 0)
            save.pet.hunger_timer = s->hunger_period;
    }
    p = &save.pet;
    save.pet.x087 = 0;
    p->hunger += a;
    if (p->hunger > 5)
        p->hunger = 6;
    if (visit_partner.mode == 2) {
        visit_partner.hunger += a;
        if (visit_partner.hunger > 5)
            visit_partner.hunger = 6;
    }
    return 1;
}

/*
 * Can eat? (hunger hearts not full and not x011 bit 1, neglected).
 */
u8 pet_can_eat(void)
{
    if (save.pet.hunger > 5)
        return 0;
    if (save.pet.status & 2)
        return 0;
    return 1;
}

/*
 * After a meal (called by the feeding code of F9): x00B + 1 for stage > 2 (cap 15;
 * cleared at midnight) and pet.meal_count + 1 (cap 99).
 */
void pet_count_meal(void)
{
    struct PetState *p = &save.pet;
    if (save.pet.life_stage > 2)
        p->meals_today++;
    if (save.pet.meals_today > 14)
        save.pet.meals_today = 15;
    if (save.pet.meal_count < 99)
        save.pet.meal_count++;
}

/*
 * Gotchi points (money, pet.gotchi_points) + n, capped at 99999 (returns 1 if capped).
 */
int pet_add_points(int n)
{
    save.pet.gotchi_points += n;
    if (save.pet.gotchi_points > 99999) {
        save.pet.gotchi_points = 99999;
        return 1;
    }
    return 0;
}

/*
 * Spend n Gotchi points; returns 1 (and spends nothing) if not enough.
 */
int pet_spend_points(u32 n)
{
    if (save.pet.gotchi_points >= n) {
        save.pet.gotchi_points -= n;
        return 0;
    }
    return 1;
}

/*
 * Fewer than a Gotchi points (cannot afford a)?
 */
u8 pet_cannot_afford(int a)
{
    return save.pet.gotchi_points < a;
}

/*
 * Weight pet.weight + n (cap 99; returns 1 if capped).
 */
int pet_add_weight(u8 n)
{
    save.pet.weight += n;
    if (save.pet.weight > 99) {
        save.pet.weight = 99;
        return 1;
    }
    return 0;
}

/*
 * Weight pet.weight - n, not below the character's base weight (table x011, record x010).
 * Returns 1 at the floor.
 */
int pet_sub_weight(u8 n)
{
    struct DlCharaRecord *p;
    int r;

    if (save.pet.weight >= n)
        save.pet.weight -= n;
    else
        save.pet.weight = 0;
    if (save.pet.x000 <= 5000) {
        r = 0;
        if (save.pet.weight < chara_table[save.pet.chara_row].base_weight) {
            save.pet.weight = chara_table[save.pet.chara_row].base_weight;
            r = 1;
        }
    } else {
        p = dl_chara_record((u8)save.pet.chara_row + 0x77);
        r = 0;
        if (save.pet.weight < p->base_weight) {
            save.pet.weight = p->base_weight;
            r = 1;
        }
    }
    return r;
}

/*
 * Get sick (x011 bit 0; nothing if already sick). The 5th sickness (x010 > 4) starts the
 * departure: x010 = 0, x011 |= 0x44, x012 = 0x870, calls 1/2/4 cleared, x01e and the care
 * countdowns pet.bath_timer/BE/C0 reset, call 8 (or alarm sound 4 when it is already pending),
 * save_request = 1. Unless x011 has 0x40, half of the cases get the 0x80 sick animation
 * variant. Returns 1 when the departure started.
 */
int pet_get_sick(void)
{
    int r = 0;
    struct PetState *p = &save.pet;

    if (p->status & 1)
        goto done;
    p->status |= 1;
    if (++p->sick_count > 4) {
        p->sick_count = r;
        p->status |= 0x44;
        p->decline_minutes = 0x870;
        pet_clear_call(4);
        pet_clear_call(1);
        pet_clear_call(2);
        p->toilet_timer = r;
        save.pet.cleaning_timer = r;
        save.pet.bath_timer = r;
        save.pet.praise_timer = r;
        if (p->calls & 8) {
            if (anim_busy == 0 && room_idle != 0)
                snd_play(4);
        } else {
            pet_raise_call(8);
        }
        save_request = 1;
        r = 1;
        p = &save.pet;
    }
    if (!(p->status & 0x40)) {
        if (!(rand() & 1))
            p->status |= 0x80;
    }
done:
    return r;
}

/*
 * Make a poop: x01C = number of poops (the 6th raises call 8; leaving more than five makes
 * the character sick, see F43 / F44); x037[i] = poop type (1 / 3 for babies, 0 / 2
 * otherwise, the higher one when sick).
 */
int pet_make_poop(void)
{
    u8 *q;
    u8 *q2;

    if (save.pet.poop_count <= 5) {
        if (save.pet.life_stage == 1) {
            if (save.pet.status & 1) {
                q = (u8 *)&save.pet + save.pet.poop_count;
                q[0x37] = 3;
            } else {
                q = (u8 *)&save.pet + save.pet.poop_count;
                q[0x37] = 1;
            }
        } else {
            if (save.pet.status & 1) {
                q2 = (u8 *)&save.pet + save.pet.poop_count;
                q2[0x37] = 2;
            } else {
                q = (u8 *)&save.pet + save.pet.poop_count;
                q[0x37] = 0;
            }
        }
    }
    if (save.pet.poop_count != 0xff)
        save.pet.poop_count++;
    if (save.pet.poop_count == 6)
        pet_raise_call(8);
    return 0;
}

/*
 * Sleep state from the RTC hour (rtc_get_hour) / minute (rtc_get_minute): 0 = awake,
 * 3 = asleep (outside the character's wake x015 .. sleep x016 hours of the table; babies
 * nap at age 30..34 minutes; eggs and collapsed characters (x011 & 4) count as awake),
 * 1 = just woke up (first 15 minutes of the wake hour), 2 = getting sleepy (last 15
 * minutes before bedtime); 1 and 2 only when x011 == 0.
 */
int pet_sleep_state(void)
{
    u8 h;
    struct DlCharaRecord *p;

    h = rtc_get_hour();
    if (save.pet.life_stage == 0)
        return 0;
    if (save.pet.life_stage == 1) {
        if (save.pet.age_minutes >= 30 && save.pet.age_minutes <= 34)
            return 3;
        return 0;
    }
    if (save.pet.x000 <= 5000) {
        if (save.pet.status & 4)
            return 0;
        if (chara_table[save.pet.chara_row].wake_hour > h)
            return 3;
        if (chara_table[save.pet.chara_row].sleep_hour <= h)
            return 3;
        if (chara_table[save.pet.chara_row].wake_hour == h && save.pet.status == 0) {
            if ((u8)(rtc_get_minute() - 1) <= 14)
                return 1;
        }
        if (chara_table[save.pet.chara_row].sleep_hour - 1 == h && save.pet.status == 0 && rtc_get_minute() > 0x2c)
            return 2;
        return 0;
    } else {
        p = dl_chara_record(save.pet.chara_row + 0x77);
        if (save.pet.status & 4)
            return 0;
        if (p->wake_hour > h)
            return 3;
        if (p->sleep_hour <= h)
            return 3;
        if (p->wake_hour == h && save.pet.status == 0) {
            if ((u8)(rtc_get_minute() - 1) <= 14)
                return 1;
        }
        if (p->sleep_hour - 1 == h && save.pet.status == 0 && rtc_get_minute() > 0x2c)
            return 2;
        return 0;
    }
}

struct CharaEntryCharmView {
    u8 pad00[0x17];
    u8 charm;
    u8 pad18[0x16];
};
struct DlCharaCharmView {
    u8 pad00[0x16];
    u8 charm;
};
/*
 * Value / "charm" of the character: table byte x017 (x016 for downloaded characters),
 * +10 or +5 for each worn accessory / outfit (by item_pet_preference on the worn item: kind 1
 * +10, 2 nothing, others +5), + 8/6/4/2 by the care mistakes x00f (0, <= 3, <= 6, more).
 * Capped at 255. Probably used by the shop / Bingo.
 */
u8 pet_charm(void)
{
    u16 val;

    if (save.pet.x000 <= 5000)
        { int i = save.pet.chara_row; const struct CharaEntryCharmView *e = &chara_table[i]; val = e->charm; }
    else
        val = ((struct DlCharaCharmView *)dl_chara_record(save.pet.chara_row + 0x77))->charm;

    if (save.pet.worn_accessory != 0) {
        switch (item_pet_preference((struct TamagoFile *)(item_record_by_index(D_2535F50[save.pet.worn_accessory])))) {
        case 1:
            val += 10;
            break;
        case 2:
            break;
        default:
            val += 5;
            break;
        }
    }
    if (save.pet.worn_clothes != 0) {
        switch (item_pet_preference((struct TamagoFile *)(item_record_by_index(D_2535F68[save.pet.worn_clothes])))) {
        case 1:
            val += 10;
            break;
        case 2:
            break;
        default:
            val += 5;
            break;
        }
    }

    if (save.pet.care_mistakes == 0)
        val += 8;
    else if (save.pet.care_mistakes <= 3)
        val += 6;
    else if (save.pet.care_mistakes <= 6)
        val += 4;
    else
        val += 2;

    if (val > 255)
        val = 255;
    return val;
}

/*
 * Table field x018 of character n (downloaded: record x017).
 */
u8 chara_fit_class(s16 n)
{
    if (n <= 5000)
        return chara_table[n].fit_class;
    return dl_chara_record(n + 0x77)->fit_class;
}

/*
 * Downloaded-character code (two ASCII letters: 'A'..'Z' + 0xFF, or "AA".."AF") -> 0..31,
 * 0x25 = unknown. Meaning unclear: a class code, or a blood-type / sign code.
 */
u16 dl_code_to_class(u16 a)
{
    switch (a) {
    case 0x41ff: return 0;
    case 0x42ff: return 1;
    case 0x43ff: return 2;
    case 0x44ff: return 3;
    case 0x45ff: return 4;
    case 0x46ff: return 5;
    case 0x47ff: return 6;
    case 0x48ff: return 7;
    case 0x49ff: return 8;
    case 0x4aff: return 9;
    case 0x4bff: return 10;
    case 0x4cff: return 11;
    case 0x4dff: return 12;
    case 0x4eff: return 13;
    case 0x4fff: return 14;
    case 0x50ff: return 15;
    case 0x51ff: return 16;
    case 0x52ff: return 17;
    case 0x53ff: return 18;
    case 0x54ff: return 19;
    case 0x55ff: return 20;
    case 0x56ff: return 21;
    case 0x57ff: return 22;
    case 0x58ff: return 23;
    case 0x59ff: return 24;
    case 0x5aff: return 25;
    case 0x4141: return 26;
    case 0x4142: return 27;
    case 0x4143: return 28;
    case 0x4144: return 29;
    case 0x4145: return 30;
    case 0x4146: return 31;
    }
    return 0x25;
}

/*
 * Character id -> its 0..31 code: table x020, or the downloaded record's x01E converted by
 * dl_code_to_class; 0x25 if unknown.
 */
int chara_class(int id)
{
    struct DlCharaRecord *p;
    int i;
    u16 r;

    if (id <= 5000) {
        for (i = 0; ; i++) {
            if (chara_table[i].id == id) {
                r = chara_table[i].class_code;
                break;
            }
            if (chara_table[i].id == 0xffff) {
                r = 0x25;
                break;
            }
        }
    } else {
        r = 0x25;
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->id == id) {
                r = dl_code_to_class(p->x01e);
                break;
            }
        }
    }
    return r;
}

/*
 * Character id -> pointer to its 6-byte field at +10 of the table entry (+9 of a
 * downloaded record; a name field?), 0 if unknown.
 */
u8 *chara_trait_caps(int id)
{
    struct DlCharaRecord *p;
    int i;
    u8 *r;

    if (id <= 5000) {
        for (i = 0; ; i++) {
            if (chara_table[i].id == id) {
                r = (u8 *)&chara_table[i] + 10;
                break;
            }
            if (chara_table[i].id == 0xffff) {
                r = 0;
                break;
            }
        }
    } else {
        r = 0;
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->id == id) {
                r = (u8 *)p + 9;
                break;
            }
        }
    }
    return r;
}

/*
 * Care mistake (an ignored call): x00F + 1 (and the partner's visit_partner.care_mistakes). Every 4th
 * mistake past the baby stage starts the bad-care departure (x011 |= 0x40, x012 = 0, 0x80
 * variant at random, call 8); otherwise, unless already sick or departing (x011 & 0x41),
 * a sickness (pet_get_sick) with the character's probability (table x014 %). Returns 1
 * when something happened.
 */
u8 pet_care_mistake(void)
{
    struct PetState *p;
    struct PetState *q;
    int t;
    int r = 0;

    save.pet.care_mistakes++;
    if (visit_partner.mode == 2)
        visit_partner.care_mistakes++;
    p = &save.pet;
    if ((p->care_mistakes & 3) == 0 && save.pet.life_stage > 1) {
        if ((p->status & 0x40) == 0) {
            p->status |= 0x40;
            p->decline_minutes = 0;
            if (!(rand() & 1))
                p->status |= 0x80;
            pet_raise_call(8);
            save_request = 1;
            return 1;
        }
        return 0;
    } else {
        if (save.pet.care_mistakes == 0xff)
            save.pet.care_mistakes = 3;
        q = &save.pet;
        if (q->status & 0x41)
            return 0;
        if (q->x000 <= 0x1388) {
            t = chara_table[q->chara_row].sick_percent;
            r = rand();
        } else {
            t = dl_chara_record(save.pet.chara_row + 0x77)->sick_percent;
            r = rand();
        }
        if (t > r % 100) {
            pet_get_sick();
            return 1;
        }
        return 0;
    }
}

/*
 * Per-minute clock events (called from the RTC handler rtc_isr; month c0, day c1,
 * hour v2, minute v3 from the RTC; skipped while pet_life_paused = a new character is being set
 * up):
 *  - 07:00 = new day: evo_daily_countdown / room_weather_draw_daily, the daily flags 0x44/0x45/0x4A..0x4D
 *    cleared, pet.living_dirt = consecutive days counter (past 3 it costs 5 happiness), the daily
 *    counters pet.kitchen_dirt..125C reset, r = 1 with a stage change pending; pet.neglect_minutes > 0x10DF
 *    (3 days without care) sets x011 bit 1 (neglected) + call 8 + maybe the 0x80 variant.
 *  - 9/12/16/19:00: room_weather_update_now (shop / town refresh?).
 *  - 00:00 = date change: town_farm_midnight (the farm), x00b = 0, age in days pet.age_days from
 *    x014 (minutes / 0x5A0), the Christmas call 0x20 on 12/25 for stage > 1, flags
 *    0x42/0x43 cleared.
 *  - stage > 2 with nothing pending: call 4 at 10:00 / 15:00 (pose 0x92) or 8:00 / 16:00
 *    (pose 0xDB).
 *  - awake and well, stage > 1: the owner's birthday (pet.owner_bday_month / owner_bday_day[0] = month /
 *    day) gives call 0x10 at 7:30 / 12:30 / 19:30, a character's birthday (chara_birthday_on)
 *    call 0x40 at 8:30 / 13:30 / 15:30.
 * Returns 1 when something needs to be shown.
 */
u8 pet_clock_events(void)
{
    int v2, v3;
    u8 c0, c1;
    u8 r = 0;
    u8 t;
    struct PetState *p;
    struct PetState *e;

    if (pet_life_paused != 0)
        return 0;
    c0 = rtc_get_month();
    c1 = rtc_get_day();
    v2 = rtc_get_hour();
    v3 = rtc_get_minute();
    if (v2 == 7 && v3 == 0) {
        evo_daily_countdown();
        room_weather_draw_daily();
        pet_set_unlock_flag(0x44, 0);
        pet_set_unlock_flag(0x4a, 0);
        pet_set_unlock_flag(0x45, 0);
        pet_set_unlock_flag(0x4b, 0);
        pet_set_unlock_flag(0x4c, 0);
        pet_set_unlock_flag(0x4d, 0);
        save.pet.x082 = 0;
        if (save.pet.weather_now == 4) {
            save.pet.app_item_lock = 0;
            D_0003FF7 = 0;
        }
        save.pet.living_dirt++;
        if (save.pet.living_dirt > 3)
            pet_sub_happiness(5);
        if (save.pet.living_dirt > 4)
            save.pet.living_dirt = 5;
        save.pet.kitchen_dirt = 0;
        save.pet.toilet_dirt = 0;
        save.pet.bath_dirt = 0;
        save.pet.bedroom_dirt = 0;
        if ((s8)save.pet.calls < 0)
            r = 1;
        p = &save.pet;
        if (save.pet.neglect_minutes > 0x10df && p->status == 0) {
            p->status |= 2;
            pet_raise_call(8);
            if ((p->status & 0x41) == 0 && (rand() & 1))
                p->status |= 0x80;
            if (sys_display_state == 2)
                sys_display_wake(0);
            if (anim_busy == 0 && room_idle != 0)
                snd_play(4);
        }
    }
    if (v3 == 0 && (v2 == 9 || v2 == 12 || v2 == 16 || v2 == 19))
        room_weather_update_now();
    if (v2 == 0 && v3 == 0) {
        town_farm_midnight();
        p = &save.pet;
        p->meals_today = 0;
        save.pet.age_days = (p->age_minutes + 0x59f) / 0x5a0;
        if (c0 == 12 && c1 == 25 && save.pet.life_stage > 1 && p->status == 0) {
            r = 1;
            pet_raise_call(0x20);
        }
        pet_set_unlock_flag(0x42, 0);
        pet_set_unlock_flag(0x43, 0);
        if (save.pet.bingo_plays_today != 0 && save.pet.life_stage > 1)
            save.pet.bingo_plays_today = 0;
    }
    e = &save.pet;
    if (save.pet.life_stage > 2 && pet_sitter_state.active == 0 && e->calls == 0 && e->status == 0 && room_view == 0) {
        if ((v2 == 10 || v2 == 15) && v3 == 0 && pet_raise_call(4) != 0)
            save.pet.idle_pose = 0x92;
        if ((v2 == 8 || v2 == 16) && v3 == 0 && pet_raise_call(4) != 0)
            save.pet.idle_pose = 0xdb;
    }
    if (pet_sleep_state() == 3)
        return r;
    if (save.pet.status != 0)
        return r;
    if (save.pet.life_stage <= 1)
        return r;
    if (save.pet.owner_bday_month == c0 && save.pet.owner_bday_day == c1 && (v2 == 7 || v2 == 12 || v2 == 19) && v3 == 0x1e) {
        r = 1;
        pet_raise_call(0x10);
    }
    if (chara_birthday_on(c0, c1) >= 0 && (v2 == 8 || v2 == 13 || v2 == 15) && v3 == 0x1e) {
        r = 1;
        pet_raise_call(0x40);
    }
    return r;
}

/*
 * THE PER-MINUTE LIFE TICK (called from the RTC handler rtc_isr at each minute).
 * Returns 1 when the screen must show something.
 *  - pet.ten_day_minutes counts minutes while x011 & 0x14 is clear (0x3840 -> pet.ten_day_event_due = 1);
 *    nothing else runs during an outing tick (outing_minute_tick / job_minute_tick), with a
 *    visitor, or during character setup (pet_life_paused).
 *  - x014 = age in minutes (max 0x15F35F), paused while collapsed (x011 & 4). Stage events
 *    (a pending stage change, x00a bit 7, counts as the next stage): the egg (0) hatches
 *    at once (call 0x80); the baby (1) gets sick at 10 and 45 minutes (+ call 8) and
 *    changes stage at 60; the child (2) at 0xB7C = 2940 minutes (49 h).
 *  - pet.neglect_minutes = minutes since the last care action; at 0x10E0 (72 h) with x011 == 0 and
 *    awake the character gets x011 bit 1 (neglected) + call 8 + maybe the 0x80 variant.
 *  - Departure ladders: while x011 & 0x44 (5th sickness or bad care) x012 counts up to
 *    0x870 (36 h): then x011 |= 4 (collapsed), all calls cleared, the care countdowns
 *    pet.bath_timer/BE/C0 reset, save_request = 1; warning calls at 0x88E..0x8A7, display wakes at
 *    0x86F / 0x8AA, x011 |= 8 at 0x8AB and 0x10 beyond (the "leaves" state: pet.gone_minutes then
 *    counts up to 0x59F). The same ladder runs on x006 when hunger is at 0 (starving).
 *  - Hunger: x008 counts down from the table rate x012; at 0 one hunger heart is lost
 *    (every second loss arms the x01e countdown = 900 child / 30 baby / 300 and sets
 *    pet_request_bits bit 0) and at 0 hearts the hungry call. x009 / x00e tick down the
 *    15-minute call windows (pet_is_away() == 0 = not in a menu): an ignored call is a
 *    care mistake (pet_care_mistake) and may raise call 8.
 *  - Happiness: x00d counts down from the table rate x013; at 0 three hearts are lost
 *    (pet_sub_happiness(3), six during job) and the unhappy call is raised.
 *  - pet.whim_call_timer (call 4) and pet_alert_call_timer[0] (call 8) expire into "call cleared + redraw".
 */
u8 pet_minute_tick(void)
{
    u8 r = 0;
    u8 n = save.pet.life_stage;
    u8 f = 0;
    u8 c;
    u16 v;

    if ((save.pet.status & 0x14) == 0)
        save.pet.ten_day_minutes++;
    if (save.pet.ten_day_minutes > 0x383f) {
        save.pet.ten_day_event_due = 1;
        save.pet.ten_day_minutes = 0;
    }
    if (outing_minute_tick() != 0)
        return 1;
    if (job_minute_tick() != 0)
        return 1;
    if (visit_partner.mode != 0)
        return 1;
    if (pet_life_paused != 0)
        return 0;
    if (save.pet.age_minutes <= 0x15f35f && !(save.pet.status & 4))
        save.pet.age_minutes++;
    if ((s8)save.pet.calls < 0)
        n++;
    switch (n) {
    case 1:
        if (save.pet.age_minutes == 10 || save.pet.age_minutes == 0x2d) {
            c = save.pet.status & 1;
            pet_get_sick();
            if (c == 0) {
                pet_raise_call(8);
                r = 1;
                f = 1;
            }
        } else if (save.pet.age_minutes == 0x3c) {
            r = 1;
            f = r;
            pet_raise_call(0x80);
        }
        break;
    case 2:
        if (save.pet.age_minutes != 0xb7c)
            break;
    case 0:
        r = 1;
        f = r;
        pet_raise_call(0x80);
        break;
    }
    if (pet_sitter_state.active != 0)
        return 0;
    if (save.pet.neglect_minutes <= 0x10df)
        save.pet.neglect_minutes++;
    {
        struct PetState *p = &save.pet;
        if (save.pet.neglect_minutes == 0x10e0 && p->status == 0 && pet_sleep_state() <= 2) {
            p->status |= 2;
            pet_raise_call(8);
            if ((p->status & 0x41) == 0 && (rand() & 1))
                p->status |= 0x80;
            if (sys_display_state == 2)
                sys_display_wake(0);
            if (anim_busy == 0 && room_idle != 0)
                snd_play(4);
        }
    }
    if ((save.pet.status & 0x44) && save.pet.starve_minutes <= 0x86f && f == 0) {
        if (++save.pet.decline_minutes == 0x870) {
            save.pet.status |= 4;
            if (save.pet.calls & 8) {
                if (anim_busy == 0 && room_idle != 0)
                    snd_play(4);
            } else {
                pet_raise_call(8);
            }
            pet_clear_call(4);
            pet_clear_call(1);
            pet_clear_call(2);
            save.pet.toilet_timer = 0;
            save.pet.cleaning_timer = 0;
            save.pet.bath_timer = 0;
            save.pet.praise_timer = 0;
            save_request = 1;
            r = 1;
        } else if (save.pet.decline_minutes == 0x86f) {
            sys_display_wake(0);
        } else if (save.pet.decline_minutes == 0x88e) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.decline_minutes == 0x898) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.decline_minutes == 0x89d) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.decline_minutes == 0x8a2) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.decline_minutes == 0x8a7) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.decline_minutes == 0x8aa) {
            sys_display_wake(0);
        } else if (save.pet.decline_minutes == 0x8ab) {
            pet_raise_call(8);
            save.pet.status |= 8;
            save_request = 1;
            r = 1;
        } else if (save.pet.decline_minutes > 0x8ab) {
            if ((save.pet.status & 0x10) == 0) {
                save.pet.status |= 0x10;
                pet_clear_call(8);
                sys_debug_fast_forward = 0;
                r = 1;
            }
            save.pet.decline_minutes = 0x8ac;
        }
    }
    if (outing.destination == 0
        && ((save.pet.hunger == 0 && save.pet.life_stage != 0 && f == 0 && save.pet.decline_minutes <= 0x86f)
            || (save.pet.hunger != 0 && save.pet.starve_minutes > 0x86f && save.pet.life_stage != 0
                && save.pet.decline_minutes <= 0x86f && f == 0))) {
        if (++save.pet.starve_minutes == 0x870) {
            save.pet.status |= 4;
            pet_raise_call(8);
            pet_clear_call(4);
            pet_clear_call(1);
            pet_clear_call(2);
            save.pet.toilet_timer = 0;
            save.pet.cleaning_timer = 0;
            save.pet.bath_timer = 0;
            save.pet.praise_timer = 0;
            save_request = 1;
            r = 1;
        } else if (save.pet.starve_minutes == 0x86f) {
            sys_display_wake(0);
        } else if (save.pet.starve_minutes == 0x88e) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.starve_minutes == 0x898) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.starve_minutes == 0x89d) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.starve_minutes == 0x8a2) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.starve_minutes == 0x8a7) {
            pet_raise_call(8);
            r = 1;
        } else if (save.pet.starve_minutes == 0x8aa) {
            sys_display_wake(0);
        } else if (save.pet.starve_minutes == 0x8ab) {
            pet_raise_call(8);
            save.pet.status |= 8;
            save_request = 1;
            r = 1;
        } else if (save.pet.starve_minutes > 0x8ab) {
            if ((save.pet.status & 0x10) == 0) {
                save.pet.status |= 0x10;
                pet_clear_call(8);
                sys_debug_fast_forward = 0;
                r = 1;
            }
            save.pet.starve_minutes = 0x8ac;
        }
    }
    if (save.pet.status & 0x10) {
        if (save.pet.gone_minutes <= 0x59f)
            save.pet.gone_minutes++;
    } else {
        save.pet.gone_minutes = 0;
    }
    if (pet_sleep_state() == 3)
        return 0;
    if (save.pet.status & 0x1c)
        return r;
    if (save.pet.hunger_timer != 0 && --save.pet.hunger_timer == 0) {
        if (save.pet.hunger != 0) {
            if (--save.pet.hunger == 0)
                save.pet.starve_minutes = 0;
            if (++save.pet.hunger_loss_parity > 1) {
                save.pet.hunger_loss_parity = 0;
                if (outing.destination == 0) {
                    if (save.pet.training[0] == 0xff)
                        goto def;
                    if (save.pet.life_stage == 2)
                        v = 900;
                    else if (save.pet.life_stage == 1 || (save.pet.life_stage > 2 && save.pet.training[0] != 0xff))
                        v = 30;
                    else
                    def:
                        v = 300;
                    save.pet.toilet_timer = v;
                }
                if (save.pet.toilet_timer != 0)
                    pet_request_bits |= 1;
            }
        }
        if (save.pet.hunger == 0) {
            r = 1;
            c = save.pet.calls;
            pet_raise_call(1);
            if (!(c & 1))
                save.pet.hungry_call_timer++;
        }
        if (save.pet.x000 <= 0x1388)
            save.pet.hunger_timer = chara_table[save.pet.chara_row].hunger_period;
        else
            save.pet.hunger_timer = dl_chara_record(save.pet.chara_row + 0x77)->hunger_period;
    }
    if (pet_is_away() == 0 && save.pet.hungry_call_timer != 0 && --save.pet.hungry_call_timer == 0) {
        pet_clear_call(1);
        if (pet_care_mistake()) {
            pet_raise_call(8);
            r = 1;
        }
    }
    if (save.pet.happiness_timer != 0 && --save.pet.happiness_timer == 0) {
        if (job.kind != 0)
            pet_sub_happiness(3);
        if (pet_sub_happiness(3)) {
            r = 1;
            c = save.pet.calls;
            pet_raise_call(2);
            if (!(c & 2))
                save.pet.unhappy_call_timer++;
        }
        if (save.pet.x000 <= 0x1388)
            save.pet.happiness_timer = chara_table[save.pet.chara_row].happiness_period;
        else
            save.pet.happiness_timer = dl_chara_record(save.pet.chara_row + 0x77)->happiness_period;
    }
    if (pet_is_away() == 0 && save.pet.unhappy_call_timer != 0 && --save.pet.unhappy_call_timer == 0) {
        pet_clear_call(2);
        if (pet_care_mistake()) {
            pet_raise_call(8);
            r = 1;
        }
    }
    if (save.pet.whim_call_timer != 0 && --save.pet.whim_call_timer == 0) {
        pet_clear_call(4);
        save_request = 1;
    }
    if (save.pet.alert_call_timer != 0) {
        if (--save.pet.alert_call_timer != 0)
            return r;
        pet_clear_call(8);
        save_request = 1;
    }
    return r;
}

/*
 * Random number in [0, a) different from b.
 */
u16 util_rand_except(u16 a, u16 b)
{
    u16 r;

    if (a > b)
        a--;
    r = rand() % a;
    if (r >= b)
        r++;
    return r;
}

/*
 * Shuffle n bytes of buf (random swaps; n == 2: swap with 50 % chance).
 */
void util_rand_shuffle(u8 *buf, int n)
{
    u16 j, i, k, cnt;
    u8 t;

    if (n <= 1)
        return;
    j = n;
    if (n == 2) {
        int r = rand() & 1;
        cnt = 1;
        if (r == 0)
            return;
    } else {
        cnt = j;
    }
    for (k = 0; k < cnt; k++) {
        j = util_rand_except(n, j);
        i = util_rand_except(n, j);
        t = buf[j];
        buf[j] = buf[i];
        buf[i] = t;
    }
}

/* Text tables of this file (all in the game charset, see tools/charset.py): the room /
 * background names (ノーマル, ピンクノート, ...), the status menu (キャラパラメータ /
 * プロフィール / せってい), the birthday greetings spoken by each character (D_2542D6C..,
 * one per character, prefixed by the owner's name), the character names (D_2543878..,
 * babies first: よちよっち, がおっち, ... then adults), the idle speech lines per
 * character (D_2543CF8.. "talk" and D_2544E14.. "play"), the proposal lines (D_2545F84..),
 * the visitor / outing characters' lines, the mail templates (D_2559878.. = the fixed
 * messages a user can send; mail bodies are chosen from these), the greeting-fortune
 * results (D_25596F4..) and the daily fortune texts (D_255A228..). */
const u8 D_2542848[] = _("ノーマル");
const u8 D_2542854[] = _("ピンクノート");
const u8 D_2542864[] = _("パープルアラビア");
const u8 D_2542878[] = _("ブルースター");
const u8 D_2542888[] = _("きゃんでぃすかい");
/* pictures (in the graphics area) of the room tables below */
extern u8
    D_20DB2A0[], D_20DCEE4[], D_20DEB38[], D_20E0788[], D_20E23DC[],
    D_20DA480[], D_20DC0B8[], D_20DDD10[], D_20DF960[], D_20E15AC[],
    D_20DB9B4[], D_20DD5FC[], D_20DF24C[], D_20E0E9C[], D_20E2AF4[],
    D_20DBB1C[], D_20DD768[], D_20DF3B8[], D_20E1008[], D_20E2C60[],
    D_20DBC84[], D_20DD8D4[], D_20DF520[], D_20E1170[], D_20E2DCC[],
    D_20DBDEC[], D_20DDA40[], D_20DF68C[], D_20E12DC[], D_20E2F34[],
    D_20DBF50[], D_20DDBA8[], D_20DF7F4[], D_20E1444[], D_20E309C[],
    D_20DAB94[], D_20DC7D0[], D_20DE424[], D_20E0074[], D_20E1CC4[],
    D_20DACF8[], D_20DC93C[], D_20DE590[], D_20E01DC[], D_20E1E2C[],
    D_20DAE64[], D_20DCAA8[], D_20DE6F8[], D_20E0348[], D_20E1F98[],
    D_20DAFD0[], D_20DCC14[], D_20DE864[], D_20E04B4[], D_20E2104[],
    D_20DB138[], D_20DCD80[], D_20DE9CC[], D_20E0620[], D_20E2270[],
    D_20E441C[], D_20E4E14[], D_20E530C[], D_20E5808[], D_20E5D04[],
    D_20E6200[], D_20E6700[], D_20E6C04[], D_20E710C[], D_20E7AF8[],
    D_20E84E8[], D_20E8EE0[], D_20E98D8[], D_20EA2D4[], D_20EACD4[],
    D_20EB6DC[], D_20EC0E4[], D_20ECAF0[], D_20ED500[], D_20EDF14[],
    D_20EE92C[], D_20EF348[], D_20F4688[], D_20F4768[];
const u8 *const ui_theme_names[] = { D_2542848, D_2542854, D_2542864, D_2542878, D_2542888 };
const u8 *const ui_top_bar_pics[] = {
    D_20DB2A0, D_20DCEE4, D_20DEB38, D_20E0788, D_20E23DC,
};
const u8 *const ui_bottom_bar_pics[] = {
    D_20DA480, D_20DC0B8, D_20DDD10, D_20DF960, D_20E15AC,
};
const u8 *const ui_icon1_pics[] = {
    D_20DB9B4, D_20DD5FC, D_20DF24C, D_20E0E9C, D_20E2AF4,
};
const u8 *const ui_icon2_pics[] = {
    D_20DBB1C, D_20DD768, D_20DF3B8, D_20E1008, D_20E2C60,
};
const u8 *const ui_icon3_pics[] = {
    D_20DBC84, D_20DD8D4, D_20DF520, D_20E1170, D_20E2DCC,
};
const u8 *const ui_icon4_pics[] = {
    D_20DBDEC, D_20DDA40, D_20DF68C, D_20E12DC, D_20E2F34,
};
const u8 *const ui_icon5_pics[] = {
    D_20DBF50, D_20DDBA8, D_20DF7F4, D_20E1444, D_20E309C,
};
const u8 *const ui_icon6_pics[] = {
    D_20DAB94, D_20DC7D0, D_20DE424, D_20E0074, D_20E1CC4,
};
const u8 *const ui_icon7_pics[] = {
    D_20DACF8, D_20DC93C, D_20DE590, D_20E01DC, D_20E1E2C,
};
const u8 *const ui_icon8_pics[] = {
    D_20DAE64, D_20DCAA8, D_20DE6F8, D_20E0348, D_20E1F98,
};
const u8 *const ui_icon9_pics[] = {
    D_20DAFD0, D_20DCC14, D_20DE864, D_20E04B4, D_20E2104,
};
const u8 *const ui_call_icon_pics[] = {
    D_20DB138, D_20DCD80, D_20DE9CC, D_20E0620,
    D_20E2270, D_20E441C, D_20E441C, D_20E441C,
    D_20E441C, D_20E441C, D_20E4E14, D_20E530C,
    D_20E5808, D_20E5D04, D_20E6200, D_20E6700,
    D_20E6C04, D_20E710C, D_20E7AF8, D_20E84E8,
    D_20E8EE0, D_20E98D8, D_20EA2D4, D_20EACD4,
    D_20EB6DC, D_20EC0E4, D_20ECAF0, D_20ED500,
    D_20EDF14, D_20EE92C, D_20EF348, D_20EF348,
    D_20EF348, D_20EF348, D_20EF348, (const u8 *)D_20F44C8,
    (const u8 *)D_20F45A8, D_20F4688, D_20F4768, D_20EF348,
    D_20EF348, D_20EF348, D_20EF348,
};
const u8 D_2542A38[] = _("キャラパラメータ");
const u8 D_2542A4C[] = _("プロフィール");
const u8 D_2542A5C[] = _("せってい");
const u8 *const ui_status_menu_labels[] = { D_2542A38, D_2542A4C, D_2542A5C };
const u32 chara_value_table[32] = { 0 };
/* pose lists for chara_render_poses_flash: {count, {w, h, pose}...} */
const u8 D_2542AF4[] = { 1, 0x30, 0x48, 0x0d };
const u8 D_2542AF8[] = { 1, 0x1e, 0x38, 2 };
const u8 D_2542AFC[] = { 1, 0x30, 0x48, 3 };
/* Shift-JIS text, not in the game charset (unused?):
 *   はつめいとはしっぱいをくりかえししこうさくごのすえにうまれるものなのです。いちどくらいのしっぱいであきらめてはいけません… (+ 12 full-width spaces)
 *   「ふかのう」とかんたんにいってはダメ。きのう「ゆめ」だったものがきょうは「きぼう」となりあしたには「げんじつ」となっていくものだから… (+ 12 full-width spaces)
 *   めさきのテストのためのべんきょうばかりでは、けっしてつかえるちしきにはなりません。もくてきをもってべんきょうしましょうね… (+ 12 full-width spaces)
 *   きくはいっときのはじきかぬはいっしょうのはじ…べんきょうでわからないことがあったら、せんせいやともだちにどんどんしつもんするといいですね… (+ 12 full-width spaces)
 */
const char D_2542B00[] =
    "\x82\xcd\x82\xc2\x82\xdf\x82\xa2\x82\xc6\x82\xcd\x82\xb5\x82\xc1\x82\xcf\x82\xa2\x82\xf0\x82\xad\x82\xe8\x82\xa9\x82\xa6\x82\xb5"
    "\x82\xb5\x82\xb1\x82\xa4\x82\xb3\x82\xad\x82\xb2\x82\xcc\x82\xb7\x82\xa6\x82\xc9\x82\xa4\x82\xdc\x82\xea\x82\xe9\x82\xe0\x82\xcc"
    "\x82\xc8\x82\xcc\x82\xc5\x82\xb7\x81\x42\x82\xa2\x82\xbf\x82\xc7\x82\xad\x82\xe7\x82\xa2\x82\xcc\x82\xb5\x82\xc1\x82\xcf\x82\xa2"
    "\x82\xc5\x82\xa0\x82\xab\x82\xe7\x82\xdf\x82\xc4\x82\xcd\x82\xa2\x82\xaf\x82\xdc\x82\xb9\x82\xf1\x81\x63\x81\x40\x81\x40\x81\x40"
    "\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40"
    "\x81\x75\x82\xd3\x82\xa9\x82\xcc\x82\xa4\x81\x76\x82\xc6\x82\xa9\x82\xf1\x82\xbd\x82\xf1\x82\xc9\x82\xa2\x82\xc1\x82\xc4\x82\xcd"
    "\x83\x5f\x83\x81\x81\x42\x82\xab\x82\xcc\x82\xa4\x81\x75\x82\xe4\x82\xdf\x81\x76\x82\xbe\x82\xc1\x82\xbd\x82\xe0\x82\xcc\x82\xaa"
    "\x82\xab\x82\xe5\x82\xa4\x82\xcd\x81\x75\x82\xab\x82\xda\x82\xa4\x81\x76\x82\xc6\x82\xc8\x82\xe8\x82\xa0\x82\xb5\x82\xbd\x82\xc9"
    "\x82\xcd\x81\x75\x82\xb0\x82\xf1\x82\xb6\x82\xc2\x81\x76\x82\xc6\x82\xc8\x82\xc1\x82\xc4\x82\xa2\x82\xad\x82\xe0\x82\xcc\x82\xbe"
    "\x82\xa9\x82\xe7\x81\x63\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40"
    "\x82\xdf\x82\xb3\x82\xab\x82\xcc\x83\x65\x83\x58\x83\x67\x82\xcc\x82\xbd\x82\xdf\x82\xcc\x82\xd7\x82\xf1\x82\xab\x82\xe5\x82\xa4"
    "\x82\xce\x82\xa9\x82\xe8\x82\xc5\x82\xcd\x81\x41\x82\xaf\x82\xc1\x82\xb5\x82\xc4\x82\xc2\x82\xa9\x82\xa6\x82\xe9\x82\xbf\x82\xb5"
    "\x82\xab\x82\xc9\x82\xcd\x82\xc8\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81\x42\x82\xe0\x82\xad\x82\xc4\x82\xab\x82\xf0\x82\xe0\x82\xc1"
    "\x82\xc4\x82\xd7\x82\xf1\x82\xab\x82\xe5\x82\xa4\x82\xb5\x82\xdc\x82\xb5\x82\xe5\x82\xa4\x82\xcb\x81\x63\x81\x40\x81\x40\x81\x40"
    "\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40"
    "\x82\xab\x82\xad\x82\xcd\x82\xa2\x82\xc1\x82\xc6\x82\xab\x82\xcc\x82\xcd\x82\xb6\x82\xab\x82\xa9\x82\xca\x82\xcd\x82\xa2\x82\xc1"
    "\x82\xb5\x82\xe5\x82\xa4\x82\xcc\x82\xcd\x82\xb6\x81\x63\x82\xd7\x82\xf1\x82\xab\x82\xe5\x82\xa4\x82\xc5\x82\xed\x82\xa9\x82\xe7"
    "\x82\xc8\x82\xa2\x82\xb1\x82\xc6\x82\xaa\x82\xa0\x82\xc1\x82\xbd\x82\xe7\x81\x41\x82\xb9\x82\xf1\x82\xb9\x82\xa2\x82\xe2\x82\xc6"
    "\x82\xe0\x82\xbe\x82\xbf\x82\xc9\x82\xc7\x82\xf1\x82\xc7\x82\xf1\x82\xb5\x82\xc2\x82\xe0\x82\xf1\x82\xb7\x82\xe9\x82\xc6\x82\xa2"
    "\x82\xa2\x82\xc5\x82\xb7\x82\xcb\x81\x63\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40\x81\x40"
    "\x81\x40";
const char *const D_2542D68[] = { D_2542B00 };
const u8 D_2542D6C[] = _("たんじょうびおめでとーでちゅ \0");
const u8 D_2542D90[] = _("たんじょびーがお? ");
const u8 D_2542DA8[] = _("のたんじょうび～☼ おめでとー♥ \0");
const u8 D_2542DD0[] = _("  おめでとゆら～♥ \0");
const u8 D_2542DEC[] = _("のたんじょうびでしょーが! ぽか☼ ");
const u8 D_2542E14[] = _("のたんじょうびおめでとうふよっ↑ ふよふよ～っとおいわいするふよっ↑ \0");
const u8 D_2542E60[] = _("のたんじょうびだなも☼ そりゃあめでてぇなも☼ ");
const u8 D_2542E94[] = _("のたんじょうび! サクッと カラッとおいわいどん! ");
const u8 D_2542ECC[] = _("たんじょうびおめでとー! しかくいものもらえたのかな? ");
const u8 D_2542F08[] = _("のたんじょうびだよ! なんてこった! \0");
const u8 D_2542F34[] = _("☼ たんじょうびおめでとうぷっち☼ ");
const u8 D_2542F5C[] = _("のおたんじょうびよ～  おいわいしちゃおーね☼ ");
const u8 D_2542F90[] = _("のたんじょうびです! おめでとうございます! \0");
const u8 D_2542FC4[] = _("のたんじょーびだっち  ケーキたべたいだっち☼ ");
const u8 D_2542FF8[] = _("のたんじょうびなんですか?おめでとうございます! \0");
const u8 D_2543030[] = _("のたんじょうびだぜ  ハッピーバースデー! ");
const u8 D_2543060[] = _("のたんじょうびだろ? おめでとー! おれはねっ3がつ6か!! \0");
const u8 D_25430A4[] = _("たんじょうびおめでとー きょうはきっといいてんき! ");
const u8 D_25430DC[] = _("がたんじょうびなのよね! おめでとう! ");
const u8 D_2543108[] = _("ってきょうがたんじょうびなの! オメデトー♪ \0");
const u8 D_254313C[] = _("のおたんじょうびだね!おめでとー♥ ");
const u8 D_2543164[] = _("のたんじょうびなの? ハッピーバースデー! ");
const u8 D_2543194[] = _("のたんじょうびだねぇ!おめでたいねぇ! ");
const u8 D_25431C0[] = _("がね､きょうたんじょうびなんだって! おめでと～♪ あしたはだれのたんじょうびかな? \0");
const u8 D_254321C[] = _("のたんじょうびですよ  おいわいのメールをおくっておきますね \0");
const u8 D_2543260[] = _("のたんじょうび? はいはいオメデトーだゼ! ");
const u8 D_2543290[] = _("のたんじょうびだっちん! パーティのごちそうたのしみだっちん! ");
const u8 D_25432D4[] = _("のたんじょうびだっちぇ ");
const u8 D_25432F0[] = _("のたんじょうびだよナ! ハッピーバースデーだゼ! \0");
const u8 D_2543328[] = _("のたんじょうびだな╬ よし､オレサマからもいわってやろう! ");
const u8 D_2543368[] = _("のたんじょうびか  さて､プレゼントはなにをおくろうかな? ");
const u8 D_25433A8[] = _("し のたんじょうびですぞWW \0");
const u8 D_25433CC[] = _("のたんじょうびなのさ★ おれからもしゅくふくするよ★ \0");
const u8 D_2543408[] = _("さんのたんじょうびですよ★ ");
const u8 D_2543428[] = _("のたんじょうびらしいわよっ@  ぐるぐるのフラワーアレンジメントをプレゼントしましょっ@@@ \0");
const u8 D_254348C[] = _("のたんじょうびらしいわよ@ めめちゃんのあめちゃんあげちゃう～@@ ");
const u8 D_25434D4[] = _("のたんじょうび! パーティひらいたらまめっちもきてくれるかな?♥♥♥ \0");
const u8 D_2543520[] = _("のたんじょうびっ! せいだいにおいわいしちゃおっ!! ファイヤーーーッ! \0");
const u8 D_2543570[] = _("のたんじょうびだって～ こってりわすれてた～ \0");
const u8 D_25435A4[] = _("のたんじょうびだな♪ バースデーケーキをだーすでー★…ってか? ガッハッハ! \0");
const u8 D_25435F8[] = _("のたんじょうびだねぇ★ おめでとっ♥ \0");
const u8 D_2543624[] = _("に､きょうのしゅやくをゆずるわぁ♥ たんじょうびおめでとう♪ \0");
const u8 D_2543668[] = _("ハッピーバースデー♪おたんじょうびおめでとうチアーズ♪ ディアマイフレンド♪ \0");
const u8 D_25436BC[] = _("のたんじょうび～♥ たんじょうびうらないしてあげようかしら～ん♥イヒヒ♥ \0");
const u8 D_254370C[] = _("{00}");
/* the texts of chara_builtin_text by id, ended by id 0xffff */
struct CharaText {
    u16 id;
    const u8 *text;
};
const struct CharaText chara_text_bday[] = {
    { 0, D_2542D6C }, { 1, D_2542D90 }, { 2, D_2542DA8 },
    { 3, D_2542DD0 }, { 4, D_2542DEC }, { 5, D_2542E14 },
    { 6, D_2542E60 }, { 7, D_2542E94 }, { 0xa, D_2542ECC },
    { 0xb, D_2542F08 }, { 0xc, D_2542F34 }, { 0xd, D_2542F5C },
    { 0x10, D_2542F90 }, { 0x11, D_2542FC4 }, { 0x12, D_2542FF8 },
    { 0x13, D_2543030 }, { 0x14, D_2543060 }, { 0x15, D_25430A4 },
    { 0x3e9, D_25430DC }, { 0x3ea, D_2543108 }, { 0x3eb, D_254313C },
    { 0x3ec, D_2543164 }, { 0x3ed, D_2543194 }, { 0x3ee, D_25431C0 },
    { 0x7d1, D_254321C }, { 0x7d2, D_2543260 }, { 0x7d3, D_2543290 },
    { 0x7d4, D_25432D4 }, { 0x7d5, D_25432F0 }, { 0x7d6, D_2543328 },
    { 0x7d7, D_2543368 }, { 0x7d8, D_25433A8 }, { 0x7d9, D_25433CC },
    { 0x7da, D_2543408 }, { 0xbb9, D_2543428 }, { 0xbba, D_254348C },
    { 0xbbb, D_25434D4 }, { 0xbbc, D_2543520 }, { 0xbbd, D_2543570 },
    { 0xbbe, D_25435A4 }, { 0xbbf, D_25435F8 }, { 0xbc0, D_2543624 },
    { 0xbc1, D_2543668 }, { 0xbc2, D_25436BC }, { 0xffff, D_254370C },
};
const u8 D_2543878[] = _("よちよっち\0");
const u8 D_2543888[] = _("がおっち");
const u8 D_2543894[] = _("うふふっち\0");
const u8 D_25438A4[] = _("ゆららっち\0");
const u8 D_25438B4[] = _("ぽかぽかっち");
const u8 D_25438C4[] = _("ふよふよっち");
const u8 D_25438D4[] = _("にょきにょっち\0");
const u8 D_25438E8[] = _("てんどっち\0");
const u8 D_25438F8[] = _("ひややっち\0");
const u8 D_2543908[] = _("ぱんなこっち");
const u8 D_2543918[] = _("ぷっちけーきっち");
const u8 D_254392C[] = _("りおねっち\0");
const u8 D_254393C[] = _("まめっち");
const u8 D_2543948[] = _("くちぱっち\0");
const u8 D_2543958[] = _("スペイシーっち\0");
const u8 D_254396C[] = _("くろまめっち");
const u8 D_254397C[] = _("おれねっち\0");
const u8 D_254398C[] = _("ラブソラっち");
const u8 D_254399C[] = _("めめっち");
const u8 D_25439A8[] = _("ひめスペっち");
const u8 D_25439B8[] = _("ラブリっち\0");
const u8 D_25439C8[] = _("メロディっち");
const u8 D_25439D8[] = _("ねぇねっち\0");
const u8 D_25439E8[] = _("きゃんでぃぱく2");
const u8 D_25439FC[] = _("まめラボっち");
const u8 D_2543A0C[] = _("やんちゃまめっち");
const u8 D_2543A20[] = _("くちぱっちん");
const u8 D_2543A30[] = _("ジーニストくちぱ");
const u8 D_2543A44[] = _("キラリスペイシー");
const u8 D_2543A58[] = _("キングスペイシー");
const u8 D_2543A6C[] = _("くろまめセンパイ");
const u8 D_2543A80[] = _("オタくろまめ");
const u8 D_2543A90[] = _("おれおとなっち\0");
const u8 D_2543AA4[] = _("アストロソラっち");
const u8 D_2543AB8[] = _("ぐるめめクイーン");
const u8 D_2543ACC[] = _("めめちゃんおばっち\0");
const u8 D_2543AE4[] = _("まめラブひめスペ");
const u8 D_2543AF8[] = _("ラブリーファイヤー\0");
const u8 D_2543B10[] = _("ぽちゃラブリっち");
const u8 D_2543B24[] = _("おやめろじっち\0");
const u8 D_2543B38[] = _("スリムねぇね");
const u8 D_2543B48[] = _("ふぇありぃぱくぱく\0");
const u8 D_2543B60[] = _("メロディ♪ミューズ\0");
const u8 D_2543B78[] = _("プチデビ♥ひめスペ\0");
const struct CharaText chara_text_names[] = {
    { 0, D_2543878 }, { 1, D_2543888 }, { 2, D_2543894 },
    { 3, D_25438A4 }, { 4, D_25438B4 }, { 5, D_25438C4 },
    { 6, D_25438D4 }, { 7, D_25438E8 }, { 0xa, D_25438F8 },
    { 0xb, D_2543908 }, { 0xc, D_2543918 }, { 0xd, D_254392C },
    { 0x10, D_254393C }, { 0x11, D_2543948 }, { 0x12, D_2543958 },
    { 0x13, D_254396C }, { 0x14, D_254397C }, { 0x15, D_254398C },
    { 0x3e9, D_254399C }, { 0x3ea, D_25439A8 }, { 0x3eb, D_25439B8 },
    { 0x3ec, D_25439C8 }, { 0x3ed, D_25439D8 }, { 0x3ee, D_25439E8 },
    { 0x7d1, D_25439FC }, { 0x7d2, D_2543A0C }, { 0x7d3, D_2543A20 },
    { 0x7d4, D_2543A30 }, { 0x7d5, D_2543A44 }, { 0x7d6, D_2543A58 },
    { 0x7d7, D_2543A6C }, { 0x7d8, D_2543A80 }, { 0x7d9, D_2543A90 },
    { 0x7da, D_2543AA4 }, { 0xbb9, D_2543AB8 }, { 0xbba, D_2543ACC },
    { 0xbbb, D_2543AE4 }, { 0xbbc, D_2543AF8 }, { 0xbbd, D_2543B10 },
    { 0xbbe, D_2543B24 }, { 0xbbf, D_2543B38 }, { 0xbc0, D_2543B48 },
    { 0xbc1, D_2543B60 }, { 0xbc2, D_2543B78 }, { 0xffff, D_254370C },
};
const u8 D_2543CF8[] = _("             えいえいおーでちゅ! ");
const u8 D_2543D2C[] = _("             GAOOOOOO!!! …がお? ");
const u8 D_2543D6C[] = _("             あそんで! あそんで! \0");
const u8 D_2543DA4[] = _("             は～い♥ ここゆら～♪ \0");
const u8 D_2543DDC[] = _("             ここはあったかいぽか～☼ ぽかぽかぽか～☼ \0");
const u8 D_2543E28[] = _("             ふよふよっとおでかけしたいなーーっ↑ ");
const u8 D_2543E6C[] = _("             ひとかわむけたらもうオトナ♪ ふたかわむけたらホントのオトナなも♪ \0");
const u8 D_2543ED0[] = _("             サクサク★カリカリ♪ ふわふわ★しっとり♪ てんぷら♪ ごはん♪ ギュギュっとよそっていこう～よ♪ \0");
const u8 D_2543F54[] = _("             なにかいいことないかなー? \0");
const u8 D_2543F90[] = _("             ん?なになに? …なんてこった!! \0");
const u8 D_2543FD4[] = _("             レンジでモコモコ～♪ オーブンでモッコモコ～♪ ゆめがふくらむモコモコモコ～♪ \0");
const u8 D_2544044[] = _("             おはなしするのだーいすきー♥ やだ～♥もー♥ カーワーイーイー♥ ここだけのトークしちゃおーね \0");
const u8 D_25440C4[] = _("             どうしました? ぼくでちからになれることでしたらおてつだいしますよ? ");
const u8 D_2544128[] = _("             たべてもへらないりょうり… どうしたらつくれるだっち～? ");
const u8 D_2544180[] = _("             せいふくけいかく? そ そんなおそろしいことかんがえたこともありませんよ! \0");
const u8 D_25441EC[] = _("             あきらめないことがたいせつなんだぜ! ");
const u8 D_2544230[] = _("             どうしたのー? はやくオトナになるアドバイスしてくれるの? \0");
const u8 D_254428C[] = _("             いつかながれぼしにのれるといーな ");
const u8 D_25442CC[] = _("             いま あたしのぐるぐるヘアーにみとれてたでしょう? \0");
const u8 D_2544320[] = _("             うちになにかはなし? そっかそっか だいじょうぶ いわなくてもぜんぶわかってるから \0");
const u8 D_2544394[] = _("             わたしはうたっているときがいちばんしあわせなの♥ ♪♪～～♪♪ \0");
const u8 D_25443F4[] = _("             ミーといっしょにパーフェクトなハーモニーをかなでようよ♪ ");
const u8 D_254444C[] = _("             えっ あたしのことはなしてた? なになに どんなはなしぃ♪ \0");
const u8 D_25444A8[] = _("             おかえりなさい♥ ごはんにする? おふろにする? それともきゃんでぃぱくぱくする? \0");
const u8 D_254451C[] = _("             すみません もうすこしでこのデータかいせきがおわりそうなんです! なにかありましたか? \0");
const u8 D_2544594[] = _("             ちょうしどーヨ? ");
const u8 D_25445C4[] = _("             いちにち6しょくゴハンをたべたいだっちん! \0");
const u8 D_2544610[] = _("             どうしただっちぇ? もしかしてさびしくなっただっちぇ? \0");
const u8 D_2544668[] = _("             おや? みつかってしまったナ  やはりオレはどこにいてもめだってしまうナ★ \0");
const u8 D_25446D4[] = _("             どうした? オマエもついにオレサマのけらいになるきになったのか? オマエならいつでもかんげいだぞ ");
const u8 D_2544754[] = _("             どうしたんだ? おれでよかったらはなしてみろよ! ");
const u8 D_25447A4[] = _("             フォォォォォォー! ラブリン フォォォォォォー! ");
const u8 D_25447F4[] = _("             すまないね きょうもこれからよていでうまっているんだ★ \0");
const u8 D_254484C[] = _("             きっと､ゆめはかないますよ★ ");
const u8 D_2544888[] = _("             さあ､ステキなファッションショーのはじまりよ@@@ \0");
const u8 D_25448DC[] = _("             あめちゃんがのこりすくなくなってきたから､ちかいうちにほじゅうしなくちゃね～@@@ \0");
const u8 D_2544950[] = _("             うちとまめっちのあいしょうをうらなってみようかな……♥ でもわるいけっかがでたらイヤだし……どうしようかな…… \0");
const u8 D_25449E0[] = _("             なにごともあたってくだけろのせいしんで､ファイヤーッ!!! \0");
const u8 D_2544A3C[] = _("             おにくをたべたら3ばいのりょうのおやさいをたべるようにしてるの～♪ \0");
const u8 D_2544AA0[] = _("             ミーがつけているあかいアクセサリーは イヤリングじゃないぞ! イヤリンゴだぞ★ガッハッハッ! ");
const u8 D_2544B1C[] = _("             どんなことでも､いっしょうけんめいやることがだいじなんだよ♥ ");
const u8 D_2544B78[] = _("             きゃんでぃ★びぃ～～～む! \0");
const u8 D_2544BB4[] = _("             ハイ♪コマンタレブー?ユーはきょうもハッピーかな? ミーはゲンキだよ!メルシーボクー♪ \0");
const u8 D_2544C2C[] = _("             あなたは～だんだん～ひめがスキにな～る@@スキにな～る@@  よ～し､スキになった♥イヒヒッ♥ \0");
const struct CharaText chara_text_talk[] = {
    { 0, D_2543CF8 }, { 1, D_2543D2C }, { 2, D_2543D6C },
    { 3, D_2543DA4 }, { 4, D_2543DDC }, { 5, D_2543E28 },
    { 6, D_2543E6C }, { 7, D_2543ED0 }, { 0xa, D_2543F54 },
    { 0xb, D_2543F90 }, { 0xc, D_2543FD4 }, { 0xd, D_2544044 },
    { 0x10, D_25440C4 }, { 0x11, D_2544128 }, { 0x12, D_2544180 },
    { 0x13, D_25441EC }, { 0x14, D_2544230 }, { 0x15, D_254428C },
    { 0x3e9, D_25442CC }, { 0x3ea, D_2544320 }, { 0x3eb, D_2544394 },
    { 0x3ec, D_25443F4 }, { 0x3ed, D_254444C }, { 0x3ee, D_25444A8 },
    { 0x7d1, D_254451C }, { 0x7d2, D_2544594 }, { 0x7d3, D_25445C4 },
    { 0x7d4, D_2544610 }, { 0x7d5, D_2544668 }, { 0x7d6, D_25446D4 },
    { 0x7d7, D_2544754 }, { 0x7d8, D_25447A4 }, { 0x7d9, D_25447F4 },
    { 0x7da, D_254484C }, { 0xbb9, D_2544888 }, { 0xbba, D_25448DC },
    { 0xbbb, D_2544950 }, { 0xbbc, D_25449E0 }, { 0xbbd, D_2544A3C },
    { 0xbbe, D_2544AA0 }, { 0xbbf, D_2544B1C }, { 0xbc0, D_2544B78 },
    { 0xbc1, D_2544BB4 }, { 0xbc2, D_2544C2C }, { 0xffff, D_254370C },
};
const u8 D_2544E14[] = _("             あそぼーでちゅ! ");
const u8 D_2544E44[] = _("             がおー! あばれちゃうぞーがお! ");
const u8 D_2544E84[] = _("             たのしいな～♥ \0");
const u8 D_2544EB4[] = _("             だっこして ゆ～らゆら♪ おんぶして ゆ～らゆら♪ \0");
const u8 D_2544F08[] = _("             ぽっかぽか♪ ぽっかぽか♪ぽーかぽーか ぽかぽか～♪ ");
const u8 D_2544F5C[] = _("             ふよふよ～～～～っ↑ ……どーしたふよっ? \0");
const u8 D_2544FA8[] = _("             なんも☼ なんも☼ なんとかなるなる～なも☼ ");
const u8 D_2544FF4[] = _("             なになに? ドーンとまかせるどん! \0");
const u8 D_2545038[] = _("             なにかななにかなー? あそんでくれるのかな? ");
const u8 D_2545084[] = _("             ぱんなこった～♪ ぱんなこった～♪ ぱんなこった～イエイ! \0");
const u8 D_25450E0[] = _("             デコレーションしてくれるぷっち? ふわもこなカンジでおねがいぷっち☼ ");
const u8 D_2545144[] = _("             あら～  ゲンキかしら～? いっしょにあそんじゃおーね♪ ");
const u8 D_254519C[] = _("             こまっていることでもあるんですか? なんでもいってくださいね! \0");
const u8 D_25451FC[] = _("             なんだっち? ゴハンのおさそいだっちか? ");
const u8 D_2545244[] = _("             あれ?いまよばれたきがしたのですが… はっ テレパシーでしょうか…?? \0");
const u8 D_25452AC[] = _("             どうした? れんしゅうならつきあうぜ! \0");
const u8 D_25452F4[] = _("             おれねーおれねー  おれのはなしもきいてー \0");
const u8 D_2545340[] = _("             そらをみてたんだー くもっていろんなかたちがあるねー ");
const u8 D_2545394[] = _("             なんだか､ぐるぐるヘアーのここのカールがうまくいってないようなきがするのよね…… あなたからみてどうかしら? ");
const u8 D_2545420[] = _("             なやみごとならうちとひめスペフォンにおまかせ!♥♥♥ ");
const u8 D_2545474[] = _("             こんどしゅつえんするばんぐみのだいほんチェックしてるの♪ とってもおもしろいから､いいばんぐみにしたいな♥ \0");
const u8 D_2545500[] = _("             ステキなメロディーがひらめいたの! ちょっとえんそうしてみるね♪ ");
const u8 D_2545560[] = _("             こいはたいあたり～♪ くろまめっちにアターック♥♥♥しちゃおっかなぁ♥ \0");
const u8 D_25455C8[] = _("             ぱく2はぱくぱくっていみだよ♪ ちなみに､ほんみょうは｢きゃんでぃすーぱーくいっくぱすあんどすまいるすぱーくっち｣なの☼ ");
const u8 D_2545660[] = _("             よそうどうりですね  いえ､ぼくのけいさんではそろそろあなたがはなしかけてくるころでしたから… \0");
const u8 D_25456E0[] = _("             オ オレにきやすくはなしかけるなよナ… あっ でもときどきならいいですよ……だゼ! \0");
const u8 D_2545754[] = _("             なんだかしょくごのゴロゴロしたいキブンだっちん… ");
const u8 D_25457A4[] = _("             きぐうだっちぇね  ちょうどボクもキミのことをかんがえていただっちぇ ");
const u8 D_2545808[] = _("             まてまて そんなにあわてるなって サインならかいてやるヨ★ \0");
const u8 D_2545864[] = _("             よんだか? まさかなんのようもなくオレサマをよんだわけではあるまいな? \0");
const u8 D_25458CC[] = _("             しんぱいするな! おれはいつでもいっしょだぜ★ \0");
const u8 D_254591C[] = _("             なんだね? おぬしもラブリンについてかたりあいたいのですかな? \0");
const u8 D_254597C[] = _("             キミによばれるなんてこうえいだね★ \0");
const u8 D_25459C0[] = _("             ながれぼしをみつけると…ついねがいごとを3かいとなえてしまいます★ \0");
const u8 D_2545A24[] = _("             アタシはつねにりゅうこうのさいせんたんをいくたまごっちなのよ～@@@ ");
const u8 D_2545A88[] = _("             あらあら､おばちゃんになにかごようかしら～?@@@ \0");
const u8 D_2545ADC[] = _("             いま､まめっちはなにしてるかな……♥ あいたいな……♥♥♥ \0");
const u8 D_2545B38[] = _("             ファイヤーッ!!! ファイヤーッ!!! \0");
const u8 D_2545B80[] = _("             たべあるきばんぐみのオファーとかこないかな～? \0");
const u8 D_2545BD0[] = _("             えっミーのようになりたいって? やっぱみためがたいせつだな★スタイリッシュにすたらいいっしゅ★…てか? ガッハッハッ! \0");
const u8 D_2545C68[] = _("             なんだかからだがかるいきがする……! いまならなんでもできちゃいそう↑↑↑ \0");
const u8 D_2545CD4[] = _("             みんながしあわせなら､あたしもしあわせだわぁ♥ \0");
const u8 D_2545D24[] = _("             ミーのバイオリンはね､とってもだいじなフレンドなんだ♪ いつもいっしょだよ♪グラシアス♪ ");
const u8 D_2545D9C[] = _("             ｢ンュキガギ・キスイダ・キスキス・チッメマ｣…このじゅもんのいみがわかるかしら～ん??イヒヒ♥ \0");
const struct CharaText chara_text_play[] = {
    { 0, D_2544E14 }, { 1, D_2544E44 }, { 2, D_2544E84 },
    { 3, D_2544EB4 }, { 4, D_2544F08 }, { 5, D_2544F5C },
    { 6, D_2544FA8 }, { 7, D_2544FF4 }, { 0xa, D_2545038 },
    { 0xb, D_2545084 }, { 0xc, D_25450E0 }, { 0xd, D_2545144 },
    { 0x10, D_254519C }, { 0x11, D_25451FC }, { 0x12, D_2545244 },
    { 0x13, D_25452AC }, { 0x14, D_25452F4 }, { 0x15, D_2545340 },
    { 0x3e9, D_2545394 }, { 0x3ea, D_2545420 }, { 0x3eb, D_2545474 },
    { 0x3ec, D_2545500 }, { 0x3ed, D_2545560 }, { 0x3ee, D_25455C8 },
    { 0x7d1, D_2545660 }, { 0x7d2, D_25456E0 }, { 0x7d3, D_2545754 },
    { 0x7d4, D_25457A4 }, { 0x7d5, D_2545808 }, { 0x7d6, D_2545864 },
    { 0x7d7, D_25458CC }, { 0x7d8, D_254591C }, { 0x7d9, D_254597C },
    { 0x7da, D_25459C0 }, { 0xbb9, D_2545A24 }, { 0xbba, D_2545A88 },
    { 0xbbb, D_2545ADC }, { 0xbbc, D_2545B38 }, { 0xbbd, D_2545B80 },
    { 0xbbe, D_2545BD0 }, { 0xbbf, D_2545C68 }, { 0xbc0, D_2545CD4 },
    { 0xbc1, D_2545D24 }, { 0xbc2, D_2545D9C }, { 0xffff, D_254370C },
};
const u8 D_2545F84[] = _("             ぼくとけっこんしてください! ");
const u8 D_2545FC0[] = _("             ぼくにまいにちみそしるをつくってほしいだっち♥ \0");
const u8 D_2546010[] = _("             あの…よ､よ､よろしければ､ボクとけっこんしていただけませんでしょうか♥ ");
const u8 D_2546078[] = _("             オレとけっこんしてくれないか? \0");
const u8 D_25460B8[] = _("             おれねっ､キミとならけっこんしてもいいぜ? \0");
const u8 D_2546104[] = _("             きみのこと ぼくがまもってあげるからね☼ ");
const u8 D_254614C[] = _("             あの…あたしとけっこんしてくれない? ");
const u8 D_2546190[] = _("             けっこんしてーー♥♥ギガキュン♥♥ \0");
const u8 D_25461D4[] = _("             えっと…わたしとけっこんしてくれますか?♥ \0");
const u8 D_2546220[] = _("             ダーリンにラブなんだ♥ ミーとけっこんしてくれないかな? ");
const u8 D_2546278[] = _("             ねぇねぇ､あたしあなたのことがすきなの  けっこんしてください!ねぇねキュン♥♥♥ \0");
const u8 D_25462EC[] = _("             けっこんしきではライスシャワーのかわりにきゃんでぃシャワーをふらせようよ～♥ ");
const u8 D_2546358[] = _("             あなたへのおもいのおんどが100どをこえました! ぼくとけっこんしてくれませんか? \0");
const u8 D_25463CC[] = _("             す､すきです…だゼ! ぼく､いや､オレとけっこんしてください…だゼ! ");
const u8 D_2546430[] = _("             きみのことがたべちゃいたいくらいすきだっちん! ぼくとけっこんしてほしいだっちん! \0");
const u8 D_25464A4[] = _("             ボクじゃダメだっちぇ? \0");
const u8 D_25464DC[] = _("             オレとけっこんしてくれないかナ? ぜったいにしあわせにしてみせるからサ★ ");
const u8 D_2546544[] = _("             オレサマのきさきにしてやるぞ! どうだ､こうえいだろ? \0");
const u8 D_254659C[] = _("             きみのしあわせは､おれとともにあるんだ  ……けっこんしよう★ \0");
const u8 D_25465FC[] = _("             オレのヨメキター! マジてんし!マジてんしでしょ! \0");
const u8 D_2546650[] = _("             おれとけっこんしてくれないかな? しきじょうとハネムーンのよやくはしておいたから､あとはキミのへんじだけさ★ ");
const u8 D_25466DC[] = _("             あなたとけっこんできたなら､うちゅういちしあわせにしてみせます★ ");
const u8 D_254673C[] = _("             おしゃれなアタシにぴったりのあいてねっ@@@ けっこんしましょっ@@@ \0");
const u8 D_25467A4[] = _("             アラッ! ぐるぐるのしたたるいいオトコ! けっこんしてー@@ ");
const u8 D_2546800[] = _("             まめキュン♥♥♥ もしかして､あなたがウチのうんめいのあいて……? \0");
const u8 D_2546864[] = _("             ひとめでスキになってしまいましたっ!! ステキなかていをつくりましょっ!! ファイヤー! ");
const u8 D_25468DC[] = _("             みんなからは～よく～ぽっちゃりしてるねっていわれるの～けっこんして～ ");
const u8 D_2546940[] = _("             ミーをパートナーにしてくれ! けっこんしたらおいわいだぞ♪ パーっとな～★ ガッハッハ! ");
const u8 D_25469B8[] = _("             けっこんしてくれないと↓ ねぇねないちゃうからっ╬ \0");
const u8 D_2546A0C[] = _("             あなたのこえ､あなたのひとみ…ひとめですきになっちゃったわぁ♥ あたしをおよめさんにしてぇ♥♥ \0");
const u8 D_2546A8C[] = _("             ジュテ～ム♥アイラブユ～♥ けっこんしようよ♥オ～ソレミ～オ♪ \0");
const u8 D_2546AEC[] = _("             ひめの～♥おねがいごと～♥ぜ～んぶきいてくれる～? じゃあけっこんしよ～♥イヒヒッ♥ ");
const struct CharaText chara_text_proposal[] = {
    { 0x10, D_2545F84 }, { 0x11, D_2545FC0 }, { 0x12, D_2546010 },
    { 0x13, D_2546078 }, { 0x14, D_25460B8 }, { 0x15, D_2546104 },
    { 0x3e9, D_254614C }, { 0x3ea, D_2546190 }, { 0x3eb, D_25461D4 },
    { 0x3ec, D_2546220 }, { 0x3ed, D_2546278 }, { 0x3ee, D_25462EC },
    { 0x7d1, D_2546358 }, { 0x7d2, D_25463CC }, { 0x7d3, D_2546430 },
    { 0x7d4, D_25464A4 }, { 0x7d5, D_25464DC }, { 0x7d6, D_2546544 },
    { 0x7d7, D_254659C }, { 0x7d8, D_25465FC }, { 0x7d9, D_2546650 },
    { 0x7da, D_25466DC }, { 0xbb9, D_254673C }, { 0xbba, D_25467A4 },
    { 0xbbb, D_2546800 }, { 0xbbc, D_2546864 }, { 0xbbd, D_25468DC },
    { 0xbbe, D_2546940 }, { 0xbbf, D_25469B8 }, { 0xbc0, D_2546A0C },
    { 0xbc1, D_2546A8C }, { 0xbc2, D_2546AEC }, { 0xffff, D_254370C },
};
const u8 D_2546C68[] = _("             これまでのことをおもうと､なごりおしいですね……でも､もういかないと! いままでおせわになりました､おげんきで! ");
const u8 D_2546CF8[] = _("             いままでいっしょにいてくれてありがとうだっち! ゴハンおいしかっただっち!! ");
const u8 D_2546D64[] = _("             いままでせわしてくれてありがとな! いごこちよかったぜ! げんきでな! \0");
const u8 D_2546DCC[] = _("             サヨナラはいわないぜ  またなっ★ \0");
const u8 D_2546E10[] = _("             おれね､ぜったいぜったいわすれないぜ! サンキュー★ ");
const u8 D_2546E64[] = _("             かぜのむくまま きのむくまま あしたはどんなかぜがふくのかな  かぜがむいたらまたあおうね! ");
const u8 D_2546EE0[] = _("             いままで､ほんとうにいろんなことがあったわよね  …ありがと～@ ");
const u8 D_2546F40[] = _("             あなたとすごしたさいこうのじかん､うち､わすれないよ! \0");
const u8 D_2546F98[] = _("             いままでありがとう……あなたとすごしたこと､わすれないから!! \0");
const u8 D_2546FF8[] = _("             ユーとのおもいではぜったいにわすれないから! ");
const u8 D_2547044[] = _("             いままでほんとうにありがとうねぇ  あたし､がんばるから! \0");
const u8 D_25470A0[] = _("             あしたのあけがた､ひとつのひかりがそらにとんでいく､それがあたしだよっ さよならっ! またきゃんでぃぱくぱくしようねっ \0");
const u8 D_2547138[] = _("             あなたとのおもいではこのパソコンにぜんぶきろくしてあります  さびしくなったらこれをみてがんばりますね! ");
const u8 D_25471C0[] = _("             ふりかえってみると､ここでのくらしもわるくなかったナ  えんがあったらまたあおうゼ! ");
const u8 D_2547234[] = _("             きみからもらったいろいろなたべもののこと､ぼくはぜんぶわすれないだっちん! \0");
const u8 D_25472A0[] = _("             はなれていても､ボクたちはおなじそらのしたにいるんだっちぇ  あばよだっちぇ★ \0");
const u8 D_2547310[] = _("             いままでありがとう､ここでのくらし､たのしかったゼ! それじゃあ､げんきでナ★ \0");
const u8 D_2547380[] = _("             オマエのことはわすれない! つぎのほしをせいふくしたら､そのほしをみやげにかえってくるぞ! \0");
const u8 D_25473FC[] = _("             きみとわかれるのはつらいけど､このわかれはふたりのせいちょうにひつようなことなんだ! またあおうな! ");
const u8 D_2547480[] = _("             いままでありがとう! おぬしといっしょにいられて､ギガしあわせだったぞ! ");
const u8 D_25474E8[] = _("             おれがいなくなってさびしくなったら､そらをみあげよう  キミとおれのみるそらはつながっているからさ★ ");
const u8 D_254756C[] = _("             あなたとのおもいでが､ぼくのむねのなかいっぱいにつまっています! あなたのうちゅうよりふかいあい､わすれません★ ");
const u8 D_25475FC[] = _("             あなたとすごしたじかん! ぜったいにわすれないからっ! あなたにあえてよかったっ@@@ \0");
const u8 D_2547674[] = _("             なくんじゃないの! えがおでサヨナラだよ! ……あんたがなくとこっちまでなけるじゃないの  からだにきをつけるんだよ@ \0");
const u8 D_254770C[] = _("             あなたとすごしたひびは､ゼッタイわすれない! まめっちとおんなじくらい､あなたのことがだいすきだったよ…♥♥♥ \0");
const u8 D_254779C[] = _("             なかないで! ゆめをかなえるまでにながしていいのはアセだけよっ! あ…めからアセが…バイバイっ!!! ");
const u8 D_2547820[] = _("             ちょっぴりさびしくなっちゃうね～  ちょっぴりね… でも､あいじょうはたっぷりもらったよ～♥ まったね～♥ \0");
const u8 D_25478AC[] = _("             またあえるかって? さあな! おもいでをサンクスだぜ! ミーからはシーユーアゲーン!なんにもあげーん!…だぜ★ ガッハッハ! ");
const u8 D_2547948[] = _("             いままでありがとう♥ あなたのことだいすきだったよ♥ ぜったいに､ぜったいにわすれないからねぇ! ");
const u8 D_25479C8[] = _("             さよならはいわないで! あなたにもらったしあわせが､めからこぼれてきちゃいそうだわぁ いままでありがとう★ \0");
const u8 D_2547A54[] = _("             メルシーボークー♪♪サンキューソーマッチ♪ほんとうにありがとう それじゃあアディオス♪またあおうね♪チャオ♪ ");
const u8 D_2547AE0[] = _("             ひめね～またきがむいたらもどってくるかもしれないわよ～♥ …な～んてね…じゃあね～♥イヒッ♥ ");
const struct CharaText chara_text_farewell[] = {
    { 0x10, D_2546C68 }, { 0x11, D_2546CF8 }, { 0x12, D_2546D64 },
    { 0x13, D_2546DCC }, { 0x14, D_2546E10 }, { 0x15, D_2546E64 },
    { 0x3e9, D_2546EE0 }, { 0x3ea, D_2546F40 }, { 0x3eb, D_2546F98 },
    { 0x3ec, D_2546FF8 }, { 0x3ed, D_2547044 }, { 0x3ee, D_25470A0 },
    { 0x7d1, D_2547138 }, { 0x7d2, D_25471C0 }, { 0x7d3, D_2547234 },
    { 0x7d4, D_25472A0 }, { 0x7d5, D_2547310 }, { 0x7d6, D_2547380 },
    { 0x7d7, D_25473FC }, { 0x7d8, D_2547480 }, { 0x7d9, D_25474E8 },
    { 0x7da, D_254756C }, { 0xbb9, D_25475FC }, { 0xbba, D_2547674 },
    { 0xbbb, D_254770C }, { 0xbbc, D_254779C }, { 0xbbd, D_2547820 },
    { 0xbbe, D_25478AC }, { 0xbbf, D_2547948 }, { 0xbc0, D_25479C8 },
    { 0xbc1, D_2547A54 }, { 0xbc2, D_2547AE0 }, { 0xffff, D_254370C },
};
const u8 D_2547C64[] = _("…? だーれ? ");
const u8 D_2547C78[] = _(" つよそーがお★ \0");
const u8 D_2547C90[] = _("だいすきーーっ ");
const u8 D_2547CA4[] = _(" だっこしてほしいゆら～♥ ");
const u8 D_2547CC4[] = _("にあえないと､ハートにぽっかりあながあいちゃうぽか～? ");
const u8 D_2547D00[] = _("ってどんなかんじふよっ? ボクもあってみたいふよっ↑ \0");
const u8 D_2547D3C[] = _("のことどんくらいすきなも? おしえてほしいなも♥ \0");
const u8 D_2547D74[] = _("どん? ふたりはもうカベドンしたどん? かみをカキアゲながらやるとカッコイイどん♥ ");
const u8 D_2547DCC[] = _("がすきなのかー  たしかにステキだよねー  しかくければもっといいのにね! ");
const u8 D_2547E1C[] = _("と あってみたいなー \0");
const u8 D_2547E38[] = _("のことをかんがえると､うれしいキモチがモコモコふくらむぷっち? ");
const u8 D_2547E7C[] = _("がスキなのね～♥ いいこときいちゃったわ～♥ じょうずにあまえちゃおーね♥ ");
const u8 D_2547ECC[] = _("さんですか  どんなかたなんでしょうか? きっとステキなんでしょうね↑ ");
const u8 D_2547F18[] = _("だっちか… ちなみに とくいなりょうりはなんだっち? \0");
const u8 D_2547F54[] = _("さんってりっぱなかたですよね  ボク､そんけいしてます! \0");
const u8 D_2547F94[] = _("のことみんなにはだまっといてやるよ! \0");
const u8 D_2547FC0[] = _("がすきなの? オトナだねー★ \0");
const u8 D_2547FE4[] = _("すきー? ぼくもおねーちゃんすきー♥ \0");
const u8 D_2548010[] = _("がスキなの? なるほど…そういうタイプなんだ～!いがい～! ");
const u8 D_2548050[] = _("っていいよねー♥♥♥ \0");
const u8 D_254806C[] = _("さんってとってもかんじがイイよね!♥ \0");
const u8 D_2548098[] = _("か～ ステキだよね!  ミーにはわかるよ♪ ");
const u8 D_25480C8[] = _("ってカッコイイねぇ♥ \0");
const u8 D_25480E4[] = _("にも､きゃんでぃぱくぱくしてもらいたいな～♪ \0");
const u8 D_2548118[] = _("ですか  ぼくのデータにはありませんね  すぐにけんさくしてみましょう ");
const u8 D_2548164[] = _("にはオレとおなじリーゼントがにあうとおもうゼ すすめてみろよナ! \0");
const u8 D_25481AC[] = _("とごはんをたべにいくときがあったら いっしょにつれてってほしいだっちん! \0");
const u8 D_25481FC[] = _("だっちぇか… ちなみにキミのめに ボクはどんなふうにうつってるだっちぇ? \0");
const u8 D_254824C[] = _("がすきだって? テレずにオレのなまえをかいてくれてもよかったのにサ… \0");
const u8 D_2548298[] = _("をオレサマのけらいにしたいから､こんどスカウトしておいてくれ╬ ");
const u8 D_25482DC[] = _("  そっか  そのあいてにきみのおもいがとどくといいな★ \0");
const u8 D_254831C[] = _("  オレはダントツでラブリンですなWW ");
const u8 D_2548348[] = _("のことをおもう､キミのこのきもち……まさしく｢あい｣だね♥ ");
const u8 D_2548388[] = _("? それがあなたの｢いちばんぼし｣なんですね★ ");
const u8 D_25483BC[] = _("にふさわしいコーディネイトをアタシがみたててあげてもいいわよっ@@ ");
const u8 D_2548404[] = _("がすきなんだって? だれにもいわないでおいてあげるわよ@ おばちゃんオウエンしちゃう@ ");
const u8 D_2548460[] = _("って､まめっちとおなじくらいステキだね♥♥♥ \0");
const u8 D_2548494[] = _("さんか…♥ すきってきもちだいじにしたいよねっ! たとえそれがイバラのみちでも! ぜったいあきらめないっ!! \0");
const u8 D_2548508[] = _("ってどっしりしてるほう～? ほっぺがプニプにしてるほう～? ");
const u8 D_2548548[] = _("にラブレターをかいてたら きんちょうしてジュースがこぼれた～★…ってか? ガッハッハ! ");
const u8 D_25485A4[] = _("にアタックするならおうえんしちゃうよ♥ アターック♥ \0");
const u8 D_25485E0[] = _("かぁ…しあわせおすそわけしてもらっちゃったわぁ♥ \0");
const u8 D_2548618[] = _("はだいすきなアミーゴ? それともラブだったりするの?!アモーレ♥ \0");
const u8 D_2548660[] = _("がスキなの～? うらやましい～ん♥イジワルしちゃおうかしら～ん♥デビキュン♥ \0");
const struct CharaText chara_text_crush[] = {
    { 0, D_2547C64 }, { 1, D_2547C78 }, { 2, D_2547C90 },
    { 3, D_2547CA4 }, { 4, D_2547CC4 }, { 5, D_2547D00 },
    { 6, D_2547D3C }, { 7, D_2547D74 }, { 0xa, D_2547DCC },
    { 0xb, D_2547E1C }, { 0xc, D_2547E38 }, { 0xd, D_2547E7C },
    { 0x10, D_2547ECC }, { 0x11, D_2547F18 }, { 0x12, D_2547F54 },
    { 0x13, D_2547F94 }, { 0x14, D_2547FC0 }, { 0x15, D_2547FE4 },
    { 0x3e9, D_2548010 }, { 0x3ea, D_2548050 }, { 0x3eb, D_254806C },
    { 0x3ec, D_2548098 }, { 0x3ed, D_25480C8 }, { 0x3ee, D_25480E4 },
    { 0x7d1, D_2548118 }, { 0x7d2, D_2548164 }, { 0x7d3, D_25481AC },
    { 0x7d4, D_25481FC }, { 0x7d5, D_254824C }, { 0x7d6, D_2548298 },
    { 0x7d7, D_25482DC }, { 0x7d8, D_254831C }, { 0x7d9, D_2548348 },
    { 0x7da, D_2548388 }, { 0xbb9, D_25483BC }, { 0xbba, D_2548404 },
    { 0xbbb, D_2548460 }, { 0xbbc, D_2548494 }, { 0xbbd, D_2548508 },
    { 0xbbe, D_2548548 }, { 0xbbf, D_25485A4 }, { 0xbc0, D_25485E0 },
    { 0xbc1, D_2548618 }, { 0xbc2, D_2548660 }, { 0xffff, D_254370C },
};
const u8 D_254881C[] = _("…たべてみたいでちゅ \0");
const u8 D_2548838[] = _("たべるとおっきくなれるがお? \0");
const u8 D_254885C[] = _("…? おいしいかな? \0");
const u8 D_2548878[] = _("ってあまいゆら? にがいゆら? ");
const u8 D_254889C[] = _("にショウガをのせてたべるとおいしいかもぽか☼ \0");
const u8 D_25488D0[] = _("おいしそうふよっ↑ ボクもいちどたべてみたいふよっ ");
const u8 D_2548908[] = _("がすきなんけ? おいらがオトナになったら､はらいっぱいくわしてやりてぇなも☼ \0");
const u8 D_254895C[] = _("もおいしいけど､どんぶりごはんもおいしいどん♪ ");
const u8 D_2548990[] = _("ってどんなあじー? アタシにもたべさせてほしいなー ");
const u8 D_25489C8[] = _("ってティラミスよりおいしいの? えーっ! パンナコッタよりも? なんてこった! ");
const u8 D_2548A1C[] = _("っておいしそうぷっち! クリームとフルーツでデコっちゃおーぷっち♪ ");
const u8 D_2548A64[] = _("～? あら～☼ オシャレだわ～☼ ゆっくりあじわっちゃお～ね♪ ");
const u8 D_2548AA8[] = _("がスキなんですね  こんどぼくにもたべさせてくださいね! \0");
const u8 D_2548AE8[] = _("がすきなんだっちか? ああ､おもいうかべていたらよだれがでてきただっち…☼ ");
const u8 D_2548B38[] = _("たべたことあるきがします! ええっと…あまいようなからいような? \0");
const u8 D_2548B80[] = _("? おれもきになってたんだ! \0");
const u8 D_2548BA4[] = _("がすきなの? もっとオトナなものをたべなきゃ! ");
const u8 D_2548BD8[] = _("のかたちのくもをみたんだー おいしそうだねー \0");
const u8 D_2548C0C[] = _("をさっきたべたんだけど……えっアレあなたのだったの? ごっめーん \0");
const u8 D_2548C54[] = _("がおいしいってきいたんだけど  …どんなの? \0");
const u8 D_2548C88[] = _("がすきなの? こんどたべてみたいな★ \0");
const u8 D_2548CB4[] = _("っておいしい? ミーもたべてみたいな♪ ");
const u8 D_2548CE0[] = _("がすきなの? いいねぇ! きゅうにたべたくなってきたよ! \0");
const u8 D_2548D20[] = _("にきゃんでぃをトッピングすると さらにおいしくなりそうだね～☼ ");
const u8 D_2548D64[] = _("のえいようかは……なるほどなるほど  わるくはないですが､たべすぎにはちゅういしてくださいね \0");
const u8 D_2548DC8[] = _("がすきなのか……これはおいしそうだゼ  こんどオレのぶんもよういしてくれよナ☼ ");
const u8 D_2548E1C[] = _("をおなかいっぱいたべられたらしあわせそうだっちん! よういしてほしいだっちん! ");
const u8 D_2548E70[] = _("にチーズいれるとうまそうだっちぇ! ためしにやってみたらいいだっちぇ! ");
const u8 D_2548EBC[] = _("はおいしいそうだナ! オレにもこんどたべさせてくれよナ★ \0");
const u8 D_2548EFC[] = _("がオレサマのしょくたくにでたことなんてないぞ╬ さてはこっそりたべてるんだな!╬ \0");
const u8 D_2548F54[] = _("はおいしいそうだな  おれのすきなものか? そのうちわかってくるんじゃないかな★ \0");
const u8 D_2548FAC[] = _("はラブリンもすきかな? さしいれしたらよろこぶかな? \0");
const u8 D_2548FE8[] = _("がすきだなんて､キミもまだまだこどもだね……カワイイね♥ \0");
const u8 D_2549028[] = _("がすきときいたのですが､こんどいっしょにたべにいきませんか?★ ");
const u8 D_254906C[] = _("のエキスをにつめてつくったドリンク､アタシものんでるわよっ@@ ");
const u8 D_25490B0[] = _("の ししょく ってどっかでやってないかしらね～@@ ");
const u8 D_25490E8[] = _("をまめっちといっしょにたべられたらステキだろうなあ……♥♥♥ \0");
const u8 D_254912C[] = _("っておいしそう!! わたしもじょうずにつくれるように､きょうからしゅぎょうしようかなっ! \0");
const u8 D_254918C[] = _("にマヨネーズとごまあぶらとオリーブオイルかけるとおいしそ～! 5にんまえくださ～い! \0");
const u8 D_25491E8[] = _("をつまみにおやじっちサイダーをグイッ! ミーにもごちそうしてくださいだー★…ってか? ガッハッハ! ");
const u8 D_2549250[] = _("のなるきがウチにあったらステキだねぇ♥ ");
const u8 D_254927C[] = _("を｢もぐもぐぱくぱくおいしくなぁれ!｣っておまじないをかけてたべると さらにおいしくなるわぁ☼ ");
const u8 D_25492E0[] = _("ってトレビアーン♪ボーノ♪こんどいっしょに たべてみたいなボナペティ♪ ");
const u8 D_254932C[] = _("スキだからって～たべすぎるとカラダにワルイわよ～ん♥ ま､ひめにはカンケイないけどね～ん♥イヒヒ♥ ");
const struct CharaText chara_text_want_eat[] = {
    { 0, D_254881C }, { 1, D_2548838 }, { 2, D_254885C },
    { 3, D_2548878 }, { 4, D_254889C }, { 5, D_25488D0 },
    { 6, D_2548908 }, { 7, D_254895C }, { 0xa, D_2548990 },
    { 0xb, D_25489C8 }, { 0xc, D_2548A1C }, { 0xd, D_2548A64 },
    { 0x10, D_2548AA8 }, { 0x11, D_2548AE8 }, { 0x12, D_2548B38 },
    { 0x13, D_2548B80 }, { 0x14, D_2548BA4 }, { 0x15, D_2548BD8 },
    { 0x3e9, D_2548C0C }, { 0x3ea, D_2548C54 }, { 0x3eb, D_2548C88 },
    { 0x3ec, D_2548CB4 }, { 0x3ed, D_2548CE0 }, { 0x3ee, D_2548D20 },
    { 0x7d1, D_2548D64 }, { 0x7d2, D_2548DC8 }, { 0x7d3, D_2548E1C },
    { 0x7d4, D_2548E70 }, { 0x7d5, D_2548EBC }, { 0x7d6, D_2548EFC },
    { 0x7d7, D_2548F54 }, { 0x7d8, D_2548FAC }, { 0x7d9, D_2548FE8 },
    { 0x7da, D_2549028 }, { 0xbb9, D_254906C }, { 0xbba, D_25490B0 },
    { 0xbbb, D_25490E8 }, { 0xbbc, D_254912C }, { 0xbbd, D_254918C },
    { 0xbbe, D_25491E8 }, { 0xbbf, D_2549250 }, { 0xbc0, D_254927C },
    { 0xbc1, D_25492E0 }, { 0xbc2, D_254932C }, { 0xffff, D_254370C },
};
const u8 D_25494FC[] = _("… なってみたいでちゅ! \0");
const u8 D_254951C[] = _("! かっこいーがおー! ");
const u8 D_2549538[] = _("…? すごいすごーい! ");
const u8 D_2549554[] = _("ってステキゆら がんばるゆら \0");
const u8 D_2549578[] = _("のユメをみたぽか～ あったかそうだったぽか～☼ ");
const u8 D_25495AC[] = _("ふよっ? がんばってふよっ↑ \0");
const u8 D_25495D0[] = _("け? ええなぁ! ええなぁ! オシャレなユメもってんなも☼ ");
const u8 D_2549610[] = _("どん? がんばってせかいにはみだす… いや､とびだすどん! ");
const u8 D_2549650[] = _("ってカッコイイよね～  しかくくって…… あれ?しかくくない?? \0");
const u8 D_2549698[] = _("ってゆめ かなうとおもうよ～♪ ");
const u8 D_25496BC[] = _("がゆめぷっち? かたにはまらないでがんばるぷっち♪ ");
const u8 D_25496F4[] = _("～? あら～☼ いいんじゃな～い? ステキよ～♥ ガンバってかなえちゃおーね★ ");
const u8 D_2549748[] = _("がゆめなんですね  おたがいゆめをかなえるためにがんばりましょう! ");
const u8 D_2549790[] = _("だっちか～ おいしいゴハンがたべられるようになるだっち? \0");
const u8 D_25497D0[] = _("ですか… ハッ!そのウラでこのほしのせいふくとかかんがえてませんよね? …あいや… なんでもありません… \0");
const u8 D_2549840[] = _("がゆめなのか! おうえんするぜ! \0");
const u8 D_2549868[] = _("かー  おれもねー､はやくそういうオトナになりたいんだよねー★ ");
const u8 D_25498AC[] = _("のゆめをのせたロケットがたまごっちせいからうちあげられます! 3!2!1!はっしゃー! ");
const u8 D_2549908[] = _("がゆめなんだー  あたしもがんばらなくっちゃ♪ ");
const u8 D_254993C[] = _("がゆめなんだ～  そっか…うち､おうえんしてるから! \0");
const u8 D_2549978[] = _("なのね! おたがいにゆめをかなえよう!オー!! ");
const u8 D_25499AC[] = _("か～ それってタイヘンなのかな? レッツトライ!! ");
const u8 D_25499E4[] = _("がゆめなの? れんあいもゆめもいっしょにがんばろうねぇ♪ \0");
const u8 D_2549A24[] = _("にむかってゴーゴゴーだよ★ ぱくぱくがんばってね! ");
const u8 D_2549A5C[] = _("がゆめなんですね! ぼくもぜんりょくでバックアップしますよ! \0");
const u8 D_2549AA0[] = _("だって? そんなの､おまえならすぐになれるとおもうゼ \0");
const u8 D_2549ADC[] = _("もいいけど､ぼくせんぞくのコックさんになってほしいだっちん! \0");
const u8 D_2549B20[] = _("…キミならそのゆめ かなえられるとおもうだっちぇ! ");
const u8 D_2549B58[] = _("  ちなみにオレのゆめは､たまごっちせいのせいふく……い､いや､たまごっちせいのおしゃれなせいふくをつくることなんだ! ");
const u8 D_2549BD4[] = _("がゆめなのか  オレサマのけらいになら､いますぐしてやれるぞ? ");
const u8 D_2549C18[] = _("か……うん､きみならなれるんじゃないかな  どりょくはうらぎらないぜ! ");
const u8 D_2549C64[] = _("  ラブリンもおなじゆめをかなえたら…かわいいだろうな♥♥♥ \0");
const u8 D_2549CA8[] = _("? じゃあおれもそれをめざそうかな  キミとならなんにでもなれそうだよ★ \0");
const u8 D_2549CF8[] = _("…そのゆめにむかってがんばれば､あなたならかなえられるとしんじています★ \0");
const u8 D_2549D48[] = _("のカリスマもつけてる このぐるぐるアクセサリー@ じょしりょくUPよっ@ \0");
const u8 D_2549D98[] = _("? でっかいゆめもってるわね～@@ おばちゃんもいまからめざそうかしら? なんちゃって@オホホ@ \0");
const u8 D_2549E00[] = _("か～ うちはまめっちのおヨメさんになりたいの♥♥♥ エヘヘ､いっちゃった♥♥♥ ");
const u8 D_2549E54[] = _("をめざしてるんだねっ! ゆめは ねてみるものじゃないのっ! いっしょにどりょくしよっ!! \0");
const u8 D_2549EB4[] = _("がゆめなの～? じっくり～しっかりがんばろ～ \0");
const u8 D_2549EE8[] = _("か～♪ トライしつづければいつかかなうぞ! チャンスと～らい★…ってか? ガッハッハ! ");
const u8 D_2549F44[] = _("  そのユメ…あきらめないで♥ ");
const u8 D_2549F68[] = _("のゆめ､ぱくぱくかなえなきゃんでぃ～…なぁ～んてね! \0");
const u8 D_2549FA4[] = _("のエスプリ､つまりそしつを､ユーからかんじるよ グッドラック♪きっとうまくいくよ♪オーケー♪ \0");
const u8 D_254A008[] = _("のさいのう♥あなたからビシビシかんじるわ～ん♥ でもゆだんしちゃダメよ!なーんてね♥イヒッ♥ \0");
const struct CharaText chara_text_want_become[] = {
    { 0, D_25494FC }, { 1, D_254951C }, { 2, D_2549538 },
    { 3, D_2549554 }, { 4, D_2549578 }, { 5, D_25495AC },
    { 6, D_25495D0 }, { 7, D_2549610 }, { 0xa, D_2549650 },
    { 0xb, D_2549698 }, { 0xc, D_25496BC }, { 0xd, D_25496F4 },
    { 0x10, D_2549748 }, { 0x11, D_2549790 }, { 0x12, D_25497D0 },
    { 0x13, D_2549840 }, { 0x14, D_2549868 }, { 0x15, D_25498AC },
    { 0x3e9, D_2549908 }, { 0x3ea, D_254993C }, { 0x3eb, D_2549978 },
    { 0x3ec, D_25499AC }, { 0x3ed, D_25499E4 }, { 0x3ee, D_2549A24 },
    { 0x7d1, D_2549A5C }, { 0x7d2, D_2549AA0 }, { 0x7d3, D_2549ADC },
    { 0x7d4, D_2549B20 }, { 0x7d5, D_2549B58 }, { 0x7d6, D_2549BD4 },
    { 0x7d7, D_2549C18 }, { 0x7d8, D_2549C64 }, { 0x7d9, D_2549CA8 },
    { 0x7da, D_2549CF8 }, { 0xbb9, D_2549D48 }, { 0xbba, D_2549D98 },
    { 0xbbb, D_2549E00 }, { 0xbbc, D_2549E54 }, { 0xbbd, D_2549EB4 },
    { 0xbbe, D_2549EE8 }, { 0xbbf, D_2549F44 }, { 0xbc0, D_2549F68 },
    { 0xbc1, D_2549FA4 }, { 0xbc2, D_254A008 }, { 0xffff, D_254370C },
};
const u8 D_254A1D4[] = _("たのしそうでちゅ… ");
const u8 D_254A1EC[] = _("やりたいがお! やりたいがお! ");
const u8 D_254A210[] = _("ってなーに? \0");
const u8 D_254A224[] = _("おしえて～ゆら♥ \0");
const u8 D_254A23C[] = _("ってポカポカ? それともポッカポカ? \0");
const u8 D_254A268[] = _("やってみたいふよっ↑ ふよふよ～っとしてたのしいにちがいないふよっ↑ \0");
const u8 D_254A2B4[] = _("か～☼ おいらもできっかな? やってみてぇなも☼ \0");
const u8 D_254A2EC[] = _("がんばるどん! もっとカラダにジュワ～っとしみこんでくるどん! ");
const u8 D_254A330[] = _("がとくいなんだって? アタシもがんばればできるかな? \0");
const u8 D_254A36C[] = _("してるときってどんなキブン?   ふーん  へぇー  あたしコドモだからわかんないや \0");
const u8 D_254A3C8[] = _("ってステキぷっち! プチブレイクのよかんぷっち! \0");
const u8 D_254A400[] = _("でちゅーもくどバツグンみたいな? もっとじょうずになっちゃおーね♪ ");
const u8 D_254A448[] = _("がすきだなんてステキですね! ぼくははつめいがスキなんですよ☼ ");
const u8 D_254A48C[] = _("ってなんだっち? おいしそうなひびきだっち…☼ ");
const u8 D_254A4C0[] = _("ですか  いいごしゅみをおもちなんですね \0");
const u8 D_254A4F0[] = _("か  おれもやってみようかな! ");
const u8 D_254A514[] = _("ってたのしい? おれもチャチャッとやってみようかな★ \0");
const u8 D_254A550[] = _("ってぼくにもできるかなー でっきるかな～ でっきるかな～♪ ");
const u8 D_254A590[] = _("やってるの? すっごいじゃなーい! こんどみせてね★ \0");
const u8 D_254A5CC[] = _("? なんだかたのしそうだねー! ");
const u8 D_254A5F0[] = _("か～ たのしいキモチになれそうだね♪ \0");
const u8 D_254A61C[] = _("っておもしろそうだね♪ ミーもやってみようかな!! ");
const u8 D_254A654[] = _("っていいよねぇ  もしかしたらびようにいいかもねぇ ");
const u8 D_254A68C[] = _("ならまかせろー!ってカオしてるよ! きゃんでぃぱくぱくする? \0");
const u8 D_254A6D0[] = _("ですか  これはデータのとりがいがありそうです…! ");
const u8 D_254A708[] = _("? それくらいオレにもできる……はずだゼ  ……たぶん ");
const u8 D_254A744[] = _("だなんてすごいだっちん! ぼくはゴロゴロしてるのがいちばんだっちん! \0");
const u8 D_254A790[] = _("だっちぇ? ボクと? わるいけど､ボクもとくいかもだっちぇ ");
const u8 D_254A7D0[] = _("? オレもやってみようかナ? こんどおしえてくれよナ★ ");
const u8 D_254A80C[] = _("とはな～  よし､オレサマのしたで､そのチカラをぞんぶんにふるうがいいぞ! ");
const u8 D_254A85C[] = _("か  おれもやってみたいから､こんどみせてほしいな☼ \0");
const u8 D_254A898[] = _("にむちゅうになってるラブリンのがぞうください \0");
const u8 D_254A8CC[] = _("? おれもわりとやれるほうだよ  こんどやってみようか? \0");
const u8 D_254A90C[] = _("はむずかしいときいたのですが… ステキですね★ ");
const u8 D_254A940[] = _("は1にち1じかん  びようとけんこうにいいらしいわよっ@ \0");
const u8 D_254A980[] = _("ってけんこうにいいっておひるのばんぐみでいってたわよ@ おばちゃんもチャレンジしようかしら@ \0");
const u8 D_254A9E4[] = _("をがんばるまめっち……きっとカッコいいんだろうなあ……♥♥♥ \0");
const u8 D_254AA28[] = _("でせかいめざそうよっ!!! さっそくトレーニングしよっ! 1・2・1・2!! \0");
const u8 D_254AA7C[] = _("にどっぷりはまってるんだって～  きみにぴったりだよ～ ");
const u8 D_254AAB8[] = _("…か～♪ そんなたのしそうなことなら レクチャーしてくれなくちゃ～★…ってか? ガッハッハ! \0");
const u8 D_254AB1C[] = _("がすきなの? スゴーイ♥ こんどおしえてねぇ♥ ");
const u8 D_254AB50[] = _("のことをおしゃべりするとイキイキしてるわぁ♪ さすがぁ♪ \0");
const u8 D_254AB90[] = _("ブラボー!がんばってマエストロになるべきだよ!フレッフレッ♪ファイトッ♪ オーーーレィッ♪ ");
const u8 D_254ABF0[] = _("のジマンバナシま～だつづくの～? べつにキライじゃないけど～♥イヒヒヒ♥ \0");
const struct CharaText chara_text_hobby[] = {
    { 0, D_254A1D4 }, { 1, D_254A1EC }, { 2, D_254A210 },
    { 3, D_254A224 }, { 4, D_254A23C }, { 5, D_254A268 },
    { 6, D_254A2B4 }, { 7, D_254A2EC }, { 0xa, D_254A330 },
    { 0xb, D_254A36C }, { 0xc, D_254A3C8 }, { 0xd, D_254A400 },
    { 0x10, D_254A448 }, { 0x11, D_254A48C }, { 0x12, D_254A4C0 },
    { 0x13, D_254A4F0 }, { 0x14, D_254A514 }, { 0x15, D_254A550 },
    { 0x3e9, D_254A590 }, { 0x3ea, D_254A5CC }, { 0x3eb, D_254A5F0 },
    { 0x3ec, D_254A61C }, { 0x3ed, D_254A654 }, { 0x3ee, D_254A68C },
    { 0x7d1, D_254A6D0 }, { 0x7d2, D_254A708 }, { 0x7d3, D_254A744 },
    { 0x7d4, D_254A790 }, { 0x7d5, D_254A7D0 }, { 0x7d6, D_254A80C },
    { 0x7d7, D_254A85C }, { 0x7d8, D_254A898 }, { 0x7d9, D_254A8CC },
    { 0x7da, D_254A90C }, { 0xbb9, D_254A940 }, { 0xbba, D_254A980 },
    { 0xbbb, D_254A9E4 }, { 0xbbc, D_254AA28 }, { 0xbbd, D_254AA7C },
    { 0xbbe, D_254AAB8 }, { 0xbbf, D_254AB1C }, { 0xbc0, D_254AB50 },
    { 0xbc1, D_254AB90 }, { 0xbc2, D_254ABF0 }, { 0xffff, D_254370C },
};
const u8 D_254ADA8[] = _("             こ \0");
const u8 D_254ADCC[] = _("             ゆめをかなえるために､せかいじゅうでいろいろなことをべんきょうしたいんです  たびにでることをおうえんしてくれますか? \0");
const u8 D_254AE64[] = _("             ぼくはたびにでて､もっとたくさんのたまごっちにおいしいゴハンをとどけたいんだっち☼ どうおもうだっち? \0");
const u8 D_254AEEC[] = _("             オレサマはおおきなゆめのためにたびだつぜ! だまってみおくってくれ! ");
const u8 D_254AF50[] = _("             おれはサッカーがすきだ! このウチからとびだして､サッカーでせかいにちょうせんしたいんだ! おうえんしてくれるかな? ");
const u8 D_254AFE4[] = _("             おれね､ホンモノのオトナになるためには､ぼうけんがひつようだとおもうんだ! たびにでてもいいかな? \0");
const u8 D_254B068[] = _("             じゆうにながれるくもをみてたらね､ぼくもひとりでたびがしてみたくなっちゃったの  たびにでてもいい? ");
const u8 D_254B0EC[] = _("             あたしはね､このぐるぐるヘアーのうつくしさをきわめて､せかいじゅうにぐるぐるのよさをひろめたいの! どう?おうえんしてくれない? ");
const u8 D_254B18C[] = _("             うち…じっかにかえってひめしゅぎょうをさいかいしようかとおもうんだ  どうかなー? \0");
const u8 D_254B200[] = _("             わたし､せかいじゅうにうたをとどけるアイドルになりたいの! どうおもう? ");
const u8 D_254B268[] = _("             おんがくのたのしさをひろめるために､たびにでようとおもうの! ここをでていっちゃってもOKかな? ");
const u8 D_254B2E8[] = _("             あたし…くろまめっちのハートをいとめるためにたびにでたいのよねぇ♥ ねぇねぇ! どうおもう? ");
const u8 D_254B364[] = _("             いままでひみつにしてたけど…あたしは…みらいからきたたまごっちなの!! えっ?しってた?? あはははは☼ ");
const u8 D_254B3EC[] = _("             あなたといっしょにけんきゅうしたせいかを､じっさいのうちゅうにでて､ためしてみたいんです! \0");
const u8 D_254B468[] = _("             せかいいちのやんちゃなおとこをきめるたいかいがあるようなんだ  だまってみおくってください…だゼ ");
const u8 D_254B4E8[] = _("             せかいをたびしてせかいじゅうのごちそうをたべつくしたいだっちん! おうえんしてほしいだっちん! \0");
const u8 D_254B568[] = _("             オトコをみがくために､ココをでていくだっちぇ  きがかりなのはキミのことだっちぇ  ボクとはなれてもさびしくないだっちぇ? \0");
const u8 D_254B604[] = _("             オレにはゆめがあるんだ  そのためには､ここをでていかなくちゃいけない……えがおでみおくってくれよナ? \0");
const u8 D_254B68C[] = _("             オレサマのやぼうはとどまることをしらない! いざ､つぎのほしをせいふくにむかうぞ! \0");
const u8 D_254B700[] = _("             いつまでもきみといっしょにいたいけど…じぶんさがしのたびにでてみようかとおもうんだ  きみはどうおもうかな? ");
const u8 D_254B78C[] = _("             やはりラブリンにあいたいキモチがおさえられぬ! いろいろとおせわになりましたな  ラブリンにあいに たびにでてもいいかな? \0");
const u8 D_254B828[] = _("             おれほどのおとなになると､せかいじゅうからおよびがかかっちゃうんだよね★ でていくこと､キミはおうえんしてくれるかな? \0");
const u8 D_254B8C0[] = _("             あなたとはなれるのはさみしいですが､うちゅうのはてを､みにいきたいんです…★ おうえん､してくれますか? ");
const u8 D_254B948[] = _("             アタシもそろそろひとりだちしなきゃいけないかな～ってかんがえていたところなのっ@ どうかしらっ@? \0");
const u8 D_254B9CC[] = _("             せかいじゅうのこどもたちにあめちゃんをくばるためにここをでていこうとおもってるんだけど? どうかねえ@? ");
const u8 D_254BA54[] = _("             そろそろまめっちといっしょにくらすしんきょをさがしにいこうとおもうの♥♥♥ どうかなあ? ");
const u8 D_254BACC[] = _("             もっともっとトレーニングがひつようだとおもうのっ! しゅぎょうにでますっ! しゅっぱつは…いまっ! \0");
const u8 D_254BB50[] = _("             ぽっちゃりアイドルをめざさないかってスカウトされたの～! どうかな～? いってもいいかな?? ");
const u8 D_254BBCC[] = _("             サンキューマイフレンド! つらくなるから ユーのきもちにはふれんずに ギャグしゅぎょうのたびにでるからな★ ガッハッハ! ");
const u8 D_254BC64[] = _("             あたらしいジブンをさがしにいこうとおもうの! おうえんしてくれるよねぇ? ");
const u8 D_254BCCC[] = _("             せかいのみんなに､きゃんでぃでしあわせのおすそわけをしにいきたいわぁ…いいかしらぁ? ");
const u8 D_254BD40[] = _("             グッバイたまごっちせい♪ミーはユニバーサルなバイオリニストをめざしてたびにでるよ♪ みおくってくれる? \0");
const u8 D_254BDC8[] = _("             ひめ…あなたにあまえすぎちゃった♥ もしこれいじょうここにいたら…ひめ…ダメになっちゃうかも…だから､さよなら… ");
const struct CharaText chara_text_journey[] = {
    { 0xb, D_254ADA8 }, { 0x10, D_254ADCC }, { 0x11, D_254AE64 },
    { 0x12, D_254AEEC }, { 0x13, D_254AF50 }, { 0x14, D_254AFE4 },
    { 0x15, D_254B068 }, { 0x3e9, D_254B0EC }, { 0x3ea, D_254B18C },
    { 0x3eb, D_254B200 }, { 0x3ec, D_254B268 }, { 0x3ed, D_254B2E8 },
    { 0x3ee, D_254B364 }, { 0x7d1, D_254B3EC }, { 0x7d2, D_254B468 },
    { 0x7d3, D_254B4E8 }, { 0x7d4, D_254B568 }, { 0x7d5, D_254B604 },
    { 0x7d6, D_254B68C }, { 0x7d7, D_254B700 }, { 0x7d8, D_254B78C },
    { 0x7d9, D_254B828 }, { 0x7da, D_254B8C0 }, { 0xbb9, D_254B948 },
    { 0xbba, D_254B9CC }, { 0xbbb, D_254BA54 }, { 0xbbc, D_254BACC },
    { 0xbbd, D_254BB50 }, { 0xbbe, D_254BBCC }, { 0xbbf, D_254BC64 },
    { 0xbc0, D_254BCCC }, { 0xbc1, D_254BD40 }, { 0xbc2, D_254BDC8 },
    { 0xffff, D_254370C },
};
const u8 D_254BF68[] = _("             しょうがないぽか☼ ゆるしてあげるぽか☼ こんごともヨロシクぽか☼ \0");
const u8 D_254BFCC[] = _("             も～っとかまってほしかったふよっ↓ みていてくれないとふよふよ～っととんでいっちゃうふよっ↓ ");
const u8 D_254C048[] = _("             えがったぁ! やっぱ､おいらのことわすれてなかったんだなも! ");
const u8 D_254C0A4[] = _("             ココロがさめちゃうどん↓ ほっかほかにあっためてほしいどん… ");
const u8 D_254C100[] = _("             ひどいよ､なんでアタシにつめたくするんだよー! アタシのハートはおとうふみたいにもろいんだからねー! ");
const u8 D_254C184[] = _("             さびしかったよー! かまってくれないと､コッチコチにひからびちゃうんだからっ! \0");
const u8 D_254C1F4[] = _("             ココロがしぼむと､カラダもしぼんじゃうぷっちぃ! あなたのあたたかさでふくらませてほしいぷっち♥ ");
const u8 D_254C274[] = _("             やだ～☼ も～☼ このままあえないかとおもっちゃったじゃな～い♥ こんやはいっしょにすごそーね☼ ");
const u8 D_254C2F4[] = _("             てっきりぼくのことがキライになったのかとおもいました……これからはもっとあそんでくださいね! ");
const u8 D_254C370[] = _("             しんぱいかけてゴメンだっち↓ もうおなかペコペコだっち☼ ");
const u8 D_254C3C8[] = _("             もうどこかのほしへいってしまおうかとおもいました…… ");
const u8 D_254C41C[] = _("             しんぱいかけたな…もうすこしいっしょにいようぜ! ");
const u8 D_254C46C[] = _("             おれねおれね､ひとりでずっとまってたんだ…しかたがないからかえってやるよ! \0");
const u8 D_254C4D8[] = _("             ぼくしってたよ  やまないあめはないって  それであめがやんだあとにはキレイなにじがでるってことも \0");
const u8 D_254C55C[] = _("             なによ…いまさらなかなおりしようなんて､おそいんだから!╬╬…わかったわよ@ ");
const u8 D_254C5C8[] = _("             うち…さみしくなっちゃって…ごめんなさい! なかなおりしよう! \0");
const u8 D_254C628[] = _("             ごめんなさい… わたし､もっとかまってほしかったみたい↓ ");
const u8 D_254C680[] = _("             ソーリー  なんだかじしんがなくなっちゃって…もっとかまってもらえるようにがんばるよ! \0");
const u8 D_254C6F8[] = _("             すねちゃってごめんねぇ…! なかなおり､ねぇ? もっとキレイになれるようにいっしょにがんばりたいな! ");
const u8 D_254C77C[] = _("             またあえてうれしーな! あれ? ちょっとせがのびたんじゃない? そんなわけないか! きゃんでぃぱくぱくぱくする? ");
const u8 D_254C80C[] = _("             ひきとめてくれるんですか  わかりました､なかなおりしましょう  データだけでなく､あいもひつようなんです! ");
const u8 D_254C898[] = _("             ほっといてくれよ､どうせオレなんか……↓ ……まあ､どうしてもっていうならゆるしてやってもいいゼ↑ \0");
const u8 D_254C91C[] = _("             おなかがすいて､うごくのがつらいだっちん! なかなおりのあかしに､はやくごはんにするだっちん! \0");
const u8 D_254C99C[] = _("             もうすこし､そばにいてやってもいいんだっちぇ ");
const u8 D_254C9E8[] = _("             こきょうにかえるつもりだったけど､そこまでいってくれるならかえらなくてもいいかナ★ \0");
const u8 D_254CA5C[] = _("             おそかったな╬ もうすこしでたびにでてしまうところだったぞ╬ ");
const u8 D_254CAB8[] = _("             まだ､きみにはおれのことがひつようなのか? ……わかった  もうすこしきみのせわになろうか \0");
const u8 D_254CB34[] = _("             べ､べつにおぬしのことをまってたわけじゃないんだからなっ! さ､ラブリンのしんきょくきこーWWW ");
const u8 D_254CBB4[] = _("             もうおわかれするつもりだったのに､なんでだろうね……きづいたらここでキミをまってしまっていたんだ ");
const u8 D_254CC34[] = _("             うちゅうにたったひとりでほうりだされたきぶんでした… あなたのあたたかいえがおを､またみられてしあわせです★ ");
const u8 D_254CCC0[] = _("             きづいてくれるってしんじてたっ@@@ これからもいっぱいおしゃれさせてねっ@@ \0");
const u8 D_254CD30[] = _("             こんなおばちゃんでも､アンタのウチにいていいのかい? ありがたいねぇ@ \0");
const u8 D_254CD98[] = _("             さみしかったから､まめっちのいえにおせわになろうかとおもってたところだけど…やっぱりあなたといっしょにいる! ");
const u8 D_254CE24[] = _("             あなたのキモチよーくわかった…もういちどゆめにむかってがんばりましょっ!!! さっそくランニングよっ!!! ファイヤー! ");
const u8 D_254CEBC[] = _("             さびしすぎてこんなにゲッソリやせちゃったじゃない～╬ ");
const u8 D_254CF10[] = _("             アイムソーリーだと? ミーはショックで ひげをソーリソリするところだったぞ! ガッハッハ! まあ おおめにみてやらあ! \0");
const u8 D_254CFA8[] = _("             ねぇねのこと､しっかりつかまえておかないと､とんでっちゃうぞ╬ \0");
const u8 D_254D008[] = _("             すっごくおこってるんだからぁ! これからは､もっとかまってほしいわぁ! \0");
const u8 D_254D070[] = _("             マンマミーア!!オーマイガー!!ミーのことキライになったのかと… もっとかまってほしいな♪シルブプレ♪ \0");
const u8 D_254D0F8[] = _("             ひめ､ずーーーーーっとひとりぼっちだったのよ～╬ ひめをほうっておいて､なにしてたのかしら? ");
const struct CharaText chara_text_forgive[] = {
    { 4, D_254BF68 }, { 5, D_254BFCC }, { 6, D_254C048 },
    { 7, D_254C0A4 }, { 0xa, D_254C100 }, { 0xb, D_254C184 },
    { 0xc, D_254C1F4 }, { 0xd, D_254C274 }, { 0x10, D_254C2F4 },
    { 0x11, D_254C370 }, { 0x12, D_254C3C8 }, { 0x13, D_254C41C },
    { 0x14, D_254C46C }, { 0x15, D_254C4D8 }, { 0x3e9, D_254C55C },
    { 0x3ea, D_254C5C8 }, { 0x3eb, D_254C628 }, { 0x3ec, D_254C680 },
    { 0x3ed, D_254C6F8 }, { 0x3ee, D_254C77C }, { 0x7d1, D_254C80C },
    { 0x7d2, D_254C898 }, { 0x7d3, D_254C91C }, { 0x7d4, D_254C99C },
    { 0x7d5, D_254C9E8 }, { 0x7d6, D_254CA5C }, { 0x7d7, D_254CAB8 },
    { 0x7d8, D_254CB34 }, { 0x7d9, D_254CBB4 }, { 0x7da, D_254CC34 },
    { 0xbb9, D_254CCC0 }, { 0xbba, D_254CD30 }, { 0xbbb, D_254CD98 },
    { 0xbbc, D_254CE24 }, { 0xbbd, D_254CEBC }, { 0xbbe, D_254CF10 },
    { 0xbbf, D_254CFA8 }, { 0xbc0, D_254D008 }, { 0xbc1, D_254D070 },
    { 0xbc2, D_254D0F8 }, { 0xffff, D_254370C },
};
const u8 D_254D2BC[] = _("             いっしょにあそびましょう! なにをしてあそぶかはおまかせします☼ ");
const u8 D_254D31C[] = _("             おもいっきりあそびたいだっち! きっとたのしいだっち☼ \0");
const u8 D_254D374[] = _("             すこしからだをうごかしたいです  なにかないですかね? \0");
const u8 D_254D3CC[] = _("             あんまりいえにいると からだがなまっちまうよな! ");
const u8 D_254D41C[] = _("             ひまひまひまー! なにかおもしろいことしようぜ! ");
const u8 D_254D46C[] = _("             モヤモヤする～@@ いっしょにおそとであそぼ ");
const u8 D_254D4B8[] = _("             なんかたいくつだわ……なにかシゲキテキなことはないのかしら? ");
const u8 D_254D514[] = _("             なんだかうち､とってもからだをうごかしたいきぶんなの!↑ ");
const u8 D_254D56C[] = _("             わたし､もっとみんなとあそびたいな～♪ \0");
const u8 D_254D5B4[] = _("             なんだかタイクツだな…いろんなことしてあそぼうよ♪ レッツプレイ～♪ ");
const u8 D_254D618[] = _("             からだをうごかしたいな～  あたらしいエクササイズとかないかしらねぇ ");
const u8 D_254D67C[] = _("             あそぼうよ! それともたまごっちせいのあしたについてぎろんする? …なーんてね☼ ");
const u8 D_254D6EC[] = _("             いえにこもってばかりなのもいけませんね  きぶんてんかんにフィールドワークにいきましょう! \0");
const u8 D_254D768[] = _("             バイクではしるのもたのしいけど…たまにはオマエのオススメのあそびもおしえてくれよだゼ! \0");
const u8 D_254D7E0[] = _("             あまりうごかなくてもたのしいこと!…がしたいだっちん! \0");
const u8 D_254D838[] = _("             どっかいこうだっちぇ ");
const u8 D_254D86C[] = _("             ヒマだし､どこかにあそびにいこうゼ! ");
const u8 D_254D8B0[] = _("             たいくつだな…╬ おい､なにかおもしろいものはないのか!╬ \0");
const u8 D_254D90C[] = _("             やあ､いっしょにあそびにいこうか? \0");
const u8 D_254D950[] = _("             あそびにいきたーいWWWできればラブリンといっしょにー! ");
const u8 D_254D9A8[] = _("             うーん､たいくつだね  なにかしてあそぼうか? \0");
const u8 D_254D9F8[] = _("             そらをながめるのもいいですが､たまにはあなたとあそびたいです★ \0");
const u8 D_254DA58[] = _("             タイクツだわっ@ アタシにふさわしいあそびってないかしらっ? ");
const u8 D_254DAB4[] = _("             ちょっとそとをぐるぐるしてフィーバーしたいわぁ@@ \0");
const u8 D_254DB08[] = _("             おでかけしよ! まめっちにあえたらいいな～♥♥♥ ");
const u8 D_254DB58[] = _("             トレーニングにたいせつなことっ! それはあそびごころっ! たのしんでやることっ! あそびもシンケンだよっ! ファイヤー! ");
const u8 D_254DBF0[] = _("             あそびたいけど～うごきたくない～ごろごろしたい～まったりしよ～ \0");
const u8 D_254DC50[] = _("             レッツプレイ! かくれんぼのオニはめをつぶれい★…ってか? ガッハッハ! ");
const u8 D_254DCB8[] = _("             ねぇねぇ! ヒマー?? あそぼうよ～っ@ ");
const u8 D_254DD00[] = _("             たのしいこと､いっしょにぱくぱくしたいわぁ♥ ");
const u8 D_254DD4C[] = _("             アロハ～♪ボンジョルノ♪ いっしょにあそぼうよアミーゴ♪ ");
const u8 D_254DDA4[] = _("             ジカンあるかしら～? あそんでちょ～だい♥ ひめの～オ・ネ・ガ・イ♥イヒヒ♥ ");
const struct CharaText chara_text_play_request[] = {
    { 0x10, D_254D2BC }, { 0x11, D_254D31C }, { 0x12, D_254D374 },
    { 0x13, D_254D3CC }, { 0x14, D_254D41C }, { 0x15, D_254D46C },
    { 0x3e9, D_254D4B8 }, { 0x3ea, D_254D514 }, { 0x3eb, D_254D56C },
    { 0x3ec, D_254D5B4 }, { 0x3ed, D_254D618 }, { 0x3ee, D_254D67C },
    { 0x7d1, D_254D6EC }, { 0x7d2, D_254D768 }, { 0x7d3, D_254D7E0 },
    { 0x7d4, D_254D838 }, { 0x7d5, D_254D86C }, { 0x7d6, D_254D8B0 },
    { 0x7d7, D_254D90C }, { 0x7d8, D_254D950 }, { 0x7d9, D_254D9A8 },
    { 0x7da, D_254D9F8 }, { 0xbb9, D_254DA58 }, { 0xbba, D_254DAB4 },
    { 0xbbb, D_254DB08 }, { 0xbbc, D_254DB58 }, { 0xbbd, D_254DBF0 },
    { 0xbbe, D_254DC50 }, { 0xbbf, D_254DCB8 }, { 0xbc0, D_254DD00 },
    { 0xbc1, D_254DD4C }, { 0xbc2, D_254DDA4 }, { 0xffff, D_254370C },
};
const u8 D_254DF18[] = _("             おなかがすきました! ごはんにしませんか? \0");
const u8 D_254DF64[] = _("             おなかすいただっち… もういっぽもあるけないだっち…@ \0");
const u8 D_254DFBC[] = _("             おなかがすきましたね  そろそろごはんのじかんでしょうか? \0");
const u8 D_254E018[] = _("             ハラがへったな! なにをたべよう! \0");
const u8 D_254E05C[] = _("             おれね､おなかがへったんだけど  ごはんまだー? ");
const u8 D_254E0AC[] = _("             おなかすいちゃった↓ ");
const u8 D_254E0E0[] = _("             おなかがすいたわ～@ もうペコペコ! ");
const u8 D_254E124[] = _("             ねえ､ゴハンたべにいかない? おなかすいちゃって★ \0");
const u8 D_254E178[] = _("             わたし､おなかすいちゃった★ ");
const u8 D_254E1B4[] = _("             おなかすいちゃったよ  カフェレストランにいきたいな♪ \0");
const u8 D_254E20C[] = _("             おなかがへっちゃった! …がまんはよくないよねぇ? ねぇ? \0");
const u8 D_254E268[] = _("             おなかすいたな～ レストランでぱくぱくしたいな～ ");
const u8 D_254E2B8[] = _("             けんきゅうにむちゅうでしょくじをわすれていました  なにかたべるものはありませんか? ");
const u8 D_254E32C[] = _("             おなかがすいたゼ  お～い､レストランでもいこうゼ! ");
const u8 D_254E380[] = _("             おなかがすいただっちん! まだまだたべたりないだっちん! ");
const u8 D_254E3D8[] = _("             なんか､おなかへったかもだっちぇ ");
const u8 D_254E418[] = _("             ごはんのよういをしてくれよナ★ \0");
const u8 D_254E458[] = _("             はらがへったぞ! いますぐ､さいこうきゅうのしょくじをよういしろ!╬ ");
const u8 D_254E4BC[] = _("             レストランでしょくじでもどうかな? \0");
const u8 D_254E500[] = _("             ごはんカモーンWWW ");
const u8 D_254E534[] = _("             そろそろしょくじのじかんじゃないかな? \0");
const u8 D_254E57C[] = _("             おなかがすきすぎて､あたまのまわりを｢★｣がとんでいます…★…★…★ ");
const u8 D_254E5E0[] = _("             おしょくじのじかんよっ@ レストランよやくしといたわっ@@@ ");
const u8 D_254E63C[] = _("             なんかぐるぐるしたもんがたべたいわぁ@ \0");
const u8 D_254E684[] = _("             ご・は・ん♥♥♥ ");
const u8 D_254E6B4[] = _("             きそたいりょくをつけるにはっ たべることがじゅうようっ!! おなかへったなァァァ!! ファイヤー! \0");
const u8 D_254E738[] = _("             ごは～ん～こってり～どんぶり～おにく～ \0");
const u8 D_254E780[] = _("             ハラがへったぞハングリー╬ おまけにくちもあんぐり～★…ってか? ガッハッハ! \0");
const u8 D_254E7F0[] = _("             1 2 3 4 ごはーん♪ \0");
const u8 D_254E82C[] = _("             おなかいっぱい､きゃんでぃフルコースがたべたいわぁ! ");
const u8 D_254E880[] = _("             トレビアーン♪ボーノ♪おいしいものがたべたいな♪ カフェにつれてってプリーズ♪ \0");
const u8 D_254E8F0[] = _("             ひめね～おなかすいちゃった～♥ おいし～ものがたべた～い♥ イヒヒッ♥ \0");
const struct CharaText chara_text_hungry[] = {
    { 0x10, D_254DF18 }, { 0x11, D_254DF64 }, { 0x12, D_254DFBC },
    { 0x13, D_254E018 }, { 0x14, D_254E05C }, { 0x15, D_254E0AC },
    { 0x3e9, D_254E0E0 }, { 0x3ea, D_254E124 }, { 0x3eb, D_254E178 },
    { 0x3ec, D_254E1B4 }, { 0x3ed, D_254E20C }, { 0x3ee, D_254E268 },
    { 0x7d1, D_254E2B8 }, { 0x7d2, D_254E32C }, { 0x7d3, D_254E380 },
    { 0x7d4, D_254E3D8 }, { 0x7d5, D_254E418 }, { 0x7d6, D_254E458 },
    { 0x7d7, D_254E4BC }, { 0x7d8, D_254E500 }, { 0x7d9, D_254E534 },
    { 0x7da, D_254E57C }, { 0xbb9, D_254E5E0 }, { 0xbba, D_254E63C },
    { 0xbbb, D_254E684 }, { 0xbbc, D_254E6B4 }, { 0xbbd, D_254E738 },
    { 0xbbe, D_254E780 }, { 0xbbf, D_254E7F0 }, { 0xbc0, D_254E82C },
    { 0xbc1, D_254E880 }, { 0xbc2, D_254E8F0 }, { 0xffff, D_254370C },
};
const u8 D_254EA60[] = _("             ぼくにピッタリなファッションてどんなのでしょう? よかったらみたててほしいです! ");
const u8 D_254EAD0[] = _("             ぼくもおしゃれしたいだっち  かっこよくキメてほしいだっち☼ ");
const u8 D_254EB2C[] = _("             いろんなふくをきてみたいです  か､かっこよくなれるでしょうか… ");
const u8 D_254EB8C[] = _("             あたらしいふくを きてみたいぜ! ");
const u8 D_254EBCC[] = _("             おれね､おなじふくはあきちゃったんだよね  こうビシッとしたやつをおねがい★ ");
const u8 D_254EC38[] = _("             おほしさまみたいにキラキラになりたいなー ");
const u8 D_254EC80[] = _("             ああ､このぐるぐるヘアーがきわだつようなオシャレがしたいわ@ ");
const u8 D_254ECDC[] = _("             かわいくオシャレしたいな♥ おんなのこだもん↑ \0");
const u8 D_254ED2C[] = _("             いろんなふくをきてオシャレをけんきゅうしたいな! ");
const u8 D_254ED7C[] = _("             いろんなファッションをためしてみない? きっとたのしいよ! \0");
const u8 D_254EDD8[] = _("             バッチリおしゃれして､カレのハートをゲットしたいんだよねぇ \0");
const u8 D_254EE34[] = _("             オシャレしてきゃんでぃぱくぱくしたいな～ ぱっくぱくにしてあげる～♪ ");
const u8 D_254EE98[] = _("             なにごともじぜんのちょうさがたいせつですよ  データはうそをつきませんからね ");
const u8 D_254EF04[] = _("             げんかいまでとばしていくゼ! ");
const u8 D_254EF40[] = _("             からだがおもいだっちん… ダイエット……なにそれ??おいしいだっちん? \0");
const u8 D_254EFA8[] = _("             ちょ…まてよだっちぇ ");
const u8 D_254EFDC[] = _("             カッコよくなったオレをみてくれよナ★ ");
const u8 D_254F020[] = _("             オレサマこそ､なんでもできちゃうすごいやつ､キングスペイシーさまだ!╬ \0");
const u8 D_254F088[] = _("             おれにあこがれてくれているこのためにも､なにごともしっかりしないとな♪ \0");
const u8 D_254F0F0[] = _("             ラブリンのしんきょくのふりつけれんしゅうしなくちゃWWWW \0");
const u8 D_254F14C[] = _("             くろまめセンパイ､いまごろなにしてるのかな……♥ ");
const u8 D_254F19C[] = _("             そらをじゆうに､とべるかな?★ \0");
const u8 D_254F1DC[] = _("             パーティでもひらきたいキブンねっ@ \0");
const u8 D_254F220[] = _("             きょうのぐるぐるパーマ@ イカしてるわぁ@@ ");
const u8 D_254F26C[] = _("             まめっち､うちのことどうおもってるのかな……♥♥♥ \0");
const u8 D_254F2C0[] = _("             わたしはあきらめないっ!!! やればできるっ!! ファイヤー! \0");
const u8 D_254F320[] = _("             ちょっぴりぽっちゃりしてるほうがけんこうてきよね～ \0");
const u8 D_254F374[] = _("             スマイルがたりないぞ! ミーがわらえるネタをもってまいる★…ってか? ガッハッハ! \0");
const u8 D_254F3E8[] = _("             ねぇねキュン♥♥♥ \0");
const u8 D_254F41C[] = _("             きゃんでぃにしあわせいっぱいつめこんでぇ♪ ぱくぱくふりまくのぉ～♪ ");
const u8 D_254F480[] = _("             ハロー♪ボンジュール♪ おでかけしようよ♪レッツゴー! \0");
const u8 D_254F4D8[] = _("             ひめね～ちょっぴりゴキゲンナナメなカンジよ～♥ ねーねー なんでかしらね～? ");
const struct CharaText chara_text_fashion[] = {
    { 0x10, D_254EA60 }, { 0x11, D_254EAD0 }, { 0x12, D_254EB2C },
    { 0x13, D_254EB8C }, { 0x14, D_254EBCC }, { 0x15, D_254EC38 },
    { 0x3e9, D_254EC80 }, { 0x3ea, D_254ECDC }, { 0x3eb, D_254ED2C },
    { 0x3ec, D_254ED7C }, { 0x3ed, D_254EDD8 }, { 0x3ee, D_254EE34 },
    { 0x7d1, D_254EE98 }, { 0x7d2, D_254EF04 }, { 0x7d3, D_254EF40 },
    { 0x7d4, D_254EFA8 }, { 0x7d5, D_254EFDC }, { 0x7d6, D_254F020 },
    { 0x7d7, D_254F088 }, { 0x7d8, D_254F0F0 }, { 0x7d9, D_254F14C },
    { 0x7da, D_254F19C }, { 0xbb9, D_254F1DC }, { 0xbba, D_254F220 },
    { 0xbbb, D_254F26C }, { 0xbbc, D_254F2C0 }, { 0xbbd, D_254F320 },
    { 0xbbe, D_254F374 }, { 0xbbf, D_254F3E8 }, { 0xbc0, D_254F41C },
    { 0xbc1, D_254F480 }, { 0xbc2, D_254F4D8 }, { 0xffff, D_254370C },
};
const u8 D_254F64C[] = _("             あらおはよう! けさのマルシェはいいものがそろってるみたいね! \0");
const u8 D_254F6AC[] = _("             おはようございます! わたしのオークションにふさわしいしなものはありませんか!? ほしいくつかな? \0");
const u8 D_254F730[] = _("             みんなでオススメのしなものをもちよっているのよ♪ あなたのオススメはなにかしら? ");
const u8 D_254F7A0[] = _("             ひとりでおそうじしたりおふろにはいったり…こどものうちにおぼえないとだぴょん! けいさんもおぼえてほしいぴょん★ ");
const u8 D_254F830[] = _("             こそだてちょうってしってるぴょん? こそだてのコツやヒントがかいてあるんだぴょん! \0");
const u8 D_254F8A4[] = _("             いいところであったぴょん★ ドリームカプセルってしってるぴょん? みらいをゆききできるらしいぴょん! すごいアイテムぴょん!! \0");
const u8 D_254F944[] = _("             ぶかつどうには､はいってるぴょん? けいさんぶとかないかなぴょん～@ けいさんでこたえをだすのはたのしいぴょん♪ ");
const u8 D_254F9D4[] = _("             おれっちとカフェでおちゃしなーい? チャラッとすてきなひとときをすごそうよ～♥ \0");
const u8 D_254FA44[] = _("             やっぱ､こせいてきなたまごっちはふだんのこうどうもこせいてきっしょ? いろんなはんのうをみてみるのをオススメしちゃうよ?↑ \0");
const u8 D_254FAE0[] = _("             カフェでチャラメルマキアートをオーダーしたらことわられちゃったよ～  ハーブティもイケてるっしょ★ \0");
const u8 D_254FB64[] = _("             じょせいをほめるにはドコをほめるか? ぜんぶをほめる! じょせいにはほめるトコロしかないのさ～♥♥ \0");
const u8 D_254FBE8[] = _("             ゼロのつくひはゴミゼロのひ  エコうさっちたちといっしょにゴミひろいをがんばろうぜ! まちをキレイにシティクリ～ン★…ってか! ガッハッハ! ");
const u8 D_254FC94[] = _("             ヤセるなら､うんどうするのがオススメだぜ! うんどうじょうであそんでいいかって? うん!どうじょー!★ ガッハッハ! ");
const u8 D_254FD28[] = _("             しかるのはほめるよりむずかしいんだぜ! ほんとうにあいてのことがだいじなら､しかることもひつようかもな  コラー!コーラのみすぎんな★…ってな! ガッハッハ! \0");
const u8 D_254FDE8[] = _("             わかれがつらいってことは､カレーがかれえ…ってのとおなじくらい､みんなわかってる  ｢つらい｣なんてコトバにださず､わらってみおくってやったほうがいいかもしれねえなあ～ ");
const u8 D_254FEB0[] = _("             たまともとハイタッチするみたいに､いろいろなところにタッチしていたら､なんだかたのしくなってきたよ! そっちにもこっちにも～↑↑↑ \0");
const u8 D_254FF54[] = _("             どこかにたまごっちのおせわのしかたについてかいてあるてちょうがあるらしいよ  そのてちょうのとおりにおせわしたらどんなたまごっちにそだつんだろうね? ");
const u8 D_2550008[] = _("             これからぶかつでサッカーのれんしゅうなんだ! おどうぐがあればひとりでもあそべるんだけど､やっぱりせんせいにおしえてもらわないとうまくなったきがしないんだよね… ");
const u8 D_25500C8[] = _("             ごっちポイントはせいかつをゆたかに… たまともやたまペットとのつながりはこころをゆたかにしてくれる☼ どちらもたいせつなものだよね♪ ");
const u8 D_255016C[] = _("             うちはどくしょがすきなんだけど､かんどうするおはなしとか､かなしいおはなしをよむと､すぐになみだがでてきちゃうから､なかなかよみおわらないんだよねー ");
const u8 D_2550220[] = _("             がっこうでぶかつはしてる? ぶかつはいいよー  ともだちといっしょにかつどうすることで､みにつくものがきっとあるはずだよ↑↑↑ \0");
const u8 D_25502C0[] = _("             あさ､このこうえんにくると､あさマルシェがひらかれていて､みんながいろいろなアイテムをもちよってくるんだよー☼ みるだけでもたのしいから､あなたもこんどきてみたら? ");
const u8 D_2550384[] = _("             うちのしゅみはたべあるきなんだー  でも､すぐにおなかがいっぱいになっちゃうから､すこししかたべられなくてざんねんなんだー \0");
const u8 D_2550420[] = _("             おかしづくりのべんきょうのためにいろいろなおかしをたべるんだけど､それでおなかがいっぱいになっちゃって､ごはんがたべられなくなっちゃうときがあるのよね…… \0");
const u8 D_25504DC[] = _("             おうちにたのみごとをしにくるたまごっちがいるらしいわ  いつきてくれてもだいじょうぶなように､がっこうでわざにみがきをかけておこうっと♪ ");
const u8 D_2550584[] = _("             このまえケーキをつくったときに､ためしにちょっとかわったかくしあじをいれてみたの  みんなにおいしいっていってもらえてよかったわ♪ \0");
const u8 D_2550628[] = _("             ケーキのざいりょうをかいにいくとちゅうなの♪ あれ?わたし…いえのカギをかけてきたかしら? いちどかえってかくにんしてこようっと ");
const u8 D_25506C8[] = _("             ひるからゆうがたにかけて､やたいをひいてるんだ! おでんとおやじっちサイダーがオススメだぜ～☼ きてくれなくちゃイヤたい★…ってか? ガッハッハ! \0");
const u8 D_255077C[] = _("             メロディっちのえんそうで｢おやじのはなみち｣うたいてぇーなー♪ おれならメロディっちのかのうせいをひろげられるんじゃないかのう★…ってか? ガッハッハ! ");
const u8 D_2550834[] = _("             いやなことがあったらおやじっちサイダーをのみな! スカっとわすれちまうぜ! こんなサイダーをつくれたおれはてんさいだー★ ガッハッハ! \0");
const u8 D_25508DC[] = _("             おれのおでんはうまいぞ! コトコトにこんだわふうダシはひとあじちがう! たまごっちせい1のおでんで､おでんむかえするぜ★ ガッハッハ! \0");
const u8 D_2550984[] = _("             こうえんであそんだあとは､おふろにはいるもな  めんどくさがらずにまいにちおふろにはいると､からだもホカホカいいぐあいもな～♪ \0");
const u8 D_2550A24[] = _("             カードをピピッとかざすのって､ビジネスマンってかんじがしてカッコイイもな! ぼくもまちなかでピッピッピッピやりたいもな～ ");
const u8 D_2550ABC[] = _("             ｢みのるほどこうべをたれるたまごっちかな｣ …なかみのぎっしりつまってるたまごっちほど､あいてにたいしてやさしくなるものだもな… ");
const u8 D_2550B5C[] = _("             あんこのざいりょうは､あずきというまめなんだもな  きなこのざいりょうは､だいずというまめなんだもな  しょうゆやみそもまめでつくられているんだもな  まめははたらきものだもな～ \0");
const u8 D_2550C30[] = _("             まめっちです  きょうはこうえんに､はつめいひんのテストをしにきました  2かいばくはつしてしまいましたが､3どめのしょうじきです  こんどこそきっとせいこうします★ \0");
const u8 D_2550CF8[] = _("             ごはんをいれるだけでおにぎりをつくってくれるきかいをはつめいしました! これがそのきかいでつくったおにぎりです! あなたにもひとつおすそわけしますね☼ \0");
const u8 D_2550DB0[] = _("             くちぱっちだっち  きょうもこうえんで げんきにあそぶだっち  …それにしても､おなかがへっただっち  きょうはなにをたべようだっち? \0");
const u8 D_2550E58[] = _("             さっきぱっちのもりのしんめいぶつ・ぱっちドーナツをたべにいっただっち☼ おみやげもたくさんもってきたから､ひとつおすそわけだっち☼ \0");
const u8 D_2550EFC[] = _("             きょうこそはたまごっちせいのせいふくけいかくを… あ､ちがうんです､せいふ…そう!｢セーフマハル｣っていうカレーやさんがこんどできるらしいんで､アカやピポとたべにいくけいかくをたてていたんです! \0");
const u8 D_2550FE0[] = _("             あたまをあらうときにおゆやシャンプーがめにはいってしまうことってありますよね…… でも､このぼうしさえあればそんなしんぱいはありません↑ よろしければ､あなたもつかってみてください☼ \0");
const u8 D_25510B8[] = _("             おれねおれね､サッカーするのもスキだけど､おうえんするのもスキなんだ  オーレオーレってね  キミはなにオーレ? カフェオーレ? まっちゃオーレ? \0");
const u8 D_255116C[] = _("             おれねおれね､このまえテレビでみたロックバンドがすごくかっこよかったからギターはじめたんだ! でもやっぱり､ボーカルがやりたいからこれあげるよ! これでおれとバンドをくもうよ★ ");
const u8 D_255123C[] = _("             ボールをとばしすぎてこっちのほうまできちまったぜ… サッカーボールみなかったかな? \0");
const u8 D_25512B0[] = _("             ラブリンがすきなんだって? ちょうどさっきこれをもらったんだ  キミにもひとつあげるぜ! ");
const u8 D_2551328[] = _("             わたしはやっぱりうたうのがすき♪ あなたもいっしょにうたおうよ! …え?テルリン? テルリンならおうちでおひるねちゅうだよ@ ");
const u8 D_25513C4[] = _("             ライブようのインカムをしんちょうしたの♪ まえのもおきにいりだからどうしようかこまってるんだけど… あ､そうだ! これ､おふるだけどよかったらつかってね♪ \0");
const u8 D_2551480[] = _("             きょうのうらないによるとラッキースポットはこうえんだったの! うらないあたったみたい★ だってアナタにあえたもの♪ \0");
const u8 D_2551514[] = _("             すきなたまごっちとけっこんすることそうぞうしてたらいつのまにかつくっちゃった♥ でもじっさいにきてみるのってドキドキするよねっ♥ そうだ､よかったらあなたきてみせてくれないっ? ");
const u8 D_25515E4[] = _("             ねぇねっちうたいます! こいせよおとめ～♪ こいのかおりはなんのかおり～♪ あまいあまいハニーのかおり～♪ ハニートーストこんがり～♪ ランラン♪ \0");
const u8 D_2551698[] = _("             まいにちじぶんにむきあうことが､キレイへのだいいっぽなの! ねぇ､あなたもいっしょにキレイをめざしましょう♪ ");
const u8 D_2551724[] = _("             きょうの｢めめさんぽ｣はこうえんをぐるぐる@@しながらぐるぐる@@したものをさがすのがテーマよ! なんかぐるぐる@@したものなーい? ");
const u8 D_25517C8[] = _("             つらいとき､かなしいとき､あまいものがほしいとき… そんなときはこれをたべてげんきをだすの@ あまいものはいつもおんなのこのみかたよね@@ \0");
const u8 D_2551874[] = _("             ハロー! こんどミーのニューアルバムがリリースされるよ♪ きいてもらえるとうれしいな★ \0");
const u8 D_25518EC[] = _("             んん～すっぱい★ でもこのしげきてきなあじ  とってもヤミ～♪ おもわずチョーうめぇっていいそうになるからユーものんでみなよ♪  ");
const u8 D_255198C[] = _("             こ､こんにちは…! ここここれはその…まめっちのことをかんがえていたら､きづいたらうち… ");
const u8 D_2551A04[] = _("             (これはゆめか､げんじつか♥♥♥ うちのイメージからとびだしたみたいなまめっちが♥♥♥ うちをむかえにきてくれたのね♥♥♥) スペースまめキュン♥♥♥ \0");
const u8 D_2551ABC[] = _("             イヤーッ! こんなまめっち､うちのしってるまめっちじゃない! (…ううん､こんなワイルドなまめっちもアリかも…) まめキュン♥♥♥だゼ \0");
const u8 D_2551B64[] = _("             まえまえからくちぱっちには､むげんのかのうせいをかんじていたのよ  エクササイズしだいではモッテモテのチョーイケメンになれるかもしれないわ! ");
const u8 D_2551C10[] = _("             あら? アナタ! くちぱっちじゃない! どうしたっていうのよ? こんなカラダになって! くちぱっち…いや､くちぱっちん! いっしょにレッスンしてみない? あなたならまるでべつのじぶんにだってかわれるわ! \0");
const u8 D_2551CFC[] = _("             あらー!! アナタ､アタシごのみのタイプ♥ これからいっしょにおちゃでもどーお? ………ポ♥♥♥ ");
const u8 D_2551D7C[] = _("             くろまめっち､いつもおうえんしてくれてありがとう↑ おうえんしてくれるたまごっちがいてくれるから､つらいときでもがんばれちゃうんだよっ★ ");
const u8 D_2551E24[] = _("             いつもおうえんしてくれてありがとう↑↑ …でもね…くろまめセンパイだって､みんなのちゅーもくのまとだよ★ ");
const u8 D_2551EAC[] = _("             いつもさいぜんれつでおうえんしてくれてありがとう! ふりつけもいつもかんぺきだね♪ これからもずーーっとファンでいてね♪ ");
const u8 D_2551F44[] = _("             アニキー､いいところに! たまごっちせいをせいふくするけいかくをたてたんでヤンス! (ピポプポピポペポ…) うわー!ピポ! ここでしゃべってだれかにきかれたらどうするでヤンス!@ \0");
const u8 D_2552018[] = _("             アニキがカッコよくなったでヤンス! かがやいてみえるでヤンス! オレもカッコよくなりたいでヤンス! ピポ､オレたちもおふろでからだをみがいてみるでヤンス! (ピポポペプポ♪) \0");
const u8 D_25520E8[] = _("             ア､アニキ､そのすがたはどうしたでヤンス!? とってもつよそうにみえるでヤンス! これならもうたまごっちせいはせいふくしたもどうぜんでヤンス! (ピポペパパプポピ!) ちょ…ピポ!! アニキはそんなひどいことしないでヤンス!↓↓↓ \0");
const u8 D_25521F0[] = _("             やあ､おれねっち  いそいでオトナになろうとしてもうまくいかないものサ★ おまえはおまえのはやさでおおきくなればいいんじゃないか? \0");
const u8 D_2552294[] = _("             やあ､おまえ…ずいぶんオトナっぽくなったな  どうだい､これからいっしょにカフェでも……え､いまはつごうがわるい? そうか…おたがいおとなのつきあいがあるからな… じゃあまたのきかいに★ \0");
const u8 D_2552370[] = _("             なかなか､きたえているようだな! アイドルもカワイイだけではダメだ! たいりょくがじゅうようなのだぁぁぁぁ! ");
const u8 D_25523FC[] = _("             がんばっているようだな! これならせかいにもつうようするかもしれないぞ! どうだ､オレといっしょにせかいいちをめざしてみないかっ? \0");
const u8 D_25524A0[] = _("             お? …なんだか…ずいぶんぽっちゃりしたな! いっしょにうんどうするか? ");
const u8 D_2552508[] = _("             おう!メロディっち! またおれのやたいによってくれよな! すきなきょくをかけてやるぜ! なにがいい?クラシックか? …なに?えんかでええんか～? ガッハッハ! \0");
const u8 D_25525C8[] = _("             おれみたいなかっこうしてるなんて､さてはおまえ…おれのファンだな!? おれもビッグになったもんだ! ビッグリしたぜ★ ガッハッハ! \0");
const u8 D_255266C[] = _("             めめっちがドリームタウンにいっているあいだ､わたくしはたまストリートでオシャレにみがきをかけていたんですのよ! どうかしら､このぐるぐるヘアー､みごとですわよねぇ～! すなおにほめてくださっていいんですのよ? オーッホッホッホ! ");
const u8 D_2552770[] = _("             そ､そのすがた､いったいどうしたんですの? くやしいですけど､みごとなスタイルとぐるぐるヘアーですわね… でも､わたくしもまけるわけにはいきませんわ! \0");
const u8 D_2552828[] = _("             そ､そのすがた､いったいどうしたんですの? …ずいぶんとこう…こせいてきな… そんなぐるぐるヘアーもアリなんですの!?!? \0");
const u8 D_25528C4[] = _("             ひめスペっちがじっかからもどってきてくれてほんとうにうれしいです! またみんなであそびましょうね! \0");
const u8 D_2552948[] = _("             うわぁ､ひめスペっちのもっているそれ､ぼくにそっくりですね､かんげきです! ほかのたまごっちのもあるのでしょうか? ぼく､さがしてきますね! ");
const u8 D_25529F4[] = _("             ……えっ､ここでなにをしているのかって? つぎのサッカーのしあいについてかんがえてたんだ  またみんなでおうえんにきてくれよな! ");
const u8 D_2552A94[] = _("             いつもおうえんサンキューな! おうえんしてもらえると､さらにがんばろうってやるきがわいてくるんだ☼ これからもヨロシクな! \0");
const u8 D_2552B30[] = _("             これから､あたらしいはつめいでつかうぶひんをかいにいくところなんです  じぶんでつくったものをうごかすときって､とてもワクワクしますよね↑↑↑ \0");
const u8 D_2552BE0[] = _("             きょうはゴミゼロうんどうのひだよーー♪ てつだってくれたらプレゼントもあるよー! ");
const u8 D_2552C50[] = _("             ありがとー! おかげですっかりきれいになったよ↑ ");
const u8 D_2552CA0[] = _("             じゃー､これをあげるねー  アイテムショップでやすくおかいものできるよー♪ \0");
const u8 D_2552D0C[] = _("             じゃー､これをあげるねー  カフェレストランでやすくおしょくじできるよー☼ \0");
const u8 D_2552D78[] = _("             じゃー､これをあげるねー  カッコいいバッジでしょー? イカスー↑ \0");
const u8 D_2552DDC[] = _("             よくきたわね! さっそくエクササイズをはじめるわよ! ");
const u8 D_2552E30[] = _("             すばらしいわ! グッとステキになったんじゃないかしら!? ");
const u8 D_2552E88[] = _("             すこしはこうかがあったみたいね  でもゆだんしちゃダメよ! \0");
const u8 D_2552EE4[] = _("             やるきがかんじられないわ! でなおしていらっしゃい!! \0");
const u8 D_2552F3C[] = _("             まいど～! ちょっとみていかないか? ぜったいうまいど～★…ってか? ガッハッハ! \0");
const u8 D_2552FB0[] = _("             ひをふいてブワーッととびまわる｢アレ｣をつくってほしいんじゃ! ちょっとひつようになってしまってのう ");
const u8 D_2553034[] = _("             ぐるぐるまわるヒヤ～ッとする｢アレ｣をつくってほしいんじゃ! ちょっとひつようになってしまってのう \0");
const u8 D_25530B8[] = _("             てあしがついて､ピコピコうごく｢アレ｣をつくってほしいんじゃ! ちょっとひつようになってしまってのう ");
const u8 D_255313C[] = _("             やあキミ､ココらへんにブヒンがおちてこなかったかのう? おかしいのう…↓ ");
const u8 D_25531A4[] = _("             おお､コレじゃ  よくきづいたね  これがあればうちゅうのみんなもよろこぶぞ! キョーリョクありがとうなのじゃ! ");
const u8 D_2553234[] = _("             オイオイ… ハツメイするにはギジュツりょくがたりないみたいじゃな  がっこうでもっとスキルをみにつけたほうがよいぞ \0");
const u8 D_25532C8[] = _("             あなた…ただならぬセンスをおもちですわネ? おようふくにかけるパッショーネ☼ …じょうねつをみせてくださらナイ? ");
const u8 D_2553358[] = _("             あなた…ただならぬセンスをおもちですわネ? アクセサリーにかけるアモーレ☼ …あいじょうをみせてくださらナイ? \0");
const u8 D_25533E8[] = _("             あなた…ただならぬセンスをおもちですわネ? ユーモアたっぷりのアイテムをみせてくださらナイ? ");
const u8 D_2553464[] = _("             あら､まあまあまあ～…あなたにはまだオシャレははやかったみたいですわネ  ホホホ…☼ ");
const u8 D_25534D8[] = _("             まあっ★ これはベニッシモ! セレブリティですワ★ \0");
const u8 D_255352C[] = _("             あらあらあらあら～…ファッションセンスがたりなかったみたいですわネ…! ぶかつどうでセンスをみがいたほうがいいんじゃナクッテ? \0");
const u8 D_25535CC[] = _("             ヘイ､キミ…いいうごきをしているね★ そのあしのきようさ…そう､サッカーのテクニックをみせてくれないカナ? \0");
const u8 D_2553658[] = _("             ヘイ､キミ…いいうごきをしているね★ ぜひダンスのスキルをみせてくれないカナ? OK★ミュージックスタート♪ ");
const u8 D_25536E4[] = _("             ヘイ､キミ…いいうごきをしているね★ そのジャンプりょく…そう､なわとびしてみてくれないカナ? ヒュンヒュンヒュン★ ");
const u8 D_2553778[] = _("             ヘイ､キミ…いいうごきをしているね★ きっとしょうらい､いいダンサーになれるよ  またね★ \0");
const u8 D_25537F4[] = _("             OK★そこまで! グレイト&パーフェクトなパフォーマンスだったよ★ サンキュ～～!☼ ");
const u8 D_2553868[] = _("             OH★ノー! もっとトレーニングをつんだほうがいいね? がっこうでスキルをみにつければもっとクールになれるよ★ \0");
const u8 D_25538F8[] = _("             すこしせいさくにいきづまってしまってね､キミのかいた｢え｣をみせてもらえないだろうか? \0");
const u8 D_2553970[] = _("             キミの｢うたごえ｣がすばらしいというはなしをきいてね  なにか1きょくうたってもらえないだろうか? \0");
const u8 D_25539F4[] = _("             わたしがおもうに､これから｢とうげい｣のじだいがくる  キミ､なにか1つつくってもらえないだろうか? ");
const u8 D_2553A78[] = _("             おや､つぎのさくひんのこうそうをねっていたら､こんなところまできてしまったようだ  そろそろアトリエにもどらないと  では､しつれいするよ \0");
const u8 D_2553B24[] = _("             おお､これはすばらしい! こころがあらわれるようだ…! \0");
const u8 D_2553B7C[] = _("             ふむ､いまのキミではこんなものか  きたいはずれだったな…↓ もっとげいじゅつスキルをみがいておきたまえ! \0");
const u8 D_2553C08[] = _("             いまからパーティなんだ  おたんじょうびのおいわいだから､ぴったりなものをもっていきたいの! つくってくれない? ");
const u8 D_2553C98[] = _("             いまからパーティなんだ  はじまるまでちょっとじかんがあるから､ほっかほかのあたたかいものがたべたいな! なにかいいものないかな? \0");
const u8 D_2553D3C[] = _("             いまからパーティなんだ  パーティにぴったりのデザート! つくってくれない? ");
const u8 D_2553DA8[] = _("             きみにりょうりをまかせるには､いささかみじゅくなようだね  でなおすとしよう ");
const u8 D_2553E14[] = _("             ありがとう! こんなみごとなりょうりはいままでみたことがないよ! ");
const u8 D_2553E74[] = _("             りょうりはきほんがだいじ! がっこうでりょうりのうでまえをあげたほうがいいんじゃない? \0");
const u8 D_2553EEC[] = _("             おお､このいえはすばらしい……! わたしのオークションにしゅっぴんしてもらいたいくらいですが､これはおおきすぎてムリですね…… \0");
const u8 D_2553F8C[] = _("             きょうはウソをついてもいいひって? ウッソー! ホントー? こまるぅ～@@ \0");
const u8 D_2553FF8[] = _("             ママ…いつもありがとう☼ いつまでもキャワワなままでいてね～♥ \0");
const u8 D_2554058[] = _("             オヤジ! いつもサンキューな! オレもオヤジのようなでっかいオトコになりたいぜーっ! ");
const u8 D_25540CC[] = _("             つきっちのひかりってロマンティックね♥ わたし､かぐやひめになったみたい ");
const u8 D_2554134[] = _("             こどものけんこうと､かれいなるせいちょうをねがって  ちとせあめをなめましょう♪ ぺろぺろ  ぺろりんちょ \0");
const u8 D_25541C0[] = _("             きょうはすきなこにチョコをあげるひだす  ことしは8000こくらいもらえるよていだす ");
const u8 D_2554234[] = _("             やあ､みんなでげんきになかよく､ぶかつをたのしもうね☼ \0");
const u8 D_255428C[] = _("             ほっほっほっ､きょうもせいとたちがたのしそうにしていてうれしいですね～☼ ");
const u8 D_25542F4[] = _("             もうがっこうはおわったわよ  もっとはやいじかんにきてほーたいな! \0");
const u8 D_2554358[] = _("             をわすれずにもってきてね  それじゃあ､きをつけてかえるんだよ☼ ");
const u8 D_25543B8[] = _("             よくきたね  ここははつめいぶのぶしつだよ  つかうあいてのキモチをかんがえて､こころをこめてつくるのがたいせつだよ☼ \0");
const u8 D_2554450[] = _("             よいものをはつめいするためには｢しくみ｣をしってなきゃいけない  きょうはもののしくみをべんきょうしよう! \0");
const u8 D_25544DC[] = _("             なにをつくるにしても､まずはせっけいずをかかなきゃいけないんだ  きみはひっきようぐをもってないみたいだね… またきてね ");
const u8 D_2554574[] = _("             きょうはいよいよじっけんをするよ! あら? きみはじっけんキットをもってないみたいだね… またきてね ");
const u8 D_25545F8[] = _("             ようこそ､ファッションぶへ  みんなでたかめあって､ステキなたまごっちになりましょう★ \0");
const u8 D_2554670[] = _("             メイクでいちばんじゅうようなことは｢こころ｣よ  メイクをするあいてをおもいやるこころ…それがあればステキなメイクがきっとできるわ★ ");
const u8 D_2554714[] = _("             ひっきようぐをわすれたの? しかたないわね､まっていてあげるから､もっていらっしゃい↓ \0");
const u8 D_255478C[] = _("             メイクボックスをもってこなかったですって? やるきがないのかしら? あるならはやくもっていらっしゃい↓↓↓ \0");
const u8 D_2554818[] = _("             うんどうぶへよくきたなっ! このぶでは､みんなでからだをきたえるぞっ! めざすはたまごっちせいナンバーワンだぁぁぁぁぁ! ");
const u8 D_25548B0[] = _("             サッカーでいちばんたいせつなのはチームワークだっ! ひとりはみんなのために､みんなはひとりのために! なかまのけってんをおぎない､ちょうしょをいかせるよう､きをくばってうごくことがじゅうようなんだっ! ");
const u8 D_2554998[] = _("             こらぁぁぁぁぁ! きょうはサッカーのルールをおしえるっていっただろー! ひっきようぐをもってでなおしてこぉぉい! ");
const u8 D_2554A28[] = _("             こらぁぁぁぁぁ! サッカーボールをもってこなかったら､サッカーができないじゃないかっ! ダッシュでとってこぉぉぉぉぉい! ");
const u8 D_2554AC0[] = _("             ここはげいじゅつぶまる! さあ!じゃんじゃんアートするまるー! \0");
const u8 D_2554B20[] = _("             アートのきほんはかんさつりょく  おてほんをよくみることがアートのだいいっぽです  え?…2ほめですか? …そうぞうりょくかしら…? じゆうにのびのびやってみましょう♪ ");
const u8 D_2554BE8[] = _("             きょうはデッサンをするから､ひっきようぐがひつようまる☼ ");
const u8 D_2554C40[] = _("             かみねんど､わすれちゃったまる? ");
const u8 D_2554C80[] = _("             ここはりょうりぶ! おいしいりょうりをつくろうね! \0");
const u8 D_2554CD4[] = _("             りょうりのあじは､ざいりょうのよしあしだけできまるものじゃない! ウデがものをいうせかい! そしてウデよりだいじなものがある…それはココロ! ココロをこめてこそ､おいしいりょうりができるんだ! ");
const u8 D_2554DB4[] = _("             ひっきようぐがないと､レシピをメモできないよ… \0");
const u8 D_2554E04[] = _("             フライパンもなしにりょうりぶにくるなんて…しゃしんもとらずにグルメブログをかくようなものだね╬ \0");
const u8 D_2554E84[] = _("             いらっしゃいませ! ごちゅうもんをどうぞ♪ \0");
const u8 D_2554ED0[] = _("             いらっしゃいませ～♪ たまデパへようこそ～☼ ");
const u8 D_2554F1C[] = _("             ゲームいろいろ! ごっちポイントゲットだよ↑ ");
const u8 D_2554F68[] = _("             ハロー! インテリアコーディネートならおまかせ! ");
const u8 D_2554FB8[] = _("             いらっしゃいませ～♪ なにをおもとめでしょうか? ");
const u8 D_2555008[] = _("             いらっしゃいませ～♪ ナウいコーディネートはいかが♥ ");
const u8 D_255505C[] = _("             みんなオマチカネよ! しょうかいするわ♥ ");
const u8 D_25550A4[] = _("             しっかりおせわするわよ～ ");
const u8 D_25550DC[] = _("             げんきいっぱいだったけど､あなたにあってもっとげんきになったみたい♪ ");
const u8 D_2555140[] = _("             たくさんあそんで､きょうはたのしそうだったわ☼ \0");
const u8 D_2555190[] = _("             ほんをよんだりして､おとなしくしていたわ～ \0");
const u8 D_25551DC[] = _("             きょうはちょっとげんきがなかったわ… あなたとはなれて､さみしかったのかしら… \0");
const u8 D_255524C[] = _("             イタズラもいっぱいするくらい､げんきいっぱい! パワフルだったわよ! ");
const u8 D_25552B0[] = _("             とてもいいこにしてたわよ! いつもおうちでかわいがってもらっているからかしら? \0");
const u8 D_2555320[] = _("             すぐにどこかへいこうとするから､めがはなせなかったわ～  あなたにあいたかったのかしら? ");
const u8 D_2555398[] = _("             おむかえ､うれしいみたいね! やっぱりあなたといっしょにいるのが1ばんみたい☼ \0");
const u8 D_2555408[] = _("             このあいだたべたごはんがとってもおいしかったんだって! みんなにじまんしてたわよ! \0");
const u8 D_255547C[] = _("             おえかきのじかんに､あなたのにがおえをかいてたわ～♪ ");
const u8 D_25554D0[] = _("             ちいさいこがころんだときに､いたいのいたいのとんでいけ～♪ ってしてくれたのよ～ ");
const u8 D_2555540[] = _("             あたらしいおともだちができたみたい♪ おやつをなかよくはんぶんこしていたわ♥ ");
const u8 D_25555AC[] = _("             おひるねからめがさめたときに､ねぼけてあなたのなまえをよんでたわ～  ねぼすけさん☼ ");
const u8 D_2555620[] = _("             みんなでやきゅうをしてたら､きゅうにちょうちょをおいかけはじめちゃってビックリ! ちょうちょすきなのね～ ");
const u8 D_25556A8[] = _("             あなたとはなれてから､げんきがなかったわ… あなたのかおをみたら､ほっとしてないちゃったみたいね♥ \0");
const u8 D_255572C[] = _("             ちょっとホームシックになっちゃったのかもしれないわね… きょうはあまえさせてあげてね♪ \0");
const u8 D_25557A4[] = _("             きょうのきぶんは､どのきぶん? \0");
const u8 D_25557E4[] = _("             このみのタイプはいたかしら♥ どのたまごっちとおみあいする? ");
const u8 D_2555840[] = _("             これは､すてきなおうちになりましたね! きゃんでぃがだいすきなあのこも､このいえをきにいってくれたようですよ! \0");
const u8 D_25558D0[] = _("             なんと!そらにてがとどきそうないえになりましたよ! いちばんたかいおへやには そらのすきなたまごっちがすんでくれそうですね! ");
const u8 D_255596C[] = _("             これがツーしん100かいきねんハウス! つぎはどんなおうちになるのかたのしみですね! ");
const u8 D_25559E0[] = _("             おお､すばらしい……! このいえはまた､じつにほしくなるいえですね★★★ ");
const u8 D_2555A48[] = _("             ねえ､あなたもしょくぶつをそだててみない? めずらしいタネをあげるから､はたけにまいてみてね! \0");
const u8 D_2555AC8[] = _("             さくもつがかれちゃったんだもー? だれにでもしっぱいはあるんだもー  こんどはまいにち､みずやりをわすれないようにするんだもー☼ ");
const u8 D_2555B68[] = _("             たねをまいたら､まいにちおみずをあげにきてね☼ しょくぶつもきっとあなたをまっているわ! ");
const u8 D_2555BE0[] = _("             どう? あげたタネはじゅんちょうにそだっているかしら? ステキなはながさくといいわね☼ \0");
const u8 D_2555C58[] = _("             いっしょうけんめいそだてたら､きっとしょくぶつもあなたにかんしゃをするとおもうの  だから､がんばってそだててあげてね♪ ");
const u8 D_2555CF0[] = _("             ぼくものうじょうで､たくさんやさいをそだててるもー  ときどきしっぱいもあるけど､めげずにがんばるもー☼ ");
const u8 D_2555D78[] = _("             そういえば､ときどきめずらしいさくもつがとれるもー  キラキラだったり､ピカピカだったりするもー  あじもいいから､うりにいかずに､ついついたべちゃうんだもー↓↓ \0");
const u8 D_2555E3C[] = _("             カフェレストランにはよくいくも～? まいにちぼくののうじょうから､とれたてやさいをおくっているも～☼ いつもしんせんなんだも～☼☼ \0");
const u8 D_2555EE0[] = _("             いちばでうるしなものをしいれにきたのよ  やさいもくだものも､せんどがいのち♥ いつもしんせんなものを､たべてもらいたいからがんばるわ♪ ");
const u8 D_2555F88[] = _("             このはたけでは､ときどきキラキラしたやさいやくだものがとれるってホント?  どんなあじがするのかしらね～ ");
const u8 D_2556010[] = _("             まちをはなれて､だいしぜんのくうきをむねいっぱいすいこむと､すがすがしいきもちになるのよね♪ ふぁ～… ねむくなってきちゃった～☼☼ ");
const u8 D_25560B4[] = _("             おうさまのキングききっちでしょ! これからなかよくしてほしいんだし! ");
const u8 D_2556118[] = _("             さいきんよくあうでしょ! なかよくなってきたし､いっしょにあそびにいってみたいワケ★ ");
const u8 D_255618C[] = _("             このまえのドライブはたのしかったし! きょうはじかんがあるし､またいっしょにあそびにいきたいでしょ★ ");
const u8 D_2556210[] = _("             まってたでしょ! キミといっしょにいるととってもたのしいし､いっしょにすごしたいんだけど★ \0");
const u8 D_255628C[] = _("             だいすきでしょ! おれとけっこんしてほしいんだし! \0");
const u8 D_25562E0[] = _("             わたしはたまひこおうじです! あなたにおあいできてこうえいです★ ");
const u8 D_2556340[] = _("             またおあいしましたね! どうです､いっしょにどこかへでかけませんか? ");
const u8 D_25563A4[] = _("             このあいだはたのしかったですね…… どうでしょう､またいっしょにでかけませんか? ");
const u8 D_2556414[] = _("             あなたがくるのをいまかいまかとまっていました…… またわたしとおなじときをすごしてもらえませんか? \0");
const u8 D_2556498[] = _("             きょうは､あなたにわたしのおもいをつたえるためにまっていました…… わたしだけのプリンセスになってくれませんか?★ \0");
const u8 D_255652C[] = _("             むしゃござるっちでござる  けんさばきならだれにもまけないでござる★ ");
const u8 D_2556590[] = _("             しんこうをふかめるために､おぬしとあそびにいってみたいでござる★ ");
const u8 D_25565F0[] = _("             このまえはたのしかったでござる  きょうもまた､いっしょにでかけるというのはいかがでござろう? \0");
const u8 D_2556670[] = _("             まっていたでござる★ おぬしといっしょにいるととてもたのしいでござるな★ ");
const u8 D_25566D8[] = _("             おぬしをまっていたでござる! いざ!せっしゃとけっこんでござる★ ");
const u8 D_2556738[] = _("             はじめまして ゆめフェアリボンくるん♥ よろしくおねがいしますくるん♥ \0");
const u8 D_25567A0[] = _("             なんだかとおくにあそびにいきたいくるん♥ ");
const u8 D_25567E8[] = _("             このまえはたのしかったくるん♥  こんどはうみにいきたいくるんっ♥ \0");
const u8 D_255684C[] = _("             あなたとカラオケにいきたいくるん♥ いっしょにサイコーのデュエットしたいくるん♥ ");
const u8 D_25568BC[] = _("             あなたのおよめさんになりたいくるん♥ あなたのやさしさでくるんでくれたらきっとしあわせになれるくるん♥ \0");
const u8 D_2556944[] = _("             わらわはバランランバラランランおうこくのひめ､ひめばらっちであるぞ! よ､よろしくねっ♥ ");
const u8 D_25569BC[] = _("             はよう!くるまっちをまわさぬか! わらわにあしであるけというのかぇ?  …あ､ピンクのくるまっちかわいい～～♥ \0");
const u8 D_2556A4C[] = _("             そちも､わらわとおるとしあわせであろう! わらわのかおがあかい?!ぶれいもの! ひやけしただけなんだからぁ…♥ \0");
const u8 D_2556ADC[] = _("             わらわとカラオケに? ほう､わらわとデュエットしたら､たまともにじまんできるぞぇ  すてきすてきぃ～バランランバラランラン～♪ \0");
const u8 D_2556B7C[] = _("             こうえいにおもうがよいぞ!わらわをきさきにできるのだから ……つまり､けっこんしよう～♥ ");
const u8 D_2556BF4[] = _("             キラおりひめっていいます★よろしくね★ \0");
const u8 D_2556C3C[] = _("             いろんなところにあそびにいきたいな★ ");
const u8 D_2556C80[] = _("             うみってミルキーウェイよりひろいってほんと?あたしをつれていってほしいな★ \0");
const u8 D_2556CEC[] = _("             あなたといっしょにうたいたいな★ きっとアルタイルより､かがやけるとおもうわ★ だから ねぇ､いっしょにカラオケにいきましょ★ \0");
const u8 D_2556D8C[] = _("             ひろいぎんがであなたとめぐりあえたことはうんめいだわ★ あたしとけっこんしてください★ \0");
const u8 D_2556E04[] = _("             グッドアフタヌーン! これはおみやげ! どこにいったのか､わかるかな～? それじゃ､そろそろつぎのたびにでるからいくね  ボン・ボヤージュ★ ");
const u8 D_2556EB0[] = _("             ぼくね､おそらのくもをながめてたんだー  くもっていろんなかたちがあってすごくたのしいんだよ! きみはあのくもなににみえる? ");
const u8 D_2556F4C[] = _("             おそらのくももこれとおんなじであまくておいしいのかなぁ  そうだ! これあげるねっ! いつかあのくももみんなでちぎってたべたいなー \0");
const u8 D_2556FF0[] = _("             あたし､みんなのえがおがだいすきなの★ だからあしたもあさっても､みんなげんきいっぱいでいられるように､もっときゃんでぃぱくぱくするんだ! あなたもきゃんでぃぱくぱくしよっ★ \0");
const u8 D_25570C0[] = _("             げんきがないときはきゃんでぃぱくぱくだよっ! もっとゲンキになりたいときは  はい! きゃんでぃぱくぱん! \0");
const u8 D_255714C[] = _("             お? メロディっち､きょうはいちだんとゴージャスじゃねーか! ゴージャスでごーじゃいます★…ってか? ガッハッハ! \0");
const u8 D_25571E0[] = _("             おや､ラブソラっち  キミはそらがだいすきらしいのぉ  そらはいいぞ…  そらはだれのところにだってつながってるんじゃ! いつかいっしょにそらのかなたまでとんでいきたいのう! \0");
const u8 D_25572B0[] = _("             おお､ラブソラっちがさらにりっぱなおとこのこになったようじゃのう! いまのきみとなら､どんなたかいくものうえだって､いっしょにとびまわれそうじゃ! \0");
const u8 D_2557364[] = _("             あれ? ひめスぺっち…じゃないんですか? なんだかいつもといんしょうがちがうので､びっくりしてしまいました☼ ");
const u8 D_25573F0[] = _("             あらあら～@@ きゃんでぃぱくぱくもみんなのえがおのためにあめちゃんくばってるんだって@? わかいのにかんしんねぇ～@@ おばちゃんもまけてられないわねっ! いっしょにみんなをげんきにしましょ@@ ");
const u8 D_25574D4[] = _("             あらっ! あんた､あめちゃんだいすきってすごくつたわってくるわぁ@@ そうだ! めめちゃんのあめちゃんもあげるっ! ほらほらえんりょしないで､たくさんもっていきなさいっ@ \0");
const u8 *const txt_general[] = {
    D_254F64C, D_254F6AC, D_254F730, D_254F7A0,
    D_254F830, D_254F8A4, D_254F944, D_254F9D4,
    D_254FA44, D_254FAE0, D_254FB64, D_254FBE8,
    D_254FC94, D_254FD28, D_254FDE8, D_254FEB0,
    D_254FF54, D_2550008, D_25500C8, D_255016C,
    D_2550220, D_25502C0, D_2550384, D_2550420,
    D_25504DC, D_2550584, D_2550628, D_25506C8,
    D_255077C, D_2550834, D_25508DC, D_2550984,
    D_2550A24, D_2550ABC, D_2550B5C, D_2550C30,
    D_2550CF8, D_2550DB0, D_2550E58, D_2550EFC,
    D_2550FE0, D_25510B8, D_255116C, D_255123C,
    D_25512B0, D_2551328, D_25513C4, D_2551480,
    D_2551514, D_25515E4, D_2551698, D_2551724,
    D_25517C8, D_2551874, D_25518EC, D_255198C,
    D_2551A04, D_2551ABC, D_2551B64, D_2551C10,
    D_2551CFC, D_2551D7C, D_2551E24, D_2551EAC,
    D_2551F44, D_2552018, D_25520E8, D_25521F0,
    D_2552294, D_2552370, D_25523FC, D_25524A0,
    D_2552508, D_25525C8, D_255266C, D_2552770,
    D_2552828, D_25528C4, D_2552948, D_25529F4,
    D_2552A94, D_2552B30, D_2552BE0, D_2552C50,
    D_2552CA0, D_2552D0C, D_2552D78, D_2552DDC,
    D_2552E30, D_2552E88, D_2552EE4, D_2552F3C,
    D_2552FB0, D_2553034, D_25530B8, D_255313C,
    D_25531A4, D_2553234, D_25532C8, D_2553358,
    D_25533E8, D_2553464, D_25534D8, D_255352C,
    D_25535CC, D_2553658, D_25536E4, D_2553778,
    D_25537F4, D_2553868, D_25538F8, D_2553970,
    D_25539F4, D_2553A78, D_2553B24, D_2553B7C,
    D_2553C08, D_2553C98, D_2553D3C, D_2553DA8,
    D_2553E14, D_2553E74, D_2553EEC, D_2553F8C,
    D_2553FF8, D_2554058, D_25540CC, D_2554134,
    D_25541C0, D_2554234, D_255428C, D_25542F4,
    D_2554358, D_25543B8, D_2554450, D_25544DC,
    D_2554574, D_25545F8, D_2554670, D_2554714,
    D_255478C, D_2554818, D_25548B0, D_2554998,
    D_2554A28, D_2554AC0, D_2554B20, D_2554BE8,
    D_2554C40, D_2554C80, D_2554CD4, D_2554DB4,
    D_2554E04, D_2554E84, D_2554ED0, D_2554F1C,
    D_2554F68, D_2554FB8, D_2555008, D_255505C,
    D_25550A4, D_25550DC, D_2555140, D_2555190,
    D_25551DC, D_255524C, D_25552B0, D_2555320,
    D_2555398, D_2555408, D_255547C, D_25554D0,
    D_2555540, D_25555AC, D_2555620, D_25556A8,
    D_255572C, D_25557A4, D_25557E4, D_2555840,
    D_25558D0, D_255596C, D_25559E0, D_2555A48,
    D_2555AC8, D_2555B68, D_2555BE0, D_2555C58,
    D_2555CF0, D_2555D78, D_2555E3C, D_2555EE0,
    D_2555F88, D_2556010, D_25560B4, D_2556118,
    D_255618C, D_2556210, D_255628C, D_25562E0,
    D_2556340, D_25563A4, D_2556414, D_2556498,
    D_255652C, D_2556590, D_25565F0, D_2556670,
    D_25566D8, D_2556738, D_25567A0, D_25567E8,
    D_255684C, D_25568BC, D_2556944, D_25569BC,
    D_2556A4C, D_2556ADC, D_2556B7C, D_2556BF4,
    D_2556C3C, D_2556C80, D_2556CEC, D_2556D8C,
    D_2556E04, D_2556EB0, D_2556F4C, D_2556FF0,
    D_25570C0, D_255714C, D_25571E0, D_25572B0,
    D_2557364, D_25573F0, D_25574D4, 0,
};
const u8 D_2557950[] = _("             こどもがトイレにいきたそうにしていたら､すぐにつれていってあげてね! ひとりでいけるようになるためのれんしゅうよ! \0");
const u8 D_25579E4[] = _("             よるになったらこどもをおふろにいれてあげてね! いつかひとりで､はいれるようになるといいわよね! ");
const u8 D_2557A64[] = _("             こどもがおへやをちらかしていたら､すぐにおそうじしてあげてね! そのうちじぶんでおかたづけしなきゃっておもうかもしれないわね! \0");
const u8 D_2557B04[] = _("             ひとりではちいさなエコでも､みんなでやればジ・アースをたすけるちからになるよねー☼ あなたからそっせんしてエコをはじめよー★ ");
const u8 D_2557BA0[] = _("             おかいものでレジぶくろをもらわないのも､あきカンをリサイクルするのも､とってもかんきょうにやさしいことだよー★ \0");
const u8 D_2557C30[] = _("             ワン・ツー・スリー・フォー♪ ボクのパフォーマンスについてこられるパートナーぼしゅうちゅー★ スポーツできたえたカラダはげいじゅつひんさ★ \0");
const u8 D_2557CDC[] = _("             わたしはせかいじゅうをまわってステキなアートをさがしているんだ! キミもえがじょうずってきいたけど､そうなのかい? \0");
const u8 D_2557D70[] = _("             ぼくはおいしいものにめがなくてね  いいにおいのするウチにはおじゃましたりしているんだ  クンクン♪おいしいりょうりのにおいがするぞ♪ \0");
const u8 D_2557E18[] = _("             トイレをガマンするとカラダにわるいぜ～  がっこうでも､いきたくなったらはずかしがらず､トイレにいっといれ～ってな★ ガッハッハ! \0");
const u8 D_2557EBC[] = _("             おめんこうちょう､カゼひいたんだって? え?なに?…ぜっこうちょう? わりーわりーおれのかんちがいだったぜ おめんなさい★…ってか? ガッハッハ! \0");
const u8 D_2557F70[] = _("             くるしいことがあったら､たまともにそうだんしてみな! きっときもちがラクになるぜ! くるしいことのあとには､たのしいことがやってくるし～★…ってか? ガッハッハ! \0");
const u8 D_2558034[] = _("             ちいさいころからはつめいをがんばったら､きっとまじめでステキなたまごっちにそだつです～ \0");
const u8 D_25580AC[] = _("             よもぎだんごのようなくちぱっち  きっとこどものころからおいしいものをたくさんたべていたんだろうな～ ");
const u8 D_2558130[] = _("             カレーをあびるようにたべつづければ､きっとくろくておとこらしいたまごっちになれるでヤンス  ス､スパイシ～?ってかんじでヤンス \0");
const u8 D_25581D0[] = _("             ちいさいころからしっかりおせわするのがだいじみたい  だけど､ときにはすこしきょりをおいてみるのもだいじかもねぇ…ねぇ? ");
const u8 D_2558268[] = _("             ちいさなころからバッチリうんどうしておくと､うんどうがとくいなたまごっちになれるらしいんだよねぇ! \0");
const u8 D_25582EC[] = _("             たべるのがだいすきなこがおいしそうにものをたべるすがたってみていてきもちいいよね～  たくさんたべてビッグになってね! \0");
const u8 D_2558384[] = _("             アタシのこのひきしまったボディをみて! エクササイズのたまものよ! ひきしめボディでみちがえるかも! こうえんでまってるわね! レッツダンシング! \0");
const u8 D_2558438[] = _("             ときどきこうえんでラブリンをみかけることがありますよ  ラブリンファンならあいにいってみてもよいのでは? ｢ほし3つ!｣なことがあるかもしれませんよ★★★ \0");
const u8 D_25584F4[] = _("             まちであったキラキラのイケメンにきいたんだ! ふろでピカピカにみがいただけで､あの､しんじゅのようなかがやきだってよ! しんじゅられない★…ってか? ");
const u8 D_25585A8[] = _("             おとうとぶんにであうと､いっぽオトナなじぶんになれるとおもうんでヤンス  スカーフがおそろいのあのたまごっちも､そんなであいでオトナになるとおもうでヤンス! ");
const u8 D_2558664[] = _("             はつめいずきなこには､べんきょうさせるだけじゃなくて､はつめいしやすいかんきょうをととのえてあげるといいかもね  すごいことがおきるかもしれないよ? ");
const u8 D_2558718[] = _("             ほし3つなファッションセンスのもちぬしというのは､やはりすんでいるおへやもすばらしいファッションであふれているんでしょうね \0");
const u8 D_25587B4[] = _("             まじめなヤツほど､へんなグレかたをしたりするもんなんだ  こうみえておれもまじめなわけよ…あいじょーのあるツッコミたのむぜ! ");
const u8 D_2558850[] = _("             たまごっちせいをせいふくするためには…すききらいせず､すべてのことをがんばるのがちかみちでヤンス! あ､ふくのせいふくのことでヤンス! \0");
const u8 D_25588F8[] = _("             よるになったらこどもをおふろにいれてあげてね! いつかひとりで､はいれるようになるといいよね! \0");
const u8 D_2558978[] = _("             おみずのだしっぱなし  でんきのつけっぱなし  れいぞうこあけっぱなしにしてなあい? なんでも｢ぱなし｣はよくないよー \0");
const u8 D_2558A10[] = _("             0のつくひはゴミゼロうんどうのひだよー! こうえんでゴミひろいをしているから､よかったらてつだってねー♪ ちょっとしたおれいもよういしてるよー★ ");
const u8 D_2558AC0[] = _("             ふるくてもまだつかえるもの､こわれてもなおせばつかえるものはたいせつにつかおうねー  ｢モッタイナーイ｣とおもうきもちをだいじにしようねー♥ ");
const u8 D_2558B6C[] = _("             99パーセントのドリョクも1パーセントのヒラメキがなければムダになってしまうんじゃ  ぐるぐる@ズドーン↓ピカーッ☼っとヒラメキがおりてこんかのう \0");
const u8 D_2558C20[] = _("             ボンボンボンジョルノ@オッシャーレなファッションをもとめてウォーキングちゅうヨ  アナタのうちにもいくかもネ ");
const u8 D_2558CAC[] = _("             おやをおこらせるのはよくないが､かなしませるのはもっとよくないぞ  えがおにできるのがイチバンだな! ");
const u8 D_2558D30[] = _("             どんなにしっぱいしたって､おおきくなればわらえるひもくるもんだ  だからしっぱいしても､くよくよするんじゃないぞ? いいな? ");
const u8 D_2558DCC[] = _("             ラクだからって､だれかのマネばっかりしてるとジブンらしさをみうしなっちまうぞ､マネーはおっかねーってな \0");
const u8 D_2558E54[] = _("             レストランやたまデパでは､いまうっているアイテムいがいにも､あたらしいアイテムがうりだされることがあるらしいよー  まめにじょうほうをチェックしよー ");
const u8 D_2558F08[] = _("             こどものころからげいじゅつのセンスをみがくとよりビューティフルなおとなになるわよ! そう､このアタシのように! \0");
const u8 D_2558F98[] = _("             ゆうしゅうなはつめいかはこどものときにハツメイのちからをみがいているみたいなんだね  きみたちもがんばってね ");
const u8 D_2559024[] = _("             ねぇね? …いや､ねぇちゃんのこどものころ? んー､たべることがだいすきだったよ! しあわせそうなかおしてね☼ \0");
const u8 D_25590B4[] = _("             ごきげんよう! こどものころからファッションをこころがけていれば､ぐるぐるオシャレになれましてよ! わたくしのライバルになれましてよ! \0");
const u8 D_255915C[] = _("             メロディっちはああみえてたいりょくにじしんがあるそうですよ! ");
const u8 D_25591B8[] = _("             あまりたいじゅうをきにしないでいると…からだにへんかがおきることもあるようです! ");
const u8 D_2559228[] = _("             まいにちこうえんでエクササイズしてたら､カラダがひきしまったみたいだっ! にくたいかいぞうだぁあああ! \0");
const u8 D_25592B0[] = _("             みんなそれぞれ､うんめいのであいってあるとおもうの  たとえばこうえんで､デアイはとつぜんに…♥♥ \0");
const u8 D_2559334[] = _("             きがむいたら､いつでもおみあいパーティにきてね  かんげいするわ★ きっとステキなであいが…♥♥ ");
const u8 D_25593B4[] = _("             ぐるぐるヘアがカンペキでも､みもこころもじぶんのへやもオシャレにみがきあげなきゃ､ファッションクイーンにはなれないぜ★ \0");
const u8 D_255944C[] = _("             わかいころは､すべてのぶかつにかよってじぶんをみがいたものよ…! あのときはかがやいてたわ! せーしゅんっ★ ");
const u8 D_25594D8[] = _("             あいじょーがないじょー､さみしいじょー★…ってか? ガッハッハ! おやじギャグずきのたまごっちにきかしてやりてーな! ");
const u8 D_255956C[] = _("             きょうのふくイケてるじゃねーか! コーディネートはこーでぃねーと★ おまえオシャレでおれダジャレ♪…ってか? ガッハッハ! \0");
const u8 *const txt_tips[] = {
    D_2557950, D_25579E4, D_2557A64, D_2557B04,
    D_2557BA0, D_2557C30, D_2557CDC, D_2557D70,
    D_2557E18, D_2557EBC, D_2557F70, D_2558034,
    D_25580AC, D_2558130, D_25581D0, D_2558268,
    D_25582EC, D_2558384, D_2558438, D_25584F4,
    D_25585A8, D_2558664, D_2558718, D_25587B4,
    D_2558850, D_2557950, D_25588F8, D_2557A64,
    D_2558978, D_2558A10, D_2558AC0, D_2558B6C,
    D_2558C20, D_2558CAC, D_2558D30, D_2558DCC,
    D_2558E54, D_2558F08, D_2558F98, D_2559024,
    D_25590B4, D_255915C, D_25591B8, D_2559228,
    D_25592B0, D_2559334, D_25593B4, D_255944C,
    D_25594D8, D_255956C, 0,
};
const u8 D_25596D4[] = _("╬ダミーテキストをいれます╬");
const u8 D_25596F4[] = _("ゆうじょうパワー★\nきずなマックス!?\0");
const u8 D_2559720[] = _("すてきなライバル!\nしげきてきなあいて\0");
const u8 D_255974C[] = _("なんかテレパシーで\nかいわできちゃう");
const u8 D_2559774[] = _("いっぱいはなすと\nきずなふかまる♪\0");
const u8 D_255979C[] = _("ふたりのきもちが\nつうじあう!?");
const u8 D_25597C0[] = _("きょうのふたりは\nベストフレンド♥\0");
const u8 D_25597E8[] = _("もっといっぱいかま\nってほしかった…\nさみしかった…\nこんどあえたらそのときは\nもっともっとあそぼうね…");
const u8 *const txt_fortune_results[] = {
    D_25596D4, D_25596F4, D_2559720, D_255974C,
    D_2559774, D_255979C, D_25597C0, D_25597E8,
    0,
};
const u8 D_2559878[] = _("☼☼☼☼☼☼☼☼☼\n☼ あけまして ☼\n☼ おめでとう ☼\n☼☼☼☼☼☼☼☼☼\0");
const u8 D_25598CC[] = _("★★★★★★★★★★\n★  メリー   ★\n★ クリスマス♥ ★\n★★★★★★★★★★\0");
const u8 D_2559928[] = _("ちょっとぐあいが\nわるいみたい…");
const u8 D_255994C[] = _("からだに\nきをつけて､\nおしごと\nがんばってね☼");
const u8 D_2559980[] = _("おたんじょうびおめで\nとう!これからの1ね\nんが､すてきなものに\nなりますように★\0");
const u8 D_25599D8[] = _("いつもありがとう!\nこれからもよろしくね");
const u8 D_2559A04[] = _("おてつだい\nできることがあったら\nいつでもいってね\0");
const u8 D_2559A3C[] = _("きょうのごはん\nとっても\nおいしかったよ!\nまたたべたいな\0");
const u8 D_2559A7C[] = _("おそくまでおしごと\nごくろーさまです★\nむりしすぎないでね!");
const u8 D_2559ABC[] = _("きょうもいちにち\nおつかれさま★\nあしたもいいひで\nありますように★★★");
const u8 D_2559B08[] = _("べんきょう\nがんばります!\nみててね!!");
const u8 D_2559B34[] = _("あとではなしたい\nことがあるよ!");
const u8 D_2559B58[] = _("いつまでも\nげんきでいてね!");
const u8 D_2559B78[] = _("きょうはなんだか\nハッピーなキブン♥");
const u8 D_2559BA0[] = _("おはよう!\nきょうもいちにち､\nげんきにはりきって\nいきましょー↑↑↑\0");
const u8 D_2559BEC[] = _("きょうの\nコーディネート､\nどうかな?\0");
const u8 D_2559C18[] = _("ちょっとほしいものが\nあるんだけど…\n｢なにがほしいの?｣\nってきいてください\0");
const u8 D_2559C6C[] = _("おなかすいたー!\nおいしいおやつが\nたべたいな!");
const u8 D_2559CA0[] = _("きょうのばんごはんは\nなにかな?\nすきなものだったら\nいいな♪♪");
const u8 D_2559CE4[] = _("こんどのおやすみ\nどこかいきたいな★");
const u8 D_2559D0C[] = _("おでかけしてるあいだ\nたまごっちのおせわを\nよろしくね♪");
const u8 D_2559D48[] = _("おこづかいアップ\nしてほしいな～!!");
const u8 D_2559D70[] = _("ごめんなさい…｡");
const u8 D_2559D84[] = _("これからもずっと\nなかよしでいようね");
const u8 D_2559DAC[] = _("いま､そだててる\nたまごっち…また\nかわいくせいちょう\nしたんだ～!");
const u8 D_2559DF4[] = _("きょうははやくかえっ\nてきてね!\nまってるからね!\0");
const u8 D_2559E2C[] = _("きょうはかえりが\nおそくなります!\0");
const u8 D_2559E54[] = _("おやすみなさい@\nまたあした～@@\0");
const u8 D_2559E7C[] = _("なんだかとっても\nつかれたよ……↓↓↓\0");
const u8 D_2559EA8[] = _("あしたの☼あさ☼\nちゃんとおこしてね★\0");
const u8 *const mail_card_templates[] = {
    D_2559878, D_25598CC, D_2559928, D_255994C,
    D_2559980, D_25599D8, D_2559A04, D_2559A3C,
    D_2559A7C, D_2559ABC, D_2559B08, D_2559B34,
    D_2559B58, D_2559B78, D_2559BA0, D_2559BEC,
    D_2559C18, D_2559C6C, D_2559CA0, D_2559CE4,
    D_2559D0C, D_2559D48, D_2559D70, D_2559D84,
    D_2559DAC, D_2559DF4, D_2559E2C, D_2559E54,
    D_2559E7C, D_2559EA8, 0,
};
const u8 D_2559F50[] = _("このあいだはアリガトウ♥\nツーしんは\nたのしかったよ♪\nまたあそぼうね～");
const u8 D_2559F9C[] = _("はあ～い@@さいきんきに\nなるアイテムがあるんだけ\nど､2つもってた\nりしないかな～?\0");
const u8 D_2559FF8[] = _("やっほー↑げんきしてる?\nまたこんどあそびたいなっ\nておもっててがみ\nをおくったよ★");
const u8 D_255A050[] = _("ひさしぶり!こんどおしゃ\nべりしながらプロフこうか\nんしようね!↑↑\nやくそくだよ★");
const u8 D_255A0A8[] = _("ハロー♪ハロー♪♪\nあさのこうえんでしかやっ\nてないおみせのウ\nワサ､しってる?");
const u8 D_255A0FC[] = _("こんど､ふたりのあいしょ\nううらないしたいな～♥♥\nどんなけっかがで\nるかたのしみ♪");
const u8 D_255A154[] = _("ねぇねぇ!きょうはなにし\nてるの?よかったらまたあ\nそびたいなって､\nおもってるんだ★\0");
const u8 D_255A1B0[] = _("いっしょにしゃしんをとっ\nて､ステキなおもいでをの\nこしたいんだけど\nどうかな?");
const u8 *const mail_friend_templates[] = {
    D_2559F50, D_2559F9C, D_2559FF8, D_255A050,
    D_255A0A8, D_255A0FC, D_255A154, D_255A1B0,
    0,
};
const u8 D_255A228[] = _("★おおあたり★\n★おめでとう!★");
const u8 D_255A24C[] = _("ミラクルハッピー!\nなんでもチャレンジ\0");
const u8 D_255A278[] = _("ふたりのキモチ\nシンクロしてる?♥\0");
const u8 D_255A2A0[] = _("★ゴージャス★\n★まるでセレブ★");
const u8 D_255A2C4[] = _("イメージどおりに\nからだがうごく!?");
const u8 D_255A2EC[] = _("テストで100てん\nとれるよかん★\0");
const u8 D_255A314[] = _("みんながあなたを\nちやほやしたい!\0");
const u8 D_255A33C[] = _("みんながあなたを\nたよりにしてるよ★");
const u8 D_255A364[] = _("♥りゅうこう♥\n♥さいせんたん♥");
const u8 D_255A388[] = _("カリスマオーラ★\nあふれでてるかも!");
const u8 D_255A3B0[] = _("なんでもできちゃう\nそんなきがします!\0");
const u8 D_255A3DC[] = _("あなたのミリョクが\nつたわるヨカン!?\0");
const u8 D_255A408[] = _("キミはまるで\nまねきねこっち★\0");
const u8 D_255A42C[] = _("じこベストこうしん\nできるかも↑↑↑");
const u8 D_255A454[] = _("にがてなかもくも\nきょうはできるかも");
const u8 D_255A47C[] = _("なにもいわなくても\nわかりあえる!?");
const u8 D_255A4A4[] = _("かのうなかぎり\nてつだってあげよう\0");
const u8 D_255A4CC[] = _("アナタがじだいの\nトレンドになる!?");
const u8 D_255A4F4[] = _("アナタがウワサの\nちゅうしんかも!?");
const u8 D_255A51C[] = _("7わりがた\nうまくいきます♥");
const u8 D_255A53C[] = _("きになるあのコと\nめがあっちゃう?\0");
const u8 D_255A564[] = _("おこづかいが\nもらえたらいいね♪");
const u8 D_255A588[] = _("ライバルにさを\nつけるチャンスかも\0");
const u8 D_255A5B0[] = _("しゅうちゅうして\nべんきょうできそう");
const u8 D_255A5D8[] = _("きょうはともだちと\nあいしょうばっちり\0");
const u8 D_255A604[] = _("たのまれたらイヤな\nかおはしないでね☼\0");
const u8 D_255A630[] = _("ながれにさからわず\nのっていこう★\0");
const u8 D_255A658[] = _("なにげにちゅーもく\nされているかも!?\0");
const u8 D_255A684[] = _("じんせい ときには\nふつうもいいよね!\0");
const u8 D_255A6B0[] = _("キミのおもい\nとどくといいねっ\0");
const u8 D_255A6D4[] = _("くじびきも\nときどきあたる!?\0");
const u8 D_255A6F8[] = _("あがるかおちるかの\nしょうぶどころ!");
const u8 D_255A720[] = _("まちがえちゃったら\nふくしゅうしよう");
const u8 D_255A748[] = _("あいてのいけんを\nちゃんときこうね♪");
const u8 D_255A770[] = _("いっしょうけんめい\nやることがたいせつ\0");
const u8 D_255A79C[] = _("あなたのセンスが\nとわれるのはいま!");
const u8 D_255A7C4[] = _("もうすこしアピール\nしてもいいかもね★\0");
const u8 D_255A7F0[] = _("いいこともある!\nわるいこともある!");
const u8 D_255A818[] = _("ビミョ～にモテる\nかのうせいアリ");
const u8 D_255A83C[] = _("なくしものには\nちゅいうしよう↓");
const u8 D_255A860[] = _("なっとくできる\nけっかがでないかも\0");
const u8 D_255A888[] = _("わからないときは\nすなおにしつもん★");
const u8 D_255A8B0[] = _("おこらしちゃうかも\nすなおにあやまろう\0");
const u8 D_255A8DC[] = _("しんせつのつもりが\nうらめになるかも");
const u8 D_255A904[] = _("じだいがキミに\nおいついてないカモ\0");
const u8 D_255A92C[] = _("ジブンをもっと\nハッシンしよう!");
const u8 D_255A950[] = _("いまがそこだから\nあとはあがるだけ\0");
const u8 D_255A978[] = _("どんなにわるくても\nゼロではないよ!");
const u8 D_255A9A0[] = _("ムダづかいに\nきをつけて!\0");
const u8 D_255A9C0[] = _("ケガをしないよう\nきをつけてね!");
const u8 D_255A9E4[] = _("あきらめたらおわり\nがんばろう!");
const u8 D_255AA08[] = _("ときがかいけつして\nくれることもあるよ\0");
const u8 D_255AA34[] = _("なにごともめげずに\nガンバってこー★");
const u8 D_255AA5C[] = _("りゅうこうさきどり\nしすぎかも…↓\0");
const u8 D_255AA84[] = _("やまないあめは\nないよ\0");
const u8 D_255AAA0[] = _("おおきくのびをして\nきぶんスッキリ!");
const u8 D_255AAC8[] = _("しんこきゅうして\nリフレッシュ★");
const u8 D_255AAEC[] = _("はやおきして\nひのでをみよう☼\0");
const u8 D_255AB10[] = _("キライなことから\nさいのうひらくかも");
const u8 D_255AB38[] = _("きれいなことばを\nいしきしてはなそう");
const u8 D_255AB60[] = _("ちょきんばこに\nちょきんしよう♪");
const u8 D_255AB84[] = _("ゆびのぐるぐるを\nじっくりみよう@\0");
const u8 D_255ABAC[] = _("ストラップを\nかえてみよう♥");
const u8 D_255ABCC[] = _("おともだちと\nツーしんしよう♪\0");
const u8 D_255ABF0[] = _("いえのまわりを\nさんぽしてみよう@\0");
const u8 D_255AC18[] = _("まえがみのわけめを\nかえてイメチェン★\0");
const u8 D_255AC44[] = _("おにくをたべて\nパワーアップ↑↑↑\0");
const u8 D_255AC6C[] = _("あなたのまわりが\nパワースポット!!");
const u8 D_255AC94[] = _("9 2 7…\nなんかきになります");
const u8 D_255ACB8[] = _("おっかなせんせい\nのめにもなみだ");
const u8 D_255ACDC[] = _("わらうかどには\nハピハピっち");
const u8 D_255ACFC[] = _("はらがへってはくち\nぱっちんにならず");
const u8 D_255AD24[] = _("おわりよければ\nオールライトっち");
const u8 D_255AD48[] = _("りんきおうへんな\nたいおうをしよう\0");
const u8 D_255AD70[] = _("あせらないで\nいそがばまわれ@\0");
const u8 D_255AD94[] = _("こうじまおおし\nゆだんせずにいこう\0");
const u8 D_255ADBC[] = _("きょうできることは\nきょうやろう☼\0");
const u8 D_255ADE4[] = _("たにんにまけても\nじぶんにまけるな!");
const u8 D_255AE0C[] = _("けいぞくはちから\nつづけるのがだいじ");
const u8 D_255AE34[] = _("ときはかねなり\nじかんはたいせつに\0");
const u8 D_255AE5C[] = _("ころばぬさきのつえ\nきをつけていこう");
const u8 D_255AE84[] = _("のこりものには\nふくがあるかも☼");
const u8 D_255AEA8[] = _("せんてひっしょう\nはやさがカギだよ★");
const u8 D_255AED0[] = _("たんきはそんき\nじっくりいこう☼");
const u8 D_255AEF4[] = _("きょうのどりょくが\nしょうりのカギかも\0");
const u8 D_255AF20[] = _("きょうはもしかして\nくもりのちはれ?");
const u8 D_255AF48[] = _("あおのワンポイント\nみりょくばいぞう?\0");
const u8 D_255AF74[] = _("ピンクのアクセで\nアゲていこ♪\0");
const u8 D_255AF98[] = _("すききらいなく\nごはんをたべよう!\0");
const u8 D_255AFC0[] = _("あたまもつかって\nれんしゅうしよう!");
const u8 D_255AFE8[] = _("きゅうけいするのも\nだいじだよ\0");
const u8 D_255B00C[] = _("とりあえず\nやってみよう!\0");
const u8 D_255B02C[] = _("すいぶんほきゅうは\nこまめにしよう!");
const u8 D_255B054[] = _("よしゅうふくしゅう\nしっかりやろう\0");
const u8 D_255B07C[] = _("あかいハンカチが\nこううんのカギかも");
const u8 D_255B0A4[] = _("できることから\nはじめていこう♪");
const u8 D_255B0C8[] = _("まわりのひとに\nやさしくしよう☼");
const u8 D_255B0EC[] = _("わすれものがないか\nかくにんしよう!");
const u8 D_255B114[] = _("たのしんでやれば\nけっかはついてくる");
const u8 D_255B13C[] = _("もう1かい\nチャレンジ!");
const u8 *const txt_daily_fortune[] = {
    D_255A228, D_255A24C, D_255A278, D_255A2A0,
    D_255A2C4, D_255A2EC, D_255A314, D_255A33C,
    D_255A364, D_255A388, D_255A3B0, D_255A3DC,
    D_255A408, D_255A42C, D_255A454, D_255A47C,
    D_255A4A4, D_255A4CC, D_255A4F4, D_255A51C,
    D_255A53C, D_255A564, D_255A588, D_255A5B0,
    D_255A5D8, D_255A604, D_255A630, D_255A658,
    D_255A684, D_255A6B0, D_255A6D4, D_255A6F8,
    D_255A720, D_255A748, D_255A770, D_255A79C,
    D_255A7C4, D_255A7F0, D_255A818, D_255A83C,
    D_255A860, D_255A888, D_255A8B0, D_255A8DC,
    D_255A904, D_255A92C, D_255A950, D_255A978,
    D_255A9A0, D_255A9C0, D_255A9E4, D_255AA08,
    D_255AA34, D_255AA5C, D_255AA84, D_255AAA0,
    D_255AAC8, D_255AAEC, D_255AB10, D_255AB38,
    D_255AB60, D_255AB84, D_255ABAC, D_255ABCC,
    D_255ABF0, D_255AC18, D_255AC44, D_255AC6C,
    D_255AC94, D_255ACB8, D_255ACDC, D_255ACFC,
    D_255AD24, D_255AD48, D_255AD70, D_255AD94,
    D_255ADBC, D_255ADE4, D_255AE0C, D_255AE34,
    D_255AE5C, D_255AE84, D_255AEA8, D_255AED0,
    D_255AEF4, D_255AF20, D_255AF48, D_255AF74,
    D_255AF98, D_255AFC0, D_255AFE8, D_255B00C,
    D_255B02C, D_255B054, D_255B07C, D_255B0A4,
    D_255B0C8, D_255B0EC, D_255B114, D_255B13C,
    0,
};

/*
 * Text of a DOWNLOADED character (record from dl_chara_record_by_id): group k -> one of 15 0x96-byte
 * strings at fixed offsets in the record (1 = name at +0x42, 0 = +0x6E, 4 = +0x104, ...),
 * the same speech groups the built-in characters have in ROM tables (chara_builtin_text).
 * chara_text(id, group, variant) is the common entry: group 1 = name, 4 = proposal line,
 * 0x66 = fortune results, ...
 */
char *dl_chara_text(u16 id, u8 lang)
{
    struct DlCharaRecord *s;
    char *r;

    s = dl_chara_record_by_id(id);
    r = "0x00";
    switch (lang) {
    case 0:
        r = (char *)s + 0x6e;
        break;
    case 4:
        r = (char *)s + 0x104;
        break;
    case 13:
        r = (char *)s + 0x19a;
        break;
    case 12:
        r = (char *)s + 0x230;
        break;
    case 14:
        r = (char *)s + 0x2c6;
        break;
    case 11:
        r = (char *)s + 0x35c;
        break;
    case 10:
        r = (char *)s + 0x3f2;
        break;
    case 5:
        r = (char *)s + 0x488;
        break;
    case 9:
        r = (char *)s + 0x51e;
        break;
    case 8:
        r = (char *)s + 0x5b4;
        break;
    case 7:
        r = (char *)s + 0x64a;
        break;
    case 6:
        r = (char *)s + 0x6e0;
        break;
    case 2:
        r = (char *)s + 0x776;
        break;
    case 3:
        r = (char *)s + 0x80c;
        break;
    case 1:
        r = (char *)s + 0x42;
        break;
    }
    return r;
}

/* per-file aliases: the tables are defined (as data) elsewhere in the merged file */
struct IdTextEntry {
    u16 chara_id;
    const u8 *text;
};























/*
 * Text of a built-in character: kind b (0..0x69) selects a table of {u16 id, u32 text}
 * (its switch table, D_255B330 in the image), index c for some lists. Returns 0 if none.
 */
const u8 *chara_builtin_text(u16 a, u8 b, u8 c)
{
    const u8 *r = 0;
    const struct IdTextEntry *t = 0;
    const u8 *const *l = 0;
    u8 i;

    switch (b) {
    case 0: t = chara_text_bday; break;
    case 1: t = chara_text_names; break;
    case 2: t = chara_text_talk; break;
    case 3: t = chara_text_play; break;
    case 4: t = chara_text_proposal; break;
    case 5: t = chara_text_farewell; break;
    case 6: t = chara_text_crush; break;
    case 7: t = chara_text_want_eat; break;
    case 8: t = chara_text_want_become; break;
    case 9: t = chara_text_hobby; break;
    case 10: t = chara_text_journey; break;
    case 11: t = chara_text_forgive; break;
    case 12: t = chara_text_play_request; break;
    case 13: t = chara_text_hungry; break;
    case 14: t = chara_text_fashion; break;
    case 0x64: l = txt_general; break;
    case 0x65: l = txt_tips; break;
    case 0x66: l = txt_fortune_results; break;
    case 0x67: l = mail_card_templates; break;
    case 0x68: l = mail_friend_templates; break;
    case 0x69: l = txt_daily_fortune; break;
    }
    if (t != 0) {
        for (; t->chara_id != 0xffff; t++) {
            if (t->chara_id == a) {
                r = t->text;
                break;
            }
        }
    } else if (l != 0) {
        for (i = 0; i < c; i++, l++)
            if (*l == 0)
                break;
        if (i == c)
            r = *l;
        else
            r = 0;
    }
    return r;
}

/*
 * Character text by id and kind (built-in: chara_builtin_text, downloaded: dl_chara_text).
 */
u8 *chara_text(u16 a, u8 b, u8 c)
{
    if (a <= 5000)
        chara_builtin_text(a, b, c);
    else
        dl_chara_text(a, b);
}

/*
 * Character table getters (built-in table chara_table, or the 8 downloadable character slots
 * dl_chara_record(0..7) / dl_chara_record(index + 0x77)).
 *
 * chara_find_adult_by_birthday: the adult (table x00F == 3, or a downloaded one past stage 2) whose
 * x01E/x01F = birthday month / day are (a, b); -1 if none (see chara_birthday_on).
 */
int chara_find_adult_by_birthday(u8 a, u8 b)
{
    int i;
    struct DlCharaRecord *s;

    for (i = 0;; i++) {
        if (chara_table[i].id == 0xffff)
            break;
        if (chara_table[i].stage == 3 && chara_table[i].bday_month == a && chara_table[i].bday_day == b)
            return chara_table[i].id;
    }
    for (i = 0; i <= 7; i++) {
        s = dl_chara_record(i);
        if (s != (struct DlCharaRecord *)-1 && s->stage > 2 && s->bday_month == a && s->bday_day == b)
            return s->id;
    }
    return -1;
}

/*
 * Built-in adult (table stage 3) whose birthday x01E/x01F equals (a, b), else a downloaded
 * character of stage > 2 (asm).
 */
int chara_birthday_match(u8 a, u8 b)
{
    int i;
    struct DlCharaRecord *s;

    if (save.pet.x000 <= 5000) {
        for (i = 0;; i++) {
            if (chara_table[i].id == 0xffff)
                break;
            if (chara_table[i].stage > 2 && chara_table[i].bday_month == a && chara_table[i].bday_day == b
                && save.pet.x000 == chara_table[i].id)
                return chara_table[i].family;
        }
    } else {
        for (i = 0; i <= 7; i++) {
            s = dl_chara_record(i);
            if (s != (struct DlCharaRecord *)-1 && s->stage > 2 && s->bday_month == a && s->bday_day == b
                && save.pet.x000 == s->id)
                return save.pet.x000;
        }
    }
    for (i = 0; i <= 7; i++) {
        s = dl_chara_record(i);
        if (s != (struct DlCharaRecord *)-1 && s->stage > 2 && s->bday_month == a && s->bday_day == b)
            return s->id;
    }
    for (i = 0;; i++) {
        if (chara_table[i].id == 0xffff)
            break;
        if (chara_table[i].stage == 3 && chara_table[i].bday_month == a && chara_table[i].bday_day == b)
            return chara_table[i].id;
    }
    return -1;
}

/*
 * Character id -> birthday month, table field x01E (downloaded: x01C).
 */
u8 chara_bday_month(u16 id)
{
    struct DlCharaRecord *p;
    int i;

    if (save.pet.x000 <= 5000) {
        for (i = 0; ; i++) {
            if (chara_table[i].id == id)
                return chara_table[i].bday_month;
            if (chara_table[i].id == 0xffff)
                break;
        }
    } else {
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->id == id)
                return p->bday_month;
        }
    }
    return 0;
}

/*
 * Character id -> birthday day, table field x01F (downloaded: x01D).
 */
u8 chara_bday_day(u16 id)
{
    struct DlCharaRecord *p;
    int i;

    if (id <= 5000) {
        for (i = 0; ; i++) {
            if (chara_table[i].id == id)
                return chara_table[i].bday_day;
            if (chara_table[i].id == 0xffff)
                break;
        }
    } else {
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->id == id)
                return p->bday_day;
        }
    }
    return 0;
}

/*
 * Character id -> table field x02C (downloaded: x008).
 */
u8 chara_club_class(u16 id)
{
    struct DlCharaRecord *p;
    int i;

    if (id <= 5000) {
        for (i = 0; ; i++) {
            if (chara_table[i].id == id)
                return chara_table[i].club_class;
            if (chara_table[i].id == 0xffff)
                break;
        }
    } else {
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->id == id)
                return p->x008;
        }
    }
    return 0;
}

/*
 * Character id -> its 0..31 code, table field x020 (downloaded: x01E).
 */
u8 chara_class_field(u16 id)
{
    struct DlCharaRecord *p;
    int i;

    if (id <= 5000) {
        for (i = 0; ; i++) {
            if (chara_table[i].id == id)
                return chara_table[i].class_code;
            if (chara_table[i].id == 0xffff)
                break;
        }
        return 0;
    } else {
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->id == id)
                return p->x01e;
        }
    }
}

/*
 * Character id -> index in the character table (or 5001 + slot for downloaded
 * characters; 9999 is a special table entry), -1 if unknown.
 */
int chara_index_by_id(u16 id)
{
    struct DlCharaRecord *p;
    int i;

    if (id <= 5000 || id == 9999) {
        for (i = 0; ; i++) {
            if (chara_table[i].id == id)
                return i;
            if (chara_table[i].id == 0xffff)
                break;
        }
    } else {
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->id == id)
                return i + 5001;
        }
    }
    return -1;
}

/*
 * Sprite set id (field x000) -> character table index (5001 + slot for downloaded
 * characters), 0 if unknown.
 */
int chara_index_by_sprite_set(u16 id)
{
    struct DlCharaRecord *p;
    int i;

    if (save.pet.x000 <= 5000) {
        for (i = 0; ; i++) {
            if (chara_table[i].sprite_set == id)
                return i;
            if (chara_table[i].sprite_set == 0xffff)
                break;
        }
    } else {
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->sprite_set == id)
                return i + 5001;
        }
    }
    return 0;
}

/*
 * Character id -> table field x004 (downloaded: x004).
 */
u8 func_24085C6(u16 id)
{
    int k = chara_index_by_id(id);

    if (id <= 5000)
        return chara_table[k].x004;
    return dl_chara_record(k + 0x77)->album_key;
}

/*
 * Character id -> gender byte (x008 / downloaded x007, see pet_get_gender).
 */
int chara_gender(u16 id)
{
    int k = chara_index_by_id(id);

    if (id <= 5000)
        return chara_table[k].gender;
    return dl_chara_record(k + 0x77)->gender;
}

/*
 * TAMAGO file: type byte +0x4E.
 */
int item_type(struct TamagoFile *p)
{
    return p->x04e;
}

/*
 * TAMAGO file: category +0x4F.
 */
int item_category(struct TamagoFile *p)
{
    return p->x04f;
}

/*
 * TAMAGO file: sub-kind +0x51.
 */
int item_sub_kind(struct TamagoFile *p)
{
    return p->sub_kind;
}

/*
 * TAMAGO file: 16-bit content id (+0x5A big endian).
 */
int item_id(struct TamagoFile *p)
{
    return p->x05b | (p->x05a << 8);
}

/*
 * Table field x009 of the current (built-in) character.
 */
u8 func_2408658(void)
{
    if (save.pet.x000 <= 5000)
        return chara_table[save.pet.chara_row].x009;
    dl_chara_record((u8)save.pet.chara_row + 0x77);
    return 0;
}

/*
 * Reset the five growth stats pet.traits[0..4] (sent in the exchange header at 0x7C..) and
 * pet.dominant_trait.
 */
void pet_clear_traits(int unused)
{
    u8 buf[8];

    memset(save.pet.traits, 0, 5);
    save.pet.dominant_trait = 0;
}

/*
 * Count up (cap 20) one of the four adult activity counters pet.activity0_count..D_00012C4
 * (stage > 3).
 */
void pet_count_activity(u8 k)
{
    if (save.pet.life_stage > 3) {
        switch (k) {
        case 0:
            if (save.pet.activity0_count <= 0x13)
                save.pet.activity0_count++;
            break;
        case 1:
            if (save.pet.shop_count <= 0x13)
                save.pet.shop_count++;
            break;
        case 2:
            if (save.pet.tamamori_count <= 0x13)
                save.pet.tamamori_count++;
            break;
        case 3:
            if (save.pet.restaurant_count <= 0x13)
                save.pet.restaurant_count++;
            break;
        }
    }
}

/*
 * Clear the four counters pet.activity0_count..D_00012C4.
 */
void pet_clear_activities(void)
{
    save.pet.activity0_count = 0;
    save.pet.shop_count = 0;
    save.pet.tamamori_count = 0;
    save.pet.restaurant_count = 0;
}

void func_2408726(void)
{
}

/*
 * Returns 0 (stub).
 */
int func_2408728(void)
{
    return 0;
}

void func_240872C(void)
{
}

void func_240872E(void)
{
}

/*
 * Returns 0 (stub).
 */
int func_2408730(void)
{
    return 0;
}

/*
 * Character id -> table field x004 (downloaded record x004); unknown -> id.
 */
u16 chara_album_key(int id)
{
    struct DlCharaRecord *p;
    int i;

    if (id <= 5000) {
        for (i = 0; ; i++) {
            if (chara_table[i].id == id)
                return chara_table[i].x004;
            if (chara_table[i].id == 0xffff)
                break;
        }
    } else {
        for (i = 0; i <= 7; i++) {
            p = dl_chara_record(i);
            if (p != (struct DlCharaRecord *)-1 && p->id == id)
                return p->album_key;
        }
    }
    return id;
}

/*
 * Find the character of stage c whose x01E/x01F codes are (a, b) (downloaded characters
 * only if it is the current pet); -1 if none.
 */
int chara_find_by_codes(u8 a, u8 b, u8 c)
{
    int i;
    struct DlCharaRecord *s;

    for (i = 0;; i++) {
        if (chara_table[i].id == 0xffff)
            break;
        if (chara_table[i].stage == c && chara_table[i].bday_month == a && chara_table[i].bday_day == b)
            return chara_table[i].id;
    }
    for (i = 0; i <= 7; i++) {
        s = dl_chara_record(i);
        if (s != (struct DlCharaRecord *)-1 && s->stage == c && s->bday_month == a && s->bday_day == b)
            return s->id == save.pet.x000 ? s->id : -1;
    }
    return -1;
}

/*
 * Is the family of character id in the album (sector 0x37 records)? Returns the family
 * key or 0.
 */
int chara_family_in_album(u16 id)
{
    int k;
    int i;
    u8 *p;

    if (id >= 16 && id <= 1999) {
        k = id;
    } else if (id >= 2001 && id <= 3999) {
        for (i = 0;; i++) {
            if (chara_table[i].id == id) {
                k = chara_table[i].family;
                break;
            }
            if (chara_table[i].id == 0xffff)
                return 0;
        }
    } else if (id >= 5001 && id <= 8999) {
        k = id;
    } else {
        return 0;
    }
    p = flash_sector_addr(0x37);
    for (i = 0; i <= 0x4f; i++) {
        if (*(u16 *)p == k)
            return k;
        p += 0x1000;
    }
    return 0;
}

/*
 * The RAM variables this file defines.
 */
struct VisitPartner visit_partner;
volatile u8 pet_life_paused;
u8 save_request;
u8 pet_request_bits;
