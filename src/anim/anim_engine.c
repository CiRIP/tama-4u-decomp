/*
 * The display-object and animation engine behind the generated scripts: display list reset,
 * object create/set/move, tint and palette set-up, LCD init, scene set-ups for every room and
 * place, sprite/picture/text drawing in scenes, keyframe wait and sound queueing. Also renders
 * character pose sets to flash and the glyph text renderer used by text input.
 *
 * Code 2401DD6..240413C, .rodata from 255C800.
 */
#include "proto/anim/anim_engine.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/chara/chara_draw.h"
#include "proto/food/food_meals.h"
#include "proto/room/room_draw.h"
#include "proto/room/room_window.h"
#include "proto/shop/shop_reform.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/evo/evo_special.h"
#include "proto/ui/ui_message.h"
#include "proto/pet/pet_gone.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/dl/dl_chara.h"
#include "proto/chara/chara_select.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"
#include "hw.h"

extern void func_2402014_2400D12(int, int, int) __asm__("gfx_object_set_image") ;

extern void func_2402082_2400D4E(int, int) __asm__("gfx_object_set_image_attr") ;

/*
 * An empty function (between the API functions in the original).
 */
void func_2401DD6(void)
{
}

/*
 * LCD controller init (called once by main sys_main, before anything is drawn). The
 * panel/controller variant comes from the P07 strap: P0 data register 0x300380 bit 7,
 * input with the internal pull-up on since sys_gpio_init (pMISC_PUP0 = 0xEF), so an open
 * pin reads 1. The result goes to lcd_is_st7735 (1 = P07 high) and only selects the window
 * offsets of the IRAM flush routines (func_00006B8 / func_000081A: column +2, row +1 when
 * 1, none when 0). Right after this call main makes P07 an output driven low (P0 IOC 0xA8,
 * data 0x5F), so the strap is read exactly once per boot.
 * P07 high -> lcd_init_st7735, the Sitronix ST7735 sequence (B1/B2/B3/C0..C4 parameter counts
 * fit only the ST7735).
 * P07 low  -> the inline sequence below for an Ilitek ILI9163(C)-type controller (see
 * notes/findings/lcd.md; the C2 / C6 commands do not fit the ST7735). A halfword store to
 * 0x600000 writes the low byte to A0=0 (command) and the high byte to A0=1 (first
 * parameter), the LCD being an 8-bit device on #CE8:
 *   01 SWRESET, 20 ms;
 *   C0 03 00 PWCTR1: VRH = 3 (GVDD), VC = 0;
 *   C2 05    PWCTR3: op-amp current APA = 5 (large);
 *   C5 C4    VMCTR1: VMH = 0x44 -> VCOMH 4.200 V (bit 7 unused; VML not sent, stays at
 *            the reset value);
 *   C6 13    VMCTR2: VMA = 0x13 (ILI9163C command list: the reset value);
 *   36 C0    MADCTL: MY = MX = 1 (rotated by 180 degrees), RGB order (lcd_init_ili9163_old, the
 *            unused older copy, sends 36 08 instead);
 *   11 SLPOUT, 120 ms;
 *   E0 / BuiltinItemKeyEntry  gamma, 16 bytes each (the ILI9163C takes 15; it also applies E0/E1 only
 *            after F2 01 = GAM_R_SEL, which is never sent);
 *   29 DISPON.
 * Neither path sends COLMOD (3A) or a window: the controllers stay in their reset pixel
 * format, 18 bits per pixel in three byte transfers, which is what the flush routines
 * send.
 */
void lcd_init(void)
{
    if ((s8)P0_P0D < 0) {
        lcd_init_st7735();
        lcd_is_st7735 = 1;
    } else {
        lcd_is_st7735 = 0;
        REG8(0x600000) = 0x01;
        tmr_delay_ms(20);
        REG16(0x600000) = 0x03c0;
        REG8(0x600001) = 0x00;
        REG16(0x600000) = 0x05c2;
        REG16(0x600000) = 0xc4c5;
        REG16(0x600000) = 0x13c6;
        REG16(0x600000) = 0xc036;
        REG8(0x600000) = 0x11;
        tmr_delay_ms(120);
        REG8(0x600000) = 0xe0;
        REG8(0x600001) = 0x03;
        REG8(0x600001) = 0x04;
        REG8(0x600001) = 0x08;
        REG8(0x600001) = 0x25;
        REG8(0x600001) = 0x22;
        REG8(0x600001) = 0x0a;
        REG8(0x600001) = 0x1e;
        REG8(0x600001) = 0x02;
        REG8(0x600001) = 0x02;
        REG8(0x600001) = 0x04;
        REG8(0x600001) = 0x0f;
        REG8(0x600001) = 0x0f;
        REG8(0x600001) = 0x00;
        REG8(0x600001) = 0x04;
        REG8(0x600001) = 0x03;
        REG8(0x600001) = 0x06;
        REG8(0x600000) = 0xe1;
        REG8(0x600001) = 0x0a;
        REG8(0x600001) = 0x22;
        REG8(0x600001) = 0x25;
        REG8(0x600001) = 0x15;
        REG8(0x600001) = 0x0f;
        REG8(0x600001) = 0x0f;
        REG8(0x600001) = 0x01;
        REG8(0x600001) = 0x1e;
        REG8(0x600001) = 0x06;
        REG8(0x600001) = 0x04;
        REG8(0x600001) = 0x05;
        REG8(0x600001) = 0x01;
        REG8(0x600001) = 0x0f;
        REG8(0x600001) = 0x0f;
        REG8(0x600001) = 0x04;
        REG8(0x600001) = 0x02;
        REG8(0x600000) = 0x29;
    }
}

/*
 * An empty function.
 */
void func_2401EEC(void)
{
}

/*
 * Reset the renderer / start a new screen: empty the display list (gfx_object_count = number of
 * objects in gfx_objects = 0) and set both dirty rectangles to empty (main area
 * gfx_dirty_main_y0..D_0001808 and top/bottom icon bars gfx_dirty_bar_y0..D_000180C, each y0, y1, x0, x1:
 * y0 = 0x7F, y1 = 0, x0 = 0x7F, x1 = 0); gfx_tint_first/F (the palette-index range the
 * main-area flush tints, see gfx_set_tint_range) and ui_attention_icon_state = care icon state are cleared.
 */
void gfx_display_reset(void)
{
    gfx_dirty_main_y0 = 0x7F;
    gfx_dirty_main_y1 = 0;
    gfx_dirty_main_x0 = 0x7F;
    gfx_dirty_main_x1 = 0;
    gfx_dirty_bar_y0 = 0x7F;
    gfx_dirty_bar_y1 = 0;
    gfx_dirty_bar_x0 = 0x7F;
    gfx_dirty_bar_x1 = 0;
    gfx_object_count = 0;
    gfx_tint_first = 0;
    gfx_tint_end = 0;
    ui_attention_icon_state = 0;
}

/*
 * An empty function.
 */
void func_2401F52(void)
{
}

/*
 * Compute the palette slot base of display objects up to n (gfx_tint_first / gfx_tint_end); objects
 * from 9 on start after the colours of the previous ones (asm). The resulting index range
 * [gfx_tint_first, gfx_tint_end) is what the main-area LCD flush (IRAM func_000081A) blends
 * towards pink (R 0xFF, G 0x51, B 0x58): weight (91 - row) / 64, so almost all pink at
 * row 28, fading out at row 91; rows below 91 are not tinted.
 */
void gfx_set_tint_range(u8 n)
{
    int i;
    const u8 *img;

    if (gfx_objects[n].image == 0) {
        gfx_tint_first = 0;
        gfx_tint_end = 0;
    } else {
        for (i = 9; i <= n; i++) {
            img = gfx_objects[i].image;
            if (i == 9) {
                gfx_tint_first = gfx_objects[i].palette_base;
                gfx_tint_end = gfx_objects[i].palette_base;
            }
            gfx_tint_end += img[2];
        }
    }
}

/*
 * An empty function.
 */
void func_2401FCE(void)
{
}

/*
 * Set display object a: image d with size e x f, palette slot b, attribute c; loads the
 * image palette into the shared palette tables (IRAM func_0000312) (asm).
 */
void gfx_object_set(u8 a, u16 b, u8 c, const u8 *d, u8 e, u8 f)
{
    struct GfxObject *p = &gfx_objects[a];

    p->width = e;
    p->height = f;
    p->frame = c;
    p->image = d;
    func_0000312(p, d, b);
}

/*
 * An empty function.
 */
void func_2402012(void)
{
}

/*
 * Re-point display object a to image d with the image's own size (palette slot / layer b,
 * byte +0x0A = c), marking both the old and the new area dirty (func_0000000 = IRAM
 * "extend the dirty rectangle by this sprite"); d == 0 empties the slot.
 */
void gfx_object_set_image(u8 a, u16 b, u8 c, const u8 *d)
{
    struct GfxObject *p = &gfx_objects[a];

    func_0000000(p);
    if (d != 0) {
        gfx_object_set(a, b, c, d, d[0], d[1]);
        func_0000000(p);
    } else {
        gfx_object_set(a, b, c, d, 0, 0);
    }
}

/*
 * An empty function.
 */
void func_2402080(void)
{
}

/*
 * Display object a: change only the image pointer/size and attribute byte b (+0x0A).
 */
void gfx_object_set_image_attr(u8 a, u8 b, const u8 *d)
{
    struct GfxObject *p = &gfx_objects[a];

    func_0000000(p);
    if (d != 0) {
        p->width = d[0];
        p->height = d[1];
        p->frame = b;
        p->image = d;
        func_0000000(p);
    } else {
        p->width = 0;
        p->height = 0;
        p->frame = b;
        p->image = d;
    }
}

/*
 * An empty function.
 */
void func_24020E2(void)
{
}

/*
 * Display object a: set image d clipped to e x f pixels (partial image, e.g. gauges).
 */
void gfx_object_set_image_clipped(u8 a, u16 b, u8 c, const u8 *d, u8 e, u8 f)
{
    struct GfxObject *p = &gfx_objects[a];

    if (d != 0) {
        if (d[0] < e)
            e = d[0];
        if (d[1] < f)
            f = d[1];
    }
    func_0000000(p);
    gfx_object_set(a, b, c, d, e, f);
    func_0000000(p);
}

/*
 * An empty function.
 */
void func_2402154(void)
{
}

/*
 * Display object a: move to screen position (b, c) (top-left corner, screen pixels) and
 * mark its area dirty.
 */
void gfx_object_move(u8 a, s16 b, s16 c)
{
    struct GfxObject *p = &gfx_objects[a];

    p->x = b;
    p->y = c;
    func_0000000(p);
}

/*
 * An empty function.
 */
void func_2402186(void)
{
}

/*
 * Display object a: move to (b, c), marking both the old and the new area dirty.
 */
void gfx_object_move_dirty(u8 a, s16 b, s16 c)
{
    func_0000000(&gfx_objects[a]);
    gfx_object_move(a, b, c);
}

/*
 * An empty function.
 */
void func_24021C0(void)
{
}


/*
 * gfx_object_create(x, y, c, d, flags, image) (asm): create a display object: appends entry
 * gfx_object_count to the display list gfx_objects (48 entries of 20 bytes: +0 image pointer, +4/+6
 * x/y, +8/+9 width/height, +0x0A attribute (flip or frame), +0x0C first palette slot,
 * +0x0E flags; `brk` if the list is full): x0e = flags, gfx_object_set(slot, c, d, image, w,
 * h) (c = palette slot, d = byte +0x0A) and gfx_object_move(slot, x, y) at screen position
 * (x, y); returns the slot number. image == 0 = an empty placeholder (used to reserve
 * slots for text drawn later). Objects are drawn in list order (later = on top) by the
 * IRAM renderer.
 */
s8 gfx_object_create(s16 x, s16 y, u16 c, u8 d, u16 flags, const u8 *img)
{
    if (gfx_object_count > 0x2f)
        asm volatile("brk");
    gfx_objects[gfx_object_count].flags = flags;
    if (img != 0)
        gfx_object_set(gfx_object_count, c, d, img, img[0], img[1]);
    else
        gfx_object_set(gfx_object_count, c, d, img, 0, 0);
    gfx_object_move(gfx_object_count, x, y);
    return gfx_object_count++;
}

/*
 * An empty function.
 */
void func_2402246(void)
{
}

/*
 * Display object a: render text/glyphs (txt_set_glyph).
 */
void gfx_object_draw_glyph(u8 a, u16 b, int c, u8 d)
{
    txt_set_glyph(a, b, c, d);
}

/*
 * An empty function.
 */
void func_2402264(void)
{
}

void f_2266(u8, s16, s16, u16, u8 *, u8, u8) __asm__("gfx_draw_text_line");

void f_2266(u8 a, s16 b, s16 c, u16 d, u8 *e, u8 f, u8 g)
{
    txt_render_line(a, b, c, d, e, f, g);
}

void func_2402294(void)
{
}

/*
 * Draw a centred text line (txt_draw_centered).
 */
void gfx_draw_text_centered(u8 a, s16 b, u8 c, const u8 *d, u8 e, u8 f)
{
    txt_draw_centered(a, b, c, d, e, f);
}

void func_24022BE(void)
{
}

/*
 * Display object a: set its attribute/flag halfword (+0x0E), keeping bits 14-15
 * (0x4000 = drawn in the icon-bar layer?, 0x8000 = ?). Low bits 0-2 select a colour effect
 * in the palette conversion (func_000014A: 1 = grey?, 2 = darker, 4 = ...), 0x400/0x800
 * = other effects, 0x10 = horizontal flip? (M)
 */
void gfx_object_set_flags(u8 a, u16 b)
{
    struct GfxObject *p = &gfx_objects[a];

    p->flags = (p->flags & 0xC000) | b;
}

void func_24022F0(void)
{
}

/*
 * Animation scene set-up (used at the start of the generated animation scripts): new
 * screen (gfx_display_reset), room background pointer anim_scene_bg = room_kitchen_bg_pics[pet.room_designs[1]]
 * (the current room, as in room_set_kitchen_design), then n empty sprite slots (anim_first_slot_idx = index of
 * the first one = the current sprite count), refresh the icon bars (ui_draw_icon_bars), no
 * queued sound (anim_queued_sound = 0). Other scene set-ups: anim_scene_window (animated background
 * kind 1), anim_scene_window2 (kind 2), anim_scene_living (the background of room_draw_background instead of
 * the room), anim_scene_window_mode (kind 1 with sub-mode m), anim_scene_kitchen / anim_scene_bath /
 * anim_scene_toilet / anim_scene_bedroom (location backgrounds chosen by pet.room_designs[1]..x06A with
 * variants x0BD..x0C0), anim_scene_game_corner (fixed background gfx_bg_game_corner), anim_scene_tama_depa /
 * anim_scene_park (gfx_bg_tama_depa / gfx_bg_park), anim_scene_blank.
 */
void anim_scene_room(u8 n)
{
    int i;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    i = 0;
    gfx_display_reset();
    anim_scene_bg = room_kitchen_bg_pics[save.pet.room_designs[1]];
    anim_first_slot_idx = gfx_object_count;
    for (; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_2402378(void)
{
}

/*
 * Animation scene set-up with animated background kind 1 (room_window_draw(0)); skipped when
 * ui_message_shown is set. See anim_scene_room.
 */
void anim_scene_window(u8 n)
{
    int i;

    if (ui_message_shown == 0) {
        anim_window_mode = 0;
        anim_bg_activity = 1;
        sys_debug_fast_forward = 0;
        gfx_display_reset();
        room_window_draw(0);
        anim_first_slot_idx = gfx_object_count;
        for (i = 0; i < n; i++)
            gfx_object_create(0, 0x1c, 0, 0, 0, 0);
        ui_draw_icon_bars();
        anim_queued_sound = 0;
    }
}

void func_24023FC(void)
{
}

/*
 * Animation scene set-up, animated background kind 1, sub-mode m (anim_window_mode = m + 1).
 */
void anim_scene_window_mode(u8 n, u8 m)
{
    int i;

    anim_window_mode = m + 1;
    anim_bg_activity = 1;
    sys_debug_fast_forward = 0;
    gfx_display_reset();
    room_window_draw(0);
    i = 0;
    anim_first_slot_idx = gfx_object_count;
    for (; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_240247C(void)
{
}

/*
 * Animation API: like anim_draw_pet for the character of the fifth slot (chara_fifth_row,
 * selected with chara_fifth_sprite_set by chara_select_fifth_by_id / chara_select_fifth_by_set).
 */
void anim_draw_fifth_chara(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    u8 t;

    anim_busy = 1;
    a += anim_first_slot_idx;
    t = gfx_palette_slot_for(a, 0, c);
    chara_draw_any(a, t, b, c, d, e, chara_fifth_row);
}

void func_24024E0(void)
{
}

void func_24024F2(void)
{
}

void func_2402504(void)
{
}

/*
 * Animation scene set-up with animated background kind 2 (room_window_draw(2)). See
 * anim_scene_room.
 */
void anim_scene_window2(u8 n)
{
    int i;

    if (ui_message_shown == 0) {
        anim_bg_activity = 2;
        sys_debug_fast_forward = 0;
        gfx_display_reset();
        room_window_draw(2);
        anim_first_slot_idx = gfx_object_count;
        for (i = 0; i < n; i++)
            gfx_object_create(0, 0x1c, 0, 0, 0, 0);
        ui_draw_icon_bars();
        anim_queued_sound = 0;
    }
}

void func_2402580(void)
{
}

/*
 * Animation scene set-up with the background of room_draw_background. See anim_scene_room.
 */
void anim_scene_living(u8 n)
{
    int i;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    i = 0;
    gfx_display_reset();
    room_draw_background();
    anim_first_slot_idx = gfx_object_count;
    for (; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_24025F0(void)
{
}

/*
 * Animation scene set-up with animated background kind 3 (room_draw_background, room_draw_poops(0)).
 */
void anim_scene_living_poops(u8 n)
{
    int i;

    anim_bg_activity = 3;
    sys_debug_fast_forward = 0;
    gfx_display_reset();
    room_draw_background();
    room_draw_poops(0);
    anim_first_slot_idx = gfx_object_count;
    for (i = 0; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_2402668(void)
{
}

/*
 * Animation scene set-up in the location whose background is room_kitchen_designs[pet.room_designs[1]]
 * (variants by pet.x0BD: object attribute 1 or 6).
 */
void anim_scene_kitchen(u8 n)
{
    int i;
    u8 a;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    gfx_display_reset();
    if (save.pet.room_designs[1] <= 3) {
        switch (save.pet.kitchen_dirt) {
        case 4:
            a = gfx_object_create(0, 0x1c, 0, 0, 1, 0);
            break;
        case 5:
            a = gfx_object_create(0, 0x1c, 0, 0, 6, 0);
            break;
        default:
            a = gfx_object_create(0, 0x1c, 0, 0, 0, 0);
            break;
        }
    } else {
        a = gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    }
    gfx_object_set_image(a, 0, 0, (const u8 *)room_kitchen_designs[save.pet.room_designs[1]]);
    anim_scene_bg = room_kitchen_bg_pics[save.pet.room_designs[1]];
    anim_scene_fg = room_kitchen_fg_pics[save.pet.room_designs[1]];
    anim_first_slot_idx = gfx_object_count;
    for (i = 0; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_2402752(void)
{
}

/*
 * Animation scene set-up with the fixed background picture gfx_bg_game_corner.
 */
void anim_scene_game_corner(u8 n)
{
    int i;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    i = 0;
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, gfx_bg_game_corner);
    anim_first_slot_idx = gfx_object_count;
    for (; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_24027D4(void)
{
}

/*
 * Animation scene set-up with the fixed background gfx_bg_tama_depa.
 */
void anim_scene_tama_depa(u8 n)
{
    int i;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    i = 0;
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, gfx_bg_tama_depa);
    anim_scene_extra = room_bath_extra_pics[save.pet.room_designs[3]];
    anim_first_slot_idx = gfx_object_count;
    for (; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_2402872(void)
{
}

/*
 * Animation scene set-up with the fixed background gfx_bg_park.
 */
void anim_scene_park(u8 n)
{
    int i;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    i = 0;
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, gfx_bg_park);
    anim_scene_extra = room_bath_extra_pics[save.pet.room_designs[3]];
    anim_first_slot_idx = gfx_object_count;
    for (; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_2402910(void)
{
}

/*
 * Animation scene set-up without background picture.
 */
void anim_scene_blank(u8 n)
{
    int i;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    i = 0;
    gfx_display_reset();
    anim_scene_extra = room_bath_extra_pics[save.pet.room_designs[3]];
    anim_first_slot_idx = gfx_object_count;
    for (; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_2402998(void)
{
}

/*
 * Animation scene set-up in location room_bath_designs[pet.room_designs[3]].
 */
void anim_scene_bath(u8 n)
{
    int i;
    u8 a;
    u32 v;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    gfx_display_reset();
    switch (save.pet.bath_dirt) {
    case 4:
        a = gfx_object_create(0, 0x1c, 0, 0, 1, 0);
        break;
    case 5:
        a = gfx_object_create(0, 0x1c, 0, 0, 6, 0);
        break;
    default:
        a = gfx_object_create(0, 0x1c, 0, 0, 0, 0);
        break;
    }
    gfx_object_set_image(a, 0, 0, (const u8 *)room_bath_designs[save.pet.room_designs[3]]);
    if (save.pet.life_stage == 1)
        v = (u32)D_2179D68;
    else
        v = room_bath_extra_pics[save.pet.room_designs[3]];
    anim_scene_extra = v;
    anim_first_slot_idx = gfx_object_count;
    for (i = 0; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_2402A78(void)
{
}

/*
 * Animation scene set-up in location room_toilet_designs[pet.room_designs[2]].
 */
void anim_scene_toilet(u8 n)
{
    int i;
    u8 a;
    u32 v;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    gfx_display_reset();
    switch (save.pet.toilet_dirt) {
    case 4:
        a = gfx_object_create(0, 0x1c, 0, 0, 1, 0);
        break;
    case 5:
        a = gfx_object_create(0, 0x1c, 0, 0, 6, 0);
        break;
    default:
        a = gfx_object_create(0, 0x1c, 0, 0, 0, 0);
        break;
    }
    gfx_object_set_image(a, 0, 0, (const u8 *)room_toilet_designs[save.pet.room_designs[2]]);
    if (save.pet.life_stage <= 2)
        v = (u32)D_214F0CC;
    else
        v = (u32)D_214F680;
    anim_scene_fg = v;
    anim_first_slot_idx = gfx_object_count;
    for (i = 0; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_2402B4A(void)
{
}

/*
 * Animation scene set-up in location room_bedroom_designs[pet.x06A], animated background kind 2.
 */
void anim_scene_bedroom(u8 n)
{
    int i;
    u8 a;
    u8 b;

    anim_bg_activity = 2;
    sys_debug_fast_forward = 0;
    gfx_display_reset();
    room_window_draw(2);
    switch (save.pet.bedroom_dirt) {
    case 4:
        a = gfx_object_create(0, 0x1c, 0, 0, 1, 0);
        break;
    case 5:
        a = gfx_object_create(0, 0x1c, 0, 0, 6, 0);
        break;
    default:
        a = gfx_object_create(0, 0x1c, 0, 0, 0, 0);
        break;
    }
    b = gfx_palette_slot_for(a, 0, 0);
    gfx_object_set_image(a, b, 0, (const u8 *)room_bedroom_designs[save.pet.room_designs[4]]);
    anim_scene_bg = room_bed_pics[save.pet.room_designs[2]];
    anim_first_slot_idx = gfx_object_count;
    for (i = 0; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

void func_2402C2A(void)
{
}

/*
 * Palette slot allocation for display object a: returns the first free index in the shared
 * palette tables (0x3C10/0x3D50/0x3E90) after the objects below a; reuses the slot of the
 * previous object if it shows the same image b with the same flags.
 */
u8 gfx_palette_slot_for(u8 a, const u8 *b, u16 c)
{
    if (a == 0)
        return 0;
    do {
        a--;
        if (gfx_objects[a].image != 0) {
            if (b == gfx_objects[a].image && (c & 0xf) == (gfx_objects[a].flags & 0xf))
                return gfx_objects[a].palette_base;
            return gfx_objects[a].palette_base + gfx_objects[a].image[2];
        }
    } while (a != 0);
    return 0;
}

/*
 * Animation-frame primitive: draw the current character (chara_pet_sprite_set) in pose b (0..15:
 * standing, walking, happy, eating... from its sprite set) into animation slot a
 * (relative to the scene's first free object anim_first_slot_idx) with flags c, centred at (d, e) in
 * main-area coordinates (y + 28 = screen). chara_draw_pet is the character renderer, which
 * also places the worn accessories in the next two objects. anim_busy = 1 (no key clicks
 * while animating).
 */
void anim_draw_pet(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    u8 t;

    anim_busy = 1;
    a += anim_first_slot_idx;
    t = gfx_palette_slot_for(a, 0, c);
    chara_draw_pet(a, t, b, c, d, e);
}

void func_2402CE8(void)
{
}

/*
 * Animation API: like anim_draw_pet but for the "other" character selected by
 * chara_select_other_by_id (a visitor / friend / partner: chara_other_sprite_set).
 */
void anim_draw_other_chara(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    u8 t;

    anim_busy = 1;
    a += anim_first_slot_idx;
    t = gfx_palette_slot_for(a, 0, c);
    chara_draw_second(a, t, b, c, d, e);
}

void func_2402D42(void)
{
}

/*
 * Animation API: show a character pose with chara_draw_third in sprite slot a.
 */
void anim_draw_third_chara(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    u8 t;

    anim_busy = 1;
    a += anim_first_slot_idx;
    t = gfx_palette_slot_for(a, 0, c);
    chara_draw_third(a, t, b, c, d, e);
}

void func_2402D9E(void)
{
}

/*
 * Animation API: show a character pose with chara_draw_fourth in sprite slot a.
 */
void anim_draw_fourth_chara(u8 a, u8 b, u16 c, s16 d, s16 e)
{
    u8 t;

    anim_busy = 1;
    a += anim_first_slot_idx;
    t = gfx_palette_slot_for(a, 0, c);
    chara_draw_fourth(a, t, b, c, d, e);
}

void func_2402DFA(void)
{
}

/*
 * Animation-frame primitive (the most frequent call of the generated zone F0): show the
 * still image pos (asset header: [0] = width, [1] = height, then colours, palette,
 * pixels) in sprite slot anim_first_slot_idx + id, flags c (0x10 = mirror, 0x20 = flip,
 * palette/layer bits), centred at (x, y + 0x1c) in main-area coordinates; pos == 0 hides
 * the slot. Slot 0 is normally the 0x40x0x24-centred background.
 */
void anim_draw_pic(u8 id, const u8 *pos, u16 c, s16 x, s16 y)
{
    u8 t;

    anim_busy = 1;
    id += anim_first_slot_idx;
    t = gfx_palette_slot_for(id, pos, c);
    gfx_object_set_flags(id, c);
    func_0000000(&gfx_objects[id]);
    if (pos != 0) {
        gfx_object_set(id, t, 0, pos, pos[0], pos[1]);
        gfx_object_move(id, x - (pos[0] >> 1), y - (pos[1] >> 1) + 0x1c);
    } else {
        gfx_object_set(id, t, 0, pos, 0, 0);
    }
}

void func_2402EA8(void)
{
}

/*
 * Animation-frame primitive: draw pose k of the pet companion (sprite set
 * chara_sprite_sets[pet.companion_sprite_set]) in slot id when one has been adopted (pet.companion_sprite_set != 0, see
 * town_street_encounter), else hide the slot.
 */
void anim_draw_companion(u8 id, u8 k, u16 c, s16 x, s16 y)
{
    u8 t;
    u8 *pos;

    anim_busy = 1;
    id += anim_first_slot_idx;
    t = gfx_palette_slot_for(id, 0, c);
    gfx_object_set_flags(id, c);
    func_0000000(&gfx_objects[id]);
    if (save.pet.companion_sprite_set != 0) {
        pos = (u8 *)((u32 *)&chara_sprite_sets[save.pet.companion_sprite_set])[k];
        gfx_object_set(id, t, 0, pos, pos[0], pos[1]);
        gfx_object_move(id, x - (pos[0] >> 1), y - (pos[1] >> 1) + 0x1c);
    } else {
        gfx_object_set(id, t, 0, 0, 0, 0);
    }
}

/*
 * F40 = animation-frame primitives used by the generated zone (F0) and the game code.
 * Frame sprites are addressed relative to anim_first_slot_idx (the sprite count when the animation
 * started, see anim_scene_room / anim_scene_living); coordinates are centre points, y + 0x1c for
 * the status bar. gfx_palette_slot_for(id, image, c) allocates/looks up the palette for a slot.
 *
 * func_2402F7C: an empty function.
 */
void func_2402F7C(void)
{
}

/*
 * Animation API: show digit m (0..9, frame m of the digit strip: palette image D_2143818,
 * frames D_21435DC) in slot id centred at (x, y) with flags c. The callers draw 5-digit
 * numbers with it, one slot per digit, 9 px apart.
 */
void anim_draw_digit(u8 id, u8 m, u16 c, s16 x, s16 y)
{
    u8 t;
    u8 *pos;

    anim_busy = 1;
    id += anim_first_slot_idx;
    t = gfx_palette_slot_for(id, D_2143818, c);
    gfx_object_set_flags(id, c);
    func_0000000(&gfx_objects[id]);
    pos = D_21435DC;
    gfx_object_set(id, t, m, pos, pos[0], pos[1]);
    gfx_object_move(id, x - (pos[0] >> 1), y - (pos[1] >> 1) + 0x1c);
}

/*
 * An empty function.
 */
void func_2403022(void)
{
}

/*
 * Animation API / frame primitive: text line b (game charset, 2 bytes per glyph) in slot
 * a centred at (d, e) (anim_draw_text_line); c = 1 narrow font (9 px, up to 14 glyphs) / 0 normal
 * (12 px, 12 glyphs). Returns the pointer to the rest of the string (next line) or 0 at
 * the end.
 */
int anim_draw_text(u8 a, const u8 *b, u8 c, s16 d, s16 e)
{
    return (int)anim_draw_text_line(a, b, c, d, e);
}

/*
 * An empty function.
 */
void func_2403048(void)
{
}

/*
 * Animation API: show a text/number b in slot a using the glyph strip D_2147CF4
 * (gfx_draw_glyph_obj).
 */
void anim_draw_glyph(u8 a, int b, u16 c, s16 d, s16 e)
{
    gfx_draw_glyph_obj(a, b, c, d, e);
}

/*
 * An empty function.
 */
void func_240306E(void)
{
}

/*
 * Animation API: queue sound effect a; it is started (snd_play) at the next frame
 * shown by anim_frame / anim_frame_wait.
 */
void anim_queue_sound(int a)
{
    anim_queued_sound = a;
}

/*
 * An empty function.
 */
void func_240308A(void)
{
}

/*
 * Animation API: end of a keyframe: flush the dirty areas to the LCD (func_00009A6), start
 * the queued sound, then wait a milliseconds (T16 ch.5 one-shot, tmr_t5_start_timeout) halting
 * between interrupts; the idle tick keeps the care icon updated meanwhile (ui_call_icon_sync).
 * Bits of b = buttons allowed to skip (1 = A, 2 = B, 4 = C): buttons not in the mask are
 * marked "already pressed" so they are ignored; the allowed A/B are cleared first unless
 * anim_keep_ab_presses is set (C always). A press of an allowed button plays its click (sound 1/2/3;
 * btn_click_played = 1 for A/B) and returns 1; otherwise returns 0 when the time is up. a < 0
 * with b != 0 = wait for a button only.
 */
u8 anim_frame_wait(int a, int b)
{
    if (!(b & 1))
        btn_a_presses = 1;
    else if (anim_keep_ab_presses == 0)
        btn_a_presses = 0;
    if (!(b & 2))
        btn_b_presses = 1;
    else if (anim_keep_ab_presses == 0)
        btn_b_presses = 0;
    if (!(b & 4))
        btn_c_presses = 1;
    else
        btn_c_presses = 0;
    func_00009A6();
    if (anim_queued_sound) {
        snd_play(anim_queued_sound);
        anim_queued_sound = 0;
    }
    if (a < 0 && b != 0) {
        T16_CTL5 = 0;
        tmr_flags &= ~0x20;
    } else {
        if (sys_debug_fast_forward)
            tmr_t5_start_timeout(10);
        else
            tmr_t5_start_timeout(a);
    }
    for (;;) {
        if (btn_a_presses && (b & 1)) {
            snd_play(1);
            btn_click_played = 1;
            anim_busy = 0;
            return 1;
        }
        if (btn_b_presses && (b & 2)) {
            snd_play(2);
            btn_click_played = 1;
            anim_busy = 0;
            return 1;
        }
        if (btn_c_presses && (b & 4)) {
            snd_play(3);
            anim_busy = 0;
            return 1;
        }
        sys_idle_tick();
        if (tmr_flags & 0x20)
            break;
        ui_call_icon_sync();
    }
    tmr_flags &= ~0x20;
    anim_busy = 0;
    return 0;
}

/*
 * An empty function.
 */
void func_24031D4(void)
{
}

/*
 * Animation API "frame(a ms, skip mask b)": the call that ends every keyframe of the
 * generated animation scripts. a = milliseconds (values > 100 have 100 subtracted: a 500 ms
 * frame shows for 400 ms), b = button mask that may cut it short. In normal scenes it is
 * anim_frame_wait; when anim_bg_activity (background activity: 3 = walking character room_blink_poops,
 * other values = the idle animation room_window_animate) is set, the wait is split into 500 ms
 * pieces and the background animation is stepped between them.
 */
u8 anim_frame(int a, int b)
{
    int t, n;

    if (a > 100)
        a -= 100;
    if (anim_bg_activity != 0) {
        for (t = a; t > 0; t -= 500) {
            if (anim_bg_activity == 3)
                room_blink_poops();
            else
                room_window_animate(0);
            n = t;
            if (n > 500)
                n = 500;
            if (anim_frame_wait(n, b))
                return 1;
        }
        return 0;
    }
    return anim_frame_wait(a, b);
}

/*
 * An empty function.
 */
void func_2403254(void)
{
}

/*
 * Set palette entry a of the shared palette from the RGB565 colour b: the renderer keeps
 * three 256-byte tables of 8-bit components, blue gfx_pal_blue (bits 0..4 of b), green
 * gfx_pal_green, red gfx_pal_red (bits 11..15), which the IRAM blitters send to the LCD (3 bytes
 * per pixel, in this order: blue, green, red; the controller uses the top 6 bits); for
 * blue and red the lowest bit is copied into bit 2 (5 -> 6 bits).
 */
void gfx_palette_set(u16 a, u16 b)
{
    gfx_pal_blue[a] = ((b & 0x1f) << 3) | ((b & 1) << 2);
    gfx_pal_green[a] = (b & 0x7e0) >> 3;
    gfx_pal_red[a] = ((b & 0xf800) >> 8) | ((b & 0x800) >> 9);
}

/*
 * An empty function.
 */
void func_24032AE(void)
{
}

/*
 * Background colour: palette entry 0 = a (RGB565: 0xFFFF white, 0xF800 red, 0xE7FC pale
 * green...); mark the play area (rows 0x1C..0x63 = 28..99, columns 0..0x7F) dirty in the
 * first rectangle and the whole screen (main area plus both 28-pixel icon bars) in the
 * second, so the next func_00009A6 repaints everything.
 */
void gfx_set_background(u16 a)
{
    gfx_palette_set(0, a);
    gfx_dirty_main_y0 = 0x1c;
    gfx_dirty_main_y1 = 0x63;
    gfx_dirty_main_x0 = 0;
    gfx_dirty_main_x1 = 0x7F;
    gfx_dirty_bar_y0 = 0;
    gfx_dirty_bar_y1 = 0x7F;
    gfx_dirty_bar_x0 = 0;
    gfx_dirty_bar_x1 = 0x7F;
}

/*
 * An empty function.
 */
void func_2403304(void)
{
}

/*
 * Render the character's pose set a (table chara_pose_sets[a] = {n, {w, h, pose|flags}...}) to a
 * picture file in flash at b (chara_render_poses_to_flash). This is how the character portrait is
 * "photographed" into a TAMAGO-style record (profile exchange / commemorative photo / the
 * record at 0x2050000).
 */
int chara_render_pose_set(int a, struct PictureSetHdr *b)
{
    return chara_render_poses_to_flash((const struct PoseSet *)chara_pose_sets[a], (u8 *)b);
}

u8 f_4C8(s16, s16) __asm__("func_00004C8");
/*
 * Render each pose of a pose list a (width, height, pose code: low nibble = pose, 0x10 =
 * flipped, 0x80 = drawn with pet.worn_clothes cleared) of the current character with its
 * accessories into the frame buffer and PROGRAM the result into flash at b as a picture
 * set: big-endian u16 count, then per picture a big-endian u16 length, w, h, colour count
 * (>= 0x11), 0, 1, 0xFF, the palette (RGB565 big endian, from the rendered sprites via
 * func_000014A, padded with zeros to the colour count) and the pixels, one 8-bit index
 * per pixel read back with func_00004C8. Used to produce the pictures of the own
 * character sent to other toys / the app. Returns the number of bytes written or -1.
 */
int chara_render_poses_to_flash(const struct PoseSet *a, u8 *b)
{
    int i;
    int k;
    int off;
    u16 start;
    int j;
    u8 h[6];
    int z;
    u16 attr;
    int x, y;
    u8 n;
    u8 kept;
    u16 len, v, w;
    u8 *p;

    off = 0;
    sys_irq_disable();
    if (func_0000B52((u16 *)(b + off), a->count << 8, 0x2000000) != 0) {
        off = -1;
        goto out;
    }
    off += 2;
    for (i = 0; i < a->count; i++) {
        h[0] = a->entries[i].w;
        h[1] = a->entries[i].h;
        h[3] = 0;
        h[4] = 1;
        h[5] = 0xff;
        gfx_display_reset();
        z = 0;
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        k = a->entries[i].pose & 0xf;
        if (chara_pet_sprite_set < 100) {
            x = h[0] >> 1;
            y = h[1] - (((u8 *)((u32 *)&chara_sprite_sets[chara_pet_sprite_set])[k])[1] >> 1) - 0x1c;
        } else {
            p = (u8 *)dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, k);
            x = h[0] >> 1;
            y = h[1] - (p[1] >> 1) - 0x1c;
        }
        anim_busy = 1;
        attr = 0x4080;
        if (a->entries[i].pose & 0x10)
            attr += 0x10;
        if (!(a->entries[i].pose & 0x80)) {
            chara_draw_pet(0, 0, k, attr, x, y);
        } else {
            kept = save.pet.worn_clothes;
            save.pet.worn_clothes = 0;
            chara_draw_pet(0, 0, k, attr, x, y);
            save.pet.worn_clothes = kept;
        }
        anim_busy = 0;
        n = gfx_palette_slot_for(3, 0, 0);
        if (n > 0x10)
            h[2] = n;
        else
            h[2] = 0x11;
        start = off;
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[1] << 8 | h[0], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[3] << 8 | h[2], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[5] << 8 | h[4], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        for (j = 0; j < gfx_objects[0].image[2]; j++) {
            v = func_000014A(&gfx_objects[0], j);
            if (func_0000B52((u16 *)(b + off), v >> 8 | (v & 0xff) << 8, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        if (gfx_objects[1].image != 0) {
            for (j = 0; j < gfx_objects[1].image[2]; j++) {
                v = func_000014A(&gfx_objects[1], j);
                if (func_0000B52((u16 *)(b + off), v >> 8 | (v & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        if (gfx_objects[2].image != 0) {
            for (j = 0; j < gfx_objects[2].image[2]; j++) {
                v = func_000014A(&gfx_objects[2], j);
                if (func_0000B52((u16 *)(b + off), v >> 8 | (v & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        for (j = n; j < h[2]; j++) {
            if (func_0000B52((u16 *)(b + off), 0, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        for (y = 0; y < h[1]; y++) {
            for (x = 0; x < h[0]; x += 2) {
                w = f_4C8(x, y);
                w |= f_4C8(x + 1, y) << 8;
                if (func_0000B52((u16 *)(b + off), w, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        len = off - start - 2;
        if (func_0000B52((u16 *)(b + start), (len & 0xff) << 8 | len >> 8, 0x2000000) != 0) {
            off = -1;
            goto out;
        }
    }
out:
    sys_irq_enable();
    return off;
}

/*
 * Like chara_render_pose_set with a per-pose horizontal-flip mask c (bit i set = pose i flipped;
 * chara_render_poses_to_flash_flip).
 */
void chara_render_pose_set_flip(int a, int b, u8 c)
{
    chara_render_poses_to_flash_flip(chara_pose_sets[a], b, c);
}

/*
 * Same as chara_render_poses_to_flash with per-picture flip bits taken from mask c.
 */
int chara_render_poses_to_flash_flip(const struct PoseSet *a, u8 *b, u8 c)
{
    int i;
    int k;
    int off;
    u16 start;
    u8 mask;
    u16 attr;
    u8 h[6];
    int z;
    int x, y;
    u8 n;
    u8 kept;
    u16 len, v, w;
    u8 *p;

    off = 0;
    sys_irq_disable();
    if (func_0000B52((u16 *)(b + off), a->count << 8, 0x2000000) != 0) {
        off = -1;
        goto out;
    }
    off += 2;
    for (i = 0, mask = 1; i < a->count; i++, mask <<= 1) {
        h[0] = a->entries[i].w;
        h[1] = a->entries[i].h;
        h[3] = 0;
        h[4] = 1;
        h[5] = 0xff;
        attr = (c & mask) ? 0x10 : 0;
        gfx_display_reset();
        z = 0;
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        gfx_object_create(z, z, z, z, z, (const u8 *)z);
        k = a->entries[i].pose & 0xf;
        if (chara_pet_sprite_set < 100) {
            x = h[0] >> 1;
            y = h[1] - (((u8 *)((u32 *)&chara_sprite_sets[chara_pet_sprite_set])[k])[1] >> 1) - 0x1c;
        } else {
            p = dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, k);
            x = h[0] >> 1;
            y = h[1] - (p[1] >> 1) - 0x1c;
        }
        anim_busy = 1;
        if (!(a->entries[i].pose & 0x80)) {
            chara_draw_pet(0, 0, k, attr | 0x4080, x, y);
        } else {
            kept = save.pet.worn_clothes;
            save.pet.worn_clothes = 0;
            chara_draw_pet(0, 0, k, attr | 0x4080, x, y);
            save.pet.worn_clothes = kept;
        }
        anim_busy = 0;
        n = gfx_palette_slot_for(3, 0, 0);
        if (n > 0x10)
            h[2] = n;
        else
            h[2] = 0x11;
        start = off;
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[1] << 8 | h[0], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[3] << 8 | h[2], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        if (func_0000B52((u16 *)(b + off), h[5] << 8 | h[4], 0x2000000) != 0) {
            off = -1;
            goto out;
        }
        off += 2;
        for (x = 0; x < gfx_objects[0].image[2]; x++) {
            v = func_000014A(&gfx_objects[0], x);
            if (func_0000B52((u16 *)(b + off), v >> 8 | (v & 0xff) << 8, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        if (gfx_objects[1].image != 0) {
            for (x = 0; x < gfx_objects[1].image[2]; x++) {
                v = func_000014A(&gfx_objects[1], x);
                if (func_0000B52((u16 *)(b + off), v >> 8 | (v & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        if (gfx_objects[2].image != 0) {
            for (x = 0; x < gfx_objects[2].image[2]; x++) {
                v = func_000014A(&gfx_objects[2], x);
                if (func_0000B52((u16 *)(b + off), v >> 8 | (v & 0xff) << 8, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        for (x = n; x < h[2]; x++) {
            if (func_0000B52((u16 *)(b + off), 0, 0x2000000) != 0) {
                off = -1;
                goto out;
            }
            off += 2;
        }
        for (y = 0; y < h[1]; y++) {
            for (x = 0; x < h[0]; x += 2) {
                w = f_4C8(x, y);
                w |= f_4C8(x + 1, y) << 8;
                if (func_0000B52((u16 *)(b + off), w, 0x2000000) != 0) {
                    off = -1;
                    goto out;
                }
                off += 2;
            }
        }
        len = off - start - 2;
        if (func_0000B52((u16 *)(b + start), (len & 0xff) << 8 | len >> 8, 0x2000000) != 0) {
            off = -1;
            goto out;
        }
    }
out:
    sys_irq_enable();
    return off;
}

/*
 * An empty function.
 */
void func_2403BAA(void)
{
}

const u8 D_255C804[] = { 3, 0x30, 0x30, 0x8d, 0x30, 0x48, 0x0d, 0x30, 0x30, 0x00 };
const u8 D_255C810[] = { 3, 0x30, 0x30, 0x00, 0x30, 0x30, 0x01, 0x30, 0x30, 0x09 };
const u8 D_255C81C[] = { 3, 0x30, 0x30, 0x00, 0x30, 0x30, 0x09, 0x30, 0x30, 0x0c };
const u8 D_255C828[] = { 3, 0x30, 0x30, 0x80, 0x30, 0x30, 0x82, 0x30, 0x30, 0x8a };
const u8 D_255C834[] = { 3, 0x30, 0x30, 0x80, 0x30, 0x30, 0x89, 0x30, 0x30, 0x8c };
const u32 chara_pose_sets[5] = {
    (u32)D_255C804, (u32)D_255C810, (u32)D_255C81C, (u32)D_255C828, (u32)D_255C834,
};


/*
 * Render glyphs of text into display object a (asm): character codes are 2 bytes in the
 * game's character set (tools/charset.py), glyph images come from the font strips.
 */
void txt_set_glyph(u8 a, u16 b, u8 *p, u8 flag)
{
    u8 hi = p[0];
    u8 lo = p[1];
    u8 idx;
    u32 img;

    img = flag ? 0x21493b0 : 0x2147244;
    idx = 0x1b;
    if (lo >= 0x51 && lo <= 0x6c) {
        idx = lo - 0x51;
        img = flag ? 0x21493b0 : 0x2147244;
    } else if (lo >= 0x6d && lo <= 0x76) {
        idx = lo - 0x6d;
        img = flag ? 0x2147b80 : 0x2143818;
    } else if (lo >= 0xd0 && lo <= 0xdf) {
        idx = lo - 0xd0;
        img = flag ? 0x214a3f4 : 0x214a640;
    } else if (lo >= 0xe0 && lo <= 0xf9) {
        idx = lo - 0xe0;
        img = flag ? 0x21497ac : 0x2149b60;
    } else if (lo > 0xf9 && lo != 0xff) {
        idx = 0;
        switch (lo) {
        case 0xfa:
            img = flag ? 0x214d55c : 0x214d4fc;
            break;
        case 0xfb:
            img = flag ? 0x214d67c : 0x214d5ec;
            break;
        case 0xfc:
            img = flag ? 0x214d64c : 0x214d58c;
            break;
        case 0xfe:
            img = flag ? 0x214d70c : 0x214d6ac;
            break;
        }
    } else {
        switch (hi) {
        case 8:
            break;
        case 0xc:
            if (lo >= 1 && lo <= 0x10) {
                idx = lo - 1;
                img = flag ? 0x214ab8c : 0x214add8;
            }
            break;
        case 0x10:
            if (lo >= 1 && lo <= 7) {
                idx = lo - 1;
                img = flag ? 0x214d19c : 0x214d2a4;
            }
            break;
        case 0x14:
            if (lo >= 1 && lo <= 7) {
                idx = lo - 1;
                img = flag ? 0x214b324 : 0x214b42c;
            }
            break;
        case 0x18:
            if (lo >= 1 && lo <= 0xc) {
                idx = lo - 1;
                img = flag ? 0x214b684 : 0x214b840;
            }
            break;
        case 0x1c:
            if (lo >= 1 && lo <= 3) {
                idx = lo - 1;
                img = flag ? 0x214bc3c : 0x214bcb4;
            }
            break;
        case 0x20:
            if (lo >= 1 && lo <= 9) {
                idx = lo - 1;
                img = flag ? 0x214bdbc : 0x214bf0c;
            }
            break;
        case 0x24:
            if (lo >= 1 && lo <= 0x21) {
                idx = lo - 1;
                img = flag ? 0x214c20c : 0x214c6bc;
            }
            break;
        default:
            if (lo >= 1 && lo <= 0x50) {
                idx = lo - 1;
                img = flag ? 0x2147cf4 : 0x2143d58;
            } else if (lo >= 0x77 && lo <= 0xc7) {
                idx = lo - 0x77;
                img = flag ? 0x2148840 : 0x21457a4;
            }
            break;
        }
    }
    gfx_object_set_image(a, b, idx, (const u8 *)img);
}

/*
 * Display object id: text/number object using the glyph strip D_2147CF4, centred at
 * (x, y).
 */
void gfx_draw_glyph_obj(u8 id, int s, u16 c, s16 x, s16 y)
{
    u8 t;

    anim_busy = 1;
    id += anim_first_slot_idx;
    t = gfx_palette_slot_for(id, D_2147CF4, c);
    gfx_object_set_flags(id, c);
    func_0000000(&gfx_objects[id]);
    gfx_object_move(id, x - (D_2147CF4[0] >> 1), y - (D_2147CF4[1] >> 1) + 0x1c);
    txt_set_glyph(id, t, s, 0);
}

/*
 * Render one text line s (2 bytes per character in the game's character set) into display
 * object id at (x, y): each glyph is drawn by txt_set_glyph and composed at once into the
 * main-area frame buffer 0x1810 (func_00007A2; no LCD access, the next func_00009A6 sends
 * it), advancing 12 pixels per glyph (9 with the small font,
 * flag != 0); stops at 00xx, at a line break (second byte 0xFF), at xx00 or after n
 * glyphs. The object then becomes one composite text sprite (width = the advance, flag
 * 0x1000: the composer func_0000586 takes such an object's pixels back from the frame
 * buffer instead of an image). Returns the position in s where it stopped (used to print the next line).
 */
u8 *txt_render_line(u8 id, s16 x, s16 y, u16 c, const u8 *s, u8 flag, u8 n)
{
    struct GfxObject *e = &gfx_objects[id];

    e->x = x;
    e->y = y;
    e->flags &= ~0x1010;
    while (s[0] != 0 && s[1] != 0xff && s[1] != 0 && n != 0) {
        txt_set_glyph(id, c, (int)s, flag);
        func_00007A2(*(u8 *)&e->y, *(u8 *)&e->y + e->height,
                     *(u8 *)&e->x, *(u8 *)&e->x + e->width);
        if (flag)
            e->x += 9;
        else
            e->x += 12;
        s += 2;
        n--;
    }
    e->width = *(u8 *)&e->x - x;
    e->x = x;
    e->y = y;
    *(u16 *)&e->palette_base = c;
    e->flags |= 0x1000;
    return s;
}

/*
 * Show a line of text s in display object id (relative to anim_first_slot_idx) centred at (x, y):
 * flag = 0 large font box D_2143D58 (12 glyphs), else small font D_2147CF4 (14 glyphs).
 * Returns the rest of the text (after the line break) or 0 at the end.
 */
u8 *anim_draw_text_line(u8 id, const u8 *s, u8 flag, s16 x, s16 y)
{
    u8 *pos;
    u8 t;

    pos = D_2147CF4;
    if (flag == 0)
        pos = D_2143D58;
    if (s == 0) {
        anim_draw_pic(id, s, 0, x, y);
        return s;
    }
    anim_busy = 1;
    id += anim_first_slot_idx;
    t = gfx_palette_slot_for(id, pos, 0);
    if (flag)
        s = txt_render_line(id, x - (pos[0] >> 1), y - (pos[1] >> 1) + 0x1c, t, s, flag, 14);
    else
        s = txt_render_line(id, x - (pos[0] >> 1), y - (pos[1] >> 1) + 0x1c, t, s, flag, 12);
    if (s[1] == 0xff)
        s += 2;
    if (s[1] == 0)
        s = 0;
    return s;
}

/*
 * Draw a horizontally centred text line s in object a at row b: counts the characters of s
 * (2 bytes each, at most f), centres them on the 128-pixel width (9 or 12 pixels per
 * glyph depending on flag e) and renders with txt_render_line.
 */
void txt_draw_centered(u8 a, s16 b, u8 c, const u8 *s, u8 e, u8 f)
{
    int n;
    u8 *p;
    int w;

    p = s;
    if (p[1] != 0) {
        do {
            p += 2;
            n++;
        } while (p[1] != 0);
    }
    if (n > f)
        n = f;
    w = (0x80 - (e ? n * 9 : n * 12)) >> 1;
    txt_render_line(a, w, b, c, s, e, n);
}

/* the character's pose sets: count, then {width, height, pose} per picture */

/*
 * Animation scene set-up with the background picture gfx_bg_closet.
 */
void anim_scene_closet(u8 n)
{
    int i;
    u8 t;

    anim_bg_activity = 0;
    sys_debug_fast_forward = 0;
    i = 0;
    gfx_display_reset();
    t = gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    gfx_object_set_image(t, 0, 0, gfx_bg_closet);
    anim_first_slot_idx = gfx_object_count;
    for (; i < n; i++)
        gfx_object_create(0, 0x1c, 0, 0, 0, 0);
    ui_draw_icon_bars();
    anim_queued_sound = 0;
}

/*
 * First sprite slot of the current scene (anim_first_slot_idx).
 */
int anim_first_slot(void)
{
    return anim_first_slot_idx;
}

/*
 * The RAM variables this file defines.
 */
volatile u8 sys_debug_fast_forward;
struct GfxObject gfx_objects[48];
u8 gfx_object_count;
u8 gfx_dirty_main_y0;
u8 gfx_dirty_main_y1;
u8 gfx_dirty_main_x0;
u8 gfx_dirty_main_x1;
u8 gfx_dirty_bar_y0;
u8 gfx_dirty_bar_y1;
u8 gfx_dirty_bar_x0;
u8 gfx_dirty_bar_x1;
struct FrameBuffer gfx_frame_buffer;
u8 gfx_pal_blue[320];
u8 gfx_pal_green[320];
u8 gfx_pal_red[320];
volatile u8 anim_busy DSTRAM;
u8 anim_window_mode DSTRAM;
u8 gfx_tint_first DSTRAM;
u8 gfx_tint_end DSTRAM;
u8 anim_queued_sound DSTRAM;
u8 anim_first_slot_idx DSTRAM;
u8 anim_bg_activity DSTRAM;
u8 anim_keep_ab_presses DSTRAM;
u8 lcd_is_st7735 DSTRAM;
