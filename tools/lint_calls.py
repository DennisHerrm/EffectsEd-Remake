#!/usr/bin/env python3
"""Prueft qualifizierte Aufrufe wie `diag::warn(...)` gegen die Koepfe.

Anlass: der vierte Baufehler in dieser Reihe, und die dritte verschiedene
Fehlerklasse. Ich hatte `diag::warning(...)` geschrieben — die Funktion heisst
`warn`. Weder der Memberpruefer noch der Einbindungspruefer sieht so etwas:
der eine kennt nur Namen mit Unterstrich am Ende, der andere nur Konstanten und
enum-Klassen.

Gemeinsame Ursache aller vier: gui/ laesst sich nur auf einem Windows-Rechner
mit ImGui uebersetzen. Was hier nicht uebersetzt wird, muss anders geprueft
werden — oder es geht als Baufehler an den Benutzer.

Grob, aber genau fuer diesen Fehler gebaut: fuer jeden Namensraum aus
include/efx/ wird gesammelt, welche freien Funktionen er anbietet. Dann werden
in gui/*.cpp alle Aufrufe der Form `raum::name(` dagegen gehalten.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Eine Funktionsdeklaration oder -definition auf Namensraumebene:
#   void write(Level level, const std::string& text);
#   inline void info(const std::string& text) { ... }
#   const std::vector<std::string>& lines();
DECLARATION = re.compile(
    r'^\s*(?:inline\s+|static\s+|constexpr\s+|virtual\s+)*'
    r'[A-Za-z_][\w:<>,*& ]*[\s&*]'
    r'([a-z][A-Za-z0-9]*)\s*\(')

# Namensraeume, die aus mehreren Koepfen zusammenkommen duerfen.
NAMESPACE = re.compile(r'^\s*namespace\s+efx::(\w+)\s*\{')
NESTED = re.compile(r'^\s*namespace\s+(\w+)\s*\{')

def collect():
    """namensraum -> Menge angebotener Funktionsnamen."""
    offered = {}
    for header in sorted((ROOT / "include" / "efx").glob("*.h")):
        current = None
        depth = 0
        for line in header.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            match = NAMESPACE.match(line)
            if match:
                current = match.group(1)
                depth = 0
                continue
            if current is None:
                continue
            # Innerhalb von Klassen und Strukturen nicht sammeln: deren
            # Methoden sind keine freien Funktionen.
            #
            # Gezaehlt wird an geschweiften Klammern, nicht an "};". Der erste
            # Entwurf suchte nach "};" in derselben Zeile — bei einer Klasse,
            # die anders schliesst, blieb der Zaehler fuer den Rest der Datei
            # auf 1 stehen, und alles danach wurde uebersehen.
            #
            # Genau so verlor er `Pool& pool()` in jobs.h und meldete einen
            # gueltigen Aufruf als Fehler. Ein Pruefer, der Richtiges
            # anmeckert, wird ignoriert.
            if depth == 0 and re.match(r'^\s*(struct|class|enum)\b', line):
                if ";" not in line.split("//")[0] or "{" in line:
                    depth = 1
                    depth += line.count("{") - line.count("}") - 1
                continue
            if depth > 0:
                depth += line.count("{") - line.count("}")
                if depth < 0:
                    depth = 0
                continue
            found = DECLARATION.match(line)
            if found:
                offered.setdefault(current, set()).add(found.group(1))
    return offered

def main():
    offered = collect()

    problems = []
    for path in sorted((ROOT / "gui").glob("*.cpp")):
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            for space, name in re.findall(r'\b(\w+)::([a-z][A-Za-z0-9]*)\s*\(', line):
                if space not in offered:
                    continue  # kein efx-Namensraum, den wir kennen
                if name in offered[space]:
                    continue
                problems.append((path.name, number, space, name, stripped[:70]))

    for file, number, space, name, line in problems:
        known = sorted(offered[space])
        # Den aehnlichsten Namen vorschlagen — bei einem Tippfehler ist das
        # fast immer der gemeinte.
        close = [k for k in known if k.startswith(name[:3]) or name.startswith(k[:3])]
        hint = ("  gibt es: " + ", ".join(close)) if close else ""
        print(f"\n{file}\n  Zeile {number}: {space}::{name}() gibt es nicht{hint}"
              f"\n      {line}")
    print(f"\n{len(problems)} unbekannte Aufrufe")
    return 1 if problems else 0

if __name__ == "__main__":
    sys.exit(main())
