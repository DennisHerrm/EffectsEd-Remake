#!/usr/bin/env python3
"""Baut das Paket fuer ein GitHub-Release: output/EffectsEd-Remake-revNN.zip.

Inhalt: EffectsEd-Remake/efxed.exe und EffectsEd-Remake/LIESMICH.txt
(aus tools/release/). Die Revision kommt aus include/efx/version.h.

Warum ein eigenes Skript und nicht Compress-Archive: Windows PowerShell 5.1
schreibt Rueckstriche in die Eintragsnamen ("EffectsEd-Remake\\efxed.exe").
Der Updater kommt inzwischen damit zurecht (src/update.cpp), aber aeltere
Fassungen nicht — und ein Paket, das nur die neueste Fassung versteht, ist
genau das Paket, das die alten nie erreicht.

Ablauf eines Releases (siehe UEBERGABE.md, "Releases"):
    python tools/bump_revision.py
    cmake --build build --config Release
    build/Release/efxtests.exe  +  Selbsttest alles (d3d11 und gl3)
    python tools/make_release.py
    gh release create v<Fassung>-rev<NN> output/EffectsEd-Remake-rev<NN>.zip \\
        -R DennisHerrm/EffectsEd-Remake-Releases --title "EffectsEd-Remake <Fassung>-rev<NN>" \\
        --notes-file <Notizen>.md
"""
import re
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    header = (ROOT / "include" / "efx" / "version.h").read_text(encoding="utf-8")
    version = re.search(r'kVersion\s*=\s*"([^"]+)"', header).group(1)
    revision = re.search(r"kRevision\s*=\s*(\d+)", header).group(1)
    exe = ROOT / "build" / "Release" / "efxed.exe"
    readme = ROOT / "tools" / "release" / "LIESMICH.txt"
    if not exe.exists():
        print(f"{exe} fehlt - erst bauen")
        return 1
    # Die exe muss die Fassung tragen, die das Paket verspricht — sonst
    # meldet der Updater nach dem Neustart gleich wieder ein "neues" Update.
    full = f"{version}-rev{revision}".encode("utf-16-le")
    if full not in exe.read_bytes():
        print(f"{exe} traegt nicht {version}-rev{revision} - neu bauen")
        return 1

    out_dir = ROOT / "output"
    out_dir.mkdir(exist_ok=True)
    out = out_dir / f"EffectsEd-Remake-rev{revision}.zip"
    out.unlink(missing_ok=True)
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(exe, "EffectsEd-Remake/efxed.exe")
        z.write(readme, "EffectsEd-Remake/LIESMICH.txt")
    shutil.copy2(exe, out_dir / "efxed.exe")

    with zipfile.ZipFile(out) as z:
        for info in z.infolist():
            if "\\" not in info.filename:
                continue
            print(f"Rueckstrich im Eintrag {info.filename}")
            return 1
        if z.testzip() is not None:
            print("Paket ist beschaedigt")
            return 1
    print(f"{out} ({out.stat().st_size} Bytes), Tag v{version}-rev{revision}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
