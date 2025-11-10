# Synthrave

Synthrave ist ein reines C-Programm, das über OpenAL einen kleinen, mehrspurigen
Software-Synthesizer bereitstellt. Jede Spur kann einem Instrument zugeordnet
werden; Instrumente besitzen einfache ADSR-Hüllkurven und nutzen unterschiedliche
Wellenformen (Sinus, Rechteck, Sägezahn, Dreieck). Das Beispielprogramm rendert
die Audiodaten blockweise, legt sie in einen Ringpuffer und streamt die Mischung
in Echtzeit an OpenAL.

## Abhängigkeiten

- C-Compiler mit C11-Support (getestet mit GCC 13)
- GNU Make oder ein kompatibles `make`
- OpenAL-Implementierung (z. B. `openal-soft`)
- Optional für Repo-Automation: [GitHub CLI](https://cli.github.com/) (`gh`)

## Build & Run

```bash
make            # kompiliert nach build/synthrave
make run        # kompiliert (falls nötig) und startet das Demo
make clean      # löscht den build-Ordner
```

Beim Start legt Synthrave mehrere Streaming-Puffer an, die fortlaufend über den
internen Ringpuffer aufgefüllt werden. So laufen Rendering und OpenAL-Wiedergabe
parallel; die Sequenz dauert ca. 8 Sekunden.

### CLI (oabeep-Modus)

```bash
./build/synthrave 440:500 0:250 660:500     # Ad-hoc Tokens
./build/synthrave C4:1000                   # Noten-Syntax
./build/synthrave -f examples/minute_showcase.aox -g 0.35
```

Optionen:

- `-sr <rate>` – Samplerate (Default 44100)
- `-g <gain>` – Output-Gain 0..1 (Default 0.3)
- `-l <ms>` – Default-Dauer pro Token (Default 120 ms)
- `-fade <ms>` – Fade pro Event (Default 8 ms)
- `-f <file>` – `.srave/.aox`-Datei abspielen
- `-espeak <pfad>` – Pfad für SAY-Events (Default `espeak`)
- `SAY@voice;text=...` – Syntax für Sprachereignisse (z. B. `SAY@en;text=Hello`).

## Git/GitHub-Helfer

Das Makefile bringt zwei Komfort-Targets mit:

- `make push` staged alle Änderungen, erstellt automatisch einen Commit
  (`COMMIT_MSG="..."` anpassbar) und führt anschließend einen `git push --force`
  auf das gewählte Remote/Branch aus (Standard: `origin`). Vorsicht: Damit wird
  die Historie auf dem Remote überschrieben.
- `make repo REPO_NAME=<owner/name>` erstellt per `gh repo create` ein neues
  GitHub-Repo, setzt das Remote (Default `origin`) und macht direkt den ersten
  Push. Die Sichtbarkeit lässt sich via `VISIBILITY=public|private|internal`
  steuern (`public` ist Standard).

## Projektstruktur

- `include/` – Öffentliche Header (`synthrave/…`).
- `src/` – Implementierung von Instrumenten, Synth-Engine, Ringpuffer und `main.c`.
- `Makefile` – klassischer Build plus Git/GitHub-Helfer.

## Dateiformat (.srave / .aox)

`.srave` (alias `.aox`) beschreibt Block-Sequenzen für den Synth. Die Datei
beginnt optional mit Makros, gefolgt von Timeline-Zeilen.
Eine erzählerische Gesamtübersicht (inkl. CLI-Ideen) findest du zusätzlich in
`docs/synthrave_guide.md`. Ein konkretes Beispiel liegt unter
`examples/hyper_shimmer_demo.aox`. Ein ausführliches 60‑Sekunden‑Showcase,
das alle Features (Makros, BG/ADV, SAY, WAV, neue Instrumente) nutzt, findest
du in `examples/minute_showcase.aox`.

### Kopfbereich mit Makros

```
@BELLSTACK {
    440 , 200 , 0 , NORMAL
    880 , 150 , 10 , NORMAL|ADV
}

@SCENE_BG {
    WAV("wind.wav"), 8000, 0, BG
    SAY@guide;text=Welcome to Synthrave!
}
```

- `@NAME { ... }` definiert Makros; sie können Tokens, weitere Makros,
  `SAY`-Zeilen oder `WAV("…")` enthalten.
- Makros dürfen verschachtelt sein, `BG`/`ADV`-Flags direkt neben Timeline-Zeilen
  oder als Default für den gesamten Block tragen.
- Innerhalb eines Makros bleiben alle Zeilen gültige Timeline-Einträge.

### Timeline-Zeilen

```
token , duration_ms [, gap_ms] [, mode] [, flags]
```

- `token` siehe Tabelle unten.
- `duration_ms` bestimmt Blocklänge; `gap_ms` pausiert danach.
- `mode` fasst Effekte zusammen, z. B. `GLIDE:220->880|BG`.
- `flags` (optional) dienen für `BG`, `ADV` oder spätere Optionen.

### Unterstützte Tokens

- Frequenzen (`440`), Noten (`C4`, `A#3`), Akkorde (`440+550+660`).
- Stereo-Notationen (`"L,R"`) sowie Glides `A~B` oder `"L0~L1,R0~R1"`.
- Rests (`0`, `R`), SAY-Events (`SAY@voice;opts:text`).
- Synth-Shortcuts: `KICK`, `SNARE`, `HAT`, `BASS`, `FLUTE`, `PIANO`,
  `GUITAR`, `EGTR`, `BIRDS`, `STRPAD`, `BELL`, `BRASS`, `KALIMBA`,
  `WAV("file.wav")`.
- Neue Instrumente: `LASER`, `CHOIR`, `ANALOGLEAD`, `SIDBASS`, `CHIPARP`.

### Modes & Flags

- `GLIDE:220->880` erzwingt Frequenzverlauf.
- `UPx:1.3`, `DOWNx:2` skalieren Pitch-/Tempo-Faktoren.
- `BINAURAL:7` erzeugt Links/Rechts-Versatz.
- `|BG`, `|ADV` dürfen am Ende von `mode` stehen oder separat in `flags`.

### Beispiel-Datei

```text
@LASER_BG {
    LASER@E5 , 120 , 10 , GLIDE:1320->220|BG
}

@PAD_ADV {
    CHOIR@C4 , 800 , 0 , UPx:1.1 , ADV
    STRPAD , 600 , 50 , BINAURAL:5
}

SAY@narrator;text=Boot sequence initiated , 400 , 120
WAV("vox/warmup.wav") , 8000 , 0 , BG
LASER , 120 , 30 , GLIDE:1760->220|ADV
CHOIR@G3 , 600 , 20
CHIPARP@C5+E5+G5 , 240 , 0 , MODEX|ADV
PAD_ADV
SAY@narrator;text=Entering synthrave hyperspace , 400 , 0 , ADV
```

## Erweiterungsideen

- Weitere Instrumenttypen oder Filter hinzufügen.
- Ereignisse zur Laufzeit generieren (z. B. über MIDI-In oder Netzwerk).
- Effekte wie Delay/Reverb hinzufügen oder externe Controller einbinden.

## Instrument-Erweiterungen

Die neuen Block-Synth-Stubs befinden sich in `include/synthrave/instruments_ext.h`
und `src/instruments_ext.c`. Jede Implementierung erhält einen State + einen
Prozess-Schritt (blocküblich `float`-Samples pro Frame):

| Instrument  | Token / Parameter | Typische Dauer | State/Workflow |
|-------------|------------------|----------------|----------------|
| LASER       | `LASER@E5`       | 60–200 ms      | `LaserSynthState` hält Sweep-Fortschritt + Resonanz; `laser_synth_process` fächert Frequenzen mit `lerp`, erzeugt resonante Sinus-Schleifer. |
| CHOIR       | `CHOIR@C4`       | 400–1200 ms    | `ChoirSynthState` speichert Formant-Sines (4 Stimmen) + Envelope; `choir_synth_process` akkumuliert detunete Sines mit weicher Attack. |
| ANALOGLEAD  | `ANALOGLEAD@A4`  | 120–800 ms     | `AnalogLeadState` verfolgt `current/target` Frequenz und `glide_rate`; im Block dreieckiger Portamento-Saw mit Pulsanteilen. |
| SIDBASS     | `SIDBASS@C2`     | 200–600 ms     | `SidBassState` simuliert 3-stufige Lautstärke (C64-Lautstärkeregister) mit festen Pulswellen. |
| CHIPARP     | `CHIPARP@C5+E5`  | Mehrfach 60 ms | `ChipArpState` rotiert bis zu 4 Frequenzen, wechselt alle `tick_ms` (16tel ≈ 62.5 ms bei 120 BPM). |

- Jeder Stub nutzt `SynthBlockConfig` (Sample-Rate + Blockdauer).
- Portamento/Glide funktioniert blockweise (z. B. `analog_lead_set_target`).
- `chip_arp_init` erlaubt Tick-Zeiten für extreme 16tel (z. B. 32 ms).

## Syntax-Referenz

| Token / Syntax            | Beschreibung                                   | Beispiel                           | Hinweis |
|---------------------------|------------------------------------------------|------------------------------------|---------|
| `440` / `C4` / `A#3`      | Einzelton (Hz oder Note)                      | `C#5 , 250`                        | Mono, BG/ADV erlaubt |
| `440+550+660`             | Akkord, additiv                                | `C4+E4+G4 , 500 , 0 , UPx:1.1`     | Mono; Flags möglich |
| `"L,R"`                   | Stereo-Split                                  | `"C4,E4" , 400 , 10 , BINAURAL:7`  | Stereo; BG ok |
| `A~B` / `L0~L1,R0~R1`     | Glide zwischen zwei Tönen (Mono/Stereo)       | `A3~E4 , 300 , 0 , GLIDE:220->880` | BG/ADV ok |
| `0` / `R`                 | Rest                                          | `0 , 120`                          | Wird oft mit `BG` kombiniert |
| `SAY@voice;opts:text`     | Sprachbefehl                                  | `SAY@ai;speed=1.2:Hello!`          | Hintergrund (`BG`) sinnvoll |
| `WAV("file.wav")`         | WAV-Snippet                                   | `WAV("fx/boom.wav"),300`           | Mono/Stereo je nach Datei |
| Percussion Shortcuts      | `KICK`, `SNARE`, `HAT`, `BASS` …              | `HAT , 90 , 10`                    | Mono |
| New Instruments           | `LASER`, `CHOIR`, `ANALOGLEAD`, `SIDBASS`, `CHIPARP` | `CHOIR@F3 , 600 , 0 , BG`   | Mono (Pseudo-Stereo via Mode) |
| SAY / BG / ADV Flags      | `BG`, `ADV` an `mode` anhängen oder im `flags` Feld positionieren | `SAY@nav;text=... , 500 , 0 , ADV` | Flags steuern Mixer-Priorität |

### Makros

- Syntax: `@NAME { ... }`.
- Innerhalb können weitere Makros referenziert werden, solange zyklische
  Abhängigkeiten vermieden werden.
- Ein Makroaufruf in der Timeline wird wie eine expandierte Blockliste
  behandelt.

### WAV-Import & SAY

- `WAV("...")` lädt Samples (Mono/Stereo). Optionale `mode` kann `BG` setzen.
- `SAY@voice;opts:text` erzeugt TTS-artige Events; `voice` wählt Profil,
  `opts` (z. B. `speed=1.1,pitch=-2`) verändern Engine-Parameter.

### BG/ADV Flags

- `BG` (Background) mischt Spur mit niedrigem Prioritätsgewicht
  – praktisch für Ambiente.
- `ADV` (Advance) kennzeichnet Szenenfortschritte (beeinflusst Sequencer/Scheduler).
- Flags können sowohl in `mode` (per `|`) als auch im separaten `flags` Feld
  gesetzt werden.

## Streaming-Architektur & Tooling

- Jede Stimme rendert blockweise (`STREAM_CHUNK_FRAMES`), schreibt in einen
  Ringbuffer (`AudioRingBuffer`), der wiederum OpenAL-Puffer versorgt.
- Der Mixer-Scheduler füllt den Ringbuffer nach, sobald Platz vorhanden ist,
  und queued/dépequeued AL-Buffers (`src/main.c`).
- Die neuen Instrument-Stubs können direkt im Block-Scheduler genutzt werden,
  indem sie pro Timeline-Zeile instanziiert werden.
- In Planung: `mid2sr` CLI konvertiert `.mid` → `.srave`, mappt MIDI
  Program-Changes auf Synth-Tokens, überträgt Velocity auf `gain/pan` und
  fügt optional `mode`/`flags` hinzu, sodass bestehende DAW-Arrangements
  direkt ins Blockformat übertragen werden.
