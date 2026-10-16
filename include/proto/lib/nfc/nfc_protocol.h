/* Declarations of src/lib/nfc/nfc_protocol.c: its types, variables and functions. */
#ifndef PROTO_LIB_NFC_NFC_PROTOCOL_H
#define PROTO_LIB_NFC_NFC_PROTOCOL_H

#include "structs.h"

extern u8 nfc_snep_odd_byte;
extern u32 nfc_snep_payload_len;
extern u8 nfc_cmd_buf[0x140];   /* PN512 register / value command pairs, frames */
extern u8 card_auth_answer[]; /* touch card: the 4 data bytes of the AUTHENTICATE answer (unused) */
extern u16 card_status_word; /* touch card: SW1SW2 of the last APDU */
extern s8 nfc_snep_put_template[56];
extern u8 nfc_snep_response_template[12];

s8 card_write(int, int, int, u8 *, int, int);
s8 card_activate(int);
int card_deselect(int);
int card_apdu_f5(int, int, int, int, int, int, int, int);
int card_apdu_c4(int a, int b, int c);
s8 nfc_error_code(s8, s8);
void nfc_send_sensf_req(void);
void nfc_send_sensf_res(void);
void nfc_send_atr_req(s8);
void nfc_send_atr_res(s8);
void nfc_send_dep_res_atn(void);
void nfc_llcp_send_symm_req(int);
void nfc_llcp_send_symm_res(int);
void nfc_llcp_send_connect(int);
void nfc_llcp_send_cc(int);
void nfc_llcp_send_dm_sdp(int);
void nfc_snep_send_put(u8 seq, u8 b, u8 **pp, u32 *len);
void nfc_snep_send_response(int, u8, u8);
void nfc_llcp_send_rr(int, u8, s8);
void nfc_llcp_send_disc(int);
void nfc_llcp_send_disc_server(int);
void nfc_llcp_send_dm(int);
s8 nfc_snep_store_fragment(int, u8 *, int *, u32, int, int *, s8, s8);
s8 nfc_poll_type_a(int, int, int);
void nfc_setup_passive_initiator(void);
void nfc_setup_active_initiator(void);

#endif
