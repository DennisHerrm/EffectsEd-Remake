#!/usr/bin/env python3
"""Prueft, ob gui/*.cpp einen Namensraum benutzt, dessen Kopf es nicht einbindet.

Anlass: ein Baufehler, der erst beim Anwender auf Windows aufschlug.

    gui/renderer_d3d11.cpp:356: error C2653: "scene": Keine Klasse oder Namespace
    gui/renderer_d3d11.cpp:356: error C3861: "rgba": Bezeichner nicht gefunden

Ich hatte `scene::rgba(...)` geschrieben, ohne `efx/scene.h` einzubinden. Kein
vorhandener Pruefer konnte das sehen:

  - check_gui.sh uebersetzt die Renderer NICHT. Sie brauchen d3d11.h
    beziehungsweise die WGL-Schnittstelle, und beides gibt es hier nicht.
  - lint_includes.py laesst Funktionsnamen bewusst weg: `length`, `reset` und
    `parse` kommen ueberall vor und ergaeben nur Fehlmeldungen.

Der Namensraum ist die Luecke dazwischen. `scene::` ist eindeutig — es gibt
genau einen Kopf, der `namespace efx::scene` aufmacht. Wer den Namensraum
benutzt und den Kopf nicht einbindet, hat einen Fehler, und zwar sicher, nicht
vermutlich. Deshalb meldet dieser Pruefer nichts auf Verdacht.

Bewusst nur direkte Einbindungen, mit einer Ausnahme: gui/app.h ist der
Sammelkopf der Oberflaeche, was er einbindet, zaehlt mit. Dass ein Name ueber
einen dritten Kopf mitkommt, ist Zufall und kein Zustand, auf den man sich
verlassen sollte.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

NAMESPACE_DECL = re.compile(r'namespace\s+efx::(\w+)')
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
# Ein qualifizierter Zugriff: nicht selbst schon qualifiziert (kein ::davor)
# und kein Teil eines laengeren Namens.
QUALIFIED = re.compile(r'(?<![\w:])(\w+)::')

# Was der Uebersetzer ohnehin kennt oder was nicht aus unseren Koepfen kommt.
NOT_OURS = {"std", "efx", "fs", "chrono", "filesystem", "this_thread",
            "ImGui", "Microsoft", "WRL", "gl", "detail"}


def strip_comments_and_strings(text):
    """Damit ein Namensraum in einem Kommentar oder Text nicht zaehlt."""
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.DOTALL)
    text = re.sub(r'//[^\n]*', ' ', text)
    text = re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', text)
    return text


def main():
    # Schritt 1: welcher Kopf macht welchen Namensraum auf?
    providers = {}
    for header in sorted((ROOT / "include" / "efx").glob("*.h")):
        body = strip_comments_and_strings(header.read_text(encoding="utf-8"))
        for name in NAMESPACE_DECL.findall(body):
            providers.setdefault(name, set()).add("efx/" + header.name)

    if not providers:
        print("keine Namensraeume gefunden — steht der Pruefer richtig?")
        return 1

    # Schritt 2: was ein gui/-Kopf einbindet, zaehlt fuer den mit.
    #
    # gui/app.h und gui/app_shared.h sind Sammelkoepfe der Oberflaeche — sie
    # gehoeren uns, ihre Einbindungen sind eine Zusage und kein Zufall. Der
    # erste Entwurf loeste nur app.h auf und meldete daraufhin sechzehn
    # Namensraeume, die alle in Ordnung waren. Ein Pruefer mit sechzehn
    # Fehlmeldungen wird ignoriert, und dann faengt er die richtige auch nicht
    # mehr.
    #
    # Fremde Koepfe werden NICHT aufgeloest: dass ein Name ueber imgui.h
    # mitkommt, waere Zufall.
    gui_headers = {}
    for header in (ROOT / "gui").glob("*.h"):
        gui_headers[header.name] = set(
            INCLUDE.findall(header.read_text(encoding="utf-8")))

    def expand(names):
        out = set(names)
        pending = list(names)
        while pending:
            current = pending.pop()
            for nested in gui_headers.get(current, ()):
                if nested not in out:
                    out.add(nested)
                    pending.append(nested)
        return out

    problems = []
    for path in sorted((ROOT / "gui").glob("*.cpp")):
        raw = path.read_text(encoding="utf-8")
        body = strip_comments_and_strings(raw)

        included = expand(INCLUDE.findall(raw))

        # Der eigene Namensraum der Datei braucht keinen Kopf.
        own = set(NAMESPACE_DECL.findall(body))

        seen = set()
        for name in QUALIFIED.findall(body):
            if name in NOT_OURS or name in own or name in seen:
                continue
            if name not in providers:
                continue
            seen.add(name)
            if not (providers[name] & included):
                wanted = " oder ".join(sorted(providers[name]))
                problems.append((path.name, name, wanted))

    for filename, name, wanted in problems:
        print(f"{filename}: benutzt {name}:: ohne {wanted}")
    print(f"{len(problems)} Namensraeume ohne ihren Kopf")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
