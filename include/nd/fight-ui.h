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
	if (!st || st->target_ref == 0) {
		bud_node *empty = bud_element("div");
		bud_set_attr(empty, "id", "combat-target-hud");
		bud_set_attr(empty, "class", "hidden");
		return empty;
	}

	const char *target_name = "Target";
	for (size_t i = 0; i < st->entity_count; i++) {
		if (st->entities[i].ref == st->target_ref) {
			target_name = st->entities[i].name;
			break;
		}
	}

	bud_node *card = bud_element("div");
	bud_set_attr(card, "id", "combat-target-hud");
	bud_set_attr(card, "class",
		"absolute top-4 left-1/2 -translate-x-1/2 z-30 flex items-center gap-3 "
		"bg-neutral-900/90 border border-red-700/60 shadow-lg rounded-lg px-4 py-2 backdrop-blur-sm pointer-events-none select-none");

	bud_node *badge = bud_element("div");
	bud_set_attr(badge, "class", "text-red-500 font-bold text-sm tracking-wider flex items-center gap-1.5");
	bud_append(badge, bud_text("⚔"));
	bud_append(badge, bud_text(target_name));

	bud_node *info = bud_element("div");
	bud_set_attr(info, "class", "flex flex-col gap-1 w-32");

	bud_node *ref_tag = bud_element("span");
	bud_set_attr(ref_tag, "class", "text-[10px] text-neutral-400 font-mono");
	char ref_buf[32];
	snprintf(ref_buf, sizeof(ref_buf), "Target #%u", st->target_ref);
	bud_append(ref_tag, bud_text(ref_buf));

	bud_node *bar_bg = bud_element("div");
	bud_set_attr(bar_bg, "class", "w-full bg-neutral-800 rounded h-1.5 overflow-hidden");
	bud_node *bar = bud_element("div");
	bud_set_attr(bar, "class", "bg-red-600 h-full rounded transition-all duration-300 w-full");
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
