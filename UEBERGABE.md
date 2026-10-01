# Übergabe an einen neuen Chat

Alles, was nötig ist, um ohne die alten Gespräche weiterzuarbeiten.

Stand: **1.0.0** — die erste Fassung ohne Zusatz. 4051 Prüfungen grün, zehn Prüfwerkzeuge still.

---

## 1. Was das Projekt ist

Ein moderner 64-Bit-Ersatz für **EffectsEd.exe** von Raven Software (2003) —
den Editor für `.efx`-Effektdateien in *Jedi Knight: Jedi Academy*.

Das Original ist PE32, MSVC6, OpenGL im Direktmodus. Der Nachbau ist C++20 mit
CMake, Dear ImGui und zwei Grafikschnittstellen (Direct3D 11 / OpenGL 3.3).

**Zwei Fehler im Original sind belegt und behoben:**
- MP3-Wiedergabe kaputt (nur WAV funktionierte)
- CameraShake ohne Vorschau

Dazu: Absturz bei leerer Liste, Parser-Fehler bei Leerzeichen am Zeilenende.

### Der Anwender

DH — arbeitet an JKA-Mods (Movie Duels) und WC3-Werkzeugen (NeoDex), baut auf
Windows 11 mit **Visual Studio 2026 (MSVC 19.50)**, 32 Kerne, DPI 144.
Kommuniziert auf Deutsch. Kommentare und Dokumente sind deutsch, Bezeichner im
Quelltext englisch.

---

## 2. Aufbau in einem Absatz

```
include/ + src/   Der Kern. Kennt kein Windows, kein ImGui, keine Grafik-
                  schnittstelle → auf Linux vollständig prüfbar.
gui/              Oberfläche: ImGui, Win32, D3D11/GL3. Kennt den Kern,
                  aber nie umgekehrt.
tools/            Erzeuger und Prüfer (Python/Shell), laufen beim Bauen.
tests/            4051 Prüfungen gegen den Kern.
```

**Die Trennlinie ist die wichtigste Entscheidung des Projekts.** Deshalb liegt
fast alles Prüfbare im Kern. Details in `ARCHITEKTUR.md`.

`gui/app.cpp` war einmal 3524 Zeilen und ist jetzt auf **acht** Dateien
verteilt (`app`, `app_panels`, `app_dialogs`, `app_properties`, `app_widgets`,
`app_playback`, `app_resources`, `app_browser`) — dieselbe Klasse `App`, nach
Aufgaben getrennt.

---

## 3. Was ich nachgeschlagen habe, und was davon noch gilt

### Aus dem Engine-Quelltext (OpenJK, liegt unter `/home/claude/openjk`)

Diese Fakten sind **aus dem Quelltext belegt**, nicht geraten. Sie sind die
Grundlage der Simulation und sollten nicht ohne Fundstelle geändert werden:

| Regel | Fundstelle |
|---|---|
| Kurve ohne Flag → immer Startwert (`perc1 = 1.0`) | `CParticle::UpdateSize` |
| `parm` ist **nicht** 0–1: Welle → Frequenz, sonst Zeitpunkt | `FxUtil.cpp` |
| `random` **dämpft**, ersetzt nicht: `perc = rand * perc` | `UpdateSize` |
| Beschleunigung wirkt **doppelt** (`1.0*a*t²`, Raven notierte selbst Zweifel) | `UpdateOrigin` |
| Gleichverteilung beginnt bei **0**, nicht beim Minimum | `FxScheduler.cpp` |
| Emitter sendet nach **Strecke** aus, nicht nach Zeit | `CEmitter::UpdateEmitter` |
| Blitz: Chaos wirkt nur **quer**, nicht entlang der Achse | `RB_SurfaceElectricity` |
| `deathFx` braucht `FX_DEATH_RUNS_FX`; Achse ist **zufällig** | `CParticle::Die` |
| `impactfx`: Achse ist die **Flächennormale** (nicht zufällig!) | `CParticle::Update` |
| Abprall: spiegeln, dämpfen, bei `normal.z>0 && vel.z<4` liegenbleiben | `CParticle::Update` |
| Beim Liegenbleiben werden **beide** Flags gelöscht | `CParticle::Update` |
| Wind ist in SP **und** MP toter Code | `CL_GetWindVector` fehlt |
| `useAlpha` entscheidet, WOHIN das Ausblenden wirkt: ohne → `rgb *= alpha`, mit → Alphakanal | `CParticle::UpdateAlpha` |
| Fehlende Kurvenwerte sind **1.0**, nicht 0 — `alpha { end 0 }` heißt „von voll auf null" | Erzeuger von `CPrimitiveTemplate` |
| `repeatDelay` legt nach, **ohne** den laufenden Effekt abzubrechen — Generationen überlagern sich | `CFxScheduler::AddLoopedEffects` |
| Ein `Light` zeichnet **nichts**, es erhellt nur | `CLight::Draw` |
| `.wav` nicht gefunden → dieselbe Datei als `.mp3` versuchen | `S_LoadSound_Actual` in `snd_mem.cpp` |
| `count` gilt für **alle** Typen, auch `Sound` — `count 2` spielt zweimal | `CFxScheduler::AddScheduledEffects` |
| Bildfolge: `index = (int)(t·fps)`, einmalig hält auf dem letzten Bild, sonst modulo | `RB_ComputeAnimatedImage` in `tr_shade.cpp` |

**Alle 80 Schlüsselwörter** aus `FxTemplate.cpp` sind abgedeckt (geprüft).

### Aus dem Originalbinary (`EffectsEd.exe`, liegt in den Uploads)

Mit Python + `pefile` dekodiert. Ergebnisse:

- **49 Menübefehle**, davon 46 umgesetzt, 3 bewusst weggelassen (Drucken)
- **23 Dialoge, 388 Bedienelemente, 157 Beschriftungen** — alle abgedeckt
- Werkzeugleisten **128, 155, 158, 167** mit 14 Knöpfen
- Maßstäbe: `10 units/foot (WARS)` und `16 units/foot (SOF2)` — die Klammern
  stehen so im Binary
- Toolbar-ID **32870** = „Draw Wireframe" (Menü) = „Draw Grid" (Toolbar), ein
  und derselbe Befehl

**Wenn du etwas am Verhalten änderst, prüfe es gegen das Binary**, nicht gegen
das Gefühl. Das hat in diesem Projekt mehrfach eigene Annahmen widerlegt.

### Aus dem Netz (in diesem Gespräch nachgeschlagen)

| Thema | Ergebnis | Wo es einfließt |
|---|---|---|
| **C++ Core Guidelines** | Prüfbare Regeln: kein nacktes `new`/`delete` (R.11), wenige Argumente (I.23), Rule of Five (C.21) | `PRUEFBERICHT.md`, Nachtrag |
| **Renderziele vs. Ausschnitte** | Der Zielwechsel ist teuer: 50 Ziele ≈ 12 ms, gebündelt ≈ 1,3 ms. Ein Ausschnitt desselben Ziels vermeidet die Puffer-Leerungen | Effektbrowser, `setViewportRect` |
| **MSVC und Windows 7** | Visual Studio 2026 kann **nicht** mehr auf Windows 7 zielen (Microsofts eigene Doku). Die SDK-Köpfe können es sehr wohl — der Blocker ist die Laufzeitbibliothek | `WINDOWS7.md` |

**Was ein Nachfolger nachschlagen sollte, wenn er daran arbeitet:**
- ImGui-Änderungen zwischen Fassungen (wir sind auf **v1.91.5** festgenagelt)
- `SHBrowseForFolder` vs. `IFileDialog` (wir nehmen Ersteres wegen Windows 7)
- Wenn Fäden dazukommen: das Verhalten von `jobs::Pool` bei einem Kern

---

## 4. Fehler, die ich gemacht habe — damit sie nicht wiederkommen

Das ist der wertvollste Teil dieses Dokuments. Jeder dieser Fehler hat eine
Runde gekostet.

### ImGui

**Früher Ausstieg zwischen `Begin` und `End`.** Ein `return` dazwischen
überspringt das `End`, und ImGui hängt eine Fehlermeldung an den Mauszeiger.
Sie ist kein Anzeigefehler, sondern die richtige Diagnose.

**`SetSelected` bei jedem Bild.** Koppelt man das Kennzeichen an einen Zustand,
wählt ImGui den Reiter jedes Bild neu aus — man kann ihn nicht verlassen. Es
muss ein **einmaliges** Kennzeichen sein.

**`IsItemHovered()` bezieht sich auf das zuletzt eingereichte Element.** Wer es
vor `ImGui::Image` fragt, fragt über etwas anderes.

**`AlwaysAutoResize` + Elementbreite `-1`** schaukeln sich auf: das Fenster
wächst nach dem Inhalt, der Inhalt nimmt sich die neue Breite. Ergebnis: ein
meterbreiter Dialog.

**Interne Schnittstelle meiden** (`GetCurrentContext()->…`). Zweimal fast
benutzt, zweimal unnötig gewesen. Ausnahme mit Begründung: `SplitterBehavior`.

### Grafik

**Billboards brauchen die Kameraachsen.** Baut man sie mit festen Achsen auf
und dreht die Kamera danach, stehen sie hochkant — und eine Fläche ohne Dicke
ist unsichtbar. Ein Muzzleflash verschwand deshalb ganz.
Gemessen: 0.996 (richtig) gegen 0.016 (falsch) über den Umlauf.

**Nicht alles additiv zeichnen.** Die Mischart kommt aus dem Shader.
Undurchsichtiges zuerst, nur Undurchsichtiges schreibt Tiefe. Diese Abkürzung
war **zweimal** falsch — erst in der Hauptansicht, dann in den Kacheln.

**OpenGL zählt von unten links.** Bei Ausschnitten muss Y gedreht werden.

**`gl::`-Präfix nicht vergessen.** Windows' `opengl32` bietet nur OpenGL 1.1;
alles Neuere wird über `wglGetProcAddress` geladen. `lint_gl.py` prüft das.

### Windows

**Direkte Aufrufe neuerer APIs verhindern den Programmstart** — nicht das
Übersetzen. Der Lader findet das Symbol nicht, und es passiert *gar nichts*:
kein Fenster, keine Meldung, kein Protokoll. `GetProcAddress` mit Stufen.
`lint_win7.py` prüft das.

**Fest gebundene DLLs ebenso.** `d3d11` und `d3dcompiler_47` gibt es auf
Windows 7 nicht — der Rückfall auf OpenGL wurde nie erreicht, weil das Programm
nicht lud. Ein Rückfall hinter einem harten Ausfall ist kein Rückfall.

**ImGuis DX11-Rücken bindet `d3dcompiler` selbst ein**, über ein
`#pragma comment(lib, ...)` mitten in der Datei. Gelöst über
`gui/imgui_dx11_nodll.h`, das `D3DCompile` per Makro umbiegt — **ohne** die
Fremddatei zu ändern.

**`std::min` braucht `<algorithm>`.** GCC zieht es mit herein, MSVC nicht.
`lint_std.py` prüft das.

### Eigene Tests

**Sechs meiner Tests hielten meine Annahme fest, nicht die Regel.** Beispiele:
„16 steht an erster Stelle" (galt nur, solange 10 fehlte), „Billboards stehen
bei 90° hochkant" (dort standen sie zufällig günstig).

**Ein einzelner Winkel/Wert beweist nichts** — den ungünstigsten Fall suchen.

**Keine festen Zeitschranken.** Ein Test, der unter Last umkippt, prüft die
Rechnerlast, nicht das Programm.

**Effektkoordinaten sind nicht Weltkoordinaten.** Das erste Feld heißt
„vorwärts", nicht „x" (`org = forward·x + right·y + up·z`).

### Zwei Fehler, die sich gegenseitig verdeckt haben

Der lehrreichste Fall des Projekts. `useAlpha` war nie umgesetzt: das
Ausblenden landete **immer** im Alphakanal. Gleichzeitig war die Voreinstellung
fehlender Kurvenwerte 0 statt 1.0.

Beide Fehler zusammen ergaben ein brauchbares Bild — additive Mischung wertet
den Alphakanal nicht aus, also war die falsche Null folgenlos. Erst als
`useAlpha` richtig umgesetzt war, wurde die Null tödlich und ein ganzer Effekt
verschwand.

**Lehre:** wenn eine Korrektur etwas kaputtmacht, das vorher lief, ist nicht
unbedingt die Korrektur falsch. Möglicherweise hat sie nur einen zweiten
Fehler freigelegt.

### Was nur unter EINER Grafikschnittstelle kaputt ist

Drei Fehler dieser Art, alle nur unter einer der beiden sichtbar:

- **Ausschnitt einer gedrehten Textur.** Für das ganze Bild genügt es, oben
  und unten zu tauschen; für einen Ausschnitt muss `1 - y/höhe` gerechnet
  werden. Nur OpenGL. Symptom: jede Kachel zeigte das Bild einer anderen.
- **Löschviereck bei z = 0.** Bei Direct3D ist 0 die *nahe* Ebene — mit
  Tiefenschreiben sperrt das Viereck alles aus. Nur Direct3D. Symptom: alle
  Vorschauen schwarz.
- **Texturkennung nach dem Renderer-Wechsel.** Unter Direct3D ist sie ein
  Zeiger. Symptom: Absturz beim Umschalten.

**Wer eine der beiden Schnittstellen ändert, muss die andere mitdenken.**

### Vier Fehler mit derselben Form

Etwas eingebaut, an einer zweiten Stelle nicht eingetragen:

| vergessen | Folge |
|---|---|
| `src/tiles.cpp` nicht in `CMakeLists.txt` | Linkerfehler beim Anwender |
| `fallbackTexture_` nicht in `forgetGraphicsResources` | Absturz beim Renderer-Wechsel |
| `viewportUsedByEditor_` nicht gesetzt | leeres Raster nach dem Zurückgehen |
| `settleTextureJobs()` nicht gerufen | Wettlauf auf dem Bestand |

Für **jeden** dieser Fälle gibt es jetzt einen Prüfer. Das ist die eigentliche
Antwort auf diese Fehlerform: nicht besser aufpassen, sondern prüfen lassen.

### Werkzeug

**Ein Ersetzen, das nichts findet, ist gefährlicher als eines, das
fehlschlägt.** Bei Patches immer `assert alter_text in datei` — einmal
weggelassen, und die Wiedergabe war stumm kaputt.

**Vor dem Bauen den Bestand prüfen, nicht die eigene Liste.** Ich habe einmal
einen TGA-Schreiber und Wandtexturen gebaut, die es längst gab.

---

## 5. Die Prüfwerkzeuge

Jedes ist als Antwort auf einen Baufehler entstanden, der beim Anwender
aufschlug. Sie melden fast nie etwas — genau deshalb schaut man hin, wenn doch.

| | |
|---|---|
| `tests/tests.cpp` | 4051 Prüfungen gegen den Kern |
| `tools/check_gui.sh` | übersetzt **neun** `gui/`-Dateien gegen echtes ImGui |
| `tools/fuzz.sh` | alle Zerleger unter Address- und UB-Sanitizer |
| `tools/lint_warnings.sh` | schärfste Warnungen, meldend statt blockierend |
| `tools/lint_i18n.py` | Oberflächentexte ohne `tr()` |
| `tools/lint_members.py` | Membernamen, die nirgends deklariert sind |
| `tools/lint_includes.py` | benutzte Namen ohne ihren Kopf |
| `tools/lint_calls.py` | Aufrufe von Funktionen, die es nicht gibt |
| `tools/lint_gl.py` | OpenGL-Aufrufe ohne `gl::` |
| `tools/lint_std.py` | fehlende Standard-Köpfe |
| `tools/lint_win7.py` | Aufrufe neuer als Windows 7 |
| `tools/lint_ns.py` | Namensraum benutzt, Kopf nicht eingebunden |
| `tools/lint_sources.py` | Quelldatei im Baum, aber nicht im Bauskript |
| `tools/lint_gpu.py` | drei Regeln zu geteilten Ressourcen (siehe unten) |
| `tools/check_tsan.sh` | Kern unter dem Fadenwächter |
| `tools/check_win7_exe.py` | liest die **fertige** `.exe`: worauf läuft sie? |
| `tools/build_win7.sh` | Kreuzbau mit MinGW für Windows 7–11 |
| `tools/make_icon.py` | erzeugt `gui/efxed.ico` (Bitmap bis 128, PNG für 256) |

`lint_gpu.py` prüft drei Dinge, die der Übersetzer nicht sehen kann und ein
Test auch nicht — dafür bräuchte es zwei echte Grafikschnittstellen
nacheinander bzw. einen Wettlauf, der einmal unter tausend Läufen eintritt:

1. jedes Feld vom Typ `render::TextureId` muss in `forgetGraphicsResources`
   zurückgesetzt werden
2. jeder `beginViewport`-Aufruf muss `viewportUsedByEditor_` setzen — Browser
   und Editor teilen sich ein Ansichtsziel
3. jede Änderung an `assets_` muss `settleTextureJobs()` vorausgehen — die
   Arbeitsfäden lesen den Bestand

**Prüflauf vor jedem Paket:** `bash tools/verify.sh` — oder von Hand:

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -pthread \
    -Iinclude -isystem third_party src/*.cpp tests/tests.cpp -o /tmp/t && /tmp/t data
bash tools/check_gui.sh
for f in lint_i18n lint_members lint_includes lint_calls lint_ns lint_sources \
         lint_gpu lint_gl lint_std lint_win7; do
  python3 tools/$f.py
done
bash tools/check_tsan.sh    # dauert Minuten, vor einem Paket
```

---

## 6. Erzeugte Dateien — nicht von Hand ändern

| Datei | Erzeuger |
|---|---|
| `include/efx/i18n.h`, `src/i18n.cpp` | `tools/gen_i18n.py` (420 Texte × 4 Sprachen) |
| `build.bat` | `tools/gen_build_bat.py` (nur ASCII, CRLF) |

`gen_i18n.py` prüft seit Kurzem, dass alle vier Sprachfassungen **dieselben
Formatplatzhalter** haben — sonst ist es undefiniertes Verhalten, und zwar nur
in einer Sprache.

---

## 7. Aktueller Funktionsumfang

**Klang läuft** — bestätigt vom Anwender. Damit ist die letzte Stelle
geprüft, die noch nie gelaufen war. Der Weg dorthin lohnt das Nachlesen: die
Datei wurde nicht gefunden (`.wav` in der `.efx`, `.mp3` im `.pk3`), und
danach lehnte das Tongerät die Rate 11025 ab. Beides steht in `app_playback.cpp`
begründet.

Vollständig: alle 13 Primitivtypen, Simulation mit Kurven, Zeitplanung,
Abprallern (`usePhysics`, `bounce`, `impactfx`), Untereffekte bis Tiefe 4,
`deathFx`, Klang (WAV selbst, MP3 über `minimp3`), eigene Leser für TGA, JPEG
und PNG, eigener Deflate, `.pk3`-Zugriff, **40 Prüfregeln**, Rückgängig, sechs
Themen, vier Sprachen, zwei Grafikschnittstellen, Zeitleiste mit
Einzelbildschaltung, **Reiter für mehrere Effekte**, **Effektbrowser als
Startseite** mit laufenden texturierten Vorschauen, mehrere Spielpfade mit
Überschreibregel, verschiebbare Windfahne.

### Was offen ist

- **ImGui-Umstieg auf 1.92.** Wir sind auf 1.91.5 festgenagelt, aktuell ist
  1.92.9. Die Fassung 1.92 bringt die größte Menge Breaking Changes seit 2015:
  dynamische Schriften, `ImGuiBackendFlags_RendererHasTextures` ist Pflicht für
  eigene Grafikschnittstellen, `PushFont` braucht ein zweites Argument, die
  Argumentreihenfolge der `ImDrawList`-Funktionen hat sich geändert (betrifft
  `gui/icons.cpp`). Gewinn: das Vorabbacken der CJK-Bereiche fiele weg. Ein
  eigenes Vorhaben, nicht nebenbei.

- Die Kacheln zeigen keine Untereffekte des Browsers in eigener Auflösung
- `shader::parseInto` hat 160 Zeilen (bewusst, siehe `ARCHITEKTUR.md`)
- 54 Warnungen bei schärfster Einstellung, fast alle Vorzeichen-Umwandlungen an
  harmlosen Stellen

---

## 8. Arbeitsweise, die sich bewährt hat

1. **Messen statt behaupten.** Jede Zahl im Text kommt aus einem Lauf.
2. **Gegenprobe bei jedem neuen Prüfer**: Fehler einbauen → meldet er? →
   zurücksetzen → still?
3. **Eigene Fehler benennen**, nicht glätten. Der Anwender hat mehrfach Fehler
   gefunden, die ich für behoben hielt.
4. **Kommentare begründen das Warum**, nicht das Was — besonders dort, wo eine
   Entscheidung gegen die naheliegende ausfiel.
5. **Nach jeder Runde**: Prüflauf, Kreuzbau, Paket, und `ENTSCHEIDUNGEN.md`
   fortschreiben.

---

## 9. Dateien, die den Rest erklären

| | |
|---|---|
| `ARCHITEKTUR.md` | Schichten, was wo liegt, Regeln |
| `ENTSCHEIDUNGEN.md` | 4600 Zeilen: jede Entscheidung mit Begründung |
| `PRUEFBERICHT.md` | Abgleich gegen Original, Engine und C++-Regeln |
| `ABGLEICH.md` | Menübefehle 1:1 gegen das Binary |
| `WINDOWS7.md` | Was für Windows 7 nötig war und warum |
| `OFFEN.md` | Was noch fehlt |
| `README.md` | Kurzüberblick |

**Für den Einstieg in einen neuen Chat genügen dieses Dokument,
`ARCHITEKTUR.md` und der Quelltext.** `ENTSCHEIDUNGEN.md` ist zum Nachschlagen
gedacht, nicht zum Lesen am Stück.
