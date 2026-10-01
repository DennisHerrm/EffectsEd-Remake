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

#include <string>

namespace efx::paths {

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
