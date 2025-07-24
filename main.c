#include "./include/uapi/fight.h"

#include <nd/nd.h>
#include <nd/mortal.h>
#include <nd/attr.h>

#include <stdlib.h>
#include <stdio.h>

unsigned fighter_hd, fighter_skel_hd, bcp_stats, act_fight;

/* API */

SIC_DEF(short, fight_damage, unsigned, dmg_type, short, dmg, short, def, unsigned, def_type);
SIC_DEF(int, fighter_attack, unsigned, player_ref, sic_str_t, ss, hit_t, hit);

/* SIC */

SIC_DEF(int, on_will_attack, unsigned, ent_ref)
SIC_DEF(int, on_attack, unsigned, ent_ref, hit_t, hit)
SIC_DEF(int, on_hit, unsigned, ent_ref, hit_t, hit)
SIC_DEF(int, on_did_attack, unsigned, player_ref, hit_t, hit)
SIC_DEF(int, on_dodge_attempt, unsigned, player_ref, hit_t, hit)
SIC_DEF(int, on_dodge, unsigned, player_ref, hit_t, hit)

int on_before_leave(unsigned ref) {
	fighter_t fighter;

	nd_get(fighter_hd, &fighter, &ref);

	if (fighter.target == NOTHING)
		return 0;

	OBJ obj;

	nd_get(HD_OBJ, &obj, &ref);
	nd_writef(ref, "%s stops fighting.\n", obj.name);
	fighter.target = NOTHING;
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
	call_attr_award(player_ref, 2 * (cxp / 1000));
	while (cxp >= 1000) {
		nd_writef(player_ref, "You level up!\n");
		cxp -= 1000;
		fighter->lvl += 1;
	}
	fighter->cxp = cxp;
}

static inline unsigned char
d20(void)
{
	return (random() % 20) + 1;
}

// returns 1 if target dodges
static inline int
dodge_get(unsigned ref, fighter_t *fighter)
{
	return d20() < call_effect(ref, AF_DODGE);
}

int
dodge(unsigned ref, hit_t hit)
{
	fighter_t fighter;
	char *wts[BUFSIZ];

	int stuck = call_on_dodge_attempt(ref, hit);
	nd_get(fighter_hd, &fighter, &ref);
	nd_get(HD_WTS, wts, &fighter.wtst);

	if (stuck || call_effect(ref, AF_MOV) || !dodge_get(ref, &fighter)) {
		call_on_hit(ref, hit);
		return 1;
	}

	call_on_dodge(ref, hit);
	notify_wts_to(fighter.target, ref, "dodge", "dodges", "'s %s", wts);
	return 0;
}

static inline void
notify_attack(unsigned player_ref, unsigned target_ref, sic_str_t ss, hit_t hit)
{
	char buf[BUFSIZ];
	char wts_buf[BUFSIZ], *wts = wts_buf;
	unsigned i = 0;

	if (ss.str[0])
		wts = ss.str;
	else
		nd_get(HD_WTS, wts_buf, &hit.wtst);

	if (hit.ndmg || hit.cdmg) {
		buf[i++] = ' ';
		buf[i++] = '(';

		if (hit.ndmg)
			i += snprintf(&buf[i], sizeof(buf) - i, "%d%s", hit.ndmg, hit.cdmg ? ", " : "");

		if (hit.cdmg)
			i += snprintf(&buf[i], sizeof(buf) - i, "%s%d%s", ansi_fg[hit.color], hit.cdmg, ANSI_RESET);

		buf[i++] = ')';
	}
	buf[i] = '\0';

	notify_wts_to(player_ref, target_ref, wts, wts_plural(wts), "%s", buf);
}

static inline short
randd_dmg(short dmg)
{
	register unsigned short xx = 1 + (random() & 7);
	return xx = dmg + ((dmg * xx * xx * xx) >> 9);
}

short
fight_damage(unsigned dmg_type, short dmg,
	short def, unsigned def_type)
{
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

int fighter_attack(unsigned ref, sic_str_t ss, hit_t hit) {
	fighter_t fighter;

	call_on_attack(ref, hit);

	nd_get(fighter_hd, &fighter, &ref);
	if (fighter.target == NOTHING)
		return 0;

	if (dodge(ref, hit))
		return 0;

	nd_get(fighter_hd, &fighter, &ref);
	notify_attack(ref, fighter.target, ss, hit);
	call_mortal_damage(ref, fighter.target, hit.ndmg + hit.cdmg);
	call_on_did_attack(ref, hit);
	return hit.ndmg + hit.cdmg;
}

int
on_mortal_update(unsigned ref, double dt __attribute__((unused)))
{
	fighter_t fighter, target;
	unsigned target_ref;

	nd_get(fighter_hd, &fighter, &ref);
	if (fighter.target == NOTHING)
		return 0;

	nd_get(fighter_hd, &target, &fighter.target);

	if (target.target == NOTHING) {
		target.target = ref;
		nd_put(fighter_hd, &fighter.target, &target);
	}

	fighter.flags |= FF_ATTACK;
	nd_put(fighter_hd, &ref, &fighter);

	fighter.attack.ndmg = -fight_damage(ELM_PHYSICAL,
			call_effect(ref, AF_DMG),
			call_effect(fighter.target, AF_DEF)
			+ call_effect(fighter.target, AF_MDEF),
			dt);
	fighter.attack.wtst = fighter.wtso;
	nd_put(fighter_hd, &ref, &fighter);

	call_on_will_attack(ref);

	nd_get(fighter_hd, &fighter, &ref);
	sic_str_t ss = { .str = "" };
	if (fighter.flags & FF_ATTACK)
		fighter_attack(ref, ss, fighter.attack);

	return 0;
}

int on_move(unsigned player_ref, int cant_move) {
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
	nd_get(HD_OBJ, &player, &player_ref);
	unsigned target_ref = strcmp(argv[1], "me")
		? ematch_near(player_ref, argv[1])
		: player_ref;

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
	nd_writef(player_ref, "Fighter:\tlock %u\n", fighter.klock);
	return 0;
}

int on_murder(unsigned ref, unsigned victim_ref) {
	fighter_t fighter, victim;

	nd_get(fighter_hd, &fighter, &ref);
	nd_get(fighter_hd, &victim, &victim_ref);

	if (victim.target && (victim.flags & FF_AGGRO)) {
		fighter_t tartar;
		nd_get(fighter_hd, &tartar, &victim.target);
		tartar.klock --;
		nd_put(fighter_hd, &victim.target, &tartar);
	}

	if (ref != NOTHING) {
		fighter_award(ref, &fighter, &victim);
		fighter.target = NOTHING;
	}

	nd_put(fighter_hd, &ref, &fighter);
	return 0;
}

static inline void
stats_init(unsigned ref, fighter_t *fighter, fighter_skel_t *skel)
{
	unsigned char stat = skel->stat;
	int lvl = skel->lvl, spend, i, sp,
	    v = skel->lvl_v ? skel->lvl_v : 0xf;

	lvl += random() & v;

	if (!stat)
		stat = 0x1f;

	spend = 1 + lvl;
	char attr_c[] = "acdiwh";
	for (char *c = attr_c; *c; c++)
		if (stat & (1<<i)) {
			sp = random() % spend;
			call_train(ref, *c, sp);
		}

	fighter->lvl = lvl;
}

unsigned hp_max(unsigned ref) {
	unsigned last;
	fighter_t fighter;

	sic_last(&last);
	nd_get(fighter_hd, &fighter, &ref);
	return last + 5 * fighter.lvl;
}

int on_birth(unsigned ref, uint64_t v) {
	OBJ obj;
	fighter_t fighter;
	fighter_skel_t skel;

	memset(&fighter, 0, sizeof(fighter));

	nd_get(HD_OBJ, &obj, &ref);
	nd_get(fighter_skel_hd, &skel, &obj.skid);
	fighter.wtso = skel.wt;
	fighter.target = NOTHING;

	if (!(obj.flags & OF_PLAYER))
		stats_init(ref, &fighter, &skel);

	nd_put(fighter_hd, &ref, &fighter);
	return 0;
}

struct icon on_icon(struct icon i, unsigned ref, unsigned type) {
	if (type != TYPE_ENTITY)
		return i;

	i.actions |= act_fight;
	return i;
}

void mod_open() {
	SIC_AREG(fight_damage);
	SIC_AREG(fighter_attack);

	SIC_AREG(on_attack);
	SIC_AREG(on_hit);
	SIC_AREG(on_did_attack);
	SIC_AREG(on_dodge_attempt);
	SIC_AREG(on_dodge);

	nd_len_reg("fighter", sizeof(fighter_t));
	nd_len_reg("fighter_skel", sizeof(fighter_skel_t));
	// FIXME. If we chane the order of the following two lines, we get horrible bugs
	fighter_hd = nd_open("fighter", "u", "fighter", 0);
	fighter_skel_hd = nd_open("fighter_skel", "u", "fighter_skel", 0);

	act_fight = action_register("fight", "⚔️");

	nd_register("fight", do_fight, 0);
}

void mod_install() {
	mod_open();
}
