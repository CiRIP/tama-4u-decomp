/* Declarations of src/food/food_meals.c: its types, variables and functions. */
#ifndef PROTO_FOOD_FOOD_MEALS_H
#define PROTO_FOOD_FOOD_MEALS_H

#include "structs.h"

struct PartyState {
    u8 kind;
    u8 table_charas;
    u8 variant;
};

struct PartyInfo {
    u32 party_kind;
    u32 kind;
    u8 month;
    u8 day;
};

struct FoodMenuEntry {
    u32 label;
    u32 venues;
};

struct LabelPair {
    u32 first;
    u32 second;
};

extern struct PartyState food_party_state;
extern struct PartyInfo food_birthday_info;

extern const struct FoodMenuEntry food_menu_tbl[];
extern const struct LabelPair food_meal_snack_labels;
extern const u8 D_253A488[];
extern const u8 D_253A498[];

void food_share_pics(u8);
void room_set_kitchen_design(int);
int food_owner_birthday_today(void);
int food_chara_birthday_today(void);
int food_meal_kitchen_style_b(void);
u8 food_meal_kitchen_style(void);
int food_party_table_pic(void);
int food_party_table_pic_chara(void);
void food_order_guest_list(u16 *, int);
void food_load_party_guests(int);
void food_load_candidate_guests(void);
u8 food_party_scene(const u8 *);
u8 food_party_scene_b(const u8 *);
int food_party_dish_screen(int);
int food_eat_meal(int);
int food_eat_snack(int);
int func_240E606(void);
int func_240E60A(void);
u8 food_eat_restaurant_meal(int, int, const u8 *);
int func_240E744(void);
u8 food_eat_restaurant_snack(int, int, const u8 *);
int food_pick_friend_cook(void);
int food_restaurant_visit(int, u8);
u8 food_menu_entry_available(int);
u8 food_menu(void);
s8 food_meal_or_snack_choice(u8);

#endif
