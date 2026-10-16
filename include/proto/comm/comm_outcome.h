/* Declarations of src/comm/comm_outcome.c: its types, variables and functions. */
#ifndef PROTO_COMM_COMM_OUTCOME_H
#define PROTO_COMM_COMM_OUTCOME_H

#include "structs.h"

struct PeerId {
    u8 pad000[0x1];
    u8 owner_id_hi;
    u8 owner_id_lo;
    u8 pad003[0x1];
    u8 name[16];
};

extern u16 comm_storing_gift;
extern u8 *comm_rx_record;
extern u32 comm_outcome_record;
extern struct PeerId comm_phase1_peer;

extern const u32 D_2562238;
extern const u32 D_25622C8;
extern const u32 D_25623B0;
extern const u8 D_2562144[];
extern const u8 D_2562148[];
extern const u8 D_256215C[];
extern const u8 D_2562170[];
extern const u8 D_2562184[];
extern const u8 D_2562194[];
extern const u8 D_25621A8[];
extern const char D_25621B4[];
extern const char D_25621C0[];
extern const char D_25621CC[];
extern const u8 *const owner_mood_icons[];
extern const char D_25621EC[];
extern const char D_25621F8[];
extern const char D_2562208[];
extern const char D_2562218[];
extern const char D_2562220[];
extern const char D_256222C[];
extern const u8 D_2562290[];
extern const u8 D_25622A0[];
extern const u8 D_25622AC[];
extern const u8 D_25622B4[];
extern const u8 D_25622EC[];
extern const u8 D_25622FC[];
extern const u8 D_256230C[];
extern const u8 D_2562320[];
extern const u8 D_2562334[];
extern const u8 D_2562394[];
extern const u8 D_25623A0[];
extern const u8 D_2562254[];
extern const u8 D_2562268[];
extern const u8 D_256227C[];
extern const u8 D_25622BC[];

int item_find_builtin_match(int, struct TamagoFile **);
u8 comm_take_gift_item(struct ExchangeRecord *);
void comm_show_msg(int);
void comm_get_profile(u8 *, void *, u8 *);
void comm_get_peer_profile(u8 *, void *);
void owner_gender_text(u8, char *);
int owner_mood_icon(u8);
void owner_mood_text(u8, char *);
void comm_draw_profile_card(struct OwnerProfile *);
void comm_info_screen(int);
s8 comm_ask_fortune(void);
void comm_fortune_number(u8, struct OwnerProfile *, u8 *);
void comm_show_fortune(u8);
void comm_outcome_greeting(void);
int comm_load_section_pics(u32 *dst, u8 *list, int n);
void comm_outcome_gift(void);
void evo_after_marriage(void);
void comm_proposal_refused_msg(int);
u8 comm_can_marry(void);
void comm_outcome_marriage(void);
void comm_outcome_app(void);
void comm_retry_msg(u8);
void comm_outcome_game1(void);
void comm_outcome_game2(void);
void comm_outcome_photo(void);
int comm_run_outcome(void);
void comm_idle_touch_event(void);
void comm_trace(void);
void comm_trace2(int);
u8 comm_user_cancelled(void);
void nfc_short_delay(void);
void nfc_power_off(void);
void comm_apply_points(struct GiftPoints *);
void btn_mask(int);
void btn_unmask(int);
void ui_wait_ticks(u8);
void ui_wait_ticks_hold_a(u8);
int comm_erase_banks(void);
int flash_fill(u32, u32, u16);
int util_fill16(u32, u8 *, u16);
int flash_program_bytes(u32, u32, u8 *);
int flash_program_halfwords(u32, u16 *, int);
int comm_record_item(struct ExchangeRecord *);
void save_after_sound(void);
void snd_wait_before_flash(void);
int comm_same_peer(u16, int);
void comm_remember_peer(u16, u8 *);
u8 friend_lookup_or_add(u16, int, u8);
u8 *friend_portrait(int);
void friend_book_update(void);
void comm_store_pending_friend(void);
void comm_make_random_friend(void);
void pet_pick_variant_flag(void);
int chara_family(u16);

#endif
