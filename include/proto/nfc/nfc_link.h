/* Functions defined in src/nfc/nfc_link.c. */
#ifndef PROTO_NFC_NFC_LINK_H
#define PROTO_NFC_NFC_LINK_H

#include "structs.h"

void nfc_irq_isr(void);
s8 nfc_link_open(int);
s8 nfc_link_open_921k(int);
void nfc_link_close(void);
int nfc_uart_send(u8 *, int);
int nfc_uart_recv(u8 *, int, u8 *);
s8 nfc_rearm_field_detect(void);

#endif
