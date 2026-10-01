# Aufbau

Wer das Programm zum ersten Mal öffnet, sollte nach dieser Seite wissen, wo
was steht und warum.

## Die Schichten

```
tools/     Erzeuger und Prüfer (Python, Shell) — laufen beim Bauen
             │
include/   Die Schnittstelle des Kerns
src/       Der Kern: Dateiformat, Simulation, Material, Bilder, Klang
             │  kennt kein Windows, kein ImGui, keine Grafikschnittstelle
             │  → deshalb vollständig prüfbar, auch auf Linux
gui/       Die Oberfläche: ImGui, Win32, Direct3D/OpenGL
             │  kennt den Kern, aber nie umgekehrt
tests/     3877 Prüfungen gegen den Kern
```

**Die Trennlinie ist die wichtigste Entscheidung des ganzen Programms.** Der
Kern hat keine einzige Zeile, die ein Fenster oder eine Grafikschnittstelle
voraussetzt. Alles, was sich prüfen lässt, liegt dort — und das ist fast
alles: Kurven, Zeitplanung, Bahnen, Abpraller, Zerleger, Prüfregeln.

Was in `gui/` liegt, ließe sich nur mit einem Fenster prüfen. Deshalb liegt
dort so wenig wie möglich.

## Der Kern

| Datei | Aufgabe |
|---|---|
| `effect.h` / `effect.cpp` | das Datenmodell: dreizehn Primitivtypen, Flags, Kurven |
| `read.cpp` / `write.cpp` | `.efx` lesen und verlustfrei schreiben |
| `validate.cpp` | 33 Prüfregeln, jede aus dem Spielcode belegt |
| `fields.cpp` | welcher Typ welche Felder hat — gegen das Original gemessen |
| `curve.cpp` | wie sich ein Wert über die Lebensdauer verändert |
| `sim.cpp` | Zeitplanung, Bahn, Kollision |
| `particles.cpp` | die laufende Vorschau, erzeugt Geometrie |
| `timeline.cpp` | die Uhr der Wiedergabe |
| `scene.cpp` | Testraum, Gitter, Himmel, Windfahne |
| `camera.cpp` | Umlaufkamera, Matrizen, Strahl vom Bildschirm in die Welt |
| `assets.cpp` | Materialbestand, `.pk3` lesen |
| `inflate.cpp` | Deflate auspacken |
| `image*.cpp` | TGA, JPEG, PNG |
| `sound.cpp` | WAV selbst, MP3 über `third_party/minimp3.h` |
| `shader.cpp` | `.shader`-Dateien |
| `i18n.cpp` | 392 Texte in vier Sprachen — **erzeugt**, nicht von Hand |
| `theme.cpp` | sechs Themen mit gemessenen Kontrasten |
| `jobs.cpp` | Arbeitsverteiler |
| `undo.cpp` | Rückgängig als Abzüge |

## Die Oberfläche

`App` ist eine Klasse, aber auf sieben Dateien verteilt — nach Aufgaben:

| Datei | Aufgabe |
|---|---|
| `app.cpp` | Lebenszyklus, Datei öffnen und speichern, Rückgängig, Ansicht |
| `app_panels.cpp` | Menü, Werkzeugleiste, Zeitleiste, Segmentliste, Statuszeile |
| `app_properties.cpp` | die Eigenschaftsseite des gewählten Segments |
| `app_dialogs.cpp` | alle Dialoge |
| `app_widgets.cpp` | wiederverwendbare Eingabeelemente |
| `app_playback.cpp` | Uhr, Auslösen, Klang, untergeordnete Effekte |
| `app_resources.cpp` | Texturen, Materialbestand, Diagnose |

Vorher war das **eine Datei mit 3524 Zeilen und einem Dutzend
Zuständigkeiten** — die Stelle, an der ein Leser aufgibt.

Die Aufteilung ändert nichts am Verhalten: dieselbe Klasse, dieselben
Methoden, nur nach Aufgaben sortiert. Sie hat aber sofort etwas sichtbar
gemacht, das vorher verborgen war: zwei Hilfsfunktionen wurden von mehreren
Stellen gebraucht und stehen jetzt in `app_shared.h`. In einer großen Datei
fällt so etwas nicht auf.

## Regeln, an die sich der Quelltext hält

**Der Kern kennt die Oberfläche nicht.** Nie. Wenn der Kern etwas von außen
braucht — eine Datei laden, einen Klang abspielen —, bekommt er einen Rückruf
hereingereicht.

**Kein nacktes `new`/`delete`.** Null im ganzen Baum.

**Keine Ausnahmen.** Fehler kommen als Rückgabewert. Bei einem Programm, das
ständig fremde Dateien liest, sind Fehler der Normalfall, nicht die Ausnahme.

**Eine Wahrheit je Sache.** Zweimal ist es schiefgegangen — `playing_` neben
der Uhr, die Betriebsart an zwei Stellen —, und beide Male war es ein Fehler,
den der Benutzer gemeldet hat.

**Erzeugte Dateien werden nicht von Hand bearbeitet.** `i18n.h`, `i18n.cpp` und
`build.bat` entstehen aus `tools/`. Wer sie ändert, verliert es beim nächsten
Bau.

## Was geprüft wird und womit

| | |
|---|---|
| `tests/tests.cpp` | 3877 Prüfungen gegen den Kern |
| `tools/check_gui.sh` | übersetzt acht `gui/`-Dateien gegen echtes ImGui |
| `tools/fuzz.sh` | alle Zerleger unter Address- und UB-Sanitizer |
| `tools/lint_warnings.sh` | schärfste Warnungen, meldend |
| `tools/lint_i18n.py` | Oberflächentexte ohne `tr()` |
| `tools/lint_members.py` | Membernamen, die nirgends deklariert sind |
| `tools/lint_includes.py` | benutzte Namen, deren Kopf fehlt |
| `tools/lint_calls.py` | Aufrufe von Funktionen, die es nicht gibt |
| `tools/lint_gl.py` | OpenGL-Aufrufe ohne `gl::` |
| `tools/lint_std.py` | fehlende Standard-Köpfe |

Die sechs Prüfer sind nicht aus Prinzip entstanden, sondern jeder als Antwort
auf einen Baufehler, der beim Benutzer aufschlug. Das erklärt ihre Form: sie
sind eng gebaut und melden fast nie etwas — genau deshalb schaut man hin, wenn
sie etwas melden.

## Wo ich bewusst abweiche

**Kein GSL.** `span` und `not_null` wären nützlich, sind aber eine
Abhängigkeit. Das Programm soll als einzelne `.exe` ohne Zusatzpakete laufen.

**`third_party/minimp3.h`** ist die einzige fremde Datei. Ein MP3-Decoder ist
rund zweitausend Zeilen, bei denen ein Fehler als Rauschen auftritt und nur bei
bestimmten Bitraten. Gemeinfrei (CC0), eine Datei, ISO-konform geprüft.

**`shader::parseInto` hat 160 Zeilen.** Ein Zerleger für ein Format mit vierzig
Schlüsselwörtern ist eine lange Fallunterscheidung; sie aufzuteilen würde sie
schwerer lesbar machen.
