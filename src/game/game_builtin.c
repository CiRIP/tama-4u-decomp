/*
 * The built-in games written against the API like downloaded games: cake, treasure hunt and
 * quiz, with their scenes.
 *
 * Code 251F560..25276F8, .rodata from 255FB44.
 */
#include "proto/game/game_builtin.h"
#include "proto/iram.h"
#include "proto/api/api_entries.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"

/* tables defined further down */
extern const u8 D_255FB80[10][2];
extern const u8 *const *const game_quiz_answer_tbl[3];
extern const u8 *const game_quiz_question_texts[100];
extern void (*const D_255FBD0[3][3])(void);
extern const u8 D_255FBF4[3][3];

/*
 * Slot game: the start positions of the three reels for round game_slot_round (table
 * game_slot_round_tbl: picture count and pictures per round), three different pictures,
 * in steps of 8 (a picture is position >> 2).
 */
void game_slot_init_reels(void)
{
    u8 *d;
    struct SlotRound *t;
    int m;
    int i;

    d = game_slot_reel_pos;
    t = game_slot_round_tbl;
    d[0] = util_rand_except((int)t[game_slot_round].count / 2, (int)t[game_slot_round].count / 2);
    d[1] = util_rand_except((int)t[game_slot_round].count / 2, d[0]);
    d[2] = 3 - d[0] - d[1];
    if (game_slot_round != 0) {
        m = util_rand_except((int)t[game_slot_round].count / 2, (int)t[game_slot_round].count / 2);
        for (i = 0; i <= 2; i++)
            if (game_slot_reel_pos[i] >= m)
                game_slot_reel_pos[i]++;
    }
    for (i = 0; i <= 2; i++)
        game_slot_reel_pos[i] <<= 3;
}

/*
 * Slot game screen (background 0x21C335C, three reel windows): score 0, reel speed
 * game_slot_reel_speed = 4 (added to each spinning reel per tick), round 0; then the game
 * loop. Returns the score.
 */
int game_slot_screen(void)
{
    int i;
    int r;

    anim_busy = 1;
    game_slot_score = 0;
    game_slot_reel_speed = 4;
    game_slot_round = 0;
    game_slot_init_reels();
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_21C335C);
    gfx_object_create(8, 0x30, 0x18, 0, 0, D_21C457C);
    for (i = 0; i < 3; i++) {
        game_slot_frame_objs[i] = gfx_object_create(i * 40 + 8, 0x30, 0x20, 0, 0, 0);
        game_slot_reel_objs[i] = gfx_object_create(i * 40 + 8, 0x30, i * 24 + 0x28, 0, 0,
                                  ((u32 *)game_slot_round_tbl[game_slot_round].pics)[game_slot_reel_pos[i] >> 2]);
    }
    gfx_object_create(4, 0x28, 0x70, 0, 0, D_21C1490);
    gfx_object_create(0x62, 0x1c, 0x80, 0, 0, D_21C4FB4);
    gfx_object_create(1, 0x4d, 0x90, 0, 0, D_21C5120);
    game_slot_round_obj = gfx_object_create(0x1a, 0x1b, 0xa0, 0, 0, 0);
    game_slot_level_obj = gfx_object_create(0x26, 0x52, 0xa8, 0, 0, 0);
    func_00009A6();
    snd_play(0x12);
    tmr_delay_ms(1000);
    r = game_slot_loop();
    anim_busy = 0;
    return r;
}

/*
 * Slot game: when reels a and b have both just stopped (state 0x12) on the same picture,
 * light their frames (0x21C59E0 for picture 6, else 0x21C4D6C); a reel that has just
 * stopped moves on to state 0x13 either way.
 */
void game_slot_light_match(int a, int b)
{
    if (game_slot_reel_state[a] <= 0x11 || game_slot_reel_state[b] <= 0x11)
        return;
    if (game_slot_reel_pos[a] == game_slot_reel_pos[b]) {
        if (game_slot_reel_state[a] == 0x12) {
            if (game_slot_reel_pos[a] == 0x18)
                gfx_object_set_image(game_slot_frame_objs[a], 0x24, 0, D_21C59E0);
            else
                gfx_object_set_image(game_slot_frame_objs[a], 0x20, 0, D_21C4D6C);
            game_slot_reel_state[a]++;
        }
        if (game_slot_reel_state[b] == 0x12) {
            if (game_slot_reel_pos[b] == 0x18)
                gfx_object_set_image(game_slot_frame_objs[b], 0x24, 0, D_21C59E0);
            else
                gfx_object_set_image(game_slot_frame_objs[b], 0x20, 0, D_21C4D6C);
            game_slot_reel_state[b]++;
        }
    } else {
        if (game_slot_reel_state[a] == 0x12)
            game_slot_reel_state[a]++;
        if (game_slot_reel_state[b] == 0x12)
            game_slot_reel_state[b]++;
    }
}

/*
 * Slot game: two stopped reels a and b on the same picture (the third differs): light their
 * frames (0x21C59E0 / 0x21C4D6C, sound 10 / 0x1E), add 30 / 20 / 10 points by the picture
 * group, and blink the frames.
 */
void game_slot_score_pair(int a, int b)
{
    int i;

    if (game_slot_reel_state[a] > 0x11 && game_slot_reel_pos[a] == game_slot_reel_pos[b] && game_slot_reel_state[b] > 0x11) {
        if (game_slot_reel_pos[a] == 0x18)
            gfx_object_set_image(game_slot_frame_objs[a], 0x24, 0, D_21C59E0);
        else
            gfx_object_set_image(game_slot_frame_objs[a], 0x20, 0, D_21C4D6C);
        if (game_slot_reel_pos[b] == 0x18) {
            snd_play(10);
            gfx_object_set_image(game_slot_frame_objs[b], 0x24, 0, D_21C59E0);
        } else {
            snd_play(0x1e);
            gfx_object_set_image(game_slot_frame_objs[b], 0x20, 0, D_21C4D6C);
        }
        switch (game_slot_reel_pos[a] >> 3) {
        case 0:
            game_slot_score += 0x1e;
            break;
        case 1:
            game_slot_score += 0x14;
            break;
        case 2:
            game_slot_score += 10;
            break;
        }
        for (i = 0; i < 3; i++) {
            gfx_object_move_dirty(game_slot_frame_objs[a], a * 40 + 8, 0x30);
            gfx_object_move_dirty(game_slot_frame_objs[b], b * 40 + 8, 0x30);
            func_00009A6();
            tmr_delay_ms(0xfa);
            gfx_object_move_dirty(game_slot_frame_objs[a], a * 40 + 8, 0x64);
            gfx_object_move_dirty(game_slot_frame_objs[b], b * 40 + 8, 0x64);
            func_00009A6();
            tmr_delay_ms(0xfa);
        }
    }
}

/*
 * Slot game loop, every 50 ms: the spinning reels advance by game_slot_reel_speed; A or B
 * (or 20 ticks of the 500 ms timer) stops the next reel, which creeps on by 2 then 1 until
 * it rests on a picture. When all three stand: three of a kind 60 / 40 / 20 points by the
 * picture group (with a blinking frame), a pair game_slot_score_pair, all different
 * nothing. Three rounds, then the score is returned (180 becomes 200).
 */
int game_slot_loop(void)
{
    int i;
    int k;
    int n;
    int lim;
    const u8 *img;
    u8 t;

again:
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    n = 0;
    for (i = 0; i < 3; i++) {
        gfx_object_set_image(game_slot_frame_objs[i], 0x20, 0, 0);
        gfx_object_move_dirty(game_slot_frame_objs[i], i * 40 + 8, 0x30);
        game_slot_reel_state[i] = 0;
    }
    game_slot_reels_stopped = 0;
    tmr_t5_start_timeout(500);
    for (;;) {
        for (i = 0; i < 3; i++) {
            if (game_slot_reel_state[i] == 0) {
                game_slot_reel_pos[i] += game_slot_reel_speed;
            } else {
                if (game_slot_reel_state[i] <= 0x11)
                    game_slot_reel_state[i]++;
                if (game_slot_reel_state[i] >= 2 && game_slot_reel_state[i] <= 9)
                    game_slot_reel_pos[i] += 2;
                else if (game_slot_reel_state[i] >= 10 && game_slot_reel_state[i] <= 17) {
                    if (game_slot_reel_pos[i] & 7)
                        game_slot_reel_pos[i] += 1;
                    else
                        game_slot_reel_state[i] = 0x12;
                }
            }
            if (game_slot_reel_pos[i] >= (int)game_slot_round_tbl[game_slot_round].count * 4)
                game_slot_reel_pos[i] = 0;
            gfx_object_set_image(game_slot_reel_objs[i], i * 24 + 0x28, 0,
                         (const u8 *)((u32 *)game_slot_round_tbl[game_slot_round].pics)[game_slot_reel_pos[i] >> 2]);
            func_00009A6();
        }
        game_slot_light_match(0, 1);
        game_slot_light_match(0, 2);
        game_slot_light_match(1, 2);
        if (game_slot_reel_state[0] > 0x11 && game_slot_reel_state[1] > 0x11 && game_slot_reel_state[2] > 0x11) {
            if (game_slot_reel_pos[0] == game_slot_reel_pos[1] && game_slot_reel_pos[0] == game_slot_reel_pos[2]) {
                if (game_slot_reel_pos[0] == 0x18) {
                    gfx_object_set_image(game_slot_frame_objs[0], 0x24, 0, (const u8 *)(k = 0x21c59e0));
                    gfx_object_set_image(game_slot_frame_objs[1], 0x24, 0, (const u8 *)k);
                    gfx_object_set_image(game_slot_frame_objs[2], 0x24, 0, (const u8 *)k);
                } else {
                    gfx_object_set_image(game_slot_frame_objs[0], 0x20, 0, (const u8 *)(k = 0x21c4d6c));
                    gfx_object_set_image(game_slot_frame_objs[1], 0x20, 0, (const u8 *)k);
                    gfx_object_set_image(game_slot_frame_objs[2], 0x20, 0, (const u8 *)k);
                }
                switch (game_slot_reel_pos[0] >> 3) {
                case 0:
                    game_slot_score += 0x3c;
                    break;
                case 1:
                    game_slot_score += 0x28;
                    break;
                case 2:
                    game_slot_score += 0x14;
                    break;
                }
                if (game_slot_reel_pos[0] == 0) {
                    snd_play(0x16);
                    lim = 5;
                } else {
                    snd_play(game_slot_reel_pos[0] == 0x18 ? 10 : 0x1e);
                    lim = 3;
                }
                for (k = 0; k < lim; k++) {
                    for (i = 0; i < 3; i++)
                        gfx_object_move_dirty(game_slot_frame_objs[i], i * 40 + 8, 0x30);
                    func_00009A6();
                    tmr_delay_ms(0xfa);
                    for (i = 0; i < 3; i++)
                        gfx_object_move_dirty(game_slot_frame_objs[i], i * 40 + 8, 0x64);
                    func_00009A6();
                    tmr_delay_ms(0xfa);
                }
                tmr_delay_ms(500);
            } else if (game_slot_reel_pos[0] != game_slot_reel_pos[1] && game_slot_reel_pos[1] != game_slot_reel_pos[2] && game_slot_reel_pos[0] != game_slot_reel_pos[2]) {
                snd_play(10);
                tmr_delay_ms(1000);
            } else {
                game_slot_score_pair(0, 1);
                game_slot_score_pair(0, 2);
                game_slot_score_pair(1, 2);
                tmr_delay_ms(500);
            }
            if (++game_slot_round > 2)
                break;
            switch (game_slot_round) {
            case 1:
                gfx_object_set_image(game_slot_level_obj, 0xa8, 0, D_21C30F8);
                break;
            case 2:
                gfx_object_set_image(game_slot_round_obj, 0xa0, 0, D_21C2E94);
                break;
            }
            snd_play(0x12);
            func_00009A6();
            game_slot_init_reels();
            tmr_delay_ms(1000);
            goto again;
        }
        if ((btn_a_presses != 0 || btn_b_presses != 0 || n > 0x13) && game_slot_reels_stopped <= 2) {
            if (btn_a_presses != 0 || btn_b_presses != 0) {
                snd_play(2);
                btn_a_presses = 0;
                btn_b_presses = 0;
                n = 0;
            }
            game_slot_reel_state[game_slot_reels_stopped] = 1;
            game_slot_reels_stopped++;
        }
        tmr_delay_ms(0x32);
        sys_idle_housekeeping();
        if (tmr_flags & 0x20) {
            tmr_flags &= ~0x20;
            n++;
        }
    }
    func_00009A6();
    tmr_delay_ms(1000);
    if (game_slot_score == 0xb4)
        game_slot_score = 0xc8;
    return game_slot_score;
}

/*
 * F65 = two mini games built on the F0-style animation primitives (api_anim_draw_image sprite,
 * api_anim_frame wait, anim_draw_pic/func_2403070 ...):
 *  1. "ケーキできるかな" (can you make a cake?), a button-sequence game (game_cake /
 *     game_cake_loop): ten cake orders, each an animation D_255FBA8[i] followed by a
 *     sequence of D_255FB80[2i] button presses whose expected button (bit k of
 *     D_255FB80[2i+1]: 0 = A, 1 = B) must be hit within 250 ticks each; a wrong button or
 *     a timeout ends the game, completing the sequence plays game_cake_success and raises the
 *     level game_cake_level (which widens the choice of orders through the masks game_cake_order_masks[i]).
 *     The ten animations game_cake_scene0 .. game_treasure_bust_scene are the scenes (images 0x21C5C28..
 *     = the 4U characters performing).
 *  2. The 100-question quiz (game_quiz): questions D_255FC20.. with three answers each
 *     (answer tables in asm data, game_quiz_answer_tbl), 100 Gotchi points for a correct answer.
 *     "どれをきる?" / "もう1ぽん!" (which one to cut? / one more!) belong to the scene
 *     animations.
 *
 * game_cake: entry of the built-in game "ケーキできるかな" (game menu entry 1,
 * game_menu_tbl): runs game_cake_loop from level 0 in mini-game mode (api_set_scene_busy(1), key
 * clicks suppressed); returns the result class used for the rewards.
 */
int game_cake(void)
{
    int r;

    api_set_scene_busy(1);
    game_cake_level = 0;
    r = game_cake_loop();
    api_set_scene_busy(0);
    return r;
}

/*
 * Pick a random cake order among the 10 allowed at the current level: those whose
 * game_cake_order_masks mask has bit game_cake_level set, shuffled with the API api_shuffle.
 */
int game_cake_pick_order(void)
{
    int i;
    int n = 0;

    for (i = 0; i <= 9; i++) {
        if ((game_cake_order_masks[i] >> game_cake_level) & 1)
            game_cake_choices[n++] = i;
    }
    api_shuffle(game_cake_choices, n);
    return game_cake_choices[0];
}

extern void (*const D_255FBA8[])(void);
/*
 * The cake game main loop (asm): clear the button latches (api_set_btn_a/B78/B82), choose an
 * order (game_cake_pick_order), intro (game_cake_intro), its animation, then for each expected press
 * poll A (api_get_btn_b) / B (api_get_btn_a) against the pattern bit, C (api_get_btn_c)
 * aborts, 250-tick timeout per press (api_timeout_start arms, api_timeout_elapsed / api_timeout_clear /
 * api_play_sound service it between halts); on success game_cake_success, level++ and the next
 * order; returns the game result.
 */
int game_cake_loop(void)
{
    int k;
    int i;
    int t;
    u8 x;

again:
    api_set_btn_a(0);
    ((void (*)(u8))api_set_btn_b)(0);
    api_set_btn_c(0);
    k = game_cake_pick_order();
    game_cake_intro();
    api_set_btn_a(0);
    api_set_btn_b(0);
    api_set_btn_c(0);
    D_255FBA8[k]();
    i = 0;
    t = D_255FB80[k][0];
    api_timeout_start(250);
    t <<= 3;
    for (;;) {
        if ((D_255FB80[k][1] >> i) & 1) {
            if (api_get_btn_b()) {
                api_set_btn_b(0);
                i++;
                if (D_255FB80[k][0] == i)
                    goto ok;
            }
            x = api_get_btn_a();
        } else {
            if (api_get_btn_a()) {
                api_set_btn_a(0);
                i++;
                if (D_255FB80[k][0] == i)
                    goto ok;
            }
            x = api_get_btn_b();
        }
        if (x)
            goto fail;
        if (api_get_btn_c())
            goto cancel;
        asm volatile("halt");
        if (api_timeout_elapsed()) {
            api_timeout_clear();
            if (t <= 0)
                goto fail;
            if (t <= 3)
                api_play_sound(3);
            t--;
        }
    }
ok:
    api_set_btn_a(0);
    api_set_btn_b(0);
    api_set_btn_c(0);
    game_cake_success();
    if (api_get_btn_c())
        goto cancel2;
    if (++game_cake_level <= 5) {
        api_delay_ms(1000);
        goto again;
    }
    goto rank;
fail:
    api_play_sound(0x11);
    game_cake_fail();
rank:
    api_delay_ms(1000);
    if (game_cake_level > 5)
        return 5;
    if (game_cake_level > 4)
        return 4;
    if (game_cake_level > 3)
        return 3;
    if (game_cake_level > 2)
        return 2;
    if (game_cake_level != 0)
        return 1;
    goto zero;
cancel:
    api_set_btn_c(0);
    goto quit;
cancel2:
    api_set_btn_c(0);
quit:
    api_play_sound(3);
    return -1;
zero:
    return 0;
}

/*
 * Cake game intro animation (API calls): the character bounces in (two 500 ms frames,
 * sound 0x35).
 */
void game_cake_intro(void)
{
    api_scene_room(5);
    api_anim_draw_image(0, D_21C5C28, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21C7BAC, 0, 0x79, 0x25);
    api_anim_draw_image(2, D_21C8904, 0x10, 7, 0x25);
    api_anim_draw_image(3, D_21C7504, 0, 0x5a, 0x34);
    api_anim_draw_image(4, D_21C6E50, 0x10, 0x26, 0x34);
    api_anim_queue_sound(0x35);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C5C28, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21C7BAC, 0x10, 0x79, 0x25);
    api_anim_draw_image(2, D_21C8904, 0, 7, 0x25);
    api_anim_draw_image(3, D_21C7504, 0, 0x5a, 0x35);
    api_anim_draw_image(4, D_21C6E50, 0x10, 0x26, 0x35);
    api_anim_frame(500, 0);
}

/*
 * Cake game scene 0 (D_255FBA8[0]): three 500 ms frames with sound 0x15 / 0xB; each
 * anim_frame(500, 7) returns non-zero when a button interrupts the wait. The following
 * game_cake_scene1 .. game_treasure_bust_scene are scenes 1..9 and the shared sub-animations.
 */
void game_cake_scene0(void)
{
    anim_scene_room(0xa);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(0x1f4, 7) != 0)
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(0x1f4, 7) != 0)
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x33);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x33);
    anim_draw_pic(9, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(0x1f4, 7);
}

/*
 * Cake game scene / round animation (one of several similar generated scenes).
 */
void game_cake_scene1(void)
{
    anim_scene_room(0xb);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x2e, 0x26);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x2e, 0x26);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x19);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x33);
    anim_draw_pic(8, D_21C8340, 0, 0x2e, 0x23);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x16);
    anim_draw_pic(10, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(500, 7);
end:;
}

/*
 * Cake game scene (one of the generated keyframe scenes of the game).
 */
void game_cake_scene2(void)
{
    anim_scene_room(0xb);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x4e, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(9, D_21C8340, 0, 0x2e, 0x26);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x33);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x33);
    anim_draw_pic(9, D_21C8340, 0, 0x2e, 0x23);
    anim_draw_pic(10, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(500, 7);
end:;
}

/*
 * Cake game scene.
 */
void game_cake_scene3(void)
{
    anim_scene_room(0xb);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x33);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x25);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x33);
    anim_draw_pic(10, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(500, 7);
end:;
}

/*
 * Cake game scene.
 */
void game_cake_scene4(void)
{
    anim_scene_room(0xb);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_draw_pic(9, D_21C81FC, 0, 0x4d, 0x19);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x33);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x25);
    anim_draw_pic(9, D_21C81FC, 0, 0x4d, 0x16);
    anim_draw_pic(10, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(500, 7);
end:;
}

/*
 * Cake game scene.
 */
void game_cake_scene5(void)
{
    anim_scene_room(0xc);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7))
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x2e, 0x26);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7))
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x2e, 0x26);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x19);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7))
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(7, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(9, D_21C8340, 0, 0x2e, 0x26);
    anim_draw_pic(10, D_21C8340, 0, 0x33, 0x19);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7))
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x33);
    anim_draw_pic(8, D_21C81FC, 0, 0x4e, 0x33);
    anim_draw_pic(9, D_21C8340, 0, 0x2e, 0x23);
    anim_draw_pic(10, D_21C8340, 0, 0x33, 0x16);
    anim_draw_pic(11, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(500, 7);
}

/*
 * Cake game scene.
 */
void game_cake_scene6(void)
{
    anim_scene_room(0xc);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_draw_pic(9, D_21C81FC, 0, 0x4d, 0x19);
    anim_draw_pic(10, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x33);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x25);
    anim_draw_pic(9, D_21C81FC, 0, 0x4d, 0x16);
    anim_draw_pic(10, D_21C8340, 0, 0x33, 0x33);
    anim_draw_pic(11, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(500, 7);
end:;
}

/*
 * Cake game scene.
 */
void game_cake_scene7(void)
{
    anim_scene_room(0xd);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7))
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x4e, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7))
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(9, D_21C8340, 0, 0x2e, 0x26);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7))
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(9, D_21C8340, 0, 0x2e, 0x26);
    anim_draw_pic(10, D_21C8340, 0, 0x33, 0x19);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7))
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x2e, 0x26);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x19);
    anim_draw_pic(10, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(11, D_21C81FC, 0, 0x53, 0x28);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7))
        return;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x33);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x25);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x33);
    anim_draw_pic(10, D_21C8340, 0, 0x2e, 0x23);
    anim_draw_pic(11, D_21C8340, 0, 0x33, 0x16);
    anim_draw_pic(12, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(500, 7);
}

/*
 * Cake game scene.
 */
void game_cake_scene8(void)
{
    anim_scene_room(0xd);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x4e, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(9, D_21C81FC, 0, 0x53, 0x28);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(10, D_21C8340, 0, 0x2e, 0x26);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_draw_pic(9, D_21C81FC, 0, 0x4d, 0x19);
    anim_draw_pic(10, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(11, D_21C8340, 0, 0x2e, 0x26);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x33);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x25);
    anim_draw_pic(9, D_21C81FC, 0, 0x4d, 0x16);
    anim_draw_pic(10, D_21C8340, 0, 0x33, 0x33);
    anim_draw_pic(11, D_21C8340, 0, 0x2e, 0x23);
    anim_draw_pic(12, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(500, 7);
end:;
}

/*
 * Cake game scene.
 */
void game_cake_scene9(void)
{
    anim_scene_room(0xd);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x36);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x23);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x24);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_draw_pic(9, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(10, D_21C8340, 0, 0x2e, 0x26);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x23);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x24);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C7504, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C6E50, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x36);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x28);
    anim_draw_pic(9, D_21C81FC, 0, 0x4d, 0x19);
    anim_draw_pic(10, D_21C8340, 0, 0x33, 0x36);
    anim_draw_pic(11, D_21C8340, 0, 0x2e, 0x26);
    anim_queue_sound(0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C8340, 0, 0xb, 0x24);
    anim_draw_pic(2, D_21C81FC, 0, 0x72, 0x24);
    anim_draw_pic(3, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(4, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(5, D_21C773C, 0, 0x5f, 0x34);
    anim_draw_pic(6, D_21C708C, 0x10, 0x21, 0x34);
    anim_draw_pic(7, D_21C81FC, 0, 0x4e, 0x33);
    anim_draw_pic(8, D_21C81FC, 0, 0x53, 0x25);
    anim_draw_pic(9, D_21C81FC, 0, 0x4d, 0x16);
    anim_draw_pic(10, D_21C8340, 0, 0x33, 0x33);
    anim_draw_pic(11, D_21C8340, 0, 0x2e, 0x23);
    anim_draw_pic(12, D_21C847C, 0, 0x40, 8);
    anim_queue_sound(0xb);
    anim_frame(500, 7);
end:;
}

/*
 * Cake game scene.
 */
void game_cake_success(void)
{
    anim_scene_room(0xc);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(2, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(3, D_21C7504, 0, 0x5f, 0x33);
    anim_draw_pic(4, D_21C6E50, 0x10, 0x21, 0x33);
    anim_draw_pic(5, D_21C81FC, 0, 0x4e, 0x32);
    anim_draw_pic(6, D_217F794, 0, 0x32, 0x20);
    anim_draw_pic(7, D_21C81FC, 0, 0x48, 0x27);
    anim_draw_pic(8, D_21C8340, 0, 0x33, 0x32);
    anim_draw_pic(9, D_21C8340, 0, 0x38, 0x25);
    anim_draw_pic(10, D_21C80D4, 0, 0x40, 0x3e);
    anim_draw_pic(11, D_217F130, 0, 0x45, 0x2c);
    anim_queue_sound(0x2a);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C7BAC, 0, 0x79, 0x26);
    anim_draw_pic(2, D_21C8904, 0x10, 7, 0x26);
    anim_draw_pic(3, D_21C7504, 0, 0x61, 0x32);
    anim_draw_pic(4, D_21C6E50, 0x10, 0x1f, 0x32);
    anim_draw_pic(5, D_217F794, 0x10, 0x45, 0x30);
    anim_draw_pic(6, D_21C80D4, 0, 0x40, 0x3e);
    anim_draw_pic(7, D_214E570, 0x60, 0x38, 0x39);
    anim_draw_pic(8, D_217F130, 0x20, 0x35, 0x27);
    anim_draw_pic(9, (u8 *)0, 0, 0, 0);
    anim_draw_pic(10, (u8 *)0, 0, 0, 0);
    anim_draw_pic(11, (u8 *)0, 0, 0, 0);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C7E40, 0, 0x79, 0x26);
    anim_draw_pic(2, D_21C8B98, 0x10, 7, 0x26);
    anim_draw_pic(3, D_21C773C, 0, 0x61, 0x32);
    anim_draw_pic(4, D_21C708C, 0x10, 0x1f, 0x32);
    anim_draw_pic(5, D_217F794, 0x10, 0x2e, 0x1f);
    anim_draw_pic(6, D_21C86A0, 0, 0x40, 0x23);
    anim_draw_pic(7, D_214E570, 0, 0x4c, 0x2b);
    anim_draw_pic(8, D_214E930, 0, 0x50, 0x18);
    anim_queue_sound(0x10);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C7E40, 0, 0x79, 0x27);
    anim_draw_pic(2, D_21C8B98, 0x10, 7, 0x27);
    anim_draw_pic(3, D_21C773C, 0, 0x61, 0x2d);
    anim_draw_pic(4, D_21C708C, 0x10, 0x1f, 0x2d);
    anim_draw_pic(5, D_21C86A0, 0, 0x40, 0x21);
    anim_draw_pic(6, D_214E930, 0, 0x36, 0x1b);
    anim_draw_pic(7, D_21A4E74, 0, 0x2e, 0x2a);
    anim_draw_pic(8, D_21A51CC, 0, 0x4f, 0x21);
    anim_draw_pic(9, D_214E930, 0, 0x57, 0x15);
    if (anim_frame(500, 7) != 0)
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C7E40, 0, 0x79, 0x22);
    anim_draw_pic(2, D_21C8B98, 0x10, 7, 0x22);
    anim_draw_pic(3, D_21C773C, 0, 0x61, 0x32);
    anim_draw_pic(4, D_21C708C, 0x10, 0x1f, 0x32);
    anim_draw_pic(5, D_21C86A0, 0, 0x40, 0x1f);
    anim_draw_pic(6, D_214E930, 0, 0x2a, 0x1c);
    anim_draw_pic(7, D_21A4E74, 0, 0x5a, 0x16);
    anim_draw_pic(8, D_21A51CC, 0, 0x2c, 0x28);
    anim_draw_pic(9, D_214E930, 0, 0x4a, 0x2a);
    anim_draw_pic(10, D_214E930, 0, 0x46, 0x12);
    anim_frame(500, 7);
end:;
}

/*
 * Cake game scene.
 */
void game_cake_fail(void)
{
    anim_scene_room(0xf);
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C7BAC, 0, 0x79, 0x25);
    anim_draw_pic(2, D_21C8904, 0x10, 7, 0x25);
    anim_draw_pic(3, D_21C7974, 0, 0x5f, 0x34);
    anim_draw_pic(4, D_21C72C8, 0x10, 0x21, 0x34);
    anim_draw_pic(5, D_21C8340, 0, 0x3d, 0x2d);
    anim_draw_pic(6, D_21C80D4, 0, 0x40, 0x3e);
    anim_draw_pic(7, D_21C81FC, 0, 0x3e, 0x23);
    anim_draw_pic(8, D_21C8340, 0, 0x46, 0x17);
    anim_draw_pic(9, D_21C81FC, 0, 0x4f, 0xe);
    anim_draw_pic(10, D_214DDA8, 0x10, 0x2f, 0xd);
    anim_queue_sound(0x2d);
    if (anim_frame(500, 7))
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C7BAC, 0x10, 0x7e, 0x26);
    anim_draw_pic(2, D_21C8904, 0, 2, 0x26);
    anim_draw_pic(3, D_21C7974, 0x10, 0x5f, 0x15);
    anim_draw_pic(4, D_21C72C8, 0, 0x1f, 0x16);
    anim_draw_pic(5, D_21C81FC, 0x20, 0x4b, 0x2e);
    anim_draw_pic(6, D_21C8340, 0x10, 0x41, 0x2d);
    anim_draw_pic(7, D_21C81FC, 0, 0x37, 0x26);
    anim_draw_pic(8, D_21C8340, 0x20, 0x39, 0x13);
    anim_draw_pic(9, D_21C80D4, 0, 0x40, 0x3e);
    anim_draw_pic(10, D_21AD050, 0, 0x41, 0x27);
    anim_draw_pic(11, D_21AD050, 0, 0x45, 0x1b);
    if (anim_frame(500, 7))
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C7BAC, 0x10, 0x83, 0x25);
    anim_draw_pic(2, D_21C8904, 0, -3, 0x25);
    anim_draw_pic(3, D_214E600, 0, 0x33, 0x27);
    anim_draw_pic(4, D_21C7974, 0, 0x5f, 0x34);
    anim_draw_pic(5, D_21C72C8, 0x10, 0x21, 0x34);
    anim_draw_pic(6, D_21C81FC, 0x60, 0x4b, 0x2e);
    anim_draw_pic(7, D_21C81FC, 0, 0x40, 0x35);
    anim_draw_pic(8, D_21C8340, 0x20, 0x35, 0x31);
    anim_draw_pic(9, D_21C8340, 0, 0x43, 0x16);
    anim_draw_pic(10, D_21C80D4, 0, 0x40, 0x3e);
    anim_draw_pic(11, D_219A9E0, 0, 0x4a, 0x3a);
    anim_draw_pic(12, D_21AD050, 0, 0x4c, 0x23);
    anim_draw_pic(13, D_214E600, 0, 0x48, 0x36);
    if (anim_frame(500, 7))
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_214E600, 0, 0x37, 0x3c);
    anim_draw_pic(2, D_21C7BAC, 0x10, 0x88, 0x26);
    anim_draw_pic(3, D_21C8904, 0, -8, 0x26);
    anim_draw_pic(4, D_21C7974, 0x10, 0x5f, 0x34);
    anim_draw_pic(5, D_21C72C8, 0, 0x21, 0x34);
    anim_draw_pic(6, D_21C81FC, 0x60, 0x4c, 0x31);
    anim_draw_pic(7, D_21C8340, 0x20, 0x35, 0x31);
    anim_draw_pic(8, D_21C81FC, 0, 0x3e, 0x37);
    anim_draw_pic(9, D_21C8340, 0x10, 0x43, 0x23);
    anim_draw_pic(10, D_214DDA8, 0, 0x36, 0x15);
    anim_draw_pic(11, D_214DE34, 0x10, 0x54, 0x19);
    anim_draw_pic(12, D_21C80D4, 0, 0x40, 0x3e);
    anim_draw_pic(13, D_214F038, 0, 0x31, 0x3d);
    anim_draw_pic(14, D_214E600, 0, 0x46, 0x43);
    if (anim_frame(500, 7))
        goto end;
    anim_draw_pic(0, D_21C5C28, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21C7974, 0, 0x5f, 0x34);
    anim_draw_pic(2, D_21C72C8, 0x10, 0x21, 0x34);
    anim_draw_pic(3, D_21C81FC, 0x60, 0x4b, 0x31);
    anim_draw_pic(4, D_21C8340, 0x20, 0x35, 0x32);
    anim_draw_pic(5, D_21C81FC, 0, 0x3f, 0x37);
    anim_draw_pic(6, D_21C8340, 0x10, 0x43, 0x24);
    anim_draw_pic(7, D_214DDA8, 0x10, 0x53, 0x16);
    anim_draw_pic(8, D_214DE34, 0, 0x38, 0x17);
    anim_draw_pic(9, D_21C80D4, 0, 0x40, 0x3e);
    anim_draw_pic(10, D_214F038, 0, 0x31, 0x3f);
    anim_draw_pic(11, 0, 0, 0, 0);
    anim_draw_pic(12, 0, 0, 0, 0);
    anim_draw_pic(13, 0, 0, 0, 0);
    anim_draw_pic(14, 0, 0, 0, 0);
    anim_frame(500, 7);
end:;
}

/*
 * Built-in game "おたからザクザク" (treasure dig) entry (game menu entry 0): game_treasure_loop.
 */
void game_treasure(void)
{
    game_treasure_finds = 0;
    game_treasure_loop();
}

/*
 * Treasure game main loop ("どれをきる?" which one to cut? / "もう1ぽん!" one more) (asm).
 */
int game_treasure_loop(void)
{
    u8 h;
    int sel;
    int t;
    int idx;

    api_set_btn_a(0);
    api_set_btn_b(0);
    api_set_btn_c(0);
    for (;;) {
        game_treasure_scene_a();
        t = 10;
        api_new_screen();
        api_object_create_plain(0, 0x1c, 0, 0, 0x21c8e2c);
        api_set_btn_a(0);
        api_set_btn_b(0);
        api_set_btn_c(0);
        api_object_create_plain(0x5c, 0x1c, 0x10, 0, 0x21ca050);
        api_object_create_plain(0, 0x17, 0x20, 0, 0x21cb210);
        api_object_create_plain(0x1e, 0x17, 0x20, 0, 0x21cb210);
        api_object_create_plain(0x3c, 0x17, 0x20, 0, 0x21cb210);
        api_object_create_plain(0x32, 0x1c, 0x30, 0, api_picture_address(0x65));
        api_object_create_plain(0x47, 0x26, 0x40, 0, api_picture_address(0xba));
        api_object_create_plain(0x47, 0x20, 0x50, 0, api_picture_address(0xba));
        api_object_create_plain(0, 0x54, 0x60, 0, 0x21cc29c);
        h = api_object_create(0x11, 0x34, 0xc8, 0, 0x10, 0x21cbac8);
        game_treasure_text_obj = api_object_create_plain(0, 0, 0x80, 0, 0);
        api_draw_text(game_treasure_text_obj, 0x40, 0x24, 0x80, D_255FC00, 1, 6);
        sel = 0;
        api_present();
        api_timeout_start_ch5(1000);
        for (;;) {
            if (api_get_btn_c())
                goto cancel;
            if (api_get_btn_a()) {
                api_set_btn_a(0);
                if (++sel > 2)
                    sel = 0;
                api_object_move_dirty(h, sel * 30 + 0x11, 0x34);
                api_present();
            }
            if (api_get_btn_b()) {
                api_set_btn_b(0);
                break;
            }
            asm volatile("halt");
            if (api_timeout_elapsed_ch5()) {
                api_timeout_clear_ch5();
                if (--t < 0)
                    break;
            }
            api_idle_tick();
        }
        idx = (u16)api_rand_except(3, 3) * 3 + sel;
        D_255FBD0[idx / 3][idx % 3]();
        if (api_get_btn_c())
            goto cancel;
        switch (D_255FBF4[idx / 3][idx % 3]) {
        case 0:
            game_treasure_scene_f();
            if (api_get_btn_c()) {
                api_set_btn_c(0);
                if (++game_treasure_finds > 2)
                    goto rank;
                return -1;
            }
            game_treasure_found_scene();
            if (++game_treasure_finds > 2)
                goto rank;
            break;
        case 1:
            game_treasure_scene_g();
            if (api_get_btn_c()) {
                api_set_btn_c(0);
                goto rank;
            }
            game_treasure_bust_scene();
            goto rank;
        case 2:
            game_treasure_play();
            if (api_get_btn_c())
                goto cancel;
            game_treasure_scene_h();
            break;
        }
        api_set_btn_a(0);
        api_set_btn_b(0);
        api_set_btn_c(0);
    }
rank:
    if (game_treasure_finds > 2)
        return 3;
    if (game_treasure_finds > 1)
        return 2;
    if (game_treasure_finds != 0)
        return 1;
    return 0;
cancel:
    api_set_btn_c(0);
    return -1;
}

/*
 * Treasure game scene.
 */
void game_treasure_scene_a(void)
{
    api_scene_room(0xd);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x4b, 0x1f);
    api_anim_draw_image(2, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(3, D_21CB210, 0, 0xf, 0x1f);
    api_anim_draw_image(4, api_picture_address(0x1b), 0, 9, 0x18);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x16, 0x24);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x27, 0x18);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x45, 0x18);
    api_anim_draw_image(8, api_picture_address(0x1b), 0, 0x34, 0x24);
    api_anim_draw_image(9, api_picture_address(0x1b), 0, 0x52, 0x24);
    api_anim_draw_image(10, D_21CC29C, 0, 0x40, 0x41);
    api_anim_queue_sound(0x3c);
    if (api_anim_frame(500, 3) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x4b, 0x1f);
    api_anim_draw_image(2, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(3, D_21CB210, 0, 0xf, 0x1f);
    api_anim_draw_image(4, api_picture_address(0x1b), 0, 9, 0x21);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x16, 0x1c);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x27, 0x21);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x34, 0x1c);
    api_anim_draw_image(8, api_picture_address(0x1b), 0, 0x45, 0x21);
    api_anim_draw_image(9, api_picture_address(0x1b), 0, 0x52, 0x1c);
    api_anim_draw_image(10, D_21CA734, 0, 0x83, 0x24);
    api_anim_draw_image(11, D_21CC29C, 0, 0x40, 0x41);
    if (api_anim_frame(500, 3) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x4b, 0x1f);
    api_anim_draw_image(2, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(3, D_21CB210, 0, 0xf, 0x1f);
    api_anim_draw_image(4, api_picture_address(0x1b), 0, 9, 0x18);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x16, 0x24);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x27, 0x18);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x45, 0x18);
    api_anim_draw_image(8, api_picture_address(0x1b), 0, 0x34, 0x24);
    api_anim_draw_image(9, api_picture_address(0x1b), 0, 0x52, 0x24);
    api_anim_draw_image(10, D_21CA734, 0, 0x74, 0x24);
    api_anim_draw_image(11, D_21CC29C, 0, 0x40, 0x41);
    api_anim_queue_sound(0x3c);
    if (api_anim_frame(500, 3) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x4b, 0x1f);
    api_anim_draw_image(2, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(3, D_21CB210, 0, 0xf, 0x1f);
    api_anim_draw_image(4, api_picture_address(0x1b), 0, 9, 0x21);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x16, 0x1c);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x27, 0x21);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x34, 0x1c);
    api_anim_draw_image(8, api_picture_address(0x1b), 0, 0x45, 0x21);
    api_anim_draw_image(9, api_picture_address(0x1b), 0, 0x52, 0x1c);
    api_anim_draw_image(10, D_21CA050, 0, 0x74, 0x24);
    api_anim_draw_image(11, api_picture_address(6), 0, 0x74, 0x11);
    api_anim_draw_image(12, D_21CC29C, 0, 0x40, 0x41);
    api_anim_frame(500, 3);
end:;
}

/*
 * Treasure game scene.
 */
void game_treasure_scene_b(void)
{
    api_scene_room(0xd);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x4b, 0x1f);
    api_anim_draw_image(2, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(3, D_21CB210, 0, 0xf, 0x1f);
    api_anim_draw_image(4, D_21CA050, 0, 0x74, 0x24);
    api_anim_draw_image(5, D_21CBAC8, 0x10, 0x57, 0x22);
    api_anim_draw_image(6, D_21CBAC8, 0x10, 0x39, 0x22);
    api_anim_draw_image(7, D_21CBAC8, 0x10, 0x1b, 0x22);
    api_anim_draw_image(8, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(9, api_picture_address(0x65), 0, 0x5a, 0x10);
    api_anim_draw_image(10, api_picture_address(0xba), 0, 0x60, 0xf);
    api_anim_draw_image(11, api_picture_address(0xba), 0, 0x60, 9);
    api_anim_text_line(0xc, D_255FC00, 1, 0x44, 0xc);
    api_anim_frame(500, 7);
}

/*
 * Treasure game scene.
 */
void game_treasure_scene_c(void)
{
    u8 r;

    api_scene_room(8);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0xf, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x74, 0x24);
    api_anim_draw_image(3, D_21CBAC8, 0x10, 0x57, 0x33);
    api_anim_draw_image(4, D_21CC29C, 0, 0x40, 0x41);
    r = api_anim_frame(500, 7);
    if (r != 0)
        return;
    api_anim_draw_image(0, D_21C8E2C, 0, -0x2d, 0x22);
    api_anim_draw_image(1, D_21C8E2C, 0, 0x4f, 0x24);
    api_anim_draw_image(2, D_21CB210, 0, 0x1e, 0x1f);
    api_anim_draw_image(3, D_21CA734, 0, 0x65, 0x24);
    api_anim_draw_image(4, D_21CBAC8, 0x10, 0x48, 0x33);
    api_anim_draw_image(5, D_21CC29C, 0, 0x4f, 0x41);
    api_anim_draw_image(6, D_21CC29C, 0, -0x2e, 0x41);
    r = api_anim_frame(500, 7);
    if (r != 0)
        return;
    api_anim_draw_image(0, D_21C8E2C, 0, -0x1e, 0x22);
    api_anim_draw_image(1, D_21C8E2C, 0, 0x5e, 0x24);
    api_anim_draw_image(2, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(3, D_21CA734, 0, 0x56, 0x24);
    api_anim_draw_image(4, D_21CBAC8, 0x10, 0x38, 0x32);
    api_anim_draw_image(5, D_21CC29C, 0, 0x5e, 0x41);
    api_anim_draw_image(6, D_21CC29C, 0, -0x1f, 0x41);
    r = api_anim_frame(500, 7);
    if (r != 0)
        return;
    api_anim_draw_image(0, D_21C8E2C, 0, -0x1e, 0x22);
    api_anim_draw_image(1, D_21C8E2C, 0, 0x5e, 0x24);
    api_anim_draw_image(2, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(3, D_21CA050, 0, 0x56, 0x24);
    api_anim_draw_image(4, D_21CBAC8, 0x10, 0x39, 0x22);
    api_anim_draw_image(5, D_21CC29C, 0, 0x5e, 0x41);
    api_anim_draw_image(6, D_21CC29C, 0, -0x1f, 0x41);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x31, 0x1b);
    api_anim_queue_sound(0xb);
    api_anim_frame(500, 7);
}

/*
 * Treasure game scene.
 */
void game_treasure_scene_d(void)
{
    u8 r;

    api_scene_room(6);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x74, 0x24);
    api_anim_draw_image(3, D_21CBAC8, 0x10, 0x57, 0x33);
    api_anim_draw_image(4, D_21CC29C, 0, 0x40, 0x41);
    r = api_anim_frame(500, 7);
    if (r != 0)
        return;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x65, 0x24);
    api_anim_draw_image(3, D_21CBAC8, 0x10, 0x48, 0x33);
    api_anim_draw_image(4, D_21CC29C, 0, 0x40, 0x41);
    r = api_anim_frame(500, 7);
    if (r != 0)
        return;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x56, 0x24);
    api_anim_draw_image(3, D_21CBAC8, 0x10, 0x38, 0x32);
    api_anim_draw_image(4, D_21CC29C, 0, 0x40, 0x41);
    r = api_anim_frame(500, 7);
    if (r != 0)
        return;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x56, 0x24);
    api_anim_draw_image(3, D_21CBAC8, 0x10, 0x39, 0x22);
    api_anim_draw_image(4, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x31, 0x1b);
    api_anim_queue_sound(0xb);
    api_anim_frame(500, 7);
}

/*
 * Treasure game scene.
 */
void game_treasure_scene_e(void)
{
    api_scene_room(8);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB210, 0, 0x4b, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x74, 0x24);
    api_anim_draw_image(3, D_21CBAC8, 0x10, 0x57, 0x33);
    api_anim_draw_image(4, D_21CC29C, 0, 0x40, 0x41);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x31, 0x24);
    api_anim_draw_image(1, D_21C8E2C, 0, 0xb1, 0x24);
    api_anim_draw_image(2, D_21CB210, 0, 0x3c, 0x1f);
    api_anim_draw_image(3, D_21CA734, 0, 0x65, 0x24);
    api_anim_draw_image(4, D_21CBAC8, 0x10, 0x48, 0x33);
    api_anim_draw_image(5, D_21CC29C, 0, 0x31, 0x41);
    api_anim_draw_image(6, D_21CC29C, 0, 0xaf, 0x41);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x22, 0x24);
    api_anim_draw_image(1, D_21C8E2C, 0, 0xa2, 0x24);
    api_anim_draw_image(2, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(3, D_21CA734, 0, 0x56, 0x24);
    api_anim_draw_image(4, D_21CBAC8, 0x10, 0x38, 0x32);
    api_anim_draw_image(5, D_21CC29C, 0, 0x22, 0x41);
    api_anim_draw_image(6, D_21CC29C, 0, 0xa0, 0x41);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0xa2, 0x24);
    api_anim_draw_image(1, D_21C8E2C, 0, 0x22, 0x24);
    api_anim_draw_image(2, D_21CB210, 0, 0x2d, 0x1f);
    api_anim_draw_image(3, D_21CA050, 0, 0x56, 0x24);
    api_anim_draw_image(4, D_21CBAC8, 0x10, 0x39, 0x22);
    api_anim_draw_image(5, D_21CC29C, 0, 0x22, 0x41);
    api_anim_draw_image(6, D_21CC29C, 0, 0xa0, 0x41);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x31, 0x1b);
    api_anim_queue_sound(0xb);
    api_anim_frame(500, 7);
end:;
}

/*
 * Treasure game scene.
 */
void game_treasure_scene_f(void)
{
    api_scene_room(5);
    api_anim_draw_image(0, api_picture_address(0x76), 0, 0x40, 0x24);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, api_picture_address(0x76), 0, 0x18, 0x24);
    api_anim_queue_sound(0x3b);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, api_picture_address(0x76), 0, -0x18, 0x23);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, 0, 0, 0, 0);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0x76), 0, 0x40, 0x24);
    api_anim_draw_image(1, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(2, api_picture_address(0xbf), 0x20, 0x40, 0x43);
    api_anim_queue_sound(0x1c);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, api_picture_address(0xc0), 0, 0x40, 5);
    api_anim_draw_image(4, api_picture_address(0xc0), 0x20, 0x40, 0x43);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0x76), 0, 0x40, 0x24);
    api_anim_draw_image(1, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(2, api_picture_address(0xbf), 0x20, 0x40, 0x43);
    api_anim_draw_image(3, 0, 0, 0, 0);
    api_anim_draw_image(4, 0, 0, 0, 0);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, api_picture_address(0xc0), 0, 0x40, 5);
    api_anim_draw_image(4, api_picture_address(0xc0), 0x20, 0x40, 0x43);
    api_anim_frame(500, 7);
end:;
}

/*
 * Treasure game scene.
 */
void game_treasure_scene_g(void)
{
    api_scene_room(7);
    api_anim_draw_image(0, api_picture_address(0x76), 0, 0x40, 0x24);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, api_picture_address(0x76), 0, 0x18, 0x24);
    api_anim_queue_sound(0x3b);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, api_picture_address(0x76), 0, -0x18, 0x23);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, 0, 0, 0, 0);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0xbf), 0, 0x40, 4);
    api_anim_draw_image(2, api_picture_address(0xbf), 0x20, 0x40, 0x45);
    api_anim_draw_image(3, api_picture_address(0xbf), 0x20, 0x40, 0x46);
    api_anim_draw_image(4, D_21CC72C, 0, 0x33, 0x2d);
    api_anim_draw_image(5, api_picture_address(0x76), 0, 0x40, -1);
    api_anim_draw_image(6, api_picture_address(0x76), 0, 0x40, 0x4b);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x56, 0x23);
    api_anim_draw_image(3, D_21CC72C, 0, 0x33, 0x2d);
    api_anim_draw_image(4, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(5, api_picture_address(2), 0, 0x6f, 0xc);
    api_anim_draw_image(6, 0, 0, 0, 0);
    api_anim_queue_sound(0x22);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x56, 0x24);
    api_anim_draw_image(3, D_21CC72C, 0, 0x33, 0x2d);
    api_anim_draw_image(4, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(5, 0, 0, 0, 0);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x56, 0x23);
    api_anim_draw_image(3, D_21CC72C, 0, 0x33, 0x2d);
    api_anim_draw_image(4, api_picture_address(2), 0, 0x6f, 0xc);
    api_anim_draw_image(5, D_21CC29C, 0, 0x40, 0x41);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x56, 0x24);
    api_anim_draw_image(3, D_21CC72C, 0, 0x33, 0x2d);
    api_anim_draw_image(4, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(5, 0, 0, 0, 0);
    api_anim_frame(500, 7);
end:;
}

/*
 * Treasure game play loop (digging choices, timing).
 */
void game_treasure_play(void)
{
    api_scene_room(0x13);
    api_anim_draw_image(0, api_picture_address(0x76), 0, 0x40, 0x24);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, api_picture_address(0x76), 0, 0x18, 0x24);
    api_anim_queue_sound(0x3b);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, api_picture_address(0x76), 0, -0x18, 0x23);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0x76), 0, 0x40, 0);
    api_anim_draw_image(2, api_picture_address(0x76), 0, 0x40, 0x49);
    api_anim_draw_image(3, 0, 0, 0, 0);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0xbf), 0, 0x40, 4);
    api_anim_draw_image(2, api_picture_address(0xbf), 0x20, 0x40, 0x45);
    api_anim_draw_image(3, api_picture_address(0x76), 0, 0x40, -1);
    api_anim_draw_image(4, api_picture_address(0xb2), 0, 0x40, 0x24);
    api_anim_draw_image(5, api_picture_address(0x76), 0, 0x40, 0x4a);
    api_anim_queue_sound(0x1f);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0xbf), 0, 0x40, 4);
    api_anim_draw_image(2, api_picture_address(0xbf), 0x20, 0x40, 0x45);
    api_anim_draw_image(3, api_picture_address(0x76), 0, 0x40, -1);
    api_anim_draw_image(4, api_picture_address(0xb2), 0, 0x40, 0x1d);
    api_anim_draw_image(5, api_picture_address(0xb2), 0, 0x2d, 0x27);
    api_anim_draw_image(6, api_picture_address(0xb2), 0, 0x4f, 0x27);
    api_anim_draw_image(7, api_picture_address(0x76), 0, 0x40, 0x4a);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0xbf), 0, 0x40, 5);
    api_anim_draw_image(1, api_picture_address(0xbf), 0, 0x40, 4);
    api_anim_draw_image(2, api_picture_address(0xbf), 0x20, 0x40, 0x45);
    api_anim_draw_image(3, api_picture_address(0x76), 0, 0x40, -1);
    api_anim_draw_image(4, api_picture_address(0xb2), 0, 0x40, 0xf);
    api_anim_draw_image(5, api_picture_address(0xb2), 0, 0x2d, 0x19);
    api_anim_draw_image(6, api_picture_address(0xb2), 0, 0x4f, 0x19);
    api_anim_draw_image(7, api_picture_address(0xb2), 0, 0x40, 0x25);
    api_anim_draw_image(8, api_picture_address(0xb2), 0, 0x22, 0x26);
    api_anim_draw_image(9, api_picture_address(0xb2), 0, 0x5f, 0x26);
    api_anim_draw_image(10, api_picture_address(0x76), 0, 0x40, 0x4a);
    api_anim_queue_sound(0x1f);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, api_picture_address(0x18), 0, 0x7a, 10);
    api_anim_draw_image(1, api_picture_address(0x18), 0x10, 0xb, 0x3d);
    api_anim_draw_image(2, api_picture_address(0x18), 0, 0x39, 7);
    api_anim_draw_image(3, api_picture_address(0x18), 0, 0x39, 0x18);
    api_anim_draw_image(4, api_picture_address(0x18), 0x10, 0x6c, 10);
    api_anim_draw_image(5, api_picture_address(0x18), 0x10, 0x78, 0x23);
    api_anim_draw_image(6, api_picture_address(0x18), 0, 0x53, 9);
    api_anim_draw_image(7, api_picture_address(0x18), 0, 0x62, 0x22);
    api_anim_draw_image(8, api_picture_address(0x18), 0, 0x4a, 0x23);
    api_anim_draw_image(9, api_picture_address(0x18), 0, 0x78, 0x3c);
    api_anim_draw_image(10, api_picture_address(0x18), 0, 0x6a, 0x3d);
    api_anim_draw_image(11, api_picture_address(0x18), 0x10, 0xb, 0x26);
    api_anim_draw_image(12, api_picture_address(0x18), 0x10, 0xc, 0xd);
    api_anim_draw_image(13, api_picture_address(0x18), 0, 0x22, 10);
    api_anim_draw_image(14, api_picture_address(0x18), 0, 0x30, 0x2b);
    api_anim_draw_image(15, api_picture_address(0x18), 0, 0x52, 0x3c);
    api_anim_draw_image(16, api_picture_address(0x18), 0, 0x1f, 0x20);
    api_anim_draw_image(17, api_picture_address(0x18), 0x10, 0x23, 0x3d);
    api_anim_draw_image(18, api_picture_address(0x18), 0, 0x3c, 0x3d);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(2, api_picture_address(0x18), 0, 0x3e, 0x18);
    api_anim_draw_image(3, api_picture_address(0x18), 0, 0x2c, 0x1f);
    api_anim_draw_image(4, api_picture_address(0x18), 0, 0x57, 0x16);
    api_anim_draw_image(5, api_picture_address(0x18), 0, 0x67, 0x28);
    api_anim_draw_image(6, api_picture_address(0x18), 0, 0x52, 0x2a);
    api_anim_draw_image(7, api_picture_address(0x18), 0, 0x39, 0x2b);
    api_anim_draw_image(8, api_picture_address(0x18), 0, 0x5c, 0x3c);
    api_anim_draw_image(9, api_picture_address(0x18), 0x10, 0x30, 0x3c);
    api_anim_draw_image(10, api_picture_address(0x18), 0, 0x44, 0x3d);
    api_anim_draw_image(11, 0, 0, 0, 0);
    api_anim_draw_image(12, 0, 0, 0, 0);
    api_anim_draw_image(13, 0, 0, 0, 0);
    api_anim_draw_image(14, 0, 0, 0, 0);
    api_anim_draw_image(15, 0, 0, 0, 0);
    api_anim_draw_image(16, 0, 0, 0, 0);
    api_anim_draw_image(17, 0, 0, 0, 0);
    api_anim_draw_image(18, 0, 0, 0, 0);
    api_anim_queue_sound(0x1f);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x56, 0x24);
    api_anim_draw_image(3, D_21CAE18, 0, 0x56, 0x22);
    api_anim_draw_image(4, api_picture_address(0x18), 0, 0x48, 0x24);
    api_anim_draw_image(5, api_picture_address(0x17), 0, 0x34, 0x1a);
    api_anim_draw_image(6, api_picture_address(0x18), 0x10, 0x40, 0x1b);
    api_anim_draw_image(7, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(8, 0, 0, 0, 0);
    api_anim_draw_image(9, 0, 0, 0, 0);
    api_anim_draw_image(10, 0, 0, 0, 0);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x56, 0x24);
    api_anim_draw_image(3, D_21CB018, 0, 0x56, 0x22);
    api_anim_draw_image(4, D_21ACF20, 0, 0x44, 0x1b);
    api_anim_draw_image(5, api_picture_address(0x22), 0, 0x73, 0xc);
    api_anim_draw_image(6, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(7, 0, 0, 0, 0);
    api_anim_queue_sound(0x13);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x56, 0x25);
    api_anim_draw_image(3, D_21CB018, 0, 0x56, 0x23);
    api_anim_draw_image(4, D_21ACF20, 0, 0x45, 0x1d);
    api_anim_draw_image(5, api_picture_address(0x23), 0, 0x73, 0xd);
    api_anim_draw_image(6, D_21CC29C, 0, 0x40, 0x41);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x56, 0x24);
    api_anim_draw_image(3, D_21CB018, 0, 0x56, 0x22);
    api_anim_draw_image(4, D_21ACF20, 0, 0x44, 0x1b);
    api_anim_draw_image(5, api_picture_address(0x22), 0, 0x73, 0xc);
    api_anim_draw_image(6, D_21CC29C, 0, 0x40, 0x41);
    api_anim_queue_sound(0x13);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x56, 0x25);
    api_anim_draw_image(3, D_21CB018, 0, 0x56, 0x23);
    api_anim_draw_image(4, D_21ACF20, 0, 0x45, 0x1d);
    api_anim_draw_image(5, api_picture_address(0x23), 0, 0x73, 0xd);
    api_anim_draw_image(6, D_21CC29C, 0, 0x40, 0x41);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x56, 0x24);
    api_anim_draw_image(3, D_21CAE18, 0, 0x56, 0x22);
    api_anim_draw_image(4, D_21AD050, 0, 0x44, 0x23);
    api_anim_draw_image(5, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(6, 0, 0, 0, 0);
    api_anim_queue_sound(0x14);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x56, 0x25);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(4, 0, 0, 0, 0);
    api_anim_draw_image(5, 0, 0, 0, 0);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0x10, 0x58, 0x25);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_queue_sound(0x16);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x56, 0x25);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0x10, 0x58, 0x25);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_queue_sound(0x16);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA734, 0, 0x56, 0x25);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_frame(500, 7);
end:;
}

/*
 * Treasure game scene.
 */
void game_treasure_scene_h(void)
{
    api_scene_room(10);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x56, 0x24);
    api_anim_draw_image(3, D_21CBAC8, 0x10, 0x3a, 0x22);
    api_anim_draw_image(4, api_picture_address(0x16), 0, 0x73, 0x21);
    api_anim_draw_image(5, api_picture_address(0x65), 0, 0x43, 0xf);
    api_anim_draw_image(6, api_picture_address(0xba), 0, 0x48, 7);
    api_anim_draw_image(7, api_picture_address(0xba), 0, 0x49, 0xe);
    api_anim_text_line(8, D_255FC10, 1, 0x2c, 0xb);
    api_anim_draw_image(9, D_21CC29C, 0, 0x40, 0x41);
    api_anim_queue_sound(0x1e);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x56, 0x25);
    api_anim_draw_image(3, D_21CBAC8, 0x10, 0x3a, 0x23);
    api_anim_draw_image(4, api_picture_address(0x16), 0, 0x74, 0x22);
    api_anim_draw_image(5, api_picture_address(0x65), 0, 0x43, 0xf);
    api_anim_draw_image(6, api_picture_address(0xba), 0, 0x48, 7);
    api_anim_draw_image(7, api_picture_address(0xba), 0, 0x49, 0xe);
    api_anim_text_line(8, D_255FC10, 1, 0x2c, 0xb);
    api_anim_draw_image(9, D_21CC29C, 0, 0x40, 0x41);
    api_anim_frame(500, 0);
}

/*
 * Treasure game result.
 */
void game_treasure_found_scene(void)
{
    int k;
    api_scene_room(9);
    k = 0x24;
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, k);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x68, 0x20);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(4, D_21CBBAC, 0, 0x2d, 0x30);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x1d, 0x43);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x25, k);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x48, 0x3d);
    api_anim_draw_image(8, api_picture_address(2), 0, 0x53, 0x11);
    api_anim_queue_sound(0x19);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, k);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x68, k);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(4, D_21CBBAC, 0, 0x2d, 0x30);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x14, 0x35);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x40, 0x29);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x37, 0x44);
    api_anim_draw_image(8, (u8 *)0, 0, 0, 0);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, k);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x68, 0x20);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(4, D_21CBBAC, 0, 0x2d, 0x30);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x1d, 0x43);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x25, k);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x48, 0x3d);
    api_anim_draw_image(8, api_picture_address(2), 0, 0x53, 0x11);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, k);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x68, k);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(4, D_21CBBAC, 0, 0x2d, 0x30);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x14, 0x35);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x40, 0x29);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x37, 0x44);
    api_anim_draw_image(8, (u8 *)0, 0, 0, 0);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, k);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x68, 0x20);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(4, D_21CBBAC, 0, 0x2d, 0x30);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x1d, 0x43);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x25, k);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x48, 0x3d);
    api_anim_draw_image(8, api_picture_address(2), 0, 0x53, 0x11);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0, 0x40, k);
    api_anim_draw_image(1, D_21CB66C, 0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0, 0x68, k);
    api_anim_draw_image(3, D_21CC29C, 0, 0x40, 0x41);
    api_anim_draw_image(4, D_21CBBAC, 0, 0x2d, 0x30);
    api_anim_draw_image(5, api_picture_address(0x1b), 0, 0x14, 0x35);
    api_anim_draw_image(6, api_picture_address(0x1b), 0, 0x40, 0x29);
    api_anim_draw_image(7, api_picture_address(0x1b), 0, 0x37, 0x44);
    api_anim_draw_image(8, (u8 *)0, 0, 0, 0);
    api_anim_frame(500, 0);
}

/*
 * Treasure game main round.
 */
void game_treasure_bust_scene(void)
{
    api_scene_room(0x17);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0x0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0x0, 0x56, 0x22);
    api_anim_draw_image(3, D_21CAE18, 0x0, 0x56, 0x20);
    api_anim_draw_image(4, D_21CCF44, 0x0, 0x33, 0x2d);
    api_anim_draw_image(5, api_picture_address(0x6), 0x0, 0x56, 0xb);
    api_anim_draw_image(6, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_queue_sound(0xd);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0x0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0x0, 0x56, 0x22);
    api_anim_draw_image(3, D_21CAE18, 0x0, 0x56, 0x20);
    api_anim_draw_image(4, D_21CCF44, 0x0, 0x33, 0x2d);
    api_anim_draw_image(5, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_draw_image(6, (u8 *)0x0, 0x0, 0x0, 0);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0x0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CA050, 0x10, 0x59, 0x24);
    api_anim_draw_image(3, D_21CAE18, 0x10, 0x59, 0x22);
    api_anim_draw_image(4, api_picture_address(0x16), 0x10, 0x40, 0x16);
    api_anim_draw_image(5, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(6, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_queue_sound(0xf);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CB66C, 0x0, 0x2d, 0x1f);
    api_anim_draw_image(2, D_21CCF44, 0x10, 0x8a, 0x28);
    api_anim_draw_image(3, D_21CA050, 0x10, 0x5f, 0x20);
    api_anim_draw_image(4, D_21CAE18, 0x10, 0x5f, 0x1e);
    api_anim_draw_image(5, api_picture_address(0x16), 0x10, 0x44, 0x11);
    api_anim_draw_image(6, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(7, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CA050, 0x0, 0x4e, 0x20);
    api_anim_draw_image(2, D_21CAE18, 0x0, 0x4e, 0x1e);
    api_anim_draw_image(3, api_picture_address(0x16), 0x0, 0x6a, 0x13);
    api_anim_draw_image(4, D_21CCF44, 0x0, 0x26, 0x1e);
    api_anim_draw_image(5, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(6, D_21CCF44, 0x0, 0x1a, 0x27);
    api_anim_draw_image(7, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(8, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_queue_sound(0xf);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CCF44, 0x10, 0x70, 0x12);
    api_anim_draw_image(2, D_21CA050, 0x10, 0x5c, 0x1d);
    api_anim_draw_image(3, D_21CAE18, 0x10, 0x5c, 0x1b);
    api_anim_draw_image(4, api_picture_address(0x16), 0x10, 0x41, 0xc);
    api_anim_draw_image(5, D_21CCF44, 0x10, 0x82, 0x16);
    api_anim_draw_image(6, D_21CC72C, 0x0, 0x26, 0x1e);
    api_anim_draw_image(7, D_21CC72C, 0x0, 0x1a, 0x27);
    api_anim_draw_image(8, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(9, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(10, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CC72C, 0x10, 0x70, 0x12);
    api_anim_draw_image(2, D_21CC72C, 0x10, 0x82, 0x15);
    api_anim_draw_image(3, D_21CCF44, 0x0, 0x44, 0x17);
    api_anim_draw_image(4, D_21CCF44, 0x0, 0x44, 0x17);
    api_anim_draw_image(5, D_21CCF44, 0x0, 0x25, 0x12);
    api_anim_draw_image(6, D_21CA050, 0x0, 0x57, 0x23);
    api_anim_draw_image(7, D_21CAE18, 0x0, 0x57, 0x21);
    api_anim_draw_image(8, api_picture_address(0x16), 0x0, 0x70, 0x11);
    api_anim_draw_image(9, D_21CC72C, 0x0, 0x26, 0x1e);
    api_anim_draw_image(10, D_21CC72C, 0x0, 0x1a, 0x27);
    api_anim_draw_image(11, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(12, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(13, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_queue_sound(0xf);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CC72C, 0x10, 0x70, 0x12);
    api_anim_draw_image(2, D_21CC72C, 0x10, 0x82, 0x15);
    api_anim_draw_image(3, D_21CC72C, 0x0, 0x44, 0x17);
    api_anim_draw_image(4, D_21CC72C, 0x0, 0x25, 0x12);
    api_anim_draw_image(5, D_21CCF44, 0x10, 0x6c, 0x24);
    api_anim_draw_image(6, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(7, D_21CCF44, 0x10, 0x75, 0x31);
    api_anim_draw_image(8, D_21CA050, 0x10, 0x62, 0x28);
    api_anim_draw_image(9, D_21CAE18, 0x10, 0x62, 0x26);
    api_anim_draw_image(10, api_picture_address(0x16), 0x10, 0x4a, 0x17);
    api_anim_draw_image(11, D_21CC72C, 0x0, 0x26, 0x1e);
    api_anim_draw_image(12, D_21CC72C, 0x0, 0x1a, 0x27);
    api_anim_draw_image(13, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(14, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CC72C, 0x10, 0x70, 0x12);
    api_anim_draw_image(2, D_21CC72C, 0x10, 0x82, 0x15);
    api_anim_draw_image(3, D_21CC72C, 0x0, 0x44, 0x17);
    api_anim_draw_image(4, D_21CC72C, 0x0, 0x25, 0x12);
    api_anim_draw_image(5, D_21CC72C, 0x10, 0x6c, 0x24);
    api_anim_draw_image(6, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(7, D_21CC72C, 0x10, 0x75, 0x31);
    api_anim_draw_image(8, D_21CC72C, 0x0, 0x26, 0x1e);
    api_anim_draw_image(9, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(10, D_21CCF44, 0x0, 0x5, 0x20);
    api_anim_draw_image(11, D_21CC72C, 0x0, 0x1a, 0x27);
    api_anim_draw_image(12, D_21CCF44, 0x0, 0x2c, 0x34);
    api_anim_draw_image(13, D_21CA050, 0x0, 0x57, 0x2c);
    api_anim_draw_image(14, D_21CAE18, 0x0, 0x57, 0x2a);
    api_anim_draw_image(15, api_picture_address(0x16), 0x0, 0x71, 0x1f);
    api_anim_draw_image(16, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_queue_sound(0x1e);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CC72C, 0x10, 0x70, 0x12);
    api_anim_draw_image(2, D_21CC72C, 0x10, 0x82, 0x15);
    api_anim_draw_image(3, D_21CC72C, 0x0, 0x44, 0x17);
    api_anim_draw_image(4, D_21CC72C, 0x0, 0x25, 0x12);
    api_anim_draw_image(5, D_21CC72C, 0x10, 0x6c, 0x24);
    api_anim_draw_image(6, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(7, D_21CC72C, 0x10, 0x75, 0x31);
    api_anim_draw_image(8, D_21CC72C, 0x0, 0x26, 0x1e);
    api_anim_draw_image(9, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(10, D_21CC72C, 0x0, 0x5, 0x20);
    api_anim_draw_image(11, D_21CC72C, 0x0, 0x1a, 0x27);
    api_anim_draw_image(12, D_21CC72C, 0x0, 0x2c, 0x34);
    api_anim_draw_image(13, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_draw_image(14, D_21CCF44, 0x0, 0x21, 0x40);
    api_anim_draw_image(15, D_21CCF44, 0x0, 0xa, 0x4a);
    api_anim_draw_image(16, D_21CA050, 0x10, 0x60, 0x32);
    api_anim_draw_image(17, D_21CAE18, 0x10, 0x60, 0x30);
    api_anim_draw_image(18, api_picture_address(0x16), 0x10, 0x43, 0x25);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CCF44, 0x0, 0xb, 0xe);
    api_anim_draw_image(2, D_21CC72C, 0x10, 0x70, 0x12);
    api_anim_draw_image(3, D_21CC72C, 0x10, 0x82, 0x15);
    api_anim_draw_image(4, D_21CC72C, 0x0, 0x44, 0x17);
    api_anim_draw_image(5, D_21CC72C, 0x0, 0x25, 0x12);
    api_anim_draw_image(6, D_21CC72C, 0x10, 0x6c, 0x24);
    api_anim_draw_image(7, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(8, D_21CC72C, 0x10, 0x75, 0x31);
    api_anim_draw_image(9, D_21CC72C, 0x0, 0x26, 0x1e);
    api_anim_draw_image(10, D_21CC72C, 0x0, 0x5, 0x20);
    api_anim_draw_image(11, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(12, D_21CC72C, 0x0, 0x1a, 0x27);
    api_anim_draw_image(13, D_21CC72C, 0x0, 0x2c, 0x34);
    api_anim_draw_image(14, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_draw_image(15, D_21CC72C, 0x0, 0x21, 0x40);
    api_anim_draw_image(16, D_21CC72C, 0x0, 0xa, 0x4a);
    api_anim_draw_image(17, D_21CCF44, 0x0, 0x49, 0x3e);
    api_anim_draw_image(18, D_21CA050, 0x10, 0x6f, 0x38);
    api_anim_draw_image(19, D_21CAE18, 0x10, 0x6f, 0x36);
    api_anim_draw_image(20, api_picture_address(0x16), 0x10, 0x52, 0x2b);
    api_anim_queue_sound(0x1e);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CCF44, 0x0, 0x45, 0x3);
    api_anim_draw_image(2, D_21CC72C, 0x0, 0xb, 0xe);
    api_anim_draw_image(3, D_21CC72C, 0x10, 0x70, 0x12);
    api_anim_draw_image(4, D_21CC72C, 0x10, 0x82, 0x15);
    api_anim_draw_image(5, D_21CC72C, 0x0, 0x44, 0x17);
    api_anim_draw_image(6, D_21CC72C, 0x0, 0x25, 0x12);
    api_anim_draw_image(7, D_21CC72C, 0x10, 0x6c, 0x24);
    api_anim_draw_image(8, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(9, D_21CC72C, 0x10, 0x75, 0x31);
    api_anim_draw_image(10, D_21CC72C, 0x0, 0x26, 0x1e);
    api_anim_draw_image(11, D_21CC72C, 0x0, 0x5, 0x20);
    api_anim_draw_image(12, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(13, D_21CC72C, 0x0, 0x1a, 0x27);
    api_anim_draw_image(14, D_21CC72C, 0x0, 0x2c, 0x34);
    api_anim_draw_image(15, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_draw_image(16, D_21CC72C, 0x0, 0x21, 0x40);
    api_anim_draw_image(17, D_21CC72C, 0x0, 0xa, 0x4a);
    api_anim_draw_image(18, D_21CC72C, 0x0, 0x49, 0x3e);
    api_anim_draw_image(19, D_21CCF44, 0x0, 0x68, 0x46);
    api_anim_draw_image(20, D_21CA050, 0x10, 0x81, 0x42);
    api_anim_draw_image(21, D_21CAE18, 0x10, 0x81, 0x40);
    api_anim_draw_image(22, api_picture_address(0x16), 0x10, 0x64, 0x35);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CC72C, 0x0, 0x45, 0x3);
    api_anim_draw_image(2, D_21CC72C, 0x0, 0xb, 0xe);
    api_anim_draw_image(3, D_21CC72C, 0x10, 0x70, 0x12);
    api_anim_draw_image(4, D_21CC72C, 0x0, 0x25, 0x12);
    api_anim_draw_image(5, D_21CC72C, 0x10, 0x82, 0x15);
    api_anim_draw_image(6, D_21CC72C, 0x0, 0x44, 0x17);
    api_anim_draw_image(7, D_21CC72C, 0x10, 0x6c, 0x24);
    api_anim_draw_image(8, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(9, D_21CC72C, 0x10, 0x75, 0x31);
    api_anim_draw_image(10, D_21CC72C, 0x0, 0x26, 0x1e);
    api_anim_draw_image(11, D_21CC72C, 0x0, 0x5, 0x20);
    api_anim_draw_image(12, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(13, D_21CC72C, 0x0, 0x1a, 0x27);
    api_anim_draw_image(14, D_21CC72C, 0x0, 0x2c, 0x34);
    api_anim_draw_image(15, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_draw_image(16, D_21CC72C, 0x0, 0x21, 0x40);
    api_anim_draw_image(17, D_21CC72C, 0x0, 0x49, 0x3e);
    api_anim_draw_image(18, D_21CC72C, 0x0, 0x68, 0x46);
    api_anim_draw_image(19, D_21CC72C, 0x0, 0xa, 0x4a);
    api_anim_draw_image(20, (u8 *)0x0, 0x0, 0x0, 0);
    api_anim_draw_image(21, (u8 *)0x0, 0x0, 0x0, 0);
    api_anim_draw_image(22, (u8 *)0x0, 0x0, 0x0, 0);
    api_anim_queue_sound(0x1e);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CCF44, 0x0, 0x45, 0x3);
    api_anim_draw_image(2, D_21CCF44, 0x0, 0xb, 0xe);
    api_anim_draw_image(3, D_21CCF44, 0x10, 0x70, 0x12);
    api_anim_draw_image(4, D_21CCF44, 0x0, 0x25, 0x12);
    api_anim_draw_image(5, D_21CCF44, 0x10, 0x82, 0x15);
    api_anim_draw_image(6, D_21CCF44, 0x0, 0x44, 0x17);
    api_anim_draw_image(7, D_21CCF44, 0x10, 0x6c, 0x24);
    api_anim_draw_image(8, D_21CCF44, 0x10, 0x8a, 0x28);
    api_anim_draw_image(9, D_21CCF44, 0x10, 0x75, 0x31);
    api_anim_draw_image(10, D_21CCF44, 0x0, 0x26, 0x1e);
    api_anim_draw_image(11, D_21CCF44, 0x0, 0x5, 0x20);
    api_anim_draw_image(12, D_21CCF44, 0x0, 0x33, 0x2d);
    api_anim_draw_image(13, D_21CCF44, 0x0, 0x1a, 0x27);
    api_anim_draw_image(14, D_21CCF44, 0x0, 0x2c, 0x34);
    api_anim_draw_image(15, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_draw_image(16, D_21CCF44, 0x0, 0x21, 0x40);
    api_anim_draw_image(17, D_21CCF44, 0x0, 0x49, 0x3e);
    api_anim_draw_image(18, D_21CCF44, 0x0, 0x68, 0x46);
    api_anim_draw_image(19, D_21CCF44, 0x0, 0xa, 0x4a);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21C8E2C, 0x0, 0x40, 0x24);
    api_anim_draw_image(1, D_21CC72C, 0x0, 0x45, 0x3);
    api_anim_draw_image(2, D_21CC72C, 0x0, 0xb, 0xe);
    api_anim_draw_image(3, D_21CC72C, 0x10, 0x70, 0x12);
    api_anim_draw_image(4, D_21CC72C, 0x0, 0x25, 0x12);
    api_anim_draw_image(5, D_21CC72C, 0x10, 0x82, 0x15);
    api_anim_draw_image(6, D_21CC72C, 0x0, 0x44, 0x17);
    api_anim_draw_image(7, D_21CC72C, 0x10, 0x6c, 0x24);
    api_anim_draw_image(8, D_21CC72C, 0x10, 0x8a, 0x28);
    api_anim_draw_image(9, D_21CC72C, 0x10, 0x75, 0x31);
    api_anim_draw_image(10, D_21CC72C, 0x0, 0x26, 0x1e);
    api_anim_draw_image(11, D_21CC72C, 0x0, 0x5, 0x20);
    api_anim_draw_image(12, D_21CC72C, 0x0, 0x33, 0x2d);
    api_anim_draw_image(13, D_21CC72C, 0x0, 0x1a, 0x27);
    api_anim_draw_image(14, D_21CC72C, 0x0, 0x2c, 0x34);
    api_anim_draw_image(15, D_21CC29C, 0x0, 0x40, 0x41);
    api_anim_draw_image(16, D_21CC72C, 0x0, 0x21, 0x40);
    api_anim_draw_image(17, D_21CC72C, 0x0, 0x49, 0x3e);
    api_anim_draw_image(18, D_21CC72C, 0x0, 0x68, 0x46);
    api_anim_draw_image(19, D_21CC72C, 0x0, 0xa, 0x4a);
    api_anim_frame(500, 0);
}

/*
 * Quiz setup: answer order game_quiz_answer_order[0..2] = 0, 1, 2 (0 = the correct one; the asm
 * game_quiz_question_screen draws the question screen with the three answers from the table
 * game_quiz_answer_tbl[answer][game_quiz_current] and reads the choice into game_quiz_choice), and n distinct random
 * question numbers 0..99 into game_quiz_questions (only n <= 1 is used).
 */
void game_quiz_setup(int n)
{
    int i;
    int j;
    u16 v = 100;

    for (i = 0; i <= 2; i++)
        game_quiz_answer_order[i] = i;
    if (n > 1)
        return;
    game_quiz_correct = 0;
    for (i = 0; i < n; i++) {
    again:
        v = api_rand_except(100, v);
        for (j = 0; j < i; j++) {
            if (game_quiz_questions[j] == v)
                goto again;
        }
        game_quiz_questions[i] = v;
    }
}



extern u8 game_quiz_objs[];






/*
 * Quiz question screen (asm).
 */
u8 game_quiz_question_screen(void)
{
    int i;
    int n;
    u8 h;
    u8 k;

    game_quiz_choice = 0;
    api_new_screen();
    api_object_create_plain(0, 0x1c, 0, 0, D_21D1E10);
    h = api_object_create_plain(6, 0x22, 0x40, 0, D_21D40D4);
    for (i = 0; i <= 2; i++)
        game_quiz_objs[i] = api_object_create_plain(0x26, i * 16 + 0x20, 0x80, 0, 0);
    for (i = 0; i <= 2; i++)
        api_draw_text(game_quiz_objs[i], 0x26, i * 16 + 0x20, 0x80, (int)game_quiz_answer_tbl[game_quiz_answer_order[i]][game_quiz_current], 0, 8);
    k = api_object_create_plain(0x70, 0x54, 0x70, 9, D_2143B6C);
    api_present();
    api_set_btn_a(0);
    api_set_btn_b(0);
    api_set_btn_c(0);
    api_set_scene_busy(1);
    api_timeout_start_ch5(1000);
    n = 0;
    for (;;) {
        if (api_get_btn_a()) {
            if (++game_quiz_choice > 2)
                game_quiz_choice = 0;
            api_object_move_dirty(h, 6, game_quiz_choice * 16 + 0x22);
            api_present();
            api_set_btn_a(0);
        }
        if (api_get_btn_c()) {
            api_set_scene_busy(0);
            return 0;
        }
        if (api_get_btn_b())
            break;
        asm volatile("halt");
        if (api_timeout_elapsed_ch5()) {
            api_timeout_clear_ch5();
            if (++n > 9)
                break;
            api_play_sound(0x14);
            api_object_set_image(k, 0x70, 9 - n, D_2143B6C);
            api_present();
        }
        api_idle_tick();
    }
    api_set_scene_busy(0);
    return 1;
}

/*
 * The quiz about Tamagotchi characters ("キラリっちのいもうとはだれ?" ...): one question
 * (game_quiz_ask(i), asm: shows question game_quiz_questions[i] and takes the answer; non-zero =
 * cancelled); a correct choice (game_quiz_answer_order[chosen] == 0) shows the "correct" picture
 * 0x21D3DF8 with sound 0xE, a wrong one 0x21D3B1C with sound 0x32. Correct: 100 Gotchi
 * points (anim_quiz_points shows, pet_add_points adds), weight (pet_sub_weight), the "got it"
 * animation, correct answer count game_quiz_correct; returns 1, else 0 (anim_quiz_wrong = sad
 * ending), -1 cancelled. Called from the event picker comm_idle_touch_event (the idle touch, see
 * notes/README.md item 8).
 */
int game_quiz(void)
{
    int i, n = 1;

    game_quiz_correct = 0;
    game_quiz_setup(1);
    anim_quiz_start();
    for (i = 0; i < n; i++) {
        if (game_quiz_ask(i) != 0)
            return -1;
        api_new_screen();
        api_object_create_plain(0, 0x1c, 0, 0, D_21D0BE8);
        api_object_create(7, 0x3e, 0x50, 0, 0x10, D_21D38F4);
        if (game_quiz_answer_order[game_quiz_choice] == 0)
            game_quiz_correct++;
        if (game_quiz_answer_order[game_quiz_choice] != 0) {
            game_quiz_obj = api_object_create_plain(0, 0, 0, 0, 0);
            api_object_create_plain(0, 0, 0, 0, 0);
            api_object_create_plain(0, 0, 0, 0, 0);
            api_draw_pet(game_quiz_obj, 0x10, 3, 0, 0x68, 0x30);
            api_object_create_plain(0x1b, 0x25, 0x48, 0, D_21D3DF8);
            api_present();
            api_play_sound(0xe);
            api_delay_ms(1000);
            break;
        }
        game_quiz_obj = api_object_create_plain(0, 0, 0, 0, 0);
        api_object_create_plain(0, 0, 0, 0, 0);
        api_object_create_plain(0, 0, 0, 0, 0);
        api_draw_pet(game_quiz_obj, 0x10, 2, 0, 0x68, 0x30);
        api_object_create_plain(0x1b, 0x25, 0x48, 0, D_21D3B1C);
        api_present();
        api_play_sound(0x32);
        api_delay_ms(1000);
    }
    if (game_quiz_correct != 0) {
        anim_quiz_points(100);
        pet_sub_weight(1);
        pet_add_points(100);
        pet_add_happiness(1);
        anim_quiz_right();
        return 1;
    }
    anim_quiz_wrong();
    return 0;
}


u8 *f_E0A7_25276F8(u8, s16, s16, u16, u8 *, int, int) __asm__("api_draw_text");

/*
 * Quiz: show question i with its three answers and get the choice (asm).
 */
u8 game_quiz_ask(int i)
{
    u8 *s;
    int y;

    api_set_scene_busy(1);
    y = 0x10;
    api_new_screen();
    api_object_create_plain(0, 0x1c, 0, 0, D_21D0BE8);
    api_object_create(7, 0x3e, 0x50, 0, y, D_21D38F4);
    game_quiz_obj = api_object_create_plain(0, 0, 0, 0, 0);
    api_object_create_plain(0, 0, 0, 0, 0);
    api_object_create_plain(0, 0, 0, 0, 0);
    api_draw_pet(game_quiz_obj, y, 0, 0, 0x68, 0x30);
    api_object_create_plain(4, 0x1c, 0x48, 0, D_21D3038);
    game_quiz_objs[0] = api_object_create_plain(0x1c, 0x20, 0x80, 0, 0);
    game_quiz_objs[1] = api_object_create_plain(0xa, 0x2c, 0x80, 0, 0);
    game_quiz_current = game_quiz_questions[i];
    s = (u8 **)game_quiz_question_texts[game_quiz_current];
    api_set_scene_busy(0);
    if (*s != 0) {
        do {
            s = f_E0A7_25276F8(game_quiz_objs[0], 0x1c, 0x20, 0x80, s, 1, 10);
            if (*s != 0 && s[1] == 0xff) {
                s += 2;
                s = f_E0A7_25276F8(game_quiz_objs[1], 0xa, 0x2c, 0x80, s, 1, 12);
            }
            else
                api_object_set_image(game_quiz_objs[1], 0x80, 0, 0);
            api_present();
            api_play_sound(0x15);
            api_set_btn_a(0);
            api_set_btn_b(0);
            api_set_btn_c(0);
            api_timeout_start_ch5(5000);
            for (;;) {
                if (api_get_btn_c()) {
                    api_set_btn_c(0);
                    return 1;
                }
                if (api_get_btn_b())
                    break;
                asm volatile("halt");
                api_idle_tick();
                if (api_timeout_elapsed_ch5())
                    break;
            }
            api_timeout_clear_ch5();
        } while (*s != 0);
    }
    api_shuffle(game_quiz_answer_order, 3);
    api_object_set_image(game_quiz_objs[0], 0, 0, 0);
    api_object_set_image(game_quiz_objs[1], 0, 0, 0);
    if (game_quiz_question_screen() != 0)
        return 0;
    return 1;
}

/* unreferenced: two picture lists */
extern u8 D_21C2058[];
extern u8 D_21C22B0[];
extern u8 D_21C2514[];
extern u8 D_21C2774[];
extern u8 D_21C29D8[];
extern u8 D_21C2C38[];
extern u8 D_21C5524[];
extern u8 D_21C52C4[];
extern u8 D_21C5784[];
const u8 *const D_255FB48[6] = {
    D_21C2058, D_21C22B0, D_21C2514, D_21C2774, D_21C29D8, D_21C2C38,
};
const u8 *const D_255FB60[8] = {
    D_21C2058, D_21C22B0, D_21C2514, D_21C2774, D_21C29D8, D_21C5524, D_21C52C4, D_21C5784,
};
/* per cake order: number of button presses, expected buttons (bit k: 0 = A, 1 = B) */
const u8 D_255FB80[10][2] = {
    { 2, 1 }, { 3, 0 }, { 3, 2 }, { 3, 5 }, { 3, 7 }, { 4, 8 }, { 4, 13 }, { 5, 18 }, { 5, 22 }, { 5, 21 },
};
const u16 game_cake_order_masks[10] = { 0x1f, 0x3c, 0x1f, 0x1f, 0x0f, 0x0f, 0x0f, 0x3c, 0x0f, 0x3c };
void (*const D_255FBA8[10])(void) = {
    game_cake_scene0, game_cake_scene1, game_cake_scene2, game_cake_scene3, game_cake_scene4,
    game_cake_scene5, game_cake_scene6, game_cake_scene7, game_cake_scene8, game_cake_scene9,
};
void (*const D_255FBD0[3][3])(void) = {
    { game_treasure_scene_c, game_treasure_scene_d, game_treasure_scene_e },
    { game_treasure_scene_c, game_treasure_scene_d, game_treasure_scene_e },
    { game_treasure_scene_c, game_treasure_scene_d, game_treasure_scene_e },
};
const u8 D_255FBF4[3][3] = { { 2, 1, 0 }, { 0, 2, 1 }, { 1, 0, 2 } };
const u8 D_255FC00[] = _("どれをきる?");
const u8 D_255FC10[] = _("もう1ぽん!");
const u8 D_255FC20[] = _("キラもっちのくちぐせ\n｢ゲイジュツは?｣");
const u8 D_255FC4C[] = _("ラブリンの\nマネージャーはだれ?");
const u8 D_255FC70[] = _("たまごっつんは\nなんねんにいちどおこる?");
const u8 D_255FC9C[] = _("キラリっちの\nいもうとはだれ?\0");
const u8 D_255FCC0[] = _("ラブリっちの\nきょうだいはだれ?");
const u8 D_255FCE4[] = _("くちぱっちの\nおとうさんはだれ?");
const u8 D_255FD08[] = _("ききっちが\nすんでいるのはどこ?");
const u8 D_255FD2C[] = _("くろまめっちの\nすきなアイドルは?\0");
const u8 D_255FD54[] = _("スペイシーっちの\nこぶんではないのは?\0");
const u8 D_255FD80[] = _("めめっちの\nいもうとはだれ?");
const u8 D_255FDA0[] = _("まめっちの\nママはだれ?");
const u8 D_255FDBC[] = _("キラリっちのすきな\nアクセサリーのかたちは?");
const u8 D_255FDEC[] = _("あまくっちと\nからくっちのかんけいは?\0");
const u8 D_255FE18[] = _("すのぽっちの\nこきょうはなにぽいんと?\0");
const u8 D_255FE44[] = _("まめっちのオンチを\nなおしたのはだれ?\0");
const u8 D_255FE70[] = _("エコうさっちは\nなんにんぐみ?\0");
const u8 D_255FE94[] = _("たまこひめのかぞく\nではないのはだれ?\0");
const u8 D_255FEC0[] = _("ドリたまスクールで\nまめっちのぶかつどうは?");
const u8 D_255FEF0[] = _("わがシエっちが\nひとめぼれしたのはだれ?");
const u8 D_255FF1C[] = _("キラキラガールズは\nどんなグループ?");
const u8 D_255FF44[] = _("まめっちがすきな\nヒーローのなまえは?\0");
const u8 D_255FF70[] = _("めめっちの\n｢ぐるぐる｣ライバルは?");
const u8 D_255FF98[] = _("あんぜんおばっちの\nこどもはだれかな?\0");
const u8 D_255FFC4[] = _("ござるむらしゅっしん\nなのは､つぎのうちだれ?\0");
const u8 D_255FFF8[] = _("みみっちの\nとくいなことは?");
const u8 D_2560018[] = _("ラブリっちは\nなんにんかぞく?\0");
const u8 D_256003C[] = _("ラブリっちのママの\nおみせのなまえは?\0");
const u8 D_2560068[] = _("みらいっちがすきな\nモチーフはどれ?");
const u8 D_2560090[] = _("おてがみをはこんで\nくれるたまごっちは?");
const u8 D_25600BC[] = _("まきこのキライな\nてんきはなに?");
const u8 D_25600E0[] = _("たまごっちせいでは\nちきゅうをなんとよぶ?\0");
const u8 D_2560110[] = _("ぱっちマンがあたまに\nかぶっているのはなに?");
const u8 D_2560140[] = _("ラブパパリっちが\nはたらいているおみせは?\0");
const u8 D_2560170[] = _("ギラギラっちの\nたまペットのなまえは?\0");
const u8 D_256019C[] = _("ラブリンけっせいの\nダンスユニットはなに?\0");
const u8 D_25601CC[] = _("ラブソラっちにできた\nはじめてのたまともは?");
const u8 D_25601FC[] = _("メロディっちの\nおんがくのせんせいは?\0");
const u8 D_2560228[] = _("サンタブルーっちの\nパートナーは?\0");
const u8 D_2560250[] = _("ふらわっちの\nパパのなまえは?\0");
const u8 D_2560274[] = _("どやっちの\nたまペットは?\0");
const u8 D_2560294[] = _("ハピハピっちが\nおすそわけするのはどれ?");
const u8 D_25602C0[] = _("ハーとうにすんでいる\nのはだれ?");
const u8 D_25602E4[] = _("たまリシャスとうに\nいるのはだれ?\0");
const u8 D_256030C[] = _("ちゃまめっちは\nまめっちをなんとよぶ?\0");
const u8 D_2560338[] = _("みらいからきた\nたまごっちは?\0");
const u8 D_256035C[] = _("くちぱっちの\nおうちはどこにある?\0");
const u8 D_2560384[] = _("めめっちの\nしょうらいのゆめは?");
const u8 D_25603A8[] = _("くちぱっちの\nしょうらいのゆめは?\0");
const u8 D_25603D0[] = _("ジ・アースの\nアンテナのかたちは?\0");
const u8 D_25603F8[] = _("ブラックハットの\nぼうしは なににかわる?\0");
const u8 D_2560428[] = _("もりりっちが\nよくいうフレーズは?\0");
const u8 D_2560450[] = _("ぺろっちのママの\nなまえは?");
const u8 D_2560470[] = _("べにっちがニガテな\nはなはなにかな?");
const u8 D_2560498[] = _("5がつうまれの\nたまごっちは?\0");
const u8 D_25604BC[] = _("9がつうまれの\nたまごっちは?\0");
const u8 D_25604E0[] = _("きゃんでぃぱくぱくの\nたんじょうびは､いつ?");
const u8 D_2560510[] = _("たまごっちせいの\nおかねはなに?");
const u8 D_2560534[] = _("ふらわっちのあたまの\nはな､みどりとなにいろ?\0");
const u8 D_2560568[] = _("メロディっちが\nかぶっているのは?\0");
const u8 D_2560590[] = _("くろまめっちの\nスカーフはなにいろ?");
const u8 D_25605B8[] = _("オタくろまめが\nてにもっているのは?");
const u8 D_25605E0[] = _("サロン・ド・ドリーム\nではたらいていないのは?\0");
const u8 D_2560614[] = _("ひめスペっちが\nだいすきなのはだれ?");
const u8 D_256063C[] = _("たまデパのことなら\nなんでもしってるのは?\0");
const u8 D_256066C[] = _("くろまめっちとおなじ\nスカーフをしているのは?\0");
const u8 D_25606A0[] = _("たんじょうびに\nたべるものといったら?\0");
const u8 D_25606CC[] = _("くちぱっちの\nくちぐせはどれかな?\0");
const u8 D_25606F4[] = _("ゴミゼロうんどうの\nごほうびはどれ?");
const u8 D_256071C[] = _("くろまめっちが\nすきなのは?");
const u8 D_256073C[] = _("おれねっちの\nあこがれのセンパイは?");
const u8 D_2560764[] = _("であいストリートで\nあえるのはだれ?");
const u8 D_256078C[] = _("であいストリートで\nできるのは?");
const u8 D_25607B0[] = _("つぎのうち､にわに\nでてもあえないのはだれ?");
const u8 D_25607E0[] = _("まめっちが\nとくいなぶかつは?\0");
const u8 D_2560804[] = _("コフレっちが\nとくいなのはどれかな?");
const u8 D_256082C[] = _("おみあいじょでケーキ\nをつくっているのはだれ?\0");
const u8 D_2560860[] = _("12がつうまれの\nたまごっちはだれ?");
const u8 D_2560888[] = _("チューリップの\nはなことばは?\0");
const u8 D_25608AC[] = _("｢ハーブティ｣の\n｢ハーブ｣のいみは?\0");
const u8 D_25608D8[] = _("つちのなかにできる\nやさいはどれかな?\0");
const u8 D_2560904[] = _("チョコレートの\nざいりょうは?\0");
const u8 D_2560928[] = _("にじのいろに\nないいろはどれ?\0");
const u8 D_256094C[] = _("｢みそ｣の\nげんざいりょうは?\0");
const u8 D_2560970[] = _("でんわを\nはつめいしたのはだれ?");
const u8 D_2560994[] = _("｢ストライプ｣って\nどんなもようのこと?");
const u8 D_25609C0[] = _("｢もものせっく｣に\nかざるのはどれ?");
const u8 D_25609E8[] = _("｢もち｣のはいった\nうどんはなにうどん?");
const u8 D_2560A14[] = _("くうきちゅうに\nいちばんおおいのは?");
const u8 D_2560A3C[] = _("ハロウィンに\nかざるやさいはどれ?\0");
const u8 D_2560A64[] = _("じゅうにせいざに\nないせいざはどれ?");
const u8 D_2560A8C[] = _("ところてんの\nざいりょうはなに?");
const u8 D_2560AB0[] = _("うるうどしは\nなんねんに1かい?");
const u8 D_2560AD4[] = _("おしょうがつにいえの\nまえにかざるものは?\0");
const u8 D_2560B04[] = _("しおひがりでつかう\nどうぐはどれ?\0");
const u8 D_2560B2C[] = _("ボールをつかう\nスポーツはどれかな?");
const u8 D_2560B54[] = _("1せいきは\nなんねん?\0");
const u8 D_2560B70[] = _("｢じょやのかね｣を\nつくかいすうは?");
const u8 D_2560B98[] = _("おおみそかにえんぎを\nかついでたべるのは?\0");
const u8 D_2560BC8[] = _("5がつ5かは\nなんのせっく?");
const u8 D_2560BE8[] = _("3がつ3かは\nなんのせっく?");
/* the quiz questions */
const u8 *const game_quiz_question_texts[100] = {
    D_255FC20, D_255FC4C, D_255FC70, D_255FC9C, D_255FCC0, D_255FCE4,
    D_255FD08, D_255FD2C, D_255FD54, D_255FD80, D_255FDA0, D_255FDBC,
    D_255FDEC, D_255FE18, D_255FE44, D_255FE70, D_255FE94, D_255FEC0,
    D_255FEF0, D_255FF1C, D_255FF44, D_255FF70, D_255FF98, D_255FFC4,
    D_255FFF8, D_2560018, D_256003C, D_2560068, D_2560090, D_25600BC,
    D_25600E0, D_2560110, D_2560140, D_2560170, D_256019C, D_25601CC,
    D_25601FC, D_2560228, D_2560250, D_2560274, D_2560294, D_25602C0,
    D_25602E4, D_256030C, D_2560338, D_256035C, D_2560384, D_25603A8,
    D_25603D0, D_25603F8, D_2560428, D_2560450, D_2560470, D_2560498,
    D_25604BC, D_25604E0, D_2560510, D_2560534, D_2560568, D_2560590,
    D_25605B8, D_25605E0, D_2560614, D_256063C, D_256066C, D_25606A0,
    D_25606CC, D_25606F4, D_256071C, D_256073C, D_2560764, D_256078C,
    D_25607B0, D_25607E0, D_2560804, D_256082C, D_2560860, D_2560888,
    D_25608AC, D_25608D8, D_2560904, D_2560928, D_256094C, D_2560970,
    D_2560994, D_25609C0, D_25609E8, D_2560A14, D_2560A3C, D_2560A64,
    D_2560A8C, D_2560AB0, D_2560AD4, D_2560B04, D_2560B2C, D_2560B54,
    D_2560B70, D_2560B98, D_2560BC8, D_2560BE8,
};
const u8 D_2560D98[] = _("バクハツですの\0");
const u8 D_2560DAC[] = _("まねーねっち");
const u8 D_2560DBC[] = _("1000ねん");
const u8 D_2560DCC[] = _("キラもっち\0");
const u8 D_2560DDC[] = _("ラブソラっち");
const u8 D_2560DEC[] = _("ぱぱぱっち\0");
const u8 D_2560DFC[] = _("セレブリア\0");
const u8 D_2560E0C[] = _("ラブリン");
const u8 D_2560E18[] = _("ひめスペっち");
const u8 D_2560E28[] = _("いもっち");
const u8 D_2560E34[] = _("ままめっち\0");
const u8 D_2560E44[] = _("ほし");
const u8 D_2560E4C[] = _("ふたご\0");
const u8 D_2560E58[] = _("のーすのーす");
const u8 D_2560E68[] = _("パパピアニっち\0");
const u8 D_2560E7C[] = _("3にん\0");
const u8 D_2560E88[] = _("たまひこおうじ\0");
const u8 D_2560E9C[] = _("せいとかいぶ");
const u8 D_2560EAC[] = _("ひめばらっち");
const u8 D_2560EBC[] = _("おんがく");
const u8 D_2560EC8[] = _("ゴッチマン\0");
const u8 D_2560ED8[] = _("まきこ\0");
const u8 D_2560EE4[] = _("うわさっち\0");
const u8 D_2560EF4[] = _("しぐれひめっち\0");
const u8 D_2560F08[] = _("けいさん");
const u8 D_2560F14[] = _("4にん\0");
const u8 D_2560F20[] = _("たまカフェ\0");
const u8 D_2560F30[] = _("ローズ\0");
const u8 D_2560F3C[] = _("なぞっち");
const u8 D_2560F48[] = _("あめ");
const u8 D_2560F50[] = _("ジ・アース\0");
const u8 D_2560F60[] = _("なべ");
const u8 D_2560F68[] = _("しゃしんかん");
const u8 D_2560F78[] = _("ルビーっち\0");
const u8 D_2560F88[] = _("TAMA6\0");
const u8 D_2560F98[] = _("つよっち");
const u8 D_2560FA4[] = _("くらしっくっち\0");
const u8 D_2560FB8[] = _("アオハナっち");
const u8 D_2560FC8[] = _("ぱぱきざっち");
const u8 D_2560FD8[] = _("どやケンっち");
const u8 D_2560FE8[] = _("ハッピー");
const u8 D_2560FF4[] = _("きずなっち\0");
const u8 D_2561004[] = _("ツアコンっち");
const u8 D_2561014[] = _("おにーちゃま");
const u8 D_2561024[] = _("くるるっち\0");
const u8 D_2561034[] = _("ぱっちのもり");
const u8 D_2561044[] = _("びようし");
const u8 D_2561050[] = _("コック\0");
const u8 D_256105C[] = _("ハート\0");
const u8 D_2561068[] = _("ぶき");
const u8 D_2561070[] = _("そんなバナナ");
const u8 D_2561080[] = _("ママコッペっち\0");
const u8 D_2561094[] = _("たまローズ\0");
const u8 D_25610A4[] = _("くちぱっち\0");
const u8 D_25610B4[] = _("ラブリっち\0");
const u8 D_25610C4[] = _("3がつ30にち\0");
const u8 D_25610D8[] = _("ごっちポイント\0");
const u8 D_25610EC[] = _("きいろ\0");
const u8 D_25610F8[] = _("ぼうし\0");
const u8 D_2561104[] = _("ピンク\0");
const u8 D_2561110[] = _("うちわ\0");
const u8 D_256111C[] = _("まじょりて\0");
const u8 D_256112C[] = _("まめっち");
const u8 D_2561138[] = _("デパガっち\0");
const u8 D_2561148[] = _("おれねっち\0");
const u8 D_2561158[] = _("ケーキ\0");
const u8 D_2561164[] = _("だっち\0");
const u8 D_2561170[] = _("エコバッグ\0");
const u8 D_2561180[] = _("サッカー");
const u8 D_256118C[] = _("くろまめっち");
const u8 D_256119C[] = _("キングききっち\0");
const u8 D_25611B0[] = _("デート\0");
const u8 D_25611BC[] = _("パティっち\0");
const u8 D_25611CC[] = _("はつめいぶ\0");
const u8 D_25611DC[] = _("ドリームメイク\0");
const u8 D_25611F0[] = _("わがシエっち");
const u8 D_2561200[] = _("メロディっち");
const u8 D_2561210[] = _("おもいやり\0");
const u8 D_2561220[] = _("くさ");
const u8 D_2561228[] = _("じゃがいも\0");
const u8 D_2561238[] = _("カカオまめ\0");
const u8 D_2561248[] = _("くろ");
const u8 D_2561250[] = _("だいず\0");
const u8 D_256125C[] = _("ベル");
const u8 D_2561264[] = _("しまもよう\0");
const u8 D_2561274[] = _("ひなにんぎょう\0");
const u8 D_2561288[] = _("ちからうどん");
const u8 D_2561298[] = _("ちっそ\0");
const u8 D_25612A4[] = _("カボチャ");
const u8 D_25612B0[] = _("コップざ");
const u8 D_25612BC[] = _("てんぐさ");
const u8 D_25612C8[] = _("4ねん\0");
const u8 D_25612D4[] = _("かどまつ");
const u8 D_25612E0[] = _("くまで\0");
const u8 D_25612EC[] = _("テニス\0");
const u8 D_25612F8[] = _("100ねん\0");
const u8 D_2561308[] = _("108かい\0");
const u8 D_2561318[] = _("そば");
const u8 D_2561320[] = _("たんご\0");
const u8 D_256132C[] = _("もも");
/* the answer choices: game_quiz_answer_tbl[k][question], k = 0..2 */
const u8 *const game_quiz_answers_a[100] = {
    D_2560D98, D_2560DAC, D_2560DBC, D_2560DCC, D_2560DDC, D_2560DEC,
    D_2560DFC, D_2560E0C, D_2560E18, D_2560E28, D_2560E34, D_2560E44,
    D_2560E4C, D_2560E58, D_2560E68, D_2560E7C, D_2560E88, D_2560E9C,
    D_2560EAC, D_2560EBC, D_2560EC8, D_2560ED8, D_2560EE4, D_2560EF4,
    D_2560F08, D_2560F14, D_2560F20, D_2560F30, D_2560F3C, D_2560F48,
    D_2560F50, D_2560F60, D_2560F68, D_2560F78, D_2560F88, D_2560F98,
    D_2560FA4, D_2560FB8, D_2560FC8, D_2560FD8, D_2560FE8, D_2560FF4,
    D_2561004, D_2561014, D_2561024, D_2561034, D_2561044, D_2561050,
    D_256105C, D_2561068, D_2561070, D_2561080, D_2561094, D_25610A4,
    D_25610B4, D_25610C4, D_25610D8, D_25610EC, D_25610F8, D_2561104,
    D_2561110, D_256111C, D_256112C, D_2561138, D_2561148, D_2561158,
    D_2561164, D_2561170, D_2561180, D_256118C, D_256119C, D_25611B0,
    D_25611BC, D_25611CC, D_25611DC, D_25611F0, D_2561200, D_2561210,
    D_2561220, D_2561228, D_2561238, D_2561248, D_2561250, D_256125C,
    D_2561264, D_2561274, D_2561288, D_2561298, D_25612A4, D_25612B0,
    D_25612BC, D_25612C8, D_25612D4, D_25612E0, D_25612EC, D_25612F8,
    D_2561308, D_2561318, D_2561320, D_256132C,
};
const u8 D_25614C4[] = _("ラブですの\0");
const u8 D_25614D4[] = _("ADっち");
const u8 D_25614E0[] = _("デコレっち\0");
const u8 D_25614F0[] = _("ピアニっち\0");
const u8 D_2561500[] = _("ぱぱぱぱっち");
const u8 D_2561510[] = _("たまデパ");
const u8 D_256151C[] = _("マドンナっち");
const u8 D_256152C[] = _("あかスペっち");
const u8 D_256153C[] = _("まままめっち");
const u8 D_256154C[] = _("いとこ\0");
const u8 D_2561558[] = _("さうすさうす");
const u8 D_2561568[] = _("ママピアニっち\0");
const u8 D_256157C[] = _("ごっちだいおう\0");
const u8 D_2561590[] = _("けんだまぶ\0");
const u8 D_25615A0[] = _("おわらい");
const u8 D_25615AC[] = _("ぱっちマン\0");
const u8 D_25615BC[] = _("もりりっち\0");
const u8 D_25615CC[] = _("りょうり");
const u8 D_25615D8[] = _("7にん\0");
const u8 D_25615E4[] = _("♪カフェ♪\0");
const u8 D_25615F4[] = _("クラウン");
const u8 D_2561600[] = _("やぎっち");
const u8 D_256160C[] = _("はれ");
const u8 D_2561614[] = _("ランド・ランド\0");
const u8 D_2561628[] = _("バケツ\0");
const u8 D_2561634[] = _("おかしやさん");
const u8 D_2561644[] = _("ばぐばぐっち");
const u8 D_2561654[] = _("どりぼんず\0");
const u8 D_2561664[] = _("ちゃまめっち");
const u8 D_2561674[] = _("アカハナっち");
const u8 D_2561684[] = _("ぱぱふらわっち\0");
const u8 D_2561698[] = _("どやワンっち");
const u8 D_25616A8[] = _("ラッキー");
const u8 D_25616B4[] = _("ゆめキャンっち\0");
const u8 D_25616C8[] = _("エアコンっち");
const u8 D_25616D8[] = _("おにーちゃん");
const u8 D_25616E8[] = _("ぱっちのたに");
const u8 D_25616F8[] = _("レースクイーン\0");
const u8 D_256170C[] = _("ゴールキーパー\0");
const u8 D_2561720[] = _("ぐるぐる");
const u8 D_256172C[] = _("はね");
const u8 D_2561734[] = _("そんなパイン");
const u8 D_2561744[] = _("さんどいっち");
const u8 D_2561754[] = _("たまヒマワリ");
const u8 D_2561764[] = _("2がつ28にち\0");
const u8 D_2561778[] = _("ごっちマネー");
const u8 D_2561788[] = _("あか");
const u8 D_2561790[] = _("ヘルメット\0");
const u8 D_25617A0[] = _("あくしゅけん");
const u8 D_25617B0[] = _("サイケっち\0");
const u8 D_25617C0[] = _("スペイシーっち\0");
const u8 D_25617D4[] = _("とんかっち\0");
const u8 D_25617E4[] = _("ライトっち\0");
const u8 D_25617F4[] = _("ちとせあめ\0");
const u8 D_2561804[] = _("でーす\0");
const u8 D_2561810[] = _("エコカップ\0");
const u8 D_2561820[] = _("しょうぎ");
const u8 D_256182C[] = _("ろぼっち");
const u8 D_2561838[] = _("アルバイト\0");
const u8 D_2561848[] = _("おやじっち\0");
const u8 D_2561858[] = _("りょうりぶ\0");
const u8 D_2561868[] = _("ドリームダンス\0");
const u8 D_256187C[] = _("もなかっち\0");
const u8 D_256188C[] = _("おもわせぶり");
const u8 D_256189C[] = _("みず");
const u8 D_25618A4[] = _("トマト\0");
const u8 D_25618B0[] = _("えだまめ");
const u8 D_25618BC[] = _("レタス\0");
const u8 D_25618C8[] = _("ワット\0");
const u8 D_25618D4[] = _("みずたま");
const u8 D_25618E0[] = _("こいのぼり\0");
const u8 D_25618F0[] = _("たぬきうどん");
const u8 D_2561900[] = _("さんそ\0");
const u8 D_256190C[] = _("みずがめざ\0");
const u8 D_256191C[] = _("ゼラチン");
const u8 D_2561928[] = _("10ねん");
const u8 D_2561934[] = _("かまくら");
const u8 D_2561940[] = _("まごのて");
const u8 D_256194C[] = _("フェンシング");
const u8 D_256195C[] = _("359かい\0");
const u8 D_256196C[] = _("おせち\0");
const u8 D_2561978[] = _("だんご\0");
const u8 D_2561984[] = _("うめ");
const u8 *const game_quiz_answers_b[100] = {
    D_25614C4, D_25614D4, D_25612F8, D_25614E0, D_25614F0, D_2561500,
    D_2561510, D_256151C, D_256152C, D_2560ED8, D_256153C, D_256105C,
    D_256154C, D_2561558, D_2561568, D_2560F14, D_256157C, D_2561590,
    D_25610B4, D_25615A0, D_25615AC, D_2561200, D_25615BC, D_2560EAC,
    D_25615CC, D_25615D8, D_25615E4, D_25615F4, D_2561600, D_256160C,
    D_2561614, D_2561628, D_2561634, D_2561644, D_2561654, D_2561664,
    D_2560E68, D_2561674, D_2561684, D_2561698, D_25616A8, D_25616B4,
    D_25616C8, D_25616D8, D_2560E18, D_25616E8, D_25616F8, D_256170C,
    D_2561720, D_256172C, D_2561734, D_2561744, D_2561754, D_256112C,
    D_2561200, D_2561764, D_2561778, D_2561788, D_2561790, D_2561248,
    D_25617A0, D_25617B0, D_25617C0, D_25617D4, D_25617E4, D_25617F4,
    D_2561804, D_2561810, D_2561820, D_25617C0, D_256182C, D_2561838,
    D_2561848, D_2561858, D_2561868, D_256187C, D_2560E18, D_256188C,
    D_256189C, D_25618A4, D_25618B0, D_2561788, D_25618BC, D_25618C8,
    D_25618D4, D_25618E0, D_25618F0, D_2561900, D_25618A4, D_256190C,
    D_256191C, D_2561928, D_2561934, D_2561940, D_256194C, D_2560DBC,
    D_256195C, D_256196C, D_2561978, D_2561984,
};
const u8 D_2561B1C[] = _("ハートですの");
const u8 D_2561B2C[] = _("500ねん\0");
const u8 D_2561B3C[] = _("わたわたっち");
const u8 D_2561B4C[] = _("くちぱぱっち");
const u8 D_2561B5C[] = _("がっこう");
const u8 D_2561B68[] = _("ゆめみっち\0");
const u8 D_2561B78[] = _("ピポスペっち");
const u8 D_2561B88[] = _("まっち\0");
const u8 D_2561B94[] = _("おんぷ\0");
const u8 D_2561BA0[] = _("おやこ\0");
const u8 D_2561BAC[] = _("すのーすのー");
const u8 D_2561BBC[] = _("5にん\0");
const u8 D_2561BC8[] = _("たまここひめ");
const u8 D_2561BD8[] = _("ロボットぶ\0");
const u8 D_2561BE8[] = _("めめっち");
const u8 D_2561BF4[] = _("てじな\0");
const u8 D_2561C00[] = _("ヒーローっち");
const u8 D_2561C10[] = _("なちゅらっち");
const u8 D_2561C20[] = _("えいご\0");
const u8 D_2561C2C[] = _("ラブコーヒー");
const u8 D_2561C3C[] = _("ドクロ\0");
const u8 D_2561C48[] = _("ポストっち\0");
const u8 D_2561C58[] = _("くもり\0");
const u8 D_2561C64[] = _("ザ・ちきゅう");
const u8 D_2561C74[] = _("タライ\0");
const u8 D_2561C80[] = _("はくぶつかん");
const u8 D_2561C90[] = _("ふりらぶっち");
const u8 D_2561CA0[] = _("レインボーイズ\0");
const u8 D_2561CB4[] = _("ママコフレっち\0");
const u8 D_2561CC8[] = _("キイロハナっち\0");
const u8 D_2561CDC[] = _("ふらわーパパ");
const u8 D_2561CEC[] = _("どやニャンっち\0");
const u8 D_2561D00[] = _("クッキー");
const u8 D_2561D0C[] = _("とこなっち\0");
const u8 D_2561D1C[] = _("パソコンっち");
const u8 D_2561D2C[] = _("あにじゃ");
const u8 D_2561D38[] = _("くちぱヒルズ");
const u8 D_2561D48[] = _("トリマー");
const u8 D_2561D54[] = _("スタイリスト");
const u8 D_2561D64[] = _("メガネ\0");
const u8 D_2561D70[] = _("みんなバナナ");
const u8 D_2561D80[] = _("ジャムママっち\0");
const u8 D_2561D94[] = _("たまデイジー");
const u8 D_2561DA4[] = _("5がつ28にち\0");
const u8 D_2561DB8[] = _("ごっちコイン");
const u8 D_2561DC8[] = _("あお");
const u8 D_2561DD0[] = _("ずきん\0");
const u8 D_2561DDC[] = _("メガホン");
const u8 D_2561DE8[] = _("シックっち\0");
const u8 D_2561DF8[] = _("すけろくずし");
const u8 D_2561E08[] = _("なのだよ");
const u8 D_2561E14[] = _("エコノート\0");
const u8 D_2561E24[] = _("たべあるき\0");
const u8 D_2561E34[] = _("シュートっち");
const u8 D_2561E44[] = _("ぐるめっち\0");
const u8 D_2561E54[] = _("ドリフト");
const u8 D_2561E60[] = _("ファッションぶ\0");
const u8 D_2561E74[] = _("ゆめしばい\0");
const u8 D_2561E84[] = _("チャラっち\0");
const u8 D_2561E94[] = _("たんぱく");
const u8 D_2561EA0[] = _("つち");
const u8 D_2561EA8[] = _("きゅうり");
const u8 D_2561EB4[] = _("ピーナッツ\0");
const u8 D_2561EC4[] = _("みどり\0");
const u8 D_2561ED0[] = _("だいこん");
const u8 D_2561EDC[] = _("ひらがげんない\0");
const u8 D_2561EF0[] = _("かがみもち\0");
const u8 D_2561F00[] = _("きつねうどん");
const u8 D_2561F10[] = _("すいそ\0");
const u8 D_2561F1C[] = _("ナス");
const u8 D_2561F24[] = _("てんびんざ\0");
const u8 D_2561F34[] = _("ぎゅうにゅう");
const u8 D_2561F44[] = _("2ねん\0");
const u8 D_2561F50[] = _("ねこのて");
const u8 D_2561F5C[] = _("バドミントン");
const u8 D_2561F6C[] = _("753かい\0");
const u8 D_2561F7C[] = _("チャーハン\0");
const u8 D_2561F8C[] = _("さんご\0");
const u8 D_2561F98[] = _("さくら\0");
const u8 *const game_quiz_answers_c[100] = {
    D_2561B1C, D_256118C, D_2561B2C, D_2560E28, D_2561B3C, D_2561B4C,
    D_2561B5C, D_2561B68, D_2561B78, D_2561664, D_2561B88, D_2561B94,
    D_2561BA0, D_2561BAC, D_25614F0, D_2561BBC, D_2561BC8, D_2561BD8,
    D_2561BE8, D_2561BF4, D_2561C00, D_25610B4, D_2561C10, D_2560E18,
    D_2561C20, D_2561BBC, D_2561C2C, D_2561C3C, D_2561C48, D_2561C58,
    D_2561C64, D_2561C74, D_2561C80, D_2561C90, D_2561CA0, D_2560DCC,
    D_2561CB4, D_2561CC8, D_2561CDC, D_2561CEC, D_2561D00, D_2561D0C,
    D_2561D1C, D_2561D2C, D_25614F0, D_2561D38, D_2561D48, D_2561D54,
    D_2560E44, D_2561D64, D_2561D70, D_2561D80, D_2561D94, D_2561BE8,
    D_2560E18, D_2561DA4, D_2561DB8, D_2561DC8, D_2561DD0, D_2561DC8,
    D_2561DDC, D_2561DE8, D_2560DDC, D_2561848, D_25617C0, D_2561DF8,
    D_2561E08, D_2561E14, D_2561E24, D_2561E34, D_2561E44, D_2561E54,
    D_2560F3C, D_2561E60, D_2561E74, D_2561E84, D_256112C, D_2561E94,
    D_2561EA0, D_2561EA8, D_2561EB4, D_2561EC4, D_2561ED0, D_2561EDC,
    D_2561720, D_2561EF0, D_2561F00, D_2561F10, D_2561F1C, D_2561F24,
    D_2561F34, D_2561F44, D_2561EF0, D_2561F50, D_2561F5C, D_2561928,
    D_2561F6C, D_2561F7C, D_2561F8C, D_2561F98,
};
const u8 *const *const game_quiz_answer_tbl[3] = { game_quiz_answers_a, game_quiz_answers_b, game_quiz_answers_c };

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 game_slot_score DSTRAM;
u8 game_slot_frame_objs[3] DSTRAM;
u8 game_slot_round_obj DSTRAM;
u8 game_slot_reel_objs[3] DSTRAM;
u8 game_slot_level_obj DSTRAM;
u8 game_slot_reel_pos[3] DSTRAM;
u8 game_slot_reels_stopped DSTRAM;
u8 game_slot_reel_state[3] DSTRAM;
u8 game_slot_reel_speed DSTRAM;
u8 game_slot_round DSTRAM;
u8 game_cake_level DSTRAM;
u8 game_cake_choices[12] DSTRAM;
u8 game_treasure_finds DSTRAM;
u8 game_treasure_text_obj DSTRAM;
u8 game_quiz_obj DSTRAM;
u8 game_quiz_objs[4] DSTRAM;
u8 game_quiz_answer_order[4] DSTRAM;
u8 game_quiz_questions[1] DSTRAM;
u8 game_quiz_correct DSTRAM;
u8 game_quiz_current DSTRAM;
u32 game_quiz_choice DSTRAM;
/* per round 0..2: the reel pictures and their count */
struct SlotRound game_slot_round_tbl[3] = {
    { (u32)D_255FB48, 6 }, { (u32)D_255FB60, 8 }, { (u32)D_255FB60, 8 },
};
