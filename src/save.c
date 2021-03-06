/**
 * \file save.c
 * \brief Individual saving functions
 *
 * Copyright (c) 1997 Ben Harrison
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cave.h"
#include "game-world.h"
#include "init.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "monster.h"
#include "object.h"
#include "obj-pile.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "option.h"
#include "player.h"
#include "savefile.h"
#include "store.h"
#include "obj-util.h"
#include "player-history.h"
#include "player-timed.h"
#include "trap.h"


/**
 * Write a description of the character
 */
void wr_description(void)
{
	char buf[1024];

	if (player->is_dead)
		strnfmt(buf, sizeof buf, "%s, dead (%s)",
				op_ptr->full_name,
				player->died_from);
	else
		strnfmt(buf, sizeof buf, "%s, L%d %s %s, at DL%d",
				op_ptr->full_name,
				player->lev,
				player->race->name,
				player->class->name,
				player->depth);

	wr_string(buf);
}


/**
 * Write an "item" record
 */
static void wr_item(const object_type *obj)
{
	size_t i;
	struct brand *b;
	struct slay *s;

	wr_u16b(0xffff);
	wr_byte(ITEM_VERSION);

	/* Location */
	wr_byte(obj->iy);
	wr_byte(obj->ix);

	wr_byte(obj->tval);
	wr_byte(obj->sval);

	wr_s16b(obj->pval);

	wr_byte(obj->number);
	wr_s16b(obj->weight);

	if (obj->artifact) wr_byte(obj->artifact->aidx);
	else wr_byte(0);

	if (obj->ego) wr_byte(obj->ego->eidx);
	else wr_byte(0);

	wr_s16b(obj->timeout);

	wr_s16b(obj->to_h);
	wr_s16b(obj->to_d);
	wr_s16b(obj->to_a);
	wr_s16b(obj->ac);
	wr_byte(obj->dd);
	wr_byte(obj->ds);

	wr_byte(obj->marked);

	wr_byte(obj->origin);
	wr_byte(obj->origin_depth);
	wr_u16b(obj->origin_xtra);
	wr_byte(obj->ignore);

	for (i = 0; i < OF_SIZE; i++)
		wr_byte(obj->flags[i]);

	for (i = 0; i < OF_SIZE; i++)
		wr_byte(obj->known_flags[i]);

	for (i = 0; i < ID_SIZE; i++)
		wr_byte(obj->id_flags[i]);

	for (i = 0; i < OBJ_MOD_MAX; i++) {
		wr_s16b(obj->modifiers[i]);
	}

	/* Write a sentinel byte */
	wr_byte(obj->brands ? 1 : 0);
	for (b = obj->brands; b; b = b->next) {
		wr_string(b->name);
		wr_s16b(b->element);
		wr_s16b(b->multiplier);
		wr_byte(b->known ? 1 : 0);
		wr_byte(b->next ? 1 : 0);
	}

	/* Write a sentinel byte */
	wr_byte(obj->slays ? 1 : 0);
	for (s = obj->slays; s; s = s->next) {
		wr_string(s->name);
		wr_s16b(s->race_flag);
		wr_s16b(s->multiplier);
		wr_byte(s->known ? 1 : 0);
		wr_byte(s->next ? 1 : 0);
	}

	for (i = 0; i < ELEM_MAX; i++) {
		wr_s16b(obj->el_info[i].res_level);
		wr_byte(obj->el_info[i].flags);
	}

	/* Held by monster index */
	wr_s16b(obj->held_m_idx);
	
	wr_s16b(obj->mimicking_m_idx);

	/* Activation and effects*/
	if (obj->activation)
		wr_u16b(obj->activation->index);
	else
		wr_u16b(0);
	wr_u16b(obj->time.base);
	wr_u16b(obj->time.dice);
	wr_u16b(obj->time.sides);

	/* Save the inscription (if any) */
	if (obj->note)
		wr_string(quark_str(obj->note));
	else
		wr_string("");
}


/**
 * Write a monster record (including held or mimicked objects)
 */
static void wr_monster(const monster_type *mon)
{
	size_t j;
	struct object *obj = mon->held_obj; 
	struct object *dummy = object_new();

	wr_s16b(mon->race->ridx);
	wr_byte(mon->fy);
	wr_byte(mon->fx);
	wr_s16b(mon->hp);
	wr_s16b(mon->maxhp);
	wr_byte(mon->mspeed);
	wr_byte(mon->energy);
	wr_byte(MON_TMD_MAX);

	for (j = 0; j < MON_TMD_MAX; j++)
		wr_s16b(mon->m_timed[j]);

	for (j = 0; j < MFLAG_SIZE; j++)
		wr_byte(mon->mflag[j]);

	for (j = 0; j < OF_SIZE; j++)
		wr_byte(mon->known_pstate.flags[j]);

	for (j = 0; j < ELEM_MAX; j++)
		wr_s16b(mon->known_pstate.el_info[j].res_level);

	/* Write mimicked object if any */
	if (mon->mimicked_obj) {
		wr_byte(1);
		wr_item(mon->mimicked_obj);
	} else
		wr_byte(0);

	/* Write all objects, followed by a dummy as a marker */
	while (obj) {
		wr_item(obj);
		obj = obj->next;
	}
	wr_item(dummy);
	object_delete(dummy);
}

/**
 * Write a trap record
 */
static void wr_trap(struct trap *trap)
{
    size_t i;

    wr_byte(trap->t_idx);
    wr_byte(trap->fy);
    wr_byte(trap->fx);
    wr_byte(trap->xtra);

    for (i = 0; i < TRF_SIZE; i++)
		wr_byte(trap->flags[i]);
}

/**
 * Write RNG state
 *
 * There were originally 64 bytes of randomizer saved. Now we only need
 * 32 + 5 bytes saved, so we'll write an extra 27 bytes at the end which won't
 * be used.
 */
void wr_randomizer(void)
{
	int i;

	/* current value for the simple RNG */
	wr_u32b(Rand_value);

	/* state index */
	wr_u32b(state_i);

	/* RNG variables */
	wr_u32b(z0);
	wr_u32b(z1);
	wr_u32b(z2);

	/* RNG state */
	for (i = 0; i < RAND_DEG; i++)
		wr_u32b(STATE[i]);

	/* NULL padding */
	for (i = 0; i < 59 - RAND_DEG; i++)
		wr_u32b(0);
}


/**
 * Write the "options"
 */
void wr_options(void)
{
	int i;

	/* Special Options */
	wr_byte(op_ptr->delay_factor);
	wr_byte(op_ptr->hitpoint_warn);
	wr_u16b(op_ptr->lazymove_delay);

	/* Normal options */
	for (i = 0; i < OPT_MAX; i++) {
		const char *name = option_name(i);
		if (!name)
			continue;

		wr_string(name);
		wr_byte(op_ptr->opt[i]);
   }

	/* Sentinel */
	wr_byte(0);
}


void wr_messages(void)
{
	s16b i;
	u16b num;

	num = messages_num();
	if (num > 80) num = 80;
	wr_u16b(num);

	/* Dump the messages (oldest first!) */
	for (i = num - 1; i >= 0; i--) {
		wr_string(message_str(i));
		wr_u16b(message_type(i));
	}
}


void wr_monster_memory(void)
{
	int r_idx;

	wr_byte(MFLAG_SIZE);

	for (r_idx = 0; r_idx < z_info->r_max; r_idx++) {
		monster_race *r_ptr = &r_info[r_idx];
		monster_lore *l_ptr = &l_list[r_idx];

		/* Names and kill counts */
		if (!r_ptr->name || !l_ptr->pkills) continue;
		wr_string(r_ptr->name);
		wr_u16b(l_ptr->pkills);
	}
	wr_string("No more monsters");
}



void wr_object_memory(void)
{
	int k_idx;

	wr_u16b(z_info->k_max);
	wr_byte(OF_SIZE);
	wr_byte(ID_SIZE);
	wr_byte(OBJ_MOD_MAX);
	wr_byte(ELEM_MAX);
	for (k_idx = 0; k_idx < z_info->k_max; k_idx++) {
		byte tmp8u = 0;
		object_kind *k_ptr = &k_info[k_idx];

		if (k_ptr->aware) tmp8u |= 0x01;
		if (k_ptr->tried) tmp8u |= 0x02;
		if (kind_is_ignored_aware(k_ptr)) tmp8u |= 0x04;
		if (k_ptr->everseen) tmp8u |= 0x08;
		if (kind_is_ignored_unaware(k_ptr)) tmp8u |= 0x10;

		wr_byte(tmp8u);
	}
}


void wr_quests(void)
{
	int i;

	/* Dump the quests */
	wr_u16b(z_info->quest_max);
	for (i = 0; i < z_info->quest_max; i++) {
		wr_byte(player->quests[i].level);
		wr_u16b(player->quests[i].cur_num);
	}
}


void wr_artifacts(void)
{
	int i;
	u16b tmp16u;

	/* Hack -- Dump the artifacts */
	tmp16u = z_info->a_max;
	wr_u16b(tmp16u);
	for (i = 0; i < tmp16u; i++) {
		artifact_type *a_ptr = &a_info[i];
		wr_byte(a_ptr->created);
		wr_byte(a_ptr->seen);
		wr_byte(a_ptr->everseen);
		wr_byte(0);
	}
}


void wr_player(void)
{
	int i;

	wr_string(op_ptr->full_name);

	wr_string(player->died_from);

	wr_string(player->history);

	/* Race/Class/Gender/Spells */
	wr_byte(player->race->ridx);
	wr_byte(player->class->cidx);
	wr_byte(op_ptr->name_suffix);

	wr_byte(player->hitdie);
	wr_byte(player->expfact);

	wr_s16b(player->age);
	wr_s16b(player->ht);
	wr_s16b(player->wt);

	/* Dump the stats (maximum and current and birth) */
	wr_byte(STAT_MAX);
	for (i = 0; i < STAT_MAX; ++i) wr_s16b(player->stat_max[i]);
	for (i = 0; i < STAT_MAX; ++i) wr_s16b(player->stat_cur[i]);
	for (i = 0; i < STAT_MAX; ++i) wr_s16b(player->stat_birth[i]);

	wr_s16b(player->ht_birth);
	wr_s16b(player->wt_birth);
	wr_s16b(0);
	wr_u32b(player->au_birth);

	/* Player body */
	wr_string(player->body.name);
	wr_u16b(player->body.count);
	for (i = 0; i < player->body.count; i++) {
		wr_u16b(player->body.slots[i].type);
		wr_string(player->body.slots[i].name);
	}

	/* Padding */
	wr_u32b(0);

	wr_u32b(player->au);


	wr_u32b(player->max_exp);
	wr_u32b(player->exp);
	wr_u16b(player->exp_frac);
	wr_s16b(player->lev);

	wr_s16b(player->mhp);
	wr_s16b(player->chp);
	wr_u16b(player->chp_frac);

	wr_s16b(player->msp);
	wr_s16b(player->csp);
	wr_u16b(player->csp_frac);

	/* Max Player and Dungeon Levels */
	wr_s16b(player->max_lev);
	wr_s16b(player->max_depth);

	/* More info */
	wr_s16b(0);	/* oops */
	wr_s16b(0);	/* oops */
	wr_s16b(0);	/* oops */
	wr_s16b(0);	/* oops */
	wr_byte(0);
	wr_byte(player->unignoring);
	wr_s16b(player->deep_descent);

	wr_s16b(player->food);
	wr_s16b(player->energy);
	wr_s16b(player->word_recall);
	wr_byte(player->confusing);
	wr_byte(player->searching);

	/* Find the number of timed effects */
	wr_byte(TMD_MAX);

	/* Read all the effects, in a loop */
	for (i = 0; i < TMD_MAX; i++)
		wr_s16b(player->timed[i]);

	/* Total energy used so far */
	wr_u32b(player->total_energy);
	/* # of turns spent resting */
	wr_u32b(player->resting_turn);

	/* Future use */
	for (i = 0; i < 8; i++) wr_u32b(0L);
}


void wr_ignore(void)
{
	size_t i, j, n;

	/* Write number of ignore bytes */
	wr_byte(ignore_size);
	for (i = 0; i < ignore_size; i++)
		wr_byte(ignore_level[i]);

	/* Write ego-item ignore bits */
	wr_u16b(z_info->e_max);
	wr_u16b(ITYPE_SIZE);
	for (i = 0; i < z_info->e_max; i++) {
		bitflag everseen = 0, itypes[ITYPE_SIZE];

		/* Figure out and write the everseen flag */
		if (e_info[i].everseen)
			everseen |= 0x02;
		wr_byte(everseen);

		/* Figure out and write the ignore flags */
		itype_wipe(itypes);
		for (j = ITYPE_NONE; j < ITYPE_MAX; j++)
			if (ego_is_ignored(i, j))
				itype_on(itypes, j);

		for (j = 0; j < ITYPE_SIZE; j++)
			wr_byte(itypes[j]);
	}

	n = 0;
	for (i = 0; i < z_info->k_max; i++)
		if (k_info[i].note)
			n++;

	/* Write the current number of auto-inscriptions */
	wr_u16b(n);

	/* Write the autoinscriptions array */
	for (i = 0; i < z_info->k_max; i++) {
		if (!k_info[i].note)
			continue;
		wr_s16b(i);
		wr_string(quark_str(k_info[i].note));
	}

	return;
}


void wr_misc(void)
{
	/* Random artifact seed */
	wr_u32b(seed_randart);

	/* Write the "object seeds" */
	wr_u32b(seed_flavor);

	/* Special stuff */
	wr_u16b(player->total_winner);
	wr_u16b(player->noscore);

	/* Write death */
	wr_byte(player->is_dead);

	/* Current turn */
	wr_s32b(turn);
}


void wr_player_hp(void)
{
	int i;

	wr_u16b(PY_MAX_LEVEL);
	for (i = 0; i < PY_MAX_LEVEL; i++)
		wr_s16b(player->player_hp[i]);
}


void wr_player_spells(void)
{
	int i;

	wr_u16b(player->class->magic.total_spells);

	for (i = 0; i < player->class->magic.total_spells; i++)
		wr_byte(player->spell_flags[i]);

	for (i = 0; i < player->class->magic.total_spells; i++)
		wr_byte(player->spell_order[i]);
}

static void wr_gear_aux(struct object *gear)
{
	struct object *obj;

	/* Write the inventory */
	for (obj = gear; obj; obj = obj->next) {
		/* Skip non-objects */
		assert(obj->kind);

		/* Write code for equipment or other gear*/
		if (object_is_equipped(player->body, obj))
			wr_byte(EQUIP_CODE);
		else
			wr_byte(INVEN_CODE);

		/* Dump object */
		wr_item(obj);

	}

	/* Write finished code */
	wr_byte(FINISHED_CODE);
}


void wr_gear(void)
{
	wr_gear_aux(player->gear);
	wr_gear_aux(player->gear_k);
}


void wr_stores(void)
{
	int i;

	wr_u16b(MAX_STORES);
	for (i = 0; i < MAX_STORES; i++) {
		const struct store *store = &stores[i];
		struct object *obj;

		/* Save the current owner */
		wr_byte(store->owner->oidx);

		/* Save the stock size */
		wr_byte(store->stock_num);

		/* Save the stock */
		for (obj = store->stock; obj; obj = obj->next)
			wr_item(obj);
	}
}



/**
 * Write the current dungeon terrain features and info flags
 *
 * Note that the cost and when fields of c->squares[y][x] are not saved
 */
static void wr_dungeon_aux(struct chunk *c)
{
	int y, x;
	size_t i;

	byte tmp8u;

	byte count;
	byte prev_char;

	/* Dungeon specific info follows */
	wr_string(c->name ? c->name : "Blank");
	wr_u16b(c->height);
	wr_u16b(c->width);

	/* Run length encoding of c->squares[y][x].info */
	for (i = 0; i < SQUARE_SIZE; i++) {
		count = 0;
		prev_char = 0;

		/* Dump for each grid */
		for (y = 0; y < c->height; y++) {
			for (x = 0; x < c->width; x++) {
				/* Extract the important c->squares[y][x].info flags */
				tmp8u = c->squares[y][x].info[i];

				/* If the run is broken, or too full, flush it */
				if ((tmp8u != prev_char) || (count == MAX_UCHAR)) {
					wr_byte((byte)count);
					wr_byte((byte)prev_char);
					prev_char = tmp8u;
					count = 1;
				} else /* Continue the run */
					count++;
			}
		}

		/* Flush the data (if any) */
		if (count) {
			wr_byte((byte)count);
			wr_byte((byte)prev_char);
		}
	}

	/* Now the terrain */
	count = 0;
	prev_char = 0;

	/* Dump for each grid */
	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			/* Extract a byte */
			tmp8u = c->squares[y][x].feat;

			/* If the run is broken, or too full, flush it */
			if ((tmp8u != prev_char) || (count == MAX_UCHAR)) {
				wr_byte((byte)count);
				wr_byte((byte)prev_char);
				prev_char = tmp8u;
				count = 1;
			} else /* Continue the run */
				count++;
		}
	}

	/* Flush the data (if any) */
	if (count) {
		wr_byte((byte)count);
		wr_byte((byte)prev_char);
	}

	/* Write feeling */
	wr_byte(c->feeling);
	wr_u16b(c->feeling_squares);
	wr_s32b(c->created_at);
}

/**
 * Write the dungeon floor objects
 */
static void wr_objects_aux(struct chunk *c)
{
	int y, x;
	struct object *dummy;

	if (player->is_dead)
		return;
	
	/* Write the objects */
	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			struct object *obj = c->squares[y][x].obj;
			while (obj) {
				wr_item(obj);
				obj = obj->next;
			}
		}
	}

	/* Write a dummy record as a marker */
	dummy = mem_zalloc(sizeof(*dummy));
	wr_item(dummy);
	mem_free(dummy);
}

/**
 * Write the monster list
 */
static void wr_monsters_aux(struct chunk *c)
{
	int i;

	if (player->is_dead)
		return;

	/* Total monsters */
	wr_u16b(cave_monster_max(c));

	/* Dump the monsters */
	for (i = 1; i < cave_monster_max(c); i++) {
		const monster_type *mon = cave_monster(c, i);

		wr_monster(mon);
	}
}

static void wr_traps_aux(struct chunk *c)
{
    int x, y;
	struct trap *dummy;

    if (player->is_dead)
	return;

    wr_byte(TRF_SIZE);

	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			struct trap *trap = c->squares[y][x].trap;
			while (trap) {
				wr_trap(trap);
				trap = trap->next;
			}
		}
	}

	/* Write a dummy record as a marker */
	dummy = mem_zalloc(sizeof(*dummy));
	wr_trap(dummy);
	mem_free(dummy);
}

void wr_dungeon(void)
{
	if (player->is_dead)
		return;

	/* Dungeon specific info follows */
	wr_u16b(player->depth);
	wr_u16b(daycount);
	wr_u16b(player->py);
	wr_u16b(player->px);
	wr_byte(SQUARE_SIZE);

	/* Write caves */
	wr_dungeon_aux(cave);
	wr_dungeon_aux(cave_k);

	/* Compact the monsters */
	compact_monsters(0);
}


void wr_objects(void)
{
	wr_objects_aux(cave);
	wr_objects_aux(cave_k);
}

void wr_monsters(void)
{
	wr_monsters_aux(cave);
	wr_monsters_aux(cave_k);
}

void wr_traps(void)
{
	wr_traps_aux(cave);
	wr_traps_aux(cave_k);
}

/*
 * Write the chunk list
 */
void wr_chunks(void)
{
	int j;

	if (player->is_dead)
		return;

	wr_u16b(chunk_list_max);

	/* Now write each chunk */
	for (j = 0; j < chunk_list_max; j++) {
		struct chunk *c = chunk_list[j];

		/* Write the terrain and info */
		wr_dungeon_aux(c);

		/* Write the objects */
		wr_objects_aux(c);

		/* Write the monsters */
		wr_monsters_aux(c);

		/* Write the traps */
		wr_traps_aux(c);
	}
}


void wr_history(void)
{
	size_t i, j;
	u32b tmp32u = history_get_num();

	wr_byte(HIST_SIZE);
	wr_u32b(tmp32u);
	for (i = 0; i < tmp32u; i++) {
		for (j = 0; j < HIST_SIZE; j++)
			wr_byte(history_list[i].type[j]);
		wr_s32b(history_list[i].turn);
		wr_s16b(history_list[i].dlev);
		wr_s16b(history_list[i].clev);
		wr_byte(history_list[i].a_idx);
		wr_string(history_list[i].event);
	}
}
