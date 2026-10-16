# Name suggestions

Things that still carry an address name (or a placeholder field name) where the cleanup made
their role clear. Not applied: the names stay as in tama_decomp until decided.

| Current | Suggestion | Why |
|---|---|---|
| `D_25626FC` (linker: load address of `.data`) | `sys_ram_data_load` | `firmware.ld` defines it as `LOADADDR(.data)`; sys_copy_data copies from it |
| `D_2562840` (linker: load address of `.iram`) | `sys_iram_code_load` | `LOADADDR(.iram)`; the start-up copies the RAM code from it |
| `D_0084434` | `snd_isr_saved_mul` | `asm/snd_tick_isr.s` saves the multiply registers AHR / ALR there around the C tick |
| `D_0084271` | `sys_bday_a_held_ticks` | blink ticks the A button has been held on the birthday screen (auto-repeat from the third) |
| `struct Friend.x001` | `owner_id_hi` | with `owner_id_lo` the friend's owner id (friend_find_or_add compares both) |
| `struct FriendBookState.x004..x006` | (unknown) | cleared with the visitor record (visit_record_clear), never read |
| `D_255C7DC` | `iram_draw_mode_table` | jump table of the switch on the object drawing mode in `asm/iram.s` |
| `D_255C668`, `D_255C780` | (unused) | tables that came with the RAM code; nothing reads them |
| `D_255FC10` | `game_treasure_again_text` | "もう1ぽん!" (one more!), shown by the treasure game |
| `D_255FB80` | `game_cake_patterns` | per round: the button sequence and the expected buttons of the cake game |
| `D_255FBD0`, `D_255FBF4` | `game_treasure_scenes`, `game_treasure_order` | the [3][3] scene functions and their order |
| `D_255FA88` | `game_training_rounds` | per round of the training game: target and timing |
| `D_2562458`, `D_25623D8` | `comm_check_perm`, `comm_check_expect` | the permutation and expected-byte tables comm_validate_record checks a record with |
| `D_2535FAC`, `D_2536CD0` | `chara_accessory_slots`, `chara_accessory_xy` | the accessory table and offsets chara_draw_accessory reads |
| `D_0084498` | `town_encounter_handlers` | the six handlers of a town walk encounter (town_places.c calls `D_0084498[n]()`) |
| `chara_sprite_sets[44]` | (which character?) | a sprite set some scenes in anim_scripts.c draw directly (poses 0, 6..9) |
| `D_008426C` (now `sys_obj_ids[4]`) | `sys_bday_digit_objs` | the four digit objects of the birthday screen |
