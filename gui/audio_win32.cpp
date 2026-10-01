#include "audio_win32.h"

#include <windows.h>
#include <mmsystem.h>

#include <cstring>
#include <memory>
#include <utility>

namespace efx::gui {

struct Audio::Buffer {
    WAVEHDR header{};
    std::vector<int16_t> data;
};

// Hier ist `Buffer` vollstaendig — deshalb stehen die Sonderfunktionen in
// dieser Datei und nicht im Kopf.
Audio::Audio() = default;
Audio::~Audio() { close(); }
Audio::Audio(Audio&&) noexcept = default;
Audio& Audio::operator=(Audio&&) noexcept = default;

bool Audio::open(int sampleRate, int channels) {
    if (handle_ && sampleRate == sampleRate_ && channels == channels_) return true;
    close();
    if (sampleRate <= 0 || channels < 1 || channels > 2) {
        error_ = "unsupported audio format";
        return false;
    }

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = static_cast<WORD>(channels);
    format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(channels * 2);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    HWAVEOUT device = nullptr;
    const MMRESULT result =
        waveOutOpen(&device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        error_ = "waveOutOpen failed";
        return false;
    }
    handle_ = device;
    sampleRate_ = sampleRate;
    channels_ = channels;
    error_ = "";
    return true;
}

void Audio::close() {
    if (!handle_) return;
    auto device = static_cast<HWAVEOUT>(handle_);
    waveOutReset(device);
    for (const auto& buffer : buffers_) {
        waveOutUnprepareHeader(device, &buffer->header, sizeof(WAVEHDR));
    }
    buffers_.clear();
    waveOutClose(device);
    handle_ = nullptr;
    sampleRate_ = 0;
    channels_ = 0;
}

bool Audio::play(const std::vector<int16_t>& samples) {
    if (!handle_ || samples.empty()) return false;
    auto device = static_cast<HWAVEOUT>(handle_);

    // Erst aufraeumen, sonst waechst die Liste bei jedem Abspielen.
    update();

    // Eine Obergrenze gegen einen Effekt, der hundert Klaenge auf einmal
    // startet: waveOut nimmt sie alle an, und das Ergebnis ist Krach.
    constexpr size_t kMaxConcurrent = 16;
    if (buffers_.size() >= kMaxConcurrent) return false;

    auto buffer = std::make_unique<Buffer>();
    buffer->data = samples;  // kopieren: waveOut liest asynchron
    buffer->header.lpData = reinterpret_cast<LPSTR>(buffer->data.data());
    buffer->header.dwBufferLength =
        static_cast<DWORD>(buffer->data.size() * sizeof(int16_t));

    // Auf beiden Fehlerpfaden kein `delete` mehr: faellt `buffer` aus dem
    // Bereich, gibt der Halter frei. Genau dafuer ist er da.
    if (waveOutPrepareHeader(device, &buffer->header, sizeof(WAVEHDR)) !=
        MMSYSERR_NOERROR) {
        error_ = "waveOutPrepareHeader failed";
        return false;
    }
    if (waveOutWrite(device, &buffer->header, sizeof(WAVEHDR)) !=
        MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(device, &buffer->header, sizeof(WAVEHDR));
        error_ = "waveOutWrite failed";
        return false;
    }
    buffers_.push_back(std::move(buffer));
    return true;
}

void Audio::update() {
    if (!handle_) return;
    auto device = static_cast<HWAVEOUT>(handle_);
    for (size_t i = 0; i < buffers_.size();) {
        if (buffers_[i]->header.dwFlags & WHDR_DONE) {
            waveOutUnprepareHeader(device, &buffers_[i]->header, sizeof(WAVEHDR));
            buffers_.erase(buffers_.begin() + static_cast<ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

void Audio::stopAll() {
    if (!handle_) return;
    waveOutReset(static_cast<HWAVEOUT>(handle_));
    update();
}

int Audio::activeBuffers() const { return static_cast<int>(buffers_.size()); }

}  // namespace efx::gui
