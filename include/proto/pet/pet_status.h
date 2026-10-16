/* Declarations of src/pet/pet_status.c: its types, variables and functions. */
#ifndef PROTO_PET_PET_STATUS_H
#define PROTO_PET_PET_STATUS_H

#include "structs.h"

extern const u8 D_255D07C[];
extern const u8 D_255D090[];
extern const u8 *const ui_status_happiness_pics[];

int ui_status_browser(void);
void pet_status_pages(u8);
void ui_status_page_hearts(u8);
void ui_status_page_profile(u8);
void ui_status_page_marks(u8);
void pet_status_page_clover(u8);

#endif
