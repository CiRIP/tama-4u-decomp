/*
 * Special-form evolution: the condition checks, slot handling and the choice of the next
 * character, and the evolve / revert actions.
 *
 * Code 24F6792..24F70EC, .rodata from 2535F10.
 */
#include "proto/evo/evo_special.h"
#include "proto/pet/pet_core.h"
#include "proto/chara/chara_draw.h"
#include "proto/evo/evo_counters.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/comm/comm_outcome.h"

/*
 * F4 = the special-form routes of the twelve adults 0x10..0x15 / 0x3E9..0x3EE (the special
 * forms they turn into, and back). Each has conditions on the counters kept in
 * pet.evo_counters[0]..D_0001394, pet.park_meeting..D_0001390 (snacks, goods used, games played
 * 1394, the baths 138E (evo_count_bath), the park-square meeting 138D (pet.park_meeting),
 * meeting おれねっち 1390 (evo_flag_met_chara)), the weight pet.weight (0..99, floored at
 * the character's base value), the happiness x00c, the stats pet.traits.., owned items
 * (item_owned_slot_by_id finds the accessory / clothes slot holding item id) and the club mastery
 * x217[]. Return code 14 = "wear the item" variant (the item index goes to pet.worn_accessory /
 * pet.worn_clothes). (MED)
 *
 * evo_reset_special_counters: reset the growth counters pet.evo_bath_count / pet.met_orenetchi / pet.park_meeting and the 10
 * bytes pet.evo_counters.
 */
void evo_reset_special_counters(void)
{
    int i;

    save.pet.evo_bath_count = 0;
    save.pet.met_orenetchi = 0;
    save.pet.park_meeting = 0;
    for (i = 0; i <= 9; i++)
        save.pet.evo_counters[i] = 0;
}

/*
 * Evolution condition: weight > 89 (fat).
 */
u8 evo_cond_fat(void)
{
    u8 r;

    if (save.pet.weight > 0x59)
        r = 1;
    else
        r = 0;
    return r;
}

/*
 * Evolution condition: weight <= 25 (thin) -> 2.
 */
u8 evo_cond_thin(void)
{
    return save.pet.weight <= 0x19 ? 2 : 0;
}

/*
 * Evolution condition: 30 uses counted in pet.evo_counters[0] -> 3.
 */
u8 evo_cond_snacks(void)
{
    if (save.pet.evo_counters[0] > 29) {
        save.pet.evo_counters[0] = 0;
        return 3;
    }
    return 0;
}

/*
 * Evolution condition: flag pet.park_meeting -> 5.
 */
u8 evo_cond_park_meeting(void)
{
    if (save.pet.park_meeting != 0) {
        save.pet.park_meeting = 0;
        return 5;
    }
    return 0;
}

/*
 * Evolution condition: flag pet.park_meeting and pet.evo_counters[3] >= 20 -> 7.
 */
u8 evo_cond_games20(void)
{
    if (save.pet.park_meeting != 0) {
        if (save.pet.evo_counters[3] > 19) {
            save.pet.park_meeting = 0;
            save.pet.evo_counters[3] = 0;
            return 7;
        }
    }
    return 0;
}

/*
 * Evolution condition: flag pet.park_meeting and pet.evo_counters[2] >= 10 -> 6.
 */
u8 evo_cond_item6(void)
{
    if (save.pet.park_meeting != 0) {
        if (save.pet.evo_counters[2] > 9) {
            save.pet.park_meeting = 0;
            save.pet.evo_counters[2] = 0;
            return 6;
        }
    }
    return 0;
}

/*
 * Evolution condition: pet.evo_counters[1] >= 3 -> 4.
 */
u8 evo_cond_item15(void)
{
    if (save.pet.evo_counters[1] > 2) {
        save.pet.evo_counters[1] = 0;
        return 4;
    }
    return 0;
}

/*
 * Slot index of the owned accessory (kind 2, list A) / clothes (kind 3, list B) whose item
 * id is id, 0 when not owned.
 */
int item_owned_slot_by_id(u8 kind, u16 id)
{
    int n, i;
    u8 cur;
    u8 *p;

    switch (kind) {
    case 2:
        n = item_owned_accessory_count();
        if (n > 0) {
            cur = item_first_owned_accessory();
            for (i = 0; i < n; i++) {
                p = (u8 *)item_record_by_index(D_2535F50[cur]);
                if (*p != 0xff && item_id((struct TamagoFile *)p) == id)
                    return cur;
                cur = item_next_owned_accessory_wrap(cur);
            }
            return 0;
        }
        break;
    case 3:
        n = item_owned_clothes_count();
        if (n > 0) {
            cur = item_first_owned_clothes();
            for (i = 0; i < n; i++) {
                p = (u8 *)item_record_by_index(D_2535F68[cur]);
                if (*p != 0xff && item_id((struct TamagoFile *)p) == id)
                    return cur;
                cur = item_next_owned_clothes_wrap(cur);
            }
        }
        break;
    default:
        return 0;
    }
    return 0;
}

/*
 * Evolution condition 14/8: pet.evo_bath_count >= 5 (with accessory 20 worn in slot A for k == 1).
 */
u8 evo_cond_baths(u8 k)
{
    u8 t = 0;

    if (k == 1) {
        t = item_owned_slot_by_id(2, 20);
        if (t == 0)
            return t;
    }
    if (save.pet.evo_bath_count > 4) {
        if (k == 1) {
            save.pet.worn_accessory = t;
            return 14;
        } else {
            save.pet.evo_bath_count = 0;
            return 8;
        }
    }
    return 0;
}

/*
 * Evolution condition: pet.met_orenetchi -> 9.
 */
u8 evo_cond_met(void)
{
    if (save.pet.met_orenetchi != 0) {
        save.pet.met_orenetchi = 0;
        return 9;
    }
    return 0;
}

/*
 * Evolution condition 10/14 (album sector 0x76 counter and item counter pet.traits[0]).
 */
u8 evo_cond_trait0(u8 k)
{
    u8 t = 0;

    if (k == 1) {
        t = item_owned_slot_by_id(2, 5);
        if (t == 0)
            return t;
    }
    if (flash_sector_addr(0x76)[0xb8] > 2 && save.pet.traits[0] > 9) {
        if (k != 1)
            return 10;
        save.pet.worn_accessory = t;
        return 14;
    }
    return 0;
}

/*
 * Evolution condition 11/14 (pet.traits[1]).
 */
u8 evo_cond_trait1(u8 k)
{
    u8 t;

    if (k == 1) {
        t = item_owned_slot_by_id(3, 12);
        if (t == 0)
            return t;
    }
    if (flash_sector_addr(0x76)[0xb9] > 2 && save.pet.traits[1] > 9) {
        if (k != 1)
            return 11;
        save.pet.worn_clothes = t;
        return 14;
    }
    return 0;
}

/*
 * Evolution condition: happiness <= 15 -> 12.
 */
u8 evo_cond_unhappy(void)
{
    return save.pet.happiness <= 0xf ? 0xc : 0;
}

/*
 * Evolution condition 13/14: happiness > 79 and the 5 entries x217 set.
 */
u8 evo_cond_happy_clubs(u8 k)
{
    u8 t = 0;
    int i;

    if (k == 1) {
        t = item_owned_slot_by_id(2, 0x11);
        if (t == 0)
            return t;
    }
    if (save.pet.happiness <= 0x4f)
        goto zero;
    for (i = 0; i <= 4; i++)
        if (save.pet.club_mastered[i] == 0)
            return save.pet.club_mastered[i];
    if (k != 1)
        return 13;
    save.pet.worn_accessory = t;
    return 14;
zero:
    return 0;
}

/*
 * Is evolution slot k (pet.special_evo_slots[k]) unused?
 */
u8 evo_slot_unused(u8 k)
{
    u8 x = save.pet.special_evo_slots[k];

    return x == 0;
}

/*
 * Clear the evolution slots x204[4] and the worn accessories.
 */
void evo_clear_slots(void)
{
    int i;

    for (i = 0; i <= 3; i++)
        save.pet.special_evo_slots[i] = 0;
    save.pet.worn_clothes = 0;
    save.pet.worn_accessory = 0;
}

/*
 * Check special evolution k (0..3) and remember its accessory (pet.special_evo_item/D_00013A0).
 */
u8 evo_check_special(u8 k)
{
    u32 v;
    u8 r;

    switch (k) {
    case 0:
        r = evo_cond_baths(1);
        if (r == 14) {
            save.pet.special_evo_item = v;
            save.pet.special_evo_slots[0] = 1;
            return 1;
        }
        break;
    case 1:
        r = evo_cond_trait0(1);
        if (r == 14) {
            save.pet.special_evo_slots[1] = 1;
            return 1;
        }
        break;
    case 2:
        r = evo_cond_trait1(1);
        if (r == 14) {
            save.pet.special_evo_slots[2] = 1;
            return 1;
        }
        break;
    case 3:
        r = evo_cond_happy_clubs(1);
        if (r == 14) {
            save.pet.special_evo_slots[3] = 1;
            return 1;
        }
        break;
    default:
        r = 0;
        break;
    }
    if (r != 0)
        save_after_sound();
    return 0;
}

/*
 * Check the special evolutions selected by mask (bits 0..3).
 */
void evo_check_special_mask(u8 x)
{
    if ((x & 1) && evo_slot_unused(0) && evo_check_special(0))
        return;
    if ((x & 2) && evo_slot_unused(1) && evo_check_special(1))
        return;
    if ((x & 4) && evo_slot_unused(2) && evo_check_special(2))
        return;
    if ((x & 8) && evo_slot_unused(3))
        evo_check_special(3);
}

/*
 * Choose the next character when growing up: per current character id pet.x000
 * test the conditions above and call pet_become_character(new id) (= become that character) for
 * the first that holds, e.g. 0x10 (まめっち line): 2001 when the home-cooking count
 * pet.traits[0] > 9 with the shop record byte 0xB8 > 2, 2002 when happiness <= 15; 0x11:
 * 2003 weight > 89 / 2004 weight <= 25; 0x12: 2005 five meals of kind pet.evo_bath_count / 2006 all
 * five clubs; ... (2001..2010 from 0x10..0x15, 3001..3010 from 0x3E9..0x3EE; the
 * counter-based ones also set pet.evo_pending_days = 2 = pending). At life stage 5 and in the
 * fall-through cases only the four item-based variants are checked (evo_check_special_mask bits).
 */
u8 evo_choose_next(void)
{
    u8 t[4];
    u8 r;

    if (save.pet.life_stage == 5) {
        evo_check_special_mask(0xf);
        return 0;
    }
    switch (save.pet.x000) {
    case 0x10:
        r = evo_cond_trait0(0);
        if (r == 10) { pet_become_character(2001); break; }
        r = evo_cond_unhappy();
        if (r == 12) { pet_become_character(2002); break; }
        evo_check_special_mask(0xd);
        break;
    case 0x11:
        r = evo_cond_fat();
        if (r == 1) { pet_become_character(2003); break; }
        r = evo_cond_thin();
        if (r == 2) { pet_become_character(2004); break; }
        evo_check_special_mask(0xf);
        break;
    case 0x12:
        r = evo_cond_baths(0);
        if (r == 8) { save.pet.evo_pending_days = 2; pet_become_character(2005); break; }
        r = evo_cond_happy_clubs(0);
        if (r == 13) { pet_become_character(2006); break; }
        evo_check_special_mask(6);
        break;
    case 0x13:
        r = evo_cond_games20();
        if (r == 7) { save.pet.evo_pending_days = 2; pet_become_character(2008); break; }
        r = evo_cond_met();
        if (r == 9) { save.pet.evo_pending_days = 2; pet_become_character(2007); break; }
        evo_check_special_mask(0xf);
        break;
    case 0x14:
        r = evo_cond_happy_clubs(0);
        if (r == 13) { pet_become_character(2009); break; }
        evo_check_special_mask(7);
        break;
    case 0x15:
        r = evo_cond_item15();
        if (r == 4) { save.pet.evo_pending_days = 2; pet_become_character(2010); break; }
        evo_check_special_mask(0xf);
        break;
    case 0x3e9:
        r = evo_cond_trait1(0);
        if (r == 11) { pet_become_character(3001); break; }
        r = evo_cond_unhappy();
        if (r == 12) { pet_become_character(3002); break; }
        evo_check_special_mask(0xb);
        break;
    case 0x3ea:
        r = evo_cond_park_meeting();
        if (r == 5) { save.pet.evo_pending_days = 2; pet_become_character(3003); break; }
        r = evo_cond_trait1(0);
        if (r == 11) { pet_become_character(3010); break; }
        evo_check_special_mask(0xb);
        break;
    case 0x3eb:
        r = evo_cond_fat();
        if (r == 1) { pet_become_character(3005); break; }
        r = evo_cond_happy_clubs(0);
        if (r == 13) { pet_become_character(3004); break; }
        evo_check_special_mask(7);
        break;
    case 0x3ec:
        r = evo_cond_item6();
        if (r == 6) { save.pet.evo_pending_days = 2; pet_become_character(3006); break; }
        r = evo_cond_happy_clubs(0);
        if (r == 13) { pet_become_character(3009); break; }
        evo_check_special_mask(7);
        break;
    case 0x3ed:
        r = evo_cond_thin();
        if (r == 2) { pet_become_character(3007); break; }
        evo_check_special_mask(0xf);
        break;
    case 0x3ee:
        r = evo_cond_snacks();
        if (r == 3) { save.pet.evo_pending_days = 2; pet_become_character(3008); break; }
        evo_check_special_mask(0xf);
        break;
    default:
        r = 0;
        break;
    }
    return r;
}


/*
 * Re-check the condition of the route pet.evo_route that produced the current special form
 * (1 / 2 weight thresholds 0x28, 10 / 11 sector 0x76 bytes 0xB8 / 0xB9, 12 / 13 happiness,
 * 3..9 the counters). Returns 0 while it still holds (keep the form), else non-zero: the
 * caller evo_evolve_now then reverts the form (evo_revert_special_form). A counter route
 * that has been reached again resets the counters with evo_reset_special_counters and sets
 * pet.evo_pending_days = 2 (another two days).
 */
u8 evo_route_lost(void)
{
    switch (save.pet.evo_route) {
    case 0:
        return 0;
    case 1:
        if (save.pet.weight <= 0x28)
            return 1;
        return 0;
    case 2:
        if (save.pet.weight > 0x27)
            return 2;
        return 0;
    case 10:
        if (flash_sector_addr(0x76)[0xb8] <= 2)
            return 4;
        return 0;
    case 11:
        if (flash_sector_addr(0x76)[0xb9] <= 2)
            return 5;
        return 0;
    case 12:
        if (save.pet.happiness > 0x1d)
            return 6;
        return 0;
    case 13:
        if (save.pet.happiness <= 0x3c)
            return 7;
        return 0;
    case 3:
        if (save.pet.evo_pending_days == 0) {
            if (save.pet.evo_counters[0] <= 0x1d)
                return 3;
            evo_reset_special_counters();
            save.pet.evo_pending_days = 2;
        }
        return 0;
    case 4:
        if (save.pet.evo_pending_days == 0) {
            if (save.pet.evo_counters[1] <= 2)
                return 3;
            evo_reset_special_counters();
            save.pet.evo_pending_days = 2;
        }
        return 0;
    case 5:
        if (save.pet.evo_pending_days == 0) {
            if (save.pet.park_meeting == 0)
                return 3;
            evo_reset_special_counters();
            save.pet.evo_pending_days = 2;
        }
        return 0;
    case 6:
        if (save.pet.evo_pending_days == 0) {
            if (save.pet.park_meeting == 0)
                return 3;
            if (save.pet.evo_counters[2] <= 9)
                return 3;
            evo_reset_special_counters();
            save.pet.evo_pending_days = 2;
        }
        return 0;
    case 7:
        if (save.pet.evo_pending_days == 0) {
            if (save.pet.park_meeting == 0 || save.pet.evo_counters[3] <= 0x13)
                return 3;
            evo_reset_special_counters();
            save.pet.evo_pending_days = 2;
        }
        return 0;
    case 8:
        if (save.pet.evo_pending_days == 0) {
            if (save.pet.evo_bath_count <= 4)
                return 3;
            evo_reset_special_counters();
            save.pet.evo_pending_days = 2;
        }
        return 0;
    case 9:
        if (save.pet.evo_pending_days == 0) {
            if (save.pet.met_orenetchi == 0)
                return 3;
            evo_reset_special_counters();
            save.pet.evo_pending_days = 2;
        }
        return 0;
    }
    return 0;
}

/*
 * Turn a special form (2001..2010 / 3001..3010, stage 4) back into its base adult (stage 3:
 * 2001 / 2002 -> 0x10 まめっち, ...) and reset the special-form counters; called when
 * evo_route_lost reports that the form's condition no longer holds. Other ids: no effect.
 */
void evo_revert_special_form(void)
{
    switch (save.pet.x000) {
    case 2001:
    case 2002:
        pet_become_character(0x10);
        break;
    case 2003:
    case 2004:
        pet_become_character(0x11);
        break;
    case 2005:
    case 2006:
        pet_become_character(0x12);
        break;
    case 2007:
    case 2008:
        pet_become_character(0x13);
        break;
    case 2009:
        pet_become_character(0x14);
        break;
    case 2010:
        pet_become_character(0x15);
        break;
    case 3001:
    case 3002:
        pet_become_character(1001);
        break;
    case 3003:
    case 3010:
        pet_become_character(1002);
        break;
    case 3004:
    case 3005:
        pet_become_character(1003);
        break;
    case 3006:
    case 3009:
        pet_become_character(1004);
        break;
    case 3007:
        pet_become_character(1005);
        break;
    case 3008:
        pet_become_character(1006);
        break;
    default:
        return;
    }
    evo_reset_counters();
}

/*
 * Evolve now for stages 3..5: evo_choose_next, then reset the counters and remember the
 * route (pet.evo_route).
 */
void evo_evolve_now(void)
{
    u8 r;
    u8 s = save.pet.life_stage;
    if (s < 3 || s > 5)
        return;
    switch (s) {
    case 3:
    case 5:
        r = evo_choose_next();
        if (r == 0)
            return;
        evo_reset_counters();
        evo_clear_slots();
        save.pet.evo_route = r;
        if (save.pet.special_id == 0)
            save.pet.special_id = save.pet.x000;
        break;
    case 4:
        r = evo_route_lost();
        if (r == 0)
            return;
        evo_revert_special_form();
        save.pet.evo_route = r;
        break;
    default:
        return;
    }
    save_after_sound();
}

/*
 * Run evo_evolve_now twice.
 */
void evo_evolve_now_twice(void)
{
    evo_evolve_now();
    evo_evolve_now();
}

/*
 * Stage-change resets (adult, stage 3: the special-form counters; special form, stage 4:
 * per-character presets, e.g. weight and route of 2003 / 2004).
 */
void evo_stage_change_reset(void)
{
    switch (save.pet.life_stage) {
    case 3:
        evo_reset_counters();
        save.pet.evo_pending_days = 0;
        break;
    case 4:
        switch (save.pet.x000) {
        case 2001:
        case 2002:
            break;
        case 2003:
            save.pet.weight = 90;
            save.pet.evo_route = 1;
        case 2004:
            save.pet.weight = 25;
            save.pet.evo_route = 2;
            break;
        case 3000:
            break;
        case 4000:
            break;
        case 5000:
            break;
        }
        break;
    }
}
