# Prüfbericht

Ein vollständiger Abgleich gegen drei Quellen: das Originalprogramm, den
Engine-Quelltext und die verbreiteten C++-Regeln.

Stand: 1.0.0-rc39

## 1. Gegen das Originalprogramm

### Menübefehle

```
49 Menuebefehle im Original
46 umgesetzt
 3 bewusst weggelassen (Drucken, Druckvorschau, Druckeinrichtung)
 0 ohne Entsprechung
```

### Bedienelemente in den Dialogen

Alle 23 Dialoge aus der Ressource dekodiert — **388 Bedienelemente**, davon
157 mit Beschriftung.

```
157 Beschriftungen im Original
  0 ohne Entsprechung bei uns
```

Sechs sahen zunächst nach Lücken aus und waren Wortvergleichs-Artefakte:
„Cull Distance" heißt bei uns unter derselben Kennung `FieldCullRange`, „Yaw"
steckt in der Winkelgruppe, und so fort. Einzeln nachgeprüft.

### Werkzeugleisten

Alle vier Leisten (128, 155, 158, 167) mit allen 14 Knöpfen, samt der
Statuszeilentexte des Originals als Hilfetexte.

## 2. Gegen den Engine-Quelltext

### Schlüsselwörter des Dateiformats

Aus `FxTemplate.cpp` extrahiert und gegen unseren Leser gehalten:

```
81 Schluesselwoerter in der Engine
80 kennen wir
 1 Rest: "list" — steht in einem Kommentar, kein Schluesselwort
```

Also **vollständig**.

### Verhaltensregeln, die aus dem Quelltext belegt sind

| | Fundstelle |
|---|---|
| Kurvenauswertung, `perc1` beginnt bei 1.0 | `CParticle::UpdateSize` |
| `parm` bedeutet Zeitpunkt oder Frequenz | `FxUtil.cpp` |
| `random` dämpft, ersetzt nicht | `UpdateSize` |
| Beschleunigung wirkt doppelt | `UpdateOrigin` |
| Gleichverteilung beginnt bei null | `FxScheduler.cpp` |
| Emitter sendet nach Strecke aus | `CEmitter::UpdateEmitter` |
| Blitz: Chaos wirkt nur quer | `RB_SurfaceElectricity` |
| `deathFx` braucht ein Flag, Achse zufällig | `CParticle::Die` |
| `impactfx`: Achse ist die Flächennormale | `CParticle::Update` |
| Abpraller: spiegeln, dämpfen, liegenbleiben | `CParticle::Update` |
| Kamerawackeln: linearer Abfall, Deckel 16 | `cg_camera.cpp` |
| Wind ist in beiden Zweigen tot | `CL_GetWindVector` fehlt |

## 3. Gegen die C++-Regeln

Die Leitlinien betonen **maschinelle Prüfung** statt Lektüre. Also geprüft
statt gelesen.

### Schärfste Warnungen

```
vorher (mit Fremdcode):  141
jetzt:                    54
davon C-Stil-Umwandlungen: 0
```

Der Sprung kommt daher, dass `third_party` jetzt als **Systemkopf** eingebunden
ist: `minimp3.h` ist gemeinfremder Code, sein Stil ist nicht unsere Sache, und
seine 39 Meldungen überdeckten unsere.

Die verbleibenden 54 sind fast alle `sign-conversion` an Stellen, wo eine
Schleifenvariable von 0 bis 63 läuft und dann als Index dient — harmlos, aber
der Übersetzer weiß das nicht. Mechanisch zu unterdrücken wäre Beschäftigung;
die **Zahl im Blick zu behalten** ist der Wert. `tools/lint_warnings.sh` meldet
sie, ohne zu blockieren.

### Zerleger unter Sanitizer

```
4000 Runden verbogene echte Dateien   — kein Treffer
6000 Runden reiner Zufall             — kein Treffer
```

Address- und UB-Sanitizer, über **sieben Zerleger**: `.efx`, Shader, GP2,
Einstellungen, Bilder (TGA/JPEG/PNG), Klänge (WAV/MP3), Deflate.

Das ist die Frage, die bei fremden Dateien zählt — eine `.pk3` kommt aus dem
Netz. `tools/fuzz.sh` macht es wiederholbar.

### Ein echter Fund

`-Wformat-nonliteral` zeigte auf die Meldungen der Prüfung: ihre
Formatzeichenkette kommt aus der **Übersetzungstabelle**, also kann sie niemand
prüfen. Schreibt jemand in einer Sprache `%d`, wo die anderen `%s` haben, ist
das undefiniertes Verhalten — ein Absturz, und nur in dieser einen Sprache.

Der Erzeuger prüft das jetzt: alle vier Sprachfassungen eines Textes müssen
dieselben Platzhalter in derselben Reihenfolge haben. Gegenprobe gemacht.

## Werkzeuge, die den Stand halten

| | |
|---|---|
| `tests/tests.cpp` | 3877 Prüfungen |
| `tools/check_gui.sh` | übersetzt `app.cpp` und `icons.cpp` gegen echtes ImGui |
| `tools/fuzz.sh` | Zerleger unter Sanitizer |
| `tools/lint_warnings.sh` | schärfste Warnungen, meldend |
| `lint_i18n` | Oberflächentexte ohne `tr()` |
| `lint_members` | Membernamen, die nirgends deklariert sind |
| `lint_includes` | benutzte Namen, deren Kopf fehlt |
| `lint_calls` | Aufrufe von Funktionen, die es nicht gibt |
| `lint_gl` | OpenGL-Aufrufe ohne `gl::` |
| `lint_std` | fehlende Standard-Köpfe |

Jeder der sechs Prüfer ist als Antwort auf einen Baufehler entstanden, der beim
Benutzer aufschlug. Keine schöne Bilanz, aber die ehrliche — und sie greifen
inzwischen: die letzten Runden hat der Übersetzerlauf mehrfach Fehler
abgefangen, bevor ein Paket rausging.

---

# Nachtrag: Aufbau statt Abwesenheit von Fehlern

Der erste Durchgang hat gemessen, ob **Fehler abwesend** sind — Warnungen,
Sanitizer, Fuzzing. Das ist eine andere Frage als: *ist der Code gut gebaut?*

Also gegen die C++ Core Guidelines gemessen, dort wo sie prüfbar sind.

## Zwei echte Funde

### R.11 — nacktes `new` und `delete`

`audio_win32.cpp` hatte ein `new` und **vier** `delete`, davon zwei auf
Fehlerpfaden:

```cpp
auto* buffer = new Buffer();
if (waveOutPrepareHeader(...) != MMSYSERR_NOERROR) {
    delete buffer;                        // Pfad 1
    return false;
}
if (waveOutWrite(...) != MMSYSERR_NOERROR) {
    waveOutUnprepareHeader(...);
    delete buffer;                        // Pfad 2
    return false;
}
```

Vergisst man eines, ist es ein Leck; schreibt man eines zu viel, ein doppeltes
Freigeben. Mit `unique_ptr` verschwindet die Frage: der Halter gibt frei, wenn
er aus dem Bereich fällt.

**Nachher: null nackte `new`/`delete` im ganzen Baum.**

Der Umbau brachte prompt die klassische Folgefalle: ein `unique_ptr` auf einen
unvollständigen Typ braucht seine Sonderfunktionen dort, wo der Typ vollständig
ist — sonst schlägt der Übersetzer bei jedem zu, der den Kopf einbindet. Alle
fünf stehen jetzt ausdrücklich da (C.21), Kopieren verboten, Verschieben
erlaubt.

### I.23 — neun Argumente

`playInto` hatte **neun** Argumente, gewachsen mit jeder neuen Fähigkeit:
Achse, Lader, Tiefe, Versatz, Ort, Flächen.

Die Leitlinie nennt den Grund beim Namen: *„Missing an abstraction."* Bei neun
Argumenten vertauscht man zwei, ohne dass es auffällt.

Jetzt ein `PlayContext` mit dem, was während des Absteigens gleich bleibt. Die
drei Werte, die sich ändern (`depth`, `atMs`, `atPosition`), stehen bewusst
nicht darin — sie gehören zum Aufruf, nicht zum Zustand.

## Was die Messung sonst ergab

| Regel | Befund |
|---|---|
| P.8 / R.11 nackte `new`/`delete` | **0** |
| ES.49 C-Stil-Umwandlungen | **0** |
| I.3 Singletons | 0 |
| ES.20 uninitialisierte Member | 0 |
| C.21 Kopieren ausdrücklich verboten | 6 Klassen |
| Con.1 `const`-Methoden | 134 |
| ES.71 range-for | 98 |
| CP.20 Sperren als `lock_guard` | 13 (alle) |
| Enum.3 `enum class` gegen `enum` | 31 zu 4 |
| SL Standardbibliothek statt roher Felder | 681 Stellen |

## Wo wir bewusst abweichen

**Keine Ausnahmen (E-Regeln).** Null `try`/`catch` im Baum; Fehler kommen als
Rückgabewert. Die Leitlinien empfehlen Ausnahmen — aber ein Zerleger für fremde
Dateien meldet ständig Fehler, und die sind hier kein Ausnahmefall, sondern der
Normalfall. `Result{ok, error}` macht das an jeder Aufrufstelle sichtbar.

**Kein GSL.** `span`, `not_null` und `Expects` wären nützlich, sind aber eine
Abhängigkeit. Bei einem Programm, das als einzelne `.exe` ohne Zusatzpakete
laufen soll, wiegt das schwerer.

**Eine Funktion über 60 Zeilen:** `shader::parseInto` mit 160. Ein Zerleger für
ein Format mit vierzig Schlüsselwörtern ist eine lange Fallunterscheidung; sie
aufzuteilen würde sie schwerer lesbar machen, nicht leichter.

## Die ehrliche Antwort auf „ist das hochwertig?"

Was ich belegen kann:

- keine nackten Besitzzeiger, keine C-Stil-Umwandlungen, keine Singletons
- 3877 Prüfungen, davon viele gegen den Spielcode belegt
- Zerleger unter Sanitizer gegen 10 000 verbogene Eingaben ohne Treffer
- zehn Werkzeuge, die den Stand halten

Was ich **nicht** behaupten kann: dass der Aufbau optimal ist. `App` ist mit
über dreitausend Zeilen groß, und `gui/app.cpp` macht mehr als eine Sache.
Wäre das eine Bibliothek für andere, würde ich es aufteilen. Für ein Programm
mit einem Fenster ist es tragbar — aber es ist die Stelle, an der ich zuerst
ansetzen würde, wenn du weitermachen willst.

---

# Nachtrag: Windows 10 und Vollständigkeit nach dem Umbau

## Der Schriftfehler aus g2c

Bei g2c stürzte das Programm an der Schrift ab. Der Fehler hatte **zwei**
Teile, und beide sind hier behoben:

**Die Zeichenbereiche müssen `Build()` überleben.** `ImVector<ImWchar> ranges`
war dort eine Variable im Block; ImGui merkt sich den Zeiger und liest ihn erst
beim Bauen. Hier ist sie `static`, mit begründendem Kommentar an Ort und Stelle.

**Ein gelungener Aufbau heißt nicht, dass die Zeichen da sind.** Der Fehler war
stumm: ImGui meldete nichts, und erst auf dem Bildschirm standen Fragezeichen.

Die Ausfallkette hat jetzt vier Stufen:

```
1. Datei da?          GetFileAttributesA vorher — kein Ladeversuch ins Leere
2. Laden geglueckt?   Rueckgabe von AddFontFromFileTTF geprueft
3. Keine geladen?     ImGui benutzt seine eingebaute Schrift
4. Zeichen da?        FindGlyphNoFallback je CJK-Zeichen NACH Build()
```

Stufe 4 ist die, die bei g2c fehlte. Fehlt ein Zeichen, steht es mit seiner
Nummer im Ablaufprotokoll — man sieht es, bevor jemand anruft.

Auch der Fall „gar keine Schrift geladen" ist abgefangen: die Prüfung greift
auf `Fonts[0]` erst zu, nachdem sie auf leer geprüft hat.

## Die anderen Windows-Bruchstellen

| | |
|---|---|
| Direct3D nicht verfügbar | Rückfall auf OpenGL, mit Begründung im Protokoll |
| OpenGL 3.3 nicht verfügbar | die erste fehlende Funktion wird benannt |
| Dateidialog | `GetOpenFileNameW` — Pfade mit Umlauten und CJK |
| Windows-Fassung | über `CurrentBuildNumber`, nicht über die veraltete API |
| `APPDATA` fehlt | Rückfall auf `HOME`, dann auf den Programmordner |
| Laufzeitbibliothek | statisch gebunden — keine DLL nachzuinstallieren |

## Ist nach dem Aufteilen noch alles da?

Gemessen gegen das Paket **vor** dem Umbau, nicht behauptet:

```
vorher: 56 Methoden in einer Datei
jetzt:  56 Methoden in sieben Dateien

verloren: keine
neu:      keine

55 von 55 Methodenrümpfen Zeichen fuer Zeichen identisch
```

Die 56. ist `App::App() = default;` — kein Rumpf zu vergleichen.

Die Aufteilung hat also **nichts verändert, nur verschoben**. Dazu: alle acht
`gui/`-Dateien übersetzen gegen echtes ImGui, 3877 Prüfungen grün, sechs
Prüfer still, Zerleger unter Sanitizer sauber.
