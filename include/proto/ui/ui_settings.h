/* Declarations of src/ui/ui_settings.c: its types, variables and functions. */
#ifndef PROTO_UI_UI_SETTINGS_H
#define PROTO_UI_UI_SETTINGS_H

#include "structs.h"


extern const u8 *const ui_settings_labels[];

int ui_settings_menu(void);
int ui_settings_blocked(void);
int ui_settings_theme(void);
int ui_settings_brightness(void);
int ui_settings_sound(void);

#endif
