/*
 * ui_text_banner: a two-line text on a decorated background (the fortune lines).
 *
 * Open: a file of its own (R1?) or the end of anim_scripts.
 *
 * Code 24F51C8..24F51C8, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/ui/ui_text_banner.h"
#include "proto/anim/anim_engine.h"
#include "flash.h"

/*
 * Animation: text p on a background with decorations (two lines, 100 ms frame).
 */
void ui_text_banner(void *p)
{
    anim_scene_room(10);
    anim_draw_pic(0, D_21C335C, 0, 0x40, 0x24);
    anim_draw_pic(1, D_2195EA0, 0, 0x67, 0x29);
    anim_draw_pic(2, D_2195EA0, 0, 0x19, 0x29);
    anim_draw_pic(3, D_2195EA0, 0, 0x67, 0x1b);
    anim_draw_pic(4, D_2195EA0, 0, 0x19, 0x1b);
    anim_draw_pic(5, D_2195EA0, 0, 0x40, 0x1b);
    anim_draw_pic(6, D_2195EA0, 0, 0x40, 0x29);
    anim_draw_pic(7, D_21AC6E8, 0, 0x41, 0x38);
    p = (void *)anim_draw_text(8, p, 0, 0x10, 0x18);
    anim_draw_text(9, p, 0, 0x10, 0x28);
    anim_frame(100, 0);
}
