# Source files

The source files as the file analyses of 2026-10-16 reconstructed them (work/files/: four
independent analyses, merge/README.md, merge/plan.tsv). One file is one object of the original
link: one contiguous code range in one code section and one contiguous .rodata stretch
(config/objects.txt, tools/files.py). The order of the files in each section and in .rodata is
the original's; only the boundaries between neighbours were open. Folders are subsystems
(the name prefixes); src/lib/ holds what was built as libraries (config/library.txt): the NFC
hardware unit, the NFC protocol library, the sound library and Epson's sys.c. src/unplaced/
holds .rodata-only files whose code is not identified. Base names are unique on purpose: the
2-pass ext optimizer goes by gcc's .file, which has no directory (tools/gen_lds.py).

| file | was | code | .rodata | what | open |
|---|---|---|---|---|---|
| src/api/api_entries.c | F8a_2400B50 | 2400B50..2401DA0 | 2538994 | Entry points for downloaded games and outings (and the built-in API-style games): thin wrappers over buttons, pet stats, clock, timers, sound, display objects, animation scenes and character sprites. |  |
| src/anim/anim_engine.c | F40_2402266 | 2401DD6..240413C | 255C800 | The display-object and animation engine behind the generated scripts: display list reset, object create/set/move, tint and palette set-up, LCD init, scene set-ups for every room and place, sprite/picture/text drawing in scenes, keyframe wait and sound queueing. |  |
| src/item/item_use.c | G_2404146, F18_2404578 | 2404146..24057F8 | 253BF9C | Using items from the inventory: effects and preference reactions with the item animations, item cards and names, the usable-item pickers and the special-items browser. |  |
| src/pet/pet_core.c | G_2405882, G_24058A4, F30_2405970, G_2408726 | 2405882..240883A | 2542844 | The pet's core state and the character data it reads: character table look-ups (class, gender, birthday, family, texts), happiness, hunger, weight, points, sickness, poop and sleep, the per-minute and per-clock-event updates, unlock flags, item counts, the album register, random helpers, and the big tables of character and game texts (fortunes, tips, mail templates). |  |
| src/room/room_main.c | G_24088C0, F21_240895A | 24088C0..240C616 | 253C844 | The main loop and main screen: sys_main, the icon bars and cursor, new-screen set-up, the speech bubble widgets, display sleep / wake and power down, idle tick, the care-event dispatch, poke reactions and the NFC idle-touch arming. |  |
| src/chara/chara_draw.c | G_240C730, F5_240C738 | 240C730..240D768 | 2535F4C | Drawing the pet and up to four other characters with their clothes and accessories (pose pictures, palettes, downloaded characters), and the wardrobe menus that change the outfit and iterate over the owned clothes and accessories. |  |
| src/pet/pet_bath.c | G_240D976 | 240D976..240DB12 | - | The bath care action (bath scene, bath training and praise window), the bedroom background of the sleeping scene and the Christmas animation wrapper. |  |
| src/food/food_meals.c | F9_240DB16 | 240DB16..240F184 | 25391D8 | Meals: the food menu (meal or snack from the inventory), eating with the like / dislike reactions, the restaurant visit, and the birthday / party dinners with guests and the kitchen styles. |  |
| src/room/room_draw.c | G_240F2DE, F20a_240F2E8, G_240F784 | 240F2DE..240F784 | 253C63C | The living room: background with the seasonal decorations, the poops and their blinking, and the characters' birthday event on the main screen. |  |
| src/room/room_window.c | F24_240F818 | 240F818..2410F86 | 253EFF8 | What the room's window shows (weather by period and month, passers-by, seasonal flower), the daily random event that sends visitors, the signpost outings and their two destinations (Tamamori shop, Gotchi Reform). |  |
| src/town/town_places.c | F28_24110A6, G_24147E8 | 24110A6..241488C | 2541848 | The town: the place menu and its places (park and plaza, sports ground, lottery, school and clubs, Tama-Depa floors, matchmaking, transformation booth), jobs, outings with their diary pages, and the street encounters. |  |
| src/shop/shop_reform.c | F32_241489E | 241489E..24152BE | 255BD48 | Gotchi Reform: browsing and paying for room designs, the room-design tables used by every scene, and the generic do / yes-no confirmation screens it uses. |  |
| src/album/album_store.c | F35_24154DC, G_24157E0 | 24154DC..2415804 | 255C258 | Storing photos in the album: free entry search, writing a record, and the 'storage full: pick a photo to erase' dialog. |  |
| src/shop/shop_browser.c | F36_24159A4 | 241585C..2416230 | 255C2C0 | The item-bank browsers and the shops: the purchase confirmation, the discount (sale) mode, browsing the occupied slots of a bank with item cards, the small two-entry menu, the fashion shop (clothes / accessories) and the play-tools shop. |  |
| src/diary/diary_notebook.c | F53_241628E, G_241AD5C | 241628E..241AD5C | 255D614 | The notebook: picture-diary pages (composition, rasterising, storage, viewer), the friend book with profiles, messages and farewells, the mail list, the photo album, and the notebook menu. |  |
| src/mail/mail_letters.c | F55_241AF9A, G_241B8A2 | 241AF9A..241B8A2 | 255F2D4 | Letters: letters from friends and the farewell letter, the letter box with item letters and their descriptions, read flags, and the coupon box. |  |
| src/pet/pet_toilet.c |  | 241B91C..241B9E2 | - | The toilet care action (pet_toilet) and the falling-object animation (anim_falling_object). |  |
| src/rtc/rtc_clock_screen.c | F59_241BA86, G_241BDA8 | 241BA86..241C3D2 | 255F718 | The clock screen: drawing the time with digit pictures, blinking, day clamping, the clock setting and the LCD brightness setting. |  |
| src/anim/anim_scripts.c | F0_241C5FA | 241C5FA..24F507E | 2535B88 | The generated animation scripts: one function per scene (events, visitors, meals, care, town places, items, pokes, messages), each setting up a scene and drawing keyframes through the animation engine. |  |
| src/ui/ui_text_banner.c | G_24F51C8 | 24F51C8..24F51C8 | - | ui_text_banner: a two-line text on a decorated background (the fortune lines). | a file of its own (R1?) or the end of anim_scripts. |
| src/snd/snd_data.c |  | 24F529C..24F52F4 | - | The sound driver's note period tables and the sequence data of its sound bank: data in the code section. |  |
| src/sys/sys_boot.c | G_24F61BC | 24F61BC..24F659E | - | Start-up: the reset handler, bus / interrupt controller / GPIO / timer set-up, copying the RAM code and .data, clearing .bss, interrupt enable / disable, and the A/D converter set-up. |  |
| src/evo/evo_counters.c | G_24F65A2 | 24F65A2..24F674C | - | Counters and flags for the special-form evolution (item uses, games, baths, meetings, daily countdown) and their reset. |  |
| src/evo/evo_special.c | F4_24F6792, G_24F6FCC | 24F6792..24F70EC | 2535F10 | Special-form evolution: the condition checks, slot handling and the choice of the next character, and the evolve / revert actions. |  |
| src/sys/sys_low_power.c | G_24F714C | 24F714C..24F71D8 | - | Low-power mode: sleep on OSC1, resume on OSC3, leaving low power (sound off, clock back to 30 MHz) and an empty function (removed). |  |
| src/comm/comm_menu.c | F7_24F71DA | 24F71DA..24F913A | 2536DD4 | The communication menu and its sessions (greeting, item exchange, games, photo, proposal, app, debug), the handling of received items (capsules, stamps, letters), and a run of shared helpers at its end (button clear, flash begin/end, byte order, picture slots, traits). |  |
| src/ir/ir_session.c | F7b_24F9218 | 24F9218..24FC3B2 | 25381D8 | The legacy IR exchange session: running a session, the packet exchange and record checks, point gifts, the IR test mode, and visits started by an exchange. |  |
| - (was src/util/util_empty.c) | G_24FC4F2 | 24FC4F2..24FC4F2 | - | An empty function called twice by the main screen; removed with its calls. | the end of ir_session or the start of ui_message (one empty function). |
| src/ui/ui_message.c | F8b_24FC4F4 | 24FC4F4..24FE038 | 2538F24 | The scrolling message dispatcher: ui_show_message picks the message scene for each message kind (visitors, town places, care, communication...) and plays it with a click on A/B; also the merchant and training message scenes. |  |
| src/pet/pet_gone.c | G_24FE0C6, R10 | 24FE0C6..24FE21C | 253A4D0 | The pet-left screen and the search question that may bring it back. |  |
| src/txt/txt_input.c | F11_24FE82A | 24FE82A..24FF5A2 | 253A4D4 | Text input: the character-grid keyboard (hiragana, katakana, symbols, latin), cursor and confirmation, used for names and messages. |  |
| src/sys/sys_interrupts.c | G_24FF904, R12 | 24FF904..2500436 | 253B244 | Interrupt handlers: the RTC second / minute tick that drives the pet clock, button and timer interrupts, traps, the A/D interrupt, and idle housekeeping. |  |
| src/ir/ir_profile_record.c | G_250043A, R13 | 250043A..250090C | 253B248 | Writes our 0x6C-byte profile block of the old serial (IR) protocol into a record, padding it; followed by three empty functions (removed) and two stubs that return 0. |  |
| src/comm/comm_record_pics.c | F14_2500910 | 2500910..2500C9A | 253B24C | Pictures for the exchange records: a sample TAMAGO record (test), filling a record, the pose and picture lists rendered into the record, plus the short IR pauses with animation and the gift bonus scene. |  |
| src/chara/chara_render_flash.c | G_2500E7E, R15 | 2500E7E..25029FA | 253B738 | Rendering characters and pictures into flash records: the gift sector, picture records, the five pose sets and their flipped variants (a variant of gfx_anim's pose rendering with the same pose table); then screen read-back, the battery check and the photo. | where this file ends and comm_flash_buffers begins (somewhere in 25020BA..25029FA), and whether the first two functions are comm_record_pics's. |
| src/comm/comm_flash_buffers.c | G_25029FE, R15b | 25029FE..2502CF0 | 253B78C | Flash buffers of an exchange (erase tx / rx sectors, append, markers, IR receive blocks) and the exclusive-timing mode for sessions. |  |
| src/ir/ir_receive.c | F16_2502DCE | 2502DCE..2503EFC | 253BEAC | Receiving over IR: finding the marker and feeding received bytes, the directory and status words, applying points, partner pictures, friend events and their minute check, stamp peers, item checks and the packet-kind decisions of the exchange. |  |
| src/ir/ir_packets.c | G_2503F2E | 2503F2E..2504824 | - | Legacy IR packets: building requests and answers, item packets, storing attached files, received diary pages and messages, plus small session helpers. | the end of ir_receive, or a file of its own (R17). |
| src/ir/ir_driver.c | G_2504844 | 2504844..25051CE | - | The IR port driver: carrier, edge interrupt, timer 5 ticks, packet send / receive with checksum, button history and critical sections. |  |
| src/owner/owner_mood.c | F19_25051F0 | 25051F0..25051F0 | 253C400 | The owner's mood setting screen. |  |
| src/dl/dl_chara.c | G_2505414, F20b_2505594, G_250629E | 2505414..2506658 | 253C4E4 | Downloaded characters: descriptions and profile cards, affection, class and gender marks, storing into slots, the read / delete menu and the list screen. |  |
| src/ui/ui_marks.c | G_250669A, R22 | 250669A..25067E2 | 253EFAC | Notification marks on the icon bars (food, touch, town, special, notebook, mail). |  |
| src/ui/ui_menu.c | F23_25068E2, G_2507710 | 25068E2..2507C06 | 253EFB0 | Generic menus and dialogs: framed and plain menus, the message box, two-line questions, yes / no and use / no / delete choices, a three-entry menu and a paged list. |  |
| src/visit/visit_guests.c | F25_2507FBE | 2507FBE..2508B14 | 2540F5C | Visitors at the house: the garden visitor who buys an item, the travelling merchant who sells one, and the scheduled visitor, with the item helpers they need. |  |
| src/visit/visit_gift.c | G_2508D66, R26 | 2508D66..2508D68 | 254182C | A visitor who brings a gift (and an empty function, removed). |  |
| src/visit/visit_friend.c | F27_2508DC0 | 2508DC0..2508EEC | 2541830 | A friend from the friend book comes to visit, with a message scene. |  |
| src/town/town_street.c | G_2508FEE, F29_2509048 | 2508FEE..2509EA0 | 25426D4 | The street and the farm: street encounters and friendships, farm planting and harvest, outing timing checks, and the fountain with its honesty test. |  |
| src/chara/chara_select.c | G_2509FD0, R31 | 2509FD0..250B316 | 255B4D8 | town_visit_place, then becoming a character, sprite-set selection for the pet and the other characters, new game / new egg, the stage changes baby -> child -> adult and the next generation. | whether 2509FD0 and town_visit_place (2509FD4) are the end of town_street. |
| src/util/util_swap.c | G_250B3EA | 250B3EA..250B460 | - | A patch-list routine and byte swaps, all uncalled. |  |
| src/rtc/rtc_driver.c | G_250B46A, F33_250B670, G_250B6DC | 250B46A..250B6DC | 255BF20 | The real-time clock driver: reset and interrupt control, reading and setting the time, day period, and the date roll-over with leap years. |  |
| src/save/save_flash.c | G_250B78E, F34_250B8F4, G_250D866, G_250DA08 | 250B78E..250DA4C | 255BF84 | Saving and the records in flash: factory reset, erase confirmation, power-on, save_game, sector backup / restore, item banks and inventories (find, add, receive, discard), seasonal decorations, downloaded program install, gashapon / bingo records and friend portraits. |  |
| src/snd/snd_play.c | G_250DB16, F37_250DB8E, G_250DC10 | 250DB16..250DC28 | 255C510 | The sound front end: the bank of 83 sounds, playing a sound, waiting for its end, the error beep and the sound initialisation on top of the sequencer library. |  |
| src/spi/spi_flash.c | G_250DC34, R38 | 250DC30..250DF3C | 255C7F8 | Vestigial SPI flash driver (enable, chip select, byte transfer, read id / status, sector commands, page read / program); never called. |  |
| src/sys/sys_startup.c | G_250DF58, R39 | 250DF58..250DF58 | 255C7FC | The start-up flow after power-on: first-boot set-up (clock, birthday, name), new game or continue. |  |
| src/test/test_debug_chara.c | G_250E980, R41 | 250E980..250E980 | 255C8C8 | Debug menu entry: set the pet to a chosen character and stage. |  |
| src/spot/spot_events.c | G_250ECDC, F42_250ED06, G_250F122 | 250ECDC..250F122 | 255C8CC | Events of an idle touch or Touch Spot: the traveller and his souvenir, the fortune cookie, random idle scenes, profile talk, phrases, and the score rewards. |  |
| src/pet/pet_care_room.c | F43_250F1EE | 250F1EE..250F4F2 | 255CA0C | The care menu of the room (toilet, bath / teeth, cleaning locker, closet) and the cleaning action. |  |
| src/pet/pet_care.c | G_250F506, F44_250F616 | 250F506..25109EE | 255CA60 | The care menu (praise, scold, talk, comfort), the sitter (call, questions, return, takeover) and the request overlays drawn on the room (sitter, toilet, bath, cleaning, praise). |  |
| src/spot/spot_bingo_menu.c | F45_2510B0A | 2510B0A..2510B0A | 255CFAC | The bingo / Gashapon menu: daily bingo plays and tickets. |  |
| src/pet/pet_status.c | G_2510E24, F46_2510EDE | 2510E24..2511372 | 255D024 | The pet status pages: hearts, profile, marks and clover. |  |
| src/owner/owner_profile.c | F47_251149E | 251149E..2512CBC | 255D0A4 | The owner's profile editor and the friend profile view. |  |
| src/ui/ui_settings.c | F48_25133C4 | 25133C4..25136CC | 255D23C | The settings menu: theme, brightness, sound, and the clock (blocked during visits). |  |
| src/item/item_special.c | F49_25136DA | 25136DA..2513DD8 | 255D298 | The 'special' items: the bingo card viewer, the coupon viewer and its erase, and which of the two is missing. |  |
| src/ui/ui_yes_no.c | F50_2513E1C | 2513E1C..2513E1C | 255D304 | The yes/no question screen used by the departure, downloaded-characters and spot screens. |  |
| src/pet/pet_see_off.c | G_2513FC6 | 2513FC6..2513FC6 | - | The ten-day see-off event (pet_see_off): the pet's departure wish, the see-off scenes, the life story and the next generation's egg. | the end of ui_yes_no or the start of spot_reader. |
| src/spot/spot_reader.c | F51_25140E8 | 2514036..2518658 | 255D344 | The Touch Spot / card reader: reading a tag or card, sums and checks, the content browser (characters and items of a card), bingo card install, Gashapon tickets, visitor messages, and the reader test screens of the test mode. |  |
| src/mail/mail_edit.c | G_2518760, F52_2518958, G_25191AC | 2518760..251926E | 255D5C0 | Friend messages: the message card, loading and editing a message. |  |
| src/item/item_record_browser.c | F54_25193BE, G_25196A4 | 25193BE..25196A4 | 255F2B0 | Browsing the special-area TAMAGO records (decorations) with delete. |  |
| src/test/test_mode.c | F56_2519F2E | 25196DC..251A41E | 255F64C | The factory test mode: entry loop and ROM sums, flash erase-all (factory reset), battery, sound, RTC count tests. |  |
| src/tmr/tmr_timeouts.c | G_251A566 | 251A566..251A650 | - | Millisecond timers and timeouts on T16 ch.3 / ch.5, delays, and the LCD backlight level (requested and applied). |  |
| src/game/game_menu.c | G_251A6E0, R60 | 251A6E0..251A9BA | 255FA00 | The game corner menu: the two built-in games and the two downloaded game slots, drawing the page, running the chosen game. |  |
| src/game/game_timing.c | G_251AF66, R61 | 251AF66..251C29A | 255FA80 | The timing mini-game played as a communication game round. |  |
| src/game/game_training.c | F62_251C5DC | 251C5DC..251D2E4 | 255FA84 | The sports-ground training mini-game, written against the game API. |  |
| src/spot/spot_bingo.c | F63_251D666, G_251DB6E | 251D666..251DB6E | 255FAD8 | The bingo card: marking numbers, prizes and drawing the card; with erasing a card. |  |
| src/spot/spot_gashapon.c | F64_251F41A | 251DB9A..251F41A | 255FB24 | Tama Gashapon: the draw, its scenes, and the scrolling text with the pet. |  |
| src/game/game_builtin.c | G_251F560, F65_251FF4E | 251F560..25276F8 | 255FB44 | The built-in games written against the API like downloaded games: cake, treasure hunt and quiz, with their scenes. |  |
| src/comm/comm_outcome.c | G_2527918, F66_2527AA0, G_25290F6 | 2527918..25294CE | 2562140 | The outcome of a communication: greeting (profile card, fortune), gift, proposal / marriage, app, game rounds, photo; the idle-touch event; friend book update; and the flash helpers used while building records. |  |
| src/comm/comm_validate.c | F67_25294F4 | 25294F4..25294F4 | 25623D4 | Validation of a received exchange record (magic, sizes, sum). |  |
| src/comm/comm_checks.c | G_25295E6, R68 | 25295E6..25298CC | 25624E0 | The receiver's checks on a received phase-2 record (attached item flag, no food to the app), the peer phase check, counting a completed communication (with and without saving) and storing the peer's mail. |  |
| src/nfc/nfc_session.c | F69_25298CE, G_252A752 | 25298CE..252AA4C | 25624E4 | NFC sessions: session screen, field detection and peer finding, power-up, send / receive, initiator and target sessions, the menu session, the idle sessions and the NFC link tests of the test mode. |  |
| src/comm/comm_record_sum.c |  | 252AAC2..252AAEC | - | The additive sum of an exchange record and its check (comm_record_sum, comm_record_check_sum). | the end of nfc_session or the start of comm_exchange. |
| src/comm/comm_exchange.c | F70b_252AB30 | 252AB30..252C57C | 2562558 | The record exchange: building the header and sections (mail, profile, images, poses, proposal, item), the payloads per session type, the three phases, and the initiator / target exchange. |  |
| src/nfc/nfc_link.c | G_252C5A6 | 252C5A6..252CFD8 | - | The PN512 UART link: opening and closing the link at 460k / 921k, sending and receiving frames, the field-detect interrupt. |  |
| src/lib/nfc_hw/nfc_hw.c | G_252D204 | 252D204..252D3F2 | - | NFC hardware unit: UART interrupts, the 100 ms tick, PN512 power and init. |  |
| src/lib/nfc/nfc_reader.c | G_252D75E | 252D75E..252D9C0 | - | NFC reader-mode API: sleep until field, abort, card parameters, poll / read / write a tag. |  |
| src/lib/nfc/nfc_dep.c | G_252DAE4 | 252DAE4..252ECE0 | - | NFC library: card APDU sessions and NFC-DEP (detect initiator / target, send, receive). |  |
| src/lib/nfc/nfc_snep.c | F70d_252F292 | 252F292..252FE2C | 256261C | SNEP server and client over LLCP for the phone app link. |  |
| src/lib/nfc/nfc_tags.c | G_25305C2 | 25305C2..2532650 | - | Reader-mode tag access: the last raw frame, NFC-A activation, Type 2 tag identification and read / write, the FeliCa poll, MIFARE write. |  |
| src/lib/nfc/nfc_protocol.c | G_253284A | 253284A..25350DE | - | Protocol frames: ISO-DEP card commands, FeliCa SENSF, NFC-DEP ATR, LLCP and SNEP frames, PN512 initiator set-ups. |  |
| src/lib/snd/snd_tick.c | G_253520C | 253520C..25352A2 | - | Sound library: the sequencer tick timer (T16 ch.4). |  |
| src/lib/snd/snd_ch_on.c | G_25352AE | 25352AE..253540A | - | Sound library: tone channel on. |  |
| src/lib/snd/snd_ch_off.c | G_253545E | 253545E..253550A | - | Sound library: tone channel off. |  |
| src/lib/snd/snd_ch_pitch.c | G_2535542 | 2535542..253558A | - | Sound library: tone channel period. |  |
| src/lib/snd/snd_ch_restart.c | G_25355E6 | 25355E6..2535610 | - | Sound library: tone channel restart. |  |
| src/lib/snd/snd_sequencer.c | G_253561E | 253561E..2535B6C | - | Sound library: the sequencer (voices, notes, tick interrupt, end callback). |  |
| src/unplaced/R1.c | R1 | no code identified | 2535F04 | A source file whose .rodata is only the per-file marker (0x2535F04); its code is not identified. |  |
| src/unplaced/R2.c | R2 | no code identified | 2535F08 | A source file whose .rodata is only the per-file marker (0x2535F08); its code is not identified. |  |
| src/unplaced/R3.c | R3 | no code identified | 2535F0C | A source file whose .rodata is only the per-file marker (0x2535F0C); its code is not identified. |  |
| src/unplaced/R6.c | R6 | no code identified | 2536DD0 | A source file whose .rodata is only the per-file marker (0x2536DD0); its code is not identified. |  |
| src/unplaced/R17.c | R17 | no code identified | 253BF98 | A source file whose .rodata is only the per-file marker (0x253BF98); its code is not identified. |  |
| src/sys/sys_iram_rodata.c | R37b | no code identified | 255C664 | The .rodata of the code copied to internal RAM at boot (asm/iram.s: the display-object compositor, the dirty-rectangle LCD flush and the NOR flash erase / program routines that must not run from flash): its jump table D_255C7DC. |  |
| src/unplaced/R57.c | R57 | no code identified | 255F710 | A source file whose .rodata is only the per-file marker (0x255F710); its code is not identified. |  |
| src/unplaced/R58.c | R58 | no code identified | 255F714 | A source file whose .rodata is only the per-file marker (0x255F714); its code is not identified. |  |
