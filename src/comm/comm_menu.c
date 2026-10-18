/*
 * The communication menu and its sessions (greeting, item exchange, games, photo, proposal,
 * app, debug), the handling of received items (capsules, stamps, letters), and a run of shared
 * helpers at its end (button clear, flash begin/end, byte order, picture slots, traits).
 *
 * Code 24F71DA..24F913A, .rodata from 2536DD4.
 */
#include "proto/comm/comm_menu.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/shop/shop_browser.h"
#include "proto/diary/diary_notebook.h"
#include "proto/mail/mail_letters.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/ir/ir_session.h"
#include "proto/ui/ui_message.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/comm/comm_record_pics.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/ir/ir_receive.h"
#include "proto/ir/ir_packets.h"
#include "proto/ir/ir_driver.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/test/test_debug_chara.h"
#include "proto/spot/spot_reader.h"
#include "proto/test/test_mode.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/comm/comm_outcome.h"
#include "proto/nfc/nfc_session.h"
#include "proto/comm/comm_exchange.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * F7 = the menu system and the Tamagotchi-to-Tamagotchi ("つうしん") session logic.
 * Menus are tables of {label text, handler function pointer} (comm_main_menu_items / comm_main_menu_debug_items =
 * the main menu in its two variants, comm_tama4u_menu_items / ir_point_gift_menu_items = sub menus) driven by
 * ui_menu_run_plain(table, &index) (returns 0 = done, 1 = entry chosen, 2 = cancelled, 3 =
 * timeout) and ui_yes_no(title, yes, no, ..) / ui_message_box (message boxes).
 * Handlers return 1 to leave the menu, 0 to stay.
 *
 * Prepare a communication (menu entry): copy the RAM-resident IR/timer routines
 * (sys_copy_ram_routines; the timer-5 handler at 0xD94 is needed by the live delays tmr_t5_pause /
 * EE2) and split pet.owner_id into ir_owner_id_lo/D_00840B5 (two 6-bit values, from its
 * low and high byte). pet.owner_id is the random non-zero owner id drawn when a new game is set
 * up (sys_startup_flow, or the debug set) and sent at 0x72 of the exchange record. The two
 * halves go
 * into bits 0-5 of bytes 2 and 4 of the legacy IR packet (ir_point_gift_packets and friends).
 */
void comm_prepare(void)
{
    sys_copy_ram_routines();
    ir_owner_id_lo = save.pet.owner_id & 0x3f;
    ir_owner_id_hi = (save.pet.owner_id >> 8) & 0x3f;
}

/*
 * After a communication: show the result picture 0x208B004 and wait 10 ticks. On timeout
 * / C, roll back the up-to-3 friends queued in comm_mail_queue[0..2] (each a friend index whose
 * message records, found by mail_find(friend, kind 0xF, ...), are deleted with
 * mail_store and the rest renumbered with mail_renumber_friends) and clear the pending-event
 * record friend_book_state; when A/B is pressed early, open the friend book friend_book_open instead.
 */
void comm_result_wait(void)
{
    u8 buf[100];
    int n;
    int i, j, r;

    ui_new_screen_with_picture(0, 0x208b004);
    btn_wait_press_or_timeout(10);
    snd_wait_before_flash();
    if (ui_timeout == 0 || btn_c_presses != 0) {
        if (btn_c_presses != 0)
            snd_wait_end();
        for (i = 0; i <= 2; i++) {
            if (((u8 *)&comm_mail_queue)[i] != 0xff) {
                for (j = 0, n = 100; j <= 0x63; j++) {
                    r = mail_find(((u8 *)&comm_mail_queue)[i], 0xf, buf, -1, (int)&n);
                    if (r >= 0) {
                        snd_wait_before_flash();
                        sys_irq_disable();
                        mail_store(((u8 *)&comm_mail_queue)[i], r, 1, (int)buf, 0);
                        sys_irq_enable();
                    }
                }
                mail_renumber_friends(((u8 *)&comm_mail_queue)[i]);
                ((u8 *)&comm_mail_queue)[i] = 0xff;
            }
        }
        friend_book_state.open_after_comm = 0;
        friend_book_state.new_friend_waiting = 0;
        friend_book_state.x002 = 0;
        visit_record_clear();
        comm_friendbook_updated = 0;
        save_after_sound();
    } else {
        ui_timeout = 10;
        if (btn_a_presses + btn_b_presses != 0) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            ui_new_screen();
            friend_book_open();
            comm_friendbook_updated = 0;
            friend_book_state.open_after_comm = 0;
            visit_record_clear();
            save_after_sound();
        }
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
    }
}

/*
 * Same as comm_result_wait (result wait + rollback of the new friends' messages), but sets
 * friend_book_state.open_after_comm/x001 = 1 before opening the friend book friend_book_open.
 */
void comm_result_wait_book(void)
{
    u8 buf[100];
    int n;
    int i, j, r;

    ui_new_screen_with_picture(0, 0x208b004);
    btn_wait_press_or_timeout(10);
    snd_wait_before_flash();
    if (ui_timeout == 0 || btn_c_presses != 0) {
        if (btn_c_presses != 0)
            snd_wait_end();
        for (i = 0; i <= 2; i++) {
            if (((u8 *)&comm_mail_queue)[i] != 0xff) {
                for (j = 0, n = 100; j <= 0x63; j++) {
                    r = mail_find(((u8 *)&comm_mail_queue)[i], 0xf, buf, -1, (int)&n);
                    if (r >= 0) {
                        snd_wait_before_flash();
                        sys_irq_disable();
                        mail_store(((u8 *)&comm_mail_queue)[i], r, 1, (int)buf, 0);
                        sys_irq_enable();
                    }
                }
                mail_renumber_friends(((u8 *)&comm_mail_queue)[i]);
                ((u8 *)&comm_mail_queue)[i] = 0xff;
            }
        }
        friend_book_state.open_after_comm = 0;
        friend_book_state.new_friend_waiting = 0;
        friend_book_state.x002 = 0;
        visit_record_clear();
        comm_friendbook_updated = 0;
        save_after_sound();
    } else {
        ui_timeout = 10;
        if (btn_a_presses + btn_b_presses != 0) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            ui_new_screen();
            friend_book_state.open_after_comm = 1;
            friend_book_state.new_friend_waiting = 1;
            friend_book_state.x002 = 0;
            friend_book_open();
            comm_friendbook_updated = 0;
            friend_book_state.open_after_comm = 0;
            visit_record_clear();
            save_after_sound();
        }
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
    }
}

/*
 * Communication menu handlers (all the same shape): run an NFC session through
 * nfc_menu_session(menu choice n), restore the ports (ir_power_down), stop sound, battery
 * check, and when the partner slot comm_partner_slot == 30 (a visiting character record) deal
 * with its mail (comm_result_wait_book). Return 1 = exchange happened (leave the menu).
 * Numbering (see F69 nfc_menu_session and notes/findings/sessions.md): "menu choice" = the
 * argument of nfc_menu_session, "session type" = nfc_session.x003 = record byte 0x4C.
 * This one: "4Uアプリ" (main-menu table comm_main_menu_items), the smartphone-app download: menu
 * choice 6 = session type 8, role 2 (target). Type 8 makes nfc_find_peer_target report the
 * pseudo peer 0xF0F0 without an NFC-DEP wait, so the F70b exchange runs its "phone"
 * branch (the app is the logical initiator, codes 0 / 0x10): nfc_receive = SNEP server
 * receiving the phase-1 header record, our reply by SNEP PUT (nfc_send ->
 * nfc_snep_client), then the phase-2 record carrying the TAMAGO file (section 3), which
 * comm_outcome_app / item_receive files into the item bank of its type (type 0x94 / 2 / 2 = an
 * executable app into bank 8, see F28).
 */
int comm_menu_app(void)
{
    int n;

    ir_answer_only = 0;
    n = nfc_menu_session(6);
    ir_power_down();
    snd_wait_end();
    if (sys_battery_low() == 1)
        sys_power_down();
    if (n > 0)
        return 1;
    return 0;
}

/*
 * Debug menu "アイテムSEND" (debug table comm_main_menu_debug_items): menu choice 7 = session type 8, role 1
 * (initiator): the toy plays the app's part and sends the item at flash 0x2070000 (else
 * the default background record) in a bare type-8 record (comm_build_header, comm_payload_app_init).
 * It does not send to the app: the initiator finds its peer with nfc_detect_target, which only
 * accepts an NFC-DEP target with NFCID3 F0 40, while a toy in "4Uアプリ" (type 8 target)
 * uses the SNEP path and an idle toy refuses type 8 (0x58). The intended receiver is
 * unknown (a factory tool emulating a toy?).
 */
int comm_menu_item_send(void)
{
    int n;

    ir_answer_only = 0;
    n = nfc_menu_session(7);
    ir_power_down();
    snd_wait_end();
    if (sys_battery_low() == 1)
        sys_power_down();
    if (n > 0)
        return 1;
    return 0;
}

/*
 * Result handling stub (uses an uninitialised n: dead code?).
 */
int comm_result_unused(void)
{
    int n;

    ir_power_down();
    if (n == 2) {
        comm_result_wait_book();
        return 1;
    }
    if (n > 0)
        return 1;
    return 0;
}

/*
 * Hidden communication menu entry ("\n" at index 4 of comm_main_menu_items and index 6 of
 * comm_main_menu_debug_items, both beyond the menu count, so not shown): the old IR session 4
 * (ir_session(4) -> ir_session_run). ir_answer_only = 1 = answer only: the toy never sends the
 * ping and waits for a peer's, which can never be seen on the 4U+ (no P04 edge ISR), so
 * it waits about 10 s (C cancels) and returns. Reachable only by a test-strap trick: with
 * P06 low choose debug entry 4 or 5 of comm_main_menu_debug_items, its handler returns 0, P06 high when
 * comm_main_menu loops; the normal table is then used with the kept index 4 and B runs this
 * (comm_menu_enable(4) returns 2 = accepted). Never in normal play (P06 pulled up).
 */
int ir_menu_hidden_session(void)
{
    ir_point_gift_active = 0;
    ir_answer_only = 1;
    ir_session(4);
    ir_power_down();
    return 0;
}

/*
 * Unreferenced (no call, no table entry, no word in the image): IR communication mode 5
 * (ir_session(5), 20 s wait) with friend-list update. Dead.
 */
int ir_session5_unused(void)
{
    int r;
    ir_answer_only = 0;
    r = ir_session(5);
    ir_power_down();
    if (comm_partner_slot == 30) {
        if (save.friend_list[comm_partner_slot].x000_b == 0) {
            visit_record_clear();
        } else {
            comm_result_wait_book();
            visit_record_clear();
            return 1;
        }
    }
    if (r > 0)
        return 1;
    return 0;
}

/*
 * Give item a to another Tamagotchi: menu choice 3 = session type 5, initiator (the
 * receiver is an idle toy), with the item file comm_gift_item = a; friend list refresh,
 * battery check.
 */
u8 comm_give_item(int a)
{
    int n;

    comm_gift_item = a;
    ir_answer_only = 0;
    n = nfc_menu_session(3);
    ir_power_down();
    if (comm_partner_slot == 30) {
        if (save.friend_list[comm_partner_slot].x000_b == 0) {
            visit_record_clear();
        } else {
            comm_result_wait_book();
            visit_record_clear();
            snd_wait_end();
            if (sys_battery_low() == 1)
                sys_power_down();
            return 1;
        }
    }
    snd_wait_end();
    if (sys_battery_low() == 1)
        sys_power_down();
    if (n > 0)
        return 1;
    return 0;
}

/*
 * Unreferenced (no call, no table entry, no word in the image): IR communication mode 3
 * (ir_session(3)) after the stub check ir_session3_check_stub. Dead.
 */
int ir_session3_unused(void)
{
    int n;

    ir_session3_check_stub();
    ir_answer_only = 0;
    n = ir_session(3);
    ir_power_down();
    if (comm_partner_slot == 30) {
        if (save.friend_list[comm_partner_slot].x000_b == 0) {
            visit_record_clear();
        } else {
            comm_result_wait_book();
            visit_record_clear();
            return 1;
        }
    }
    if (n > 0)
        return 1;
    return 0;
}

/*
 * Debug menu "デバックセット": test_debug_set_character (debug data set).
 */
int comm_menu_debug_set(void)
{
    test_debug_set_character();
    return 0;
}

#define REFRESH() do { snd_wait_end(); if (sys_battery_low() == 1) sys_power_down(); } while (0)
/*
 * Commemorative photo with another Tamagotchi: menu choice 4 = session type 7, initiator.
 */
int comm_photo_session(void)
{
    int r;
    ir_answer_only = 0;
    r = nfc_menu_session(4);
    ir_power_down();
    if (comm_partner_slot == 30) {
        if (save.friend_list[comm_partner_slot].x000_b == 0) {
            visit_record_clear();
        } else {
            comm_result_wait_book();
            visit_record_clear();
            REFRESH();
            return 1;
        }
    }
    REFRESH();
    if (r > 0)
        return 1;
    return 0;
}

/*
 * "さつえいする?" (take a photo?) する/やめる dialog -> comm_photo_session (menu choice 4 =
 * session type 7, the commemorative photo exchange).
 */
int comm_menu_photo(void)
{
    int v = 0;
    u8 done;

    comm_prepare();
    do {
        switch (ui_yes_no(D_2536DD8, D_2536DE8, D_2536DF0, 16, &v)) {
        case 1:
            switch (v) {
            case 0:
                done = comm_photo_session();
                break;
            case 1:
                return 0;
            }
            break;
        case 2:
            return 0;
        case 3:
            return 1;
        }
    } while (!done);
    sys_idle_seconds = 0;
    return 1;
}

/*
 * Set the age in days pet.age_days = 5 (debug helper?).
 */
int pet_debug_set_age_5days(void)
{
    save.pet.age_days = 5;
    return 0;
}

/*
 * Returns 1 (stub).
 */
int ir_session3_check_stub(void)
{
    return 1;
}

#define REFRESH_24F7768() do { snd_wait_end(); if (sys_battery_low() == 1) sys_power_down(); } while (0)
/*
 * NFC communication with menu choice a (nfc_menu_session(a)) and the usual friend/battery
 * handling; only caller the proposal menu comm_menu_proposal with menu choice 5.
 */
u8 comm_session_menu(u8 a)
{
    int r;
    ir_answer_only = 0;
    r = nfc_menu_session(a);
    ir_power_down();
    REFRESH_24F7768();
    if (comm_partner_slot == 30) {
        if (save.friend_list[comm_partner_slot].x000_b == 0) {
            visit_record_clear();
        } else {
            comm_result_wait_book();
            visit_record_clear();
            REFRESH_24F7768();
            return 1;
        }
    }
    REFRESH_24F7768();
    if (r > 0)
        return 1;
    return 0;
}

/*
 * Proposal (プロポーズ): younger pets (life stage < 3) get "まだプロポーズできないよ!";
 * adults choose する (propose: nfc_session.x002 = 2, the target, which waits 30 s for the
 * partner and sends its proposal line, comm_build_header), うける (accept: x002 = 1, the
 * initiator) or やめる, then the session comm_session_menu(5) = menu choice 5 = session type 6
 * with that role (verified in the asm). Both sides must use this menu: an idle toy
 * refuses type 6 (comm_exchange_target). Outcome and refusal rules: F66 comm_outcome_marriage /
 * comm_can_marry.
 */
int comm_menu_proposal(void)
{
    int ret = 0;
    int x = ret;
    u32 r;

    if (save.pet.life_stage < 3) {
        ui_message_box(D_2536DFC, D_2536E0C, D_2536DF8, D_2536DF8, 16);
        return ret;
    }
    x = 0;
    do {
        r = ui_question_two_lines_b((int)D_2536E1C, (int)D_2536DE8, (int)D_2536E28, (int)D_2536DF0, 0, (u32 *)(&x));
        switch (r) {
        case 2:
            return 0;
        case 1:
            break;
        case 3:
        default:
            return 1;
        }
        switch (x) {
        case 0:
            nfc_session.x002 = 2;
            break;
        case 1:
            nfc_session.x002 = 1;
            break;
        case 2:
        default:
            return 0;
        }
    } while (!comm_session_menu(5));
    sys_idle_seconds = 0;
    return 1;
}

/*
 * Returns 0x1 (stub).
 */
int comm_stub_one(void)
{
    return 1;
}

/*
 * "けっこん" (marry?) する/しない dialog: 0 yes, 1 no, 2/3 cancel/timeout.
 */
int comm_ask_marry(void)
{
    int v = 0;
    int res;

    switch (ui_yes_no(D_2536E30, D_2536DE8, D_2536E3C, 16, &v)) {
    case 1:
        switch (v) {
        case 0:
            res = 0;
            break;
        case 1:
            res = 1;
            goto out;
        default:
            res = 1;
            break;
        }
        break;
    case 2:
        res = 2;
        break;
    case 3:
    default:
        res = 3;
        goto out;
    }
out:
    return res;
}

/*
 * Message "このともだちには/このともだちからは ... プロポーズできないよ" depending on the
 * exchange kind comm_exchange_kind (== 7; IR-era, the NFC proposal uses F66 comm_proposal_refused_msg).
 */
int ir_show_proposal_refusal(u8 k)
{
    u8 f = 0;

    if (comm_exchange_kind == 7)
        f = 1;
    if (k == 1) {
        if (f == 1)
            ui_message_box(D_2536E44, D_2536E1C, D_2536E0C, 0, 8);
        else
            ui_message_box(D_2536E58, D_2536E1C, D_2536E0C, 0, 8);
    }
    return 0;
}

/*
 * Animation for the connection result comm_game_result with the third character 1 or 3.
 */
void comm_result_anim(void)
{
    anim_just_married();
    switch (comm_game_result) {
    case 0:
        chara_select_fifth_by_id(1);
        anim_wedding_couple();
        break;
    case 1:
        chara_select_fifth_by_id(1);
        anim_wedding_couple();
        break;
    case 2:
        chara_select_fifth_by_id(3);
        anim_wedding_couple();
        break;
    case 3:
        chara_select_fifth_by_id(3);
        anim_wedding_couple();
        break;
    }
}

/*
 * Scrolling message text comm_proposal_texts[k - 16] (communication results).
 */
void comm_show_proposal_line(u8 k)
{
    int i = k - 16;

    if ((u8)i > 44)
        i = 45;
    ui_show_message(11, 0, comm_proposal_texts[i]);
}

/*
 * Show the 38-byte name/message received at 0x27D5460.
 */
void comm_show_rx_message(void)
{
    u8 buf[40];
    u8 *p = (u8 *)(FLASH_COMM_RX + 0x5460);
    u32 i;

    memset(buf, 0, 40);
    for (i = 0; i <= 0x25; i++)
        buf[i] = *p++;
    anim_msg_comm_a(buf);
}

/*
 * Record the connection result comm_game_result (pet_start_new_egg).
 */
void comm_record_result(void)
{
    pet_start_new_egg(comm_game_result);
}

/*
 * Record the connection result comm_game_result (pet_start_new_egg); the same as
 * comm_record_result.
 */
void comm_result_show_record(void)
{
    pet_start_new_egg(comm_game_result);
}

/*
 * Load the pictures of the received file at 0x27D5414 into the game variables.
 */
void comm_load_rx_pictures(void)
{
    item_load_pictures((u32 *)&anim_pic_slots, (struct TamagoFile *)(FLASH_COMM_RX + 0x5414), 6);
}

/*
 * Walk a received profile record's sub-lists at p (big-endian u16 count, then count x
 * (u16 length + bytes)): with more than 2 entries, skip the first list and point *out at
 * the data of the second list's first entry (+2 = past its length); with 1 or 2 entries,
 * at the first entry's data; 0 if the list is empty. Used to find the partner's portrait
 * image inside the record at 0x27D5514.
 */
void comm_find_portrait(u8 **out, u8 *p)
{
    u32 n;
    int hi, len;
    u16 i;

    hi = (u16)(*p << 8);
    p++;
    n = hi | *p;
    p++;
    *out = 0;
    if (n > 2) {
        for (i = 0; i < n; i++) {
            hi = (u16)(*p << 8);
            p++;
            len = hi | *p;
            p++;
            p += len;
        }
        hi = (u16)(*p << 8);
        p++;
        n = hi | *p;
        p++;
        if (n == 0)
            return;
    } else {
        if (n == 0)
            return;
    }
    p += 2;
    *out = p;
}

/*
 * Show the received item (pictures of 0x27D5414 / 0x27D5514) with a frame depending on its
 * category (+0x4C 0x0681..., +0x51).
 */
void comm_show_rx_item(void)
{
    u8 *img;
    int k;
    struct TamagoFile *p;
    const u8 *s;

    item_load_pictures((void *)anim_pic_slots, (void *)(FLASH_COMM_RX + 0x5414), 3);
    k = 0;
    comm_find_portrait(&img, (u8 *)(FLASH_COMM_RX + 0x5514));
    p = comm_attached_file((void *)k);
    if ((p->hdr_word & 0xffff0000) == 0x6810000) {
        switch (p->sub_kind) {
        case 3:
            k = 1;
            break;
        case 1:
            k = 2;
            break;
        }
    }
    if (k == 1)
        s = D_208D718;
    else if (k == 2)
        s = D_208DA68;
    else {
        s = img;
        s += 0x104;
    }
    anim_present_box((const u8 *)s);
    asm("nop");
}

/*
 * After receiving a gift: load the partner images (anim_pic_slots <- 0x27D5414), and unless the
 * item stored in sector 0x86 (flash_sector_86_item) is the special goods item 0x3AB8 (category 4,
 * sub-kind 1 = a coupon?), play the generic "got an item" animation anim_gift_given.
 */
void comm_gift_received(void)
{
    u8 f = 0;
    u8 t;

    item_load_pictures((void *)anim_pic_slots, (void *)(FLASH_COMM_RX + 0x5414), 3);
    if (flash_sector_86_item.x04e == 0x81) {
        if (flash_sector_86_item.x04f == 4) {
            t = flash_sector_86_item.sub_kind;
            if (t == 1) {
                if (item_get_id(&flash_sector_86_item) == 0x3ab8)
                    f = t;
            }
        }
    }
    if (f == 0)
        anim_gift_given();
    __asm__("nop");
}

/*
 * Capsule / lottery item (0x81, category 6, sub-kind 3; Gashapon-type or stamp-card
 * prize): draw a prize class 0/1/2 with the percentages stored at p->x0b4[0..2]; -1 for
 * other items, 0 for an empty slot.
 */
int item_capsule_prize_class(struct TamagoFile *p)
{
    u8 t[3];
    int i;
    int n;
    int r = -1;

    if (p == 0)
        return 0;
    if (p->x000_h == 0xffff)
        return 0;
    if (p->x04e == 0x81) {
        if (p->x04f == 6) {
            if (p->sub_kind != 3)
                return r;
            for (i = 0; i <= 2; i++)
                t[i] = p->prize_percent[i];
            n = rand() % 100;
            if (n < t[0])
                r = 0;
            else if (n < t[0] + t[1])
                r = 1;
            else
                r = 2;
        }
    }
    return r;
}

/*
 * Select the second character for the item type (food 0x3C/0x3D, toy 0x46, accessory
 * 0x48).
 */
void item_select_presenter(struct TamagoFile *p)
{
    if (p->x04e != 0x81)
        return;
    switch (p->x04f) {
    case 1:
        if (p->sub_kind == 1)
            chara_select_other_by_set(0x3c);
        else
            chara_select_other_by_set(0x3d);
        break;
    case 4:
        chara_select_other_by_set(0x46);
        break;
    case 2:
    case 3:
        chara_select_other_by_set(0x48);
        break;
    }
}

/*
 * Count the received items liked by the visiting character (pet.stamp_liked_count).
 */
void pet_count_stamp_liked(struct TamagoFile *p)
{
    if (item_chara_preference(p, visit_partner.chara_id) == 1)
        save.pet.stamp_liked_count++;
}

/*
 * Use a capsule/lottery item p (stamp card): count it (pet_count_stamp_liked: pet.stamp_liked_count++ when
 * item_chara_preference(p, visit_partner.chara_id) == 1: liked by the visiting character (Opus) or the
 * daily item (Fable)); after 3 or more stamps (pet.stamp_count > 2) clear flag 0x52
 * (item_set_count), draw a prize class (forced to 0 when pet.stamp_liked_count > 2), take the prize
 * record q = item_prize_record(class, p), show it (anim_capsule_prize), store it (item_add_to_inventory with
 * item_no_full_dialog = 1 = "no inventory-full dialog") and save.
 */
int item_use_capsule(struct TamagoFile *p)
{
    int k;
    struct TamagoFile *q;

    pet_count_stamp_liked(p);
    if (save.pet.stamp_count > 2) {
        item_set_count(0x52, 0);
        k = item_capsule_prize_class(p);
        if (k == -1)
            return 0;
        if (save.pet.stamp_liked_count > 2)
            k = 0;
        q = (struct TamagoFile *)item_prize_record(k, p);
        if (q != 0) {
            item_load_pictures(anim_pic_slots[0], q, 3);
            item_select_presenter(q);
            anim_capsule_prize(anim_pic_slots[0][0]);
            asm("nop");
            asm("nop");
            item_no_full_dialog = 1;
            item_add_to_inventory(q);
            item_no_full_dialog = 0;
            ir_stamp_peers_clear();
        } else {
            asm("nop");
            asm("nop");
            return 0;
        }
    }
    save_game();
    return 1;
}

/*
 * Index of item p in the stamp list item_inv_stamps (or -1).
 */
int item_stamp_index(struct TamagoFile *p)
{
    flash_sector_addr(FLASH_SEC_LETTERS);
    return item_inv_find_owned_id(&item_inv_stamps, item_get_id(p));
}

/*
 * Store a received item p (item_add_to_inventory) with the receiving animation; stamp-card items
 * update item 0x52.
 */
int item_store_received(struct TamagoFile *p)
{
    u8 v;
    int f;

    if (comm_exchange_kind == 5) {
        anim_present_box(D_208D718);
    } else {
        anim_pic_slots[2][2] = D_208D92C;
        anim_receive_item();
    }
    if (item_stamp_index(p) >= 0) {
        v = item_get_count(0x52);
        f = 1;
    } else {
        v = 3;
        f = 0;
    }
    item_no_full_dialog = 1;
    if (item_add_to_inventory(p) == 0) {
        item_no_full_dialog = 0;
        return 0;
    }
    item_no_full_dialog = 0;
    if (f == 0)
        ir_stamp_peers_clear();
    item_set_count(0x52, v);
    save_with_short_sound();
    return 1;
}

const u8 D_2536DD8[] = _("さつえいする?");
const u8 D_2536DE8[] = _("する");
const u8 D_2536DF0[] = _("やめる");
const u8 D_2536DF8[] = _("");
const u8 D_2536DFC[] = _("まだプロポーズ");
const u8 D_2536E0C[] = _("できないよ!");
const u8 D_2536E1C[] = _("プロポーズ");
const u8 D_2536E28[] = _("うける");
const u8 D_2536E30[] = _("けっこん");
const u8 D_2536E3C[] = _("しない");
const u8 D_2536E44[] = _("このともだちには");
const u8 D_2536E58[] = _("このともだちからは");
const u8 comm_proposal_texts[79][50] = {
    _("ぼくと…\nけっこんしてください!!"),
    _("おれとけっこん\nしてくれないか!!"),
    _("わたくしとけっこん\nしていただけますか?"),
    _("ぼくと\nけっこんしてくれないかな"),
    _("ぼくとけっこんして\nほしいんだぞう"),
    _("ぼくと\nけっこんしてほしいだっち"),
    _("オレとキミでけっこん\nのゴールをきめよう!"),
    _("せっしゃとけっこん\nしてほしいでござる★"),
    _("ぽくと\nけっこんしてほしいのぽ★"),
    _("おれとけっこんしたら\nハッピーでしょ♪"),
    _("ぼくとけっこんできる\nなんてラッキーだよ!!"),
    _("ぼくと\nけっこんしてほしいころ♥"),
    _("ボ､ボクとけっこんして★\nいただけないでしょうか?\c"),
    _("キミはボクとけっこん\nするうんめいなんだ!!"),
    _("キミのためにぼくは\nプロポーズするよ♥"),
    _("わたしをおよめさんに\nしてください♥"),
    _("HI★ダーリン!\nミーとけっこんしよう♪"),
    _("あたしとけっこんして\nちょんまげ♥"),
    _("ちゃまとけっこんして\nくださいです～♥"),
    _("あたしとけっこんして\nください♪"),
    _("あたしとスイーツな\nけっこんしてぺろ★"),
    _("わらわは､おぬしのおよめ\nさんになりたいじょよ♪"),
    _("わたくしとけっこん\nしませんこと・・・?"),
    _("わたしもぴちぴちの\nおよめさんになりたいぴち"),
    _("アイリス・・・あなたを\nずっとたいせつにします!"),
    _("ゴーゴー!!いっきに\nけっこん♥だよー!!"),
    _("あげ～!あたしきめたわ★\nけっこんだね!!"),
    _("わたし・・・\nけっこんしたいわた♥"),
    _("わたしとけっこん\nするなちゅ?"),
    _("スクープだわさ!!あたし\nプロポーズするだわさ★"),
    _("わたし♥あなたのかわいい\nおよめさんになります♪"),
    _("わたしギラギラの\nけっこんがしたいのよ★"),
    _("わたしと…けっこん\nしていただけますか?"),
    _("オレとけっこん\nしてほしいんだ★"),
    _("だっち～!ボクとけっこん\nしてほしいだっち～!"),
    _("せっしゃと\nいざ!けっこんでござる"),
    _("おれとけっこんして\nほしいんだけど!"),
    _("わたしとけっこん\nしていただけますか?"),
    _("アイラブユー\nだよ♥♥♥"),
    _("あたしをおよめさんに\nしてちょんまげ★"),
    _("けっこんしてほしい\nですワンッ♥"),
    _("あたしとけっこんして\nほしいデビ～★"),
    _("わたくしとけっこんして\nほしいのですわよね?"),
    _("わたしとけっこんして\nほしいの★"),
    _("あたしとけっこん\nしてほしいだわさ～!"),
    _("けっこんしてください♥\n"),
    _("かんげきです!!\nしあわせになりましょう♥"),
    _("ありがとう♥\nしあわせになろうね♪"),
    _("ありがとう★でも\nごめんなさい・・・@"),
    _("ごめんなさい@\nおことわりします↓"),
    _("おれのよめに\nなってくれい!がっはっは"),
    _("ぼくとステキなせかいに\nとびたとう↑"),
    _("わたしだけのプリンセスに\nなってくれませんか?!"),
    _("ぼくとステキな\nクリスマスをすごそうよ★"),
    _("ぽくとけっこんして\nほしいんだっぺ!"),
    _("あなたとのけっこん…\nいいかもですね♥"),
    _("みみっちとけっこんして\nほしいぴょん!"),
    _("わたしとけっこんして\nくれませんか?"),
    _("ももとけっこんして\nほしいのね♥"),
    _("わたくしと…けっこんして\nいただけますか?"),
    _("わたくしとけっこんして\nいただけますわよねっ?"),
    _("スキーですいすいなの!\nけっこんしてちょんまげ♥"),
    _("ぼくとけっこん\nしてくれだっちYO!"),
    _("せっしゃのプロポーズを\nうけてくれ!でござる★"),
    _("ちゃま､あなたちゃまと\nけっこんするです～↑"),
    _("ミーとけっこんして\nラブラブになろうよ!"),
    _("ズバリ!きみはおれと\nけっこんしたいでしょ!"),
    _("おれがおまえと\nけっこんしてやるぜ★"),
    _("ボクとけっこんして\nほしいんですな～↑"),
    _("わたくしとけっこんして\nほしいんですの♥"),
    _("あたしとけっこん\nしましょう…うふふっ♥"),
    _("わたしとけっこんして\nくださいわた～"),
    _("さあ､ぼくと\nけっこんしよう!"),
    _("わたしと\nけっこんしましょう!"),
    _("ピポポピププピパピポ♥\n(けっこんしてくれ♥)"),
    _("オレとけっこんして\nほしいでヤンス!"),
    _("♥♥ギガキュン♥♥\nうちとけっこんして～♥♥"),
    _("ギガキュン↑↑\nあなたの♥をいとめたいわ"),
    _("{00}"),
};

/*
 * Can item record p be given away / sold ("あげる")? type 1: categories 2 and 4 (sub 1);
 * type 0x81: category 1 with kind 1, 2, 3, 4 (sub 1/2); everything else (and type 0x82)
 * no.
 */
u8 item_can_give(struct TamagoFile *p)
{
    switch (p->x04e) {
    case 1:
        switch (p->x04f) {
        case 0: case 1: case 3: case 5: case 6: case 7: case 8:
            return 0;
        case 2:
            return 1;
        case 4:
            if (p->sub_kind == 1)
                return 1;
            return 0;
        }
        return 0;
    case 0x81:
        switch (p->x04f) {
        case 0: case 5: case 6: case 7: case 8:
            return 0;
        case 1:
            if (p->kind == 1)
                return 1;
            return 0;
        case 2:
        case 3:
            return 1;
        case 4:
            switch (p->sub_kind) {
            case 1:
                return 1;
            case 2:
                return 1;
            }
            return 0;
        }
        return 0;
    case 0x82:
    default:
        return 0;
    }
}

/*
 * Receive a letter: animation, store it, show it (mail_show_letter_item).
 */
void item_receive_letter(int p0)
{
    int v0;

    anim_present_box((const u8 *)p0);
    item_store_received_stamp();
    v0 = item_record_by_index(0x52);
    mail_show_letter_item((struct TamagoFile *)v0);
    tmr_t5_wait_100ms(0x19);
}

/*
 * Receive a letter (other animation), store and show it.
 */
void item_receive_letter2(int p0)
{
    int v0;

    anim_result_alt();
    item_store_received_stamp();
    v0 = item_record_by_index(0x52);
    mail_show_letter_item((struct TamagoFile *)v0);
    tmr_t5_wait_100ms(0x19);
}

/*
 * Pick the "receiving an item" presentation for record p by its type / category /
 * sub-type (built-in 1 / downloaded 0x81 / 0x94): *a = the gift-box picture (0x20C273C for
 * most, 0x20B292C for a 0x94/2/1 program item), *b = the effect image (0x2199D68
 * backgrounds, 0x2199EB0 / 0x219A3D8 program items); in game mode 5 the special coupon
 * 0x3AB8 triggers item_receive_letter/func_24F7DF2 (its own reveal + 2.5 s wait). Returns 0 for
 * items that cannot be received (category 4 sub 2, category 6 sub 3).
 */
int item_receive_presentation(struct TamagoFile *p, int *a, u32 *b)
{
    int flag = 0;
    struct TamagoFile *q = p;

    if (a != 0)
        *a = 0;
    if (b != 0)
        item_load_pictures(b, p, 1);
    switch (q->x04e) {
    case 1:
        switch (q->x04f) {
        case 1:
            switch (q->sub_kind) {
            case 1:
                if (a != 0) *a = 0x20c273c;
                break;
            case 2:
            case 3:
                if (a != 0) *a = 0x20c273c;
                break;
            }
            break;
        case 2:
            if (a != 0) *a = 0x20c273c;
            break;
        case 3:
            break;
        case 4:
            switch (q->sub_kind) {
            case 1:
                if (a != 0) *a = 0x20c273c;
                break;
            case 2:
                return 0;
            default:
                if (a != 0) *a = 0x20c273c;
                break;
            }
            break;
        case 7:
            if (a != 0) *a = 0x20c273c;
            if (b != 0) *b = 0x2199d68;
            break;
        }
        break;
    case 0x81:
        switch (q->x04f) {
        case 1:
            switch (q->sub_kind) {
            case 1:
                if (a != 0) *a = 0x20c273c;
                break;
            case 2:
                if (q->kind == 12 && a != 0) *a = 0x20c273c;
                if (q->kind == 13) {
                    if (a != 0) *a = 0x20c273c;
                }
                break;
            }
            break;
        case 2:
        case 3:
            if (a != 0) *a = 0x20c273c;
            break;
        case 4:
            switch (q->sub_kind) {
            case 1:
                if (item_get_id(p) == 0x3ab8)
                    flag = 1;
                if (a != 0) *a = 0x20c273c;
                break;
            case 2:
                if (a != 0) *a = 0x20c273c;
                break;
            }
            break;
        case 6:
            switch (q->sub_kind) {
            case 1:
                break;
            case 3:
                return 0;
            }
            break;
        case 7:
            if (a != 0) *a = 0x20c273c;
            if (b != 0) *b = 0x2199d68;
            break;
        case 8:
            if (a != 0) *a = 0x20c273c;
            break;
        }
        break;
    case 0x94:
        if (q->x04f == 2) {
            switch (q->sub_kind) {
            case 1:
                if (a != 0) *a = 0x20b292c;
                *b = 0x2199eb0;
                break;
            case 2:
                if (a != 0) *a = 0x20c273c;
                *b = 0x219a3d8;
                break;
            }
        }
        break;
    default:
        return 0;
    }
    if (comm_exchange_kind == 5) {
        if (comm_is_initiator == 0) {
            if (flag) {
                item_receive_letter(p);
                return 1;
            }
        } else {
            if (flag) {
                item_receive_letter2(p);
                return 1;
            }
        }
        anim_present_box((const u8 *)*b);
    }
    return 1;
}

/*
 * Item exchange result handler: a = the exchange kind comm_exchange_kind (the IR kind of
 * ir_session, which the NFC gift outcome comm_outcome_gift sets to the side): 4 = giving side
 * (the item in our sent record b is checked and item_consume marks it as given), 5 =
 * receiving side (store the item of the received record c via item_add_to_inventory or
 * item_receive). b / c = our sent and the received record: 0x27C0800 / 0x27D0800 from the
 * NFC outcome handler (F66, session type 5), 0x27C5428 / 0x27D5400 from the old IR
 * exchange.
 */
int comm_item_exchange_result(int a, struct ExchangeRecord *b, struct ExchangeRecord *c)
{
    int t0;
    int t1;
    struct TamagoFile *p;
    struct TamagoFile *q;

    switch (a) {
    case 4:
        p = comm_attached_file(b);
        q = comm_attached_file(b);
        if (q->x04e == 0x81) {
            if (q->x04f == 4 && q->sub_kind == 1 && item_get_id(q) == 0x3ab8)
                return;
        }
        if (item_can_give(p)) {
            if ((comm_gift_source.flags & 3) == 3) {
                item_consume(comm_gift_source.item_index);
                save_with_short_sound();
            }
            btn_disable();
            return 1;
        }
        return;
    case 5:
        p = comm_attached_file(c);
        if (item_can_give(p)) {
            item_receive_presentation(p, &t0, &t1);
            if (item_add_to_inventory(p)) {
                snd_wait_end();
                save_game();
                btn_disable();
                return 1;
            }
        } else {
            return item_receive(p);
        }
        break;
    }
    btn_disable();
    return 0;
}

/*
 * Number of items in download category 6/7/8... for entry k.
 */
int item_bank_has_items(int k)
{
    int r;
    int ret;

    switch (k) {
    case 0:
        r = item_bank_count(6, 0);
        break;
    case 1:
        r = item_bank_count(7, 0);
        break;
    case 2:
        r = item_bank_count(8, 0);
        break;
    case 3:
        r = item_bank_count(0, 0);
        break;
    case 4:
        r = item_bank_count(1, 0);
        break;
    default:
        return 0;
    }
    ret = 1;
    if (r <= 0)
        ret = 0;
    return ret;
}

const u8 D_2537E7C[] = _("リビング");
const u8 D_2537E88[] = _("ミニゲーム");
const u8 D_2537E94[] = _("おでかけさき");
const u8 D_2537EA4[] = _("ごはん");
const u8 D_2537EAC[] = _("おやつ");
const struct MenuTable comm_give_shop_menu_items = {
    5, item_bank_has_items, {
        { D_2537E7C, 0 },
        { D_2537E88, 0 },
        { D_2537E94, 0 },
        { D_2537EA4, 0 },
        { D_2537EAC, 0 },
    }
};

/*
 * The おみせ (shop items) branch of "あげる" (give): sub-menu comm_give_shop_menu_items リビング (living room
 * items) / ミニゲーム (mini game items) / おでかけさき (outing) / ごはん (meals) / おやつ
 * (snacks): pick an item from the respective bank browser (item_bank_browser(bank, ..)), then
 * give it over NFC with comm_give_item (menu choice 3 = session type 5).
 */
u8 comm_give_shop_item(void)
{
    int a = 0;
    int b;
    int n;
    u32 r;

    for (;;) {
        comm_gift_source.flags &= ~3;
        comm_gift_source.item_index = 0;
        r = ui_menu_run_plain(&comm_give_shop_menu_items, &a);
        switch (r) {
        case 0:
            sys_idle_seconds = 0;
            return 1;
        case 1:
            b = -1;
            switch (a) {
            case 0:
                n = item_bank_browser(6, 0, 0, &b);
                break;
            case 1:
                n = item_bank_browser(7, 0, 0, &b);
                break;
            case 2:
                n = item_bank_browser(8, 0, 0, &b);
                break;
            case 3:
                n = item_bank_browser(0, 0, 0, &b);
                break;
            case 4:
                n = item_bank_browser(1, 0, 0, &b);
                break;
            }
            if (ui_timeout == 0)
                return 1;
            if (n != 0) {
                if (comm_give_item(n)) {
                    asm("nop");
                    sys_idle_seconds = 0;
                    return 1;
                }
                asm("nop");
            }
            break;
        case 2:
            return 0;
        case 3:
            return 1;
        default:
            sys_idle_seconds = 0;
            return 1;
        }
    }
}

/*
 * Is there an owned item in list k (food/snack/clothes/accessories/tools...)?
 */
int item_list_has_owned(int k)
{
    int ret;

    switch (k) {
    case 0:
        if (item_inv_usable_count(&item_inv_fridge) > 0)
            ret = 1;
        else
            ret = 0;
        break;
    case 1:
        if (item_inv_usable_count(&item_inv_clothes) > 0)
            ret = 1;
        else
            ret = 0;
        break;
    case 2:
        if (item_inv_usable_count(&item_inv_accessories) > 0)
            ret = 1;
        else
            ret = 0;
        break;
    case 3:
        if (item_inv_usable_count(&item_inv_tools) > 0)
            ret = 1;
        else
            ret = 0;
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

/*
 * The もちもの (inventory) branch of "あげる" (give): sub-menu comm_give_inventory_menu_items れいぞうこ
 * (fridge) / ようふく (clothes) / アクセサリー (accessories) / おどうぐ (tools), picker
 * item_inv_picker over the index lists item_inv_fridge/2E0/2EC/2D4, then comm_give_item.
 */
u8 comm_give_inventory_item(void)
{
    int a = 0;
    int b;
    int n = -1;
    u32 r;
    struct TamagoFile *q;

again:
    comm_gift_source.flags &= ~3;
    comm_gift_source.item_index = 0;
    r = ui_menu_run_plain(&comm_give_inventory_menu_items, &a);
    switch (r) {
    case 0:
    out:
        sys_idle_seconds = 0;
        return 1;
    case 1:
        b = -1;
        switch (a) {
        case 0:
            n = item_inv_picker((const struct Inventory *)(&item_inv_fridge), 0, (u32 *)(&b));
            break;
        case 1:
            n = item_inv_picker((const struct Inventory *)(&item_inv_clothes), 0, (u32 *)(&b));
            break;
        case 2:
            n = item_inv_picker((const struct Inventory *)(&item_inv_accessories), 0, (u32 *)(&b));
            break;
        case 3:
            n = item_inv_picker((const struct Inventory *)(&item_inv_tools), 0, (u32 *)(&b));
            break;
        }
        if (ui_timeout == 0)
            return 1;
        if (n >= 0) {
            q = item_record_by_index(n);
            if (q == 0) {
                asm("nop");
                return 0;
            }
            comm_gift_source.item_index = n;
            comm_gift_source.flags |= 3;
            if (comm_give_item(q)) {
                asm("nop");
                comm_gift_source.item_index = 0;
                comm_gift_source.flags &= ~3;
                goto out;
            }
            asm("nop");
        }
        goto again;
    case 2:
        return 0;
    case 3:
        return 1;
    default:
        sys_idle_seconds = 0;
        return 1;
    }
}

/*
 * "あげる" (give): choose もちもの (inventory, comm_give_inventory_item) or おみせ (shop items,
 * comm_give_shop_item).
 */
int comm_give_menu(void)
{
    int v = 0;
    u8 done;

    comm_prepare();
    do {
        switch (ui_yes_no(D_2536DF8, D_2537F54, D_2537F60, -16, &v)) {
        case 1:
            switch (v) {
            case 0:
                done = comm_give_inventory_item();
                break;
            case 1:
                done = comm_give_shop_item();
                break;
            }
            break;
        case 2:
            return 0;
        case 3:
            return 1;
        }
    } while (!done);
    sys_idle_seconds = 0;
    return 1;
}

/*
 * The "アイテムこうかん" (item exchange) entry: "あげる" (give) -> comm_give_menu.
 */
int comm_menu_item_exchange(void)
{
    int v = 0;
    u8 done;

    comm_prepare();
    do {
        switch (ui_yes_no(D_2536DF8, D_2537F68, D_2536DF0, -16, &v)) {
        case 1:
            switch (v) {
            case 0:
                done = comm_give_menu();
                break;
            case 1:
                return 0;
            }
            break;
        case 2:
            return 0;
        case 3:
            return 1;
        }
    } while (!done);
    sys_idle_seconds = 0;
    return 1;
}

/*
 * Returns 0 (stub).
 */
int comm_menu_enable_zero(void)
{
    return 0;
}

/*
 * Connection game choice k -> comm_game_choice = 1..4.
 */
int comm_set_game_choice(int k)
{
    switch (k) {
    case 0:
        comm_game_choice = 1;
        return 1;
    case 1:
        comm_game_choice = 2;
        return 1;
    case 2:
        comm_game_choice = 3;
        return 1;
    case 3:
        comm_game_choice = 4;
        return 1;
    }
    return 0;
}

/*
 * Connection game with another Tamagotchi: menu choice 2 = session type 3 (round 1),
 * initiator; nfc_menu_session then runs round 2 as session type 4.
 */
int comm_game_session(void)
{
    int n;

    ir_answer_only = 0;
    n = nfc_menu_session(2);
    ir_power_down();
    if (comm_partner_slot == 30) {
        if (save.friend_list[comm_partner_slot].x000_b == 0) {
            visit_record_clear();
        } else {
            comm_result_wait_book();
            visit_record_clear();
            snd_wait_end();
            if (sys_battery_low() == 1)
                sys_power_down();
            return 1;
        }
    }
    snd_wait_end();
    if (sys_battery_low() == 1)
        sys_power_down();
    if (n > 0)
        return 1;
    return 0;
}

/*
 * Connection games menu (かくれんぼ hide-and-seek, クッキング cooking, おしろのおたから
 * castle treasure, モデル♥コーデ model coordinate) -> comm_game_session.
 */
int comm_menu_games(void)
{
    int v = 0;
    u8 done;

    comm_prepare();
    do {
        switch (ui_yes_no(D_2537FE0, D_2537FF0, D_2536DF0, 16, &v)) {
        case 1:
            switch (v) {
            case 0:
                done = comm_game_session();
                break;
            case 1:
                return 0;
            }
            break;
        case 2:
            return 0;
        case 3:
            return 1;
        }
    } while (!done);
    sys_idle_seconds = 0;
    return 1;
}

/*
 * Enable callback of the "たまごっち4U" sub-menu comm_tama4u_menu_items (called by the menu driver for
 * the entry under the cursor, last for the chosen one): sets the sub-menu kind comm_submenu_choice =
 * 0x13 ツーしんゲーム, 0x14 アイテムこうかん / きねんしゃしん, 0x15 プロポーズ; all
 * entries enabled. comm_submenu_choice is a byte of the old IR profile block (flash_write_text_record) and is
 * not sent in the NFC records; otherwise read only by item_discard_from_inventory / item_add_to_inventory
 * (comm_menu_choice == 1 && comm_submenu_choice == 0x13, see comm_menu_enable) and the IR gift code (where
 * comm_set_gift_kind sets it to 0 / 1). Not the session type.
 */
int comm_submenu_enable(int k)
{
    rtc_get_hour();
    switch (k) {
    case 0:
        comm_submenu_choice = 0x13;
        return 1;
    case 1:
        comm_submenu_choice = 0x14;
        return 1;
    case 2:
        comm_submenu_choice = 0x14;
        return 1;
    case 3:
        comm_submenu_choice = 0x15;
        return 1;
    }
    return 0;
}

/*
 * The "たまごっち4U" sub-menu (table comm_tama4u_menu_items): ツーしんゲーム (communication games,
 * comm_menu_games), アイテムこうかん (item exchange, comm_menu_item_exchange), きねんしゃしん (photo,
 * comm_menu_photo), プロポーズ (proposal, comm_menu_proposal); each entry's handler runs the
 * session. (The 100$/500$/1000$ stakes menu ir_point_gift_menu_items belongs to the unreferenced IR
 * point-gift menu ir_menu_point_gift, not to the games.)
 */
int comm_menu_tama4u(void)
{
    int idx = 0;

    ir_point_gift_active = 0;
    for (;;) {
        switch (ui_menu_run_plain(&comm_tama4u_menu_items, &idx)) {
        case 0:
            break;
        case 1:
            if (comm_tama4u_menu_items.entries[idx].fn == 0) {
                snd_error_beep();
                continue;
            }
            if (comm_tama4u_menu_items.entries[idx].fn() == 0)
                continue;
            break;
        case 2:
            return 0;
        case 3:
            return 1;
        }
        break;
    }
    sys_idle_seconds = 0;
    return 1;
}

/*
 * Point gift (100/500/1000) to another toy over IR (ir_point_gift_session); comm_submenu_choice = 0 gives
 * (amount from ir_gift_amount; refuses when the pet has not enough points), 1 receives (the
 * amount then comes from the peer; ret is passed uninitialised). Only in the tables
 * ir_point_gift_menu_items (used by ir_menu_point_gift) and D_25380B0, which nothing references: dead.
 */
int ir_point_gift(void)
{
    int ret;
    int k;

    if (comm_submenu_choice == 0) {
        k = ir_gift_amount;
        if (k == 0)
            ret = 100;
        else if (k == 1)
            ret = 500;
        else
            ret = 1000;
        if (pet_cannot_afford(ret)) {
            ui_new_screen();
            ret = 0;
            gfx_object_create(0, 0x1c, 0, 0, 0, 0x210d65c);
            func_00009A6();
            snd_play(12);
            btn_wait_press_or_timeout(3);
            test_clear_buttons();
            ir_gift_menu_done = 2;
            goto out;
        }
    }
    ir_point_gift_active = 1;
    k = ir_point_gift_session(0, ret);
    ir_power_down();
    ret = 1;
    if (comm_partner_slot == 30) {
        if (save.friend_list[comm_partner_slot].x000_b == 0) {
            visit_record_clear();
        } else {
            comm_result_wait_book();
            ret = 1;
            visit_record_clear();
            goto out;
        }
    }
    if (k <= 0)
        ret = 0;
out:
    return ret;
}

/*
 * Gift amount choice k -> ir_gift_amount.
 */
int ir_set_gift_amount(int k)
{
    switch (k) {
    case 0:
        ir_gift_amount = 0;
        return 1;
    case 1:
        ir_gift_amount = 1;
        return 1;
    case 2:
        ir_gift_amount = 2;
        return 1;
    }
    return 0;
}

/*
 * Point-gift amount menu (table ir_point_gift_menu_items: 100$/500$/1000$, each ir_point_gift);
 * ir_gift_menu_done = 2 ends it. Only referenced from the "あげる" entry of D_25380B0, which
 * itself is referenced by nothing: dead.
 */
int ir_menu_point_gift(void)
{
    int idx = 0;

    ir_point_gift_active = 0;
again:
    switch (ui_menu_run_plain(&ir_point_gift_menu_items, &idx)) {
    case 0:
        break;
    case 1:
        if (ir_point_gift_menu_items.entries[idx].fn == 0) {
            snd_error_beep();
            goto again;
        }
        ir_point_gift_menu_items.entries[idx].fn();
        if (ir_gift_menu_done == 2)
            return 1;
        goto again;
    case 2:
        return 0;
    case 3:
        ir_gift_menu_done = 2;
        return 1;
    }
    sys_idle_seconds = 0;
    return 1;
}

/*
 * Gift kind k -> comm_submenu_choice.
 */
int comm_set_gift_kind(int k)
{
    switch (k) {
    case 0:
        comm_submenu_choice = 0;
        return 1;
    case 1:
        comm_submenu_choice = 1;
        return 1;
    }
    return 0;
}

/*
 * Profile exchange / greeting: menu choice 1 = session type 2, initiator (the other toy
 * idle): the TAMAGO profile records 0xA0/0xA1 (character, name, birthday) are exchanged
 * and the friend is added.
 */
int comm_greeting_session(void)
{
    int n;

    ir_answer_only = 0;
    n = nfc_menu_session(1);
    ir_power_down();
    if (comm_partner_slot == 30) {
        if (save.friend_list[comm_partner_slot].x000_b == 0) {
            visit_record_clear();
        } else {
            comm_result_wait_book();
            visit_record_clear();
            snd_wait_end();
            if (sys_battery_low() == 1)
                sys_power_down();
            return 1;
        }
    }
    snd_wait_end();
    if (sys_battery_low() == 1)
        sys_power_down();
    if (n > 0)
        return 1;
    return 0;
}

/*
 * "あいさツーしん" (greeting) entry of the main menu: yes/no confirmation "プロフこうかん"
 * (profile exchange, D_25380C8) -> comm_greeting_session = menu choice 1 = session type 2
 * (profile cards + compatibility fortune, F66).
 */
int comm_menu_greeting(void)
{
    int v = 0;
    u8 done;

    comm_prepare();
    do {
        switch (ui_yes_no(D_25380C8, D_2536DE8, D_2536DF0, 16, &v)) {
        case 1:
            switch (v) {
            case 0:
                done = comm_greeting_session();
                break;
            case 1:
                return 0;
            }
            break;
        case 2:
            return 0;
        case 3:
            return 1;
        }
    } while (!done);
    sys_idle_seconds = 0;
    return 1;
}

const u8 D_2537EF8[] = _("れいぞうこ");
const u8 D_2537F04[] = _("ようふく");
const u8 D_2537F10[] = _("アクセサリー");
const u8 D_2537F20[] = _("おどうぐ");
const struct MenuTable comm_give_inventory_menu_items = {
    4, item_list_has_owned, {
        { D_2537EF8, 0 },
        { D_2537F04, 0 },
        { D_2537F10, 0 },
        { D_2537F20, 0 },
    }
};
const u8 D_2537F54[] = _("もちもの");
const u8 D_2537F60[] = _("おみせ");
const u8 D_2537F68[] = _("あげる");
const u8 D_2537F70[] = _("かくれんぼ");
const u8 D_2537F7C[] = _("クッキング");
const u8 D_2537F88[] = _("おしろのおたから");
const u8 D_2537F9C[] = _("たまスクでさがせ");
const struct MenuTable D_2537FB0 = {
    4, comm_set_game_choice, {
        { D_2537F70, (u8 (*)(void))comm_menu_enable_zero },
        { D_2537F7C, (u8 (*)(void))comm_menu_enable_zero },
        { D_2537F88, (u8 (*)(void))comm_menu_enable_zero },
        { D_2537F9C, (u8 (*)(void))comm_menu_enable_zero },
        { 0, 0 },
    }
};
const u8 D_2537FE0[] = _("モデル♥コーデ");
const u8 D_2537FF0[] = _("あそぶ");
const u8 D_2537FF8[] = _("ツーしんゲーム");
const u8 D_2538008[] = _("アイテムこうかん");
const u8 D_253801C[] = _("きねんしゃしん");
const struct MenuTable comm_tama4u_menu_items = {
    4, comm_submenu_enable, {
        { D_2537FF8, (u8 (*)(void))comm_menu_games },
        { D_2538008, (u8 (*)(void))comm_menu_item_exchange },
        { D_253801C, (u8 (*)(void))comm_menu_photo },
        { D_2536E1C, (u8 (*)(void))comm_menu_proposal },
        { 0, 0 },
    }
};
const u8 D_253805C[] = _("100$");
const u8 D_2538068[] = _("500$");
const u8 D_2538074[] = _("1000$");
const struct MenuTable ir_point_gift_menu_items = {
    3, ir_set_gift_amount, {
        { D_253805C, (u8 (*)(void))ir_point_gift },
        { D_2538068, (u8 (*)(void))ir_point_gift },
        { D_2538074, (u8 (*)(void))ir_point_gift },
        { 0, 0 },
    }
};
const u8 D_25380A8[] = _("もらう");
/* The IR point-gift menu: あげる (give) -> amount menu ir_menu_point_gift, もらう (receive) ->
 * ir_point_gift; comm_set_gift_kind sets comm_submenu_choice = 0 / 1. Nothing references this table. */
const struct MenuTable D_25380B0 = {
    2, comm_set_gift_kind, {
        { D_2537F68, (u8 (*)(void))ir_menu_point_gift },
        { D_25380A8, (u8 (*)(void))ir_point_gift },
    }
};
const u8 D_25380C8[] = _("プロフこうかん");

/*
 * Enable callback of the communication main menu (comm_main_menu_items / comm_main_menu_debug_items; called by the
 * menu driver for the entry under the cursor, last for the chosen one): sets the main-menu
 * kind comm_menu_choice = 1 あいさツーしん, 2 たまごっち4U, 3 4Uアプリ, 4 タッチスポット, 7 for
 * the debug entries 4/5 (デバックセット / アイテムSEND), which are enabled (1) only when
 * P06 reads low and hidden (2) otherwise. comm_menu_choice is a byte of the old IR profile block
 * (flash_write_text_record) and is not sent in the NFC records; otherwise it is read only by
 * ir_locate_partner_pics (== 2, IR portrait filing) and item_discard_from_inventory / item_add_to_inventory (an extra
 * comm_show_item_get when == 1 with comm_submenu_choice == 0x13, a pairing the 4U+ menus never set
 * together: IR-era logic) and the uncalled comm_peer_allows_game. Not the session type
 * nfc_session.x003.
 */
int comm_menu_enable(int k)
{
    int x;

    switch (k) {
    case 0:
        comm_menu_choice = 1;
        return 1;
    case 1:
        comm_menu_choice = 2;
        return 1;
    case 2:
        comm_menu_choice = 3;
        return 1;
    case 3:
        comm_menu_choice = 4;
        return 1;
    case 4:
    case 5:
        comm_menu_choice = 7;
        x = P0_P0D & 0x40;
        if (x == 0)
            return 1;
        return 2;
    }
    return 0;
}

/*
 * THE COMMUNICATION (ツーしん) MAIN MENU (reached from the room with the NFC icon). Table
 * layout: {count, enable callback comm_menu_enable, then {label, handler} pairs, 0}:
 * あいさツーしん (greeting = profile exchange) -> comm_menu_greeting (menu choice 1, session
 * type 2), たまごっち4U (with another 4U: games, items, photo, proposal) -> comm_menu_tama4u
 * (sub-menu), 4Uアプリ (phone app) -> comm_menu_app (menu choice 6, session type 8 target
 * = SNEP download), タッチスポット (Touch Spots and touch cards, NFC reader mode, no
 * nfc_menu_session session) -> spot_touch_menu_entry (F51) and a hidden "\n" entry beyond the count ->
 * ir_menu_hidden_session (the dead IR session; the cursor never goes past the count, the entry is
 * reached only through the P06 toggle described there). The variant comm_main_menu_debug_items, chosen when P06 is pulled low (debug strap /
 * test unit), adds "╬デバックセット" (debug set) -> comm_menu_debug_set and "アイテムSEND" (the
 * toy sends an item in the app's role) -> comm_menu_item_send (menu choice 7, session type 8
 * initiator). After the debug table the ROM holds a 37-byte JPEG stub (SOI/JFIF/DQT/EOI:
 * the "fake .jpg" wrapper the app uses for TAMAGO files) and a legacy sample record
 * "DL_MDP_ハンバーガー.jpg" (type 1 food with big-endian size and the sender signature
 * constant). comm_menu_enable(k) sets the main-menu kind comm_menu_choice = 1..4 / 7 (a legacy IR
 * byte, not the session type) before the handlers run. The exchange kind comm_exchange_kind is
 * reset to 0 here; returns 0 when cancelled, 1 when a session ran, followed by the
 * anniversary check comm_notebook_unlock_check (animations at the 5th / 10th count of pet.comm_count).
 */
int comm_main_menu(void)
{
    int idx;
    int m;
    u8 x;
    u8 c;

    comm_menu_choice = 0;
    comm_prepare();
    idx = 0;
    item_no_full_dialog = 0;
again:
    comm_exchange_kind = 0;
    comm_remember_count();
    nfc_cancel_request = 0;
    ir_gift_menu_done = 0;
    x = P0_P0D & 0x40;
    m = 1 - (x != 0);
    visit_record_clear();
    switch (ui_menu_run_plain(m == 1 ? &comm_main_menu_debug_items : &comm_main_menu_items, &idx)) {
    case 0:
        break;
    case 1:
        if ((m == 1 ? comm_main_menu_debug_items.entries[idx].fn : comm_main_menu_items.entries[idx].fn) == 0) {
            snd_error_beep();
            goto again;
        }
        c = (m == 1 ? comm_main_menu_debug_items.entries[idx].fn : comm_main_menu_items.entries[idx].fn)();
        visit_record_clear();
        if (c == 0)
            goto again;
        break;
    case 2:
        comm_exchange_kind = 0;
        comm_menu_choice = 0;
        sys_idle_seconds = 0;
        sys_display_state = 0;
        return 0;
    }
    comm_notebook_unlock_check();
    comm_exchange_kind = 0;
    comm_menu_choice = 0;
    sys_idle_seconds = 0;
    sys_display_state = 0;
    return 1;
}

/*
 * Put the pet's name (character text 0x6C) into a record dst (+4 = 1, +6 = text).
 */
void comm_put_pet_name(u8 *dst)
{
    u16 id = save.pet.x000;
    int n;

    n = strlen((char *)chara_text(id, 0x6c, 0));
    dst[4] = 1;
    memcpy(dst + 6, chara_text(id, 0x6c, 0), n);
}

/*
 * Index of the lowest set bit of m.
 */
u8 util_lowest_set_bit(u8 m)
{
    u8 bit = 1;
    u8 r;
    int i;

    for (i = 0; i <= 7; i++) {
        r = i;
        if (m & bit)
            break;
        bit <<= 1;
    }
    return r;
}

/*
 * Busy-wait n x 100 ms on T16 ch.5, then restart its standard periodic tick
 * (tmr_t5_standard_tick).
 */
void tmr_t5_wait_100ms(u16 n)
{
    int i;

    for (i = 0; i < n; i++) {
        tmr_t5_start_timeout(100);
        while ((tmr_flags & 0x20) == 0)
            ;
    }
    tmr_t5_standard_tick();
}

/*
 * Clear the three button press counters.
 */
void btn_clear(void)
{
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
}

/*
 * Wait for the key-click sound to end and disable interrupts (before flash writes).
 */
void flash_begin(void)
{
    snd_wait_end();
    sys_irq_disable();
}

/*
 * Re-enable interrupts (after flash writes; pairs with flash_begin).
 */
void flash_end(void)
{
    sys_irq_enable();
}

/*
 * Byte-swap a 32-bit word (big endian to native: the big-endian fields of the TAMAGO
 * records).
 */
u32 util_bswap32(u32 *p)
{
    u32 x = *p;

    x = ((x & 0xff00ff) << 8) | ((x & 0xff00ff00) >> 8);
    return (x << 16) | (x >> 16);
}

/*
 * Byte-swap a 16-bit value read at a (big-endian fields of the TAMAGO records).
 */
u16 util_read_be16(int a)
{
    u16 v = *(u16 *)a;

    return (v & 0xff) << 8 | v >> 8;
}

/*
 * Clear game-variable picture slot a (anim_pic_slots[0][a] = the placeholder picture D_2258DF4,
 * a "?" image).
 */
void anim_clear_pic_slot(u8 a)
{
    anim_pic_slots[0][a] = D_2258DF4;
}

/*
 * Clear the 15 game-variable picture slots.
 */
void anim_clear_pic_slots(void)
{
    int i;

    chara_index_by_id(9999);
    for (i = 0; i <= 14; i++)
        anim_clear_pic_slot(i);
}

/*
 * Clear the first n (max 15) game-variable picture slots.
 */
void anim_clear_pic_slots_n(int n)
{
    int i;

    if (n > 15)
        n = 15;
    chara_index_by_id(9999);
    for (i = 0; i < n; i++)
        anim_clear_pic_slot(i);
}

/*
 * Clear the 15 game-variable picture slots.
 */
void anim_clear_pic_slots2(void)
{
    int i;

    for (i = 0; i <= 14; i++)
        anim_clear_pic_slot(i);
}

/*
 * Load the visiting partner's image set into the game variables anim_pic_slots (15 slots of
 * image pointers): when a partner record is present (visit mode visit_partner.mode == 2)
 * copy its images from the save bank (0x2780014 / 0x2782814 via item_load_pictures), else
 * placeholders (anim_clear_pic_slots_n).
 */
u8 visit_load_partner_pics(u8 k)
{
    if (visit_partner.mode == 2) {
        if (k)
            item_load_pictures((void *)anim_pic_slots[2], (void *)(FLASH_DL_CHARA + 0x14), visit_partner.mode);
        item_load_pictures((void *)anim_pic_slots, (void *)(FLASH_DL_CHARA + 0x2814), 7);
        return 1;
    }
    if (k)
        anim_clear_pic_slots_n(8);
    else
        anim_clear_pic_slots_n(7);
    return 0;
}

/*
 * Trait table helper (asm).
 */
int item_list_blobs(u32 *out, u8 *p, int max)
{
    int n, i, len;

    n = p[0];
    n = (n << 8) | p[1];
    p += 2;
    if (n > max)
        n = max;
    for (i = 0; i < n; i++) {
        len = (p[0] << 8) | p[1];
        p += 2;
        out[i] = (u32)p;
        p += len;
    }
    for (; i < max; i++)
        out[i] = 0;
    return n;
}

/*
 * Advance the random generator a random number of times.
 */
void util_rand_advance(void)
{
    u16 m;
    int n;
    int i;

    m = util_rand_except(100, 100) % 5u;
    n = m + 1;
    for (i = 0; i < n; i++)
        n = util_rand_except(100, 100);
}

/*
 * Is a visit/outing mode active (visit_partner.mode)?
 */
int visit_active(void)
{
    if (visit_partner.mode)
        return 1;
    return 0;
}

/*
 * Returns 0 (stub).
 */
u8 sys_stub_zero(void)
{
    return 0;
}

/*
 * The staying partner's traits (only with a partner present, visit_partner.mode == 2): add
 * an item's five trait increments at arg (the same vector pet_add_traits applies to our
 * pet) to visit_partner[5..9] (capped by [0xB..0xF], loaded from its record at the visit
 * start), then pick its dominant trait visit_partner.dominant_trait among the maximal ones
 * at random (unless the current one already is), the same ranking as for our pet.
 */
void visit_partner_add_traits(int arg)
{
    u8 a[5][2];
    u8 b[5];
    u8 *p;
    int i;
    u8 n;
    u8 t0, t1, m;

    if (visit_partner.mode != 2)
        return;
    p = (u8 *)arg;
    for (i = 0; i <= 4; i++, p++) {
        ((u8 *)&visit_partner)[i + 5] += *p;
        if (((u8 *)&visit_partner)[i + 5] > ((u8 *)&visit_partner)[i + 0xb])
            ((u8 *)&visit_partner)[i + 5] = ((u8 *)&visit_partner)[i + 0xb];
    }
    for (i = 0; i <= 4; i++) {
        a[i][0] = i;
        a[i][1] = ((u8 *)&visit_partner)[i + 5];
    }
    for (i = 0; i <= 3; i++) {
        if (a[i][1] > a[i + 1][1]) {
            t0 = a[i][0];
            t1 = a[i][1];
            a[i][0] = a[i + 1][0];
            a[i][1] = a[i + 1][1];
            a[i + 1][0] = t0;
            a[i + 1][1] = t1;
        }
    }
    n = i = 0;
    m = a[4][1];
    for (; i <= 4; i++) {
        if (a[i][1] == m) {
            b[n] = a[i][0];
            n++;
        }
    }
    if (n != 0) {
        for (i = 0; i < n; i++)
            if (b[i] == visit_partner.dominant_trait)
                return;
    }
    i = util_rand_except(100, 100) % n;
    visit_partner.dominant_trait = b[i];
    if (visit_partner.dominant_trait > 4)
        visit_partner.dominant_trait = 0;
}

/*
 * Apply 5 trait points from an item/game record (+0xB8 bytes) to the pet's trait table
 * (pet+0x118.., capped), then choose the dominant trait pet.dominant_trait (asm).
 */
void pet_add_traits(u8 *arg)
{
    u8 a[5][2];
    u8 c[5];
    u8 b[5];
    u8 *q;
    int i;
    int p, o;
    int p2, o2;
    u8 n;
    u8 t0, t1, m;

    visit_partner_add_traits((int)arg);
    q = chara_trait_caps(save.pet.x000);
    for (i = 0; i <= 4; i++)
        c[i] = *q++;
    for (i = 0; i <= 4; i++, arg++) {
        p = (int)&save.pet;
        o = 0x118;
        *(u8 *)(i + p + o + 2) += *arg;
        if (*(u8 *)(i + p + o + 2) > c[i])
            *(u8 *)(i + p + o + 2) = c[i];
    }
    for (i = 0; i <= 4; i++) {
        p2 = (int)&save.pet;
        o2 = 0x118;
        a[i][0] = i;
        a[i][1] = *(u8 *)(i + p2 + o2 + 2);
    }
    for (i = 0; i <= 3; i++) {
        if (a[i][1] > a[i + 1][1]) {
            t0 = a[i][0];
            t1 = a[i][1];
            a[i][0] = a[i + 1][0];
            a[i][1] = a[i + 1][1];
            a[i + 1][0] = t0;
            a[i + 1][1] = t1;
        }
    }
    n = i = 0;
    m = a[4][1];
    for (; i <= 4; i++) {
        if (a[i][1] == m) {
            b[n] = a[i][0];
            n++;
        }
    }
    if (n != 0) {
        for (i = 0; i < n; i++)
            if (b[i] == save.pet.dominant_trait)
                return;
    }
    i = util_rand_except(100, 100) % n;
    save.pet.dominant_trait = b[i];
    if (save.pet.dominant_trait > 4)
        save.pet.dominant_trait = 0;
}

/*
 * Apply a received partner record (bank 0x27D5400) to the pet's trait table: its bytes
 * 0xD0 (-> the dominant trait pet.dominant_trait), 0xD1..0xD5 (stats -> pet + 0x11A..),
 * 0xC0/0xC8 (the clover leaves -> pet.clover_leaves bits 0-2, with pet_add_happiness(4)),
 * 0xC2..0xC7 (-> pet.clover1_id_hi..x0cb: the ids / kinds of the liked items that filled
 * the leaves, as pet_fill_clover_leaf keeps them), 0xDE/0xDD (pet_feed /
 * pet_add_happiness), and when all four leaves are filled album_count.
 */
void comm_apply_partner_record(void)
{
    int i;
    int p, o, s;
    u8 *q;
    u8 a, b;
    u8 t;

    ir_rx_record = (struct TamagoFile *)(FLASH_COMM_RX + 0x5400);
    q = (u8 *)ir_rx_record;
    pet_feed(q[0xde]);
    save.pet.dominant_trait = q[0xd0];
    for (i = 0; i <= 4; i++) {
        p = (int)&save.pet;
        o = 0x118;
        s = 0xd0;
        *(u8 *)(i + p + o + 2) = *(u8 *)(i + (int)q + s + 1);
    }
    a = q[0xc0];
    b = q[0xc8];
    if (b != 0) {
        for (i = a; i < a + b; i++) {
            switch (i) {
            case 0:
                anim_pic_slots[4][0] = D_2196370;
                t = save.pet.clover_leaves;
                save.pet.clover_leaves = t | 1;
                pet_add_happiness(4);
                break;
            case 1:
                anim_pic_slots[4][0] = D_2196370;
                t = save.pet.clover_leaves;
                save.pet.clover_leaves = t | 2;
                pet_add_happiness(4);
                break;
            case 2:
                anim_pic_slots[4][0] = D_2196370;
                t = save.pet.clover_leaves;
                save.pet.clover_leaves = t | 4;
                pet_add_happiness(4);
                break;
            }
        }
    }
    save.pet.clover1_id_hi = q[0xc2];
    save.pet.clover1_id_lo = q[0xc4];
    save.pet.clover2_id_hi = q[0xc3];
    save.pet.clover2_id_lo = q[0xc5];
    save.pet.clover1_kind = q[0xc6];
    save.pet.clover2_kind = q[0xc7];
    if (a <= 3 && b != 0 && (save.pet.clover_leaves & 0xf) == 0xf && album_register() && save.pet.x000 <= 5000)
        album_count();
    pet_add_happiness(q[0xdd]);
}

/*
 * Dominant trait of the pet: the same ranking as visit_partner_add_traits on the character's own 5
 * stats at pet + 0x11A.
 */
u8 pet_calc_dominant_trait(void)
{
    int i;
    int p, o;
    u8 a[5][2];
    u8 b[5];
    u8 n;
    u8 t0, t1, m, x;

    for (i = 0; i <= 4; i++) {
        p = (int)&save.pet;
        o = 0x118;
        a[i][0] = i;
        a[i][1] = *(u8 *)(i + p + o + 2);
    }
    for (i = 0; i <= 3; i++) {
        if (a[i][1] > a[i + 1][1]) {
            t0 = a[i][0];
            t1 = a[i][1];
            a[i][0] = a[i + 1][0];
            a[i][1] = a[i + 1][1];
            a[i + 1][0] = t0;
            a[i + 1][1] = t1;
        }
    }
    n = i = 0;
    m = a[4][1];
    for (; i <= 4; i++) {
        if (a[i][1] == m) {
            b[n] = a[i][0];
            n++;
        }
    }
    i = util_rand_except(100, 100) % n;
    x = b[i];
    if (x > 4)
        x = 0;
    return x;
}

/*
 * Clear pet.x12D[8].
 */
void pet_clear_growth_items(void)
{
    int i;

    for (i = 0; i <= 7; i++)
        save.pet.growth_item_counts[i] = 0;
}

/*
 * Character id for preferences: downloaded characters (> 5000) map to their base id.
 */
int chara_base_id(u16 a)
{
    if (a > 5000)
        a = chara_album_key(a);
    return a;
}

/*
 * Set anim_busy = a and reset the display timeouts.
 */
void anim_set_busy(u8 a)
{
    anim_busy = a;
    sys_idle_seconds = 0;
    sys_display_state = 0;
}

/*
 * Random choice helper by kind k.
 */
u8 util_rand_choice(int k)
{
    int r = util_rand_except(100, 100);
    int v;

    switch (k) {
    case 2:
        if (r <= 49)
            v = 0;
        else
            v = 1;
        break;
    case 3:
        if (r <= 32)
            v = 0;
        else if (r <= 65)
            v = 1;
        else
            v = 2;
        break;
    case 4:
    default:
        if (r <= 24)
            v = 0;
        else if (r <= 49)
            v = 1;
        else if (r <= 74)
            v = 2;
        else
            v = 3;
        break;
    }
    return v;
}

/*
 * Program len bytes from src into flash at addr (no erase; sounds waited for, interrupts
 * off via flash_begin - note it never re-enables them itself). Returns len.
 */
int flash_program_raw(u32 addr, u16 *src, u32 len)
{
    u32 a;

    flash_begin();
    for (a = addr; a < addr + len; a += 2, src++)
        if (func_0000B52((u16 *)a, *src, FLASH_BASE) != 0)
            break;
    return len;
}

/*
 * IR communication of kind a (comm_exchange_kind; the hidden menu entries pass 3 and 4; F66's NFC
 * item exchange uses the same variable with 4 = giving side, 5 = receiving side),
 * inherited from the IR models and only called by ir_menu_hidden_session / ir_session5_unused /
 * ir_session3_unused (see there): ir_session_run runs the exchange; ir_session_flags bits returned by it
 * ask for a second run (bit 1: after a pause ir_anim_pause_a(0); bit 0: after the marriage
 * question comm_ask_marry, answer -> bits 2/3).
 */
int ir_session(u8 a)
{
    int r;

    comm_exchange_kind = a;
    ir_clear_flags();
    if (a != 0) {
        r = ir_session_run(a);
        if (ir_session_flags & 2) {
            ir_anim_pause_a(0);
            r = ir_session_run(a);
        } else if (ir_session_flags & 1) {
            switch (comm_ask_marry()) {
            case 0:
                ir_session_flags |= 4;
                r = ir_session_run(a);
                break;
            case 1:
                ir_session_flags &= ~4;
                r = ir_session_run(a);
                break;
            case 2:
            case 3:
                r = 1;
                break;
            default:
                ir_session_flags |= 8;
                r = ir_session_run(a);
                break;
            }
        }
    }
    return r;
}

/*
 * Visitor timer, called by the RTC tick rtc_isr: while a visitor/partner is present
 * (visit_partner.mode) count x016 up to 270; past it set x015 = "visit over" and return 1.
 * x016 counts seconds (a visit lasts 270 s = 4.5 minutes): the tick calls it on its
 * per-second path (checked in the emulator: once a second), and once per simulated minute
 * when the debug fast-forward sys_debug_fast_forward is set.
 */
int visit_tick(void)
{
    struct VisitPartner *p;

    if (visit_partner.mode == 0)
        return 0;
    if (visit_partner.seconds <= 269)
        visit_partner.seconds++;
    p = &visit_partner;
    if (p->seconds <= 269)
        return 0;
    p->over = 1;
    return 1;
}

const u8 D_25380F0[] = _("あいさツーしん");
const u8 D_2538100[] = _("たまごっち4U");
const u8 D_2538110[] = _("4Uアプリ");
const u8 D_253811C[] = _("タッチスポット");
const u8 D_253812C[] = _("\n");
const struct MenuTable comm_main_menu_items = {
    4, comm_menu_enable, {
        { D_25380F0, (u8 (*)(void))comm_menu_greeting },
        { D_2538100, (u8 (*)(void))comm_menu_tama4u },
        { D_2538110, (u8 (*)(void))comm_menu_app },
        { D_253811C, (u8 (*)(void))spot_touch_menu_entry },
        { D_253812C, (u8 (*)(void))ir_menu_hidden_session },
        { 0, 0 },
    }
};
const u8 D_2538168[] = _("╬デバックセット");
const u8 D_253817C[] = _("アイテムSEND");
const struct MenuTable comm_main_menu_debug_items = {
    6, comm_menu_enable, {
        { D_25380F0, (u8 (*)(void))comm_menu_greeting },
        { D_2538100, (u8 (*)(void))comm_menu_tama4u },
        { D_2538110, (u8 (*)(void))comm_menu_app },
        { D_253811C, (u8 (*)(void))spot_touch_menu_entry },
        { D_2538168, (u8 (*)(void))comm_menu_debug_set },
        { D_253817C, (u8 (*)(void))comm_menu_item_send },
        { D_253812C, (u8 (*)(void))ir_menu_hidden_session },
        { 0, 0 },
    }
};

/*
 * The RAM variables this file defines.
 */
u8 ir_point_gift_active;
u8 ir_answer_only;
u8 comm_menu_choice;
u8 comm_submenu_choice;
u8 ir_gift_amount;
u8 comm_game_choice;
volatile u8 ir_gift_menu_done;
struct FriendBookState friend_book_state;
struct MailQueue comm_mail_queue;
u8 sys_idle_seconds;
u8 comm_friendbook_updated;
u8 ir_owner_id_lo DSTRAM;
u8 ir_owner_id_hi DSTRAM;
struct GiftSource comm_gift_source DSTRAM;

