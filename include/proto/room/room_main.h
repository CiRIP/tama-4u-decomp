/* Declarations of src/room/room_main.c: its types, variables and functions. */
#ifndef PROTO_ROOM_ROOM_MAIN_H
#define PROTO_ROOM_ROOM_MAIN_H

#include "structs.h"

extern u8 lcd_palette_set; /* palette table set of the LCD flush func_00006B8: 1 = 0x3C10.., 0 = +0x100; 1 in the test mode */
extern u8 room_clock_notice;
extern u8 comm_menu_active;
extern u8 sys_backlight_level;
extern volatile u8 room_idle;
extern u16 ui_main_icon;
extern u8 room_view;
extern volatile u8 ui_bubble_line_idx;
extern u8 sys_battery_dead;
extern struct RoomStepSigned *room_scene_current;
extern struct RoomObjStepSigned *room_scene_objs;
extern u16 ui_icon_drawn;
extern u8 room_view_at_sleep;
extern u8 room_poops_at_sleep;
extern u8 room_scene_obj_idx;
extern u8 ui_call_icon_obj;
extern u8 ui_icon_cursor_obj;
extern u8 room_status_icon_obj;
extern u8 sys_sleep_request;
extern u8 room_calls_drawn;
extern u8 room_status_drawn;
extern u8 room_period_drawn;
extern u8 room_last_sleep_state;
extern u8 lcd_asleep; /* 1 = LCD controller in sleep-in (SLPIN sent, sys_display_off) */
extern u8 ui_attention_icon_state;
extern u8 D_0003FF7;
extern u8 room_mood_question_due;
extern u8 ui_bubble_frame_obj;
extern u8 ui_bubble_line1_obj;
extern u8 ui_bubble_line2_obj;
extern u8 ui_choice_cursor_l_obj;
extern u8 ui_choice_cursor_r_obj;
extern u8 ui_arrow1_obj;
extern u8 ui_arrow2_obj;
extern u8 sys_battery_empty;
extern u16 comm_count_before;

extern const struct XyPair ui_arrow1_positions[];
extern const struct XyPair ui_arrow2_positions[];
extern const u32 D_253EEA0;
extern const u8 D_253EF10[];
extern const u8 D_253EF20[];
extern const u8 D_253EF34[];
extern const u8 D_253EF44[];
extern const u8 D_253EED8[];
extern const u8 D_253EEEC[];
extern const u8 D_253EEFC[];
extern const u8 D_253EEC8[];
extern const u8 D_253EED0[];

void ui_update_mail_mark(void);
void ui_show_attention_icon(void);
void ui_call_icon_hide(void);
void ui_call_icon_sync(void);
void ui_icon_cursor_draw(void);
void ui_draw_icon_bars(void);
void ui_new_screen(void);
void ui_bubble_alloc(void);
struct XyPair *ui_bubble_next_line(struct XyPair *);
void ui_bubble_fill(void);
void ui_bubble_hide(void);
void ui_choice_cursors_create(void);
void ui_arrows_create(void);
void ui_arrows_move(int);
void ui_choice_cursors_bob(int);
u8 pet_try_daily_care_leaf(void);
void sys_display_off(void);
void sys_display_wake(int);
void sys_idle_tick(void);
void lcd_init_ili9163_old(void);
void lcd_init_st7735(void);
int ui_sound_menu(void);
void room_draw_away(void);
void ui_clock_check_msg(void);
void pet_poke_animation(void);
u8 ui_main_icon_blocked(u8, u8);
u8 pet_is_away(void);
u8 ui_run_away_screen(void);
u8 ui_main_pending_hook(void);
void pet_away_minute_check(void);
u8 pet_special_mode_active(void);
int nfc_may_arm(void);
void comm_notebook_unlock_check(void);
void comm_remember_count(void);
int nfc_idle_touch(void);
u8 pet_asleep_refuse(void);
void nfc_arm_after_draw(void);
void sys_main(void);
int sys_power_down(void);
u8 pet_care_event_dispatch(void);

#endif
