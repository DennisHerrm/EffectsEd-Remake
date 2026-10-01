#!/usr/bin/env bash
# Baut die Windows-7-Fassung von einem Linux-Rechner aus.
#
# Warum ueberhaupt: Visual Studio 2026 kann nicht mehr auf Windows 7 zielen —
# das ist eine Entscheidung von Microsoft. MinGW-w64 kann es, und als
# Kreuzuebersetzer laeuft es auch auf Linux.
#
# Der Vorteil gegenueber build_mingw.bat: **niemand muss etwas installieren.**
# Wer die .exe bekommt, startet sie einfach.
#
#   apt-get install g++-mingw-w64-x86-64
#   git clone --depth 1 --branch v1.91.5 https://github.com/ocornut/imgui.git
#
# Die Fassung enthaelt Direct3D UND OpenGL und laeuft auf Windows 7, 8, 10
# und 11 — eine Datei fuer alle.
#
# Der Trick: nichts, was neuer als Windows 7 ist, steht in der Importtabelle.
# Direct3D, der Shaderuebersetzer und die DPI-Funktionen werden zur Laufzeit
# gesucht. Sind sie da, werden sie benutzt; sonst der aeltere Weg.
#
# ImGuis DX11-Ruecken ruft `D3DCompile` direkt auf und wuerde
# `d3dcompiler_47.dll` fest einbinden. gui/imgui_impl_dx11_wrapper.cpp biegt
# den Aufruf auf eine Fassung um, die die DLL zur Laufzeit sucht — ImGui nennt
# genau diesen Weg selbst im Quelltext.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMGUI="${IMGUI_DIR:-/home/claude/imgui-check}"
OUT="${1:-$ROOT/out-win7}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

CXX=x86_64-w64-mingw32-g++
command -v "$CXX" >/dev/null || { echo "MinGW-Kreuzuebersetzer fehlt"; exit 1; }
[ -f "$IMGUI/imgui.h" ] || { echo "ImGui fehlt unter $IMGUI"; exit 1; }

FLAGS="-std=c++20 -O2 -DNDEBUG -D_WIN32_WINNT=0x0601 -DWINVER=0x0601"
FLAGS="$FLAGS -DNTDDI_VERSION=0x06010000 -municode"
INC="-I$ROOT/include -isystem $ROOT/third_party -I$ROOT/gui"
INC="$INC -I$IMGUI -I$IMGUI/backends"

echo "== ImGui =="
for f in imgui imgui_draw imgui_tables imgui_widgets \
         backends/imgui_impl_win32 backends/imgui_impl_opengl3; do
    $CXX $FLAGS $INC -c "$IMGUI/$f.cpp" -o "$WORK/$(basename "$f").o"
done
# Der DX11-Ruecken ueber unsere Huelle — nicht die Originaldatei, sonst gaebe
# es die Funktionen doppelt.
$CXX $FLAGS $INC -c "$ROOT/gui/imgui_impl_dx11_wrapper.cpp" \
    -o "$WORK/imgui_impl_dx11.o"

echo "== Kern =="
for f in "$ROOT"/src/*.cpp; do
    $CXX $FLAGS $INC -c "$f" -o "$WORK/core_$(basename "$f" .cpp).o"
done

echo "== Oberflaeche =="
for f in "$ROOT"/gui/*.cpp; do
    # Die Huelle ist schon oben uebersetzt.
    case "$(basename "$f")" in imgui_impl_dx11_wrapper.cpp) continue;; esac
    $CXX $FLAGS $INC -c "$f" -o "$WORK/gui_$(basename "$f" .cpp).o"
done

# Das Symbol. windres uebersetzt die .rc in eine gewoehnliche Objektdatei,
# die einfach mitgebunden wird — MinGWs Gegenstueck zu rc.exe.
#
# Ohne das haette die Kreuzfassung als einzige kein Symbol, und das faellt
# erst auf dem Zielrechner auf.
echo "== Symbol =="
RC=x86_64-w64-mingw32-windres
if command -v "$RC" >/dev/null; then
    "$RC" -I"$ROOT/gui" "$ROOT/gui/efxed.rc" -O coff -o "$WORK/efxed_res.o"
else
    echo "  windres fehlt - die .exe bekommt kein Symbol"
fi

echo "== Binden =="
mkdir -p "$OUT"
# -static: keine MinGW-DLLs mitzuliefern, eine einzelne Datei.
$CXX -O2 -municode -mwindows -o "$OUT/efxed.exe" "$WORK"/*.o \
    -static -static-libgcc -static-libstdc++ \
    -ldxgi -lopengl32 -lcomdlg32 -lwinmm -lgdi32 -luser32 -ladvapi32 \
    -lole32 -lshell32 -luuid -limm32 -ldwmapi

$CXX $FLAGS $INC -c "$ROOT/tools/efxtool.cpp" -o "$WORK/efxtool.o"
$CXX -O2 -o "$OUT/efxtool.exe" "$WORK/efxtool.o" "$WORK"/core_*.o \
    -static -static-libgcc -static-libstdc++

echo
ls -la "$OUT"/*.exe
echo
echo "Fertig. Die Dateien laufen ohne Zusatzpakete."
