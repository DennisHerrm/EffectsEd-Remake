#include "efx/layout.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace efx::layout {
namespace {

// Reihenfolge und Beschriftungen wie im Original. "10 units/foot (WARS)"
// stand dort an erster Stelle und hatte bei mir gefehlt — WARS ist Star Wars
// Jedi Knight, also genau das Spiel, um das es geht.
const WorldScale kScales[] = {
    {i18n::Str::Scale10, 10.0f},
    {i18n::Str::ScaleJka, 16.0f},
    {i18n::Str::Scale32, 32.0f},
    {i18n::Str::Scale48, 48.0f},
    {i18n::Str::Scale64, 64.0f},
    {i18n::Str::Scale8, 8.0f},
};

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

}  // namespace

const WorldScale* worldScales() { return kScales; }
int worldScaleCount() { return static_cast<int>(sizeof(kScales) / sizeof(kScales[0])); }

void Split::clampTo(float windowWidthPx, float windowHeightPx, float dpiScale) {
    const float minPanel = kMinPanelPx * dpiScale;
    const float minView = kMinViewPx * dpiScale;

    const float minProperties = kMinPropertiesPx * dpiScale;
    if (windowWidthPx > minProperties + minView) {
        float lowest = minProperties / windowWidthPx;
        float highest = 1.0f - minView / windowWidthPx;
        propertiesFraction = std::clamp(propertiesFraction, lowest, highest);
    } else if (windowWidthPx > minPanel + minView) {
        float lowest = minPanel / windowWidthPx;
        float highest = 1.0f - minView / windowWidthPx;
        propertiesFraction = std::clamp(propertiesFraction, lowest, highest);
    }

    if (windowHeightPx > minPanel + minView) {
        float lowest = minPanel / windowHeightPx;
        float highest = 1.0f - minView / windowHeightPx;
        listFraction = std::clamp(listFraction, lowest, highest);
    }
}

std::vector<std::string> Settings::allGamePaths() const {
    std::vector<std::string> out;
    if (!gamePath.empty()) out.push_back(gamePath);
    for (const auto& path : extraGamePaths) {
        if (path.empty()) continue;
        // Doppelte weglassen: sonst wird derselbe Ordner zweimal durchsucht,
        // und die Zaehlung im Dialog stimmt nicht mehr.
        bool known = false;
        for (const auto& seen : out) {
            if (seen == path) known = true;
        }
        if (!known) out.push_back(path);
    }
    return out;
}

std::string Settings::toIni() const {
    std::ostringstream out;
    out << "# EffectsEd — Einstellungen\n";
    out << "theme=" << themeId << "\n";
    out << "language=" << languageCode << "\n";
    out << "renderer=" << rendererCode << "\n";
    out << "splitProperties=" << split.propertiesFraction << "\n";
    out << "splitList=" << split.listFraction << "\n";
    out << "windowX=" << window.x << "\n";
    out << "windowY=" << window.y << "\n";
    out << "windowW=" << window.width << "\n";
    out << "windowH=" << window.height << "\n";
    out << "windowMaximized=" << (window.maximized ? 1 : 0) << "\n";
    out << "drawAxes=" << (drawAxes ? 1 : 0) << "\n";
    out << "drawRoom=" << (drawRoom ? 1 : 0) << "\n";
    out << "drawGrid=" << (drawGrid ? 1 : 0) << "\n";
    out << "legacyDrawOrder=" << (legacyDrawOrder ? 1 : 0) << "\n";
    out << "drawWindVector=" << (drawWindVector ? 1 : 0) << "\n";
    out << "playSounds=" << (playSounds ? 1 : 0) << "\n";
    out << "worldScale=" << worldScale << "\n";
    out << "timeScale=" << timeScale << "\n";
    out << "repeatRate=" << repeatRate << "\n";
    out << "repeat=" << (repeat ? 1 : 0) << "\n";
    out << "playbackMode=" << playbackMode << "\n";
    out << "playDuration=" << playDuration << "\n";
    out << "perFrameRespawn=" << (perFrameRespawn ? 1 : 0) << "\n";
    out << "animateSpawnPoint=" << (animateSpawnPoint ? 1 : 0) << "\n";
    out << "spawnVelocityX=" << spawnVelocity[0] << "\n";
    out << "spawnVelocityY=" << spawnVelocity[1] << "\n";
    out << "spawnVelocityZ=" << spawnVelocity[2] << "\n";
    out << "spawnResetSeconds=" << spawnResetSeconds << "\n";
    out << "orientation=" << orientation << "\n";
    out << "showStatusBar=" << (showStatusBar ? 1 : 0) << "\n";
    out << "showMainToolbar=" << (showMainToolbar ? 1 : 0) << "\n";
    out << "showEffectsToolbar=" << (showEffectsToolbar ? 1 : 0) << "\n";
    out << "showPlaybackToolbar=" << (showPlaybackToolbar ? 1 : 0) << "\n";
    out << "showWorldToolbar=" << (showWorldToolbar ? 1 : 0) << "\n";
    out << "resetRepeatRate=" << (resetRepeatRateOnStart ? 1 : 0) << "\n";
    out << "checkUpdates=" << (checkUpdates ? 1 : 0) << "\n";
    out << "openLibraryOnStart=" << (openLibraryOnStart ? 1 : 0) << "\n";
    out << "segmentRowHeight=" << segmentRowHeight << "\n";
    out << "preRoll=" << (preRoll ? 1 : 0) << "\n";
    out << "gridOnWalls=" << (gridOnWalls ? 1 : 0) << "\n";
    out << "roomTexture=" << roomTexture << "\n";
    out << "effectTextured=" << (effectTextured ? 1 : 0) << "\n";
    out << "effectWireframe=" << (effectWireframe ? 1 : 0) << "\n";
    out << "effectOverdraw=" << (effectOverdraw ? 1 : 0) << "\n";
    out << "gamePath=" << gamePath << "\n";
    // Je Pfad eine eigene Zeile: ein Trennzeichen waere in Windows-Pfaden
    // immer irgendwo enthalten.
    for (const auto& path : extraGamePaths) {
        out << "extraGamePath=" << path << "\n";
    }
    out << "windDirection=" << windDirection[0] << " " << windDirection[1]
        << " " << windDirection[2] << "\n";
    out << "windSpeed=" << windSpeed << "\n";
    out << "windFlagPos=" << windFlagPos[0] << " " << windFlagPos[1] << "\n";
    out << "roomStyle=" << roomStyle << "\n";
    // Ein Eintrag je Zeile. Pfade koennen alles enthalten ausser einem
    // Zeilenumbruch — ein Trennzeichen innerhalb der Zeile waere unsicher.
    for (const auto& path : recentFiles) {
        out << "recentFile=" << path << "\n";
    }
    out << "sunEnabled=" << (sunEnabled ? 1 : 0) << "\n";
    out << "sunDirection=" << sunDirection[0] << " " << sunDirection[1] << " "
        << sunDirection[2] << "\n";
    out << "sunAmbient=" << sunAmbient << "\n";
    out << "drawSunDisc=" << (drawSunDisc ? 1 : 0) << "\n";
    return out.str();
}

Settings Settings::fromIni(const std::string& text) {
    Settings s;
    bool sawPlaybackMode = false;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        // strtof liest auch "nan" und "inf". Ein NaN ueberlebt jede Pruefung
        // der Form "x <= 0" und machte etwa die Uhr dauerhaft NaN; dann
        // lieber 0, das die Pruefungen unten auf die Voreinstellung setzen.
        auto toFloat = [&] {
            const float parsed = std::strtof(value.c_str(), nullptr);
            return std::isfinite(parsed) ? parsed : 0.0f;
        };
        auto toInt = [&] { return std::atoi(value.c_str()); };
        auto toBool = [&] { return toInt() != 0; };

        if (key == "theme") s.themeId = value;
        else if (key == "language") s.languageCode = value;
        else if (key == "renderer") s.rendererCode = value;
        else if (key == "splitProperties") s.split.propertiesFraction = toFloat();
        else if (key == "splitList") s.split.listFraction = toFloat();
        else if (key == "windowX") { s.window.x = toInt(); s.window.valid = true; }
        else if (key == "windowY") s.window.y = toInt();
        else if (key == "windowW") s.window.width = toInt();
        else if (key == "windowH") s.window.height = toInt();
        else if (key == "windowMaximized") s.window.maximized = toBool();
        else if (key == "drawAxes") s.drawAxes = toBool();
        else if (key == "drawRoom") s.drawRoom = toBool();
        else if (key == "drawGrid") s.drawGrid = toBool();
        else if (key == "legacyDrawOrder") s.legacyDrawOrder = toBool();
        // Alter Name derselben Sache — Einstellungsdateien von frueher
        // sollen weiter gelten.
        else if (key == "drawWireframe") s.drawGrid = toBool();
        else if (key == "drawWindVector") s.drawWindVector = toBool();
        else if (key == "playSounds") s.playSounds = toBool();
        else if (key == "worldScale") s.worldScale = toFloat();
        else if (key == "timeScale") s.timeScale = toFloat();
        else if (key == "repeatRate") s.repeatRate = toFloat();
        else if (key == "repeat") s.repeat = toBool();
        else if (key == "playbackMode") { s.playbackMode = toInt(); sawPlaybackMode = true; }
        else if (key == "playDuration") s.playDuration = toFloat();
        else if (key == "perFrameRespawn") s.perFrameRespawn = toBool();
        else if (key == "animateSpawnPoint") s.animateSpawnPoint = toBool();
        else if (key == "spawnVelocityX") s.spawnVelocity[0] = toFloat();
        else if (key == "spawnVelocityY") s.spawnVelocity[1] = toFloat();
        else if (key == "spawnVelocityZ") s.spawnVelocity[2] = toFloat();
        else if (key == "spawnResetSeconds") s.spawnResetSeconds = toFloat();
        else if (key == "orientation") s.orientation = toInt();
        else if (key == "showStatusBar") s.showStatusBar = toBool();
        else if (key == "showMainToolbar") s.showMainToolbar = toBool();
        else if (key == "showEffectsToolbar") s.showEffectsToolbar = toBool();
        else if (key == "showPlaybackToolbar") s.showPlaybackToolbar = toBool();
        else if (key == "showWorldToolbar") s.showWorldToolbar = toBool();
        else if (key == "resetRepeatRate") s.resetRepeatRateOnStart = toBool();
        else if (key == "checkUpdates") s.checkUpdates = toBool();
        else if (key == "openLibraryOnStart") s.openLibraryOnStart = toBool();
        else if (key == "segmentRowHeight") s.segmentRowHeight = toFloat();
        else if (key == "preRoll") s.preRoll = toBool();
        else if (key == "gridOnWalls") s.gridOnWalls = toBool();
        else if (key == "roomTexture") s.roomTexture = toInt();
        else if (key == "effectRenderMode") {
            // Alte Einstellungsdateien: eine Art aus dreien.
            const int mode = toInt();
            s.effectTextured = mode == 0;
            s.effectWireframe = mode == 1;
            s.effectOverdraw = mode == 2;
        }
        else if (key == "effectTextured") s.effectTextured = toBool();
        else if (key == "effectWireframe") s.effectWireframe = toBool();
        else if (key == "effectOverdraw") s.effectOverdraw = toBool();
        else if (key == "gamePath") s.gamePath = value;
        else if (key == "extraGamePath") {
            if (!value.empty()) s.extraGamePaths.push_back(value);
        }
        else if (key == "windDirection") {
            std::istringstream parts(value);
            for (int i = 0; i < 3; ++i) parts >> s.windDirection[i];
        }
        else if (key == "windSpeed") s.windSpeed = toFloat();
        else if (key == "windFlagPos") {
            std::istringstream parts(value);
            parts >> s.windFlagPos[0] >> s.windFlagPos[1];
        }
        else if (key == "roomStyle") s.roomStyle = toInt();
        else if (key == "recentFile") {
            if (!value.empty() &&
                s.recentFiles.size() < Settings::kMaxRecentFiles) {
                s.recentFiles.push_back(value);
            }
        }
        else if (key == "sunEnabled") s.sunEnabled = toBool();
        else if (key == "sunAmbient") s.sunAmbient = toFloat();
        else if (key == "drawSunDisc") s.drawSunDisc = toBool();
        else if (key == "sunDirection") {
            std::istringstream parts(value);
            for (int i = 0; i < 3; ++i) parts >> s.sunDirection[i];
        }
        // Unbekannte Schluessel werden uebergangen: eine aeltere Fassung des
        // Programms soll eine neuere Einstellungsdatei lesen koennen, ohne zu
        // klagen.
    }

    // Gerettete Werte sind nicht zwangslaeufig vernuenftig. Ein minimiertes
    // Fenster wird als normal gespeichert, eine unsinnige Groesse verworfen —
    // dieselbe Vorsichtsmassnahme wie bei g2c.
    if (s.window.width < 640 || s.window.height < 480) {
        s.window.valid = false;
    }
    if (s.window.width > 32000 || s.window.height > 32000) {
        s.window.valid = false;
    }
    s.split.propertiesFraction = std::clamp(s.split.propertiesFraction, 0.05f, 0.8f);
    s.split.listFraction = std::clamp(s.split.listFraction, 0.05f, 0.8f);
    if (!(s.timeScale > 0.0f && s.timeScale <= 100.0f)) s.timeScale = 1.0f;
    if (s.worldScale <= 0.0f) s.worldScale = 16.0f;
    if (s.orientation < 0 || s.orientation > 2) s.orientation = 0;
    if (s.roomTexture < 0 || s.roomTexture > 3) s.roomTexture = 0;
    s.updateRenderMode();
    // Ein Nullvektor waere keine Richtung; dann lieber die Voreinstellung.
    if (s.windDirection[0] == 0.0f && s.windDirection[1] == 0.0f &&
        s.windDirection[2] == 0.0f) {
        s.windDirection[0] = 1.0f;
    }
    if (s.windSpeed <= 0.0f) s.windSpeed = 1.0f;
    // Eine unsinnige Stelle zurueckholen: eine Fahne zehn Kilometer neben dem
    // Raum ist unsichtbar, und man findet sie nicht wieder.
    for (float& value : s.windFlagPos) {
        if (!(value > -100000.0f && value < 100000.0f)) value = -32.0f;
    }
    if (s.roomStyle < 0 || s.roomStyle > 2) s.roomStyle = 0;
    // Aeltere Einstellungsdateien kennen nur "repeat" — ihre Wahl gilt.
    if (!sawPlaybackMode) s.playbackMode = s.repeat ? 1 : 0;
    if (s.playbackMode < 0 || s.playbackMode > 2) s.playbackMode = 1;
    if (!(s.playDuration >= 0.0f && s.playDuration <= 600.0f)) s.playDuration = 2.0f;
    if (!(s.spawnResetSeconds >= 0.0f && s.spawnResetSeconds <= 600.0f)) s.spawnResetSeconds = 1.0f;
    for (float& v : s.spawnVelocity) {
        if (!(v >= -4000.0f && v <= 4000.0f)) v = 0.0f;
    }
    if (s.sunAmbient < 0.0f || s.sunAmbient > 1.0f) s.sunAmbient = 0.35f;
    // Eine Sonne ohne Richtung waere keine — dann lieber die Voreinstellung.
    if (s.sunDirection[0] == 0.0f && s.sunDirection[1] == 0.0f &&
        s.sunDirection[2] == 0.0f) {
        s.sunDirection[2] = 1.0f;
    }

    return s;
}

void Settings::addRecentFile(const std::string& path) {
    if (path.empty()) return;

    // Schon dabei? Dann herausnehmen und vorn wieder einfuegen.
    for (size_t i = 0; i < recentFiles.size(); ++i) {
        if (recentFiles[i] == path) {
            recentFiles.erase(recentFiles.begin() + static_cast<ptrdiff_t>(i));
            break;
        }
    }
    recentFiles.insert(recentFiles.begin(), path);
    if (recentFiles.size() > kMaxRecentFiles) {
        recentFiles.resize(kMaxRecentFiles);
    }
}

}  // namespace efx::layout
