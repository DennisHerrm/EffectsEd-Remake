#include "efx/i18n.h"
#include "efx/gamepath.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace efx::gamepath {
namespace {

// Zaehlt Dateien mit einer Endung, hoert aber bei einer Obergrenze auf.
//
// Ein base-Ordner kann zehntausende Dateien haben, und der Dialog laeuft auf
// dem Hauptfaden — er soll eine Antwort geben, keine vollstaendige Statistik.
int countByExtension(const fs::path& dir, const char* extension, int limit) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return 0;

    int count = 0;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == extension) {
            if (++count >= limit) break;
        }
    }
    return count;
}

}  // namespace

std::string normalise(const std::string& path) {
    std::string out = path;
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    // Doppelte Trenner zusammenziehen, aber einen fuehrenden UNC-Pfad
    // (\\server\freigabe) nicht kaputtmachen.
    const bool unc = path.size() >= 2 && (path[0] == '\\' || path[0] == '/') &&
                     (path[1] == '\\' || path[1] == '/');
    std::string collapsed;
    collapsed.reserve(out.size());
    for (size_t i = 0; i < out.size(); ++i) {
        if (out[i] == '/' && !collapsed.empty() && collapsed.back() == '/') continue;
        collapsed += out[i];
    }
    if (unc) collapsed = "/" + collapsed;

    // Abschliessenden Trenner entfernen, ausser bei einem Laufwerksstamm.
    while (collapsed.size() > 1 && collapsed.back() == '/' &&
           !(collapsed.size() == 3 && collapsed[1] == ':')) {
        collapsed.pop_back();
    }
    return collapsed;
}

Inspection inspect(const std::string& rawPath) {
    Inspection result;
    if (rawPath.empty()) {
        result.status = Status::Empty;
        return result;
    }

    std::error_code ec;
    const fs::path dir(normalise(rawPath));
    if (!fs::is_directory(dir, ec)) {
        result.status = Status::Missing;
        return result;
    }

    result.pk3Count = countByExtension(dir, ".pk3", 999);

    const fs::path shaders = dir / "shaders";
    result.hasShadersFolder = fs::is_directory(shaders, ec);
    if (result.hasShadersFolder) {
        result.shaderCount = countByExtension(shaders, ".shader", 9999);
    }

    const fs::path effects = dir / "effects";
    result.hasEffectsFolder = fs::is_directory(effects, ec);
    if (result.hasEffectsFolder) {
        // Effekte liegen in Unterordnern, deshalb rekursiv — aber mit
        // Obergrenze, sonst laeuft der Dialog ueber ein Netzlaufwerk minutenlang.
        int count = 0;
        for (const auto& entry : fs::recursive_directory_iterator(effects, ec)) {
            if (ec) break;
            if (entry.is_regular_file(ec) && entry.path().extension() == ".efx") {
                if (++count >= 5000) break;
            }
        }
        result.efxCount = count;
    }

    const bool looksRight = result.pk3Count > 0 || result.shaderCount > 0 ||
                            result.efxCount > 0;
    result.status = looksRight ? Status::Ok : Status::NotBaseLike;

    if (!looksRight) {
        result.notes.push_back(
            i18n::tr(i18n::Str::PathNoAssets));
        // Der haeufigste Tippfehler: der GameData-Ordner statt base.
        if (fs::is_directory(dir / "base", ec)) {
            result.notes.push_back(
                "Es gibt hier einen Unterordner \"base\" — vermutlich ist der "
                "gemeint.");
        }
    }

    return result;
}

const std::vector<Preset>& presets() {
    // Das Original bot „Default options for JK2 / JA / SOF2" an und trug
    // w:/game/base/ ein — ein Netzlaufwerk in Ravens Studio. Hier stehen die
    // Pfade, an denen die Spiele heute tatsaechlich liegen.
    static const std::vector<Preset> kPresets = {
        {"Jedi Academy (Steam)",
         "C:/Program Files (x86)/Steam/steamapps/common/Jedi Academy/GameData/base"},
        {"Jedi Academy (GOG)",
         "C:/Program Files (x86)/GOG Galaxy/Games/Star Wars Jedi Knight - Jedi Academy/GameData/base"},
        {"Jedi Outcast (Steam)",
         "C:/Program Files (x86)/Steam/steamapps/common/Jedi Outcast/GameData/base"},
        {"Movie Duels",
         "C:/Program Files (x86)/Steam/steamapps/common/Jedi Academy/GameData/MovieDuels/base"},
        // Die eigenstaendige Movie-Duels-Fassung liegt NEBEN GameData und
        // bringt die Original-.pk3 von Jedi Academy in ihrem base mit; der Mod
        // selbst steht in MD (fs_game). Wie in der Engine gehoert MD vor base
        // — Rechtsklick auf die zweite Vorlage nimmt base dazu.
        {"Movie Duels (MD)",
         "C:/Program Files (x86)/Steam/steamapps/common/Jedi Academy/GameData Movie Duels/MD"},
        {"Movie Duels (base)",
         "C:/Program Files (x86)/Steam/steamapps/common/Jedi Academy/GameData Movie Duels/base"},

        // Die Ladenfassungen auf CD. Sie legen NICHT unter Steam oder GOG ab,
        // sondern dorthin, wohin der Installer von LucasArts schreibt.
        //
        // Der Ordneraufbau darunter ist bei allen dreien derselbe —
        // GameData/base mit den .pk3 — es ist wirklich nur der Pfad davor,
        // der sich unterscheidet. Fuer das Programm macht die Herkunft
        // keinen Unterschied.
        //
        // Wer nicht nach C:\Program Files installiert hat, findet seinen
        // Ordner ueber die Erkennung aus der Registrierung, die der Dialog
        // zusaetzlich anbietet.
        {"Jedi Academy (CD)",
         "C:/Program Files (x86)/LucasArts/Star Wars Jedi Knight Jedi Academy/GameData/base"},
        {"Jedi Academy (CD, 32 Bit)",
         "C:/Program Files/LucasArts/Star Wars Jedi Knight Jedi Academy/GameData/base"},
        {"Jedi Outcast (CD)",
         "C:/Program Files (x86)/LucasArts/Star Wars JK II Jedi Outcast/GameData/base"},
        {"Jedi Outcast (CD, 32 Bit)",
         "C:/Program Files/LucasArts/Star Wars JK II Jedi Outcast/GameData/base"},
    };
    return kPresets;
}

}  // namespace efx::gamepath
