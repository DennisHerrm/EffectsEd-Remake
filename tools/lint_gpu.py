#!/usr/bin/env python3
"""Prueft, ob jede Texturkennung beim Wechsel der Grafikschnittstelle vergessen wird.

Anlass: ein Absturz beim Umschalten von OpenGL auf Direct3D 11.

Eine `render::TextureId` ist unter Direct3D ein ZEIGER auf eine
ID3D11ShaderResourceView. Wechselt die Grafikschnittstelle, verschwindet der
alte Renderer samt seiner Ressourcen — die Kennung zeigt danach auf
freigegebenen Speicher, und der erste Zeichenaufruf damit stuerzt ab.

Dafuer gibt es `App::forgetGraphicsResources()`. Der Fehler ist nie, dass die
Funktion fehlt; der Fehler ist, dass jemand ein neues Feld anlegt und vergisst,
es dort einzutragen. Genau das ist mit `fallbackTexture_` passiert — dem
weichen Ersatzfleck fuer fehlende Bilder, ein paar Runden nach `wallTexture_`,
das ordentlich eingetragen war.

Es ist eine Regel, die sich mechanisch pruefen laesst: jedes Feld vom Typ
`render::TextureId` in gui/app.h muss in forgetGraphicsResources vorkommen.
Der Uebersetzer kann das nicht sehen, ein Test auch nicht — dafuer braeuchte es
zwei echte Grafikschnittstellen nacheinander.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Ein Feld: "render::TextureId name_ = ...;" — nur Member, an der Endung mit
# Unterstrich erkennbar. Oertliche Variablen heissen nicht so.
FIELD = re.compile(r'render::TextureId\s+(\w+_)\s*(?:=|;)')


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.DOTALL)
    return re.sub(r'//[^\n]*', ' ', text)


def main():
    header = strip_comments((ROOT / "gui" / "app.h").read_text(encoding="utf-8"))
    fields = sorted(set(FIELD.findall(header)))

    if not fields:
        print("keine Texturkennungen gefunden — steht der Pruefer richtig?")
        return 1

    source = (ROOT / "gui" / "app_resources.cpp").read_text(encoding="utf-8")
    marker = "void App::forgetGraphicsResources()"
    if marker not in source:
        print("forgetGraphicsResources() nicht gefunden")
        return 1

    # Nur den Rumpf betrachten: eine Erwaehnung anderswo zaehlt nicht.
    start = source.index(marker)
    depth = 0
    end = start
    for i in range(source.index("{", start), len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                end = i
                break
    body = strip_comments(source[start:end])

    missing = [name for name in fields if name not in body]
    for name in missing:
        print(f"{name} wird beim Wechsel der Grafikschnittstelle nicht vergessen "
              f"(gui/app_resources.cpp, forgetGraphicsResources)")

    # Zweite Regel, dasselbe Thema: eine geteilte Ressource, deren Benutzer
    # sich nicht abmelden.
    #
    # Browser und Bearbeitungsansicht teilen sich EIN Ansichtsziel. Der
    # Browser laesst seine Kacheln darin ueber Bilder hinweg stehen; jeder
    # beginViewport-Aufruf leert es. Wer es leert, muss das dem Browser sagen,
    # sonst zeigt der ein leeres Raster und haelt es fuer gezeichnet.
    #
    # Genau so ist es passiert: Effekt oeffnen, zurueck zur Startseite, alle
    # Kacheln weg.
    shared = 0
    for path in sorted((ROOT / "gui").glob("app*.cpp")):
        lines = path.read_text(encoding="utf-8").splitlines()
        for i, line in enumerate(lines):
            if "->beginViewport(" not in line:
                continue
            # In den fuenf Zeilen davor muss die Abmeldung stehen.
            before = "\n".join(lines[max(0, i - 5):i])
            if "viewportUsedByEditor_ = true" not in before:
                print(f"{path.name}:{i + 1}: leert das gemeinsame Ansichtsziel, "
                      f"ohne viewportUsedByEditor_ zu setzen")
                shared += 1

    # Dritte Regel, wieder dasselbe Thema: eine Ressource, die ein anderer
    # Faden gerade liest.
    #
    # Texturen werden im Hintergrund gesucht und dekodiert; die Arbeitsfaeden
    # LESEN dabei `assets_`. Wer den Bestand ersetzt oder ein Archiv
    # einmischt, veraendert genau das — waehrend gelesen wird. Ein Wettlauf
    # auf einem vector, der gerade umzieht, endet nicht mit falschen Daten,
    # sondern mit einem Absturz an unverstaendlicher Stelle.
    #
    # `settleTextureJobs()` wartet die Auftraege ab. Der Uebersetzer kann das
    # nicht erzwingen, ein Test auch nicht — dafuer muesste der Wettlauf
    # eintreten, und das tut er einmal unter tausend Laeufen.
    CHANGES_ASSETS = re.compile(r'\bassets_\s*=|assets::scanArchive\s*\(')
    unsettled = 0
    for path in sorted((ROOT / "gui").glob("app*.cpp")):
        lines = path.read_text(encoding="utf-8").splitlines()
        for i, line in enumerate(lines):
            if line.lstrip().startswith("//"):
                continue
            if not CHANGES_ASSETS.search(line):
                continue
            before = "\n".join(lines[max(0, i - 6):i])
            if "settleTextureJobs()" not in before:
                print(f"{path.name}:{i + 1}: aendert den Bestand, ohne vorher "
                      f"settleTextureJobs() zu rufen")
                unsettled += 1

    total = len(missing) + shared + unsettled
    print(f"{len(missing)} von {len(fields)} Texturkennungen nicht zurueckgesetzt, "
          f"{shared} unangemeldete Zielwechsel, "
          f"{unsettled} ungesicherte Bestandsaenderungen")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
