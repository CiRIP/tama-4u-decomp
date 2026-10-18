/* Declarations of src/comm/comm_checks.c: its types, variables and functions. */
#ifndef PROTO_COMM_COMM_CHECKS_H
#define PROTO_COMM_COMM_CHECKS_H

#include "structs.h"

/* communication count (all NFC sessions, Touch Spot reads, reader touches; max 1001,
   saved): the house levels of comm_house_level and the unlocks at 5 / 10 / 20 / 50 */
extern u8 comm_peer_ident[18];
extern u16 comm_peer_id;

int comm_check_item_flag(struct ExchangeRecord *);
int comm_check_app_food(struct ExchangeRecord *);
int comm_check_peer_phase(struct ExchangeRecord *, u8);
void comm_count_and_save(void);
void comm_count_add(void);
int comm_store_peer_mail(void);

#endif
