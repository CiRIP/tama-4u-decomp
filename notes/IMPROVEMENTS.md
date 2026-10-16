# Later improvements

Source improvements that keep the build byte-identical, noted for later. Each entry: what, why,
what is known, how to do it, and the open questions.

## 1. The system clock as one constant (2026-10-16)

**What.** Every value derived from the 30 MHz main clock (MCLK, OSC3) written as a formula of
one constant in a shared header, e.g. `include/clock.h`: `#define MCLK_HZ 30000000`, plus
helpers for timer counts and UART reloads. No `#ifdef`: changing the constant gives a
consistent firmware for another crystal or an overclock (animation speed, sound pitch and
tempo, NFC baud rates and timeouts, delays). The pet clock needs nothing: the RTC runs from
the separate 32.768 kHz OSC1.

**Why it can stay byte-identical.** gcc folds constant expressions before code generation:
`(t * (MCLK_HZ / 1000)) >> 12` compiles to the same code as `(t * 30000) >> 12`, as long as
each formula gives exactly the original integer at 30 MHz with the same C type. The build
checks every substitution. Checked so far: NFC 100 ms tick 0xB71B = 30e6 / 64 / 10 = 46875
(exact); 3 Hz menu tick 0x989 = 30e6 / 4096 / 3 = 2441 (integer division); PN512 start baud
reload 0xC3 = 30e6 / (16 * 9600) - 1 = 195; tmr_t5_start_timeout's 30000 / 15000 =
MCLK_HZ / 1000 / 2000.

**Where (counts of 2026-10-16).**
- About 59 timer compare / UART reload writes in C, 7 places with millisecond arithmetic
  (tmr_t5_start_timeout, tmr_t3_start_timeout ...), the sequencer tempo 0xE3 (snd_init,
  timer 4 at MCLK/1024), the menu tick (tmr_t5_standard_tick).
- 169 `nop` busy-wait loops (many in src/lib/nfc), e.g. 2000 in card_activate. Their
  intended delays are unknown: write them as cycle counts, don't invent microseconds.
- 11 assembly-only / NON_MATCHING functions touch the timers (27 places): their constants
  are in asm/func/*.s. Either wait until they are C, or use assembler symbols from a shared
  include (gas folds absolute expressions; check each against the ext optimizer).
- The sound period tables (src/snd/snd_data.c: 30 MHz table at 0x24F529C, and the unused
  half-clock table at 0x24F5394, tuned for 9.216 MHz). An equal-tempered scale at
  MCLK / 16 (first note 14333 = C3, 130.81 Hz; neighbours a semitone apart), but plain
  rounding of 1.875 MHz / f does not give every entry (the third gives 12770, the table has
  12769). Find the generator's rule, or keep the table as numbers with a tools/ script that
  regenerates it for another clock.

**Precedent.** The firmware already does a hand-made version for its never-used
MCLKDIV half-clock mode: 15000 instead of 30000, tempo 0x71, the second period table.

**Constraint.** A compile-time constant only: reading the clock at run time changes the
code.

## 2. The save state's names as fields of `pet` (2026-10-16)

**What.** `struct PetState pet` (internal RAM 0x119C..0x13DF) is the save state; about 130 names
lie inside it (`pet_weight`, `owner_name`, `rtc_month`, `evo_route` ...) and are defined by the
linker script as `pet + offset` (`config/ram.txt`). In a real source tree they would be fields:
`pet.weight`. Give `PetState` those fields (`tools/rename.py` handles fields) and write the
accesses as `pet.field`; the alias lines go away.

**Why it can stay byte-identical.** `pet + 0x30` and a symbol at that address assemble the same
(the ext optimizer adds the offset to the symbol's address). Accesses whose type differs from
the field's (u32 over four u8 fields: `pet_toilet_training`) need a cast or a union.

**Open.** Whether some of the names are separate variables after all (the save area may be
several structs); the field types from the widest accesses, as for the other structs.

## 3. Name the RAM bytes no code names (2026-10-16)

**What.** The RAM definitions keep the original layout with unnamed fillers: 42 `D_ADDR` byte
arrays in external RAM (declared in their file's header, `/* bytes no code names */`), and
padding rows in `config/ram.txt` for internal RAM. Some are the rest of an array whose size
the code does not show (the declared type is what the accesses use), some unused variables.
Also `FW_ADDR((u8 *)&friend_event + 0x4, 0xe14)` in town_places / diary_notebook: a buffer
after `friend_event` that has no name of its own.

**How.** Per filler: the functions around it (its object), the sizes the code implies (loops,
`memcpy` lengths). Merge into the neighbouring array where it clearly belongs.
