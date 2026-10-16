/* Declarations of src/lib/nfc/nfc_reader.c: its types, variables and functions. */
#ifndef PROTO_LIB_NFC_NFC_READER_H
#define PROTO_LIB_NFC_NFC_READER_H

#include "structs.h"

extern u16 card_file_no; /* touch card: P1P2 = file number of the next READ / WRITE APDU */
extern u8 card_auth_key[]; /* touch card: 8-byte AUTHENTICATE key (D_255D378) */
extern u32 nfc_unused_word;
extern u8 nfc_wake_on_field_off;
extern u8 card_block_number;
extern u8 card_auth_enable;

void nfc_get_unused_word(u32 *);
int nfc_sleep_until_field(u8);
s8 nfc_request_abort(void);
s8 card_set_params(int, u8 *, u8);
s8 nfc_poll_tag(int, int, int, int);
s8 nfc_tag_read(int, int, int, u8 *, int, int, s8);
s8 nfc_tag_write(int, int, int, u8 *, int, int);

#endif
