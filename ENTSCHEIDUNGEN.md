# Entscheidungen

Kurze Begründungen zu Dingen, die später sonst niemand mehr nachvollziehen kann
— einschließlich der Fehler, aus denen sie stammen.

## Zielplattform

Wie bei g2c: **Windows x64, MSVC, statisch gelinkte Laufzeit**
(`MultiThreaded` in `CMakeLists.txt`). Kein Redistributable, kein
„MSVCP140.dll fehlt" bei jemandem, der das Werkzeug zum ersten Mal startet.

Der Kern (`src/`, `include/`) kommt ohne Windows-Header aus und lässt sich
deshalb auch mit GCC übersetzen und testen. Das ist nicht Kosmetik: bei g2c
war `main_win32.cpp` die einzige Datei, die vor der Auslieferung nie
übersetzt worden war, und genau dort steckten die Fehler. Was hier im Kern
liegt, ist geprüft; was in der Oberfläche liegt, muss auf einer Windows-Maschine
gebaut werden, bevor es jemand bekommt.

## Schriftarten: kein zweites SegoeIcons

Bei g2c stürzte das Programm auf einem Windows-10-Rechner beim Start ab, weil
`AddFontFromFileTTF` für `SegoeIcons.ttf` aufgerufen wurde. Diese Datei gibt es
**nur unter Windows 11**. ImGui bricht mit einer Zusicherung ab, wenn die Datei
fehlt — kein Fehlertext, nur ein sofortiger Absturz beim Start, und zwar
ausschließlich auf fremden Rechnern.

Daraus drei feste Regeln für die Oberfläche dieses Werkzeugs:

1. **Keine Schriftart aus dem Systemverzeichnis wird vorausgesetzt.** Die
   Symbole werden als Zeichensatz **mitgeliefert und eingebettet**, nicht aus
   `C:\Windows\Fonts` geladen. Damit kann der Fall gar nicht erst eintreten.
2. Wo doch eine Systemdatei gelesen wird, ausschließlich über einen Helfer,
   der vorher mit `GetFileAttributesW` prüft, ob sie existiert, und sonst
   stillschweigend auf die eingebaute Schrift zurückfällt. Nie direkt
   `AddFontFromFileTTF` auf einen fest verdrahteten Pfad.
3. Der angeforderte Zeichenbereich umfasst nur die tatsächlich benutzten
   Zeichen, nicht mehrere tausend auf Verdacht. Bei g2c waren es 4608
   angeforderte für 20 benutzte.

Zusätzlich schreibt das Programm ab dem ersten Befehl ein Startprotokoll.
Bei g2c war der einzige brauchbare Hinweis auf den Absturz die letzte Zeile
dieses Protokolls. Diagnose zuerst, Vermutungen später.

## Dialekte: SP und MP sind zwei Parser

Singleplayer und Multiplayer haben getrennte Quelldateien
(`code/cgame/FxTemplate.cpp` gegen `codemp/client/FxTemplate.cpp`) und
unterschiedliche Schlüsselwörter. Der jeweils andere Zweig **überliest ein
unbekanntes Flag stumm** — kein Fehler, keine Meldung, es passiert nur nichts.

| Wort | SP | MP |
|---|---|---|
| `lessAttenuation` | ja | **nein** |
| `affectedByWind` | **nein** | ja |
| `relative` | **nein** | ja |
| `paperPhysics`, `localizedFlash`, `playerView` | **nein** | ja |
| `materialImpact shellsound` | **nein** | ja |

Der Leser nimmt immer alles an — sonst könnte man mit dem Werkzeug fremde
Dateien nicht öffnen. Die Unterscheidung passiert in der Prüfung, über
`Dialect`:

- `Dialect::SP` / `Dialect::MP` — meldet, was der gewählte Zweig nicht kennt
- `Dialect::Both` (Voreinstellung) — meldet alles, was nicht in beiden läuft

Die Kamera-Erschütterung ist in beiden Zweigen zeichengleich
(`code/cgame/cg_camera.cpp` gegen `codemp/cgame/cg_view.c`), dort reicht eine
Implementierung.

## Geteilte Flagbits

Mehrere Flags belegen dieselben Bits wie die Übergangskurven. Das ist kein
Versehen, sondern Raven hat Bits gespart und sich auf Ordnung verlassen:

- `ghoul2Collision` / `ghoul2Decals` liegen auf `FX_SIZE2_RAND` und
  `FX_SIZE2_NONLINEAR` — an einem Cylinder ergibt das eine falsche Kurve
- `paperPhysics`, `localizedFlash` und `playerView` liegen **alle drei** auf
  `0x00010000`, also auf `FX_SIZE2_LINEAR`. Raven schreibt an der Stelle selbst
  in den Quelltext, dass ein Cylinder damit „evilness" ergibt
- `nonlinear`, `wave` und `clamp` teilen sich zwei Bits für drei Zustände:
  `nonlinear|wave` **ist** bitgleich `clamp`

Der letzte Punkt ist der Grund, warum `Channel` die ursprünglichen Wörter
speichert und nicht nur die Bits. Aus `0xC` lässt sich nicht mehr ablesen, ob
dort `clamp` stand oder `nonlinear wave` — die Prüfung braucht aber genau
diesen Unterschied, um sagen zu können, was der Autor geschrieben hat und was
die Engine daraus liest.

## Zahlformat

Der alte Editor schreibt alle Zahlen mit `%1.4g`. Wer eine Datei nur öffnet und
speichert, verliert Stellen: `0.988235` wird zu `0.9882`. Voreinstellung ist
deshalb die kürzeste Schreibweise, die den `float` exakt wiederherstellt.
`NumberFormat::Raven` gibt es weiterhin, für Fälle, in denen ein Textvergleich
mit einer Originaldatei gebraucht wird.

## Was der Shaderleser bewusst nicht kann

Er liest genau so viel, wie die Vorschau braucht: Bildquelle, Überblendung,
Farbherkunft, Bildfolgen, Texturbewegung. `deformVertexes`, Lichtkarten und
q3map-Direktiven werden erkannt und übersprungen, nicht ausgewertet — die
gehören zur Weltgeometrie, nicht zum Effekt.

## Fensteraufteilung: zwei Teiler, kein Docking

Die Aufteilung des Originals ist keine Vermutung. In `EffectsEd.exe` stehen die
Registrierungsschlüssel im Klartext:

```
MainFrame SplitTB      MainFrame SplitLR
MainFrame Left/Top/Right/Bottom, MinX/MinY/MaxX/MaxY, Flags, Show
```

Also genau **zwei** MFC-Splitter und eine gespeicherte Fensterlage. Das bauen
wir nach:

```
+-------------------------------+-----------------+
|        3D-Ansicht             |  Eigenschaften  |
+===============================+  (volle Höhe)   |   <- SplitTB
|        Segmentliste           |                 |
+-------------------------------+-----------------+
                                ^ SplitLR
```

Bewusst **kein ImGui-Docking**. Docking erlaubt es, Bereiche abzureißen, zu
stapeln und zu schließen — und dann findet man sie nicht wieder. Wer den alten
Editor kennt, soll das Fenster wiedererkennen und nicht erst eine
Fensterverwaltung lernen müssen. Zwei ziehbare Teiler, mehr nicht.

Die Teilerstellungen werden als **Anteil** gespeichert, nicht in Bildpunkten.
Sonst verrutscht die Aufteilung bei jedem Monitorwechsel. Untergrenzen sind in
Bildpunkten angegeben und skalieren mit der Bildschirmauflösung mit, damit ein
Bereich auf keinem Gerät so klein wird, dass man ihn nicht mehr fassen kann.

## Farben werden gemessen, nicht behauptet

Sechs Themen: Dunkel (Voreinstellung), Mitternacht, Raven Klassik, Hell,
Solarized Dunkel, Hoher Kontrast.

Jedes läuft durch `enforceReadability`, das Kontrastverhältnisse nach WCAG 2.1
prüft — 4.5:1 für Fließtext, 3:1 für zurücktretende Schrift und Betonungen.
Wo ein Paar durchfällt, wird die Vordergrundfarbe in HSL so weit auf- oder
abgehellt, bis es trägt. **Farbton und Sättigung bleiben erhalten**, es wird
also nicht grau.

Das gilt auch für selbst zusammengestellte Farben. Eine Palette, in der alles
im Bereich `#202020` bis `#303030` liegt (Text/Fläche 1.11:1, praktisch
unsichtbar), kommt danach mit 4.50:1 heraus.

Zwei Details, die beim Messen aufgefallen sind:

- Die Richtung darf **nicht** über die Helligkeit des Hintergrunds entschieden
  werden. Bei mittleren Tönen wie einem Blau liegt die Schwelle falsch, und man
  läuft gegen Weiß, obwohl Schwarz besser trägt. Beide Enden messen, das
  bessere nehmen.
- Die Betonungsfarbe muss festgezurrt sein, **bevor** die Schrift darauf
  geprüft wird — sonst prüft man gegen einen Wert, der sich danach noch ändert.

Solarized behält seine Kennfarben, aber die Schrift ist `base1` statt `base0`:
`base0` schafft die 4.5:1 gegen `base03` nicht. Solarized wurde für
Terminalkontrast entworfen, nicht für WCAG.

Wand- und Hintergrundfarbe der 3D-Ansicht bleiben wie im Original frei wählbar
(Menü Bearbeiten), aber jedes Thema bringt eine passende Voreinstellung mit —
sonst leuchtet Ravens grüner Testraum im dunklen Programm wie eine Taschenlampe.
Wer ihn will, nimmt „Raven Klassik": dort steht exakt das Grün aus den
Bildschirmfotos.

## Zwei Grafikschnittstellen statt einer

Direct3D 11 als Voreinstellung, OpenGL 3.3 als gleichwertige Alternative, beide
fest eingebaut.

Der Grund ist nicht Geschmack. Direct3D ist nicht immer verfügbar: über
Remotedesktop, in virtuellen Maschinen, unter Wine und auf sehr alter Hardware
schlägt `D3D11CreateDevice` fehl. Bei g2c hat ein Rechner die Arbeit eines
Abends gekostet, und dort war es nur eine Schriftart. Eine Grafikschnittstelle
ist der größere Einzelpunkt, an dem ein Programm bei jemand anderem nicht
startet. Der alte EffectsEd war ohnehin OpenGL, und wer ihn unter Wine
betreibt, soll das weiter können.

**Wechsel zur Laufzeit heißt Fensterneustart.** Dear ImGui erlaubt es nicht,
den Renderer im laufenden Betrieb auszutauschen — Schriftartentexturen,
Zeichenpuffer und bei mehreren Ansichtsfenstern die Fensterverwaltung hängen
daran (`ocornut/imgui#4616` beschreibt genau diesen Versuch und die Ausnahmen,
die er auslöst). Der verlässliche Weg ist, die Oberflächenschicht abzubauen und
neu aufzubauen.

Das ist hier unproblematisch, weil der gesamte Zustand — geöffnete Datei,
Änderungsverlauf, Auswahl, Einstellungen — im Kern liegt und nichts davon den
Renderer kennt. Der Wechsel ist ein Neustart des Fensters, nicht des Programms.

**Der Rückfall wird gemeldet, nicht verschwiegen.** Wer Direct3D eingestellt hat
und OpenGL bekommt, liest den Grund im Klartext:

```
Direct3D 11 steht nicht zur Verfügung (D3D11CreateDevice lieferte
DXGI_ERROR_UNSUPPORTED), es wird OpenGL 3.3 benutzt.
```

Läuft keine von beiden, nennt die Fehlermeldung beide Gründe. Sonst rät der
Nutzer, und wir sind wieder bei „bei mir startet es nicht".

Die Auswahllogik in `src/renderer.cpp` kommt ohne Grafikheader aus und ist
deshalb hier geprüft — das Anlegen des Geräts kann niemand ohne Grafikkarte
testen, aber die Entscheidung darüber, was bei welchem Ausgang passiert, sehr
wohl. Genau dort sitzen die Fehler.

## Vier Sprachen, Englisch zuerst

English, Deutsch, 中文, 日本語 — dieselben vier wie bei g2c, aber **Englisch ist
jetzt die Hauptsprache**. Das ist die Sprache der JKA-Modding-Gemeinde, der
Dateiformate und der Fehlermeldungen der Engine.

136 Texte × 4 Sprachen = 544 Einträge, erzeugt von `tools/gen_i18n.py`. Von Hand
wäre die Gefahr zu groß, dass eine Spalte verrutscht. Ein `static_assert`
vergleicht Tabellenlänge und Aufzählung; geraten beide aus dem Tritt, fällt es
beim Übersetzen auf und nicht als leerer Knopf im laufenden Programm.

Zwei Tests sichern die Tabelle ab: kein Eintrag darf leer sein, und kein
chinesischer oder japanischer Text darf wortgleich mit dem englischen sein.
Letzteres wäre eine vergessene Übersetzung — mit einer Ausnahmeliste für
Eigennamen und Kürzel wie `RGB`, `FxRunner` oder `EffectsEd`.

Beim ersten Start wird die Sprache aus den Systemeinstellungen abgeleitet.
Eine Feinheit dabei: **traditionelles Chinesisch bekommt Englisch.** `zh-Hant`,
`zh-TW`, `zh-HK` und `zh-MO` sind nicht dasselbe wie `zh-Hans`, und wir haben
nur Vereinfachtes. Falsche Zeichen sind schlechter als eine Fremdsprache.

Für CJK gilt dieselbe Lehre wie bei den Symbolschriften: die Schriften werden
**mitgeliefert**, nicht aus `C:\Windows\Fonts` erwartet. `msyh.ttc` und
`YuGothM.ttc` sind auf einer frisch aufgesetzten Windows-Installation nicht
zwangsläufig da. `LanguageInfo::needsCjkFont` sagt, wann eine zweite Schrift
per `MergeMode` dazugelegt werden muss — Latein bleibt dabei in der
Hauptschrift und damit scharf.

## Einstellungen liegen unter %APPDATA%

Genau wie bei g2c, aus denselben Gründen:

**Nicht neben die Exe.** Wer das Programm nach `C:\Program Files` legt, hat dort
keine Schreibrechte, und Windows leitet Schreibzugriffe seit Vista
stillschweigend in einen benutzerabhängigen Ordner um (VirtualStore). Die
Einstellungen sind damit mal da und mal weg.

**Auch nicht unter „Dokumente".** Der Ordner gehört dem Nutzer für eigene
Dateien. Spielstände gehören dorthin, weil man die sichern und weitergeben
will — Fensterpositionen und Häkchen nicht.

Richtig ist `%APPDATA%\efxed\`:

```
%APPDATA%\efxed\efxed_settings.txt    Thema, Sprache, Renderer, Teiler, Schalter
%APPDATA%\efxed\efxed_gui.ini         Fensterzustand von Dear ImGui
%APPDATA%\efxed\efxed_start.log       Startprotokoll
```

Alle drei im **selben** Ordner. Das ist kein Detail: liegen sie an drei Orten,
findet man im Fehlerfall nichts. Das Startprotokoll war bei g2c die einzige
brauchbare Spur, als das Programm auf einem fremden Rechner beim Start abstürzte.

**Mitnehmbarer Betrieb** über eine leere Datei `efxed_portable.txt` neben der
Exe — für Stick, Netzlaufwerk oder mehrere Zweige nebeneinander. Dann liegt
alles daneben.

**Einmalige Übernahme:** Findet sich beim ersten Start eine Einstellungsdatei
neben der Exe, wird sie in den neuen Ordner kopiert. Niemand verliert beim
Aktualisieren seine Einstellungen.

Der Ordner wird über den Pfad der **Exe** bestimmt, nicht über das
Arbeitsverzeichnis. Wer das Programm über eine Verknüpfung startet oder eine
Datei darauf zieht, hat ein ganz anderes Arbeitsverzeichnis — die
Markierungsdatei läge dann am falschen Ort.

## build.bat

Wird von `tools/gen_build_bat.py` **erzeugt**, nicht getippt. Zwei Gründe, beide
aus früheren Projekten:

**CRLF ist Pflicht.** Bei g2c hatte `build.bat` Unix-Zeilenenden, und `cmd.exe`
hat sie stillschweigend falsch ausgeführt — kein Fehler, nur unerklärliches
Verhalten. Das Erzeugungsskript prüft: 150 Zeilen, 150 CRLF, 0 nackte LF.

**Keine Umlaute.** `cmd.exe` läuft in Codepage 850 oder 437; UTF-8-Umlaute
erscheinen dort als Zeichensalat. Die Datei enthält 0 Bytes über 127.

Drei weitere Punkte:

**`call :main` als Rahmen.** Springt etwas vorzeitig zu `:eof`, landet man
hinter dem `call` und nicht im Nichts — das Fenster bleibt offen und man kann
die Meldung lesen. Ohne diesen Umweg schließt sich das Fenster bei einem Fehler
sofort, und man sieht genau gar nichts.

**Kein `-G` beim CMake-Aufruf.** In einem früheren Projekt stand
`"Visual Studio 17 2022"` fest verdrahtet, und auf einem Rechner mit
**Visual Studio 2026** schlug jeder einzelne Aufruf mit „could not find any
instance of Visual Studio" fehl. CMake findet die neueste installierte Fassung
selbst. `vswhere` wird nur benutzt, um den Namen anzuzeigen und um vorab zu
melden, wenn der Baustein „Desktopentwicklung mit C++" fehlt.

**Tests laufen am Ende automatisch** und der Rückgabewert wird durchgereicht.
Schlägt eine Prüfung fehl, liegen die Programme trotzdem in `out\` — aber die
Meldung sagt deutlich, dass man sie nicht für wichtige Dateien benutzen soll,
bevor die Ursache geklärt ist.

Aufrufe:

```
build.bat              einrichten, uebersetzen, testen
build.bat nurbauen     ohne Tests
build.bat sauber       build\ und out\ entfernen
build.bat /nopause     ohne "Beliebige Taste druecken" (fuer Skripte)
```

## Ablaufprotokoll

Wenn das Programm auf einem fremden Rechner beim Start abstürzt, gibt es keinen
Debugger, keinen Stapelabzug und niemanden, der sagen kann, was er getan hat.
Es gibt eine Datei. **Die letzte Zeile darin muss den Schritt benennen, der
gerade lief.**

Bei g2c hat genau das den Absturz gefunden — aber erst im zweiten Anlauf. Das
Protokoll endete bei „Schriften laden", und weil zwischen dieser Marke und der
nächsten *drei* Dinge lagen (`buildFonts`, `ImGui_ImplWin32_Init`,
`ImGui_ImplDX11_Init`), war die Stelle noch nicht bestimmt. Erst feinere Marken
haben es auf das Laden der Symbolschrift eingegrenzt.

Vier Regeln, die dieses Protokoll durchsetzt:

**1. Nach jeder Zeile wird geleert.** `std::FILE` mit `fflush`, nicht
`std::ofstream` — bei letzterem steht zwischen unserem Schreiben und der Datei
noch ein Puffer, dessen Verhalten beim harten Programmende nicht zugesichert
ist. Ohne das stünde ausgerechnet die letzte Zeile noch im Puffer.

**2. Marken sind fein.** Lieber eine zu viel als eine, die drei Schritte
zusammenfasst.

**3. Ein Schritt vermerkt seinen Beginn, bevor er anfängt.** Ein Schritt ohne
Abschlusszeile ist der Schuldige. `Step` ist bewusst ein Objekt und kein
Funktionspaar: ein vorzeitiges `return` oder eine Ausnahme darf die
Abschlusszeile nicht verschlucken, sonst sähe ein übersprungener Schritt aus
wie ein abgestürzter.

**4. Das vorige Protokoll wird beiseitegelegt, nicht überschrieben.** Nach einem
Absturz startet jeder das Programm noch einmal — und würde damit den Beweis
löschen. Die alte Datei heißt danach `efxed_start.log.vorher`.

Geprüft wird das nicht durch Hinsehen, sondern hart: ein Kindprozess läuft in
einen Schritt hinein und ruft mitten darin `abort()` — kein Abflauf, keine
Destruktoren, keine Pufferleerung, genau wie bei einem echten Absturz. Der Test
verlangt danach, dass die letzte Zeile der Datei `> Symbolschrift laden` lautet,
keine Abschlussmarke trägt, die vorherigen Schritte als abgeschlossen erkennbar
sind und `Ordentlich beendet` **fehlt**.

Das Protokoll ist über `efxtool log` einsehbar, auch wenn die Oberfläche gar
nicht mehr startet — der Fall, in dem man es braucht. Es zeigt beide Dateien,
die aktuelle und die vorige.

Beispiel:

```
[     3 ms]          > Grafikschnittstelle anlegen
[     3 ms]            > Direct3D 11 pruefen
[    11 ms] FEHLER     < Direct3D 11 pruefen (8 ms) -- D3D11CreateDevice
                         lieferte DXGI_ERROR_UNSUPPORTED
[    11 ms]            > OpenGL 3.3 pruefen
[    23 ms]            < OpenGL 3.3 pruefen (12 ms)
[    23 ms] WARNUNG    Rueckfall auf OpenGL 3.3
[    23 ms]          < Grafikschnittstelle anlegen (20 ms)
```

Die Zeitangabe ist die Zeit seit Programmstart, nicht die Uhrzeit. Beim Suchen
nach einem Absturz will man wissen, wie lange etwas gedauert hat — bei einer
nach Sekunden abstürzenden Anwendung sagt eine Uhrzeit nichts. Nebeneffekt: man
sieht sofort, welcher Schritt den Start langsam macht.

## Multithreading: wann, wo und warum nicht überall

Recherchiert statt geraten. Drei Quellen bestimmen den Plan:

**1. Dear ImGui ist nicht threadsicher** (`ocornut/imgui#221`, bestätigt in
`#7234`). Ein Kontext gehört einem Faden. Wer es mit einem Mutex versucht,
bekommt Fenster, die mal erscheinen und mal nicht, oder Inhalte doppelt
gezeichnet — ein Arbeitsfaden weiß nicht, ob gerade ein Bild aufgebaut wird.
`#6895` zeigt denselben Fehler bei Eingabeereignissen aus einem Fremdfaden.

→ **Kein einziger `ImGui::`-Aufruf aus einem Arbeitsfaden.** Ergebnisse gehen
über `postToMain()` in eine Schlange, die der Hauptfaden einmal je Bild leert.

**2. Direct3D 11 trennt Erzeugen und Zeichnen.** Alle `ID3D11Device`-Methoden
sind *free-threaded*, `ID3D11DeviceContext` ist es **nicht**. Texturen dürfen
also aus Arbeitsfäden erzeugt werden, gezeichnet wird nur auf einem.

Dabei muss `D3D11_FEATURE_DATA_THREADING.DriverConcurrentCreates` abgefragt
werden: ist es `FALSE`, verhindert grobe Synchronisierung im Treiber jede
Nebenläufigkeit, und das Verteilen kostet dann nur. Bei OpenGL entspricht dem
ein geteilter Kontext — der alte EffectsEd importiert `wglShareLists`, hat das
also schon gekannt.

**3. Microsoft warnt ausdrücklich vor dem naheliegenden Fehler:** *„creating and
loading a texture is typically limited by memory bandwidth. Attempting to create
and load multiple textures might be no faster than doing one texture at a time,
even if this leaves multiple CPU cores idle."*

Das ist der wichtigste Satz der ganzen Recherche und er ändert den Plan. Ich
hatte „Texturen laden" als Kandidaten für Parallelisierung genannt — falsch.
Zu trennen sind:

| | |
|---|---|
| **Dekodieren** (TGA, JPG, PNG, PCX, BMP → RGBA) | reine Rechenlast, verteilt sich gut |
| **Hochladen** zur Grafikkarte | speicherbandbreitenbegrenzt, verteilt sich **nicht** |

Also: dekodieren auf allen Kernen, hochladen der Reihe nach.

### Wo Fäden eingesetzt werden

| Stelle | Verteilt? | Begründung |
|---|---|---|
| `.shader`-Dateien einlesen | ja | rein Rechen- und Lesearbeit, 30+ Dateien |
| `.efx`-Ordner durchsuchen | ja | hunderte kleine Dateien |
| Bilder dekodieren | ja | rechenlastig, gut teilbar |
| Texturen hochladen | nein | Speicherbandbreite, nicht CPU |
| Partikel simulieren | ja, ab ~500 | darunter kostet das Verteilen mehr |
| Zeichnen | nein | ein Kontext, ein Faden |
| Alles mit `ImGui::` | nein | siehe oben |

### Warum es einen eigenen Pool gibt

Ein Arbeitsfaden weniger als Kerne — der Hauptfaden zeichnet und darf nicht um
Kerne streiten, sonst ruckelt das Fenster genau dann, wenn etwas passiert. Bei
einem Kern bleiben null Arbeitsfäden, und `parallelFor` wird zur gewöhnlichen
Schleife; ein Arbeitsfaden auf einem Kern macht nichts schneller, kostet aber
Umschaltungen.

**Abbruch** ist keine Bequemlichkeit: wer den Grundpfad wechselt, während der
alte durchsucht wird, soll nicht warten müssen — und das Ergebnis des alten
Laufs darf die neue Liste nicht überschreiben.

**`drainMainQueue(maxMillis)`** begrenzt die Zeit. Fallen tausend Texturen
gleichzeitig an, kommen sie über mehrere Bilder verteilt an, statt in einem
Ruck, der als Hänger sichtbar wird.

### Zwei Dinge, die die Tests korrigiert haben

Der erste Versuch prüfte auf „mindestens zwei Fäden gleichzeitig aktiv". Auf
einer Maschine mit einem physischen Kern laufen vier Fäden **nie** im selben
Augenblick — der Test wäre dort rot gewesen, obwohl alles richtig ist.

Der zweite Versuch prüfte, ob mehrere Faden-Kennungen Arbeit bekommen — und
lieferte **eine**. Auch das war kein Fehler: bei reiner Rechenarbeit holt sich
der aufrufende Faden alle Portionen, bevor die Arbeiter aus dem Warten
aufwachen. Genau so soll es sein, weil Aufwecken teurer ist als eine kurze
Portion.

Verteilung zeigt sich erst bei Portionen, die lange genug dauern — also bei
genau der Arbeit, um die es geht: eine Datei lesen, ein Bild dekodieren. Mit
4 ms je Portion sind es dann 5 von 5 möglichen Fäden.

Geprüft mit dem Thread-Sanitizer: keine Datenrennen.

## Was der erste Windows-Lauf gefunden hat

Vier Punkte, alle in den Tests oder im Bauskript, keiner im eigentlichen Code.
Das ist der Grund, warum es einen ersten Lauf auf der Zielplattform braucht,
bevor die Oberfläche dazukommt.

**1. Der Absturztest lief unter Windows gar nicht.** Er stand in einem
`#ifndef _WIN32` (fork + abort im Kind). Damit war ausgerechnet auf der
Zielplattform der wichtigste Test des Protokolls nie gelaufen — genau der, der
belegt, dass die letzte Zeile einen Absturz überlebt. Jetzt startet sich das
Testprogramm für diesen Zweck selbst neu (`--crash-child`), das geht auf beiden
Systemen gleich.

**2. Der APPDATA-Test war rot, obwohl der Code richtig ist.** Auch hier ein
`#ifndef _WIN32`, diesmal um `setenv`. Unter Windows galt deshalb die *echte*
Umgebungsvariable, und der Test verglich sie mit einem Wegwerfordner.
`C:\Users\...\AppData\Roaming\efxed` ist genau, was herauskommen soll. Jetzt
`_putenv_s` unter Windows.

**3. Ein Test war unter Windows unbrauchbar langsam.** Die Zeitbegrenzung von
`drainMainQueue` wurde mit 2000 Aufgaben à `sleep_for(50 us)` geprüft. Der
Windows-Zeitgeber hat rund 15 ms Auflösung — ein `sleep_for(50 us)` schläft
dort ein Vielfaches davon. Der Testlauf blieb sichtbar hängen. Jetzt eine
Warteschleife auf `steady_clock` statt `sleep_for`, und 400 statt 2000
Aufgaben.

**4. `-DCMAKE_BUILD_TYPE=Release` war überflüssig.** Der Visual-Studio-Generator
kennt mehrere Konfigurationen und wählt sie erst beim Übersetzen; CMake gab die
Variable mit *„Manually-specified variables were not used"* zurück. Die
Konfiguration steht ohnehin beim `--build --config Release`.

Dazu eine Vorbeugung: **`-Wshadow` ist bei GCC jetzt dauerhaft an.** MSVC hat
mit `C4456` eine verdeckte Variable in den Tests gemeldet, die GCC mit
`-Wall -Wextra` durchgehen lässt. Ohne diese Option fällt so etwas erst beim
Bauen auf dem Zielrechner auf.

## Der erste Startversuch: Anbindung vor Kontext

Das Fenster kam nicht hoch. Das Startprotokoll endete hier:

```
[      4 ms]            > Grafikschnittstelle anlegen
[      4 ms]              > pruefen: Direct3D 11
[     31 ms]                DriverConcurrentCreates: ja
```

Danach nichts. Kein `<`, kein weiterer Schritt — genau die Signatur, für die
das Protokoll gebaut wurde: der laufende Schritt ist der Schuldige, und die
Zeile davor sagt, wie weit er gekommen war.

Die Ursache stand zwischen dieser Zeile und dem Ende von `create()`:

```cpp
createBackBufferView();
ImGui_ImplDX11_Init(device_.Get(), context_.Get());   // <- hier
```

`ImGui_ImplDX11_Init` greift auf den ImGui-Kontext zu. Der entsteht aber erst
mit `ImGui::CreateContext()` — und das passiert *nach* dem Anlegen der
Grafikschnittstelle, weil die Schriftgröße an der Bildschirmskalierung hängt und
die erst bekannt ist, wenn das Fenster steht. Nullzeiger, wortloser Tod.

Derselbe Fehler noch einmal beim Beenden, nur umgekehrt:

```cpp
ImGui_ImplWin32_Shutdown();
ImGui::DestroyContext();
renderer.reset();          // Destruktor rief ImGui_ImplDX11_Shutdown auf —
                           // nach der Zerstörung des Kontexts
```

**Die Lösung:** Anbindung vom Anlegen trennen. `initImGuiBackend()` und
`shutdownImGuiBackend()` sind jetzt Teil der Renderer-Schnittstelle, und die
Reihenfolge steht dort im Klartext:

```
Start:  Renderer anlegen -> CreateContext -> Schriften -> initImGuiBackend
Ende:   shutdownImGuiBackend -> DestroyContext -> Renderer freigeben
```

Beide Umsetzungen prüfen zusätzlich `ImGui::GetCurrentContext()` und melden
einen fehlenden Kontext ins Protokoll, statt abzustürzen. Aus einem stummen Tod
wird eine lesbare Zeile.

Dazu feinere Marken im Anlegen des Geräts — zwischen „DriverConcurrentCreates"
und dem Ende lagen vier Anweisungen, und das war eine zu viel für eine
eindeutige Aussage. Dieselbe Lehre wie bei g2c, nur eine Ebene tiefer.

## Zwei Fehler aus dem ersten laufenden Fenster

### Das Sprachmenü zeigte `??` und `???`

Genau so viele Fragezeichen wie Zeichen: 中文 hat zwei, 日本語 hat drei. Die
Glyphen fehlten im Zeichensatz.

Die Ursache: eine CJK-Schrift wurde nur geladen, wenn beim **Start** eine
CJK-Sprache eingestellt war. Bei deutscher Oberfläche also nie — und damit
konnte man die Sprache nicht auswählen, weil man nicht lesen konnte, welche es
ist. Derselbe Fehler wie bei g2c.

Jetzt zwei Stufen über `ImFontGlyphRangesBuilder`:

- **immer** die Zeichen der Sprachnamen selbst, damit das Menü in jeder Sprache
  lesbar ist. Das sind fünf Glyphen, und sie werden aus der Sprachtabelle
  gelesen, nicht fest verdrahtet — kommt eine Sprache dazu, ist ihr Name
  automatisch dabei.
- **bei Bedarf** der volle Bereich, wenn die Oberfläche selbst auf Chinesisch
  oder Japanisch steht. Dann auch der richtige: `GetGlyphRangesJapanese` für
  Japanisch, nicht der chinesische Bereich.

Beim Sprachwechsel wird der Zeichensatz neu gebaut — zwischen zwei Bildern, nie
mitten in einem, und mit `shutdownImGuiBackend` / `initImGuiBackend` darum
herum, weil die Schrifttextur an der Anbindung hängt.

Die Schriftsuche geht jetzt über fünf Kandidaten (`msyh`, `meiryo`, `YuGothM`,
`msgothic`, `simsun`), damit auch eine schlanke Windows-Installation eine
findet. Fehlt jede, steht das im Protokoll statt in Kästchen auf dem Bildschirm.

### Nicht alles war übersetzt

Auf die Frage konnte ich nicht antworten, weil ich es nie geprüft hatte. Also
messen statt behaupten: `tools/lint_i18n.py` sucht Textargumente von
Oberflächenfunktionen, die nicht durch `tr()` laufen.

Der erste Lauf fand **zwölf**. Darunter drei Fensterüberschriften — und die
waren nicht nur unübersetzt, sondern hätten beim Sprachwechsel einen zweiten
Fehler ausgelöst: ImGui leitet die Kennung eines Fensters aus seinem Namen ab.
Übersetzt man den Titel, gilt das Fenster als ein anderes und verliert Größe,
Lage und Bildlauf. Die Schreibweise `Sichtbarer Titel###feste_kennung` trennt
beides.

Die Prüfung läuft jetzt in `build.bat` mit. Ist kein Python da, wird sie
übersprungen statt den Bau abzubrechen.

### Das Protokoll ist jetzt englisch

Es war deutsch, was bei englischer Hauptsprache schon in sich falsch war. Die
Entscheidung, es **nicht** zu übersetzen, ist bewusst:

Ein Protokoll wird weitergegeben — in ein Forum, auf Discord, an jemanden, der
helfen soll. Eine japanische Fehlermeldung nützt dort niemandem, und ein
Protokoll, dessen Wortlaut von den Einstellungen des Absenders abhängt, lässt
sich nicht durchsuchen. Englisch ist im JKA-Umfeld die Sprache, die alle lesen.

Die **Oberfläche** ist vollständig übersetzt, das Protokoll bewusst nicht.

### Windows meldet sich falsch

`System: Windows 10 Home Build 26200` — Build 26200 ist Windows 11.
`ProductName` in der Registrierung blieb seit Windows 11 auf „Windows 10"
stehen, Microsoft hat den Wert nie umgestellt. Die Buildnummer stimmt dagegen:
ab 22000 ist es Windows 11. Wird jetzt korrigiert, damit später niemand eine
Fehlermeldung auf die falsche Fassung schiebt.

## Bestandsaufnahme statt Gedächtnis

Auf die Frage „was fehlt sonst noch" habe ich nicht überlegt, sondern das
Original ausgelesen: `RT_MENU 128` und `RT_DIALOG` aus `EffectsEd.exe`
dekodiert. Damit ist die Liste in `FEHLENDES.md` vollständig — es gibt dort
nichts, was nicht drinsteht.

Drei Dinge, die ich nur so gefunden habe:

- **Orient Down.** Wir hatten eine Zwei-Zustands-Ankreuzung „Nach oben
  ausrichten". Das Original hat *drei* Ausrichtungen. Jetzt ein Auswahlfeld.
- **Draw Textured Room** mit vier Untereinträgen (ohne, Ziegel, Erde, Putz).
  Die Bilder liegen als `brick.jpg`, `dirt.jpg`, `stucco.jpg` im Original.
- **Effect Render Options ▸ Overdraw.** Färbt Flächen nach der Anzahl der
  Überzeichnungen. Genau das Werkzeug, mit dem man sieht, warum ein Effekt im
  Spiel Bilder kostet.

## Spielpfad-Dialog

Nachbau von Dialog 170, mit zwei Abweichungen.

**Weggelassen: der SourceSafe-Kasten.** INI-Pfad
`\\ravendata1\vss\central_assets\SRCSAFE.INI`, Projektpfad `$/jedi/base/`,
Ein/Aus. Das war Ravens interne Versionsverwaltung. Der Server steht seit
zwanzig Jahren nicht mehr, Visual SourceSafe ist eingestellt. Ein Feld, das
niemand ausfüllen kann, ist keine Funktion.

**Dazugekommen: eine Prüfung.** Der alte Dialog nahm jeden Pfad wortlos an.
Dass er falsch war, merkte man erst, wenn Shader fehlten — und dann sucht man
den Fehler beim Effekt statt beim Pfad. `gamepath::inspect()` sieht sich den
Ordner an, während getippt wird, und erkennt einen `base`-Ordner an mindestens
einem von dreien: `.pk3`-Dateien darin, ein `shaders`-Ordner mit
`.shader`-Dateien, oder ein `effects`-Ordner mit `.efx`-Dateien. Gefunden wird
beides — ausgepacktes und unausgepacktes Spiel.

Der häufigste Tippfehler bekommt einen eigenen Hinweis: wer `GameData` statt
`GameData/base` wählt, liest *„Es gibt hier einen Unterordner base — vermutlich
ist der gemeint."*

Die Vorlagen ersetzen Ravens „Default options for JK2 / JA / SOF2", die
`w:/game/base/` eintrugen — ein Netzlaufwerk im Studio. Jetzt stehen dort die
Pfade, an denen Steam, GOG und Movie Duels heute liegen, und eine Vorlage ist
ausgegraut, wenn es den Ordner auf diesem Rechner nicht gibt. Eine Schaltfläche,
die einen toten Pfad einträgt, hilft niemandem.

Die Prüflogik steht in `src/gamepath.cpp` und damit außerhalb der Oberfläche —
14 Prüfungen decken sie ab, unter anderem beide Spielformen, Endungen in
Großbuchstaben und die vier Fälle beim Vereinheitlichen der Schrägstriche.

## Die 3D-Ansicht

Aufgeteilt in einen prüfbaren und einen unprüfbaren Teil — dieselbe Trennung
wie überall sonst, nur hier besonders wichtig, weil man einen Renderfehler
schlecht durch Hinsehen findet.

**Prüfbar (`src/camera.cpp`, `src/scene.cpp`):** Kameramathematik, Erschütterung
und die gesamte Geometrie. 104 Prüfungen decken das ab.

**Unprüfbar (`gui/renderer_*.cpp`):** Shader, Puffer, Bildpuffer. Beide
Schnittstellen bekommen dieselben Matrizen und dieselben Eckpunkte.

### Kamera

Bedienung wie in Ravens Anleitung: linke Taste dreht, rechte fährt heran,
Alt+links schiebt, Z+links rollt. Eine Umlaufkamera, weil man einen Effekt
betrachtet, der an einer Stelle steht, statt durch eine Welt zu laufen.

Drei Dinge, die die Tests erzwungen haben:

- **Der Abstand darf sich beim Drehen nicht ändern.** Das ist die Eigenschaft,
  die eine Umlaufkamera ausmacht; 40 Drehungen später stimmt sie auf 0.01
  Einheiten.
- **Nicken wird bei ±89° begrenzt.** Genau am Pol ist die Aufwärtsrichtung
  unbestimmt und das Bild kippt schlagartig. Geprüft wird auf brauchbare Zahlen
  nach 200 Umdrehungen in beide Richtungen.
- **Heranfahren ist verhältnismäßig, nicht in festen Schritten.** Aus der Ferne
  will man große Sprünge, aus der Nähe kleine. Ein fester Betrag macht das eine
  zäh und das andere unbrauchbar. Begrenzt auf 0.25 bis 400 Fuß, damit der
  Abstand nie null oder unendlich wird.

Die nahe Ebene richtet sich am Weltmaßstab aus. Eine zu kleine verschenkt
Tiefengenauigkeit und lässt weit entfernte Flächen flackern.

### Kamerawackeln — der Fehler, den du gemeldet hast

Jetzt eingebaut, Zeile für Zeile aus `cg_camera.cpp` (SP) und `cg_view.c` (MP);
beide rechnen identisch. Gemessen statt behauptet:

```
Staerke 10, Radius 300: im Zentrum 10.00, bei 150 Einheiten 5.00
Staerke 40 angefordert -> 16.00 (Deckel 16)
```

Die Stärke fällt linear mit dem Abstand, außerhalb des Radius passiert gar
nichts, und `MAX_SHAKE_INTENSITY` deckelt bei 16. Kein Rollen — die Engine
lässt es ausdrücklich aus, und der Test prüft das.

Eigener Zufallsgeber statt `std::rand`: der ist zwischen Laufzeitbibliotheken
verschieden, und ein Test soll auf jedem System dasselbe ergeben.

### Testraum

32 × 48 × 20 Fuß, bei 16 Einheiten je Fuß also 512 × 768 × 320 Einheiten. Das
Gitter hat **genau einen Fuß je Quadrat** — so steht es in Ravens Anleitung, und
der Test misst nach statt es auf dem Bildschirm abzuschätzen. Jede zehnte Linie
ist kräftiger, sonst zählt man beim Abschätzen einer Entfernung Linien.

Boden, Wände und Decke bekommen verschiedene Helligkeiten. Ohne diesen
Unterschied verschwimmen die Kanten und man verliert jedes Gefühl für den Raum;
im Original macht das die Beleuchtung, hier stecken die Werte in den Eckpunkten.

Das Gitter liegt eine Hundertstel Fuß über dem Boden. Läge es genau darauf,
streiten beide um dieselben Tiefenwerte und es flackert.

### Ein Fallstrick, den ich vorbeugend abgesichert habe

`scene::Vertex` wird **ohne Kopie** an den Renderer gereicht. Weicht das Layout
von `render::Vertex` ab, zeichnet er Müll — und das sieht man nicht als Absturz,
sondern als seltsame Farben, die man dann in der Effektdatei sucht. Vier
`static_assert` in `scene.cpp` halten beide Strukturen zusammen; weichen sie ab,
fällt es beim Übersetzen auf.

### Warum keine Rückseitenaussortierung

Der Raum wird von innen betrachtet, und Effekte sind flache Vierecke, die man
von beiden Seiten sieht. Mit Aussortierung bliebe entweder der Raum leer oder
die Hälfte der Partikel unsichtbar.

### OpenGL: Funktionen von Hand laden

Die Windows-Header bringen nur OpenGL 1.1 mit; alles darüber muss zur Laufzeit
über `wglGetProcAddress` geholt werden. Bewusst ohne GLEW oder glad — es sind
genau 36 Funktionen, und eine Fremdbibliothek dafür wäre mehr Abhängigkeit als
Nutzen.

Jede wird einzeln geprüft. Fehlt eine, steht ihr Name im Protokoll und die
Auswahllogik fällt sauber auf Direct3D zurück, statt beim ersten Zeichnen in
einen Nullzeiger zu laufen.

## Ein Zeiger passt nicht in 32 Bit

Der Bau schlug fehl, dazu zwölf Warnungen. Alle dieselbe Ursache:

```cpp
using TextureId = unsigned int;   // 32 Bit
```

Gedacht war an OpenGL, das eine Nummer zurückgibt (`GLuint`, 32 Bit). Vergessen
war Direct3D, das dort einen **Zeiger** ablegt
(`ID3D11ShaderResourceView*`, unter x64 also 64 Bit).

Zwei Folgen:

- `reinterpret_cast<TextureId>(srv)` schneidet die oberen 32 Bit ab. MSVC meldet
  das als `C4311`/`C4302` — als **Warnung**. Eine Warnung unter zwölf anderen
  übersieht man, und dann zeichnet das Programm mit einer halben Adresse.
- `reinterpret_cast<ImTextureID>(texture)` scheiterte, weil `unsigned int` zu
  schmal für einen Zeiger ist. Das war der Fehler in `app.cpp:541` — und
  ausgerechnet der hat den Bau angehalten und damit die stille Verkürzung
  verhindert.

**Behoben:** `TextureId` ist jetzt `std::uintptr_t`, mit einem `static_assert`,
der das auch auf einer Plattform sicherstellt, an die heute niemand denkt.

**Vorbeugung:** `C4311`, `C4302`, `C4312` und `C4826` sind jetzt **Fehler**, nicht
Warnungen (`/we4311` und Geschwister). Zeigerverkürzung ist unter x64 nie
harmlos; wenn sie auftritt, soll der Bau anhalten und nicht ein Programm
entstehen, das gelegentlich falsche Texturen zeigt.

Dazu ein Helfer für ImGuis Texturtyp: `ImTextureID` ist je nach Fassung ein
Zeiger (bis 1.91) oder eine 64-Bit-Zahl (ab 1.92). Wir haben die Fassung
festgenagelt, aber eine Aktualisierung soll nicht an einer Typumwandlung
scheitern — `toImTexture()` lässt den Übersetzer entscheiden. Beide Zweige sind
nachgerechnet: ein Zeiger überlebt den Umweg unverändert, und als 64-Bit-Zahl
auch.

## `if constexpr` verwirft nur in Vorlagen

Der zweite Anlauf scheiterte an derselben Zeile — aus einem anderen Grund, und
der ist lehrreich.

**Erstens, die Tatsache:** ImGui hat `ImTextureID` in **v1.91.4** (08.10.2024)
von `void*` auf `ImU64` umgestellt. Grund laut Quelltext: der Typ muss auch auf
32-Bit-Systemen einen 64-Bit-Deskriptor aufnehmen können, etwa bei Direct3D 12
und Vulkan. Wir sind auf 1.91.5 festgenagelt, hier gilt also `ImU64` — eine
Zahl, kein Zeiger.

**Zweitens, mein Fehler:**

```cpp
inline ImTextureID toImTexture(render::TextureId id) {   // keine Vorlage!
    if constexpr (std::is_pointer_v<ImTextureID>) {
        return reinterpret_cast<ImTextureID>(id);        // wird trotzdem geprüft
    } else {
        return static_cast<ImTextureID>(id);
    }
}
```

`if constexpr` verwirft den nicht genommenen Zweig **nur innerhalb einer
Vorlage**. In einer gewöhnlichen Funktion werden beide Zweige vollständig
geprüft — und `reinterpret_cast` von `uintptr_t` nach `ImU64` ist nicht
erlaubt, weil beides Zahlen sind.

Behoben, indem die Hilfsfunktion eine Vorlage wurde.

**Und der eigentliche Fehler war mein Vorgehen.** Ich hatte die Umwandlung
nachgerechnet, bevor ich sie ausgeliefert habe — aber meine Nachrechnung
benutzte versehentlich eine *Vorlage*, während der ausgelieferte Code keine war.
Ich habe also etwas anderes geprüft, als ich gebaut habe, und das Ergebnis für
einen Beweis gehalten.

Deshalb gibt es jetzt `tests/imgui_compat.cpp`: ein eigenes Testprogramm, das
`ImTextureID` beide Male nachstellt — einmal als Zeiger, einmal als 64-Bit-Zahl
— und die Hilfsfunktion in *derselben* Form prüft, in der sie im Programm steht.
Es kommt ohne ImGui aus und läuft deshalb auch dort, wo die Oberfläche gar nicht
gebaut wird. `build.bat` führt es mit aus.

## Drei Fehler aus dem ersten laufenden Fenster mit 3D-Ansicht

### Die Matrix wurde umsonst transponiert

Das Bild zeigte ein kleines, verzerrtes Rechteck statt des Raums.

```cpp
mvp[row * 4 + col] = sum;  // beim Ablegen transponiert
```

**HLSL legt eine `float4x4` in einem Konstantenpuffer standardmäßig
spaltenweise ab.** Unsere Matrizen sind bereits spaltenweise, weil sie der
OpenGL-Form folgen — es ist also nichts umzurechnen.

Das Transponieren ist die übliche Korrektur, wenn man mit DirectXMath rechnet,
weil das zeilenweise arbeitet. Bei uns war sie falsch herum: aus P·V wurde die
transponierte Matrix, und die Geometrie erschien verzerrt und an der falschen
Stelle.

Beide Renderer rechnen jetzt zeichengleich — was in `renderer_d3d11.cpp` steht,
steht so auch in `renderer_gl3.cpp`.

### Der Übersetzungsprüfer sah nur die halbe Miete

Im englischen Menü standen die Themen als „Dunkel", „Mitternacht", „Raven
Klassik" — und im Auswahlfeld „16 Einheiten/Fuss (JKA, SOF2)".

Die Texte standen fest verdrahtet in `src/theme.cpp` und `src/layout.cpp`. Der
Prüfer sah nur `gui/` und meldete null Lücken. Ein Prüfer, der die Hälfte nicht
ansieht, ist schlimmer als keiner: er erzeugt Vertrauen, das er nicht deckt.

Behoben durch `i18n::Str` statt fester Zeichenketten, und der Prüfer sieht
jetzt auch `src/` an — dort mit einer eigenen Regel, weil es dort kein ImGui
gibt: Zuweisungen an Felder namens `name` oder `label`.

Nachgerechnet über drei Sprachen:

```
Language  : Dark | Midnight | Raven Classic | ...   16 units/foot (JKA, SOF2)
Sprache   : Dunkel | Mitternacht | Raven Klassik | ...  16 Einheiten/Fuß
言語      : ダーク | ミッドナイト | Raven クラシック | ...  16 単位/フィート
```

### Ein Test, der manchmal rot war

`benchmark()` prüft, ob der Beschleunigungsfaktor über der Fadenzahl liegt —
dann taugt der Messaufbau nichts. Die Toleranz waren fünf Prozent.

Bei **null** Arbeitsfäden ist die Obergrenze aber 1.00, serielle und verteilte
Messung sind dieselbe Arbeit, und der Faktor schwankt um 1.00. Ein Ausreißer
auf 1.06 ließ den Test rot werden, obwohl nichts kaputt war — einmal unter
sechs Läufen.

Auf 32 Kernen sind fünf Prozent reichlich, auf einem Kern zu wenig. Jetzt
zusätzlich ein fester Sockel von 0.35. Zwölf Läufe hintereinander grün.

Ein Test, der gelegentlich ohne Grund rot wird, ist schlimmer als gar keiner:
man gewöhnt sich an, ihn zu ignorieren.

### `where python` findet einen Platzhalter

```
Python wurde nicht gefunden; ohne Argumente ausfuehren, um aus dem
Microsoft Store zu installieren...
[WARNUNG] Es gibt nicht uebersetzte Oberflaechentexte.
```

Windows legt unter `%LOCALAPPDATA%\Microsoft\WindowsApps` eine `python.exe` ab,
die nur den Store öffnet. `where` findet sie, der Aufruf schlägt fehl — und die
Prüfung meldete fälschlich fehlende Übersetzungen.

Jetzt werden `py`, `python3` und `python` der Reihe nach probiert und jeweils
mit `--version` geprüft, ob wirklich ein Python antwortet.

Dazu: `gen_build_bat.py` **weigert sich jetzt zu schreiben**, wenn nackte
Zeilenumbrüche oder Nicht-ASCII-Zeichen entstehen würden. Beides ist schon
einmal durchgerutscht — bei diesem Durchgang hätte mir ein Gedankenstrich in
einem Kommentar wieder Zeichensalat in `cmd.exe` beschert.

## Chinesisch und Japanisch: der Bereichsvektor war schon tot

Beim Umschalten auf Chinesisch oder Japanisch wurde **jeder** Text zu `??`.

Die Ursache steht in `imgui.h` unter „Common pitfalls":

> If you pass a 'glyph_ranges' array to AddFont*** functions, you need to make
> sure that your array persist up until the atlas is build (when calling
> GetTexData*** or Build()). **We only copy the pointer, not the data.**

Mein Vektor stand in einem eigenen `{ }`-Block:

```cpp
{
    ImVector<ImWchar> ranges;          // wird hier zerstört
    builder.BuildRanges(&ranges);
    addFontIfPresent(io, path, size, ranges.Data, true);
}
io.Fonts->Build();                     // liest den Zeiger — auf freigegebenen Speicher
```

`ocornut/imgui#2052` beschreibt genau diesen Fall, inklusive des Kommentars
*„I must move the variable out of the brace"*, und schließt mit: *„This makes it
failed to add glyph **without any runtime error**."*

Kein Absturz, keine Meldung — nur Fragezeichen auf dem Bildschirm.

Behoben durch `static` statt Blockvariable: der Zeiger muss auch dann noch
gelten, wenn der Zeichensatz beim Sprachwechsel neu gebaut wird.

**Und weil der Fehler stumm war, gibt es jetzt eine Nachschau.** Nach
`Build()` wird für jedes Zeichen der Sprachnamen geprüft, ob die Glyphe
tatsächlich im Zeichensatz liegt (`FindGlyphNoFallback`). Fehlt eine, steht sie
mit Codepunkt im Protokoll:

```
ERROR  font atlas is missing 5 glyphs used by the language menu:
       U+4E2D U+6587 U+65E5 U+672C U+8A9E - those entries will show as boxes
```

Die UTF-8-Dekodierung dafür habe ich getrennt nachgerechnet: 中 = U+4E2D,
文 = U+6587, 日 = U+65E5, 本 = U+672C, 語 = U+8A9E. Stimmt.

## OpenGL stand auf dem Kopf

**OpenGL legt den Ursprung einer Textur unten links, Direct3D oben links.** Wer
in einen Bildpuffer zeichnet und das Ergebnis als Bild anzeigt, bekommt bei
OpenGL deshalb ein senkrecht gespiegeltes Bild.

Der Renderer sagt es jetzt selbst — `viewportTextureFlipped()` — und die
Oberfläche dreht die Texturkoordinaten. Bewusst so und nicht über eine
gespiegelte Projektionsmatrix: die würde zusätzlich die Umlaufrichtung der
Dreiecke umkehren und damit die Aussortierung durcheinanderbringen. Gedrehte
Texturkoordinaten kosten nichts.

Dass Direct3D richtig aussieht und OpenGL nicht, war der Hinweis: wären beide
falsch, läge es an Kamera oder Geometrie. Ist nur einer falsch, liegt es an dem,
worin sich die beiden unterscheiden.

## Der Raum blendet Wände aus — Rückseitenaussortierung

Im Original verschwindet beim Herauszoomen die nächstgelegene Wand, und man
schaut in den Kasten hinein. Das ist keine Sonderbehandlung, sondern schlicht
**Rückseitenaussortierung** an einem Raum, dessen Flächen nach innen zeigen:

- von innen: alle sechs Flächen zeigen einen zur Kamera → alle sichtbar
- von außen: die nächstgelegene zeigt weg → fällt weg → Blick hinein

Ich hatte sie ausdrücklich abgeschaltet, mit der Begründung, Effektpartikel
seien flache Vierecke, die man von beiden Seiten sieht. Das stimmt — aber es ist
eine Eigenschaft der Partikel, nicht des Raums. **Aussortierung gehört an den
Zeichenaufruf, nicht an den Renderer.** Jetzt `setCulling(Cull::BackFaces)` für
den Raum, `Cull::None` für Linien und später für Partikel.

### Meine Wickelrichtung war verkehrt herum

Im Quelltext stand:

```cpp
// Reihenfolge so, dass die Vorderseiten nach innen zeigen.
```

Die Nachrechnung ergab bei **allen sechs** Flächen „nach außen". Der Kommentar
war eine Behauptung, kein Beweis — und weil ohne Aussortierung beide Richtungen
gleich aussehen, wäre das nie aufgefallen.

Jetzt misst der Test es: für jedes der zwölf Dreiecke wird die Normale aus dem
Kreuzprodukt gebildet und geprüft, ob sie zur Raummitte zeigt. Beide Dreiecke
jeder Fläche, nicht nur das erste.

### Die beiden Schnittstellen sind sich uneins, was „vorne" heißt

Direct3D nennt im Uhrzeigersinn die Vorderseite, OpenGL gegen den
Uhrzeigersinn. Bei gleicher Geometrie und gleichen Matrizen wäre also in genau
einer der beiden die falsche Hälfte sichtbar — und man suchte den Fehler in der
Geometrie statt im Rasterzustand.

Angeglichen über `FrontCounterClockwise = TRUE` im Direct3D-Rasterzustand.
Gegen den Uhrzeigersinn ist jetzt in beiden die Vorderseite.

### Tiefenbereich: ich hatte den Schaden überschätzt

Beim Nachsehen fiel auf, dass die Projektion in beiden Schnittstellen dieselbe
war, obwohl **OpenGL die Tiefe auf −1 bis 1 abbildet und Direct3D auf 0 bis 1**.

Meine erste Fassung des Tests behauptete, Direct3D klippe damit die vordere
Hälfte der Szene weg. Die Zahlen sagen etwas anderes: die OpenGL-Tiefe wird
null bei 2·f·n/(f+n), hier **1.60 Einheiten** bei einer nahen Ebene von 0.80 —
gut ein halber Zoll vor der Kamera. Geklippt wird nur dieser schmale Streifen.

Der wirkliche Verlust ist Genauigkeit: die Hälfte des Tiefenpuffers bleibt
ungenutzt, also ein Bit weniger. Kein sichtbarer Fehler, aber ein messbarer.

`projectionMatrix(aspect, zeroToOneDepth)` liefert jetzt beide Formen, und der
Renderer sagt über `wantsZeroToOneDepth()`, welche er will. Vier Prüfungen
belegen die Lage von naher und ferner Ebene in beiden Fällen.

Der Test sagt jetzt, was passiert, nicht was ich vermutet hatte.

## Vier Dialoge, Drahtgitter und die Farbwähler

### Wiedergabe-Einstellungen (Dialog 188)

Der interessante Teil sind die zwei Felder „Repeat rate (seconds per spawn)"
und „Repeat frequency (spawns per second)". Sie meinen **denselben Wert** —
0.300 s und 3.333 je Sekunde sind dieselbe Einstellung.

Gespeichert wird nur die Rate; die Frequenz ist ihr Kehrwert und wird gerechnet.
Zwei Felder getrennt zu speichern ist die verlässlichste Art, sie auseinander
laufen zu lassen — und dann zeigt der Dialog etwas anderes an, als die Vorschau
tut. Der Test dreht zwanzigmal hin und her und prüft, dass sich nichts
verschiebt.

Dazu die Randfälle, an denen eine Kehrwertrechnung gern scheitert: Frequenz
null, negative Frequenz, Rate null, Rate eine Milliarde. Alle liefern einen
brauchbaren Wert statt unendlich.

„Total repetitions: n/a" ist kein Platzhalter, sondern eine Aussage: bei
einmaligem Abspielen und bei „bis zum Anhalten" gibt es keine Zahl. Bei „jedes
Bild neu" auch nicht, weil sie an der Bildrate hängt.

Der Rücksprung des bewegten Ursprungs („Reset location after") ist wichtiger,
als er aussieht: ohne ihn wandert der Ursprung bei längerer Wiedergabe aus dem
Raum, und man sucht den Effekt.

### Neues Segment (Dialog 157)

Dieselben dreizehn Typen, alphabetisch wie im Original, mit der Beschreibung
rechts neben der Liste — die ist das einzige, was einem beim Auswählen hilft.
Doppelklick übernimmt sofort, wie dort. Zusätzlich auf `Einf` gelegt, Löschen
auf `Entf`.

### Eigener Ursprung (Dialog 192)

Sechs Lagen wie im Original. Dazu eine Ergänzung: die **berechnete Lage wird
sofort angezeigt**. Der alte Dialog ließ einen raten, wo „an der Wand"
eigentlich liegt.

Alles außer „eigene Werte" wandert mit dem Weltmaßstab mit — sonst säße der
Ursprung bei 64 Einheiten je Fuß im Boden statt an der Decke. Der Test prüft
das.

### Drahtgitter

Braucht eine Füllart im Renderer. Bei Direct3D ist sie Teil des
Rasterzustands, bei OpenGL ein eigener Aufruf.

Zwei Fallstricke:

- Direct3D braucht damit **vier** Rasterzustände statt zwei: gefüllt und
  Drahtgitter, jeweils mit und ohne Aussortierung. Vorab angelegt — einen
  Rasterzustand je Bild zu erzeugen ist eine der Stellen, an denen
  unerklärliche Ruckler herkommen.
- Bei OpenGL gehört `glPolygonMode` dem Kontext, nicht uns. Ohne Zurücksetzen
  am Ende der Vorschau zeichnet **ImGui** seine Flächen als Drahtgitter.

### Wand- und Hintergrundfarbe

`ColorPicker3` statt des Windows-Farbdialogs — der ist unter Windows 11 immer
noch das Fenster von 1995 und kennt keine dunkle Darstellung.

Zwei Details: beim Öffnen wird mit der aktuell wirksamen Farbe vorbelegt, sonst
springt die Ansicht beim ersten Anfassen auf einen fremden Wert. Und es gibt
„Auf Thema zurücksetzen" — wer einmal eine Farbe gewählt hat, soll nicht raten
müssen, welche das Thema vorgesehen hatte.

## Die Eigenschaftenblätter

### Welcher Reiter zu welchem Typ gehört, steht im Spielcode

Ravens Anleitung sagt nur: „Generation und Origin/Size immer, der Rest je nach
Typ." **Welcher Rest**, steht dort nicht.

Also hergeleitet aus dem `switch (mType)` in `CreateEffect` und `PlayEffect`
(`code/cgame/FxScheduler.cpp`). Dort steht Zeile für Zeile, was die Engine je
Typ ausliest:

```cpp
case CameraShake:
    theFxHelper.CameraShake( org, fx->mElasticity.GetVal(),
                             fx->mRadius.GetVal(), fx->mLife.GetVal() );
case FxRunner:
    PlayEffect( fx->mPlayFxHandles.GetHandle(), org, ax );
case Light:
    FX_AddLight( org, mSizeStart, mSizeEnd, mSizeParm, sRGB, eRGB, mRGBParm,
                 mLife, mFlags );
```

Ein `CameraShake` liest **drei** Werte. Ein `Sound` liest die Klangliste und
ein Spawn-Flag. Ein `Light` liest keine Alphakurve, weil es über die Farbe
mischt.

Die Tabelle steht in `src/fields.cpp` und damit im Kern — 160 Prüfungen decken
sie ab, jede gegen eine Zeile im Spielcode. Ein falscher Eintrag hieße sonst,
dass jemand ein Feld ausfüllt, das im Spiel nichts tut.

### Und das meldet das Programm jetzt auch

Über der Reiterleiste steht in Warnfarbe, welche Felder gesetzt sind, obwohl
der Typ sie nicht ausliest:

```
Wird bei diesem Typ nicht ausgewertet: rgb velocity shaders
```

Das ist die häufigste Art, Zeit zu verlieren: man dreht an einer Zahl, die
niemand liest, und sucht den Fehler dann woanders.

### Spannen sichtbar statt zwei Spalten

Fast jedes Feld einer .efx-Datei ist entweder ein fester Wert oder eine Spanne,
aus der beim Abspielen gewürfelt wird. Ravens Blätter haben dafür zwei Spalten
„Min" und „Max" — auch dort, wo nur eine Zahl steht.

Hier stattdessen ein Häkchen `~` je Feld: solange es aus ist, gibt es eine
Zahl. Damit sieht man auf einen Blick, welche Felder würfeln, und das ist beim
Lesen einer fremden Effektdatei die erste Frage.

Ein Feld ohne Häkchen wird **gar nicht geschrieben**. Damit bleibt eine Datei,
in der es fehlte, nach dem Speichern unverändert.

### Übergangsarten als Auswahlfeld, nicht als vier Häkchen

`nonlinear`, `wave` und `clamp` teilen sich zwei Bits — wer zwei ankreuzt,
bekommt stillschweigend eine dritte Art. Im Original sind es Häkchen und
nirgends steht, dass sie sich ausschließen.

Hier ein Auswahlfeld für diese drei, Häkchen nur für `linear` und `random`.
Damit ist der Fehler gar nicht erst möglich.

### Ein Fehler, der beim Verdrahten auffiel

`refreshDiagnostics()` hängte an, statt neu aufzubauen. Beim Öffnen einer Datei
fiel das nicht auf — aber die Eigenschaftenblätter rufen es bei **jeder**
Feldänderung. Nach hundert Tastendrücken stand dieselbe Warnung hundertmal in
der Statusleiste.

Behoben durch Trennung: `parseDiagnostics_` gilt, bis die Datei neu gelesen
wird, `diagnostics_` wird bei jeder Änderung daraus neu aufgebaut.

## Warum unsere Eigenschaftenblätter anders aussehen

Drei verschiedene Gründe, und einer davon ist ein Fehler.

### 1. Mein Fehler: CameraShake fehlte der Reiter Origin/Size

Ich hatte die Reitertabelle aus der `switch (mType)`-Verzweigung in
`FxScheduler.cpp` hergeleitet. Dort steht beim CameraShake nur:

```cpp
theFxHelper.CameraShake( org, mElasticity, mRadius, mLife );
```

Also drei Werte, also zwei Reiter — dachte ich. Zu kurz gedacht: der Block
**„Origin calculations" steht auf Zeile 1490, die Verzweigung erst auf 1692.**
Der Ursprung wird für *jeden* Typ berechnet, bevor der Typ überhaupt betrachtet
wird.

Und beim CameraShake ist er nicht nebensächlich, sondern entscheidend: die
Stärke fällt mit dem Abstand des Betrachters zu genau diesem Punkt. Wer den
Ursprung nicht setzen kann, kann kein Erdbeben an eine Stelle legen.

Behoben, und der Test prüft jetzt für **jeden** Typ, dass der Ursprung gilt.
Dasselbe für `radius` und `height`, die zur Kugel- und Zylinderverteilung
gehören und damit ebenfalls überall.

### 2. Drei Dinge, die Raven besser gemacht hat — übernommen

**Forward / Right / Up statt X / Y / Z.** Der Ursprung wird mit der Effektachse
verrechnet: `org = ax[0]·x + ax[1]·y + ax[2]·z`. `ax[0]` ist vorwärts, `ax[1]`
rechts, `ax[2]` oben. „X/Y/Z" zu schreiben ist schlicht falsch, solange die
Effektachse nicht der Weltachse entspricht.

**„Relative to effect axis" statt `cheapOrgCalc`.** Dasselbe Flag, aber
umgekehrt formuliert und dort platziert, wo es wirkt. Wer `cheapOrgCalc` in
einer Flagliste liest, weiß nicht, dass es die Bedeutung der drei Zahlen
darüber ändert.

**Beschrifteter Regler für die Erschütterungsstärke.** Raven schreibt „Nothing"
unten und „Quake"/„Insane" oben an den Schieberegler. Eine nackte Zahl zwischen
0 und 16 sagt nichts; mit Landmarken weiß man sofort, wo man steht.

Dasselbe Prinzip bei „Use even delay distribution": das gehört direkt unter das
Verzögerungsfeld, nicht in eine Flagliste weiter unten — es beschreibt, *wie*
diese Spanne benutzt wird. Und es ist ausgegraut, solange die Verzögerung keine
Spanne ist.

### 3. Wo wir bewusst abweichen

**Spannen als Häkchen statt zweier Spalten.** Raven zeigt überall „Min" und
„Max", auch wo nur eine Zahl steht — man sieht nicht, welche Felder würfeln.

**Übergangsarten als Auswahlfeld.** Bei Raven ist „Transition" ein
Auswahlfeld für die Kurvenform und „Apply random factor" ein eigenes Häkchen.
Das ist im Kern dasselbe wie hier; wir schließen zusätzlich aus, dass sich
`nonlinear`, `wave` und `clamp` gegenseitig überschreiben.

**Nicht ausgewertete Felder werden benannt.** Raven graut sie aus, was besser
ist als nichts — aber es sagt nicht, warum. Wir schreiben es hin:
„Wird bei diesem Typ nicht ausgewertet: rgb velocity shaders".

## Alle Eigenschaftsseiten gegen das Binary geprüft

Du hattest recht: es reichte nicht, die Reitertabelle aus dem Spielcode
herzuleiten. Der Spielcode sagt, was die *Engine* liest — nicht, wie Ravens
Editor die Felder gruppiert hat. Also habe ich die Dialoge aus `RT_DIALOG` in
`EffectsEd.exe` dekodiert und Beschriftung für Beschriftung verglichen.

**Vier Fehler, eine Lücke:**

| Feld | wo ich es hatte | wo es hingehört |
|---|---|---|
| `min` / `max` | Origin/Size | **Physics**, Kasten „Bounding Box" mit `useBBox` |
| `angles` / `angleDelta` | Motion | **Model**, als Pitch/Yaw/Roll |
| `rotation` | Motion | **Origin/Size** |
| `deathfx` | Physics | **Generation**, Kasten „Death Effects" |
| Electricity-Felder | fehlten | **Line**: Chaos, Taper, Branching, Grow |

Die Begrenzungsbox ist der lehrreichste Fall: sie *sieht aus* wie eine
Lageangabe, ist aber die Kollisionsbox der Physik — und ohne das Flag `useBBox`
wirkungslos. Im Original stehen beide in einem Kasten zusammen; bei mir standen
sie zwei Reiter auseinander.

**Und ein Fund, der in die andere Richtung geht:** `gravity` und `wind` stehen
in **keiner** der zwölf Eigenschaftsseiten. Die Engine liest `mGravity` in
`FX_AddParticle`, aber Raven hat nie ein Feld dafür gebaut. Wer Schwerkraft
wollte, musste die Datei mit einem Texteditor bearbeiten. Wir zeigen sie im
Bewegungs-Reiter.

### Flags gehören dorthin, wo sie wirken

Der eigentliche Unterschied zwischen Ravens Blättern und meinen war nicht die
Zuordnung der Zahlen, sondern die der **Flags**. Ich hatte alle in eine Liste
unter „Generation" gepackt. Raven verteilt sie:

```
depthHack       -> Origin/Size, als "Always draw on top"
useAlpha        -> Color, als "Modulate RGB value using alpha value"
setShaderTime   -> Color, direkt unter der Shaderliste
useModel        -> Model, als "Attach model"
emitFx          -> Emitter, als "Emit effects"
impactKills     -> Physics, als "Kill effect on impact"
useBBox         -> Physics, über min/max
deathFx         -> Generation, über der deathfx-Liste
randrotaroundfwd-> FxRunner
org2fromTrace / org2isOffset -> Line, als drei Auswahlknöpfe
```

Das ist deutlich besser, und der Grund ist einfach: ein Flag in einer Liste sagt
nur seinen Namen. Direkt über dem Feld, das es scharf schaltet, sagt es, wozu
es da ist. `useBBox` über `min`/`max` ist selbsterklärend; `useBBox` in einer
alphabetischen Liste ist eine Vokabel.

Die vollständige Flagliste bleibt zusätzlich, aber zugeklappt — wer eine fremde
Datei öffnet, will alles an einer Stelle sehen können.

## Emitter und Motion: gemessen statt abgeleitet

### Emitter hat sechs Reiter, nicht sieben — und keine Farbseite

Gemessen: **Generation, Origin/Size, Motion, Physics, Emitter, Model.**

Ich hatte sieben, in falscher Reihenfolge, mit Color an dritter Stelle.

Die Farbseite hängen wir trotzdem an, und zwar begründet:

```cpp
FX_AddEmitter( org, vel, accel, size1, size2, sizeParm,
               alpha1, alpha2, alphaParm,        // <- Alpha
               rgb1, rgb2, rgbParm, ... );       // <- Farbe
```

Der Scheduler füllt sie aus `fx->mAlphaStart` und `sRGB`. **Die Engine kann es,
Ravens Editor zeigt es nur nicht** — dasselbe Muster wie bei `gravity` und
`wind`. Der Reiter steht am Ende und trägt einen Hinweis, damit niemand denkt,
wir hätten uns das ausgedacht.

### Dialog 160 (Motion) endlich gesehen

Aus dem Binary war er nicht zu dekodieren; das Bild vom Emitter zeigt ihn:

```
Velocity      Forward / Right / Up      [x] Relative to effect axis
              [ ] Affected by wind
                  Percentage (generally 1 to 100)
Acceleration  Forward / Right / Up      [x] Relative to effect axis
Gravity                                 (Standard Quake gravity is -800)
Rotation, Rotation Delta
```

Drei Dinge daraus:

- **Meine Vermutung stimmte** — velocity, acceleration, gravity und wind
  gehören hierher.
- **„Relative to effect axis" gibt es dreimal**, mit drei verschiedenen Flags:
  `cheapOrgCalc` beim Ursprung, `cheapOrg2Calc` beim Endpunkt,
  `absoluteVel` und `absoluteAccel` bei Geschwindigkeit und Beschleunigung.
  Alle vier sind umgekehrt formuliert: angehakt heißt Flag **nicht** gesetzt.
- **„Affected by wind" und der Prozentwert gehören zusammen.** Das Flag kennt
  nur der Multiplayer-Zweig — das steht jetzt neben dem Häkchen, und die
  Prüfung meldet es beim SP-Ziel.

Der Hinweis „Standard Quake gravity is -800" ist übernommen. Eine leere Zahl
ohne Bezugsgröße hilft niemandem.

### Die Drehung steht im Original in zwei Seiten

In 171 (Origin/Size) für Cylinder und Decal, in 160 (Motion) für bewegte
Primitiven — jeweils dort aktiv, wo sie hingehört, und in der anderen
ausgegraut.

Wir zeigen sie **einmal**: bei Typen mit Bewegungsseite dort, sonst in
Origin/Size. Dasselbe Feld an zwei Stellen anzubieten ist die sicherste Art,
jemanden am falschen Ort suchen zu lassen.

### Stand der Überprüfung

Sechs von dreizehn Typen sind gemessen: CameraShake, Cylinder, Decal, Line,
Electricity, Emitter. Die übrigen sieben sind abgeleitet — bei Particle,
OrientedParticle und Tail mit hoher Sicherheit, weil der Emitter ihre Reiter
bestätigt hat; bei Light und Flash am wenigsten.

## Eine feste Seitenreihenfolge erklärt alles

Beim siebten gemessenen Typ fiel ein Muster auf: Ravens Editor hängt **nicht**
je Typ eine eigene Reiterliste an. Er filtert eine einzige feste Reihenfolge:

```
Generation → Origin/Size → Motion → Physics → Line → Length/Size2
           → Emitter → Model → Sound → FxRunner → CameraShake → Color
```

Alle sieben gemessenen Typen passen widerspruchsfrei hinein — CameraShake,
Cylinder, Decal, Line, Electricity, Emitter, Flash. Keine zwei widersprechen
sich.

Der auffälligste Punkt daran: **Color steht ganz hinten**, nicht in der Mitte.
Ich hatte es bei Particle an dritter Stelle. Sichtbar wurde es erst beim
Emitter, weil dort die Farbseite ganz fehlt und die übrigen fünf dadurch
lückenlos aneinanderstanden.

`tabsFor()` ist jetzt entsprechend gebaut: eine Reihenfolge, eine Zuordnung je
Reiter. Damit ist die Reihenfolge auch für die sechs noch nicht gemessenen Typen
richtig, ohne sie zu raten — und der Test hält es fest: **kein Typ darf zwei
Reiter in umgekehrter Reihenfolge zeigen**, und die sieben gemessenen Fälle
werden Reiter für Reiter verglichen.

## Flash: zweimal derselbe Denkfehler

Gemessen: Generation, **Origin/Size**, Color. Ich hatte Origin/Size
weggelassen, weil ein bildschirmfüllendes Aufblitzen keine Lage zu brauchen
scheint.

Es braucht sie doch — die Stärke hängt vom Abstand zum Ursprung ab, genau wie
beim CameraShake. Dort hatte ich denselben Fehler gemacht, aus demselben Grund:
ich habe von der sichtbaren Wirkung auf die gelesenen Felder geschlossen statt
umgekehrt.

Zwei Details aus dem Bild, beide ausgegraut und damit gemessen: Flash hat
**keine** Ende-Effekte und **keine** Kugel- oder Zylinderverteilung.

## Stand

Sieben von dreizehn Typen gemessen. Die übrigen sechs folgen jetzt aus der
Seitenreihenfolge statt aus Vermutung — bei Particle, OrientedParticle und Tail
mit hoher Sicherheit, weil Emitter ihre Reiterkombination bestätigt hat.

## FxRunner: die Reiter stimmten, drei Felder nicht

Gemessen: Generation, Origin/Size, FxRunner — genau wie bei mir. Aber im Bild
sind drei Dinge **ausgegraut**, die ich als gültig führte:

- **Life (ms)** — er löst andere Effekte aus und hat selbst keine Lebensdauer.
  `PlayEffect(mPlayFxHandles, org, ax)` liest sie auch nicht.
- **die ganze Gruppe Size/Width** — er zeichnet nichts.
- **Death Effects** — dito.

Ausgegraute Felder sind genauso viel wert wie sichtbare: sie sagen, was ein Typ
*nicht* liest. Das hätte ich aus dem Spielcode ableiten können und habe es
nicht.

Für den Sound gilt dasselbe bei Lebensdauer und Größe — ein Klang bringt seine
Länge selbst mit.

## Zwei Quellen für dieselbe Wahrheit

Beim Nachziehen fiel auf, dass `validate.cpp` **eigene** Funktionen für
„welcher Typ liest life" und „welcher liest Shader" hatte. Als die Tabelle in
`fields.cpp` nach den Messungen genauer wurde, wichen sie ab: die Prüfung hielt
einen Decal für lebensdauerlos, die Tabelle nicht.

Das ist derselbe Fehler wie bei den Themennamen, die einmal in `theme.cpp` und
einmal in der Übersetzungstabelle standen. Zwei Quellen für dieselbe Wahrheit
laufen früher oder später auseinander — und man merkt es erst, wenn eine von
beiden geändert wird.

Die Prüfung fragt jetzt die Tabelle. Ein Test hält es fest: über alle dreizehn
Typen darf keine Meldung über ein Feld kommen, das die Tabelle für diesen Typ
gar nicht vorsieht.

Stand: **acht von dreizehn Typen gemessen.** Offen: Particle, OrientedParticle,
Tail, Sound, Light.

## Light: und ein Fund im ersten Bild, das ich schon hatte

Gemessen: Generation, Origin/Size, Color — Reiter stimmten. Vier Felder nicht.
Ausgegraut sind bei Light: **Count**, „Use even delay distribution", die
besonderen Verteilungen, Death Effects, die Shaderliste und die ganze
Alphagruppe.

Dass Light keine **Anzahl** hat, ist einleuchtend, sobald man es sieht:
`FX_AddLight` setzt genau ein Licht an eine Stelle. Mehrere übereinander wären
sinnlos. Und deshalb ist auch „Use even delay distribution" ausgegraut — das
verteilt eine Anzahl über die Verzögerungsspanne, und ohne Anzahl gibt es
nichts zu verteilen.

**Und damit fiel etwas auf, das ich schon vor zwölf Runden auf dem Tisch
hatte:** im allerersten CameraShake-Bild war „Count" ebenfalls ausgegraut. Ich
habe es damals nicht gesehen, weil ich auf die Reiterleiste geschaut habe.

Dieselbe Begründung: ein CameraShake rüttelt einmal.

## Die vollständige Matrix

Neun von dreizehn Typen sind jetzt gemessen. Offen: Particle,
OrientedParticle, Tail, Sound.

Bei den ersten dreien stützt sich die Ableitung auf zwei Messungen zugleich —
die Seitenreihenfolge und den Emitter, der ihre Reiterkombination
(Motion + Physics) bestätigt hat. Beim Sound bleibt am meisten Unsicherheit,
aber dort gibt es auch am wenigsten zu holen: er hat drei Reiter, und der
mittlere ist fast leer.

### Was das Ausgegraute gelehrt hat

Bei den ersten sechs Typen habe ich auf die Reiterleiste geschaut. Erst beim
FxRunner ist mir aufgefallen, dass die **grauen** Felder mindestens so viel
sagen wie die schwarzen: sie benennen, was ein Typ *nicht* liest.

Aus dem Spielcode ist das schwerer zu holen, weil dort nur steht, was gelesen
wird — nicht, was fehlt. Ein `switch`-Zweig, der `mCount` nicht erwähnt, sieht
genauso aus wie einer, der es vergessen hat.

## Line: zwei ausgegraute Gruppen, und ein Test, der einen Fehler verteidigt hat

Die Reiter stimmten (das war schon gemessen). Aber im Line-Bild sind zwei
Gruppen grau, die bei mir aktiv waren:

- **Endpoint Effects** — grau, solange „Use specified endpoint" gewählt ist.
  Ohne Trace gibt es keinen Aufprallpunkt, an dem etwas ausgelöst werden könnte.
  Das hatte ich schon so gebaut.
- **Die ganze Electricity-Gruppe** — grau bei einem Line. Auch das hatte ich
  schon richtig.

Neu ist die **Drehung**: im Line-Bild fehlt sie ganz. Und im Emitter-Bild war
sie ausgegraut. Beides ließ sich am Spielcode nachschlagen — `mRotation` wird an
genau **vier** Stellen gelesen:

```cpp
FX_AddParticle( ..., mRotation, mRotationDelta, ... );   // zweimal
CG_ImpactMark( handle, org, ax[0], mRotation, ... );      // Decal
FX_AddOrientedParticle( ..., mRotation, mRotationDelta, ... );
```

Also genau drei Typen: **Particle, OrientedParticle, Decal.** Ich hatte sie für
jeden Typ mit Lage- oder Bewegungsseite freigegeben.

Und eine Feinheit: `CG_ImpactMark` nimmt nur `mRotation` entgegen, keine
Änderung — ein Decal liegt still auf der Wand. Es bekommt also die Drehung,
aber keine Drehänderung.

### Ein Test, der einen Fehler verteidigt hat

Beim Umstellen wurden zwei Prüfungen rot, die ich in der Emitter-Runde
geschrieben hatte:

```
FEHLER: Cylinder hat eine Drehung
FEHLER: Emitter auch
```

Beide waren **falsch** — geschrieben aus meiner damaligen Annahme, nicht aus
einer Messung. Sie standen dort und hätten die Korrektur blockiert, wenn ich
ihnen geglaubt hätte statt dem Quelltext.

Ein Test, der eine falsche Annahme festhält, ist schlimmer als gar keiner: er
verteidigt den Fehler gegen die Korrektur. Ich habe sie durch einen Kommentar
ersetzt, der sagt, was dort stand und warum es weg ist.

## OrientedParticle: die erste Messung ohne Korrektur

Gemessen: **Generation, Origin/Size, Motion, Physics, Color** — genau wie
abgeleitet, in genau dieser Reihenfolge. Zum ersten Mal in dieser Reihe stimmte
alles.

Das bestätigt nachträglich zwei Dinge:

- Die **Seitenreihenfolge** trägt: `Particle` bekommt dieselben fünf Reiter, weil
  es dieselben Felder liest. Damit ist auch `Tail` gedeckt, das sich nur um
  Length/Size2 unterscheidet.
- Die **Drehung** ist im Motion-Reiter aktiv, nicht ausgegraut wie beim Emitter.
  Genau das sagt der Quelltext: `FX_AddOrientedParticle` nimmt `mRotation` und
  `mRotationDelta`, `FX_AddEmitter` nicht.

Eine Kleinigkeit war noch daneben: „Use Extremely Expensive Physics" steht im
Original **direkt unter** „Enable Physics", nicht am Ende der Seite. Das ist
richtiger, weil es beschreibt, *wie* die Physik rechnet — es gehört neben den
Schalter, der sie einschaltet, nicht hinter alles andere.

## Bilanz der Eigenschaftsseiten

**Zehn von dreizehn Typen gemessen.** Offen bleiben Particle, Tail und Sound —
alle drei durch die Seitenreihenfolge und durch OrientedParticle bzw. Emitter
gedeckt.

Was die Reihe insgesamt gebracht hat, gegenüber der ersten Fassung aus dem
Spielcode allein:

| | |
|---|---|
| Reiter falsch zugeordnet | CameraShake, Flash, Emitter |
| Reihenfolge falsch | Cylinder, Line, Electricity, Emitter, Particle |
| Felder im falschen Reiter | min/max, angles, rotation, deathfx |
| Felder zu weit gefasst | rotation (9 statt 3), life, size, count, radius |
| Ganz gefehlt | Electricity-Gruppe, Motion-Seite |
| Flags falsch platziert | zehn Stück |
| Eigene Tests, die einen Fehler festhielten | zwei |

Der Spielcode sagt, **was** gelesen wird. Er sagt nicht, wie es gruppiert
gehört, und er sagt vor allem nicht, was ein Typ **nicht** liest — ein
`switch`-Zweig ohne `mCount` sieht genauso aus wie einer, der es vergessen hat.
Die ausgegrauten Felder im Original sagen genau das.

## Particle: die zweite Messung ohne Fund

Generation, Origin/Size, Motion, Physics, Color — identisch mit
OrientedParticle, wie abgeleitet. Im Bild ist bei beiden **nichts**
ausgegraut, einschließlich Drehung und Drehänderung im Motion-Reiter.

Damit ist auch `Tail` gedeckt: es unterscheidet sich von `Particle` nur um die
Seite Length/Size2, und die ist beim Cylinder gemessen.

### Elf von dreizehn gemessen

Offen bleiben `Tail` und `Sound`. Beide folgen aus Messungen, die schon
vorliegen — Tail aus Particle plus Cylinder, Sound aus der Seitenreihenfolge
plus dem Umstand, dass seine einzige eigene Seite eine Klangliste enthält.

Bemerkenswert an den letzten beiden Runden: nach zehn Typen mit Korrekturen
kamen zwei ohne. Das ist kein Zufall, sondern die Wirkung der Seitenreihenfolge
— sobald sie stand, folgten die restlichen Typen daraus, statt einzeln geraten
zu werden.

Rückblickend hätte ich sie früher suchen sollen. Ich habe zehn Typen einzeln
verglichen, bevor mir auffiel, dass sie alle einer Regel folgen. Das Muster war
schon nach dem vierten sichtbar gewesen.

## Sound: der letzte Typ, und damit zwölf von dreizehn

Gemessen: Generation, Origin/Size, Sound — Reiter stimmten. Ein Feld nicht:
die **besonderen Verteilungen** sind ausgegraut. Das ist einleuchtend, sobald
man es sieht — eine Kugel- oder Zylinderverteilung streut den Ursprung, und ein
Klang wird an genau einer Stelle abgespielt.

Aktiv sind dagegen Anzahl, Verzögerung, gleichmäßige Verteilung, Sichtweite und
der Ursprung. Der Ursprung wird für die Panoramierung gebraucht.

### Endstand der Feldmatrix

Zwölf von dreizehn Typen gegen das Original gemessen. Nur `Tail` fehlt, und der
folgt aus zwei Messungen zugleich: `Particle` liefert seine Reiterkombination,
`Cylinder` die Seite Length/Size2.

```
Typ                    count  life   delay  origin radius size   rgb    alpha  veloci gravit shader deathf
Particle           [M]   x      x      x      x      x      x      x      x      x      x      x      x
OrientedParticle   [M]   x      x      x      x      x      x      x      x      x      x      x      x
Line               [M]   x      x      x      x      x      x      x      x      -      -      x      x
Electricity        [M]   x      x      x      x      x      x      x      x      -      -      x      x
Tail                     x      x      x      x      x      x      x      x      x      x      x      x
Cylinder           [M]   x      x      x      x      x      x      x      x      -      -      x      x
Emitter            [M]   x      x      x      x      x      x      x      x      x      x      x      x
Decal              [M]   x      x      x      x      x      x      x      x      -      -      x      x
Sound              [M]   x      -      x      x      -      -      -      -      -      -      -      -
FxRunner           [M]   x      -      x      x      x      -      -      -      -      -      -      -
Light              [M]   -      x      x      x      -      x      x      -      -      -      -      -
CameraShake        [M]   -      x      x      x      x      -      -      -      -      -      -      -
Flash              [M]   x      x      x      x      -      x      x      x      -      -      x      -
```

Was diese Reihe insgesamt geliefert hat: **68 neue Prüfungen**, jede gegen eine
Beschriftung im Original oder eine Zeile im Spielcode. Und drei Erkenntnisse,
die keine Einzelkorrektur sind:

1. **Der Spielcode sagt nicht, was fehlt.** Ein `switch`-Zweig ohne `mCount`
   sieht aus wie einer, der es vergessen hat. Die ausgegrauten Felder im
   Original sagen genau das — sie waren am Ende die ergiebigste Quelle.
2. **Es gibt eine feste Seitenreihenfolge.** Sobald sie stand, folgten die
   restlichen Typen daraus. Ich hätte sie nach der vierten Messung suchen
   sollen, nicht nach der siebten.
3. **Ein Test kann einen Fehler verteidigen.** Zwei meiner eigenen Prüfungen
   hielten eine Annahme fest und wurden rot, als die Messung sie widerlegte.
   Der Unterschied zu den guten: die belegen eine Zeile im Original, nicht
   das, was ich gerade dachte.

## Tail: der dreizehnte Typ korrigiert die Reihenfolge

Gemessen: **Generation, Origin/Size, Length/Size2, Motion, Physics, Color.**

Bei mir stand Length/Size2 **hinter** Physics. Der Tail ist der einzige Typ, der
Length/Size2 **und** Motion hat — die anderen zwölf zeigen nie beide
gleichzeitig und passten deshalb auch zur falschen Reihenfolge:

```
BISHER  passt fuer 12/13  — bricht bei: Tail
NEU     passt fuer 13/13  — alle
```

Die richtige Reihenfolge:

```
Generation → Origin/Size → Length/Size2 → Line → Motion → Physics
           → Emitter → Model → Sound → FxRunner → CameraShake → Color
```

Wo genau `Line` zwischen Length/Size2 und Motion sitzt, bleibt unbestimmt: kein
Typ hat beide. Die Stelle ist gewählt, nicht gemessen — und das steht so im
Quelltext.

Dass zwölf Messungen eine falsche Regel stützen konnten, ist die eigentliche
Lehre. Ich hatte die Reihenfolge nach sieben Typen für belegt gehalten. Sie war
es nicht — sie war nur nicht widerlegt, und das ist etwas anderes.

### Und size2 gibt es nur beim Cylinder

Im Bild ist beim Tail die Gruppe „Size2/Width2" ausgegraut. Der Spielcode sagt
dasselbe: `FX_AddCylinder` liest die drei `mSize2`-Werte, `FX_AddTail` nimmt sie
nicht entgegen. Ich hatte size2 an die Seite gebunden statt an den Typ.

## Alle dreizehn Typen gemessen

```
Particle           Generation | Origin/Size | Motion | Physics | Color
OrientedParticle   Generation | Origin/Size | Motion | Physics | Color
Line               Generation | Origin/Size | Line | Color
Electricity        Generation | Origin/Size | Line | Color
Tail               Generation | Origin/Size | Length/Size2 | Motion | Physics | Color
Cylinder           Generation | Origin/Size | Length/Size2 | Color
Emitter            Generation | Origin/Size | Motion | Physics | Emitter | Model | Color
Decal              Generation | Origin/Size | Color
Sound              Generation | Origin/Size | Sound
FxRunner           Generation | Origin/Size | FxRunner
Light              Generation | Origin/Size | Color
CameraShake        Generation | Origin/Size | CameraShake
Flash              Generation | Origin/Size | Color
```

Nichts ist mehr abgeleitet. Jede Zeile hat ein Bildschirmfoto hinter sich, jede
Feldregel eine Beschriftung im Original oder eine Zeile im Spielcode.

## Der Windvektor: er tut nichts

Der Pfeil in der Mitte zeigt eine Richtung an, die das Spiel **in keinem
Modus** benutzt. Das steht im Quelltext, und zwar dreifach belegt.

**Singleplayer:** `FX_AFFECTED_BY_WIND` ist definiert und wird nirgends
geprüft. Der Parser kennt das Wort nicht einmal.

**Multiplayer:** der Parser liest es, aber der auswertende Block ist
auskommentiert — in `codemp/client/FxScheduler.cpp`, Zeile 1345, mit Ravens
eigenem Kürzel davor:

```cpp
if ( fx->mSpawnFlags & FX_AFFECTED_BY_WIND )
{
/*rjr    vec3_t wind;
        CL_GetWindVector( wind );
        VectorMA( vel, fx->mWindModifier.GetVal() * 0.01f, wind, vel );
*/
}
```

**Und die Funktion gibt es nicht.** `CL_GetWindVector` kommt im gesamten
Quelltext **genau einmal** vor: in dieser auskommentierten Zeile. Sie wurde nie
geschrieben. Es existiert nur `R_GetWindVector` im Renderer, und das treibt
Regen und Grasbüschel, keine Effekte.

`mWindModifier` wird ebenfalls nur an dieser einen toten Stelle gelesen.

Wer `wind` in eine .efx-Datei schreibt oder `affectedByWind` setzt, bekommt also
nichts — in keinem Spielmodus. Der alte Editor zeigt dafür trotzdem einen
Windpfeil, ein Häkchen und ein Prozentfeld. Die Prüfung sagt das jetzt hin,
statt es beim Dialekt zu belassen.

Den Pfeil selbst behalten wir, aber als das, was er ist: eine Anzeigehilfe, um
sich eine Richtung vorzustellen. Richtung und Länge sind einstellbar und werden
gespeichert; im Dialog steht der Hinweis, damit niemand daran dreht und sich
wundert, warum die Partikel nicht reagieren.

### Und wieder ein zu grober Test

Eine ältere Prüfung sagte „im MP-Ziel ist die Meldungsliste leer". Das war nicht
falsch, aber zu grob — gemeint war „keine Beschwerde über den Dialekt", und
sobald die Windmeldung dazukam, wurde sie rot. Jetzt prüft sie, was sie meint.

## Auswahlfenster für Shader, Modelle und Klänge

Nachbau von Dialog 184 („Choose Shaders"), aber als **ein** Fenster für alle
vier Listen — sie unterscheiden sich nur in der Quelle.

Zwei Abweichungen vom Original:

- **Ein Filterfeld.** Ein `base`-Ordner hat leicht zweitausend Shader; eine
  reine Liste durchzublättern ist keine Bedienung mehr. Das Original hat nur
  die Liste.
- **Ein Hinweis, wenn kein Spielpfad gesetzt ist.** Im Original bleibt das
  Fenster dann einfach leer, und man rät.

„Textures..." und „Preview" sind angelegt, aber ausgegraut: der erste öffnet im
Original einen gewöhnlichen Dateidialog mit dem Filter
`*.tga; *.png; *.jpg; *.pcx; *.bmp` und trägt den Pfad **direkt in die
Shaderliste** ein — die Engine baut daraus einen Ersatzshader aus der Bilddatei.
Beides braucht den Materialsuchlauf, der als Nächstes kommt.

## Fünf Punkte aus dem Feldtest

### 1. Die Typbeschreibungen waren gar nicht übersetzt

„Stehender Zylinder mit veraenderlicher Groesse und Laenge." — fest verdrahtet
in `src/effect.cpp`, deutsch, **ohne Umlaute**, in jeder Sprache dieselbe.

Der Prüfer hatte sie nicht gefunden, weil er nur Zuweisungen an Felder namens
`name` oder `label` kannte. Diese dreizehn standen in einer Tabelle.

Jetzt in der Übersetzungstabelle, mit Umlauten. Und der Prüfer hat eine zweite
Regel bekommen: Zeichenketten, die wie ein Satz aussehen (Leerzeichen und ein
Satzzeichen), sind mit hoher Wahrscheinlichkeit Anzeigetext — ein Bezeichner
wie `orgOnSphere` nicht.

**Damit findet er 49 weitere**, davon 15 in `validate.cpp`: die Meldungstexte
der Prüfung sind alle deutsch und einsprachig. Das ist ein eigener Brocken und
steht als Nächstes an; ich sage es lieber, als es stillschweigend liegen zu
lassen.

### 2. Ankreuzfelder vor jedem Feld — weg

Das war ehrlich gegenüber dem Dateiformat: ein Feld ohne Häkchen wird nicht
geschrieben, und eine Datei, in der es fehlte, bleibt nach dem Speichern
unverändert. Diese Eigenschaft bleibt.

Aber als Bedienung war es falsch. Zwölf Häkchen auf einer Seite, und keines
sagt, wozu es gehört.

Jetzt wie im Original: **Gruppenrahmen mit Titel**, und das Häkchen sitzt im
Titel der Gruppe. Ein Feld gilt als gesetzt, sobald jemand es anfasst; wer es
loswerden will, nimmt das Häkchen der Gruppe. Die Felder darin werden
**ausgegraut, nicht versteckt** — auch das wie im Original, und es ist besser,
weil die Seite ihre Form behält.

### 3. Der Windvektor ist eine Fahne

Du hattest recht, das fehlte komplett. Im Original ist es kein Pfeil, sondern
ein **Wimpel am Mast**, der weht und dabei die Richtung wechselt.

Das ist mehr als Zierde: eine feste Linie sagt „der Wind kommt von dort", eine
wehende Fahne sagt zusätzlich, wie stark er ist und dass er schwankt.

```
Windfahne: 25 Linien, Richtung schwankt bis 29 Grad
```

Zwei Details, die die Tests erzwungen haben:

- **Unrunde Frequenzen** für die Schwankung (0.7 und 1.9 Hz). Bei ganzzahligem
  Verhältnis sieht man nach wenigen Sekunden dieselbe Bewegung wieder, und dann
  wirkt es wie eine Maschine, nicht wie Wind.
- **Der Ausschlag wächst quadratisch zur Spitze.** So bewegt sich ein Wimpel
  wirklich — das Ende schlägt am weitesten aus, der Mast steht still. Der Test
  prüft beides.

Geprüft wird außerdem, dass die Richtung spürbar schwankt (>0.05 rad), aber
nicht davonläuft (<1.2 rad), und dass starker Wind deutlich mehr schwankt als
schwacher.

### 4. Fenster zu klein skalierbar

`SetNextWindowSizeConstraints` mit einer Untergrenze von 420×320. Ohne sie ließ
sich das Auswahlfenster auf Briefmarkengröße ziehen.

### 5. Sind alle Einstellungen korrekt?

Die **Feldzuordnung** ja — dreizehn von dreizehn Typen gegen das Original
gemessen, jede Regel gegen eine Beschriftung oder eine Zeile Spielcode.

Die **Anordnung innerhalb** der Seiten war es nicht, und das ist genau, was
dieser Durchgang behoben hat: Gruppen statt loser Felder. Was noch abweicht,
sind die zwei Spalten „Min/Max" des Originals — dort steht immer beides, bei uns
schaltet ein `~` um. Dabei bleibe ich: so sieht man auf einen Blick, welche
Felder würfeln.

## Damit du testen kannst: Dateidialoge und Materialsuchlauf

Zwei Dinge standen zwischen dem Stand und „damit arbeiten können".

### Dateien öffnen und speichern

`Öffnen`, `Speichern` und `Speichern unter` waren Menüpunkte ohne Dialog. Jetzt
über `GetOpenFileNameW`.

Drei Entscheidungen dabei:

- **Die W-Fassung, nicht A.** Pfade mit Umlauten oder chinesischen Zeichen sind
  sonst kaputt — und genau dort liegen bei vielen Leuten die Mods.
- **`OFN_NOCHANGEDIR`.** Ohne das setzt der Dialog das Arbeitsverzeichnis des
  ganzen Programms um, und relative Pfade zeigen danach woanders hin.
- **`Speichern` ohne bekannten Pfad wird zu `Speichern unter`.** Sonst
  verschwindet die Datei wortlos irgendwohin.

Die Oberfläche kennt weiterhin keine Win32-Aufrufe; der Dialog kommt als
Rückruf vom Fensterrahmen.

### Der Materialsuchlauf liest .pk3

Das war die wichtigere Entscheidung. Bei einer normalen Installation liegt
praktisch **alles** gepackt — ein Suchlauf, der nur ausgepackte Dateien findet,
findet nichts.

Ein `.pk3` ist eine Zip-Datei. Wir lesen **nur das Inhaltsverzeichnis am Ende**,
nicht die Dateien selbst: für eine Liste der vorhandenen Namen reichen die
Namen, und das Verzeichnis zu lesen kostet einen Bruchteil der Zeit, die das
Entpacken bräuchte. Bei zwanzig Archiven à 200 MB werden so ein paar hundert
Kilobyte gelesen statt vier Gigabyte.

Jedes Archiv kann ein eigener Faden lesen — das ist genau der Fall, für den der
Arbeitsverteiler gebaut wurde.

**Eine Ausnahme:** `.shader`-Dateien müssen ausgepackt werden, weil die
Shadernamen *im Inhalt* stehen, nicht im Dateinamen. Die im Archiv bleiben
vorerst ungelesen — und das steht als Hinweis im Ergebnis, statt stillschweigend
zu fehlen. Dafür bräuchte es einen Entpacker (deflate), und das ist ein eigener
Brocken.

### Kaputte Archive dürfen nicht abstürzen

Ein `.pk3` liegt im Spielordner, und da kommt alles Mögliche her. Der Leser wird
deshalb gegen fünf Fälle geprüft: nicht vorhanden, leer, zwei Byte lang, reiner
Text mit `.pk3`-Endung, und — der interessanteste — ein **gültiger Endeintrag,
dessen Verzeichnis aus der Datei hinauszeigt**. Genau damit bringt man einen
naiven Zip-Leser zum Absturz.

```
ausgepackt: 2 Shader, 1 Texturen, 1 Modelle, 1 Klaenge, 1 Effekte
gepackt: 1 Archiv, 1 Modelle, 1 Klaenge, 1 Effekte, 1 Texturen
```

Das Archiv im Test ist von Hand aus Bytes gebaut, damit er ohne Zusatzwerkzeug
läuft — und geprüft wird auch, dass verteiltes Lesen dasselbe ergibt wie
serielles.

### Was noch offen ist

Die **49 Meldungstexte der Prüfung** sind weiterhin einsprachig deutsch. Das
steht als Nächstes an; es ist mechanisch, aber viel, und ich wollte es nicht
zwischen zwei anderen Dingen halb machen.

## Zwei erfundene Namen — und was daraus folgt

Der Bau schlug fehl an zwei Bezeichnern, die ich mir ausgedacht hatte:

- `currentPath_` — heißt `filePath_`
- `window` — heißt `hwnd`, **und lebt in einer anderen Funktion**

Der zweite war der ernstere. Ich hatte den Dateidialog nach `efx::gui::App app;`
eingesetzt, in `wWinMain`. Das Fensterhandle entsteht aber in der
Sitzungsfunktion darüber. Selbst mit dem richtigen Namen wäre es nicht in
Reichweite gewesen — ich habe eine Stelle nach Textmuster gewählt, ohne den
Aufbau der Datei anzusehen.

Behoben, indem die Lambda direkt hinter die Fenstererzeugung wandert.

### Eine Prüfung, die ohne Übersetzer auskommt

Beide Fehler fallen erst beim Übersetzen auf — und übersetzen lässt sich `gui/`
nur auf einem Windows-Rechner mit ImGui. Also nicht hier.

`tools/lint_members.py` prüft jetzt, ob jeder in `gui/*.cpp` benutzte
Membername irgendwo deklariert ist: in `app.h` oder in derselben Datei (die
Renderer haben eigene Klassen). Grob, aber genau für diesen Fehler gebaut.

Ein Detail, das den Unterschied macht: es reicht **nicht**, alle vorkommenden
Namen als bekannt zu zählen. Ein Tippfehler gilt sonst als deklariert, sobald
er zweimal auftaucht — und `currentPath_` kam fünfmal vor. Deshalb erkennt der
Prüfer echte Deklarationen an ihrer Form (Typ, Name, dann `=`, `;` oder `{`).

Gegenprobe: den Fehler kurz wieder eingesetzt, Prüfer meldet ihn, zurückgesetzt,
Prüfer schweigt.

### Und ein Fund nebenbei

Im Kopf des Ablaufprotokolls stand `{"Version", "0.9.1"}` — seit zwölf
Fassungen. Die Nummer stand an zwei Stellen, und meine Suchen-und-Ersetzen-
Läufe trafen immer nur eine.

Ausgerechnet die Angabe, die man bei einem Fehlerbericht braucht, war die
falsche. Jetzt gibt es `include/efx/version.h` mit genau einer Zeile, und beide
Stellen lesen von dort.

## Der dritte Baufehler in Folge — und was er wirklich zeigt

Diesmal: `kVersion` benutzt, `efx/version.h` nicht eingebunden.

Alle drei Fehler haben dieselbe Ursache: **`gui/` lässt sich nur auf einem
Windows-Rechner mit ImGui übersetzen.** Der Quelltext entsteht woanders, und
dort fällt nichts davon auf.

Zweimal ein erfundener Membername, einmal ein vergessenes `#include`. Nichts
davon ist knifflig — es sind genau die Fehler, die ein Übersetzer in einer
Zehntelsekunde findet, und ich habe sie stattdessen an dich weitergereicht.

### Also prüfen, was ohne Übersetzer prüfbar ist

Jetzt laufen drei Prüfer in `build.bat`:

| | |
|---|---|
| `lint_i18n.py` | Oberflächentexte, die nicht durch `tr()` gehen |
| `lint_members.py` | Membernamen, die nirgends deklariert sind |
| `lint_includes.py` | benutzte Namen, deren Kopf fehlt |

Beim dritten war die Kalibrierung der eigentliche Teil der Arbeit. Der erste
Entwurf prüfte auch Funktionsnamen und meldete **vierzig** Stellen — `length`,
`reset`, `parse`, `scan` kommen in jeder zweiten Zeile vor, und fast keine
Meldung stimmte.

Ein Prüfer, der überwiegend falsch meldet, wird ignoriert. Und dann fängt er
auch die eine richtige Meldung nicht mehr. Lieber weniger prüfen und dem
Ergebnis trauen können.

Jetzt sieht er nur noch Konstanten (`kVersion`, `kNoTexture`) und
`enum class`-Namen — beides eindeutig genug. Dazu zählt eine Ebene
weitergereichter Einbindungen mit: `gui/app.h` ist der Sammelkopf dieses
Programms, und wer ihn einbindet, darf seinen Inhalt benutzen. Mehr als eine
Ebene bewusst nicht, sonst wäre alles erreichbar und die Prüfung wertlos.

Gegenprobe wie beim vorigen: `#include` entfernt → gemeldet, zurückgesetzt →
still. Null Fehlmeldungen im ganzen Baum.

### Was das nicht löst

Diese drei Prüfer fangen Tippfehler und vergessene Einbindungen. Sie fangen
**keine** Typfehler, keine falschen Argumente, keine ImGui-Aufrufe mit falscher
Signatur. Dafür bräuchte es einen Übersetzer mit den echten Köpfen.

Wenn dir die Baufehler weiter auf die Nerven gehen, wäre das der nächste
sinnvolle Schritt: ImGui hier hereinholen und `gui/` gegen Ersatz-Köpfe für
Windows übersetzen, nur zum Prüfen. Das ist Arbeit, aber es würde die Schleife
über dich schließen.

## Schritt eins zur Vorschau: die Kurven

Die Vorschau steht auf einem Stück, das alles andere trägt: wie sich ein Wert
über die Lebensdauer verändert. Größe, Farbe, Alpha, Länge laufen alle darüber.

Die Formeln stehen Zeile für Zeile in `CParticle::UpdateSize` und den
gleichgebauten `UpdateRGB`/`UpdateAlpha`, die Umrechnung des Parameters in
`FxUtil.cpp`. Zwei Dinge daran sind überraschend genug, dass sie eigene Tests
bekommen haben.

### Ohne Flag bleibt der Wert auf `start`

```cpp
// completely biased towards start if it doesn't get overridden
float perc1 = 1.0f, perc2 = 1.0f;
...
mRefEnt.radius = (mSizeStart * perc1) + (mSizeEnd * (1.0f - perc1));
```

`perc1` beginnt bei 1.0 und wird **nur** verändert, wenn ein Flag gesetzt ist.
Ohne Kurvenart steht am Ende also durchgehend `start`.

Wer einen Farbverlauf schreibt und `linear` vergisst, bekommt die Startfarbe —
und sucht den Fehler beim Shader. Das ist vermutlich der häufigste Stolperstein
am ganzen Format, und man sieht ihn der Datei nicht an.

**Die Prüfung meldet ihn jetzt.**

### `parm` bedeutet zweierlei

Vor dem Abspielen wird die Zahl umgerechnet, und zwar je nach Kurvenart
verschieden:

```
wave:              parm * PI * 0.001            eine Kreisfrequenz
nonlinear, clamp:  parm * 0.01 * life + jetzt   ein absoluter Zeitpunkt
```

Dieselbe Zahl in der Datei bedeutet also einmal „wie schnell schwingt es" und
einmal „ab wann fängt es an". Gemessen:

```
nonlinear: parm 50 -> Zeitpunkt 500 ms
wave:      parm 1000 -> Frequenz 3.1416 rad/ms
```

### Und `random` dämpft, es ersetzt nicht

Ravens Handbuch sagt, `random` beginne bei null. Der Quelltext sagt:

```cpp
// Random simply modulates the existing value
perc1 = Q_flrand(0.0f, 1.0f) * perc1;
```

Es multipliziert den vorhandenen Anteil. Mit Faktor 1 ändert sich nichts, mit
Faktor 0 springt der Wert ganz auf `end`. Der Zufallswert wird deshalb
hereingereicht statt intern gezogen — sonst ließe sich nichts davon prüfen.

### Was als Nächstes darauf aufbaut

Die Kurven sind das Fundament. Darauf kommen: der Zeitgeber (Segmente nach
`delay` einplanen, `count`-mal auslösen), die Bewegung
(`velocity`/`acceleration`/`gravity`), und dann erst das Zeichnen.

Alles davon außer dem Zeichnen ist prüfbar.

## Schritt zwei: Zeitplanung und Bahn

Beide aus der Engine hergeleitet, beide ohne Grafikschnittstelle und damit
prüfbar. Zwei Funde dabei, die man der Datei nicht ansieht.

### Die gleichmäßige Verteilung beginnt bei null

`FxScheduler.cpp`:

```cpp
factor = abs( mSpawnDelay.GetMax() - mSpawnDelay.GetMin() ) / (float)count;
...
delay = t * factor;
```

Der **Minimalwert geht nicht ein.** Bei `delay 200 400` und `count 4` ergibt das
`0, 50, 100, 150` — nicht `200, 250, 300, 350`, wie man erwarten würde.

Wer also eine Verzögerung von 200 ms einstellt und „gleichmäßig verteilen"
ankreuzt, verliert die 200 ms. Das steht in keiner Anleitung.

### Die Beschleunigung wirkt doppelt

`CParticle::UpdateOrigin` rechnet die Lage geschlossen statt schrittweise:

```cpp
realVel[2] += 0.5f * mGravity * time;
VectorMA( realVel, time, realAccel, realVel );
VectorMA( org, time, realVel, mOrigin1 );
```

Ausmultipliziert:

| | |
|---|---|
| Schwerkraft | `0.5 · g · t²` — richtig |
| Beschleunigung | `1.0 · a · t²` — **doppelt so weit** |

Raven hat es selbst gesehen. Im Quelltext steht daneben:
*„NOTE: not sure if this is even 100% correct math-wise"*.

Gemessen:

```
Beschleunigung 100 u/s², t = 2 s: 400 Einheiten (saubere Physik ergäbe 200)
```

**Wir bauen es genau so nach.** Das Ziel ist nicht richtige Physik, sondern
dieselbe Bahn wie im Spiel — eine Vorschau, die anders fliegt als das Spiel, ist
schlimmer als keine. Der Test hält den Faktor zwei ausdrücklich fest, damit ihn
niemand später „repariert".

### Das Häkchen in der Segmentliste gehört nicht in die Datei

Beim Bauen fiel auf, dass `Primitive` kein `enabled` hat — zu Recht. Das
Häkchen ist eine reine Vorschau-Einstellung des Editors und steht in keiner
.efx-Datei. Es kommt deshalb als Parameter in die Planung, nicht als Feld in
die Primitive.

### Stand

Damit ist alles Prüfbare an der Simulation fertig: Kurven, Zeitplanung, Bahn.
Was bleibt, ist das Zeichnen — Billboards für Particle, ausgerichtete Vierecke
für OrientedParticle, Linien für Line und Electricity, Röhren für Cylinder.
Das lässt sich hier nicht prüfen, nur bei dir ansehen.

## Schritt drei: die Vorschau bewegt sich

Kurven, Zeitplanung und Bahn sind jetzt zusammengebunden, und die Ansicht
zeichnet, was dabei herauskommt.

```
Lebende bei 0/100/400/1000/2000 ms: 1 2 5 4 0
Groesse der ersten bei 0/500/900 ms: 10.0 5.0 1.0
```

### Gewürfelt wird beim Auslösen, nicht bei jedem Bild

Das ist die Entscheidung, die den größten sichtbaren Unterschied macht. Eine
Größe mit Spanne würde sonst die ganze Lebensdauer zittern, statt einmal
festzustehen.

Genau so macht es die Engine: `GetVal()` wird beim Erzeugen gerufen, und der
gezogene Wert bleibt. Dasselbe gilt für den Zufallswert des `random`-Flags —
bei jedem Bild neu zu würfeln ergäbe Flimmern statt einer gedämpften Kurve.
Deshalb hat jede Kurve ihren eigenen, einmal gezogenen Wert.

### Die Achsen drehen, nicht die Ecken

Ein Billboard mit `rotation` wird gedreht, indem die beiden Spannachsen gedreht
werden — nicht die vier Ecken einzeln. Nur so bleibt es quadratisch. Der Test
prüft es bei 45 Grad: alle vier Ecken gleich weit von der Mitte, beide Seiten
gleich lang.

Die Spannachsen kommen aus der Blickmatrix: ihre ersten beiden Zeilen sind
„rechts" und „oben" in Weltkoordinaten. Damit steht das Viereck immer senkrecht
zur Blickrichtung, ohne dass die Kamera extra gefragt werden müsste.

### Bei jedem Start ein neuer Ausgangswert

Sonst sieht ein Effekt mit Spannen jedes Mal identisch aus — und gerade das
Würfeln will man beurteilen. Für die Tests bleibt der Wert wählbar, damit
derselbe Ablauf zweimal dasselbe ergibt.

### Was jetzt schon stimmt und was noch nicht

**Stimmt:** Zeitpunkte, Lebensdauer, Bahn, Größe, Farbe, Alpha, Drehung.
Segmente ohne Bild (Sound, CameraShake, FxRunner) leben mit, zeichnen aber
nichts — die Statusleiste zeigt beide Zahlen getrennt, und sie standen bis eben
fest auf null.

**Noch nicht:** alles außer Particle und Tail wird als Billboard gezeichnet.
OrientedParticle braucht seine eigene Ausrichtung, Cylinder eine Röhre, Decal
eine Projektion, Electricity das Zacken aus `CElectricity`. Und alles ist
weiß — bis die Texturen kommen, mischt die Vorschau fest additiv, weil das die
Mischung ist, die JKA-Effekte fast immer benutzen.

## Schritt vier: der Auspacker

Der Schlüssel zu den Texturen. Bei einer echten Installation liegt alles in
`.pk3`-Dateien, und deren Inhalte sind deflate-komprimiert — bis eben lasen wir
nur die Verzeichnisse.

### Selbst geschrieben statt zlib eingebunden

Etwa dreihundert Zeilen, das Format ist seit 1996 unverändert, und es hält die
Abhängigkeitsliste des Programms bei **null**. Der Bau soll auf einem frischen
Rechner ohne Vorbereitung durchlaufen — das war von Anfang an die Linie, und
für eine Datei, die sich in einem Nachmittag schreiben lässt, gebe ich sie
nicht auf.

### Geprüft gegen zlib, nicht gegen meine Vorstellung

```
21 Faelle von zlib, zusammen 757506 Byte ausgepackt
```

Die Testdaten erzeugt Pythons zlib, in drei Kompressionsstufen und mit
verschiedenen Inhalten, damit alle drei Blockarten vorkommen: gespeichert,
feste Huffman-Tabellen, eigene Tabellen. Verglichen wird byteweise.

Das ist der Unterschied zwischen „läuft bei meinem Beispiel" und „packt aus,
was ein anderer Packer erzeugt hat".

### Und gegen Böswilliges

Eine `.pk3` liegt im Spielordner, und da kommt alles Mögliche her. Geprüft
gegen: Nullzeiger, Länge null, abgeschnitten mitten im Block, reservierte
Blockart, falsche Längenprüfung, unsinnig große angekündigte Größe — und
**vierhundert zufällige Eingaben**, von denen keine abstürzt oder die
Speichergrenze überschreitet.

Der wichtigste Einzelfall ist der **Rückverweis vor den Anfang**:

```cpp
if (distance > result.data.size()) {
    result.error = "back reference points before the start";
```

Genau damit bricht man einen naiven Auspacker auf — er liest dann fremden
Speicher. Ohne diese eine Zeile wäre das Programm über eine präparierte Datei
angreifbar.

Dazu zwei Dinge, die man leicht übersieht: eine **überbelegte Huffman-Tabelle**
wird abgewiesen (mehr Codes einer Länge, als es geben kann — sonst verläuft man
sich im Baum), und der Kopiervorgang bei Rückverweisen läuft **byteweise**, weil
Quelle und Ziel absichtlich überlappen dürfen. So drückt Deflate Wiederholungen
aus.

### Was davon jetzt sichtbar ist

Shader **innerhalb** von Archiven werden gelesen. Das ging vorher nicht, weil
ihre Namen im Dateiinhalt stehen und nicht im Dateinamen — der Suchlauf trug
einen Hinweis, dass sie fehlen. Der Hinweis ist weg, die Shader sind da.

```
komprimiertes Archiv: Shader gelesen, 5000 Byte ausgepackt
```

Als Nächstes die Bilder selbst: TGA und PNG lassen sich mit dem Auspacker
lesen, JPEG braucht einen eigenen Decoder und wird ein größerer Brocken.

## Ein Test, der nur bei mir lief

Der Bau schlug bei dir fehl, und zwar an meinem eigenen Test:

```
== Auspacken ==
  FEHLER: genug Faelle geprueft
  0 Faelle von zlib, zusammen 0 Byte ausgepackt
```

Der Test las die Deflate-Daten aus `/tmp/deflate`. Dort lagen sie, **weil ich
sie dort erzeugt hatte**. Auf einem anderen Rechner gibt es den Ordner nicht,
der Test findet null Fälle und wird rot.

Das ist kein Testfehler, sondern ein Entwurfsfehler: **ein Test, der von der
Umgebung seines Autors abhängt, prüft anderswo gar nichts.** Er hätte auch
stillschweigend grün sein können, wenn ich die Zählprüfung vergessen hätte —
dann wäre der Auspacker bei dir völlig ungeprüft geblieben.

Dieselbe Falle steckte ein zweites Mal im Materialsuchlauf, mit
`/tmp/realzip`. Beide sind jetzt eingebettet: die gepackten Bytes stehen als
Zahlenfeld im Test, die erwarteten Daten als Erzeugungsvorschrift — sonst wäre
die Testdatei um ein Vielfaches größer als der Auspacker selbst.

Dazu die Warnung `C4244` in `particles.cpp`: eine Initialisierungsliste aus
`int` in eine `uint16_t`-Schleifenvariable. MSVC hatte recht, die Werte gehen
in einen `uint16_t`-Puffer.

## Bilder: erst TGA

`R_ImageLoader_Add` in `tr_image_load.cpp` meldet drei Formate an: `jpg`, `png`
und `tga`. Für Effekttexturen ist TGA das häufigste, weil es einen Alphakanal
ohne Verluste trägt.

Geprüft gegen **Pillow**, nicht gegen mich selbst — sonst prüfte der Test nur,
ob mein Leser zu meinem Schreiber passt. Drei Fassungen derselben zwölf
Bildpunkte: 24 Bit, 32 Bit, und lauflängenkodiert.

Drei Fallen, die alle drei in freier Wildbahn zuschlagen:

- **Targa legt Farben als BGR ab.** Der Klassiker, an dem umgesetzte Bilder
  blau statt rot werden.
- **Die Zeilenrichtung steht in Bit 5 des Deskriptors**, und die Voreinstellung
  ist *von unten nach oben*. Wer das übersieht, bekommt jedes Bild auf dem
  Kopf — und merkt es bei einem Funkenbild nie.
- **Bei 15 und 16 Bit werden fünf Bit auf acht gestreckt**, indem die oberen
  Bits nach unten wiederholt werden. Einfaches Verschieben lässt Weiß bei 248
  enden, und das sieht man bei einem hellen Effekt.

### Und die Formaterkennung schaut in die Datei

In JKA-Mods liegen regelmäßig JPEG-Dateien mit der Endung `.tga`, weil jemand
umbenannt statt umgewandelt hat. Die Engine stolpert darüber; ein Editor sollte
wenigstens sagen, was wirklich drinsteht.

Targa hat als einziges der drei keine Kennung am Anfang — das ist die Schwäche
des Formats. Erkannt wird es an der Fußzeile `TRUEVISION-XFILE` oder, bei
älteren Dateien, an einem plausiblen Kopf.

### Und der Include-Prüfer schlug falschen Alarm

Sobald `image.h` eine `enum class Format` bekam, meldete er
`renderer_d3d11.cpp` — dort steht `desc.Format = DXGI_FORMAT_...`.

Das ist die Kalibrierung, vor der ich beim Bau des Prüfers selbst gewarnt
hatte, und sie hört nie auf: je größer der Quelltext, desto mehr gewöhnliche
Wörter kollidieren. `Format` steht jetzt auf der Ausnahmeliste, zusammen mit
acht weiteren, die aus derselben Runde stammen.

Der Preis ist real: jeder Name auf dieser Liste ist eine echte Meldung, die
verloren geht. Deshalb kommt dort nur etwas hin, das nachweislich anderswo
vorkommt — nicht, was mir gerade unbequem ist. Die Gegenprobe mit `kVersion`
läuft nach jeder Änderung.

## Schritt fünf: die Kette vom Shadernamen zum Bild

Nicht noch ein Decoder — der Weg dazwischen. TGA lag fertig da und **nichts
benutzte es**: `createTexture` wurde in der ganzen Oberfläche kein einziges Mal
gerufen.

Der Weg ist länger, als man denkt:

```
1. "gfx/effects/spark"                       steht in der .efx-Datei
2. Shaderbestand fragen: gibt es den Namen?  -> map-Zeile der ersten Stufe
3. wenn nicht: der Name IST der Bildname     -> die Engine tut dasselbe
4. Endungen durchprobieren                   -> .tga, .jpg, .jpeg, .png, ...
5. ausgepackt oder im .pk3                   -> ausgepackt gewinnt
```

**Schritt vier ist der, an dem Nachbauten scheitern.** In einer `.efx`-Datei
steht *nie* eine Endung, und in einer `map`-Zeile fast nie. Die Reihenfolge ist
dabei nicht beliebig: liegen `spark.tga` und `spark.jpg` nebeneinander, gewinnt
tga, weil die Engine die erste nimmt, die es gibt.

**Schritt zwei ebenso.** Der Shadername ist oft ein ganz anderer als der
Dateiname:

```
Shader gfx/effects/coolflame -> gfx/misc/flame_base.tga
```

Wer den Shadernamen direkt als Dateinamen nimmt, findet in solchen Fällen
nichts — und das sind nicht wenige.

### Ein Zeichenaufruf je Shader, nicht je Partikel

Die Zeichenliste ist jetzt nach Shadername gruppiert. Bei zweihundert Funken
ist das der Unterschied zwischen flüssig und ruckelig, und die Gruppierung
kostet nichts, weil ohnehin über alle gelaufen wird.

```
20 Partikel mit 2 Shadern -> 2 Zeichenaufrufe
```

Ein `map` und kein `unordered_map`: die Reihenfolge soll von Bild zu Bild
dieselbe sein. Bei additiver Mischung fällt das nicht auf, bei alphagemischten
Flächen sehr wohl — dann flackert die Verdeckung.

### Auch das Scheitern wird gemerkt

Der Texturspeicher merkt sich fehlgeschlagene Versuche mit `kNoTexture`. Ohne
das versucht die Vorschau **sechzigmal je Sekunde**, dieselbe fehlende Datei zu
öffnen.

### Und ein Leck, das ich beinahe wegerklärt hätte

Beim Neueinlesen des Materialbestands wollte ich den Texturspeicher leeren.
Geschrieben hatte ich dazu: *„Der Renderer kommt hier nicht her, deshalb nur die
Zuordnung leeren — die Texturen selbst räumt das nächste Bild ab."*

Das stimmte nicht. Die Zuordnung zu leeren, ohne freizugeben, lässt die
Texturen im Grafikspeicher liegen, ohne dass jemand sie noch kennt — ein Leck,
und mein Kommentar hätte es als Absicht getarnt.

Jetzt wird nur vorgemerkt und im nächsten Bild freigegeben, wo der Renderer
bekannt ist. Ein Kommentar, der eine Unsauberkeit begründet statt sie zu
beheben, ist schlimmer als gar keiner.

## Der vierte Baufehler, die dritte Fehlerklasse

`diag::warning(...)` — die Funktion heißt `warn`.

Weder der Memberprüfer noch der Einbindungsprüfer sieht so etwas: der eine
kennt nur Namen mit Unterstrich am Ende, der andere nur Konstanten und
`enum class`-Namen. Ein erfundener **Funktionsname** in einem eingebundenen
Namensraum fällt zwischen beide.

Also ein vierter Prüfer. Er sammelt aus `include/efx/`, welche freien
Funktionen jeder Namensraum anbietet, und hält alle Aufrufe der Form
`raum::name(` in `gui/*.cpp` dagegen. Bei einem Treffer schlägt er den
ähnlichsten Namen vor:

```
app.cpp
  Zeile 268: diag::warning() gibt es nicht  gibt es: warn
```

### Und er hatte im ersten Anlauf selbst einen Fehler

Er meldete `jobs::pool()` als unbekannt, obwohl es die Funktion gibt. Ursache:
der Zähler, der Klassenrümpfe überspringt, suchte nach `};` in derselben Zeile.
Bei einer Klasse, die anders schließt, blieb er für den Rest der Datei auf 1
stehen — und alles danach wurde übersehen.

Jetzt wird an geschweiften Klammern gezählt. Das ist die dritte Runde
Kalibrierung an diesen Prüfern, und die Lehre bleibt dieselbe: **ein Prüfer,
der Richtiges anmeckert, wird ignoriert** — und dann fängt er auch das Falsche
nicht mehr.

### Die vier zusammen

| | |
|---|---|
| `lint_i18n.py` | Oberflächentexte ohne `tr()` |
| `lint_members.py` | Membernamen, die nirgends deklariert sind |
| `lint_includes.py` | benutzte Namen, deren Kopf fehlt |
| `lint_calls.py` | Aufrufe von Funktionen, die es nicht gibt |

Alle vier haben dieselbe Ursache: `gui/` lässt sich nur auf einem
Windows-Rechner mit ImGui übersetzen. Was hier nicht übersetzt wird, muss
anders geprüft werden — oder es geht als Baufehler an den Benutzer, und das ist
viermal passiert.

Was sie weiterhin **nicht** fangen: Typfehler, falsche Argumentzahlen, falsche
ImGui-Signaturen. Dafür bräuchte es einen Übersetzer mit den echten Köpfen.

## Schritt sechs: die Mischung kommt aus dem Shader

Bis eben mischte die Vorschau fest **additiv**. Der Parser las `blendFunc`
schon, der Renderer kannte fünf Mischungen — es fehlte nur die Verbindung. Ein
alphagemischter Rauch sah additiv gemischt völlig falsch aus.

### Runden statt scheitern

JKA-Shader schreiben mehr Faktorenkombinationen, als eine Vorschau braucht:

```
28 von 144 Faktorenpaaren genau getroffen, der Rest gerundet
```

Das ist Absicht. Eine Vorschau muss nicht jede Kombination exakt treffen, sie
muss den **Charakter** treffen: ein additiv gemischter Funke und ein
alphagemischter Rauch sehen völlig verschieden aus; ob der Funke mit
`GL_ONE GL_ONE` oder `GL_SRC_ALPHA GL_ONE` gemischt ist, sieht man kaum.

Maßgeblich ist der **Zielfaktor**: bleibt das Vorhandene erhalten (`GL_ONE`),
wird aufgehellt. Wird es mit dem Quellalpha verrechnet, ist es eine
Alphamischung.

`blendModeOf` meldet über `exact`, ob gerundet wurde — damit die Prüfung
später sagen kann, dass die Vorschau eine Mischung nur annähert.

Der Test geht **alle 144 Paare** durch und prüft, dass keines ohne Ergebnis
bleibt. Ohne das könnte eine Stufe stumm durchfallen und gar nicht gezeichnet
werden.

### Reihenfolge und Tiefenschreiben

Zwei Dinge, die erst mit echten Mischungen wichtig werden:

- **Erst die undurchsichtigen, dann die durchsichtigen.** Bei additiver
  Mischung ist die Reihenfolge gleichgültig — Addition ist kommutativ. Bei
  Alphamischung nicht: eine undurchsichtige Fläche nach einer durchsichtigen
  überdeckt sie vollständig.
- **Nur undurchsichtige schreiben in den Tiefenpuffer.** Sonst verdecken sich
  durchsichtige Partikel gegenseitig, und man sieht Löcher statt Rauch.

### Zwei Aufzählungen, die zusammenbleiben müssen

`shader::BlendMode` und `render::Blend` werden direkt ineinander umgesetzt.
Weichen sie ab, mischt die Vorschau stillschweigend falsch — und bei einem
Funken fällt das kaum auf.

Fünf `static_assert` halten sie zusammen. Sie stehen in `renderer.cpp` und
nicht im Kopf, weil `renderer.h` sonst `shader.h` einbinden müsste, nur um eine
Behauptung aufzustellen.

### Und der Einbindungsprüfer hat vorher gegriffen

Ich hatte `shader::BlendMode` in `app.cpp` benutzt, ohne `shader.h`
einzubinden — derselbe Fehler wie bei `kVersion`, der vier Runden zuvor als
Baufehler bei dir gelandet war.

Diesmal meldete ihn der Prüfer hier, bevor das Paket rausging. Das ist das
erste Mal, dass einer der vier tatsächlich etwas abgefangen hat, statt nur
nachträglich zu belegen, dass er es gekonnt hätte.

## Zwei Regler, die nichts taten — und die gewünschten Raumarten

### Die Wiedergabegeschwindigkeit war nie verdrahtet

Der untere Regler in der Werkzeugleiste ist `timeScale`. Er wurde angezeigt,
gespeichert, beim Start wieder eingelesen — und **nirgends benutzt**. Dieselbe
Sache wie beim Windvektor und bei den Statuszahlen: ein Bedienelement, das nach
Funktion aussieht und keine hat.

Jetzt geht er in die verstrichene Zeit, nicht in die Zeitplanung: derselbe
Effekt läuft langsamer ab, statt anders geplant zu werden. Zeitlupe soll
zeigen, was ohnehin passiert. Wie im Original bedeutet 0.50 halbe
Geschwindigkeit.

Die Windfahne läuft mit derselben Uhr — bei Zeitlupe weht sie jetzt auch
langsamer.

### Raumarten

Wie gewünscht, mit dem geschlossenen Kasten als Voreinstellung:

| | |
|---|---|
| **Geschlossener Raum** | Boden, vier Wände, Decke — wie im Original |
| **Boden und Himmel** | nur der Boden, darüber eine Himmelskuppel |
| **Nichts** | freier Blick |

Der geschlossene Kasten bleibt die Voreinstellung, weil er sofort ein Gefühl
für Maßstab gibt: Wände und Decke stehen in bekanntem Abstand.

Draußen bleibt ein **niedriger Sockel** statt der Wände:

```
Raum: geschlossen 320 hoch, draussen 8 (Sockel)
```

Ohne ihn schwebt der Boden im Nichts, und man verliert das Gefühl für die Höhe.

### Der Himmel kommt ohne Textur aus

Eine Kuppel mit eingebackenem Verlauf, 192 Flächen. Ein Himmel aus dem
Spielordner wäre schöner, aber er hinge am Spielpfad — und die Vorschau soll
auch ohne gesetzten Pfad brauchbar sein.

Die Ringe sind **quadratisch verteilt**: am Horizont dichter, weil dort der
Verlauf am stärksten ist und Kanten am ehesten auffallen.

Der Sonnenhof benutzt einen hohen Exponenten (`cos²⁴`). Ein flacherer ließe den
halben Himmel leuchten, und man sähe die Richtung nicht mehr.

### Sonnenlicht in die Eckpunkte gebacken

Der Renderer hat keine Beleuchtung, also rechnet die Geometrie sie aus —
dasselbe Verfahren, mit dem `buildRoom` schon Boden, Wände und Decke
unterscheidet.

Lambert mit einem **Grundanteil**, damit abgewandte Flächen nicht schwarz
werden: im Spiel gibt es immer Streulicht, und eine schwarze Wand verrät nichts
über den Effekt davor.

### Höhe und Himmelsrichtung statt dreier Zahlen

Ein Richtungsvektor ist genau, aber niemand denkt in Vektoren, wenn er die
Sonne tiefer stellen will. Zwei Winkel treffen die Vorstellung — und ein
Vektor, den man aus Winkeln baut, ist immer normiert.

### Der Himmel gehört ins Thema

Feste Himmelsfarben wären über der Ansicht von „Hoher Kontrast" so grell, dass
man den Effekt davor nicht mehr sieht. Sie werden deshalb aus der Wandfarbe
abgeleitet und dabei ins Blaue gezogen — sonst verschwimmt der Horizont mit dem
Boden.

Ein Test prüft für alle sechs Themen: Horizont nicht schwarz, Zenit nicht
schwarz, beide sichtbar verschieden, und der Zenit zieht ins Blaue.

## Die Werkzeugleiste, aus dem Binary gelesen

Statt aus den Bildern zu raten, habe ich die `RT_TOOLBAR`-Ressourcen dekodiert
und die Befehlsnummern gegen die Zeichenkettentabelle aufgelöst. Damit steht
zu jedem Knopf sein Statuszeilentext da — und der sagt präziser, was er tut,
als jeder Name.

```
Leiste 128:  New | Open | Save | Clone | About
Leiste 155:  New Segment | Delete Segment
Leiste 158:  Play | Pause | Stop | Playback Settings |
             Orient Up | Sideways | Down | SetOrigin
Leiste 167:  Draw Axes | Draw Room | Draw Grid |
             Draw Textured | Wireframe | Overdraw | Wind Vector
```

### Zwei Dinge hatte ich falsch verstanden

**„Draw Room" schaltet nur die Wände.** Der Hilfetext lautet
*„Draw the walls of the testing room"* — der Boden bleibt stehen. Ich hatte den
Knopf als „ganzer Raum" gelesen und damit auch den Boden mit abgeschaltet.

Das ist die bessere Lösung, und man sieht sofort warum: der Boden ist der
Bezug, an dem man Höhe und Entfernung abliest. Ohne ihn schwebt alles.

**„Draw Textured", „Wireframe" und „Overdraw" betreffen die Effekte, nicht den
Raum.** Die Hilfetexte sagen es wörtlich: *„Renders **effects** with textures
active"*, *„**Effects** are rendered in wireframe"*. Ich hatte Wireframe auf den
Raum gelegt.

Es sind dieselben drei, die im Ansichtsmenü unter „Effect Rendering" stehen —
nur zusätzlich als Knöpfe, von denen immer genau einer gedrückt ist.

**Und „Draw Grid" gab es bei mir gar nicht.** Das Gitter hing am Raum;
*„Draw outline of testing room"* ist ein eigener Schalter.

### Overdraw

*„Shows areas of high polygon overlap"* — die Frage, die der Modus beantwortet,
ist: **wo kostet dieser Effekt Bilder?**

Umgesetzt, indem jede Fläche als schwaches gleichmäßiges Additiv ohne Textur
gezeichnet wird. Wo viele Flächen übereinanderliegen, summiert sich das zu
Weiß. Ein Schritt von 21 bedeutet: zwölf Überdeckungen ergeben Weiß — das ist
etwa die Schwelle, ab der es im Spiel wehtut.

### Ein Zeiger, der nicht überlebt hätte

Beim Umfärben für Overdraw hatte ich zuerst
`overdrawMesh(mesh).vertices.data()` geschrieben — ein Zeiger auf ein
temporäres Objekt, das noch vor dem Zeichenaufruf zerstört wird.

Die umgefärbte Fassung liegt jetzt in einem Member. Das überlebt den Aufruf und
spart nebenbei die Neubelegung je Bild.

### Die Hilfetexte sind übernommen

Alle vierzehn, übersetzt in die vier Sprachen. Sie sind besser als alles, was
ich mir ausgedacht hätte — Raven hat gewusst, was ihre Knöpfe tun.

## „Die Fahne folgt der Kamera"

Sie folgt ihr nicht — die Funktion, die sie baut, bekommt gar keine Kamera
übergeben, und zweimal dieselbe Zeit ergibt zweimal dieselbe Geometrie. Das
prüft jetzt ein Test.

Der Eindruck hatte trotzdem einen echten Grund, und es waren sogar **zwei**:

**Sie stand im Ursprung — dem Zielpunkt der Umlaufkamera.** Beim Drehen bleibt
der Ursprung in der Bildmitte stehen, und damit die Fahne. Das sieht aus, als
hinge sie an der Kamera, obwohl sie stillsteht.

**Und der Ursprung ist genau die Stelle, an der der Effekt entsteht.** Eine
Anzeigehilfe, die mitten in dem steht, was man beurteilen will, ist keine
Hilfe.

Beides löst dieselbe Änderung: die Fahne gehört an den Rand.

```
Fahne steht bei -184 / -276 statt im Ursprung
```

Bei etwa drei Vierteln zur Ecke hin, nicht ganz hinein — dort verschwände sie
hinter der Wand, sobald man von außen schaut. Der Test prüft beides: weit genug
weg vom Ursprung, aber innerhalb des Raums.

### Warum das ohne dein Bild schwer zu finden war

Ich hätte die Geometrie hundertmal nachrechnen können, ohne etwas zu finden —
sie war ja richtig. Der Fehler lag darin, **wo** ich sie hingestellt habe, und
das sieht man erst, wenn man die Kamera dreht.

Das ist die Art Beobachtung, die aus dem Benutzen kommt und nicht aus dem
Quelltext.

## Vollständiger Abgleich mit dem Original

Alle 49 Menübefehle und 14 Werkzeugleistenknöpfe aus dem Binary, gegenüber-
gestellt mit dem, was bei uns **tatsächlich etwas tut**. Steht in
`ABGLEICH.md`.

Der Unterschied ist der ganze Punkt: ein Menüpunkt, der eine Einstellung
setzt, die niemand liest, sieht aus wie eine Funktion und ist keine.

### Ein Befehl mit zwei Namen

`32870` heißt im Menü **„Draw Wireframe"**, in der Werkzeugleiste **„Draw
Grid"** — derselbe Befehl. Sein Hilfetext sagt, was er wirklich tut: *„Draw
outline of testing room"*.

Bei mir waren es **zwei getrennte Einstellungen**, und keine tat, was ihr Name
versprach: `drawWireframe` zeichnete die Wände als Drahtgitter, `drawGrid` das
Bodengitter. Zusammengelegt. Der alte Schlüssel wird beim Lesen der
Einstellungen weiter erkannt.

### Fünf Befehle ohne Wirkung — drei davon behoben

**Orient Up / Sideways / Down.** Die Einstellung wurde gesetzt, gespeichert,
beim Start wieder eingelesen — und die Effektachse nie gedreht.

Jetzt wird alles, was in den Eigenschaftsseiten „Forward / Right / Up" heißt,
damit verrechnet: `org = ax[0]·x + ax[1]·y + ax[2]·z`, wie in
`FxScheduler.cpp`. Gemessen:

```
Ausrichtung nach 1 s: oben z=100, seitwaerts x=100, unten z=-100
```

**Segment Enabled.** Die Maske gab es, sie wurde an die Simulation gereicht —
es fehlte die Stelle, an der man sie umschaltet. Jetzt ein Häkchen links vom
Namen in der Segmentliste, wie im Original, und es wirkt sofort: sonst müsste
man neu starten, um zu sehen, was ein Segment beiträgt, und genau dafür ist es
da.

**Pause.** Setzte eine Zahl, die niemand las — Pause und Stop taten dasselbe.
Jetzt hält Pause die Uhr an und merkt sich die vergangene Zeit; beim Fortsetzen
wird sie abgezogen. Ohne das springt der Effekt an die Stelle, an der er ohne
Pause wäre.

### Und ein Test, der zu eng gemessen hat

Der Tail-Test prüfte die Länge **entlang der X-Achse**. Das ging, solange die
Effektachse die Einheitsachse war. Seit „Orient Up" wirklich dreht, zeigt
„vorwärts" nach oben, und die Messung entlang X ergab null.

Der Test war zu eng, nicht die Änderung falsch: gemessen werden sollte die
Länge, nicht eine Achse davon. Jetzt in drei Dimensionen, plus eine Prüfung,
dass der Schweif entgegen der Flugrichtung zeigt.

Das ist inzwischen der vierte eigene Test, der eine Annahme festhielt statt
einer Tatsache. Sie fallen zuverlässig auf, sobald die Annahme nicht mehr gilt
— was für sie spricht, aber sie waren trotzdem zu eng geschrieben.

## Aufräumrunde: ein Übersetzer für gui/

Viermal habe ich dir einen Baufehler geschickt, alle vier in `app.cpp`. Die
vier Prüfer in `tools/lint_*.py` fangen Tippfehler und vergessene
Einbindungen — aber keine Typfehler, keine falschen Argumentzahlen, keine
falschen ImGui-Signaturen.

Es stellte sich heraus, dass das leichter geht als gedacht: **`app.cpp` bindet
keine Windows-Köpfe ein**, nur ImGui und unsere eigenen. Damit lässt sich genau
die Datei, in der alle vier Fehler steckten, hier vollständig übersetzen.

`tools/check_gui.sh` holt ImGui v1.91.5 und ruft `g++ -fsyntax-only`. Kein
lauffähiges Programm — gebaut wird ohnehin auf dem Zielrechner.

Die Gegenprobe mit drei künstlich eingebauten Fehlern:

| Fehler | vier Prüfer | Übersetzer |
|---|---|---|
| falscher Membername | gefunden | gefunden |
| falsche Argumentzahl | **übersehen** | gefunden |
| falscher Typ | **übersehen** | gefunden |

### Und dabei ist etwas aufgefallen

Die Arbeitskopie und das ausgelieferte Paket **rc18** waren in `validate.cpp`
auseinandergelaufen. Die Arbeitskopie hatte die bessere Fassung — alle 31
Meldungen der Prüfung übersetzt, über eine `message()`-Hilfsfunktion mit
eingesetzten Werten. Das erklärt, wo die „51 nicht übersetzten Texte"
geblieben sind, die ich rundenlang als offenen Punkt mitgeführt habe.

Verloren war dabei die Warnung *„abweichendes Ende ohne Übergangsart"* samt
ihren drei Tests. Beide sind wieder da, die Meldung jetzt in allen vier
Sprachen.

Ich kann nicht rekonstruieren, wie die beiden Stände auseinandergelaufen sind.
Was ich tun konnte: **beide vergleichen, den besseren nehmen, das Fehlende
nachtragen und es hier aufschreiben** — statt es stillschweigend
geradezuziehen.

Der Vergleich lief über alle Kennzeichen der letzten zehn Runden
(`axisFor`, `windFlagPosition`, `buildSky`, `blendModeOf`, `readFromZip`,
`decodeTga`, `inflate`, `togglePause`, `segmentEnabled_`); nur `validate.cpp`
wich ab.

### Damit ist die Übersetzung vollständig

```
0 nicht uebersetzte Oberflaechentexte
```

366 Texte in vier Sprachen. Der Prüfer kennt jetzt eine kurze Ausnahmeliste für
Produktnamen — `EffectsEd`, `OpenGL`, `Direct3D` werden nicht übersetzt.

## Die übrigen Primitivtypen

Bis eben wurde alles außer Particle und Tail als Billboard gezeichnet. Ein
Cylinder als weißes Rechteck ist irreführender als eine fehlende Textur — dort
weiß man wenigstens, dass nur das Bild fehlt.

### Der Blitz, Zeile für Zeile aus dem Renderer

Das Zacken steht nicht im Effektcode, sondern in `RB_SurfaceElectricity`
(`tr_surface.cpp`). `CElectricity::Draw` reicht nur `mChaos` in den Winkeln
weiter.

```
alle 20 Einheiten ein Punkt
Abweichung = zufall*3 entlang der Achse
           + zufall*7*chaos quer dazu (zwei Richtungen)
die Abweichungen summieren sich über die Kette
danach: cur = start+off, linear nach end interpoliert
```

Zwei Dinge daran, die man nicht raten würde:

**Chaos wirkt nur quer.** Entlang der Achse steht fest `*3`, ohne Chaos. Ravens
Kommentar dazu: *„chaos also does not affect this"*.

**Das Zurückziehen auf die Gerade ist der Trick.** Die Abweichungen summieren
sich frei — trotzdem sitzen beide Enden exakt, weil der Punkt danach zwischen
Start und Ende interpoliert wird. Ravens Kommentar: *„by nature, we always move
from exactly start....to end"*. Der Test prüft genau das: Anfang und Ende auf
0.5 Einheiten genau, dazwischen aber deutliche Abweichung.

```
Blitz ueber 200 Einheiten: 10 Segmente, groesste Abweichung 6.8
```

Der Ausgangswert hängt am Partikel und wechselt etwa alle 50 ms. Bei jedem Bild
neu würfeln ergäbe Flimmern statt Zappeln; gar nicht wechseln ergäbe einen
starren Zickzack.

### Wo die Unruhe steht

Nicht in einem Feld namens `chaos` — das gibt es nicht. Die Engine liest
`mElasticity.GetVal()`, **dasselbe Feld**, das bei anderen Typen den Abpraller
beschreibt und beim Kamerawackeln die Stärke. In der Datei heißt es je nach Typ
`bounce`, `intensity` oder gar nichts.

Das ist keine Schönheit, aber es ist, was dasteht.

### Zylinder und ausgerichtete Vierecke

```
Zylinder: 64 Ecken statt 4
```

Der Zylinder ist ein Mantel aus sechzehn Segmenten, unten `size`, oben `size2`,
`length` hoch — ohne Deckel, weil die Engine auch keine zeichnet. Die Textur
läuft einmal herum.

OrientedParticle und Decal bekommen ein Viereck in eigener Lage statt zur
Kamera. Das ist der ganze Unterschied zu Particle: ein Billboard dreht sich
immer zum Betrachter, ein ausgerichtetes Viereck bleibt liegen — und ist von
der Seite ein Strich.

### Was weiterhin fehlt

**Emitter und FxRunner** sollen unterwegs weitere Effekte aussenden. Das ist
kein Zeichenproblem, sondern eines der Struktur: dafür muss die Vorschau andere
`.efx`-Dateien nachladen und rekursiv abspielen — mit einer Tiefenbegrenzung,
sonst hängt sich ein Effekt auf, der sich selbst startet.

## JPEG, selbst geschrieben

Wie der Auspacker: das Projekt hat bis hierhin keine einzige Abhängigkeit, und
ein JPEG-Decoder lässt sich gegen fremde Referenzdaten genauso sauber prüfen
wie Deflate.

Gemessen gegen **Pillow**, vier Fassungen desselben Bildes:

```
4:4:4       groesste Abweichung von Pillow: 0 Stufen
4:2:2       groesste Abweichung von Pillow: 3 Stufen
4:2:0       groesste Abweichung von Pillow: 7 Stufen
Graustufen  groesste Abweichung von Pillow: 0 Stufen
```

Ohne Unterabtastung **exakt**. Die Abweichung bei 4:2:2 und 4:2:0 kommt vom
Hochrechnen der Farbkanäle: wir wiederholen den Wert, Pillow interpoliert. Das
sind ein paar Stufen an Farbkanten und sonst nichts.

Das Testbild hat einen Farbverlauf **und** harte Kanten — beides fordert den
Decoder auf andere Weise.

### Der Spielraum ist Absicht

Verglichen wird mit 12 Stufen Toleranz. Zwei JPEG-Decoder liefern nie Byte für
Byte dasselbe, weil die inverse DCT in Gleitkomma gerechnet wird und jeder
anders rundet.

Zwölf Stufen trennen „rundet anders" sauber von „dekodiert falsch": bei einem
echten Fehler liegen die Werte um **Hunderte** daneben, nicht um zehn. Ein
Test, der Byte-Gleichheit fordert, wäre bei jedem Compilerwechsel rot — und
einer mit 128 Stufen Toleranz würde nichts mehr merken.

### Drei Stellen, an denen man es falsch macht

**Die eingeschobenen Nullen.** Ein `0xFF` im Datenstrom wird als `0xFF 0x00`
geschrieben, damit es nicht mit einer Abschnittskennung verwechselt wird. Beim
Lesen muss die Null wieder verschwinden — sonst verschiebt sich alles ab dem
ersten hellen Bildpunkt.

**Die Zahlendarstellung.** Werte stehen als `length` Bits, und der obere
Halbbereich zählt negativ. Ohne die Umrechnung ist jeder Wert um die Hälfte
daneben, und das Bild sieht aus wie Rauschen mit Struktur.

**Der Gleichanteil ist eine Differenz** zum vorigen Block, nicht ein absoluter
Wert. Wer das übersieht, bekommt ein Bild, das nach rechts unten immer dunkler
wird.

### Eine bewusst langsame Stelle

Die inverse DCT rechnet mit einer Kosinustabelle statt einer schnellen
Variante. Sie ist nachvollziehbar, und der Unterschied fällt beim Laden einer
Textur nicht auf. Eine schnelle DCT einzubauen, ohne sie prüfen zu können, wäre
die schlechtere Wahl gewesen.

### Progressive JPEGs werden abgewiesen

Sie brauchen einen völlig anderen Dekodierweg und kommen in Spieldaten
praktisch nicht vor — ihr Vorteil ist ein früher Vorschau-Aufbau beim Laden
über eine langsame Leitung, was für eine Textur sinnlos ist.

Eine solche Datei wird **beim Namen genannt**, statt Müll zu liefern. Der Test
baut sich eine, indem er `SOF0` zu `SOF2` ändert.

### Und gegen Böswilliges

Hundertfünfzig zufällig verbogene JPEGs, keines stürzt ab oder schreibt über
die Bildgröße hinaus. Dazu abgeschnittene Dateien an drei Stellen: der Decoder
hört auf und liefert, was er hat, statt in den Speicher daneben zu greifen.

## Rückgängig, Wiederherstellen, Klonen

### Abzüge statt einzelner Änderungen

Rückgängig arbeitet mit Abzügen des ganzen Effekts. Das ist die unelegantere
Bauart, aber die richtige hier: eine `.efx`-Datei hat höchstens 24 Primitiven
mit je ein paar Dutzend Zahlen — ein Abzug sind wenige Kilobyte.

Einzelne Änderungen zu verfolgen hieße, für jedes Feld eine eigene
Rückgängig-Klasse zu schreiben, und ein vergessenes Feld fiele erst auf, wenn
jemand es rückgängig machen will und **nichts passiert**. Bei einem Programm,
in dem man an Zahlen dreht, ist das der falsche Ort für Eleganz.

### Ein Merkzeichen als Warnsignal

Der erste Entwurf merkte den Zustand *vor* einer Änderung. Das klingt
natürlicher, führt aber dazu, dass der jetzige Stand nirgends steht — und beim
ersten Rückgängig nachträglich angehängt werden muss, damit das
Wiederherstellen irgendwohin zurückführen kann.

Dafür brauchte es ein `holdsCurrent_`-Merkzeichen, und Merkzeichen dieser Art
sind fast immer ein Zeichen, dass die Bauart nicht stimmt. Neu geschrieben:
`reset` legt den Anfangszustand ab, `record` den Zustand *nach* jeder
Änderung. Die Liste hält immer den aktuellen Stand an der aktuellen Stelle,
und beides ist zwei Zeilen lang.

Geprüft wird auch der unangenehme Fall:

```
Grenze 4: nach 20 Aenderungen 4 Zustaende, 3 Schritte zurueck
```

Fällt vorne etwas heraus, bleibt die Liste benutzbar. Und dass der Inhalt
wirklich kopiert wird, nicht nur verwiesen — sonst zeigte der Verlauf auf einen
Effekt, der sich unter ihm ändert.

### Klonen sucht einen freien Namen

Nicht blind eine 2 anhängen: bei zweimal Klonen stünden dort zwei gleichnamige
Segmente, und die Prüfung meldet das zu Recht. Gesucht wird die erste freie
Nummer, und der Name wird auf 31 Zeichen gekürzt — das ist die Grenze des
Parsers.

### Der Übersetzer hat sich sofort bezahlt gemacht

Bei dieser einen Änderung hat `tools/check_gui.sh` **zwei** Fehler gefangen,
bevor das Paket rausging:

- `settings_.dialect` — gibt es nicht; der Dialekt steht vorerst fest in
  `refreshDiagnostics`
- vier Übersetzungskennungen, die ich benutzt, aber nicht erzeugt hatte

Beide wären als Baufehler bei dir gelandet. Das ist genau die Sorte, die
viermal durchgerutscht ist.

## Emitter und FxRunner starten andere Effekte

Kein Zeichenproblem, sondern eines der Struktur: die Vorschau muss andere
`.efx`-Dateien nachladen und rekursiv abspielen.

### Der Emitter sendet nach Strecke aus, nicht nach Zeit

Das ist die Zahl, die man nicht raten kann. `CEmitter::UpdateEmitter`:

```cpp
if ( DistanceSquared( org, mOldOrigin ) >= step )
{
    step = mDensity + Q_flrand(-1.0f, 1.0f) * mVariance;
    step *= step;
    theFxScheduler.PlayEffect( mEmitterFxID, org, mRefEnt.axis );
```

Der Abstand zum letzten Aussenden wird mit `density ± variance` verglichen —
ein Emitter, der schnell fliegt, sendet also häufiger aus als einer, der
langsam fliegt, obwohl beide gleich lange leben. Gemessen:

```
Emitter: 100 Einheiten Weg, density 25 -> 3 Aussendungen
```

Abgeschritten wird in **festen Zeitschritten** von 10 ms, nicht in der
Bildrate. Sonst hinge das Ergebnis davon ab, wie schnell der Rechner ist, und
die Vorschau sähe auf jedem Rechner anders aus.

### Der FxRunner steht nicht in der Liste

Er startet einen anderen Effekt und ist damit fertig — genau so macht es die
Engine, wo der Scheduler für `Fx_RUNNER` direkt `PlayEffect` ruft und keine
Primitive anlegt.

```
FxRunner: 1 Effekt gestartet, Kind bei 100 ms
```

Ein alter Test erwartete, dass ein FxRunner „lebt". Das war schon vorher
falsch, fiel aber nicht auf, weil ohne Lader ohnehin nichts passierte — der
fünfte eigene Test, der eine Annahme festhielt statt einer Tatsache.

### Ein Effekt, der sich selbst startet

In `.efx`-Dateien nicht verboten. Ohne Grenze hängt sich die Vorschau daran
auf:

```
Selbstaufruf: bei Tiefe 4 abgebrochen nach 4 Starts
```

Vier Ebenen sind mehr, als in echten Dateien vorkommt. Dazu eine zweite Grenze
beim Emitter: 256 Aussendungen je Primitive — ein Emitter mit `density 0.01`
und langer Lebensdauer würde sonst Tausende Effekte starten.

### Der Kern weiß nichts vom Dateisystem

Der Lader wird als Rückruf hereingereicht. Die Oberfläche speichert die
geladenen Effekte zwischen — **auch das Scheitern**, sonst wird bei jedem Start
wieder vergeblich gesucht, und ein Emitter fragt denselben Namen zwanzigmal.

Die Zeiger müssen das ganze Abspielen überleben; eine lokale Variable wäre
dafür der falsche Ort.

## Klang — und eine Abhängigkeit, die erste

Der Fehler, den du am Original gefunden hast: EffectsEd importiert nur
`PlaySoundA`, und das kann kein MP3. Jeder Effekt mit einem MP3 bleibt im
Editor stumm, obwohl er im Spiel klingt.

### WAV selbst, MP3 nicht

**WAV** ist ein paar Dutzend Zeilen und vollständig prüfbar — geschrieben,
geprüft gegen selbst gebaute Referenzdateien in 8, 16, 24 und 32 Bit.

**Für MP3 liegt jetzt `third_party/minimp3.h` bei.** Das ist die erste fremde
Datei im Baum, und die Entscheidung fiel nicht leicht: bis hierhin hatte das
Projekt keine einzige Abhängigkeit, und Deflate und JPEG habe ich selbst
geschrieben.

Ein MP3-Decoder ist eine andere Größenordnung — Huffman, IMDCT,
Polyphasen-Synthesebank, Bit-Reservoir. Rund zweitausend Zeilen, bei denen ein
Fehler nicht als falsche Farbe auffällt, sondern als Rauschen, das vielleicht
nur bei bestimmten Bitraten auftritt.

minimp3 ist gemeinfrei (CC0, **keine Auflagen**), eine einzige Datei ohne
eigene Abhängigkeiten, ISO-konform geprüft. Übersetzt mit `MINIMP3_NO_SIMD`,
damit das Ergebnis auf jedem Rechner gleich ist.

### Geprüft gegen ffmpeg

Die Referenzdateien sind ein Sinus mit **Pegelwechsel in der Mitte**. Bei einem
gleichmäßigen Ton fällt es nicht auf, wenn die Hälfte fehlt oder die
Reihenfolge stimmt nicht.

```
WAV mono16: 22050 Hz, 1 Kanal, 150 ms
Pegel: erste Haelfte 23999, zweite 7499
MP3: 22050 Hz, 1 Kanal, 235 ms (WAV: 150 ms)
MP3: 76 ms Vorlauf, hoerbar von 1682 bis 4987, Pegel 22934 dann 7180
```

**Der Vorlauf ist der Fallstrick.** Mein erster Test halbierte die MP3-Datei
und maß beide Hälften — und bekam gleiche Pegel, weil der Kodierer vorn 76 ms
Stille anhängt. Gemessen wurde Stille gegen Signal.

Jetzt sucht der Test erst den hörbaren Bereich und halbiert **den**. Dazu ein
Abstand zur Mitte, weil der Übergang beim MP3 verschmiert ist: ein Rahmen
umfasst 1152 Abtastwerte, und der Pegelwechsel liegt mitten in einem.

### Acht Bit sind vorzeichenlos

Als einzige Breite. Wer das übersieht, bekommt ein lautes Knacken und ein
Signal, das nur die obere Hälfte nutzt. Der Test prüft, dass der Sinus bei null
beginnt und trotzdem voll aussteuert.

### Und die WAV-Abschnitte stehen nicht an festen Stellen

Wer annimmt, dass `fmt` bei Byte 12 und `data` bei 36 steht, scheitert an jeder
Datei mit einem `LIST`-Abschnitt — und die sind häufig. Also durchgehen statt
rechnen.

## Was ich hier nicht prüfen konnte

Die **Ausgabe** über `waveOut`. Auf dem Rechner, auf dem der Quelltext
entsteht, gibt es kein Tongerät.

Der Klangleser ist vollständig geprüft: Formaterkennung, alle Bittiefen,
Stereo, MP3, zweihundert verbogene WAVs und hundert verbogene MP3s. Nur das
Abspielen ist es nicht — das steht auch so im Kopf von `audio_win32.h`.

Gewählt habe ich `waveOut` statt WASAPI oder einer Bibliothek: seit Windows 3.1
unverändert, kein COM, keine Initialisierung, keine zusätzliche Datei. Für ein
paar Effektklänge reicht es.

Drei Dinge, die auffallen könnten:

- Ein Klang wird **einmal** angestoßen, nicht in jedem Bild. Das Merkzeichen
  wird gesetzt, *bevor* gespielt wird — schlägt es fehl, soll es nicht in
  jedem Bild erneut versucht werden.
- Beim Neustart bricht `stopAll` alles Laufende ab, sonst überlagern sich die
  Klänge zweier Durchläufe.
- Höchstens sechzehn gleichzeitig. Ein Effekt, der hundert Klänge auf einmal
  startet, ergibt sonst nur Krach.

### Der Memberprüfer musste nachziehen

Er las nur `app.h` und meldete die zweiundzwanzig Member von
`audio_win32.cpp` als unbekannt — die stehen in `audio_win32.h`. Jetzt liest er
zusätzlich den gleichnamigen Kopf jeder Datei. Gegenprobe wie immer: alten
Fehler eingesetzt, gemeldet, zurückgesetzt, still.

## deathFx: drei Bedingungen, die man alle falsch rät

Wenn eine Primitive stirbt, kann sie einen weiteren Effekt starten. Das steht
in fast jeder Geschossdatei: das Projektil fliegt, und beim Aufschlag kommt die
Explosion.

`CParticle::Die` ist zehn Zeilen lang und enthält drei Überraschungen:

```cpp
if ( mFlags & FX_DEATH_RUNS_FX && !(mFlags & FX_KILL_ON_IMPACT) )
{
    // Man, this just seems so, like, uncool and stuff...
    VectorSet( norm, Q_flrand(-1,1), Q_flrand(-1,1), Q_flrand(-1,1));
    VectorNormalize( norm );
    theFxScheduler.PlayEffect( mDeathFxID, mOrigin1, norm );
}
```

**Ein gesetztes `deathfx` allein tut nichts.** Es braucht zusätzlich
`FX_DEATH_RUNS_FX`. Das ist der häufigste Irrtum am Feld — die Datei sieht
vollständig aus, und beim Sterben passiert nichts.

**`killOnImpact` schaltet es beim natürlichen Tod ab.** Dann gehört der Effekt
zum Aufschlag, nicht zum Ablauf der Zeit.

**Die Achse ist eine zufällige Richtung**, nicht die Flugrichtung. Ravens
Kommentar daneben spricht für sich. Gemessen:

```
deathFx: Kind startet bei 400 ms, Hoehe 80 Einheiten
deathFx-Achse: 9 verschiedene Richtungen bei 12 Ausgangswerten
```

Neun verschiedene Richtungen aus zwölf Ausgangswerten — wäre die Achse fest,
stünde dort eine.

### Die Prüfung meldet beides jetzt

Ein `deathfx` ohne `deathRunsFx` wird als Warnung gemeldet, `deathfx` mit
`killOnImpact` als Hinweis. Der zweite ist kein Fehler, aber überraschend
genug: wer eine Explosion erwartet und keine bekommt, sucht sonst lange.

Damit ist die Simulation vollständig: Kurven, Zeitplanung, Bahn, Ausrichtung,
alle dreizehn Darstellungsarten, Emitter, FxRunner und Todeseffekte.

## Physik: Abpraller, und eine Änderung an der Bauart

Bis hierhin war die Bahn **eine Formel** — jeder Zeitpunkt direkt ausrechenbar.
Mit Abprallern geht das nicht mehr: nach jedem Aufprall gilt eine neue
Geschwindigkeit, und *wann* er kommt, hängt vom Weg ab.

Die Bahn wird deshalb beim Auslösen einmal in **Abschnitte** zerlegt. Jeder
einzelne ist wieder geschlossen ausrechenbar, und die Vorschau bleibt
bildratenunabhängig: sie läuft nicht Schritt für Schritt mit, sondern schlägt
nach.

Zerlegt wird in festen 8-ms-Schritten. Wieder derselbe Grund wie beim Emitter:
mit der Bildrate hinge das Ergebnis davon ab, wie schnell der Rechner ist.

### Die Formel aus der Engine

```cpp
VectorMA( mVel, frameTime * trace.fraction, mAccel, mVel );
dot = DotProduct( mVel, trace.plane.normal );
VectorMA( mVel, -2 * dot, trace.plane.normal, mVel );
VectorScale( mVel, mElasticity, mVel );
if ( trace.plane.normal[2] > 0 && mVel[2] < 4 ) { VectorClear( mVel ); … }
```

Spiegeln, dämpfen, und dann die Bedingung zum Liegenbleiben. Ohne die letzte
Zeile zittert ein Funke ewig auf dem Boden — sie ist keine Optimierung, sie ist
sichtbar.

Gemessen:

```
Abpraller: 10 Aufpralle, erster Gipfel 100, zweiter 25
```

Bei Elastizität 0.5 ist der zweite Gipfel ein Viertel des ersten — die
Geschwindigkeit halbiert sich, die Höhe geht mit dem Quadrat. Das ist die
Prüfung, die zeigt, dass nicht nur *irgendetwas* abprallt.

### Drei Stellen, an denen man es falsch macht

**Die früheste Berührung gewinnt.** Wer die erste gefundene Ebene nimmt,
bekommt in einer Ecke den falschen Abpraller.

**Von hinter der Ebene wird nicht getroffen.** Sonst prallt ein Partikel, das
schon im Boden steckt, nach unten weiter weg statt zurück.

**Nach dem Aufprall ein Stück von der Fläche wegsetzen.** Sonst fängt der
nächste Schritt schon wieder in ihr an, und man bekommt eine Endlosschleife aus
Aufprallen.

Der Test prüft alle drei — inklusive eines Partikels, das im Boden startet, und
einer Elastizität von 0.99, die nicht aufschaukeln darf.

### Draußen gibt es nur den Boden

Bei „Boden und Himmel" fallen die Wände weg. Nichts ist verwirrender als ein
Funke, der im Nichts abprallt.

### Und wieder ein Test mit einer falschen Annahme

Mein erster Testfall schrieb `origin = (0, 0, 100)` und meinte hundert
Einheiten hoch. Herausgekommen ist `(0, 100, 0)`: der Partikel stand auf dem
Boden, prallte sofort auf, und `killOnImpact` tötete ihn bei 0 ms.

**Das erste Feld ist „vorwärts", nicht „x."** Die Effektachse rechnet um
(`org = forward·x + right·y + up·z`), und bei „nach oben" ist `forward` die
Z-Achse. Das Programm rechnete richtig; der Test hatte Weltkoordinaten
angenommen, wo Effektkoordinaten stehen.

Nach der Korrektur:

```
Partikel mit Physik: 10 Aufpralle
killOnImpact: statt 2000 ms nur 500 ms
```

500 ms ist genau die Fallzeit aus 100 Einheiten bei `g = -800`. Dass die Zahl
stimmt, ist der eigentliche Beleg — vorher stand dort eine Null, und die sah
auch wie ein Ergebnis aus.

## impactfx: bei jedem Aufprall ein Effekt

Die Aufprallpunkte lagen nach der Physikrunde vor; es fehlte der Aufruf.

```
impactfx: 8 Effekte bei den Aufprallen
Aufprallachse: Kind fliegt mit z=60 (von der Flaeche weg)
```

### Die Achse ist die Flächennormale, nicht zufällig

Anders als beim `deathFx`. Das ist auch sinnvoll: ein Einschlag zeigt von der
Wand weg, und die Engine reicht `trace.plane.normal` durch. Der Test prüft es,
indem er dem Kindeffekt eine Geschwindigkeit „vorwärts" gibt — fliegt sie nach
oben, stimmt die Achse.

Dass zwei so ähnliche Felder sich hier unterscheiden, ist genau die Sorte
Detail, die man beim Nachbauen vereinheitlicht und damit falsch macht.

### Beim Liegenbleiben ist Schluss

```cpp
mFlags &= ~(FX_APPLY_PHYSICS|FX_IMPACT_RUNS_FX);
```

Die Engine löscht **beide** Flags, sobald ein Partikel zur Ruhe kommt — ein
liegender Funke soll nicht weiter Aufpralleffekte auslösen. Die Löschung steht
dabei *nach* dem Auslösen, der letzte Aufprall zählt also noch.

Dazu eine eigene Obergrenze von acht: ein Funke mit hoher Elastizität prallt
ein Dutzend Mal, und jeder Aufprall startet einen Effekt.

### Zwei Bedingungen, nicht eine

`impactfx` braucht das Flag **und** `usePhysics`. Ohne Kollision gibt es keinen
Aufprall.

Das ist der unangenehmere der beiden Stolpersteine, weil die Felder in
**verschiedenen Reitern** stehen: `impactfx` im Physik-Reiter, das Flag ebenso,
aber wer nur eines setzt, sieht der Datei nichts an.

Die Prüfung meldet jetzt beide Fälle getrennt — „das Flag fehlt" und „die
Physik fehlt" sind verschiedene Fehler und brauchen verschiedene Antworten.

## Die letzten offenen Punkte

### PNG — weil es fast nichts kostete

Der Auspacker war schon für die `.pk3`-Dateien nötig; was blieb, sind die
Zeilenfilter. Vier Farbarten von Pillow, **byteweise** verglichen:

```
vier PNG-Farbarten, alle byteweise gleich
```

Kein Spielraum wie bei JPEG — PNG ist verlustfrei, also muss es exakt stimmen.
Zwei Stellen, an denen man es falsch macht: die Bilddaten können auf **mehrere
IDAT-Blöcke** verteilt sein (wer nur den ersten nimmt, bekommt das obere
Drittel und darunter Müll), und verschachtelte PNGs brauchen einen ganz anderen
Weg — die werden abgewiesen und beim Namen genannt.

### Bildschirmfoto: Targa, nicht PNG

Obwohl der Auspacker da ist. **Packen ist etwas anderes als Auspacken**, und
einen Deflate-Packer zu schreiben, nur um ein Bildschirmfoto kleiner zu machen,
wäre unverhältnismäßig. Quake und seine Nachfolger schreiben ihre
Bildschirmfotos ebenfalls als Targa.

Geprüft über den Rundlauf — und zusätzlich damit, dass **Pillow** die Datei
liest. Sonst hätte ich nur geprüft, dass mein Schreiber zu meinem Leser passt.

Zwei Fallen im Auslesen: OpenGL beginnt **unten links**, ein Bild oben links —
die Zeilen müssen getauscht werden. Und Direct3D füllt Zeilen auf, `RowPitch`
ist nicht `width*4`; wer das übersieht, bekommt ein schräg verzogenes Bild.

Die Zwischenablage braucht Win32 und ist noch nicht da — der Menüpunkt
speichert stattdessen und sagt es im Ablaufprotokoll. Besser als ein Punkt, der
schweigt.

### Wandtexturen: gerechnet, nicht geladen

Das Original liefert `brick.jpg`, `dirt.jpg` und `stucco.jpg` mit. Wir haben
sie nicht und dürften sie auch nicht beilegen.

```
Wandtexturen: Ziegel 108, Erde 79, Putz 36 Stufen Kontrast
```

Ein Menüpunkt, der nichts tut, ist schlechter als ein gerechnetes Muster — und
für die Aufgabe, ein Gefühl für Maßstab zu geben, reicht ein Muster vollkommen.
Das Muster hängt nur von x und y ab, damit es sich nahtlos kacheln lässt.

Der Test prüft nicht nur, *dass* etwas herauskommt, sondern dass Ziegel
kontrastreicher sind als Putz. Sonst könnte die Zuordnung vertauscht sein, und
man merkte es nur beim Hinsehen.

### Zuletzt geöffnete Dateien

Acht Einträge, neueste zuerst. Eine schon bekannte Datei **wandert nach vorn**
statt sich zu verdoppeln — sonst steht dieselbe achtmal da, sobald jemand daran
arbeitet.

Im Menü nur der Dateiname, der ganze Pfad im Hinweis: vollständige Pfade
sprengen jedes Menü. Und durchlaufen wird eine Kopie, weil `openFile` die Liste
dabei umordnet.

### Und der Memberprüfer, zum dritten Mal

Er meldete `d_` aus dem Format `"efxed_%Y%m%d_%H%M%S.tga"`. Ein Prüfer, der
Zeichenketten durchsucht, meldet früher oder später jeden Dateinamen mit
Unterstrich — jetzt schneidet er sie vorher heraus.

Das ist keine Ausnahmeliste, sondern eine Korrektur: Ausnahmen kosten echte
Meldungen, das Herausschneiden nicht.

## Doppelte Arbeit — und warum

Diese Runde habe ich einen TGA-Schreiber, gerechnete Wandtexturen und Tests
dafür gebaut. **Alles drei gab es schon** — `src/image_png.cpp` enthält den
Schreiber, `scene::buildWallTexture` die Muster, und beide waren in `app.cpp`
längst angeschlossen.

Entstanden ist das in einem Teil dieses Gesprächs, den ich nicht mehr sehe.
Meine Liste offener Punkte war aus Suchbefehlen abgeleitet, die daneben lagen:
ich hatte nach `decodePng` und `screenshot` in `src/` und `gui/` gesucht und
die Treffer falsch gezählt.

Alles Doppelte ist wieder entfernt, und beim Herausnehmen ist mir prompt ein
zweiter Fehler unterlaufen: mein Ausschneiden traf einen Anker, den es zweimal
gab, und riss achtzig Zeilen aus der Klangprüfung. Aufgefallen ist es sofort,
weil der Übersetzer meckerte — aber es zeigt, wie schnell so etwas passiert.

**Die Lehre:** vor dem Bauen den Bestand prüfen, nicht die eigene Liste. Für
die letzten drei Punkte habe ich deshalb jeden Menüpunkt einzeln durchgesehen,
mit einem kleinen Suchprogramm statt nach Gefühl:

```
Menuepunkte ohne Wirkung: keine
```

### Die drei, die wirklich offen waren

- **F5 „Materialbestand neu einlesen"** — Menüpunkt ohne Aufruf. Der Knopf im
  Datei-Menü tat es längst, dieser nicht.
- **„Löschen" im Bearbeiten-Menü** — nur die `Entf`-Taste funktionierte.
- **„Löschen" im Effekte-Menü** — dieselbe Sache ein zweites Mal.

Alle drei gehen jetzt über denselben Weg, samt Rückgängig-Eintrag und
Auswahlkorrektur, wenn das letzte Segment verschwindet.

## Der Abgleich ist geschlossen

```
Menuebefehle des Originals: 49
davon umgesetzt:            46
bewusst weggelassen:         3
ohne Entsprechung:      keine
```

Nachgeprüft statt behauptet: ein kleines Programm liest die Menüressource aus
dem Binary und hält jeden Befehl gegen die Übersetzungstabelle und `app.cpp`.
Dass ich das brauche, hat zwei Runden gekostet — meine Liste offener Punkte war
zweimal falsch, einmal zu lang und einmal zu kurz.

Zuletzt gefunden: **„Segment Enabled" fehlte im Menü.** Das Häkchen stand in
der Segmentliste, aber wer im Menü sucht, fand dort nichts. Und die vier
Werkzeugleisten.

### Bei den Werkzeugleisten weiche ich ab

Im Original sind es vier andockbare Fenster. Bei uns bleibt es eine feste
Leiste, deren vier **Gruppen** sich einzeln abschalten lassen — sie entsprechen
genau den Leisten 128, 155, 158 und 167 aus dem Binary.

Dasselbe Ergebnis, ohne dass man Fenster herumzieht. Andockbare Leisten sind in
einer ImGui-Oberfläche machbar, aber sie wären viel Arbeit für eine Sache, die
2003 üblich war und heute niemand vermisst.

### Zwei Dialoge, die entfallen

```
"Your changes will not take effect until you click the Apply button."
"You have made changes that have not been applied to the effect."
```

Das Original übernahm Änderungen erst auf „Übernehmen" und musste davor warnen.
Bei uns wirkt jede Änderung sofort — da gibt es nichts zu warnen. Ein Dialog,
den man nicht braucht, ist besser als einer, den man gut gebaut hat.

## `glBindFramebuffer` ohne `gl::`

Kein Nachschlagefall, sondern ein fehlendes Namensraum-Präfix an zwei Stellen.

Windows' `opengl32` bietet nur **OpenGL 1.1**. Alles Neuere wird in
`renderer_gl3.cpp` zur Laufzeit über `wglGetProcAddress` geholt und liegt im
Namensraum `gl`. Ohne Präfix sucht der Übersetzer eine Systemfunktion, die es
dort nicht gibt.

Vier andere Stellen in derselben Datei hatten das Präfix; genau die zwei im
Bildschirmfoto-Code nicht.

### Warum es durchgerutscht ist

Der Übersetzerlauf deckt nur `app.cpp` ab. Die Renderer brauchen Windows- und
GL-Köpfe, die hier nicht liegen — das war von Anfang an die Grenze dieses
Verfahrens, und hier hat sie zugeschlagen.

### Ein fünfter Prüfer, sehr eng gebaut

`tools/lint_gl.py` liest die **Liste der nachgeladenen Funktionen aus dem Makro
in der Datei selbst** und meldet jeden Aufruf ohne `gl::`.

Die Liste zu lesen statt zu pflegen ist der Punkt: kommt eine Funktion dazu,
wird sie automatisch mitgeprüft. Eine abgeschriebene Liste wäre nach dem
nächsten Renderer-Umbau veraltet, ohne dass es jemand merkt.

Gegenprobe wie immer: Präfix entfernt → gemeldet, zurückgesetzt → still.

### Die fünf zusammen

| | |
|---|---|
| `lint_i18n.py` | Oberflächentexte ohne `tr()` |
| `lint_members.py` | Membernamen, die nirgends deklariert sind |
| `lint_includes.py` | benutzte Namen, deren Kopf fehlt |
| `lint_calls.py` | Aufrufe von Funktionen, die es nicht gibt |
| `lint_gl.py` | OpenGL-Aufrufe ohne `gl::` |

Dazu der Übersetzerlauf für `app.cpp`. Jeder der fünf ist als Reaktion auf
einen Baufehler entstanden, der bei dir aufschlug — das ist keine schöne
Bilanz, aber es ist die ehrliche.

## Symbole in der Werkzeugleiste — gezeichnet, nicht als Schrift

Die Buchstabenkürzel („N O S | + - | |> || []") waren ein Platzhalter. Jetzt
richtige Symbole.

### Warum nicht Font Awesome

Eine Symbolschrift wäre weniger Arbeit, hängt aber daran, dass die Schrift
geladen wird. Fällt sie aus, stehen **leere Kästen** in der Leiste — und genau
das ist der Fehler, den man auf einem fremden Windows nicht bemerkt, bevor
jemand anruft. Du hattest ausdrücklich nach Windows 10 gefragt; die Antwort ist
damit eindeutig.

Gezeichnete Symbole haben gar keine Schriftabhängigkeit, skalieren mit der
Bildschirmauflösung, ohne unscharf zu werden, und folgen dem Thema, weil die
Farbe aus der Palette kommt.

### Ein Einheitsquadrat für alle

Jedes Symbol ist in Koordinaten von 0 bis 1 beschrieben und wird erst beim
Zeichnen auf die Knopfgröße gestreckt. So stimmen die Proportionen bei jeder
DPI, und es gibt genau eine Stelle, an der gerechnet wird.

Die Strichstärke wächst mit, aber **nie unter einen Bildpunkt** — sonst
verschwinden die Symbole bei kleiner Leiste ganz.

Ein paar Entscheidungen zur Form:

- **Speichern ist eine Diskette.** Nach zwanzig Jahren hat noch niemand ein
  besseres Zeichen dafür gefunden.
- **Texturiert und Drahtgitter sind dasselbe Quadrat**, einmal gefüllt und
  einmal als Gitter. Der Gegensatz ist die Aussage.
- **Overdraw sind drei sich überlappende Kreise** — genau das, was der Modus
  zeigt.
- **Die Achsen sind einfarbig.** Rot, grün und blau wären naheliegend, aber die
  Farbe kommt aus dem Thema, und drei feste Farben würden bei „Hoher Kontrast"
  aus der Reihe fallen. Die Form allein genügt.

### Und wieder ImGuis Innenleben, das ich fast benutzt hätte

Für ausgegraute Symbole hatte ich `GetCurrentContext()->CurrentItemFlags`
geschrieben — dieselbe interne Schnittstelle, die ich bei den Gruppenrahmen
schon einmal verworfen hatte.

Es ist gar nicht nötig: `BeginDisabled` senkt `style.Alpha`, und `GetColorU32`
rechnet das ein. Das Symbol wird von selbst blass.

### Der Übersetzerlauf deckt jetzt zwei Dateien ab

`icons.cpp` braucht nur ImGui, also kommt sie mit dazu. Die Renderer und
`main_win32.cpp` bleiben außen vor — die gehen nur auf dem Zielrechner.

## Zur Fahne

Sie steht absichtlich nicht mehr in der Mitte. Im Ursprung stand sie genau
dort, wo der Effekt entsteht, und war damit im Weg; außerdem ist der Ursprung
der Zielpunkt der Umlaufkamera, wodurch sie beim Drehen mitzugehen schien.

Falls die Ecke zu weit weg ist, lässt sich der Abstand leicht ändern — sag
Bescheid.

## Eine Zeitleiste — und eine eigene Uhr

Ja, das geht, und es behebt nebenbei etwas, das vorher schwach war.

### Die Wanduhr reichte nicht

Bis eben kam die Wiedergabezeit direkt aus `ImGui::GetTime()` — verstrichene
Wanduhrzeit seit dem Start. Das reicht zum Abspielen und für **sonst nichts**:
man kann nicht springen, nicht zurückspulen und nicht Bild für Bild gehen, weil
die Wanduhr sich nicht verschieben lässt.

`efx::timeline::Clock` gehört dem Programm. Sie wird um einen Zeitschritt
weitergestellt, wenn abgespielt wird, und lässt sich sonst frei setzen. Damit
wird die Wiedergabe außerdem **vorhersagbar**: dieselbe Datei bei derselben
Zeit ergibt dasselbe Bild, unabhängig davon, wie schnell der Rechner ist.

Vollständig geprüft — vierzig Prüfungen, ohne eine Zeile Oberfläche.

### Vier Entscheidungen, die man merkt

**Spulen hält an.** Wer eine Stelle sucht, will sie ansehen; liefe die Uhr
weiter, wäre sie beim Loslassen schon woanders.

**Bild für Bild hält ebenfalls an.** Einzelschritt und Abspielen zugleich
ergäbe keinen Sinn.

**Große Sprünge werden gedeckelt.** Bleibt das Fenster eine Sekunde hängen —
beim Verschieben, beim Laden einer Textur —, soll der Effekt nicht eine Sekunde
weiterspringen:

```
nach 3000 ms Haenger: 100 ms weiter (gedeckelt)
```

Der Deckel greift auf die **Wanduhrzeit**, vor der Geschwindigkeit. Sonst
ließe er sich mit hoher Geschwindigkeit umgehen, und genau das prüft ein Test.

**Der Überhang wird mitgenommen.** Bei 1100 ms auf 1000 ms Dauer steht die Uhr
bei 100, nicht bei 0 — sonst ruckelt jede Wiederholung um wenige Millisekunden.

### Was in der Leiste steht

Abspielen, Pause, Stop | an den Anfang, ein Bild zurück, ein Bild vor |
Zeit und Bildnummer | Schieberegler | Geschwindigkeit | Bildrate | was am Ende
geschieht.

**Zeit und Bildnummer beide**, weil man beim Feinarbeiten in Bildern denkt und
beim Vergleich mit der Datei in Millisekunden.

Die Knöpfe gibt es doppelt — auch in der Werkzeugleiste. Das ist kein Fehler:
die Werkzeugleiste ist weit weg vom Bild, und wer die Zeitleiste bedient, will
die Knöpfe dort.

### Die Bildrate ist einstellbar

Voreingestellt 30 Hz. JKA-Effekte sind für kein festes Raster gemacht, aber zum
Vergleichen braucht man eines — und wer prüfen will, wie ein Effekt bei 60 Hz
aussieht, stellt es um.

## `std::min` ohne `<algorithm>` — und warum mein Prüflauf es nicht sah

GCC zieht `<algorithm>` über andere Köpfe mit herein, MSVC nicht. Der
Übersetzerlauf hier benutzt GCC und meldete deshalb nichts.

**Das ist die unangenehmste Sorte Unterschied**: das Verfahren, das ich gegen
Baufehler gebaut habe, ist gegen genau diese Klasse blind.

### Ein sechster Prüfer

`tools/lint_std.py` bildet häufig gebrauchte Standardsymbole auf ihren Kopf ab
und meldet, was fehlt. Bewusst **nicht** vollständig — eine Abdeckung der
ganzen Standardbibliothek wäre Unsinn und würde vor lauter Fehlmeldungen
ignoriert.

Der erste Entwurf meldete **sechzig** Stellen. Fast keine stimmte: eine `.cpp`,
die `"efx/effect.h"` einbindet, darf `std::vector` benutzen, wenn dieser Kopf
`<vector>` mitbringt — das ist keine Nachlässigkeit, sondern der Sinn eines
Kopfes.

Nachdem eigene Köpfe eine Ebene tief mitzählen, bleiben **acht** — und alle
acht waren echte Zeitbomben, die auf einem anderen Übersetzer zuschlagen:

```
app.cpp             <cmath>, <cstdio>
diag.cpp            <algorithm>
main_win32.cpp      <cstdio>
renderer_d3d11.cpp  <cstdio>
```

Dass der Prüfer auch in Dateien fündig wurde, die ich in dieser Runde gar nicht
angefasst habe, ist der eigentliche Gewinn.

## Das Wiedergabefenster war meterbreit

Zwei Dinge, die sich gegenseitig aufschaukeln:

```cpp
ImGuiWindowFlags_AlwaysAutoResize   // Fenster waechst nach dem Inhalt
ImGui::SetNextItemWidth(-1.0f);     // Regler nimmt die volle Breite
```

Das Fenster wächst nach dem breitesten Element, und die Regler nehmen sich die
neue Breite. Auf einem breiten Bildschirm wird der Dialog dadurch so breit wie
der Bildschirm.

Jetzt feste Breite (460 Punkte) statt automatischer. Die **Höhe** darf sich
weiter nach dem Inhalt richten — nur die Breite war das Problem.

Ein kurzes Suchprogramm hat alle Dialoge auf dieselbe Falle geprüft: von zehn
mit `AlwaysAutoResize` hatten **zwei** Regler mit Breite −1. Der zweite war der
Spielpfad-Dialog, wo ein langer Pfad denselben Effekt gehabt hätte.

## Die Wiedergabe war kaputt — ein stilles Ersetzen

Beim Umbau auf die eigene Uhr habe ich zwei Änderungen gemacht:

1. `startPlayback` setzt `playbackStartMs_` nicht mehr — stattdessen die Uhr.
2. Der Rechenblock sollte von der Wanduhr auf die Uhr umgestellt werden.

**Die zweite hat nicht gegriffen.** Mein Suchtext hatte einen anderen
Variablennamen (`nowMs` statt `clockMs`), das Ersetzen fand nichts und meldete
das nicht.

Ergebnis: der Block las weiter `playbackStartMs_`, das jetzt für immer auf null
stand. Damit war die verstrichene Zeit sofort größer als die Effektdauer, und
die Wiedergabe startete in jedem Bild neu — man sah nichts.

Der Übersetzerlauf konnte das nicht fangen: der Code war syntaktisch
einwandfrei, nur logisch tot.

### Was ich daraus mitnehme

Ein Ersetzen, das nichts findet, ist gefährlicher als eines, das fehlschlägt.
Bei den Kernbibliotheken prüfe ich das mit `assert ... in s` — beim
Oberflächenquelltext hatte ich es diesmal weggelassen.

Nach der Korrektur habe ich gegengeprüft, dass der Zeitfluss geschlossen ist:
`ImGui::GetTime()` kommt in der Wiedergabe nicht mehr vor, und alle sechs
Übergänge (Dauer setzen, weiterstellen, lesen, starten, anhalten, pausieren)
gehen über die Uhr.

Nebenbei sind zwei Felder weggefallen: `playbackStartMs_` und
`pauseStartedMs_`. Sie existierten nur, weil sich die Wanduhr nicht anhalten
ließ — die eigene Uhr kann das selbst.

## Die Fahne steht wieder in der Mitte

Am Rand war sie schlecht zu sehen und stand bei „Boden und Himmel" im Nichts.

Ganz in den Ursprung gehört sie aber auch nicht: dort entsteht der Effekt, und
der Ursprung ist der Zielpunkt der Umlaufkamera.

Der Kompromiss: **in der Mitte, um zwei Fuß versetzt.**

```
Fahne steht bei -32 / -32 (Mitte, 45 Einheiten versetzt)
```

Nah genug, um den Wind am Effekt abzulesen, weit genug, um nicht mitten darin
zu stehen.

## Die Fahne lässt sich anfassen und verschieben

Dafür fehlte etwas Grundsätzliches: **ein Weg vom Bildschirm in die Welt.** Man
konnte nichts im Bild anfassen, weil sich zu einem Mauszeiger keine
Weltkoordinate finden ließ.

`efx::camera` hat jetzt `rayThroughPixel`, `intersectGroundPlane` und
`distanceToRay` — geprüft, ohne eine Zeile Oberfläche:

```
Bildmitte: Abstand zum Zielpunkt 0.00 Einheiten
```

Der Strahl durch die Bildmitte trifft den Zielpunkt der Kamera auf null genau.
Das ist die Prüfung, die zeigt, dass die Umrechnung stimmt — und sie braucht
keinen Bildschirm.

Gerechnet wird über die **Umkehrung der Blickmatrix**, nicht über die
Kamerawinkel: die Matrizen sind die Wahrheit, die Winkel nur ein Weg dorthin.
Eine Blickmatrix besteht aus Drehung und Verschiebung, ihre Umkehrung ist die
transponierte Drehung und die zurückgedrehte Verschiebung — keine allgemeine
Matrixinversion nötig.

### Drei Dinge, die man beim Anfassen falsch macht

**Der Greifradius muss mit der Entfernung wachsen.** Eine weit entfernte Fahne
ist auf dem Bildschirm nur wenige Bildpunkte breit; mit festem Radius trifft
man sie nie.

**Die Kamera muss stillstehen.** Beide hängen an der linken Maustaste — dreht
sich das Bild beim Ziehen, rutscht die Fahne unter der Maus weg.

**Die Fahne bleibt im Raum.** Hinter der Wand ist sie unsichtbar, und man
findet sie nicht wieder. Dasselbe beim Einlesen der Einstellungen: eine Stelle
zehn Kilometer daneben wird zurückgeholt.

### Und ein Fehler, den der Übersetzer nicht fangen konnte

Ich hatte den Fahnentest an die Stelle geschrieben, an der die Geometrie
entsteht — **vor** `ImGui::Image`. Dort beziehen sich `IsItemHovered()` und
`GetItemRectMin()` aber auf das zuletzt eingereichte Element, also auf
irgendetwas anderes.

Der Code übersetzt einwandfrei und tut das Falsche. Jetzt steht die Bedienung
hinter dem Bild, in einer eigenen Funktion, und die Reihenfolge ist im
Kommentar begründet — Fahne vor Kamera, beide nach dem Bild.

### Was jetzt geht

Zeiger über den Mast: er wird heller und der Mauszeiger wird zur Hand. Ziehen
verschiebt ihn über den Boden, in beide Richtungen. Die Stelle steht in den
Einstellungen und ist beim nächsten Start wieder da.

Der richtige Platz hängt vom Effekt ab: bei einem kleinen Funken will man sie
nah haben, bei einer Explosion weiter weg. Ein fester Ort ist für das eine zu
weit und für das andere zu nah — deshalb war jede meiner beiden festen
Positionen falsch.

## Absturz beim Wechsel der Grafikschnittstelle

Beim Umschalten wird das Fenster neu gebaut — das **App-Objekt überlebt aber**,
mitsamt seiner Texturkennungen. Die zeigen danach auf Ressourcen eines
Renderers, den es nicht mehr gibt.

Das erklärt beide Beobachtungen auf einmal, und der Unterschied zwischen ihnen
ist aufschlussreich:

| | |
|---|---|
| **nach Direct3D** | Absturz — eine `TextureId` ist dort ein **Zeiger** |
| **nach OpenGL** | still nichts — dort ist sie eine kleine **Zahl**, die nur nichts mehr bedeutet |

Derselbe Fehler, zwei völlig verschiedene Symptome. Wer nur das eine sieht,
sucht an der falschen Stelle.

`forgetGraphicsResources()` wirft alle Kennungen weg, **ohne sie freizugeben** —
freigeben darf man sie nicht, der Besitzer existiert nicht mehr. Gerufen wird
sie zwischen den beiden Sitzungen, dort wo das Fenster neu entsteht.

Betroffen waren drei Dinge: der Texturspeicher der Shader, die Wandtextur und
die Marke, die den Neubau der Geometrie auslöst.

## Der Neustart mitten im Zeichnen

Bei „Wiederholen" rief die Wiedergabe am Ende jedes Durchlaufs `startPlayback()`
— **innerhalb der Zeichenroutine**.

`startPlayback` plant den ganzen Effekt neu, lädt untergeordnete Dateien nach
und bricht Klänge ab. Bei einem großen Effekt stockt das Bild an genau der
Stelle, an der die Wiederholung beginnt, und das sieht aus, als liefe nichts
mehr.

Jetzt wird nur vorgemerkt und im nächsten Bild erledigt, bevor gezeichnet wird.

Dabei ist noch etwas aufgefallen: `startPlayback` holte die Geschwindigkeit aus
der Werkzeugleiste zurück. Wer sie in der **Zeitleiste** eingestellt hatte,
verlor sie bei jeder Wiederholung.

### Was ich nicht nachstellen konnte

Die Uhr selbst arbeitet richtig — ich habe den Bildablauf mit Wiederholung
nachgebaut und über 120 Bilder mitgeschrieben: drei saubere Durchläufe, die
Zeit läuft durchgehend.

Wenn es bei dir nach diesen beiden Korrekturen immer noch klemmt, hilft mir das
Ablaufprotokoll — dort steht jetzt eine Zeile, wenn Grafikressourcen vergessen
werden.

## Zwei Wahrheiten, zwei Fehler

Beide Beobachtungen kommen daher, dass dieselbe Sache an zwei Stellen stand.

### Play zweimal drücken — eine Verklemmung

```cpp
if (playing_ || clock_.timeMs() > 0.0f) {   // Zeile 1421
    ...
    playing_ = clock_.state() != Stopped;   // Zeile 1449 — INNERHALB
```

Nach dem Ende steht die Uhr bei `durationMs`, `playing_` ist falsch. Play
drücken setzt die Uhr auf null und startet sie. Nächstes Bild: `playing_` ist
immer noch falsch — es wurde ja nie neu gesetzt — und `timeMs` ist null. Die
Bedingung ist falsch, der Block wird übersprungen, `playing_` wird nie gesetzt.

**Die Bedingung hing an einem Wert, der nur innerhalb der Bedingung gesetzt
wurde.** Man kam nicht mehr heraus.

`playing_` ist jetzt weg. Ob abgespielt wird, sagt die Uhr — sie weiß es
ohnehin, und ein zweites Merkzeichen daneben kann nur auseinanderlaufen.

### Wiederholen sprang auf Anhalten zurück

`startPlayback()` holte die Betriebsart aus `playback_.mode` zurück — dem
Wiedergabe-Dialog, der auf „einmal abspielen" steht. Die Zeitleiste hat aber
ein **eigenes** Auswahlfeld dafür.

Man stellte also Wiederholen ein, und beim nächsten Start sprang es zurück.

Jetzt ist die Uhr die einzige Wahrheit: der Dialog schreibt in sie hinein, die
Zeitleiste ebenfalls, und die Zeitleiste führt den Dialog mit, damit beide
dasselbe zeigen. `startPlayback` fasst die Betriebsart nicht mehr an.

### Und alle Play-Knöpfe gehen durch dieselbe Stelle

Werkzeugleiste, Zeitleiste, Menüpunkt und Leertaste riefen vier verschiedene
Dinge — einer `startPlayback()`, einer `clock_.play()`, einer schaltete um.

Alle vier gehen jetzt durch `pressPlay()`:

- läuft es → Pause
- steht es am Ende oder gibt es nichts zu zeigen → neu auslösen, von vorn
- sonst → fortsetzen, wo es stand

Ein Druck genügt, auch vom Ende aus. Und die Leertaste schaltet um, statt
anzuhalten: wer im Bild arbeitet, will kurz stoppen und weitersehen, nicht von
vorn beginnen.

### Und ein Test, der unter Last umkippte

Beim Nachprüfen wurde ein Lauf rot, den ich danach zwanzigmal nicht mehr
nachstellen konnte. Die Zahlen verrieten den Grund:

```
roter Lauf:     seriell 92.2 ms
normale Laeufe: seriell ~40 ms
```

Der Rechner war mehr als doppelt so langsam — nebenher lief ein Übersetzer.

Die Ursache war eine feste Zeitschranke: `elapsed < 40` bei einer Begrenzung
von 4 ms. Auf einem ruhigen Rechner reichlich, unter Last zu knapp.

Was der Test wirklich prüfen soll, ist **dass die Begrenzung greift** — also
deutlich weniger Zeit vergeht, als für alle Aufgaben nötig wäre. Das ist
unabhängig davon, wie schnell der Rechner ist:

```
Zeitbegrenzung 4 ms: 80 von 400 in 4 ms (alles waere ~20 ms)
```

Gegengeprüft mit vier parallel laufenden Übersetzern: bleibt grün.

Ein Test, der von der Rechnerlast abhängt, ist fast so schlecht wie einer, der
von der Umgebung abhängt — dieselbe Lehre wie bei den Deflate-Daten aus `/tmp`,
nur eine Runde später.

## app.cpp aufgeteilt

3524 Zeilen, 47 Methoden, ein Dutzend Zuständigkeiten — Datei-Ein-/Ausgabe,
Rückgängig, Wiedergabe, Klang, Texturen, Materialbestand, Menü,
Werkzeugleiste, Zeitleiste, Ansicht, Segmentliste, Eigenschaftsseiten,
Dialoge, Eingabeelemente.

**Das ist die Stelle, an der ein erfahrener Leser aufgibt** — und die erste,
die er anspricht.

Jetzt sieben Dateien, dieselbe Klasse, nach Aufgaben getrennt:

```
 918  app.cpp             Lebenszyklus, Datei, Rueckgaengig, Ansicht
 778  app_panels.cpp      Menue, Werkzeugleiste, Zeitleiste, Liste
 740  app_dialogs.cpp     alle Dialoge
 639  app_properties.cpp  die Eigenschaftsseite
 367  app_widgets.cpp     wiederverwendbare Eingabeelemente
 237  app_playback.cpp    Uhr, Ausloesen, Klang, Untereffekte
 143  app_resources.cpp   Texturen, Materialbestand, Diagnose
```

Am Verhalten ändert das nichts: dieselbe Klasse, dieselben Methoden, nur
sortiert.

### Die Aufteilung hat etwas sichtbar gemacht

Zwei Funktionen aus dem anonymen Namensraum — `activeTheme` und
`severityColor` — wurden von mehreren Stellen gebraucht. In einer
3500-Zeilen-Datei fällt das nicht auf; sobald die Datei geteilt ist, **muss**
man sich entscheiden, wohin sie gehören. Sie stehen jetzt in `app_shared.h`,
und der Kopf sagt, warum.

Genau das ist der Gewinn einer Aufteilung: sie macht sichtbar, was gemeinsam
benutzt wird.

### Eine Datei bleibt groß, und das ist Absicht

`app_properties.cpp` enthält `drawTab` mit über fünfhundert Zeilen. Sie zu
zerschneiden wäre möglich, aber falsch: es ist eine Fallunterscheidung über die
Reiter, und jeder Zweig steht für sich. Eine Aufteilung würde den Zusammenhang
zerreißen, den die Reiter gerade ausmachen.

Was sie stattdessen bekommen hat: eine eigene Datei, in der nichts anderes
steht.

## ARCHITEKTUR.md

Dazu ein Dokument, das ein Leser als Erstes aufschlägt: die Schichten, was wo
liegt, die Regeln, an die sich der Quelltext hält, und wo bewusst abgewichen
wird.

Die wichtigste Zeile darin ist die Trennlinie: **der Kern kennt die Oberfläche
nicht.** Er hat keine einzige Zeile, die ein Fenster oder eine
Grafikschnittstelle voraussetzt — deshalb liegt fast alles Prüfbare dort, und
deshalb gibt es 3877 Prüfungen.

## Aufräumen nach der Aufteilung

### 131 überflüssige Einbindungen

Beim Aufteilen hatte ich jeder neuen Datei **dieselbe Liste** von Einbindungen
verpasst. Das ist die Sorte Nachlässigkeit, die ein Leser sofort sieht: wenn
oben zwanzig Köpfe stehen, weiß er nicht, was die Datei wirklich braucht.

Entfernt nicht durch Raten, sondern durch Ausprobieren — jede Einbindung
einmal heraus, übersetzen, und wenn es durchgeht, war sie überflüssig.
`tools/lint_unused_includes.sh` macht das wiederholbar.

### Und dieses Werkzeug hat mich fast in dieselbe Falle laufen lassen

Beim ersten Lauf entfernte es **zehn Standard-Köpfe** — und `lint_std.py` war
sofort rot.

Der Grund ist derselbe wie bei `std::min` ohne `<algorithm>`: GCC zieht viele
Standard-Köpfe über andere mit herein, MSVC nicht. Ein Werkzeug, das
„entfernt, was hier noch übersetzt", entfernt genau die, die beim Benutzer
fehlen.

Es fasst Standard-Köpfe jetzt nicht mehr an — dafür ist `lint_std.py`
zuständig. Bei eigenen Köpfen ist GCC nicht nachsichtiger als MSVC, dort ist
das Verfahren sicher.

Zwei Werkzeuge, die sich gegenseitig prüfen, sind mehr wert als eines, dem man
glauben muss.

### Benennung

Mechanisch geprüft, weil Einheitlichkeit das ist, was ein Leser zuerst spürt:

| | |
|---|---|
| Member mit Unterstrich am Ende | 170, davon 0 im Stil `m_x` oder `_x` |
| Konstanten `kName` | 26, davon 0 in GROSSBUCHSTABEN |
| Typen mit großem Anfang | 121, davon 0 klein |
| Funktionen in kleinCamel | 573, davon 0 mit Unterstrich |

Die 21 Ausnahmen sind alle aus der Standardbibliothek (`c_str`,
`emplace_back`).

### `.clang-format` und `README.md`

Die Formatierung liegt jetzt als Datei bei — mit vier begründeten Abweichungen
von Google-Stil. Der Zweck ist nicht Schönheit: eine Änderung soll im
Vergleich nur das zeigen, was sich tatsächlich geändert hat.

Und ein `README.md`, das die Frage beantwortet, die ein Fremder zuerst stellt:
*wofür ist das, was kann es, wie baue ich es, wo fange ich an.*

### Wo ich aufgehört habe

46 Warnungen bei schärfster Einstellung, fast alle
`sign-conversion` an Stellen, wo eine Schleifenvariable von 0 bis 63 läuft und
dann als Index dient. Acht davon habe ich beseitigt, wo es sauber ging.

Den Rest lasse ich stehen. Sie mechanisch wegzucasten würde die Zahl senken
und nichts verbessern — und ein `static_cast`, der nur einen Prüfer
ruhigstellt, ist schlechter als eine Warnung, die man verstanden hat.

## Windows 7: zwei harte Abhängigkeiten beseitigt

Bei der Frage nach Windows 7 kam ein Fund heraus, der **auch auf Windows 10**
zählt.

`SetProcessDpiAwarenessContext` und `GetDpiForWindow` wurden **direkt**
aufgerufen. Solche Aufrufe löst der Lader beim Programmstart auf: fehlt das
Symbol, startet das Programm gar nicht — kein Fenster, keine Meldung, nichts im
Protokoll.

Das ist die unangenehmste Sorte Ausfall, weil sie keine Spur hinterlässt.
Jemand meldet „es startet nicht", und man hat nichts in der Hand.

Beide gehen jetzt über `GetProcAddress`, mit Stufen von neu nach alt bis
hinunter zu `SetProcessDPIAware` (Vista) und `GetDeviceCaps(LOGPIXELSX)`
(Windows 3.1). Welche Stufe gegriffen hat, steht im Protokoll.

### Der eigentliche Blocker liegt nicht im Code

Microsofts Dokumentation ist eindeutig: **Visual Studio 2026 unterstützt
Windows 7 als Ziel nicht mehr.** Das ist genau der Übersetzer, mit dem gebaut
wird.

Die Auswege — VS 2022 mit abgekündigter Unterstützung, MinGW-w64, oder zwei
Fassungen pflegen — stehen in `WINDOWS7.md`, samt Einschätzung. Kurz: der Code
ist nicht mehr der Blocker, der Übersetzer schon.

## Windows 7: der eigentliche Blocker war Direct3D

Die DPI-Aufrufe waren nur die Hälfte. `d3d11` und `d3dcompiler` waren **fest
gebunden** — das erzeugt Einträge in der Importtabelle, und fehlt eine der
DLLs, lädt Windows das Programm gar nicht.

Auf Windows 7 fehlen **beide**: `d3d11.dll` kommt erst mit dem Platform Update,
`d3dcompiler_47.dll` gehört dort überhaupt nicht zum System.

Der Rückfall auf OpenGL half nichts, weil er nie erreicht wurde — das Programm
startete nicht. Ein Rückfall, der hinter einem harten Ausfall liegt, ist kein
Rückfall.

Jetzt über `LoadLibrary`, mit Rückfall auf `d3dcompiler_46` und `_43`. Fehlen
sie, meldet Direct3D sich als nicht verfügbar, und die Wahl fällt auf OpenGL —
mit der fehlenden DLL im Protokoll.

Nebenbei: `dwmapi` war gebunden und wird nirgends benutzt.

### Ein siebter Prüfer

`lint_win7.py` kennt sechzehn Funktionen mit ihrer Mindestfassung und meldet
direkte Aufrufe. Zeilen mit `GetProcAddress` zählen nicht — genau so soll es
gemacht werden.

Er ist wichtig, weil dieser Fehler **nicht beim Übersetzen** auffällt: er
schlägt beim Laden zu, auf dem Rechner eines anderen, ohne jede Spur.

## Die Windows-7-Fassung ist gebaut

MinGW-w64 gibt es auch als Kreuzübersetzer. Damit entsteht die `.exe` hier —
**niemand muss etwas installieren**, weder du noch dein Kumpel.

### Der Fund, den nur der echte Bau brachte

**ImGuis DX11-Rücken ruft selbst `D3DCompile`** und bindet damit
`d3dcompiler_47.dll` fest ein. Unser Laufzeitladen half nicht: der Eintrag kam
aus ImGui, nicht aus uns.

Auf Windows 7 gehört diese DLL nicht zum System — das Programm hätte gar nicht
geladen, trotz aller Vorarbeit. Das findet man nur, wenn man **für das Ziel
baut und in die Importtabelle sieht**, nicht durch Lesen des Quelltexts.

Lösung: `EFX_NO_D3D11`, eine Fassung ohne Direct3D. Auf Windows 7 gibt es das
ohnehin nur mit dem Platform Update.

### Die Prüfung der fertigen Datei

```
Benoetigte DLLs: advapi32 comdlg32 dwmapi gdi32 kernel32
                 msvcrt opengl32 shell32 user32 winmm
Funktionen neuer als Windows 7: keine
Mindest-Windows laut Kopf: 5.2
```

Das ist die beste Prüfung ohne einen echten Windows-7-Rechner: sie schaut nicht
in den Quelltext, sondern in die fertige Datei.

### Zwei Fehler, die der Kreuzübersetzer gefangen hat

`probe.note` gibt es nicht — das Feld heißt `failure`. Und `windowDpi` stand
hinter seiner Benutzung. Beide hätten den MSVC-Bau bei dir zerbrochen.

Das ist das Argument für den Kreuzbau, unabhängig von Windows 7: **er übersetzt
`renderer_d3d11.cpp` und `main_win32.cpp`**, die der bisherige Prüflauf nicht
erreicht.

## Der Einwand: „das SDK kann Windows 7 nativ"

Berechtigt, und ich hatte es zu pauschal dargestellt. Die SDK-Köpfe sind
**nicht** der Blocker — `_WIN32_WINNT=0x0601` gibt genau das frei, was Windows
7 hat.

Übrig bleiben zwei andere Dinge:

**ImGuis DX11-Rücken** bindet `d3dcompiler_47.dll` fest ein. Dafür hat MSVC
etwas, das MinGW nicht hat: `/DELAYLOAD`. Die DLL landet nicht in der
Importtabelle, sondern wird erst beim ersten Aufruf geholt — und dazu kommt es
nie, weil `renderer_d3d11.cpp` vorher mit `LoadLibrary` prüft.

**Damit behält die MSVC-Fassung Direct3D und lädt trotzdem auf Windows 7.**

**Die MSVC-Laufzeitbibliothek** ruft möglicherweise neuere Funktionen. Ob
tatsächlich, lässt sich nicht durch Lesen entscheiden.

### Also ein Werkzeug, das es entscheidet

`tools/check_win7_exe.py` liest die **fertige Datei**: welche DLLs fest
gebunden sind, welche Funktionen importiert werden, was der Kopf verlangt.
Verzögerte Einträge zählen nicht.

Das ist die einzige Prüfung, die den Streit beendet — jede Funktion in der
Importtabelle muss beim Start auflösbar sein, sonst startet nichts. Was dort
nicht steht, kann nicht fehlen.

Gegenprobe mit einer Datei, die `shcore.dll` benutzt: beides erkannt, die DLL
und die Funktion.

## Eine Datei für Windows 7 bis 11

Die Frage war berechtigt: warum zwei Fassungen, wenn eine reichen kann?

Sie kann. **Eine `.exe` nennt keine Windows-Fassung, sie nennt Funktionen** —
und läuft überall, wo die vorhanden sind. Stehen dort nur Funktionen aus
Windows 7, läuft sie auf 7, 8, 10 und 11, denn die neueren haben alles davon
ebenfalls.

Neuere Fähigkeiten werden zur Laufzeit gesucht: Direct3D, der
Shaderübersetzer, die Skalierung je Bildschirm. Da, wenn vorhanden; sonst der
ältere Weg.

### Das letzte Hindernis war ImGui

`imgui_impl_dx11.cpp` ruft `D3DCompile` direkt und bindet `d3dcompiler_47.dll`
über ein `#pragma comment(lib, ...)` mitten in der Datei fest ein.

ImGui nennt die Auswege selbst im Quelltext — Zeile 393:

```
//  1) compile once, save the compiled shader blobs into a file [preferred]
//  2) use code to detect any version of the DLL and grab a pointer to D3DCompile
```

Weg 2, und zwar **ohne die Fremddatei zu ändern**: `gui/imgui_dx11_nodll.h`
biegt `D3DCompile` per Makro auf eine eigene Funktion um, die die DLL zur
Laufzeit sucht (`_47`, `_46`, `_43`).
`gui/imgui_impl_dx11_wrapper.cpp` bindet die Fremddatei danach ein.

Fremden Quelltext zu patchen wäre die schlechtere Wahl gewesen: beim nächsten
ImGui-Wechsel wäre der Patch weg, und niemand merkt es.

Eine Kleinigkeit dabei: das Makro muss **nach** `d3dcompiler.h` stehen, sonst
trifft es die echte Deklaration — die steht in einem `extern "C"`-Block und
passt dann nicht mehr zu unserer C++-Funktion.

### Gemessen an der fertigen Datei

```
d3d11.dll           NICHT gebunden — wird zur Laufzeit geholt
d3dcompiler_47.dll  NICHT gebunden — wird zur Laufzeit geholt
dxgi.dll            NICHT gebunden — wird zur Laufzeit geholt

Direct3D-Code enthalten: ja
Nichts gefunden, was Windows 7 am Laden hindert.
```

Der Code ist drin, die Bindung nicht. Genau das war das Ziel.

`EFX_NO_D3D11` ist damit überflüssig geworden und wieder entfernt — ein
Schalter, den man nicht braucht, ist einer weniger, den jemand falsch setzt.

### Zur OpenGL-Fassung

Wir verlangen **OpenGL 3.3** (März 2010). Windows 7 kann je nach Treiber bis
4.6 — reichlich Luft. Unterhalb von 3.3 fehlen die Vertexpuffer-Objekte, auf
denen ImGui aufsetzt; das Programm meldet dann die gefundene Fassung, statt
abzustürzen.

## „Baut er dann für alle drei Fassungen?"

Nein — und das ist die ehrliche Antwort auf eine berechtigte Erwartung.

```
build.bat  (Visual Studio 2026)
  Windows 10, 11:  sicher
  Windows 8, 8.1:  wahrscheinlich
  Windows 7:       offen — haengt an der MSVC-Laufzeitbibliothek

tools/build_win7.sh  (MinGW)
  Windows 7, 8, 10, 11:  alle
```

Der Quelltext ist identisch. `/DELAYLOAD`, die Laufzeitauflösung von Direct3D
und die DPI-Stufen stecken in beiden. Der Unterschied liegt **allein** in der
Laufzeitbibliothek des Übersetzers.

### Die Antwort steht jetzt am Ende jedes Baus

`build.bat` prüft die fertige `.exe` und sagt es:

```
============================================================
 Auf welchen Windows-Fassungen laeuft die .exe?
============================================================
  ...
  Diese Fassung laedt auch auf Windows 7.
```

Oder, wenn nicht, mit dem Verweis auf `tools/build_win7.sh`. Keine Vermutung,
keine Diskussion — die fertige Datei sagt es.

### Ohne Fremdpaket

Der erste Entwurf brauchte `pefile` (`pip install`). Das passt nicht zu einem
Programm, das ohne Abhängigkeiten auskommt — und niemand soll erst etwas
installieren, um eine Frage beantwortet zu bekommen.

Die Importtabelle selbst zu lesen sind sechzig Zeilen. Gegen `pefile`
gegengeprüft: beide melden dieselbe Subsystemfassung.

Ein Detail dabei, das man leicht falsch macht: **`MajorSubsystemVersion` liegt
bei Versatz 48, nicht bei 40.** Dort steht die `OperatingSystemVersion`, die
Windows gar nicht auswertet. Mein erster Anlauf las 4.0 statt 5.2 — die Zahl
sah plausibel aus, war aber die falsche.

## Vier Punkte aus dem Feldtest

### `.pk3` direkt lesen — war schon fertig

An deiner `z_Update7_Bugfix.pk3` gemessen:

```
97 Eintraege im Archiv
  1 .efx      5 .shader      7 .tga      5 .jpg

EFX   effects/cinematics/hugesparks.efx   2066 Byte, 5 Primitive
SHDR  shaders/ui.shader                   154 Shader
BILD  gfx/sprites/fog.jpg                 gelesen 256x128
BILD  gfx/menus/.../md_clo_boss.tga       gelesen 256x256
```

Effekt, Shader, JPEG und TGA kommen alle direkt aus dem Archiv — ohne
Entpacken, ohne Zwischendatei. Der Deflate-Auspacker und die Bildleser waren
genau dafür da.

Klänge ebenso; in dieser Datei sind keine drin.

### `10 units/foot (WARS)` hat gefehlt

Aus dem Binary geholt, Zeichen für Zeichen:

```
'10 units/foot (WARS)'
'16 units/foot (SOF2)'
```

Zwei Funde in einer Zeile: der Eintrag fehlte **und** ich hatte 16 falsch
beschriftet — im Original steht dort nur `(SOF2)`, nicht `(JKA, SOF2)`. WARS
ist Star Wars Jedi Knight, also genau das Spiel, um das es geht.

Ein Test hielt fest, dass 16 an erster Stelle steht. Das war keine Tatsache,
sondern der Zustand, solange 10 fehlte — der sechste eigene Test dieser Art.

### Mehrere Spielpfade

Ein einzelner reicht nicht: wer an einem Mod arbeitet, hat das Grundspiel an
einer Stelle, den Mod an einer zweiten und vielleicht eigene Dateien an einer
dritten. Ein Effekt aus dem Mod verweist auf einen Shader aus dem Grundspiel —
mit nur einem Pfad findet man den nie.

**Die Reihenfolge entscheidet: der erste Treffer gewinnt.** So überschreibt ein
Mod die Dateien des Grundspiels, genau wie die Engine es mit ihren
`.pk3`-Dateien macht.

```
zwei Spielpfade: 3 Texturen, bei Namensgleichheit gewinnt "MOD"
```

Der Test dreht die Reihenfolge um und prüft, dass sich der Gewinner mitdreht —
sonst wäre es Zufall statt Regel.

Im Dialog: Liste mit Pfeilen zum Umsortieren, „Ordner hinzufügen",
Durchsuchen-Knopf, und die Vorlagen lassen sich mit der **rechten Maustaste**
als zusätzlicher Ordner anhängen. Zwei Klicks zu „Grundspiel plus Mod", ohne
etwas zu tippen.

Jeder gefundene Fund merkt sich, unter welcher Wurzel er lag — sonst ließe er
sich später nicht mehr öffnen.

## Mehrere Effekte gleichzeitig — Reiter wie im Browser

Der größte Umbau seit der Aufteilung von `app.cpp`, und er war mechanisch:
dreizehn Felder, **228 Fundstellen**.

### Die Trennung ist die eigentliche Aussage

```
Je Datei eigen (Document)        Für alle gemeinsam (App)
  effect                           camera_
  filePath, dirty                  settings_
  selectedPrimitive                assets_
  undo                             textureCache_, soundCache_
  particles, clock, paused         audio_
  segmentEnabled, playbackSeed     wallTexture_, roomMesh_
  diagnostics, parseDiagnostics
```

**Was zu einer Datei gehört, steht im Dokument; was zum Programm gehört, bleibt
in App.** Der Texturspeicher, der Materialbestand, die Kamera und das Thema zu
vervielfachen wäre falsch — man würde dieselbe Textur pro Reiter einmal in den
Grafikspeicher legen, und beim Umschalten spränge die Kamera.

Dass die Kamera dem Fenster gehört, ist die Entscheidung, die man beim
Benutzen merkt: wer an einer Explosion und ihrem Aufschlageffekt zugleich
arbeitet, will umschalten, **ohne die Ansicht neu zu stellen**.

### Wie der Umbau abgesichert war

Die 228 Ersetzungen liefen mechanisch (`effect_` → `doc().effect`). Das ist
genau die Art Änderung, bei der ein Tippfehler unbemerkt bleibt — deshalb war
der Übersetzerlauf über alle acht `gui/`-Dateien die eigentliche Absicherung.
Ohne ihn hätte ich das nicht angefasst.

### Ein paar Entscheidungen im Kleinen

**Immer mindestens ein Dokument.** Ein Fenster ohne Dokument gäbe es nur beim
Schließen des letzten Reiters; für diesen einen Fall überall auf Leerheit zu
prüfen wäre der schlechtere Tausch.

**Ein leerer Reiter wird wiederverwendet.** Sonst hätte man nach dem Start
immer einen leeren zu viel.

**Beim Umschalten hält die Wiedergabe an.** Sonst laufen Klänge eines Effekts
weiter, den man gar nicht mehr sieht.

**Die Reiterkennung bleibt stabil** (`###doc0`), auch wenn sich der Titel
ändert — sonst hält ImGui den Reiter für einen neuen, sobald ein Stern dazu
kommt.

**Kein Nachfragedialog beim Schließen.** Der Stern im Titel warnt; ein Dialog
unterbricht, und Rückgängig überlebt das Schließen ohnehin nicht, egal ob man
fragt. Wer es anders will, findet im Quelltext an der Stelle einen Hinweis,
wo man ansetzt.

Tastenkürzel wie im Browser: `Strg+T` neuer Reiter, `Strg+W` schließen,
`Strg+Tab` weiterschalten.

## Ordner statt Datei, und die Knöpfe aus dem Weg

### „Kann er nur aus einer .pk3 lesen?"

Nein — aus **allen gleichzeitig**. An deinem Archiv plus einem zweiten
daneben gemessen:

```
1 Ordner, 2 Archive gefunden
  183 Shader, 13 Texturen, 3 Effekte

Effekte aus BEIDEN Archiven:
   cinematics/hugesparks
   test/eins
   test/zwei
```

Gemeint war immer der **Ordner**, nie eine bestimmte Datei. Dass man im
Dialog trotzdem eine Datei anklicken musste, war der Fehler.

### Die Ordnerauswahl

Vorher gab es nur die Dateiauswahl, und man musste darin irgendeine Datei
anklicken, damit das Programm den Ordner davon nimmt. Das funktioniert, aber
niemand kommt von selbst darauf — und in einem `base`-Ordner voller
`.pk3`-Dateien sieht es aus, als solle man ein Archiv auswählen.

Jetzt `SHBrowseForFolder`. Gewählt statt `IFileDialog` mit `FOS_PICKFOLDERS`:
das braucht COM und gibt es erst ab Vista. `SHBrowseForFolder` gibt es seit
Windows 95, kommt ohne COM aus und tut genau das eine. Mit `BIF_EDITBOX`,
damit man den Pfad auch eintippen kann.

### Die Knöpfe lagen auf dem Text

`SameLine(breite - 150)` setzt sie an eine **feste** Stelle. Bei einem langen
Pfad liegen sie mitten darauf.

Jetzt eine Tabelle mit fester Knopfspalte: der Text bekommt, was übrig bleibt,
und wird abgeschnitten statt überdeckt. Der volle Pfad steht im Hinweis.

## Der Effektbrowser

Viele Effekte nebeneinander, **alle laufend**. Der Zweck ist Suchen, nicht
Bearbeiten: wer in einem Mod nach „dem mit den blauen Funken" sucht, kennt den
Dateinamen nicht — aber er erkennt den Effekt, sobald er ihn sieht.

Genau deshalb laufen die Vorschauen. Ein Standbild von einer Explosion nach
20 ms ist ein schwarzes Rechteck.

### Drei Entscheidungen, ohne die es nicht liefe

**Nur die sichtbaren Kacheln werden belebt.** Bei tausend Effekten wären das
tausend Simulationen; man sieht ohnehin nur ein Dutzend. ImGuis
Sichtbarkeitsprüfung sagt, welche.

**Keine 3D-Ansicht je Kachel.** Sechzig Ansichten wären sechzig
Renderdurchgänge je Bild. Stattdessen werden die Partikel als flache Punkte
direkt in die Kachel gemalt, in leichter Schrägsicht. Für das Wiedererkennen
genügt das — Größe, Farbe und Bewegung sind das, was man erkennt, nicht die
Perspektive.

**Der Maßstab wird einmal bestimmt und behalten.** Die Bahn wird einmal
abgeschritten, um die Reichweite zu finden. Bei jedem Bild neu gerechnet
zappelte die Größe mit jedem Partikel, der auftaucht.

Dazu: ein eigener Ausgangswert je Kachel, damit nicht alle im Gleichschritt
zappeln — nebeneinander fällt das sofort auf.

### Doppelklick öffnet in einem neuen Reiter

Dafür gibt es sie. Der Pfad bleibt dabei **leer**: der Effekt kommt aus einem
Archiv, und dorthin zurückschreiben kann man nicht. „Speichern" wird damit zu
„Speichern unter", und das ist richtig so.

## Der Browser wird zur Startseite

Er war ein Extrafenster — man konnte es zumachen und fand es dann nicht
wieder. Das war falsch herum gedacht.

Jetzt ist er der **erste Reiter**, und der lässt sich nicht schließen. So gibt
es immer einen Weg zurück zu den Kacheln, genau wie die Startseite eines
Browsers.

Beim Programmstart landet man dort, bevor irgendetwas offen ist.

### Ohne Spielpfad zeigt die Startseite die Wege, die von hier wegführen

Eine leere Kachelfläche wäre nutzlos. Stattdessen vier Schaltflächen:
Spielpfad setzen, `.pk3` öffnen, `.efx` öffnen, neuer leerer Effekt.

**Das ist der Zweck einer Startseite** — nicht Inhalt zeigen, den es noch nicht
gibt, sondern zeigen, wie man zu Inhalt kommt.

### „Archiv öffnen" gab es nicht

Wer eine Mod-Datei bekommen hat, will hineinsehen — nicht seinen Spielpfad
umstellen und dabei die eigene Einrichtung zerlegen.

`assets::scanArchive` liest ein einzelnes `.pk3` als Bestand und mischt es in
den vorhandenen ein. Der Spielpfad bleibt unberührt; die Archivpfade kommen
dazu, damit Texturen und Klänge daraus später auch gefunden werden.

```
ein Archiv, sein Shader wurde gelesen, sein Modell gefunden
aus einer kaputten Datei kommt nichts, und es steht eine Begruendung dabei
```

Zu finden unter *Datei → PK3-Archiv öffnen* und auf der Startseite.

### Alles über Menüpunkte erreichbar

45 Menüpunkte insgesamt. `Strg+B` schaltet zwischen Startseite und
Bearbeitung, der Menüpunkt zeigt mit einem Haken, wo man gerade ist.

## Texturierte Kacheln — ein Ziel statt sechzig

Die Kacheln zeigten nur Punkte und Linien. Für „welcher ist der mit den blauen
Funken" reicht das, für „welcher Rauch ist dichter" nicht.

### Die Messung, auf der der Entwurf steht

Nachgeschlagen statt geraten: **den Zielpuffer zu wechseln ist das Teure, nicht
das Zeichnen.** Fünfzig einzelne Renderziele in einem Bild kosten rund 12 ms,
dieselbe Arbeit in einem Ziel gebündelt rund 1,3 ms — Faktor zehn, und zwar für
Arbeit, die man gar nicht leisten müsste. Ein Teilbereich desselben Ziels
vermeidet die Puffer-Leerungen.

Also: **ein** Ansichtsziel für alle Kacheln, je Kachel nur der Ausschnitt
umgestellt. Sechzig sichtbare Kacheln kosten sechzig Zeichenaufrufe statt
sechzig Zielwechsel.

`Renderer::setViewportRect` gibt es jetzt in beiden Schnittstellen. Dazu ein
Schnittrechteck: der Ausschnitt allein wirkt auf die Eckpunkte, bei dicken
Linien kann trotzdem etwas darüber hinausragen — und in einem Raster liefe das
in die Nachbarkachel.

Bei OpenGL musste die Y-Achse gedreht werden: OpenGL zählt von unten links, die
Oberfläche von oben. Ohne das landen die Kacheln spiegelverkehrt im Raster, und
zwar erst auffallend, wenn das Raster nicht quadratisch ist.

### Kein zweites Ziel

Der Browser benutzt **dasselbe** Ansichtsziel wie die Bearbeitungsansicht. Die
beiden sind nie gleichzeitig sichtbar — der Browser ist der Startreiter. Ein
zweites Ziel wäre Grafikspeicher für nichts.

### Die parallele Simulation, und was ich dazu nicht sagen kann

Der Aufbau der Geometrie hängt zwischen den Kacheln an nichts: jede hat ihr
eigenes System und ihre eigene Uhr. Also alle auf einmal über den vorhandenen
Arbeitsverteiler, Körnung 1 — selten sind mehr als ein Dutzend Kacheln
sichtbar, bei gröberer Körnung bliebe auf einem 32-Kern-Rechner die Hälfte
ungenutzt.

**Wie viel das bringt, kann ich hier nicht messen:**

```
Kerne laut System: 1
Faeden im Pool:    0
```

Der Container hat einen einzigen Kern. Die Messung ergab Faktor 1.0 — eine
Eigenschaft dieser Umgebung, nicht des Codes. Dasselbe erklärt übrigens
rückwirkend eine Zahl aus der Testreihe, die mich schon einmal gewundert hat:
„4 erzwungene Fäden, Faktor 0.98".

Was sich messen lässt, ist das Wichtigere: **dass verteilt dasselbe herauskommt
wie der Reihe nach.**

```
24 Kacheln, 11520 Eckpunkte verglichen
mit 8 erzwungenen Faeden: 24 Kacheln, Ergebnis gleich
```

Mit erzwungenen Fäden, weil der Pool auf einem Kern gar keine anlegt und der
Test dann nichts über das Verteilen aussagen würde. Dazu ein Durchlauf unter
dem Fadenwächter: keine Wettläufe.

Das ist die ehrliche Trennung — die Geschwindigkeit musst du auf deinen 32
Kernen selbst sehen, die Richtigkeit ist geprüft.

## Drei Fehler aus der Kachelrunde

### Die Meldung, die an der Maus klebt

Mein früher Ausstieg auf der Startseite übersprang das `ImGui::End()` des
Hauptfensters:

```cpp
ImGui::Begin("##main", ...);       // Zeile 861
...
if (startTabActive_) { ...; return; }   // Zeile 894 — springt heraus
...
ImGui::End();                      // Zeile 964 — nie erreicht
```

ImGui merkt das und hängt eine Fehlermeldung an den Mauszeiger. Genau die war
zu sehen — sie war kein Anzeigefehler, sondern **die richtige Diagnose**.

Ein früher Ausstieg zwischen `Begin` und `End` ist immer falsch; die beiden
gehören zusammen wie öffnende und schließende Klammer.

### Die Startseite ließ sich nicht verlassen

```cpp
ImGui::BeginTabItem(..., startTabActive_ ? ImGuiTabItemFlags_SetSelected : ...)
```

`SetSelected` wählt den Reiter aus — bei **jedem** Bild, solange man das
Kennzeichen mitgibt. Wer es an den Zustand koppelt, hält den Reiter für immer
fest: man klickt einen anderen an und ist im nächsten Bild wieder da.

Jetzt ein eigenes, einmaliges Kennzeichen, das nach der ersten Verwendung
gelöscht wird.

Beide Fehler haben dieselbe Form: **ein Zustand wird jedes Bild neu
durchgesetzt, obwohl er nur einmal gemeint war.**

### Die Kacheln waren schlechter zu erkennen als vorher

Mein Kameraabstand war geraten — über Zoomstufen und einen Logarithmus. Ergebnis:
mal zu klein, mal abgeschnitten.

`Orbit::distanceToFit` rechnet ihn jetzt aus:

```
d = radius / tan(fov/2)
```

Eine Zeile Trigonometrie statt Probieren. Der Test prüft die Umkehrung:

```
Reichweite 80 -> Abstand 138.6, halbe Bildhoehe 80.0
```

Bei diesem Abstand ist die halbe Bildhöhe genau die Reichweite — randvoll, auf
0,0 genau. Dazu ein Randfaktor von 1.25, sonst klebt der Effekt am Rahmen.

Dass das flache Punktbild vorher besser aussah, lag also nicht am Verfahren,
sondern daran, dass es keinen Abstand zu raten hatte.

### Zu den Texturen

Ja. Die Kacheln gehen durch denselben Texturspeicher wie die Hauptansicht —
Shader werden aufgelöst, Bilder aus Ordnern und `.pk3` geladen, und jede
Textur liegt nur **einmal** im Grafikspeicher, weil der Speicher dem Programm
gehört und nicht dem Dokument.

## Der Muzzleflash war unsichtbar — Billboards und Kameraachsen

Der beste Fehlerbericht dieser Runde: „im Editor richtig, in der Vorschau
nicht". Zwei Wege für dieselben Daten, einer stimmt — dann liegt es am
Unterschied.

### Der Unterschied

```
Editor:   Achsen aus der Blickmatrix  -> Billboard zeigt zur Kamera
Kachel:   feste Achsen (1,0,0)/(0,0,1), Kamera danach gedreht
```

Ein Billboard ist eine **Fläche ohne Dicke**. Steht sie hochkant zur Kamera,
ist sie unsichtbar.

Ein Muzzleflash besteht oft aus *einem* solchen Viereck und verschwand deshalb
ganz. Eine Funkenwolke aus hundert Teilchen sah nur dünner aus — und das war
das diffuse „irgendwie schlechter als vorher" aus der Runde davor. **Es war
dieselbe Ursache, nur schwächer sichtbar.**

Die Kameras werden jetzt **vor** dem Aufbau der Geometrie berechnet, und ihre
Achsen gehen hinein. Nebenbei fiel dabei auf, dass ich sie ohnehin zweimal
gerechnet hatte.

### Der Test dazu

Über den ganzen Umlauf gemessen, statt einen Winkel zu raten:

```
ueber 360 Grad: Kameraachsen mindestens 0.996, feste Achsen mindestens 0.016
```

1.0 heißt frontal, 0.0 heißt hochkant. Mit festen Achsen wird die Fläche
irgendwo praktisch vollständig unsichtbar.

Mein erster Anlauf nahm 90 Grad an und erwartete dort den Fehler — dort stand
die Kamera aber zufällig noch günstig (0.944), und der Test wurde rot, obwohl
die Aussage stimmte. **Ein einzelner Winkel beweist nichts; der ungünstigste
tut es.**

### Und noch eine Abkürzung, die falsch war

In den Kacheln stand fest `Blend::Additive` für alles — dieselbe Abkürzung, die
in der Hauptansicht schon einmal falsch war. Ein alphagemischter Rauch sieht
damit völlig anders aus als im Spiel.

Jetzt wie im Editor: Mischart aus dem Shader, undurchsichtige Flächen zuerst
(additive Mischung ist kommutativ, Alphamischung nicht), und nur
Undurchsichtiges schreibt in den Tiefenpuffer.
