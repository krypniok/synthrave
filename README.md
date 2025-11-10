# Synthrave

Synthrave ist ein moderner C/OpenAL-Synth, der drei Szenarien abdeckt:

- **oabeep-kompatible Tokens** direkt auf der Kommandozeile.
- **Sequencer-Dateien** im `.aox`-Format (Makros, Loops, SAY, BG/ADV).
- **Native MIDI-SMF-Wiedergabe** (`.mid`) mit kanalbasierter Instrument-Zuordnung.

Alle Varianten laufen über dieselbe Streaming-Engine: Timeline -> Scheduler ->
Ringbuffer -> OpenAL, ergänzt durch optionale Sprachereignisse via `espeak`.

## Highlights

- Drop-in Ersatz für `oabeep`: `./build/synthrave 440:500 0:250 660:500`
- Sequencer mit Makros, Wiederholungsmarkern, BG/ADV-Flags, SAY-Events und Sample-Imports.
- MIDI-Loader (SMF Type 0/1) mit Kanalpan, Velocity/Gain-Mapping und Programmwechseln.
- Live-Klangerzeugung (Sinus, Pads, Percussion, Chip-Arps, Gitarren, etc.) komplett aus C-Code.
- Sämtliche Header liegen direkt unter `include/` – keine Unterordner nötig.

## Anforderungen

- GCC/Clang mit C11-Support
- GNU Make
- OpenAL-Implementierung (getestet mit `openal-soft`)
- Optional: `espeak` oder `espeak-ng` für `SAY@...` Tokens

## Build & Run

```bash
make            # kompiliert nach build/synthrave
make clean      # räumt build/ auf
```

### CLI-Quickstart

```bash
./build/synthrave 440:500 0:250 660:500         # Einzelne Tokens (Hz, Noten, Akkorde)
./build/synthrave C4:1000                       # Noten-Syntax
./build/synthrave -f examples/minute_showcase.aox -g 0.35
./build/synthrave -m examples/monkeyislandtitle.mid -g 0.35
./build/synthrave -espeak /usr/bin/espeak SAY@de;text=Hallo
```

| Option | Beschreibung |
|--------|--------------|
| `-sr <rate>` | Sample-Rate (Default 44100) |
| `-g <gain>` | Ausgangs-Gain 0..1 (Default 0.30) |
| `-l <ms>` | Defaultdauer pro Token (Default 120 ms) |
| `-fade <ms>` | Fade-In/Out pro Event (Default 8 ms) |
| `-f <file>` | `.aox`/`.srave` Sequenz laden |
| `-m <file>` | MIDI-SMF wiedergeben |
| `-espeak <pfad>` | Binary für SAY-Events (Default `espeak`) |

Tokens funktionieren wie bei `oabeep`: `Freq[:ms]`, `L,R[:ms]`, `A~B[:ms]`,
`f1+f2+f3[:ms]`, `0:ms`/`R:ms`, Instrument-Shortcuts (`KICK`, `SNARE`, `HAT`,
`FLUTE`, `PIANO`, `CHOIR`, `LASER`, `CHIPARP`, ...), sowie `SAY@voice;opts:text`.

## SAY-Events & Flags

- `SAY@en;text=Hello` startet zum Zeitpunkt der aktuellen Timeline das TTS-Event.
- Optionale Parameter (per `;`) werden in `espeak`-Argumente übersetzt, z. B.
  `SAY@de;speed=170;text=Hallo Synthrave`.
- Flags: `BG` mischt Ereignisse als Hintergrund, `ADV` erzwingt Timeline-Advance,
  `BINAURAL:x`, `GLIDE:A->B`, `UPx:r`, `DOWNx:r` steuern Synth-Effekte.
- `-espeak` akzeptiert beliebige Binärdateien (z. B. `espeak-ng`, Wrapper-Skripte).

## `.aox` / `.srave` Format

CSV-ähnliche Zeilen:

```
token , duration_ms [, gap_ms] [, mode] [, flags]
```

- `token` siehe Tabelle oben (Frequenz, Note, Akkord, Stereo, Glide, Instrument, SAY, WAV).
- `duration_ms` fällt zurück auf `-l`, falls leer; `gap_ms` pausiert nach der Zeile.
- `mode` erlaubt zusammengesetzte Effekte (`GLIDE:220->880|BG`).
- `flags` werden zusätzlich geparst (z. B. `BG`, `ADV`).

### Makros & Loops

- `@NAME { ... }` definiert Makros; innerhalb können weitere Makros referenziert werden.
- `-N , R` Zeilen (mit negativem Wert im ersten Feld) definieren Wiederholungsmarker:
  `-8 , 4` wiederholt die letzten 8 Zeilen viermal.
- Makros + Loops wurden in `examples/minute_showcase.aox` ausführlich genutzt
  (ca. 60 Sekunden Demo mit BG/ADV, SAY und Layern).

### Beispiel

```text
@PAD {
    STRPAD@C4 , 800 , 0 , BG
    CHOIR@E4 , 600 , 50
}

SAY@narrator;text=Booting systems , 400 , 80
PAD
KICK , 120
HAT , 60 , 10 , BG
440+554+659 , 360 , 0 , BINAURAL:6
```

## MIDI / SMF

```
./build/synthrave -m examples/monkeyislandtitle.mid
```

- Unterstützt SMF Type 0 und Type 1, beliebige Auflösung (Ticks per Quarter Note).
- Pro Kanal wird ein Instrument gewählt (Program Change -> Synth-Mapping).
- Velocity -> Gain, Kanalnummer -> Stereo-Pan (pseudo-random pro Kanal).
- Sustain Pedal (CC64), Pitchbend (grundlegend) und Notenüberlappungen werden beachtet.
- Läuft innerhalb derselben Scheduler-Pipeline wie `.aox` (daher identische Audioqualität).

## Beispiele

- `examples/minute_showcase.aox` – 60 Sekunden Feature-Demo.
- `examples/monkeyislandtitle.mid` – Mehrspurige LucasArts-Fanfare.
- `examples/goonies.mid` – Courtesy of https://download.file-hunter.com/Music/Midi/.

Alle Beispiele lassen sich mit `-g 0.35` gut abhören; bei Bedarf `-sr 48000` setzen.

## Projektstruktur

```
include/          # Öffentliche Header (instrument.h, midi_loader.h, sequence.h, …)
src/              # Sämtliche .c-Dateien (Synth-Engine, CLI, mid2sr-Tool, …)
examples/         # AOX- und MIDI-Beispiele
docs/             # Zusatzguides / Skizzen
Makefile          # Build + Komfort-Targets (push, repo)
```

Alle Includes nutzen `#include "instrument.h"` usw.; Unterordner in `include/`
werden nicht mehr benötigt.

## Entwicklung & Ideen

- Weitere Instrumente oder Filter (Delay, Chorus) ergänzen.
- Live-MIDI-In oder Netzwerksteuerung an den Scheduler anbinden.
- AOX <-> MIDI Konverter (`src/mid2sr.c`) erweitern, um Velocity/Pan/Effekte
  in Flags zu übersetzen.
- Tests lassen sich via `make test` (falls vorhanden) oder individuellen
  CLI-Aufrufen durchführen; für neue Beispiele einfach Dateien unter `examples/`
  ablegen.

Viel Spaß beim Schrauben – Synthrave versteht Tokens, Sequenzen und echte
MIDI-Dateien gleichermaßen und bleibt dabei vollständig in portablem C-Code.
