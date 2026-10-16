# LCD controllers, init sequences, draw path (OPEN.md item 5)

Sources: firmware (lcd_init, lcd_init_st7735, lcd_init_ili9163_old, sys_display_off, sys_display_wake,
IRAM asm/iram.s func_00006B8 / func_000081A / func_00009A6), the owner's datasheets
"ST7735P3" v0.1 2021-08 (Sitronix) and "ILI9163C" v0.09 (Ilitek), and the S1C33E07
Technical Manual Rev.1.4 (docs/id001557.pdf). Page numbers are the printed page numbers.

## Answers

* **P07 high -> Sitronix ST7735** (lcd_init_st7735). Verified: every command and parameter
  count matches the ST7735 panel command set, and several do not fit the ILI9163C at all.
* **P07 low -> an Ilitek ILI9163(C)-type controller** (the inline sequence of
  lcd_init, and the unused older copy lcd_init_ili9163_old). High confidence that it is *not*
  an ST7735; medium-high that it is the ILI9163 family (ILI9163C command set matches
  command for command; one inconsistency below).
* **Panel**: 128x128 visible, 18-bit colour (3 bytes per pixel), no COLMOD sent (reset
  default). ST7735 panel: MADCTL 0x08 (no mirroring, BGR), window offset column +2 / row +1.
  ILI panel: MADCTL 0xC0 (MY = MX = 1, rotated 180 degrees, RGB), no offset.
* **Strap**: read once at boot from P0 data bit 7 with the internal pull-up on, stored in
  lcd_is_st7735; afterwards P07 is an output driven low. A board with nothing on P07 runs
  the ST7735 sequence.
* **Sleep**: only SLPIN (10) / SLPOUT (11); no DISPOFF, no re-init, no redraw on wake.
  The backlight (T16 ch.2 PWM on P12) is switched separately.

## The P07 strap (verified)

* Boot order (sys_reset): sys_bus_setup (bus) -> sys_gpio_init (GPIO) -> sys_init
  -> nfc_session_clear -> main sys_main -> lcd_init first thing after clearing flags.
* sys_gpio_init: pMISC_PUP0 0x300C42 = 0xEF (P07 pull-up on; S1C33E07 manual: bit 7
  PUP07, 1 = pulled up, only effective as input), pP0_IOC0 0x300381 = 0x28 (P07 input),
  pP0_47_CFP 0x3003A1 = 5 (CFP07 = 00: GPIO; the alternatives would be #SRDY1 / I2S_MCLK).
* lcd_init: `(s8)REG8(0x300380) < 0` = P0D bit 7. High: lcd_init_st7735, lcd_is_st7735 = 1.
  Low: inline sequence, lcd_is_st7735 = 0.
* main right after the call: 0x300381 = 0xA8 (P03, P05, P07 outputs), 0x300380 = 0x5F
  (P07 = 0). So P07 is read exactly once and then driven low, which only makes sense if the
  strap is "open" or "to GND" (driving low against a pull-up/open pin draws nothing).
  Nothing else reads bit 7 of P0D (all other P0D reads are buttons P00..P02 or P06).
* lcd_is_st7735 is used only by the IRAM flush routines (offsets); there is no other
  controller-dependent code (sleep/wake send the same commands to both).
* Whether 4U+ boards have anything on P07 cannot be told from the firmware. Inferred: a
  board without a strap reads 1 (pull-up) -> ST7735 path.

## Bus (verified)

The LCD sits in area 8 (#CE8, 0x600000..0x7FFFFF). sys_bus_setup: pSRAMC_SLV_SIZE =
0x4655 -> CE8SIZE = 10 = 8-bit device (every other area 16-bit); pSRAMC_SWAIT =
0x72707777 -> #CE8 0 wait cycles; BCLK = SRAMC_CLK / 2. A0 = D/CX: 0x600000 command,
0x600001 data. A halfword store to 0x600000 is split little-endian into 0x600000 (low
byte, command) then 0x600001 (high byte, first parameter): this is how the packed init
pairs and the flush's `ld.h 0x002B` (= RASET + its first parameter 0x00) work. The
existing comment "16-bit flash/LCD areas" in sys/sys_boot.c was wrong and is fixed.

## ST7735 sequence (lcd_init_st7735)

ST7735P3 sections 10.1 / 10.2 (system commands p.103-105, panel commands p.153-156,
details p.157-182).

| bytes | command | meaning |
|---|---|---|
| 01, 20 ms | SWRESET | (datasheet p.107: wait 120 ms) |
| 11, 120 ms | SLPOUT | |
| B1 01 2C 2D | FRMCTR1 normal mode | RTNA 1, FPA 44, BPA 45; rate = fosc/((2*RTNA+40)*(lines+FPA+BPA+2)) p.157; ~91 Hz for 132 lines at the P3's 850 kHz |
| B2 01 2C 2D | FRMCTR2 idle mode | same |
| B3 01 2C 2D 01 2C 2D | FRMCTR3 partial mode | dot-inversion and column-inversion triplets, same |
| B4 00 | INVCTR | dot inversion in all modes (reset 07 = column) p.160 |
| C0 A2 02 84 | PWCTR1 | AVDD 101 = 5.0 V, VRHP 2 = GVDD 4.6 V, VRHN 2 = GVCL -4.6 V; third byte 84 = reset value (p.162 lists three default bytes A8/08/84 though only two are documented) |
| C1 C5 | PWCTR2 | VGH25 11 = 2.4 V, VGLSEL 01 = -10 V, VGHBT 01 = 3*AVDD-0.5 V (p.163; the bit table is garbled in the PDF, the ST7735S layout D7:6/D3:2/D1:0 assumed) |
| C2 0A 00 | PWCTR3 normal | SAPA 001 small, APA 010 medium-low, DCA 0 (= reset value) p.165 |
| C3 8A 2A | PWCTR4 idle | same currents, DCB 0x22A (reset 8A 26) |
| C4 8A EE | PWCTR5 partial | same currents, DCC 0x2EE (reset 8A EA) |
| C5 0E | VMCTR1 | VCOMS 14 = VCOM -0.775 V p.171 |
| 36 08 | MADCTL | MY=MX=MV=ML=0, RGB bit = 1 (BGR) |
| E0 12 20 10 18 33 2C 25 28 28 27 2F 3C 00 03 03 10 | GMCTRP1 | 16 x 6-bit (VRF0P, VOS0P, PK0..9, SELV0/1/62/63) p.180 |
| BuiltinItemKeyEntry 12 20 10 18 2D 28 23 28 28 26 2F 3B 00 03 03 10 | GMCTRN1 | same layout p.182 |
| 29 | DISPON | |

Why ST7735 and not ILI9163C: B1/B2 take 3 parameters and B3 6 (ILI: 2 each, DIVA/VPA);
C0 3 bytes, C3/C4 2 bytes with bits 7:3 set (ILI C3/C4: one byte, bits 7:3 = 0); C1 0xC5
sets bits the ILI defines as 0 (BT[2:0] only); the 16-byte all-6-bit gamma is the Sitronix
layout. The frame-rate triplet 01 2C 2D and the C0..C5 values are (from memory, not from
the supplied documents) the Sitronix sample init used by most ST7735R/S drivers, e.g.
Adafruit's "INITR" tables; the gamma tables are panel-specific.

## P07-low sequence (lcd_init inline; lcd_init_ili9163_old = unused older copy)

ILI9163C command list p.83-89, details p.158-181.

| bytes | ILI9163C | ST7735 reading |
|---|---|---|
| 01, 20 ms | SWRESET | SWRESET |
| C0 03 00 | Power_Control1: VRH 3 (GVDD), VC 0 (reset 10 / 5) p.158 | PWCTR1: AVDD 4.5 V, GVDD 4.55 V, GVCL -4.7 V (possible) |
| C2 05 | Power_Control3: APA 5 = large, one parameter p.161 | PWCTR3 needs 2 bytes; 05 = SAPA 000 = "operational amplifier stops" (p.165): no picture |
| C5 C4 | VCOM_Control1: VMH 0x44 = VCOMH 4.200 V (bit 7 don't care); VML not sent, stays 77 = -0.575 V p.164-165 | VMCTR1: VCOM -0.525 V (possible) |
| C6 13 | VCOM_Control2: VMA 0x13 = its documented reset value (command list p.87; no detail page in v0.09) | not defined; "undefined commands are treated as NOP" p.105 |
| 36 C0 | MADCTL: MY = MX = 1, RGB (older copy: 36 08) | same meaning |
| 11, 120 ms | Sleep Out | |
| E0 / BuiltinItemKeyEntry | gamma (15 parameters, p.177/179) | 16 x 6-bit |
| 29 | Display On | |

Evidence for the ILI family:
1. C2 with one parameter and C6 (VCOM control 2, written with exactly its reset value)
   exist only in the ILI9163C list; on the ST7735 the C2 byte would switch the source
   op-amp off.
2. The older table in lcd_init_ili9163_old is unmistakably in the ILI gamma layout:
   E0 3F 22 20 30 29 0C 4E B7 3C 19 22 1E 02 01 00 = VP63..VP0 with byte 7 = VP43 (7-bit,
   0x4E), byte 8 = VP27|VP36 nibbles (0xB7), byte 9 = VP20 (7-bit, 0x3C), bytes 6/10 fit
   5 bits; BuiltinItemKeyEntry 00 1B 1F 0F 16 13 31 84 43 06 1D 21 3D 3E 3F = VN0..VN63 likewise (0x84,
   0x43). 15 bytes each (the old comment said 16: fixed). Values above 0x3F cannot go into
   the ST7735's 6-bit gamma fields.
3. Window offsets 0 with MY = MX = 1 fit a controller strapped to a native 128x128
   frame memory (ILI9163C GM[2:0] = 001, 128RGB x 128, pin table p.10); the ST7735 has no
   128x128 mode (GM 00 132x162, 01 132x132, 11 128x160; p.24), hence its +2/+1 offsets.

Against / unexplained:
* The live E0/E1 tables are 16 bytes, all <= 0x3F, and do not look like an ILI curve
  (03 04 08 25 22 0A 1E 02 02 04 0F 0F 00 04 03 06 / 0A 22 25 15 0F 0F 01 1E 06 04 05 01
  0F 0F 04 02); on an ILI9163C the 16th byte is surplus.
* ILI9163C v0.09 says E0/E1 only apply "when EXTC=1 and GAM_R_SEL=1" (p.177, F2h p.181,
  reset 0). Neither sequence sends F2 01 (nor 26h GAMSET), so on a genuine ILI9163C both
  gamma tables would be inert. Possibly an ILI9163 variant / compatible clone where gamma
  is always on; cannot be settled with the supplied documents.
* Both sequences differ only in MADCTL (08 -> C0) and gamma: same controller, probably a
  different glass or mounting (rotated 180 degrees).

## Panel geometry and pixel format

* 128 x 128 visible; rows 0..27 and 100..127 icon bars, 28..99 main area.
* 18-bit colour: neither path sends COLMOD; reset value is 6 = 18 bits/pixel on both
  (ST7735P3 reset table p.88; ILI9163C reset tables p.73-78; the 128x128 one on p.75 is mislabelled "GM=010"). With the 8-bit interface that
  is three byte transfers per pixel, D7..D2 used. Colours are RGB565 in the assets,
  expanded to 6 bits per component by gfx_palette_set (lowest bit copied into bit 2).
* Byte order per pixel is blue, green, red (tables 0x3C10 = blue, 0x3D50 = green, 0x3E90 =
  red, gfx_palette_set), so the MADCTL RGB/BGR bit (or the glass) has to swap: BGR on the
  ST7735 panel, RGB together with MX/MY on the ILI panel. Which subpixel order each glass
  has is not visible in the firmware.
* ST7735: CASET x + 2, RASET y + 1 -> columns 2..129, rows 1..128 of the 132-wide frame
  memory (GM = 01 132x132 fits: 2 + 128 + 2 columns, 1 + 128 + 3 rows; the "1.44 inch
  128x128" ST7735 modules use the same 2/1 offsets unmirrored, from memory).
* ILI: no offsets.

## Draw path (verified from asm/iram.s)

* Dirty rectangles: main gfx_dirty_main_y0 y0, gfx_dirty_main_y1 y1, gfx_dirty_main_x0 x0, gfx_dirty_main_x1 x1 (clamped
  to rows 28..99, columns 0..127 by func_0000000); bars gfx_dirty_bar_y0..D_000180C same order.
  Empty = 0x7F, 0, 0x7F, 0. (F8a's gfx_display_reset comment listed them as x0, x1, y0, y1:
  fixed.)
* func_00009A6 ("present", about 240 call sites): func_000081A(main rect); if bar y0 <= 27:
  func_00006B8(y0..27); if bar y1 > 99: func_00006B8(100..y1); then sys_apply_backlight (backlight
  PWM update) and reset both rectangles.
* func_000081A: func_00007A2 composes the rectangle pixel by pixel (func_0000586 = the
  display list at (x, y)) into the 128x72 8-bit indexed frame buffer 0x1810..0x3C0F
  (index (y-28)*128 + x; it ends exactly where the palette tables start). Then:
  `2B 00 y0+r 00 y1+r`, `2A 00 x0+c 00 x1+c`, `2C`, and 3 bytes per pixel; r = 1, c = 2
  if lcd_is_st7735 else 0. The window is sent even for an empty rectangle (start > end, no
  data). Pixels whose palette index is in [gfx_tint_first, gfx_tint_end) (gfx_set_tint_range: the palette
  range of objects 9..n) in rows 28..91 are blended towards pink R 0xFF G 0x51 B 0x58 with
  weight (91 - row)/64.
* func_00006B8 (icon bars): composes the rectangle packed into the start of 0x1810 (as
  scratch, overwriting main-area buffer bytes), same window/RAMWR sequence, skipped when
  empty; palette set +0x100 unless lcd_palette_set == 1.
* Objects with flag 0x1000 (composite text, txt_render_line) take their pixels back from the
  frame buffer; txt_render_line renders glyphs into the buffer with func_00007A2 (F40 comment
  "pushed to the LCD at once" was wrong: fixed).
* Two IRAM helpers without any caller: 0xA3C (write command byte), 0xA48 (write data byte).
* No TE sync, no partial mode (12/30), no scrolling (33/37), no memory reads.

## Sleep / wake / power-down

* sys_display_off (display off): backlight level 0 (sys_set_backlight/func_251A650: T16 ch.2 PWM
  on P12 stopped), room_idle = 1, `10` SLPIN, lcd_asleep = 1, then the 32 kHz clock unless
  sys_battery_empty. SLPIN blanks the panel and stops booster/oscillator; frame memory and the
  DISPON state are kept (ST p.118).
* sys_display_wake (wake): `11` SLPOUT if lcd_asleep, backlight restored; nothing is redrawn,
  the old frame memory reappears.
* sys_power_down (battery empty): `10` SLPIN when sys_display_state == 2, then `slp`.
* No 120 ms waits after runtime SLPIN/SLPOUT (both datasheets ask for one before the next
  command); SWRESET is followed by only 20 ms (ST asks 120 ms). Apparently harmless.
* No DISPOFF (28) / DISPON at runtime, no hardware reset pulse from the code (only SWRESET;
  RESX is probably tied to the system reset).

## Comment changes

api/api_entries.c (file header draw path, lcd_init, gfx_display_reset, gfx_set_tint_range),
room/room_main.c (sys_display_off, sys_display_wake, lcd_init_ili9163_old, lcd_init_st7735, main's P0 note),
anim/anim_engine.c (gfx_palette_set, txt_render_line), sys/sys_boot.c (#CE8 8-bit), include/globals.h
(gfx_dirty_main_y0.., lcd_asleep, lcd_is_st7735).

Corrections to the earlier notes: fable/peripherals.md calls 0x1810 a "line buffer" (it
is the 128x72 main-area frame buffer), puts the LCD in area 7 (#CE7; it is area 8, #CE8)
and says lcd_init is "window + RAMWR + fill" (it is the strap read + init);
fable/NOTES.md calls lcd_init_ili9163_old the init sequence (it is the unused ILI copy) and
lcd_init_st7735 an "alternate panel init" (it is the live ST7735 init).

## Open

* Is the P07-low controller exactly an ILI9163C, or a sibling/clone whose E0/E1 work
  without F2? A datasheet of the ILI9163 (non-C) or of the actual module would settle it.
* Does any 4U+ board populate the P07 strap (photos of the PCB near the S1C33)?
* Subpixel order of each glass (needed to explain the 08 -> C0 MADCTL change).
