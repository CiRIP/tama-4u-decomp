/* Declarations of src/ir/ir_session.c: its types, variables and functions. */
#ifndef PROTO_IR_IR_SESSION_H
#define PROTO_IR_IR_SESSION_H

#include "structs.h"

/*
 * The legacy IR packets (20 bytes, see notes/findings/ir.md): ir_rx_packet = the peer's
 * (0xDBC..0xDCF), ir_tx_packet = ours (0xDD0..0xDE3, followed by the tick counter
 * tmr_t5_ticks). x000 low nibble 0xD = signature, x000 >> 4 with x002 bits 6-7 = a 6-bit
 * code (0x38 / 0x3C sent); x001 = packet kind; x002 / x004 bits 0-5 = the owner id halves
 * ir_owner_id_lo / ir_owner_id_hi; bytes 5.. = owner name; x00b role / game, x00c-x00d amount,
 * x00e / x00f result or item index; x013 = 8-bit sum of bytes 0..18. ir_edge_count /
 * ir_edge_count2 = edges counted by the (unvectored) P04 ISR ir_edge_isr.
 */
struct IrPacket {
    u8 sig_code;
    u8 kind;
    u8 owner_id_lo;
    u8 char_byte;
    u8 owner_id_hi;
    u8 pad005[0x6];
    u8 role;
    u8 arg_lo;
    u8 pad00d[0x1];
    u8 result;
    u8 result2;
    u8 points_hi;
    u8 points_lo;
    u8 flags;
    u8 checksum;
};

struct IrPacketTx {
    u8 sig_code;
    u8 kind;
    u8 owner_id_lo;
    u8 char_byte;
    u8 owner_id_hi;
    u8 pad005[0x6];
    u8 role;
    u8 arg_lo;
    u8 arg_hi;
    u8 result;
    u8 result2;
    u8 points_hi;
    u8 points_lo;
    u8 x012;
    u8 x013;
};

extern u8 ir_block_kind;
extern u32 ir_tx_ptr;
extern u32 ir_send_data_ptr;
extern volatile u16 ir_rx_status;
extern struct TamagoFile *ir_rx_record;
extern u8 ir_session_flags;
extern u32 ir_xfer_started;
extern u32 ir_xfer_failed;
extern volatile u8 btn_samples[];
extern u16 ir_test_counter;
extern u8 ir_rx_hunting;
extern u32 ir_rx_buf_ptr;
extern u32 ir_edge_count;
extern u16 ir_edge_count2;
extern struct IrPacket ir_rx_packet;
extern struct IrPacketTx ir_tx_packet;
extern volatile u32 tmr_t5_ticks;
extern volatile u32 ir_rx_buf_count;
extern u8 visit_wish2_id_lo;
extern u8 visit_wish1_kind;
extern u8 visit_wish2_kind;
extern u8 visit_wish_count;
extern u8 ir_partner_id_lo;
extern u8 ir_partner_id_hi;
extern u8 ir_partner_name[5];
extern u8 visit_diary_page_mark;
extern u8 visit_diary_full;
/* sound on (1) / off (0): menu ui_sound_menu, 1 at boot (sys_main) */
extern u8 snd_enabled;
extern u8 ir_test_mode;

extern const u32 D_2538958;
extern const u32 D_253896C;
extern const u32 D_2538998;
extern const u32 D_2538F00;
extern const u8 D_25388F8[];
extern const u8 D_253890C[];
extern const u8 D_2538920[];
extern const u8 D_2538934[];
extern const u8 D_2538948[];
extern const char D_253898C[];

int visit_leave_message(void);
int visit_end(void);
int ir_stamp_card_messages(void);
int ir_session_run(u8);
void ir_session_cleanup(void);
u8 ir_exchange(void);
int ir_record_sum(u8 *, u32, u16 *);
int ir_validate_record(u8, u8 *, int, u16);
int ir_render_section(int, struct PictureSetHdr *);
void ir_abort_and_store_status(void);
int ir_session_receive_only(void);
u8 ir_receive_record_step(void);
int item_is_special_builtin(struct TamagoFile *);
int item_is_special_category(struct TamagoFile *);
u8 ir_check_rx_record(void);
int ir_check_rx_sum(void);
void ir_cancel_sound(void);
void ir_apply_point_gift(u8, int);
u8 ir_point_gift_packets(int);
int ir_point_gift_session(int, int);
u8 ir_test_exchange(void);
int ir_test_session(int);
int ir_test_entry(void);
int ir_test_hello(void);
int ir_test_packets(void);
u8 visit_start(u8);
u8 ir_visit_pair_check(void);

#endif
