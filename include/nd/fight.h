/* fight.h — nd-fight's cross-module API: damage, targeting, skeletons, and the
 * attack-event hooks other modules co-implement.
 *
 * Include this from a module TU that wants to deal damage, query a target, or
 * fire the attack hooks, and NOT from nd-fight's own src/libnd-fight.c
 * without FIGHT_IMPL: an XY_IMPL and an XY_DECL of the same name in one TU
 * collide, which is the direct replacement for the old `SIC_DECL` + `SIC_DEF`
 * pairing in a single file.
 *
 * Usage:
 *
 *     #include <ttypt/xy-mod.h>     // must come first: injects the xy context
 *     #include <nd/xy.h>            // engine service hooks (nd_get, ...)
 *     #include <nd/fight.h>         // this file
 *
 * The consumer does not need to load nd-fight itself -- the engine loads every
 * module in mods.load into one region and XY dispatches by name -- but the
 * engine's mods.load must list fight, or these forward to a provider that is
 * not there.
 *
 * NOTE: this is a MODULE-OWNED header, not an engine one. The old location was
 * `include/uapi/fight.h`; the old `~/nd/module.mk` installed it as
 * `$(PREFIX)/include/nd/fight.h`, so `nd/` is this header's home and it is
 * installed here with `FOLDER := nd`.
 *
 * The old header included `<nd/type.h>`, a file that no longer exists. Its only
 * live content for this module was SIC_DECL/SIC_DEF/SIC_CALL (all now XY_*);
 * `hit_t` stays module-owned here and needs only `enum color` from
 * `<nd/xy-types.h>`, which `<nd/xy.h>` already pulls in.
 */

#ifndef ND_FIGHT_H
#define ND_FIGHT_H

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

#ifndef FIGHT_IMPL

/* API */
XY_DECL(long, fight_damage, unsigned, dmg_type, long, dmg, long, def, unsigned, def_type);
XY_DECL(int, fighter_attack, unsigned, player_ref, hit_t, hit);
XY_DECL(unsigned, fighter_wt, unsigned, ref);
XY_DECL(unsigned, fighter_target, unsigned, ref);
XY_DECL(int, fighter_skel_add,
		unsigned, skid, unsigned char, lvl,
		unsigned char, lvl_v, unsigned char, flags);

/* SIC — attack hooks nd-fight fires and other modules co-implement.
 * on_attack, on_did_attack, on_dodge and on_dodge_attempt have no implementor
 * in-tree; the dispatch finds nothing and returns 0. */
XY_DECL(hit_t, on_will_attack, unsigned, ent_ref, double, dt);
XY_DECL(int, on_attack, unsigned, ent_ref, hit_t, hit);
XY_DECL(int, on_hit, unsigned, ent_ref, hit_t, hit);
XY_DECL(int, on_did_attack, unsigned, player_ref, hit_t, hit);
XY_DECL(int, on_dodge_attempt, unsigned, player_ref, hit_t, hit);
XY_DECL(int, on_dodge, unsigned, player_ref, hit_t, hit);

#endif /* !FIGHT_IMPL */

#endif /* !ND_FIGHT_H */