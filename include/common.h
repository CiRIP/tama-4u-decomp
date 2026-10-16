#ifndef COMMON_H
#define COMMON_H

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

/* A variable in the 2 KB DST RAM (0x84000..) instead of the internal RAM, which is too
 * small for all of them: the ones the original firmware kept there. */
#define DSTRAM __attribute__((section(".dstram")))

#endif
