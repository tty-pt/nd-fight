#include "./include/uapi/fight.h"

#include <stdlib.h>
#include <stdio.h>

#include <nd/nd.h>
#include <nd/level.h>
#include <nd/attr.h>
#include <nd/mortal.h>
#include <nd/verb.h>

typedef struct {
	unsigned char stat, lvl, lvl_v, flags;
} fighter_skel_t;

typedef struct {
	unsigned target;
	unsigned char klock;
	unsigned flags;
	unsigned lvl, cxp;
} fighter_t;

unsigned fighter_hd, fighter_skel_hd, bcp_stats, act_fight;
unsigned wt_hit, wt_dodge;

/* API */

SIC_DEF(long, fight_damage, unsigned, dmg_type, long, dmg, long, def, unsigned, def_type);
SIC_DEF(int, fighter_attack, unsigned, player_ref, hit_t, hit);
SIC_DEF(unsigned, fighter_wt, unsigned, ref);
SIC_DEF(unsigned, fighter_target, unsigned, ref);
SIC_DEF(int, fighter_skel_add,
		unsigned, skid, unsigned char, lvl,
		unsigned char, lvl_v, unsigned char, flags);

/* SIC */

SIC_DEF(hit_t, on_will_attack, unsigned, ent_ref, double, dt);
SIC_DEF(int, on_attack, unsigned, ent_ref, hit_t, hit);
SIC_DEF(int, on_hit, unsigned, ent_ref, hit_t, hit);
SIC_DEF(int, on_did_attack, unsigned, player_ref, hit_t, hit);
SIC_DEF(int, on_dodge_attempt, unsigned, player_ref, hit_t, hit);
SIC_DEF(int, on_dodge, unsigned, player_ref, hit_t, hit);

static inline void fighter_untarget(unsigned ref, fighter_t *fighter, unsigned loc) {
	nd_cur_t c = nd_iter(HD_CONTENTS, &loc);
	unsigned other_ref;

	while (nd_next(&loc, &other_ref, &c)) {
		fighter_t other;

		nd_get(fighter_hd, &other, &other_ref);

		if (other.target != ref)
			continue;

		fighter->klock --;
		other.target = NOTHING;
		nd_put(fighter_hd, &other_ref, &other);
	}
}

int on_before_leave(unsigned ref) {
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
	
	nd_cur_t c = nd_iter(HD_CONTENTS, &obj.location);
	while (nd_next(&obj.location, &aggro_ref, &c)) {
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

int on_after_enter(unsigned ent_ref) {
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
	if (r < 0)
		return 0;
	else
		return r;
}

static inline void
fighter_award(unsigned player_ref, fighter_t *fighter, fighter_t *victim)
{
	unsigned xp = entity_xp(fighter, victim);
	unsigned cxp = fighter->cxp;
	nd_writef(player_ref, "You gain %u xp!\n", xp);
	cxp += xp;

	if (cxp >= 1000) {
		call_attr_award(player_ref, 2 * (cxp / 1000));
		call_level_up(player_ref, (cxp / 1000));
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
	return d20() < call_effect(ref, AF_DODGE);
}

int
dodge(unsigned ref, unsigned target_ref, hit_t hit)
{
	int stuck = call_on_dodge_attempt(ref, hit);
	char wts[BUFSIZ];
	char extra[BUFSIZ];

	if (stuck || call_effect(ref, AF_MOV) > 0 || !dodge_get(ref)) {
		return 0;
	}

	call_on_dodge(ref, hit);
	nd_get(HD_WTS, wts, &hit.wt);
	snprintf(extra, sizeof(extra), "'s %s", wts);
	call_verb_to(target_ref, ref, wt_dodge, extra);
	return 1;
}

static inline void
notify_attack(unsigned player_ref, unsigned target_ref, sic_str_t ss __attribute__((unused)), hit_t hit)
{
	char buf[BUFSIZ * 2];
	unsigned i = 0;

	if (hit.ndmg || hit.cdmg) {
		buf[i++] = ' ';
		buf[i++] = '(';

		if (hit.ndmg)
			i += snprintf(&buf[i], sizeof(buf) - i, "%ld%s", hit.ndmg, hit.cdmg ? ", " : "");

		if (hit.cdmg)
			i += snprintf(&buf[i], sizeof(buf) - i, "%s%ld%s", ansi_fg[hit.color], hit.cdmg, ANSI_RESET);

		buf[i++] = ')';

		buf[i] = '\0';

		call_verb_to(player_ref, target_ref, hit.wt, buf);
		return;
	}

	call_verb_to(player_ref, target_ref, hit.wt, " (0)");
}

int on_hit(unsigned ref, hit_t hit) {
	fighter_t fighter;
	sic_str_t ss = { .pos = 0 };

	nd_get(fighter_hd, &fighter, &ref);
	notify_attack(ref, fighter.target, ss, hit);
	call_mortal_damage(ref, fighter.target, hit.ndmg + hit.cdmg);
	call_on_did_attack(ref, hit);
	return 0;
}

static inline long
randd_dmg(long dmg)
{
	register long xx = 1 + (random() & 7);
	return xx = dmg + ((dmg * xx * xx * xx) >> 9);
}

long
fight_damage(unsigned dmg_type, long dmg,
	long def, unsigned def_type)
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

int fighter_attack(unsigned ref, hit_t hit) {
	fighter_t fighter;

	call_on_attack(ref, hit);

	nd_get(fighter_hd, &fighter, &ref);
	if (fighter.target == NOTHING)
		return 0;

	if (dodge(ref, fighter.target, hit))
		return 0;

	call_on_hit(ref, hit);
	return hit.ndmg + hit.cdmg;
}

unsigned fighter_wt(unsigned ref __attribute__((unused))) {
	unsigned wt = wt_hit;
	sic_last(&wt);
	return wt;
}

unsigned fighter_target(unsigned ref) {
	fighter_t fighter;
	nd_get(fighter_hd, &fighter, &ref);
	return fighter.target;
}

hit_t on_will_attack(unsigned ref, double dt) {
	fighter_t fighter;
	hit_t hit;

	hit.wt = call_fighter_wt(ref);
	nd_get(fighter_hd, &fighter, &ref);
	hit.ndmg = 1 + fight_damage(ELM_PHYSICAL,
			call_effect(ref, AF_DMG),
			call_effect(fighter.target, AF_DEF)
			+ call_effect(fighter.target, AF_MDEF),
			dt);
	if (hit.ndmg < 0)
		hit.ndmg = 0;
	hit.cdmg = 0;
	return hit;
}

int
on_mortal_life(unsigned ref, double dt)
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

	hit = call_on_will_attack(ref, dt);

	fighter_attack(ref, hit);

	return 0;
}

int on_move(unsigned player_ref) {
	fighter_t fighter;

	nd_get(fighter_hd, &fighter, &player_ref);

	if (fighter.klock) {
		nd_writef(player_ref, "You can't move while being targeted.\n");
		return 1;
	}

	return 0;
}

void
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
		nd_writef(player_ref, CANTDO_MESSAGE);
		return;
	}

	nd_get(HD_OBJ, &target, &target_ref);
	if (target_ref == NOTHING
	    || player_ref == target_ref
	    || target.type != TYPE_ENTITY)
	{
		nd_writef(player_ref, CANTDO_MESSAGE);
		return;
	}

	fighter_t fighter;
	nd_get(fighter_hd, &fighter, &player_ref);
	fighter.target = target_ref;
	nd_put(fighter_hd, &player_ref, &fighter);
	/* ndc_writef(fd, "You form a combat pose."); */
}

int
on_status(unsigned player_ref)
{
	fighter_t fighter;
	nd_get(fighter_hd, &fighter, &player_ref);
	nd_writef(player_ref, "Fight\tlock %3u\n", fighter.klock);
	return 0;
}

int on_murder(unsigned ref, unsigned victim_ref) {
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
			call_train(ref, *c, sp);
		}

	fighter->lvl = lvl;
}

int on_add(unsigned ref, unsigned type, uint64_t v __attribute__((unused))) {
	OBJ obj;
	fighter_t fighter;
	fighter_skel_t skel;

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

struct icon on_icon(unsigned ref __attribute__((unused)),
		unsigned type, unsigned player_ref __attribute__((unused)))
{
	struct icon i;

	sic_last(&i);
	if (type != TYPE_ENTITY)
		return i;

	i.actions |= act_fight;
	return i;
}

int fighter_skel_add(
		unsigned skid,
		unsigned char lvl,
		unsigned char lvl_v,
		unsigned char flags)
{
	fighter_skel_t fighter_skel = {
		.lvl = lvl,
		.lvl_v = lvl_v,
		.flags = flags,
	};

	nd_put(fighter_skel_hd, &skid, &fighter_skel);
	return 0;
}

void mod_open(void) {
	nd_len_reg("fighter", sizeof(fighter_t));
	nd_len_reg("fighter_skel", sizeof(fighter_skel_t));
	// FIXME. If we chane the order of the following two lines, we get horrible bugs
	fighter_hd = nd_open("fighter", "u", "fighter", 0);
	fighter_skel_hd = nd_open("fighter_skel", "u", "fighter_skel", 0);

	nd_register("fight", do_fight, 0);
	nd_get(HD_RWTS, &wt_dodge, "dodge");
	nd_get(HD_RWTS, &wt_hit, "hit");
}

void mod_install(void) {
	act_fight = action_register("fight", "⚔️");
	nd_put(HD_WTS, NULL,  "dodge");
	nd_put(HD_WTS, NULL,  "hit");
	mod_open();
}
