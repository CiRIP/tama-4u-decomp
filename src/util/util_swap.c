/*
 * A patch-list routine and byte swaps, all uncalled.
 *
 * Code 250B3EA..250B460, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/util/util_swap.h"

/*
 * Apply a patch list p of {address, mask, value} u32 triples to memory until a 0
 * address: mask < 0xFF byte and-or, 0xFF byte store, < 0xFFFF halfword and-or, 0xFFFF
 * halfword store, < 0xFFFFFFFF word and-or, 0xFFFFFFFF word store. DEAD CODE: no call, no
 * code pointer and no data word 0x0250B3EA anywhere in the image, and none of the 29
 * downloadable programs of the download pack calls it (their calls all go to the API
 * block 0x2400AD0..0x2401D92) or holds its address (notes/findings/tamago.md). A
 * downloaded program could still call it: nothing protects any firmware address.
 */
void sys_apply_patch_list(u32 *p)
{
    u32 a;
    u32 m;
    u32 v;

    while (*p != 0) {
        a = *p++;
        m = *p++;
        v = *p++;
        if (m <= 0xfe)
            *(u8 *)a = (*(u8 *)a & m) | v;
        else if (m == 0xff)
            *(u8 *)a = v;
        else if (m <= 0xfffe)
            *(u16 *)a = (*(u16 *)a & m) | v;
        else if (m == 0xffff)
            *(u16 *)a = v;
        else if (m != 0xffffffff)
            *(u32 *)a = (*(u32 *)a & m) | v;
        else
            *(u32 *)a = v;
    }
}

/*
 * Swap the halfwords of a (swaph).
 */
int util_swaph(int a)
{
    int r;

    asm("swaph %0,%1" : "=r"(r) : "r"(a));
    return r;
}

/*
 * Swap the bytes of a (swap).
 */
int util_swap_bytes(int a)
{
    int r;

    asm("swap %0,%1" : "=r"(r) : "r"(a));
    return r;
}

/*
 * Swap two bytes.
 */
void util_swap_u8(u8 *a, s8 *b)
{
    s8 t = *b;

    *b = *a;
    *a = t;
}

/*
 * Swap two halfwords.
 */
void util_swap_u16(u16 *a, s16 *b)
{
    s16 t = *b;

    *b = *a;
    *a = t;
}

/*
 * Swap two words.
 */
void util_swap_u32(u32 *a, u32 *b)
{
    u32 t = *b;

    *b = *a;
    *a = t;
}
