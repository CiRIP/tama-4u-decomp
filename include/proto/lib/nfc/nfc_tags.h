/* Declarations of src/lib/nfc/nfc_tags.c: its types, variables and functions. */
#ifndef PROTO_LIB_NFC_NFC_TAGS_H
#define PROTO_LIB_NFC_NFC_TAGS_H

#include "structs.h"

extern u8 nfc_rx_data_buf[320];

int nfc_copy_last_frame(u8 *, int *);
s8 nfc_activate_type_a(u8 *, int, u16, int);
s8 nfc_identify_type2_tag(u8 *, int, int);
s8 nfc_poll_felica(int, int, int, int);
s8 nfc_mifare_read(int, int, int, u8 *, int, int, int);
s8 nfc_type2_read(int a, u32 b, u32 c, u8 *d, int e, int tmo, s8 g);
s8 card_read(int, u32, u32, u8 *, int, int, s8);
s8 nfc_mifare_write(int, int, int, u8 *, int, int);
s8 nfc_type2_write(int, int, int, u8 *, int, int);

#endif
