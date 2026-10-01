# Was noch fehlt

Stand: 1.0.0-rc24

Der Editor ist vollständig benutzbar, die Vorschau zeigt alle Primitivtypen mit
Texturen, Mischung, Emittern und Klang. Was bleibt, ist überschaubar — und eine
Sache davon ist wichtiger, als sie klingt.

## Die eine, die zählt

### deathFx wird nicht ausgelöst

Wenn eine Primitive stirbt, kann sie einen weiteren Effekt starten. Das steht
in fast jeder Geschossdatei: das Projektil fliegt, und beim Aufschlag kommt die
Explosion.

Bei uns fliegt das Projektil und verschwindet.

Die Maschinerie liegt bereit — `playInto` startet schon untergeordnete Effekte
für Emitter und FxRunner. Es fehlt der Aufruf beim Sterben. Das ist die
letzte echte Lücke in der Simulation.

## Sieht aus wie eine Funktion, ist keine (3)

| Befehl | was fehlt |
|---|---|
| **View > Draw Textured Room** | vier Untereinträge schalten `roomTexture`; `brick.jpg`, `dirt.jpg`, `stucco.jpg` werden nie geladen |
| **View > Screenshot to file** | Menüpunkt ohne Wirkung |
| **View > Screenshot to clipboard** | dito |

## Fehlt ganz

| | |
|---|---|
| **Physik** | `usePhysics`, `bounce`, `impactfx` — die Vorschau kennt keine Kollision, Partikel fallen durch den Boden |
| **OpenGL-Treiberinfo** | die Daten liegen in `Probe` bereit, es fehlt das Fenster |
| **Zuletzt geöffnete Dateien** | |
| **Vier einzeln schaltbare Werkzeugleisten** | wir haben eine feste |
| **PNG** | nutzt den vorhandenen Auspacker, etwa 200 Zeilen. JKA-Effekte benutzen praktisch nur TGA und JPG, beide sind da |

**Bewusst weggelassen:** Drucken, Druckvorschau, Druckeinrichtung.

## Ungeprüft ausgeliefert

Genau ein Teil: die **Klangausgabe** über `waveOut` in `gui/audio_win32.cpp`.
Auf dem Rechner, auf dem der Quelltext entsteht, gibt es kein Tongerät.

Der Klangleser ist vollständig geprüft — Formaterkennung, alle Bittiefen,
Stereo, MP3, 200 verbogene WAVs, 100 verbogene MP3s. Nur das Abspielen nicht.

## Meine Empfehlung

1. **deathFx** — die letzte echte Lücke, und sie fällt an echten Dateien sofort auf
2. **Physik** — Kollision und Abpraller; danach stimmt die Vorschau auch bei
   Funken, die über den Boden springen
3. Der Rest sind Kleinigkeiten, die man machen kann, wenn sie stören

## Was fertig ist

Dateiformat (lesen, schreiben, verlustfrei), Eigenschaftsseiten aller dreizehn
Typen gegen das Original gemessen, Prüfung mit 31 Regeln, 3D-Ansicht mit
Raumarten, Himmel und Sonne, vollständige Simulation (Kurven, Zeitplanung,
Bahn, Ausrichtung, alle Darstellungsarten, Emitter, FxRunner), Materialbestand
mit eigenem Deflate, TGA und JPEG selbst geschrieben, WAV selbst und MP3 über
minimp3, Mischung aus dem Shader, Rückgängig und Wiederherstellen, Klonen,
Dateidialoge, vier Sprachen vollständig, sechs Themen, zwei
Grafikschnittstellen.

**3696 Prüfungen**, vier Prüfer, Übersetzerlauf für `gui/app.cpp`.
