// efxtool — Kommandozeile fuer alles, was ohne Oberflaeche geht.
//
// Gedacht fuer den Stapelbetrieb: ein ganzes base/effects-Verzeichnis pruefen,
// bevor man es ausliefert, oder alle Dateien einmal verlustfrei neu schreiben.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "efx/io.h"
#include "efx/shader.h"
#include "efx/diag.h"
#include "efx/paths.h"

namespace {

std::string readFile(const std::filesystem::path& path, bool& ok) {
    std::ifstream in(path, std::ios::binary);
    ok = in.good();
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

const char* label(efx::Severity s) {
    switch (s) {
        case efx::Severity::Error: return "FEHLER ";
        case efx::Severity::Warning: return "Warnung";
        default: return "Hinweis";
    }
}

std::vector<std::filesystem::path> collect(const std::string& arg) {
    std::vector<std::filesystem::path> files;
    std::filesystem::path path(arg);
    if (std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".efx") {
                files.push_back(entry.path());
            }
        }
    } else {
        files.push_back(path);
    }
    return files;
}

void usage() {
    std::cout <<
        "efxtool — Werkzeug fuer Jedi-Academy-Effektdateien\n"
        "\n"
        "  efxtool check  <datei|ordner>...   Dateien pruefen\n"
        "  efxtool info   <datei>...          Aufbau anzeigen\n"
        "  efxtool format <datei|ordner>...   verlustfrei neu schreiben\n"
        "\n"
        "  efxtool log                        Ablaufprotokoll anzeigen\n"
        "\n"
        "Zusatz zu check:\n"
        "  --sp | --mp     nur diesen Zweig pruefen (Voreinstellung: beide)\n"
        "  --shaders <p>   .shader-Datei oder Ordner zum Gegenpruefen\n"
        "\n"
        "Zusatz zu format:\n"
        "  --raven    Zahlen wie der alte Editor (%1.4g), sonst verlustfrei\n"
        "  --dry      nur zeigen, was sich aendern wuerde\n";
}

int commandCheck(const std::vector<std::filesystem::path>& files,
                 efx::Dialect target, const efx::shader::Library* shaders) {
    int errors = 0;
    int warnings = 0;
    for (const auto& file : files) {
        bool ok = false;
        std::string text = readFile(file, ok);
        if (!ok) {
            std::cout << file.string() << ": nicht lesbar\n";
            ++errors;
            continue;
        }
        efx::ReadResult r = efx::read(text);
        std::vector<efx::Diagnostic> all = r.diagnostics;
        for (auto& d : efx::validate(r.effect, target)) all.push_back(d);
        if (shaders) {
            for (auto& d : efx::validateAgainstShaders(r.effect, *shaders)) {
                all.push_back(d);
            }
        }

        if (all.empty()) continue;
        std::cout << "\n" << file.string() << "\n";
        for (const auto& d : all) {
            std::cout << "  " << label(d.severity);
            if (d.line > 0) std::cout << " Zeile " << d.line;
            std::cout << ": " << d.message << "\n";
            if (d.severity == efx::Severity::Error) ++errors;
            if (d.severity == efx::Severity::Warning) ++warnings;
        }
    }
    std::cout << "\n" << files.size() << " Dateien, " << errors << " Fehler, "
              << warnings << " Warnungen\n";
    return errors == 0 ? 0 : 1;
}

int commandInfo(const std::vector<std::filesystem::path>& files) {
    for (const auto& file : files) {
        bool ok = false;
        std::string text = readFile(file, ok);
        if (!ok) continue;
        efx::ReadResult r = efx::read(text);
        std::cout << "\n" << file.filename().string() << "\n";
        if (r.effect.repeatDelaySet) {
            std::cout << "  repeatDelay " << r.effect.repeatDelay << "\n";
        }
        for (size_t i = 0; i < r.effect.primitives.size(); ++i) {
            const auto& p = r.effect.primitives[i];
            std::cout << "  " << (i + 1) << ". " << efx::typeName(p.type);
            if (!p.name.empty()) std::cout << " \"" << p.name << "\"";
            if (p.life.set) {
                std::cout << "  life " << p.life.min;
                if (p.life.ranged) std::cout << "-" << p.life.max;
            }
            if (p.count.set) {
                std::cout << "  count " << p.count.min;
                if (p.count.ranged) std::cout << "-" << p.count.max;
            }
            if (!p.shaders.empty()) {
                std::cout << "  " << p.shaders.size() << " Shader";
            }
            if (!p.sounds.empty()) {
                std::cout << "  " << p.sounds.size() << " Klang";
            }
            std::cout << "\n";
        }
    }
    return 0;
}

int commandFormat(const std::vector<std::filesystem::path>& files,
                  efx::NumberFormat numbers, bool dryRun) {
    int changed = 0;
    int failed = 0;
    for (const auto& file : files) {
        bool ok = false;
        std::string text = readFile(file, ok);
        if (!ok) {
            ++failed;
            continue;
        }
        efx::ReadResult r = efx::read(text);
        if (r.hasErrors()) {
            std::cout << file.string() << ": nicht fehlerfrei lesbar, uebersprungen\n";
            ++failed;
            continue;
        }
        efx::WriteOptions options;
        options.numbers = numbers;
        std::string written = efx::write(r.effect, options);

        // Sicherheitsnetz: nie etwas schreiben, das nicht wieder dasselbe ergibt.
        efx::ReadResult back = efx::read(written);
        if (back.hasErrors() ||
            back.effect.primitives.size() != r.effect.primitives.size()) {
            std::cout << file.string() << ": Umlauf fehlgeschlagen, nicht angefasst\n";
            ++failed;
            continue;
        }

        if (written == text) continue;
        ++changed;
        std::cout << (dryRun ? "wuerde aendern: " : "geaendert: ") << file.string()
                  << "\n";
        if (!dryRun) {
            std::ofstream out(file, std::ios::binary);
            out << written;
        }
    }
    std::cout << changed << " geaendert, " << failed << " uebersprungen\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "log") {
        for (const char* suffix : {"", ".vorher"}) {
            const std::string path = efx::paths::startupLogPath() + suffix;
            std::ifstream in(path, std::ios::binary);
            if (!in.good()) continue;
            std::cout << "=== " << path << " ===\n" << in.rdbuf() << "\n";
        }
        return 0;
    }
    if (argc < 3) {
        usage();
        return argc < 2 ? 0 : 1;
    }

    std::string command = argv[1];
    efx::NumberFormat numbers = efx::NumberFormat::Exact;
    bool dryRun = false;
    efx::Dialect target = efx::Dialect::Both;
    std::vector<std::filesystem::path> shaderPaths;
    std::vector<std::filesystem::path> files;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--raven") {
            numbers = efx::NumberFormat::Raven;
        } else if (arg == "--dry") {
            dryRun = true;
        } else if (arg == "--sp") {
            target = efx::Dialect::SP;
        } else if (arg == "--mp") {
            target = efx::Dialect::MP;
        } else if (arg == "--shaders" && i + 1 < argc) {
            std::filesystem::path sp(argv[++i]);
            if (std::filesystem::is_directory(sp)) {
                for (const auto& e : std::filesystem::recursive_directory_iterator(sp)) {
                    if (e.is_regular_file() && e.path().extension() == ".shader") {
                        shaderPaths.push_back(e.path());
                    }
                }
            } else {
                shaderPaths.push_back(sp);
            }
        } else {
            for (auto& f : collect(arg)) files.push_back(f);
        }
    }

    if (files.empty() && command != "log") {
        std::cout << "Keine .efx-Dateien gefunden.\n";
        return 1;
    }

    efx::shader::Library library;
    if (!shaderPaths.empty()) {
        for (const auto& sp : shaderPaths) {
            bool ok = false;
            std::string text = readFile(sp, ok);
            if (ok) efx::shader::parseInto(library, text, sp.filename().string());
        }
        std::cout << library.shaders.size() << " Shader aus " << shaderPaths.size()
                  << " Dateien geladen\n";
        for (const auto& d : library.diagnostics) {
            std::cout << "  " << label(d.severity) << " Zeile " << d.line << ": "
                      << d.message << "\n";
        }
    }

    if (command == "log") {
        // Nicht das eigene Protokoll: das der Oberflaeche. Wer nach einem
        // Absturz Hilfe sucht, hat die Oberflaeche nicht mehr, mit der er es
        // sich anzeigen lassen koennte.
        for (const char* suffix : {"", ".vorher"}) {
            const std::string path = efx::paths::startupLogPath() + suffix;
            std::ifstream in(path, std::ios::binary);
            if (!in.good()) continue;
            std::cout << "=== " << path << " ===\n" << in.rdbuf() << "\n";
        }
        return 0;
    }

    if (command == "check") {
        return commandCheck(files, target,
                            shaderPaths.empty() ? nullptr : &library);
    }
    if (command == "info") return commandInfo(files);
    if (command == "format") return commandFormat(files, numbers, dryRun);

    usage();
    return 1;
}
