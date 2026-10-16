/*
 * The record exchange: building the header and sections (mail, profile, images, poses,
 * proposal, item), the payloads per session type, the three phases, and the initiator / target
 * exchange.
 *
 * Code 252AB30..252C57C, .rodata from 2562558.
 */
#include "proto/comm/comm_exchange.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/pet/pet_core.h"
#include "proto/diary/diary_notebook.h"
#include "proto/sys/sys_boot.h"
#include "proto/evo/evo_special.h"
#include "proto/comm/comm_menu.h"
#include "proto/comm/comm_record_pics.h"
#include "proto/ir/ir_packets.h"
#include "proto/chara/chara_select.h"
#include "proto/test/test_debug_chara.h"
#include "proto/owner/owner_profile.h"
#include "proto/comm/comm_outcome.h"
#include "proto/comm/comm_validate.h"
#include "proto/comm/comm_checks.h"
#include "proto/nfc/nfc_session.h"
#include "proto/comm/comm_record_sum.h"
#include "proto/lib/nfc/nfc_reader.h"
#include "flash.h"
#include "libc.h"

/*
 * F70b = the Tamagotchi-to-Tamagotchi NFC exchange: builds our "communication record" in
 * RAM at comm_tx_record (a 0x100-byte TAMAGO-style header + sections), writes it to the send
 * banks in flash (0x27C0000 phase 1, 0x27C0800 phase 2, 0x27C0400 phase 3), sends it with
 * nfc_send and receives the peer's record with nfc_receive into the receive banks
 * (0x27D0000 / 0x27D0800 / 0x27D0400; the greeting, session type 2, stays in RAM:
 * comm_rx_record, 0x180 bytes). Session type = nfc_session.x003, written into every record as
 * byte 0x4C (one numbering: the menu choice passed to nfc_menu_session is mapped to it there,
 * see F69 and notes/findings/sessions.md): 0 = automatic (the idle toy as target adopts
 * the initiator's 0x4C, comm_exchange_target), 1 = factory link test (0x2000 bytes of
 * 0x55AA/0xAA55, test mode only), 2 = greeting / profile exchange, 3/4 = the
 * communication game (round 1 / round 2 with the summed scores), 5 = give an item,
 * 6 = proposal / marriage, 7 = commemorative photo, 8 = the app download link (target =
 * the toy in "4Uアプリ", initiator = the phone app, or a toy in the debug "アイテムSEND"
 * playing the app's part); 9/10 only have message codes in comm_phase2_code and are never set.
 * The outcome handlers are in F66 (comm_run_outcome). Role x002: 1 = initiator
 * (comm_exchange_initiator), 2 = target (comm_exchange_target).
 *
 * Record header (offsets, the same TAMAGO magic as the item files but a little-endian
 * container): 0x00 "TAMAGO", 0x48 total length as a native (little-endian) u32 - the
 * builders do *(u32 *)(comm_tx_record + 0x48) += n and flash_program_bytes copies the header to flash
 * unchanged, whereas the downloadable item files store it big-endian (checked on the 916
 * files of the download pack, notes/findings/tamago.md) - 0x4C session type, 0x4D 4 (0 in
 * the bare type-8 initiator record), 0x52..0x59 signature (fixed constants D_2562564 / D_2562570, checked by
 * comm_validate_record with the permutation tables D_2562458 / D_25623D8 keyed by byte 0x55 - the
 * same scheme signs the downloadable items), 0x5E message code (phase 1: 0/1 or 0xE0/E1 or
 * 0xA0/A1, phase 2: comm_phase2_code 0x20..0x29 / 0x10..0x13, phase 3: 0x90/0x91; initiator
 * even, target odd), 0x5F the gender byte (pet_get_gender), 0x60 owner name (16 bytes,
 * pet.owner_name), 0x72 owner id (pet.owner_id, a random u16 drawn at character creation), 0x74
 * character id (pet.x000), 0x76 seconds the target must wait before its own
 * animation (so both screens stay in step; F66 waits 0x76 x 10 x 100 ms), 0x78.. growth
 * fields (st->x00f, x00c, pet.generation, life stage pet.life_stage, the five pet.traits.. stats),
 * 0x84 record version (0, 1 or 3 accepted), 0x88 species group (chara_family), 0x8A/0x8B
 * pet.owner_bday_month/D_00011E0, 0x8C half length of the proposal text, 0xB0 game parameter
 * (x01a/x01c), 0xB4 wish list of the character poses we want the peer to render for us
 * (count, ids, 0xFF), 0xC8 given-item descriptor (type, cat, kind, sub, id, 0xCF 1/2 =
 * give/take money 0xD0), 0xD8 our money (pet.gotchi_points), 0xE0..0xEC section directory
 * (u16 offsets of the sections of type 0..6), 0xF8 flags (0x02 bare type-8 initiator
 * record, 0x10 Tamagotchi record), 0xFF bit 0 = valid.
 * Sections follow at 0x100, each {u32 type, u16 version, u16 length} + data, 4-aligned:
 * 0 = rendered character images (comm_render_pose_list: w 0x30 x h 0x48 bitmaps of the poses in a
 * list) + at +0xE08 the 0x80-byte profile, 1 = rendered poses from the peer's wish list
 * (comm_render_poses_or_room), 2 = profile (owner_profile_bytes 4 bytes + pet.owner_profile_texts 0x50 bytes + 2), 3 = an item
 * (a whole TAMAGO file, comm_gift_item), 4 = a mail record (100 bytes, mail_find kind 1),
 * 5 = the proposal text, 6 = more images. The transfer ends with a 16-bit additive
 * checksum (comm_record_sum), written as a native halfword by flash_fill / util_fill16,
 * i.e. LITTLE-endian (an item file ends with a big-endian one).
 *
 * comm_check_codes_paired: peer record sanity: our message code 0x5E and the peer's must differ by
 * exactly 1 (the paired request / answer), else 2 = protocol mismatch.
 */
u8 comm_check_codes_paired(void)
{
    u8 r;

    if (((u8 *)comm_tx_record)[0x5e] == comm_rx_record[0x5e] + 1)
        r = 0;
    else if (((u8 *)comm_tx_record)[0x5e] + 1 == comm_rx_record[0x5e])
        r = 0;
    else
        r = 2;
    return r;
}

/*
 * Both records must carry the same session type (byte 0x4C): 0 ok, 2 mismatch.
 */
u8 comm_check_same_type(void)
{
    u8 r;

    if (((u8 *)comm_tx_record)[0x4c] == comm_rx_record[0x4c])
        r = 0;
    else
        r = 2;
    return r;
}

/*
 * Both checks: message codes paired (comm_check_codes_paired) and same type (comm_check_same_type): 0 ok,
 * 2 mismatch.
 */
u8 comm_check_pair_and_type(void)
{
    u8 r;

    r = comm_check_codes_paired();
    if (r == 2)
        return r;
    r = comm_check_same_type();
    if (r == 2)
        return r;
    return 0;
}

struct CommRecord {
    char magic[0x48];
    u32 length;
    u8 session_type;
    u8 version;
    u8 pad04e[4];
    char signature[0xd];
    u8 gender;
    u8 owner_name[0x10];
    u8 pad070[2];
    u16 owner_id;
    u16 chara_id;
    u16 wait_time;
    u8 care_mistakes;
    u8 happiness;
    u8 generation;
    u8 stage;
    u8 trait0;
    u8 trait1;
    u8 trait2;
    u8 trait3;
    u8 trait4;
    u8 x081;
    u8 x082;
    u8 x083;
    u8 pad084[2];
    u8 x086;
    u8 pad087;
    u16 family;
    u8 bday_month;
    u8 bday_day;
    u8 proposal_len;
    u8 pad08d[0x4b];
    u32 points;
    u8 pad0dc[4];
    u16 directory[8];
    u8 pad0f0[8];
    u8 flags[7];
    u8 complete;
};
#define R ((struct CommRecord *)comm_tx_record)
/*
 * Fill our record header comm_tx_record (see the file comment): magic "TAMAGO" (D_256255C),
 * length 0, session type x003 as byte 0x4C, signature 0x52, gender byte 0x5F
 * (pet_get_gender), section directory cleared, flags. Session type 8 as initiator
 * (x000 & 0xFFFF0000 == 0x08010000: x002 == 1, x003 == 8, the debug "アイテムSEND" playing
 * the app's part) builds a bare record with signature D_2562564 and flag 0x02; every other
 * session builds the full character header (owner name pet.owner_name, ids, growth fields
 * 0x72..0x8C, money 0xD8) with signature D_2562570 and flag 0x10. In session type 6 as
 * target (= the proposing side, する in comm_menu_proposal), a character of stage > 2 also
 * attaches its proposal line (text 4 of the character, chara_text) through
 * nfc_session.proposal_text / 0x8C.
 */
void comm_build_header(void)
{
    int i;

    if ((nfc_session.busy & 0xffff0000) == 0x8010000) {
        strcpy(R->magic, D_256255C);
        R->length = 0;
        R->session_type = nfc_session.x003;
        R->version = 0;
        strcpy(R->signature, D_2562564);
        R->gender = pet_get_gender();
        memset(R->owner_name, 0, 0x10);
        R->owner_id = 0;
        R->chara_id = 0;
        R->wait_time = 0;
        R->care_mistakes = 0;
        R->happiness = 0;
        R->generation = 0;
        R->stage = 0;
        R->trait0 = 0;
        R->trait1 = 0;
        R->trait2 = 0;
        R->trait3 = 0;
        R->trait4 = 0;
        R->x081 = 0;
        R->x082 = 0;
        R->x083 = 0;
        R->x086 = 0;
        R->family = 0;
        R->bday_month = 0;
        R->bday_day = 0;
        R->proposal_len = 0;
        nfc_session.proposal_text = 0;
        R->points = 0;
        for (i = 0; i < 8; i++)
            R->directory[i] = 0;
        memset(R->flags, 0, 8);
        R->flags[0] |= 2;
        R->complete |= 1;
    } else {
        u16 z;
        struct PetState *st;

        strcpy(R->magic, D_256255C);
        z = 0;
        R->length = 0;
        R->session_type = nfc_session.x003;
        R->version = 4;
        strcpy(R->signature, D_2562570);
        R->gender = pet_get_gender();
        memcpy(R->owner_name, save.pet.owner_name, 0x10);
        {
            struct CommRecord *p = R;

            st = &save.pet;
            p->owner_id = save.pet.owner_id;
        }
        R->chara_id = st->x000;
        R->wait_time = 0;
        R->care_mistakes = st->care_mistakes;
        R->happiness = st->happiness;
        R->generation = save.pet.generation;
        R->stage = save.pet.life_stage;
        R->trait0 = save.pet.traits[0];
        R->trait1 = save.pet.traits[1];
        R->trait2 = save.pet.traits[2];
        R->trait3 = save.pet.traits[3];
        R->trait4 = save.pet.traits[4];
        R->x081 = 0;
        R->x082 = 0;
        R->x083 = 0;
        R->x086 = 0;
        R->family = chara_family(R->chara_id);
        R->bday_month = save.pet.owner_bday_month;
        R->bday_day = save.pet.owner_bday_day;
        R->proposal_len = 0;
        nfc_session.proposal_text = 0;
        if (nfc_session.x003 == 6) {
            if (nfc_session.x002 == 2) {
                if (save.pet.life_stage > 2) {
                    u8 *s = chara_text(st->x000, 4, 0);
                    int n = strlen((char *)s);

                    R->proposal_len = (n + 1) / 2;
                    nfc_session.proposal_text = (u32)s;
                }
            }
        }
        R->points = save.pet.gotchi_points;
        for (i = 0; i < 8; i++)
            R->directory[i] = 0;
        memset(R->flags, 0, 8);
        R->flags[0] |= 0x10;
        R->complete |= 1;
    }
}

int f_2528DA2(u16, u8 *) __asm__("friend_lookup_or_add");
/*
 * Section 4 = mail: look the peer up in the friend list (friend_lookup_or_add by owner id 0x72 +
 * name 0x60 of the received record comm_rx_phase1 -> comm_partner_slot, comm_friend_is_new = 1 if new) and, for
 * a known friend that has a pending message (mail_find kind 1), append {4, 1, 100} +
 * the 100-byte message record at base + off (flash). Directory entry 0xE8. Returns the
 * bytes written (0 = nothing).
 */
int comm_build_section_mail(int base, int off)
{
    u8 buf[100];
    struct {
        u32 a;
        u16 b;
        u16 c;
    } s;
    int n = 0;
    int k;

    comm_partner_slot = f_2528DA2(comm_rx_peer_id, D_27D0060);
    if (comm_friend_is_new == 1)
        return n;
    if (mail_find(comm_partner_slot, 1, buf, -1, 0) == -1)
        return 0;
    base += off;
    s.a = 4;
    s.b = 1;
    s.c = 100;
    k = 8;
    if (flash_program_halfwords(base, (u16 *)&s, k) != 0)
        return 0;
    base += k;
    n += k;
    k = 100;
    if (flash_program_halfwords(base, (u16 *)buf, k) != 0)
        return 0;
    *(u16 *)((u8 *)comm_tx_record + 0xe8) = off;
    return n + k;
}

struct SectionHeader {
    u32 type;
    u16 version;
    u16 length;
};
/*
 * Section 2 = profile: {2, 1, 0x5E} + owner_profile_bytes (4 bytes) + pet.owner_profile_texts (0x50 bytes: five
 * 16-byte profile strings, see F47) + pet.owner_bday_month/D_00011E0[0]; written to RAM (flag 0) or
 * programmed into flash (flag 1, length 0x54, without the last two bytes). Directory
 * entry 0xE4. Returns the section size.
 */
int comm_build_section_profile(int base, int off, int flag)
{
    int p;
    int o;
    u8 buf[12];
    struct SectionHeader *h;

    p = base;
    o = off;
    if ((s8)flag == 0) {
        h = (struct SectionHeader *)(base + off);
        h->type = 2;
        h->version = 1;
        h->length = 0x5e;
        memcpy(h, h, 8);
        p = (int)h + 8;
        memcpy((void *)p, &save.pet.profile_gender, 4);
        p += 4;
        memcpy((void *)p, save.pet.owner_profile_texts, 0x50);
        p += 0x50;
        buf[0] = save.pet.owner_bday_month;
        buf[1] = save.pet.owner_bday_day;
        memcpy((void *)p, buf, 2);
    } else {
        h = (struct SectionHeader *)buf;
        h->type = 2;
        h->version = 1;
        h->length = 0x54;
        p += off;
        flash_program_bytes(8, p, buf);
        p += 8;
        flash_program_bytes(4, p, &save.pet.profile_gender);
        p += 4;
        flash_program_bytes(0x50, p, save.pet.owner_profile_texts);
    }
    *(u16 *)((u8 *)comm_tx_record + 0xe4) = o;
    return h->length + 8;
}

/*
 * Program the 0x80-byte profile blob (same fields as section 2, zero padded) at dst in
 * flash (used at section 0 + 0xE08). Returns 0x80, 0 on a write error.
 */
int comm_program_profile_blob(int dst)
{
    u8 buf[128];
    int n = 0;

    memset(buf, n, 0x80);
    memcpy(buf, &save.pet.profile_gender, 4);
    memcpy(buf + 4, save.pet.owner_profile_texts, 0x50);
    buf[0x54] = save.pet.owner_bday_month;
    buf[0x55] = save.pet.owner_bday_day;
    n = 0x80;
    if (flash_program_halfwords(dst, (u16 *)buf, n) != 0)
        n = 0;
    return n;
}

/*
 * Section 0 = our character's images: {0, 1, ...} header (6 bytes), then comm_render_pose_list
 * renders pose 13 (the portrait, 0x30 x 0x48) into flash at +4, and the 0x80-byte profile
 * blob at +0xE08. Directory entry 0xE0. (Returns the odd value &comm_mail_queue on success - the
 * callers only test != 0 and round; the real length is implied by the fixed layout.)
 */
int comm_build_section_images(int base, int off)
{
    u8 hdr[12];
    struct {
        u32 a;
        u16 b;
        u16 c;
    } s;
    int len = 0;
    int p = base + off;
    int n = 0;
    int r;

    s.a = len;
    s.b = 1;
    s.c = len;
    len = 6;
    if (flash_program_halfwords(p, (u16 *)&s, len) != n)
        return n;
    p += 4;
    hdr[0] = 13;
    hdr[1] = 0xff;
    r = comm_render_pose_list(0, hdr, p);
    if (r == 0)
        goto out;
    p = base + off + 0xe08;
    r = comm_program_profile_blob(p);
    if (r == 0)
        goto out;
    *(u16 *)((u8 *)comm_tx_record + 0xe0) = off;
    r = (int)&comm_mail_queue;
out:
    return r;
}

/*
 * Section 1 = rendered poses: a 4-byte header (type 1), then comm_render_poses_or_room(1, list, p)
 * renders every pose id in src (0xFF-terminated, 0x30 x 0x30 each) - normally the peer's
 * wish list copied from its record at 0xB4. Directory entry 0xE2. Returns the 4-byte
 * aligned size.
 */
int comm_build_section_poses(int base, int off, u8 *src)
{
    struct {
        u32 a;
        u16 b;
        u16 c;
    } s;
    int n = 0;
    int p = base + off;
    int len;

    s.a = 1;
    s.b = n;
    s.c = n;
    len = 4;
    if (flash_program_halfwords(p, (u16 *)&s, len) != n)
        return n;
    p += 4;
    n += 4;
    len = comm_render_poses_or_room(1, src, p);
    n += len;
    n += 3;
    *(u16 *)((u8 *)comm_tx_record + 0xe2) = off;
    return n & ~3;
}

/*
 * Section 6 = more images: like section 1 but rendered with comm_render_poses_or_room(2, src).
 * Directory entry 0xEC.
 */
int comm_build_section_images2(int base, int off, u8 *src)
{
    struct {
        u32 a;
        u16 b;
        u16 c;
    } s;
    int n = 0;
    int p = base + off;
    int len;

    s.a = 6;
    s.b = n;
    s.c = n;
    len = 4;
    if (flash_program_halfwords(p, (u16 *)&s, len) != n)
        return n;
    p += 4;
    n += 4;
    len = comm_render_poses_or_room(2, src, p);
    n += len;
    n += 3;
    *(u16 *)((u8 *)comm_tx_record + 0xec) = off;
    return n & ~3;
}

/*
 * Section 5 = the proposal text (nfc_session.proposal_text, at most 0x8C bytes). Directory entry
 * 0xEA.
 */
int comm_build_section_proposal(int base, int off)
{
    struct {
        u32 a;
        u16 b;
        u16 c;
    } s;
    int n = 0;
    int p = base + off;
    u8 *str;
    int n2;

    s.a = 5;
    s.b = 1;
    str = (u8 *)nfc_session.proposal_text;
    n2 = strlen(str);
    if (n2 > 200)
        s.c = 0x8c;
    else
        s.c = n2 + 4;
    if (flash_program_halfwords(p, (u16 *)&s, 8) != 0)
        return 0;
    n += 8;
    p += 8;
    if (flash_program_halfwords(p, (u16 *)str, s.c) != 0)
        return 0;
    n += s.c;
    n += 3;
    *(u16 *)((u8 *)comm_tx_record + 0xea) = off;
    return n & ~3;
}

/*
 * Section 3 = an item: {3, 1, size} + the whole TAMAGO file at comm_gift_item (size = its
 * big-endian u32 at 0x48, read byte by byte), programmed halfword-wise into the record in
 * flash with the IRAM routine func_0000C46. Directory entry 0xE6. This is how items
 * travel between two Tamagotchis (and, in session 8, from the phone app: comm_check_app_food
 * inspects the file of a received session-8 record). The file keeps its own
 * big-endian checksum and signature, which the receiver never checks (comm_check_item_flag only
 * tests its byte 0xFF bit 4).
 */
int comm_build_section_item(int base, int off)
{
    u8 tmp[4];
    struct {
        u32 a;
        u16 b;
        u16 c;
    } s;
    int n;
    u8 *p;
    u32 size;
    u8 *q;
    int r;

    n = 0;
    snd_wait_before_flash();
    p = (u8 *)comm_gift_item;
    nfc_session.item_file = (u32)p;
    q = p + 0x48;
    tmp[3] = *q++;
    tmp[2] = *q++;
    tmp[1] = *q++;
    tmp[0] = *q;
    size = *(u32 *)tmp;
    s.a = 3;
    s.b = 1;
    s.c = size;
    sys_irq_disable();
    if (func_0000C46(&s, base + off, 4, 0) != 0) {
        sys_irq_enable();
        return 0;
    }
    r = func_0000C46((void *)comm_gift_item, base + off + 8, (size + 1) / 2, 0);
    sys_irq_enable();
    if (r == 0) {
        n = (size + 1) / 2;
        n = n * 2 + 8;
        n = (n + 3) / 4 * 4;
    }
    *(u16 *)((u8 *)comm_tx_record + 0xe6) = off;
    return n;
}

/*
 * Type 5 (give item) as initiator: describe the item we give (comm_gift_item) at header
 * 0xC8.. (type, category, kind, sub-kind, id, no money transfer, attribute u32 from
 * 0x74..0x77 BE) and our money at 0xD8. The receiver applies 0xCF/0xD0 with comm_apply_points.
 */
void comm_describe_given_item(struct CommHeaderItem *d)
{
    struct TamagoFile *q;

    if (nfc_session.x003 == 5) {
        if (nfc_session.x002 == 1) {
            q = (struct TamagoFile *)comm_gift_item;
            d->item_type = q->x04e;
            d->item_category = q->x04f;
            d->item_kind = q->kind;
            d->item_subkind = q->sub_kind;
            d->item_id = item_id(q);
            d->money_flag = 0;
            d->money_amount = 0;
            d->item_attr = q->price_b0 | (q->price_b1 << 8) | (q->price_b2 << 16) | (q->price_b3 << 24);
            d->our_points = save.pet.gotchi_points;
        }
    }
}

extern void *memset(void *, int, u32);
struct TxRecordView {
    u8 pad00[0x76];
    s16 wait_time;
    u8 pad78[0x3c];
    u8 wish_list[20];
};
/*
 * Phase-2 payload builders, one per session type and role (selected by comm_build_payload); each returns
 * the total section bytes appended at base + off and fills our wish list at header 0xB4
 * for the peer's next record.
 *
 * comm_payload_gift_init: type 5 initiator: images (0) + the item (3) + mail (4); 0x76 = 8; wish
 * list 0, 2, 10.
 */
int comm_payload_gift_init(void *ctx, u8 *buf)
{
    u8 unused[20];
    int total = 0;
    int n;

    ((struct TxRecordView *)comm_tx_record)->wait_time = 8;
    n = comm_build_section_images((int)ctx, (int)buf);
    if (n != total) {
        n += 3;
        total = n & ~3;
        buf += total;
    }
    memset(((struct TxRecordView *)comm_tx_record)->wish_list, 0xff, 20);
    ((struct TxRecordView *)comm_tx_record)->wish_list[0] = 3;
    ((struct TxRecordView *)comm_tx_record)->wish_list[1] = 0;
    ((struct TxRecordView *)comm_tx_record)->wish_list[2] = 2;
    ((struct TxRecordView *)comm_tx_record)->wish_list[3] = 10;
    ((struct TxRecordView *)comm_tx_record)->wish_list[4] = 0xff;
    n = comm_build_section_item((int)ctx, (int)buf);
    if (n != 0) {
        n = (n + 3) & ~3;
        total += n;
        buf += n;
    }
    n = comm_build_section_mail((int)ctx, (int)buf);
    if (n != 0)
        total += (n + 3) & ~3;
    return total;
}

/*
 * Type 5 target: images (0) + the poses the peer wished (1, its list at 0xB4 of the
 * received phase-2 record 0x27D0800) + mail (4).
 */
int comm_payload_gift_target(int base, int off)
{
    u8 name[20];
    int p;
    int o;
    int total;
    int r;
    int i;

    total = comm_build_section_images(base, off);
    off += total;
    p = (int)&comm_rx_phase2;
    i = 0;
    o = 0xb4;
    do {
        name[i] = *(u8 *)(i + p + o + 1);
        i++;
    } while (i <= *(u8 *)(p + o));
    r = comm_build_section_poses(base, off, name);
    total += r;
    off += r;
    r = comm_build_section_mail(base, off);
    if (r != 0)
        total += (r + 3) & ~3;
    return total;
}

/*
 * Type 8 initiator (debug "アイテムSEND", the toy in the app's role): the item at
 * 0x2070000 (or the default record of the background bank D_26E0000 when that slot is
 * erased) as section 3; 0x76 = 0, version 0x84 = 3 (the receiver shows "name registered"
 * for version 3, comm_outcome_app).
 */
int comm_payload_app_init(int a, int b)
{
    u8 buf[20];
    int n = 0;
    u8 *p;

    *(u16 *)((u8 *)comm_tx_record + 0x76) = n;
    *(u16 *)((u8 *)comm_tx_record + 0x84) = 3;
    p = (u8 *)0x2070000;
    if (*p == 0xff)
        p = D_26E0000;
    comm_gift_item = (u32)p;
    n += comm_build_section_item(a, b);
    return n;
}

/*
 * Type 8 target ("4Uアプリ": the toy only acknowledges the app's item): no sections.
 */
int comm_payload_app_target(void)
{
    return 0;
}

/*
 * Type 6 initiator: poses 2, 8, 12 (1); wait time 0x76 = 0.8 x the peer's 0x8C (proposal
 * text length) + 16 s; a character of species group 0x15 / 0x3EE presents itself as the
 * special id 0x138B / 0x1B65 (0x74 and 0x88) when the peer's flag 0xF8 bit 0 is set; wish
 * list 8, 10, 12.
 */
int comm_payload_proposal_init(int base, int off)
{
    u8 buf[20];
    int n = 0;
    u16 k;
    int t;

    t = D_27D008C * 800 / 1000;
    *(u16 *)((u8 *)comm_tx_record + 0x76) = (u8)(t + 6);
    *(u16 *)((u8 *)comm_tx_record + 0x76) += 10;
    if (D_27D00F8 & 1) {
        k = chara_family(save.pet.x000);
        if (k == 0x15) {
            *(u16 *)((u8 *)comm_tx_record + 0x74) = 0x138b;
            *(u16 *)((u8 *)comm_tx_record + 0x88) = 0x138b;
        } else if (k == 0x3ee) {
            *(u16 *)((u8 *)comm_tx_record + 0x74) = 0x1b65;
            *(u16 *)((u8 *)comm_tx_record + 0x88) = 0x1b65;
        }
    }
    *((u8 *)comm_tx_record + 0x86) = 0;
    memset(buf, 0xff, 0x14);
    buf[0] = 2;
    buf[1] = 8;
    buf[2] = 12;
    n += (comm_build_section_poses(base, off, buf) + 3) & ~3;
    memset((u8 *)comm_tx_record + 0xb4, 0xff, 0x14);
    *((u8 *)comm_tx_record + 0xb4) = 3;
    *((u8 *)comm_tx_record + 0xb5) = 8;
    *((u8 *)comm_tx_record + 0xb6) = 10;
    *((u8 *)comm_tx_record + 0xb7) = 12;
    *((u8 *)comm_tx_record + 0xb8) = 0xff;
    return n;
}

/*
 * Type 6 target: the poses the peer wished (1) + our proposal text (5); the same special
 * ids as comm_payload_proposal_init.
 */
int comm_payload_proposal_target(int base, int off)
{
    u8 buf[20];
    int n = 0;
    int i;
    int r;
    u16 k;
    int p;
    int o;

    if (D_27D00F8 & 1) {
        k = chara_family(save.pet.x000);
        if (k == 0x15) {
            *(u16 *)((u8 *)comm_tx_record + 0x74) = 0x138b;
            *(u16 *)((u8 *)comm_tx_record + 0x88) = 0x138b;
        } else if (k == 0x3ee) {
            *(u16 *)((u8 *)comm_tx_record + 0x74) = 0x1b65;
            *(u16 *)((u8 *)comm_tx_record + 0x88) = 0x1b65;
        }
    }
    *((u8 *)comm_tx_record + 0x86) = 0;
    p = (int)&comm_rx_phase2;
    i = 0;
    o = 0xb4;
    do {
        buf[i] = *(u8 *)(i + p + o + 1);
        i++;
    } while (i <= *(u8 *)(p + o));
    r = (comm_build_section_poses(base, off, buf) + 3) & ~3;
    n += r;
    off += r;
    n += (comm_build_section_proposal(base, off) + 3) & ~3;
    return n;
}

/*
 * Types 3/4 initiator: type 3 just a wish list (0, 2, 9) with 0x76 = 11; type 4 images (0)
 * + mail (4), the same wish list, 0xB0 = nfc_session.score_initiator, 0x76 = 20.
 */
int comm_payload_game_init(int base, int off)
{
    int n = 0;
    int r = n;
    u8 m;
    u8 buf[20];

    m = nfc_session.x003;
    if (m == 3) {
        *(u16 *)((u8 *)comm_tx_record + 0x76) = 11;
        memset((u8 *)comm_tx_record + 0xb4, 0xff, 0x14);
        *((u8 *)comm_tx_record + 0xb4) = m;
        *((u8 *)comm_tx_record + 0xb5) = n;
        *((u8 *)comm_tx_record + 0xb6) = 2;
        *((u8 *)comm_tx_record + 0xb7) = 9;
        *((u8 *)comm_tx_record + 0xb8) = 0xff;
    } else {
        *(u16 *)((u8 *)comm_tx_record + 0x76) = 20;
        *(u16 *)((u8 *)comm_tx_record + 0xb0) = nfc_session.score_initiator;
        r = comm_build_section_images(base, off);
        n += r;
        off += r;
        memset((u8 *)comm_tx_record + 0xb4, 0xff, 0x14);
        *((u8 *)comm_tx_record + 0xb4) = 3;
        *((u8 *)comm_tx_record + 0xb5) = 0;
        *((u8 *)comm_tx_record + 0xb6) = 2;
        *((u8 *)comm_tx_record + 0xb7) = 9;
        *((u8 *)comm_tx_record + 0xb8) = 0xff;
        r = comm_build_section_mail(base, off);
        if (r != 0) {
            r = (r + 3) & ~3;
            n += r;
        }
    }
    n += r;
    return n;
}

/*
 * Types 3/4 target: type 3 the wished poses (1); type 4 images (0) + wished poses (1) +
 * mail (4), 0xB0 = nfc_session.score_target.
 */
int comm_payload_game_target(int base, int off)
{
    u8 buf[20];
    int n = 0;
    int i;
    int r;
    int p;
    int o;

    p = (int)&comm_rx_phase2;
    if (nfc_session.x003 == 3) {
        i = 0;
        while (i <= *(u8 *)(p + 0xb4)) {
            o = 0xb4;
            buf[i] = *(u8 *)(i + p + o + 1);
            i++;
        }
        n += (comm_build_section_poses(base, off, buf) + 3) & ~3;
    } else {
        *(u16 *)((u8 *)comm_tx_record + 0xb0) = nfc_session.score_target;
        r = (comm_build_section_images(base, off) + 3) & ~3;
        n += r;
        off += r;
        p = (int)&comm_rx_phase2;
        i = 0;
        o = 0xb4;
        do {
            buf[i] = *(u8 *)(i + p + o + 1);
            i++;
        } while (i <= *(u8 *)(p + o));
        r = (comm_build_section_poses(base, off, buf) + 3) & ~3;
        n += r;
        off += r;
        r = comm_build_section_mail(base, off);
        if (r != 0)
            n += (r + 3) & ~3;
    }
    return n;
}

/*
 * Type 7 initiator: images (0) + poses 2, 3, 9 (1) + pose 2 as section 6 + mail (4);
 * 0x76 = 0x1D; wish list 2, 3, 9.
 */
int comm_payload_photo_init(int base, int off)
{
    u8 buf[20];
    int n;
    int r;

    *(u16 *)((u8 *)comm_tx_record + 0x76) = 0x1d;
    n = 0;
    n += (comm_build_section_images(base, off) + 3) & ~3;
    off += n;
    memset(buf, 0xff, 0x14);
    buf[0] = 2;
    buf[1] = 3;
    buf[2] = 9;
    r = (comm_build_section_poses(base, off, buf) + 3) & ~3;
    n += r;
    off += r;
    memset((u8 *)comm_tx_record + 0xb4, 0xff, 0x14);
    *((u8 *)comm_tx_record + 0xb4) = 3;
    *((u8 *)comm_tx_record + 0xb5) = 2;
    *((u8 *)comm_tx_record + 0xb6) = 3;
    *((u8 *)comm_tx_record + 0xb7) = 9;
    *((u8 *)comm_tx_record + 0xb8) = 0xff;
    memset(buf, 0xff, 0x14);
    buf[0] = 2;
    buf[1] = 0xff;
    r = (comm_build_section_images2(base, off, buf) + 3) & ~3;
    n += r;
    off += r;
    r = comm_build_section_mail(base, off);
    if (r != 0)
        n += (r + 3) & ~3;
    return n;
}

/*
 * Type 7 target: images (0) + wished poses (1) + mail (4).
 */
int comm_payload_photo_target(int base, int off)
{
    u8 name[20];
    int p;
    int o;
    int total;
    int r;
    int i;

    total = comm_build_section_images(base, off);
    off += total;
    p = (int)&comm_rx_phase2;
    i = 0;
    o = 0xb4;
    do {
        name[i] = *(u8 *)(i + p + o + 1);
        i++;
    } while (i <= *(u8 *)(p + o));
    r = comm_build_section_poses(base, off, name);
    total += r;
    off += r;
    r = comm_build_section_mail(base, off);
    if (r != 0)
        total += (r + 3) & ~3;
    return total;
}

const char D_256255C[] = "TAMAGO";
/* two fixed record signatures (valid for comm_validate_record's check) */
const char D_2562564[] = "\x7c\x68\x48\x97\x75\xa6\x4e\x99";
const char D_2562570[] = "\xa0\xa5\x7b\xf2\x60\x93\x72\x53";

/*
 * Dispatch to the phase-2 payload builder for the current session type x003 and role x002 (see
 * comm_payload_gift_init).
 */
int comm_build_payload(int base, int off)
{
    int r = 0;

    switch (nfc_session.x003) {
    case 5:
        if (nfc_session.x002 == 1)
            r = comm_payload_gift_init(base, off);
        else
            r = comm_payload_gift_target(base, off);
        break;
    case 6:
        if (nfc_session.x002 == 1)
            r = comm_payload_proposal_init(base, off);
        else
            r = comm_payload_proposal_target(base, off);
        break;
    case 8:
        if (nfc_session.x002 == 1)
            r = comm_payload_app_init(base, off);
        else
            r = comm_payload_app_target();
        break;
    case 3:
    case 4:
        if (nfc_session.x002 == 1)
            r = comm_payload_game_init(base, off);
        else
            r = comm_payload_game_target(base, off);
        break;
    case 7:
        if (nfc_session.x002 == 1)
            r = comm_payload_photo_init(base, off);
        else
            r = comm_payload_photo_target(base, off);
        break;
    }
    return r;
}

/*
 * Build the PHASE-1 record (the header exchange). Type 1 (test-mode link test): code 0xE0/0xE1,
 * header + 0x2000 bytes of 0x55AA (initiator) / 0xAA55 (target) + checksum, in flash at
 * 0x27C0000. Type 2 (greeting): code 0xA0/0xA1, header + profile section (2) in RAM only.
 * Types 3..8: code 0 / 1, header + checksum at 0x27C0000. The length goes to 0x48 and the
 * additive checksum (comm_record_sum) is programmed right after the data.
 */
void comm_build_phase1(void)
{
    u16 crc;
    int n;
    u32 p;
    u8 *b;
    u8 *q;

    comm_build_header();
    switch (nfc_session.x003) {
    case 1:
        if (nfc_session.x002 == 1) {
            *((u8 *)comm_tx_record + 0x5e) = 0xe0;
            nfc_session.msg_code = *((u8 *)comm_tx_record + 0x5e);
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p = 0x27C0000;
            p += n;
            n = 0x2000;
            flash_fill(n, p, 0x55aa);
            p += n;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            n = 2;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            flash_program_bytes(0x100, 0x27C0000, (u8 *)comm_tx_record);
            comm_record_sum((u8 *)0x27C0000, *(u32 *)((u8 *)comm_tx_record + 0x48), &crc);
            flash_fill(n, p, crc);
        } else {
            *((u8 *)comm_tx_record + 0x5e) = 0xe1;
            nfc_session.msg_code = *((u8 *)comm_tx_record + 0x5e);
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p = 0x27C0000;
            p += n;
            n = 0x2000;
            flash_fill(n, p, 0xaa55);
            p += n;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            n = 2;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            flash_program_bytes(0x100, 0x27C0000, (u8 *)comm_tx_record);
            comm_record_sum((u8 *)0x27C0000, *(u32 *)((u8 *)comm_tx_record + 0x48), &crc);
            flash_fill(n, p, crc);
        }
        break;
    case 2:
        if (nfc_session.x002 == 1) {
            *((u8 *)comm_tx_record + 0x5e) = 0xa0;
            nfc_session.msg_code = *((u8 *)comm_tx_record + 0x5e);
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            n = comm_build_section_profile((int)comm_tx_record, n, 0);
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            n = 2;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            q = (u8 *)comm_tx_record;
            b = q;
            q += *(u32 *)(q + 0x48);
            comm_record_sum(b, *(u32 *)(b + 0x48), &crc);
            util_fill16(n, q - 2, crc);
        } else {
            *((u8 *)comm_tx_record + 0x5e) = 0xa1;
            nfc_session.msg_code = *((u8 *)comm_tx_record + 0x5e);
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            n = comm_build_section_profile((int)comm_tx_record, n, 0);
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            n = 2;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            q = (u8 *)comm_tx_record;
            b = q;
            q += *(u32 *)(q + 0x48);
            comm_record_sum(b, *(u32 *)(b + 0x48), &crc);
            util_fill16(n, q - 2, crc);
        }
        break;
    case 3: case 4: case 5: case 6: case 7: case 8:
        if (nfc_session.x002 == 1) {
            *((u8 *)comm_tx_record + 0x5e) = 0;
            nfc_session.msg_code = *((u8 *)comm_tx_record + 0x5e);
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p = 0x27C0000;
            p += n;
            n = 2;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            flash_program_bytes(0x100, 0x27C0000, (u8 *)comm_tx_record);
            comm_record_sum((u8 *)0x27C0000, *(u32 *)((u8 *)comm_tx_record + 0x48), &crc);
            flash_fill(n, p, crc);
        } else {
            *((u8 *)comm_tx_record + 0x5e) = 1;
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p = 0x27C0000;
            p += n;
            n = 2;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            flash_program_bytes(0x100, 0x27C0000, (u8 *)comm_tx_record);
            comm_record_sum((u8 *)0x27C0000, *(u32 *)((u8 *)comm_tx_record + 0x48), &crc);
            flash_fill(n, p, crc);
        }
        break;
    }
}

/*
 * Phase-2 message code by session type (initiator even / target odd): 3: 0x20/21,
 * 4: 0x22/23, 5: 0x24/25, 6: 0x28/29, 7: 0x26/27, 8: 0x10/11 (the app sends 0x10,
 * comm_check_peer_phase), 9-10: 0x12/13 (types 9/10 are never set by this firmware).
 */
int comm_phase2_code(void)
{
    int r = 0;

    if (nfc_session.x002 == 1) {
        switch (nfc_session.x003) {
        case 3: r = 0x20; break;
        case 4: r = 0x22; break;
        case 5: r = 0x24; break;
        case 6: r = 0x28; break;
        case 7: r = 0x26; break;
        case 8: r = 0x10; break;
        case 9:
        case 10: r = 0x12; break;
        }
    } else {
        switch (nfc_session.x003) {
        case 3: r = 0x21; break;
        case 4: r = 0x23; break;
        case 5: r = 0x25; break;
        case 6: r = 0x29; break;
        case 7: r = 0x27; break;
        case 8: r = 0x11; break;
        case 9:
        case 10: r = 0x13; break;
        }
    }
    return r;
}

/*
 * Build the PHASE-2 record at 0x27C0800 (types 3..8): header with the comm_phase2_code code,
 * the payload sections (comm_build_payload), the given-item descriptor (comm_describe_given_item, initiator)
 * and the checksum.
 */
void comm_build_phase2(void)
{
    u16 crc;
    u8 *d;
    int n;
    u32 p;

    comm_build_header();
    switch (nfc_session.x003) {
    case 3: case 4: case 5: case 6: case 7: case 8:
        if (nfc_session.x002 == 1) {
            d = (u8 *)comm_tx_record;
            d[0x5e] = comm_phase2_code();
            nfc_session.msg_code = *((u8 *)comm_tx_record + 0x5e);
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p = n + 0x27C0800;
            n = comm_build_payload(0x27C0800, n);
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p += n;
            comm_describe_given_item((struct CommHeaderItem *)comm_tx_record);
        } else {
            d = (u8 *)comm_tx_record;
            d[0x5e] = comm_phase2_code();
            nfc_session.msg_code = *((u8 *)comm_tx_record + 0x5e);
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p = n + 0x27C0800;
            n = comm_build_payload(0x27C0800, n);
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p += n;
        }
        n = 2;
        *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
        flash_program_bytes(0x100, 0x27C0800, (u8 *)comm_tx_record);
        comm_record_sum((u8 *)0x27C0800, *(u32 *)((u8 *)comm_tx_record + 0x48), &crc);
        flash_fill(n, p, crc);
        break;
    }
}

/*
 * Build the PHASE-3 record (the final acknowledgement, types 3..7) at 0x27C0400: header
 * only, code 0x90 (initiator) / 0x91 (target), checksum.
 */
void comm_build_phase3(void)
{
    u16 crc;
    int n;
    u32 p;

    comm_build_header();
    switch (nfc_session.x003) {
    case 3: case 4: case 5: case 6: case 7:
        if (nfc_session.x002 == 1) {
            *((u8 *)comm_tx_record + 0x5e) = 0x90;
            nfc_session.msg_code = *((u8 *)comm_tx_record + 0x5e);
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p = n + (u32)D_27C0400;
            n = 2;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            flash_program_bytes(0x100, 0x27C0400, (u8 *)comm_tx_record);
            comm_record_sum((u8 *)0x27C0400, *(u32 *)((u8 *)comm_tx_record + 0x48), &crc);
            flash_fill(n, p, crc);
        } else {
            *((u8 *)comm_tx_record + 0x5e) = 0x91;
            nfc_session.msg_code = *((u8 *)comm_tx_record + 0x5e);
            *(u32 *)((u8 *)comm_tx_record + 0x48) = 0;
            n = 0x100;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            p = n + (u32)D_27C0400;
            n = 2;
            *(u32 *)((u8 *)comm_tx_record + 0x48) += n;
            flash_program_bytes(0x100, 0x27C0400, (u8 *)comm_tx_record);
            comm_record_sum((u8 *)0x27C0400, *(u32 *)((u8 *)comm_tx_record + 0x48), &crc);
            flash_fill(n, p, crc);
        }
        break;
    }
}

/*
 * INITIATOR side of the exchange (returns 0 ok, 1 failed; nfc_result = driver code): wait
 * for the peer (nfc_find_peer_initiator; the id must be 0xF040 = Tamagotchi or 0xF0F0, but the
 * initiator detector nfc_detect_target only yields 0xF040 / 0xF0EF, so an initiator always
 * talks raw NFC-DEP to an F0 40 peer, the debug type-8 sender included), erase the
 * send / receive banks (comm_erase_banks, except in types 2/4), build and send the phase-1
 * record (nfc_send), receive theirs (nfc_receive into 0x27D0000, or RAM for type 2),
 * copy its header to comm_rx_record and verify it (comm_validate_record: "TAMAGO", length, checksum,
 * signature, flag; comm_check_pair_and_type: codes and the same session type 0x4C) - a bad record
 * sets x000_b = 0xF and releases the peer (nfc_request_abort). Remember the peer's id + name
 * (comm_remember_peer). Types 3..8: phase 2 (0x27C0800 -> 0x27D0800; the peer must still be
 * the same, comm_same_peer). Types 3..7: phase 3 (0x27C0400 -> 0x27D0400) and apply the received results / money
 * transfer (comm_apply_points). Each transfer has a 10 s timeout (n = 10).
 */
u8 comm_exchange_initiator(void)
{
    u8 pad[12];
    u16 k;
    int x;
    int n;

    k = 0;
    n = 10;
    nfc_result = nfc_find_peer_initiator(&k);
    if (nfc_result != 0)
        return 1;
    if (k != 0xf040 && k != 0xf0f0)
        return 1;
    if (nfc_session.x003 != 1)
        nfc_session.peer_kind = nfc_session.detected_peer;
    if (nfc_session.x003 != 2 && nfc_session.x003 != 4) {
        if (comm_erase_banks() != 0)
            return 1;
    }
    comm_build_phase1();
    switch (nfc_session.x003) {
    case 1:
        nfc_result = nfc_send(D_27C0000, *(u32 *)((u8 *)comm_tx_record + 0x48), n);
        if (nfc_result != 0)
            return 1;
        x = 0;
        nfc_result = nfc_receive((int)comm_rx_phase1, &x, n, 1, 0);
        if (nfc_result != 0)
            return 1;
        memcpy(comm_rx_record, (void *)0x27D0000, 0x104);
        if (comm_validate_record((struct TamagoFile *)0x27D0000) != 0)
            return 1;
        break;
    case 2:
        nfc_result = nfc_send((void *)comm_tx_record, *(u32 *)((u8 *)comm_tx_record + 0x48), n);
        if (nfc_result != 0)
            return 1;
        x = 0;
        nfc_result = nfc_receive((int)comm_rx_record, &x, n, 0, 0x180);
        if (nfc_result != 0)
            return 1;
        if (comm_validate_record(comm_rx_record) != 0 || comm_check_pair_and_type() != 0) {
            nfc_session.x000_b = 0xf;
            nfc_result = nfc_request_abort();
            return 1;
        }
        break;
    default:
        nfc_result = nfc_send(D_27C0000, *(u32 *)((u8 *)comm_tx_record + 0x48), n);
        if (nfc_result != 0)
            return 1;
        x = 0;
        nfc_result = nfc_receive((int)comm_rx_phase1, &x, n, 1, 0);
        if (nfc_result != 0)
            return 1;
        memcpy(comm_rx_record, (void *)0x27D0000, 0x104);
        if (comm_validate_record(comm_rx_record) != 0 || comm_check_pair_and_type() != 0) {
            nfc_session.x000_b = 0xf;
            nfc_result = nfc_request_abort();
            return 1;
        }
        break;
    }
    if (nfc_session.x003 != 4)
        comm_remember_peer(*(u16 *)(comm_rx_record + 0x72), comm_rx_record + 0x60);
    switch (nfc_session.x003) {
    case 1: case 2:
        break;
    case 3: case 4: case 5: case 6: case 7: case 8:
        comm_build_phase2();
        nfc_result = nfc_send(D_27C0800, *(u32 *)((u8 *)comm_tx_record + 0x48), n);
        if (nfc_result != 0)
            return 1;
        x = 0;
        nfc_result = nfc_receive((int)&comm_rx_phase2, &x, n, 1, 0);
        if (nfc_result != 0)
            return 1;
        memcpy(comm_rx_record, (void *)0x27D0800, 0x104);
        if (comm_validate_record((struct TamagoFile *)0x27D0800) != 0)
            return 1;
        if (comm_same_peer(*(u16 *)(comm_rx_record + 0x72), (int)(comm_rx_record + 0x60)) != 0)
            return 1;
        break;
    }
    switch (nfc_session.x003) {
    case 0: case 1: case 2: case 8:
        break;
    case 3: case 4: case 5: case 6: case 7:
        comm_build_phase3();
        nfc_result = nfc_send(D_27C0400, *(u32 *)((u8 *)comm_tx_record + 0x48), n);
        if (nfc_result != 0)
            return 1;
        x = 0;
        nfc_result = nfc_receive((int)D_27D0400, &x, n, 1, 0);
        if (nfc_result != 0)
            return 1;
        memcpy(comm_rx_record, (void *)0x27D0400, 0x104);
        if (comm_validate_record((struct TamagoFile *)0x27D0400) != 0)
            return 1;
        comm_apply_points((struct GiftPoints *)comm_rx_record);
        break;
    }
    return 0;
}


/*
 * TARGET side, the mirror of comm_exchange_initiator (returns 0 ok, 1 failed): receive the
 * initiator's phase-1 record first (0x27D0000 for a 0xF0F0 peer, else RAM; timeout 30 s
 * for a 0xF0F0 peer - ineffective: nfc_receive ignores it for the app and runs up to
 * 6 x 10 s SNEP sessions - 8 s in the automatic session type 0 (the idle toy), else 10 s),
 * verify it, and in type 0 adopt the sender's session type from its byte 0x4C - but refuse
 * types 4, 6 and 8 (nfc_result = 0x58, result 1): type 4 only follows a type-3 round, 6
 * (proposal) needs the proposer's own menu choice, 8 is reserved for the "4Uアプリ" menu.
 * Outside type 0 the target does not compare the received 0x4C with its own (only the
 * initiator does, comm_check_pair_and_type); comm_check_peer_phase checks the message codes. Then answer
 * with our phase-1 record (type 2: after comm_check_peer_phase checks the received one; others: the
 * received record is first copied to 0x27D0000). Types 3..8: phase 2 (receive 0x27D0800,
 * check with comm_check_peer_phase / comm_check_item_flag (the item section must carry a valid item) /
 * comm_check_app_food (type 8: no food items of kind 1), then send ours). Types 3..7: phase 3 +
 * money transfer. The comm_trace2(1) calls are progress trace points; comm_trace() the
 * trace reset.
 */
#define CHECK(e) for (;;) { int t = (e); r = 1; if (t != 0) goto end; break; }
#define CHECKD(e) for (;;) { int t = (s8)(nfc_result = (e)); r = 1; if (t != 0) goto end; break; }
int comm_exchange_target(void)
{
    u8 pad[12];
    volatile int x;
    volatile int r;
    int n = 10;
    int s;
    int k;

    comm_trace2(1);
    comm_trace();
    if (nfc_session.x003 == 1) {
        CHECK(comm_erase_banks());
        x = 0;
        CHECKD(nfc_receive((int)comm_rx_phase1, (int *)&x, n, 1, 0));
        memcpy(comm_rx_record, (void *)0x27D0000, 0x104);
        CHECK(comm_validate_record((struct TamagoFile *)0x27D0000));
        comm_build_phase1();
        CHECKD(nfc_send(D_27C0000, *(u32 *)((u8 *)comm_tx_record + 0x48), n));
    } else {
        x = 0;
        if (nfc_session.detected_peer == 0xf0f0) {
            CHECK(comm_erase_banks());
            n = 30;
            CHECKD(nfc_receive((int)comm_rx_phase1, (int *)&x, n, 1, 0x180));
            memcpy(comm_rx_record, (void *)0x27D0000, 0x180);
        } else {
            n = nfc_session.x003 == 0 ? 8 : 10;
            nfc_result = nfc_receive((int)comm_rx_record, (int *)&x, n, 0, 0x180);
            n = 10;
        }
        for (;;) { int t = nfc_result; r = 1; if (t != 0) goto end; break; }
        CHECK(comm_validate_record(comm_rx_record));
        if (nfc_session.x003 == 0) {
            if (comm_rx_record[0x4c] == 6) {
                nfc_result = 0x58;
                k = 1;
                goto out;
            } else if (comm_rx_record[0x4c] == 8) {
                nfc_result = 0x58;
                for (;;) { r = 1; goto end; }
            } else if (comm_rx_record[0x4c] == 4) {
                nfc_result = 0x58;
                k = 1;
                goto out;
            } else
                nfc_session.x003 = comm_rx_record[0x4c];
        }
        if (nfc_session.x003 == 2) {
            CHECK(comm_check_peer_phase((struct ExchangeRecord *)comm_tx_record, 1));
            comm_build_phase1();
            CHECKD(nfc_send((void *)comm_tx_record, *(u32 *)((u8 *)comm_tx_record + 0x48), n));
        } else {
            if (nfc_session.x003 != 4) {
                CHECK(comm_erase_banks());
            }
            flash_program_bytes(*(u32 *)(comm_rx_record + 0x48), (u32)comm_rx_phase1, comm_rx_record);
            CHECK(comm_check_peer_phase((struct ExchangeRecord *)comm_rx_record, 1));
            comm_build_phase1();
            CHECKD(nfc_send(D_27C0000, *(u32 *)((u8 *)comm_tx_record + 0x48), n));
        }
    }
    comm_trace2(1);
    switch (nfc_session.x003) {
    case 1: case 2:
        break;
    case 3: case 4: case 5: case 6: case 7: case 8:
        x = 0;
        CHECKD(nfc_receive((int)&comm_rx_phase2, (int *)&x, n, 1, 0));
        memcpy(comm_rx_record, (void *)0x27D0800, 0x104);
        CHECK(comm_validate_record((struct TamagoFile *)0x27D0800));
        CHECK(comm_check_peer_phase((struct ExchangeRecord *)0x27D0800, 2));
        CHECK(comm_check_item_flag((struct ExchangeRecord *)0x27D0800));
        CHECK(comm_check_app_food((struct ExchangeRecord *)0x27D0800));
        comm_build_phase2();
        CHECKD(nfc_send(D_27C0800, *(u32 *)((u8 *)comm_tx_record + 0x48), n));
        break;
    }
    comm_trace2(1);
    switch (nfc_session.x003) {
    case 0: case 1: case 2: case 8:
        break;
    case 3: case 4: case 5: case 6: case 7:
        x = 0;
        CHECKD(nfc_receive((int)D_27D0400, (int *)&x, n, 1, 0));
        memcpy(comm_rx_record, (void *)0x27D0400, 0x104);
        CHECK(comm_validate_record((struct TamagoFile *)0x27D0400));
        comm_apply_points((struct GiftPoints *)comm_rx_record);
        comm_build_phase3();
        CHECKD(nfc_send(D_27C0400, *(u32 *)((u8 *)comm_tx_record + 0x48), n));
        break;
    }
    comm_trace2(1);
    k = 0;
out:
    r = k;
end:
    return r;
}

/*
 * Run the exchange for the current role (1: comm_exchange_initiator, else comm_exchange_target); x000_b = 1
 * marks "exchange in progress" for the NFC driver. Called from nfc_session_initiator (initiator) /
 * nfc_session_target (target) in F69.
 */
int comm_exchange(void)
{
    u8 r;

    nfc_session.x000_b = 1;
    if (nfc_session.x002 == 1)
        r = comm_exchange_initiator();
    else
        r = comm_exchange_target();
    nfc_session.x000_b = 0;
    return r;
}

/*
 * The RAM variables this file defines.
 */
u32 comm_gift_item;
u32 comm_tx_record;
