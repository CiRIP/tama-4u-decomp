/*
 * NFC sessions: session screen, field detection and peer finding, power-up, send / receive,
 * initiator and target sessions, the menu session, the idle sessions and the NFC link tests of
 * the test mode.
 *
 * Code 25298CE..252AA4C, .rodata from 25624E4.
 */
#include "proto/nfc/nfc_session.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/room/room_main.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/comm/comm_flash_buffers.h"
#include "proto/ir/ir_receive.h"
#include "proto/ir/ir_packets.h"
#include "proto/ir/ir_driver.h"
#include "proto/ui/ui_menu.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/test/test_mode.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/comm/comm_outcome.h"
#include "proto/comm/comm_checks.h"
#include "proto/comm/comm_exchange.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_reader.h"
#include "proto/lib/nfc/nfc_dep.h"
#include "proto/lib/nfc/nfc_snep.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

const u8 D_25624E8[] = _("NFC");
const u8 D_25624F0[] = _("INITIALIZE");
const u8 D_2562508[] = _("OK");
const u8 D_2562510[] = _("NG");


/*
 * NFC application layer (F69): the "つうしん" (communication) sessions with another
 * Tamagotchi 4U or with the smartphone app, on top of the PN512 driver (252C5A6..252D3F2),
 * the NFC-DEP transfer (252DC6E/252E5D0/252ECE0) and the LLCP/SNEP link (252F292/252FE2C).
 * Session state lives in nfc_session (struct NfcSession): x000_b = busy flag, x001 = chip
 * state (1 ok, 2 failed), x002 = role (1 initiator, 2 target), x003 = session type,
 * x004 = peer kind used by the transfers, x006 = peer kind last detected, x010 = our
 * character record address, x018 = item refused flag (-1, item_add_to_inventory); nfc_result = last result code,
 * nfc_cancel_request = abort request (C button), nfc_field_irq = PN512 IRQ seen.
 *
 * Numbering used in the comments (notes/findings/sessions.md has the full table):
 * "menu choice n" = the argument of nfc_menu_session (1..7, chosen by the F7 menu handlers);
 * "session type N" = nfc_session.x003, which is also the byte 0x4C of every exchanged record
 * (comm_build_header copies it): 0 automatic (idle toy, adopts the peer's 0x4C), 1 factory link
 * test, 2 greeting, 3 / 4 communication game round 1 / round 2, 5 item gift, 6 proposal,
 * 7 commemorative photo, 8 app download link. Peer kinds: 0xF040 = another Tamagotchi
 * (its NFC-DEP ATR carries an NFCID3 starting F0 40), 0xF0F0 = the smartphone app (a
 * pseudo peer: never detected, set for session type 8 and the test-mode tester), 0xF0EF =
 * "no Tamagotchi": the default the detectors (nfc_detect_initiator / nfc_detect_target) return when no
 * F0 40 ATR came, both on timeout and after 3 foreign frames.
 *
 * NFC communication screens: a = 0/1/2 draw the full-screen "communication" pictures
 * 0x21091E4 / 0x210A3FC / 0x210B614 ("touch", "communicating", "failed"?) on a white
 * background when the peer is another Tamagotchi (nfc_session.peer_kind == 0xF040), red
 * otherwise; 3: wait for the sound to end, battery check (power down when low), show
 * 0x2110CCC; 4: show 0x210B614 then the same battery check; 5/6/7: the debug / factory
 * test text screens "NFC INITIALIZE", "... OK", "... NG".
 */
void nfc_show_session_screen(u8 a)
{
    u8 r;

    switch (a) {
    case 0:
        gfx_display_reset();
        gfx_set_background(nfc_session.peer_kind == 0xf040 ? 0xffff : 0xf800);
        func_00009A6();
        gfx_object_create(0, 0x1c, 0x10, 0, 0, D_21091E4);
        func_00009A6();
        break;
    case 1:
        gfx_display_reset();
        gfx_set_background(nfc_session.peer_kind == 0xf040 ? 0xffff : 0xf800);
        gfx_object_create(0, 0x1c, 0x10, 0, 0, D_210A3FC);
        func_00009A6();
        break;
    case 2:
        gfx_display_reset();
        gfx_set_background(nfc_session.peer_kind == 0xf040 ? 0xffff : 0xf800);
        gfx_object_create(0, 0x1c, 0x10, 0, 0, D_210B614);
        func_00009A6();
        break;
    case 3:
        snd_wait_end();
        if (sys_battery_low() == 1)
            sys_power_down();
        ui_new_screen_with_picture(0, 0x2110ccc);
        break;
    case 4:
        ui_new_screen_with_picture(0, 0x210b614);
        snd_wait_end();
        if (sys_battery_low() == 1)
            sys_power_down();
        break;
    case 5:
        gfx_display_reset();
        gfx_set_background(0xffff);
        r = gfx_object_create(0, 0, 0x10, 0, 0, 0);
        txt_draw_centered(r, 0x1c, 0x10, D_25624E8, 0, 9);
        r = gfx_object_create(0, 0, 0x20, 0, 0, 0);
        txt_draw_centered(r, 0x38, 0x20, D_25624F0, 0, 10);
        func_00009A6();
        break;
    case 6:
        gfx_display_reset();
        gfx_set_background(0xffff);
        r = gfx_object_create(0, 0, 0x10, 0, 0, 0);
        txt_draw_centered(r, 0x1c, 0x10, D_25624E8, 0, 9);
        r = gfx_object_create(0, 0, 0x20, 0, 0, 0);
        txt_draw_centered(r, 0x38, 0x20, D_25624F0, 0, 10);
        r = gfx_object_create(0, 0, 0x30, 0, 0, 0);
        txt_draw_centered(r, 0x54, 0x30, D_2562508, 0, 9);
        func_00009A6();
        break;
    case 7:
        gfx_display_reset();
        gfx_set_background(0xffff);
        r = gfx_object_create(0, 0, 0x10, 0, 0, 0);
        txt_draw_centered(r, 0x1c, 0x10, D_25624E8, 0, 9);
        r = gfx_object_create(0, 0, 0x20, 0, 0, 0);
        txt_draw_centered(r, 0x38, 0x20, D_25624F0, 0, 10);
        r = gfx_object_create(0, 0, 0x30, 0, 0, 0);
        txt_draw_centered(r, 0x54, 0x30, D_2562510, 0, 9);
        func_00009A6();
        break;
    default:
        ui_new_screen_with_picture(0, 0x2110ccc);
        break;
    }
}

/*
 * Factory NFC loopback test (no caller, never returns): power up the front-end until it
 * answers, then forever: send a 0x400-byte counting pattern as a Tamagotchi-to-Tamagotchi
 * initiator (nfc_dep_send), arm detection and wait for an RF field (nfc_sleep_until_field + IRQ
 * flag nfc_field_irq), then receive 0x400 bytes into RAM (nfc_dep_receive with toflash = 0).
 */
void test_nfc_loopback(void)
{
    volatile char state;
    u8 buf[0x400];
    int len;
    int k = 0x1e;
    int z;
    u32 i;
    u32 lim;

    state = 0;
    nfc_show_session_screen(3);
    comm_stop_ms_timers();
    do {
    } while (nfc_init((struct FlashBase *)0x2000000) != 0);
    z = 0;
    for (;;) {
        switch (state) {
        case 0:
            for (i = 0; i <= 0x3ff; i++)
                buf[i] = i;
            len = 0x400;
            if (nfc_dep_send(buf, len, k) == 0)
                state = 1;
            break;
        case 1:
            nfc_field_irq = 0;
            nfc_sleep_until_field(0);
            if (nfc_field_irq == 1) {
                nfc_sleep_until_field(1);
                state = 2;
            }
            break;
        case 2:
            i = z;
            len = z;
            lim = 0x3ff;
            do {
                buf[i] = 0xaa;
                i++;
            } while (i <= lim);
            nfc_dep_receive((int)buf, (u32 *)&len, 0, k, z);
            break;
        }
    }
}

/*
 * Clear the NFC IRQ flag nfc_field_irq. Called from the reset handler and from main.
 * Quirk kept: the original apparently meant to clear the 0x24-byte session state nfc_session
 * as well, but its code copies the struct into a dead local and then calls memset with
 * (pointer 0, value 0x24) and the count register left at 0 by that copy - a no-op, so
 * nfc_session is not cleared here. This version does only what the original did.
 */
void nfc_session_clear(void)
{
    nfc_field_irq = 0;
}

/*
 * NFC session: point comm_tx_record at our TAMAGO packet header / profile to send (0x180 bytes
 * at comm_tx_packet) and comm_rx_record at the peer's received copy (comm_rx_packet), and clear both.
 */
void comm_reset_records(void)
{
    comm_tx_record = (u32)comm_tx_packet;
    comm_rx_record = comm_rx_packet.bytes;
    memset((void *)comm_tx_record, 0, 0x180);
    memset(comm_rx_record, 0, 0x180);
}

/*
 * User abort of an NFC session (long press of the C button, from the port-2 ISR): set
 * nfc_cancel_request = 1 (cancel requested: the 100 ms tick will raise the abort flag) and raise
 * the driver abort flag now (nfc_request_abort).
 */
int nfc_user_abort(void)
{
    nfc_cancel_request = 1;
    return nfc_request_abort();
}

/*
 * NFC: clear the IRQ flag, arm the PN512 field detector and power the chip down, waiting
 * for an RF field (RfOn detect = "wait for a peer to appear").
 */
void nfc_wait_field_on(void)
{
    nfc_field_irq = 0;
    nfc_sleep_until_field(0);
}

/*
 * NFC: set the IRQ flag and put the front-end to sleep with the RfOff interrupt.
 */
void nfc_sleep_field_off_set(void)
{
    nfc_field_irq = 1;
    nfc_sleep_until_field(1);
}

/*
 * NFC: clear the IRQ flag, front-end sleep with the RfOff interrupt ("wait for the peer
 * to go away").
 */
void nfc_wait_field_off(void)
{
    nfc_field_irq = 0;
    nfc_sleep_until_field(1);
}

/*
 * NFC: if an RF field was detected (nfc_field_irq), re-arm the field detection.
 */
void nfc_rearm_if_field(void)
{
    if (nfc_field_irq != 0)
        nfc_wait_field_on();
}

/*
 * NFC: returns 0 (and re-arms with RfOff) if an RF field was detected, else 1.
 */
s8 nfc_field_seen(void)
{
    if (nfc_field_irq != 0) {
        nfc_sleep_until_field(1);
        return 0;
    }
    return 1;
}

/*
 * NFC session, target side: wait for a peer and report its kind in *out: nfc_detect_initiator
 * listens as an active-mode target for an ATR_REQ, 2 s in session type 0 (the idle toy,
 * already woken by an RF field), 30 s otherwise. 0xF040 = another Tamagotchi; 0xF0EF =
 * no Tamagotchi (nothing within the timeout, which still returns 0, or 3 frames that were
 * not a Tamagotchi ATR_REQ) - the callers decide what that means: failure in nfc_session_target,
 * the "touch" event of a phone/reader in the idle session (nfc_idle_target_session result 4).
 * Session type 8 (only reached here through menu choice 6, "4Uアプリ") and type 1 with
 * x004 == 0xF0F0 (the test-mode run against the app-side tester, test_mode) skip the
 * wait and report the pseudo peer 0xF0F0 = "the smartphone app": with x004 != 0xF040 the
 * transfer functions nfc_send / nfc_receive then use the LLCP/SNEP path (toy as
 * Type A initiator, SNEP server / PUT) instead of raw NFC-DEP. comm_user_cancelled() != 0 =
 * session cancelled by button C (returns the previous result).
 */
s8 nfc_find_peer_target(u16 *out)
{
    u8 buf[12];
    u16 v = 0;
    int n;
    struct NfcSession *p;

    if (comm_user_cancelled() != 0)
        return nfc_result;
    p = &nfc_session;
    if (p->x003 == 8 || (p->x003 == 1 && p->peer_kind == 0xf0f0)) {
        nfc_result = 0;
        p->detected_peer = 0xf0f0;
        *out = 0xf0f0;
    } else {
        if (nfc_session.x003 == 0)
            n = 2;
        else
            n = 30;
        *out = v;
        nfc_result = nfc_detect_initiator(buf, &v, n);
        nfc_session.detected_peer = v;
        *out = v;
    }
    comm_user_cancelled();
    return nfc_result;
}


/*
 * NFC session: same as nfc_find_peer_target but as initiator, through nfc_detect_target (the
 * active-mode initiator variant of the peer detection) with a 30 s timeout; peer kind in
 * *out. Used by the initiator-side exchange comm_exchange_initiator.
 */
int nfc_find_peer_initiator(u16 *out)
{
    u8 buf[12];
    u16 v = 0;
    volatile int n = 30;

    if (comm_user_cancelled() == 0) {
        *out = v;
        nfc_result = nfc_detect_target(buf, &v, n);
        nfc_session.detected_peer = v;
        *out = v;
        if (comm_user_cancelled() != 0)
            return nfc_result;
    }
    return nfc_result;
}

/*
 * NFC: make sure the front-end is powered and answering: if not done yet
 * (nfc_session.x001 != 1), up to two attempts of nfc_init; on success x001 = 1 and the
 * detector is armed waiting for an RF field (nfc_wait_field_on), on failure x001 = 2. Returns
 * the driver result (0 ok, -15 no answer). nfc_init_if_enabled does it only when nfc_may_arm()
 * allows.
 */
s8 nfc_power_up(void)
{
    s8 r = 0;
    int i;

    if (nfc_session.x001 != 1) {
        for (i = 0; i <= 1; i++) {
            r = nfc_init((struct FlashBase *)0x2000000);
            if (r == 0)
                goto ok;
            nfc_session.x001 = 2;
            nfc_sleep_field_off_set();
        }
        goto done;
    ok:
        nfc_session.x001 = 1;
        nfc_wait_field_on();
    }
done:
    return r;
}

/*
 * NFC init at start-up if the feature flag nfc_may_arm() allows it.
 */
int nfc_init_if_enabled(void)
{
    if (nfc_may_arm() == 0)
        return 0;
    return nfc_power_up();
}


/*
 * NFC session: send b bytes from a to the peer, timeout c seconds. To a Tamagotchi
 * (x004 == 0xF040): one raw NFC-DEP transfer (nfc_dep_send). To a phone/app: 0x28 short
 * busy waits (nfc_short_delay) then up to 3 LLCP/SNEP PUT attempts as a SNEP client
 * (nfc_snep_client, c seconds each) with 0x14 busy waits between them, stopping on 0 ok /
 * 4 abort / -6 / -7 (peer
 * released). comm_trace() calls are empty debug/progress trace hooks, comm_user_cancelled() =
 * session cancelled. Result also in nfc_result.
 */
int nfc_send(void *a, int b, int c)
{
    int i, j;
    int n = 3;
    int r;

    comm_trace();
    if (comm_user_cancelled() == 0) {
        if (nfc_session.peer_kind == 0xf040) {
            nfc_result = nfc_dep_send(a, b, c);
        } else {
            for (i = 0; i < 0x28; i++) {
                nfc_short_delay();
                if (comm_user_cancelled())
                    goto out;
            }
            i = 0;
            do {
                comm_trace();
                r = comm_user_cancelled();
                if (r)
                    goto out;
                r = nfc_snep_client(a, b, c);
                nfc_result = r;
                comm_trace();
                if (nfc_result == 4 || nfc_result == 0 || nfc_result == -6 || nfc_result == -7)
                    break;
                comm_trace();
                for (j = 0; j < 0x14; j++) {
                    nfc_short_delay();
                    if (comm_user_cancelled())
                        goto out;
                }
                comm_trace();
                i++;
            } while (i < n);
        }
        comm_trace();
    }
out:
    return nfc_result;
}

/*
 * NFC session: receive a file from the peer into a (d = 1: a is a flash address and the
 * data is programmed into flash as it arrives, else RAM), *b = bytes received, e =
 * maximum size, c = timeout in seconds. Tamagotchi: raw NFC-DEP receive (nfc_dep_receive:
 * c s for the first frame, then 5 s per frame). Phone/app: 0x28 short busy waits
 * (nfc_short_delay), then up to 6 SNEP server sessions (nfc_snep_server with a fixed 10 s each,
 * c is ignored on this path) until 0 / 4 / -6 / -7. This is how TAMAGO item files get
 * into the flash item banks. Result in nfc_result.
 */
int nfc_receive(int a, int *b, int c, s8 d, int e)
{
    int i, j;
    int n = 6;

    comm_trace();
    if (comm_user_cancelled() == 0) {
        if (nfc_session.peer_kind == 0xf040) {
            nfc_result = nfc_dep_receive(a, (u32 *)b, e, c, d);
        } else {
            for (i = 0; i < 0x28; i++) {
                nfc_short_delay();
                if (comm_user_cancelled())
                    goto out;
            }
            for (;;) {
                if (comm_user_cancelled())
                    goto out;
                break;
            }
            for (i = 0; i < n; i++) {
                *b = 0;
                comm_trace();
                nfc_result = nfc_snep_server(a, b, e, 10, d);
                comm_trace();
                if (nfc_result == 4 || nfc_result == 0 || nfc_result == -6 || nfc_result == -7)
                    break;
                comm_trace();
                for (j = 0; j < 0x14; j++) {
                    nfc_short_delay();
                    if (comm_user_cancelled())
                        goto out;
                }
                comm_trace();
            }
        }
        comm_trace();
    }
out:
    return nfc_result;
}


/*
 * Test mode NFC check (only caller test_mode, which first sets nfc_session.x001 = 2 to
 * force a new power-up): show "NFC INITIALIZE", probe the front-end (nfc_power_up) and show
 * "OK" (returns 1) or "NG" (halts forever).
 */
int test_nfc_init_check(void)
{
    nfc_show_session_screen(5);
    if (nfc_power_up() != 0) {
        nfc_show_session_screen(7);
        for (;;)
            asm volatile("halt");
    }
    nfc_show_session_screen(6);
    return 1;
}

/*
 * Test-mode NFC link test (asm; only caller the factory test mode test_mode, which sets
 * session type 1 (nfc_session.x003 = 1) and runs it twice: peer x004 = 0xF040, another toy
 * over NFC-DEP, then 0xF0F0, the app-side tester over SNEP). Power-up, then wait: an RF
 * field makes it the target (role 2; the peer must be 0xF040 or 0xF0F0), B held (button
 * history btn_samples[1] & 3 == 1) with a 0xF040 peer makes it the initiator (role 1); the
 * exchange runs in comm_exchange (type 1: the 0x2000-byte 0x55AA / 0xAA55 pattern).
 * Success: the peer record's header byte 0x4D (the version, 4) as three digits on the
 * "VER:" picture 0x2087100 with sound 0x19; A alone = run again, B = return 1. Failure:
 * the "failed" picture; B = retry, A+C = return 0. With x004 == 0xF0F0, A+C also gives up
 * while waiting.
 */
u8 test_nfc_link(void)
{
    u16 id;
    u8 v;
    int k;
    int a;

    a = 0;
again:
    comm_trace2(a);
    nfc_show_session_screen(0);
    nfc_cancel_request = 0;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    comm_reset_records();
    comm_exclusive_timing_on();
    if (nfc_power_up() != 0)
        goto fail;
    nfc_wait_field_on();
    nfc_wait_field_on();
    for (;;) {
        btn_history_sample();
        if (nfc_field_seen() == 0) {
            nfc_session.x000_b = 1;
            if (nfc_field_irq == 1) {
                nfc_show_session_screen(1);
                nfc_field_irq = 2;
            }
            if (nfc_find_peer_target(&id) != 0)
                goto fail;
            if (id != 0xf040 && id != 0xf0f0)
                goto fail;
            nfc_session.x002 = 2;
            goto run;
        }
        k = btn_samples[1] & 3;
        if (k == 1 && nfc_session.peer_kind == 0xf040) {
            nfc_show_session_screen(k);
            nfc_session.x002 = k;
run:
            if (comm_exchange() != 0)
                goto fail;
            break;
        }
        if (nfc_session.peer_kind == 0xf0f0 && (P0_P0D & 5) == 0)
            goto quit;
        asm volatile("halt");
    }
    comm_exclusive_timing_off();
    nfc_sleep_until_field(1);
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2087100);
    v = comm_rx_record[0x4d];
    gfx_object_create(0x44, 0x38, 0x20, v / 100, 0, 0x208edb0);
    gfx_object_create(0x4d, 0x38, 0x20, v / 10, 0, 0x208edb0);
    gfx_object_create(0x56, 0x38, 0x20, v % 10, 0, 0x208edb0);
    func_00009A6();
    snd_enabled = 1;
    snd_play(0x19);
    snd_wait_end();
    snd_enabled = 0;
    test_clear_buttons();
    for (;;) {
        if (btn_a_presses != 0 && (P0_P0D & 7) == 6) {
            nfc_wait_field_on();
            nfc_wait_field_on();
            a = 0;
            goto again;
        }
        if (btn_b_presses != 0) {
            comm_exclusive_timing_off();
            return 1;
        }
        if (nfc_session.peer_kind == 0xf0f0 && (P0_P0D & 5) == 0)
            goto quit;
        asm volatile("halt");
    }
fail:
    nfc_sleep_until_field(1);
    nfc_show_session_screen(2);
    btn_history_init();
    for (;;) {
        btn_history_sample();
        if ((P0_P0D & 5) == 0)
            goto quit;
        if ((btn_samples[1] & 3) == 1)
            break;
    }
    btn_history_init();
    btn_history_sample();
    a = 0;
    goto again;
quit:
    comm_exclusive_timing_off();
    return 0;
}


/*
 * NFC communication, initiator side (role 1; menu choices 1..4, 7 and the accepting side
 * of a proposal): power up, run the exchange (comm_exchange -> comm_exchange_initiator, which finds
 * the peer with nfc_find_peer_initiator), outcome comm_run_outcome, result screens. After a completed
 * session type 3 (game round 1) it returns 0x63 so that nfc_menu_session runs round 2 as
 * session type 4; entering type 4 first shows "もういちど つうしんするよ" (comm_retry_msg)
 * and waits for the game to end (tmr_flags bit 5) (asm).
 */

int nfc_session_initiator(u8 a)
{
    int kept;
    int r;
    int res;
    u8 c;
    int k;

    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    kept = room_view;
    r = 0;
    if (kept > 1)
        r = 1;
    if (r == 1)
        return 0;
again:
    comm_trace2(0);
    if (nfc_session.x003 == 4) {
        btn_mask(7);
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        nfc_cancel_request = 0;
        tmr_t5_start_timeout(3000);
        comm_retry_msg(0);
        comm_erase_banks();
        while (!(tmr_flags & 0x20))
            asm volatile("halt");
        anim_busy = 0;
    }
    if (nfc_session.x003 == 4) {
        btn_mask(7);
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        nfc_cancel_request = 0;
    } else {
        btn_mask(3);
    }
    nfc_show_session_screen(3);
    snd_wait_before_flash();
    comm_reset_records();
    comm_exclusive_timing_on();
    if (nfc_power_up() != 0)
        goto nfail;
    nfc_wait_field_off();
    res = comm_exchange();
    if (res != 0 || nfc_session.x003 != 3)
        btn_unmask(7);
    if (res == 0) {
        snd_wait_end();
        if (sys_battery_low() == 1)
            sys_power_down();
        snd_play(0x4f);
        comm_run_outcome();
        nfc_wait_field_on();
        comm_exclusive_timing_off();
        if (nfc_session.x003 == 3)
            return 0x63;
        comm_count_and_save();
        return 1;
    }
    if (nfc_result == 4) {
        if (nfc_cancel_request != 0) {
            nfc_cancel_request = 0;
            nfc_wait_field_on();
            comm_exclusive_timing_off();
            return 0;
        }
    } else {
        room_view = 0;
    }
nfail:
    nfc_wait_field_off();
    nfc_show_session_screen(4);
    comm_exclusive_timing_off();
wait:
    btn_wait_press_or_timeout(10);
    if ((c = btn_a_presses) != 0)
        goto wait;
    if (btn_b_presses != 0) {
        btn_b_presses = c;
        goto again;
    }
    if (btn_c_presses != 0) {
        btn_c_presses = 0;
        room_view = kept;
    } else if (ui_timeout == 0) {
        goto none;
    }
    nfc_wait_field_on();
    nfc_session.x000_b = 0;
    comm_exclusive_timing_off();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    return 0;
none:
    nfc_wait_field_on();
    nfc_session.x000_b = 0;
    comm_exclusive_timing_off();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 1;
}


/*
 * Communication session, target side (role 2, nfc_session.x002 != 1), reached from the menus
 * only for menu choice 6 (session type 8, "4Uアプリ") and menu choice 5 with role 2 (the
 * proposing side of session type 6); the other toy-to-toy sessions find their target in
 * an idle toy (nfc_idle_session / nfc_idle_target_session, session type 0). Show the "touch" screen
 * (nfc_show_session_screen(3)), clear the exchange buffers, power up the front-end (nfc_power_up),
 * arm detection and wait for a peer (nfc_find_peer_target). When a peer is found (id 0xF040 = a
 * Tamagotchi, or the pseudo peer 0xF0F0 = the app in type 8) run the packet exchange
 * comm_exchange (F70b, by session type x003); on success play the jingle
 * (snd_play(0x4F)), store the received data (comm_run_outcome) and return 1. 0xF0EF (no
 * Tamagotchi within 30 s, or a foreign device) or any error ends the session
 * (room_view = 0). Afterwards it shows the
 * failure screen, re-arms RfOff detection and polls the buttons (btn_wait_press_or_timeout(10) = 10
 * ticks): A = keep waiting, B = restart the session, C = cancel (returns 0);
 * ui_timeout = 10 restores the menu timeout. a = the menu choice passed down from
 * nfc_menu_session.
 */
int nfc_session_target(u8 a)
{
    int s;
    int t;
    s8 r;
    int k;
    u8 c;
    u16 id;

    btn_mask(3);
    s = room_view;
    t = 0;
    if (s > 1)
        t = 1;
    if (t == 1) {
        btn_unmask(7);
        return 0;
    }
again:
    for (;;) {
        nfc_show_session_screen(3);
        comm_reset_records();
        comm_exclusive_timing_on();
        r = nfc_power_up();
        if (r != 0)
            break;
        nfc_wait_field_on();
        comm_trace();
        btn_mask(3);
        nfc_field_irq = 2;
        nfc_session.x000_b = 1;
        r = nfc_find_peer_target(&id);
        if (r == 0) {
            comm_trace2(r);
            nfc_session.peer_kind = nfc_session.detected_peer;
            if (id == 0xf0ef) {
                room_view = 0;
                break;
            }
            btn_mask(3);
            k = comm_exchange();
            btn_unmask(7);
            if (k == 0) {
                snd_wait_end();
                if (sys_battery_low() == 1)
                    sys_power_down();
                snd_play(0x4f);
                comm_run_outcome();
                comm_count_and_save();
                nfc_wait_field_on();
                comm_exclusive_timing_off();
                return 1;
            }
            if (nfc_result != 4)
                goto L10;
            if (nfc_cancel_request == 0)
                break;
            nfc_cancel_request = 0;
            nfc_wait_field_on();
            comm_exclusive_timing_off();
            return 0;
        }
        if (nfc_result != 4)
            goto L10;
        if (nfc_cancel_request == 0 && btn_c_presses == 0) {
            comm_trace();
            break;
        }
        nfc_cancel_request = 0;
        nfc_wait_field_on();
        comm_exclusive_timing_off();
        btn_unmask(7);
        return 0;
L10:
        room_view = 0;
        break;
    }
    nfc_sleep_until_field(1);
    nfc_show_session_screen(4);
    btn_unmask(7);
    comm_exclusive_timing_off();
wait:
    btn_wait_press_or_timeout(10);
    if ((c = btn_a_presses) != 0)
        goto wait;
    if (btn_b_presses != 0) {
        btn_b_presses = c;
        goto again;
    }
    if (btn_c_presses != 0) {
        btn_c_presses = 0;
        room_view = s;
        goto L16;
    }
    if (ui_timeout != 0) {
L16:
        nfc_wait_field_on();
        nfc_session.x000_b = 0;
        comm_exclusive_timing_off();
        btn_unmask(7);
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        ui_timeout = 10;
        return 0;
    }
    nfc_wait_field_on();
    nfc_session.x000_b = 0;
    comm_exclusive_timing_off();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return 1;
}

/*
 * Entry point of the NFC menus (called from the F7 menu handlers with the menu choice a):
 * maps the menu choice to the session type nfc_session.x003 (= record byte 0x4C) and role
 * x002:
 *   menu choice 1 (あいさツーしん, comm_greeting_session) -> session type 2, initiator;
 *   2 (ツーしんゲーム, comm_game_session) -> type 3, initiator, then type 4 (round 2) when
 *     nfc_session_initiator returns 0x63 after round 1; x003 is cleared to 0 after type 4;
 *   3 (アイテムこうかん あげる, comm_give_item) -> type 5, initiator;
 *   4 (きねんしゃしん, comm_photo_session) -> type 7, initiator;
 *   5 (プロポーズ, comm_menu_proposal via comm_session_menu) -> type 6, role set by the caller
 *     (する = propose = 2 target, うける = accept = 1 initiator);
 *   6 (4Uアプリ, comm_menu_app) -> type 8, target (the app link);
 *   7 (debug アイテムSEND, comm_menu_item_send) -> type 8, initiator;
 *   0 -> nothing changed (no caller).
 * Initiator sessions run nfc_session_initiator (asm), target sessions nfc_session_target. Returns 1 on a
 * completed exchange.
 */
int nfc_menu_session(u8 a)
{
    int r;

    nfc_cancel_request = 0;
    switch (a) {
    case 0:
        break;
    case 1:
        nfc_session.x003 = 2;
        nfc_session.x002 = 1;
        break;
    case 2:
        nfc_session.x003 = 3;
        nfc_session.x002 = 1;
        break;
    case 3:
        nfc_session.x003 = 5;
        nfc_session.x002 = 1;
        break;
    case 4:
        nfc_session.x003 = 7;
        nfc_session.x002 = 1;
        break;
    case 5:
        nfc_session.x003 = 6;
        break;
    case 6:
        nfc_session.x003 = 8;
        nfc_session.x002 = 2;
        break;
    case 7:
        nfc_session.x003 = 8;
        nfc_session.x002 = 1;
        break;
    }
again:
    if (nfc_session.x002 == 1) {
        r = nfc_session_initiator(a);
        if (r == 0x63) {
            if (nfc_session.x003 == 3) {
                nfc_session.x003 = 4;
                goto again;
            }
        } else if (nfc_session.x003 == 4) {
            nfc_session.x003 = 0;
            r = 1;
        }
    } else
        r = nfc_session_target(a);
    return r;
}



/*
 * NFC: reset the session packet headers (comm_reset_records).
 */
int comm_reset_records_wrapper(void)
{
    comm_reset_records();
    return 0;
}

/*
 * Idle toy as NFC target (asm; called by nfc_idle_session with session type 0, or 4 for the
 * second game round): returns 0 at once when no RF field was seen (nfc_field_seen); else
 * waits 2 s for an ATR_REQ (nfc_find_peer_target). Peer 0xF0EF in type 0 (a field but no
 * Tamagotchi: a phone or reader) -> jingle 0x4F and result 4, the "touch" event of
 * nfc_idle_touch; 0xF0EF in type 4 -> failure screen. Peer 0xF040 -> the exchange
 * comm_exchange (role 2; F70b adopts the peer's session type) and the outcome comm_run_outcome;
 * after type 3 it returns 0x63 so that the caller runs round 2, else 3.
 */
int nfc_idle_target_session(void)
{
    u16 id;
    int kept;
    int r;
    s8 res;
    u8 a, b, c;

    kept = room_view;
    r = 0;
again:
    if (nfc_session.x003 == 4) {
        btn_mask(7);
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        nfc_cancel_request = 0;
        comm_exclusive_timing_off();
        tmr_t5_start_timeout(3000);
        comm_retry_msg(0);
        comm_erase_banks();
        while (!(tmr_flags & 0x20))
            asm volatile("halt");
        anim_busy = 0;
        nfc_field_irq = 2;
    }
    comm_exclusive_timing_on();
    if (nfc_power_up() != 0) {
        btn_unmask(7);
        goto done;
    }
    if (nfc_field_seen() != 0) {
        nfc_session.x000_b = 0;
        nfc_session.x003 = 0;
        a = btn_a_presses;
        b = btn_b_presses;
        c = btn_c_presses;
        comm_exclusive_timing_off();
        btn_a_presses = a;
        btn_b_presses = b;
        btn_c_presses = c;
        return r;
    }
    room_idle = 0;
    comm_trace2(0);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    nfc_field_irq = 2;
    if (nfc_session.x003 == 4) {
        btn_mask(7);
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        nfc_cancel_request = 0;
    } else {
        btn_mask(3);
    }
    nfc_show_session_screen(3);
    nfc_session.x000_b = 1;
    r = 3;
    comm_reset_records_wrapper();
    if (nfc_find_peer_target(&id) != 0)
        goto nod0a;
    comm_trace2(1);
    nfc_session.x002 = 2;
    nfc_session.peer_kind = nfc_session.detected_peer;
    if (id == 0xf0ef && nfc_session.x003 == 0) {
        btn_mask(7);
        snd_wait_end();
        if (sys_battery_low() == 1)
            sys_power_down();
        snd_play(0x4f);
        comm_exclusive_timing_off();
        btn_unmask(7);
        return 4;
    }
    if (id == 0xf0ef) {
        room_view = 0;
        btn_unmask(7);
        goto retry;
    }
    res = comm_exchange();
    btn_unmask(7);
    if (res != 0)
        goto bad;
    snd_wait_end();
    if (sys_battery_low() == 1)
        sys_power_down();
    snd_play(0x4f);
    comm_run_outcome();
    nfc_wait_field_on();
    room_view = 0;
    if (nfc_session.x003 == 3)
        return 0x63;
    comm_count_and_save();
    goto done;
bad:
    if (nfc_result == 4)
        nfc_wait_field_on();
    else
        room_view = 0;
    goto retry;
nod0a:
    if (nfc_result == 4)
        nfc_wait_field_on();
    room_view = 0;
    goto retry;
done:
    nfc_session.x000_b = 0;
    nfc_session.x003 = 0;
    comm_exclusive_timing_off();
    return r;
retry:
    nfc_sleep_until_field(1);
    nfc_show_session_screen(4);
    comm_exclusive_timing_off();
    btn_unmask(4);
    do {
        btn_wait_press_or_timeout(10);
    } while ((c = btn_a_presses) != 0);
    if (btn_b_presses != 0) {
        btn_b_presses = c;
        nfc_wait_field_on();
        btn_mask(4);
        goto again;
    }
    if (btn_c_presses != 0) {
        btn_c_presses = 0;
        room_view = kept;
    } else if (ui_timeout != 0) {
    }
    nfc_wait_field_on();
    nfc_session.x000_b = 0;
    comm_exclusive_timing_off();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    if (nfc_session.x003 == 4)
        nfc_session.x003 = 0;
    return 3;
}

/*
 * The idle toy's NFC session (from nfc_idle_touch): session type 0 = automatic, the toy is
 * the target and adopts the initiator's session type from its record byte 0x4C (types 4,
 * 6 and 8 refused, comm_exchange_target): nfc_idle_target_session. After a type-3 round 1 (result 0x63) it
 * runs round 2 as session type 4; then refresh the friend list (comm_result_wait_book,
 * visit_record_clear). Result: 3 = Tamagotchi session done or failed, 4 = a non-Tamagotchi
 * peer (0xF0EF) = the idle-touch event, other values = not run.
 */
/*
 * The idle toy's NFC session (from nfc_idle_touch): session type 0 = automatic, the toy is
 * the target and adopts the initiator's session type from its record byte 0x4C (types 4,
 * 6 and 8 refused, comm_exchange_target): nfc_idle_target_session. After a type-3 round 1 (result 0x63) it
 * runs round 2 as session type 4; then refresh the friend list (comm_result_wait_book,
 * visit_record_clear). Result: 3 = Tamagotchi session done or failed, 4 = a non-Tamagotchi
 * peer (0xF0EF) = the idle-touch event, other values = not run.
 */

int nfc_idle_session(void)
{
    struct NfcSession *p;
    int r;

    nfc_session.x003 = 0;
again:
    p = &nfc_session;
    nfc_cancel_request = 0;
    btn_mask(4);
    r = nfc_idle_target_session();
    btn_unmask(4);
    if (r == 0x63) {
        if (p->x003 == 3) {
            p->x003 = 4;
            goto again;
        }
    }
    if (comm_partner_slot == 30) {
        if (save.friend_list[comm_partner_slot].x000_b != 0)
            comm_result_wait_book();
        visit_record_clear();
    }
    nfc_cancel_request = 0;
    p = 0;
    return r;
}

/*
 * The RAM variables this file defines.
 */
u16 nfc_field_irq;
u8 comm_tx_packet[0x180];
union CommPacket comm_rx_packet;
struct NfcSession nfc_session;
s8 nfc_result;
