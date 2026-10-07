// Die Fassungsnummer. An genau einer Stelle.
//
// Anlass: sie stand an zwei Stellen — im Fenster und im Kopf des
// Ablaufprotokolls. Die Fassung wurde zwölfmal erhöht, aber nur an einer
// Stelle; das Protokoll meldete monatelang 0.9.1, während das Programm längst
// 1.0.0-rc4 war.
//
// Genau die Angabe, die man bei einem Fehlerbericht braucht, war die falsche.
#pragma once

#include <string>

namespace efx {
// 1.0.0 — die erste Fassung ohne Zusatz.
//
// Achtundachtzig Vorabfassungen, 4051 Prüfungen, zehn Prüfwerkzeuge. Was den
// Ausschlag gab, war nicht eine fehlende Funktion, sondern dass die letzten
// Fehler alle aus derselben Ecke kamen: Dinge, die nur unter einer der beiden
// Grafikschnittstellen kaputt waren, und Dinge, die an einer zweiten Stelle
// einzutragen vergessen wurden. Für beides gibt es jetzt Prüfer.
//
// Die nächste Nummer ist keine 1.0.1, sondern der Umstieg auf Dear ImGui
// 1.92 — der fasst beide Renderpfade an, und deshalb bekommt er eine Fassung,
// auf die man zurückkann.
inline constexpr const char* kVersion = "1.18.0";

// Die Revisionsnummer. Zaehlt bei JEDEM Paket hoch, auch wenn sich die
// Fassungsnummer nicht aendert.
//
// Wofuer: bei mehreren Paketen am selben Tag ist "1.0.7" allein nicht genug,
// um zu sagen, welches das neuere ist. Der Dateiname traegt sie deshalb mit —
// `efxed-1.0.8-rev42.zip` — und sie steht im Protokollkopf und im
// Ueber-Dialog. Wer einen Fehler meldet, nennt damit genau ein Paket.
//
// tools/bump_revision.py erhoeht sie; von Hand anzufassen ist sie nicht
// gedacht.
inline constexpr int kRevision = 80;

// "1.0.8-rev42" — fuer Dateinamen, Protokoll und Dialoge.
inline std::string versionWithRevision() {
    return std::string(kVersion) + "-rev" + std::to_string(kRevision);
}
}  // namespace efx
