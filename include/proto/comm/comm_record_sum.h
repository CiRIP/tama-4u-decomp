/* Functions defined in src/comm/comm_record_sum.c. */
#ifndef PROTO_COMM_COMM_RECORD_SUM_H
#define PROTO_COMM_COMM_RECORD_SUM_H

#include "structs.h"

int comm_record_sum(u8 *, u32, u16 *);
int comm_record_check_sum(u8 *);

#endif
