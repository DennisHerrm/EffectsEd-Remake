#include "efx/paths.h"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace efx::paths {
namespace {

std::string g_override;

// Der Ordner, in dem die Exe liegt. Nicht das Arbeitsverzeichnis: wer das
// Programm über eine Verknüpfung oder per Ziehen einer Datei startet, hat ein
// ganz anderes Arbeitsverzeichnis, und die Markierungsdatei für den
// mitnehmbaren Betrieb läge dann am falschen Ort.
//
// Unter Windows füllt main_win32.cpp das über GetModuleFileNameW; hier ist der
// Rückfall auf das Arbeitsverzeichnis, damit die Tests laufen.
std::string g_exeDir;

fs::path exeDirectory() {
    if (!g_exeDir.empty()) return fs::path(g_exeDir);
    std::error_code ec;
    fs::path here = fs::current_path(ec);
    return ec ? fs::path(".") : here;
}

const char* kPortableMarker = "efxed_portable.txt";
const char* kFolderName = "efxed";

}  // namespace

void setOverrideDirForTesting(const std::string& dir) { g_override = dir; }

bool isPortable() {
    if (!g_override.empty()) return false;
    std::error_code ec;
    return fs::exists(exeDirectory() / kPortableMarker, ec);
}

std::string configDir() {
    std::error_code ec;

    if (!g_override.empty()) {
        fs::create_directories(g_override, ec);
        return g_override;
    }

    if (isPortable()) return exeDirectory().string();

    // APPDATA unter Windows, HOME als Rückfall — Letzteres nur, damit die
    // Tests hier durchlaufen; unter Windows ist APPDATA immer gesetzt.
    const char* base = std::getenv("APPDATA");
    if (!base || !*base) base = std::getenv("HOME");
    if (!base || !*base) return exeDirectory().string();

    const fs::path dir = fs::path(base) / kFolderName;
    fs::create_directories(dir, ec);
    if (ec) return exeDirectory().string();
    return dir.string();
}

namespace {

// Einmalige Übernahme aus dem alten Ort. Wer eine frühere Fassung neben der
// Exe laufen hatte, verliert seine Einstellungen nicht.
std::string resolveWithMigration(const char* fileName) {
    const fs::path target = fs::path(configDir()) / fileName;

    std::error_code ec;
    if (!fs::exists(target, ec)) {
        const fs::path old = exeDirectory() / fileName;
        if (fs::exists(old, ec) && old != target) {
            fs::copy_file(old, target, ec);
        }
    }
    return target.string();
}

}  // namespace

std::string settingsPath() { return resolveWithMigration("efxed_settings.txt"); }

std::string imguiIniPath() {
    // Der Fensterzustand von Dear ImGui gehört neben die Einstellungen, nicht
    // ins Arbeitsverzeichnis. Sonst entstehen bei jedem Start aus einem
    // anderen Ordner neue Dateien, und die Spaltenbreiten sind jedes Mal weg.
    return (fs::path(configDir()) / "efxed_gui.ini").string();
}

std::string startupLogPath() {
    return (fs::path(configDir()) / "efxed_start.log").string();
}

}  // namespace efx::paths
