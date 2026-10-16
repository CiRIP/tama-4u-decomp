/* Declarations of src/tmr/tmr_timeouts.c: its types, variables and functions. */
#ifndef PROTO_TMR_TMR_TIMEOUTS_H
#define PROTO_TMR_TMR_TIMEOUTS_H

#include "structs.h"

extern volatile u8 tmr_flags;
extern u8 sys_backlight_request;
extern u8 sys_backlight_applied;

void tmr_t3_start_timeout(u16);
void tmr_t5_start_timeout(u16);
void tmr_delay_ms(u16);
void tmr_delay_ms_t5(u16);
void sys_set_backlight(int);
void sys_apply_backlight(void);

#endif
