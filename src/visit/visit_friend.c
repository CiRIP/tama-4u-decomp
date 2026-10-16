/*
 * A friend from the friend book comes to visit, with a message scene.
 *
 * Code 2508DC0..2508EEC, .rodata from 2541830.
 */
#include "proto/visit/visit_friend.h"
#include "proto/anim/anim_engine.h"
#include "proto/room/room_main.h"
#include "proto/room/room_window.h"
#include "proto/diary/diary_notebook.h"
#include "proto/mail/mail_letters.h"
#include "proto/anim/anim_scripts.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"

/*
 * A friend from the friend list visits (random friend, picture from the friend banks,
 * message D_2541834). Returns 0 when there are no friends.
 */
int visit_friend(void)
{
    u8 buf[20];
    int n;
    int r;
    u8 k;
    u8 bank;
    u8 *e;
    const u8 **t;

    ui_new_screen();
    if ((n = friend_count()) <= 0)
        return 0;
    memcpy(buf, D_2541834, 0x11);
    r = rand() % n;
    room_idle = 0;
    strncpy((char *)buf, (char *)save.friend_list[r].name, 0x10);
    k = save.friend_list[r].met_flags[1] - 1;
    bank = k / 17 + 9;
    k = k % 17;
    e = flash_sector_addr(bank) + k * FRIEND_PORTRAIT_SIZE;
    room_visitor_scene = 1;
    anim_friend_visit();
    room_visitor_scene = 0;
    t = (const u8 **)&mail_letter_templates;
    visit_friend_message_scene((u8 *)t[rand() % 8], buf, e);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    do {
        if (btn_a_presses != 0)
            break;
        if (btn_b_presses != 0)
            break;
        if (btn_c_presses != 0)
            break;
        sys_idle_tick();
    } while (ui_timeout != 0);
    return 1;
}

/*
 * Friend visit message scene: picture c and up to three text lines a.
 */
void visit_friend_message_scene(u8 *a, u8 *b, u8 *c)
{
    u8 *r;

    anim_scene_room(0xc);
    anim_draw_pic(0, D_2104794, 0, 0x40, 0x24);
    anim_draw_pic(1, c, 0, 0x68, 0x21);
    anim_draw_pic(2, D_211EF88, 0, 0x40, 0x24);
    if (a != 0) {
        r = (u8 *)anim_draw_text(3, a, 1, 0xe, 0xa);
        if (r != 0) {
            r = (u8 *)anim_draw_text(4, r, 1, 0xe, 0x15);
            if (r != 0) {
                r = (u8 *)anim_draw_text(5, r, 1, 0xe, 0x20);
                if (r != 0)
                    anim_draw_text(6, r, 1, 0xe, 0x2b);
            }
        }
    }
    anim_draw_text(7, b, 1, 0xe, 0x36);
    anim_draw_pic(8, 0, 0, 0, 0);
    anim_draw_pic(9, 0, 0, 0, 0);
    anim_draw_pic(10, 0, 0, 0, 0);
    anim_draw_pic(11, 0, 0, 0, 0);
    anim_frame(0x1f4, 0);
}

const u8 D_2541834[] = _("        ");
