/*
 * Writes our 0x6C-byte profile block of the old serial (IR) protocol into a record, padding it;
 * followed by two stubs that return 0.
 *
 * Code 250043A..250090C, .rodata from 253B248. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/ir/ir_profile_record.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/pet/pet_core.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/ir/ir_receive.h"
#include "proto/owner/owner_profile.h"
#include "libc.h"
#include "flash_map.h"

/*
 * Pad a 0x6C-byte flash record from offset a with zero halfwords.
 */
int flash_pad_record(u32 a, u8 *p)
{
    u16 buf[2];
    u32 i, off;

    memset(buf, 0, 4);
    a = 0x6c - a;
    for (i = off = 0; i < a >> 1; i++) {
        u16 *q = (u16 *)(p + off);
        if (func_0000B52(q, buf[0], FLASH_BASE) != 0)
            return -1;
        off += 2;
    }
    return 0;
}

/* Write our 0x6C-byte PROFILE BLOCK of the old serial protocol (the record the
 * Tama-to-Tama session ir_exchange exchanges) at dst, for request a (a[1] = message
 * kind: 0 / 1 or 0x20..0x23; other kinds get an empty block): 28 bytes {u16 character
 * id (byte-swapped by chara_base_id / util_read_be16), session kind comm_menu_choice, comm_submenu_choice,
 * gender, life stage, class letters, owner birthday, character name (12), item flags
 * ir_stub_return0, friend_event.x000, 0x10 at +0x1A}; 16 bytes of pose ids {7, 2, 3, 9, 0, 6,
 * 7, 8}; 16 bytes of game-unlock state (species group, for 0x22/0x23 the partner's
 * wish list visit_partner.wish_count.., else the unlock items pet.clover1_id_hi..); 16 bytes of stats
 * (pet.dominant_trait + the five at pet + 0x11A, care / happiness / hunger, the class
 * name chara_trait_caps) ; flash_pad_record appends the trailer. Returns 0x6C or -1. */
/*
 * Write a name/text record from a into flash at dst (character conversion of the
 * game's 2-byte text, padded to 0x6C bytes).
 */
int flash_write_text_record(u8 *a, u8 *dst)
{
    u8 buf[40];
    u16 t;
    u8 *p, *q, *r, *s, *z;
    int i;
    u32 off;
    int k;
    int n;
    int total;
    int pp, o;
    u8 c, m;

    z = buf;
    total = 0;
    flash_begin();
    k = a[1];
    if (!(k <= 1 || (k >= 0x20 && k <= 0x23))) {
        if (flash_pad_record(total, dst) != 0)
            return -1;
        return 0x6c;
    }
    memset(buf, 0, 0x10);
    p = buf;
    t = chara_base_id(save.pet.x000);
    *(u16 *)buf = util_read_be16((int)&t);
    buf[2] = comm_menu_choice;
    buf[3] = comm_submenu_choice;
    c = pet_get_gender();
    if (c != 0)
        c = 1;
    buf[4] = c;
    r = (u8 *)&save.pet;
    p[5] = save.pet.life_stage;
    p[6] = chara_bday_month(((struct PetState *)r)->x000);
    p[7] = chara_bday_day(((struct PetState *)r)->x000);
    p[8] = save.pet.owner_bday_month;
    p[9] = save.pet.owner_bday_day;
    memset(p + 10, 0, 12);
    n = strlen((void *)chara_text(((struct PetState *)r)->x000, 1, 0));
    if (n > 12)
        n = 12;
    memcpy(p + 10, chara_text(save.pet.x000, 1, 0), n);
    p[0x16] = ir_stub_return0();
    p[0x16] |= ir_stub_return0();
    p[0x17] = friend_event.x000;
    p[0x1a] |= 0x10;
    p[0x1b] = 0;
    s = p;
    for (i = 0, off = 0; i < 14u; i++, off += 2)
        if (func_0000B52((u16 *)(dst + off), *(u16 *)(s + off), FLASH_BASE) != 0)
            return -1;
    dst += off;
    total += off;

    memset(buf, 0, 0x10);
    q = buf;
    if (a[1] == 0) {
        buf[0] = 7;
        buf[1] = 2;
        buf[2] = 3;
        buf[3] = 9;
        buf[4] = 0;
        buf[5] = 6;
        buf[6] = 7;
        buf[7] = 8;
    } else if (a[1] == 1) {
        buf[0] = 7;
        buf[1] = 2;
        buf[2] = 3;
        buf[3] = 9;
        buf[4] = 0;
        buf[5] = 6;
        buf[6] = 7;
        buf[7] = 8;
    }
    s = q;
    for (i = 0, off = 0; i < 8u; i++, off += 2)
        if (func_0000B52((u16 *)(dst + off), *(u16 *)(s + off), FLASH_BASE) != 0)
            return -1;
    dst += off;
    total += off;

    memset(buf, 0, 0x10);
    buf[1] = chara_class_field(save.pet.x000);
    if (a[1] >= 0x22 && a[1] <= 0x23) {
        if ((u8)(visit_partner.wish2_id_hi | visit_wish2_id_lo))
            buf[0] = 2;
        else if ((u8)(visit_partner.wish1_id_hi | visit_partner.wish1_id_lo))
            buf[0] = 1;
        else
            buf[0] = 0;
        buf[0] = visit_partner.wish_count;
        buf[2] = visit_partner.wish1_id_hi;
        buf[4] = visit_partner.wish1_id_lo;
        buf[3] = visit_partner.wish2_id_hi;
        buf[5] = visit_wish2_id_lo;
        buf[6] = visit_wish1_kind;
        buf[7] = visit_wish2_kind;
        buf[8] = visit_wish_count;
    } else {
        m = 7;
        if ((save.pet.clover_leaves & 0xf) == 0xf) {
            buf[0] = 4;
        } else if ((save.pet.clover_leaves & m) == m) {
            buf[0] = 3;
            buf[2] = save.pet.clover1_id_hi;
            buf[4] = save.pet.clover1_id_lo;
            buf[3] = save.pet.clover2_id_hi;
            buf[5] = save.pet.clover2_id_lo;
            buf[6] = save.pet.clover1_kind;
            buf[7] = save.pet.clover2_kind;
        } else {
            if ((u8)(save.pet.clover2_id_hi | save.pet.clover2_id_lo))
                buf[0] = 2;
            else if ((u8)(save.pet.clover1_id_hi | save.pet.clover1_id_lo))
                buf[0] = 1;
            else
                buf[0] = 0;
            buf[2] = save.pet.clover1_id_hi;
            buf[4] = save.pet.clover1_id_lo;
            buf[3] = save.pet.clover2_id_hi;
            buf[5] = save.pet.clover2_id_lo;
            buf[6] = save.pet.clover1_kind;
            buf[7] = save.pet.clover2_kind;
            buf[8] = 0;
        }
    }
    s = buf;
    for (i = 0, off = 0; i < 8u; i++, off += 2)
        if (func_0000B52((u16 *)(dst + off), *(u16 *)(s + off), FLASH_BASE) != 0)
            return -1;
    dst += off;
    total += off;

    memset(buf, 0, 0x14);
    r = buf;
    if (a[1] >= 0x22 && a[1] <= 0x23) {
        z[0] = visit_partner.dominant_trait;
        q = buf + 1;
        for (i = 0; i < 5; i++)
            q[i] = ((u8 *)&visit_partner)[i + 5];
        r[0xd] = r[0xc] = ((u8 *)&visit_partner)[i + 5];
        r[0xe] = ((u8 *)&visit_partner)[0x1a];
    } else {
        q = buf;
        *q++ = save.pet.dominant_trait;
        pp = (int)&save.pet;
        o = 0x118;
        for (i = 0; i < 5; i++)
            q[i] = *(u8 *)(i + pp + o + 2);
        r[0xc] = save.pet.care_mistakes;
        r[0xd] = save.pet.happiness;
        r[0xe] = save.pet.hunger;
    }
    if (*chara_trait_caps(save.pet.x000) != 0)
        memcpy(r + 7, chara_trait_caps(save.pet.x000), 5);
    s = buf;
    for (i = 0, off = 0; i < 8u; i++, off += 2)
        if (func_0000B52((u16 *)(dst + off), *(u16 *)(s + off), FLASH_BASE) != 0)
            return -1;
    dst += off;
    total += off;
    if (flash_pad_record(total, dst) != 0)
        return -1;
    return 0x6c;
}

/*
 * Returns 0 (stub).
 */
int func_2500908(void)
{
    return 0;
}

/*
 * Returns 0 (stub).
 */
int func_250090C(void)
{
    return 0;
}
