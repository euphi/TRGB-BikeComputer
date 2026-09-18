#!/usr/bin/env python3
"""Convert the single-color RimRidge icons from baked-color 32bpp bitmaps to
alpha-only bitmaps + LVGL img_recolor (smaller, recolorable at runtime).

Battery icon is split: outline+nub (fixed brass, alpha bitmap) stays an
image; the charge-level fill becomes a separate small rect widget with its
own color (currently a static placeholder - "dynamisch" per the request
means it's meant to be wired to real battery % later, not implemented yet).

Deliberately NOT touched: 'SettingsIcon' and 'Nav_noNav' - both are SHARED
bitmaps also used by the already-converted MainNoFL page (ImgWifi/
ImgSettings/ImgState). Changing their bpp would require also adding
img_recolor on those MainNoFL widgets to preserve their current look, which
is out of scope for this RimRidge-only pass.
"""
import json, shutil, time, uuid, base64, subprocess

REPO = "/home/ian/Coding/Bike/TRGB-BikeComputer"
PROJ = f"{REPO}/EEZStudio/TRGB-BikeComputer.eez-project"
ICON_DIR = "/tmp/claude-1000/-home-ian-Coding-Bike-TRGB-BikeComputer/2f8ac32e-c071-4372-a930-f31be764c333/scratchpad/rimridge/icons"

CF_ALPHA_8_BIT = 4  # keeps anti-aliased edges at this small size, unlike CF_ALPHA_1_BIT (binary mask, jagged at 32px)

def oid():
    return str(uuid.uuid4())

# icon name -> recolor token to apply on its widget's img_recolor
SINGLE_COLOR_ICONS = {
    "rr_icon_wifi": "RRBrass",
    "rr_icon_gps": "RRBrass",
    "rr_icon_turn": "RRBrass",
    "rr_icon_cadence": "RRBrass",
    "rr_icon_power": "RRBrass",
    "rr_icon_temp": "RRBrass",
    "rr_icon_height": "RRSage",
    "rr_icon_gradient": "RRBrass",
    "rr_icon_heart": "RRBrass",
    "rr_icon_pause": "RRBrass",
}

BATTERY_OUTLINE_SVG = '''<rect x="-9" y="-5.5" width="16" height="11" rx="1.5" fill="none" stroke="#000" stroke-width="1.8"/>
<rect x="7.3" y="-2.5" width="2.2" height="5" rx="0.6" fill="#000"/>'''
ICON_VB = "-16 -16 32 32"

def find(node, ident):
    if isinstance(node, dict):
        if node.get("identifier") == ident:
            return node
        for v in node.values():
            r = find(v, ident)
            if r is not None:
                return r
    elif isinstance(node, list):
        for x in node:
            r = find(x, ident)
            if r is not None:
                return r
    return None

def find_container_of(node, ident):
    if isinstance(node, dict):
        for c in node.get("children", []):
            if c.get("identifier") == ident:
                return node
        for v in node.values():
            r = find_container_of(v, ident)
            if r is not None:
                return r
    elif isinstance(node, list):
        for x in node:
            r = find_container_of(x, ident)
            if r is not None:
                return r
    return None

def main():
    proj = json.load(open(PROJ))
    page = next(p for p in proj["userPages"] if p["name"] == "RimRidge")
    root = page["components"][0]

    ts = time.strftime("%Y%m%d-%H%M%S")
    backup = PROJ + f".bak.{ts}"
    shutil.copyfile(PROJ, backup)
    print("Backup:", backup)

    bitmaps_by_name = {b["name"]: b for b in proj["bitmaps"]}

    # --- single-color icons: bpp -> alpha-only, add img_recolor on widget ---
    for icon_name, color_tok in SINGLE_COLOR_ICONS.items():
        bitmaps_by_name[icon_name]["bpp"] = CF_ALPHA_8_BIT
        # widget identifier == bitmap name minus the leading 'rr_icon_' ->
        # actual widget identifiers are rr_ic_<thing>, not rr_icon_<thing>
        widget_ident = "rr_ic_" + icon_name[len("rr_icon_"):]
        w = find(root, widget_ident)
        ls = w.setdefault("localStyles", {}).setdefault("definition", {}).setdefault("MAIN", {}).setdefault("DEFAULT", {})
        ls["img_recolor"] = color_tok
        ls["img_recolor_opa"] = 255
    print(f"Converted {len(SINGLE_COLOR_ICONS)} single-color icons to alpha-only (bpp={CF_ALPHA_8_BIT}) + img_recolor")

    # --- battery: split into outline (alpha bitmap, fixed brass) + fill rect (own color) ---
    svg_path = f"{ICON_DIR}/rr_icon_battery_outline.svg"
    png_path = f"{ICON_DIR}/rr_icon_battery_outline.png"
    with open(svg_path, "w") as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{ICON_VB}">{BATTERY_OUTLINE_SVG}</svg>')
    subprocess.run(["rsvg-convert", "-w", "32", "-h", "32", svg_path, "-o", png_path], check=True)
    b64 = base64.b64encode(open(png_path, "rb").read()).decode()
    bmp = bitmaps_by_name["rr_icon_battery"]
    bmp["image"] = f"data:image/png;base64,{b64}"
    bmp["bpp"] = CF_ALPHA_8_BIT

    ic_battery = find(root, "rr_ic_battery")
    ls = ic_battery.setdefault("localStyles", {}).setdefault("definition", {}).setdefault("MAIN", {}).setdefault("DEFAULT", {})
    ls["img_recolor"] = "RRBrass"
    ls["img_recolor_opa"] = 255

    # fill rect: local (-7,-3.5,8.8,7) inside a 32x32 icon centered at
    # (292,60) absolute -> abs top-left (285,56.5), size (9,7)
    fill_widget = {
        "objID": oid(), "type": "LVGLContainerWidget",
        "left": 285, "top": 57, "width": 9, "height": 7,
        "customInputs": [], "customOutputs": [],
        "style": {"objID": oid(), "useStyle": "default", "conditionalStyles": [], "childStyles": []},
        "timeline": [], "eventHandlers": [],
        "leftUnit": "px", "topUnit": "px", "widthUnit": "px", "heightUnit": "px",
        "children": [],
        "widgetFlags": "CLICK_FOCUSABLE|GESTURE_BUBBLE|PRESS_LOCK|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_WITH_ARROW|SNAPPABLE",
        "hiddenFlagType": "literal", "clickableFlag": True, "clickableFlagType": "literal",
        "flagScrollbarMode": "", "flagScrollDirection": "", "scrollSnapX": "", "scrollSnapY": "",
        "checkedStateType": "literal", "disabledStateType": "literal", "states": "",
        "identifier": "rr_battery_fill",
        "localStyles": {"objID": oid(), "definition": {"MAIN": {"DEFAULT": {
            "bg_color": "RRSage", "radius": 1  # placeholder color - meant to be driven by real battery % later
        }}}},
        "group": "", "groupIndex": 0,
    }
    root["children"].append(fill_widget)
    print("Battery icon split: rr_ic_battery (outline, alpha+recolor) + new rr_battery_fill (static placeholder color)")

    with open(PROJ, "w") as f:
        json.dump(proj, f, indent=2)
        f.write("\n")

    json.load(open(PROJ))
    print("G1 JSON parse: OK")

if __name__ == "__main__":
    main()
