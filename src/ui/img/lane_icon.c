/*
 * lane_icon.c
 *
 * See lane_icon.h for the "why" (protocol's per-lane combinatorics are too
 * large to bake as static bitmaps; runtime rendering follows
 * roundabout-icon.c's precedent).
 *
 * ---- Geometry model (deliberate approximation, not a pixel-accurate copy
 * of the design study's SVGs) ----
 *
 * Every tile shares one shape recipe: a vertical shaft from the bottom of
 * the tile up to a junction point, then one branch per direction the lane
 * carries (1-3), each fanning out from that junction at a fixed angle
 * bucket and ending in an open two-stroke chevron. Concretely, vs. the
 * design study:
 *
 *   - Single-direction STRAIGHT tiles come out identical to the study
 *     (junction-to-tip branch at angle 0 is visually just a straight
 *     continuation of the shaft).
 *   - Single-direction TURN tiles (only one of LEFT/RIGHT family set) use
 *     the *same* shaft+diagonal-branch shape as a Y-split combo branch,
 *     not the study's distinct sharp right-angle "corner" glyph. This is
 *     the one intentional visual departure from the mockup -- reusing one
 *     drawing primitive for both cases keeps this module a fraction of the
 *     size, and a diagonal turn arrow is itself a completely standard
 *     turn-lane glyph. Revisit if the user wants pixel fidelity later.
 *   - Multi-direction (Y-split / trident) tiles match the study's
 *     technique exactly: shared bright shaft, one branch per direction,
 *     brightness resolved per branch (see the brightness rule below).
 *   - U-turn tiles are their own fixed "hook" shape (lane_draw_uturn()),
 *     matching the study's "Wendespur" glyph. A lane whose primary
 *     direction is a U-turn renders *only* the hook -- combining a U-turn
 *     with other directions on the same lane essentially never happens in
 *     practice (a dedicated U-turn lane is U-turn-only by definition,
 *     exactly the study's own example), so this is not handled.
 *   - Every branch reaches a *fixed* length from the junction regardless
 *     of angle (rather than the study's "fixed vertical rise" rule, which
 *     would make sharp-angle branches disproportionately long and risk
 *     colliding with the neighboring tile at the tighter 5-lane pitch) --
 *     tuned by eye against the pitch table below, not measured off the
 *     SVGs.
 *
 * Angle-off-vertical convention used throughout this file: 0 = straight
 * up, positive = clockwise/right, negative = counter-clockwise/left --
 * plain trig (sin/cos), not LVGL's lv_trigo_*() fixed-point convention
 * (this module only re-renders on data change and is cached afterwards, so
 * there's no hot-path reason to avoid floats here).
 *
 * ---- Brightness rule ----
 *
 * Corrected 2026-09-20 (user feedback, confirmed against PROTOCOL.md):
 * the ACTIVE flag (bit 0 of byte 3) is a per-LANE recommendation from
 * OsmAnd's router, full stop - it says nothing about which of a lane's
 * up to 3 directions is "the" one to take. An earlier version of this
 * file invented a heuristic comparing each direction against the current
 * maneuver code to guess that, which isn't in the protocol at all and
 * broke on KEEP_LEFT/KEEP_RIGHT (see the ui-tooling-eez-studio-migration
 * memory for the full incident - a real KEEP_LEFT instruction rendered a
 * combo lane's non-primary branch bright instead of dim because no lane
 * direction ever literally equals KEEP_LEFT/KEEP_RIGHT).
 *
 * The actual rule, straight from PROTOCOL.md's own wording ("primärer
 * Manöver-Code -- Haupt-Pfeilrichtung dieser Spur", i.e. primary IS the
 * lane's main/emphasized direction, secondary/tertiary are just other
 * directions it also permits): shaft + primary branch share the lane's
 * ACTIVE-flag brightness; secondary/tertiary branches are ALWAYS dim,
 * regardless of ACTIVE or of any current maneuver. No maneuver code is
 * consulted anywhere in this file anymore - lanes render purely from
 * their own NavLane content, independent of navigation state, exactly as
 * the protocol intends.
 *
 * ---- Overflow-collapse rule ----
 *
 * See lane_row_select() below for the implementation; matches the design
 * study's "Überlauf-Regel": every ACTIVE lane guaranteed visible, padded
 * with neighbors up to maxTiles, everything else collapsed into a single
 * "..." tile per side.
 *
 * Created on: 20.09.2026
 */

#include "lane_icon.h"

#include <math.h>
#include <string.h>

/* ---- Tunable geometry constants (unit-tile space, multiplied by the
 * per-tile `scale` from the pitch/scale profile tables below) ---- */
#define LANE_UNIT_SHAFT_BOTTOM   10.0f   /* shaft start, below the junction */
#define LANE_UNIT_JUNCTION       -2.0f   /* fan-out point */
#define LANE_UNIT_BRANCH_LEN      9.0f   /* junction -> branch tip, any angle */
#define LANE_UNIT_CHEVRON_ARM     4.5f   /* chevron stroke length */
#define LANE_CHEVRON_SPREAD_DEG  32.0f   /* chevron half-angle off "straight back" */

#define LANE_STROKE_BRIGHT        3.4f   /* unit-tile stroke width, bright */
#define LANE_STROKE_DIM           2.8f   /* unit-tile stroke width, dim */
#define LANE_DIM_OPA          LV_OPA_40  /* matches the study's stroke-opacity 0.4 */

#define LANE_ELLIPSIS_DOT_R       1.3f   /* unit-tile dot radius */
#define LANE_ELLIPSIS_DOT_PITCH   6.0f   /* unit-tile vertical spacing between dots */

typedef struct {
	float x, y;
} FPt;

/* Point at `r` from (originx,originy), `angleDeg` off vertical (0=up,
 * +=clockwise/right) -- see file header for the convention. */
static FPt lane_rot(float originx, float originy, float angleDeg, float r) {
	float rad = angleDeg * (float) M_PI / 180.0f;
	FPt p = { originx + r * sinf(rad), originy - r * cosf(rad) };
	return p;
}

static void lane_stroke(lv_obj_t * canvas, FPt a, FPt b, lv_coord_t width, lv_opa_t opa) {
	lv_draw_line_dsc_t dsc;
	lv_draw_line_dsc_init(&dsc);
	dsc.color = lv_color_black();
	dsc.width = width;
	dsc.opa = opa;
	dsc.round_start = 1;
	dsc.round_end = 1;

	lv_point_t pts[2] = { { (lv_coord_t) lroundf(a.x), (lv_coord_t) lroundf(a.y) },
			{ (lv_coord_t) lroundf(b.x), (lv_coord_t) lroundf(b.y) } };
	lv_canvas_draw_line(canvas, pts, 2, &dsc);
}

/* Open two-stroke chevron ("V") at `tip`, opening back along
 * `branchAngleDeg` (i.e. pointing in the direction of travel). */
static void lane_draw_chevron(lv_obj_t * canvas, FPt tip, float branchAngleDeg, float armLen, lv_coord_t width,
		lv_opa_t opa) {
	FPt left = lane_rot(tip.x, tip.y, branchAngleDeg + 180.0f - LANE_CHEVRON_SPREAD_DEG, armLen);
	FPt right = lane_rot(tip.x, tip.y, branchAngleDeg + 180.0f + LANE_CHEVRON_SPREAD_DEG, armLen);
	lane_stroke(canvas, left, tip, width, opa);
	lane_stroke(canvas, tip, right, width, opa);
}

static void lane_draw_branch(lv_obj_t * canvas, FPt from, float angleDeg, float len, float scale, lv_coord_t width,
		lv_opa_t opa) {
	FPt tip = lane_rot(from.x, from.y, angleDeg, len);
	lane_stroke(canvas, from, tip, width, opa);
	lane_draw_chevron(canvas, tip, angleDeg, LANE_UNIT_CHEVRON_ARM * scale, width, opa);
}

/* U-turn "hook": shaft up, corner across, corner back down, chevron
 * pointing back down across the loop -- see the file header comment.
 * mirror=+1 loops via the right (UTURN_RIGHT), -1 via the left
 * (UTURN_LEFT). Coordinates are the study's "Wendespur" path, taken as a
 * self-contained glyph centered on (cx,cyCenter) rather than built from
 * the shared shaft/junction/branch primitives above. */
static void lane_draw_uturn(lv_obj_t * canvas, float cx, float cyCenter, float scale, int8_t mirror,
		lv_coord_t width, lv_opa_t opa) {
	FPt p0 = { cx + mirror * 4.0f * scale, cyCenter + 9.0f * scale };
	FPt p1 = { cx + mirror * 4.0f * scale, cyCenter - 6.0f * scale };
	FPt p2 = { cx - mirror * 4.0f * scale, cyCenter - 6.0f * scale };
	FPt p3 = { cx - mirror * 4.0f * scale, cyCenter + 6.0f * scale };
	lane_stroke(canvas, p0, p1, width, opa);
	lane_stroke(canvas, p1, p2, width, opa);
	lane_stroke(canvas, p2, p3, width, opa);

	FPt chevA = { cx - mirror * 8.0f * scale, cyCenter + 1.0f * scale };
	FPt chevB = { cx, cyCenter + 1.0f * scale };
	lane_stroke(canvas, chevA, p3, width, opa);
	lane_stroke(canvas, p3, chevB, width, opa);
}

static void lane_draw_ellipsis(lv_obj_t * canvas, float cx, float cyCenter, float scale) {
	lv_draw_rect_dsc_t dsc;
	lv_draw_rect_dsc_init(&dsc);
	dsc.bg_color = lv_color_black();
	dsc.bg_opa = LV_OPA_COVER;
	dsc.border_width = 0;
	dsc.radius = LV_RADIUS_CIRCLE;

	lv_coord_t r = (lv_coord_t) LV_MAX(1, lroundf(LANE_ELLIPSIS_DOT_R * scale));
	for (int8_t i = -1; i <= 1; i++) {
		float dy = i * LANE_ELLIPSIS_DOT_PITCH * scale;
		lv_coord_t x1 = (lv_coord_t) lroundf(cx - r);
		lv_coord_t y1 = (lv_coord_t) lroundf(cyCenter + dy - r);
		lv_canvas_draw_rect(canvas, x1, y1, r * 2, r * 2, &dsc);
	}
}

/* Angle-off-vertical bucket for one maneuver code's arrow branch. U-turns
 * are handled separately (lane_draw_uturn) and never reach here from
 * lane_draw_tile(). */
static float lane_branch_angle_deg(uint8_t maneuver) {
	switch (maneuver) {
	case NAV_MANEUVER_TURN_SHARP_LEFT:
		return -75.0f;
	case NAV_MANEUVER_TURN_LEFT:
		return -50.0f;
	case NAV_MANEUVER_TURN_SLIGHT_LEFT:
	case NAV_MANEUVER_KEEP_LEFT:
		return -25.0f;
	case NAV_MANEUVER_TURN_SHARP_RIGHT:
		return 75.0f;
	case NAV_MANEUVER_TURN_RIGHT:
		return 50.0f;
	case NAV_MANEUVER_TURN_SLIGHT_RIGHT:
	case NAV_MANEUVER_KEEP_RIGHT:
		return 25.0f;
	default:
		return 0.0f; /* STRAIGHT, DEPART, ARRIVE, ROUNDABOUT, UNKNOWN */
	}
}

static lv_coord_t lane_stroke_width(float scale, bool bright) {
	float w = (bright ? LANE_STROKE_BRIGHT : LANE_STROKE_DIM) * scale;
	if (w < 1.5f) w = 1.5f;
	return (lv_coord_t) lroundf(w);
}

static lv_opa_t lane_opa(bool bright) {
	return bright ? LV_OPA_COVER : LANE_DIM_OPA;
}

/* Draws one lane's full tile (shaft + 1-3 branches, or a U-turn hook) at
 * horizontal center `cx`. Brightness: ACTIVE flag drives the shaft and
 * the primary branch; secondary/tertiary are always dim - see the
 * "Brightness rule" section in this file's header comment. */
static void lane_draw_tile(lv_obj_t * canvas, float cx, float cyCenter, float scale, const NavLane * lane) {
	bool laneBright = (lane->flags & NAV_LANE_FLAG_ACTIVE) != 0;

	if (lane->primary == NAV_MANEUVER_UTURN_LEFT || lane->primary == NAV_MANEUVER_UTURN_RIGHT) {
		int8_t mirror = (lane->primary == NAV_MANEUVER_UTURN_RIGHT) ? 1 : -1;
		lane_draw_uturn(canvas, cx, cyCenter, scale, mirror, lane_stroke_width(scale, laneBright),
				lane_opa(laneBright));
		return;
	}

	float shaftBottomY = cyCenter + LANE_UNIT_SHAFT_BOTTOM * scale;
	float junctionY = cyCenter + LANE_UNIT_JUNCTION * scale;
	FPt junction = { cx, junctionY };

	lane_stroke(canvas, (FPt ) { cx, shaftBottomY }, junction, lane_stroke_width(scale, laneBright),
			lane_opa(laneBright));

	uint8_t dirs[3] = { lane->primary, lane->secondary, lane->tertiary };
	for (uint8_t i = 0; i < 3; i++) {
		if (dirs[i] == NAV_MANEUVER_NONE) continue;
		bool bright = (i == 0) && laneBright; /* primary only, and only if this lane is ACTIVE */
		lane_draw_branch(canvas, junction, lane_branch_angle_deg(dirs[i]), LANE_UNIT_BRANCH_LEN * scale, scale,
				lane_stroke_width(scale, bright), lane_opa(bright));
	}
}

/* ---- Overflow-collapse tile selection ---- */

typedef struct {
	bool isEllipsis;
	uint8_t laneIdx; /* valid only if !isEllipsis */
} LaneSlot;

/* Picks which lanes (and, if laneCount > maxTiles, which ellipsis markers)
 * to show, writing up to maxTiles entries into `out` and returning how
 * many were written. See the file header "Overflow-collapse rule". */
static uint8_t lane_row_select(const NavLane * lanes, uint8_t laneCount, uint8_t maxTiles, LaneSlot * out) {
	if (laneCount <= maxTiles) {
		for (uint8_t i = 0; i < laneCount; i++) {
			out[i].isEllipsis = false;
			out[i].laneIdx = i;
		}
		return laneCount;
	}

	int16_t lo = -1, hi = -1;
	for (uint8_t i = 0; i < laneCount; i++) {
		if (lanes[i].flags & NAV_LANE_FLAG_ACTIVE) {
			if (lo < 0) lo = i;
			hi = i;
		}
	}
	if (lo < 0) {
		/* No lane flagged ACTIVE at all (protocol allows it) - just center
		 * a maxTiles-wide window instead of guessing which lanes matter. */
		lo = (laneCount - maxTiles) / 2;
		hi = lo + maxTiles - 1;
	}

	bool leftOverflow = lo > 0;
	bool rightOverflow = hi < (int16_t) laneCount - 1;
	int16_t budget = (int16_t) maxTiles - (leftOverflow ? 1 : 0) - (rightOverflow ? 1 : 0);

	if ((hi - lo + 1) > budget) {
		/* Extreme edge case: even the active-lane span alone doesn't fit
		 * the budget. Thin it symmetrically from both ends rather than
		 * picking a side - real-world roads don't produce this shape
		 * (would need >maxTiles non-contiguous active lanes). */
		int16_t excess = (hi - lo + 1) - LV_MAX(budget, 0);
		int16_t trimEach = excess / 2;
		lo += trimEach;
		hi -= (excess - trimEach);
		if (lo > hi) hi = lo;
	} else {
		int16_t remaining = budget - (hi - lo + 1);
		bool growLeft = true;
		while (remaining > 0 && (lo > 0 || hi < (int16_t) laneCount - 1)) {
			if (growLeft && lo > 0) {
				lo--;
				remaining--;
			} else if (!growLeft && hi < (int16_t) laneCount - 1) {
				hi++;
				remaining--;
			}
			growLeft = !growLeft;
		}
	}
	leftOverflow = lo > 0;
	rightOverflow = hi < (int16_t) laneCount - 1;

	uint8_t n = 0;
	if (leftOverflow && n < maxTiles) {
		out[n].isEllipsis = true;
		n++;
	}
	for (int16_t i = lo; i <= hi && n < maxTiles; i++, n++) {
		out[n].isEllipsis = false;
		out[n].laneIdx = (uint8_t) i;
	}
	if (rightOverflow && n < maxTiles) {
		out[n].isEllipsis = true;
		n++;
	}
	return n;
}

/* ---- Pitch/scale profiles ----
 *
 * Nav strip (RimRidgeNav, maxTiles==5): the design study's original
 * "Lane-Widget - Extremfälle" numbers don't apply to this project's real,
 * as-built layout (see patch_rimridgenav_lane_row.py's module comment) -
 * the actual box went through two rounds: first a genuinely-free-but-tight
 * 24px-tall gap, then the user rearranged rrnav_ic_turn/rrnav_dist_val/
 * rrnav_street/rrnav_ic_gradient in EEZ Studio to grow rrnav_lane_row to
 * 50px tall (2026-09-20, same height as rr_lane_row on the Main screen) -
 * width stayed a generous 280px. Scale therefore just matches Main's own
 * 0.9 (same vertical budget, see MAIN_PROFILE's comment below for the
 * fit math - it applies identically here), pitch is simply spread wider
 * since 280px has plenty of room to spare even at 5 tiles.
 *
 * Main chip (RimRidge, maxTiles==3): the study's original, already-
 * approved compact 3-tile layout - fixed pitch/scale regardless of how
 * many of the 3 slots are actually filled this frame (unlike the Nav
 * strip, this one never adapts).
 */
typedef struct {
	lv_coord_t pitch;
	float scale;
} LaneRowProfile;

static const LaneRowProfile NAV_PROFILE_BY_COUNT[] = {
/* 1 */{ 0,  0.9f },
/* 2 */{ 70, 0.9f },
/* 3 */{ 55, 0.9f },
/* 4 */{ 46, 0.9f },
/* 5 */{ 40, 0.9f }, };
// Scale bumped 2026-09-20 (0.75->0.9) once rr_lane_row grew from 80x40 to
// 80x50 - the user's own call ("Icons könnten größer sein ... 50 hoch,
// aber nur so breit wie nötig"): a taller box has vertical room for
// bigger tiles without needing to be square. Pitch tightened right back
// down afterwards (30->24, a second round of user feedback: "Könnte noch
// kompakter sein (kleiner in X-Richtung)") - moderate-angle branches
// (this profile's realistic case; TURN_LEFT/RIGHT's ~50 degree bucket)
// reach ~8.5px each side at scale 0.9, so pitch 24 still keeps them a few
// px apart. Only a rare double-SHARP_LEFT/RIGHT-adjacent combination
// (~11.7px reach each side) would touch at this pitch - an accepted
// tradeoff for "more compact", consistent with this whole icon set's
// "good enough for a glance while riding" tolerance (see roundabout-
// icon.c's own precedent).
static const LaneRowProfile MAIN_PROFILE = { 24, 0.9f };

static LaneRowProfile lane_row_profile(uint8_t maxTiles, uint8_t shownCount) {
	if (maxTiles <= 3) return MAIN_PROFILE;
	uint8_t idx = (shownCount == 0) ? 0 : (uint8_t) LV_MIN(shownCount, 5) - 1;
	return NAV_PROFILE_BY_COUNT[idx];
}

/* ---- Canvas cache (two slots: one per fixed (width,height) call site --
 * RimRidge's chip and RimRidgeNav's strip. A third distinct size still
 * works, just evicts and re-renders more often - see roundabout-icon.c's
 * identical tradeoff.) ---- */
#define LANE_ROW_CACHE_SLOTS 2

typedef struct {
	lv_obj_t * canvas;
	void * buf;
	lv_coord_t width, height;
	NavLane lastLanes[NAV_LANES_MAX];
	uint8_t lastLaneCount;
	uint8_t lastMaxTiles;
	bool everRendered;
} LaneRowSlot;

static LaneRowSlot s_slots[LANE_ROW_CACHE_SLOTS];
static uint8_t s_nextEvict = 0;

static LaneRowSlot * lane_row_find_slot(lv_coord_t width, lv_coord_t height) {
	for (uint8_t i = 0; i < LANE_ROW_CACHE_SLOTS; i++) {
		if (s_slots[i].canvas != NULL && s_slots[i].width == width && s_slots[i].height == height) {
			return &s_slots[i];
		}
	}
	return NULL;
}

static void lane_row_free_slot(LaneRowSlot * slot) {
	if (slot->canvas != NULL) {
		lv_obj_del(slot->canvas);
		slot->canvas = NULL;
	}
	if (slot->buf != NULL) {
		lv_mem_free(slot->buf);
		slot->buf = NULL;
	}
	slot->width = slot->height = 0;
	slot->everRendered = false;
}

static LaneRowSlot * lane_row_alloc_slot(lv_coord_t width, lv_coord_t height) {
	LaneRowSlot * slot = NULL;
	for (uint8_t i = 0; i < LANE_ROW_CACHE_SLOTS; i++) {
		if (s_slots[i].canvas == NULL) {
			slot = &s_slots[i];
			break;
		}
	}
	if (slot == NULL) {
		slot = &s_slots[s_nextEvict];
		s_nextEvict = (uint8_t) ((s_nextEvict + 1) % LANE_ROW_CACHE_SLOTS);
		lane_row_free_slot(slot);
	}

	uint32_t bufSize = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(width, height);
	void * buf = lv_mem_alloc(bufSize);
	if (buf == NULL) return NULL;

	/* Same "hidden canvas parented to the system layer" trick as
	 * roundabout-icon.c - it's an off-screen pixel buffer, never itself a
	 * visible widget, see lane_icon.h. */
	lv_obj_t * canvas = lv_canvas_create(lv_layer_sys());
	if (canvas == NULL) {
		lv_mem_free(buf);
		return NULL;
	}
	lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
	lv_canvas_set_buffer(canvas, buf, width, height, LV_IMG_CF_TRUE_COLOR_ALPHA);

	slot->canvas = canvas;
	slot->buf = buf;
	slot->width = width;
	slot->height = height;
	slot->everRendered = false;
	return slot;
}

static bool lane_row_content_changed(const LaneRowSlot * slot, const NavLane * lanes, uint8_t laneCount,
		uint8_t maxTiles) {
	if (!slot->everRendered) return true;
	if (slot->lastLaneCount != laneCount) return true;
	if (slot->lastMaxTiles != maxTiles) return true;
	return memcmp(slot->lastLanes, lanes, sizeof(NavLane) * laneCount) != 0;
}

static void lane_row_render(lv_obj_t * canvas, lv_coord_t width, lv_coord_t height, const NavLane * lanes,
		uint8_t laneCount, uint8_t maxTiles) {
	lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

	LaneSlot slots[LANE_ROW_MAX_TILES];
	uint8_t shown = lane_row_select(lanes, laneCount, LV_MIN(maxTiles, LANE_ROW_MAX_TILES), slots);
	if (shown == 0) return;

	LaneRowProfile profile = lane_row_profile(maxTiles, shown);
	float cyCenter = height / 2.0f;
	float rowCenterX = width / 2.0f;
	float startOffset = -((float) (shown - 1)) * profile.pitch / 2.0f;

	for (uint8_t i = 0; i < shown; i++) {
		float cx = rowCenterX + startOffset + i * profile.pitch;
		if (slots[i].isEllipsis) {
			lane_draw_ellipsis(canvas, cx, cyCenter, profile.scale);
		} else {
			lane_draw_tile(canvas, cx, cyCenter, profile.scale, &lanes[slots[i].laneIdx]);
		}
	}
}

const lv_img_dsc_t * lane_row_get(const NavLane * lanes, uint8_t laneCount, uint8_t maxTiles, lv_coord_t width,
		lv_coord_t height) {
	if (laneCount == 0 || width <= 0 || height <= 0) return NULL;
	if (laneCount > NAV_LANES_MAX) laneCount = NAV_LANES_MAX;

	LaneRowSlot * slot = lane_row_find_slot(width, height);
	if (slot == NULL) {
		slot = lane_row_alloc_slot(width, height);
		if (slot == NULL) return NULL;
	}

	if (lane_row_content_changed(slot, lanes, laneCount, maxTiles)) {
		lane_row_render(slot->canvas, width, height, lanes, laneCount, maxTiles);
		memcpy(slot->lastLanes, lanes, sizeof(NavLane) * laneCount);
		slot->lastLaneCount = laneCount;
		slot->lastMaxTiles = maxTiles;
		slot->everRendered = true;
	}

	return lv_canvas_get_img(slot->canvas);
}

void lane_icon_deinit(void) {
	for (uint8_t i = 0; i < LANE_ROW_CACHE_SLOTS; i++) {
		lane_row_free_slot(&s_slots[i]);
	}
}
