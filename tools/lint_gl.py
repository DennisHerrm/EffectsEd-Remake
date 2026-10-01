#!/usr/bin/env python3
"""Prueft, ob nachgeladene OpenGL-Funktionen mit gl:: gerufen werden.

Anlass: `glBindFramebuffer(...)` statt `gl::BindFramebuffer(...)` — ein
Baufehler, der beim Benutzer aufschlug.

Windows' opengl32 bietet nur OpenGL 1.1. Alles Neuere wird in
gui/renderer_gl3.cpp zur Laufzeit ueber wglGetProcAddress geholt und liegt im
Namensraum gl. Ohne Praefix sucht der Uebersetzer eine Systemfunktion, die es
dort nicht gibt — und weil renderer_gl3.cpp Windows- und GL-Koepfe braucht,
faengt der Uebersetzerlauf fuer app.cpp das nicht.

Die Liste der nachgeladenen Funktionen steht als Makro in der Datei selbst.
Die zu lesen ist genauer, als sie zu pflegen: kommt eine Funktion dazu, wird
sie automatisch mitgeprueft.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "gui" / "renderer_gl3.cpp"

def main():
    if not SOURCE.exists():
        print("renderer_gl3.cpp nicht gefunden")
        return 0
    text = SOURCE.read_text(encoding="utf-8")

    # X(rueckgabe, Name, (Argumente)) in der Makroliste.
    loaded = set(re.findall(r'^\s*X\(\s*\w[\w:]*\s*,\s*(\w+)\s*,', text, re.M))
    if not loaded:
        print("keine nachgeladenen Funktionen gefunden — Muster geprueft?")
        return 0

    problems = []
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        if stripped.startswith("X(") or "#define" in line:
            continue
        for name in loaded:
            # gl<Name>( ohne gl:: davor
            if re.search(r'(?<!:)\bgl' + re.escape(name) + r'\s*\(', line):
                problems.append((number, name, stripped[:66]))

    for number, name, line in problems:
        print(f"\nrenderer_gl3.cpp\n  Zeile {number}: gl{name}() ohne gl:: — "
              f"Windows kennt nur OpenGL 1.1\n      {line}")
    print(f"\n{len(problems)} OpenGL-Aufrufe ohne gl::")
    return 1 if problems else 0

if __name__ == "__main__":
    sys.exit(main())
