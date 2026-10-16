/* Declarations of src/visit/visit_guests.c: its types, variables and functions. */
#ifndef PROTO_VISIT_VISIT_GUESTS_H
#define PROTO_VISIT_VISIT_GUESTS_H

#include "structs.h"

struct BankLayout {
    u16 slot_size;
    u8 sector;
    u8 slot_count;
};

/* the scheduled visitors' requests (visit_scheduled_visitor) */
struct VisitorRequest {
    u8 kinds[4];
    const u8 *pics[3];
    const u8 *unowned_pic;
    u16 reward_points[3];
    u16 consolation;
    u32 club;
};

extern u16 visit_garden_visitor_ids[][2];

extern const struct BankLayout item_bank_slots_four[];
extern const u8 *const visit_garden_lines[];
extern const u32 visit_garden_flower_items[];
extern const u8 D_25416F0[];
extern const u16 visit_scheduled_charas[][2];
extern const u8 visit_scheduled_lines[][6];
extern const struct VisitorRequest visit_scheduled_requests[];
extern const int visit_merchant_goods[];
extern const u8 D_2541748[];
extern const u8 D_2541738[];
extern const u8 D_2541740[];
extern const struct Inventory item_inventories_four[];
extern const u8 D_2541728[];

void item_copy_affinities(struct TamagoFile *, u8 *);
s8 item_copy_name(struct TamagoFile *, u8 *);
s8 item_strongest_affinity(u8 *);
u16 visit_garden_item_price(int, u8, u8, u8, int, int);
int item_pick_random_owned(const struct Inventory *, int *);
void item_delete_from_bank(int, u32, int);
int item_browse_inventories(const struct Inventory *, u32 *, int);
void visit_garden_visitor(void);
int visit_merchant_browse(int *, int *);
int item_owned_position(const struct Inventory *, int);
int item_code_and_add_copy(const struct Inventory *, int);
u8 visit_merchant_purchase(int *);
void visit_merchant(void);
void visit_scheduled_visitor(void);

#endif
