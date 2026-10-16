# Validation

How this fork is checked against the original firmware, and what was found.

## Method

`tools/emucompare.py` runs the built image and `FW_4UPlusJP.bin` in tamaemu (headless,
`--no-state`, `--no-auto-link`, lock-step emulated time), with the same button script
(`--press`) and the same starting flash, and compares per scenario:

* **frames**: a screenshot every `snap` seconds of emulated time (pixel hash);
* **lcd**: the sequence of LCD controller commands and pixel-write bursts, without cycle
  counts (what was drawn and in which order, independent of timing);
* **tones**: the writes to the sound timers, in order per timer;
* **save**: the flash bytes the run changed;
* **end**: the last frame, the emulator's exit code and stop reason.

A frame that differs while the LCD streams agree is accepted as "mid-redraw" when each of its
pixels equals the original's frame one snapshot earlier, at the same time or one later, or
when it is a single differing snapshot between equal ones (a snapshot taken during a redraw a
few cycles earlier or later). Anything else fails.

**Emulator.** tamaemu, worktree `tamaemu-clean`, branch `tama-clean-profile`. The original runs
with the `4u` profile. This image runs with `4u-clean`: identical hardware, but the two
firmware addresses tamaemu hooks are looked up in `build/image.sym` (written by
`tools/symfile.py`): the bingo / Gashapon reader routine where auto-touch puts a store NFC
point in the field (`spot_touch_any_reader`; the end of the touch is the return address read
at its entry, as both of its call sites are in `spot_bingo_gashapon_menu`), and the display
state byte `--stay-awake` clears (`sys_display_state`). With those, both images run with
auto-touch and `--stay-awake` (except scenarios marked `awake=False`).

**Timing.** Code that is correct but faster or slower would make different random choices:
the firmware seeds `rand()` from the free-running counter of T16 ch.2 and advances it by that
counter in the set-up, and the idle loops call `rand()` once per turn only to stir it. Unless
`--no-patch`, both images are patched before they run: every 16-bit load from T16_TC2
(0x300794) loads 0 (8 sites in each image), and every `call rand` whose result is dropped
because the next instruction is a call or a jump that overwrites r4 becomes nops (3 sites
each: pet_care_mistake, the sys_main idle loop, the birthday loop of sys_startup_flow). The
factory test mode scenario runs unpatched (it checksums the program).

**Saves.** Chained scenarios (`after`) start both images from the save the original left at
the end of the named scenario, with this image's program sectors put in
(`transplant`), so this image also has to load the original's saves. Scenarios with
`own=True` start each image from the save it wrote itself (a reboot from its own save).

## Scenarios (76)

boot, walk (every keyboard row of the name entry), setup (first boot to the hatched baby),
testmode (unpatched), resume (continue a saved game, mood question), fuzz01..40 (seeded random
walks from the resumed game), grow_child (a game day at 30x), grow_adult (two game days at
120x, the pet grows up and leaves), adult (the resumed save edited into a fed adult with 9999
points) and afuzz01..12 (random walks with it), spot_menu (the communication menu's Touch Spot
entry with a store point in the field), touch01..06 and atouch01..03 (random walks with a
store point coming into the field), sleep (no stay-awake: the screen goes off, a key wakes
it), reboot_setup / reboot_fuzz / reboot_adult / reboot_touch / reboot_grown (power-on from
the save the image itself wrote).

## Results

Every commit after the build change was validated with the whole suite; the last runs:
76 of 76 pass (about 28 with "frames mid-redraw"). The log of every run is
`build/emu/validation.log` (one line per scenario).

## Coverage

`tools/emucover.py` hooks the entry of each function in `tools/validation/functions.txt` (the
104 functions that had no matching C in tama_decomp, so their C was never checked against
the original bytes) and runs the scenarios: 32 of the 104 are reached (the main loop, food,
care, status, the sound sequencer, the touch reader and idle-touch paths, the set-up text
input, the clock setting, the test mode, the departure screen ...). Not reached: the
toy-to-toy and legacy IR exchange (comm_*, ir_*), shops and Gotchi Reform, the game corner
and built-in games, the special items and bingo card screens, downloaded characters, card
writes. Most of the rest of the code (the functions that matched in tama_decomp) is plain C
whose compiled form changed with the build, and is covered by the same scenarios.

## Divergences found, and what they were

* **Emulator couplings to original addresses** (not firmware bugs): tamaemu's auto-touch hooks
  the PC 0x2514704 of the original, which in a rebuilt image is other code (it put a touch in
  the field in the middle of unrelated code); and `--stay-awake` clears RAM 0x1440. Fixed by
  the `4u-clean` profile.
* **Timer interleaving and `rand()` stirring**: handled as described under Timing.
* **The RAM layout** (when the variables stopped being pinned to the original addresses):
  - the birthday screen's four digit objects were stored through a one-byte variable into the
    three bytes after it (a filler); a black bar with a "1" appeared on the set-up screen.
    `sys_obj_ids[4]`;
  - the exchange record received over NFC was declared as 95 bytes while 0x180 are received
    and cleared; four variables named inside it (gender, name, owner id, profile offset)
    were separate variables in the fork. Random walks that met a store point failed. One
    0x180-byte `union CommPacket`;
  - found the same way, before they showed: game_slot_frame_objs / reel_objs [3],
    mail_card_line_objs [5], room_poop_objects [4], spot_record_class [2],
    friend_book_state (8 bytes), ir_partner_name [5], ir_tx_packet (20 bytes), the slot
    rounds and the item inventories as one table each, and the legacy IR name quirks (below).
  Method: `tamaemu --watch-gaps` (added in the worktree) logs every read and write of the
  bytes no variable of the original covers; run over all scenarios on the original firmware,
  plus PC hooks on memset / memcpy / memcmp / strncpy / strncmp reporting block operations that
  span two original variables. Each access found is now inside one variable of the fork.
* **An address used as a number**: the diary page store programmed
  `(address of sys_awake_seconds) / 2` halfwords; the original constant was 0x142A / 2 (the
  page size), written through the name of the RAM variable then at 0x142A. Not reached by a
  scenario; found by searching RAM symbols cast to integers.

## Quirks of the original, kept on purpose

* `sys_reset` clears the CMU gated-clock registers through a zeroed pointer: the last writes
  go to RAM address 0 (commented in place).
* `nfc_session_clear` does not clear `nfc_session` (its memset ran with a zero count).
* Legacy IR (old-generation exchange, not reachable on the 4U+): the owner name is copied as
  16 bytes from byte 5 of the 20-byte packet, so its last byte lands on the low byte of
  `tmr_t5_ticks` (`ir_tx_put_owner_name`); `friend_find_or_add` reads 16 name bytes where the
  packet has 5 or 15, so the bytes that followed them in the original RAM are reproduced
  (`ir_rx_name16`, `ir_partner_name16`).
* `snd_set_pitch` reads the state of the voice whose number is still in the argument register
  (`snd_voice_state(sel)`).

## Not covered

* Toy-to-toy communication (greeting, games, gifts, proposal, photo over NFC): tamaemu can
  link two cores in one process (`--link`; its headless mode did not reset core A, fixed in
  the worktree), but in this mode the two 4U+ cores both act as NFC initiators and the session
  fails ("つうしんしっぱい") for the original firmware as well, so there is nothing to compare.
  The comparison tool supports linked scenarios (`link=True`, `press_b`, `edits_b`) for when
  the emulator does. The phone app (SNEP) needs an injected peer; the legacy IR exchange
  cannot run on the 4U+ (its receive interrupt is not in the vector table).
* Touch cards (ISO-DEP), downloaded games and outings, the diary page store, the camera /
  album, most of the town places and the shops beyond what the random walks reach.
* Functions that fall off their end without a return value where a caller uses the
  result (empty stubs such as `diary_page_play_a`, `friend_event_hook_a`, `spi_recv_byte`,
  `town_hook_stub_a`; `friend_event_fire`, `chara_text`, `chara_class_field`,
  `ui_settings_theme` on some paths): the original returns whatever was left in r4, and so
  does this build, but the leftover value can differ.
