/*
 * A visitor who brings a gift (and an empty hook).
 *
 * Code 2508D66..2508D68, .rodata from 254182C. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/visit/visit_gift.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/room/room_window.h"
#include "proto/anim/anim_scripts.h"
#include "proto/ui/ui_message.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"

void func_2508D66(void)
{
}

/*
 * Visitor a gives a gift: message text b with animation, +1 happiness, save.
 */
void visit_gift(int a, u8 b)
{
    ui_new_screen();
    chara_select_other_by_set(a);
    room_visitor_scene = 1;
    anim_visitor_gift();
    ui_show_message(6, 0, chara_text(0, 100, b));
    pet_add_happiness(1);
    save_game();
    chara_select_other_by_set(a);
    anim_visitor_gift_end();
    room_visitor_scene = 0;
}
