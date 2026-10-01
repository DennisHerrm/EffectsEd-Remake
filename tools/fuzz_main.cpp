// Alle Zerleger gegen zufaellig verbogene Eingaben.
//
// Nicht "laeuft es durch", sondern: bricht irgendeiner aus seinem Puffer aus,
// haengt sich auf, oder belegt unbegrenzt Speicher? Das ist die Frage, die
// bei fremden Dateien zaehlt — eine .pk3 oder .efx kommt aus dem Netz.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "efx/io.h"
#include "efx/gp2.h"
#include "efx/shader.h"
#include "efx/image.h"
#include "efx/sound.h"
#include "efx/inflate.h"
#include "efx/layout.h"

static unsigned state = 20260808u;
static unsigned rnd() { state = state * 1664525u + 1013904223u; return state >> 8; }

static void randomRound(int rounds) {
    const unsigned char heads[][4] = {
        {0xFF,0xD8,0xFF,0xE0},                 // JPEG
        {0x89,'P','N','G'},                    // PNG
        {'R','I','F','F'},                     // WAV
        {0x00,0x00,0x02,0x00},                 // TGA-artig
        {'I','D','3',0x03},                    // MP3
        {0x78,0x9C,0,0},                       // zlib
    };
    for (int i = 0; i < rounds; ++i) {
        const size_t n = 16 + rnd() % 3000;
        std::vector<unsigned char> data(n);
        for (auto& b : data) b = (unsigned char)(rnd() & 0xFF);
        const auto& head = heads[rnd() % (sizeof(heads)/sizeof(heads[0]))];
        std::memcpy(data.data(), head, 4);
        // Plausible Groessen einsetzen, damit es tiefer kommt
        if (n > 24) { data[12] = 32; data[13] = 0; data[14] = 24; data[15] = 0; data[16] = 32; }

        (void)efx::image::decode(data.data(), data.size());
        (void)efx::sound::decode(data.data(), data.size());
        (void)efx::inflate::raw(data.data(), data.size(), 1 << 18);
        const std::string text(data.begin(), data.end());
        auto e = efx::read(text);
        (void)efx::write(e.effect);
    }
    std::printf("  %d Runden reiner Zufall ueberstanden\n", rounds);
}

int main(int argc, char** argv) {
    const int rounds = argc > 1 ? std::atoi(argv[1]) : 4000;
    // Ausgangsmaterial: echte Dateien, damit die Koepfe plausibel sind —
    // reiner Zufall wuerde meist schon an der Kennung scheitern.
    std::vector<std::vector<unsigned char>> seeds;
    for (const char* name : {"data/explosion.efx", "data/hitwall.efx",
                             "data/shot.efx"}) {
        FILE* f = std::fopen(name, "rb");
        if (!f) continue;
        std::vector<unsigned char> data;
        int c;
        while ((c = std::fgetc(f)) != EOF) data.push_back((unsigned char)c);
        std::fclose(f);
        seeds.push_back(data);
    }
    if (seeds.empty()) { std::printf("keine Beispieldateien\n"); return 1; }

    size_t worstOut = 0;
    for (int i = 0; i < rounds; ++i) {
        auto data = seeds[rnd() % seeds.size()];
        const int hits = 1 + (int)(rnd() % 20);
        for (int k = 0; k < hits && !data.empty(); ++k) {
            data[rnd() % data.size()] = (unsigned char)(rnd() & 0xFF);
        }
        if ((rnd() & 7) == 0 && data.size() > 8) data.resize(rnd() % data.size());

        const std::string text(data.begin(), data.end());
        auto effect = efx::read(text);
        worstOut = std::max(worstOut, effect.effect.primitives.size());
        (void)efx::validate(effect.effect, efx::Dialect::Both);
        (void)efx::write(effect.effect);

        efx::shader::Library lib;
        efx::shader::parseInto(lib, text, "fuzz");
        (void)efx::gp2::parse(text);
        (void)efx::layout::Settings::fromIni(text);
        (void)efx::image::decode(data.data(), data.size());
        (void)efx::sound::decode(data.data(), data.size());
        auto inf = efx::inflate::raw(data.data(), data.size(), 1 << 20);
        worstOut = std::max(worstOut, inf.data.size());
    }
    std::printf("  %d Runden ueberstanden, groesstes Ergebnis %zu\n", rounds, worstOut);
    randomRound(rounds);
    return 0;
}
