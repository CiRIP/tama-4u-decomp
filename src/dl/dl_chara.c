/*
 * Downloaded characters: descriptions and profile cards, affection, class and gender marks,
 * storing into slots, the read / delete menu and the list screen.
 *
 * Code 2505414..2506658, .rodata from 253C4E4.
 */
#include "proto/dl/dl_chara.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/sys/sys_boot.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/ui/ui_yes_no.h"
#include "proto/spot/spot_reader.h"
#include "proto/comm/comm_checks.h"
#include "flash.h"
#include "libc.h"

/*
 * Number of downloaded characters stored (8 slots of 32 KB at flash 0x2780000).
 */
int dl_chara_count(void)
{
    u8 *p = D_2780000;
    int n = 0;
    int i;

    for (i = 0; i <= 7; i++, p += 0x8000)
        if (*p != 0xff)
            n++;
    return n;
}

/*
 * Downloaded character slot k (0..7): address of its TAMAGO file, or -1 if empty.
 */
u8 *dl_chara_file(u8 k)
{
    u8 *p;
    int i;

    if (k > 7)
        return (u8 *)-1;
    p = D_2780000;
    for (i = 0; i < k; i++)
        p += 0x8000;
    if (*p != 0xff)
        return p;
    return (u8 *)-1;
}

/*
 * Downloaded character k: its character record (file + 0x204: id x002, kind, stage x00E,
 * base weight x010, hunger/happiness periods x011/x012, sickness % x013, wake/sleep hours
 * x014/x015, value x016, codes x01C/x01D/x01E, texts from +0x42 (0x96 bytes each),
 * sprites from +0x8FC (file + 0xB00)); -1 if empty. Characters with ids > 5000 use these
 * records instead of the built-in table chara_table.
 */
struct DlCharaRecord *dl_chara_record(u8 k)
{
    u8 *p;
    int i;

    if (k > 7)
        return (struct DlCharaRecord *)-1;
    p = D_2780000;
    for (i = 0; i < k; i++)
        p += 0x8000;
    if (*p != 0xff)
        return (struct DlCharaRecord *)(p + 0x204);
    return (struct DlCharaRecord *)-1;
}

/*
 * Downloaded character record by character id (-1 if not stored).
 */
struct DlCharaRecord *dl_chara_record_by_id(u16 id)
{
    u8 *p = D_2780000;
    int i;

    for (i = 0; i <= 7; i++, p += 0x8000) {
        if (*p != 0xff && ((struct DlCharaRecord *)(p + 0x204))->id == id) {
            p += 0x200;
            return (struct DlCharaRecord *)(p + 4);
        }
    }
    return (struct DlCharaRecord *)-1;
}

/*
 * Slot of the downloaded character whose name (+0x5E, 20 bytes) equals name; -1 if none.
 */
u8 dl_chara_slot_by_name(u8 *name)
{
    int i;
    int found;
    u8 *p;

    p = D_2780000;
    i = 0;
    found = -1;
    for (; i <= 7; i++) {
        if (memcmp(name, p + 0x5e, 20) == 0) {
            found = i;
            break;
        }
        p += 0x8000;
    }
    if (found >= 0)
        return found;
    return 0xff;
}

/*
 * Downloaded character k: start of its sprite data (file + 0xB00).
 */
u8 *dl_chara_sprites(u8 k)
{
    u8 *p;
    int i;

    if (k > 7)
        return (u8 *)-1;
    p = D_2780000;
    for (i = 0; i < k; i++)
        p += 0x8000;
    if (*p != 0xff)
        return p + 0xb00;
    return (u8 *)-1;
}

/*
 * Downloaded character k: picture of pose n (0..14) inside its sprite data (13 poses
 * then an extra one).
 */
u8 *dl_chara_pose_pic(u8 k, u8 n)
{
    u8 *p;
    u8 i;

    if (k > 7)
        return (u8 *)-1;
    if (n > 14)
        return (u8 *)-1;
    p = dl_chara_sprites(k);
    if (p != (u8 *)-1) {
        p += 0x204;
        for (i = 0; i < n; i++) {
            if (i <= 12)
                p += 0x246;
            else
                p += 0x4a2;
        }
    }
    return p;
}

/*
 * F20b = DOWNLOADABLE CHARACTERS: 8 slots of 0x8000 bytes at 0x2780000 (sectors 0x80..
 * 0x83, two slots per sector), each a TAMAGO record of type 0x94 / category 2 / sub-kind
 * 3 received over NFC (item_store_in_bank -> dl_chara_store). Record layout (as read here and in
 * F30): +0x5E name (0x14 bytes), +0x204 the character table entry (struct DlCharaRecord,
 * dl_chara_record(k): x002 character id, x02a favourite item class, x02c favourite item
 * id, text groups from +0x42 (dl_chara_text) ...), +0x20B gender?, +0x22E class byte,
 * +0x232 description text, +0xB00 the sprite data (dl_chara_sprites): pose pictures from
 * +0xB00 + 0x204 (dl_chara_pose_pic), a large picture at +0xB00 + 0x3A2C. dl_chara_file(k) =
 * slot base, dl_chara_sprites(k) = slot base + 0xB00, or -1 when the slot is erased. Affection
 * counters pet.dl_affection[8] count how often a slot's favourite item was
 * received while the pet is young (dl_chara_affection_update): probably what makes the character
 * appear.
 *
 * Downloaded character k: the large picture at file + 0xB00 + 0x3A2C, drawn on page 1 of
 * the profile card dl_chara_profile_card (page 0 shows pose 1, dl_chara_pose_pic(k, 1)); what it
 * depicts is not known (accessory/outfit pictures?). -1 when the slot is empty.
 */
u8 *dl_chara_large_pic(u8 k)
{
    u8 *p;

    if (k > 7)
        return (u8 *)-1;
    p = dl_chara_sprites(k);
    if (p != (u8 *)-1)
        return p + 0x3a2c;
    return (u8 *)-1;
}

/*
 * Downloaded character k: its description text (file + 0x232), shown on page 1 of the
 * profile card dl_chara_profile_card. -1 when the slot is empty.
 */
u8 *dl_chara_description(u8 k)
{
    u8 *p;

    if (k > 7)
        return (u8 *)-1;
    p = dl_chara_file(k);
    if (p != (u8 *)-1)
        return p + 0x232;
    return (u8 *)-1;
}

/*
 * Empty function (stub, `ret` only).
 */
void func_25055CE(void)
{
}


/*
 * Affection counters of the downloaded characters (pet.dl_affection), updated
 * when a downloaded item/character file is used: a == 0 resets the counters of the empty
 * slots, 1..8 clears slot a-1, 0xFF clears all 8 (pet + 0x226); otherwise a is a
 * received item record: every stored character whose favourite item id (x02c, big-endian
 * 0x5A of the item) matches and whose class x02a agrees with the item's type / category /
 * sub-kind (0 meal, 1 snack, 2 goods, 4 accessory, 5 clothes, 6-7 app sub-kind 1, 8 app
 * sub-kind 2) gets its counter bumped (max 3) while the pet is young (life stage
 * pet.life_stage <= 2).
 */
void dl_chara_affection_update(int a)
{
    struct TamagoFile *p;
    struct DlCharaRecord *s;
    int i;
    int f;
    int base, off;
    u8 c4e, c4f, c51;

    p = (struct TamagoFile *)a;
    if (a == 0) {
        for (i = 0; i <= 7; i++)
            if (dl_chara_file(i) == (u8 *)-1)
                save.pet.dl_affection[i] = 0;
    } else if ((u32)a <= 8) {
        save.pet.dl_affection[a - 1] = 0;
    } else if (a == 0xff) {
        for (i = 0; i <= 7; i++) {
            base = (int)&save.pet;
            off = 0x224;
            *(u8 *)(i + base + off + 2) = 0;
        }
    } else {
        for (i = 0; i <= 7; i++) {
            if (dl_chara_file(i) == (u8 *)-1)
                continue;
            s = dl_chara_record(i);
            f = -1;
            if ((p->x05a << 8) + p->x05b == s->fav_item_id) {
                c4e = p->x04e;
                c4f = p->x04f;
                c51 = p->sub_kind;
                switch (s->fav_item_class) {
                case 0:
                    if (c4e == 0x81 && c4f == 1 && c51 == 1)
                        f = 0;
                    break;
                case 1:
                    if (c4e == 0x81 && c4f == 1 && c51 == 2)
                        f = 0;
                    break;
                case 2:
                    if (c4e == 0x81 && c4f == 4 && c51 == 1)
                        f = 0;
                    break;
                case 4:
                    if (c4e == 0x81 && c4f == 2)
                        f = 0;
                    break;
                case 5:
                    if (c4e == 0x81 && c4f == 3)
                        f = 0;
                    break;
                case 6:
                case 7:
                    if (c4e == 0x94 && c4f == 2 && c51 == 1)
                        f = 0;
                    break;
                case 8:
                    if (c4e == 0x94 && c4f == 2 && c51 == 2)
                        f = 0;
                    break;
                case 9:
                case 10:
                    break;
                }
            }
            if (f == 0 && save.pet.dl_affection[i] <= 2 && save.pet.life_stage <= 2)
                save.pet.dl_affection[i]++;
        }
    }
}

/*
 * Downloaded character k: its class byte (file + 0x22E; picks the background of page 1
 * of the profile card dl_chara_profile_card), 0xFF when the slot is empty.
 */
u8 dl_chara_class(u8 k)
{
    u8 *p;

    if (k > 7)
        return 0xff;
    p = dl_chara_file(k);
    if (p == (u8 *)-1)
        return 0xff;
    return p[0x22e];
}

/*
 * Downloaded character k: the byte at file + 0x20B (gender? picks the mark drawn on page
 * 1 of the profile card dl_chara_profile_card), 0xFF when the slot is empty.
 */
u8 dl_chara_gender_mark(u8 k)
{
    u8 *p;

    if (k > 7)
        return 0xff;
    p = dl_chara_file(k);
    if (p == (u8 *)-1)
        return 0xff;
    return p[0x20b];
}

/*
 * Store a received downloaded character file a (0x47FE bytes) into slot k: the slot's
 * partner in the same 64 KB sector (two 32 KB slots per sector) is first copied to the
 * scratch sector 0xE (0x2060000 / 0x2068000), the new record is written next to it, the
 * sector (0x80 + k/2) is erased and the 64 KB are written back. a == 0 just rewrites the
 * sector without the slot (= erase slot k).
 */
void dl_chara_slot_write(int k, int a)
{
    u16 *p;
    u16 *s, *d;
    u16 *s3;
    u16 *d2;
    u16 *d3;
    int sec;
    u8 nb[4];
    int i;

    p = (u16 *)a;
    if (k > 7)
        return;
    sys_irq_disable();
    func_0000A54(0x2000000, 0xe, 0xe);
    switch (k) {
    case 0:
        s = dl_chara_slot1;
        d = (u16 *)0x2068000;
        s3 = (u16 *)0x2060000;
        d2 = (u16 *)0x2060000;
        d3 = (u16 *)D_2780000;
        sec = 0x80;
        break;
    case 1:
        s = (u16 *)D_2780000;
        d = (u16 *)0x2060000;
        d2 = (u16 *)0x2068000;
        s3 = (u16 *)0x2060000;
        d3 = (u16 *)D_2780000;
        sec = 0x80;
        break;
    case 2:
        s = dl_chara_slot3;
        d = (u16 *)0x2068000;
        s3 = (u16 *)0x2060000;
        d2 = (u16 *)0x2060000;
        d3 = dl_slot2_area;
        sec = 0x81;
        break;
    case 3:
        s = dl_slot2_area;
        d = (u16 *)0x2060000;
        d2 = (u16 *)0x2068000;
        s3 = (u16 *)0x2060000;
        d3 = dl_slot2_area;
        sec = 0x81;
        break;
    case 4:
        s = dl_chara_slot5;
        d = (u16 *)0x2068000;
        s3 = (u16 *)0x2060000;
        d2 = (u16 *)0x2060000;
        d3 = dl_chara_slot4;
        sec = 0x82;
        break;
    case 5:
        s = dl_chara_slot4;
        d = (u16 *)0x2060000;
        d2 = (u16 *)0x2068000;
        s3 = (u16 *)0x2060000;
        d3 = dl_chara_slot4;
        sec = 0x82;
        break;
    case 6:
        s = dl_chara_slot7;
        d = (u16 *)0x2068000;
        s3 = (u16 *)0x2060000;
        d2 = (u16 *)0x2060000;
        d3 = dl_chara_slot6;
        sec = 0x83;
        break;
    case 7:
    default:
        s = dl_chara_slot6;
        d = (u16 *)0x2060000;
        d2 = (u16 *)0x2068000;
        s3 = (u16 *)0x2060000;
        d3 = dl_chara_slot6;
        sec = 0x83;
        break;
    }
    *(u32 *)nb = 0x8000;
    i = 0;
    do {
        if (func_0000B52(d, *s, 0x2000000) != 0)
            goto out;
        i++;
        d++;
        s++;
    } while (i < (int)(*(u32 *)nb >> 1));
    if (p != 0) {
        i = 0;
        *(u32 *)nb = 0x47fe;
        do {
            if (func_0000B52(d2, *p, 0x2000000) != 0)
                goto out;
            i++;
            d2++;
            p++;
        } while (i < (int)(*(u32 *)nb >> 1));
    }
    func_0000A54(0x2000000, sec, sec);
    *(u32 *)nb = 0x10000;
    i = 0;
    do {
        if (func_0000B52(d3, *s3, 0x2000000) != 0)
            goto out;
        i++;
        d3++;
        s3++;
    } while (i < (int)(*(u32 *)nb >> 1));
out:
    sys_irq_enable();
}

/*
 * Is the downloaded character's name (+0x5E) one of the two protected names dl_chara_protected_names
 * (きゃんでぃぱく2 / ラブソラっち: built-in characters that must not be overwritten?)?
 * Returns -1 for them, else 0.
 */
int dl_chara_name_protected(u8 *name)
{
    int i;

    for (i = 0; i <= 1; i++)
        if (memcmp(name + 0x5e, dl_chara_protected_names[i], 20) == 0)
            return -1;
    return 0;
}

/*
 * Store a received downloadable character file p: a slot with the same name (+0x5E) is
 * replaced in place; else the first erased slot (4-byte marker D_253C56C); else the "slots
 * full" notice spot_items_full_notice and dl_chara_pick_slot lets the user pick a slot to replace (its
 * affection counter is cleared). Returns 0 stored, -1 cancelled, -2 timeout.
 */
int dl_chara_store(int p)
{
    int r;
    u8 *a;
    int i;

    a = D_2780000;
    i = 0;
    r = -1;
    for (; i <= 7; i++) {
        if (memcmp((u8 *)p + 0x5e, a + 0x5e, 0x14) == 0) {
            r = 0;
            break;
        }
        a += 0x8000;
    }
    if (r == 0) {
        dl_chara_slot_write(i, p);
        return r;
    }
    a = D_2780000;
    i = 0;
    r = -1;
    for (; i <= 7; i++) {
        if (memcmp(a, D_253C56C, 4) == 0) {
            r = 0;
            break;
        }
        a += 0x8000;
    }
    if (r == 0) {
        dl_chara_affection_update(0);
        dl_chara_slot_write(i, p);
        return r;
    }
    spot_items_full_notice();
    i = dl_chara_pick_slot();
    if (i >= 0) {
        dl_chara_affection_update(i + 1);
        dl_chara_slot_write(i, p);
        return 0;
    }
    if (i == -2)
        return -2;
    return -1;
}

/*
 * Menu for downloaded character a: "よむ?" (read?) with the entries はい / いいえ / a third
 * option D_253C58C, which is greyed and refused (snd_error_beep) when slot a holds the
 * character currently being raised. Returns the entry (0..2), 1 on C, 3 on timeout.
 */
u8 dl_chara_read_menu(u8 a)
{
    u8 h;
    int flag = 0;
    u8 i;
    int n = 3;
    u8 t;

    if (dl_chara_record(a)->id == save.pet.x000)
        flag = 1;
    ui_new_screen();
    i = 0;
    gfx_object_create(i, 0x1c, i, i, i, D_2104794);
    t = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_centered(t, 0x20, 0x30, D_253C574, 0, 9);
    t = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_line(t, 0x18, 0x30, 0x30, (int)D_253C57C, 0, 8);
    t = gfx_object_create(0x18, 0x30, 0x30, 0, 0, 0);
    gfx_draw_text_line(t, 0x18, 0x40, 0x30, (int)D_253C584, 0, 8);
    if (flag == 0) {
        t = gfx_object_create(0x18, 0x30, 0x30, 0, 0, 0);
        gfx_draw_text_line(t, 0x18, 0x50, 0x30, (int)D_253C58C, 0, 8);
    } else {
        t = gfx_object_create(0x18, 0x30, 0x34, 0, 3, 0);
        gfx_draw_text_line(t, 0x18, 0x50, 0x34, (int)D_253C58C, 0, 8);
    }
    h = gfx_object_create(6, i * 16 + 0x32, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses != 0) {
            ui_timeout = 10;
            i++;
            if (i >= n)
                i = 0;
            gfx_object_move_dirty(h, 6, i * 16 + 0x32);
            func_00009A6();
            btn_a_presses = 0;
        }
        if (btn_b_presses != 0) {
            if (i == 2 && flag != 0) {
                snd_error_beep();
                btn_b_presses = 0;
                ui_timeout = 10;
            } else {
                ui_timeout = 10;
                break;
            }
        }
        if (btn_c_presses != 0) {
            i = 1;
            break;
        }
        sys_idle_tick();
        if (ui_timeout == 0) {
            i = 3;
            break;
        }
    }
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return i;
}

/*
 * Browse the 8 downloaded-character slots (portrait + name): A = next used slot, B =
 * confirm (ui_yes_no_question(1, 1) dialog) -> slot index, C = -1, timeout = -2. Slots that are
 * erased or hold the current character are skipped.
 */
int dl_chara_pick_slot(void)
{
    int k;
    u8 *base;
    u8 *s;
    struct DlCharaRecord *e;
    int redraw;
    u8 z;
    int n = 8;
    u8 *p;
    u8 c;

    k = 0;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    base = D_2780000;
    s = base;
    e = (struct DlCharaRecord *)(base + 0x204);
    redraw = 1;
    z = 0;
    for (;;) {
    if (*s == 0xff || e->id == save.pet.x000) {
        btn_a_presses = 1;
        redraw = 0;
    }
    if (redraw) {
        ui_new_screen();
        gfx_object_create(z, 0x1c, z, z, z, D_2104794);
        p = dl_chara_pose_pic(k, 1);
        gfx_object_create(0x40 - (p[0] >> 1), 0x31 - (p[1] >> 1), 0x30, z, z, (const u8 *)p);
        gfx_draw_text_centered(gfx_object_create(z, 0x38, 0x90, z, z, (const u8 *)z), 0x44, 0x90, base + 0x5e, z, 9);
        gfx_object_create(0x72, 0x58, 0x40, n > k + 1 ? z : 1, z, D_21059A4);
        func_00009A6();
        redraw = z;
    }
    if (btn_a_presses != 0) {
        do {
            k++;
            base += 0x8000;
            if (k >= n) {
                k = 0;
                base = D_2780000;
            }
            s = base;
        } while (*base == 0xff);
        e = (struct DlCharaRecord *)(base + 0x204);
        redraw = 1;
        btn_a_presses = z;
        ui_timeout = 10;
    }
    if (btn_b_presses != 0) {
        int r = ui_yes_no_question(1, 1);
        if (r == 0)
            return k;
        if (r == 3) {
            btn_a_presses = z;
            btn_b_presses = z;
            btn_c_presses = z;
            return -2;
        }
        btn_b_presses = z;
        ui_timeout = 10;
        redraw = 1;
    }
    if (btn_c_presses != 0)
        goto quit;
    sys_idle_tick();
    if ((c = ui_timeout) == 0) {
        btn_a_presses = c;
        btn_b_presses = c;
        btn_c_presses = c;
        return -2;
    }
    }
quit:
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return -1;
}

s16 txt_center_x(int p, u8 big, u8 n)
{
    int cnt, i;
    u8 *s, *q;
    u8 c;
    int w;

    s = (u8 *)p;
    i = cnt = 0;
    for (; i < n; i++) {
        q = s + 2;
        c = *q;
        if (c == 0)
            break;
        if (c == 0x6c)
            break;
        cnt++;
        s = q;
    }
    w = 0x80 - (big ? cnt * 9 : cnt * 12);
    return w >> 1;
}

const u8 D_253C534[] = _("きゃんでぃぱく2\0\0");
const u8 D_253C54C[] = _("ラブソラっち\0\0\0\0");
const u8 *const dl_chara_protected_names[2] = { D_253C534, D_253C54C };
const u8 D_253C56C[8] = { 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0 };
const u8 D_253C574[] = _("よむ?");
const u8 D_253C57C[] = _("はい");
const u8 D_253C584[] = _("いいえ");
const u8 D_253C58C[] = _("けす");
/* unreferenced picture (12 x 14) */
const u8 D_253C594[] = {
    0x0c, 0x0e, 0x02, 0x00, 0x01, 0xff, 0x07, 0xe0, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x10, 0x01, 0x11, 0x01, 0x00, 0x00, 0x11, 0x01, 0x10, 0x11, 0x00, 0x10, 0x11, 0x00, 0x00, 0x11,
    0x01, 0x11, 0x01, 0x00, 0x00, 0x10, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x11, 0x01, 0x00, 0x00,
    0x00, 0x10, 0x11, 0x11, 0x00, 0x00, 0x00, 0x11, 0x01, 0x11, 0x01, 0x00, 0x10, 0x11, 0x00, 0x10,
    0x11, 0x00, 0x11, 0x01, 0x00, 0x00, 0x11, 0x01, 0x11, 0x00, 0x00, 0x00, 0x10, 0x01, 0x00, 0x00,
};

/*
 * Profile card of downloaded character a ("よむ?" = read), two pages: page 0 = portrait
 * (pose 1) + name; page 1 = background by the class byte +0x22E (0/1, 2, 4, 8, 6 -> five
 * pictures), gender mark (+0x20B), the large picture (dl_chara_large_pic) and the description
 * text (dl_chara_description). A flips the page, C leaves (0), 10 s timeout -> 1.
 */
u8 dl_chara_profile_card(s8 a)
{
    u8 *t1;
    u8 *t2;
    u8 *t3;
    u8 *t4;
    u8 page = 0;
    int redraw = 1;
    u8 c;

    if (a < 0)
        return 0;
    t1 = dl_chara_pose_pic(a, 1);
    t2 = dl_chara_file(a) + 0x5e;
    t3 = dl_chara_large_pic(a);
    t4 = dl_chara_description(a);
    btn_a_presses = 0;
    btn_b_presses = 1;
    btn_c_presses = 0;
    ui_timeout = 10;
    do {
        if (redraw) {
            if (page == 0) {
                anim_scene_room(4);
                anim_draw_pic(0, D_21182DC, 0, 0x40, 0x24);
                anim_draw_pic(1, t1, 0, 0x40, 0x1a);
                anim_draw_pic(2, D_21059A4, 0, 0x76, 0x40);
                anim_draw_text(3, t2, 0, txt_center_x((int)t2, 0, 9), 0x34);
                anim_frame(10, 0);
            } else {
                if (dl_chara_gender_mark(a) == 0)
                    anim_pic_slots[0][0] = D_2125D74;
                else
                    anim_pic_slots[0][0] = D_2125DCC;
                anim_pic_slots[0][1] = t3;
                switch (dl_chara_class(a)) {
                case 0:
                case 1:
                    anim_pic_slots[0][2] = D_21194F8;
                    break;
                case 2:
                    anim_pic_slots[0][2] = D_211A714;
                    break;
                case 4:
                    anim_pic_slots[0][2] = D_211B930;
                    break;
                case 8:
                    anim_pic_slots[0][2] = D_211CB50;
                    break;
                case 6:
                    anim_pic_slots[0][2] = D_211DD6C;
                    break;
                default:
                    anim_pic_slots[0][2] = D_21182DC;
                    break;
                }
                anim_scene_room(5);
                anim_draw_pic(0, anim_pic_slots[0][2], 0, 0x40, 0x24);
                anim_draw_pic(1, D_21059F4, 0, 0x76, 0x40);
                anim_draw_pic(2, anim_pic_slots[0][1], 0, 0x5d, 0x16);
                anim_draw_pic(3, anim_pic_slots[0][0], 0, 0x13, 0x13);
                anim_draw_text(4, t4, 1, 0xe, 0x2b);
                anim_frame(10, 0);
            }
            btn_a_presses = 0;
            btn_c_presses = 0;
            redraw = 0;
        }
        if (btn_a_presses != 0) {
            page++;
            if (page > 1)
                page = 0;
            redraw = 1;
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_c_presses != 0) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            return 0;
        }
        sys_idle_tick();
        c = ui_timeout;
    } while (c != 0);
    btn_a_presses = c;
    btn_b_presses = c;
    btn_c_presses = c;
    return 1;
}

/*
 * "このたまごっちが ついかされたよ" (these Tamagotchi were added) screen: two character
 * badges, lit when the communication count pet.comm_count exceeds 4 and 9 (0x2127324 /
 * 0x2127038, else the dark 1-bit pictures 0x21278F0 / 0x2127614): the characters unlocked
 * by communicating (announced at the 5th / 10th communication by comm_notebook_unlock_check; the growth
 * choices evo_pick_adult_girl / evo_pick_adult_boy use the same thresholds). dl_chara_count = number of
 * stored downloaded characters; when there are some, an arrow is shown and A continues to
 * their list (returns 0, as does C). 1 on timeout.
 */
u8 dl_chara_added_screen(void)
{
    int n;
    u8 a;
    const u8 *p;
    u8 c;

    snd_wait_end();
    n = dl_chara_count();
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2125E24);
    a = gfx_object_create(0, 0, 0x10, 0, 0, 0);
    gfx_draw_text_centered(a, 0x1f, 0x10, D_253C618, 0, 9);
    a = gfx_object_create(0, 0, 0x14, 0, 0, 0);
    gfx_draw_text_centered(a, 0x55, 0x14, D_253C62C, 0, 9);
    if (save.pet.comm_count > 4) {
        p = D_2127324;
        gfx_object_create(0x28 - (p[0] >> 1), 0x41 - (p[1] >> 1), 0x20, 0, 0, (const u8 *)p);
    } else {
        p = D_21278F0;
        gfx_object_create(0x28 - (p[0] >> 1), 0x41 - (p[1] >> 1), 0x20, 0, 0, (const u8 *)p);
    }
    if (save.pet.comm_count > 9) {
        p = D_2127038;
        gfx_object_create(0x58 - (p[0] >> 1), 0x41 - (p[1] >> 1), 0x30, 0, 0, (const u8 *)p);
    } else {
        p = D_2127614;
        gfx_object_create(0x58 - (p[0] >> 1), 0x41 - (p[1] >> 1), 0x30, 0, 0, (const u8 *)p);
    }
    if (n > 0)
        gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 1;
    btn_c_presses = 0;
    do {
        if (btn_a_presses != 0) {
            if (n > 0)
                goto out;
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_c_presses != 0)
            goto out;
        sys_idle_tick();
    } while ((c = ui_timeout) != 0);
    ui_main_icon = c;
    return 1;
out:
    ui_timeout = 10;
    return 0;
}

const u8 D_253C618[] = _("このたまごっちが");
const u8 D_253C62C[] = _("ついかされたよ");

/*
 * The downloaded-characters list (たまともてちょう entry): after the touch-count card
 * (dl_chara_added_screen), the names (+0x5E) of the up to 8 downloaded-character slots
 * (dl_chara_file), three per page, up to 3 pages; B -> "よむ?" (dl_chara_read_menu): read = the
 * two-page card dl_chara_profile_card, option 2 = delete the slot (ui_yes_no_question(1, 1) confirm,
 * dl_chara_slot_write(slot, 0), save), timeout -> leave.
 */
int dl_chara_list(void)
{
    u8 *items[9];
    s8 ids[9];
    u8 n;
    u8 sel;
    int redraw;
    u8 cursor;
    u8 pages;
    u8 page;
    int flag2;
    u8 i, j;
    u8 *p;
    u8 *q;
    int c;
    int r;

    sel = 0;
    redraw = 1;
    flag2 = 0;
restart:
    if (dl_chara_added_screen() != 0)
        return 1;
    if (btn_c_presses != 0)
        return 0;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
rescan:
    n = dl_chara_count();
    if (n == 0)
        goto restart;
    pages = 3;
    if (n <= 6)
        pages = 2;
    if (n <= 3)
        pages = 1;
    if (sel >= n)
        sel = n - 1;
    for (i = 0; i <= 8; i++) {
        items[i] = 0;
        ids[i] = -1;
    }
    i = 0;
    j = i;
    for (; i <= 7; i++) {
        q = dl_chara_file(i);
        if ((int)q != -1) {
            items[j] = q;
            ids[j] = i;
            j++;
        }
    }
    do {
        if (redraw) {
            ui_new_screen();
            gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
            if (sel <= 2)
                page = 1;
            else if (sel <= 5)
                page = 2;
            else
                page = 3;
            for (i = 0; i <= 2; i++) {
                if (ids[(page - 1) * 3 + i] >= 0) {
                    p = items[(page - 1) * 3 + i] + 0x5e;
                    gfx_draw_text_line(gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0), 0x18, i * 16 + 0x20, 0x30, (int)p, 0, 8);
                }
            }
            if (pages > 1) {
                if (pages != page)
                    gfx_object_create(0x72, 0x58, 0x40, 0, 0, 0x21059a4);
                else
                    gfx_object_create(0x72, 0x58, 0x40, 1, 0, 0x21059a4);
            }
            if (pages == 1)
                gfx_object_create(0x72, 0x58, 0x40, 1, 0, 0x21059a4);
            cursor = gfx_object_create(6, (sel % 3) * 16 + 0x22, 0x20, 0, 0, 0x2104714);
            func_00009A6();
            redraw = 0;
        }
        if (btn_a_presses) {
            sel++;
            if (n <= sel) {
                sel = 0;
                redraw = 1;
                goto restart;
            }
            if (sel == 3 || sel == 6)
                redraw = 1;
            else {
                gfx_object_move_dirty(cursor, 6, (sel % 3) * 16 + 0x22);
                func_00009A6();
            }
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            btn_b_presses = 0;
            ui_timeout = 10;
            for (;;) {
                i = dl_chara_read_menu(ids[sel]);
                if (i == 0) {
                    if (dl_chara_profile_card(ids[sel]) == 1) {
                        btn_a_presses = 0;
                        btn_b_presses = 0;
                        btn_c_presses = 0;
                        return 0;
                    }
                } else if (i == 2) {
                    r = ui_yes_no_question(1, 1);
                    if (r == 0) {
                        dl_chara_slot_write(ids[sel], 0);
                        flag2 = 1;
                        snd_wait_end();
                        save_game();
                        break;
                    }
                    if (r == 3) {
                        btn_a_presses = 0;
                        btn_b_presses = 0;
                        btn_c_presses = 0;
                        return 0;
                    }
                } else if (i == 3) {
                    btn_a_presses = 0;
                    btn_b_presses = 0;
                    btn_c_presses = 0;
                    ui_main_icon = 0;
                    return 1;
                } else
                    break;
            }
            redraw = 1;
            btn_b_presses = 0;
            ui_timeout = 10;
            if (flag2)
                goto rescan;
        }
        if (btn_c_presses) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            return 0;
        }
        sys_idle_tick();
    } while ((c = ui_timeout) != 0);
    btn_a_presses = c;
    btn_b_presses = c;
    btn_c_presses = c;
    return c;
}

/*
 * Is the current pet a downloaded character with codes (a, b)? Returns its id or -1.
 */
int dl_pet_by_codes(u8 a, u8 b)
{
    struct DlCharaRecord *p;
    int i;

    for (i = 0; i <= 7; i++) {
        p = dl_chara_record(i);
        if (p != (struct DlCharaRecord *)-1 && p->id == save.pet.x000 && p->bday_month == a &&
            p->bday_day == b)
            return p->id;
    }
    return -1;
}
