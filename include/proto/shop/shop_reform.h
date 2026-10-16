/* Declarations of src/shop/shop_reform.c: its types, variables and functions. */
#ifndef PROTO_SHOP_SHOP_REFORM_H
#define PROTO_SHOP_SHOP_REFORM_H

#include "structs.h"

extern u8 ui_dialog_cursor_obj;

extern const struct BankListEntry room_tbl[];
extern const u32 room_living_designs[];
extern const u32 room_kitchen_designs[];
extern const u32 room_toilet_designs[];
extern const u32 room_bath_designs[];
extern const u32 room_bedroom_designs[];
extern const u32 room_kitchen_bg_pics[];
extern const u32 room_kitchen_fg_pics[];
extern const u32 room_bath_extra_pics[];
extern const u32 room_bed_pics[];
extern const u32 D_255BF24;
extern const u32 D_255BF54;
extern const u8 D_255BF0C[];
extern const u8 D_255BEEC[];
extern const u8 D_255BEF4[];
extern const u8 D_255BEFC[];
extern const u8 D_255BF04[];

void ui_question_do_screen(int, u8);
void ui_question_yes_screen(int, u8);
void ui_set_dialog_cursor(int);
u8 ui_two_choice_run(u8);
u8 ui_confirm_yes_no(const u8 *, u8);
void ui_question_two_line_screen(int, int, u8);
int ui_yes_no_two_lines(int, int, u8);
u8 pet_can_pay(u16);
int shop_reform_pay(u16);
int room_next_design(int, int);
int shop_reform_browser(int);
int shop_reform_living(void);
struct TamagoFile *room_living_design_at(int);
int room_living_design_count(void);
int shop_reform_browser_living(u8);

#endif
