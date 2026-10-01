# Abgleich mit dem Original

Alles, was `EffectsEd.exe` von Raven Software (2003) an Oberfläche mitbringt,
gegenübergestellt mit dem, was bei uns **tatsächlich etwas tut**.

Stand: 1.0.0-rc88

**Nachgeprüft, nicht behauptet.** `tools/check_menu.py` liest die
Menü-Ressource aus dem Binary — die Wahrheit, nicht eine Abschrift — und hält
jeden Befehl gegen unsere Übersetzungstabelle:

```
python3 tools/check_menu.py [pfad/zu/EffectsEd.exe]
```

Ohne das Binary überspringt er sich, statt fehlzuschlagen; nicht jeder Rechner,
auf dem gebaut wird, hat es.

## Bilanz

| | |
|---|---|
| Menübefehle des Originals | **49** |
| davon umgesetzt | **44** |
| bewusst ausgelassen | 5 |
| ohne Entsprechung | **keine** |
| Dialoge / Bedienelemente | 23 / 388 — alle abgedeckt |
| gespeicherte Einstellungen | 9 von 9, dazu rund 30 eigene |

## Bewusst ausgelassen

| Befehl | Begründung |
|---|---|
| `Print...`, `Print Preview`, `Print Setup...` | Einen Partikeleffekt zu drucken ergibt nichts Sinnvolles |
| `Recent File` | Wir haben eine eigene Liste zuletzt geöffneter Dateien |
| `OpenGL Driver Info` | Bei uns „Grafiktreiber-Information" — **beide** Schnittstellen, nicht nur OpenGL |

## Wo der Wortlaut abweicht

Acht Befehle heißen bei uns anders. Sie stehen in `check_menu.py` einzeln
aufgeführt, damit die Abweichung eine Entscheidung bleibt und keine Lücke wird:

| Original | bei uns |
|---|---|
| Refresh (reload) assets | Reload Assets |
| Delete | Delete Segment |
| Choose Wall Color... | Wall Color... |
| Choose Background Color... | Background Color... |
| Draw Wind Vector | Show Wind Vector |
| Draw Wireframe | Wireframe |
| Reset/zero view position | Reset View |
| Set Custom FX Origin | Set Custom FX Origin... |

## Die 23 Dialoge

Alle Titel haben eine Entsprechung. Die Eigenschaftsseiten des Originals sind
eigene Dialoge (`Generation`, `Origin/Size`, `Motion`, `Physics`, `Color`,
`Line`, `Length/Size2`, `Model`, `Emitter`, `FxRunner`, `Sound`,
`CameraShake`); bei uns sind es Reiter derselben Fläche — dasselbe Ergebnis,
ohne dass man Fenster herumschiebt.

Beim Abgleich fiel auf, dass wir `Camera Shake` mit Leerzeichen schrieben,
während sowohl das Original als auch das **Schlüsselwort der `.efx`**
`CameraShake` lauten. Zwei Schreibweisen für dieselbe Sache in einer
Oberfläche — behoben.

## Gespeicherte Einstellungen

Das Original merkt sich neun Werte in der Registry. Alle neun haben eine
Entsprechung:

| Original | bei uns |
|---|---|
| `PseudoEngine DrawAxes` | `drawAxes` |
| `PseudoEngine DrawRoom` | `drawRoom` |
| `PseudoEngine DrawWindVector` | `drawWindVector` |
| `PseudoEngine DrawWireframe` | `drawGrid` — Befehl 32870 heißt im Menü „Draw Wireframe" und in der Werkzeugleiste „Draw Grid" |
| `EffectTextured` / `EffectWireframe` / `EffectOverdraw` | `effectRenderMode`, ein Wert für die drei Arten |
| `PseudoEngine PlaySounds` | `playSounds` |
| `PseudoEngine RoomTexture` | `roomTexture` |

Dazu rund dreißig eigene: Thema, Sprache, Grafikschnittstelle, Fensterlage,
Teilerstellungen, Spielpfade, Sonne, Wind, Raumart.

## Werkzeugleisten

Vier im Original (Ressourcen 128, 155, 158, 167) mit zusammen 14 Knöpfen. Bei
uns eine feste Leiste, deren vier **Gruppen** sich einzeln abschalten lassen —
sie entsprechen genau jenen vier. Dasselbe Ergebnis, ohne andockbare Fenster.

## Was wir haben und das Original nicht

Effektbrowser als Startseite mit laufenden Vorschauen, Reiter für mehrere
Effekte gleichzeitig, Zeitleiste mit Einzelbildschaltung, vier Sprachen, sechs
Themen, zwei Grafikschnittstellen mit Rückfall, 40 Prüfregeln mit
Meldungsfenster, Protokollfenster, mehrere Spielpfade mit Überschreibregel,
`.pk3`-Zugriff ohne Auspacken, Rückgängig.

## Zwei belegte Fehler des Originals, die bei uns nicht auftreten

- **MP3-Wiedergabe kaputt** — nur WAV funktionierte
- **CameraShake ohne Vorschau**

Dazu: Absturz bei leerer Liste, Parser-Fehler bei Leerzeichen am Zeilenende.

## Wie dieser Abgleich zustande kam

Drei Anläufe, und die ersten beiden waren **still falsch**. Der erste las nur
das File-Menü aus (10 von 49) und hätte den Rest kommentarlos übersprungen.
Der zweite scheiterte an den beiden Untermenüs in „Ansicht": der Stapel geriet
aus dem Gleichgewicht, und die Auslese endete eine Zeile zu früh — ausgerechnet
bei `About EffectsEd...`.

Der dritte liest bis zum Ende der Ressource und lässt die Schachtelung
beiseite. Gesucht sind Befehlsnummern, nicht der Baum.

Beim selben Abgleich hat sich außerdem eine Änderung von mir als falsch
erwiesen: ich hatte die Reihenfolge des Untermenüs „Draw Textured Room" nach
der Reihenfolge der **Dateinamen** im Binary umgestellt. Die Menü-Ressource
sagt aber eindeutig `No Texture, Brick, Dirt, Stucco` — die Dateinamen stehen
in der Ladereihenfolge des Programms, nicht in der Anzeige. Zurückgenommen.

**Ein Abgleich, der einmal von Hand gemacht wurde, ist nach der nächsten Runde
eine Behauptung.** Deshalb ist er jetzt ein Werkzeug. Eine Gegenprobe hat
gezeigt, dass es anschlägt: zwei Menüpunkte aus der Übersetzungstabelle
entfernt, beide gemeldet.
