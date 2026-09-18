#!/usr/bin/env python3
"""Add a 'BootLogo' page to the EEZ project: the "Rim & Ridge" boot-logo
direction from the boot-logo design study
(claude.ai/artifact/RutufhR3sjX9zit2ppEiAY), rasterized as a single
full-screen bitmap. The design study's own closing note explicitly argues
vector-vs-bitmap doesn't matter here since LVGL only ever shows a
converted C bitmap for a static image like this - so one flat image is a
faithful choice, not a shortcut.
"""
import json, shutil, time, uuid, base64, subprocess

REPO = "/home/ian/Coding/Bike/TRGB-BikeComputer"
PROJ = f"{REPO}/EEZStudio/TRGB-BikeComputer.eez-project"
ICON_DIR = "/tmp/claude-1000/-home-ian-Coding-Bike-TRGB-BikeComputer/2f8ac32e-c071-4372-a930-f31be764c333/scratchpad"
PAGE_NAME = "BootLogo"
BITMAP_NAME = "BootLogoRimRidge"

CF_TRUE_COLOR_ALPHA = 32

BOOTLOGO_SVG = '''<svg xmlns="http://www.w3.org/2000/svg" width="480" height="480" viewBox="0 0 480 480">
  <circle cx="240" cy="240" r="240" fill="#161B1F"/>
  <polygon points="60,290 140,250 195,200 240,175 290,205 345,245 420,285 420,480 60,480" fill="#1F2C27"/>
  <polyline points="60,290 140,250 195,200 240,175 290,205 345,245 420,285" fill="none" stroke="#7FA08F" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
  <circle cx="240" cy="240" r="196" fill="none" stroke="#CBA36B" stroke-width="3"/>
  <circle cx="240" cy="175" r="8" fill="#CBA36B"/>
  <g stroke="#CBA36B" stroke-width="2.5" opacity="0.65" stroke-linecap="round">
    <line x1="240" y1="44" x2="240" y2="58"/>
    <line x1="436" y1="240" x2="422" y2="240"/>
    <line x1="240" y1="436" x2="240" y2="422"/>
    <line x1="44" y1="240" x2="58" y2="240"/>
  </g>
</svg>'''

def oid():
    return str(uuid.uuid4())

def main():
    proj = json.load(open(PROJ))
    if any(p["name"] == PAGE_NAME for p in proj["userPages"]):
        print(f"Page '{PAGE_NAME}' already exists - aborting.")
        return

    ts = time.strftime("%Y%m%d-%H%M%S")
    backup = PROJ + f".bak.{ts}"
    shutil.copyfile(PROJ, backup)
    print("Backup:", backup)

    svg_path = f"{ICON_DIR}/bootlogo_rimridge.svg"
    png_path = f"{ICON_DIR}/bootlogo_rimridge_480.png"
    with open(svg_path, "w") as f:
        f.write(BOOTLOGO_SVG)
    subprocess.run(["rsvg-convert", "-w", "480", "-h", "480", svg_path, "-o", png_path], check=True)
    b64 = base64.b64encode(open(png_path, "rb").read()).decode()

    if not any(b["name"] == BITMAP_NAME for b in proj["bitmaps"]):
        proj["bitmaps"].append({
            "objID": oid(), "name": BITMAP_NAME,
            "image": f"data:image/png;base64,{b64}",
            "bpp": CF_TRUE_COLOR_ALPHA,
            "lvglBinaryOutputFormat": 3,
            "lvglDither": False,
        })
    print(f"Bitmap '{BITMAP_NAME}' added (480x480, bpp={CF_TRUE_COLOR_ALPHA})")

    img_widget = {
        "objID": oid(), "type": "LVGLImageWidget",
        "left": 0, "top": 0, "width": 480, "height": 480,
        "customInputs": [], "customOutputs": [],
        "style": {"objID": oid(), "useStyle": "default", "conditionalStyles": [], "childStyles": []},
        "timeline": [], "eventHandlers": [],
        "leftUnit": "px", "topUnit": "px", "widthUnit": "px", "heightUnit": "px",
        "children": [],
        "widgetFlags": "CLICK_FOCUSABLE|GESTURE_BUBBLE|PRESS_LOCK|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_WITH_ARROW|SNAPPABLE",
        "hiddenFlagType": "literal", "clickableFlag": True, "clickableFlagType": "literal",
        "flagScrollbarMode": "", "flagScrollDirection": "", "scrollSnapX": "", "scrollSnapY": "",
        "checkedStateType": "literal", "disabledStateType": "literal", "states": "",
        "identifier": "img_bootlogo",
        "localStyles": {"objID": oid()},
        "group": "", "groupIndex": 0,
        "image": BITMAP_NAME,
        "setPivot": False, "pivotX": 0, "pivotY": 0,
        "zoom": 256, "angle": 0, "innerAlign": "CENTER", "sizeMode": "VIRTUAL",
        "value": 0, "valueType": "literal", "previewValue": 0,
    }

    screen_root = {
        "objID": oid(), "type": "LVGLScreenWidget",
        "left": 0, "top": 0, "width": 480, "height": 480,
        "customInputs": [], "customOutputs": [],
        "style": {"objID": oid(), "useStyle": "default", "conditionalStyles": [], "childStyles": []},
        "timeline": [], "eventHandlers": [],
        "leftUnit": "px", "topUnit": "px", "widthUnit": "px", "heightUnit": "px",
        "children": [img_widget],
        "widgetFlags": "CLICKABLE|PRESS_LOCK|CLICK_FOCUSABLE|GESTURE_BUBBLE|SNAPPABLE|SCROLLABLE|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER",
        "hiddenFlagType": "literal", "clickableFlag": True, "clickableFlagType": "literal",
        "checkedStateType": "literal", "disabledStateType": "literal", "states": "",
        "localStyles": {"objID": oid()},
        "groupIndex": 0,
    }

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
    print(f"Page '{PAGE_NAME}' added with single full-screen image widget 'img_bootlogo'")

if __name__ == "__main__":
    main()
