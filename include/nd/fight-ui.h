/*
 * fight-ui.h — Combat HUD overlay UI for nd-fight.
 * Renders a floating target card with vitality bar and combat status when in combat.
 */

#ifndef ND_FIGHT_UI_H
#define ND_FIGHT_UI_H

#include <stdio.h>
#include <bud/bud.h>
#include <bud/bud_app.h>
#include <nd/ui-slot-types.h>
#include <nd/bcp-types.h>
#include <nd/fight-types.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline bud_node *
nd_fight_render_target_hud(const void *state)
{
	const nd_client_state_t *st = (const nd_client_state_t *)state;
	int active = st && st->target_ref != 0;

	/* Fixed shape: the card is always present and only toggles `hidden`, so
	 * the bud patch stream (which cannot append to existing elements) can
	 * update it in place when a target appears or vanishes. */
	const char *target_name = "Target";
	if (active)
		for (size_t i = 0; i < st->entity_count; i++)
			if (st->entities[i].ref == st->target_ref) {
				target_name = st->entities[i].name;
				break;
			}

	/* Vitality comes from the generic target gauge (val,max); the width is
	 * computed here, never hard-coded. */
	const nd_bar_state_t *tg = active ? nd_client_gauge_get(st, ND_CLIENT_TARGET_GAUGE) : NULL;
	unsigned tval = tg ? tg->val : 0, tmax = tg && tg->max ? tg->max : 1;
	unsigned pct = tval >= tmax ? 100 : (tval * 100) / tmax;
	char w_buf[32], hp_buf[32], pct_buf[16], target_str[16] = "", ref_buf[32] = " ";
	snprintf(w_buf, sizeof(w_buf), "width: %u%%;", active ? pct : 0);
	snprintf(hp_buf, sizeof(hp_buf), "%u/%u", tval, tg ? tg->max : 0);
	snprintf(pct_buf, sizeof(pct_buf), "%u", active ? pct : 0);
	if (active) {
		snprintf(target_str, sizeof(target_str), "%u", st->target_ref);
		snprintf(ref_buf, sizeof(ref_buf), "Target #%u", st->target_ref);
	}

	bud_node *card = bud_element("div");
	bud_set_attr(card, "id", "combat-target-hud");
	bud_set_attr(card, "data-billboard-target", target_str);
	bud_set_attr(card, "data-target-pct", pct_buf);
	bud_set_attr(card, "class", active
		? "absolute top-4 left-1/2 -translate-x-1/2 z-30 flex items-center gap-3 "
		  "bg-neutral-900/90 border border-red-700/60 shadow-lg rounded-lg px-4 py-2 backdrop-blur-sm pointer-events-none select-none"
		: "hidden");

	bud_node *badge = bud_element("div");
	bud_set_attr(badge, "class", "text-red-500 font-bold text-sm tracking-wider flex items-center gap-1.5");
	bud_append(badge, bud_text(active ? target_name : " "));

	bud_node *info = bud_element("div");
	bud_set_attr(info, "class", "flex flex-col gap-1 w-32");

	bud_node *ref_tag = bud_element("span");
	bud_set_attr(ref_tag, "class", "text-[10px] text-neutral-400 font-mono");
	bud_append(ref_tag, bud_text(ref_buf));

	bud_node *bar_bg = bud_element("div");
	bud_set_attr(bar_bg, "class", "w-full bg-neutral-800 rounded h-1.5 overflow-hidden");
	bud_set_attr(bar_bg, "title", hp_buf);
	bud_node *bar = bud_element("div");
	bud_set_attr(bar, "class", "bg-red-600 h-full rounded transition-all duration-300");
	bud_set_attr(bar, "style", w_buf);
	bud_append(bar_bg, bar);

	bud_append(info, ref_tag);
	bud_append(info, bar_bg);

	bud_append(card, badge);
	bud_append(card, info);

	return card;
}

#ifdef __cplusplus
}
#endif

#endif /* !ND_FIGHT_UI_H */
