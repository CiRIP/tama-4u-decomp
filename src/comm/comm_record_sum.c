/*
 * The additive sum of an exchange record and its check (comm_record_sum,
 * comm_record_check_sum).
 *
 * Open: the end of nfc_session or the start of comm_exchange.
 *
 * Code 252AAC2..252AAEC, no .rodata.
 */
#include "proto/comm/comm_record_sum.h"

/*
 * TAMAGO checksum: 16-bit additive sum of the first n - 2 bytes of record p (the last two
 * hold the checksum itself) into *out; n must be > 0xFF (returns 2 otherwise). Used by
 * all the NFC record builders.
 */
int comm_record_sum(u8 *p, u32 n, u16 *out)
{
    u16 sum;
    u32 i;
    u8 c;

    if (n <= 0xff)
        return 2;
    sum = 0;
    for (i = 0; i < n - 2; i++) {
        c = *p++;
        sum += c;
    }
    *out = sum;
    return 0;
}

int comm_record_check_sum(u8 *p)
{
    u8 *e = p;
    u16 sum;
    u32 i, n;
    u8 *s;

    n = *(u32 *)(p + 0x48);
    s = p;
    sum = 0;
    for (i = 0; i < n - 2; i++) {
        u8 c = *s++;
        sum += c;
    }
    e += *(u32 *)(p + 0x48);
    return (sum != *(u16 *)(e - 2)) << 1;
}
