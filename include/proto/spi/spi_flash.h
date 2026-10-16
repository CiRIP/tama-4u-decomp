/* Functions defined in src/spi/spi_flash.c. */
#ifndef PROTO_SPI_SPI_FLASH_H
#define PROTO_SPI_SPI_FLASH_H

#include "structs.h"

void spi_init_hook(void);
void spi_enable(void);
void spi_disable(void);
void spi_cs_assert(void);
void spi_cs_release(void);
void spi_send_byte(int);
void spi_send_bytes(u8 *, int);
int spi_recv_byte(void);
void spi_recv_bytes(int, int);
void spi_flash_setup(void);
void spi_flash_read_id(int);
void spi_flash_write_enable(void);
void spi_flash_command_zero_arg(int);
void spi_flash_sector_command(u16);
void spi_flash_read_page(u16, int);
void spi_flash_program_page(u16, int);
void spi_clock_on(void);
int spi_flash_read_status(void);
void func_250DC32(void);
void func_250DF38(void);
void func_250DF3A(void);

#endif
