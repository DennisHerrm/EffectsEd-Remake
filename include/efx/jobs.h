// Arbeitsverteilung auf mehrere Kerne.
//
// Der Grund, warum das hier eine eigene Einheit ist und nicht ein paar
// verstreute std::thread: es gibt genau eine Regel, die alles zusammenhält,
// und die ist nicht verhandelbar.
//
//     Dear ImGui ist nicht threadsicher. Kein einziger ImGui::-Aufruf darf
//     von einem Arbeitsfaden kommen.
//
// Das steht so in ocornut/imgui#221 und wird in #7234 bestätigt: ein Kontext
// gehört einem Faden. Wer es trotzdem mit einem Mutex versucht, bekommt
// Fenster, die mal erscheinen und mal nicht, oder Inhalte doppelt gezeichnet —
// weil ein Arbeitsfaden nicht weiß, ob gerade ein Bild aufgebaut wird.
//
// Deshalb: Arbeitsfäden rechnen und lesen Dateien. Ergebnisse gehen über
// postToMain() in eine Schlange, und der Hauptfaden holt sie einmal je Bild
// ab. Kein Sperren an der Oberfläche, keine halb aktualisierten Listen.
#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace efx::jobs {

// Wird an lange Arbeiten weitergereicht, damit sie sich abbrechen lassen.
//
// Nötig, nicht bequem: wer den Grundpfad wechselt, während der alte noch
// durchsucht wird, soll nicht warten müssen, bis der fertig ist — und das
// Ergebnis des alten Laufs darf die neue Liste nicht überschreiben.
class Cancellation {
public:
    void cancel() { flag_->store(true, std::memory_order_relaxed); }
    bool cancelled() const { return flag_->load(std::memory_order_relaxed); }

private:
    std::shared_ptr<std::atomic<bool>> flag_ =
        std::make_shared<std::atomic<bool>>(false);
};

// Fortschritt, den die Oberfläche anzeigen kann. Nur Zahlen, keine
// Zeichenketten je Schritt — sonst kostet das Melden mehr als die Arbeit.
struct Progress {
    std::atomic<size_t> done{0};
    std::atomic<size_t> total{0};

    float fraction() const {
        size_t t = total.load(std::memory_order_relaxed);
        return t == 0 ? 0.0f
                      : static_cast<float>(done.load(std::memory_order_relaxed)) /
                            static_cast<float>(t);
    }
};

class Pool {
public:
    // threadCount 0 heißt: so viele wie Kerne, minus einer für den
    // Hauptfaden. Der zeichnet und darf nicht mit den Rechenfäden um Kerne
    // streiten, sonst ruckelt das Fenster genau dann, wenn etwas passiert.
    explicit Pool(unsigned threadCount = 0);
    ~Pool();

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    unsigned threadCount() const { return threadCount_; }

    // Eine einzelne Aufgabe einreihen.
    void post(std::function<void()> task);

    // Eine Schleife über count Elemente aufteilen und warten, bis alle fertig
    // sind. Der aufrufende Faden hilft mit, statt untätig zu warten.
    //
    // grainSize ist die kleinste Portion, die sich zu verteilen lohnt. Zu
    // klein gewählt kostet das Verteilen mehr als die Arbeit; deshalb gibt es
    // hier keinen Standardwert, über den man nicht nachgedacht hat.
    void parallelFor(size_t count, size_t grainSize,
                     const std::function<void(size_t begin, size_t end)>& body,
                     Cancellation* cancel = nullptr, Progress* progress = nullptr);

    // Etwas in die Schlange des Hauptfadens legen. Der einzige erlaubte Weg,
    // ein Ergebnis aus einem Arbeitsfaden an die Oberfläche zu geben.
    void postToMain(std::function<void()> task);

    // Vom Hauptfaden einmal je Bild aufzurufen. Führt aus, was sich
    // angesammelt hat.
    //
    // maxMillis begrenzt die Zeit: fallen tausend Texturen gleichzeitig an,
    // sollen sie über mehrere Bilder verteilt ankommen statt in einem Ruck,
    // der als Hänger sichtbar wird. Gibt zurück, wie viele erledigt wurden.
    size_t drainMainQueue(int maxMillis = 4);

    // Wie viele Aufgaben warten noch auf den Hauptfaden?
    size_t pendingMainTasks() const;

    // Wartet, bis alle eingereihten Aufgaben durch sind. Nur für Tests und
    // fürs geordnete Beenden — im laufenden Betrieb wartet man nicht.
    void waitIdle();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    unsigned threadCount_ = 0;
};

// Die eine Pool-Instanz des Programms.
Pool& pool();

}  // namespace efx::jobs
