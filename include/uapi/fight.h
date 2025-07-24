/* REQUIRES: mortal */

#ifndef FIGHT_H
#define FIGHT_H

#include <nd/type.h>
#include <nd/mortal.h>

enum fighter_flags {
	FF_AGGRO = 1,
	FF_ATTACK = 2,
};

typedef struct {
	unsigned char stat, lvl, lvl_v, wt, flags;
} fighter_skel_t;

typedef struct {
	enum color color;
	short ndmg, cdmg;
	unsigned short wtst;
} hit_t;

typedef struct {
	unsigned target;
	unsigned char klock;
	unsigned flags;
	unsigned short wtso, wtst;
	unsigned lvl, cxp;
	hit_t attack;
} fighter_t;

/* API */

SIC_DECL(short, fight_damage, unsigned, dmg_type, short, dmg, short, def, unsigned, def_type);
SIC_DECL(int, fighter_attack, unsigned, player_ref, sic_str_t, ss, hit_t, hit);

/* SIC */

SIC_DECL(int, on_will_attack, unsigned, ent_ref)
SIC_DECL(int, on_attack, unsigned, ent_ref, hit_t, hit)
SIC_DECL(int, on_hit, unsigned, ent_ref, hit_t, hit)
SIC_DECL(int, on_did_attack, unsigned, player_ref, hit_t, hit)
SIC_DECL(int, on_dodge_attempt, unsigned, player_ref, hit_t, hit)
SIC_DECL(int, on_dodge, unsigned, player_ref, hit_t, hit)

#endif
