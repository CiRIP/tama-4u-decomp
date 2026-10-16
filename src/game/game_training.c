/*
 * The sports-ground training mini-game, written against the game API.
 *
 * Code 251C5DC..251D2E4, .rodata from 255FA84.
 */
#include "proto/game/game_training.h"
#include "proto/api/api_entries.h"
#include "flash.h"

/* tables defined further down */
extern const u8 D_255FA88[10][2];

/*
 * F62 = the SPORTS-GROUND training game (called from the park, F28 town_sports_ground): the
 * same button-sequence mechanic as F65 - ten exercise drills gated by the level
 * game_training_level through the masks game_training_masks (game_training_pick picks one), run by game_training_loop
 * (asm); the drill scenes below show the character with its pet performing them (the
 * pet sprites at 0x226C130.., sounds 0x49 / 0x32 / 0x39 / 0xC / 0xE). Written against
 * the game API block (api_anim_draw_image, api_anim_frame, ...), like the other built-in games.
 *
 * Entry: runs game_training_loop at level 0 with anim_busy = 1 (set through the game API
 * api_set_scene_busy: an animation owns the screen, the RTC tick defers its events). Returns
 * the rank (-1 cancelled, 0 fail, 1..4+) that town_sports_ground turns into Gotchi points.
 */
int game_training(void)
{
    int r;

    api_set_scene_busy(1);
    game_training_level = 0;
    r = game_training_loop();
    api_set_scene_busy(0);
    return r;
}

/*
 * Pick a random round among the 10 of table game_training_masks allowed at level game_training_level.
 */
int game_training_pick(void)
{
    int i;
    int n = 0;

    for (i = 0; i <= 9; i++) {
        if ((game_training_masks[i] >> game_training_level) & 1)
            game_training_choices[n++] = i;
    }
    api_shuffle(game_training_choices, n);
    return game_training_choices[0];
}

extern void (*const D_255FAB0[])(void);
/*
 * Mini-game main loop (asm).
 */
int game_training_loop(void)
{
    int k;
    const u8 *p;
    int i;
    int t;
    u8 x;

again:
    api_set_btn_a(0);
    ((void (*)(u8))api_set_btn_b)(0);
    api_set_btn_c(0);
    k = game_training_pick();
    game_training_intro();
    for (;;) {
        if (api_get_btn_a())
            goto fail;
        if (api_get_btn_b())
            goto fail;
        if (api_get_btn_c())
            goto cancel;
        break;
    }
    api_set_btn_a(0);
    api_set_btn_b(0);
    api_set_btn_c(0);
    D_255FAB0[k]();
    i = 0;
    t = D_255FA88[k][0] * 4;
    for (;;) {
        api_timeout_start_ch5(250);
        break;
    }
    p = D_255FA88[k];
    k *= 2;
    for (;;) {
        if ((p[1] >> i) & 1) {
            if (api_get_btn_b()) {
                api_set_btn_b(0);
                i++;
                if (p[0] == i)
                    goto ok;
            }
            x = api_get_btn_a();
        } else {
            if (api_get_btn_a()) {
                api_set_btn_a(0);
                i++;
                if (((const u8 *)D_255FA88)[k] == i)
                    goto ok;
            }
            x = api_get_btn_b();
        }
        if (x)
            goto fail;
        if (api_get_btn_c())
            goto cancel;
        asm volatile("halt");
        if (api_timeout_elapsed_ch5()) {
            api_timeout_clear_ch5();
            if (t <= 0)
                goto fail;
            if (t <= 3)
                api_play_sound(3);
            t--;
        }
        continue;
ok:
        api_set_btn_a(0);
        api_set_btn_b(0);
        api_set_btn_c(0);
        game_training_scene_c();
        if (api_get_btn_c()) {
            api_set_btn_c(0);
            goto quit;
        }
        game_training_level = game_training_level + 1;
        if (game_training_level > 9)
            goto rank;
        api_delay_ms_ch5(1000);
        goto again;
    }
fail:
    api_play_sound(0x11);
    game_training_scene_d();
rank:
    api_delay_ms_ch5(1000);
    if (game_training_level > 9)
        return 5;
    if (game_training_level > 6)
        return 4;
    if (game_training_level > 4)
        return 3;
    if (game_training_level > 2)
        return 2;
    if (game_training_level != 0)
        return 1;
    goto zero;
cancel:
    api_set_btn_c(0);
    goto quit;
quit:
    api_play_sound(3);
    return -1;
zero:
    return 0;
}

/*
 * Mini-game intro animation (API calls; skippable with the buttons in mask 7).
 */
void game_training_intro(void)
{
    api_scene_room(7);
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 0, 0, 0x5f, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D7FC, 0, 0x48, 0x15);
    api_anim_queue_sound(0x49);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0x10, 0x1f, 0x2a);
    api_anim_draw_pet(2, 0, 0x10, 0x62, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D890, 0, 0x38, 0xc);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 0, 0, 0x5f, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D7FC, 0, 0x48, 0x15);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0x10, 0x1f, 0x2a);
    api_anim_draw_pet(2, 0, 0x10, 0x62, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D890, 0, 0x38, 0xc);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 0, 0, 0x5f, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D7FC, 0, 0x48, 0x15);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0x10, 0x1f, 0x2a);
    api_anim_draw_pet(2, 0, 0x10, 0x62, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D890, 0, 0x38, 0xc);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 0, 0, 0x5f, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D7FC, 0, 0x48, 0x15);
    api_anim_frame(500, 7);
end:;
}

/*
 * Mini-game animation (round).
 */
void game_training_scene_a(void)
{
    api_scene_room(10);
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(2, *(u8 * *)chara_sprite_set75_pose8, 0, 0x1f, 0x29);
    api_anim_draw_pet(3, 8, 0, 0x5d, 0x2a);
    api_anim_draw_image(6, D_21BC704, 0, 0x1c, 0xc);
    api_anim_draw_image(7, D_21BCB64, 0, 0x40, 0x3c);
    api_anim_draw_image(8, D_214E930, 0, 0xc, 0x1c);
    api_anim_draw_image(9, D_214E930, 0, 0x49, 0x1c);
    api_anim_queue_sound(0x32);
    api_anim_frame(500, 7);
}

/*
 * Mini-game animation (round).
 */
void game_training_scene_b(void)
{
    api_scene_room(10);
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(2, *(u8 * *)chara_sprite_set75_pose8, 0x10, 0x23, 0x29);
    api_anim_draw_pet(3, 8, 0x10, 0x61, 0x2a);
    api_anim_draw_image(6, D_21BC838, 0, 0x64, 0xc);
    api_anim_draw_image(7, D_21BCB64, 0, 0x40, 0x3c);
    api_anim_draw_image(8, D_214E930, 0x10, 0x36, 0x1a);
    api_anim_draw_image(9, D_214E930, 0x10, 0x74, 0x1a);
    api_anim_queue_sound(0x32);
    api_anim_frame(500, 7);
}

/*
 * Mini-game animation (result).
 */
void game_training_result_a(void)
{
    int y = 0x24;

    api_scene_room(10);
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, y);
    api_anim_draw_image(1, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(2, *(u8 * *)chara_sprite_coach_pose9, 0x10, 0x21, 0x2a);
    api_anim_draw_pet(3, 9, 0x10, 0x5f, 0x2b);
    api_anim_draw_image(6, D_21BC838, 0, 0x64, 0xc);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, y);
    api_anim_draw_image(1, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(2, *(u8 * *)chara_sprite_set75_pose8, 0, 0x1f, 0x29);
    api_anim_draw_pet(3, 8, 0, 0x5d, 0x2a);
    api_anim_draw_image(6, D_21BC704, 0, 0x1c, 0xc);
    api_anim_draw_image(7, D_21BCB64, 0, 0x40, 0x3c);
    api_anim_draw_image(8, D_214E930, 0, 0xc, 0x1c);
    api_anim_draw_image(9, D_214E930, 0, 0x49, 0x1c);
    api_anim_queue_sound(0x32);
    api_anim_frame(500, 7);
}

/*
 * Mini-game animation (result).
 */
void game_training_result_b(void)
{
    int y = 0x24;

    api_scene_room(10);
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, y);
    api_anim_draw_image(1, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(2, *(u8 * *)chara_sprite_coach_pose9, 0, 0x21, 0x2a);
    api_anim_draw_pet(3, 9, 0, 0x5f, 0x2b);
    api_anim_draw_image(6, D_21BC704, 0, 0x1c, 0xc);
    api_anim_frame(500, 0);
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, y);
    api_anim_draw_image(1, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(2, *(u8 * *)chara_sprite_set75_pose8, 0x10, 0x23, 0x29);
    api_anim_draw_pet(3, 8, 0x10, 0x61, 0x2a);
    api_anim_draw_image(6, D_21BC838, 0, 0x63, 0xc);
    api_anim_draw_image(7, D_21BCB64, 0, 0x40, 0x3c);
    api_anim_draw_image(8, D_214E930, 0x10, 0x36, 0x1a);
    api_anim_draw_image(9, D_214E930, 0x10, 0x74, 0x1a);
    api_anim_queue_sound(0x32);
    api_anim_frame(500, 7);
}

/*
 * Mini-game animation.
 */
void game_training_scene_c(void)
{
    api_scene_room(7);
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose2, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 2, 0, 0x5f, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D7FC, 0, 0x48, 0x15);
    api_anim_queue_sound(0x39);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose2, 0x10, 0x1f, 0x2a);
    api_anim_draw_pet(2, 2, 0x10, 0x60, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D890, 0, 0x38, 0xc);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose2, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 2, 0, 0x5f, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D7FC, 0, 0x48, 0x15);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose2, 0x10, 0x1f, 0x2a);
    api_anim_draw_pet(2, 2, 0x10, 0x60, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D890, 0, 0x38, 0xc);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose2, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 2, 0, 0x5f, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D7FC, 0, 0x48, 0x15);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose2, 0x10, 0x1f, 0x2a);
    api_anim_draw_pet(2, 2, 0x10, 0x60, 0x2b);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214D890, 0, 0x38, 0xc);
    api_anim_frame(500, 7);
end:;
}

/*
 * Mini-game animation.
 */
void game_training_scene_d(void)
{
    int y = 0x2b;

    api_scene_room(8);
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 3, 0, 0x5f, y);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214DD1C, 0x10, 0xf, 0x12);
    api_anim_draw_image(7, D_214DDA8, 0, 0x4d, 0x16);
    api_anim_queue_sound(0xc);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, y);
    api_anim_draw_pet(2, 3, 0, 0x5f, 0x2a);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214DD1C, 0x10, 0xd, 0x14);
    api_anim_draw_image(7, D_214DE34, 0, 0x4d, 0x16);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 3, 0, 0x5f, y);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214DD1C, 0x10, 0xf, 0x12);
    api_anim_draw_image(7, D_214DDA8, 0, 0x4d, 0x16);
    api_anim_queue_sound(0xe);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, y);
    api_anim_draw_pet(2, 3, 0, 0x5f, 0x2a);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214DD1C, 0x10, 0xd, 0x14);
    api_anim_draw_image(7, D_214DE34, 0, 0x4d, 0x16);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, 0x2a);
    api_anim_draw_pet(2, 3, 0, 0x5f, y);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214DD1C, 0x10, 0xf, 0x12);
    api_anim_draw_image(7, D_214DDA8, 0, 0x4d, 0x16);
    api_anim_queue_sound(0xe);
    if (api_anim_frame(500, 7) != 0)
        goto end;
    api_anim_draw_image(0, D_21BB4DC, 0, 0x40, 0x24);
    api_anim_draw_image(1, *(u8 * *)chara_sprite_coach_pose0, 0, 0x21, y);
    api_anim_draw_pet(2, 3, 0, 0x5f, 0x2a);
    api_anim_draw_image(5, D_21BC96C, 0, 0x40, 0x2e);
    api_anim_draw_image(6, D_214DD1C, 0x10, 0xd, 0x14);
    api_anim_draw_image(7, D_214DE34, 0, 0x4d, 0x16);
    api_anim_frame(500, 7);
end:;
}

/* per drill: number of button presses, expected buttons (bit k: 0 = A, 1 = B) */
const u8 D_255FA88[10][2] = {
    { 1, 0 }, { 1, 1 }, { 2, 1 }, { 2, 2 }, { 1, 0 }, { 1, 1 }, { 2, 1 }, { 2, 2 }, { 1, 0 }, { 1, 1 },
};
const u16 game_training_masks[10] = { 0x00f, 0x00f, 0x3f0, 0x3f0, 0x00f, 0x00f, 0x3f0, 0x3f0, 0x00f, 0x00f };
void (*const D_255FAB0[10])(void) = {
    game_training_scene_a, game_training_scene_b, game_training_result_a, game_training_result_b, game_training_scene_a,
    game_training_scene_b, game_training_result_a, game_training_result_b, game_training_scene_a, game_training_scene_b,
};

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 game_training_level DSTRAM;
u8 game_training_choices[32] DSTRAM;
