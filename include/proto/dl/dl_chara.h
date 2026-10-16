/* Declarations of src/dl/dl_chara.c: its types, variables and functions. */
#ifndef PROTO_DL_DL_CHARA_H
#define PROTO_DL_DL_CHARA_H

#include "structs.h"

struct DlCharaRecord {
    u16 sprite_set;
    u16 id;
    u8 album_key;
    u8 pad005[0x2];
    u8 gender;
    u8 x008;
    u8 pad009[0x5];
    u8 stage;
    u8 pad00f[0x1];
    u8 base_weight;
    u8 hunger_period;
    u8 happiness_period;
    u8 sick_percent;
    u8 wake_hour;
    u8 sleep_hour;
    u8 pad016[0x1];
    u8 fit_class;
    u16 x018;
    u16 x01a;
    u8 bday_month;
    u8 bday_day;
    u16 x01e;
    u16 x020;
    u16 x022;
    u8 pad024[0x6];
    u16 fav_item_class;
    u16 fav_item_id;
};


extern const u8 *const dl_chara_protected_names[];
extern const u32 D_253C5F4;
extern const u8 D_253C56C[];
extern const u8 D_253C618[];
extern const u8 D_253C62C[];
extern const u8 D_253C574[];
extern const u8 D_253C57C[];
extern const u8 D_253C584[];
extern const u8 D_253C58C[];

int dl_chara_count(void);
u8 *dl_chara_file(u8);
struct DlCharaRecord *dl_chara_record(u8);
struct DlCharaRecord *dl_chara_record_by_id(u16);
u8 dl_chara_slot_by_name(u8 *);
u8 *dl_chara_sprites(u8);
u8 *dl_chara_pose_pic(u8, u8);
u8 *dl_chara_large_pic(u8);
u8 *dl_chara_description(u8);
void dl_chara_affection_update(int);
u8 dl_chara_class(u8);
u8 dl_chara_gender_mark(u8);
void dl_chara_slot_write(int, int);
int dl_chara_name_protected(u8 *);
int dl_chara_store(int);
u8 dl_chara_read_menu(u8);
int dl_chara_pick_slot(void);
s16 txt_center_x(int, u8, u8);
u8 dl_chara_profile_card(s8);
u8 dl_chara_added_screen(void);
int dl_chara_list(void);
int dl_pet_by_codes(u8, u8);
void func_25055CE(void);

#endif
