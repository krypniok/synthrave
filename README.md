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

## Erweiterungsideen

- Weitere Instrumenttypen oder Filter hinzufügen.
- Ereignisse zur Laufzeit generieren (z. B. über MIDI-In oder Netzwerk).
- Effekte wie Delay/Reverb hinzufügen oder externe Controller einbinden.
