/*
 * The real-time clock driver: reset and interrupt control, reading and setting the time, day
 * period, and the date roll-over with leap years.
 *
 * Code 250B46A..250B6DC, .rodata from 255BF20.
 */
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "hw.h"

/*
 * Real-time clock reset (first boot / clock reset): RTC interrupt off and cleared,
 * software reset + 24 h mode (pRTC_CNTL0 = 0x11), hold the counters (rtc_acquire), stop
 * (0x12), load second/minute/hour/day/month from the RAM clock fields (set here to
 * 00:00:00 on 1 Jan), year register fixed to 8 (BCD counters, so "2008"), day of week from
 * rtc_last_weekday (the firmware's day counter), release and run (0x10), then pRTCINTMODE = 6:
 * 1 s cycle selected but the interrupt still disabled (RTCIEN = 0); the callers enable it
 * with rtc_irq_on.
 * RAM clock fields: pet.clock_second = second, pet.clock_minute = minute, pet.clock_hour = hour,
 * pet.clock_day = day, pet.clock_month = month (BCD in the registers, binary in RAM).
 */
void rtc_reset(void)
{
    rtc_last_weekday = 0;
    save.pet.clock_month = 1;
    save.pet.clock_day = 1;
    save.pet.clock_hour = 0;
    save.pet.clock_minute = 0;
    save.pet.clock_second = 0;
    RTCINTMODE = 0;
    RTCINTSTAT = 1;
    RTC_CNTL0 = 0x11;
    RTC_CNTL0 = 0x10;
    rtc_acquire();
    RTC_CNTL0 = 0x12;
    RTCSEC = save.pet.clock_second;
    RTCMIN = save.pet.clock_minute;
    RTCHOUR = save.pet.clock_hour;
    RTCDAY = save.pet.clock_day;
    RTCMONTH = save.pet.clock_month;
    RTCYEAR = 8;
    RTCDAYWEEK = rtc_last_weekday;
    RTC_CNTL1 = 0;
    RTC_CNTL0 = 0x10;
    RTCINTMODE = 6;
}

/*
 * RTC access: request a register read/write window (RTC_CNTL1 bit 0) and retry while the RTC
 * reports busy (bit 1). The caller releases it by writing 0.
 */
void rtc_acquire(void)
{
    for (;;) {
        RTC_CNTL1 = 1;
        if (!(RTC_CNTL1 & 2))
            break;
        RTC_CNTL1 = 0;
    }
}

/*
 * RTC interrupt off: pRTCINTMODE = 6 = 1 s cycle, level mode, RTCIEN 0 (the counters keep
 * running). rtc_irq_on (7) turns it back on.
 */
void rtc_irq_off(void)
{
    RTCINTMODE = 6;
}

/*
 * RTC 1 s interrupt on: pRTCINTMODE = 7 = RTCT[1:0] 01 (1 second), RTCIMD 1 (level),
 * RTCIEN 1 (S1C33E07 manual III-3-15). rtc_irq_off writes 6, the same with the interrupt
 * disabled. (Not "another period": both select 1 s.)
 */
void rtc_irq_on(void)
{
    RTCINTMODE = 7;
}

/*
 * Current month (pet.clock_month).
 */
u8 rtc_get_month(void)
{
    return save.pet.clock_month;
}

/*
 * Current day (pet.clock_day).
 */
u8 rtc_get_day(void)
{
    return save.pet.clock_day;
}

/*
 * Current hour (pet.clock_hour).
 */
u8 rtc_get_hour(void)
{
    return save.pet.clock_hour;
}

/*
 * Current minute (pet.clock_minute).
 */
u8 rtc_get_minute(void)
{
    return save.pet.clock_minute;
}

/*
 * Current second (pet.clock_second).
 */
u8 rtc_get_second(void)
{
    return save.pet.clock_second;
}

/*
 * Time-of-day period from the hour (rtc_get_hour): 7-8 -> 0 (morning), 9-11 -> 1, 12-15 -> 2
 * (afternoon), 16-18 -> 3 (evening), otherwise 4 (night).
 */
int rtc_day_period(void)
{
    u8 m = rtc_get_hour();
    int r;

    if (m >= 7 && m <= 8)
        r = 0;
    else if (m >= 9 && m <= 11)
        r = 1;
    else if (m >= 12 && m <= 15)
        r = 2;
    else if (m >= 16 && m <= 18)
        r = 3;
    else
        r = 4;
    return r;
}

/*
 * Set the clock: month a, day b, hour c, minute d (seconds = 0), converting to BCD for the
 * RTC registers.
 */
void rtc_set_time(u8 a, u8 b, u8 c, u8 d)
{
    save.pet.clock_month = a;
    save.pet.clock_day = b;
    save.pet.clock_hour = c;
    save.pet.clock_minute = d;
    save.pet.clock_second = 0;
    a = (a / 10) << 4 | a % 10;
    b = (b / 10) << 4 | b % 10;
    c = (c / 10) << 4 | c % 10;
    d = (d / 10) << 4 | d % 10;
    rtc_acquire();
    RTCMONTH = a;
    RTCDAY = b;
    RTCHOUR = c;
    RTCMIN = d;
    RTCSEC = 0;
    RTC_CNTL1 = 0;
}

/*
 * Next day after d in month m (wraps to 1 past the month length; February 29 days).
 */
u8 rtc_next_day_leap(u8 m, u8 d)
{
    switch (m) {
    case 2:
        if (d > 29)
            d = 1;
        break;
    case 4:
    case 6:
    case 9:
    case 11:
        if (d > 30)
            d = 1;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    default:
        if (d > 31)
            d = 1;
        break;
    }
    return d;
}

/*
 * Next day after d in month m (February 28 days): used to advance the date at midnight.
 */
int rtc_next_day(u8 m, u8 d)
{
    switch (m) {
    case 2:
        if (d > 28)
            d = 1;
        break;
    case 4:
    case 6:
    case 9:
    case 11:
        if (d > 30)
            d = 1;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    default:
        if (d > 31)
            d = 1;
        break;
    }
    return d;
}

/*
 * Read the RTC into the game clock variables: second pet.clock_second, minute pet.clock_minute, hour
 * pet.clock_hour (BCD -> binary). The RTC day-of-week register is used as a day-change counter:
 * when it changes, the day pet.clock_day advances (rtc_next_day = days-in-month wrap) and the
 * month pet.clock_month wraps after 12.
 */
void rtc_read_clock(void)
{
    u8 s, m, h, x, old;

    rtc_acquire();
    s = RTCSEC;
    m = RTCMIN;
    h = RTCHOUR;
    x = RTCDAYWEEK;
    RTC_CNTL1 = 0;
    save.pet.clock_second = (s & 0xf) + ((s >> 4) & 7) * 10;
    save.pet.clock_minute = (m & 0xf) + ((m >> 4) & 7) * 10;
    save.pet.clock_hour = (h & 0xf) + ((h >> 4) & 3) * 10;
    if (rtc_last_weekday != x) {
        old = save.pet.clock_day;
        rtc_last_weekday = x;
        save.pet.clock_day = rtc_next_day(save.pet.clock_month, old + 1);
        if (save.pet.clock_day < old) {
            if (++save.pet.clock_month > 12)
                save.pet.clock_month = 1;
        }
    }
}

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 rtc_last_weekday DSTRAM;
