# Peripheral register inventory (FW_4UPlusJP.bin)

Method: every memory access in the disassembly (build/fw_code.dis for 0x2400000..0x2543000 plus
the internal-RAM code copied to 0x0..0xD08) was resolved with a constant-tracking scan
(work/mmio_scan.py in my worktree; raw list work/mmio.tsv, 1012 accesses, 136 addresses).
Addresses outside RAM (0..0x100000) and flash (0x2000000..0x27FFFFF) are listed. Register names
and bit meanings from the S1C33E07 Technical Manual (docs/id001557.pdf, Appendix A) and the
Epson BIOS headers (docs/33E07_Demo_Ver1.2_Release/include/BIOS/io_*.h; base addresses from
BIOS/src/system.c). Width = access width seen (B = 8 bit, H = 16, W = 32).

Consistency with the S1C33E07 manual: every register the code touches exists in the E07 map at
the documented address with plausible values; nothing is accessed that the E07 does not have
(no USB, no LCDC pixel registers, no DMA, no SPI data path). So the chip is an S1C33E07 or a
pin/register-compatible derivative (confidence: high).

## Memory map (as used)

| Range | Use | Evidence |
|---|---|---|
| 0x0000000-0x0004FFF | 20 KB internal RAM (A0RAM 8K + the 12K IVRAM moved to area 0 by pLCDC_IRAM=1). 0x0..0xD08 code copied from flash (sys_copy_iram_code), 0xD08..0x4AE4 .bss (zeroed by sys_clear_bss_iram), stack top 0x5000 | sys_reset clears 0..0x5000 and sets sp=0x5000 |
| 0x0084000-0x00847FF | 2 KB DST RAM: 0x84000..0x84474 bss (sys_clear_bss_ram), 0x84474..0x845B8 .data (copied from 0x25626FC by sys_copy_data); 0x845B8.. more bss | manual memory map "DST RAM (2KB)" |
| 0x0300000-0x030FFFF | on-chip peripherals (below) | |
| 0x0600000, 0x0600001 | external bus area 7 (#CE7): LCD controller command (0x600000) / data (0x600001) register, 8-bit (one 16-bit write of the command register in lcd_init/2409138/iram, same effect) | lcd_init_ili9163_old init sequence, iram blitters |
| 0x2000000-0x27FFFFF | 8 MB parallel NOR flash (16-bit; JEDEC/AMD command set at word offsets 0x555/0x2AA = byte 0xAAA/0x554; see iram func_0000A54/0B52/0C46). Code runs from here. | iram.s |
| 0xFFFFFFFF | one `ld.b [0xffffffff]` in food_restaurant_visit: looks like a (bogus) store through a -1 pointer, not MMIO | mmio.tsv |

## Clock Management Unit (0x301B00-0x301B24, 32-bit)

| Addr | Register | Acc | Value / meaning | Functions |
|---|---|---|---|---|
| 0x301B00 | pCMU_GATEDCLK0 | W | 0xF then 8 (DSTRAM_CKE on, LCDC/SDRAM/USB clocks off) | sys_reset (reset) |
| 0x301B04 | pCMU_GATEDCLK1 | RW W | 0x0A07F1AD at reset, then 0x0A07E12D (clocks for RTC, ITC, ADC, EFSIO, GPIO, WDT, TM0/2/3/4/5, SRAMC...; TM1 (bit14), SPI (bit6), CARD, DCSIO, I2S off). Bit 0 (RTCSAPB_CKE) toggled around RTC register writes (rtc_irq_rearm); byte 0x301B04 bit6 (SPI_CKE) set and byte 0x301B06 bit3 (TM1_CKE, bit 19) cleared by spi_clock_on | 24F6256, 25029FE, 250DF1A |
| 0x301B08 | pCMU_CLKCNTL | RW | 0x00770003: OSC3 and OSC1 on, OSCSEL=OSC3, OSC3DIV=/1, MCLKDIV=/1, LCDCDIV=7, PLLINDIV=7. sys_sleep_osc1 writes 0x771007 then 0x771005 (switch to OSC1 32 kHz = low-power mode) and sys_sleep_osc3 restores 0x770003 (OSC3). sys_clock_osc3 also writes it. Read by 250DB42/251A566/251A5B4/251A650 to pick timer constants for the current clock | 24F6256, 24F714C, 24F717E, 24F71B0, 2504984, 250DB42, 251A566, 251A5B4, 251A650 |
| 0x301B0C | pCMU_PLL | W | 0x00101804 (PLL off, PLLV=2) | 24F6256 |
| 0x301B14 | pCMU_OPT | W | 0x5C00 (OSCTM=0x5C wait timer) then 1 (WAKEUPWT); 0 while sleeping | 24F6256, 24F714C, 24F717E |
| 0x301B24 | pCMU_PROTECT | W | 0x96 unlock / 0 lock around every CMU write | all of the above |

MCLK = OSC3 undivided. From the UART baud reloads (0xC3 -> 9600 baud = the PN512 power-on
rate, 3 -> 460800, 1 -> 921600 with baud = MCLK/(16(n+1))) OSC3 is about 29.49 MHz (29.4912
MHz crystal, confidence medium-high). OSC1 = 32.768 kHz (RTC).

## Interrupt controller ITC (0x300260-0x3002AF, 8-bit)

Initial setup sys_itc_init: all enables 0 except pINT_EK01_EP03=7 (port inputs 0,1,2 = the three
buttons), pINT_E16T23=0x40 (timer 3 cmp B), pINT_E16T45=0x40 (timer 5 cmp B); all flags
cleared with 0xFF; priorities: port 0/1 = 3/3 (0x33), port 2/3 = 3/0, timers 2-3 = 0/3 (0x30),
timers 4-5 = 0/3, SIO1/AD = 7/3 (0x37), RTC = 3, port 12-13 = 3.

| Addr | Register | Acc | Use | Functions |
|---|---|---|---|---|
| 0x300260 | pINT_PP01L | W | 0x33 | 24F63B6 |
| 0x300261 | pINT_PP23L | W | 3 | 24F63B6 |
| 0x300267 | pINT_P16T23 | W | 0x30 | 24F63B6 |
| 0x300268 | pINT_P16T45 | RW | 0x30; timer 4 level set to 3 by the tick timer setup | 24F63B6, 253520C |
| 0x30026A | pINT_PSIO1_PAD | W | 0x37 (SIO1 level 7 = highest, A/D level 3) | 24F63B6 |
| 0x30026B | pINT_PRTC | W | 3 | 24F63B6 |
| 0x30026C | pINT_PP45L | W | 4 | 2504844 |
| 0x300270 | pINT_EK01_EP03 | RW | 7 = enable port-input 0,1,2 (buttons); written 0/7 to mask buttons | 24F63B6, 24FF904, 25050AC, 2519748, 2528ACE, 2528AE0 |
| 0x300271 | pINT_EDMA | W | 0 | 24F63B6 |
| 0x300272 | pINT_E16T01 | W | 0 | 24F63B6 |
| 0x300273 | pINT_E16T23 | RW | 0x40 (E16TU3); bit7 (E16TC3) set/cleared by the NFC timeout timer (252D2FC/252D340) | 24F63B6, 252D2FC, 252D340 |
| 0x300274 | pINT_E16T45 | RW | 0x40 (E16TU5); low nibble (timer 4 cmp A/B) set to 4 by 253520C, written by sleep/wake code | 24F63B6, 2409C76, 250325E, 25048D0, 2505112, 2505164, 250BC60, 250DF58, 253520C, 2535270 |
| 0x300276 | pINT_ESIF01 | RW | SIO1 RX/ERR enable (bits 3,4 = 0x18) and TX empty (bit 5) toggled by the NFC UART driver | 24F63B6, 252C5CE, 252C8CA, 252CBEA, 252CE54, 252D204, 252D3F2 |
| 0x300277 | pINT_EP47_ERTC_EAD | RW | bit1 EADE (A/D complete), bit2 ERTC, bit3 EP4 set/cleared by ADC, RTC, sleep code | 240999A, 2409C76, 240C51C, 2502298, 2502A3E, 2502A56, 25049DE, 2504A14, 250BC60, 250DF58, 250FC10, 2514846, 2519748, 251A062, 251A41E, 24F63B6 |
| 0x300278 | pINT_ELCDC | W | 0 | 24F63B6 |
| 0x300279 | pINT_ESIF2_ESPI | W | 0 | 24F63B6 |
| 0x300280 | pINT_FK01_FP03 | RW | flag clear (1/2/4) in the three button ISRs; 0xFF at init | 24F63B6, 24FF904, 25000F4, 25001E4, 25002D6, 25050AC, 2528AE0 |
| 0x300281 | pINT_FDMA | W | 0xFF | 24F63B6 |
| 0x300282 | pINT_F16T01 | W | 0xFF | 24F63B6 |
| 0x300283 | pINT_F16T23 | W | 0x40 in ISR 25000C4 (timer 3 cmp B), 0x80 in ISR 252D366 (timer 3 cmp A) | 24F63B6, 25000C4, 252D2FC, 252D340, 252D366 |
| 0x300284 | pINT_F16T45 | W | 0x40 in ISR 25000DC (timer 4 cmp B), 0x80 in 25051AE (timer 5 cmp A, halfword write), 12 in 253520C | 24F63B6, 25000DC, 25051AE, 2409C76, 25048D0, 2505112, 2505164, 250BC60, 250DF58, 253520C, 2535270, 25352A2 |
| 0x300286 | pINT_FSIF01 | W | 0x38 clear SIO1 flags; 0x08/0x10/0x20 in the SIO1 ERR/RX/TX ISRs | 24F63B6, 252C5CE, 252C8CA, 252CBEA, 252D204, 252D2A2, 252D2D6, 252D3F2 |
| 0x300287 | pINT_FP47_FRTC_FAD | W | 2 = A/D done flag clear (ISR 25003FA), 4 = RTC flag clear (ISR 24FF966); 0xFF | 24F63B6, 24FF966, 25003FA, 2504844, 250489C, 25049DE |
| 0x300288 | pINT_FLCDC | W | 0xFF | 24F63B6 |
| 0x300289 | pINT_FSIF2_FSPI | W | 0xFF | 24F63B6 |
| 0x300291 | pIDMAREQ_R16T14 | RW | bit6 (R16TU4) cleared: timer 4 cmp B -> interrupt, not IDMA | 253520C |
| 0x3002A2 | pINT_PP1213L | W | 3 (port input 12 level 3 = NFC IRQ) | 24F63B6 |
| 0x3002A6 | pINT_EP815 | RW | bit4 EP12 (NFC IRQ pin) enable/disable | 24F63B6, 252CBEA, 252D3F2 |
| 0x3002A7 | pINT_EI2S | W | 0 | 24F63B6 |
| 0x3002A9 | pINT_FP815 | W | 0x10 = clear FP12 (NFC IRQ) | 24F63B6, 252C5A6, 252CBEA, 252CFD8, 252D3F2 |
| 0x3002AA | pINT_FI2S | W | 0xFF | 24F63B6 |

Vector table (asm/vectors.s, base 0x2400000), handlers that are not the halt stub sys_isr_halt_a:

| Vec | Source (manual table III.2.1.1.1) | Handler | What it does |
|---|---|---|---|
| 0 | reset | sys_reset | clocks, RAM clear, SRAMC, ports, sys_init (system init), nfc_session_clear, sys_main (main, never returns) |
| 1 | reserved | 2500052 | halt loop |
| 2 | ext exception | 2500058 | halt loop |
| 3 | undefined instruction | 250005E | halt; sets sys_trap_state=1, spins while ==1 (debug hook), then sys_trap_state++ |
| 6 | address misaligned | 250007E | same as 3 |
| 7 | NMI (watchdog) | 250009E | halt loop |
| 11 | illegal interrupt | 25000A4 | sys_trap_last = sys_trap_state ? sys_trap_state : 0x3F |
| 16 | port input 0 (P00) | 25000F4 | button A: debounce-polls P00 up to 0x600 times, counts btn_a_debounce, sets btn_events bit0, calls snd_play(1) (wake/beep?), sys_display_wake(1); clears FP0 |
| 17 | port input 1 (P01) | 25001E4 | button B (same, btn_b_debounce, bit1, snd_play(2)) |
| 18 | port input 2 (P02) | 25002D6 | button C (same, btn_c_debounce, bit2, snd_play(3)); long press (0x600) sets nfc_session=0xF and calls nfc_user_abort |
| 42 | timer 3 compare B | 25000C4 | tmr_flags |= 4, clear flag |
| 43 | timer 3 compare A | 252D366 | NFC timeout tick: nfc_ticks++, calls nfc_request_abort when nfc_cancel_request==1 |
| 46 | timer 4 compare B | 253569E | saves ahr/alr, calls snd_sequencer_tick (sound sequencer step; timer 4 set up by snd_tick_start) |
| 50 | timer 5 compare B | 25000DC | tmr_flags |= 0x20, clear flag |
| 51 | timer 5 compare A | 25051A2 | jumps through RAM pointer ir_ram_routines (initialised to tmr_t5_tick_handler by sys_copy_ram_routines: tmr_t5_ticks++ and clear flag). A RAM-held ISR vector: candidate hook for loaded code |
| 60 | SIO ch1 receive error | 252D2D6 | ORs status errors (bits 2-4) into nfc_rx_fifo.x009 (NFC UART error byte) |
| 61 | SIO ch1 RX full | 252D2A2 | stores RXD1 into the NFC RX ring (struct at 0x4034: x014 = write ptr, x004 = count) |
| 62 | SIO ch1 TX empty | 252D204 | sends next byte from the NFC TX buffer (struct at 0x401C: x014 = read ptr, x004 = remaining), disables TX int when empty; also drains RX into the RX ring |
| 64 | A/D end of conversion | 25003FA | sys_battery_level = result (pAD_ADD then pAD_CH0_BUF), clears ADE/ADST/int bits, sys_adc_busy=0, clear FADE |
| 65 | RTC (1 s) | 24FF966 | the once-per-second game clock: care timers, sleep counter, RTC read, flags (pet_toilet_expired.., sys_awake_seconds..); clears pRTCINTSTAT and FRTC |
| 88 | port input 12 (P94 = PN512 IRQ) | 252C5A6 | nfc_field_irq = 1 (NFC IRQ seen), clear FP12, delay |

## I/O ports (0x300380-0x3003D5, 8-bit)

Pin assignment deduced from the function-select registers (sys_gpio_init, nfc_init, sound
code) and the ISRs:

| Pin | Function | Evidence |
|---|---|---|
| P00, P01, P02 | buttons A, B, C (inputs, pulled up, active low; port-input interrupts 0,1,2 on falling edge; SPT0..2 = P00..P02 via pPINTSEL_SPT03=0) | 25000F4/25001E4/25002D6 poll P0D bits 0/1/2, pPINTPOL_SPP07=0 (low/falling), pPINTEL_SEPT07=0x17 |
| P03 | output (IOC0=0x28), driven low (P0D=0xDF) | 24F644A, 2504844 |
| P04 / P05 | SIN1 / SOUT1 = UART to the NXP PN512 (pP0_47_CFP=5) | 24F644A, 252D3F2 |
| P06 | input, hidden test/debug switch (probably a test pad or the reset switch): counted low by sys_sample_debug_strap into sys_debug_strap_count on every idle tick; == 8 at boot selects a special start path in main; low at menu entry selects the debug variant of the communication menu (comm_main_menu, table comm_main_menu_debug_items with "デバックセット"/"アイテムSEND") | |
| P07 | input, unused? | |
| P10, P11, P12, P13 | TM0..TM3 timer outputs (pP1_03_CFP=0x15 at init, bits set to TMx by the tone functions 25352AE/2535304/253535A/25353B2) = piezo speaker (sound is 16-bit timer PWM; see Sound) | 25352AE etc. |
| P1 others | outputs (IOC1=0xFF), P1D=8 | 24F644A |
| P13 | GPIO bit 3/4 toggled by 252D3AC/252D3CC (bclr/bset P1D bits 3 and 4): PN512 reset (NRSTPD) / power control, pulsed low-high-low around the PN512 init (bit 4), bit 3 cleared/set | 252D3AC, 252D3CC |
| P2, P3, P4, P5, P6, P8 | all outputs, driven low (unused pins parked); P25/P24 high in sys_init (P2D=0, IOC2=0x30 then 0xFF) | 24F644A |
| P64 | toggled by 250DC50/250DC5A (bclr/bset P6D bit 4): vestigial "SPI flash CS" of the dead SPI-flash group 250DC50..250DF3C | |
| P70 | AIN0 (pP7_03_CFP=1): A/D channel 0 = battery voltage (ISR 25003FA, used by sys_battery_low low-battery check) | 24F644A, 24F61BC(0,0,..) ch 0 |
| P94 | input, pull-up off, routed to FPT12 (pPINTSEL_SPT1215 |= 3), low-active: PN512 IRQ | 252D3F2 |
| P9 others | outputs (IOC9=0xEF) | 24F644A |

| Addr | Register | Acc | Functions |
|---|---|---|---|
| 0x300380 | pP0_P0D | R (buttons, bits 0-2) W (0xDF) | 2401DD8, 240935A, 2409C76, 2412ABA, 241BDA8, 241C3D2, 24F644A, 24F87DE, 24F8832, 24FA926, 24FB30E, 24FB64E, 24FBB38, 24FE21C, 24FE82A, 24FF0BA, 24FF32A, 24FF928, 25000F4, 25001E4, 25002D6, 2504844, 250494C, 25049B2, 25049DE, 2504D6E, 2504E44, 2505034, 2505070, 250DF58, 2514274, 2519748, 251A26E, 251A364, 251A41E, 252A018 |
| 0x300381 | pP0_IOC0 | W 0x28 | 2409C76, 24F644A, 2504844 |
| 0x300382 | pP1_P1D | RW bits 3,4 | 24F644A, 252D3AC, 252D3CC |
| 0x300383 | pP1_IOC1 | W 0xFF | 24F644A |
| 0x300384/5 | pP2_P2D / pP2_IOC2 | W | 24F6316, 24F644A |
| 0x300386..0x300392 | P3..P9 data/IOC | W (init) | 24F644A |
| 0x30038C | pP6_P6D | RW bit4 | 250DC50, 250DC5A |
| 0x300393 | pP9_IOC9 | W 0xEF, bclr 4 | 24F644A, 252D3F2 |
| 0x3003A1 | pP0_47_CFP | RW | 24F644A, 2504844, 25049B2, 252D3F2 |
| 0x3003A2 | pP1_03_CFP | RW | 24F644A, 25352AE, 2535304, 253535A, 25353B2, 253545E, 2535488, 25354B2, 25354DE |
| 0x3003AE | pP7_03_CFP | W 1 | 24F644A |
| 0x3003B3 | pP9_47_CFP | RW | 252D3F2 |
| 0x3003C0 | pPINTSEL_SPT03 | W 0 | 24F644A |
| 0x3003C2 | pPINTPOL_SPP07 | W 0 | 24F644A, 2504844 |
| 0x3003C3 | pPINTEL_SEPT07 | W 0x17 | 24F644A, 2504844 |
| 0x3003C5 | pPINTSEL_SPT1215 | RW |= 3 | 252D3F2 |
| 0x3003C6 | pPINTPOL_SPP815 | bclr 4 | 252D3F2 |

## Misc (2) pull-ups (0x300C42-0x300C4B)

| Addr | Register | Value | Functions |
|---|---|---|---|
| 0x300C42 | pMISC_PUP0 | 0xEF (all P0 but P04=SIN1 pulled up) | 24F644A, 2504844 |
| 0x300C43 | pMISC_PUP1 | 0xFF | 24F644A |
| 0x300C45 | pMISC_PUP3 | 0x7F | 24F644A |
| 0x300C48 | pMISC_PUP6 | 0xFF | 24F644A |
| 0x300C49 | pMISC_PUP7 | 0x1F | 24F644A |
| 0x300C4A | pMISC_PUP8 | 0x3F | 24F644A |
| 0x300C4B | pMISC_PUP9 | 0xEF, bclr 4 (P94 no pull-up) | 24F644A, 252D3F2 |

## SRAM controller (0x301500-0x30150C, 32-bit) - sys_bus_setup

pSRAMC_BCLK_SETUP = 0x81 (CE9 BCLK/2, BCLK/2), pSRAMC_SWAIT = 0x72707777 (wait states
CE4=7, CE5=7, CE6=7, CE7=7, CE8=0, CE9=7, CE10=2, CE11=7), pSRAMC_SLV_SIZE = 0x4655 (CE4..CE7
16-bit, CE8 8-bit?, CE9 16-bit, CE11 8-bit), pSRAMC_A0_BSL = 0 (A0 type). The NOR flash is on
the chip-enable that decodes 0x2000000 (area 16/17, #CE10 on the E07: wait=2), CE7 = area 7 =
0x600000 LCD.

## 16-bit timers (0x300780-0x3007EA, 16-bit; several byte accesses to the low byte of CTLx)

tmr_init: pT16_ADVMODE=1 (advanced mode), timers 0,2,3,5 PRESET.

| Timer | Role | Setup | Users |
|---|---|---|---|
| T0 (0x300780 CR0A, 0x300782 CR0B, 0x300786 CTL0, 0x3007E0 CLKCTL0) | tone generator channel 1, output TM0 on P10 (speaker). CLKCTL0=11 (on, MCLK/16); CR0B=period, CR0A=duty; CTL0=0x34 (fine+buffered+inverted+PTM out) or 0x24; stop: 253545E/25355E6 | 25352AE, 2535542 (update period), 253545E (stop), 25355E6, 2535656 | 240C51C, 241BDA8, 24FE21C, 24FED98, 24FE82A, 24FF0BA, 24FF32A, 250B9AA, 250BC60, 250DF58 write CTL0/1/3/5=2 (reset) when entering sleep |
| T1 (0x300788/8A/8E, CLKCTL1 0x3007E2) | tone channel 2 -> TM1/P11 | 2535304, 2535554, 2535488, 25355F4 | |
| T2 (0x300790/92/94/96, CLKCTL2 0x3007E4) | tone channel 3 -> TM2/P12 AND the LCD backlight PWM on the same pin (sys_apply_backlight: period 0x44 at OSC3 / 0xF4 at OSC1, duty k*(9-level)/10, CLKCTL 0xA or 0xD, level 0 = reset + clock off) AND a free-running counter read as a time base (TC2 at 0x300794 read by 2410668, 2412366, 250DF58). sys_display_wake/251A650 program it | 253535A, 2535566, 25354B2, 2535602, 251A650 | |
| T3 (0x300798/9A/9E, CLKCTL3 0x3007E6) | tone channel 4 -> TM3/P13, and the NFC timeout timer (252D2FC start with CR3A/CR3B, compare-A interrupt vec 43; 252D340 stop), also 251A566 (delay with timer 3?) | 25353B2, 2535578, 25354DE, 2535610 | |
| T4 (0x3007A0 CR4A, 0x3007A2 CR4B, 0x3007A4 TC4, 0x3007A6 CTL4, CLKCTL4 0x3007E8) | sound sequencer tick: 253520C sets CLKCTL4=14 (on, MCLK/1024), CR4B=v, CR4A=0, compare-B interrupt (vec 46 -> 25356FA); 2535270 stops it; TC4 read by 2528CC0/2528CF8 (random seed?) | 253520C, 2535270, 250DB16, 250DB42, 250DB8E, 2409C76, 240C51C | |
| T5 (0x3007A8 CR5A, 0x3007AA CR5B, 0x3007AE CTL5, CLKCTL5 0x3007EA) | general timer: compare-A ISR via RAM vector ir_ram_routines (vec 51), compare-B vec 50 sets tmr_flags bit5. Programmed by 25048D0/2505112/2505164 (delay / timeout with CR5A/CR5B), 251A062, 251A5B4 (ms delay: tmr_t5_start_timeout(3000) in init), 2504C44/2504D6E/2504E44 (sleep wake-up timer), 25051CE | | |
| 0x3007DE | pT16_ADVMODE | W 1 | 24F64F4 |
| 0x3007E0..EA | pT16_CLKCTL_0..5 | W | as above |

## A/D converter (0x300520-0x30055E, 16-bit)

sys_adc_setup(0,0,0xF,0): pAD_ADVMODE |= 0x100 (advanced mode), pAD_CLKCTL = 0xF (clock on,
MCLK/256), pAD_TRIG_CHNL start=end=channel 0, software trigger, normal mode.
sys_adc_irq_setup(0,1): pAD_EN_SMPL_STAT: conversion-complete int enabled, bit 6 INTMODE=1,
pAD_CH04_INTMASK = 1 (channel 0). sys_adc_set_limits (unused?) sets the upper/lower limits.
Then `REG16(0x300544) &= ~6` (ADE/ADST off) and sys_battery_level = 0x3FF (last reading = full scale).
Conversion is started by sys_battery_low / test_battery_check (set ADE|ADST = 6, wait, read); the ISR
sys_adc_isr stores pAD_ADD / pAD_CH0_BUF into sys_battery_level. Channel 0 = AIN0 = P70: battery
voltage (sys_battery_low compares the reading with thresholds -> low-battery handling; confidence
medium).

| Addr | Register | Functions |
|---|---|---|
| 0x300520 | pAD_CLKCTL | 24F61BC |
| 0x300540 | pAD_ADD | 25003FA |
| 0x300542 | pAD_TRIG_CHNL | 24F61BC |
| 0x300544 | pAD_EN_SMPL_STAT | 24F61F2, 24F621C, 24F6316, 25003FA, 2502298, 251A062 |
| 0x300548 | pAD_CH0_BUF | 25003FA |
| 0x300558/5A | pAD_UPPER / pAD_LOWER | 24F61F2 |
| 0x30055C | pAD_CH04_INTMASK | 24F621C |
| 0x30055E | pAD_ADVMODE | 24F61BC |

## Serial interface ch.1 (0x300B10-0x300B17, 8-bit): UART to the NXP PN512

Only channel 1 is used (one read of 0x300B02 = pEFSIF0_STATUS in the SIO1 error ISR is
probably a copy/paste slip reading channel 0's status). Configuration (nfc_init and the
NFC command functions 252C5CE/252C8CA/252CBEA/252CFD8): pEFSIF1_CTL = 3 (8-bit async, 1 stop
bit, no parity, internal clock) then |= 0xC0 (TX+RX enable); pEFSIF1_IRDA = 0x10 (DIVMD1 = 1/8
clock divider); pEFSIF1_BRTRDL = 0xC3 (9600 baud, the PN512 default), later 3 (460800) or 1
(921600) after the PN512 SerialSpeedReg (0x1F) is set to 0x3A / 0x1C; pEFSIF1_BRTRUN 0/1.
Register access protocol = PN512 UART: address byte (bit 7 = read) then data byte, written to
pEFSIF1_TXD, answer read from pEFSIF1_RXD after a busy-wait (0x1387 / 0x7CF loops).

| Addr | Register | Acc | Functions |
|---|---|---|---|
| 0x300B10 | pEFSIF1_TXD | W | 252C5CE, 252C8CA, 252CBEA, 252CE54, 252CFD8, 252D204 (TX ISR), 252D3F2 |
| 0x300B11 | pEFSIF1_RXD | R | 252C5CE, 252C8CA, 252CBEA, 252CFD8, 252D204, 252D2A2 (RX ISR), 252D3F2 |
| 0x300B12 | pEFSIF1_STATUS | R | bit0 RDBF1 polled | 252C5CE, 252C8CA, 252CBEA, 252CE54, 252CFD8, 252D204 |
| 0x300B13 | pEFSIF1_CTL | RW | 252C5CE, 252C8CA, 252CBEA, 252CFD8, 252D3F2 |
| 0x300B14 | pEFSIF1_IRDA | W 0x10 | 252D3F2 |
| 0x300B15 | pEFSIF1_BRTRUN | W | 252C5CE, 252C8CA, 252CBEA, 252CFD8, 252D3F2 |
| 0x300B16 | pEFSIF1_BRTRDL/M | W (halfword) | 252C5CE, 252C8CA, 252CBEA, 252D3F2 |
| 0x300B02 | pEFSIF0_STATUS | R | 252D2D6 (SIO1 error ISR; reads ch.0 status, probably a bug) |

## SPI (0x301700-0x30171C)

Only pSPI_CTL1 (0x301708) is read-modify-written by spi_enable / spi_disable (the dead
SPI-flash group 250DC50..250DF3C; the byte-transfer helpers are stubs). No TXD/RXD access
anywhere: the SPI block is not used for anything real. spi_clock_on gates SPI_CKE on, TM1_CKE
off.

## Real-time clock (0x301900-0x301928, 32-bit)

rtc_reset (RTC init): pRTCINTMODE=0, pRTCINTSTAT=1 (clear), pRTC_CNTL0 = 0x11 (24h + reset),
0x10, rtc_acquire (hold: pRTC_CNTL1 = 1, wait busy, ...), CNTL0 = 0x12 (stop), write
sec/min/hour/day/month from rtc_second/1222/1221/1220/121F, year=8 (BCD; the counters are
BCD), day-of-week = rtc_last_weekday, CNTL1 = 0 (release hold), CNTL0 = 0x10 (run), pRTCINTMODE = 6
(1-second interrupt, level sense). rtc_set_time writes the time (set clock), rtc_read_clock reads
it (BCD -> D_0001xxx fields), rtc_irq_rearm re-arms the 1 s interrupt. ISR = vector 65
rtc_isr. ir_recv_packet/2504D6E/2504E44 change pRTCINTMODE (e.g. 1-minute cycle while
sleeping) and rtc_irq_off/250B540 set it to fixed values.

| Addr | Register | Functions |
|---|---|---|
| 0x301900 | pRTCINTSTAT | 24FF966, 250B46A |
| 0x301904 | pRTCINTMODE | 25029FE, 250325E, 2504C44, 2504D6E, 2504E44, 25051CE, 250B46A, 250B534, 250B540 |
| 0x301908 | pRTC_CNTL0 | 250B46A |
| 0x30190C | pRTC_CNTL1 | 250B46A, 250B51A, 250B5A6, 250B6DC |
| 0x301910..0x301924 | SEC, MIN, HOUR, DAY, MONTH, YEAR | 250B46A, 250B5A6 (write), 250B6DC (read) |
| 0x301928 | pRTCDAYWEEK | 250B46A, 250B6DC |

## LCD controller block (0x301A00-0x301AAC)

Only pLCDC_IRAM (0x301A64) is written: 1 at reset (IVRAM moved to area 0 as general RAM) and
by spi_flash_setup. The on-chip LCDC is NOT used; the display is an external controller on the
bus (0x600000/1).

## External LCD controller (0x600000 cmd, 0x600001 data)

ST7735-class 128x128 TFT. Writers: lcd_init_ili9163_old (init sequence: 0x01 SWRESET, packed
command/parameter table, 0x11 SLPOUT, 0xE0/0xE1 gamma, 0x29 DISPON), lcd_init (window +
RAMWR with the 0x2A/0x2B/0x2C commands and a fill), lcd_init_st7735 (another long
command/parameter sequence: alternate panel init or sleep/wake), sys_display_off, sys_display_wake
(display on/off), sys_power_down (sleep), and the internal-RAM blitters func_00006B8 /
func_000081A (2A column set, 2B row set, 2C memory write, pixel bytes from the palette tables
at 0x3C10/0x3D50/0x3E90 = R/G/B 8-bit tables of a 256-entry palette (+0x100 = second palette),
line buffer at 0x1810). The LCD data path is 3 bytes (R,G,B) per pixel = 18-bit colour mode.

## Watchdog, DMA, USB, I2S, DCSIO, SDRAMC, Card, Misc(1)

Not touched at all. (WDT_CKE stays on in GATEDCLK1 but 0x300660.. is never written: the watchdog
is left at its reset state = stopped.)
