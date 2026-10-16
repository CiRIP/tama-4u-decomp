/* Declarations of src/comm/comm_record_pics.c: its types, variables and functions. */
#ifndef PROTO_COMM_COMM_RECORD_PICS_H
#define PROTO_COMM_COMM_RECORD_PICS_H

#include "structs.h"


extern const u16 test_sample_tamago[];
extern const u32 D_253B700;

void test_program_sample_tamago(void);
void comm_fill_record(int, int, struct PictureSetHdr *);
void ir_anim_pause_a(u8);
void ir_anim_pause_b(u8);
void ir_anim_pause_c(u8);
void ir_clear_flags(void);
int ir_pause_short(void);
void comm_gift_bonus(void);
void comm_clear_pose_list(u8 *);
int comm_render_pose_list(int, u8 *, int);
int comm_render_poses_or_room(int, u8 *, int);
int comm_render_record_poses(int, int, int);

#endif
