# Sound (OPEN.md item 10)

## Answer

* **Only T16 ch.0 and ch.1 make sound, and they make all of it**: key clicks, beeps,
  jingles and the long songs. Both play the same melody at the same time; ch.1 has its
  output inverted. TM0 (P10) and TM1 (P11) are therefore two complementary square waves:
  the piezo sits between P10 and P11 and is driven as a bridge (push-pull on both
  terminals, twice the voltage swing of a single pin). Opus was closer ("only ch.0/1"),
  but these channels play everything, not only the clicks. Fable's "all 4 tone channels in
  use" is wrong.
* The sound library has code for 4 voices on T16 ch.0-3 (TM0-TM3 = P10-P13), but the game
  only loads voices 0 and 1 (snd_play is the only caller of snd_voice_load /
  snd_voice_start). Voices 2 and 3 would break other hardware: TM2/P12 is the LCD backlight
  PWM, P13 is a GPIO (NFC front-end power, nfc_hw_power_on / nfc_hw_power_off), and T16 ch.3 is the
  NFC tick and the ms timeout.
* The README point that **T16 ch.3 is the 100 ms NFC tick** is confirmed (nfc_tick_start:
  CLKCTL3 = 0xC = MCLK/64, CR3A = CR3B = 0xB71B, compare-A interrupt; 30e6 / 64 / 46876 =
  10.0 Hz). Ch.3 is also the ms one-shot of tmr_t3_start_timeout (MCLK/4096, compare B). There is
  no sound on ch.3.
* There is no volume control. The duty is always 50 % (CRxA = period/2 - 1); the only
  setting is sound on/off (snd_enabled, menu ui_sound_menu; on by default at boot,
  sys_main).

## Timer channels

Register base x = 0x300780 + 8*x (CRxA +0, CRxB +2, TCx +4, CTLx +6); CLKCTL_x =
0x3007E0 + 2*x. CLKCTL values: bit 3 = on, bits 2:0 = prescaler 1/1, 2, 4, 16, 64, 256,
1024, 4096 (0x8..0xF; Epson BIOS sys_t16.h T16_CLK_*). CTL bits (manual IV-1-20, BIOS
io_t16.h): 0 PRUN, 1 PRESET, 2 PTM (TMx output on), 3 CKSL, 4 OUTINV, 5 SELCRB (buffered
compare), 6 SELFM (fine mode), 8 INITOL. Interrupt vectors are from asm/vectors.s.

| ch | clock | use | functions |
|---|---|---|---|
| 0 | 0xB = MCLK/16 = 1.875 MHz | sound voice 0, TM0 = P10 (pP1_03_CFP bits 1:0 = 01), CTL 0x24 = PTM + SELCRB, output not inverted | snd_ch0_on (on), snd_set_pitch_0 (pitch), snd_ch0_off (off), snd_restart_ch0 (run), snd_preset_timers (preset) |
| 1 | 0xB = MCLK/16 | sound voice 1, TM1 = P11, CTL 0x34 = PTM + OUTINV + SELCRB: the inverse of TM0 = the other piezo terminal | snd_ch1_on, snd_set_pitch_1, snd_ch1_off, snd_restart_ch1 |
| 2 | 0xA = MCLK/4 (OSC3) or 0xD = /256 (OSC1) | LCD backlight PWM on TM2 = P12 (CTL 0x25, sys_apply_backlight, sys_display_wake). The sound code for it (snd_ch2_on etc.) is never called. TC2 is also read as a random seed (earlier inventory) | sys_apply_backlight, sys_display_wake |
| 3 | 0xC = MCLK/64 (NFC) or 0xF = /4096 (timeout) | NFC 100 ms tick (nfc_tick_start / nfc_tick_stop, compare A, vector 43 -> nfc_tick_isr); ms one-shot (tmr_t3_start_timeout, compare B, vector 42 -> tmr_t16_23_isr). The sound code for it (snd_ch3_on etc.) is never called | nfc_tick_start, nfc_tick_stop, tmr_t3_start_timeout |
| 4 | 0xE = MCLK/1024 = 29.3 kHz | sound sequencer tick: CR4B = 0xE3, so a period of 228 counts = **7.78 ms (128.5 Hz)**; compare B, vector 46 -> snd_tick_isr -> snd_sequencer_tick | snd_tick_start (setup), snd_tick_timer_stop (stop, unused), snd_tick_ack (ack); snd_play / snd_end_callback_fn / snd_init start and stop it through CTL4 (1 / 0) |
| 5 | 0xF = MCLK/4096 = 7.3 kHz | ms delays (tmr_t5_start_timeout, compare B, vector 50 -> tmr_t16_45_isr), 3 Hz menu tick (tmr_t5_standard_tick), IR bit timer of the legacy IR link (tmr_t5_start_tick / tmr_t5_start_raw, compare A, vector 51 -> tmr_t5_cmpa_vector) | as listed |

## Hardware (verified from the code and the manual; the board itself is not seen)

* sys_gpio_init sets pP1_03_CFP = 0x15 (P10-P12 = TM0-TM2, P13 = GPIO), P1 all outputs
  (0x300383 = 0xFF), P1 data = 0x08 (P10/P11 low). Manual I-3-15 (PDF p. 49): CFP1x = 01
  selects TMx.
* A tone: snd_ch0_on / snd_ch1_on route the pin to the timer, set CLKCTL = 0xB and
  CRxB / CRxA, and set CTL to 0x24 (voice 0) or 0x34 (voice 1, mode 1) plus PRESET. Manual
  IV-1-11 (PDF p. 465): TMx goes high after the compare-A match and low again at the
  compare-B match, where the counter resets; OUTINV inverts it. The sequencer writes
  CRxB = P - 1 and CRxA = P/2 - 1, which gives a square wave of P counts:
  f = 1.875 MHz / P.
* Silence (rests, voices that are loaded but not playing) is CRxB = 1000, CRxA = 2000.
  Manual IV-1-12 (PDF p. 466), precaution 3: with A > B there is no compare-A match and the
  output stays at its off level. During rests TM0 is low and TM1 high (OUTINV): a DC level
  across the element, harmless for a piezo. At the end of a sound snd_end_callback_fn ->
  snd_voice_stop -> snd_ch0_off / snd_ch1_off turns the pins back to GPIO (both low) and
  stops the timers and their clocks.
* A new note presets the timers of the voices that changed (snd_preset_timers, bit mask) in the
  same tick, so TM0 and TM1 restart in antiphase together. A repeated note (a tie) is not
  re-preset.
* Inferred (high confidence): one piezo element between P10 and P11, driven in bridge
  mode. The inverted second voice playing the same data has no other purpose. One or two
  transistors cannot be ruled out without the board, but the code does not need them.

## The sound library (0x253520C..0x2535B6C)

A separate library compiled with -mgda (%r15 = 0 = GDA base). Its RAM: snd_saved_psr (saved
PSR), D_0084434 (AHR/ALR saved by the ISR), snd_voice_notes[4] (current note of each voice,
0xFF = none / rest), snd_end_callback (end callback, -1 = none), snd_voices[4] (voice records,
12 bytes).

SndVoice record (struct SndVoice in src/lib/snd/snd_sequencer.c):

| off | field |
|---|---|
| 0 | state: 0 free, 1 loaded / paused, 2 playing, 3 finished |
| 1 | ticks left in the current event (countdown) |
| 2 | transposition in semitones (signed) |
| 3 | "ended" flag for the callback |
| 4 | period table pointer (past the 0x3301 magic) |
| 8 | event pointer (past the 0x3302 magic) |

API:

| function | what it does |
|---|---|
| snd_sequencer_init(tempo) | init: callback = -1, all voices free, snd_tick_start(tempo) starts T16 ch.4 |
| snd_set_end_callback(fn) | set the end callback |
| snd_voice_load(v, table, seq, transpose, mode) | load voice v: checks v <= 3 (else -1), transpose >= -24 (else -3; the upper bound test checks v > 24 instead of transpose > 24, a bug that does not matter here), magic 0x3301 / 0x3302 (else -4); stops the voice, turns its tone channel on (silent, mode = output inversion), state 1 |
| snd_voice_start(v) | start: state 2, countdown 0, PRESET + PRUN of the channel's timer |
| snd_voice_pause(v) | pause (state 1). Unused |
| snd_voice_stop(v) | stop: channel off (pin back to GPIO, timer and clock off), state 0 |
| snd_voice_state(v) | state of voice v |
| snd_tick_stop | stop the tick (snd_tick_timer_stop). Unused |
| snd_sequencer_tick | one tick, see below |

Low level: snd_ch_on / snd_ch_off / snd_channel_restart / snd_set_pitch dispatch on the
channel to the per-channel on / off / run / set-pitch functions. **Bug:** snd_ch3_off
("channel 3 off") clears CTL of T16 ch.1 (0x30078E) instead of ch.3 (0x30079E) (verified
in the disassembly: 0x3003A2 + 0x3EC). It is never reached, since voice 3 is never used.

## Data format

**Period table**: u16 0x3301, then 61 u16 periods (in timer counts) for notes 1..61. Note 1
= C3, note 61 (0x3D) = C8, equal temperament with A4 = 440 Hz (note 22). Three tables
in ROM:

| address | C3 period | tuned for MCLK/16 with MCLK = |
|---|---|---|
| snd_period_table_main | 14333 | 30 MHz (14333 -> 130.8 Hz) |
| 0x24F5318 | 8806 | 18.432 MHz (A4 = 2618 -> 440.0 Hz); not referenced |
| D_24F5394 | 4403 | 9.216 MHz = 18.432 / 2 |

snd_init picks snd_period_table_main with tempo 0xE3 when pCMU_CLKCNTL bit 12 (MCLKDIV, manual
III-1-40, PDF p. 308) is clear, else D_24F5394 with tempo 0x71. MCLKDIV is set only by
sys_sleep_osc1 (sleep on OSC1, 0x771005/0x771007), and snd_init runs only at boot
(sys_main) and in sys_leave_low_power right after 0x770003 is written. So the second branch
never runs. If it did run, D_24F5394 at 15 MHz would sound about 8.4 semitones sharp. The
two extra tables look like leftovers of an earlier model clocked at 18.432 MHz (inferred).

**Sequence**: u16 0x3302, then one byte per event, 0x00 = end.

* bits 7:6 = length: 0 = 4, 1 = 8, 2 = 16, 3 = 32 ticks (31 / 62 / 125 / 249 ms).
* bits 5:0 = what:
  * 1..0x3D: a note (plus the voice's transposition, clamped to 1..0x3D). If it equals
    the note already sounding (snd_voice_notes, compared before transposition), it is a tie:
    the note goes on without a restart.
  * 0x3F: a rest of that length.
  * 0x3E: a short gap of length/4 ticks (1, 2, 4, 8). An event followed by a 0x3E byte is
    shortened by (that byte >> 6) + 1 ticks, so for 0x3E itself (the only form in the
    data: the "r1" below) the note gives up its last tick to a 1-tick gap and the total
    length stays the same. Used for articulation between repeated notes.
* When a voice reaches 0x00 it goes to state 3. When no voice is left playing (state 2),
  the step calls the callback once with the voice number.

Each tick snd_sequencer_tick: acknowledges the interrupt; for each voice in state 1 or 3 keeps
the channel silent; for each playing voice counts down, or reads the next event; presets
the timers of the voices that started a new note; then calls the callback for any voice
that has its "ended" flag set.

## The game side (snd/snd_play, snd/snd_play, snd/snd_play, snd/snd_play, snd/snd_play)

* snd_play(id): snd_finished = 0. If sound is on (snd_enabled): CTL4 = 1 (tick running),
  load voice 0 (mode 0) and voice 1 (mode 1) with the period table snd_period_table and
  snd_bank[id], transposition 0, and start both with interrupts masked. If sound is off:
  CTL4 = 0 and nothing else.
* snd_end_callback_fn = the callback: stops voices 0 and 1, CTL4 = 0, snd_finished = 1.
* snd_wait_end waits for snd_finished (returns at once when sound is off); snd_error_beep =
  sound 12 + wait.
* anim_queue_sound(id) (animation API, also game API api_anim_queue_sound for downloaded programs)
  queues an id in anim_queued_sound, and anim_frame_wait starts it with the next frame.
* Key click: the port-input ISRs btn_a_isr / btn_b_isr / btn_c_isr (vectors
  16-18, buttons A/B/C on P00-P02) debounce, then play 1 / 2 / 3 unless anim_busy (a
  sub-screen owns the buttons) or the button was already counted; the menus play the same
  ids themselves.
* A few test and engineering screens (test_spot_screen, test_nfc_link from test mode
  test_mode) and the legacy IR exchanges (ir_test_session, ir_test_hello, ir_test_packets) set
  snd_enabled = 1 around sound 0x19, then write 0 back.
* save_after_sound / snd_wait_before_flash (before NOR flash work) busy-loop 65535 times (the CTL4 test
  inside only adds a nop, it does not exit early), then wait for the sound with
  snd_wait_end. The sound ISR and its data are in the NOR flash, which is probably why
  (inferred).
* The test-mode sound test (test_sound) plays ids 1..0x53 with sound forced on.

## Sound bank snd_bank (84 pointers; entry 0 is NULL)

Data 0x24F5410..0x24F61BB (D_24F5410), right after the third period table. The whole
0x24F529C..0x24F61BB area is one data object in the code section, src/snd/snd_data.c (the
asm split had taken it for two functions, func_24F529C / func_24F52F4, the second starting in
the middle of the first table). Lengths count ties and 0x3E gaps; "notes" =
note events. Uses are the constant ids passed to snd_play / anim_queue_sound. Ids 6, 9,
0x17, 0x26, 0x2E, 0x33, 0x39, 0x3B, 0x3F-0x41, 0x47, 0x48, 0x4A, 0x4C-0x4E, 0x50, 0x51 do
not appear as constants (they may be passed as variables, or played by downloaded
programs, or be unused). 0x43-0x4E and 0x53 are tunes of 6-32 s.

| id | address | ms | notes | range | known use |
|---|---|---|---|---|---|
| 1 (0x01) | 24F5410 | 31 | 1 | E5..E5 | key click A (button ISR btn_a_isr, menus, anim_frame_wait) |
| 2 (0x02) | 24F5418 | 125 | 4 | E5..G6 | key click B (btn_b_isr, menus) |
| 3 (0x03) | 24F5424 | 31 | 1 | F#5..F#5 | key click C (btn_c_isr, menus) |
| 4 (0x04) | 24F542C | 1494 | 5 | D6..C7 | call / alarm (care call while the screen is off: RTC ISR rtc_isr, pet_get_sick, sys_main; visitor leaving visit_leave_message) |
| 5 (0x05) | 24F5440 | 1743 | 12 | C4..G#6 |  |
| 6 (0x06) | 24F5454 | 1245 | 24 | G5..G7 |  |
| 7 (0x07) | 24F5470 | 996 | 7 | C6..C7 |  |
| 8 (0x08) | 24F5480 | 1899 | 17 | D5..A6 |  |
| 9 (0x09) | 24F54A4 | 3082 | 32 | G4..G6 |  |
| 10 (0x0A) | 24F54D8 | 187 | 3 | F3..C4 | card-matching pair found (game_slot_score_pair), animations |
| 11 (0x0B) | 24F54E0 | 187 | 3 | A5..A6 | many animations |
| 12 (0x0C) | 24F54E8 | 249 | 2 | C5..F#5 | error / "not possible" (snd_error_beep, ui_main_icon_blocked, pet_asleep_refuse, pet_can_pay, ...) |
| 13 (0x0D) | 24F54F0 | 623 | 7 | C5..C6 |  |
| 14 (0x0E) | 24F5500 | 374 | 2 | C#5..C#5 |  |
| 15 (0x0F) | 24F5508 | 436 | 10 | C5..D#6 |  |
| 16 (0x10) | 24F5518 | 996 | 11 | C6..C7 |  |
| 17 (0x11) | 24F552C | 436 | 10 | B5..G6 |  |
| 18 (0x12) | 24F553C | 436 | 4 | F#6..G6 | card-matching game (game_slot_screen, game_slot_loop), animations |
| 19 (0x13) | 24F5548 | 374 | 6 | D#5..F#5 |  |
| 20 (0x14) | 24F5554 | 187 | 3 | C5..C7 | sys_main (main loop), animations |
| 21 (0x15) | 24F555C | 249 | 3 | F#5..E6 |  |
| 22 (0x16) | 24F5564 | 218 | 4 | E6..A6 | game_slot_loop, many animations |
| 23 (0x17) | 24F556C | 809 | 21 | C5..C7 |  |
| 24 (0x18) | 24F5588 | 93 | 3 | D5..D#5 |  |
| 25 (0x19) | 24F5590 | 1619 | 18 | C5..C7 | "read OK" fanfare: tag test screen test_spot_screen, test-mode NFC test_nfc_link, legacy IR (sound forced on) |
| 26 (0x1A) | 24F55B0 | 1743 | 6 | C#5..D#5 | sys_main, 1 animation |
| 27 (0x1B) | 24F55C4 | 2055 | 9 | C6..F#6 |  |
| 28 (0x1C) | 24F560C | 1743 | 17 | G3..C5 |  |
| 29 (0x1D) | 24F5628 | 872 | 6 | F6..G6 |  |
| 30 (0x1E) | 24F5644 | 62 | 2 | F#6..G6 | card-matching game (game_slot_score_pair), many animations |
| 31 (0x1F) | 24F564C | 498 | 16 | C3..G4 |  |
| 32 (0x20) | 24F5660 | 1432 | 14 | C5..F#6 |  |
| 33 (0x21) | 24F5694 | 1743 | 17 | G5..C7 |  |
| 34 (0x22) | 24F56CC | 374 | 3 | C6..G6 |  |
| 35 (0x23) | 24F56D4 | 1619 | 13 | D5..C6 |  |
| 36 (0x24) | 24F56E4 | 1992 | 10 | D4..C5 |  |
| 37 (0x25) | 24F56F4 | 1806 | 13 | A4..D#5 |  |
| 38 (0x26) | 24F5710 | 1806 | 11 | C7..C8 |  |
| 39 (0x27) | 24F5720 | 1276 | 8 | B5..F6 |  |
| 40 (0x28) | 24F5738 | 623 | 3 | C6..E6 |  |
| 41 (0x29) | 24F5744 | 1245 | 20 | C5..A#5 |  |
| 42 (0x2A) | 24F575C | 747 | 8 | D6..B6 |  |
| 43 (0x2B) | 24F5774 | 872 | 14 | C5..B5 |  |
| 44 (0x2C) | 24F5788 | 1183 | 19 | C6..A#6 |  |
| 45 (0x2D) | 24F57B4 | 747 | 8 | D4..B4 |  |
| 46 (0x2E) | 24F57C4 | 872 | 14 | C6..B6 |  |
| 47 (0x2F) | 24F57E4 | 1806 | 20 | A#3..F5 | town town_award_care_mark, animations |
| 48 (0x30) | 24F5804 | 1245 | 17 | F3..C5 |  |
| 49 (0x31) | 24F5818 | 405 | 12 | C5..B5 |  |
| 50 (0x32) | 24F5828 | 623 | 4 | G5..E6 |  |
| 51 (0x33) | 24F5830 | 93 | 2 | E4..A4 |  |
| 52 (0x34) | 24F5838 | 218 | 7 | D3..G3 |  |
| 53 (0x35) | 24F5844 | 498 | 8 | E6..A6 |  |
| 54 (0x36) | 24F5860 | 1370 | 10 | C4..C5 |  |
| 55 (0x37) | 24F5870 | 747 | 3 | B6..B6 | test mode test_mode (one B6 held for 96 ticks: a speaker test) |
| 56 (0x38) | 24F5878 | 2739 | 9 | G5..D6 |  |
| 57 (0x39) | 24F589C | 1370 | 10 | C6..C7 |  |
| 58 (0x3A) | 24F58B4 | 1432 | 20 | C4..C5 |  |
| 59 (0x3B) | 24F58CC | 529 | 3 | C6..C7 |  |
| 60 (0x3C) | 24F58D4 | 685 | 10 | C7..C8 |  |
| 61 (0x3D) | 24F58E4 | 3237 | 12 | C6..C7 | power-on jingle after the logo (sys_power_on) |
| 62 (0x3E) | 24F58FC | 498 | 16 | C5..G6 |  |
| 63 (0x3F) | 24F5910 | 840 | 5 | C5..C6 |  |
| 64 (0x40) | 24F5928 | 1712 | 10 | C5..D6 |  |
| 65 (0x41) | 24F5954 | 2646 | 16 | C5..D6 |  |
| 66 (0x42) | 24F5998 | 3393 | 20 | C5..D6 |  |
| 67 (0x43) | 24F59EC | 8903 | 46 | B4..A#5 |  |
| 68 (0x44) | 24F5A7C | 7347 | 35 | D5..D6 |  |
| 69 (0x45) | 24F5AC8 | 9090 | 43 | C5..C6 |  |
| 70 (0x46) | 24F5B18 | 10771 | 124 | F4..F6 |  |
| 71 (0x47) | 24F5C10 | 8063 | 54 | G5..G6 |  |
| 72 (0x48) | 24F5C88 | 8903 | 40 | G4..G5 |  |
| 73 (0x49) | 24F5CF8 | 10428 | 83 | C4..F#6 |  |
| 74 (0x4A) | 24F5DA4 | 32437 | 133 | C6..C7 |  |
| 75 (0x4B) | 24F5F3C | 20421 | 91 | C6..C7 |  |
| 76 (0x4C) | 24F5F9C | 11954 | 48 | C5..A5 |  |
| 77 (0x4D) | 24F5FFC | 5977 | 31 | G4..F5 |  |
| 78 (0x4E) | 24F603C | 9339 | 53 | A4..D6 |  |
| 79 (0x4F) | 24F60D4 | 996 | 4 | A#5..A#6 | NFC / touch success jingle (spot_reader, F69 NFC functions, nfc_idle_target_session) |
| 80 (0x50) | 24F60E0 | 996 | 4 | C5..C5 |  |
| 81 (0x51) | 24F60F4 | 2988 | 11 | C5..A5 |  |
| 82 (0x52) | 24F6110 | 2988 | 10 | C5..D#5 |  |
| 83 (0x53) | 24F612C | 18927 | 101 | C#4..G#6 |  |

## Corrections to earlier notes and comments

* Fable "4 square-wave channels ... all in use": wrong; only ch.0/1, the same melody,
  bridge drive. Fable "CTL0 = 0x34 (fine + buffered + inverted + PTM)": 0x34 has no fine
  mode (bit 6); it is PTM + OUTINV + SELCRB.
* Opus "only ch.0/1 for key clicks": channels right, but they play every sound.
* The snd_init comment ("D_24F5394 with the halved MCLK") is right about the code, but
  that table is tuned for 9.216 MHz, and the branch is never taken.
* save_after_sound / snd_wait_before_flash "wait for the T16 ch.4 tick to stop": they do not wait for
  it. The loop has a fixed length.

## Still open

* The board: one piezo between P10 and P11 is inferred from the code. A photo or a
  continuity check would settle it.
* Names for the unlabelled ids (which event or animation plays each one). F0 calls carry
  the scene names; a pass over the F0 script summaries would label most of them.
* Whether the ids that never appear as constants are reachable (variable ids, downloaded
  programs).
