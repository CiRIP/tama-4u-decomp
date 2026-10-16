/*
 * The bath care action (bath scene, bath training and praise window), the bedroom background of
 * the sleeping scene and the Christmas animation wrapper.
 *
 * Code 240D976..240DB12, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/pet/pet_bath.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/room/room_window.h"
#include "proto/shop/shop_reform.h"
#include "proto/anim/anim_scripts.h"
#include "proto/evo/evo_counters.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/pet/pet_care.h"
#include "flash.h"

/*
 * Bath: the おふろ・はみがき (bath / tooth brushing) entry of the care menu (case 1; a
 * trained pet also does it by itself, pet_care_event_dispatch): bath scene anim_bath, the
 * bath count of the special-form route 8 (evo_count_bath); when the 19:00 bath request
 * pet.bath_timer was pending: a child's bath training pet.training[1], the praise
 * window (300 s), the request ended. Clears the dirty looks, +1 happiness, save (save_game).
 */
void pet_bath(void)
{
    int f = 0;

    if (save.pet.bath_timer != 0)
        f = 1;
    anim_bath();
    evo_count_bath();
    if (f) {
        if (save.pet.life_stage == 2 && save.pet.training[1] != 0xff)
            save.pet.training[1]++;
        save.pet.discipline_flags |= 2;
        save.pet.praise_timer = 300;
        save.pet.bath_timer = 0;
        pet_bath_expired = 0;
        pet_praise_expired = 0;
    }
    save.pet.neglect_minutes = 0;
    save.pet.x062 = 0;
    save.pet.meals_today = 0;
    save.pet.x0cc = 0;
    save.pet.x148 = 0;
    pet_add_happiness(1);
    save_game();
}

/*
 * Bedroom background of the sleeping scene: the bedroom design x06A (room table: しんしつ)
 * with its bed (room_bed_pics[x06A], the baby bed D_217AF28 for a baby) and the bedroom
 * mess level pet.bedroom_dirt.
 */
void room_draw_bedroom(void)
{
    struct GfxObject *e;
    u8 t;
    u8 a;
    u8 *p;

    room_window_draw(2);
    e = &gfx_objects[gfx_object_count - 1];
    t = e->palette_base + e->image[2];
    switch (save.pet.bedroom_dirt) {
    case 4:
        a = gfx_object_create(0, 0x1c, 0, 0, 1, 0);
        break;
    case 5:
        a = gfx_object_create(0, 0x1c, 0, 0, 6, 0);
        break;
    default:
        a = gfx_object_create(0, 0x1c, 0, 0, 0, 0);
        break;
    }
    gfx_object_set_image(a, t, 0, (const u8 *)room_bedroom_designs[save.pet.room_designs[4]]);
    e = &gfx_objects[gfx_object_count - 1];
    t = e->palette_base + e->image[2];
    if (save.pet.life_stage == 1) {
        p = D_217AF28;
        gfx_object_create(0x40 - (p[0] >> 1), 0x56 - (p[1] >> 1), t, 0, 0, (const u8 *)p);
    } else {
        p = (u8 *)room_bed_pics[save.pet.room_designs[4]];
        gfx_object_create(0x40 - (p[0] >> 1), 0x51 - (p[1] >> 1), t, 0, 0, (const u8 *)p);
    }
}

/*
 * Wrapper of anim_christmas.
 */
void anim_christmas_play(void)
{
    anim_christmas();
}

/*
 * Returns 1 (stub).
 */
int func_240DB12(void)
{
    return 1;
}

/*
 * The RAM variables this file defines.
 */
u8 pet_bath_expired;
