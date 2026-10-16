/* anim_script_api.h: the declarations src/anim/anim_scripts.c (the generated animation scripts) sees,
 * instead of protos.h: the animation API of the engine, as the scripts call it. They match
 * the definitions (protos.h) except anim_scene_living, which two scripts call with an extra
 * argument (anim_receive_item, anim_present_box): declared without a prototype. */
#ifndef ANIM_SCRIPT_API_H
#define ANIM_SCRIPT_API_H

#include "structs.h"

/* The data of src/anim/anim_scripts.c the scripts use (its header proto/anim/anim_scripts.h
 * declares the scripts as their callers see them, which conflicts with the definitions).
 * The RAM variables in their address order: gcc emits .bss in declaration order. */
extern u32 anim_scene_bg;
extern u32 anim_scene_fg;
extern u32 anim_scene_extra;
extern const u8 D_2535DDC[];
extern const u8 D_2535DEC[];
extern const u8 D_2535E3C[];
extern const u8 D_2535E5C[];
extern const u8 D_2535E7C[];
extern const u8 D_2535E9C[];
extern const u8 D_2535EBC[];
extern const u8 D_2535EC0[];
extern const u8 D_2535ECC[];
extern const u8 D_2535ED8[];

void anim_scene_room(u8);
void anim_scene_window(u8);
void anim_scene_window2(u8);
void anim_scene_living();
void anim_scene_living_poops(u8);
void anim_scene_kitchen(u8);
void anim_scene_game_corner(u8);
void anim_scene_bath(u8);
void anim_scene_toilet(u8);
void anim_draw_pet(u8, u8, u16, s16, s16);
void anim_draw_other_chara(u8, u8, u16, s16, s16);
void anim_draw_third_chara(u8, u8, u16, s16, s16);
void anim_draw_fourth_chara(u8, u8, u16, s16, s16);
void anim_draw_pic(u8, const u8 *, u16, s16, s16);
void anim_draw_digit(u8, u8, u16, s16, s16);
int anim_draw_text(u8, const u8 *, u8, s16, s16);
void anim_queue_sound(int);
u8 anim_frame(int, int);
void anim_scene_closet(u8);

#endif
