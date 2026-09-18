#!/usr/bin/env python3
"""Targeted patch on top of the user's manual edits in EEZ Studio - NOT a
full regenerate (that would blow away their arc resize + nav nesting).

1. Nav pill: rr_icon_turn re-rasterized at 64x64 (was 32x32), rr_nav_pill
   container enlarged to fit, rr_nav_dist font bumped up.
2. rr_tour_pill: enlarged and repositioned to match where rr_distance_val
   and rr_tour_label ended up after the user's manual moves, and both are
   re-parented into it as children (single tappable "distance + mode"
   control, mirroring PanelClock's time+mode bundling on MainNoFL).
"""
import json, shutil, time, uuid, base64, subprocess

REPO = "/home/ian/Coding/Bike/TRGB-BikeComputer"
PROJ = f"{REPO}/EEZStudio/TRGB-BikeComputer.eez-project"
ICON_DIR = "/tmp/claude-1000/-home-ian-Coding-Bike-TRGB-BikeComputer/2f8ac32e-c071-4372-a930-f31be764c333/scratchpad/rimridge/icons"

def oid():
    return str(uuid.uuid4())

TURN_ICON_SVG = '''<g fill="none" stroke="#CBA36B" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round">
    <path d="M0,12 L0,-4 L-9,-4"/><path d="M-9,-4 L-4,-8 M-9,-4 L-4,0"/></g>'''
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
    """Return the widget whose 'children' list directly contains ident."""
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

    # --- 1. Nav pill: bigger icon, bigger container, bigger label font ---
    svg_path = f"{ICON_DIR}/rr_icon_turn_64.svg"
    png_path = f"{ICON_DIR}/rr_icon_turn_64.png"
    with open(svg_path, "w") as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{ICON_VB}">{TURN_ICON_SVG}</svg>')
    subprocess.run(["rsvg-convert", "-w", "64", "-h", "64", svg_path, "-o", png_path], check=True)
    b64 = base64.b64encode(open(png_path, "rb").read()).decode()
    bmp = next(b for b in proj["bitmaps"] if b["name"] == "rr_icon_turn")
    bmp["image"] = f"data:image/png;base64,{b64}"
    print("Re-rasterized rr_icon_turn at 64x64")

    nav_pill = find(root, "rr_nav_pill")
    nav_pill["width"] = 200
    nav_pill["height"] = 78
    nav_pill["left"] = 140   # keep horizontally centered on screen (240 - 200/2)
    nav_pill["top"] = 85

    ic_turn = find(root, "rr_ic_turn")
    ic_turn["width"] = 64
    ic_turn["height"] = 64
    ic_turn["left"] = 15
    ic_turn["top"] = 7

    nav_dist = find(root, "rr_nav_dist")
    nav_dist["left"] = 94
    nav_dist["top"] = 24
    nav_dist["localStyles"]["definition"]["MAIN"]["DEFAULT"]["text_font"] = "MONTSERRAT_28"
    print("Nav pill resized to 200x78, icon 64x64, label -> MONTSERRAT_28")

    # --- 2. Tour pill: enlarge, reposition to match the user's manual
    #        distance/tour moves, re-parent distance_val + tour_label in ---
    tour_pill = find(root, "rr_tour_pill")
    dist_val = find(root, "rr_distance_val")
    tour_label = find(root, "rr_tour_label")

    dist_parent = find_container_of(root, "rr_distance_val") or root
    tour_label_parent = find_container_of(root, "rr_tour_label") or root
    dist_parent["children"] = [c for c in dist_parent["children"] if c.get("identifier") != "rr_distance_val"]
    tour_label_parent["children"] = [c for c in tour_label_parent["children"] if c.get("identifier") != "rr_tour_label"]

    tour_pill["left"] = 173
    tour_pill["top"] = 383
    tour_pill["width"] = 134
    tour_pill["height"] = 92

    # convert from screen-center-relative (align:CENTER) to plain
    # container-relative TOP_LEFT, matching rr_nav_pill's children pattern
    dist_val["left"] = 3
    dist_val["top"] = 8
    dist_val["localStyles"]["definition"]["MAIN"]["DEFAULT"].pop("align", None)

    tour_label["left"] = 25
    tour_label["top"] = 49
    tour_label["localStyles"]["definition"]["MAIN"]["DEFAULT"].pop("align", None)

    tour_pill["children"] = [dist_val, tour_label]
    print("Tour pill enlarged to 134x92 @ (173,383), distance_val + tour_label nested inside")

    with open(PROJ, "w") as f:
        json.dump(proj, f, indent=2)
        f.write("\n")

    json.load(open(PROJ))
    print("G1 JSON parse: OK")

if __name__ == "__main__":
    main()
