#!/usr/bin/env python3
"""Baut src/help_document.cpp aus data/help/Using_EffectsEd.html.

Warum eingebettet und nicht als Datei daneben: die Windows-7-Fassung besteht
aus GENAU ZWEI Dateien, und das soll so bleiben. Eine Hilfe, die nur dann da
ist, wenn jemand einen Ordner mitkopiert hat, ist bei einem Werkzeug, das per
ZIP weitergereicht wird, keine Hilfe.

Die Datei ist rund 270 KB — in einer .exe von fuenf Megabyte faellt das nicht
ins Gewicht, und dafuer funktioniert sie immer.

Als Zeichenkettenfeld statt als ein langes Literal: MSVC begrenzt einzelne
Zeichenketten auf 65535 Bytes, und aneinandergehaengte Literale haben dieselbe
Grenze. Ein Feld aus unsigned char hat sie nicht.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "data" / "help" / "Using_EffectsEd.html"
TARGET = ROOT / "src" / "help_document.cpp"


def main() -> int:
    if not SOURCE.exists():
        print(f"{SOURCE} fehlt")
        return 1
    data = SOURCE.read_bytes()

    lines = []
    for start in range(0, len(data), 20):
        chunk = data[start:start + 20]
        lines.append("    " + "".join(f"{b}," for b in chunk))

    TARGET.write_text(
        "// ERZEUGT von tools/gen_help.py — nicht von Hand aendern.\n"
        "//\n"
        "// Ravens Originalhandbuch \"EffectsEd Users Guide\" von 2002, als HTML\n"
        "// mit eingebetteten Bildern. Quelle: data/help/Using_EffectsEd.html\n"
        "#include \"efx/help.h\"\n\n"
        "namespace efx::help {\n\n"
        "const unsigned char kUsersGuide[] = {\n"
        + "\n".join(lines) +
        "\n};\n\n"
        f"const unsigned long kUsersGuideSize = {len(data)}ul;\n\n"
        "}  // namespace efx::help\n",
        encoding="utf-8")
    print(f"{TARGET.name}: {len(data)} Byte eingebettet")
    return 0


if __name__ == "__main__":
    sys.exit(main())
