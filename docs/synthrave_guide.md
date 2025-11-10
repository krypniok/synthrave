# synthrave – ein AI-gebauter Block-Synthesizer

Synthrave versteht sich als moderner Nachfolger klassischer Beep-/Beeper-Tools.
Die komplette Engine – vom Parser über den Mixer bis hin zu den erweiterten
Instrumenten – wurde iterativ mit GPT/Codex entworfen. Ziel: Skripte, Makros und
Sequencer-Spuren ohne DAW bauen, aber mit einem flexiblen, OpenAL-gestützten
Streaming-Backend.

Warum? Das historische `beep` konnte nur einfache Töne ausgeben und wurde aus
vielen Distributionen entfernt. Synthrave liefert dagegen:

* **Mono/Stereo/Glide/Akkorde/Rests** – direkt pro Timeline-Zeile
* **Drums, Bässe, Leads & Pads** (`KICK`, `SNARE`, `HAT`, `BASS`, `FLUTE`, ...)
* **Samples via `WAV("file.wav")` und Sprachereignisse mit `SAY@voice`**
* **Makros, Wiederholungen, BG/ADV-Flags, Realtime-Routing**
* **Offenes `.srave`/`.aox`-Format + OpenAL-Binary für Low-Latency-Streaming**

---

## 🔧 Usage (geplant)

```
synthrave [global options] token [token...]
synthrave --play sampler.aox [global options]
```

### 🎶 Tokens

* **Mono:** `F[:ms]` → Frequenz in Hz, optional Dauer
* **Stereo:** `L,R[:ms]` → zwei Frequenzen (oder Noten) für L/R
* **Glide:** `A~B[:ms]` bzw. `L0~L1,R0~R1` → linearer Pitch-Verlauf
* **Chord:** `f1+f2+...[:ms]` → mehrere Frequenzen gleichzeitig
* **Rest:** `R:ms` oder `0:ms` → Stille
* **Percussion:** `KICK[:ms]`, `SNARE[:ms]`, `HAT[:ms]` – Drums mit automatischer Hüllkurve
* **Bass/Leads:** `BASS@freq`, `ANALOGLEAD@C4`, `SIDBASS@C2`, `CHIPARP@C5+E5+G5`
* **Pads & FX:** `CHOIR@G3`, `LASER@E5`, `STRPAD`, `BIRDS`, `BELL`, `BRASS`, `KALIMBA`
* **Samples:** `WAV("bells.wav")[:ms]` – 16-bit PCM, mono/stereo
* **Sprachereignisse:** `SAY@voice;opts:text`

Tokens ohne explizite Dauer nutzen die Standardlänge (siehe `-l`).

### ⚙️ Global Options

* `-g` / `--gain` – Gesamtausgabe 0..1
* `-s` / `--sample-rate` – Standard 44100 Hz
* `-l` / `--length` – Default-Dauer in Millisekunden
* `-f` / `--play-file` – `.srave/.aox` oder CSV-ähnliche Timeline laden
* `--fade` – Fade in/out pro Block (ms)
* `--tts-path` – Pfad zu `espeak` oder kompatiblen SAY-Backends

---

## 📄 Sampler-/Timeline-Dateien

Das Textformat ist CSV-inspiriert:

```
token , duration_ms [, gap_ms] [, mode] [, flags]
```

* `token`: Frequenz, Note, Akkord, Stereo-Paar, Glide, Rest, `SAY@` oder eines
  der Shortcut-Instrumente (`KICK`, `SIDBASS`, `LASER`, ...).
* `duration_ms`: Dauer in Millisekunden.
* `gap_ms`: optionale Pause nach dem Block.
* `mode`: Effekte (`GLIDE:220->880`, `UPx:1.3`, `DOWNx:2`, `BINAURAL:7`).
  `|BG` / `|ADV` können hier angehängt werden.
* `flags`: alternative Stelle für `BG`, `ADV` oder zukünftige Optionen.

Sonderzeilen und Kommentare:

* `@MACRO { ... }` – Makrodefinitionen (verschachtelt erlaubt).
* `-SPAN,REPS` – wiederholt die **folgenden** `SPAN` Zeilen `REPS`-mal.
* Kommentare starten mit `#`, `//` oder `--`.

### 🎛️ Makros & Samples

Makroblöcke kapseln Intro/Outro, Layer oder wiederkehrende Patterns. Innerhalb
eines Makros dürfen weitere Makros, `WAV`-Zeilen oder SAY-Events auftauchen.

Beispiel (`examples/hyper_shimmer_demo.aox`):

```
@HYPERBASS {
BASS@45,800,60,BG|ADV
KICK@150->40,200,20,BG|ADV
SAY@de:hyperbass aktiviert!,0,0
}

@SHIMMERPAD {
WAV("bells.wav"),0,0,BG
FLUTE@C5,600,40,
FLUTE@E5,600,40,
FLUTE@G5,800,80,
}

# Timeline
@HYPERBASS,0,0,
@SHIMMERPAD,0,0,
PIANO@C4,400,40,
PIANO@E4,400,40,
PIANO@G4,800,80,
@HYPERBASS,0,0,
```

`BG` markiert Hintergrund-Layer, `ADV` signalisiert dem Scheduler, dass nach der
Sektion zum nächsten Szenenabschnitt gewechselt werden darf.

---

## 🎼 Beispiele (CLI)

```bash
# einfacher Ton 440 Hz, 200 ms
synthrave 440:200

# Stereo & Glide
synthrave "440,660:500" 220~880:1000

# Akkord, Pause, SAY-Event
synthrave 440+550+660:800 0:200 SAY@ai;text=Done!

# Sequenz aus Datei
synthrave --play examples/hyper_shimmer_demo.aox -g 0.35
```

---

## 🛠️ Build

```
make
./build/synthrave
```

Abhängigkeiten: `libopenal-dev` (ESpeak optional für SAY).

---

## Streaming-Architektur

Synthrave rendert blockweise (`STREAM_CHUNK_FRAMES`) in einen Ringpuffer und
füllt eine OpenAL-Queue. Der Mixer-Scheduler kümmert sich um Prioritäten (`BG`
vs. Front-Layer), Buffer-Refills und Soft-Clipping. Die neuen Instrument-Stubs
(`LASER`, `CHOIR`, `ANALOGLEAD`, `SIDBASS`, `CHIPARP`) liefern schon
Block-orientierte Samples, die direkt in den Mixer eingespeist werden können.

### mid2sr (Ausblick)

Ein zukünftiges `mid2sr`-Tool wandelt `.mid` in `.srave` um:

1. **Program Changes → Token-Mapping** (z. B. Piano → `PIANO`, Lead → `ANALOGLEAD`).
2. **Velocity → Gain/Pan** (Skalierung + BINAURAL-Modes für Stereo).
3. **Controller → Modes** (`mod wheel` → `UPx`, Channel Pressure → `GLIDE`).

Damit lassen sich klassische DAW-Arrangements in Skriptform exportieren.
