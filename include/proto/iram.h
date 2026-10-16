/* Functions defined in the RAM-resident code (asm/iram.s) and the other functions below 0x2400000. */
#ifndef PROTO_IRAM_H
#define PROTO_IRAM_H

#include "structs.h"

void func_0000000(struct GfxObject *);
u16 func_000014A(struct GfxObject *, int);
u16 func_00002D4(u16);
void func_0000312(struct GfxObject *, const u8 *, u16);
int func_00004C8(int, int);
void func_00007A2(u8, u8, u8, u8);
void func_00009A6(void);
int func_0000A54(int, int, int);
int func_0000B52(u16 *, u16, int);
int func_0000C46(void *, int, int, int);

#endif
