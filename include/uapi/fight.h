#ifndef FIGHT_H
#define FIGHT_H

#include <nd/type.h>

typedef struct {
	enum color color;
	long ndmg, cdmg;
	unsigned wt;
} hit_t;

enum fighter_flags {
	FF_AGGRO = 1,
};

/* API */

SIC_DECL(long, fight_damage, unsigned, dmg_type, long, dmg, long, def, unsigned, def_type);
SIC_DECL(int, fighter_attack, unsigned, player_ref, hit_t, hit);
SIC_DECL(unsigned, fighter_wt, unsigned, ref);
SIC_DECL(unsigned, fighter_target, unsigned, ref);
SIC_DECL(int, fighter_skel_add,
		unsigned, skid, unsigned char, lvl,
		unsigned char, lvl_v, unsigned char, flags);

/* SIC */

SIC_DECL(hit_t, on_will_attack, unsigned, ent_ref, double, dt);
SIC_DECL(int, on_attack, unsigned, ent_ref, hit_t, hit);
SIC_DECL(int, on_hit, unsigned, ent_ref, hit_t, hit);
SIC_DECL(int, on_did_attack, unsigned, player_ref, hit_t, hit);
SIC_DECL(int, on_dodge_attempt, unsigned, player_ref, hit_t, hit);
SIC_DECL(int, on_dodge, unsigned, player_ref, hit_t, hit);

#endif
