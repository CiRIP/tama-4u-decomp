/*
 * Legacy IR packets: building requests and answers, item packets, storing attached files,
 * received diary pages and messages, plus small session helpers.
 *
 * Open: the end of ir_receive, or a file of its own (R17).
 *
 * Code 2503F2E..2504824, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/ir/ir_packets.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/town/town_places.h"
#include "proto/diary/diary_notebook.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/comm/comm_flash_buffers.h"
#include "proto/ir/ir_driver.h"
#include "proto/dl/dl_chara.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/owner/owner_profile.h"
#include "proto/spot/spot_reader.h"
#include "proto/comm/comm_exchange.h"
#include "flash.h"
#include "libc.h"

/*
 * Clamp code b: b <= 0x3E unchanged, else 0x13 / 0x1F by a.
 */
int ir_clamp_code(u8 a, u16 b)
{
    if (b <= 0x3e)
        return b;
    if (a == 0)
        return 0x13;
    return 0x1f;
}

/*
 * IR protocol (legacy): fill the outgoing 20-byte packet ir_tx_packet for the profile kind
 * (character kind, random values...).
 */
int ir_build_request_packet(int a)
{
    int z = a;

    if ((ir_session_flags & 1) == 0)
        goto other;
        ir_tx_packet.kind = 0xc;
        if (save.pet.x000 <= 5000) {
            if (chara_table[save.pet.x000].gender == 0)
                ir_tx_packet.role = (rand() >> 3) % 2 + 2;
            else
                ir_tx_packet.role = (rand() >> 3) % 2;
        } else {
            if (dl_chara_record(save.pet.chara_row + 0x77)->gender == 0)
                ir_tx_packet.role = (rand() >> 3) % 2 + 2;
            else
                ir_tx_packet.role = (rand() >> 3) % 2;
        }
        comm_game_result = ir_tx_packet.role;
        if (ir_session_flags & 4) {
            ir_tx_packet.arg_lo = 1;
            ir_tx_packet.result = 0x93;
            ir_tx_packet.result2 = 2;
            ir_tx_packet.points_hi = 8;
            ir_tx_packet.points_lo = 0xc;
            return 0;
        }
        ir_tx_packet.arg_lo = 0;
        ir_tx_packet.result = 0x8d;
        ir_tx_packet.result2 = 0;
        ir_tx_packet.points_hi = 3;
        ir_tx_packet.points_lo = 5;
        return 0;
other:
    if ((ir_rx_packet.owner_id_hi & 0xc0) == 0xc0)
        goto ok;
    return 7;
ok:
    ir_tx_packet.kind = 0xa;
    ir_tx_packet.role = (rand() >> 3) % 2;
    comm_game_result = ir_tx_packet.role;
    if (z == 0)
        ir_tx_packet.arg_lo = 1;
    else
        ir_tx_packet.arg_lo = 0;
    ir_tx_packet.result = 0xa8;
    ir_tx_packet.result2 = 2;
    ir_tx_packet.points_hi = 8;
    ir_tx_packet.points_lo = 0xc;
    return 0;
}

/*
 * IR protocol (legacy): fill the outgoing packet fields 0x0F..0x11 by mode.
 */
int ir_fill_packet_mode_fields(void)
{
    int r = 7;

    if (ir_session_flags & 1) {
        r = 0;
        if ((ir_session_flags & 4) != r) {
            ir_tx_packet.result2 = 2;
            ir_tx_packet.points_hi = 8;
            ir_tx_packet.points_lo = 12;
        } else {
            ir_tx_packet.result2 = 0;
            ir_tx_packet.points_hi = 3;
            ir_tx_packet.points_lo = 5;
        }
    }
    return r;
}

/*
 * Returns 7 (error, stub).
 */
int ir_stub_return7(void)
{
    return 7;
}

/*
 * IR protocol (legacy): answer packet kind 11 from the received packet ir_rx_packet.
 */
int ir_build_request_answer(void)
{
    struct IrPacket *p = &ir_rx_packet;

    if ((p->owner_id_hi & 0xc0) != 0xc0)
        return 7;
    comm_game_result = p->role;
    ir_tx_packet.kind = 11;
    ir_tx_packet.arg_lo = p->arg_lo;
    ir_tx_packet.result = 0xa8;
    ir_tx_packet.result2 = 0xff;
    ir_tx_packet.points_hi = 0xff;
    ir_tx_packet.points_lo = 0xff;
    return 0;
}

/*
 * IR protocol (legacy): build the item packet for connection kind 4 from the item file
 * comm_gift_item.
 */
int ir_build_item_packet(void)
{
    u8 buf[10];
    struct TamagoFile *p;
    int r;
    int n;

    if (comm_exchange_kind == 4) {
        p = (struct TamagoFile *)comm_gift_item;
        if ((s8)p->x04e < 0) {
            buf[0] = p->x04e;
            buf[1] = p->x04f;
            buf[2] = p->kind;
            buf[3] = p->sub_kind;
            buf[4] = p->x05a;
            buf[5] = p->x05b;
        } else {
            buf[0] = p->x04e;
            buf[1] = p->x04f;
            buf[2] = 0;
            buf[3] = p->sub_kind;
            buf[4] = 0;
            buf[5] = p->kind;
        }
    } else {
        buf[0] = 0;
        buf[1] = 0;
        buf[2] = 0;
        buf[3] = 0;
        buf[4] = 0;
        buf[5] = 0;
    }
    buf[9] = save.pet.gotchi_points;
    buf[8] = (save.pet.gotchi_points & 0xff00) >> 8;
    buf[7] = (save.pet.gotchi_points & 0xff0000) >> 16;
    buf[6] = (save.pet.gotchi_points & 0xff000000) >> 24;
    n = 10;
    sys_irq_disable();
    r = comm_flash_append((u16 *)buf, n);
    sys_irq_enable();
    if (r != 0)
        n = 0;
    return n;
}

/*
 * New screen with the icon bars and a full main-area picture b (frame a); its object is
 * ui_picture_screen_obj.
 */
void ui_new_screen_with_picture(int a, const u8 *b)
{
    ui_new_screen();
    ui_picture_screen_obj = gfx_object_create(0, 0x1c, 0x10, a, 0, b);
    func_00009A6();
}

/*
 * Load the visiting pet's pictures (visit_load_partner_pics(1)).
 */
void comm_load_visitor_pictures(int unused)
{
    visit_load_partner_pics(1);
}

/*
 * Write a buffer of len bytes from src into flash sector 0x86 (0x27E0000, 64 KB): erase
 * the sector (func_0000A54(base, first, last) with the AMD command set, interrupts disabled
 * around the whole operation because the flash is unreadable while busy) and program it
 * halfword by halfword (func_0000B52(dst, value, flash base)). Used for one of the
 * save/record banks (callers ir_store_attached_file; the record kind is read back through
 * flash_sector_86_item by F7/F14).
 */
void flash_write_sector_86(u16 *src, u16 len)
{
    u16 *dst;
    int i;

    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_GIFT, FLASH_SEC_GIFT);
    dst = (u16 *)&flash_sector_86_item;
    for (i = 0; i < len >> 1; i++, dst++)
        func_0000B52(dst, src[i], FLASH_BASE);
    sys_irq_enable();
}

/*
 * IR protocol (legacy): check that received/sent packet kinds pair up (8/9, ...).
 */
int ir_check_kinds_pair(void)
{
    int r;

    r = 7;
    switch (ir_rx_packet.kind) {
    case 8:
        if (ir_tx_packet.kind == 9)
            r = 0;
        break;
    case 9:
        if (ir_tx_packet.kind == 8)
            r = 0;
        break;
    }
    if (r != 0) {
        switch (ir_tx_packet.kind) {
        case 8:
            if (ir_rx_packet.kind == 9)
                r = 0;
            break;
        case 9:
            if (ir_rx_packet.kind == 8)
                r = 0;
            break;
        default:
            r = 7;
        }
    }
    if (r != 0)
        return 0;
    if (comm_exchange_kind == 5)
        r = ir_store_attached_file((int)(FLASH_COMM_RX + 0x5400));
    else if (comm_exchange_kind == 4)
        r = ir_store_attached_file((int)(FLASH_COMM_TX + 0x5428));
    return r;
}

int ir_store_attached_file(int a)
{
    u16 *p;
    u16 n, i, v;
    int k;

    if (a == -1)
        return 7;
    a += 0x114;
    if (a == -1)
        return 7;
    p = (u16 *)a;
    v = *p;
    n = v >> 8;
    n |= (v & 0xff) << 8;
    if (n == 0xffff)
        return 7;
    i = 0;
    if (n != 1) {
        p++;
        for (; i < n; i++) {
            v = *p;
            if (v == 0xffff)
                return 7;
            k = (v & 0xff00u) >> 8 | (v & 0xff) << 8;
            p = (u16 *)((u8 *)p + k);
            p++;
        }
    }
    p++;
    v = *p;
    k = v >> 8 | (v & 0xff) << 8;
    p++;
    flash_write_sector_86(p, k);
    return 0;
}

/* Pointer to the item (section 3 data) inside a received exchange record: p + u16 at
 * 0xE6 + 8 (comm_record_item). */
/*
 * Attached file inside a received packet (comm_record_item).
 */
struct TamagoFile *comm_attached_file(struct ExchangeRecord *p)
{
    return (struct TamagoFile *)comm_record_item(p);
}

/*
 * Animation script anim_item_get.
 */
void comm_show_item_get(void)
{
    anim_item_get();
}

/*
 * Animation script anim_now_printing.
 */
void comm_show_now_printing(void)
{
    anim_now_printing();
}

/*
 * Save with a short sound, keeping the button counters (tmr_t5_pause_restore(1), save_game).
 */
void save_with_short_sound(void)
{
    u8 a = btn_a_presses;
    u8 b = btn_b_presses;
    u8 c = btn_c_presses;

    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 1;
    tmr_t5_pause_restore(1);
    snd_wait_end();
    save_game();
    btn_a_presses = a;
    btn_b_presses = b;
    btn_c_presses = c;
}

/*
 * Does received record p allow the connection game comm_menu_choice (flag bits at +0x12)?
 */
int comm_peer_allows_game(u8 *p)
{
    int r = 0;
    u8 x;
    u8 *q = p;

    switch (comm_menu_choice) {
    case 1:
        if (p[0x12] & 0x10)
            r = 1;
        break;
    case 2:
        if ((p[0x12] & 0x10) != 0) {
            return 0;
        }
        if ((p[0x12] & 0xd) == 0)
            goto zero;
        switch (comm_submenu_choice) {
        case 2:
            x = q[4] & 0xc0;
            if (x == 0 || x == 0xc0)
                r = 1;
            else
                r = 0;
            break;
        case 3:
            r = 1;
            break;
        default:
            r = 0;
            break;
        }
        break;
    }
    return r;
zero:
    return 0;
}

/*
 * A random bit that is clear in a | b (low 8 bits): list the clear bits, shuffle them
 * (util_shuffle_u16) and return the first as a mask; 0 when none.
 */
u8 util_random_clear_bit(int a, int b)
{
    u16 list[12];
    u8 bit;
    int n;
    int i;

    a |= b;
    a = (u8)a;
    memset(list, 0, 12);
    bit = 1;
    n = 0;
    for (i = 0; i <= 7; i++) {
        if (!(a & bit)) {
            list[n] = i;
            n++;
        }
        bit <<= 1;
    }
    util_shuffle_u16(list, n);
    bit = 0;
    if (n > 0) {
        bit = 1;
        bit <<= list[0];
    }
    return bit;
}

/*
 * Returns 0 (stub).
 */
int ir_stub_return0(void)
{
    return 0;
}

/*
 * Write a chunk-type-1 header and the friend data into the outgoing flash buffer.
 */
int ir_write_owner_profile_chunk(void)
{
    u8 buf[100];
    int total = 0;
    int n;

    buf[0] = total;
    buf[1] = total;
    buf[2] = total;
    buf[3] = 1;
    n = 4;
    comm_flash_append((u16 *)buf, n);
    total = n;
    save.pet.profile_bday_month = save.pet.owner_bday_month;
    save.pet.profile_bday_day = save.pet.owner_bday_day;
    n = 0x48;
    if (comm_flash_append((u16 *)save.pet.ir_owner_profile, n) != 0)
        n = 0;
    total += n;
    n = 0x80 - total;
    memset(buf, 0, 100);
    if (comm_flash_append((u16 *)buf, n) != 0)
        n = 0;
    total += n;
    return total;
}

/*
 * Write the friend's message record (mail_find) into the outgoing buffer.
 */
int ir_write_message_chunk(void)
{
    u8 hdr[4];
    u8 tmp[8];
    u8 buf[100];
    int len = 0;
    int n;

    if (comm_friend_is_new != 1 && mail_find(comm_partner_slot, 1, buf, -1, 0) != -1) {
        hdr[0] = 0;
        hdr[1] = 0;
        hdr[2] = 0;
        hdr[3] = 1;
        n = 4;
        comm_flash_append((u16 *)hdr, n);
        len += n;
        n = 100;
        comm_flash_append((u16 *)buf, n);
        return len + n;
    }
    hdr[0] = 0;
    hdr[1] = 0;
    hdr[2] = 0;
    hdr[3] = 0;
    n = 4;
    comm_flash_append((u16 *)hdr, n);
    return n;
}

int ir_store_received_diary(u8 mode, int b)
{
    u8 buf[0x7c];
    u8 *p;
    u32 w;
    u16 n, i;
    int r;

    p = (u8 *)b;
    if (b == -1) {
        r = 7;
        goto out;
    }
    p += 0x114;
    if ((int)p == -1) {
        r = 7;
        goto out;
    }
    w = *(u16 *)p;
    n = ((w & 0xff00) >> 8) | ((w & 0xff) << 8);
    if (n == 0xffff) {
        r = 7;
        goto out;
    }
    if (n != 1) {
        p += 2;
        for (i = 0; i < n; i++) {
            w = *(u16 *)p;
            if (w == 0xffff) {
                r = 7;
                goto out;
            }
            p += ((w & 0xff00) >> 8) | ((w & 0xff) << 8);
            p += 2;
        }
    }
    if (mode == 1) {
        w = *(u16 *)p;
        if ((((w & 0xff00) >> 8) | ((w & 0xff) << 8)) != 0) {
            p += 2;
            w = *(u16 *)p;
            p += ((w & 0xff00) >> 8) | ((w & 0xff) << 8);
            p += 2;
        }
    }
    p += 3;
    if (*p == 0)
        p += 1;
    else
        p += 0x65;
    p++;
    r = *p;
    if (r == 0)
        goto out;
    p++;
    if (diary_can_write())
        diary_store_page((int)(p + 2));
    r = diary_writing_scene();
out:
    return r;
}

int ir_store_received_message(u8 mode, int b)
{
    u8 *p;
    u16 n, i, v;
    u32 buf[31];
    u32 *bp;
    int k, r;

    comm_mail_queue.friend0 = 0xff;
    comm_mail_queue.friend1 = 0xff;
    comm_mail_queue.friend2 = 0xff;
    p = (u8 *)b;
    if (b == -1) {
        r = 7;
        goto out;
    }
    k = mail_find(comm_partner_slot, 1, buf, -1, 0);
    if (k != -1) {
        buf[0] = comm_partner_slot;
        buf[1] = 0;
        mail_store(comm_partner_slot, k, 0, (int)buf, 0);
    }
    p += 0x114;
    if ((int)p == -1) {
        r = 7;
        goto out;
    }
    v = *(u16 *)p;
    n = v >> 8;
    n |= (v & 0xff) << 8;
    if (n == 0xffff) {
        r = 7;
        goto out;
    }
    if (n != 1) {
        p += 2;
        for (i = 0; i < n; i++) {
            v = *(u16 *)p;
            if (v == 0xffff) {
                r = 7;
                goto out;
            }
            k = (v & 0xff00u) >> 8 | (v & 0xff) << 8;
            p = p + k;
            p += 2;
        }
    }
    if (mode == 1) {
        v = *(u16 *)p;
        k = v >> 8 | (v & 0xff) << 8;
        if (k != 0) {
            p += 2;
            v = *(u16 *)p;
            k = (v & 0xff) << 8 | v >> 8;
            p = p + k;
            p += 2;
        }
    }
    p += 3;
    r = *p;
    if (r == 0)
        goto out;
    p++;
    bp = buf;
    memcpy(bp, p, 100);
    comm_mail_queue.friend0 = comm_partner_slot;
    bp[0] = comm_mail_queue.friend0;
    bp[1] = 1;
    mail_store(comm_mail_queue.friend0, -1, 0, (int)bp, 1);
    r = 0;
out:
    return r;
}

/*
 * If the diary page counter matches visit_diary_page_mark, write the pending diary page.
 */
void diary_write_pending_page(void)
{
    if ((u8)diary_page_count() == visit_diary_page_mark) {
        visit_load_partner_pics(0);
        diary_write_comm_page();
    }
}

/*
 * The RAM variables this file defines.
 */
u8 ui_picture_screen_obj;
u8 comm_game_result DSTRAM;
u8 visit_reward_pic_idx DSTRAM;
u8 ir_peer_has_code DSTRAM;
u8 comm_exchange_kind DSTRAM;
u8 comm_friend_is_new DSTRAM;
u8 comm_is_initiator DSTRAM;
u8 comm_partner_slot DSTRAM;
u8 ir_exchange_flag DSTRAM;
