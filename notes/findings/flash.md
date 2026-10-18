# Flash sector map

8 MB NOR at 0x2000000, bottom boot: sectors 1..8 are 8 KB, sectors 9..0x87 are 64 KB.
Sector numbers are 1-based as the firmware uses them (flash_sector_addr, the erase routine
func_0000A54(0x2000000, first, last)): sector k = 0x2000000 + (k - 1) * 0x2000 for k <= 8,
0x2000000 + (k - 8) * 0x10000 from 9 on. "Dump" = what FW_4UPlusJP.bin holds (everything
not listed as holding data is 0xFF there). Compiled 2026-10-18 from the erase calls,
item_bank_tbl (src/shop/shop_browser.c), item_record_by_index and the test-mode table
test_flash_areas.

| Sectors | Address | Size | Contents | Dump |
|---|---|---|---|---|
| 1-4 | 0x2000000 | 32 KB | downloaded program (game / outing) installed and run in place (dl_install_program, notes/findings/tamago.md) | empty |
| 5-7 | 0x2008000 | 24 KB | not used | empty |
| 8 | 0x200E000 | 8 KB | main save: pet state + friend list (save_game, sys_power_on) | empty |
| 9-0B | 0x2010000 | 192 KB | friend portraits, 17 x 0xE80 per sector (friend_portrait_store) | empty |
| 0C | 0x2040000 | 64 KB | scratch copy for rewriting one sector (flash_backup_sector / flash_restore_sector_except) | 8 KB (a TAMAGO record, config/assets.txt save_0040000) |
| 0D | 0x2050000 | 64 KB | temporary TAMAGO record (comm_make_random_friend, mail loading, reader-mode downloads up to 0xF000) | empty |
| 0E | 0x2060000 | 64 KB | scratch for rewriting a downloaded-character slot (dl_chara, 0x2060000 / 0x2068000) | empty |
| 0F | 0x2070000 | 64 KB | scratch for diary_delete_last; the debug "アイテムSEND" item (comm_payload_app_init) | empty |
| 10-2E | 0x2080000 | 1.9 MB | graphics (gfx_0080000, ends 0x226D890; notes/findings/assets.md) | data |
| 2F-33 | 0x2270000 | 320 KB | not used | empty |
| 34 | 0x22C0000 | 64 KB | erased by factory reset / test mode with 0x35-0x3B; no other user found | empty |
| 35 | 0x22D0000 | 64 KB | mail (メッセ): 100 x 100-byte records (mail_store) | empty |
| 36 | 0x22E0000 | 64 KB | as 0x34 | empty |
| 37-3B | 0x22F0000 | 320 KB | character album: 80 x 4 KB records (album_register) | empty |
| 3C-45 | 0x2340000 | 613 KB | built-in TAMAGO items + their tables (gfx_0340000, ends 0x23D92B0) | data |
| 46-47 | 0x23E0000 | 128 KB | not used | empty |
| 48-5E | 0x2400000 | 1.4 MB | the program: code, rodata, .data / .iram load images (ends 0x2563548; make TEXT_LANG=en ends 0x256E780) | data |
| 5F-61 | 0x2570000 | 192 KB | picture diary: 23 pages x 8 KB (diary_page_count, diary_store_page) | empty |
| 62 | 0x25A0000 | 64 KB | bank 15: seasonal decorations, 4 x 8 KB | empty |
| 63 | 0x25B0000 | 64 KB | bank 0: meals (shop stock), 20 x 2 KB | empty |
| 64 | 0x25C0000 | 64 KB | bank 1: snacks, 20 x 2 KB | empty |
| 65 | 0x25D0000 | 64 KB | inventory: food, 22 x 2 KB (item_record_by_index 0..21) | default food record written by factory reset (save_05D0000) |
| 66-68 | 0x25E0000 | 192 KB | inventory: clothes, 16 KB slots (index 22..; item_inv_clothes has 10) | empty |
| 69-6A | 0x2610000 | 128 KB | bank 2: clothes, 5 x 16 KB | empty |
| 6B-6D | 0x2630000 | 192 KB | inventory: accessories, 20 x 8 KB (37..56) | empty |
| 6E-6F | 0x2660000 | 128 KB | bank 3: accessories, 10 x 8 KB | empty |
| 70-73 | 0x2680000 | 256 KB | inventory: goods / toys, 25 x 8 KB (57..81) | empty |
| 74 | 0x26C0000 | 64 KB | bank 4: toys, 15 x 8 KB (slots 8..14 run on into sector 0x75) | empty |
| 75 | 0x26D0000 | 64 KB | bank 5: 5 x 8 KB (shares the sector with bank 4's tail; no pack file uses bank 5) | empty |
| 76 | 0x26E0000 | 64 KB | default background record written by factory reset (save_06E0000) | 8 KB |
| 77-78 | 0x26F0000 | 128 KB | bank 6: rooms / backgrounds, 10 x 8 KB (banks 11..14 point here with built-in items only) | empty |
| 79 | 0x2710000 | 64 KB | bank 7: downloaded minigames, 2 x 32 KB | empty |
| 7A-7B | 0x2720000 | 128 KB | bank 8: downloaded outings, 4 x 32 KB | empty |
| 7C | 0x2740000 | 64 KB | bank 9: letters, 40 x 1 KB; bank 16 (coupon) and inventory index 82 also start here; more records at +0x8000 | empty |
| 7D | 0x2750000 | 64 KB | bank 17: bingo card, 32 KB (inventory 93..96 = 4 x 32 KB from here) | empty |
| 7E | 0x2760000 | 64 KB | bank 18: gashapon, 32 KB | empty |
| 7F | 0x2770000 | 64 KB | bank 10: letters / pictures, 6 x 10 KB | empty |
| 80-83 | 0x2780000 | 256 KB | downloaded characters, 8 x 32 KB (dl_chara); 0x81-0x83 double as the scratch of diary_delete_page / diary_drop_oldest, both reachable only from code without callers (diary_viewer, diary_drop_oldest) | empty |
| 84 | 0x27C0000 | 64 KB | NFC session buffers, connection kind 0 (incoming at 0x27C8000) | empty |
| 85 | 0x27D0000 | 64 KB | NFC session buffers, other kinds (0x27D0400..; per-game stats 0x27D0800..) | empty |
| 86 | 0x27E0000 | 64 KB | flash_write_sector_86 buffer (ir_packets) | empty |
| 87 | 0x27F0000 | 64 KB | erased with 0x84 by comm_erase_tx_sectors | empty |

What gets erased:
* Factory reset (save_factory_reset) and the test-mode flash test: 1-4, 8 (test only), 9-0B,
  0C-0F (test only), 0x34-0x3B, 0x62-0x87. The picture diary 0x5F-0x61 is NOT erased by
  either.
* Nothing ever erases 0x10-0x33, 0x3C-0x5E (graphics, built-in items, program).

Consequences for a rebuilt image: the program must end before 0x2570000 (the program region
of ldscripts/firmware.ld ends there). Sectors 0x46-0x47 (128 KB) and 0x2F-0x33 (320 KB) are
never written by the firmware and are free for a larger program or assets if the linker script
places them there.

## Names in the source (tama_clean)

include/flash_map.h names every area: FLASH_SEC_X = first sector (FLASH_SEC_X_LAST for areas
of several sectors), FLASH_X = its address, FLASH_SECTOR(k) = start of sector k, FLASH_BASE =
the chip. The C reaches the areas only through these names: the erase calls
(func_0000A54(FLASH_BASE, first, last)), the sector arguments, item_bank_tbl,
test_flash_areas, the factory reset, and every address in an area (as `FLASH_X + offset`, or
the named objects of flash.h: comm_rx_phase1 / comm_rx_phase2, mail_records,
spot_bingo_card_slot, the downloaded-character slots, ...). No linker symbol is left for the
areas the firmware writes (ldscripts/address_symbols.ld keeps the assets and the NOR command
addresses 0x2000AAA / 0x2000554 the asm uses).

Still fixed elsewhere: flash_sector_offsets (src/ui/ui_message.c), which the IRAM erase routine
reads, is the chip geometry; the halfword counts of test_flash_areas; the program region of
ldscripts/firmware.ld (0x2400000, LENGTH 0x170000: it ends at the diary) and the factory-reset
records it places at 0x25D0000 / 0x26E0000.