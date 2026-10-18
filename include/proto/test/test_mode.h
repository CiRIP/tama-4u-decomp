/* Declarations of src/test/test_mode.c: its types, variables and functions. */
#ifndef PROTO_TEST_TEST_MODE_H
#define PROTO_TEST_TEST_MODE_H

#include "structs.h"

struct FlashTestArea {
    u16 flags;
    u32 first_sector;
    u32 last_sector;
    u32 address;
    u16 fill_value;
    u32 halfwords;
};

extern u8 test_rtc_count_mode;
extern s8 test_rtc_seconds;
extern u16 test_rom_sum_ref; /* test mode program checksum reference, 0x8F12 (test_rom_sum_program) */

extern const struct FlashTestArea test_flash_areas[];
extern const u8 D_255F6F8[];

u16 test_rom_sum_program(void);
u16 test_rom_sum_graphics(void);
void test_mode(void);
int test_flash_erase_all(void);
void test_battery_check(void);
void test_clear_buttons(void);
void btn_wait_release(void);
void test_sound_draw(int);
void test_sound(void);
void test_rtc_count(void);

#endif
