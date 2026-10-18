/* Declarations of src/ir/ir_packets.c: its types, variables and functions. */
#ifndef PROTO_IR_IR_PACKETS_H
#define PROTO_IR_IR_PACKETS_H

#include "structs.h"

extern u8 ui_picture_screen_obj;
extern u8 comm_game_result;
extern u8 visit_reward_pic_idx;
extern u8 ir_peer_has_code;
extern u8 comm_exchange_kind;
extern u8 comm_friend_is_new;
extern u8 comm_is_initiator;
extern u8 comm_partner_slot;
extern u8 ir_exchange_flag;

int ir_clamp_code(u8, u16);
int ir_build_request_packet(int);
int ir_fill_packet_mode_fields(void);
int ir_stub_return7(void);
int ir_build_request_answer(void);
int ir_build_item_packet(void);
void ui_new_screen_with_picture(int, const u8 *);
void comm_load_visitor_pictures(int);
void flash_write_sector_86(u16 *, u16);
int ir_check_kinds_pair(void);
int ir_store_attached_file(int);
struct TamagoFile *comm_attached_file(struct ExchangeRecord *);
void comm_show_item_get(void);
void comm_show_now_printing(void);
void save_with_short_sound(void);
int comm_peer_allows_game(u8 *);
u8 util_random_clear_bit(int, int);
int ir_stub_return0(void);
int ir_write_owner_profile_chunk(void);
int ir_write_message_chunk(void);
int ir_store_received_diary(u8, int);
int ir_store_received_message(u8, int);
void diary_write_pending_page(void);

#endif
