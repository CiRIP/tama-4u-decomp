/* Declarations of src/room/room_window.c: its types, variables and functions. */
#ifndef PROTO_ROOM_ROOM_WINDOW_H
#define PROTO_ROOM_ROOM_WINDOW_H

#include "structs.h"

struct PasserObjects {
    u8 obj_a;
    u8 obj_b;
};

struct PasserPalettes {
    u8 pal_a;
    u8 pal_b;
};

/* A room scene step, one per frame (the current list is room_scene_current, drawn by sys_main,
 * which reads it as its struct RoomStepSigned with a signed picture byte): picture x000
 * (0xff ends the list), flags x001 (0x10 = mirrored), position x002 / x004. The lists are
 * in F21 (room_scene_egg..), F24 (room_window_scene_a..) and F44 (room_scene_sitter.., the sitter scenes). */
struct RoomStep {
    u8 pose;
    u8 flags;
    s16 x;
    s16 y;
};

extern u8 room_visitor_scene;
extern u8 town_visit_flag;
/* communication level 0..4 = the pet's house (comm_house_level) */
extern u32 room_window_tick;
extern u8 room_window_sky_obj;
extern u8 room_window_sky_anim;
extern u8 D_00841FE;
extern u8 D_00841FF;
extern struct PasserObjects room_window_passer_objs;
extern struct PasserPalettes room_window_passer_palettes;
extern struct PasserStep *room_window_passer_steps;
extern u32 shop_offer_item;
extern u8 shop_offer_kind;
extern u32 shop_offer_index;

extern const u32 D_25406A8;
extern const u32 D_25406D8;
extern const u32 D_2540E18;
extern const u32 D_2540F48;
extern const u8 room_weather_by_period[][5];
extern const u8 D_2540DF0[];
extern const u8 D_2540E04[];
extern const u8 D_2540E0C[];
extern const u8 D_2540E70[];
extern const u8 D_2540E84[];
extern const u8 D_2540E94[];
extern const u8 D_2540E9C[];
extern const u8 room_weather_month_odds[][6];
extern const u8 room_weather_month_ids[][6];
extern const u8 D_2540F20[];
extern const u8 D_2540F34[];
extern const u8 D_2540EA4[];
extern const u8 D_2540DB0[];
extern const u8 D_2540DC4[];
extern const u8 D_2540DD0[];
extern const u8 D_2540DE4[];
extern const struct RoomStep room_window_scene_a[];
extern const struct RoomStep room_window_scene_b[];
extern const struct RoomStep room_window_scene_c[];
extern const struct RoomStep room_window_scene_bad_weather[];

void room_weather_update_now(void);
void room_weather_draw_daily(void);
int comm_house_level(void);
int room_window_draw(u8);
void room_window_animate(u8);
int town_visit_active(void);
void town_visit_begin(void);
void town_visit_end(void);
void func_24101F6(void);
u8 outing_signpost(int);
void outing_signpost_random(void);
void outing_park_gate(void);
void util_shuffle_u8(u8 *, int);
void outing_random(void);
void room_window_seasonal_flower(void);
int util_nonzero_or_one(int);
u8 pet_event_random_daily(void);
int pet_event_random_daily_wrapper(void);
int shop_tamamori(void);
u8 shop_tamamori_card(void);
void shop_tamamori_pick(void);
int shop_reform(void);
u8 shop_reform_card(void);
void shop_reform_pick(void);

#endif
