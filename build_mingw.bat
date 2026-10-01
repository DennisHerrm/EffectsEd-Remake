@echo off
setlocal EnableDelayedExpansion
rem ===========================================================
rem  EffectsEd - Bauen fuer Windows 7 (MinGW-w64)
rem ===========================================================
rem
rem  Visual Studio 2026 kann nicht mehr auf Windows 7 zielen - das ist eine
rem  Entscheidung von Microsoft, kein Fehler im Programm. MinGW-w64 kann es.
rem
rem  Gebraucht wird:
rem    - MinGW-w64 (z.B. ueber MSYS2: pacman -S mingw-w64-x86_64-gcc)
rem    - CMake
rem    - Ninja oder mingw32-make
rem
rem  Die fertige efxed.exe laeuft dann auf Windows 7 SP1 und neuer.
rem  Direct3D 11 gibt es dort nur mit dem Platform Update (KB2670838);
rem  ohne das faellt das Programm auf OpenGL zurueck - siehe Ablaufprotokoll.

set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set BUILD=%ROOT%\build-mingw

echo ============================================================
echo  EffectsEd - Bauen fuer Windows 7 (MinGW-w64)
echo ============================================================

where g++ >nul 2>&1
if errorlevel 1 (
    echo [FEHLER] g++ nicht gefunden.
    echo          MinGW-w64 installieren und in den PATH aufnehmen.
    echo          Ueber MSYS2:  pacman -S mingw-w64-x86_64-gcc
    exit /b 1
)
where cmake >nul 2>&1
if errorlevel 1 (
    echo [FEHLER] cmake nicht gefunden.
    exit /b 1
)

set GENERATOR=MinGW Makefiles
where ninja >nul 2>&1
if not errorlevel 1 set GENERATOR=Ninja

echo.
echo  Uebersetzer: 
g++ --version | findstr /R "."
echo  Erzeuger   : %GENERATOR%
echo.

cmake -S "%ROOT%" -B "%BUILD%" -G "%GENERATOR%" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0601 -DWINVER=0x0601" ^
      -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"
if errorlevel 1 (
    echo [FEHLER] Das Einrichten ist fehlgeschlagen.
    exit /b 1
)

cmake --build "%BUILD%" --config Release
if errorlevel 1 (
    echo [FEHLER] Das Uebersetzen ist fehlgeschlagen.
    exit /b 1
)

if not exist "%ROOT%\out" mkdir "%ROOT%\out"
copy /y "%BUILD%\efxed.exe" "%ROOT%\out\" >nul 2>&1
copy /y "%BUILD%\efxtool.exe" "%ROOT%\out\" >nul 2>&1

echo.
echo  bereit: out\efxed.exe
echo.
echo  -static sorgt dafuer, dass keine MinGW-DLLs mitgeliefert werden
echo  muessen - eine einzelne .exe, wie bei der MSVC-Fassung.
echo.
endlocal
