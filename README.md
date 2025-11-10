# Synthrave

Synthrave ist ein reines C-Programm, das über OpenAL einen kleinen, mehrspurigen
Software-Synthesizer bereitstellt. Jede Spur kann einem Instrument zugeordnet
werden; Instrumente besitzen einfache ADSR-Hüllkurven und nutzen unterschiedliche
Wellenformen (Sinus, Rechteck, Sägezahn, Dreieck). Das Beispielprogramm rendert
mehrere Instrumente parallel, mischt sie und spielt das Ergebnis direkt über
OpenAL ab.

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

Nach dem Start rendert Synthrave alle Spuren in einen Puffer, lädt ihn in einen
OpenAL-Buffer und spielt die Sequenz komplett ab. Die Dauer des Demos beträgt
ca. 8 Sekunden.

## Git/GitHub-Helfer

Das Makefile bringt zwei Komfort-Targets mit:

- `make push` pusht den aktuellen Branch auf das konfigurierte Remote
  (Standard: `origin`). Auf Wunsch `make push REMOTE=upstream`.
- `make repo REPO_NAME=<owner/name>` erstellt per `gh repo create` ein neues
  GitHub-Repo, setzt das Remote (Default `origin`) und macht direkt den ersten
  Push. Die Sichtbarkeit lässt sich via `VISIBILITY=public|private|internal`
  steuern (`public` ist Standard).

## Projektstruktur

- `include/` – Öffentliche Header (`synthrave/…`).
- `src/` – Implementierung von Instrumenten, Synth-Engine und `main.c`.
- `Makefile` – klassischer Build plus Git/GitHub-Helfer.

## Erweiterungsideen

- Weitere Instrumenttypen oder Filter hinzufügen.
- Ereignisse zur Laufzeit generieren (z. B. über MIDI-In oder Netzwerk).
- Ringpuffer verwenden und den Mixer in Echtzeit füttern, anstatt komplette
  Blöcke vorab zu rendern.
