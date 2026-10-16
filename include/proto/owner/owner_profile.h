/* Declarations of src/owner/owner_profile.c: its types, variables and functions. */
#ifndef PROTO_OWNER_OWNER_PROFILE_H
#define PROTO_OWNER_OWNER_PROFILE_H

#include "structs.h"

struct ProfilePageObjs {
    u8 background;
    u8 arrow;
    u8 mood_icon;
    u8 name;
    u8 gender;
    u8 mood;
    u8 age_tens;
    u8 age_ones;
    u8 age_unit;
    u8 sign;
    u8 text1;
};

extern u32 owner_work_copy;
extern struct ProfilePageObjs owner_page_objs;
extern const u8 *owner_gender_labels[];
extern const u8 *owner_mood_label_tbl[];
extern const u8 *owner_star_sign_labels[];

extern const u8 *const owner_mood_icon_tbl[];
extern const u8 D_255D1D0[];
extern const u8 D_255D228[];
extern const u8 D_255D0A8[];

int owner_editor(void);
void owner_empty_pages(int);
void owner_draw_page(u8, u8, u8, u8);
int owner_edit_page0(void);
int owner_edit_page1(void);
int owner_edit_page2(void);
int owner_mood_icon_value(u8);
const u8 *owner_digit_pic(u8 idx);
int friend_profile_view(int);
void friend_profile_draw_page(u8, int, const u8 *, const u8 *);

#endif
