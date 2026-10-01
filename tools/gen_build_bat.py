# -*- coding: utf-8 -*-
# build.bat wird erzeugt statt getippt, weil sie zwingend CRLF braucht.
# Bei g2c hat genau das einmal einen halben Abend gekostet: die Datei hatte
# Unix-Zeilenenden, und cmd.exe hat sie stillschweigend falsch ausgefuehrt.
#
# Kein Umlaut in dieser Datei: cmd.exe laeuft in Codepage 850 oder 437, und
# UTF-8-Umlaute erscheinen dort als Zeichensalat.
lines = r'''@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1

REM ===================================================================
REM  EffectsEd - Bauen
REM
REM  Der gesamte Rumpf laeuft ueber "call :main". Grund: springt etwas
REM  vorzeitig zu :eof, landen wir hinter dem call und nicht im Nichts -
REM  das Fenster bleibt offen und man kann die Meldung lesen. Ohne diesen
REM  Umweg schliesst sich das Fenster bei einem Fehler sofort.
REM ===================================================================

call :main %*
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
    echo ============================================================
    echo  Fertig.
    echo ============================================================
) else (
    echo ============================================================
    echo  Mit Fehlern beendet. Meldungen stehen weiter oben.
    echo ============================================================
)
if not "%1"=="/nopause" pause
exit /b %RC%


:main
REM -------------------------------------------------------------------
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
cd /d "%ROOT%"

echo ============================================================
echo  EffectsEd - Bauen
echo ============================================================
echo  Verzeichnis: %ROOT%
echo.

REM --- Voraussetzung: CMake -----------------------------------------
where cmake >nul 2>&1
if errorlevel 1 (
    echo [FEHLER] CMake wurde nicht gefunden.
    echo          Von https://cmake.org holen und beim Installieren
    echo          "Add to PATH" ankreuzen.
    exit /b 1
)
for /f "tokens=3" %%V in ('cmake --version ^| findstr /r "^cmake version"') do set "CMAKEVER=%%V"
echo  CMake      : !CMAKEVER!

REM --- Voraussetzung: Visual Studio ---------------------------------
REM  Absichtlich OHNE -G: der Generator wird nicht festgenagelt.
REM  Bei einem frueheren Projekt stand hier "Visual Studio 17 2022"
REM  fest verdrahtet, und auf einem Rechner mit VS 2026 schlug jeder
REM  einzelne Aufruf mit "could not find any instance of Visual Studio"
REM  fehl. CMake findet die neueste installierte Fassung selbst.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName`) do set "VSNAME=%%I"
    if defined VSNAME (
        echo  Visual Stu.: !VSNAME!
    ) else (
        echo  Visual Stu.: gefunden, aber ohne C++-Werkzeuge
        echo.
        echo [FEHLER] Im Visual-Studio-Installer fehlt der Baustein
        echo          "Desktopentwicklung mit C++".
        exit /b 1
    )
) else (
    echo  Visual Stu.: vswhere nicht gefunden, CMake sucht selbst
)
echo.

REM --- Was soll gebaut werden? --------------------------------------
set "TARGET=%1"
if "%TARGET%"=="/nopause" set "TARGET="
if "%TARGET%"=="" set "TARGET=alles"

if /i "%TARGET%"=="sauber" (
    echo --- Aufraeumen ---
    if exist "%ROOT%\build" rmdir /s /q "%ROOT%\build"
    if exist "%ROOT%\out"   rmdir /s /q "%ROOT%\out"
    echo  build und out entfernt.
    exit /b 0
)

REM --- Quellen gegen das Bauskript pruefen ---------------------------
REM
REM Vor dem Uebersetzen, nicht danach. Eine Quelldatei, die in
REM CMakeLists.txt fehlt, faellt erst beim BINDEN auf - also nach mehreren
REM Minuten Uebersetzen, mit einer Meldung ueber ein "nicht aufgeloestes
REM externes Symbol", die den eigentlichen Grund nicht nennt.
if defined PYEXE (
    !PYEXE! "%ROOT%\tools\lint_sources.py"
    if errorlevel 1 (
        echo.
        echo [FEHLER] Eine Quelldatei fehlt in CMakeLists.txt ^(siehe oben^).
        echo          Ohne sie gibt es beim Binden LNK2019.
        echo.
        pause
        exit /b 1
    )
)

REM --- Einrichten ----------------------------------------------------
echo ============================================================
echo  Einrichten
echo ============================================================
if not exist "%ROOT%\build" mkdir "%ROOT%\build"

REM  Kein -DCMAKE_BUILD_TYPE: der Visual-Studio-Generator kennt mehrere
REM  Konfigurationen und waehlt sie erst beim Uebersetzen. CMake wirft die
REM  Variable sonst mit "Manually-specified variables were not used" zurueck.
REM  Die Konfiguration steht unten beim --build.
cmake -S "%ROOT%" -B "%ROOT%\build" -A x64
if errorlevel 1 (
    echo.
    echo [FEHLER] CMake konnte das Projekt nicht einrichten.
    echo          Haeufigste Ursache: der Baustein "Desktopentwicklung
    echo          mit C++" fehlt im Visual-Studio-Installer.
    exit /b 1
)
echo.

REM --- Uebersetzen ---------------------------------------------------
echo ============================================================
echo  Uebersetzen
echo ============================================================
cmake --build "%ROOT%\build" --config Release --parallel
if errorlevel 1 (
    echo.
    echo [FEHLER] Das Uebersetzen ist fehlgeschlagen.
    exit /b 1
)
echo.

REM --- Bereitstellen -------------------------------------------------
if not exist "%ROOT%\out" mkdir "%ROOT%\out"
for %%F in (efxtool.exe efxed.exe) do (
    if exist "%ROOT%\build\Release\%%F" (
        copy /y "%ROOT%\build\Release\%%F" "%ROOT%\out\" >nul
        echo  bereit: out\%%F
    )
)
echo.

REM --- Tests ---------------------------------------------------------
if /i "%TARGET%"=="nurbauen" (
    echo  Tests uebersprungen ^(Aufruf mit "nurbauen"^).
    exit /b 0
)

REM --- Uebersetzungspruefung -----------------------------------------
REM  Findet Oberflaechentexte, die nicht durch tr() laufen.
REM
REM  "where python" allein reicht nicht: Windows legt unter
REM  %LOCALAPPDATA%\Microsoft\WindowsApps einen Platzhalter namens python.exe
REM  ab, der nur den Microsoft Store oeffnet. "where" findet ihn, der Aufruf
REM  gibt aber "Python wurde nicht gefunden" aus und liefert einen
REM  Fehlercode, und die Pruefung meldete faelschlich fehlende
REM  Uebersetzungen.
REM
REM  Deshalb: der Reihe nach py, python3 und python probieren und jeweils
REM  pruefen, ob wirklich ein Python antwortet.
set "PYEXE="
for %%P in (py python3 python) do (
    if not defined PYEXE (
        %%P --version >nul 2>&1 && set "PYEXE=%%P"
    )
)

if defined PYEXE (
    echo ============================================================
    echo  Uebersetzungspruefung  ^(!PYEXE!^)
    echo ============================================================
    !PYEXE! "%ROOT%\tools\lint_i18n.py"
    if errorlevel 1 (
        echo [WARNUNG] Es gibt nicht uebersetzte Oberflaechentexte.
    )
    !PYEXE! "%ROOT%\tools\lint_members.py"
    if errorlevel 1 (
        echo [WARNUNG] Unbekannte Membernamen in gui\ - siehe oben.
    )
    !PYEXE! "%ROOT%\tools\lint_includes.py"
    if errorlevel 1 (
        echo [WARNUNG] Fehlende #include in gui\ - siehe oben.
    )
    !PYEXE! "%ROOT%\tools\lint_calls.py"
    if errorlevel 1 (
        echo [WARNUNG] Unbekannte Aufrufe in gui\ - siehe oben.
    )
    !PYEXE! "%ROOT%\tools\lint_gl.py"
    if errorlevel 1 (
        echo [WARNUNG] OpenGL-Aufrufe ohne gl:: - siehe oben.
    )
    !PYEXE! "%ROOT%\tools\lint_std.py"
    if errorlevel 1 (
        echo [WARNUNG] Fehlende Standard-Koepfe - siehe oben.
    )
    !PYEXE! "%ROOT%\tools\lint_win7.py"
    if errorlevel 1 (
        echo [WARNUNG] Aufrufe neuer als Windows 7 - siehe oben.
    )
    !PYEXE! "%ROOT%\tools\lint_ns.py"
    if errorlevel 1 (
        echo [WARNUNG] Namensraum ohne seinen Kopf - siehe oben.
    )
    !PYEXE! "%ROOT%\tools\lint_gpu.py"
    if errorlevel 1 (
        echo [WARNUNG] Texturkennung ueberlebt den Renderer-Wechsel - siehe oben.
    )

    rem Die fertige .exe darauf pruefen, auf welchen Windows-Fassungen sie
    rem ueberhaupt laedt. Das beantwortet die Frage, die sich nicht durch
    rem Lesen klaeren laesst: die MSVC-Laufzeitbibliothek ruft moeglicherweise
    rem Funktionen, die es auf Windows 7 nicht gibt.
    if exist "%ROOT%\out\efxed.exe" (
        echo.
        echo ============================================================
        echo  Auf welchen Windows-Fassungen laeuft die .exe?
        echo ============================================================
        !PYEXE! "%ROOT%\tools\check_win7_exe.py" "%ROOT%\out\efxed.exe"
        if errorlevel 1 (
            echo.
            echo  Diese Fassung laeuft auf Windows 8 aufwaerts, nicht auf 7.
            echo  Eine Fassung fuer 7 bis 11 baut tools/build_win7.sh mit
            echo  MinGW - siehe WINDOWS7.md.
        ) else (
            echo.
            echo  Diese Fassung laedt auch auf Windows 7.
        )
    )
    rem tools/check_gui.sh uebersetzt gui/app.cpp gegen die echten
    rem ImGui-Koepfe. Das laeuft auf dem Entwicklungsrechner, nicht hier -
    rem auf Windows uebersetzt ohnehin MSBuild die ganze Datei.
    echo.
) else (
    echo  Uebersetzungspruefung uebersprungen: kein Python gefunden.
    echo.
)

echo ============================================================
echo  Testlauf
echo ============================================================
if not exist "%ROOT%\build\Release\efxtests.exe" (
    echo [WARNUNG] efxtests.exe wurde nicht gebaut, Tests entfallen.
    exit /b 0
)
"%ROOT%\build\Release\efxtests.exe" "%ROOT%\data"
if errorlevel 1 goto :testfail
if exist "%ROOT%\build\Release\efx_imgui_compat.exe" (
    echo.
    "%ROOT%\build\Release\efx_imgui_compat.exe"
)
:testfail
if errorlevel 1 (
    echo.
    echo [FEHLER] Mindestens eine Pruefung ist fehlgeschlagen.
    echo          Die Programme liegen trotzdem in out\ - benutze sie
    echo          aber nicht zum Bearbeiten wichtiger Dateien, bevor
    echo          die Ursache geklaert ist.
    exit /b 1
)

exit /b 0
'''
# CRLF erzwingen
import os, sys
target = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      'build.bat')
data = lines.replace('\n', '\r\n').encode('utf-8')

# Zwei Bedingungen, die cmd.exe stillschweigend bestraft. Beide sind schon
# einmal durchgerutscht, deshalb pruefen wir sie beim Erzeugen statt spaeter.
crlf_ok = data.count(b'\n') == data.count(b'\r\n')
bad = [i for i, b in enumerate(data) if b > 127]
if not crlf_ok:
    sys.exit('FEHLER: nackte Zeilenumbrueche in build.bat')
if bad:
    around = data[max(0, bad[0] - 50):bad[-1] + 30].decode('utf-8', 'replace')
    sys.exit('FEHLER: Nicht-ASCII in build.bat bei Byte %d -- cmd.exe laeuft in '
             'Codepage 850 und zeigt Zeichensalat.\n  ...%s...' % (bad[0], around))

open(target, 'wb').write(data)
print("build.bat geschrieben: %d Zeilen, nur ASCII, CRLF" % data.count(b'\r\n'))
