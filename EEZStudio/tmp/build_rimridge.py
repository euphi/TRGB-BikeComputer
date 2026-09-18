#!/usr/bin/env python3
"""Build an experimental 'RimRidge' page in the EEZ project from the
Rim & Ridge mainscreen design study (claude.ai/artifact/Q2DsdjYZ7sBQKEZ794F5uj).

Scope for this pass (see chat for the full list):
- Faithful: layout/positions (design's 480x480 SVG coords map 1:1 onto our
  480x480 display), exact color palette, speed arc (track+value, same
  gap-at-bottom style as the existing arcs), HR value + GRADIENT bar
  (explicitly requested simplification of the design's 5-segment zone
  band), nav/tour pill badges, all text values, custom vector icons
  (rasterized here from the design's own SVG paths via rsvg-convert).
- Placeholder: typography uses built-in MONTSERRAT_* everywhere except the
  big center speed number, which reuses the already-embedded 'by7x128'
  custom font (128px digits-only - a near-perfect accidental fit for this
  design's 130px speed readout). The design's real fonts (Big Shoulders
  Display, IBM Plex Mono/Sans) are Google Fonts, not yet sourced/embedded.
- Skipped: the average-speed marker tick on the arc, the subtle inner rim
  highlight ring - both small cosmetic details, not structural.
- Settings button reuses the existing 'SettingsIcon' bitmap (already
  embedded for MainNoFL) instead of rasterizing the design's own gear glyph,
  for consistency with the rest of the project.
"""
import json, re, shutil, time, uuid, base64, subprocess, os

REPO = "/home/ian/Coding/Bike/TRGB-BikeComputer"
PROJ = f"{REPO}/EEZStudio/TRGB-BikeComputer.eez-project"
PAGE_NAME = "RimRidge"
ICON_DIR = "/tmp/claude-1000/-home-ian-Coding-Bike-TRGB-BikeComputer/2f8ac32e-c071-4372-a930-f31be764c333/scratchpad/rimridge/icons"
os.makedirs(ICON_DIR, exist_ok=True)

def oid():
    return str(uuid.uuid4())

# ---------- palette (Rim & Ridge) ----------
PALETTE_HINTS = {
    "#161B1F": "RRBackground",
    "#3A362E": "RRRimOutline",
    "#332F28": "RRArcTrack",
    "#CBA36B": "RRBrass",
    "#F3ECDF": "RRParchmentBright",
    "#E7E2D6": "RRParchment",
    "#9BA097": "RRMuted",
    "#7FA08F": "RRSage",
    "#1E252B": "RRPanelBg",
    "#282019": "RRTourBg",
    "#6C90B0": "RRZoneBlue",
    "#6FA98C": "RRZoneGreen",
    "#D7B463": "RRZoneYellow",
    "#CE8A4C": "RRZoneOrange",
    "#C1604A": "RRZoneRed",
}

class Palette:
    def __init__(self, colors_list, themes_list):
        self.colors = colors_list
        self.themes = themes_list
        self.by_hex = {}
        for i, c in enumerate(colors_list):
            if i < len(themes_list[0]["colors"]):
                self.by_hex[themes_list[0]["colors"][i].lower()] = c["name"]

    def token(self, hexval):
        hexval = hexval.lower()
        if hexval in self.by_hex:
            return self.by_hex[hexval]
        name = PALETTE_HINTS.get(hexval.upper(), f"RRColor{hexval[1:]}")
        existing = {c["name"] for c in self.colors}
        base, n = name, 2
        while name in existing:
            name = f"{base}{n}"; n += 1
        self.colors.append({"objID": oid(), "name": name})
        for th in self.themes:
            th["colors"].append(hexval)
        self.by_hex[hexval] = name
        return name

# ---------- icon rasterization (from the design's own SVG paths) ----------
ICON_SVGS = {
    "rr_icon_wifi": '''<g fill="none" stroke="#CBA36B" stroke-width="2.1" stroke-linecap="round">
        <circle cx="0" cy="6" r="1.6" fill="#CBA36B" stroke="none"/>
        <path d="M -5,1 A 7 7 0 0 1 5,1"/>
        <path d="M -9,-4 A 13 13 0 0 1 9,-4"/></g>''',
    "rr_icon_gps": '''<path d="M0,-9 C4.4,-9 7.5,-5.7 7.5,-1.6 C7.5,4 0,11 0,11 C0,11 -7.5,4 -7.5,-1.6 C-7.5,-5.7 -4.4,-9 0,-9 Z" fill="none" stroke="#CBA36B" stroke-width="2"/>
        <circle cx="0" cy="-1.6" r="2.3" fill="#CBA36B"/>''',
    "rr_icon_battery": '''<rect x="-9" y="-5.5" width="16" height="11" rx="1.5" fill="none" stroke="#CBA36B" stroke-width="1.8"/>
        <rect x="7.3" y="-2.5" width="2.2" height="5" rx="0.6" fill="#CBA36B"/>
        <rect x="-7" y="-3.5" width="8.8" height="7" rx="0.6" fill="#7FA08F"/>''',
    "rr_icon_turn": '''<g fill="none" stroke="#CBA36B" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round">
        <path d="M0,12 L0,-4 L-9,-4"/><path d="M-9,-4 L-4,-8 M-9,-4 L-4,0"/></g>''',
    "rr_icon_cadence": '''<g fill="none" stroke="#CBA36B" stroke-width="2" stroke-linecap="round">
        <path d="M -6,2 A 7 7 0 1 1 3,7.5"/><path d="M3,7.5 L7,7 L4,3" fill="#CBA36B" stroke="none"/></g>''',
    "rr_icon_power": '<polygon points="2,-9 -4,1 0,1 -2,9 5,-1 1,-1" fill="#CBA36B"/>',
    "rr_icon_temp": '''<g fill="none" stroke="#CBA36B" stroke-width="1.8">
        <rect x="-3" y="-9" width="6" height="13" rx="3"/><circle cx="0" cy="7" r="4.5" fill="#CBA36B" stroke="none"/></g>''',
    "rr_icon_height": '<polyline points="-9,7 -2,-6 3,0 9,-8" fill="none" stroke="#7FA08F" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/>',
    "rr_icon_gradient": '''<g stroke="#CBA36B" stroke-width="2.2" stroke-linecap="round">
        <line x1="-8" y1="8" x2="8" y2="-8"/><path d="M2,-8 L8,-8 L8,-2" fill="none"/></g>''',
    "rr_icon_heart": '<path d="M0,4 C-6,-2 -10,-8 -4,-11 C-1,-12.5 0,-9.5 0,-8 C0,-9.5 1,-12.5 4,-11 C10,-8 6,-2 0,4 Z" fill="#CBA36B"/>',
    "rr_icon_pause": '''<g fill="#CBA36B"><rect x="-5" y="-10" width="5" height="20"/><rect x="5" y="-10" width="5" height="20"/></g>''',
}
ICON_VB = "-16 -16 32 32"

def rasterize_icons():
    result = {}
    for name, inner in ICON_SVGS.items():
        svg_path = f"{ICON_DIR}/{name}.svg"
        png_path = f"{ICON_DIR}/{name}.png"
        with open(svg_path, "w") as f:
            f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{ICON_VB}">{inner}</svg>')
        # Render 1 SVG unit = 1 px: the viewBox is "-16 -16 32 32" and this
        # design's SVG coordinates already map 1:1 onto our 480x480 display,
        # so the bitmap's native pixel size must equal the target on-screen
        # size directly - LVGL renders bitmaps at native size (sizeMode
        # VIRTUAL does NOT rescale to the widget's width/height fields,
        # confirmed from the oversized-icon screenshot: rendering at 96x96
        # here made every icon show up 3x too big on the actual canvas).
        subprocess.run(["rsvg-convert", "-w", "32", "-h", "32", svg_path, "-o", png_path], check=True)
        with open(png_path, "rb") as f:
            result[name] = base64.b64encode(f.read()).decode()
    return result

# ---------- EEZ widget builders (mirroring MainNoFL / Example page shapes) ----------
BASE_FLAGS = "CLICK_FOCUSABLE|GESTURE_BUBBLE|PRESS_LOCK|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_WITH_ARROW|SNAPPABLE"

def base_widget(wtype, left, top, width, height, width_unit="px", height_unit="px"):
    return {
        "objID": oid(), "type": wtype,
        "left": left, "top": top, "width": width, "height": height,
        "customInputs": [], "customOutputs": [],
        "style": {"objID": oid(), "useStyle": "default", "conditionalStyles": [], "childStyles": []},
        "timeline": [], "eventHandlers": [],
        "leftUnit": "px", "topUnit": "px", "widthUnit": width_unit, "heightUnit": height_unit,
        "children": [],
        "widgetFlags": BASE_FLAGS,
        "hiddenFlagType": "literal", "clickableFlag": True, "clickableFlagType": "literal",
        "flagScrollbarMode": "", "flagScrollDirection": "", "scrollSnapX": "", "scrollSnapY": "",
        "checkedStateType": "literal", "disabledStateType": "literal", "states": "",
        "localStyles": {"objID": oid()},
        "group": "", "groupIndex": 0,
    }

def set_style(w, props, part="MAIN", state="DEFAULT"):
    w["localStyles"].setdefault("definition", {}).setdefault(part, {}).setdefault(state, {}).update(props)

CENTER_X, CENTER_Y = 240, 240

def center_label(ident, svg_x, svg_y, font_size, text, color_tok, font, baseline_frac=0.3):
    """text-anchor=middle SVG label -> content-sized EEZ label, align=CENTER,
    left/top = offset of the label's own center from the SCREEN's center
    (this widget is always a direct child of the screen here)."""
    adj_y = svg_y - font_size * baseline_frac
    left = round(svg_x - CENTER_X)
    top = round(adj_y - CENTER_Y)
    w = base_widget("LVGLLabelWidget", left, top, 40, 20, "content", "content")
    w["identifier"] = ident
    w["text"] = text; w["textType"] = "literal"; w["longMode"] = "WRAP"
    w["recolor"] = False; w["useStaticText"] = True
    set_style(w, {"text_color": color_tok, "text_font": font, "align": "CENTER"})
    return w

def topleft_label(ident, svg_x, svg_y, font_size, text, color_tok, font, ascent_frac=0.8):
    """text-anchor=start (default) SVG label -> content-sized EEZ label,
    left/top = absolute top-left (svg_x is already the left edge; svg_y is
    the baseline, converted to an approximate glyph-box top)."""
    left = round(svg_x)
    top = round(svg_y - font_size * ascent_frac)
    w = base_widget("LVGLLabelWidget", left, top, 40, 20, "content", "content")
    w["identifier"] = ident
    w["text"] = text; w["textType"] = "literal"; w["longMode"] = "WRAP"
    w["recolor"] = False; w["useStaticText"] = True
    set_style(w, {"text_color": color_tok, "text_font": font})
    return w

def icon(ident, cx, cy, bitmap_name, size=32):
    """Icon anchor (cx,cy) from the SVG is the icon's own visual center
    (its paths are locally centered on the g's translate point) -> absolute
    top-left = (cx - size/2, cy - size/2), plain TOP_LEFT, no align."""
    left = round(cx - size / 2)
    top = round(cy - size / 2)
    w = base_widget("LVGLImageWidget", left, top, size, size, "content", "content")
    w["identifier"] = ident
    w["image"] = bitmap_name
    w["setPivot"] = False; w["pivotX"] = 0; w["pivotY"] = 0
    w["zoom"] = 256; w["angle"] = 0; w["innerAlign"] = "CENTER"; w["sizeMode"] = "VIRTUAL"
    w["value"] = 0; w["valueType"] = "literal"; w["previewValue"] = 0
    return w

def rect_container(ident, x, y, w_, h_, props, children=None):
    """SVG rect x,y is already the top-left corner - plain TOP_LEFT, no align."""
    w = base_widget("LVGLContainerWidget", round(x), round(y), w_, h_)
    w["identifier"] = ident
    w["children"] = children or []
    set_style(w, props)
    return w

def circle_container(ident, cx, cy, diameter, props, children=None):
    """SVG circle cx,cy,r -> top-left = (cx-r, cy-r), plain TOP_LEFT."""
    r = diameter / 2
    return rect_container(ident, cx - r, cy - r, diameter, diameter, props, children)

def arc(ident, left, top, size, rng, value, track_tok, ind_tok, width=16, bg_angles=(120, 60)):
    """left/top here are already the absolute top-left of the arc's bounding
    box (matches the verified Example-page ArcWidget: plain TOP_LEFT, no
    align override needed for a widget whose own size we already know)."""
    w = base_widget("LVGLArcWidget", left, top, size, size)
    w["identifier"] = ident
    w["useAngle"] = False
    w["rangeMin"] = rng[0]; w["rangeMinType"] = "literal"
    w["rangeMax"] = rng[1]; w["rangeMaxType"] = "literal"
    w["value"] = value; w["valueType"] = "literal"; w["previewValue"] = str(value)
    w["mode"] = "NORMAL"
    for k, v in (("startAngle", bg_angles[0]), ("endAngle", bg_angles[1]),
                 ("bgStartAngle", bg_angles[0]), ("bgEndAngle", bg_angles[1])):
        w[k] = v; w[k + "Type"] = "literal"; w["preview" + k[0].upper() + k[1:]] = str(v)
    w["rotation"] = 0; w["rotationType"] = "literal"; w["previewRotation"] = "0"
    set_style(w, {"arc_color": track_tok, "arc_width": width})
    set_style(w, {"arc_color": ind_tok, "arc_width": width}, part="INDICATOR")
    # KNOB part deliberately left unstyled (LVGL default) - same as the
    # existing ArcCad on MainNoFL; hiding it needs a verified style
    # property I don't have confirmed, revisit after the canvas check.
    return w

def bar_gradient(ident, x, y, w_, h_, rng, value, c1_tok, c2_tok, radius=7):
    """SVG rect x,y is already top-left - plain TOP_LEFT, no align."""
    w = base_widget("LVGLBarWidget", round(x), round(y), w_, h_)
    w["identifier"] = ident
    w["min"] = rng[0]; w["minType"] = "literal"
    w["max"] = rng[1]; w["maxType"] = "literal"
    w["mode"] = "NORMAL"
    w["value"] = value; w["valueType"] = "literal"; w["previewValue"] = str(value)
    w["valueStart"] = rng[0]; w["valueStartType"] = "literal"; w["previewValueStart"] = str(rng[0])
    w["enableAnimation"] = False
    props = {"bg_color": c1_tok, "bg_grad_color": c2_tok, "bg_grad_dir": "HOR",
             "bg_main_stop": 0, "bg_grad_stop": 255, "radius": radius}
    set_style(w, props)
    set_style(w, props, part="INDICATOR")
    return w

# ---------- main ----------

def main():
    proj = json.load(open(PROJ))
    if any(p["name"] == PAGE_NAME for p in proj["userPages"]):
        print(f"Page '{PAGE_NAME}' already exists - aborting.")
        return

    ts = time.strftime("%Y%m%d-%H%M%S")
    backup = PROJ + f".bak.{ts}"
    shutil.copyfile(PROJ, backup)
    print("Backup:", backup)

    pal = Palette(proj["colors"], proj["themes"])
    for hexval in PALETTE_HINTS:
        pal.token(hexval)  # pre-register with intended names, dedup against existing

    icons_b64 = rasterize_icons()
    existing_bitmaps = {b["name"] for b in proj["bitmaps"]}
    for name, b64 in icons_b64.items():
        if name not in existing_bitmaps:
            proj["bitmaps"].append({"objID": oid(), "name": name, "image": f"data:image/png;base64,{b64}"})
    print(f"Bitmaps added: {len(icons_b64)} icons (+ reusing existing 'SettingsIcon')")

    T = pal.token  # shorthand
    bg = T("#161B1F"); brass = T("#CBA36B"); parch = T("#E7E2D6"); parch_bright = T("#F3ECDF")
    muted = T("#9BA097"); sage = T("#7FA08F"); panel_bg = T("#1E252B"); track = T("#332F28")
    tour_bg = T("#282019")
    z_blue = T("#6C90B0"); z_red = T("#C1604A")

    # All widgets are direct children of the screen (flattened - no nested
    # containers) so every position is computed straight from the source
    # SVG's own absolute coordinates, with a single, consistent rule per
    # widget kind (see the builder docstrings above). This avoids the
    # parent-relative-coordinate class of bug found in the first attempt.
    children = [
        # speed arc (track + value) - fixed-size, plain TOP_LEFT like the
        # verified Example-page ArcWidget
        arc("rr_speed_arc", 18, 18, 444, (0, 60), 28, track, brass, width=16),

        # top status icons (translate anchor = icon's own visual center)
        icon("rr_ic_wifi", 188, 60, "rr_icon_wifi"),
        icon("rr_ic_gps", 240, 58, "rr_icon_gps"),
        icon("rr_ic_battery", 292, 60, "rr_icon_battery"),

        # nav hint pill (rect x/y = literal top-left) + its icon/label
        rect_container("rr_nav_pill", 150, 92, 180, 40,
                        {"bg_color": panel_bg, "radius": 20, "border_color": brass, "border_width": 1, "border_opa": 90}),
        icon("rr_ic_turn", 178, 112, "rr_icon_turn"),
        topleft_label("rr_nav_dist", 203, 120, 21, "180 m", parch, "MONTSERRAT_20"),

        # cadence (left) - value/unit are text-anchor=middle -> center_label
        icon("rr_ic_cadence", 80, 193, "rr_icon_cadence"),
        center_label("rr_cadence_val", 80, 223, 26, "82", parch, "MONTSERRAT_26"),
        center_label("rr_cadence_unit", 80, 239, 13, "rpm", muted, "MONTSERRAT_14"),

        # power (right)
        icon("rr_ic_power", 400, 190, "rr_icon_power"),
        center_label("rr_power_val", 400, 223, 26, "186", parch, "MONTSERRAT_26"),
        center_label("rr_power_unit", 400, 239, 13, "W", muted, "MONTSERRAT_14"),

        # center speed (text-anchor=middle in the SVG)
        center_label("rr_speed_val", 240, 272, 130, "28.4", parch_bright, "by7x128"),
        center_label("rr_speed_unit", 240, 301, 18, "KM/H", muted, "MONTSERRAT_18"),

        # temp / height / gradient row - these three are left-anchored in
        # the SVG (no text-anchor) -> topleft_label
        icon("rr_ic_temp", 88, 325, "rr_icon_temp"),
        topleft_label("rr_temp_val", 104, 334, 22, "14°", parch, "MONTSERRAT_22"),
        icon("rr_ic_height", 208, 325, "rr_icon_height"),
        topleft_label("rr_height_val", 224, 334, 22, "612m", parch, "MONTSERRAT_22"),
        icon("rr_ic_gradient", 328, 325, "rr_icon_gradient"),
        topleft_label("rr_gradient_val", 344, 334, 22, "+4.2%", parch, "MONTSERRAT_22"),

        # HR: value (left-anchored) + GRADIENT bar (explicit simplification
        # of the design's 5-zone band) - bar rect x/y is literal top-left
        icon("rr_ic_heart", 218, 353, "rr_icon_heart"),
        topleft_label("rr_hr_val", 232, 359, 20, "142", parch, "MONTSERRAT_20"),
        bar_gradient("rr_hr_bar", 110, 366, 260, 14, (40, 200), 142, z_blue, z_red, radius=7),

        # bottom: pause button (circle cx/cy/r), distance + tour pill, settings button
        circle_container("rr_btn_pause", 148, 408, 50,
                          {"bg_color": panel_bg, "radius": 25, "border_color": brass, "border_width": 2}),
        icon("rr_ic_pause", 148, 408, "rr_icon_pause", size=32),
        center_label("rr_distance_val", 240, 403, 24, "38.6 km", parch, "MONTSERRAT_24"),
        rect_container("rr_tour_pill", 210, 412, 60, 19, {"bg_color": tour_bg, "radius": 9}),
        center_label("rr_tour_label", 240, 426, 12, "TOUR 1", brass, "MONTSERRAT_14"),
        circle_container("rr_btn_settings", 332, 408, 50,
                          {"bg_color": panel_bg, "radius": 25, "border_color": brass, "border_width": 2}),
        icon("rr_ic_settings", 332, 408, "SettingsIcon", size=48),  # SettingsIcon's real native size
    ]

    screen_root = {
        "objID": oid(), "type": "LVGLScreenWidget",
        "left": 0, "top": 0, "width": 480, "height": 480,
        "customInputs": [], "customOutputs": [],
        "style": {"objID": oid(), "useStyle": "default", "conditionalStyles": [], "childStyles": []},
        "timeline": [], "eventHandlers": [],
        "leftUnit": "px", "topUnit": "px", "widthUnit": "px", "heightUnit": "px",
        "children": children,
        "widgetFlags": "CLICKABLE|PRESS_LOCK|CLICK_FOCUSABLE|GESTURE_BUBBLE|SNAPPABLE|SCROLLABLE|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER",
        "hiddenFlagType": "literal", "clickableFlag": True, "clickableFlagType": "literal",
        "checkedStateType": "literal", "disabledStateType": "literal", "states": "",
        "localStyles": {"objID": oid()},
        "groupIndex": 0,
    }
    set_style(screen_root, {"bg_color": bg})

    page = {
        "objID": oid(),
        "connectionLines": [], "localVariables": [], "componentGroups": [], "userProperties": [],
        "name": PAGE_NAME,
        "left": 0, "top": 0, "width": 480, "height": 480,
        "isUsedAsUserWidget": False, "createAtStart": True, "deleteOnScreenUnload": False,
        "components": [screen_root],
    }
    proj["userPages"].append(page)

    with open(PROJ, "w") as f:
        json.dump(proj, f, indent=2)
        f.write("\n")

    json.load(open(PROJ))
    print("G1 JSON parse: OK")

    color_names = {c["name"] for c in proj["colors"]}
    font_names = {f["name"] for f in proj["fonts"]} | {f"MONTSERRAT_{n}" for n in range(6, 50, 2)}
    bitmap_names = {b["name"] for b in proj["bitmaps"]}
    missing = []
    def check(node):
        if isinstance(node, dict):
            defn = node.get("localStyles", {}).get("definition", {})
            for part, states in defn.items():
                for state, props in states.items():
                    for k, v in props.items():
                        if k in ("bg_color", "text_color", "arc_color", "border_color") and isinstance(v, str) and not v.startswith("#"):
                            if v not in color_names:
                                missing.append(("color", v))
                        if k == "text_font" and v not in font_names:
                            missing.append(("font", v))
            if node.get("type") == "LVGLImageWidget" and node.get("image") not in bitmap_names:
                missing.append(("bitmap", node.get("image")))
            for v in node.values():
                check(v)
        elif isinstance(node, list):
            for x in node:
                check(x)
    check(page)
    print(f"G3 Named-reference check: {len(missing)} unresolved")
    for kind, name in sorted(set(missing)):
        print(f"   - {kind}: {name!r}")

    print()
    print("New color tokens (Rim & Ridge palette):")
    for c, hx in zip(proj["colors"][-len(PALETTE_HINTS):], proj["themes"][0]["colors"][-len(PALETTE_HINTS):]):
        print(f"  {c['name']}: {hx}")

if __name__ == "__main__":
    main()
