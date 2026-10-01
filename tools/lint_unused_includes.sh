#!/usr/bin/env bash
# Findet Einbindungen, die man weglassen kann, ohne dass es bricht.
#
# Nicht durch Raten, sondern durch Ausprobieren: jede Einbindung einmal
# herausnehmen, uebersetzen, und wenn es durchgeht, war sie ueberfluessig.
# Langsam, aber ohne Fehlmeldung.
#
# Anlass: beim Aufteilen von app.cpp hatte ich jeder neuen Datei dieselbe
# Liste verpasst — 122 davon waren unnoetig. Das ist die Sorte Nachlaessigkeit,
# die ein Leser sofort sieht: wenn oben zwanzig Koepfe stehen, weiss er nicht,
# was die Datei wirklich braucht.
#
# Laeuft nicht bei jedem Bau (jede Datei wird dutzendfach uebersetzt).
set -u
cd "$(dirname "$0")/.."
IMGUI="${IMGUI_DIR:-/home/claude/imgui-check}"
[ -f "$IMGUI/imgui.h" ] || { echo "ImGui nicht gefunden — uebersprungen"; exit 0; }

echo "== Ueberfluessige Einbindungen =="
total=0
for file in gui/app*.cpp gui/icons.cpp; do
    [ -f "$file" ] || continue
    backup="$(mktemp)"; cp "$file" "$backup"
    count=0
    while read -r inc; do
        [ -n "$inc" ] || continue
        # Standard-Koepfe NICHT anfassen.
        #
        # GCC zieht viele davon ueber andere mit herein, MSVC nicht. Ein
        # Werkzeug, das "entfernt, was hier noch uebersetzt", entfernt genau
        # die, die beim Benutzer fehlen — und das ist beim ersten Lauf prompt
        # passiert: zehn Koepfe raus, lint_std sofort rot.
        #
        # Fuer Standard-Koepfe ist lint_std.py zustaendig. Hier geht es nur um
        # eigene Koepfe, und bei denen ist GCC nicht nachsichtiger als MSVC.
        case "$inc" in *'"'*) ;; *) continue ;; esac
        trial="$(mktemp)"
        grep -vxF "$inc" "$file" > "$trial"
        cp "$trial" "$file"
        if g++ -std=c++20 -fsyntax-only -Iinclude -Igui -I"$IMGUI" "$file" 2>/dev/null; then
            count=$((count + 1))
        else
            cp "$backup" "$file"
        fi
        rm -f "$trial"
        cp "$file" "$backup"
    done < <(grep -E '^#include' "$file")
    [ "$count" -gt 0 ] && echo "  $file: $count ueberfluessig"
    total=$((total + count))
    rm -f "$backup"
done
echo
echo "  $total ueberfluessige Einbindungen"
[ "$total" -eq 0 ]
