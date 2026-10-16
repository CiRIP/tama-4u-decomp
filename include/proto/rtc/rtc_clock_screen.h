/* Declarations of src/rtc/rtc_clock_screen.c: its types, variables and functions. */
#ifndef PROTO_RTC_RTC_CLOCK_SCREEN_H
#define PROTO_RTC_RTC_CLOCK_SCREEN_H

#include "structs.h"

extern u8 rtc_clock_blink;
extern u8 rtc_clock_ampm_obj;
extern u8 rtc_clock_month_tens_obj;
extern u8 rtc_clock_month_ones_obj;
extern u8 rtc_clock_day_tens_obj;
extern u8 rtc_clock_day_ones_obj;
extern u8 rtc_clock_hour_tens_obj;
extern u8 rtc_clock_hour_ones_obj;
extern u8 rtc_clock_min_tens_obj;
extern u8 rtc_clock_min_ones_obj;
extern u8 rtc_clock_sec_tens_obj;
extern u8 rtc_clock_sec_ones_obj;

extern const u8 *const rtc_clock_small_digits[];
extern const u8 *const rtc_clock_big_digits[];
extern const u32 D_255F9D0;

void rtc_clock_draw(u8, u8, u8, u8, u8, u8);
void rtc_clock_show_now(void);
void rtc_clock_blink_toggle(void);
u8 rtc_clamp_day(u8, u8);
void rtc_clock_setting(int);
void rtc_clock_create(void);
int lcd_brightness_setting(void);

#endif
