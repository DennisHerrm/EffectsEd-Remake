#!/usr/bin/env python3
"""Findet Kernfunktionen, die NUR von Tests aufgerufen werden.

Warum es dieses Werkzeug gibt: derselbe Fehler ist in einer Sitzung dreimal
aufgetreten.

    camera::Shake      vollstaendig umgesetzt, jedes Bild weitergerechnet,
                       an die Kamera gegeben — aber nie ausgeloest.
    applyTexMods       Formeln aus dem Renderer nachgebaut, geprueft, mit
                       einer Notiz ueber einen behobenen Vorzeichenfehler —
                       und von niemandem ausser den Tests aufgerufen.
    tcMod turb         umgesetzt und geprueft, aber der Aufrufer gab die
                       Position des Eckpunkts nicht weiter; der Zweig lief nie.

Alle drei sahen im Quelltext fertig aus. Alle drei waren im laufenden Programm
wirkungslos. Kein Test faellt darauf herein, weil der Test die Funktion ja
selbst aufruft — genau deshalb braucht es eine Regel, die von aussen schaut.

Gemeldet wird: in `include/efx` deklariert, in `tests/` benutzt, aber in `src/`
und `gui/` nirgends. Das ist kein Beweis fuer einen Fehler — manches ist
absichtlich nur fuer Werkzeuge oder die Zukunft da. Deshalb gibt es eine
Ausnahmeliste, und jeder Eintrag darin braucht eine Begruendung.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Was absichtlich (noch) nicht aus dem Programm gerufen wird.
# Jeder Eintrag mit Grund — ohne Grund gehoert er nicht hierher.
EXPECTED = {
    # Nur fuer efxtool auf der Kommandozeile.
    "describeEffect",
    "formatDiagnostics",
    "blendModeName",

    # Stellen fuer die Pruefungen einen Zustand her oder lesen ihn aus.
    "setOverrideDirForTesting",
    "resetForTesting",
    "currentStep",
    "trIn",

    # Bausteine, die von groesseren Funktionen benutzt werden und deshalb
    # einzeln pruefbar sein sollen. `multiply` und `transformPoint` stecken in
    # den Matrizenrechnungen, `checkContrast` in `ensureContrast`, `tabName`
    # in `tabLabel`. Sie einzeln zu pruefen ist der Sinn der Sache — sonst
    # muesste man einen Fehler in der Matrix am fertigen Bild suchen.
    "multiply",
    "transformPoint",
    "checkContrast",
    "tabName",

    # Vorgaenger, die noch geprueft werden, weil ihre Regel weitergilt:
    # `buildRoom` ist in `buildRoomLit` aufgegangen, `visualReach` in
    # `describePreview`. Beide rufen die alte Fassung nicht mehr auf, aber die
    # Zusicherungen gelten unveraendert.
    "buildRoom",
    "visualReach",

    # Ein einzelner Spielordner. Das Programm liest seit dem Abgleich vom
    # 03.10.2026 nur noch ueber scanAll, weil die Shaderdateien ueber ALLE
    # Ordner zusammen geordnet werden muessen (wie die Engine ihren
    # Shadertext zusammenhaengt). scan bleibt die pruefbare Einheit dafuer.
    "scan",

    # Wird von der Oberflaeche ueber `settings_.windFlagPos` erledigt; die
    # Funktion rechnet die Vorgabelage aus und wird beim ersten Start
    # gebraucht.
    "windFlagPosition",

    # Braucht eine zerlegte Shaderbibliothek, die das Programm nicht fuehrt —
    # es hat den Bestand. Fuer den Anwender laeuft `validateShaderNames`, die
    # dieselbe Frage mit einem Rueckruf beantwortet. Diese Fassung prueft
    # zusaetzlich, ob ein Shader mit GL_SRC_ALPHA einen Alphawert bekommt;
    # dafuer muss man die Mischung kennen. Sie bleibt geprueft, damit die
    # Regel festgehalten ist, wenn die Bibliothek einmal dazukommt.
    "validateAgainstShaders",
}


def declaredFunctions():
    names = {}
    for header in sorted((ROOT / "include" / "efx").glob("*.h")):
        raw = header.read_text(encoding="utf-8").splitlines()

        # Deklarationen ueber mehrere Zeilen zusammenfassen.
        #
        # Ohne das sah der Pruefer nur einzeilige Deklarationen — und die
        # interessanten sind gerade die langen. `applyTexMods` mit seinen vier
        # Parametern passte in keine Zeile und wurde deshalb nie geprueft.
        joined = []
        buffer = ""
        for number, line in enumerate(raw, 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                if not buffer:
                    joined.append((number, line))
                continue
            if buffer:
                buffer += " " + stripped
                if stripped.endswith(";") or stripped.endswith("{"):
                    joined.append((bufferLine, buffer))
                    buffer = ""
                continue
            if ("(" in stripped and ")" not in stripped
                    and not stripped.startswith("#")):
                buffer = stripped
                bufferLine = number
                continue
            joined.append((number, line))

        for number, line in joined:
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            # Eine freie Funktion: Rueckgabetyp, Name, Klammer, Semikolon.
            match = re.match(
                r"^[A-Za-z_][\w:<>,&*\s]*?\b([a-z][A-Za-z0-9_]*)\s*\([^;]*\)\s*;$",
                stripped)
            if not match:
                continue
            name = match.group(1)
            if name in ("if", "for", "while", "switch", "return", "sizeof"):
                continue
            names.setdefault(name, (header.name, number))
    return names


def countCalls(name, folders):
    pattern = re.compile(r"\b" + re.escape(name) + r"\s*\(")
    total = 0
    for folder in folders:
        for path in sorted((ROOT / folder).rglob("*.cpp")):
            for line in path.read_text(encoding="utf-8").splitlines():
                stripped = line.strip()
                if stripped.startswith("//") or stripped.startswith("*"):
                    continue
                # Die Definition selbst ist kein Aufruf.
                #
                # Erkannt an der Einrueckung: eine Definition in einer .cpp
                # steht in Spalte eins. Der erste Anlauf pruefte stattdessen
                # auf "endet mit Klammer auf" — und verschluckte damit jedes
                #
                #     if (camera::intersectGroundPlane(ray, 0.0f, at)) {
                #
                # also genau die Aufrufe, die es finden sollte. Neunzehn
                # Meldungen, die meisten davon falsch.
                if line and not line[0].isspace():
                    continue
                total += len(pattern.findall(stripped))
    return total


def main() -> int:
    problems = 0
    for name, (header, number) in sorted(declaredFunctions().items()):
        if name in EXPECTED:
            continue
        inTests = countCalls(name, ["tests"])
        inProgram = countCalls(name, ["src", "gui"])
        if inTests > 0 and inProgram == 0:
            print(f"{header}:{number}: {name} wird {inTests}x von Tests "
                  f"gerufen, aber nirgends im Programm")
            problems += 1

    print(f"{problems} Funktionen nur von Tests benutzt")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
