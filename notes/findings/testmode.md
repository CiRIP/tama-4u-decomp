# Test mode and other hidden / debug features (OPEN.md item 15)

Status keys: **V** = verified in the code or the dump, **I** = inferred (confidence given).

## 1. How the test mode is entered (V)

Only one call path leads to test_mode, and it runs once per power-on:

reset vector -> sys_reset (reset handler) -> sys_init (init) -> sys_main (main)
-> sys_startup_flow (power-on sequence, its first call) -> sys_power_on -> test_check_entry_code ->
test_mode.

sys_power_on shows the logo 0x212A834, does the battery check (a dead battery freezes in
sys_power_down), shows the start-up picture D_2133924 (ui_show_startup_picture), then test_check_entry_code checks a
**button code: A, C, A, B, C** (btn_a_presses = A/P00, btn_b_presses = B/P01, btn_c_presses = C/P02, the
counters of the port ISRs btn_a_isr/25001E4/25002D6). The window is 6 ticks of the 500 ms
T16 ch.5 timer (tmr_t5_start_timeout(500), flag tmr_flags bit 5), about 3 s from the start-up picture.
A press out of order ends the check at once; if a button was already counted when the check
starts (held during the logo) it is skipped. No flash flag, card or TAMAGO file is involved.
test_mode and test_check_entry_code have no other caller; no pointer to test_mode exists in the
image (LE/BE 32-bit scan).

After test_mode returns, sys_power_on continues the normal start-up (lcd_palette_set = 0, clear
screen); if the save sector 0x200E000 is blank (as after the flash test below) the toy starts a
new game.

## 2. What the test mode does (V unless marked)

Pictures were decoded from the graphics area (format: w, h, colour count n, 0, frame count,
then n big-endian RGB565 colours from +6, then 4-bit pixels for n <= 16; a decoder is easy to
rebuild from func_0000312). lcd_palette_set = 1 for the whole test (it selects which of the two
palette table sets the LCD flush func_00006B8 uses; main runs with 0). Backlight level 4.

1. **"DB ON" (debug strap)**: if sys_debug_strap_count == 8 (P06 held low during the ten boot idle ticks,
   sys_sample_debug_strap), PN512 off (nfc_hw_power_off), picture 0x20804B8 = "DB ON", halt forever. So the
   code only confirms the strap; the test proper runs only with P06 high.
2. **LCD colours + buttons**: PN512 off; full-screen fills 0x001F (blue), 0x07E0 (green),
   0xF800 (red), 0xFFFF (white), each left only with a given button: A, B, C, A.
3. **Power-down**: wait until all buttons are released, mask the port 4-7/RTC/A-D interrupts
   (0x300277 = 0), enable only the A key interrupt (0x300270 = 1), sys_display_off (backlight 0,
   LCD sleep-in, CPU to SLEEP on OSC1 via sys_sleep_osc1); A wakes it; then sys_display_state = 2 and a
   second `slp`; the next A wakes it and the ISR's sys_display_wake(1) turns the LCD and backlight
   back on. I (medium): the two sleeps are the standby-current measurement points.
4. **RTC "COUNT"** (test_rtc_count): resets the RTC to 00:00:00 1 Jan (rtc_reset, so the test
   mode clears the clock), RTC 1 s interrupt on (rtc_irq_on), test_rtc_count_mode = 1 makes the RTC
   handler rtc_isr only count seconds in test_rtc_seconds (up to 100) instead of running the
   game; the count is shown in 2 digits; after 3 s, A ends it (interrupt off again,
   rtc_irq_off).
5. **Version**: picture 0x2087100 "VER:" + digits "004" and "15-05-16" (dash glyph 0x208F37C):
   firmware version 4, I (high): build date 2015-05-16. The same "VER:" screen shows the
   peer's TAMAGO header byte 0x4D in the link test (step 10); the toy sends 4 there, so 0x4D
   is the protocol/firmware version (I, medium-high).
6. Branch at the version screen: **A** -> step 7; **B, then B+C** -> the **sound test**
   test_sound ("SOUND" + number, A = next, C = previous (0..0x52), B = play sound n+1 with
   sound forced on, hold C then add A = end) and **leave the test mode**.
7. **Battery** (test_battery_check): "バッテリチェック", one A/D conversion of the battery channel,
   checked over 10 ticks of 0x1E8 T16 counts (about 67 ms each, ~0.7 s); a reading <= 0x2E3
   shows 0x2129614 "でんちをかえてね!!" and halts; otherwise "V.v" = reading / 310 and
   (reading / 31) % 10 (I, medium: volts with a 3.3 V full scale).
8. Branch: **A** -> step 9 (flash test); **B+C** -> **ROM checksum**: picture 0x2084CE8
   "ROM TEST" on green, then A = "メイン" (blue): program sum test_rom_sum_program; B+C = "オール" (red):
   graphics sum test_rom_sum_graphics must be 0x520A, then the program sum. Pass = 0x2083ADC "ROM OK",
   A -> step 10 (the flash test is skipped). Fail = 0x20828D0 "ROM ERROR", halt.
   The program sum is the 16-bit sum of the halfwords 0x2400000..0x2563548 (vectors, code,
   .rodata, the .data and .iram load images) minus the stored reference, compared with the
   reference test_rom_sum_ref = 0x8F12 (its load image at 0x25627D4 is inside the summed range, which
   is why it is subtracted). Both sums match on the dump: program 0x8F12, graphics 0x520A.
   The text uses the 12x14 katakana font 0x208F3C0 (index = gojuon order ア=0..ン=45, then
   small kana ァ=46.., ッ=51, voiced ガ=55.., バ=70..) and the symbol font 0x2090E0C (0 = ー,
   9 = ".").
9. **Flash test** (only via A at step 8): "ROM TEST" on white, test_flash_erase_all runs the table
   test_flash_areas {flags, first sector, last sector, address, value, halfwords}; flags 0xF = erase
   (func_0000A54), fill with the value 0 (func_0000C46 in fixed-source mode), verify every
   halfword, erase again. Areas: sectors 1-4 0x2000000 (game/app code area, 32 KB), 8
   0x200E000 (save), 9-11 0x2010000, 12-15 0x2040000, 0x34-0x3B 0x22C0000, 0x62-0x87
   0x25A0000..0x2800000 (item banks, NFC buffers, received data). Program and graphics are
   not touched. So this is both a flash write test and the **factory reset**: all user data
   is blank afterwards. Error -> "ROM ERROR", halt. OK -> "ROM OK", A -> picture 0x2089DF0
   (the "さうんど おん/おふ" panel), A -> sound 0x37 (forced on) -> 0x2085EF4 "SOUND OK", A.
10. **Final loop** (both paths of step 8 end here):
    - "NFC INITIALIZE" (test_nfc_init_check): PN512 power-up nfc_power_up; "OK", or "NG" and halt; A.
    - Touch Spot tag tests (F51): test_spot_read / test_spot_check / test_spot_write =
      test_spot_screen(3 "R" read and show the values, 4 "C" check -> OK/NG picture, 5 "W" write:
      the tag counter is decremented then incremented back, each write verified ->
      "TAG WRITE OK/NG"); B starts each.
    - NFC power-up again (NG -> halt), then the **Tamagotchi link test** test_nfc_link with
      session type 1 and peer kind 0xF040: becomes target when a field appears, initiator
      when B is held; on success shows the peer header byte 0x4D on "VER:" with sound 0x19
      (A = again, B = next; on failure B = retry, A+C = give up).
    - The same with peer kind 0xF0F0 (target only; A+C quits): success + B **leaves the test
      mode**. Otherwise the brightness screen lcd_brightness_setting is shown (with pet_life_stage forced to
      1 so it does not exit at once) and the loop starts again.

Ways out: the sound test (step 6) or the second link test (step 10). "DB ON", the empty
battery picture, "ROM ERROR" and "NFC INITIALIZE NG" halt until the battery is removed / reset.
No IR test (the IR routines are not called), no RTC accuracy check beyond counting.

## 3. Other hidden / debug features met

- **P06 debug strap** (sys_debug_strap_count == 8, sys_sample_debug_strap; 8 consecutive low samples): (V)
  - test mode only shows "DB ON" (above);
  - main sys_main: no boot-time notice event (room_mood_question_due = 0);
  - RTC handler rtc_isr: the automatic screen-off counter sys_idle_seconds is not advanced, so
    the screen never turns off by itself;
  - C button ISR btn_c_isr: a C press does not count as C but sets **sys_debug_fast_forward = 1, a
    time fast-forward**: while it is set, each RTC second advances the game clock by up to 5
    minutes, running the minute simulation (pet_minute_tick) and daily events (pet_clock_events)
    each step and stopping early at the first event; main reloads its animation timer with
    0x26 counts (~5 ms) and anim_frame_wait waits 10 ms instead of its delay. The next C press
    (sys_debug_fast_forward already set) counts as a normal C and clears it; scene set-ups, care calls and
    sleep also clear it. The main comment called sys_debug_fast_forward a "minute flag": wrong, fixed.
  - communication menu comm_main_menu: P06 low selects the table comm_main_menu_debug_items with
    "╬デバックセット" (comm_menu_debug_set -> test_debug_set_character) and "アイテムSEND" (comm_menu_item_send);
    the menu's init function comm_menu_enable returns 1 for those two entries only while P06 is
    still low (else 2). test_debug_set_character gives all souvenirs when sys_debug_strap_count == 8 (other agent's
    topic).
- **Brightness screen easter egg** (V): in lcd_brightness_setting (settings > あかるさせってい, also the
  test mode's last screen) holding A+B shows the picture 0x2134B38, a character saying
  "くろまめセンパイみたいになるのがユメなんだよねー★", until a button or 10 s.
- **Test-only code**: test_nfc_link (link test; its comment said "used by nfc_idle_target_session",
  wrong: the test mode is its only caller), test_nfc_init_check, nfc_show_session_screen cases 5-7 ("NFC
  INITIALIZE" OK/NG), test_spot_read/F0/FA + test_spot_screen (tag R/C/W), test_battery_check,
  test_sound_draw, test_sound, test_rtc_count, test_flash_erase_all, test_rom_sum_program, test_rom_sum_graphics.
- **Unreferenced**: test_nfc_loopback (factory NFC loopback, no caller), lcd_init_ili9163_old (old LCD
  init), album_special_charas and diary_viewer (below). The "\n" entry after the count of the
  communication menu (ir_menu_hidden_session) is probably not reachable through the menu either
  (count 4 / 6; I, not traced into ui_menu_run_plain).

## 4. Code observations

- **flash_fill (F66) returns -1 with interrupts still disabled: confirmed** (0x2528BB0
  `jrne.d` to the epilogue skips the sys_irq_enable call at 0x2528BBE). **flash_program_bytes has the
  same bug** (0x2528C30 skips 0x2528C3E). sys_irq_disable = `psrclr 4` (IE off), sys_irq_enable =
  `psrset 4`. Does it matter: practically no. func_0000B52 fails only on a flash program
  timeout (DQ5) or on a cell that is neither erased nor already equal to the value, and the
  callers (F70b comm_build_phase1 & co., F66, comm/comm_outcome) erase the banks first and ignore the
  result. If it did happen: per the S1C33E07 manual (III.2.1.5) `halt` still returns on an
  interrupt request with IE = 0 but no handler runs, so the button counters, timer flags, RTC
  and NFC IRQ flag stop changing and the next wait loop spins forever, unless a later flash
  call of the sequence succeeds (its sys_irq_enable re-enables). The last write of each record
  is flash_fill(2, p, crc), so a failure there would freeze the toy until reset.
- **diary_viewer and album_special_charas have no callers: confirmed**: no call/jump in the
  disassembly of the whole image, no 32-bit pointer (LE or BE, also +1) anywhere in the
  8 MB dump, no table entry in the sources. Only a downloaded game calling a hard-coded
  firmware address could reach them (not checkable here).

## 5. Comment fixes made

- test_rom_sum_program: returned "0 = intact" was wrong; it returns the sum without the reference,
  which must equal test_rom_sum_ref (0x8F12).
- rtc_irq_on / rtc_irq_off / rtc_reset: pRTCINTMODE 7 = 1 s cycle, level, interrupt
  enabled; 6 = the same with the interrupt disabled (manual III-3-15, RTCT[1:0] = 01 = 1 s).
  The comments said "the other period, used during IR transfers" / "every second?" / "the
  1 s interrupt (= 6)".
- lcd_brightness_setting and ui_settings_brightness: brightness screen (sys_backlight_level, 7 bars), not the clock.
- test_battery_check: the timer ticks are 0x1E8 T16 counts (~67 ms), not 0x1E8 ms.
- test_nfc_link: only caller is the test mode.
- test_sound ("reads the buttons") and test_rtc_count ("button / A-D interrupt test"):
  they are the sound test and the RTC count test.
- main sys_main: sys_debug_fast_forward is the debug fast-forward, not a minute flag.
- New or extended: test_mode, test_check_entry_code, sys_power_on, test_flash_erase_all (+ test_flash_areas),
  test_rom_sum_graphics, test_nfc_init_check, test_spot_screen and its three wrappers, sys_sample_debug_strap,
  btn_c_isr, flash_fill / flash_program_bytes, album_special_charas / diary_viewer, and the globals
  sys_debug_fast_forward, sys_debug_strap_count, test_rom_sum_ref, lcd_palette_set.

## 6. Open

- What exactly 0x6000 in the 2-colour pictures maps to (func_000014A treats it as a key
  colour); the pictures read correctly either way.
- Purpose of the two separate sleeps of step 3 (likely current measurements).
- Peer kind 0xF0F0 in the second link test: which device answers (phone app or a jig).
