// Baseline-JPEG.
//
// Aufbau der Datei: eine Folge von Abschnitten, jeder mit einer Kennung
// (0xFF, dann ein Byte). Uns interessieren fuenf davon:
//
//   DQT  die Quantisierungstabellen — womit die DCT-Werte geteilt wurden
//   SOF0 Groesse, Anzahl der Kanaele, deren Unterabtastung
//   DHT  die Huffman-Tabellen
//   DRI  alle wieviel Bloecke ein Neustart kommt
//   SOS  ab hier die eigentlichen Bilddaten
//
// Der Weg eines Bildpunkts ist umgekehrt zur Kodierung:
//
//   Huffman auspacken -> 64 Koeffizienten
//   mit der Quantisierungstabelle multiplizieren
//   inverse DCT -> 8x8 Helligkeitswerte
//   Kanaele hochrechnen (Farbe liegt oft in halber Aufloesung)
//   YCbCr -> RGB
#include "efx/image.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace efx::image {
namespace {

// Die Reihenfolge, in der die 64 Koeffizienten in der Datei stehen: von der
// linken oberen Ecke im Zickzack nach rechts unten. Grobe Strukturen zuerst,
// feine zuletzt — deshalb kann man hinten abschneiden, und genau das macht
// die Kompression.
const int kZigZag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

struct HuffmanTable {
    unsigned char counts[17] = {};   // wie viele Codes je Laenge
    unsigned char symbols[256] = {};
    // Vorgerechnet, damit das Dekodieren nicht Bit fuer Bit suchen muss.
    int minCode[17] = {};
    int maxCode[17] = {};
    int valuePointer[17] = {};
    bool present = false;

    void build() {
        int code = 0, k = 0;
        for (int length = 1; length <= 16; ++length) {
            valuePointer[length] = k;
            minCode[length] = code;
            code += counts[length];
            k += counts[length];
            maxCode[length] = counts[length] ? code - 1 : -1;
            code <<= 1;
        }
        present = true;
    }
};

struct Component {
    int id = 0;
    int hSampling = 1, vSampling = 1;
    int quantTable = 0;
    int dcTable = 0, acTable = 0;
    int previousDc = 0;
    std::vector<unsigned char> pixels;  // volle Bildgroesse, hochgerechnet
    int blocksPerLine = 0, blocksPerColumn = 0;
    std::vector<unsigned char> raw;     // in eigener Aufloesung
    int rawWidth = 0, rawHeight = 0;
};

// Bitweise lesen, mit der JPEG-Eigenheit: ein 0xFF im Datenstrom wird als
// 0xFF 0x00 geschrieben, damit es nicht mit einer Kennung verwechselt wird.
// Beim Lesen muss die Null wieder verschwinden.
class BitReader {
public:
    BitReader(const unsigned char* data, size_t size, size_t position)
        : data_(data), size_(size), pos_(position) {}

    int bit() {
        if (bitCount_ == 0) {
            if (pos_ >= size_) { overrun_ = true; return 0; }
            current_ = data_[pos_++];
            if (current_ == 0xFF) {
                if (pos_ < size_ && data_[pos_] == 0x00) {
                    ++pos_;  // eingeschobene Null
                } else {
                    // Eine echte Kennung — der Abschnitt ist zu Ende.
                    overrun_ = true;
                    return 0;
                }
            }
            bitCount_ = 8;
        }
        --bitCount_;
        return (current_ >> bitCount_) & 1;
    }

    int bits(int count) {
        int value = 0;
        for (int i = 0; i < count; ++i) value = (value << 1) | bit();
        return value;
    }

    void alignAndSkipRestart() {
        // Nach einem Neustart beginnt ein neues Byte, und die Kennung
        // RST0..RST7 wird uebersprungen.
        bitCount_ = 0;
        while (pos_ + 1 < size_) {
            if (data_[pos_] == 0xFF && data_[pos_ + 1] >= 0xD0 &&
                data_[pos_ + 1] <= 0xD7) {
                pos_ += 2;
                return;
            }
            ++pos_;
        }
    }

    bool overrun() const { return overrun_; }
    size_t position() const { return pos_; }

private:
    const unsigned char* data_;
    size_t size_;
    size_t pos_;
    unsigned char current_ = 0;
    int bitCount_ = 0;
    bool overrun_ = false;
};

int decodeHuffman(BitReader& reader, const HuffmanTable& table) {
    int code = 0;
    for (int length = 1; length <= 16; ++length) {
        code = (code << 1) | reader.bit();
        if (reader.overrun()) return -1;
        if (table.maxCode[length] >= 0 && code <= table.maxCode[length]) {
            const int index = table.valuePointer[length] + code - table.minCode[length];
            if (index < 0 || index >= 256) return -1;
            return table.symbols[index];
        }
    }
    return -1;
}

// Die Zahlen stehen in einer eigenen Darstellung: `length` Bits, und der
// obere Halbbereich zaehlt negativ. Ohne diese Umrechnung sind alle Werte um
// die Haelfte daneben.
int extend(int value, int length) {
    if (length == 0) return 0;
    return value < (1 << (length - 1)) ? value - (1 << length) + 1 : value;
}

// Inverse DCT, getrennt in Zeilen und Spalten.
//
// Die einfache Fassung mit Kosinustabelle statt einer schnellen Variante:
// sie ist nachvollziehbar, und der Unterschied faellt beim Laden einer Textur
// nicht auf. Eine schnelle DCT einzubauen, ohne sie prüfen zu koennen, waere
// die schlechtere Wahl.
void inverseDct(const int* input, unsigned char* output, int stride) {
    // Einmal angelegt, ueber die Initialisierung einer lokalen statischen
    // Variable — die ist in C++ fadensicher. Vorher fuellten mehrere
    // Texturaufgaben dieselbe Tabelle gleichzeitig (ein "ready"-Merker ohne
    // Sperre): ein Datenwettlauf.
    struct Table {
        float value[8][8];
        Table() {
            for (int x = 0; x < 8; ++x) {
                for (int u = 0; u < 8; ++u) {
                    value[x][u] = std::cos((2.0f * x + 1.0f) * u * 3.14159265f / 16.0f);
                }
            }
        }
    };
    static const Table table;
    const auto& cosTable = table.value;
    auto scale = [](int u) { return u == 0 ? 0.70710678f : 1.0f; };

    float temp[64];
    // Erst die Zeilen, dann die Spalten — 8+8 statt 64 Durchlaeufen je Punkt.
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            float sum = 0.0f;
            for (int u = 0; u < 8; ++u) {
                sum += scale(u) * static_cast<float>(input[y * 8 + u]) * cosTable[x][u];
            }
            temp[y * 8 + x] = sum * 0.5f;
        }
    }
    for (int x = 0; x < 8; ++x) {
        for (int y = 0; y < 8; ++y) {
            float sum = 0.0f;
            for (int v = 0; v < 8; ++v) {
                sum += scale(v) * temp[v * 8 + x] * cosTable[y][v];
            }
            // +128, weil beim Kodieren 128 abgezogen wurde.
            const int value = static_cast<int>(std::lround(sum * 0.5f)) + 128;
            output[y * stride + x] =
                static_cast<unsigned char>(value < 0 ? 0 : (value > 255 ? 255 : value));
        }
    }
}

unsigned char clampByte(int v) {
    return static_cast<unsigned char>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

}  // namespace

Image decodeJpeg(const unsigned char* data, size_t size) {
    Image out;
    auto fail = [&](const char* why) {
        out.error = why;
        return out;
    };
    if (!data || size < 4) return fail("too small for a JPEG");
    if (!(data[0] == 0xFF && data[1] == 0xD8)) return fail("no JPEG start marker");

    unsigned short quant[4][64] = {};
    HuffmanTable dcTables[4], acTables[4];
    std::vector<Component> components;
    int width = 0, height = 0;
    int restartInterval = 0;
    size_t pos = 2;

    while (pos + 1 < size) {
        if (data[pos] != 0xFF) { ++pos; continue; }
        const unsigned char marker = data[pos + 1];
        pos += 2;
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            continue;  // ohne Nutzlast
        }
        if (marker == 0xD9) break;  // Bildende
        if (pos + 2 > size) return fail("truncated segment header");
        const size_t length =
            (static_cast<size_t>(data[pos]) << 8) | data[pos + 1];
        if (length < 2 || pos + length > size) return fail("bad segment length");
        const unsigned char* segment = data + pos + 2;
        const size_t segmentSize = length - 2;

        switch (marker) {
            case 0xC0:    // SOF0, Baseline
            case 0xC1: {  // SOF1, erweitert sequenziell — gleicher Aufbau
                if (segmentSize < 6) return fail("short SOF segment");
                height = (segment[1] << 8) | segment[2];
                width = (segment[3] << 8) | segment[4];
                const int count = segment[5];
                if (width <= 0 || height <= 0) return fail("bad image size");
                if (implausibleSize(width, height)) return fail("implausible size");
                if (count < 1 || count > 4) return fail("unsupported component count");
                if (segmentSize < 6 + static_cast<size_t>(count) * 3) {
                    return fail("short component list");
                }
                components.resize(static_cast<size_t>(count));
                for (int i = 0; i < count; ++i) {
                    const unsigned char* entry = segment + 6 + i * 3;
                    components[static_cast<size_t>(i)].id = entry[0];
                    components[static_cast<size_t>(i)].hSampling = entry[1] >> 4;
                    components[static_cast<size_t>(i)].vSampling = entry[1] & 0x0F;
                    components[static_cast<size_t>(i)].quantTable = entry[2] & 3;
                    if (components[static_cast<size_t>(i)].hSampling < 1 ||
                        components[static_cast<size_t>(i)].hSampling > 4 ||
                        components[static_cast<size_t>(i)].vSampling < 1 ||
                        components[static_cast<size_t>(i)].vSampling > 4) {
                        return fail("bad sampling factors");
                    }
                }
                break;
            }
            case 0xC2:
                return fail("progressive JPEG is not supported");
            case 0xC3: case 0xC5: case 0xC6: case 0xC7:
            case 0xC9: case 0xCA: case 0xCB:
            case 0xCD: case 0xCE: case 0xCF:
                return fail("unsupported JPEG encoding");
            case 0xC4: {  // DHT
                size_t at = 0;
                while (at + 17 <= segmentSize) {
                    const int id = segment[at] & 3;
                    const bool isAc = (segment[at] >> 4) != 0;
                    HuffmanTable& table = isAc ? acTables[id] : dcTables[id];
                    int total = 0;
                    for (int i = 1; i <= 16; ++i) {
                        table.counts[i] = segment[at + i];
                        total += table.counts[i];
                    }
                    if (total > 256 || at + 17 + static_cast<size_t>(total) >
                                           segmentSize) {
                        return fail("bad huffman table");
                    }
                    std::memcpy(table.symbols, segment + at + 17,
                                static_cast<size_t>(total));
                    table.build();
                    at += 17 + static_cast<size_t>(total);
                }
                break;
            }
            case 0xDB: {  // DQT
                size_t at = 0;
                while (at < segmentSize) {
                    const int id = segment[at] & 3;
                    const bool sixteenBit = (segment[at] >> 4) != 0;
                    ++at;
                    const size_t need = sixteenBit ? 128u : 64u;
                    if (at + need > segmentSize) return fail("short quant table");
                    for (size_t i = 0; i < 64; ++i) {
                        quant[id][kZigZag[i]] =
                            sixteenBit
                                ? static_cast<unsigned short>((segment[at + i * 2] << 8) |
                                                              segment[at + i * 2 + 1])
                                : segment[at + i];
                    }
                    at += need;
                }
                break;
            }
            case 0xDD:  // DRI
                if (segmentSize >= 2) {
                    restartInterval = (segment[0] << 8) | segment[1];
                }
                break;
            case 0xDA: {  // SOS — ab hier die Bilddaten
                if (components.empty()) return fail("scan before frame header");
                if (segmentSize < 1) return fail("short scan header");
                const int scanCount = segment[0];
                if (segmentSize < 1 + static_cast<size_t>(scanCount) * 2) {
                    return fail("short scan component list");
                }
                for (int i = 0; i < scanCount; ++i) {
                    const int id = segment[1 + i * 2];
                    const int tables = segment[2 + i * 2];
                    for (auto& c : components) {
                        if (c.id == id) {
                            c.dcTable = tables >> 4;
                            c.acTable = tables & 0x0F;
                        }
                    }
                }

                // Groesse in Bloecken. Ein MCU fasst so viele Bloecke, wie die
                // groesste Unterabtastung verlangt.
                int hMax = 1, vMax = 1;
                for (const auto& c : components) {
                    hMax = std::max(hMax, c.hSampling);
                    vMax = std::max(vMax, c.vSampling);
                }
                const int mcuWidth = 8 * hMax, mcuHeight = 8 * vMax;
                const int mcusPerLine = (width + mcuWidth - 1) / mcuWidth;
                const int mcusPerColumn = (height + mcuHeight - 1) / mcuHeight;

                for (auto& c : components) {
                    c.rawWidth = mcusPerLine * c.hSampling * 8;
                    c.rawHeight = mcusPerColumn * c.vSampling * 8;
                    c.raw.assign(static_cast<size_t>(c.rawWidth) * c.rawHeight, 128);
                    c.previousDc = 0;
                }

                BitReader reader(data, size, pos + length);
                int block[64];
                unsigned char samples[64];
                int mcuCount = 0;

                for (int mcuY = 0; mcuY < mcusPerColumn; ++mcuY) {
                    for (int mcuX = 0; mcuX < mcusPerLine; ++mcuX) {
                        if (restartInterval > 0 && mcuCount > 0 &&
                            mcuCount % restartInterval == 0) {
                            reader.alignAndSkipRestart();
                            for (auto& c : components) c.previousDc = 0;
                        }
                        ++mcuCount;

                        for (auto& c : components) {
                            for (int by = 0; by < c.vSampling; ++by) {
                                for (int bx = 0; bx < c.hSampling; ++bx) {
                                    std::memset(block, 0, sizeof(block));

                                    const HuffmanTable& dc = dcTables[c.dcTable & 3];
                                    const HuffmanTable& ac = acTables[c.acTable & 3];
                                    if (!dc.present || !ac.present) {
                                        return fail("scan uses a missing huffman table");
                                    }

                                    // Der Gleichanteil steht als Differenz zum
                                    // vorigen Block — deshalb previousDc.
                                    const int t = decodeHuffman(reader, dc);
                                    if (t < 0 || t > 15) {
                                        if (reader.overrun()) goto scanDone;
                                        return fail("bad DC code");
                                    }
                                    const int diff = t ? extend(reader.bits(t), t) : 0;
                                    c.previousDc += diff;
                                    block[0] = c.previousDc *
                                               quant[c.quantTable & 3][0];

                                    // Die Wechselanteile: je Eintrag erst die
                                    // Zahl der uebersprungenen Nullen, dann der
                                    // Wert.
                                    for (int k = 1; k < 64;) {
                                        const int rs = decodeHuffman(reader, ac);
                                        if (rs < 0) {
                                            if (reader.overrun()) goto scanDone;
                                            return fail("bad AC code");
                                        }
                                        const int run = rs >> 4;
                                        const int sizeBits = rs & 0x0F;
                                        if (sizeBits == 0) {
                                            if (run != 15) break;  // Blockende
                                            k += 16;
                                            continue;
                                        }
                                        k += run;
                                        if (k > 63) break;
                                        const int value =
                                            extend(reader.bits(sizeBits), sizeBits);
                                        block[kZigZag[k]] =
                                            value * quant[c.quantTable & 3][kZigZag[k]];
                                        ++k;
                                    }

                                    inverseDct(block, samples, 8);

                                    const int originX =
                                        (mcuX * c.hSampling + bx) * 8;
                                    const int originY =
                                        (mcuY * c.vSampling + by) * 8;
                                    for (int y = 0; y < 8; ++y) {
                                        const int targetY = originY + y;
                                        if (targetY >= c.rawHeight) break;
                                        for (int x = 0; x < 8; ++x) {
                                            const int targetX = originX + x;
                                            if (targetX >= c.rawWidth) break;
                                            c.raw[static_cast<size_t>(targetY) *
                                                      c.rawWidth + targetX] =
                                                samples[y * 8 + x];
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            scanDone:

                // Zusammensetzen. Farbkanaele liegen oft in halber Aufloesung
                // und werden hochgerechnet — schlicht durch Wiederholen, wie
                // es die meisten Decoder tun.
                out.rgba.assign(static_cast<size_t>(width) * height * 4, 255);
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        auto sampleOf = [&](const Component& c) {
                            const int sx = x * c.hSampling / hMax;
                            const int sy = y * c.vSampling / vMax;
                            const int cx = sx < c.rawWidth ? sx : c.rawWidth - 1;
                            const int cy = sy < c.rawHeight ? sy : c.rawHeight - 1;
                            return static_cast<int>(
                                c.raw[static_cast<size_t>(cy) * c.rawWidth + cx]);
                        };
                        const size_t at = (static_cast<size_t>(y) * width + x) * 4;

                        if (components.size() >= 3) {
                            const int Y = sampleOf(components[0]);
                            const int cb = sampleOf(components[1]) - 128;
                            const int cr = sampleOf(components[2]) - 128;
                            // JFIF-Umrechnung. Die Beiwerte stehen in der Norm.
                            out.rgba[at + 0] =
                                clampByte(static_cast<int>(std::lround(
                                    Y + 1.402f * cr)));
                            out.rgba[at + 1] = clampByte(static_cast<int>(std::lround(
                                Y - 0.344136f * cb - 0.714136f * cr)));
                            out.rgba[at + 2] =
                                clampByte(static_cast<int>(std::lround(
                                    Y + 1.772f * cb)));
                        } else {
                            const int grey = sampleOf(components[0]);
                            out.rgba[at + 0] = clampByte(grey);
                            out.rgba[at + 1] = clampByte(grey);
                            out.rgba[at + 2] = clampByte(grey);
                        }
                        out.rgba[at + 3] = 255;
                    }
                }
                out.width = width;
                out.height = height;
                out.hasAlpha = false;  // JPEG kennt keinen Alphakanal
                out.ok = true;
                return out;
            }
            default:
                break;  // APPn, COM und anderes ueberspringen
        }
        pos += length;
    }
    return fail("no image data found");
}

}  // namespace efx::image
