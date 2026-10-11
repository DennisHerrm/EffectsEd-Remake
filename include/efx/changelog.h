// Die Aenderungsliste: was in jeder Fassung neu ist, in allen Sprachen der
// Oberflaeche.
//
// Steht im Programm, nicht in einer Datei daneben: das Fenster soll auch
// funktionieren, wenn jemand nur die nackte .exe herunterlaedt (seit rev80
// liegen die .exe einzeln im Release).
//
// Pflege: bei JEDER neuen Revision oben einen Eintrag anlegen, in allen vier
// Sprachen mit gleich vielen Stichpunkten. testChangelog prueft das — ein
// Release ohne Eintrag faellt dort durch.
#pragma once

#include <string>
#include <vector>

#include "efx/i18n.h"

namespace efx::changelog {

struct Entry {
    int revision = 0;
    const char* date = "";  // JJJJ-MM-TT
    // Reihenfolge wie i18n::Language (Englisch, Deutsch, Chinesisch,
    // Japanisch). Stichpunkte durch '\n' getrennt.
    const char* text[4] = {"", "", "", ""};
};

// Alle Fassungen, die neueste zuerst.
const std::vector<Entry>& entries();

// Die Stichpunkte einer Fassung in einer Sprache.
std::vector<std::string> points(const Entry& entry, i18n::Language language);

// Nach einem Update einmal von selbst zeigen.
//
// Nur, wenn es schon Einstellungen gab — bei einer Neuinstallation gibt es
// kein "neu gegenueber vorher" — und die zuletzt gezeigte Fassung aelter ist.
// Fassungen vor rev81 kannten die Einstellung nicht; dort steht 0, und das
// zaehlt als aelter.
bool showAfterStart(bool settingsExisted, int lastShownRevision, int currentRevision);

}  // namespace efx::changelog
