/*
 * The firmware image: the program in the NOR flash at 0x2400000 (the CPU boots from the
 * vector table there), the RAM variables, and the graphics and save data the program reads
 * from the rest of the flash (assets/, extracted from your firmware).
 */
INCLUDE ldscripts/address_symbols.ld

MEMORY
{
  /* the program sectors; the flash from 0x2570000 on holds saved data (diary pages ...) */
  program (rx) : ORIGIN = 0x2400000, LENGTH = 0x170000
  /* internal RAM: the code copied there at start-up, the variables; the stack is above */
  iram (rwx)   : ORIGIN = 0, LENGTH = 0x4AE4
  /* the 2 KB DST RAM; downloaded programs keep their variables at 0x84740..0x847BF */
  dstram (rw)  : ORIGIN = 0x84000, LENGTH = 0x740
}

SECTIONS
{
  /* the image starts at the flash base (objcopy fills the gaps with 0xFF) */
  .flash_base 0x2000000 : { BYTE(0xff) }
  /* the flash contents outside the program (tools/extract_assets.py; raw binary inputs,
     each a .data section) */
  .save_0040000 0x2040000 : { *save_0040000.bin(.data) }
  .gfx_0080000 0x2080000 : { *gfx_0080000.bin(.data) }
  .gfx_0340000 0x2340000 : { *gfx_0340000.bin(.data) }
  .save_05D0000 0x25d0000 : { *save_05D0000.bin(.data) }
  .save_06E0000 0x26e0000 : { *save_06E0000.bin(.data) }

  .vectors 0x2400000 : { *(.vectors) } > program
  /* the entry points of the downloaded-program API (asm/api.s), at fixed addresses */
  .api 0x2400AD0 : { *(.api) } > program
  .text : { *(.text) } > program
  .rodata ALIGN(4) : { *(.rodata) } > program

  /* the DST RAM: the variables marked DSTRAM and the C library's, zeroed at start-up
     (sys_clear_bss_ram), then the initialized variables, stored after .rodata and copied
     at start-up (sys_copy_data) */
  .dstram 0x84000 (NOLOAD) : { *(.dstram) *libc.a:*(.bss) *libstdio.a:*(.bss) } > dstram
  .data : AT(LOADADDR(.rodata) + SIZEOF(.rodata)) { *(.data) } > dstram

  /* the code run from internal RAM (asm/iram.s), stored after .data, copied to 0 at
     start-up (sys_copy_iram_code) */
  .iram 0 : AT(LOADADDR(.data) + SIZEOF(.data)) { *(.iram) } > iram
  /* the other variables, zeroed at start-up (sys_clear_bss_iram) */
  .bss ADDR(.iram) + SIZEOF(.iram) (NOLOAD) : { *(.bss) *(COMMON) } > iram

  /DISCARD/ : { *(.comment) *(.comm) }
}

/* the layout as the start-up code and the test mode's ROM check see it */
flash_program_start = ADDR(.vectors);
flash_program_end = LOADADDR(.iram) + SIZEOF(.iram);
D_25626FC = LOADADDR(.data);
D_2562840 = LOADADDR(.iram);
sys_ram_data_start = ADDR(.data);
sys_ram_data_end = ADDR(.data) + SIZEOF(.data);
sys_ram_bss_start = ADDR(.dstram);
sys_iram_code = ADDR(.iram);
sys_iram_bss_start = ADDR(.bss);
sys_iram_bss_end = ADDR(.bss) + SIZEOF(.bss);
sys_heap_end = ADDR(.bss) + 0x400;

INCLUDE ldscripts/aliases.ld
