// Der Auto-Updater: was sich ohne Netz und ohne Fenster pruefen laesst.
//
// Gewuenscht: "eine auto update funktion wenn ich dinge update die das direkt
// runterladen koennen". Der Ablauf (gui/update_win32.cpp):
//
//   1. GET api.github.com/repos/<kRepo>/releases/latest  -> Release (JSON)
//   2. Ist das Release neuer als diese Fassung (Revisionsnummer)?
//   3. Das .zip des Releases laden, die Dateien neben die .exe legen (die
//      laufende .exe wird dabei nur umbenannt), neu starten.
//
// Hier steht der pruefbare Teil: JSON lesen, Fassungen vergleichen, Zielpfade
// aus den Archiveintraegen. Uebernommen aus dem BehavEd-Nachbau, wo derselbe
// Ablauf seit Wochen laeuft.
#pragma once

#include <string>
#include <vector>

namespace efx::update {

// Das OEFFENTLICHE Repository, aus dem die Updates kommen (Muster wie beim
// BehavEd-Nachbau: Quelltext privat, "-Releases" oeffentlich). Darin liegen nur
// die fertigen Programme; jeder bekommt sie ohne GitHub-Konto.
inline constexpr const char* kRepo = "DennisHerrm/EffectsEd-Remake-Releases";

struct Asset {
    std::string name;         // "efxed-1.18.0-rev78.zip"
    std::string downloadUrl;  // github.com/.../releases/download/... (oeffentlich)
    long long size = 0;
};

struct Release {
    std::string tag;      // "v1.18.0-rev78"
    std::string name;     // "EffectsEd 1.18.0-rev78"
    std::string body;     // die Notizen (Markdown)
    std::string htmlUrl;  // die Seite des Releases
    std::vector<Asset> assets;
};

// Liest die Antwort von /releases/latest. false bei kaputtem JSON oder ohne
// tag_name; `error` bekommt dann die beste Auskunft (GitHub schickt bei
// Fehlern {"message": "..."}).
[[nodiscard]] bool parseRelease(const std::string& json, Release& out,
                                std::string* error = nullptr);

// Die Nummer hinter "rev": "v1.18.0-rev78" -> 78, "rev78" -> 78, sonst -1.
//
// Verglichen wird NUR sie, nicht die Fassung davor: sie zaehlt bei jedem
// Paket hoch (include/efx/version.h), die Fassung nicht.
[[nodiscard]] int revisionOf(const std::string& text);

// Ist `tag` (vom Server) neuer als `local`? Ohne erkennbare Nummer auf einer
// Seite: nein — lieber kein Update als ein falsches.
[[nodiscard]] bool isNewer(const std::string& tag, const std::string& local);

// Das erste .zip unter den Dateien des Releases, sonst nullptr.
[[nodiscard]] const Asset* zipAsset(const Release& release);

// Der gemeinsame oberste Ordner aller Eintraege ("efxed/"), oder leer.
[[nodiscard]] std::string commonFolder(const std::vector<std::string>& entries);

// Wohin ein Archiveintrag neben der .exe gehoert, relativ zum Programmordner.
// Ein gemeinsamer oberster Ordner wird abgestreift. Leer heisst ueberspringen:
// Ordner selbst, absolute Pfade, ".." — ein Archiv fasst nichts ausserhalb
// des Programmordners an. Ebenso die Dateien, die dem Anwender gehoeren
// (Einstellungen, Protokolle, der Schalter fuer den tragbaren Betrieb): ein
// Update ersetzt Programm, nicht Einstellungen.
[[nodiscard]] std::string targetInFolder(const std::string& entry,
                                         const std::string& topFolder);

}  // namespace efx::update
