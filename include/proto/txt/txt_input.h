/* Declarations of src/txt/txt_input.c: its types, variables and functions. */
#ifndef PROTO_TXT_TXT_INPUT_H
#define PROTO_TXT_TXT_INPUT_H

#include "structs.h"

extern u8 txt_input_charset;
extern u8 txt_input_char_objs[];
extern u8 txt_input_grid_objs[];
extern u8 txt_input_max_len;
extern s8 txt_input_pos;
extern u8 txt_input_page_count;
extern u8 txt_input_grid_len;
extern char txt_input_grid_chars[];
extern u8 txt_input_hold_count;
extern u8 txt_input_prompt;

extern const u32 D_253B0C8;
extern const u32 D_253B13C;
extern const u32 D_253B150;
extern const u32 D_253B164;
extern const u32 D_253B178;
extern const u32 D_253B18C;
extern const u32 D_253B1A0;
extern const u32 D_253B1B4;
extern const u32 D_253B1C8;
extern const u32 D_253B1E0;

int txt_input(int, u8, int);
int txt_input_confirm(u8, int);
int txt_input_pick_char(int, u8);
int txt_input_cursor_loop(int);
void txt_input_load_grid(u8);

#endif
