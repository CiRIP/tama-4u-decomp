/* Declarations of src/sys/sys_boot.c: its types, variables and functions. */
#ifndef PROTO_SYS_SYS_BOOT_H
#define PROTO_SYS_SYS_BOOT_H

#include "structs.h"

extern u16 flash_program_start[];
extern u8 sys_iram_bss_end[];
extern u8 sys_iram_code[];
extern u8 sys_ram_data_end[];
extern u8 sys_iram_bss_start[];
extern u8 sys_heap_end[];
extern volatile u16 sys_battery_level;
extern u8 sys_ram_bss_start[];
extern u8 sys_ram_data_start[];

extern u8 D_2562840[];        /* the load image of the internal-RAM code (linker script) */
extern u8 D_25626FC[];        /* the load image of .data (linker script) */

void sys_adc_setup(int, int, int, int);
void sys_adc_set_limits(int, u16, u16);
void sys_adc_irq_setup(u16, u16);
void sys_reset(void);
void sys_init(void);
void sys_bus_setup(void);
void sys_itc_init(void);
void sys_gpio_init(void);
void tmr_init(void);
void sys_copy_iram_code(void);
void sys_copy_data(void);
void sys_clear_bss_ram(void);
void sys_clear_bss_iram(void);
void sys_irq_enable(void);
void sys_irq_disable(void);

#endif
