#include "audio_win32.h"

#include <windows.h>
#include <mmsystem.h>

#include <cstring>
#include <memory>
#include <utility>

namespace efx::gui {

// Ein Klang = ein eigenes waveOut-Geraet.
//
// Vorher liefen alle Klaenge ueber EIN Geraet. waveOut mischt aber nicht, es
// reiht die Puffer eines Geraets aneinander: drei Klaenge zu 0,5 s endeten
// bei 0,5 / 1,0 / 1,5 s, ein Einschlag mit Knall und Splittern klang wie ein
// Echo (Fehlersuche 03.10.2026). Mehrere Geraete mischt Windows selbst.
struct Audio::Buffer {
    HWAVEOUT device = nullptr;
    WAVEHDR header{};
    std::vector<int16_t> data;

    // Ein Klang zu Ende (oder abgebrochen): Kopf freigeben, Geraet schliessen.
    void release() {
        if (!device) return;
        waveOutReset(device);
        waveOutUnprepareHeader(device, &header, sizeof(WAVEHDR));
        waveOutClose(device);
        device = nullptr;
    }
};

namespace {

WAVEFORMATEX formatFor(int sampleRate, int channels) {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = static_cast<WORD>(channels);
    format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(channels * 2);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    return format;
}

}  // namespace

// Hier ist `Buffer` vollstaendig — deshalb stehen die Sonderfunktionen in
// dieser Datei und nicht im Kopf.
Audio::Audio() = default;
Audio::~Audio() { close(); }

// Verschieben ausdruecklich: die vorgegebene Fassung kopierte den rohen
// Zustand, und beide Halter haetten dieselben Geraete geschlossen.
Audio::Audio(Audio&& other) noexcept
    : ready_(std::exchange(other.ready_, false)),
      sampleRate_(std::exchange(other.sampleRate_, 0)),
      channels_(std::exchange(other.channels_, 0)),
      error_(std::exchange(other.error_, "")),
      buffers_(std::move(other.buffers_)) {}

Audio& Audio::operator=(Audio&& other) noexcept {
    if (this != &other) {
        close();
        ready_ = std::exchange(other.ready_, false);
        sampleRate_ = std::exchange(other.sampleRate_, 0);
        channels_ = std::exchange(other.channels_, 0);
        error_ = std::exchange(other.error_, "");
        buffers_ = std::move(other.buffers_);
    }
    return *this;
}

bool Audio::open(int sampleRate, int channels) {
    if (ready_ && sampleRate == sampleRate_ && channels == channels_) return true;
    close();
    if (sampleRate <= 0 || channels < 1 || channels > 2) {
        error_ = "unsupported audio format";
        return false;
    }
    // Nur fragen, ob das Format geht — geoeffnet wird je Klang.
    const WAVEFORMATEX format = formatFor(sampleRate, channels);
    if (waveOutOpen(nullptr, WAVE_MAPPER, &format, 0, 0, WAVE_FORMAT_QUERY) != MMSYSERR_NOERROR) {
        error_ = "waveOutOpen failed";
        return false;
    }
    ready_ = true;
    sampleRate_ = sampleRate;
    channels_ = channels;
    error_ = "";
    return true;
}

void Audio::close() {
    for (const auto& buffer : buffers_) buffer->release();
    buffers_.clear();
    ready_ = false;
    sampleRate_ = 0;
    channels_ = 0;
}

bool Audio::play(const std::vector<int16_t>& samples) {
    if (!ready_ || samples.empty()) return false;

    // Erst aufraeumen, sonst waechst die Liste bei jedem Abspielen.
    update();

    // Eine Obergrenze gegen einen Effekt, der hundert Klaenge auf einmal
    // startet: das Ergebnis waere Krach, und jedes Geraet kostet etwas.
    constexpr size_t kMaxConcurrent = 16;
    if (buffers_.size() >= kMaxConcurrent) return false;

    auto buffer = std::make_unique<Buffer>();
    const WAVEFORMATEX format = formatFor(sampleRate_, channels_);
    if (waveOutOpen(&buffer->device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        buffer->device = nullptr;
        error_ = "waveOutOpen failed";
        return false;
    }
    buffer->data = samples;  // kopieren: waveOut liest asynchron
    buffer->header.lpData = reinterpret_cast<LPSTR>(buffer->data.data());
    buffer->header.dwBufferLength = static_cast<DWORD>(buffer->data.size() * sizeof(int16_t));

    if (waveOutPrepareHeader(buffer->device, &buffer->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
        waveOutClose(buffer->device);
        error_ = "waveOutPrepareHeader failed";
        return false;
    }
    if (waveOutWrite(buffer->device, &buffer->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(buffer->device, &buffer->header, sizeof(WAVEHDR));
        waveOutClose(buffer->device);
        error_ = "waveOutWrite failed";
        return false;
    }
    buffers_.push_back(std::move(buffer));
    return true;
}

void Audio::update() {
    for (size_t i = 0; i < buffers_.size();) {
        if (buffers_[i]->header.dwFlags & WHDR_DONE) {
            buffers_[i]->release();
            buffers_.erase(buffers_.begin() + static_cast<ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

void Audio::stopAll() {
    for (const auto& buffer : buffers_) buffer->release();
    buffers_.clear();
}

int Audio::activeBuffers() const { return static_cast<int>(buffers_.size()); }

}  // namespace efx::gui
