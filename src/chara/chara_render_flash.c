/*
 * Rendering characters and pictures into flash records: the gift sector, picture records, the
 * five pose sets and their flipped variants (a variant of gfx_anim's pose rendering with the
 * same pose table); then screen read-back, the battery check and the photo.
 *
 * Open: where this file ends and comm_flash_buffers begins (somewhere in 25020BA..25029FA), and
 * whether the first two functions are comm_record_pics's.
 *
 * Code 2500E7E..25029FA, .rodata from 253B738. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/chara/chara_render_flash.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/pet/pet_bath.h"
#include "proto/room/room_draw.h"
#include "proto/album/album_store.h"
#include "proto/sys/sys_boot.h"
#include "proto/evo/evo_special.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/comm/comm_record_pics.h"
#include "proto/ir/ir_receive.h"
#include "proto/ir/ir_packets.h"
#include "proto/ir/ir_driver.h"
#include "proto/dl/dl_chara.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/owner/owner_profile.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/* called without a prototype (the original had no declaration in scope: the arguments
 * are passed unconverted) */

/* the character's pose sets (as F40's D_255C804..): count, then {width, height, pose} */
const u8 D_253B73C[] = { 3, 0x30, 0x30, 0x8d, 0x30, 0x48, 0x0d, 0x30, 0x30, 0x00 };
const u8 D_253B748[] = { 3, 0x30, 0x30, 0x00, 0x30, 0x30, 0x01, 0x30, 0x30, 0x09 };
const u8 D_253B754[] = { 3, 0x30, 0x30, 0x00, 0x30, 0x30, 0x09, 0x30, 0x30, 0x0c };
const u8 D_253B760[] = { 3, 0x30, 0x30, 0x80, 0x30, 0x30, 0x82, 0x30, 0x30, 0x8a };
const u8 D_253B76C[] = { 3, 0x30, 0x30, 0x80, 0x30, 0x30, 0x89, 0x30, 0x30, 0x8c };
const u32 chara_pic_set_table[5] = {
    (u32)D_253B73C, (u32)D_253B748, (u32)D_253B754, (u32)D_253B760, (u32)D_253B76C,
};

/*
 * Erase flash sector 0x86 and write a fresh 3-entry header record (1, 0x80, 0x48) there
 * (comm_render_picture_records mode 3).
 */
void comm_gift_sector_init(int a)
{
    u8 buf[12];

    sys_irq_disable();
    func_0000A54(0x2000000, 0x86, 0x86);
    sys_irq_enable();
    comm_clear_pose_list(buf);
    buf[0] = 1;
    buf[1] = 0x80;
    buf[2] = 0x48;
    buf[3] = 0;
    comm_render_picture_records(buf, a, 3, 0);
}

/*
 * Picture records for the exchange: for each entry {width, height, pose} of the pose list a,
 * draw the pet in that pose (modes 0 / 1), a pose of the companion's sprite set (2) or the
 * current screen, the room (3), read the pixels back (gfx_screen_pixel) and program a picture
 * record (length, size, palettes, pixels) into flash at b; mode 4 writes the count header.
 * Returns the bytes written.
 */
int comm_render_picture_records(const struct PoseSet *a, u8 *b, int mode, u8 c)
{
    int i;
    int j;
    int k;
    int off;
    u16 start;
    u8 mask;
    int attr;
    u8 h[6];
    int z;
    int x, y;
    u8 n;
    u16 w, len;
    u32 t;
    u8 *q;
    u16 *ps;

    off = 0;
    sys_irq_disable();
    if (mode == 0 || mode == 4 || mode == 3) {
        if (func_0000B52((u16 *)b, a->count, 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (mode == 4)
            goto out;
    } else if (mode == 1) {
        off += 2;
    }
    for (i = 0, mask = 1; i < a->count; i++, mask <<= 1) {
        h[0] = a->entries[i].w;
        h[1] = a->entries[i].h;
        h[3] = 0;
        h[4] = 1;
        h[5] = 0xff;
        attr = (c & mask) ? 0x10 : 0;
        switch (mode) {
        case 0:
        case 1:
            gfx_display_reset();
            z = 0;
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            k = a->entries[i].pose;
            ps = &chara_pet_sprite_set;
            if (*ps < 100) {
                x = h[0] >> 1;
                y = h[1] - (((u8 *)((u32 *)&chara_sprite_sets[chara_pet_sprite_set])[k])[1] >> 1) - 0x1c;
            } else {
                t = (int)dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, k);
                x = h[0] >> 1;
                y = h[1] - (((u8 *)t)[1] >> 1) - 0x1c;
            }
            anim_busy = 1;
            if (chara_pet_sprite_set < 100)
                q = (u8 *)chara_sprite_sets[chara_pet_sprite_set].pose0;
            else
                q = dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, 0);
            if (q[1] <= 0x2d)
                chara_draw_pet(0, 0, k, attr | 0x4080, x, y);
            else
                chara_draw_pet(0, 0, k, attr | 0x4080, x, y + 5);
            anim_busy = 0;
            j = 3;
            break;
        case 2:
            gfx_display_reset();
            z = 0;
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            k = a->entries[i].pose;
            gfx_object_set_flags(0, attr | 0x4080);
            gfx_object_set_image(0, 0, 0, (const u8 *)((u32 *)&chara_sprite_sets[(s8)save.pet.companion_sprite_set])[k]);
            x = h[0] >> 1;
            x -= gfx_objects[0].image[0] >> 1;
            y = h[1] - gfx_objects[0].image[1] - 0xe;
            gfx_object_move_dirty(0, x, y);
            j = 3;
            break;
        case 3:
            gfx_display_reset();
            z = 0;
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            gfx_object_create(z, z, z, z, z, (const u8 *)z);
            gfx_display_reset();
            room_draw_background();
            j = 4;
            break;
        }
        n = gfx_palette_slot_for(j, 0, 0);
        if (n > 0x10)
            h[2] = n;
        else
            h[2] = 0x11;
        start = off;
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[1] << 8 | h[0], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[3] << 8 | h[2], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[5] << 8 | h[4], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (mode == 0 || mode == 1) {
            if (save.pet.x0cc == 1) {
                gfx_objects[0].flags |= 0x800;
                gfx_objects[1].flags |= 0x800;
                gfx_objects[2].flags |= 0x800;
            } else if (save.pet.x148 != 0) {
                if ((u8)(save.pet.life_stage - 2) <= 1)
                    gfx_objects[0].flags |= 0x400;
                gfx_objects[1].flags |= 0x400;
                gfx_objects[2].flags |= 0x400;
            }
        }
        for (j = 0; j < gfx_objects[0].image[2]; j++) {
            t = func_000014A(&gfx_objects[0], j);
            if ((mode == 0 || mode == 1) && save.pet.x0cc == 1)
                t = func_00002D4(t);
            if (func_0000B52((u16 *)(b + off), t >> 8 | (t & 0xff) << 8, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        if (gfx_objects[1].image != 0) {
            for (j = 0; j < gfx_objects[1].image[2]; j++) {
                t = func_000014A(&gfx_objects[1], j);
                if ((mode == 0 || mode == 1) && save.pet.x0cc == 1)
                    t = func_00002D4(t);
                if (func_0000B52((u16 *)(b + off), t >> 8 | (t & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        if (gfx_objects[2].image != 0) {
            for (j = 0; j < gfx_objects[2].image[2]; j++) {
                t = func_000014A(&gfx_objects[2], j);
                if ((mode == 0 || mode == 1) && save.pet.x0cc == 1)
                    t = func_00002D4(t);
                if (func_0000B52((u16 *)(b + off), t >> 8 | (t & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        for (j = n; j < h[2]; j++) {
            if (func_0000B52((u16 *)(b + off), 0, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        switch (mode) {
        case 0:
        case 1:
        case 2:
            for (y = 0; y < h[1]; y++) {
                for (x = 0; x < h[0]; x += 2) {
                    w = func_00004C8(x, y);
                    w |= func_00004C8(x + 1, y) << 8;
                    if (func_0000B52((u16 *)(b + off), w, 0x2000000) != 0) {
                        off = -1;
                        goto out;
                    }
                    off += 2;
                }
            }
            break;
        case 3:
            for (y = 0x1c; y < h[1] + 0x1c; y++) {
                for (x = 0; x < h[0]; x += 2) {
                    w = gfx_screen_pixel(x, y);
                    w |= gfx_screen_pixel(x + 1, y) << 8;
                    if (func_0000B52((u16 *)(b + off), w, 0x2000000) != 0) {
                        off = -1;
                        goto out;
                    }
                    off += 2;
                }
            }
            break;
        }
        len = off - start - 2;
        if (func_0000B52((u16 *)(b + start), len, 0x2000000) != 0) {
            off = -1;
            goto out;
        }
    }
out:
    sys_irq_enable();
    return off;
}

/*
 * Increment both bytes of v when non-zero (index -> palette slot conversion).
 */
u16 gfx_index_to_palette_slot(u16 v)
{
    u8 b[2];

    b[0] = v;
    if (b[0])
        b[0]++;
    b[1] = v >> 8;
    if (b[1])
        b[1]++;
    return b[0] | (b[1] << 8);
}

/* RENDER THE CHARACTER INTO FLASH: for each entry {w, h, pose | 0x10 = accessory flag,
 * 0x80 = without clothes} of list a, draw the character (chara_draw_pet with its clothes /
 * accessory sprites) into the offscreen sprite list, then program at b an image record:
 * u16 length, {w, h}, {palette count (>= 0x12), 0}, {1, 0xFF}, the palettes of the up to
 * three sprites (byte-swapped, remapped by func_00002D4 when dirty), zero padding, then
 * the w x h pixels read back from the composed frame (func_00004C8(x, y), two per
 * halfword, gfx_index_to_palette_slot filtered). A leading u16 = count << 8. Used by album_register
 * for the raised-characters snapshot (D_2542AF8 list); chara_render_poses_flash_flip is the same
 * without the pixel filter (the portrait images sent in the NFC exchange records,
 * comm_render_picture_records the variant with explicit modes). Returns the bytes written, -1 on a
 * flash error. */
/*
 * Render the pose list a of the current pet (with accessories) and program the pictures
 * into flash at b (same picture format as chara_render_poses_to_flash). Used by the album
 * (album_register) and the TAMAGO profile.
 */
int chara_render_poses_flash(const struct PoseSet *a, u8 *b)
{
    int i;
    int k;
    int off;
    u16 start;
    int j;
    u8 h[6];
    int z;
    u16 attr;
    int x, y;
    u32 dy;
    u8 n;
    u8 kept;
    u16 len, v, w;
    u8 *p;
    u8 *q;
    u16 d;
    int s;
    u8 t;

    off = 0;
    sys_irq_disable();
    if (func_0000B52((u16 *)(b + off), a->count << 8, 0x2000000) != 0) {
        off = -1;
        goto out;
    }
    off += 2;
    for (i = 0; i < a->count; i++) {
        h[0] = a->entries[i].w;
        h[1] = a->entries[i].h;
        h[3] = 0;
        h[4] = 1;
        h[5] = 0xff;
        gfx_display_reset();
        z = 0;
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        k = a->entries[i].pose;
        d = chara_pet_sprite_set;
        k &= 0xf;
        if (d < 100) {
            x = h[0] >> 1;
            dy = h[1];
            t = ((u8 *)((u32 *)&chara_sprite_sets[chara_pet_sprite_set])[k])[1];
        } else {
            const u8 *pp = dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, k);

            x = h[0] >> 1;
            dy = h[1];
            t = pp[1];
        }
        dy = dy - (t >> 1) - 0x1c;
        anim_busy = 1;
        attr = 0x4080;
        if (a->entries[i].pose & 0x10)
            attr = 0x4090;
        if (!(a->entries[i].pose & 0x80)) {
            if (chara_pet_sprite_set < 100)
                q = (u8 *)((u32 *)&chara_sprite_sets[chara_pet_sprite_set])[0];
            else
                q = (u8 *)dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, 0);
            if (q[1] <= 0x2d)
                chara_draw_pet(0, 0, k, attr, x, dy);
            else
                chara_draw_pet(0, 0, k, attr, x, dy + 5);
        } else {
            kept = save.pet.worn_clothes;
            save.pet.worn_clothes = 0;
            if (chara_pet_sprite_set < 100)
                q = (u8 *)((u32 *)&chara_sprite_sets[chara_pet_sprite_set])[0];
            else
                q = (u8 *)dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, 0);
            if (q[1] <= 0x2d)
                chara_draw_pet(0, 0, k, attr, x, dy);
            else
                chara_draw_pet(0, 0, k, attr, x, dy + 5);
            save.pet.worn_clothes = kept;
        }
        anim_busy = 0;
        dy = (u8)(gfx_palette_slot_for(3, 0, 0) + 1);
        h[2] = dy;
        if (dy <= 0x10)
            h[2] = 0x12;
        start = off;
        off += 2;
                if (func_0000B52((u16 *)(b + off), h[1] << 8 | h[0], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
                if (func_0000B52((u16 *)(b + off), h[3] << 8 | h[2], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
                if (func_0000B52((u16 *)(b + off), h[5] << 8 | h[4], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (save.pet.x0cc == 1) {
            gfx_objects[0].flags |= 0x800;
            gfx_objects[1].flags |= 0x800;
            gfx_objects[2].flags |= 0x800;
        } else if (save.pet.x148 != 0) {
            if (save.pet.life_stage == 2 || save.pet.life_stage == 3)
                gfx_objects[0].flags |= 0x400;
            gfx_objects[1].flags |= 0x400;
            gfx_objects[2].flags |= 0x400;
        }
        for (x = 0; x < gfx_objects[0].image[2]; x++) {
            v = func_000014A(&gfx_objects[0], x);
            if (save.pet.x0cc == 1)
                v = func_00002D4(v);
            if (x == 1) {
                if (func_0000B52((u16 *)(b + off), 0x60, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
                        if (func_0000B52((u16 *)(b + off), v >> 8 | (v & 0xff) << 8, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        if (gfx_objects[1].image != 0) {
            for (x = 0; x < gfx_objects[1].image[2]; x++) {
                v = func_000014A(&gfx_objects[1], x);
                if (save.pet.x0cc == 1)
                    v = func_00002D4(v);
                            if (func_0000B52((u16 *)(b + off), v >> 8 | (v & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        if (gfx_objects[2].image != 0) {
            for (x = 0; x < gfx_objects[2].image[2]; x++) {
                v = func_000014A(&gfx_objects[2], x);
                if (save.pet.x0cc == 1)
                    v = func_00002D4(v);
                            if (func_0000B52((u16 *)(b + off), v >> 8 | (v & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        for (x = dy; x < h[2]; x++) {
            if (func_0000B52((u16 *)(b + off), 0, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        for (y = 0; y < h[1]; y++) {
            for (x = 0; x < h[0]; x += 2) {
                w = func_00004C8(x, y);
                w |= func_00004C8(x + 1, y) << 8;
                w = gfx_index_to_palette_slot(w);
                if (func_0000B52((u16 *)(b + off), w, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        len = off - start - 2;
        if (func_0000B52((u16 *)(b + start), (len & 0xff) << 8 | len >> 8, 0x2000000) != 0) {
            off = -1;
            goto out;
        }
    }
out:
    sys_irq_enable();
    return off;
}

/*
 * Render picture set a (chara_pic_set_table[a]) of the pet into flash record b.
 */
int chara_render_pic_set(int a, struct PictureSetHdr *b)
{
    return chara_render_poses_flash((const struct PoseSet *)chara_pic_set_table[a], (u8 *)b);
}

/* Render the character poses of list a (bit i of c = accessory on for entry i) into
 * flash at b as image records - see chara/chara_render_flash.c for the record layout; this is the
 * writer behind the image sections of the Tama-to-Tama exchange records (F70b). */
/*
 * Like chara_render_poses_flash with a horizontal-flip mask c.
 */
int chara_render_poses_flash_flip(const struct PoseSet *a, u8 *b, u8 c)
{
    int i;
    int k;
    int off;
    u16 start;
    u8 mask;
    int attr;
    u8 h[6];
    int z;
    int x, y, j;
    u8 n;
    u8 kept;
    u16 len, w;
    u32 t;
    u8 *q;
    u16 *ps;

    off = 0;
    sys_irq_disable();
    if (func_0000B52((u16 *)(b + off), a->count << 8, 0x2000000) != 0) {
        off = -1;
        goto out;
    }
    off += 2;
    for (i = 0, mask = 1; i < a->count; i++, mask <<= 1) {
        h[0] = a->entries[i].w;
        h[1] = a->entries[i].h;
        h[3] = 0;
        h[4] = 1;
        h[5] = 0xff;
        attr = (c & mask) ? 0x10 : 0;
        gfx_display_reset();
        z = 0;
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        k = a->entries[i].pose & 0xf;
        ps = &chara_pet_sprite_set;
        if (*ps < 100) {
            x = h[0] >> 1;
            y = h[1] - (((u8 *)((u32 *)&chara_sprite_sets[chara_pet_sprite_set])[k])[1] >> 1) - 0x1c;
        } else {
            t = (int)dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, k);
            x = h[0] >> 1;
            y = h[1] - (((u8 *)t)[1] >> 1) - 0x1c;
        }
        anim_busy = 1;
        if (!(a->entries[i].pose & 0x80)) {
            if (chara_pet_sprite_set < 100)
                q = (u8 *)chara_sprite_sets[chara_pet_sprite_set].pose0;
            else
                q = dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, 0);
            if (q[1] <= 0x2d)
                chara_draw_pet(0, 0, k, attr | 0x4080, x, y);
            else
                chara_draw_pet(0, 0, k, attr | 0x4080, x, y + 5);
        } else {
            kept = save.pet.worn_clothes;
            save.pet.worn_clothes = 0;
            if (chara_pet_sprite_set < 100)
                q = (u8 *)chara_sprite_sets[chara_pet_sprite_set].pose0;
            else
                q = dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, 0);
            if (q[1] <= 0x2d)
                chara_draw_pet(0, 0, k, attr | 0x4080, x, y);
            else
                chara_draw_pet(0, 0, k, attr | 0x4080, x, y + 5);
            save.pet.worn_clothes = kept;
        }
        anim_busy = 0;
        n = gfx_palette_slot_for(3, 0, 0);
        if (n > 0x10)
            h[2] = n;
        else
            h[2] = 0x11;
        start = off;
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[1] << 8 | h[0], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[3] << 8 | h[2], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[5] << 8 | h[4], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (save.pet.x0cc == 1) {
            gfx_objects[0].flags |= 0x800;
            gfx_objects[1].flags |= 0x800;
            gfx_objects[2].flags |= 0x800;
        } else if (save.pet.x148 != 0) {
            if ((u8)(save.pet.life_stage - 2) <= 1)
                gfx_objects[0].flags |= 0x400;
            gfx_objects[1].flags |= 0x400;
            gfx_objects[2].flags |= 0x400;
        }
        for (j = 0; j < gfx_objects[0].image[2]; j++) {
            t = func_000014A(&gfx_objects[0], j);
            if (save.pet.x0cc == 1)
                t = func_00002D4(t);
            if (func_0000B52((u16 *)(b + off), t >> 8 | (t & 0xff) << 8, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        if (gfx_objects[1].image != 0) {
            for (j = 0; j < gfx_objects[1].image[2]; j++) {
                t = func_000014A(&gfx_objects[1], j);
                if (save.pet.x0cc == 1)
                    t = func_00002D4(t);
                if (func_0000B52((u16 *)(b + off), t >> 8 | (t & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        if (gfx_objects[2].image != 0) {
            for (j = 0; j < gfx_objects[2].image[2]; j++) {
                t = func_000014A(&gfx_objects[2], j);
                if (save.pet.x0cc == 1)
                    t = func_00002D4(t);
                if (func_0000B52((u16 *)(b + off), t >> 8 | (t & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        for (j = n; j < h[2]; j++) {
            if (func_0000B52((u16 *)(b + off), 0, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        for (y = 0; y < h[1]; y++) {
            x = 0;
            if (x < h[0]) {
                do {
                    w = func_00004C8(x, y);
                    w |= func_00004C8(x + 1, y) << 8;
                    if (func_0000B52((u16 *)(b + off), w, 0x2000000) != 0) {
                        off = -1;
                        goto out;
                    }
                    off += 2;
                    x += 2;
                } while (x < h[0]);
            }
        }
        len = off - start - 2;
        if (func_0000B52((u16 *)(b + start), (len & 0xff) << 8 | len >> 8, 0x2000000) != 0) {
            off = -1;
            goto out;
        }
    }
out:
    sys_irq_enable();
    return off;
}

/*
 * Render picture set a with flip mask c into b.
 */
int chara_render_pic_set_flip(int a, int b, u8 c)
{
    return chara_render_poses_flash_flip((const struct PoseSet *)chara_pic_set_table[a], (u8 *)b, c);
}

/*
 * Create a full-screen display object showing picture p; returns its index.
 */
u8 gfx_fullscreen_object(const u8 *p)
{
    u8 t = gfx_object_create(0, 0x1c, 0, 0, 0, 0);

    gfx_object_set_image(t, 0, 0, p);
    return t;
}

struct ImageData { u8 width; u8 height; u8 data[1]; };
/*
 * Pixel (palette index) of display object p at (x, y), honouring its flip/swap flags.
 */
u8 gfx_object_pixel(struct GfxObject *p, u8 x, u8 y)
{
    struct ImageData *img;
    u8 t;

    img = (struct ImageData *)p->image;
    if (p->flags & 0x40) {
        t = x;
        x = y;
        y = t;
    }
    if (p->flags & 0x20)
        y = p->height - y - 1;
    if (p->flags & 0x10)
        x = p->width - x - 1;
    if ((s16)p->flags < 0)
        return img->data[p->pixel_offset + y * img->width + x + 4];
    if (x & 1)
        return img->data[p->pixel_offset + y * (img->width >> 1) + (x >> 1) + 4] >> 4;
    return img->data[p->pixel_offset + y * (img->width >> 1) + (x >> 1) + 4] & 0xf;
}

/*
 * Screen read-back: the palette colour shown at (x, y) by the topmost main-area object
 * (icon-bar objects skipped; the frame-buffer byte for text objects); the picture-record
 * writer comm_render_picture_records (mode 3, the room picture) copies the screen with it.
 */
u8 gfx_screen_pixel(s16 x, s16 y)
{
    s16 i;
    s16 dx, dy;
    u8 c;
    struct GfxObject *e;

    c = 0;
    for (i = gfx_object_count - 1; i >= 0; i--) {
        e = &gfx_objects[i];
        if (e->image == 0)
            continue;
        if (e->flags & 0x4000)
            continue;
        dy = y - e->y;
        if (dy < 0 || e->height <= dy)
            continue;
        dx = x - e->x;
        if (dx < 0 || e->width <= dx)
            continue;
        if (e->flags & 0x1000) {
            c = ((u8 *)&gfx_frame_buffer)[(u16)(x + ((y - 0x1c) << 7))];
            break;
        }
        c = gfx_object_pixel(e, dx, dy);
        if (e->flags & 0x2000) {
            c = ~c;
            c %= e->image[2];
        }
        if (c != 0) {
            c += e->palette_base;
            break;
        }
    }
    return c;
}

/*
 * Legacy IR: store a received letter / picture (exchange banks 0x27D5500 / 0x27C5528) into
 * the photo album slots (0x2770000; the slot helper is incomplete).
 */
void ir_store_received_letter(int a)
{
    int u;

    btn_enable();
    comm_redraw_hook();
    if (a == 0)
        album_photo_slot();
    else
        album_photo_slot();
    btn_disable();
}

/*
 * Battery check: sys_awake_seconds = 0 and wake the display (sys_display_wake(0)), then up to 10 A/D
 * conversions of channel 0 (AIN0/P70, the battery divider), each started with ADE|ADST
 * (pAD_EN_SMPL_STAT bits 2,1) and waited for with halt until the end-of-conversion ISR
 * sys_adc_isr stores the result in sys_battery_level and clears sys_adc_busy; the A/D interrupt is
 * enabled for the duration if it was off. Returns 1 ("battery low") as soon as 5
 * readings are <= 0x2E3 (of 0x3FF), else 0. Called before/after every NFC session and on
 * wake; a 1 makes the caller run sys_power_down (shutdown).
 */
int sys_battery_low(void)
{
    int flag;
    u8 i;
    u8 n;

    i = 10;
    flag = 0;
    n = 0;
    sys_awake_seconds = n;
    sys_display_wake(n);
    if ((INT_EP47_ERTC_EAD & 2) == 0) {
        INT_EP47_ERTC_EAD |= 2;
        flag = 1;
    }
    while (i-- != 0) {
        sys_adc_busy = 1;
        AD_EN_SMPL_STAT |= 4;
        AD_EN_SMPL_STAT |= 2;
        do
            sys_idle_tick();
        while (sys_adc_busy != 0);
        if (sys_battery_level <= 0x2e3) {
            n++;
            if (n > 4) {
                if (flag)
                    INT_EP47_ERTC_EAD ^= 2;
                return 1;
            }
        }
    }
    if (flag)
        INT_EP47_ERTC_EAD ^= 2;
    return 0;
}

/*
 * Address of photo album slot i (0x2770000 + i * 0x2800). (Decompiled with i
 * uninitialised: incomplete C.)
 */
u8 *album_photo_slot(void)
{
    u8 tmp[72];
    int i;

    return (u8 *)0x2770000 + i * 0x2800;
}

struct RecordDirView {
    u8 pad000[0xe2];
    u16 sect_poses;
    u8 pad0e4[0x8];
    u16 sect_images;
};
/*
 * Compose the commemorative photo of an exchange from the records b and c: the room picture
 * of section 0xEC as background, our and the peer's character pictures side by side (the
 * side by the role a), both owner names, then store the screen in a free photo slot
 * (0x2770000 + k * 0x2800, album_photo_write).
 */
u8 *album_make_photo(u8 a, u8 *b, u8 *c)
{
    u8 s1[18];
    u8 s2[18];
    u8 *pb[6];
    u8 *pc[6];
    u8 buf[6];
    struct RecordDirView *hb;
    struct RecordDirView *hc;
    int r;
    int i;
    int n;
    int len;
    u8 *p;
    u8 *q;
    u8 t, k;
    int x, y;

    hc = (struct RecordDirView *)c;
    hb = (struct RecordDirView *)b;
    r = flash_first_erased_record(D_2770000, 0x2800, 6);
    if (r < 0) {
        r = item_pick_slot_to_overwrite();
        if (r < 0)
            return (u8 *)-2;
    }
    comm_show_now_printing();
    memset(pc, 0, 0x18);
    p = c + hc->sect_poses + 4;
    n = p[1] << 8 | p[0];
    p += 2;
    for (i = 0; i < n; i++) {
        len = p[1] << 8 | p[0];
        p += 2;
        pc[i] = p;
        p += len;
    }
    memset(pb, 0, 0x18);
    p = b + hb->sect_poses + 4;
    n = p[1] << 8 | p[0];
    p += 2;
    for (i = 0; i < n; i++) {
        len = p[1] << 8 | p[0];
        p += 2;
        pb[i] = p;
        p += len;
    }
    gfx_display_reset();
    if (a)
        p = b + hb->sect_images;
    else
        p = c + hc->sect_images;
    p += 8;
    gfx_object_create(0, 0, 0, 0, 0, (const u8 *)p);

    t = gfx_palette_slot_for(gfx_object_count, 0, 0);
    p = pb[0];
    x = a ? 0x2b : 0x55;
    y = 0x13;
    x -= p[0] >> 1;
    y -= p[1] >> 1;
    if (a)
        gfx_object_create(x, y, t, 0, 0x10, (const u8 *)p);
    else
        gfx_object_create(x, y, t, 0, 0, (const u8 *)p);

    t = gfx_palette_slot_for(gfx_object_count, 0, 0);
    p = pc[0];
    x = a ? 0x55 : 0x2b;
    y = 0x13;
    x -= p[0] >> 1;
    y -= p[1] >> 1;
    if (a)
        gfx_object_create(x, y, t, 0, 0, (const u8 *)p);
    else
        gfx_object_create(x, y, t, 0, 0x10, (const u8 *)p);

    t = gfx_palette_slot_for(gfx_object_count, 0, 0);
    gfx_object_create(0, 0, t, 0, 0, D_21170C0);
    memset(s1, 0, 0x12);
    memset(s2, 0, 0x12);
    if (a) {
        memcpy(s1, b + 0x60, 0x10);
        memcpy(s2, save.pet.owner_name, 0x10);
    } else {
        memcpy(s1, save.pet.owner_name, 0x10);
        memcpy(s2, b + 0x60, 0x10);
    }
    t = gfx_palette_slot_for(gfx_object_count, 0, 0);
    k = gfx_object_create(0x1a, 0x2e, t, 0, 0, 0);
    gfx_draw_text_line(k, 0x1a, 0x2e, t, (int)s2, 1, 8);
    t = gfx_palette_slot_for(gfx_object_count, 0, 0);
    k = gfx_object_create(0x1a, 0x39, t, 0, 0, 0);
    gfx_draw_text_line(k, 0x1a, 0x39, t, (int)s1, 1, 8);
    buf[0] = 0x80;
    buf[1] = 0x48;
    buf[2] = gfx_palette_slot_for(gfx_object_count, 0, 0);
    buf[3] = 0;
    buf[4] = 1;
    buf[5] = 0xff;
    if (album_photo_write(buf, r) != 0)
        return (u8 *)-1;
    return (u8 *)0x2770000 + r * 0x2800;
}

/*
 * Program the photo a into photo album slot b (0x2770000 + b * 0x2800).
 */
int album_photo_write(u8 *a, int b)
{
    int i;
    int off;
    u8 *base;
    int j, k, p, o;
    u16 v;

    base = (u8 *)(b * 0x2800 + 0x2770000);
    sys_irq_disable();
    off = 0;
    if (func_0000B52((u16 *)(base + off), a[1] << 8 | a[0], 0x2000000) != 0)
        goto fail;
    off += 2;
    if (func_0000B52((u16 *)(base + off), a[3] << 8 | a[2], 0x2000000) != 0)
        goto fail;
    off += 2;
    if (func_0000B52((u16 *)(base + off), a[5] << 8 | a[4], 0x2000000) != 0)
        goto fail;
    off += 2;
    for (i = 0; i < gfx_object_count; i++) {
        if (gfx_objects[i].image != 0) {
            for (j = 0; j < gfx_objects[i].image[2]; j++) {
                v = func_000014A(&gfx_objects[i], j);
                if (func_0000B52((u16 *)(base + off), (v & 0xff) << 8 | v >> 8, 0x2000000) != 0)
                    goto fail;
                off += 2;
            }
        }
    }
    for (i = 0; i < a[1]; i++) {
        for (j = 0; j < a[0]; j += 2) {
            v = gfx_photo_pixel(j, i);
            v |= gfx_photo_pixel(j + 1, i) << 8;
            if (func_0000B52((u16 *)(base + off), v, 0x2000000) != 0)
                goto fail;
            off += 2;
        }
    }
    sys_irq_enable();
    for (k = 0; k < 6; k++) {
        p = (int)&save.pet;
        o = 0xb4;
        if (*(u8 *)(k + p + o + 2) == 0)
            break;
    }
    save.pet.photo_slots[k] = b + 1;
    save_game();
    return 0;
fail:
    sys_irq_enable();
    return -1;
}

/*
 * Screen read-back for the photo: the colour index shown at (x, y) (the topmost
 * non-transparent object pixel + its palette base, else the frame-buffer byte); used by
 * album_photo_write to copy the screen into a photo.
 */
u8 gfx_photo_pixel(s16 x, s16 y)
{
    s16 i;
    s16 dx, dy;
    u8 c;
    struct GfxObject *e;

    for (i = gfx_object_count - 1; i >= 0; i--) {
        e = &gfx_objects[i];
        if (e->image == 0)
            continue;
        if (e->flags & 0x4000)
            continue;
        dy = y - e->y;
        if (dy < 0 || e->height <= dy)
            continue;
        dx = x - e->x;
        if (dx < 0 || e->width <= dx)
            continue;
        if (e->flags & 0x1000)
            return ((u8 *)&gfx_frame_buffer)[(u16)(x + ((y - 0x1c) << 7))];
        c = gfx_object_pixel_b(e, dx, dy);
        if (e->flags & 0x2000) {
            c = ~c;
            c %= e->image[2];
        }
        if (c != 0)
            return c + e->palette_base;
    }
    return 0;
}

/*
 * Pixel of object p at (x, y) (same as gfx_object_pixel).
 */
u8 gfx_object_pixel_b(struct GfxObject *p, u8 x, u8 y)
{
    struct ImageData *img;
    u8 t;

    img = (struct ImageData *)p->image;
    if (p->flags & 0x40) {
        t = x;
        x = y;
        y = t;
    }
    if (p->flags & 0x20)
        y = p->height - y - 1;
    if (p->flags & 0x10)
        x = p->width - x - 1;
    if ((s16)p->flags < 0)
        return img->data[p->pixel_offset + y * img->width + x + 4];
    if (x & 1)
        return img->data[p->pixel_offset + y * (img->width >> 1) + (x >> 1) + 4] >> 4;
    return img->data[p->pixel_offset + y * (img->width >> 1) + (x >> 1) + 4] & 0xf;
}

/*
 * Returns 0 (stub). ir_exchange calls it as f(&data, &len, &c) for the record to send in
 * the legacy IR session; like the transfer ir_transfer (`ret`) it is emptied out in this
 * firmware, so the outputs stay uninitialised.
 */
int ir_send_data_stub(void)
{
    return 0;
}

/*
 * Returns 0 (stub).
 */
int ir_stub_zero_c(void)
{
    return 0;
}

void func_25029E2(void)
{
}

/*
 * Returns 0 (stub).
 */
int ir_stub_zero_d(void)
{
    return 0;
}

/*
 * Returns 0x0 (stub).
 */
int ir_stub_zero_e(void)
{
    return 0;
}

/*
 * Returns 0x0 (stub).
 */
int ir_stub_zero_f(void)
{
    return 0;
}

void func_25029F0(void)
{
}

/*
 * Returns 0x0 (stub).
 */
int ir_stub_zero_g(void)
{
    return 0;
}

/*
 * Returns 0x0 (stub).
 */
int ir_stub_zero_h(void)
{
    return 0;
}

/*
 * Returns 0x0 (stub). The data source of the legacy IR exchange ir_test_exchange (same role as
 * ir_send_data_stub for ir_exchange).
 */
int ir_send_data_stub_b(void)
{
    return 0;
}
