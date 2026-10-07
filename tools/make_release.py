#!/usr/bin/env python3
"""Baut die Pakete fuer ein GitHub-Release.

    output/EffectsEd-Remake-revNN.zip        build/Release/efxed.exe (MSVC, Windows 10/11)
    output/EffectsEd-Remake-win7-revNN.zip   build-mingw/efxed.exe   (MinGW, ab Windows 7)
    output/EffectsEd-Remake-revNN.exe        dieselben exe einzeln — der Anwender
    output/EffectsEd-Remake-win7-revNN.exe   will sie auch ohne Auspacken laden

Inhalt der zip jeweils: EffectsEd-Remake/efxed.exe und EffectsEd-Remake/LIESMICH.txt
(aus tools/release/). Die Revision kommt aus include/efx/version.h. Fehlt der
MinGW-Bau, entsteht nur das erste Paket. Die exe stoeren den Updater nicht:
er nimmt nur .zip (src/update.cpp zipAsset, ebenso rev78).

Warum ein eigenes Skript und nicht Compress-Archive: Windows PowerShell 5.1
schreibt Rueckstriche in die Eintragsnamen ("EffectsEd-Remake\\efxed.exe").
Der Updater kommt inzwischen damit zurecht (src/update.cpp), aber aeltere
Fassungen nicht — und ein Paket, das nur die neueste Fassung versteht, ist
genau das Paket, das die alten nie erreicht.

REIHENFOLGE beim Hochladen: das normale Paket ZUERST. Fassungen bis rev78
nehmen das erste .zip des Releases, egal wie es heisst.

Ablauf eines Releases (siehe UEBERGABE.md, "Releases"):
    python tools/bump_revision.py
    cmake --build build --config Release     und  build_mingw.bat
    efxtests + Selbsttest alles (d3d11 und gl3), fuer beide exe
    python tools/make_release.py
    gh release create v<Fassung>-rev<NN> output/EffectsEd-Remake-rev<NN>.zip \\
        -R DennisHerrm/EffectsEd-Remake-Releases --title "EffectsEd-Remake <Fassung>-rev<NN>" \\
        --notes-file <Notizen>.md
    gh release upload v<Fassung>-rev<NN> output/EffectsEd-Remake-win7-rev<NN>.zip \\
        output/EffectsEd-Remake-rev<NN>.exe output/EffectsEd-Remake-win7-rev<NN>.exe \\
        -R DennisHerrm/EffectsEd-Remake-Releases
    git tag -a v<Fassung>-rev<NN> -m "..." && git push origin v<Fassung>-rev<NN>
    (und in die Notizen: Quelltext dieser Fassung -> .../EffectsEd-Remake/tree/v<Fassung>-rev<NN>)
"""
import re
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def package(exe: Path, out: Path, readme: Path, full: str) -> bool:
    # Die exe muss die Fassung tragen, die das Paket verspricht — sonst
    # meldet der Updater nach dem Neustart gleich wieder ein "neues" Update.
    if full.encode("utf-16-le") not in exe.read_bytes():
        print(f"{exe} traegt nicht {full} - neu bauen")
        return False
    out.unlink(missing_ok=True)
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(exe, "EffectsEd-Remake/efxed.exe")
        z.write(readme, "EffectsEd-Remake/LIESMICH.txt")
    with zipfile.ZipFile(out) as z:
        for info in z.infolist():
            if "\\" in info.filename:
                print(f"Rueckstrich im Eintrag {info.filename}")
                return False
        if z.testzip() is not None:
            print(f"{out} ist beschaedigt")
            return False
    print(f"{out} ({out.stat().st_size} Bytes)")
    return True


def main() -> int:
    header = (ROOT / "include" / "efx" / "version.h").read_text(encoding="utf-8")
    version = re.search(r'kVersion\s*=\s*"([^"]+)"', header).group(1)
    revision = re.search(r"kRevision\s*=\s*(\d+)", header).group(1)
    full = f"{version}-rev{revision}"
    readme = ROOT / "tools" / "release" / "LIESMICH.txt"
    out_dir = ROOT / "output"
    out_dir.mkdir(exist_ok=True)

    exe = ROOT / "build" / "Release" / "efxed.exe"
    if not exe.exists():
        print(f"{exe} fehlt - erst bauen")
        return 1
    if not package(exe, out_dir / f"EffectsEd-Remake-rev{revision}.zip", readme, full):
        return 1
    shutil.copy2(exe, out_dir / f"EffectsEd-Remake-rev{revision}.exe")

    win7 = ROOT / "build-mingw" / "efxed.exe"
    if win7.exists():
        if not package(win7, out_dir / f"EffectsEd-Remake-win7-rev{revision}.zip", readme, full):
            return 1
        shutil.copy2(win7, out_dir / f"EffectsEd-Remake-win7-rev{revision}.exe")
    else:
        print("build-mingw/efxed.exe fehlt - kein Windows-7-Paket")
    print(f"Tag v{full}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
