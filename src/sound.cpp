#include "efx/sound.h"

#include <cmath>
#include <cstring>

#define MINIMP3_ONLY_MP3       // MP1 und MP2 brauchen wir nicht
#define MINIMP3_NO_SIMD        // damit das Ergebnis auf jedem Rechner gleich ist
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

namespace efx::sound {
namespace {

// Eine Obergrenze. Ein Effektklang ist ein paar Sekunden lang; eine Datei, die
// eine Stunde behauptet, ist entweder kaputt oder gehoert nicht hierher.
constexpr size_t kMaxSamples = 48000u * 2 * 600;  // zehn Minuten in Stereo

uint32_t readU32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t readU16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

int16_t clampSample(float value) {
    if (value > 32767.0f) return 32767;
    if (value < -32768.0f) return -32768;
    return static_cast<int16_t>(std::lround(value));
}

}  // namespace

size_t Sound::frameCount() const {
    return channels > 0 ? samples.size() / static_cast<size_t>(channels) : 0;
}

float Sound::durationMs() const {
    if (sampleRate <= 0) return 0.0f;
    return static_cast<float>(frameCount()) * 1000.0f /
           static_cast<float>(sampleRate);
}

Format sniff(const unsigned char* data, size_t size) {
    if (!data || size < 4) return Format::Unknown;
    if (std::memcmp(data, "RIFF", 4) == 0) return Format::Wav;

    // MP3 hat keine feste Kennung am Anfang. Zwei Faelle: ein ID3-Kopf, oder
    // gleich ein Rahmen, der mit elf gesetzten Bits beginnt.
    if (std::memcmp(data, "ID3", 3) == 0) return Format::Mp3;
    if (data[0] == 0xFF && (data[1] & 0xE0) == 0xE0) return Format::Mp3;
    return Format::Unknown;
}

Sound decodeWav(const unsigned char* data, size_t size) {
    Sound out;
    auto fail = [&](const std::string& why) {
        out.error = why;
        return out;
    };
    if (!data || size < 44) return fail("too small for a WAV file");
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return fail("not a RIFF/WAVE file");
    }

    // Die Abschnitte durchgehen. Reihenfolge und Anzahl sind nicht
    // festgelegt — wer fest annimmt, dass fmt bei Byte 12 und data bei 36
    // steht, scheitert an jeder Datei mit einem LIST-Abschnitt, und die sind
    // haeufig.
    size_t pos = 12;
    int formatTag = 0, channels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;
    const unsigned char* audio = nullptr;
    size_t audioSize = 0;

    while (pos + 8 <= size) {
        const char* id = reinterpret_cast<const char*>(data + pos);
        const uint32_t chunkSize = readU32(data + pos + 4);
        const size_t body = pos + 8;
        if (body + chunkSize > size) {
            // Abgeschnitten: nehmen, was da ist, statt alles zu verwerfen.
            if (std::memcmp(id, "data", 4) == 0 && body < size) {
                audio = data + body;
                audioSize = size - body;
            }
            break;
        }
        if (std::memcmp(id, "fmt ", 4) == 0 && chunkSize >= 16) {
            formatTag = readU16(data + body);
            channels = readU16(data + body + 2);
            sampleRate = readU32(data + body + 4);
            bitsPerSample = readU16(data + body + 14);
            // WAVE_FORMAT_EXTENSIBLE traegt die echte Art im Untertyp.
            if (formatTag == 0xFFFE && chunkSize >= 40) {
                formatTag = readU16(data + body + 24);
            }
        } else if (std::memcmp(id, "data", 4) == 0) {
            audio = data + body;
            audioSize = chunkSize;
        }
        // Abschnitte sind auf gerade Laenge aufgefuellt.
        pos = body + chunkSize + (chunkSize & 1u);
    }

    if (!audio || audioSize == 0) return fail("no audio data in the WAV file");
    if (channels < 1 || channels > 8) return fail("unsupported channel count");
    if (sampleRate < 1000 || sampleRate > 192000) return fail("implausible sample rate");

    constexpr int kPcm = 1, kFloat = 3;
    if (formatTag != kPcm && formatTag != kFloat) {
        return fail("compressed WAV files are not supported (format " +
                    std::to_string(formatTag) + ")");
    }

    const size_t bytesPerSample = static_cast<size_t>(bitsPerSample) / 8;
    if (bytesPerSample == 0) return fail("bad bit depth");
    const size_t count = audioSize / bytesPerSample;
    if (count > kMaxSamples) return fail("sound is implausibly long");

    out.samples.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const unsigned char* p = audio + i * bytesPerSample;
        switch (bitsPerSample) {
            case 8:
                // Acht Bit sind vorzeichenlos, mit 128 als Mitte — als
                // einzige Breite. Wer das uebersieht, bekommt ein lautes
                // Knacken und ein Signal, das nur die obere Haelfte nutzt.
                out.samples[i] = static_cast<int16_t>((static_cast<int>(p[0]) - 128)
                                                      * 256);
                break;
            case 16:
                out.samples[i] = static_cast<int16_t>(readU16(p));
                break;
            case 24: {
                const int32_t value = (static_cast<int32_t>(p[2]) << 24) |
                                      (static_cast<int32_t>(p[1]) << 16) |
                                      (static_cast<int32_t>(p[0]) << 8);
                out.samples[i] = static_cast<int16_t>(value >> 16);
                break;
            }
            case 32:
                if (formatTag == kFloat) {
                    float value;
                    std::memcpy(&value, p, 4);
                    out.samples[i] = clampSample(value * 32767.0f);
                } else {
                    const int32_t value = static_cast<int32_t>(readU32(p));
                    out.samples[i] = static_cast<int16_t>(value >> 16);
                }
                break;
            default:
                return fail("unsupported bit depth " + std::to_string(bitsPerSample));
        }
    }

    out.sampleRate = static_cast<int>(sampleRate);
    out.channels = channels;
    out.ok = true;
    return out;
}

Sound decodeMp3(const unsigned char* data, size_t size) {
    Sound out;
    if (!data || size < 4) {
        out.error = "too small for an MP3";
        return out;
    }

    mp3dec_t decoder;
    mp3dec_init(&decoder);

    size_t offset = 0;
    int16_t frame[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3dec_frame_info_t info{};
    int guard = 0;

    while (offset < size) {
        const int samples = mp3dec_decode_frame(
            &decoder, data + offset, static_cast<int>(size - offset), frame, &info);

        if (info.frame_bytes == 0) break;  // nichts mehr zu holen
        offset += static_cast<size_t>(info.frame_bytes);

        if (samples > 0) {
            if (out.channels == 0) {
                out.channels = info.channels;
                out.sampleRate = info.hz;
            }
            const size_t count = static_cast<size_t>(samples) *
                                 static_cast<size_t>(info.channels);
            if (out.samples.size() + count > kMaxSamples) {
                out.error = "sound is implausibly long";
                return out;
            }
            out.samples.insert(out.samples.end(), frame, frame + count);
        }
        // Eine Notbremse gegen Dateien, die den Decoder nicht vorankommen
        // lassen: ohne sie kann eine praeparierte Datei die Schleife ewig
        // laufen lassen.
        if (++guard > 200000) break;
    }

    if (out.samples.empty()) {
        out.error = out.error.empty() ? "no MP3 frames found" : out.error;
        return out;
    }
    out.ok = true;
    return out;
}

Sound decode(const unsigned char* data, size_t size) {
    switch (sniff(data, size)) {
        case Format::Wav: return decodeWav(data, size);
        case Format::Mp3: return decodeMp3(data, size);
        default: {
            Sound out;
            out.error = "unrecognised sound format";
            return out;
        }
    }
}

std::vector<int16_t> convert(const std::vector<int16_t>& samples,
                             int sourceRate, int sourceChannels,
                             int targetRate, int targetChannels) {
    std::vector<int16_t> out;
    if (samples.empty() || sourceRate <= 0 || targetRate <= 0 ||
        sourceChannels < 1 || targetChannels < 1) {
        return out;
    }

    const size_t frames = samples.size() / static_cast<size_t>(sourceChannels);
    if (frames == 0) return out;

    // Wie viele Bilder kommen heraus? Aufrunden, damit der letzte nicht
    // abgeschnitten wird — bei einem kurzen Aufschlag ist das hoerbar.
    const double ratio = static_cast<double>(targetRate) /
                         static_cast<double>(sourceRate);
    const auto outFrames = static_cast<size_t>(
        std::ceil(static_cast<double>(frames) * ratio));
    if (outFrames == 0) return out;

    out.resize(outFrames * static_cast<size_t>(targetChannels));

    // Einen Kanal an einer gebrochenen Stelle lesen, linear dazwischen.
    const auto sampleAt = [&](double frame, int channel) {
        const auto index = static_cast<size_t>(frame);
        const double fraction = frame - static_cast<double>(index);
        const size_t last = frames - 1;
        const size_t a = index < last ? index : last;
        const size_t b = a < last ? a + 1 : last;
        const int ch = channel < sourceChannels ? channel : sourceChannels - 1;
        const double first = samples[a * static_cast<size_t>(sourceChannels) +
                                     static_cast<size_t>(ch)];
        const double second = samples[b * static_cast<size_t>(sourceChannels) +
                                      static_cast<size_t>(ch)];
        return first + (second - first) * fraction;
    };

    for (size_t i = 0; i < outFrames; ++i) {
        const double source = static_cast<double>(i) / ratio;
        for (int c = 0; c < targetChannels; ++c) {
            double value;
            if (sourceChannels == targetChannels || targetChannels > sourceChannels) {
                // Gleich viele Kanaele, oder es kommen welche dazu: dann
                // bekommt jeder zusaetzliche denselben Inhalt. Mono auf zwei
                // Lautsprecher heisst mittig, und das ist richtig so.
                value = sampleAt(source, c);
            } else {
                // Weniger Kanaele: mitteln. Sonst faellt bei Stereo eine
                // Haelfte des Klangs weg.
                double sum = 0.0;
                for (int sc = 0; sc < sourceChannels; ++sc) {
                    sum += sampleAt(source, sc);
                }
                value = sum / static_cast<double>(sourceChannels);
            }
            out[i * static_cast<size_t>(targetChannels) +
                static_cast<size_t>(c)] = clampSample(static_cast<float>(value));
        }
    }
    return out;
}

}  // namespace efx::sound
