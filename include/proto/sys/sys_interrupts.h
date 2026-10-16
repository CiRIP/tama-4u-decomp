/* Declarations of src/sys/sys_interrupts.c: its types, variables and functions. */
#ifndef PROTO_SYS_SYS_INTERRUPTS_H
#define PROTO_SYS_SYS_INTERRUPTS_H

#include "structs.h"

extern u8 sys_trap_state;
extern u8 sys_trap_last;
extern u8 btn_click_played;
extern volatile u16 sys_awake_seconds;
extern volatile u8 sys_debug_strap_count; /* P06 debug strap: low-sample count, 8 = strap on (sys_sample_debug_strap) */
extern volatile u8 btn_events;
extern volatile u16 btn_a_debounce;
extern volatile u16 btn_b_debounce;
extern volatile u16 btn_c_debounce;
extern u8 sys_battery_check_due;
extern volatile u8 sys_display_state;
extern u8 pet_cleaning_reminder;
extern u8 sys_adc_busy;

void btn_rearm(void);
void sys_sample_debug_strap(void);
void sys_idle_housekeeping(void);
void rtc_isr(void);
void sys_isr_halt_a(void);
void sys_trap_halt(void);
void sys_trap_wait(void);
void sys_address_error_trap(void);
void sys_nmi_trap(void);
void sys_trap_record(void);
void tmr_t16_23_isr(void);
void tmr_t16_45_isr(void);
void btn_a_isr(void);
void btn_b_isr(void);
void btn_c_isr(void);
void sys_adc_isr(void);
int func_2500436(void);

#endif
