/* Functions defined in src/spi/spi_flash.c. */
#ifndef PROTO_SPI_SPI_FLASH_H
#define PROTO_SPI_SPI_FLASH_H

#include "structs.h"

void spi_enable(void);
void spi_disable(void);
void spi_cs_assert(void);
void spi_cs_release(void);
int spi_recv_byte(void);
void spi_flash_setup(void);
void spi_flash_read_id(void);
void spi_flash_write_enable(void);
void spi_flash_command_zero_arg(void);
void spi_flash_sector_command(void);
void spi_flash_read_page(void);
void spi_flash_program_page(void);
void spi_clock_on(void);
int spi_flash_read_status(void);

#endif
