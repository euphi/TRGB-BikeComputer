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

Screens: `rim_ridge` (Main), `rim_ridge_nav` (Navigation), `rim_ridge_rq`
(RQ-Ride), `rim_ridge_settings` (Einstellungen). Handgeschriebene Logik je Screen
in `src/ui/RimRidge*CustFunc.*`. Die Fahrzustand/RQ-Gruppe unten
(`*_group_rq_mode`) ist auf allen vier Screens identisch -- Änderungen dort
überall gleich machen (`EEZStudio/tmp/add_settings_screen.py` hat sie geklont).

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

## Headless-Export und Gerätetest (für Agenten)

**Standard-Ablauf seit 2026-09-27 (Nutzerwunsch):** nach jeder JSON-Änderung
selbst headless exportieren, nicht auf ein manuelles Ctrl+B warten.

EEZ Studio 0.29 exportiert ohne GUI-Klick:
`EEZ-Studio-0.29.0.AppImage --build-project <pfad>.eez-project`.
`Tools/eez_export_headless.sh` kapselt das. Stolpersteine:

- In der Claude-Code-Shell ist `ELECTRON_RUN_AS_NODE=1` gesetzt → "bad option:
  --build-project". Mit `env -u ELECTRON_RUN_AS_NODE` starten.
- Der Headless-Build erzeugt **keine** `ui_font_*.c` und schreibt eine
  `.eez-project-build` ohne sie. Deshalb exportiert das Skript in eine Kopie und
  übernimmt nur geänderte Code-/Bilddateien. Alle erzeugten Dateien waren
  byte-gleich zum Ctrl+B-Export des Nutzers (geprüft 2026-09-27). Font-Änderungen
  brauchen weiterhin Ctrl+B in EEZ Studio.
- Der Nutzer hat EEZ Studio oft mit dem Projekt offen. Nach einer JSON-Änderung
  muss er dort *File → Reload Project* machen, bevor er speichert, sonst
  überschreibt die offene Instanz die Änderung. `--reload-project` (zweite
  Instanz) würde das auslösen, verwirft aber ungespeicherte Änderungen des
  Nutzers -- nicht ungefragt benutzen.
- `json.dump(..., ensure_ascii=False)` und ohne abschließenden Zeilenumbruch
  schreiben, sonst werden alle Umlaute zu `\u00e4` und der Diff unlesbar.

Danach auf dem Gerät prüfen: `Tools/uishot.py` (Screenshot, Tap, Long-Press,
Wischen über `/debug/ui/*`, siehe `doc/DEBUG.md`). Das ersetzt nicht die
Sichtprüfung des Nutzers im Canvas, deckt aber Canvas-Geräte-Abweichungen und
tote Buttons auf.

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
