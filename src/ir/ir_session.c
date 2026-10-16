/*
 * The legacy IR exchange session: running a session, the packet exchange and record checks,
 * point gifts, the IR test mode, and visits started by an exchange.
 *
 * Code 24F9218..24FC3B2, .rodata from 25381D8.
 */
#include "proto/ir/ir_session.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/pet/pet_bath.h"
#include "proto/diary/diary_notebook.h"
#include "proto/anim/anim_scripts.h"
#include "proto/comm/comm_menu.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/comm/comm_record_pics.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/comm/comm_flash_buffers.h"
#include "proto/ir/ir_receive.h"
#include "proto/ir/ir_packets.h"
#include "proto/ir/ir_driver.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care.h"
#include "proto/owner/owner_profile.h"
#include "proto/spot/spot_reader.h"
#include "proto/test/test_mode.h"
#include "proto/comm/comm_checks.h"
#include "proto/comm/comm_exchange.h"
#include "proto/town/town_places.h"
#include "proto/town/town_street.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/* declarations shared with F7 */
#define REFRESH() do { snd_wait_end(); if (sys_battery_low() == 1) sys_power_down(); } while (0)
#define REFRESH_24F7768() do { snd_wait_end(); if (sys_battery_low() == 1) sys_power_down(); } while (0)

/*
 * End of a visit/outing: the visitor-leaving message screen (longer and with a beep once
 * x016 has reached 270).
 */
int visit_leave_message(void)
{
    u8 t;

    if (visit_partner.mode == 0)
        return 0;
    if (visit_partner.seconds <= 0x10d) {
        ui_new_screen_with_picture(0, D_208B004);
        t = 10;
    } else {
        ui_new_screen_with_picture(0, D_208B004);
        snd_play(4);
        snd_wait_end();
        t = 0x3c;
    }
    ui_timeout = t;
    btn_clear();
    for (;;) {
        if (btn_a_presses != 0) {
            ui_timeout = t;
            btn_a_presses = 0;
        }
        if (btn_b_presses != 0) {
            ui_timeout = 10;
            sys_idle_seconds = 0;
            sys_display_state = 0;
            return 0;
        }
        if (btn_c_presses != 0) {
            ui_timeout = 10;
            if (visit_partner.seconds <= 0x10d)
                return 1;
            btn_c_presses = 0;
            sys_idle_seconds = 0;
            sys_display_state = 0;
        }
        sys_idle_tick();
        if (ui_timeout == 0) {
            if (visit_partner.seconds <= 0x10d)
                return 1;
            sys_idle_seconds = 0;
            sys_display_state = 0;
            return 0;
        }
    }
}

/*
 * The owner name into the outgoing legacy IR packet from byte 5: 16 bytes, one more than the
 * 20-byte packet holds. In the original the last one landed on the byte after the packet, the
 * low byte of tmr_t5_ticks; kept.
 */
static void ir_tx_put_owner_name(void)
{
    memcpy((u8 *)&ir_tx_packet + 5, save.pet.owner_name, 15);
    *(volatile u8 *)&tmr_t5_ticks = save.pet.owner_name[15];
}

/*
 * The 16 name bytes friend_find_or_add compares and stores, for a partner of the legacy IR
 * protocol, whose packets carry a 5-byte name. The original passed the RAM address of that
 * name, so the bytes after it in its RAM went along; kept, for the same friend records:
 *   ir_rx_name16: the 15 bytes of the received packet from its byte 5, then the first byte of
 *                 the outgoing packet (ir_tx_packet.sig_code);
 *   ir_partner_name16: ir_partner_name (5), visit_diary_page_mark, visit_diary_full, three
 *                 unused (zero) bytes, the first 4 bytes of outing and the first 2 of job.
 */
static int ir_rx_name16(u8 *buf)
{
    memcpy(buf, (u8 *)&ir_rx_packet + 5, 15);
    buf[15] = ir_tx_packet.sig_code;
    return (int)buf;
}

static int ir_partner_name16(u8 *buf)
{
    memset(buf, 0, 16);
    memcpy(buf, ir_partner_name, 5);
    buf[5] = visit_diary_page_mark;
    buf[6] = visit_diary_full;
    memcpy(buf + 10, &outing, 4);
    memcpy(buf + 14, &job, 2);
    return (int)buf;
}

/*
 * Visit/outing over (x016 >= 270): end it - new partner record (friend_find_or_add ->
 * comm_partner_slot), the diary-writing scene (diary_writing_scene), save.
 */
int visit_end(void)
{
    u8 name[16];

    if (visit_partner.mode == 0)
        return 0;
    if (visit_partner.seconds <= 0x10d)
        return 0;
    visit_partner.seconds = 0;
    visit_partner.over = 0;
    if (visit_partner.mode == 1) {
        pet_add_happiness(2);
        pet_feed(2);
    }
    visit_load_partner_pics(1);
    ir_session_leave();
    comm_partner_slot = friend_find_or_add(ir_partner_id_lo, ir_partner_id_hi, ir_partner_name16(name));
    if (comm_friend_is_new != 1)
        comm_load_visitor_pictures(1);
    diary_writing_scene();
    visit_partner.mode = 0;
    snd_wait_end();
    save_game();
    return 1;
}

/*
 * Stamp-card messages on the receiving side of an IR exchange (comm_exchange_kind == 5, the
 * communication mode, not a Touch Spot): "スタンプカードを
 * もっていないよ!" / "このともだちからはスタンプをおしてもらえないよ!".
 */
int ir_stamp_card_messages(void)
{
    if (comm_exchange_kind == 5) {
        if (ir_session_flags & 0x20) {
            anim_scene_room(3);
            anim_draw_pic(0, D_2104794, 0, 0x40, 0x24);
            anim_draw_text(1, D_25388F8, 0, 0x16, 0x1b);
            anim_draw_text(2, D_253890C, 0, 0x16, 0x2b);
            anim_frame(500, 0);
            return 1;
        }
        if (ir_session_flags & 0x10) {
            anim_scene_room(4);
            anim_draw_pic(0, D_2104794, 0, 0x40, 0x24);
            anim_draw_text(1, D_2538920, 0, 0x10, 0x14);
            anim_draw_text(2, D_2538934, 0, 0x16, 0x24);
            anim_draw_text(3, D_2538948, 0, 0x1c, 0x34);
            anim_frame(500, 0);
            return 1;
        }
    }
    return 0;
}


/*
 * IR communication session of kind a for ir_session (legacy, reachable only from
 * hidden entries). Copy the RAM routines, ir_port_ptr = P0 (sys_clock_osc3), handle a running
 * visit, poll the buttons on P0 instead of their interrupts (btn_disable / 2505034 /
 * 2505070), power the transceiver up (ir_rx_start), clear both 20-byte buffers, then
 * poll on a 6.3 ms tick (tmr_t5_start_raw(0x2D, 0xF)) until ui_timeout runs out (10 s, 20 s in
 * mode 5, or comm_gift_item on a retry): edges seen (ir_edge_count: the peer pinged) -> we are the
 * responder, comm_is_initiator = 0; else a fresh B press (not with ir_answer_only == 1 or ir_session_flags
 * bit 1) shows the "connecting" picture and sends the 10-pulse ping ir_send_ping -> we are
 * the initiator, comm_is_initiator = 1; either way ir_exchange runs the exchange (1 = ok, 3 ->
 * 2, 0 = failure picture, then B = retry, C = quit). C held while waiting cancels. On the
 * 4U+ no edge is ever counted (no P04 ISR) and ir_exchange's transfer is a stub, see
 * notes/findings/ir.md.
 */
int ir_session_run(u8 a)
{
    int k;
    u8 t;
    u8 c;

    ir_test_mode = 0;
    sys_copy_ram_routines();
    ir_edge_count = 0;
    ir_edge_count2 = 0;
    sys_clock_osc3();
    if (visit_partner.mode != 0) {
        if (visit_leave_message() != 0)
            return 0;
        if (visit_partner.mode == 2)
            diary_write_pending_page();
    }
retry:
    btn_disable();
    btn_history_init();
    ir_rx_start();
    memset(&ir_tx_packet, 0, 0x14);
    memset(&ir_rx_packet, 0, 0x14);
    tmr_t5_start_raw(0x2d, 0xf);
    comm_is_initiator = 0;
    if (!(ir_session_flags & 2)) {
        if (comm_exchange_kind == 5)
            t = 0x14;
        else
            t = 10;
    } else
        t = comm_gift_item;
    ui_timeout = t;
    btn_history_init();
    do {
        btn_history_sample();
        if (ir_edge_count != 0) {
            comm_is_initiator = 0;
        } else {
            if ((ir_session_flags & 2) || ir_answer_only == 1 || ((btn_samples[1] & 3) != 1 && !(ir_session_flags & 1)))
                goto wait;
            if (!(ir_session_flags & 1))
                snd_play(2);
            ui_new_screen_with_picture(1, D_21091E4);
            snd_wait_end();
            if (ir_edge_count != 0)
                goto wait;
            ir_send_ping((int)&ir_tx_packet);
            comm_is_initiator = 1;
        }
        k = ir_exchange();
        tmr_t5_standard_tick();
        switch (k) {
        case 1:
            goto ok;
        case 3:
            goto ok2;
        case 0:
            goto c0;
        default:
            return 0;
        }
    wait:
        if (!(ir_session_flags & 2) && (btn_samples[2] & 1)) {
            snd_play(3);
            snd_wait_end();
            btn_enable();
            visit_end();
            return 0;
        }
        tmr_t5_ticks = 0;
        while (tmr_t5_ticks == 0)
            asm volatile("halt");
    } while (ui_timeout != 0);
    visit_end();
ok:
    btn_enable();
    return 1;
ok2:
    btn_enable();
    return 2;
c0:
    if ((u8)ir_stamp_card_messages() == 0)
        ui_new_screen_with_picture(2, D_21091E4);
    btn_enable();
w0:
    btn_wait_press_or_timeout(10);
    if ((c = btn_a_presses) != 0)
        goto w0;
    if (btn_b_presses != 0) {
        btn_b_presses = c;
        goto retry;
    }
    if (btn_c_presses != 0)
        visit_end();
    if (ui_timeout == 0) {
        visit_end();
        return 1;
    }
    ui_timeout = 10;
    return 0;
}

/*
 * Session clean-up: the empty hook comm_redraw_hook, restore the standard T16 ch.5 tick,
 * 10 s menu timeout.
 */
void ir_session_cleanup(void)
{
    comm_redraw_hook();
    tmr_t5_standard_tick();
    ui_timeout = 10;
}

/*
 * The old Tamagotchi-to-Tamagotchi exchange over IR (legacy state machine, application
 * level; built from asm). Both sides swap a 20-byte header (ours in ir_tx_packet, theirs in
 * ir_rx_packet; x001 = message type, paired request/answer codes: 2/3 greeting, 8/9 gift
 * (item follows), 10/11 play-together with ir_rx_packet.owner_id_hi bits 6-7 = "has the stamp card"
 * and x00e/x00f = result/item index (bit 7 = reject), 12/13 photo (x003 & 0x3f = pose,
 * x00c = which side), 14/15 proposal, 5 = marriage with x00b = the agreed game, 0x70 =
 * point gift: a peer 0x70 grants 1000 points here, as in ir_point_gift_packets; not a refusal) and
 * then the payload records that land in the save banks 0x27D5400 /
 * 0x27A5400 (profile + images at +0x14) and 0x27C5428 / 0x27CB814 (received items /
 * portrait). The transfer is ir_transfer (status 0x20 = ok, 0xAA = aborted by the user,
 * 0x10 = retry), with ir_transfer_abort (abort) and ir_send_data_stub (data to send): in this
 * firmware all three are stubs (`ret`, `ret`, `return 0`), so st/sub stay uninitialised
 * and the session fails, aborts or loops on "0x10" forever. comm_is_initiator == 0: the peer
 * pinged first and we answer (first branch); 1: we pressed B and initiated (second branch,
 * header loaded from the bank + 0x200). Outcomes: store received items
 * (item_add_to_inventory / comm_item_exchange_result / ir_store_received_message), record the friend (friend_add_after_exchange +
 * comm_load_visitor_pictures(1) = add to the たまとも list), unlock mini games (pet_add_happiness), apply the
 * partner record (comm_apply_partner_record...), save (save_game). Return 0 = failed, 1 = ok, 2 =
 * cancelled, 3 = partner left. ir_block_kind/D14/D28/D4C are the transfer-state bytes shared
 * with ir_transfer. (Confidence medium on the message-type names.)
 */
u8 f_25029DA(u8 **, u32 *, u8 *) __asm__("ir_send_data_stub");
void ir_partner_hook_b(int);
void ir_partner_hook_a(int);
void f_2500A9C(int) __asm__("ir_pause_short");
u8 ir_exchange(void)
{
    u8 tmp[12];
    u8 st;
    s8 sub;
    u8 c;
    u8 *a;
    u32 b;
    u8 i;
    struct DlCharaRecord *s;
    struct TamagoFile *p;
    int x, y;

    ir_exchange_flag = 0;
    memset(&ir_tx_packet, 0, 20);
    memset(&ir_rx_packet, 0, 20);
    if (comm_is_initiator == 0) {
        btn_history_sample();
        ui_new_screen_with_picture(1, 0x21091e4);
        ir_xfer_enter();
        ir_rx_status = 0;
        ir_rx_buf_count = 0;
        ir_rx_hunting = 1;
        ir_xfer_failed = 0;
        ir_session_busy = 1;
        ir_rx_buf_ptr = (u32)&gfx_frame_buffer;
        ir_block_kind = 2;
        ir_xfer_started = 0;
again1:
        ir_transfer();
        if (st == 0x20 && sub == 0)
            ir_xfer_failed = 0;
        else if (st == 0xaa)
            goto aa;
        else if (st == 0x10)
            goto again1;
        else
            ir_xfer_failed = 1;
        ir_session_busy = 0;
        if (ir_xfer_failed == 1)
            goto fail;
        if (ir_rx_hunting != 0)
            goto fail;
        if (ir_answer_only == 1 && ir_tx_packet.kind == 0x70) {
            ir_session_cleanup();
            return 2;
        }
        if (ir_block_kind != 0x88)
            goto fail;
        tmr_t5_standard_tick();
        if (ir_check_kinds_pair() == 7) {
            tmr_t5_standard_tick();
            ui_timeout = 10;
            comm_redraw_hook();
            return 0;
        }
        if (visit_start(4) != 0) {
            snd_wait_end();
        } else if (ir_visit_pair_check() != 0) {
            snd_wait_end();
            diary_writing_scene();
        } else if (ir_rx_packet.kind != 10 && (s8)ir_rx_packet.result2 < 0) {
            if (ir_msg_pair_check() != 0) {
                ir_session_leave();
                return 0;
            }
            ir_anim_pause_a(ir_rx_packet.result2 & 0x7f);
            if ((ir_rx_packet.kind != 8 && ir_rx_packet.kind != 9) || comm_exchange_kind == 4)
                anim_good_result();
            ir_next_packet_kind();
            if (ir_rx_packet.kind == 2) {
                friend_add_after_exchange();
                comm_load_visitor_pictures(1);
            }
            if (ir_rx_packet.kind == 4) {
                friend_add_after_exchange();
                comm_load_visitor_pictures(1);
            }
            if (ir_rx_packet.kind == 14) {
                pet_add_happiness(2);
                friend_add_after_exchange();
                comm_load_visitor_pictures(1);
            }
            if (ir_rx_packet.kind == 8) {
                comm_gift_bonus();
                ir_stamp_peers_confirm();
                if (ir_apply_points() == 0) {
                    item_load_pictures((void *)anim_pic_slots, (void *)0x27D5414, 3);
                    comm_item_exchange_result(comm_exchange_kind, (void *)0x27C5428, (void *)0x27D5400);
                } else {
                    anim_good_result();
                }
                ir_xfer_failed = ir_store_received_message(1, 0x27D5400);
                pet_add_happiness(1);
                friend_add_after_exchange();
                comm_load_visitor_pictures(1);
            }
            if (ir_rx_packet.kind == 9) {
                comm_gift_bonus();
                ir_stamp_peers_confirm();
                item_load_pictures((void *)anim_pic_slots, (void *)0x27D5414, 3);
                comm_item_exchange_result(comm_exchange_kind, (void *)0x27C5428, (void *)0x27D5400);
                ir_xfer_failed = ir_store_received_message(1, 0x27D5400);
                pet_add_happiness(1);
                friend_add_after_exchange();
                comm_load_visitor_pictures(1);
            }
        } else if (ir_rx_packet.kind == 12) {
            if (ir_tx_packet.kind != 13) {
                ir_session_leave();
                return 0;
            }
            if (ir_rx_packet.arg_lo == 1) {
                ir_anim_pause_b(ir_rx_packet.result & 0x7f);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27A5414, 3);
                comm_result_show_record();
            } else {
                ir_anim_pause_b(ir_rx_packet.result & 0x7f);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27A5414, 3);
                ir_partner_hook_b(ir_rx_packet.char_byte & 0x3f);
            }
            tmr_t5_standard_tick();
            ui_timeout = 0;
            comm_redraw_hook();
            return 1;
        } else if (ir_rx_packet.kind == 2) {
            if (ir_tx_packet.kind != 3) {
                ir_session_leave();
                return 0;
            }
            f_2500A9C((u8)((ir_tx_packet.owner_id_hi & 0x40) ^ 0x40));
            friend_add_after_exchange();
            comm_load_visitor_pictures(1);
        } else if (ir_rx_packet.kind == 10) {
            if (ir_tx_packet.kind != 11) {
                ir_session_leave();
                return 0;
            }
            if (ir_rx_packet.arg_lo == 0) {
                ir_session_leave();
                ir_show_proposal_refusal(1);
                ui_timeout = 10;
                return 1;
            }
            if ((ir_rx_packet.owner_id_hi & 0xc0) != 0xc0 && comm_exchange_kind == 7) {
                ir_session_flags |= 2;
                comm_gift_item = ir_rx_packet.result & 0x7f;
                ir_session_leave();
                return 1;
            }
            if ((ir_rx_packet.owner_id_hi & 0xc0) == 0xc0 && comm_exchange_kind != 7) {
                ir_session_flags |= 1;
                tmr_t5_pause_restore(30);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27D5414, 3);
                anim_proposal();
                if (ir_rx_packet.flags & 0x1c)
                    comm_show_rx_message();
                else
                    comm_show_proposal_line(ir_rx_packet.char_byte & 0x3f);
                ir_session_leave();
                return 1;
            }
            comm_game_result = ir_rx_packet.role;
            tmr_t5_standard_tick();
            if ((s8)ir_rx_packet.result < 0) {
                ir_anim_pause_a(ir_rx_packet.result & 0x7f);
                comm_result_pre_hook();
            } else {
                tmr_t5_pause_restore(30);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27D5414, 3);
                comm_result_anim();
            }
            pet_start_new_egg(comm_game_result);
            ui_timeout = 10;
            comm_redraw_hook();
            return 1;
        } else {
            comm_redraw_hook();
            return 0;
        }
        save_game();
        ui_timeout = 10;
        comm_redraw_hook();
        return 1;
    } else {
        if (f_25029DA(&a, &b, &c) == 7)
            return 0;
        ir_xfer_enter();
        ir_rx_status = 0;
        ir_rx_buf_count = 0;
        ir_rx_hunting = 1;
        ir_xfer_failed = 0;
        ir_session_busy = 1;
        ir_block_kind = 1;
        ir_tx_ptr = b;
        ir_send_data_ptr = (u32)a;
again2:
        ir_transfer();
        if (ir_rx_status == 0xfc && ir_block_kind == 0x85)
            goto r3;
        if (st == 0x20 && sub == 0)
            ir_xfer_failed = 0;
        else if (st == 0xaa)
            goto aa;
        else if (st == 0x10)
            goto again2;
        else
            ir_xfer_failed = 1;
        ir_session_busy = 0;
        if (ir_xfer_failed == 1)
            goto fail;
        if (ir_rx_hunting != 0)
            goto fail;
        if (ir_rx_packet.kind == 0x70) {
            tmr_t5_standard_tick();
            anim_points_gain(1000);
            pet_add_points(1000);
            visit_load_partner_pics(1);
            anim_happy();
            save_game();
            comm_redraw_hook();
            return 1;
        }
        if (ir_block_kind != 7) {
fail:
            ir_session_cleanup();
            return 0;
        }
        tmr_t5_standard_tick();
        ir_rx_record = (struct TamagoFile *)((ir_session_flags & 3) ? D_27A5400 : D_27D5400);
        a = (u8 *)*(u32 *)&ir_rx_record + 0x200;
        for (i = 0; i < 20; i++)
            ((u8 *)&ir_rx_packet)[i] = *a++;
        if (ir_check_kinds_pair() == 7) {
            tmr_t5_standard_tick();
            ui_timeout = 10;
            comm_redraw_hook();
            return 0;
        }
        if (visit_start(3) != 0) {
            snd_wait_end();
        } else if (ir_visit_pair_check() != 0) {
            diary_writing_scene();
            snd_wait_end();
        } else if (ir_tx_packet.kind != 10 && (s8)ir_rx_packet.result2 < 0) {
            ir_anim_pause_a(ir_rx_packet.result2 & 0x7f);
            anim_good_result();
            ui_timeout = 10;
            comm_redraw_hook();
            friend_add_after_exchange();
            comm_load_visitor_pictures(1);
            return 1;
        } else if (ir_tx_packet.kind == 14) {
            if (ir_rx_packet.kind != 15) {
                tmr_t5_standard_tick();
                ui_timeout = 10;
                comm_redraw_hook();
                return 0;
            }
            tmr_t5_standard_tick();
            comm_load_rx_pictures();
            ir_store_received_letter(0);
            pet_add_happiness(2);
            friend_add_after_exchange();
            comm_load_visitor_pictures(1);
        } else if (ir_tx_packet.kind == 8) {
            if (ir_rx_packet.kind != 9) {
                ui_timeout = 10;
                comm_redraw_hook();
                return 0;
            }
            tmr_t5_standard_tick();
            if (comm_exchange_kind == 4) {
                comm_gift_received();
                comm_gift_bonus();
                item_load_pictures((void *)anim_pic_slots, (void *)0x27D5414, 3);
                comm_item_exchange_result(comm_exchange_kind, (void *)0x27C5428, (void *)0x27D5400);
                ir_xfer_failed = ir_store_received_message(1, 0x27D5400);
            } else if (comm_exchange_kind == 5) {
                ir_stamp_peers_confirm();
                item_load_pictures((void *)anim_pic_slots, (void *)0x27D5414, 3);
                comm_item_exchange_result(comm_exchange_kind, (void *)0x27C5428, (void *)0x27D5400);
                ir_xfer_failed = ir_store_received_message(1, 0x27D5400);
            }
            snd_wait_end();
            pet_add_happiness(1);
            snd_wait_end();
            friend_add_after_exchange();
            comm_load_visitor_pictures(1);
            save_with_short_sound();
            ui_timeout = 10;
            comm_redraw_hook();
            return 1;
        } else if (ir_tx_packet.kind == 12) {
            if (ir_rx_packet.kind != 13) {
                ir_session_leave();
                return 0;
            }
            if (ir_rx_packet.arg_lo == 1) {
                item_load_pictures((void *)anim_pic_slots, (void *)0x27A5414, 3);
                comm_record_result();
            } else {
                item_load_pictures((void *)anim_pic_slots, (void *)0x27A5414, 3);
                ir_partner_hook_a(ir_rx_packet.char_byte & 0x3f);
            }
            ui_timeout = 0;
            comm_redraw_hook();
            return 1;
        } else if (ir_tx_packet.kind == 10 && ir_rx_packet.kind != 11) {
            comm_redraw_hook();
            return 0;
        } else if (ir_tx_packet.kind == 10 && ir_rx_packet.kind == 11) {
            if (ir_rx_packet.arg_lo == 0) {
                ir_session_leave();
                ir_show_proposal_refusal(1);
                ui_timeout = 10;
                return 1;
            }
            if ((ir_rx_packet.owner_id_hi & 0xc0) != 0xc0 && comm_exchange_kind == 7) {
                ir_session_flags |= 2;
                comm_gift_item = ir_rx_packet.result & 0x7f;
                ir_session_leave();
                return 1;
            }
            if ((ir_rx_packet.owner_id_hi & 0xc0) == 0xc0 && comm_exchange_kind != 7) {
                ir_session_flags |= 1;
                tmr_t5_pause_restore(30);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27D5414, 3);
                anim_proposal();
                if (ir_rx_packet.flags & 0x1c)
                    comm_show_rx_message();
                else
                    comm_show_proposal_line(ir_rx_packet.char_byte & 0x3f);
                ir_session_leave();
                return 1;
            }
            tmr_t5_standard_tick();
            if ((s8)ir_rx_packet.result < 0) {
                ir_anim_pause_a(ir_rx_packet.result & 0x7f);
                comm_result_pre_hook();
            } else {
                tmr_t5_pause_restore(30);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27D5414, 3);
                comm_result_anim();
            }
            comm_result_anim_hook(comm_game_result);
            pet_start_new_egg(comm_game_result);
            ui_timeout = 0;
            comm_redraw_hook();
            return 1;
        } else if (ir_tx_packet.kind == 2) {
            if (ir_rx_packet.kind != 3) {
                ir_session_leave();
                return 0;
            }
            f_2500A9C(ir_rx_packet.owner_id_hi & 0x40);
            friend_add_after_exchange();
            comm_load_visitor_pictures(1);
        } else if (ir_rx_packet.kind != 5) {
            ir_session_leave();
            return 0;
        } else {
            tmr_t5_pause_restore(5);
            item_load_pictures((void *)anim_pic_slots, (void *)0x27D5414, 3);
            if (ir_rx_packet.role != ir_tx_packet.role)
                goto mm;
            switch (ir_rx_packet.role) {
            case 4:
                visit_reward_pic_idx = 1;
                pet_add_happiness(2);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27CB814, 3);
                anim_present_box(visit_reward_pics[visit_reward_pic_idx].pic_a);
                break;
            case 3:
                visit_reward_pic_idx = 0;
                pet_add_happiness(2);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27CB814, 3);
                anim_present_box(visit_reward_pics[visit_reward_pic_idx].pic_a);
                break;
            case 2:
                visit_reward_pic_idx = 2;
                pet_sub_happiness(1);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27CB814, 3);
                anim_present_box(visit_reward_pics[visit_reward_pic_idx].pic_a);
                break;
            case 1:
                visit_reward_pic_idx = 4;
                pet_sub_happiness(1);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27CB814, 3);
                anim_present_box(visit_reward_pics[visit_reward_pic_idx].pic_a);
                break;
            case 0:
                visit_reward_pic_idx = 3;
                pet_sub_happiness(1);
                item_load_pictures((void *)anim_pic_slots, (void *)0x27CB814, 3);
                anim_present_box(visit_reward_pics[visit_reward_pic_idx].pic_a);
                break;
            default:
                if (save.pet.x000 <= 5000) {
                    x = chara_table[save.pet.chara_row].x022;
                    y = chara_table[save.pet.chara_row].x024;
                } else {
                    s = dl_chara_record(save.pet.chara_row + 0x77);
                    x = s->x020;
                    y = s->x022;
                }
                p = item_builtin_find(x, y);
                if ((int)p == -1) {
                    visit_reward_pic_idx = 0;
                    pet_add_happiness(2);
                    item_load_pictures((void *)anim_pic_slots, (void *)0x27CB814, 3);
                    anim_present_box(visit_reward_pics[visit_reward_pic_idx].pic_a);
                    comm_redraw_hook();
                    item_add_to_inventory((struct TamagoFile *)item_builtin_records[visit_reward_pic_idx]);
                } else {
                    pet_add_happiness(2);
                    item_load_pictures((void *)anim_pic_slots, (void *)0x27CB814, 3);
                    item_load_pictures((void *)&anim_pic_slots[3][1], p, 1);
                    anim_present_box(anim_pic_slots[3][1]);
                    comm_redraw_hook();
                    item_add_to_inventory(p);
                }
                break;
            }
            friend_add_after_exchange();
            comm_load_visitor_pictures(1);
        }
        save_game();
        ui_timeout = 10;
        comm_redraw_hook();
        return 1;
    }
r3:
    ir_session_cleanup();
    return 3;
aa:
    ir_transfer_abort();
    comm_redraw_hook();
    snd_play(3);
    snd_wait_end();
    visit_end();
    tmr_t5_standard_tick();
    ui_timeout = 10;
    return 2;
mm:
    comm_redraw_hook();
    ui_timeout = 10;
    return 2;
}

/*
 * 16-bit additive checksum of the n bytes at p into *out (n must be > 255; else 7).
 */
int ir_record_sum(u8 *p, u32 n, u16 *out)
{
    u16 s;
    u32 i;
    u8 c;

    if (n <= 255)
        return 7;
    s = 0;
    for (i = 0; i < n; i++) {
        c = *p++;
        s += c;
    }
    *out = s;
    return 0;
}

/*
 * Validate a received TAMAGO record p: the big-endian u32 at 0x48 (total size) must be
 * > 255 and <= 0x27D8 (mode 0: items) or <= 0xD3D8 (mode 1: the big app/profile records);
 * the additive checksum of all bytes but the last two must equal expect (the last two
 * bytes of the transfer). 0 ok, 1 bad size, 7 bad checksum. The stored item files hold the
 * size big-endian at 0x48 as well (e.g. 00 00 05 A0).
 */
int ir_validate_record(u8 mode, u8 *p, int unused, u16 expect)
{
    u32 n;
    u16 sum;
    u32 i;
    u8 *base;

    base = p;
    p += 0x48;
    n = *p++ << 24;
    n |= *p++ << 16;
    n |= *p << 8;
    n |= p[1];
    if (n <= 0xff)
        return 1;
    if (mode == 0) {
        if (n > 0x27d8)
            return 1;
    } else {
        if (n > 0xd3d8)
            return 1;
    }
    p = base;
    sum = 0;
    for (i = 0; i < n - 2; i++) {
        u8 c = *p++;
        sum += c;
    }
    if (sum != expect)
        return 7;
    return 0;
}

/* 10 00 30 00 again, then D_25381E0..: the same block starts comm/comm_flash_buffers's .rodata (after its
 * marker, 0x253B790): items of a header both files include, not a second marker */
static const unsigned short D_25381DC[2] = { 16, 48 };
const u8 D_25381E0[5] = { 1, 0, 2, 4, 3 };
const u16 D_25381E8[4][2] = {
    { 7, 300 },
    { 13, 300 },
    { 20, 300 },
    { 255, 65535 },
};
const u8 D_25381F8[12] = { 20, 40, 50, 50, 40, 30, 30, 30, 40, 30, 20, 20 };
/* a JPEG header (SOI, APP0 "JFIF", a cut DQT, EOI) */
const u8 D_2538204[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x12, 0x0c, 0x0d, 0x10, 0x0d, 0x0b, 0x12,
    0xe2, 0x80, 0x3f, 0xff, 0xd9,
};
/* TAMAGO files (cf. test_sample_tamago) */
const u8 D_253822C[] = {
    0x54, 0x41, 0x4d, 0x41, 0x47, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x51, 0x61, 0x80, 0x00, 0x5a, 0x56, 0x86, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};
const u8 D_2538430[] = {
    0x54, 0x41, 0x4d, 0x41, 0x47, 0x4f, 0x00, 0x44, 0x00, 0x4c, 0x00, 0x5f, 0x00, 0x4d, 0x00, 0x44,
    0x00, 0x50, 0x00, 0x5f, 0x30, 0xcf, 0x30, 0xf3, 0x30, 0xd0, 0x30, 0xfc, 0x30, 0xac, 0x30, 0xfc,
    0x00, 0x2e, 0x00, 0x6a, 0x00, 0x70, 0x00, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0a, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xc8, 0xcd, 0x80, 0x01, 0x01,
    0x01, 0x01, 0x7c, 0x68, 0x48, 0x97, 0x75, 0xa6, 0x4e, 0x99, 0x90, 0xa4, 0xbd, 0x51, 0xae, 0x51,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x64, 0x40, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x03, 0x01, 0x44, 0x18, 0x18, 0x0f, 0x00, 0x01, 0xff, 0x07, 0xe0, 0x60, 0x00, 0x14, 0x5c,
    0x1c, 0xbc, 0x25, 0x3d, 0x25, 0x1d, 0x0a, 0xd3, 0x01, 0xfb, 0x02, 0x71, 0x01, 0xed, 0x0e, 0x11,
    0x16, 0x93, 0x15, 0xdd, 0x02, 0x5f, 0x35, 0xbd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x11, 0x21, 0x22, 0x22, 0x12, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x11, 0x00, 0x00, 0x00, 0x10, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x01, 0x00, 0x00, 0x21, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x12, 0x00,
    0x10, 0x22, 0x22, 0x22, 0x32, 0x33, 0x33, 0x23, 0x22, 0x22, 0x22, 0x01, 0x10, 0x22, 0x22, 0x32,
    0x43, 0x44, 0x44, 0x34, 0x23, 0x22, 0x22, 0x01, 0x10, 0x25, 0x22, 0x22, 0x33, 0x44, 0x44, 0x33,
    0x22, 0x22, 0x52, 0x01, 0x00, 0x51, 0x25, 0x22, 0x22, 0x33, 0x33, 0x22, 0x22, 0x52, 0x15, 0x00,
    0x10, 0x16, 0x51, 0x55, 0x22, 0x22, 0x22, 0x22, 0x55, 0x15, 0x61, 0x01, 0x10, 0x76, 0x17, 0x11,
    0x55, 0x55, 0x55, 0x55, 0x11, 0x71, 0x87, 0x01, 0x10, 0x61, 0x96, 0xba, 0x11, 0x11, 0x11, 0x11,
    0x7c, 0xd7, 0x18, 0x01, 0x10, 0x1e, 0x61, 0xb6, 0xbb, 0x9a, 0x99, 0xcc, 0x66, 0x18, 0xe1, 0x01,
    0x10, 0xee, 0x1e, 0x11, 0x68, 0x66, 0x68, 0x86, 0x11, 0xe1, 0xee, 0x01, 0x10, 0xe2, 0xee, 0xee,
    0x11, 0x11, 0x11, 0x11, 0xee, 0xee, 0x2e, 0x01, 0x00, 0x21, 0xe2, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0x2e, 0x12, 0x00, 0x00, 0x10, 0x21, 0x22, 0xe2, 0xee, 0xee, 0x2e, 0x22, 0x12, 0x01, 0x00,
    0x00, 0x00, 0x10, 0x11, 0x22, 0x22, 0x22, 0x22, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0x18, 0x18, 0x0d, 0x00, 0x01, 0xff,
    0x07, 0xe0, 0x60, 0x00, 0x14, 0x5c, 0x35, 0xbd, 0x1c, 0xbc, 0x25, 0x3d, 0x25, 0x1d, 0x01, 0xfb,
    0x01, 0xed, 0x0a, 0xd3, 0x0e, 0x11, 0x02, 0x71, 0x16, 0x93, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x21, 0x22, 0x22, 0x12, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x11, 0x22, 0x22, 0x22, 0x22, 0x22, 0x11, 0x00, 0x00, 0x00, 0x00, 0x10, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x11, 0x33, 0x01, 0x00, 0x00, 0x00, 0x21, 0x22, 0x22, 0x22, 0x22, 0x12, 0x33, 0x33, 0x01,
    0x00, 0x00, 0x10, 0x22, 0x22, 0x22, 0x42, 0x44, 0x31, 0x33, 0x33, 0x01, 0x00, 0x00, 0x10, 0x22,
    0x22, 0x42, 0x54, 0x55, 0x31, 0x33, 0x66, 0x01, 0x00, 0x00, 0x10, 0x26, 0x22, 0x22, 0x44, 0x15,
    0x33, 0x66, 0x11, 0x00, 0x00, 0x00, 0x00, 0x61, 0x26, 0x22, 0x22, 0x14, 0x63, 0x11, 0x87, 0x01,
    0x00, 0x00, 0x10, 0x19, 0x61, 0x66, 0x22, 0x12, 0x16, 0x77, 0x88, 0x01, 0x00, 0x00, 0x10, 0x79,
    0x17, 0x11, 0x66, 0x16, 0xa1, 0x88, 0x9b, 0x01, 0x00, 0x00, 0x10, 0x91, 0x89, 0xca, 0x11, 0x11,
    0x8a, 0xb9, 0x11, 0x01, 0x00, 0x00, 0x10, 0x13, 0x91, 0xc9, 0xcc, 0x1a, 0xb8, 0x11, 0x33, 0x01,
    0x00, 0x00, 0x10, 0x33, 0x13, 0x11, 0x9b, 0x19, 0x1b, 0x33, 0x33, 0x01, 0x00, 0x00, 0x10, 0x32,
    0x33, 0x33, 0x11, 0x11, 0x31, 0x33, 0x22, 0x01, 0x00, 0x00, 0x00, 0x21, 0x32, 0x33, 0x33, 0x13,
    0x33, 0x22, 0x11, 0x00, 0x00, 0x00, 0x00, 0x10, 0x21, 0x22, 0x32, 0x33, 0x21, 0x11, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x22, 0x22, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x11, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3a, 0x18, 0x18, 0x0a, 0x00,
    0x01, 0xff, 0x07, 0xe0, 0x60, 0x00, 0x14, 0x5c, 0x35, 0xbd, 0x25, 0x1d, 0x01, 0xfb, 0x01, 0xed,
    0x0e, 0x11, 0x02, 0x71, 0x0a, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x22, 0x11, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x22, 0x12, 0x33, 0x13, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x21, 0x22, 0x31, 0x33, 0x44, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x22,
    0x22, 0x31, 0x44, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x22, 0x12, 0x43, 0x11, 0x55,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x24, 0x12, 0x14, 0x55, 0x66, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x41, 0x14, 0x71, 0x66, 0x98, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x19,
    0x11, 0x67, 0x89, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x59, 0x15, 0x96, 0x11, 0x33,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x91, 0x19, 0x19, 0x33, 0x33, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x13, 0x11, 0x31, 0x33, 0x22, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x33,
    0x13, 0x33, 0x22, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x32, 0x13, 0x23, 0x11, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x47,
};
const u8 D_25388F8[] = _("スタンプカードを");
const u8 D_253890C[] = _("もっていないよ!");
const u8 D_2538920[] = _("このともだちからは");
const u8 D_2538934[] = _("スタンプをおして");
const u8 D_2538948[] = _("もらえないよ!");

/*
 * Render our pictures of section kind k into the record p: kinds 0 / 1 the pet's pose set
 * (chara_render_pic_set), 5 the mirrored set (chara_render_pic_set_flip), 6 / 7 the pose
 * set of a male pet (chara_render_pose_set). Nothing is validated.
 */
int ir_render_section(int k, struct PictureSetHdr *p)
{
    int r = 0;
    u8 f;

    switch (k) {
    case 0:
    case 1:
        r = chara_render_pic_set(0, p);
        break;
    case 5:
        r = chara_render_pic_set_flip(3, (int)p, 4);
        break;
    case 6:
    case 7:
        if (save.pet.x000 <= 5000)
            f = chara_table[save.pet.x000].gender;
        else
            f = dl_chara_record(save.pet.chara_row + 0x77)->gender;
        if (f == 0)
            r = chara_render_pose_set(4, p);
        break;
    }
    return r;
}

/*
 * ir_transfer_abort, then ir_status_word_write(ir_rx_status).
 */
void ir_abort_and_store_status(void)
{
    ir_transfer_abort();
    ir_status_word_write(ir_rx_status);
}


/*
 * Unreferenced IR session entry (no call, no table, no word in the image): answer-only
 * receive of a record into 0x27C1000 (ir_receive_record_step, which uses the stub transfer
 * ir_transfer), filed by item_receive. Dead.
 */
int ir_session_receive_only(void)
{
    int k;
    u8 c;

    comm_exchange_kind = 0;
    ir_rx_record = (struct TamagoFile *)D_27C1000;
    ir_edge_count = 0;
    ir_edge_count2 = 0;
    sys_clock_osc3();
    snd_wait_end();
retry:
    btn_disable();
    btn_history_init();
    ui_new_screen_with_picture(0, D_21091E4);
    ir_rx_start();
    memset(&ir_tx_packet, 0, 0x14);
    memset(&ir_rx_packet, 0, 0x14);
    tmr_t5_start_raw(0x2d, 0xf);
    ui_new_screen_with_picture(0, D_21091E4);
    ui_timeout = 10;
    btn_history_init();
    do {
        btn_history_sample();
        if (ir_edge_count != 0) {
            k = ir_receive_record_step();
            tmr_t5_standard_tick();
            switch (k) {
            case 1:
                goto ok;
            case 3:
                goto ok2;
            case 0:
                goto c0;
            case 5:
                goto c5;
            default:
                return 0;
            }
        }
        if (btn_samples[2] & 1) {
            snd_play(3);
            snd_wait_end();
            btn_enable();
            return 0;
        }
        tmr_t5_ticks = 0;
        while (tmr_t5_ticks == 0)
            asm volatile("halt");
    } while (ui_timeout != 0);
ok:
    btn_enable();
    return 1;
ok2:
    btn_enable();
    return 2;
c0:
    ui_new_screen_with_picture(2, D_21091E4);
    btn_enable();
w0:
    btn_wait_press_or_timeout(10);
    if ((c = btn_a_presses) != 0)
        goto w0;
    if (btn_b_presses != 0) {
        btn_b_presses = c;
        goto retry;
    }
    if (ui_timeout == 0)
        return 1;
    ui_timeout = 10;
    return 0;
c5:
    ui_new_screen_with_picture(2, D_21091E4);
    btn_enable();
    btn_wait_press_or_timeout(5);
    if (btn_c_presses != 0)
        snd_play(3);
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_208DB9C);
    func_00009A6();
    btn_enable();
    btn_wait_press_or_timeout(5);
    ui_timeout = 10;
    return 1;
}

/*
 * IR exchange step with retries for ir_session_receive_only (legacy, dead): receive a record through
 * the stub transfer ir_transfer into gfx_frame_buffer, check it (ir_rx_store_block, ir_check_rx_record).
 */
u8 ir_receive_record_step(void)
{
    u8 st;
    s8 sub;
    u8 retry;
    int r;

    retry = 0;
again:
    btn_history_sample();
    ui_new_screen_with_picture(1, D_21091E4);
    ir_xfer_started = 0;
    ir_test_mode = 0;
    ir_xfer_enter();
    ir_block_kind = 0;
    ir_rx_status = 0;
    ir_rx_buf_count = 0;
    ir_rx_hunting = 1;
    ir_xfer_failed = 0;
    ir_session_busy = 1;
    ir_rx_buf_ptr = (u32)&gfx_frame_buffer;
    for (;;) {
        ir_transfer();
        if (st == 0x20 && sub == 0) {
            if (ir_rx_buf_count != 0) {
                ir_xfer_failed = ir_rx_store_block((u16 *)&gfx_frame_buffer, ir_rx_buf_count + 1);
                if (ir_xfer_failed != 0) {
                    ir_rx_status = 1;
                    break;
                }
            }
            r = 0;
            goto set;
        }
        if (st == 0xaa)
            goto aa;
        if (retry <= 3) {
            retry++;
            goto again;
        }
        if (st == 0x10)
            continue;
        r = 1;
set:
        ir_xfer_failed = r;
        break;
    }
    ir_session_busy = 0;
    if (ir_xfer_failed == 1) {
        ir_session_cleanup();
        return 0;
    }
    if (ir_rx_hunting != 0)
        goto fail;
    ir_rx_record = (struct TamagoFile *)0x27C1000;
    if (ir_check_rx_record() == 0)
        goto ok;
fail:
    ir_session_cleanup();
    return 0;
aa:
    ir_transfer_abort();
    comm_redraw_hook();
    snd_play(3);
    snd_wait_end();
    tmr_t5_standard_tick();
    ui_timeout = 10;
    return 2;
ok:
    comm_redraw_hook();
    tmr_t5_standard_tick();
    item_receive((struct TamagoFile *)0x27C1000);
    ui_timeout = 10;
    return 1;
}

/*
 * Is record p a special built-in item (category 1/2 checks with the flag bits at
 * +0xF8/+0xF9)?
 */
int item_is_special_builtin(struct TamagoFile *p)
{
    switch (p->x04e) {
    case 1:
        if (p->x04f == 2 && ((p->code_hi >> 4) | ((p->code_lo & 0xc0) >> 2)) != 0x3c)
            return 1;
    case 2:
        if (p->sub_kind == 1 && p->legacy_value == 1 && ((p->code_hi >> 4) | ((p->code_lo & 0xc0) >> 2)) != 0x3c)
            return 1;
        break;
    }
    return 0;
}

/*
 * Is record p of category 0x14/2 (special)?
 */
int item_is_special_category(struct TamagoFile *p)
{
    u8 k;

    if (p->x04e != 0x14)
        goto zero;
    if (p->x04f != 2)
        goto zero;
    if (p->legacy_text == 0x37) {
        k = p->x05b;
        if (k == 2 || k == 0xb)
            return 1;
        return 0;
    }
    if (p->x05b == 5)
        return 1;
zero:
    return 0;
}

/*
 * Check the received record at ir_rx_record: starts with the text D_253898C, byte 0x4C low
 * nibble 0xD, and (unless ir_test_mode = test) the 6-bit code (x04c >> 4 | (x04d & 0xC0) >> 2)
 * above 0x2A or ir_check_rx_version accepts it; then the checksum ir_check_rx_sum. 0 = ok. The same
 * packed "0xD + 6-bit code" sits in bytes 0/2 of the IR packet.
 */
u8 ir_check_rx_record(void)
{
    u8 buf[10];
    int n;

    memset(buf, 0, 10);
    strcpy(buf, D_253898C);
    n = strlen(buf);
    if (strncmp(ir_rx_record, buf, n) != 0)
        return 1;
    if ((ir_rx_record->x04c_b & 0xf) != 0xd)
        return 1;
    if (ir_test_mode != 1) {
        if (ir_block_kind == 0) {
            if (ir_check_rx_version() == 7)
                return 1;
        } else {
            if (((ir_rx_record->x04c_b >> 4) | ((ir_rx_record->x04d & 0xc0) >> 2)) <= 0x2a)
                return 1;
        }
    }
    if (ir_check_rx_sum())
        return 1;
    return 0;
}

/*
 * Verify the checksum of the record at ir_rx_record.
 */
int ir_check_rx_sum(void)
{
    u8 *p, *q;
    u32 n;
    u16 sum;
    u32 i;

    p = (u8 *)ir_rx_record;
    q = p + 0x48;
    n = *q++ << 24;
    n |= *q++ << 16;
    n |= *q << 8;
    n |= q[1];
    if (n <= 0xff)
        return 1;
    q = (u8 *)ir_rx_record;
    sum = 0;
    for (i = 0; i < n - 2; i++) {
        u8 c = *q++;
        sum += c;
    }
    i = (u16)(q[0] << 8);
    if (sum != (q[1] | i))
        return 1;
    return 0;
}

/*
 * Cancel sound + wait + btn_enable.
 */
void ir_cancel_sound(void)
{
    snd_play(3);
    snd_wait_end();
    btn_enable();
}

/*
 * Apply the IR point gift result: k = comm_submenu_choice == 0 (giver) pays v points (pet_spend_points)
 * and gains pet_add_happiness(5/10/15) for >= 100/500/1000; k == 1 (receiver) gets v points
 * (pet_add_points, anim_points_gain, visit_load_partner_pics(1), anim_happy). Then comm_load_visitor_pictures(1).
 */
void ir_apply_point_gift(u8 k, int v)
{
    tmr_t5_standard_tick();
    if (k == 0) {
        pet_spend_points(v);
        if (v > 999)
            pet_add_happiness(15);
        else if (v > 499)
            pet_add_happiness(10);
        else if (v > 99)
            pet_add_happiness(5);
    } else {
        pet_add_points(v);
        anim_points_gain(v);
        visit_load_partner_pics(1);
        anim_happy();
    }
    comm_load_visitor_pictures(1);
}

/*
 * IR point gift at the packet level (legacy, dead: only from ir_point_gift_session). Hello: the
 * responder (comm_is_initiator == 0) receives the initiator's kind-0 packet (up to 5 tries) and
 * answers kind 1; the initiator sends kind 0 and waits for kind 1 (31 ms between tries).
 * Our packet: byte 0 = 0x8D (low nibble 0xD = signature, high nibble + bits 6-7 of byte 2
 * = the 6-bit code 0x38), byte 2 = ir_owner_id_lo | 0xC0, byte 3 = 0x3D/0x3F, byte 4 =
 * ir_owner_id_hi, bytes 5.. = the owner name pet.owner_name (16 bytes copied, so it overwrites the
 * 0x38 at byte 16, the checksum byte and one byte past the packet). A peer is accepted with
 * byte 0 low nibble 0xD and, when P06 reads high, byte 16 != 0x38 and byte 17 == 1; it is
 * recorded as a friend (friend_find_or_add). Then the gift: the initiator sends kind 0x70 with
 * x00b = 1 (giver, x00c/x00d = amount a, little-endian) or 2 (receiver), the responder
 * checks the complementary role and answers 0x71; both apply ir_apply_point_gift and save.
 * Returns 1 ok, 0 failed, 2 cancelled (C).
 */
u8 ir_point_gift_packets(int a)
{
    struct IrPacket *p = &ir_rx_packet;
    u8 retry;
    u8 name[16];

    ir_exchange_flag = 0;
    retry = 0;
    if (comm_is_initiator == 0) {
    again1:
        btn_history_sample();
        tmr_t5_start_raw(0x2e5, 8);
        if (ir_recv_packet((int)&ir_rx_packet) == 0x14)
            goto ok1;
        if (btn_samples[2] & 1) {
            ir_cancel_sound();
            return 2;
        }
        memset(&ir_rx_packet, 0, 0x14);
        retry++;
        if (retry > 4)
            return 0;
        ir_rx_start();
        ir_xfer_started = 1;
        goto again1;
    ok1:
        ui_new_screen_with_picture(1, D_21091E4);
        if (ir_rx_packet.kind != 0)
            return 0;
        if (P0_P0D & 0x40) {
            if (ir_rx_packet.points_hi == 0x38)
                return 0;
            if (ir_rx_packet.points_lo != 1)
                return 0;
        }
        ir_peer_has_code = 0;
        if ((ir_rx_packet.sig_code & 0xf) != 0xd)
            return 0;
        if (((ir_rx_packet.sig_code >> 4) | ((ir_rx_packet.owner_id_lo & 0xc0) >> 2)) > 0)
            ir_peer_has_code = 1;
        memset(&ir_tx_packet, 0, 0x14);
        ir_tx_packet.kind = 1;
        ir_tx_packet.sig_code = 0x8d;
        ir_tx_packet.owner_id_lo = ir_owner_id_lo | 0xc0;
        if (pet_get_gender() == 0)
            ir_tx_packet.char_byte = 0x3d;
        else
            ir_tx_packet.char_byte = 0x3f;
        ir_tx_packet.owner_id_hi = ir_owner_id_hi & 0x3f;
        ir_tx_packet.points_hi = 0x38;
        ir_tx_put_owner_name();
        comm_partner_slot = friend_find_or_add(ir_rx_packet.owner_id_lo & 0x3f, ir_rx_packet.owner_id_hi & 0x3f, ir_rx_name16(name));
        if (!(ir_rx_packet.char_byte & 0x40))
            save.friend_list[comm_partner_slot].gender = 0x3d;
        else
            save.friend_list[comm_partner_slot].gender = 0x3f;
        ir_send_packet((int)&ir_tx_packet);
    } else {
        memset(&ir_tx_packet, 0, 0x14);
        ir_tx_packet.kind = 0;
        ir_tx_packet.sig_code = 0x8d;
        ir_tx_packet.owner_id_lo = ir_owner_id_lo | 0xc0;
        if (pet_get_gender() == 0)
            ir_tx_packet.char_byte = 0x3d;
        else
            ir_tx_packet.char_byte = 0x3f;
        ir_tx_packet.owner_id_hi = ir_owner_id_hi & 0x3f;
        ir_tx_packet.points_hi = 0x38;
        ir_tx_put_owner_name();
    again2:
        memset(&ir_rx_packet, 0, 0x14);
        ir_send_packet((int)&ir_tx_packet);
        btn_history_sample();
        tmr_t5_start_raw(0x2e5, 8);
        if (ir_recv_packet((int)&ir_rx_packet) == 0x14)
            goto ok2;
        if (btn_samples[2] & 1) {
            ir_cancel_sound();
            return 2;
        }
        retry++;
        if (retry > 4)
            return 0;
        tmr_t5_pause(1);
        goto again2;
    ok2:
        if (ir_rx_packet.kind != 1)
            return 0;
        if (P0_P0D & 0x40) {
            if (ir_rx_packet.points_hi == 0x38)
                return 0;
            if (ir_rx_packet.points_lo != 1)
                return 0;
        }
        ir_peer_has_code = 0;
        if ((ir_rx_packet.sig_code & 0xf) != 0xd)
            return 0;
        if (((ir_rx_packet.sig_code >> 4) | ((ir_rx_packet.owner_id_lo & 0xc0) >> 2)) > 0)
            ir_peer_has_code = 1;
        comm_partner_slot = friend_find_or_add(ir_rx_packet.owner_id_lo & 0x3f, ir_rx_packet.owner_id_hi & 0x3f, ir_rx_name16(name));
        if (!(ir_rx_packet.char_byte & 0x40))
            save.friend_list[comm_partner_slot].gender = 0x3d;
        else
            save.friend_list[comm_partner_slot].gender = 0x3f;
    }
    if (comm_is_initiator == 1) {
        ir_tx_packet.kind = 0x70;
        if (comm_submenu_choice == 0) {
            ir_tx_packet.role = 1;
            ir_tx_packet.arg_lo = a;
            ir_tx_packet.arg_hi = a >> 8;
        } else {
            ir_tx_packet.role = 2;
            ir_tx_packet.arg_lo = 0;
            ir_tx_packet.arg_hi = 0;
        }
        ir_tx_packet.points_hi = 0;
        tmr_t5_pause(1);
        ir_send_packet((int)&ir_tx_packet);
        tmr_t5_start_raw(0x2e5, 8);
        if (ir_recv_packet((int)&ir_rx_packet) != 0x14)
            return 0;
        if (ir_rx_packet.kind != 0x71)
            return 0;
        if (ir_rx_packet.role == 0)
            return 0;
        if (comm_submenu_choice == 0) {
            if (ir_rx_packet.role != 2)
                return 0;
        } else {
            if (ir_rx_packet.role != 1)
                return 0;
            a = *(u16 *)&p->arg_lo;
        }
        ir_apply_point_gift(comm_submenu_choice, a);
        comm_write_default_partner_image();
        comm_exchange_kind = 0;
    } else {
        tmr_t5_start_raw(0x2e5, 8);
        if (ir_recv_packet((int)&ir_rx_packet) != 0x14)
            return 0;
        if (ir_rx_packet.kind != 0x70)
            return 0;
        if (ir_rx_packet.role == 0)
            return 0;
        if (comm_submenu_choice == 0) {
            if (ir_rx_packet.role != 2)
                return 0;
        } else {
            if (ir_rx_packet.role != 1)
                return 0;
        }
        ir_tx_packet.kind = 0x71;
        if (comm_submenu_choice == 0) {
            ir_tx_packet.role = 1;
            ir_tx_packet.arg_lo = a;
            ir_tx_packet.arg_hi = a >> 8;
        } else {
            ir_tx_packet.role = 2;
            ir_tx_packet.arg_lo = 0;
            ir_tx_packet.arg_hi = 0;
            a = *(u16 *)&p->arg_lo;
        }
        ir_tx_packet.points_hi = 0;
        tmr_t5_pause(1);
        ir_send_packet((int)&ir_tx_packet);
        ir_apply_point_gift(comm_submenu_choice, a);
        comm_write_default_partner_image();
        comm_exchange_kind = 0;
    }
    friend_add_after_exchange();
    save_game();
    ui_timeout = 10;
    return 1;
}

/*
 * IR point-gift session (legacy; reached only from ir_point_gift, i.e. the unreferenced
 * gift menu D_25380B0): same waiting loop as ir_session_run (6.3 ms tick, 10 s, B
 * initiates, C cancels) around ir_point_gift_packets(b = amount) (asm).
 */
int ir_point_gift_session(int a, int b)
{
    int k;
    u8 c;
    u8 two;

    comm_exchange_kind = a;
    sys_copy_ram_routines();
    ir_edge_count = 0;
    ir_edge_count2 = 0;
    sys_clock_osc3();
retry:
    btn_disable();
    btn_history_init();
    ui_new_screen_with_picture(0, (int)D_21091E4);
    ir_rx_start();
    memset(&ir_tx_packet, 0, 0x14);
    memset(&ir_rx_packet, 0, 0x14);
    tmr_t5_start_raw(0x2d, 0xf);
    comm_is_initiator = 0;
    ui_new_screen_with_picture(0, (int)D_21091E4);
    ui_timeout = 10;
    btn_history_init();
    two = 2;
    for (;;) {
        btn_history_sample();
        if (ir_edge_count == 0) {
            if ((btn_samples[1] & 3) != 1)
                goto wait;
            snd_play(2);
            ui_new_screen_with_picture(1, (int)D_21091E4);
            snd_wait_end();
            if (ir_edge_count != 0)
                goto wait;
            comm_is_initiator = 1;
        }
        k = ir_point_gift_packets(b);
        tmr_t5_standard_tick();
        switch (k) {
        case 1:
            ir_gift_menu_done = 2;
            btn_enable();
            return 1;
        case 3:
            goto c3;
        case 0:
            goto c0;
        default:
            return 0;
        }
    wait:
        if (btn_samples[2] & 1) {
            snd_play(3);
            snd_wait_end();
            btn_enable();
            return 0;
        }
        tmr_t5_ticks = 0;
        while (tmr_t5_ticks == 0)
            asm volatile("halt");
        if (ui_timeout == 0) {
            btn_enable();
            ir_gift_menu_done = 2;
            return 1;
        }
    }
c3:
    btn_enable();
    return 2;
c0:
    ui_new_screen_with_picture(2, (int)D_21091E4);
    btn_enable();
w0:
    btn_wait_press_or_timeout(10);
    if ((c = btn_a_presses) != 0)
        goto w0;
    if (btn_b_presses != 0) {
        btn_b_presses = c;
        goto retry;
    }
    if (ui_timeout == 0) {
        ir_gift_menu_done = 2;
        return 1;
    }
    ui_timeout = 10;
    return 0;
}

u8 f_25029FA(u8 **, u32 *, u8 *) __asm__("ir_send_data_stub_b");
/*
 * IR exchange for ir_test_session (legacy, dead): kinds 0xB2/0xB3 through the stub transfer
 * ir_transfer (the data to send comes from the stub ir_send_data_stub_b). 1 = ok.
 */
u8 ir_test_exchange(void)
{
    u8 c;
    u8 st;
    s8 sub;
    u8 *a;
    u32 b;
    u8 i;
    u8 t;

    ir_exchange_flag = 0;
    memset(&ir_tx_packet, 0, 20);
    memset(&ir_rx_packet, 0, 20);
    if (comm_is_initiator == 0) {
        btn_history_sample();
        ir_xfer_enter();
        ir_rx_status = 0;
        ir_rx_buf_count = 0;
        ir_rx_hunting = 1;
        ir_xfer_failed = 0;
        ir_session_busy = 1;
        ir_rx_buf_ptr = (u32)&gfx_frame_buffer;
        ir_block_kind = 2;
        ir_xfer_started = 0;
again1:
        ir_transfer();
        if (st == 0x20 && sub == 0)
            ir_xfer_failed = 0;
        else if (st == 0xaa)
            goto aa;
        else if (st == 0x10)
            goto again1;
        else
            ir_xfer_failed = 1;
        ir_session_busy = 0;
        if (ir_xfer_failed == 1)
            goto fail;
        t = ir_rx_hunting;
        if (t != 0)
            goto fail;
        t = ir_block_kind;
        if (t != 0x88)
            goto fail;
        comm_redraw_hook();
        tmr_t5_standard_tick();
        if (ir_rx_packet.kind == 0xb2)
            return 1;
        return 0;
    } else {
        if (f_25029FA(&a, &b, &c) == 7)
            return 0;
        ir_xfer_enter();
        ir_rx_status = 0;
        ir_rx_buf_count = 0;
        ir_rx_hunting = 1;
        ir_xfer_failed = 0;
        ir_session_busy = 1;
        ir_block_kind = 1;
        ir_tx_ptr = b;
        ir_send_data_ptr = (u32)a;
again2:
        ir_transfer();
        if (st == 0x20 && sub == 0)
            ir_xfer_failed = 0;
        else if (st == 0xaa)
            goto aa;
        else if (st == 0x10)
            goto again2;
        else
            ir_xfer_failed = 1;
        ir_session_busy = 0;
        if (ir_xfer_failed == 1)
            goto fail;
        t = ir_rx_hunting;
        if (t != 0)
            goto fail;
        t = ir_block_kind;
        if (t != 7)
            goto fail;
        comm_redraw_hook();
        tmr_t5_standard_tick();
        a = D_27CB800;
        ir_rx_record = (struct TamagoFile *)a;
        a += 0x200;
        for (i = 0; i < 20; i++)
            ((u8 *)&ir_rx_packet)[i] = *a++;
        if (ir_tx_packet.kind == 0xb2 && ir_rx_packet.kind != 0xb3)
            return 0;
        return 1;
    }
aa:
    ir_transfer_abort();
fail:
    comm_redraw_hook();
    tmr_t5_standard_tick();
    ui_timeout = 10;
    return 0;
}

/*
 * The old Tamagotchi-to-Tamagotchi exchange over IR (legacy state machine, application
 * level; built from asm). Both sides swap a 20-byte header (ours in ir_tx_packet, theirs in
 * ir_rx_packet; x001 = message type, paired request/answer codes: 2/3 greeting, 8/9 gift
 * (item follows), 10/11 play-together with ir_rx_packet.owner_id_hi bits 6-7 = "has the stamp card"
 * and x00e/x00f = result/item index (bit 7 = reject), 12/13 photo (x003 & 0x3f = pose,
 * x00c = which side), 14/15 proposal, 5 = marriage with x00b = the agreed game, 0x70 =
 * point gift: a peer 0x70 grants 1000 points here, as in ir_point_gift_packets; not a refusal) and
 * then the payload records that land in the save banks 0x27D5400 /
 * 0x27A5400 (profile + images at +0x14) and 0x27C5428 / 0x27CB814 (received items /
 * portrait). The transfer is ir_transfer (status 0x20 = ok, 0xAA = aborted by the user,
 * 0x10 = retry), with ir_transfer_abort (abort) and ir_send_data_stub (data to send): in this
 * firmware all three are stubs (`ret`, `ret`, `return 0`), so st/sub stay uninitialised
 * and the session fails, aborts or loops on "0x10" forever. comm_is_initiator == 0: the peer
 * pinged first and we answer (first branch); 1: we pressed B and initiated (second branch,
 * header loaded from the bank + 0x200). Outcomes: store received items
 * (item_add_to_inventory / comm_item_exchange_result / ir_store_received_message), record the friend (friend_add_after_exchange +
 * comm_load_visitor_pictures(1) = add to the たまとも list), unlock mini games (pet_add_happiness), apply the
 * partner record (comm_apply_partner_record...), save (save_game). Return 0 = failed, 1 = ok, 2 =
 * cancelled, 3 = partner left. ir_block_kind/D14/D28/D4C are the transfer-state bytes shared
 * with ir_transfer. (Confidence medium on the message-type names.)
 */

/*
 * Verify the checksum of the record at ir_rx_record.
 */


/*
 * IR test session (legacy, dead: only from ir_test_entry): status pictures D_21091E4
 * frames 0/1/2 on a cleared screen, ping or answer as in ir_session_run, exchange
 * ir_test_exchange; on success show the peer's 6-bit code (byte 0 >> 4 | (byte 2 & 0xC0) >> 2)
 * as two digits with sound 0x19; A = again, B = done (asm).
 */
int ir_test_session(int a)
{
    volatile u8 *p1420;
    u8 k;
    u32 n;
    volatile u8 *port;

    comm_exchange_kind = a;
    sys_copy_ram_routines();
    ir_edge_count = 0;
    ir_edge_count2 = 0;
    sys_clock_osc3();
retry:
    btn_disable();
    btn_history_init();
    ir_rx_start();
    memset(&ir_tx_packet, 0, 0x14);
    memset(&ir_rx_packet, 0, 0x14);
    tmr_t5_start_raw(0x2d, 0xf);
    gfx_display_reset();
    gfx_set_background(0xf800);
    func_00009A6();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_21091E4);
    func_00009A6();
    comm_is_initiator = 0;
    ui_timeout = 10;
    btn_history_init();
    do {
        btn_history_sample();
        if (ir_edge_count != 0) {
            gfx_display_reset();
            gfx_set_background(0xf800);
            func_00009A6();
            gfx_object_create(0, 0x1c, 0, 1, 0, D_21091E4);
            func_00009A6();
            k = ir_test_exchange();
            tmr_t5_standard_tick();
            if (k == 0)
                goto c0;
            if (k != 1)
                return 0;
            goto c1;
        }
        if ((btn_samples[1] & 3) == 1) {
            gfx_display_reset();
            gfx_set_background(0xf800);
            func_00009A6();
            gfx_object_create(0, 0x1c, 0, 1, 0, D_21091E4);
            func_00009A6();
            if (ir_edge_count == 0) {
                ir_send_ping((int)&ir_tx_packet);
                comm_is_initiator = 1;
                k = ir_test_exchange();
                tmr_t5_standard_tick();
                if (k == 0)
                    goto c0;
                if (k != 1)
                    return 0;
                goto c1;
            }
        }
        tmr_t5_ticks = 0;
        while (tmr_t5_ticks == 0)
            asm volatile("halt");
    } while (ui_timeout != 0);
    btn_enable();
    return 1;
c1:
    btn_enable();
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2087100);
    n = (ir_rx_packet.sig_code >> 4) | ((ir_rx_packet.owner_id_lo & 0xc0) >> 2);
    gfx_object_create(0x44, 0x38, 0x10, n / 10, 0, D_208EDB0);
    gfx_object_create(0x4d, 0x38, 0x10, n % 10, 0, D_208EDB0);
    func_00009A6();
    snd_enabled = 1;
    snd_play(0x19);
    snd_wait_end();
    snd_enabled = 0;
    test_clear_buttons();
    p1420 = &btn_a_presses;
    for (;;) {
        if (*p1420 != 0 && (P0_P0D & 6) == 6)
            goto retry;
        if (btn_b_presses != 0 && (P0_P0D & 5) == 5)
            goto fail;
        test_clear_buttons();
        asm volatile("halt");
        sys_idle_housekeeping();
    }
c0:
    gfx_display_reset();
    gfx_set_background(0xf800);
    func_00009A6();
    gfx_object_create(0, 0x1c, 0, 2, 0, D_21091E4);
    func_00009A6();
    btn_enable();
    test_clear_buttons();
    port = (volatile u8 *)0x300380;
wait0:
    if (btn_a_presses != 0) {
        if (!(*port & 5))
            goto fail;
    } else if (btn_b_presses != 0 && (*port & 5)) {
        btn_b_presses = 0;
        goto retry;
    }
    test_clear_buttons();
    asm volatile("halt");
    sys_idle_housekeeping();
    goto wait0;
fail:
    btn_enable();
    return 0;
}

/*
 * Empty function (stub, `ret` only).
 */
void func_24FB610(void)
{
}

void func_24FB612(void)
{
}

/*
 * Unreferenced IR test entry (no call, no table, no word in the image): with ir_test_mode = 1
 * (test mode: relaxed record checks), the test session ir_test_session(0), then the packet
 * test ir_test_packets. Dead.
 */
int ir_test_entry(void)
{
    int r;

    ir_test_mode = 1;
    ir_test_counter &= ~1;
    comm_erase_tx_sectors();
    ir_test_session(0);
    ir_test_counter |= 1;
    r = ir_test_packets();
    ir_power_down();
    ir_test_mode = 0;
    return r;
}

/*
 * Unreferenced IR test (no call, no table, no word in the image): packet-level hello only.
 * The responder receives kind 0 and answers kind 1, the initiator (B) sends kind 0 and
 * waits for kind 1 (5 tries each); our byte 0 = 0xCD (code 0x3C), byte 3 = character
 * flag << 6 | id bits, bytes 5.. = 16 bytes of pet + 0x70 (the owner name), byte 10
 * = ir_count_tier(x00c). Shows the peer's code as two digits; A = again, B = done (asm).
 */
int ir_test_hello(void)
{
    u8 i, j;
    int p, o;
    struct DlCharaRecord *s;
    u32 n;

    sys_copy_ram_routines();
    ir_edge_count = 0;
    ir_edge_count2 = 0;
    sys_clock_osc3();
    ir_rx_start();
    for (i = 0; i <= 0x13; i++) {
        ((u8 *)&ir_tx_packet)[i] = 0;
        ((u8 *)&ir_rx_packet)[i] = 0;
    }
retry:
    ir_rx_start();
    tmr_t5_start_raw(0x2d, 0xf);
    btn_disable();
    btn_history_init();
    gfx_display_reset();
    gfx_set_background(0xffff);
    func_00009A6();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x21091e4);
    func_00009A6();
    for (;;) {
        btn_history_sample();
        if (ir_edge_count != 0) {
            j = 0;
            do {
                gfx_display_reset();
                gfx_set_background(0xffff);
                func_00009A6();
                gfx_object_create(0, 0x1c, 0, 1, 0, 0x21091e4);
                func_00009A6();
                tmr_t5_start_raw(0x2e5, 8);
                if (ir_recv_packet((int)&ir_rx_packet) == 0x14)
                    goto got;
                j++;
            } while (j <= 4);
            goto fail;
        got:
            if (ir_rx_packet.kind != 0)
                goto fail;
            for (i = 0; i <= 0x13; i++)
                ((u8 *)&ir_tx_packet)[i] = 0;
            ir_tx_packet.sig_code = 0xcd;
            ir_tx_packet.kind = 1;
            ir_tx_packet.owner_id_lo = ir_owner_id_lo | 0xc0;
            if (save.pet.x000 <= 5000)
                ir_tx_packet.char_byte = (chara_table[save.pet.x000].gender << 6) | save.pet.x000_b;
            else {
                s = dl_chara_record(save.pet.chara_row + 0x77);
                ir_tx_packet.char_byte = (s->gender << 6) | save.pet.x000_b;
            }
            ir_tx_packet.owner_id_hi = ir_owner_id_hi & 0x3f;
            ir_tx_put_owner_name();
            ((u8 *)&ir_tx_packet)[10] = ir_count_tier(save.pet.happiness);
            tmr_t5_pause(1);
            ir_send_packet((int)&ir_tx_packet);
            goto ok;
        }
        if ((btn_samples[1] & 3) == 1) {
            gfx_display_reset();
            gfx_set_background(0xffff);
            func_00009A6();
            gfx_object_create(0, 0x1c, 0, 1, 0, 0x21091e4);
            func_00009A6();
            for (i = 0; i <= 0x13; i++)
                ((u8 *)&ir_tx_packet)[i] = 0;
            ir_tx_packet.sig_code = 0xcd;
            ir_tx_packet.kind = 0;
            ir_tx_packet.owner_id_lo = ir_owner_id_lo | 0xc0;
            if (save.pet.x000 <= 5000)
                ir_tx_packet.char_byte = (chara_table[save.pet.x000].gender << 6) | save.pet.x000_b;
            else {
                s = dl_chara_record(save.pet.chara_row + 0x77);
                ir_tx_packet.char_byte = (s->gender << 6) | save.pet.x000_b;
            }
            ir_tx_packet.owner_id_hi = ir_owner_id_hi & 0x3f;
            ir_tx_put_owner_name();
            ((u8 *)&ir_tx_packet)[10] = ir_count_tier(save.pet.happiness);
            j = 0;
            do {
                tmr_t5_pause(1);
                ir_send_packet((int)&ir_tx_packet);
                for (i = 0; i <= 0x13; i++)
                    ((u8 *)&ir_rx_packet)[i] = 0;
                tmr_t5_start_raw(0x2e5, 8);
                if (ir_recv_packet((int)&ir_rx_packet) == 0x14)
                    goto ok;
                j++;
            } while (j <= 4);
            goto fail;
        }
        tmr_t5_start_raw(0xf4, 0xf);
        tmr_t5_ticks = 0;
        if (tmr_t5_ticks == 0)
            asm volatile("halt");
    }
ok:
    btn_enable();
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2087100);
    n = (ir_rx_packet.sig_code >> 4) | ((ir_rx_packet.owner_id_lo & 0xc0) >> 2);
    gfx_object_create(0x44, 0x38, 0x10, n / 10, 0, 0x208edb0);
    gfx_object_create(0x4d, 0x38, 0x10, n % 10, 0, 0x208edb0);
    func_00009A6();
    snd_enabled = 1;
    snd_play(0x19);
    snd_wait_end();
    snd_enabled = 0;
    tmr_t5_start_raw(0xf4, 0xf);
    test_clear_buttons();
    for (;;) {
        if (btn_a_presses != 0)
            goto retry;
        if (btn_b_presses != 0)
            return 1;
        asm volatile("halt");
        sys_idle_housekeeping();
    }
fail:
    gfx_display_reset();
    gfx_set_background(0xffff);
    func_00009A6();
    gfx_object_create(0, 0x1c, 0, 2, 0, 0x21091e4);
    func_00009A6();
    tmr_t5_start_raw(0xf4, 0xf);
    btn_history_init();
    for (;;) {
        btn_history_sample();
        if (!(P0_P0D & 5))
            break;
        if ((btn_samples[1] & 3) == 1)
            goto retry;
        tmr_t5_ticks = 0;
        if (tmr_t5_ticks == 0)
            asm volatile("halt");
    }
    btn_enable();
    return 0;
}

/*
 * IR packet test (legacy, dead: only from ir_test_entry): the hello of ir_point_gift_packets (0x8D
 * packets, kinds 0/1), then a 0x70 / 0x71 exchange with x00b = 2 on both sides, retries
 * up to 5 / 3 times; on success shows the peer's 6-bit code as two digits with sound 0x19
 * (A = again, B = done). Returns 1 ok, 0 failed (asm).
 */
int ir_test_packets(void)
{
    struct IrPacketTx *p;
    int fails;
    u8 retry;
    u32 n;

    fails = 0;
again:
    sys_copy_ram_routines();
    ir_edge_count = 0;
    ir_edge_count2 = 0;
    sys_clock_osc3();
    ir_rx_start();
    memset(&ir_tx_packet, 0, 0x14);
    memset(&ir_rx_packet, 0, 0x14);
    ir_rx_start();
    tmr_t5_start_raw(0x2d, 0xf);
    btn_disable();
    btn_history_init();
    gfx_display_reset();
    gfx_set_background(0x7e0);
    func_00009A6();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x21091e4);
    func_00009A6();
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 1, 0, 0x21091e4);
    p = &ir_tx_packet;
    for (;;) {
    btn_history_sample();
    if (ir_edge_count != 0) {
        retry = 0;
    again1:
        tmr_t5_start_raw(0x2e5, 8);
        if (ir_recv_packet((int)&ir_rx_packet) == 0x14)
            goto ok1;
        if (btn_samples[2] & 1)
            goto cancel;
        memset(&ir_rx_packet, 0, 0x14);
        retry++;
        if (retry > 4)
            goto fail;
        ir_rx_start();
        ir_xfer_started = 1;
        goto again1;
    ok1:
        if (ir_rx_packet.kind != 0)
            goto fail;
        func_00009A6();
        memset(&ir_tx_packet, 0, 0x14);
        ir_tx_packet.kind = 1;
        ir_tx_packet.sig_code = 0x8d;
        ir_tx_packet.owner_id_lo = ir_owner_id_lo | 0xc0;
        if (pet_get_gender() == 0)
            ir_tx_packet.char_byte = 0x3d;
        else
            p->char_byte = 0x3f;
        p->owner_id_hi = ir_owner_id_hi & 0x3f;
        p->points_hi = 0x38;
        ir_tx_put_owner_name();
        ir_send_packet((int)p);
        tmr_t5_start_raw(0x2e5, 8);
        if (ir_recv_packet((int)&ir_rx_packet) != 0x14) {
            if (fails++ > 1)
                goto fail;
            memset(&ir_rx_packet, 0, 0x14);
            ir_rx_start();
            goto again1;
        }
        if (ir_rx_packet.kind != 0x70) {
            if (fails++ > 1)
                goto fail;
            memset(&ir_rx_packet, 0, 0x14);
            ir_rx_start();
            goto again1;
        }
        p->kind = 0x71;
        p->role = 2;
        p->arg_lo = 0;
        p->arg_hi = 0;
        p->points_hi = 0;
        tmr_t5_pause(1);
        ir_send_packet((int)p);
        goto done;
    }
    if ((btn_samples[1] & 3) == 1) {
        gfx_display_reset();
        gfx_object_create(0, 0x1c, 0, 1, 0, 0x21091e4);
        func_00009A6();
        memset(&ir_tx_packet, 0, 0x14);
        ir_tx_packet.kind = 0;
        ir_tx_packet.sig_code = 0x8d;
        ir_tx_packet.owner_id_lo = ir_owner_id_lo | 0xc0;
        if (pet_get_gender() == 0)
            ir_tx_packet.char_byte = 0x3d;
        else
            p->char_byte = 0x3f;
        p->owner_id_hi = ir_owner_id_hi & 0x3f;
        p->points_hi = 0x38;
        ir_tx_put_owner_name();
        retry = 0;
    again2:
        memset(&ir_rx_packet, 0, 0x14);
        tmr_t5_pause(1);
        ir_send_packet((int)&ir_tx_packet);
        tmr_t5_start_raw(0x2e5, 8);
        if (ir_recv_packet((int)&ir_rx_packet) == 0x14)
            goto ok2;
        if (btn_samples[2] & 1)
            goto cancel;
        retry++;
        if (retry <= 4)
            goto again2;
        goto fail;
    ok2:
        if (ir_rx_packet.kind == 1) {
            p->kind = 0x70;
            p->role = 2;
            p->arg_lo = 0;
            p->arg_hi = 0;
            p->points_hi = 0;
            tmr_t5_pause(1);
            ir_send_packet((int)p);
            tmr_t5_start_raw(0x2e5, 8);
            if (ir_recv_packet((int)&ir_rx_packet) == 0x14 && ir_rx_packet.kind == 0x71)
                goto done;
        }
        if (fails++ <= 1)
            goto again2;
        goto fail;
    }
    tmr_t5_start_raw(0xf4, 0xf);
    tmr_t5_ticks = 0;
    if (tmr_t5_ticks == 0)
        asm volatile("halt");
    }

done:
    btn_enable();
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2087100);
    n = (ir_rx_packet.sig_code >> 4) | ((ir_rx_packet.owner_id_lo & 0xc0) >> 2);
    gfx_object_create(0x44, 0x38, 0x10, n / 10, 0, 0x208edb0);
    gfx_object_create(0x4d, 0x38, 0x10, n % 10, 0, 0x208edb0);
    func_00009A6();
    snd_enabled = 1;
    snd_play(0x19);
    snd_wait_end();
    snd_enabled = 0;
    tmr_t5_start_raw(0xf4, 0xf);
    test_clear_buttons();
    fails = 0;
    for (;;) {
        if (btn_a_presses != 0) {
            if (P0_P0D & 5)
                goto again;
            goto out;
        }
        if (btn_b_presses != 0)
            return 1;
        test_clear_buttons();
        asm volatile("halt");
        sys_idle_housekeeping();
    }

cancel:
    ir_cancel_sound();
fail:
    gfx_display_reset();
    gfx_set_background(0x7e0);
    gfx_object_create(0, 0x1c, 0, 2, 0, 0x21091e4);
    func_00009A6();
    tmr_t5_start_raw(0xf4, 0xf);
    fails = 0;
    btn_history_init();
    for (;;) {
        btn_history_sample();
        if (!(P0_P0D & 5))
            goto out;
        if ((btn_samples[1] & 3) == 1)
            goto again;
        tmr_t5_ticks = 0;
        if (tmr_t5_ticks == 0)
            asm volatile("halt");
    }
out:
    btn_enable();
    return 0;
}

/*
 * Start a visit (legacy IR exchange result a): a = 3 with our code 0x20 and the peer's 0x21
 * = the partner moves in (visit_partner.x014 = 2, its character data copied from the
 * received record at 0x27D5400, its owner id halves to ir_partner_id_lo / _hi); a = 4 with
 * the codes swapped = our pet goes visiting (x014 = 1, x016 = -12).
 */
u8 visit_start(u8 a)
{
    u8 r = 0;
    int p, o, i;
    struct PetState *q;

    switch (a) {
    case 3:
        if (ir_tx_packet.kind == 0x20 && ir_rx_packet.kind == 0x21) {
            visit_partner.mode = 2;
            p = (int)D_27D5400;
            visit_partner.chara_id = util_read_be16(p + 0x94);
            visit_partner.gender = *(u8 *)(p + 0x98);
            visit_partner.life_stage = *(u8 *)(p + 0x99);
            visit_partner.x010 = *(u8 *)(p + 0x9a);
            visit_partner.x011 = *(u8 *)(p + 0x9b);
            visit_partner.x012 = *(u8 *)(p + 0x9c);
            visit_partner.x013 = *(u8 *)(p + 0x9d);
            visit_partner.dominant_trait = *(u8 *)(p + 0xd0);
            for (i = 0; i < 5; i++) {
                visit_partner.pad006[i - 1] = (o = 0xd0, *(u8 *)(i + p + o + 1));
            }
            visit_partner.x00a = *(u8 *)(p + 0xd6);
            visit_partner.care_mistakes = 0;
            visit_partner.x019 = 0;
            visit_partner.hunger = 0;
            for (i = 0; i < 5; i++) {
                visit_partner.pad00b[i] = (o = 0xd4, *(u8 *)(i + p + o + 3));
            }
            visit_partner.class_code = *(u8 *)(p + 0xc1);
            visit_partner.wish_count = *(u8 *)(p + 0xc0);
            visit_partner.wish1_id_hi = *(u8 *)(p + 0xc2);
            visit_partner.wish1_id_lo = *(u8 *)(p + 0xc4);
            visit_partner.wish2_id_hi = *(u8 *)(p + 0xc3);
            visit_wish2_id_lo = *(u8 *)(p + 0xc5);
            visit_wish1_kind = *(u8 *)(p + 0xc6);
            visit_wish2_kind = *(u8 *)(p + 0xc7);
            visit_wish_count = *(u8 *)(p + 0xc8);
            visit_partner.over = 0;
            ir_partner_id_lo = ir_rx_packet.owner_id_lo & 0x3f;
            ir_partner_id_hi = ir_rx_packet.owner_id_hi & 0x3f;
            memcpy(ir_partner_name, (u8 *)&ir_rx_packet + 5, 5);
            visit_diary_page_mark = diary_page_count();
            if (diary_can_write() != 0)
                visit_diary_full = 0;
            else
                visit_diary_full = 1;
            item_load_pictures((void *)anim_pic_slots, (void *)D_27D5414, 7);
            q = &save.pet;
            save.pet.x062 = 0;
            save.pet.x0cc = 0;
            save.pet.x148 = 0;
            q->poop_count = 0;
            pet_poops_drawn = 0;
            q->toilet_timer = 0;
            save.pet.toilet_count = 0;
            q->status = 0;
            util_rand_advance();
            if (rand() % 100 < 50)
                visit_reward_pic_idx = 1;
            else
                visit_reward_pic_idx = 0;
            pet_add_happiness(2);
            flash_erase_sector(0x80);
            flash_program(D_2780000, D_27D0400, 0x2800);
            flash_program(D_2780000 + 0x2800, D_27D5400, 0x5000);
            friend_add_after_exchange();
            ir_xfer_failed = ir_store_received_message(0, 0x27D5400);
            visit_partner.seconds = 0;
            goto done;
        }
        break;
    case 4:
        if (ir_rx_packet.kind == 0x20 && ir_tx_packet.kind == 0x21) {
            visit_partner.mode = 1;
            visit_partner.over = 0;
            anim_clear_pic_slots();
            anim_pic_slots[4][0] = D_217A188;
            save.pet.x062 = 0;
            save.pet.x0cc = 0;
            save.pet.x148 = 0;
            item_load_pictures((void *)anim_pic_slots, (void *)D_27D5414, 7);
            anim_comm_done();
            friend_add_after_exchange();
            ir_partner_id_lo = ir_rx_packet.owner_id_lo & 0x3f;
            ir_partner_id_hi = ir_rx_packet.owner_id_hi & 0x3f;
            memcpy(ir_partner_name, (u8 *)&ir_rx_packet + 5, 5);
            ir_xfer_failed = ir_store_received_message(0, 0x27D5400);
            visit_partner.seconds = -12;
        done:
            pet_clear_call(4);
            pet_clear_call(1);
            pet_clear_call(2);
            r = 1;
        }
        break;
    }
    return r;
}

/*
 * Pairing check of the two exchanged IR codes (0x22/0x23) for the visit modes.
 */
u8 ir_visit_pair_check(void)
{
    u8 r = 0;

    if ((ir_tx_packet.kind == 0x22 && ir_rx_packet.kind == 0x23)
        || (ir_rx_packet.kind == 0x22 && ir_tx_packet.kind == 0x23)) {
        switch (visit_partner.mode) {
        case 2:
            visit_partner.mode = 0;
            item_load_pictures((void *)anim_pic_slots, (void *)D_27D5414, 7);
            diary_write_comm_page();
            ui_timeout = 10;
            comm_redraw_hook();
            friend_add_after_exchange();
            visit_load_partner_pics(1);
            visit_partner.over = 0;
            visit_partner.seconds = 0;
            comm_load_visitor_pictures(1);
            ir_xfer_failed = ir_store_received_message(0, 0x27D5400);
            r = 1;
            break;
        case 1:
            item_load_pictures((void *)anim_pic_slots, (void *)D_27D5414, 7);
            ir_anim_pause_b(ir_rx_packet.result2 & 0x7f);
            visit_partner.mode = 0;
            anim_good_result();
            ui_timeout = 10;
            comm_redraw_hook();
            friend_add_after_exchange();
            visit_load_partner_pics(1);
            visit_partner.over = 0;
            visit_partner.seconds = 0;
            comm_load_visitor_pictures(1);
            ir_xfer_failed = ir_store_received_message(0, 0x27D5400);
            comm_apply_partner_record();
            ir_store_received_diary(0, 0x27D5400);
            r = 1;
            break;
        }
    }
    return r;
}

const char D_253898C[] = "TAMAGO";

/*
 * The RAM variables this file defines.
 */
u8 ir_block_kind;
u32 ir_tx_ptr;
u32 ir_send_data_ptr;
volatile u16 ir_rx_status;
struct TamagoFile *ir_rx_record;
u8 ir_session_flags;
u32 ir_xfer_started;
u32 ir_xfer_failed;
volatile u8 btn_samples[3];
u16 ir_test_counter;
u8 ir_rx_hunting;
u32 ir_rx_buf_ptr;
u32 ir_edge_count;
u16 ir_edge_count2;
struct IrPacket ir_rx_packet;
struct IrPacketTx ir_tx_packet;
volatile u32 tmr_t5_ticks;
volatile u32 ir_rx_buf_count;
u8 visit_wish2_id_lo;
u8 visit_wish1_kind;
u8 visit_wish2_kind;
u8 visit_wish_count;
u8 ir_partner_id_lo;
u8 ir_partner_id_hi;
u8 ir_partner_name[5];
u8 visit_diary_page_mark;
u8 visit_diary_full;
u8 snd_enabled;
u8 ir_test_mode;

