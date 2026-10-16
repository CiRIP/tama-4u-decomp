/*
 * Pictures for the exchange records: a sample TAMAGO record (test), filling a record, the pose
 * and picture lists rendered into the record, plus the short IR pauses with animation and the
 * gift bonus scene.
 *
 * Code 2500910..2500C9A, .rodata from 253B24C.
 */
#include "proto/comm/comm_record_pics.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/pet/pet_core.h"
#include "proto/pet/pet_bath.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/pet/pet_gone.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/ir/ir_packets.h"
#include "proto/ir/ir_driver.h"
#include "proto/chara/chara_select.h"
#include "proto/comm/comm_exchange.h"
#include "flash.h"

/*
 * Program the built-in sample TAMAGO record test_sample_tamago (0x4B0 bytes, "t4u_gh230_1", a food
 * item?) into flash sector 0x85 (0x27D0000, the NFC receive area; erased first) and make it
 * the current item file comm_gift_item (the file an exchange sends as section 3). No caller: a
 * debug/test leftover.
 */
void test_program_sample_tamago(void)
{
    int i;
    u16 *q;

    sys_irq_disable();
    func_0000A54(0x2000000, 0x85, 0x85);
    q = comm_rx_phase1;
    for (i = 0; i <= 0x257; i++, q++)
        func_0000B52(q, test_sample_tamago[i], 0x2000000);
    comm_gift_item = (u32)comm_rx_phase1;
    sys_irq_enable();
}

/*
 * Fill record c (struct PictureSetHdr) with the data of kind a: 0 and 1 render the pet into it
 * (chara_render_pic_set); for 1 with bit 7 of ir_rx_packet.owner_id_hi set, pet.x062 / pet.x0cc /
 * pet.x148 are zeroed around the call so the pet is drawn without its sick / dirty looks
 * (see chara_draw_pet). 11 = comm_render_record_poses with the bank at 0x27D5400 (ir_rx_packet.owner_id_hi & 0xC0
 * == 0xC0) or 0x27D0400, 12/13 = bank 0x27A0400, others 0x27D0400. The 0x27x0400 banks are
 * the per-game save areas written by F16.
 */
void comm_fill_record(int a, int b, struct PictureSetHdr *c)
{
    switch (a) {
    case 1:
        if ((s8)ir_rx_packet.owner_id_hi < 0) {
            int s1 = save.pet.x062;
            u8 s2 = save.pet.x0cc;
            u8 s3 = save.pet.x148;
            save.pet.x062 = 0;
            save.pet.x0cc = 0;
            save.pet.x148 = 0;
            chara_render_pic_set(0, c);
            save.pet.x062 = s1;
            save.pet.x0cc = s2;
            save.pet.x148 = s3;
        } else {
            chara_render_pic_set(0, c);
        }
        break;
    case 0:
        chara_render_pic_set(0, c);
        break;
    case 11:
        if ((ir_rx_packet.owner_id_hi & 0xc0) == 0xc0) {
            comm_render_record_poses(a, (int)D_27D5400, (int)c);
            break;
        }
        comm_render_record_poses(a, (int)D_27D0400, (int)c);
        break;
    case 12:
    case 13:
        comm_render_record_poses(a, (int)D_27A0400, (int)c);
        break;
    default:
        comm_render_record_poses(a, (int)D_27D0400, (int)c);
        break;
    }
}

/*
 * Animation anim_comm_done, then pause (k & 0x7F) x 0.62 s (tmr_t5_pause(0x14) = 20 x 31 ms
 * on T16 ch.5) and restore the 3 Hz tick. Not IR (nothing is sent); used by the result
 * code of the IR exchange ir_exchange and by ir_session.
 */
void ir_anim_pause_a(u8 k)
{
    u8 i;

    anim_comm_done();
    for (i = 0; i < (k & 0x7f); i++)
        tmr_t5_pause(0x14);
    tmr_t5_standard_tick();
}

/*
 * As ir_anim_pause_a with the short animation anim_comm_wait: animation, then (k & 0x7F) x
 * 0.62 s pause. Not IR.
 */
void ir_anim_pause_b(u8 k)
{
    u8 i;

    anim_comm_wait();
    for (i = 0; i < (k & 0x7f); i++)
        tmr_t5_pause(0x14);
    tmr_t5_standard_tick();
}

/*
 * Identical to ir_anim_pause_b (animation anim_comm_wait + pause). Not IR.
 */
void ir_anim_pause_c(u8 k)
{
    u8 i;

    anim_comm_wait();
    for (i = 0; i < (k & 0x7f); i++)
        tmr_t5_pause(0x14);
    tmr_t5_standard_tick();
}

/*
 * Clear the IR exchange flags ir_session_flags.
 */
void ir_clear_flags(void)
{
    ir_session_flags = 0;
}

/*
 * Pause 0x19 x 31 ms = 0.77 s (tmr_t5_pause) and restart the 3 Hz tick; returns 1. Not IR
 * (nothing is sent). The argument the callers in ir_exchange pass is ignored.
 */
int ir_pause_short(void)
{
    tmr_t5_pause(0x19);
    tmr_t5_standard_tick();
    return 1;
}

/*
 * Connection kind 4: every 5th connection gives 500 points with an animation.
 */
void comm_gift_bonus(void)
{
    int n;

    if (comm_exchange_kind != 4)
        return;
    save.pet.gift_count++;
    if (save.pet.gift_count <= 4)
        return;
    visit_load_partner_pics(1);
    anim_clear_pic_slots2();
    save.pet.gift_count = 0;
    n = 500;
    anim_gift_bonus();
    anim_points_gain(n);
    pet_add_points(n);
    save_with_short_sound();
    visit_load_partner_pics(1);
    anim_clear_pic_slots2();
    anim_happy();
}

struct PoseList3 { u8 count; struct PoseEntry entries[3]; };

/*
 * Clear a pose list (the count and three {width, height, pose} entries).
 */
void comm_clear_pose_list(u8 *p)
{
    struct PoseList3 *h = (struct PoseList3 *)p;
    int i;

    for (i = 0; i <= 2; i++) {
        h->entries[i].w = 0;
        h->entries[i].h = 0;
        h->entries[i].pose = 0;
    }
    h->count = 0;
}

struct PoseList19 { u8 count; struct PoseEntry entries[19]; };
/*
 * Exchange record pictures: from the 0xFF-terminated pose numbers s, a pose list of up to
 * 15 entries {0x30 wide, 0x48 (mode 0) or 0x30 high, pose}, and render the pet in those
 * poses into picture records at flash address c (comm_render_picture_records mode 1).
 * Returns the bytes written. Used for section 0 (the portrait, pose 13).
 */
int comm_render_pose_list(int mode, u8 *s, int c)
{
    struct PoseList19 buf;
    struct PoseList19 *h;
    struct { int a, b; } r;
    int i;

    h = &buf;
    i = 0;
    comm_clear_pose_list((u8 *)h);
    r.a = 0;
    r.b = 0;
    if (*s != 0xff) {
        h->count = 0;
        for (; i <= 14; i++) {
            if (*s == 0xff)
                break;
            if (mode == 0) {
                h->entries[i].w = 0x30;
                h->entries[i].h = 0x48;
                h->entries[i].pose = *s;
            } else {
                h->entries[i].w = 0x30;
                h->entries[i].h = 0x30;
                h->entries[i].pose = *s;
            }
            h->count++;
            s++;
        }
        r.b = comm_render_picture_records((const struct PoseSet *)h, (u8 *)c, 1, 0);
    }
    return r.b;
}

struct PoseList13 { u8 count; struct PoseEntry entries[13]; };
/*
 * Exchange record pictures: modes 0 / 1 like comm_render_pose_list for up to 10 poses
 * (0x30 x 0x48 / 0x30 x 0x30, rendered with a count, comm_render_picture_records mode 0);
 * mode 2 one 0x80 x 0x48 picture of the room (mode 3). Returns the bytes written. Used for
 * sections 1 and 6 of the exchange record.
 */
int comm_render_poses_or_room(int mode, u8 *s, int c)
{
    struct PoseList13 buf;
    struct PoseList13 *h;
    struct { int a, b; } r;
    int i;

    h = &buf;
    i = 0;
    comm_clear_pose_list((u8 *)&buf);
    r.a = 0;
    r.b = 0;
    if (*s != 0xff) {
        buf.count = 0;
        for (; i <= 9; i++) {
            if (*s == 0xff)
                break;
            if (mode == 0) {
                h->entries[i].w = 0x30;
                h->entries[i].h = 0x48;
                h->entries[i].pose = *s;
                h->count++;
            } else if (mode == 1) {
                h->entries[i].w = 0x30;
                h->entries[i].h = 0x30;
                h->entries[i].pose = *s;
                h->count++;
            } else {
                h->count = 1;
                h->entries[0].w = 0x80;
                h->entries[0].h = 0x48;
                h->entries[0].pose = 0;
            }
            s++;
        }
        if (mode > 1)
            r.b = comm_render_picture_records((const struct PoseSet *)h, (u8 *)c, 3, 0);
        else
            r.b = comm_render_picture_records((const struct PoseSet *)h, (u8 *)c, 0, 0);
    }
    return r.b;
}

/*
 * Exchange record pictures of sections 14 / 15: the three pose numbers at record + 0x10F
 * rendered as 0x30 x 0x30 pet pictures, then the companion's two poses (when pet.companion_sprite_set
 * is set, mode 2); section 14 also copies the item record of flash sector 0x86 after them.
 * Other sections: the three poses only.
 */
int comm_render_record_poses(int k, int rec, int c)
{
    struct PoseList3 buf;
    struct PoseList3 *h = &buf;
    u8 *p;
    int i, r, n;
    u16 *src;
    int dest;
    int off, len;
    u16 v;

    i = 0;
    comm_clear_pose_list((u8 *)&buf);
    p = (u8 *)rec + 0x10f;
    r = i;
    if (k == 14 || k == 15) {
        if (*(u8 *)(rec + 0x10f) == 0xff)
            return r;
        buf.count = 3;
        for (i = 0; i <= 2; i++) {
            h->entries[i].w = 0x30;
            h->entries[i].h = 0x30;
            h->entries[i].pose = *p++;
        }
        switch (k) {
        case 14:
            r = comm_render_picture_records((const struct PoseSet *)&buf, (u8 *)(c), 1, 2);
            n = 3;
            comm_clear_pose_list((u8 *)&buf);
            if (save.pet.companion_sprite_set != 0) {
                buf.count = 2;
                buf.entries[0].w = 0x30;
                buf.entries[0].h = 0x30;
                buf.entries[0].pose = 0;
                buf.entries[1].w = 0x30;
                buf.entries[1].h = 0x30;
                buf.entries[1].pose = 1;
                r += comm_render_picture_records((const struct PoseSet *)&buf, (u8 *)(c + r), 2, 0);
                n = 5;
            }
            src = (u16 *)&flash_sector_86_item;
            dest = c + r;
            off = 0;
            v = *src;
            len = ((v << 8) & 0xff00) | (v >> 8);
            len += 2;
            sys_irq_disable();
            for (i = off; i < len; i += 2, off += 2) {
                if (func_0000B52((u16 *)(dest + off), *src, 0x2000000) != 0) {
                    sys_irq_enable();
                    return 0;
                }
                src++;
            }
            sys_irq_enable();
            r += len;
            n = n + 1;
            break;
        case 15:
            r = comm_render_picture_records((const struct PoseSet *)&buf, (u8 *)(c), 1, 0);
            n = 3;
            comm_clear_pose_list((u8 *)&buf);
            if (save.pet.companion_sprite_set != 0) {
                buf.count = 2;
                buf.entries[0].w = 0x30;
                buf.entries[0].h = 0x30;
                buf.entries[0].pose = 0;
                buf.entries[1].w = 0x30;
                buf.entries[1].h = 0x30;
                buf.entries[1].pose = 1;
                r += comm_render_picture_records((const struct PoseSet *)&buf, (u8 *)(c + r), 2, 1);
                n = 5;
            }
            break;
        default:
            return r;
        }
        comm_clear_pose_list((u8 *)&buf);
        buf.count = n;
        comm_render_picture_records((const struct PoseSet *)&buf, (u8 *)(c), 4, 0);
        return r;
    }
    if (*p == 0xff)
        return r;
    h->count = 3;
    for (i = 0; i <= 2; i++) {
        h->entries[i].w = 0x30;
        h->entries[i].h = 0x30;
        h->entries[i].pose = *p++;
    }
    r = comm_render_picture_records((const struct PoseSet *)&buf, (u8 *)(c), 0, 0);
    return r;
}

const u16 test_sample_tamago[0x258] = {
    0x4154, 0x414d, 0x4f47, 0x4400, 0x4c00, 0x5f00, 0x6900, 0x4400,
    0x4c00, 0x5300, 0x5f00, 0x5f30, 0x7e30, 0x5430, 0x4b30, 0x5130,
    0x5430, 0x6f30, 0x9330, 0x2e00, 0x6a00, 0x7000, 0x6700, 0x0000,
    0x0000, 0x4208, 0x6867, 0x3332, 0x5f30, 0x0031, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xb004, 0xc02d, 0x0681,
    0x0201, 0x7980, 0x0f86, 0x5661, 0x515a, 0xfb2a, 0x0000, 0x1f10,
    0x063c, 0x3c09, 0x2e1a, 0x0000, 0x0100, 0x0000, 0x6400, 0x0000,
    0x0a02, 0x0009, 0x8000, 0x0001, 0x4000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0300, 0x3601, 0x1818, 0x0008, 0xff01, 0xe007, 0x0060, 0x5f07,
    0x3b06, 0xffff, 0x6dfe, 0x6ff6, 0x48cc, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x1100, 0x0011, 0x0000, 0x0000, 0x0000,
    0x0000, 0x2211, 0x1123, 0x0000, 0x0000, 0x0000, 0x1000, 0x3324,
    0x4233, 0x0001, 0x0000, 0x0000, 0x2100, 0x2233, 0x3322, 0x0014,
    0x0000, 0x0000, 0x3410, 0x3223, 0x2322, 0x0142, 0x0000, 0x0000,
    0x2210, 0x2324, 0x4243, 0x0143, 0x0000, 0x0000, 0x3441, 0x2432,
    0x3432, 0x1444, 0x0000, 0x0000, 0x4441, 0x4234, 0x4233, 0x1444,
    0x0000, 0x0000, 0x4411, 0x4444, 0x4444, 0x1144, 0x0000, 0x0000,
    0x1151, 0x4441, 0x1444, 0x1611, 0x0000, 0x0000, 0x6651, 0x1116,
    0x6111, 0x1666, 0x0000, 0x0000, 0x6510, 0x6666, 0x6666, 0x0166,
    0x0000, 0x0000, 0x6510, 0x6666, 0x6666, 0x0176, 0x0000, 0x0000,
    0x5100, 0x6666, 0x6666, 0x0017, 0x0000, 0x0000, 0x1000, 0x6666,
    0x7666, 0x0001, 0x0000, 0x0000, 0x0000, 0x1111, 0x1111, 0x0000,
    0x0000, 0x0000, 0x0000, 0x6661, 0x1776, 0x0000, 0x0000, 0x0000,
    0x0000, 0x1110, 0x0111, 0x0000, 0x0000, 0x3801, 0x1818, 0x0009,
    0xff01, 0xe007, 0x0060, 0xffff, 0x5f07, 0x3b06, 0x3cd7, 0x6ff6,
    0x6dfe, 0x48cc, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1111, 0x0000,
    0x0000, 0x0000, 0x0000, 0x1000, 0x4132, 0x0001, 0x0000, 0x0000,
    0x0000, 0x3100, 0x1344, 0x0014, 0x0000, 0x0000, 0x0000, 0x4210,
    0x1334, 0x0154, 0x0000, 0x0000, 0x0000, 0x3310, 0x3432, 0x1151,
    0x0011, 0x0000, 0x0000, 0x4221, 0x3243, 0x6141, 0x0166, 0x0000,
    0x0000, 0x2221, 0x2342, 0x1214, 0x1666, 0x0000, 0x0000, 0x2211,
    0x2222, 0x2152, 0x1161, 0x0000, 0x0000, 0x1171, 0x2221, 0x1522,
    0x1611, 0x0000, 0x0000, 0x6671, 0x1116, 0x6111, 0x1666, 0x0000,
    0x0000, 0x6710, 0x6666, 0x6666, 0x0166, 0x0000, 0x0000, 0x6710,
    0x6666, 0x6666, 0x0186, 0x0000, 0x0000, 0x7100, 0x6666, 0x6666,
    0x0018, 0x0000, 0x0000, 0x1000, 0x6666, 0x8666, 0x0001, 0x0000,
    0x0000, 0x0000, 0x1111, 0x1111, 0x0000, 0x0000, 0x0000, 0x0000,
    0x6661, 0x1886, 0x0000, 0x0000, 0x0000, 0x0000, 0x1110, 0x0111,
    0x0000, 0x0000, 0x3801, 0x1818, 0x0009, 0xff01, 0xe007, 0x0060,
    0x5f07, 0x3b06, 0xffff, 0x6ff6, 0x3cd7, 0x48cc, 0x6dfe, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x1000, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x2100, 0x0013,
    0x0000, 0x0000, 0x0000, 0x0000, 0x3410, 0x1121, 0x0111, 0x0000,
    0x0000, 0x0000, 0x2210, 0x5131, 0x1555, 0x0011, 0x0000, 0x0000,
    0x1441, 0x1226, 0x5555, 0x0155, 0x0000, 0x0000, 0x1441, 0x3343,
    0x5551, 0x1555, 0x0000, 0x0000, 0x1411, 0x4346, 0x7771, 0x1155,
    0x0000, 0x0000, 0x1181, 0x6661, 0x1716, 0x1511, 0x0000, 0x0000,
    0x5581, 0x1115, 0x5111, 0x1555, 0x0000, 0x0000, 0x5810, 0x5555,
    0x5555, 0x0155, 0x0000, 0x0000, 0x5810, 0x5555, 0x5555, 0x0175,
    0x0000, 0x0000, 0x8100, 0x5555, 0x5555, 0x0017, 0x0000, 0x0000,
    0x1000, 0x5555, 0x7555, 0x0001, 0x0000, 0x0000, 0x0000, 0x1111,
    0x1111, 0x0000, 0x0000, 0x0000, 0x0000, 0x5551, 0x1775, 0x0000,
    0x0000, 0x0000, 0x0000, 0x1110, 0x0111, 0x0000, 0x0000, 0x0000,
};

