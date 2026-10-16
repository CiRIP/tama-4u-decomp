/*
 * Counters and flags for the special-form evolution (item uses, games, baths, meetings, daily
 * countdown) and their reset.
 *
 * Code 24F65A2..24F674C, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/evo/evo_counters.h"
#include "proto/save/save_flash.h"
#include "proto/pet/pet_core.h"
#include "proto/evo/evo_special.h"
#include "proto/comm/comm_menu.h"
#include "proto/comm/comm_outcome.h"

/*
 * Special-form counters (stages 3..5: adult, special form, downloaded) when an item p of
 * category 0x81 is used:
 * sub-type 1/2 counts pet.evo_counters[0] (max 30), sub-type 4 ... (evolution conditions, see
 * evo_choose_next).
 */
void evo_count_item_use(struct TamagoFile *p)
{
    int k = save.pet.life_stage;
    int ok = k >= 3 && k <= 5;

    if (ok && item_type(p) == 0x81) {
        switch (item_category(p)) {
        case 1:
            if (item_sub_kind(p) == 2 && save.pet.evo_counters[0] < 30) {
                save.pet.evo_counters[0]++;
                save_after_sound();
            }
            break;
        case 4:
            if (item_sub_kind(p) == 1) {
                if (item_id(p) == 15) {
                    if (save.pet.evo_counters[1] < 3) {
                        save.pet.evo_counters[1]++;
                        save_after_sound();
                    }
                } else if (item_id(p) == 6 && save.pet.evo_counters[2] < 10) {
                    save.pet.evo_counters[2]++;
                    save_after_sound();
                }
            }
            break;
        }
    }
}

/*
 * Special-form counter pet.evo_counters[3] + 1 (max 20) for stages 3..5 (games played).
 */
void evo_count_game(void)
{
    int k = save.pet.life_stage;
    int r = k >= 3 && k <= 5;

    if (r && save.pet.evo_counters[3] <= 0x13)
        save.pet.evo_counters[3]++;
}

/*
 * Special-form counter pet.evo_bath_count + 1 (max 5) for stages 3..5: the baths (pet_bath),
 * the condition of route 8.
 */
void evo_count_bath(void)
{
    int k = save.pet.life_stage;

    if (k >= 3 && k <= 5) {
        save.pet.evo_bath_count++;
        if (save.pet.evo_bath_count > 5)
            save.pet.evo_bath_count = 5;
        save_after_sound();
    }
}

/*
 * Special-form flag pet.met_orenetchi = 1 when a communication partner is character 20
 * (おれねっち) or 2009 (おれおとなっち) (stages 3..5): the condition of route 9.
 */
void evo_flag_met_chara(u16 id)
{
    int k;

    if (id == 20 || id == 2009) {
        k = save.pet.life_stage;
        if (k >= 3 && k <= 5) {
            save.pet.met_orenetchi++;
            if (save.pet.met_orenetchi > 1)
                save.pet.met_orenetchi = 1;
        }
    }
}

/*
 * Special-form flag pet.park_meeting = 1 (and save) when the pet is an adult (stage 3) or one
 * of the forms 2008 / 3003 / 3006 that depend on it; called by the park-square meeting with
 * the pet's contact character (town_park_meet_contact). Condition of routes 5 / 6 / 7.
 */
void evo_note_park_meeting(void)
{
    switch (save.pet.x000) {
    case 2008:
    case 3003:
    case 3006:
        break;
    default:
        if (save.pet.life_stage != 3)
            return;
        break;
    }
    save.pet.park_meeting = 1;
    save_after_sound();
}

/*
 * Daily countdown (at 7:00) pet.evo_pending_days - 1 for a special form (stage 4): the days left
 * before a counter route's condition is re-checked (evo_route_lost).
 */
void evo_daily_countdown(void)
{
    if (save.pet.life_stage == 4 && save.pet.evo_pending_days != 0)
        save.pet.evo_pending_days--;
}

/*
 * Reset the growth counters (pet.evo_bath_count, pet.met_orenetchi, pet.park_meeting, pet.x1F5[10], items
 * pet.traits...) and pet_clear_growth_items.
 */
void evo_reset_counters(void)
{
    int i;

    save.pet.evo_bath_count = 0;
    save.pet.met_orenetchi = 0;
    save.pet.park_meeting = 0;
    for (i = 0; i <= 9; i++)
        save.pet.evo_counters[i] = 0;
    pet_clear_traits(1);
    pet_clear_growth_items();
}

/*
 * Like evo_reset_counters, also pet.evo_pending_days.
 */
void evo_reset_counters_all(void)
{
    int i;

    save.pet.evo_pending_days = 0;
    save.pet.evo_bath_count = 0;
    save.pet.met_orenetchi = 0;
    save.pet.park_meeting = 0;
    for (i = 0; i <= 9; i++)
        save.pet.evo_counters[i] = 0;
    pet_clear_traits(1);
    pet_clear_growth_items();
}
