# Windows 7

Was nötig wäre, und was ich schon gemacht habe.

## Der Einwand deines Kumpels — und was daran stimmt

> „Das Windows-10-SDK unterstützt Windows 7 nativ, das sollte einfach sein."

**Das stimmt, und zwar genau für den Teil, den er meint.** Die SDK-Köpfe geben
mit `_WIN32_WINNT=0x0601` frei, was Windows 7 hat — sie sind nicht der Blocker.

Was übrig bleibt, sind zwei andere Dinge, und beide lassen sich lösen:

1. **Die MSVC-Laufzeitbibliothek** ruft selbst neuere Funktionen. Microsoft
   sagt „nicht unterstützt". Ob es *tatsächlich* bricht, sieht man nur in der
   Importtabelle der fertigen Datei.
2. **ImGuis DX11-Rücken** bindet `d3dcompiler_47.dll` fest ein.

Für Punkt 2 hat MSVC etwas, das MinGW nicht hat: `/DELAYLOAD`.

Microsofts Dokumentation zur Unterstützung:

> Visual Studio 2026 18.0 und später unterstützt das Zielen auf Windows 7/8/8.1
> nicht mehr. Visual Studio 2026 und später zielen auf Windows 10 oder später.

Das ist genau der Übersetzer, mit dem du baust (MSVC 19.50, VS 18). Egal, was
im Quelltext steht — das Ergebnis läuft nicht auf Windows 7.

### Drei Auswege, alle mit Preis

**Visual Studio 2022 (Toolset v143).** Kann Windows 7 SP1 als Ziel. Microsoft
hat die Unterstützung zwischenzeitlich entfernt und wieder eingesetzt, mit dem
Vermerk, dass sie endgültig verschwinden wird. Wer darauf baut, baut auf etwas,
das abgekündigt ist.

**MinGW-w64 oder Clang.** Beide können Windows 7 als Ziel. Der Bau müsste
umgestellt werden; `build.bat` und die CMake-Datei gehen von MSVC aus. Machbar,
aber ein eigener Arbeitsgang — und die statisch gebundene Laufzeit sähe anders
aus.

**Zwei Fassungen pflegen.** Eine für Windows 10 aufwärts mit dem aktuellen
Übersetzer, eine für Windows 7 mit einem älteren. Doppelte Prüfung, doppelte
Pakete.

## Was ich im Code schon behoben habe

Unabhängig vom Übersetzer gab es **zwei harte Abhängigkeiten**, die ich beseitigt
habe — sie sind auch auf Windows 10 eine Verbesserung.

### `SetProcessDpiAwarenessContext` und `GetDpiForWindow`

Beide wurden **direkt** aufgerufen. Solche Aufrufe werden beim Laden des
Programms aufgelöst: fehlt das Symbol, startet das Programm **gar nicht** — kein
Fenster, keine Meldung, nichts im Protokoll.

Das ist die Sorte Fehler, die man auf einem fremden Rechner nicht
diagnostiziert. Jetzt beide über `GetProcAddress`, mit Stufen:

```
Windows 10 1703   SetProcessDpiAwarenessContext   (je Bildschirm, Fassung 2)
Windows 8.1       SetProcessDpiAwareness          (shcore.dll)
Windows Vista     SetProcessDPIAware              (nur an/aus)
sonst             nichts — klein, aber lauffähig
```

Für die Auflösung entsprechend: `GetDpiForWindow`, sonst
`GetDeviceCaps(LOGPIXELSX)` — das gibt es seit Windows 3.1. Es kennt nur den
Hauptbildschirm, nicht die Auflösung je Fenster; bei einem Bildschirm ist das
dasselbe, bei zweien eine Näherung. **Besser als gar nicht starten.**

Welche Stufe gegriffen hat, steht im Ablaufprotokoll.

## Was sonst noch zu tun wäre

| | |
|---|---|
| **Direct3D 11** | ✅ zur Laufzeit geladen; fehlt es, greift OpenGL |
| **`_WIN32_WINNT`** | ✅ gesetzt |
| **Weitere APIs** | ✅ keine mehr, `lint_win7.py` hält es so |
| **Prüfen** | auf einer echten Windows-7-Installation. Ohne das ist jede Aussage eine Vermutung |

## Was jetzt gemacht ist

### Direct3D wird zur Laufzeit geladen

Das war der **eigentliche** Blocker, und er war größer als die DPI-Sache.

`d3d11` und `d3dcompiler` waren fest gebunden. Das erzeugt Einträge in der
Importtabelle: fehlt eine der beiden DLLs, lädt Windows das Programm gar nicht.

Auf Windows 7 fehlen **beide** — `d3d11.dll` kommt erst mit dem Platform
Update, und `d3dcompiler_47.dll` gehört dort überhaupt nicht zum System.

Der Rückfall auf OpenGL half nichts, weil er nie erreicht wurde: das Programm
startete nicht.

Jetzt über `LoadLibrary`, mit Rückfall auf ältere Fassungen des
Shaderübersetzers (`_46`, `_43`). Fehlen sie, meldet Direct3D sich als nicht
verfügbar, und die Wahl fällt auf OpenGL — mit einer Zeile im Protokoll, die
sagt, welche DLL fehlte.

`dwmapi` war ebenfalls gebunden und wird nirgends benutzt. Entfernt.

### `build_mingw.bat`

Ein zweiter Bauweg mit MinGW-w64. Er setzt `_WIN32_WINNT=0x0601` und bindet
statisch (`-static -static-libgcc -static-libstdc++`) — also wieder **eine
einzelne .exe** ohne mitzuliefernde DLLs, wie bei der MSVC-Fassung.

Der bestehende Bau bleibt unverändert; die Datei liegt daneben.

### `tools/lint_win7.py`

Prüft, ob wieder ein Aufruf hineinrutscht, den Windows 7 nicht kennt. Sechzehn
Funktionen mit ihrer Mindestfassung; Zeilen mit `GetProcAddress` zählen nicht,
denn genau so soll es gemacht werden.

Der Prüfer ist wichtig, weil dieser Fehler **nicht beim Übersetzen** auffällt.
Er schlägt beim Laden zu, auf dem Rechner eines anderen.

### `_WIN32_WINNT=0x0601` in der CMake-Datei

Damit meldet der Übersetzer neuere Aufrufe, statt sie stillschweigend
anzunehmen. Man findet sie also beim Bauen und nicht bei deinem Kumpel.

## Meine Einschätzung

Der Code ist **nicht mehr der Blocker**. Was bleibt, ist der Übersetzer,
und dort gibt es keine saubere Lösung mehr — nur abgekündigte oder umständliche.

Ob sich das lohnt, hängt daran, ob jemand JKA noch auf Windows 7 moddet.
Windows 7 hat seit Januar 2020 keine Sicherheitsaktualisierungen mehr; wer
heute damit im Netz ist, hat größere Sorgen als ein Effekt-Werkzeug.

Falls du es trotzdem willst, wäre **MinGW-w64** mein Vorschlag: es ist der
einzige Weg, der nicht auf etwas Abgekündigtem steht, und der Kern übersetzt
schon heute mit GCC — die Testreihe läuft hier damit.

## Für deinen Kumpel

1. **MSYS2** installieren, dann `pacman -S mingw-w64-x86_64-gcc` und
   `pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja`
2. In der MinGW-64-Bit-Eingabeaufforderung `build_mingw.bat` aufrufen
3. `out\efxed.exe` läuft dann auf Windows 7 SP1 — als einzelne Datei

Falls es nicht startet, ist die erste Anlaufstelle
`%APPDATA%\efxed\efxed_start.log`. Dort steht, welche Grafikschnittstelle
gewählt wurde und warum, welche DPI-Stufe gegriffen hat, und was beim
Schriftaufbau passiert ist.

**Was ich nicht versprechen kann:** dass es läuft. Ich habe kein Windows 7, und
niemand hat es je darauf gestartet. Was ich sagen kann: alle Stellen, an denen
ich einen Grund zum Scheitern gefunden habe, sind abgefangen und melden sich im
Protokoll.

Wenn dein Kumpel es probiert, ist dieses Protokoll das Wertvollste, was er mir
schicken kann — auch und gerade, wenn es nicht funktioniert.

---

# Nachtrag: die fertige .exe

**Niemand muss etwas installieren.** Ich habe die Windows-7-Fassung hier gebaut
— mit MinGW-w64 als Kreuzübersetzer. Dein Kumpel entpackt und startet.

## Was die Prüfung der fertigen Datei ergab

```
Benoetigte DLLs:
  advapi32  comdlg32  dwmapi  gdi32  kernel32
  msvcrt    opengl32  shell32  user32  winmm      — alle ab Windows 7 da

Importierte Funktionen, die Windows 7 nicht hat:
  keine

Mindest-Windows laut Kopf: 5.2
```

Alle DLLs gehören zu Windows 7. Keine einzige Funktion aus der Importtabelle
ist neuer. Der Kopf meldet sogar 5.2 als Mindestfassung.

Das ist die beste Prüfung, die ohne einen echten Windows-7-Rechner möglich ist:
sie schaut nicht in den Quelltext, sondern in die **fertige Datei**.

## Ohne Direct3D — und warum das nötig war

Beim Bauen kam ein Fund heraus, den ich vorher nicht hatte:

**ImGuis DX11-Rücken ruft selbst `D3DCompile`.** Er bindet `d3dcompiler_47.dll`
fest ein — die gehört auf Windows 7 nicht zum System, und das Programm würde
gar nicht erst laden.

Unser eigenes Laufzeitladen half dagegen **nicht**: der Eintrag kam aus ImGui,
nicht aus uns. Ein Fehler, den man nur findet, wenn man tatsächlich für das
Ziel baut und in die Importtabelle sieht.

Also eine Fassung ohne Direct3D, gesteuert über `EFX_NO_D3D11`. Auf Windows 7
gibt es Direct3D 11 ohnehin nur mit dem Platform Update; eine Fassung ohne ist
dort die ehrlichere — **sie startet immer**.

## `tools/build_win7.sh`

Der Kreuzbau ist als Skript hinterlegt, damit er wiederholbar ist. Er braucht
nur `g++-mingw-w64-x86-64` und die ImGui-Quellen.

## Was ich weiterhin nicht versprechen kann

Dass es läuft. Die Datei wurde nie auf Windows 7 gestartet.

Was ich sagen kann: sie **lädt** dort — das sagt die Importtabelle, und das war
der harte Blocker. Was danach kommt, sagt das Ablaufprotokoll.

---

# Nachtrag: der Einwand war berechtigt

## `/DELAYLOAD` löst das Direct3D-Problem — bei MSVC

`d3d11.dll`, `d3dcompiler_47.dll` und `dxgi.dll` werden in der MSVC-Fassung
jetzt **verzögert** gebunden. Sie landen nicht in der Importtabelle, sondern
werden erst beim ersten Aufruf geholt.

Fehlen sie, kommt es nie dazu: `renderer_d3d11.cpp` prüft vorher mit
`LoadLibrary`, meldet Direct3D als nicht verfügbar, und die Wahl fällt auf
OpenGL.

**Damit behält die MSVC-Fassung Direct3D und lädt trotzdem auf Windows 7.** Die
MinGW-Fassung kann das nicht — MinGW kennt kein `/DELAYLOAD`, deshalb ist sie
ohne Direct3D gebaut.

## `tools/check_win7_exe.py` — die Frage entscheiden statt diskutieren

```
python3 tools/check_win7_exe.py out\efxed.exe
```

Das Werkzeug liest die **fertige Datei**, nicht den Quelltext. Es prüft:

- welche DLLs fest gebunden sind, und ob es die auf Windows 7 gibt
- welche Funktionen importiert werden, und ob Windows 7 sie kennt
- was der Kopf als Mindestfassung verlangt
- verzögert gebundene Einträge werden **nicht** als Problem gezählt

Das ist die einzige Prüfung, die den Streit wirklich beendet: jede Funktion in
der Importtabelle muss beim Programmstart auflösbar sein, sonst startet nichts.
Was dort nicht steht, kann auch nicht fehlen.

An der MinGW-Fassung gemessen:

```
Fest gebundene DLLs: advapi32 comdlg32 dwmapi gdi32 kernel32
                     msvcrt opengl32 shell32 user32 winmm  — alle ok
Mindestfassung im Kopf: 5.2
Nichts gefunden, was Windows 7 am Laden hindert.
```

Gegenprobe mit einer Datei, die `shcore.dll` benutzt (Windows 8.1):

```
shcore.dll                                     PROBLEM
shcore.dll!SetProcessDpiAwareness gibt es auf Windows 7 nicht
```

Beides erkannt — die DLL und die Funktion.

## Was jetzt zu tun ist

**Bau ganz normal mit Visual Studio** — `_WIN32_WINNT=0x0601` und `/DELAYLOAD`
stehen schon in der CMake-Datei. Dann:

```
python3 tools\check_win7_exe.py out\efxed.exe
```

Meldet es nichts, hatte dein Kumpel recht und die MSVC-Fassung läuft auf
Windows 7 — mit Direct3D, wo verfügbar.

Meldet es etwas, steht **genau da**, welche Funktion oder DLL im Weg ist. Dann
weiß man, worüber man redet, und die MinGW-Fassung liegt als Rückfall bereit.

Ich tippe darauf, dass etwas aus der MSVC-Laufzeitbibliothek auftaucht — aber
das ist eine Vermutung, und das Werkzeug ersetzt sie durch eine Antwort.
