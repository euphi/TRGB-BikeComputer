/*
 * roundabout-icon.c
 *
 * See roundabout-icon.h for the "why" (protocol only gives an exit number,
 * no total-exit-count; flash is tight; runtime rendering avoids baking a
 * new per-exit-count image set).
 *
 * Icon anatomy, all proportional to the requested `size` so the same code
 * serves both the 64px small-panel icon and the 192px main-panel icon:
 *
 *   - a ring (the roundabout itself)
 *   - a short, plain, fixed stub at the bottom (the entry -- always the
 *     same, regardless of exit number: you always enter "from the bottom")
 *   - a bold stub with an arrowhead, rotated to the exit angle (the exit
 *     actually taken)
 *   - the exit number itself, as a digit in the middle of the ring, so the
 *     icon stays unambiguous even for exit counts high enough that the
 *     rotated-arrow angle alone would be hard to distinguish at a glance
 *
 * Angle convention:
 *
 *   Screen-space angles here follow LVGL's lv_trigo_sin()/lv_trigo_cos()
 *   convention: 0 deg = to the right (3 o'clock, +x), and increasing angle
 *   rotates clockwise on screen (because +y is down). We fix the entry at
 *   90 deg (straight down, 6 o'clock) and then, per exit index, rotate by
 *   a further fixed ROUNDABOUT_ICON_ANGLE_STEP_DEG (45 deg -- 8 positions
 *   around the ring, which comfortably covers real-world roundabout exit
 *   counts).
 *
 *   The protocol gives us no bearing/heading data to compute a
 *   geometrically correct exit angle (unlike e.g. OsmAnd, which has the
 *   full route geometry and computes a real turn angle -- see
 *   TurnType.getTurnAngle() upstream). Lacking that, a fixed angular step
 *   is the standard fallback and is exactly what this does.
 *
 *   Direction of travel around the roundabout is assumed
 *   counter-clockwise, matching right-hand-traffic countries (mainland
 *   Europe, this project's market) -- decreasing angle from the entry.
 *   Set ROUNDABOUT_ICON_COUNTERCLOCKWISE to 0 below to flip it for a
 *   left-hand-traffic build. With this convention:
 *     exit 1 -> slight right, exit 2 -> right, exit 4 -> straight ahead,
 *     exit 6 -> left, exit 8 -> back to the entry angle (near U-turn).
 *
 * Color format:
 *
 *   The canvas uses LV_IMG_CF_TRUE_COLOR_ALPHA (not the
 *   LV_IMG_CF_INDEXED_1BIT of the static Nav64ImgTable/NavImgTable icons)
 *   so that lv_obj_set_style_img_recolor() -- used elsewhere in
 *   ui_NaviCustFunc.c to tint nav icons by distance-to-maneuver -- keeps
 *   working the same way: it recolors the ink pixels and leaves the rest
 *   transparent, instead of tinting a solid opaque square. This costs a
 *   few KB to a bit over 100KB of RAM per cached size (3 bytes/px at
 *   LV_COLOR_DEPTH=16), which is cheap here: LV_MEM_CUSTOM routes
 *   lv_mem_alloc() through ps_malloc() (see include/lv_conf.h), i.e. onto
 *   PSRAM, not the tight flash/internal-SRAM budget this project is
 *   actually short on.
 *
 * Created on: 14.09.2026
 */

#include "roundabout-icon.h"

#include <stdio.h>

/* Fixed angular step per exit index and the entry's fixed screen angle.
 * See the file header comment above for the full rationale. */
#define ROUNDABOUT_ICON_ANGLE_STEP_DEG   45
#define ROUNDABOUT_ICON_ENTRY_ANGLE_DEG  90
#define ROUNDABOUT_ICON_COUNTERCLOCKWISE 1

/* Small bounded cache of rendered canvases, keyed by pixel size. In
 * practice only two sizes are ever requested (64 for the small nav panel,
 * 192 for the large one -- see Nav64ImgTable/NavImgTable in
 * src/ui/img/ui_img_nav_64_allimages.c / ui_img_nav_allimages.c), so two
 * slots cover the real usage; a third distinct size just costs an extra
 * re-render via round-robin eviction instead of a crash or a leak. */
#define ROUNDABOUT_ICON_CACHE_SLOTS 2

typedef struct {
    lv_obj_t * canvas;     /* hidden lv_canvas, owns the lv_img_dsc_t */
    void * buf;             /* pixel buffer we allocated for the canvas */
    lv_coord_t size;        /* 0 = unused slot */
    uint8_t exit_number;    /* last rendered exit number, 0 = "never drawn" */
} roundabout_icon_slot_t;

static roundabout_icon_slot_t s_slots[ROUNDABOUT_ICON_CACHE_SLOTS];
static uint8_t s_next_evict = 0;

static void roundabout_icon_point(lv_point_t * p, lv_coord_t cx, lv_coord_t cy, lv_coord_t r, int32_t angle_deg)
{
    int32_t a = ((angle_deg % 360) + 360) % 360;
    int32_t dx = ((int32_t)r * lv_trigo_cos((int16_t)a)) >> LV_TRIGO_SHIFT;
    int32_t dy = ((int32_t)r * lv_trigo_sin((int16_t)a)) >> LV_TRIGO_SHIFT;
    p->x = (lv_coord_t)(cx + dx);
    p->y = (lv_coord_t)(cy + dy);
}

static const lv_font_t * roundabout_icon_pick_font(lv_coord_t size)
{
#if LV_FONT_MONTSERRAT_36
    if(size >= 140) return &lv_font_montserrat_36;
#endif
#if LV_FONT_MONTSERRAT_24
    if(size >= 90) return &lv_font_montserrat_24;
#endif
#if LV_FONT_MONTSERRAT_18
    if(size >= 60) return &lv_font_montserrat_18;
#endif
    return LV_FONT_DEFAULT;
}

static void roundabout_icon_render(lv_obj_t * canvas, lv_coord_t size, uint8_t exit_number)
{
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    const lv_coord_t cx = size / 2;
    const lv_coord_t cy = size / 2;
    const lv_color_t ink = lv_color_black();

    /* All proportions below keep the farthest drawn point (exit arrowhead
     * tip) within ~0.48 * size of the center, safely inside the cx/cy-
     * centered canvas of edge length `size`. */
    lv_coord_t ring_r  = (size * 27) / 100;
    lv_coord_t ring_w  = LV_MAX(2, (size * 8) / 100);
    if(ring_r < 4) ring_r = 4;

    /* Roundabout ring. */
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = ink;
    arc_dsc.width = ring_w;
    arc_dsc.opa = LV_OPA_COVER;
    lv_canvas_draw_arc(canvas, cx, cy, ring_r, 0, 360, &arc_dsc);

    /* Entry stub: fixed, plain, always at the bottom. */
    {
        lv_coord_t entry_len = (size * 13) / 100;
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = ink;
        line_dsc.width = LV_MAX(2, (size * 6) / 100);
        line_dsc.opa = LV_OPA_COVER;
        line_dsc.round_start = 1;
        line_dsc.round_end = 1;

        lv_point_t pts[2];
        roundabout_icon_point(&pts[0], cx, cy, ring_r - ring_w / 2, ROUNDABOUT_ICON_ENTRY_ANGLE_DEG);
        roundabout_icon_point(&pts[1], cx, cy, ring_r + entry_len, ROUNDABOUT_ICON_ENTRY_ANGLE_DEG);
        lv_canvas_draw_line(canvas, pts, 2, &line_dsc);
    }

    /* Exit stub + arrowhead: bold, rotated to the exit angle. */
    {
        int32_t raw_step = (int32_t)exit_number * ROUNDABOUT_ICON_ANGLE_STEP_DEG;
#if ROUNDABOUT_ICON_COUNTERCLOCKWISE
        int32_t exit_angle = ROUNDABOUT_ICON_ENTRY_ANGLE_DEG - raw_step;
#else
        int32_t exit_angle = ROUNDABOUT_ICON_ENTRY_ANGLE_DEG + raw_step;
#endif
        exit_angle = ((exit_angle % 360) + 360) % 360;

        lv_coord_t exit_len = (size * 15) / 100;
        lv_coord_t exit_outer = ring_r + exit_len;

        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = ink;
        line_dsc.width = LV_MAX(3, (size * 9) / 100);
        line_dsc.opa = LV_OPA_COVER;
        line_dsc.round_start = 1;
        line_dsc.round_end = 1;

        lv_point_t pts[2];
        roundabout_icon_point(&pts[0], cx, cy, ring_r - ring_w / 2, exit_angle);
        roundabout_icon_point(&pts[1], cx, cy, exit_outer, exit_angle);
        lv_canvas_draw_line(canvas, pts, 2, &line_dsc);

        /* Arrowhead: a small dart at the tip, pointing outward. */
        lv_coord_t arrow_ext = (size * 6) / 100;
        lv_point_t tri[3];
        roundabout_icon_point(&tri[0], cx, cy, exit_outer + arrow_ext, exit_angle);
        roundabout_icon_point(&tri[1], cx, cy, exit_outer, exit_angle - 20);
        roundabout_icon_point(&tri[2], cx, cy, exit_outer, exit_angle + 20);

        lv_draw_rect_dsc_t poly_dsc;
        lv_draw_rect_dsc_init(&poly_dsc);
        poly_dsc.bg_color = ink;
        poly_dsc.bg_opa = LV_OPA_COVER;
        poly_dsc.border_width = 0;
        lv_canvas_draw_polygon(canvas, tri, 3, &poly_dsc);
    }

    /* Exit number, centered inside the ring -- disambiguates exit counts
     * beyond what the 45-degree-stepped arrow angle alone can convey. */
    {
        char txt[4];
        snprintf(txt, sizeof(txt), "%u", (unsigned)exit_number);

        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.color = ink;
        label_dsc.opa = LV_OPA_COVER;
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        label_dsc.font = roundabout_icon_pick_font(size);

        lv_coord_t line_h = lv_font_get_line_height(label_dsc.font);
        lv_coord_t label_y = cy - line_h / 2;
        lv_canvas_draw_text(canvas, 0, label_y, size, &label_dsc, txt);
    }
}

static roundabout_icon_slot_t * roundabout_icon_find_slot(lv_coord_t size)
{
    for(uint8_t i = 0; i < ROUNDABOUT_ICON_CACHE_SLOTS; i++) {
        if(s_slots[i].canvas != NULL && s_slots[i].size == size) {
            return &s_slots[i];
        }
    }
    return NULL;
}

static void roundabout_icon_free_slot(roundabout_icon_slot_t * slot)
{
    if(slot->canvas != NULL) {
        lv_obj_del(slot->canvas);
        slot->canvas = NULL;
    }
    if(slot->buf != NULL) {
        lv_mem_free(slot->buf);
        slot->buf = NULL;
    }
    slot->size = 0;
    slot->exit_number = 0;
}

static roundabout_icon_slot_t * roundabout_icon_alloc_slot(lv_coord_t size)
{
    /* Reuse a free slot if there is one, else evict round-robin. */
    roundabout_icon_slot_t * slot = NULL;
    for(uint8_t i = 0; i < ROUNDABOUT_ICON_CACHE_SLOTS; i++) {
        if(s_slots[i].canvas == NULL) {
            slot = &s_slots[i];
            break;
        }
    }
    if(slot == NULL) {
        slot = &s_slots[s_next_evict];
        s_next_evict = (uint8_t)((s_next_evict + 1) % ROUNDABOUT_ICON_CACHE_SLOTS);
        roundabout_icon_free_slot(slot);
    }

    uint32_t buf_size = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(size, size);
    void * buf = lv_mem_alloc(buf_size);
    if(buf == NULL) {
        return NULL;
    }

    /* Parented to the system layer so it always has a valid parent
     * regardless of which screen is currently active, and hidden since it
     * is only ever used as an off-screen pixel buffer, never drawn as a
     * widget itself -- callers plug the resulting lv_img_dsc_t into their
     * own, already-visible lv_img objects instead (see roundabout-icon.h). */
    lv_obj_t * canvas = lv_canvas_create(lv_layer_sys());
    if(canvas == NULL) {
        lv_mem_free(buf);
        return NULL;
    }
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_canvas_set_buffer(canvas, buf, size, size, LV_IMG_CF_TRUE_COLOR_ALPHA);

    slot->canvas = canvas;
    slot->buf = buf;
    slot->size = size;
    slot->exit_number = 0;
    return slot;
}

const lv_img_dsc_t * roundabout_icon_get(uint8_t exit_number, lv_coord_t size)
{
    if(size <= 0) {
        return NULL;
    }
    if(exit_number == 0) {
        exit_number = 1; /* defensive: protocol defines exits as 1-based */
    }

    roundabout_icon_slot_t * slot = roundabout_icon_find_slot(size);
    if(slot == NULL) {
        slot = roundabout_icon_alloc_slot(size);
        if(slot == NULL) {
            return NULL;
        }
    }

    if(slot->exit_number != exit_number) {
        roundabout_icon_render(slot->canvas, size, exit_number);
        slot->exit_number = exit_number;
    }

    return lv_canvas_get_img(slot->canvas);
}

void roundabout_icon_deinit(void)
{
    for(uint8_t i = 0; i < ROUNDABOUT_ICON_CACHE_SLOTS; i++) {
        roundabout_icon_free_slot(&s_slots[i]);
    }
}
