# efxed

Ein Editor für die Effektdateien von *Jedi Knight: Jedi Academy* — ein
Nachbau von Ravens `EffectsEd` aus dem Jahr 2003, für heutige Rechner.

**Fertiges Programm herunterladen:**
[EffectsEd-Remake-Releases](https://github.com/DennisHerrm/EffectsEd-Remake-Releases/releases/latest)
— entpacken, `efxed.exe` starten. Es aktualisiert sich danach selbst.

## Warum

Das Original ist ein 32-Bit-Programm gegen OpenGL im unmittelbaren Modus. Es
läuft noch, hat aber zwei Fehler, die beim Arbeiten stören:

- **MP3-Klänge spielen nicht.** Es importiert nur `PlaySoundA`, und das kann
  kein MP3 — jeder Effekt mit einem MP3 bleibt im Editor stumm, obwohl er im
  Spiel klingt.
- **Kamerawackeln hat keine Vorschau.** Man stellt es ein und sieht nichts.

Beides ist hier behoben.

## Was es kann

- `.efx` lesen, bearbeiten und **verlustfrei** zurückschreiben — jeder Wert
  bleibt erhalten; nur Kommentare (`//`, `/* */`) gehen wie im Original
  verloren, und die Prüfung sagt das schon beim Öffnen
- alle dreizehn Primitivtypen mit den Eigenschaftsseiten des Originals,
  Feld für Feld gegen das Binary gemessen
- **Vorschau**, die der Engine folgt: Kurven, Zeitplanung, Bahnen, Abpraller,
  Emitter, Todeseffekte — jede Regel aus dem Spielcode belegt
- **Prüfung** mit 33 Regeln, die Fehler meldet, die das Original stillschweigend
  gespeichert hat
- Materialbestand aus dem Spielordner, **einschließlich `.pk3`**
- Texturen (TGA, JPEG, PNG) und Klänge (WAV, MP3)
- Zeitleiste zum Spulen und Einzelbild
- vier Sprachen, sechs Themen, Direct3D 11 und OpenGL 3.3

## Bauen

```
build.bat
```

Braucht Visual Studio 2022 oder neuer und CMake. Python ist optional — ohne
Python werden die Prüfer übersprungen, gebaut wird trotzdem.

Ergebnis: `out\efxed.exe`. **Eine einzelne Datei**, statisch gebunden, ohne
Zusatzpakete — zum Weitergeben genügt sie allein.

`out\efxtool.exe` ist ein Kommandozeilenwerkzeug für ganze Ordner
(`efxtool check <ordner>` prüft alle Effekte auf einmal). Zum Bearbeiten wird
es nicht gebraucht.

## Erste Schritte

1. *Bearbeiten → Spielpfad festlegen* auf den `base`-Ordner der Installation
2. *Datei → Öffnen* auf eine `.efx`
3. Leertaste

Ohne Spielpfad läuft alles außer Texturen und Klängen.

## Wo was steht

`ARCHITEKTUR.md` erklärt die Schichten und die Regeln, an die sich der
Quelltext hält.

| | |
|---|---|
| `ARCHITEKTUR.md` | Aufbau, Schichten, Regeln |
| `ENTSCHEIDUNGEN.md` | warum etwas so ist, wie es ist — das Arbeitstagebuch |
| `ABGLEICH.md` | jeder Befehl des Originals gegen unseren |
| `PRUEFBERICHT.md` | Abgleich mit Binary, Engine-Quelltext und C++-Regeln |
| `OFFEN.md` | was noch fehlt |

## Einstellungen und Protokoll

`%APPDATA%\efxed\` — darin `efxed_settings.txt` und `efxed_start.log`.

Das Protokoll wird **zeilenweise sofort geschrieben**. Bei einem Absturz steht
die letzte Zeile schon darin, und sie sagt, womit das Programm beschäftigt war.

Eine leere Datei `efxed_portable.txt` neben der `.exe` legt beides in den
Programmordner — praktisch, wenn mehrere Fassungen nebeneinander liegen.

## Herkunft

Die Feldzuordnung, die Menübefehle und die Hilfetexte stammen aus den
Ressourcen von `EffectsEd.exe`. Das Verhalten der Effekte ist aus dem
OpenJK-Quelltext hergeleitet, mit Fundstelle je Regel.

Fremde Dateien im Baum:

- `third_party/minimp3.h` — gemeinfrei (CC0).
- `data/help/Using_EffectsEd.html` — Ravens Handbuch zu EffectsEd aus dem
  Jedi-Academy-SDK (2002), unverändert; das Programm zeigt es unter
  *Help → Original manual (2002)*.
- `data/*.efx`, `data/*.shader` — fünf kleine Dateien aus Jedi Academy, nur als
  Prüfmaterial für die Tests.

Diese Inhalte gehören Raven Software / Activision. Ein Fan-Projekt, nicht
verbunden mit Raven Software, Activision oder Lucasfilm; auf Wunsch der
Rechteinhaber werden die Dateien entfernt.
