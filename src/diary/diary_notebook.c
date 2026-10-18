/*
 * The notebook: picture-diary pages (composition, rasterising, storage, viewer), the friend
 * book with profiles, messages and farewells, the mail list, the photo album, and the notebook
 * menu.
 *
 * Code 241628E..241AD5C, .rodata from 255D614.
 */
#include "proto/diary/diary_notebook.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/chara/chara_draw.h"
#include "proto/food/food_meals.h"
#include "proto/town/town_places.h"
#include "proto/shop/shop_reform.h"
#include "proto/album/album_store.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/ir/ir_receive.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/owner/owner_profile.h"
#include "proto/spot/spot_reader.h"
#include "proto/mail/mail_edit.h"
#include "proto/item/item_record_browser.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/comm/comm_outcome.h"
#include "flash.h"
#include "libc.h"

/*
 * F53 = the notebooks ("てちょう") menu ui_notebook_menu, table ui_notebook_menu_tbl: たまともてちょう
 * (Tama-friends book: the character species met, records of 0x1000 bytes in sector 0x37 =
 * 0x22F0000; album_browser), ともだちてちょう (friend book: the 30 friend records
 * friend_list with their profile / message editor / message history / farewell sub-menu),
 * きねんしゃしん (photo album: 6 slots of 0x2800 bytes at 0x2770000), こそだてちょう
 * (handler ui_notebook_childcare -> dl_chara_list: the touch stamp card and the downloaded
 * characters, F20b), きせつかざり (handler ui_notebook_decorations). The picture diary "えにっき":
 * 23 pages of 0x2000 bytes at 0x2570000..0x259E000, sectors 0x5F..0x61, scratch copy at
 * 0x2790000 sectors 0x81..0x83 - which also hold the downloaded-character slots 2..7
 * (0x2780000 + 0x8000 k, dl_chara_file): a diary delete would overwrite them, but its
 * users (the viewer diary_viewer, diary_drop_oldest) have no caller. Mail ("メッセ") lives at 0x22D0000 (sector 0x35): 100
 * records of 100 bytes {s32 friend index, s32 kind (0/1/2, -1 = our draft), u8 sender
 * name[0x14], u8 body[0x40], pad[8]}; sector 0xC (0x2040000) is the scratch copy used to
 * rewrite it.
 */

/*
 * Number of used display objects among the first n (index of the last non-empty sprite
 * slot + 1); the diary pages store the sprite list.
 */
u8 gfx_count_used_objects(u8 n)
{
    int i;

    for (i = n - 1; i >= 0; i--)
        if (gfx_objects[i].image != 0)
            return i + 1;
    return 0;
}

/*
 * Copy diary text template k (table diary_templates, 0x4B templates) into dst; returns its
 * length.
 */
int diary_copy_template(char *dst, int k)
{
    strcpy(dst, diary_templates[k]);
    return strlen(diary_templates[k]);
}

/*
 * Picture-diary page compositions (diary_page_meal_a .. diary_page_bath, chosen by diary_compose_page
 * by event code and variant): each builds the sprite list of one picture-diary scene with
 * the animation API from the event's images (a = background, anim_pic_slots[..] = the partner
 * / character images, D_20F5384 / D_20F4E14 / D_20F5324 = the page frame and corner
 * decorations; room, table, outdoor, location backgrounds), with the character pose via
 * anim_draw_pet, and returns the number of sprite slots used so diary_rasterize_page can
 * rasterise and store the page.
 * This one: event 2000 variant 0, a meal with the partners ("A と B はなかよしだね↑ C を
 * たべたよ"; no writer of event 2000 in this firmware): table scene, pet pose 6.
 */
u8 diary_page_meal_a(const u8 *a)
{
    u32 t = anim_scene_bg;

    anim_scene_room(0x17);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_scene_bg = t;
    anim_draw_pic(1, (u8 *)t, 0, 0x61, 0x39);
    anim_draw_pic(2, anim_pic_slots[4][0], 0, 0x60, 0x29);
    anim_draw_pic(3, D_217C9CC, 0x10, 0x4c, 0x3b);
    anim_draw_pic(4, anim_pic_slots[1][1], 0x10, 0x4f, 0x28);
    anim_draw_pic(5, D_217C9CC, 0, 0x74, 0x3b);
    anim_draw_pet(6, 6, 0, 0x71, 0x2e);
    anim_draw_pic(9, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(10, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(11, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x17);
}

/*
 * Diary page of event 2000 variant 1, the meal with the partners (see diary_page_meal_a).
 */
u8 diary_page_meal_b(const u8 *a)
{
    u32 t = anim_scene_bg;

    anim_scene_room(0x18);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_scene_bg = t;
    anim_draw_pic(1, (u8 *)t, 0, 0x61, 0x39);
    anim_draw_pic(2, anim_pic_slots[4][0], 0, 0x60, 0x29);
    anim_draw_pic(3, D_217C9CC, 0x10, 0x4c, 0x3b);
    anim_draw_pic(4, anim_pic_slots[1][2], 0x10, 0x4f, 0x28);
    anim_draw_pic(5, D_217C9CC, 0, 0x74, 0x3b);
    anim_draw_pet(6, 2, 0, 0x71, 0x1d);
    anim_draw_pic(9, D_214E810, 0, 0x73, 0x37);
    anim_draw_pic(10, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(11, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(12, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x18);
}

/*
 * Diary page of event 2000 variant 2, the meal with the partners (see diary_page_meal_a).
 */
u8 diary_page_meal_c(const u8 *a)
{
    u32 t = anim_scene_bg;

    anim_scene_room(0x17);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_scene_bg = t;
    anim_draw_pic(1, (u8 *)t, 0, 0x61, 0x39);
    anim_draw_pic(2, anim_pic_slots[4][0], 0, 0x60, 0x29);
    anim_draw_pic(3, D_217C9CC, 0x10, 0x4c, 0x3b);
    anim_draw_pic(4, anim_pic_slots[0][1], 0x10, 0x4f, 0x2f);
    anim_draw_pic(5, D_217C9CC, 0, 0x74, 0x3b);
    anim_draw_pet(6, 3, 0, 0x71, 0x35);
    anim_draw_pic(9, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(10, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(11, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x17);
}

/*
 * Diary page of the school club classes, events 4000 (inventions), 4001 (fashion), 4003
 * (arts), 4004 (cooking): the pet, the teacher and the class picture.
 */
u8 diary_page_school_lesson(const u8 *a)
{
    anim_scene_room(0x17);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_draw_third_chara(1, 2, 0x10, 0x5f, 0x24);
    anim_draw_pic(4, anim_pic_slots[0][0], 0x10, 0x4f, 0x27);
    anim_draw_pet(5, 2, 0, 0x71, 0x35);
    anim_draw_pic(8, anim_pic_slots[2][2], 0, 0x58, 0x3b);
    anim_draw_pic(9, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(10, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(11, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x17);
}

/*
 * Diary page of event 2002 (school with the partner) and of the sports class 4002: the pet
 * and a second character.
 */
u8 diary_page_school_friend(const u8 *a)
{
    anim_scene_room(0x17);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_draw_pic(1, anim_pic_slots[3][0], 0x10, 0x5f, 0x24);
    anim_draw_pic(2, 0, 0, 0, 0);
    anim_draw_pic(3, 0, 0, 0, 0);
    anim_draw_pic(4, anim_pic_slots[0][0], 0x10, 0x4f, 0x27);
    anim_draw_pet(5, 2, 0, 0x71, 0x35);
    anim_draw_pic(8, anim_pic_slots[2][2], 0, 0x58, 0x3b);
    anim_draw_pic(9, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(10, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(11, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x17);
}

/*
 * Diary page of event 2003 variant 1 (the nursery drawing, "みんなでおえかき"): an empty composer (`ret`), the page gets no picture.
 */
u8 diary_page_nursery_drawing(void)
{
}

/*
 * Diary page of event 2003 variant 0 (the nursery, "ほいくえんへいったよ!"): an empty composer (`ret`), the page gets no picture.
 */
u8 diary_page_nursery_story(void)
{
}

/*
 * Diary page of events 1000..1002 variant 0, something made ("...をつくったよ" / "...が
 * できたよ"; no writer): the pet, the partners and the made thing b.
 */
u8 diary_page_make_success(const u8 *a, const u8 *b)
{
    anim_scene_room(0x15);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_draw_pic(1, anim_pic_slots[1][0], 0, 0x4f, 0x1a);
    anim_draw_pic(2, anim_pic_slots[2][0], 0, 0x71, 0x1a);
    anim_draw_pic(3, anim_pic_slots[0][0], 0, 0x50, 0x31);
    anim_draw_pet(4, 2, 0, 0x70, 0x37);
    anim_draw_pic(7, (u8 *)b, 0, 0x5f, 0x2c);
    anim_draw_pic(8, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(9, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(10, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x15);
}

/*
 * Diary page of events 1000..1002 variant 1, a failure ("...でだいしっぱい"; no writer).
 */
u8 diary_page_make_failure(const u8 *a, const u8 *b)
{
    anim_scene_room(0x15);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_draw_pic(1, anim_pic_slots[1][1], 0, 0x4f, 0x1a);
    anim_draw_pic(2, anim_pic_slots[2][1], 0, 0x71, 0x1a);
    anim_draw_pic(3, anim_pic_slots[0][1], 0, 0x50, 0x31);
    anim_draw_pet(4, 3, 0, 0x70, 0x37);
    anim_draw_pic(7, (u8 *)b, 0, 0x5f, 0x2c);
    anim_draw_pic(8, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(9, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(10, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x15);
}

/*
 * Diary page of events 1003 / 1004 variants 0..2 (playing together; no writer): an empty composer (`ret`), the page gets no picture.
 */
u8 diary_page_play_a(const u8 *a)
{
}

/*
 * Diary page of events 1003 / 1004 variants 3.. (no writer): an empty composer (`ret`), the page gets no picture.
 */
u8 diary_page_play_b(void)
{
}

/*
 * Diary page of the Tama Depa job, event 3001: an empty composer (`ret`), the page gets no picture.
 */
u8 diary_page_job_depa(void)
{
}

/*
 * Diary page of the Tama-mori shop job, event 3002: an empty composer (`ret`), the page gets no picture.
 */
u8 diary_page_job_tamamori(void)
{
}

/*
 * Diary page of the music cafe job, event 3003: an empty composer (`ret`), the page gets no picture.
 */
u8 diary_page_job_cafe(void)
{
}

/*
 * Diary page of event 2000 variant 3, the meal in the location background.
 */
u8 diary_page_meal_d(const u8 *a)
{
    u32 t = anim_scene_bg;

    anim_scene_kitchen(0x16);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_scene_bg = t;
    anim_draw_pic(1, (u8 *)t, 0, 0x61, 0x39);
    anim_draw_pic(2, anim_pic_slots[4][0], 0, 0x60, 0x29);
    anim_draw_pic(3, D_217C9CC, 0x10, 0x4c, 0x3b);
    anim_draw_pic(4, anim_pic_slots[1][1], 0x10, 0x4f, 0x28);
    anim_draw_pic(5, D_217C9CC, 0, 0x74, 0x3b);
    anim_draw_pet(6, 6, 0, 0x71, 0x2e);
    anim_draw_pic(9, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(10, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x16);
}

/*
 * Diary page of event 2000 variant 4, the meal in the location background.
 */
u8 diary_page_meal_e(const u8 *a)
{
    u32 t = anim_scene_bg;

    anim_scene_kitchen(0x17);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_scene_bg = t;
    anim_draw_pic(1, (u8 *)t, 0, 0x61, 0x39);
    anim_draw_pic(2, anim_pic_slots[4][0], 0, 0x60, 0x29);
    anim_draw_pet(3, 2, 0, 0x71, 0x1d);
    anim_draw_pic(6, D_217C9CC, 0x10, 0x4c, 0x3b);
    anim_draw_pic(7, anim_pic_slots[1][2], 0x10, 0x4f, 0x28);
    anim_draw_pic(8, D_217C9CC, 0, 0x74, 0x3b);
    anim_draw_pic(9, D_214E810, 0, 0x73, 0x37);
    anim_draw_pic(10, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(11, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x17);
}

/*
 * Diary page of event 2000 variant 5, the meal in the location background.
 */
u8 diary_page_meal_f(const u8 *a)
{
    u32 t = anim_scene_bg;

    anim_scene_kitchen(0x16);
    anim_draw_pic(0, (u8 *)a, 0, 0x40, 0x24);
    anim_scene_bg = t;
    anim_draw_pic(1, (u8 *)t, 0, 0x61, 0x39);
    anim_draw_pic(2, anim_pic_slots[4][0], 0, 0x60, 0x29);
    anim_draw_pic(3, D_217C9CC, 0x10, 0x4c, 0x3b);
    anim_draw_pic(4, anim_pic_slots[0][1], 0x10, 0x4f, 0x2f);
    anim_draw_pic(5, D_217C9CC, 0, 0x74, 0x3b);
    anim_draw_pet(6, 3, 0, 0x71, 0x35);
    anim_draw_pic(9, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(10, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x16);
}

/*
 * Diary page of event 2001 variant 0, the park climbing frame (no writer; animated
 * background kind 2).
 */
u8 diary_page_park_athletic(void)
{
    anim_scene_window2(0x16);
    anim_draw_pic(0, gfx_bg_park, 0, 0x40, 0x24);
    anim_draw_pic(1, D_2182770, 0x10, 0x4a, 0x2a);
    anim_draw_pic(2, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(3, D_2183688, 0, 0x4c, 0x2f);
    anim_draw_pic(4, D_2183688, 0, 0x67, 0x2f);
    anim_draw_pet(5, 7, 0, 0x50, 0x2f);
    anim_draw_pic(8, anim_pic_slots[1][2], 0, 0x6b, 0x29);
    anim_draw_pic(9, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(10, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x16);
}

/*
 * Diary page of event 2001 variant 1, the sand mountain in the park (no writer).
 */
u8 diary_page_park_sand(void)
{
    anim_scene_window2(0x16);
    anim_draw_pic(0, gfx_bg_park, 0, 0x40, 0x24);
    anim_draw_pic(1, D_2181CA8, 0, 0x5e, 0x39);
    anim_draw_pic(2, D_2182564, 0, 0x60, 0x31);
    anim_draw_pic(3, D_2182304, 0x10, 0x44, 0x36);
    anim_draw_pic(4, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pet(5, 2, 0, 0x68, 0x1f);
    anim_draw_pic(8, anim_pic_slots[1][2], 0x10, 0x4f, 0x28);
    anim_draw_pic(9, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(10, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x16);
}

/*
 * Diary page of event 2001 variant 2, the park slide (no writer).
 */
u8 diary_page_park_slide(void)
{
    anim_scene_window2(0x14);
    anim_draw_pic(0, gfx_bg_park, 0, 0x40, 0x24);
    anim_draw_pet(1, 7, 0, 0x4f, 0x29);
    anim_draw_pic(4, anim_pic_slots[2][0], 0, 0x7a, 0x27);
    anim_draw_pic(5, D_2181264, 0, 0x5d, 0x2a);
    anim_draw_pic(6, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(7, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x14);
}

/*
 * Diary page of the park patrol job, event 3000 (the park with whistle and broom).
 */
u8 diary_page_job_patrol(void)
{
    anim_scene_window2(0x14);
    anim_draw_pic(0, gfx_bg_park, 0, 0x40, 0x24);
    anim_draw_pet(1, 2, 0, 0x62, 0x30);
    anim_draw_pic(4, D_219ABC0, 0x10, 0x49, 0x2a);
    anim_draw_pic(5, D_219ACF0, 0, 0x77, 0x2d);
    anim_draw_pic(6, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(7, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(8, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x14);
}

/*
 * Diary page of event 2005 variant 0, together with the partner outdoors (no writer).
 */
u8 diary_page_together_a(void)
{
    anim_scene_window2(0x13);
    anim_draw_pic(0, anim_pic_slots[2][1], 0, 0x60, 0x28);
    anim_draw_pic(1, anim_pic_slots[0][0], 0x10, 0x4f, 0x2f);
    anim_draw_pet(2, 2, 0, 0x71, 0x35);
    anim_draw_pic(5, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(6, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(7, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x13);
}

/*
 * Diary page of event 2005 variant 1, together with the partner at home (no writer;
 * background kind 3).
 */
u8 diary_page_together_b(void)
{
    anim_scene_living_poops(0x13);
    anim_draw_pic(0, anim_pic_slots[2][1], 0, 0x60, 0x28);
    anim_draw_pic(1, anim_pic_slots[0][0], 0x10, 0x4f, 0x2f);
    anim_draw_pet(2, 2, 0, 0x71, 0x35);
    anim_draw_pic(5, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(6, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(7, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x13);
}

/*
 * Diary page of event 2007, playing in the garden with the partner (no writer).
 */
u8 diary_page_garden(void)
{
    anim_scene_living_poops(0x12);
    anim_draw_pic(0, anim_pic_slots[0][0], 0, 0x71, 0x26);
    anim_draw_pet(1, 0xd, 0, 0x56, 0x3d);
    anim_draw_pic(4, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(5, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(6, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x12);
}

/*
 * Diary page of event 2006, written at the end of a partner's visit (diary_write_comm_page):
 * "...といっしょだとたのしいね♪"; home room, the partner and the pet (pose 0xD).
 */
u8 diary_page_comm(void)
{
    anim_scene_living_poops(0x12);
    anim_draw_pic(0, anim_pic_slots[0][0], 0, 0x71, 0x26);
    anim_draw_pet(1, 0xd, 0, 0x56, 0x32);
    anim_draw_pic(4, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(5, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(6, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x12);
}

/*
 * Diary page of event 2004, the bath with the partner (no writer): the bathroom
 * pet.room_designs[3].
 */
u8 diary_page_bath(void)
{
    anim_scene_bath(0x15);
    anim_draw_pet(0, 10, 0, 0x52, 0x2b);
    anim_draw_pic(3, anim_pic_slots[0][0], 0, 0x70, 0x25);
    anim_draw_pic(4, (u8 *)anim_scene_extra, 0, 0x60, 0x3c);
    anim_draw_pic(5, (u8 *)&D_214DEC0, 0, 0x50, 0x16);
    anim_draw_pic(6, (u8 *)&D_214DEC0, 0, 0x6e, 0x16);
    anim_draw_pic(7, D_20F5384, 0, 0x40, 0x24);
    anim_draw_pic(8, D_20F4E14, 0, 0x5a, 9);
    anim_draw_pic(9, D_20F5324, 0, 0x79, 9);
    return gfx_count_used_objects(0x15);
}

/*
 * Empty loop over the 31 friend records (optimised-away body, probably a debug counter;
 * likewise friend_loop_noop_b and friend_loop_noop_c).
 */
void friend_loop_noop_a(void)
{
    int i;

    for (i = 0; i < 0x1f; i++)
        ;
}

/*
 * Portrait slot of friend k: the stored index x015 - 1 (save.friend_list[k].met_flags[1] - 1) when
 * assigned, else the first free 0xE80-byte slot in the three portrait sectors 9..11 (17
 * per sector, flash_first_erased_record scans for an erased record), 0 when all are full.
 */
u8 friend_portrait_slot(int k)
{
    int i;
    int r;

    if (save.friend_list[k].met_flags[1] != 0)
        return save.friend_list[k].met_flags[1] - 1;
    for (i = 0; i <= 2; i++) {
        r = flash_first_erased_record(flash_sector_addr(i + FLASH_SEC_PORTRAITS), FRIEND_PORTRAIT_SIZE, 0x11);
        if (r >= 0) {
            r += i * 17;
            break;
        }
    }
    if (i == 3)
        r = 0;
    return r;
}

/*
 * Empty loop over the 31 friend records (see friend_loop_noop_a).
 */
void friend_loop_noop_b(void)
{
    int i;

    for (i = 0; i < 0x1f; i++)
        ;
}

/*
 * Empty loop over the 31 friend records (see friend_loop_noop_a), returns 0.
 */
int friend_loop_noop_c(void)
{
    int i, r = 0;

    for (i = 0; i < 0x1f; i++)
        ;
    return r;
}

/*
 * Number of friends in the book (used records among the 30 of friend_list).
 */
int friend_count(void)
{
    int n = 0;
    int i;

    for (i = 0; i <= 0x1d; i++)
        if (save.friend_list[i].x000_b != 0)
            n++;
    return n;
}

/*
 * "ほんとうに?" (really?) confirmation (D_255EF18), default yes.
 */
u8 ui_confirm_really(void)
{
    return ui_confirm_yes_no(D_255EF18, 1);
}

/*
 * "おわかれ?" (say goodbye?) + "ほんとうに?": delete friend k: free its portrait record
 * (rewrite the portrait sector without it, flash_restore_sector_except), shift the following friend
 * records down, save. 1 = deleted. Its mail is deleted by the callers (friend_page,
 * friend_book).
 */
u8 friend_delete(int k)
{
    u8 *q;
    int i;
    u8 m, t;
    u8 *p;

    ui_question_do_screen((int)D_255EF28, 1);
    if (ui_two_choice_run(1) != 0) {
        if (ui_confirm_really() != 0) {
            snd_wait_end();
            if (comm_friendbook_updated == 2)
                comm_info_screen(0);
            friend_portrait_backup(k);
            if (save.friend_list[k].met_flags[1] != 0) {
                m = save.friend_list[k].met_flags[1] - 1;
                snd_wait_end();
                t = m / 17 + FLASH_SEC_PORTRAITS;
                m = m % 17;
                q = flash_sector_addr(t);
                q += m * 0xe80;
                flash_restore_sector_except(t, q, 0xe80);
            }
            for (i = k; i <= 0x1d; i++)
                memcpy(&save.friend_list[i], &save.friend_list[i + 1], 0x16);
            p = (u8 *)save.friend_list;
            ((struct Friend *)p)[i].x000_b = 0;
            p++;
            ((struct Friend *)p)[i].met_flags[0] = 0;
            snd_wait_end();
            func_25039AA();
            anim_busy = 0;
            save_game();
            return 1;
        }
        if (ui_timeout == 0)
            return 0;
    }
    snd_wait_end();
    return 0;
}

/*
 * Draw friend idx's portrait centred at (x, y) in sprite slot/layer c: its stored 0xE80
 * record from the portrait sectors (or the placeholder at 0x226BC00 when erased), else the
 * generic sprite of its species (chara_select_other_by_id loads it, chara_sprite_sets[chara_other_sprite_set].pose13).
 */
void friend_draw_portrait(int idx, u8 x, u8 y, u8 c)
{
    u8 *s;
    u8 k;
    u8 bank;

    if (save.friend_list[idx].met_flags[1] != 0) {
        k = save.friend_list[idx].met_flags[1] - 1;
        bank = k / 17 + FLASH_SEC_PORTRAITS;
        k = k % 17;
        s = flash_sector_addr(bank) + k * FRIEND_PORTRAIT_SIZE;
        if (*(int *)s == -1)
            s = *friend_portrait_placeholder;
    } else {
        chara_select_other_by_id(save.friend_list[idx].gender);
        s = (u8 *)chara_sprite_sets[chara_other_sprite_set].pose13;
    }
    if (s[1] == 0x30)
        y -= 3;
    gfx_object_create(x - (s[0] >> 1), y - s[1], c, 0, 0, (const u8 *)s);
}

/*
 * "おともだちからでんごんだよ～" (a message from a friend!): picks a random friend and
 * builds the notice D_255EF34 with its name, then waits up to 10 s / until a button (no
 * drawing call is left in this function). Returns 1, 0 when there are no friends.
 */
int mail_notice_from_friend(void)
{
    u8 buf[36];
    int n;
    int r;
    u8 k;

    if ((n = friend_count()) <= 0)
        return 0;
    memcpy(buf, D_255EF34, 0x21);
    r = rand() % n;
    strncpy((char *)buf, (char *)save.friend_list[r].name, 0x10);
    k = save.friend_list[r].met_flags[1] - 1;
    flash_sector_addr((u8)(k / 17 + FLASH_SEC_PORTRAITS));
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
 * Friend a's sub-menu (cursor on b): プロフィール (profile) / メッセへんしゅう (edit
 * message) / メッセりれき (message history, greyed when the friend has no messages of kinds
 * 0/1/2: mail_friend_msg_count = their count) / おわかれ (farewell); A moves the cursor, B returns the
 * entry (0..3), C / timeout -1.
 */
int friend_submenu(int a, int b)
{
    u8 buf[100];
    int cnt;
    int i;
    u8 d, e;
    u8 c;
    u8 sel;

    save.mail_friend_msg_count = 0;
    for (i = 0, cnt = 100; i <= 0x63; i++)
        if (mail_find(a, 0xe, 0, -1, (int)&cnt) >= 0)
            save.mail_friend_msg_count++;
    if (save.mail_friend_msg_count != 0)
        cnt = 0;
    else
        cnt = 1;
    sel = b;
    do {
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
        switch (cnt) {
        case 0:
            d = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
            gfx_draw_text_line(d, 0x18, 0x20, 0x10, (int)D_255EF58, 0, 8);
            d = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
            gfx_draw_text_line(d, 0x18, 0x30, 0x10, (int)D_255EF68, 0, 8);
            d = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
            gfx_draw_text_line(d, 0x18, 0x40, 0x10, (int)D_255EF7C, 0, 8);
            break;
        case 1:
            e = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
            gfx_draw_text_line(e, 0x18, 0x20, 0x10, (int)D_255EF58, 0, 8);
            e = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
            gfx_draw_text_line(e, 0x18, 0x30, 0x10, (int)D_255EF68, 0, 8);
            e = gfx_object_create(0x18, 0x20, 0x14, 0, 3, 0);
            gfx_draw_text_line(e, 0x18, 0x40, 0x14, (int)D_255EF7C, 0, 8);
            break;
        default:
            d = gfx_object_create(0x18, 0x20, 0x14, 0, 3, 0);
            gfx_draw_text_line(d, 0x18, 0x20, 0x14, (int)D_255EF58, 0, 8);
            d = gfx_object_create(0x18, 0x20, 0x14, 0, 3, 0);
            gfx_draw_text_line(d, 0x18, 0x30, 0x14, (int)D_255EF68, 0, 8);
            d = gfx_object_create(0x18, 0x20, 0x14, 0, 3, 0);
            gfx_draw_text_line(d, 0x18, 0x40, 0x14, (int)D_255EF7C, 0, 8);
            break;
        }
        d = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
            gfx_draw_text_line(d, 0x18, 0x50, 0x10, (int)D_255EF8C, 0, 8);
        c = gfx_object_create(6, sel * 16 + 0x22, 0x20, 0, 0, 0x2104714);
        func_00009A6();
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        ui_timeout = 10;
        for (;;) {
            if (btn_a_presses != 0) {
                ui_timeout = 10;
                if (cnt == 0)
                    sel++;
                else if (cnt == 1) {
                    if (sel == 1)
                        sel = 3;
                    else
                        sel++;
                }
                if (sel == 4) {
                    sel = 0;
                    goto check;
                }
                gfx_object_move_dirty(c, 6, sel * 16 + 0x22);
                func_00009A6();
                btn_a_presses = 0;
            }
            if (btn_b_presses != 0) {
                btn_a_presses = 1;
                btn_b_presses = 1;
                btn_c_presses = 1;
                snd_wait_end();
                return sel;
            }
            if (btn_c_presses != 0)
                goto check;
            sys_idle_tick();
            if (ui_timeout == 0) {
                btn_c_presses = 1;
                break;
            }
        }
    check:;
    } while (btn_c_presses == 0);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return -1;
}

/*
 * Count the mail records (100 records of 100 bytes at flash 0x22D0000) of friend id (-1 =
 * any) whose kind is selected by mask: bit 1 kind 0, bit 3 kind 1, bit 2 kind 2, bit 0
 * kind 3.
 */
int mail_count(int id, u8 mask)
{
    struct MailRecord *p;
    int n;
    int i;

    i = 99;
    p = &mail_records[99];
    n = 0;
    for (; i >= 0; i--, p--) {
        if (p->friend != id && id >= 0)
            continue;
        switch (p->kind) {
        case 0:
            if (mask & 2)
                n++;
            break;
        case 1:
            if (mask & 8)
                n++;
            break;
        case 2:
            if (mask & 4)
                n++;
            break;
        case 3:
            if (mask & 1)
                n++;
            break;
        }
    }
    return n;
}

/*
 * Mail lookup: find the d-th (from the newest; d < 0 = first) record of friend id (-1 =
 * any) whose kind matches fl (same bits as mail_count), scanning down from *pcnt - 1 (or
 * 99); copy it to out (100 bytes) and return its index (also in *pcnt), -1 if none. fl &
 * 0x10 instead allocates: the first erased record, pre-filled with id, kind -1 and our name
 * pet.owner_name (a draft to be edited). Messages come with communications
 * ("おともだちからでんごんだよ").
 */
struct MailRecordView {
    int friend_id;
    int kind;
    u8 header[0x14];
    u8 text[0x40];
    u8 pad[8];
};
#define O ((struct MailRecordView *)out)
int mail_find(int id, u8 fl, u8 *out, int d, int pcnt)
{
    struct MailRecordView *e;
    int r, i, n, m, c;
    u8 *q;

    if (out) {
        q = (u8 *)out;
        for (r = 0; r < 100; r++)
            *q++ = 0xff;
    }
    if (fl & 0x10) {
        i = 0;
        e = (struct MailRecordView *)mail_records;
        r = -1;
        for (; i < 100; i++, e++) {
            if (e->friend_id == -1) {
                r = 0;
                break;
            }
        }
        if (r != 0)
            goto fail;
        if (out) {
            O->friend_id = id;
            O->kind = -1;
            memset(O->header, r, 0x14);
            memcpy(O->header, save.pet.owner_name, 0x10);
            for (r = 0; r < 0x40; r++)
                O->text[r] = 0;
        }
        goto done;
    }
    if (pcnt == 0)
        goto def;
    c = *(int *)pcnt;
    n = c - 1;
    if (c >= 0)
        goto go;
    goto fail2;
def:
    n = 99;
go:
    e = (struct MailRecordView *)FLASH_MAIL + n;
    i = n;
    r = -1;
    m = 0;
    for (; i >= 0; i--, e--) {
        if (e->friend_id == id || id < 0) {
            switch (e->kind) {
            case 0:
                if (fl & 2) {
                    if (m == d || d < 0)
                        r = 0;
                    else
                        m++;
                }
                break;
            case 1:
                if (fl & 8) {
                    if (m == d || d < 0)
                        r = 0;
                    else
                        m++;
                }
                break;
            case 2:
                if (fl & 4) {
                    if (m == d || d < 0)
                        r = 0;
                    else
                        m++;
                }
                break;
            default:
                if (fl & 1)
                    r = 0;
                break;
            }
            if (r == 0)
                break;
        }
    }
    if (r < 0) {
    fail:
        if (pcnt)
            *(int *)pcnt = -1;
    fail2:
        return -1;
    }
    if (out) {
        O->friend_id = e->friend_id;
        O->kind = e->kind;
        for (r = 0; r < 16; r++)
            O->header[r] = e->header[r];
        for (r = 0; r < 0x40; r++)
            O->text[r] = e->text[r];
    }
done:
    if (pcnt)
        *(int *)pcnt = i;
    return i;
}


/*
 * Friend a's sub-menu (cursor on b): プロフィール (profile) / メッセへんしゅう (edit
 * message) / メッセりれき (message history, greyed when the friend has no messages of kinds
 * 0/1/2: mail_friend_msg_count = their count) / おわかれ (farewell); A moves the cursor, B returns the
 * entry (0..3), C / timeout -1.
 */
/*
 * Mail lookup: find the d-th (from the newest; d < 0 = first) record of friend id (-1 =
 * any) whose kind matches fl (same bits as mail_count), scanning down from *pcnt - 1 (or
 * 99); copy it to out (100 bytes) and return its index (also in *pcnt), -1 if none. fl &
 * 0x10 instead allocates: the first erased record, pre-filled with id, kind -1 and our name
 * pet.owner_name (a draft to be edited). Messages come with communications
 * ("おともだちからでんごんだよ").
 */

/*
 * mail_dialog(k) (asm) = the mail dialogs: 0 "けす?" (delete?) yes/no, 1 "ほんとうに?",
 * 2 "メッセが いっぱいだよ! いらないメッセを けしてね!" (mail full, delete unneeded ones),
 * 0x64 "さくじょしたよ!" (deleted); returns 0 = yes, -1 = no, -2 = timeout. mail_deleted_notice
 * shows the "deleted" notice.
 */
int mail_dialog(u8 k)
{
    u8 sel;
    u8 cur;
    u8 o;

    switch (k) {
    case 0:
        ui_new_screen();
        sel = 1;
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_centered(o, 0x20, 0x10, (u8 *)D_255EF98, 0, 9);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_line(o, 0x18, 0x40, 0x10, (int)D_255EFA0, 0, 8);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_line(o, 0x18, 0x50, 0x10, (int)D_255EFA8, 0, 8);
        cur = gfx_object_create(6, 0x52, 0x20, 0, 0, 0x2104714);
        func_00009A6();
        break;
    case 1:
        ui_new_screen();
        sel = 1;
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_centered(o, 0x20, 0x10, (u8 *)D_255EF18, 0, 9);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_line(o, 0x18, 0x40, 0x10, (int)D_255EFA0, 0, 8);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_line(o, 0x18, 0x50, 0x10, (int)D_255EFA8, 0, 8);
        cur = gfx_object_create(6, 0x52, 0x20, 0, 0, 0x2104714);
        func_00009A6();
        break;
    case 2:
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_centered(o, 0x20, 0x10, (u8 *)D_255EFB0, 0, 9);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_centered(o, 0x30, 0x10, (u8 *)D_255EFBC, 0, 9);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_centered(o, 0x40, 0x10, (u8 *)D_255EFCC, 0, 9);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_centered(o, 0x50, 0x10, (u8 *)D_255EFE0, 0, 9);
        func_00009A6();
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        ui_timeout = 10;
        for (;;) {
            if (ui_timeout == 0)
                break;
            if (btn_a_presses != 0 || btn_b_presses != 0 || btn_c_presses != 0) {
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                break;
            }
        }
        return 0;
    case 100:
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
        o = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
        gfx_draw_text_centered(o, 0x21, 0x10, (u8 *)D_255EFEC, 0, 9);
        func_00009A6();
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        tmr_t5_start_timeout(3000);
        for (;;) {
            if (tmr_flags & 0x20)
                break;
            if (btn_a_presses != 0 || btn_b_presses != 0 || btn_c_presses != 0) {
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                break;
            }
        }
        return 0;
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses != 0) {
            ui_timeout = 10;
            sel++;
            if (sel > 1)
                sel = 0;
            gfx_object_move_dirty(cur, 6, sel * 16 + 0x42);
            func_00009A6();
            btn_a_presses = 0;
        }
        if (btn_b_presses != 0) {
            if (sel != 0)
                goto fail;
            btn_a_presses = 1;
            btn_b_presses = 1;
            btn_c_presses = 1;
            snd_wait_end();
            return sel;
        }
        if (btn_c_presses != 0)
            goto fail;
        sys_idle_tick();
        if (ui_timeout == 0)
            return -2;
    }
fail:
    return -1;
}

/*
 * mail_dialog(0x64).
 */
void mail_deleted_notice(void)
{
    mail_dialog(0x64);
}

/*
 * Mail full: drop the oldest record - back up the mail sector 0x35 to the scratch sector
 * (flash_backup_sector), erase it and write the 99 records from scratch + 100 back, shifted down
 * one slot.
 */
void mail_drop_oldest(void)
{
    u16 *p;
    u16 *q;
    int i, j;

    j = 0;
    flash_backup_sector(FLASH_SEC_MAIL);
    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_MAIL, FLASH_SEC_MAIL);
    p = (u16 *)(FLASH_SECTOR_COPY + 0x64);
    q = (u16 *)mail_records;
    for (; j <= 0x62; j++)
        for (i = 0; i <= 0x7f; i++, p++, q++)
            func_0000B52(q, *p, FLASH_BASE);
    sys_irq_enable();
}

/*
 * Store (mode 0: the 100 bytes at buf) or delete (mode 1) mail record idx in the mail area
 * 0x22D0000 (idx < 0: the first erased slot; when none and force, the oldest is dropped
 * first by mail_drop_oldest, else -1). An occupied slot can only be rewritten by copying the
 * whole mail area to the scratch sector 0xC, erasing sector 0x35 and writing everything
 * back with the slot replaced / skipped.
 */
int mail_store(int x, int idx, int mode, int buf, u8 force)
{
    u8 *p;
    u16 *s;
    u16 *d;
    int i, j, k, empty;

    p = (u8 *)mail_records;
    if (idx >= 0) {
        for (i = 0; i < idx; i++)
            p += 100;
        if (*(int *)p == -1 && *(int *)(p + 4) == -1)
            empty = -1;
        else
            empty = 0;
    } else {
        empty = 0;
        for (i = 0; i <= 99; i++, p += 100) {
            if (*(int *)p == -1 && *(int *)(p + 4) == -1) {
                empty = -1;
                break;
            }
        }
        if (empty == 0) {
            if (force == 0)
                return -1;
            mail_drop_oldest();
            empty = -1;
            p -= 100;
        }
    }
    sys_irq_disable();
    if (empty == 0) {
        func_0000A54(FLASH_BASE, FLASH_SEC_SECTOR_COPY, FLASH_SEC_SECTOR_COPY);
        s = (u16 *)mail_records;
        d = (u16 *)FLASH_SECTOR_COPY;
        for (i = 0; i <= 0x7fff; i++, d++, s++)
            if (func_0000B52(d, *s, FLASH_BASE) != 0)
                goto fail;
        func_0000A54(FLASH_BASE, FLASH_SEC_MAIL, FLASH_SEC_MAIL);
        p = (u8 *)FLASH_SECTOR_COPY;
        d = (u16 *)mail_records;
        for (i = 0; i <= 0x63; i++, p += 100) {
            if (i == idx) {
                k = 0;
                s = (u16 *)buf;
            } else {
                s = (u16 *)p;
                k = -1;
            }
            if (mode == 0 || (mode == 1 && k != 0)) {
                for (j = 0; j <= 0x31; j++, d++, s++)
                    if (func_0000B52(d, *s, FLASH_BASE) != 0)
                        goto fail;
            }
        }
        goto done;
    fail:
        sys_irq_enable();
        return -1;
    }
    if (mode == 0) {
        s = (u16 *)buf;
        d = (u16 *)p;
        for (j = 0; j <= 0x31; j++, d++, s++)
            if (func_0000B52(d, *s, FLASH_BASE) != 0)
                goto fail;
    }
done:
    sys_irq_enable();
    return 0;
}

/*
 * After friend a was removed: renumber the mail records' friend index (every index >= a
 * is decremented) by rewriting the mail sector 0x35 through the scratch copy.
 */
void mail_renumber_friends(int a)
{
    u8 buf[100];
    u8 *p;
    u16 *q;
    u16 *s;
    int i, j;

    flash_backup_sector(FLASH_SEC_MAIL);
    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_MAIL, FLASH_SEC_MAIL);
    p = (u8 *)FLASH_SECTOR_COPY;
    q = (u16 *)mail_records;
    for (j = 0; j <= 0x63; j++) {
        for (i = 0; i <= 0x63; i++)
            buf[i] = *p++;
        if (buf[0] >= (u8)a && buf[0] != 0xff)
            buf[0]--;
        s = (u16 *)buf;
        for (i = 0; i <= 0x31; i++, q++, s++)
            func_0000B52(q, *s, FLASH_BASE);
    }
    sys_irq_enable();
}

/*
 * Friend a's page: loop on the sub-menu friend_submenu: 0 = profile (friend_profile_view, F47),
 * 1 = edit / write our message to the friend (take the existing kind-0 record or a new
 * draft, mail_card(1, ...) = the message editor, with our character's pictures
 * rendered into flash 0x2050000 (sector 13, chara_render_pose_set(0)); saved with mail_store),
 * 2 = message history browser (every kind 0/1/2 record shown with mail_card(0, ...),
 * 3 s auto-advance, A = next, B = delete it after the two confirmations, C = back;
 * backlight off + -2 on inactivity), 3 = farewell (delete the friend, its mail of kinds
 * 0..3 (mask 0xF) and renumber the rest; returns 2). Returns 0 = message sent, -1 = back,
 * -2 = timeout.
 */
int friend_page(int a)
{
    int ok;
    u8 buf[100];
    int n;
    int m;
    int q;
    u8 cnt;
    u8 *ptr;
    int loaded;
    int saved;
    int st;
    int s;
    int k, r, i;
    u8 old;

    loaded = 0;
    st = 0;
for (;;) {
    s = friend_submenu(a, st);
    st = s;
    if (s < 0)
        break;
    switch (s) {
    case 0:
        if (friend_profile_view(a) != 0)
            return -2;
        continue;
    case 1:
        n = 100;
        k = mail_find(a, 1, buf, -1, (int)&n);
        if (k < 0)
            k = mail_find(a, 0x10, buf, -1, (int)&n);
        if (k >= 0) {
            if (loaded == 0) {
                sys_irq_disable();
                old = room_view;
                room_view = 0;
                func_0000A54(FLASH_BASE, FLASH_SEC_TEMP_RECORD, FLASH_SEC_TEMP_RECORD);
                n = chara_render_pose_set(0, (struct PictureSetHdr *)FLASH_TEMP_RECORD);
                room_view = old;
                sys_irq_enable();
                if (n < 0)
                    return -1;
                n = ((struct PictureSetHdr *)FLASH_TEMP_RECORD)->x002 << 8;
                n += ((struct PictureSetHdr *)FLASH_TEMP_RECORD)->x003;
                ptr = (u8 *)(FLASH_TEMP_RECORD + 0x6) + n;
                loaded = 1;
            }
            n = mail_card(1, (int)buf, a, 0, (int)ptr);
            if (n == 0) {
                snd_wait_end();
                sys_irq_disable();
                mail_store(a, k, 0, (int)buf, 0);
                sys_irq_enable();
                return 0;
            }
            if (n == -2)
                return n;
            continue;
        } else {
            mail_dialog(2);
            return -1;
        }
    case 2:
        ui_timeout = 30;
        if (loaded == 0) {
            sys_irq_disable();
            old = room_view;
            room_view = 0;
            func_0000A54(FLASH_BASE, FLASH_SEC_TEMP_RECORD, FLASH_SEC_TEMP_RECORD);
            n = chara_render_pose_set(0, (struct PictureSetHdr *)FLASH_TEMP_RECORD);
            room_view = old;
            sys_irq_enable();
            if (n < 0)
                return -1;
            n = ((struct PictureSetHdr *)FLASH_TEMP_RECORD)->x002 << 8;
            n += ((struct PictureSetHdr *)FLASH_TEMP_RECORD)->x003;
            ptr = (u8 *)(FLASH_TEMP_RECORD + 0x6) + n;
            loaded = 1;
        }
    restart:
        cnt = 0;
        n = 100;
        m = 100;
        for (;;) {
            saved = m;
            k = mail_find(a, 0xe, buf, -1, (int)&m);
            if (k >= 0) {
                cnt++;
                mail_card(0, (int)buf, a, save.mail_friend_msg_count - cnt, (int)ptr);
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                tmr_flags &= ~0x20;
                tmr_t5_start_timeout(3000);
                for (;;) {
                    if (tmr_flags & 0x20)
                        tmr_flags &= ~0x20;
                    if (btn_a_presses != 0) {
                        btn_a_presses = 0;
                        ui_timeout = 30;
                        btn_c_presses = 0;
                        n--;
                        break;
                    }
                    if (btn_b_presses != 0) {
                        btn_b_presses = 0;
                        if ((r = mail_dialog(0)) == 0 && (r = mail_dialog(1)) == 0) {
                            sys_irq_disable();
                            mail_store(a, k, 1, (int)buf, 0);
                            sys_irq_enable();
                            if (save.mail_friend_msg_count != 0)
                                save.mail_friend_msg_count--;
                            if (cnt != 0)
                                cnt -= 2;
                        } else {
                            if (r == -2)
                                return -1;
                            cnt--;
                        }
                        ui_timeout = 30;
                        m = saved;
                        goto clr;
                    }
                    if (btn_c_presses != 0) {
                        ui_timeout = 30;
                        break;
                    }
                    sys_idle_tick();
                    if (sys_display_state != 0 || ui_timeout == 0) {
                        sys_display_state = 2;
                        sys_set_backlight(0);
                        sys_apply_backlight();
                        return -2;
                    }
                }
            } else {
                n--;
                if (n < 0) {
                    q = 100;
                    for (i = 0, ok = -1; i <= 0x63; i++) {
                        if (mail_find(a, 0xe, buf, -1, (int)&q) >= 0) {
                            ok = 0;
                            break;
                        }
                    }
                    if (ok != 0)
                        return -1;
                    goto restart;
                }
            clr:
                btn_c_presses = 0;
            }
            if (btn_c_presses != 0)
                break;
        }
        if (btn_c_presses == 0)
            return -1;
        btn_c_presses = 0;
        continue;
    default:
        if (friend_delete(a) != 0) {
            for (n = 0, m = 100; n <= 0x63; n++) {
                k = mail_find(a, 0xf, buf, -1, (int)&m);
                if (k >= 0) {
                    sys_irq_disable();
                    mail_store(a, k, 1, (int)buf, 0);
                    sys_irq_enable();
                }
            }
            mail_renumber_friends(a);
            return 2;
        }
        if (ui_timeout == 0)
            return -2;
        continue;
    }
}
    btn_c_presses = 0;
    return -1;
}

/*
 * The friend book browser (ともだちてちょう, entered through friend_book_open): no friends =
 * "no entries" sound and exit; A = previous friend (friend_prev_used walks the used records
 * backwards), B = open the friend (friend_page); while friend_book_state.new_friend_waiting is set, B instead
 * says goodbye to the friend (friend_delete) and deletes its mail. C / timeout = leave;
 * with friend_book_state.new_friend_waiting set, the mail of the friend indexes queued in comm_mail_queue[0..2] is
 * deleted first. Each page is drawn by friend_draw_page.
 */
u8 friend_book(void)
{
    u8 buf[100];
    int cnt;
    int flag;
    int i;
    u8 sel;
    int n, j, r;
    u8 *p;
    struct FriendBookState *e;

    flag = 1;
top:
    switch (r = friend_count()) {
    case 0:
        if (flag == 0)
            goto out;
        snd_error_beep();
        btn_a_presses = 1;
        btn_c_presses = 1;
        goto zero;
    case 1:
        flag = 0;
        break;
    }
    sel = friend_prev_used(0x1e);
redraw:
    friend_draw_page(sel);
    if (flag != 0) {
        p = (u8 *)(sel > friend_prev_used(sel) ? 0x21059a4 : 0x21059f4);
        gfx_object_create(0x76 - (p[0] >> 1), 0x5c - (p[1] >> 1), 0x60, 0, 0, (int)p);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses != 0) {
            ui_timeout = 10;
            sel = friend_prev_used(sel);
            goto redraw;
        }
        if (btn_b_presses != 0) {
            ui_timeout = 10;
            e = &friend_book_state;
            if (e->new_friend_waiting != 0) {
                if (friend_delete(sel) == 0)
                    goto redraw;
                for (i = 0, cnt = 100; i <= 0x63; i++) {
                    n = mail_find(sel, 0xf, buf, -1, (int)&cnt);
                    if (n >= 0) {
                        sys_irq_disable();
                        mail_store(sel, n, 1, (int)buf, 0);
                        sys_irq_enable();
                        for (j = 0; j <= 2; j++) {
                            if (((u8 *)&comm_mail_queue)[j] == sel) {
                                ((u8 *)&comm_mail_queue)[j] = 0xff;
                                break;
                            }
                        }
                    }
                }
                mail_renumber_friends(sel);
                e = &friend_book_state;
                r = e->new_friend_waiting;
                if (r == 0)
                    goto out;
                if (sel != 0)
                    sel--;
                sel = friend_prev_used(sel);
                goto redraw;
            }
            r = e->open_after_comm;
            if (r == 1)
                goto out;
            i = friend_page(sel);
            if (i == 0)
                goto redraw;
            if (i == 2) {
                switch (r = friend_count()) {
                case 0:
                    if (flag == 0)
                        return 0;
                quit:
                    snd_error_beep();
                    btn_a_presses = 1;
                    btn_c_presses = 1;
                    goto zero;
                case 1:
                    flag = 0;
                    goto top;
                }
                if (sel >= r)
                    sel = 0;
                goto redraw;
            }
            if (i == -2)
                goto zero;
            if (ui_timeout == 0)
                goto zero;
            goto redraw;
        }
        if (btn_c_presses != 0) {
            snd_wait_before_flash();
            e = &friend_book_state;
            if (e->new_friend_waiting != 0) {
                for (j = 0; j <= 2; j++) {
                    if (((u8 *)&comm_mail_queue)[j] != 0xff) {
                        for (i = 0, cnt = 100; i <= 0x63; i++) {
                            n = mail_find(((u8 *)&comm_mail_queue)[j], 0xf, buf, -1, (int)&cnt);
                            if (n >= 0) {
                                snd_wait_before_flash();
                                sys_irq_disable();
                                mail_store(((u8 *)&comm_mail_queue)[j], n, 1, (int)buf, 0);
                                sys_irq_enable();
                            }
                        }
                        mail_renumber_friends(((u8 *)&comm_mail_queue)[j]);
                        ((u8 *)&comm_mail_queue)[j] = 0xff;
                    }
                }
            }
            ui_timeout = 10;
            goto zero;
        }
        sys_idle_tick();
    } while ((n = ui_timeout) != 0);
    e = &friend_book_state;
    if (e->new_friend_waiting != 0) {
        j = n;
        do {
            if (((u8 *)&comm_mail_queue)[j] != 0xff) {
                for (i = 0, cnt = 100; i <= 0x63; i++) {
                    n = mail_find(((u8 *)&comm_mail_queue)[j], 0xf, buf, -1, (int)&cnt);
                    if (n >= 0) {
                        snd_wait_before_flash();
                        sys_irq_disable();
                        mail_store(((u8 *)&comm_mail_queue)[j], n, 1, (int)buf, 0);
                        sys_irq_enable();
                    }
                }
                mail_renumber_friends(((u8 *)&comm_mail_queue)[j]);
                ((u8 *)&comm_mail_queue)[j] = 0xff;
            }
        } while (++j <= 2);
    }
zero:
    r = 0;
out:
    return r;
}

/*
 * Friend notebook (friend_book).
 */
int friend_book_open(void)
{
    return friend_book();
}

/*
 * Handler of the きせつかざり entry of the notebook menu ui_notebook_menu_tbl: the record browser
 * item_special_area_browser (sector 0x62 area); 1 when it returned non-zero.
 */
int ui_notebook_decorations(void)
{
    return item_special_area_browser() != 0;
}

/*
 * Photo album: pet.photo_slots[0..5] = the 6 album slots (0 = empty, else the photo record
 * index + 1 into 0x2770000 + k * 0x2800, see album_photo_base[cur + 2]). Returns the number of
 * photos.
 */
int album_photo_count(void)
{
    int n = 0;
    int i;

    for (i = 0; i <= 5; i++)
        if (save.pet.photo_slots[i] != 0)
            n++;
    return n;
}

/*
 * Previous used photo album slot of pet.x0B6 before k (wraps).
 */
int album_prev_photo(int k)
{
    do {
        if (k > 0)
            k--;
        else
            k = 5;
    } while (save.pet.photo_slots[k] == 0);
    return k;
}

/*
 * Photo album browser (きねんしゃしん): shows the photo bitmap of the current slot, A =
 * previous, B = "けす? はい/いいえ" + "ほんとうに?" to delete it (the 0x2800-byte record is
 * removed from sector 0x7F via the scratch copy and the slot list is compacted), C /
 * timeout = leave.
 */
u8 album_photo_browser(void)
{
    int sel = 1;
    int cur;
    u8 *img;
    int i, p, o, q, k;
    int res;

    switch (album_photo_count()) {
    case 0:
        snd_error_beep();
        btn_a_presses = 1;
        btn_c_presses = 1;
        return 0;
    case 1:
        sel = 0;
    default:
        cur = album_prev_photo(0);
    }
redraw:
    k = save.pet.photo_slots[cur] - 1;
    img = (u8 *)(k * 0x2800 + FLASH_PHOTOS);
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, (const u8 *)img);
    if (sel != 0) {
        if (cur > album_prev_photo(cur))
            gfx_object_create(0x72, 0x58, 0xb0, 0, 0, D_21059A4);
        else
            gfx_object_create(0x72, 0x58, 0xb0, 1, 0, D_21059A4);
    }
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    do {
        if (btn_a_presses != 0) {
            cur = album_prev_photo(cur);
            goto redraw;
        }
        if (btn_b_presses != 0) {
            res = 1;
            switch (ui_yes_no(D_255EF98, D_255EFA0, D_255EFA8, 0x10, &res)) {
            case 1:
                switch (res) {
                case 0:
                    if (ui_confirm_really() != 0) {
                        snd_wait_end();
                        flash_backup_sector(FLASH_SEC_PHOTOS);
                        flash_restore_sector_except(FLASH_SEC_PHOTOS, img, 0x2800);
                        for (i = cur; i <= 4; i++) {
                            p = (int)&save.pet;
                            o = 0xb4;
                            q = 0xb5;
                            *(u8 *)(i + p + o + 2) = *(u8 *)(i + p + q + 2);
                        }
                        save.pet.photo_slots[i] = 0;
                        save_game();
                        switch (album_photo_count()) {
                        case 0:
                            btn_b_presses = 0;
                            btn_c_presses = 0;
                            return 0;
                        case 1:
                            sel = 0;
                        default:
                            cur = album_prev_photo(cur);
                        }
                    } else if (ui_timeout == 0)
                        return 0;
                    btn_b_presses = 0;
                    btn_c_presses = 0;
                    goto redraw;
                case 1:
                    goto redraw;
                }
                break;
            case 2:
                goto redraw;
            case 3:
                return 0;
            }
        }
        if (btn_c_presses != 0) {
            ui_timeout = 10;
            return 0;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
    return 0;
}

/*
 * Five pages; page k shows the character k (names album_special_names[k]: ゆめキャンっち, チャピっち,
 * もなトトっち, リンスっち, ふりらぶっち; picture via chara_select_other_by_set(0x2C + k)) when the
 * character owns flag 0x3D + k (pet_get_unlock_flag), else "?????"; A = next page, C = leave.
 * Unclear whether this is the seasonal decorations (Fable) or the child-raising notebook
 * (Opus) viewer. Unused: no call or jump to it in the image and no pointer to it (32-bit
 * LE/BE scan of the whole dump), so dead code unless a downloaded game calls it by address.
 */
void album_special_charas(void)
{
    u8 k;
    u8 a;

    k = 0;
again:
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    if (pet_get_unlock_flag(k + 0x3d) != 0) {
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        chara_select_other_by_set(k + 0x2c);
        chara_draw_second(a, 0x20, 0, 0, 0x40, 0x30);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x2b, 0x40, album_special_names[k], 0, 9);
    } else {
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x31, 0x40, D_255F05C, 0, 9);
    }
    if (k + 1 > 9)
        gfx_object_create(4, 0x39, 0x50, 1, 0, D_2142E38);
    gfx_object_create(0xc, 0x39, 0x50, (k + 1) % 10, 0, D_2142E38);
    if (k == 4)
        gfx_object_create(0x72, 0x58, 0x60, 1, 0, D_21059A4);
    else
        gfx_object_create(0x72, 0x58, 0x60, 0, 0, D_21059A4);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses != 0) {
            ui_timeout = 10;
            k++;
            if (k > 4)
                k = 0;
            goto again;
        }
        if (btn_b_presses != 0) {
            btn_b_presses = 0;
            ui_timeout = 10;
        }
        if (btn_c_presses != 0)
            return;
        sys_idle_tick();
    } while (ui_timeout != 0);
}

/*
 * Friend index of mail record a (first word of 0x22D0000 + 100a).
 */
int mail_friend_index(int a)
{
    return *(int *)(a * 100 + FLASH_MAIL);
}

/*
 * Mark all kind-1 mail (newly received) as kind 2 (read), rewriting the mail sector via
 * the scratch copy.
 */
void mail_mark_all_read(void)
{
    u8 buf[100];
    u8 *p;
    u8 *t;
    u16 *d;
    u16 *s;
    int i, j;

    j = 0;
    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_SECTOR_COPY, FLASH_SEC_SECTOR_COPY);
    s = (u16 *)mail_records;
    d = (u16 *)FLASH_SECTOR_COPY;
    for (; j <= 0x7fff; j++, d++, s++)
        if (func_0000B52(d, *s, FLASH_BASE) != 0)
            goto out;
    func_0000A54(FLASH_BASE, FLASH_SEC_MAIL, FLASH_SEC_MAIL);
    p = (u8 *)FLASH_SECTOR_COPY;
    d = (u16 *)mail_records;
    for (j = 0; j <= 0x63; j++, p += 100) {
        if (*(u32 *)(p + 4) == 1) {
            t = p;
            for (i = 0; i <= 0x63; i++)
                buf[i] = *t++;
            buf[4] = 2;
            s = (u16 *)buf;
        } else
            s = (u16 *)p;
        for (i = 0; i <= 0x31; i++, d++, s++)
            if (func_0000B52(d, *s, FLASH_BASE) != 0)
                goto out;
    }
out:
    sys_irq_enable();
}

/*
 * Read the unread mail (kind 1, mask 8), newest first: each shown by mail_card(0, ...)
 * for 3 s; A = next, B = delete (two confirmations), C = stop; everything shown is then
 * marked read (mail_mark_all_read). 30 s timeout; -2 on inactivity (display off).
 */
int mail_read_unread(void)
{
    u8 buf[100];
    int cnt;
    int pos, n, x, r;

    ui_timeout = 30;
    cnt = 100;
    pos = mail_count(-1, 8);
    if (pos <= 1)
        pos = -1;
    for (;;) {
    again:
        n = mail_find(-1, 8, buf, -1, (int)&cnt);
        if (n < 0) {
            mail_mark_all_read();
            return 0;
        }
        if (n >= 0) {
            x = mail_friend_index(cnt);
            if (pos != -1)
                pos--;
            mail_card(0, (int)buf, x, pos, 0);
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            tmr_flags &= ~0x20;
            tmr_t5_start_timeout(3000);
            do {
                if (tmr_flags & 0x20)
                    tmr_flags &= ~0x20;
                if (btn_a_presses != 0) {
                    btn_a_presses = 0;
                    ui_timeout = 30;
                    goto again;
                }
                if (btn_b_presses != 0) {
                    btn_b_presses = 0;
                    r = mail_dialog(0);
                    if (r == 0) {
                        r = mail_dialog(1);
                        if (r == 0) {
                            sys_irq_disable();
                            mail_store(x, n, 1, (int)buf, r);
                            sys_irq_enable();
                        } else if (r == -2)
                            return 0;
                    } else if (r == -2)
                        return 0;
                    ui_timeout = 30;
                    cnt++;
                    pos++;
                    goto again;
                }
                if (btn_c_presses != 0) {
                    btn_c_presses = 0;
                    mail_mark_all_read();
                    ui_timeout = 10;
                    return 0;
                }
                sys_idle_tick();
                if (sys_display_state != 0)
                    break;
            } while (ui_timeout != 0);
            snd_wait_end();
            mail_mark_all_read();
            sys_display_state = 2;
            sys_set_backlight(0);
            sys_apply_backlight();
            return -2;
        }
    }
}

/*
 * Notebook menu ui_notebook_menu_tbl: handler of the きねんしゃしん entry (photo album,
 * album_photo_browser).
 */
int ui_notebook_photos(void)
{
    return album_photo_browser();
}

/*
 * Notebook menu ui_notebook_menu_tbl: handler of the こそだてちょう entry (dl_chara_list: the touch
 * stamp card and the downloaded characters).
 */
int ui_notebook_childcare(void)
{
    return dl_chara_list() != 0;
}

/*
 * Notebook menu: ともだちてちょう entry disabled when there are no friends.
 */
int ui_notebook_friends_enable(void)
{
    if (friend_count() > 0)
        return 0;
    return 1;
}

/*
 * Notebook menu: きねんしゃしん entry disabled when there are no photos.
 */
int ui_notebook_photos_enable(void)
{
    if (album_photo_count() > 0)
        return 0;
    return 1;
}

/*
 * Notebook menu: こそだてちょう entry, never disabled (returns 0).
 */
int ui_notebook_childcare_enable(void)
{
    return 0;
}

/*
 * Notebook menu: きせつかざり entry disabled when there are no records (item_special_area_count
 * counts them).
 */
int ui_notebook_decorations_enable(void)
{
    if (item_special_area_count() > 0)
        return 0;
    return 1;
}

/*
 * The notebooks entry: loops on the unread-mail reader mail_read_unread while unread mail
 * exists (mail_unread_pending = 1), then opens the notebook menu ui_place_menu_run(ui_notebook_menu_tbl, 8)
 * (たまともてちょう / ともだちてちょう / きねんしゃしん / こそだてちょう / きせつかざり).
 */
void ui_notebook_menu(void)
{
    volatile int a = 0;
    volatile int b = 0;
    volatile int f;
    volatile int c;

again:
    save.mail_unread_pending = 0;
    f = 0;
    if (mail_find(-1, 8, 0, -1, 0) >= 0) {
        f = 1;
        save.mail_unread_pending = 1;
    }
    if (f != 0) {
        if (mail_read_unread() == -2)
            return;
        snd_wait_end();
        goto again;
    }
    ui_place_menu_run((u8 *)ui_notebook_menu_tbl, 8);
}

/*
 * Picture diary ("えにっき"). The draft page diary_draft (struct DiaryDraft):
 * 0x00..0x31 = 6 text lines (5, 8, 9, 9, 9, 5 chars, 0x6C = blank), 0x32 = event code
 * (templates diary_templates: 1000..1004 something made / played together, 2000 a meal
 * with the partner, 2001 the park, 2002 school with the partner, 2003 the nursery, 2004
 * the bath, 2005 together, 2006 a partner's visit, 2007 the garden, 3000..3003 the four
 * jobs, 4000..4004 the school clubs; only 2002, 2003, 2006, 3000..3003 and 4000..4004
 * are written by this firmware), 0x34 = variant, 0x36 = state (0 none, 3 composed, 4 written, 5 shown),
 * 0x38 = 0xFF once stored, 0x3C = 1 "diary full" / 2 "could not write".
 *
 * Scene snapshot for the page's image list diary_images: the 15 image pointers anim_pic_slots
 * plus a and b copied to out.
 */
void diary_snapshot_images(u32 *out, u32 a, u32 b)
{
    int i;

    for (i = 0; i <= 14; i++)
        *out++ = ((u32 *)anim_pic_slots)[i];
    *out++ = a;
    *out = b;
}

/*
 * Compose a picture-diary draft p (diary_draft) for event x / variant y: refused (-1) when
 * a page is already pending (state > 2) or when a non-communication event (x outside
 * 2000..2999) would replace a pending one; writes the six text fields a..f
 * (blank-padded with 0x6C), event code x and variant y, state 3.
 */
int diary_compose_draft(u8 *p, const u8 *a, const u8 *b, const u8 *c, const u8 *d, const u8 *e, const u8 *f, u16 x, u8 y)
{
    u8 buf[60];
    u16 *q;
    u8 *s;
    int i, j;

    q = (u16 *)(p + 0x32);
    s = p;
    if (s[0x36] > 2 || (s[0x36] > 3 && s[0x38] == 0xff))
        return -1;
    if (!(x >= 2000 && x <= 2999) && s[0x36] != 0)
        return -1;
    s[0x36] = 3;
    for (j = 0; j <= 0x31; j++)
        s[j] = 0;
    memcpy(buf, a, 5);
    memcpy(buf + 10, b, 8);
    memcpy(buf + 20, c, 9);
    memcpy(buf + 30, d, 9);
    memcpy(buf + 40, e, 9);
    memcpy(buf + 50, f, 5);
    for (j = 0; j <= 5; j++)
        for (i = 0; i <= 9; i++)
            if (buf[j * 10 + i] == 0)
                buf[j * 10 + i] = 0x6c;
    memcpy(s, buf, 5);
    memcpy(s + 5, buf + 10, 8);
    memcpy(s + 0xd, buf + 20, 9);
    memcpy(s + 0x16, buf + 30, 9);
    memcpy(s + 0x1f, buf + 40, 9);
    memcpy(s + 0x28, buf + 50, 5);
    *q = x;
    s[0x34] = y;
    return 0;
}

/*
 * Reset the picture-diary counters.
 */
void diary_reset(void)
{
    struct DiaryDraft *p = diary_draft;

    p->state = 0;
    p->stored = 0;
}

/*
 * Diary entry for a communication (event 2006 = 0x7D6, variant 0), unless a page is
 * already stored: partner images loaded (visit_load_partner_pics), draft composed with the partner
 * name ir_partner_name, page composed (diary_assign_partner_pics) and rendered + stored (diary_render_store, asm).
 */
void diary_write_comm_page(void)
{
    int r;

    if (diary_draft->stored == 0xff)
        return;
    visit_load_partner_pics(0);
    snd_wait_end_b(0);
    snd_wait_end_b(1);
    snd_wait_end_b(2);
    snd_wait_end_b(3);
    r = diary_compose_draft((u8 *)diary_draft, ir_partner_name, D_255F174, D_255F160, D_255F160, friend_event.x004, (u8 *)&save.pet.owner_name, 0x7d6, 0);
    if (r == 0) {
        diary_snapshot_images((u32 *)diary_images, food_party_table_pic_chara(), r);
        diary_assign_partner_pics(diary_draft, (u32 (*)[3])diary_images);
        snd_wait_end();
        diary_render_store(diary_draft, diary_images);
    }
}

/*
 * "えにっきちゅう" (drawing in the diary) scene after a page was written (state 4 -> 5),
 * 3 s, then "えにっきが いっぱいに なったよ!" (the diary is now full) or "えにっきが
 * いっぱいで かけなかったよ!" (full, could not write) when x03c says so.
 */
int diary_writing_scene(void)
{
    struct DiaryDraft *q;
    u8 a;

    q = diary_draft;
    if (q->state != 4)
        return -1;
    q->state = 5;
    anim_scene_room(10);
    anim_draw_pic(0, (u8 *)room_bedroom_designs[save.pet.room_designs[4]], 0, 0x40, 0x24);
    anim_draw_pic(1, D_21A05D8, 0, 0x40, 0x40);
    anim_draw_pic(2, D_219FFAC, 0, 0x40, 0x33);
    anim_draw_pic(3, D_21A06D8, 0, 0x3e, 0x28);
    anim_draw_pic(4, D_21A2010, 0, 0x54, 0x3e);
    anim_draw_pet(5, 7, 0, 0x4d, 0x31);
    anim_draw_pic(8, D_2163F38, 0, 0x41, 0x11);
    anim_draw_text(9, D_255F188, 0, 0x1e, 0x11);
    func_00009A6();
    save_game();
    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 1;
    ui_timeout = 3;
    do
        sys_idle_tick();
    while (ui_timeout != 0);
    if (q->store_result != 0) {
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
        if (q->store_result == 1) {
            a = gfx_object_create(0, 0, 0x20, 0, 0, 0);
            gfx_draw_text_centered(a, 0x2c, 0x20, D_255F198, 0, 9);
            a = gfx_object_create(0, 0, 0x20, 0, 0, 0);
            gfx_draw_text_centered(a, 0x3c, 0x20, D_255F1A4, 0, 9);
            a = gfx_object_create(0, 0, 0x20, 0, 0, 0);
            gfx_draw_text_centered(a, 0x4c, 0x20, D_255F1B0, 0, 9);
        } else {
            a = gfx_object_create(0, 0, 0x20, 0, 0, 0);
            gfx_draw_text_centered(a, 0x2c, 0x20, D_255F198, 0, 9);
            a = gfx_object_create(0, 0, 0x20, 0, 0, 0);
            gfx_draw_text_centered(a, 0x3c, 0x20, D_255F1BC, 0, 9);
            a = gfx_object_create(0, 0, 0x20, 0, 0, 0);
            gfx_draw_text_centered(a, 0x4c, 0x20, D_255F1C8, 0, 9);
        }
        func_00009A6();
        ui_timeout = 3;
        do
            sys_idle_tick();
        while (ui_timeout != 0);
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    anim_busy = 0;
    return 0;
}

/*
 * Number of picture-diary pages written: the first erased 0x2000-byte page among 23 (0x17)
 * at flash 0x2570000, 23 = full.
 */
int diary_page_count(void)
{
    int r = flash_first_erased_record((u8 *)FLASH_DIARY, 0x2000, 0x17);

    return r == -1 ? 0x17 : r;
}

/*
 * Previous diary page index (from page 0: the last written page).
 */
int diary_prev_page(int k)
{
    int t;

    if (k > 0)
        return k - 1;
    t = diary_page_count();
    return t > 0 ? t - 1 : 0;
}

/*
 * Delete diary page n: copy the three diary sectors to the scratch 0x2790000 (sectors
 * 0x81..0x83, where the downloaded-character slots 2..7 also live), erase 0x5F..0x61, write
 * back all pages but n. Only called by the unreferenced viewer diary_viewer.
 */
void diary_delete_page(int n)
{
    u16 *e;
    u16 *s;
    u16 *d;
    int i, j;

    s = (u16 *)FLASH_DIARY;
    d = dl_slot2_area;
    e = diary_area_end;
    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_DIARY_DELETE_COPY, FLASH_SEC_DL_CHARA_LAST);
    do {
        if (func_0000B52(d, *s, FLASH_BASE) != 0)
            goto out;
        s++;
        d++;
    } while (s < e);
    func_0000A54(FLASH_BASE, FLASH_SEC_DIARY, FLASH_SEC_DIARY_LAST);
    s = dl_slot2_area;
    d = (u16 *)FLASH_DIARY;
    for (j = 0; j <= 0x17; j++) {
        if (j != n) {
            for (i = 0; i <= 0xfff; i++, s++, d++)
                if (func_0000B52(d, *s, FLASH_BASE) != 0)
                    goto out;
        } else
            s += 0x1000;
    }
out:
    sys_irq_enable();
}

/*
 * Picture diary viewer: page = 0x24 bytes of text lines + the stored bitmap (drawn from
 * +0x24: w, h at [0..1], pixel data) with the text drawn over the page frame; A =
 * previous page, B = delete it (two confirmations), C / 30 s = leave. pet.diary_unseen_page = index +
 * 1 of the newest page the user has not yet seen (pages past it are not browsable).
 * Unused: no call or jump to it in the image and no pointer to it (32-bit LE/BE scan of
 * the whole dump), so dead code unless a downloaded game calls it by address.
 */
int diary_viewer(void)
{
    u8 buf[0x25];
    int k;
    int flag;
    int r, y, e, sw;
    u8 z;
    u8 x, c;
    u8 *p, *s;
    int n;
    u8 v;

    flag = 1;
top:
    sw = save.pet.diary_unseen_page;
    if (sw == 0) {
        sw = diary_page_count();
        switch (sw) {
        case 0:
            goto fail;
        case 1:
            flag = 0;
        }
    } else {
        sw = diary_page_count();
        switch (sw) {
        case 0:
        fail:
            snd_error_beep();
            btn_a_presses = 1;
            btn_c_presses = 1;
            goto done;
        case 1:
            flag = 0;
        }
        n = save.pet.diary_unseen_page;
        if (n - 1 >= diary_prev_page(0))
            flag = 0;
    }
    k = diary_prev_page(0);
redraw:
    ui_new_screen();
    p = (u8 *)FLASH_DIARY + (k << 13);
    memset(buf, 0, 0x25);
    memcpy(buf, p, 0x24);
    p += 0x24;
    z = 0;
    gfx_object_create(0x40, 0x1c, z, z, z, (const u8 *)p);
    x = p[2];
    gfx_object_create(z, 0x1c, x, z, z, D_20F5384);
    x += D_20F5384[2];
    s = buf;
    y = 0x20;
    while (*s != 0) {
        c = gfx_object_create(0, 0x1c, x, 0, 0, D_20F5384);
        gfx_objects[c].flags &= ~0x30;
        s = (u8 *)gfx_draw_text_line(c, 6, y, x, (int)s, 1, 6);
        x += D_2091748.colours;
        y += 0xb;
    }
    if (flag) {
        if (save.pet.diary_unseen_page == 0) {
            if (k > diary_prev_page(k))
                gfx_object_create(0x78, 0x5c, x, 0, 0, D_21059A4);
            else
                gfx_object_create(0x78, 0x5c, x, 1, 0, D_21059A4);
        } else {
            if (save.pet.diary_unseen_page - 1 >= k)
                gfx_object_create(0x78, 0x5c, x, 1, 0, D_21059A4);
            else
                gfx_object_create(0x78, 0x5c, x, 0, 0, D_21059A4);
        }
    }
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 30;
    for (;;) {
        if (btn_a_presses != 0) {
            if (save.pet.diary_unseen_page == 0 || save.pet.diary_unseen_page - 1 < k) {
                k = diary_prev_page(k);
                goto redraw;
            }
            e = 1;
            break;
        }
        if (btn_b_presses != 0) {
            btn_b_presses = 0;
            r = mail_dialog(0);
            if (r == 0) {
                r = mail_dialog(1);
                if (r == 0) {
                    diary_delete_page(k);
                    if (diary_page_count() <= 0) {
                        if (save.pet.diary_unseen_page == 0)
                            goto done;
                        save.pet.diary_unseen_page = 0;
                    } else {
                        n = save.pet.diary_unseen_page;
                        if (n - 1 < diary_page_count())
                            goto top;
                        save.pet.diary_unseen_page = 0;
                    }
                    goto bye;
                }
            }
            ui_timeout = 30;
            goto redraw;
        }
        v = btn_c_presses;
        e = 1;
        if (v != 0) {
            ui_timeout = 30;
            break;
        }
        sys_idle_tick();
        if (ui_timeout == 0) {
            e = 1;
            break;
        }
    }
    if (e > 0) {
        save.pet.diary_unseen_page = 0;
    bye:
        save_game();
    }
done:
    return 0;
}

/*
 * Wait for the key-click sound to end (wrapper of snd_wait_end).
 */
void snd_wait_end_b(int unused)
{
    snd_wait_end();
}

/*
 * Drop the oldest diary page (page 0): copy the diary sectors to the scratch 0x2790000
 * (shared with the downloaded-character slots 2..7), erase 0x5F..0x61 and write back the
 * copy shifted by one page; pet.diary_unseen_page follows. No caller in this firmware.
 */
int diary_drop_oldest(void)
{
    u16 *e;
    u16 *s;
    u16 *d;
    int r;

    r = 0;
    s = (u16 *)FLASH_DIARY;
    d = dl_slot2_area;
    e = diary_area_end;
    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_DIARY_DELETE_COPY, FLASH_SEC_DL_CHARA_LAST);
    do {
        if (func_0000B52(d, *s, FLASH_BASE) != 0) {
            r = -1;
            break;
        }
        s++;
        d++;
    } while (s < e);
    if (r == 0) {
        func_0000A54(FLASH_BASE, FLASH_SEC_DIARY, FLASH_SEC_DIARY_LAST);
        s = diary_scratch_page1;
        d = (u16 *)FLASH_DIARY;
        e = diary_scratch_end;
        do {
            if (func_0000B52(d, *s, FLASH_BASE) != 0) {
                r = -1;
                break;
            }
            s++;
            d++;
        } while (s < e);
    }
    sys_irq_enable();
    if (save.pet.diary_unseen_page > 1)
        save.pet.diary_unseen_page--;
    return r;
}

/*
 * Delete the last (newest) diary page: copy its sector to the scratch sector 0xF
 * (0x2070000), re-erase the sector and write back the earlier pages it held (probably
 * "undo the last page").
 */
int diary_delete_last(void)
{
    int n, i, sec, len;
    u16 *s, *t, *a, *b;

    n = diary_page_count();
    if (n == 0)
        return -1;
    n--;
    sys_irq_disable();
    func_0000A54(FLASH_BASE, FLASH_SEC_DIARY_COPY, FLASH_SEC_DIARY_COPY);
    b = t = (u16 *)FLASH_DIARY_COPY;
    if (n >= 0 && n <= 7) {
        a = s = (u16 *)FLASH_DIARY;
        sec = FLASH_SEC_DIARY;
        len = n * 0x2000;
    } else if (n >= 8 && n <= 15) {
        a = s = (u16 *)(FLASH_DIARY + 0x10000);
        sec = FLASH_SEC_DIARY + 1;
        len = (n - 8) * 0x2000;
    } else {
        a = s = (u16 *)(FLASH_DIARY + 0x20000);
        sec = FLASH_SEC_DIARY + 2;
        len = (n - 16) * 0x2000;
    }
    if (n == 0 || n == 8 || n == 16) {
        func_0000A54(FLASH_BASE, sec, sec);
        goto done;
    fail:
        sys_irq_enable();
        return -1;
    }
    for (i = 0; i <= 0x7fff; i++, a++, b++)
        if (func_0000B52(b, *a, FLASH_BASE) != 0)
            goto fail;
    func_0000A54(FLASH_BASE, sec, sec);
    i = 0;
    if (i >= len / 2)
        goto done;
again:
    if (func_0000B52(s, *t, FLASH_BASE) != 0)
        goto fail;
    i++;
    s++;
    t++;
    if (i < len / 2)
        goto again;
done:
    sys_irq_enable();
    return 0;
}


/*
 * diary_render_store(page, images) (asm): render the composed picture-diary page (diary_compose_page
 * picks the scene) and store it (diary_rasterize_page / diary_store_page).
 */
int diary_render_store(struct DiaryDraft *p, int b)
{
    u8 buf1[36];
    u8 buf2[36];
    u8 hdr[8];
    struct DiaryDraft *q;
    int n;
    u8 k, i, j, i2, s;
    const u8 *img;

    q = p;
    if (p->state > 4 && p->stored == 0xff) {
        anim_busy = 0;
        return -1;
    }
    if (q->state == 3 && q->stored == 0xff)
        diary_delete_last();
    q->state++;
    n = diary_page_count();
    if (n > 0x16) {
        q->store_result = 2;
        anim_busy = 0;
        return -1;
    }
    if (n == 0x16)
        q->store_result = 1;
    else
        q->store_result = 0;
    memset(buf1, 0, 0x24);
    k = diary_compose_page((char *)buf1, p, (u32 (*)[3])b);
    memset(buf2, 0, 0x24);
    for (j = 0, i = 0; i <= 0x23; i++) {
        if (buf1[i] != 0x6c) {
            buf2[j] = buf1[i];
            j++;
        }
    }
    memcpy(buf1, buf2, 0x24);
    s = 0;
    for (i2 = 0; i2 < k; i2++) {
        img = gfx_objects[i2].image;
        if (img != 0)
            s += img[2];
    }
    hdr[0] = 0x40;
    hdr[1] = 0x48;
    hdr[2] = s;
    hdr[3] = 0;
    hdr[4] = 1;
    hdr[5] = 0xff;
    diary_rasterize_page(hdr, buf1, n, k);
    q->stored = 0xff;
    if (save.pet.diary_unseen_page == 0) {
        save.pet.diary_unseen_page = n + 1;
        save_game();
    }
    anim_busy = 0;
}

const char diary_templates[80][80] = {
    _("     &     でウキウキ!         をつくったよ♥♥♥"),
    _("     &     であそんだよ!        でだいしっぱい↓"),
    _("みんなで        をしたら……         ができたよ!♥♥♥"),
    _("みんなで        をしたら…だいしっぱい↓つぎはがんばろうね@"),
    _("みんなで        をしたら……         ができちゃった!♥"),
    _("みんなで        をしたら…だいしっぱい↓つぎはがんばろうね@"),
    _("     &     でウキウキ!!        ､ハラハラしたね～♪"),
    _("     と     はなかよしだね♪        ､たのしかった～@"),
    _("     &     でワクワク!!        ､またやりたいな～↑"),
    _("みんなで        をしたよ!とてもドキドキしたね!またやりたいな♪"),
    _("みんなで        をしたよ!とてもたのしかったな～!またやろうね♪"),
    _("みんなで        をしたよ!とてもハッピーなじかんをすごせたね♪♪"),
    _("     と     はなかよしだね↑         をたべたよ～@"),
    _("     &     でワクワク★★こうえんのアスレチックたのしかった↑"),
    _("     &     であそんだよ!すなのおやまがたくさんできたね～↑↑"),
    _("     &     でウキウキ!♪すべりだい､スイスイきもちいい～♥"),
    _("     と     はなかよしだね★がっこうでべんきょうがんばったよ!"),
    _("ほいくえんへいったよ!せんせいのかみしばいはいつもとってもたのしいな♪"),
    _("ほいくえんでみんなでおえかきしたよ♪     にもみせてあげたいな♥"),
    _("     &     であそんだよ♪おふろにはいってきもちよかった～♥♥"),
    _("     と     はおもしろい☼ふたりいっしょだと2ばいたのしいね↑"),
    _("     や     といっしょだとたのしいね♪たくさんわらったな～♪♪"),
    _("     の         といっしょにおにわではしゃいじゃったな～♪"),
    _("こうえんパトロールのおしごとをしたよ!きょうもこうえんはへいわだったよ♪"),
    _("こうえんパトロールのおしごと!まいごのこをおうちにつれていってあげたの!"),
    _("こうえんパトロールのおしごとをしたよ!たくさんあるいてすこしつかれたな@"),
    _("たまデパでおしごとをしたよ!いっぱいおきゃくさんがきてくれてうれしいな♪"),
    _("たまデパでおしごとをしたよ!かわいいおどうぐにかこまれてたのしかったよ♪"),
    _("たまデパでおしごとをしたよ!たくさんうれたけどすこしつかれちゃったなぁ…"),
    _("たまもりショップでおしごとをしたよ★いっぱいおきゃくさんがきてくれたよ♪"),
    _("たまもりショップでおしごとをしたよ★みんなおしゃれをしておでかけしてね♪"),
    _("たまもりショップでおしごとをしたよ!じょうずにたまもりができたんだ～★"),
    _("ミュージックカフェでおしごとをしたよ!おきゃくさんでおおにぎわいだった♪"),
    _("ミュージックカフェでおしごとをしたよみんなたのしくおしゃべり♪またきてね"),
    _("ミュージックカフェでおしごとをしたよおさらをわっちゃってだいしっぱい…!"),
    _("きょうはロボットのぎじゅつについてべんきょうしたよ!むずかしかったな～@"),
    _("きょうはロボットのしくみについてべんきょうしたよ!きかいってふしぎだね↑"),
    _("きょうはがっこうでロボットをつくってきたよ!きかいっておくがふかいなぁ★"),
    _("がっこうでロボットをつくったよ!         もがんばってたよ☼"),
    _("きょうはとつぜんロボットづくりのテスト!びっくりしたけどがんばったよ↑"),
    _("きょうはがっこうでヘアカットのれんしゅうをしたよ!むずかしかったな～@"),
    _("きょうはがっこうでシャンプーのれんしゅうをしたよ!たのしかったな↑↑"),
    _("きょうはがっこうでメイクのべんきょうをしたよ!はやくじょうたつしたいな★"),
    _("きょうはヘアカットのれんしゅう!         もがんばっていたよ♪♪"),
    _("きょうはとつぜんヘアカットのテスト!いままでのせいかをはっきできたかな?"),
    _("がっこうでミシンのれんしゅうをしたよ!はやくステキなふくをつくりたいな♪"),
    _("がっこうでししゅうのべんきょうをしたよ!いろんなぬいかたがあるんだね♪"),
    _("がっこうでさいほうどうぐについてべんきょうしたよ!いろんなものがあるね@"),
    _("きょうはミシンでシャツをつくったよ!         はセンスあるなぁ★"),
    _("きょうはとつぜんようふくづくりのテスト!じょうずにたいおうできたよ♪♪"),
    _("がっこうでドライヤーのれんしゅうをしたよ!かげんがむずかしいなぁ@"),
    _("きょうはブラシのつかいかたをべんきょうしたよ!みんなをキレイにしたいな♥"),
    _("きょうはたまペットのトリミングをしてあげたよ!ピカピカにしてあげたんだ♪"),
    _("がっこうでトリミングをしたよ!         はてさきがきようだなぁ@"),
    _("きょうはとつぜんトリミングのテスト!かわいくできあがってハッピー♥♥♥"),
    _("がっこうでとびばこのれんしゅう★たかいだんがとべるようにがんばろう!!!"),
    _("がっこうでバスケのしあいをしたよ!シュートをきめてだいかつやく↑↑↑"),
    _("がっこうで100たまMのタイムをはかったよ!しんきろくがでてうれしい↑"),
    _("きょうはスポーツかのじゅぎょう★         にはまけるもんか～～!"),
    _("きょうはとびばこのテスト!きんちょうしたけど､がんばってとんだよ☼☼"),
    _("がっこうでえんぎのれんしゅう!うまくえんじるのってむずかしいなぁ…@"),
    _("がっこうでうたとおどりのレッスン♪ステキなアイドルになるためにがんばろ!"),
    _("がっこうでうたとおどりのレッスン♪ひびのつみかさねがだいせつなんだね↑"),
    _("きょうはダンスのれんしゅう!         のダンスはキレがあるね♪"),
    _("きょうはダンスのテスト★きんちょうしたけど､おもいっきりおどれたよ!!"),
    _("がっこうでうたとおどりのレッスン♪たのしくできてノリノリハッピー♥♥♥"),
    _("がっこうではっせいれんしゅう!ステキなこえがでるようにとっくんしなきゃ♪"),
    _("きょうはみんなでがっしょうしたよ!きれいにハモれるときもちがいいね♪♪"),
    _("きょうはうたのれんしゅうをしたよ♪         のこえにかんどう…!"),
    _("きょうはうたのテスト!ドキドキしたけど､せんせいにもほめてもらえたよ♪"),
    _("きょうはちょうりじっしゅうでカレーをつくったよ!おいしくできたんだ～♥"),
    _("きょうはちょうりじっしゅうでケーキをつくったよ!かわいくできたよね～♪"),
    _("がっこうでちょうりじっしゅう!つまみぐいをしておなかいっぱいに…☼"),
    _("きょうはりょうりかのじゅぎょう!         にはまけられないの★ "),
    _("きょうはりょうりのテスト★じょうずにできたからせんせいにほめられたよ!!"),
    _("{00}{00}"),
};
const u8 D_255EF18[] = _("ほんとうに?");
const u8 D_255EF28[] = _("おわかれ?");
const u8 D_255EF34[] = _("おともだち\nからでんごんだよ～☼");
const u8 D_255EF58[] = _("プロフィール");
const u8 D_255EF68[] = _("メッセへんしゅう");
const u8 D_255EF7C[] = _("メッセりれき");
const u8 D_255EF8C[] = _("おわかれ");
const u8 D_255EF98[] = _("けす?");
const u8 D_255EFA0[] = _("はい");
const u8 D_255EFA8[] = _("いいえ");
const u8 D_255EFB0[] = _("メッセが");
const u8 D_255EFBC[] = _("いっぱいだよ!");
const u8 D_255EFCC[] = _("いらないメッセを");
const u8 D_255EFE0[] = _("けしてね!");
const u8 D_255EFEC[] = _("さくじょしたよ!");
const u8 D_255F000[] = _("ゆめキャンっち");
const u8 D_255F010[] = _("チャピっち");
const u8 D_255F01C[] = _("もなトトっち");
const u8 D_255F02C[] = _("リンスっち");
const u8 D_255F038[] = _("ふりらぶっち");
const u8 *const album_special_names[5] = { D_255F000, D_255F010, D_255F01C, D_255F02C, D_255F038 };
const u8 D_255F05C[] = _("?????");
const u8 D_255F068[] = _("たまともてちょう");
const u8 D_255F07C[] = _("ともだちてちょう");
const u8 D_255F090[] = _("きねんしゃしん");
const u8 D_255F0A0[] = _("こそだてちょう");
const u8 D_255F0B0[] = _("きせつかざり");
const struct PlaceEntry ui_notebook_menu_tbl[8] = {
    { (u32)D_255F068, (int (*)(void))town_always_open, 0, (u8 (*)(void))album_browser, 0 },
    { (u32)D_255F07C, ui_notebook_friends_enable, 0, (u8 (*)(void))friend_book_open, 0 },
    { (u32)D_255F090, ui_notebook_photos_enable, 0, (u8 (*)(void))ui_notebook_photos, 0 },
    { (u32)D_255F0A0, (int (*)(void))ui_notebook_childcare_enable, 0, (u8 (*)(void))ui_notebook_childcare, 0 },
    { (u32)D_255F0B0, ui_notebook_decorations_enable, 0, (u8 (*)(void))ui_notebook_decorations, 0 },
};
const u8 D_255F160[] = _("         ");
const u8 D_255F174[] = _("        ");
const u8 D_255F188[] = _("えにっきちゅう");
const u8 D_255F198[] = _("えにっきが");
const u8 D_255F1A4[] = _("いっぱいに");
const u8 D_255F1B0[] = _("なったよ!");
const u8 D_255F1BC[] = _("いっぱいで");
const u8 D_255F1C8[] = _("かけなかったよ!");

/*
 * For a meal page (event 2000, the meal with the partners; no writer) of the diary state p: choose which partner image
 * of the image list q goes where by the variant x034 (D_00841B8 = q[4][0], one of
 * anim_pic_slots[1][1] / [1][2] / [0][1]). Nothing when the page has already been shown and
 * stored (state > 4, x038 = 0xFF).
 */
void diary_assign_partner_pics(struct DiaryDraft *p, u32 (*q)[3])
{
    u16 *v = &p->event;
    u8 k;
    u16 x;

    if (p->state > 4 && p->stored == 0xff)
        return;
    x = *v;
    k = p->variant;
    if (x != 2000)
        return;
    anim_pic_slots[4][0] = (const u8 *)q[4][0];
    switch (k) {
    case 0:
    case 3:
        anim_pic_slots[1][1] = (const u8 *)q[1][1];
        break;
    case 1:
    case 4:
        anim_pic_slots[1][2] = (const u8 *)q[1][2];
        break;
    case 2:
    default:
        anim_pic_slots[0][1] = (const u8 *)q[0][1];
        break;
    }
}

#define P(x) ((const u8 *)(x))
#define S(o) ((u8 *)a1 + (o))
/*
 * Picture-diary page composer: build the page text (a0) and scene for draft a1 by event
 * code x032 (1000.. = events) and variant x034: the template from diary_templates gets the
 * draft's name fields spliced in (S(0x28) = the 5-char field, S(5) = the 8-char, S(0xD) /
 * S(0x1F) = the 9-char ones), the stored images a2 (the diary_images list: rows 0..2 =
 * partner poses, 5 = backgrounds) are assigned to anim_pic_slots and the matching composition
 * function draws the sprite list. Returns the sprite count.
 */
u8 diary_compose_page(char *a0, struct DiaryDraft *a1, u32 (*a2)[3])
{
    u16 v = a1->event;
    int k = a1->variant;
    u8 r = gfx_object_count;
    int o1, o2;

    switch (v) {
    case 1000:
        if (k == 0) {
            diary_copy_template(a0, 0);
            memcpy(a0, S(0x28), 5);
            memcpy(a0 + 6, a1, 5);
            memcpy(a0 + 0x11, S(0xd), 9);
            anim_pic_slots[0][0] = (const u8 *)a2[0][0];
            anim_pic_slots[1][0] = (const u8 *)a2[1][0];
            anim_pic_slots[2][0] = (const u8 *)a2[2][0];
            r = diary_page_make_success(P(a2[5][0]), P(a2[5][1]));
        } else {
            diary_copy_template(a0, 1);
            memcpy(a0, S(0x28), 5);
            memcpy(a0 + 6, a1, 5);
            memcpy(a0 + 0x12, S(5), 8);
            anim_pic_slots[0][1] = (const u8 *)a2[0][1];
            anim_pic_slots[1][1] = (const u8 *)a2[1][1];
            anim_pic_slots[2][1] = (const u8 *)a2[2][1];
            r = diary_page_make_failure(P(a2[5][0]), P(a2[5][1]));
        }
        break;
    case 1001:
        if (k == 0) {
            diary_copy_template(a0, 2);
            memcpy(a0 + 4, S(5), 8);
            memcpy(a0 + 0x12, S(0xd), 9);
            anim_pic_slots[0][0] = (const u8 *)a2[0][0];
            anim_pic_slots[1][0] = (const u8 *)a2[1][0];
            anim_pic_slots[2][0] = (const u8 *)a2[2][0];
            r = diary_page_make_success(P(a2[5][0]), P(a2[5][1]));
        } else {
            diary_copy_template(a0, 3);
            memcpy(a0 + 4, S(5), 8);
            anim_pic_slots[0][1] = (const u8 *)a2[0][1];
            anim_pic_slots[1][1] = (const u8 *)a2[1][1];
            anim_pic_slots[2][1] = (const u8 *)a2[2][1];
            r = diary_page_make_failure(P(a2[5][0]), P(a2[5][1]));
        }
        break;
    case 1002:
        if (k == 0) {
            diary_copy_template(a0, 4);
            memcpy(a0 + 4, S(5), 8);
            memcpy(a0 + 0x12, S(0xd), 9);
            anim_pic_slots[0][0] = (const u8 *)a2[0][0];
            anim_pic_slots[1][0] = (const u8 *)a2[1][0];
            anim_pic_slots[2][0] = (const u8 *)a2[2][0];
            r = diary_page_make_success(P(a2[5][0]), P(a2[5][1]));
        } else {
            diary_copy_template(a0, 5);
            memcpy(a0 + 4, S(5), 8);
            anim_pic_slots[0][1] = (const u8 *)a2[0][1];
            anim_pic_slots[1][1] = (const u8 *)a2[1][1];
            anim_pic_slots[2][1] = (const u8 *)a2[2][1];
            r = diary_page_make_failure(P(a2[5][0]), P(a2[5][1]));
        }
        break;
    case 1003:
        switch (k) {
        case 0: diary_copy_template(a0, 6); o1 = 0; o2 = 0x12; break;
        case 1: diary_copy_template(a0, 7); o1 = 0; o2 = 0x13; break;
        case 2: diary_copy_template(a0, 8); o1 = 0; o2 = 0x13; break;
        case 3: diary_copy_template(a0, 6); o1 = 0; o2 = 0x12; break;
        case 4: diary_copy_template(a0, 7); o1 = 0; o2 = 0x13; break;
        default: diary_copy_template(a0, 8); o1 = 0; o2 = 0x12; break;
        }
        if (k <= 2) {
            memcpy(a0 + o1, S(0x28), 5);
            memcpy(a0 + 6, a1, 5);
            memcpy(a0 + o2, S(5), 8);
            anim_pic_slots[0][0] = (const u8 *)a2[0][0];
            anim_pic_slots[1][0] = (const u8 *)a2[1][0];
            anim_pic_slots[2][0] = (const u8 *)a2[2][0];
            r = diary_page_play_a(P(a2[5][0]));
        } else {
            memcpy(a0 + o1, S(0x28), 5);
            memcpy(a0 + 6, a1, 5);
            memcpy(a0 + o2, S(5), 8);
            anim_pic_slots[0][0] = (const u8 *)a2[0][0];
            anim_pic_slots[1][0] = (const u8 *)a2[1][0];
            anim_pic_slots[2][0] = (const u8 *)a2[2][0];
            r = diary_page_play_b();
        }
        break;
    case 1004:
        switch (k) {
        case 0: case 3: diary_copy_template(a0, 9); break;
        case 1: case 4: diary_copy_template(a0, 10); break;
        case 2: default: diary_copy_template(a0, 11); break;
        }
        if (k <= 2) {
            memcpy(a0 + 4, S(5), 8);
            anim_pic_slots[0][0] = (const u8 *)a2[0][0];
            anim_pic_slots[1][0] = (const u8 *)a2[1][0];
            anim_pic_slots[2][0] = (const u8 *)a2[2][0];
            r = diary_page_play_a(P(a2[5][0]));
        } else {
            memcpy(a0 + 4, S(5), 8);
            anim_pic_slots[0][0] = (const u8 *)a2[0][0];
            anim_pic_slots[1][0] = (const u8 *)a2[1][0];
            anim_pic_slots[2][0] = (const u8 *)a2[2][0];
            r = diary_page_play_b();
        }
        break;
    case 2000:
        diary_copy_template(a0, 0xc);
        memcpy(a0, S(0x28), 5);
        memcpy(a0 + 6, a1, 5);
        memcpy(a0 + 0x13, S(0xd), 9);
        switch (k) {
        case 0: r = diary_page_meal_a(P(a2[5][0])); break;
        case 1: r = diary_page_meal_b(P(a2[5][0])); break;
        case 2: r = diary_page_meal_c(P(a2[5][0])); break;
        case 3: r = diary_page_meal_d(P(a2[5][0])); break;
        case 4: r = diary_page_meal_e(P(a2[5][0])); break;
        default: r = diary_page_meal_f(P(a2[5][0])); break;
        }
        break;
    case 2001:
        switch (k) {
        case 0:
            diary_copy_template(a0, 0xd);
            anim_pic_slots[1][2] = (const u8 *)a2[1][2];
            r = diary_page_park_athletic();
            break;
        case 1:
            diary_copy_template(a0, 0xe);
            anim_pic_slots[1][2] = (const u8 *)a2[1][2];
            r = diary_page_park_sand();
            break;
        default:
            diary_copy_template(a0, 0xf);
            anim_pic_slots[2][0] = (const u8 *)a2[1][2];
            r = diary_page_park_slide();
            break;
        }
        memcpy(a0, S(0x28), 5);
        memcpy(a0 + 6, a1, 5);
        break;
    case 2002:
        diary_copy_template(a0, 0x10);
        memcpy(a0, S(0x28), 5);
        memcpy(a0 + 6, a1, 5);
        anim_pic_slots[0][0] = (const u8 *)a2[0][0];
        anim_pic_slots[2][2] = (const u8 *)a2[2][2];
        anim_pic_slots[3][0] = (const u8 *)a2[3][0];
        r = diary_page_school_friend(P(a2[5][0]));
        break;
    case 2003:
        if (k == 0) {
            diary_copy_template(a0, 0x11);
            r = diary_page_nursery_story();
        } else {
            diary_copy_template(a0, 0x12);
            memcpy(a0 + 0x12, S(0x28), 5);
            anim_pic_slots[0][0] = (const u8 *)*chara_sprite_set33_pose2;
            r = diary_page_nursery_drawing();
        }
        break;
    case 2004:
        diary_copy_template(a0, 0x13);
        memcpy(a0, S(0x28), 5);
        memcpy(a0 + 6, a1, 5);
        anim_pic_slots[0][0] = (const u8 *)a2[0][0];
        r = diary_page_bath();
        break;
    case 2005:
        diary_copy_template(a0, 0x14);
        memcpy(a0, S(0x28), 5);
        memcpy(a0 + 6, a1, 5);
        if (k == 0) {
            anim_pic_slots[2][1] = (const u8 *)a2[2][1];
            anim_pic_slots[0][0] = (const u8 *)a2[0][0];
            r = diary_page_together_a();
        } else {
            anim_pic_slots[2][1] = (const u8 *)a2[2][1];
            anim_pic_slots[0][0] = (const u8 *)a2[0][0];
            r = diary_page_together_b();
        }
        break;
    case 2006:
        diary_copy_template(a0, 0x15);
        memcpy(a0, S(0x28), 5);
        memcpy(a0 + 6, a1, 5);
        anim_pic_slots[0][0] = (const u8 *)a2[0][0];
        r = diary_page_comm();
        break;
    case 2007:
        diary_copy_template(a0, 0x16);
        memcpy(a0, a1, 5);
        memcpy(a0 + 6, S(0x1f), 9);
        anim_pic_slots[0][0] = (const u8 *)a2[0][0];
        r = diary_page_garden();
        break;
    case 3000:
        if (k == 0) diary_copy_template(a0, 0x17); else if (k == 1) diary_copy_template(a0, 0x18); else diary_copy_template(a0, 0x19);
        r = diary_page_job_patrol();
        break;
    case 3001:
        if (k == 0) diary_copy_template(a0, 0x1a); else if (k == 1) diary_copy_template(a0, 0x1b); else diary_copy_template(a0, 0x1c);
        r = diary_page_job_depa();
        break;
    case 3002:
        if (k == 0) diary_copy_template(a0, 0x1d); else if (k == 1) diary_copy_template(a0, 0x1e); else diary_copy_template(a0, 0x1f);
        r = diary_page_job_tamamori();
        break;
    case 3003:
        if (k == 0) diary_copy_template(a0, 0x20); else if (k == 1) diary_copy_template(a0, 0x21); else diary_copy_template(a0, 0x22);
        r = diary_page_job_cafe();
        break;
    case 4000:
        switch (k) {
        case 0: diary_copy_template(a0, 0x23); break;
        case 1: diary_copy_template(a0, 0x24); break;
        case 2: diary_copy_template(a0, 0x25); break;
        case 3: diary_copy_template(a0, 0x26); memcpy(a0 + 0x10, S(0x1f), 9); break;
        default: diary_copy_template(a0, 0x27); break;
        }
        anim_pic_slots[0][0] = (const u8 *)a2[0][0];
        anim_pic_slots[2][2] = (const u8 *)a2[2][2];
        r = diary_page_school_lesson(P(a2[5][0]));
        break;
    case 4001:
        switch (k) {
        case 0: diary_copy_template(a0, 0x28); break;
        case 1: diary_copy_template(a0, 0x29); break;
        case 2: diary_copy_template(a0, 0x2a); break;
        case 3: diary_copy_template(a0, 0x2b); memcpy(a0 + 0x10, S(0x1f), 9); break;
        case 4: diary_copy_template(a0, 0x2c); break;
        case 5: diary_copy_template(a0, 0x2d); break;
        case 6: diary_copy_template(a0, 0x2e); break;
        case 7: diary_copy_template(a0, 0x2f); break;
        case 8: diary_copy_template(a0, 0x30); memcpy(a0 + 0x12, S(0x1f), 9); break;
        case 9: diary_copy_template(a0, 0x31); break;
        case 10: diary_copy_template(a0, 0x32); break;
        case 11: diary_copy_template(a0, 0x33); break;
        case 12: diary_copy_template(a0, 0x34); break;
        case 13: diary_copy_template(a0, 0x35); memcpy(a0 + 0xf, S(0x1f), 9); break;
        default: diary_copy_template(a0, 0x36); break;
        }
        anim_pic_slots[0][0] = (const u8 *)a2[0][0];
        anim_pic_slots[2][2] = (const u8 *)a2[2][2];
        r = diary_page_school_lesson(P(a2[5][0]));
        break;
    case 4002:
        switch (k) {
        case 0: diary_copy_template(a0, 0x37); break;
        case 1: diary_copy_template(a0, 0x38); break;
        case 2: diary_copy_template(a0, 0x39); break;
        case 3: diary_copy_template(a0, 0x3a); memcpy(a0 + 0x10, S(0x1f), 9); break;
        default: diary_copy_template(a0, 0x3b); break;
        }
        anim_pic_slots[0][0] = (const u8 *)a2[0][0];
        anim_pic_slots[2][2] = (const u8 *)a2[2][2];
        anim_pic_slots[3][0] = (const u8 *)a2[3][0];
        r = diary_page_school_friend(P(a2[5][0]));
        break;
    case 4003:
        switch (k) {
        case 0: diary_copy_template(a0, 0x3c); break;
        case 1: diary_copy_template(a0, 0x3d); break;
        case 2: diary_copy_template(a0, 0x3e); break;
        case 3: diary_copy_template(a0, 0x3f); memcpy(a0 + 0xe, S(0x1f), 9); break;
        case 4: diary_copy_template(a0, 0x40); break;
        case 5: diary_copy_template(a0, 0x41); break;
        case 6: diary_copy_template(a0, 0x42); break;
        case 7: diary_copy_template(a0, 0x43); break;
        case 8: diary_copy_template(a0, 0x44); memcpy(a0 + 0x11, S(0x1f), 9); break;
        default: diary_copy_template(a0, 0x45); break;
        }
        anim_pic_slots[0][0] = (const u8 *)a2[0][0];
        anim_pic_slots[2][2] = (const u8 *)a2[2][2];
        r = diary_page_school_lesson(P(a2[5][0]));
        break;
    case 4004:
        switch (k) {
        case 0: diary_copy_template(a0, 0x46); break;
        case 1: diary_copy_template(a0, 0x47); break;
        case 2: diary_copy_template(a0, 0x48); break;
        case 3: diary_copy_template(a0, 0x49); memcpy(a0 + 0x10, S(0x1f), 9); break;
        default: diary_copy_template(a0, 0x4a); break;
        }
        anim_pic_slots[0][0] = (const u8 *)a2[0][0];
        anim_pic_slots[2][2] = (const u8 *)a2[2][2];
        r = diary_page_school_lesson(P(a2[5][0]));
        break;
    }
    return r;
}

/*
 * Can a diary page be written (not full)?
 */
int diary_can_write(void)
{
    struct DiaryDraft *p = diary_draft;
    int r;

    if (p->state > 3 && p->stored == 0xff)
        r = 0;
    else
        r = 1;
    return r;
}

/*
 * Diary page writer: store a rendered page (a = the data; the halfword count comes from
 * sys_awake_seconds, which the renderer reuses as its byte counter) into the next free diary page
 * k (0x2570000 + k * 0x2000); 22 pages written -> "full" notice (x03c = 1), more -> "could
 * not write" (x03c = 2, nothing stored); state 4, x038 = 0xFF; the first page sets
 * pet.diary_unseen_page (the unseen-page marker) and saves.
 */
int diary_store_page(int a)
{
    struct DiaryDraft *q;
    u16 *p;
    u16 *s;
    int k, i, r;
    u8 t;

    q = diary_draft;
    p = (u16 *)a;
    t = q->state;
    if (t > 2)
        goto fail;
    if (t > 3 && q->stored == 0xff)
        goto fail;
    k = diary_page_count();
    if (k > 0x16) {
        q->store_result = 2;
        goto fail;
    }
    if (k != 0x16)
        goto zero;
    q->store_result = 1;
    goto go;
fail1:
    sys_irq_enable();
fail:
    return -1;
zero:
    q->store_result = 0;
go:
    sys_irq_disable();
    i = 0;
    s = (u16 *)FLASH_DIARY + (k << 12);
again:
    if (func_0000B52(s, *p, FLASH_BASE) != 0)
        goto fail1;
    i++;
    s++;
    p++;
    if (i < 0x142A / 2)     /* the page: 0x142A bytes */
        goto again;
    sys_irq_enable();
    q->stored = 0xff;
    q->state = 4;
    if (save.pet.diary_unseen_page == 0) {
        save.pet.diary_unseen_page = k + 1;
        save_game();
    }
    return 0;
}

/*
 * Store a rendered picture into diary page k directly: 0x24 bytes of text (b), then
 * a[0..5] = width, height and two more halfwords, then the raw palette data of every
 * sprite in the list (n slots, func_000014A reads their entries, byte-swapped) and finally
 * the page bitmap, one byte per pixel, produced by SOFTWARE RASTERISATION of the current
 * sprite list (gfx_raster_pixel). This is how the diary keeps pictures of characters/items
 * that may later be deleted.
 */
int diary_rasterize_page(u8 *a, u8 *b, int k, u8 n)
{
    int j, r, off, i;
    u16 *s, *d;
    u8 *p;
    u16 v;
    int t;
    int x0 = 0x40, y0;

    off = 0;
    r = -1;
    s = (u16 *)FLASH_DIARY + (k << 12);
    p = b;
    sys_irq_disable();
    for (i = off; i < 0x24; i += 2, off += 2) {
        d = (u16 *)((u8 *)s + off);
        t = p[i + 1] << 8;
        t |= p[i];
        if (func_0000B52(d, t, FLASH_BASE) != 0)
            goto out;
    }
    d = (u16 *)((u8 *)s + off);
    t = a[1] << 8;
    t |= a[0];
    if (func_0000B52(d, t, FLASH_BASE) != 0)
        goto out;
    off += 2;
    d = (u16 *)((u8 *)s + off);
    t = a[3] << 8;
    t |= a[2];
    if (func_0000B52(d, t, FLASH_BASE) != 0)
        goto out;
    off += 2;
    d = (u16 *)((u8 *)s + off);
    t = a[5] << 8;
    t |= a[4];
    if (func_0000B52(d, t, FLASH_BASE) != 0)
        goto out;
    off += 2;
    for (j = 0; j < n; j++) {
        if (gfx_objects[j].image == 0)
            continue;
        for (i = 0; i < gfx_objects[j].image[2]; i++, off += 2) {
            v = func_000014A(&gfx_objects[j], i);
            if (func_0000B52((u16 *)((u8 *)s + off), (v & 0xff) << 8 | v >> 8, FLASH_BASE) != 0)
                goto out;
        }
    }
    y0 = 0x1c;
    for (j = y0; j < y0 + a[1]; j++) {
        for (i = x0; i < x0 + a[0]; i += 2, off += 2) {
            v = gfx_raster_pixel(i, j, n);
            v |= gfx_raster_pixel(i + 1, j, n) << 8;
            if (func_0000B52((u16 *)((u8 *)s + off), v, FLASH_BASE) != 0)
                goto out;
        }
    }
    r = 0;
out:
    sys_irq_enable();
    return r;
}

/*
 * Software rasteriser: the global palette index of pixel (x, y) among the first n sprites:
 * the topmost sprite covering it (0x4000 = skipped), its pixel decoded by gfx_image_pixel
 * (0x1000 = read from the frame buffer gfx_frame_buffer instead, 0x2000 = inverted index), offset
 * by the palette sizes of the sprites below it; 0 when no sprite covers it. Used by
 * diary_rasterize_page for the picture-diary bitmaps.
 */
u8 gfx_raster_pixel(s16 x, s16 y, u8 n)
{
    s16 i, dx, dy;
    struct GfxObject *e;
    u8 c, s, j;

    for (i = n - 1; i >= 0; i--) {
        e = &gfx_objects[i];
        if (e->image == 0)
            continue;
        if (e->flags & 0x4000)
            continue;
        dy = y - e->y;
        if (dy < 0)
            continue;
        if (e->height <= dy)
            continue;
        dx = x - e->x;
        if (dx < 0)
            continue;
        if (e->width <= dx)
            continue;
        if (e->flags & 0x1000) {
            x += (y - 0x1c) << 7;
            return ((u8 *)&gfx_frame_buffer)[(u16)x];
        }
        c = gfx_image_pixel(e, dx, dy);
        if (e->flags & 0x2000) {
            c = ~c;
            c = c % e->image[2];
        }
        if (c != 0) {
            s = j = 0;
            for (; j < i; j++)
                if (gfx_objects[j].image != 0)
                    s += gfx_objects[j].image[2];
            return c + s;
        }
    }
    return 0;
}

struct ImageData { u8 width; u8 height; u8 data[1]; };
/*
 * Pixel of object p's image at (x, y): flag 0x40 swaps x/y (transpose), 0x20 / 0x10 flip
 * y / x; 8-bit image when bit 15 of x00e is set, else 4-bit.
 */
u8 gfx_image_pixel(struct GfxObject *p, u8 x, u8 y)
{
    struct ImageData *img;
    u8 t;

    img = (struct ImageData *)p->image;
    if (p->flags & 0x40) {
        t = x;
        x = y;
        y = t;
    }
    if (p->flags & 0x20)
        y = p->height - y - 1;
    if (p->flags & 0x10)
        x = p->width - x - 1;
    if ((s16)p->flags < 0)
        return img->data[p->pixel_offset + y * img->width + x + 4];
    if (x & 1)
        return img->data[p->pixel_offset + y * (img->width >> 1) + (x >> 1) + 4] >> 4;
    return img->data[p->pixel_offset + y * (img->width >> 1) + (x >> 1) + 4] & 0xf;
}

/*
 * Compare function (descending).
 */
int util_cmp_desc(int *a, int *b)
{
    return *b - *a;
}

/*
 * Swap two 8-byte entries of t.
 */
void util_swap_entries8(struct AlbumSortEntry *t, int i, int j)
{
    struct AlbumSortEntry tmp;

    tmp.key = t[i].key;
    tmp.index = t[i].index;
    t[i].key = t[j].key;
    t[i].index = t[j].index;
    t[j].key = tmp.key;
    t[j].index = tmp.index;
}

/*
 * Sort entries by their first word, descending, 0xFFFFFFFF last.
 */
void util_sort_desc(struct AlbumSortEntry *t, u32 n)
{
    u32 i, j;

    if (n <= 1)
        return;
    for (i = 0; i < n - 1; i++)
        for (j = i + 1; j < n; j++)
            if (t[j].key != 0xffffffff) {
                if (t[i].key == 0xffffffff)
                    util_swap_entries8(t, i, j);
                else if (t[i].key < t[j].key)
                    util_swap_entries8(t, i, j);
            }
}

/*
 * Tama-friends book / character album (たまともてちょう): sector 0x37 (0x22F0000) holds
 * 80 records of 0x1000 bytes (a received character record: u16 species group at +0, u32
 * timestamp/serial at +8, ...). album_draw_overview(n, list) draws the overview: one icon per
 * species group met (0x10..0x15 and 0x3E9..0x3EE) and the count.
 *
 * Collect the used records sorted by that u32 descending (util_sort_desc, a selection sort
 * with erased entries last) into album_order[80]; returns their number.
 */
u8 album_sort_records(void)
{
    struct AlbumSortEntry buf[80];
    u8 *p;
    u16 *s;
    u8 i;
    u8 n;

    n = 0;
    for (i = 0; i <= 0x4f; i++) {
        p = flash_sector_addr(FLASH_SEC_ALBUM) + (i << 12);
        buf[i].key = *(u32 *)(p + 8);
        for (s = (u16 *)p; s < (u16 *)(p + 0x10); s++) {
            if (*s != 0xffff) {
                buf[i].index = i;
                break;
            }
            buf[i].index = 0xff;
        }
    }
    util_sort_desc(buf, 0x50);
    for (i = 0; i <= 0x4f; i++)
        save.album_order[i] = buf[i].index;
    for (i = 0; i <= 0x4f; i++) {
        p = flash_sector_addr(FLASH_SEC_ALBUM) + (i << 12);
        for (s = (u16 *)p; s < (u16 *)(p + 0x10); s++) {
            if (*s != 0xffff) {
                n++;
                break;
            }
        }
    }
    return n;
}

/*
 * Create a display object for image e centred at (x, y) in main-area coordinates.
 */
void gfx_object_create_centered(s16 x, s16 y, u16 c, u8 d, const u8 *e)
{
    gfx_object_create(x - (e[0] >> 1), y - (e[1] >> 1) + 0x1c, c, d, 0, (const u8 *)e);
}

/*
 * Album page with n characters from list (pictures and names).
 */
void album_draw_overview(u8 n, void *list)
{
    u16 e[16];
    u8 *tbl[10];
    int mask;
    int i;
    u8 *p;

    mask = 0;
    ui_new_screen();
    gfx_object_create_centered(0x40, 0x24, 0, 0, D_2114320);
    for (i = 0; i < n; i++) {
        p = flash_sector_addr(FLASH_SEC_ALBUM) + (((u8 *)list)[i] << 12);
        memcpy(e, p, 0x20);
        switch (e[0]) {
        case 0x10:
            if (!(mask & 1)) {
                gfx_object_create_centered(0x1b, 0x1d, 0x10, 0, D_2116748);
                mask |= 1;
            }
            break;
        case 0x11:
            if (!(mask & 2)) {
                gfx_object_create_centered(0x27, 0x2f, 0x20, 0, D_211683C);
                mask |= 2;
            }
            break;
        case 0x12:
            if (!(mask & 4)) {
                gfx_object_create_centered(0x2a, 0xa, 0x30, 0, D_2116A20);
                mask |= 4;
            }
            break;
        case 0x13:
            if (!(mask & 8)) {
                gfx_object_create_centered(0xc, 0x2f, 0x40, 0, D_211692C);
                mask |= 8;
            }
            break;
        case 0x14:
            if (!(mask & 0x10)) {
                gfx_object_create_centered(0xc, 0xa, 0x50, 0, D_2116B14);
                mask |= 0x10;
            }
            break;
        case 0x3e9:
            if (!(mask & 0x20)) {
                gfx_object_create_centered(0x67, 0x1c, 0x60, 0, D_2116C08);
                mask |= 0x20;
            }
            break;
        case 0x3ea:
            if (!(mask & 0x40)) {
                gfx_object_create_centered(0x59, 0x2e, 0x70, 0, D_2116EDC);
                mask |= 0x40;
            }
            break;
        case 0x3eb:
            if (!(mask & 0x80)) {
                gfx_object_create_centered(0x74, 0xa, 0x80, 0, D_2116CF8);
                mask |= 0x80;
            }
            break;
        case 0x3ec:
            if (!(mask & 0x100)) {
                gfx_object_create_centered(0x74, 0x2f, 0x90, 0, D_2116DE8);
                mask |= 0x100;
            }
            break;
        case 0x3ed:
            if (!(mask & 0x200)) {
                gfx_object_create_centered(0x59, 0xa, 0xa0, 0, D_2116FD0);
                mask |= 0x200;
            }
            break;
        case 0x15:
            if (!(mask & 0x400)) {
                gfx_object_create_centered(0x33, 0x1d, 0xb0, 0, D_2127BCC);
                mask |= 0x400;
            }
            break;
        case 0x3ee:
            if (!(mask & 0x800)) {
                gfx_object_create_centered(0x4a, 0x1d, 0xc0, 0, D_2127CC0);
                mask |= 0x800;
            }
            break;
        }
    }
    memcpy(tbl, gfx_digit_pics, 0x28);
    if (n > 9)
        gfx_object_create_centered(0x56, 0x3f, 0xd0, 0, tbl[(u8)(n / 10)]);
    gfx_object_create_centered(0x62, 0x3f, 0xd0, 0, tbl[(u8)(n % 10)]);
    if (n != 0)
        gfx_object_create(0x72, 0x58, 0xd8, 0, 0, D_21059A4);
    func_00009A6();
}

extern u8 D_21432AC[];
extern u8 D_214330C[];
extern u8 D_214336C[];
extern u8 D_21433CC[];
extern u8 D_214342C[];
extern u8 D_214348C[];
u8 *const gfx_digit_pics[10] = {
    D_2142E38, D_214318C, D_21431EC, D_214324C, D_21432AC,
    D_214330C, D_214336C, D_21433CC, D_214342C, D_214348C,
};

/*
 * Character album browser (sorted by album_sort_records): pictures and names of the saved
 * characters.
 */
int album_browser(void)
{
    u8 buf[0x20];
    u8 sel;
    int i;
    u8 *p;
    const u8 *q;
    u8 a, k, n;

    i = -1;
    sel = album_sort_records();
again:
    if (i == -1) {
        album_draw_overview(sel, save.album_order);
    } else {
        if (save.album_order[i] == 0xff) {
            do {
                i++;
                if (i > 0x4f) {
                    i = -1;
                    goto again;
                }
            } while (save.album_order[i] == 0xff);
        }
        p = flash_sector_addr(FLASH_SEC_ALBUM) + (save.album_order[i] << 12);
        memcpy(buf, p, 0x20);
        p += 0x20;
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, D_2115530);
        gfx_draw_number_b(0x54, 0x48, 0x20, *(u16 *)(buf + 2));
        q = D_20F5384;
        gfx_object_create(0x66 - (q[0] >> 1), 0x4f - (q[1] >> 1), 0x40, 0, 0, (const u8 *)q);
        gfx_object_create(0x25 - (p[4] >> 1), 0x40 - (p[5] >> 1), 0x50, 0, 0, (const u8 *)(p + 4));
        q = *(u16 *)(buf + 4) == 0 ? D_20F4BC4 : D_20F4CEC;
        gfx_object_create(0x5d - (q[0] >> 1), 0x3f - (q[1] >> 1), 0x80, 0, 0, (const u8 *)q);
        a = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_centered(a, 0x24, 0x90, buf + 0xc, 0, 9);
        if (sel != 0) {
            n = 0;
            k = i + 1;
            if (k != 0x50)
                for (; k <= 0x4f; k++)
                    if (save.album_order[k] != 0xff) {
                        n++;
                        break;
                    }
            if (n == 0)
                gfx_object_create(0x72, 0x58, 0xa0, 1, 0, D_21059A4);
            else
                gfx_object_create(0x72, 0x58, 0xa0, 0, 0, D_21059A4);
        }
        func_00009A6();
    }
    btn_a_presses = 0;
    btn_b_presses = 1;
    btn_c_presses = 0;
    for (;;) {
        if (btn_a_presses != 0) {
            ui_timeout = 10;
            i++;
            if (i > 0x4f)
                i = -1;
            goto again;
        }
        if (btn_c_presses != 0)
            return 0;
        sys_idle_tick();
        if (ui_timeout == 0)
            return 0;
    }
}

/*
 * The RAM variables this file defines.
 */
struct DiaryDraft *diary_draft;
