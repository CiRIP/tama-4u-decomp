# tama_clean

A cleaned-up, buildable C source of the Tamagotchi 4U+ firmware `FW_4UPlusJP.bin` (Epson
S1C33 CPU). It is a fork of the matching decompilation tama_decomp at commit
6d04943f77cffb5af8abfa11e6ef92b4bb0e312f, with everything that only served to reproduce the
original bytes removed. The image it builds is **not** byte-identical to the firmware; it is
meant to behave the same, and that is checked in an emulator against the original firmware
(see Validation).

## Build

Needs the Epson GNU33 toolchain in `C:/EPSON/GNU33` (or `make TOOLS=...`), Python 3, and your
own copy of the firmware as `FW_4UPlusJP.bin` in the repository root (it is not part of the
repository, nor are the assets taken from it).

    python tools/extract_assets.py      # once: graphics, sounds and save data into assets/
    C:/EPSON/GNU33/make -j              # build/image.bin, the whole 8 MB flash image

One compiler pass per file (gcc 3.3.2, `-O1`, no ext optimizer), one link with a hand-written
linker script (`ldscripts/firmware.ld`). After linking, `tools/romsum.py` stores the
program's checksum where the factory test mode expects it, `tools/apicheck.py` checks the
91 entry points of the downloaded-program API, and `tools/symfile.py` writes
`build/image.sym` for the emulator.

## Layout

* `src/<subsystem>/<subsystem>_<topic>.c`: the C, one file per object of the original link
  (`notes/source_files.md`), named by subsystem prefix (`pet_`, `room_`, `comm_`, `nfc_`, ...;
  `src/lib/` the code that was linked as libraries). Each file defines its RAM variables at
  its end. Game text is written as `_("...")` in the game's character set
  (`tools/charset.py`); `tools/textpp.py` turns it into bytes before compiling.
* `include/proto/<subsystem>/<file>.h`: what each source file defines; `structs.h` shared
  types; `flash.h` the flash data outside the program; `hw.h` the peripheral registers;
  `common.h` the basic types and `DSTRAM`.
* `asm/`: what stays assembly: the vector table, the downloaded-program API jump table
  (`api.s`), the code run from internal RAM (`iram.s`: display compositor, LCD flush, NOR
  flash erase / program, which must not run from flash), the sound tick interrupt entry, and
  four timing-critical IR routines (`asm/func/`, bit timing counted in instructions).
* `ldscripts/firmware.ld` (memory map and sections), `address_symbols.ld` (the fixed
  addresses of the flash data the program reads: pictures, sounds, save areas, the NOR command
  addresses), `aliases.ld` (names inside data objects, as symbol + offset).
* `tools/`: build helpers above, `extract_assets.py`, `tamago.py` (TAMAGO file reader),
  and the validation tools `emucompare.py`, `scenarios.py`, `emucover.py`.
* `notes/`: what is known about the firmware (`notes/README.md`), the names
  (`notes/names.txt`, address -> name), `notes/VALIDATION.md`, `notes/NAME_SUGGESTIONS.md`.

## What is fixed, and what the linker decides

Fixed by the hardware or by things outside this program, and kept:

* the vector table at 0x2400000 (the CPU boots from it); the image is the whole flash
  0x2000000..0x27FFFFF, gaps filled with 0xFF;
* the downloaded-program API: 91 entries at fixed addresses in 0x2400AD0..0x2401D92
  (`asm/api.s`, checked by `tools/apicheck.py`); downloaded programs keep their RAM at
  0x84740..0x847BF, which stays free;
* the code copied to internal RAM at 0 at start-up, and the flash layout: graphics and sound
  data, the save areas, the diary pages from 0x2570000 (the program must end before it), the
  NOR command addresses 0x2000AAA / 0x2000554;
* the save data's byte layout: `struct SaveBlock` (`include/proto/save/save_flash.h`), one
  struct holding the friend list, mail counters, album order and `struct PetState`, with the
  original offsets, so saves of the original firmware load and the other way round;
* hand-written timing code and its `nop` delays.

Everything else is placed by the linker: functions, read-only data, and all RAM variables
(internal RAM `.bss` after the RAM code; the variables the original kept in the 2 KB DST RAM
at 0x84000 are marked `DSTRAM`). No variable or function is pinned to an address.

## Validation

`tools/emucompare.py` runs this image and the original firmware side by side in the
emulator tamaemu (headless, lock-step, same button script, same starting save) and compares
the screen every snapshot, the stream of LCD commands and pixel bursts, the sound/timer
register writes, and the flash bytes each run changes (the save). The original runs with
tamaemu's `4u` device profile; this image with `4u-clean`, which takes the two firmware
addresses the emulator hooks (the reader routine of auto-touch and the display-sleep flag)
from `build/image.sym` (tamaemu branch `tama-clean-profile`). Timing is neutralised on both
sides: the reads of the free-running timer that seed `rand()` read 0, and the idle loops'
"stir" calls to `rand()` are removed, so code speed does not change random choices.
Scenarios (`tools/scenarios.py`, `python tools/emucompare.py --list`): first boot and
set-up, resume, the factory test mode (unpatched: it checksums the program), 40 random walks,
an edited adult pet and 12 walks with it, Touch Spot / NFC reader walks, the display going to
sleep, growing up over game days at 30x / 120x, and reboots from each image's own save.
Details and results: `notes/VALIDATION.md`.

    python tools/emucompare.py -j 22          # all scenarios; exit status 0 = all pass

## Names

Functions and data are named as in tama_decomp (English, `snake_case` with a subsystem
prefix, struct types `CamelCase`); what is not understood keeps its address name
(`func_XXXXXXX`, `D_XXXXXXX`; the addresses are the original firmware's). `notes/names.txt`
maps address names to names.
