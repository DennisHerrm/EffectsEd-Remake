#!/usr/bin/env python3
"""Prueft eine fertige .exe darauf, ob Windows 7 sie laden kann.

    python3 tools/check_win7_exe.py out/efxed.exe

Der Streitpunkt bei Windows 7 laesst sich nicht durch Lesen entscheiden. Die
SDK-Koepfe koennen auf Windows 7 zielen — das stimmt. Ob die fertige Datei
dort laedt, haengt aber daran, was in ihrer IMPORTTABELLE steht: jede Funktion
darin muss beim Programmstart aufloesbar sein, sonst startet gar nichts.

Genau das prueft dieses Werkzeug. Es liest die Datei, nicht den Quelltext.

Verzoegert gebundene Eintraege (/DELAYLOAD) zaehlen nicht: die werden erst
beim ersten Aufruf geholt, und fehlen sie, kann das Programm ausweichen.
"""
import struct
import sys
from pathlib import Path

# DLLs, die es auf Windows 7 gibt.
WIN7_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
    "ole32.dll", "oleaut32.dll", "comdlg32.dll", "comctl32.dll", "winmm.dll",
    "opengl32.dll", "imm32.dll", "shlwapi.dll", "version.dll", "ws2_32.dll",
    "dwmapi.dll", "dxgi.dll", "msvcrt.dll", "setupapi.dll", "uxtheme.dll",
    "psapi.dll", "crypt32.dll", "userenv.dll", "gdiplus.dll", "rpcrt4.dll",
    # Die Universal CRT gibt es auf Windows 7 nur mit KB2999226. Deshalb
    # statisch binden — dann taucht sie hier gar nicht auf.
}
# Funktionen, die es auf Windows 7 NICHT gibt. Nicht vollstaendig, aber die,
# ueber die man in der Praxis stolpert.
NEWER_FUNCS = {
    "SetThreadDescription", "GetThreadDescription",
    "GetSystemTimePreciseAsFileTime", "CreateFile2", "SetProcessMitigationPolicy",
    "SetDefaultDllDirectories", "AddDllDirectory", "GetPackageFullName",
    "SetProcessDpiAwarenessContext", "GetDpiForWindow", "GetDpiForSystem",
    "AdjustWindowRectExForDpi", "GetSystemMetricsForDpi",
    "EnableNonClientDpiScaling", "SetThreadDpiAwarenessContext",
    "SetProcessDpiAwareness", "GetDpiForMonitor",
    "CompareStringOrdinal_", "PathCchCombine", "PathCchCanonicalizeEx",
    "RoInitialize", "WaitOnAddress", "WakeByAddressSingle", "WakeByAddressAll",
    "DiscardVirtualMemory", "OfferVirtualMemory", "QuirkIsEnabled",
    "CreateDXGIFactory2", "D3D11CreateDevice1",
}

def readPortableExecutable(path):
    """Importtabelle und Mindestfassung aus einer .exe lesen.

    Selbst geschrieben statt `pefile` eingebunden — das Programm kommt ohne
    Abhaengigkeiten aus, und dieses Werkzeug soll sich niemand erst
    installieren muessen. Das PE-Format ist an dieser Stelle einfach genug.
    """
    data = path.read_bytes()
    if len(data) < 0x40 or data[:2] != b"MZ":
        return None, None, None
    peAt = struct.unpack_from("<I", data, 0x3C)[0]
    if peAt + 24 > len(data) or data[peAt:peAt + 4] != b"PE\0\0":
        return None, None, None

    sectionCount = struct.unpack_from("<H", data, peAt + 6)[0]
    optionalSize = struct.unpack_from("<H", data, peAt + 20)[0]
    optionalAt = peAt + 24
    magic = struct.unpack_from("<H", data, optionalAt)[0]
    plus = magic == 0x20B           # PE32+ (64 Bit)

    # MajorSubsystemVersion liegt bei 48, nicht bei 40 — dort steht die
    # OperatingSystemVersion, die Windows nicht auswertet. Die Subsystem-
    # fassung ist die, an der der Lader die Datei abweist.
    version = (struct.unpack_from("<H", data, optionalAt + 48)[0],
               struct.unpack_from("<H", data, optionalAt + 50)[0])

    # Die Datenverzeichnisse stehen hinter dem festen Teil.
    dirAt = optionalAt + (112 if plus else 96)
    def directory(index):
        at = dirAt + index * 8
        return struct.unpack_from("<II", data, at)   # (rva, groesse)

    # Abschnitte, um RVA in Dateiversatz umzurechnen.
    sections = []
    sectionAt = optionalAt + optionalSize
    for i in range(sectionCount):
        base = sectionAt + i * 40
        if base + 40 > len(data): break
        virtualSize, virtualAddress, rawSize, rawAt = struct.unpack_from(
            "<IIII", data, base + 8)
        sections.append((virtualAddress, max(virtualSize, rawSize), rawAt))

    def offsetOf(rva):
        for virtualAddress, size, rawAt in sections:
            if virtualAddress <= rva < virtualAddress + size:
                return rawAt + (rva - virtualAddress)
        return None

    def stringAt(rva):
        at = offsetOf(rva)
        if at is None or at >= len(data): return ""
        end = data.find(b"\0", at)
        return data[at:end if end >= 0 else len(data)].decode(errors="replace")

    def readTable(rva, delayFormat):
        """Ein Importverzeichnis auslesen."""
        result = {}
        at = offsetOf(rva)
        if at is None: return result
        entrySize = 32 if delayFormat else 20
        while at + entrySize <= len(data):
            fields = struct.unpack_from(f"<{entrySize // 4}I", data, at)
            if not any(fields): break
            if delayFormat:
                nameRva, thunkRva = fields[1], fields[4]
            else:
                thunkRva, nameRva = fields[0] or fields[4], fields[3]
            name = stringAt(nameRva).lower()
            if not name: break
            functions = set()
            thunkAt = offsetOf(thunkRva)
            step = 8 if plus else 4
            fmt = "<Q" if plus else "<I"
            while thunkAt is not None and thunkAt + step <= len(data):
                value = struct.unpack_from(fmt, data, thunkAt)[0]
                if value == 0: break
                ordinalBit = (1 << 63) if plus else (1 << 31)
                if not (value & ordinalBit):
                    hintAt = offsetOf(value & 0x7FFFFFFF)
                    if hintAt is not None and hintAt + 2 < len(data):
                        end = data.find(b"\0", hintAt + 2)
                        functions.add(data[hintAt + 2:end].decode(errors="replace"))
                thunkAt += step
            result[name] = functions
            at += entrySize
        return result

    importRva, _ = directory(1)
    delayRva, _ = directory(13)
    imports = readTable(importRva, False) if importRva else {}
    delayed = set(readTable(delayRva, True).keys()) if delayRva else set()
    return imports, delayed, version


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    path = Path(sys.argv[1])
    if not path.exists():
        print(f"nicht gefunden: {path}")
        return 2

    imports, delayed, version = readPortableExecutable(path)
    if imports is None:
        print("keine gueltige Windows-Programmdatei")
        return 2

    problems = []
    print(f"{path.name}\n")
    print("Fest gebundene DLLs:")
    for dll in sorted(imports):
        known = dll in WIN7_DLLS
        marks = [f for f in imports[dll] if f in NEWER_FUNCS]
        state = "ok" if known and not marks else "PROBLEM"
        print(f"  {dll:<24} {state}")
        if not known:
            problems.append(f"{dll} gibt es auf Windows 7 moeglicherweise nicht")
        for m in marks:
            problems.append(f"{dll}!{m} gibt es auf Windows 7 nicht")

    if delayed:
        print("\nVerzoegert gebunden (unkritisch — erst beim Aufruf):")
        for dll in sorted(delayed):
            print(f"  {dll}")
    print(f"\nMindestfassung im Kopf: {version[0]}.{version[1]}", end="")
    if version > (6, 1):
        print("  <-- hoeher als Windows 7 (6.1)!")
        problems.append(f"Der Kopf verlangt Windows {version[0]}.{version[1]}")
    else:
        print("  (6.1 = Windows 7, niedriger ist auch gut)")

    print()
    if problems:
        print(f"{len(problems)} Punkte, die Windows 7 verhindern:")
        for p in problems:
            print(f"  - {p}")
        return 1
    print("Nichts gefunden, was Windows 7 am Laden hindert.")
    print("(Das heisst: sie LAEDT. Ob sie fehlerfrei laeuft, sagt erst ein Test.)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
