/*
 * Two tables of the .rodata that came with the code copied to internal RAM at boot
 * (asm/iram.s: the display-object compositor, the dirty-rectangle LCD flush and the NOR
 * flash erase / program routines that must not run from flash). No code reads them; the
 * jump table that followed them (D_255C7DC) is in asm/iram.s.
 *
 * .rodata from 255C664 in the original image.
 */
#include "common.h"

struct IramTableEntry {
    u16 x0;
    u8 x2;
    u8 x3;
};
const struct IramTableEntry D_255C668[70] = {
    { 0x18ff, 1, 1 },
    { 0x18fa, 1, 2 },
    { 0x329f, 1, 3 },
    { 0x67bf, 2, 1 },
    { 0x5e5f, 2, 2 },
    { 0x5800, 3, 1 },
    { 0x6ee0, 1, 1 },
    { 0x5640, 1, 2 },
    { 0x67fb, 2, 1 },
    { 0x5739, 2, 2 },
    { 0x57f4, 2, 1 },
    { 0x2e90, 2, 2 },
    { 0xa7fa, 2, 3 },
    { 0x76e0, 3, 1 },
    { 0x5e40, 3, 2 },
    { 0xfc1f, 1, 1 },
    { 0xc3bb, 1, 2 },
    { 0xf67f, 2, 1 },
    { 0xf55f, 2, 2 },
    { 0xffff, 2, 1 },
    { 0xdefb, 2, 2 },
    { 0xf41f, 3, 1 },
    { 0xd3bb, 3, 2 },
    { 0x3f9f, 1, 1 },
    { 0x06ba, 1, 2 },
    { 0x2d30, 1, 1 },
    { 0x246e, 1, 2 },
    { 0xee5f, 2, 1 },
    { 0xccde, 2, 2 },
    { 0x7d5f, 2, 1 },
    { 0x545d, 2, 2 },
    { 0xab9e, 3, 1 },
    { 0x8a9b, 3, 2 },
    { 0x755f, 3, 1 },
    { 0x4c5d, 3, 2 },
    { 0x068f, 1, 1 },
    { 0x05ec, 1, 2 },
    { 0x1d1f, 1, 1 },
    { 0x1c7c, 1, 2 },
    { 0x0f9f, 2, 1 },
    { 0x0e9c, 2, 2 },
    { 0xd4bf, 3, 1 },
    { 0xbbdc, 3, 2 },
    { 0xd642, 1, 1 },
    { 0xcc80, 1, 2 },
    { 0x165f, 1, 1 },
    { 0x15bc, 1, 2 },
    { 0xff9a, 2, 1 },
    { 0xfe54, 2, 2 },
    { 0xff94, 2, 3 },
    { 0xce42, 3, 1 },
    { 0xbc80, 3, 2 },
    { 0x3650, 3, 1 },
    { 0xfda2, 1, 1 },
    { 0xfc60, 1, 2 },
    { 0xd33a, 1, 1 },
    { 0xcd1f, 2, 1 },
    { 0x0f9f, 2, 1 },
    { 0x0e1f, 2, 2 },
    { 0x079f, 3, 1 },
    { 0x17f9, 3, 1 },
    { 0x16b4, 3, 2 },
    { 0x0f9f, 1, 1 },
    { 0x0e9c, 1, 2 },
    { 0xafdf, 2, 1 },
    { 0x969c, 2, 2 },
    { 0x179f, 2, 1 },
    { 0x0d1f, 3, 1 },
    { 0x0c7c, 3, 2 },
    { 0x0e5f, 3, 3 },
};
const u16 D_255C780[45] = {
    0xd460, 0xc400, 0xed45, 0xffff, 0xef5d, 0xffff, 0x02bd, 0x0a9b,
    0x1b3e, 0xf691, 0xee2e, 0xf716, 0xb7df, 0x977e, 0xd7df, 0xc59e,
    0xbd3e, 0xce1e, 0x06d9, 0x0698, 0x0f5b, 0xffff, 0xef5d, 0xffff,
    0xb45d, 0xa3dc, 0xcd5e, 0x10c4, 0x0000, 0x5acb, 0xd574, 0xcc70,
    0xe637, 0xb43d, 0x9b5c, 0xcd5f, 0x581c, 0x585b, 0x711f, 0xeeff,
    0xde1e, 0xf75f, 0xb43d, 0xabdd, 0xc4ff,
};
