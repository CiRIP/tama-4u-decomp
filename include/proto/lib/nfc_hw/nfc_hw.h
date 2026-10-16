/* Declarations of src/lib/nfc_hw/nfc_hw.c: its types, variables and functions. */
#ifndef PROTO_LIB_NFC_HW_NFC_HW_H
#define PROTO_LIB_NFC_HW_NFC_HW_H

#include "structs.h"

/* Our NFCID3 (10 bytes): the random ID the PN512 generates in nfc_init (Generate
 * RandomID + WrNFCIDtoFIFO), bytes 0/1 and 8/9 forced to F0 40 (the Tamagotchi marker).
 * Sent in ATR_REQ / ATR_RES; bytes 0..5 also form the IDm of nfc_send_sensf_res. */
struct NfcId3 {
    u8 marker0;
    u8 marker1;
    u8 rand0;
    u8 rand1;
    u8 rand2;
    u8 rand3;
    u8 rand4;
    u8 rand5;
    u8 marker2;
    u8 marker3;
};

/* The FIFOs of the serial link to the NFC controller (nfc/nfc_link fills / drains them, the
 * ISRs in lib/nfc_hw/nfc_hw move bytes to 0x300B10 / from 0x300B11, EFSIO ch.1): transmit
 * nfc_tx_fifo, receive nfc_rx_fifo (st: bit 7 = timeout; bits 2..4 were meant to collect the
 * UART receive errors, but the error ISR nfc_uart_err_isr reads 0x300B02 = ch.0 STATUS, so
 * they stay 0). */
struct NfcTxFifo { u32 size; u32 count; u8 *buf; u8 *end; u8 *wr; u8 *rd; };

struct NfcRxFifo { u32 size; u32 count; u8 overflow; u8 status; u8 pad[2]; u8 *buf; u8 *end; u8 *wr; u8 *rd; };

extern volatile u32 nfc_ticks;      /* counted by an ISR */
extern volatile struct NfcTxFifo nfc_tx_fifo;
extern volatile struct NfcRxFifo nfc_rx_fifo;
extern struct NfcId3 nfc_id3;
extern u8 nfc_cancel_request;
extern u32 nfc_flash_base;

void nfc_uart_tx_isr(void);
void nfc_uart_rx_isr(void);
void nfc_uart_err_isr(void);
void nfc_tick_start(void);
void nfc_tick_stop(void);
void nfc_tick_isr(void);
void nfc_power_delay(void);
void nfc_hw_power_on(void);
void nfc_hw_power_off(void);
s8 nfc_init(struct FlashBase *);

#endif
