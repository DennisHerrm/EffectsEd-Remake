// Der Spielpfad.
//
// Nachbau von Dialog 170 aus EffectsEd.exe („Set Default Game Path"), aber
// ohne dessen SourceSafe-Kasten: Ravens interner Server steht seit zwanzig
// Jahren nicht mehr, und Visual SourceSafe ist eingestellt. Ein Feld, das
// niemand ausfüllen kann, ist keine Funktion.
//
// Was das Original nicht hatte und hier dazukommt: eine Prüfung, ob der
// angegebene Ordner tatsächlich wie ein `base`-Ordner aussieht. Ein Tippfehler
// im Pfad fiel dort erst auf, wenn Shader fehlten — und dann sucht man den
// Fehler beim Effekt statt beim Pfad.
//
// Die Prüfung steht hier und nicht in der Oberfläche, damit sie ohne Fenster
// geprüft werden kann.
#pragma once

#include <string>
#include <vector>

namespace efx::gamepath {

enum class Status {
    Empty,        // nichts eingetragen
    Missing,      // Ordner gibt es nicht
    NotBaseLike,  // Ordner da, sieht aber nicht nach base aus
    Ok,
};

struct Inspection {
    Status status = Status::Empty;

    // Was gefunden wurde. Die Zahlen stehen im Dialog, damit man sofort sieht,
    // ob man den richtigen Ordner erwischt hat.
    int pk3Count = 0;
    int shaderCount = 0;
    int efxCount = 0;
    bool hasShadersFolder = false;
    bool hasEffectsFolder = false;

    std::vector<std::string> notes;
};

// Sieht sich den Ordner an. Fasst nichts an, liest nur.
//
// Ein `base`-Ordner erkennt man an mindestens einem davon:
//   - .pk3-Dateien direkt darin (unausgepacktes Spiel)
//   - ein Unterordner `shaders` mit .shader-Dateien
//   - ein Unterordner `effects` mit .efx-Dateien
Inspection inspect(const std::string& path);

// Vorlagen wie im Original („Default options for JK2 / JA / SOF2"), aber mit
// Pfaden, die es heute gibt. Raven hatte dort `w:/game/base/` stehen — ein
// Netzlaufwerk im Studio.
struct Preset {
    const char* label;
    const char* path;
};
const std::vector<Preset>& presets();

// Trennt Schrägstriche auf eine Form. Die Engine akzeptiert beide, aber ein
// Vergleich zweier Pfade schlägt sonst fehl, obwohl sie dasselbe meinen.
std::string normalise(const std::string& path);

}  // namespace efx::gamepath
