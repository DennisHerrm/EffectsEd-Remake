#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Sucht Oberflaechentexte, die nicht durch tr() laufen.

Der Anlass: auf die Frage "ist wirklich alles uebersetzt?" konnte ich nicht
antworten, weil ich es nie geprueft hatte. Ein Blick in die Dateien reicht
nicht - die Oberflaeche hat inzwischen ueber hundert Zeichenketten, und
uebersehen ist eine davon schnell.

Geprueft wird nur, was der Nutzer zu sehen bekommt. Nicht geprueft werden:
Dateipfade, ImGui-Kennungen mit ##, Formatangaben, Tastenkuerzel, Einbindungen
und die Meldungen des Ablaufprotokolls (die sind bewusst englisch, siehe
ENTSCHEIDUNGEN.md).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GUI = ROOT / "gui"

# Auch der Kern wird geprueft. Anlass: die Themennamen ("Dunkel",
# "Mitternacht", ...) standen fest verdrahtet in src/theme.cpp, und die
# Weltmassstaebe in src/layout.cpp. Der Pruefer sah nur gui/ und meldete null
# Lueckenen — im englischen Menue stand trotzdem "Dunkel".
CORE_PATTERN = re.compile(r'(name|label)\s*=\s*"([^"]{2,})"')

# Zweite Regel fuer den Kern: Zeichenketten in Tabellen und Rueckgabewerten.
#
# Anlass: dreizehn deutsche Typbeschreibungen standen in einer Tabelle in
# effect.cpp — ohne Umlaute, weil der Kern damals ASCII bleiben sollte. Der
# Pruefer kannte nur Zuweisungen an name/label und meldete null Luecken.
#
# Ein Satz mit Leerzeichen und einem Satzzeichen ist mit hoher
# Wahrscheinlichkeit Anzeigetext; ein Bezeichner wie "orgOnSphere" nicht.
CORE_SENTENCE = re.compile(r'"([A-Za-zÄÖÜäöüß][^"]*\s[^"]*[.:!?])"')

# Funktionen, deren Textargumente der Nutzer sieht.
UI_CALLS = re.compile(
    r'\b(?:ImGui::)?('
    r'MenuItem|BeginMenu|Button|SmallButton|Checkbox|Selectable|Text|TextUnformatted'
    r'|TextDisabled|TextColored|SetTooltip|BeginTabItem|BeginTabBar|InputText'
    r'|SliderFloat|DragFloat|BeginCombo|CollapsingHeader|TableSetupColumn'
    r'|BeginPopupModal|OpenPopup|Begin'
    r')\s*\('
)

# Was kein Uebersetzungsfall ist.
# Produktnamen und Fassungsnummern werden nicht uebersetzt.
NOT_TRANSLATED = {"EffectsEd ", "EffectsEd", "OpenGL", "Direct3D"}

def is_exempt(text: str) -> bool:
    if text in NOT_TRANSLATED:
        return True
    if not text:
        return True
    if text.startswith("##") or text.startswith("###"):
        return True                      # nur eine ImGui-Kennung
    # Formatangaben — aber nur, wenn ausser den Platzhaltern nichts uebrig
    # bleibt, was uebersetzt gehoert.
    #
    # Die erste Fassung nahm jede Zeichenkette mit `%s` heraus. Damit rutschte
    #
    #     ImGui::SetTooltip("%s\n%zu Segmente", ...)
    #
    # durch: deutscher Text mitten in einer Formatangabe, in vier Sprachen
    # unuebersetzt. Ein Prueferausnahme darf nicht so grob sein, dass sie das
    # Geprueffte gleich mit herausnimmt.
    stripped = re.sub(r'%[-+ #0-9.*]*[a-zA-Z]', ' ', text)
    stripped = stripped.replace("\\n", " ").replace("\\t", " ")
    if re.search(r'%', text) and not re.search(r'[A-Za-zÄÖÜäöüß]{3,}', stripped):
        return True                      # reine Formatangabe
    if re.fullmatch(r"[A-Za-z]+\+[A-Za-z0-9]+", text):
        return True                      # Tastenkuerzel wie Ctrl+N
    if re.fullmatch(r"[\[\]|<>()\s.,;:_\-+*/=0-9]+", text):
        return True                      # Symbole wie "|>" oder "[]"
    if text in ("Ins", "Del", "Space", "F5", "EffectsEd", "RGB", "OK"):
        return True                      # Eigennamen und Tastennamen
    if re.fullmatch(r"EffectsEd [0-9.]+(-[a-z0-9]+)?", text):
        return True                      # Programmname mit Fassungsnummer,
                                         # auch mit Zusatz wie -rc1
    if re.fullmatch(r"[a-zA-Z][a-zA-Z0-9_]*", text) and text[0].islower():
        return True                      # reine ImGui-Kennung wie propertyTabs
    return True if len(text) < 2 else False

def check(path: Path):
    findings = []
    source = path.read_text(encoding="utf-8")

    # Aufrufe koennen ueber MEHRERE Zeilen gehen.
    #
    # Vorher wurde Zeile fuer Zeile geprueft, und ein Text auf einer
    # Fortsetzungszeile rutschte durch. Genau so ist ein deutsches "Ersatz"
    # in die Oberflaeche gelangt:
    #
    #     ImGui::TextColored(toImGui(pal.warning), "%s: %s (%s)",
    #                        name.c_str(), modeText,
    #                        "Ersatz");            <- eigene Zeile, unbemerkt
    #
    # Deshalb wird jetzt mitgezaehlt, ob die Klammern eines Oberflaechen-
    # aufrufs noch offen sind. Solange sie es sind, gehoeren alle Texte dazu.
    depth = 0          # offene Klammern des laufenden Aufrufs
    startLine = 0      # wo er anfing — dorthin gehoert die Meldung
    hasTranslation = False

    for number, line in enumerate(source.splitlines(), 1):
        if depth == 0:
            if not UI_CALLS.search(line):
                continue
            startLine = number
            hasTranslation = False

        if "tr(" in line and "Str::" in line:
            # Der Aufruf benutzt bereits eine Uebersetzung; ein zusaetzliches
            # Literal darin ist meist eine Kennung ("##name", ein Format).
            hasTranslation = True

        if not hasTranslation:
            for literal in re.findall(r'"((?:[^"\\]|\\.)*)"', line):
                if is_exempt(literal):
                    continue
                findings.append((startLine, literal, line.strip()))

        # Klammernstand fortschreiben. Texte werden dabei ausgeblendet, sonst
        # zaehlt eine Klammer IN einem Text mit.
        withoutText = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)
        depth += withoutText.count("(") - withoutText.count(")")
        if depth < 0:
            depth = 0

    return findings

def check_core():
    """Feste Anzeigetexte im Kern. Dort gibt es kein ImGui, deshalb eine
    eigene Regel: Zuweisungen an Felder namens name oder label."""
    findings = []
    # i18n.cpp ist die Uebersetzungstabelle selbst und wird erzeugt. Sie dort
    # zu pruefen heisst, jeden uebersetzten Text als "nicht uebersetzt" zu
    # melden — und die echten Funde gehen darin unter.
    #
    # Vorher fiel das nicht auf, weil die zweite Regel (Zeichenketten, die wie
    # ein Satz aussehen) noch nicht existierte. Seit die Pruefliste
    # uebersetzte Saetze enthaelt, meldete sie 15 davon.
    GENERATED = {"i18n.cpp"}
    for path in sorted((ROOT / "src").glob("*.cpp")):
        if path.name in GENERATED:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if "i18n::" in line or "Str::" in line:
                continue
            match = CORE_PATTERN.search(line)
            if match and not is_exempt(match.group(2)):
                findings.append((path.name, number, match.group(2)))
                continue
            sentence = CORE_SENTENCE.search(line)
            if sentence and not is_exempt(sentence.group(1)):
                findings.append((path.name, number, sentence.group(1)))
    return findings

def main():
    total = 0
    for name, number, literal in check_core():
        print(f"\n{name}\n  Zeile {number}: \"{literal}\"  (fester Text im Kern)")
        total += 1
    for path in sorted(GUI.glob("*.cpp")) + sorted(GUI.glob("*.h")):
        findings = check(path)
        if not findings:
            continue
        print(f"\n{path.name}")
        for number, literal, line in findings:
            print(f"  Zeile {number}: \"{literal}\"")
            print(f"      {line[:100]}")
            total += 1
    print(f"\n{total} nicht uebersetzte Oberflaechentexte")
    return 1 if total else 0

if __name__ == "__main__":
    sys.exit(main())
