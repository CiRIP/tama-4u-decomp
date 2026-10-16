/* The struct types several headers use (function parameters, shared variables) and the
 * constants several files use. */
#ifndef STRUCTS_H
#define STRUCTS_H

#include "common.h"

struct GfxObject {
    const u8 *image;
    u16 x;
    s16 y;
    u8 width;
    u8 height;
    u8 frame;
    u8 pad0b[0x1];
    u8 palette_base;
    u8 palette_base_hi;
    u16 flags;
    u16 pixel_offset;
    u8 pad12[0x2];
};

struct FlashBase {
    u8 pad000[0x51];
    u8 x051;
    u8 pad052[0x6];
    u32 x058;
};

/* Flash sector 13 (0x2050000..0x205FFFF) is a scratch sector, erased by each user before
 * it writes: the touch card content download (spot_reader -> spot_content_browser), the friend
 * portrait staging of 0xE00 + 0x80 bytes (friend_book_update, comm_make_random_friend, consumed by
 * friend_portrait_store / comm_store_pending_friend) and the picture set of our character rendered for the
 * message editor (friend_page via chara_render_pose_set). This struct is the picture-set view:
 * u16 BE picture count, then u16 BE length of picture 0 at 2. See notes/findings/cards.md. */
struct PictureSetHdr {
    u8 pad000[0x2];
    u8 x002;
    u8 x003;
};

struct PictureHeader {
    u8 pad000[0x2];
    u8 colours;
};

struct PicHeader3 {
    u8 width;
    u8 height;
    u8 colours;
};

struct PicHeaderDigits {
    u8 width;
    u8 height;
};

struct PicHeaderGlyphs {
    u8 width;
    u8 height;
};

struct PicHeaderBubble {
    u8 width;
    u8 height;
};

struct PicHeaderBedroomBaby {
    u8 width;
    u8 height;
};

struct PictureSize {
    u8 width;
    u8 height;
};

struct SpriteSet {
    u32 pose0;
    u32 pose1;
    u32 pose2;
    u32 pose3;
    u32 pose4;
    u32 pose5;
    u32 pose6;
    u32 pose7;
    u32 pose8;
    u32 pose9;
    u32 pose10;
    u32 pose11;
    u32 pose12;
    u32 pose13;
    u32 pose14;
};

/* A list menu (ui_menu_run_plain): count entries shown, fn(i) = the entry callback of the
 * menu driver (2 = hidden), then the entries {text, action} (some end with {0, 0}). */
/* A pose list (chara_pose_sets ...): count, then per picture its width, height and pose
 * code (low nibble the pose, 0x10 flipped, 0x80 without the worn clothes). */
struct PoseEntry {
    u8 w, h, pose;
};
struct PoseSet {
    u8 count;
    struct PoseEntry entries[1];
};

struct MenuTable {
    u32 count;
    int (*fn)(int);
    struct {
        const u8 *text;
        u8 (*fn)(void);
    } entries[];
};

struct ArrowPosA {
    u8 x;
    u8 y;
};

struct ArrowPosB {
    u8 x;
    u8 y;
};

struct PlaceEntry {
    const u8 *name;
    int (*x004)(void);
    u8 (*x008)(void);
    u8 (*x00c)(void);
    void (*x010)(void);
};

struct BankListEntry {
    const u8 *label;
    u32 bank;
    u8 flags;
};

struct TxRecordView20 {
    u8 x000;
    u8 pad001[0xf];
    u8 x010;
};

struct RxPacketView404 {
    u8 x000;
    u8 pad001[0x1];
    u8 x002;
};

struct ExchangeRecord {
    u8 pad000[0x5f];
    u8 gender;
    u8 name[0x12];          /* the owner name */
    u16 owner_id;
    u8 pad074[0x10];
    u16 version;
    u8 pad086[0x2e];
    u8 wish_list;
    u8 pad0b5[0x2b];
    u16 sect_portrait;
    u16 sect_poses;
    u16 sect_profile;
    u16 sect_item;
    u16 sect_mail;
};

struct OwnerBdayBlock {
    u8 bday_day;
    u8 bday_month;
    u8 life_stage;
    u8 item_counts[0x19];
};

struct MailRecord {
    int friend;
    int kind;
    u8 pad008[0x5c];
};

/* size of a friend portrait record (17 per 64 KB flash sector) */
#define FRIEND_PORTRAIT_SIZE 0xE80

struct XyPair {
    u8 x;
    u8 y;
};

/* An inventory list (item_inv_daily.., F18): x000 its name, x004[] the global item indexes of
 * its items (records item_record_by_index(index)), x008 their count. */
struct Inventory {
    const u8 *name;
    const s8 *indexes;
    u8 count;
};

struct EncounterEntry {
    u16 sprite_id;
    u8 first_line;
    u8 line_count;
};

struct DiaryDraft {
    u8 pad000[0x32];
    u16 event;
    u8 variant;
    u8 pad035[0x1];
    u8 state;
    u8 pad037[0x1];
    u8 stored;
    u8 pad039[0x3];
    u8 store_result;
};

struct AlbumSortEntry {
    u32 key;
    u8 index;
};

struct GiftPoints {
    u8 pad000[0xcf];
    u8 points_mode;
    u32 points;
};

struct OwnerProfile {
    u8 gender;
    u8 mood;
    u8 age;
    u8 sign;
};

/*
 * A TAMAGO item file (the downloadable items, games, outings and characters; layout
 * checked on the 916 files of the download pack, notes/findings/tamago.md). All its
 * multi-byte fields are BIG-endian: 0x00 "TAMAGO\0", 0x06 file name UTF-16BE, 0x30 size
 * of the .jpg container, 0x34 ASCII id ("t4u_gh01067_1"), 0x48 u32 total size, 0x4C/0x4D
 * 01 01, 0x4E type (0x81 item, 0x94 program / character, 1 legacy), 0x4F category, 0x50
 * kind, 0x51 sub-kind, 0x52..0x59 signature keyed by 0x55, 0x5A u16 item id, 0x5E name
 * (game charset, 0x14 bytes), 0x74 u32 price, 0x79 use animation, 0x7A hunger, 0x7B
 * happiness, 0x94..0x9B 2-bit likings per character class, 0x9C/0x9D identity bytes,
 * 0xAE u32 program entry offset, 0xB2 weight, 0xB8..0xBC trait increments, 0xC0 character
 * list, 0xF4..0xF7 date window, 0xFF flags (0x10); body from 0x200 = u16 count + {u16
 * length, data} blobs; last 2 bytes = additive sum. The F70b exchange records reuse the
 * magic but store 0x48 and the sum little-endian.
 */
struct TamagoFile {
    union {
        u32 x000;
        u16 x000_h;
    };
    u8 pad004[0x48];
    union {
        u32 hdr_word;
        struct {
            u16 x04c_h;
            u8 x04e;
            u8 x04f;
        };
        struct {
            u8 x04c_b;
            u8 x04d;
        };
    };
    u8 kind;
    u8 sub_kind;
    u8 pad052[0x6];
    union {
        u32 id_word;
        struct {
            u16 x058_h;
            u8 x05a;
            u8 x05b;
        };
    };
    u8 reserved0;
    u8 reserved1;
    u8 name;
    u8 pad05f[0x4];
    u8 legacy_value;
    u8 legacy_text;
    u8 pad065[0x7];
    s8 legacy_hunger;
    u8 legacy_happiness;
    u8 pad06e[0x6];
    u8 price_b3;
    u8 price_b2;
    u8 price_b1;
    u8 price_b0;
    u8 pad078[0x1];
    u8 use_anim;
    s8 hunger;
    u8 happiness;
    u8 pad07c[0x22];
    u8 app_params[8];
    u8 pad0a6[0x8];
    u8 entry_b3;
    u8 entry_b2;
    u8 entry_b1;
    u8 entry_b0;
    u8 weight;
    u8 pad0b3[0x1];
    u8 prize_percent[3];
    u8 pad0b7[0x1];
    u8 trait_add[5];
    u8 pad0bd[0x3b];
    u8 code_hi;
    u8 code_lo;
    u8 format_hi;
    u8 format_lo;
    u8 pad0fc[0x3];
    u8 flags;
};

struct CommHeaderItem {
    u8 pad000[0xc8];
    u8 item_type;
    u8 item_category;
    u8 item_kind;
    u8 item_subkind;
    u16 item_id;
    u8 pad0ce[0x1];
    u8 money_flag;
    u32 money_amount;
    u32 item_attr;
    u32 our_points;
};

#endif
