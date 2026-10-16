/* Declarations of src/lib/nfc/nfc_dep.c: its types, variables and functions. */
#ifndef PROTO_LIB_NFC_NFC_DEP_H
#define PROTO_LIB_NFC_NFC_DEP_H

#include "structs.h"

extern u8 nfc_tx_fifo_buf[];
extern u8 nfc_rx_fifo_buf[];
extern s8 nfc_abort;
extern s8 nfc_last_frame_len;
extern u8 nfc_last_frame[];

s8 card_apdu_f5_session(u16, int, int, int, int, int, int, int);
int card_last_result(u16 *, u8 *);
s8 card_apdu_c4_session(u8 *, int, int, int);
int nfc_detect_initiator(u8 *, u16 *, int);
int nfc_detect_target(u8 *, u16 *, int);
int nfc_dep_receive(int, u32 *, u32, int, s8);
s8 nfc_dep_send(void *, int, int);

#endif
