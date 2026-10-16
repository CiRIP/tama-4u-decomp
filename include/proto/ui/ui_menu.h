/* Declarations of src/ui/ui_menu.c: its types, variables and functions. */
#ifndef PROTO_UI_UI_MENU_H
#define PROTO_UI_UI_MENU_H

#include "structs.h"

extern volatile u8 ui_timeout;
extern u8 ui_cursor_obj;
extern u8 ui_label_obj;

extern const u8 D_253EFB4[];
extern const u8 D_253EFC8[];
extern const u8 D_253EFE0[];
extern const u8 D_253EFE8[];
extern const u8 D_253EFF0[];

void ui_menu_draw(struct MenuTable *, int *);
void ui_menu_draw_framed(struct MenuTable *, int *);
int ui_menu_run(struct MenuTable *, int *, u8, u8);
u32 ui_menu_run_plain(const struct MenuTable *, int *);
void ui_menu_run_framed(struct MenuTable *, int *, u8);
int ui_message_box(const u8 *, const u8 *, const u8 *, const u8 *, int);
int ui_question_two_lines(const void *, const void *, const void *, const void *, int, int *);
u32 ui_yes_no(const u8 *, const u8 *, const u8 *, int, int *);
int ui_use_no_delete(int, int, u32 *);
int ui_question_two_lines_b(int, int, int, int, int, u32 *);
int ui_three_entry_menu(int a, int b, u16 c, int d, u16 e, int g, u16 h, int y, int *f);
int ui_paged_list(const u8 *, int, u8 *, int, u32 *);

#endif
