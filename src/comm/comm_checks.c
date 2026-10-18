/*
 * The receiver's checks on a received phase-2 record (attached item flag, no food to the app),
 * the peer phase check, counting a completed communication (with and without saving) and
 * storing the peer's mail.
 *
 * Code 25295E6..25298CC, .rodata from 25624E0. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/comm/comm_checks.h"
#include "proto/save/save_flash.h"
#include "proto/diary/diary_notebook.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_packets.h"
#include "proto/test/test_debug_chara.h"
#include "proto/spot/spot_events.h"
#include "proto/comm/comm_outcome.h"
#include "proto/nfc/nfc_session.h"
#include "flash.h"
#include "libc.h"

/*
 * Receiver check: if the received phase-2 record p carries an item section (directory
 * entry 0xE6 != 0), the item file inside (comm_attached_file = p + x0e6 + 8) must have flag
 * 0x10 (bit 4) in its byte 0xFF, else 0x4D. comm_outcome_record = p.
 */
int comm_check_item_flag(struct ExchangeRecord *p)
{
    comm_outcome_record = (u32)p;
    if (p->sect_item == 0)
        return 0;
    if ((comm_attached_file(p)->flags & 0x10) == 0)
        return 0x4d;
    return 0;
}

/*
 * Receiver check, session type 8 only (nfc_session.x003 == 8): refuse (0x4D) an attached
 * file (p->x0e6 != 0, comm_attached_file) that is a food item (0x81 / category 1) of kind
 * +0x50 == 1 with sub-kind +0x51 1 or 2. Ordinary meals have kind 2 and snacks 0x0D; in
 * the download pack kind 1 marks only the four bonus "fridge" foods of the bingo /
 * gashapon sets (notes/findings/tamago.md), which therefore cannot come in this way.
 */
int comm_check_app_food(struct ExchangeRecord *p)
{
    struct TamagoFile *q;

    if (nfc_session.x003 != 8)
        return 0;
    comm_outcome_record = (u32)p;
    if (p->sect_item == 0)
        return 0;
    q = comm_attached_file(p);
    if (q->x04e != 0x81)
        return 0;
    if (q->x04f != 1)
        return 0;
    if (q->sub_kind == 1) {
        if (q->kind == 1)
            return 0x4d;
    }
    if (q->sub_kind == 2) {
        if (q->kind == 1)
            return 0x4d;
    }
    return 0;
}

extern u8 comm_peer_ident[];
int comm_check_peer_phase(struct ExchangeRecord *p, u8 k)
{
    u8 *q = (u8 *)p;
    int i;
    int o;
    int r = 0;

    if (nfc_session.x002 == 1)
        goto out;
    if (k == 1) {
        if (nfc_session.x003 == 1) {
            if (q[0x5e] != 0xe0)
                return 0x4d;
            return r;
        }
        if (q[0x5e] != 0)
            return 0x4d;
        for (i = 0; i <= 17; i++) {
            o = 0x60;
            comm_peer_ident[i] = *(u8 *)(i + (int)p + o);
        }
        comm_peer_id = p->owner_id;
        return r;
    } else if (k == 2) {
        if (q[0x5e] == 0)
            return 0x4d;
        if (nfc_session.x003 == 8) {
            if (q[0x5e] != 0x10)
                return 0x4d;
            return r;
        }
        for (i = 0; i <= 17; i++) {
            o = 0x60;
            if (comm_peer_ident[i] != *(u8 *)(i + (int)p + o))
                return 0x4d;
        }
        goto id;
    } else if (k == 3) {
        if (q[0x5e] != 0x90)
            return 0x4d;
        for (i = 0; i <= 17; i++) {
            o = 0x60;
            if (comm_peer_ident[i] != *(u8 *)(i + (int)p + o))
                return 0x4d;
        }
id:
        if (comm_peer_id != p->owner_id)
            return 0x4d;
        return r;
    } else {
        r = 0x4d;
    }
out:
    return r;
}

/*
 * Count a completed communication: pet.comm_count + 1 (max 1001; the count behind the house
 * levels of comm_house_level), comm_count_tenth (every 10th: pet.comm_traveller_due), then save_after_sound (wait
 * for the sound, save). Used by the NFC sessions and the idle reader touch.
 */
void comm_count_and_save(void)
{
    if (++save.pet.comm_count > 1001)
        save.pet.comm_count = 1001;
    comm_count_tenth();
    save_after_sound();
}

/*
 * Count a completed communication: pet.comm_count + 1 (max 1001), comm_count_tenth (every 10th:
 * pet.comm_traveller_due). Does not save: the callers (Touch Spot reader spot_reader, touch Bingo /
 * Gashapon spot_touch_any_reader) save after it.
 */
void comm_count_add(void)
{
    if (++save.pet.comm_count > 1001)
        save.pet.comm_count = 1001;
    comm_count_tenth();
}

/*
 * Apply a received session: find/add the peer as friend (friend_lookup_or_add with its id at
 * 0x27D0872/0x27D0860/0x27D085F) and store the 100-byte record from the received packet
 * (chunk at +0xE8) with mail_store. Returns 1 if stored.
 */
struct MailStoreBuf {
    u32 friend_idx;
    u32 direction;
    u8 pad[116];
};
int comm_store_peer_mail(void)
{
    struct MailStoreBuf s;
    int r;
    int res;
    struct ExchangeRecord *p;
    struct MailStoreBuf *bp;
    u8 *src;

    comm_mail_queue.friend0 = 0xff;
    comm_mail_queue.friend1 = 0xff;
    comm_mail_queue.friend2 = 0xff;
    if (comm_tx_message_chunk == 0)
        goto skip;
    comm_partner_slot = friend_lookup_or_add(comm_rx_phase2.owner_id, comm_rx_phase2.name, comm_rx_phase2.gender);
    res = mail_find(comm_partner_slot, 1, (void *)&s, -1, 0);
    if (res == -1)
        goto skip;
    s.friend_idx = comm_partner_slot;
    s.direction = 0;
    mail_store(s.friend_idx, res, 0, (int)(&s), 0);
    p = &comm_rx_phase2;
    goto go;
skip:
    p = &comm_rx_phase2;
go:
    r = 2;
    if (comm_rx_phase2.sect_mail != 0) {
        comm_partner_slot = friend_lookup_or_add(p->owner_id, p->name, p->gender);
        if (comm_friend_is_new != 1) {
            src = (u8 *)&comm_rx_phase2 + 8 + p->sect_mail;
            bp = &s;
            memcpy(bp, src, 100);
            s.friend_idx = comm_mail_queue.friend0 = comm_partner_slot;
            s.direction = 1;
            mail_store(s.friend_idx, -1, 0, (int)bp, 1);
            r = 1;
        }
    }
    return r;
}

/*
 * The RAM variables this file defines.
 */
u8 comm_peer_ident[18];
u16 comm_peer_id;
