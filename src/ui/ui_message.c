/*
 * The scrolling message dispatcher: ui_show_message picks the message scene for each message
 * kind (visitors, town places, care, communication...) and plays it with a click on A/B; also
 * the merchant and training message scenes.
 *
 * Code 24FC4F4..24FE038, .rodata from 2538F24.
 */
#include "proto/ui/ui_message.h"
#include "proto/anim/anim_engine.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"

/*
 * F8b = the SPEECH-BUBBLE driver (ui_show_message below).
 *
 * Button A or B pressed during a message? Plays its click (sound 1 / 2) unless btn_click_played
 * is set, and returns 1.
 */
u8 ui_bubble_button_pressed(void)
{
    u8 r = 0;

    if (btn_a_presses != r) {
        if (btn_click_played == r)
            snd_play(1);
        r = 1;
    } else if (btn_b_presses != 0) {
        if (btn_click_played == 0)
            snd_play(2);
        r = 1;
    }
    return r;
}

#define S ((const u8 *)s)
#define WAIT                                \
    for (n = 0; n < 2; n++) {               \
        s += 2;                             \
        if (*s == 0)                        \
            goto end;                       \
    }
#define ANIM_DF1(f1, f2)                                        \
    ui_message_shown = 0;                                              \
    btn_click_played = 0;                                              \
    for (i = 0; i < 1000; i++) {                                \
        if (*s == 0) break;                                     \
        f1(S); if (ui_bubble_button_pressed()) break; anim_keep_ab_presses = 1; WAIT;  \
        ui_message_shown = 1;                                          \
        f1(S); if (ui_bubble_button_pressed()) break; WAIT;                 \
        f2(S); if (ui_bubble_button_pressed()) break; WAIT;                 \
        f2(S); if (ui_bubble_button_pressed()) break; WAIT;                 \
    }                                                           \
    ui_message_shown = 0;                                              \
    break
#define ANIM_DF0(f1, f2)                                        \
    ui_message_frames_kept = 0;                                              \
    for (i = 0; i < 1000; i++) {                                \
        if (*s == 0) break;                                     \
        f1(S); WAIT;                                            \
        ui_message_frames_kept = 2;                                          \
        f1(S); WAIT;                                            \
        f2(S); WAIT;                                            \
        f2(S); WAIT;                                            \
    }                                                           \
    ui_message_frames_kept = 0;                                              \
    break
#define ANIM_PLAIN(f1, f2)                                      \
    for (i = 0; i < 1000; i++) {                                \
        if (*s == 0) break;                                     \
        f1(S); WAIT;                                            \
        f1(S); WAIT;                                            \
        f2(S); WAIT;                                            \
        f2(S); WAIT;                                            \
    }                                                           \
    break
#define ANIM_CHK(f1, f2)                                        \
    btn_click_played = 0;                                              \
    for (i = 0; i < 1000; i++) {                                \
        if (*s == 0) break;                                     \
        f1(S); if (ui_bubble_button_pressed()) break; anim_keep_ab_presses = 1; WAIT;  \
        f1(S); if (ui_bubble_button_pressed()) break; WAIT;                 \
        f2(S); if (ui_bubble_button_pressed()) break; WAIT;                 \
        f2(S); if (ui_bubble_button_pressed()) break; WAIT;                 \
    }                                                           \
    break
#define ANIM_CHK2(f1, f2)                                               \
    btn_click_played = 0;                                                      \
    for (i = 0; i < 1000; i++) {                                        \
        if (*s == 0) break;                                             \
        f1(p, S); if (ui_bubble_button_pressed()) break; anim_keep_ab_presses = 1; WAIT;       \
        f1(p, S); if (ui_bubble_button_pressed()) break; WAIT;                      \
        f2(p, S); if (ui_bubble_button_pressed()) break; WAIT;                      \
        f2(p, S); if (ui_bubble_button_pressed()) break; WAIT;                      \
    }                                                                   \
    break
/*
 * Show a message (text s, optional picture p) inside talking animation a: 37 kinds
 * (0..36), each a pair of generated-zone frame functions (e.g. anim_msg_visitor_a /
 * anim_msg_visitor_b; 0 / 1 the room chat, 13 the party, 0x14.. the town scenes) played f1, f1,
 * f2, f2 in a loop while the text s (game charset, two bytes per character) scrolls through
 * the bubble: every frame advances s by two characters until the terminator; the frames
 * take the current pointer and anim_draw_text renders it. The animation repeats until the
 * text is done. Variants: DF1 / DF0 toggle ui_message_shown / ui_message_frames_kept (blink and mouth state
 * used by the character sprite), CHK stops on a button (ui_bubble_button_pressed = A or B pressed,
 * with the click sound unless btn_click_played). anim_keep_ab_presses = 1 after the first frame ("bubble
 * visible"). Used everywhere with texts from chara_text(0, 100, n).
 */
int ui_show_message(int a, const u8 *p, const u8 *s)
{
    int i;
    int n;

    btn_events = 0;
    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 1;
    anim_keep_ab_presses = 0;
    btn_click_played = 0;
    switch (a) {
    case 0: ANIM_DF1(anim_msg_visitor_a, anim_msg_visitor_b);
    case 1: ANIM_DF1(anim_msg_visitor2_a, anim_msg_visitor2_b);
    case 2: ANIM_DF0(anim_msg_profile_talk_a, anim_msg_profile_talk_b);
    case 3: ANIM_DF1(anim_msg_merchant_a, anim_msg_merchant_b);
    case 4: ANIM_DF0(anim_msg_journey_a, anim_msg_journey_b);
    case 5: ANIM_PLAIN(anim_msg_found_a, anim_msg_found_b);
    case 6: ANIM_DF1(anim_msg_house_a, anim_msg_house_b);
    case 7: ANIM_DF0(anim_msg_pet_a, anim_msg_pet_b);
    case 8: ANIM_DF0(anim_msg_farewell_a, anim_msg_farewell_b);
    case 9: ANIM_DF0(anim_msg_char_birthday_a, anim_msg_char_birthday_b);
    case 10: ANIM_PLAIN(anim_msg_spot_visitor_a, anim_msg_spot_visitor_b);
    case 11: ANIM_PLAIN(anim_msg_comm_a, anim_msg_comm_b);
    case 12: ANIM_PLAIN(anim_msg_phrase_a, anim_msg_phrase_b);
    case 13: ANIM_CHK2(anim_msg_restaurant_a, anim_msg_restaurant_b);
    case 14:
        ui_message_frames_kept = 0;
        btn_click_played = 0;
        for (i = 0; i < 1000; i++) {
            if (*s == 0) break;
            anim_msg_sitter_a(S); if (ui_bubble_button_pressed()) break; anim_keep_ab_presses = 1; WAIT;
            ui_message_frames_kept = 2;
            anim_msg_sitter_a(S); if (ui_bubble_button_pressed()) break; WAIT;
            anim_msg_sitter_b(S); if (ui_bubble_button_pressed()) break; WAIT;
            anim_msg_sitter_b(S); if (ui_bubble_button_pressed()) break; WAIT;
        }
        ui_message_frames_kept = 0;
        break;
    case 15: ANIM_DF0(anim_msg_sitter_report_a, anim_msg_sitter_report_b);
    case 16: ANIM_DF0(anim_msg_sitter_report2_a, anim_msg_sitter_report2_b);
    case 17: ANIM_CHK(anim_msg_shop3_a, anim_msg_shop3_b);
    case 18: ANIM_CHK(anim_msg_shop1_a, anim_msg_shop1_b);
    case 19: ANIM_CHK(anim_msg_game_center_a, anim_msg_game_center_b);
    case 20: ANIM_CHK(anim_msg_matchmaking_a, anim_msg_matchmaking_b);
    case 21: ANIM_CHK(anim_msg_matchmaking_candidate_a, anim_msg_matchmaking_candidate_b);
    case 22: ANIM_CHK(anim_msg_town_guest_a, anim_msg_town_guest_b);
    case 23: ANIM_CHK(anim_msg_relative_a, anim_msg_relative_b);
    case 24: ANIM_CHK(anim_msg_lottery_a, anim_msg_lottery_b);
    case 25: ANIM_CHK(anim_msg_training_a, anim_msg_training_b);
    case 26: ANIM_CHK(anim_msg_school_a, anim_msg_school_b);
    case 27: ANIM_CHK(anim_msg_school2_a, anim_msg_school2_b);
    case 28: ANIM_CHK2(anim_msg_club_a, anim_msg_club_b);
    case 29: ANIM_CHK(anim_msg_tamadepa_a, anim_msg_tamadepa_b);
    case 30: ANIM_DF1(anim_msg_farm_a, anim_msg_farm_b);
    case 31: ANIM_DF1(anim_msg_farm_withered_a, anim_msg_farm_withered_b);
    case 32: ANIM_DF1(anim_msg_street_a, anim_msg_street_b);
    case 33: ANIM_CHK(anim_msg_traveller_a, anim_msg_traveller_b);
    case 34: ANIM_CHK2(anim_msg_kind34_a, anim_msg_kind34_b);
    case 35: ANIM_CHK2(anim_msg_kind35_a, anim_msg_kind35_b);
    case 36: ANIM_CHK2(anim_msg_kind36_a, anim_msg_kind36_b);
    }
end:
    snd_wait_end();
    anim_keep_ab_presses = 0;
    btn_click_played = 0;
    ui_message_frames_kept = 0;
    ui_message_shown = 0;
    btn_events = 0;
}

/*
 * Message animation kind 3 (intro): scene with the pet, a picture and object p.
 */
int anim_msg_merchant_a(int p0)
{
    anim_scene_window(7);
    anim_draw_pic(0, D_219BD80, 0, 0x1e, 0x30);
    anim_draw_pic(1, (u8 *)chara_sprite_merchant_pose2[0], 0x10, 0x37, 0x31);
    anim_draw_pet(2, 2, 0, 0x67, 0x31);
    anim_draw_pic(5, D_21621EC, 0, 0x40, 0xd);
    p0 = anim_draw_text(6, (u8 *)p0, 1, 6, 8);
    anim_frame(0xc8, 3);
    return p0;
}

/*
 * Message animation kind 3 (loop).
 */
int anim_msg_merchant_b(int p0)
{
    anim_scene_window(7);
    anim_draw_pic(0, D_219BD80, 0, 0x1e, 0x30);
    anim_draw_pic(1, (u8 *)chara_sprite_merchant_pose2[0], 0x10, 0x37, 0x32);
    anim_draw_pet(2, 2, 0, 0x67, 0x32);
    anim_draw_pic(5, D_21621EC, 0, 0x40, 0xd);
    p0 = anim_draw_text(6, (u8 *)p0, 1, 6, 8);
    anim_frame(0xc8, 3);
    return p0;
}

/*
 * Message animation (other kind).
 */
int anim_msg_training_a(int p0)
{
    anim_scene_window2(7);
    anim_draw_pic(0, D_21BB4DC, 0, 0x40, 0x24);
    anim_draw_pet(1, 0, 0, 0x56, 0x32);
    anim_draw_pic(4, (u8 *)chara_sprite_coach_pose2[0], 0x10, 0x18, 0x32);
    anim_draw_pic(5, D_216287C, 0x10, 0x40, 0xd);
    p0 = anim_draw_text(6, (u8 *)p0, 1, 6, 8);
    anim_frame(0xc8, 3);
    return p0;
}

/*
 * Message animation (other kind).
 */
int anim_msg_training_b(int p0)
{
    anim_scene_window2(7);
    anim_draw_pic(0, D_21BB4DC, 0, 0x40, 0x24);
    anim_draw_pet(1, 0, 0, 0x56, 0x32);
    anim_draw_pic(4, (u8 *)chara_sprite_coach_pose2[0], 0x10, 0x19, 0x32);
    anim_draw_pic(5, D_216287C, 0x10, 0x40, 0xd);
    p0 = anim_draw_text(6, (u8 *)p0, 1, 6, 8);
    anim_frame(0xc8, 3);
    return p0;
}

/*
 * flash_sector_offsets (in this data block): offsets of the 136 NOR flash sector starts (8 x 8 KB
 * then 127 x 64 KB = 8 MB, bottom-boot device) followed by the end 0x7FFFFF. Sector
 * numbers used by the code are 1-based indices into this table (flash_sector_addr,
 * func_0000A54).
 */
const u32 flash_sector_offsets[136] = {
    0x00000000, 0x00002000, 0x00004000, 0x00006000, 0x00008000, 0x0000a000, 0x0000c000, 0x0000e000,
    0x00010000, 0x00020000, 0x00030000, 0x00040000, 0x00050000, 0x00060000, 0x00070000, 0x00080000,
    0x00090000, 0x000a0000, 0x000b0000, 0x000c0000, 0x000d0000, 0x000e0000, 0x000f0000, 0x00100000,
    0x00110000, 0x00120000, 0x00130000, 0x00140000, 0x00150000, 0x00160000, 0x00170000, 0x00180000,
    0x00190000, 0x001a0000, 0x001b0000, 0x001c0000, 0x001d0000, 0x001e0000, 0x001f0000, 0x00200000,
    0x00210000, 0x00220000, 0x00230000, 0x00240000, 0x00250000, 0x00260000, 0x00270000, 0x00280000,
    0x00290000, 0x002a0000, 0x002b0000, 0x002c0000, 0x002d0000, 0x002e0000, 0x002f0000, 0x00300000,
    0x00310000, 0x00320000, 0x00330000, 0x00340000, 0x00350000, 0x00360000, 0x00370000, 0x00380000,
    0x00390000, 0x003a0000, 0x003b0000, 0x003c0000, 0x003d0000, 0x003e0000, 0x003f0000, 0x00400000,
    0x00410000, 0x00420000, 0x00430000, 0x00440000, 0x00450000, 0x00460000, 0x00470000, 0x00480000,
    0x00490000, 0x004a0000, 0x004b0000, 0x004c0000, 0x004d0000, 0x004e0000, 0x004f0000, 0x00500000,
    0x00510000, 0x00520000, 0x00530000, 0x00540000, 0x00550000, 0x00560000, 0x00570000, 0x00580000,
    0x00590000, 0x005a0000, 0x005b0000, 0x005c0000, 0x005d0000, 0x005e0000, 0x005f0000, 0x00600000,
    0x00610000, 0x00620000, 0x00630000, 0x00640000, 0x00650000, 0x00660000, 0x00670000, 0x00680000,
    0x00690000, 0x006a0000, 0x006b0000, 0x006c0000, 0x006d0000, 0x006e0000, 0x006f0000, 0x00700000,
    0x00710000, 0x00720000, 0x00730000, 0x00740000, 0x00750000, 0x00760000, 0x00770000, 0x00780000,
    0x00790000, 0x007a0000, 0x007b0000, 0x007c0000, 0x007d0000, 0x007e0000, 0x007f0000, 0x007fffff,
};

/*
 * The RAM variables this file defines.
 */
u8 ui_message_frames_kept;
u8 ui_message_shown;
