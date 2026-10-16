/* Declarations of src/spot/spot_reader.c: its types, variables and functions. */
#ifndef PROTO_SPOT_SPOT_READER_H
#define PROTO_SPOT_SPOT_READER_H

#include "structs.h"

struct SpotRecordMagic {
    u8 hi;
    u8 lo;
};

struct SpotRecordInfo {
    u8 kind;
    u8 content_type;
};

extern volatile u8 btn_a_presses;
extern volatile u8 btn_b_presses;
extern volatile u8 btn_c_presses;
/* UID of the last tag / card polled by the Touch Spot reader (10 bytes, RAM only, not
   saved): the "not twice in a row" check of spot_reader */
extern u8 spot_last_uid[];
extern s8 spot_reader_result;
extern struct SpotRecordMagic spot_record_magic;
extern struct SpotRecordInfo spot_record_info;
extern u8 spot_record_class[2];

extern const u32 D_255D40C;
extern const u32 D_255D424;
extern const u32 D_255D450;
extern const u32 D_255D484;
extern const u32 D_255D4B0;
extern const u32 D_255D4D4;
extern const u32 D_255D508;
extern const u8 D_255D544[];
extern const u8 D_255D55C[];
extern const u8 D_255D574[];
extern const u8 D_255D57C[];
extern const u8 D_255D584[];
extern const u8 D_255D590[];
extern const u8 D_255D5A0[];
extern const u8 D_255D5B0[];
extern const char D_255D47C[];
extern const u8 D_255D378[];
extern const u8 D_255D380[];
extern const u8 D_255D390[];
extern const u8 D_255D3A4[];
extern const u8 D_255D3BC[];
extern const u8 D_255D3D0[];
extern const u8 D_255D3E4[];
extern const u8 D_255D3F8[];
extern const u8 D_255D348[];
extern const u8 D_255D34C[];
extern const u8 D_255D350[];
extern const u8 D_255D354[];
extern const u8 D_255D35C[];
extern const u8 D_255D368[];
extern const u8 D_255D370[];

int spot_bingo_read(void);
int spot_touch_menu_entry(void);
void spot_reader_mode2(void);
void test_spot_show_ver(u8 *, u8 *);
void test_spot_show_inf(struct SpotRecordInfo *, u8 *);
void test_spot_show_cls(const u8 *src, u8 *dst);
u8 test_spot_screen(u8);
u8 test_spot_read(void);
u8 test_spot_check(void);
u8 test_spot_write(void);
int spot_touch_any_reader(u8);
void nfc_restore_game_timing(void);
int spot_reader(u8);
int spot_check_sum(u8 *, u32);
int spot_sum(u8 *, u32);
int spot_visitor_sprite(u16);
int item_coupon_class(struct TamagoFile *);
int spot_bingo_card_kind(struct TamagoFile *);
int spot_bingo_id_matches(u8);
int spot_is_gashapon_ticket(struct TamagoFile *);
u32 spot_content_entry_count(s8);
int spot_content_header_field(u8);
int spot_content_character(u8);
int spot_content_picture(u8);
int spot_content_name(u8);
int spot_content_file(u8);
int spot_content_file_length(u8);
int spot_content_file_record(u8);
int spot_content_file_picture(u8, u8);
int spot_content_file_name(u8);
u8 spot_content_file_kind(u8);
void spot_content_browser(u8);
void spot_bingo_card_screen(void);
void spot_show_message(int);
void spot_gift_points(u16);
void spot_visitor_message(const u8 *);
void spot_visitor_scene(const u8 *);
int chara_is_builtin(s16);
u32 ui_question_four_lines(const void *, const void *, const void *, const void *, int, int *);
s8 spot_ask_downloaded_all(void);
void spot_items_full_notice(void);
int spot_replace_card_dialog(u8);
int spot_install_gashapon_ticket(u16 *);
int spot_install_bingo_card(u16 *);
int spot_wait_reader(void);

#endif
