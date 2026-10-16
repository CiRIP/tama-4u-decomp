/* Declarations of src/mail/mail_edit.c: its types, variables and functions. */
#ifndef PROTO_MAIL_MAIL_EDIT_H
#define PROTO_MAIL_MAIL_EDIT_H

#include "structs.h"

extern u8 mail_card_line_objs[5];

extern const u8 D_255D5CC[];
extern const u8 D_255D5E0[];
extern const u8 D_255D5E8[];
extern const u8 D_255D5F0[];
extern const u8 D_255D604[];
extern const u8 D_255D60C[];
extern const u8 D_255D5C4[];
extern const u8 D_255D5C8[];

u8 mail_count_for_friend(int);
int mail_card(u8, int, int, int, int);
int mail_edit_message(int, int);
void mail_draw_card_line(u8, int, int);
void mail_draw_card(int, int, u8, int, int);
void mail_load_message(int, int);
int mail_edit_dialog(u8);
int friend_prev_used(int);
int friend_picture_record(int);
void friend_draw_page(u8);

#endif
