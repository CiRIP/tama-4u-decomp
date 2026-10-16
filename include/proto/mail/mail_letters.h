/* Declarations of src/mail/mail_letters.c: its types, variables and functions. */
#ifndef PROTO_MAIL_MAIL_LETTERS_H
#define PROTO_MAIL_MAIL_LETTERS_H

#include "structs.h"

extern const u8 *const mail_letter_templates[];
extern const u8 D_255F5AC[];
extern const u8 D_255F5BC[];
extern const u8 D_255F5D4[];
extern const u8 D_255F5E4[];
extern const u8 D_255F5F8[];
extern const u8 D_255F5FC[];
extern const u8 D_255F610[];
extern const u8 D_255F624[];

void mail_farewell_letter(void);
void mail_show_letter_item(struct TamagoFile *);
void item_description_card(struct TamagoFile *);
void mail_letter_from_friend(int);
int mail_letter_read_flag(int);
void mail_set_letter_read(int, u8);
int mail_unread_letter_count(void);
int mail_letter_count(void);
int mail_next_letter(int);
int mail_letter_box(void);
int func_241B738(void);
int mail_coupon_box(void);
int mail_letter_box_screen(void);

#endif
