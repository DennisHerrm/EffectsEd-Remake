#!/usr/bin/env python3
"""Erhoeht die Revisionsnummer in include/efx/version.h.

Warum eine eigene Zahl neben der Fassung: bei mehreren Paketen am selben Tag
sagt "1.0.7" allein nicht, welches das neuere ist. Die Revision zaehlt bei
JEDEM Paket hoch, auch wenn sich sonst nichts aendert.

    python3 tools/bump_revision.py           erhoeht um eins
    python3 tools/bump_revision.py --show    zeigt nur den Stand

Ausgegeben wird der vollstaendige Name, so wie er in den Dateinamen gehoert:

    1.0.8-rev43
"""
import re
import sys
from pathlib import Path

HEADER = Path(__file__).resolve().parent.parent / "include" / "efx" / "version.h"


def main() -> int:
    text = HEADER.read_text(encoding="utf-8")

    version = re.search(r'kVersion\s*=\s*"([^"]+)"', text)
    revision = re.search(r"kRevision\s*=\s*(\d+)", text)
    if not version or not revision:
        print("version.h sieht anders aus als erwartet")
        return 1

    current = int(revision.group(1))
    if "--show" in sys.argv:
        print(f"{version.group(1)}-rev{current}")
        return 0

    text = text.replace(f"kRevision = {current};", f"kRevision = {current + 1};", 1)
    HEADER.write_text(text, encoding="utf-8")
    print(f"{version.group(1)}-rev{current + 1}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
