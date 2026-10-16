/*
 * The main loop and main screen: sys_main, the icon bars and cursor, new-screen set-up, the
 * speech bubble widgets, display sleep / wake and power down, idle tick, the care-event
 * dispatch, poke reactions and the NFC idle-touch arming. Also the two LCD controller init
 * sequences called by lcd_init.
 *
 * Code 24088C0..240C616, .rodata from 253C844.
 */
#include "proto/room/room_main.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/chara/chara_draw.h"
#include "proto/pet/pet_bath.h"
#include "proto/food/food_meals.h"
#include "proto/room/room_draw.h"
#include "proto/room/room_window.h"
#include "proto/town/town_places.h"
#include "proto/diary/diary_notebook.h"
#include "proto/pet/pet_toilet.h"
#include "proto/rtc/rtc_clock_screen.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/evo/evo_counters.h"
#include "proto/evo/evo_special.h"
#include "proto/sys/sys_low_power.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/util/util_empty.h"
#include "proto/ui/ui_message.h"
#include "proto/pet/pet_gone.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/ir/ir_receive.h"
#include "proto/ui/ui_marks.h"
#include "proto/ui/ui_menu.h"
#include "proto/town/town_street.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/sys/sys_startup.h"
#include "proto/pet/pet_care_room.h"
#include "proto/pet/pet_care.h"
#include "proto/spot/spot_bingo_menu.h"
#include "proto/owner/owner_profile.h"
#include "proto/ui/ui_settings.h"
#include "proto/pet/pet_see_off.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/comm/comm_outcome.h"
#include "proto/comm/comm_checks.h"
#include "proto/nfc/nfc_session.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_reader.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * Main screen: light the "letter/mail" icon (ui_set_mark(5, ...)) if there is pending
 * mail (mail_find(-1, 8, ...) >= 0), unless a special mode is active.
 */
void ui_update_mail_mark(void)
{
    u8 f;
    int a;

    if (room_clock_notice != 0)
        return;
    if (visit_partner.mode == 2)
        return;
    if (job.kind != 0)
        return;
    f = outing.destination;
    if (f != 0)
        return;
    a = f;
    if (mail_find(-1, 8, (void *)a, -1, f) >= 0)
        a = 1;
    if (a == 0)
        ui_set_mark(5, 0);
    else
        ui_set_mark(5, 1);
}

/*
 * Main screen: show the attention ("call") icon in the bottom bar (theme x0C5).
 */
void ui_show_attention_icon(void)
{
    if (save.pet.icon_theme <= 3) {
        gfx_object_set_image(ui_call_icon_obj, 0x10c, 0, ui_call_icon_pics[save.pet.icon_theme]);
        func_00009A6();
        ui_attention_icon_state = 2;
    }
}

/*
 * F21 = the system layer: the standard room scene, display power, the idle tick, LCD
 * init, event polling helpers and the main program sys_main (asm).
 * Conventions used all over the firmware:
 *   btn_a_presses/1421/1422 = button A/B/C pressed (set by the ISRs, cleared by the consumer),
 *   ui_timeout = menu inactivity countdown in seconds (RTC tick), tmr_flags = timer flags,
 *   sys_display_state = display asleep, room_view = "a sub-screen is active" kind, anim_busy = 1
 *   while an animation/menu owns the screen (the RTC tick then defers events),
 *   pet = the character state record, pet.life_stage = life stage (0 egg, 1 baby, 2 child,
 *   3 adult, 4 special form, 5 a downloaded character's stage byte),
 *   pet.status = status flags (sleeping/sick/away...), x00a = care-miss bits.
 * Sprite engine: gfx_object_create(x, y, c, flip, flags, image) adds a sprite (48 slots in
 * gfx_objects, count gfx_object_count; y is in screen rows, the status bar occupies the top 0x1c);
 * c = palette offset/layer; gfx_object_set_image(id, c, flip, image) re-points a slot;
 * gfx_object_move_dirty(id, x, y) moves it; func_00009A6() flushes the dirty rectangles to the LCD.
 *
 * Main screen: hide the care-miss / attention icon: clear sprite ui_call_icon_obj (0x10c = its
 * layer), present, icon state ui_attention_icon_state = 1.
 */
void ui_call_icon_hide(void)
{
    gfx_object_set_image(ui_call_icon_obj, 0x10c, 0, 0);
    func_00009A6();
    ui_attention_icon_state = 1;
}

/*
 * Idle housekeeping, called every idle tick (from sys_idle_housekeeping): keep the care-miss
 * attention icon in sync with pet.calls bits 0-3 (the pending call bits:
 * hungry/unhappy/...) and the icon state ui_attention_icon_state: hide it when the bits clear, show it
 * (ui_show_attention_icon, blinking) when a bit is set and the character is not an egg.
 */
void ui_call_icon_sync(void)
{
    if ((save.pet.calls & 0xf) == 0 && ui_attention_icon_state == 2)
        ui_call_icon_hide();
    if ((save.pet.calls & 0xf) != 0 && ui_attention_icon_state == 1 && save.pet.life_stage != 0)
        ui_show_attention_icon();
}

const u8 D_253C848[] = _("おなかがすいてきました!");
const u8 D_253C864[] = _("ハラペコだぜ★");
const u8 D_253C874[] = _("おなかがすきました");
const u8 D_253C888[] = _("ごはんはまだかな?");
const u8 D_253C89C[] = _("おなかすいたんだぞう☼");
const u8 D_253C8B4[] = _("ごはんのじかんだっち!!");
const u8 D_253C8D0[] = _("オレ､ハラペコだよ★");
const u8 D_253C8E8[] = _("おなかがすいたでござる@");
const u8 D_253C904[] = _("おなかすいたのぽ～!");
const u8 D_253C91C[] = _("ごはんのじかんでしょ!!");
const u8 D_253C938[] = _("ぼく､ごはんたべたいよ!");
const u8 D_253C954[] = _("おなかペコペコだころ↓");
const u8 D_253C96C[] = _("ハラがへったぜ★");
const u8 D_253C980[] = _("さあ★ごはんのじかんです");
const u8 D_253C99C[] = _("ごはんのじかんだね★");
const u8 D_253C9B4[] = _("おなかすいちゃった♥");
const u8 D_253C9CC[] = _("ミー､おなかすいたよ♪");
const u8 D_253C9E4[] = _("おなかすいたよ～!!");
const u8 D_253C9FC[] = _("おなかすいたです～!");
const u8 D_253CA14[] = _("ごはんのじかんよ!");
const u8 D_253CA28[] = _("ごはんたべたい♥ぺろ");
const u8 D_253CA40[] = _("ごはんにしたいじょよ★");
const u8 D_253CA58[] = _("おなかがすきましたわ★");
const u8 D_253CA70[] = _("おなかペコペコ★ぴち");
const u8 D_253CA88[] = _("ごはんのじかんだよ♥");
const u8 D_253CAA0[] = _("ゴーゴー★ごはんたべよー");
const u8 D_253CABC[] = _("あげ?ごはんのじかんよ♥");
const u8 D_253CAD8[] = _("おなかすいたわた～");
const u8 D_253CAEC[] = _("ハラペコなちゅ♪");
const u8 D_253CB00[] = _("ごはんのじかんだわさ★");
const u8 D_253CB18[] = _("ごはんタイムです♥");
const u8 D_253CB2C[] = _("ごはんのじかんだわ!!");
const u8 D_253CB44[] = _("ごはんはまだでしょうか?");
const u8 D_253CB60[] = _("ごはんにしようぜ★");
const u8 D_253CB74[] = _("だっち～ごはんだっち～!");
const u8 D_253CB90[] = _("いざ!ごはんでござる");
const u8 D_253CBA8[] = _("おなかすいたし～!!");
const u8 D_253CBC0[] = _("ごはんにしませんか?");
const u8 D_253CBD8[] = _("ごはんプリ～ズ★");
const u8 D_253CBEC[] = _("ごはんよろぴく～★");
const u8 D_253CC00[] = _("おなかすいたですワンッ!");
const u8 D_253CC1C[] = _("ごはんのじかんデビ!");
const u8 D_253CC34[] = _("ハラペコですわよ～★");
const u8 D_253CC4C[] = _("おなかがすいたわ♥");
const u8 D_253CC60[] = _("ごはんたべたいだわさ～");
const u8 D_253CC78[] = _("めしだーーーー!!");
const u8 D_253CC8C[] = _("おなかがすいたな");
const u8 D_253CCA0[] = _("しょくじにしませんか?!");
const u8 D_253CCBC[] = _("おなかすいたなー★");
const u8 D_253CCD0[] = _("おなかすいただっぺ↓");
const u8 D_253CCE8[] = _("おいしいものたべたーい");
const u8 D_253CD00[] = _("おなかすいたぴょん");
const u8 D_253CD14[] = _("おなかがすきました♥");
const u8 D_253CD2C[] = _("もも､おなかすいたのね");
const u8 D_253CD44[] = _("おしょくじはまだですの?");
const u8 D_253CD60[] = _("おしょくじまだかしらっ?");
const u8 D_253CD7C[] = _("おなかペコペコなう");
const u8 D_253CD90[] = _("ハラペコだっちYO!");
const u8 D_253CDA8[] = _("ゴハンだぜ!でござる★");
const u8 D_253CDC0[] = _("おなかすいたです～");
const u8 D_253CDD4[] = _("ミー､ハングリーだよ…↓");
const u8 D_253CDF0[] = _("ズバリおなかすいたし!");
const u8 D_253CE08[] = _("たべものがよんでいる…");
const u8 D_253CE20[] = _("おなかがペコペコですな～");
const u8 D_253CE3C[] = _("おしょくじのじかんですわ");
const u8 D_253CE58[] = _("おなかすいたな♥うふふ");
const u8 D_253CE70[] = _("おなかすきましたわた～");
const u8 D_253CE88[] = _("ごはんのじかんだよー!");
const u8 D_253CEA0[] = _("さあ､ごはんタイムよ!");
const u8 D_253CEB8[] = _("ピポピポ!(ごはん!)");
const u8 D_253CED0[] = _("おなかがすいたでヤンス!");
const u8 D_253CEEC[] = _("ごはんまだー?");
const u8 D_253CEFC[] = _("ごはんはまだですか?");
const u8 D_253CF14[] = _("おやつがたべたいです!");
const u8 D_253CF2C[] = _("たまカフェにいこうぜ!");
const u8 D_253CF44[] = _("おやつがたべたいですな");
const u8 D_253CF5C[] = _("おやつがたべたいよ");
const u8 D_253CF70[] = _("おやつがたべたいぞう");
const u8 D_253CF88[] = _("おやつがたべたいだっち!");
const u8 D_253CFA4[] = _("たまカフェいこうよ!");
const u8 D_253CFBC[] = _("おやつがたべたいでござる");
const u8 D_253CFD8[] = _("おやつがたぺたいのぽ");
const u8 D_253CFF0[] = _("おやつがたべたいでしょ!");
const u8 D_253D00C[] = _("ぼく､おやつがたべたい!");
const u8 D_253D028[] = _("おやつがたべたいころ!");
const u8 D_253D040[] = _("おやつがたべたいぜ!");
const u8 D_253D058[] = _("おやつがたべたいかな★");
const u8 D_253D070[] = _("たまカフェいきたーい♥");
const u8 D_253D088[] = _("スイーツたべたいな♪");
const u8 D_253D0A0[] = _("おやつをよろぴく～★");
const u8 D_253D0B8[] = _("おやつがたべたいです～!");
const u8 D_253D0D4[] = _("おやつがたべたいわ!");
const u8 D_253D0EC[] = _("おやつがたべたいぺろ!");
const u8 D_253D104[] = _("おやつがたべたいじょよ!");
const u8 D_253D120[] = _("おやつがたべたいですわ!");
const u8 D_253D13C[] = _("おやつがたべたいぴち!");
const u8 D_253D154[] = _("おやつがたべたいな★");
const u8 D_253D16C[] = _("たまカフェにゴーゴー!");
const u8 D_253D184[] = _("おやつがたべたいね♥");
const u8 D_253D19C[] = _("おやつがたべたいわた～");
const u8 D_253D1B4[] = _("おやつがたべたいなちゅ");
const u8 D_253D1CC[] = _("おやつがたべたいだわさ!");
const u8 D_253D1E8[] = _("おやつがたべたいです♥");
const u8 D_253D200[] = _("おやつがたべたいのですが");
const u8 D_253D21C[] = _("おやつにしようぜ★");
const u8 D_253D230[] = _("だっち～おやつだっち～!");
const u8 D_253D24C[] = _("いざ!おやつでござる");
const u8 D_253D264[] = _("おやつ､まだなわけ?");
const u8 D_253D27C[] = _("スイーツタイムだよ～♪");
const u8 D_253D294[] = _("おやつにしてちょんまげ!");
const u8 D_253D2B0[] = _("おやつたべたいですワンッ");
const u8 D_253D2CC[] = _("おやつがたべたいデビ!");
const u8 D_253D2E4[] = _("おやつはまだですの!!");
const u8 D_253D2FC[] = _("おやつがたべたいわ♥");
const u8 D_253D314[] = _("おやつにしようだわさ!");
const u8 D_253D32C[] = _("おやつだーーーー!!");
const u8 D_253D344[] = _("おやつのじかんさ");
const u8 D_253D358[] = _("ティータイムですよね?!");
const u8 D_253D374[] = _("おやつたべたいなー★");
const u8 D_253D38C[] = _("おやつたぺたいだっぺ");
const u8 D_253D3A4[] = _("スイーツたべたーい♥");
const u8 D_253D3BC[] = _("おやつがたべたいぴょん");
const u8 D_253D3D4[] = _("もも､おやつたべたいのね");
const u8 D_253D3F0[] = _("ティータイムですわね!");
const u8 D_253D408[] = _("スイーツタイムですわっ");
const u8 D_253D420[] = _("おやっ､おやつのじかん?");
const u8 D_253D43C[] = _("おやつタイムだっちYO!");
const u8 D_253D458[] = _("おやつだぜ!でござる★");
const u8 D_253D470[] = _("おやつのじかんです～");
const u8 D_253D488[] = _("スイーツプリーズ♪");
const u8 D_253D49C[] = _("ズバリおやつタイムだし!");
const u8 D_253D4B8[] = _("ティータイムがきたようだ");
const u8 D_253D4D4[] = _("おやつがたべごろですな～");
const u8 D_253D4F0[] = _("おやつのじかんですわ♥");
const u8 D_253D508[] = _("おやつのじかん♥うふふ");
const u8 D_253D520[] = _("おちゃにしませんかわた?");
const u8 D_253D53C[] = _("おやつのじかんだよー!");
const u8 D_253D554[] = _("さあ､おやつタイムよ!");
const u8 D_253D56C[] = _("ピポピポパ!(おやつ!)");
const u8 D_253D588[] = _("おやつがたべたいでヤンス");
const u8 D_253D5A4[] = _("うち､おやつたべたいな!");
const u8 D_253D5C0[] = _("あそびませんか?");
const u8 D_253D5D4[] = _("あそぼうぜ!");
const u8 D_253D5E4[] = _("あそびたいですな");
const u8 D_253D5F8[] = _("あそばないかい?");
const u8 D_253D60C[] = _("あそびたいんだぞう");
const u8 D_253D620[] = _("あそぼうだっち!");
const u8 D_253D634[] = _("あそぼうよ!");
const u8 D_253D644[] = _("あそぼうでござる!");
const u8 D_253D658[] = _("あそびたいのぽ!");
const u8 D_253D66C[] = _("あそびたいでしょ!");
const u8 D_253D680[] = _("ぼくあそびたいんだ");
const u8 D_253D694[] = _("あそびたいころ!");
const u8 D_253D6A8[] = _("いまからあそぶぞ!");
const u8 D_253D6BC[] = _("いっぱいあそびましょう!");
const u8 D_253D6D8[] = _("ぼくとあそぼうよ!");
const u8 D_253D6EC[] = _("あそぼうよ♥");
const u8 D_253D6FC[] = _("ミーとあそぼうよ♪");
const u8 D_253D710[] = _("あそびにレッツらゴー!");
const u8 D_253D728[] = _("あそびたいです～!");
const u8 D_253D73C[] = _("あそびましょ!");
const u8 D_253D74C[] = _("あそびたいぺろ♥");
const u8 D_253D760[] = _("あそびたいじょよ!");
const u8 D_253D774[] = _("たいくつですわ～");
const u8 D_253D788[] = _("あそぶぴち★");
const u8 D_253D798[] = _("あそぼうよ♪");
const u8 D_253D7A8[] = _("ゴーゴー!あそぼうよ!");
const u8 D_253D7C0[] = _("あげ?あそばない?");
const u8 D_253D7D4[] = _("あそびたいわた～");
const u8 D_253D7E8[] = _("あそびたいなちゅ!");
const u8 D_253D7FC[] = _("あそぶだわさ!");
const u8 D_253D80C[] = _("あそびましょう♥");
const u8 D_253D820[] = _("あそびたいわ!");
const u8 D_253D830[] = _("あそんでいただけませんか");
const u8 D_253D84C[] = _("オレとあそぼうぜ!");
const u8 D_253D860[] = _("だっち～あそぼうだっち～");
const u8 D_253D87C[] = _("いざ､あそぼうでござる!");
const u8 D_253D898[] = _("あそぶんでしょ!!");
const u8 D_253D8AC[] = _("あそびませんか♥");
const u8 D_253D8C0[] = _("ミーとあそんでほしいな♪");
const u8 D_253D8DC[] = _("たいくつだよ～↓↓");
const u8 D_253D8F0[] = _("あそびたいですワンッ!");
const u8 D_253D908[] = _("あそぼうデビ～!");
const u8 D_253D91C[] = _("たいくつでしてよ!!");
const u8 D_253D934[] = _("あそびましょ★");
const u8 D_253D944[] = _("あそぼうだわさ♪");
const u8 D_253D958[] = _("たいくつだーーーー!!");
const u8 D_253D970[] = _("あそびませんか?!");
const u8 D_253D984[] = _("あそびたいなー★");
const u8 D_253D998[] = _("あそびたいだっぺ!");
const u8 D_253D9AC[] = _("おもしろいものなーい?");
const u8 D_253D9C4[] = _("あそぶぴょん♪");
const u8 D_253D9D4[] = _("ももね､あそびたいのね…");
const u8 D_253D9F0[] = _("あそんであげますわっ");
const u8 D_253DA08[] = _("☼ヒマわりむすめ☼だよ～");
const u8 D_253DA24[] = _("あそぼうだっちYO★");
const u8 D_253DA3C[] = _("あそぼうぜ!でござる★");
const u8 D_253DA54[] = _("あそびたいです～♥");
const u8 D_253DA68[] = _("レッツプレイ♪");
const u8 D_253DA78[] = _("ズバリあそびたいでしょ!");
const u8 D_253DA94[] = _("ひまだ…");
const u8 D_253DAA0[] = _("たいくつですな～");
const u8 D_253DAB4[] = _("あそびませんこと?");
const u8 D_253DAC8[] = _("あそぼうよ♥うふふ");
const u8 D_253DADC[] = _("あそぼうわた～");
const u8 D_253DAEC[] = _("あそびのじかんだよー!");
const u8 D_253DB04[] = _("さあ､いっしょにあそぼ!");
const u8 D_253DB20[] = _("ピポ↓(あそんでくれ↓)");
const u8 D_253DB3C[] = _("あそびたいでヤンス!");
const u8 D_253DB54[] = _("うちとあそばない?");
const u8 D_253DB68[] = _("うちとあそびましょう♥");
const u8 D_253DB80[] = _("おしゃれしたいですね!");
const u8 D_253DB98[] = _("おしゃれしたいぜ…★");
const u8 D_253DBB0[] = _("みだしなみはだいじですな");
const u8 D_253DBCC[] = _("おしゃれはいろいろだよ♪");
const u8 D_253DBE8[] = _("おしゃれしたいんだぞう");
const u8 D_253DC00[] = _("おしゃれしたいだっち☼");
const u8 D_253DC18[] = _("おしゃれしたいな★");
const u8 D_253DC2C[] = _("おしゃれしたいでござる!");
const u8 D_253DC48[] = _("おしゃれしたいのぽ!");
const u8 D_253DC60[] = _("カッコよくキメたいでしょ");
const u8 D_253DC7C[] = _("ぼく､おしゃれしたいな");
const u8 D_253DC94[] = _("おしゃれしたいころ!");
const u8 D_253DCAC[] = _("なにかみにつけたいぜ!");
const u8 D_253DCC4[] = _("おしゃれしたいです!");
const u8 D_253DCDC[] = _("おしゃれをきにしなきゃね");
const u8 D_253DCF8[] = _("おしゃれがしたいの♪");
const u8 D_253DD10[] = _("ミー､おしゃれしたいな♪");
const u8 D_253DD2C[] = _("おしゃれしてチョベリグ♥");
const u8 D_253DD48[] = _("おしゃれがしたいです～");
const u8 D_253DD60[] = _("おしゃれがしたいわ!");
const u8 D_253DD78[] = _("おしゃれしたいぺろ♥");
const u8 D_253DD90[] = _("おしゃれしたいじょよ♥");
const u8 D_253DDA8[] = _("おしゃれしたいですわ～♥");
const u8 D_253DDC4[] = _("おしゃれしたいぴち!");
const u8 D_253DDDC[] = _("おしゃれしたいな～♥");
const u8 D_253DDF4[] = _("おしゃれしよ!ゴーゴー!");
const u8 D_253DE10[] = _("おしゃれしたいんだけど…");
const u8 D_253DE2C[] = _("おしゃれしたいわた～");
const u8 D_253DE44[] = _("おしゃれしたいなちゅ");
const u8 D_253DE5C[] = _("おしゃれしたいだわさ!");
const u8 D_253DE74[] = _("おしゃれしたいです♥");
const u8 D_253DE8C[] = _("おしゃれしたいわ!");
const u8 D_253DEA0[] = _("おげんきですか～★");
const u8 D_253DEB4[] = _("ロックだぜ★イエーイ!!");
const u8 D_253DED0[] = _("ボクこわくないだっちよ～");
const u8 D_253DEEC[] = _("ほれなおしたでござる?");
const u8 D_253DF04[] = _("おれはおうさまだし★");
const u8 D_253DF1C[] = _("ドレスすてきですよね?");
const u8 D_253DF34[] = _("ミー､きものラブ♥だよ～");
const u8 D_253DF50[] = _("たまもりレッツらゴー★");
const u8 D_253DF68[] = _("ハッピーですワンッ♥");
const u8 D_253DF80[] = _("いたずらしちゃうデビ～★");
const u8 D_253DF9C[] = _("おーほっほっほっ!!");
const u8 D_253DFB4[] = _("わーい!フワフワよ～♥");
const u8 D_253DFCC[] = _("インタビューするだわさ?");
const u8 D_253DFE8[] = _("おしゃれしたいぞ～!");
const u8 D_253E000[] = _("おしゃれがしたいな!");
const u8 D_253E018[] = _("おしゃれしたいのですが…");
const u8 D_253E034[] = _("おしゃれさせてよ!");
const u8 D_253E048[] = _("おしゃれするだっぺ");
const u8 D_253E05C[] = _("おしゃれにキメたいわ");
const u8 D_253E074[] = _("おしゃれしたいぴょん!");
const u8 D_253E08C[] = _("おしゃれしたいです★");
const u8 D_253E0A4[] = _("もも､おしゃれしたいのね");
const u8 D_253E0C0[] = _("おしゃれがしたいですわ");
const u8 D_253E0D8[] = _("おしゃれしたいですわっ!");
const u8 D_253E0F4[] = _("あたしのでばん､まだ～?");
const u8 D_253E110[] = _("カモンおどるだっちYO");
const u8 D_253E128[] = _("うたおうぜ!でござる★");
const u8 D_253E140[] = _("ずきんがおきにいりです～");
const u8 D_253E15C[] = _("ミーとトークしようよ♪");
const u8 D_253E174[] = _("しんじつはひとつでしょ!");
const u8 D_253E190[] = _("しずかだな…");
const u8 D_253E1A0[] = _("わらいはたいせつですな～");
const u8 D_253E1BC[] = _("もっとめだちたいですわ～");
const u8 D_253E1D8[] = _("ハチミツだいすき♥うふふ");
const u8 D_253E1F4[] = _("このきもの､すてきわた～");
const u8 D_253E210[] = _("やあ､げんきかな?");
const u8 D_253E224[] = _("あくはゆるさないわ!");
const u8 D_253E23C[] = _("ピポ!(おしゃれしたい)");
const u8 D_253E258[] = _("おしゃれしたいでヤンス!");
const u8 D_253E274[] = _("うち､おしゃれしたいな♥");
const u8 D_253E290[] = _("ラブはどこにあるかしら?");
const u8 D_253E2AC[] = _("ほんとうは､こんな\nふうにおわかれした\nくなかったよ…こんどあえた\nら､もっといっしょにいれる\nといいな…またねバイバイ…");
const u8 *const D_253E328[] = {
    D_253C848, D_253C864, D_253C874, D_253C888, D_253C89C, D_253C8B4,
    D_253C8D0, D_253C8E8, D_253C904, D_253C91C, D_253C938, D_253C954,
    D_253C96C, D_253C980, D_253C99C, D_253C9B4, D_253C9CC, D_253C9E4,
    D_253C9FC, D_253CA14, D_253CA28, D_253CA40, D_253CA58, D_253CA70,
    D_253CA88, D_253CAA0, D_253CABC, D_253CAD8, D_253CAEC, D_253CB00,
    D_253CB18, D_253CB2C, D_253CB44, D_253CB60, D_253CB74, D_253CB90,
    D_253CBA8, D_253CBC0, D_253CBD8, D_253CBEC, D_253CC00, D_253CC1C,
    D_253CC34, D_253CC4C, D_253CC60, D_253CC78, D_253CC8C, D_253CCA0,
    D_253CCBC, D_253CCD0, D_253CCE8, D_253CD00, D_253CD14, D_253CD2C,
    D_253CD44, D_253CD60, D_253CD7C, D_253CD90, D_253CDA8, D_253CDC0,
    D_253CDD4, D_253CDF0, D_253CE08, D_253CE20, D_253CE3C, D_253CE58,
    D_253CE70, D_253CE88, D_253CEA0, D_253CEB8, D_253CED0, D_253CEEC,
    D_253CEFC, D_253CF14, D_253CF2C, D_253CF44, D_253CF5C, D_253CF70,
    D_253CF88, D_253CFA4, D_253CFBC, D_253CFD8, D_253CFF0, D_253D00C,
    D_253D028, D_253D040, D_253CF14, D_253D058, D_253D070, D_253D088,
    D_253D0A0, D_253D0B8, D_253D0D4, D_253D0EC, D_253D104, D_253D120,
    D_253D13C, D_253D154, D_253D16C, D_253D184, D_253D19C, D_253D1B4,
    D_253D1CC, D_253D1E8, D_253D0D4, D_253D200, D_253D21C, D_253D230,
    D_253D24C, D_253D264, D_253D1E8, D_253D27C, D_253D294, D_253D2B0,
    D_253D2CC, D_253D2E4, D_253D2FC, D_253D314, D_253D32C, D_253D344,
    D_253D358, D_253D374, D_253D38C, D_253D3A4, D_253D3BC, D_253D1E8,
    D_253D3D4, D_253D3F0, D_253D408, D_253D420, D_253D43C, D_253D458,
    D_253D470, D_253D488, D_253D49C, D_253D4B8, D_253D4D4, D_253D4F0,
    D_253D508, D_253D520, D_253D53C, D_253D554, D_253D56C, D_253D588,
    D_253D5A4, D_253D1E8, D_253D5C0, D_253D5D4, D_253D5E4, D_253D5F8,
    D_253D60C, D_253D620, D_253D634, D_253D644, D_253D658, D_253D66C,
    D_253D680, D_253D694, D_253D6A8, D_253D6BC, D_253D6D8, D_253D6EC,
    D_253D6FC, D_253D710, D_253D728, D_253D73C, D_253D74C, D_253D760,
    D_253D774, D_253D788, D_253D798, D_253D7A8, D_253D7C0, D_253D7D4,
    D_253D7E8, D_253D7FC, D_253D80C, D_253D820, D_253D830, D_253D84C,
    D_253D860, D_253D87C, D_253D898, D_253D8AC, D_253D8C0, D_253D8DC,
    D_253D8F0, D_253D908, D_253D91C, D_253D934, D_253D944, D_253D958,
    D_253D634, D_253D970, D_253D984, D_253D998, D_253D9AC, D_253D9C4,
    D_253D5C0, D_253D9D4, D_253D5C0, D_253D9F0, D_253DA08, D_253DA24,
    D_253DA3C, D_253DA54, D_253DA68, D_253DA78, D_253DA94, D_253DAA0,
    D_253DAB4, D_253DAC8, D_253DADC, D_253DAEC, D_253DB04, D_253DB20,
    D_253DB3C, D_253DB54, D_253DB68, D_253DB80, D_253DB98, D_253DBB0,
    D_253DBCC, D_253DBE8, D_253DC00, D_253DC18, D_253DC2C, D_253DC48,
    D_253DC60, D_253DC7C, D_253DC94, D_253DCAC, D_253DCC4, D_253DCDC,
    D_253DCF8, D_253DD10, D_253DD2C, D_253DD48, D_253DD60, D_253DD78,
    D_253DD90, D_253DDA8, D_253DDC4, D_253DDDC, D_253DDF4, D_253DE10,
    D_253DE2C, D_253DE44, D_253DE5C, D_253DE74, D_253DE8C, D_253DEA0,
    D_253DEB4, D_253DED0, D_253DEEC, D_253DF04, D_253DF1C, D_253DF34,
    D_253DF50, D_253DF68, D_253DF80, D_253DF9C, D_253DFB4, D_253DFCC,
    D_253DFE8, D_253E000, D_253E018, D_253E034, D_253E048, D_253E05C,
    D_253E074, D_253E08C, D_253E0A4, D_253E0C0, D_253E0D8, D_253E0F4,
    D_253E110, D_253E128, D_253E140, D_253E15C, D_253E174, D_253E190,
    D_253E1A0, D_253E1BC, D_253E1D8, D_253E1F4, D_253E210, D_253E224,
    D_253E23C, D_253E258, D_253E274, D_253E290, D_253E2AC
};
extern u8 D_217CB04[];
extern u8 D_217CC40[];
extern u8 D_217CD7C[];
extern u8 D_217D26C[];
const u8 *const room_egg_pics[] = { D_217CB04, D_217CC40, D_217CD7C, D_217D26C };
/* the room animations (struct RoomStep in globals.h) follow */
/* the second sprite of an animation (through room_scene_objs): shown if x000, picture x001,
 * flags x002, position x003/x004 */
struct RoomObjStep {
    u8 kind;
    u8 pic;
    u8 flags;
    u8 x;
    u8 y;
};
const struct RoomStep room_scene_egg[] = {
    { 0x03, 0x00, 68, 56 },
    { 0x00, 0x10, 63, 56 },
    { 0x02, 0x10, 51, 56 },
    { 0x00, 0x30, 44, 56 },
    { 0x03, 0x20, 31, 56 },
    { 0x00, 0x20, 23, 56 },
    { 0x02, 0x00, 18, 55 },
    { 0x01, 0x00, 15, 55 },
    { 0x03, 0x00, 12, 55 },
    { 0x03, 0x00, 24, 39 },
    { 0x03, 0x00, 34, 56 },
    { 0x03, 0x00, 51, 18 },
    { 0x03, 0x00, 68, 56 },
    { 0x03, 0x00, 95, 10 },
    { 0x03, 0x10, 117, 57 },
    { 0x03, 0x00, 74, 43 },
    { 0x03, 0x00, 53, 55 },
    { 0x03, 0x00, 30, 46 },
    { 0x03, 0x00, 12, 55 },
    { 0x01, 0x00, 15, 55 },
    { 0x02, 0x00, 18, 55 },
    { 0x00, 0x20, 23, 56 },
    { 0x03, 0x20, 31, 56 },
    { 0x00, 0x30, 44, 56 },
    { 0x02, 0x10, 51, 56 },
    { 0x00, 0x10, 63, 56 },
    { 0x03, 0x00, 68, 56 },
    { 0x01, 0x00, 72, 56 },
    { 0x02, 0x00, 78, 56 },
    { 0x01, 0x20, 86, 56 },
    { 0x03, 0x20, 93, 56 },
    { 0x01, 0x30, 103, 56 },
    { 0x02, 0x10, 109, 56 },
    { 0x01, 0x10, 115, 56 },
    { 0x03, 0x00, 118, 56 },
    { 0x01, 0x10, 115, 56 },
    { 0x02, 0x10, 109, 56 },
    { 0x01, 0x30, 103, 56 },
    { 0x03, 0x20, 93, 56 },
    { 0x01, 0x20, 86, 56 },
    { 0x02, 0x00, 78, 56 },
    { 0x01, 0x00, 72, 56 },
    { 0xff, 0x00, 0, 0 }
};
const u32 room_egg_step_ms[] = {
    500, 200, 300, 200, 400, 200, 300, 200, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 200, 300, 200, 500, 200,
    300, 200, 500, 200, 300, 200, 400, 200, 300, 200, 500, 200,
    300, 200, 400, 200, 300, 200, 500
};
const struct RoomStep room_scene_call_child[] = {
    { 0x00, 0x00, 95, 50 },
    { 0x00, 0x00, 95, 50 },
    { 0x02, 0x00, 95, 50 },
    { 0x02, 0x00, 95, 50 },
    { 0x02, 0x00, 94, 50 },
    { 0x02, 0x00, 94, 50 },
    { 0xff, 0x00, 56, 80 }
};
const struct RoomStep room_scene_call_adult[] = {
    { 0x0d, 0x00, 98, 54 },
    { 0x0d, 0x00, 98, 54 },
    { 0x0d, 0x00, 98, 54 },
    { 0x0e, 0x00, 98, 54 },
    { 0x0d, 0x00, 98, 53 },
    { 0x0e, 0x00, 98, 54 },
    { 0x0d, 0x00, 98, 53 },
    { 0x0e, 0x00, 98, 54 },
    { 0x0d, 0x00, 98, 53 },
    { 0x0e, 0x00, 98, 54 },
    { 0xff, 0x00, 56, 80 }
};
const struct RoomObjStep room_scene_walk_objs[] = {
    { 0, 3, 0x10, 15, 44 },
    { 1, 3, 0x10, 15, 44 },
    { 1, 3, 0x10, 15, 45 },
    { 0, 6, 0x10, 15, 44 },
    { 0, 5, 0x20, 33, 20 },
    { 0, 0, 0x10, 64, 44 },
    { 0, 3, 0x10, 65, 44 },
    { 1, 2, 0x10, 75, 44 },
    { 1, 5, 0x20, 85, 28 },
    { 1, 0, 0x10, 95, 44 },
    { 1, 3, 0x10, 105, 44 },
    { 1, 3, 0x10, 113, 44 },
    { 1, 3, 0x00, 113, 44 },
    { 1, 0, 0x00, 113, 44 },
    { 0, 6, 0x10, 113, 44 },
    { 1, 2, 0x10, 107, 12 },
    { 1, 5, 0x20, 87, 21 },
    { 1, 6, 0x10, 77, 12 },
    { 0, 0, 0x10, 64, 44 },
    { 0, 3, 0x00, 55, 44 },
    { 1, 3, 0x00, 35, 44 },
    { 1, 3, 0x00, 15, 44 },
    { 0xff, 0xff, 0x00, 48, 64 }
};
const struct RoomStep room_scene_walk[] = {
    { 0x00, 0x00, 64, 50 },
    { 0x00, 0x00, 64, 50 },
    { 0x00, 0x00, 64, 51 },
    { 0x00, 0x00, 48, 50 },
    { 0x00, 0x00, 32, 50 },
    { 0x00, 0x00, 32, 51 },
    { 0x00, 0x00, 31, 50 },
    { 0x00, 0x00, 16, 50 },
    { 0x00, 0x00, 16, 51 },
    { 0x00, 0x00, 16, 50 },
    { 0x00, 0x10, 16, 50 },
    { 0x00, 0x10, 32, 50 },
    { 0x08, 0x10, 48, 50 },
    { 0x02, 0x10, 64, 28 },
    { 0x08, 0x10, 80, 50 },
    { 0x00, 0x10, 80, 50 },
    { 0x00, 0x10, 80, 51 },
    { 0x00, 0x00, 96, 50 },
    { 0x00, 0x00, 96, 51 },
    { 0x00, 0x00, 96, 50 },
    { 0x00, 0x00, 80, 50 },
    { 0x00, 0x00, 80, 51 },
    { 0xff, 0x00, 48, 64 }
};
const struct RoomStep room_scene_unused_a[] = {
    { 0x03, 0x00, 64, 50 },
    { 0x03, 0x00, 64, 51 },
    { 0xff, 0x00, 48, 64 }
};
const struct RoomStep D_253EB00[] = {
    { 0x00, 0x00, 64, 50 },
    { 0x0a, 0x00, 64, 50 },
    { 0xff, 0x00, 48, 64 }
};
const u8 *const room_arrow_pics[] = { D_214DF50, D_214DFE4 };
const struct RoomStep D_253EB1C[] = {
    { 0x00, 0x00, 57, 59 },
    { 0x01, 0x00, 41, 58 }
};
const struct RoomStep room_scene_request[] = {
    { 0x09, 0x00, 64, 50 },
    { 0x09, 0x10, 63, 50 },
    { 0xff, 0x00, 56, 80 }
};
const struct RoomStep room_scene_baby_sick[] = {
    { 0x03, 0x00, 64, 50 },
    { 0x03, 0x00, 64, 51 },
    { 0xff, 0x00, 48, 64 }
};
const struct RoomStep room_sick_icon_pos_baby[] = {
    { 0x00, 0x00, 84, 29 },
    { 0x00, 0x00, 84, 28 }
};
const struct RoomStep room_scene_neglected[] = {
    { 0x03, 0x00, 63, 50 },
    { 0x03, 0x00, 65, 50 },
    { 0xff, 0x00, 48, 64 }
};
const struct RoomStep room_neglect_icon_pos[] = {
    { 0x00, 0x00, 82, 29 },
    { 0x00, 0x00, 83, 28 }
};
const struct RoomStep room_scene_baby_asleep[] = {
    { 0x0b, 0x00, 56, 47 },
    { 0x0b, 0x00, 56, 47 },
    { 0x0b, 0x00, 57, 47 },
    { 0x0b, 0x00, 57, 47 },
    { 0xff, 0x00, 44, 56 }
};
const struct RoomStep room_scene_unused_b[] = {
    { 0x03, 0x00, 64, 50 },
    { 0x03, 0x00, 65, 50 },
    { 0xff, 0x00, 49, 64 }
};
const struct RoomStep room_scene_collapsed[] = {
    { 0x03, 0x00, 76, 50 },
    { 0x03, 0x00, 66, 49 },
    { 0x03, 0x00, 48, 50 },
    { 0x03, 0x00, 35, 50 },
    { 0x03, 0x00, 24, 50 },
    { 0x04, 0x00, 24, 50 },
    { 0x04, 0x00, 24, 43 },
    { 0x03, 0x00, 24, 50 },
    { 0x03, 0x10, 24, 49 },
    { 0x03, 0x10, 33, 50 },
    { 0x03, 0x10, 51, 49 },
    { 0x03, 0x10, 71, 50 },
    { 0x03, 0x10, 92, 50 },
    { 0x04, 0x10, 92, 50 },
    { 0x04, 0x10, 92, 30 },
    { 0x03, 0x10, 92, 50 },
    { 0x03, 0x00, 92, 50 },
    { 0x03, 0x00, 92, 50 },
    { 0xff, 0x00, 52, 60 }
};
const struct RoomStep room_scene_collapsed_path[] = {
    { 0x00, 0x00, 116, 4 },
    { 0x00, 0x00, 106, 5 },
    { 0x00, 0x00, 88, 5 },
    { 0x00, 0x00, 75, 5 },
    { 0x00, 0x00, 64, 7 },
    { 0x00, 0x00, 102, 64 },
    { 0x00, 0x00, 101, 56 },
    { 0x00, 0x10, 97, 55 },
    { 0x00, 0x00, -49, 55 },
    { 0x00, 0x10, -8, 7 },
    { 0x00, 0x10, 9, 5 },
    { 0x00, 0x10, 30, 4 },
    { 0x00, 0x10, 52, 6 },
    { 0x00, 0x00, 133, 60 },
    { 0x00, 0x00, 173, 52 },
    { 0x00, 0x00, 19, 56 },
    { 0x00, 0x10, 168, 58 },
    { 0x00, 0x00, 134, 6 },
    { 0xff, 0x00, 52, 60 }
};
const u8 D_253EC98[][2] = {
    { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 },
    { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 },
    { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }
};
const struct RoomStep room_scene_sick[] = {
    { 0x03, 0x00, 64, 50 },
    { 0x03, 0x00, 64, 51 },
    { 0xff, 0x00, 48, 64 }
};
const struct RoomStep room_sick_icon_pos[] = {
    { 0x00, 0x00, 83, 28 },
    { 0x00, 0x00, 83, 27 }
};
const struct RoomStep room_scene_departing[] = {
    { 0x03, 0x00, 64, 50 },
    { 0x03, 0x00, 65, 50 },
    { 0xff, 0x00, 48, 64 }
};
const struct RoomStep room_depart_icon_pos[] = {
    { 0x00, 0x00, 42, 35 },
    { 0x00, 0x10, 42, 35 }
};
const struct RoomStep room_scene_asleep[] = {
    { 0x0b, 0x00, 62, 43 },
    { 0x0b, 0x00, 62, 43 },
    { 0x0b, 0x00, 63, 43 },
    { 0x0b, 0x00, 63, 43 },
    { 0xff, 0x00, 46, 49 }
};
const struct RoomStep room_scene_arrows[] = {
    { 0x02, 0x00, 64, 50 },
    { 0x02, 0x10, 50, 50 },
    { 0x02, 0x00, 36, 50 },
    { 0x02, 0x10, 36, 50 },
    { 0x02, 0x00, 50, 50 },
    { 0x02, 0x10, 64, 50 },
    { 0x02, 0x00, 78, 50 },
    { 0x02, 0x10, 94, 50 },
    { 0x02, 0x00, 92, 50 },
    { 0x02, 0x10, 78, 50 },
    { 0xff, 0x00, 48, 64 }
};
const struct XyPair ui_arrow1_positions[] = {
    { 0x20, 0x38 }, { 0x40, 0x38 }, { 0x00, 0x38 }, { 0x30, 0x38 }, { 0x10, 0x38 },
    { 0x50, 0x38 }, { 0x30, 0x38 }, { 0x70, 0x38 }, { 0x40, 0x38 }, { 0x60, 0x38 }
};
const struct XyPair ui_arrow2_positions[] = {
    { 0x50, 0x48 }, { 0x10, 0x44 }, { 0x30, 0x48 }, { 0x00, 0x44 }, { 0x40, 0x48 },
    { 0x20, 0x44 }, { 0x60, 0x48 }, { 0x40, 0x44 }, { 0x70, 0x48 }, { 0x30, 0x44 }
};
const struct RoomStep room_scene_unused_c[] = {
    { 0x03, 0x00, 64, 50 },
    { 0x03, 0x00, 64, 50 },
    { 0x03, 0x00, 65, 50 },
    { 0x03, 0x00, 65, 50 },
    { 0xff, 0x00, 48, 64 }
};
const struct RoomStep room_scene_leaving[] = {
    { 0x03, 0x00, 76, 50 },
    { 0x03, 0x00, 66, 49 },
    { 0x03, 0x00, 48, 50 },
    { 0x03, 0x00, 35, 49 },
    { 0x03, 0x00, 24, 50 },
    { 0x04, 0x00, 24, 50 },
    { 0x03, 0x00, 24, 50 },
    { 0x03, 0x10, 24, 49 },
    { 0x03, 0x10, 33, 50 },
    { 0x03, 0x10, 51, 49 },
    { 0x03, 0x10, 71, 50 },
    { 0x03, 0x10, 92, 49 },
    { 0x04, 0x10, 92, 50 },
    { 0x03, 0x10, 92, 49 },
    { 0x03, 0x00, 92, 50 },
    { 0x03, 0x00, 92, 49 },
    { 0xff, 0x00, 60, 64 }
};
const struct RoomStep D_253EE10[] = {
    { 0x03, 0x10, 101, 50 },
    { 0x03, 0x10, 91, 50 },
    { 0x03, 0x10, 73, 50 },
    { 0x03, 0x10, 60, 50 },
    { 0x03, 0x10, 49, 50 },
    { 0x05, 0x10, 49, 50 },
    { 0x03, 0x10, 49, 50 },
    { 0x03, 0x00, 49, 50 },
    { 0x03, 0x00, 58, 50 },
    { 0x03, 0x00, 76, 50 },
    { 0x03, 0x00, 96, 50 },
    { 0x03, 0x00, 117, 50 },
    { 0x05, 0x00, 117, 50 },
    { 0x03, 0x00, 117, 50 },
    { 0x03, 0x10, 117, 50 },
    { 0x03, 0x10, 117, 50 },
    { 0xff, 0x00, 60, 64 }
};
const u8 D_253EE78[][2] = {
    { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 },
    { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 },
    { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x40, 0x36 }, { 0x00, 0x00 }, { 0x00, 0x00 },
    { 0x00, 0x00 }, { 0x00, 0x00 }
};

/* The strings above (D_253C848..D_253E290) are the per-character "speech bubble" lines
 * for the four needs: hungry (ごはん), wants a snack (おやつ), wants to play (あそぶ),
 * wants to dress up (おしゃれ); each character type has its own phrasing/verbal tic. */

/*
 * Main screen: move the icon cursor ui_icon_cursor_obj to the selected icon ui_main_icon (1..5 top
 * bar x = 2/27/52/77/102, 6..9 bottom bar y = 100) and show its highlighted picture from
 * the ui_icon1_pics.. tables indexed by pet.icon_theme (UI theme / room variant); 0 = no
 * cursor (hidden).
 */
void ui_icon_cursor_draw(void)
{
    switch (ui_main_icon) {
    case 1:
        gfx_object_move_dirty(ui_icon_cursor_obj, 2, 0);
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, ui_icon1_pics[save.pet.icon_theme]);
        break;
    case 2:
        gfx_object_move_dirty(ui_icon_cursor_obj, 0x1b, 0);
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, ui_icon2_pics[save.pet.icon_theme]);
        break;
    case 3:
        gfx_object_move_dirty(ui_icon_cursor_obj, 0x34, 0);
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, ui_icon3_pics[save.pet.icon_theme]);
        break;
    case 4:
        gfx_object_move_dirty(ui_icon_cursor_obj, 0x4d, 0);
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, ui_icon4_pics[save.pet.icon_theme]);
        break;
    case 5:
        gfx_object_move_dirty(ui_icon_cursor_obj, 0x66, 0);
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, ui_icon5_pics[save.pet.icon_theme]);
        break;
    case 6:
        gfx_object_move_dirty(ui_icon_cursor_obj, 2, 0x64);
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, ui_icon6_pics[save.pet.icon_theme]);
        break;
    case 7:
        gfx_object_move_dirty(ui_icon_cursor_obj, 0x1b, 0x64);
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, ui_icon7_pics[save.pet.icon_theme]);
        break;
    case 8:
        gfx_object_move_dirty(ui_icon_cursor_obj, 0x34, 0x64);
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, ui_icon8_pics[save.pet.icon_theme]);
        break;
    case 9:
        gfx_object_move_dirty(ui_icon_cursor_obj, 0x4d, 0x64);
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, ui_icon9_pics[save.pet.icon_theme]);
        break;
    default:
        gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, 0);
        break;
    }
}

/*
 * Main screen icon bars (sprites 0..3): top bar picture ui_top_bar_pics[theme] at y 0 and bottom
 * bar ui_bottom_bar_pics at y 100 (layers 0x117 / 0x120, theme = pet.icon_theme), the cursor
 * object ui_icon_cursor_obj, the care-miss / attention icon ui_call_icon_obj (shown if a call is pending,
 * x00a & 0xf), then the status bar and the per-icon state marks (ui_create_marks, the cursor
 * ui_icon_cursor_draw, ui_update_mail_mark, the menu icons ui_draw_marks). Objects use flag 0x4000
 * (icon-bar layer). The icon-bar theme index pet.icon_theme (= pet.icon_theme, chosen in the
 * settings) is clamped to 5.
 */
void ui_draw_icon_bars(void)
{
    if (save.pet.icon_theme > 5)
        save.pet.icon_theme = 5;
    gfx_object_create(0, 0, 0x117, 0, 0x4000, (const u8 *)ui_top_bar_pics[save.pet.icon_theme]);
    gfx_object_create(0, 100, 0x120, 0, 0x4000, (const u8 *)ui_bottom_bar_pics[save.pet.icon_theme]);
    ui_icon_cursor_obj = gfx_object_create(2, 0, 0x100, 0, 0x4000, 0);
    if (save.pet.calls & 0xf) {
        ui_attention_icon_state = 2;
        ui_call_icon_obj = gfx_object_create(0x66, 100, 0x10c, 0, 0x4000, (const u8 *)ui_call_icon_pics[save.pet.icon_theme]);
    } else {
        ui_attention_icon_state = 1;
        ui_call_icon_obj = gfx_object_create(0x66, 100, 0x10c, 0, 0x4000, 0);
    }
    ui_create_marks();
    ui_icon_cursor_draw();
    ui_update_mail_mark();
    ui_draw_marks();
}

/*
 * "New screen": reset the sprite list and dirty rectangles (gfx_display_reset) and rebuild the
 * icon bars (ui_draw_icon_bars). Every menu/sub-screen starts with this.
 */
void ui_new_screen(void)
{
    gfx_display_reset();
    ui_draw_icon_bars();
}

/*
 * Main screen: allocate the three name/speech-bubble sprites ui_bubble_frame_obj (bubble frame,
 * layer 0xc9) and ui_bubble_line1_obj/D_000400F (text lines, layer 0xc5): centred around the
 * egg/baby (life stage <= 2, small bubble D_216359C) or at the top for older stages.
 * ui_bubble_fill fills them, ui_bubble_hide clears them.
 */
void ui_bubble_alloc(void)
{
    if (save.pet.life_stage <= 2) {
        ui_bubble_frame_obj = gfx_object_create(0x37 - (D_216359C[0] >> 1), 0x37 - (D_216359C[1] >> 1), 0xc9, 0, 0, 0);
        ui_bubble_line1_obj = gfx_object_create(0x41 - (D_216359C[0] >> 1), 0x3c - (D_216359C[1] >> 1), 0xc5, 0, 0, 0);
        ui_bubble_line2_obj = gfx_object_create(0x41 - (D_216359C[0] >> 1), 0x3c - (D_216359C[1] >> 1), 0xc5, 0, 0, 0);
    } else {
        ui_bubble_frame_obj = gfx_object_create(0, 0x1c, 0xc9, 0, 0, 0);
        ui_bubble_line1_obj = gfx_object_create(10, 0x20, 0xc5, 0, 0, 0);
        ui_bubble_line2_obj = gfx_object_create(10, 0x2d, 0xc5, 0, 0, 0);
        ui_bubble_line_idx = 0;
    }
}

/*
 * Next entry of a cyclic phrase table t (index ui_bubble_line_idx, restarts at a null entry).
 */
struct XyPair *ui_bubble_next_line(struct XyPair *t)
{
    struct XyPair *p = &t[ui_bubble_line_idx];

    if (p->x == 0) {
        ui_bubble_line_idx = 0;
        p = &t[ui_bubble_line_idx];
    }
    ui_bubble_line_idx++;
    return p;
}

/*
 * Main screen: fill the speech bubble: babies/children (life stage <= 2) show their name
 * pet.owner_name; older stages say a line chosen by the current sprite set pet.idle_pose (the
 * idle pose, see F30; <= 0x48 / 0xDA / 0x123 -> character text table 13 / 12 / 14 via
 * chara_text(character id, table, 0) and the rotating line index ui_bubble_next_line).
 */
void ui_bubble_fill(void)
{
    const u8 *img;

    if (save.pet.life_stage <= 2) {
        gfx_object_set_image(ui_bubble_frame_obj, 0xc9, 0, D_216359C);
        gfx_draw_text_line(ui_bubble_line1_obj, 0x41 - (D_216359C[0] >> 1), 0x3c - (D_216359C[1] >> 1), 0xc5, save.pet.owner_name, 1, 8);
    } else {
        if (save.pet.idle_pose > 0x124)
            pet_choose_adult_pose();
        gfx_object_set_image(ui_bubble_frame_obj, 0xc9, 0, D_216287C);
        if (save.pet.idle_pose <= 0x48)
            img = ui_bubble_next_line(chara_text(save.pet.x000, 13, 0));
        else if (save.pet.idle_pose <= 0xda)
            img = ui_bubble_next_line(chara_text(save.pet.x000, 12, 0));
        else if (save.pet.idle_pose <= 0x123)
            img = ui_bubble_next_line(chara_text(save.pet.x000, 14, 0));
        gfx_draw_text_line(ui_bubble_line1_obj, 2, 0x20, 0xc5, img, 1, 14);
    }
}

/*
 * Main screen: hide the speech bubble.
 */
void ui_bubble_hide(void)
{
    gfx_object_set_image(ui_bubble_frame_obj, 0xc9, 0, 0);
    gfx_object_set_image(ui_bubble_line1_obj, 0xc5, 0, 0);
    gfx_object_set_image(ui_bubble_line2_obj, 0xc5, 0, 0);
}

/*
 * Two-choice cursor pair: create the display objects ui_choice_cursor_l_obj/D_0004011 at x 0x20 /
 * 0x50, y 0x50 with the picture D_214DEC0, the second one mirrored with flag 0x10;
 * ui_choice_cursors_bob(flag) nudges them by one pixel (blink animation).
 */
void ui_choice_cursors_create(void)
{

    ui_choice_cursor_l_obj = gfx_object_create(0x20, 0x50, 0xc1, 0, 0, (const u8 *)&D_214DEC0);
    ui_choice_cursor_r_obj = gfx_object_create(0x50, 0x50, 0xc1, 0, 0, 0);
    gfx_object_set_flags(ui_choice_cursor_r_obj, 0x10);
    gfx_object_set_image(ui_choice_cursor_r_obj, 0xc1, 0, D_214DEC0);
}

/*
 * The two scrolling arrow sprites: create ui_arrow1_obj/D_0004013 with pictures
 * D_214DF50/D_214DFE4 at the positions of ui_arrow1_positions/D_253ED74 entry 0 (ui_arrows_move moves
 * them).
 */
void ui_arrows_create(void)
{
    ui_arrow1_obj = gfx_object_create(ui_arrow1_positions[0].x, ui_arrow1_positions[0].y, 0xe0, 0, 0, (const u8 *)&D_214DF50);
    ui_arrow2_obj = gfx_object_create(ui_arrow2_positions[0].x, ui_arrow2_positions[0].y, 0xe8, 0, 0, (const u8 *)&D_214DFE4);
}

/*
 * Move the two objects of ui_arrows_create to the positions of entry i.
 */
void ui_arrows_move(int i)
{
    const struct XyPair *p;
    const struct XyPair *q;
    u8 a;

    a = ui_arrow1_obj;
    p = &ui_arrow1_positions[i];
    gfx_object_move_dirty(a, p->x, p->y);
    a = ui_arrow2_obj;
    q = &ui_arrow2_positions[i];
    gfx_object_move_dirty(a, q->x, q->y);
}

/*
 * Shift the two objects of ui_choice_cursors_create one pixel (flag) - a small bobbing animation.
 */
void ui_choice_cursors_bob(int flag)
{
    if (flag) {
        gfx_object_move_dirty(ui_choice_cursor_l_obj, 0x1f, 0x50);
        gfx_object_move_dirty(ui_choice_cursor_r_obj, 0x4f, 0x50);
    } else {
        gfx_object_move_dirty(ui_choice_cursor_l_obj, 0x20, 0x50);
        gfx_object_move_dirty(ui_choice_cursor_r_obj, 0x50, 0x50);
    }
}

/*
 * May the fourth clover leaf ("まいにちおせわ", daily care: pet.clover_leaves bit 3, granted
 * by pet_fill_clover_leaf(8, 0)) be given now? Requires: no status flag (not sick / leaving),
 * awake (pet_sleep_state != 3), not with the sitter (pet_sitter_state), age counter
 * pet.age_minutes > 5820, no sub-screen, and bit 3 still clear; returns 1 when
 * pet_fill_clover_leaf gave it.
 */
u8 pet_try_daily_care_leaf(void)
{
    if (save.pet.status != 0)
        goto fail;
    if (pet_sleep_state() == 3) {
    fail:
        return 0;
    }
    if (pet_sitter_state.active != 0 || save.pet.age_minutes <= 5820 || room_view != 0 || (save.pet.clover_leaves & 8))
        goto fail2;
    if (pet_fill_clover_leaf(8, 0))
        return 1;
fail2:
    return 0;
}

/*
 * Display off / "display sleep": backlight PWM to 0 (sys_set_backlight sets the level
 * sys_backlight_request, sys_apply_backlight applies it to timer 2 = TM2 on P12 = backlight), room_idle = 1,
 * LCD controller sleep-in (command 0x10 SLPIN: booster, oscillator and panel scan stop,
 * the screen goes blank; the frame memory and the display-on state are kept), lcd_asleep = 1
 * (panel asleep), and unless sys_battery_empty (set only by the battery-empty screen
 * sys_power_down, which runs its own final sleep loop) switch the CPU to the 32 kHz clock and
 * sleep (sys_sleep_osc1). The same command for both controllers; no DISPOFF (28) is ever
 * sent, and there is no 120 ms wait after SLPIN (the datasheets ask for one before the next
 * command). Called by main's auto screen-off, the "pet has gone" screen (pet_gone_screen),
 * F11, F34, rtc_clock_setting and the test mode.
 */
void sys_display_off(void)
{
    sys_set_backlight(0);
    sys_apply_backlight();
    room_idle = 1;
    *(volatile u8 *)0x600000 = 0x10;
    lcd_asleep = 1;
    if (sys_battery_empty == 0)
        sys_sleep_osc1();
}

/*
 * Wake the display (called with a = 1 by the button ISRs, also by the RTC tick and the
 * menus). First sys_idle_seconds (seconds since the last activity, counted up by the RTC
 * tick, which turns the screen off past 30) = 0x14 when sys_battery_empty (the
 * battery-empty screen, sys_power_down), else 0. Only acts when sys_display_state (0 awake,
 * 1 sleeping, 2 = screen-off requested) is set: LCD sleep-out (0x11 SLPOUT; the picture comes back from the
 * kept frame memory; no redraw or re-initialisation here) if the panel was asleep
 * (lcd_asleep); with
 * sys_battery_empty it is awake at once with the backlight PWM on T16 ch.2 restored at a fixed
 * duty (0x1A of period 0x35, MCLK/4, PWM out); otherwise back to the 30 MHz OSC3 clock
 * (sys_sleep_osc3) and, when a != 0, fully awake: sys_display_state = 0, auto-sleep countdown
 * sys_awake_seconds = 0x4AF (= 20 min minus one second), backlight level sys_backlight_level; a == 0 stays
 * in the "asleep" state with sys_idle_seconds = 0x19 (the screen turns off again about 6 s
 * later).
 */
void sys_display_wake(int a)
{
    if (sys_battery_empty != 0)
        sys_idle_seconds = 0x14;
    else
        sys_idle_seconds = 0;
    if (sys_display_state == 0)
        return;
    if (lcd_asleep != 0) {
        REG8(0x600000) = 0x11;
        lcd_asleep = 0;
    }
    if (sys_battery_empty != 0) {
        sys_display_state = 0;
        T16_CR2A = 0x1a;
        T16_CR2B = 0x35;
        T16_CLKCTL_2 = 10;
        T16_CTL2 = 0x25;
        return;
    }
    sys_sleep_osc3();
    if (a != 0) {
        sys_display_state = 0;
        sys_awake_seconds = sys_battery_empty ? 0 : 0x4af;
        sys_set_backlight(sys_backlight_level);
    } else {
        sys_display_state = 1;
        sys_idle_seconds = 0x19;
    }
}

/*
 * sys_idle_tick (asm) = the idle tick used by every wait loop: `halt` (sleep until the
 * next interrupt: RTC second, timer, button, NFC) then the idle housekeeping sys_idle_housekeeping
 * (P06 / button-hold sampling sys_sample_debug_strap, backlight PWM update sys_apply_backlight, care icon
 * ui_call_icon_sync).
 */
void sys_idle_tick(void)
{
    asm volatile("halt");
    sys_idle_housekeeping();
}

/*
 * Unused (no caller): older copy of the init sequence lcd_init runs inline for the
 * Ilitek ILI9163(C)-type controller (P07 strap low; the ST7735 one is lcd_init_st7735). LCD
 * bus: 0x600000 = command, 0x600001 = data; a halfword store writes both (low byte =
 * command, high byte = first parameter). 01 SWRESET, 20 ms, C0 03 00 PWCTR1, C2 05 PWCTR3,
 * C5 C4 VMCTR1, C6 13 VMCTR2 (as in the live copy), 36 08 MADCTL (no mirroring, BGR order;
 * the live copy uses 36 C0), 11 SLPOUT, 120 ms, E0/E1 gamma with 15 bytes each in the
 * ILI9163C layout (VP63..VP0 / VN0..VN63; bytes 7..9 = 7-bit VP43, packed VP27|VP36
 * nibbles 0xB7, 7-bit VP20, values above 0x3F that the ST7735's 6-bit gamma fields
 * cannot take), 29 DISPON. No COLMOD: the reset format, 18 bits per pixel, is used.
 */
void lcd_init_ili9163_old(void)
{
    REG8(0x600000) = 0x01;
    tmr_delay_ms(20);
    REG16(0x600000) = 0x03c0;
    REG8(0x600001) = 0x00;
    REG16(0x600000) = 0x05c2;
    REG16(0x600000) = 0xc4c5;
    REG16(0x600000) = 0x13c6;
    REG16(0x600000) = 0x0836;
    REG8(0x600000) = 0x11;
    tmr_delay_ms(120);
    REG8(0x600000) = 0xe0;
    REG8(0x600001) = 0x3f;
    REG8(0x600001) = 0x22;
    REG8(0x600001) = 0x20;
    REG8(0x600001) = 0x30;
    REG8(0x600001) = 0x29;
    REG8(0x600001) = 0x0c;
    REG8(0x600001) = 0x4e;
    REG8(0x600001) = 0xb7;
    REG8(0x600001) = 0x3c;
    REG8(0x600001) = 0x19;
    REG8(0x600001) = 0x22;
    REG8(0x600001) = 0x1e;
    REG8(0x600001) = 0x02;
    REG8(0x600001) = 0x01;
    REG8(0x600001) = 0x00;
    REG8(0x600000) = 0xe1;
    REG8(0x600001) = 0x00;
    REG8(0x600001) = 0x1b;
    REG8(0x600001) = 0x1f;
    REG8(0x600001) = 0x0f;
    REG8(0x600001) = 0x16;
    REG8(0x600001) = 0x13;
    REG8(0x600001) = 0x31;
    REG8(0x600001) = 0x84;
    REG8(0x600001) = 0x43;
    REG8(0x600001) = 0x06;
    REG8(0x600001) = 0x1d;
    REG8(0x600001) = 0x21;
    REG8(0x600001) = 0x3d;
    REG8(0x600001) = 0x3e;
    REG8(0x600001) = 0x3f;
    REG8(0x600000) = 0x29;
}

/*
 * Sitronix ST7735 init sequence (P07 strap high, lcd_init); names and values from the
 * ST7735 datasheet (ST7735P3 v0.1, section 10):
 *   01 SWRESET, 20 ms (the datasheet asks for 120 ms); 11 SLPOUT, 120 ms;
 *   B1 01 2C 2D FRMCTR1 (normal mode): RTNA = 1, front porch 44, back porch 45 lines;
 *      frame rate = fosc / ((2 * RTNA + 40) * (lines + 44 + 45 + 2));
 *   B2 01 2C 2D FRMCTR2 (idle mode), B3 01 2C 2D 01 2C 2D FRMCTR3 (partial mode, dot and
 *      column inversion): the same timing;
 *   B4 00 INVCTR: dot inversion in all modes (reset value 07 = column inversion);
 *   C0 A2 02 84 PWCTR1: AVDD 5.0 V, GVDD 4.6 V, GVCL -4.6 V, third byte 84 = reset value;
 *   C1 C5 PWCTR2: VGH25 2.4 V, VGL -10 V, VGH = 3 * AVDD - 0.5 V;
 *   C2 0A 00 PWCTR3 (normal mode): op-amp currents small / medium-low, booster clocks
 *      DCA = 0 (the reset values);
 *   C3 8A 2A PWCTR4 (idle mode), C4 8A EE PWCTR5 (partial mode): same currents, booster
 *      clocks 0x22A / 0x2EE;
 *   C5 0E VMCTR1: VCOMS = 14 -> VCOM -0.775 V;
 *   36 08 MADCTL: no mirroring or exchange, BGR order;
 *   E0 / BuiltinItemKeyEntry GMCTRP1 / GMCTRN1: 16 bytes each (VRF0, VOS0, PK0..PK9, SELV0, SELV1, SELV62,
 *      SELV63), all 6-bit;
 *   29 DISPON.
 * No COLMOD (reset value 18 bits per pixel) and no window: the flush routines address
 * columns x + 2 and rows y + 1 (128x128 panel inside the 132-column frame memory).
 */
void lcd_init_st7735(void)
{
    REG8(0x600000) = 0x01;
    tmr_delay_ms(20);
    REG8(0x600000) = 0x11;
    tmr_delay_ms(120);
    REG8(0x600000) = 0xb1;
    REG8(0x600001) = 0x01;
    REG8(0x600001) = 0x2c;
    REG8(0x600001) = 0x2d;
    REG8(0x600000) = 0xb2;
    REG8(0x600001) = 0x01;
    REG8(0x600001) = 0x2c;
    REG8(0x600001) = 0x2d;
    REG8(0x600000) = 0xb3;
    REG8(0x600001) = 0x01;
    REG8(0x600001) = 0x2c;
    REG8(0x600001) = 0x2d;
    REG8(0x600001) = 0x01;
    REG8(0x600001) = 0x2c;
    REG8(0x600001) = 0x2d;
    REG8(0x600000) = 0xb4;
    REG8(0x600001) = 0x00;
    REG8(0x600000) = 0xc0;
    REG8(0x600001) = 0xa2;
    REG8(0x600001) = 0x02;
    REG8(0x600001) = 0x84;
    REG8(0x600000) = 0xc1;
    REG8(0x600001) = 0xc5;
    REG8(0x600000) = 0xc2;
    REG8(0x600001) = 0x0a;
    REG8(0x600001) = 0x00;
    REG8(0x600000) = 0xc3;
    REG8(0x600001) = 0x8a;
    REG8(0x600001) = 0x2a;
    REG8(0x600000) = 0xc4;
    REG8(0x600001) = 0x8a;
    REG8(0x600001) = 0xee;
    REG8(0x600000) = 0xc5;
    REG8(0x600001) = 0x0e;
    REG8(0x600000) = 0x36;
    REG8(0x600001) = 0x08;
    REG8(0x600000) = 0xe0;
    REG8(0x600001) = 0x12;
    REG8(0x600001) = 0x20;
    REG8(0x600001) = 0x10;
    REG8(0x600001) = 0x18;
    REG8(0x600001) = 0x33;
    REG8(0x600001) = 0x2c;
    REG8(0x600001) = 0x25;
    REG8(0x600001) = 0x28;
    REG8(0x600001) = 0x28;
    REG8(0x600001) = 0x27;
    REG8(0x600001) = 0x2f;
    REG8(0x600001) = 0x3c;
    REG8(0x600001) = 0x00;
    REG8(0x600001) = 0x03;
    REG8(0x600001) = 0x03;
    REG8(0x600001) = 0x10;
    REG8(0x600000) = 0xe1;
    REG8(0x600001) = 0x12;
    REG8(0x600001) = 0x20;
    REG8(0x600001) = 0x10;
    REG8(0x600001) = 0x18;
    REG8(0x600001) = 0x2d;
    REG8(0x600001) = 0x28;
    REG8(0x600001) = 0x23;
    REG8(0x600001) = 0x28;
    REG8(0x600001) = 0x28;
    REG8(0x600001) = 0x26;
    REG8(0x600001) = 0x2f;
    REG8(0x600001) = 0x3b;
    REG8(0x600001) = 0x00;
    REG8(0x600001) = 0x03;
    REG8(0x600001) = 0x03;
    REG8(0x600001) = 0x10;
    REG8(0x600000) = 0x29;
}

/*
 * Sound on/off menu ("オン" / "オフ", D_253EEC8/D_253EED0): cursor on snd_enabled (1 = sound
 * on), A moves, B sets it and returns 1, C leaves; 10 s timeout returns -1. flag tracks a
 * C button held at entry (ignore its release). Leaves immediately when the egg hatches
 * (x00a bit 7).
 */
int ui_sound_menu(void)
{
    int r = 0;
    u8 sel;
    u8 w;
    u8 flag;
    u8 v;

    anim_busy = 1;
    ui_timeout = 10;
    sel = 1 - snd_enabled;
    v = P0_P0D;
    flag = (v & 4) == 0;
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_21314E0);
    gfx_draw_text_line(gfx_object_create(0, 0, 0, 0, 0, 0), 0x18, 0x20, 0x20, (int)D_253EEC8, 0, 2);
    gfx_draw_text_line(gfx_object_create(0, 0, 0, 0, 0, 0), 0x18, 0x30, 0x20, (int)D_253EED0, 0, 2);
    w = gfx_object_create(6, sel * 16 + 0x22, 0x30, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses) {
            snd_play(1);
            sel++;
            if (sel > 1)
                sel = 0;
            gfx_object_move_dirty(w, 6, sel * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            snd_wait_end();
            snd_enabled = 1 - sel;
            snd_play(2);
            btn_b_presses = 0;
            r = 1;
            goto out;
        }
        if (btn_c_presses) {
            snd_play(3);
            btn_c_presses = 0;
            if (!flag)
                goto out;
        }
        sys_idle_tick();
        if ((s8)save.pet.calls < 0 && save.pet.life_stage == 0)
            goto out;
        if (flag && (P0_P0D & 4))
            flag = 0;
    } while (ui_timeout);
    r = -1;
out:
    anim_busy = 0;
    return r;
}

/*
 * Main screen while the pet is away (visit_partner.mode == 1, an outing after a
 * communication; outing on an outing; job at a part-time job): the empty room
 * (room_draw_background, room_draw_poops) with a centred sign picture (0x210C82C, or 0x210CF44 for
 * an outing) on the layer just above the last sprite.
 */
void room_draw_away(void)
{
    struct GfxObject *p; u8 n;

    if (room_view != 0)
        return;
    if (visit_partner.mode == 1) {
        ui_new_screen();
        room_draw_background();
        room_draw_poops(0);
        p = &gfx_objects[gfx_object_count - 1];
        n = p->palette_base + p->image[2];
        { const u8 *img = D_210C82C; gfx_object_create(0x40 - (img[0] >> 1), 0x40 - (img[1] >> 1), n, 0, 0, (const u8 *)img); }
    } else if (outing.destination != 0) {
        ui_new_screen();
        room_draw_background();
        room_draw_poops(0);
        p = &gfx_objects[gfx_object_count - 1];
        n = p->palette_base + p->image[2];
        if (outing.destination == 1) {
            { const u8 *img = D_210CF44; gfx_object_create(0x40 - (img[0] >> 1), 0x40 - (img[1] >> 1), n, 0, 0, (const u8 *)img); }
        } else if (outing.destination > 1) {
            { const u8 *img = D_210CF44; gfx_object_create(0x40 - (img[0] >> 1), 0x40 - (img[1] >> 1), n, 0, 0, (const u8 *)img); }
        }
    } else if (job.kind != 0) {
        ui_new_screen();
        room_draw_background();
        room_draw_poops(0);
        p = &gfx_objects[gfx_object_count - 1];
        n = p->palette_base + p->image[2];
        { const u8 *img = D_210C82C; gfx_object_create(0x40 - (img[0] >> 1), 0x40 - (img[1] >> 1), n, 0, 0, (const u8 *)img); }
    }
}

/*
 * Three-line message screen "とけいのじかんが あっているか かくにんしてね!" (please check
 * that the clock time is right; D_253EED8/D_253EEEC/D_253EEFC).
 */
void ui_clock_check_msg(void)
{
    u8 a;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    a = gfx_object_create(0, 0, 0x20, 0, 0, 0);
    gfx_draw_text_centered(a, 0x29, 0x20, D_253EED8, 0, 9);
    a = gfx_object_create(0, 0, 0x24, 0, 0, 0);
    gfx_draw_text_centered(a, 0x39, 0x24, D_253EEEC, 0, 9);
    a = gfx_object_create(0, 0, 0x28, 0, 0, 0);
    gfx_draw_text_centered(a, 0x49, 0x28, D_253EEFC, 0, 9);
}

/*
 * Random idle animation when the pet is selected/poked ("the character does something"):
 * three generic ones (anim_poke_a / 24DAE8A / 24DB208, 33/33/34 %) or, at life stage 4
 * with about 35 %, the character-specific one chosen by the character id pet.x000
 * (2001..2010 = the ten boy characters, 3001..3010 = the ten girl characters; the
 * generated animation functions 24DB59E..24F2CCE).
 */
void pet_poke_animation(void)
{
    u8 n;

    if (save.pet.life_stage <= 3 || save.pet.life_stage == 5) {
        n = rand() % 100;
    } else {
        n = rand() % 100;
        if (n > 0x41)
            goto sw;
        n = rand() % 100;
    }
    if (n <= 0x20)
        anim_poke_a();
    else if (n <= 0x41)
        anim_poke_b();
    else
        anim_poke_c();
    btn_b_presses = 0;
    return;
sw:
    switch (save.pet.x000) {
    case 2001: anim_poke_mamelabotchi(); break;
    case 2002: anim_poke_yanchamametchi(); break;
    case 2003: anim_poke_kuchipatchin(); break;
    case 2004: anim_poke_jeanistkuchipa(); break;
    case 2005: anim_poke_kirarispacy(); break;
    case 2006: anim_poke_kingspacy(); break;
    case 2007: anim_poke_kuromamesenpai(); break;
    case 2008: anim_poke_otakuromame(); break;
    case 2009: anim_poke_oreotonatchi(); break;
    case 2010: anim_poke_astrosoratchi(); break;
    case 3001: anim_poke_gurumemequeen(); break;
    case 3002: anim_poke_memechanobatchi(); break;
    case 3003: anim_poke_mamelovehimespe(); break;
    case 3004: anim_poke_lovelyfire(); break;
    case 3005: anim_poke_pocharabritchi(); break;
    case 3006: anim_poke_oyamerojitchi(); break;
    case 3007: anim_poke_slimneene(); break;
    case 3008: anim_poke_fairypakupaku(); break;
    case 3009: anim_poke_melodymuse(); break;
    case 3010:
    default: anim_poke_petitdevihimespe(); break;
    }
    btn_b_presses = 0;
}

/*
 * Is menu entry b blocked now (returns 1 = blocked; a == 1 also plays the error beep,
 * sound 12)? Never inside a sub-screen (room_view). With visit_partner.mode == 1 everything
 * but entry 5 is blocked, with x014 == 2 only entries 6 and 9; otherwise everything while
 * the pet is away (outing / job).
 */
u8 ui_main_icon_blocked(u8 a, u8 b)
{
    if (room_view != 0)
        return 0;
    if (visit_partner.mode == 1) {
        if (b == 5)
            return 0;
    } else if (visit_partner.mode == 2) {
        if (b != 6 && b != 9)
            return 0;
    } else {
        if (outing.destination == 0 && job.kind == 0)
            goto fail;
    }
    if (a == 1)
        snd_play(12);
    return 1;
fail:
    return 0;
}

/*
 * Is the pet away (outing or job active)?
 */
u8 pet_is_away(void)
{
    u8 r;

    if (outing.destination != 0)
        r = 1;
    else if (job.kind != 0)
        r = 1;
    else
        r = 0;
    return r;
}

/*
 * If the pet is away, run the corresponding "away" screen (outing_screen / job_end).
 */
u8 ui_run_away_screen(void)
{
    if (room_view != 0)
        return 0;
    if (outing.destination != 0) {
        outing_screen(0);
        return 1;
    }
    if (job.kind != 0) {
        job_end(0);
        return 1;
    }
    return 0;
}

/*
 * Main-loop hook when returning to the main screen (sub-screen level 0..2): handle a
 * pending request flagged by the sub-systems: friend_event.kind1_bit / x002 (NFC results ->
 * func_24FC4F2(1 / 0)), the end of the visit_partner.mode mode (x015 == 1: the menu
 * comm_main_menu with the RTC interrupt temporarily reconfigured, 0x300277 = 4 then 6),
 * the outing return (outing.come_home == 1 -> outing_screen) and the job end
 * (job.finished == 1 -> job_end). Returns 1 when something ran.
 */
u8 ui_main_pending_hook(void)
{
    u8 k = room_view;

    if (k != 0 && k != 2 && k != 1)
        return 0;
    if (friend_event.kind1_bit != 0) {
        room_view = 0;
        func_24FC4F2(1);
        return 1;
    }
    if (friend_event.kind2_bit != 0) {
        room_view = 0;
        func_24FC4F2(0);
        return 1;
    }
    if (visit_partner.mode != 0 && visit_partner.over == 1) {
        room_idle = 0;
        INT_EP47_ERTC_EAD = 4;
        room_view = 0;
        if (comm_main_menu() != 0)
            room_view = 0;
        INT_EP47_ERTC_EAD = 6;
        if (ui_timeout == 0)
            ui_main_icon = 0;
        return 1;
    }
    if (outing.destination != 0 && outing.come_home == 1) {
        room_view = 0;
        outing_screen(outing.come_home);
        return 1;
    }
    if (job.kind != 0 && job.finished == 1) {
        room_view = 0;
        job_end(job.finished);
        return 1;
    }
    return 0;
}

/*
 * Per-minute checks of the away modes (job_check_hours, outing_hours_check) and what happens when
 * the pet falls asleep while away (diary_writing_scene, job_clear, outing_end).
 */
void pet_away_minute_check(void)
{
    u8 s;

    job_check_hours();
    if (pet_sleep_state() == 3) {
        if (job.kind != 0) {
            s = room_view;
            room_view = 0;
            diary_writing_scene();
            room_view = s;
        }
        job_clear();
    }
    outing_hours_check();
    if (pet_sleep_state() == 3) {
        if (outing.destination != 0) {
            s = room_view;
            room_view = 0;
            diary_writing_scene();
            room_view = s;
        }
        outing_end();
    }
}

/*
 * Is any special mode active (outing visit_partner.mode, outing, job)?
 */
u8 pet_special_mode_active(void)
{
    u8 r;

    if (visit_partner.mode != 0)
        r = 1;
    else if (outing.destination != 0)
        return 1;
    else if (job.kind != 0)
        r = 1;
    else
        r = 0;
    return r;
}

/*
 * May the NFC front-end be armed / a communication be started now? Not from a deep
 * sub-screen (room_view > 1), not as an egg, not with a status flag set (pet healthy),
 * not while the pet sleeps (pet_sleep_state 3), not while the clock-check notice is up
 * (room_clock_notice) or the pet is with the sitter (pet_sitter_state), and only with the
 * display on (sys_display_state == 0). Used before starting the field detection
 * (nfc_init_if_enabled).
 */
int nfc_may_arm(void)
{
    int k = room_view;

    if (k > 1)
        return 0;
    if (save.pet.life_stage == 0)
        return 0;
    if (save.pet.status != 0)
        return 0;
    if (pet_sleep_state() == 3)
        return 0;
    if (room_clock_notice != 0)
        return 0;
    if (pet_sitter_state.active != 0)
        return 0;
    if (sys_display_state != 0)
        return 0;
    return 1;
}

/*
 * After a communication: when the communication count pet.comm_count has changed since
 * comm_remember_count (or comm_book_notice_mode == 2) and has just reached 5 or 10, play the
 * "こそだてちょうを てにいれたよ!" animation anim_notebook_get / anim_notebook_get2 (the same
 * thresholds as the two character badges of dl_chara_added_screen, > 4 / > 9).
 */
void comm_notebook_unlock_check(void)
{
    u8 s = save.comm_book_notice_mode;

    if (s == 1)
        return;
    if (comm_count_before != save.pet.comm_count || s == 2) {
        if (save.pet.comm_count == 5)
            anim_notebook_get();
        else if (save.pet.comm_count == 10)
            anim_notebook_get2();
    }
}

/*
 * Remember the communication counter pet.comm_count in comm_count_before.
 */
void comm_remember_count(void)
{
    comm_count_before = save.pet.comm_count;
}

/*
 * Idle touch: called from main when the PN512 reports an RF field while the toy sits in
 * the room (not in a menu). Remembers the touch counter (comm_remember_count), checks the
 * preconditions, then runs nfc_idle_session -> nfc_idle_target_session with an automatic session type:
 * init the chip, wait 2 s for an initiator. Another Tamagotchi -> the exchange adopts its
 * mode (comm_exchange, result 3). A phone or reader (0xF0EF) -> result 4, the "touch"
 * event: comm_idle_touch_event picks an event by the day of the month (a gift item game_score_reward,
 * the quiz, a chat line, a visitor...) and counts the touch in pet.comm_count. No data is
 * received on this path. Then comm_notebook_unlock_check (the 5th / 10th communication animations).
 * Returns 1 if it ran.
 * "A reader" is any RF field that does not start a Tamagotchi NFC-DEP exchange within 2 s
 * (nfc_detect_initiator reports 0xF0EF, also when no frame arrives). Nothing identifies the reader
 * and nothing about it is stored, so the toy cannot tell readers apart: every touch
 * counts. pet.comm_count is the general communication count (also counted by every Tama-to-Tama
 * and app session and by the Touch Spot reader, Bingo and Gashapon); the house upgrades
 * come from it (comm_house_level: 5 / 20 / 100 / 1000).
 */
int nfc_idle_touch(void)
{
    comm_remember_count();
    if (nfc_may_arm() == 0)
        return 0;
    comm_menu_active = 1;
    switch (nfc_idle_session()) {
    case 3:
        comm_menu_active = 0;
        break;
    case 4:
        comm_idle_touch_event();
        room_view = 0;
        comm_menu_active = 0;
        break;
    default:
        goto fail;
    }
    comm_notebook_unlock_check();
    return 1;
fail:
    comm_menu_active = 0;
    return 0;
}

/*
 * If the pet is asleep: play the "no" sound, show the sleeping message and return 1.
 */
u8 pet_asleep_refuse(void)
{
    if (pet_sleep_state() != 3)
        return 0;
    snd_play(12);
    ui_message_box(D_253EF10, D_253EF20, D_253EF34, D_253EF44, 8);
    return 1;
}

/*
 * Flush the screen, then (re)arm the NFC front-end (nfc_init_if_enabled).
 */
void nfc_arm_after_draw(void)
{
    func_00009A6();
    nfc_init_if_enabled();
}

/* a room scene step (struct RoomStep) as main() reads it: the picture byte signed (its
 * ld.b and `< 0` end test need s8) */
struct RoomStepSigned { s8 picture; u8 flags; s16 x; s16 y; };

/* the second sprite of a scene (struct RoomObjStep) as main() reads it, picture signed */
struct RoomObjStepSigned { u8 kind; s8 picture; u8 flags; u8 x; u8 y; };






































#define D_253EB1C_w ((struct RoomStepSigned *)D_253EB1C)

#define D_253EC24_w ((struct RoomStepSigned *)room_scene_collapsed_path)

#define D_253ECF0_w ((struct RoomStepSigned *)room_depart_icon_pos)

#define D_253EB50_w ((struct RoomStepSigned *)room_sick_icon_pos_baby)

#define D_253ECD0_w ((struct RoomStepSigned *)room_sick_icon_pos)

#define D_253EB70_w ((struct RoomStepSigned *)room_neglect_icon_pos)




extern struct RoomStepSigned room_scene_sitter[];

extern struct RoomStepSigned room_scene_praise_window[];

extern struct RoomStepSigned room_scene_toilet_pending[];

extern struct RoomStepSigned room_scene_toilet_call[];

extern struct RoomStepSigned room_scene_clean_pending[];

extern struct RoomStepSigned room_scene_clean_call[];

extern struct RoomStepSigned room_scene_bath_pending[];

extern struct RoomStepSigned room_scene_bath_call[];























void evo_evolve_now_twice(void);





#define ROOM (room_scene_current[room_scene_obj_idx])

#define OBJ (room_scene_objs[room_scene_obj_idx])

#define IW(p) (((u8 *)(p))[0] >> 1)

#define IH(p) (((u8 *)(p))[1] >> 1)

/* main()'s switch tables follow these strings (as data while main() is built from assembly) */
const u8 D_253EEC8[] = _("オン");
const u8 D_253EED0[] = _("オフ");
const u8 D_253EED8[] = _("とけいのじかんが");
const u8 D_253EEEC[] = _("あっているか");
const u8 D_253EEFC[] = _("かくにんしてね!");
const u8 D_253EF10[] = _("たまごっちが");
const u8 D_253EF20[] = _("おきているときに");
const u8 D_253EF34[] = _("つかえるよ!");
const u8 D_253EF44[] = _("");

/*
 * sys_main = MAIN (asm, 3584 instructions, never returns; entered from the reset
 * handler). Start: allocates the two sprite-record scratch buffers on the stack
 * (diary_draft/D_0000E94 point at them, 0x40 and 0x50 bytes), clears the event flags
 * (lcd_palette_set, sys_battery_check_due, sys_battery_dead, pet_toilet_expired..3FFC = care-timer expiry flags,
 * pet_cleaning_reminder/C, save_request, room_idle, sys_battery_empty, sys_display_state, sys_debug_fast_forward), draws the boot
 * image / initialises the LCD (lcd_init), sets P0 again (P0 IOC 0xA8: P03, P05, P07
 * become outputs after the LCD init has read the P07 strap; P0 data 0x5F drives P07 low
 * from then on), redoes the
 * sound setup (snd_init), anim_busy = 1, calls sys_idle_housekeeping ten times (ten idle
 * ticks to settle the inputs), enables the timer-4/5 compare-B interrupts (pINT_E16T45
 * 0x300274 = 0x44), sys_awake_seconds = 0x960 (40 min) and runs the power-on sequence
 * sys_startup_flow (wake / clock set / "continue or reset" dialogs; result in room_clock_notice:
 * 1 = a save was continued, so the clock-check notice is shown until the first key, 0 = a
 * new game was set up). Then it
 * decides the initial situation from sys_debug_strap_count (== 8: the P06 debug strap ->
 * room_mood_question_due = 0, else the mood question is armed), the activity state
 * (pet_sleep_state() == 3 = asleep), the life stage pet.life_stage and the status flags
 * pet.status (bits 3 / 4 = the last two steps of the departure ladder: bit 4 ->
 * pet_gone_screen(1), the "pet has gone" screen; bit 3 -> pet_gone_screen(0), the leaving
 * sequence until bit 4 is set; pet.gone_minutes counts the minutes away), and enters the
 * event loop:
 *   - picks the room decoration table room_scene_current (room_scene_egg / room_scene_baby_sick / room_scene_request /
 *     room_scene_collapsed / room_scene_leaving ... = lists of {image, x, y} per situation: egg, baby, sick,
 *     sleeping, away, hungry (room_scene_toilet_call..), ...) and draws it,
 *   - draws the character (pet_get_gender chooses the pose, images 0x217A4B8/0x217A380 for
 *     the egg, 0x214E108.. for the hatched stages),
 *   - waits for events with the idle routine sys_idle_tick (halt + housekeeping): button
 *     presses open the menus (comm_main_menu = the communication menu, spot_bingo_gashapon_menu =
 *     Bingo/Gashapon, pet_habit_learned, pet_sitter_return, anim_christmas_play, anim_owner_birthday /
 *     pet_event_chara_birthday / anim_happy = generated animations), the RTC tick raises the care
 *     requests and events (pet_care_event_dispatch, on a key press: the mood question, the
 *     daily-care clover leaf, the ten-day see-off, and a trained pet answering its own
 *     toilet / 18:00 cleaning / 19:00 bath request; otherwise the owner answers them from
 *     the care menu), the counters sys_idle_seconds/1426 (sys_debug_fast_forward is not a
 *     minute flag but the debug time fast-forward, see btn_c_isr; main shortens its animation
 *     timer while it is set), and the auto screen-off (sys_idle_seconds counts up; past 30
 *     the RTC tick requests sys_display_off).
 * Not fully traced; see notes/fable/NOTES.md section 12.
 */
void sys_main(void)
{
    u8 buf1[0x40];
    u8 buf2[0x50];
    u8 spr_room;
    u8 spr_obj;
    u8 spr_pet;
    int no_greet;
    int pending;
    int first;
    int zero = 0;
    int i;
    int mode;

    memset(buf1, 0, 0x40);
    memset(buf2, 0, 0x50);
    diary_draft = (struct DiaryDraft *)buf1;
    diary_images = (u32)buf2;
    pending = 0;
    lcd_palette_set = 0;
    sys_battery_check_due = 0;
    sys_battery_dead = 0;
    pet_toilet_expired = 0;
    pet_bath_expired = 0;
    pet_cleaning_expired = 0;
    pet_praise_expired = 0;
    pet_cleaning_reminder = 0;
    pet_request_bits = 0;
    save_request = 0;
    friend_event.blocked = 1;
    room_idle = 0;
    sys_battery_empty = 0;
    mode = 0;
    sys_display_state = 0;
    sys_debug_fast_forward = 0;
    lcd_init();
    P0_IOC0 = 0xa8;
    P0_P0D = 0x5f;
    lcd_asleep = 0;
    snd_init();
    anim_busy = 1;
    snd_enabled = 1;
    item_no_full_dialog = 0;
    for (i = 0; i < 10; i++)
        sys_idle_housekeeping();
    snd_enabled = 1;
    sys_backlight_level = 4;
    INT_E16T45 = 0x44;
    sys_awake_seconds = 0x960;
    room_clock_notice = 1;
    room_clock_notice = sys_startup_flow(mode);
    mode = 0;
    no_greet = 0;
    lcd_palette_set = 0;
    if (sys_debug_strap_count == 8)
        room_mood_question_due = 0;
    else if ((pet_sleep_state() != 3 && save.pet.life_stage != 0 && save.pet.status == 0) || room_clock_notice == 0
             || (pet_sleep_state() != 3 && save.pet.life_stage == 0 && save.pet.status == 0))
        room_mood_question_due = 1;
    else
        room_mood_question_due = 0;
    func_2408726();
    room_period_drawn = rtc_day_period();
    room_calls_drawn = 0;
    room_status_drawn = 0;
    D_0003FF7 = 0;
    ui_main_icon = 0;
    ui_icon_drawn = 0;
    room_view = 0;
    sys_backlight_level = 4;
    sys_set_backlight(4);
    snd_enabled = 1;
    T16_CTL4 = 1;
    pet_poops_drawn = save.pet.saved_poop_display;
outer:
    if (zero == 0 && room_clock_notice == 0 && (save.pet.status & 0x18) != 0 && save.pet.life_stage > 1) {
        if (save.pet.status & 0x10)
            pet_gone_screen(1);
        else if (save.pet.status & 8)
            pet_gone_screen(0);
    }
    rtc_seconds = zero;
    sys_awake_seconds = 0x960;
    sys_sleep_request = zero;
    nfc_session_clear();
restart:
    first = 1;
top:
    friend_event.blocked = 0;
    if (room_clock_notice == 0)
        nfc_wait_field_on();
    INT_F16T45 = 0x40;
    INT_E16T45 = 0x44;
    tmr_t5_start_timeout(500);
    if (room_clock_notice == 0 && !(save.pet.calls & 0x80) && pet_sleep_state() != 3
        && (save.pet.status & 0x18) != 0) {
        room_view = 0;
        if (save.pet.life_stage > 1) {
            if (save.pet.status & 0x10) {
                pet_gone_screen(0);
                goto restart;
            }
            if (save.pet.status & 8) {
                pet_gone_screen(0);
                goto restart;
            }
        }
    }
    room_idle = 1;
    pet_life_paused = 0;
    sys_debug_fast_forward = 0;
    room_scene_obj_idx = 0;
    pet_cleaning_reminder = 0;
    switch (save.pet.life_stage) {
    case 0:
        room_scene_current = room_scene_egg;
        break;
    case 1:
        if (save.pet.status != 0)
            room_scene_current = room_scene_baby_sick;
        else if (save.pet.toilet_timer == 0) {
            room_scene_current = room_scene_walk;
            room_scene_objs = room_scene_walk_objs;
        } else if (pet_special_mode_active() == 0)
            room_scene_current = room_scene_request;
        break;
    case 3:
    case 4:
    case 5:
        if (save.pet.status != 0) {
            if ((save.pet.status & 8) && room_clock_notice == 0)
                room_scene_current = room_scene_leaving;
            else if ((save.pet.status & 4) && room_clock_notice == 0) {
                room_scene_current = room_scene_collapsed;
                save.pet.hungry_call_timer = 0;
                save.pet.unhappy_call_timer = 0;
            } else if (save.pet.status & 0x40)
                room_scene_current = room_scene_departing;
            else if (save.pet.status & 1)
                room_scene_current = room_scene_sick;
            else
                room_scene_current = room_scene_neglected;
        } else if (pet_sitter_state.active != 0)
            room_scene_current = room_scene_sitter;
        else if (save.pet.praise_timer != 0 && pet_special_mode_active() == 0)
            room_scene_current = room_scene_praise_window;
        else if (save.pet.toilet_timer != 0 && pet_special_mode_active() == 0)
            room_scene_current = save.pet.training[0] == 0xff ? room_scene_toilet_call : room_scene_request;
        else if (save.pet.cleaning_timer != 0 && pet_special_mode_active() == 0)
            room_scene_current = save.pet.training[2] == 0xff ? room_scene_clean_call : room_scene_request;
        else if (save.pet.bath_timer != 0 && pet_special_mode_active() == 0)
            room_scene_current = save.pet.training[1] == 0xff ? room_scene_bath_call : room_scene_request;
        else {
            room_scene_current = room_scene_walk;
            room_scene_objs = room_scene_walk_objs;
        }
        break;
    default:
        if (save.pet.status != 0) {
            if ((save.pet.status & 8) && room_clock_notice == 0)
                room_scene_current = room_scene_leaving;
            else if ((save.pet.status & 4) && room_clock_notice == 0) {
                room_scene_current = room_scene_collapsed;
                save.pet.hungry_call_timer = 0;
                save.pet.unhappy_call_timer = 0;
            } else if (save.pet.status & 0x40)
                room_scene_current = room_scene_departing;
            else if (save.pet.status & 1)
                room_scene_current = room_scene_sick;
            else
                room_scene_current = room_scene_neglected;
        } else if (pet_sitter_state.active != 0)
            room_scene_current = room_scene_sitter;
        else if (save.pet.praise_timer != 0 && pet_special_mode_active() == 0)
            room_scene_current = room_scene_praise_window;
        else if (save.pet.toilet_timer != 0 && pet_special_mode_active() == 0) {
            if (save.pet.training[0] != 0xff)
                room_scene_current = room_scene_toilet_pending;
            else
                room_scene_current = room_scene_toilet_call;
        } else if (save.pet.cleaning_timer != 0 && pet_special_mode_active() == 0) {
            if (save.pet.training[2] != 0xff)
                room_scene_current = room_scene_clean_pending;
            else
                room_scene_current = room_scene_clean_call;
        } else if (save.pet.bath_timer != 0 && pet_special_mode_active() == 0) {
            if (save.pet.training[1] != 0xff)
                room_scene_current = room_scene_bath_pending;
            else
                room_scene_current = room_scene_bath_call;
        } else {
            room_scene_current = room_scene_walk;
            room_scene_objs = room_scene_walk_objs;
        }
        break;
    }
    if (save.pet.status != 0 && room_view == 1)
        room_view = 0;
    if (pet_sleep_state() == 3 && room_view == 0) {
        room_view = 3;
        if (save.pet.life_stage > 1)
            room_mood_question_due = 1;
        if (save.pet.poop_count != pet_poops_drawn) {
            if (save.pet.poop_count > 5 && pet_poops_drawn <= 5)
                pet_get_sick();
            if (save.pet.poop_count > 6)
                save.pet.poop_count = 6;
            pet_poops_drawn = save.pet.poop_count;
        }
    } else if (pet_sleep_state() != 3 && room_view == 3)
        room_view = 0;
    if (room_view == 3) {
        if (save.pet.life_stage == 1)
            room_scene_current = room_scene_baby_asleep;
        else
            room_scene_current = room_scene_asleep;
        diary_reset();
    }
    ui_new_screen();
    switch (room_view) {
    case 0:
        if (first == 1) {
            u16 old = save.pet.x000;
            evo_evolve_now_twice();
            first = 0;
            if (save.pet.x000 != old)
                goto top;
        }
        rtc_get_month();
        rtc_get_day();
        {
            u8 t = room_draw_background();
            if (save.pet.status != 0)
                gfx_set_tint_range(t);
        }
        room_draw_poops(1);
        break;
    case 1:
        room_scene_current = (struct RoomStepSigned *)room_window_draw(1);
        room_period_drawn = rtc_day_period();
        break;
    case 3:
        nfc_power_off();
        room_draw_bedroom();
        break;
    }
    if (save.pet.status != 0 && (room_view == 0 || room_view == 3)) {
        if (!(save.pet.status & 8)) {
            if ((save.pet.status & 4) && save.pet.life_stage > 1) {
                room_status_icon_obj = gfx_object_create(D_253EC98[0][0] - IW(D_2183E78),
                                         D_253EC98[0][1] - IH(D_2183E78) + 0x1c, 0xf0, 0, 0, (int)D_2183E78);
                spr_pet = gfx_object_create(D_253EC24_w[0].x - IW(D_21926E0), D_253EC24_w[0].y - IH(D_21926E0) + 0x1c,
                                    0x98, 0, 0, (int)D_21926E0);
            }
        }
    }
    if (room_scene_current == room_scene_walk && visit_load_partner_pics(0) != 0 && OBJ.kind == 1) {
        spr_obj = gfx_object_create(OBJ.x, OBJ.y + 0x1c, 0x50, 0, OBJ.flags, 0);
        gfx_object_create(OBJ.x, OBJ.y, 0x50, 0, 0, 0);
    }
    if (room_scene_current == room_scene_clean_pending) {
        gfx_object_create(0x40 - IW(D_2187038), 0x40 - IH(D_2187038), 0x50, 0, 0, (int)D_2187038);
    }
    spr_room = gfx_object_create(ROOM.x, ROOM.y, 0x70, 0, ROOM.flags, 0);
    gfx_object_create(ROOM.x, ROOM.y, 0x70, 0, 0, 0);
    gfx_object_create(ROOM.x, ROOM.y, 0x70, 0, 0, 0);
    if (room_scene_current == room_scene_walk && visit_load_partner_pics(0) != 0 && OBJ.kind == 0) {
        spr_obj = gfx_object_create(OBJ.x, OBJ.y + 0x1c, 0x50, 0, OBJ.flags, 0);
        gfx_object_create(OBJ.x, OBJ.y, 0x50, 0, 0, 0);
    }
    if (room_view == 3) {
        if (save.pet.life_stage == 1) {
            gfx_object_create(0x4b - IW(D_217A7B0), 0x51 - IH(D_217A7B0), 0x98, 0, 0, (int)D_217A7B0);
            spr_pet = gfx_object_create(0x54 - IW(D_214E220), 0x3c - IH(D_214E220), 0xa8, 0, 0, (int)D_214E220);
        } else {
            if (pet_get_gender())
                gfx_object_create(0x53 - IW(D_217A4B8), 0x4b - IH(D_217A4B8), 0x98, 0, 0, (int)D_217A4B8);
            else
                gfx_object_create(0x53 - IW(D_217A380), 0x4b - IH(D_217A380), 0x98, 0, 0, (int)D_217A380);
            spr_pet = gfx_object_create(0x56 - IW(D_214E108), 0x37 - IH(D_214E108), 0xa8, 0, 0, (int)D_214E108);
        }
    }
    ui_bubble_alloc();
    ui_choice_cursor_l_obj = 0;
    ui_choice_cursor_r_obj = 0;
    if (pet_sitter_state.active == 0 && room_view == 0) {
        struct PetState *q = &save.pet;
        int k = save.pet.life_stage;
        if (k != 0) {
            if ((q->calls & 7) != 0) {
                if (q->status == 0 && pet_special_mode_active() == 0) {
                    switch (k) {
                    case 1:
                    case 2:
                        room_scene_current = room_scene_call_child;
                        break;
                    case 3:
                    case 4:
                    case 5:
                        room_scene_current = room_scene_call_adult;
                        break;
                    default:
                        room_scene_current = room_scene_call_adult;
                        break;
                    }
                    ui_bubble_fill();
                }
            } else if (save.pet.status == 0 && room_scene_current == room_scene_request)
                ui_choice_cursors_create();
            else if (save.pet.status == 0 && save.pet.x040 != 0) {
                room_scene_current = room_scene_arrows;
                ui_arrows_create();
            } else if (save.pet.status == 0 && save.pet.x087 != 0) {
                room_scene_current = D_253EB00;
                room_status_icon_obj = gfx_object_create(D_253EB1C_w[0].x - 8, D_253EB1C_w[0].y + 0x14, 0xf0, 0, 0, (int)D_214DF50);
            }
        }
    }
    if (save.pet.life_stage != 0) {
        chara_draw_pet(spr_room, 0x70, ROOM.picture, ROOM.flags, ROOM.x, ROOM.y);
        if (room_view == 3) {
            u8 *p = (u8 *)chara_accessory_pics;
            if (p != 0)
                gfx_object_create(0x17 - (p[0] >> 1), 0x5e - (p[1] >> 1), 0x90, 0, 0, (int)p);
        }
        if (room_scene_current == room_scene_walk) {
            if (visit_load_partner_pics(0) != 0) {
                gfx_object_set_flags(spr_obj, OBJ.flags);
                gfx_object_set_image(spr_obj, 0x50, 0, anim_pic_slots[0][OBJ.picture]);
                gfx_object_move_dirty(spr_obj, OBJ.x - IW(anim_pic_slots[0][OBJ.picture]), OBJ.y - IH(anim_pic_slots[0][OBJ.picture]) + 0x1c);
            }
        } else if (room_scene_current == room_scene_sitter)
            room_sitter_overlay(0xff);
        else if (room_scene_current == room_scene_praise_window)
            room_praise_overlay(0xff);
        else if (room_scene_current == room_scene_toilet_pending || room_scene_current == room_scene_toilet_call)
            room_toilet_request_overlay(0xff);
        else if (room_scene_current == room_scene_bath_pending || room_scene_current == room_scene_bath_call)
            room_bath_request_overlay(0xff);
        else if (room_scene_current == room_scene_clean_pending || room_scene_current == room_scene_clean_call)
            room_cleaning_request_overlay(0xff);
    } else {
        gfx_object_set_flags(spr_room, ROOM.flags);
        gfx_object_set_image(spr_room, 0x70, 0, (const u8 *)room_egg_pics[ROOM.picture]);
        gfx_object_move_dirty(spr_room, ROOM.x - IW(room_egg_pics[ROOM.picture]), ROOM.y - IH(room_egg_pics[ROOM.picture]) + 0x1c);
    }
    if (save.pet.status != 0 && room_view == 0) {
        if ((save.pet.status & 0x4c) == 0x40) {
            room_status_icon_obj = gfx_object_create(D_253ECF0_w[0].x - IW(D_214EFAC), D_253ECF0_w[0].y - IH(D_214EFAC) + 0x1c,
                                     0xf0, 0, 0, (int)D_214EFAC);
        } else if ((save.pet.status & 0xd) == 1) {
            if (save.pet.life_stage == 1) {
                room_status_icon_obj = gfx_object_create(D_253EB50_w[0].x - IW(D_214E3C4), D_253EB50_w[0].y - IH(D_214E3C4) + 0x1c,
                                         0xf0, 0, 0, (int)D_214E3C4);
            } else {
                room_status_icon_obj = gfx_object_create(D_253ECD0_w[0].x - IW(D_214E338), D_253ECD0_w[0].y - IH(D_214E338) + 0x1c,
                                         0xf0, 0, 0, (int)D_214E338);
            }
        } else if ((save.pet.status & 0xe) == 2) {
            room_status_icon_obj = gfx_object_create(D_253EB70_w[0].x - IW(D_214E450), D_253EB70_w[0].y - IH(D_214E450) + 0x1c,
                                     0xf0, 0, 0, (int)D_214E450);
        }
    } else if (save.pet.status != 0 && room_view == 3) {
        u8 *p;
        if ((save.pet.status & 0x4c) == 0x40)
            p = D_214EFAC;
        else if ((save.pet.status & 0xd) == 1)
            p = save.pet.life_stage == 1 ? D_214E3C4 : D_214E338;
        else if ((save.pet.status & 0xe) == 2)
            p = D_214E450;
        else
            goto no_icon;
        room_status_icon_obj = gfx_object_create(0x30 - (p[0] >> 1), 0x35 - (p[1] >> 1), 0xf0, 0, 0, (int)p);
    }
no_icon:
    if (room_clock_notice != 0)
        ui_clock_check_msg();
    else
        room_draw_away();
    {
        int attn = 0;
        if (room_clock_notice == 0 && pet_sitter_state.active == 0 && pet_is_away() == 0) {
            if ((save.pet.calls & 0x80) && pet_sleep_state() != 3)
                attn = -1;
            if (save.pet.calls & 0x10)
                attn = -1;
            if (save.pet.calls & 0x40)
                attn = -1;
            if (save.pet.calls & 0x20)
                attn = -1;
        }
        if (attn == 0)
            nfc_arm_after_draw();
    }
    if (no_greet == 0)
        btn_clear();
    no_greet = 0;
    ui_timeout = 10;
    for (;;) {
        if (save_request != 0) {
            save_game();
            save_request = 0;
        }
        if (room_clock_notice == 0 && pet_sitter_state.active == 0 && pet_is_away() == 0) {
            if ((save.pet.calls & 0x80) && pet_sleep_state() != 3)
                goto attn;
            if (save.pet.calls & 0x10)
                goto attn;
            if (save.pet.calls & 0x40)
                goto attn;
            if (save.pet.calls & 0x20)
                goto attn;
        }
        {
            ui_update_mail_mark();
            ui_draw_marks();
            if (room_clock_notice != 0 && (btn_a_presses != 0 || btn_b_presses != 0 || btn_c_presses != 0)) {
                btn_clear();
                room_clock_notice = 0;
                if (save.pet.status == 0)
                    pet_habit_learned();
                goto outer;
            }
            if (sys_sleep_request == 1) {
                room_view_at_sleep = room_view;
                room_poops_at_sleep = save.pet.poop_count;
                sys_sleep_request = 2;
            }
            if (sys_sleep_request == 2) {
                if (sys_display_state == 3) {
                    nfc_power_off();
                    sys_display_state = 2;
                    T16_CTL1 = 0;
                    T16_CTL3 = 0;
                    T16_CTL5 = 0;
                    sys_display_off();
                    anim_busy = 0;
                    while (sys_display_state == 2)
                        asm volatile("slp");
                    sys_sleep_request = 0;
                    if (room_clock_notice != 0) {
                        room_clock_notice = 0;
                        if (save.pet.status == 0)
                            pet_habit_learned();
                        goto outer;
                    }
                    if (pet_sleep_state() == 3 && room_view_at_sleep == 0)
                        goto restart;
                    if (pet_sleep_state() != 3 && room_view_at_sleep == 3)
                        goto restart;
                    if (room_poops_at_sleep != save.pet.poop_count)
                        goto restart;
                    T16_CTL5 = 1;
                } else {
                    anim_busy = 0;
                    sys_sleep_request = 0;
                }
            }
            if (ui_main_pending_hook() != 0)
                goto restart;
            if (pet_sitter_due() != 0) {
                pet_sitter_return();
                goto restart;
            }
            if (btn_a_presses != 0) {
                btn_a_presses = 0;
                if (pet_care_event_dispatch() != 0)
                    goto restart;
                ui_timeout = 10;
                if (room_view == 2)
                    rtc_clock_blink_toggle();
                else {
                    if (pet_sitter_state.active != 0) {
                        pet_sitter_return();
                        goto restart;
                    }
                    if (save.pet.life_stage != 0) {
                        if (++ui_main_icon > 9)
                            ui_main_icon = 0;
                    }
                }
                btn_a_presses = 0;
            }
            if (btn_b_presses != 0) {
                btn_b_presses = 0;
                if (ui_main_icon == 4 && ui_run_away_screen() != 0)
                    goto restart;
                if (save.pet.calls != 0)
                    pet_cleaning_reminder = 0;
                pet_clear_call(4);
                if (pet_care_event_dispatch() != 0)
                    goto restart;
                ui_timeout = 10;
                if (room_view == 2) {
                    room_view = 0;
                    pet_away_minute_check();
                    goto restart;
                }
                if (pet_sitter_state.active != 0) {
                    pet_sitter_return();
                    goto restart;
                }
                switch (ui_main_icon) {
                case 1:
                    if (room_scene_current != room_scene_leaving && room_scene_current != room_scene_collapsed && (save.pet.status & 4)) {
                        snd_play(12);
                        break;
                    }
                    pet_care_flags_reset();
                    room_idle = 0;
                    ui_status_menu();
                    save.pet.x040 = 0;
                    if (ui_timeout == 0)
                        ui_main_icon = 0;
                    if (pet_sleep_state() == 3)
                        room_view = 0;
                    if (sys_display_state != 2)
                        goto restart;
                    if (((save.pet.calls & 0x80) && pet_sleep_state() != 3) || (save.pet.calls & 0x70)) {
                        sys_idle_seconds = 0;
                        sys_display_state = 0;
                        break;
                    }
                    goto restart;
                case 4:
                    if (pet_asleep_refuse() != 0)
                        goto restart;
                    if (save.pet.status != 0) {
                        anim_refuse();
                        goto restart;
                    }
                    pet_care_flags_reset();
                    room_idle = 0;
                    friend_event.blocked = 1;
                    ui_set_mark(0, 0);
                    if (food_menu() != 0)
                        room_view = 0;
                    save.pet.x040 = 0;
                    goto menu_done;
                case 6:
                    if (pet_asleep_refuse() != 0)
                        goto restart;
                    if (save.pet.status != 0) {
                        anim_refuse();
                        goto restart;
                    }
                    pet_care_flags_reset();
                    room_idle = 0;
                    if ((save.pet.toilet_timer == 0 && save.pet.poop_count != 0)
                        || (save.pet.toilet_timer != 0
                            && (save.pet.life_stage == 1 || (save.pet.life_stage > 2 && save.pet.training[0] != 0xff)))) {
                        if (room_view == 1 && save.pet.toilet_timer == 0 && save.pet.poop_count != 0) {
                            pending = -1;
                            room_view = 0;
                            ui_bubble_hide();
                            goto restart;
                        }
                        ui_bubble_hide();
                        pet_toilet();
                        room_view = 0;
                    } else {
                        pending = pet_care_room_menu();
                        if (pending > 0) {
                            room_view = 0;
                            pending = 0;
                        } else if (pending < 0) {
                            room_view = 0;
                            pet_poops_drawn = save.pet.poop_count;
                            goto restart;
                        }
                    }
                    save.pet.x040 = 0;
                    goto menu_done;
                case 3:
                    if (pet_asleep_refuse() != 0)
                        goto restart;
                    if (save.pet.status != 0) {
                        anim_refuse();
                        goto restart;
                    }
                    pet_care_flags_reset();
                    ui_set_mark(1, 0);
                    room_idle = 0;
                    spot_bingo_gashapon_menu();
                    save.pet.x040 = 0;
                    goto menu_done;
                case 2:
                    if (pet_asleep_refuse() != 0)
                        goto restart;
                    if (save.pet.status != 0) {
                        anim_refuse();
                        goto restart;
                    }
                    if (sys_battery_low() == 1) {
                        btn_clear();
                        break;
                    }
                    pet_care_flags_reset();
                    room_idle = 0;
                    INT_EP47_ERTC_EAD = 4;
                    sys_leave_low_power();
                    friend_event.blocked = 1;
                    comm_menu_active = 1;
                    comm_main_menu();
                    if (sys_battery_level <= 0x2e3)
                        break;
                    comm_menu_active = 0;
                    func_24F71D8();
                    INT_EP47_ERTC_EAD = 6;
                    goto menu_done;
                case 7:
                    if (pet_asleep_refuse() != 0)
                        goto restart;
                    if (save.pet.status != 0 || save.pet.weather_now == 3 || save.pet.weather_now == 4) {
                        anim_refuse();
                        goto restart;
                    }
                    pet_care_flags_reset();
                    room_idle = 0;
                    friend_event.blocked = 1;
                    ui_set_mark(2, 0);
                    {
                        int v = sys_battery_low();
                        if (v == 0) {
                            if (town_menu() != 0)
                                room_view = 0;
                        } else
                            room_view = 0;
                    }
                    save.pet.x040 = 0;
                    goto menu_done;
                case 8:
                    if (pet_asleep_refuse() != 0)
                        goto restart;
                    if (save.pet.status != 0) {
                        anim_refuse();
                        goto restart;
                    }
                    pet_care_flags_reset();
                    room_idle = 0;
                    save.pet.x040 = 0;
                    friend_event.blocked = 1;
                    ui_set_mark(3, 0);
                    ui_set_mark(6, 0);
                    item_special_menu();
                    goto menu_done;
                case 9:
                    if (save.pet.status != 0) {
                        if (pet_sleep_state() == 3) {
                            snd_error_beep();
                            break;
                        }
                        anim_refuse();
                        goto restart;
                    }
                    pet_care_flags_reset();
                    room_idle = 0;
                    friend_event.blocked = 1;
                    ui_set_mark(4, 0);
                    ui_notebook_menu();
                    save.pet.x040 = 0;
                    goto menu_done;
                case 5:
                    if (pet_asleep_refuse() != 0)
                        goto restart;
                    if (save.pet.status & 8) {
                        anim_refuse();
                        goto restart;
                    }
                    room_idle = 0;
                    if (pet_care_menu() != 0)
                        room_view = 0;
                menu_done:
                    if (ui_timeout == 0)
                        ui_main_icon = 0;
                    goto restart;
                default:
                    if (room_view == 1 || save.pet.status != 0 || save.pet.life_stage == 0 || room_view == 3
                        || ui_main_icon_blocked(0, 0xf0) != 0) {
                        room_view = 2;
                        save.pet.x040 = 0;
                        ui_new_screen();
                        rtc_clock_create();
                        rtc_clock_show_now();
                        func_00009A6();
                        break;
                    }
                    room_view = 1;
                    if (comm_house_level() != 0)
                        goto restart;
                    friend_event.blocked = 1;
                    if (pet_event_random_daily_wrapper() != 0)
                        room_view = 0;
                    friend_event.blocked = 0;
                    save.pet.x040 = 0;
                    goto restart;
                }
                btn_b_presses = 0;
            }
            if (btn_c_presses != 0) {
                btn_c_presses = 0;
                if (pet_care_event_dispatch() != 0)
                    goto restart;
                if (save.pet.life_stage != 0) {
                    if (ui_main_icon != 0)
                        ui_main_icon = 0;
                    else if (room_view == 1) {
                        room_view = 0;
                        goto restart;
                    } else if (room_view == 0 && save.pet.status == 0) {
                        if (pet_sitter_state.active != 0) {
                            pet_sitter_return();
                            goto restart;
                        }
                        if (save.pet.praise_timer != 0) {
                            pet_habit_learned();
                            goto restart;
                        }
                        if (ui_main_icon_blocked(0, 0xe0) == 0) {
                            if (save.pet.calls != 0)
                                pet_cleaning_reminder = 0;
                            pet_clear_call(4);
                            pet_poke_animation();
                            no_greet = 1;
                            btn_c_presses = 0;
                            nfc_wait_field_on();
                            goto restart;
                        }
                    }
                }
                btn_c_presses = 0;
            }
            if (ui_timeout == 0)
                ui_main_icon = 0;
            if (ui_main_icon != ui_icon_drawn) {
                gfx_object_set_image(ui_icon_cursor_obj, 0x100, 0, 0);
                func_00009A6();
                ui_icon_cursor_draw();
                nfc_arm_after_draw();
                ui_icon_drawn = ui_main_icon;
            }
        }
    attn:
        if (room_clock_notice != 0) {
            if (save.pet.poop_count != pet_poops_drawn) {
                if (save.pet.poop_count > 5 && pet_poops_drawn <= 5)
                    pet_get_sick();
                if (save.pet.poop_count > 6)
                    save.pet.poop_count = 6;
                pet_poops_drawn = save.pet.poop_count;
            }
            sys_idle_tick();
        } else {
            if (pet_sitter_state.active == 0 && pet_is_away() == 0) {
                if ((save.pet.calls & 0x80) && pet_sleep_state() != 3) {
                    pet_clear_call(0x80);
                    room_view = 0;
                    evo_stage_change();
                    if (save.pet.life_stage != 3)
                        goto restart;
                    pet_clear_call(1);
                    pet_clear_call(2);
                    goto restart;
                }
                if (save.pet.calls & 0x10) {
                    pet_clear_call(0x10);
                    room_view = 0;
                    sys_display_wake(1);
                    anim_owner_birthday();
                    anim_happy();
                    goto restart;
                }
                if (save.pet.calls & 0x40) {
                    pet_clear_call(0x40);
                    room_view = 0;
                    sys_display_wake(1);
                    pet_event_chara_birthday();
                    anim_happy();
                    goto restart;
                }
                if (save.pet.calls & 0x20) {
                    pet_clear_call(0x20);
                    anim_christmas_play();
                    room_view = 0;
                    goto restart;
                }
            }
            if (pet_request_bits != 0 && save.pet.status == 0 && save.pet.calls == 0) {
                if ((pet_request_bits & 4) && pet_cleaning_reminder != 0) {
                    if (room_view == 0 && save.pet.toilet_timer == 0 && save.pet.bath_timer == 0 && save.pet.praise_timer == 0)
                        anim_room_messy();
                    pet_cleaning_reminder = 0;
                }
                pet_request_bits = 0;
                if (room_view == 0)
                    goto restart;
            }
            if (pet_praise_expired == 0)
                goto no_ffc;
            if (pet_sleep_state() <= 2 && pet_sitter_state.active == 0 && room_view == 0 && save.pet.status == 0) {

                pet_habit_learned();
                pet_praise_expired = 0;
                goto restart;
            }
        no_ffc:
            if (pet_toilet_expired != 0) {
                struct PetState *q = &save.pet;
                if (save.pet.training[0] == 0xff && q->status == 0) {
                    q->toilet_timer = 0;
                    save_game();
                }
                pet_toilet_expired = 0;
                if (room_view != 2)
                    goto restart;
            }
            if (pet_bath_expired != 0) {
                struct PetState *q = &save.pet;
                if (save.pet.training[1] == 0xff && q->status == 0) {
                    evo_count_bath();
                    save.pet.neglect_minutes = 0;
                    save.pet.x062 = 0;
                    q->meals_today = 0;
                    save.pet.x0cc = 0;
                    save.pet.x148 = 0;
                    save_game();
                }
                pet_bath_expired = 0;
                if (room_view != 2)
                    goto restart;
            }
            if (pet_cleaning_expired != 0) {
                if (save.pet.training[2] == 0xff && save.pet.status == 0) {
                    save.pet.kitchen_dirt = 0;
                    save.pet.toilet_dirt = 0;
                    save.pet.living_dirt = 0;
                    save.pet.bedroom_dirt = 0;
                    save.pet.bath_dirt = 0;
                    save_game();
                } else {
                    if (room_scene_current == room_scene_clean_pending && sys_display_state == 0 && room_view == 0)
                        anim_cleaning_missed();
                    if (++save.pet.living_dirt > 5)
                        save.pet.living_dirt = 5;
                }
                pet_cleaning_expired = 0;
                if (room_view != 2)
                    goto restart;
            }
            {
            struct PetState *q = &save.pet;
            int c = q->poop_count;
            if (c != pet_poops_drawn && pet_special_mode_active() == 0) {
                u8 s = q->status;
                if (c != 0 && room_view == 0) {
                    if (ui_choice_cursor_l_obj != 0 || ui_choice_cursor_r_obj != 0) {
                        gfx_object_set_image(ui_choice_cursor_l_obj, 0, 0, 0);
                        gfx_object_set_image(ui_choice_cursor_r_obj, 0, 0, 0);
                    }
                    if (save.pet.life_stage == 1)
                        chara_draw_pet(spr_room, 0x70, 3, 0, 0x40, 0x32);
                    else
                        chara_draw_pet(spr_room, 0x70, 10, 0, 0x40, 0x32);
                    gfx_object_create(0x50, 0x50, 0xa8, 0, 0, (int)D_214FBA8);
                    func_00009A6();
                    snd_play(0x14);
                    snd_wait_end();
                    if (save.pet.poop_count > 5 && pet_poops_drawn <= 5) {
                        pet_get_sick();
                        if ((s & 0xfd) == 0)
                            snd_play(4);
                    }
                    if (save.pet.poop_count > 6)
                        save.pet.poop_count = 6;
                    pet_poops_drawn = save.pet.poop_count;
                    goto restart;
                }
                if (save.pet.poop_count > 5 && pet_poops_drawn <= 5) {
                    pet_get_sick();
                    if ((s & 0xfd) == 0)
                        snd_play(4);
                }
                if (save.pet.poop_count > 6)
                    save.pet.poop_count = 6;
                pet_poops_drawn = save.pet.poop_count;
            }
            }
            if (save.pet.calls != room_calls_drawn) {
                room_calls_drawn = save.pet.calls;
                sys_debug_fast_forward = 0;
                if (room_view == 0)
                    goto restart;
            }
            if (rtc_day_period() != room_period_drawn) {
                room_period_drawn = rtc_day_period();
                if (room_view != 2) {
                    pet_away_minute_check();
                    goto restart;
                }
            }
            rand();
            sys_idle_tick();
            if (!(save.pet.calls & 0x80) && pet_sleep_state() != 3 && (save.pet.status & 0x18) != 0) {
                room_view = 0;
                if (save.pet.life_stage > 1) {
                    if (save.pet.status & 0x10) {
                        pet_gone_screen(0);
                        goto restart;
                    }
                    if (save.pet.status & 8) {
                        pet_gone_screen(0);
                        goto restart;
                    }
                }
            }
            if (save.pet.status != room_status_drawn) {
                room_status_drawn = save.pet.status;
                if (save.pet.status != 0 && room_view != 2)
                    goto restart;
            }
            switch (pet_sleep_state()) {
            case 0:
                if (room_last_sleep_state != 0) {
                    room_last_sleep_state = 0;
                    if (room_view != 1 && room_view != 2) {
                        room_view = 0;
                        goto restart;
                    }
                }
                break;
            case 1:
                room_last_sleep_state = 1;
                if (pet_special_mode_active() == 0 && rtc_get_minute() % 5 == 0 && rtc_get_second() == 0
                    && room_view == 0) {
                    anim_drowsy();
                    btn_a_presses = 0;
                    btn_b_presses = 0;
                    btn_c_presses = 0;
                    no_greet = 1;
                    goto restart;
                }
                break;
            case 2:
                room_last_sleep_state = 2;
                if (pet_special_mode_active() == 0 && rtc_get_minute() % 5 == 0 && rtc_get_second() == 0
                    && (rand() & 1) == 0 && room_view == 0) {
                    anim_drowsy();
                    btn_a_presses = 0;
                    btn_b_presses = 0;
                    btn_c_presses = 0;
                    no_greet = 1;
                    goto restart;
                }
                break;
            case 3:
                if (room_last_sleep_state != 3) {
                    room_last_sleep_state = 3;
                    if (room_view != 2) {
                        room_view = 0;
                        goto restart;
                    }
                }
                break;
            }
            if ((tmr_flags & 0x20) == 0)
                goto no_tick;
            tmr_flags &= ~0x20;
            mode = 0;
            if (sys_debug_fast_forward != 0) {
                T16_CTL5 = 2;
                T16_CR5B = 0x26;
                T16_CLKCTL_5 = 0xf;
                T16_CTL5 = 1;
            }
            if (room_view == 2)
                rtc_clock_show_now();
            else {
                if (ROOM.picture < 0)
                    room_scene_obj_idx = 0;
                if (save.pet.life_stage != 0) {
                    chara_draw_pet(spr_room, 0x70, ROOM.picture, ROOM.flags, ROOM.x, ROOM.y);
                    if (room_scene_current == room_scene_walk && visit_load_partner_pics(0) != 0) {
                        gfx_object_set_flags(spr_obj, OBJ.flags);
                        gfx_object_set_image(spr_obj, 0x50, 0, anim_pic_slots[0][OBJ.picture]);
                        gfx_object_move_dirty(spr_obj, OBJ.x - IW(anim_pic_slots[0][OBJ.picture]), OBJ.y - IH(anim_pic_slots[0][OBJ.picture]) + 0x1c);
                    }
                } else {
                    gfx_object_set_flags(spr_room, ROOM.flags);
                    gfx_object_set_image(spr_room, 0x70, 0, (const u8 *)room_egg_pics[ROOM.picture]);
                    gfx_object_move_dirty(spr_room, ROOM.x - IW(room_egg_pics[ROOM.picture]), ROOM.y - IH(room_egg_pics[ROOM.picture]) + 0x1c);
                    tmr_t5_start_timeout(room_egg_step_ms[room_scene_obj_idx]);
                }
                if (room_scene_current == room_scene_call_child || room_scene_current == room_scene_call_adult) {
                    if (save.pet.life_stage > 2)
                        ui_bubble_fill();
                    else
                        switch (room_scene_obj_idx) {
                        case 0:
                            ui_bubble_hide();
                            break;
                        case 2:
                            ui_bubble_fill();
                            break;
                        }
                }
                if (room_scene_current == room_scene_request)
                    ui_choice_cursors_bob(room_scene_obj_idx & 1);
                if (room_scene_current == room_scene_baby_asleep) {
                    switch (room_scene_obj_idx) {
                    case 0:
                        gfx_object_set_image(spr_pet, 0xa8, 0, D_214E220);
                        break;
                    case 2:
                        gfx_object_set_image(spr_pet, 0xa8, 0, D_214E2AC);
                        break;
                    }
                    switch (room_scene_obj_idx) {
                    case 0:
                        if (save.pet.status & 0x40) {
                            gfx_object_set_flags(room_status_icon_obj, 0);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214EFAC), 0x35 - IH(D_214EFAC));
                        } else if (save.pet.status & 1) {
                            gfx_object_set_flags(room_status_icon_obj, 0);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214E3C4), 0x35 - IH(D_214E3C4));
                        }
                        break;
                    case 2:
                        if (save.pet.status & 0x40) {
                            gfx_object_set_flags(room_status_icon_obj, 0x10);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214EFAC), 0x35 - IH(D_214EFAC));
                        } else if (save.pet.status & 1) {
                            gfx_object_set_flags(room_status_icon_obj, 0);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214E3C4), 0x36 - IH(D_214E3C4));
                        }
                        break;
                    }
                }
                if (room_scene_current == room_scene_asleep) {
                    switch (room_scene_obj_idx) {
                    case 0:
                        gfx_object_set_image(spr_pet, 0xa8, 0, D_214E108);
                        break;
                    case 2:
                        gfx_object_set_image(spr_pet, 0xa8, 0, D_214E194);
                        break;
                    }
                    switch (room_scene_obj_idx) {
                    case 0:
                        if (save.pet.status & 0x40) {
                            gfx_object_set_flags(room_status_icon_obj, 0);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214EFAC), 0x35 - IH(D_214EFAC));
                        } else if (save.pet.status & 1) {
                            gfx_object_set_flags(room_status_icon_obj, 0);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214E338), 0x35 - IH(D_214E338));
                        } else if (save.pet.status & 2) {
                            gfx_object_set_flags(room_status_icon_obj, 0);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214E450), 0x35 - IH(D_214E450));
                        }
                        break;
                    case 2:
                        if (save.pet.status & 0x40) {
                            gfx_object_set_flags(room_status_icon_obj, 0x10);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214EFAC), 0x35 - IH(D_214EFAC));
                        } else if (save.pet.status & 1) {
                            gfx_object_set_flags(room_status_icon_obj, 0);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214E338), 0x36 - IH(D_214E338));
                        } else if (save.pet.status & 2) {
                            gfx_object_set_flags(room_status_icon_obj, 0);
                            gfx_object_move_dirty(room_status_icon_obj, 0x30 - IW(D_214E450), 0x36 - IH(D_214E450));
                        }
                        break;
                    }
                }
                if (room_scene_current == room_scene_baby_sick) {
                    u8 id = room_status_icon_obj;
                    struct RoomStepSigned *t = D_253EB50_w;
                    gfx_object_set_flags(id, t[room_scene_obj_idx].flags);
                    gfx_object_move_dirty(room_status_icon_obj, t[room_scene_obj_idx].x - IW(D_214E3C4),
                                 t[room_scene_obj_idx].y - IH(D_214E3C4) + 0x1c);
                }
                if (room_scene_current == room_scene_sick) {
                    u8 id = room_status_icon_obj;
                    struct RoomStepSigned *t = D_253ECD0_w;
                    gfx_object_set_flags(id, t[room_scene_obj_idx].flags);
                    gfx_object_move_dirty(room_status_icon_obj, t[room_scene_obj_idx].x - IW(D_214E338),
                                 t[room_scene_obj_idx].y - IH(D_214E338) + 0x1c);
                }
                if (room_scene_current == room_scene_neglected) {
                    u8 id = room_status_icon_obj;
                    struct RoomStepSigned *t = D_253EB70_w;
                    gfx_object_set_flags(id, t[room_scene_obj_idx].flags);
                    gfx_object_move_dirty(room_status_icon_obj, t[room_scene_obj_idx].x - IW(D_214E450),
                                 t[room_scene_obj_idx].y - IH(D_214E450) + 0x1c);
                }
                if (room_scene_current == room_scene_departing) {
                    u8 id = room_status_icon_obj;
                    struct RoomStepSigned *t = D_253ECF0_w;
                    gfx_object_set_flags(id, t[room_scene_obj_idx].flags);
                    gfx_object_move_dirty(room_status_icon_obj, t[room_scene_obj_idx].x - IW(D_214EFAC),
                                 t[room_scene_obj_idx].y - IH(D_214EFAC) + 0x1c);
                }
                if (room_scene_current == room_scene_unused_b) {
                    switch (room_scene_obj_idx) {
                    case 0:
                        gfx_object_set_flags(room_status_icon_obj, 0);
                        gfx_object_move_dirty(room_status_icon_obj, 0x4b, 0x2e);
                        break;
                    case 1:
                        gfx_object_set_flags(room_status_icon_obj, 0x10);
                        gfx_object_move_dirty(room_status_icon_obj, 0x46, 0x2e);
                        break;
                    }
                }
                if (room_scene_current == room_scene_collapsed) {
                    u8 (*e)[2] = D_253EC98;
                    struct RoomStepSigned *t;
                    gfx_object_move_dirty(room_status_icon_obj, e[room_scene_obj_idx][0] - IW(D_2183E78),
                                 e[room_scene_obj_idx][1] - IH(D_2183E78) + 0x1c);
                    t = D_253EC24_w;
                    gfx_object_set_flags(spr_pet, t[room_scene_obj_idx].flags);
                    gfx_object_set_image(spr_pet, 0x98, 0, D_21926E0);
                    gfx_object_move_dirty(spr_pet, t[room_scene_obj_idx].x - IW(D_21926E0),
                                 t[room_scene_obj_idx].y - IH(D_21926E0) + 0x1c);
                }
                if (room_scene_current == room_scene_unused_c) {
                    switch (room_scene_obj_idx) {
                    case 0:
                        gfx_object_set_flags(room_status_icon_obj, 0);
                        gfx_object_move_dirty(room_status_icon_obj, 0x23, 0x2e);
                        snd_play(0x1a);
                        break;
                    case 2:
                        gfx_object_set_flags(room_status_icon_obj, 0x10);
                        gfx_object_move_dirty(room_status_icon_obj, 0x46, 0x2e);
                        break;
                    }
                }
                if (room_scene_current == room_scene_leaving) {
                    switch (room_scene_obj_idx) {
                    case 0: case 2: case 4: case 7: case 9: case 11: case 14:
                        snd_play(1);
                        break;
                    case 5: case 12:
                        snd_play(10);
                        break;
                    }
                }
                if (room_scene_current == room_scene_arrows) {
                    ui_arrows_move(room_scene_obj_idx);
                    if (save.pet.x040 == 0)
                        goto restart;
                }
                if (room_scene_current == room_scene_unused_a) {
                    switch (room_scene_obj_idx) {
                    case 0:
                        gfx_object_set_image(room_status_icon_obj, 0xf0, 0, D_214DC90);
                        break;
                    case 1:
                        gfx_object_set_image(room_status_icon_obj, 0xf0, 0, D_214DD1C);
                        break;
                    }
                }
                if (room_scene_current == D_253EB00) {
                    struct RoomStepSigned *t = D_253EB1C_w;
                    gfx_object_move_dirty(room_status_icon_obj, t[room_scene_obj_idx].x - 8, t[room_scene_obj_idx].y + 0x14);
                    gfx_object_set_image(room_status_icon_obj, 0xf0, 0, (const u8 *)room_arrow_pics[room_scene_obj_idx]);
                }
                if (room_scene_current == room_scene_sitter)
                    room_sitter_overlay(room_scene_obj_idx);
                if (room_scene_current == room_scene_clean_pending)
                    room_cleaning_request_overlay(room_scene_obj_idx);
                if (room_scene_current == room_scene_praise_window)
                    room_praise_overlay(room_scene_obj_idx);
                if (room_view == 1 || room_view == 3)
                    room_window_animate(room_scene_obj_idx);
                else if (room_blink_poops() != 0)
                    goto restart;
                room_scene_obj_idx++;
            }
        }
        if (room_clock_notice != 0)
            ui_clock_check_msg();
        else
            room_draw_away();
        func_00009A6();
        room_view_at_sleep = room_view;
        room_poops_at_sleep = save.pet.poop_count;
        if (pending < 0) {
            ui_bubble_hide();
            pet_toilet();
            pending = 0;
            room_poops_at_sleep = save.pet.poop_count;
            goto restart;
        }
        if (nfc_idle_touch() == 1) {
            nfc_rearm_if_field();
            goto restart;
        }
        nfc_rearm_if_field();
    no_tick:
        if (sys_battery_check_due != 0) {
            sys_battery_low();
            sys_battery_check_due = 0;
        }
        if (sys_battery_level <= 0x2e3)
            sys_power_down();
        if (sys_display_state == 2) {
            sys_set_backlight(0);
            sys_apply_backlight();
            anim_busy = 1;
            sys_display_state = 3;
            if (room_view_at_sleep == room_view && (room_view_at_sleep == 0 || room_view_at_sleep == 3))
                sys_sleep_request = 2;
            else {
                sys_sleep_request = 1;
                room_view = 0;
                pet_away_minute_check();
                goto restart;
            }
        }
    }
}

/*
 * sys_power_down (asm): battery empty / power down: show the low-battery picture
 * (0x2129614), stop the tone timers (CTL0/1/3/5 = 2), power down the NFC front-end, sound
 * off, backlight off, 32 kHz clock (sys_sleep_osc1); then loop forever on `halt` (the LCD is
 * put to sleep (0x10) and the CPU does `slp` while the display state sys_display_state == 2).
 * Never returns; called wherever sys_battery_low() reports a low battery.
 */
int sys_power_down(void)
{
    sys_battery_dead = 1;
    comm_menu_active = 0;
    sys_idle_seconds = 0;
    pet_life_paused = 1;
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2129614);
    func_00009A6();
    T16_CTL0 = 0;
    T16_CTL1 = 0;
    T16_CTL3 = 0;
    T16_CTL4 = 0;
    T16_CTL5 = 0;
    nfc_hw_power_off();
    nfc_sleep_until_field(1);
    snd_enabled = 0;
    sys_set_backlight(0);
    sys_apply_backlight();
    sys_sleep_osc1();
    sys_battery_empty = 1;
    anim_busy = 0;
    sys_set_backlight(4);
    sys_apply_backlight();
    btn_unmask(7);
    INT_EP47_ERTC_EAD = 4;
    for (;;) {
        if (sys_display_state == 2) {
            INT_EP47_ERTC_EAD = 0;
            T16_CTL2 = 2;
            REG8(0x600000) = 0x10;
            lcd_asleep = 1;
            while (sys_display_state == 2)
                asm volatile("slp");
            INT_EP47_ERTC_EAD = 4;
        }
        asm volatile("halt");
    }
}

/*
 * Main loop event checks, on a key press at home (no sub-screen, awake, no status flag):
 * run one pending event: the mood question room_mood_question_due (text 100/0xB1
 * "きょうのきぶんは､どのきぶん?") -> ui_show_message(7, ..) + owner_mood_setting; else the
 * daily-care clover leaf pet_try_daily_care_leaf; the ten-day event
 * pet.ten_day_event_due -> pet_see_off (not while with the sitter); and a trained pet
 * (training counter 0xFF) answering its own request: the toilet urge pet.toilet_timer ->
 * pet_toilet (stage > 1), the 18:00 cleaning request -> pet_cleaning, the 19:00 bath request
 * -> pet_bath. pet.training[0] / pet.training[1] / pet.training[2] count a
 * child's answered requests and become 0xFF when the habit is learned (pet_habit_learned);
 * the RTC tick only reads them. Returns 1 when an event ran.
 */
u8 pet_care_event_dispatch(void)
{
    u8 r = 0;

    if (room_view != r || pet_sleep_state() == 3 || save.pet.status != r)
        return r;
    if (room_mood_question_due != 0 && pet_sleep_state() != 3 && save.pet.life_stage != 0 && save.pet.status == 0) {
        ui_show_message(7, 0, chara_text(0, 100, 0xb1));
        owner_mood_setting();
        room_mood_question_due = 0;
        r = 1;
    } else {
        r = pet_try_daily_care_leaf();
        if (r == 0) {
            if (pet_sitter_state.active == 0 && save.pet.ten_day_event_due != 0) {
                pet_see_off();
                save.pet.ten_day_event_due = 0;
                r = 1;
            } else if (save.pet.toilet_timer != 0 && save.pet.training[0] == 0xff && save.pet.life_stage > 1) {
                pet_toilet();
                r = 1;
            } else if (save.pet.cleaning_timer != 0 && save.pet.training[2] == 0xff) {
                pet_cleaning();
                r = 1;
            } else if (save.pet.bath_timer != 0 && save.pet.training[1] == 0xff) {
                pet_bath();
                r = 1;
            }
        }
    }
    if (r != 0)
        btn_clear();
    return r;
}

/*
 * The RAM variables this file defines.
 */
u8 lcd_palette_set;
u8 room_clock_notice;
u8 comm_menu_active;
u8 sys_backlight_level;
volatile u8 room_idle;
u16 ui_main_icon;
u8 room_view;
volatile u8 ui_bubble_line_idx;
u8 sys_battery_dead;
struct RoomStepSigned *room_scene_current;
struct RoomObjStepSigned *room_scene_objs;
u16 ui_icon_drawn;
u8 room_view_at_sleep;
u8 room_poops_at_sleep;
u8 room_scene_obj_idx;
u8 ui_call_icon_obj;
u8 ui_icon_cursor_obj;
u8 room_status_icon_obj;
u8 sys_sleep_request;
u8 room_calls_drawn;
u8 room_status_drawn;
u8 room_period_drawn;
u8 room_last_sleep_state;
u8 lcd_asleep;
u8 ui_attention_icon_state;
u8 D_0003FF7;
u8 room_mood_question_due;
u8 ui_bubble_frame_obj;
u8 ui_bubble_line1_obj;
u8 ui_bubble_line2_obj;
u8 ui_choice_cursor_l_obj;
u8 ui_choice_cursor_r_obj;
u8 ui_arrow1_obj;
u8 ui_arrow2_obj;
u8 sys_battery_empty;
u16 comm_count_before;
