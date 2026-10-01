#!/usr/bin/env python3
"""Prueft, ob gui/*.cpp die Koepfe einbindet, aus denen es Namen benutzt.

Anlass: drei Baufehler hintereinander, alle aus derselben Ursache — gui/ laesst
sich nur auf einem Windows-Rechner mit ImGui uebersetzen, also nicht dort, wo
der Quelltext entsteht. Zweimal ein erfundener Membername, einmal ein
vergessenes #include.

Die Pruefung sammelt aus include/efx/*.h, welcher Name in welchem Kopf steht,
und meldet Namen, deren Kopf in der benutzenden Datei nicht eingebunden ist.

Bewusst nur direkte Einbindungen: dass ein Name ueber einen anderen Kopf
mitkommt, ist Zufall und kein Zustand, auf den man sich verlassen sollte.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Was ein Kopf anbietet: Konstanten, Funktionen, Typen, Enumwerte.
# Nur eindeutige Namen. Funktionsnamen sind bewusst NICHT dabei: `length`,
# `reset`, `parse` und `scan` kommen in jeder zweiten Zeile vor und ergaeben
# vierzig Meldungen, von denen fast keine stimmt.
#
# Ein Pruefer, der ueberwiegend falsch meldet, wird ignoriert — und dann faengt
# er auch die richtige Meldung nicht mehr. Lieber weniger pruefen und dem
# Ergebnis trauen koennen.
PATTERNS = [
    # Konstanten: kVersion, kMaxIntensity, kNoTexture ...
    re.compile(r'^\s*(?:inline\s+)?constexpr\s+[\w:<>,* &]+\s+(k[A-Z]\w*)\s*='),
    # Enumwerte einer enum class kommen immer qualifiziert vor, also sicher.
    re.compile(r'^\s*enum\s+class\s+([A-Z]\w*)'),
]

# Namen, die zu gewoehnlich sind, um daraus etwas zu schliessen.
TOO_COMMON = {"Index", "Settings", "Kind", "Range", "Color", "Severity",
              "Diagnostic", "Matrix", "Vec3", "Vertex", "Blend", "Fill",
              "Cull", "Library", "Shader", "Effect", "Primitive", "Step",
              "Tab", "Field", "Language", "Str",
              # Nachgetragen: "Format" ist auch ein Direct3D-Feldname
              # (desc.Format). Der Pruefer meldete renderer_d3d11.cpp, sobald
              # image.h eine enum class Format bekam.
              #
              # Das ist die Kalibrierung, vor der ich beim Bau dieses Pruefers
              # gewarnt hatte, und sie hoert nie auf: je groesser der
              # Quelltext, desto mehr gewoehnliche Woerter kollidieren. Wer
              # hier einen Namen eintraegt, verliert eine echte Meldung —
              # deshalb nur, wenn der Name nachweislich anderswo vorkommt.
              "Format", "Image", "Result", "System", "Random", "Spawn",
              "Curve", "Flags", "Live"}

def offered(path):
    names = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        for pattern in PATTERNS:
            match = pattern.match(line)
            if match:
                names.add(match.group(1))
    return names

def main():
    # Name -> Koepfe, die ihn anbieten. Nur eindeutige Namen pruefen: steht
    # etwas in zwei Koepfen, waere nicht zu sagen, welcher gemeint ist.
    owner = {}
    for header in sorted((ROOT / "include" / "efx").glob("*.h")):
        for name in offered(header):
            owner.setdefault(name, set()).add(header.name)
    unique = {n: next(iter(h)) for n, h in owner.items()
              if len(h) == 1 and n not in TOO_COMMON}

    problems = []
    for path in sorted((ROOT / "gui").glob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        included = set(re.findall(r'#include\s+"efx/([\w.]+)"', text))

        # Eine Ebene weitergereicht zaehlt mit: gui/app.h ist der Sammelkopf
        # dieses Programms und bindet die Kerne ein, die die Oberflaeche
        # braucht. Wer app.h einbindet, darf deren Inhalt benutzen.
        #
        # Mehr als eine Ebene bewusst nicht — dann waere praktisch alles
        # erreichbar und die Pruefung wertlos.
        for own in re.findall(r'#include\s+"([\w.]+)"', text):
            local = ROOT / "gui" / own
            if local.exists():
                included |= set(re.findall(r'#include\s+"efx/([\w.]+)"',
                                           local.read_text(encoding="utf-8")))

        body = "\n".join(line for line in text.splitlines()
                         if not line.strip().startswith("//"))
        for name, header in unique.items():
            if header in included:
                continue
            if re.search(r'\b' + re.escape(name) + r'\b', body):
                problems.append((path.name, name, header))

    for name, symbol, header in problems:
        print(f"\n{name}\n  benutzt {symbol}, aber efx/{header} ist nicht "
              f"eingebunden")
    print(f"\n{len(problems)} fehlende Einbindungen")
    return 1 if problems else 0

if __name__ == "__main__":
    sys.exit(main())
