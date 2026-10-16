/*
 * Tama Gashapon: the draw, its scenes, and the scrolling text with the pet.
 *
 * Code 251DB9A..251F41A, .rodata from 255FB24.
 */
#include "proto/spot/spot_gashapon.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/comm/comm_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"
#include "libc.h"

/*
 * Run spot_gashapon_draw with the key clicks suppressed.
 */
int spot_gashapon(void)
{
    int r;
    anim_busy = 1;
    r = spot_gashapon_draw();
    anim_busy = 0;
    return r;
}

/*
 * Tama Gashapon (capsule toy) draw: after the opening animation (spot_gashapon_scene_open) pick
 * rand() % 10: 0-1 = an item prize from the Gashapon ticket at 0x2760000 (item_prize_record(n),
 * stored with item_add_to_inventory after its animation spot_gashapon_scene_item), 2-3 = a background/character
 * prize (item_body_value -> spot_gashapon_scene_points, pet_add_points), 4-9 = a message (sub-record n-4 of
 * the ticket via spot_app_record, a text in the 2-byte charset shown with spot_gashapon_scene_message /
 * ui_scroll_text_with_pet). Saves afterwards.
 */
int spot_gashapon_draw(void)
{
    u8 saved[60];
    u8 buf[130];
    int i;
    u8 n;
    u8 *s;
    struct TamagoFile *e;
    u16 k;

    for (i = 0; i < 0x1a; i += 2) {
        buf[i] = 4;
        buf[i + 1] = 0x6c;
    }
    for (; i < 0x82; i++)
        buf[i] = 0;
    memcpy(saved, anim_pic_slots, 0x3c);
    spot_gashapon_scene_open();
    n = rand() % 10;
    switch (n) {
    case 0:
    case 1:
        e = (struct TamagoFile *)item_prize_record(n, (struct TamagoFile *)spot_gashapon_ticket_slot);
        item_load_pictures(anim_pic_slots[0], e, 0xf);
        if ((e->x04e == 1 || e->x04e == 0x81) && e->x04f == 3) {
            anim_pic_slots[0][0] = anim_pic_slots[2][0];
            anim_pic_slots[0][1] = D_2195C6C;
        } else {
            anim_pic_slots[0][1] = 0;
        }
        spot_gashapon_scene_item();
        item_add_to_inventory(e);
        break;
    case 2:
    case 3:
        k = item_body_value((struct AnimPicSlots *)anim_pic_slots, n - 2, (int)spot_gashapon_ticket_slot);
        spot_gashapon_scene_points(k);
        pet_add_points(k);
        break;
    default:
        s = (u8 *)spot_app_record(n - 4, (struct TamagoFile *)spot_gashapon_ticket_slot);
        i = 0x1a;
        for (;;) {
            if (*s == 0)
                break;
            buf[i] = *s++;
            i++;
        }
        s = buf;
        spot_gashapon_scene_message();
        ui_scroll_text_with_pet(s);
        snd_wait_end();
        break;
    }
    save_game();
    btn_clear();
    memcpy(anim_pic_slots, saved, 0x3c);
    return 0;
}

/*
 * Animated scene (background 0x21BEB08).
 */
void spot_gashapon_scene_open(void)
{
    anim_scene_room(6);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_draw_pic(2, D_21C0B90, 0x60, 0x28, 0x3b);
    anim_queue_sound(0x14);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_draw_pic(2, D_21C0B90, 0x30, 0x17, 0x44);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_draw_pic(2, D_21C0B90, 0x50, 7, 0x50);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_draw_pic(2, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_draw_pic(2, D_21C1188, 0, 0x69, 0x10);
    anim_draw_pic(3, D_214E930, 0, 0x62, 8);
    anim_draw_pic(4, D_214E930, 0x10, 0x72, 0x10);
    anim_queue_sound(0x14);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_draw_pic(2, D_21C1188, 0, 0x69, 0x10);
    anim_draw_pic(3, D_214E930, 0x10, 0x64, 0x11);
    anim_draw_pic(4, D_214E930, 0, 0x70, 8);
    anim_draw_pic(5, D_21C13A0, 0, 0x38, 0x17);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_draw_pic(2, D_21C1188, 0, 0x69, 0x10);
    anim_draw_pic(3, D_214E930, 0, 0x62, 8);
    anim_draw_pic(4, D_21C13A0, 0, 0x38, 0x17);
    anim_draw_pic(5, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x41, 0x24);
    anim_draw_pic(2, D_21C1188, 0, 0x69, 0x10);
    anim_draw_pic(3, D_21C13A0, 0, 0x38, 0x17);
    anim_draw_pic(4, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x3c, 0x29);
    anim_draw_pic(2, D_21C1188, 0, 0x64, 0x15);
    anim_draw_pic(3, D_21C13A0, 0, 0x33, 0x1c);
    anim_queue_sound(0x18);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x2d, 0x32);
    anim_draw_pic(2, D_21C1188, 0, 0x55, 0x1e);
    anim_draw_pic(3, D_21C13A0, 0, 0x24, 0x25);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x20, 0x34);
    anim_draw_pic(2, D_21C1188, 0, 0x48, 0x20);
    anim_draw_pic(3, D_21C13A0, 0, 0x17, 0x27);
    anim_queue_sound(0x18);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x1c, 0x44);
    anim_draw_pic(2, D_21C13A0, 0, 0x13, 0x37);
    anim_draw_pic(3, D_21C0D68, 0, 0x42, 0x22);
    anim_draw_pic(4, D_21C0F78, 0, 0x42, 0x23);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21BFD1C, 0, 0x13, 0x5a);
    anim_draw_pic(2, D_21C13A0, 0, 0xa, 0x4d);
    anim_draw_pic(3, D_21C0D68, 0, 0x40, 0x24);
    anim_draw_pic(4, D_21C0F78, 0, 0x40, 0x25);
    anim_queue_sound(0x18);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x24);
    anim_draw_pic(2, D_21C0F78, 0, 0x40, 0x25);
    anim_draw_pic(3, 0, 0, 0, 0);
    anim_draw_pic(4, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
}

/*
 * Animated scene (background 0x21BEB08 with 0x21C0D68/0x21C0F78).
 */
void spot_gashapon_scene_item(void)
{
    anim_scene_room(6);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x24);
    anim_draw_pic(2, D_21C0F78, 0, 0x40, 0x25);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x22);
    anim_draw_pic(2, D_2197434, 0, 0x40, 0xb);
    anim_draw_pic(3, D_21C0F78, 0, 0x40, 0x2c);
    anim_queue_sound(0x21);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x1c);
    anim_draw_pic(2, D_2198640, 0, 0x40, 0x11);
    anim_draw_pic(3, D_21C0F78, 0, 0x40, 0x30);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x1a);
    anim_draw_pic(2, D_2197434, 0, 0x40, 0x11);
    anim_draw_pic(3, D_21C0F78, 0, 0x40, 0x32);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, anim_pic_slots[0][1], 0, 0x40, 0x26);
    anim_draw_pic(2, D_21C0D68, 0x60, 0x51, 7);
    anim_draw_pic(3, anim_pic_slots[0][0], 0, 0x40, 0x2e);
    anim_draw_pic(4, D_2198640, 0, 0x40, 0x1a);
    anim_draw_pic(5, D_21C0F78, 0, 0x40, 0x39);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0x60, 0x63, -8);
    anim_draw_pic(2, D_21C0F78, 0, 0x40, 0x40);
    anim_draw_pic(3, anim_pic_slots[0][1], 0, 0x40, 0x23);
    anim_draw_pic(4, anim_pic_slots[0][0], 0, 0x40, 0x2b);
    anim_draw_pic(5, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, anim_pic_slots[0][1], 0, 0x40, 0x21);
    anim_draw_pic(2, anim_pic_slots[0][0], 0, 0x40, 0x29);
    anim_draw_pic(3, 0, 0, 0, 0);
    anim_draw_pic(4, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, anim_pic_slots[0][1], 0, 0x40, 0x21);
    anim_draw_pic(2, anim_pic_slots[0][0], 0, 0x40, 0x29);
    anim_draw_pic(3, D_214E930, 0, 0x28, 0x1d);
    anim_draw_pic(4, D_214E930, 0x10, 0x58, 0x2a);
    anim_queue_sound(0x2f);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, anim_pic_slots[0][1], 0, 0x40, 0x21);
    anim_draw_pic(2, anim_pic_slots[0][0], 0, 0x40, 0x29);
    anim_draw_pic(3, D_214E930, 0x20, 0x28, 0x30);
    anim_draw_pic(4, D_214E930, 0, 0x58, 0x1c);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, anim_pic_slots[0][1], 0, 0x40, 0x21);
    anim_draw_pic(2, anim_pic_slots[0][0], 0, 0x40, 0x29);
    anim_draw_pic(3, D_214E930, 0, 0x28, 0x1d);
    anim_draw_pic(4, D_214E930, 0x10, 0x58, 0x2a);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, anim_pic_slots[0][1], 0, 0x40, 0x21);
    anim_draw_pic(2, anim_pic_slots[0][0], 0, 0x40, 0x29);
    anim_draw_pic(3, D_214E930, 0x20, 0x28, 0x30);
    anim_draw_pic(4, D_214E930, 0, 0x58, 0x1c);
    anim_frame(0x1f4, 0);
}

#define WAIT anim_frame(500, 0)
/*
 * Animated scene with value n (background 0x21BEB08).
 */
void spot_gashapon_scene_points(u16 n)
{
    anim_scene_room(0xb);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x24);
    anim_draw_pic(2, D_21C0F78, 0, 0x40, 0x25);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x22);
    anim_draw_pic(2, D_2197434, 0, 0x40, 0xb);
    anim_draw_pic(3, D_21C0F78, 0, 0x40, 0x2c);
    anim_queue_sound(0x21);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x1c);
    anim_draw_pic(2, D_2198640, 0, 0x40, 0x11);
    anim_draw_pic(3, D_21C0F78, 0, 0x40, 0x30);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x1a);
    anim_draw_pic(2, D_2197434, 0, 0x40, 0x11);
    anim_draw_pic(3, D_21C0F78, 0, 0x40, 0x32);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_217C2DC, 0, 0x40, 0x32);
    anim_draw_pic(2, D_21C0D68, 0x60, 0x51, 7);
    anim_draw_pic(3, D_2198640, 0, 0x40, 0x1a);
    anim_draw_pic(4, D_21C0F78, 0, 0x40, 0x39);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0x60, 0x63, -8);
    anim_draw_pic(2, D_21C0F78, 0, 0x40, 0x40);
    anim_draw_pic(3, D_217C2DC, 0, 0x40, 0x2e);
    anim_draw_pic(4, 0, 0, 0, 0);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_217C2DC, 0, 0x40, 0x2e);
    anim_draw_pic(2, 0, 0, 0, 0);
    anim_draw_pic(3, 0, 0, 0, 0);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_217C2DC, 0, 0x40, 0x2e);
    anim_draw_pic(2, D_214E930, 0x10, 0x54, 0x32);
    anim_draw_pic(3, D_214E930, 0, 0x2c, 0x23);
    anim_draw_pic(4, D_2163A28, 0, 0x40, 0x12);
    anim_draw_digit(5, n % 10, 0, 0x4e, 0xe);
    if (n > 9)
        anim_draw_digit(6, n / 10 % 10, 0, 0x45, 0xe);
    if (n > 99)
        anim_draw_digit(7, n / 100 % 10, 0, 0x3c, 0xe);
    if (n > 999)
        anim_draw_digit(8, n / 1000 % 10, 0, 0x33, 0xe);
    if (n > 9999)
        anim_draw_digit(9, n / 10000 % 10, 0, 0x2a, 0xe);
    anim_draw_pic(10, D_21434EC, 0, 0x22, 0xd);
    anim_queue_sound(0x22);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_217C2DC, 0, 0x40, 0x2e);
    anim_draw_pic(2, D_214E930, 0x20, 0x2e, 0x39);
    anim_draw_pic(3, D_2163A28, 0, 0x40, 0x12);
    anim_draw_digit(4, n % 10, 0, 0x4e, 0xe);
    if (n > 9)
        anim_draw_digit(5, n / 10 % 10, 0, 0x45, 0xe);
    if (n > 99)
        anim_draw_digit(6, n / 100 % 10, 0, 0x3c, 0xe);
    if (n > 999)
        anim_draw_digit(7, n / 1000 % 10, 0, 0x33, 0xe);
    if (n > 9999)
        anim_draw_digit(8, n / 10000 % 10, 0, 0x2a, 0xe);
    anim_draw_pic(9, D_21434EC, 0, 0x22, 0xd);
    anim_draw_pic(10, 0, 0, 0, 0);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_217C2DC, 0, 0x40, 0x2e);
    anim_draw_pic(2, D_214E930, 0x10, 0x54, 0x32);
    anim_draw_pic(3, D_214E930, 0, 0x2c, 0x23);
    anim_draw_pic(4, D_2163A28, 0, 0x40, 0x12);
    anim_draw_digit(5, n % 10, 0, 0x4e, 0xe);
    if (n > 9)
        anim_draw_digit(6, n / 10 % 10, 0, 0x45, 0xe);
    if (n > 99)
        anim_draw_digit(7, n / 100 % 10, 0, 0x3c, 0xe);
    if (n > 999)
        anim_draw_digit(8, n / 1000 % 10, 0, 0x33, 0xe);
    if (n > 9999)
        anim_draw_digit(9, n / 10000 % 10, 0, 0x2a, 0xe);
    anim_draw_pic(10, D_21434EC, 0, 0x22, 0xd);
    WAIT;
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_217C2DC, 0, 0x40, 0x2e);
    anim_draw_pic(2, D_214E930, 0x20, 0x2e, 0x39);
    anim_draw_pic(3, D_2163A28, 0, 0x40, 0x12);
    anim_draw_digit(4, n % 10, 0, 0x4e, 0xe);
    if (n > 9)
        anim_draw_digit(5, n / 10 % 10, 0, 0x45, 0xe);
    if (n > 99)
        anim_draw_digit(6, n / 100 % 10, 0, 0x3c, 0xe);
    if (n > 999)
        anim_draw_digit(7, n / 1000 % 10, 0, 0x33, 0xe);
    if (n > 9999)
        anim_draw_digit(8, n / 10000 % 10, 0, 0x2a, 0xe);
    anim_draw_pic(9, D_21434EC, 0, 0x22, 0xd);
    anim_draw_pic(10, 0, 0, 0, 0);
    WAIT;
}

/*
 * Animated scene (background 0x21BEB08).
 */
void spot_gashapon_scene_message(void)
{
    anim_scene_room(5);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x24);
    anim_draw_pic(2, D_21C0F78, 0, 0x40, 0x25);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x22);
    anim_draw_pic(2, D_21C0F78, 0, 0x40, 0x2c);
    anim_draw_pic(3, D_2197434, 0, 0x40, 0xb);
    anim_queue_sound(0x21);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x1c);
    anim_draw_pic(2, D_2198640, 0, 0x40, 0x11);
    anim_draw_pic(3, D_21C0F78, 0, 0x40, 0x30);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0, 0x40, 0x1a);
    anim_draw_pic(2, D_2197434, 0, 0x40, 0x11);
    anim_draw_pic(3, D_21C0F78, 0, 0x40, 0x32);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, (u8 *)chara_sprite_set86_pose9, 0, 0x40, 0x2a);
    anim_draw_pic(2, D_21C0D68, 0x60, 0x51, 7);
    anim_draw_pic(3, D_2198640, 0, 0x40, 0x1a);
    anim_draw_pic(4, D_21C0F78, 0, 0x40, 0x39);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C0D68, 0x60, 0x63, -8);
    anim_draw_pic(2, D_21C0F78, 0, 0x40, 0x40);
    anim_draw_pic(3, (u8 *)chara_sprite_set86_pose9, 0, 0x40, 0x26);
    anim_draw_pic(4, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, (u8 *)chara_sprite_set86_pose9, 0, 0x40, 0x24);
    anim_draw_pic(2, 0, 0, 0, 0);
    anim_draw_pic(3, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
}

/*
 * Scroll a text s (wrapped with D_255FB28) on a frame with the pet, 400 ms steps.
 */
void ui_scroll_text_with_pet(const u8 *s)
{
    char buf[256];
    char *q;
    int len;
    int i;
    int h;
    int k;

    q = buf;
    len = strlen((char *)s);
    i = 0;
    strcpy(q, (char *)s);
    strcat((char *)q, (char *)D_255FB28);
    anim_scene_room(4);
    anim_draw_pic(0, D_21BEB08, 0, 0x40, 0x24);
    anim_draw_pic(1, (u8 *)*chara_sprite_set86_pose2, 0, 0x40, 0x24);
    anim_draw_pic(2, D_21621EC, 0, 0x40, 0xd);
    anim_draw_text(3, (u8 *)q, 1, 6, 8);
    func_00009A6();
    h = gfx_objects[1].y;
    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 1;
    tmr_t5_start_timeout(400);
    while (q - buf < len) {
        if (tmr_flags & 0x20) {
            tmr_flags &= ~0x20;
            k = i > 1 ? 1 : 0;
            gfx_object_move_dirty(1, gfx_objects[1].x, h + k);
            anim_draw_text(3, (u8 *)q, 1, 6, 8);
            func_00009A6();
            q += 4;
            i = i > 2 ? 0 : i + 1;
        }
        sys_idle_tick();
    }
    tmr_delay_ms_t5(200);
    btn_clear();
    anim_busy = 0;
}

const u8 D_255FB28[] = _("            ");
