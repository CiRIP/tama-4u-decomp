/*
 * The timing mini-game played as a communication game round.
 *
 * Code 251AF66..251C29A, .rodata from 255FA80. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/game/game_timing.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/anim/anim_scripts.h"
#include "proto/comm/comm_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * Run game_timing_run (a built-in mini-game) with the key-click sounds suppressed.
 */
void game_timing(void)
{
    anim_busy = 1;
    game_timing_hits = 0;
    game_timing_run();
    anim_busy = 0;
}

int game_timing_run(void)
{
    u16 ys[4];
    int r;
    int dir;
    u8 round;
    u8 cnt;
    int speed;
    int x0;
    int blink;
    int target;
    s8 pos;
    int b;

    r = -1;
    round = 0;
    do {
        switch (round) {
        case 0:
        case 1:
            target = rand() % 11 + 30;
            speed = 4;
            break;
        case 2:
        case 3:
            pos = 0;
            target = rand() % 11 + 45;
            speed = 8;
            break;
        default:
            target = rand() % 11 + 45;
            speed = 13;
            break;
        }
        pos = 0;
        dir = 1;
        cnt = 0;
        game_timing_scene_intro();
        x0 = (u8)gfx_objects[4].x;
        ys[0] = gfx_objects[6].y;
        ys[1] = gfx_objects[7].y;
        ys[2] = gfx_objects[8].y;
        ys[3] = gfx_objects[9].y;
        blink = 0;
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        tmr_t3_start_timeout(500);
        tmr_t5_start_timeout(target);
        for (;;) {
            if (btn_a_presses != 0 || btn_b_presses != 0) {
                game_timing_scene_marker(pos);
                if (pos > 0x5a) {
                    game_timing_scene_c();
                    game_timing_hits++;
                    break;
                }
                if (pos > 0x26) {
                    game_timing_scene_b();
                    break;
                }
                game_timing_scene_a();
                break;
            }
            if (btn_c_presses != 0) {
                snd_play(3);
                goto out;
            }
            if (tmr_flags & 4) {
                blink = !blink;
                tmr_flags &= ~4;
                if (++cnt > 20) {
                    game_timing_scene_marker(0);
                    game_timing_scene_a();
                    break;
                }
            }
            b = blink != 0;
            gfx_object_move_dirty(6, gfx_objects[6].x, ys[0] - b);
            gfx_object_move_dirty(7, gfx_objects[7].x, ys[1] - b);
            gfx_object_move_dirty(8, gfx_objects[8].x, ys[2] - b);
            gfx_object_move_dirty(9, gfx_objects[9].x, ys[3] - b);
            func_00009A6();
            pos = dir < 0 ? pos - speed : pos + speed;
            if (pos <= 0) {
                dir = 1;
                pos = 0;
            } else if (pos > 0x67) {
                dir = -1;
                pos = 0x68;
            }
            gfx_object_move_dirty(4, x0 + pos, gfx_objects[4].y);
            func_00009A6();
            while (!(tmr_flags & 0x20))
                asm volatile("halt");
            tmr_flags &= ~0x20;
        }
        round++;
    } while (round <= 4);
    tmr_delay_ms_t5(1000);
    if (game_timing_hits > 4)
        r = 3;
    else if (game_timing_hits > 2)
        r = 2;
    else if (game_timing_hits != 0)
        r = 1;
    else
        r = 0;
out:
    T16_CTL3 = 0;
    T16_CTL5 = 0;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return r;
}

/*
 * Animated scene (background 0x21B782C) written with the animation API (scripted
 * keyframes).
 */
void game_timing_scene_intro(void)
{
    anim_scene_room(0xa);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8A4C, 0, 0x40, 0x3f);
    anim_draw_pic(2, D_21B91C8, 0, 0x31, 0xd);
    anim_draw_pic(3, D_21B91C8, 0, 0x56, 0x2f);
    anim_draw_pic(4, D_21B8D6C, 0, 0xc, 0x44);
    anim_draw_pic(5, D_21B92F4, 0, 0x45, 7);
    anim_draw_pet(6, 0, 0, 0x6e, 0x22);
    anim_draw_pic(9, D_21B8D9C, 0x10, 0x78, 0x2d);
    if (anim_frame(0x1f4, 7) != 0)
        return;
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8A4C, 0, 0x40, 0x3f);
    anim_draw_pic(2, D_21B91C8, 0, 0x31, 0xd);
    anim_draw_pic(3, D_21B91C8, 0, 0x56, 0x2f);
    anim_draw_pic(4, D_21B8D6C, 0, 0xc, 0x44);
    anim_draw_pic(5, D_21B92F4, 0, 0x45, 7);
    anim_draw_pet(6, 0, 0, 0x6e, 0x21);
    anim_draw_pic(9, D_21B8D9C, 0x10, 0x78, 0x2c);
    anim_frame(0x1f4, 7);
}

/*
 * Animated scene with an object at x = a + 12 (background 0x21B782C).
 */
void game_timing_scene_marker(u8 a)
{
    anim_scene_room(10);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8A4C, 0, 0x40, 0x3f);
    anim_draw_pic(2, D_21B91C8, 0, 0x31, 0xd);
    anim_draw_pic(3, D_21B91C8, 0, 0x56, 0x2f);
    anim_draw_pic(4, D_21B8D6C, 0, a + 12, 0x44);
    anim_draw_pic(5, D_21B92F4, 0, 0x45, 7);
    anim_draw_pet(6, 0, 0, 0x6e, 0x22);
    anim_draw_pic(9, D_21B8D9C, 0x10, 0x78, 0x2e);
    anim_frame(500, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8A4C, 0, 0x40, 0x3f);
    anim_draw_pic(2, D_21B91C8, 0, 0x31, 0xd);
    anim_draw_pic(3, D_21B91C8, 0, 0x56, 0x2f);
    anim_draw_pic(5, D_21B92F4, 0, 0x45, 7);
    anim_draw_pet(6, 9, 0, 0x67, 0x22);
    anim_draw_pic(9, D_21B8D9C, 0x10, 0x73, 0x2e);
    anim_frame(500, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8A4C, 0, 0x40, 0x3f);
    anim_draw_pic(2, D_21B91C8, 0, 0x31, 0xd);
    anim_draw_pic(3, D_21B91C8, 0, 0x56, 0x2f);
    anim_draw_pic(5, D_21B92F4, 0, 0x45, 7);
    anim_draw_pet(6, 8, 0, 0x62, 0x21);
    anim_draw_pic(9, D_21B8D9C, 0x20, 0x43, 0x2d);
    anim_queue_sound(11);
    anim_frame(500, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8A4C, 0, 0x40, 0x3f);
    anim_draw_pic(2, D_21B91C8, 0, 0x31, 0xd);
    anim_draw_pic(3, D_21B91C8, 0, 0x56, 0x2f);
    anim_draw_pic(5, D_21B92F4, 0, 0x45, 7);
    anim_draw_pet(6, 8, 0, 0x62, 0x22);
    anim_draw_pic(9, D_21B8D9C, 0x10, 0x28, 0x2d);
    anim_frame(500, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8A4C, 0, 0x40, 0x3f);
    anim_draw_pic(2, D_21B91C8, 0, 0x31, 0xd);
    anim_draw_pic(3, D_21B91C8, 0, 0x56, 0x2f);
    anim_draw_pic(5, D_21B92F4, 0, 0x45, 7);
    anim_draw_pet(6, 8, 0, 0x62, 0x22);
    anim_draw_pic(9, D_21B8D9C, 0x10, -2, 0x2d);
    anim_frame(500, 0);
}

/*
 * Animated scene (background 0x21B782C, sound 0x13).
 */
void game_timing_scene_a(void)
{
    anim_scene_room(4);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, 0x7a, 0x2d);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x10, 0x5b, 0x2b);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x20, 0x2d, 0x29);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, -1, 0x28);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, 0x7a, 0x25);
    anim_draw_pic(2, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_draw_pic(3, D_21B8E2C, 0, 0x10, 0xc);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x10, 0x55, 0x20);
    anim_draw_pic(2, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_draw_pic(3, D_21B8E2C, 0, 0x10, 0xc);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x20, 0x3d, 0x1c);
    anim_draw_pic(2, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_draw_pic(3, D_21B8E2C, 0, 0x10, 0xc);
    anim_queue_sound(0xa);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, 0x2e, 0x1d);
    anim_draw_pic(2, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(3, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(2, D_21B8D9C, 0x10, 0x16, 0x1d);
    anim_draw_pic(3, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(2, D_21B8D9C, 0x20, 4, 0x1d);
    anim_draw_pic(3, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_draw_pic(2, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(3, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
}

/*
 * Animated scene (background 0x21B782C, sound 0x13).
 */
void game_timing_scene_b(void)
{
    anim_scene_room(6);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, 0x7a, 0x2d);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x10, 0x5b, 0x2d);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x20, 0x2d, 0x2d);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, -1, 0x2d);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, 0x7a, 0x2d);
    anim_draw_pic(2, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_draw_pic(3, D_21B8E2C, 0, 0x10, 0xc);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x10, 0x5b, 0x2d);
    anim_draw_pic(2, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_draw_pic(3, D_21B8E2C, 0, 0x10, 0xc);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B9094, 0, 0x1e, 0x2c);
    anim_draw_pic(2, D_21B8F60, 0, 0x1e, 0x2c);
    anim_draw_pic(3, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(4, D_21B8D9C, 0x20, 0x27, 0x2d);
    anim_draw_pic(5, D_21B8F60, 0x50, 0x1b, 0x30);
    anim_queue_sound(0x14);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, -1, 0x2d);
    anim_draw_pic(2, D_21B9094, 0, 0x1e, 0x2c);
    anim_draw_pic(3, D_21B8F60, 0x50, 0xe, 0x33);
    anim_draw_pic(4, D_21B9094, 0x50, 0xc, 0x27);
    anim_draw_pic(5, D_21B8E2C, 0, 0x10, 0xc);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B9094, 0, 0x1e, 0x2c);
    anim_draw_pic(2, D_21B9094, 0x50, -4, 0x20);
    anim_draw_pic(3, D_21B9094, 0x60, 4, 0x3a);
    anim_draw_pic(4, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(5, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B9094, 0, 0x1e, 0x2c);
    anim_draw_pic(2, D_21B9094, 0x60, -2, 0x3f);
    anim_draw_pic(3, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(4, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B9094, 0, 0x1e, 0x2c);
    anim_draw_pic(2, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(3, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
}

/*
 * Animated scene (background 0x21B782C, sound 0x13).
 */
void game_timing_scene_c(void)
{
    anim_scene_room(6);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, 0x7a, 0x2d);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x10, 0x5b, 0x2d);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x20, 0x2d, 0x2d);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, -1, 0x2d);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0, 0x7a, 0x2d);
    anim_draw_pic(2, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_draw_pic(3, D_21B8E2C, 0, 0x10, 0xc);
    anim_queue_sound(0x13);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8D9C, 0x10, 0x5b, 0x2d);
    anim_draw_pic(2, D_21B8E2C, 0, 0x1e, 0x2c);
    anim_draw_pic(3, D_21B8E2C, 0, 0x10, 0xc);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8F60, 0, 0x1e, 0x2c);
    anim_draw_pic(2, D_21B8D9C, 0x20, 0x26, 0x2c);
    anim_draw_pic(3, D_21B9094, 0, 0x1e, 0x2c);
    anim_draw_pic(4, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(5, D_21B8F60, 0x50, 0x14, 0x32);
    anim_queue_sound(0x14);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8F60, 0x50, 0x18, 0x32);
    anim_draw_pic(2, D_21B8F60, 0x20, 8, 0x30);
    anim_draw_pic(3, D_21B9094, 0x50, 0xf, 0x25);
    anim_draw_pic(4, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(5, D_21B8D9C, 0, -1, 0x2d);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B9094, 0x50, 2, 0x24);
    anim_draw_pic(2, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(3, 0, 0, 0, 0);
    anim_draw_pic(4, 0, 0, 0, 0);
    anim_draw_pic(5, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B9094, 0x50, -6, 0x22);
    anim_draw_pic(2, D_21B8E2C, 0, 0x10, 0xc);
    anim_frame(0x1f4, 0);
    anim_draw_pic(0, D_21B782C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_21B8E2C, 0, 0x10, 0xc);
    anim_draw_pic(2, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
}

/*
 * Mini-game round: intro animations (k), play (game_timing_play), outro; returns the score.
 */
int game_timing_round(u8 k)
{
    int r;

    if (k) {
        anim_comm_game_intro1();
        anim_comm_game_intro2();
    }
    anim_comm_game_round_start();
    r = game_timing_play();
    anim_comm_game_round_end();
    btn_clear();
    return r;
}

/*
 * Result animation by score b (levels at 0x28/0x50/0x78/0x8C).
 */
void game_timing_result_anim(u8 a, int b)
{
    int lvl;
    u16 pts;

    if (b > 0x8b)
        lvl = 4;
    else if (b > 0x77)
        lvl = 3;
    else if (b > 0x4f)
        lvl = 2;
    else if (b > 0x27)
        lvl = 1;
    else
        lvl = 0;
    if (a != 0) {
        anim_comm_game_result();
        switch (lvl) {
        case 3:
        case 4:
            anim_comm_game_result_good();
            anim_comm_game_result_great();
            break;
        case 1:
        case 2:
            anim_comm_game_result_good();
            anim_comm_game_result_fair();
            break;
        default:
            anim_comm_game_result_bad();
            anim_comm_game_result_poor();
            break;
        }
        if (lvl == 0) {
            anim_comm_game_lose();
            anim_sad();
        } else {
            anim_comm_game_win();
        }
    } else {
        if (lvl == 0) {
            anim_result_alt();
            anim_sad();
        } else {
            anim_good_result();
        }
    }
    if (lvl > 0) {
        switch (lvl) {
        case 4:
            pet_add_happiness(2);
            pts = 1000;
            break;
        case 3:
            pet_add_happiness(2);
            pts = 300;
            break;
        case 2:
            pet_add_happiness(2);
            pts = 200;
            break;
        case 1:
            pet_add_happiness(1);
            pts = 50;
            break;
        default:
            pts = 0;
            break;
        }
        anim_points_gain(pts);
        anim_happy();
        pet_add_points(pts);
        save_game();
    }
    btn_clear();
}

/*
 * Timing mini-game: moving targets, button presses scored (5 positions). The value shown
 * with the picture (v) grows with the score: D_2153D68[0] * score / 70, the full x000 from
 * a score of 70 (the firmware's only float arithmetic: soft-float calls into libgcc).
 */
int game_timing_play(void)
{
    u8 k;
    u8 i = 0;
    int t = 0;
    int score = 0;
    float f;
    u8 idx;
    u16 pos[5];
    int off;
    u8 v = 0;

    f = D_2153D68[0];
    f /= 70;
    anim_scene_living(8);
    idx = anim_first_slot();
    anim_draw_pic(0, gfx_bg_closet, 0, 0x40, 0x24);
    anim_draw_pet(1, 4, 0, 0x37, 0x32);
    anim_draw_pic(4, D_2153B6C, 0, 0x59, 8);
    anim_draw_pic(5, (u8 *)D_2153D68, 0, 0x59, 8);
    k = idx + 5;
    gfx_object_set_image_clipped(k, gfx_objects[k].palette_base, 0, (const u8 *)D_2153D68, 0, D_2153D68[1]);
    anim_draw_pic(6, D_214D7FC, 0, 0x1e, 0x1d);
    anim_draw_pic(7, D_214D890, 0, 0x5e, 0x24);
    func_00009A6();
    pos[0] = gfx_objects[idx + 1].x;
    pos[1] = gfx_objects[idx + 2].x;
    pos[2] = gfx_objects[idx + 3].x;
    pos[3] = gfx_objects[idx + 6].x;
    pos[4] = gfx_objects[idx + 7].x;
    anim_busy = 1;
    btn_clear();
    tmr_t5_start_timeout(500);
    while (i < 20) {
        if (tmr_flags & 0x20) {
            t = !t;
            off = t ? 0x17 : 0;
            gfx_object_set_flags(idx + 1, t ? 0x10 : 0);
            gfx_object_set_flags(idx + 2, t ? 0x10 : 0);
            gfx_object_set_flags(idx + 3, t ? 0x10 : 0);
            gfx_object_move_dirty(idx + 1, off + pos[0], gfx_objects[idx + 1].y);
            gfx_object_move_dirty(idx + 2, off + pos[1], gfx_objects[idx + 2].y);
            gfx_object_move_dirty(idx + 3, off + pos[2], gfx_objects[idx + 3].y);
            off = t ? 0x40 : 0;
            gfx_object_move_dirty(idx + 6, off + pos[3], gfx_objects[idx + 6].y);
            gfx_object_move_dirty(idx + 7, pos[4] - off, gfx_objects[idx + 7].y);
            func_00009A6();
            tmr_flags &= ~0x20;
            i++;
        }
        if (btn_a_presses != 0) {
            snd_play(1);
            snd_wait_end();
            score += btn_a_presses;
            {
                v = score < 70 ? f * score : D_2153D68[0];
                gfx_object_set_image_clipped(k, gfx_objects[k].palette_base, 0, (const u8 *)D_2153D68, v, D_2153D68[1]);
            }
            func_00009A6();
            btn_a_presses = 0;
        }
        sys_idle_tick();
    }
    anim_busy = 0;
    btn_clear();
    tmr_delay_ms_t5(500);
    return score;
}

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 game_timing_hits DSTRAM;
