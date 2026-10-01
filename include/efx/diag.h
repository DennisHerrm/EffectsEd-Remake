// Ablaufprotokoll.
//
// Wozu das da ist: wenn das Programm auf einem fremden Rechner beim Start
// abstürzt, gibt es keinen Debugger, keinen Stapelabzug und keine Person, die
// sagen kann, was sie getan hat. Es gibt nur eine Datei. Die letzte Zeile
// darin muss den Schritt benennen, der gerade lief.
//
// Bei g2c hat genau das den Absturz gefunden: das Protokoll endete bei
// „Schriften laden", und weil zwischen dieser Marke und der nächsten drei
// Dinge lagen, war die Stelle noch nicht bestimmt. Erst feinere Marken haben
// es auf das Laden der Symbolschrift eingegrenzt.
//
// Daraus vier Regeln, die dieses Protokoll durchsetzt:
//
//  1. Nach jeder Zeile wird geleert. Ein gepuffertes Protokoll verliert genau
//     die Zeile, auf die es ankommt.
//  2. Marken sind fein. Lieber eine zu viel als eine, die drei Schritte
//     zusammenfasst.
//  3. Ein Schritt vermerkt seinen Beginn, bevor er anfängt — nicht sein Ende,
//     wenn er fertig ist. Ein Schritt ohne Abschlusszeile ist der Schuldige.
//  4. Das vorige Protokoll wird beim Start beiseitegelegt, nicht überschrieben.
//     Sonst löscht der Neustartversuch nach dem Absturz gerade den Beweis.
#pragma once

#include <string>
#include <vector>

namespace efx::diag {

enum class Level {
    Step,     // Ablaufmarke
    Info,
    Warning,
    Error,
};

// Öffnet die Protokolldatei und legt ein vorhandenes Protokoll als
// "<name>.vorher" beiseite. Muss als Erstes im Programm laufen — vor allem
// vor allem, was abstürzen könnte.
//
// Gibt false zurück, wenn die Datei nicht geschrieben werden kann. Das ist
// kein Grund abzubrechen: das Protokoll läuft dann nur im Speicher weiter und
// lässt sich über den Menüpunkt in der Oberfläche ansehen.
bool open(const std::string& path);

// Schließt die Datei und vermerkt einen ordentlichen Abschluss. Fehlt diese
// Zeile in einem Protokoll, ist das Programm nicht sauber beendet worden.
void close();

// Eine Zeile schreiben. Die Einrückung ergibt sich aus der Schachtelungstiefe.
void write(Level level, const std::string& text);

inline void info(const std::string& text) { write(Level::Info, text); }
inline void warn(const std::string& text) { write(Level::Warning, text); }
inline void error(const std::string& text) { write(Level::Error, text); }

// Ein Schritt. Der Beginn wird sofort vermerkt, das Ende beim Verlassen des
// Gültigkeitsbereichs — samt Dauer.
//
//     {
//         efx::diag::Step step("Symbolschrift laden");
//         ladeSymbolschrift();      // stürzt das hier ab, fehlt die
//     }                             // Abschlusszeile, und man weiß es
//
// Bewusst als Objekt und nicht als Funktionspaar: ein vorzeitiges return oder
// eine Ausnahme darf die Abschlusszeile nicht verschlucken, sonst sähe ein
// übersprungener Schritt aus wie ein abgestürzter.
class Step {
public:
    explicit Step(std::string name);
    ~Step();

    Step(const Step&) = delete;
    Step& operator=(const Step&) = delete;

    // Vermerkt einen Misserfolg, ohne den Schritt abzubrechen. Die
    // Abschlusszeile trägt dann den Grund.
    void fail(const std::string& reason);

private:
    std::string name_;
    long long startMicros_ = 0;
    std::string failure_;
};

// Der zuletzt begonnene, noch nicht abgeschlossene Schritt. Für einen
// Ausnahmebehandler: der kann damit sagen, wo es passiert ist, ohne das
// Protokoll lesen zu müssen.
std::string currentStep();

// Alle bisherigen Zeilen. Für den Menüpunkt „Protokoll anzeigen" und für den
// Fall, dass die Datei nicht geschrieben werden konnte.
const std::vector<std::string>& lines();

// Kopfzeilen mit Angaben zur Umgebung. Der Aufrufer liefert sie, weil sie
// betriebssystemabhängig sind; hier steht nur, wie sie im Protokoll landen.
void writeHeader(const std::vector<std::pair<std::string, std::string>>& entries);

// Nur für Tests: setzt alles zurück.
void resetForTesting();

}  // namespace efx::diag
