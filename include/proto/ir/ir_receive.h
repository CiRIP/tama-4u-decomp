/* Declarations of src/ir/ir_receive.c: its types, variables and functions. */
#ifndef PROTO_IR_IR_RECEIVE_H
#define PROTO_IR_IR_RECEIVE_H

#include "structs.h"

struct FriendEvent {
    u8 x000;
    u8 kind1_bit;
    u8 kind2_bit;
    u8 blocked;
    u8 x004[16];            /* a name for the diary page of a friend event (never set) */
};

extern u8 ir_saved_timer_ints;
extern u8 ir_marker_matched;
extern struct FriendEvent friend_event;
extern u8 ir_coupon_flag;

extern const u32 D_253BEB8;
extern const u32 D_253BEE8;
extern const u32 D_253BF00;
extern const u32 D_253BF20;
extern const u32 D_253BF5C;
extern const char D_253BEB0[];

int ir_rx_find_marker(int, u16, int);
void ir_rx_feed(u8 *, u16, u8);
void ir_rx_reset(void);
void ir_dir_entry_write(int, int, u32, u8);
void ir_status_word_write(u16);
void ir_xfer_enter(void);
void comm_redraw_hook(void);
int ir_count_tier(u8);
u8 ir_apply_points(void);
void ir_session_leave(void);
int ir_stub_zero_a(void);
int ir_stub_zero_b(void);
u8 friend_find_or_add(u8, u8, int);
int ir_locate_partner_pics(void);
void friend_new_hook(int);
void comm_write_default_partner_image(void);
void friend_portrait_hook(int);
u8 friend_event_hook_a(u8, u8);
void friend_event_hook_b(u8, u8, u8);
u8 friend_event_pick(u8, u8);
int friend_event_fire(u8, u8);
u8 friend_event_run_pending(void);
int friend_event_minute_check(void);
void friend_clear_hook(void);
void friend_portrait_file(int);
void friend_portrait_file_received(void);
void friend_add_after_exchange(void);
void friend_mark_new(void);
void visit_record_clear(void);
void func_25039AA(void);
void ir_stamp_peers_clear(void);
u8 ir_stamp_peer_register(u8, u8, int);
void ir_stamp_peers_confirm(void);
int comm_gift_is_special_coupon(void);
u8 ir_stamp_card_result(u8, u8, int);
int item_kind_whitelist(int, int);
int ir_item_validate(void);
int ir_check_rx_version(void);
int ir_msg_pair_check(void);
void ir_next_packet_kind(void);
void friend_add_hook(void);
void func_25037BE(void);

#endif
