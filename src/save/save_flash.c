/*
 * Saving and the records in flash: factory reset, erase confirmation, power-on, save_game,
 * sector backup / restore, item banks and inventories (find, add, receive, discard), seasonal
 * decorations, downloaded program install, gashapon / bingo records and friend portraits.
 *
 * Code 250B78E..250DA4C, .rodata from 255BF84.
 */
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/chara/chara_draw.h"
#include "proto/shop/shop_browser.h"
#include "proto/diary/diary_notebook.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/evo/evo_special.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/ui/ui_message.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/ir/ir_receive.h"
#include "proto/ir/ir_packets.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_marks.h"
#include "proto/ui/ui_menu.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care.h"
#include "proto/spot/spot_bingo_menu.h"
#include "proto/spot/spot_reader.h"
#include "proto/test/test_mode.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/comm/comm_outcome.h"
#include "proto/nfc/nfc_session.h"
#include "flash.h"
#include "hw.h"

/*
 * Address of flash sector k (1-based, 1..0x87) from the sector table flash_sector_offsets (byte
 * offsets from 0x2000000; entry k-1 is the start of sector k, i.e. flash_sector_offsets[k] is its
 * end): sectors 1..8 are the 8 KB boot sectors, 9..0x87 are 64 KB. 0 if out of range.
 * Every item bank, save bank and copy area is addressed through this.
 */
u8 *flash_sector_addr(int k)
{
    u8 *p = 0;

    if (k >= 1 && k <= 0x87)
        p = (u8 *)(flash_sector_offsets[k - 1] + FLASH_BASE);
    return p;
}

/*
 * Address of slot b of item bank (download category) a, or 0 if b is beyond the bank.
 * item_bank_tbl[a] is the bank directory (F36 data): x000 = slot size (0x800 food, 0x2000
 * accessories/goods/backgrounds, 0x4000, 0x8000 app data), x002 = first flash sector,
 * x003 = number of slots, x004 = list of global item indexes. Each slot holds one TAMAGO
 * item file or 0xFF (empty).
 */
u8 *item_bank_slot(int a, int b)
{
    if (item_bank_tbl[a].slot_count <= b)
        return 0;
    return flash_sector_addr(item_bank_tbl[a].first_sector) + item_bank_tbl[a].slot_size * b;
}

/*
 * Item b of category a: b < slot count -> the downloaded record in its flash slot,
 * otherwise the built-in record item_builtin_records[item_bank_tbl[a].builtin_list[b - count]] (0 past the end).
 */
struct TamagoFile *item_bank_record(int a, int b)
{
    if (item_bank_tbl[a].slot_count > b)
        return (struct TamagoFile *)(flash_sector_addr(item_bank_tbl[a].first_sector) + item_bank_tbl[a].slot_size * b);
    b -= item_bank_tbl[a].slot_count;
    if (item_bank_tbl[a].builtin_count <= b)
        return 0;
    return item_builtin_records[item_bank_tbl[a].builtin_list[b]];
}

/*
 * Does item b of category a exist (c: including built-in items; flags/visibility checks)?
 */
u8 item_bank_has(int a, int b, int c)
{
    struct TamagoFile *p;

    if (c)
        p = item_bank_record(a, b);
    else
        p = (struct TamagoFile *)item_bank_slot(a, b);
    if (p == 0 || p->x000 == 0xffffffff)
        return 0;
    if (c == 0)
        return 1;
    if (p->x04e == 0x81 || p->x04e == 0x94) {
        if (p->kind == c)
            return 1;
        else if (p->kind == 0x35 && c == 0x34)
            return 1;
        else
            return 0;
    }
    if (p->x04e == 1) {
        if (p->x04f == 1) {
            int t = p->sub_kind;
            if (t >= 2 && t <= 3) {
                if (c == 13)
                    return 1;
                else
                    return 0;
            }
        }
    }
    return 1;
}

/* Item storage manager (F34): the downloadable/collectable items (TAMAGO files, see
 * notes/fable/NOTES.md section 8) live in flash banks described by item_bank_tbl
 * (per category: slot size, first sector, slot count, list of global indexes); flash is
 * written only from RAM-resident code with interrupts off (sys_irq_disable ... sys_irq_enable). */

/*
 * Factory reset of the flash data ("データをけす" = erase data): erase sectors 1..4 (the
 * 0x2000000 program/app area), the friend picture banks 9..11, sectors 0x34..0x3B (mail
 * + ?) and 0x62..0x87 (all downloads, item banks, characters, save banks, NFC buffers),
 * then install the defaults: the two built-in food items item_builtin_records[D_255BF88[i]] (ROM
 * copies of TAMAGO files in the asset area) into sector 0x65 slots 0/1 with 0x800 stride
 * (the TAMAGO file found at flash 0x25D0000 in the dump), marked owned (item_set_count),
 * and the default room background item_builtin_records[47] into sector 0x76 (flash 0x26E0000,
 * "t4u_bg00001"). comm_menu_choice = comm_submenu_choice = 0.
 */
void save_factory_reset(void)
{
    u32 i;

    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_DL_PROGRAM, FLASH_SEC_DL_PROGRAM_LAST);
    func_0000A54(FLASH_BASE, FLASH_SEC_PORTRAITS, FLASH_SEC_PORTRAITS_LAST);
    func_0000A54(FLASH_BASE, FLASH_SEC_RECORDS, FLASH_SEC_RECORDS_LAST);
    func_0000A54(FLASH_BASE, FLASH_SEC_USER, FLASH_SEC_USER_LAST);
    i = 0;
    sys_irq_enable();
    for (; i <= 1; i++) {
        flash_program_record((u16 *)item_builtin_records[D_255BF88[i]], (u32)flash_sector_addr(FLASH_SEC_INV_FOOD), 0x800, i);
        item_set_count(i, 1);
    }
    flash_program_record((u16 *)item_builtin_records[47], (u32)flash_sector_addr(FLASH_SEC_DEFAULT_BG), 0x2000, 0);
    comm_menu_choice = 0;
    comm_submenu_choice = 0;
}

extern const u8 D_255BF8C[];

extern const u8 D_255BF9C[];

extern const u8 D_255BFA4[];

/*
 * Factory test / initial set-up screen (asm).
 */
u8 save_confirm_erase(void)
{
    u8 sel, m, k;
    u8 r;

    ui_new_screen();
    sel = 1;
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    k = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_centered(k, 0x20, 0x10, (u8 *)D_255BF8C, 0, 9);
    k = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(k, 0x18, 0x40, 0x10, (int)D_255BF9C, 0, 8);
    k = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(k, 0x18, 0x50, 0x10, (int)D_255BFA4, 0, 8);
    m = gfx_object_create(6, 0x52, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    do {
        if (btn_a_presses) {
            ui_timeout = 10;
            sel++;
            if (sel > 1)
                sel = 0;
            gfx_object_move_dirty(m, 6, sel * 16 + 0x42);
            func_00009A6();
            btn_a_presses = 0;
        }
        if (btn_b_presses) {
            if (sel != 0)
                break;
            btn_a_presses = 1;
            btn_b_presses = 1;
            btn_c_presses = 1;
            snd_wait_end();
            sys_irq_disable();
            func_0000A54(FLASH_BASE, FLASH_SEC_SAVE, FLASH_SEC_SAVE);
            sys_irq_enable();
            r = 1;
            goto out;
        }
        if (btn_c_presses)
            break;
        if (sys_display_state) {
            T16_CTL0 = 0;
            T16_CTL1 = 0;
            T16_CTL3 = 0;
            T16_CTL5 = 0;
            sys_display_off();
            while (sys_display_state)
                asm volatile("slp");
            if (sys_battery_check_due) {
                sys_battery_low();
                sys_battery_check_due = 0;
            }
            if (sys_battery_level <= 0x2e3)
                sys_power_down();
        } else
            sys_idle_tick();
    } while (ui_timeout != 0);
    r = 0;
out:
    return r;
}

/*
 * Show the start-up picture D_2133924.
 */
void ui_show_startup_picture(void)
{
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2133924);
    func_00009A6();
}

/*
 * Start-up: the test-mode button code. Returns 1 (enter the test mode test_mode) when
 * A, C, A, B, C (press counters btn_a_presses / btn_c_presses / btn_a_presses / btn_b_presses /
 * btn_c_presses) are pressed in this order within 6 ticks of the 500 ms T16 ch.5 timer (about
 * 3 s after the start-up picture); any press out of order, the timeout, or a press
 * already counted on entry returns 0. Busy loop (no idle tick). Only caller sys_power_on.
 */
u8 test_check_entry_code(void)
{
    int st, n;

    if (btn_a_presses + btn_b_presses + btn_c_presses != 0)
        return 0;
    tmr_t5_start_timeout(500);
    n = st = 0;
    for (;;) {
        if (tmr_flags & 0x20) {
            tmr_flags &= ~0x20;
            if (++n > 5)
                return 0;
        }
        if (btn_a_presses) {
            btn_a_presses = 0;
            if (st == 0 || st == 2)
                st++;
            else
                return 0;
        }
        if (btn_b_presses) {
            btn_b_presses = 0;
            if (st != 3)
                return 0;
            st = 4;
        }
        if (btn_c_presses) {
            btn_c_presses = 0;
            if (st == 4)
                return 1;
            if (st != 1)
                return 0;
            st = 2;
        }
    }
}

extern const u8 D_255BFAC[];
extern const u8 D_255BFB8[];
/*
 * Power-on sequence (called by sys_startup_flow): logo (0x212A834), battery check (dead
 * battery: reset the RTC and freeze in sys_power_down), start-up screen and the test-mode
 * button code (test_check_entry_code -> the factory test mode test_mode, the only way into
 * it), then load the saved game from flash 0x200E000 if present: 1 = loaded, 0 = none
 * (sys_startup_flow then sets up a new game). The test mode's flash test blanks 0x200E000,
 * so a new game follows it.
 */
u8 sys_power_on(void)
{
    u8 sel, m, k;
    int r;
    u32 i;
    u16 *q;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x212a834);
    func_00009A6();
    snd_play(0x3d);
    snd_wait_end();
    r = sys_battery_low();
    if (r == 1) {
        btn_mask(7);
        INT_F16T45 = 0x40;
        INT_E16T45 = 0x44;
        tmr_flags = 0;
        tmr_t5_start_timeout(500);
        snd_enabled = r;
        do
            asm volatile("halt");
        while (!(tmr_flags & 0x20));
        tmr_flags &= ~0x20;
        anim_busy = 0;
        rtc_reset();
        INT_EP47_ERTC_EAD = 6;
        rtc_set_time(1, 1, 0, 0);
        rtc_irq_on();
        sys_power_down();
    }
    ui_show_startup_picture();
    if (test_check_entry_code()) {
        test_mode();
        lcd_palette_set = 0;
        ui_new_screen();
    }
    if (*(int *)FLASH_SAVE == -1)
        return 0;
    anim_busy = 0;
    rtc_reset();
    INT_EP47_ERTC_EAD = 6;
    rtc_irq_on();
    nfc_session.x001 = 2;
    nfc_power_up();
again:
    ui_new_screen();
    sel = 0;
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    k = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_line(k, 0x18, 0x20, 0x34, (int)D_255BFAC, 0, 8);
    k = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_line(k, 0x18, 0x30, 0x34, (int)D_255BFB8, 0, 8);
    m = gfx_object_create(6, 0x22, 0x20, 0, 0, 0x2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    for (;;) {
        if (btn_a_presses) {
            sel++;
            if (sel > 1)
                sel = 0;
            gfx_object_move_dirty(m, 6, sel * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
        }
        if (btn_b_presses) {
            if (sel != 0)
                goto other;
            q = (u16 *)FLASH_SAVE;
            for (i = 0; i <= 0x123; i++)
                ((u16 *)&save.pet)[i] = *q++;
            chara_select_pet_sprites_by_id(save.pet.x000);
            pet_wear_accessory(save.pet.worn_accessory);
            for (i = 0; i <= 0x293; i++)
                ((u16 *)save.friend_list)[i] = *q++;
            friend_book_state.open_after_comm = 0;
            friend_book_state.new_friend_waiting = 0;
            friend_book_state.x002 = 0;
            visit_record_clear();
            rtc_set_time(save.pet.clock_month, save.pet.clock_day, save.pet.clock_hour, save.pet.clock_minute);
            return 1;
        other:
            if (save_confirm_erase() != 0)
                return 0;
            goto again;
        }
        if (sys_display_state) {
            nfc_power_off();
            T16_CTL0 = 0;
            T16_CTL1 = 0;
            T16_CTL3 = 0;
            T16_CTL5 = 0;
            sys_display_off();
            while (sys_display_state)
                asm volatile("slp");
            if (sys_battery_check_due) {
                sys_battery_low();
                sys_battery_check_due = 0;
            }
            if (sys_battery_level <= 0x2e3)
                sys_power_down();
        } else
            sys_idle_tick();
    }
}

/*
 * SAVE GAME: wait for the sound (snd_wait_end), pet.saved_poop_display = pet_poops_drawn (remembers the
 * current animation/mood state at save time), then erase flash sector 8
 * (0x200E000..0x2010000, 8 KB) and program the character state block (0x124 halfwords =
 * 0x248 bytes from pet, i.e. RAM 0x119C..0x13E3 including the related globals)
 * followed by 0x294 halfwords (0x528 bytes) from friend_list, RAM 0xE9C..0x13C3: the friend
 * list and the state after it, which also covers 0x119C..0x13C3 - the pet record a second
 * time. The loader (sys_power_on, "つづきから") copies this block last, so the pet record
 * effectively comes from the second copy. One sector, erased and rewritten in place, no
 * checksum: only the first word is tested on loading (0xFFFFFFFF = no save).
 * Called after every event that changes the state (item use, NFC exchange, minigame...).
 */
void save_game(void)
{
    u32 i;
    u16 *q;

    snd_wait_end();
    save.pet.saved_poop_display = pet_poops_drawn;
    sys_irq_disable();
    q = (u16 *)FLASH_SAVE;
    func_0000A54(FLASH_BASE, FLASH_SEC_SAVE, FLASH_SEC_SAVE);
    for (i = 0; i <= 0x123; i++, q++)
        func_0000B52(q, ((u16 *)&save.pet)[i], FLASH_BASE);
    for (i = 0; i <= 0x293; i++, q++)
        func_0000B52(q, ((u16 *)save.friend_list)[i], FLASH_BASE);
    sys_irq_enable();
}

/*
 * Back up flash sector k (1-based) into the scratch sector 0xC (0x2040000): erase 0xC,
 * copy the whole sector k (flash_sector_offsets[k-1]..D_2538F24[k]) halfword by halfword. First half
 * of the "delete one item from a bank" operation (see flash_restore_sector_except).
 */
void flash_backup_sector(int k)
{
    u32 off;
    u16 *q;

    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_SECTOR_COPY, FLASH_SEC_SECTOR_COPY);
    q = (u16 *)FLASH_SECTOR_COPY;
    for (off = flash_sector_offsets[k - 1]; off < flash_sector_offsets[k]; off += 2, q++) {
        if (func_0000B52(q, *(u16 *)(off + FLASH_BASE), FLASH_BASE) != 0)
            break;
    }
    sys_irq_enable();
}

/*
 * Second half of "delete item": erase sector k and write the backup from 0x2040000 back,
 * skipping the n bytes at item p, which therefore stay erased (an empty slot a new record
 * can be programmed into).
 */
void flash_restore_sector_except(int k, void *p, int n)
{
    u32 off;
    u16 *src;

    sys_irq_disable();
    func_0000A54(FLASH_BASE, k, k);
    src = (u16 *)FLASH_SECTOR_COPY;
    for (off = flash_sector_offsets[k - 1]; off < flash_sector_offsets[k]; off += 2, src++) {
        u32 a = off + FLASH_BASE;
        if (a < (u32)p || a >= (u32)p + n) {
            if (func_0000B52((u16 *)(off + FLASH_BASE), *src, FLASH_BASE) != 0)
                break;
        }
    }
    sys_irq_enable();
}

u8 item_same_identity(int a, struct TamagoFile *q)
{
    struct TamagoFile *p = a;
    struct TamagoFile *p2, *q2;
    int c;
    u8 r;

    p2 = p;
    q2 = q;
    c = p->x04e;
    if (c != q->x04e)
        goto zero;
    if (p->x04f != q->x04f)
        goto zero;
    if (p->kind != q->kind)
        goto zero;
    if (p->sub_kind != q->sub_kind)
        goto zero;
    if (c == 0x81 || c == 0x94) {
        r = 0;
        if (p2->x05a != q2->x05a)
            goto out;
        if (p2->x05b != q2->x05b)
            goto out;
        if (((u8 *)p2)[0x9c] != ((u8 *)q2)[0x9c])
            goto out;
        r = 1;
        if (((u8 *)p2)[0x9d] == ((u8 *)q2)[0x9d])
            goto out;
        goto zero;
    }
    if ((p->hdr_word & 0xffff0000) == 0x2140000) {
        r = 1;
        if (p->x05b != q->x05b)
            goto zero;
        if (p->legacy_text == q->legacy_text)
            goto out;
        goto zero;
    }
    r = 1;
    if ((p->hdr_word & 0xffff0000) != 0x6010000)
        goto out;
    if (p->legacy_value == q->legacy_value)
        goto out;
zero:
    r = 0;
out:
    return r;
}

/*
 * Find the item a (pointer to a TAMAGO record) among the n slots of sz bytes starting at
 * p: index of the slot whose record matches (item_same_identity compares the ids), or -1.
 */
int item_find_in_bank(int a, u8 *p, int sz, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (item_same_identity(a, (struct TamagoFile *)(p + i * sz)))
            return i;
    }
    return -1;
}

/*
 * The same as item_find_in_bank through a global index list: position in q->x004[] of the owned
 * item (item_get_count(idx) != 0) whose record matches x, or -1.
 */
int item_find_in_inventory(const struct Inventory *q, int x)
{
    int i;

    for (i = 0; i < q->count; i++) {
        if (item_get_count(q->indexes[i]) == 0)
            continue;
        if (item_same_identity(x, item_record_by_index(q->indexes[i])))
            return i;
    }
    return -1;
}

/*
 * First free (erased) record among n records of size sz at base (asm).
 */
int flash_first_erased_record(u8 *base, int sz, int n)
{
    int i;
    u8 *s, *p;

    for (i = 0; i < n; i++) {
        s = base + i * sz;
        for (p = s; p < s + sz; p += 2)
            if (*(u16 *)p != 0xffff)
                break;
        if (p >= s + sz)
            break;
    }
    if (i >= n)
        return -1;
    return i;
}

/*
 * Find a free or matching record slot (asm).
 */
int item_bank_free_slot(int base, int sz, int n, int max, int cat)
{
    int i, cnt, found;
    u8 *s, *p;
    struct TamagoFile *e;

    cnt = 0;
    found = -1;
    for (i = 0; i < n; i++) {
        s = (u8 *)base + i * sz;
        for (p = s; p < s + sz; p += 2)
            if (*(u16 *)p != 0xffff)
                break;
        if (p >= s + sz && found < 0)
            found = i;
        if (((struct TamagoFile *)s)->x000_h == 0x4154) {
            if (cat == 0)
                cnt++;
            else {
                e = (struct TamagoFile *)s;
                if (((struct TamagoFile *)s)->x04e == 0x81 || ((struct TamagoFile *)s)->x04e == 0x94) {
                    if (e->kind == cat)
                        cnt++;
                } else if (((struct TamagoFile *)s)->x04e != 1)
                    cnt++;
                else if (((struct TamagoFile *)s)->x04f != 1)
                    cnt++;
                else if (((struct TamagoFile *)s)->sub_kind != 2 && ((struct TamagoFile *)s)->sub_kind != 3)
                    cnt++;
                else if (cat == 13)
                    cnt++;
            }
        }
    }
    if (cnt >= max)
        return -1;
    return found;
}

/*
 * Program an item record (size bytes from src) into slot idx of the bank at base (slot
 * stride = size). No erase: the slot must be empty (0xFF).
 */
void flash_program_record(u16 *src, u32 base, int size, int idx)
{
    u32 a;

    base += size * idx;
    sys_irq_disable();
    for (a = base; a < base + size; a += 2, src++) {
        if (func_0000B52((u16 *)a, *src, FLASH_BASE) != 0)
            break;
    }
    sys_irq_enable();
}

/*
 * Storage-full message screen (texts D_255BFC8...): "アイテムがいっぱいだよ /
 * すてるものをえらんでね!" (items are full, choose one to throw away), 2 s.
 */
void ui_show_items_full(void)
{
    u8 a;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    a = gfx_object_create(0x22, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x22, 0x20, 0x10, (int)D_255BFC8, 0, 9);
    a = gfx_object_create(0x1c, 0x30, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x1c, 0x30, 0x10, (int)D_255BFD4, 0, 9);
    a = gfx_object_create(0x1c, 0x40, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x1c, 0x40, 0x10, (int)D_255BFE4, 0, 9);
    a = gfx_object_create(0x1c, 0x50, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x1c, 0x50, 0x10, (int)D_255BFF4, 0, 9);
    func_00009A6();
    tmr_delay_ms_t5(2000);
}

/*
 * Next occupied slot after i in bank (download category) a, wrapping, optionally
 * filtered: c != 0 keeps only items whose kind byte x050 == c (0x35 also matches 0x34)
 * for type 0x81/0x94 items, or (for other types) anything but type-1 food of sub-kind 2/3
 * unless c == 13. Used by the item browsers.
 */
int item_bank_next_slot(int a, int i, int c)
{
    struct TamagoFile *p;

    for (;;) {
        if (++i >= item_bank_tbl[a].slot_count)
            i = 0;
        p = (struct TamagoFile *)item_bank_slot(a, i);
        if (p->x000 == 0xffffffff)
            continue;
        if (c == 0)
            break;
        if (p->x04e == 0x81 || p->x04e == 0x94) {
            if (p->kind == c)
                break;
            if (p->kind == 0x35 && c == 0x34)
                break;
        } else {
            if (p->x04e != 1)
                break;
            if (p->x04f != 1)
                break;
            if (p->sub_kind < 2 || p->sub_kind > 3)
                break;
            if (c == 13)
                break;
        }
    }
    return i;
}

/*
 * Bank full: let the user browse the stored records of bank (download category) a (slots
 * at c, stride d, e = start index, f = kind filter) with A = next, B = delete the shown
 * item (after the confirmation ui_confirm_really; sector backup/restore through
 * flash_backup_sector / flash_restore_sector_except, b = first sector of the bank), C = cancel. Returns the freed
 * slot index or -1. Bank 9 shows the item name, 7/8 and others draw the item card with
 * item_draw_card.
 */
int item_bank_discard_browser(int a, int b, int c, int d, int e, int f)
{
    u8 k;
    struct TamagoFile *p;
    int q;
    int r;

    snd_wait_end();
    btn_b_presses = 0;
    btn_c_presses = 0;
    k = item_bank_next_slot(a, e, f);
redraw:
    p = (struct TamagoFile *)(c + d * k);
    switch ((u32)a) {
    case 9:
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
        gfx_object_create(0x34, 0x24, 0x10, 0, 0, D_208DA68);
        item_draw_name(p, gfx_object_create(8, 0x41, 0x10, 0, 0, 0), 0x41, 0x10);
        break;
    default:
    case 7:
    case 8:
        item_draw_card(p, 0);
        break;
    }
    if (e > 1) {
        if (k < item_bank_next_slot(a, k, f))
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
        else
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
    }
    func_00009A6();
    btn_a_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses != 0) {
            k = item_bank_next_slot(a, k, f);
            goto redraw;
        }
        if (btn_b_presses != 0) {
            if (ui_confirm_really() != 0) {
                snd_wait_end();
                p = (struct TamagoFile *)(c + d * k);
                q = b + ((u32)(d * k) >> 16);
                flash_backup_sector(q);
                flash_restore_sector_except(q, p, d);
                r = k;
                goto out;
            }
            if (ui_timeout == 0)
                break;
            btn_b_presses = 0;
            btn_c_presses = 0;
            goto redraw;
        }
        {
            u8 t = btn_c_presses;
            if (t != 0)
                break;
        }
        sys_idle_tick();
        if (ui_timeout == 0)
            break;
    }
    r = -1;
out:
    return r;
}

#define RNG(l, h) ((v >= 1000 + l && v <= 1000 + h) || (v >= 2000 + l && v <= 2000 + h) \
    || (v >= 3000 + l && v <= 3000 + h) || (v >= 4000 + l && v <= 4000 + h) \
    || (v >= 5000 + l && v <= 5000 + h) || (v >= 6000 + l && v <= 6000 + h) \
    || (v >= 7000 + l && v <= 7000 + h) || (v >= 8000 + l && v <= 8000 + h) \
    || (v >= 9000 + l && v <= 9000 + h))
/*
 * Special item check: classify a type 0x81 item p by its id (item_id = id from
 * x05a/x05b, 1001..9999, the last three digits giving the kind): category 4 sub-kind 1
 * (coupons): x601-x749 -> 0x61, x801-x949 -> 0x62; category 8 (app items): x001-x299 ->
 * 0x63, x301-x599 -> 0x64, x601-x999 -> 0x65. Writes the class to *o and returns 1, or 0
 * when the item is none of these.
 */
int item_special_class(struct TamagoFile *p, int o)
{
    u16 v;
    u8 *out = (u8 *)o;
    struct TamagoFile *q = p;

    if (p->x04e == 0x81) {
        v = item_id(q);
        switch (q->x04f) {
        case 4:
            if (q->sub_kind == 1) {
                if (RNG(601, 749)) {
                    *out = 0x61;
                    return 1;
                }
                if (RNG(801, 949)) {
                    *out = 0x62;
                    return 1;
                }
            }
            break;
        case 8:
            if (RNG(1, 299)) {
                *out = 0x63;
                return 1;
            }
            if (RNG(301, 599)) {
                *out = 0x64;
                return 1;
            }
            if (RNG(601, 999)) {
                *out = 0x65;
                return 1;
            }
            break;
        }
    }
    return 0;
}

/*
 * Seasonal / downloaded room decoration active today ("きせつかざり"?): scan the bank
 * described by item_bank_tbl[15] (sector 0x62, 4 slots of 0x2000) for a record whose validity
 * window (bytes 0xF4-0xF5 = start month/day, 0xF6-0xF7 = end month/day, wrapping over the
 * year end) contains today's date (rtc_get_month = month, rtc_get_day = day); if found,
 * resolve its picture (item_load_pictures) into *(u32 *)a and return 1, else 0.
 */
int item_seasonal_decoration(int a)
{
    volatile u8 b0;
    volatile u8 b1;
    volatile u8 flag;
    volatile u32 start;
    volatile u32 end;
    volatile u32 now;
    volatile u32 now2;
    volatile u8 t[4];
    struct { u32 x000; u8 pad[56]; } res;
    u8 *p;
    u8 *e;
    u16 size;
    u8 n;
    u32 i;
    u8 r;
    u8 id;
    const struct ItemBank *bk;

    b0 = rtc_get_month();
    b1 = rtc_get_day();
    bk = &item_bank_tbl[15];
    id = bk->first_sector;
    size = bk->slot_size;
    n = bk->slot_count;
    p = flash_sector_addr(id);
    for (i = 0; i < n; i++) {
        e = p + size * i;
        if (*(u16 *)e == 0xffff)
            continue;
        t[0] = e[0xf4];
        t[1] = e[0xf5];
        t[2] = e[0xf6];
        t[3] = e[0xf7];
        flag = 0;
        if (t[0] <= t[2]) {
            start = (t[0] << 8) | t[1];
            end = (t[2] << 8) | t[3];
            now = (b0 << 8) | b1;
            if (now >= start && now <= end)
                flag = 1;
        } else {
            start = (t[0] << 8) | t[1];
            end = ((t[2] + 12) << 8) | t[3];
            now = (b0 << 8) | b1;
            now2 = ((b0 + 12) << 8) | b1;
            if (now >= start) {
                if (now <= end)
                    flag = 1;
            } else if (now2 <= end)
                flag = 1;
        }
        r = flag;
        if (r == 1) {
            item_load_pictures(&res.x000, (struct TamagoFile *)e, r);
            *(u32 *)a = res.x000;
            return r;
        }
    }
    return 0;
}

/*
 * Item 0x11: a new bingo board: pet.bingo_board = (random layout 1..3) << 12 with no
 * cells marked; pet.bingo_plays_today = 0.
 */
void spot_new_bingo_card(int k)
{
    if (k == 0x11) {
        save.pet.bingo_board = (util_rand_except(3, 3) + 1) << 12;
        save.pet.bingo_plays_today = 0;
    }
}

/*
 * Is k one of the special items 0x11/0x12?
 */
u8 item_bank_is_app_data(int k)
{
    u8 r;

    if (k == 0x11)
        r = 1;
    else if (k == 0x12)
        r = 1;
    else
        r = 0;
    return r;
}

/*
 * Items 7/8 (games/apps) erase the program execution area (sectors 1..4) when deleted.
 */
void dl_erase_program_area(int k)
{
    if (k >= 7 && k <= 8) {
        sys_irq_disable();
        func_0000A54(FLASH_BASE, FLASH_SEC_DL_PROGRAM, FLASH_SEC_DL_PROGRAM_LAST);
        sys_irq_enable();
    }
}

const u8 D_255BF88[2] = { 0x46, 0x60 };
const u8 D_255BF8C[] = _("ほんとうに?");
const u8 D_255BF9C[] = _("はい");
const u8 D_255BFA4[] = _("いいえ");
const u8 D_255BFAC[] = _("つづきから");
const u8 D_255BFB8[] = _("データをけす");
const u8 D_255BFC8[] = _("アイテムが");
const u8 D_255BFD4[] = _("いっぱいだよ");
const u8 D_255BFE4[] = _("すてるものを");
const u8 D_255BFF4[] = _("えらんでね!");

/*
 * Store / replace a received item record a in its bank: decide its bank index s from
 * type/category/sub-kind (0x81: cat 1 food (x051 1 meal -> bank 0, 2 snack -> 1), 2 -> 3,
 * 3 -> 2, 4 goods -> 4/5 (coupons 0x61/0x62 give images 0x219A1E0/0x2199D68 and banks
 * 15/16), 6 (x051 1) -> 9, 7 background -> 6, 8 app items -> 17/18 by class; 0x94 cat 2:
 * x051 1 -> 7 (games), 2 -> 8 (apps), 3 -> a downloadable character stored by
 * dl_chara_store), fill in the animation to play (*b = image, c->x000 = effect image) and
 * *d = s. If an identical item already exists in the bank it is replaced (delete +
 * write); otherwise take the first free slot (item_bank_free_slot) or ask the user to discard
 * one (item_bank_discard_browser; nfc_session.item_refused = -1 when refused). Then program the record
 * (flash_program_record), special-case bank 0x11 (spot_new_bingo_card: new bingo card -> random card
 * number) and re-erase sectors 1-4 for banks 7/8. Returns 1 when stored.
 * In the download pack (notes/findings/tamago.md): meals -> 0, snacks -> 1, clothes -> 2,
 * accessories -> 3, toys -> 4, coupons (ids 1601..1608, class 0x61) -> 16, seasonal
 * decorations (ids 1801..1806, class 0x62, date window at 0xF4) -> 15, rooms -> 6, bingo
 * cards (ids 1301..1308, class 0x64) -> 17, gashapon (ids 1602..1620, class 0x65) -> 18,
 * games -> 7, outings -> 8, characters -> dl_chara_store; no file uses bank 5 or 9.
 */
u8 item_store_in_bank(struct TamagoFile *a, int b, u32 *c, int d)
{
    struct TamagoFile *p = a;
    u8 x;
    int s;
    int w;
    int y;
    int z;
    int h, t2, r;
    u8 *buf;
    u8 *dst;
    u32 off;

    y = z = 0;
    if (b != 0)
        *(int *)b = 0;
    if (c != 0)
        item_load_pictures(c, a, 1);
    switch (p->x04e) {
    case 0x81:
        switch (p->x04f) {
        case 1:
            switch (p->sub_kind) {
            case 1:
                s = 0;
                if (b != 0)
                    *(int *)b = 0x20a85c4;
                break;
            case 2:
                s = 1;
                z = 13;
                if (b != 0)
                    *(int *)b = 0x20a85c4;
                break;
            }
            break;
        case 2:
            s = 3;
            if (b != 0)
                *(int *)b = 0x20b04dc;
            break;
        case 3:
            s = 2;
            if (b != 0)
                *(int *)b = 0x20b04dc;
            break;
        case 4:
            switch (p->sub_kind) {
            case 1:
                if (item_special_class(a, (int)&x) != 0) {
                    switch (x) {
                    case 0x62:
                        *(int *)b = 0;
                        *c = 0x2199d68;
                        s = 15;
                        break;
                    case 0x61:
                    default:
                        *(int *)b = 0;
                        *c = 0x219a1e0;
                        s = 16;
                        break;
                    }
                } else {
                    s = 4;
                    if (b != 0)
                        *(int *)b = 0x20b04dc;
                }
                break;
            case 2:
                s = 5;
                if (b != 0)
                    *(int *)b = 0x20b04dc;
                break;
            }
            break;
        case 6:
            switch (p->sub_kind) {
            case 1:
                s = 9;
                break;
            case 3:
                return 0;
            }
            break;
        case 7:
            s = 6;
            if (b != 0)
                *(int *)b = 0x20b04dc;
            if (c != 0)
                *c = 0x2199d68;
            break;
        case 8:
            if (item_special_class(a, (int)&x) == 0)
                break;
            switch (x) {
            case 0x63:
                *(int *)b = 0;
                *c = 0x2199ff0;
                s = 17;
                break;
            case 0x64:
                *(int *)b = 0;
                *c = 0x2199ff0;
                s = 17;
                break;
            case 0x65:
            default:
                *(int *)b = 0;
                *c = 0x2199eb0;
                s = 18;
                break;
            }
            break;
        }
        break;
    case 0x94:
        if (p->x04f == 2) {
            switch (p->sub_kind) {
            case 1:
                s = 7;
                if (b != 0)
                    *(int *)b = 0x20b04dc;
                *c = 0x2199eb0;
                break;
            case 2:
                s = 8;
                if (b != 0)
                    *(int *)b = 0x20ae08c;
                *c = 0x219a3d8;
                break;
            case 3:
                *(int *)b = 0;
                *c = 0x219a1e0;
                *(int *)d = 22;
                if (dl_chara_name_protected((u8 *)a) != 0) {
                    anim_dl_chars_full();
                    return 0;
                }
                if (dl_chara_store((int)a) == 0)
                    return 1;
                return 0;
            }
        }
        break;
    default:
        return 0;
    }
    if (comm_exchange_kind == 5 && comm_is_initiator != 0)
        anim_present_box((const u8 *)*c);
    t2 = item_bank_tbl[s].first_sector;
    w = item_bank_tbl[s].slot_size;
    h = item_bank_tbl[s].slot_count;
    if (y == 0)
        y = h;
    buf = flash_sector_addr(t2);
    *(int *)d = s;
    if (item_bank_is_app_data(s) == 0 && (r = item_find_in_bank((int)a, buf, w, h)) >= 0) {
        dst = buf;
        snd_wait_end();
        off = w * r;
        dst += off;
        t2 += off >> 16;
        flash_backup_sector(t2);
        flash_restore_sector_except(t2, dst, w);
        dl_erase_program_area(s);
    } else {
        r = item_bank_free_slot((int)buf, w, h, y, z);
        if (r < 0) {
            if (x == 0x62)
                comm_show_msg(1);
            else
                ui_show_items_full();
            r = item_bank_discard_browser(s, t2, (int)buf, w, h, z);
            if (r < 0) {
                snd_wait_end();
                nfc_session.item_refused = -1;
                return 0;
            }
            nfc_session.item_refused = 0;
        }
    }
    snd_wait_end();
    flash_program_record((u16 *)a, (u32)buf, w, r);
    spot_new_bingo_card(s);
    dl_erase_program_area(s);
    return 1;
}

/*
 * Light the notification mark of the menu matching item kind k.
 */
void ui_light_mark_for_bank(int k)
{
    u8 n;

    switch (k) {
    case 0:
    case 1:
        ui_set_mark(0, 1);
        return;
    case 18:
        n = 1;
        break;
    case 2:
    case 3:
    case 4:
    case 6:
    case 7:
    case 8:
        n = 2;
        break;
    case 17:
        n = 3;
        break;
    case 15:
    case 22:
        n = 4;
        break;
    case 16:
        n = 6;
        break;
    default:
        return;
    }
    ui_set_mark(n, 1);
}

/*
 * Receive item p (from NFC or a shop): store it with item_store_in_bank and play the matching
 * animation (anim_receive_item_outdoor(image, effect) / anim_receive_item(effect)), then light the
 * notification mark of the menu for the bank class (ui_light_mark_for_bank -> ui_set_mark(n, 1);
 * not a sound). Items 0x81/6/3 (item 0x52 in sector 0x7C, list item_inv_stamps; unclear: a
 * stamp card, a letter / coupon or a lottery item, the readings differ between files) go
 * through item_store_received with their pictures instead.
 * comm_exchange_kind == 5 is a special game mode (guest/visit?) with its own handling
 * (anim_good_result, comm_show_item_get). Returns 1 when the item was accepted.
 */
u8 item_receive(struct TamagoFile *p)
{
    const u8 *a;
    int b;
    int c;

    if (((volatile struct TamagoFile *)p)->x04e == 0x81) {
        if (p->x04f == 6) {
            if (p->sub_kind == 3) {
                item_load_pictures(&a, p, 1);
                return item_store_received(p) ? 1 : 0;
            }
        }
    }
    if (item_store_in_bank(p, (int)&b, &a, (int)&c)) {
        if (comm_exchange_kind == 5) {
            if (comm_is_initiator == 0) {
                anim_receive_item_outdoor((const u8 *)b, a);
                anim_good_result();
            } else
                anim_receive_item_outdoor((const u8 *)b, a);
            comm_show_item_get();
        } else {
            pet_add_happiness(1);
            if (b == 0)
                anim_receive_item();
            else
                anim_receive_item_outdoor((const u8 *)b, a);
            ui_light_mark_for_bank(c);
        }
        return 1;
    } else {
        if (comm_exchange_kind == 5) {
            if (comm_is_initiator == 0)
                anim_good_result();
        }
        return 0;
    }
}

/*
 * "Choose an item to throw away" for the banks addressed by a global index list s
 * (item_inv_picker = the picker UI; k = first sector of the bank, n = slot size): on
 * confirmation (ui_confirm_really) delete the chosen record (sector backup/restore,
 * flash_backup_sector / flash_restore_sector_except) and mark the index free (item_set_count(r, 0)). Nothing is
 * stored here. Returns the index freed or -1.
 */
int item_discard_from_inventory(const struct Inventory *s, int k, int n)
{
    u32 t;
    int r;
    int res;
    u8 *base;
    struct TamagoFile *q;
    int m;

    snd_wait_end();
    t = -1;again:
    r = item_inv_picker(s, 0, &t);
    res = -1;
    if (r < 0)
        goto out;
    if (ui_confirm_really()) {
            if ((comm_menu_choice == 1 && comm_submenu_choice == 0x13) || comm_exchange_kind == 5 || comm_exchange_kind == 3)
                comm_show_item_get();
            snd_wait_end();
            base = flash_sector_addr(k);
            q = item_record_by_index(r);
            m = k + (((u32)q - (u32)base) >> 16);
            flash_backup_sector(m);
            flash_restore_sector_except(m, q, n);
            item_set_count(r, 0);        res = r;
        goto out;
    }
    {
        u8 c = ui_timeout;
        res = -1;
        if (c)
            goto again;
    }
out:
    return res;
}

/*
 * Receive an item record p into the inventory (the item_record_by_index item slots): its flash
 * bank by type/category: type 1 or 0x81, category 1 -> bank sector 0x65 (0x800 slots,
 * index list item_inv_food), 2 -> 0x6B (0x2000, item_inv_accessories), 3 -> 0x66 (0x4000, item_inv_clothes), 4 ->
 * 0x70 (0x2000, item_inv_tools / item_inv_daily by sub-kind), 6 (sub 3) -> 0x7C (0x8000, item_inv_stamps),
 * 7 (background) -> 0x76 (single slot), 8 -> 0x7D (0x8000, item_inv_app_items). An existing copy of
 * the same id is rewritten in place and its ownership count increased (item_acquire, max
 * 3); else the first free global index (item_get_count == 0) is used, or the user picks one
 * to discard (item_discard_from_inventory; nfc_session.item_refused = -1 when refused), and it gets count 1. The
 * count is set to 3 instead (Fable: "new/unseen") when item_find_builtin_match says the item is a
 * gift (1/2) and comm_storing_gift == 1. Returns 1 when stored, 0 if not.
 */
#define COND if ((comm_menu_choice == 1 && comm_submenu_choice == 0x13) || comm_exchange_kind == 5 || comm_exchange_kind == 3 || item_no_full_dialog == 1) comm_show_item_get()
u8 item_add_to_inventory(struct TamagoFile *p)
{
    struct TamagoFile *tmp;
    const struct Inventory *list;
    u8 *base;
    int size;
    int k;
    struct TamagoFile *q;
    int idx;
    int i;
    int sv;
    u8 s;

    switch (p->x04e) {
    case 1:
        switch (p->x04f) {
        case 4:
            switch (p->sub_kind) {
            case 1:
                list = &item_inv_tools; k = 0x70; size = 0x2000;
                break;
            case 2:
                return 0;
            }
            break;
        case 2:
            list = &item_inv_accessories; k = 0x6b; size = 0x2000;
            break;
        case 7:
            list = 0; k = 0x76; size = 0x2000;
            break;
        default:
            return 0;
        }
        break;
    case 0x81:
        switch (p->x04f) {
        case 1: list = &item_inv_food; k = 0x65; size = 0x800; break;
        case 2: list = &item_inv_accessories; k = 0x6b; size = 0x2000; break;
        case 3: list = &item_inv_clothes; k = 0x66; size = 0x4000; break;
        case 4:
            switch (p->sub_kind) {
            case 1:
                list = &item_inv_tools; k = 0x70; size = 0x2000;
                break;
            case 2:
                list = &item_inv_daily; k = 0x70; size = 0x2000;
                break;
            }
            break;
        case 6:
            if (p->sub_kind != 3)
                return 0;
            list = &item_inv_stamps; k = 0x7c; size = 0x8000;
            break;
        case 7: list = 0; k = 0x76; size = 0x2000; break;
        case 8: list = &item_inv_app_items; k = 0x7d; size = 0x8000; break;
        }
        break;
    default:
        return 0;
    }
    base = flash_sector_addr(k);
    if (list) {
        idx = item_find_in_inventory(list, (int)p);
        if (idx >= 0) {
            idx = list->indexes[idx];
            q = item_record_by_index(idx);
            s = item_find_builtin_match((int)p, &tmp);
            if ((s == 1 || s == 2) && comm_storing_gift == 1)
                item_set_count(idx, 3);
            else
                item_acquire(idx);
        }
    } else {
        idx = 0;
        q = (struct TamagoFile *)flash_sector_addr(k);
    }
    if (idx >= 0) {
        COND;
        snd_wait_end();
        {
            u32 d = (u32)q - (u32)base;
            int m = k + (d >> 16);
            flash_backup_sector(m);
            snd_wait_end();
            flash_restore_sector_except(m, q, size);
            snd_wait_end();
            flash_program_record((u16 *)p, (u32)base, size, d / size);
        }
    } else {
        for (i = 0; i < list->count; i++) {
            idx = list->indexes[i];
            if (item_get_count(idx) == 0) {
                COND;
                snd_wait_end();
                q = item_record_by_index(idx);
                {
                    int m = k + (((u32)q - (u32)base) >> 16);
                    flash_backup_sector(m);
                    snd_wait_end();
                    flash_restore_sector_except(m, q, size);
                }
                break;
            }
        }
        if (i == list->count) {
            ui_show_items_full();
            sv = shop_get_discount_mode();
            shop_set_discount_mode(0);
            idx = item_discard_from_inventory(list, k, size);
            shop_set_discount_mode(sv);
            if (idx < 0) {
                snd_wait_end();
                nfc_session.item_refused = -1;
                return 0;
            }
            nfc_session.item_refused = 0;
        }
        COND;
        s = item_find_builtin_match((int)p, &tmp);
        if ((s == 1 || s == 2) && comm_storing_gift == 1)
            item_set_count(idx, 3);
        else
            item_set_count(idx, 1);
        q = item_record_by_index(idx);
        snd_wait_end();
        flash_program_record((u16 *)p, (u32)base, size, ((u32)q - (u32)base) / size);
    }
    return 1;
}

/*
 * Install a downloaded PROGRAM p (type 0x94 / category 2: the downloadable games and
 * outings) from its download slot into the execution area at the very start of the
 * flash, 0x2000000: returns 0 for any other type; skipped (returns 1) if the file
 * installed there has the same sub-kind and ids (x051, x05a, x05b). The body of a program
 * file is a list with ONE blob (u16 count 1 at +0x200, u16 blob length at +0x202, the
 * code + data from +0x204), so 0x204 + length = the whole file but its 2-byte checksum;
 * the 8 KB sectors 1..k+1 (0x2000000..) are erased and the file is programmed from
 * 0x2000000 halfword by halfword (IRAM func_0000B52), interrupts masked. The programs
 * are linked for this address header included (their code starts at 0x2000204 and
 * addresses its own data absolutely), keep their globals in RAM 0x84740..0x847BF and
 * call the firmware only through the API block 0x2400AD0..0x2401D92 (F8a); checked on the
 * 29 distinct programs of the download pack, notes/findings/tamago.md.
 *
 * THIS IS CODE: the town menu (F28 town_menu, app entries 8/9/12/13 of town_places,
 * items of sub-kind 2 in bank 8 = sectors 0x7A-0x7B) and the game corner (game/game_menu.c
 * game_menu, sub-kind 1 games in bank 7 = sector 0x79) call dl_install_program on the chosen
 * item and then JUMP to 0x2000000 + the big-endian u32 at item offset 0xAE..0xB1 (the
 * app's entry point), i.e. downloaded apps are native S1C33 code run in place from the
 * boot sectors (the item's data/sub-record table at +0x200 is read by F14/F16
 * comm_fill_record as well, and the bytes at +0xB8 are a stat vector applied after a won
 * game). The only checks on the way in are comm_validate_record on the exchange record that
 * carried the file (TAMAGO magic, size, additive checksum, the table-driven signature),
 * byte 0xFF bit 4 of the file (comm_check_item_flag) and its type/category bytes; the file's own
 * checksum and signature are not checked. The entry is called with no arguments; a game
 * returns 0 / a rank 1..5 / raw points, an outing non-zero when it gave something. No 0x94
 * item is present in this dump (banks 7 and 8 are erased).
 */
int dl_install_program(struct TamagoFile *p)
{
    u8 *base = (u8 *)p;
    struct TamagoFile *q;
    u8 *h;
    u16 n;
    u16 k;
    u16 *src;

    q = (struct TamagoFile *)FLASH_DL_PROGRAM;
    if (p->x04e != 0x94 || p->x04f != 2)
        return 0;
    if (q->sub_kind == p->sub_kind && q->x05a == p->x05a && q->x05b == p->x05b)
        return 1;
    h = (u8 *)p + 0x200;
    n = (h[2] << 8) + h[3] + 0x204;
    k = 0;
    if (n != 0)
        k = ((n + 0x1fff) >> 13) - 1;
    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_DL_PROGRAM, FLASH_SEC_DL_PROGRAM + k);
    h = (u8 *)FLASH_DL_PROGRAM;
    for (src = (u16 *)base; (u8 *)src < base + n; src++, h += 2)
        if (func_0000B52((u16 *)h, *src, FLASH_BASE))
            break;
    sys_irq_enable();
    return 1;
}

const s8 D_255C084[20] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21 };
const u8 D_255C098[] = _("れいぞうこ");
const struct Inventory item_inv_food = { D_255C098, D_255C084, 20 };

/*
 * Global item index a -> its record slot in flash: 0..21 food bank sector 0x65 (2 KB
 * slots, flash 0x25D0000), 22..36 sector 0x66 (16 KB), 37..56 sector 0x6B (8 KB), 57..81
 * sector 0x70 (8 KB), 82 sector 0x7C, 93..96 sector 0x7D (32 KB); else 0. Erased slots
 * read 0xFFFF. The index lists D_255C2C4.. (F36 data) enumerate these per category.
 */
struct TamagoFile *item_record_by_index(int a)
{
    u8 *r;

    switch (a) {
    case 0 ... 21:
        r = (u8 *)(a * 0x800) + (int)flash_sector_addr(FLASH_SEC_INV_FOOD);
        break;
    case 22 ... 36:
        r = (u8 *)((a - 22) * 0x4000) + (int)flash_sector_addr(FLASH_SEC_INV_CLOTHES);
        break;
    case 37 ... 56:
        r = (u8 *)((a - 37) * 0x2000) + (int)flash_sector_addr(FLASH_SEC_INV_ACCESSORIES);
        break;
    case 57 ... 81:
        r = (u8 *)((a - 57) * 0x2000) + (int)flash_sector_addr(FLASH_SEC_INV_GOODS);
        break;
    case 82:
        r = flash_sector_addr(FLASH_SEC_LETTERS);
        break;
    case 93 ... 96:
        r = (u8 *)((a - 93) * 0x8000) + (int)flash_sector_addr(FLASH_SEC_BINGO);
        break;
    default:
        r = 0;
        break;
    }
    return (struct TamagoFile *)r;
}


/*
 * Load the picture list of record p into c (up to d pictures): the record body after the
 * 0x200 header holds big-endian length-prefixed pictures (asm).
 */
int item_load_pictures(u32 *c, u8 *q, int d)
{
    struct TamagoFile *p;
    int i, n, len;

    p = (struct TamagoFile *)q;
    if (p == 0)
        return 0;
    if (p->x000_h == 0xffff)
        return 0;
    q += 0x200;
    switch (p->x04e) {
    case 1:
        if (p->x04f == 2)
            q += 0x312;
        break;
    case 0x81:
        switch (p->x04f) {
        case 2:
            n = (q[0] << 8) | q[1];
            q += n;
            q += 2;
            break;
        case 6:
            q += 0x44;
            break;
        case 0x80:
            break;
        }
        break;
    }
    n = (q[0] << 8) | q[1];
    q += 2;
    if (n > d)
        n = d;
    for (i = 0; i < n; i++) {
        len = (q[0] << 8) | q[1];
        q += 2;
        c[i] = (u32)q;
        q += len;
    }
    for (; i < d; i++)
        c[i] = 0;
    return n;
}


/*
 * Prize record k of a lottery record p (asm).
 */
int item_prize_record(int k, struct TamagoFile *p)
{
    u8 *q;
    int i, n, len;
    int f;
    u8 c;

    q = (u8 *)p;
    f = 0;
    if (p == 0)
        return 0;
    if (p->x000_h == 0xffff)
        return 0;
    q += 0x200;
    if (p->x04e != 0x81)
        return 0;
    switch (p->x04f) {
    case 6:
        c = p->sub_kind;
        q += 0x44;
        if (c == 3)
            break;
    default:
        return 0;
    case 8:
        n = (q[0] << 8) | q[1];
        q += 2;
        for (i = 0; i < n; i++) {
            len = (q[0] << 8) | q[1];
            q += 2;
            q += len;
        }
        break;
    }
    n = (q[0] << 8) | q[1];
    q += 2;
    for (i = 0; i < n; i++) {
        len = (q[0] << 8) | q[1];
        q += 2;
        if (f)
            q += 4;
        q += len;
    }
    n = (q[0] << 8) | q[1];
    q += 2;
    if (n <= k)
        return 0;
    for (i = 0; i < n; i++) {
        len = (q[0] << 8) | q[1];
        q += 2;
        if (i == k)
            break;
        q += len;
    }
    return (int)q;
}


/*
 * Text block of record p (category 0x0881...) (asm).
 */
int item_body_value(struct AnimPicSlots *a, int k, struct TamagoFile *p)
{
    u8 *q;
    int i, n, len;
    int v;

    q = (u8 *)p;
    if (p == 0)
        return 0;
    if (p->x000_h == 0xffff)
        return 0;
    q += 0x200;
    if ((p->hdr_word & 0xffff0000) != 0x8810000)
        return 0;
    n = (q[0] << 8) | q[1];
    q += 2;
    for (i = 0; i < n; i++) {
        len = (q[0] << 8) | q[1];
        q += 2;
        q += len;
    }
    n = (q[0] << 8) | q[1];
    q += 2;
    for (i = 0; i < n; i++) {
        len = (q[0] << 8) | q[1];
        q += 2;
        v = q[3] | (q[2] << 8) | (q[1] << 16) | (q[0] << 24);
        if (i == k)
            break;
        q += len;
    }
    return v;
}

/*
 * Sub-record a of an "app data" item p (type 0x81, category 8 (checked through the u32 at
 * 0x4C), 0x8000 bytes: the Bingo / Gashapon data): after the 0x200-byte header come three
 * big-endian-counted lists of length-prefixed blobs (graphics / palettes / text?), then a
 * count and n fixed 0x68-byte records; returns the address of record a, or 0 (empty slot,
 * other type, a >= n).
 */
int spot_app_record(int a, struct TamagoFile *p)
{
    u8 *q;
    int i, n, len;

    q = (u8 *)p;
    if (p == 0)
        return 0;
    if (p->x000_h == 0xffff)
        return 0;
    q += 0x200;
    if ((p->hdr_word & 0xffff0000) != 0x8810000)
        return 0;
    n = (q[0] << 8) | q[1];
    q += 2;
    for (i = 0; i < n; i++) {
        len = (q[0] << 8) | q[1];
        q += 2;
        q += len;
    }
    n = (q[0] << 8) | q[1];
    q += 2;
    for (i = 0; i < n; i++) {
        len = (q[0] << 8) | q[1];
        q += 2;
        q += len;
    }
    n = (q[0] << 8) | q[1];
    q += 2;
    for (i = 0; i < n; i++) {
        len = (q[0] << 8) | q[1];
        q += 2;
        q += len;
    }
    n = (q[0] << 8) | q[1];
    q += 2;
    if (n <= a)
        return 0;
    for (i = 0; i < n; i++) {
        if (i == a)
            break;
        q += 0x68;
    }
    return (int)q;
}


/*
 * Data block of record p (asm).
 */
int item_data_block(int out, int k, struct TamagoFile *p)
{
    u8 *q;
    int i, n, len;
    int v;

    q = (u8 *)p;
    if (p == 0)
        return 0;
    if (p->x000_h == 0xffff)
        return 0;
    q += 0x200;
    if ((p->hdr_word & 0xffff0000) != 0x8810000)
        return 0;
    for (i = 0; i < 2; i++) {
        len = (q[0] << 8) | q[1];
        q += 2;
        if (k == 4 && i == 1) {
            *(int *)out = (int)q;
            return v;
        }
        q += len;
    }
    n = (q[0] << 8) | q[1];
    q += 2;
    i = 0;
    do {
        len = (q[0] << 8) | q[1];
        q += 2;
        v = q[3] | (q[2] << 8) | (q[1] << 16) | (q[0] << 24);
        q += 4;
        if (i == k) {
            *(int *)out = (int)q;
            return v;
        }
        i++;
        q += len;
    } while (i <= n);
    return v;
}

struct AppItemRates {
    u8 pad[0x9c];
    u8 rates[8];
};
/*
 * Weighted random pick 0..3 for an app item p (category 8): the four bytes 0xA2..0xA5
 * (0x9E + 4..7) of the record are percentages; returns the first index whose running sum
 * exceeds a random number 0..99 (the Gashapon prize rarity table?), 3 by default.
 */
u8 spot_gashapon_rarity_pick(struct TamagoFile *p)
{
    int i;
    int sum;
    u16 lim;

    if (p != 0 && p->x000 != 0xffffffff && p->x04f == 8) {
        lim = util_rand_except(100, 100);
        sum = 0;
        for (i = 4; i <= 7; i++) {
            sum += ((struct AppItemRates *)((u8 *)p + 2))->rates[i];
            if (sum > lim)
                return i - 4;
        }
    }
    return 3;
}

/*
 * Item id of record p: the big-endian u16 at 0x5A for type 0x81/0x94 items, else the kind
 * byte 0x50 (legacy built-in records); -1 for a null pointer or an empty slot.
 */
int item_get_id(struct TamagoFile *p)
{
    int r = -1;

    if (p != 0 && p->x000_h != 0xffff) {
        if (p->x04e == 0x81 || p->x04e == 0x94)
            r = p->x05b | (p->x05a << 8);
        else
            r = p->kind;
    }
    return r;
}

/*
 * Friend portrait storage: the 30 friend records friend_list/D_0000E9D[k] each own a
 * 0xE80-byte image record in the friend picture banks starting at sector 9 (17 per 64 KB
 * sector, sector = 9 + slot/17, i.e. sectors 9..11).
 *
 * friend_portrait_backup: back up (flash_backup_sector) the sector of friend k's slot.
 */
void friend_portrait_backup(int k)
{
    flash_backup_sector(friend_portrait_slot(k) / 0x11 + FLASH_SEC_PORTRAITS);
}

/*
 * Store friend k's picture data p (0xE80 bytes from p + 0x118) after friend_portrait_backup: assign
 * slot a = friend_portrait_slot(k) (save.friend_list[k].met_flags[1] = a + 1, i.e. x015 of the friend_list
 * record), rewrite the sector without the old record (flash_restore_sector_except) and program the new
 * image (flash_program_record).
 */
void friend_portrait_store(int k, u8 *p)
{
    u8 a;
    u8 b;

    a = friend_portrait_slot(k);
    save.friend_list[k].met_flags[1] = a + 1;
    b = a / 0x11 + FLASH_SEC_PORTRAITS;
    a = a % 0x11;
    flash_restore_sector_except(b, (flash_sector_addr(b) + a * 0xe80), 0xe80);
    flash_program_record((u16 *)(p + 0x118), (u32)flash_sector_addr(b), 0xe80, a);
}

/*
 * Erase flash sector a (1-based), interrupts masked.
 */
void flash_erase_sector(int p0)
{
    sys_irq_disable();
    func_0000A54(FLASH_BASE, p0, p0);
    sys_irq_enable();
}

/*
 * Copy flash sector a to the scratch sector 12 (0x2040000), skipping the range b
 * (returns ok).
 */
int flash_sector_to_scratch(int a, int b)
{
    int ok = 1;
    u32 i;
    int n;
    u16 *dst;

    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_SECTOR_COPY, FLASH_SEC_SECTOR_COPY);
    dst = (u16 *)FLASH_SECTOR_COPY;
    n = 0;
    for (i = flash_sector_offsets[a - 1]; i < flash_sector_offsets[a]; i += 2) {
        if (func_0000B52(dst, *(u16 *)(i + FLASH_BASE), FLASH_BASE) != 0) {
            ok = 0;
            break;
        }
        n += 2;
        if (n == b)
            break;
        dst++;
    }
    sys_irq_enable();
    return ok;
}

/*
 * Restore flash sector a from the scratch sector, except range b.
 */
int flash_sector_from_scratch(int a, int b)
{
    int ok = 1;
    u32 i;
    int n;
    int j;
    u8 *src;

    sys_irq_disable();
    func_0000A54(FLASH_BASE, a, a);
    src = (u8 *)FLASH_SECTOR_COPY;
    j = 0;
    n = 0;
    for (i = flash_sector_offsets[a - 1]; i < flash_sector_offsets[a]; i += 2) {
        if (func_0000B52((u16 *)(i + FLASH_BASE), *(u16 *)(src + j), FLASH_BASE) != 0) {
            ok = 0;
            break;
        }
        n += 2;
        if (n == b)
            break;
        j += 2;
    }
    sys_irq_enable();
    return ok;
}

/*
 * Program n bytes from src to flash dst; returns 1 if all succeeded.
 */
int flash_program(u8 *dst, const u8 *src, int n)
{
    int ok = 1;
    int i;
    int off;

    sys_irq_disable();
    for (i = 0, off = 0; i < n / 2; i++, off += 2) {
        if (func_0000B52((u16 *)(dst + off), *(u16 *)(src + off), FLASH_BASE) != 0) {
            ok = 0;
            break;
        }
    }
    sys_irq_enable();
    return ok;
}

/*
 * Load the stamp-card pictures (sector 0x7C + 0x8000, one 4 KB picture per stamp
 * collected, pet.stamp_count) into the game-variable slots.
 */
void item_load_stamp_pictures(void)
{
    int i;
    u8 *p;

    p = flash_sector_addr(FLASH_SEC_LETTERS) + 0x8000;
    for (i = 0; i <= 2; i++) {
        if (i < save.pet.stamp_count)
            anim_pic_slots[2][i + 1] = p;
        else
            anim_clear_pic_slot(i + 7);
        p += 0x1000;
    }
}

/*
 * Store a newly received stamp (from the packet at 0x27D0400) into the stamp card (max
 * 3); returns the stamp count.
 */
u8 item_store_received_stamp(void)
{
    const u8 *kept[3];
    int i;
    int off;
    u8 n;
    u8 *p;
    u8 *s;

    if (save.pet.stamp_count > 3)
        return save.pet.stamp_count;
    s = (u8 *)(FLASH_COMM_RX + 0x400);
    flash_begin();
    n = save.pet.stamp_count;
    off = n * 0x1000 + 0x8000;
    flash_sector_to_scratch(0x7c, off);
    flash_sector_from_scratch(0x7c, off);
    p = flash_sector_addr(FLASH_SEC_LETTERS) + off;
    for (i = 0; i <= 2; i++)
        kept[i] = anim_pic_slots[0][i];
    item_load_pictures(anim_pic_slots[0], (struct TamagoFile *)(s + 0x14), 3);
    flash_program(p, anim_pic_slots[0][2], 0x1000);
    if (++save.pet.stamp_count > 3)
        save.pet.stamp_count = 3;
    for (i = 0; i <= 2; i++)
        anim_pic_slots[0][i] = kept[i];
    save_game();
    return save.pet.stamp_count;
}

/* the saved game (struct SaveBlock: its layout is the flash save's) */
struct SaveBlock save;
