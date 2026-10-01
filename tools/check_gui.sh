#!/usr/bin/env bash
# Uebersetzt gui/app.cpp gegen die echten ImGui-Koepfe.
#
# Anlass: vier Baufehler in Folge, alle in app.cpp, alle beim Benutzer
# aufgeschlagen. Die vier Pruefer in tools/lint_*.py fangen Tippfehler und
# vergessene Einbindungen, aber keine Typfehler, keine falschen
# Argumentzahlen, keine falschen ImGui-Signaturen.
#
# app.cpp bindet keine Windows-Koepfe ein — nur ImGui und unsere eigenen.
# Damit laesst sich genau die Datei, in der alle vier Fehler steckten,
# vollstaendig uebersetzen, ohne Windows.
#
# Erzeugt kein lauffaehiges Programm: -fsyntax-only prueft nur. Das reicht,
# denn gebaut wird ohnehin auf dem Zielrechner.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMGUI="${IMGUI_DIR:-/home/claude/imgui-check}"

if [ ! -f "$IMGUI/imgui.h" ]; then
    echo "ImGui nicht gefunden unter $IMGUI"
    echo "  git clone --depth 1 --branch v1.91.5 https://github.com/ocornut/imgui.git"
    echo "Uebersprungen."
    exit 0
fi

# Alle gui-Dateien, die ohne Windows- und GL-Koepfe auskommen.
#
# icons.cpp kam dazu, als die Werkzeugleiste gezeichnete Symbole bekam — sie
# braucht nur ImGui.
#
# main_win32.cpp und die Renderer brauchen Windows-Koepfe und stehen deshalb
# nicht in dieser Liste. Sie werden weiter unten mit dem MinGW-Uebersetzer
# geprueft, wenn es ihn gibt — davor war das nicht moeglich, und genau dort
# ist ein Fehler durchgerutscht: ein Setter landete im privaten Teil von
# App, was hier niemand sah und erst der Kreuzbau meldete.
FILES="gui/app.cpp gui/app_widgets.cpp gui/app_panels.cpp \
       gui/app_playback.cpp gui/app_resources.cpp gui/app_dialogs.cpp \
       gui/app_properties.cpp gui/app_browser.cpp gui/icons.cpp"

status=0
for file in $FILES; do
    [ -f "$ROOT/$file" ] || continue
    echo "== $file gegen ImGui uebersetzen =="
    g++ -std=c++20 -fsyntax-only \
        -Wall -Wextra -Wshadow \
        -I"$ROOT/include" -I"$ROOT/gui" -I"$IMGUI" \
        "$ROOT/$file" 2>&1 | head -40
    if [ "${PIPESTATUS[0]}" -eq 0 ]; then
        echo "  $file uebersetzt sauber"
    else
        echo "  FEHLER in $file"
        status=1
    fi
done

# --- Die Windows-Seite, wenn ein Kreuzuebersetzer da ist -------------------
#
# Nur eine Syntaxpruefung, kein Bau: die geht in Sekunden und faengt genau
# das, was der reinen ImGui-Pruefung entgeht — Sichtbarkeiten, fehlende
# Einbindungen, falsche Signaturen in den Windows-Dateien.
CROSS=x86_64-w64-mingw32-g++
if command -v "$CROSS" >/dev/null && [ -f "$IMGUI/imgui.h" ]; then
    for file in gui/main_win32.cpp gui/renderer_gl3.cpp gui/renderer_d3d11.cpp \
                gui/registry_win32.cpp gui/audio_win32.cpp; do
        [ -f "$ROOT/$file" ] || continue
        echo "== $file gegen Windows uebersetzen =="
        "$CROSS" -std=c++20 -fsyntax-only \
            -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -DNTDDI_VERSION=0x06010000 \
            -Wall -Wextra -Wshadow \
            -I"$ROOT/include" -isystem "$ROOT/third_party" -I"$ROOT/gui" \
            -I"$IMGUI" -I"$IMGUI/backends" \
            "$ROOT/$file" || status=1
    done
else
    echo "(kein MinGW-Uebersetzer - die Windows-Dateien bleiben ungeprueft)"
fi

exit $status
