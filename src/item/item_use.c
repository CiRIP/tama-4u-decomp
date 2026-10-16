/*
 * Using items from the inventory: effects and preference reactions with the item animations,
 * item cards and names, the usable-item pickers and the special-items browser. Holds the item
 * animation tables and the inventory category lists other files read.
 *
 * Code 2404146..24057F8, .rodata from 253BF9C.
 */
#include "proto/item/item_use.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/shop/shop_browser.h"
#include "proto/anim/anim_scripts.h"
#include "proto/evo/evo_counters.h"
#include "proto/comm/comm_menu.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care.h"
#include "proto/spot/spot_reader.h"
#include "proto/comm/comm_outcome.h"
#include "flash.h"
#include "libc.h"

/*
 * Apply the 5 attribute bytes (+0xB8) of an item/game file to the pet (pet_add_traits).
 */
void item_apply_attributes(struct TamagoFile *p)
{
    u8 buf[5];
    int i;

    if (p != 0 && p->x000_h != 0xffff) {
        if (p->x04e == 0x81 || p->x04e == 0x94) {
            for (i = 0; i <= 4; i++)
                buf[i] = p->trait_add[i];
            pet_add_traits(buf);
        }
    }
}

/*
 * Does character id like item p? The item record (category 0x81 or 0x94) lists up to 18
 * big-endian character ids at +0xC0 (bit 15 = dislikes; 1 / 2 = "all of kind 0/1"):
 * returns 1 (likes) or 2 (dislikes); otherwise the 2-bit preference of the character's
 * class c = chara_class(id) (0x25 = none): bits 2 * (c & 3) of byte 0x94 + c / 4 (the
 * code indexes 0x7C + (c + 0x60) / 4), i.e. bytes 0x94..0x9B hold 32 classes. In the
 * download pack the 0xC0 list is always empty and only bytes 0x94..0x9B are used.
 */
int item_chara_preference(struct TamagoFile *p, int id)
{
    struct TamagoFile *q;
    int i;
    u16 v;
    int w;

    if (p != 0 && p->x000_h == 0xffff)
        return 0;
    q = p;
    if (q->x04e == 0x81 || q->x04e == 0x94) {
        for (i = 0; i <= 0x23; i += 2) {
            v = (((u8 *)(i + (int)q))[0xc0] << 8) | ((u8 *)(i + (int)q))[0xc1];
            w = v;
            if ((v & 0x7fff) == 1 && chara_gender(id) == 0)
                goto found;
            w &= 0x7fff;
            if (w == 2 && chara_gender(id) == 1)
                goto found;
            if ((v & 0x7fff) == id) {
            found:
                if (v & 0x8000)
                    return 2;
                return 1;
            }
        }
        if (q->x04e == 0x81 || q->x04e == 0x94) {
            id = chara_class(id);
            if (id == 0x25)
                return 0;
            id += 0x60;
            return (((u8 *)((id >> 2) + (int)q))[0x7c] >> ((id & 3) * 2)) & 3;
        }
    }
    return 0;
}

int item_partner_preference(struct TamagoFile *p)
{
    u8 tmp[8];
    int i;
    u16 v;
    int id;
    struct VisitPartner *e;

    if (p != 0 && p->x000_h != 0xffff && visit_partner.life_stage == 4) {
        if (p->flags & 2) {
            if (p->x04e == 0x81 || p->x04e == 0x94) {
                i = 0;
                e = &visit_partner;
            again:
                v = (((u8 *)(i + (int)p))[0xc0] << 8) | ((u8 *)(i + (int)p))[0xc1];
                if ((v & 0x7fff) == 1 && e->gender == 0)
                    goto found;
                if ((v & 0x7fff) == 2 && e->gender != 0)
                    goto found;
                if ((v & 0x7fff) == e->chara_id) {
                found:
                    if (v & 0x8000)
                        return 2;
                    return 1;
                }
                i += 2;
                if (i <= 0x23)
                    goto again;
            }
        }
        if (p->x04e == 0x81 || p->x04e == 0x94) {
            id = visit_partner.class_code;
            if (id == 0x25)
                return 0;
            id += 0x60;
            return (((u8 *)((id >> 2) + (int)p))[0x7c] >> ((id & 3) * 2)) & 3;
        }
    }
    return 0;
}

/*
 * Like item_chara_preference for the current pet (stage > 2 only; only if the item has flag 2 in
 * +0xFF): 0 = neutral, 1 = likes, 2 = dislikes.
 */
int item_pet_preference_raw(struct TamagoFile *p)
{
    u8 tmp[8];
    struct TamagoFile *q;
    int id;
    int i;
    u16 v;
    int w;

    if (p != 0 && p->x000_h != 0xffff && save.pet.life_stage > 2) {
        q = p;
        if (q->flags & 2) {
            if (q->x04e == 0x81 || q->x04e == 0x94) {
                id = chara_base_id(save.pet.x000);
                for (i = 0; i <= 0x23; i += 2) {
                    v = (((u8 *)(i + (int)q))[0xc0] << 8) | ((u8 *)(i + (int)q))[0xc1];
                    w = v;
                    if ((v & 0x7fff) == 1 && chara_gender(id) == 0)
                        goto found;
                    w &= 0x7fff;
                    if (w == 2 && chara_gender(id) == 1)
                        goto found;
                    if ((v & 0x7fff) == id) {
                    found:
                        if (v & 0x8000)
                            return 2;
                        return 1;
                    }
                }
            }
        }
        if (q->x04e == 0x81 || q->x04e == 0x94) {
            id = chara_class(save.pet.x000);
            if (id == 0x25)
                return 0;
            id += 0x60;
            return (((u8 *)((id >> 2) + (int)q))[0x7c] >> ((id & 3) * 2)) & 3;
        }
    }
    return 0;
}

/*
 * Preference of the current pet for item record p (item_pet_preference_raw).
 */
u8 item_pet_preference(struct TamagoFile *p)
{
    return item_pet_preference_raw(p);
}

/*
 * Preference of the current pet for item number a.
 */
u8 item_pet_preference_by_index(int a)
{
    return item_pet_preference(item_record_by_index(a));
}

/*
 * Happiness effect of item p (+0x7B for downloaded items, +0x63/+0x6D for built-in ones by
 * sub-type), changed by the preference: liked -> 2/3, disliked -> 0.
 */
s8 item_happiness_effect(struct TamagoFile *p)
{
    u8 r = 0;
    struct TamagoFile *q;
    struct TamagoFile *s;

    if (p != 0 && p->x000 != 0xffffffff) {
        q = p;
        if (q->x04e == 1) {
            switch (q->x04f) {
            case 4:
                r = q->legacy_value;
                break;
            case 1:
            case 2:
                r = q->legacy_happiness;
                break;
            }
        } else if (q->x04e == 0x81 || q->x04e == 0x94) {
            r = p->happiness;
        }
    }
    if ((q->x04e == 0x81 && q->x04f == 1) || (q->x04e == 1 && q->x04f == 1)) {
        if (q->sub_kind == 1)
            return 0;
    }
    s = p;
    if (save.pet.life_stage <= 2)
        return r;
    switch (item_pet_preference(s)) {
    case 1:
        if (s->x04e != 0x81)
            return 2;
        if (s->x04f != 1)
            goto two;
        if (s->sub_kind == 2)
            return 3;
    two:
        return 2;
    case 2:
        return 0;
    }
    if ((u8)(s->x04f - 2) <= 1)
        return 1;
    return r;
}

/*
 * Happiness effect of item number a.
 */
s8 item_happiness_effect_by_index(int a)
{
    return item_happiness_effect(item_record_by_index(a));
}

/*
 * Hunger effect of item p (+0x7A, or +0x6C for built-in food records).
 */
int item_hunger_effect(struct TamagoFile *p)
{
    if (p != 0 && p->x000 != 0xffffffff) {
        if ((p->hdr_word & 0xffff0000) == 0x1010000)
            return p->legacy_hunger;
        if (p->x04e == 0x81 || p->x04e == 0x94)
            return p->hunger;
    }
    return 0;
}

/*
 * Hunger effect of item number a.
 */
s8 item_hunger_effect_by_index(int a)
{
    return item_hunger_effect(item_record_by_index(a));
}

/*
 * F18 = ITEM USE: the inventory browsers, the item card, "つかう?" and the effects of
 * using an item. Inventories are index lists (struct Inventory: x004[] global item
 * indexes, x008 count; item_inv_daily.. = にちようひん / おどうぐ / ようふく / アクセサリー /
 * れいぞうこ / ハッピーメール) over the item records item_record_by_index(index). Item record
 * bytes used here: 0x79 = use-animation id (item_use_anim_id), 0xB2 = effect on pet.weight
 * (item_apply_effects), hunger / happiness effects via item_hunger_effect / item_happiness_effect, liking
 * item_pet_preference_by_index (2 dislikes (flag 0x8000 in the item's character list, no happiness
 * effect), 1 likes / favourite, 0 plain).
 *
 * item_use_anim_id: use-animation id (byte 0x79) of item record p, for types 0x81 / 0x94
 * only (else 0, also for a null or erased record).
 */
u8 item_use_anim_id(struct TamagoFile *p)
{
    if (p == 0 || p->x000 == 0xffffffff)
        return 0;
    switch (p->x04e) {
    case 0x81:
    case 0x94:
        return p->use_anim;
    }
    return 0;
}

/*
 * Use-animation number of item a.
 */
u8 item_use_anim_id_by_index(int a)
{
    return item_use_anim_id(item_record_by_index(a));
}

/*
 * Position in inventory list q of the first owned item (item_get_count != 0) whose id
 * (item_get_id) is c, -1 if not owned.
 */
int item_inv_find_owned_id(const struct Inventory *q, u16 c)
{
    int i;

    for (i = 0; i < q->count; i++) {
        if (item_get_count(q->indexes[i]) != 0) {
            if (item_get_id(item_record_by_index(q->indexes[i])) == c)
                return i;
        }
    }
    return -1;
}

/*
 * Apply the effects of item record p: pet.weight (weight) += byte 0xB2
 * (pet_add_weight), hunger hearts + |item_hunger_effect(p)| (pet_feed, added for
 * either sign), happiness + or - item_happiness_effect(p) (pet_add_happiness / pet_sub_happiness).
 */
void item_apply_effects(struct TamagoFile *p)
{
    s8 t;

    pet_add_weight(p->weight);
    t = item_hunger_effect(p);
    if (t > 0)
        pet_feed(t);
    else
        pet_feed(-t);
    t = item_happiness_effect(p);
    if (t > 0)
        pet_add_happiness(t);
    else
        pet_sub_happiness(-t);
}




/*
 * Apply the effects of item number a (item_apply_effects on its record item_record_by_index(a)).
 */
void item_apply_effects_by_index(int a)
{
    item_apply_effects(item_record_by_index(a));
}

/*
 * Can item index a not be used now? 1 when the slot is empty or erased, or for an
 * app-data item (category 0x4F == 8) while pet.app_item_lock is set; else 0.
 */
u8 item_is_unusable(int a)
{
    struct TamagoFile *p = item_record_by_index(a);
    u8 r = 1;

    if (p != 0 && p->x000 != 0xffffffff) {
        if (p->x04f != 8 || save.pet.app_item_lock == 0)
            r = 0;
    }
    return r;
}

/*
 * Is item id a in the coupon range 0x3A9C..0x3AAA or 0x3AB6?
 */
int item_id_is_coupon(u16 a)
{
    int r;

    if ((u16)(a - 0x3a9c) <= 14)
        r = 1;
    else
        r = a == 0x3ab6;
    return r;
}


/*
 * USE item index a: register it (dl_chara_affection_update, evo_count_item_use_young care log, evo_count_item_use
 * stats), then: 1 in 10 the character refuses (anim_item_refused, pet.discipline_flags bit 0); a disliked
 * item (liking 2) plays the refusal animation anim_item_disliked and has no effect; the goods
 * sub-kind 2 items 0x23 / 0x24 / 0x25 reset daily counters (pet.kitchen_dirt.., pet.living_dirt..,
 * pet.bath_dirt); a favourite (liking 1) with a built-in id <= 999 runs its special
 * animation item_liked_anims[k] (k = byte 0x79) and fills a leaf of the four-leaf clover
 * (pet_fill_clover_leaf(4)); nothing is unlocked; else the normal use animation item_use_anims[k] (generated animation
 * scripts) or a default one (anim_happy). Goods (category 4) give +2 / +4 happiness
 * hearts, other items their effects (item_apply_effects_by_index). Ends with item_apply_attributes.
 */
int item_use(int a)
{
    int flag = 0;
    struct TamagoFile *p;
    const u8 *tmp[3];
    int i;
    u8 k;
    u16 v;

    p = item_record_by_index(a);
    dl_chara_affection_update((int)p);
    evo_count_item_use_young(5, p);
    evo_count_item_use(p);
    if (rand() % 10 == flag) {
        tmp[0] = anim_pic_slots[3][0];
        tmp[1] = anim_pic_slots[3][1];
        tmp[2] = anim_pic_slots[3][2];
        for (i = 0; i <= 2; i++)
            anim_pic_slots[3][i] = anim_pic_slots[0][i];
        anim_item_refused();
        for (i = 0; i <= 2; i++)
            anim_pic_slots[3][i] = tmp[i];
        save.pet.discipline_flags |= 1;
        return 1;
    }
    if (item_pet_preference_by_index(a) == 2) {
        anim_item_disliked();
        return 1;
    }
    if (p != 0 && p->x000 != 0xffffffff && p->x04f == 4 && p->sub_kind == 2) {
        switch (item_use_anim_id(p)) {
        case 0x23:
            save.pet.kitchen_dirt = 0;
            save.pet.toilet_dirt = 0;
            break;
        case 0x24:
            save.pet.living_dirt = 0;
            save.pet.bedroom_dirt = 0;
            break;
        case 0x25:
            save.pet.bath_dirt = 0;
            break;
        }
    }
    k = item_use_anim_id_by_index(a);
    if (item_pet_preference_by_index(a) == 1) {
        flag = 1;
        v = p->x05b | (p->x05a << 8);
        if (k < 0x4a && item_liked_anims[k] != 0 && v <= 999) {
            item_liked_anims[k]();
            pet_fill_clover_leaf(4, p);
            if ((p->x04e == 1 || p->x04e == 0x81) && p->x04f == 4)
                pet_add_happiness(4);
            else
                item_apply_effects_by_index(a);
            goto end;
        }
    }
    if (k < 0x4a && item_use_anims[k] != 0) {
        item_use_anims[k]();
        if (!flag) {
            if ((p->x04e == 1 || p->x04e == 0x81) && p->x04f == 4)
                pet_add_happiness(2);
        } else {
            pet_fill_clover_leaf(4, p);
            if ((p->x04e == 1 || p->x04e == 0x81) && p->x04f == 4)
                pet_add_happiness(4);
        }
    } else {
        anim_happy();
        if ((p->x04e == 1 || p->x04e == 0x81) && p->x04f == 4)
            pet_add_happiness(2);
        else
            item_apply_effects_by_index(a);
    }
end:
    item_apply_attributes(item_record_by_index(a));
    return 1;
}

/*
 * The plain variant of item_use for a record pointer: use item record p directly
 * (animation item_use_anims[k] + effects), e.g. a received item.
 */
int item_use_record(struct TamagoFile *p)
{
    struct TamagoFile *q = p;
    u8 k = item_use_anim_id(p);

    if (k < 0x4a && item_use_anims[k] != 0)
        item_use_anims[k]();
    else
        anim_happy();
    if ((q->x04e == 1 || q->x04e == 0x81) && q->x04f == 4)
        pet_add_happiness(2);
    else
        item_apply_effects(p);
    item_apply_attributes(p);
    return 1;
}

/*
 * Draw the name of item record p (at 0x5E for type 0x81 / 0x94 records, 0x5A for the
 * legacy type 1) as a centred text line in sprite a at (b, c). item_draw_name_by_index = by index.
 */
void item_draw_name(struct TamagoFile *p, u8 a, s16 b, u8 c)
{
    u8 *q;

    if (p == 0 || p->x000 == 0xffffffff)
        return;
    if (p->x04e == 0x81 || p->x04e == 0x94)
        q = &p->name;
    else
        q = &p->x05a;
    gfx_draw_text_centered(a, b, c, q, 0, 9);
}

/*
 * Draw the name of item number id.
 */
void item_draw_name_by_index(u8 id, u8 a, s16 b, u8 c)
{
    item_draw_name(item_record_by_index(id), a, b, c);
}



/*
 * THE ITEM CARD for record a (b != 0 = with price): the record's pictures are loaded into
 * anim_pic_slots (item_load_pictures) and laid out per kind: backgrounds (0x81 / category 7) full
 * screen; goods / toys of sub-kind 1 (0x81/4/1), with the coupon pictures for the
 * item_special_class classes 0x61 / 0x62; app-data items (0x81/8: 0x63 / 0x64 / 0x65 icons);
 * program items (0x94/2/1 and 0x94/2/2 = games/apps: the two app icons 0x2199EB0 /
 * 0x219A3D8); clothes (0x81/3) on a mannequin 0x2195C6C; else the item image. Then the
 * name, the price digits
 * (0x2142E38 font) with the points symbol, or "FREE" (D_253C3AC) when 0, and the "NEW"
 * mark 0x21090E0 when item_find_builtin_match says the item is new.
 */
void item_draw_card(struct TamagoFile *a, int b)
{
    u8 c = 0;
    struct TamagoFile *out;
    struct TamagoFile *p;
    int i;
    const u8 *s;
    u8 r;
    u16 n;
    int x;
    int y;
    int j;
    int k;

    p = a;
    item_load_pictures(anim_pic_slots[0], p, 0xf);
    ui_new_screen();
    if ((a->x04e == 0x81 || a->x04e == 1) && p->x04f == 7) {
        gfx_object_create(0, 0x1c, 0, 0, 0, anim_pic_slots[0][0]);
        gfx_object_create(0, 0x1c, 0x20, 0, 0, D_2106CA4);
        goto done;
    }
    if (p->x04e == 0x81)
        if (p->x04f == 4)
            if (p->sub_kind == 1) {
                if (item_special_class(a, (int)&c) != 0) {
                    switch (c) {
                    case 0x61:
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        s = D_219A1E0;
                        gfx_object_create(0x40 - (s[0] >> 1), 0x3f - s[1], 0x10, 0, 0, (const u8 *)s);
                        break;
                    case 0x62:
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        gfx_object_create(0, 0x1c, 0x10, 0, 0, anim_pic_slots[0][0]);
                        gfx_object_create(0, 0x1c, 0x20, 0, 0, D_2107ECC);
                        break;
                    default:
                        gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                        s = D_219A1E0;
                        gfx_object_create(0x40 - (s[0] >> 1), 0x3f - s[1], 0x10, 0, 0, (const u8 *)s);
                        break;
                    }
                } else {
                    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                    s = anim_pic_slots[0][0];
                    gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x20, 0, 0, (const u8 *)s);
                }
                goto done;
            }
    if (p->x04e == 0x81)
        if (p->x04f == 8) {
            if (item_special_class(a, (int)&c) != 0) {
                gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                switch (c) {
                case 0x63:
                    s = (u8 *)&spot_bingo_card_pic;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x3f - s[1], 0x10, 0, 0, (const u8 *)s);
                    break;
                case 0x64:
                    s = (u8 *)&spot_bingo_card_pic;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x3f - s[1], 0x10, 0, 0, (const u8 *)s);
                    break;
                case 0x65:
                default:
                    s = D_2199EB0;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x3c - s[1], 0x10, 0, 0, (const u8 *)s);
                    break;
                }
            }
            goto done;
        }
    if (p->x04e == 0x94)
        if (p->x04f == 2)
            if (p->sub_kind == 1) {
                gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                s = D_2199EB0;
                gfx_object_create(0x40 - (s[0] >> 1), 0x3c - s[1], 0x10, 0, 0, (const u8 *)s);
                goto done;
            }
    if (p->x04e == 0x94)
        if (p->x04f == 2)
            if (p->sub_kind == 2) {
                gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                s = D_219A3D8;
                gfx_object_create(0x40 - (s[0] >> 1), 0x3f - s[1], 0x20, 0, 0, (const u8 *)s);
                goto done;
            }
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    if (p->x04e == 0x81)
        if (p->x04f == 3) {
            s = D_2195C6C;
            gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x10, 0, 0, (const u8 *)s);
            s = anim_pic_slots[2][0];
            gfx_object_create(0x40 - (s[0] >> 1), 0x38 - (s[1] >> 1), 0x20, 0, 0, (const u8 *)s);
            goto done;
        }
    s = anim_pic_slots[0][0];
    gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x20, 0, 0, (const u8 *)s);
done:
    j = 0;
    r = gfx_object_create(j, 0x41, 0x40, j, j, (const u8 *)j);
    if (c == 0x62)
        item_draw_name(a, r, 0x49, 0x40);
    else {
        if (p->x04e == 0x81)
            if (p->x04f == 7) {
                item_draw_name(a, r, 0x44, 0x40);
                goto drawn;
            }
        item_draw_name(a, r, 0x41, 0x40);
    }
drawn:
    n = item_price(a);
    if (b != 0) {
        y = 0x51;
        if (p->x04e == 0x81)
            if (p->x04f == 7)
                y = 0x52;
        if (n != 0) {
            x = 0x43;
            for (i = 0; i <= 4; i++) {
                if (n == 0 && i != 0)
                    break;
                gfx_object_create(x, y, 0x50, n % 10, 0, D_2142E38);
                x -= 10;
                n /= 10;
            }
            gfx_object_create(0x4f, y, 0x40, 0x1a, 0, (const u8 *)&D_2090E0C);
        } else {
            k = 0;
            r = gfx_object_create(k, k, k, k, k, (const u8 *)k);
            gfx_draw_text_centered(r, y, 0x50, D_253C3AC, k, 4);
        }
    }
    if (item_find_builtin_match((int)a, &out) == 1) {
        s = D_21090E0;
        gfx_object_create(0x5c - (s[0] >> 1), 0x2e - (s[1] >> 1), 0x58, 0, 0, (const u8 *)s);
    }
}

/*
 * The item card of item_draw_card for inventory index a (b != 0 = with price): picture,
 * name, owned count (item_spare_count) unless the item is unlimited, price (item_price_by_index), and
 * the meal / snack tags for food (0x81/1/1, 0x81/1/2); index > 0x60 = the "はずす" (take
 * off) entry of the wardrobe.
 */
void item_draw_card_by_index(int a, int b)
{
    struct TamagoFile *p;
    struct TamagoFile *q;
    struct TamagoFile *out;
    int i;
    const u8 *s;
    u8 r;
    u16 n;
    int x;
    int y;
    int j;
    int k;

    q = item_record_by_index(a);
    p = q;
    item_load_pictures(anim_pic_slots[0], p, 0xf);
    ui_new_screen();
    j = 0;
    gfx_object_create(j, 0x1c, j, j, j, D_2104794);
    if ((u32)a <= 0x60) {
        if (q->x04e == 0x81)
            if (q->x04f == 3) {
                s = D_2195C6C;
                gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x10, j, j, (const u8 *)s);
                s = anim_pic_slots[2][0];
                gfx_object_create(0x40 - (s[0] >> 1), 0x38 - (s[1] >> 1), 0x20, j, j, (const u8 *)s);
                goto drawn;
            }
        s = anim_pic_slots[0][0];
        gfx_object_create(0x40 - (s[0] >> 1), 0x30 - (s[1] >> 1), 0x20, 0, 0, (const u8 *)s);
    }
drawn:
    j = 0;
    r = gfx_object_create(j, 0x41, 0x40, j, j, (const u8 *)j);
    if ((u32)a > 0x60) {
        gfx_draw_text_centered(r, 0x41, 0x40, D_253C3B8, j, 9);
    } else {
        item_draw_name_by_index(a, r, 0x41, 0x40);
        if (p->x04e == 0x81)
            if (p->x04f == 6)
                if (p->sub_kind == 3)
                    goto skip;
        if (p->x04e == 0x81)
            if (p->x04f == 4)
                if (p->sub_kind == 1)
                    if (item_get_id(p) == 0x3ab8)
                        goto skip;
        if (p->x04e == 0x81)
            if (p->x04f == 1)
                if (item_get_id(q) == 1)
                    goto skip;
        gfx_object_create(10, 0x22, 0x48, item_spare_count(a), 0, D_208F0FC);
    }
skip:
    n = item_price_by_index(a);
    if (b != 0) {
        y = 0x51;
        if (p->x04e == 0x81)
            if (p->x04f == 7)
                y = 0x52;
        if (n != 0) {
            x = 0x43;
            for (i = 0; i <= 4; i++) {
                if (n == 0 && i != 0)
                    break;
                gfx_object_create(x, y, 0x50, n % 10, 0, D_2142E38);
                x -= 10;
                n /= 10;
            }
            gfx_object_create(0x4f, y, 0x40, 0x1a, 0, (const u8 *)&D_2090E0C);
        } else {
            k = 0;
            r = gfx_object_create(k, k, k, k, k, (const u8 *)k);
            gfx_draw_text_centered(r, 0x51, 0x50, D_253C3AC, k, 4);
        }
    }
    if (p->x04e == 0x81)
        if (p->x04f == 1) {
            switch (p->sub_kind) {
            case 1:
                gfx_object_create(0x69, 0x21, 0x60, 0, 0, D_219AA94);
                break;
            case 2:
                gfx_object_create(0x69, 0x21, 0x60, 0, 0, D_219AB2C);
                break;
            }
        }
    if (item_find_builtin_match((int)q, &out) == 1) {
        s = D_21090E0;
        gfx_object_create(0x5c - (s[0] >> 1), 0x2e - (s[1] >> 1), 0x6a, 0, 0, (const u8 *)s);
    }
}


/*
 * "つかう? はい / いいえ / けす" (use it? yes / no / delete) for item index a: yes ->
 * item_use and returns 1 (refused, snd_error_beep, when item_is_unusable says unusable);
 * delete -> "ほんとうに?" confirmation (ui_confirm_really), then the ownership count is
 * cleared (item_set_count(a, 0)). Returns 0 otherwise.
 */
u8 item_use_dialog(int a)
{
    int i;
    int n = 3;
    u8 h;
    u8 t;

    ui_new_screen();
    i = 0;
    gfx_object_create(i, 0x1c, i, i, i, D_2104794);
    t = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_centered(t, 0x20, 0x30, D_253C3C0, 0, 9);
    t = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_line(t, 0x18, 0x30, 0x30, (int)D_253C3CC, 0, 8);
    t = gfx_object_create(0x18, 0x30, 0x30, 0, 0, 0);
    gfx_draw_text_line(t, 0x18, 0x40, 0x30, (int)D_253C3D4, 0, 8);
    t = gfx_object_create(0x18, 0x30, 0x30, 0, 0, 0);
    gfx_draw_text_line(t, 0x18, 0x50, 0x30, (int)D_253C3DC, 0, 8);
    h = gfx_object_create(6, 0x32, 0x20, 0, 0, D_2104714);
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 1;
    btn_c_presses = 0;
    for (;;) {
        if (btn_a_presses != 0) {
            i = (u8)(i + 1);
            if (i >= n)
                i = 0;
            gfx_object_move_dirty(h, 6, i * 16 + 0x32);
            func_00009A6();
            ui_timeout = 10;
            btn_a_presses = 0;
        }
        if (btn_b_presses > 1) {
            btn_b_presses = 1;
            ui_timeout = 10;
            switch (i) {
            case 0:
                if (item_is_unusable(a)) {
                    snd_error_beep();
                    break;
                }
                snd_play(2);
                item_use(a);
                ui_timeout = 10;
                return 1;
            case 1:
                snd_play(2);
                return 0;
            case 2:
                snd_play(2);
                if (ui_confirm_really() == 0)
                    goto ret0;
                item_set_count(a, 0);
                return 0;
            }
        }
        if (btn_c_presses != 0) {
            ui_timeout = 10;
            return 0;
        }
        sys_idle_tick();
        {
            int c = ui_timeout;
            if (c != 0)
                continue;
        }
        sys_idle_seconds = 10;
    ret0:
        return 0;
    }
}

/*
 * The nine special built-in items granted by the flags 0x14..0x1C (records
 * item_builtin_records[D_253C014[i]]; unclear: lottery prizes etc. (Fable) or the "ハッピーメール"
 * list (Opus)): item_special_count = how many of the flags are set, item_special_next = next,
 * item_special_show / item_special_browser = their browsers (B = use through item_use_record).
 */
int item_special_count(void)
{
    int n = 0;
    int i;

    for (i = 0x14; i <= 0x1c; i++)
        if (pet_get_unlock_flag(i))
            n++;
    return n;
}

/*
 * Next set flag after a in 0x14..0x1C (wraps).
 */
int item_special_next(int a)
{
    do {
        if (++a > 8)
            a = 0;
    } while (pet_get_unlock_flag(a + 0x14) == 0);
    return a;
}

/*
 * Show item card p (if any flag 0x14..0x1C is set) and wait for a button.
 */

int item_special_show(struct TamagoFile *p)
{
    int k = 0;
    u8 i;

    switch (item_special_count()) {
    case 0:
        snd_error_beep();
        btn_a_presses = 1;
        btn_c_presses = 1;
        return -1;
    case 1:
        k = 0;
    }
    btn_b_presses = 0;
    btn_c_presses = 0;
    if (visit_partner.mode == 2) {
        snd_wait_end_b(0);
        snd_wait_end_b(1);
    }
    i = 0;
    item_draw_card(p, i);
    if (k != 0) {
        if (i < item_special_next(i))
            gfx_object_create(0x72, 0x58, 0x30, i, i, D_21059A4);
        else
            gfx_object_create(0x72, 0x58, 0x30, 1, 0, D_21059A4);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses) {
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses)
            return -1;
        if (btn_c_presses)
            return -1;
        sys_idle_tick();
    } while (ui_timeout);
    return -1;
}

/*
 * Browse the items unlocked by the flags 0x14..0x1C (built-in item records
 * item_builtin_records[D_253C014[i]]); A = next, B = use (item_use_record), C = back.
 */
int item_special_browser(void)
{
    int k = 1;
    u8 i;

    switch (item_special_count()) {
    case 0:
        snd_error_beep();
        btn_a_presses = 1;
        btn_c_presses = 1;
        return -1;
    case 1:
        k = 0;
    }
    btn_b_presses = 0;
    btn_c_presses = 0;
    i = item_special_next(8);
again:
    item_draw_card((struct TamagoFile *)item_builtin_records[D_253C014[i]], 0);
    if (k != 0) {
        if (i < item_special_next(i))
            gfx_object_create(0x72, 0x58, 0x30, 0, 0, D_21059A4);
        else
            gfx_object_create(0x72, 0x58, 0x30, 1, 0, D_21059A4);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses) {
            if (k != 0) {
                i = item_special_next(i);
                goto again;
            }
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            item_use_record((struct TamagoFile *)item_builtin_records[D_253C014[i]]);
            ui_timeout = 10;
            return 0;
        }
        if (btn_c_presses)
            break;
        sys_idle_tick();
    } while (ui_timeout);
    return -1;
}

/*
 * Number of usable (owned, spare: not worn, item_spare_count) items in inventory list q.
 */
int item_inv_usable_count(const struct Inventory *q)
{
    int n = 0;
    int i;

    for (i = 0; i < q->count; i++)
        if (item_spare_count(q->indexes[i]))
            n++;
    return n;
}

/*
 * Number of items of list q owned at all (item_get_count), worn ones included.
 */
int item_inv_owned_count(const struct Inventory *q)
{
    int n = 0;
    int i;

    for (i = 0; i < q->count; i++)
        if (item_get_count(q->indexes[i]))
            n++;
    return n;
}

/*
 * Next usable (spare, item_spare_count) position in list ring after position i (wraps).
 */
int item_inv_next_usable(const struct Inventory *ring, int i)
{
    do {
        i++;
        if (i >= ring->count)
            i = 0;
    } while (item_spare_count(ring->indexes[i]) == 0);
    return i;
}

/*
 * Inventory browser (item_inv_picker).
 */
int item_inv_picker_b(const struct Inventory *p, u8 b, u32 *c)
{
    return item_inv_picker(p, b, c);
}


/*
 * THE INVENTORY PICKER over item list p: shows the item card of each usable item
 * (item_draw_card_by_index); A = next usable item, B = return the item index (b == 0) or run the
 * "つかう?" action menu item_use_dialog first (b != 0, returning the index only when used),
 * C = -1, timeout = -2; *q keeps the cursor (-1 = start at the first).
 * item_inv_picker_b is an alias.
 */
int item_inv_picker(const struct Inventory *p, u8 b, u32 *q)
{
    int flag = 1;
    s8 c;
    int n;

    switch (item_inv_usable_count(p)) {
    case 0:
        snd_error_beep();
        btn_a_presses = 1;
        btn_c_presses = 1;
        return -1;
    case 1:
        flag = 0;
    }
    btn_b_presses = 0;
    btn_c_presses = 0;
    if ((int)*q < 0)
        *q = item_inv_next_usable(p, p->count);
again:
    item_record_by_index(p->indexes[*q]);
    item_draw_card_by_index(p->indexes[*q], shop_get_discount_mode());
    if (flag) {
        if ((int)*q < item_inv_next_usable(p, *q))
            gfx_object_create(0x72, 0x58, 0x30, 0, 0, D_21059A4);
        else
            gfx_object_create(0x72, 0x58, 0x30, 1, 0, D_21059A4);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses != 0) {
            if (flag) {
                *q = item_inv_next_usable(p, *q);
                goto again;
            }
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            ui_timeout = 10;
            c = p->indexes[*q];
            if (b == 0)
                return c;
            if (item_use_dialog(c) != 0)
                return c;
            if (ui_timeout == 0)
                break;
            n = item_inv_usable_count(p);
            if (n == 0)
                return -1;
            if (n == 1)
                flag = 0;
            if (item_get_count(c) == 0)
                *q = item_inv_next_usable(p, *q);
            ui_timeout = 10;
            goto again;
        }
        if (btn_c_presses != 0)
            break;
        sys_idle_tick();
    } while (ui_timeout != 0);
    if (ui_timeout != 0)
        return -1;
    return -2;
}

/*
 * Find the built-in item record with id in class k (index range item_builtin_class_ranges[k] over the ROM
 * item table item_builtin_records; class 2 also searches entries 45..46); -1 if not found.
 */
struct TamagoFile *item_builtin_find(int k, int id)
{
    struct TamagoFile *q;
    int i;
    int r;
    u8 lo;
    u8 hi;
    const struct IndexRange *t;

    r = -1;
    t = &item_builtin_class_ranges[k];
    hi = t->last;
    lo = t->first;
    for (i = lo; i <= hi; i++) {
        q = item_builtin_records[i];
        r = item_get_id(q);
        if (r == id)
            break;
        r = -1;
    }
    if (r == -1 && k == 2) {
        lo = 45;
        hi = 46;
        for (i = lo; i <= hi; i++) {
            q = item_builtin_records[i];
            r = item_get_id(q);
            if (r == id)
                break;
            r = -1;
        }
    }
    if (r == -1)
        q = (struct TamagoFile *)r;
    return q;
}

/*
 * item_use_anims (in this data block): item use animations (0x4A function pointers into the
 * generated scripts of F0), indexed by the animation byte +0x79 of an item record; a
 * second table item_liked_anims holds the "liked item" variants (item_use).
 */
const s8 D_253BFA0[] = {
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15
};
const s8 D_253BFB4[] = { 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };
const s8 D_253BFC0[] = {
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
    0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38
};
const s8 D_253BFD4[] = {
    0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42,
    0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
    0x4d, 0x4e, 0x4f, 0x50, 0x51
};
const s8 D_253BFF0[] = {
    0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42,
    0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
    0x4d, 0x4e, 0x4f, 0x50, 0x51
};
const s8 D_253C00C[] = { 0x5d, 0x5e, 0x5f, 0x60 };
const s8 D_253C010[] = { 0x52 };
const u8 D_253C014[] = { 0x0f, 0x10, 0x11, 0x12, 0x13, 0x29, 0x2a, 0x2b, 0x2c };
void (*const item_use_anims[])(void) = {
    0, 0, anim_use_item_2, anim_use_item_3,
    anim_use_item_4, anim_use_item_5, anim_use_item_6, anim_use_item_7,
    anim_use_item_8, anim_use_item_9, anim_use_item_10, 0,
    0, 0, anim_use_item_14, anim_use_item_15,
    anim_use_item_16, anim_use_item_17, anim_use_item_18, 0,
    anim_use_item_20, anim_use_item_21, anim_use_item_22, anim_use_item_23,
    anim_use_item_24, anim_use_item_25, anim_use_item_26, anim_use_item_27,
    anim_use_item_28, anim_use_item_29, anim_use_item_30, anim_use_item_31,
    anim_use_item_32, anim_use_item_33, anim_use_item_34, 0,
    0, 0, 0, 0,
    0, anim_use_item_41, 0, 0,
    anim_use_item_44, 0, 0, 0,
    0, 0, anim_use_item_50, anim_use_item_51,
    anim_use_item_52, anim_use_item_53, anim_use_item_54, anim_use_item_55,
    anim_use_item_56, anim_use_item_57, anim_use_item_58, anim_use_item_59,
    anim_use_item_60, anim_use_item_61, anim_use_item_62, anim_use_item_63,
    anim_use_item_64, anim_use_item_65, anim_use_item_66, anim_use_item_67,
    anim_use_item_68, anim_use_item_69, anim_use_item_70, anim_use_item_71,
    anim_use_item_72, anim_use_item_73
};
void (*const item_liked_anims[])(void) = {
    0, 0, 0, 0,
    0, anim_use_item_liked_5, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    anim_use_item_liked_20, 0, 0, 0,
    anim_use_item_liked_24, anim_use_item_liked_25, 0, anim_use_item_liked_27,
    0, 0, 0, anim_use_item_liked_31,
    0, anim_use_item_liked_33, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, anim_use_item_liked_51,
    anim_use_item_liked_52, anim_use_item_liked_53, anim_use_item_liked_54, anim_use_item_liked_55,
    anim_use_item_liked_56, anim_use_item_liked_57, anim_use_item_liked_58, anim_use_item_liked_59,
    anim_use_item_liked_60, anim_use_item_liked_61, anim_use_item_liked_62, anim_use_item_liked_63,
    anim_use_item_liked_64, anim_use_item_liked_65, anim_use_item_liked_66, anim_use_item_liked_67,
    anim_use_item_liked_68, anim_use_item_liked_69, 0, 0,
    0, 0
};
const u8 D_253C270[] = _("にちようひん");
const u8 D_253C280[] = _("おどうぐ");
const u8 D_253C28C[] = _("ようふく");
const u8 D_253C298[] = _("アクセサリー");
const u8 D_253C2A8[] = _("れいぞうこ");
const u8 D_253C2B4[] = _("ハッピーメール");
const u8 D_253C2C4[] = _("");
/* the item inventories by category (INV_DAILY ..; item_inv_daily .. name them) */
const struct Inventory item_inventories[7] = {
    { D_253C270, D_253BFF0, 25 },   /* item_inv_daily */
    { D_253C280, D_253BFD4, 25 },   /* item_inv_tools */
    { D_253C28C, D_253BFB4, 10 },   /* item_inv_clothes */
    { D_253C298, D_253BFC0, 20 },   /* item_inv_accessories */
    { D_253C2A8, D_253BFA0, 20 },   /* item_inv_fridge */
    { D_253C2B4, D_253C010, 1 },    /* item_inv_stamps */
    { D_253C2C4, D_253C00C, 4 },    /* item_inv_app_items */
};
const struct PicPair visit_reward_pics[] = {
    { D_21938F4, 0 },
    { D_214DF50, 0 },
    { D_214FB14, 0 },
    { D_214E338, 0 },
    { D_2193A38, D_2193B68 }
};
const u8 D_253C344[] = {
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01,
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00
};
const u8 D_253C3AC[] = _("FREE");
const u8 D_253C3B8[] = _("はずす");
const u8 D_253C3C0[] = _("つかう?");
const u8 D_253C3CC[] = _("はい");
const u8 D_253C3D4[] = _("いいえ");
const u8 D_253C3DC[] = _("けす");
const struct IndexRange item_builtin_class_ranges[] = {
    { 0x46, 0x5f },
    { 0x60, 0x7f },
    { 0x0f, 0x2c },
    { 0x00, 0x00 },
    { 0x00, 0x0e },
    { 0x3e, 0x45 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x2f, 0x3d },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
    { 0x00, 0x00 }
};

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 item_no_full_dialog DSTRAM;
const u8 *anim_pic_slots[6][3] DSTRAM;
