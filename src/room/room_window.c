/*
 * What the room's window shows (weather by period and month, passers-by, seasonal flower), the
 * daily random event that sends visitors, the signpost outings and their two destinations
 * (Tamamori shop, Gotchi Reform). Also the house-level gift and the town visit flags.
 *
 * Code 240F818..2410F86, .rodata from 253EFF8.
 */
#include "proto/room/room_window.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/room/room_draw.h"
#include "proto/town/town_places.h"
#include "proto/shop/shop_reform.h"
#include "proto/shop/shop_browser.h"
#include "proto/anim/anim_scripts.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_message.h"
#include "proto/ir/ir_driver.h"
#include "proto/ui/ui_menu.h"
#include "proto/visit/visit_guests.h"
#include "proto/visit/visit_gift.h"
#include "proto/visit/visit_friend.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/owner/owner_profile.h"
#include "proto/spot/spot_reader.h"
#include "proto/comm/comm_outcome.h"
#include "proto/comm/comm_checks.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * F24 = WEATHER, SEASONS and the room window, plus the random outings (the park gate, the
 * "Tama-mori" shop, Gotchi Reform) - unreachable in this firmware: their picker
 * outing_random has no caller and no pointer. pet.weather_today = today's weather (drawn each day by
 * room_weather_draw_daily from the month tables room_weather_month_odds (probabilities) / room_weather_month_ids (weather ids),
 * with fixed-weather date ranges (e.g. 2/6-10 snow 0x13, 6/12-16 rain 11) and clear skies
 * on birthdays). Also the touch-count levels and the seasonal decorations.
 *
 * Current weather shown now: pet.weather_now = room_weather_by_period[pet.weather_today = the room's weather
 * set / region][time-of-day period rtc_day_period].
 */
void room_weather_update_now(void)
{
    int t = rtc_day_period();

    save.pet.weather_now = room_weather_by_period[save.pet.weather_today][t];
}

const u8 room_weather_by_period[20][5] = {
    { 0, 0, 0, 0, 0 },
    { 0, 0, 1, 1, 1 },
    { 0, 0, 1, 0, 0 },
    { 0, 0, 0, 2, 0 },
    { 2, 1, 0, 0, 0 },
    { 1, 1, 0, 0, 0 },
    { 0, 0, 1, 2, 2 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 2, 2 },
    { 1, 2, 1, 2, 1 },
    { 1, 1, 2, 2, 2 },
    { 2, 2, 2, 2, 2 },
    { 2, 2, 1, 1, 0 },
    { 1, 1, 2, 1, 1 },
    { 0, 0, 0, 0, 0 },
    { 1, 1, 1, 0, 0 },
    { 0, 1, 5, 5, 5 },
    { 5, 5, 1, 0, 0 },
    { 5, 5, 2, 2, 2 },
    { 5, 5, 5, 5, 5 },
};
const u8 room_weather_month_ids[12][6] = {
    _("{1411}{080B}{1213}\c"),
    _("{1314}{080D}{1112}\c"),
    _("{0601}{0302}{0508}\c"),
    _("{0106}{0203}か\c"),
    _("{080B}{0C01}{0507}\c"),
    _("{0C07}{0108}{0B0D}\c"),
    _("あ{0C0D}{0506}\c"),
    _("{0104}{0B03}{0507}\c"),
    _("{0304}{0D01}{0205}\c"),
    _("{0307}{0102}{050B}\c"),
    _("{0208}{0103}{050D}\c"),
    _("{1114}{080B}{1112}\c"),
};
const u8 room_weather_month_odds[12][6] = {
    _("{2814}{0A0A}{0A0A}\c"),
    _("{1E1E}{0A0A}{0A0A}\c"),
    _("{1E14}{140A}{0A0A}\c"),
    _("{1E1E}{0A0A}{0A0A}\c"),
    _("{1E14}{140A}{0A0A}\c"),
    _("{1E1E}{0A0A}{0A0A}\c"),
    _("{2814}{0A0A}{0A0A}\c"),
    _("{1E1E}{0A0A}{0A0A}\c"),
    _("{1E14}{140A}{0A0A}\c"),
    _("{1E1E}{0A0A}{0A0A}\c"),
    _("{1E1E}{0A0A}{0A0A}\c"),
    _("{1E1E}{0A0A}{0A0A}\c"),
};
/* The passers-by of the room window: in frame k of a scene the two sprites a and b are
 * drawn at (x06, x08) and (x12, x14) (room_window_passer_steps = the scene's table; room_window_draw,
 * room_window_animate). */
struct PasserStep {
    const u8 *pic_a;
    u8 flags_a;
    u16 x_a;
    u16 y_a;
    const u8 *pic_b;
    u8 flags_b;
    u16 x_b;
    u16 y_b;
};
/* Unreferenced 5-byte steps, one per frame of the scenes that have no passers-by table. */
struct WindowStep5 {
    u8 x0;
    u8 x1;
    u8 x2;
    u8 x3;
    u8 x4;
};
const struct RoomStep room_window_scene_a[39] = {
    { 0, 0, 55, 50 },
    { 8, 0, 28, 50 },
    { 9, 0, 1, 50 },
    { 0, 0, -32, 0 },
    { 8, 0x10, 7, 50 },
    { 9, 0x10, 31, 50 },
    { 8, 0x10, 58, 50 },
    { 0, 0x10, 73, 50 },
    { 6, 0x10, 75, 50 },
    { 6, 0x10, 73, 50 },
    { 10, 0x10, 73, 50 },
    { 10, 0, 73, 50 },
    { 10, 0x10, 73, 50 },
    { 10, 0, 73, 50 },
    { 10, 0x10, 73, 50 },
    { 0, 0, 73, 50 },
    { 8, 0, 43, 36 },
    { 9, 0, 15, 49 },
    { 0, 0, -32, 0 },
    { 8, 0x10, 34, 50 },
    { 9, 0x10, 63, 50 },
    { 2, 0x10, 63, 29 },
    { 2, 0x10, 62, 50 },
    { 2, 0x10, 63, 29 },
    { 0, 0, 63, 50 },
    { 8, 0, 55, 51 },
    { 9, 0, 30, 50 },
    { 4, 0x10, 30, 50 },
    { 5, 0, 30, 50 },
    { 0, 0, 30, 50 },
    { 9, 0, 1, 50 },
    { 0, 0, -32, 0 },
    { 8, 0x10, 7, 50 },
    { 9, 0x10, 31, 50 },
    { 8, 0x10, 58, 50 },
    { 0, 0, 55, 50 },
    { 2, 0, 55, 25 },
    { 0, 0, 55, 50 },
    { 0xff, 0, 64, 64 },
};
const struct PasserStep room_window_passers_a[39] = {
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { D_2191ECC, 0x30, 113, 60, 0, 0x00, 0, 0 },
    { D_2191ECC, 0x60, 93, 61, 0, 0x00, 0, 0 },
    { D_2191ECC, 0x00, 64, 60, 0, 0x00, 0, 0 },
    { D_2191ECC, 0x50, 41, 60, 0, 0x00, 0, 0 },
    { D_2191ECC, 0x30, 24, 60, 0, 0x00, 0, 0 },
    { D_2191ECC, 0x60, 2, 60, 0, 0x00, 0, 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { D_2191E34, 0x00, 78, 10, D_2191ECC, 0x00, 41, 20 },
    { 0 },
    { D_2191E34, 0x00, 78, 10, D_2191ECC, 0x00, 41, 20 },
    { D_2191E34, 0x00, 46, 20, D_2191ECC, 0x00, 46, 36 },
    { D_2191E34, 0x00, 40, 19, D_2191ECC, 0x00, 36, 35 },
    { D_2191E34, 0x00, 18, 18, D_2191ECC, 0x00, 13, 36 },
    { D_2191E34, 0x00, 39, 20, D_2191ECC, 0x00, 47, 33 },
    { D_2191E34, 0x00, 30, 12, D_2191ECC, 0x00, 16, 31 },
    { D_2191E34, 0x00, 13, 20, D_2191ECC, 0x00, 13, 36 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
};
const struct RoomStep D_253F584[38] = {
    { 0, 0, 147, 52 },
    { 0, 0, 147, 52 },
    { 0, 0, 147, 52 },
    { 0, 0, 147, 52 },
    { 0, 0, 90, 45 },
    { 0, 0, 86, 45 },
    { 1, 0, 85, 45 },
    { 2, 0, 83, 50 },
    { 0, 0, 74, 50 },
    { 1, 0, 42, 50 },
    { 0, 0x10, 38, 50 },
    { 2, 0x10, 37, 50 },
    { 0, 0x10, 38, 50 },
    { 0, 0x10, 40, 49 },
    { 0, 0x10, 70, 50 },
    { 2, 0x10, 75, 50 },
    { 0, 0x10, 77, 50 },
    { 1, 0x10, 84, 49 },
    { 0, 0x10, 90, 50 },
    { 1, 0x10, 96, 49 },
    { 0, 0x10, 102, 50 },
    { 2, 0, 102, 50 },
    { 1, 0, 102, 50 },
    { 0, 0, 94, 47 },
    { 1, 0, 68, 47 },
    { 0, 0, 47, 46 },
    { 1, 0, 34, 48 },
    { 0, 0, 30, 48 },
    { 2, 0x10, 29, 48 },
    { 1, 0, 29, 49 },
    { 0, 0, 22, 46 },
    { 1, 0, 12, 48 },
    { 0, 0, 2, 47 },
    { 1, 0, -2, 49 },
    { 1, 0, -19, 52 },
    { 1, 0, -19, 52 },
    { 1, 0, -19, 52 },
    { 0xff, 0, 64, 64 },
};
const struct WindowStep5 D_253F668[38] = {
    { 0x01, 0x03, 0x00, 0x0f, 0x2c },
    { 0x01, 0x03, 0x10, 0x0f, 0x2c },
    { 0x01, 0x03, 0x00, 0x0f, 0x2c },
    { 0x01, 0x03, 0x10, 0x0f, 0x2c },
    { 0x01, 0x03, 0x00, 0x0f, 0x2c },
    { 0x01, 0x00, 0x10, 0x0f, 0x2c },
    { 0x01, 0x06, 0x10, 0x15, 0x2c },
    { 0x00, 0x00, 0x10, 0x1d, 0x2c },
    { 0x00, 0x02, 0x00, 0x17, 0x2c },
    { 0x00, 0x03, 0x10, 0x0f, 0x2c },
    { 0x00, 0x03, 0x10, 0x0e, 0x2c },
    { 0x00, 0x00, 0x10, 0x0d, 0x2c },
    { 0x00, 0x03, 0x10, 0x0e, 0x2c },
    { 0x00, 0x03, 0x10, 0x0d, 0x2c },
    { 0x01, 0x03, 0x10, 0x0f, 0x2c },
    { 0x01, 0x00, 0x10, 0x0e, 0x2c },
    { 0x01, 0x03, 0x10, 0x0f, 0x2c },
    { 0x01, 0x06, 0x10, 0x15, 0x2b },
    { 0x01, 0x02, 0x10, 0x19, 0x2c },
    { 0x01, 0x06, 0x10, 0x1f, 0x2b },
    { 0x01, 0x02, 0x10, 0x25, 0x2c },
    { 0x01, 0x00, 0x10, 0x25, 0x2c },
    { 0x01, 0x03, 0x10, 0x25, 0x2c },
    { 0x01, 0x06, 0x10, 0x2b, 0x2b },
    { 0x01, 0x02, 0x10, 0x31, 0x2c },
    { 0x01, 0x06, 0x10, 0x39, 0x2c },
    { 0x01, 0x03, 0x00, 0x38, 0x2c },
    { 0x01, 0x03, 0x10, 0x39, 0x2c },
    { 0x01, 0x03, 0x00, 0x38, 0x2c },
    { 0x01, 0x03, 0x00, 0x39, 0x2c },
    { 0x01, 0x06, 0x00, 0x33, 0x2b },
    { 0x01, 0x02, 0x00, 0x2d, 0x2c },
    { 0x01, 0x06, 0x00, 0x27, 0x2b },
    { 0x01, 0x02, 0x00, 0x21, 0x2b },
    { 0x01, 0x06, 0x00, 0x1b, 0x2b },
    { 0x01, 0x02, 0x00, 0x15, 0x2c },
    { 0x01, 0x03, 0x00, 0x0f, 0x2d },
    { 0xff, 0x00, 0x40, 0x40, 0x00 },
};
const struct RoomStep room_window_scene_b[35] = {
    { 6, 0, 72, 52 },
    { 6, 0, 71, 52 },
    { 6, 0, 72, 52 },
    { 7, 0, 72, 52 },
    { 0, 0, 72, 52 },
    { 0, 0, 72, 52 },
    { 2, 0, 72, 52 },
    { 0, 0, 65, 50 },
    { 0, 0, 65, 50 },
    { 0, 0, 58, 52 },
    { 0, 0, 58, 52 },
    { 0, 0, 65, 50 },
    { 0, 0, 65, 50 },
    { 0, 0, 65, 50 },
    { 0, 0, 65, 50 },
    { 0, 0, 65, 50 },
    { 6, 0, 72, 52 },
    { 6, 0, 71, 52 },
    { 6, 0, 72, 52 },
    { 6, 0, 72, 52 },
    { 6, 0, 72, 52 },
    { 6, 0, 72, 52 },
    { 6, 0, 72, 52 },
    { 6, 0, 72, 52 },
    { 6, 0, 72, 52 },
    { 6, 0, 72, 52 },
    { 6, 0, 72, 52 },
    { 6, 0, 72, 52 },
    { 5, 0, 72, 52 },
    { 5, 0x10, 72, 52 },
    { 10, 0x10, 72, 52 },
    { 10, 0x10, 73, 52 },
    { 10, 0x10, 72, 52 },
    { 10, 0x10, 73, 52 },
    { 0xff, 0, 64, 64 },
};
const struct PasserStep room_window_passers_b[35] = {
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 51, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_219FFAC, 0x00, 52, 65, D_219F814, 0x00, 74, 54 },
    { D_219FFAC, 0x00, 52, 65, D_219F814, 0x00, 54, 54 },
    { D_219FFAC, 0x00, 45, 33, D_219F814, 0x00, 41, 56 },
    { D_219F604, 0x00, 47, 56, 0, 0x00, 0, 0 },
    { D_219E874, 0x00, 69, 48, D_219F604, 0x00, 47, 56 },
    { D_219E874, 0x00, 41, 51, D_219F604, 0x00, 47, 56 },
    { D_219E874, 0x00, 44, 51, D_219F604, 0x00, 47, 56 },
    { D_219E874, 0x00, 32, 23, D_219F604, 0x00, 35, 38 },
    { D_219E874, 0x00, 26, 8, D_219F604, 0x00, 29, 23 },
    { D_219E874, 0x00, 16, 65529, D_219F604, 0x00, 19, 8 },
    { D_219F604, 0x00, 15, 65529, 0, 0x00, 0, 0 },
    { 0 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 51, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_214ED7C, 0x00, 94, 35, D_2192470, 0x00, 52, 61 },
    { D_214EE08, 0x00, 94, 35, D_2192470, 0x00, 52, 61 },
    { D_214ED7C, 0x00, 94, 35, D_2192470, 0x00, 52, 61 },
    { D_214EE08, 0x00, 94, 35, D_2192470, 0x00, 52, 61 },
    { D_2196400, 0x10, 48, 45, D_2192470, 0x00, 52, 61 },
    { D_2196370, 0x10, 48, 45, D_2192470, 0x00, 52, 61 },
    { D_2196400, 0x10, 48, 45, D_2192470, 0x00, 52, 61 },
    { D_2196370, 0x10, 48, 45, D_2192470, 0x00, 52, 61 },
    { D_21AD050, 0x10, 48, 45, D_2192470, 0x00, 52, 61 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { D_2192470, 0x00, 52, 61, 0, 0x00, 0, 0 },
    { 0 },
};
const struct RoomStep D_253FB44[33] = {
    { 0, 0, 58, 50 },
    { 0, 0, 54, 51 },
    { 0, 0, 50, 50 },
    { 0, 0, 46, 51 },
    { 0, 0, 42, 50 },
    { 2, 0x10, 42, 50 },
    { 0, 0x10, 42, 50 },
    { 0, 0x10, 46, 51 },
    { 0, 0x10, 50, 50 },
    { 0, 0x10, 54, 51 },
    { 0, 0x10, 58, 50 },
    { 2, 0x10, 58, 50 },
    { 0, 0x10, 58, 50 },
    { 0, 0x10, 62, 51 },
    { 0, 0x10, 66, 50 },
    { 0, 0x10, 70, 51 },
    { 0, 0x10, 74, 50 },
    { 2, 0, 74, 50 },
    { 0, 0, 74, 50 },
    { 0, 0, 70, 51 },
    { 0, 0, 66, 50 },
    { 0, 0, 62, 51 },
    { 0, 0, 58, 50 },
    { 2, 0, 58, 50 },
    { 0, 0, 58, 50 },
    { 0, 0, 58, 50 },
    { 2, 0, 58, 50 },
    { 0, 0, 58, 50 },
    { 2, 0, 58, 40 },
    { 0, 0, 58, 50 },
    { 2, 0, 58, 50 },
    { 0, 0, 58, 50 },
    { 0xff, 0, 64, 64 },
};
const struct WindowStep5 D_253FC0C[33] = {
    { 0x00, 0x03, 0x00, 0x53, 0x2c },
    { 0x00, 0x03, 0x00, 0x4f, 0x2d },
    { 0x00, 0x03, 0x00, 0x4b, 0x2c },
    { 0x00, 0x03, 0x00, 0x47, 0x2d },
    { 0x00, 0x03, 0x00, 0x43, 0x2c },
    { 0x00, 0x00, 0x10, 0x43, 0x2c },
    { 0x00, 0x03, 0x10, 0x43, 0x2c },
    { 0x00, 0x03, 0x10, 0x47, 0x2d },
    { 0x00, 0x03, 0x10, 0x4b, 0x2c },
    { 0x00, 0x03, 0x10, 0x4f, 0x2d },
    { 0x00, 0x03, 0x10, 0x53, 0x2c },
    { 0x00, 0x00, 0x10, 0x53, 0x2c },
    { 0x00, 0x03, 0x10, 0x53, 0x2c },
    { 0x00, 0x03, 0x10, 0x57, 0x2d },
    { 0x00, 0x03, 0x10, 0x5b, 0x2c },
    { 0x00, 0x03, 0x10, 0x5f, 0x2d },
    { 0x00, 0x03, 0x10, 0x63, 0x2c },
    { 0x00, 0x00, 0x00, 0x63, 0x2c },
    { 0x00, 0x03, 0x00, 0x63, 0x2c },
    { 0x00, 0x03, 0x00, 0x5f, 0x2d },
    { 0x00, 0x03, 0x00, 0x5b, 0x2c },
    { 0x00, 0x03, 0x00, 0x57, 0x2d },
    { 0x00, 0x03, 0x00, 0x53, 0x2c },
    { 0x00, 0x00, 0x00, 0x53, 0x2c },
    { 0x00, 0x03, 0x00, 0x53, 0x2c },
    { 0x00, 0x03, 0x00, 0x53, 0x2c },
    { 0x00, 0x00, 0x00, 0x53, 0x2c },
    { 0x00, 0x03, 0x00, 0x53, 0x2c },
    { 0x00, 0x00, 0x00, 0x53, 0x2c },
    { 0x00, 0x03, 0x00, 0x53, 0x2c },
    { 0x00, 0x00, 0x00, 0x53, 0x22 },
    { 0x00, 0x03, 0x00, 0x53, 0x2c },
    { 0xff, 0x00, 0x40, 0x40, 0x00 },
};
const struct RoomStep room_window_scene_c[38] = {
    { 0, 0, 63, 49 },
    { 0, 0, 63, 50 },
    { 0, 0, 63, 49 },
    { 0, 0x10, 63, 49 },
    { 2, 0x10, 63, 49 },
    { 0, 0, 63, 49 },
    { 8, 0, 44, 49 },
    { 9, 0, 33, 50 },
    { 8, 0, 19, 50 },
    { 9, 0, 8, 50 },
    { 0, 0x10, 15, 50 },
    { 8, 0x10, 31, 50 },
    { 9, 0x10, 45, 50 },
    { 8, 0x10, 55, 50 },
    { 0, 0x10, 63, 50 },
    { 0, 0, 63, 50 },
    { 0, 0, 63, 49 },
    { 2, 0, 63, 50 },
    { 2, 0, 63, 49 },
    { 2, 0, 63, 49 },
    { 2, 0x10, 63, 49 },
    { 2, 0x10, 63, 49 },
    { 2, 0, 63, 49 },
    { 2, 0, 63, 49 },
    { 2, 0x10, 63, 50 },
    { 0, 0x10, 63, 49 },
    { 0, 0x10, 63, 50 },
    { 0, 0x10, 63, 49 },
    { 0, 0x10, 63, 49 },
    { 0, 0, 63, 50 },
    { 0, 0, 63, 49 },
    { 0, 0x10, 63, 49 },
    { 0, 0x10, 63, 49 },
    { 0, 0x10, 63, 49 },
    { 0, 0x10, 63, 49 },
    { 0, 0x10, 63, 49 },
    { 0, 0, 63, 49 },
    { 0xff, 0, 64, 64 },
};
const struct PasserStep room_window_passers_c[38] = {
    { 0 },
    { 0 },
    { D_218421C, 0x00, 128, 11, 0, 0x00, 0, 0 },
    { D_218421C, 0x00, 104, 22, 0, 0x00, 0, 0 },
    { D_218421C, 0x00, 78, 13, 0, 0x00, 0, 0 },
    { D_218421C, 0x00, 41, 19, 0, 0x00, 0, 0 },
    { D_218421C, 0x00, 29, 29, 0, 0x00, 0, 0 },
    { D_218421C, 0x00, 13, 25, 0, 0x00, 0, 0 },
    { D_218421C, 0x00, 2, 18, 0, 0x00, 0, 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { D_21840E8, 0x00, 2, 12, 0, 0x00, 0, 0 },
    { D_2183FB4, 0x00, 26, 20, 0, 0x00, 0, 0 },
    { D_21840E8, 0x00, 44, 32, 0, 0x00, 0, 0 },
    { D_2183FB4, 0x00, 60, 25, 0, 0x00, 0, 0 },
    { D_21840E8, 0x00, 81, 28, 0, 0x00, 0, 0 },
    { D_2183FB4, 0x10, 73, 33, 0, 0x00, 0, 0 },
    { D_21840E8, 0x10, 45, 31, 0, 0x00, 0, 0 },
    { D_2183FB4, 0x00, 50, 29, 0, 0x00, 0, 0 },
    { D_2183FB4, 0x00, 76, 18, 0, 0x00, 0, 0 },
    { D_21840E8, 0x00, 96, 14, 0, 0x00, 0, 0 },
    { D_2183FB4, 0x00, 115, 15, 0, 0x00, 0, 0 },
    { D_21840E8, 0x00, 123, 10, 0, 0x00, 0, 0 },
    { 0 },
    { 0 },
    { D_219BB1C, 0x00, 128, 11, 0, 0x00, 0, 0 },
    { D_219BC50, 0x00, 104, 22, 0, 0x00, 0, 0 },
    { D_219BB1C, 0x00, 78, 13, 0, 0x00, 0, 0 },
    { D_219BC50, 0x00, 41, 19, 0, 0x00, 0, 0 },
    { D_219BB1C, 0x00, 13, 11, 0, 0x00, 0, 0 },
    { D_219BC50, 0x00, 2, 18, 0, 0x00, 0, 0 },
    { 0 },
    { 0 },
};
const struct RoomStep D_2540128[36] = {
    { 0, 0x10, 6, 50 },
    { 0, 0x10, 5, 50 },
    { 0, 0x10, 6, 50 },
    { 0, 0x10, 5, 50 },
    { 1, 0x10, 6, 50 },
    { 8, 0x10, 12, 50 },
    { 9, 0x10, 18, 50 },
    { 8, 0x10, 24, 50 },
    { 9, 0x10, 30, 50 },
    { 8, 0x10, 36, 50 },
    { 9, 0x10, 58, 30 },
    { 2, 0x10, 86, 20 },
    { 8, 0, 101, 5 },
    { 9, 0, 78, -14 },
    { 9, 0, 78, -21 },
    { 9, 0, 78, -21 },
    { 9, 0, 78, -21 },
    { 9, 0, 78, -21 },
    { 9, 0, 78, -21 },
    { 0, 0x10, 30, -14 },
    { 0, 0x10, 30, -2 },
    { 0, 0x10, 30, 10 },
    { 0, 0x10, 30, 23 },
    { 2, 0x10, 50, 18 },
    { 0, 0, 64, 50 },
    { 0, 0, 58, 50 },
    { 2, 0, 58, 49 },
    { 2, 0, 58, 50 },
    { 2, 0, 58, 49 },
    { 2, 0, 58, 50 },
    { 2, 0, 58, 49 },
    { 2, 0, 58, 50 },
    { 0, 0, 53, 50 },
    { 2, 0, 47, 47 },
    { 0, 0, 44, 49 },
    { 0xff, 0, 64, 64 },
};
const struct WindowStep5 D_2540200[36] = {
    { 0x00, 0x03, 0x00, 0x54, 0x2c },
    { 0x00, 0x06, 0x10, 0x5a, 0x2c },
    { 0x00, 0x02, 0x10, 0x60, 0x2c },
    { 0x00, 0x06, 0x10, 0x66, 0x2c },
    { 0x00, 0x02, 0x10, 0x6c, 0x2c },
    { 0x00, 0x03, 0x00, 0x71, 0x2c },
    { 0x00, 0x03, 0x00, 0x74, 0x2c },
    { 0x00, 0x03, 0x00, 0x71, 0x2c },
    { 0x00, 0x03, 0x00, 0x74, 0x2c },
    { 0x01, 0x03, 0x00, 0x71, 0x2c },
    { 0x00, 0x03, 0x00, 0x71, 0x2e },
    { 0x00, 0x03, 0x00, 0x71, 0x25 },
    { 0x00, 0x03, 0x00, 0x71, 0x2c },
    { 0x00, 0x03, 0x00, 0x71, 0x25 },
    { 0x00, 0x03, 0x00, 0x71, 0x2c },
    { 0x00, 0x03, 0x00, 0x71, 0x25 },
    { 0x00, 0x03, 0x00, 0x71, 0x2c },
    { 0x00, 0x03, 0x00, 0x71, 0x25 },
    { 0x00, 0x03, 0x00, 0x71, 0x2c },
    { 0x00, 0x03, 0x00, 0x71, 0x25 },
    { 0x00, 0x03, 0x00, 0x71, 0x2c },
    { 0x01, 0x02, 0x00, 0x6c, 0x2c },
    { 0x00, 0x06, 0x00, 0x66, 0x2c },
    { 0x01, 0x02, 0x00, 0x60, 0x2c },
    { 0x00, 0x06, 0x00, 0x5a, 0x2c },
    { 0x01, 0x03, 0x00, 0x54, 0x2c },
    { 0x01, 0x00, 0x00, 0x54, 0x2b },
    { 0x00, 0x00, 0x00, 0x54, 0x2c },
    { 0x00, 0x00, 0x00, 0x54, 0x2b },
    { 0x00, 0x00, 0x00, 0x54, 0x2c },
    { 0x00, 0x00, 0x00, 0x54, 0x2b },
    { 0x00, 0x00, 0x00, 0x54, 0x2c },
    { 0x00, 0x03, 0x00, 0x4f, 0x2c },
    { 0x00, 0x00, 0x00, 0x4c, 0x29 },
    { 0x00, 0x00, 0x00, 0x4c, 0x2c },
    { 0xff, 0x00, 0x40, 0x40, 0x00 },
};
const struct RoomStep room_window_scene_bad_weather[27] = {
    { 0, 0, 55, 50 },
    { 3, 0, 51, 50 },
    { 3, 0, 48, 50 },
    { 0, 0x10, 47, 50 },
    { 0, 0, 47, 50 },
    { 0, 0x10, 51, 50 },
    { 8, 0x10, 53, 50 },
    { 9, 0x10, 53, 50 },
    { 0, 0x10, 52, 49 },
    { 0, 0x10, 55, 50 },
    { 2, 0x10, 55, 36 },
    { 2, 0, 55, 36 },
    { 0, 0, 55, 50 },
    { 0, 0, 43, 50 },
    { 0, 0, 31, 50 },
    { 0, 0x10, 31, 50 },
    { 5, 0x10, 31, 50 },
    { 5, 0x10, 41, 50 },
    { 8, 0x10, 51, 50 },
    { 5, 0x10, 63, 50 },
    { 3, 0x10, 63, 50 },
    { 0, 0x10, 63, 50 },
    { 4, 0x10, 63, 50 },
    { 0, 0x10, 63, 50 },
    { 8, 0, 57, 50 },
    { 0, 0, 55, 50 },
    { 0xff, 0, 64, 64 },
};
const struct PasserStep room_window_passers_bad_weather[27] = {
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { D_2191F64, 0x10, 74, 49, 0, 0x00, 0, 0 },
    { D_2191F64, 0x10, 74, 49, 0, 0x00, 0, 0 },
    { D_2191F64, 0x10, 78, 49, 0, 0x00, 0, 0 },
    { D_2192158, 0x10, 74, 37, 0, 0x00, 0, 0 },
    { D_2192158, 0x00, 41, 30, 0, 0x00, 0, 0 },
    { D_2192158, 0x00, 47, 2, 0, 0x00, 0, 0 },
    { D_2192158, 0x10, 63, 1, 0, 0x00, 0, 0 },
    { D_2192158, 0x10, 69, 29, 0, 0x00, 0, 0 },
    { D_2192158, 0x10, 56, 30, 0, 0x00, 0, 0 },
    { D_2192158, 0x10, 44, 30, 0, 0x00, 0, 0 },
    { D_2192158, 0x00, 18, 30, 0, 0x00, 0, 0 },
    { D_214DBFC, 0x00, 28, 24, D_2192158, 0x20, 63, 49 },
    { D_214E4E0, 0x10, 20, 39, D_2192158, 0x00, 104, 44 },
    { D_214E4E0, 0x10, 28, 37, D_2192158, 0x20, 125, 48 },
    { 0 },
    { 0 },
    { 0 },
    { D_214DDA8, 0x00, 53, 28, 0, 0x00, 0, 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
};
const struct RoomStep D_25405E0[18] = {
    { 0, 0, 96, 46 },
    { 0, 0, 96, 45 },
    { 0, 0, 96, 46 },
    { 0, 0, 96, 45 },
    { 0, 0, 96, 46 },
    { 0, 0, 96, 45 },
    { 0, 0, 96, 46 },
    { 0, 0, 96, 45 },
    { 2, 0, 73, 35 },
    { 5, 0, 70, 41 },
    { 5, 0, 69, 50 },
    { 3, 0x10, 71, 50 },
    { 3, 0x10, 76, 50 },
    { 3, 0x10, 96, 46 },
    { 0, 0x10, 96, 46 },
    { 0, 0, 96, 46 },
    { 0, 0, 96, 45 },
    { 0xff, 0, 64, 64 },
};
const struct WindowStep5 D_254064C[18] = {
    { 0x01, 0x03, 0x00, 0x78, 0x2c },
    { 0x01, 0x00, 0x00, 0x78, 0x2c },
    { 0x01, 0x03, 0x00, 0x78, 0x2c },
    { 0x01, 0x00, 0x00, 0x78, 0x2c },
    { 0x01, 0x03, 0x00, 0x78, 0x2c },
    { 0x01, 0x00, 0x00, 0x78, 0x2c },
    { 0x01, 0x03, 0x00, 0x78, 0x2c },
    { 0x01, 0x00, 0x00, 0x78, 0x2c },
    { 0x01, 0x00, 0x00, 0x78, 0x28 },
    { 0x01, 0x03, 0x00, 0x78, 0x2c },
    { 0x01, 0x03, 0x00, 0x78, 0x2d },
    { 0x01, 0x03, 0x00, 0x78, 0x2c },
    { 0x01, 0x03, 0x00, 0x78, 0x2d },
    { 0x01, 0x03, 0x00, 0x78, 0x2c },
    { 0x01, 0x03, 0x00, 0x78, 0x2d },
    { 0x01, 0x03, 0x00, 0x78, 0x2c },
    { 0x01, 0x03, 0x00, 0x78, 0x2d },
    { 0xff, 0x00, 0x40, 0x40, 0x00 },
};

/*
 * Daily weather draw (pet.weather_today) from the month's probability table room_weather_month_odds /
 * room_weather_month_ids, with fixed weather on some dates (snow, rain seasons...), clear on birthdays.
 */
void room_weather_draw_daily(void)
{
    u8 a, b;
    int r, j, s;
    const u8 *t;

    a = rtc_get_month();
    b = rtc_get_day();
    r = rand() % 100;
    t = room_weather_month_odds[a - 1];
    s = j = 0;
    for (; j <= 5; j++) {
        if (r < t[j] + s && r >= s)
            break;
        s += t[j];
    }
    if (j == 6)
        j = 0;
    save.pet.weather_today = room_weather_month_ids[a - 1][j] - 1;
    switch (a) {
    case 2:
        if (b >= 6 && b <= 10)
            save.pet.weather_today = 0x13;
        break;
    case 4:
        if (b >= 1 && b <= 7)
            save.pet.weather_today = 0;
        break;
    case 5:
        if (b >= 1 && b <= 5)
            save.pet.weather_today = 0;
        break;
    case 6:
        if (b >= 12 && b <= 16)
            save.pet.weather_today = 11;
        break;
    case 7:
        if (b >= 18 && b <= 22)
            save.pet.weather_today = 0;
        break;
    case 8:
        if (b >= 3 && b <= 9)
            save.pet.weather_today = 0;
        break;
    case 9:
        if (b >= 15 && b <= 19)
            save.pet.weather_today = 2;
        break;
    case 11:
        if (b >= 20 && b <= 24)
            save.pet.weather_today = 2;
        break;
    }
    if ((save.pet.owner_bday_month == a && save.pet.owner_bday_day == b) || chara_birthday_on(a, b) >= 0)
        save.pet.weather_today = 0;
    room_weather_update_now();
}

/*
 * Communication level, the house upgrades: pet.comm_count (every completed communication:
 * Tama-to-Tama and app sessions, idle reader touches comm_idle_touch_event, the Touch Spot reader,
 * touch Bingo and Gashapon; capped at 1001) -> level 0..4 from 5 / 20 / 100 / 1000
 * communications. The level pet.house_level picks the pet's house in the outdoor scene
 * (room_window_draw: 0x218A91C cabin, 0x218BB44 small house, 0x218CD6C two-storey house,
 * 0x218DF94 mansion, 0x218F1BC palace). On a change (checked when the main menu key is
 * handled) the mascot 0x49 announces it (texts 100/0xB3..0xB6), +1 heart (happiness),
 * save. Returns 1 if the level changed.
 */
int comm_house_level(void)
{
    int lv;
    u8 *p;

    if (save.pet.comm_count > 999)
        lv = 4;
    else if (save.pet.comm_count > 99)
        lv = 3;
    else if (save.pet.comm_count > 19)
        lv = 2;
    else if (save.pet.comm_count > 4)
        lv = 1;
    else
        lv = 0;
    p = &save.pet.house_level;
    if (*p == lv)
        goto zero;
    *p = lv;
    chara_select_other_by_set(0x49);
    room_visitor_scene = 1;
    anim_visitor_gift();
    if (lv == 1)
        ui_show_message(6, 0, chara_text(0, 100, 0xb3));
    else if (lv == 2)
        ui_show_message(6, 0, chara_text(0, 100, 0xb4));
    else if (lv == 3)
        ui_show_message(6, 0, chara_text(0, 100, 0xb5));
    else if (lv == 4)
        ui_show_message(6, 0, chara_text(0, 100, 0xb6));
    anim_visitor_gift_end();
    room_visitor_scene = 0;
    pet_add_happiness(1);
    save_after_sound();
    return 1;
zero:
    return 0;
}

/*
 * Draw the room WINDOW / outdoor scene: sky by weather pet.weather_now and time of day (0 clear:
 * dawn / day / dusk / night pictures 0x20951A0, 0x20963BC .. 0x209E298, with the
 * twinkling variant 9; 1 cloudy; 2 rain (animated 10 / 11); 5 snow (16 / 17); animated
 * variant in room_window_sky_anim), a random pair of passers-by from the tables room_window_scene_a /
 * room_window_scene_b / room_window_scene_c (room_window_passer_steps = the chosen pair, two sprites room_window_passer_objs animated by
 * room_window_animate), the dated decorations (7/7 Tanabata bamboo 0x2172F48, 7/20 0x2174170,
 * 8/14 0x21789E8), the seasonal flower pet.window_flower (1 spring / 2 summer / 4 autumn / 8
 * winter pictures) and the pet's house, chosen by the communication level pet.house_level
 * (comm_house_level: 0x218A91C cabin .. 0x218F1BC palace, full-scene pictures). a = 1
 * the full room (on the birthday or a special date (chara_birthday_on) the weather is cleared;
 * the passers-by are drawn), 2 = background only. Returns the passers-by table.
 */
int room_window_draw(u8 a)
{
    struct GfxObject *e;
    u8 b, c, d, t, k;
    const u8 *p;
    int r;

    b = rtc_day_period();
    r = 0;
    c = rtc_get_month();
    d = rtc_get_day();
    room_window_sky_obj = gfx_object_create(r, 0x1c, r, r, r, (const u8 *)r);
    room_window_sky_anim = r;
    room_window_tick = r;
    if (a == 1) {
        anim_window_mode = r;
        if (save.pet.owner_bday_month != c || save.pet.owner_bday_day != d) {
            if (chara_birthday_on(c, d) < 0)
                goto skip;
        }
        save.pet.weather_today = 0;
        room_window_sky_anim = 0;
    }
skip:
    room_weather_update_now();
    room_window_passer_objs.obj_a = 0;
    room_window_passer_objs.obj_b = 0;
    switch (util_rand_except(3, 3)) {
    case 0:
        r = (int)room_window_scene_a;
        room_window_passer_steps = (struct PasserStep *)room_window_passers_a;
        break;
    case 1:
        r = (int)room_window_scene_b;
        room_window_passer_steps = (struct PasserStep *)room_window_passers_b;
        break;
    default:
        r = (int)room_window_scene_c;
        room_window_passer_steps = (struct PasserStep *)room_window_passers_c;
        break;
    }
    switch (save.pet.weather_now) {
    case 0:
        switch (b) {
        case 0:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20951A0);
            break;
        case 1:
        case 2:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20963BC);
            break;
        case 3:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_209D070);
            break;
        default:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_209E298);
            room_window_sky_anim = 9;
            break;
        }
        break;
    case 1:
        if (b == 4)
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20A06E0);
        else
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20975DC);
        break;
    case 2:
        if (b == 4) {
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20A1904);
            room_window_sky_anim = 11;
            r = (int)room_window_scene_bad_weather;
            room_window_passer_steps = (struct PasserStep *)room_window_passers_bad_weather;
        } else {
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_2098800);
            room_window_sky_anim = 10;
            r = (int)room_window_scene_bad_weather;
            room_window_passer_steps = (struct PasserStep *)room_window_passers_bad_weather;
        }
        break;
    case 5:
        if (b == 4) {
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20A3D54);
            room_window_sky_anim = 17;
            r = (int)room_window_scene_bad_weather;
            room_window_passer_steps = (struct PasserStep *)room_window_passers_bad_weather;
        } else {
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_209AC50);
            room_window_sky_anim = 16;
            r = (int)room_window_scene_bad_weather;
            room_window_passer_steps = (struct PasserStep *)room_window_passers_bad_weather;
        }
        break;
    }
    if (a == 1 || room_visitor_scene == 1) {
        if (c == 7 && d == 7) {
            e = &gfx_objects[gfx_object_count - 1];
            t = e->palette_base + e->image[2];
            gfx_object_create(0, 0x1c, t, 0, 0, D_2172F48);
        }
        if (c == 7 && d == 0x14) {
            e = &gfx_objects[gfx_object_count - 1];
            t = e->palette_base + e->image[2];
            gfx_object_create(0, 0x1c, t, 0, 0, D_2174170);
        }
        if (c == 8 && d == 0xe) {
            e = &gfx_objects[gfx_object_count - 1];
            t = e->palette_base + e->image[2];
            gfx_object_create(0, 0x1c, t, 0, 0, D_21789E8);
        }
    }
    D_00841FF = 0;
    D_00841FE = 0;
    if (a == 2)
        return r;
    k = save.pet.window_flower;
    if (k & 0xf) {
        e = &gfx_objects[gfx_object_count - 1];
        t = e->palette_base + e->image[2];
        if (k == 1) {
            p = D_219C248;
            gfx_object_create(0x3b - (p[0] >> 1), 0x3f - (p[1] >> 1), t, 0, 0, (const u8 *)p);
        } else if (save.pet.window_flower == 2) {
            p = D_219C968;
            gfx_object_create(0x3b - (p[0] >> 1), 0x3f - (p[1] >> 1), t, 0, 0, (const u8 *)p);
        } else if (save.pet.window_flower == 4) {
            p = D_219D100;
            gfx_object_create(0x3b - (p[0] >> 1), 0x3f - (p[1] >> 1), t, 0, 0, (const u8 *)p);
        } else if (save.pet.window_flower == 8) {
            p = D_219D820;
            gfx_object_create(0x3b - (p[0] >> 1), 0x3f - (p[1] >> 1), t, 0, 0, (const u8 *)p);
        }
    }
skip2:
    e = &gfx_objects[gfx_object_count - 1];
    t = e->palette_base + e->image[2];
    switch (save.pet.house_level) {
    case 4:
        gfx_object_create(0, 0x1c, t, 0, 0, D_218F1BC);
        break;
    case 3:
        gfx_object_create(0, 0x1c, t, 0, 0, D_218DF94);
        break;
    case 2:
        gfx_object_create(0, 0x1c, t, 0, 0, D_218CD6C);
        break;
    case 1:
        gfx_object_create(0, 0x1c, t, 0, 0, D_218BB44);
        break;
    default:
        gfx_object_create(0, 0x1c, t, 0, 0, D_218A91C);
        break;
    }
    if (a == 1) {
        e = &gfx_objects[gfx_object_count - 1];
        room_window_passer_palettes.pal_a = e->palette_base + 0x10;
        room_window_passer_objs.obj_a = gfx_object_create(room_window_passer_steps->x_a - (room_window_passer_steps->pic_a[0] >> 1),
                                      room_window_passer_steps->y_a - (room_window_passer_steps->pic_a[1] >> 1) + 0x1c,
                                      room_window_passer_palettes.pal_a, 0, room_window_passer_steps->flags_a, (int)room_window_passer_steps->pic_a);
        room_window_passer_palettes.pal_b = room_window_passer_palettes.pal_a + 8;
        room_window_passer_objs.obj_b = gfx_object_create(room_window_passer_steps->x_b - (room_window_passer_steps->pic_b[0] >> 1),
                                      room_window_passer_steps->y_b - (room_window_passer_steps->pic_b[1] >> 1) + 0x1c,
                                      room_window_passer_palettes.pal_b, 0, room_window_passer_steps->flags_b, (int)room_window_passer_steps->pic_b);
    }
    return r;
}

/*
 * Animate the window every tick (frame counter room_window_tick, half speed while ui_message_shown):
 * alternate the sky frames for the animated weathers and move the two passers-by to
 * frame a of their table.
 */
void room_window_animate(u8 a)
{
    volatile u8 k = a;
    int t;

    room_window_tick++;
    if (ui_message_shown)
        t = (int)room_window_tick / 2;
    else
        t = room_window_tick;
    switch (room_window_sky_anim) {
    case 9:
        switch (t % 4) {
        case 0: case 1:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_209E298);
            break;
        default:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_209F4BC);
            break;
        }
        break;
    case 10:
        if ((t & 1) == 0)
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_2098800);
        else
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_2099A28);
        break;
    case 11:
        if ((t & 1) == 0)
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20A1904);
        else
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20A2B2C);
        break;
    case 16:
        switch (t % 4) {
        case 0: case 1:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_209AC50);
            break;
        default:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_209BE60);
            break;
        }
        break;
    case 17:
        switch (t % 4) {
        case 0: case 1:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20A3D54);
            break;
        default:
            gfx_object_set_image(room_window_sky_obj, 0, 0, D_20A4F64);
            break;
        }
        break;
    }
    if (room_window_passer_objs.obj_a != 0 && room_window_passer_objs.obj_b != 0) {
        gfx_object_set_flags(room_window_passer_objs.obj_a, room_window_passer_steps[k].flags_a);
        gfx_object_set_image(room_window_passer_objs.obj_a, room_window_passer_palettes.pal_a, 0, room_window_passer_steps[k].pic_a);
        gfx_object_move_dirty(room_window_passer_objs.obj_a, room_window_passer_steps[k].x_a - (room_window_passer_steps[k].pic_a[0] >> 1),
                     room_window_passer_steps[k].y_a - (room_window_passer_steps[k].pic_a[1] >> 1) + 0x1c);
        gfx_object_set_flags(room_window_passer_objs.obj_b, room_window_passer_steps[k].flags_b);
        gfx_object_set_image(room_window_passer_objs.obj_b, room_window_passer_palettes.pal_b, 0, room_window_passer_steps[k].pic_b);
        gfx_object_move_dirty(room_window_passer_objs.obj_b, room_window_passer_steps[k].x_b - (room_window_passer_steps[k].pic_b[0] >> 1),
                     room_window_passer_steps[k].y_b - (room_window_passer_steps[k].pic_b[1] >> 1) + 0x1c);
    }
}

/*
 * town_visit_flag ("outing in progress" flag; set by town_visit_begin, cleared by town_visit_end)
 * == 1?
 */
int town_visit_active(void)
{
    if (town_visit_flag != 1)
        return 0;
    return 1;
}

/*
 * town_visit_flag = 1.
 */
void town_visit_begin(void)
{
    town_visit_flag = 1;
}

/*
 * town_visit_flag = 0.
 */
void town_visit_end(void)
{
    town_visit_flag = 0;
}

/*
 * Reads the hour and the day (results unused).
 */
void func_24101F6(void)
{
    rtc_get_hour();
    rtc_get_day();
}

/*
 * Outing arrival screen a: 1 = つぼみ☼こうえん (the park gate), 2 = the Tama-mori shop with
 * its featured item (shop_tamamori_pick), 3 = Gotchi Reform, the redecorating shop
 * (shop_reform_pick), other = the signpost naming a random destination unlocked by the clubs
 * (つぼみ☼こうえん / たまデパ / たまもりショップ / ♪カフェ♪). B = go in (1), C / timeout 0.
 */
u8 outing_signpost(int a)
{
    u16 list[50];
    u8 t;
    const u8 *p;
    int n;

    ui_new_screen();
    switch (a) {
    case 1: {
        int z = 0;
        gfx_object_create(z, 0x1c, z, z, z, D_20951A0);
        p = D_21D5850;
        gfx_object_create(0x15 - (p[0] >> 1), 0x4d - (p[1] >> 1), 0x20, z, 0x10, (const u8 *)p);
    }
        func_00009A6();
        break;
    case 2:
        shop_tamamori_pick();
        break;
    case 3:
        shop_reform_pick();
        break;
    default: {
        int z = 0;
        gfx_object_create(z, 0x1c, z, z, z, D_20951A0);
        p = D_21D5A8C;
        gfx_object_create(0x14 - (p[0] >> 1), 0x4e - (p[1] >> 1), 0x20, z, 0x10, (const u8 *)p);
        p = D_21D5F0C;
        gfx_object_create(0x69 - (p[0] >> 1), 0x4e - (p[1] >> 1), 0x30, z, z, (const u8 *)p);
    }
        n = 0;
        if (pet_activity_done(n) != n)
            list[n++] = 0;
        if (pet_activity_done(1))
            list[n++] = 1;
        if (pet_activity_done(2))
            list[n++] = 2;
        if (pet_activity_done(3))
            list[n++] = 3;
        util_shuffle_u16(list, n);
        t = gfx_object_create(0, 0, 0x40, 0, 0, 0);
        switch (list[0]) {
        case 0:
            gfx_draw_text_centered(t, 0x2e, 0x40, D_2540DB0, 0, 9);
            break;
        case 1:
            gfx_draw_text_centered(t, 0x2e, 0x40, D_2540DC4, 0, 9);
            break;
        case 2:
            gfx_draw_text_centered(t, 0x2e, 0x40, D_2540DD0, 0, 9);
            break;
        default:
            gfx_draw_text_centered(t, 0x2e, 0x40, D_2540DE4, 0, 9);
            break;
        }
        break;
    }
    func_00009A6();
    ui_timeout = 10;
    btn_clear();
    do {
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            btn_b_presses = 0;
            ui_timeout = 10;
            return 1;
        }
        if (btn_c_presses != 0) {
            btn_c_presses = 0;
            return 0;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
    return 0;
}

/*
 * Announcement screen 4 (outing_signpost).
 */
void outing_signpost_random(void)
{
    outing_signpost(4);
}

/*
 * Outing 1, the park gate: announcement (outing_signpost(1)), then "ゲームセンターに いく /
 * いかない" (go to the game centre?) and on "いく" town_visit_place.
 */
void outing_park_gate(void)
{
    int v;
    u8 t = outing_signpost(1);

    if (t != 1)
        return;
    tmr_t5_standard_tick();
    ui_timeout = 10;
    v = t;
    if (ui_yes_no(D_2540DF0, D_2540E04, D_2540E0C, 16, &v) == 1 && v == 0)
        town_visit_place(v);
}

/*
 * Shuffle n bytes.
 */
void util_shuffle_u8(u8 *buf, int n)
{
    u8 j, i, k, cnt;
    u8 t;

    if (n <= 1)
        return;
    cnt = n;
    j = cnt;
    for (k = 0; k < cnt; k++) {
        j = util_rand_except(n, j);
        i = util_rand_except(n, j);
        t = buf[j];
        buf[j] = buf[i];
        buf[i] = t;
    }
}

/*
 * A random outing allowed for the life stage (list shuffled by util_shuffle_u8): 1 the park
 * gate outing_park_gate and 3 Gotchi Reform shop_reform, and for a special form (stage 4)
 * also 2 the Tama-mori shop shop_tamamori. No caller in this firmware.
 */
void outing_random(void)
{
    u8 buf[4];
    int n;

    n = 0;
    memset(buf, n, 4);
    buf[0] = 1;
    n = 1;
    if (save.pet.life_stage <= 3) {
        buf[n] = 3;
        n++;
    } else if (save.pet.life_stage > 3) {
        buf[n] = 2;
        n++;
        buf[n] = 3;
        n++;
    }
    util_shuffle_u8(buf, n);
    switch (buf[0]) {
    case 1:
        outing_park_gate();
        break;
    case 2:
        shop_tamamori();
        break;
    case 3:
        shop_reform();
        break;
    }
}

const u8 D_25406FC[] = _("ふりふりふりっ☼\n(なにかありそう☼)");
const u8 D_2540724[] = _("ふりふりふりっ☼(ゆきだ\nるまつくろうよ☼)");
const u8 D_2540754[] = _("ここになにか\nあるみたいキャン☼");
const u8 D_2540778[] = _("ゆきだるまつくるといいこ\nとあるかもキャン☼");
const u8 D_25407A8[] = _("スリンスリン～@(ここに\nなにかありそう☼)");
const u8 D_25407D8[] = _("スリンスリン～@(ゆきだ\nるまつくろうよ～☼)");
const u8 D_2540808[] = _("あんあーん☼(ここに\nなにかあるかも☼)");
const u8 D_2540834[] = _("あんあーん☼(ゆきだるま\nつくりたい～☼)");
const u8 D_2540860[] = _("チャピチャピ～☼\n(なにかあるかも～☼)");
const u8 D_254088C[] = _("チャピチャピ～☼(ゆきだ\nるまつくってみて～☼)");
const u8 D_25408C0[] = _("れいぞうこにいれておいて\nね★おいしくめしあがれ♪");
const u8 D_25408F4[] = _("オマケするからふたりで\nなかよくたべてね♥");
const u8 D_2540920[] = _("これを♪カフェ♪にもって\nきて!オマケするわよ★");
const u8 D_2540954[] = _("レストランでやすくなる\nクーポンをあげるわ♪");
const u8 D_2540984[] = _("をさがしているんじゃが?");
const u8 D_25409A0[] = _("あたまがよくなるおどうぐ\nをさがしているんじゃが?");
const u8 D_25409D4[] = _("おしゃれなおどうぐを\nさがしているんじゃが?");
const u8 D_2540A04[] = _("ステキなおどうぐ\nをさがしているんじゃが?");
const u8 D_2540A30[] = _("ちからがつくおどうぐを\nさがしているんじゃが?");
const u8 D_2540A60[] = _("グルメなおどうぐ\nをさがしているんじゃが?");
const u8 D_2540A8C[] = _("このにわはせかいいち\nのにわだぞい!");
const u8 D_2540AB4[] = _("がほしいんだけどー?");
const u8 D_2540ACC[] = _("かしこくみえるようふくか\nアクセのオススメあるー?");
const u8 D_2540B00[] = _("おしゃれなようふくか\nアクセのオススメあるー?");
const u8 D_2540B30[] = _("カリスマせいバツグンの\nようふくかアクセあるー?");
const u8 D_2540B64[] = _("ちからみなぎるようふくか\nアクセのオススメあるー?");
const u8 D_2540B98[] = _("グルメっぽいようふくか\nアクセのオススメあるー?");
const u8 D_2540BCC[] = _("ガーデニング\nちょースゴーい★★★");
const u8 D_2540BF0[] = _("があるってきいたんだけど");
const u8 D_2540C0C[] = _("あたまがよくなるオススメ\nりょうりはあるかい?");
const u8 D_2540C3C[] = _("おしゃれなオススメ\nりょうりはあるかい?");
const u8 D_2540C68[] = _("みんなににんきのオススメ\nりょうりはあるかい?");
const u8 D_2540C98[] = _("ちからがでるオススメ\nりょうりはあるかい?");
const u8 D_2540CC4[] = _("おいしさバッチリなオスス\nメりょうりはあるかい?");
const u8 D_2540CF8[] = _("こ､このにわは…!\nなんと…すばらしい…");
const u8 *const D_2540D24[35] = {
    D_25406FC,
    D_2540724,
    D_2540754,
    D_2540778,
    D_25407A8,
    D_25407D8,
    D_2540808,
    D_2540834,
    D_2540860,
    D_254088C,
    D_25408C0,
    D_25408F4,
    D_2540920,
    D_2540954,
    D_2540984,
    D_25409A0,
    D_25409D4,
    D_2540A04,
    D_2540A30,
    D_2540A60,
    D_2540A8C,
    D_2540AB4,
    D_2540ACC,
    D_2540B00,
    D_2540B30,
    D_2540B64,
    D_2540B98,
    D_2540BCC,
    D_2540BF0,
    D_2540C0C,
    D_2540C3C,
    D_2540C68,
    D_2540C98,
    D_2540CC4,
    D_2540CF8,
};
const u8 D_2540DB0[] = _("つぼみ☼こうえん");
const u8 D_2540DC4[] = _("たまデパ");
const u8 D_2540DD0[] = _("たまもりショップ");
const u8 D_2540DE4[] = _("♪カフェ♪");
const u8 D_2540DF0[] = _("ゲームセンターに");
const u8 D_2540E04[] = _("いく");
const u8 D_2540E0C[] = _("いかない");

/*
 * Seasonal flower in the window: by month (12-2 winter 8, 3-5 spring 1, 6-8 summer 2,
 * 9-11 autumn 4) an 80 % chance to set pet.window_flower (the seasonal picture, +2 hearts of
 * happiness), else -1 heart.
 */
void room_window_seasonal_flower(void)
{
    u8 k;
    int r;

    k = rtc_get_month();
    visit_load_partner_pics(0);
    switch (k) {
    case 1:
    case 2:
    case 12:
        if (rand() % 100 > 80) {
            r = 0;
        } else {
            anim_pic_slots[4][0] = D_219D820;
            r = 1;
            save.pet.window_flower = 8;
        }
        break;
    default:
        switch (k) {
        case 3:
        case 4:
        case 5:
            if (rand() % 100 > 80) {
                r = 0;
            } else {
                anim_pic_slots[4][0] = D_219C248;
                r = 1;
                save.pet.window_flower = r;
            }
            break;
        case 6:
        case 7:
        case 8:
            if (rand() % 100 > 80) {
                r = 0;
            } else {
                anim_pic_slots[4][0] = D_219C968;
                r = 1;
                save.pet.window_flower = 2;
            }
            break;
        case 9:
        case 10:
        case 11:
        default:
            if (rand() % 100 > 80) {
                r = 0;
            } else {
                r = 1;
                save.pet.window_flower = 4;
            }
            break;
        }
        break;
    }
    if (r != 1)
        pet_sub_happiness(1);
    else
        pet_add_happiness(2);
}

/*
 * Returns a, or 1 when a == 0.
 */
int util_nonzero_or_one(int a)
{
    if (a == 0)
        return 1;
    return a;
}

/*
 * Random outing event by date / hour (random seed from the T16 ch.2 counter), internal
 * kinds in brackets: once a day at 8, 13 and 17 h (flags 0x4B / 0x4C / 0x4D) the
 * scheduled visitor visit_scheduled_visitor (7); on fixed dates (2/14, 4/1, 5/12-15, 6/18-21, 9/13,
 * 11/15: 40 % chance) the dated special visit_gift(y, x) (6); 13-17 h 20 % the
 * travelling merchant visit_merchant (3, the only one that returns 1); 20 % visit_friend
 * (5).
 */
u8 pet_event_random_daily(void)
{
    int s;
    u8 a, b, c;
    u8 x, y;

    s = 0;
    a = rtc_get_month();
    b = rtc_get_hour();
    c = rtc_get_day();
    srand(T16_TC2);
    util_rand_advance();
    for (;;) {
        if (b == 8 && pet_get_unlock_flag(0x4b) == s) {
            s = 7;
            pet_set_unlock_flag(0x4b, 1);
            break;
        }
        if (b == 13 && pet_get_unlock_flag(0x4c) == 0) {
            s = 7;
            pet_set_unlock_flag(0x4c, 1);
            break;
        }
        if (b == 17 && pet_get_unlock_flag(0x4d) == 0) {
            s = 7;
            pet_set_unlock_flag(0x4d, 1);
            break;
        }
        for (;;) {
            if (a == 2 && c == 14) {
                s = 6;
                y = 0x5a;
                x = 0x80;
                break;
            }
            if (a == 4 && c == 1) {
                s = 6;
                y = 0x57;
                x = 0x7b;
                break;
            }
            if (a == 5 && (c >= 12 && c <= 15)) {
                s = 6;
                y = 0x57;
                x = 0x7c;
                break;
            }
            if (a == 6 && (c >= 0x12 && c <= 0x15)) {
                s = a;
                y = 0x58;
                x = 0x7d;
                break;
            }
            if (a == 9 && c == 13) {
                s = 6;
                y = 0x59;
                x = 0x7e;
                break;
            }
            if (a == 11 && c == 15) {
                s = 6;
                y = 0x59;
                x = 0x7f;
            }
            break;
        }
        if (s == 6) {
            if (rand() % 100 < 40)
                break;
            s = 0;
        }
        if (b >= 13 && b <= 17 && rand() % 100 < 20) {
            s = 3;
            break;
        }
        if (rand() % 100 < 20)
            s = 5;
        break;
    }
    switch (s) {
    case 3:
        visit_merchant();
        return 1;
    case 5:
        visit_friend();
        return 0;
    case 6:
        visit_gift(y, x);
        return 0;
    case 7:
        visit_scheduled_visitor();
        return 0;
    }
    return 0;
}

/*
 * Wrapper of pet_event_random_daily.
 */
int pet_event_random_daily_wrapper(void)
{
    return pet_event_random_daily();
}

int f_2413B08(const u8 *, int, int) __asm__("town_travel_scene");
/*
 * Tama-mori shop (たまもりショップ, adults): "いく?" then the featured item shop_offer_item
 * (picked by shop_tamamori_pick) on its card (shop_tamamori_card, price or FREE) and "おかいあげ?";
 * on yes and enough points (pet_can_pay): buy (club counter 2, store with item_add_to_inventory,
 * pay, animation anim_tamamori_shop) and save.
 */
int shop_tamamori(void)
{
    int r = 0;
    int a, b, id;

    if (outing_signpost(2) == 0) {
        shop_set_discount_mode(0);
        return r;
    }
    a = 1;
    if (ui_yes_no(D_2540E70, D_2540E04, D_2540E0C, 16, &a) == 1 && a == 0
        && f_2413B08(D_20BA824, 0, 0) == 0
        && shop_tamamori_card() != 0) {
        b = 1;
        if (ui_yes_no(D_2540E84, D_2540E94, D_2540E9C, 16, &b) == 1 && b == 0) {
            pet_count_activity(2);
            id = item_price((struct TamagoFile *)shop_offer_item);
            if (pet_can_pay(id)) {
                snd_wait_end();
                if (item_add_to_inventory((struct TamagoFile *)shop_offer_item)) {
                    anim_tamamori_shop();
                    r = 1;
                    pet_spend_points(id);
                    save_game();
                    anim_happy_outdoor();
                }
            }
        }
    }
    shop_set_discount_mode(0);
    return r;
}

/*
 * Tama-mori shop item card: item picture, name, price (or "FREE").
 */
u8 shop_tamamori_card(void)
{
    const u8 *buf[15];
    int i;
    u16 k;
    int x;
    u8 *q;
    const u8 *s;
    const u8 *t;
    struct TamagoFile *p;
    u8 a;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    p = (struct TamagoFile *)shop_offer_item;
    item_load_pictures(buf, p, 0xf);
    q = 0;
    if (p->x04e == 0x81) {
        if (p->x04f == 3) {
            s = D_2092294;
            gfx_object_create(0x40 - (s[0] >> 1), 0x3f - s[1], 0x10, (u8)(int)q, 0, (const u8 *)s);
            q = (u8 *)buf[6];
        } else
            q = (u8 *)buf[0];
    } else
        q = (u8 *)buf[0];
    gfx_object_create(0x40 - (q[0] >> 1), 0x3f - q[1], 0x20, 0, 0, (const u8 *)q);
    t = D_2194F18;
    gfx_object_create(7, 0x3f - t[1], 0x40, 0, 0, (const u8 *)t);
    a = gfx_object_create(0, 0x41, 0x50, 0, 0, 0);
    item_draw_name((struct TamagoFile *)shop_offer_item, a, 0x41, 0x50);
    k = item_price((struct TamagoFile *)shop_offer_item);
    if (k != 0) {
        x = 0x45;
        for (i = 0; i <= 4; i++) {
            if (k == 0 && i != 0)
                break;
            gfx_object_create(x, 0x51, 0x60, k % 10, 0, D_208EDB0);
            x -= 10;
            k /= 10;
        }
        gfx_object_create(0x4f, 0x51, 0x50, 0x1a, 0, (const u8 *)&D_2090E0C);
    } else {
        a = gfx_object_create(0, 0x51, 0x60, 0, 0, 0);
        gfx_draw_text_centered(a, 0x51, 0x60, D_2540EA4, 0, 4);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_b_presses)
            return 1;
        if (btn_c_presses)
            break;
        sys_idle_tick();
    } while (ui_timeout);
    return 0;
}

struct ShopCategory { const u8 *label; u32 bank; u32 kind; };
extern const struct ShopCategory shop_tamamori_tabs[2];
extern const struct ShopCategory shop_reform_rooms[5];
/*
 * Pick the Tama-mori shop's featured item: a random category (bank) from shop_tamamori_tabs, a
 * random record among the built-in records of that category (item_bank_record(k, count + n));
 * shop_offer_kind / shop_offer_index remember the choice; draw it.
 */
void shop_tamamori_pick(void)
{
    const u8 *buf[15];
    u32 k, r;
    struct TamagoFile *p;
    u8 *img;
    u8 *q;
    u8 a;

    shop_offer_kind = rand() % 2;
    k = shop_tamamori_tabs[shop_offer_kind].bank;
    r = item_bank_tbl[k].slot_count + rand() % item_bank_tbl[k].slot_count;
    shop_offer_index = r;
    shop_offer_item = (u32)item_bank_record(k, r);
    p = (struct TamagoFile *)shop_offer_item;
    item_load_pictures(buf, p, 0xf);
    if (p->x04e == 0x81) {
        if (p->x04f == 3) {
            img = (u8 *)buf[6];
            goto ok;
        }
    }
    img = (u8 *)buf[3];
ok:
    shop_set_discount_mode(2);
    gfx_display_reset();
    q = *chara_sprite_set71_pose2;
    gfx_object_create(0x15 - (q[0] >> 1), 0x4f - (q[1] >> 1), 0x10, 0, 0x10, (const u8 *)q);
    if (p->x04e == 0x81) {
        if (p->x04f == 3) {
            gfx_object_create(0x4e - (img[0] >> 1), 0x46 - img[1], 0x30, 0, 0, (const u8 *)img);
            goto done;
        }
    }
    gfx_object_create(0x4e - (img[0] >> 1), 0x3c - (img[1] >> 1), 0x30, 0, 0, (const u8 *)img);
done:
    a = gfx_object_create(0, 0, 0x50, 0, 0, 0);
    item_draw_name((struct TamagoFile *)shop_offer_item, a, 0x1f, 0x50);
    func_00009A6();
}

/*
 * Gotchi Reform (ごっちリフォーム): "いく?", the preview of a random room design of one
 * of five rooms (リビング / キッチン / トイレ / おふろ / しんしつ, shop_reform_pick),
 * "リフォームする?", half price; the makeover animation anim_reform_preview / anim_reform_done, the
 * new design index stored at pet.weather_now + 2 + kind, +2 hearts.
 */
int shop_reform(void)
{
    int r = 0;
    int n;
    int x, y, z;
    u32 k;
    u8 *q;
    u8 c;

    if (outing_signpost(3) == r)
        return r;
    x = 1;
    n = ui_yes_no(D_2540F20, D_2540E04, D_2540E0C, 0x10, &x);
    if (n == 1) {
        z = x;
        if (z != 0)
            goto out;
        if (f_2413B08((const u8 *)0x20bba4c, z, z) != 0)
            goto out;
        if (shop_reform_card() == 0)
            goto out;
        y = n;
        if (ui_yes_no(D_2540F34, D_2540E94, D_2540E9C, 0x10, &y) != 1)
            goto out;
        n = y;
        if (n != 0)
            goto out;
        k = item_price(shop_offer_item) >> 1;
        if (pet_can_pay(k) == 0)
            goto out;
        q = (u8 *)&save.pet + shop_offer_kind;
        if (q[0xbc] > 3)
            q[0xbc] = n;
        c = save.pet.room_designs[shop_offer_kind];
        switch (shop_offer_kind) {
        case 0:
            anim_reform_preview((const u8 *)room_living_designs[c]);
            anim_reform_done(shop_offer_item + 0x204);
            snd_wait_end();
            item_add_to_inventory(shop_offer_item);
            break;
        case 1:
            anim_reform_preview((const u8 *)room_kitchen_designs[c]);
            anim_reform_done(room_kitchen_designs[shop_offer_index]);
            break;
        case 2:
            anim_reform_preview((const u8 *)room_toilet_designs[c]);
            anim_reform_done(room_toilet_designs[shop_offer_index]);
            break;
        case 3:
            anim_reform_preview((const u8 *)room_bath_designs[c]);
            anim_reform_done(room_bath_designs[shop_offer_index]);
            break;
        case 4:
            anim_reform_preview((const u8 *)room_bedroom_designs[c]);
            anim_reform_done(room_bedroom_designs[shop_offer_index]);
            break;
        }
        save.pet.room_designs[shop_offer_kind] = shop_offer_index;
        pet_add_happiness(2);
        pet_spend_points(k);
        save_game();
        r = 1;
    }
out:
    return r;
}

/*
 * Second merchant item card with the halved price.
 */
u8 shop_reform_card(void)
{
    const u8 *buf[15];
    int i = 0;
    u16 k;
    int x;
    const u8 *q;
    u8 a;

    ui_new_screen();
    item_load_pictures(buf, (struct TamagoFile *)shop_offer_item, 0xf);
    gfx_object_create(0, 0x1c, 0, 0, 0, (const u8 *)buf[0]);
    gfx_object_create(0, 0x1c, 0x20, 0, 0, D_2106CA4);
    q = D_2194F18;
    gfx_object_create(0x3c - (q[0] >> 1), 0x29 - (q[1] >> 1), 0x30, 0, 0, (const u8 *)q);
    a = gfx_object_create(0, 0x41, 0x40, 0, 0, 0);
    item_draw_name((struct TamagoFile *)shop_offer_item, a, 0x41, 0x40);
    k = item_price((struct TamagoFile *)shop_offer_item) >> 1;
    if (k != 0) {
        x = 0x45;
        for (i = 0; i <= 4; i++) {
            if (k == 0 && i != 0)
                break;
            gfx_object_create(x, 0x51, 0x50, k % 10, 0, D_208EDB0);
            x -= 10;
            k /= 10;
        }
        gfx_object_create(0x4f, 0x51, 0x40, 0x1a, 0, (const u8 *)&D_2090E0C);
    } else {
        a = gfx_object_create(0, 0x51, 0x50, 0, 0, 0);
        gfx_draw_text_centered(a, 0x51, 0x50, D_2540EA4, 0, 4);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_b_presses)
            return 1;
        if (btn_c_presses)
            break;
        sys_idle_tick();
    } while (ui_timeout);
    return 0;
}

/*
 * Pick the Gotchi Reform design to offer: a random room design from the tables shop_reform_rooms
 * / room_living_designs.., never the current design pet.room_designs[0][kind].
 */
void shop_reform_pick(void)
{
    const u8 *buf[15];
    u32 k;
    int r;
    int n;
    u32 img;
    const u8 *q;
    u8 a;
    struct TamagoFile *p;

    shop_offer_kind = rand() % 5;
    k = shop_reform_rooms[shop_offer_kind].bank;
    shop_offer_index = item_bank_tbl[k].slot_count;
    do {
        n = rand() % item_bank_tbl[k].builtin_count;
        r = n;
    } while (n == 0 || n == save.pet.room_designs[shop_offer_kind]);
    shop_offer_index += r;
    p = item_bank_record(k, shop_offer_index);
    shop_offer_item = (u32)p;
    item_load_pictures(buf, p, 0xf);
    img = buf[0];
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, (const u8 *)img);
    q = D_21D5CCC;
    gfx_object_create(0x6c - (q[0] >> 1), 0x4f - (q[1] >> 1), 0x20, 0, 0, (const u8 *)q);
    a = gfx_object_create(0, 0, 0x30, 0, 0, 0);
    item_draw_name((struct TamagoFile *)shop_offer_item, a, 0x1f, 0x40);
    func_00009A6();
}

const u8 D_2540E3C[] = _("ようふく");
const u8 D_2540E48[] = _("アクセサリー");
const struct ShopCategory shop_tamamori_tabs[2] = {
    { D_2540E3C, 2, 0x33 },
    { D_2540E48, 3, 0x34 },
};
const u8 D_2540E70[] = _("たまもりショップに");
const u8 D_2540E84[] = _("おかいあげ?");
const u8 D_2540E94[] = _("はい");
const u8 D_2540E9C[] = _("いいえ");
const u8 D_2540EA4[] = _("FREE");
const u8 D_2540EB0[] = _("リビング");
const u8 D_2540EBC[] = _("キッチン");
const u8 D_2540EC8[] = _("トイレ");
const u8 D_2540ED0[] = _("おふろ");
const u8 D_2540ED8[] = _("しんしつ");
const struct ShopCategory shop_reform_rooms[5] = {
    { D_2540EB0, 6, 0x1f },
    { D_2540EBC, 11, 0x1f },
    { D_2540EC8, 12, 0x1f },
    { D_2540ED0, 13, 0x1f },
    { D_2540ED8, 14, 0x1f },
};
const u8 D_2540F20[] = _("ごっちリフォームに");
const u8 D_2540F34[] = _("リフォームする?");

/*
 * The RAM variables this file defines.
 */
u8 room_visitor_scene;
u8 town_visit_flag;
u32 room_window_tick DSTRAM;
u8 room_window_sky_obj DSTRAM;
u8 room_window_sky_anim DSTRAM;
u8 D_00841FE DSTRAM;
u8 D_00841FF DSTRAM;
struct PasserObjects room_window_passer_objs DSTRAM;
struct PasserPalettes room_window_passer_palettes DSTRAM;
struct PasserStep *room_window_passer_steps;
u32 shop_offer_item = 0;
u8 shop_offer_kind = 0;
u32 shop_offer_index = 0;
