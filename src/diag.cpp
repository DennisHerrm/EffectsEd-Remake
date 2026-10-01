#include "efx/diag.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
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
    close();
    g_lines.clear();
    g_openSteps.clear();
    g_startMicros = nowMicros();

    std::error_code ec;
    // Das vorige Protokoll beiseitelegen statt überschreiben. Wer nach einem
    // Absturz das Programm noch einmal startet — und das tut jeder —, würde
    // sonst gerade den Beweis löschen.
    if (fs::exists(path, ec)) {
        fs::rename(path, path + ".vorher", ec);
        if (ec) fs::remove(path, ec);
    }

    g_file = std::fopen(path.c_str(), "w");
    return g_file != nullptr;
}

void close() {
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
    emit(timestamp() + marker(level) + std::string(g_openSteps.size() * 2, ' ') +
         text);
}

void writeHeader(const std::vector<std::pair<std::string, std::string>>& entries) {
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
    return g_openSteps.empty() ? std::string("(no step active)")
                               : g_openSteps.back();
}

const std::vector<std::string>& lines() { return g_lines; }

void resetForTesting() {
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
    g_lines.clear();
    g_openSteps.clear();
    g_startMicros = nowMicros();
}

}  // namespace efx::diag
