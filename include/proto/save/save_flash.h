/* Declarations of src/save/save_flash.c: its types, variables and functions. */
#ifndef PROTO_SAVE_SAVE_FLASH_H
#define PROTO_SAVE_SAVE_FLASH_H

#include "structs.h"
#include "proto/comm/comm_menu.h"
#include "proto/pet/pet_core.h"

/*
 * The saved game: save_game copies it to the flash sector at 0x200E000 as raw bytes and
 * sys_power_on copies it back ("つづきから"), so its layout is the original firmware's and
 * old saves load. save_game writes .pet with the 4 bytes after it first (0x248 bytes), then
 * the whole block up to and including .pet's first 0x228 bytes (0x528 bytes): the pet
 * record is in the sector twice and the loader takes the second copy.
 */
struct SaveBlock {
    /* 31 friends; the legacy IR stamp card code (dead) also uses [31] and [32], which lie
       over the bytes that follow, as in the original */
    struct Friend friend_list[31];
    u8 mail_friend_msg_count;
    u8 mail_unread_pending;
    u8 album_order[80];
    u8 pad2fc[4];
    struct PetState pet;
    u8 comm_book_notice_mode;
    u8 pad545[3];
};

extern struct SaveBlock save;

struct AnimPicSlots {
    u32 pic0;
    u32 pic1;
    u32 pic2;
    u32 pic3;
    u32 pic4;
    u32 pic5;
    u32 pic6;
    u32 pic7;
    u32 pic8;
    u32 pic9;
    u32 pic10;
};

extern const u8 D_255BF88[];
extern const u32 D_255C004;
extern const u32 D_255C028;
extern const u32 D_255C0B0;
extern const u32 D_255C0D4;
extern const u8 D_255BFC8[];
extern const u8 D_255BFD4[];
extern const u8 D_255BFE4[];
extern const u8 D_255BFF4[];
extern const struct Inventory item_inv_food;

u8 *flash_sector_addr(int);
u8 *item_bank_slot(int, int);
struct TamagoFile *item_bank_record(int, int);
u8 item_bank_has(int, int, int);
void save_factory_reset(void);
u8 save_confirm_erase(void);
void ui_show_startup_picture(void);
u8 test_check_entry_code(void);
u8 sys_power_on(void);
void save_game(void);
void flash_backup_sector(int);
void flash_restore_sector_except(int, void *, int);
u8 item_same_identity(int, struct TamagoFile *);
int item_find_in_bank(int, u8 *, int, int);
int item_find_in_inventory(const struct Inventory *, int);
int flash_first_erased_record(u8 *, int, int);
int item_bank_free_slot(int, int, int, int, int);
void flash_program_record(u16 *, u32, int, int);
void ui_show_items_full(void);
int item_bank_next_slot(int, int, int);
int item_bank_discard_browser(int, int, int, int, int, int);
int item_special_class(struct TamagoFile *, int);
int item_seasonal_decoration(int);
void spot_new_bingo_card(int);
u8 item_bank_is_app_data(int);
void dl_erase_program_area(int);
u8 item_store_in_bank(struct TamagoFile *, int, u32 *, int);
void ui_light_mark_for_bank(int);
u8 item_receive(struct TamagoFile *);
int item_discard_from_inventory(const struct Inventory *, int, int);
u8 item_add_to_inventory(struct TamagoFile *);
int dl_install_program(struct TamagoFile *);
struct TamagoFile *item_record_by_index(int);
int item_load_pictures(u32 *c, u8 *q, int d);
int item_prize_record(int, struct TamagoFile *);
int item_body_value(struct AnimPicSlots *a, int k, struct TamagoFile *p);
int spot_app_record(int, struct TamagoFile *);
int item_data_block(int out, int k, struct TamagoFile *p);
u8 spot_gashapon_rarity_pick(struct TamagoFile *);
int item_get_id(struct TamagoFile *);
void friend_portrait_backup(int);
void friend_portrait_store(int, u8 *);
void flash_erase_sector(int);
int flash_sector_to_scratch(int, int);
int flash_sector_from_scratch(int, int);
int flash_program(u8 *, const u8 *, int);
void item_load_stamp_pictures(void);
u8 item_store_received_stamp(void);
void func_250D248(void);

#endif
