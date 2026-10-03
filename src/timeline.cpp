#include "efx/timeline.h"

#include <cmath>

namespace efx::timeline {
namespace {

float clampRange(float value, float low, float high) {
    // NaN zaehlt als "zu klein". Vorher ging es durch beide Vergleiche
    // hindurch, und Zeitfaktor oder Bildrate blieben dauerhaft NaN.
    if (!(value >= low)) return low;
    return value > high ? high : value;
}

}  // namespace

void Clock::setDuration(float ms) {
    // Kuerzer als ein Bild ergibt keinen Sinn — und eine Dauer von null
    // wuerde jede Anteilsrechnung durch null teilen.
    durationMs_ = ms > 1.0f ? ms : 1.0f;
    if (timeMs_ > durationMs_) timeMs_ = durationMs_;
}

void Clock::play() {
    // Vom Ende aus neu beginnen: sonst drueckt man Abspielen und es passiert
    // nichts, weil die Uhr schon durch ist.
    if (timeMs_ >= durationMs_) timeMs_ = 0.0f;
    state_ = State::Playing;
}

void Clock::pause() {
    if (state_ == State::Playing) state_ = State::Paused;
}

void Clock::togglePause() {
    if (state_ == State::Playing) {
        state_ = State::Paused;
    } else {
        play();
    }
}

void Clock::stop() {
    state_ = State::Stopped;
    timeMs_ = 0.0f;
    wrapped_ = false;
}

float Clock::progress() const {
    return durationMs_ > 0.0f ? clampRange(timeMs_ / durationMs_, 0.0f, 1.0f) : 0.0f;
}

void Clock::setProgress(float value) {
    scrubTo(clampRange(value, 0.0f, 1.0f) * durationMs_);
}

void Clock::scrubTo(float ms) {
    timeMs_ = clampRange(ms, 0.0f, durationMs_);
    // Anhalten. Wer eine Stelle sucht, will sie ansehen — liefe die Uhr
    // weiter, waere sie beim Loslassen schon woanders.
    if (state_ == State::Playing) state_ = State::Paused;
}

void Clock::advance(float deltaMs) {
    if (state_ != State::Playing) return;
    if (!(deltaMs > 0.0f)) return;   // faengt auch NaN ab

    // Grosse Spruenge deckeln. Bleibt das Fenster eine Sekunde haengen — beim
    // Verschieben, beim Laden einer Textur —, soll der Effekt nicht eine
    // Sekunde weiterspringen, sondern dort fortsetzen, wo er war.
    constexpr float kMaxStepMs = 100.0f;
    if (deltaMs > kMaxStepMs) deltaMs = kMaxStepMs;

    timeMs_ += deltaMs * speed_;

    if (timeMs_ < durationMs_) return;

    switch (endMode_) {
        case EndMode::Repeat:
            // Den Ueberhang mitnehmen statt auf null zu setzen: sonst
            // ruckelt eine Wiederholung bei jedem Durchlauf um wenige
            // Millisekunden.
            timeMs_ = std::fmod(timeMs_, durationMs_);
            wrapped_ = true;
            break;
        case EndMode::Hold:
            timeMs_ = durationMs_;
            break;
        case EndMode::Stop:
            timeMs_ = durationMs_;
            state_ = State::Stopped;
            break;
    }
}

void Clock::stepFrames(int frames) {
    if (frameRate_ <= 0.0f) return;
    const float step = 1000.0f / frameRate_;
    // Anhalten: Bild fuer Bild und Abspielen zugleich ergaebe keinen Sinn.
    if (state_ == State::Playing) state_ = State::Paused;
    timeMs_ = clampRange(timeMs_ + static_cast<float>(frames) * step, 0.0f,
                         durationMs_);
}

void Clock::setSpeed(float value) {
    if (!std::isfinite(value)) return;  // NaN/inf: der bisherige Wert bleibt
    // Nach unten begrenzt, damit die Uhr nicht stehenbleibt und man glaubt,
    // das Programm haenge. Nach oben, weil darueber ohnehin nichts mehr zu
    // erkennen ist.
    speed_ = clampRange(value, 0.01f, 8.0f);
}

void Clock::setFrameRate(float fps) {
    if (!std::isfinite(fps)) return;  // NaN/inf: der bisherige Wert bleibt
    frameRate_ = clampRange(fps, 1.0f, 240.0f);
}

int Clock::currentFrame() const {
    return static_cast<int>(timeMs_ * frameRate_ / 1000.0f + 0.5f);
}

int Clock::totalFrames() const {
    return static_cast<int>(durationMs_ * frameRate_ / 1000.0f + 0.5f);
}

bool Clock::consumeWrapped() {
    const bool value = wrapped_;
    wrapped_ = false;
    return value;
}

}  // namespace efx::timeline
