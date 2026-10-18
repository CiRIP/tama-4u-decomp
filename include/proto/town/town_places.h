/* Declarations of src/town/town_places.c: its types, variables and functions. */
#ifndef PROTO_TOWN_TOWN_PLACES_H
#define PROTO_TOWN_TOWN_PLACES_H

#include "structs.h"

struct Job {
    u8 kind;
    u8 hours[5];
    u16 minutes;
    u8 finished;
};

struct ShopKeeper {
    u16 sprite_id;
    u8 first_line;
    u8 line_count;
};

struct ParkContact {
    u16 chara_id;
    u16 visitor_sprite;
    u8 first_line;
    u8 line_count;
};

struct ClubLimits {
    u8 x000;
    u8 practice_limit;
    u8 achievement_limit;
};

/* menu entries of ui_paged_list: text, flags */
struct ClubListEntry {
    const u8 *text;
    u16 flags;
};

extern u8 town_place_aborted;
extern struct Job job;
extern u32 diary_images;
extern u32 town_locked_jump;
extern u8 town_last_street_chara;
extern int (*D_0084498[])(void);
extern const u8 *town_club_tool_names[];
extern u8 *town_club_common_tool_name;
extern const u8 *town_club_backgrounds[];

extern const struct ShopKeeper shop_keepers[];
extern const u8 D_2541D20[];
extern const u8 D_2541D24[];
extern const u8 D_2541D28[];
extern const struct PlaceEntry town_places[];
extern const struct ParkContact town_park_contacts[];
extern const u32 D_254228C;
extern const u32 D_25422A8;
extern const u16 town_match_species_a[];
extern const u16 town_match_species_b[];
extern const u32 D_254231C;
extern const u32 D_2542468;
extern const u32 D_2542498;
extern const u32 D_25424B4;
extern const u32 D_25424D0;
extern const struct ClubLimits town_club_limits[];
extern const u8 D_2542548[];
extern const u16 town_club_lines[];
extern const u32 D_25425C8;
extern const u32 D_25425DC;
extern const u32 D_25425F0;
extern const u32 D_2542688;
extern const u32 D_25426A4;
extern const u32 D_25426D8;
extern const u8 D_2541FA8[];
extern const u8 D_2541FB8[];
extern const u8 D_2541FC4[];
extern const u8 D_2541FD8[];
extern const u8 D_2542674[];
extern const struct ClubListEntry town_club_menu_tbl[];
extern const struct PlaceEntry shop_floor_menu_tbl[];
extern const u8 D_2542094[];
extern const struct EncounterEntry town_park_charas_morning[];
extern const struct EncounterEntry town_park_charas_midday[];
extern const struct EncounterEntry town_park_charas_evening[];
extern const struct EncounterEntry town_park_charas_night[];
extern const u8 D_254226C[];
extern const u8 D_254227C[];
extern const u8 D_2542284[];
extern const struct BankListEntry shop_bank_list[];
extern const u8 D_25422C0[];
extern const u8 D_25422D0[];
extern const u8 D_25422D8[];
extern const u8 D_2542444[];
extern const u8 D_2542458[];
extern const u8 D_2541FE8[];
extern const u8 D_2541FFC[];
extern const u8 D_2542010[];
extern const u8 D_254201C[];
extern const u8 D_2542024[];
extern const u8 D_2541FDC[];
extern const u8 D_254223C[];
extern const u8 D_254224C[];
extern const u8 D_254225C[];
extern const u8 D_2542264[];
extern const u8 D_2542484[];
extern const u8 D_25426BC[];
extern const u8 D_2542300[];
extern const u8 D_254230C[];
extern const u8 D_2542314[];
extern const u8 D_25426CC[];

void town_award_care_mark(int);
u8 *job_select_chara(u8);
void job_clear(void);
void job_clear_all(void);
void job_start(int);
int job_is_finished(void);
u8 job_check_hours(void);
int job_minute_tick(void);
int job_reward_item(void);
void job_diary_page(void);
int job_end(u8 a);
u8 pet_activity_done(u8);
u8 pet_activity_done_count(void);
int pet_activity_done_bool(int);
void anim_place_enter_blank(const u8 *);
int town_street_encounter(struct EncounterEntry *, int);
void shop_pick_featured_item(void);
int town_park_event_nobody(void);
int town_park_greeting(void);
void item_give_builtin(int);
int town_visitor_entry(u16 id, int *x, int *y);
int town_list_unlocked_visitors(u16 *, u32 *, u32 *, u32 *, u16);
int town_park_event_friend(void);
int town_park_meet_contact(void);
int shop_sell(void);
int shop_tama_depa(void);
int town_lottery(void);
int town_park_square_event(void);
void town_sports_ground(void);
int town_park_menu(void);
int chara_id_alias(u8);
void util_shuffle_u16(u16 *, int);
void pet_life_story(void);
u8 town_match_candidates(u8 *);
u8 town_match_choose(u8 *);
int town_matchmaking(void);
int chara_transform_target(void);
int chara_transform(void);
int util_pick_one_of_two(int, int);
int outing_story_chara(u8);
int outing_return_diary(void);
void outing_pick_destination(void);
void outing_setup(void);
u8 outing_return_handler(void);
int outing_screen(u8);
int outing_text_number(u8);
const u8 *town_club_background(int);
void town_club_teacher_load(int, int);
void town_club_teacher_line(u8 *);
void town_club_missing_tool(int, u16, u8 *);
void town_club_practice_anim(int);
void town_club_progress_scene(int, u8 *);
void town_club_first_visit(int);
void town_club_visit(int);
int town_club_choose(u32 *);
int town_school(void);
u8 town_travel_scene(const u8 *, u8, int);
int town_game_center_greeting(void);
u8 town_enter_park(void);
int town_enter_school(void);
int town_enter_tamadepa(void);
u8 shop_floor_menu(void);
int town_enter_matchmaking(void);
int town_enter_nopic(void);
int town_enter_nopic_b(void);
u8 town_hook_stub_a(int);
int town_hook_stub_call(void);
int town_always_open(void);
int town_always_open_b(void);
int town_always_open_c(void);
int town_always_open_d(void);
int town_app_slot_state(u8);
void town_app_slot0_state(void);
void town_app_slot1_state(void);
void town_app_slot2_state(void);
void town_app_slot3_state(void);
void town_car_slot_pic(void);
int town_place_state(int);
int town_entry_app_slot(int);
int town_next_visible_entry(int);
u8 town_place_count(void);
void town_menu_draw_page(int);
u8 town_menu(void);
int ui_place_entry_avail(struct PlaceEntry *, int);
int ui_place_next_avail(struct PlaceEntry *, int, int);
void ui_place_draw_page(struct PlaceEntry *, int, int);
u8 ui_place_menu_run(u8 *, int);
int town_depa_floor_enter(void);
int town_game_corner(void);
int town_reform_shop(void);
int town_item_shop(void);
int town_fashion_shop(void);
void town_place_leave(void);

#endif
