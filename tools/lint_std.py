#!/usr/bin/env python3
"""Prueft, ob benutzte Standardfunktionen ihren Kopf eingebunden haben.

Anlass: `std::min` in icons.cpp ohne `<algorithm>`. Auf dem Entwicklungs-
rechner uebersetzt das, weil GCC den Kopf ueber andere mitzieht; MSVC tut das
nicht, und der Fehler schlug beim Benutzer auf.

Das ist die unangenehmste Sorte Unterschied: der Uebersetzerlauf hier meldet
nichts, weil er GCC benutzt. Nur eine eigene Regel faengt es.

Geprueft wird nur, was haeufig gebraucht und leicht vergessen wird — eine
vollstaendige Abdeckung der Standardbibliothek waere Unsinn und wuerde vor
lauter Fehlmeldungen ignoriert.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Symbol -> Kopf, in dem es steht.
NEEDS = {
    "min": "algorithm", "max": "algorithm", "clamp": "algorithm",
    "sort": "algorithm", "stable_sort": "algorithm", "find": "algorithm",
    "unique": "algorithm", "reverse": "algorithm", "binary_search": "algorithm",
    "lower_bound": "algorithm", "count": "algorithm", "fill": "algorithm",
    "memcpy": "cstring", "memset": "cstring", "memcmp": "cstring",
    "strlen": "cstring", "strcmp": "cstring",
    "sqrt": "cmath", "fabs": "cmath", "sin": "cmath", "cos": "cmath",
    "pow": "cmath", "floor": "cmath", "ceil": "cmath", "lround": "cmath",
    "isfinite": "cmath", "atan2": "cmath", "asin": "cmath", "acos": "cmath",
    "fmod": "cmath", "isnan": "cmath",
    "printf": "cstdio", "snprintf": "cstdio", "fprintf": "cstdio",
    "string": "string", "to_string": "string", "stof": "string",
    "vector": "vector", "map": "map", "set": "set",
    "unique_ptr": "memory", "make_unique": "memory", "shared_ptr": "memory",
    "function": "functional", "mutex": "mutex", "thread": "thread",
    "uint32_t": "cstdint", "int16_t": "cstdint", "uintptr_t": "cstdint",
}

def main():
    problems = []
    folders = [ROOT / "src", ROOT / "gui", ROOT / "include" / "efx"]
    for folder in folders:
        for path in sorted(list(folder.glob("*.cpp")) + list(folder.glob("*.h"))):
            text = path.read_text(encoding="utf-8")
            included = set(re.findall(r'#include\s+<([\w./]+)>', text))

            # Eigene Koepfe zaehlen mit, eine Ebene tief.
            #
            # Eine .cpp, die "efx/effect.h" einbindet, darf std::vector
            # benutzen, wenn dieser Kopf <vector> mitbringt. Das ist keine
            # Nachlaessigkeit, sondern der Sinn eines Kopfes: er stellt seine
            # eigene Schnittstelle vollstaendig bereit.
            #
            # Der erste Entwurf sah nur den gleichnamigen Kopf — src/write.cpp
            # gehoert aber zu include/efx/io.h. Ergebnis: sechzig Meldungen,
            # von denen fast keine stimmte.
            for own in re.findall(r'#include\s+"([\w./]+)"', text):
                for base in (ROOT / "include", ROOT / "gui", folder):
                    candidate = base / own
                    if candidate.exists():
                        included |= set(re.findall(
                            r'#include\s+<([\w./]+)>',
                            candidate.read_text(encoding="utf-8")))
                        break

            body = "\n".join(l for l in text.splitlines()
                             if not l.strip().startswith("//"))
            for symbol, header in NEEDS.items():
                if header in included:
                    continue
                if re.search(r'\bstd::' + re.escape(symbol) + r'\b', body):
                    problems.append((path.name, symbol, header))

    for name, symbol, header in sorted(set(problems)):
        print(f"\n{name}\n  benutzt std::{symbol}, aber <{header}> fehlt")
    print(f"\n{len(set(problems))} fehlende Standard-Koepfe")
    return 1 if problems else 0

if __name__ == "__main__":
    sys.exit(main())
