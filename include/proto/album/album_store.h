/* Declarations of src/album/album_store.c: its types, variables and functions. */
#ifndef PROTO_ALBUM_ALBUM_STORE_H
#define PROTO_ALBUM_ALBUM_STORE_H

#include "structs.h"


extern const u8 D_255C25C[];
extern const u8 D_255C26C[];
extern const u8 D_255C280[];
extern const u8 D_255C290[];
extern const u8 D_255C2A0[];
extern const u8 D_255C2AC[];
extern const u8 D_255C2B4[];

int item_pick_slot_to_overwrite(void);
int album_free_photo_entry(void);
int album_store_record(struct TamagoFile *);

#endif
