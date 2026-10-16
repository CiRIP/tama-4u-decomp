/* Declarations of src/chara/chara_draw.c: its types, variables and functions. */
#ifndef PROTO_CHARA_CHARA_DRAW_H
#define PROTO_CHARA_CHARA_DRAW_H

#include "structs.h"

/* the wardrobe menu entries (ui_wardrobe_menu): label, handler, inventory index */
struct WardrobeEntry {
    const u8 *label;
    int (*x004)(void);
    u32 inventory;
};

extern u32 chara_accessory_pics[7];
extern u32 chara_clothes_pics[];
extern u32 chara_accessory_offsets;

extern const u8 D_2535F50[];
extern const u8 D_2535F68[];
extern const u8 *const chara_own_accessory_offsets[];
extern const u32 D_2536D6C;
extern const u32 D_2536D84;
extern const struct WardrobeEntry ui_wardrobe_menu_tbl[];
extern const u8 D_2536D5C[];

void pet_wear_accessory(int);
void pet_wear_clothes(int);
int chara_special_accessory_stub(void);
void chara_draw_accessory(u8, u8, u8, u16, s16, s16, u8, u16);
void chara_draw_clothes(u8 a, u8 b, u8 c, u16 d, s16 e, s16 f, u8 g, u16 h);
void chara_draw_pet(u8, u8, u8, u16, s16, s16);
void chara_draw_second(u8, u8, u8, u16, s16, s16);
void chara_draw_third(u8, u8, u8, u16, s16, s16);
void chara_draw_fourth(u8, u8, u8, u16, s16, s16);
void chara_draw_any(u8, u8, u8, u16, s16, s16, int);
int item_next_owned_accessory(int i);
int item_next_owned_clothes(int);
int ui_wardrobe_accessories(void);
int ui_wardrobe_clothes(void);
u8 item_first_owned_accessory(void);
u8 item_next_owned_accessory_wrap(int);
int item_owned_accessory_count(void);
u8 item_first_owned_clothes(void);
u8 item_next_owned_clothes_wrap(int);
int item_owned_clothes_count(void);
int ui_wardrobe_menu(void);

#endif
