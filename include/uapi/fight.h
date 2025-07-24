/* REQUIRES: mortal */

#ifndef FIGHT_H
#define FIGHT_H

#include <nd/type.h>
#include <nd/mortal.h>

#define DMG_G(v) G(v)
#define DEF_G(v) G(v)
#define DODGE_G(v) G(v)

#define EFFECT(fighter, w) (fighter)->e[AF_ ## w]

enum fighter_flags {
	FF_AGGRO = 1,
	FF_ATTACK = 2,
};

enum affect {
       // these are changed by bufs
       AF_HP,
       AF_MOV,
       AF_MDMG,
       AF_MDEF,
       AF_DODGE,

       // these aren't.
       AF_DMG,
       AF_DEF,

       // these are flags, not types of buf
       AF_NEG = 0x10,
       AF_BUF = 0x20,
};

struct effect {
	short value;
	unsigned char mask;
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
	struct effect e[7];
	unsigned target;
	unsigned char klock;
	unsigned flags;
	unsigned short wtso, wtst;
	unsigned lvl, spend, cxp;
	unsigned attr[ATTR_MAX];
	hit_t attack;
} fighter_t;

/* API */

SIC_DECL(short, fight_damage, unsigned, dmg_type, short, dmg, short, def, unsigned, def_type);
SIC_DECL(int, mcp_stats, unsigned, player_ref);
SIC_DECL(int, fighter_attack, unsigned, player_ref, sic_str_t, ss, hit_t, hit);

/* SIC */

SIC_DECL(int, on_will_attack, unsigned, ent_ref)
SIC_DECL(int, on_attack, unsigned, ent_ref, hit_t, hit)
SIC_DECL(int, on_hit, unsigned, ent_ref, hit_t, hit)
SIC_DECL(int, on_did_attack, unsigned, player_ref, hit_t, hit)
SIC_DECL(int, on_dodge_attempt, unsigned, player_ref, hit_t, hit)
SIC_DECL(int, on_dodge, unsigned, player_ref, hit_t, hit)

SIC_DECL(int, on_reroll, unsigned, player_ref)

#endif
