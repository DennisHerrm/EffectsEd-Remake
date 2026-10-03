#include "efx/jobs.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>

namespace efx::jobs {

struct Pool::Impl {
    std::vector<std::thread> workers;

    std::deque<std::function<void()>> queue;
    std::mutex queueMutex;
    std::condition_variable queueSignal;
    bool stopping = false;

    // Zähler statt "Schlange leer": eine Aufgabe, die gerade läuft, ist nicht
    // mehr in der Schlange, aber auch nicht fertig. Ohne diesen Zähler würde
    // waitIdle() zu früh zurückkehren.
    size_t outstanding = 0;
    std::condition_variable idleSignal;

    std::deque<std::function<void()>> mainQueue;
    mutable std::mutex mainMutex;

    // Für parallelFor. Bewusst HIER und nicht dort als lokale Variablen.
    //
    // Der Fehler, der das nötig machte: Sperre und Signal standen als lokale
    // Variablen in parallelFor. Der wartende Faden sah den Zähler auf null,
    // kehrte zurück — und ZERSTÖRTE beide, während der letzte Arbeitsfaden
    // noch mitten in `notify_all` steckte.
    //
    // Eine `condition_variable` zu zerstören, während ein anderer Faden sie
    // benachrichtigt, ist undefiniertes Verhalten. Unter glibc geht es
    // zufällig gut; MinGWs Umsetzung führt ein Betriebssystem-Handle mit,
    // und der Zugriff darauf nach der Zerstörung endete beim Anwender als
    //
    //     CRASH code 0xC0000008        (STATUS_INVALID_HANDLE)
    //
    // Genau deshalb stürzte NUR die MinGW-Fassung ab und die mit Visual
    // Studio gebaute nicht: deren Umsetzung benutzt SRWLOCK und
    // CONDITION_VARIABLE, also gar kein Handle.
    //
    // Als Mitglied des Verteilers leben sie so lange wie er selbst, und die
    // Frage stellt sich nicht mehr. Verschachtelte Aufrufe stören einander
    // nicht: jeder wartet auf SEINEN Zähler, geweckt wird notfalls einer zu
    // viel.
    std::mutex splitMutex;
    std::condition_variable splitSignal;

    // Eine Aufgabe ausfuehren, ohne dass eine Ausnahme den Faden verlaesst.
    //
    // Eine Ausnahme, die aus einem std::thread herausfaellt, ruft
    // std::terminate — das ganze Programm endet. Genau das geschah bei einem
    // 82-Byte-JPEG, das 2 GB anforderte (std::bad_alloc in der
    // Texturaufgabe): ein kaputtes Bild in einer heruntergeladenen .pk3
    // beendete den Editor beim Blaettern. Die Aufgabe gilt dann als erledigt,
    // ihr Ergebnis bleibt aus; wer darauf wartet, sieht "nicht geladen".
    static void runGuarded(const std::function<void()>& task) {
        try {
            task();
        } catch (...) {
            failedTasks.fetch_add(1, std::memory_order_relaxed);
        }
    }
    static inline std::atomic<unsigned> failedTasks{0};

    // Holt eine Aufgabe und führt sie aus. Gibt false zurück, wenn nichts da
    // war.
    bool runOne() {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (queue.empty()) return false;
            task = std::move(queue.front());
            queue.pop_front();
        }
        runGuarded(task);
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (--outstanding == 0) idleSignal.notify_all();
        }
        return true;
    }

    void workerLoop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueSignal.wait(lock, [this] { return stopping || !queue.empty(); });
                if (stopping && queue.empty()) return;
                task = std::move(queue.front());
                queue.pop_front();
            }
            runGuarded(task);
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (--outstanding == 0) idleSignal.notify_all();
            }
        }
    }
};

Pool::Pool(unsigned threadCount) : impl_(std::make_unique<Impl>()) {
    if (threadCount == 0) {
        unsigned cores = std::thread::hardware_concurrency();
        if (cores == 0) cores = 1;
        // Einer bleibt für den Hauptfaden. Auf einem Einkerner bleibt null —
        // dann läuft alles auf dem aufrufenden Faden, und parallelFor wird zur
        // gewöhnlichen Schleife. Das ist richtig so: ein Arbeitsfaden auf
        // einem Kern macht nichts schneller, kostet aber Umschaltungen.
        threadCount = cores > 1 ? cores - 1 : 0;
    }
    threadCount_ = threadCount;

    impl_->workers.reserve(threadCount);
    for (unsigned i = 0; i < threadCount; ++i) {
        impl_->workers.emplace_back([this] { impl_->workerLoop(); });
    }
}

Pool::~Pool() {
    {
        std::lock_guard<std::mutex> lock(impl_->queueMutex);
        impl_->stopping = true;
    }
    impl_->queueSignal.notify_all();
    for (auto& t : impl_->workers) {
        if (t.joinable()) t.join();
    }
}

void Pool::post(std::function<void()> task) {
    if (threadCount_ == 0) {
        Impl::runGuarded(task);  // auf einem Einkerner: hier, ebenso geschuetzt
        return;
    }
    {
        std::lock_guard<std::mutex> lock(impl_->queueMutex);
        impl_->queue.push_back(std::move(task));
        ++impl_->outstanding;
    }
    impl_->queueSignal.notify_one();
}

void Pool::parallelFor(size_t count, size_t grainSize,
                       const std::function<void(size_t, size_t)>& body,
                       Cancellation* cancel, Progress* progress) {
    if (count == 0) return;
    if (grainSize == 0) grainSize = 1;

    if (progress) {
        progress->total.store(count, std::memory_order_relaxed);
        progress->done.store(0, std::memory_order_relaxed);
    }

    // Lohnt sich die Verteilung überhaupt? Bei wenig Arbeit ist die Antwort
    // nein, und dann wird gar nicht erst eingereiht.
    const size_t chunks = (count + grainSize - 1) / grainSize;
    if (threadCount_ == 0 || chunks <= 1) {
        if (!cancel || !cancel->cancelled()) {
            body(0, count);
            if (progress) progress->done.store(count, std::memory_order_relaxed);
        }
        return;
    }

    // Gewartet wird auf ERLEDIGTE PORTIONEN, nicht auf die Helfer.
    //
    // Vorher zaehlte "running" die Helfer, und der aufrufende Faden wartete,
    // bis jeder eingereihte Helfer einmal drangekommen war — auch wenn er
    // selbst laengst alle Portionen erledigt hatte. Standen die Helfer hinter
    // fremder Arbeit (Texturen laden) in der Schlange, stand der Hauptfaden
    // mit: gemessen 583 ms neben 4 x 300 ms Laden, fuer eine leere Schleife.
    //
    // Jetzt liegt der Zaehler in einem geteilten Block, den auch ein spaeter
    // Helfer noch sicher anfassen kann: er holt sich eine Nummer >= chunks und
    // geht wieder. body/cancel/progress fasst er nur mit einer gueltigen
    // Nummer an — und solange es die gibt, wartet der Aufrufer noch.
    struct Shared {
        std::atomic<size_t> next{0};
        std::atomic<size_t> finished{0};
        // Die erste Ausnahme aus body. Sie darf weder einen Helfer beenden
        // (dann fehlte seine Portion, und der Aufrufer wartete ewig) noch den
        // Aufrufer vorzeitig verlassen (dann liefen Helfer mit einem body
        // weiter, den es nicht mehr gibt). Also: Portion als erledigt zaehlen,
        // Ausnahme merken, nach dem Warten im Aufrufer weiterwerfen.
        std::mutex errorMutex;
        std::exception_ptr error;
    };
    auto shared = std::make_shared<Shared>();
    Impl* impl = impl_.get();

    auto worker = [shared, chunks, grainSize, count, &body, cancel, progress, impl] {
        for (;;) {
            const size_t index = shared->next.fetch_add(1, std::memory_order_relaxed);
            if (index >= chunks) break;
            // Abgebrochen: die Portion zaehlt als erledigt, nur ohne Arbeit —
            // sonst wartete der Aufrufer auf sie.
            if (!cancel || !cancel->cancelled()) {
                const size_t begin = index * grainSize;
                const size_t end = std::min(begin + grainSize, count);
                try {
                    body(begin, end);
                } catch (...) {
                    std::lock_guard<std::mutex> lock(shared->errorMutex);
                    if (!shared->error) shared->error = std::current_exception();
                }
                if (progress) progress->done.fetch_add(end - begin, std::memory_order_relaxed);
            }
            if (shared->finished.fetch_add(1, std::memory_order_acq_rel) + 1 == chunks) {
                // Unter der Sperre benachrichtigen, damit der Wartende nicht
                // zwischen Zaehler und Signal hindurchschluepft. Sperre und
                // Signal gehoeren dem Verteiler und leben laenger als dieser
                // Aufruf.
                std::lock_guard<std::mutex> lock(impl->splitMutex);
                impl->splitSignal.notify_all();
            }
        }
    };

    // Nicht mehr Helfer als Portionen. Zehn Fäden auf drei Portionen anzusetzen
    // bringt sieben Umschaltungen und keinen Nutzen.
    const size_t helpers = std::min<size_t>(threadCount_, chunks - 1);
    for (size_t i = 0; i < helpers; ++i) post(worker);

    // Der aufrufende Faden arbeitet mit, statt zu warten. Auf einem Rechner
    // mit vier Kernen ist das ein Viertel mehr Durchsatz, das man sonst
    // verschenkt.
    worker();

    std::unique_lock<std::mutex> lock(impl_->splitMutex);
    impl_->splitSignal.wait(lock, [&] {
        return shared->finished.load(std::memory_order_acquire) == chunks;
    });
    lock.unlock();
    if (shared->error) std::rethrow_exception(shared->error);
}

void Pool::postToMain(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(impl_->mainMutex);
    impl_->mainQueue.push_back(std::move(task));
}

size_t Pool::drainMainQueue(int maxMillis) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(maxMillis);
    size_t executed = 0;

    for (;;) {
        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(impl_->mainMutex);
            if (impl_->mainQueue.empty()) break;
            task = std::move(impl_->mainQueue.front());
            impl_->mainQueue.pop_front();
        }
        // Außerhalb der Sperre ausführen: die Aufgabe darf ihrerseits etwas
        // einreihen, und mit gehaltener Sperre wäre das eine Verklemmung.
        task();
        ++executed;

        if (maxMillis >= 0 && std::chrono::steady_clock::now() >= deadline) break;
    }
    return executed;
}

size_t Pool::pendingMainTasks() const {
    std::lock_guard<std::mutex> lock(impl_->mainMutex);
    return impl_->mainQueue.size();
}

void Pool::waitIdle() {
    if (threadCount_ == 0) return;
    std::unique_lock<std::mutex> lock(impl_->queueMutex);
    impl_->idleSignal.wait(lock, [this] { return impl_->outstanding == 0; });
}

Pool& pool() {
    static Pool instance;
    return instance;
}

}  // namespace efx::jobs
