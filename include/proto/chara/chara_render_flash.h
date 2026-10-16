/* Declarations of src/chara/chara_render_flash.c: its types, variables and functions. */
#ifndef PROTO_CHARA_CHARA_RENDER_FLASH_H
#define PROTO_CHARA_CHARA_RENDER_FLASH_H

#include "structs.h"

extern const u32 chara_pic_set_table[];

void comm_gift_sector_init(int);
int comm_render_picture_records(const struct PoseSet *, u8 *, int, u8);
u16 gfx_index_to_palette_slot(u16);
int chara_render_poses_flash(const struct PoseSet *, u8 *);
int chara_render_pic_set(int, struct PictureSetHdr *);
int chara_render_poses_flash_flip(const struct PoseSet *, u8 *, u8);
int chara_render_pic_set_flip(int, int, u8);
u8 gfx_fullscreen_object(const u8 *);
u8 gfx_object_pixel(struct GfxObject *, u8, u8);
u8 gfx_screen_pixel(s16, s16);
void ir_store_received_letter(int);
int sys_battery_low(void);
u8 *album_photo_slot(void);
u8 *album_make_photo(u8, u8 *, u8 *);
int album_photo_write(u8 *, int);
u8 gfx_photo_pixel(s16, s16);
u8 gfx_object_pixel_b(struct GfxObject *, u8, u8);
int ir_send_data_stub(void);
int ir_stub_zero_c(void);
int ir_stub_zero_d(void);
int ir_stub_zero_e(void);
int ir_stub_zero_f(void);
int ir_stub_zero_g(void);
int ir_stub_zero_h(void);
int ir_send_data_stub_b(void);
void func_25029E2(void);
void func_25029F0(void);

#endif
