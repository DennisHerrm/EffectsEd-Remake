#!/usr/bin/env python3
"""Prueft, ob jeder in gui/*.cpp benutzte Membername in app.h deklariert ist.

Anlass: ich habe zweimal einen Membernamen erfunden, der nicht existiert
(`currentPath_` statt `filePath_`), und einmal eine Variable aus einer anderen
Funktion benutzt (`window` statt `hwnd`). Beides faellt erst beim Uebersetzen
auf — und uebersetzen laesst sich gui/ nur auf einem Windows-Rechner mit ImGui.

Diese Pruefung braucht keinen Uebersetzer. Sie ist grob, aber sie faengt genau
den Fehler, der hier zweimal durchgerutscht ist.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "gui" / "app.h"

# Namen, die auf _ enden, sind hier durchgaengig Member.
MEMBER = re.compile(r'\b([a-z][A-Za-z0-9]*_)\b')

# Zeichenketten heraussschneiden, bevor gesucht wird.
#
# Anlass: das Format "efxed_%Y%m%d_%H%M%S.tga" enthaelt "d_", und das sieht
# fuer das Muster oben wie ein Membername aus. Ein Pruefer, der Zeichenketten
# durchsucht, meldet frueher oder spaeter jeden Dateinamen mit Unterstrich.
STRING_LITERAL = re.compile(r'"(?:[^"\\]|\\.)*"')

def without_strings(line: str) -> str:
    return STRING_LITERAL.sub('""', line)

# Eine Deklaration: ein Typ, dann der Name, dann =, ; oder {.
#   int foo_ = 0;        ComPtr<ID3D11Device> device_;      Foo bar_{};
DECLARATION = re.compile(
    r'^\s*(?:static\s+|mutable\s+|const\s+|inline\s+)*'
    r'[A-Za-z_][\w:]*(?:\s*<[^;{]*>)?(?:\s*[*&])?\s+'
    r'([a-z][A-Za-z0-9]*_)\s*(?:=|;|\{|\[)')

def declared_members(text):
    """Namen, die in diesem Text tatsaechlich deklariert werden.

    Nicht einfach alle vorkommenden: sonst gilt ein Tippfehler als deklariert,
    sobald er zweimal auftaucht — und genau zweimal kam currentPath_ vor."""
    names = set()
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        match = DECLARATION.match(without_strings(line))
        if match:
            names.add(match.group(1))
    return names

def main():
    header = HEADER.read_text(encoding="utf-8")
    fromHeader = declared_members(header)

    problems = []
    for path in sorted((ROOT / "gui").glob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        # Jede Datei darf eigene Member haben — die Renderer haben ihre
        # eigenen Klassen. Deshalb: app.h, der gleichnamige Kopf dieser Datei,
        # und was sie selbst deklariert.
        #
        # Der gleichnamige Kopf kam erst dazu, als audio_win32.cpp entstand:
        # dessen Member stehen in audio_win32.h, und der Pruefer meldete
        # zweiundzwanzig gueltige Namen als unbekannt. Ein Pruefer, der
        # Richtiges anmeckert, wird ignoriert — also nachgezogen.
        known = fromHeader | declared_members(text)
        ownHeader = path.with_suffix(".h")
        if ownHeader.exists():
            known |= declared_members(ownHeader.read_text(encoding="utf-8"))
        for number, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            for name in MEMBER.findall(without_strings(line)):
                if name in known:
                    continue
                # Lokale Variablen mit Unterstrich am Ende gibt es nicht in
                # diesem Quelltext; wer eine einfuehrt, bekommt hier eine
                # Meldung und kann sie umbenennen.
                problems.append((path.name, number, name, stripped[:70]))

    for name, number, member, line in problems:
        print(f"\n{name}\n  Zeile {number}: {member} ist in app.h nicht deklariert"
              f"\n      {line}")
    print(f"\n{len(problems)} unbekannte Member")
    return 1 if problems else 0

if __name__ == "__main__":
    sys.exit(main())
