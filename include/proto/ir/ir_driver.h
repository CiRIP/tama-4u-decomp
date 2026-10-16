/* Declarations of src/ir/ir_driver.c: its types, variables and functions. */
#ifndef PROTO_IR_IR_DRIVER_H
#define PROTO_IR_IR_DRIVER_H

#include "structs.h"

extern volatile u16 ir_result;
extern u32 ir_port_ptr;
extern u32 sys_saved_psr;
extern u16 ir_last_error;
extern u8 ir_session_busy;
extern u16 ir_carrier_ram[];
extern u16 ir_ram_routines[];

extern u16 ir_carrier_code[];
extern u16 tmr_t5_handler_code[];

void ir_port_setup(void);
void ir_edge_isr(void);
void tmr_t5_start_tick(u16, u8);
void tmr_t5_wait_period(u16, u8);
int ir_count_low(u16, u8);
void sys_clock_osc3(void);
void ir_power_up(void);
void ir_power_down(void);
void ir_rx_start(void);
int ir_detect_start(void);
int ir_decode_bytes(int);
int ir_recv_packet(int);
void ir_send_packet(int);
void ir_send_ping(int);
void tmr_t5_pause(u8);
void tmr_t5_pause_restore(u8);
void sys_copy_ram_routines(void);
void ir_carrier_burst(int);
void btn_wait_press_or_timeout(u8);
u8 ir_checksum(u8 *);
void sys_irq_save_disable(void);
void sys_irq_restore(void);
void btn_history_init(void);
void btn_history_sample(void);
void btn_disable(void);
void btn_enable(void);
void tmr_t5_start_raw(u16, u8);
void tmr_t5_standard_tick(void);
void tmr_t5_cmpa_vector(void);
void tmr_t5_tick_handler(void);
int ir_stop(int v);
void func_2504F9E(void);
void func_25051CC(void);

#endif
