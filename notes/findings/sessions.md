# NFC session types (OPEN.md item 8) and the peer kind 0xF0EF

## Answer

The NFC code uses **two numberings**, and only two:

* **menu choice** = the argument of `nfc_menu_session` (F69), 0..7, passed by the F7 menu handlers;
* **session type** = `nfc_session.x003`. `comm_build_header` (F70b) copies it into byte **0x4C** of every record it builds, so the session type and the "mode byte on the wire" are the same number.

`nfc_menu_session` maps the first to the second. The proposal is **session type 6** (menu choice 5). Fable was right. Opus's "marriage = type 8" is wrong; Opus's own table (5 -> type 6) already shows that. Type 8 is the app download link.

`comm_menu_choice` / `comm_submenu_choice` are a third set of values, but they are **not session numbers**. They are legacy bytes of the old IR profile block (`flash_write_text_record` writes them at +2/+3). `comm_exchange_kind` is the IR exchange kind, which the NFC gift outcome reuses for "giving / receiving side".

## Table (all verified in code unless marked)

| Menu entry (table) | Handler | Menu choice | Session type x003 / 0x4C | Role x002 | Peer side | Outcome (F66 `comm_run_outcome`) |
|---|---|---|---|---|---|---|
| あいさツーしん (comm_main_menu_items #0) | comm_menu_greeting -> comm_greeting_session | 1 | 2 greeting | 1 initiator | idle toy (auto target) | comm_outcome_greeting |
| たまごっち4U > ツーしんゲーム (comm_tama4u_menu_items #0) | comm_menu_games -> comm_game_session | 2 | 3, then 4 (round 2) | 1 | idle toy; round 2 via nfc_idle_session on the target | comm_outcome_game1 / comm_outcome_game2 |
| たまごっち4U > アイテムこうかん > あげる (#1) | comm_menu_item_exchange -> comm_give_menu -> comm_give_inventory_item / comm_give_shop_item -> comm_give_item | 3 | 5 gift | 1 | idle toy | comm_outcome_gift |
| たまごっち4U > きねんしゃしん (#2) | comm_menu_photo -> comm_photo_session | 4 | 7 photo | 1 | idle toy | comm_outcome_photo |
| たまごっち4U > プロポーズ (#3) | comm_menu_proposal -> comm_session_menu(5) | 5 | 6 proposal | する = 2 (target), うける = 1 (initiator) | the other toy, also in this menu | comm_outcome_marriage (rules comm_can_marry) |
| 4Uアプリ (#2) | comm_menu_app | 6 | 8 app link | 2 target | phone app (pseudo peer 0xF0F0, SNEP) | comm_outcome_app (target only) |
| アイテムSEND (debug comm_main_menu_debug_items #5, P06 low) | comm_menu_item_send | 7 | 8 | 1 initiator | see below | none |
| タッチスポット (#3) | spot_touch_menu_entry (F51) | none | none (reader mode; F51 spot_wait_reader sets x003 = 0) | none | spot / card | F51 |
| hidden "\n" (#4, beyond count) | ir_menu_hidden_session | none | IR ir_session(4) | none | IR | IR |
| idle toy, RF field (main loop) | nfc_idle_touch -> nfc_idle_session -> nfc_idle_target_session | none | 0 auto, adopts peer's 0x4C | 2 | the initiating toy | by adopted type |
| test mode | test_mode -> test_nfc_link | none | 1 link test (x004 = 0xF040, then 0xF0F0) | field: 2, key: 1 | toy / app-side tester | none |

`nfc_menu_session(0)` changes nothing and has no caller. After a type-4 round `nfc_menu_session` clears x003 to 0.

### Phase message codes (byte 0x5E, `comm_build_phase1` / `comm_phase2_code` / `comm_build_phase3`)

Initiator codes are even, target codes odd. Phase 1: type 1 0xE0/E1, type 2 0xA0/A1, types 3..8 0/1. Phase 2 (types 3..8): 3 0x20/21, 4 0x22/23, 5 0x24/25, 6 0x28/29, 7 0x26/27, 8 0x10/11. Phase 3 (types 3..7): 0x90/91. Codes 0x12/0x13 exist for types 9/10, but nothing in this firmware sets those types. The app, as type-8 initiator, must send 0x10 in phase 2 (`comm_check_peer_phase` k = 2).

### Receiving side

* **Who is the target.** Every toy-to-toy menu choice except the proposal makes the toy an initiator. The target is an **idle toy**: `nfc_idle_touch` -> `nfc_idle_session` (x003 = 0) -> `nfc_idle_target_session` -> `comm_exchange_target`. With x003 == 0, `comm_exchange_target` adopts the initiator's byte 0x4C but refuses 4, 6 and 8 (nfc_result = 0x58, result 1):
  * 4 only follows a type-3 round;
  * 6 needs the proposer's own menu choice;
  * 8 is reserved for 4Uアプリ.
* **Mode check.** The initiator compares 0x4C on both records (`comm_check_same_type` via `comm_check_pair_and_type`). The target never compares them: it checks only the message codes (`comm_check_peer_phase`).
* **Proposal.** The proposer (する) is the **target**. It waits 30 s in `nfc_session_target` and attaches its proposal line: `comm_build_header` sets 0x8C/x014 for type 6, role 2, life stage > 2, and `comm_payload_proposal_target` adds section 5. The accepter (うける) is the initiator; it shows the proposer's text and the proposal animation (`comm_outcome_marriage`, role 1).
* **Refusals (`comm_can_marry`).**
  * Same **gender** byte 0x5F (`pet_get_gender`). The F66 comment said "family byte" (Fable's notes); this is now fixed.
  * Either life stage 0x7B <= 2.
  * The peer's 0x86 != 0.
  * Species pair in comm_forbidden_pairs.
  * The message depends on role: the initiator gets "このともだちからは" (k = 2), the target "このともだちには" (k = 1).
* **comm_exchange_kind.** It is set only around the item storage of a type-5 outcome: 4 = giving side (initiator), 5 = receiving side (idle target), then back to 0 (`comm_outcome_gift`). It selects the branches of `comm_item_exchange_result`, `item_receive`, `item_add_to_inventory`, `item_store_received`, and ir/ir_packets / ir/ir_packets, which are shared with the IR code.

### comm_menu_choice / comm_submenu_choice

These are menu **enable callbacks** (`ui_menu_run` calls `cb(idx)` while drawing and moving, and last for the chosen entry):

* `comm_menu_enable`: comm_menu_choice = 1 あいさツーしん, 2 たまごっち4U, 3 4Uアプリ, 4 タッチスポット, 7 debug entries (enabled only when P06 is low, hidden otherwise);
* `comm_submenu_enable`: comm_submenu_choice = 0x13 ツーしんゲーム, 0x14 アイテムこうかん and きねんしゃしん, 0x15 プロポーズ;
* `comm_set_gift_kind` (IR point gift): comm_submenu_choice = 0 / 1.

F70b, F69 and F66 never read them. The only readers are:

* the IR profile block (`flash_write_text_record`, bytes 2/3);
* `ir_locate_partner_pics` (comm_menu_choice == 2);
* `item_discard_from_inventory` / `item_add_to_inventory` (`comm_menu_choice == 1 && comm_submenu_choice == 0x13`, a pair the 4U+ menus never set together);
* the uncalled `comm_peer_allows_game`;
* the IR gift code.

Factory reset clears both (`save_factory_reset`).

## 0xF0EF

**It means "no Tamagotchi".** It does not positively identify a phone. Evidence (lib/nfc/nfc_dep.c, lib/nfc/nfc_dep.c):

* Both detectors set `*b = 0xF0EF` on entry and replace it with 0xF040 only for an ATR_REQ/ATR_RES whose NFCID3 starts F0 40.
* After 3 other frames they set 0xF0EF explicitly.
* On timeout, `nfc_detect_initiator` (target) **returns 0 with 0xF0EF**, so the caller cannot tell "nothing" from "foreign device". `nfc_detect_target` (initiator) returns -4.

The consumers give it a meaning:

* `nfc_idle_target_session`, x003 == 0. It runs only after `nfc_field_seen` saw an RF field, and the wait is 2 s. 0xF0EF -> jingle, result 4 -> `nfc_idle_touch` calls `comm_idle_touch_event` (the idle-touch event). Here it effectively means "something with an RF field that is not a Tamagotchi: phone or reader".
* F51 `spot_wait_reader`: also gated by an RF field; 0xF0EF = touch success.
* `nfc_session_target` (menu target, 30 s, no field pre-check): 0xF0EF = failure (room_view = 0, failure screen).
* `nfc_idle_target_session` in type 4: retry/failure.
* `test_nfc_link`, `comm_exchange_initiator`: anything other than 0xF040/0xF0F0 fails.

The F69 header ("0xF0EF = other/phone") and the `nfc_find_peer_target` comment were fixed accordingly. The app is the separate pseudo peer **0xF0F0**. It is never detected: `nfc_find_peer_target` reports it directly for x003 == 8, or for x003 == 1 with x004 == 0xF0F0 in test mode.

## Corrections made to earlier comments / notes

* F69 `nfc_find_peer_target`: "アイテムSEND" does not reach this function (it is an initiator, which uses `nfc_find_peer_initiator`). Type 1 + 0xF0F0 is the test-mode tester run.
* F69 `test_nfc_link`: this is not the type-0 initiator used by `nfc_idle_target_session`. It is the test-mode link test called only from `test_mode`, with session type 1.
* F66 `comm_outcome_marriage`: the roles were swapped. The accepter (initiator) shows the proposer's text; the proposer (target) plays anim_just_married.
* F66 `comm_can_marry`: 0x5F is the gender byte, and the refusal message depends on role.
* F70b: "mode N" became "session type N" throughout. The header now lists type 0 and 9/10 and the app-link meaning of type 8. The target side's 0x4C handling is described.
* F70b `comm_exchange_initiator`: the initiator detector returns only 0xF040/0xF0EF, so initiators always use raw NFC-DEP.
* F7: every handler now says "menu choice n = session type N". `comm_menu_enable` / `comm_submenu_enable` are described as enable callbacks that set the legacy IR bytes comm_menu_choice/DFA.
* Opus's "type 3 (falls back to 4)" and the old F69 wording ("0x63 = wrong mode") are wrong: 4 is the second round, run after a successful type-3 round (`nfc_session_initiator` / `nfc_idle_target_session` return 0x63).

## Remaining unknowns

* **Debug アイテムSEND (menu choice 7).** It sends a bare type-8 record with the item at flash 0x2070000 (or the default background), using version 3. As initiator it can only find an NFC-DEP target with NFCID3 F0 40. A 4U+ in 4Uアプリ waits on the SNEP path instead, and an idle toy refuses type 8, so no 4U+ can receive it. The intended receiver (a factory or PC tool emulating a toy?) is unknown, and so is what normally sits at 0x2070000.
* **Types 9/10.** Only their phase-2 codes exist. An idle target would adopt them from a peer, skip phases 2 and 3, and have no outcome handler. Probably reserved.
* **The 3-digit number in the test mode.** `test_nfc_link` shows the peer's byte 0x4D as a 3-digit number. For a full record that byte is 4, so the meaning of the display is unclear.
