/* Declarations of src/pet/pet_care_room.c: its types, variables and functions. */
#ifndef PROTO_PET_PET_CARE_ROOM_H
#define PROTO_PET_PET_CARE_ROOM_H

#include "structs.h"

extern u8 pet_cleaning_expired;
extern u8 pet_poop_overflow;

extern const u8 *const pet_care_room_menu_labels[];

int pet_care_room_menu(void);
void pet_cleaning(void);
int ui_wardrobe_menu_cleared(void);

#endif
