/* Declarations of src/game/game_builtin.c: its types, variables and functions. */
#ifndef PROTO_GAME_GAME_BUILTIN_H
#define PROTO_GAME_GAME_BUILTIN_H

#include "structs.h"

struct SlotRound {
    u32 pics;
    u32 count;
};

extern u8 game_slot_score;
extern u8 game_slot_frame_objs[3];
extern u8 game_slot_round_obj;
extern u8 game_slot_reel_objs[3];
extern u8 game_slot_level_obj;
extern u8 game_slot_reel_pos[3];
extern u8 game_slot_reels_stopped;
extern u8 game_slot_reel_state[3];
extern u8 game_slot_reel_speed;
extern u8 game_slot_round;
extern u8 game_cake_level;
extern u8 game_cake_choices[];
extern u8 game_treasure_finds;
extern u8 game_treasure_text_obj;
extern u8 game_quiz_obj;
extern u8 game_quiz_objs[4];
extern u8 game_quiz_answer_order[];
extern u8 game_quiz_questions[];
extern u8 game_quiz_correct;
extern u8 game_quiz_current;
extern u32 game_quiz_choice;
extern struct SlotRound game_slot_round_tbl[3];

extern const u16 game_cake_order_masks[];
extern const u8 D_255FC00[];
extern const u8 D_255FC10[];

void game_slot_init_reels(void);
int game_slot_screen(void);
void game_slot_light_match(int, int);
void game_slot_score_pair(int, int);
int game_slot_loop(void);
int game_cake(void);
int game_cake_pick_order(void);
int game_cake_loop(void);
void game_cake_intro(void);
void game_cake_scene0(void);
void game_cake_scene1(void);
void game_cake_scene2(void);
void game_cake_scene3(void);
void game_cake_scene4(void);
void game_cake_scene5(void);
void game_cake_scene6(void);
void game_cake_scene7(void);
void game_cake_scene8(void);
void game_cake_scene9(void);
void game_cake_success(void);
void game_cake_fail(void);
void game_treasure(void);
int game_treasure_loop(void);
void game_treasure_scene_a(void);
void game_treasure_scene_b(void);
void game_treasure_scene_c(void);
void game_treasure_scene_d(void);
void game_treasure_scene_e(void);
void game_treasure_scene_f(void);
void game_treasure_scene_g(void);
void game_treasure_play(void);
void game_treasure_scene_h(void);
void game_treasure_found_scene(void);
void game_treasure_bust_scene(void);
void game_quiz_setup(int);
u8 game_quiz_question_screen(void);
int game_quiz(void);
u8 game_quiz_ask(int);

#endif
