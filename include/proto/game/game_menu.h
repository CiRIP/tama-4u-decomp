/* Declarations of src/game/game_menu.c: its types, variables and functions. */
#ifndef PROTO_GAME_GAME_MENU_H
#define PROTO_GAME_GAME_MENU_H

#include "structs.h"

struct GameSession {
    u8 active;
    u8 x001;
    u8 x002;
};

struct GameEntry {
    u32 label;
    u32 run;
};

extern struct GameSession game_session;
extern u8 game_menu_cursor;
extern u8 game_menu_cursor_obj;

extern const struct GameEntry game_menu_tbl[];
extern const u32 D_255FA68;
extern const u8 D_255FA5C[];

void game_menu_state(u8 *);
u8 game_menu_draw(int, int, void *);
u8 game_menu_next(int, int, void *);
u8 game_menu(void);

#endif
