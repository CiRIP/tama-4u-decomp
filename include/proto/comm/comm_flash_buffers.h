/* Declarations of src/comm/comm_flash_buffers.c: its types, variables and functions. */
#ifndef PROTO_COMM_COMM_FLASH_BUFFERS_H
#define PROTO_COMM_COMM_FLASH_BUFFERS_H

#include "structs.h"

extern u32 comm_flash_write_ptr;

void rtc_irq_rearm(void);
void comm_stop_ms_timers(void);
void comm_exclusive_timing_on(void);
void comm_exclusive_timing_off(void);
void comm_erase_tx_sectors(void);
void comm_erase_rx_sectors(void);
void comm_erase_session_buffers(void);
void comm_select_rx_write_pointer(void);
int comm_flash_append(u16 *, int);
int comm_flash_append_fill(u16 *, int);
void comm_flash_append_marker(void);
int ir_rx_store_block(u16 *, int);
void ir_rx_bytes(u8 *, u16);

#endif
