/* Declarations of src/comm/comm_menu.c: its types, variables and functions. */
#ifndef PROTO_COMM_COMM_MENU_H
#define PROTO_COMM_COMM_MENU_H

#include "structs.h"

struct FriendBookState {
    u8 open_after_comm;
    u8 new_friend_waiting;
    u8 x002;
    u8 deleted_count;
    u8 x004;                /* x004..x006: cleared with the visitor record, never read */
    u8 x005;
    u8 x006;
    u8 x007;
};

struct MailQueue {
    u8 friend0;
    u8 friend1;
    u8 friend2;
};

/* The friend list: 31 records of 0x16 bytes (record 30 = the current visitor; F16
 * friend_find_or_add): x000 used flag, x001 / x002 owner id high / low byte, x003 flag,
 * x004 the 16-byte name, x014[0] met-by bits (1 greeting / 2 other), x014[1] portrait
 * slot + 1. */
/* A friend-book record (save.friend_list). */
struct Friend {
    u8 x000_b;              /* 0 = free; the IR stamp peers 31 / 32: 0xFE pending */
    u8 x001;                /* with owner_id_lo: the friend's owner id (high byte) */
    u8 owner_id_lo;
    u8 gender;
    u8 name[0x10];
    u8 met_flags[2];        /* [1]: the portrait slot + 1 (friend_portrait_slot) */
};

struct GiftSource {
    u32 flags;
    u32 item_index;
};

extern u8 ir_point_gift_active;
extern u8 ir_answer_only;
extern u8 comm_menu_choice;
extern u8 comm_submenu_choice;
extern u8 ir_gift_amount;
extern u8 comm_game_choice;
extern volatile u8 ir_gift_menu_done;
extern struct FriendBookState friend_book_state;
extern struct MailQueue comm_mail_queue;
extern u8 sys_idle_seconds;
extern u8 comm_friendbook_updated;
extern u8 ir_owner_id_lo;
extern u8 ir_owner_id_hi;
extern struct GiftSource comm_gift_source;

extern const u32 D_2537DDC;
extern const u32 D_2537E00;
extern const u32 D_2537E24;
extern const u32 D_2537E44;
extern const u32 D_2537E68;
extern const u32 D_2537EE4;
extern const struct MenuTable comm_tama4u_menu_items;
extern const struct MenuTable ir_point_gift_menu_items;
extern const u32 D_25380D8;
extern const u8 D_2536DD8[];
extern const u8 D_2536DE8[];
extern const u8 D_2536DF0[];
extern const u8 D_2536E30[];
extern const u8 D_2536E3C[];
extern const u8 D_2536E44[];
extern const u8 D_2536E58[];
extern const u8 D_2536E1C[];
extern const u8 D_2536E0C[];
extern const u8 comm_proposal_texts[][50];
extern const u8 D_2536DF8[];
extern const u8 D_2537F54[];
extern const u8 D_2537F60[];
extern const u8 D_2537F68[];
extern const u8 D_2537FE0[];
extern const u8 D_2537FF0[];
extern const u8 D_25380C8[];
extern const u8 D_2536DFC[];
extern const u8 D_2536E28[];
extern const struct MenuTable comm_give_shop_menu_items;
extern const struct MenuTable comm_give_inventory_menu_items;
extern const struct MenuTable comm_main_menu_debug_items;
extern const struct MenuTable comm_main_menu_items;

void comm_prepare(void);
void comm_result_wait(void);
void comm_result_wait_book(void);
int comm_menu_app(void);
int comm_menu_item_send(void);
int comm_result_unused(void);
int ir_menu_hidden_session(void);
int ir_session5_unused(void);
u8 comm_give_item(int);
int ir_session3_unused(void);
int comm_menu_debug_set(void);
int comm_photo_session(void);
int comm_menu_photo(void);
int pet_debug_set_age_5days(void);
int ir_session3_check_stub(void);
u8 comm_session_menu(u8);
int comm_menu_proposal(void);
int comm_stub_one(void);
int comm_ask_marry(void);
int ir_show_proposal_refusal(u8);
void comm_result_anim(void);
void comm_show_proposal_line(u8);
void comm_show_rx_message(void);
void comm_record_result(void);
void comm_result_show_record(void);
void comm_load_rx_pictures(void);
void comm_find_portrait(u8 **, u8 *);
void comm_show_rx_item(void);
void comm_gift_received(void);
int item_capsule_prize_class(struct TamagoFile *);
void item_select_presenter(struct TamagoFile *);
void pet_count_stamp_liked(struct TamagoFile *);
int item_use_capsule(struct TamagoFile *);
int item_stamp_index(struct TamagoFile *);
int item_store_received(struct TamagoFile *);
u8 item_can_give(struct TamagoFile *);
void item_receive_letter(int p0);
void item_receive_letter2(int p0);
int item_receive_presentation(struct TamagoFile *, int *, u32 *);
int comm_item_exchange_result(int, struct ExchangeRecord *, struct ExchangeRecord *);
int item_bank_has_items(int);
u8 comm_give_shop_item(void);
int item_list_has_owned(int);
u8 comm_give_inventory_item(void);
int comm_give_menu(void);
int comm_menu_item_exchange(void);
int comm_menu_enable_zero(void);
int comm_set_game_choice(int);
int comm_game_session(void);
int comm_menu_games(void);
int comm_submenu_enable(int);
int comm_menu_tama4u(void);
int ir_point_gift(void);
int ir_set_gift_amount(int);
int ir_menu_point_gift(void);
int comm_set_gift_kind(int);
int comm_greeting_session(void);
int comm_menu_greeting(void);
int comm_menu_enable(int);
int comm_main_menu(void);
void comm_put_pet_name(u8 *);
u8 util_lowest_set_bit(u8);
void tmr_t5_wait_100ms(u16);
void btn_clear(void);
void flash_begin(void);
void flash_end(void);
u32 util_bswap32(u32 *);
u16 util_read_be16(int);
void anim_clear_pic_slot(u8);
void anim_clear_pic_slots(void);
void anim_clear_pic_slots_n(int);
void anim_clear_pic_slots2(void);
u8 visit_load_partner_pics(u8);
int item_list_blobs(u32 *, u8 *, int);
void util_rand_advance(void);
int visit_active(void);
u8 sys_stub_zero(void);
void visit_partner_add_traits(int);
void pet_add_traits(u8 *);
void comm_apply_partner_record(void);
u8 pet_calc_dominant_trait(void);
void pet_clear_growth_items(void);
int chara_base_id(u16);
void anim_set_busy(u8);
u8 util_rand_choice(int);
int flash_program_raw(u32, u16 *, u32);
int ir_session(u8);
int visit_tick(void);

#endif
