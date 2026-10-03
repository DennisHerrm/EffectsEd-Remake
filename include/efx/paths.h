// Wo die Einstellungen liegen.
//
// Dieselbe Regelung wie bei g2c, aus denselben Gründen:
//
// Nicht neben die Exe. Wer das Programm nach `C:\Program Files` legt, hat
// dort keine Schreibrechte, und Windows leitet Schreibzugriffe seit Vista
// stillschweigend in einen benutzerabhängigen Ordner um. Die Einstellungen
// waren damit mal da und mal weg.
//
// Auch nicht unter „Dokumente": der Ordner gehört dem Nutzer für eigene
// Dateien. Programme, die dort ihre Konfiguration ablegen, müllen ihn zu.
//
// Richtig ist `%APPDATA%\efxed\`. Wer es mitnehmbar haben will — Stick,
// Netzlaufwerk, mehrere Zweige nebeneinander —, legt neben die Exe eine leere
// Datei `efxed_portable.txt`; dann liegt alles daneben.
//
// Die Ordnerlogik steht hier und nicht in der Oberfläche, damit sie prüfbar
// bleibt: unter Linux tritt `HOME` an die Stelle von `APPDATA`, und die Tests
// laufen gegen einen Wegwerfordner statt gegen den echten Benutzerordner.
#pragma once

#include <filesystem>
#include <string>

namespace efx::paths {

// ALLE Pfade im Programm sind UTF-8 (Dateidialog, Einstellungen,
// Kommandozeile, Bestand). An die Dateifunktionen gehen sie nur ueber diese
// beiden Umwandlungen.
//
// Anlass (Fehlersuche 03.10.2026): ein std::string direkt an ifstream oder
// std::filesystem::path gegeben, liest Windows als ANSI-Codepage. Unter einem
// Benutzerordner "Jörg" liess sich dann keine Datei oeffnen, der Spielpfad
// galt als leer, und die Windows-7-Fassung stuerzte schon beim Start ab.
std::filesystem::path fromUtf8(const std::string& utf8);
std::string toUtf8(const std::filesystem::path& path);

// Der Ordner der laufenden .exe (UTF-8). main setzt ihn als Erstes; davon
// haengt der mitnehmbare Betrieb ab. Vorher wurde er nie gesetzt, und
// "neben der Exe" hiess "im aktuellen Arbeitsordner" — wer ueber eine
// Verknuepfung mit anderem "Ausfuehren in" startete, verlor den Betrieb.
void setExeDirectory(const std::string& utf8);

// Eine Datei vollstaendig ersetzen, oder gar nicht: erst "<pfad>.tmp"
// schreiben und pruefen, dann darueber umbenennen. Vorher wurde die Datei
// zuerst geleert und dann beschrieben — ging dabei etwas schief (Platte voll,
// Netzlaufwerk weg, Absturz), war das Original weg, und das Speichern galt
// trotzdem als gelungen. false heisst: am Ziel hat sich nichts geaendert.
bool writeFileReplacing(const std::string& utf8Path, const void* data, size_t size);
inline bool writeFileReplacing(const std::string& utf8Path, const std::string& text) {
    return writeFileReplacing(utf8Path, text.data(), text.size());
}

// Ordner für Einstellungen, Fensterzustand und Startprotokoll.
// Legt ihn an, falls er fehlt. Schlägt das fehl, kommt der Ordner der Exe
// zurück — lieber am falschen Ort speichern als gar nicht.
std::string configDir();

// Einzelne Dateien darin.
std::string settingsPath();   // efxed_settings.txt
std::string imguiIniPath();   // efxed_gui.ini — Fensterzustand von Dear ImGui
std::string startupLogPath(); // efxed_start.log — die erste Diagnosequelle

// Gilt gerade der mitnehmbare Betrieb?
bool isPortable();

// Nur für Tests: setzt den Basisordner und schaltet die Umgebungsvariablen
// aus. Leerer Wert stellt das normale Verhalten wieder her.
void setOverrideDirForTesting(const std::string& dir);

}  // namespace efx::paths
