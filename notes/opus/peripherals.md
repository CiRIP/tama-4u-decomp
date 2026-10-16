# Peripheral register inventory (FW_4UPlusJP.bin)

Method: `work/mmio.py` (in the opus worktree) walks every function of the code region
(build/funcs.csv boundaries) plus the internal-RAM code (flash 0x2562840 -> 0x0..0xD08),
tracks registers holding constants (`xld.w`/`ld.w` imm, `add`/`sub` imm, register copies;
r4..r15 forgotten at calls) and records every load/store/bit-op whose address is outside
RAM/flash. Data was also scanned for words in 0x300000..0x30FFFF: only the per-file
`.rodata` marker 0x00300010 shows up, so no register addresses come from tables.
Indirect accesses: `ir_port_ptr` (RAM) holds 0x300380 (P0 data, set by sys_clock_osc3) and is
used by the IR carrier routine run from RAM (ir_carrier_burst copied to 0xD58).
Register names: S1C33E07 Technical Manual (docs/id001557.pdf). Every register the code uses
exists at the same address in the E07 manual, with plausible bit usage; nothing contradicts
the E07 map (no accesses to registers the E07 doesn't have). Confidence that the chip is an
E07 or a pin-compatible E0x variant: high.

Access codes: Rb/Rh/Rw = read byte/half/word, Wb/Wh/Ww = write, bsetN/bclrN = bit
read-modify-write on bit N. Function addresses < 0x1000 are the internal-RAM routines.

## Summary by block (interpretation; evidence in the table below)

| block | addresses | what the firmware does with it | conf. |
|---|---|---|---|
| SRAMC (external bus) | 0x301500-0x30150C | set once at reset (sys_bus_setup): BCLK/setup 0x81, waits 0x72707777, device sizes 0x4655, A0/BSL 0. Configures the chip-select areas for the NOR flash and the LCD controller at 0x600000 | high |
| CMU (clocks) | 0x301B00-0x301B24 | reset: GATEDCLK0=8 (only DST RAM clock), GATEDCLK1=0xA07F1AD (switches off unused SPI/CARD/DMA... clocks), PLL control written but PLL left off; CLKCNTL=0x770003 = system clock OSC3 /1, both oscillators on. sys_sleep_osc1 = enter low-speed mode (CLKCNTL 0x771007 -> OSC1 32 kHz + MCLK/2, `slp`, then 0x771005 = OSC3 off). sys_sleep_osc3 / 24F71B0 / 2504984 = back to OSC3 (0x770003, OSC stabilisation wait 0x5C). All writes are bracketed by CMU_PROTECT=0x96 / 0. Timer code tests CLKCNTL bit 1 (OSC3 on) and bit 12 (MCLK/2) to pick timer reload constants | high |
| ITC | 0x300260-0x3002AA | sys_itc_init sets priorities (port 0-1 = 3/3, port 2-3 = 3, T16 2-3 = 3 (ch.3), T16 4-5 = 3 (ch.5), SIF1/AD = 3/7?, RTC = 3, port 12-13 = 3), clears all flags, enables only ports 0-2 (0x300270=7), T16 ch.3 B (0x40), T16 ch.5 B (0x40). Others enabled on demand (see below) | high |
| P0 | 0x300380/81, PUP0 0x300C42, CFP 0x3003A1 | P00/P01/P02 = the three buttons (inputs, pull-ups, active low, port-input interrupts 0/1/2, edge). P03 output (IR receiver enable?), P04/P05 = SIN1/SOUT1 of serial Ch.1 (NFC front-end UART) when CFP=5, or GPIO for the IR link (P05 = IR LED, P04 = IR receiver input) when CFP=0. P07 input = LCD panel type strap read by lcd_init | high (buttons, UART), medium (IR pins), medium (P07 strap) |
| P1 | 0x300382/83, CFP 0x3003A2 | P10-P13 can be TM0-TM3 timer outputs (sound driver snd_ch0_on.. sets CFP bits per channel, P12 = TM2 = LCD backlight PWM). P13/P14 GPIO pulses in nfc_hw_power_on/252D3CC around the NFC front-end power-up/down | high (TMx), low (P13/P14 meaning) |
| P2..P9 | 0x300384-0x300393, PUPx | set once in sys_gpio_init (mostly outputs driven low = unused pins). P64 (0x30038C bit 4) = chip select of a (vestigial) SPI device, toggled by spi_cs_assert/250DC5A. P94 (0x300392 bit 4) = NFC front-end IRQ input (port-input interrupt 12, vector 88) | high (init), medium (P64), high (P94 as NFC IRQ) |
| Port input interrupt select | 0x3003C0-0x3003C6 | FPT0-3 from P0x, FPT0-2 + FPT4 edge-triggered, active low; FPT12 selected from P9x (0x3003C5 \|= 3) with low polarity for the NFC IRQ | high |
| A/D | 0x300520-0x30055E | battery voltage measurement: sys_adc_setup/24F61F2/24F621C configure, conversion started by setting 0x300544 bits 1/2, end-of-conversion interrupt (vector 64, sys_adc_isr) stores the result in sys_battery_level. sys_battery_low = battery-low test (result <= 0x2E3 in 5 of 10 samples); test_battery_check = battery check / dead-battery screen | high |
| T16 Ch.0-3 | 0x300780-0x30079E, CLKCTL 0x3007E0-E6 | Ch.0-3 PWM tone generators for sound (snd_ch0_on/2535304/253535A/25353B2 enable the TMx pin function and start the timer; 2535542.. set the period/duty; 253545E.. stop). Ch.2 is also the LCD backlight PWM (sys_display_wake, sys_apply_backlight: duty from brightness level sys_backlight_request); its counter (0x300794) is read as a random seed (pet_event_random_daily, 2412366). Ch.3 = one-shot ms timeout (tmr_t3_start_timeout, interrupt 42 sets tmr_flags bit 2) and NFC receive timeout (nfc_tick_start, compare-A interrupt 43 = nfc_tick_isr) | high |
| T16 Ch.4 | 0x3007A2-0x3007A6, CLKCTL 0x3007E8 | sound sequencer tick: snd_tick_start starts it, compare-B interrupt 46 (snd_tick_isr -> snd_sequencer_tick) runs the music/sfx engine; IDMA request bit for T16 ch.4 (0x300291 bit 6) explicitly cleared | high |
| T16 Ch.5 | 0x3007A8-0x3007AE, CLKCTL 0x3007EA | general delay/timeout (tmr_t5_start_timeout: t ms -> interrupt 50 sets tmr_flags bit 5), IR bit timing (tmr_t5_start_tick/2505112: compare-A interrupt 51 = tmr_t5_cmpa_vector jumps to a handler copied into RAM at 0xD94), 10 Hz-ish tick tmr_t5_standard_tick | high |
| EFSIF Ch.1 (UART) | 0x300B10-0x300B16 | NFC front-end link (PN512-style register protocol, see NOTES.md): 8N1 async, baud reload 0xC3 (~9600 bd at ~30 MHz) at start, later 3 or 1 (~460.8k / ~921.6k bd after writing the PN512 SerialSpeedReg 0x1F). Interrupts: rx error 60 (nfc_uart_err_isr), rx full 61 (nfc_uart_rx_isr), tx empty 62 (nfc_uart_tx_isr) | high |
| EFSIF Ch.0 | 0x300B02 | only read by the Ch.1 receive-error handler nfc_uart_err_isr (reads the STATUS of Ch.0 instead of Ch.1: probably a bug; the flags of Ch.1 are cleared via 0x300286=8) | medium |
| SPI | 0x301708 | only SPI_CTL1 bit 0 (enable) set/cleared by spi_enable/250DC42; no data register access: the SPI-flash code group is vestigial | high |
| RTC | 0x301900-0x301928 | clock: rtc_reset resets the time (BCD registers), rtc_set_time sets h:m:s, rtc_read_clock reads h:m:s and uses the day-of-week counter to detect day changes (the firmware keeps day/month itself, year register fixed to 8). Interrupt 65 (rtc_isr, RTCINTMODE = 6/7 i.e. 1-second/1-minute) is the game's 1 Hz tick | high |
| LCDC IRAM select | 0x301A64 | =1 at reset and in spi_flash_setup: gives the LCDC's internal RAM area to the CPU (the on-chip LCD controller is not used; the display is an external controller) | medium |
| LCD controller | 0x600000 (A0=0 command), 0x600001 (A0=1 data) | external 128x128 TFT controller on the bus (8-bit writes; 16-bit writes put a command at 0x600000 and its parameter at 0x600001 in one access). Two init sequences selected by P07: lcd_init_st7735 (Sitronix ST7735(S): B1-B4 frame rate/inversion, C0-C5 power, E0/E1 gamma, 36 MADCTL, 11 sleep out, 29 on) and an inline one in lcd_init (packed cmd+param pairs C0/C2/C4C5/C6 13/36 C0 + E0/E1 gamma: another controller). lcd_init_ili9163_old = an unused older variant of the second one. Drawing: RAM routines 0x4C8/0x7A2 send 2A (column), 2B (row), 2C (memory write) then pixel data; 0x10 = sleep in (sys_display_off), 0x11 = sleep out | high |
| NOR flash | 0x2000000 + 0xAAA / 0x554 | AMD/JEDEC command set in 16-bit mode from RAM routines: erase 0x0A54 (AA 55 80 AA 55 30 per sector, sector table flash_sector_offsets: 8 x 8 KB + 127 x 64 KB = 8 MB bottom-boot device), program 0x0B52 / 0x0C46 (AA 55 A0 data), status by DQ7 data polling and DQ5 timeout, F0 reset | high |

Not used at all (no access found): watchdog, HSDMA, IDMA (except clearing a request bit),
on-chip LCDC (except IRAM select), I2S, USB, DCSIO, SDRAMC, card I/F, serial Ch.0/Ch.2
(except the one Ch.0 STATUS read), key-input interrupts.

## Interrupt vector table (0x2400000, TTBR never changed)

| vector | cause (E07) | handler | what it does |
|---|---|---|---|
| 0 | reset | sys_reset | boot (clocks, SRAMC, stack 0x5000, clear IRAM 0..0x5000, init chain, main) |
| 2 | ext exception | sys_trap_halt | halt + hang |
| 3 | undefined instruction | sys_trap_wait | halt, set sys_trap_state=1, spin while 1 |
| 6 | address misaligned | sys_address_error_trap | same as 3 |
| 7 | NMI | sys_nmi_trap | halt + hang |
| 11 | illegal interrupt | sys_trap_record | records sys_trap_state (or 0x3F) into sys_trap_last |
| 16, 17, 18 | port input 0/1/2 | btn_a_isr / 25001E4 / 25002D6 | buttons A/B/C (P00/P01/P02): debounce by polling the pin up to 0x600 times, wake backlight (sys_display_wake(1)), key-click sound (snd_play), count presses in btn_a_presses/1421/1422 (capped at 100), set bit 0/1/2 of the button event byte btn_events, clear the flag |
| 42 | T16 ch.3 compare B | tmr_t16_23_isr | tmr_flags \|= 4 (timeout elapsed) |
| 43 | T16 ch.3 compare A | nfc_tick_isr | NFC receive timeout |
| 46 | T16 ch.4 compare B | snd_tick_isr | saves AHR/ALR, calls the sound engine snd_sequencer_tick |
| 50 | T16 ch.5 compare B | tmr_t16_45_isr | tmr_flags \|= 0x20 (delay elapsed) |
| 51 | T16 ch.5 compare A | tmr_t5_cmpa_vector | jumps to the RAM copy at 0xD94 of tmr_t5_tick_handler (tmr_t5_ticks++, the IR bit-timer tick) |
| 60, 61, 62 | serial Ch.1 rx error / rx full / tx empty | nfc_uart_err_isr / 252D2A2 / 252D204 | NFC UART driver |
| 64 | A/D end of conversion | sys_adc_isr | sys_battery_level = result, sys_adc_busy = 0 |
| 65 | RTC | rtc_isr | the 1 Hz game tick (needs, timers, clock, ...) |
| 88 | port input 12 | nfc_irq_isr | NFC front-end IRQ (P94) |
| all others | - | sys_isr_halt_a | halt + hang |

ir_edge_isr (port input 4 handler: IR receive edge counter ir_edge_count/DB8) and
ir_port_setup (IR port setup with port-input-4 interrupt) are present but not referenced:
dead code from an interrupt-driven IR receiver.

## Full table (generated)

| address | register (S1C33E07 manual) | access | functions | constant values written (func=value) |
|---|---|---|---|---|
| 0x300260 | Port Input 0–1 Interrupt Priority Register (pINT_PP01L) | Wb | Wb: 24F63B6 | 24F63B6=33 |
| 0x300261 | Port Input 2–3 Interrupt Priority Register (pINT_PP23L) | Wb | Wb: 24F63B6 | 24F63B6=3 |
| 0x300267 | 16-bit Timer 2–3 Interrupt Priority Register (pINT_P16T23) | Wb | Wb: 24F63B6 | 24F63B6=30 |
| 0x300268 | 16-bit Timer 4–5 Interrupt Priority Register (pINT_P16T45) | Rb Wb | Rb: 253520C<br>Wb: 24F63B6 253520C | 24F63B6=30 |
| 0x30026A | Serial I/F Ch.1, A/D Interrupt Priority Register (pINT_PSI01_PAD) | Wb | Wb: 24F63B6 | 24F63B6=37 |
| 0x30026B | RTC Interrupt Priority Register (pINT_PRTC) | Wb | Wb: 24F63B6 | 24F63B6=3 |
| 0x30026C | Port Input 4–5 Interrupt Priority Register (pINT_PP45L) | Wb | Wb: 2504844 | 2504844=4 |
| 0x300270 | Key Input, Port Input 0–3 Interrupt Enable Register (pINT_EK01_EP03) | Rb Wb | Rb: 2528ACE 2528AE0<br>Wb: 24F63B6 24FF904 25050AC 2519748 2528ACE 2528AE0 | 24F63B6=7 24FF904=7 25050AC=0 2519748=1 |
| 0x300271 | DMA Interrupt Enable Register (pINT_EDMA) | Wb | Wb: 24F63B6 | 24F63B6=0 |
| 0x300272 | 16-bit Timer 0–1 Interrupt Enable Register (pINT_E16T01) | Wb | Wb: 24F63B6 | 24F63B6=0 |
| 0x300273 | 16-bit Timer 2–3 Interrupt Enable Register (pINT_E16T23) | Wb bclr7 bset7 | Wb: 24F63B6<br>bclr7: 252D340<br>bset7: 252D2FC | 24F63B6=40 |
| 0x300274 | 16-bit Timer 4–5 Interrupt Enable Register (pINT_E16T45) | Rb Wb bset2 | Rb: 250325E 25048D0 2505112 253520C 2535270<br>Wb: 2409C76 24F63B6 250325E 25048D0 2505112 2505164 250BC60 250DF58 253520C 2535270<br>bset2: 253520C | 2409C76=44 24F63B6=40 250325E=0 2505164=44 250BC60=44 250DF58=44 |
| 0x300276 | Serial I/F Ch.0–1 Interrupt Enable Register (pINT_ESIF01) | Rb Wb bclr5 bset5 | Rb: 252C5CE 252C8CA 252CBEA 252D3F2<br>Wb: 24F63B6 252C5CE 252C8CA 252CBEA 252D3F2<br>bclr5: 252CE54 252D204<br>bset5: 252CE54 | 24F63B6=0 |
| 0x300277 | Port Input 4–7, RTC, A/D Interrupt Enable Register (pINT_EP47_ERTC_EAD) | Rb Wb bclr2 bclr3 bset1 bset2 bset3 | Rb: 2502298<br>Wb: 240999A 2409C76 240C51C 24F63B6 2502298 250BC60 250DF58 2519748 251A062 251A41E<br>bclr2: 2502A3E 250FC10<br>bclr3: 25049DE<br>bset1: 2502298<br>bset2: 2502A56 250FC10 2514846<br>bset3: 2504A14 | 240999A=4 240999A=6 2409C76=4 2409C76=6 240C51C=0 240C51C=4 24F63B6=0 250BC60=6 250DF58=6 2519748=0 251A062=0 251A062=2 251A41E=0 251A41E=6 |
| 0x300278 | LCDC Interrupt Enable Register (pINT_ELCDC) | Wb | Wb: 24F63B6 | 24F63B6=0 |
| 0x300279 | Serial I/F Ch.2, SPI Interrupt Enable Register (pINT_ESIF2_ESPI) | Wb | Wb: 24F63B6 | 24F63B6=0 |
| 0x300280 | Key Input, Port Input 0–3 Interrupt Cause Flag Register (pINT_FK01_FP03) | Rb Wb | Rb: 2528AE0<br>Wb: 24F63B6 24FF904 25000F4 25001E4 25002D6 25050AC 2528AE0 | 24F63B6=FF 24FF904=7 25000F4=1 25001E4=2 25002D6=4 25050AC=FF |
| 0x300281 | DMA Interrupt Cause Flag Register (pINT_FDMA) | Wb | Wb: 24F63B6 | 24F63B6=FF |
| 0x300282 | 16-bit Timer 0–1 Interrupt Cause Flag Register (pINT_F16T01) | Wb | Wb: 24F63B6 | 24F63B6=FF |
| 0x300283 | 16-bit Timer 2–3 Interrupt Cause Flag Register (pINT_F16T23) | Wb | Wb: 24F63B6 25000C4 252D2FC 252D340 252D366 | 24F63B6=FF 25000C4=40 252D2FC=80 252D340=80 252D366=80 |
| 0x300284 | 16-bit Timer 4–5 Interrupt Cause Flag Register (pINT_F16T45) | Wb Wh | Wb: 2409C76 24F63B6 25000DC 25048D0 2505112 2505164 250BC60 250DF58 253520C 2535270 25352A2<br>Wh: 25051AE | 2409C76=40 24F63B6=FF 25000DC=40 25048D0=80 2505112=80 2505164=C0 25051AE=80 250BC60=40 250DF58=40 253520C=C 2535270=C 25352A2=C |
| 0x300286 | Serial I/F Ch.0–1 Interrupt Cause Flag Register (pINT_FSIF01) | Wb | Wb: 24F63B6 252C5CE 252C8CA 252CBEA 252D204 252D2A2 252D2D6 252D3F2 | 24F63B6=FF 252C5CE=38 252C8CA=38 252CBEA=38 252D204=10 252D204=20 252D2A2=10 252D2D6=8 252D3F2=38 |
| 0x300287 | Port Input 4–7, RTC, A/D Interrupt Cause Flag Register | Wb | Wb: 24F63B6 24FF966 25003FA 2504844 250489C 25049DE | 24F63B6=FF 24FF966=4 25003FA=2 2504844=FF 250489C=8 25049DE=0 |
| 0x300288 | LCDC Interrupt Cause Flag Register (pINT_FLCDC) | Wb | Wb: 24F63B6 | 24F63B6=FF |
| 0x300289 | Serial I/F Ch.2, SPI Interrupt Cause Flag Register (pINT_FSIF2_FSPI) | Wb | Wb: 24F63B6 | 24F63B6=FF |
| 0x300291 | 16-bit Timer 1–4 IDMA Request Register (pIDMAREQ_R16T14) | bclr6 | bclr6: 253520C |  |
| 0x3002A2 | Port Input 12–13 Interrupt Priority Register (pINT_PP1213L) | Wb | Wb: 24F63B6 | 24F63B6=3 |
| 0x3002A6 | Port Input 8–15 Interrupt Enable Register (pINT_EP815) | Wb bclr4 bset4 | Wb: 24F63B6<br>bclr4: 252CBEA<br>bset4: 252CBEA 252D3F2 | 24F63B6=0 |
| 0x3002A7 | I2S Interrupt Enable Register (pINT_EI2S) | Wb | Wb: 24F63B6 | 24F63B6=0 |
| 0x3002A9 | Port Input 8–15 Interrupt Cause Flag Register (pINT_FP815) | Wb | Wb: 24F63B6 252C5A6 252CBEA 252CFD8 252D3F2 | 24F63B6=FF 252C5A6=10 252CBEA=10 252CFD8=10 252D3F2=10 |
| 0x3002AA | I2S Interrupt Cause Flag Register (pINT_FI2S) | Wb | Wb: 24F63B6 | 24F63B6=FF |
| 0x300380 | P0 D (port data) | Rb Wb | Rb: 2401DD8 240935A 2412ABA 241BDA8 241C3D2 24F87DE 24F8832 24FA926 24FB30E 24FB64E 24FBB38 24FE21C 24FE82A 24FF0BA 24FF32A 24FF928 25000F4 25001E4 25002D6 250494C 25049B2 2505034 2505070 250DF58 2514274 2519748 251A26E 251A364 251A41E 252A018<br>Wb: 2409C76 24F644A 2504844 25049B2 25049DE 2504D6E 2504E44 | 2409C76=5F 24F644A=DF 2504844=DF 25049B2=0 25049DE=18 2504D6E=1F 2504E44=1F |
| 0x300381 | P0 IOC (I/O direction) | Wb | Wb: 2409C76 24F644A 2504844 | 2409C76=A8 24F644A=28 2504844=28 |
| 0x300382 | P1 D (port data) | Wb bclr3 bclr4 bset3 bset4 | Wb: 24F644A<br>bclr3: 252D3AC<br>bclr4: 252D3AC 252D3CC<br>bset3: 252D3CC<br>bset4: 252D3AC 252D3CC | 24F644A=8 |
| 0x300383 | P1 IOC (I/O direction) | Wb | Wb: 24F644A | 24F644A=FF |
| 0x300384 | P2 D (port data) | Wb | Wb: 24F6316 24F644A | 24F6316=0 24F644A=0 |
| 0x300385 | P2 IOC (I/O direction) | Wb | Wb: 24F6316 24F644A | 24F6316=30 24F644A=FF |
| 0x300386 | P3 D (port data) | Wb | Wb: 24F644A | 24F644A=0 |
| 0x300387 | P3 IOC (I/O direction) | Wb | Wb: 24F644A | 24F644A=FF |
| 0x300388 | P4 D (port data) | Wb | Wb: 24F644A | 24F644A=0 |
| 0x300389 | P4 IOC (I/O direction) | Wb | Wb: 24F644A | 24F644A=FF |
| 0x30038A | P5 D (port data) | Wb | Wb: 24F644A | 24F644A=0 |
| 0x30038B | P5 IOC (I/O direction) | Wb | Wb: 24F644A | 24F644A=FF |
| 0x30038C | P6 D (port data) | Wb bclr4 bset4 | Wb: 24F644A<br>bclr4: 250DC50<br>bset4: 250DC5A | 24F644A=0 |
| 0x30038D | P6 IOC (I/O direction) | Wb | Wb: 24F644A | 24F644A=FF |
| 0x30038E | P7 D (port data) | Wb | Wb: 24F644A | 24F644A=0 |
| 0x300390 | P8 D (port data) | Wb | Wb: 24F644A | 24F644A=0 |
| 0x300391 | P8 IOC (I/O direction) | Wb | Wb: 24F644A | 24F644A=FF |
| 0x300392 | P9 D (port data) | Wb | Wb: 24F644A | 24F644A=0 |
| 0x300393 | P9 IOC (I/O direction) | Wb bclr4 | Wb: 24F644A<br>bclr4: 252D3F2 | 24F644A=EF |
| 0x3003A1 | P04–P07 Port Function Select Register (pP0_47_CFP) | Rb Wb | Rb: 252D3F2<br>Wb: 24F644A 2504844 25049B2 252D3F2 | 24F644A=5 2504844=0 25049B2=0 |
| 0x3003A2 | P10–P13 Port Function Select Register (pP1_03_CFP) | Rb Wb bset0 bset2 bset4 bset6 | Rb: 25352AE 2535304 253535A 25353B2 253545E 2535488 25354B2 25354DE<br>Wb: 24F644A 25352AE 2535304 253535A 25353B2 253545E 2535488 25354B2 25354DE<br>bset0: 25352AE<br>bset2: 2535304<br>bset4: 253535A<br>bset6: 25353B2 | 24F644A=15 |
| 0x3003AE | P70–P73 Port Function Select Register (pP7_03_CFP) | Wb | Wb: 24F644A | 24F644A=1 |
| 0x3003B3 | P94–P97 Port Function Select Register (pP9_47_CFP) | Rb Wb | Rb: 252D3F2<br>Wb: 252D3F2 |  |
| 0x3003C0 | Port Input Interrupt Select Register 1 (pPINTSEL_SPT03) | Wb | Wb: 24F644A | 24F644A=0 |
| 0x3003C2 | Port Input Interrupt Polarity Select Register 1 (pPINTPOL_SPP07) | Wb | Wb: 24F644A 2504844 | 24F644A=0 2504844=0 |
| 0x3003C3 | Port Input Interrupt Edge/Level Select Register 1 (pPINTEL_SEPT07) | Wb | Wb: 24F644A 2504844 | 24F644A=17 2504844=17 |
| 0x3003C5 | Port Input Interrupt Select Register 4 (pPINTSEL_SPT1215) | Rb Wb | Rb: 252D3F2<br>Wb: 252D3F2 |  |
| 0x3003C6 | Port Input Interrupt Polarity Select Register 2 (pPINTPOL_SPP815) | bclr4 | bclr4: 252D3F2 |  |
| 0x300520 | A/D Clock Control Register (pAD_CLKCTL) | Rh Wh | Rh: 24F61BC<br>Wh: 24F61BC |  |
| 0x300540 | A/D Conversion Result Register (pAD_ADD) | Rh | Rh: 25003FA |  |
| 0x300542 | A/D Trigger/Channel Select Register (pAD_TRIG_CHNL) | Rh Wh | Rh: 24F61BC<br>Wh: 24F61BC |  |
| 0x300544 | A/D Control/Status Register (pAD_EN_SMPL_STAT) | Rh Wh | Rh: 24F61F2 24F621C 24F6316 25003FA 2502298 251A062<br>Wh: 24F61F2 24F621C 24F6316 25003FA 2502298 251A062 |  |
| 0x300548 | A/D Ch.x Conversion Result Buffer Registers (pAD_CHx_BUF) | Rh | Rh: 25003FA |  |
| 0x300558 | A/D Upper Limit Value Register (pAD_UPPER) | Wh | Wh: 24F61F2 |  |
| 0x30055A | A/D Lower Limit Value Register (pAD_LOWER) | Wh | Wh: 24F61F2 |  |
| 0x30055C | A/D Conversion Complete Interrupt Mask Register (pAD_CH04_INTMASK) | Wh | Wh: 24F621C |  |
| 0x30055E | A/D Converter Mode Select/Internal Status Register (pAD_ADVMODE) | Rh Wh | Rh: 24F61BC<br>Wh: 24F61BC |  |
| 0x300780 | T16 Ch.0 CRA (compare A) | Wh | Wh: 25352AE 2535542 |  |
| 0x300782 | T16 Ch.0 CRB (compare B) | Wh | Wh: 25352AE 2535542 |  |
| 0x300786 | T16 Ch.0 CTL (control) | Rb Wb Wh bset1 | Rb: 253545E 25355E6<br>Wb: 25352AE 253545E 25355E6<br>Wh: 240C51C 241BDA8 24F64F4 24FE21C 24FE82A 24FED98 24FF0BA 24FF32A 250B9AA 250BC60 250DF58<br>bset1: 25352AE 2535656 | 240C51C=0 241BDA8=0 24F64F4=2 24FE21C=0 250BC60=0 250DF58=0 25352AE=24 253545E=0 |
| 0x300788 | T16 Ch.1 CRA (compare A) | Wh | Wh: 2535304 2535554 |  |
| 0x30078A | T16 Ch.1 CRB (compare B) | Wh | Wh: 2535304 2535554 |  |
| 0x30078E | T16 Ch.1 CTL (control) | Rb Wb Wh bset1 | Rb: 2535488 25354DE 25355F4<br>Wb: 2535304 2535488 25354DE 25355F4<br>Wh: 2409C76 240C51C 241BDA8 24FE21C 24FE82A 24FED98 24FF0BA 24FF32A 250B9AA 250BC60 250DF58<br>bset1: 2535304 2535656 | 2409C76=0 240C51C=0 241BDA8=0 24FE21C=0 250BC60=0 250DF58=0 2535304=24 2535488=0 25354DE=0 |
| 0x300790 | T16 Ch.2 CRA (compare A) | Wh | Wh: 2409068 251A650 253535A 2535566 | 2409068=1A |
| 0x300792 | T16 Ch.2 CRB (compare B) | Wh | Wh: 2409068 251A650 253535A 2535566 | 2409068=35 |
| 0x300794 | T16 Ch.2 TC (counter) | Rh | Rh: 2410668 2412366 250DF58 |  |
| 0x300796 | T16 Ch.2 CTL (control) | Rb Wb Wh bset1 | Rb: 25354B2 2535602<br>Wb: 253535A 25354B2 2535602<br>Wh: 2409068 240C51C 24F64F4 251A650<br>bset1: 253535A 2535656 | 2409068=25 24F64F4=2 251A650=2 253535A=24 25354B2=0 |
| 0x300798 | T16 Ch.3 CRA (compare A) | Wh | Wh: 252D2FC 25353B2 2535578 | 252D2FC=B71B |
| 0x30079A | T16 Ch.3 CRB (compare B) | Wh | Wh: 251A566 252D2FC 25353B2 2535578 | 252D2FC=B71B |
| 0x30079E | T16 Ch.3 CTL (control) | Rb Wb Wh bset1 | Rb: 2535610<br>Wb: 25353B2 2535610<br>Wh: 2409C76 240C51C 241BDA8 24F64F4 24FE21C 24FE82A 24FED98 24FF0BA 24FF32A 2502A2C 2502A3E 250B9AA 250BC60 250DF58 251A566 251AF84 252D2FC 252D340<br>bset1: 25353B2 2535656 | 2409C76=0 240C51C=0 241BDA8=0 24F64F4=2 24FE21C=0 2502A2C=2 2502A3E=2 250BC60=0 250DF58=0 251A566=1 251A566=2 251AF84=0 252D2FC=1 252D2FC=2 252D340=0 25353B2=24 |
| 0x3007A2 | T16 Ch.4 CRB (compare B) | Wh | Wh: 253520C |  |
| 0x3007A4 | T16 Ch.4 TC (counter) | Wh | Wh: 253520C | 253520C=0 |
| 0x3007A6 | T16 Ch.4 CTL (control) | Rh Wb Wh bclr0 bset0 | Rh: 2528CC0 2528CF8<br>Wb: 253520C<br>Wh: 2409C76 240C51C 250DB16 250DB42 250DB8E<br>bclr0: 2535270<br>bset0: 253520C | 2409C76=1 240C51C=0 250DB16=0 250DB42=0 250DB8E=1 253520C=2 |
| 0x3007A8 | T16 Ch.5 CRA (compare A) | Wh | Wh: 25048D0 2504C44 2504D6E 2504E44 2505112 2505164 25051CE | 2504C44=0 2504D6E=0 2504E44=0 2505164=0 25051CE=0 |
| 0x3007AA | T16 Ch.5 CRB (compare B) | Wh | Wh: 2409C76 25048D0 2505112 2505164 251A062 251A5B4 | 2409C76=26 2505164=989 251A062=1E8 |
| 0x3007AE | T16 Ch.5 CTL (control) | Wh | Wh: 240308C 2409C76 240C51C 241BDA8 24F64F4 24FE21C 24FE82A 24FED98 24FF0BA 24FF32A 2502A2C 2502A3E 25048D0 2504C44 2504D6E 2504E44 2505112 2505164 25051CE 250B9AA 250BC60 250DF58 251A062 251A5B4 251AF84 251D67E | 240308C=0 2409C76=0 2409C76=1 2409C76=2 240C51C=0 241BDA8=0 241BDA8=1 241BDA8=2 24F64F4=2 24FE21C=0 24FE82A=1 24FE82A=2 24FF32A=1 24FF32A=2 2502A2C=2 2502A3E=2 25048D0=1 25048D0=2 2504C44=0 2504D6E=0 2504E44=0 2505112=1 2505112=2 2505164=1 2505164=2 25051CE=0 250BC60=0 250DF58=0 250DF58=1 250DF58=2 251A062=1 251A062=2 251A5B4=1 251A5B4=2 251AF84=0 251D67E=0 |
| 0x3007DE | 16-bit Timer STD/ADV Mode Select Register (pT16_ADVMODE) | Wh | Wh: 24F64F4 | 24F64F4=1 |
| 0x3007E0 | T16 Ch.0 CLKCTL (clock control) | Wb bclr3 | Wb: 25352AE<br>bclr3: 253545E | 25352AE=B |
| 0x3007E2 | T16 Ch.1 CLKCTL (clock control) | Wb bclr3 | Wb: 2535304<br>bclr3: 2535488 | 2535304=B |
| 0x3007E4 | T16 Ch.2 CLKCTL (clock control) | Wb Wh bclr3 | Wb: 253535A<br>Wh: 2409068 251A650<br>bclr3: 25354B2 | 2409068=A 251A650=7 251A650=D 253535A=B |
| 0x3007E6 | T16 Ch.3 CLKCTL (clock control) | Wb Wh bclr3 | Wb: 25353B2<br>Wh: 251A566 252D2FC 252D340<br>bclr3: 25354DE | 251A566=F 252D2FC=C 252D340=4 25353B2=B |
| 0x3007E8 | T16 Ch.4 CLKCTL (clock control) | Wb bclr3 | Wb: 253520C<br>bclr3: 2535270 | 253520C=E |
| 0x3007EA | T16 Ch.5 CLKCTL (clock control) | Wh | Wh: 2409C76 25048D0 2505112 2505164 251A062 251A5B4 | 2409C76=F 2505164=F 251A062=F 251A5B4=F |
| 0x300B02 | EFSIF Ch.0 STATUS | Rb | Rb: 252D2D6 |  |
| 0x300B10 | EFSIF Ch.1 TXD | Wb | Wb: 252C5CE 252C8CA 252CBEA 252CE54 252CFD8 252D204 252D3F2 | 252C5CE=0 252C5CE=1 252C5CE=3 252C5CE=4 252C5CE=5 252C5CE=15 252C5CE=18 252C5CE=1F 252C5CE=26 252C5CE=27 252C5CE=3A 252C5CE=48 252C5CE=55 252C5CE=7F 252C5CE=81 252C5CE=84 252C5CE=88 252C8CA=0 252C8CA=1 252C8CA=3 252C8CA=4 252C8CA=5 252C8CA=15 252C8CA=18 252C8CA=1C 252C8CA=1F 252C8CA=24 252C8CA=26 252C8CA=27 252C8CA=48 252C8CA=55 252C8CA=7F 252C8CA=81 252C8CA=84 252C8CA=88 252CBEA=1 252CBEA=3 252CBEA=4 252CBEA=5 252CBEA=15 252CBEA=18 252CBEA=26 252CBEA=27 252CBEA=2F 252CBEA=37 252CBEA=40 252CBEA=55 252CBEA=59 252CBEA=7F 252CBEA=81 252CBEA=82 252CBEA=F4 252CFD8=0 252CFD8=1 252CFD8=3 252CFD8=4 252CFD8=5 252CFD8=15 252CFD8=18 252CFD8=26 252CFD8=27 252CFD8=37 252CFD8=40 252CFD8=55 252CFD8=59 252CFD8=7F 252CFD8=81 252CFD8=82 252CFD8=F4 252D3F2=1 252D3F2=3 252D3F2=4 252D3F2=5 252D3F2=A 252D3F2=C 252D3F2=14 252D3F2=15 252D3F2=18 252D3F2=20 252D3F2=22 252D3F2=26 252D3F2=27 252D3F2=37 252D3F2=40 252D3F2=55 252D3F2=59 252D3F2=7F 252D3F2=80 252D3F2=81 252D3F2=82 252D3F2=89 252D3F2=8A 252D3F2=F4 |
| 0x300B11 | EFSIF Ch.1 RXD | Rb | Rb: 252C5CE 252C8CA 252CBEA 252CFD8 252D204 252D2A2 252D3F2 |  |
| 0x300B12 | EFSIF Ch.1 STATUS | Rb | Rb: 252C5CE 252C8CA 252CBEA 252CE54 252CFD8 252D204 |  |
| 0x300B13 | EFSIF Ch.1 CTL | Rb Wb | Rb: 252C5CE 252C8CA 252CBEA 252CFD8 252D3F2<br>Wb: 252C5CE 252C8CA 252CBEA 252CFD8 252D3F2 | 252C5CE=3 252C8CA=3 252D3F2=3 |
| 0x300B14 | EFSIF Ch.1 IRDA | Wb | Wb: 252D3F2 | 252D3F2=10 |
| 0x300B15 | EFSIF Ch.1 BRTRUN (baud timer ctl) | Wb | Wb: 252C5CE 252C8CA 252CBEA 252CFD8 252D3F2 | 252C5CE=0 252C5CE=1 252C8CA=0 252C8CA=1 252CBEA=0 252CBEA=1 252CFD8=0 252CFD8=1 252D3F2=0 252D3F2=1 |
| 0x300B16 | EFSIF Ch.1 BRTRD (baud reload, 16-bit) | Wh | Wh: 252C5CE 252C8CA 252CBEA 252D3F2 | 252C5CE=3 252C5CE=C3 252C8CA=1 252C8CA=C3 252CBEA=C3 252D3F2=C3 |
| 0x300C42 | P0 Pull-up Control Register (pMISC_PUP0) | Wb | Wb: 24F644A 2504844 | 24F644A=EF 2504844=EF |
| 0x300C43 | P1 Pull-up Control Register (pMISC_PUP1) | Wb | Wb: 24F644A | 24F644A=FF |
| 0x300C45 | P3 Pull-up Control Register (pMISC_PUP3) | Wb | Wb: 24F644A | 24F644A=7F |
| 0x300C48 | P6 Pull-up Control Register (pMISC_PUP6) | Wb | Wb: 24F644A | 24F644A=FF |
| 0x300C49 | P7 Pull-up Control Register (pMISC_PUP7) | Wb | Wb: 24F644A | 24F644A=1F |
| 0x300C4A | P8 Pull-up Control Register (pMISC_PUP8) | Wb | Wb: 24F644A | 24F644A=3F |
| 0x300C4B | P9 Pull-up Control Register (pMISC_PUP9) | Wb bclr4 | Wb: 24F644A<br>bclr4: 252D3F2 | 24F644A=EF |
| 0x301500 | BCLK and Setup Time Control Register (pSRAMC_BCLK_SETUP) | Ww | Ww: 24F6390 | 24F6390=81 |
| 0x301504 | Wait Control Register (pSRAMC_SWAIT) | Ww | Ww: 24F6390 | 24F6390=72707777 |
| 0x301508 | Device Size Setup Register (pSRAMC_SLV_SIZE) | Ww | Ww: 24F6390 | 24F6390=4655 |
| 0x30150C | Device Type Setup Register (pSRAMC_A0_BSL) | Ww | Ww: 24F6390 | 24F6390=0 |
| 0x301708 | SPI Control Register 1 (pSPI_CTL1) | Rw Ww | Rw: 250DC34 250DC42<br>Ww: 250DC34 250DC42 |  |
| 0x301900 | RTC Interrupt Status Register (pRTCINTSTAT) | Ww | Ww: 24FF966 250B46A | 24FF966=1 250B46A=1 |
| 0x301904 | RTC Interrupt Mode Register (pRTCINTMODE) | Rw Ww | Rw: 250325E<br>Ww: 25029FE 250325E 2504C44 2504D6E 2504E44 25051CE 250B46A 250B534 250B540 | 25029FE=6 2504C44=6 2504C44=7 2504D6E=6 2504E44=6 2504E44=7 25051CE=7 250B46A=0 250B46A=6 250B534=6 250B540=7 |
| 0x301908 | RTC Control Register (pRTC_CNTL0) | Ww | Ww: 250B46A | 250B46A=10 250B46A=11 250B46A=12 |
| 0x30190C | RTC Access Control Register (pRTC_CNTL1) | Rw Ww | Rw: 250B51A<br>Ww: 250B46A 250B51A 250B5A6 250B6DC | 250B46A=0 250B51A=0 250B51A=1 250B5A6=0 250B6DC=0 |
| 0x301910 | RTC Second Register (pRTCSEC) | Rw Ww | Rw: 250B6DC<br>Ww: 250B46A 250B5A6 | 250B5A6=0 |
| 0x301914 | RTC Minute Register (pRTCMIN) | Rw Ww | Rw: 250B6DC<br>Ww: 250B46A 250B5A6 |  |
| 0x301918 | RTC Hour Register (pRTCHOUR) | Rw Ww | Rw: 250B6DC<br>Ww: 250B46A 250B5A6 |  |
| 0x30191C | RTC Day Register (pRTCDAY) | Ww | Ww: 250B46A 250B5A6 |  |
| 0x301920 | RTC Month Register (pRTCMONTH) | Ww | Ww: 250B46A 250B5A6 |  |
| 0x301924 | RTC Year Register (pRTCYEAR) | Ww | Ww: 250B46A | 250B46A=8 |
| 0x301928 | RTC Days of Week Register (pRTCDAYWEEK) | Rw Ww | Rw: 250B6DC<br>Ww: 250B46A |  |
| 0x301A64 | IRAM Select Register (pLCDC_IRAM) | Ww | Ww: 24F6256 250DC6C | 24F6256=1 250DC6C=1 |
| 0x301B00 | Gated Clock Control Register 0 (pCMU_GATEDCLK0) | Ww | Ww: 24F6256 | 24F6256=8 24F6256=F |
| 0x301B04 | Gated Clock Control Register 1 (pCMU_GATEDCLK1) | Rw Ww bset6 | Rw: 25029FE<br>Ww: 24F6256 25029FE<br>bset6: 250DF1A | 24F6256=A07F1AD |
| 0x301B06 | Gated Clock Control Register 1 (pCMU_GATEDCLK1) (+2) | bclr3 | bclr3: 250DF1A |  |
| 0x301B08 | System Clock Control Register (pCMU_CLKCNTL) | Rw Ww | Rw: 250DB42 251A566 251A5B4 251A650<br>Ww: 24F6256 24F714C 24F717E 24F71B0 2504984 | 24F6256=770003 24F714C=771005 24F714C=771007 24F717E=770003 24F71B0=770003 2504984=770003 |
| 0x301B0C | PLL Control Register (pCMU_PLL) | Ww | Ww: 24F6256 | 24F6256=101804 |
| 0x301B14 | Clock Option Register (pCMU_OPT) | Ww | Ww: 24F6256 24F714C 24F717E | 24F6256=1 24F6256=5C00 24F714C=0 24F714C=1 24F717E=1 24F717E=5C00 |
| 0x301B24 | Clock Control Protect Register (pCMU_PROTECT) | Ww | Ww: 24F6256 24F714C 24F717E 24F71B0 25029FE 2504984 250DF1A | 24F6256=0 24F6256=96 24F714C=0 24F714C=96 24F717E=0 24F717E=96 24F71B0=0 24F71B0=96 25029FE=0 25029FE=96 2504984=0 2504984=96 250DF1A=0 250DF1A=96 |
| 0x600000 | LCD controller command (A0=0), external bus #CE? | Wb Wh | Wb: 4C8 7A2 9A6 2401DD8 2409030 2409068 2409138 240921A 240C51C<br>Wh: 4C8 7A2 2401DD8 2409138 | 4C8=2A 4C8=2B 4C8=2C 7A2=2A 7A2=2B 7A2=2C 2401DD8=1 2401DD8=11 2401DD8=29 2401DD8=E0 2401DD8=BuiltinItemKeyEntry 2401DD8=3C0 2401DD8=5C2 2401DD8=13C6 2401DD8=C036 2401DD8=C4C5 2409030=10 2409068=11 2409138=1 2409138=11 2409138=29 2409138=E0 2409138=BuiltinItemKeyEntry 2409138=3C0 2409138=5C2 2409138=836 2409138=13C6 2409138=C4C5 240921A=1 240921A=11 240921A=29 240921A=36 240921A=B1 240921A=B2 240921A=B3 240921A=B4 240921A=C0 240921A=C1 240921A=C2 240921A=C3 240921A=C4 240921A=C5 240921A=E0 240921A=BuiltinItemKeyEntry 240C51C=10 |
| 0x600001 | LCD controller data/parameter (A0=1) | Wb | Wb: 4C8 7A2 9A6 2401DD8 2409138 240921A | 4C8=0 7A2=0 2401DD8=0 2401DD8=1 2401DD8=2 2401DD8=3 2401DD8=4 2401DD8=5 2401DD8=6 2401DD8=8 2401DD8=A 2401DD8=F 2401DD8=15 2401DD8=1E 2401DD8=22 2401DD8=25 2409138=0 2409138=1 2409138=2 2409138=6 2409138=C 2409138=F 2409138=13 2409138=16 2409138=19 2409138=1B 2409138=1D 2409138=1E 2409138=1F 2409138=20 2409138=21 2409138=22 2409138=29 2409138=30 2409138=31 2409138=3C 2409138=3D 2409138=3E 2409138=3F 2409138=43 2409138=4E 2409138=84 2409138=B7 240921A=0 240921A=1 240921A=2 240921A=3 240921A=8 240921A=A 240921A=E 240921A=10 240921A=12 240921A=18 240921A=20 240921A=23 240921A=25 240921A=26 240921A=27 240921A=28 240921A=2A 240921A=2C 240921A=2D 240921A=2F 240921A=33 240921A=3B 240921A=3C 240921A=84 240921A=8A 240921A=A2 240921A=C5 240921A=EE |
