/* Declarations of src/snd/snd_data.c: its types, variables and functions. */
#ifndef PROTO_SND_SND_DATA_H
#define PROTO_SND_SND_DATA_H

#include "structs.h"

/* the note period tables for MCLK = 30, 18.432, 9.216 MHz, and the sequence data of the
 * sound bank snd_bank (in the code section) */
extern const u16 snd_period_table_main[];
extern const u16 D_24F5318[];
extern const u16 D_24F5394[];
extern const u8 D_24F5410[];

#endif
