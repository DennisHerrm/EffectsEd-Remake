// Klangausgabe über waveOut.
//
// Bewusst die alte, einfache Windows-Schnittstelle statt WASAPI oder einer
// Bibliothek: sie ist seit Windows 3.1 unverändert, braucht kein COM, keine
// Initialisierung und keine zusätzliche Datei. Für ein paar Effektklänge
// gleichzeitig reicht sie vollkommen.
//
// **Achtung: dieser Teil ist ungeprüft.** Alles andere in diesem Projekt
// wurde gegen Referenzdaten gemessen, bevor es ausgeliefert wurde. Hier geht
// das nicht — es gibt keine Tonausgabe auf dem Rechner, auf dem der Quelltext
// entsteht. Der Klangleser (`efx::sound`) ist vollständig geprüft; nur das
// Abspielen ist es nicht.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace efx::gui {

class Audio {
public:
    // Alle fünf Sonderfunktionen ausdrücklich — die „Rule of Five".
    //
    // `Buffer` ist hier nur angekündigt, nicht ausdefiniert (er braucht
    // `windows.h`, das nicht in diesen Kopf gehört). Ein `unique_ptr` auf
    // einen unvollständigen Typ ist erlaubt, aber sein Aufräumen muss dort
    // stehen, wo der Typ vollständig ist — sonst schlägt der Übersetzer bei
    // jedem zu, der diesen Kopf einbindet.
    //
    // Kopieren ist verboten: ein Tongerät gibt es nur einmal, und zwei
    // Halter, die dieselben Puffer freigeben, sind ein doppeltes Freigeben.
    // Verschieben ist erlaubt und wird in der .cpp erzeugt.
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;
    Audio(Audio&&) noexcept;
    Audio& operator=(Audio&&) noexcept;

    // Prüft, ob das Format abspielbar ist, und merkt es sich. Geöffnet wird
    // je Klang ein eigenes Gerät, damit Windows die Klänge mischt statt sie
    // aneinanderzureihen. Ein zweiter Aufruf mit anderem Format beendet
    // alle laufenden Klänge.
    bool open(int sampleRate, int channels);
    void close();

    // Spielt einen Puffer ab. Kopiert ihn, weil waveOut asynchron liest und
    // der Aufrufer seinen Puffer sonst zu früh freigeben könnte.
    bool play(const std::vector<int16_t>& samples);

    // Fertige Puffer freigeben. Muss regelmäßig gerufen werden, sonst
    // sammeln sie sich an.
    void update();

    void stopAll();

    bool ready() const { return ready_; }
    const char* lastError() const { return error_; }
    int activeBuffers() const;

private:
    // Jeder Klang hat sein eigenes Geraet (siehe .cpp); hier steht nur das
    // geprüfte Format.
    bool ready_ = false;
    int sampleRate_ = 0;
    int channels_ = 0;
    const char* error_ = "";

    struct Buffer;
    // Besitzende Zeiger gehören in einen Halter, nicht in einen rohen Zeiger.
    //
    // Vorher stand hier `std::vector<Buffer*>` mit einem `new` und vier
    // `delete` — davon zwei auf Fehlerpfaden. Vergisst man eines, ist es ein
    // Leck; schreibt man eines zu viel, ein doppeltes Freigeben.
    //
    // Mit `unique_ptr` verschwindet die Frage: der Halter gibt frei, wenn er
    // aus dem Bereich fällt, auch wenn dazwischen etwas fehlschlägt.
    std::vector<std::unique_ptr<Buffer>> buffers_;
};

}  // namespace efx::gui
