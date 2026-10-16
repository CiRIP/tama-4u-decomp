/* Declarations of src/lib/nfc/nfc_snep.c: its types, variables and functions. */
#ifndef PROTO_LIB_NFC_NFC_SNEP_H
#define PROTO_LIB_NFC_NFC_SNEP_H

#include "structs.h"

struct Nfcid3 {
    u8 pad000[0x8];
    u8 id8;
    u8 id9;
};

extern struct Nfcid3 nfc_peer_nfcid3;

extern const u32 D_256261C;
extern const u32 D_2562654;
extern const u32 D_256268C;
extern const u32 D_25626C4;

int nfc_snep_server(int, int *, int, int, s8);
int nfc_snep_client(void *, int, int);

#endif
