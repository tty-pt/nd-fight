/* src/libnd-fight.c — nd-fight, ported to libxylem.
 *
 * Owns combat: damage calculation with elemental weaknesses, targeting and
 * lock-on, dodge, XP award, fighter skeletons for mobs, and the attack-event
 * hooks (on_will_attack, on_attack, on_hit, on_did_attack, on_dodge_attempt,
 * on_dodge) that spell and seat co-implement.
 *
 * Original: tty-pt/nd-fight @ 488 lines main.c, from the nd-basics
 * superproject.
 *
 * This TU XY_IMPLs fight_damage, fighter_attack, fighter_wt, fighter_target,
 * fighter_skel_add, on_will_attack, on_hit, on_mortal_life, on_murder,
 * on_before_leave, on_after_enter, on_move, on_add, on_status and dodge (as a
 * plain internal, see below), and so defines FIGHT_IMPL before including its
 * own header: an XY_IMPL and an XY_DECL of the same name in one TU is the XY
 * equivalent of the old SIC_DEF + SIC_DECL collision. The four attack hooks it
 * CALLS but does not implement (on_attack, on_did_attack, on_dodge_attempt,
 * on_dodge) are re-declared manually below, since the guard suppresses the
 * header's copies.
 *
 * fighter_wt, on_will_attack and on_hit are both implemented AND called here.
 * The XY_DECL cannot coexist with the XY_IMPL, so those three dispatch
 * manually through their registered adapters (the _chain helpers). The struct
 * and adapter symbols are emitted by our own XY_IMPLs, so each helper must
 * come after its XY_IMPL.
 *
 * fight_damage is also implemented and called here, but it has no
 * co-implementor in-tree, so the internal call is a direct C call, not a
 * dispatch. dodge() is a plain internal function for the same reason.
 *
 * The old call_verb_to() named a service that never existed (MODS.md: "no such
 * symbols exist anywhere in nd-basics"), so attack and dodge announcements are
 * written directly to the actor, the target and the room.
 *
 * on_icon does not exist here anymore. nd-fight amends icons through nd-core's
 * decorator table (core_icon_decorate), because XY cannot observe a
 * co-implemented chain; see <nd/core.h> and MODS.md §7.
 */

#include <ttypt/xy-mod.h>

#include <nd/xy.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FIGHT_IMPL
#include <nd/fight.h>

#include <nd/attr.h>
#include <nd/level.h>
#define MORTAL_IMPL
#include <nd/mortal.h>
#include <nd/core.h>

/* mortal_damage is called but not implemented here. The MORTAL_IMPL guard
 * above suppresses the header's XY_DECLs (to protect the on_mortal_life and
 * on_murder names this TU XY_IMPLs), so it is re-declared here verbatim. */
XY_DECL(int, mortal_damage, unsigned, killer_ref, unsigned, victim_ref, long, amt);

/* Attack hooks this TU calls but does not implement. The FIGHT_IMPL guard
 * above suppresses the header's XY_DECLs (to protect the names this TU
 * XY_IMPLs), so the called-but-not-implemented ones are re-declared here,
 * verbatim from the header. */
XY_DECL(int, on_attack, unsigned, ent_ref, hit_t, hit);
XY_DECL(int, on_did_attack, unsigned, player_ref, hit_t, hit);
XY_DECL(int, on_dodge_attempt, unsigned, player_ref, hit_t, hit);
XY_DECL(int, on_dodge, unsigned, player_ref, hit_t, hit);

typedef struct {
	unsigned char stat, lvl, lvl_v, flags;
} fighter_skel_t;

typedef struct {
	unsigned target;
	unsigned char klock;
	unsigned flags;
	unsigned lvl, cxp;
} fighter_t;

static unsigned fighter_hd, fighter_skel_hd, act_fight;
static unsigned wt_hit, wt_dodge;

/* API. XY_IMPL both defines the function and emits the dispatch adapter, so
 * each name gets exactly one, with its body -- no forward declarations.
 *
 * Order matters below: XY_IMPL emits a definition, so a caller has to come
 * after its callee. */

static inline void fighter_untarget(unsigned ref, fighter_t *fighter, unsigned loc) {
	unsigned c = nd_iter(HD_CONTENTS, &loc);
	unsigned other_ref;

	while (nd_next(&loc, &other_ref, c)) {
		fighter_t other;

		nd_get(fighter_hd, &other, &other_ref);

		if (other.target != ref)
			continue;

		fighter->klock --;
		other.target = NOTHING;
		nd_put(fighter_hd, &other_ref, &other);
	}
}

XY_IMPL(int, on_before_leave, unsigned, ref)
{
	fighter_t fighter;

	nd_get(fighter_hd, &fighter, &ref);

	if (fighter.target == NOTHING)
		return 0;

	OBJ obj;

	nd_get(HD_OBJ, &obj, &ref);
	fighter_untarget(ref, &fighter, obj.location);
	fighter.target = NOTHING;
	fighter.klock = 0; // FIXME this is here because klock *still* isn't properly managed
	nd_put(fighter_hd, &ref, &fighter);
	return 0;
}

static inline int
fighter_aggro(unsigned ref)
{
	OBJ obj;
	fighter_t fighter;
	unsigned aggro_ref, iklock;

	nd_get(fighter_hd, &fighter, &ref);
	iklock = fighter.klock;

	nd_get(HD_OBJ, &obj, &ref);
	
	unsigned c = nd_iter(HD_CONTENTS, &obj.location);
	while (nd_next(&obj.location, &aggro_ref, c)) {
		fighter_t aggro;

		if (nd_get(fighter_hd, &aggro, &aggro_ref))
			continue;

		if (!(aggro.flags & FF_AGGRO))
			continue;

		fighter.klock ++;
		aggro.target = ref;
		nd_put(fighter_hd, &aggro_ref, &aggro);
	}

	if (iklock != fighter.klock)
		nd_put(fighter_hd, &ref, &fighter);

	return fighter.klock;
}

XY_IMPL(int, on_after_enter, unsigned, ent_ref)
{
	fighter_aggro(ent_ref);
	return 0;
}

static inline unsigned
entity_xp(fighter_t *fighter, fighter_t *victim)
{
	// alternatively (2000/x)*y/x
	if (!fighter->lvl)
		return 0;

	unsigned r = 254 * victim->lvl / (fighter->lvl * fighter->lvl);
	/* The original tested `r < 0` here, which is always false for an
	 * unsigned -- a dead branch. Return r directly. */
	return r;
}

static inline void
fighter_award(unsigned player_ref, fighter_t *fighter, fighter_t *victim)
{
	unsigned xp = entity_xp(fighter, victim);
	unsigned cxp = fighter->cxp;
	nd_printf(player_ref, "You gain %u xp!\n", xp);
	cxp += xp;

	if (cxp >= 1000) {
		attr_award(player_ref, 2 * (cxp / 1000));
		level_up(player_ref, (cxp / 1000));
	}

	fighter->cxp = cxp / 1000;
}

static inline unsigned char
d20(void)
{
	return (random() % 20) + 1;
}

// returns 1 if target dodges
static inline int
dodge_get(unsigned ref)
{
	return d20() < effect(ref, AF_DODGE);
}

/* Combat announcements. The old code called call_verb_to(a, b, wt, msg), but
 * verb_to never existed as a service. "<Actor><msg>" goes to the room,
 * "You<msg>" to the actor, and "<Actor><msg>" to the target. */
static void
say_combat(unsigned actor_ref, unsigned target_ref, char *msg)
{
	OBJ actor;
	char buf[BUFSIZ * 2];
	int len;

	nd_get(HD_OBJ, &actor, &actor_ref);
	nd_printf(actor_ref, "You%s\n", msg);
	if (target_ref != NOTHING && target_ref != actor_ref) {
		OBJ target;
		nd_get(HD_OBJ, &target, &target_ref);
		nd_printf(target_ref, "%s%s\n", actor.name, msg);
	}
	len = snprintf(buf, sizeof(buf), "%s%s\n", actor.name, msg);
	nd_rwrite(actor.location, actor_ref, buf, (size_t)len);
}

/* Bounded string append without snprintf (which -Wformat-truncation cannot
 * prove safe for the ansi escapes). Numbers are formatted into a small temp
 * where %ld provably fits, then appended here. Defined before dodge(), its
 * first user; order matters because XY_IMPL emits definitions. */
static unsigned
buf_put(char *buf, unsigned pos, unsigned size, const char *s)
{
	unsigned len = strlen(s);
	if (pos + len >= size)
		len = size - pos - 1;
	memcpy(buf + pos, s, len);
	pos += len;
	buf[pos] = '\0';
	return pos;
}

static int
dodge(unsigned ref, unsigned target_ref, hit_t hit)
{
	int stuck = on_dodge_attempt(ref, hit);
	char wts[BUFSIZ];
	char extra[BUFSIZ];

	if (stuck || effect(ref, AF_MOV) > 0 || !dodge_get(ref)) {
		return 0;
	}

	on_dodge(ref, hit);
	nd_get(HD_WTS, wts, &hit.wt);
	{
		unsigned p = 0;
		p = buf_put(extra, p, sizeof(extra), "'s ");
		p = buf_put(extra, p, sizeof(extra), wts);
		(void) p;
	}
	say_combat(ref, target_ref, extra);
	return 1;
}

static inline void
notify_attack(unsigned player_ref, unsigned target_ref, sic_str_t ss __attribute__((unused)), hit_t hit)
{
	char buf[BUFSIZ * 2];
	char num[32];
	unsigned i = 0;

	if (hit.ndmg || hit.cdmg) {
		i = buf_put(buf, i, sizeof(buf), " (");

		if (hit.ndmg) {
			snprintf(num, sizeof(num), "%ld", hit.ndmg);
			i = buf_put(buf, i, sizeof(buf), num);
			if (hit.cdmg)
				i = buf_put(buf, i, sizeof(buf), ", ");
		}

		if (hit.cdmg) {
			i = buf_put(buf, i, sizeof(buf), ansi_fg[hit.color]);
			snprintf(num, sizeof(num), "%ld", hit.cdmg);
			i = buf_put(buf, i, sizeof(buf), num);
			i = buf_put(buf, i, sizeof(buf), ANSI_RESET);
		}

		i = buf_put(buf, i, sizeof(buf), ")");
		(void) i;

		say_combat(player_ref, target_ref, buf);
		return;
	}

	say_combat(player_ref, target_ref, " (0)");
}

XY_IMPL(int, on_hit, unsigned, ref, hit_t, hit)
{
	fighter_t fighter;
	sic_str_t ss = { .pos = 0 };

	nd_get(fighter_hd, &fighter, &ref);
	notify_attack(ref, fighter.target, ss, hit);
	mortal_damage(ref, fighter.target, hit.ndmg + hit.cdmg);
	on_did_attack(ref, hit);
	return 0;
}

/* on_hit is XY_IMPL'd above AND called by fighter_attack below. Dispatch
 * manually; see the header comment. */
static int
on_hit_chain(unsigned ref, hit_t hit)
{
	struct on_hit_args args = { ref, hit };
	int ret = 0;
	xy_call(&ret, &on_hit_adapter, &args);
	return ret;
}

static inline long
randd_dmg(long dmg)
{
	register long xx = 1 + (random() & 7);
	return xx = dmg + ((dmg * xx * xx * xx) >> 9);
}

XY_IMPL(long, fight_damage, unsigned, dmg_type, long, dmg,
	long, def, unsigned, def_type)
{
	if (!dmg)
		return 0;

	if (dmg > 0) {
		element_t element;
		nd_get(HD_ELEMENT, &element, &def_type);
		if (dmg_type == element.weakness)
			dmg *= 2;
		else {
			nd_get(HD_ELEMENT, &element, &dmg_type);
			if (element.weakness == def_type)
				dmg /= 2;
		}

		if (dmg < def)
			return 0;

	} else
		// heal TODO make type matter
		def = 0;

	return randd_dmg(dmg - def);
}

XY_IMPL(int, fighter_attack, unsigned, ref, hit_t, hit)
{
	fighter_t fighter;

	on_attack(ref, hit);

	nd_get(fighter_hd, &fighter, &ref);
	if (fighter.target == NOTHING)
		return 0;

	if (dodge(ref, fighter.target, hit))
		return 0;

	on_hit_chain(ref, hit);
	return hit.ndmg + hit.cdmg;
}

XY_IMPL(unsigned, fighter_wt, unsigned, ref)
{
	(void) ref;
	unsigned wt = wt_hit;
	nd_last(&wt);
	return wt;
}

/* fighter_wt is XY_IMPL'd above AND called by on_will_attack below (the
 * weapon-weight chain that race and equip amend). Dispatch manually. */
static unsigned
fighter_wt_chain(unsigned ref)
{
	struct fighter_wt_args args = { ref };
	unsigned ret = 0;
	xy_call(&ret, &fighter_wt_adapter, &args);
	return ret;
}

XY_IMPL(unsigned, fighter_target, unsigned, ref)
{
	fighter_t fighter;
	nd_get(fighter_hd, &fighter, &ref);
	return fighter.target;
}

XY_IMPL(hit_t, on_will_attack, unsigned, ref, double, dt)
{
	fighter_t fighter;
	hit_t hit;

	hit.wt = fighter_wt_chain(ref);
	nd_get(fighter_hd, &fighter, &ref);
	hit.ndmg = 1 + fight_damage(ELM_PHYSICAL,
			effect(ref, AF_DMG),
			effect(fighter.target, AF_DEF)
			+ effect(fighter.target, AF_MDEF),
			dt);
	if (hit.ndmg < 0)
		hit.ndmg = 0;
	hit.cdmg = 0;
	return hit;
}

/* on_will_attack is XY_IMPL'd above AND called by on_mortal_life below (seat
 * and spell co-implement it). Dispatch manually. */
static hit_t
on_will_attack_chain(unsigned ref, double dt)
{
	struct on_will_attack_args args = { ref, dt };
	hit_t ret = { 0 };
	xy_call(&ret, &on_will_attack_adapter, &args);
	return ret;
}

XY_IMPL(int, on_mortal_life, unsigned, ref, double, dt)
{
	fighter_t fighter, target;
	hit_t hit;

	nd_get(fighter_hd, &fighter, &ref);
	if (fighter.target == NOTHING)
		return 0;

	nd_get(fighter_hd, &target, &fighter.target);

	if (target.target == NOTHING) {
		target.target = ref;
		nd_put(fighter_hd, &fighter.target, &target);
	}

	hit = on_will_attack_chain(ref, dt);

	fighter_attack(ref, hit);

	return 0;
}

XY_IMPL(int, on_move, unsigned, player_ref)
{
	fighter_t fighter;

	nd_get(fighter_hd, &fighter, &player_ref);

	if (fighter.klock) {
		nd_printf(player_ref, "You can't move while being targeted.\n");
		return 1;
	}

	return 0;
}

static void
do_fight(int fd, int argc __attribute__((unused)), char *argv[])
{
	unsigned player_ref = fd_player(fd);
	OBJ player, loc, target;
	unsigned target_ref = strcmp(argv[1], "me")
		? ematch_near(player_ref, argv[1])
		: player_ref;

	nd_get(HD_OBJ, &player, &player_ref);
	nd_get(HD_OBJ, &loc, &player.location);
	if (player.location == 0 || (loc.flags & RF_HAVEN)) {
		nd_printf(player_ref, "You can't do that.\n");
		return;
	}

	nd_get(HD_OBJ, &target, &target_ref);
	if (target_ref == NOTHING
	    || player_ref == target_ref
	    || target.type != TYPE_ENTITY)
	{
		nd_printf(player_ref, "You can't do that.\n");
		return;
	}

	fighter_t fighter;
	nd_get(fighter_hd, &fighter, &player_ref);
	fighter.target = target_ref;
	nd_put(fighter_hd, &player_ref, &fighter);
	/* ndc_writef(fd, "You form a combat pose."); */
}

XY_IMPL(int, on_status, unsigned, player_ref)
{
	fighter_t fighter;
	nd_get(fighter_hd, &fighter, &player_ref);
	nd_printf(player_ref, "Fight\tlock %3u\n", fighter.klock);
	return 0;
}

XY_IMPL(int, on_murder, unsigned, ref, unsigned, victim_ref)
{
	fighter_t fighter, victim;

	nd_get(fighter_hd, &victim, &victim_ref);

	if (victim.target && (victim.flags & FF_AGGRO)) {
		fighter_t tartar;
		nd_get(fighter_hd, &tartar, &victim.target);
		tartar.klock --;
		nd_put(fighter_hd, &victim.target, &tartar);
	}

	if (ref == NOTHING)
		return 1;

	nd_get(fighter_hd, &fighter, &ref);
	fighter_award(ref, &fighter, &victim);
	fighter.target = NOTHING;
	nd_put(fighter_hd, &ref, &fighter);
	return 0;
}

static inline void
stats_init(unsigned ref, fighter_t *fighter, fighter_skel_t *skel)
{
	unsigned char stat = skel->stat;
	int lvl = skel->lvl, spend, i = 0, sp,
	    v = skel->lvl_v ? skel->lvl_v : 0xf;

	lvl += random() & v;

	if (!stat)
		stat = 0x1f;

	spend = 1 + lvl;
	char attr_c[] = "acdiwh";
	for (char *c = attr_c; *c; c++, i++)
		if (stat & (1<<i)) {
			sp = random() % spend;
			train(ref, *c, sp);
		}

	fighter->lvl = lvl;
}

XY_IMPL(int, on_add, unsigned, ref, unsigned, type, uint64_t, v)
{
	OBJ obj;
	fighter_t fighter;
	fighter_skel_t skel;

	(void) v;
	if (type != TYPE_ENTITY)
		return 1;

	memset(&fighter, 0, sizeof(fighter));

	nd_get(HD_OBJ, &obj, &ref);
	nd_get(fighter_skel_hd, &skel, &obj.skid);
	fighter.target = NOTHING;

	if (!(obj.flags & OF_PLAYER))
		stats_init(ref, &fighter, &skel);

	nd_put(fighter_hd, &ref, &fighter);
	return 0;
}

/* The on_icon co-implementation is now a decorator. nd-core owns on_icon and
 * runs the registered decorators in order; this one adds the fight action to
 * entities. Registered in xy_install below. */
static struct icon
fight_icon_decorate(struct icon i, unsigned ref, unsigned type,
	unsigned player_ref)
{
	(void) ref;
	(void) player_ref;
	if (type != TYPE_ENTITY)
		return i;

	i.actions |= act_fight;
	return i;
}

XY_IMPL(int, fighter_skel_add,
		unsigned, skid,
		unsigned char, lvl,
		unsigned char, lvl_v,
		unsigned char, flags)
{
	fighter_skel_t fighter_skel = {
		.lvl = lvl,
		.lvl_v = lvl_v,
		.flags = flags,
	};

	nd_put(fighter_skel_hd, &skid, &fighter_skel);
	return 0;
}

XY_MODULE_API void
xy_install(void)
{
	/* Order matches the original mod_install: actions and WTS words first,
	 * then the table opens. */
	act_fight = action_register("fight", "⚔️");
	nd_put(HD_WTS, NULL,  "dodge");
	nd_put(HD_WTS, NULL,  "hit");

	nd_len_reg("fighter", sizeof(fighter_t));
	nd_len_reg("fighter_skel", sizeof(fighter_skel_t));
	// FIXME. If we chane the order of the following two lines, we get horrible bugs
	fighter_hd = (unsigned)nd_open("fighter", "u", "fighter", 0);
	fighter_skel_hd = (unsigned)nd_open("fighter_skel", "u", "fighter_skel", 0);

	nd_register("fight", do_fight, 0);
	nd_get(HD_RWTS, &wt_dodge, "dodge");
	nd_get(HD_RWTS, &wt_hit, "hit");

	core_icon_decorate(fight_icon_decorate);
}