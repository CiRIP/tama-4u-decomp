/* Declarations of src/anim/anim_engine.c: its types, variables and functions. */
#ifndef PROTO_ANIM_ANIM_ENGINE_H
#define PROTO_ANIM_ANIM_ENGINE_H

#include "structs.h"

struct GfxObjView {
    u32 pic;
    u8 pad004[0x8];
    u8 pal_slot;
};

/* The 128 x 72 frame buffer, one palette index per pixel (also the IR receive buffer). */
struct FrameBuffer {
    u8 x000;
    u8 x001;
    u8 x002[128 * 72 - 2];
};

extern volatile u8 sys_debug_fast_forward; /* debug time fast-forward (P06 strap + C, btn_c_isr) */
extern struct GfxObject gfx_objects[];
extern struct GfxObject gfx_object4;
extern u8 gfx_object_count;
/* dirty rectangles flushed to the LCD by func_00009A6: main area gfx_dirty_main_y0..D_0001808 =
 * y0, y1, x0, x1 (rows 28..99), icon bars gfx_dirty_bar_y0..D_000180C = y0, y1, x0, x1; empty =
 * 0x7F, 0, 0x7F, 0 */
extern u8 gfx_dirty_main_y0;
extern u8 gfx_dirty_main_y1;
extern u8 gfx_dirty_main_x0;
extern u8 gfx_dirty_main_x1;
extern u8 gfx_dirty_bar_y0;
extern u8 gfx_dirty_bar_y1;
extern u8 gfx_dirty_bar_x0;
extern u8 gfx_dirty_bar_x1;
extern struct FrameBuffer gfx_frame_buffer;
extern u8 gfx_pal_blue[];
extern u8 gfx_pal_green[];
extern u8 gfx_pal_red[];
extern volatile u8 anim_busy;
extern u8 anim_window_mode;
extern u8 gfx_tint_first;
extern u8 gfx_tint_end;
/* sound id queued by anim_queue_sound, played with the next frame (anim_frame_wait) */
extern u8 anim_queued_sound;
extern u8 anim_first_slot_idx;
extern u8 anim_bg_activity;
extern u8 anim_keep_ab_presses;
/* LCD strap P07 at boot (lcd_init): 1 = high = ST7735 panel (flush window offset
 * column +2, row +1), 0 = low = ILI9163(C)-type panel (no offset) */
extern u8 lcd_is_st7735;

extern const u32 chara_pose_sets[];

void lcd_init(void);
void gfx_display_reset(void);
void gfx_set_tint_range(u8);
void gfx_object_set(u8, u16, u8, const u8 *, u8, u8);
void gfx_object_set_image(u8, u16, u8, const u8 *);
void gfx_object_set_image_attr(u8, u8, const u8 *);
void gfx_object_set_image_clipped(u8, u16, u8, const u8 *, u8, u8);
void gfx_object_move(u8, s16, s16);
void gfx_object_move_dirty(u8, s16, s16);
s8 gfx_object_create(s16 x, s16 y, u16 c, u8 d, u16 flags, const u8 *img);
void gfx_object_draw_glyph(u8, u16, int, u8);
int gfx_draw_text_line(u8, s16, s16, u16, int, int, int);
void gfx_draw_text_centered(u8, s16, u8, const u8 *, u8, u8);
void gfx_object_set_flags(u8, u16);
void anim_scene_room(u8);
void anim_scene_window(u8);
void anim_scene_window_mode(u8, u8);
void anim_draw_fifth_chara(u8, u8, u16, s16, s16);
void anim_scene_window2(u8);
void anim_scene_living(u8);
void anim_scene_living_poops(u8);
void anim_scene_kitchen(u8);
void anim_scene_game_corner(u8);
void anim_scene_tama_depa(u8);
void anim_scene_park(u8);
void anim_scene_blank(u8);
void anim_scene_bath(u8);
void anim_scene_toilet(u8);
void anim_scene_bedroom(u8);
u8 gfx_palette_slot_for(u8, const u8 *, u16);
void anim_draw_pet(u8, u8, u16, s16, s16);
void anim_draw_other_chara(u8, u8, u16, s16, s16);
void anim_draw_third_chara(u8, u8, u16, s16, s16);
void anim_draw_fourth_chara(u8, u8, u16, s16, s16);
void anim_draw_pic(u8, const u8 *, u16, s16, s16);
void anim_draw_companion(u8, u8, u16, s16, s16);
void anim_draw_digit(u8, u8, u16, s16, s16);
int anim_draw_text(u8, const u8 *, u8, s16, s16);
void anim_draw_glyph(u8, int, u16, s16, s16);
void anim_queue_sound(int);
u8 anim_frame_wait(int, int);
u8 anim_frame(int, int);
void gfx_palette_set(u16, u16);
void gfx_set_background(u16);
int chara_render_pose_set(int, struct PictureSetHdr *);
int chara_render_poses_to_flash(const struct PoseSet *, u8 *);
void chara_render_pose_set_flip(int, int, u8);
int chara_render_poses_to_flash_flip(const struct PoseSet *, u8 *, u8);
void txt_set_glyph(u8 a, u16 b, u8 *p, u8 flag);
void gfx_draw_glyph_obj(u8, int, u16, s16, s16);
u8 *txt_render_line(u8, s16, s16, u16, const u8 *, u8, u8);
u8 *anim_draw_text_line(u8, const u8 *, u8, s16, s16);
void txt_draw_centered(u8, s16, u8, const u8 *, u8, u8);
void anim_scene_closet(u8);
int anim_first_slot(void);

#endif
