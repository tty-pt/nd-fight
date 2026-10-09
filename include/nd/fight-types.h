#ifndef ND_FIGHT_TYPES_H
#define ND_FIGHT_TYPES_H

/*
 * nd/fight-types.h — Shared types, structs, and enums for nd-fight.
 * Contains zero XY_DECLs.
 */

#include <ttypt/xy.h>
#include <nd/xy-types.h>

typedef struct {
	enum color color;
	long ndmg, cdmg;
	unsigned wt;
} hit_t;

enum fighter_flags {
	FF_AGGRO = 1,
};

#endif /* ND_FIGHT_TYPES_H */
