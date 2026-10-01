#!/usr/bin/env python3
"""Prueft, dass die Fassungsnummern nicht auseinanderlaufen.

Sie stehen an zwei Stellen und muessen es auch:

    include/efx/version.h   der Programmtext
    gui/efxed.rc            die Ressourcentabelle, die Windows liest

Der Ressourcenuebersetzer liest kein C++, also laesst sich die Zahl dort nicht
einbinden. Genau so entstehen die Fehler, die dieses Projekt schon mehrfach
hatte: etwas an einer Stelle geaendert, an der zweiten vergessen. Der Dialog
zeigte dann 1.0.8, die Dateieigenschaften 1.0.2 — und bei einem Fehlerbericht
weiss niemand mehr, welche Fassung wirklich lief.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    header = (ROOT / "include" / "efx" / "version.h").read_text(encoding="utf-8")
    resource = (ROOT / "gui" / "efxed.rc").read_text(encoding="utf-8")

    version = re.search(r'kVersion\s*=\s*"([^"]+)"', header)
    revision = re.search(r"kRevision\s*=\s*(\d+)", header)
    if not version or not revision:
        print("version.h sieht anders aus als erwartet")
        return 1

    full = f"{version.group(1)}-rev{revision.group(1)}"
    parts = version.group(1).split(".")
    expected = ",".join(parts + [revision.group(1)])

    problems = 0
    for field in ("FILEVERSION", "PRODUCTVERSION"):
        found = re.search(field + r"\s+([\d, ]+)", resource)
        got = found.group(1).replace(" ", "").rstrip(",") if found else "(fehlt)"
        if got != expected:
            print(f"{field} in efxed.rc ist {got}, erwartet {expected}")
            problems += 1

    for field in ("FileVersion", "ProductVersion"):
        for found in re.finditer(r'VALUE\s+"' + field + r'"\s*,\s*"([^"]+)"', resource):
            if found.group(1) != full:
                print(f'VALUE "{field}" in efxed.rc ist '
                      f"{found.group(1)}, erwartet {full}")
                problems += 1

    print(f"{problems} Abweichungen zwischen version.h und efxed.rc ({full})")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
