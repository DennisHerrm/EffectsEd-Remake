// Vom Effektmodell zurueck in Text.
//
// Ausgabeform folgt Ravens Dateien: Tabs als Einrueckung, Schluessel und Wert
// durch Tabs getrennt, Leerzeile zwischen den Bloecken. Der alte Editor
// schreibt Zahlen mit "%1.4g" — das ist die Voreinstellung nur, wenn man sie
// ausdruecklich waehlt, sonst schreiben wir verlustfrei.
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <sstream>

#include "efx/io.h"

namespace efx {
namespace {

// Kuerzeste Dezimaldarstellung, die denselben float wieder ergibt — immer
// in Festkomma.
//
// Vorher: "%.*g" mit steigender Genauigkeit. Das ist zwar verlustfrei, aber
// %g schaltet auf Exponentenschreibweise, sobald der Exponent die Genauigkeit
// erreicht: aus `life 1500 2500` wurde `life 1.5e+03 2.5e+03`, aus `size 10`
// wurde `1e+01`. Die Engine liest das (atof), aber kein Mensch, und keine
// einzige Raven-Datei sieht so aus. Gefunden vom Selbsttest, nicht von den
// 4209 Kernpruefungen — die verglichen nur Rundlaeufe, nie den Text.
//
// std::to_chars mit chars_format::fixed liefert die kuerzeste Festkomma-Form,
// die exakt zuruecklaeuft: 1500 -> "1500", 0.001 -> "0.001".
std::string exactFloat(float value) {
    if (value == 0.0f) return "0";  // auch -0 als "0"
    char buffer[96];
    const auto result =
        std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed);
    if (result.ec == std::errc{}) return std::string(buffer, result.ptr);
    std::snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));
    return buffer;
}

std::string ravenFloat(float value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%1.4g", static_cast<double>(value));
    return buffer;
}

class Writer {
public:
    explicit Writer(const WriteOptions& options) : options_(options) {}

    std::string take() { return out_.str(); }

    void line(const std::string& text = {}) {
        out_ << text << (options_.crlf ? "\r\n" : "\n");
    }

    std::string num(float value) const {
        return options_.numbers == NumberFormat::Raven ? ravenFloat(value)
                                                       : exactFloat(value);
    }

    std::string indent(int depth) const {
        return options_.tabs ? std::string(depth, '\t') : std::string(depth * 4, ' ');
    }

    // Schluessel und Wert werden mit Tabs auf Spalte gebracht, wie bei Raven.
    void keyValue(int depth, const std::string& key, const std::string& value) {
        std::string prefix = indent(depth) + key;
        // Zielspalte 5 Tabs ab Zeilenanfang; mindestens ein Tab dazwischen.
        int columns = static_cast<int>(prefix.size());
        int tabs = 1;
        if (options_.tabs) {
            const int target = 20;  // in Zeichen, bei Tabbreite 4
            int width = depth * 4 + static_cast<int>(key.size());
            tabs = width >= target ? 1 : (target - width + 3) / 4;
            if (tabs < 1) tabs = 1;
        }
        (void)columns;
        line(prefix + std::string(tabs, '\t') + value);
    }

    std::string rangeText(const Range& r) const {
        return r.ranged ? num(r.min) + " " + num(r.max) : num(r.min);
    }

    std::string vec3Text(const Vec3Range& v) const {
        std::string text = num(v.min[0]) + " " + num(v.min[1]) + " " + num(v.min[2]);
        if (v.ranged) {
            text += "\t" + num(v.max[0]) + " " + num(v.max[1]) + " " + num(v.max[2]);
        }
        return text;
    }

    // Die Kurvenwoerter einer Datei.
    //
    // Stehen die gelesenen Woerter noch da und ergeben sie dieselben Bits,
    // werden sie unveraendert geschrieben — Reihenfolge und Schreibweise wie
    // beim Autor. Nur wenn die Bits geaendert wurden (die Oberflaeche leert
    // dann die Woerter), wird neu formuliert.
    //
    // Der Fehler, den das behebt: hier stand
    //
    //     if ((flags & kCurveClamp) == kCurveClamp) add("clamp");
    //     else { nonlinear; random }
    //     ...
    //     if (flags & kCurveWave) add("wave");
    //
    // clamp IST nonlinear|wave (FX_CLAMP 0x0C in FxPrimitives.h). Aus jedem
    // "linear clamp" wurde so "clamp linear wave" — und die Pruefung meldete
    // nach dem Speichern einen Fehler, den die Datei vorher nicht hatte
    // (126 von 583 Raven-Dateien). Schlimmer: random steckte im else-Zweig,
    // aus "random linear clamp" wurde "clamp linear wave" — das zufaellige
    // Ausblenden war nach einmal Oeffnen und Speichern weg.
    std::string curveText(int flags, const std::vector<std::string>& words) const {
        if (!words.empty() && curveFlagsFromWords(words) == flags) {
            std::string text;
            for (const auto& word : words) {
                if (!text.empty()) text += " ";
                text += word;
            }
            return text;
        }

        // Neu formuliert in Ravens Reihenfolge: "random linear clamp",
        // "linear nonlinear". Die beiden Bits der Parametermaske ergeben genau
        // EIN Wort — nie zwei, sonst steht eine Kollision in der Datei.
        std::string text;
        auto add = [&](const char* name) {
            if (!text.empty()) text += " ";
            text += name;
        };
        if (flags & kCurveRandom) add("random");
        if (flags & kCurveLinear) add("linear");
        switch (flags & kCurveClamp) {
            case kCurveNonLinear: add("nonlinear"); break;
            case kCurveWave: add("wave"); break;
            case kCurveClamp: add("clamp"); break;
            default: break;
        }
        return text;
    }

    void writeChannel(const Channel& c, const std::string& blockName) {
        if (!c.present) return;
        line();
        line(indent(1) + blockName);
        line(indent(1) + "{");
        if (c.start.set) keyValue(2, "start", rangeText(c.start));
        if (c.end.set) keyValue(2, "end", rangeText(c.end));
        if (c.parm.set) keyValue(2, c.parmPlural ? "parms" : "parm", rangeText(c.parm));
        writeCurveFlags(c.curveFlags, c.curveWords, c.flagsPlural);
        line(indent(1) + "}");
    }

    // Auch ein Wort ohne Wirkung (unbekannt, oder das fuenfte) bleibt stehen —
    // deshalb zaehlen hier die Woerter mit, nicht nur die Bits.
    void writeCurveFlags(int flags, const std::vector<std::string>& words,
                         bool plural) {
        const std::string text = curveText(flags, words);
        if (!text.empty()) keyValue(2, plural ? "flags" : "flag", text);
    }

    // Ein Block, den die Engine nicht kennt, wortwoertlich (ForeignGroup).
    // Zeilenweise ueber line(), damit die eingestellten Zeilenenden gelten.
    void writeForeign(const ForeignGroup& group) {
        line();
        size_t from = 0;
        while (from <= group.text.size()) {
            const size_t to = group.text.find('\n', from);
            if (to == std::string::npos) {
                line(group.text.substr(from));
                break;
            }
            line(group.text.substr(from, to - from));
            from = to + 1;
        }
    }

    void writeColor(const ColorChannel& c) {
        if (!c.present) return;
        line();
        line(indent(1) + "rgb");
        line(indent(1) + "{");
        if (c.start.set) keyValue(2, "start", vec3Text(c.start));
        if (c.end.set) keyValue(2, "end", vec3Text(c.end));
        if (c.parm.set) keyValue(2, c.parmPlural ? "parms" : "parm", rangeText(c.parm));
        writeCurveFlags(c.curveFlags, c.curveWords, c.flagsPlural);
        line(indent(1) + "}");
    }

    void writeList(const std::vector<std::string>& items, const std::string& key) {
        if (items.empty()) return;
        line();
        line(indent(1) + key);
        line(indent(1) + "[");
        for (const auto& item : items) line(indent(2) + item);
        line(indent(1) + "]");
    }

    std::string flagText(uint32_t bits, const std::vector<FlagName>& table) const {
        std::string text;
        uint32_t remaining = bits;
        // Erst die Sammelnamen (mehrere Bits), damit ghoul2Collision nicht als
        // drei Einzelflags herauskommt.
        for (int pass = 0; pass < 2; ++pass) {
            for (const auto& entry : table) {
                bool multi = (entry.bits & (entry.bits - 1)) != 0;
                if ((pass == 0) != multi) continue;
                if (entry.bits && (remaining & entry.bits) == entry.bits) {
                    if (!text.empty()) text += " ";
                    text += entry.name;
                    remaining &= ~entry.bits;
                }
            }
        }
        return text;
    }

private:
    WriteOptions options_;
    std::ostringstream out_;
};

}  // namespace

std::string write(const Effect& effect, const WriteOptions& options) {
    Writer w(options);

    if (effect.repeatDelaySet) {
        w.keyValue(0, "repeatDelay", std::to_string(effect.repeatDelay));
    }

    // Fremde Bloecke an ihrer alten Stelle: vor der Primitive, vor der sie
    // standen. Wurden inzwischen Primitive geloescht, landen sie am Ende —
    // verloren geht keiner.
    auto writeForeignBefore = [&](size_t index) {
        for (const auto& group : effect.foreignGroups) {
            const size_t at = std::min(group.beforePrimitive, effect.primitives.size());
            if (at == index) w.writeForeign(group);
        }
    };

    for (size_t index = 0; index < effect.primitives.size(); ++index) {
        writeForeignBefore(index);
        const Primitive& p = effect.primitives[index];
        w.line();
        w.line(typeName(p.type));
        w.line("{");

        if (!p.name.empty()) w.keyValue(1, "name", p.name);
        if (p.flags) {
            w.keyValue(1, p.flagsSingular ? "flag" : "flags",
                       w.flagText(p.flags, flagNames()));
        }
        if (p.spawnFlags) {
            w.keyValue(1, p.spawnFlagsSingular ? "spawnFlag" : "spawnFlags",
                       w.flagText(p.spawnFlags, spawnFlagNames()));
        }
        if (p.materialImpactSet) {
            w.keyValue(1, "materialImpact",
                       p.materialImpact == MaterialImpact::ShellSound ? "shellsound"
                                                                     : "none");
        }
        if (p.cullRangeSet) w.keyValue(1, "cullrange", std::to_string(p.cullRange));
        if (p.count.set) w.keyValue(1, "count", w.rangeText(p.count));
        if (p.life.set) w.keyValue(1, "life", w.rangeText(p.life));
        if (p.delay.set) w.keyValue(1, "delay", w.rangeText(p.delay));
        if (p.elasticity.set) {
            w.keyValue(1, p.elasticityAsIntensity ? "intensity" : "bounce",
                       w.rangeText(p.elasticity));
        }
        if (p.origin.set) w.keyValue(1, "origin", w.vec3Text(p.origin));
        if (p.origin2.set) w.keyValue(1, "origin2", w.vec3Text(p.origin2));
        if (p.min.set) w.keyValue(1, "min", w.vec3Text(p.min));
        if (p.max.set) w.keyValue(1, "max", w.vec3Text(p.max));
        if (p.radius.set) w.keyValue(1, "radius", w.rangeText(p.radius));
        if (p.height.set) w.keyValue(1, "height", w.rangeText(p.height));
        if (p.rotation.set) w.keyValue(1, "rotation", w.rangeText(p.rotation));
        if (p.rotationDelta.set) {
            w.keyValue(1, "rotationDelta", w.rangeText(p.rotationDelta));
        }
        if (p.angles.set) {
            w.keyValue(1, p.anglesSingular ? "angle" : "angles", w.vec3Text(p.angles));
        }
        if (p.anglesDelta.set) w.keyValue(1, "angleDelta", w.vec3Text(p.anglesDelta));
        if (p.velocity.set) {
            w.keyValue(1, p.velocityShort ? "vel" : "velocity", w.vec3Text(p.velocity));
        }
        if (p.acceleration.set) {
            w.keyValue(1, p.accelShort ? "accel" : "acceleration",
                       w.vec3Text(p.acceleration));
        }
        if (p.gravity.set) w.keyValue(1, "gravity", w.rangeText(p.gravity));
        if (p.density.set) w.keyValue(1, "density", w.rangeText(p.density));
        if (p.variance.set) w.keyValue(1, "variance", w.rangeText(p.variance));
        if (p.windModifier.set) w.keyValue(1, "wind", w.rangeText(p.windModifier));

        w.writeColor(p.rgb);
        w.writeChannel(p.alpha, "alpha");
        w.writeChannel(p.size, p.sizeAsWidth ? "width" : "size");
        w.writeChannel(p.size2, p.size2AsWidth ? "width2" : "size2");
        w.writeChannel(p.length, p.lengthAsHeight ? "height" : "length");

        w.writeList(p.shaders, p.shadersPlural ? "shaders" : "shader");
        w.writeList(p.models, p.modelsPlural ? "models" : "model");
        w.writeList(p.sounds, p.soundsPlural ? "sounds" : "sound");
        w.writeList(p.playFx, "playfx");
        w.writeList(p.emitFx, "emitfx");
        w.writeList(p.impactFx, "impactfx");
        w.writeList(p.deathFx, "deathfx");

        w.line("}");
    }
    writeForeignBefore(effect.primitives.size());

    return w.take();
}

}  // namespace efx
