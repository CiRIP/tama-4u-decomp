/*
 * The living room: background with the seasonal decorations, the poops and their blinking, and
 * the characters' birthday event on the main screen.
 *
 * Code 240F2DE..240F784, .rodata from 253C63C.
 */
#include "proto/room/room_draw.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/shop/shop_reform.h"
#include "proto/anim/anim_scripts.h"
#include "proto/ui/ui_message.h"
#include "proto/dl/dl_chara.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/pet/pet_care.h"
#include "proto/owner/owner_profile.h"
#include "flash.h"
#include "libc.h"

/*
 * Character whose birthday is month a, day b (chara_find_adult_by_birthday), or -1.
 */
int chara_birthday_on(u8 month, u8 day)
{
    return chara_find_adult_by_birthday(month, day);
}

/* unreferenced picture (32 x 30) */
const u8 D_253C640[] = {
    0x20, 0x1e, 0x09, 0x00, 0x01, 0xff, 0x07, 0xe0, 0x60, 0x00, 0x35, 0xeb, 0x03, 0x80, 0x07, 0x1f,
    0x05, 0x7f, 0x04, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21,
    0x22, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21,
    0x32, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21,
    0x32, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
    0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x44,
    0x44, 0x14, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x44, 0x41, 0x55,
    0x55, 0x65, 0x51, 0x15, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x41, 0x41, 0x15, 0x54, 0x55,
    0x55, 0x55, 0x16, 0x56, 0x61, 0x11, 0x00, 0x00, 0x00, 0x00, 0x41, 0x15, 0x54, 0x15, 0x54, 0x55,
    0x55, 0x55, 0x15, 0x66, 0x15, 0x66, 0x01, 0x00, 0x00, 0x10, 0x54, 0x41, 0x55, 0x54, 0x55, 0x55,
    0x55, 0x55, 0x65, 0x66, 0x56, 0x61, 0x16, 0x00, 0x00, 0x41, 0x55, 0x41, 0x55, 0x54, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x66, 0x56, 0x61, 0x66, 0x01, 0x00, 0x41, 0x15, 0x54, 0x55, 0x51, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x61, 0x66, 0x15, 0x66, 0x01, 0x10, 0x54, 0x15, 0x54, 0x15, 0x11, 0x55, 0x55,
    0x55, 0x55, 0x15, 0x11, 0x66, 0x15, 0x66, 0x16, 0x10, 0x54, 0x41, 0x55, 0x11, 0x11, 0x51, 0x55,
    0x55, 0x55, 0x11, 0x11, 0x61, 0x56, 0x61, 0x16, 0x10, 0x54, 0x41, 0x55, 0x11, 0x11, 0x61, 0x55,
    0x55, 0x55, 0x11, 0x11, 0x61, 0x56, 0x61, 0x16, 0x10, 0x54, 0x41, 0x15, 0x11, 0x11, 0x11, 0x55,
    0x55, 0x15, 0x11, 0x11, 0x11, 0x56, 0x61, 0x16, 0x10, 0x54, 0x41, 0x15, 0x11, 0x11, 0x11, 0x56,
    0x51, 0x15, 0x11, 0x11, 0x11, 0x56, 0x61, 0x16, 0x10, 0x54, 0x41, 0x55, 0x55, 0x54, 0x55, 0x15,
    0x11, 0x55, 0x55, 0x66, 0x66, 0x56, 0x61, 0x16, 0x10, 0x54, 0x41, 0x15, 0x51, 0x54, 0x55, 0x11,
    0x11, 0x51, 0x55, 0x66, 0x11, 0x56, 0x61, 0x16, 0x10, 0x54, 0x41, 0x15, 0x11, 0x41, 0x55, 0x55,
    0x55, 0x55, 0x65, 0x11, 0x11, 0x56, 0x61, 0x16, 0x10, 0x54, 0x41, 0x15, 0x11, 0x41, 0x65, 0x11,
    0x11, 0x41, 0x65, 0x11, 0x11, 0x56, 0x61, 0x16, 0x10, 0x54, 0x15, 0x14, 0x11, 0x41, 0x65, 0x11,
    0x11, 0x41, 0x65, 0x11, 0x11, 0x15, 0x66, 0x16, 0x00, 0x41, 0x15, 0x54, 0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x61, 0x15, 0x66, 0x01, 0x00, 0x41, 0x54, 0x41, 0x15, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x56, 0x61, 0x66, 0x01, 0x00, 0x10, 0x54, 0x41, 0x55, 0x11, 0x11, 0x41,
    0x65, 0x11, 0x11, 0x61, 0x56, 0x61, 0x16, 0x00, 0x00, 0x00, 0x41, 0x15, 0x54, 0x55, 0x11, 0x51,
    0x55, 0x11, 0x61, 0x66, 0x15, 0x66, 0x01, 0x00, 0x00, 0x00, 0x10, 0x51, 0x41, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x65, 0x56, 0x61, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x66, 0x16, 0x66,
    0x66, 0x16, 0x66, 0x16, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00,
};

/*
 * Today is another character's birthday (chara_birthday_match from the date)? If it is the pet's
 * own family: special animation; otherwise a scrolling message "<name> ... <pet name>"
 * (ui_show_message) with an animation.
 */
void pet_event_chara_birthday(void)
{
    u8 a[0x32];
    u8 b[0x100];
    const struct CharaEntry *p;
    struct DlCharaRecord *q;
    u16 v;
    int i;
    u8 x, y;
    u32 n;
    const u8 *s;

    v = 0xffff;
    for (p = chara_table; p->id != 0xffff; p++) {
        if (p->id == save.pet.x000) {
            v = p->family;
            break;
        }
    }
    if (v == 0xffff) {
        for (i = 0; i <= 7; i++) {
            q = dl_chara_record(i);
            if (q != (struct DlCharaRecord *)-1 && q->id == save.pet.x000) {
                v = q->id;
                break;
            }
        }
    }
    if (v != 0xffff) {
        x = rtc_get_month();
        y = rtc_get_day();
        n = chara_birthday_match(x, y);
        if (n > 0x2326)
            return;
        if (v == n) {
            anim_char_birthday_end();
            return;
        }
        s = D_253C840;
        memset(a, 0, 0x32);
        strcat((char *)a, (char *)chara_text(n, 1, 0));
        memset(b, 0, 0x100);
        for (i = 0; i <= 12; i++)
            strcat((char *)b, (char *)s);
        strcat((char *)b, (char *)a);
        strcat((char *)b, (char *)chara_text(save.pet.x000, 0, 0));
        strcat((char *)b, (char *)s);
        anim_char_birthday_start();
        ui_show_message(9, 0, b);
        anim_char_birthday_party();
    }
}

/*
 * Room decorations on special dates: the pet's birthday, a downloaded decoration
 * (item_seasonal_decoration), and seasonal ones for the default rooms: 3/1-3 (Hinamatsuri), 5/3-5
 * (Children's day), 10/29-31 (Halloween), 1/1-3 (New year), 12/23-25 (Christmas).
 */
u8 room_draw_decorations(u8 a)
{
    u8 r, m, d, t;
    int img;

    r = a;
    if (pet_special_mode_active() == 1)
        return;
    m = rtc_get_month();
    d = rtc_get_day();
    if (save.pet.owner_bday_month == m && save.pet.owner_bday_day == d) {
        r = a + 1;
        t = gfx_palette_slot_for(r, 0, 0);
        gfx_object_create(0, 0x1c, t, 0, 0, D_2185E10);
    } else if (item_seasonal_decoration((int)&img)) {
        r = a + 1;
        t = gfx_palette_slot_for(r, 0, 0);
        gfx_object_create(0, 0x1c, t, 0, 0, (const u8 *)img);
    } else if (save.pet.room_designs[0] <= 0x18) {
        if (m == 3 && d >= 1 && d <= 3) {
            r = a + 1;
            t = gfx_palette_slot_for(r, 0, 0);
            gfx_object_create(0, 0x1c, t, 0, 0, D_2170AF8);
        } else if (m == 5 && d >= 3 && d <= 5) {
            r = a + 1;
            t = gfx_palette_slot_for(r, 0, 0);
            gfx_object_create(0, 0x1c, t, 0, 0, D_2171D20);
        } else if (m == 10 && d >= 0x1d && d <= 0x1f) {
            r = a + 1;
            t = gfx_palette_slot_for(r, 0, 0);
            gfx_object_create(0, 0x1c, t, 0, 0, D_2176598);
        } else if (m == 1 && d >= 1 && d <= 3) {
            r = a + 1;
            t = gfx_palette_slot_for(r, 0, 0);
            gfx_object_create(0, 0x1c, t, 0, 0, D_216F8D0);
        } else if (m == 12 && d >= 0x17 && d <= 0x19) {
            r = a + 1;
            t = gfx_palette_slot_for(r, 0, 0);
            gfx_object_create(0, 0x1c, t, 0, 0, D_21777C0);
        }
    }
    return r;
}

/*
 * Room background: room picture room_living_designs[x066] + date decorations + dirt overlay after
 * 4/5 days without cleaning (pet.living_dirt). Returns the last object index.
 */
u8 room_draw_background(void)
{
    u8 a;
    u8 t;

    a = gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    gfx_object_set_image(a, 0, 0, (const u8 *)room_living_designs[save.pet.room_designs[0]]);
    a = room_draw_decorations(a);
    switch (save.pet.living_dirt) {
    case 4:
        t = gfx_palette_slot_for(a + 1, 0, 0);
        if (save.pet.life_stage > 2)
            gfx_object_create(0, 0x1c, t, 0, 0, D_2188248);
        else
            gfx_object_create(0, 0x1c, t, 0, 0, D_2187038);
        break;
    case 5:
        t = gfx_palette_slot_for(a + 1, 0, 0);
        if (save.pet.life_stage <= 2)
            gfx_object_create(0, 0x1c, t, 0, 0, D_2187038);
        else
            gfx_object_create(0, 0x1c, t, 0, 0, D_2189458);
        break;
    }
    return a;
}

/*
 * Draw the poops in the room (count pet_poops_drawn, max 6, positions room_poop_positions; pairs share a
 * picture, an odd one uses a single-poop picture); a != 0 also creates empty slots up to 4.
 */
void room_draw_poops(u8 a)
{
    struct GfxObject *e;
    int i;
    u8 c;
    u32 m;
    u8 z;
    u16 zz = 0;

    if (ui_message_frames_kept != 2)
        room_poop_blink = 0;
    e = &gfx_objects[gfx_object_count - 1];
    c = e->palette_base + e->image[2];
    room_poop_count = pet_poops_drawn;
    m = room_poop_count;
    if (m > 5)
        m = 6;
    if (pet_special_mode_active())
        return;
    i = 0;
    if (i >= (int)(m >> 1))
        goto done;
again:
    room_poop_objects[i] = gfx_object_create(room_poop_positions[i][0], room_poop_positions[i][1], c, 0, zz, D_214FB14);
    i++;
    if (i < (int)(m >> 1))
        goto again;
done:
    if (m - (m >> 1) * 2 == 1) {
        e = &gfx_objects[gfx_object_count - 1];
        c = e->palette_base + e->image[2];
        room_poop_objects[i] = gfx_object_create(room_poop_positions[i][0], room_poop_positions[i][1], c, 0, 0, D_214FBA8);
        i++;
    }
    if (a) {
        z = 0;
        if (i <= 3) {
        again2:
            room_poop_objects[i] = gfx_object_create(room_poop_positions[i][0], room_poop_positions[i][1], z, z, z, (const u8 *)z);
            i++;
            if (i <= 3)
                goto again2;
        }
    }
}

const u8 room_poop_positions[4][2] = { { 104, 80 }, { 104, 64 }, { 104, 48 }, { 104, 32 } };
const u8 D_253C840[] = _(" ");

/*
 * Blink the poops in the room (every other call they switch picture flag 0x10).
 */
u8 room_blink_poops(void)
{
    u8 b;
    int n;
    int i;

    b = ++room_poop_blink;
    rtc_get_month();
    rtc_get_day();
    n = room_poop_count;
    if (n > 6)
        n = 6;
    n = (n + 1) / 2;
    for (i = 0; i < n; i++) {
        if (b & 1)
            gfx_object_set_flags(room_poop_objects[i], 0x10);
        else
            gfx_object_set_flags(room_poop_objects[i], 0);
        func_0000000(&gfx_objects[room_poop_objects[i]]);
    }
    return 0;
}

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 room_poop_objects[4] DSTRAM;
u8 room_poop_blink DSTRAM;
u32 room_poop_count DSTRAM;
