/* Declarations of src/rtc/rtc_driver.c: its types, variables and functions. */
#ifndef PROTO_RTC_RTC_DRIVER_H
#define PROTO_RTC_RTC_DRIVER_H

#include "structs.h"

extern u8 rtc_last_weekday;

void rtc_reset(void);
void rtc_acquire(void);
void rtc_irq_off(void);
void rtc_irq_on(void);
u8 rtc_get_month(void);
u8 rtc_get_day(void);
u8 rtc_get_hour(void);
u8 rtc_get_minute(void);
u8 rtc_get_second(void);
int rtc_day_period(void);
void rtc_set_time(u8, u8, u8, u8);
u8 rtc_next_day_leap(u8, u8);
int rtc_next_day(u8, u8);
void rtc_read_clock(void);

#endif
