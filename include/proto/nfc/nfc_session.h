/* Declarations of src/nfc/nfc_session.c: its types, variables and functions. */
#ifndef PROTO_NFC_NFC_SESSION_H
#define PROTO_NFC_NFC_SESSION_H

#include "structs.h"

/* NFC session state (F69): x000_b busy, x001 front-end state (1 ok, 2 failed), x002 role
 * (1 initiator, 2 target), x003 session type (= byte 0x4C of the exchanged records: 0 auto,
 * 1 link test, 2 greeting, 3/4 game rounds, 5 gift, 6 proposal, 7 photo, 8 app link; set
 * from the menu choice by nfc_menu_session), x004 / x006 peer kind (0xF040 Tamagotchi, 0xF0F0
 * the app, 0xF0EF no Tamagotchi), x00c our message code, x014 proposal text pointer, x018
 * item refused flag, x01a / x01c / x01e game scores (initiator / target / sum). */
struct NfcSession {
    union {
        u32 busy;
        struct {
            u8 x000_b;
            u8 x001;
            u8 x002;
            u8 x003;
        };
    };
    u16 peer_kind;
    u16 detected_peer;
    u32 item_file;
    u8 msg_code;
    u8 pad00d[0x3];
    u32 friend_data;
    u32 proposal_text;
    s8 item_refused;
    u8 pad019[0x1];
    u16 score_initiator;
    u16 score_target;
    u16 score_total;
};

extern u16 nfc_field_irq;
/* a TAMAGO exchange record in RAM (0x180 bytes), its header as struct ExchangeRecord */
union CommPacket {
    u8 bytes[0x180];
    struct ExchangeRecord rec;
};

extern u8 comm_tx_packet[0x180];
extern union CommPacket comm_rx_packet;
extern struct NfcSession nfc_session;
extern s8 nfc_result;

extern const u32 D_2562518;
extern const u32 D_2562538;
extern const u8 D_25624E8[];
extern const u8 D_25624F0[];
extern const u8 D_2562508[];
extern const u8 D_2562510[];

void nfc_show_session_screen(u8);
void test_nfc_loopback(void);
void nfc_session_clear(void);
void comm_reset_records(void);
int nfc_user_abort(void);
void nfc_wait_field_on(void);
void nfc_sleep_field_off_set(void);
void nfc_wait_field_off(void);
void nfc_rearm_if_field(void);
s8 nfc_field_seen(void);
s8 nfc_find_peer_target(u16 *);
int nfc_find_peer_initiator(u16 *);
s8 nfc_power_up(void);
int nfc_init_if_enabled(void);
int nfc_send(void *, int, int);
int nfc_receive(int, int *, int, s8, int);
int test_nfc_init_check(void);
u8 test_nfc_link(void);
int nfc_session_initiator(u8);
int nfc_session_target(u8);
int nfc_menu_session(u8);
int comm_reset_records_wrapper(void);
int nfc_idle_target_session(void);
int nfc_idle_session(void);

#endif
