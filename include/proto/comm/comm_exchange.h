/* Declarations of src/comm/comm_exchange.c: its types, variables and functions. */
#ifndef PROTO_COMM_COMM_EXCHANGE_H
#define PROTO_COMM_COMM_EXCHANGE_H

#include "structs.h"

extern u32 comm_gift_item;
extern u32 comm_tx_record;

extern const u32 D_256257C;
extern const u32 D_2562594;
extern const u32 D_25625B4;
extern const u32 D_25625D4;
extern const u32 D_25625F8;
extern const char D_256255C[];
extern const char D_2562564[];
extern const char D_2562570[];

u8 comm_check_codes_paired(void);
u8 comm_check_same_type(void);
u8 comm_check_pair_and_type(void);
void comm_build_header(void);
int comm_build_section_mail(int, int);
int comm_build_section_profile(int base, int off, int flag);
int comm_program_profile_blob(int);
int comm_build_section_images(int, int);
int comm_build_section_poses(int, int, u8 *);
int comm_build_section_images2(int, int, u8 *);
int comm_build_section_proposal(int, int);
int comm_build_section_item(int, int);
void comm_describe_given_item(struct CommHeaderItem *);
int comm_payload_gift_init(void *ctx, u8 *buf);
int comm_payload_gift_target(int, int);
int comm_payload_app_init(int, int);
int comm_payload_app_target(void);
int comm_payload_proposal_init(int, int);
int comm_payload_proposal_target(int, int);
int comm_payload_game_init(int, int);
int comm_payload_game_target(int, int);
int comm_payload_photo_init(int, int);
int comm_payload_photo_target(int, int);
int comm_build_payload(int, int);
void comm_build_phase1(void);
int comm_phase2_code(void);
void comm_build_phase2(void);
void comm_build_phase3(void);
u8 comm_exchange_initiator(void);
int comm_exchange_target(void);
int comm_exchange(void);

#endif
