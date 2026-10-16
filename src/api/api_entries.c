/*
 * Entry points for downloaded games and outings (and the built-in API-style games): thin
 * wrappers over buttons, pet stats, clock, timers, sound, display objects, animation scenes and
 * character sprites. The block starts at the fixed address the downloaded programs call (with
 * the libgcc members before it) and ends with api_chara_pose_image.
 *
 * Code 2400B50..2401DA0, .rodata from 2538994.
 */
#include "proto/api/api_entries.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/chara/chara_draw.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/dl/dl_chara.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"
#include "hw.h"

/*
 * Game API: read the button A press count btn_a_presses (used by every downloadable program
 * that calls the firmware).
 */
u8 api_get_btn_a(void)
{
    return btn_a_presses;
}

/*
 * Game API: read the button B press count btn_b_presses.
 */
u8 api_get_btn_b(void)
{
    return btn_b_presses;
}

/*
 * Game API (0x2400AD0..0x2401DA0 = libgcc division + small entry points; the interface of
 * the downloaded games and outings, which call the firmware only here - see F8a and
 * notes/findings/tamago.md): read the button C press count (19 of the 28 downloadable
 * programs that call the firmware use it).
 */
u8 api_get_btn_c(void)
{
    return btn_c_presses;
}

/*
 * Game API: set the button A press count.
 */
void api_set_btn_a(u8 a)
{
    btn_a_presses = a;
}

/*
 * Game API: set the button B press count.
 */
void api_set_btn_b(u8 a)
{
    btn_b_presses = a;
}

/*
 * Game API: set the button C press count.
 */
void api_set_btn_c(u8 a)
{
    btn_c_presses = a;
}

/*
 * Game API: idle housekeeping (sys_idle_housekeeping).
 */
void api_idle_tick(void)
{
    sys_idle_housekeeping();
}

/*
 * Game API: read the button event bits btn_events.
 */
u8 api_get_btn_events(void)
{
    return btn_events;
}

/*
 * Game API: write the button event bits btn_events.
 */
void api_set_btn_events(u8 a)
{
    btn_events = a;
}

/*
 * Game API: random number in [0, a) different from b (util_rand_except).
 */
int api_rand_except(int a, int b)
{
    return util_rand_except(a, b);
}

/*
 * Game API: shuffle the n bytes at p (util_rand_shuffle).
 */
void api_shuffle(u8 *p, int n)
{
    util_rand_shuffle(p, n);
}

/*
 * Game API: pet happiness meter (pet.x00C).
 */
int api_get_happiness(void)
{
    return save.pet.happiness;
}

/*
 * Game API: pet hunger hearts (pet.hunger).
 */
int api_get_hunger(void)
{
    return save.pet.hunger;
}

/*
 * Game API: pet weight (pet.weight).
 */
int api_get_weight(void)
{
    return save.pet.weight;
}

/*
 * Game API: gotchi points (pet.gotchi_points).
 */
int api_get_points(void)
{
    return save.pet.gotchi_points;
}

/*
 * Game API: add a (u8) to the happiness; a == 0 calls the subtract routine with 0, so
 * nothing is ever subtracted.
 */
void api_add_happiness(u8 a)
{
    if (a != 0)
        pet_add_happiness(a);
    else
        pet_sub_happiness(-a);
}

/*
 * Game API: feed a hunger hearts.
 */
void api_add_hunger(u8 a)
{
    if (a != 0)
        pet_feed(a);
    else
        pet_feed(-a);
}

/*
 * Game API: change the weight by a.
 */
void api_add_weight(u8 a)
{
    if (a != 0)
        pet_add_weight(a);
    else
        pet_sub_weight(-a);
}

/*
 * Game API: change the gotchi points by a.
 */
void api_add_points(int a)
{
    if (a != 0)
        pet_add_points(a);
    else
        pet_spend_points(-a);
}

/*
 * Game API: current month (rtc_get_month).
 */
u8 api_get_month(void)
{
    return rtc_get_month();
}

/*
 * Game API: current day (rtc_get_day).
 */
u8 api_get_day(void)
{
    return rtc_get_day();
}

/*
 * Game API: current hour (rtc_get_hour).
 */
u8 api_get_hour(void)
{
    return rtc_get_hour();
}

/*
 * Game API: current minute (rtc_get_minute).
 */
u8 api_get_minute(void)
{
    return rtc_get_minute();
}

/*
 * Game API: delay p0 milliseconds (tmr_delay_ms).
 */
void api_delay_ms(u16 p0)
{
    tmr_delay_ms(p0);
}

/*
 * Game API: tmr_delay_ms_t5(a) (asm, delay variant).
 */
void api_delay_ms_ch5(u16 a)
{
    tmr_delay_ms_t5(a);
}

/*
 * Game API: start a p0 ms timeout on T16 ch.3.
 */
void api_timeout_start(u16 p0)
{
    tmr_t3_start_timeout(p0);
}

/*
 * Game API: start a p0 ms timeout on T16 ch.5.
 */
void api_timeout_start_ch5(u16 p0)
{
    tmr_t5_start_timeout(p0);
}

/*
 * Game API: T16 ch.3 timeout elapsed?
 */
u8 api_timeout_elapsed(void)
{
    return (tmr_flags >> 2) & 1;
}

/*
 * Game API: T16 ch.5 timeout elapsed?
 */
u8 api_timeout_elapsed_ch5(void)
{
    return (tmr_flags >> 5) & 1;
}

/*
 * Game API: clear the T16 ch.3 timeout flag.
 */
void api_timeout_clear(void)
{
    tmr_flags &= ~4;
}

/*
 * Game API: clear the T16 ch.5 timeout flag.
 */
void api_timeout_clear_ch5(void)
{
    tmr_flags &= ~0x20;
}

/*
 * Game API: play short sound p0 (snd_play).
 */
void api_play_sound(u8 p0)
{
    snd_play(p0);
}

/*
 * Game API: wait for the short sound to end.
 */
void api_wait_sound(void)
{
    snd_wait_end();
}

/*
 * Game API: ui_new_screen (screen reset with the icon bars).
 */
void api_new_screen(void)
{
    ui_new_screen();
}

/*
 * Game API: flush the display to the LCD (func_00009A6).
 */
void api_present(void)
{
    func_00009A6();
}

/*
 * Game API: draw the pet (chara_draw_pet).
 */
void api_draw_pet(u8 a, u8 b, u8 c, u16 d, s16 e, s16 f)
{
    chara_draw_pet(a, b, c, d, e, f);
}

/*
 * Game API: set anim_busy (1 = an animation / sub-screen owns the screen: the RTC tick
 * defers its events until it is cleared).
 */
void api_set_scene_busy(u8 a)
{
    anim_busy = a;
}

/*
 * Graphics engine, part 1 (the sprite/graphics engine API), and the "game API" entry
 * points.
 *
 * Screen: 128x128 TFT (colours kept as RGB565, sent to the controller as 18-bit); rows
 * 0..27 and 100..127 are the icon bars, rows 28..99 the 128x72 main area (main-area
 * coordinates add 0x1C to y). Controller init: lcd_init.
 * Display list: gfx_objects[48] objects (see gfx_object_create), gfx_object_count = count. Sprite record
 * struct GfxObject: x00 image, x04/x06 x/y, x08/x09 w/h, x0a attribute (flip or frame:
 * unclear), x0c palette base, x0e flags, x10 row offset. Drawing an object = setting its
 * image/position; every change adds the old/new bounds to a dirty rectangle (IRAM
 * func_0000000: gfx_dirty_main_y0..D_0001808 main area, gfx_dirty_bar_y0..D_000180C icon bars).
 * func_00009A6 (IRAM, "present") composes the dirty rectangles from the display list and
 * sends them to the LCD controller (8-bit bus: 0x600000 command, 0x600001 data), then
 * calls sys_apply_backlight (backlight) and empties both rectangles. Main area: func_000081A
 * composes the rectangle (func_00007A2) into the 128x72 8-bit indexed frame buffer
 * 0x1810..0x3C0F, then sends 2B RASET y0..y1, 2A CASET x0..x1 (16-bit values, high byte
 * 0 from the halfword command store), 2C RAMWR and 3 bytes per pixel (it sends the window
 * even when the rectangle is empty). Icon bars: func_00006B8, called for rows y0..27
 * and/or 100..y1 when the bar rectangle reaches them, composes into the start of the same
 * buffer as scratch and sends the same command sequence (skipped when empty). Window
 * offsets: column +2, row +1 on the ST7735 panel (lcd_is_st7735 = 1), none otherwise. Each
 * pixel goes out as the palette tables' blue (0x3C10), green (0x3D50), red (0x3E90)
 * bytes, top 6 bits used (18-bit colour, the reset pixel format of both controllers); the
 * bars use the second palette set (+0x100) unless lcd_palette_set == 1. Two more IRAM helpers
 * with no caller in the firmware: 0xA3C writes a command byte, 0xA48 a data byte.
 * Every update is such a rectangle (a full repaint is just whole-screen rectangles, see
 * gfx_set_background); no tearing-effect sync (TE), partial or scroll mode is used.
 * Images (assets in flash): u8 width, u8 height, u8 colour count n, u8 0, u8 1, u8 0xFF?,
 * n big-endian RGB565 colours (colour 0 = 0x07E0 green = transparent), then the pixels
 * as 4-bit indices (two per byte) for n <= 16. Palettes of all visible objects are
 * merged into the tables at 0x3C10/0x3D50/0x3E90 (func_0000312, gfx_palette_slot_for), with
 * optional colour effects selected by the object flags (func_000014A).
 * The first part of this file (0x2400CF4..0x2401DA0) is a block of thin wrappers and
 * resource tables (api_picture_address: picture number -> address) used by the built-in
 * mini-games (F62, F65; the generated zone F0 calls only a few, and most entries have no
 * caller). Together with the G_2400B50..G_2400B50 wrappers before it and the libgcc
 * division routines at 0x2400AD0..0x2400B2A it is THE INTERFACE OF THE DOWNLOADED
 * PROGRAMS (verified on the 29 distinct games and outings of the download pack,
 * notes/findings/tamago.md): they are linked for 0x2000000, and every firmware address
 * they call lies in 0x2400AD0..0x2401D92. Games use the button / timer / display-object
 * half (api_get_btn_a..func_2400DCA), outings the animation half (api_anim_draw_image,
 * api_anim_frame, api_scene_room, api_anim_draw_pet, api_anim_text_line, api_reaction_anim, api_set_pic_slot /
 * api_get_pic_slot, api_store_item); api_object_move, api_object_set_image_attr, api_scene_window2, api_reaction_anim,
 * api_set_pic_slot, api_get_pic_slot and api_store_item have no direct caller in the firmware at
 * all. The stat and clock getters (api_get_happiness..func_2400C54) are used by no program in
 * the pack. The real work is in func_2401Fxx/2402xxx (asm + C below) and the IRAM
 * blitters.
 */

/*
 * Game API: forwards to gfx_display_reset (clear display list + dirty rects).
 */
void api_display_reset(void)
{
    gfx_display_reset();
}

/*
 * Game API (block 0x2400CF4..0x2401DA0 of thin wrappers around the display-object and
 * animation functions, plus resource tables): the entry points of the downloaded games
 * and outings (see the file comment); the built-in mini-games (0x251C..0x2527) call the
 * engine through it too. This one: set image of object a (gfx_object_set; used by no
 * downloadable program in the pack).
 */
void api_object_set_image_sized(u8 a, u16 b, u8 c, const u8 *d, u8 e, u8 f)
{
    gfx_object_set(a, b, c, d, e, f);
}

/*
 * Game API: forwards to gfx_object_set_image (set image of object).
 */
void api_object_set_image(u8 obj, u16 palette, u8 frame, const u8 *image)
{
    gfx_object_set_image(obj, palette, frame, image);
}

/*
 * Game API: forwards to gfx_object_set_image_clipped.
 */
void api_object_set_image_clipped(u8 a, u16 b, u8 c, const u8 *d, u8 e, u8 f)
{
    gfx_object_set_image_clipped(a, b, c, d, e, f);
}

/*
 * Game API: forwards to gfx_object_move (move object).
 */
void api_object_move(u8 p0, s16 p1, s16 p2)
{
    gfx_object_move(p0, p1, p2);
}

/*
 * Game API: forwards to gfx_object_move_dirty.
 */
void api_object_move_dirty(u8 p0, s16 p1, s16 p2)
{
    gfx_object_move_dirty(p0, p1, p2);
}

extern void func_2402082_2400D4E(int, int) __asm__("gfx_object_set_image_attr") ;
/*
 * Game API: forwards to gfx_object_set_image_attr.
 */
void api_object_set_image_attr(u8 p0, u8 p1)
{
    func_2402082_2400D4E(p0, p1);
}

/*
 * Game API: create a display object (gfx_object_create).
 */
u8 api_object_create(s16 a, s16 b, u16 c, u8 d, u16 e, const u8 *f)
{
    return gfx_object_create(a, b, c, d, e, f);
}

/*
 * Game API: forwards to gfx_object_create (append display object).
 */
u8 api_object_create_plain(s16 a, s16 b, u16 c, u8 d, const u8 *f)
{
    return gfx_object_create(a, b, c, d, 0, f);
}

/*
 * Game API: forwards to gfx_object_draw_glyph.
 */
void api_object_draw_glyph(u8 a, u16 b, int c, u8 d)
{
    gfx_object_draw_glyph(a, b, c, d);
}

/*
 * Game API: forwards to gfx_object_set_flags (set attribute/flags).
 */
void api_object_set_flags(u8 p0, u16 p1)
{
    gfx_object_set_flags(p0, p1);
}

/*
 * Game API: forwards to anim_scene_game_corner.
 */
void api_scene_game_corner(u8 p0)
{
    anim_scene_game_corner(p0);
}

/*
 * Game API: forwards to anim_draw_pic (show an image in a sprite slot).
 */
void api_anim_draw_image(u8 a, const u8 *b, u16 c, s16 d, s16 e)
{
    anim_draw_pic(a, b, c, d, e);
}

/*
 * Game API: forwards to anim_queue_sound (queue sound for next frame).
 */
void api_anim_queue_sound(u8 p0)
{
    anim_queue_sound(p0);
}

/*
 * Game API: keyframe end / wait (anim_frame).
 */
u8 api_anim_frame(int a, int b)
{
    return anim_frame(a, b);
}

/*
 * Game API: forwards to chara_draw_second.
 */
void api_draw_other_chara(u8 a, u8 b, u8 c, u16 d, s16 e, s16 f)
{
    chara_draw_second(a, b, c, d, e, f);
}

/*
 * Game API: forwards to chara_draw_any.
 */
void api_draw_chara_by_id(u8 a, u8 b, u8 c, u16 d, s16 e, s16 f, int g)
{
    chara_draw_any(a, b, c, d, e, f, g);
}


/*
 * Game API: draw text glyphs into an object (forwards to gfx_draw_text_line -> txt_render_line)
 * (asm).
 */
void api_draw_text(u8 a, s16 b, s16 c, u16 d, int e, u8 f, u8 g)
{
    gfx_draw_text_line(a, b, c, d, e, f, g);
}

/*
 * Game API: forwards to chara_select_other_by_id.
 */
u8 api_select_other_chara(u16 a)
{
    return chara_select_other_by_id(a);
}

/*
 * Game API: forwards to anim_draw_companion.
 */
void api_anim_draw_companion(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    anim_draw_companion(a, b, c, d, e);
}


/*
 * Game API, asset directory: picture number a (0..345, 346 entries) -> address of the
 * image in the graphics area of the flash (0x2080000.. and 0x2240000..); unknown ->
 * D_21918A8. Part of the API block used by the mini-games (and available to downloaded
 * games).
 */
const u8 *api_picture_address(int a)
{
    const u8 *r = D_21918A8;
    switch (a) {
    case 0: r = D_214D7FC; break;
    case 1: r = D_214D890; break;
    case 2: r = D_214D924; break;
    case 3: r = D_214D9B8; break;
    case 4: r = D_214DA4C; break;
    case 5: r = D_214DB6C; break;
    case 6: r = D_214DBFC; break;
    case 7: r = D_214DC90; break;
    case 8: r = D_214DD1C; break;
    case 9: r = D_214DDA8; break;
    case 10: r = D_214DE34; break;
    case 11: r = D_214DEC0; break;
    case 12: r = D_214DF50; break;
    case 13: r = D_214DFE4; break;
    case 14: r = D_214E078; break;
    case 15: r = D_214E108; break;
    case 16: r = D_214E194; break;
    case 17: r = D_214E220; break;
    case 18: r = D_214E2AC; break;
    case 19: r = D_214E338; break;
    case 20: r = D_214E3C4; break;
    case 21: r = D_214E450; break;
    case 22: r = D_214E4E0; break;
    case 23: r = D_214E570; break;
    case 24: r = D_214E600; break;
    case 25: r = D_214E810; break;
    case 26: r = D_214E8A0; break;
    case 27: r = D_214E930; break;
    case 28: r = D_214E9C4; break;
    case 29: r = D_214EA58; break;
    case 30: r = D_214EAEC; break;
    case 31: r = D_214EB80; break;
    case 32: r = D_214EC5C; break;
    case 33: r = D_214ECEC; break;
    case 34: r = D_214ED7C; break;
    case 35: r = D_214EE08; break;
    case 36: r = D_214EE94; break;
    case 37: r = D_214EF20; break;
    case 38: r = D_214EFAC; break;
    case 39: r = D_214F038; break;
    case 40: r = D_214F0CC; break;
    case 41: r = D_214F680; break;
    case 42: r = D_214FB14; break;
    case 43: r = D_214FBA8; break;
    case 44: r = D_214FC3C; break;
    case 45: r = D_214FCD0; break;
    case 46: r = D_214FD64; break;
    case 47: r = D_215017C; break;
    case 48: r = D_2150A48; break;
    case 49: r = D_2151640; break;
    case 50: r = D_2151BC8; break;
    case 51: r = D_2152DF0; break;
    case 52: r = D_2153084; break;
    case 53: r = D_2153310; break;
    case 54: r = D_215359C; break;
    case 55: r = D_2153828; break;
    case 56: r = D_21538C0; break;
    case 57: r = D_2153958; break;
    case 58: r = D_21539F0; break;
    case 59: r = D_2153A88; break;
    case 60: r = D_2153AB8; break;
    case 61: r = D_2153B6C; break;
    case 62: r = D_2153D68; break;
    case 63: r = D_2153DB0; break;
    case 64: r = D_2154050; break;
    case 65: r = D_2155274; break;
    case 66: r = D_2156488; break;
    case 67: r = D_21566B8; break;
    case 68: r = D_2156A4C; break;
    case 69: r = D_215B7E0; break;
    case 70: r = D_215C658; break;
    case 71: r = D_215C920; break;
    case 72: r = D_215CBFC; break;
    case 73: r = D_215CE10; break;
    case 74: r = D_215D918; break;
    case 75: r = D_215DA7C; break;
    case 76: r = D_215E2C4; break;
    case 77: r = D_215FA14; break;
    case 78: r = D_215FCAC; break;
    case 79: r = D_215FF44; break;
    case 80: r = D_21601A4; break;
    case 81: r = D_2160404; break;
    case 82: r = D_2160664; break;
    case 83: r = D_21608C4; break;
    case 84: r = D_2160B24; break;
    case 85: r = D_2160D80; break;
    case 86: r = D_2160FDC; break;
    case 87: r = D_2161234; break;
    case 88: r = D_2161490; break;
    case 89: r = D_21616EC; break;
    case 90: r = D_2161824; break;
    case 91: r = D_2161964; break;
    case 92: r = D_2161AA8; break;
    case 93: r = D_2161BE0; break;
    case 94: r = D_2161D18; break;
    case 95: r = D_2161E4C; break;
    case 96: r = D_2161F80; break;
    case 97: r = D_21620B8; break;
    case 98: r = D_21621EC; break;
    case 99: r = D_216287C; break;
    case 100: r = D_2162F0C; break;
    case 101: r = D_2163A28; break;
    case 102: r = D_2163F38; break;
    case 103: r = D_21647C0; break;
    case 104: r = D_2164E20; break;
    case 105: r = D_2164EAC; break;
    case 106: r = D_21650B8; break;
    case 107: r = D_2165D28; break;
    case 108: r = D_2166D70; break;
    case 109: r = D_2166E70; break;
    case 110: r = D_2166F1C; break;
    case 111: r = D_21681EC; break;
    case 112: r = D_216940C; break;
    case 113: r = D_216AE0C; break;
    case 114: r = D_216B014; break;
    case 115: r = D_216B21C; break;
    case 116: r = D_216B434; break;
    case 117: r = D_216B64C; break;
    case 118: r = D_216BBCC; break;
    case 119: r = D_216CDD8; break;
    case 120: r = D_216D484; break;
    case 121: r = D_216D7C8; break;
    case 122: r = D_216DFDC; break;
    case 123: r = D_216E1EC; break;
    case 124: r = D_216E27C; break;
    case 125: r = D_216F498; break;
    case 126: r = D_216F6B4; break;
    case 127: r = D_2179C10; break;
    case 128: r = D_2179D68; break;
    case 129: r = D_2179F60; break;
    case 130: r = D_217A188; break;
    case 131: r = D_217A5F0; break;
    case 132: r = D_217A684; break;
    case 133: r = D_217A718; break;
    case 134: r = D_217A7B0; break;
    case 135: r = D_217A848; break;
    case 136: r = D_217AD30; break;
    case 137: r = D_217AF28; break;
    case 138: r = D_21840E8; break;
    case 139: r = D_218421C; break;
    case 140: r = D_2184350; break;
    case 141: r = D_21845A0; break;
    case 142: r = D_2184E70; break;
    case 143: r = D_2185080; break;
    case 144: r = D_217C178; break;
    case 145: r = D_217C2DC; break;
    case 146: r = D_217C40C; break;
    case 147: r = D_217C5A0; break;
    case 148: r = D_217C6D8; break;
    case 149: r = D_217C84C; break;
    case 150: r = D_217C9CC; break;
    case 151: r = D_217ECFC; break;
    case 152: r = D_217EF0C; break;
    case 153: r = D_217F130; break;
    case 154: r = D_217F794; break;
    case 155: r = D_217F9B4; break;
    case 156: r = D_217FCE4; break;
    case 157: r = D_2181AB8; break;
    case 158: r = D_2181CA8; break;
    case 159: r = D_2182304; break;
    case 160: r = D_2182434; break;
    case 161: r = D_2182564; break;
    case 162: r = D_2183688; break;
    case 163: r = D_21838F8; break;
    case 164: r = D_2183E78; break;
    case 165: r = D_2183FB4; break;
    case 166: r = D_2185270; break;
    case 167: r = D_2185440; break;
    case 168: r = D_218568C; break;
    case 169: r = D_21858DC; break;
    case 170: r = D_2185B24; break;
    case 171: r = D_2185C98; break;
    case 172: r = D_218A668; break;
    case 173: r = D_218A7E0; break;
    case 174: r = D_21903E4; break;
    case 175: r = D_21906C4; break;
    case 176: r = D_21908BC; break;
    case 177: r = D_21918A8; break;
    case 178: r = D_21919D0; break;
    case 179: r = D_2191C3C; break;
    case 180: r = D_21938F4; break;
    case 181: r = D_2193A38; break;
    case 182: r = D_2193B68; break;
    case 183: r = D_2195B50; break;
    case 184: r = D_2195C6C; break;
    case 185: r = D_2195EA0; break;
    case 186: r = D_2196134; break;
    case 187: r = D_2196238; break;
    case 188: r = D_21962EC; break;
    case 189: r = D_2196370; break;
    case 190: r = D_2196400; break;
    case 191: r = D_2197434; break;
    case 192: r = D_2198640; break;
    case 193: r = D_219A9E0; break;
    case 194: r = D_219AB2C; break;
    case 195: r = D_219ABC0; break;
    case 196: r = D_219AF60; break;
    case 197: r = D_219AFF4; break;
    case 198: r = D_219B084; break;
    case 199: r = D_219B114; break;
    case 200: r = D_219B24C; break;
    case 201: r = D_219B37C; break;
    case 202: r = D_219B4AC; break;
    case 203: r = D_219B5DC; break;
    case 204: r = D_219B710; break;
    case 205: r = D_219B980; break;
    case 206: r = D_219BB1C; break;
    case 207: r = D_219BC50; break;
    case 208: r = D_219DE98; break;
    case 209: r = D_219DEF0; break;
    case 210: r = D_219DF48; break;
    case 211: r = D_219E0D0; break;
    case 212: r = D_219E874; break;
    case 213: r = D_219EA08; break;
    case 214: r = D_219EC20; break;
    case 215: r = D_219EE38; break;
    case 216: r = D_219F3A8; break;
    case 217: r = D_219F604; break;
    case 218: r = D_219F814; break;
    case 219: r = D_219FFAC; break;
    case 220: r = D_21A02DC; break;
    case 221: r = D_21A05D8; break;
    case 222: r = D_21A06D8; break;
    case 223: r = D_21A0814; break;
    case 224: r = D_21A1088; break;
    case 225: r = D_21A1278; break;
    case 226: r = D_21A146C; break;
    case 227: r = D_21A1664; break;
    case 228: r = D_21A179C; break;
    case 229: r = D_21A1998; break;
    case 230: r = D_21A2140; break;
    case 231: r = D_21A2478; break;
    case 232: r = D_21A264C; break;
    case 233: r = D_21A2840; break;
    case 234: r = D_21A3230; break;
    case 235: r = D_21A3484; break;
    case 236: r = D_21A36D8; break;
    case 237: r = D_21A392C; break;
    case 238: r = D_21A4E74; break;
    case 239: r = D_21A4F04; break;
    case 240: r = D_21A508C; break;
    case 241: r = D_21A50CC; break;
    case 242: r = D_21A5124; break;
    case 243: r = D_21A5178; break;
    case 244: r = D_21A51CC; break;
    case 245: r = D_21A5220; break;
    case 246: r = D_21A5538; break;
    case 247: r = D_21A584C; break;
    case 248: r = D_21A5C20; break;
    case 249: r = D_21A60F8; break;
    case 250: r = D_21A6230; break;
    case 251: r = D_21A6364; break;
    case 252: r = D_21A6498; break;
    case 253: r = D_21A65D4; break;
    case 254: r = D_21A670C; break;
    case 255: r = D_21A6840; break;
    case 256: r = D_21A6970; break;
    case 257: r = D_21A6AAC; break;
    case 258: r = D_21A6D90; break;
    case 259: r = D_21A6EC0; break;
    case 260: r = D_21A6F58; break;
    case 261: r = D_21A71AC; break;
    case 262: r = D_21A72E0; break;
    case 263: r = D_21A74E4; break;
    case 264: r = D_21A7620; break;
    case 265: r = D_21A7D34; break;
    case 266: r = D_21A7E64; break;
    case 267: r = D_21A8064; break;
    case 268: r = D_21A82B8; break;
    case 269: r = D_21A8434; break;
    case 270: r = D_21A85C0; break;
    case 271: r = D_21A8D1C; break;
    case 272: r = D_21A8FB8; break;
    case 273: r = D_21A9454; break;
    case 274: r = D_21A9584; break;
    case 275: r = D_21ACF20; break;
    case 276: r = D_21AD050; break;
    case 277: r = D_21AD53C; break;
    case 278: r = D_21AD9CC; break;
    case 279: r = D_21ADAB0; break;
    case 280: r = D_21AE7A4; break;
    case 281: r = D_21AF338; break;
    case 282: r = D_21AF648; break;
    case 283: r = D_21B09A0; break;
    case 284: r = D_21B1BB8; break;
    case 285: r = D_21B1EEC; break;
    case 286: r = D_21B2168; break;
    case 287: r = D_21B33D8; break;
    case 288: r = D_21B35D8; break;
    case 289: r = D_21BE018; break;
    case 290: r = D_21BE39C; break;
    case 291: r = D_21BE720; break;
    case 292: r = D_21BEB08; break;
    case 293: r = D_21BFD1C; break;
    case 294: r = D_21C0B90; break;
    case 295: r = D_21C0D68; break;
    case 296: r = D_21C0F78; break;
    case 297: r = D_21C1188; break;
    case 298: r = D_21C13A0; break;
    case 299: r = D_20963BC; break;
    case 300: r = D_20C8204; break;
    case 301: r = D_20C942C; break;
    case 302: r = D_20CA650; break;
    case 303: r = D_20CB878; break;
    case 304: r = D_20CCAA0; break;
    case 305: r = gfx_bg_closet; break;
    case 306: r = D_20B83D4; break;
    case 307: r = D_20B95FC; break;
    case 308: r = D_20BA824; break;
    case 309: r = D_20BBA4C; break;
    case 310: r = D_20BCC74; break;
    case 311: r = D_20BDE9C; break;
    case 312: r = D_20BF0C4; break;
    case 313: r = D_20C02EC; break;
    case 314: r = D_20C1514; break;
    case 315: r = D_2240AFC; break;
    case 316: r = D_2240D2C; break;
    case 317: r = D_2240F5C; break;
    case 318: r = D_224118C; break;
    case 319: r = D_225565C; break;
    case 320: r = D_2255894; break;
    case 321: r = D_2255ACC; break;
    case 322: r = D_2255D04; break;
    case 323: r = D_2258740; break;
    case 324: r = D_225897C; break;
    case 325: r = D_2258BB8; break;
    case 326: r = D_2259000; break;
    case 327: r = D_2259454; break;
    case 328: r = D_22598A8; break;
    case 329: r = D_2259CFC; break;
    case 330: r = D_225A150; break;
    case 331: r = D_225A5A8; break;
    case 332: r = D_225AA00; break;
    case 333: r = D_225AE58; break;
    case 334: r = D_225B2B0; break;
    case 335: r = D_225B704; break;
    case 336: r = D_225BB58; break;
    case 337: r = D_225BFAC; break;
    case 338: r = D_225C400; break;
    case 339: r = D_225C848; break;
    case 340: r = D_225CC90; break;
    case 341: r = D_225D0D8; break;
    case 342: r = D_225F8D4; break;
    case 343: r = D_225FB18; break;
    case 344: r = D_225FD5C; break;
    case 345: r = D_225FFA0; break;
    }
    return r;
}

/*
 * Game API: forwards to anim_scene_room (scene set-up in the room).
 */
void api_scene_room(u8 p0)
{
    anim_scene_room(p0);
}

/*
 * Game API: forwards to anim_scene_window.
 */
void api_scene_window(u8 p0)
{
    anim_scene_window(p0);
}

/*
 * Game API: forwards to anim_scene_window_mode.
 */
void api_scene_window_mode(u8 p0, u8 p1)
{
    anim_scene_window_mode(p0, p1);
}

/*
 * Game API: forwards to anim_scene_window2.
 */
void api_scene_window2(u8 p0)
{
    anim_scene_window2(p0);
}

/*
 * Game API: forwards to anim_scene_living.
 */
void api_scene_living(u8 p0)
{
    anim_scene_living(p0);
}

/*
 * Game API: forwards to anim_scene_living_poops.
 */
void api_scene_living_poops(u8 p0)
{
    anim_scene_living_poops(p0);
}

/*
 * Game API: forwards to anim_scene_kitchen.
 */
void api_scene_kitchen(u8 p0)
{
    anim_scene_kitchen(p0);
}

/*
 * Game API: forwards to anim_scene_tama_depa.
 */
void api_scene_tama_depa(u8 p0)
{
    anim_scene_tama_depa(p0);
}

/*
 * Game API: forwards to anim_scene_park.
 */
void api_scene_park(u8 p0)
{
    anim_scene_park(p0);
}

/*
 * Game API: forwards to anim_scene_blank.
 */
void api_scene_blank(u8 p0)
{
    anim_scene_blank(p0);
}

/*
 * Game API: forwards to anim_scene_bath.
 */
void api_scene_bath(u8 p0)
{
    anim_scene_bath(p0);
}

/*
 * Game API: forwards to anim_scene_toilet.
 */
void api_scene_toilet(u8 p0)
{
    anim_scene_toilet(p0);
}

/*
 * Game API: forwards to anim_scene_bedroom.
 */
void api_scene_bedroom(u8 p0)
{
    anim_scene_bedroom(p0);
}

/*
 * Game API: forwards to anim_draw_pet (show the pet pose in a sprite slot).
 */
void api_anim_draw_pet(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    anim_draw_pet(a, b, c, d, e);
}

/*
 * Game API: forwards to anim_draw_other_chara.
 */
void api_anim_draw_other_chara(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    anim_draw_other_chara(a, b, c, d, e);
}

/*
 * Game API: forwards to anim_draw_fifth_chara.
 */
void api_anim_draw_fifth_chara(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    anim_draw_fifth_chara(a, b, c, d, e);
}

/*
 * Game API: forwards to anim_draw_digit.
 */
void api_anim_draw_digit(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    anim_draw_digit(a, b, c, d, e);
}

/*
 * Game API: forwards to anim_draw_text.
 */
void api_anim_text_line(u8 a, const u8 *b, u8 c, s16 d, s16 e)
{
    anim_draw_text(a, (u8 *)b, c, d, e);
}

/*
 * Game API: forwards to anim_draw_glyph.
 */
void api_anim_glyph_text(u8 a, int b, u16 c, s16 d, s16 e)
{
    anim_draw_glyph(a, b, c, d, e);
}

/*
 * Game API: forwards to anim_frame_wait (flush, sound, wait, buttons).
 */
u8 api_anim_frame_wait(int a, int b)
{
    return anim_frame_wait(a, b);
}

/*
 * Game API: forwards to chara_select_fifth_by_id.
 */
void api_select_fifth_chara(u16 id)
{
    chara_select_fifth_by_id(id);
}

/*
 * Game API: sprite set of the current character: chara_sprite_sets[chara_pet_sprite_set] (100 built-in
 * characters, 15 pose images each) or, for the downloaded/special ones (chara_pet_sprite_set >= 100),
 * the set found by dl_chara_pose_pic(pet.chara_row + 0x77, 0) in the item banks.
 */
struct SpriteSet *api_pet_sprite_set(void)
{
    if (chara_pet_sprite_set < 100)
        return &chara_sprite_sets[chara_pet_sprite_set];
    return dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, 0);
}

/*
 * Game API: sprite table of character a.
 */
struct SpriteSet *api_chara_sprite_set(int a)
{
    return &chara_sprite_sets[a];
}

/*
 * Game API: returns the address of the digit font D_2142E38 (stub).
 */
int api_digit_font(void)
{
    return D_2142E38;
}

/*
 * Game API: dispatch to one of nine shared generated "reaction" effects/animations
 * (anim_reaction0 .. anim_reaction8 in F0) by index a, with the item/partner record b (and c
 * for 7/8). No firmware caller; used by 5 downloadable outings.
 */
void api_reaction_anim(u8 a, const u8 *b, int c)
{
    switch (a) {
    case 0:
        anim_reaction0(b);
        break;
    case 1:
        anim_reaction1(b);
        break;
    case 2:
        anim_reaction2(b);
        break;
    case 3:
        anim_reaction3(b);
        break;
    case 4:
        anim_reaction4(b);
        break;
    case 5:
        anim_reaction5(b);
        break;
    case 6:
        anim_reaction6(b);
        break;
    case 7:
        anim_reaction7(b, c);
        break;
    case 8:
        anim_reaction8(b, c);
        break;
    }
}

/*
 * Game API: forwards to ui_show_message.
 */
void api_show_message(int kind, const u8 *p, const u8 *s)
{
    ui_show_message(kind, p, s);
}

/*
 * Game API: set word a of the shared game-variable array anim_pic_slots (no firmware caller;
 * used by the downloadable outings, with api_get_pic_slot).
 */
void api_set_pic_slot(u8 a, int v)
{
    (&anim_pic_slots[0][0])[a] = (const u8 *)v;
}

/*
 * Game API: read word a of the shared game-variable array anim_pic_slots.
 */
u32 api_get_pic_slot(u8 a)
{
    return (u32)(&anim_pic_slots[0][0])[a];
}

/*
 * Game API: put item record p into the inventory (item_add_to_inventory), returning 0/1. No caller
 * in the firmware: 7 of the 9 downloadable outings carry their prizes as complete TAMAGO
 * item files inside their own image and hand them over through this.
 */
int api_store_item(struct TamagoFile *p)
{
    int r = item_add_to_inventory(p);
    if (r)
        r = 1;
    return r;
}

/*
 * Game API: pose b of character a's sprite table.
 */
u32 api_chara_pose_image(int a, int b)
{
    return (&chara_sprite_sets[a].pose0)[b];
}

