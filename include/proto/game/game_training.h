/* Declarations of src/game/game_training.c: its types, variables and functions. */
#ifndef PROTO_GAME_GAME_TRAINING_H
#define PROTO_GAME_GAME_TRAINING_H

#include "structs.h"

extern u8 game_training_level;
extern u8 game_training_choices[];

extern const u32 D_255FAF8;
extern const u16 game_training_masks[];

int game_training(void);
int game_training_pick(void);
int game_training_loop(void);
void game_training_intro(void);
void game_training_scene_a(void);
void game_training_scene_b(void);
void game_training_result_a(void);
void game_training_result_b(void);
void game_training_scene_c(void);
void game_training_scene_d(void);

#endif
