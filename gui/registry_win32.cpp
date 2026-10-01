// Spielordner aus der Windows-Registrierung lesen.
//
// Warum: die Ladenfassungen auf CD liegen dort, wohin man sie installiert
// hat. Eine Vorlage mit festem Pfad trifft das nur zufaellig — der Installer
// von LucasArts traegt den echten Ordner aber ein. Steam und GOG ebenso.
//
// Alle Schluessel werden ZWEIMAL versucht, einmal in der 64-Bit- und einmal
// in der 32-Bit-Sicht. Die Spiele sind 32-Bit-Programme; auf einem 64-Bit-
// Windows landen ihre Eintraege unter Wow6432Node, und ein 64-Bit-Programm
// wie unseres sieht sie ohne KEY_WOW64_32KEY schlicht nicht. Genau daran
// scheitern solche Suchen ueblicherweise.
#include "app.h"

#include <windows.h>

#include <string>
#include <utility>
#include <vector>

namespace efx::gui {
namespace {

// Einen Zeichenkettenwert lesen. Leer, wenn es ihn nicht gibt.
std::string readValue(HKEY root, const wchar_t* path, const wchar_t* name,
                      REGSAM view) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, path, 0, KEY_READ | view, &key) != ERROR_SUCCESS) {
        return {};
    }
    wchar_t buffer[MAX_PATH * 2] = {};
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    const LONG result =
        RegQueryValueExW(key, name, nullptr, &type,
                         reinterpret_cast<LPBYTE>(buffer), &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        return {};
    }

    const int needed = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0,
                                           nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, out.data(), needed, nullptr,
                        nullptr);
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    while (!out.empty() && (out.back() == '/' || out.back() == ' ')) out.pop_back();
    return out;
}

}  // namespace

std::vector<std::pair<std::string, std::string>> App::registryGamePaths() const {
    struct Candidate {
        const wchar_t* path;
        const wchar_t* value;
        const char* label;
        const char* suffix;   // was hinter dem eingetragenen Ordner fehlt
    };

    // Die Schluessel der Ladenfassung schreibt der LucasArts-Installer; die
    // beiden Deinstallations-Eintraege sind der Weg, ueber den Steam und GOG
    // sich finden lassen.
    static const Candidate kCandidates[] = {
        {L"SOFTWARE\\LucasArts Entertainment Company LLC\\Star Wars Jedi Knight "
         L"Jedi Academy\\v1.0",
         L"PATH", "Jedi Academy (CD, gefunden)", "/GameData/base"},
        {L"SOFTWARE\\LucasArts\\Star Wars Jedi Knight Jedi Academy\\v1.0",
         L"PATH", "Jedi Academy (CD, gefunden)", "/GameData/base"},
        {L"SOFTWARE\\LucasArts Entertainment Company LLC\\Star Wars JK II Jedi "
         L"Outcast\\v1.0",
         L"PATH", "Jedi Outcast (CD, gefunden)", "/GameData/base"},
        {L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 6020",
         L"InstallLocation", "Jedi Academy (Steam, gefunden)", "/GameData/base"},
        {L"SOFTWARE\\GOG.com\\Games\\1421404528",
         L"path", "Jedi Academy (GOG, gefunden)", "/GameData/base"},
    };

    std::vector<std::pair<std::string, std::string>> found;
    for (const auto& candidate : kCandidates) {
        for (const REGSAM view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) {
            std::string base =
                readValue(HKEY_LOCAL_MACHINE, candidate.path, candidate.value, view);
            if (base.empty()) {
                base = readValue(HKEY_CURRENT_USER, candidate.path, candidate.value,
                                 view);
            }
            if (base.empty()) continue;

            std::string full = base + candidate.suffix;
            bool known = false;
            for (const auto& seen : found) {
                if (seen.second == full) known = true;
            }
            if (!known) found.emplace_back(candidate.label, std::move(full));
            break;
        }
    }
    return found;
}

}  // namespace efx::gui
