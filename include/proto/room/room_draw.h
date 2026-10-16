/* Declarations of src/room/room_draw.c: its types, variables and functions. */
#ifndef PROTO_ROOM_ROOM_DRAW_H
#define PROTO_ROOM_ROOM_DRAW_H

#include "structs.h"

extern u8 room_poop_objects[4];
extern u8 room_poop_blink;
extern u32 room_poop_count;

extern const u8 room_poop_positions[][2];
extern const u8 D_253C840[];

int chara_birthday_on(u8, u8);
void pet_event_chara_birthday(void);
u8 room_draw_decorations(u8);
u8 room_draw_background(void);
void room_draw_poops(u8);
u8 room_blink_poops(void);

#endif
