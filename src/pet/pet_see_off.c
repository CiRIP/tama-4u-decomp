/*
 * The ten-day see-off event (pet_see_off): the pet's departure wish, the see-off scenes, the
 * life story and the next generation's egg.
 *
 * Open: the end of ui_yes_no or the start of spot_reader.
 *
 * Code 2513FC6..2513FC6, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/pet/pet_see_off.h"
#include "proto/save/save_flash.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/town/town_places.h"
#include "proto/anim/anim_scripts.h"
#include "proto/ui/ui_message.h"
#include "proto/chara/chara_select.h"
#include "proto/ui/ui_yes_no.h"
#include "proto/comm/comm_outcome.h"

/*
 * Ten-day see-off event (pet.ten_day_event_due, raised every 14400 minutes of an adult's
 * life): the pet's departure wish (character text 10), "みおくる?" (ui_yes_no_question),
 * then the see-off scenes, the life story (pet_life_story) and the next generation's egg.
 */
void pet_see_off(void)
{
    ui_show_message(4, 0, chara_text(save.pet.x000, 10, 0));
    if (ui_yes_no_question(2, 1) != 0)
        return;
    anim_seeoff_start();
    pet_life_story();
    anim_seeoff_goodbye();
    ui_show_message(8, 0, chara_text(save.pet.x000, 5, 0));
    anim_seeoff_leave();
    pet_pick_variant_flag();
    pet_start_next_generation();
    rtc_seconds = 0;
    ui_main_icon = 0;
    anim_new_egg();
    rtc_seconds = 0;
}
