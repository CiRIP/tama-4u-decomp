/* Declarations of src/item/item_use.c: its types, variables and functions. */
#ifndef PROTO_ITEM_ITEM_USE_H
#define PROTO_ITEM_ITEM_USE_H

#include "structs.h"

/* pictures by index visit_reward_pic_idx (ir_exchange) */
struct PicPair {
    const void *pic_a;
    const void *pic_b;
};

struct IndexRange {
    u8 first;
    u8 last;
};

extern u8 item_no_full_dialog;
extern const u8 *anim_pic_slots[][3];
extern u32 D_0084194[];
extern u32 D_0084198[];
extern u32 D_008419C[];
extern u32 D_00841A8;
extern u32 D_00841B8;
extern u32 D_00841BC;
extern u32 D_00841C0;

extern const u8 D_253C014[];
extern void (*const item_use_anims[])(void);
extern void (*const item_liked_anims[])(void);
enum { INV_DAILY, INV_TOOLS, INV_CLOTHES, INV_ACCESSORIES, INV_FRIDGE, INV_STAMPS, INV_APP_ITEMS };
extern const struct Inventory item_inventories[7];
#define item_inv_daily       (item_inventories[INV_DAILY])
#define item_inv_tools       (item_inventories[INV_TOOLS])
#define item_inv_clothes     (item_inventories[INV_CLOTHES])
#define item_inv_accessories (item_inventories[INV_ACCESSORIES])
#define item_inv_fridge      (item_inventories[INV_FRIDGE])
#define item_inv_stamps      (item_inventories[INV_STAMPS])
#define item_inv_app_items   (item_inventories[INV_APP_ITEMS])
extern const struct PicPair visit_reward_pics[];
extern const struct IndexRange item_builtin_class_ranges[];
extern const u8 D_253C3C0[];
extern const u8 D_253C3CC[];
extern const u8 D_253C3D4[];
extern const u8 D_253C3DC[];
extern const u8 D_253C3B8[];
extern const u8 D_253C3AC[];

void item_apply_attributes(struct TamagoFile *);
int item_chara_preference(struct TamagoFile *, int);
int item_partner_preference(struct TamagoFile *);
int item_pet_preference_raw(struct TamagoFile *);
u8 item_pet_preference(struct TamagoFile *);
u8 item_pet_preference_by_index(int);
s8 item_happiness_effect(struct TamagoFile *);
s8 item_happiness_effect_by_index(int);
int item_hunger_effect(struct TamagoFile *);
s8 item_hunger_effect_by_index(int);
u8 item_use_anim_id(struct TamagoFile *);
u8 item_use_anim_id_by_index(int);
int item_inv_find_owned_id(const struct Inventory *, u16);
void item_apply_effects(struct TamagoFile *);
void item_apply_effects_by_index(int);
u8 item_is_unusable(int);
int item_id_is_coupon(u16);
int item_use(int);
int item_use_record(struct TamagoFile *);
void item_draw_name(struct TamagoFile *, u8, s16, u8);
void item_draw_name_by_index(u8, u8, s16, u8);
void item_draw_card(struct TamagoFile *, int);
void item_draw_card_by_index(int, int);
u8 item_use_dialog(int);
int item_special_count(void);
int item_special_next(int);
int item_special_show(struct TamagoFile *);
int item_special_browser(void);
int item_inv_usable_count(const struct Inventory *);
int item_inv_owned_count(const struct Inventory *);
int item_inv_next_usable(const struct Inventory *, int);
int item_inv_picker_b(const struct Inventory *, u8, u32 *);
int item_inv_picker(const struct Inventory *, u8, u32 *);
struct TamagoFile *item_builtin_find(int, int);

#endif
