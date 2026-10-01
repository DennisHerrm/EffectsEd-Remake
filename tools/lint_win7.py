#!/usr/bin/env python3
"""Prueft, ob gui/ Windows-Aufrufe benutzt, die es auf Windows 7 nicht gibt.

Anlass: eine Fassung fuer Windows 7. Der Fallstrick ist, dass solche Aufrufe
nicht beim Uebersetzen scheitern, sondern beim LADEN — Windows loest die
Importtabelle auf, findet das Symbol nicht, und das Programm startet gar
nicht. Kein Fenster, keine Meldung, nichts im Protokoll.

Genau die Sorte Fehler, die man auf einem fremden Rechner nicht diagnostiziert.

Der richtige Umgang damit ist GetProcAddress: fehlt die Funktion, faellt man
auf eine aeltere zurueck. Dieser Pruefer meldet direkte Aufrufe — Zeilen mit
GetProcAddress zaehlen nicht.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Funktion -> ab welcher Windows-Fassung es sie gibt.
NEWER_THAN_WIN7 = {
    "SetProcessDpiAwarenessContext": "Windows 10 1703",
    "SetThreadDpiAwarenessContext": "Windows 10 1607",
    "GetDpiForWindow": "Windows 10 1607",
    "GetDpiForSystem": "Windows 10 1607",
    "AdjustWindowRectExForDpi": "Windows 10 1607",
    "GetSystemMetricsForDpi": "Windows 10 1607",
    "EnableNonClientDpiScaling": "Windows 10",
    "SetProcessDpiAwareness": "Windows 8.1",
    "GetDpiForMonitor": "Windows 8.1",
    "CreateDXGIFactory2": "Windows 8.1",
    "D3D11CreateDevice1": "Windows 8",
    "GetSystemTimePreciseAsFileTime": "Windows 8",
    "CreateFile2": "Windows 8",
    "PathCchCombine": "Windows 8",
    "RoInitialize": "Windows 8",
    "GetPackageFullName": "Windows 8",
    "CompareStringEx": "Windows Vista",  # nur zur Erinnerung, Win7 hat es
}
# Was Windows 7 hat, obwohl es modern aussieht.
ON_WIN7 = {"CompareStringEx", "GetUserDefaultLocaleName", "InitOnceExecuteOnce"}

def main():
    problems = []
    for path in sorted((ROOT / "gui").glob("*.cpp")):
        lines = path.read_text(encoding="utf-8").splitlines()
        for number, line in enumerate(lines, 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            # Ueber GetProcAddress geholt ist in Ordnung — genau so soll es sein.
            if "GetProcAddress" in line:
                continue
            for name, since in NEWER_THAN_WIN7.items():
                if name in ON_WIN7:
                    continue
                # Nur echte Aufrufe, keine Zeichenketten
                if re.search(r'(?<![\w"])' + re.escape(name) + r'\s*\(', line):
                    problems.append((path.name, number, name, since, stripped[:60]))

    for file, number, name, since, line in problems:
        print(f"\n{file}\n  Zeile {number}: {name}() gibt es erst ab {since}"
              f"\n      {line}"
              f"\n      -> ueber GetProcAddress holen, sonst startet das Programm"
              f" auf aelteren Fassungen gar nicht")
    print(f"\n{len(problems)} Aufrufe neuer als Windows 7")
    return 1 if problems else 0

if __name__ == "__main__":
    sys.exit(main())
