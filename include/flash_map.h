/* The NOR flash map (notes/findings/flash.md): 8 MB at FLASH_BASE, sectors numbered from 1
 * as the firmware numbers them (flash_sector_addr, the erase routine func_0000A54(FLASH_BASE,
 * first, last)): 1..8 are 8 KB, 9..0x87 are 64 KB, the layout of the table
 * flash_sector_offsets. FLASH_SEC_X is the first sector of area X (FLASH_SEC_X_LAST the last
 * one of an area of several sectors) and FLASH_X its address. */
#ifndef FLASH_MAP_H
#define FLASH_MAP_H

#define FLASH_BASE 0x2000000
/* start of sector k (a constant k: flash_sector_addr does the same through the table) */
#define FLASH_SECTOR(k) (FLASH_BASE + ((k) <= 8 ? ((k) - 1) * 0x2000 : ((k) - 8) * 0x10000))

/* downloaded game / outing, installed and run in place (dl_install_program) */
#define FLASH_SEC_DL_PROGRAM 0x01
#define FLASH_SEC_DL_PROGRAM_LAST 0x04
/* the main save: pet state and friend list (save_game) */
#define FLASH_SEC_SAVE 0x08
/* friend portraits, 17 records of 0xE80 bytes per sector */
#define FLASH_SEC_PORTRAITS 0x09
#define FLASH_SEC_PORTRAITS_LAST 0x0B
/* copy of one sector while it is rewritten (flash_backup_sector) */
#define FLASH_SEC_SECTOR_COPY 0x0C
/* a temporary TAMAGO record: new friend, mail loading, reader-mode downloads */
#define FLASH_SEC_TEMP_RECORD 0x0D
/* copy while a downloaded-character slot is rewritten (dl_chara) */
#define FLASH_SEC_CHARA_COPY 0x0E
/* copy for diary_delete_last; the debug "アイテムSEND" item (comm_payload_app_init) */
#define FLASH_SEC_DIARY_COPY 0x0F
/* graphics (assets) */
#define FLASH_SEC_GFX 0x10
#define FLASH_SEC_GFX_LAST 0x2E
/* the records the factory reset erases with the mail and the album (0x34 and 0x36 have no
 * other user) */
#define FLASH_SEC_RECORDS 0x34
#define FLASH_SEC_RECORDS_LAST 0x3B
/* mail: 100 records of 100 bytes */
#define FLASH_SEC_MAIL 0x35
/* character album: 80 records of 4 KB */
#define FLASH_SEC_ALBUM 0x37
#define FLASH_SEC_ALBUM_LAST 0x3B
/* built-in TAMAGO items (assets) */
#define FLASH_SEC_BUILTIN_ITEMS 0x3C
#define FLASH_SEC_BUILTIN_ITEMS_LAST 0x45
/* the program */
#define FLASH_SEC_PROGRAM 0x48
#define FLASH_SEC_PROGRAM_LAST 0x5E
/* picture diary: 23 pages of 8 KB */
#define FLASH_SEC_DIARY 0x5F
#define FLASH_SEC_DIARY_LAST 0x61
/* from here to the end: item banks, inventories, downloads, communication buffers (all
 * erased by the factory reset) */
#define FLASH_SEC_USER 0x62
#define FLASH_SEC_USER_LAST 0x87
/* item banks (item_bank_tbl) and inventories (item_record_by_index) */
#define FLASH_SEC_DECORATIONS 0x62      /* bank 15: 4 x 8 KB */
#define FLASH_SEC_MEALS 0x63            /* bank 0: 20 x 2 KB */
#define FLASH_SEC_SNACKS 0x64           /* bank 1: 20 x 2 KB */
#define FLASH_SEC_INV_FOOD 0x65         /* inventory 0..21: 2 KB slots */
#define FLASH_SEC_INV_CLOTHES 0x66      /* inventory 22..: 16 KB slots */
#define FLASH_SEC_INV_CLOTHES_LAST 0x68
#define FLASH_SEC_CLOTHES 0x69          /* bank 2: 5 x 16 KB */
#define FLASH_SEC_CLOTHES_LAST 0x6A
#define FLASH_SEC_INV_ACCESSORIES 0x6B  /* inventory 37..56: 8 KB slots */
#define FLASH_SEC_INV_ACCESSORIES_LAST 0x6D
#define FLASH_SEC_ACCESSORIES 0x6E      /* bank 3: 10 x 8 KB */
#define FLASH_SEC_ACCESSORIES_LAST 0x6F
#define FLASH_SEC_INV_GOODS 0x70        /* inventory 57..81: 8 KB slots */
#define FLASH_SEC_INV_GOODS_LAST 0x73
#define FLASH_SEC_TOYS 0x74             /* bank 4: 15 x 8 KB (runs on into 0x75) */
#define FLASH_SEC_BANK5 0x75            /* bank 5: 5 x 8 KB */
#define FLASH_SEC_DEFAULT_BG 0x76       /* the default background record (factory reset) */
#define FLASH_SEC_ROOMS 0x77            /* bank 6: 10 x 8 KB; banks 11..14 */
#define FLASH_SEC_ROOMS_LAST 0x78
#define FLASH_SEC_GAMES 0x79            /* bank 7: downloaded minigames, 2 x 32 KB */
#define FLASH_SEC_OUTINGS 0x7A          /* bank 8: downloaded outings, 4 x 32 KB */
#define FLASH_SEC_OUTINGS_LAST 0x7B
#define FLASH_SEC_LETTERS 0x7C          /* bank 9: 40 x 1 KB; bank 16 (coupon); inventory 82 */
#define FLASH_SEC_BINGO 0x7D            /* bank 17: the bingo card; inventory 93..96 */
#define FLASH_SEC_GASHAPON 0x7E         /* bank 18: the gashapon ticket */
#define FLASH_SEC_PHOTOS 0x7F           /* bank 10: letters / pictures, 6 x 10 KB */
/* downloaded characters, 8 slots of 32 KB (dl_chara) */
#define FLASH_SEC_DL_CHARA 0x80
#define FLASH_SEC_DL_CHARA_LAST 0x83
/* sectors 0x81..0x83 also take the copy of the diary in diary_delete_page / diary_drop_oldest */
#define FLASH_SEC_DIARY_DELETE_COPY (FLASH_SEC_DL_CHARA + 1)
/* communication buffers (the areas belong to the connection kinds, comm_erase_session_buffers):
 * TX / TX2 erased together (comm_erase_tx_sectors), RX / RX2 (comm_erase_rx_sectors); RX2 is
 * the downloaded-character sector of slots 4 and 5 */
#define FLASH_SEC_COMM_TX 0x84
#define FLASH_SEC_COMM_RX 0x85
#define FLASH_SEC_COMM_RX2 (FLASH_SEC_DL_CHARA + 2)
#define FLASH_SEC_GIFT 0x86             /* a received item file (flash_write_sector_86) */
#define FLASH_SEC_COMM_TX2 0x87

#define FLASH_DL_PROGRAM FLASH_SECTOR(FLASH_SEC_DL_PROGRAM)     /* 0x2000000 */
#define FLASH_SAVE FLASH_SECTOR(FLASH_SEC_SAVE)                 /* 0x200E000 */
#define FLASH_PORTRAITS FLASH_SECTOR(FLASH_SEC_PORTRAITS)       /* 0x2010000 */
#define FLASH_SECTOR_COPY FLASH_SECTOR(FLASH_SEC_SECTOR_COPY)   /* 0x2040000 */
#define FLASH_TEMP_RECORD FLASH_SECTOR(FLASH_SEC_TEMP_RECORD)   /* 0x2050000 */
#define FLASH_CHARA_COPY FLASH_SECTOR(FLASH_SEC_CHARA_COPY)     /* 0x2060000 */
#define FLASH_DIARY_COPY FLASH_SECTOR(FLASH_SEC_DIARY_COPY)     /* 0x2070000 */
#define FLASH_GFX FLASH_SECTOR(FLASH_SEC_GFX)                   /* 0x2080000 */
#define FLASH_RECORDS FLASH_SECTOR(FLASH_SEC_RECORDS)           /* 0x22C0000 */
#define FLASH_MAIL FLASH_SECTOR(FLASH_SEC_MAIL)                 /* 0x22D0000 */
#define FLASH_ALBUM FLASH_SECTOR(FLASH_SEC_ALBUM)               /* 0x22F0000 */
#define FLASH_BUILTIN_ITEMS FLASH_SECTOR(FLASH_SEC_BUILTIN_ITEMS) /* 0x2340000 */
#define FLASH_PROGRAM FLASH_SECTOR(FLASH_SEC_PROGRAM)           /* 0x2400000 */
#define FLASH_DIARY FLASH_SECTOR(FLASH_SEC_DIARY)               /* 0x2570000 */
#define FLASH_USER FLASH_SECTOR(FLASH_SEC_USER)                 /* 0x25A0000 */
#define FLASH_DECORATIONS FLASH_SECTOR(FLASH_SEC_DECORATIONS)   /* 0x25A0000 */
#define FLASH_MEALS FLASH_SECTOR(FLASH_SEC_MEALS)               /* 0x25B0000 */
#define FLASH_SNACKS FLASH_SECTOR(FLASH_SEC_SNACKS)             /* 0x25C0000 */
#define FLASH_INV_FOOD FLASH_SECTOR(FLASH_SEC_INV_FOOD)         /* 0x25D0000 */
#define FLASH_INV_CLOTHES FLASH_SECTOR(FLASH_SEC_INV_CLOTHES)   /* 0x25E0000 */
#define FLASH_CLOTHES FLASH_SECTOR(FLASH_SEC_CLOTHES)           /* 0x2610000 */
#define FLASH_INV_ACCESSORIES FLASH_SECTOR(FLASH_SEC_INV_ACCESSORIES) /* 0x2630000 */
#define FLASH_ACCESSORIES FLASH_SECTOR(FLASH_SEC_ACCESSORIES)   /* 0x2660000 */
#define FLASH_INV_GOODS FLASH_SECTOR(FLASH_SEC_INV_GOODS)       /* 0x2680000 */
#define FLASH_TOYS FLASH_SECTOR(FLASH_SEC_TOYS)                 /* 0x26C0000 */
#define FLASH_BANK5 FLASH_SECTOR(FLASH_SEC_BANK5)               /* 0x26D0000 */
#define FLASH_DEFAULT_BG FLASH_SECTOR(FLASH_SEC_DEFAULT_BG)     /* 0x26E0000 */
#define FLASH_ROOMS FLASH_SECTOR(FLASH_SEC_ROOMS)               /* 0x26F0000 */
#define FLASH_GAMES FLASH_SECTOR(FLASH_SEC_GAMES)               /* 0x2710000 */
#define FLASH_OUTINGS FLASH_SECTOR(FLASH_SEC_OUTINGS)           /* 0x2720000 */
#define FLASH_LETTERS FLASH_SECTOR(FLASH_SEC_LETTERS)           /* 0x2740000 */
#define FLASH_BINGO FLASH_SECTOR(FLASH_SEC_BINGO)               /* 0x2750000 */
#define FLASH_GASHAPON FLASH_SECTOR(FLASH_SEC_GASHAPON)         /* 0x2760000 */
#define FLASH_PHOTOS FLASH_SECTOR(FLASH_SEC_PHOTOS)             /* 0x2770000 */
#define FLASH_DL_CHARA FLASH_SECTOR(FLASH_SEC_DL_CHARA)         /* 0x2780000 */
#define FLASH_DIARY_DELETE_COPY FLASH_SECTOR(FLASH_SEC_DIARY_DELETE_COPY) /* 0x2790000 */
#define FLASH_COMM_RX2 FLASH_SECTOR(FLASH_SEC_COMM_RX2)         /* 0x27A0000 */
#define FLASH_COMM_TX FLASH_SECTOR(FLASH_SEC_COMM_TX)           /* 0x27C0000 */
#define FLASH_COMM_RX FLASH_SECTOR(FLASH_SEC_COMM_RX)           /* 0x27D0000 */
#define FLASH_GIFT FLASH_SECTOR(FLASH_SEC_GIFT)                 /* 0x27E0000 */
#define FLASH_COMM_TX2 FLASH_SECTOR(FLASH_SEC_COMM_TX2)         /* 0x27F0000 */

#endif
