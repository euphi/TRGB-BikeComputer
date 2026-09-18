#!/usr/bin/env python3
"""Add event handlers (actions) to the MainNoFL page widgets, mirroring the
REAL current dispatch logic found in src/ui/Screens/MainNoFL/ui.c (ground
truth - NOT the stale event names recorded in the SquareLine .spj, which
don't exist anywhere in the actual generated/hand-written code).

flowSupport is false, so each action is just a named C function stub
(action_<name>(lv_event_t *e)) that must be hand-implemented later, once
the firmware actually builds against EEZ-generated code. For now this only
adds the JSON wiring (actions[] + eventHandlers on widgets) so it's visible
and editable in EEZ Studio's Actions tab instead of buried in generated C.
"""
import json, shutil, time, uuid

PROJ = "/home/ian/Coding/Bike/TRGB-BikeComputer/EEZStudio/TRGB-BikeComputer.eez-project"
PAGE_NAME = "MainNoFL"

def oid():
    return str(uuid.uuid4())

# (widget identifier, eventName, action name, one-line note on the body
#  this action needs once implemented - mirrors ui.c 1:1)
BINDINGS = [
    ("panel_nav", "CLICKED", "GoToNavi",
     "_ui_screen_change(ui_SNavi, LV_SCR_LOAD_ANIM_MOVE_TOP, 500, 0);"),
    ("bar_hr", "LONG_PRESSED", "GoToChart",
     "_ui_screen_change(ui_SChart, LV_SCR_LOAD_ANIM_OVER_TOP, 500, 0);"),
    ("bar_batt", "LONG_PRESSED", "GoToFLScreen",
     "_ui_screen_change(ui_ScreenFL, LV_SCR_LOAD_ANIM_OVER_LEFT, 500, 0);"),
    ("img_wifi", "CLICKED", "GoToWlan",
     "_ui_screen_change(ui_SWLAN, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0);"),
    ("img_settings", "CLICKED", "GoToSettings",
     "_ui_screen_change(ui_ScrSettings, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);"),
    ("cont_height", "LONG_PRESSED", "ReloadMainScreen",
     "_ui_screen_change(ui_SMainNoFL, LV_SCR_LOAD_ANIM_MOVE_TOP, 500, 0); "
     "// NB: reloads itself - looks vestigial, verify intent before wiring for real"),
    ("img_state", "CLICKED", "DriveStateShortPress",
     "driveStateUpdate(DSE_delayStandby);"),
    ("img_state", "LONG_PRESSED", "DriveStateLongPress",
     "driveStateUpdate(DSE_toggleStandbyMode);"),
    # PanelClock: LVGL only fires generic GESTURE (no per-direction eventName
    # in EEZ's schema) - the 4-way direction check stays hand-written in the
    # action body, same as today's ui_event_PanelClock.
    ("panel_clock", "GESTURE", "ClockGesture",
     "if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_TOP) statsTimeMode(true);\n"
     "  else if (dir == LV_DIR_BOTTOM) statsTimeMode(false);\n"
     "  else if (dir == LV_DIR_RIGHT) statModeNext(true);\n"
     "  else if (dir == LV_DIR_LEFT) statModeNext(false);"),
    ("panel_clock", "LONG_PRESSED", "ResetStats",
     "resetStats();"),
]

def find_widget(node, ident):
    if isinstance(node, dict):
        if node.get("identifier") == ident:
            return node
        for v in node.values():
            r = find_widget(v, ident)
            if r is not None:
                return r
    elif isinstance(node, list):
        for x in node:
            r = find_widget(x, ident)
            if r is not None:
                return r
    return None

def main():
    proj = json.load(open(PROJ))
    page = next(p for p in proj["userPages"] if p["name"] == PAGE_NAME)

    ts = time.strftime("%Y%m%d-%H%M%S")
    backup = PROJ + f".bak.{ts}"
    shutil.copyfile(PROJ, backup)
    print("Backup:", backup)

    action_names = {a["name"] for a in proj["actions"]}
    added_actions = []
    added_handlers = []
    notes = []

    for ident, event_name, action_name, body_note in BINDINGS:
        w = find_widget(page["components"], ident)
        if w is None:
            print(f"SKIP: widget '{ident}' not found on page '{PAGE_NAME}'")
            continue
        if action_name not in action_names:
            proj["actions"].append({
                "objID": oid(),
                "components": [], "connectionLines": [],
                "localVariables": [], "userProperties": [],
                "name": action_name,
            })
            action_names.add(action_name)
            added_actions.append(action_name)
        already = any(eh.get("eventName") == event_name and eh.get("action") == action_name
                      for eh in w.get("eventHandlers", []))
        if not already:
            w.setdefault("eventHandlers", []).append({
                "objID": oid(),
                "eventName": event_name,
                "handlerType": "action",
                "action": action_name,
                "userData": 0,
            })
            added_handlers.append((ident, event_name, action_name))
        notes.append((action_name, body_note))

    with open(PROJ, "w") as f:
        json.dump(proj, f, indent=2)
        f.write("\n")

    json.load(open(PROJ))
    print("G1 JSON parse: OK")

    action_names_final = {a["name"] for a in proj["actions"]}
    dangling = []
    def check(node):
        if isinstance(node, dict):
            for eh in node.get("eventHandlers", []):
                if eh.get("handlerType") == "action" and eh.get("action") not in action_names_final:
                    dangling.append((node.get("identifier"), eh.get("action")))
            for v in node.values():
                check(v)
        elif isinstance(node, list):
            for x in node:
                check(x)
    check(page)
    print(f"G3 Named-reference check (actions): {len(dangling)} dangling")
    for ident, aname in dangling:
        print(f"   - {ident}: {aname!r}")

    print()
    print(f"Actions added: {added_actions}")
    print(f"Event handlers added: {len(added_handlers)}")
    for ident, ev, act in added_handlers:
        print(f"  - {ident} [{ev}] -> {act}")

    print()
    print("Action bodies to hand-implement once the firmware builds against")
    print("EEZ-generated ui/actions.h (not yet - EEZStudio/ isn't wired into")
    print("platformio.ini's build):")
    for aname, note in notes:
        print(f"  action_{aname}: {note}")

if __name__ == "__main__":
    main()
