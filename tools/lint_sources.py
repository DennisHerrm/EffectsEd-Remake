#!/usr/bin/env python3
"""Prueft, ob jede Quelldatei auch im Bauskript steht.

Anlass: ein Linkerfehler, der erst beim Anwender auftrat.

    app_browser.obj : error LNK2019: Verweis auf nicht aufgeloestes externes
    Symbol "efx::tiles::assign(...)"

`src/tiles.cpp` gab es, sie uebersetzte, sie war geprueft — sie stand nur
nicht in der Quellenliste von CMakeLists.txt. Microsofts Doku nennt genau das
als ersten Grund fuer LNK2019: die Datei mit der Definition wird nicht
mituebersetzt.

Warum kein vorhandener Pruefer das sah: mein Prueflauf baut mit

    g++ ... src/*.cpp tests/tests.cpp

also mit einem Muster, das jede neue Datei automatisch mitnimmt. Das Bauskript
des Anwenders zaehlt sie einzeln auf. Genau zwischen diesen beiden Wegen faellt
eine neue Datei hindurch, und zwar lautlos: hier gruen, dort ein Linkerfehler.

Die Aufzaehlung in CMakeLists ist Absicht und bleibt — ein Muster (GLOB) merkt
nicht, wenn eine Datei dazukommt, und CMake laeuft dann nicht neu. Statt die
Aufzaehlung abzuschaffen, wird sie abgeglichen.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Dateien, die absichtlich NICHT im Kern stehen. Jede braucht eine Begruendung,
# sonst ist die Ausnahmeliste bald der Ort, an dem Fehler verschwinden.
EXPECTED_ELSEWHERE = set()   # keine — der ganze Kern gehoert in efxcore


def main():
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

    listed = set(re.findall(r'\bsrc/([A-Za-z0-9_]+\.cpp)\b', cmake))
    on_disk = {p.name for p in (ROOT / "src").glob("*.cpp")}

    missing = sorted(on_disk - listed - EXPECTED_ELSEWHERE)
    stale = sorted(listed - on_disk)

    for name in missing:
        print(f"src/{name} liegt im Baum, steht aber nicht in CMakeLists.txt")
    for name in stale:
        print(f"src/{name} steht in CMakeLists.txt, gibt es aber nicht")

    # Dasselbe fuer gui/: dort trifft es sonst erst beim Binden unter Windows.
    gui_listed = set(re.findall(r'\bgui/([A-Za-z0-9_]+\.cpp)\b', cmake))
    gui_on_disk = {p.name for p in (ROOT / "gui").glob("*.cpp")}
    gui_missing = sorted(gui_on_disk - gui_listed)
    gui_stale = sorted(gui_listed - gui_on_disk)
    for name in gui_missing:
        print(f"gui/{name} liegt im Baum, steht aber nicht in CMakeLists.txt")
    for name in gui_stale:
        print(f"gui/{name} steht in CMakeLists.txt, gibt es aber nicht")

    total = len(missing) + len(stale) + len(gui_missing) + len(gui_stale)
    print(f"{total} Quelldateien weichen vom Bauskript ab")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
