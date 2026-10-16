/*
 * The Touch Spot / card reader: reading a tag or card, sums and checks, the content browser
 * (characters and items of a card), bingo card install, Gashapon tickets, visitor messages, and
 * the reader test screens of the test mode.
 *
 * Code 2514036..2518658, .rodata from 255D344.
 */
#include "proto/spot/spot_reader.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/ui/ui_message.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/comm/comm_flash_buffers.h"
#include "proto/ir/ir_driver.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_marks.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_events.h"
#include "proto/spot/spot_bingo_menu.h"
#include "proto/owner/owner_profile.h"
#include "proto/ui/ui_yes_no.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/comm/comm_outcome.h"
#include "proto/comm/comm_checks.h"
#include "proto/nfc/nfc_session.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_reader.h"
#include "proto/lib/nfc/nfc_dep.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * Spot bingo (bingo card id x001-x299, from the F45 menu): a 3 s window to cancel with C
 * (-1), then the bingo spot reader spot_reader(0), whose result is returned (left in the
 * return register: the source has no return statement for it). The spot's number is left
 * in spot_bingo_number (-1 when none).
 */
int spot_bingo_read(void)
{
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 3;
    do {
        if (btn_a_presses != 0)
            break;
        if (btn_b_presses != 0)
            break;
        if (btn_c_presses != 0) {
            nfc_cancel_request = 0;
            btn_clear();
            return -1;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return spot_reader(0);
}

/*
 * Communication menu "タッチスポット": the Touch Spot / touch card reader in menu mode
 * (spot_reader(1): Type 2 spot tags and ISO-DEP touch cards), then the battery check.
 * Returns 0 when cancelled or failed (-1), else -1 (room_view = 0 after a success).
 */
int spot_touch_menu_entry(void)
{
    int r = spot_reader(1);

    snd_wait_end();
    if (sys_battery_low() == 1)
        sys_power_down();
    if (r != -1) {
        if (r == 0)
            room_view = r;
        return -1;
    }
    return 0;
}

/*
 * Touch Spot reader in mode 2 (spot_reader(2)): run by the set-up flow sys_startup_flow right
 * after the name entry when the P06 test strap was held at boot (sys_debug_strap_count == 8). Like the
 * menu mode 1, except that a touch card is neither registered nor counted and the content
 * browser spot_content_browser ends on a blank screen instead of the room.
 */
void spot_reader_mode2(void)
{
    spot_reader(2);
}

/*
 * Engineering read screen (test_spot_screen): the line "VER" + the big-endian 16-bit value
 * in[0..1] as 5 digits, in game characters (04 xx). in = spot_record_magic, the record's 00 80
 * magic (bytes 0..1 of a Type 2 spot record, 0xA..0xB of a touch card header).
 */
void test_spot_show_ver(u8 *in, u8 *out)
{
    int v;
    u8 i;
    int d;
    int c;

    out[0] = 4;
    out[1] = 0xf5;
    out[2] = 4;
    out[3] = 0xe4;
    out[4] = 4;
    out[5] = 0xf1;
    out += 6;
    v = in[0] << 8;
    v += in[1];
    for (i = 0, d = 10000; i <= 4; i++) {
        c = v / d;
        out[0] = 4;
        out[1] = c + 0x6d;
        out += 2;
        v = v % d;
        d = d / 10;
    }
}

/*
 * Engineering read screen: the line "INF" + the two bytes a (spot_record_info = record bytes 4, 5:
 * the kind and the text page / content type) as two 3-digit numbers.
 */
void test_spot_show_inf(struct SpotRecordInfo *a, u8 *b)
{
    u8 *s = (u8 *)a;
    u8 *d = (u8 *)b;
    u8 i;
    u8 j;
    int v;
    int div;
    int q;

    d[0] = 4;
    d[1] = 0xe8;
    d[2] = 4;
    d[3] = 0xed;
    d[4] = 4;
    d[5] = 0xe5;
    d += 6;
    for (i = 0; i <= 1; i++) {
        v = *s++;
        j = 0;
        div = 100;
        do {
            q = v / div;
            d[0] = 4;
            d[1] = q + 0x6d;
            d += 2;
            v = v % div;
            div = div / 10;
        } while (++j <= 2);
    }
}

/*
 * Engineering read screen: the line "CLS" + the two bytes src (spot_record_class = record bytes
 * 6, 7: the bingo spot id and the flags) as two 3-digit numbers.
 */
void test_spot_show_cls(const u8 *src, u8 *dst)
{
    int v, d, q;
    u8 i, j;

    dst[0] = 4;
    dst[1] = 0xe2;
    dst[2] = 4;
    dst[3] = 0xeb;
    dst[4] = 4;
    dst[5] = 0xf2;
    dst += 6;
    for (j = 0; j <= 1; j++) {
        v = *src++;
        for (i = 0, d = 100; i <= 2; i++) {
            q = v / d;
            dst[0] = 4;
            dst[1] = q + 0x6d;
            dst += 2;
            v = v % d;
            d = d / 10;
        }
    }
}

/*
 * Engineering screens of the Touch Spot / touch card reader, from the test mode
 * test_mode (test_spot_read / 25146F0 / 25146FA): shows "R", "C" or "W" (a = 3 read,
 * 4 check, else 5 = write test), waits for the B button, runs one reader pass
 * spot_reader(a) (spot_reader_result = 0 on success) and shows: R the record fields "VER" / "INF" /
 * "CLS" (bytes 0..1 or 0xA..0xB, 4..5, 6..7; test_spot_show_ver / test_spot_show_inf / test_spot_show_cls),
 * C the picture 0x2083ADC when the record's 16-bit sum is good (0x20828D0 otherwise), W
 * "TAG WRITE OK" / "TAG WRITE NG" (the use counter is written back decremented, then
 * incremented again, and read back each time). Returns 1 (B) / 0 (A+C held).
 */
u8 test_spot_screen(u8 a)
{
    u8 b1[20] = {0};
    u8 b2[20] = {0};
    u8 b3[20] = {0};
    u8 c;
    int n;

    for (;;) {
        btn_a_presses = 1;
        btn_b_presses = 0;
        btn_c_presses = 1;
        gfx_display_reset();
        gfx_object_create(0, 0x1c, 0, 0, 0, D_21091E4);
        switch (a) {
        case 3:
            gfx_draw_text_line(gfx_object_create(0, 0, 0, 0, 0, 0), 0x6f, 0x52, 0x10, (int)D_255D348, 0, 1);
            break;
        case 4:
            gfx_draw_text_line(gfx_object_create(0, 0, 0, 0, 0, 0), 0x6f, 0x52, 0x10, (int)D_255D34C, 0, 1);
            break;
        default:
            gfx_draw_text_line(gfx_object_create(0, 0, 0, 0, 0, 0), 0x6f, 0x52, 0x10, (int)D_255D350, 0, 1);
            break;
        }
        gfx_set_background(0xe7fc);
        func_00009A6();
        for (;;) {
            if (btn_b_presses != 0) {
                btn_clear();
                spot_reader_result = -1;
                spot_reader(a);
                if (spot_reader_result == 0) {
                    gfx_display_reset();
                    switch (a) {
                    case 3:
                        test_spot_show_ver((u8 *)&spot_record_magic, b1);
                        n = 10;
                        test_spot_show_inf(&spot_record_info, b2);
                        test_spot_show_cls(spot_record_class, b3);
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        c = gfx_object_create(0, 0, 0, 0, 0, 0);
                        n = 10;
                        gfx_draw_text_centered(c, 0x29, 0x10, b1, 0, n);
                        c = gfx_object_create(0, 0, 0, 0, 0, 0);
                        gfx_draw_text_centered(c, 0x39, 0x10, b2, 0, n);
                        c = gfx_object_create(0, 0, 0, 0, 0, 0);
                        gfx_draw_text_centered(c, 0x49, 0x10, b3, 0, n);
                        break;
                    case 4:
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        gfx_object_create(0, 0x1c, 0x10, 0, 0, D_2083ADC);
                        break;
                    default:
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        c = gfx_object_create(0, 0, 0, 0, 0, 0);
                        n = 10;
                        gfx_draw_text_centered(c, 0x29, 0x10, D_255D354, 0, 8);
                        c = gfx_object_create(0, 0, 0, 0, 0, 0);
                        gfx_draw_text_centered(c, 0x39, 0x10, D_255D35C, 0, 8);
                        c = gfx_object_create(0, 0, 0, 0, 0, 0);
                        gfx_draw_text_centered(c, 0x49, 0x10, D_255D368, 0, 8);
                        break;
                    }
                    gfx_set_background(0xe7fc);
                    func_00009A6();
                    snd_enabled = 1;
                    snd_play(0x19);
                    snd_wait_end();
                    snd_enabled = 0;
                    btn_a_presses = 0;
                    btn_b_presses = 0;
                    btn_c_presses = 1;
                    for (;;) {
                        if (btn_a_presses != 0) {
                            btn_a_presses = 0;
                            break;
                        }
                        if (btn_b_presses != 0) {
                            btn_b_presses = 0;
                            return 1;
                        }
                        sys_idle_tick();
                    }
                } else {
                    gfx_display_reset();
                    switch (a) {
                    case 3:
                        gfx_object_create(0, 0x1c, 0, 0, 0, 0x210b614);
                        break;
                    case 4:
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        gfx_object_create(0, 0x1c, 0x10, 0, 0, D_20828D0);
                        break;
                    default:
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        c = gfx_object_create(0, 0, 0, 0, 0, 0);
                        n = 10;
                        gfx_draw_text_centered(c, 0x29, 0x10, D_255D354, 0, 8);
                        c = gfx_object_create(0, 0, 0, 0, 0, 0);
                        gfx_draw_text_centered(c, 0x39, 0x10, D_255D35C, 0, 8);
                        c = gfx_object_create(0, 0, 0, 0, 0, 0);
                        gfx_draw_text_centered(c, 0x49, 0x10, D_255D370, 0, 8);
                        break;
                    }
                    gfx_set_background(0xe7fc);
                    func_00009A6();
                    btn_a_presses = 1;
                    btn_b_presses = 0;
                    btn_c_presses = 0;
                    for (;;) {
                        if (btn_b_presses != 0) {
                            btn_b_presses = 0;
                            break;
                        }
                        if (btn_a_presses != 0)
                            btn_a_presses = 0;
                        if (btn_c_presses != 0) {
                            btn_c_presses = 0;
                            nfc_cancel_request = 0;
                        }
                        if ((P0_P0D & 5) == 0)
                            return 0;
                        sys_idle_tick();
                    }
                }
                break;
            }
            sys_idle_tick();
        }
        sys_idle_tick();
    }
}

/*
 * Test mode: the Touch Spot reader "R" screen (read and show the record fields).
 */
u8 test_spot_read(void)
{
    return test_spot_screen(3);
}

/*
 * Test mode: the Touch Spot reader "C" screen (check the record's sum).
 */
u8 test_spot_check(void)
{
    return test_spot_screen(4);
}

/*
 * Test mode: the Touch Spot reader "W" screen (write test of the use counter).
 */
u8 test_spot_write(void)
{
    return test_spot_screen(5);
}

/*
 * "Touch a reader" step of the touch Bingo (a = 0: a bingo card with id x301-x599,
 * spot_bingo_card_kind == 0) and of the Gashapon (a = 1), called from the F45 menu. No data is
 * read: spot_wait_reader only waits for the RF field of any reader that is not a Tamagotchi
 * (a phone, a Touch Spot, any NFC reader). For Bingo first a 3 s window to cancel with C
 * (-1); then repeatedly run spot_wait_reader until it returns 0 (success: one communication
 * counted in pet.comm_count by comm_count_add; the caller then draws a random bingo number or the
 * capsule) or -1; on a timeout show the failure picture 0x210B614 ("つうしん しっぱい")
 * and wait for B (retry), C (-1) or the 10 s timeout (-2).
 */
int spot_touch_any_reader(u8 a)
{
    int r;

    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    if (a == 0) {
        ui_timeout = 3;
        do {
            if (btn_a_presses != 0)
                break;
            if (btn_b_presses != 0)
                break;
            if (btn_c_presses != 0) {
                nfc_cancel_request = 0;
                btn_clear();
                return -1;
            }
            sys_idle_tick();
        } while (ui_timeout != 0);
        btn_a_presses = 1;
        btn_b_presses = 1;
        btn_c_presses = 0;
    }
    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 0;
    for (;;) {
        r = spot_wait_reader();
        if (r == 0) {
            comm_count_add();
            return r;
        }
        if (r == -1)
            return r;
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, D_210B614);
        func_00009A6();
        btn_a_presses = 1;
        btn_b_presses = 0;
        btn_c_presses = 0;
        ui_timeout = 10;
        for (;;) {
            if (btn_b_presses != 0)
                break;
            if (btn_c_presses != 0) {
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                nfc_cancel_request = 0;
                return -1;
            }
            sys_idle_tick();
            if (ui_timeout == 0) {
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                return -2;
            }
        }
    }
}

/*
 * After an NFC reader access: re-enable the RTC interrupt (pINT_EP47_ERTC_EAD bit 2) and
 * restart the standard timer-5 tick (tmr_t5_standard_tick); the NFC code runs with the RTC tick
 * masked (comm_exclusive_timing_on).
 */
void nfc_restore_game_timing(void)
{
    INT_EP47_ERTC_EAD |= 4;
    tmr_t5_standard_tick();
}

/*
 * The Touch Spot / touch card reader (タッチスポット; notes/findings/spots.md). The toy is
 * the reader here: each pass erases flash sector 13 (0x2050000), then with the RTC masked
 * (comm_exclusive_timing_on) polls up to 30 s (nfc_poll_tag: ISO 14443 Type A, then NFC-F) and keeps
 * the first 10 bytes of the UID buffer in spot_last_uid (e0[0] = 0 when its first 8 bytes equal
 * the previous touch's: the same tag or card as last time).
 *   a = 0 bingo spot (spot_bingo_read, with a spot bingo card at 0x2750000), 1 the タッチスポット
 *   menu (spot_touch_menu_entry), 2 set-up with the P06 test strap (spot_reader_mode2), 3 / 4 / 5 the
 *   engineering R / C / W screens (test_spot_screen, one pass each).
 * Only two card types are accepted (type code x28 from nfc_activate_type_a / nfc_identify_type2_tag); all
 * others, including MIFARE Classic (0x0004 / 0x0002), the other Type 2 sizes (0x0044,
 * 0x0812, 0x0816) and FeliCa (0x0FFF, IDm in hdr, system code in x2a), fail:
 *   0x0810 = a Type 2 tag (ATQA 0x0044) whose capability container reads BuiltinItemKeyEntry 10 06 00: a
 *     48-byte MIFARE Ultralight / NTAG210 with write access. READ of bytes 0x10..0x3F
 *     (pages 4..15, the whole user area; nfc_tag_read takes byte offsets). The 0x30-byte
 *     spot record: [0..1] 00 80 (shown as "VER"), [2..3] remaining uses (big-endian,
 *     0xFFFF = unlimited), [4] kind, [5] page byte of the text (high byte of each 2-byte
 *     game character, 04), [6] bingo spot id (must equal byte 0xEE of the bingo card,
 *     spot_bingo_id_matches), [7] flags, [8..9] parameter (kind 8: points; kinds 9/10: visitor
 *     character id; bingo: [9] = the number 1..9), [0xA..0x2D] 36 text characters (low
 *     bytes), [0x2E..0x2F] 16-bit byte sum of [0..0x2D] (spot_check_sum). a = 0 accepts only
 *     kind 6 (res 0); a = 1 / 2 accept kinds 1-4 and 7-10 (res 2); kinds 0 and 5 always fail.
 *   0x0EEF = an ISO-DEP card (SAK bit 5) with the touch card applet (github.com/CiRIP/emu4u;
 *     protocol in notes/findings/cards.md). Every access re-activates the card (RATS, the
 *     ATS must hold "TNPS064", PPS to 424 kbit/s) and sends AUTHENTICATE 90 1B 00 00 08
 *     <D_255D378> 04 (card_activate; the same bytes as emu4u's dump_card.py); there is no
 *     SELECT. Then READ 90 AD / WRITE 90 8D with P2 = file 2 or 3 (card_set_params(file, key,
 *     1)), status 0x9000. File 2, 0x3C bytes: [0..1] 00 80, [2..3] ?, [4..7] remaining uses
 *     (32-bit BE, 0xFFFFFFFF = unlimited), two 0x18-byte owner slots at 8 and 0x20 (0xFF =
 *     free), 16-bit sum of [0..0x37] at [0x38..0x39], [0x3A..0x3B] ? (rewritten unchanged).
 *     File 3
 *     starts with a 0x30-byte header: [0..3] content length, [4] kind, [5] content type
 *     0x65 (characters + items) / 0x66 (a bingo card), [6..7] spot id and flags as above,
 *     [8..9] parameter, [0xA..0xB] 00 80. With a = 0 the header must be type 0x66 with kind
 *     0 or 6 (res 1, the bingo spot check as above); otherwise kind 0 or 0xFF and a length of
 *     1..0xF000: file 3 is read whole (header included) straight into flash 0x2050000
 *     (toflash = 1) and checked by the 16-bit sum in its last 2 bytes (res 3).
 * Flags [7]: bit 1 (ISO-DEP only; from the menu a = 1, or a card read at a bingo spot):
 * the player's name slot (name pet.owner_name, 0x10 bytes, + birth month pet.owner_bday_month + birth day
 * owner_bday_day[0] as two 2-digit glyph pairs 04 6D+d: 0x18 bytes) must be on the card: if it is in neither slot it is written into a free
 * one (new sum, read back; "あなたのなまえが とうろくされたよ"), and with both slots
 * taken by other toys state 4 (0x211310C) shows the two names: the card is full. Such
 * cards are not counted. Bit 0 with a limited counter: 0 uses left -> state 5
 * "ここからは もう もらえないよ!", else the decremented counter is written back (Type 2:
 * pages 4 and 15; ISO-DEP: file 2 offset 4 and the sum) and verified by reading back.
 * Bit 2, checked when the touch is not counted: the same UID as the previous touch ->
 * state 3 "れんぞくで おなじスポットに タッチできないよ!".
 * Deliveries: bingo -> spot_bingo_number = [9] (> 9 -> 1) for the board spot_bingo_screen; a wrong
 * spot id (or a bingo tag with a bad sum) shows state 2, picture 0x2111EF4
 * "このビンゴせんようスポットに タッチしてね" (touch this bingo's own spot). Type 2 kinds:
 * 1 random idle animation pet_event_idle_random, 2 visitor phrase pet_event_phrase, 3 profile phrase
 * spot_profile_talk, 4 card-matching game and prize game_score_reward(game_slot_screen()) (1-4 are the
 * events of the idle reader touch comm_idle_touch_event), 7 the 36-character message
 * (spot_show_message), 8 Gotchi points [8..9] (spot_gift_points), 9 / 10 the visitor [8..9]
 * (spot_visitor_sprite -> chara_select_other_by_set) saying the message (spot_visitor_message / spot_visitor_scene).
 * ISO-DEP content 0x65 -> spot_content_browser (store characters and TAMAGO items), 0x66 ->
 * spot_bingo_card_screen (install the bingo card in sector 0x7D). A Type 2 tag cannot carry TAMAGO
 * files: items only come from ISO-DEP cards. Every success counts one communication
 * (comm_count_add: pet.comm_count + 1) and saves (save_game). Screens: state 0 0x2110CCC
 * "タッチ!", 1 0x210B614 "つうしん しっぱい", 2-5 as above; B retries after a failure,
 * C cancels. Returns 0 ok, -1 cancelled/failed, -2 timed out.
 */
#define HDR (hdr)
#define ERR { state = 1; redraw = 1; ui_timeout = 10; res = 0xff; }
#define OK2 { state = 2; redraw = 1; ui_timeout = 10; res = 0xfe; \
    if (played == 0) { snd_play(0x4f); played = 1; } }
int spot_reader(u8 a)
{
    u8 b0c[4];
    u16 sw;
    u8 key[8];
    u8 hdr[12];
    u16 x28;
    u16 x2a;
    volatile int x2c;
    volatile int x30;
    volatile int x34;
    u8 buf38[0x30];
    u8 buf68[0x3c];
    u8 bufa4[0x3c];
    u8 e0[2];
    u8 e4[2];
    u8 e8[2];
    u8 ec[2];
    u8 f0[2];
    u8 f4[0x66];
    u8 buf15c[0x18];
    int len;
    int err;
    int redraw;
    int state;
    u32 val;
    u8 j;
    int f;
    int played;
    u8 *p;
    u8 i;
    u8 c;
    s8 r0;
    int g;
    int res;
    int crc;
    u32 v;
    int n;
    int x;

    memcpy(key, D_255D378, 8);
    x28 = 0;
    x2a = 0;
    x2c = 10;
    x30 = 0;
    x34 = 0;
    len = 0;
    v = 0;
    redraw = 1;
    state = 0;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 30;
    nfc_cancel_request = 0;
    for (;;) {
        played = 0;
        if (redraw != 0) {
            snd_wait_end();
            if (sys_battery_low() == 1)
                sys_power_down();
            if (a > 2 && state != 0)
                goto fail;
            if (a <= 2)
                ui_new_screen();
            else
                gfx_display_reset();
            if (state == 0) {
                btn_a_presses = 1;
                btn_b_presses = 1;
                btn_c_presses = 0;
                btn_mask(3);
            } else {
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                btn_unmask(7);
            }
            switch (state) {
            case 0:
                gfx_object_create(0, 0x1c, 0, 0, 0, 0x2110ccc);
                break;
            case 1:
                gfx_object_create(0, 0x1c, 0, 0, 0, 0x210b614);
                break;
            case 2:
                gfx_object_create(0, 0x1c, 0, 0, 0, 0x2111ef4);
                break;
            case 3:
                gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
                c = gfx_object_create(0, 0, 0x90, 0, 0, 0);
                gfx_draw_text_centered(c, 0x29, 0x90, D_255D380, 0, 8);
                c = gfx_object_create(0, 0, 0x92, 0, 0, 0);
                gfx_draw_text_centered(c, 0x39, 0x92, D_255D390, 0, 8);
                c = gfx_object_create(0, 0, 0x94, 0, 0, 0);
                gfx_draw_text_centered(c, 0x49, 0x94, D_255D3A4, 0, 8);
                break;
            case 4:
                gfx_object_create(0, 0x1c, 0, 0, 0, 0x211310c);
                c = gfx_object_create(0, 0, 0x92, 0, 0, 0);
                gfx_draw_text_centered(c, 0x31, 0x92, buf68 + 8, 0, 8);
                c = gfx_object_create(0, 0, 0x94, 0, 0, 0);
                gfx_draw_text_centered(c, 0x41, 0x94, buf68 + 0x20, 0, 8);
                break;
            case 5:
                gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
                c = gfx_object_create(0, 0, 0x90, 0, 0, 0);
                gfx_draw_text_centered(c, 0x30, 0x90, D_255D3BC, 0, 8);
                c = gfx_object_create(0, 0, 0x92, 0, 0, 0);
                gfx_draw_text_centered(c, 0x40, 0x92, D_255D3D0, 0, 8);
                break;
            }
            if (a > 2)
                gfx_set_background(0xe7fc);
            func_00009A6();
            redraw = 0;
        }
        if (state == 0) {
            x28 = 0;
            x2a = 0;
            x2c = 30;
            sw = 0;
            b0c[0] = 0;
            if (btn_c_presses != 0) {
                nfc_cancel_request = 0;
                if (a != 2)
                    goto fail;
                ui_new_screen();
                gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
                func_00009A6();
                goto fail;
            }
            sys_irq_disable();
            func_0000A54(0x2000000, 0xd, 0xd);
            sys_irq_enable();
            tmr_delay_ms(0x32);
            memset(buf38, 0xff, 0x30);
            memset(buf68, 0xff, 0x3c);
            if (btn_c_presses != 0) {
                nfc_cancel_request = 0;
                btn_unmask(7);
                if (a != 2)
                    goto fail;
                goto draw_fail;
            }
            comm_exclusive_timing_on();
            nfc_session.x000_b = 1;
            r0 = nfc_poll_tag((int)hdr, (int)&x28, (int)&x2a, x2c);
            nfc_session.x000_b = 0;
            nfc_restore_game_timing();
            tmr_delay_ms(0x32);
            if (r0 == 0) {
                e0[0] = memcmp(spot_last_uid, hdr, 8);
                memcpy(spot_last_uid, hdr, 10);
                if (x28 == 0x810) {
                    comm_exclusive_timing_on();
                    nfc_session.x000_b = 1;
                    x34 = 0x10;
                    len = 0x30;
                    r0 = nfc_tag_read((int)buf38, x34, len, HDR, (int)&x28, x2c, 0);
                    nfc_session.x000_b = 0;
                    nfc_restore_game_timing();
                    tmr_delay_ms(0x32);
                    if (r0 == 0) {
                        if (a == 3) {
                            spot_record_magic.hi = buf38[0];
                            spot_record_magic.lo = buf38[1];
                            spot_record_info.kind = buf38[4];
                            spot_record_info.content_type = buf38[5];
                            spot_record_class[0] = buf38[6];
                            spot_record_class[1] = buf38[7];
                            spot_reader_result = 0;
                            goto fail;
                        }
                        e4[0] = buf38[0];
                        e4[1] = buf38[1];
                        if (e4[0] == 0 && e4[1] == 0x80)
                            f = 0;
                        else
                            f = -1;
                        g = spot_check_sum(buf38, len);
                        if (a == 4) {
                            if (g == 0)
                                spot_reader_result = 0;
                            goto fail;
                        }
                        if (g == 0 && f == 0) {
                            val = (u16)(buf38[2] << 8) | buf38[3];
                            if (val == 0xffff)
                                val = -1;
                            e8[0] = buf38[4];
                            e8[1] = buf38[5];
                            ec[0] = buf38[6];
                            ec[1] = buf38[7];
                            f0[0] = buf38[8];
                            f0[1] = buf38[9];
                            for (i = 0, j = 0; i <= 0x31; i++, j += 2) {
                                f4[j] = e8[1];
                                f4[j + 1] = 0x6c;
                            }
                            f4[j] = 0;
                            f4[j + 1] = 0;
                            for (i = 0, j = 0; i <= 0x23; i++, j += 2) {
                                f4[j + 0x1a] = e8[1];
                                f4[j + 0x1b] = buf38[i + 0xa];
                            }
                            if (a == 0) {
                                if (e8[0] == 6) {
                                    x = spot_bingo_card_kind(spot_bingo_card_slot);
                                    if (x == 1) {
                                        x = spot_bingo_id_matches(ec[0]);
                                        res = 0;
                                        if (x != 0)
                                            OK2
                                    } else
                                        ERR
                                } else
                                    ERR
                            } else {
                                switch (e8[0]) {
                                case 1:
                                case 2:
                                case 3:
                                case 4:
                                case 7:
                                case 8:
                                case 9:
                                case 10:
                                    res = 2;
                                    break;
                                case 0:
                                case 5:
                                case 6:
                                default:
                                    ERR
                                }
                            }
                        } else if (f != 0)
                            ERR
                        else if (a != 0)
                            ERR
                        else
                            OK2
                    } else if (r0 == 4)
                        goto fail;
                    else
                        ERR
                } else if (x28 == 0xeef) {
                    comm_exclusive_timing_on();
                    r0 = card_set_params(2, key, 1);
                    nfc_restore_game_timing();
                    tmr_delay_ms(0x32);
                    if (r0 == 0) {
                        comm_exclusive_timing_on();
                        nfc_session.x000_b = 1;
                        x34 = 0;
                        len = 0x3c;
                        r0 = nfc_tag_read((int)buf68, x34, len, HDR, (int)&x28, x2c, 0);
                        card_last_result(&sw, b0c);
                        nfc_session.x000_b = 0;
                        nfc_restore_game_timing();
                        tmr_delay_ms(0x32);
                        if (buf68[0] == 0 && buf68[1] == 0x80)
                            g = 0;
                        else
                            g = -1;
                        f = spot_check_sum(buf68, 0x3a);
                        if (a == 4 && f != 0)
                            goto fail;
                        if (r0 == 0 && g == 0 && f == 0 && sw == 0x9000) {
                            comm_exclusive_timing_on();
                            r0 = card_set_params(3, key, 1);
                            nfc_restore_game_timing();
                            tmr_delay_ms(0x32);
                            if (r0 == 0) {
                                comm_exclusive_timing_on();
                                nfc_session.x000_b = 1;
                                x34 = 0;
                                len = 0x30;
                                r0 = nfc_tag_read((int)buf38, x34, len, HDR, (int)&x28, x2c, 0);
                                card_last_result(&sw, b0c);
                                nfc_session.x000_b = 0;
                                nfc_restore_game_timing();
                                tmr_delay_ms(0x32);
                                if (r0 == 0 && sw == 0x9000) {
                                    if (a == 3) {
                                        spot_record_magic.hi = buf38[0xa];
                                        spot_record_magic.lo = buf38[0xb];
                                        spot_record_info.kind = buf38[4];
                                        spot_record_info.content_type = buf38[5];
                                        spot_record_class[0] = buf38[6];
                                        spot_record_class[1] = buf38[7];
                                        spot_reader_result = 0;
                                        goto fail;
                                    }
                                    e4[0] = buf38[0xa];
                                    e4[1] = buf38[0xb];
                                    if (e4[0] == 0 && e4[1] == 0x80)
                                        f = 0;
                                    else
                                        f = -1;
                                    val = (s8)buf68[4] << 24;
                                    val |= buf68[5] << 16;
                                    val |= buf68[6] << 8;
                                    val |= buf68[7];
                                    e8[0] = buf38[4];
                                    e8[1] = buf38[5];
                                    ec[0] = buf38[6];
                                    ec[1] = buf38[7];
                                    f0[0] = buf38[8];
                                    f0[1] = buf38[9];
                                    if (a == 0 && f == 0 && e8[1] == 0x66) {
                                        if (e8[0] == 0 || e8[0] == 6) {
                                            x = spot_bingo_card_kind(spot_bingo_card_slot);
                                            if (x != 1)
                                                ERR
                                            else {
                                                n = spot_bingo_id_matches(ec[0]);
                                                res = x;
                                                if (n != 0)
                                                    OK2
                                            }
                                        } else
                                            ERR
                                    } else if (a != 0 && f == 0) {
                                        v = (s8)buf38[0] << 24;
                                        v |= buf38[1] << 16;
                                        v |= buf38[2] << 8;
                                        v |= buf38[3];
                                        if (v != 0xffffffff && v != 0 && v <= 0xf000) {
                                            comm_exclusive_timing_on();
                                            nfc_session.x000_b = 1;
                                            card_set_params(3, key, 1);
                                            nfc_restore_game_timing();
                                            tmr_delay_ms(0x32);
                                            comm_exclusive_timing_on();
                                            x30 = 0x2050000;
                                            x34 = 0;
                                            len = v;
                                            r0 = nfc_tag_read(x30, x34, len, HDR, (int)&x28, x2c, 1);
                                            card_last_result(&sw, b0c);
                                            nfc_session.x000_b = 0;
                                            nfc_restore_game_timing();
                                            tmr_delay_ms(0x32);
                                            if (r0 == 0 && sw == 0x9000) {
                                                n = spot_check_sum((u8 *)x30, len);
                                                if (a == 4) {
                                                    if (n == 0)
                                                        spot_reader_result = 0;
                                                    goto fail;
                                                }
                                                if (n == 0) {
                                                    if (e8[0] == 0 || e8[0] == 0xff)
                                                        res = 3;
                                                    else
                                                        ERR
                                                } else
                                                    ERR
                                            } else if (r0 == 4)
                                                goto fail;
                                            else
                                                ERR
                                        } else
                                            ERR
                                    } else if (a == 0)
                                        ERR
                                    else
                                        ERR
                                } else if (r0 == 4)
                                    goto fail;
                                else
                                    ERR
                            } else
                                ERR
                        } else if (r0 == 4)
                            goto fail;
                        else
                            ERR
                    } else
                        ERR
                } else
                    ERR
                switch (res) {
                case 0:
                case 1:
                {
                    spot_bingo_number = f0[1];
                    if (spot_bingo_number > 9)
                        spot_bingo_number = 1;
                    if ((ec[1] & 1) == 0 || (n = val) == -1) {
                        if (a == 5)
                            goto fail;
                        if (ec[1] & 4) {
                            if (e0[0] == 0) {
                                spot_bingo_number = -1;
                                state = 3;
                                goto setst;
                            } else
                                err = 0;
                        } else if (res == 1 && (ec[1] & 2)) {
                            memcpy(buf15c, save.pet.owner_name, 0x10);
                            buf15c[0x10] = 4;
                            {

                                u8 t = save.pet.owner_bday_month;

                                buf15c[0x11] = t / 10 + 0x6d;

                                buf15c[0x12] = 4;

                                buf15c[0x13] = t % 10 + 0x6d;

                            }

                            buf15c[0x14] = 4;

                            {

                                u8 t = save.pet.owner_bday_day;

                                buf15c[0x15] = t / 10 + 0x6d;

                                buf15c[0x16] = 4;

                                buf15c[0x17] = t % 10 + 0x6d;

                            }
                            e0[0] = memcmp(buf15c, buf68 + 8, 0x18);
                            e0[1] = memcmp(buf15c, buf68 + 0x20, 0x18);
                            if (e0[0] != 0 && e0[1] != 0) {
                                if (buf68[8] != 0xff && buf68[0x20] != 0xff) {
                                    buf68[0x18] = 0;
                                    buf68[0x30] = 0;
                                    spot_bingo_number = -1;
                                    state = 4;
                                setst:
                                    redraw = 1;
                                    ui_timeout = 10;
                                    err = 1;
                                    if (played == 0) {
                                        snd_play(0x4f);
                                        played = 1;
                                    }
                                } else if (buf68[8] == 0xff || buf68[0x20] == 0xff) {
                                    comm_exclusive_timing_on();
                                    nfc_session.x000_b = 1;
                                    r0 = card_set_params(2, key, 1);
                                    nfc_restore_game_timing();
                                    tmr_delay_ms(0x32);
                                    comm_exclusive_timing_on();
                                    if (r0 == 0) {
                                        if (buf68[8] == 0xff) {
                                            x34 = 8;
                                            len = 0x18;
                                            r0 = nfc_tag_write((int)buf15c, x34, len, HDR, (int)&x28, x2c);
                                            card_last_result(&sw, b0c);
                                            if (r0 == 0 && sw == 0x9000) {
                                                tmr_delay_ms(0x32);
                                                memcpy(buf68 + 8, buf15c, len);
                                                crc = spot_sum(buf68, 0x3a);
                                                buf68[0x38] = crc >> 8;
                                                buf68[0x39] = crc;
                                                x34 = 0x38;
                                                len = 4;
                                                r0 = nfc_tag_write((int)(buf68 + 0x38), x34, len, HDR, (int)&x28, x2c);
                                                card_last_result(&sw, b0c);
                                            }
                                        } else {
                                            x34 = 0x20;
                                            len = 0x18;
                                            r0 = nfc_tag_write((int)buf15c, x34, len, HDR, (int)&x28, x2c);
                                            card_last_result(&sw, b0c);
                                            if (r0 == 0 && sw == 0x9000) {
                                                tmr_delay_ms(0x32);
                                                memcpy(buf68 + 0x20, buf15c, len);
                                                crc = spot_sum(buf68, 0x3a);
                                                buf68[0x38] = crc >> 8;
                                                buf68[0x39] = crc;
                                                x34 = 0x38;
                                                len = 4;
                                                r0 = nfc_tag_write((int)(buf68 + 0x38), x34, len, HDR, (int)&x28, x2c);
                                                card_last_result(&sw, b0c);
                                            }
                                        }
                                        if (r0 == 0 && sw == 0x9000) {
                                            nfc_restore_game_timing();
                                            tmr_delay_ms(0x32);
                                            comm_exclusive_timing_on();
                                            x34 = 0;
                                            len = 0x3c;
                                            r0 = nfc_tag_read((int)bufa4, x34, len, HDR, (int)&x28, x2c, 0);
                                            card_last_result(&sw, b0c);
                                            if (r0 == 0 && sw == 0x9000 && memcmp(buf68, bufa4, len) != 0)
                                                r0 = -4;
                                        }
                                    }
                                    nfc_session.x000_b = 0;
                                    nfc_restore_game_timing();
                                    tmr_delay_ms(0x32);
                                    if (r0 == 0 && sw == 0x9000) {
                                        ui_new_screen();
                                        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
                                        c = gfx_object_create(0, 0, 0x90, 0, 0, 0);
                                        gfx_draw_text_centered(c, 0x30, 0x90, D_255D3E4, 0, 8);
                                        c = gfx_object_create(0, 0, 0x92, 0, 0, 0);
                                        gfx_draw_text_centered(c, 0x40, 0x92, D_255D3F8, 0, 8);
                                        func_00009A6();
                                        if (played == 0) {
                                            snd_play(0x4f);
                                            played = 1;
                                        }
                                        ui_timeout = 3;
                                        for (;;) {
                                            if (ui_timeout == 0)
                                                break;
                                            sys_idle_tick();
                                        }
                                        err = 0;
                                    } else {
                                        if (r0 == 4) {
                                            spot_bingo_number = -1;
                                            goto fail;
                                        }
                                        spot_bingo_number = -1;
                                        state = 1;
                                        redraw = 1;
                                        ui_timeout = 10;
                                        err = 1;
                                    }
                                }
                            } else
                                err = 0;
                        } else
                            err = 0;
                    } else {
                        if (n == 0) {
                            spot_bingo_number = -1;
                            state = 5;
                            redraw = 1;
                            ui_timeout = 10;
                            err = 1;
                            if (played == 0) {
                                snd_play(0x4f);
                                played = 1;
                            }
                        } else {
                            j = (a == 5) ? 2 : 1;
                            for (i = 0; i < j; i++) {
                                if (i == 0)
                                    val--;
                                else
                                    val++;
                                comm_exclusive_timing_on();
                                nfc_session.x000_b = 1;
                                if (res == 0) {
                                    buf38[2] = val >> 8;
                                    buf38[3] = val;
                                    g = spot_sum(buf38, len);
                                    buf38[0x2e] = g >> 8;
                                    buf38[0x2f] = g;
                                    x34 = 0x10;
                                    len = 4;
                                    r0 = nfc_tag_write((int)buf38, x34, len, HDR, (int)&x28, x2c);
                                    if (r0 == 0) {
                                        tmr_delay_ms(0x32);
                                        x34 = 0x3c;
                                        r0 = nfc_tag_write((int)(buf38 + 0x2c), x34, len, HDR, (int)&x28, x2c);
                                        if (r0 == 0) {
                                            nfc_restore_game_timing();
                                            tmr_delay_ms(0x32);
                                            comm_exclusive_timing_on();
                                            x34 = 0x10;
                                            len = 0x30;
                                            r0 = nfc_tag_read((int)bufa4, x34, len, HDR, (int)&x28, x2c, 0);
                                            if (r0 == 0 && memcmp(buf38, bufa4, len) != 0) {
                                                r0 = -4;
                                                spot_bingo_number = -1;
                                            }
                                        }
                                    }
                                    sw = 0x9000;
                                } else {
                                    p = (u8 *)((int)buf68 + 4);
                                    buf68[4] = val >> 24;
                                    buf68[5] = val >> 16;
                                    buf68[6] = val >> 8;
                                    buf68[7] = val;
                                    r0 = card_set_params(2, key, 1);
                                    nfc_restore_game_timing();
                                    tmr_delay_ms(0x32);
                                    comm_exclusive_timing_on();
                                    if (r0 == 0) {
                                        x34 = 4;
                                        len = 4;
                                        r0 = nfc_tag_write((int)p, x34, len, HDR, (int)&x28, x2c);
                                        card_last_result(&sw, b0c);
                                        if (r0 == 0 && sw == 0x9000) {
                                            tmr_delay_ms(0x32);
                                            crc = spot_sum(buf68, 0x3a);
                                            buf68[0x38] = crc >> 8;
                                            buf68[0x39] = crc;
                                            x34 = 0x38;
                                            r0 = nfc_tag_write((int)(buf68 + 0x38), x34, len, HDR, (int)&x28, x2c);
                                            card_last_result(&sw, b0c);
                                            if (r0 == 0 && sw == 0x9000) {
                                                nfc_restore_game_timing();
                                                tmr_delay_ms(0x32);
                                                comm_exclusive_timing_on();
                                                x34 = 0;
                                                len = 0x3c;
                                                r0 = nfc_tag_read((int)bufa4, x34, len, HDR, (int)&x28, x2c, 0);
                                                card_last_result(&sw, b0c);
                                                if (r0 == 0 && sw == 0x9000 && memcmp(buf68, bufa4, len) != 0)
                                                    r0 = -4;
                                            }
                                        }
                                    }
                                }
                                nfc_session.x000_b = 0;
                                nfc_restore_game_timing();
                                tmr_delay_ms(0x32);
                                if (r0 == 0 && sw == 0x9000)
                                    err = 0;
                                else {
                                    if (r0 == 4) {
                                        spot_bingo_number = -1;
                                        goto fail;
                                    }
                                    spot_bingo_number = -1;
                                    state = 1;
                                    redraw = 1;
                                    ui_timeout = 10;
                                    err = 1;
                                }
                                if (err != 0)
                                    break;
                            }
                            if (a == 5) {
                                if (err == 0)
                                    spot_reader_result = 0;
                                goto fail;
                            }
                        }
                    }
                    if (err == 0) {
                        if (sys_battery_low() == 1)
                            sys_power_down();
                        if (played == 0) {
                            snd_play(0x4f);
                            snd_wait_end();
                        }
                        comm_count_add();
                        save_game();
                        goto ret0;
                    }
                    break;
                }
                case 2:
                    if ((ec[1] & 1) == 0 || val == -1) {
                        if (a == 5)
                            goto fail;
                        err = 0;
                        if ((ec[1] & 4) && e0[0] == 0) {
                            state = 3;
                            redraw = 1;
                            ui_timeout = 10;
                            err = 1;
                            if (played == 0) {
                                snd_play(0x4f);
                                played = 1;
                            }
                        }
                    } else if (val == 0) {
                        state = 5;
                        redraw = 1;
                        ui_timeout = 10;
                        err = 1;
                        if (played == 0) {
                            snd_play(0x4f);
                            played = 1;
                        }
                    } else {
                        j = (a == 5) ? 2 : 1;
                        for (i = 0; i < j; i++) {
                            if (i == 0)
                                val--;
                            else
                                val++;
                            comm_exclusive_timing_on();
                            nfc_session.x000_b = 1;
                            buf38[2] = val >> 8;
                            buf38[3] = val;
                            g = spot_sum(buf38, len);
                            buf38[0x2e] = g >> 8;
                            buf38[0x2f] = g;
                            x34 = 0x10;
                            len = 4;
                            r0 = nfc_tag_write((int)buf38, x34, len, HDR, (int)&x28, x2c);
                            if (r0 == 0) {
                                tmr_delay_ms(0x32);
                                x34 = 0x3c;
                                r0 = nfc_tag_write((int)(buf38 + 0x2c), x34, len, HDR, (int)&x28, x2c);
                                if (r0 == 0) {
                                    nfc_restore_game_timing();
                                    tmr_delay_ms(0x32);
                                    comm_exclusive_timing_on();
                                    x34 = 0x10;
                                    len = 0x30;
                                    r0 = nfc_tag_read((int)bufa4, x34, len, HDR, (int)&x28, x2c, 0);
                                    if (r0 == 0 && memcmp(buf38, bufa4, len) != 0)
                                        r0 = -4;
                                }
                            }
                            nfc_session.x000_b = 0;
                            nfc_restore_game_timing();
                            tmr_delay_ms(0x32);
                            err = r0;
                            if (r0 != 0) {
                                if (r0 == 4)
                                    goto fail;
                                state = 1;
                                redraw = 1;
                                ui_timeout = 10;
                                err = 1;
                            }
                            if (err != 0)
                                break;
                        }
                        if (a == 5) {
                            if (err == 0)
                                spot_reader_result = 0;
                            goto fail;
                        }
                    }
                    if (err == 0) {
                        btn_unmask(7);
                        if (sys_battery_low() == 1)
                            sys_power_down();
                        if (played == 0) {
                            snd_play(0x4f);
                            snd_wait_end();
                        }
                        switch (e8[0]) {
                        case 1:
                            pet_event_idle_random();
                            break;
                        case 2:
                            pet_event_phrase();
                            break;
                        case 3:
                            spot_profile_talk();
                            break;
                        case 4:
                            game_score_reward(game_slot_screen());
                            break;
                        case 7:
                            spot_show_message((int)(f4 + 0x1a));
                            break;
                        case 8:
                            spot_gift_points((f0[0] << 8) + f0[1]);
                            break;
                        case 9:
                            x = spot_visitor_sprite((f0[0] << 8) + f0[1]);
                            chara_select_other_by_set(x);
                            spot_visitor_message(f4);
                            break;
                        case 10:
                            x = spot_visitor_sprite((f0[0] << 8) + f0[1]);
                            chara_select_other_by_set(x);
                            spot_visitor_scene(f4);
                            break;
                        }
                        switch (e8[0]) {
                        case 1 ... 4:
                        case 7 ... 10:
                            snd_wait_end();
                            comm_count_add();
                            save_game();
                            break;
                        }
                        goto ret0;
                    }
                    break;
                case 3:
                    if (a == 1 && (ec[1] & 2)) {
                        memcpy(buf15c, save.pet.owner_name, 0x10);
                        buf15c[0x10] = 4;
                        {

                            u8 t = save.pet.owner_bday_month;

                            buf15c[0x11] = t / 10 + 0x6d;

                            buf15c[0x12] = 4;

                            buf15c[0x13] = t % 10 + 0x6d;

                        }

                        buf15c[0x14] = 4;

                        {

                            u8 t = save.pet.owner_bday_day;

                            buf15c[0x15] = t / 10 + 0x6d;

                            buf15c[0x16] = 4;

                            buf15c[0x17] = t % 10 + 0x6d;

                        }
                        e0[0] = memcmp(buf15c, buf68 + 8, 0x18);
                        e0[1] = memcmp(buf15c, buf68 + 0x20, 0x18);
                        if (e0[0] != 0 && e0[1] != 0) {
                            if (buf68[8] != 0xff && buf68[0x20] != 0xff) {
                                buf68[0x18] = 0;
                                buf68[0x30] = 0;
                                state = 4;
                                redraw = 1;
                                ui_timeout = 10;
                                err = 1;
                                if (played == 0) {
                                    snd_play(0x4f);
                                    played = 1;
                                }
                            } else if (buf68[8] == 0xff || buf68[0x20] == 0xff) {
                                comm_exclusive_timing_on();
                                nfc_session.x000_b = 1;
                                r0 = card_set_params(2, key, 1);
                                nfc_restore_game_timing();
                                tmr_delay_ms(0x32);
                                comm_exclusive_timing_on();
                                if (r0 == 0) {
                                    if (buf68[8] == 0xff) {
                                        x34 = 8;
                                        len = 0x18;
                                        r0 = nfc_tag_write((int)buf15c, x34, len, HDR, (int)&x28, x2c);
                                        card_last_result(&sw, b0c);
                                        if (r0 == 0 && sw == 0x9000) {
                                            tmr_delay_ms(0x32);
                                            memcpy(buf68 + 8, buf15c, len);
                                            crc = spot_sum(buf68, 0x3a);
                                            buf68[0x38] = crc >> 8;
                                            buf68[0x39] = crc;
                                            x34 = 0x38;
                                            len = 4;
                                            r0 = nfc_tag_write((int)(buf68 + 0x38), x34, len, HDR, (int)&x28, x2c);
                                            card_last_result(&sw, b0c);
                                        }
                                    } else {
                                        x34 = 0x20;
                                        len = 0x18;
                                        r0 = nfc_tag_write((int)buf15c, x34, len, HDR, (int)&x28, x2c);
                                        card_last_result(&sw, b0c);
                                        if (r0 == 0 && sw == 0x9000) {
                                            tmr_delay_ms(0x32);
                                            memcpy(buf68 + 0x20, buf15c, len);
                                            crc = spot_sum(buf68, 0x3a);
                                            buf68[0x38] = crc >> 8;
                                            buf68[0x39] = crc;
                                            x34 = 0x38;
                                            len = 4;
                                            r0 = nfc_tag_write((int)(buf68 + 0x38), x34, len, HDR, (int)&x28, x2c);
                                            card_last_result(&sw, b0c);
                                        }
                                    }
                                    if (r0 == 0 && sw == 0x9000) {
                                        nfc_restore_game_timing();
                                        tmr_delay_ms(0x32);
                                        comm_exclusive_timing_on();
                                        x34 = 0;
                                        len = 0x3c;
                                        r0 = nfc_tag_read((int)bufa4, x34, len, HDR, (int)&x28, x2c, 0);
                                        card_last_result(&sw, b0c);
                                        if (r0 == 0 && sw == 0x9000 && memcmp(buf68, bufa4, len) != 0)
                                            r0 = -4;
                                    }
                                }
                                nfc_session.x000_b = 0;
                                nfc_restore_game_timing();
                                tmr_delay_ms(0x32);
                                if (r0 == 0 && sw == 0x9000) {
                                    ui_new_screen();
                                    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
                                    c = gfx_object_create(0, 0, 0x90, 0, 0, 0);
                                    gfx_draw_text_centered(c, 0x30, 0x90, D_255D3E4, 0, 8);
                                    c = gfx_object_create(0, 0, 0x92, 0, 0, 0);
                                    gfx_draw_text_centered(c, 0x40, 0x92, D_255D3F8, 0, 8);
                                    func_00009A6();
                                    if (played == 0) {
                                        snd_play(0x4f);
                                        played = 1;
                                    }
                                    ui_timeout = 3;
                                    for (;;) {
                                        if (ui_timeout == 0)
                                            break;
                                        sys_idle_tick();
                                    }
                                    if (sys_battery_low() == 1)
                                        sys_power_down();
                                    err = 0;
                                } else {
                                    if (r0 == 4)
                                        goto fail;
                                    state = 1;
                                    redraw = 1;
                                    ui_timeout = 10;
                                    err = 1;
                                }
                            }
                        } else
                            err = 0;
                    } else if ((a == 1 || a == 5) && (ec[1] & 1)) {
                        if (a == 1 && val == 0) {
                            state = 5;
                            redraw = 1;
                            ui_timeout = 10;
                            err = 1;
                            if (played == 0) {
                                snd_play(0x4f);
                                played = 1;
                            }
                        } else {
                            j = (a == 5) ? 2 : 1;
                            for (i = 0; i < j; i++) {
                                if (i == 0) {
                                    val--;
                                } else {
                                    val++;
                                }
                                comm_exclusive_timing_on();
                                nfc_session.x000_b = 1;
                                buf68[4] = val >> 24;
                                buf68[5] = val >> 16;
                                buf68[6] = val >> 8;
                                buf68[7] = val;
                                r0 = card_set_params(2, key, 1);
                                nfc_restore_game_timing();
                                tmr_delay_ms(0x32);
                                comm_exclusive_timing_on();
                                if (r0 == 0) {
                                    x34 = 4;
                                    len = 4;
                                    r0 = nfc_tag_write((int)(buf68 + 4), x34, len, HDR, (int)&x28, x2c);
                                    card_last_result(&sw, b0c);
                                    if (sw == 0x9000) {
                                        tmr_delay_ms(0x32);
                                        crc = spot_sum(buf68, 0x3a);
                                        buf68[0x38] = crc >> 8;
                                        buf68[0x39] = crc;
                                        x34 = 0x38;
                                        r0 = nfc_tag_write((int)(buf68 + 0x38), x34, len, HDR, (int)&x28, x2c);
                                        card_last_result(&sw, b0c);
                                        if (r0 == 0 && sw == 0x9000) {
                                            nfc_restore_game_timing();
                                            tmr_delay_ms(0x32);
                                            comm_exclusive_timing_on();
                                            x34 = 0;
                                            len = 0x3c;
                                            r0 = nfc_tag_read((int)bufa4, x34, len, HDR, (int)&x28, x2c, 0);
                                            card_last_result(&sw, b0c);
                                            if (r0 == 0 && sw == 0x9000 && memcmp(buf68, bufa4, len) != 0)
                                                r0 = -4;
                                        }
                                    }
                                }
                                if (r0 == 0 && sw == 0x9000)
                                    err = 0;
                                else {
                                    if (r0 == 4)
                                        goto fail;
                                    state = 1;
                                    redraw = 1;
                                    ui_timeout = 10;
                                    err = 1;
                                }
                                nfc_session.x000_b = 0;
                                nfc_restore_game_timing();
                                tmr_delay_ms(0x32);
                                if (err != 0)
                                    break;
                            }
                            if (a == 5) {
                                if (err == 0)
                                    spot_reader_result = 0;
                                goto fail;
                            }
                        }
                    } else if (ec[1] & 4) {
                        if (e0[0] == 0) {
                            state = 3;
                            redraw = 1;
                            ui_timeout = 10;
                            err = 1;
                            if (played == 0) {
                                snd_play(0x4f);
                                played = 1;
                            }
                        } else
                            err = 0;
                    } else {
                        if (a == 5)
                            goto fail;
                        err = 0;
                    }
                    if (err == 0) {
                        switch (e8[1]) {
                        case 0x65:
                        {
                            if (sys_battery_low() == 1)
                                sys_power_down();
                            if (played == 0) {
                                snd_play(0x4f);
                                snd_wait_end();
                            }
                            spot_content_browser(a);
                            comm_count_add();
                            save_game();
                            goto ret0;
                        }
                        case 0x66:
                        {
                            if (sys_battery_low() == 1)
                                sys_power_down();
                            if (played == 0) {
                                snd_play(0x4f);
                                snd_wait_end();
                            }
                            spot_bingo_card_screen();
                            comm_count_add();
                            save_game();
                            goto ret0;
                        }
                        default:
                            state = 1;
                            redraw = 1;
                            ui_timeout = 10;
                        }
                    }

                    break;
                case 0xfe:
                    if (played == 0) {
                        snd_play(0x4f);
                        snd_wait_end();
                    }
                    break;
                }
            } else if (r0 == 4) {
                btn_unmask(7);
                if (a != 2)
                    goto fail;
            draw_fail:
                ui_new_screen();
                gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
                func_00009A6();
                goto fail;
            } else {
                state = 1;
                redraw = 1;
                ui_timeout = 10;
            }
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            btn_unmask(7);
        }
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            switch (state) {
            case 1:
            case 2:
                ui_timeout = 10;
                break;
            case 3:
            default:
                ui_timeout = 0;
            }
        }
        if (btn_b_presses != 0) {
            btn_b_presses = 0;
            switch (state) {
            case 1:
            case 2:
                break;
            case 3:
            default:
                goto ret0;
            }
            state = 0;
            redraw = 1;
            ui_timeout = 30;
        }
        if (btn_c_presses != 0) {
            btn_c_presses = 0;
            nfc_cancel_request = 0;
            if (state != 0)
                goto fail;
        }
        sys_idle_tick();
        if (ui_timeout == 0 && state != 0)
            goto timeout;
    }
fail:
    btn_unmask(7);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    nfc_cancel_request = 0;
    return -1;
ret0:
    btn_unmask(7);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 0;
timeout:
    btn_unmask(7);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return -2;
}

/*
 * Verify the 16-bit big-endian additive checksum (byte sum) stored in the last 2 bytes of
 * the n-byte tag record p: 0 ok, -1 bad. spot_sum computes it.
 */
int spot_check_sum(u8 *p, u32 n)
{
    u32 sum = 0;
    u32 i;

    for (i = 0; i < n - 2; i++)
        sum += *p++;
    if (p[0] == (u8)(sum >> 8) && p[1] == (u8)sum)
        return 0;
    return -1;
}

/*
 * 16-bit byte sum of the first n - 2 bytes.
 */
int spot_sum(u8 *p, u32 n)
{
    u32 sum = 0;
    u32 i;

    for (i = 0; i < n - 2; i++)
        sum += *p++;
    return sum;
}

/*
 * Map the visitor character id a of a Type 2 spot record (kinds 9 / 10, bytes 8..9) to its
 * sprite set (field x000 of the character table chara_table, e.g. id 10 -> 8), which
 * chara_select_other_by_set then selects as the visitor; 0x30 when unknown. Ids 0..0x14, 1001-1005,
 * 2001-2009, 3001-3007 and 9001-9052 (character ids, not item ids).
 */
int spot_visitor_sprite(u16 a)
{
    switch (a) {
    case 0x0: return 0x0;
    case 0x2: return 0x2;
    case 0x4: return 0x4;
    case 0x5: return 0x5;
    case 0xa: return 0x8;
    case 0xb: return 0x9;
    case 0x10: return 0xc;
    case 0x11: return 0xd;
    case 0x12: return 0xe;
    case 0x13: return 0xf;
    case 0x14: return 0x10;
    case 0x3e9: return 0x12;
    case 0x3ea: return 0x13;
    case 0x3eb: return 0x14;
    case 0x3ec: return 0x15;
    case 0x3ed: return 0x16;
    case 0x7d1: return 0x18;
    case 0x7d2: return 0x19;
    case 0x7d3: return 0x1a;
    case 0x7d4: return 0x1b;
    case 0x7d5: return 0x1c;
    case 0x7d6: return 0x1d;
    case 0x7d7: return 0x1e;
    case 0x7d8: return 0x1f;
    case 0x7d9: return 0x20;
    case 0xbb9: return 0x22;
    case 0xbba: return 0x23;
    case 0xbbb: return 0x24;
    case 0xbbc: return 0x25;
    case 0xbbd: return 0x26;
    case 0xbbe: return 0x27;
    case 0xbbf: return 0x28;
    case 0x2329: return 0x2c;
    case 0x232a: return 0x2d;
    case 0x232b: return 0x2e;
    case 0x232c: return 0x2f;
    case 0x232d: return 0x30;
    case 0x232e: return 0x31;
    case 0x232f: return 0x32;
    case 0x2331: return 0x33;
    case 0x2332: return 0x34;
    case 0x2333: return 0x35;
    case 0x2334: return 0x36;
    case 0x2335: return 0x37;
    case 0x2336: return 0x38;
    case 0x2337: return 0x39;
    case 0x2338: return 0x3a;
    case 0x2339: return 0x3b;
    case 0x233a: return 0x3c;
    case 0x233b: return 0x3d;
    case 0x233c: return 0x3e;
    case 0x233d: return 0x3f;
    case 0x233e: return 0x40;
    case 0x233f: return 0x41;
    case 0x2340: return 0x42;
    case 0x2341: return 0x43;
    case 0x2342: return 0x44;
    case 0x2343: return 0x45;
    case 0x2344: return 0x46;
    case 0x2345: return 0x47;
    case 0x2346: return 0x48;
    case 0x2347: return 0x49;
    case 0x2348: return 0x4a;
    case 0x2349: return 0x4b;
    case 0x234a: return 0x4c;
    case 0x234b: return 0x4d;
    case 0x234c: return 0x4e;
    case 0x234d: return 0x4f;
    case 0x234e: return 0x50;
    case 0x234f: return 0x51;
    case 0x2350: return 0x52;
    case 0x2351: return 0x53;
    case 0x2352: return 0x54;
    case 0x2353: return 0x55;
    case 0x2354: return 0x56;
    case 0x2355: return 0x57;
    case 0x2356: return 0x58;
    case 0x2357: return 0x59;
    case 0x2358: return 0x5a;
    case 0x2359: return 0x5b;
    case 0x235a: return 0x5c;
    case 0x235b: return 0x5d;
    case 0x235c: return 0x5e;
    }
    return 0x30;
}

/*
 * Classify a stored TAMAGO item p (any item record, not a Touch Spot record): -1 unless it
 * starts with "TAMAGO" (D_255D47C) and is type 0x81 / category 4 goods; sub-kind > 1 ->
 * 0; else by the last three digits of the id: x001-x599 -> 0, x601-x749 -> 1, x801-x949
 * -> 2, else 0 (the coupon classes 0x61 / 0x62 of item_special_class).
 */
int item_coupon_class(struct TamagoFile *p)
{
    char buf[10];
    u16 r;
    int hi;

    memset(buf, 0, 10);
    strcpy(buf, D_255D47C);
    if (strncmp((char *)p, buf, strlen(buf)) != 0)
        return -1;
    if (p->x04e != 0x81)
        return -1;
    if (p->x04f != 4)
        return -1;
    if (p->sub_kind > 1)
        return 0;
    hi = p->x05a;
    r = hi << 8;
    r |= p->x05b;
    if ((r >= 1001 && r <= 1599) || (r >= 2001 && r <= 2599) || (r >= 3001 && r <= 3599)
        || (r >= 4001 && r <= 4599) || (r >= 5001 && r <= 5599) || (r >= 6001 && r <= 6599)
        || (r >= 7001 && r <= 7599) || (r >= 8001 && r <= 8599) || (r >= 9001 && r <= 9599))
        r = 0;
    else if ((r >= 1601 && r <= 1749) || (r >= 2601 && r <= 2749) || (r >= 3601 && r <= 3749)
        || (r >= 4601 && r <= 4749) || (r >= 5601 && r <= 5749) || (r >= 6601 && r <= 6749)
        || (r >= 7601 && r <= 7749) || (r >= 8601 && r <= 8749) || (r >= 9601 && r <= 9749))
        r = 1;
    else if ((r >= 1801 && r <= 1949) || (r >= 2801 && r <= 2949) || (r >= 3801 && r <= 3949)
        || (r >= 4801 && r <= 4949) || (r >= 5801 && r <= 5949) || (r >= 6801 && r <= 6949)
        || (r >= 7801 && r <= 7949) || (r >= 8801 && r <= 8949) || (r >= 9801 && r <= 9949))
        r = 2;
    else
        r = 0;
    return r;
}

/*
 * Is p a Bingo card item ("TAMAGO" magic D_255D47C, type 0x81, category 8)? id x001-x299
 * -> 1: a spot bingo card, whose numbers come from its own Touch Spots (F45 ->
 * spot_bingo_read -> spot_reader(0); the spot id must equal the card's byte 0xEE);
 * x301-x599 -> 0: a touch bingo card, a random number after touching any reader, at most
 * 3 plays a day (spot_touch_any_reader(0)); else -1. The Bingo card lives in the 0x8000-byte slot at
 * 0x2750000 (it is an app-data item, not the Touch Spot tag record).
 */
int spot_bingo_card_kind(struct TamagoFile *p)
{
    char buf[12];
    u16 v;
    int hi;

    memset(buf, 0, 10);
    strcpy(buf, D_255D47C);
    if (strncmp((char *)p, buf, strlen(buf)) != 0)
        return -1;
    if (p->x04e != 0x81)
        return -1;
    if (p->x04f != 8)
        return -1;
    hi = p->x05a;
    v = hi << 8;
    v |= p->x05b;
    if ((v >= 1001 && v <= 1299) || (v >= 2001 && v <= 2299) || (v >= 3001 && v <= 3299)
        || (v >= 4001 && v <= 4299) || (v >= 5001 && v <= 5299) || (v >= 6001 && v <= 6299)
        || (v >= 7001 && v <= 7299) || (v >= 8001 && v <= 8299) || (v >= 9001 && v <= 9299))
        return 1;
    if ((v >= 1301 && v <= 1599) || (v >= 2301 && v <= 2599) || (v >= 3301 && v <= 3599)
        || (v >= 4301 && v <= 4599) || (v >= 5301 && v <= 5599) || (v >= 6301 && v <= 6599)
        || (v >= 7301 && v <= 7599) || (v >= 8301 && v <= 8599) || (v >= 9301 && v <= 9599))
        return 0;
    return -1;
}

/*
 * Bingo spot check: is k (byte 6 of the spot record, the spot's bingo id) the id stored in
 * byte 0xEE of the installed bingo card (D_27500EE: which spots belong to this card, not
 * the last spot touched)? 0 yes, -1 no (spot_reader then shows "このビンゴせんよう
 * スポットに タッチしてね").
 */
int spot_bingo_id_matches(u8 k)
{
    if (D_27500EE == k)
        return 0;
    return -1;
}

/*
 * The spot_bingo_card_kind test for the Gashapon ticket (slot 0x2760000): 0 when p is a "TAMAGO"
 * type 0x81 / category 8 item with id x601-x999, else -1.
 */
int spot_is_gashapon_ticket(struct TamagoFile *p)
{
    char buf[12];
    int v;
    int hi;

    memset(buf, 0, 10);
    strcpy(buf, D_255D47C);
    if (strncmp((char *)p, buf, strlen(buf)) != 0)
        return -1;
    if (p->x04e != 0x81)
        return -1;
    if (p->x04f != 8)
        return -1;
    hi = p->x05a;
    v = hi << 8;
    v |= p->x05b;
    if ((v >= 1601 && v <= 1999) || (v >= 2601 && v <= 2999) || (v >= 3601 && v <= 3999)
        || (v >= 4601 && v <= 4999) || (v >= 5601 && v <= 5999) || (v >= 6601 && v <= 6999)
        || (v >= 7601 && v <= 7999) || (v >= 8601 && v <= 8999) || (v >= 9601 && v <= 9999))
        return 0;
    return -1;
}

const u8 D_255D348[] = _("R");
const u8 D_255D34C[] = _("C");
const u8 D_255D350[] = _("W");
const u8 D_255D354[] = _("TAG");
const u8 D_255D35C[] = _("WRITE");
const u8 D_255D368[] = _("OK");
const u8 D_255D370[] = _("NG");
/*
 * The touch card key: the 8 data bytes of AUTHENTICATE 90 1B 00 00 08 .. 04 sent before
 * every card READ / WRITE (spot_reader copies it, card_set_params -> card_auth_key ->
 * card_activate). A fixed password, the same for every card and toy; emu4u's
 * scripts/dump_card.py sends exactly these bytes. Not an AID: the firmware never SELECTs.
 */
const u8 D_255D378[8] = { 0xae, 0x51, 0x9f, 0xb5, 0x8a, 0x78, 0x71, 0xf4 };
const u8 D_255D380[] = _("れんぞくで\0");
const u8 D_255D390[] = _("おなじスポットに");
const u8 D_255D3A4[] = _("タッチできないよ!\0");
const u8 D_255D3BC[] = _("ここからはもう\0");
const u8 D_255D3D0[] = _("もらえないよ!\0");
const u8 D_255D3E4[] = _("あなたのなまえが");
const u8 D_255D3F8[] = _("とうろくされたよ");
const char D_255D47C[] = "TAMAGO";

/*
 * Entry count a of the content block downloaded from a touch card into flash 0x2050000
 * (spot_reader, card file 3): big-endian u16 at 0x28 (a = 0: the number of characters)
 * or 0x2E..0x40 (a = 1..10: further counts); a < 0 returns the sum of all 11 non-erased
 * fields = the number of entries. 0xFFFF (erased) reads as 0. Only user: spot_content_browser
 * (m = spot_content_entry_count(0) characters first, n = spot_content_entry_count(-1) entries in all). Not the
 * character-creation counters of Fable's notes: nothing else reads 0x2050028.. (sector 13
 * users: notes/findings/cards.md).
 */
u32 spot_content_entry_count(s8 a)
{
    volatile int sum;
    volatile u8 *p;

    p = 0;
    sum = 0;
    if (a >= 0) {
        switch (a) {
        case 0: p = (volatile u8 *)0x2050028; break;
        case 1: p = (volatile u8 *)0x205002e; break;
        case 2: p = (volatile u8 *)0x2050030; break;
        case 3: p = (volatile u8 *)0x2050032; break;
        case 4: p = (volatile u8 *)0x2050034; break;
        case 5: p = (volatile u8 *)0x2050036; break;
        case 6: p = (volatile u8 *)0x2050038; break;
        case 7: p = (volatile u8 *)0x205003a; break;
        case 8: p = (volatile u8 *)0x205003c; break;
        case 9: p = (volatile u8 *)0x205003e; break;
        case 10: p = (volatile u8 *)0x2050040; break;
        }
        sum = p[0] << 8;
        sum += p[1];
    } else {
        p = (volatile u8 *)0x2050000;
        sum = 0;
        if (p[0x28] != 0xff || p[0x29] != 0xff) { sum += p[0x28] << 8; sum += p[0x29]; }
        if (p[0x2e] != 0xff || p[0x2f] != 0xff) { sum += p[0x2e] << 8; sum += p[0x2f]; }
        if (p[0x30] != 0xff || p[0x31] != 0xff) { sum += p[0x30] << 8; sum += p[0x31]; }
        if (p[0x32] != 0xff || p[0x33] != 0xff) { sum += p[0x32] << 8; sum += p[0x33]; }
        if (p[0x34] != 0xff || p[0x35] != 0xff) { sum += p[0x34] << 8; sum += p[0x35]; }
        if (p[0x36] != 0xff || p[0x37] != 0xff) { sum += p[0x36] << 8; sum += p[0x37]; }
        if (p[0x38] != 0xff || p[0x39] != 0xff) { sum += p[0x38] << 8; sum += p[0x39]; }
        if (p[0x3a] != 0xff || p[0x3b] != 0xff) { sum += p[0x3a] << 8; sum += p[0x3b]; }
        if (p[0x3c] != 0xff || p[0x3d] != 0xff) { sum += p[0x3c] << 8; sum += p[0x3d]; }
        if (p[0x3e] != 0xff || p[0x3f] != 0xff) { sum += p[0x3e] << 8; sum += p[0x3f]; }
        if (p[0x40] != 0xff || p[0x41] != 0xff) { sum += p[0x40] << 8; sum += p[0x41]; }
    }
    if (sum == 0xffff)
        return 0;
    return sum;
}

/*
 * Big-endian field k of the header of the card content block at 0x2050000 (card file 3,
 * see spot_reader): k = 0: total length (u32 at 0), 1: u16 at 0x22 = extra bytes between
 * the 0x80-byte header and the character area, 2: u32 at 0x24 = size of the character
 * area (the file list follows it), 3: u32 at 0x2A (no caller asks for it).
 */
int spot_content_header_field(u8 k)
{
    u8 *p = (u8 *)0x2050000;
    int r = 0;

    switch (k) {
    case 0:
        r += p[0] << 24;
        r += p[1] << 16;
        r += p[2] << 8;
        r += p[3];
        break;
    case 1:
        p += 0x22;
        r += p[0] << 8;
        r += p[1];
        break;
    case 2:
        p += 0x24;
        r += p[0] << 24;
        r += p[1] << 16;
        r += p[2] << 8;
        r += p[3];
        break;
    case 3:
        p += 0x2a;
        r += p[0] << 24;
        r += p[1] << 16;
        r += p[2] << 8;
        r += p[3];
        break;
    default:
        r = 0;
        break;
    }
    return r;
}

/*
 * Address of character k of the card content block: 0x2050080 + field 0x22 + 2 (after a
 * 2-byte length), +0x4800 for the second one (k != 0; at most 2 characters). Each is a
 * downloadable character file (TAMAGO type 0x94 / category 2 / 3, 0x47DE bytes in the
 * sample card files) installed with dl_chara_store (0x47FE bytes copied).
 */
int spot_content_character(u8 k)
{
    int a = spot_content_header_field(1) + 0x2050082;

    if (k)
        a += 0x4800;
    return a;
}

/*
 * Touch Spot block: picture area of entry a.
 */
int spot_content_picture(u8 a)
{
    return spot_content_character(a) + 0xf4a;
}

/*
 * Touch Spot block: name of entry a.
 */
int spot_content_name(u8 a)
{
    return spot_content_character(a) + 0x5e;
}

/*
 * Card content block: address of embedded file n of the file list at 0x2050080 + field
 * 0x22 + field 0x24 (after the characters): a sequence of big-endian u16 length + TAMAGO
 * file (items type 0x81, games / outings type 0x94 / 2, bingo and Gashapon cards 0x81 / 8).
 */
int spot_content_file(u8 n)
{
    u8 *p;
    int i;
    int len;

    p = (u8 *)(spot_content_header_field(1) + 0x2050080);
    p += spot_content_header_field(2);
    for (i = 0; i < n; i++) {
        len = ((p[0] << 8) & 0xff00) + p[1];
        p += 2;
        p += len;
    }
    p += 2;
    return (int)p;
}

/*
 * Touch Spot block: length of embedded file n.
 */
int spot_content_file_length(u8 n)
{
    u8 *p;
    int i;
    int len;

    p = (u8 *)(spot_content_header_field(1) + 0x2050080);
    p += spot_content_header_field(2);
    for (i = 0; i < n; i++) {
        len = ((p[0] << 8) & 0xff00) + p[1];
        p += 2;
        p += len;
    }
    p += 2;
    return (int)p;
}

/*
 * Embedded file a's record (+0x204).
 */
int spot_content_file_record(u8 a)
{
    return spot_content_file(a) + 0x204;
}

/*
 * Embedded file a: picture n.
 */
int spot_content_file_picture(u8 a, u8 n)
{
    u8 *p;
    u8 i;
    int len;
    int lo;

    p = (u8 *)spot_content_file(a) + 0x202;
    for (i = 0; i < n; i++) {
        len = p[0] << 8;
        lo = p[1];
        len &= 0xff00;
        len |= lo;
        p += 2;
        p += len;
    }
    p += 2;
    return (int)p;
}

/*
 * Embedded file a's name (+0x5E).
 */
int spot_content_file_name(u8 a)
{
    return spot_content_file(a) + 0x5e;
}

/*
 * Embedded file a's kind (category 0x81 sub-types).
 */
u8 spot_content_file_kind(u8 a)
{
    u8 *p = (u8 *)spot_content_file(a);
    u8 r;

    if (p[0x4e] == 0x81) {
        switch (p[0x4f]) {
        case 1:
            if (p[0x51] == 1)
                r = 2;
            else if (p[0x51] == 2)
                r = 3;
            else
                r = 0;
            break;
        case 2:
            r = 4;
            break;
        case 3:
            r = 5;
            break;
        case 4:
            r = 9;
            break;
        case 7:
            r = 6;
            break;
        case 8:
            r = 12;
            break;
        default:
            r = 0;
            break;
        }
    } else {
        r = 0;
        if (p[0x4e] == 0x94) {
            if (p[0x4f] == 2) {
                if (p[0x51] == 1)
                    r = 7;
                else if (p[0x51] == 2)
                    r = 8;
                else if (p[0x51] == 3)
                    r = 1;
                else
                    r = 0;
            }
        }
    }
    return r;
}

/*
 * Browse and take the content of a touch card block (content type 0x65) downloaded to
 * 0x2050000 by spot_reader: n = spot_content_entry_count(-1) entries, the first m = spot_content_entry_count(0)
 * being characters (0x4800-byte records, spot_content_character: picture spot_content_picture, name
 * spot_content_name; B stores one with dl_chara_name_protected / dl_chara_store unless protected,
 * anim_dl_chars_full when there is no room), then the embedded TAMAGO files of the list
 * (spot_content_file) sorted by spot_content_file_kind: food (2 / 3), accessories (4), category 3 (5),
 * backgrounds (6), games / apps (7 / 8), goods and coupons (9-11, item_coupon_class), all stored
 * with item_receive ("アイテムがいっぱいだよ すてるものをえらんでね!" when full); bingo
 * cards (12 touch bingo, 13 spot bingo) go to sector 0x7D (0x2750000, spot_install_bingo_card; a new
 * touch bingo card clears the plays-today count pet.bingo_plays_today) and Gashapon tickets (14) to 0x7E
 * (0x2760000, spot_install_gashapon_ticket). A = next entry, B = take it, C / 20 s timeout asks
 * "すべてのアイテムを ダウンロードした?" and leaves on yes; a = 2 (set-up) ends on a blank
 * screen instead of the room. Reads the block in place, so it must run before anything else
 * erases sector 13.
 */
void spot_content_browser(u8 a)
{
    u8 state;
    u8 i;
    u8 n;
    u8 m;
    u8 k;
    int v;
    int redraw;
    const u8 *s;
    u8 t;
    int x;
    struct TamagoFile *e;
    s8 c;

    state = 0;
    i = 0;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_unmask(7);
    ui_timeout = 20;
    n = spot_content_entry_count(-1);
    m = spot_content_entry_count(0);
    redraw = 1;
    for (;;) {
        if (redraw) {
            ui_new_screen();
            if (m > i) {
                gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                s = (u8 *)spot_content_picture(i);
                gfx_object_create(0x40 - (s[0] >> 1), 0x31 - (s[1] >> 1), 0x30, 0, 0, (const u8 *)s);
                s = D_219A1E0;
                t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                gfx_draw_text_centered(t, 0x44, 0x90, (u8 *)spot_content_name(i), 0, 9);
                state = 1;
            } else {
                k = spot_content_file_kind(i - m);
                switch (k) {
                case 2:
                case 3:
                case 4:
                    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                    if (k == 4)
                        s = (u8 *)spot_content_file_record(i - m) + 0x3c2;
                    else
                        s = (u8 *)spot_content_file_record(i - m);
                    gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x30, 0, 0, (const u8 *)s);
                    t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                    gfx_draw_text_centered(t, 0x41, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                    state = k;
                    break;
                case 5:
                    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                    s = D_2195C6C;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x20, 0, 0, (const u8 *)s);
                    s = (u8 *)spot_content_file_picture(i - m, 6);
                    gfx_object_create(0x40 - (s[0] >> 1), 0x38 - (s[1] >> 1), 0x30, 0, 0, (const u8 *)s);
                    t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                    gfx_draw_text_centered(t, 0x41, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                    state = k;
                    break;
                case 6:
                    gfx_object_create(0, 0x1c, 0, 0, 0, (const u8 *)spot_content_file_record(i - m));
                    gfx_object_create(0, 0x1c, 0x20, 0, 0, D_2106CA4);
                    t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                    gfx_draw_text_centered(t, 0x44, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                    s = D_2199D68;
                    state = k;
                    break;
                case 7:
                    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                    s = D_2199EB0;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x30, 0, 0, (const u8 *)s);
                    t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                    gfx_draw_text_centered(t, 0x41, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                    state = k;
                    break;
                case 8:
                    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                    s = D_219A3D8;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x30, 0, 0, (const u8 *)s);
                    t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                    gfx_draw_text_centered(t, 0x41, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                    state = k;
                    break;
                case 9:
                    e = (struct TamagoFile *)spot_content_file(i - m);
                    if (item_coupon_class(e) == 0) {

                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        s = (u8 *)spot_content_file_record(i - m);
                        gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x30, 0, 0, (const u8 *)s);
                        t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                        gfx_draw_text_centered(t, 0x41, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                        state = 9;
                    } else if (item_coupon_class(e) == 1) {
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        s = D_219A1E0;
                        gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x30, 0, 0, (const u8 *)s);
                        t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                        gfx_draw_text_centered(t, 0x41, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                        state = 10;
                    } else if (item_coupon_class(e) == 2) {
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        gfx_object_create(0, 0x1c, 0x10, 0, 0, (const u8 *)spot_content_file_record(i - m));
                        gfx_object_create(0, 0x1c, 0x20, 0, 0, D_2107ECC);
                        t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                        gfx_draw_text_centered(t, 0x49, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                        s = D_2199D68;
                        state = 11;
                    } else
                        goto def;
                    break;
                case 12:
                    e = (struct TamagoFile *)spot_content_file(i - m);
                    v = spot_bingo_card_kind(e);
                    if (v >= 0) {
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        s = (u8 *)&spot_bingo_card_pic;
                        gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x30, 0, 0, (const u8 *)s);
                        t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                        gfx_draw_text_centered(t, 0x41, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                        state = v + 12;
                    } else if (spot_is_gashapon_ticket(e) >= 0) {
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        s = D_2199EB0;
                        gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x30, 0, 0, (const u8 *)s);
                        t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                        gfx_draw_text_centered(t, 0x41, 0x90, (u8 *)spot_content_file_name(i - m), 0, 9);
                        state = 14;
                    } else
                        goto def;
                    break;
                default:
                def:
                    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                    state = 0;
                    break;
                }
            }
            if (n > 1)
                gfx_object_create(0x72, 0x58, 0x40, n > i + 1 ? 0 : 1, 0, D_21059A4);
            func_00009A6();
            redraw = 0;
        }
        if (btn_a_presses != 0) {
            redraw = 1;
            i++;
            if (i >= n)
                i = 0;
            btn_a_presses = 0;
            ui_timeout = 20;
        }
        if (btn_b_presses != 0) {
            if (state >= 1 && state <= 14) {
                snd_wait_end();
                switch (state) {
                case 4: case 5: case 6: case 7: case 8: case 9:
                    if (item_receive((struct TamagoFile *)spot_content_file(i - m)) == 1) {
                        snd_wait_end();
                        save_game();
                    }
                    break;
                case 2: case 3: case 10: case 11:
                    if (item_receive((struct TamagoFile *)spot_content_file(i - m)) == 1) {
                        snd_wait_end();
                        save_game();
                    }
                    break;
                case 1:
                    x = spot_content_character(i);
                    if (dl_chara_name_protected((u8 *)x) != 0)
                        anim_dl_chars_full();
                    else if (dl_chara_store(x) == 0) {
                        anim_receive_item();
                        pet_add_happiness(1);
                        ui_set_mark(4, 1);
                        snd_wait_end();
                        save_game();
                    }
                    break;
                case 12: case 13:
                    v = spot_install_bingo_card((u16 *)spot_content_file(i - m));
                    if (v == -2)
                        return;
                    if (v == 0 && state == 12)
                        save.pet.bingo_plays_today = 0;
                    if (v == 0) {
                        anim_receive_item();
                        pet_add_happiness(1);
                        ui_set_mark(3, 1);
                        snd_wait_end();
                        save_game();
                    }
                    break;
                case 14:
                    v = spot_install_gashapon_ticket((u16 *)spot_content_file(i - m));
                    if (v == -2)
                        return;
                    if (v == 0) {
                        anim_receive_item();
                        pet_add_happiness(1);
                        ui_set_mark(1, 1);
                        snd_wait_end();
                        save_game();
                    }
                    break;
                }
                redraw = 1;
                i++;
                if (i >= n)
                    i = 0;
                state = 0;
            }
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            btn_unmask(7);
            ui_timeout = 20;
        }
        if (btn_c_presses != 0 || ui_timeout == 0) {
            c = spot_ask_downloaded_all();
            if (c == 0 || c == 3) {
                if (a != 2) {
                    anim_happy();
                    if (a == 1) {
                        pet_add_happiness(a);
                        snd_wait_end();
                        save_game();
                    }
                } else {
                    ui_new_screen();
                    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                    func_00009A6();
                }
                i = 0;
                redraw = 0;
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                btn_unmask(7);
                nfc_cancel_request = 0;
                return;
            }
            btn_c_presses = 0;
            nfc_cancel_request = 0;
            ui_timeout = 20;
            redraw = 1;
        }
        sys_idle_tick();
    }
}

/*
 * Touch card content type 0x66, a bingo card: show embedded file 0 of the block at
 * 0x2050000 (picture 0x2199FF0 and its name); B installs it in sector 0x7D (0x2750000,
 * spot_install_bingo_card, which asks first when a card is already there), then the room animation;
 * C / 10 s timeout leaves.
 */
void spot_bingo_card_screen(void)
{
    int redraw;
    int z;
    u32 total = 1;
    int page = 1;
    u8 c;
    struct PictureSize *img;

    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_unmask(7);
    ui_timeout = 10;
    redraw = 1;
    z = 0;
    for (;;) {
        if (redraw) {
            ui_new_screen();
            gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
            img = &spot_bingo_card_pic;
            gfx_object_create(0x40 - (img->width >> 1), 0x30 - (img->height >> 1), 0x30, 0, 0, (int)img);
            c = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
            gfx_draw_text_centered(c, 0x41, 0x90, (u8 *)spot_content_file_name(0), 0, 9);
            if (total > 1)
                gfx_object_create(0x72, 0x58, 0x40, page > 1 ? 0 : 1, z, 0x21059a4);
            func_00009A6();
            redraw = 0;            z = 0;
        }
        if (btn_a_presses != 0) {
            btn_a_presses = z;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            if (spot_install_bingo_card((u16 *)spot_content_file(0)) == 0) {
                anim_receive_item();
                pet_add_happiness(1);
                ui_set_mark(3, 1);
                anim_happy();
                pet_add_happiness(1);
                snd_wait_end();
                ui_timeout = 10;
                goto quit;
            }
            goto common;
        }
        if (btn_c_presses != 0 || ui_timeout == 0) {
            nfc_cancel_request = z;
        common:
            anim_happy();
            pet_add_happiness(1);
            snd_wait_end();
        quit:
            btn_a_presses = z;
            btn_b_presses = z;
            btn_c_presses = z;
            btn_unmask(7);
            break;
        }
        sys_idle_tick();
    }
}

/*
 * Spot kind 7: show the 36-character message p of a Type 2 spot record (three lines of 12)
 * until B, C or 30 s.
 */
void spot_show_message(int p)
{
    int c;

    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_unmask(7);
    ui_timeout = 30;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    c = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_line(c, 9, 0x21, 0x10, p, 1, 0xc);
    c = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_line(c, 9, 0x2b, 0x10, p + 0x18, 1, 0xc);
    c = gfx_object_create(0, 0, 0, 0, 0, 0);
    gfx_draw_text_centered(c, 0x44, 0x10, (u8 *)(p + 0x30), 0, 8);
    func_00009A6();
    for (;;) {
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            ui_timeout = 30;
        }
        if (btn_b_presses != 0)
            break;
        if (btn_c_presses != 0 || ui_timeout == 0) {
            nfc_cancel_request = 0;
            break;
        }
        sys_idle_tick();
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_unmask(7);
}

/*
 * Spot kind 8: a gift of k Gotchi points (the amount animation anim_points_gain, then
 * pet_add_points adds them, capped at 99999).
 */
void spot_gift_points(u16 k)
{
    anim_points_gain(k);
    anim_happy();
    pet_add_points(k);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_unmask(7);
}

/*
 * Spot kind 9: the visitor chosen by chara_select_other_by_set comes in (anim_spot_visitor_arrive), says the spot's
 * message x (message box style 10) and leaves (anim_spot_visitor_leave).
 */
void spot_visitor_message(const u8 *x)
{
    anim_pic_slots[0][0] = D_2154050;
    anim_pic_slots[0][1] = D_215D918;
    anim_pic_slots[0][2] = D_21566B8;
    anim_spot_visitor_arrive();
    ui_show_message(10, 0, (u8 *)x);
    anim_spot_visitor_leave();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_unmask(7);
}

/*
 * Spot kind 10: the visitor scene of pet_event_phrase (anim_phrase_guest, message box style 12,
 * anim_phrase_guest_end) with the spot's message x.
 */
void spot_visitor_scene(const u8 *x)
{
    anim_pic_slots[0][0] = D_2155274;
    anim_pic_slots[0][1] = D_215DA7C;
    anim_phrase_guest();
    ui_show_message(12, 0, (u8 *)x);
    anim_phrase_guest_end();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_unmask(7);
}

/*
 * Is character id a built-in character?
 */
int chara_is_builtin(s16 id)
{
    int i;
    int r;

    for (i = 0; ; i++) {
        if (chara_table[i].id == id) {
            r = 1;
            break;
        }
        if (chara_table[i].id == 0xffff) {
            r = 0;
            break;
        }
    }
    return r;
}

/*
 * Two-answer question with four text lines (Touch Spot dialogs); *p = choice.
 */
u32 ui_question_four_lines(const void *a, const void *b, const void *c, const void *d, int e, int *p)
{
    u8 t;
    u8 s;

    *p &= 1;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    if (a != 0) {
        t = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_centered(t, 0x20, 0x30, a, 0, 9);
    }
    if (b != 0) {
        t = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
        gfx_draw_text_centered(t, 0x30, 0x30, b, 0, 9);
    }
    if (c != 0) {
        t = gfx_object_create(0x18, 0x20, 0x34, 0, 0, 0);
        gfx_draw_text_line(t, 0x18, e + 0x30, 0x34, (int)c, 0, 8);
    }
    if (d != 0) {
        t = gfx_object_create(0x18, 0x20, 0x34, 0, 0, 0);
        gfx_draw_text_line(t, 0x18, e + 0x40, 0x34, (int)d, 0, 8);
    }
    s = gfx_object_create(6, ((*p + 1) % 4) * 16 + e + 0x22, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_unmask(7);
    ui_timeout = 20;
    for (;;) {
        if (btn_a_presses) {
            *p += 1;
            if (*p > 1)
                *p = 0;
            gfx_object_move_dirty(s, 6, ((*p + 1) % 4) * 16 + e + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 20;
        }
        if (btn_b_presses)
            goto ok;
        if (btn_c_presses) {
            btn_c_presses = 0;
            ui_timeout = 20;
            nfc_cancel_request = 0;
            return 2;
        }
        sys_idle_tick();
        if (ui_timeout == 0) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            return 3;
        }
    }
ok:
    btn_b_presses = 0;
    ui_timeout = 20;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 1;
}

/*
 * "すべてのアイテムをダウンロードした?" (did you download everything?) yes/no.
 */
s8 spot_ask_downloaded_all(void)
{
    int x = 0;

    switch (ui_question_four_lines(D_255D544, D_255D55C, D_255D574, D_255D57C, 16, &x)) {
    case 1:
        switch (x) {
        case 0:
            return 0;
        case 1:
            return 1;
        }
        break;
    case 2:
        return 2;
    case 3:
    default:
        return 3;
    }
}

/*
 * Touch Spot info screen (texts D_255D584...).
 */
void spot_items_full_notice(void)
{
    u8 a;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    a = gfx_object_create(0x22, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x22, 0x20, 0x10, (int)D_255D584, 0, 9);
    a = gfx_object_create(0x1c, 0x30, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x1c, 0x30, 0x10, (int)D_255D590, 0, 9);
    a = gfx_object_create(0x1c, 0x40, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x1c, 0x40, 0x10, (int)D_255D5A0, 0, 9);
    a = gfx_object_create(0x1c, 0x50, 0x10, 0, 0, 0);
    gfx_draw_text_line(a, 0x1c, 0x50, 0x10, (int)D_255D5B0, 0, 9);
    func_00009A6();
    tmr_delay_ms_t5(2000);
}

/*
 * Slot already taken: "アイテムが いっぱいだよ すてるものを えらんでね!" (spot_items_full_notice),
 * then the installed bingo card (a = 0, 0x2750000) or Gashapon ticket (a = 1, 0x2760000)
 * with its name; B and the confirmation ui_yes_no_question(1, 1) -> 0 (replace it), C -> -1,
 * 10 s -> -2.
 */
int spot_replace_card_dialog(u8 a)
{
    struct PictureSize *img;
    u8 *s;
    u8 c;

    spot_items_full_notice();
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    img = a == 0 ? (struct PictureSize *)&spot_bingo_card_pic : (struct PictureSize *)D_2199EB0;
    gfx_object_create(0x40 - (img->width >> 1), 0x30 - (img->height >> 1), 0x30, 0, 0, (const u8 *)img);
    if (a == 0)
        s = D_275005E;
    else
        s = D_276005E;
    c = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
    gfx_draw_text_centered(c, 0x41, 0x90, s, 0, 9);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    btn_unmask(7);
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            if (ui_yes_no_question(1, 1) == 0) {
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                return 0;
            }
            btn_b_presses = 0;
            ui_timeout = 10;
            break;
        }
        if (btn_c_presses != 0) {
            nfc_cancel_request = 0;
            break;
        }
        sys_idle_tick();
        if (ui_timeout == 0) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            return -2;
        }
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return -1;
}

/*
 * Install the Gashapon ticket p (an embedded file of a touch card block, its length in
 * the 2 bytes before it) in sector 0x7E (0x2760000), asking first when one is there
 * (spot_replace_card_dialog(1)).
 */
int spot_install_gashapon_ticket(u16 *p)
{
    u32 n;
    int i;
    u16 *dst;
    u8 *s;
    u16 *src;
    int r;

    src = p;
    if (spot_is_gashapon_ticket(spot_gashapon_ticket_slot) >= 0) {
        r = spot_replace_card_dialog(1);
        if (r == 0)
            goto skip;
        return r;
skip:
        snd_wait_end();
    }
    sys_irq_disable();
    func_0000A54(0x2000000, 0x7e, 0x7e);
    dst = spot_gashapon_ticket_slot;
    s = (u8 *)p - 2;
    n = *s++ << 8;
    n += *s;
    n >>= 0;
    i = 0;
    if (i >= (int)(n / 2))
        goto done;
again:
    if (func_0000B52(dst, *src, 0x2000000) != 0)
        return -1;
    i++;
    dst++;
    src++;
    if (i < (int)(n / 2))
        goto again;
done:
    sys_irq_enable();
    return 0;
}

/*
 * Install the bingo card p (an embedded file of a touch card block, its length in the 2
 * bytes before it) in sector 0x7D (0x2750000), asking first when one is there
 * (spot_replace_card_dialog(0)); the board state pet.bingo_board restarts with a random layout 1..3 and no
 * marks.
 */
int spot_install_bingo_card(u16 *p)
{
    u32 n;
    int i;
    u16 *dst;
    u8 *s;
    u8 *src;
    int r;

    src = (u8 *)p;
    if (spot_bingo_card_kind(spot_bingo_card_slot) >= 0) {
        r = spot_replace_card_dialog(0);
        if (r == 0)
            goto skip;
        return r;
skip:
        snd_wait_end();
    }
    sys_irq_disable();
    func_0000A54(0x2000000, 0x7d, 0x7d);
    dst = spot_bingo_card_slot;
    s = (u8 *)p - 2;
    n = *s++ << 8;
    n += *s;
    i = 0;
    if (i >= (int)(n >> 1))
        goto done;
again:
    if (func_0000B52(dst, *(u16 *)src, 0x2000000) != 0)
        return -1;
    i++;
    dst++;
    src += 2;
    if (i < (int)(n >> 1))
        goto again;
done:
    sys_irq_enable();
    save.pet.bingo_board = (util_rand_except(3, 3) + 1) << 12;
    return 0;
}

/*
 * Wait for a reader touch (touch Bingo and Gashapon, spot_touch_any_reader). Nothing is read or
 * exchanged: with session type 0, for up to 30 s, whenever the PN512 reports an RF field
 * (nfc_field_seen) nfc_find_peer_target listens 2 s as NFC-DEP target; peer 0xF0EF (no Tamagotchi
 * ATR_REQ: a phone, a Touch Spot, any NFC reader, nfc_detect_initiator) -> sound 0x4F, return 0.
 * Another Tamagotchi (0xF040) is ignored. -1 on C or abort, -2 on timeout.
 */
int spot_wait_reader(void)
{
    int r;
    struct NfcSession *p;
    u16 key;
    s8 c;

    r = -2;
    p = &nfc_session;
    nfc_cancel_request = 0;
    comm_exclusive_timing_on();
    nfc_sleep_until_field(1);
    btn_mask(3);
    nfc_show_session_screen(3);
    p->x000_b = 1;
    p->x003 = 0;
    comm_reset_records_wrapper();
    nfc_restore_game_timing();
    ui_timeout = 30;
    nfc_field_irq = 0;
    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 0;
    nfc_sleep_until_field(0);
    do {
        if (nfc_field_seen() == 0) {
            comm_exclusive_timing_on();
            c = nfc_find_peer_target(&key);
            if (c == 0) {
                if (key == 0xf0ef) {
                    snd_play(0x4f);
                    snd_wait_end();
                    nfc_field_irq = 0;
                    r = 0;
                    goto out;
                }
            } else if (c == 4) {
                r = -1;
                goto out;
            }
        }
        if (btn_c_presses != 0) {
            r = -1;
            nfc_cancel_request = 0;
            goto out;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
out:
    nfc_session.x000_b = 0;
    nfc_session.x003 = 0;
    nfc_cancel_request = 0;
    nfc_restore_game_timing();
    btn_unmask(7);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return r;
}

const u8 D_255D544[] = _("すべてのアイテムを\0");
const u8 D_255D55C[] = _("ダウンロードした?\0");
const u8 D_255D574[] = _("はい");
const u8 D_255D57C[] = _("いいえ");
const u8 D_255D584[] = _("アイテムが");
const u8 D_255D590[] = _("いっぱいだよ");
const u8 D_255D5A0[] = _("すてるものを");
const u8 D_255D5B0[] = _("えらんでね!");

/*
 * The RAM variables this file defines.
 */
volatile u8 btn_a_presses;
volatile u8 btn_b_presses;
volatile u8 btn_c_presses;
u8 spot_last_uid[10];
s8 spot_reader_result DSTRAM;
struct SpotRecordMagic spot_record_magic DSTRAM;
struct SpotRecordInfo spot_record_info DSTRAM;
u8 spot_record_class[2] DSTRAM;
