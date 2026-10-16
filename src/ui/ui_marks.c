/*
 * Notification marks on the icon bars (food, touch, town, special, notebook, mail).
 *
 * Code 250669A..25067E2, .rodata from 253EFAC. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/ui/ui_marks.h"
#include "proto/save/save_flash.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "flash.h"

/*
 * Set the notification mark a (pet.marks[0] + a, shown on the icon bars) to b.
 */
void ui_set_mark(u8 a, int b)
{
    save.pet.marks[a] = b;
}

/*
 * Show/hide the five notification marks on the icon bars (picture 0x219B980) from
 * pet.marks[0]..D_00013D1.
 */
void ui_draw_marks(void)
{
    if (room_clock_notice != 0)
        return;
    if (save.pet.marks[0] == 1)
        gfx_object_set_image(ui_mark_objs.top_right, 0x12a, 0, D_219B980);
    else
        gfx_object_set_image(ui_mark_objs.top_right, 0x12a, 0, 0);
    if (save.pet.marks[1] == 1)
        gfx_object_set_image(ui_mark_objs.touch, 0x12e, 0, D_219B980);
    else
        gfx_object_set_image(ui_mark_objs.touch, 0x12e, 0, 0);
    if (save.pet.marks[2] == 1)
        gfx_object_set_image(ui_mark_objs.bottom_left, 0x132, 0, D_219B980);
    else
        gfx_object_set_image(ui_mark_objs.bottom_left, 0x132, 0, 0);
    if (save.pet.marks[3] == 1 || save.pet.marks[6] == 1)
        gfx_object_set_image(ui_mark_objs.special, 0x136, 0, D_219B980);
    else
        gfx_object_set_image(ui_mark_objs.special, 0x136, 0, 0);
    if (save.pet.marks[4] == 1 || save.pet.marks[5] == 1)
        gfx_object_set_image(ui_mark_objs.bottom_right, 0x13a, 0, D_219B980);
    else
        gfx_object_set_image(ui_mark_objs.bottom_right, 0x13a, 0, 0);
}

/*
 * Create the five notification-mark objects on the icon bars (positions near the icons,
 * icon-bar layer 0x4000).
 */
void ui_create_marks(void)
{
    const u8 *p = D_219B980;

    ui_mark_objs.top_right = gfx_object_create(0x59 - (p[0] >> 1), 0xe - (p[1] >> 1), 0x12a, 0, 0x4000, 0);
    ui_mark_objs.touch = gfx_object_create(0x40 - (p[0] >> 1), 0xe - (p[1] >> 1), 0x12e, 0, 0x4000, 0);
    ui_mark_objs.bottom_left = gfx_object_create(0x27 - (p[0] >> 1), 0x73 - (p[1] >> 1), 0x132, 0, 0x4000, 0);
    ui_mark_objs.special = gfx_object_create(0x40 - (p[0] >> 1), 0x73 - (p[1] >> 1), 0x136, 0, 0x4000, 0);
    ui_mark_objs.bottom_right = gfx_object_create(0x59 - (p[0] >> 1), 0x73 - (p[1] >> 1), 0x13a, 0, 0x4000, 0);
}

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
struct MarkObjs ui_mark_objs DSTRAM;
