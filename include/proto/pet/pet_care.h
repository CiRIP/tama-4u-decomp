/* Declarations of src/pet/pet_care.c: its types, variables and functions. */
#ifndef PROTO_PET_PET_CARE_H
#define PROTO_PET_PET_CARE_H

#include "structs.h"

struct Sitter {
    u8 active;
    u8 due;
};

struct OverlayObjs {
    u8 obj0;
    u8 obj1;
    u8 obj2;
    u8 obj3;
};

extern struct Sitter pet_sitter_state;
extern u8 pet_poops_drawn;
extern u8 pet_praise_expired;
extern struct OverlayObjs room_overlay_objs;
extern u8 pet_care_poops_at_entry;
extern u8 pet_care_entry;

extern const u32 D_255CD34;
extern struct GfxObject *const room_overlay_base_a;
extern struct GfxObject *const room_overlay_base_e;
extern struct GfxObject *const room_overlay_base_b;
extern struct GfxObject *const room_overlay_base_c;
extern const u8 *const pet_sitter_busy_msg[];
extern const u8 *const pet_sitter_takeover_msg[];
extern const u8 *const pet_sitter_hours_msg[];
extern const u8 *const pet_sitter_question_msg[];
extern const u8 *const pet_care_menu_texts[];

int pet_habit_learned(void);
int pet_care_menu(void);
void pet_care_praise(void);
void pet_care_scold(void);
void pet_care_talk(void);
void pet_care_comfort(void);
void pet_sitter_call(void);
u8 pet_sitter_question(void);
int pet_sitter_return(void);
u8 pet_sitter_takeover_question(void);
u8 pet_sitter_hours(void);
int pet_sitter_due(void);
void pet_care_flags_reset(void);
void room_sitter_overlay(u8);
void room_toilet_request_overlay(u8);
void room_bath_request_overlay(u8);
void room_cleaning_request_overlay(u8);
void room_praise_overlay(u8);

#endif
