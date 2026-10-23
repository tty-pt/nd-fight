#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ttypt/xy-mod.h>
#include <bud/bud.h>
#include <bud/bud_app.h>
#include <nd/ui-slot-types.h>
#include <nd/bcp-types.h>
#include <nd/fight-ui.h>

static bud_node *
fight_render_slot(int slot, const void *state)
{
	if (slot == ND_UI_SLOT_HUD_OVERLAYS) {
		return nd_fight_render_target_hud(state);
	}
	return NULL;
}

ND_UI_GLUE(fight, .render_slot = fight_render_slot);
