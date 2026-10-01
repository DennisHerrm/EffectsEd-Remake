#!/usr/bin/env python3
"""Haelt jeden Menuebefehl von EffectsEd.exe gegen unsere Oberflaeche.

Warum als Werkzeug und nicht als Dokument: ABGLEICH.md stand auf rc29 und war
damit fuenfzig Fassungen alt. Ein Abgleich, der einmal von Hand gemacht wurde,
ist nach der naechsten Runde eine Behauptung.

Das Programm liest die MENUE-Ressource aus dem Binary — also die Wahrheit und
nicht eine Abschrift — und sucht zu jedem Befehl den englischen Text in
`tools/gen_i18n.py`. Findet es ihn, ist der Punkt in der Oberflaeche
vorhanden; findet es ihn nicht, wird er gemeldet.

    python3 tools/check_menu.py [pfad/zu/EffectsEd.exe]

Was das NICHT prueft: ob der Menuepunkt auch etwas tut. Dafuer gibt es
lint_calls.py und die Tests. Es prueft, dass keiner fehlt — und genau das ist
zweimal schiefgegangen: einmal war meine Liste zu lang (Dinge, die es schon
gab), einmal zu kurz (drei Punkte ohne Wirkung, die ich uebersehen hatte).

Braucht `pefile`. Fehlt es, wird der Abgleich uebersprungen statt
fehlzuschlagen: nicht jeder Rechner, auf dem gebaut wird, hat das Binary.
"""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = Path("/mnt/user-data/uploads/EffectsEd.exe")

# Befehle, die bewusst fehlen, mit Begruendung. Jede Zeile ist eine
# Entscheidung, keine Luecke.
DELIBERATE = {
    "Print...": "Drucken eines Partikeleffekts ergibt nichts Sinnvolles",
    "Print Preview": "siehe Drucken",
    "Print Setup...": "siehe Drucken",
    "Recent File": "wir haben eine eigene Liste zuletzt geoeffneter Dateien",
    "OpenGL Driver Info": "bei uns 'Grafiktreiber-Information' - beide Schnittstellen",
}

# Wo der Wortlaut abweicht, weil er bei uns allgemeiner ist.
ALIASES = {
    "Refresh (reload) assets": "Refresh assets",
    "Choose Wall Color...": "Choose wall colour...",
    "Choose Background Color...": "Choose background colour...",
    "Set Default Game Path...": "Set Default Game Path",
    "Reset Default FX Repeat Rate On Restart": "Reset repeat rate on restart",
    "Set Custom FX Origin": "Set custom effect origin",
    "About EffectsEd...": "About EffectsEd",
    # Nachgeschlagen, nicht geraten: zu jedem dieser acht gibt es bei uns
    # einen Menuepunkt, er heisst nur anders.
    "Refresh (reload) assets": "Reload Assets",
    "Delete": "Delete Segment",
    "Choose Wall Color...": "Wall Color...",
    "Choose Background Color...": "Background Color...",
    "Draw Wind Vector": "Show Wind Vector",
    "Draw Wireframe": "Wireframe",
    "Reset/zero view position": "Reset View",
    "Set Custom FX Origin": "Set Custom FX Origin...",
}


def menu_commands(exe):
    import pefile
    pe = pefile.PE(str(exe))

    def resource(kind):
        for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
            if pefile.RESOURCE_TYPE.get(entry.struct.Id) != kind:
                continue
            for res in entry.directory.entries:
                for lang in res.directory.entries:
                    yield pe.get_data(lang.data.struct.OffsetToData,
                                      lang.data.struct.Size)

    def wide(data, pos):
        out = []
        while True:
            value = struct.unpack('<H', data[pos:pos + 2])[0]
            pos += 2
            if value == 0:
                break
            out.append(chr(value))
        return ''.join(out), pos

    POPUP = 0x0010
    found = []
    for data in resource('RT_MENU'):
        # Bis zum Ende der Ressource lesen, statt die Schachtelung
        # nachzubilden.
        #
        # Zwei Anlaeufe ueber MF_END sind gescheitert. Beim ersten kam nur das
        # File-Menue an (10 von 49). Beim zweiten brachten die beiden
        # Untermenues in „Ansicht" den Stapel aus dem Gleichgewicht, und die
        # Auslese endete eine Zeile zu frueh — ausgerechnet bei
        # „About EffectsEd...", dem letzten Befehl.
        #
        # Die Laenge der Ressource ist die verlaesslichere Grenze: sie steht
        # im Ressourcenverzeichnis und endet hier auf das Byte genau mit dem
        # letzten Eintrag. Die Schachtelung interessiert uns ohnehin nicht —
        # gesucht sind die Befehlsnummern, nicht der Baum.
        pos = 4          # MENUHEADER
        while pos + 4 <= len(data):
            flags = struct.unpack('<H', data[pos:pos + 2])[0]
            pos += 2
            if flags & POPUP:
                _text, pos = wide(data, pos)
                continue
            command = struct.unpack('<H', data[pos:pos + 2])[0]
            pos += 2
            text, pos = wide(data, pos)
            # Trennlinien haben die Nummer 0 und keinen Text.
            if command:
                found.append((command,
                              text.replace('&', '').split('\t')[0].strip()))
    return found


def main():
    exe = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_EXE
    if not exe.exists():
        print(f"{exe} nicht vorhanden - Abgleich uebersprungen")
        return 0
    try:
        commands = menu_commands(exe)
    except ImportError:
        print("pefile fehlt - Abgleich uebersprungen "
              "(pip install pefile --break-system-packages)")
        return 0

    # Unsere englischen Texte. Die englische Spalte ist die zweite in jeder
    # Zeile von gen_i18n.py.
    source = (ROOT / "tools" / "gen_i18n.py").read_text(encoding="utf-8")
    ours = set()
    for line in source.splitlines():
        match = re.match(r'\s*\("(\w+)","((?:[^"\\]|\\.)*)"', line)
        if match:
            ours.add(match.group(2).replace('\\"', '"'))

    def present(text):
        wanted = ALIASES.get(text, text)
        if wanted in ours:
            return True
        # Grosszuegiger zweiter Versuch: gleiche Woerter, andere Zeichensetzung.
        squash = lambda t: re.sub(r'[^a-z0-9]', '', t.lower())
        target = squash(wanted)
        return any(squash(one) == target for one in ours)

    missing, skipped = [], []
    for command, text in commands:
        if text in DELIBERATE:
            skipped.append((command, text))
        elif not present(text):
            missing.append((command, text))

    print(f"{len(commands)} Menuebefehle in {exe.name}")
    print(f"  umgesetzt          {len(commands) - len(missing) - len(skipped)}")
    print(f"  bewusst ausgelassen {len(skipped)}")
    for command, text in skipped:
        print(f"      {command:6d}  {text:42s} {DELIBERATE[text]}")
    if missing:
        print(f"  OHNE ENTSPRECHUNG  {len(missing)}")
        for command, text in missing:
            print(f"      {command:6d}  {text}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
