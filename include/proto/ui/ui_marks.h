/* Declarations of src/ui/ui_marks.c: its types, variables and functions. */
#ifndef PROTO_UI_UI_MARKS_H
#define PROTO_UI_UI_MARKS_H

#include "structs.h"

struct MarkObjs {
    u8 top_right;
    u8 touch;
    u8 bottom_left;
    u8 special;
    u8 bottom_right;
};

extern struct MarkObjs ui_mark_objs;

void ui_set_mark(u8, int);
void ui_draw_marks(void);
void ui_create_marks(void);

#endif
