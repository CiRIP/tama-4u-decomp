/* Declarations of src/spot/spot_gashapon.c: its types, variables and functions. */
#ifndef PROTO_SPOT_SPOT_GASHAPON_H
#define PROTO_SPOT_SPOT_GASHAPON_H

#include "structs.h"

extern const u8 D_255FB28[];

int spot_gashapon(void);
int spot_gashapon_draw(void);
void spot_gashapon_scene_open(void);
void spot_gashapon_scene_item(void);
void spot_gashapon_scene_points(u16);
void spot_gashapon_scene_message(void);
void ui_scroll_text_with_pet(const u8 *);

#endif
