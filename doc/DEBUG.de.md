# Debugging

## Debug-Seiten (Web-Oberfläche)

Verlinkt von `/debug/menu`:

* **Live-Log** (`/debug/`) -- dieselben Zeilen wie auf der seriellen Konsole, nach Tag filterbar
* **NVS-Inhalt** (`/debug/nvs`) -- gespeicherte Schlüssel und Füllgrad (nur mit `-DDEBUG_APP`)
* **Beschleunigungssensor** (`/debug/imu`) -- Zustand des BMI160, Kalibrierung,
  Wegequalität, Stöße, Steigung, I²C-Fehlerzähler
* **Anstiege** (`/debug/climb`) -- Zustand des Höhenprofils, Einstellungen, Demo-Profil
  (siehe [Anstiege](CLIMB.md))
* **Streckenübersicht** (`/debug/route.json`, `/debug/route/demo`) -- die Liste, wie der
  Screen sie bekommt, Demo-Strecke (siehe [ROUTE.md](ROUTE.md)); auch der serielle Befehl
  `route`
* **Letzter Absturz** (`/debug/coredump`) -- Neustart-Grund, Task und Backtrace des Core-Dumps
* **Simulator** (`/debug/sim`) -- nur im Simulator-Build, siehe [Simulator](SIMULATOR.md)
* **Chart-Array** (`/stat/debugarray`) und **Distanz-Details**
  (`/stat/dist_debug.html`)
* **SD-Karte roh** (`/log/`)

Log-Level werden auf `/log.html` (Level anklicken) oder auf der seriellen Konsole gesetzt,
siehe unten.


## Oberfläche aus der Ferne testen

Screens lassen sich auf der echten Hardware prüfen, ohne sie anzufassen (`src/UiDebug.h`):
ein Screenshot des aktiven Screens und künstliche Touch-Eingaben über HTTP. Die Host-Seite
ist [`Tools/uishot.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/uishot.py):

```
python3 Tools/uishot.py shot main.png          # Screenshot (PNG, 480x480)
python3 Tools/uishot.py tap 332 408            # Tipp -- hier: Einstellungs-Knopf auf RimRidge
python3 Tools/uishot.py press 160 366 800      # langer Druck
python3 Tools/uishot.py swipe 60 200 320 200   # Wischen (alle Screens: zurück zu RimRidge)
python3 Tools/uishot.py screen                 # aktiver Screen, z. B. rim_ridge_settings
```

Koordinaten sind Bildschirmpixel, dieselben wie im EEZ-Canvas. Ein Touch lässt sich
verzögern (`/debug/ui/touch?...&wait=15000`), um etwas anzutippen, während das WLAN aus
ist, z. B. die WLAN-Pille nach `wifi off` auf der seriellen Konsole.


## Logging

Jede Zeile hat einen **Tag** (`RAW FL BLE STAT WIFI SD OP CLI UI WEB`) und einen **Level**
(`DEBUG INFO WARN ERROR`). Der Level wird je Tag gesetzt, getrennt für die serielle Konsole
und die Debug-Logdatei, und im NVS gespeichert:

```
loglevel BLE DEBUG -serial        # BLE-Debugzeilen auf der seriellen Konsole
loglevel STAT WARN -file          # nur Warnungen und Fehler von STAT in die Datei
showloglevel                      # aktuelle Tabelle
```

Dateien auf der SD-Karte, ein Satz je Sitzung (= ein Boot). Die laufende Sitzung schreibt
nach `/BIKECOMP/CUR/`; der nächste Boot verschiebt sie nach `/BIKECOMP/<JJJJMMTT>/` (oder
`/BIKECOMP/NO_TIME/`, wenn die Uhr nie gestellt wurde). Namensschema und die
Kurzstatistik `I_*.txt` stehen unter [Logformat und CLI](TOOLS.md).

| Datei | Inhalt |
|---|---|
| `L_<HHMMSS>.bin` | Binärlog der Fahrt (Format: `src/LogRecords.h`, Werkzeuge: [Logformat und CLI](TOOLS.md)) |
| `D_<HHMMSS>.log` | Debug-Log (Text) |
| `N_<HHMMSS>.log` | rohe Forumslader-Daten (Text, abspielbar mit `replay <pfad>`) |
| `R_<HHMMSS>_NN.bin`, `S_<HHMMSS>.bin` | Rohmitschnitte des Beschleunigungssensors / Stoß-Ausschnitte |

`mem` auf der seriellen Konsole gibt freien internen/DMA/PSRAM-Heap, offene HTTP-Requests,
die Stack-Reserven der Tasks und die Adressen der LVGL-Draw-Buffer aus (warum die wichtig
sind, steht unter [Fallstricke](PITFALLS.md)).


## Serielle Konsole

Der USB-Port bietet eine Kommandozeile (Prompt `bc> `) mit Zeileneditor, Verlauf und
Tab-Vervollständigung. Jedes Terminal, das jede Taste sofort sendet, funktioniert:

```
pio device monitor
python -m serial.tools.miniterm /dev/ttyACM0 115200
picocom /dev/ttyACM0
```

Besondere Optionen sind nicht nötig: Die Konsole zeichnet nur mit CR, Backspace und
Leerzeichen, der Standardfilter von miniterm (der ESC als Symbol zeigt) stört also nicht.
`--eol CR` von miniterm nicht verwenden -- es macht aus jedem empfangenen CR einen
Zeilenvorschub.

| Taste | Wirkung |
|---|---|
| Tab | Befehl oder Argument vervollständigen; noch einmal drücken listet die Kandidaten |
| Hoch/Runter, Strg-P/N | Verlauf (16 Zeilen) |
| Links/Rechts, Pos1/Ende, Strg-A/E | Cursor bewegen |
| Strg-Links/Rechts, Alt-B/F | wortweise bewegen |
| Backspace, Entf, Strg-D | Zeichen löschen |
| Strg-W, Strg-U, Strg-K | Wort / bis Zeilenanfang / bis Zeilenende löschen |
| Strg-C | Zeile verwerfen |
| Strg-L | Zeile neu zeichnen |

`wifi` zeigt den WLAN-Zustand, `wifi off` schaltet das WLAN aus, `wifi on` startet die
automatische Verbindung -- dasselbe wie die WLAN-Pille auf dem Einstellungs-Screen.
`wifi ap [on|off]` ist die Hotspot-Pille, `wifi scan` eine Suche, `wifi list` die
gespeicherten Netze, `wifi add <ssid> <passwort>` und `wifi del <ssid>` bearbeiten sie
(Namen mit Leerzeichen in Anführungszeichen), `wifi apset <ssid> <passwort>` stellt den
Hotspot ein. Das Passwort von `add`/`apset` kommt nicht ins Log. Siehe [WLAN](WIFI.md).

`help` listet alle Befehle, `help <befehl>` zeigt einen. Logzeilen erscheinen über dem
Prompt, der halb getippte Befehl bleibt stehen. Das Terminal sollte mindestens 80 Spalten
breit sein; längere Befehlszeilen scrollen seitwärts. In picocom ist Strg-A die
Escape-Taste -- stattdessen Pos1 verwenden.

Zeilenbasierte Monitore (Arduino IDE) funktionieren weiterhin: Sie senden die ganze Zeile
mit Zeilenumbruch.

## Core-Dump

Ein Core-Dump ist eine Kopie von Stack und weiterem Speicher im Moment eines Absturzes,
mit der sich später die Ursache untersuchen lässt.

Arduino ESP32 ist standardmäßig so konfiguriert, dass Core-Dumps in den Flash geschrieben
werden.

Ohne USB: `/debug/coredump` zeigt Task und Backtrace des letzten Absturzes, und
`/debug/coredump.elf` lädt den Dump für `esp-coredump` herunter (Befehl und Hinweise unter
[Fallstricke](PITFALLS.md), „Abstürze ohne USB"). Die Schritte unten lesen ihn über USB.

Die dort angezeigte ELF-ID („Firmware ELF 4f1cb0873“) findet den passenden Build: jeder Build liegt in
`firmware-archive/builds/<id>/` (lokal, `archive_firmware.py`, die neuesten 30 ungetaggten und alle
getaggten). `Tools/fwarchive.sh <id>` entpackt dessen `firmware.elf` und gibt den Pfad aus, ohne
Argument listet es die Builds. Builds mit uncommitteten Änderungen haben einen `dirty.patch`.

### So kommt man an einen Core-Dump

1. ESP32 IDF installieren: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/
1. IDF-Umgebung aktivieren, im Installationsverzeichnis des IDF (der führende Punkt ist
   wichtig -- er führt das Skript in der aktuellen Shell aus):

    `. export.sh`

1. Den ESP32 per USB anschließen
1. Das Werkzeug espcoredump mit der ELF-Datei des eigenen Builds aufrufen:

    `espcoredump.py  --port /dev/ttyACM0 info_corefile ~/Coding/Bike/TRGB-BikeComputer/.pio/build/trgb-esp32-s3/firmware.elf`

Das lädt den Core-Dump über USB herunter und wertet ihn mit der `firmware.elf` aus. Die
ELF-Datei enthält Debug-Informationen, man sieht also Variablennamen, Funktionsnamen und
Verweise auf Quelldateien.

Die ELF-Datei muss deshalb zwingend zu der installierten Firmware passen.

Statt `info_corefile` geht auch `debug_corefile`; das öffnet einen Debugger (gdb), statt
nur einige Informationen auszugeben. Das ist viel mächtiger, braucht aber Erfahrung mit gdb.


### Einen Core-Dump verstehen (Beispiel)

(Das Beispiel stammt aus einer älteren Firmware-Version; Dateinamen und Zeilennummern sind
heute anders.)

Die erste Information aus einem Core-Dump ist, warum und wo die Software abgestürzt ist.

Der Absturz kann aber recht „weit weg" von der eigentlichen Ursache liegen. Der ESP32
stürzt nur ab bei unzulässigem Speicherzugriff, unzulässigen Befehlen, kritischen
Laufzeitfehlern (z. B. ganzzahlige Division durch null) und wenn die Software absichtlich
abbricht (`abort`), etwa wegen erkannter Heap-Beschädigung, Stack-Überlauf,
Software-Watchdog oder einem fehlgeschlagenen `assert()`.

Ungültige Zeiger können über mehrere Funktionsaufrufe weitergereicht werden, ohne dass auf
den Speicher zugegriffen wird, und Speicher kann sogar von einem anderen Thread beschädigt
worden sein.

Im folgenden Beispiel führt ein falscher Array-Index einige Funktionsaufrufe „weiter
unten" dazu, dass an eine unzulässige Adresse geschrieben wird.

Die Ausgabe von `info_corefile` beginnt mit Meldungen vom Zugriff auf den Flash des ESP32
und dem Herunterladen, dann folgen die aktuellen Register.

Am wichtigsten sind exccause (0x1d StoreProhibitedCause -- Versuch, an eine unzulässige
Adresse zu schreiben) und pc (Adresse des Codes, an dem der unzulässige Zugriff passiert).

```

[...]

================== CURRENT THREAD REGISTERS ===================
exccause       0x1d (StoreProhibitedCause)
excvaddr       0x7e
epc1           0x420f0a91

[...]

pc             0x420223f2          0x420223f2 <lv_chart_set_x_start_point+46>
lbeg           0x40056f08          1074097928
lend           0x40056f12          1074097938
lcount         0x0                 0
sar            0x19                25
ps             0x60c20             396320
threadptr      <unavailable>

[...]

a15            0x3fcafa00          1070266880

```

Danach kommt der Backtrace des aktuellen Stacks des Threads, in dem der Absturz passiert
ist. Er zeigt, welche Funktionen vor dem Absturz aufgerufen wurden, und die Werte der
Parameter:

```

==================== CURRENT THREAD STACK =====================
#0  lv_chart_set_x_start_point (obj=0x3d965540, ser=0x74, id=1) at .pio/libdeps/trgb-esp32-s3/lvgl/src/extra/widgets/chart/lv_chart.c:425
#1  0x4200dd56 in ui_ScrChartSetPostFirst (pos=1, idx=<optimized out>) at src/ui/Screens/Chart/ui_Chart_CustFunc.c:19
#2  0x4200b5c8 in UIFacade::setChartPosFirst (this=0x3fca1750 <ui>, pos=1, idx=149 '\\225') at src/UIFacade.cpp:308
#3  0x4200a6e7 in Statistics::createChartArray (this=0x3fca1800 <stats>, idx=149 '\\225') at src/Stats/Statistics.cpp:411
#4  0x4200a7b9 in Statistics::autoStore (this=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:112
#5  0x4200a7f0 in Statistics::<lambda(Statistics*)>::operator() (__closure=0x0, thisInstance=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:50
#6  Statistics::<lambda(Statistics*)>::_FUN(Statistics *) () at src/Stats/Statistics.cpp:50
#7  0x42086a11 in timer_process_alarm (dispatch_method=ESP_TIMER_TASK) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_timer/src/esp_timer.c:360
#8  timer_task (arg=<optimized out>) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_timer/src/esp_timer.c:386


```

Das reicht, um das Beispiel zu untersuchen.

In anderen Fällen kann aber auch das Folgende wichtig sein:

Als Nächstes stehen Zustand und aktuelle Adresse aller Threads da. Das ist wichtig, wenn
man untersucht, warum die Software „einfriert". In diesem Beispiel sieht man aber, dass es
normal ist, wenn einige Threads des Fahrradcomputers in eigenem Code warten (ID 5 und
ID 7); also vorsichtig mit Schlussfolgerungen.

```
======================== THREADS INFO =========================
  Id   Target Id          Frame 
* 1    process 1070542680 lv_chart_set_x_start_point (obj=0x3d965540, ser=0x74, id=1) at .pio/libdeps/trgb-esp32-s3/lvgl/src/extra/widgets/chart/lv_chart.c:425
  2    process 1070515272 0x400559e0 in ?? ()
  3    process 1070551252 0x4216c746 in esp_pm_impl_waiti () at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_pm/pm_impl.c:832
  4    process 1070549852 0x4216c746 in esp_pm_impl_waiti () at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_pm/pm_impl.c:832
  5    process 1070338036 UIFacade::updateHandler (this=0x3fca1750 <ui>) at src/UIFacade.cpp:114
  6    process 1070522212 0x40382a0e in vPortEnterCritical (mux=0x3fced2bc) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  7    process 1070337676 BCLogger::flushAllFiles (this=0x3fcaea94 <bclog>) at src/BCLogger.cpp:115
  8    process 1070535080 0x40382b6e in vPortEnterCritical (mux=0x3fcf0d80) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  9    process 1070523704 0x40382a0e in vPortEnterCritical (mux=0x3fcee2bc) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  10   process 1070524064 0x40382a0e in vPortEnterCritical (mux=0x3fcee1b0) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  11   process 1070423616 0x40382a0e in vPortEnterCritical (mux=0x3fcd1d98) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  12   process 1070368888 0x40382b70 in vPortEnterCritical (mux=0x3fcc77d4) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  13   process 1070387128 0x40382b70 in vPortEnterCritical (mux=0x3fccc4c8) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  14   process 1070395268 0x40382b70 in vPortEnterCritical (mux=0x3fccdc94) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  15   process 1070382024 0x40382b70 in vPortEnterCritical (mux=0x3fccacd8) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  16   process 1070557508 0x40382a0e in vPortEnterCritical (mux=0x3fcf62ec) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578
  17   process 1070536780 0x40382b70 in vPortEnterCritical (mux=0x3fcf1424) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/include/freertos/portmacro.h:578

```

Dann folgt eine lange Liste mit den Backtraces aller Threads. Ein kurzer Blick gibt einen
Überblick, ob etwas seltsam aussieht; wer andere Threads wirklich untersuchen muss, nimmt
besser `debug_corefile`.

```
==================== THREAD 1 (TCB: 0x3fcf2f58, name: 'esp_timer') =====================
#0  lv_chart_set_x_start_point (obj=0x3d965540, ser=0x74, id=1) at .pio/libdeps/trgb-esp32-s3/lvgl/src/extra/widgets/chart/lv_chart.c:425
#1  0x4200dd56 in ui_ScrChartSetPostFirst (pos=1, idx=<optimized out>) at src/ui/Screens/Chart/ui_Chart_CustFunc.c:19
#2  0x4200b5c8 in UIFacade::setChartPosFirst (this=0x3fca1750 <ui>, pos=1, idx=149 '\\225') at src/UIFacade.cpp:308
#3  0x4200a6e7 in Statistics::createChartArray (this=0x3fca1800 <stats>, idx=149 '\\225') at src/Stats/Statistics.cpp:411
#4  0x4200a7b9 in Statistics::autoStore (this=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:112
#5  0x4200a7f0 in Statistics::<lambda(Statistics*)>::operator() (__closure=0x0, thisInstance=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:50
#6  Statistics::<lambda(Statistics*)>::_FUN(Statistics *) () at src/Stats/Statistics.cpp:50
#7  0x42086a11 in timer_process_alarm (dispatch_method=ESP_TIMER_TASK) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_timer/src/esp_timer.c:360
#8  timer_task (arg=<optimized out>) at /Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/esp_timer/src/esp_timer.c:386

==================== THREAD 2 (TCB: 0x3fcec448, name: 'loopTask') =====================

[...]

```

Was sieht man also im Core-Dump? Die LVGL-Bibliothek ist mit einer _StoreProhibitedCause_
abgestürzt (Versuch, an eine verbotene Adresse zu schreiben).

Wichtig ist aber: `ser->start_point = id;` kann nicht die eigentliche Ursache sein. Bei
genauerem Hinsehen fällt auf, dass `lv_chart_series_t* ser` den Wert 0x74 hat, ein
seltsamer Wert für einen Zeiger. Woher kommt er?
`lv_chart_set_x_start_point(ui_Chart1, ui_Chart1_series[idx], pos);`

Es ist also der Wert von `ui_Chart1_series[idx]` -- das Array speichert Zeiger auf
`lv_chart_series_t`, hat aber nur die Größe 4. Der Core-Dump zeigt idx als „optimized
out". Eine Ebene höher ist idx '149' -- also völlig außerhalb der Grenzen. Lesen außerhalb
der Grenzen ist in C++ meist möglich: Man greift auf Speicher direkt hinter dem Array zu,
in dem andere Variablen liegen, und bekommt deren Wert.

Um zu verstehen, warum außerhalb der Grenzen gelesen wird, geht es noch eine Ebene höher:

`#3  0x4200a6e7 in Statistics::createChartArray (this=0x3fca1800 <stats>, idx=149 '\\225') at src/Stats/Statistics.cpp:411`

Auch diese Funktion bekommt idx als Parameter und reicht ihn nur durch, also noch weiter
nach oben:

`#4  0x4200a7b9 in Statistics::autoStore (this=0x3fca1800 <stats>) at src/Stats/Statistics.cpp:112`

Dort kommt idx aus dem Zähler einer for-Schleife:

```C++
for (uint_fast8_t j; j < 4 ; j++) {
	createChartArray(j);
}
```

Fällt der Fehler auf?

Genau -- `j` ist nicht initialisiert und hat deshalb einen beliebigen Wert. Die Korrektur:

`for (uint_fast8_t j=0; j < 4 ; j++) {`
