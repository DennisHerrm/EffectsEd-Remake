#include <cmath>
#include "efx/shader.h"

#include <cctype>
#include <cstdlib>

namespace efx::shader {
namespace {

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

// Shaderdateien werden zeilenweise in Woerter zerlegt. Kommentare sind "//",
// Bloecke stehen in geschweiften Klammern, die auch allein auf einer Zeile
// stehen duerfen.
class Tokenizer {
public:
    Tokenizer(std::string_view text) : text_(text) {}

    // Naechstes Wort. Leerer Rueckgabewert heisst Dateiende.
    std::string next() {
        for (;;) {
            skipSpace();
            if (pos_ >= text_.size()) return {};
            if (text_[pos_] == '/' && pos_ + 1 < text_.size() &&
                text_[pos_ + 1] == '/') {
                while (pos_ < text_.size() && text_[pos_] != '\n') ++pos_;
                continue;
            }
            if (text_[pos_] == '/' && pos_ + 1 < text_.size() &&
                text_[pos_ + 1] == '*') {
                pos_ += 2;
                while (pos_ + 1 < text_.size() &&
                       !(text_[pos_] == '*' && text_[pos_ + 1] == '/')) {
                    if (text_[pos_] == '\n') ++line_;
                    ++pos_;
                }
                pos_ = pos_ + 2 <= text_.size() ? pos_ + 2 : text_.size();
                continue;
            }
            break;
        }
        return readWord();
    }

    // Rest der aktuellen Zeile, in Woerter zerlegt.
    std::vector<std::string> restOfLine() {
        std::vector<std::string> words;
        for (;;) {
            size_t save = pos_;
            int saveLine = line_;
            // Nur Leerzeichen und Tabs ueberspringen, kein Zeilenumbruch.
            while (pos_ < text_.size() &&
                   (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                    text_[pos_] == '\r')) {
                ++pos_;
            }
            if (pos_ >= text_.size() || text_[pos_] == '\n') return words;
            if (text_[pos_] == '/' && pos_ + 1 < text_.size() &&
                text_[pos_ + 1] == '/') {
                while (pos_ < text_.size() && text_[pos_] != '\n') ++pos_;
                return words;
            }
            // /* ... */ mitten in der Zeile ist ein Kommentar, keine Woerter.
            // Vorher zaehlte "animMap 10 a b /* alt: c */" vier Bilder mehr.
            if (text_[pos_] == '/' && pos_ + 1 < text_.size() &&
                text_[pos_ + 1] == '*') {
                pos_ += 2;
                while (pos_ + 1 < text_.size() &&
                       !(text_[pos_] == '*' && text_[pos_ + 1] == '/')) {
                    if (text_[pos_] == '\n') ++line_;
                    ++pos_;
                }
                pos_ = pos_ + 2 <= text_.size() ? pos_ + 2 : text_.size();
                continue;
            }
            // Eine Klammer gehoert zum Aufbau, nicht zum Wert davor: in
            // "{ map gfx/a.tga }" schliesst die } die Stufe. Vorher wurde sie
            // Teil des Werts, die Stufe blieb offen, und der NAECHSTE Shader
            // landete als zweite Stufe in diesem.
            if (text_[pos_] == '{' || text_[pos_] == '}') {
                pos_ = save;
                line_ = saveLine;
                return words;
            }
            const size_t start = pos_;
            std::string word = readWord();
            if (pos_ == start) {
                pos_ = save;
                line_ = saveLine;
                return words;
            }
            words.push_back(std::move(word));
        }
    }

    int line() const { return line_; }

private:
    bool isSpace(char c) const {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }
    // Ein Wort ab pos_. In Anfuehrungszeichen bis zum schliessenden, OHNE die
    // Zeichen — wie COM_ParseExt der Engine. Vorher blieben sie stehen, und
    // "gfx/q" wurde als Shader \"gfx/q\" gefuehrt, den niemand findet.
    std::string readWord() {
        if (pos_ < text_.size() && text_[pos_] == '"') {
            const size_t start = ++pos_;
            while (pos_ < text_.size() && text_[pos_] != '"' && text_[pos_] != '\n') ++pos_;
            std::string word(text_.substr(start, pos_ - start));
            if (pos_ < text_.size() && text_[pos_] == '"') ++pos_;
            return word;
        }
        const size_t start = pos_;
        while (pos_ < text_.size() && !isSpace(text_[pos_])) ++pos_;
        return std::string(text_.substr(start, pos_ - start));
    }
    void skipSpace() {
        while (pos_ < text_.size() && isSpace(text_[pos_])) {
            if (text_[pos_] == '\n') ++line_;
            ++pos_;
        }
    }

    std::string_view text_;
    size_t pos_ = 0;
    int line_ = 1;
};

float toFloat(const std::string& s) { return std::strtof(s.c_str(), nullptr); }

BlendFactor blendFactor(const std::string& word) {
    if (iequals(word, "GL_ONE")) return BlendFactor::One;
    if (iequals(word, "GL_ZERO")) return BlendFactor::Zero;
    if (iequals(word, "GL_SRC_COLOR")) return BlendFactor::SrcColor;
    if (iequals(word, "GL_ONE_MINUS_SRC_COLOR")) return BlendFactor::OneMinusSrcColor;
    if (iequals(word, "GL_DST_COLOR")) return BlendFactor::DstColor;
    if (iequals(word, "GL_ONE_MINUS_DST_COLOR")) return BlendFactor::OneMinusDstColor;
    if (iequals(word, "GL_SRC_ALPHA")) return BlendFactor::SrcAlpha;
    if (iequals(word, "GL_ONE_MINUS_SRC_ALPHA")) return BlendFactor::OneMinusSrcAlpha;
    if (iequals(word, "GL_DST_ALPHA")) return BlendFactor::DstAlpha;
    if (iequals(word, "GL_ONE_MINUS_DST_ALPHA")) return BlendFactor::OneMinusDstAlpha;
    if (iequals(word, "GL_SRC_ALPHA_SATURATE")) return BlendFactor::SrcAlphaSaturate;
    return BlendFactor::Unset;
}

// Die Kurzformen, die Q3 zusaetzlich erlaubt.
bool shorthandBlend(const std::string& word, BlendFactor& src, BlendFactor& dst) {
    if (iequals(word, "add") || iequals(word, "GL_ADD")) {
        src = BlendFactor::One;
        dst = BlendFactor::One;
        return true;
    }
    if (iequals(word, "filter")) {
        src = BlendFactor::DstColor;
        dst = BlendFactor::Zero;
        return true;
    }
    if (iequals(word, "blend")) {
        src = BlendFactor::SrcAlpha;
        dst = BlendFactor::OneMinusSrcAlpha;
        return true;
    }
    return false;
}

WaveForm readWave(const std::vector<std::string>& words, size_t from) {
    WaveForm w;
    if (from < words.size()) w.func = words[from];
    if (from + 1 < words.size()) w.base = toFloat(words[from + 1]);
    if (from + 2 < words.size()) w.amplitude = toFloat(words[from + 2]);
    if (from + 3 < words.size()) w.phase = toFloat(words[from + 3]);
    if (from + 4 < words.size()) w.frequency = toFloat(words[from + 4]);
    return w;
}

void parseStage(Tokenizer& tok, Stage& stage, Library& library,
                std::string_view fileName) {
    for (;;) {
        std::string word = tok.next();
        if (word.empty() || word == "}") return;

        if (iequals(word, "map") || iequals(word, "clampmap")) {
            auto rest = tok.restOfLine();
            if (!rest.empty()) {
                if (iequals(word, "clampmap")) stage.clampMap = rest[0];
                else stage.map = rest[0];
            }
        } else if (iequals(word, "animMap") || iequals(word, "oneshotanimMap")) {
            // JKA ergaenzt oneshotanimMap: die Folge laeuft einmal statt in
            // Schleife. Der Aufbau ist identisch.
            stage.animOneShot = iequals(word, "oneshotanimMap");
            auto rest = tok.restOfLine();
            if (!rest.empty()) stage.animFrequency = toFloat(rest[0]);
            for (size_t i = 1; i < rest.size(); ++i) stage.animMaps.push_back(rest[i]);
            if (stage.animMaps.empty()) {
                library.diagnostics.push_back(
                    {Severity::Warning, tok.line(),
                     std::string(fileName) + ": animMap ohne Bilder"});
            }
        } else if (iequals(word, "blendFunc")) {
            auto rest = tok.restOfLine();
            if (rest.size() == 1) {
                if (!shorthandBlend(rest[0], stage.srcBlend, stage.dstBlend)) {
                    library.diagnostics.push_back(
                        {Severity::Warning, tok.line(),
                         std::string(fileName) + ": unbekannte blendFunc-Kurzform \"" +
                             rest[0] + "\""});
                }
            } else if (rest.size() >= 2) {
                stage.srcBlend = blendFactor(rest[0]);
                stage.dstBlend = blendFactor(rest[1]);
                if (stage.srcBlend == BlendFactor::Unset ||
                    stage.dstBlend == BlendFactor::Unset) {
                    library.diagnostics.push_back(
                        {Severity::Warning, tok.line(),
                         std::string(fileName) + ": unbekannter blendFunc-Faktor"});
                }
            }
        } else if (iequals(word, "rgbGen")) {
            auto rest = tok.restOfLine();
            if (rest.empty()) continue;
            const std::string& g = rest[0];
            stage.rgbGenSet = true;
            if (iequals(g, "identity")) stage.rgbGen = ColorGen::Identity;
            else if (iequals(g, "identityLighting")) stage.rgbGen = ColorGen::IdentityLighting;
            else if (iequals(g, "vertex")) {
                stage.rgbGen = ColorGen::Vertex;
                // ParseStage: `if ( stage->alphaGen == 0 ) stage->alphaGen =
                // AGEN_VERTEX;` — 0 ist AGEN_IDENTITY, also auch ein schon
                // ausdruecklich gesetztes identity.
                if (stage.alphaGen == AlphaGen::Identity) stage.alphaGen = AlphaGen::Vertex;
            }
            else if (iequals(g, "exactVertex")) stage.rgbGen = ColorGen::ExactVertex;
            else if (iequals(g, "entity")) stage.rgbGen = ColorGen::Entity;
            else if (iequals(g, "oneMinusEntity")) stage.rgbGen = ColorGen::OneMinusEntity;
            else if (iequals(g, "oneMinusVertex")) stage.rgbGen = ColorGen::OneMinusVertex;
            else if (iequals(g, "lightingDiffuse")) stage.rgbGen = ColorGen::LightingDiffuse;
            else if (iequals(g, "lightingSpecular")) stage.rgbGen = ColorGen::LightingSpecular;
            else if (iequals(g, "const")) {
                stage.rgbGen = ColorGen::Const;
                // Form: const ( r g b ) — die Klammern sind eigene Woerter.
                int found = 0;
                for (size_t i = 1; i < rest.size() && found < 3; ++i) {
                    if (rest[i] == "(" || rest[i] == ")") continue;
                    stage.rgbConst[found++] = toFloat(rest[i]);
                }
            } else if (iequals(g, "wave")) {
                stage.rgbGen = ColorGen::Wave;
                stage.rgbWave = readWave(rest, 1);
            }
        } else if (iequals(word, "alphaGen")) {
            auto rest = tok.restOfLine();
            if (rest.empty()) continue;
            const std::string& g = rest[0];
            stage.alphaGenSet = true;
            if (iequals(g, "identity")) stage.alphaGen = AlphaGen::Identity;
            else if (iequals(g, "vertex")) stage.alphaGen = AlphaGen::Vertex;
            else if (iequals(g, "oneMinusVertex")) stage.alphaGen = AlphaGen::OneMinusVertex;
            else if (iequals(g, "entity")) stage.alphaGen = AlphaGen::Entity;
            else if (iequals(g, "oneMinusEntity")) stage.alphaGen = AlphaGen::OneMinusEntity;
            else if (iequals(g, "lightingSpecular")) stage.alphaGen = AlphaGen::LightingSpecular;
            else if (iequals(g, "portal")) stage.alphaGen = AlphaGen::Portal;
            else if (iequals(g, "const")) {
                stage.alphaGen = AlphaGen::Const;
                for (size_t i = 1; i < rest.size(); ++i) {
                    if (rest[i] == "(" || rest[i] == ")") continue;
                    stage.alphaConst = toFloat(rest[i]);
                    break;
                }
            } else if (iequals(g, "wave")) {
                stage.alphaGen = AlphaGen::Wave;
                stage.alphaWave = readWave(rest, 1);
            }
        } else if (iequals(word, "tcMod")) {
            auto rest = tok.restOfLine();
            if (rest.empty()) continue;
            TexMod mod;
            mod.type = rest[0];
            for (size_t i = 1; i < rest.size(); ++i) mod.args.push_back(toFloat(rest[i]));

            // `stretch` und `turb` beschreiben eine Welle. Ohne diese Zeilen
            // landet der Name der Wellenform in `args` als Null, und beide
            // taten gar nichts.
            //
            //     tcMod stretch <func> <base> <amp> <phase> <freq>
            //     tcMod turb           <base> <amp> <phase> <freq>
            if (iequals(mod.type, "stretch") && rest.size() >= 6) {
                std::string func = rest[1];
                for (char& c : func) {
                    c = static_cast<char>(
                        std::tolower(static_cast<unsigned char>(c)));
                }
                mod.wave.func = func;
                mod.wave.base = toFloat(rest[2]);
                mod.wave.amplitude = toFloat(rest[3]);
                mod.wave.phase = toFloat(rest[4]);
                mod.wave.frequency = toFloat(rest[5]);
            } else if (iequals(mod.type, "turb") && rest.size() >= 5) {
                // Kein Name: turb ist immer ein Sinus.
                mod.wave.func = "sin";
                mod.wave.base = toFloat(rest[1]);
                mod.wave.amplitude = toFloat(rest[2]);
                mod.wave.phase = toFloat(rest[3]);
                mod.wave.frequency = toFloat(rest[4]);
            }
            stage.texMods.push_back(std::move(mod));
        } else if (iequals(word, "tcGen") || iequals(word, "texGen")) {
            auto rest = tok.restOfLine();
            if (!rest.empty()) stage.tcGen = rest[0];
        } else if (iequals(word, "alphaFunc")) {
            auto rest = tok.restOfLine();
            if (!rest.empty()) stage.alphaFunc = rest[0];
        } else if (iequals(word, "depthFunc")) {
            auto rest = tok.restOfLine();
            if (!rest.empty()) stage.depthFunc = rest[0];
        } else if (iequals(word, "depthWrite")) {
            stage.depthWrite = true;
        } else if (iequals(word, "detail")) {
            stage.detail = true;
        } else if (iequals(word, "glow")) {
            stage.glow = true;
        } else {
            tok.restOfLine();  // unbekannt: Zeile verwerfen, wie die Engine
        }
    }
}

}  // namespace

bool Shader::needsAlpha() const {
    for (const auto& s : stages) {
        if (s.srcBlend == BlendFactor::SrcAlpha ||
            s.dstBlend == BlendFactor::SrcAlpha ||
            s.dstBlend == BlendFactor::OneMinusSrcAlpha ||
            !s.alphaFunc.empty()) {
            return true;
        }
    }
    return false;
}

bool Shader::usesVertexColor() const {
    for (const auto& s : stages) {
        if (s.rgbGen == ColorGen::Vertex || s.rgbGen == ColorGen::ExactVertex ||
            s.rgbGen == ColorGen::OneMinusVertex) {
            return true;
        }
    }
    return false;
}

const std::string& Shader::previewImage() const {
    static const std::string empty;
    if (!editorImage.empty()) return editorImage;
    for (const auto& s : stages) {
        if (!s.map.empty() && s.map[0] != '$') return s.map;
        if (!s.clampMap.empty()) return s.clampMap;
        if (!s.animMaps.empty()) return s.animMaps.front();
    }
    return empty;
}

TexCoord applyTexMods(const std::vector<TexMod>& mods, TexCoord in,
                      float seconds, const float position[3]) {
    for (const auto& mod : mods) {
        if (mod.type == "scroll" && mod.args.size() >= 2) {
            // RB_CalcScrollTexCoords: der Anteil wird abgeschnitten, damit die
            // Koordinaten nicht unbegrenzt wachsen.
            float s = mod.args[0] * seconds;
            float t = mod.args[1] * seconds;
            s -= std::floor(s);
            t -= std::floor(t);
            in.u += s;
            in.v += t;
        } else if (mod.type == "scale" && mod.args.size() >= 2) {
            in.u *= mod.args[0];
            in.v *= mod.args[1];
        } else if (mod.type == "rotate" && !mod.args.empty()) {
            // RB_CalcRotateTexCoords — Drehung um (0.5, 0.5). Das Minus vor
            // der Geschwindigkeit steht so im Quelltext.
            constexpr float kPi = 3.14159265358979323846f;
            const float degrees = -mod.args[0] * seconds;
            const float radians = degrees * kPi / 180.0f;
            const float c = std::cos(radians);
            const float sn = std::sin(radians);
            const float u = in.u;
            const float v = in.v;
            // RB_CalcTransformTexCoords wendet die Matrix SPALTENWEISE an:
            //
            //     st[0] = s*matrix[0][0] + t*matrix[1][0] + translate[0];
            //     st[1] = s*matrix[0][1] + t*matrix[1][1] + translate[1];
            //
            // mit matrix[1][0] = -sin und matrix[0][1] = +sin. Ich hatte die
            // beiden vertauscht, und der Punkt lief aus dem Bild statt sich zu
            // drehen: (1.00, 0.50) wurde zu (-0.50, 2.00) statt (0.50, 0.00).
            in.u = u * c + v * -sn + (0.5f - 0.5f * c + 0.5f * sn);
            in.v = u * sn + v * c + (0.5f - 0.5f * sn - 0.5f * c);
        }
        else if (mod.type == "stretch" && !mod.wave.func.empty()) {
            // RB_CalcStretchTexCoords: der KEHRWERT der Welle skaliert um die
            // Mitte.
            //
            //     p = 1.0f / EvalWaveForm( wf );
            //     matrix = { p, 0, 0, p };  translate = { 0.5-0.5p, 0.5-0.5p };
            //
            // Der Kehrwert ist der Grund, warum eine Welle um 1.0 herum
            // gemeint ist und nicht um 0: bei base 0 waere p unendlich.
            const float value = evaluateWave(mod.wave, seconds);
            if (std::fabs(value) > 1e-4f) {
                const float p = 1.0f / value;
                in.u = in.u * p + (0.5f - 0.5f * p);
                in.v = in.v * p + (0.5f - 0.5f * p);
            }
        } else if (mod.type == "turb" && position != nullptr) {
            // RB_CalcTurbulentTexCoords. Die Lage des Eckpunkts geht ein —
            // deshalb wirbelt eine grosse Flaeche anders als eine kleine.
            //
            //     now  = phase + floatTime * frequency;
            //     s   += sin( (x + z)/128 * 0.125 + now ) * amplitude;
            //     t   += sin(  y     /128 * 0.125 + now ) * amplitude;
            constexpr float kTwoPi = 6.283185307179586f;
            const float now = mod.wave.phase + seconds * mod.wave.frequency;
            const float su =
                (position[0] + position[2]) * (1.0f / 128.0f) * 0.125f + now;
            const float sv = position[1] * (1.0f / 128.0f) * 0.125f + now;
            in.u += std::sin(su * kTwoPi) * mod.wave.amplitude;
            in.v += std::sin(sv * kTwoPi) * mod.wave.amplitude;
        }
    }
    return in;
}

float evaluateWave(const WaveForm& wave, float seconds) {
    if (wave.func.empty() || wave.func == "none") return wave.base;

    constexpr float kTwoPi = 6.283185307179586f;

    // Der Anteil innerhalb einer Periode, 0 bis 1 — genau das, was die Engine
    // als Index in ihre Tabelle steckt (das `& FUNCTABLE_MASK` ist nichts
    // anderes als dieser Umlauf).
    float phase = wave.phase + seconds * wave.frequency;
    phase = phase - std::floor(phase);

    float value = 0.0f;
    if (iequals(wave.func, "sin")) {
        value = std::sin(phase * kTwoPi);
    } else if (iequals(wave.func, "square")) {
        // squareTable: erste Haelfte 1, zweite -1.
        value = phase < 0.5f ? 1.0f : -1.0f;
    } else if (iequals(wave.func, "triangle")) {
        // triangleTable (R_Init in tr_init.cpp) ist ZWEISEITIG: 0 -> 1 im
        // ersten Viertel, zurueck auf 0 bis zur Haelfte, dann gespiegelt
        // nach -1 und wieder 0:
        //
        //     if ( i < SIZE/2 ) { i < SIZE/4 ? i/(SIZE/4) : 1 - table[i-SIZE/4] }
        //     else               table[i] = -table[i-SIZE/2];
        //
        // Hier stand "laeuft 0..1..0, nicht -1..1" — falsch: `rgbGen wave
        // triangle 0.5 0.5` pulst im Spiel zwischen 0 und 1, bei uns lief es
        // nur zwischen 0.5 und 1.
        const float half = phase < 0.5f ? phase : phase - 0.5f;
        const float tri = half < 0.25f ? half * 4.0f : 2.0f - half * 4.0f;
        value = phase < 0.5f ? tri : -tri;
    } else if (iequals(wave.func, "sawtooth")) {
        value = phase;
    } else if (iequals(wave.func, "inversesawtooth")) {
        value = 1.0f - phase;
    } else if (iequals(wave.func, "noise")) {
        // EvalWaveForm (tr_shade_calc.cpp): GF_NOISE nimmt nicht die
        // Tabelle, sondern R_NoiseGet4f — weiches Wertrauschen, zwischen
        // ganzen Zeitschritten linear verbunden:
        //
        //     base + R_NoiseGet4f( 0, 0, 0, (floatTime + phase) * frequency ) * amplitude
        return wave.base +
               noiseAt((seconds + wave.phase) * wave.frequency) * wave.amplitude;
    } else if (iequals(wave.func, "random")) {
        // GF_RAND: an oder aus, je nach Rauschtabelle.
        //
        //     if ( GetNoiseTime( refdef.time + phase ) <= frequency )
        //         return base + amplitude;
        //     return base;
        //
        // refdef.time ist in Millisekunden; GetNoiseTime liefert 1 + Tabelle,
        // also 0 bis 2. Hier fehlte die Art ganz — sie ergab die Grundlinie.
        const int ms = static_cast<int>(seconds * 1000.0f + wave.phase);
        return noiseTime(ms) <= wave.frequency ? wave.base + wave.amplitude
                                               : wave.base;
    }
    return wave.base + value * wave.amplitude;
}

namespace {

// Die Rauschtabelle aus tr_noise.cpp (R_NoiseInit): srand(1001), dann je
// Eintrag zwei rand()-Aufrufe. rand() ist das der Microsoft-Laufzeit, mit der
// das Spiel gebaut ist — ein LCG mit 214013/2531011, RAND_MAX 32767. So
// entsteht dieselbe Tabelle wie im Spiel, und dasselbe Flackern.
struct NoiseTables {
    float table[256];
    int perm[256];
    NoiseTables() {
        unsigned state = 1001u;
        auto msvcRand = [&state]() {
            state = state * 214013u + 2531011u;
            return static_cast<int>((state >> 16) & 0x7fffu);
        };
        for (int i = 0; i < 256; ++i) {
            table[i] = static_cast<float>(
                (static_cast<float>(msvcRand()) / 32767.0f) * 2.0 - 1.0);
            perm[i] = static_cast<unsigned char>(
                static_cast<float>(msvcRand()) / 32767.0f * 255.0f);
        }
    }
};

const NoiseTables& noiseTables() {
    static const NoiseTables tables;
    return tables;
}

int noiseVal(int a) { return noiseTables().perm[a & 255]; }

}  // namespace

float noiseTime(int t) {
    // GetNoiseTime: 1 + s_noise_table[ VAL(t) ].
    return 1.0f + noiseTables().table[noiseVal(t)];
}

float noiseAt(float t) {
    // R_NoiseGet4f( 0, 0, 0, t ): mit x = y = z = 0 bleibt nur die lineare
    // Verbindung zwischen den ganzen Zeitschritten uebrig.
    //
    //     INDEX( x, y, z, t ) = VAL( x + VAL( y + VAL( z + VAL( t ) ) ) )
    const int it = static_cast<int>(std::floor(t));
    const float ft = t - static_cast<float>(it);
    auto value = [](int step) {
        return noiseTables().table[noiseVal(0 + noiseVal(0 + noiseVal(0 + noiseVal(step))))];
    };
    const float a = value(it);
    const float b = value(it + 1);
    return a * (1.0f - ft) + b * ft;
}

bool stageBlends(const Stage& stage) {
    if (stage.srcBlend == BlendFactor::Unset || stage.dstBlend == BlendFactor::Unset) {
        return false;
    }
    return !(stage.srcBlend == BlendFactor::One && stage.dstBlend == BlendFactor::Zero);
}

bool stageWritesDepth(const Stage& stage) {
    return !stageBlends(stage) || stage.depthWrite;
}

ColorGen effectiveColorGen(const Stage& stage) {
    if (stage.rgbGenSet) return stage.rgbGen;
    // ParseStage: "if cgen isn't explicitly specified, use either identity
    // or identitylighting".
    if (stageBlends(stage) &&
        (stage.srcBlend == BlendFactor::One || stage.srcBlend == BlendFactor::SrcAlpha)) {
        return ColorGen::IdentityLighting;
    }
    return ColorGen::Identity;
}

AlphaGen effectiveAlphaGen(const Stage& stage) { return stage.alphaGen; }

float sortValue(const Shader& shader) {
    // ParseSort: Namen oder eine Zahl (atof).
    if (!shader.sort.empty()) {
        static const struct { const char* name; int value; } kNames[] = {
            {"portal", kSortPortal},       {"sky", kSortEnvironment},
            {"opaque", kSortOpaque},       {"decal", kSortDecal},
            {"seeThrough", kSortSeeThrough}, {"banner", kSortBanner},
            {"additive", kSortBlend1},     {"nearest", kSortNearest},
            {"underwater", kSortUnderwater}, {"inside", kSortInside},
            {"mid_inside", kSortMidInside}, {"middle", kSortMiddle},
            {"mid_outside", kSortMidOutside}, {"outside", kSortOutside},
        };
        for (const auto& entry : kNames) {
            if (iequals(shader.sort, entry.name)) return static_cast<float>(entry.value);
        }
        return std::strtof(shader.sort.c_str(), nullptr);
    }
    if (shader.polygonOffset) return kSortDecal;
    // FinishShader: nur wenn schon Stufe 0 mischt, entscheidet die erste
    // mischende Stufe.
    if (!shader.stages.empty() && stageBlends(shader.stages[0])) {
        for (const auto& stage : shader.stages) {
            if (!stageBlends(stage)) continue;
            if (stage.depthWrite) return kSortSeeThrough;
            if (stage.srcBlend == BlendFactor::One && stage.dstBlend == BlendFactor::One) {
                return kSortBlend1;
            }
            return kSortBlend0;
        }
    }
    return kSortOpaque;
}

int animFrameAt(int frameCount, float framesPerSecond, bool oneShot,
                float seconds) {
    if (frameCount <= 1) return 0;
    if (!(framesPerSecond > 0.0f)) return 0;

    // Abschneiden, nicht runden — die Engine rechnet in Festkomma und wirft
    // die Nachkommastellen weg. Wer hier rundet, ist ein halbes Bild voraus.
    const float exact = seconds * framesPerSecond;
    int index = static_cast<int>(exact);

    // Kann bei zurueckgespulter Vorschau vorkommen; die Engine faengt es
    // ebenfalls ab ("may happen with shader time offsets").
    if (index < 0) index = 0;

    if (oneShot) {
        return index >= frameCount ? frameCount - 1 : index;
    }
    return index % frameCount;
}

const Shader* Library::find(std::string_view name) const {
    for (const auto& s : shaders) {
        if (iequals(s.name, name)) return &s;
    }
    return nullptr;
}

void parseInto(Library& library, std::string_view text, std::string_view fileName) {
    Tokenizer tok(text);
    for (;;) {
        std::string name = tok.next();
        if (name.empty()) return;
        if (name == "}") {
            library.diagnostics.push_back(
                {Severity::Warning, tok.line(),
                 std::string(fileName) + ": } ohne zugehoerige {"});
            continue;
        }

        Shader shader;
        shader.name = name;
        shader.sourceFile = fileName;
        shader.line = tok.line();

        std::string open = tok.next();
        if (open != "{") {
            library.diagnostics.push_back(
                {Severity::Error, tok.line(),
                 std::string(fileName) + ": nach \"" + name + "\" fehlt die {"});
            return;
        }

        for (;;) {
            std::string word = tok.next();
            if (word.empty()) {
                library.diagnostics.push_back(
                    {Severity::Error, tok.line(),
                     std::string(fileName) + ": \"" + name + "\" wird nicht geschlossen"});
                library.shaders.push_back(std::move(shader));
                return;
            }
            if (word == "}") break;
            if (word == "{") {
                Stage stage;
                parseStage(tok, stage, library, fileName);
                shader.stages.push_back(std::move(stage));
                continue;
            }

            if (iequals(word, "cull")) {
                auto rest = tok.restOfLine();
                if (!rest.empty()) {
                    if (iequals(rest[0], "twosided") || iequals(rest[0], "disable") ||
                        iequals(rest[0], "none")) {
                        shader.cull = Cull::None;
                    } else if (iequals(rest[0], "back") ||
                               iequals(rest[0], "backside") ||
                               iequals(rest[0], "backsided")) {
                        shader.cull = Cull::Back;
                    } else {
                        shader.cull = Cull::Front;
                    }
                }
            } else if (iequals(word, "qer_editorimage")) {
                auto rest = tok.restOfLine();
                if (!rest.empty()) shader.editorImage = rest[0];
            } else if (iequals(word, "surfaceparm")) {
                auto rest = tok.restOfLine();
                if (!rest.empty()) shader.surfaceParms.push_back(rest[0]);
            } else if (iequals(word, "nopicmip")) {
                shader.noPicMip = true;
                tok.restOfLine();
            } else if (iequals(word, "nomipmaps")) {
                shader.noMipMaps = true;
                tok.restOfLine();
            } else if (iequals(word, "polygonOffset")) {
                shader.polygonOffset = true;
                tok.restOfLine();
            } else if (iequals(word, "sort")) {
                auto rest = tok.restOfLine();
                if (!rest.empty()) shader.sort = rest[0];
            } else if (iequals(word, "deformVertexes")) {
                auto rest = tok.restOfLine();
                std::string joined;
                for (const auto& r : rest) {
                    if (!joined.empty()) joined += " ";
                    joined += r;
                }
                shader.deforms.push_back(joined);
            } else {
                tok.restOfLine();
            }
        }

        library.shaders.push_back(std::move(shader));
    }
}

BlendMode blendModeOf(BlendFactor src, BlendFactor dst, bool* exact) {
    auto done = [&](BlendMode mode, bool wasExact) {
        if (exact) *exact = wasExact;
        return mode;
    };

    // Kein blendFunc heisst undurchsichtig — die Stufe ersetzt, was darunter
    // liegt.
    if (src == BlendFactor::Unset || dst == BlendFactor::Unset) {
        return done(BlendMode::Opaque, true);
    }

    // Die vier Kombinationen, die der Renderer genau kann.
    if (src == BlendFactor::One && dst == BlendFactor::Zero) {
        return done(BlendMode::Opaque, true);
    }
    if (src == BlendFactor::One && dst == BlendFactor::One) {
        return done(BlendMode::Additive, true);
    }
    if (src == BlendFactor::SrcAlpha && dst == BlendFactor::OneMinusSrcAlpha) {
        return done(BlendMode::AlphaBlend, true);
    }
    if (src == BlendFactor::DstColor && dst == BlendFactor::Zero) {
        return done(BlendMode::Modulate, true);
    }
    if (src == BlendFactor::DstColor && dst == BlendFactor::SrcColor) {
        return done(BlendMode::Filter, true);
    }

    // Alles andere runden. Massgeblich ist der Zielfaktor: bleibt das
    // Vorhandene erhalten (GL_ONE), wird aufgehellt; wird es mit dem
    // Quellalpha verrechnet, ist es eine Alphamischung.
    if (dst == BlendFactor::One) {
        // GL_SRC_ALPHA GL_ONE, GL_SRC_COLOR GL_ONE, GL_ZERO GL_ONE ...
        return done(BlendMode::Additive, false);
    }
    if (dst == BlendFactor::OneMinusSrcAlpha) {
        // GL_ONE GL_ONE_MINUS_SRC_ALPHA — vormultipliziertes Alpha. Sieht
        // einer Alphamischung sehr aehnlich.
        return done(BlendMode::AlphaBlend, false);
    }
    if (dst == BlendFactor::Zero) {
        // Die Quelle ersetzt das Ziel, moduliert um ihren eigenen Faktor.
        if (src == BlendFactor::DstColor || src == BlendFactor::DstAlpha) {
            return done(BlendMode::Modulate, false);
        }
        return done(BlendMode::Opaque, false);
    }
    if (dst == BlendFactor::SrcColor || dst == BlendFactor::SrcAlpha) {
        return done(BlendMode::Filter, false);
    }

    // Uebrig bleibt wenig. Additiv ist die haeufigste Mischung bei Effekten
    // und faellt am wenigsten auf, wenn sie falsch ist.
    return done(BlendMode::Additive, false);
}

const char* blendModeName(BlendMode mode) {
    switch (mode) {
        case BlendMode::Opaque: return "opaque";
        case BlendMode::Additive: return "additive";
        case BlendMode::AlphaBlend: return "alpha blend";
        case BlendMode::Modulate: return "modulate";
        case BlendMode::Filter: return "filter";
    }
    return "?";
}

}  // namespace efx::shader
