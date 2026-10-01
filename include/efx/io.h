// Lesen und Schreiben von .efx-Dateien.
#pragma once

#include <functional>
#include <string>

#include "efx/i18n.h"
#include <string_view>
#include <vector>

#include "efx/effect.h"
#include "efx/gp2.h"

namespace efx {

enum class Severity { Error, Warning, Info };

struct Diagnostic {
    Severity severity = Severity::Error;
    int line = 0;
    std::string message;

    // Welche Meldung das ist, unabhängig von der Sprache.
    //
    // `message` ist der fertige Text in der eingestellten Sprache — gut zum
    // Anzeigen, untauglich zum Vergleichen. Tests, die auf deutschen Text
    // prüften, wurden rot, sobald die Prüfliste übersetzt war; sie prüften
    // nicht die Regel, sondern ihre Formulierung.
    //
    // `Count` heißt: keine bestimmte, etwa bei Meldungen aus dem Leser.
    i18n::Str id = i18n::Str::Count;

    // Welches Segment die Meldung betrifft (0-basiert), -1 fuer keines.
    //
    // Frueher trugen die Pruefregeln die Segmentnummer in `line`, die
    // Hauptpruefung aber 0 — ein Klick im Meldungsfenster waehlte darum bei
    // keiner ihrer Meldungen das Segment, und die Shaderpruefung zeigte die
    // Segmentnummer als "Zeile" an. `line` ist jetzt immer die Dateizeile.
    int primitive = -1;
};

struct ReadResult {
    Effect effect;
    std::vector<Diagnostic> diagnostics;
    bool hasErrors() const;
};

// Liest den Inhalt einer .efx-Datei. Unbekannte Schluessel sind Warnungen,
// keine Fehler — genau wie im Spiel, das solche Zeilen ueberliest.
ReadResult read(std::string_view text);

// Zahlformat beim Schreiben.
enum class NumberFormat {
    // "%1.4g" — bitgleich mit dem alten Editor. Kuerzt 0.988235 auf 0.9882.
    Raven,
    // Kuerzeste Schreibweise, die den float exakt wiederherstellt. Wer eine
    // Datei nur oeffnet und speichert, verliert damit nichts.
    Exact,
};

struct WriteOptions {
    NumberFormat numbers = NumberFormat::Exact;
    bool crlf = true;   // Raven-Dateien haben Windows-Zeilenenden
    bool tabs = true;   // Einrueckung mit Tabs, wie im Original
};

std::string write(const Effect& effect, const WriteOptions& options = {});

// Prueft einen Effekt auf Dinge, die die Engine anders sieht, als der Autor
// vermutlich meint. Reine Analyse, aendert nichts.
//
// Der Dialekt entscheidet ueber einen Teil der Regeln: SP und MP haben
// getrennte Parser und kennen jeweils Flags, die der andere Zweig stumm
// ueberliest. Dialect::Both meldet alles, was nicht in beiden Zweigen laeuft —
// das ist die richtige Wahl fuer Effekte, die in beiden Spielmodi vorkommen.
std::vector<Diagnostic> validate(const Effect& effect,
                                 Dialect target = Dialect::Both);

}  // namespace efx

namespace efx::shader { struct Library; }

namespace efx {

// Zusatzpruefung gegen den vorhandenen Shaderbestand. Getrennt von validate(),
// weil sie Daten von aussen braucht: ohne base/-Verzeichnis gibt es sie nicht.
std::vector<Diagnostic> validateAgainstShaders(const Effect& effect,
                                               const shader::Library& library);

// Dieselbe Prüfung, aber mit einem Rückruf statt einer Shaderbibliothek.
//
// Der Grund: das Programm führt gar keine `shader::Library` — es hat den
// Bestand (`assets::Index`), und der weiß, ob es einen Shader oder eine
// Bilddatei dieses Namens gibt. Deshalb blieb die Prüfung angeschlossen an
// nichts: sie war umgesetzt, geprüft und für den Anwender wirkungslos.
//
// `exists` beantwortet: gibt es zu diesem Namen irgendetwas Zeichenbares?
std::vector<Diagnostic> validateShaderNames(
    const Effect& effect, const std::function<bool(const std::string&)>& exists);

}  // namespace efx
