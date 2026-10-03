#include "efx/paths.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace efx::paths {

fs::path fromUtf8(const std::string& utf8) {
    // u8string-Konstruktor: MSVC und libstdc++ lesen ihn beide als UTF-8.
    // Ungueltige Bytefolgen (etwa ein alter ANSI-Pfad aus einer Datei) duerfen
    // nicht werfen — dann eben byteweise wie bisher.
    try {
        return fs::path(std::u8string(utf8.begin(), utf8.end()));
    } catch (...) {
        return fs::path(utf8);
    }
}

std::string toUtf8(const fs::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

namespace {

std::string g_override;

// Der Ordner, in dem die Exe liegt. Nicht das Arbeitsverzeichnis: wer das
// Programm über eine Verknüpfung oder per Ziehen einer Datei startet, hat ein
// ganz anderes Arbeitsverzeichnis, und die Markierungsdatei für den
// mitnehmbaren Betrieb läge dann am falschen Ort.
//
// main_win32.cpp setzt ihn als Erstes (setExeDirectory, aus
// GetModuleFileNameW); hier ist der Rückfall auf das Arbeitsverzeichnis,
// damit die Tests laufen. Bis 03.10.2026 setzte main ihn NIE — der
// Kommentar behauptete es nur.
std::string g_exeDir;

fs::path exeDirectory() {
    if (!g_exeDir.empty()) return fromUtf8(g_exeDir);
    std::error_code ec;
    fs::path here = fs::current_path(ec);
    return ec ? fs::path(".") : here;
}

const char* kPortableMarker = "efxed_portable.txt";
const char* kFolderName = "efxed";

// APPDATA (Windows) bzw. HOME, als Pfad. Unter Windows ueber die breite
// Fassung: getenv liefert ANSI-Bytes, und "Jörg" kam als J\xF6rg an — die
// Windows-7-Fassung (libstdc++) warf darauf eine Ausnahme, noch bevor das
// Protokoll offen war.
fs::path userConfigBase() {
#ifdef _WIN32
    if (const wchar_t* base = _wgetenv(L"APPDATA"); base && *base) return fs::path(base);
    if (const wchar_t* home = _wgetenv(L"HOME"); home && *home) return fs::path(home);
#else
    if (const char* base = std::getenv("APPDATA"); base && *base) return fromUtf8(base);
    if (const char* home = std::getenv("HOME"); home && *home) return fromUtf8(home);
#endif
    return {};
}

}  // namespace

void setExeDirectory(const std::string& utf8) { g_exeDir = utf8; }

bool writeFileReplacing(const std::string& utf8Path, const void* data, size_t size) {
    const fs::path target = fromUtf8(utf8Path);
    fs::path temp = target;
    temp += ".tmp";
    std::error_code ec;
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        out.flush();
        if (!out) {
            out.close();
            fs::remove(temp, ec);
            return false;
        }
        out.close();
        if (out.fail()) {
            fs::remove(temp, ec);
            return false;
        }
    }
    // Ersetzt eine vorhandene Datei (unter Windows MoveFileExW mit
    // MOVEFILE_REPLACE_EXISTING).
    fs::rename(temp, target, ec);
    if (ec) {
        fs::remove(temp, ec);
        return false;
    }
    return true;
}

void setOverrideDirForTesting(const std::string& dir) { g_override = dir; }

bool isPortable() {
    if (!g_override.empty()) return false;
    std::error_code ec;
    return fs::exists(exeDirectory() / kPortableMarker, ec);
}

std::string configDir() {
    std::error_code ec;

    if (!g_override.empty()) {
        fs::create_directories(fromUtf8(g_override), ec);
        return g_override;
    }

    if (isPortable()) return toUtf8(exeDirectory());

    // APPDATA unter Windows, HOME als Rückfall — Letzteres nur, damit die
    // Tests hier durchlaufen; unter Windows ist APPDATA immer gesetzt.
    const fs::path base = userConfigBase();
    if (base.empty()) return toUtf8(exeDirectory());

    const fs::path dir = base / kFolderName;
    fs::create_directories(dir, ec);
    if (ec) return toUtf8(exeDirectory());
    return toUtf8(dir);
}

namespace {

// Einmalige Übernahme aus dem alten Ort. Wer eine frühere Fassung neben der
// Exe laufen hatte, verliert seine Einstellungen nicht.
std::string resolveWithMigration(const char* fileName) {
    const fs::path target = fromUtf8(configDir()) / fileName;

    std::error_code ec;
    if (!fs::exists(target, ec)) {
        const fs::path old = exeDirectory() / fileName;
        if (fs::exists(old, ec) && old != target) {
            fs::copy_file(old, target, ec);
        }
    }
    return toUtf8(target);
}

}  // namespace

std::string settingsPath() { return resolveWithMigration("efxed_settings.txt"); }

std::string imguiIniPath() {
    // Der Fensterzustand von Dear ImGui gehört neben die Einstellungen, nicht
    // ins Arbeitsverzeichnis. Sonst entstehen bei jedem Start aus einem
    // anderen Ordner neue Dateien, und die Spaltenbreiten sind jedes Mal weg.
    return toUtf8(fromUtf8(configDir()) / "efxed_gui.ini");
}

std::string startupLogPath() {
    return toUtf8(fromUtf8(configDir()) / "efxed_start.log");
}

}  // namespace efx::paths
