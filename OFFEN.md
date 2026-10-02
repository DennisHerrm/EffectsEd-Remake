# Was noch offen ist

Stand: 2. Oktober 2026, nach der Nachtrunde (1.18.0-rev77 + Nachtarbeit).

Geprüft wird mit drei Läufen, alle grün:

| Lauf | Ergebnis |
|---|---|
| `build\Release\efxtests.exe` (Kern) | 4432 Prüfungen, 0 Fehler |
| Selbsttest `EFXED_SELBSTTEST=alles`, Direct3D 11 | 749 OK, 0 FEHLER |
| derselbe Selbsttest mit `EFXED_RENDERER=gl3` | 749 OK, 0 FEHLER |
| Darstellungslauf `EFXED_SELBSTTEST=darstellung` | 376 Effekte aus Movie Duels, je 4 Zeitpunkte, kein Absturz |
| `tools/lint_*.py` (12 Prüfer) | alle 0 |
| `tools/check_menu.py <EffectsEd.exe>` | 49 Menübefehle: 44 umgesetzt, 5 bewusst ausgelassen, keiner fehlt |

## Bewusst anders als das Original

| Original | efxed | Warum |
|---|---|---|
| Effektzeit hält an, sobald das Fenster nicht vorn ist | läuft weiter | Beim Vergleichen mit Spiel oder Referenz steht das Fenster meist im Hintergrund |
| Speichern ergänzt immer `repeatDelay 300` | schreibt nur, was gesetzt ist | Sonst ändert reines Öffnen und Speichern die Datei |
| Zahlen mit 4 Stellen (`%1.4g`, `1e+004`) | verlustfrei | Genauigkeitsverlust bei jedem Speichern |
| Spielpfad-Dialog mit SourceSafe-Gruppe und Raven-Vorgaben (JK2/SOF2/JA, `w:/game/base/`) | mehrere Spielpfade, kein SourceSafe | SourceSafe gibt es nicht mehr; die Vorgaben zeigen auf Raven-interne Laufwerke |
| F6 / Umschalt+F6 (MFC: nächster Bereich) | nicht belegt | Tab-Reihenfolge von ImGui übernimmt das |
| Drucken, Druckvorschau | fehlt | Ein Partikeleffekt lässt sich nicht sinnvoll drucken |
| „Entf" allein tut nichts, Strg+N/O/S stehen nur im Menütext | alle belegt | Bekannte Fehler des Originals |

## Bewusst nicht nachgebaut, weil das Spiel es so nicht zeigt

- **Glow** (`glow` in Shaderstufen): die Engine zeichnet ihn nur mit
  `r_DynamicGlow 1`. Voreinstellung der Engine ist 0, und in deinen
  Movie-Duels-Konfigurationen (base und MD) steht `seta r_DynamicGlow "0"`.
- **md3: weitere Animationsbilder, .skin-Dateien:** CEmitter setzt nie ein
  Bild und keine Skin — gezeichnet wird Bild 0 mit den Flächenshadern des md3.

## Noch offen

- **Modelllicht** wie auf einer Karte ohne Lichtgitter (Umgebung + Sonne +
  Light-Segmente). Im Spiel kommt das Licht aus dem Lichtgitter der Karte —
  das gibt es im Testraum nicht.
- **Reine MP-Unterschiede** der Darstellung sind nicht einzeln geprüft
  (Maßstab war der Singleplayer-Renderer `rd`).
- **Klangausgabe** ist im Selbsttest nur bis zum Auslösen geprüft, nicht das
  Hören.

## Ergebnisse des Bildvergleichs mit dem Original

Siehe `ABGLEICH.md`, Abschnitt „Bildvergleich". Was dort als „efxed falsch"
steht und dort nicht als behoben markiert ist, ist offen.

## Leistung (gemessen auf dem Entwicklungsrechner, Intel-Grafik, 240-Hz-Bildschirm)

| Lage | CPU in 5 s vorher | nachher |
|---|---|---|
| minimiert | 2,3 s | 0,02 s |
| sichtbar, untätig, OpenGL (ohne VSync-Intervall) | 6,4 s | 0,08 s |
| sichtbar, untätig, Direct3D 11 | 0,3–0,6 s | 0,05 s |

Läuft ein Effekt, weht die Windfahne oder ist die Bibliothek offen, wird
weiter jedes Bild gezeichnet. Große Explosionen (≈600 Teilchen bildschirmfüllend)
kosten auf dieser Grafik ≈20 ms je Bild — das ist Füllrate, nicht Rechenzeit
(Aufbau < 1 ms, Zeichenaufrufe < 0,2 ms).
