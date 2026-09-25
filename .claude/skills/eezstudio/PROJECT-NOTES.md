# Anpassungshinweise für TRGB-BikeComputer

`SKILL.md` in diesem Ordner ist eine **unveränderte** Kopie von
[trailcurrentoss/TrailCurrentClaudeSkills](https://github.com/trailcurrentoss/TrailCurrentClaudeSkills/blob/main/eezstudio/SKILL.md)
(MIT-Lizenz, Copyright (c) 2026 TrailCurrent). Absichtlich nicht angepasst
-- die Trap-/Schema-Kenntnisse dort sind mit Zitaten aus dem
`eez-open/studio`-Quellcode belegt. Diese Datei sammelt die **Deltas zu
unserem Projekt**.

## Projekt-Layout

| Was | Wo |
|---|---|
| EEZ-Projekt | `EEZStudio/TRGB-BikeComputer.eez-project` |
| Export-Ziel (`settings.build.destinationFolder`) | `src/ui_eez/` -- wird bei jedem Export komplett ersetzt |
| Handgeschriebene Screen-Logik | `src/ui/RimRidgeCustFunc.*`, `src/ui/RimRidgeNavCustFunc.*` |
| Anbindung an die Firmware | `src/UIFacade.cpp` |
| Design-Vorgaben | `doc/design/rim-ridge-design-system.md` |
| Edit-Skripte (Einmal-Patches, nicht in git) | `EEZStudio/tmp/*.py` |

Screens: `rim_ridge` (Main), `rim_ridge_nav` (Navigation).
Einstellungen (`doc/design/settings.svg`) ist spezifiziert, aber noch nicht
angelegt.

## Build-System-Unterschied

Das Skill ist für ESP-IDF geschrieben (`main/ui/`, `sdkconfig.defaults`,
`idf.py build`). Wir bauen mit **PlatformIO + Arduino-Framework**:

- `main/ui/` → `src/ui_eez/`.
- `idf.py build` → `pio run -e trgb-esp32-s3`.
- Der `sdkconfig`-Abschnitt (Trap 17) entfällt.
- `#if __has_include(...)`-Gating funktioniert identisch, nur Pfade
  anpassen.
- EEZ-Code inkludiert `<lvgl/lvgl.h>` -- dafür gibt es den Shim
  `include/lvgl/lvgl.h`.
- Fonts, die es auch im SquareLine-Altbestand unter `src/ui/font/` gibt
  (aktuell `ui_font_by7x128`), per `build_src_filter` in `platformio.ini`
  aus `src/ui_eez/` ausschließen.

LVGL ist 8.4.0 (transitiv über `TRGBArduinoSupport`, `"lvgl": "^8.3.0"`)
und passt damit zum Schema-Ziel `lvglVersion 8.4.x` des Skills. Ein
Wechsel auf LVGL 9 ist eine eigene, größere Entscheidung.

## Fehlende Begleit-Dateien

Das Skill referenziert Verifier-Skripte, die im Quell-Repo nicht
mitgeliefert werden: `verify_project.py` (Gate 3/4),
`verify_no_canvas_divergence.py` (Gate 5), `render_pages.py`,
`SCHEMA_REFERENCE.md`. Existieren hier nicht -- die Spezifikation im
SKILL.md ("The gates the script must pass", "Required gates after the
script runs") reicht, um die Prüfungen im jeweiligen Edit-Skript selbst
durchzuführen. Die visuelle Prüfung im EEZ-Canvas durch den Nutzer bleibt
der letzte Schritt.

## Verwandtes: eez-studio-mcp

`eez-studio-mcp` (IWILLTBEST) greift live in eine laufende EEZ-Studio-
Instanz (Screenshots, Input-Injection), braucht aber einen gepatchten Fork.
Bewusst abwarten, bis das offiziell in EEZ Studio landet.
