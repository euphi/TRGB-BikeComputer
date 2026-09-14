# TRGB-BikeComputer -- Projektkontext für Claude Code (BLE-Empfangsseite für BikeNavRelay)

ESP32-Fahrradcomputer (LVGL-UI; Sensoren: Herzfrequenz, CSC, Forumslader,
ehemals Komoot-BLE-Navigation). Firmware-Gegenstück zur Android-Companion-App
[BikeNavRelay](../BikeNavRelay/) -- ersetzt Komoots eingestellten
BLE-Navigationsdienst durch einen TLV-Parser nach dem dort definierten
Protokoll.

**Vor jeder Änderung an der BLE-Navigation lesen:**
[`../BikeNavRelay/PROTOCOL.md`](../BikeNavRelay/PROTOCOL.md) -- verbindlicher
Wire-Format-Vertrag zwischen der App (Peripheral/GATT-Server) und diesem
Firmware-Code (Central/GATT-Client). Nicht auf eigene Faust vom Protokoll
abweichen -- Änderungswünsche zuerst mit dem Nutzer und im
BikeNavRelay-Repo klären, dort ist das Protokoll die Quelle der Wahrheit.

## Aufgabe (Meilenstein 3 von BikeNavRelay)

`src/BLEDevices.cpp`/`.h`: den alten, größtenteils schon auskommentierten
Komoot-Client-Code (`DEV_KOMOOT`, `readKomootDataAfterNotification()`,
`komootPollingTask()`/`pollKomootData()`, das tote `komootLoop()`) entfernen
und durch einen TLV-Parser nach PROTOCOL.md ersetzen:

- Neue Service-/Characteristic-UUID abonnieren
  (`f7ac2b76-986b-45fd-8e44-f116a61f319d` /
  `7473da02-2de8-4f48-9e46-21b36380c176`, siehe PROTOCOL.md).
- Frame parsen: Byte 0 Versionsnummer, Byte 1 Message-Type
  (HELLO/NAV_UPDATE/NAV_NONE), bei NAV_UPDATE TLV-Einträge
  (Tag 1 Byte | Länge 1 Byte | Wert). Unbekannte Tags überspringen
  (Länge respektieren, Wert ignorieren) -- das ist der Sinn von TLV statt
  eines starren Byte-Layouts wie bei Komoot.
- Neue Icon-/Text-Tabelle für die erweiterten Manöver-Codes aus
  PROTOCOL.md (Referenz-Enum drüben: `Maneuver.java`) -- unabhängig vom
  alten 32-Slot-Komoot-Icon-Index und von OsmAnds internen
  `TurnType`-Konstanten.

## Verifizierte Fakten (aus dem tatsächlichen Code hier geprüft)

- **Komoot nutzte in diesem Code gar kein Notify/Indicate, sondern reines
  Polling:** `komootPollingTask()` liest zyklisch per Task mit
  `pKomootRemoteChar->readValue()`. Die einzige echte
  `registerForNotify(...)`-Registrierung in `BLEDevices.cpp` ist die für die
  CSC-Sensoren (~Zeile 539) -- **keine** auskommentierte
  CCCD-Descriptor-Stelle für Komoot im Code gefunden, anders als eine
  ältere Notiz in BikeNavRelays `CLAUDE.md` vermuten lässt. Grund fürs
  Polling laut Nutzer: **das Notify-Problem lag an der Komoot-App selbst**
  (Komoot hat als Peripheral offenbar nicht sauber notifiziert), nicht am
  ESP32-BLE-Stack hier -- Notify/Indicate funktioniert bei den CSC-Sensoren
  auf demselben Stack einwandfrei. Für die neue Characteristic ist das
  also kein bekanntes Risiko: BikeNavRelay ist eine eigene, kontrollierte
  Android-Peripheral-Implementierung (kein Closed-Source-Verhalten wie bei
  Komoot), daher spricht nichts dagegen, direkt auf Indicate zu setzen.
  Trotzdem beim Umbau kurz mit nRF Connect verifizieren (siehe
  BikeNavRelay-README), ob Indicate ankommt, bevor der TLV-Parser
  draufgesetzt wird -- normale Vorsicht, kein spezielles Warnsignal mehr.
  Für Indicate reicht bei dieser BLE-Lib (`#include <BLEDevice.h>`,
  ESP32-Arduino-Core-BLE, kein NimBLE in `platformio.ini`/`lib_deps`)
  voraussichtlich `registerForNotify(callback, false)` -- der zweite
  bool-Parameter unterscheidet Notify (`true`, Default) von Indicate
  (`false`). Vor dem Umbau kurz gegen die tatsächlich installierte
  Lib-Version prüfen, nicht blind übernehmen.
- BLE-Rollen bewusst so: **BikeNavRelay (Handy) = Peripheral/GATT-Server,
  dieser ESP32 = Central/GATT-Client** (wie bisher bei Komoot) -- nicht
  umdrehen ohne Rücksprache mit dem Nutzer, betrifft auch die bestehende
  Verbindungslogik für die anderen Sensoren (HR, CSC, Forumslader) in
  derselben Datei.
- `src/BLEConnections/` existiert als Verzeichnis, ist aber leer --
  vermutlich Rest eines angefangenen Refactorings (siehe Commit "Refactor
  BLE Connection Management (WIP!)"), kein Code darin.
- `platformio.ini` hat mehrere Build-Environments
  (`trgb-esp32-s3`, `trgb-esp32-s3-FL`, `trgb-esp32-s3-no-usb`,
  `trgb-esp32-s3-idf`) -- vor dem Bauen mit dem Nutzer klären, welches
  gerade relevant ist.

## Build

PlatformIO (`platformio.ini`, Board `esp32s3box`/ESP32-S3).
PlatformIO ist aktuell (Stand 2026-09-14) --
`pio`-Befehle können jetzt normal ausgeführt werden, wenn ein Build/Flash
ansteht. 

Environments: 

* trgb-esp32-s3 - Default für die "Gravel"-Variante 

* trgb-esp32-s3-FL - für den Forumslader am Touren/Pendler-Rad. Aktuell von zweiter Priorität. Nur auf explizite Anfrage bauen


## Programmiersprachen-Präferenz

C++/PlatformIO für die ESP32-Firmware, wie im restlichen Repo. Kein
Java/Kotlin hier -- das ist die Android-Seite in `../BikeNavRelay/`.
