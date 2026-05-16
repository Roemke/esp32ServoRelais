# Migration: Arduino IDE → VS Code + PlatformIO

## Projektstruktur

```
esp32Servo_pio/
├── platformio.ini          ← Ersetzt Arduino IDE Board/Library-Einstellungen
├── src/
│   ├── main.cpp            ← war: esp32ServoRelais.ino
│   └── power.cpp
├── include/
│   ├── config.h
│   ├── credentials.h       ← NICHT ins Git einchecken!
│   ├── credentials_template.h
│   ├── ownLists.h
│   ├── index_htmlWithJS.h
│   └── power.h
└── lib/                    ← für lokale Bibliotheken (z.B. Bluetti)
```

## Erste Schritte

### 1. VS Code + PlatformIO installieren
- [VS Code](https://code.visualstudio.com/) installieren
- Extension "PlatformIO IDE" in VS Code installieren (Extension-Marketplace)

### 2. Projekt öffnen
- VS Code öffnen → **File → Open Folder** → diesen Ordner wählen
- PlatformIO erkennt `platformio.ini` automatisch

### 3. credentials.h anlegen
Die Datei `include/credentials.h` ist nicht im Repository.  
Vorlage: `include/credentials_template.h` → kopieren und ausfüllen.

### 4. Bluetti-Bibliothek einbinden
Die `Bluetti.h` ist eine lokale Bibliothek. Sie muss in `lib/` abgelegt werden:
```
lib/
└── Bluetti/
    ├── Bluetti.h
    └── Bluetti.cpp
    (+ weitere zugehörige Dateien)
```
PlatformIO findet sie dann automatisch.

### 5. Kompilieren & Flashen
- **Bauen**: Klick auf ✓ in der unteren Statusleiste (oder `pio run`)
- **Flashen**: Klick auf → (Upload) oder `pio run --target upload`
- **Serieller Monitor**: Klick auf Stecker-Symbol oder `pio device monitor`

## Wichtige Unterschiede Arduino IDE ↔ PlatformIO

| Arduino IDE | PlatformIO |
|---|---|
| `.ino`-Datei | `src/main.cpp` (braucht `#include <Arduino.h>`) |
| Board-Einstellung in GUI | `board = wemos_d1_mini32` in `platformio.ini` |
| Partition-Schema in GUI | `board_build.partitions = min_spiffs.csv` |
| Libraries über Library Manager | `lib_deps = ...` in `platformio.ini` |
| Lokale Header im Projektordner | in `include/` ablegen |

## Hinweis: `#include <Arduino.h>`
Die Datei `src/main.cpp` braucht ganz oben:
```cpp
#include <Arduino.h>
```
In `.ino`-Dateien war das implizit — in PlatformIO muss es explizit stehen.  
**→ Ist bereits in main.cpp ergänzt.**

## NimBLE / Bluetooth
Laut readme: NimBLE Version **1.4.0** verwenden (1.4.1 liefert keinen Callback).  
In `platformio.ini` bei Bedarf pinnen:
```ini
h2zero/NimBLE-Arduino @ 1.4.0
```

## Servo-Bibliothek
Laut readme gab es Probleme mit neueren Versionen.  
`madhephaestus/ESP32Servo` ist die Standard-PlatformIO-Variante und funktioniert  
ohne manuellen Compiler-Patch. Falls Probleme auftreten → Version auf `0.13.0` pinnen:
```ini
madhephaestus/ESP32Servo @ 0.13.0
```

## OTA über WLAN (optional)
In `platformio.ini` auskommentierte Zeilen aktivieren:
```ini
upload_protocol = espota
upload_port = 192.168.0.xxx   ; IP des ESP32
```
