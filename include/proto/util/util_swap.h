/* Functions defined in src/util/util_swap.c. */
#ifndef PROTO_UTIL_UTIL_SWAP_H
#define PROTO_UTIL_UTIL_SWAP_H

#include "structs.h"

void sys_apply_patch_list(u32 *);
int util_swaph(int);
int util_swap_bytes(int);
void util_swap_u8(u8 *, s8 *);
void util_swap_u16(u16 *, s16 *);
void util_swap_u32(u32 *, u32 *);

#endif
