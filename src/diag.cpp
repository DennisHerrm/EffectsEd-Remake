#include "efx/diag.h"
#include "efx/paths.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

namespace efx::diag {
namespace {

// Bewusst std::FILE und nicht std::ofstream. Bei ofstream steht zwischen
// unserem Schreiben und der Datei noch der Puffer der Standardbibliothek, und
// dessen Verhalten beim harten Programmende ist nicht zugesichert. Mit
// std::fflush wissen wir, dass die Zeile draußen ist.
std::FILE* g_file = nullptr;

std::vector<std::string> g_lines;

// Eine Sperre fuer alles hier. Geschrieben wird aus mehreren Faeden (die
// Update-Pruefung beim Start, Arbeitsfaeden), gelesen jedes Bild vom
// Protokollfenster. Ohne Sperre beschaedigte push_back den Heap — im Test
// stuerzte das in drei von drei Laeufen ab. Rekursiv, weil die oeffentlichen
// Funktionen einander und emit() aufrufen. Absichtlich nie zerstoert: ein
// Hintergrundfaden darf auch beim Beenden noch schreiben.
std::recursive_mutex& lock() {
    static auto* mutex = new std::recursive_mutex();
    return *mutex;
}
using Guard = std::lock_guard<std::recursive_mutex>;
std::vector<std::string> g_openSteps;
long long g_startMicros = 0;

long long nowMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string timestamp() {
    // Millisekunden seit Programmstart, nicht Uhrzeit. Beim Suchen nach einem
    // Absturz will man wissen, wie lange etwas gedauert hat, nicht wann es
    // war — und eine Uhrzeit sagt bei einer nach Sekunden abstürzenden
    // Anwendung ohnehin nichts.
    long long ms = (nowMicros() - g_startMicros) / 1000;
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "[%7lld ms] ", ms);
    return buffer;
}

const char* marker(Level level) {
    switch (level) {
        case Level::Warning: return "WARNING  ";
        case Level::Error: return "ERROR    ";
        case Level::Info: return "         ";
        default: return "         ";
    }
}

void emit(const std::string& line) {
    Guard guard(lock());
    g_lines.push_back(line);
    if (!g_file) return;
    std::fputs(line.c_str(), g_file);
    std::fputc('\n', g_file);
    // Die eine Zeile, auf die es ankommt, ist immer die letzte. Ohne dieses
    // fflush stünde sie beim Absturz noch im Puffer und wäre verloren —
    // genau die Zeile, wegen der das Protokoll existiert.
    std::fflush(g_file);
}

}  // namespace

bool open(const std::string& path) {
    Guard guard(lock());
    close();
    g_lines.clear();
    g_openSteps.clear();
    g_startMicros = nowMicros();

    std::error_code ec;
    // Das vorige Protokoll beiseitelegen statt überschreiben. Wer nach einem
    // Absturz das Programm noch einmal startet — und das tut jeder —, würde
    // sonst gerade den Beweis löschen.
    const fs::path file = paths::fromUtf8(path);
    fs::path previous = file;
    previous += ".vorher";
    if (fs::exists(file, ec)) {
        fs::rename(file, previous, ec);
        if (ec) fs::remove(file, ec);
    }

#ifdef _WIN32
    // _wfopen: der Pfad ist UTF-8, fopen laese ihn als ANSI-Codepage.
    g_file = _wfopen(file.c_str(), L"w");
#else
    g_file = std::fopen(path.c_str(), "w");
#endif
    return g_file != nullptr;
}

void close() {
    Guard guard(lock());
    if (g_file) {
        // Offene Schritte gehören vermerkt: wer hier noch offen ist, wurde
        // nie abgeschlossen, und das soll man nicht erst durch Zählen der
        // Einrückungen herausfinden.
        for (auto it = g_openSteps.rbegin(); it != g_openSteps.rend(); ++it) {
            emit(timestamp() + "ERROR    step never completed: " + *it);
        }
        emit(timestamp() + "         Clean shutdown.");
        std::fclose(g_file);
        g_file = nullptr;
    }
    g_openSteps.clear();
}

void write(Level level, const std::string& text) {
    Guard guard(lock());
    emit(timestamp() + marker(level) + std::string(g_openSteps.size() * 2, ' ') +
         text);
}

void writeHeader(const std::vector<std::pair<std::string, std::string>>& entries) {
    Guard guard(lock());
    emit("============================================================");
    emit(" EffectsEd - startup log");
    emit("============================================================");
    size_t width = 0;
    for (const auto& e : entries) width = std::max(width, e.first.size());
    for (const auto& e : entries) {
        emit("  " + e.first + std::string(width - e.first.size(), ' ') + " : " +
             e.second);
    }
    emit("============================================================");
}

Step::Step(std::string name) : name_(std::move(name)) {
    Guard guard(lock());
    // Erst vermerken, dann arbeiten. Andersherum stünde nichts im Protokoll,
    // wenn die Arbeit abstürzt — und das ist der einzige Fall, für den es da
    // ist.
    emit(timestamp() + "         " + std::string(g_openSteps.size() * 2, ' ') +
         "> " + name_);
    g_openSteps.push_back(name_);
    startMicros_ = nowMicros();
}

void Step::fail(const std::string& reason) { failure_ = reason; }

Step::~Step() {
    Guard guard(lock());
    if (!g_openSteps.empty()) g_openSteps.pop_back();
    long long ms = (nowMicros() - startMicros_) / 1000;

    char duration[32];
    std::snprintf(duration, sizeof(duration), " (%lld ms)", ms);

    if (failure_.empty()) {
        emit(timestamp() + "         " + std::string(g_openSteps.size() * 2, ' ') +
             "< " + name_ + duration);
    } else {
        emit(timestamp() + "FEHLER   " + std::string(g_openSteps.size() * 2, ' ') +
             "< " + name_ + duration + " -- " + failure_);
    }
}

std::string currentStep() {
    Guard guard(lock());
    return g_openSteps.empty() ? std::string("(no step active)")
                               : g_openSteps.back();
}

std::vector<std::string> lines() {
    // Eine Kopie: wer ueber eine Referenz liefe, waehrend ein anderer Faden
    // anhaengt, liefe ueber freigegebenen Speicher.
    Guard guard(lock());
    return g_lines;
}

void resetForTesting() {
    Guard guard(lock());
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
    g_lines.clear();
    g_openSteps.clear();
    g_startMicros = nowMicros();
}

}  // namespace efx::diag
