/* fight.h — nd-fight's cross-module API: damage, targeting, skeletons, and the
 * attack-event hooks other modules co-implement.
 *
 * Caller-facing header. Implementers include <nd/fight-types.h>, not this header.
 */

#ifndef ND_FIGHT_H
#define ND_FIGHT_H

#include <ttypt/xy.h>
#include <nd/fight-types.h>

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

#endif /* !ND_FIGHT_H */
