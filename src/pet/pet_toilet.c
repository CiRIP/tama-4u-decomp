/*
 * The toilet care action (pet_toilet) and the falling-object animation (anim_falling_object).
 *
 * Code 241B91C..241B9E2, no .rodata.
 */
#include "proto/pet/pet_toilet.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/room/room_draw.h"
#include "proto/anim/anim_scripts.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/pet/pet_care_room.h"
#include "proto/pet/pet_care.h"
#include "flash.h"


/*
 * Toilet: the トイレ entry of the care menu (case 0; a trained pet also goes by itself,
 * pet_care_event_dispatch). With the toilet urge pet.toilet_timer pending, the pet uses the
 * toilet (anim_toilet), pet.toilet_count (max 99), a child's toilet training
 * pet.training[0], the praise window opens (child or trained pet); otherwise the poops
 * on the floor are flushed away (falling-picture animation, pet.poop_count = 0), or
 * anim_toilet_nothing when there are none.
 */
void pet_toilet(void)
{
    if (save.pet.toilet_timer != 0) {
        save.pet.toilet_timer = 0;
        pet_toilet_expired = 0;
        anim_toilet();
        if (save.pet.toilet_count <= 0x62)
            save.pet.toilet_count++;
        if (save.pet.life_stage == 2) {
            if (save.pet.training[0] != 0xff)
                save.pet.training[0]++;
            save.pet.discipline_flags |= 2;
            save.pet.praise_timer = 300;
            pet_praise_expired = 0;
        } else if (save.pet.training[0] == 0xff) {
            save.pet.discipline_flags |= 2;
            save.pet.praise_timer = 300;
            pet_praise_expired = 0;
        } else
            anim_happy();
        pet_add_happiness(1);
    } else {
        if (save.pet.poop_count != 0) {
            anim_falling_object();
            pet_add_happiness(1);
            anim_happy();
        } else
            anim_toilet_nothing();
        save.pet.poop_count = 0;
        pet_poops_drawn = 0;
        pet_poop_overflow = 0;
    }
    save_game();
}

/*
 * Falling-object animation (picture 0x214FD64 falls in 10 steps).
 */
void anim_falling_object(void)
{
    int x, y, i;
    u8 c;

    x = 0x60;
    y = -30;
    c = room_poop_objects[3];
    gfx_object_set_image(c, 0xba, 0, D_214FD64);
    ui_draw_icon_bars();
    for (i = 0; i <= 9; i++, y += 15) {
        gfx_object_move_dirty(c, x, y);
        switch (i) {
        case 3:
            gfx_object_set_image(room_poop_objects[2], 0, 0, 0);
            break;
        case 4:
            gfx_object_set_image(room_poop_objects[1], 0, 0, 0);
            break;
        case 5:
            gfx_object_set_image(room_poop_objects[0], 0, 0, 0);
            break;
        }
        anim_busy = 1;
        anim_frame_wait(0xfa, 0);
    }
    anim_busy = 0;
}

/*
 * The RAM variables this file defines.
 */
u8 pet_toilet_expired;
