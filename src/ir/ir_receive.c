/*
 * Receiving over IR: finding the marker and feeding received bytes, the directory and status
 * words, applying points, partner pictures, friend events and their minute check, stamp peers,
 * item checks and the packet-kind decisions of the exchange.
 *
 * Code 2502DCE..2503EFC, .rodata from 253BEAC.
 */
#include "proto/ir/ir_receive.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/sys/sys_boot.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/comm/comm_flash_buffers.h"
#include "proto/ir/ir_packets.h"
#include "proto/ir/ir_driver.h"
#include "proto/ui/ui_menu.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/comm/comm_outcome.h"
#include "proto/comm/comm_exchange.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * F16 = the receive side of the old IR exchange of the earlier models (see
 * notes/README.md item 6) plus the friend-list bookkeeping shared with the NFC path.
 * ir_rx_find_marker / ir_rx_feed stream incoming bytes into the 4 KB RAM buffer gfx_frame_buffer and
 * flush it to the flash bank selected by the transfer kind ir_block_kind (ir_rx_store_block); the
 * stream is resynchronised on the "TAMAGO" marker D_253BEB0 (state ir_rx_hunting: 1 = hunting
 * for the marker, 2 = partially matched, 0 = in the record; ir_marker_matched = matched bytes).
 *
 * ir_rx_find_marker: scan n bytes at a for the "TAMAGO\0" marker (the match state carries over
 * between calls); on a full match copy the marker into gfx_frame_buffer, set *o = index after it
 * and return 0; 7 = not (yet) found.
 */
int ir_rx_find_marker(int a, u16 n, int o)
{
    char buf[10];
    u8 *p;
    u16 i = *(u16 *)o;
    int j;

    p = (u8 *)a;
    if (ir_rx_hunting == 1) {
        memset(buf, 0, 10);
        strcpy(buf, D_253BEB0);
        ir_marker_matched = 0;
        for (i = 0; i < n; i++) {
            if (*p++ == (u8)buf[ir_marker_matched]) {
                ir_marker_matched++;
                ir_rx_hunting = 2;
                i++;
                for (j = 1; j <= 5; j++, i++) {
                    if (i >= n)
                        break;
                    if (*p++ == (u8)buf[ir_marker_matched]) {
                        if (j == 5) {
                            ir_rx_hunting = 0;
                            memset(&gfx_frame_buffer, 0, 10);
                            strcpy((char *)&gfx_frame_buffer, D_253BEB0);
                            i++;
                            break;
                        }
                    } else {
                        ir_rx_hunting = 1;
                        break;
                    }
                    ir_marker_matched++;
                }
                if (ir_rx_hunting == 0 || i >= n)
                    break;
                ir_rx_hunting = 1;
                ir_marker_matched = 0;
            }
        }
    } else if (ir_rx_hunting == 2) {
        memset(buf, 0, 10);
        strcpy(buf, D_253BEB0);
        for (i = 0; i < n; i++) {
            if (*p++ == (u8)buf[ir_marker_matched]) {
                if (ir_marker_matched == 5) {
                    ir_rx_hunting = 0;
                    memset(&gfx_frame_buffer, 0, 10);
                    strcpy((char *)&gfx_frame_buffer, D_253BEB0);
                    i++;
                    break;
                }
                ir_marker_matched++;
                ir_rx_hunting = 2;
                i++;
                for (j = ir_marker_matched; j <= 5; j++, i++) {
                    if (i >= n)
                        break;
                    if (*p++ == (u8)buf[ir_marker_matched]) {
                        if (j == 5) {
                            ir_rx_hunting = 0;
                            memset(&gfx_frame_buffer, 0, 10);
                            strcpy((char *)&gfx_frame_buffer, D_253BEB0);
                            i++;
                            break;
                        }
                    } else {
                        ir_rx_hunting = 1;
                        break;
                    }
                    ir_marker_matched++;
                }
                if (ir_rx_hunting == 0 || i >= n)
                    break;
                ir_rx_hunting = 1;
                ir_marker_matched = 0;
            }
        }
    }
    if (ir_rx_hunting == 0) {
        *(u16 *)o = i;
        return 0;
    }
    return 7;
}

const char D_253BEB0[] = "TAMAGO";


/*
 * Receive-buffer feeder: append the b received bytes at a to the 4 KB RAM buffer gfx_frame_buffer;
 * c == 2 = end of transfer (flush the remaining ir_rx_buf_count bytes to flash, ir_rx_store_block).
 * While hunting, find the marker first (ir_rx_find_marker) and choose the destination bank
 * comm_flash_write_ptr from the transfer kind ir_block_kind (3 = the game bank 0x27C8000 / 0x27A0400 /
 * 0x27D0400, 6/7 = the profile+image bank 0x27CB800 / 0x27A5400 / 0x27D5400, by game mode
 * comm_exchange_kind and ir_session_flags bits 0-1); when the buffer would overflow, program it
 * (ir_rx_store_block) and restart at gfx_frame_buffer. ir_rx_status = 1 on a flash error.
 */
void ir_rx_feed(u8 *a, u16 b, u8 c)
{
    u8 pad[12];
    u16 n;
    int r;

    if (c == 2) {
        if (ir_rx_hunting == 0 && ir_rx_buf_count != 0) {
            r = ir_rx_store_block((u16 *)&gfx_frame_buffer, ir_rx_buf_count + 1);
            if (r != 0)
                ir_rx_status = 1;
        }
        return;
    }
    n = 0;
    if (ir_rx_hunting != 0) {
        if (ir_rx_find_marker((int)a, b, (int)&n) == 7)
            return;
        a += n;
        b -= n;
        ir_rx_buf_ptr += 6;
        ir_rx_buf_count = 6;
        switch (ir_block_kind & 0x7f) {
        case 0:
        case 1:
        case 2:
        case 4:
        case 5:
            break;
        case 3:
            comm_flash_write_ptr = (u32)(comm_exchange_kind == 0 ? ((u8 *)(FLASH_COMM_TX + 0x8000)) : (ir_session_flags & 3) ? ((u8 *)(FLASH_COMM_RX2 + 0x400)) : ((u8 *)(FLASH_COMM_RX + 0x400)));
            break;
        case 6:
        case 7:
            comm_flash_write_ptr = (u32)(comm_exchange_kind == 0 ? ((u8 *)(FLASH_COMM_TX + 0xB800)) : (ir_session_flags & 3) ? ((u8 *)(FLASH_COMM_RX2 + 0x5400)) : ((u8 *)(FLASH_COMM_RX + 0x5400)));
            break;
        }
    }
    if (ir_rx_hunting != 0)
        return;
    if ((u8 *)ir_rx_buf_ptr + b - (u8 *)&gfx_frame_buffer > 0xfff) {
        if (ir_rx_buf_count & 1) {
            *(u8 *)ir_rx_buf_ptr = *a++;
            b--;
            ir_rx_buf_count++;
        }
        ir_rx_buf_ptr = (u32)&gfx_frame_buffer;
        r = ir_rx_store_block((u16 *)&gfx_frame_buffer, ir_rx_buf_count);
        ir_rx_buf_count = 0;
        if (r != 0)
            ir_rx_status = 1;
    }
    ir_rx_buf_count += b;
    memcpy((void *)ir_rx_buf_ptr, a, b);
    ir_rx_buf_ptr += b;
}

/*
 * Reset the receive stream: ir_rx_buf_count = 0, ir_rx_hunting = 1 (hunt for the marker again).
 */
void ir_rx_reset(void)
{
    ir_rx_buf_count = 0;
    ir_rx_hunting = 1;
}

/*
 * Program a 16-byte directory entry idx at 0x27C0000 + 16 * idx: {b, 0, val big-endian
 * u32} (probably the transfer bookkeeping of the old IR exchange; the NFC path reuses the
 * same bank for its phase-1 record). ir_status_word_write writes a single halfword there.
 */
void ir_dir_entry_write(int idx, int unused, u32 val, u8 b)
{
    u8 buf[16];
    int r = 0;
    int i;
    u16 *p;

    sys_irq_disable();
    p = (u16 *)(FLASH_COMM_TX + idx * 16);
    buf[0] = b;
    buf[1] = 0;
    r = func_0000B52(p, *(u16 *)buf, FLASH_BASE);
    if (r != 0)
        return;
    p++;
    buf[0] = val >> 24;
    buf[1] = val >> 16;
    buf[2] = val >> 8;
    buf[3] = val;
    for (i = 0; i <= 1; i++, p++)
        if (func_0000B52(p, ((u16 *)buf)[i], FLASH_BASE) != 0)
            break;
    sys_irq_enable();
}

/*
 * Program the halfword v at 0x27C0000 (communication status word).
 */
void ir_status_word_write(u16 v)
{
    u16 buf[10];

    buf[0] = v;
    sys_irq_disable();
    func_0000B52((u16 *)FLASH_COMM_TX, buf[0], FLASH_BASE);
    sys_irq_enable();
}

/*
 * Enter a transfer (IR exchange, ir_exchange): RTC interrupt off (bit 0 of the RTC
 * control 0x301904), save and clear the T16 ch.4/5 interrupt enables pINT_E16T45
 * (ir_saved_timer_ints), wait for the key click and disable the buttons (btn_disable), take a
 * button snapshot (btn_history_init).
 */
void ir_xfer_enter(void)
{
    RTCINTMODE &= 0xfe;
    ir_saved_timer_ints = INT_E16T45;
    INT_E16T45 = 0;
    btn_disable();
    btn_history_init();
}

/*
 * Map a count x to a tier: 0..2 -> 1, 3..19 -> (x + 1) / 2, else 11 (friend-count tier?).
 */
/*
 * Map a count x to a tier: 0..2 -> 1, 3..19 -> (x + 1) / 2, else 11 (friend-count tier?).
 */

int f_250328C(int x) __asm__("ir_count_tier");
int f_250328C(int x)
{
    u32 v = (u8)x;

    if (v > 19)
        return 11;
    if (v <= 2)
        return 1;
    return (v + 1) >> 1;
}

/*
 * Apply the points part of an IR-exchange result: the peer header ir_rx_packet carries an
 * amount (u16 big-endian at x010/x011) and flags x00e. Game mode comm_exchange_kind 4: bit 0 = we
 * get the points (pet_add_points adds); mode 5: bit 2 = we get them (returns 7 = "show the
 * prize"), else bit 0 = we pay them (pet_spend_points). (Fable read mode 4 as photo? and 5 as
 * play; comm_outcome_gift in F66 sets 4 on the giving and 5 on the receiving side of a gift.)
 */
u8 ir_apply_points(void)
{
    u8 b[4];

    b[1] = ir_rx_packet.points_hi;
    b[0] = ir_rx_packet.points_lo;
    switch (comm_exchange_kind) {
    case 4:
        if (ir_rx_packet.result & 1)
            pet_add_points(*(u16 *)b);
        return 0;
    case 5:
        if (ir_rx_packet.result & 4) {
            pet_add_points(*(u16 *)b);
            return 7;
        }
        if (ir_rx_packet.result & 1)
            pet_spend_points(*(u16 *)b);
        return 0;
    }
    return 0;
}

/*
 * Leave a session: timer 5 periodic again (tmr_t5_standard_tick), 10 s menu timeout.
 */
void ir_session_leave(void)
{
    tmr_t5_standard_tick();
    ui_timeout = 10;
}


/*
 * Returns 0x0 (stub).
 */
int ir_stub_zero_a(void)
{
    return 0;
}


/*
 * Returns 0x0 (stub).
 */
int ir_stub_zero_b(void)
{
    return 0;
}


/*
 * friend_find_or_add(id_hi, id_lo, name) = find the friend with this owner id and 16-byte name
 * in the friend list friend_list (31 records of 0x16 bytes: x000 used flag, x001/x002 owner
 * id, x003 flag, x004..x013 name, x014 met-by bits (1 greeting / 2 other), x015 portrait
 * slot + 1; record 30 is the current visitor) and return its index with comm_friend_is_new = 0;
 * else add it in the first free record (comm_friend_is_new = 1, x015 = 0) and return that index,
 * or 31 when no record is free. Same as friend_lookup_or_add (F66), which additionally stores the
 * flag and the met-by bit (asm). Note: the legacy IR callers pass (low half, high half) of
 * the peer's owner id (packet bytes 2 / 4 & 0x3F) into (hi, lo), so x001 gets the low half
 * here, while F66 stores id >> 8 in x001 for NFC peers: the two paths store the halves in
 * opposite order.
 */
u8 friend_find_or_add(u8 hi, u8 lo, int c)
{
    u8 i, j, diff;

    comm_friend_is_new = 0;
    for (i = 0; i <= 30; i++) {
        if (save.friend_list[i].x000_b != 0) {
            diff = 0;
            for (j = 0; j <= 15; j++)
                if (save.friend_list[i].name[j] != ((u8 *)c)[j])
                    diff++;
            if (diff == 0 && save.friend_list[i].x001 == hi && save.friend_list[i].owner_id_lo == lo)
                return i;
        }
    }
    comm_friend_is_new = 1;
    for (i = 0; i <= 30; i++) {
        if (save.friend_list[i].x000_b == 0) {
            save.friend_list[i].x001 = hi;
            save.friend_list[i].owner_id_lo = lo;
            for (j = 0; j <= 15; j++)
                save.friend_list[i].name[j] = ((u8 *)c)[j];
            save.friend_list[i].met_flags[1] = 0;
            return i;
        }
    }
    return i;
}

/*
 * Locate the received partner's images in the bank 0x27D0400: the record's sub-list
 * lengths (big-endian u16 at 0x27D0516..) give the offset of its image data; unless this
 * is a greeting (comm_menu_choice == 2, which returns that address directly), copy 0xE00 bytes of
 * image + 0x80 bytes of profile to 0x27D3C00 so friend_portrait_store can file them as the
 * friend's portrait. Returns the pointer (minus 0x118, the portrait-record header size)
 * for friend_portrait_store.
 */
int ir_locate_partner_pics(void)
{
    u8 b[4];
    u8 *q0;
    u8 *q;
    u8 *dst;
    int sz;
    int i;
    int n;

    q0 = (u8 *)(FLASH_COMM_RX + 0x516);
    b[1] = *q0++;
    b[0] = *q0;
    n = (FLASH_COMM_RX + 0x402) + *(u16 *)b;
    if (comm_menu_choice == 2)
        return n;
    dst = (u8 *)(FLASH_COMM_RX + 0x3C00);
    sz = 0xe00;
    flash_program(dst, (u8 *)n + 0x118, sz);
    q = (u8 *)(FLASH_COMM_RX + 0x516);
    for (i = 0; i <= 2; i++) {
        b[1] = *q++;
        b[0] = *q++;
        n = *(u16 *)b;
        q += n;
    }
    flash_program(dst + sz, q + 0xe, 0x80);
    return (int)dst - 0x118;
}

/*
 * Write the built-in default partner image record into the save bank: erase sector 0x84,
 * then program at 0x27C8114 {0, 1} + the big-endian length 0xE70 + 0xE70 bytes from ROM
 * 0x215C920, with interrupts off (flash_begin / flash_end). Nothing is received or
 * verified here.
 */
void comm_write_default_partner_image(void)
{
    u8 hdr[2];
    u16 crc;
    u16 len;
    u8 *dst;
    u8 *src;
    int off;
    int i;

    off = 0;
    flash_begin();
    func_0000A54(FLASH_BASE, FLASH_SEC_COMM_TX, FLASH_SEC_COMM_TX);
    dst = (u8 *)(FLASH_COMM_TX + 0x8114);
    hdr[0] = 0;
    hdr[1] = 1;
    src = hdr;
    for (i = off; i < 1; i++, off += 2)
        func_0000B52((u16 *)(dst + off), *(u16 *)(src + off), FLASH_BASE);
    dst += off;
    len = 0xe70;
    crc = util_read_be16((int)&len);
    src = (u8 *)&crc;
    for (i = 0, off = 0; i < sizeof(crc) / 2; i++, off += 2)
        func_0000B52((u16 *)(dst + off), *(u16 *)(src + off), FLASH_BASE);
    dst += off;
    src = D_215C920;
    for (i = 0, off = 0; i < len >> 1; i++, off += 2)
        func_0000B52((u16 *)(dst + off), *(u16 *)(src + off), FLASH_BASE);
    dst += off;
    flash_end();
}

u8 friend_event_hook_a(u8 a, u8 b)
{
}

/*
 * Daily scheduled events (MED): friend_event_minute_check, called by the clock, checks at fixed
 * times whether the corresponding bit of the event masks pet.friend_event_mask1 (kind 1) /
 * pet.friend_event_mask2 (kind 2), filtered by pet.friend_event_pending (still pending today), is set;
 * friend_event_pick picks the kind, friend_event_run_pending fires it and friend_event_fire
 * only calls the empty hook friend_event_hook_a and clears the bit. Nothing in this
 * firmware sets the masks (they are only tested and cleared here), so no event ever fires;
 * nothing ties them to visits.
 *
 * friend_event_pick: if bit m is pending in a, remember it in friend_event.kind1_bit (kind 1, from
 * pet.friend_event_mask1) or friend_event.kind2_bit (kind 2, from pet.friend_event_mask2) and return 1.
 */
u8 friend_event_pick(u8 a, u8 m)
{
    if (a & m) {
        if (m & save.pet.friend_event_mask1) {
            friend_event.kind1_bit = m;
            return 1;
        }
        if (m & save.pet.friend_event_mask2) {
            friend_event.kind2_bit = m;
            return 1;
        }
    }
    return 0;
}

/*
 * Fire the scheduled friend event of kind a (1 / 0), bit b, through the hook
 * friend_event_hook_a (an empty `ret` stub here), then clear the bit in
 * pet.friend_event_mask1 or pet.friend_event_mask2, in pet.friend_event_pending and in friend_event.
 */
int friend_event_fire(u8 a, u8 b)
{
    u8 r;

    r = friend_event_hook_a(a, b);
    if (r == 0xff)
        return;
    if (a == 1) {
        save.pet.friend_event_mask1 &= ~friend_event.kind1_bit;
        save.pet.friend_event_pending &= ~friend_event.kind1_bit;
        friend_event.kind1_bit = 0;
    } else {
        save.pet.friend_event_mask2 &= ~friend_event.kind2_bit;
        save.pet.friend_event_pending &= ~friend_event.kind2_bit;
        friend_event.kind2_bit = 0;
    }
}


/*
 * Run the pending scheduled friend event (visit / call) stored in friend_event with
 * friend_event_fire. It is not run (returns 0) when the status pet.status is 0, the state
 * room_view is 0/1/2, room_clock_notice and friend_event.blocked are not 1 and sys_stub_zero() == 0
 * (Fable read this as "the character is busy").
 */
u8 friend_event_run_pending(void)
{
    u8 a, v;

    if (friend_event.kind1_bit != 0) {
        a = 1;
        v = friend_event.kind1_bit;
    } else if (friend_event.kind2_bit != 0) {
        a = 0;
        v = friend_event.kind2_bit;
    } else
        return;
    if (save.pet.status == 0) {
        if (room_view == 0 || room_view == 2 || room_view == 1) {
            if (room_clock_notice != 1 && friend_event.blocked != 1 && sys_stub_zero() == 0)
                goto fail;
        }
    }
    if (friend_event_fire(a, v))
        return 1;
fail:
    return 0;
}

/*
 * Per-minute check of the scheduled friend events (pet.friend_event_mask1 / pet.friend_event_mask2 / pet.friend_event_pending
 * bitmasks): at 9:00, 11:00, 14:00, 14:30, 16:30, 17:00, 17:30 and 18:30 (hour/minute from
 * the RTC) bit 1, 2, 4, ... 0x80 is tried (friend_event_pick, friend_event_run_pending). Returns 1 when an
 * event is due and friend_event_run_pending did not run it, else 0.
 */
int friend_event_minute_check(void)
{
    u8 a = rtc_get_hour();
    u8 b = rtc_get_minute();
    u8 m;
    u8 k;

    m = save.pet.friend_event_mask2;
    m |= save.pet.friend_event_mask1;
    m &= save.pet.friend_event_pending;
    k = 1;
    if (a == 9 && b == 0 && friend_event_pick(m, k)) {
        if (friend_event_run_pending())
            return 0;
        return 1;
    }
    k = 2;
    if (a == 0xb && b == 0 && friend_event_pick(m, k)) {
        if (friend_event_run_pending())
            return 0;
        return 1;
    }
    k = 4;
    if (a == 0xe && b == 0 && friend_event_pick(m, k)) {
        if (friend_event_run_pending())
            return 0;
        return 1;
    }
    k = 8;
    if (a == 0xe && b == 0x1e && friend_event_pick(m, k)) {
        if (friend_event_run_pending())
            return 0;
        return 1;
    }
    k = 0x10;
    if (a == 0x10 && b == 0x1e && friend_event_pick(m, k)) {
        if (friend_event_run_pending())
            return 0;
        return 1;
    }
    k = 0x20;
    if (a == 0x11 && b == 0 && friend_event_pick(m, k)) {
        if (friend_event_run_pending())
            return 0;
        return 1;
    }
    k = 0x40;
    if (a == 0x11 && b == 0x1e && friend_event_pick(m, k)) {
        if (friend_event_run_pending())
            return 0;
        return 1;
    }
    k = 0x80;
    if (a == 0x12 && b == 0x1e && friend_event_pick(m, k)) {
        if (friend_event_run_pending())
            return 0;
        return 1;
    }
    return 0;
}

/*
 * File the portrait of the partner comm_partner_slot: for a real friend slot (< 30) back up its
 * portrait sector (friend_portrait_backup) and store the image record p (friend_portrait_store, p - 0x118);
 * a new friend (comm_friend_is_new) gets its record flag x000 set to 0xFF.
 */
void friend_portrait_file(int p)
{
    if (comm_partner_slot <= 29) {
        if (comm_friend_is_new)
            save.friend_list[comm_partner_slot].x000_b = 0xff;
        friend_portrait_backup(comm_partner_slot);
        friend_portrait_store(comm_partner_slot, (u8 *)(p - 0x118));
    } else {
        if (comm_friend_is_new)
            save.friend_list[comm_partner_slot].x000_b = 0xff;
    }
}

/*
 * File the portrait of friend comm_partner_slot (friend_portrait_backup, friend_portrait_store) with the images
 * located by ir_locate_partner_pics in the received record (game mode comm_exchange_kind > 2) or the default
 * bank 0x27C8000.
 */
void friend_portrait_file_received(void)
{
    u8 k = comm_partner_slot;

    friend_portrait_backup(k);
    if (comm_exchange_kind > 2)
        friend_portrait_store(comm_partner_slot, (u8 *)ir_locate_partner_pics());
    else
        friend_portrait_store(comm_partner_slot, (u8 *)(FLASH_COMM_TX + 0x8000));
}

/*
 * The full "add friend after an exchange": for a real slot (< 30) mark a new friend 0xFF and
 * file its portrait (friend_portrait_file_received); slot 30 (visitor) is only marked.
 */
void friend_add_after_exchange(void)
{
    if (comm_partner_slot < 30) {
        if (comm_friend_is_new != 0)
            save.friend_list[comm_partner_slot].x000_b = 0xFF;
        friend_portrait_file_received();
    } else {
        if (comm_friend_is_new != 0)
            save.friend_list[comm_partner_slot].x000_b = 0xFF;
    }
}

/*
 * Just mark friend comm_partner_slot as new (0xFF) after a communication, without filing a
 * portrait.
 */
void friend_mark_new(void)
{
    if (comm_partner_slot <= 30 && comm_friend_is_new)
        save.friend_list[comm_partner_slot].x000_b = 0xff;
}

/*
 * Map a count x to a tier: 0..2 -> 1, 3..19 -> (x + 1) / 2, else 11 (friend-count tier?).
 */

/*
 * Locate the received partner's images in the bank 0x27D0400: the record's sub-list
 * lengths (big-endian u16 at 0x27D0516..) give the offset of its image data; unless this
 * is a greeting (comm_menu_choice == 2, which returns that address directly), copy 0xE00 bytes of
 * image + 0x80 bytes of profile to 0x27D3C00 so friend_portrait_store can file them as the
 * friend's portrait. Returns the pointer (minus 0x118, the portrait-record header size)
 * for friend_portrait_store.
 */

/*
 * visit_record_clear = clear the visitor record (friend slot 30 of friend_list: flag x000, owner
 * id x001/x002, x003, name x004..x013, met-by bits x014) and the pending-event bytes
 * friend_book_state.new_friend_waiting/x002/x004/x005/x006; comm_partner_slot = 0 (asm).
 */


void visit_record_clear(void)
{
    int i;
    u8 j;

    for (i = 30; i <= 30; i++) {
        save.friend_list[i].x000_b = 0;
        save.friend_list[i].x001 = 0;
        save.friend_list[i].owner_id_lo = 0;
        for (j = 0; j <= 15; j++)
            save.friend_list[i].name[j] = 0;
        save.friend_list[i].gender = 0;
        save.friend_list[i].met_flags[0] = 0;
    }
    friend_book_state.new_friend_waiting = 0;
    friend_book_state.x002 = 0;
    friend_book_state.x004 = 0;
    friend_book_state.x005 = 0;
    friend_book_state.x006 = 0;
    comm_partner_slot = 0;
}



/*
 * Returns (stub).
 */
void func_25039AA(void)
{
    comm_store_pending_friend();
}




/*
 * Legacy IR stamp card: clear the two extra peer records 31 / 32 after the 31-record friend
 * list (flag, owner id bytes, first 5 name bytes) and the counters pet.stamp_count /
 * pet.stamp_liked_count; the friend records 0..30 are untouched (asm).
 */
void ir_stamp_peers_clear(void)
{
    u8 i, j;
    u8 *p, *q;

    for (i = 31; i <= 32; i++) {
        save.friend_list[i].x000_b = 0;
        for (j = 0; j <= 4; j++)
            save.friend_list[i].name[j] = 0;
        save.friend_list[i].x001 = 0;
        save.friend_list[i].owner_id_lo = 0;
    }
    save.pet.stamp_count = 0;
    save.pet.stamp_liked_count = 0;
}

/*
 * Legacy IR stamp card: note the peer (owner id halves hi / lo, 5 name bytes at c) in one of
 * the two extra peer records 31 / 32 after the 31-record friend list, as pending (0xFE);
 * earlier pending records are cleared first. Returns 7 when the peer is already recorded
 * as settled, else the record index (33 = no free record) (asm).
 */
u8 ir_stamp_peer_register(u8 hi, u8 lo, int c)
{
    u8 i, j, diff, k;

    for (i = 31; i <= 32; i++) {
        k = save.friend_list[i].x000_b;
        if (k == 0)
            continue;
        if (k == 0xfe) {
            save.friend_list[i].x000_b = 0;
            for (j = 0; j <= 4; j++)
                save.friend_list[i].name[j] = 0;
            save.friend_list[i].x001 = 0;
            save.friend_list[i].owner_id_lo = 0;
            continue;
        }
        diff = 0;
        for (j = 0; j <= 4; j++)
            if (save.friend_list[i].name[j] != ((u8 *)c)[j])
                diff++;
        if (diff == 0 && save.friend_list[i].x001 == hi && save.friend_list[i].owner_id_lo == lo)
            return 7;
    }
    for (i = 31; i <= 32; i++) {
        if (save.friend_list[i].x000_b == 0) {
            save.friend_list[i].x001 = hi;
            save.friend_list[i].owner_id_lo = lo;
            for (j = 0; j <= 4; j++)
                save.friend_list[i].name[j] = ((u8 *)c)[j];
            save.friend_list[i].x000_b = 0xfe;
            return i;
        }
    }
    return i;
}

/*
 * Settle the pending (0xFE) stamp-card peer records 31 / 32 (0xFF) when ir_coupon_flag is
 * set: by comm_gift_is_special_coupon (we give the coupon 0x3AB8) or by
 * ir_stamp_card_result (the peer offered the coupon). Not a friend-list merge (asm).
 */
void ir_stamp_peers_confirm(void)
{
    u8 i;

    if (ir_coupon_flag != 0) {
        for (i = 31; i <= 32; i++)
            if (save.friend_list[i].x000_b == 0xfe)
                save.friend_list[i].x000_b = 0xff;
    }
}

/*
 * In game mode comm_exchange_kind == 4 (gift): is the item about to be given (comm_gift_item) the
 * special coupon 0x3AB8 (goods 0x81 / category 4 / sub-kind 1, id by item_get_id)?
 * ir_coupon_flag = 1 and returns 1 if so, else 0.
 */
int comm_gift_is_special_coupon(void)
{
    struct TamagoFile *p;
    u8 v;
    int r;
    int t;

    ir_coupon_flag = 0;
    if (comm_exchange_kind != 4)
        return 0;
    p = (struct TamagoFile *)comm_gift_item;
    if (p->x04e != 0x81)
        return 0;
    if (p->x04f != 4)
        return 0;
    v = p->sub_kind;
    if (v != 1)
        return 0;
    t = item_get_id(p);
    r = v;
    if (t == 0x3ab8)
        ir_coupon_flag = v;
    else
        r = 0;
    return r;
}

/*
 * Stamp-card result of an IR exchange in game mode comm_exchange_kind == 5 (the receiving side,
 * as in F66's item exchange; not a Touch Spot): ir_coupon_flag = the peer header flag x00e bit 3
 * ("coupon offered"); if we own no stamp card (item_inv_usable_count(item_inv_stamps) == 0) ->
 * ir_session_flags |= 0x20 and 7 (show "no stamp card"); after 3 plays today (pet.stamp_count) -> 7;
 * else ir_stamp_peer_register (asm) settles it, 7 with ir_session_flags |= 0x10 = retry. The flags
 * 0x10/0x20 of ir_session_flags select the messages of ir_stamp_card_messages.
 */
u8 ir_stamp_card_result(u8 a, u8 b, int c)
{
    u8 tmp[4];
    u8 t;
    u8 r;

    ir_session_flags &= ~0x30;
    if (ir_rx_packet.result & 8)
        ir_coupon_flag = 1;
    else
        ir_coupon_flag = 0;
    if (comm_exchange_kind != 5)
        return 0;
    if ((ir_rx_packet.result & 8) == 0)
        return 0;
    if (item_inv_usable_count(&item_inv_stamps) == 0) {
        t = ir_session_flags;
        ir_session_flags = t | 0x20;
        return 7;
    }
    if (save.pet.stamp_count > 2)
        return 7;
    r = ir_stamp_peer_register(a, b, c);
    if (r == 7) {
        t = ir_session_flags;
        ir_session_flags = t | 0x10;
        return 7;
    }
    return 0;
}

/* unreferenced */
const u8 D_253BED8[3][5] = {
    { 20, 40, 50, 50, 0xff },
    { 40, 30, 30, 30, 0xff },
    { 40, 30, 20, 20, 0xff },
};

/*
 * Item-kind whitelist for received items, by class b: the kind byte a must fall in the
 * built-in range (0x2B..0x2D meals, 0x2A..0x2C snacks, 0x1C..0x1D accessories, 0x1F) or
 * the "downloadable" range 0xB4/0xC8/0xD2/0xDE/0xE1..0xFE; 0 = ok, 7 = rejected. Classes
 * 6/7 (type 2 items) always fall through to 7.
 */
int item_kind_whitelist(int a, int b)
{
    switch (b) {
    case 0:
        if (a >= 0x2b && a <= 0x2d)
            return 0;
        if (a >= 0xb4 && a <= 0xfe)
            return 0;
        return 7;
    case 1:
        if (a >= 0x2a && a <= 0x2c)
            return 0;
        if (a >= 0xb4 && a <= 0xfe)
            return 0;
        return 7;
    case 2:
        if (a >= 0x1c && a <= 0x1d)
            return 0;
        if (a >= 0xc8 && a <= 0xfe)
            return 0;
        return 7;
    case 3:
        if (a >= 0xde && a <= 0xfe)
            return 0;
        return 7;
    case 4:
        if (a >= 0xe1 && a <= 0xfe)
            return 0;
        return 7;
    case 5:
        if (a == 0x1f)
            return 0;
        if (a >= 0xd2 && a <= 0xfe)
            return 0;
        return 7;
    }
    return 7;
}

/*
 * Validate the received item ir_rx_record of the old IR exchange: type 0x14 / category 2 is
 * rejected; the 6-bit value packed in bytes 0xFA (high nibble) / 0xFB (bits 6-7) must be
 * 0x31 (a protocol/format version); then the kind whitelist item_kind_whitelist by
 * type/category/sub-kind. 0 = ok, 7 = rejected.
 */
int ir_item_validate(void)
{
    struct TamagoFile *p = ir_rx_record;

    if (p->x04e == 0x14) {
        if (p->x04f == 2)
            return 7;
    }
    if (((p->format_hi >> 4) | ((p->format_lo & 0xc0) >> 2)) != 0x31)
        return 7;
    switch (p->x04e) {
    case 1:
        switch (p->x04f) {
        case 1:
            switch (p->sub_kind) {
            case 1:
                return item_kind_whitelist(p->kind, 0);
            case 2:
                return item_kind_whitelist(p->kind, 1);
            case 3:
                return item_kind_whitelist(p->kind, 1);
            }
        case 4:
            if (p->sub_kind == 1)
                return item_kind_whitelist(p->kind, 2);
        case 2:
            return item_kind_whitelist(p->kind, 3);
        case 6:
            return item_kind_whitelist(p->legacy_value, 4);
        case 7:
            return item_kind_whitelist(p->kind, 5);
        }
    case 2:
        switch (p->sub_kind) {
        case 1:
            return item_kind_whitelist(p->kind, 6);
        case 2:
            return item_kind_whitelist(p->kind, 7);
        }
        return 0;
    }
    return 0;
}

/*
 * Check the same packed version, here at 0x4C/0x4D of the received record ir_rx_record: 7
 * when it is <= 0x2B; 0 only when it is > 0x31 and bit 1 of byte 0xFF is set, else 7.
 */
int ir_check_rx_version(void)
{
    struct TamagoFile *p;

    if (((ir_rx_record->x04c_b >> 4) | ((ir_rx_record->x04d & 0xc0) >> 2)) <= 0x2b)
        return 7;
    p = ir_rx_record;
    if (((p->x04c_b >> 4) | ((p->x04d & 0xc0) >> 2)) > 0x31) {
        if (p->flags & 2)
            return 0;
    }
    return 7;
}

/*
 * Old IR exchange: does the peer's message type (ir_rx_packet.kind) pair with ours
 * (ir_tx_packet.kind)? 2 -> 3 greeting, 4 -> 5 marriage, 8 <-> 9 gift, 14 -> 15 proposal;
 * 0 ok, 7 mismatch.
 */
int ir_msg_pair_check(void)
{
    switch (ir_rx_packet.kind) {
    case 2:
        if (ir_tx_packet.kind == 3)
            return 0;
        return 7;
    case 4:
        if (ir_tx_packet.kind == 5)
            return 0;
        return 7;
    case 8:
        if (ir_tx_packet.kind == 9)
            return 0;
        return 7;
    case 9:
        if (ir_tx_packet.kind == 8)
            return 0;
        return 7;
    case 14:
        if (ir_tx_packet.kind == 15)
            return 0;
        return 7;
    default:
        return 7;
    }
}

/*
 * Next packet kind after the received one (ir_rx_packet.kind + 1): the answer to a received
 * type is type + 1; a 14 answered by our 15 triggers ir_store_received_letter(1) (write the partner
 * record to its bank). The computed k is never stored: only the 14/15 case has an effect.
 * Legacy IR session code (called from ir_exchange only).
 */
void ir_next_packet_kind(void)
{
    int k = ir_rx_packet.kind;

    switch (k) {
    case 0: k = 1; break;
    case 1: k = 2; break;
    case 2: k = 3; break;
    case 3: k = 4; break;
    case 4: k = 5; break;
    case 5: k = 6; break;
    case 6: k = 7; break;
    case 7: k = 8; break;
    case 8: k = 9; break;
    case 9: k = 10; break;
    case 10: k = 11; break;
    case 11: k = 12; break;
    case 12: k = 13; break;
    case 13: k = 14; break;
    case 14:
        if (ir_tx_packet.kind == 0xf)
            ir_store_received_letter(1);
        break;
    }
}

/*
 * The RAM variables this file defines.
 */
u8 ir_saved_timer_ints;
u8 ir_marker_matched;
struct FriendEvent friend_event;
u8 ir_coupon_flag;

