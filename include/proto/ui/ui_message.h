/* Declarations of src/ui/ui_message.c: its types, variables and functions. */
#ifndef PROTO_UI_UI_MESSAGE_H
#define PROTO_UI_UI_MESSAGE_H

#include "structs.h"

extern u8 ui_message_frames_kept;
extern u8 ui_message_shown;

extern const u32 flash_sector_offsets[];
extern const u32 D_2539144;

u8 ui_bubble_button_pressed(void);
int ui_show_message(int, const u8 *, const u8 *);
int anim_msg_merchant_a(int p0);
int anim_msg_merchant_b(int p0);
int anim_msg_training_a(int p0);
int anim_msg_training_b(int p0);

#endif
