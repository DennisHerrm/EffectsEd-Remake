#!/usr/bin/env python3
"""Baut gui/efxed.ico aus gui/efxed_256.png.

Warum von Hand und nicht mit Pillows ICO-Schreiber: der legt ALLE Groessen als
PNG ab. Das kann Windows seit Vista, verlaesslich ist es aber nur fuer
256x256. Kleinere Groessen erwarten der Explorer aelterer Fassungen und
manche Werkzeuge als Bitmap (DIB) — und dieses Programm zielt ausdruecklich
auf Windows 7.

Also: 16 bis 128 als 32-Bit-Bitmap mit Alphakanal, 256 als PNG (dort ist es
Vorschrift, sonst waere die Datei ein Megabyte gross).

Die Bitmaps brauchen zwei Eigenheiten, die man ohne Fundstelle nicht ahnt:
  - die Bildhoehe im Kopf ist DOPPELT so hoch wie das Bild (Farbe + Maske)
  - die Zeilen stehen von unten nach oben
Wer das uebersieht, bekommt ein Symbol, das auf dem Kopf steht.
"""
import struct
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SIZES = [16, 24, 32, 48, 64, 128, 256]


def dib(img):
    """Ein Bild als 32-Bit-DIB mit Maske, wie eine .ico es erwartet."""
    w, h = img.size
    header = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    pixels = bytearray()
    for y in range(h - 1, -1, -1):               # von unten nach oben
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            pixels += bytes((b, g, r, a))        # BGRA
    # Die Maske. Bei 32 Bit entscheidet der Alphakanal, sie muss aber da sein.
    maskRow = ((w + 31) // 32) * 4
    mask = bytes(maskRow * h)
    return header + bytes(pixels) + mask


def main():
    source = ROOT / "gui" / "efxed_256.png"
    if not source.exists():
        print(f"{source} fehlt")
        return 1
    base = Image.open(source).convert("RGBA")

    entries = []
    for size in SIZES:
        img = base.resize((size, size), Image.LANCZOS)
        if size == 256:
            from io import BytesIO
            buffer = BytesIO()
            img.save(buffer, format="PNG", optimize=True)
            entries.append((size, buffer.getvalue()))
        else:
            entries.append((size, dib(img)))

    out = bytearray(struct.pack('<HHH', 0, 1, len(entries)))
    offset = 6 + 16 * len(entries)
    for size, data in entries:
        out += struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32,
                           len(data), offset)
        offset += len(data)
    for _, data in entries:
        out += data

    target = ROOT / "gui" / "efxed.ico"
    target.write_bytes(bytes(out))
    print(f"{target.name}: {len(entries)} Groessen, {len(out)} Byte "
          f"({len(entries) - 1} als Bitmap, 256 als PNG)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
