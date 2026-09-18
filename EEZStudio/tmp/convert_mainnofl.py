#!/usr/bin/env python3
"""Convert SquareLine/Prj_BC_ScreenMainNoFL/BC_noFL_Main.spj into a new
'MainNoFL' page inside EEZStudio/TRGB-BikeComputer.eez-project.

Pilot conversion for the SquareLine -> EEZ Studio migration (see
.claude/skills/eezstudio/SKILL.md and PROJECT-NOTES.md). Mirrors the
widget JSON shapes found in the hand-made "Example 1/2/3" pages of the
same project rather than inventing shapes.

NOT converted (by design, see PROJECT-NOTES.md): SquareLine event-handler
wiring (CLICKED/LONG_PRESSED/GESTURE_* -> CHANGE SCREEN / CALL FUNCTION).
Reported at the end for manual rewiring later.
"""
import json, re, shutil, time, uuid, base64, sys, os

REPO = "/home/ian/Coding/Bike/TRGB-BikeComputer"
PROJ = f"{REPO}/EEZStudio/TRGB-BikeComputer.eez-project"
SPJ = f"{REPO}/SquareLine/Prj_BC_ScreenMainNoFL/BC_noFL_Main.spj"
ASSETS = f"{REPO}/SquareLine/Prj_BC_ScreenMainNoFL/assets"
PAGE_NAME = "MainNoFL"

def oid():
    return str(uuid.uuid4())

def snake(name):
    s1 = re.sub(r'(.)([A-Z][a-z]+)', r'\1_\2', name)
    s2 = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', s1)
    return s2.lower()

# ---------- SquareLine .spj extraction (see scratchpad/extract_spj.py) ----------

def rgba_to_hex(arr):
    return "#%02x%02x%02x" % (arr[0], arr[1], arr[2])

COLOR_KEYS = {"Text_Color", "Bg_Color", "Arc_Color", "Border_Color",
              "Image_reColor", "Bg_gradiens_Color", "Line_Color", "Outline_Color"}

def parse_style_prop(p):
    st = p["strtype"].split("/", 1)[1]
    if "intarray" in p:
        return st, (rgba_to_hex(p["intarray"]) if st in COLOR_KEYS else p["intarray"])
    if "integer" in p:
        return st, p["integer"]
    if "strval" in p:
        return st, p["strval"]
    return st, None

def parse_style_block(p):
    result = {}
    for state_block in p.get("childs", []):
        state = state_block.get("strval", "DEFAULT")
        props = {}
        for c in state_block.get("childs", []):
            k, v = parse_style_prop(c)
            props[k] = v
        result[state] = props
    return result

def get_name(node):
    for p in node.get("properties") or []:
        if p["strtype"] == "OBJECT/Name":
            return p.get("strval")
    return None

def get_props(node):
    out = {"hidden": False, "clickable": False}
    for p in node.get("properties") or []:
        st = p["strtype"]
        if st == "OBJECT/Position":
            out["position"] = p.get("intarray")
        elif st == "OBJECT/Size":
            out["size"] = p.get("intarray")
        elif st == "OBJECT/Align":
            out["align"] = p.get("strval")
        elif st == "OBJECT/Hidden":
            out["hidden"] = p.get("strval") == "True"
        elif st == "OBJECT/Clickable":
            out["clickable"] = p.get("strval") == "True"
        elif st == "LABEL/Text":
            out["text"] = p.get("strval")
        elif st == "LABEL/Long_mode":
            out["long_mode"] = p.get("strval")
        elif st == "IMAGE/Asset":
            out["image_asset"] = p.get("strval")
        elif st in ("ARC/Range", "BAR/Range"):
            out["range"] = p.get("intarray")
        elif st in ("ARC/Value", "BAR/Value"):
            out["value"] = p.get("integer")
        elif st == "ARC/Bg_angles":
            out["bg_angles"] = p.get("intarray")
        elif st in ("ARC/Mode", "BAR/Mode"):
            out["mode"] = p.get("strval")
        elif "Style_" in st:
            out.setdefault("styles", {})[st.split("/")[1]] = parse_style_block(p)
        elif st == "OBJECT/Layout_type":
            # NB: p["strval"] is unreliable (stays "No_layout" even when flex
            # is active) - the real switch is the numeric LayoutType sub-field.
            # 0 = free/absolute positioning, 1 = flex (LVGL LV_LAYOUT_FLEX).
            if p.get("LayoutType"):
                out["layout_type"] = p.get("LayoutType")
                out["layout_flow"] = p.get("Flow")
                out["layout_main_align"] = p.get("MainAlignment")
                out["layout_cross_align"] = p.get("CrossAlignment")
    return out

def walk_spj(node, parent=None):
    """Returns list of (widget_dict, parent_name) preserving document order,
    plus builds parent->children name lists implicitly via 'parent' field."""
    out = []
    name = get_name(node)
    kind = node.get("saved_objtypeKey")
    cur_parent = parent
    if kind not in (None, "STARTEVENTS"):
        w = {"sl_type": kind, "name": name, "parent": parent, **get_props(node)}
        out.append(w)
        cur_parent = name
    for c in node.get("children") or []:
        out.extend(walk_spj(c, cur_parent))
    return out

# ---------- color palette ----------

class Palette:
    def __init__(self, colors_list, themes_list):
        self.colors = colors_list
        self.themes = themes_list
        self.by_hex = {}
        for i, c in enumerate(colors_list):
            hexval = themes_list[0]["colors"][i] if i < len(themes_list[0]["colors"]) else None
            if hexval:
                self.by_hex[hexval.lower()] = c["name"]

    def token(self, hexval, proposed_name):
        hexval = hexval.lower()
        if hexval in self.by_hex:
            return self.by_hex[hexval]
        name = proposed_name
        n = 2
        existing = {c["name"] for c in self.colors}
        while name in existing:
            name = f"{proposed_name}{n}"
            n += 1
        self.colors.append({"objID": oid(), "name": name})
        for th in self.themes:
            th["colors"].append(hexval)
        self.by_hex[hexval] = name
        return name

# name proposals per known hex, in first-seen order for MainNoFL
COLOR_NAME_HINTS = {
    "#ffffff": "ScreenBg",
    "#c8fff7": "PanelNavBg",
    "#00ff80": "NavIconRecolor",
    "#000000": "TextPrimary",
    "#e0e0e0": "ArcSpeedTrack",
    "#00ff26": "ArcSpeedKnob",
    "#e5deae": "ArcAvgTrack",
    "#ffe200": "ArcAvgIndicator",
    "#ff3a00": "ArcAvgKnob",
    "#005766": "ArcCadColor",
    "#3fff00": "GradientGoodGreen",
    "#ff0000": "GradientDangerRed",
    "#fdffde": "PanelClockBg",
    "#00ff00": "GradientFullGreen",
    "#85ebff": "HeightPanelBg",
    "#0000ff": "StateIconRecolor",
}

FONT_MAP = {
    "montserrat_24": "MONTSERRAT_24",
    "montserrat_28": "MONTSERRAT_28",
    "montserrat_36": "MONTSERRAT_36",
    "montserrat_48": "MONTSERRAT_48",
    "by7x128": "by7x128",       # custom, must be added by hand in EEZ Studio first
    "FontSchild": "FontSchild", # custom, must be added by hand in EEZ Studio first
}
CUSTOM_FONTS_NEEDED = {
    "by7x128": dict(ttf="SquareLine/assets/5by7.ttf", size=128, bpp=1, range="0x2B-0x39"),
    "FontSchild": dict(ttf="SquareLine/assets/schilder.ttf", size=64, bpp=1, range="0x20-0x7F"),
}

# ---------- EEZ widget builders (mirroring Example 1/2/3 shapes) ----------

BASE_FLAGS = "CLICK_FOCUSABLE|GESTURE_BUBBLE|PRESS_LOCK|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_WITH_ARROW|SNAPPABLE"

def base_widget(wtype, left, top, width, height, width_unit="px", height_unit="px"):
    return {
        "objID": oid(),
        "type": wtype,
        "left": left, "top": top, "width": width, "height": height,
        "customInputs": [], "customOutputs": [],
        "style": {"objID": oid(), "useStyle": "default", "conditionalStyles": [], "childStyles": []},
        "timeline": [], "eventHandlers": [],
        "leftUnit": "px", "topUnit": "px", "widthUnit": width_unit, "heightUnit": height_unit,
        "children": [],
        "widgetFlags": BASE_FLAGS,
        "hiddenFlagType": "literal",
        "clickableFlag": True,
        "clickableFlagType": "literal",
        "flagScrollbarMode": "", "flagScrollDirection": "",
        "scrollSnapX": "", "scrollSnapY": "",
        "checkedStateType": "literal", "disabledStateType": "literal",
        "states": "",
        "localStyles": {"objID": oid()},
        "group": "", "groupIndex": 0,
    }

def set_local_style(w, part, state, props):
    ls = w["localStyles"]
    ls.setdefault("definition", {}).setdefault(part, {}).setdefault(state, {}).update(props)

def apply_align_offset(w, sw_align, pos):
    if sw_align and sw_align != "TOP_LEFT":
        set_local_style(w, "MAIN", "DEFAULT", {"align": sw_align})
    w["left"] = pos[0] if pos else 0
    w["top"] = pos[1] if pos else 0

def build_label(sw, pal, identifier):
    content = (sw.get("size") in (None, [1, 1]))
    w = base_widget("LVGLLabelWidget", 0, 0, sw.get("size", [1, 1])[0] or 40, sw.get("size", [1, 1])[1] or 20,
                     width_unit="content" if content else "px", height_unit="content" if content else "px")
    w["identifier"] = identifier
    w["text"] = sw.get("text", "")
    w["textType"] = "literal"
    w["longMode"] = sw.get("long_mode", "WRAP")
    w["recolor"] = False
    w["useStaticText"] = True
    styles = (sw.get("styles") or {}).get("main", {}) or (sw.get("styles") or {}).get("Style_main", {})
    d = styles.get("DEFAULT", {})
    props = {}
    if "Text_Color" in d:
        props["text_color"] = pal.token(d["Text_Color"], COLOR_NAME_HINTS.get(d["Text_Color"].lower(), "Color"))
    if "Text_Font" in d:
        props["text_font"] = FONT_MAP.get(d["Text_Font"], d["Text_Font"])
    if "Text_Align" in d:
        props["text_align"] = d["Text_Align"]
    if props:
        set_local_style(w, "MAIN", "DEFAULT", props)
    apply_align_offset(w, sw.get("align"), sw.get("position"))
    return w

def build_container(sw, pal, identifier, children):
    size = sw.get("size") or [100, 100]
    w = base_widget("LVGLContainerWidget", 0, 0, size[0], size[1])
    w["identifier"] = identifier
    w["children"] = children
    styles = (sw.get("styles") or {}).get("Style_main", {})
    d = styles.get("DEFAULT", {})
    props = {}
    if "Bg_Color" in d:
        props["bg_color"] = pal.token(d["Bg_Color"], COLOR_NAME_HINTS.get(d["Bg_Color"].lower(), "Color"))
    if "Bg_Radius" in d:
        props["radius"] = d["Bg_Radius"]
    if "Text_Align" in d:
        props["text_align"] = d["Text_Align"]
    if props:
        set_local_style(w, "MAIN", "DEFAULT", props)
    apply_align_offset(w, sw.get("align"), sw.get("position"))
    return w

def build_image(sw, pal, identifier, bitmap_name):
    size = sw.get("size") or [64, 64]
    w = base_widget("LVGLImageWidget", 0, 0, size[0], size[1], width_unit="content", height_unit="content")
    w["identifier"] = identifier
    w["image"] = bitmap_name
    w["setPivot"] = False
    w["pivotX"] = 0
    w["pivotY"] = 0
    w["zoom"] = 256
    w["angle"] = 0
    w["innerAlign"] = "CENTER"
    w["sizeMode"] = "VIRTUAL"
    w["value"] = 0
    w["valueType"] = "literal"
    w["previewValue"] = 0
    styles = (sw.get("styles") or {}).get("Style_main", {})
    d = styles.get("DEFAULT", {})
    if "Image_reColor" in d:
        token = pal.token(d["Image_reColor"], COLOR_NAME_HINTS.get(d["Image_reColor"].lower(), "Recolor"))
        set_local_style(w, "MAIN", "DEFAULT", {"img_recolor": token, "img_recolor_opa": 255})
    apply_align_offset(w, sw.get("align"), sw.get("position"))
    return w

def build_bar(sw, pal, identifier):
    size = sw.get("size") or [100, 10]
    w = base_widget("LVGLBarWidget", 0, 0, size[0], size[1])
    w["identifier"] = identifier
    rng = sw.get("range", [0, 100])
    w["min"] = rng[0]; w["minType"] = "literal"
    w["max"] = rng[1]; w["maxType"] = "literal"
    w["mode"] = sw.get("mode", "NORMAL")
    val = sw.get("value") if sw.get("value") is not None else rng[0]
    w["value"] = val; w["valueType"] = "literal"; w["previewValue"] = str(val)
    w["valueStart"] = rng[0]; w["valueStartType"] = "literal"; w["previewValueStart"] = str(rng[0])
    w["enableAnimation"] = False
    styles = sw.get("styles") or {}
    main_d = styles.get("Style_main", {}).get("DEFAULT", {})
    ind_d = styles.get("Style_indicator", {}).get("DEFAULT", {})
    main_props = {}
    if "Bg_Color" in main_d:
        main_props["bg_color"] = pal.token(main_d["Bg_Color"], COLOR_NAME_HINTS.get(main_d["Bg_Color"].lower(), "Color"))
    if "Bg_gradiens_Color" in main_d:
        main_props["bg_grad_color"] = pal.token(main_d["Bg_gradiens_Color"], COLOR_NAME_HINTS.get(main_d["Bg_gradiens_Color"].lower(), "GradColor"))
        gdir = main_d.get("Gradient direction", "VER")
        main_props["bg_grad_dir"] = gdir
        gp = main_d.get("Bg_gradient_params", [0, 255])
        main_props["bg_main_stop"] = gp[0]
        main_props["bg_grad_stop"] = gp[1]
    if main_props:
        set_local_style(w, "MAIN", "DEFAULT", main_props)
    ind_props = {}
    if "Bg_Color" in ind_d:
        ind_props["bg_color"] = pal.token(ind_d["Bg_Color"], COLOR_NAME_HINTS.get(ind_d["Bg_Color"].lower(), "Color"))
    if "Bg_gradiens_Color" in ind_d:
        ind_props["bg_grad_color"] = pal.token(ind_d["Bg_gradiens_Color"], COLOR_NAME_HINTS.get(ind_d["Bg_gradiens_Color"].lower(), "GradColor"))
        ind_props["bg_grad_dir"] = ind_d.get("Gradient direction", "VER")
        gp = ind_d.get("Bg_gradient_params", [0, 255])
        ind_props["bg_main_stop"] = gp[0]
        ind_props["bg_grad_stop"] = gp[1]
    if "Bg_Radius" in ind_d:
        ind_props["radius"] = ind_d["Bg_Radius"]
    if ind_props:
        set_local_style(w, "INDICATOR", "DEFAULT", ind_props)
    apply_align_offset(w, sw.get("align"), sw.get("position"))
    return w

def build_arc(sw, pal, identifier):
    size = sw.get("size") or [100, 100]
    w = base_widget("LVGLArcWidget", 0, 0, size[0], size[1])
    w["identifier"] = identifier
    rng = sw.get("range", [0, 100])
    w["useAngle"] = False
    w["rangeMin"] = rng[0]; w["rangeMinType"] = "literal"
    w["rangeMax"] = rng[1]; w["rangeMaxType"] = "literal"
    val = sw.get("value") if sw.get("value") is not None else rng[0]
    w["value"] = val; w["valueType"] = "literal"; w["previewValue"] = str(val)
    w["mode"] = sw.get("mode", "NORMAL")
    ba = sw.get("bg_angles", [135, 45])
    for k, v in (("startAngle", ba[0]), ("endAngle", ba[1]),
                 ("bgStartAngle", ba[0]), ("bgEndAngle", ba[1])):
        w[k] = v; w[k + "Type"] = "literal"; w["preview" + k[0].upper() + k[1:]] = str(v)
    w["rotation"] = 0; w["rotationType"] = "literal"; w["previewRotation"] = "0"
    styles = sw.get("styles") or {}
    main_d = styles.get("Style_main", {}).get("DEFAULT", {})
    ind_d = styles.get("Style_indicator", {}).get("DEFAULT", {})
    knob_d = styles.get("Style_knob", {}).get("DEFAULT", {})
    main_props = {}
    if "Arc_Color" in main_d:
        main_props["arc_color"] = pal.token(main_d["Arc_Color"], COLOR_NAME_HINTS.get(main_d["Arc_Color"].lower(), "ArcColor"))
    if "Arc_Width" in main_d:
        main_props["arc_width"] = main_d["Arc_Width"]
    if main_props:
        set_local_style(w, "MAIN", "DEFAULT", main_props)
    ind_props = {}
    if "Arc_Color" in ind_d:
        ind_props["arc_color"] = pal.token(ind_d["Arc_Color"], COLOR_NAME_HINTS.get(ind_d["Arc_Color"].lower(), "ArcColor"))
    if "Arc_Width" in ind_d:
        ind_props["arc_width"] = ind_d["Arc_Width"]
    if ind_props:
        set_local_style(w, "INDICATOR", "DEFAULT", ind_props)
    knob_props = {}
    if "Bg_Color" in knob_d:
        knob_props["bg_color"] = pal.token(knob_d["Bg_Color"], COLOR_NAME_HINTS.get(knob_d["Bg_Color"].lower(), "KnobColor"))
    if "Border side" in knob_d:
        knob_props["border_side"] = knob_d["Border side"]
    if knob_props:
        set_local_style(w, "KNOB", "DEFAULT", knob_props)
    apply_align_offset(w, sw.get("align"), sw.get("position"))
    return w

# ---------- main ----------

def main():
    spj = json.load(open(SPJ))
    flat = walk_spj(spj["root"])
    by_name = {w["name"]: w for w in flat if w.get("name")}
    children_of = {}
    for w in flat:
        if w.get("name"):
            children_of.setdefault(w.get("parent"), []).append(w["name"])

    proj = json.load(open(PROJ))
    if any(p["name"] == PAGE_NAME for p in proj["userPages"]):
        print(f"Page '{PAGE_NAME}' already exists - aborting, nothing written.")
        return

    flex_widgets = [w for w in flat if w.get("layout_type")]
    if flex_widgets:
        print("WARNING: SquareLine flex-layout container(s) found - their children's")
        print("literal Position values are NOT meaningful (LVGL computes them at")
        print("layout time) and will render wrong if copied as literal left/top:")
        for w in flex_widgets:
            print(f"  - {w['name']}: flow={w.get('layout_flow')} "
                  f"main_align={w.get('layout_main_align')} cross_align={w.get('layout_cross_align')} "
                  f"children={children_of.get(w['name'])}")
        print("These widgets' children need manual placement/flex setup in EEZ Studio")
        print("after this script runs - see PROJECT-NOTES.md.")
        print()

    ts = time.strftime("%Y%m%d-%H%M%S")
    backup = PROJ + f".bak.{ts}"
    shutil.copyfile(PROJ, backup)
    print("Backup:", backup)

    pal = Palette(proj["colors"], proj["themes"])

    # bitmaps: reuse Nav_noNav, add SettingsIcon
    bitmap_names = {b["name"] for b in proj["bitmaps"]}
    if "SettingsIcon" not in bitmap_names:
        with open(f"{ASSETS}/Ic_settings_48px.svg.png", "rb") as f:
            b64 = base64.b64encode(f.read()).decode()
        proj["bitmaps"].append({
            "objID": oid(), "name": "SettingsIcon",
            "image": f"data:image/png;base64,{b64}"
        })
        print("Added bitmap: SettingsIcon (from assets/Ic_settings_48px.svg.png)")

    def build_widget(name):
        sw = by_name[name]
        ident = snake(name)
        kind = sw["sl_type"]
        kids = [build_widget(cn) for cn in children_of.get(name, [])]
        if kind == "PANEL":
            w = build_container(sw, pal, ident, kids)
        elif kind == "CONTAINER":
            w = build_container(sw, pal, ident, kids)
        elif kind == "LABEL":
            w = build_label(sw, pal, ident)
        elif kind == "BAR":
            w = build_bar(sw, pal, ident)
        elif kind == "ARC":
            w = build_arc(sw, pal, ident)
        elif kind == "IMAGE":
            asset = sw.get("image_asset", "")
            bmp = "Nav_noNav" if asset.endswith("no-nav.png") else "SettingsIcon"
            w = build_image(sw, pal, ident, bmp)
        else:
            raise ValueError(f"unhandled widget kind {kind} for {name}")
        if sw.get("hidden"):
            w["hiddenFlagType"] = "literal"
            w.setdefault("widgetFlags", "")
        return w

    screen_sw = by_name["SMainNoFL"]
    top_children_names = children_of.get("SMainNoFL", [])
    top_children = [build_widget(n) for n in top_children_names]

    screen_root = {
        "objID": oid(),
        "type": "LVGLScreenWidget",
        "left": 0, "top": 0, "width": 480, "height": 480,
        "customInputs": [], "customOutputs": [],
        "style": {"objID": oid(), "useStyle": "default", "conditionalStyles": [], "childStyles": []},
        "timeline": [], "eventHandlers": [],
        "leftUnit": "px", "topUnit": "px", "widthUnit": "px", "heightUnit": "px",
        "children": top_children,
        "widgetFlags": "CLICKABLE|PRESS_LOCK|CLICK_FOCUSABLE|GESTURE_BUBBLE|SNAPPABLE|SCROLLABLE|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER",
        "hiddenFlagType": "literal",
        "clickableFlag": True,
        "clickableFlagType": "literal",
        "checkedStateType": "literal", "disabledStateType": "literal",
        "states": "",
        "localStyles": {"objID": oid()},
        "groupIndex": 0,
    }
    screen_styles = (screen_sw.get("styles") or {}).get("Style_main", {}).get("DEFAULT", {})
    if "Bg_Color" in screen_styles:
        token = pal.token(screen_styles["Bg_Color"], COLOR_NAME_HINTS.get(screen_styles["Bg_Color"].lower(), "ScreenBg"))
        set_local_style(screen_root, "MAIN", "DEFAULT", {"bg_color": token})

    page = {
        "objID": oid(),
        "connectionLines": [], "localVariables": [], "componentGroups": [], "userProperties": [],
        "name": PAGE_NAME,
        "left": 0, "top": 0, "width": 480, "height": 480,
        "isUsedAsUserWidget": False,
        "createAtStart": True,
        "deleteOnScreenUnload": False,
        "components": [screen_root],
    }
    proj["userPages"].append(page)

    with open(PROJ, "w") as f:
        json.dump(proj, f, indent=2)
        f.write("\n")

    # ---- gates ----
    ok = True
    try:
        json.load(open(PROJ))
        print("G1 JSON parse: OK")
    except Exception as e:
        print("G1 JSON parse: FAILED", e)
        ok = False

    color_names = {c["name"] for c in proj["colors"]}
    font_names = {f["name"] for f in proj["fonts"]} | {
        f"MONTSERRAT_{n}" for n in (8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48)
    }
    bitmap_names2 = {b["name"] for b in proj["bitmaps"]}
    missing = []
    def check(node):
        if isinstance(node, dict):
            defn = node.get("localStyles", {}).get("definition", {})
            for part, states in defn.items():
                for state, props in states.items():
                    for k, v in props.items():
                        if k in ("bg_color", "text_color", "arc_color", "border_color",
                                  "img_recolor", "bg_grad_color") and isinstance(v, str):
                            if v not in color_names and not v.startswith("#"):
                                missing.append(("color", v))
                        if k == "text_font" and v not in font_names:
                            missing.append(("font", v))
            if node.get("type") == "LVGLImageWidget" and node.get("image") not in bitmap_names2:
                missing.append(("bitmap", node.get("image")))
            for v in node.values():
                check(v)
        elif isinstance(node, list):
            for x in node:
                check(x)
    check(page)
    print(f"G3 Named-reference check: {len(missing)} unresolved (expected: the 2 custom fonts, see below)")
    for kind, name in sorted(set(missing)):
        print(f"   - {kind}: {name!r}")

    print()
    print(f"Widgets added: {sum(1 for _ in flat)}")
    print("New color tokens:")
    existing_before = ts  # marker unused
    print(f"  total colors[] now: {len(proj['colors'])}")

    print()
    print("Custom fonts referenced but NOT embedded (add by hand in EEZ Studio, then reload):")
    for fname, spec in CUSTOM_FONTS_NEEDED.items():
        print(f"  - {fname}: {spec['ttf']}, size {spec['size']}px, bpp {spec['bpp']}, range {spec['range']}")

if __name__ == "__main__":
    main()
