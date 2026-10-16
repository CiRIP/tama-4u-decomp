/* Declarations of src/game/game_timing.c: its types, variables and functions. */
#ifndef PROTO_GAME_GAME_TIMING_H
#define PROTO_GAME_GAME_TIMING_H

#include "structs.h"

extern u8 game_timing_hits;

void game_timing(void);
int game_timing_run(void);
void game_timing_scene_intro(void);
void game_timing_scene_marker(u8);
void game_timing_scene_a(void);
void game_timing_scene_b(void);
void game_timing_scene_c(void);
int game_timing_round(u8);
void game_timing_result_anim(u8, int);
int game_timing_play(void);

#endif
