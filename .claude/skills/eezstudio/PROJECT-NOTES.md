# Anpassungshinweise für TRGB-BikeComputer

`SKILL.md` in diesem Ordner ist eine **unveränderte** Kopie von
[trailcurrentoss/TrailCurrentClaudeSkills](https://github.com/trailcurrentoss/TrailCurrentClaudeSkills/blob/main/eezstudio/SKILL.md)
(MIT-Lizenz, Copyright (c) 2026 TrailCurrent), Stand 2026-09-16. Absichtlich
nicht angepasst/umgeschrieben — die Trap-/Schema-Kenntnisse dort sind mit
Zitaten aus dem `eez-open/studio`-Quellcode belegt, und das soll so bleiben,
statt durch Vermutungen beim Eindeutschen/Anpassen verwässert zu werden.

Diese Datei hier sammelt stattdessen die **Deltas zu unserem Projekt**, noch
bevor wir die eigentliche EEZ-Studio-Migration begonnen haben (Stand: noch
kein `.eez-project` im Repo, wir sind noch auf SquareLine).

## Build-System-Unterschied (wichtigster Punkt)

Das Skill ist für ESP-IDF geschrieben (`main/ui/`, `sdkconfig.defaults`,
`idf.py build`, `#if __has_include("ui/screens.h")`-Gating vor jedem
`idf.py build`). Wir bauen mit **PlatformIO + Arduino-Framework**
(`platformio.ini`, `pio run -e trgb-esp32-s3`), UI-Code liegt unter
`src/ui/` statt `main/ui/`. Der einzige espidf-Environment-Eintrag
(`trgb-esp32-s3-idf`) baut laut Kommentar in `platformio.ini` aktuell nicht.

Beim tatsächlichen Umbau übertragen:
- `main/ui/*.c|*.h` → `src/ui/*.c|*.h` (bzw. das jeweilige Unterverzeichnis
  je Screen, analog zur bestehenden SquareLine-Struktur in
  `src/ui/Screens/<Name>/`).
- `sdkconfig`/`sdkconfig.defaults`-Abschnitt (Trap 17) entfällt komplett —
  hat mit Arduino-Framework keine Entsprechung.
- `idf.py build` → `pio run -e trgb-esp32-s3` (Default-Env laut CLAUDE.md).
- Das `#if __has_include(...)`-Gating-Pattern selbst ist reiner
  C-Präprozessor und funktioniert unter PlatformIO/Arduino identisch, nur
  Pfade anpassen.

Der Schema-/Trap-Teil des Skills (JSON-Form von Widgets, die
Canvas-Device-Divergence-Regel, Farbtoken-Pflicht, die 20+ Traps) ist
davon **nicht** betroffen — der hat nichts mit ESP-IDF vs. Arduino zu tun,
sondern rein mit EEZ Studios eigenem JSON-Schema und Canvas-Renderer.

## LVGL-Version

Skill zielt auf `lvglVersion 8.4.x` (Schema-Crib, Zeile ~488/547).
**Geprüft 2026-09-16: wir sind bereits auf LVGL 8.4.0** — nicht wie in
einer älteren Notiz vermutet 8.3.9. Tatsächliche Version steht nicht direkt
in `platformio.ini` (dort ist `lvgl` gar nicht in `lib_deps` gelistet),
sondern kommt transitiv über `TRGBArduinoSupport`, dessen `library.json`
`"lvgl": "^8.3.0"` deklariert; PlatformIO hat das zum Zeitpunkt der letzten
Dependency-Auflösung bereits zu 8.4.0 aufgelöst (siehe
`.pio/libdeps/trgb-esp32-s3/lvgl/library.json`). Laut GitHub-Tags
(`lvgl/lvgl`, geprüft 2026-09-16) ist 8.4.0 aktuell auch der neueste
8.x-Release, es gibt kein 8.4.1 o.ä. — **kein Versions-Update nötig,**
wir matchen das Skill schon exakt. Falls sich das mal ändert: einfach
`pio pkg update` im relevanten Environment, `^8.3.0` erlaubt automatisch
jede 8.x-Version, aber keinen Sprung auf 9.x.

## Fehlende Begleit-Dateien

Das Skill referenziert mehrfach Dateien, die im Quell-Repo **nicht**
mitgeliefert werden (Stand 2026-09-16, nur `SKILL.md` im `eezstudio/`-
Ordner dort):

- `~/.claude/skills/eezstudio/verify_project.py` (Gate 3/4 — Named-Reference-Integrität)
- `~/.claude/skills/eezstudio/verify_no_canvas_divergence.py` (Gate 5 — C-seitige Geometrie-Overrides)
- `~/.claude/skills/eezstudio/render_pages.py` (optionale PIL-Vorschau)
- `SCHEMA_REFERENCE.md` (kanonische Schema-Referenz mit Zeilen-Zitaten)

Müssen wir selbst schreiben, sobald wir wirklich ein `.eez-project` haben,
an dem wir sie testen können — die Spezifikation im SKILL.md ist präzise
genug dafür (Gate-Beschreibungen in den Abschnitten "The gates the script
must pass" und "Required gates after the script runs"). Bis dahin: Skill
nicht als sofort einsatzbereit missverstehen, es ist die Wissens-/
Workflow-Grundlage, nicht ein fertiges Toolset.

## Verwandtes: eez-studio-mcp

Separat davon (anderes Projekt, andere GitHub-Org: IWILLTBEST) existiert
`eez-studio-mcp` — ein MCP-Server, der behauptet, live in eine laufende
EEZ-Studio-Instanz reinzugreifen (Screenshots, Input-Injection,
Pixel-Diff-Regressionstests), braucht aber einen gepatchten Fork von
EEZ Studio. Laut Nutzer (2026-09-16) auf gutem Weg, offizieller Teil von
EEZ Studio zu werden — bewusst abwarten, bis das passiert ist, statt jetzt
den Fork zu nutzen. Unabhängig davon nutzbar: dieses SKILL.md hier braucht
keinen Fork, nur Python + eine normale EEZ-Studio-Installation.

## Lehre aus der MainNoFL-Pilotkonvertierung (2026-09-16)

Beim Konvertieren von `SquareLine/Prj_BC_ScreenMainNoFL/BC_noFL_Main.spj`
(Skript: `EEZStudio/tmp/convert_mainnofl.py`) wurden Positionen/Größen aus
den `.spj`-Properties `OBJECT/Position`/`OBJECT/Size` als literale Pixel-
Offsets relativ zum jeweiligen `OBJECT/Align`-Anker übernommen. Das war
für alle Widgets bis auf `contHeight` korrekt.

**Falle:** `contHeight` nutzt SquareLine/LVGL-Flex-Layout
(`lv_obj_set_flex_align(...)` im generierten `ui_SMainNoFL.c`). Bei
Flex-Containern sind die `Position`-Werte der Kinder **nicht** die real
gerenderte Position — LVGL berechnet die Platzierung zur Laufzeit über den
Flex-Algorithmus, die gespeicherten Zahlen sind vermutlich nur ein
veralteter/irrelevanter Editor-Reststand. 1:1-Übernahme als literales
`left`/`top` in EEZ führte dazu, dass die Kind-Labels deutlich außerhalb
des Containers landeten.

**Besonders tückisch:** Das menschenlesbare Feld
`OBJECT/Layout_type.strval` bleibt `"No_layout"`, **auch wenn Flex aktiv
ist** — das eigentliche Schaltfeld ist das numerische Geschwister-Feld
`LayoutType` in derselben Property (`0` = frei positioniert, `1` = Flex).
Wer künftig `.spj`-Dateien für weitere Screens ausliest, muss auf
`LayoutType` prüfen, nicht auf den `strval`-Text — das Konverter-Skript
tut das jetzt (siehe `parse`-Funktion, Feld `layout_type`) und gibt vor
dem Schreiben eine Warnung mit betroffenen Widgets aus, statt sie still
falsch zu platzieren.

**How to apply:** Vor jeder weiteren Screen-Konvertierung das Skript
diese Warnung prüfen lassen; bei Treffern die betroffenen Container-Kinder
nicht blind aus `.spj`-Positionen übernehmen, sondern entweder EEZ Studios
eigenes Flex-Layout auf dem Container nachbauen (Flow/Align-Werte aus
`Flow`/`MainAlignment`/`CrossAlignment` — deren genaue Enum-Zuordnung noch
nicht verifiziert ist) oder wie bei `contHeight` geschehen: Platzierung
dem Nutzer überlassen, der es im EEZ-Studio-Canvas per Auge korrigiert.

## Priorität

Migration von SquareLine zu EEZ Studio ist weiterhin nachrangig zur
BLE-Arbeit (Milestone 3). Dieses Skill liegt hier nur als vorbereitete
Referenz für den Zeitpunkt, an dem wir die Migration tatsächlich angehen —
nicht als Aufforderung, jetzt schon EEZ Studio einzuführen.
