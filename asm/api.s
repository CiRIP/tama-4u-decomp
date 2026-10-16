/*
 * The firmware API for downloaded programs (games and outings, notes/findings/tamago.md):
 * they are linked against fixed addresses in 0x2400AD0..0x2401D92, where the original
 * firmware has these functions. Each entry here is a jump to the function, at that address
 * (the .api section starts at 0x2400AD0, see ldscripts/firmware.ld); the functions
 * themselves can be anywhere. tools/apicheck.py checks every entry.
 */
    .section .api, "ax"

    .macro entry addr, func
    .org \addr - 0x2400AD0, 0xff
    xjp \func
    .endm

    entry 0x2400AD0, __udivsi3
    entry 0x2400AFA, __divsi3
    entry 0x2400B20, __umodsi3
    entry 0x2400B2A, __modsi3
    entry 0x2400B50, api_get_btn_a
    entry 0x2400B5A, api_get_btn_b
    entry 0x2400B64, api_get_btn_c
    entry 0x2400B6E, api_set_btn_a
    entry 0x2400B78, api_set_btn_b
    entry 0x2400B82, api_set_btn_c
    entry 0x2400B8C, api_idle_tick
    entry 0x2400B94, api_get_btn_events
    entry 0x2400B9E, api_set_btn_events
    entry 0x2400BA8, api_rand_except
    entry 0x2400BB4, api_shuffle
    entry 0x2400BBA, api_get_happiness
    entry 0x2400BC4, api_get_hunger
    entry 0x2400BCE, api_get_weight
    entry 0x2400BD6, api_get_points
    entry 0x2400BE0, api_add_happiness
    entry 0x2400BF8, api_add_hunger
    entry 0x2400C0A, api_add_weight
    entry 0x2400C22, api_add_points
    entry 0x2400C36, api_get_month
    entry 0x2400C40, api_get_day
    entry 0x2400C4A, api_get_hour
    entry 0x2400C54, api_get_minute
    entry 0x2400C5E, api_delay_ms
    entry 0x2400C68, api_delay_ms_ch5
    entry 0x2400C72, api_timeout_start
    entry 0x2400C7C, api_timeout_start_ch5
    entry 0x2400C86, api_timeout_elapsed
    entry 0x2400C92, api_timeout_elapsed_ch5
    entry 0x2400C9E, api_timeout_clear
    entry 0x2400CA6, api_timeout_clear_ch5
    entry 0x2400CAE, api_play_sound
    entry 0x2400CB8, api_wait_sound
    entry 0x2400CC0, api_new_screen
    entry 0x2400CC6, api_present
    entry 0x2400CCE, api_draw_pet
    entry 0x2400CE8, api_set_scene_busy
    entry 0x2400CF4, api_display_reset
    entry 0x2400CFA, api_object_set_image_sized
    entry 0x2400D12, api_object_set_image
    entry 0x2400D1E, api_object_set_image_clipped
    entry 0x2400D36, api_object_move
    entry 0x2400D42, api_object_move_dirty
    entry 0x2400D4E, api_object_set_image_attr
    entry 0x2400D58, api_object_create
    entry 0x2400D74, api_object_create_plain
    entry 0x2400D90, api_object_draw_glyph
    entry 0x2400D9C, api_object_set_flags
    entry 0x2400DA6, api_scene_game_corner
    entry 0x2400DAE, api_anim_draw_image
    entry 0x2400DC2, api_anim_queue_sound
    entry 0x2400DCA, api_anim_frame
    entry 0x2400DD2, api_draw_other_chara
    entry 0x2400DEC, api_draw_chara_by_id
    entry 0x2400E0A, api_draw_text
    entry 0x2400E28, api_select_other_chara
    entry 0x2400E34, api_anim_draw_companion
    entry 0x2400E4A, api_picture_address
    entry 0x2401BAC, api_scene_room
    entry 0x2401BB4, api_scene_window
    entry 0x2401BBC, api_scene_window_mode
    entry 0x2401BC6, api_scene_window2
    entry 0x2401BCE, api_scene_living
    entry 0x2401BD6, api_scene_living_poops
    entry 0x2401BDE, api_scene_kitchen
    entry 0x2401BE6, api_scene_tama_depa
    entry 0x2401BEE, api_scene_park
    entry 0x2401BF6, api_scene_blank
    entry 0x2401BFE, api_scene_bath
    entry 0x2401C06, api_scene_toilet
    entry 0x2401C0E, api_scene_bedroom
    entry 0x2401C16, api_anim_draw_pet
    entry 0x2401C2C, api_anim_draw_other_chara
    entry 0x2401C42, api_anim_draw_fifth_chara
    entry 0x2401C58, api_anim_draw_digit
    entry 0x2401C6E, api_anim_text_line
    entry 0x2401C82, api_anim_glyph_text
    entry 0x2401C96, api_anim_frame_wait
    entry 0x2401C9E, api_select_fifth_chara
    entry 0x2401CA6, api_pet_sprite_set
    entry 0x2401CDE, api_chara_sprite_set
    entry 0x2401CF0, api_digit_font
    entry 0x2401CF8, api_reaction_anim
    entry 0x2401D6A, api_show_message
    entry 0x2401D72, api_set_pic_slot
    entry 0x2401D82, api_get_pic_slot
    entry 0x2401D92, api_store_item
