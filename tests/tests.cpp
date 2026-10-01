// Testlauf ohne Fremdbibliothek: jeder Test meldet sich selbst.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "efx/io.h"
#include "efx/shader.h"
#include "efx/theme.h"
#include "efx/layout.h"
#include "efx/i18n.h"
#include "efx/renderer.h"
#include "efx/paths.h"
#include "efx/gamepath.h"
#include "efx/camera.h"
#include "efx/scene.h"
#include "efx/playback.h"
#include "efx/assets.h"
#include "efx/curve.h"
#include "efx/sim.h"
#include "efx/sound.h"
#include "efx/tiles.h"
#include "efx/timeline.h"
#include "efx/undo.h"
#include "efx/image.h"
#include "efx/inflate.h"
#include "efx/particles.h"
#include "efx/fields.h"
#include "efx/diag.h"
#include "efx/jobs.h"
#include <numeric>
#include <set>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <stdlib.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

// Pfad des eigenen Programms, fuer den Absturztest.
std::string g_selfPath;

int g_failures = 0;
int g_checks = 0;
// Wie viele .efx im Datenordner lagen — siehe "Echte Raven-Dateien".
int g_dataFiles = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cout << "  FEHLER: " << what << "\n";
    }
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// --- Vergleich zweier Modelle ------------------------------------------

bool same(const efx::Range& a, const efx::Range& b) {
    return a.set == b.set && a.ranged == b.ranged &&
           (!a.set || (a.min == b.min && a.max == b.max));
}

bool same(const efx::Vec3Range& a, const efx::Vec3Range& b) {
    if (a.set != b.set || a.ranged != b.ranged) return false;
    if (!a.set) return true;
    for (int i = 0; i < 3; ++i) {
        if (a.min[i] != b.min[i] || a.max[i] != b.max[i]) return false;
    }
    return true;
}

// Kurven vergleichen: die Bits UND die Woerter, wie sie in der Datei standen,
// und die Schreibweisen parm/parms, flag/flags.
//
// Hier wurden nur die Bits verglichen. Damit ging durch, dass der Schreiber
// aus "linear clamp" ein "clamp linear wave" machte — die Bits sind gleich
// (clamp IST nonlinear|wave), die Datei aber nicht, und die Pruefung meldete
// danach einen Fehler, den das Original nicht hatte. Und "random linear
// clamp" verlor sein random, ohne dass ein Rundlauf hier rot wurde, weil
// keine der fuenf Dateien in data/ es enthielt.
bool sameCurve(int flagsA, const std::vector<std::string>& wordsA, bool parmPluralA,
               bool flagsPluralA, int flagsB, const std::vector<std::string>& wordsB,
               bool parmPluralB, bool flagsPluralB) {
    return flagsA == flagsB && wordsA == wordsB && parmPluralA == parmPluralB &&
           flagsPluralA == flagsPluralB;
}

bool same(const efx::Channel& a, const efx::Channel& b) {
    return a.present == b.present && same(a.start, b.start) && same(a.end, b.end) &&
           same(a.parm, b.parm) &&
           sameCurve(a.curveFlags, a.curveWords, a.parmPlural, a.flagsPlural,
                     b.curveFlags, b.curveWords, b.parmPlural, b.flagsPlural);
}

bool same(const efx::ColorChannel& a, const efx::ColorChannel& b) {
    return a.present == b.present && same(a.start, b.start) && same(a.end, b.end) &&
           same(a.parm, b.parm) &&
           sameCurve(a.curveFlags, a.curveWords, a.parmPlural, a.flagsPlural,
                     b.curveFlags, b.curveWords, b.parmPlural, b.flagsPlural);
}

bool same(const efx::Primitive& a, const efx::Primitive& b, std::string& why) {
    auto fail = [&](const char* what) {
        why = what;
        return false;
    };
    if (a.type != b.type) return fail("Typ");
    if (a.name != b.name) return fail("Name");
    if (a.flags != b.flags) return fail("flags");
    if (a.spawnFlags != b.spawnFlags) return fail("spawnFlags");
    if (a.cullRangeSet != b.cullRangeSet || a.cullRange != b.cullRange)
        return fail("cullrange");
    if (!same(a.count, b.count)) return fail("count");
    if (!same(a.life, b.life)) return fail("life");
    if (!same(a.delay, b.delay)) return fail("delay");
    if (!same(a.elasticity, b.elasticity)) return fail("bounce");
    if (!same(a.origin, b.origin)) return fail("origin");
    if (!same(a.origin2, b.origin2)) return fail("origin2");
    if (!same(a.min, b.min)) return fail("min");
    if (!same(a.max, b.max)) return fail("max");
    if (!same(a.radius, b.radius)) return fail("radius");
    if (!same(a.height, b.height)) return fail("height");
    if (!same(a.velocity, b.velocity)) return fail("velocity");
    if (!same(a.acceleration, b.acceleration)) return fail("acceleration");
    if (!same(a.angles, b.angles)) return fail("angles");
    if (!same(a.anglesDelta, b.anglesDelta)) return fail("angleDelta");
    if (!same(a.gravity, b.gravity)) return fail("gravity");
    if (!same(a.density, b.density)) return fail("density");
    if (!same(a.variance, b.variance)) return fail("variance");
    if (!same(a.windModifier, b.windModifier)) return fail("wind");
    if (!same(a.rotation, b.rotation)) return fail("rotation");
    if (!same(a.rotationDelta, b.rotationDelta)) return fail("rotationDelta");
    if (!same(a.rgb, b.rgb)) return fail("rgb");
    if (!same(a.alpha, b.alpha)) return fail("alpha");
    if (!same(a.size, b.size)) return fail("size");
    if (!same(a.size2, b.size2)) return fail("size2");
    if (!same(a.length, b.length)) return fail("length");
    if (a.shaders != b.shaders) return fail("shaders");
    if (a.models != b.models) return fail("models");
    if (a.sounds != b.sounds) return fail("sounds");
    if (a.playFx != b.playFx) return fail("playfx");
    if (a.emitFx != b.emitFx) return fail("emitfx");
    if (a.impactFx != b.impactFx) return fail("impactfx");
    if (a.deathFx != b.deathFx) return fail("deathfx");
    return true;
}

bool same(const efx::Effect& a, const efx::Effect& b, std::string& why) {
    if (a.repeatDelaySet != b.repeatDelaySet || a.repeatDelay != b.repeatDelay) {
        why = "repeatDelay";
        return false;
    }
    if (a.primitives.size() != b.primitives.size()) {
        why = "Anzahl Primitive";
        return false;
    }
    // Bloecke, die die Engine nicht kennt (ForceFeedback), muessen
    // wortwoertlich und an derselben Stelle wiederkommen.
    if (a.foreignGroups.size() != b.foreignGroups.size()) {
        why = "Anzahl fremder Bloecke";
        return false;
    }
    for (size_t i = 0; i < a.foreignGroups.size(); ++i) {
        if (a.foreignGroups[i].text != b.foreignGroups[i].text ||
            a.foreignGroups[i].beforePrimitive != b.foreignGroups[i].beforePrimitive) {
            why = "fremder Block " + std::to_string(i + 1);
            return false;
        }
    }
    for (size_t i = 0; i < a.primitives.size(); ++i) {
        std::string field;
        if (!same(a.primitives[i], b.primitives[i], field)) {
            why = "Primitive " + std::to_string(i + 1) + ", Feld " + field;
            return false;
        }
    }
    return true;
}

// --- Tests -------------------------------------------------------------

void testRealFiles(const std::filesystem::path& dataDir) {
    std::cout << "== Echte Raven-Dateien ==\n";

    // Wie viele Dateien der Ordner enthaelt, wird MITGEZAEHLT und am Ende
    // genannt.
    //
    // Der Grund: die Zahl der Pruefungen haengt an diesem Ordner — je .efx
    // fuenf Stueck. Legt jemand eine Datei dazu, waechst die Gesamtzahl, ohne
    // dass sich am Programm etwas geaendert haette. Ich habe daraufhin
    // einmal eine Viertelstunde gesucht, warum aus 4191 Pruefungen 4184
    // geworden waren — es lag daran, dass eine Datei nicht mehr im Ordner
    // lag.
    //
    // Eine Zahl, die man nicht zwischen zwei Laeufen vergleichen kann, ist
    // als Sicherheitsmerkmal wertlos. Deshalb steht sie jetzt daneben.
    int filesInFolder = 0;

    for (const auto& entry : std::filesystem::directory_iterator(dataDir)) {
        if (entry.path().extension() != ".efx") continue;
        ++filesInFolder;
        const std::string name = entry.path().filename().string();
        std::string text = readFile(entry.path());

        efx::ReadResult first = efx::read(text);
        check(!first.hasErrors(), name + ": ohne Fehler lesbar");

        for (const auto& d : first.diagnostics) {
            if (d.severity == efx::Severity::Warning) {
                std::cout << "  Hinweis " << name << ":" << d.line << " — " << d.message
                          << "\n";
            }
        }

        // Schreiben und wieder lesen muss dasselbe Modell ergeben.
        for (auto format : {efx::NumberFormat::Exact, efx::NumberFormat::Raven}) {
            efx::WriteOptions options;
            options.numbers = format;
            std::string written = efx::write(first.effect, options);
            efx::ReadResult second = efx::read(written);
            check(!second.hasErrors(), name + ": Ausgabe ist wieder lesbar");

            std::string why;
            if (format == efx::NumberFormat::Exact) {
                bool equal = same(first.effect, second.effect, why);
                check(equal, name + ": verlustfreier Umlauf (" + why + ")");
            } else {
                // Bei %1.4g darf sich die Struktur nicht aendern, nur Nachkommastellen.
                check(second.effect.primitives.size() ==
                          first.effect.primitives.size(),
                      name + ": Raven-Format behaelt alle Primitive");
            }
        }

        std::cout << "  " << name << ": " << first.effect.primitives.size()
                  << " Primitive\n";
    }
    g_dataFiles = filesInFolder;
}

void testNumberText() {
    std::cout << "== Zahlen im Dateitext ==\n";
    // Verlustfrei UND lesbar: keine Exponentenschreibweise. Vorher schrieb
    // exactFloat "%.*g" mit kleinster Genauigkeit, und aus 1500 wurde
    // "1.5e+03" — gefunden vom Selbsttest der Oberflaeche, weil hier nur
    // Rundlaeufe verglichen wurden und nie der Text.
    efx::Effect effect;
    efx::Primitive p = efx::freshPrimitive(efx::PrimitiveType::Particle);
    p.life = efx::Range::span(1500.0f, 2500.0f);
    p.count = efx::Range::single(10.0f);
    p.delay = efx::Range::span(-2.5f, 0.001f);
    p.gravity = efx::Range::span(123456.0f, 0.1f);
    effect.primitives.push_back(p);
    efx::WriteOptions exact;
    exact.numbers = efx::NumberFormat::Exact;
    const std::string text = efx::write(effect, exact);
    check(text.find("1500 2500") != std::string::npos, "1500 2500 bleibt 1500 2500");
    check(text.find("count") != std::string::npos && text.find("\t10\r\n") != std::string::npos,
          "10 bleibt 10");
    check(text.find("-2.5 0.001") != std::string::npos, "-2.5 0.001 bleibt so");
    check(text.find("123456 0.1") != std::string::npos, "123456 0.1 bleibt so");
    check(text.find("e+") == std::string::npos && text.find("e-") == std::string::npos,
          "kein Exponent im Text");
    const efx::ReadResult back = efx::read(text);
    check(!back.hasErrors() && back.effect.primitives.size() == 1 &&
              back.effect.primitives[0].gravity.min == 123456.0f &&
              back.effect.primitives[0].delay.max == 0.001f,
          "und es laeuft exakt zurueck");
}

void testPrecision() {
    std::cout << "== Zahlgenauigkeit ==\n";
    // Der alte Editor schreibt %1.4g; 0.988235 wird dabei zu 0.9882.
    const char* source =
        "Particle\n{\n\trgb\n\t{\n\t\tstart\t0.988235 0.909804 0.788235\n\t}\n}\n";
    efx::ReadResult r = efx::read(source);
    check(r.effect.primitives.size() == 1, "eine Primitive gelesen");
    float original = r.effect.primitives[0].rgb.start.min[0];

    efx::WriteOptions exact;
    exact.numbers = efx::NumberFormat::Exact;
    float afterExact = efx::read(efx::write(r.effect, exact)).effect.primitives[0]
                           .rgb.start.min[0];
    check(afterExact == original, "Exact laesst den Wert unveraendert");

    efx::WriteOptions raven;
    raven.numbers = efx::NumberFormat::Raven;
    float afterRaven = efx::read(efx::write(r.effect, raven)).effect.primitives[0]
                           .rgb.start.min[0];
    check(afterRaven != original, "Raven-Format verliert Stellen (wie erwartet)");
    std::printf("  original %.9g, Raven %.9g, Abweichung %.3g\n",
                static_cast<double>(original), static_cast<double>(afterRaven),
                std::fabs(static_cast<double>(original - afterRaven)));
}

void testFlags() {
    std::cout << "== Flags ==\n";
    // ghoul2Collision setzt drei Bits und muss auch wieder als ein Wort
    // herauskommen, nicht als drei Einzelflags.
    const char* source = "Particle\n{\n\tflags\tghoul2Collision\n}\n";
    efx::ReadResult r = efx::read(source);
    check(r.effect.primitives[0].flags ==
              (efx::kFlagGhoul2Trace | efx::kFlagApplyPhysics |
               efx::kFlagExpensivePhysics),
          "ghoul2Collision setzt drei Bits");
    std::string written = efx::write(r.effect);
    check(written.find("ghoul2Collision") != std::string::npos,
          "ghoul2Collision wird als ein Wort zurueckgeschrieben");
    check(written.find("usePhysics") == std::string::npos,
          "keine doppelte Ausgabe der Einzelflags");

    // clamp belegt dieselben Bits wie nonlinear|random.
    efx::ReadResult c = efx::read(
        "Particle\n{\n\talpha\n\t{\n\t\tflags\tclamp\n\t}\n}\n");
    check(c.effect.primitives[0].alpha.curveFlags == efx::kCurveClamp,
          "clamp gelesen");
    check(efx::write(c.effect).find("clamp") != std::string::npos,
          "clamp wird als clamp geschrieben, nicht als nonlinear random");
}

void testTolerance() {
    std::cout << "== Fehlertoleranz ==\n";
    // Unbekannter Schluessel ist eine Warnung, kein Fehler — wie im Spiel.
    efx::ReadResult r = efx::read("Particle\n{\n\tblubb\t3\n\tlife\t100\n}\n");
    check(!r.hasErrors(), "unbekannter Schluessel bricht nicht ab");
    check(r.effect.primitives[0].life.set, "das folgende Feld wird trotzdem gelesen");

    // Fehlende Klammer muss ein Fehler mit Zeilennummer sein.
    efx::ReadResult broken = efx::read("Particle\n{\n\tlife\t100\n");
    check(broken.hasErrors(), "fehlende schliessende Klammer wird erkannt");
    check(!broken.diagnostics.empty() && broken.diagnostics[0].line > 0,
          "Fehler hat eine Zeilennummer");

    // Kommentare in beiden Formen.
    efx::ReadResult commented = efx::read(
        "// Kopf\nParticle\n{\n\tlife\t100 /* mitten drin */\n}\n");
    check(!commented.hasErrors(), "Kommentare stoeren nicht");
    check(commented.effect.primitives[0].life.min == 100.0f,
        "Wert vor Blockkommentar korrekt gelesen");
}

// Die drei Parserfehler, die Raven im Handbuch unter "Known Bugs" auffuehrt.
void testKnownRavenBugs() {
    std::cout << "== Ravens dokumentierte Fehler ==\n";

    // "File parsing can be thrown off by trailing white space after closing
    //  square brackets."
    const char* trailing =
        "Particle\n{\n\tshaders\n\t[\n\t\tgfx/misc/test\n\t]   \t \n\tlife\t100\n}\n";
    efx::ReadResult r = efx::read(trailing);
    check(!r.hasErrors(), "Leerzeichen hinter ] brechen nichts");
    check(r.effect.primitives.size() == 1 &&
              r.effect.primitives[0].shaders.size() == 1,
          "Shaderliste trotz Leerzeichen hinter ] korrekt");
    check(!r.effect.primitives.empty() && r.effect.primitives[0].life.set,
          "das Feld nach der Liste wird noch gelesen");

    // "The editor crashes if a file contains an empty shader/sound/model/
    //  effect list."
    const char* emptyLists =
        "Particle\n{\n\tshaders\n\t[\n\t]\n\tsounds\n\t[\n\t]\n\tmodels\n\t[\n\t]\n"
        "\tplayfx\n\t[\n\t]\n\tlife\t100\n}\n";
    efx::ReadResult e = efx::read(emptyLists);
    check(!e.hasErrors(), "leere Listen stuerzen nicht ab");
    check(e.effect.primitives.size() == 1, "Primitive trotzdem vorhanden");
    check(e.effect.primitives[0].shaders.empty(), "leere Liste bleibt leer");
    int emptyWarnings = 0;
    for (const auto& d : e.diagnostics) {
        if (d.message.find("ist leer") != std::string::npos) ++emptyWarnings;
    }
    check(emptyWarnings == 4, "jede leere Liste wird gemeldet");

    // Und die Ausgabe darf keine leere [ ] schreiben, die andere Werkzeuge
    // wieder aus dem Tritt bringt.
    check(efx::write(e.effect).find("[") == std::string::npos,
          "leere Listen werden nicht zurueckgeschrieben");
}

void testTransitionConflicts() {
    std::cout << "== Uebergangsarten ==\n";
    // nonlinear, clamp und wave teilen sich das Parameterfeld.
    efx::ReadResult bad = efx::read(
        "Particle\n{\n\tlife\t100\n\tsize\n\t{\n\t\tstart\t1\n\t\tend\t5\n"
        "\t\tflags\tnonlinear wave\n\t}\n\tshaders\n\t[\n\t\tx\n\t]\n}\n");
    bool conflict = false;
    for (const auto& d : efx::validate(bad.effect)) {
        if (d.severity == efx::Severity::Error &&
            d.id == efx::i18n::Str::VCurveCollision) {
            conflict = true;
        }
    }
    check(conflict, "nonlinear + wave wird als Konflikt gemeldet");

    // linear + nonlinear ist dagegen ausdruecklich erlaubt und ergibt die
    // 50-Prozent-Mischung aus dem Handbuch.
    efx::ReadResult good = efx::read(
        "Particle\n{\n\tlife\t100\n\tsize\n\t{\n\t\tstart\t1\n\t\tend\t5\n"
        "\t\tflags\tlinear nonlinear\n\t}\n\tshaders\n\t[\n\t\tx\n\t]\n}\n");
    for (const auto& d : efx::validate(good.effect)) {
        check(d.severity != efx::Severity::Error,
              "linear + nonlinear ist erlaubt, gemeldet wurde: " + d.message);
    }
    check((good.effect.primitives[0].size.curveFlags &
           (efx::kCurveLinear | efx::kCurveNonLinear)) ==
              (efx::kCurveLinear | efx::kCurveNonLinear),
          "linear + nonlinear korrekt gelesen");
}

void testWindIsDead() {
    std::cout << "== Wind ==\n";
    // Der Windpfeil im alten Editor zeigt eine Richtung an, die das Spiel nie
    // benutzt. Der auswertende Block ist in beiden Zweigen tot:
    //   Singleplayer  — prueft FX_AFFECTED_BY_WIND nirgends
    //   Multiplayer   — liest das Wort, der Block ist auskommentiert (/*rjr)
    // CL_GetWindVector steht genau einmal im Quelltext: in dieser Zeile.
    efx::Effect effect;
    effect.primitives.push_back(efx::Primitive{});
    efx::Primitive& p = effect.primitives.back();
    p.type = efx::PrimitiveType::Particle;
    p.life = efx::Range::single(500.0f);
    p.shaders.push_back("gfx/misc/test");
    p.spawnFlags = efx::kSpawnAffectedByWind;

    bool warned = false;
    for (const auto& d : efx::validate(effect, efx::Dialect::MP)) {
        if (d.id == efx::i18n::Str::VWindDead) {
            warned = true;
        }
    }
    check(warned, "affectedByWind wird auch im MP-Ziel als wirkungslos gemeldet");

    // Auch der reine Zahlenwert ohne Flag.
    p.spawnFlags = 0;
    p.windModifier = efx::Range::single(50.0f);
    warned = false;
    for (const auto& d : efx::validate(effect, efx::Dialect::SP)) {
        if (d.id == efx::i18n::Str::VWindDead) {
            warned = true;
        }
    }
    check(warned, "auch ein gesetzter wind-Wert allein wird gemeldet");

    // Ohne beides keine Meldung.
    p.windModifier = efx::Range{};
    for (const auto& d : efx::validate(effect, efx::Dialect::Both)) {
        check(d.id != efx::i18n::Str::VWindDead,
              "ohne Wind keine Windmeldung");
    }
}

void testDiagnosticSegment() {
    std::cout << "== Meldungen kennen ihr Segment ==\n";
    // Ein Klick im Meldungsfenster waehlt das betroffene Segment. Dafuer
    // muss jede Pruefmeldung sagen, welches es ist — die Hauptpruefung trug
    // frueher 0 ein, und der Klick tat nichts.
    efx::Effect effect;
    effect.primitives.resize(3);
    effect.primitives[0].type = efx::PrimitiveType::Particle;
    effect.primitives[0].shaders.push_back("gfx/misc/test");
    effect.primitives[1].type = efx::PrimitiveType::Sound;   // ohne Datei
    effect.primitives[2].type = efx::PrimitiveType::Particle;  // ohne Shader
    effect.primitives[2].shaders.push_back("gibt/es/nicht");
    bool sound = false, ohneSegment = true;
    for (const auto& d : efx::validate(effect)) {
        if (d.id == efx::i18n::Str::VSoundNoFile) sound = d.primitive == 1;
        if (d.primitive < 0 || d.primitive > 2) ohneSegment = false;
        check(d.line == 0, "Pruefmeldungen haben keine Dateizeile");
    }
    check(sound, "die Sound-Meldung gehoert zu Segment 2");
    check(ohneSegment, "jede Segmentmeldung nennt ein gueltiges Segment");
    bool shader = false;
    for (const auto& d : efx::validateShaderNames(effect, [](const std::string&) { return false; })) {
        if (d.primitive == 0 || d.primitive == 2) shader = true;
        check(d.line == 0, "die Shaderpruefung meldet das Segment nicht mehr als Zeile");
    }
    check(shader, "fehlende Shader nennen ihr Segment");
}

void testDialects() {
    std::cout << "== Dialekte SP und MP ==\n";

    // lessAttenuation kennt nur der Singleplayer-Parser.
    efx::ReadResult sp = efx::read(
        "Sound\n{\n\tspawnFlags\tlessAttenuation\n\tsounds\n\t[\n\t\tx.wav\n\t]\n}\n");
    check(sp.effect.primitives[0].spawnFlags == efx::kSpawnSoundLessAttenuation,
          "lessAttenuation gelesen");
    check(efx::validate(sp.effect, efx::Dialect::SP).empty(),
          "im SP-Ziel keine Beanstandung");
    bool mpWarns = false;
    for (const auto& d : efx::validate(sp.effect, efx::Dialect::MP)) {
        if (d.message.find("lessAttenuation") != std::string::npos) mpWarns = true;
    }
    check(mpWarns, "im MP-Ziel wird lessAttenuation gemeldet");

    // affectedByWind ist der umgekehrte Fall.
    efx::ReadResult mp = efx::read(
        "Particle\n{\n\tlife\t100\n\tspawnFlags\taffectedByWind\n"
        "\tshaders\n\t[\n\t\tx\n\t]\n}\n");
    // Frueher stand hier "die Meldungsliste ist leer". Das war zu grob: seit
    // die Pruefung weiss, dass Wind in beiden Zweigen tot ist, kommt dort eine
    // Meldung — nur eben keine über den Dialekt. Genau das wird jetzt geprueft.
    {
        bool dialectComplaint = false;
        for (const auto& d : efx::validate(mp.effect, efx::Dialect::MP)) {
            if (d.message.find("affectedByWind\" gibt es") != std::string::npos) {
                dialectComplaint = true;
            }
        }
        check(!dialectComplaint,
              "im MP-Ziel keine Dialektbeschwerde ueber affectedByWind");
    }
    bool spWarns = false;
    for (const auto& d : efx::validate(mp.effect, efx::Dialect::SP)) {
        if (d.message.find("affectedByWind") != std::string::npos) spWarns = true;
    }
    check(spWarns, "im SP-Ziel wird affectedByWind gemeldet");

    // Dialect::Both ist die strengste Einstellung: beide Faelle melden.
    check(!efx::validate(sp.effect, efx::Dialect::Both).empty() &&
              !efx::validate(mp.effect, efx::Dialect::Both).empty(),
          "Both meldet beide Richtungen");

    // MP-Flags relative und paperPhysics.
    efx::ReadResult rel = efx::read(
        "Particle\n{\n\tlife\t100\n\tflags\trelative\n"
        "\tshaders\n\t[\n\t\tx\n\t]\n}\n");
    check(rel.effect.primitives[0].flags == efx::kFlagRelative, "relative gelesen");
    check(efx::write(rel.effect).find("relative") != std::string::npos,
          "relative wird zurueckgeschrieben");

    // paperPhysics an einem Cylinder ist ein harter Fehler.
    efx::ReadResult cyl = efx::read(
        "Cylinder\n{\n\tlife\t100\n\tflags\tpaperPhysics\n"
        "\tshaders\n\t[\n\t\tx\n\t]\n}\n");
    bool cylError = false;
    for (const auto& d : efx::validate(cyl.effect, efx::Dialect::MP)) {
        if (d.severity == efx::Severity::Error &&
            d.message.find("size2") != std::string::npos) cylError = true;
    }
    check(cylError, "paperPhysics am Cylinder wird als Fehler gemeldet");

    // materialImpact ist MP-only und kennt nur einen Wert.
    efx::ReadResult mat = efx::read(
        "Particle\n{\n\tlife\t100\n\tmaterialImpact\tshellsound\n"
        "\tshaders\n\t[\n\t\tx\n\t]\n}\n");
    check(mat.effect.primitives[0].materialImpact == efx::MaterialImpact::ShellSound,
          "materialImpact shellsound gelesen");
    check(efx::write(mat.effect).find("shellsound") != std::string::npos,
          "materialImpact wird zurueckgeschrieben");
    bool matWarn = false;
    for (const auto& d : efx::validate(mat.effect, efx::Dialect::SP)) {
        if (d.message.find("materialImpact") != std::string::npos) matWarn = true;
    }
    check(matWarn, "materialImpact im SP-Ziel gemeldet");
}

void testShaders(const std::filesystem::path& dataDir) {
    std::cout << "== Shader ==\n";
    efx::shader::Library lib;
    int fileCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dataDir)) {
        if (entry.path().extension() != ".shader") continue;
        ++fileCount;
        efx::shader::parseInto(lib, readFile(entry.path()),
                               entry.path().filename().string());
    }
    if (fileCount == 0) {
        std::cout << "  (keine .shader-Dateien, uebersprungen)\n";
        return;
    }
    check(lib.diagnostics.empty(), "echte Shaderdateien ohne Meldung gelesen");
    check(lib.shaders.size() > 100, "Shader gefunden");
    std::cout << "  " << lib.shaders.size() << " Shader aus " << fileCount
              << " Dateien\n";

    // Jeder Shader muss ein Bild fuer die Vorschau anbieten koennen.
    size_t withoutImage = 0;
    for (const auto& s : lib.shaders) {
        if (s.previewImage().empty()) ++withoutImage;
    }
    check(withoutImage <= 1, "hoechstens ein Shader ohne Vorschaubild");

    // Gross-/Kleinschreibung muss egal sein, wie im Spiel.
    if (const auto* s = lib.find("gfx/effects/fire2")) {
        check(lib.find("GFX/EFFECTS/FIRE2") == s, "Suche ignoriert Schreibweise");
        check(s->usesVertexColor(), "rgbGen vertex erkannt");
    }

    // JKA-Zusatz oneshotanimMap.
    efx::shader::Library one;
    efx::shader::parseInto(one,
        "test/anim\n{\n\t{\n\t\toneshotanimMap 4 a b c\n\t\tblendFunc add\n\t}\n}\n",
        "test");
    check(one.shaders.size() == 1 && one.shaders[0].stages.size() == 1,
          "oneshotanimMap gelesen");
    check(one.shaders[0].stages[0].animOneShot, "als einmalig erkannt");
    check(one.shaders[0].stages[0].animMaps.size() == 3, "drei Bilder");
    check(one.shaders[0].stages[0].srcBlend == efx::shader::BlendFactor::One &&
              one.shaders[0].stages[0].dstBlend == efx::shader::BlendFactor::One,
          "blendFunc-Kurzform \"add\" aufgeloest");

    // Ravens Alphafalle aus dem Handbuch.
    efx::shader::Library alphaLib;
    efx::shader::parseInto(alphaLib,
        "test/alpha\n{\n\t{\n\t\tmap test/alpha\n"
        "\t\tblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n\t}\n}\n", "test");
    check(alphaLib.shaders[0].needsAlpha(), "Alphabedarf erkannt");

    efx::ReadResult noAlpha = efx::read(
        "Particle\n{\n\tlife\t100\n\tshaders\n\t[\n\t\ttest/alpha\n\t]\n}\n");
    bool warned = false;
    for (const auto& d : efx::validateAgainstShaders(noAlpha.effect, alphaLib)) {
        if (d.message.find("unsichtbar") != std::string::npos) warned = true;
    }
    check(warned, "Shader braucht Alpha, Primitive hat keins — gemeldet");

    efx::ReadResult withAlpha = efx::read(
        "Particle\n{\n\tlife\t100\n\tflags\tuseAlpha\n"
        "\tshaders\n\t[\n\t\ttest/alpha\n\t]\n}\n");
    for (const auto& d : efx::validateAgainstShaders(withAlpha.effect, alphaLib)) {
        check(d.message.find("unsichtbar") == std::string::npos,
              "mit useAlpha keine Meldung mehr");
    }
}

void testThemes() {
    std::cout << "== Farbgebung ==\n";
    const auto& themes = efx::theme::builtinThemes();
    check(themes.size() >= 5, "mehrere Themen vorhanden");
    check(themes.front().id == "dark", "Dunkel ist die Voreinstellung");

    bool hasLight = false, hasDark = false;
    for (const auto& t : themes) {
        (t.dark ? hasDark : hasLight) = true;
        check(efx::theme::findTheme(t.id) == &t, std::string(t.name()) + " ueber Kennung findbar");
    }
    check(hasLight && hasDark, "helle und dunkle Themen dabei");

    // Das eigentliche Versprechen: jedes Thema ist lesbar. Nicht behauptet,
    // gemessen — nach WCAG 2.1, 4.5:1 fuer Fliesstext.
    auto issues = efx::theme::checkContrast(themes);
    for (const auto& i : issues) {
        std::cout << "  " << i.theme << " / " << i.pair << ": " << i.ratio
                  << ":1 statt " << i.required << ":1\n";
    }
    check(issues.empty(), "alle Themen bestehen die Kontrastpruefung");

    for (const auto& t : themes) {
        std::printf("  %-18s %s  Text/Flaeche %.1f:1\n", t.name(),
                    t.dark ? "dunkel" : "hell  ",
                    static_cast<double>(efx::theme::contrastRatio(
                        t.palette.text, t.palette.windowBg)));
    }

    // Auch eine absichtlich unlesbare Zusammenstellung muss danach tragen —
    // das ist der Fall "Nutzer stellt sich eigene Farben ein".
    efx::theme::Palette bad{};
    bad.windowBg = efx::theme::Color::rgb(0x202020);
    bad.panelBg = efx::theme::Color::rgb(0x202020);
    bad.headerBg = efx::theme::Color::rgb(0x202020);
    bad.popupBg = efx::theme::Color::rgb(0x202020);
    bad.control = efx::theme::Color::rgb(0x202020);
    bad.viewportBg = efx::theme::Color::rgb(0x202020);
    bad.text = efx::theme::Color::rgb(0x282828);      // fast unsichtbar
    bad.textDim = efx::theme::Color::rgb(0x242424);
    bad.textOnAccent = efx::theme::Color::rgb(0x303030);
    bad.accent = efx::theme::Color::rgb(0x252525);
    bad.error = bad.warning = bad.info = bad.ok = efx::theme::Color::rgb(0x262626);
    bad.border = bad.grid = efx::theme::Color::rgb(0x212121);

    float beforeRatio = efx::theme::contrastRatio(bad.text, bad.windowBg);
    auto changes = efx::theme::enforceReadability(bad);
    check(beforeRatio < 1.3f, "Ausgangszustand war unlesbar");
    check(!changes.empty(), "Korrektur hat zugegriffen");
    check(efx::theme::contrastRatio(bad.text, bad.windowBg) >= 4.5f,
          "Text danach lesbar");
    check(efx::theme::contrastRatio(bad.error, bad.panelBg) >= 4.5f,
          "Fehlerfarbe danach lesbar");
    std::printf("  Notfall-Palette: Text von %.2f:1 auf %.2f:1, %zu Farben "
                "angepasst\n",
                static_cast<double>(beforeRatio),
                static_cast<double>(
                    efx::theme::contrastRatio(bad.text, bad.windowBg)),
                changes.size());

    // Farbton soll dabei erhalten bleiben, nicht in Grau kippen.
    efx::theme::Color blue = efx::theme::Color::rgb(0x101830);
    efx::theme::Color dark = efx::theme::Color::rgb(0x0A0A0A);
    efx::theme::ensureContrast(blue, dark, 4.5f);
    check(blue.b > blue.r && blue.b > blue.g, "Blau bleibt blau");
}

void testLayout() {
    std::cout << "== Aufteilung ==\n";
    efx::layout::Settings s;
    s.themeId = "midnight";
    s.languageCode = "ja";
    s.rendererCode = "gl3";
    s.split.propertiesFraction = 0.33f;
    s.split.listFraction = 0.2f;
    s.window = {120, 60, 1600, 900, true, true};
    s.worldScale = 32.0f;
    s.repeatRate = 0.45f;
    s.drawGrid = false;
    s.orientation = 2;
    s.gamePath = "C:/Games/JKA/base";
    s.windDirection[0] = 0.0f;
    s.windDirection[1] = -1.0f;
    s.windDirection[2] = 0.5f;
    s.windSpeed = 2.5f;

    efx::layout::Settings back = efx::layout::Settings::fromIni(s.toIni());
    check(back.themeId == "midnight", "Thema gespeichert");
    check(back.languageCode == "ja", "Sprache gespeichert");
    check(back.rendererCode == "gl3", "Grafikschnittstelle gespeichert");
    check(std::fabs(back.split.propertiesFraction - 0.33f) < 0.001f,
          "Teiler links/rechts gespeichert");
    check(std::fabs(back.split.listFraction - 0.2f) < 0.001f,
          "Teiler oben/unten gespeichert");
    check(back.window.width == 1600 && back.window.maximized,
          "Fensterlage gespeichert");
    check(back.worldScale == 32.0f && !back.drawGrid, "Schalter gespeichert");
    // Die Stelle der Windfahne wird gemerkt — sonst muesste man sie nach
    // jedem Start neu hinschieben.
    {
        efx::layout::Settings flag;
        flag.windFlagPos[0] = 123.5f;
        flag.windFlagPos[1] = -67.25f;
        const auto restoredFlag = efx::layout::Settings::fromIni(flag.toIni());
        check(std::fabs(restoredFlag.windFlagPos[0] - 123.5f) < 0.01f &&
                  std::fabs(restoredFlag.windFlagPos[1] + 67.25f) < 0.01f,
              "die Stelle der Fahne uebersteht einen Rundlauf");

        // Eine unsinnige Stelle wird zurueckgeholt: eine Fahne zehn Kilometer
        // neben dem Raum ist unsichtbar, und man findet sie nicht wieder.
        const auto silly =
            efx::layout::Settings::fromIni("windFlagPos=1e30 -1e30\n");
        check(std::fabs(silly.windFlagPos[0]) < 100000.0f,
              "eine unsinnige Stelle wird zurueckgeholt");
    }

    // Die vier Werkzeugleisten werden gespeichert und wieder eingelesen.
    {
        efx::layout::Settings bars;
        bars.showMainToolbar = false;
        bars.showEffectsToolbar = true;
        bars.showPlaybackToolbar = false;
        bars.showWorldToolbar = true;
        const auto restored = efx::layout::Settings::fromIni(bars.toIni());
        check(!restored.showMainToolbar && restored.showEffectsToolbar &&
                  !restored.showPlaybackToolbar && restored.showWorldToolbar,
              "die vier Werkzeugleisten ueberstehen einen Rundlauf");
        // Voreinstellung: alle sichtbar. Wer eine Datei von frueher hat, soll
        // nicht plötzlich vor einer leeren Leiste sitzen.
        const efx::layout::Settings fresh;
        check(fresh.showMainToolbar && fresh.showEffectsToolbar &&
                  fresh.showPlaybackToolbar && fresh.showWorldToolbar,
              "und sind voreingestellt alle sichtbar");
        check(efx::layout::Settings::fromIni("worldScale=16\n").showMainToolbar,
              "eine alte Einstellungsdatei ohne die Schluessel ebenso");
    }

    // Der alte Schluesselname muss weiter gelten: Einstellungsdateien von
    // frueher schrieben "drawWireframe" fuer dieselbe Sache.
    check(!efx::layout::Settings::fromIni("drawWireframe=0\n").drawGrid,
          "der alte Name drawWireframe wird noch gelesen");

    check(back.orientation == 2 && back.gamePath == "C:/Games/JKA/base",
          "Ausrichtung und Spielpfad gespeichert");
    check(back.windDirection[1] == -1.0f && back.windDirection[2] == 0.5f &&
              back.windSpeed == 2.5f,
          "Windrichtung und -staerke gespeichert");
    // Ein Nullvektor ist keine Richtung.
    auto zeroWind = efx::layout::Settings::fromIni("windDirection=0 0 0\n");
    check(zeroWind.windDirection[0] != 0.0f || zeroWind.windDirection[1] != 0.0f,
          "Nullvektor faellt auf die Voreinstellung zurueck");

    // Unsinnige Werte aus einer beschaedigten Datei duerfen nicht durchkommen.
    auto broken = efx::layout::Settings::fromIni(
        "windowW=3\nwindowH=2\nsplitProperties=9.5\ntimeScale=-4\n");
    check(!broken.window.valid, "unsinnige Fenstergroesse verworfen");
    check(broken.split.propertiesFraction <= 0.8f, "Teiler begrenzt");
    check(broken.timeScale == 1.0f, "negativer Zeitmasstab zurueckgesetzt");

    // Unbekannte Schluessel einer neueren Fassung stoeren nicht.
    auto future = efx::layout::Settings::fromIni(
        "theme=dark\nirgendwasNeues=42\ndrawAxes=0\n");
    check(future.themeId == "dark" && !future.drawAxes,
          "unbekannter Schluessel wird uebergangen");

    // Teiler duerfen nicht so weit gezogen werden, dass ein Bereich
    // verschwindet.
    efx::layout::Split split;
    split.propertiesFraction = 0.98f;
    split.listFraction = 0.99f;
    split.clampTo(1280.0f, 860.0f, 1.0f);
    check(split.propertiesFraction < 0.9f, "Eigenschaftenbreite begrenzt");
    check(split.listFraction < 0.9f, "Listenhoehe begrenzt");

    // Bei hoher Skalierung greifen die Untergrenzen frueher.
    efx::layout::Split hidpi;
    hidpi.propertiesFraction = 0.05f;
    hidpi.clampTo(1280.0f, 860.0f, 2.0f);
    check(hidpi.propertiesFraction >= 360.0f / 1280.0f,
          "Untergrenze skaliert mit der Bildschirmaufloesung");

    check(efx::layout::worldScaleCount() >= 3, "Weltmasstaebe vorhanden");

    // Die Reihenfolge ist die des Originals — dort steht "10 units/foot
    // (WARS)" an erster Stelle. Der Test prueft jetzt, dass alle sechs da
    // sind, statt einen bestimmten an einer bestimmten Stelle zu erwarten.
    //
    // Der alte Test hielt fest, dass 16 vorn steht. Das war keine Tatsache,
    // sondern der Zustand, solange 10 fehlte.
    bool has10 = false, has16 = false;
    for (int i = 0; i < efx::layout::worldScaleCount(); ++i) {
        const float units = efx::layout::worldScales()[i].unitsPerFoot;
        if (units == 10.0f) has10 = true;
        if (units == 16.0f) has16 = true;
        check(units > 0.0f, "jeder Masstab ist positiv");
    }
    check(has10, "10 Einheiten je Fuss (WARS) ist dabei");
    check(has16, "16 Einheiten je Fuss (SOF2) auch");
    // Voreingestellt sind 10, nicht 16.
    //
    // Hier stand das Gegenteil, mit der Begruendung \"das ist der Wert fuer
    // JKA\". Das war eine Annahme, keine Tatsache — und sie ist falsch. In
    // EffectsEd.exe stehen ab Adresse 607772 genau zwei Eintraege:
    //
    //     10 units/foot (WARS)
    //     16 units/foot (SOF2)
    //
    // Der erste ist der voreingestellte, und WARS ist Star Wars. 16 gehoert
    // zu Soldier of Fortune 2.
    //
    // Es ist nicht nur eine Zahl: der Masstab skaliert den Testraum und das
    // Gitter. Bei 16 statt 10 ist der Raum das 1,6-fache, und derselbe Effekt
    // wirkt entsprechend kleiner — beim Vergleich mit dem Original fiel genau
    // das auf.
    check(efx::layout::Settings{}.worldScale == 10.0f,
          "voreingestellt sind 10 Einheiten je Fuss (WARS), wie im Original");
}

void testLanguages() {
    std::cout << "== Sprachen ==\n";
    using namespace efx::i18n;

    const auto& langs = languages();
    check(langs.size() == 4, "vier Sprachen");
    check(langs[0].language == Language::English, "Englisch ist die erste");
    check(currentLanguage() == Language::English, "Englisch ist voreingestellt");

    // Kein Eintrag darf leer sein — ein leerer Knopf fällt im laufenden
    // Programm sonst niemandem auf.
    int emptyCount = 0;
    for (int i = 0; i < static_cast<int>(Str::Count); ++i) {
        Str id = static_cast<Str>(i);
        for (const auto& l : langs) {
            const char* text = trIn(l.language, id);
            if (!text || text[0] == '\0') ++emptyCount;
        }
    }
    check(emptyCount == 0, "kein Text ist leer");

    // Eine vergessene Übersetzung sieht so aus, dass die fremde Spalte
    // wortgleich mit der englischen ist. Bei Eigennamen und Kürzeln ist das
    // richtig, sonst nicht — deshalb eine Ausnahmeliste statt einer
    // Pauschalregel.
    const char* sameIsFine[] = {"EffectsEd", "RGB", "OK", "FxRunner",
                                "Direct3D 11", "OpenGL 3.3", "Min", "Max",
                                "Name", "Segment", "Emitter", "Alpha",
                                "Position", "Linear",
                                // Achsennamen der Begrenzungsbox (Physics).
                                "X", "Y", "Z",
                                // Schluesselwort des Dateiformats, kein Wort.
                                //
                                // In der .efx steht `CameraShake`, und die
                                // Segmentliste zeigt genau diesen Text. Wer
                                // ihn uebersetzt, zeigt in der Liste etwas
                                // anderes an, als in der Datei steht — und
                                // gerade danach sucht man dort.
                                //
                                // Wir schrieben zuvor `Camera Shake` mit
                                // Leerzeichen: zwei Schreibweisen fuer
                                // dieselbe Sache in einer Oberflaeche. Beim
                                // Abgleich mit dem Original aufgefallen, das
                                // ebenfalls `CameraShake` schreibt.
                                "CameraShake"};
    auto allowedSame = [&](const char* text) {
        for (const char* fine : sameIsFine) {
            if (std::string(text) == fine) return true;
        }
        return false;
    };

    int untranslated = 0;
    for (int i = 0; i < static_cast<int>(Str::Count); ++i) {
        Str id = static_cast<Str>(i);
        std::string en = trIn(Language::English, id);
        if (allowedSame(en.c_str())) continue;
        for (Language l : {Language::ChineseSimplified, Language::Japanese}) {
            if (std::string(trIn(l, id)) == en) {
                ++untranslated;
                std::cout << "  nicht übersetzt: \"" << en << "\"\n";
            }
        }
    }
    check(untranslated == 0, "keine vergessene CJK-Übersetzung");

    // Umschalten muss wirken und wieder zurückführen.
    setLanguage(Language::German);
    check(std::string(tr(Str::MenuFile)) == "Datei", "Umschalten auf Deutsch");
    setLanguage(Language::Japanese);
    check(std::string(tr(Str::MenuFile)) != "Datei", "Umschalten auf Japanisch");
    setLanguage(Language::English);
    check(std::string(tr(Str::MenuFile)) == "File", "zurück auf Englisch");

    // Codes für die Einstellungsdatei.
    check(findLanguage("zh-Hans") != nullptr, "zh-Hans gefunden");
    check(findLanguage("klingon") == nullptr, "unbekannter Code liefert nichts");
    check(findLanguage("zh-Hans")->needsCjkFont, "Chinesisch braucht CJK-Schrift");
    check(!findLanguage("en")->needsCjkFont, "Englisch nicht");

    // Ableitung aus den Systemeinstellungen.
    check(fromSystemLocale("de-DE") == Language::German, "de-DE");
    check(fromSystemLocale("de-AT") == Language::German, "de-AT");
    check(fromSystemLocale("de") == Language::German, "de");
    check(fromSystemLocale("ja-JP") == Language::Japanese, "ja-JP");
    check(fromSystemLocale("zh-Hans-CN") == Language::ChineseSimplified,
          "zh-Hans-CN");
    // Traditionelles Chinesisch haben wir nicht — dann lieber Englisch als
    // falsche Zeichen.
    check(fromSystemLocale("zh-Hant-TW") == Language::English, "zh-Hant zu Englisch");
    check(fromSystemLocale("zh-TW") == Language::English, "zh-TW zu Englisch");
    check(fromSystemLocale("fr-FR") == Language::English, "unbekannt zu Englisch");
    check(fromSystemLocale("") == Language::English, "leer zu Englisch");

    std::printf("  %d Texte x 4 Sprachen\n", static_cast<int>(Str::Count));
}

void testRenderers() {
    std::cout << "== Grafikschnittstellen ==\n";
    using namespace efx::render;

    Backend parsed{};
    check(backendFromCode("d3d11", parsed) && parsed == Backend::Direct3D11,
          "d3d11 erkannt");
    check(backendFromCode("gl3", parsed) && parsed == Backend::OpenGL3,
          "gl3 erkannt");
    check(!backendFromCode("vulkan", parsed), "unbekannter Code abgelehnt");

    auto order = fallbackOrder(Backend::OpenGL3);
    check(order.size() == 2 && order[0] == Backend::OpenGL3,
          "gewünschte Schnittstelle zuerst");
    check(order[1] == Backend::Direct3D11, "die andere danach");

    // Normalfall: beide laufen, die gewünschte wird genommen, kein Hinweis.
    std::vector<Probe> both = {
        {Backend::Direct3D11, true, "NVIDIA GeForce RTX 4070", "11.1", ""},
        {Backend::OpenGL3, true, "NVIDIA GeForce RTX 4070", "4.6", ""},
    };
    Backend chosen{};
    std::string note;
    check(chooseBackend(Backend::Direct3D11, both, chosen, note), "Auswahl gelingt");
    check(chosen == Backend::Direct3D11, "gewünschte genommen");
    check(note.empty(), "kein Hinweis nötig");

    // Der Fall, um den es geht: Direct3D fehlt, etwa über Remotedesktop.
    std::vector<Probe> onlyGl = {
        {Backend::Direct3D11, false, "", "",
         "D3D11CreateDevice lieferte DXGI_ERROR_UNSUPPORTED"},
        {Backend::OpenGL3, true, "Microsoft Basic Render Driver", "3.3", ""},
    };
    check(chooseBackend(Backend::Direct3D11, onlyGl, chosen, note),
          "Rückfall gelingt");
    check(chosen == Backend::OpenGL3, "OpenGL wird genommen");
    check(!note.empty(), "Rückfall wird gemeldet, nicht verschwiegen");
    check(note.find("DXGI_ERROR_UNSUPPORTED") != std::string::npos,
          "der Grund steht in der Meldung");
    std::cout << "  Rückfallmeldung: " << note << "\n";

    // Umgekehrt, für alte Treiber ohne OpenGL 3.3.
    std::vector<Probe> onlyD3d = {
        {Backend::Direct3D11, true, "Intel HD Graphics", "11.0", ""},
        {Backend::OpenGL3, false, "", "", "kein Kontext ab Fassung 3.3"},
    };
    check(chooseBackend(Backend::OpenGL3, onlyD3d, chosen, note), "andere Richtung");
    check(chosen == Backend::Direct3D11, "Direct3D wird genommen");

    // Und der Fall, in dem gar nichts geht: die Meldung muss beide Gründe
    // nennen, sonst rät der Nutzer.
    std::vector<Probe> none = {
        {Backend::Direct3D11, false, "", "", "kein Gerät"},
        {Backend::OpenGL3, false, "", "", "kein Kontext"},
    };
    check(!chooseBackend(Backend::Direct3D11, none, chosen, note),
          "kein Erfolg gemeldet");
    check(note.find("kein Gerät") != std::string::npos &&
              note.find("kein Kontext") != std::string::npos,
          "beide Gründe stehen in der Meldung");

    // Leere Liste darf nicht abstürzen.
    check(!chooseBackend(Backend::Direct3D11, {}, chosen, note),
          "leere Liste liefert sauber false");
}

void testPaths() {
    std::cout << "== Ablageort der Einstellungen ==\n";
    namespace fs = std::filesystem;

    // Gegen einen Wegwerfordner arbeiten, nicht gegen den echten
    // Benutzerordner — ein Test darf nichts anfassen, was ihm nicht gehört.
    const fs::path sandbox = fs::temp_directory_path() / "efxed_test_config";
    fs::remove_all(sandbox);
    efx::paths::setOverrideDirForTesting(sandbox.string());

    std::string dir = efx::paths::configDir();
    check(fs::exists(dir), "Ordner wird angelegt, falls er fehlt");
    check(dir == sandbox.string(), "der vorgegebene Ordner wird benutzt");

    // Alle drei Dateien liegen im selben Ordner. Das ist der eigentliche
    // Punkt: Einstellungen, Fensterzustand und Startprotokoll dürfen nicht
    // auf drei Orte verteilt sein, sonst findet man im Fehlerfall nichts.
    const std::string settings = efx::paths::settingsPath();
    const std::string ini = efx::paths::imguiIniPath();
    const std::string log = efx::paths::startupLogPath();
    check(fs::path(settings).parent_path() == sandbox, "Einstellungen im Ordner");
    check(fs::path(ini).parent_path() == sandbox, "Fensterzustand im Ordner");
    check(fs::path(log).parent_path() == sandbox, "Startprotokoll im Ordner");
    check(fs::path(settings).filename() != fs::path(ini).filename(),
          "verschiedene Dateinamen");

    // Der ganze Weg: schreiben, lesen, vergleichen.
    efx::layout::Settings written;
    written.themeId = "solarized-dark";
    written.languageCode = "zh-Hans";
    written.rendererCode = "gl3";
    written.split.propertiesFraction = 0.31f;
    {
        std::ofstream out(settings, std::ios::binary);
        out << written.toIni();
    }
    check(fs::exists(settings), "Datei angelegt");
    efx::layout::Settings loaded =
        efx::layout::Settings::fromIni(readFile(settings));
    check(loaded.themeId == "solarized-dark", "Thema überlebt den Umweg");
    check(loaded.languageCode == "zh-Hans", "Sprache überlebt den Umweg");
    check(loaded.rendererCode == "gl3", "Grafikschnittstelle überlebt den Umweg");
    std::cout << "  " << settings << "\n";

    // APPDATA-Weg. Der Override wird abgeschaltet und HOME umgebogen, damit
    // wirklich der Zweig läuft, der unter Windows greift.
    efx::paths::setOverrideDirForTesting("");
    const fs::path fakeHome = fs::temp_directory_path() / "efxed_test_home";
    fs::remove_all(fakeHome);
    fs::create_directories(fakeHome);
    // Die Umgebungsvariable muss auf beiden Systemen gesetzt werden.
    //
    // Vorher stand hier ein #ifndef _WIN32 um setenv — mit dem Ergebnis, dass
    // unter Windows die *echte* APPDATA-Variable galt und der Test gegen den
    // Wegwerfordner verglich. Der Test war rot, obwohl der Code richtig war:
    // C:\Users\...\AppData\Roaming\efxed ist genau, was er soll.
#ifdef _WIN32
    _putenv_s("APPDATA", fakeHome.string().c_str());
#else
    setenv("APPDATA", fakeHome.string().c_str(), 1);
#endif
    std::string appdataDir = efx::paths::configDir();
    check(appdataDir.find("efxed") != std::string::npos,
          "der Ordner heißt efxed, nicht irgendwie");
    check(fs::exists(appdataDir), "APPDATA-Ordner wird angelegt");
    check(fs::path(appdataDir).parent_path() == fakeHome,
          "liegt unter APPDATA, nicht neben der Exe");
    std::cout << "  APPDATA-Zweig: " << appdataDir << "\n";

    check(!efx::paths::isPortable(),
          "ohne Markierungsdatei kein mitnehmbarer Betrieb");

    fs::remove_all(sandbox);
    fs::remove_all(fakeHome);
    efx::paths::setOverrideDirForTesting("");
}

void testDiagLog() {
    std::cout << "== Ablaufprotokoll ==\n";
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "efxed_test_log";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string logPath = (dir / "efxed_start.log").string();

    check(efx::diag::open(logPath), "Protokolldatei wird angelegt");
    efx::diag::writeHeader({{"Fassung", "0.5"}, {"System", "Test"}});

    {
        efx::diag::Step outer("Oberflaeche starten");
        check(efx::diag::currentStep() == "Oberflaeche starten",
              "aktueller Schritt bekannt");
        {
            efx::diag::Step inner("Schriften laden");
            efx::diag::info("Grundschrift gefunden");
            check(efx::diag::currentStep() == "Schriften laden",
                  "geschachtelter Schritt gewinnt");
        }
        {
            efx::diag::Step failing("Symbolschrift laden");
            failing.fail("Datei nicht vorhanden");
        }
        efx::diag::warn("ohne Symbole weiter");
    }
    check(efx::diag::currentStep() == "(no step active)",
          "nach dem Verlassen kein Schritt offen");
    efx::diag::close();

    const std::string text = readFile(logPath);
    check(text.find("> Schriften laden") != std::string::npos, "Beginn vermerkt");
    check(text.find("< Schriften laden") != std::string::npos, "Ende vermerkt");
    check(text.find("Datei nicht vorhanden") != std::string::npos,
          "Grund des Misserfolgs vermerkt");
    check(text.find("Clean shutdown") != std::string::npos,
          "sauberer Abschluss erkennbar");
    check(text.find("  Fassung") != std::string::npos, "Kopfzeilen vorhanden");

    // Einrückung zeigt die Schachtelung.
    size_t innerPos = text.find("> Schriften laden");
    size_t outerPos = text.find("> Oberflaeche starten");
    size_t innerCol = innerPos - text.rfind('\n', innerPos);
    size_t outerCol = outerPos - text.rfind('\n', outerPos);
    check(innerCol > outerCol, "innerer Schritt ist eingerueckt");

    // Ein voriges Protokoll wird beiseitegelegt, nicht ueberschrieben — sonst
    // loescht der Neustartversuch nach dem Absturz den Beweis.
    efx::diag::open(logPath);
    efx::diag::info("zweiter Lauf");
    efx::diag::close();
    check(fs::exists(logPath + ".vorher"), "voriges Protokoll bleibt erhalten");
    check(readFile(logPath + ".vorher").find("Symbolschrift") != std::string::npos,
          "und enthaelt noch die alten Zeilen");

    // Ein nie abgeschlossener Schritt wird beim Schliessen benannt.
    efx::diag::open(logPath);
    {
        // Absichtlich auf dem Haufen, damit close() laeuft, waehrend der
        // Schritt noch offen ist. Aufgeraeumt wird danach — ein Test darf
        // nicht selbst lecken, sonst rauscht der Sanitizer bei jedem Lauf.
        auto* stillOpen = new efx::diag::Step("Absichtlich offen");
        efx::diag::close();
        check(readFile(logPath).find("step never completed: Absichtlich offen") !=
                  std::string::npos,
              "offener Schritt wird beim Schliessen gemeldet");
        delete stillOpen;  // Destruktor auf geschlossenem Protokoll: harmlos
    }
    check(efx::diag::currentStep() == "(no step active)",
          "Zerstoerung nach close() bringt nichts durcheinander");

    // Der eigentliche Zweck, hart geprueft.
    //
    // Frueher lief das nur unter Linux (fork + abort im Kind). Damit war
    // ausgerechnet auf der Zielplattform der wichtigste Test des Protokolls
    // nie gelaufen — genau der, der belegt, dass die letzte Zeile einen
    // Absturz ueberlebt. Deshalb jetzt ueber einen Neustart des eigenen
    // Programms: das geht auf beiden Systemen gleich.
    const std::string crashPath = (dir / "crash.log").string();
    if (!g_selfPath.empty()) {
        std::string command = "\"" + g_selfPath + "\" --crash-child \"" +
                              crashPath + "\"";
        // Die Fehlerausgabe des Kindes wegwerfen.
        //
        // Es stuerzt mit Absicht ab, und die Laufzeitbibliothek schreibt
        // dabei \"Aborted\" auf die Fehlerausgabe. Im Prueflauf steht das dann
        // unter \"== Kern uebersetzen und Tests ==\" — ich bin selbst darauf
        // hereingefallen und habe eine halbe Runde nach einem Absturz gesucht,
        // den es nicht gibt.
#ifdef _WIN32
        command += " 2>nul";
#else
        command += " 2>/dev/null";
#endif
#ifdef _WIN32
        // cmd.exe frisst die aeusseren Anfuehrungszeichen, wenn Pfad und
        // Argument beide welche haben. Ein zusaetzliches Paar aussen herum
        // loest das.
        command = "\"" + command + "\"";
#endif
        int rc = std::system(command.c_str());
        check(rc != 0, "das Kind ist tatsaechlich abgestuerzt");

        std::string crashText = readFile(crashPath);
        check(!crashText.empty(), "trotz Absturz ist etwas geschrieben");

        while (!crashText.empty() &&
               (crashText.back() == '\n' || crashText.back() == '\r')) {
            crashText.pop_back();
        }
        const size_t lastBreak = crashText.rfind('\n');
        std::string lastLine = lastBreak == std::string::npos
                                   ? crashText
                                   : crashText.substr(lastBreak + 1);
        while (!lastLine.empty() && lastLine.back() == '\r') lastLine.pop_back();

        check(lastLine.find("> Symbolschrift laden") != std::string::npos,
              "die letzte Zeile benennt den laufenden Schritt");
        check(lastLine.find("< ") == std::string::npos,
              "und sie hat keine Abschlussmarke");
        check(crashText.find("< Grafik anlegen") != std::string::npos,
              "die vorherigen Schritte sind als abgeschlossen erkennbar");
        check(crashText.find("Clean shutdown") == std::string::npos,
              "kein sauberer Abschluss - der Absturz ist als solcher erkennbar");
        std::cout << "  Letzte Zeile nach Absturz: " << lastLine << "\n";
    } else {
        std::cout << "  (Absturztest uebersprungen: eigener Pfad unbekannt)\n";
    }

    efx::diag::resetForTesting();
    fs::remove_all(dir);
}

// Miss den Nutzen der Verteilung — und pruefe das Ergebnis, nicht die Uhr.
//
// Hier stand einmal eine Obergrenze: mehr als „Arbeitsfaeden plus einer\" kann
// nicht herauskommen, also ist alles darueber ein Messfehler. Die Idee war
// richtig gemeint und ist trotzdem falsch.
//
// Erster Anlauf: 6.25 bei vier Faeden. Diagnose damals — die serielle Messung
// lief zuerst und war benachteiligt. Gegenmassnahmen: aufwaermen, fuenfmal
// messen, Median nehmen, Reihenfolge abwechseln.
//
// Zweiter Anlauf, auf derselben Maschine: **6.46**. Dieselbe Grenze, dieselbe
// Ueberschreitung, obwohl alle vier Vorkehrungen drin waren.
//
// Damit ist die Annahme widerlegt, nicht die Messung. Ein Faktor ueber der
// Fadenzahl ist real und heisst „superlinear\": verteilt bekommt jeder Faden
// ein Fuenftel der Aufgabe, und was seriell noch durch den Cache muss, liegt
// dann schon drin. Dazu kommt, dass eine Wanduhr auf einem Arbeitsrechner
// unter Windows um dreissig Prozent schwankt, je nach Takt und Kernvergabe.
//
// Und es steht so in der Uebergabe, im Abschnitt „Fehler, die ich gemacht
// habe\": **keine festen Zeitschranken.** Ein Test, der unter Last umkippt,
// prueft die Rechnerlast und nicht das Programm. Genau das ist hier passiert,
// zweimal, und beim Anwender statt bei mir.
//
// Also wird die Zeit weiter gemessen und ausgegeben — sie ist interessant und
// sie ist der Nachweis, dass sich das Verteilen lohnt. Geprueft wird
// stattdessen das, was IMMER gelten muss: verteilt muss Zahl fuer Zahl
// dasselbe herauskommen wie der Reihe nach.
struct BenchResult {
    double factor = 0.0;
    bool identical = false;
};

BenchResult benchmark(efx::jobs::Pool& pool, const char* label) {
    const size_t items = 512;
    std::vector<double> serialResults(items, 0.0);
    std::vector<double> parallelResults(items, 0.0);

    auto bodyInto = [&](std::vector<double>& into) {
        return [&into](size_t b, size_t e) {
            for (size_t i = b; i < e; ++i) {
                double acc = 0.0;
                for (int k = 0; k < 20000; ++k) acc += std::sin(double(i + k) * 0.001);
                into[i] = acc;
            }
        };
    };

    auto timeOnce = [&](bool parallel) {
        auto t0 = std::chrono::steady_clock::now();
        if (parallel) {
            // Portionsgroesse so waehlen, dass jeder Faden mehrere Portionen
            // bekommt. Bei genau einer je Faden wartet am Ende alles auf den
            // langsamsten — und auf 31 Faeden verschenkt man so das meiste.
            const size_t grain =
                std::max<size_t>(1, items / (pool.threadCount() * 4 + 4));
            pool.parallelFor(items, grain, bodyInto(parallelResults));
        } else {
            bodyInto(serialResults)(0, items);
        }
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - t0).count();
    };

    // Aufwaermen: der erste Durchgang zahlt fuer kalte Caches, das Wecken der
    // Faeden und eine CPU, die noch nicht hochgetaktet hat. Die Zahl wird
    // dadurch nicht verlaesslich — aber ehrlicher.
    timeOnce(false);
    timeOnce(true);

    std::vector<long long> serialRuns, parallelRuns;
    for (int round = 0; round < 5; ++round) {
        if (round % 2 == 0) {
            serialRuns.push_back(timeOnce(false));
            parallelRuns.push_back(timeOnce(true));
        } else {
            parallelRuns.push_back(timeOnce(true));
            serialRuns.push_back(timeOnce(false));
        }
    }

    // Median statt Mittelwert: ein einzelner Ausreisser durch einen
    // Prozesswechsel soll das Ergebnis nicht verschieben.
    auto median = [](std::vector<long long> v) {
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    const long long serial = median(serialRuns);
    const long long parallel = median(parallelRuns);

    BenchResult out;
    out.factor = parallel > 0 ? double(serial) / double(parallel) : 0.0;
    out.identical = serialResults == parallelResults;

    std::printf("  %-20s seriell %6.1f ms, verteilt %6.1f ms  Faktor %.2f "
                "(%u Faeden + Aufrufer)\n",
                label, double(serial) / 1000.0, double(parallel) / 1000.0,
                out.factor, pool.threadCount());
    return out;
}

void testJobs() {
    std::cout << "== Arbeitsverteilung ==\n";
    using namespace efx::jobs;

    std::printf("  Kerne gemeldet: %u, Arbeitsfaeden: %u\n",
                std::thread::hardware_concurrency(), pool().threadCount());

    // Ergebnisgleichheit. Ein paralleler Lauf muss dasselbe liefern wie ein
    // serieller — sonst ist er wertlos, egal wie schnell er ist.
    const size_t n = 200000;
    std::vector<double> out(n, 0.0);
    pool().parallelFor(n, 4096, [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) out[i] = std::sqrt(double(i)) * 2.0;
    });
    bool identical = true;
    for (size_t i = 0; i < n; ++i) {
        if (out[i] != std::sqrt(double(i)) * 2.0) { identical = false; break; }
    }
    check(identical, "paralleles Ergebnis gleicht dem seriellen");

    // Jedes Element genau einmal. Ueberlappende oder ausgelassene Bereiche
    // sind der haeufigste Fehler beim Aufteilen einer Schleife.
    std::vector<int> visits(n, 0);
    pool().parallelFor(n, 1000, [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) ++visits[i];
    });
    check(std::all_of(visits.begin(), visits.end(), [](int v) { return v == 1; }),
          "jedes Element genau einmal bearbeitet");

    // Randfaelle, an denen Aufteilungen gern scheitern.
    size_t touched = 0;
    pool().parallelFor(0, 16, [&](size_t, size_t) { ++touched; });
    check(touched == 0, "count 0 ruft den Rumpf gar nicht auf");

    std::vector<int> one(1, 0);
    pool().parallelFor(1, 16, [&](size_t b, size_t e) {
        for (size_t i = b; i < e; ++i) one[i] = 7;
    });
    check(one[0] == 7, "count 1 wird bearbeitet");

    std::vector<int> odd(1001, 0);
    pool().parallelFor(odd.size(), 100, [&](size_t b, size_t e) {
        for (size_t i = b; i < e; ++i) odd[i] = 1;
    });
    check(std::all_of(odd.begin(), odd.end(), [](int v) { return v == 1; }),
          "Restportion am Ende wird nicht vergessen");

    // Fortschritt.
    Progress progress;
    pool().parallelFor(5000, 250, [](size_t, size_t) {}, nullptr, &progress);
    check(progress.done.load() == 5000 && progress.total.load() == 5000,
          "Fortschritt zaehlt vollstaendig");
    check(progress.fraction() == 1.0f, "Anteil ist am Ende 1");

    // Abbruch. Der Punkt ist nicht, dass gar nichts laeuft, sondern dass es
    // aufhoert — wer den Grundpfad wechselt, soll nicht auf den alten
    // Suchlauf warten.
    Cancellation cancel;
    std::atomic<size_t> processed{0};
    cancel.cancel();
    pool().parallelFor(100000, 10, [&](size_t b, size_t e) {
        processed.fetch_add(e - b);
    }, &cancel);
    check(processed.load() < 100000, "abgebrochener Lauf bearbeitet nicht alles");
    std::printf("  nach sofortigem Abbruch: %zu von 100000 bearbeitet\n",
                processed.load());

    // Uebergabe an den Hauptfaden. Der einzige erlaubte Weg, ein Ergebnis aus
    // einem Arbeitsfaden an die Oberflaeche zu geben.
    std::vector<int> arrived;
    pool().parallelFor(64, 4, [&](size_t b, size_t e) {
        for (size_t i = b; i < e; ++i) {
            pool().postToMain([&arrived, i] { arrived.push_back(int(i)); });
        }
    });
    check(pool().pendingMainTasks() == 64, "alle Ergebnisse warten auf den Hauptfaden");
    check(arrived.empty(), "und keines ist vorher schon durchgerutscht");
    size_t ran = pool().drainMainQueue(-1);
    check(ran == 64 && arrived.size() == 64, "Hauptfaden arbeitet sie ab");
    check(pool().pendingMainTasks() == 0, "Schlange danach leer");

    // Eine Aufgabe, die selbst eine einreiht, darf nicht verklemmen.
    bool nested = false;
    pool().postToMain([&] {
        pool().postToMain([&] { nested = true; });
    });
    pool().drainMainQueue(-1);
    check(nested, "geschachteltes Einreihen verklemmt nicht");

    // Zeitbegrenzung: bei vielen Aufgaben soll ein Bild nicht haengen.
    //
    // Bewusst mit einer Warteschleife statt sleep_for. Der Windows-Zeitgeber
    // hat rund 15 ms Auflösung — ein sleep_for(50 us) schläft dort in
    // Wirklichkeit ein Vielfaches davon. Mit 2000 solchen Aufgaben lief
    // dieser Test unter Windows über eine halbe Minute, während er unter
    // Linux in 100 ms durch war.
    auto busyWait = [](int micros) {
        const auto until = std::chrono::steady_clock::now() +
                           std::chrono::microseconds(micros);
        while (std::chrono::steady_clock::now() < until) {}
    };
    const int kBusyTasks = 400;
    for (int i = 0; i < kBusyTasks; ++i) {
        pool().postToMain([&busyWait] { busyWait(50); });
    }
    auto before = std::chrono::steady_clock::now();
    size_t partial = pool().drainMainQueue(4);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - before).count();
    check(partial < static_cast<size_t>(kBusyTasks), "nicht alles in einem Bild");

    // Die Zeitschranke wird RELATIV geprueft, nicht gegen eine feste Zahl.
    //
    // Vorher stand hier `elapsed < 40` bei einer Schranke von 4 ms. Auf einem
    // ruhigen Rechner ist das reichlich; laeuft nebenher ein Uebersetzer,
    // dauert dieselbe Arbeit doppelt so lange, und der Test wird rot, ohne
    // dass etwas kaputt ist. Genau das ist einmal passiert — bei einem Lauf
    // unter Last, den ich danach zwanzigmal nicht mehr nachstellen konnte.
    //
    // Was der Test wirklich pruefen soll: dass die Schranke ueberhaupt
    // greift, also deutlich weniger Zeit vergeht als fuer ALLE Aufgaben
    // noetig waere. Das ist unabhaengig davon, wie schnell der Rechner ist.
    const long long allTasksMs = kBusyTasks * 50 / 1000;   // 50 us je Aufgabe
    check(elapsed < std::max<long long>(40, allTasksMs / 2),
          "die Zeitbegrenzung greift, gemessen an der Gesamtarbeit");
    std::printf("  Zeitbegrenzung 4 ms: %zu von %d in %lld ms "
                "(alles waere ~%lld ms)\n",
                partial, kBusyTasks, static_cast<long long>(elapsed), allTasksMs);
    pool().drainMainQueue(-1);

    // Messung.
    //
    // Gemessen wird der Nutzen, geprueft wird das Ergebnis. Warum keine
    // Obergrenze mehr auf dem Faktor steht, ist bei benchmark() begruendet.
    check(benchmark(pool(), "globaler Pool").identical,
          "globaler Pool: verteilt kommt Zahl fuer Zahl dasselbe heraus");

    pool().waitIdle();
    check(true, "waitIdle kehrt zurueck");

    // Der globale Pool richtet sich nach den gemeldeten Kernen. Auf einem
    // Einkerner — und in manchen Behaeltern — sind das null Arbeitsfaeden,
    // und dann laeuft die eigentliche Fadenlogik nie. Deshalb hier ein Pool
    // mit fest vorgegebener Fadenzahl: sonst haette dieser Test auf einer
    // solchen Maschine nichts geprueft und trotzdem gruen gemeldet.
    std::cout << "  -- mit erzwungenen 4 Arbeitsfaeden --\n";
    {
        Pool forced(4);
        check(forced.threadCount() == 4, "vorgegebene Fadenzahl wird benutzt");

        const size_t m = 100000;
        std::vector<int> seen(m, 0);

        // Nicht die Gleichzeitigkeit messen, sondern die Verteilung.
        //
        // Auf einer Maschine mit einem physischen Kern laufen vier Faeden
        // niemals im selben Augenblick — das Betriebssystem teilt die Zeit
        // auf. Eine Pruefung auf "mindestens zwei gleichzeitig" wuerde dort
        // fehlschlagen, obwohl alles richtig ist. Die richtige Frage ist:
        // haben mehrere Faeden Arbeit bekommen?
        std::mutex idMutex;
        std::set<std::thread::id> workerIds;

        forced.parallelFor(m, 512, [&](size_t b, size_t e) {
            for (size_t i = b; i < e; ++i) ++seen[i];
        });
        check(std::all_of(seen.begin(), seen.end(), [](int v) { return v == 1; }),
              "auch mit echten Faeden jedes Element genau einmal");

        // Zweiter Anlauf mit einer Arbeit, die tatsaechlich Zeit braucht.
        //
        // Beim ersten Versuch war das Ergebnis ein einziger Faden — und das
        // war kein Fehler: bei reiner Rechenarbeit holt sich der aufrufende
        // Faden alle Portionen, bevor die Arbeiter aus dem Warten aufwachen.
        // Das ist genau das gewuenschte Verhalten, weil Aufwecken teurer ist
        // als die Portion selbst.
        //
        // Verteilung zeigt sich erst, wenn eine Portion lange genug dauert,
        // dass sich das Aufwecken lohnt. Genau so sieht die spaetere echte
        // Arbeit aus: eine Datei lesen, ein Bild dekodieren.
        forced.parallelFor(20, 1, [&](size_t, size_t) {
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
            std::lock_guard<std::mutex> lock(idMutex);
            workerIds.insert(std::this_thread::get_id());
        });
        check(workerIds.size() >= 2,
              "bei laengeren Portionen wird auf mehrere Faeden verteilt");
        check(workerIds.count(std::this_thread::get_id()) == 1,
              "der aufrufende Faden arbeitet mit statt zu warten");
        std::printf("  beteiligte Faeden: %zu von 5 moeglichen "
                    "(4 Arbeiter + aufrufender)\n", workerIds.size());

        // Wettlauf auf einen gemeinsamen Zaehler: prueft, dass die Aufteilung
        // selbst keine Datenrennen einbaut.
        std::atomic<long long> sum{0};
        forced.parallelFor(100000, 256, [&](size_t b, size_t e) {
            long long local = 0;
            for (size_t i = b; i < e; ++i) local += static_cast<long long>(i);
            sum.fetch_add(local);
        });
        long long expected = 99999LL * 100000LL / 2;
        check(sum.load() == expected, "Summe ueber alle Portionen stimmt");

        // Einzelne Aufgaben und waitIdle mit echten Faeden.
        std::atomic<int> counter{0};
        for (int i = 0; i < 500; ++i) forced.post([&] { counter.fetch_add(1); });
        forced.waitIdle();
        check(counter.load() == 500, "waitIdle wartet auf alle eingereihten Aufgaben");

        // Uebergabe an den Hauptfaden aus echten Arbeitsfaeden.
        std::atomic<int> handed{0};
        forced.parallelFor(200, 8, [&](size_t b, size_t e) {
            for (size_t i = b; i < e; ++i) {
                forced.postToMain([&handed] { handed.fetch_add(1); });
            }
        });
        check(handed.load() == 0, "nichts laeuft vor dem Hauptfaden");
        forced.drainMainQueue(-1);
        check(handed.load() == 200, "alle 200 Ergebnisse kommen an");

        // Abbruch mitten im Lauf, nicht vorher. Das ist der Fall aus dem
        // Betrieb: der Nutzer wechselt den Grundpfad, waehrend gesucht wird.
        Cancellation midRunCancel;
        std::atomic<size_t> handled{0};
        forced.parallelFor(200000, 64, [&](size_t b, size_t e) {
            size_t total = handled.fetch_add(e - b) + (e - b);
            if (total > 20000) midRunCancel.cancel();
        }, &midRunCancel);
        check(handled.load() < 200000, "Abbruch mittendrin greift");
        std::printf("  Abbruch mittendrin: %zu von 200000 bearbeitet\n",
                    handled.load());

        check(benchmark(forced, "4 erzwungene Faeden").identical,
              "vier Faeden: verteilt kommt Zahl fuer Zahl dasselbe heraus");
    }
    check(true, "Pool laeuft beim Zerstoeren sauber aus");
}

void testGamePath() {
    std::cout << "== Spielpfad ==\n";
    namespace fs = std::filesystem;
    using namespace efx::gamepath;

    // Schraegstriche vereinheitlichen.
    check(normalise("C:\\Games\\JKA\\base") == "C:/Games/JKA/base",
          "Rueckwaertsschraegstriche werden gedreht");
    check(normalise("C:/Games/JKA/base/") == "C:/Games/JKA/base",
          "abschliessender Trenner faellt weg");
    check(normalise("C:/Games//JKA///base") == "C:/Games/JKA/base",
          "doppelte Trenner werden zusammengezogen");
    check(normalise("C:/") == "C:/", "Laufwerksstamm bleibt");
    check(normalise("") == "", "leer bleibt leer");

    check(inspect("").status == Status::Empty, "leerer Pfad");
    check(inspect("Z:/gibt/es/nicht").status == Status::Missing,
          "nicht vorhandener Pfad");

    const fs::path root = fs::temp_directory_path() / "efxed_gamepath_test";
    fs::remove_all(root);

    // Ein Ordner ohne alles ist kein base-Ordner.
    fs::create_directories(root / "leer");
    check(inspect((root / "leer").string()).status == Status::NotBaseLike,
          "leerer Ordner wird nicht als base durchgewunken");

    // Der haeufigste Tippfehler: GameData statt GameData/base gewaehlt.
    fs::create_directories(root / "GameData" / "base" / "shaders");
    { std::ofstream(root / "GameData" / "base" / "shaders" / "a.shader") << "x"; }
    auto wrongLevel = inspect((root / "GameData").string());
    check(wrongLevel.status == Status::NotBaseLike, "eine Ebene zu hoch erkannt");
    bool hinted = false;
    for (const auto& note : wrongLevel.notes) {
        if (note.find("base") != std::string::npos) hinted = true;
    }
    check(hinted, "und der Dialog sagt, welcher Ordner gemeint ist");

    // Ein echter base-Ordner mit ausgepackten Materialien.
    const fs::path base = root / "GameData" / "base";
    fs::create_directories(base / "effects" / "explosions");
    { std::ofstream(base / "effects" / "explosions" / "big.efx") << "x"; }
    { std::ofstream(base / "effects" / "small.efx") << "x"; }
    { std::ofstream(base / "shaders" / "b.shader") << "x"; }
    auto good = inspect(base.string());
    check(good.status == Status::Ok, "ausgepackter base-Ordner erkannt");
    check(good.shaderCount == 2, "Shaderdateien gezaehlt");
    check(good.efxCount == 2, "Effektdateien auch in Unterordnern gezaehlt");
    check(good.hasShadersFolder && good.hasEffectsFolder, "beide Ordner gemeldet");
    std::printf("  ausgepackt: %d Shader, %d Effekte\n", good.shaderCount,
                good.efxCount);

    // Ein unausgepacktes Spiel hat nur .pk3-Dateien.
    const fs::path packed = root / "packed";
    fs::create_directories(packed);
    for (const char* name : {"assets0.pk3", "assets1.pk3", "assets2.pk3"}) {
        std::ofstream(packed / name) << "x";
    }
    auto sealed = inspect(packed.string());
    check(sealed.status == Status::Ok, "unausgepacktes Spiel erkannt");
    check(sealed.pk3Count == 3, "pk3-Dateien gezaehlt");

    // Gross-/Kleinschreibung der Endung darf egal sein.
    const fs::path upper = root / "upper";
    fs::create_directories(upper);
    { std::ofstream(upper / "ASSETS.PK3") << "x"; }
    check(inspect(upper.string()).pk3Count == 1, "PK3 in Grossbuchstaben zaehlt");

    check(presets().size() >= 3, "Vorlagen vorhanden");
    for (const auto& preset : presets()) {
        check(std::string(preset.path).find('\\') == std::string::npos,
              std::string("Vorlage \"") + preset.label +
                  "\" benutzt Vorwaertsschraegstriche");
    }

    fs::remove_all(root);
}

void testCamera() {
    std::cout << "== Kamera ==\n";
    using namespace efx::camera;

    check(std::fabs(length(normalise({3.0f, 4.0f, 0.0f})) - 1.0f) < 1e-5f,
          "normalise liefert Laenge 1");
    check(length(normalise({0.0f, 0.0f, 0.0f})) == 0.0f,
          "Nullvektor bleibt Null statt NaN");

    // Matrizenmultiplikation gegen die Einheitsmatrix.
    Matrix a = identity();
    a[12] = 5.0f; a[13] = -3.0f; a[14] = 2.0f;
    Matrix product = multiply(a, identity());
    for (int i = 0; i < 16; ++i) check(product[i] == a[i], "M * I = M");

    Orbit cam;
    cam.reset(16.0f);
    // Grundstellung wie im Original: Auge bei (0, -80, 0) bei 10 Einheiten
    // je Fuss, Abstand waechst mit dem Massstab (16: 128), Blick entlang +Y,
    // 90 Grad senkrechtes Sichtfeld (gemessen, agentA ROOM-GEOMETRY.md).
    check(cam.distance() == 128.0f, "Abstand 8 Fuss bei 16 Einheiten/Fuss wie im Original");
    Orbit original;
    original.reset(10.0f);
    const Vec3 eye = original.position();
    check(std::fabs(eye.x) < 1e-3f && std::fabs(eye.y + 80.0f) < 1e-3f && std::fabs(eye.z) < 1e-3f,
          "Auge bei (0, -80, 0) wie im Original");
    check(original.fovDegrees() == 90.0f, "90 Grad Sichtfeld wie im Original");
    Orbit big;
    big.reset(64.0f);
    check(big.distance() == 512.0f, "und skaliert mit dem Weltmassstab mit");

    // Der Abstand zum Zielpunkt darf sich beim Drehen nicht aendern — das ist
    // die eine Eigenschaft, die eine Umlaufkamera ausmacht.
    const float before = length(cam.position() - cam.target());
    for (int i = 0; i < 40; ++i) cam.orbit(17.0f, 9.0f);
    const float after = length(cam.position() - cam.target());
    check(std::fabs(before - after) < 0.01f, "Drehen aendert den Abstand nicht");

    // Nicken darf nicht ueber den Pol laufen: dort ist die Aufwaertsrichtung
    // unbestimmt und das Bild kippt schlagartig.
    cam.reset(16.0f);
    for (int i = 0; i < 200; ++i) cam.orbit(0.0f, -50.0f);
    Matrix view = cam.viewMatrix();
    bool finite = true;
    for (float value : view) if (!std::isfinite(value)) finite = false;
    check(finite, "Blick genau von oben liefert brauchbare Zahlen");
    for (int i = 0; i < 400; ++i) cam.orbit(0.0f, 50.0f);
    view = cam.viewMatrix();
    for (float value : view) if (!std::isfinite(value)) finite = false;
    check(finite, "und von unten auch");

    // Die Blickmatrix muss orthonormal sein, sonst verzerrt das Bild.
    cam.reset(16.0f);
    cam.orbit(33.0f, 21.0f);
    view = cam.viewMatrix();
    const Vec3 right{view[0], view[4], view[8]};
    const Vec3 up{view[1], view[5], view[9]};
    const Vec3 back{view[2], view[6], view[10]};
    check(std::fabs(length(right) - 1.0f) < 1e-4f, "rechte Achse normiert");
    check(std::fabs(length(up) - 1.0f) < 1e-4f, "Aufwaertsachse normiert");
    check(std::fabs(dot(right, up)) < 1e-4f, "Achsen stehen senkrecht");
    check(std::fabs(dot(right, back)) < 1e-4f, "auch die dritte");

    // Der Zielpunkt muss in der Bildmitte landen.
    const Matrix proj = cam.projectionMatrix(16.0f / 9.0f);
    const Vec3 targetInView = transformPoint(view, cam.target());
    check(std::fabs(targetInView.x) < 1e-3f && std::fabs(targetInView.y) < 1e-3f,
          "Zielpunkt liegt auf der Blickachse");
    check(targetInView.z < 0.0f, "und vor der Kamera, nicht dahinter");
    check(proj[11] == -1.0f, "perspektivische Abbildung");

    // Tiefenbereich. OpenGL bildet auf -1..1 ab, Direct3D auf 0..1. Gibt man
    // eine OpenGL-Projektion unveraendert an Direct3D, wird die vordere
    // Haelfte weggeklippt — und man sucht den Fehler in der Geometrie.
    {
        Orbit depthCam;
        depthCam.reset(16.0f);
        const float nearPlane = 0.05f * 16.0f;
        const float farPlane = 600.0f * 16.0f;

        auto depthAt = [&](const Matrix& projection, float distance) {
            // Punkt auf der Blickachse, dann perspektivisch teilen.
            const float z = -distance;
            const float clipZ = projection[10] * z + projection[14];
            const float clipW = -z;
            return clipZ / clipW;
        };

        const Matrix gl = depthCam.projectionMatrix(1.0f, false);
        check(std::fabs(depthAt(gl, nearPlane) + 1.0f) < 1e-3f,
              "OpenGL: nahe Ebene liegt bei -1");
        check(std::fabs(depthAt(gl, farPlane) - 1.0f) < 1e-3f,
              "OpenGL: ferne Ebene liegt bei 1");

        const Matrix d3d = depthCam.projectionMatrix(1.0f, true);
        check(std::fabs(depthAt(d3d, nearPlane)) < 1e-3f,
              "Direct3D: nahe Ebene liegt bei 0");
        check(std::fabs(depthAt(d3d, farPlane) - 1.0f) < 1e-3f,
              "Direct3D: ferne Ebene liegt bei 1");

        // Beide muessen monoton steigen, sonst stimmt die Tiefensortierung nicht.
        float previousGl = -2.0f, previousD3d = -2.0f;
        bool monotone = true;
        for (float distance = nearPlane; distance < farPlane; distance *= 1.8f) {
            const float glDepth = depthAt(gl, distance);
            const float d3dDepth = depthAt(d3d, distance);
            if (glDepth <= previousGl || d3dDepth <= previousD3d) monotone = false;
            previousGl = glDepth;
            previousD3d = d3dDepth;
        }
        check(monotone, "Tiefe waechst in beiden Faellen monoton");

        // Was passiert wirklich, wenn man die OpenGL-Matrix an Direct3D gibt?
        //
        // Ich hatte behauptet, die vordere Haelfte der Szene werde
        // weggeklippt. Das ist falsch, und die Zahlen sagen es: die
        // OpenGL-Tiefe wird null bei 2*f*n/(f+n) — hier 1.6 Einheiten, also
        // gut einen halben Zoll vor der Kamera. Geklippt wird nur dieser
        // schmale Streifen.
        //
        // Der wirkliche Verlust ist die Genauigkeit: die Haelfte des
        // Tiefenpuffers bleibt ungenutzt. Kein sichtbarer Fehler, aber ein
        // messbarer — und deshalb wird er gemessen statt behauptet.
        const float crossover = 2.0f * farPlane * nearPlane / (farPlane + nearPlane);
        check(depthAt(gl, crossover * 0.99f) < 0.0f,
              "OpenGL-Tiefe ist knapp vor dem Nulldurchgang negativ");
        check(depthAt(gl, crossover * 1.01f) > 0.0f, "und knapp dahinter positiv");
        check(crossover < 2.0f * nearPlane + 0.01f,
              "der geklippte Streifen ist duenn, nicht die halbe Szene");
        check(depthAt(d3d, nearPlane * 1.001f) >= 0.0f,
              "die Direct3D-Matrix klippt dort gar nichts");

        // Der Genauigkeitsgewinn: wie viel des Bereichs 0..1 wird genutzt?
        const float glUsed = depthAt(gl, farPlane) - depthAt(gl, nearPlane);
        const float d3dUsed = depthAt(d3d, farPlane) - depthAt(d3d, nearPlane);
        check(std::fabs(glUsed - 2.0f) < 1e-3f, "OpenGL nutzt -1 bis 1");
        check(std::fabs(d3dUsed - 1.0f) < 1e-3f, "Direct3D nutzt 0 bis 1");

        // WO liegt die nahe Ebene? Das ist die Falle, nicht die Spannweite.
        //
        // Unter Direct3D ist z = 0 die NAECHSTE Tiefe, unter OpenGL die
        // MITTLERE. Ein Viereck bei z = 0, das die Tiefe schreibt, ist unter
        // Direct3D also eine Wand direkt vor der Kamera — alles dahinter
        // faellt beim Vergleich durch.
        //
        // Genau so ist es passiert: das Viereck, mit dem der Direct3D-Renderer
        // eine einzelne Kachel des Vorschaublattes leert, schrieb Tiefe. Jede
        // Kachel blieb schwarz. Unter OpenGL trat es nie auf, weil dort mit
        // glClear geleert wird und die Tiefe danach auf FERN steht.
        check(depthAt(d3d, nearPlane) < 0.001f,
              "Direct3D: die nahe Ebene liegt bei z = 0");
        check(depthAt(gl, nearPlane) < -0.999f,
              "OpenGL: die nahe Ebene liegt bei z = -1");
        std::printf("  nahe Ebene: Direct3D z=%.3f, OpenGL z=%.3f "
                    "(z=0 heisst also NICHT dasselbe)\n",
                    static_cast<double>(depthAt(d3d, nearPlane)),
                    static_cast<double>(depthAt(gl, nearPlane)));
        std::printf("  Nulldurchgang der OpenGL-Tiefe bei %.2f Einheiten "
                    "(nahe Ebene %.2f)\n",
                    static_cast<double>(crossover), static_cast<double>(nearPlane));
    }

    // Heranfahren ist verhaeltnismaessig und begrenzt.
    cam.reset(16.0f);
    const float start = cam.distance();
    cam.dolly(-100.0f);
    check(cam.distance() < start, "negativ faehrt heran");
    cam.dolly(100.0f);
    check(std::fabs(cam.distance() - start) < 0.01f, "und zurueck auf denselben Wert");
    for (int i = 0; i < 500; ++i) cam.dolly(-100.0f);
    check(cam.distance() > 0.0f, "Abstand wird nie null oder negativ");
    for (int i = 0; i < 500; ++i) cam.dolly(100.0f);
    check(cam.distance() < 1e6f, "und laeuft nicht ins Unendliche");

    // Schieben bewegt den Zielpunkt, nicht den Abstand.
    cam.reset(16.0f);
    const float distanceBefore = cam.distance();
    cam.pan(50.0f, 30.0f);
    check(length(cam.target()) > 0.0f, "Schieben bewegt den Zielpunkt");
    check(std::fabs(cam.distance() - distanceBefore) < 1e-4f,
          "aendert aber den Abstand nicht");
}

void testShake() {
    std::cout << "== Kamerawackeln ==\n";
    using namespace efx::camera;

    // Genau der Fehler, den du gemeldet hast: der alte Editor hat das nie
    // eingebaut. Hier steht die Formel aus cg_camera.cpp, und sie wird geprueft.
    Shake shake;
    shake.seed(12345);

    // Ausserhalb des Radius passiert nichts — CG_ExplosionEffects prueft das
    // vor allem anderen.
    shake.trigger(10.0f, 300, 500, 400.0f);
    check(!shake.active(), "ausserhalb des Radius keine Erschuetterung");

    // Innerhalb faellt die Staerke linear mit dem Abstand.
    shake.trigger(10.0f, 300, 500, 0.0f);
    check(shake.active(), "im Ursprung wird ausgeloest");
    shake.update(0, 90.0f);
    const float atCentre = shake.currentIntensity();
    shake.stop();

    shake.trigger(10.0f, 300, 500, 150.0f);
    shake.update(0, 90.0f);
    const float atHalf = shake.currentIntensity();
    check(std::fabs(atHalf - atCentre * 0.5f) < 0.01f,
          "auf halber Strecke halbe Staerke");
    std::printf("  Staerke 10, Radius 300: im Zentrum %.2f, bei 150 Einheiten %.2f\n",
                static_cast<double>(atCentre), static_cast<double>(atHalf));
    shake.stop();

    // MAX_SHAKE_INTENSITY greift bei 16.
    shake.trigger(40.0f, 300, 500, 0.0f);
    shake.update(0, 90.0f);
    check(shake.currentIntensity() <= Shake::kMaxIntensity + 0.01f,
          "Staerke wird bei 16 gedeckelt");
    std::printf("  Staerke 40 angefordert -> %.2f (Deckel %.0f)\n",
                static_cast<double>(shake.currentIntensity()),
                static_cast<double>(Shake::kMaxIntensity));
    shake.stop();

    // Sie klingt ab und hoert auf.
    shake.trigger(8.0f, 300, 400, 0.0f);
    shake.update(0, 90.0f);
    const float first = shake.currentIntensity();
    shake.update(200, 90.0f);
    const float middle = shake.currentIntensity();
    check(middle < first, "Staerke nimmt ab");
    shake.update(401, 90.0f);
    check(!shake.active(), "nach Ablauf der Dauer ist Schluss");
    check(shake.currentIntensity() == 0.0f, "und die Staerke ist null");
    check(length(shake.originOffset()) == 0.0f, "kein Versatz mehr");

    // Kein Rollen — die Engine laesst es ausdruecklich aus.
    shake.trigger(8.0f, 300, 400, 0.0f);
    for (int t = 0; t < 300; t += 20) shake.update(t, 90.0f);
    check(std::fabs(shake.pitchOffset()) <= 8.0f, "Nicken im Rahmen");
    check(std::fabs(shake.yawOffset()) <= 8.0f, "Gieren im Rahmen");

    // Der Versatz bleibt immer innerhalb der aktuellen Staerke.
    bool withinBounds = true;
    shake.stop();
    shake.trigger(12.0f, 500, 1000, 0.0f);
    for (int t = 0; t < 1000; t += 7) {
        shake.update(t, 80.0f);
        const Vec3 offset = shake.originOffset();
        const float bound = shake.currentIntensity() + 1e-3f;
        if (std::fabs(offset.x) > bound || std::fabs(offset.y) > bound ||
            std::fabs(offset.z) > bound) withinBounds = false;
    }
    check(withinBounds, "Versatz bleibt innerhalb der Staerke");

    // Gleiche Ausgangslage, gleiches Ergebnis — sonst waeren Vergleiche
    // zwischen zwei Laeufen wertlos.
    Shake a, b;
    a.seed(99); b.seed(99);
    a.trigger(6.0f, 200, 300, 50.0f);
    b.trigger(6.0f, 200, 300, 50.0f);
    a.update(0, 90.0f); b.update(0, 90.0f);
    a.update(100, 90.0f); b.update(100, 90.0f);
    check(a.originOffset().x == b.originOffset().x &&
          a.originOffset().y == b.originOffset().y,
          "gleicher Startwert ergibt gleichen Verlauf");
}

void testPicking() {
    std::cout << "== Anfassen im Bild ==\n";
    using namespace efx::camera;

    // Eine Kamera, die vom Punkt (0,-300,200) auf den Ursprung schaut.
    Orbit orbit;
    orbit.reset(16.0f);
    // Etwas schraeg von oben, damit es kein Sonderfall ist (die Grundstellung
    // blickt seit dem Abgleich mit dem Original waagerecht).
    orbit.orbit(40.0f, -40.0f);
    const Matrix view = orbit.viewMatrix();
    const Matrix projection = orbit.projectionMatrix(16.0f / 9.0f, false);

    constexpr float kWidth = 1600.0f, kHeight = 900.0f;

    // Der Strahl durch die Bildmitte muss den Zielpunkt treffen — darauf
    // schaut die Kamera schliesslich.
    {
        const Ray middle = rayThroughPixel(view, projection, kWidth * 0.5f,
                                           kHeight * 0.5f, kWidth, kHeight);
        check(std::fabs(length(middle.direction) - 1.0f) < 1e-4f,
              "die Richtung ist normiert");
        check(distanceToRay(middle, orbit.target()) < 1.0f,
              "der Strahl durch die Bildmitte trifft den Zielpunkt");
        // Und er beginnt an der Kamera.
        check(length(middle.origin - orbit.position()) < 1.0f,
              "und beginnt am Auge der Kamera");
        std::printf("  Bildmitte: Abstand zum Zielpunkt %.2f Einheiten\n",
                    static_cast<double>(distanceToRay(middle, orbit.target())));
    }

    // Strahlen an den Bildraendern zeigen auseinander.
    {
        const Ray left = rayThroughPixel(view, projection, 0.0f, kHeight * 0.5f,
                                         kWidth, kHeight);
        const Ray right = rayThroughPixel(view, projection, kWidth, kHeight * 0.5f,
                                          kWidth, kHeight);
        check(dot(left.direction, right.direction) < 0.999f,
              "links und rechts zeigen auseinander");
        // Der Zielpunkt liegt zwischen ihnen, also weiter weg von beiden als
        // von der Mitte.
        check(distanceToRay(left, orbit.target()) > 10.0f,
              "und der Zielpunkt liegt nicht mehr auf dem Randstrahl");
    }

    // --- Treffer auf dem Boden -------------------------------------------
    {
        Vec3 hit;
        const Ray middle = rayThroughPixel(view, projection, kWidth * 0.5f,
                                           kHeight * 0.5f, kWidth, kHeight);
        check(intersectGroundPlane(middle, 0.0f, hit), "die Bildmitte trifft den Boden");
        check(std::fabs(hit.z) < 0.01f, "und zwar auf Hoehe null");
        check(length(hit - orbit.target()) < 1.0f, "am Zielpunkt");

        // Ein Strahl nach oben trifft den Boden nicht.
        Ray upward;
        upward.origin = {0.0f, 0.0f, 100.0f};
        upward.direction = {0.0f, 0.0f, 1.0f};
        check(!intersectGroundPlane(upward, 0.0f, hit),
              "nach oben gibt es keinen Bodentreffer");

        // Ein waagerechter Strahl ebenfalls nicht — die Rechnung wuerde einen
        // Punkt in vielen Kilometern liefern, und wer das ignoriert, setzt
        // Dinge ins Unendliche.
        Ray flat;
        flat.origin = {0.0f, 0.0f, 100.0f};
        flat.direction = normalise({1.0f, 0.0f, 0.0f});
        check(!intersectGroundPlane(flat, 0.0f, hit),
              "waagerecht ebenfalls nicht");

        // Eine andere Hoehe trifft entsprechend hoeher — aber nur eine, die
        // UNTER der Kamera liegt. Der erste Anlauf nahm 50 Einheiten an, ohne
        // nachzusehen, wie hoch die Kamera ueberhaupt steht; blickt sie nach
        // unten und liegt die Ebene darueber, gibt es zu Recht keinen Treffer.
        const float eyeHeight = orbit.position().z;
        check(eyeHeight > 1.0f, "die Kamera steht ueber dem Boden");
        const float midHeight = eyeHeight * 0.5f;
        Vec3 high;
        check(intersectGroundPlane(middle, midHeight, high),
              "eine Ebene unter der Kamera wird getroffen");
        check(std::fabs(high.z - midHeight) < 0.01f, "und dort liegt der Punkt");
        check(length(high - orbit.position()) < length(hit - orbit.position()),
              "naeher an der Kamera als der Boden");
        // Und eine Ebene ueber der Kamera nicht.
        Vec3 above;
        check(!intersectGroundPlane(middle, eyeHeight + 100.0f, above),
              "eine Ebene ueber der Kamera nicht");
        std::printf("  Kamera steht %.0f Einheiten hoch\n",
                    static_cast<double>(eyeHeight));
    }

    // --- Abstand zum Strahl ----------------------------------------------
    {
        Ray ray;
        ray.origin = {0.0f, 0.0f, 0.0f};
        ray.direction = {1.0f, 0.0f, 0.0f};
        check(std::fabs(distanceToRay(ray, {50.0f, 0.0f, 0.0f})) < 0.01f,
              "ein Punkt auf dem Strahl hat Abstand null");
        check(std::fabs(distanceToRay(ray, {50.0f, 30.0f, 0.0f}) - 30.0f) < 0.01f,
              "und daneben den senkrechten Abstand");
        // Hinter der Kamera zaehlt der Abstand zum Ursprung — sonst faende
        // man Dinge im Ruecken.
        check(std::fabs(distanceToRay(ray, {-100.0f, 0.0f, 0.0f}) - 100.0f) < 0.01f,
              "hinter dem Ursprung zaehlt der Abstand dorthin");
    }

    // Randfaelle: eine Ansicht ohne Ausdehnung darf nicht durch null teilen.
    {
        const Ray degenerate = rayThroughPixel(view, projection, 10.0f, 10.0f,
                                               0.0f, 0.0f);
        check(std::isfinite(degenerate.direction.x), "Breite null bleibt brauchbar");
    }

    // Umkehrung der Blickmatrix: zweimal angewandt ergibt wieder dasselbe.
    {
        const Matrix inverse = invertRigid(view);
        const Vec3 point{123.0f, -45.0f, 67.0f};
        const Vec3 there = transformPoint(view, point);
        const Vec3 back = transformPoint(inverse, there);
        check(length(back - point) < 0.01f,
              "hin und zurueck ergibt denselben Punkt");
    }
}

void testBillboardFacing() {
    std::cout << "== Billboards zeigen zur Kamera ==\n";

    // Anlass: in den Vorschaukacheln verschwand ein Muzzleflash ganz, waehrend
    // er im Editor richtig aussah. Grund war, dass die Kacheln die Geometrie
    // mit FESTEN Achsen aufbauten und die Kamera danach drehten — ein
    // Billboard ist eine Flaeche ohne Dicke und von der Seite unsichtbar.
    //
    // Ein Effekt aus vielen Teilchen sah dabei nur duenner aus; einer aus
    // einem einzigen Viereck war ganz weg. Deshalb faellt so etwas erst bei
    // bestimmten Dateien auf — und deshalb dieser Test.
    efx::Effect effect;
    effect.primitives.push_back(efx::Primitive{});
    {
        efx::Primitive& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(1.0f);
        p.life = efx::Range::single(1000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(20.0f);
        p.shaders.push_back("gfx/blitz");
    }

    // Fuer mehrere Kamerastellungen: die Flaeche muss immer zur Kamera zeigen.
    const float angles[][2] = {{0.0f, 0.0f}, {30.0f, 20.0f}, {90.0f, 0.0f},
                              {180.0f, 45.0f}, {-60.0f, -30.0f}};
    for (const auto& angle : angles) {
        efx::camera::Orbit view;
        view.reset(16.0f);
        view.orbit(angle[0], angle[1]);
        const auto matrix = view.viewMatrix();

        // Genau wie im Programm: die ersten beiden Zeilen der Blickmatrix.
        const efx::camera::Vec3 right{matrix[0], matrix[4], matrix[8]};
        const efx::camera::Vec3 up{matrix[1], matrix[5], matrix[9]};

        efx::particles::System system;
        system.play(effect, 1u);
        const auto list = system.build(100.0f, right, up);

        size_t vertexCount = 0;
        for (const auto& group : list.byTexture) {
            vertexCount += group.second.vertices.size();
        }
        check(vertexCount >= 4, "das Viereck entsteht");
        if (vertexCount < 4) continue;

        // Die Flaechennormale aus drei Eckpunkten.
        const auto& mesh = list.byTexture.begin()->second;
        const efx::camera::Vec3 a{mesh.vertices[0].pos[0], mesh.vertices[0].pos[1],
                                  mesh.vertices[0].pos[2]};
        const efx::camera::Vec3 b{mesh.vertices[1].pos[0], mesh.vertices[1].pos[1],
                                  mesh.vertices[1].pos[2]};
        const efx::camera::Vec3 c{mesh.vertices[2].pos[0], mesh.vertices[2].pos[1],
                                  mesh.vertices[2].pos[2]};
        const auto normal = efx::camera::normalise(
            efx::camera::cross(b - a, c - a));

        // Die Richtung von der Flaeche zur Kamera.
        const auto toCamera = efx::camera::normalise(view.position() - a);

        // Zeigt die Flaeche zur Kamera, ist der Betrag des Skalarprodukts nahe
        // eins. Steht sie hochkant — der Fehler —, ist er nahe null.
        const float facing = std::fabs(efx::camera::dot(normal, toCamera));
        check(facing > 0.9f, "die Flaeche steht zur Kamera, nicht hochkant");
        std::printf("  Kamera %+4.0f/%+3.0f: Ausrichtung %.3f (1.0 = frontal)\n",
                    static_cast<double>(angle[0]), static_cast<double>(angle[1]),
                    static_cast<double>(facing));
    }

    // Die Gegenprobe: den ganzen Umlauf abfahren und den SCHLECHTESTEN Fall
    // suchen — statt einen Winkel zu raten.
    //
    // Mein erster Anlauf nahm 90 Grad an und erwartete dort den Fehler. Dort
    // stand die Kamera aber zufaellig noch guenstig (0.944), und der Test wurde
    // rot, obwohl die Aussage stimmte. Ein einzelner Winkel beweist nichts;
    // der ungünstigste tut es.
    {
        float worstFixed = 1.0f, worstCamera = 1.0f;
        for (int step = 0; step < 36; ++step) {
            const float yaw = static_cast<float>(step) * 10.0f;
            efx::camera::Orbit view;
            view.reset(16.0f);
            view.orbit(yaw, 0.0f);
            const auto matrix = view.viewMatrix();

            auto facingOf = [&](const efx::camera::Vec3& right,
                                const efx::camera::Vec3& up) {
                efx::particles::System system;
                system.play(effect, 1u);
                const auto list = system.build(100.0f, right, up);
                if (list.byTexture.empty()) return 1.0f;
                const auto& mesh = list.byTexture.begin()->second;
                if (mesh.vertices.size() < 3) return 1.0f;
                const efx::camera::Vec3 a{mesh.vertices[0].pos[0],
                                          mesh.vertices[0].pos[1],
                                          mesh.vertices[0].pos[2]};
                const efx::camera::Vec3 b{mesh.vertices[1].pos[0],
                                          mesh.vertices[1].pos[1],
                                          mesh.vertices[1].pos[2]};
                const efx::camera::Vec3 c{mesh.vertices[2].pos[0],
                                          mesh.vertices[2].pos[1],
                                          mesh.vertices[2].pos[2]};
                const auto normal =
                    efx::camera::normalise(efx::camera::cross(b - a, c - a));
                const auto toCamera =
                    efx::camera::normalise(view.position() - a);
                return std::fabs(efx::camera::dot(normal, toCamera));
            };

            worstCamera = std::min(worstCamera,
                                   facingOf({matrix[0], matrix[4], matrix[8]},
                                            {matrix[1], matrix[5], matrix[9]}));
            worstFixed = std::min(worstFixed,
                                  facingOf({1.0f, 0.0f, 0.0f},
                                           {0.0f, 0.0f, 1.0f}));
        }
        check(worstCamera > 0.9f,
              "mit Kameraachsen bleibt es ueber den ganzen Umlauf frontal");
        check(worstFixed < 0.2f,
              "mit festen Achsen wird es irgendwo hochkant — der Fehler");
        std::printf("  ueber 360 Grad: Kameraachsen mindestens %.3f, "
                    "feste Achsen mindestens %.3f\n",
                    static_cast<double>(worstCamera),
                    static_cast<double>(worstFixed));
    }
}

void testFitDistance() {
    std::cout << "== Abstand zum Einpassen ==\n";
    using namespace efx::camera;

    // Fuer die Kacheln des Browsers: aus der Reichweite eines Effekts den
    // noetigen Kameraabstand ausrechnen, statt ihn ueber Zoomstufen zu raten.
    Orbit view;
    view.reset(16.0f);

    // Doppelte Reichweite, doppelter Abstand — der Zusammenhang ist linear.
    const float small = view.distanceToFit(50.0f);
    const float large = view.distanceToFit(100.0f);
    check(std::fabs(large - small * 2.0f) < 0.01f,
          "doppelte Reichweite, doppelter Abstand");

    // Die Probe aufs Exempel: bei diesem Abstand muss der Rand des Effekts
    // gerade noch ins Bild passen. Die halbe Bildhoehe bei Abstand d ist
    // d * tan(fov/2).
    const float radius = 80.0f;
    const float fitted = view.distanceToFit(radius, 1.0f);   // randvoll
    const float halfHeight =
        fitted * std::tan(view.fieldOfView() * 0.5f * 3.14159265f / 180.0f);
    check(std::fabs(halfHeight - radius) < 0.5f,
          "randvoll heisst: halbe Bildhoehe gleich Reichweite");
    std::printf("  Reichweite %.0f -> Abstand %.1f, halbe Bildhoehe %.1f\n",
                static_cast<double>(radius), static_cast<double>(fitted),
                static_cast<double>(halfHeight));

    // Mit Rand ist der Abstand groesser — sonst klebt der Effekt am Rahmen.
    check(view.distanceToFit(radius, 1.25f) > fitted, "mit Rand weiter weg");

    // Unsinnige Werte.
    check(view.distanceToFit(0.0f) > 0.0f, "Reichweite null wird abgefangen");
    check(view.distanceToFit(-5.0f) > 0.0f, "negative ebenso");

    // Der Setzer laesst die Kamera nicht im Zielpunkt stecken.
    view.setDistance(0.0f);
    check(view.distance() > 0.0f, "Abstand null wird abgefangen");
    check(std::isfinite(view.viewMatrix()[0]), "und die Blickmatrix bleibt gut");
    view.setDistance(1.0e12f);
    check(std::isfinite(view.distance()), "ein riesiger Wert ebenso");
}

void testScene() {
    std::cout << "== Testraum ==\n";
    using namespace efx::scene;

    RoomSize size;
    const float worldScale = 16.0f;

    Mesh room = buildRoom(size, worldScale, rgba(102, 187, 106));
    check(room.vertices.size() == 24, "sechs Flaechen mit je vier Ecken");
    check(room.indices.size() == 36, "und je zwei Dreiecken");
    for (uint16_t index : room.indices) {
        check(index < room.vertices.size(), "kein Index zeigt ins Leere");
    }

    // Die Masse muessen stimmen: halbe Breite, halbe Tiefe, volle Hoehe.
    float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
    for (const auto& v : room.vertices) {
        minX = std::min(minX, v.pos[0]); maxX = std::max(maxX, v.pos[0]);
        minZ = std::min(minZ, v.pos[2]); maxZ = std::max(maxZ, v.pos[2]);
    }
    // Gemessen am Original (Disassembly + Fotos, agentA ROOM-GEOMETRY.md):
    // x -100..100, y -140..140, Boden -20, Decke 60.
    float minY = 1e9f, maxY = -1e9f;
    for (const auto& v : room.vertices) {
        minY = std::min(minY, v.pos[1]);
        maxY = std::max(maxY, v.pos[1]);
    }
    check(std::fabs(minX + 100.0f) < 0.01f && std::fabs(maxX - 100.0f) < 0.01f,
          "Breite wie im Original: x -100..100");
    check(std::fabs(minY + 140.0f) < 0.01f && std::fabs(maxY - 140.0f) < 0.01f,
          "Tiefe wie im Original: y -140..140");
    check(std::fabs(minZ + 20.0f) < 0.01f, "Boden wie im Original bei z = -20");
    check(std::fabs(maxZ - 60.0f) < 0.01f, "Decke wie im Original bei z = 60");
    // Der Raum haengt nicht vom Weltmassstab ab (das Original verschiebt nur
    // die Kamera).
    {
        const Mesh other = buildRoom(size, 10.0f, rgba(102, 187, 106));
        bool same = other.vertices.size() == room.vertices.size();
        for (size_t i = 0; same && i < room.vertices.size(); ++i) {
            for (int k = 0; k < 3; ++k) same &= other.vertices[i].pos[k] == room.vertices[i].pos[k];
        }
        check(same, "Raum bei 10 und 16 Einheiten je Fuss gleich gross");
    }
    std::printf("  Raum %.0f x %.0f x %.0f Einheiten, Boden bei %.0f\n",
                static_cast<double>(maxX - minX), static_cast<double>(maxY - minY),
                static_cast<double>(maxZ - minZ), static_cast<double>(minZ));

    // Wickelrichtung. Jede Flaeche muss nach INNEN zeigen.
    //
    // Das ist die Bedingung dafuer, dass Rueckseitenaussortierung das Richtige
    // tut: von innen sieht man alle sechs Flaechen, von aussen faellt die
    // naechstgelegene weg und man schaut in den Kasten hinein — genau so
    // verhaelt sich der alte Editor.
    //
    // Frueher stand hier nur ein Kommentar, der "nach innen" behauptete. Die
    // Nachrechnung ergab bei allen sechs Flaechen "nach aussen". Seitdem wird
    // es gemessen.
    {
        const efx::camera::Vec3 inside{0.0f, 0.0f, size.centreZ()};
        const char* faceNames[6] = {"Boden", "Decke", "Wand -Y", "Wand +Y",
                                    "Wand -X", "Wand +X"};
        int inward = 0;
        for (int face = 0; face < 6; ++face) {
            // Beide Dreiecke der Flaeche pruefen, nicht nur das erste.
            for (int triangle = 0; triangle < 2; ++triangle) {
                auto corner = [&](int i) {
                    const auto& v = room.vertices[room.indices[face * 6 + triangle * 3 + i]];
                    return efx::camera::Vec3{v.pos[0], v.pos[1], v.pos[2]};
                };
                const efx::camera::Vec3 a = corner(0), b = corner(1), c = corner(2);
                const efx::camera::Vec3 normal =
                    efx::camera::normalise(efx::camera::cross(b - a, c - a));
                const efx::camera::Vec3 middle{(a.x + b.x + c.x) / 3.0f,
                                               (a.y + b.y + c.y) / 3.0f,
                                               (a.z + b.z + c.z) / 3.0f};
                const float towardsInside =
                    efx::camera::dot(normal, efx::camera::normalise(inside - middle));
                check(towardsInside > 0.5f,
                      std::string(faceNames[face]) + " Dreieck " +
                          std::to_string(triangle + 1) + " zeigt nach innen");
                if (towardsInside > 0.5f && triangle == 0) ++inward;
            }
        }
        std::printf("  %d von 6 Flaechen zeigen nach innen\n", inward);
    }

    // Boden, Waende und Decke muessen sich unterscheiden, sonst verschwimmen
    // die Kanten.
    std::set<uint32_t> shades;
    for (const auto& v : room.vertices) shades.insert(v.colour);
    check(shades.size() >= 3, "Boden, Waende und Decke sind verschieden hell");

    // Jedes Thema braucht Himmelsfarben — ohne sie waere der Himmel schwarz,
    // und ein schwarzer Himmel unterscheidet sich nicht von "kein Himmel".
    for (const auto& t : efx::theme::builtinThemes()) {
        const auto& p = t.palette;
        const float horizon = p.skyHorizon.r + p.skyHorizon.g + p.skyHorizon.b;
        const float zenith = p.skyZenith.r + p.skyZenith.g + p.skyZenith.b;
        check(horizon > 0.1f, t.id + ": der Horizont ist nicht schwarz");
        check(zenith > 0.1f, t.id + ": der Zenit auch nicht");
        check(std::fabs(horizon - zenith) > 0.05f,
              t.id + ": Horizont und Zenit unterscheiden sich sichtbar");
        // Der Himmel soll blaeulich sein, nicht in der Wandfarbe — sonst
        // verschwimmt der Horizont mit dem Boden.
        check(p.skyZenith.b >= p.skyZenith.r, t.id + ": der Zenit zieht ins Blaue");
    }

    // Das Gitter: 20 Einheiten je Feld (gemessen; Ravens Anleitung sagt
    // "one foot", das Original zeichnet 20 Einheiten). Ohne Waende auf der
    // grossen Bodenflaeche x -400..400, y -560..560.
    LineSet grid = buildGrid(size, worldScale, rgba(80, 80, 90), rgba(120, 120, 130));
    const size_t expectedLines = static_cast<size_t>(800 / 20 + 1) +
                                 static_cast<size_t>(1120 / 20 + 1);
    check(grid.vertices.size() == expectedLines * 2,
          "eine Linie je 20 Einheiten in beiden Richtungen");

    // Abstand zweier benachbarter Linien nachmessen.
    const float lineX0 = grid.vertices[0].pos[0];
    const float lineX1 = grid.vertices[2].pos[0];
    check(std::fabs((lineX1 - lineX0) - 20.0f) < 0.01f,
          "Gitterabstand ist 20 Einheiten wie im Original");
    std::printf("  Gitterabstand %.1f Einheiten bei %.0f Einheiten/Fuss\n",
                static_cast<double>(lineX1 - lineX0), static_cast<double>(worldScale));

    // Unabhaengig vom Weltmassstab, wie der Raum.
    LineSet grid64 = buildGrid(size, 64.0f, rgba(80, 80, 90), rgba(120, 120, 130));
    check(std::fabs((grid64.vertices[2].pos[0] - grid64.vertices[0].pos[0]) - 20.0f) < 0.01f,
          "und haengt nicht vom Weltmassstab ab");

    // Jede zehnte Linie kraeftiger.
    std::set<uint32_t> gridShades;
    for (const auto& v : grid.vertices) gridShades.insert(v.colour);
    check(gridShades.size() == 2, "Haupt- und Nebenlinien unterscheidbar");

    // Das Gitter darf nicht in der Bodenflaeche liegen, sonst flackert es.
    check(grid.vertices[0].pos[2] > size.floorZ, "Gitter liegt ueber dem Boden");

    // --- Raumarten, Himmel und Sonne --------------------------------------
    {
        const efx::camera::Vec3 sun = efx::camera::normalise({0.4f, -0.3f, 0.8f});

        const Mesh enclosed = buildRoomLit(size, worldScale, rgba(102, 187, 106),
                                           RoomStyle::Enclosed, sun, 0.35f, true);
        const Mesh open = buildRoomLit(size, worldScale, rgba(102, 187, 106),
                                       RoomStyle::OpenSky, sun, 0.35f, true);
        check(enclosed.vertices.size() == 24, "geschlossen: sechs Flaechen");

        // "Draw Room" schaltet nur die WAENDE — der Hilfetext des Originals
        // lautet "Draw the walls of the testing room". Der Boden bleibt.
        // Ich hatte den Knopf zuerst als "ganzer Raum" gelesen.
        const Mesh floorOnly = buildRoomLit(size, worldScale, rgba(102, 187, 106),
                                            RoomStyle::Enclosed, sun, 0.35f, true,
                                            false);
        check(floorOnly.vertices.size() == 4, "ohne Waende bleibt nur der Boden");
        for (const auto& v : floorOnly.vertices) {
            check(std::fabs(v.pos[2] - size.floorZ) < 0.01f, "und der liegt auf Bodenhoehe (-20)");
        }
        std::printf("  ohne Waende: %zu Ecken (nur der Boden)\n",
                    floorOnly.vertices.size());
        check(open.vertices.size() == 4, "draussen: nur die grosse Bodenflaeche");
        check(buildRoomLit(size, worldScale, 0, RoomStyle::None, sun, 0.35f, true)
                  .vertices.empty(), "keine Raumart: nichts");

        // Ohne Raum zeichnet das Original eine grosse Ebene: x -400..400,
        // y -560..560 auf Bodenhoehe (gemessen).
        float openMaxX = 0.0f, openMaxY = 0.0f;
        for (const auto& v : open.vertices) {
            openMaxX = std::max(openMaxX, v.pos[0]);
            openMaxY = std::max(openMaxY, v.pos[1]);
        }
        check(openMaxX == 400.0f && openMaxY == 560.0f, "die Ebene reicht bis 400 / 560");

        // Sonnenlicht.
        check(sunLambert({0, 0, 1}, {0, 0, 1}, 0.0f) > 0.99f,
              "genau zur Sonne: voll beleuchtet");
        check(sunLambert({0, 0, -1}, {0, 0, 1}, 0.0f) < 0.01f,
              "genau weg: unbeleuchtet");
        check(sunLambert({0, 0, -1}, {0, 0, 1}, 0.35f) > 0.34f,
              "aber nie schwarz — der Grundanteil bleibt");
        check(sunLambert({0, 0, 1}, {0, 0, 0}, 0.0f) == 1.0f,
              "ohne Sonnenrichtung volle Helligkeit statt Division durch null");
        float previousLight = 2.0f;
        bool monotone = true;
        for (int deg = 0; deg <= 90; deg += 10) {
            const float rad = static_cast<float>(deg) * 3.14159265f / 180.0f;
            const float value = sunLambert({std::sin(rad), 0.0f, std::cos(rad)},
                                           {0, 0, 1}, 0.2f);
            if (value > previousLight + 1e-4f) monotone = false;
            previousLight = value;
        }
        check(monotone, "je schraeger, desto dunkler");

        const Mesh unlit = buildRoomLit(size, worldScale, rgba(102, 187, 106),
                                        RoomStyle::Enclosed, sun, 0.35f, false);
        bool differs = false;
        for (size_t i = 0; i < enclosed.vertices.size(); ++i) {
            if (enclosed.vertices[i].colour != unlit.vertices[i].colour) differs = true;
        }
        check(differs, "eingeschaltete Sonne aendert die Farben");

        // --- Himmel -------------------------------------------------------
        const Mesh sky = buildSky(4000.0f, sun, rgba(160, 180, 210),
                                  rgba(40, 70, 140), rgba(255, 245, 200));
        check(!sky.vertices.empty(), "der Himmel hat Flaechen");
        check(sky.indices.size() % 3 == 0, "und ganze Dreiecke");
        for (uint16_t index : sky.indices) {
            check(index < sky.vertices.size(), "kein Index zeigt ins Leere");
        }
        bool onSphere = true, aboveGround = true;
        for (const auto& v : sky.vertices) {
            const float distance =
                efx::camera::length({v.pos[0], v.pos[1], v.pos[2]});
            if (std::fabs(distance - 4000.0f) > 1.0f) onSphere = false;
            if (v.pos[2] < -1.0f) aboveGround = false;
        }
        check(onSphere, "alle Punkte liegen auf der Kugel");
        check(aboveGround, "und keiner unter dem Boden");

        uint32_t horizonSample = 0, zenithSample = 0;
        float lowest = 1e9f, highest = -1e9f;
        for (const auto& v : sky.vertices) {
            if (v.pos[2] < lowest) { lowest = v.pos[2]; horizonSample = v.colour; }
            if (v.pos[2] > highest) { highest = v.pos[2]; zenithSample = v.colour; }
        }
        const int horizonBlue = static_cast<int>((horizonSample >> 16) & 0xFF);
        const int zenithBlue = static_cast<int>((zenithSample >> 16) & 0xFF);
        check(horizonBlue != zenithBlue, "Horizont und Zenit sind verschieden");
        std::printf("  Himmel: %zu Flaechen, Horizont Blau %d, Zenit Blau %d\n",
                    sky.indices.size() / 6, horizonBlue, zenithBlue);

        auto brightnessTowards = [&](const efx::camera::Vec3& direction) {
            float best = -2.0f;
            int brightest = 0;
            for (const auto& v : sky.vertices) {
                const efx::camera::Vec3 d =
                    efx::camera::normalise({v.pos[0], v.pos[1], v.pos[2]});
                const float towards = efx::camera::dot(d, direction);
                if (towards > best) {
                    best = towards;
                    brightest = static_cast<int>(v.colour & 0xFF) +
                                static_cast<int>((v.colour >> 8) & 0xFF) +
                                static_cast<int>((v.colour >> 16) & 0xFF);
                }
            }
            return brightest;
        };
        check(brightnessTowards(sun) >
                  brightnessTowards({-sun.x, -sun.y, 0.1f}),
              "um die Sonne herum ist der Himmel heller");
        check(buildSky(0.0f, sun, 0, 0, 0).vertices.empty(),
              "Radius null ergibt keinen Himmel");

        // --- Sonnenscheibe -------------------------------------------------
        const Mesh disc = buildSunDisc(sun, 4000.0f, 120.0f, rgba(255, 250, 220));
        check(disc.vertices.size() == 4, "die Scheibe ist ein Viereck");
        efx::camera::Vec3 centre{};
        for (const auto& v : disc.vertices) {
            centre = centre + efx::camera::Vec3{v.pos[0], v.pos[1], v.pos[2]};
        }
        centre = centre * 0.25f;
        check(efx::camera::dot(efx::camera::normalise(centre), sun) > 0.99f,
              "sie steht in Sonnenrichtung");
        check(efx::camera::length(centre) < 4000.0f,
              "und innerhalb der Kuppel, sonst flackert sie gegen den Himmel");
        check(buildSunDisc({0, 0, 0}, 4000.0f, 100.0f, 0).vertices.empty(),
              "ohne Sonnenrichtung keine Scheibe");
        check(buildSunDisc({0, 0, 1}, 4000.0f, 100.0f, 0).vertices.size() == 4,
              "Sonne im Zenit ergibt trotzdem eine Scheibe");
    }

    // Die Achsen: 16 Einheiten, wie im Original.
    LineSet axes = buildAxes(16.0f, rgba(224, 85, 85), rgba(85, 192, 85),
                             rgba(85, 128, 224));
    check(axes.vertices.size() == 6, "drei Linien");
    check(axes.vertices[1].pos[0] == 16.0f, "X-Achse 16 Einheiten lang");
    check(axes.vertices[3].pos[1] == 16.0f, "Y-Achse");
    check(axes.vertices[5].pos[2] == 16.0f, "Z-Achse");
    check(axes.vertices[0].colour != axes.vertices[2].colour &&
          axes.vertices[2].colour != axes.vertices[4].colour,
          "drei verschiedene Farben");

    // Die Windfahne. Sie weht und wechselt dabei die Richtung — im Original
    // ist der Windvektor keine starre Linie, sondern ein Wimpel am Mast.
    const uint32_t poleColour = rgba(180, 180, 190);
    const uint32_t clothColour = rgba(200, 200, 255);
    // Der Fusspunkt. Standardmaessig der Ursprung — aber die Vorschau setzt
    // ihn an den Rand, und zwar aus zwei Gruenden, die dieselbe Aenderung
    // verlangen:
    //
    //   - im Ursprung entsteht der Effekt, und eine Anzeigehilfe mitten darin
    //     ist keine Hilfe
    //   - der Ursprung ist der Zielpunkt der Umlaufkamera; beim Drehen bleibt
    //     er in der Bildmitte, und die Fahne sieht aus, als haenge sie an der
    //     Kamera
    // Sie steht in der Mitte, aber nicht IM Ursprung.
    //
    // Zwischenzeitlich stand sie am Rand. Dort ist sie schlecht zu sehen und
    // steht bei "Boden und Himmel" im Nichts — deshalb wieder in die Mitte,
    // nur um zwei Fuss versetzt: nah genug, um den Wind am Effekt abzulesen,
    // weit genug, um nicht mitten darin zu stehen.
    const efx::camera::Vec3 foot = windFlagPosition(size, worldScale);
    check(efx::camera::length(foot) > worldScale, "sie steht nicht im Ursprung");
    check(efx::camera::length(foot) < worldScale * 5.0f,
          "aber in der Mitte, nicht am Rand");
    check(std::fabs(foot.x) < size.halfX, "innerhalb der Breite");
    check(std::fabs(foot.y) < size.halfY, "innerhalb der Tiefe");
    std::printf("  Fahne steht bei %.0f / %.0f (Mitte, %.0f Einheiten versetzt)\n",
                static_cast<double>(foot.x), static_cast<double>(foot.y),
                static_cast<double>(efx::camera::length(foot)));

    LineSet flag = buildWindFlag({1.0f, 0.0f, 0.0f}, 48.0f, 0.0f, 1.0f, poleColour,
                                 clothColour, foot);
    check(!flag.vertices.empty(), "die Fahne hat Linien");
    check(flag.vertices.size() % 2 == 0, "Linien kommen paarweise");

    // Sie bewegt sich: zwei verschiedene Zeitpunkte, zwei verschiedene Formen.
    LineSet later = buildWindFlag({1.0f, 0.0f, 0.0f}, 48.0f, 0.4f, 1.0f, poleColour,
                                  clothColour, foot);
    check(later.vertices.size() == flag.vertices.size(),
          "gleich viele Linien zu jedem Zeitpunkt");
    bool moved = false;
    for (size_t i = 0; i < flag.vertices.size(); ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            if (std::fabs(flag.vertices[i].pos[axis] -
                          later.vertices[i].pos[axis]) > 0.01f) moved = true;
        }
    }
    check(moved, "und sie bewegt sich zwischen zwei Zeitpunkten");

    // Der Mast steht still — nur das Tuch weht. Und er steht am Fusspunkt.
    check(flag.vertices[0].pos[0] == foot.x && flag.vertices[0].pos[1] == foot.y &&
              flag.vertices[0].pos[2] == foot.z,
          "der Mast steht am Fusspunkt");
    check(later.vertices[1].pos[0] == flag.vertices[1].pos[0] &&
              later.vertices[1].pos[2] == flag.vertices[1].pos[2],
          "und bleibt ueber die Zeit stehen");

    // Die Fahne kann der Kamera nicht folgen: sie bekommt gar keine. Zweimal
    // dieselbe Zeit muss zweimal dasselbe ergeben.
    {
        const LineSet a = buildWindFlag({1, 0, 0}, 48.0f, 1.234f, 1.0f, poleColour,
                                        clothColour, foot);
        const LineSet b = buildWindFlag({1, 0, 0}, 48.0f, 1.234f, 1.0f, poleColour,
                                        clothColour, foot);
        bool identical = a.vertices.size() == b.vertices.size();
        for (size_t i = 0; identical && i < a.vertices.size(); ++i) {
            for (int k = 0; k < 3; ++k) {
                if (a.vertices[i].pos[k] != b.vertices[i].pos[k]) identical = false;
            }
        }
        check(identical, "gleiche Zeit ergibt gleiche Fahne — keine Kameraabhaengigkeit");
    }

    // Die Richtung schwankt um die Grundrichtung, laeuft aber nicht davon.
    float maxAngle = 0.0f;
    for (float t = 0.0f; t < 60.0f; t += 0.05f) {
        const auto dir = windDirectionAt({1.0f, 0.0f, 0.0f}, t, 1.0f);
        check(std::fabs(efx::camera::length(dir) - 1.0f) < 1e-4f,
              "Windrichtung bleibt normiert");
        const float angle = std::acos(std::clamp(dir.x, -1.0f, 1.0f));
        if (angle > maxAngle) maxAngle = angle;
    }
    check(maxAngle > 0.05f, "die Richtung schwankt spuerbar");
    check(maxAngle < 1.2f, "aber sie laeuft nicht von der Grundrichtung weg");
    std::printf("  Windfahne: %zu Linien, Richtung schwankt bis %.0f Grad\n",
                flag.vertices.size() / 2,
                static_cast<double>(maxAngle * 180.0f / 3.14159265f));

    // Staerkere Boeen schwanken mehr.
    float calmMax = 0.0f, gustyMax = 0.0f;
    for (float t = 0.0f; t < 30.0f; t += 0.05f) {
        const float calm =
            std::acos(std::clamp(windDirectionAt({1, 0, 0}, t, 0.3f).x, -1.0f, 1.0f));
        const float gusty =
            std::acos(std::clamp(windDirectionAt({1, 0, 0}, t, 2.0f).x, -1.0f, 1.0f));
        calmMax = std::max(calmMax, calm);
        gustyMax = std::max(gustyMax, gusty);
    }
    check(gustyMax > calmMax * 2.0f, "starker Wind schwankt deutlich mehr");

    // Randfaelle.
    check(buildWindFlag({0.0f, 0.0f, 0.0f}, 48.0f, 1.0f, 1.0f, poleColour,
                        clothColour).vertices.size() == flag.vertices.size(),
          "Nullrichtung faellt auf eine Ersatzrichtung zurueck statt zu leeren");
    check(buildWindFlag({0.0f, 0.0f, 1.0f}, 48.0f, 1.0f, 1.0f, poleColour,
                        clothColour).vertices.size() == flag.vertices.size(),
          "senkrechter Wind ergibt trotzdem eine Fahne");
    check(buildWindFlag({1.0f, 0.0f, 0.0f}, 0.0f, 1.0f, 1.0f, poleColour,
                        clothColour).vertices.empty(),
          "Laenge null ergibt keine Fahne");
    bool windFinite = true;
    for (float t = 0.0f; t < 20.0f; t += 0.13f) {
        for (const auto& v : buildWindFlag({0, 0, 1}, 48.0f, t, 3.0f, poleColour,
                                           clothColour).vertices) {
            for (int i = 0; i < 3; ++i) {
                if (!std::isfinite(v.pos[i])) windFinite = false;
            }
        }
    }
    check(windFinite, "und ueber zwanzig Sekunden keine unbrauchbaren Zahlen");
}

void testPlayback() {
    std::cout << "== Wiedergabe ==\n";
    using namespace efx::playback;

    Settings settings;
    check(settings.mode == RepeatMode::Once, "einmal abspielen ist voreingestellt");

    // Rate und Frequenz sind derselbe Wert. Im Original sind es zwei Felder
    // mit zwei Schiebereglern; wer sie getrennt speichert, hat frueher oder
    // spaeter zwei verschiedene Wahrheiten. Deshalb nur eine Zahl, die andere
    // wird gerechnet.
    settings.setRate(0.300f);
    check(std::fabs(settings.frequency() - 3.3333f) < 0.001f,
          "0.300 s ergibt 3.333 Ausloesungen je Sekunde");
    std::printf("  Rate %.3f s  <->  Frequenz %.3f /s\n",
                static_cast<double>(settings.repeatRateSeconds),
                static_cast<double>(settings.frequency()));

    settings.setFrequency(4.0f);
    check(std::fabs(settings.repeatRateSeconds - 0.25f) < 1e-5f,
          "und zurueck: 4 je Sekunde ergibt 0.25 s");

    // Hin und her darf nichts verschieben.
    for (int i = 0; i < 20; ++i) {
        settings.setFrequency(settings.frequency());
    }
    check(std::fabs(settings.repeatRateSeconds - 0.25f) < 1e-4f,
          "zwanzigmal hin und her aendert nichts");

    // Randfaelle, an denen eine Kehrwertrechnung gern scheitert.
    settings.setFrequency(0.0f);
    check(settings.repeatRateSeconds == Settings::kMaxRate,
          "Frequenz null wird zur groessten Rate, nicht unendlich");
    settings.setFrequency(-5.0f);
    check(settings.repeatRateSeconds > 0.0f, "negative Frequenz bleibt brauchbar");
    settings.setRate(0.0f);
    check(settings.repeatRateSeconds >= Settings::kMinRate, "Rate null wird begrenzt");
    settings.setRate(1e9f);
    check(settings.repeatRateSeconds <= Settings::kMaxRate, "Rate wird nach oben begrenzt");
    check(std::isfinite(settings.frequency()), "Frequenz bleibt in jedem Fall endlich");

    // Gesamtzahl der Ausloesungen — das "Total repetitions" des Originals.
    Settings counted;
    counted.mode = RepeatMode::Once;
    check(counted.totalRepetitions() == 1, "einmal abspielen: genau eine");
    counted.mode = RepeatMode::UntilStopped;
    check(counted.totalRepetitions() < 0, "bis zum Anhalten: unbestimmt (n/a)");
    counted.mode = RepeatMode::ForSeconds;
    counted.repeatForSeconds = 2.0f;
    counted.setRate(0.5f);
    check(counted.totalRepetitions() == 5,
          "2 Sekunden bei 0.5 s Abstand: 5 Ausloesungen (bei 0.0 faengt es an)");
    counted.respawnEveryFrame = true;
    check(counted.totalRepetitions() < 0,
          "jedes Bild neu: unbestimmt, weil es an der Bildrate haengt");
    counted.respawnEveryFrame = false;
    counted.repeatForSeconds = 0.0f;
    check(counted.totalRepetitions() == 0, "null Sekunden: keine");

    // Bewegter Ursprung.
    Settings moving;
    const efx::camera::Vec3 base{0.0f, 0.0f, 100.0f};
    check(moving.originAt(5.0f, base).z == 100.0f,
          "ohne Bewegung bleibt der Ursprung stehen");

    moving.animateSpawnLocation = true;
    moving.spawnVelocity = {32.0f, 0.0f, 0.0f};
    moving.resetLocationAfter = 1.0f;
    check(std::fabs(moving.originAt(0.5f, base).x - 16.0f) < 0.01f,
          "nach einer halben Sekunde 16 Einheiten weiter");
    // Der Ruecksprung: ohne ihn wandert der Ursprung aus dem Raum, und man
    // sucht den Effekt.
    check(std::fabs(moving.originAt(1.5f, base).x - 16.0f) < 0.01f,
          "nach 1.5 s wieder dieselbe Stelle — der Ruecksprung greift");
    check(std::fabs(moving.originAt(100.25f, base).x - 8.0f) < 0.01f,
          "auch nach hundert Sekunden noch im Raum");
    moving.resetLocationAfter = 0.0f;
    check(std::isfinite(moving.originAt(3.0f, base).x),
          "Ruecksprungzeit null fuehrt nicht zur Division durch null");
    check(std::fabs(moving.originAt(-5.0f, base).x) < 0.01f,
          "negative Zeit wird auf null gezogen");
}

void testSpawnOrigin() {
    std::cout << "== Ursprung des Effekts ==\n";
    using namespace efx::playback;

    efx::scene::RoomSize room;
    const float worldScale = 10.0f;

    // Gemessen am Original (Custom Fx Spawn Origin, 10 Einheiten je Fuss):
    // Default 0/0/0, Raummitte 0/0/20, Boden 0/0/-19, Decke 0/0/59,
    // Wand -99/0/20.
    Origin origin;
    check(efx::camera::length(origin.resolve(room, worldScale)) == 0.0f,
          "Default liegt im Weltursprung");

    origin.mode = OriginMode::OnFloor;
    check(origin.resolve(room, worldScale).z == -19.0f, "auf dem Boden: z = -19 wie im Original");

    origin.mode = OriginMode::OnCeiling;
    check(origin.resolve(room, worldScale).z == 59.0f, "an der Decke: z = 59 wie im Original");

    origin.mode = OriginMode::RoomCentre;
    check(origin.resolve(room, worldScale).z == 20.0f, "Raummitte: z = 20 wie im Original");

    origin.mode = OriginMode::OnWall;
    const auto wall = origin.resolve(room, worldScale);
    check(wall.x == -99.0f && wall.y == 0.0f && wall.z == 20.0f,
          "an der Wand: -99/0/20 wie im Original");

    origin.mode = OriginMode::Custom;
    origin.custom = {12.0f, -34.0f, 56.0f};
    const auto custom = origin.resolve(room, worldScale);
    check(custom.x == 12.0f && custom.y == -34.0f && custom.z == 56.0f,
          "eigene Werte werden unveraendert uebernommen");

    // Der Raum haengt nicht vom Massstab ab, die Lagen also auch nicht.
    origin.mode = OriginMode::OnCeiling;
    check(origin.resolve(room, 64.0f).z == 59.0f, "an der Decke auch bei 64 Einheiten je Fuss");
    check(origin.resolve(room, 0.0f).z == 59.0f, "und bei Massstab null");
}

void testFields() {
    std::cout << "== Reiter und Felder ==\n";
    using namespace efx::fields;

    // Jeder Typ bekommt Reiter, und Generation ist immer dabei.
    const efx::PrimitiveType allTypes[] = {
        efx::PrimitiveType::Particle, efx::PrimitiveType::Line,
        efx::PrimitiveType::Tail, efx::PrimitiveType::Cylinder,
        efx::PrimitiveType::Emitter, efx::PrimitiveType::Sound,
        efx::PrimitiveType::Decal, efx::PrimitiveType::OrientedParticle,
        efx::PrimitiveType::Electricity, efx::PrimitiveType::FxRunner,
        efx::PrimitiveType::Light, efx::PrimitiveType::CameraShake,
        efx::PrimitiveType::ScreenFlash,
    };
    for (auto type : allTypes) {
        const auto tabs = tabsFor(type);
        check(!tabs.empty(), std::string(efx::typeName(type)) + " hat Reiter");
        check(tabs.front() == Tab::Generation,
              std::string(efx::typeName(type)) + ": Generation zuerst");
        // Kein Reiter darf doppelt vorkommen.
        std::set<Tab> unique(tabs.begin(), tabs.end());
        check(unique.size() == tabs.size(),
              std::string(efx::typeName(type)) + ": kein Reiter doppelt");
    }

    // Die Faelle, die aus dem Spielcode belegt sind.
    //
    // CameraShake: theFxHelper.CameraShake(org, mElasticity, mRadius, mLife).
    // Mehr liest die Engine nicht — kein Shader, keine Farbe, keine Bewegung.
    check(applies(efx::PrimitiveType::CameraShake, Field::Bounce),
          "CameraShake liest bounce (das ist die Staerke)");
    check(applies(efx::PrimitiveType::CameraShake, Field::Radius),
          "CameraShake liest radius");
    check(applies(efx::PrimitiveType::CameraShake, Field::Life),
          "CameraShake liest life");
    check(!applies(efx::PrimitiveType::CameraShake, Field::Shaders),
          "CameraShake liest keinen Shader");
    check(!applies(efx::PrimitiveType::CameraShake, Field::Rgb),
          "CameraShake liest keine Farbe");
    check(!applies(efx::PrimitiveType::CameraShake, Field::Velocity),
          "CameraShake liest keine Geschwindigkeit");

    // Der Ursprung gilt fuer JEDEN Typ: der Block "Origin calculations" in
    // FxScheduler.cpp steht vor der Verzweigung nach Typ. Beim CameraShake ist
    // er sogar entscheidend — die Staerke faellt mit dem Abstand zu genau
    // diesem Punkt. Ich hatte ihn zuerst weggelassen, weil in der Verzweigung
    // nur mElasticity, mRadius und mLife stehen.
    for (auto type : allTypes) {
        check(applies(type, Field::Origin),
              std::string(efx::typeName(type)) + " liest den Ursprung");
    }
    const auto shakeTabs = tabsFor(efx::PrimitiveType::CameraShake);
    check(std::find(shakeTabs.begin(), shakeTabs.end(), Tab::OriginSize) !=
              shakeTabs.end(),
          "CameraShake hat den Reiter Origin/Size — wie im Original");
    check(shakeTabs.size() == 3, "und damit drei Reiter, nicht zwei");

    // Weitere Zuordnungen, gemessen aus RT_DIALOG des Originals.
    //
    // Dialog 161 (Physics) enthaelt den Kasten "Bounding Box" mit X/Y/Z —
    // min und max gehoeren also zur Physik, nicht zur Lage.
    check(applies(efx::PrimitiveType::Particle, Field::MinMax),
          "min/max gehoeren zur Physik");
    check(!applies(efx::PrimitiveType::Cylinder, Field::MinMax),
          "ein Cylinder hat keine Physikseite und damit keine Begrenzungsbox");

    // Dialog 179 (Model) enthaelt Angle und Angle Delta als Pitch/Yaw/Roll.
    check(applies(efx::PrimitiveType::Emitter, Field::Angles),
          "Winkel gehoeren zum Modell-Reiter");

    // Dialog 171 (Origin/Size) enthaelt Rotation, nicht Dialog 160 (Motion).
    check(applies(efx::PrimitiveType::Particle, Field::Rotation),
          "Drehung gehoert zu Origin/Size");
    check(!applies(efx::PrimitiveType::Sound, Field::Rotation),
          "ein Klang dreht sich nicht");

    // Dialog 152 (Generation) enthaelt den Kasten "Death Effects".
    check(applies(efx::PrimitiveType::Cylinder, Field::DeathFx),
          "deathfx gilt auch fuer einen Cylinder — er steht in Generation");
    check(!applies(efx::PrimitiveType::CameraShake, Field::DeathFx),
          "aber nicht fuer einen CameraShake");

    // Dialog 176 (Emitter) enthaelt Density und Variance; Dialog 172 nennt
    // variance beim Electricity "Chaos".
    check(applies(efx::PrimitiveType::Emitter, Field::Density),
          "Dichte gehoert zum Emitter");
    check(applies(efx::PrimitiveType::Electricity, Field::Variance),
          "Streuung ist beim Electricity das Chaos");
    check(!applies(efx::PrimitiveType::Particle, Field::Density),
          "ein einfaches Particle hat keine Dichte");

    // Die drei Electricity-Flags teilen sich ihre Bits mit ganz anderen.
    //
    // FX_TAPER == FX_ATTACHED_MODEL, FX_BRANCH == FX_APPLY_PHYSICS,
    // FX_GROW == FX_USE_BBOX. Der Parser kennt keine Schluessel namens taper,
    // branch oder grow — in der Datei stehen sie als useModel, usePhysics und
    // useBBox. Wer das nicht weiss, sucht vergeblich nach "taper".
    check(efx::kFlagElectricityTaper == efx::kFlagAttachedModel,
          "taper und useModel sind dasselbe Bit");
    check(efx::kFlagElectricityBranch == efx::kFlagApplyPhysics,
          "branch und usePhysics sind dasselbe Bit");
    check(efx::kFlagElectricityGrow == efx::kFlagUseBBox,
          "grow und useBBox sind dasselbe Bit");

    // Und die Pruefung erklaert es, statt es als Fehler zu melden.
    efx::Effect boltEffect;
    boltEffect.primitives.push_back(efx::Primitive{});
    efx::Primitive& bolt = boltEffect.primitives.back();
    bolt.type = efx::PrimitiveType::Electricity;
    bolt.flags = efx::kFlagElectricityTaper | efx::kFlagElectricityBranch;
    bolt.shaders.push_back("gfx/misc/electric");
    bolt.life = efx::Range::single(200.0f);
    bool explained = false;
    bool wronglyWarned = false;
    for (const auto& d : efx::validate(boltEffect, efx::Dialect::Both)) {
        if (d.message.find("useModel=taper") != std::string::npos) explained = true;
        if (d.message.find("keine models-Liste") != std::string::npos) {
            wronglyWarned = true;
        }
    }
    check(explained, "die Doppelbelegung wird erklaert");
    check(!wronglyWarned,
          "und useModel wird am Electricity nicht als fehlendes Modell gemeldet");

    // Emitter: gemessen sind sechs Reiter ohne Color. Wir haengen Color an,
    // weil FX_AddEmitter rgb und alpha entgegennimmt — die Engine kann es,
    // Ravens Editor zeigt es nur nicht.
    const auto emitterTabs = tabsFor(efx::PrimitiveType::Emitter);
    check(emitterTabs[2] == Tab::Motion, "Motion an dritter Stelle, wie gemessen");
    check(emitterTabs[3] == Tab::Physics, "dann Physics");

    // Diese beiden Pruefungen standen hier frueher und behaupteten, Cylinder
    // und Emitter haetten eine Drehung. Sie waren falsch: der Spielcode liest
    // mRotation an genau vier Stellen, und keine davon gehoert zu diesen
    // beiden. Im Emitter-Bild ist die Drehung denn auch ausgegraut.
    //
    // Ein Test, der eine falsche Annahme festhaelt, ist schlimmer als keiner —
    // er verteidigt den Fehler gegen die Korrektur. Die richtige Fassung steht
    // weiter unten bei den vier Aufrufstellen.

    // Die feste Seitenreihenfolge.
    //
    // Der Editor haengt nicht je Typ eine eigene Liste an, sondern filtert
    // eine Reihenfolge. Sieben gemessene Typen passen widerspruchsfrei
    // hinein. Der Test haelt die Ordnung fest: kein Typ darf zwei Reiter in
    // umgekehrter Reihenfolge zeigen.
    {
        // Length/Size2 steht VOR Motion. Aufgefallen beim dreizehnten Typ:
        // der Tail ist der einzige, der beide Seiten hat, und bei ihm steht
        // Length/Size2 vor Motion. Zwoelf Typen passten auch zur falschen
        // Reihenfolge, weil sie nie beide gleichzeitig zeigen.
        const Tab pageOrder[] = {
            Tab::Generation, Tab::OriginSize, Tab::LengthSize2, Tab::Line,
            Tab::Motion, Tab::Physics, Tab::Emitter, Tab::Model,
            Tab::Sound, Tab::FxRunner, Tab::CameraShake, Tab::Color,
        };
        auto rank = [&](Tab tab) {
            for (int i = 0; i < 12; ++i) {
                if (pageOrder[i] == tab) return i;
            }
            return 99;
        };
        for (auto type : allTypes) {
            const auto tabs = tabsFor(type);
            bool ordered = true;
            for (size_t i = 1; i < tabs.size(); ++i) {
                if (rank(tabs[i]) <= rank(tabs[i - 1])) ordered = false;
            }
            check(ordered, std::string(efx::typeName(type)) +
                               ": Reiter in der festen Seitenreihenfolge");
        }

        // Die sieben gemessenen Faelle Reiter fuer Reiter.
        struct Expected { efx::PrimitiveType type; std::vector<Tab> tabs; };
        const Expected measured[] = {
            {efx::PrimitiveType::CameraShake,
             {Tab::Generation, Tab::OriginSize, Tab::CameraShake}},
            {efx::PrimitiveType::Cylinder,
             {Tab::Generation, Tab::OriginSize, Tab::LengthSize2, Tab::Color}},
            {efx::PrimitiveType::Decal,
             {Tab::Generation, Tab::OriginSize, Tab::Color}},
            {efx::PrimitiveType::Line,
             {Tab::Generation, Tab::OriginSize, Tab::Line, Tab::Color}},
            {efx::PrimitiveType::Electricity,
             {Tab::Generation, Tab::OriginSize, Tab::Line, Tab::Color}},
            {efx::PrimitiveType::ScreenFlash,
             {Tab::Generation, Tab::OriginSize, Tab::Color}},
        };
        for (const auto& expected : measured) {
            check(tabsFor(expected.type) == expected.tabs,
                  std::string(efx::typeName(expected.type)) +
                      ": Reiter genau wie im Original gemessen");
        }
        // OrientedParticle, gemessen: Generation, Origin/Size, Motion,
        // Physics, Color — genau wie abgeleitet. Damit ist auch die
        // Reiterkombination von Particle und Tail bestaetigt, denn sie folgt
        // derselben Seitenreihenfolge.
        const std::vector<Tab> orientedExpected = {
            Tab::Generation, Tab::OriginSize, Tab::Motion, Tab::Physics, Tab::Color};
        check(tabsFor(efx::PrimitiveType::OrientedParticle) == orientedExpected,
              "OrientedParticle: Reiter wie im Original gemessen");
        // Particle, ebenfalls gemessen: identisch mit OrientedParticle.
        // Beide lesen dieselben Felder, und im Bild ist bei beiden alles
        // aktiv — auch Drehung und Drehaenderung im Motion-Reiter.
        check(tabsFor(efx::PrimitiveType::Particle) == orientedExpected,
              "Particle: Reiter wie im Original gemessen");
        check(applies(efx::PrimitiveType::Particle, Field::Rotation) &&
                  applies(efx::PrimitiveType::Particle, Field::RotationDelta),
              "Particle hat beide Drehfelder");
        for (auto field : {Field::Count, Field::Life, Field::Delay, Field::Origin,
                           Field::Radius, Field::Size, Field::Rgb, Field::Alpha,
                           Field::Velocity, Field::Gravity, Field::Shaders,
                           Field::DeathFx}) {
            check(applies(efx::PrimitiveType::Particle, field),
                  std::string("Particle liest ") + fieldName(field) +
                      " — im Bild ist nichts ausgegraut");
        }

        // Tail, gemessen — der dreizehnte und letzte Typ. Er ist der einzige
        // mit Length/Size2 UND Motion und hat damit die Reihenfolge korrigiert.
        const std::vector<Tab> tailExpected = {
            Tab::Generation, Tab::OriginSize, Tab::LengthSize2, Tab::Motion,
            Tab::Physics, Tab::Color};
        check(tabsFor(efx::PrimitiveType::Tail) == tailExpected,
              "Tail: Reiter wie im Original gemessen, Length/Size2 vor Motion");

        // Und size2 gibt es dort nicht: im Spielcode liest ausschliesslich
        // FX_AddCylinder die drei mSize2-Werte, FX_AddTail nimmt sie nicht
        // entgegen. Im Bild ist die Gruppe entsprechend ausgegraut.
        check(!applies(efx::PrimitiveType::Tail, Field::Size2),
              "Tail hat kein size2 — nur der Cylinder liest es");
        check(applies(efx::PrimitiveType::Tail, Field::Length),
              "die Laenge aber schon");
        check(applies(efx::PrimitiveType::Cylinder, Field::Size2),
              "der Cylinder hat size2");
        // Und die Drehung ist beim Tail im Bild ausgegraut — passt zu den vier
        // Aufrufstellen von mRotation.
        check(!applies(efx::PrimitiveType::Tail, Field::Rotation),
              "Tail dreht sich nicht");

        // Im Motion-Reiter des OrientedParticle sind Drehung und
        // Drehaenderung aktiv, nicht ausgegraut wie beim Emitter.
        check(applies(efx::PrimitiveType::OrientedParticle, Field::Rotation) &&
                  applies(efx::PrimitiveType::OrientedParticle,
                          Field::RotationDelta),
              "beide Drehfelder sind dort aktiv");

        // Beim Emitter weichen wir bewusst ab: die gemessenen sechs plus Color.
        const auto emitter = tabsFor(efx::PrimitiveType::Emitter);
        check(emitter.size() == 7 && emitter.back() == Tab::Color,
              "Emitter: sechs gemessene Reiter plus Color hinten angehaengt");
    }

    // Die Drehung: genau drei Typen lesen mRotation.
    //
    // Der Spielcode hat vier Aufrufstellen, mehr nicht — zweimal
    // FX_AddParticle, einmal CG_ImpactMark fuer den Decal, einmal
    // FX_AddOrientedParticle. Das erklaert die Bilder: beim Emitter ist die
    // Drehung im Motion-Reiter ausgegraut, beim Line fehlt sie ganz.
    check(applies(efx::PrimitiveType::Particle, Field::Rotation), "Particle dreht");
    check(applies(efx::PrimitiveType::OrientedParticle, Field::Rotation),
          "OrientedParticle auch");
    check(applies(efx::PrimitiveType::Decal, Field::Rotation), "und der Decal");
    for (auto type : {efx::PrimitiveType::Line, efx::PrimitiveType::Electricity,
                      efx::PrimitiveType::Cylinder, efx::PrimitiveType::Emitter,
                      efx::PrimitiveType::Tail, efx::PrimitiveType::Light,
                      efx::PrimitiveType::ScreenFlash}) {
        check(!applies(type, Field::Rotation),
              std::string(efx::typeName(type)) + " dreht sich nicht");
    }
    // CG_ImpactMark nimmt nur mRotation, keine Aenderung — ein Decal liegt
    // still auf der Wand.
    check(!applies(efx::PrimitiveType::Decal, Field::RotationDelta),
          "der Decal hat keine Drehaenderung");
    check(applies(efx::PrimitiveType::Particle, Field::RotationDelta),
          "das Particle schon");

    // Light, gemessen: Generation, Origin/Size, Color.
    //
    // Die Reiter stimmten; vier Felder nicht. Ausgegraut sind: Count,
    // "Use even delay distribution", die besonderen Verteilungen, Death
    // Effects, die Shaderliste und die ganze Alphagruppe.
    check(tabsFor(efx::PrimitiveType::Light).size() == 3, "Light hat drei Reiter");
    check(!applies(efx::PrimitiveType::Light, Field::Count),
          "Light hat keine Anzahl — FX_AddLight setzt genau ein Licht");
    check(!applies(efx::PrimitiveType::Light, Field::Radius),
          "und keine Kugel- oder Zylinderverteilung");
    check(!applies(efx::PrimitiveType::Light, Field::DeathFx),
          "und keine Ende-Effekte");
    check(!applies(efx::PrimitiveType::Light, Field::Shaders),
          "und keinen Shader");
    check(!applies(efx::PrimitiveType::Light, Field::Alpha),
          "und kein Alpha — es mischt ueber die Farbe");
    check(applies(efx::PrimitiveType::Light, Field::Rgb), "Farbe aber schon");
    check(applies(efx::PrimitiveType::Light, Field::Size), "und Groesse");
    check(applies(efx::PrimitiveType::Light, Field::Life), "und Lebensdauer");

    // Beim CameraShake ist "Count" ebenfalls ausgegraut. Das stand schon im
    // ersten Bild, und ich hatte es uebersehen — dieselbe Begruendung: er
    // ruettelt einmal, mehrere uebereinander waeren sinnlos.
    check(!applies(efx::PrimitiveType::CameraShake, Field::Count),
          "CameraShake hat keine Anzahl");

    // FxRunner, gemessen: Generation, Origin/Size, FxRunner.
    //
    // Die Reiter stimmten schon; drei Felder nicht. Im Bild sind "Life (ms)",
    // die ganze Gruppe "Size/Width" und "Death Effects" ausgegraut. Er loest
    // andere Effekte aus und zeichnet selbst nichts — PlayEffect liest weder
    // Lebensdauer noch Groesse.
    check(tabsFor(efx::PrimitiveType::FxRunner).size() == 3,
          "FxRunner hat drei Reiter");
    check(!applies(efx::PrimitiveType::FxRunner, Field::Life),
          "FxRunner hat keine Lebensdauer");
    check(!applies(efx::PrimitiveType::FxRunner, Field::Size),
          "und keine Groesse");
    check(!applies(efx::PrimitiveType::FxRunner, Field::DeathFx),
          "und keine Ende-Effekte");
    check(applies(efx::PrimitiveType::FxRunner, Field::Count),
          "Anzahl und Verzoegerung aber schon");
    check(applies(efx::PrimitiveType::FxRunner, Field::Delay), "Verzoegerung");
    check(applies(efx::PrimitiveType::FxRunner, Field::Radius),
          "die besonderen Verteilungen sind nicht ausgegraut");
    check(applies(efx::PrimitiveType::FxRunner, Field::PlayFx),
          "und playfx natuerlich");

    // Die Pruefung und die Feldtabelle muessen sich einig sein.
    //
    // Frueher hatte validate.cpp eigene Funktionen dafuer. Als die Tabelle
    // nach einer Messung genauer wurde, wichen sie ab — die Pruefung hielt
    // einen Decal fuer lebensdauerlos, die Tabelle nicht. Zwei Quellen fuer
    // dieselbe Wahrheit laufen frueher oder spaeter auseinander.
    for (auto type : allTypes) {
        efx::Effect single;
        single.primitives.push_back(efx::Primitive{});
        single.primitives.back().type = type;
        // Nur pruefen, dass keine Meldung ueber ein Feld kommt, das die
        // Tabelle fuer diesen Typ gar nicht vorsieht.
        for (const auto& d : efx::validate(single, efx::Dialect::Both)) {
            if (d.message.find("kein life gesetzt") != std::string::npos) {
                check(applies(type, Field::Life),
                      std::string(efx::typeName(type)) +
                          ": life wird nur bemaengelt, wenn es der Typ liest");
            }
        }
    }

    // Sound, gemessen: Generation, Origin/Size, Sound.
    //
    // Ausgegraut sind Life, Death Effects, die besonderen Verteilungen und die
    // ganze Gruppe Size/Width. Aktiv sind Delay, Count, die gleichmaessige
    // Verteilung, die Sichtweite und der Ursprung.
    check(tabsFor(efx::PrimitiveType::Sound).size() == 3, "Sound hat drei Reiter");
    check(!applies(efx::PrimitiveType::Sound, Field::Life),
          "ein Klang hat keine eigene Lebensdauer");
    check(!applies(efx::PrimitiveType::Sound, Field::Size), "und keine Groesse");
    check(!applies(efx::PrimitiveType::Sound, Field::Radius),
          "und keine Kugel- oder Zylinderverteilung — er wird an einer Stelle "
          "abgespielt");
    check(!applies(efx::PrimitiveType::Sound, Field::DeathFx),
          "und keine Ende-Effekte");
    check(applies(efx::PrimitiveType::Sound, Field::Count),
          "eine Anzahl aber schon");
    check(applies(efx::PrimitiveType::Sound, Field::Delay), "und eine Verzoegerung");
    check(applies(efx::PrimitiveType::Sound, Field::Origin),
          "und einen Ursprung — fuer die Panoramierung");
    check(applies(efx::PrimitiveType::Sound, Field::Sounds), "und die Klangliste");

    // Flash, gemessen: Generation, Origin/Size, Color.
    //
    // Ich hatte Origin/Size weggelassen, weil ein bildschirmfuellendes
    // Aufblitzen keine Lage zu brauchen scheint. Es braucht sie doch — die
    // Staerke haengt vom Abstand ab, wie beim CameraShake.
    const auto flashTabs = tabsFor(efx::PrimitiveType::ScreenFlash);
    check(flashTabs.size() == 3, "Flash hat drei Reiter");
    check(flashTabs[1] == Tab::OriginSize, "darunter Origin/Size");
    check(flashTabs[2] == Tab::Color, "und Color");
    check(applies(efx::PrimitiveType::ScreenFlash, Field::Shaders),
          "Flash hat eine Shaderliste");
    // Im Bild ausgegraut: Death Effects und die besonderen Verteilungen.
    check(!applies(efx::PrimitiveType::ScreenFlash, Field::DeathFx),
          "Flash hat keine Ende-Effekte — im Original ausgegraut");
    check(!applies(efx::PrimitiveType::ScreenFlash, Field::Radius),
          "und keine Kugel- oder Zylinderverteilung");
    check(!applies(efx::PrimitiveType::ScreenFlash, Field::Rotation),
          "ein Aufblitzen dreht sich nicht");

    // Schwerkraft und Wind stehen in KEINER Seite des Originals, obwohl die
    // Engine sie liest. Wir zeigen sie im Bewegungs-Reiter.
    check(applies(efx::PrimitiveType::Particle, Field::Gravity),
          "Schwerkraft gehoert zur Bewegung");
    check(!applies(efx::PrimitiveType::Cylinder, Field::Gravity),
          "ein Cylinder bewegt sich nicht");

    // Sound: nur mMediaHandles und die Abschwaechung.
    check(applies(efx::PrimitiveType::Sound, Field::Sounds), "Sound liest sounds");
    check(!applies(efx::PrimitiveType::Sound, Field::Shaders),
          "Sound liest keinen Shader");
    check(!applies(efx::PrimitiveType::Sound, Field::Rgb), "Sound liest keine Farbe");

    // FxRunner: PlayEffect(mPlayFxHandles, org, ax).
    check(applies(efx::PrimitiveType::FxRunner, Field::PlayFx),
          "FxRunner liest playfx");
    check(!applies(efx::PrimitiveType::FxRunner, Field::Shaders),
          "FxRunner liest keinen Shader");

    // Light: FX_AddLight(org, size..., rgb..., life). Kein Alpha.
    check(applies(efx::PrimitiveType::Light, Field::Rgb), "Light liest rgb");
    check(applies(efx::PrimitiveType::Light, Field::Size), "Light liest size");
    check(!applies(efx::PrimitiveType::Light, Field::Alpha),
          "Light liest kein Alpha — es mischt ueber die Farbe");
    check(!applies(efx::PrimitiveType::Light, Field::Shaders),
          "Light liest keinen Shader");

    // Nur Line und Electricity haben einen zweiten Endpunkt.
    check(applies(efx::PrimitiveType::Line, Field::Origin2), "Line liest origin2");
    check(applies(efx::PrimitiveType::Electricity, Field::Origin2),
          "Electricity liest origin2");
    check(!applies(efx::PrimitiveType::Particle, Field::Origin2),
          "Particle nicht");

    // size2 nur beim Cylinder.
    check(applies(efx::PrimitiveType::Cylinder, Field::Size2), "Cylinder liest size2");
    check(!applies(efx::PrimitiveType::Particle, Field::Size2), "Particle nicht");

    // Der eigentliche Nutzen: melden, was jemand ausgefuellt hat, das nie
    // gelesen wird. Das ist die haeufigste Art, Zeit zu verlieren — man dreht
    // an einer Zahl, die niemand liest.
    efx::Primitive shake;
    shake.type = efx::PrimitiveType::CameraShake;
    shake.rgb.present = true;
    shake.velocity.set = true;
    shake.shaders.push_back("gfx/misc/test");
    const auto useless = uselessFields(shake);
    check(useless.size() == 3, "drei wirkungslose Felder am CameraShake erkannt");
    std::printf("  wirkungslos an einem CameraShake:");
    for (auto field : useless) std::printf(" %s", fieldName(field));
    std::printf("\n");

    efx::Primitive clean;
    clean.type = efx::PrimitiveType::Particle;
    clean.rgb.present = true;
    clean.velocity.set = true;
    clean.shaders.push_back("gfx/misc/test");
    check(uselessFields(clean).empty(),
          "an einem Particle ist dasselbe voellig in Ordnung");

    // Jeder Reiter hat einen Namen und eine Uebersetzungskennung.
    for (auto type : allTypes) {
        for (auto tab : tabsFor(type)) {
            check(std::string(tabName(tab)).size() > 0, "Reiter hat einen Namen");
            check(std::string(efx::i18n::tr(tabLabel(tab))).size() > 0,
                  "Reiter hat einen uebersetzten Namen");
        }
    }

    std::printf("  Reiter je Typ:");
    for (auto type : {efx::PrimitiveType::Particle, efx::PrimitiveType::Sound,
                      efx::PrimitiveType::CameraShake}) {
        std::printf("  %s=%zu", efx::typeName(type), tabsFor(type).size());
    }
    std::printf("\n");
}

void testAssets() {
    std::cout << "== Materialbestand ==\n";
    namespace fs = std::filesystem;
    using namespace efx::assets;

    // Zuordnung nach Endung.
    check(classify("shaders/effects.shader") == Kind::Shader, "shader erkannt");
    check(classify("effects/explosions/big.efx") == Kind::Effect, "efx erkannt");
    check(classify("models/weapons2/blaster.md3") == Kind::Model, "md3 erkannt");
    check(classify("models/players/kyle.glm") == Kind::Model, "glm erkannt");
    check(classify("sound/weapons/fire.wav") == Kind::Sound, "wav erkannt");
    check(classify("sound/music/theme.mp3") == Kind::Sound, "mp3 erkannt");
    check(classify("gfx/misc/spark.tga") == Kind::Texture, "tga erkannt");
    check(classify("GFX/MISC/SPARK.TGA") == Kind::Texture,
          "Grossbuchstaben ebenso");
    check(classify("readme.txt") == Kind::Ignore, "txt wird ignoriert");
    check(classify("noextension") == Kind::Ignore, "ohne Endung ignoriert");
    // Ein Punkt im Ordnernamen darf nicht als Endung durchgehen.
    check(classify("gfx/v1.2/file") == Kind::Ignore,
          "Punkt im Ordnernamen ist keine Endung");

    // Wie ein Name in einer .efx-Datei steht.
    check(normaliseName("GFX/Misc/Spark.tga", Kind::Texture) == "gfx/misc/spark",
          "Textur: klein, ohne Endung");
    check(normaliseName("effects/explosions/big.efx", Kind::Effect) ==
              "explosions/big",
          "Effekt: ohne effects/ und ohne .efx");
    check(normaliseName("sound\\weapons\\fire.wav", Kind::Sound) ==
              "sound/weapons/fire.wav",
          "Klang: mit Endung, Schraegstriche gedreht");
    check(normaliseName("/models/a.md3", Kind::Model) == "models/a.md3",
          "fuehrender Schraegstrich faellt weg");

    // --- Zip-Verzeichnis -------------------------------------------------
    const fs::path root = fs::temp_directory_path() / "efxed_assets_test";
    fs::remove_all(root);
    fs::create_directories(root);

    // Kaputte Dateien duerfen nicht zum Absturz fuehren. Sie liegen im
    // Spielordner, und da kommt alles Moegliche her.
    std::string error;
    check(readZipDirectory((root / "gibtsnicht.pk3").string(), &error).empty(),
          "nicht vorhandene Datei liefert nichts");
    check(!error.empty(), "und eine Begruendung");

    { std::ofstream(root / "leer.pk3"); }
    error.clear();
    check(readZipDirectory((root / "leer.pk3").string(), &error).empty(),
          "leere Datei liefert nichts");

    { std::ofstream(root / "kurz.pk3") << "PK"; }
    check(readZipDirectory((root / "kurz.pk3").string()).empty(),
          "zwei Byte liefern nichts");

    {   // Kein Zip, nur Text.
        std::ofstream(root / "text.pk3") << std::string(500, 'x');
    }
    error.clear();
    check(readZipDirectory((root / "text.pk3").string(), &error).empty(),
          "eine Textdatei mit .pk3 liefert nichts");
    check(error.find("end record") != std::string::npos,
          "und sagt, was fehlt");

    {   // Gueltiges Endrecord, aber das Verzeichnis zeigt aus der Datei heraus.
        // Genau der Fall, mit dem man einen Leser zum Absturz bringt.
        std::ofstream file(root / "boese.pk3", std::ios::binary);
        const unsigned char record[22] = {
            'P', 'K', 0x05, 0x06, 0, 0, 0, 0, 1, 0, 1, 0,
            0x10, 0x00, 0x00, 0x00,          // Verzeichnisgroesse 16
            0xF0, 0xFF, 0xFF, 0xFF,          // Offset weit hinter dem Dateiende
            0, 0};
        file.write(reinterpret_cast<const char*>(record), sizeof(record));
    }
    error.clear();
    check(readZipDirectory((root / "boese.pk3").string(), &error).empty(),
          "ein Verzeichnis ausserhalb der Datei wird abgewiesen");
    check(error.find("outside") != std::string::npos, "mit klarer Begruendung");

    // --- Ein echter Suchlauf ---------------------------------------------
    const fs::path base = root / "base";
    fs::create_directories(base / "shaders");
    fs::create_directories(base / "effects" / "explosions");
    fs::create_directories(base / "models" / "weapons2");
    fs::create_directories(base / "sound" / "weapons");
    fs::create_directories(base / "gfx" / "misc");

    { std::ofstream(base / "shaders" / "test.shader")
          << "gfx/misc/spark\n{\n\tcull disable\n\t{\n\t\tmap gfx/misc/spark.tga\n"
             "\t\tblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n\t}\n}\n"
             "gfx/misc/glow\n{\n\t{\n\t\tmap gfx/misc/glow.tga\n\t}\n}\n"; }
    { std::ofstream(base / "effects" / "explosions" / "big.efx") << "x"; }
    { std::ofstream(base / "models" / "weapons2" / "blaster.md3") << "x"; }
    { std::ofstream(base / "sound" / "weapons" / "fire.wav") << "x"; }
    { std::ofstream(base / "gfx" / "misc" / "spark.tga") << "x"; }
    { std::ofstream(base / "readme.txt") << "x"; }

    Index index = scan(base.string());
    check(index.shaders.size() == 2, "beide Shadernamen aus der .shader-Datei");
    check(index.hasShader("gfx/misc/spark"), "und einer davon ist auffindbar");
    check(index.hasShader("GFX/MISC/SPARK"),
          "Gross-/Kleinschreibung egal, wie im Spiel");
    check(!index.hasShader("gibtsnicht"), "ein erfundener Name nicht");
    check(index.effects.size() == 1 && index.effects[0] == "explosions/big",
          "Effekt ohne effects/ und ohne .efx");
    check(index.models.size() == 1 && index.models[0] == "models/weapons2/blaster.md3",
          "Modell mit Pfad und Endung");
    check(index.sounds.size() == 1, "ein Klang");
    check(index.textures.size() == 1 && index.textures[0] == "gfx/misc/spark",
          "Textur ohne Endung");
    check(index.shaderFilesRead == 1, "eine .shader-Datei gelesen");
    std::printf("  ausgepackt: %zu Shader, %zu Texturen, %zu Modelle, %zu Klaenge, "
                "%zu Effekte\n",
                index.shaders.size(), index.textures.size(), index.models.size(),
                index.sounds.size(), index.effects.size());

    // --- Gepackt: der Normalfall bei einer echten Installation ----------
    //
    // Ein .pk3 ist eine Zip-Datei. Wir lesen nur das Inhaltsverzeichnis am
    // Ende — fuer eine Liste der vorhandenen Namen reicht das, und es kostet
    // einen Bruchteil der Zeit, die das Entpacken braeuchte.
    const fs::path packedBase = root / "packed";
    fs::create_directories(packedBase);
    {
        // Zip von Hand bauen: gespeicherte Eintraege ohne Kompression, damit
        // der Test nichts ausser der Standardbibliothek braucht.
        struct Entry { std::string name; std::string content; };
        const Entry entries[] = {
            {"models/players/kyle.glm", "x"},
            {"sound/weapons/blaster/fire.wav", "x"},
            {"effects/blaster/wall_impact.efx", "x"},
            {"gfx/effects/spark.jpg", "x"},
            {"shaders/effects.shader", "gfx/effects/spark\n{\n}\n"},
            {"models/", ""},  // Ordnereintrag, muss weggelassen werden
        };

        auto put16 = [](std::string& out, std::uint16_t v) {
            out += static_cast<char>(v & 0xFF);
            out += static_cast<char>((v >> 8) & 0xFF);
        };
        // uint32_t, nicht unsigned long: auf Windows ist unsigned long
        // 32 Bit, auf Linux 64 — und aus size_t hineinzugeben warnt dann.
        // Ein Zip-Feld ist genau vier Byte breit, also sagen wir das auch.
        auto put32 = [](std::string& out, std::uint32_t v) {
            for (int i = 0; i < 4; ++i) out += static_cast<char>((v >> (i * 8)) & 0xFF);
        };

        std::string local, directory;
        for (const auto& entry : entries) {
            const std::uint32_t offset = static_cast<std::uint32_t>(local.size());
            local += "PK\x03\x04";
            put16(local, static_cast<std::uint16_t>(20)); put16(local, static_cast<std::uint16_t>(0)); put16(local, static_cast<std::uint16_t>(0));
            put16(local, static_cast<std::uint16_t>(0)); put16(local, static_cast<std::uint16_t>(0));
            put32(local, 0);  // CRC — beim Lesen des Verzeichnisses egal
            put32(local, static_cast<std::uint32_t>(entry.content.size()));
            put32(local, static_cast<std::uint32_t>(entry.content.size()));
            put16(local, static_cast<std::uint16_t>(entry.name.size()));
            put16(local, static_cast<std::uint16_t>(0));
            local += entry.name;
            local += entry.content;

            directory += "PK\x01\x02";
            put16(directory, static_cast<std::uint16_t>(20)); put16(directory, static_cast<std::uint16_t>(20)); put16(directory, static_cast<std::uint16_t>(0));
            put16(directory, static_cast<std::uint16_t>(0)); put16(directory, static_cast<std::uint16_t>(0)); put16(directory, static_cast<std::uint16_t>(0));
            put32(directory, 0);
            put32(directory, static_cast<std::uint32_t>(entry.content.size()));
            put32(directory, static_cast<std::uint32_t>(entry.content.size()));
            put16(directory, static_cast<std::uint16_t>(entry.name.size()));
            put16(directory, static_cast<std::uint16_t>(0)); put16(directory, static_cast<std::uint16_t>(0));
            put16(directory, static_cast<std::uint16_t>(0)); put16(directory, static_cast<std::uint16_t>(0));
            put32(directory, 0);
            put32(directory, static_cast<std::uint32_t>(offset));
            directory += entry.name;
        }
        const std::uint16_t count = sizeof(entries) / sizeof(entries[0]);
        std::string end = "PK\x05\x06";
        put16(end, static_cast<std::uint16_t>(0)); put16(end, static_cast<std::uint16_t>(0));
        put16(end, count); put16(end, count);
        put32(end, static_cast<std::uint32_t>(directory.size()));
        put32(end, static_cast<std::uint32_t>(local.size()));
        put16(end, static_cast<std::uint16_t>(0));

        std::ofstream file(packedBase / "assets0.pk3", std::ios::binary);
        file << local << directory << end;
    }

    const auto names = readZipDirectory((packedBase / "assets0.pk3").string());
    check(names.size() == 5, "fuenf Dateien im Verzeichnis, der Ordner faellt weg");

    Index packed = scan(packedBase.string());
    check(packed.pk3Count == 1, "ein Archiv gefunden");
    check(packed.models.size() == 1 && packed.models[0] == "models/players/kyle.glm",
          "Modell aus dem Archiv");
    check(packed.sounds.size() == 1, "Klang aus dem Archiv");
    check(packed.effects.size() == 1 &&
              packed.effects[0] == "blaster/wall_impact",
          "Effekt aus dem Archiv, ohne effects/ und ohne .efx");
    check(packed.textures.size() == 1 && packed.textures[0] == "gfx/effects/spark",
          "Textur aus dem Archiv");
    // Shader IM Archiv werden jetzt gelesen — das ging bis zum Auspacker
    // nicht, weil ihre Namen im Dateiinhalt stehen.
    //
    // Das Testarchiv ist von Hand gebaut und speichert unkomprimiert
    // (Methode 0); der komprimierte Weg wird gleich darunter geprueft.
    check(packed.shaders.size() == 1 && packed.shaders[0] == "gfx/effects/spark",
          "der Shader aus dem Archiv wurde gelesen");
    check(packed.shaderFilesRead == 1, "eine Shaderdatei aus dem Archiv");

    // Einzelne Datei herausholen.
    {
        std::string zipError;
        const auto bytes = readFromZip((packedBase / "assets0.pk3").string(),
                                       "shaders/effects.shader", &zipError);
        check(!bytes.empty(), "eine Datei laesst sich herausholen");
        const std::string text(bytes.begin(), bytes.end());
        check(text.find("gfx/effects/spark") != std::string::npos,
              "und der Inhalt stimmt");

        // Gross-/Kleinschreibung ist egal, wie im Spiel.
        check(!readFromZip((packedBase / "assets0.pk3").string(),
                           "SHADERS/EFFECTS.SHADER").empty(),
              "Gross-/Kleinschreibung wird ignoriert");

        zipError.clear();
        check(readFromZip((packedBase / "assets0.pk3").string(),
                          "gibtsnicht.txt", &zipError).empty(),
              "ein fehlender Name liefert nichts");
        check(zipError.find("not found") != std::string::npos, "mit Begruendung");
    }
    std::printf("  gepackt: %d Archiv, %zu Modelle, %zu Klaenge, %zu Effekte, "
                "%zu Texturen\n",
                packed.pk3Count, packed.models.size(), packed.sounds.size(),
                packed.effects.size(), packed.textures.size());

    // Mit Arbeitsverteiler dasselbe Ergebnis wie ohne.
    efx::jobs::Pool pool;
    Index parallel = scan(packedBase.string(), &pool);
    check(parallel.models == packed.models && parallel.effects == packed.effects,
          "verteilt gelesen kommt dasselbe heraus");

    // --- Ein wirklich komprimiertes Archiv --------------------------------
    //
    // Das obige ist von Hand gebaut und speichert unkomprimiert. Bei einer
    // echten Installation ist alles deflate-komprimiert, und genau darauf
    // kommt es an.
    //
    // Das Archiv steht als Bytefolge im Test, nicht als vorbereitete Datei —
    // derselbe Fehler wie beim Auspacker: was ich hier ablege, gibt es
    // anderswo nicht.
    {
// Ein vollstaendiges, deflate-komprimiertes Zip als Bytefolge.
        const unsigned char kCompressedZip[] = {
        80, 75, 3, 4, 20, 0, 0, 0, 8, 0, 0, 0, 0, 0, 58, 51,
        241, 8, 58, 0, 0, 0, 208, 7, 0, 0, 21, 0, 0, 0, 115, 104,
        97, 100, 101, 114, 115, 47, 112, 97, 99, 107, 101, 100, 46, 115, 104, 97,
        100, 101, 114, 75, 79, 171, 208, 47, 72, 76, 206, 78, 77, 209, 207, 207,
        75, 229, 170, 230, 226, 4, 34, 206, 220, 196, 2, 133, 116, 20, 25, 189,
        146, 244, 68, 46, 206, 90, 174, 90, 174, 244, 81, 29, 163, 58, 70, 117,
        140, 234, 24, 213, 49, 170, 99, 84, 199, 32, 211, 1, 0, 80, 75, 3,
        4, 20, 0, 0, 0, 8, 0, 0, 0, 0, 0, 241, 59, 190, 83, 22,
        0, 0, 0, 160, 15, 0, 0, 17, 0, 0, 0, 109, 111, 100, 101, 108,
        115, 47, 112, 97, 99, 107, 101, 100, 46, 109, 100, 51, 237, 193, 49, 1,
        0, 0, 0, 194, 160, 132, 235, 95, 199, 24, 62, 64, 1, 0, 0, 0,
        111, 3, 80, 75, 1, 2, 20, 0, 20, 0, 0, 0, 8, 0, 0, 0,
        0, 0, 58, 51, 241, 8, 58, 0, 0, 0, 208, 7, 0, 0, 21, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        115, 104, 97, 100, 101, 114, 115, 47, 112, 97, 99, 107, 101, 100, 46, 115,
        104, 97, 100, 101, 114, 80, 75, 1, 2, 20, 0, 20, 0, 0, 0, 8,
        0, 0, 0, 0, 0, 241, 59, 190, 83, 22, 0, 0, 0, 160, 15, 0,
        0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 109,
        0, 0, 0, 109, 111, 100, 101, 108, 115, 47, 112, 97, 99, 107, 101, 100,
        46, 109, 100, 51, 80, 75, 5, 6, 0, 0, 0, 0, 2, 0, 2, 0,
        130, 0, 0, 0, 178, 0, 0, 0, 0, 0,
        };

        const fs::path realBase = root / "compressed";
        fs::create_directories(realBase);
        {
            std::ofstream file(realBase / "assets1.pk3", std::ios::binary);
            file.write(reinterpret_cast<const char*>(kCompressedZip),
                       sizeof(kCompressedZip));
        }

        const Index real = scan(realBase.string());
        check(real.pk3Count == 1, "das komprimierte Archiv wurde gefunden");
        check(real.shaders.size() == 1 && real.shaders[0] == "gfx/packed/one",
              "der Shader daraus wurde ausgepackt und gelesen");
        check(real.models.size() == 1, "das Modell steht im Verzeichnis");

        const auto bytes =
            readFromZip((realBase / "assets1.pk3").string(), "models/packed.md3");
        check(bytes.size() == 4000, "eine komprimierte Datei kommt vollstaendig");
        bool intact = !bytes.empty();
        for (unsigned char byte : bytes) {
            if (byte != 'M') intact = false;
        }
        check(intact, "und unveraendert");
        std::printf("  komprimiertes Archiv: Shader gelesen, %zu Byte ausgepackt\n",
                    bytes.size());

    // --- Ein einzelnes Archiv oeffnen --------------------------------------
    //
    // Fuer "Archiv oeffnen": man hat eine Mod-Datei bekommen und will
    // hineinsehen, ohne sie zu entpacken oder den Spielpfad umzustellen.
    {
        const fs::path lone = root / "einzeln";
        fs::create_directories(lone);
        {
            // Dasselbe von Hand gebaute Archiv wie oben.
            std::ofstream file(lone / "mod.pk3", std::ios::binary);
            file.write(reinterpret_cast<const char*>(kCompressedZip),
                       sizeof(kCompressedZip));
        }
        const Index only = scanArchive((lone / "mod.pk3").string());
        check(only.pk3Count == 1, "ein Archiv");
        check(only.shaders.size() == 1, "sein Shader wurde gelesen");
        check(only.models.size() == 1, "und sein Modell gefunden");
        check(!only.archives.empty(), "der Pfad zum Archiv ist gemerkt");

        // Eine Datei, die kein Archiv ist, meldet sich statt zu schweigen.
        { std::ofstream(lone / "kaputt.pk3") << "kein Archiv"; }
        const Index broken = scanArchive((lone / "kaputt.pk3").string());
        check(broken.effects.empty(), "aus einer kaputten Datei kommt nichts");
        check(!broken.notes.empty(), "und es steht eine Begruendung dabei");
        check(!scanArchive("gibt/es/nicht.pk3").notes.empty(),
              "eine fehlende Datei ebenso");
    }


    // --- Von einem Effektnamen zur .efx-Datei -----------------------------
    //
    // In einer .efx-Datei steht der Name ohne "effects/" und ohne Endung —
    // auf der Platte liegt er darunter. Emitter und FxRunner brauchen das.
    {
        const fs::path fxBase = root / "childfx";
        fs::create_directories(fxBase / "effects" / "explosions");
        { std::ofstream(fxBase / "effects" / "explosions" / "big.efx") << "x"; }

        const Index fxIndex = scan(fxBase.string());
        const auto found = findEffect(fxIndex, fxBase.string(), "explosions/big");
        check(found.found, "der untergeordnete Effekt wird gefunden");
        check(found.path == "effects/explosions/big.efx",
              "mit effects/ davor und .efx dahinter");
        check(findEffect(fxIndex, fxBase.string(), "explosions/big.efx").found,
              "eine schon angehaengte Endung stoert nicht");
        check(findEffect(fxIndex, fxBase.string(), "EXPLOSIONS/BIG").found,
              "Gross-/Kleinschreibung wird ignoriert");
        check(!findEffect(fxIndex, fxBase.string(), "gibt/es/nicht").found,
              "ein erfundener Name nicht");
        check(!findEffect(fxIndex, fxBase.string(), "").found, "leer auch nicht");
    }

    }

    // --- Mehrere Spielpfade -----------------------------------------------
    //
    // Wer an einem Mod arbeitet, hat das Grundspiel an einer Stelle und den
    // Mod an einer zweiten. Ein Effekt aus dem Mod verweist auf einen Shader
    // aus dem Grundspiel — mit nur einem Pfad findet man den nie.
    {
        const fs::path baseGame = root / "grundspiel";
        const fs::path modDir = root / "mod";
        fs::create_directories(baseGame / "gfx" / "misc");
        fs::create_directories(baseGame / "effects");
        fs::create_directories(modDir / "gfx" / "misc");
        fs::create_directories(modDir / "effects");

        // Im Grundspiel: zwei Dateien.
        { std::ofstream(baseGame / "gfx" / "misc" / "nurbasis.tga") << "b"; }
        { std::ofstream(baseGame / "gfx" / "misc" / "beide.tga") << "GRUNDSPIEL"; }
        { std::ofstream(baseGame / "effects" / "alt.efx") << "x"; }
        // Im Mod: eine eigene und eine gleichnamige.
        { std::ofstream(modDir / "gfx" / "misc" / "nurmod.tga") << "m"; }
        { std::ofstream(modDir / "gfx" / "misc" / "beide.tga") << "MOD"; }

        // Mod zuerst — so herum ueberschreibt er das Grundspiel.
        const Index both = scanAll({modDir.string(), baseGame.string()});
        check(both.roots.size() == 2, "zwei Wurzeln durchsucht");
        check(both.textures.size() == 3,
              "drei verschiedene Texturen, die doppelte nur einmal");

        // Dateien aus BEIDEN Ordnern werden gefunden.
        check(findTexture(both, "", "gfx/misc/nurbasis").found,
              "eine Datei nur im Grundspiel wird gefunden");
        check(findTexture(both, "", "gfx/misc/nurmod").found,
              "eine Datei nur im Mod ebenso");

        // Und bei Namensgleichheit gewinnt der zuerst durchsuchte Ordner.
        const auto contested = findTexture(both, "", "gfx/misc/beide");
        check(contested.found, "die umstrittene Datei wird gefunden");
        const auto bytes = readFile("", contested);
        const std::string content(bytes.begin(), bytes.end());
        check(content == "MOD", "und zwar die aus dem zuerst genannten Ordner");
        std::printf("  zwei Spielpfade: %zu Texturen, bei Namensgleichheit "
                    "gewinnt \"%s\"\n", both.textures.size(), content.c_str());

        // Umgekehrte Reihenfolge, umgekehrtes Ergebnis — das ist der Beleg,
        // dass wirklich die Reihenfolge entscheidet und nicht der Zufall.
        const Index swapped = scanAll({baseGame.string(), modDir.string()});
        const auto other = findTexture(swapped, "", "gfx/misc/beide");
        const auto otherBytes = readFile("", other);
        check(std::string(otherBytes.begin(), otherBytes.end()) == "GRUNDSPIEL",
              "andere Reihenfolge, anderer Gewinner");

        // Auch Effekte werden ueber alle Pfade gesucht.
        check(findEffect(both, "", "alt").found,
              "ein Effekt aus dem Grundspiel wird gefunden");

        // Randfaelle.
        check(!scanAll({}).notes.empty(), "ohne Pfad meldet es sich");
        check(scanAll({modDir.string(), modDir.string()}).roots.size() <= 2,
              "derselbe Pfad zweimal haengt nichts auf");
    }

    // --- Vom Shadernamen zur Bilddatei ------------------------------------
    //
    // Der Weg ist laenger, als man denkt, und Schritt vier ist der, an dem
    // Nachbauten scheitern: in einer .efx-Datei steht NIE eine Endung, und in
    // einer map-Zeile fast nie.
    {
        const fs::path texBase = root / "textures";
        fs::create_directories(texBase / "shaders");
        fs::create_directories(texBase / "gfx" / "effects");
        fs::create_directories(texBase / "gfx" / "misc");

        // Ein Shader, dessen map-Zeile auf eine ganz andere Datei zeigt —
        // genau der Fall, den man nicht raten kann.
        { std::ofstream(texBase / "shaders" / "fx.shader")
              << "gfx/effects/coolflame\n{\n\t{\n"
                 "\t\tmap gfx/misc/flame_base.tga\n\t}\n}\n"
                 "gfx/effects/whiteonly\n{\n\t{\n\t\tmap $whiteimage\n\t}\n}\n"; }
        { std::ofstream(texBase / "gfx" / "misc" / "flame_base.tga") << "x"; }
        // Eine nackte Bilddatei ohne Shader.
        { std::ofstream(texBase / "gfx" / "effects" / "plain.jpg") << "x"; }
        // Und eine, die es in zwei Endungen gibt — tga muss gewinnen.
        { std::ofstream(texBase / "gfx" / "effects" / "both.tga") << "x"; }
        { std::ofstream(texBase / "gfx" / "effects" / "both.jpg") << "x"; }

        const Index texIndex = scan(texBase.string());
        check(texIndex.shaders.size() == 2, "zwei Shader gefunden");

        // Der Shader zeigt woandershin als sein Name.
        check(texIndex.mapOf("gfx/effects/coolflame") == "gfx/misc/flame_base.tga",
              "die map-Zeile wird gemerkt");
        const auto viaShader = findTexture(texIndex, texBase.string(),
                                           "gfx/effects/coolflame");
        check(viaShader.found, "ueber den Shader gefunden");
        check(viaShader.path == "gfx/misc/flame_base.tga",
              "und zwar die Datei aus der map-Zeile, nicht die nach dem Shader");
        std::printf("  Shader gfx/effects/coolflame -> %s\n", viaShader.path.c_str());

        // Ohne Shader gilt der Name direkt als Bildname — die Engine baut
        // sich daraus einen Ersatzshader.
        const auto direct = findTexture(texIndex, texBase.string(),
                                        "gfx/effects/plain");
        check(direct.found, "eine nackte Bilddatei wird auch ohne Shader gefunden");
        check(direct.path == "gfx/effects/plain.jpg", "mit der richtigen Endung");

        // Reihenfolge der Endungen: tga schlaegt jpg.
        const auto both = findTexture(texIndex, texBase.string(),
                                      "gfx/effects/both");
        check(both.path == "gfx/effects/both.tga", "tga gewinnt gegen jpg");

        // Eine schon angehaengte Endung wird nicht verdoppelt.
        check(findTexture(texIndex, texBase.string(),
                          "gfx/effects/both.tga").path == "gfx/effects/both.tga",
              "eine vorhandene Endung wird nicht verdoppelt");

        // `$whiteimage` fuehrt zu keiner Datei, aber zu einem Bild: dem
        // eingebauten weissen (ParseStage in tr_shader.cpp setzt
        // tr.whiteImage). Hier stand "fuehrt zu keiner Datei" mit
        // found == false — und die Vorschau zeichnete den Ersatzfleck.
        {
            const auto white = findTexture(texIndex, texBase.string(),
                                           "gfx/effects/whiteonly");
            check(white.found && white.white && white.archive.empty(),
                  "$whiteimage fuehrt zum eingebauten weissen Bild, nicht zu einer Datei");
        }
        check(!findTexture(texIndex, texBase.string(), "").found, "leer auch nicht");
        check(!findTexture(texIndex, texBase.string(), "gibt/es/nicht").found,
              "ein erfundener Name ebenso");

        // Gross-/Kleinschreibung ist egal, wie im Spiel.
        check(findTexture(texIndex, texBase.string(),
                          "GFX/Effects/Both").found,
              "Gross-/Kleinschreibung wird ignoriert");

        // Und die Datei laesst sich lesen.
        std::string readError;
        const auto bytes = readFile(texBase.string(), both, &readError);
        check(bytes.size() == 1, "die gefundene Datei laesst sich lesen");
    }

    check(scan("").notes.size() == 1, "leerer Pfad meldet sich");
    check(scan("Z:/gibt/es/nicht").shaders.empty(), "toter Pfad liefert nichts");

    // Abbruch mittendrin.
    efx::jobs::Cancellation cancel;
    cancel.cancel();
    check(scan(base.string(), nullptr, &cancel).shaders.empty(),
          "abgebrochener Suchlauf liefert nichts");
}

void testCurves() {
    std::cout << "== Kurven ==\n";
    using namespace efx::curve;

    constexpr float kStart = 0.0f, kEnd = 1000.0f;  // eine Sekunde Lebensdauer

    // Ohne Flag bleibt der Wert auf start.
    //
    // Das ist der ueberraschendste Teil und der haeufigste Stolperstein: wer
    // "end" setzt und "linear" vergisst, sieht nie eine Aenderung. Im
    // Spielcode steht dazu ausdruecklich "completely biased towards start if
    // it doesn't get overridden".
    Curve constant{10.0f, 99.0f, 0.0f, 0};
    for (float t = kStart; t <= kEnd; t += 100.0f) {
        check(evaluate(constant, t, kStart, kEnd, 0.0f) == 10.0f,
              "ohne Flag bleibt der Wert bei start");
    }

    // linear: laeuft gleichmaessig von start nach end.
    Curve line{0.0f, 100.0f, 0.0f, kLinear};
    check(std::fabs(evaluate(line, 0.0f, kStart, kEnd, 0.0f)) < 0.01f,
          "linear: am Anfang start");
    check(std::fabs(evaluate(line, 500.0f, kStart, kEnd, 0.0f) - 50.0f) < 0.01f,
          "auf halber Strecke die Mitte");
    check(std::fabs(evaluate(line, 1000.0f, kStart, kEnd, 0.0f) - 100.0f) < 0.01f,
          "am Ende end");
    // Und monoton.
    float previous = -1.0f;
    bool monotone = true;
    for (float t = 0.0f; t <= 1000.0f; t += 25.0f) {
        const float v = evaluate(line, t, kStart, kEnd, 0.0f);
        if (v < previous - 1e-4f) monotone = false;
        previous = v;
    }
    check(monotone, "linear steigt durchgehend");

    // nonlinear: bleibt auf start, bis parm erreicht ist, dann faellt es.
    //
    // parm ist KEIN Anteil, sondern wird umgerechnet:
    //   parm * 0.01 * Lebensdauer + Startzeit
    // parm = 50 bei 1000 ms Lebensdauer ergibt also 500 ms.
    Curve nonlinear{0.0f, 100.0f, 50.0f, kNonLinear};
    const float parm = resolveParm(nonlinear, kStart, kEnd - kStart);
    check(std::fabs(parm - 500.0f) < 0.01f,
          "parm 50 bei 1000 ms Leben ergibt den Zeitpunkt 500 ms");
    std::printf("  nonlinear: parm 50 -> Zeitpunkt %.0f ms\n",
                static_cast<double>(parm));
    check(std::fabs(evaluate(nonlinear, 200.0f, kStart, kEnd, parm)) < 0.01f,
          "vor dem Zeitpunkt bleibt es bei start");
    check(std::fabs(evaluate(nonlinear, 500.0f, kStart, kEnd, parm)) < 0.01f,
          "genau dort noch");
    check(std::fabs(evaluate(nonlinear, 750.0f, kStart, kEnd, parm) - 50.0f) < 0.01f,
          "danach faellt es, auf halber Reststrecke die Mitte");
    check(std::fabs(evaluate(nonlinear, 1000.0f, kStart, kEnd, parm) - 100.0f) < 0.01f,
          "und erreicht end");

    // clamp: das Gegenstueck — faellt zuerst, dann bleibt es auf end.
    Curve clamped{0.0f, 100.0f, 50.0f, kClamp};
    const float clampParm = resolveParm(clamped, kStart, kEnd - kStart);
    check(std::fabs(clampParm - 500.0f) < 0.01f, "clamp rechnet parm genauso um");
    check(std::fabs(evaluate(clamped, 0.0f, kStart, kEnd, clampParm)) < 0.01f,
          "clamp: am Anfang start");
    check(std::fabs(evaluate(clamped, 250.0f, kStart, kEnd, clampParm) - 50.0f) < 0.01f,
          "auf halber Strecke bis parm die Mitte");
    check(std::fabs(evaluate(clamped, 500.0f, kStart, kEnd, clampParm) - 100.0f) < 0.01f,
          "bei parm ist end erreicht");
    check(std::fabs(evaluate(clamped, 900.0f, kStart, kEnd, clampParm) - 100.0f) < 0.01f,
          "und bleibt dort");

    // clamp und nonlinear teilen sich die Bits von wave — das ist die
    // Kollision, die Ravens Handbuch nirgends erwaehnt.
    check((kNonLinear | kWave) == kClamp,
          "nonlinear|wave ist dasselbe Bitmuster wie clamp");

    // wave: parm ist eine Frequenz, kein Zeitpunkt.
    Curve wave{0.0f, 100.0f, 1000.0f, kWave};
    const float waveParm = resolveParm(wave, kStart, kEnd - kStart);
    check(std::fabs(waveParm - 1000.0f * 3.14159265f * 0.001f) < 1e-4f,
          "wave rechnet parm zu einer Frequenz um, nicht zu einem Zeitpunkt");
    std::printf("  wave: parm 1000 -> Frequenz %.4f rad/ms\n",
                static_cast<double>(waveParm));
    check(std::fabs(evaluate(wave, 0.0f, kStart, kEnd, waveParm)) < 0.01f,
          "wave beginnt bei start (cos 0 = 1)");
    // Nach einer halben Periode ist der Kosinus -1, der Anteil also -1 —
    // der Wert schwingt damit ueber end hinaus. Das macht die Engine auch so.
    const float halfPeriod = 3.14159265f / waveParm;
    check(evaluate(wave, halfPeriod, kStart, kEnd, waveParm) > 100.0f,
          "und schwingt ueber end hinaus, wie im Spiel");

    // random daempft den Anteil, es ersetzt ihn nicht.
    //
    // Ravens Handbuch sagt, random beginne bei null. Der Quelltext sagt
    // "Random simply modulates the existing value" — also perc = zufall * perc.
    Curve randomLine{0.0f, 100.0f, 0.0f, kLinear | kRandom};
    check(std::fabs(evaluate(randomLine, 0.0f, kStart, kEnd, 0.0f, 1.0f)) < 0.01f,
          "random mit Faktor 1 aendert nichts");
    check(std::fabs(evaluate(randomLine, 0.0f, kStart, kEnd, 0.0f, 0.0f) - 100.0f)
              < 0.01f,
          "random mit Faktor 0 zieht den Anteil auf null, also ganz auf end");
    check(std::fabs(evaluate(randomLine, 0.0f, kStart, kEnd, 0.0f, 0.5f) - 50.0f)
              < 0.01f,
          "und dazwischen anteilig");

    // linear mit nonlinear zusammen: gleichmaessig gemischt.
    Curve both{0.0f, 100.0f, 50.0f, kLinear | kNonLinear};
    const float bothParm = resolveParm(both, kStart, kEnd - kStart);
    const float mixed = bias(both, 750.0f, kStart, kEnd, bothParm);
    const float onlyLinear = bias(Curve{0, 100, 0, kLinear}, 750.0f, kStart, kEnd, 0);
    const float onlyNonlinear =
        bias(Curve{0, 100, 50, kNonLinear}, 750.0f, kStart, kEnd, bothParm);
    check(std::fabs(mixed - (onlyLinear * 0.5f + onlyNonlinear * 0.5f)) < 1e-4f,
          "linear und nonlinear zusammen werden haelftig gemischt");

    // Und die Pruefung meldet den Stolperstein jetzt auch.
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        efx::Primitive& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.life = efx::Range::single(500.0f);
        p.shaders.push_back("gfx/misc/test");
        p.size.present = true;
        p.size.start = efx::Range::single(1.0f);
        p.size.end = efx::Range::single(20.0f);
        p.size.curveFlags = 0;  // genau der Fall

        // GENAU EINMAL gemeldet, nicht zweimal.
        //
        // Es gab zwei Regeln fuer denselben Fall, mit zwei verschiedenen
        // Texten. Beim Anwender stand jede dieser Warnungen doppelt im
        // Meldungsfenster, unterschiedlich formuliert — das sieht aus wie zwei
        // verschiedene Probleme, und man sucht zweimal.
        int warnings = 0;
        for (const auto& d : efx::validate(effect, efx::Dialect::Both)) {
            if (d.id == efx::i18n::Str::VNoCurve ||
                d.id == efx::i18n::Str::VEndNoCurve ||
                d.id == efx::i18n::Str::VRgbEndNoCurve) {
                ++warnings;
            }
        }
        check(warnings == 1,
              "abweichendes Ende ohne Kurvenart wird genau EINMAL gemeldet");
        std::printf("  end ohne Kurvenart: %d Meldung(en)\n", warnings);

        p.size.curveFlags = kLinear;
        warnings = 0;
        for (const auto& d : efx::validate(effect, efx::Dialect::Both)) {
            if (d.id == efx::i18n::Str::VNoCurve ||
                d.id == efx::i18n::Str::VEndNoCurve ||
                d.id == efx::i18n::Str::VRgbEndNoCurve) {
                ++warnings;
            }
        }
        check(warnings == 0, "mit linear nicht mehr");

        // Gleicher Anfang und gleiches Ende brauchen keine Kurve.
        p.size.curveFlags = 0;
        p.size.end = efx::Range::single(1.0f);
        warnings = 0;
        for (const auto& d : efx::validate(effect, efx::Dialect::Both)) {
            if (d.id == efx::i18n::Str::VNoCurve ||
                d.id == efx::i18n::Str::VEndNoCurve ||
                d.id == efx::i18n::Str::VRgbEndNoCurve) {
                ++warnings;
            }
        }
        check(warnings == 0, "und bei gleichem Anfang und Ende auch nicht");
    }

    // impactfx braucht KEIN Flag und keine Physik in der Datei.
    //
    // Hier stand das Gegenteil ("impactfx braucht Flag UND Physik"), und die
    // Pruefung meldete in drei ausgelieferten Dateien "wird nie gestartet".
    // ParseImpactFxStrings (FxTemplate.cpp, SP und MP) setzt beim Lesen der
    // Liste selbst FX_IMPACT_RUNS_FX | FX_APPLY_PHYSICS.
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        efx::Primitive& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.life = efx::Range::single(500.0f);
        p.shaders.push_back("gfx/test");
        p.impactFx.push_back("einschlaege/stein");

        auto has = [&](efx::i18n::Str id) {
            for (const auto& d : efx::validate(effect, efx::Dialect::Both)) {
                if (d.id == id) return true;
            }
            return false;
        };
        check(!has(efx::i18n::Str::VImpactFxNoFlag) &&
                  !has(efx::i18n::Str::VImpactFxNoPhysics),
              "impactfx ohne Flag und ohne usePhysics ist kein Befund");
        check((efx::effectiveFlags(p) &
               (efx::kFlagImpactRunsFx | efx::kFlagApplyPhysics)) ==
                  (efx::kFlagImpactRunsFx | efx::kFlagApplyPhysics),
              "die Liste setzt beide Bits selbst, wie ParseImpactFxStrings");

        // killOnImpact ohne usePhysics ist mit der Liste kein Widerspruch —
        // die Physik kommt aus der Liste.
        p.flags |= efx::kFlagKillOnImpact;
        check(!has(efx::i18n::Str::VImpactKills),
              "impactKills mit impactfx-Liste braucht kein usePhysics");
        p.impactFx.clear();
        check(has(efx::i18n::Str::VImpactKills),
              "ohne die Liste fehlt die Physik wirklich");
    }

    // deathfx braucht KEIN Flag in der Datei: ParseDeathFxStrings setzt
    // FX_DEATH_RUNS_FX selbst (FxTemplate.cpp, SP und MP).
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        efx::Primitive& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.life = efx::Range::single(500.0f);
        p.shaders.push_back("gfx/test");
        p.deathFx.push_back("explosions/big");

        auto has = [&](efx::i18n::Str id) {
            for (const auto& d : efx::validate(effect, efx::Dialect::Both)) {
                if (d.id == id) return true;
            }
            return false;
        };
        check(!has(efx::i18n::Str::VDeathFxNoFlag),
              "deathfx ohne Flag ist kein Befund");

        p.flags |= efx::kFlagDeathRunsFx;
        check(!has(efx::i18n::Str::VDeathFxNoFlag), "mit Flag ebenso wenig");
        check(!has(efx::i18n::Str::VDeathFxKillOnImpact), "und kein Hinweis");

        p.flags |= efx::kFlagKillOnImpact;
        check(has(efx::i18n::Str::VDeathFxKillOnImpact),
              "mit killOnImpact kommt ein Hinweis");

        p.deathFx.clear();
        p.flags = 0;
        check(!has(efx::i18n::Str::VDeathFxNoFlag),
              "ohne deathfx gar nichts");
    }

    // Und die Pruefung meldet den Stolperstein.
    //
    // Diese drei Pruefungen waren zwischenzeitlich verlorengegangen — die
    // Arbeitskopie und das ausgelieferte Paket rc18 waren in validate.cpp
    // auseinandergelaufen. Wieder da, und die Meldung ist jetzt uebersetzt.
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        efx::Primitive& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.life = efx::Range::single(500.0f);
        p.shaders.push_back("gfx/misc/test");
        p.size.present = true;
        p.size.start = efx::Range::single(1.0f);
        p.size.end = efx::Range::single(20.0f);
        p.size.curveFlags = 0;  // genau der Fall

        auto warns = [&] {
            for (const auto& d : efx::validate(effect, efx::Dialect::Both)) {
                if (d.id == efx::i18n::Str::VNoCurve) return true;
            }
            return false;
        };
        check(warns(), "abweichendes Ende ohne Kurvenart wird gemeldet");

        p.size.curveFlags = kLinear;
        check(!warns(), "mit linear nicht mehr");

        p.size.curveFlags = 0;
        p.size.end = efx::Range::single(1.0f);
        check(!warns(), "und bei gleichem Anfang und Ende auch nicht");
    }

    // Randfaelle, an denen eine Division schiefgeht.
    check(std::isfinite(evaluate(line, 0.0f, 100.0f, 100.0f, 0.0f)),
          "Lebensdauer null ergibt keine Division durch null");
    check(std::isfinite(evaluate(nonlinear, 500.0f, 0.0f, 500.0f, 500.0f)),
          "parm genau am Ende ebenso");
    check(std::isfinite(evaluate(clamped, 0.0f, 0.0f, 1000.0f, 0.0f)),
          "clamp mit parm gleich Startzeit ebenso");
}

void testScheduling() {
    std::cout << "== Zeitplanung ==\n";
    using namespace efx::sim;

    auto makeEffect = [](int count, float delayMin, float delayMax, bool ranged,
                         bool even) {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        efx::Primitive& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(static_cast<float>(count));
        p.delay.set = true;
        p.delay.ranged = ranged;
        p.delay.min = delayMin;
        p.delay.max = delayMax;
        if (even) p.spawnFlags |= efx::kSpawnEvenDistribution;
        return effect;
    };

    // Anzahl.
    {
        Random random(1);
        check(schedule(makeEffect(1, 0, 0, false, false), random).size() == 1,
              "count 1 ergibt eine Ausloesung");
        check(schedule(makeEffect(20, 0, 0, false, false), random).size() == 20,
              "count 20 ergibt zwanzig");
    }
    {
        // Ohne count-Feld genau einmal — die Engine hat mSpawnCount mit 1
        // vorbelegt.
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        effect.primitives.back().type = efx::PrimitiveType::Particle;
        Random random(1);
        check(schedule(effect, random).size() == 1, "ohne count genau einmal");
    }

    // Gleichmaessige Verteilung.
    //
    // factor = |max - min| / count, delay = t * factor. Bemerkenswert: die
    // Engine beginnt bei NULL, nicht beim Minimum. Bei 200..400 und count 4
    // ergibt das 0, 50, 100, 150 — nicht 200, 250, 300, 350.
    {
        Random random(1);
        const auto spawns = schedule(makeEffect(4, 200.0f, 400.0f, true, true), random);
        check(spawns.size() == 4, "vier Ausloesungen");
        check(std::fabs(spawns[0].timeMs - 0.0f) < 0.01f, "die erste bei 0 ms");
        check(std::fabs(spawns[1].timeMs - 50.0f) < 0.01f, "dann 50");
        check(std::fabs(spawns[2].timeMs - 100.0f) < 0.01f, "dann 100");
        check(std::fabs(spawns[3].timeMs - 150.0f) < 0.01f, "dann 150");
        std::printf("  gleichmaessig, delay 200..400, count 4: %.0f %.0f %.0f %.0f ms"
                    "  (die Engine beginnt bei 0, nicht bei 200)\n",
                    static_cast<double>(spawns[0].timeMs),
                    static_cast<double>(spawns[1].timeMs),
                    static_cast<double>(spawns[2].timeMs),
                    static_cast<double>(spawns[3].timeMs));
    }

    // Ohne das Flag wird gewuerfelt — innerhalb der Spanne.
    {
        Random random(7);
        const auto spawns = schedule(makeEffect(200, 100.0f, 300.0f, true, false),
                                     random);
        check(spawns.size() == 200, "zweihundert Ausloesungen");
        bool inRange = true;
        bool varied = false;
        for (const auto& spawn : spawns) {
            if (spawn.timeMs < 100.0f - 0.01f || spawn.timeMs > 300.0f + 0.01f) {
                inRange = false;
            }
            if (std::fabs(spawn.timeMs - spawns[0].timeMs) > 1.0f) varied = true;
        }
        check(inRange, "alle innerhalb der Spanne");
        check(varied, "und nicht alle gleich");
    }

    // Nach Zeit sortiert — sonst muesste die Vorschau jedes Bild suchen.
    {
        Random random(3);
        const auto spawns = schedule(makeEffect(50, 0.0f, 500.0f, true, false),
                                     random);
        bool sorted = true;
        for (size_t i = 1; i < spawns.size(); ++i) {
            if (spawns[i].timeMs < spawns[i - 1].timeMs) sorted = false;
        }
        check(sorted, "die Liste kommt nach Zeit sortiert");
    }

    // Abgeschaltete Segmente kommen nicht vor.
    {
        efx::Effect effect;
        for (int i = 0; i < 3; ++i) {
            effect.primitives.push_back(efx::Primitive{});
            effect.primitives.back().type = efx::PrimitiveType::Particle;
        }
        Random random(1);
        const auto spawns = schedule(effect, random, {true, false, true});
        check(spawns.size() == 2, "zwei von drei Segmenten");
        for (const auto& spawn : spawns) {
            check(spawn.primitiveIndex != 1, "das mittlere fehlt");
        }
        check(schedule(effect, random, {}).size() == 3,
              "leere Maske heisst alle");
    }

    // Gleicher Ausgangswert, gleiches Ergebnis.
    {
        Random a(42), b(42);
        const auto first = schedule(makeEffect(30, 0.0f, 400.0f, true, false), a);
        const auto second = schedule(makeEffect(30, 0.0f, 400.0f, true, false), b);
        bool same = first.size() == second.size();
        for (size_t i = 0; same && i < first.size(); ++i) {
            if (first[i].timeMs != second[i].timeMs) same = false;
        }
        check(same, "gleicher Startwert ergibt dieselbe Planung");
    }

    // Unsinnige Werte duerfen die Vorschau nicht aufhaengen.
    {
        Random random(1);
        // count 0 heisst im Spiel: nichts. CFxRange::GetRoundedVal gibt bei
        // mMin == mMax einfach mMin zurueck, ohne Untergrenze (FxScheduler.h).
        // Hier stand "count 0 wird zu 1" — die Vorschau zeigte, was das Spiel
        // nicht zeigt.
        check(schedule(makeEffect(0, 0, 0, false, false), random).empty(),
              "count 0 ergibt nichts, wie GetRoundedVal");
        check(schedule(makeEffect(-5, 0, 0, false, false), random).empty(),
              "eine negative Anzahl ebenso nichts");
        check(schedule(makeEffect(100000, 0, 0, false, false), random).size() <= 4096,
              "eine unsinnig grosse Anzahl wird gedeckelt");
        const auto negative = schedule(makeEffect(3, -100.0f, -50.0f, true, false),
                                       random);
        for (const auto& spawn : negative) {
            check(spawn.timeMs >= 0.0f, "negative Verzoegerung wird auf 0 gezogen");
        }
    }
}

void testMotion() {
    std::cout << "== Bahn ==\n";
    using namespace efx::sim;
    const efx::camera::Vec3 origin{0.0f, 0.0f, 0.0f};

    // Ohne alles bleibt es stehen.
    check(efx::camera::length(positionAt(origin, {}, {}, 0.0f, 2.0f)) == 0.0f,
          "ohne Geschwindigkeit bleibt es liegen");

    // Gleichfoermig.
    const auto straight = positionAt(origin, {100.0f, 0.0f, 0.0f}, {}, 0.0f, 2.0f);
    check(std::fabs(straight.x - 200.0f) < 0.01f,
          "100 Einheiten je Sekunde, zwei Sekunden: 200 Einheiten");

    // Schwerkraft: 0.5 * g * t^2 — das ist die richtige Formel.
    const auto falling = positionAt(origin, {}, {}, -800.0f, 1.0f);
    check(std::fabs(falling.z - (-400.0f)) < 0.01f,
          "Schwerkraft -800 nach einer Sekunde: -400 Einheiten (0.5*g*t^2)");
    const auto fallingLonger = positionAt(origin, {}, {}, -800.0f, 2.0f);
    check(std::fabs(fallingLonger.z - (-1600.0f)) < 0.01f,
          "nach zwei Sekunden viermal so weit — quadratisch");

    // Beschleunigung: 0.5 * a * t^2.
    //
    // Hier stand einmal `a * t^2` — die doppelte Strecke — mit dem Hinweis auf
    // Ravens eigenen Zweifel im Quelltext. Das war das FALSCHE der beiden
    // Modelle der Engine:
    //
    //   Angeheftet (FX_RELATIVE) rechnet CParticle::Update geschlossen und
    //   kommt auf a*t². Das gilt fuer Effekte an einem Bolt oder Spieler.
    //
    //   In der Welt gespielt — der gewoehnliche Fall und der, den ein Editor
    //   zeigt — faltet CreateEffect die Schwerkraft in die Beschleunigung
    //   (accel[2] += mGravity) und UpdateOrigin rechnet SCHRITTWEISE:
    //       UpdateVelocity();   // mVel += mAccel * mFloatFrameTime
    //       new_origin = mOrigin1 + mFloatFrameTime * mVel;
    //   Das ergibt im Grenzwert 0.5*a*t². Nachgerechnet bei a=100, t=1 s:
    //       30 fps 51.67 | 60 fps 50.83 | 125 fps 50.40 | Grenzwert 50.00
    //
    // Fuer die Schwerkraft aendert sich dadurch nichts — beide Modelle geben
    // 0.5*g*t². Eine Datei mit `accel` flog bei uns dagegen doppelt so weit
    // wie im Spiel.
    const auto accelerated = positionAt(origin, {}, {100.0f, 0.0f, 0.0f}, 0.0f, 2.0f);
    check(std::fabs(accelerated.x - 200.0f) < 0.01f,
          "Beschleunigung 100 nach zwei Sekunden: 200 (0.5*a*t^2)");
    std::printf("  Beschleunigung 100 u/s^2, t=2 s: %.0f Einheiten\n",
                static_cast<double>(accelerated.x));

    // Schwerkraft und Beschleunigung wirken gleich stark.
    //
    // In der Welt gespielt faltet die Engine die Schwerkraft in die
    // Beschleunigung (accel[2] += mGravity), danach sind sie nicht mehr zu
    // unterscheiden. Wer eine Datei von `gravity 10` auf `accel 0 0 10`
    // umschreibt, muss dasselbe sehen.
    const auto byGravity = positionAt(origin, {}, {}, 100.0f, 2.0f);
    const auto byAccel = positionAt(origin, {}, {0.0f, 0.0f, 100.0f}, 0.0f, 2.0f);
    check(std::fabs(byGravity.z - byAccel.z) < 0.01f,
          "Schwerkraft und Beschleunigung sind dasselbe");

    // Alles zusammen, und die Reihenfolge stimmt mit der Engine ueberein.
    const auto combined =
        positionAt({10.0f, 20.0f, 30.0f}, {5.0f, 0.0f, 50.0f},
                   {0.0f, 3.0f, 0.0f}, -800.0f, 0.5f);
    check(std::fabs(combined.x - (10.0f + 5.0f * 0.5f)) < 0.01f, "x aus velocity");
    check(std::fabs(combined.y - (20.0f + 0.5f * 3.0f * 0.25f)) < 0.01f,
          "y aus acceleration (0.5*a*t^2)");
    check(std::fabs(combined.z - (30.0f + 50.0f * 0.5f + 0.5f * -800.0f * 0.25f))
              < 0.01f,
          "z aus velocity und Schwerkraft");

    // Zeit null aendert nichts, negative Zeit bleibt endlich.
    const auto atZero = positionAt({1, 2, 3}, {9, 9, 9}, {9, 9, 9}, -800.0f, 0.0f);
    check(atZero.x == 1.0f && atZero.y == 2.0f && atZero.z == 3.0f,
          "bei t=0 steht es am Ursprung");
    check(std::isfinite(positionAt({}, {1, 1, 1}, {1, 1, 1}, -800.0f, -1.0f).x),
          "negative Zeit ergibt brauchbare Zahlen");
}

void testPhysics() {
    std::cout << "== Physik ==\n";
    using namespace efx::sim;

    const auto planes = roomPlanes(200.0f, 200.0f, 300.0f);
    check(planes.size() == 6, "sechs Ebenen");

    // --- Wegstuecke ------------------------------------------------------
    {
        // Von oben auf den Boden.
        const Hit down = trace({0, 0, 50}, {0, 0, -10}, planes);
        check(down.hit, "der Boden wird getroffen");
        check(std::fabs(down.point.z) < 0.01f, "und zwar bei z = 0");
        check(down.normal.z > 0.9f, "die Normale zeigt nach oben");
        check(std::fabs(down.fraction - 50.0f / 60.0f) < 0.01f,
              "der Anteil stimmt");

        // Ein Weg, der nichts trifft.
        check(!trace({0, 0, 50}, {0, 0, 60}, planes).hit,
              "nach oben in der Raummitte trifft nichts");
        check(!trace({0, 0, 50}, {10, 10, 50}, planes).hit,
              "und quer durch die Mitte auch nicht");

        // Von hinter der Ebene kommend NICHT treffen — sonst prallt etwas,
        // das schon im Boden steckt, nach unten weiter weg.
        check(!trace({0, 0, -10}, {0, 0, -20}, planes).hit,
              "wer schon dahinter steckt, prallt nicht nach aussen ab");

        // In einer Ecke gewinnt die fruehere Beruehrung.
        const Hit corner = trace({190, 0, 50}, {210, 0, -10}, planes);
        check(corner.hit, "die Ecke wird getroffen");
        check(std::fabs(corner.normal.x) > 0.9f,
              "und zwar die Wand, die frueher kommt, nicht der Boden");
    }

    // --- Abprallen -------------------------------------------------------
    {
        // Senkrecht fallen lassen, halbe Elastizitaet.
        const Path bounce = buildPath({0, 0, 100}, {}, {}, -800.0f, 3000.0f,
                                      planes, 0.5f, false);
        check(bounce.segments.size() > 1, "es gibt Abpraller");
        check(!bounce.impactMs.empty(), "und Aufpralle werden gemerkt");
        check(bounce.impactNormal[0].z > 0.9f, "der erste ist der Boden");

        // Nach dem ersten Aufprall geht es wieder aufwaerts.
        const float justAfter = bounce.impactMs[0] + 20.0f;
        check(positionOnPath(bounce, justAfter).z >
                  positionOnPath(bounce, bounce.impactMs[0] + 1.0f).z,
              "nach dem Aufprall steigt es wieder");

        // Aber nicht so hoch wie vorher — die Elastizitaet daempft.
        float firstPeak = 0.0f, secondPeak = 0.0f;
        for (float t = 0.0f; t < bounce.impactMs[0]; t += 5.0f) {
            firstPeak = std::max(firstPeak, positionOnPath(bounce, t).z);
        }
        const float secondEnd = bounce.impactMs.size() > 1 ? bounce.impactMs[1]
                                                           : 3000.0f;
        for (float t = bounce.impactMs[0]; t < secondEnd; t += 5.0f) {
            secondPeak = std::max(secondPeak, positionOnPath(bounce, t).z);
        }
        check(secondPeak < firstPeak * 0.6f,
              "der zweite Sprung ist deutlich niedriger");
        std::printf("  Abpraller: %zu Aufpralle, erster Gipfel %.0f, zweiter %.0f\n",
                    bounce.impactMs.size(), static_cast<double>(firstPeak),
                    static_cast<double>(secondPeak));

        // Es kommt zur Ruhe statt ewig zu zittern.
        const float atEnd = positionOnPath(bounce, 2900.0f).z;
        check(atEnd < 5.0f, "am Ende liegt es auf dem Boden");
        check(atEnd > -1.0f, "und nicht darunter");

        // Nie unter den Boden.
        bool aboveFloor = true;
        for (float t = 0.0f; t < 3000.0f; t += 3.0f) {
            if (positionOnPath(bounce, t).z < -0.5f) aboveFloor = false;
        }
        check(aboveFloor, "die Bahn bleibt ueber dem Boden");
    }

    // Ohne Elastizitaet bleibt es sofort liegen.
    {
        const Path dead = buildPath({0, 0, 100}, {}, {}, -800.0f, 2000.0f, planes,
                                    0.0f, false);
        check(positionOnPath(dead, 1500.0f).z < 1.0f, "ohne Elastizitaet liegt es");
    }

    // killOnImpact: die Bahn endet am ersten Aufprall.
    {
        const Path killed = buildPath({0, 0, 100}, {}, {}, -800.0f, 2000.0f, planes,
                                      0.8f, true);
        check(killed.killed, "killOnImpact wird gemerkt");
        check(killed.impactMs.size() == 1, "genau ein Aufprall");
        check(killed.segments.size() == 1, "und kein zweiter Abschnitt");
        // Danach bleibt es am Aufprallort stehen.
        const auto atDeath = positionOnPath(killed, killed.killedMs);
        const auto later = positionOnPath(killed, killed.killedMs + 500.0f);
        check(std::fabs(atDeath.z - later.z) < 0.01f,
              "nach dem Tod bewegt es sich nicht weiter");
    }

    // Seitlich gegen die Wand: die Bewegung kehrt sich um.
    {
        const Path sideways = buildPath({0, 0, 150}, {300, 0, 0}, {}, 0.0f, 2000.0f,
                                        planes, 0.9f, false);
        check(!sideways.impactMs.empty(), "die Wand wird getroffen");
        check(std::fabs(sideways.impactNormal[0].x) > 0.9f, "es ist eine Wand");
        const float before = positionOnPath(sideways, sideways.impactMs[0] - 20.0f).x;
        const float after = positionOnPath(sideways, sideways.impactMs[0] + 100.0f).x;
        check(after < before, "nach dem Aufprall geht es zurueck");
    }

    // Randfaelle.
    check(buildPath({}, {}, {}, 0.0f, 0.0f, planes, 0.5f, false).segments.size() == 1,
          "Lebensdauer null ergibt einen Abschnitt");
    check(buildPath({}, {}, {}, -800.0f, 1000.0f, {}, 0.5f, false).impactMs.empty(),
          "ohne Ebenen gibt es keine Aufpralle");
    {
        // Ein Partikel, das im Boden startet, darf nicht endlos abprallen.
        const Path stuck = buildPath({0, 0, -50}, {}, {}, -800.0f, 2000.0f, planes,
                                     0.9f, false);
        check(stuck.segments.size() < 40, "ein steckengebliebenes prallt nicht ewig");
        bool finite = true;
        for (float t = 0.0f; t < 2000.0f; t += 25.0f) {
            if (!std::isfinite(positionOnPath(stuck, t).z)) finite = false;
        }
        check(finite, "und liefert brauchbare Zahlen");
    }
    {
        // Sehr hohe Elastizitaet: der Abpraller darf nicht aufschaukeln.
        const Path lively = buildPath({0, 0, 100}, {}, {}, -800.0f, 4000.0f, planes,
                                      0.99f, false);
        float highest = 0.0f;
        for (float t = 0.0f; t < 4000.0f; t += 10.0f) {
            highest = std::max(highest, positionOnPath(lively, t).z);
        }
        check(highest < 130.0f, "auch bei 0.99 steigt es nicht ueber den Start");
        std::printf("  Elastizitaet 0.99: hoechster Punkt %.0f (Start 100)\n",
                    static_cast<double>(highest));
    }
}

void testParticles() {
    std::cout << "== Partikel ==\n";
    using namespace efx::particles;
    const efx::camera::Vec3 right{1.0f, 0.0f, 0.0f};
    const efx::camera::Vec3 up{0.0f, 0.0f, 1.0f};

    // --- Billboard ------------------------------------------------------
    {
        efx::scene::Mesh mesh;
        addBillboard(mesh, {10.0f, 0.0f, 0.0f}, right, up, 5.0f, 0.0f,
                     efx::scene::rgba(255, 255, 255));
        check(mesh.vertices.size() == 4, "vier Ecken");
        check(mesh.indices.size() == 6, "zwei Dreiecke");

        // Es steht mittig um den Punkt und ist quadratisch.
        float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
        for (const auto& v : mesh.vertices) {
            minX = std::min(minX, v.pos[0]); maxX = std::max(maxX, v.pos[0]);
            minZ = std::min(minZ, v.pos[2]); maxZ = std::max(maxZ, v.pos[2]);
        }
        check(std::fabs((minX + maxX) * 0.5f - 10.0f) < 0.01f,
              "mittig um den Punkt");
        check(std::fabs((maxX - minX) - 10.0f) < 0.01f, "zehn Einheiten breit");
        check(std::fabs((maxZ - minZ) - 10.0f) < 0.01f, "und genauso hoch");
    }

    // Gedreht bleibt es quadratisch — das ist der Grund, die Achsen zu drehen
    // statt die Ecken einzeln.
    {
        efx::scene::Mesh mesh;
        addBillboard(mesh, {}, right, up, 5.0f, 45.0f,
                     efx::scene::rgba(255, 255, 255));
        auto corner = [&](int i) {
            return efx::camera::Vec3{mesh.vertices[i].pos[0], mesh.vertices[i].pos[1],
                                     mesh.vertices[i].pos[2]};
        };
        const float side01 = efx::camera::length(corner(1) - corner(0));
        const float side12 = efx::camera::length(corner(2) - corner(1));
        check(std::fabs(side01 - side12) < 0.01f,
              "auch um 45 Grad gedreht bleibt es quadratisch");
        // Und jede Ecke hat denselben Abstand zur Mitte.
        for (int i = 0; i < 4; ++i) {
            check(std::fabs(efx::camera::length(corner(i)) -
                            5.0f * std::sqrt(2.0f)) < 0.01f,
                  "alle Ecken gleich weit von der Mitte");
        }
    }

    check([&] {
        efx::scene::Mesh mesh;
        addBillboard(mesh, {}, right, up, 0.0f, 0.0f, 0);
        return mesh.vertices.empty();
    }(), "Groesse null ergibt kein Viereck");
    // Negativ dagegen schon: RB_SurfaceSprite skaliert die Achsen mit dem
    // Radius, ein negativer ergibt dasselbe Viereck gespiegelt (eine
    // Groessenwelle unter null). Hier stand "negativ ergibt kein Viereck".
    check([&] {
        efx::scene::Mesh mesh;
        addBillboard(mesh, {}, right, up, -3.0f, 0.0f, 0);
        return mesh.vertices.size() == 4 &&
               std::fabs(std::fabs(mesh.vertices[0].pos[0]) - 3.0f) < 0.01f;
    }(), "negative Groesse ergibt das gespiegelte Viereck mit |size|");

    // --- Lebenszyklus ---------------------------------------------------
    efx::Effect effect;
    effect.primitives.push_back(efx::Primitive{});
    efx::Primitive& p = effect.primitives.back();
    p.type = efx::PrimitiveType::Particle;
    p.count = efx::Range::single(5.0f);
    p.life = efx::Range::single(1000.0f);
    p.delay.set = true;
    p.delay.ranged = true;
    p.delay.min = 0.0f;
    p.delay.max = 400.0f;
    p.spawnFlags |= efx::kSpawnEvenDistribution;
    p.size.present = true;
    p.size.start = efx::Range::single(10.0f);
    p.size.end = efx::Range::single(0.0f);
    p.size.curveFlags = efx::kCurveLinear;
    p.velocity.set = true;
    p.velocity.min = {0.0f, 0.0f, 100.0f};
    p.velocity.max = p.velocity.min;

    System system;
    system.play(effect, 1);
    check(system.playing(), "der Effekt laeuft");
    check(system.live().size() == 5, "fuenf Primitiven eingeplant");

    // Gleichmaessig verteilt: 0, 80, 160, 240, 320 ms.
    check(system.aliveAt(-1.0f) == 0, "vor dem Start lebt nichts");
    check(system.aliveAt(0.0f) == 1, "bei 0 ms lebt die erste");
    check(system.aliveAt(100.0f) == 2, "bei 100 ms zwei");
    check(system.aliveAt(400.0f) == 5, "bei 400 ms alle fuenf");
    check(system.aliveAt(1000.0f) == 4, "bei 1000 ms ist die erste tot");
    check(system.aliveAt(2000.0f) == 0, "nach 2000 ms keine mehr");
    std::printf("  Lebende bei 0/100/400/1000/2000 ms: %d %d %d %d %d\n",
                system.aliveAt(0.0f), system.aliveAt(100.0f),
                system.aliveAt(400.0f), system.aliveAt(1000.0f),
                system.aliveAt(2000.0f));
    check(std::fabs(system.durationMs() - 1320.0f) < 1.0f,
          "die Gesamtdauer ist letzter Start plus Lebensdauer");

    // --- Geometrie ------------------------------------------------------
    const DrawList atStart = system.build(0.0f, right, up);
    check(atStart.alive == 1 && atStart.drawn == 1, "ein Billboard bei 0 ms");
    check(atStart.byTexture.size() == 1 &&
              atStart.byTexture.begin()->second.vertices.size() == 4,
          "mit vier Ecken");

    const DrawList atPeak = system.build(400.0f, right, up);
    check(atPeak.byTexture.begin()->second.vertices.size() == 20,
          "fuenf Billboards bei 400 ms");

    // Die Groesse schrumpft ueber die Lebensdauer, weil linear gesetzt ist.
    auto widthOfFirst = [&](float t) {
        const DrawList list = system.build(t, right, up);
        if (list.byTexture.empty()) return 0.0f;
        const auto& mesh = list.byTexture.begin()->second;
        if (mesh.vertices.size() < 4) return 0.0f;
        float minX = 1e9f, maxX = -1e9f;
        for (int i = 0; i < 4; ++i) {
            minX = std::min(minX, mesh.vertices[i].pos[0]);
            maxX = std::max(maxX, mesh.vertices[i].pos[0]);
        }
        return maxX - minX;
    };
    check(widthOfFirst(0.0f) > widthOfFirst(500.0f),
          "die Groesse schrumpft, weil linear gesetzt ist");
    check(widthOfFirst(500.0f) > widthOfFirst(900.0f), "und weiter");
    std::printf("  Groesse der ersten bei 0/500/900 ms: %.1f %.1f %.1f\n",
                static_cast<double>(widthOfFirst(0.0f)),
                static_cast<double>(widthOfFirst(500.0f)),
                static_cast<double>(widthOfFirst(900.0f)));

    // Und sie steigt, weil velocity nach oben zeigt.
    auto heightOfFirst = [&](float t) {
        const DrawList list = system.build(t, right, up);
        if (list.byTexture.empty()) return 0.0f;
        const auto& mesh = list.byTexture.begin()->second;
        return mesh.vertices.empty() ? 0.0f : mesh.vertices[0].pos[2];
    };
    check(heightOfFirst(500.0f) > heightOfFirst(0.0f),
          "sie steigt, weil velocity nach oben zeigt");

    // --- Die uebrigen Primitivtypen ---------------------------------------
    {
        // Ausgerichtetes Viereck: es liegt in einer eigenen Ebene, nicht zur
        // Kamera. Das ist der Unterschied zu einem Billboard.
        efx::scene::Mesh oriented;
        addOrientedQuad(oriented, {}, {0.0f, 0.0f, 1.0f}, 5.0f, 0.0f,
                        efx::scene::rgba(255, 255, 255));
        check(oriented.vertices.size() == 4, "vier Ecken");
        // Alle vier liegen in der Ebene z = 0, weil die Normale nach oben zeigt.
        bool flat = true;
        for (const auto& v : oriented.vertices) {
            if (std::fabs(v.pos[2]) > 0.01f) flat = false;
        }
        check(flat, "es liegt flach, wenn die Normale nach oben zeigt");
        // Mit einer anderen Normalen kippt es mit.
        efx::scene::Mesh tilted;
        addOrientedQuad(tilted, {}, {1.0f, 0.0f, 0.0f}, 5.0f, 0.0f,
                        efx::scene::rgba(255, 255, 255));
        bool upright = true;
        for (const auto& v : tilted.vertices) {
            if (std::fabs(v.pos[0]) > 0.01f) upright = false;
        }
        check(upright, "und steht senkrecht, wenn die Normale zur Seite zeigt");
        check([&] {
            efx::scene::Mesh zero;
            addOrientedQuad(zero, {}, {0, 0, 0}, 5.0f, 0.0f, 0);
            return zero.vertices.size() == 4;
        }(), "eine Nullnormale faellt auf eine Ersatzrichtung zurueck");

        // Zylinder wie RB_SurfaceCylinder: am Ursprung der eine Radius, am
        // fernen Ende der andere; (segments + 1) * 2 Eckpunkte, der erste Ring
        // am Ende noch einmal mit s = 1.
        efx::scene::Mesh tube;
        addCylinder(tube, {}, {0.0f, 0.0f, 1.0f}, 100.0f, 10.0f, 4.0f,
                    efx::scene::rgba(255, 255, 255), 16);
        check(tube.vertices.size() == 17 * 2, "sechzehn Segmente, Ring geschlossen");
        float lowest = 1e9f, highest = -1e9f, widestLow = 0.0f, widestHigh = 0.0f;
        bool tAtBase = true;
        for (const auto& v : tube.vertices) {
            const float r = std::sqrt(v.pos[0] * v.pos[0] + v.pos[1] * v.pos[1]);
            if (v.pos[2] < lowest) lowest = v.pos[2];
            if (v.pos[2] > highest) highest = v.pos[2];
            if (std::fabs(v.pos[2]) < 0.01f) {
                widestLow = std::max(widestLow, r);
                if (v.uv[1] != 1.0f) tAtBase = false;
            }
            if (std::fabs(v.pos[2] - 100.0f) < 0.01f) widestHigh = std::max(widestHigh, r);
        }
        check(std::fabs(highest - lowest - 100.0f) < 0.01f, "hundert Einheiten hoch");
        check(std::fabs(widestLow - 10.0f) < 0.01f, "am Ursprung Radius zehn");
        check(std::fabs(widestHigh - 4.0f) < 0.01f, "am Ende Radius vier");
        check(tAtBase, "t = 1 am Ursprung, wie in RB_SurfaceCylinder");
        check([&] {
            efx::scene::Mesh none;
            addCylinder(none, {}, {0, 0, 1}, 0.0f, 10.0f, 10.0f, 0, 16);
            addCylinder(none, {}, {0, 0, 1}, 100.0f, 0.0f, 0.0f, 0, 16);
            addCylinder(none, {}, {0, 0, 1}, 100.0f, 10.0f, 10.0f, 0, 2);
            return none.vertices.empty();
        }(), "Laenge null, Radius null oder zwei Segmente ergeben nichts");
        // Ein Ende duenner als 0.3: RB_SurfaceCone — ein Ring und eine Spitze.
        {
            efx::scene::Mesh cone;
            addCylinder(cone, {}, {0, 0, 1}, 50.0f, 0.0f, 8.0f, 0, 16);
            bool tipAtBase = false;
            for (const auto& v : cone.vertices) {
                if (std::fabs(v.pos[0]) + std::fabs(v.pos[1]) + std::fabs(v.pos[2]) < 0.01f) {
                    tipAtBase = true;
                }
            }
            check(cone.vertices.size() == 17 * 2 && cone.indices.size() == 16 * 3 && tipAtBase,
                  "ein spitzes Ende wird ein Kegel wie RB_SurfaceCone");
        }

        // Der Blitz: RB_SurfaceElectricity / DoBoltSeg / ApplyShape, als Baender.
        const efx::camera::Vec3 boltFrom{0, 0, 0}, boltTo{200, 0, 0};
        const efx::camera::Vec3 boltEye{100, -500, 0};
        efx::particles::BoltShape boltShape;
        boltShape.radius = 2.0f;
        boltShape.chaos = 1.0f;
        efx::scene::Mesh bolt;
        addElectricity(bolt, boltFrom, boltTo, boltEye, boltShape, 12345, 7u,
                       efx::scene::rgba(200, 200, 255));
        check(!bolt.vertices.empty() && bolt.vertices.size() % 4 == 0,
              "der Blitz besteht aus Baendern zu je vier Ecken");
        // Zwei Stufen ApplyShape: neun Baender je 16-Einheiten-Schritt.
        check(bolt.vertices.size() == (200 / 16) * 9 * 4,
              "neun Feinzacken je Schritt von 16 Einheiten");

        // Beide Enden sitzen exakt — "by nature, we always move from exactly
        // start....to end". Die Bandmitte des ersten Stuecks liegt am Anfang,
        // die des letzten am Ende (jedes Stueck laeuft von cur zurueck nach old).
        auto bandMiddle = [](const efx::scene::Mesh& m, size_t quad, int end) {
            const auto& a = m.vertices[quad * 4 + static_cast<size_t>(end) * 2];
            const auto& b = m.vertices[quad * 4 + static_cast<size_t>(end) * 2 + 1];
            return efx::camera::Vec3{(a.pos[0] + b.pos[0]) * 0.5f,
                                     (a.pos[1] + b.pos[1]) * 0.5f,
                                     (a.pos[2] + b.pos[2]) * 0.5f};
        };
        bool startsAtStart = false, endsAtEnd = false;
        for (size_t q = 0; q < bolt.vertices.size() / 4; ++q) {
            for (int e = 0; e < 2; ++e) {
                const auto m = bandMiddle(bolt, q, e);
                if (efx::camera::length(m - boltFrom) < 0.01f) startsAtStart = true;
                if (efx::camera::length(m - boltTo) < 0.01f) endsAtEnd = true;
            }
        }
        check(startsAtStart, "der Blitz beginnt genau am Anfang");
        check(endsAtEnd, "und endet genau am Ende");

        // Mehr Unruhe heisst mehr Abweichung SENKRECHT zur Geraden.
        auto deviationFor = [&](float chaos) {
            efx::particles::BoltShape shape = boltShape;
            shape.chaos = chaos;
            efx::scene::Mesh set;
            addElectricity(set, boltFrom, boltTo, boltEye, shape, 999, 7u, 0);
            const efx::camera::Vec3 direction = efx::camera::normalise(boltTo - boltFrom);
            float worst = 0.0f;
            for (const auto& v : set.vertices) {
                const efx::camera::Vec3 point{v.pos[0], v.pos[1], v.pos[2]};
                const efx::camera::Vec3 relative = point - boltFrom;
                const float along = efx::camera::dot(relative, direction);
                worst = std::max(worst, efx::camera::length(relative - direction * along));
            }
            return worst;
        };
        check(deviationFor(3.0f) > deviationFor(0.0f) * 2.0f,
              "mit Unruhe weicht der Blitz deutlich weiter ab als ohne");
        std::printf("  Abweichung quer: chaos 0 -> %.1f, chaos 3 -> %.1f\n",
                    deviationFor(0.0f), deviationFor(3.0f));

        // Verschiedene Ausgangswerte, verschiedene Blitze.
        efx::scene::Mesh other;
        addElectricity(other, boltFrom, boltTo, boltEye, boltShape, 54321, 7u, 0);
        bool differs = false;
        for (size_t i = 0; i < std::min(bolt.vertices.size(), other.vertices.size()); ++i) {
            if (std::fabs(bolt.vertices[i].pos[1] - other.vertices[i].pos[1]) > 0.1f) {
                differs = true;
            }
        }
        check(differs, "andere Ausgangswerte ergeben einen anderen Blitz");

        // Randfaelle. Kuerzer als ein Schritt von 16 Einheiten: die Schleife
        // in DoBoltSeg (`for ( i = 16; i <= dis; i += 16 )`) laeuft gar nicht —
        // im Spiel ist so ein Blitz unsichtbar.
        efx::scene::Mesh degenerate;
        addElectricity(degenerate, {5, 5, 5}, {5, 5, 5}, boltEye, boltShape, 1, 1u, 0);
        check(degenerate.vertices.empty(), "null Laenge ergibt keinen Blitz");
        efx::scene::Mesh shortBolt;
        addElectricity(shortBolt, {0, 0, 0}, {5, 0, 0}, boltEye, boltShape, 1, 1u, 0);
        check(shortBolt.vertices.empty(), "kuerzer als 16 Einheiten zeichnet die Engine nichts");
    }

    // --- Ausrichtung ------------------------------------------------------
    //
    // Die drei Menuepunkte "Orient Up / Sideways / Down" haben bis eben eine
    // Einstellung gesetzt, die niemand las. Sie drehen die Effektachse, und
    // alles, was "Forward / Right / Up" heisst, wird damit verrechnet.
    {
        efx::Effect upward;
        upward.primitives.push_back(efx::Primitive{});
        efx::Primitive& q = upward.primitives.back();
        q.type = efx::PrimitiveType::Particle;
        q.life = efx::Range::single(1000.0f);
        q.size.present = true;
        q.size.start = efx::Range::single(5.0f);
        // "Vorwaerts" 100 Einheiten je Sekunde.
        q.velocity.set = true;
        q.velocity.min = {100.0f, 0.0f, 0.0f};
        q.velocity.max = q.velocity.min;

        auto flightAfterOneSecond = [&](int orientation) {
            System oriented;
            oriented.play(upward, 1, {}, axisFor(orientation));
            return oriented.live()[0].positionAt(1000.0f);
        };

        const auto flownUp = flightAfterOneSecond(0);
        const auto flownSide = flightAfterOneSecond(1);
        const auto flownDown = flightAfterOneSecond(2);

        check(std::fabs(flownUp.z - 100.0f) < 0.01f && std::fabs(flownUp.x) < 0.01f,
              "nach oben: der Partikel steigt");
        check(std::fabs(flownSide.x - 100.0f) < 0.01f && std::fabs(flownSide.z) < 0.01f,
              "seitwaerts: er fliegt auf der X-Achse");
        check(std::fabs(flownDown.z + 100.0f) < 0.01f, "nach unten: er faellt");
        std::printf("  Ausrichtung nach 1 s: oben z=%.0f, seitwaerts x=%.0f, "
                    "unten z=%.0f\n", static_cast<double>(flownUp.z),
                    static_cast<double>(flownSide.x),
                    static_cast<double>(flownDown.z));

        // Die Achsen muessen zueinander senkrecht und normiert sein — sonst
        // verzerrt sich der Effekt beim Drehen.
        for (int orientation = 0; orientation < 3; ++orientation) {
            const Axis axis = axisFor(orientation);
            check(std::fabs(efx::camera::length(axis.forward) - 1.0f) < 1e-5f,
                  "Vorwaertsachse normiert");
            check(std::fabs(efx::camera::dot(axis.forward, axis.right)) < 1e-5f,
                  "vorwaerts und rechts stehen senkrecht");
            check(std::fabs(efx::camera::dot(axis.right, axis.up)) < 1e-5f,
                  "rechts und oben ebenso");
        }
        // Ein unbekannter Wert faellt auf "nach oben" zurueck statt auf null.
        check(efx::camera::length(axisFor(99).forward) > 0.9f,
              "ein unbekannter Wert ergibt trotzdem eine gueltige Achse");
    }

    // --- Gruppierung nach Shader -----------------------------------------
    //
    // Ein Zeichenaufruf je Shader statt einer je Partikel. Bei zweihundert
    // Funken ist das der Unterschied zwischen fluessig und ruckelig.
    {
        efx::Effect mixed;
        for (const char* name : {"gfx/a", "gfx/b"}) {
            mixed.primitives.push_back(efx::Primitive{});
            efx::Primitive& q = mixed.primitives.back();
            q.type = efx::PrimitiveType::Particle;
            q.count = efx::Range::single(10.0f);
            q.life = efx::Range::single(1000.0f);
            q.size.present = true;
            q.size.start = efx::Range::single(5.0f);
            q.shaders.push_back(name);
        }
        System grouped;
        grouped.play(mixed, 5);
        const DrawList list = grouped.build(100.0f, right, up);
        check(list.byTexture.size() == 2, "zwei Shader, zwei Gruppen");
        check(list.drawn == 20, "zwanzig Partikel");
        size_t vertices = 0;
        for (const auto& group : list.byTexture) vertices += group.second.vertices.size();
        check(vertices == 80, "zusammen achtzig Ecken");
        std::printf("  20 Partikel mit 2 Shadern -> %zu Zeichenaufrufe\n",
                    list.byTexture.size());

        // Die Reihenfolge muss von Bild zu Bild dieselbe sein — bei
        // alphagemischten Flaechen sieht man sonst Flackern.
        std::vector<std::string> firstOrder, secondOrder;
        for (const auto& group : grouped.build(100.0f, right, up).byTexture) {
            firstOrder.push_back(group.first);
        }
        for (const auto& group : grouped.build(101.0f, right, up).byTexture) {
            secondOrder.push_back(group.first);
        }
        check(firstOrder == secondOrder, "die Reihenfolge bleibt gleich");
    }

    // Eine Primitive mit mehreren Shadern: einer wird beim Ausloesen
    // gewuerfelt und bleibt dann.
    {
        efx::Effect several;
        several.primitives.push_back(efx::Primitive{});
        efx::Primitive& q = several.primitives.back();
        q.type = efx::PrimitiveType::Particle;
        q.count = efx::Range::single(40.0f);
        q.life = efx::Range::single(1000.0f);
        q.size.present = true;
        q.size.start = efx::Range::single(5.0f);
        q.shaders = {"gfx/one", "gfx/two", "gfx/three"};
        System varied;
        varied.play(several, 11);
        std::set<std::string> used;
        for (const auto& item : varied.live()) used.insert(item.shader);
        check(used.size() > 1, "bei vierzig Partikeln kommen mehrere Shader vor");
        for (const auto& name : used) {
            check(name == "gfx/one" || name == "gfx/two" || name == "gfx/three",
                  "und nur solche aus der Liste");
        }
        // Derselbe Partikel behaelt seinen Shader.
        const std::string atFirst = varied.live()[0].shader;
        check(varied.live()[0].shader == atFirst, "und wechselt ihn nicht");
    }

    // --- Wiederholbarkeit ------------------------------------------------
    System a, b;
    a.play(effect, 99);
    b.play(effect, 99);
    check(a.live().size() == b.live().size(), "gleicher Startwert, gleich viele");
    bool identical = true;
    for (size_t i = 0; i < a.live().size(); ++i) {
        if (a.live()[i].spawnMs != b.live()[i].spawnMs) identical = false;
    }
    check(identical, "und dieselben Zeitpunkte");

    // Jeder Typ landet in der richtigen Darstellung. Bis eben wurde alles
    // ausser Particle und Tail als Billboard gezeichnet — ein Cylinder als
    // weisses Rechteck ist irrefuehrender als eine fehlende Textur.
    {
        struct Expect { efx::PrimitiveType type; const char* what; bool asLines; };
        const Expect table[] = {
            {efx::PrimitiveType::Particle, "Billboard", false},
            {efx::PrimitiveType::OrientedParticle, "ausgerichtetes Viereck", false},
            {efx::PrimitiveType::Decal, "ausgerichtetes Viereck", false},
            {efx::PrimitiveType::Cylinder, "Roehre", false},
            {efx::PrimitiveType::Line, "Linie", true},
            {efx::PrimitiveType::Electricity, "Blitz", true},
            {efx::PrimitiveType::Tail, "Linie", true},
        };
        for (const auto& entry : table) {
            efx::Effect one;
            one.primitives.push_back(efx::Primitive{});
            efx::Primitive& q = one.primitives.back();
            q.type = entry.type;
            q.life = efx::Range::single(500.0f);
            q.size.present = true;
            q.size.start = efx::Range::single(10.0f);
            q.length.present = true;
            q.length.start = efx::Range::single(50.0f);
            q.origin2.set = true;
            q.origin2.min = {0.0f, 0.0f, 60.0f};
            q.origin2.max = q.origin2.min;
            q.velocity.set = true;
            q.velocity.min = {50.0f, 0.0f, 0.0f};
            q.velocity.max = q.velocity.min;
            q.shaders.push_back("gfx/x");

            System typed;
            typed.play(one, 3);
            const DrawList list = typed.build(100.0f, right, up);
            const std::string name = std::string(efx::typeName(entry.type)) + " (" +
                                     entry.what + ")";
            check(list.drawn == 1, name + " wird gezeichnet");
            // Linien, Blitze und Schweife sind seit RB_SurfaceLine /
            // RB_SurfaceElectricity Baender aus Dreiecken — es gibt keine
            // 1-Pixel-Linien mehr. `asLines` heisst jetzt: ein Band (Vielfaches
            // von vier Ecken, Breite aus size).
            check(!list.byTexture.empty() && !list.groups.empty(), name + ": als Flaechen");
            if (entry.asLines) {
                check(list.byTexture.begin()->second.vertices.size() % 4 == 0,
                      name + ": als Band aus Vierecken");
            }
        }

        // Der Zylinder hat deutlich mehr Ecken als ein Viereck — daran
        // erkennt man, dass es wirklich eine Roehre ist.
        efx::Effect tube;
        tube.primitives.push_back(efx::Primitive{});
        efx::Primitive& c = tube.primitives.back();
        c.type = efx::PrimitiveType::Cylinder;
        c.life = efx::Range::single(500.0f);
        c.size.present = true;
        c.size.start = efx::Range::single(20.0f);
        c.length.present = true;
        c.length.start = efx::Range::single(100.0f);
        c.origin2.set = true;
        c.origin2.min = {0.0f, 0.0f, 1.0f};
        c.origin2.max = c.origin2.min;
        c.shaders.push_back("gfx/tube");
        System tubes;
        tubes.play(tube, 1);
        const DrawList tubeList = tubes.build(100.0f, right, up);
        check(tubeList.byTexture.begin()->second.vertices.size() > 20,
              "der Zylinder ist eine Roehre, kein Viereck");
        std::printf("  Zylinder: %zu Ecken statt 4\n",
                    tubeList.byTexture.begin()->second.vertices.size());
    }

    // --- Emitter und FxRunner ---------------------------------------------
    //
    // Beide starten andere .efx-Dateien. Das ist kein Zeichenproblem, sondern
    // eines der Struktur: die Vorschau muss nachladen und rekursiv abspielen.
    {
        // Ein untergeordneter Effekt: ein einzelner Partikel.
        efx::Effect child;
        child.primitives.push_back(efx::Primitive{});
        {
            efx::Primitive& c = child.primitives.back();
            c.type = efx::PrimitiveType::Particle;
            c.life = efx::Range::single(200.0f);
            c.size.present = true;
            c.size.start = efx::Range::single(4.0f);
            c.shaders.push_back("gfx/spark");
        }

        int asked = 0;
        auto loader = [&](const std::string& name) -> const efx::Effect* {
            ++asked;
            return name == "kinder/funke" ? &child : nullptr;
        };

        // --- FxRunner: startet einmal, zeichnet nichts ---------------------
        {
            efx::Effect runner;
            runner.primitives.push_back(efx::Primitive{});
            efx::Primitive& r = runner.primitives.back();
            r.type = efx::PrimitiveType::FxRunner;
            r.delay.set = true;
            r.delay.min = r.delay.max = 100.0f;
            r.playFx.push_back("kinder/funke");

            System started;
            started.play(runner, 1, {}, {}, loader);
            check(started.startedEffects() == 1, "der FxRunner startet einen Effekt");
            check(started.live().size() == 1,
                  "und der Runner selbst steht nicht in der Liste");
            check(started.live()[0].type == efx::PrimitiveType::Particle,
                  "sondern der Partikel des Kindes");
            // Der Versatz wird weitergereicht: das Kind startet, wenn der
            // Runner erscheint.
            check(std::fabs(started.live()[0].spawnMs - 100.0f) < 0.01f,
                  "das Kind startet mit dem Versatz des Runners");
            std::printf("  FxRunner: %d Effekt gestartet, Kind bei %.0f ms\n",
                        started.startedEffects(),
                        static_cast<double>(started.live()[0].spawnMs));
        }

        // Ein Name, den es nicht gibt, wird gezaehlt statt verschluckt.
        {
            efx::Effect runner;
            runner.primitives.push_back(efx::Primitive{});
            runner.primitives.back().type = efx::PrimitiveType::FxRunner;
            runner.primitives.back().playFx.push_back("gibt/es/nicht");
            System missing;
            missing.play(runner, 1, {}, {}, loader);
            check(missing.missingEffects() == 1, "ein fehlender Effekt wird gezaehlt");
            check(missing.startedEffects() == 0, "und keiner gestartet");
        }

        // Ohne Lader passiert nichts, aber es stuerzt auch nichts ab.
        {
            efx::Effect runner;
            runner.primitives.push_back(efx::Primitive{});
            runner.primitives.back().type = efx::PrimitiveType::FxRunner;
            runner.primitives.back().playFx.push_back("kinder/funke");
            System noLoader;
            noLoader.play(runner, 1);
            check(noLoader.startedEffects() == 0, "ohne Lader wird nichts gestartet");
        }

        // --- Emitter: sendet nach STRECKE aus, nicht nach Zeit -------------
        //
        // CEmitter::UpdateEmitter vergleicht den Abstand zum letzten
        // Aussenden mit density +- variance. Bei 100 Einheiten je Sekunde,
        // einer Sekunde Leben und density 25 sind das etwa vier.
        {
            efx::Effect emitter;
            emitter.primitives.push_back(efx::Primitive{});
            efx::Primitive& e = emitter.primitives.back();
            e.type = efx::PrimitiveType::Emitter;
            e.life = efx::Range::single(1000.0f);
            e.velocity.set = true;
            e.velocity.min = {100.0f, 0.0f, 0.0f};
            e.velocity.max = e.velocity.min;
            e.density = efx::Range::single(25.0f);
            // emitfx, nicht playfx: CEmitter sendet mEmitterFxHandles aus
            // (FxScheduler.cpp, FX_AddEmitter). Der Test hatte playfx — und
            // pruefte damit genau den Fehler mit, dass in der Vorschau kein
            // einziger Raven-Emitter aussandte.
            e.emitFx.push_back("kinder/funke");

            System emitting;
            emitting.play(emitter, 1, {}, {}, loader);
            check(emitting.startedEffects() >= 3 && emitting.startedEffects() <= 5,
                  "etwa vier Aussendungen bei 100 Einheiten und density 25");
            std::printf("  Emitter: 100 Einheiten Weg, density 25 -> %d Aussendungen\n",
                        emitting.startedEffects());

            // Doppelte Dichte heisst halb so viele.
            efx::Effect sparse = emitter;
            sparse.primitives[0].density = efx::Range::single(50.0f);
            System sparser;
            sparser.play(sparse, 1, {}, {}, loader);
            check(sparser.startedEffects() < emitting.startedEffects(),
                  "groessere Dichte heisst weniger Aussendungen");

            // Ohne density gilt 10 — nicht 0. Der Erzeuger von
            // CPrimitiveTemplate setzt `mDensity.SetRange( 10.0f, 10.0f )`,
            // mit Ravens Kommentar "default this high so it doesn't do bad
            // things". Hier stand "ohne density wird nichts ausgesendet".
            // 100 Einheiten Weg bei Schritt 10 +- 1: rund zehn Aussendungen.
            efx::Effect plain = emitter;
            plain.primitives[0].density.set = false;
            System defaulted;
            defaulted.play(plain, 1, {}, {}, loader);
            check(defaulted.startedEffects() >= 8 && defaulted.startedEffects() <= 12,
                  "ohne density gilt die Voreinstellung 10: rund zehn Aussendungen");
        }

            // --- Physik am Partikel -------------------------------------------
        {
            const auto planes = efx::sim::roomPlanes(200.0f, 200.0f, 300.0f);

            auto falling = [&](uint32_t flags, float bounce) {
                efx::Effect e;
                e.primitives.push_back(efx::Primitive{});
                efx::Primitive& q = e.primitives.back();
                q.type = efx::PrimitiveType::Particle;
                q.life = efx::Range::single(2000.0f);
                q.size.present = true;
                q.size.start = efx::Range::single(4.0f);
                q.origin.set = true;
                // ACHTUNG: das erste Feld ist "vorwaerts", nicht "x".
                //
                // Die Effektachse rechnet um: org = forward*x + right*y + up*z.
                // Bei der Voreinstellung "nach oben" ist forward = (0,0,1) —
                // also legt der ERSTE Wert die Hoehe fest.
                //
                // Mein erster Anlauf schrieb (0, 0, 100) und meinte hundert
                // Einheiten hoch. Herausgekommen ist (0, 100, 0): der Partikel
                // stand auf dem Boden, prallte sofort auf, und killOnImpact
                // toetete ihn bei 0 ms. Das Programm rechnete richtig — der
                // Test hatte Weltkoordinaten angenommen, wo Effektkoordinaten
                // stehen.
                q.origin.min = {100.0f, 0.0f, 0.0f};
                q.origin.max = q.origin.min;
                q.gravity = efx::Range::single(-800.0f);
                q.elasticity = efx::Range::single(bounce);
                q.flags = flags;
                q.shaders.push_back("gfx/x");
                return e;
            };

            // Ohne das Flag faellt es durch den Boden — wie bisher.
            {
                System through;
                through.play(falling(0, 0.5f), 1, {}, {}, {}, planes);
                check(!through.live()[0].hasPath,
                      "ohne usePhysics wird keine Bahn gebaut");
                check(through.live()[0].positionAt(1500.0f).z < -100.0f,
                      "und der Partikel faellt durch den Boden");
            }

            // Mit dem Flag prallt es ab.
            {
                System bouncing;
                bouncing.play(falling(efx::kFlagApplyPhysics, 0.5f), 1, {}, {}, {},
                              planes);
                const Live& item = bouncing.live()[0];
                check(item.hasPath, "mit usePhysics wird eine Bahn gebaut");
                check(!item.path.impactMs.empty(), "und es prallt ab");
                bool aboveFloor = true;
                for (float t = item.spawnMs; t < item.deathMs; t += 10.0f) {
                    if (item.positionAt(t).z < -1.0f) aboveFloor = false;
                }
                check(aboveFloor, "der Partikel bleibt ueber dem Boden");
                std::printf("  Partikel mit Physik: %zu Aufpralle\n",
                            item.path.impactMs.size());
            }

            // killOnImpact verkuerzt die Lebensdauer bis zum Aufprall.
            {
                System dying;
                dying.play(falling(efx::kFlagApplyPhysics | efx::kFlagKillOnImpact,
                                   0.5f), 1, {}, {}, {}, planes);
                const Live& item = dying.live()[0];
                check(item.path.killed, "killOnImpact schlaegt zu");
                check(item.deathMs < 2000.0f,
                      "und die Lebensdauer endet vor der Zeit");
                std::printf("  killOnImpact: statt 2000 ms nur %.0f ms\n",
                            static_cast<double>(item.deathMs));
            }

            // --- impactfx: bei jedem Aufprall ein Effekt ------------------
            {
                efx::Effect impactChild;
                impactChild.primitives.push_back(efx::Primitive{});
                {
                    efx::Primitive& c = impactChild.primitives.back();
                    c.type = efx::PrimitiveType::Particle;
                    c.life = efx::Range::single(80.0f);
                    c.size.present = true;
                    c.size.start = efx::Range::single(2.0f);
                    c.velocity.set = true;
                    c.velocity.min = {60.0f, 0.0f, 0.0f};  // entlang der Achse
                    c.velocity.max = c.velocity.min;
                    c.shaders.push_back("gfx/impact");
                }
                auto impactLoader = [&](const std::string& n) -> const efx::Effect* {
                    return n == "kinder/einschlag" ? &impactChild : nullptr;
                };

                auto withImpact = [&](uint32_t flags) {
                    efx::Effect e = falling(flags, 0.5f);
                    e.primitives[0].impactFx.push_back("kinder/einschlag");
                    return e;
                };

                // Auch ohne das Flag in der Datei: die Liste setzt es
                // (ParseImpactFxStrings, FxTemplate.cpp). Hier stand "ohne
                // das Flag passiert nichts" — im Spiel passiert es.
                {
                    System implied;
                    implied.play(withImpact(efx::kFlagApplyPhysics), 1, {}, {},
                                 impactLoader, planes);
                    check(implied.startedEffects() > 0,
                          "die impactfx-Liste allein startet den Aufpralleffekt");
                }

                // Mit dem Flag schon, und einmal je Aufprall.
                {
                    System hitting;
                    hitting.play(withImpact(efx::kFlagApplyPhysics |
                                            efx::kFlagImpactRunsFx),
                                 1, {}, {}, impactLoader, planes);
                    check(hitting.startedEffects() > 0, "mit dem Flag schon");
                    check(hitting.startedEffects() <= 8,
                          "und hoechstens acht, damit ein huepfender Funke die "
                          "Vorschau nicht flutet");
                    std::printf("  impactfx: %d Effekte bei den Aufprallen\n",
                                hitting.startedEffects());

                    // Der erste Aufpralleffekt startet zur Aufprallzeit und
                    // steht am Aufprallort — auf dem Boden.
                    const Live* firstImpact = nullptr;
                    for (const auto& item : hitting.live()) {
                        if (item.spawnMs > 1.0f &&
                            (!firstImpact || item.spawnMs < firstImpact->spawnMs)) {
                            firstImpact = &item;
                        }
                    }
                    check(firstImpact != nullptr, "es gibt einen Aufpralleffekt");
                    if (firstImpact) {
                        check(std::fabs(firstImpact->origin.z) < 1.0f,
                              "er steht auf dem Boden");
                        // Die Achse ist die Flaechennormale: nach oben. Ein
                        // Kind mit velocity "vorwaerts" fliegt also aufwaerts.
                        check(firstImpact->velocity.z > 50.0f,
                              "und zeigt von der Flaeche weg, nicht zufaellig");
                        std::printf("  Aufprallachse: Kind fliegt mit z=%.0f "
                                    "(von der Flaeche weg)\n",
                                    static_cast<double>(firstImpact->velocity.z));
                    }
                }

                // Auch usePhysics braucht es nicht: dieselbe Funktion setzt
                // FX_APPLY_PHYSICS mit. Ohne jedes Flag prallt es also ab und
                // startet seinen Effekt — wie im Spiel.
                {
                    System bare;
                    bare.play(withImpact(0), 1, {}, {}, impactLoader, planes);
                    // Das Elternteil steht HINTER seinen Kindern in live() —
                    // es wird erst nach ihnen eingetragen. Gesucht wird also
                    // das eine mit Bahn.
                    bool anyPath = false;
                    for (const auto& item : bare.live()) anyPath |= item.hasPath;
                    check(anyPath, "die impactfx-Liste schaltet die Physik ein");
                    check(bare.startedEffects() > 0,
                          "und der Aufpralleffekt startet ohne jedes Flag");
                }
            }

            // Ohne Flaechen faellt auch mit Flag alles durch — die Vorschau
            // ohne Raum soll nicht an einer unsichtbaren Ebene abprallen.
            {
                System noRoom;
                noRoom.play(falling(efx::kFlagApplyPhysics, 0.5f), 1);
                check(!noRoom.live()[0].hasPath, "ohne Flaechen keine Bahn");
            }
        }

    // --- deathFx: beim Sterben einen Effekt starten --------------------
        //
        // Steht in fast jeder Geschossdatei: das Projektil fliegt, beim
        // Aufschlag kommt die Explosion.
        {
            auto projectile = [&](uint32_t flags) {
                efx::Effect e;
                e.primitives.push_back(efx::Primitive{});
                efx::Primitive& q = e.primitives.back();
                q.type = efx::PrimitiveType::Particle;
                q.life = efx::Range::single(400.0f);
                q.size.present = true;
                q.size.start = efx::Range::single(5.0f);
                q.velocity.set = true;
                q.velocity.min = {200.0f, 0.0f, 0.0f};
                q.velocity.max = q.velocity.min;
                q.shaders.push_back("gfx/bolt");
                q.deathFx.push_back("kinder/funke");
                q.flags = flags;
                return e;
            };

            // Auch ohne das Flag in der Datei: ParseDeathFxStrings setzt
            // FX_DEATH_RUNS_FX selbst (FxTemplate.cpp, SP und MP). Hier stand
            // "ein gesetztes deathfx allein tut nichts" — das galt nur fuer
            // unsere Vorschau.
            {
                System noFlag;
                noFlag.play(projectile(0), 1, {}, {}, loader);
                check(noFlag.startedEffects() == 1,
                      "die deathfx-Liste allein loest den Todeseffekt aus");
            }

            // Mit dem Flag schon.
            {
                System dying;
                dying.play(projectile(efx::kFlagDeathRunsFx), 1, {}, {}, loader);
                check(dying.startedEffects() == 1, "mit dem Flag wird es ausgeloest");
                check(dying.live().size() == 2,
                      "das Geschoss und der Todeseffekt");

                // Der Kindeffekt startet, wenn das Geschoss stirbt — nicht
                // wenn es erscheint.
                float latest = 0.0f;
                for (const auto& item : dying.live()) {
                    latest = std::max(latest, item.spawnMs);
                }
                check(std::fabs(latest - 400.0f) < 0.01f,
                      "der Todeseffekt startet beim Tod, nach 400 ms");

                // Und dort, wo das Geschoss gestorben ist: 200 Einheiten je
                // Sekunde mal 0.4 Sekunden — aber entlang der Effektachse,
                // die nach oben zeigt.
                const Live* spawned = nullptr;
                for (const auto& item : dying.live()) {
                    if (item.spawnMs > 1.0f) spawned = &item;
                }
                check(spawned != nullptr, "es gibt einen Kindeffekt");
                if (spawned) {
                    check(std::fabs(spawned->origin.z - 80.0f) < 1.0f,
                          "und er steht am Sterbeort des Geschosses");
                    std::printf("  deathFx: Kind startet bei %.0f ms, "
                                "Hoehe %.0f Einheiten\n",
                                static_cast<double>(spawned->spawnMs),
                                static_cast<double>(spawned->origin.z));
                }
            }

            // killOnImpact: beim natuerlichen Tod LAEUFT deathfx trotzdem —
            // FxUtil.cpp loescht das Flag vor FX_FreeMember ("this flag just
            // has to be cleared otherwise death effects might not happen
            // correctly"). Nur beim Tod durch Aufprall bleibt es gesetzt, und
            // CParticle::Die startet nichts. Hier stand das Gegenteil.
            {
                System natural;
                natural.play(projectile(efx::kFlagDeathRunsFx |
                                        efx::kFlagKillOnImpact),
                             1, {}, {}, loader);
                check(natural.startedEffects() == 1,
                      "mit killOnImpact beim natuerlichen Tod trotzdem");

                // Mit Boden, nach unten geschossen: Tod durch Aufprall.
                efx::Effect falling = projectile(efx::kFlagDeathRunsFx |
                                                 efx::kFlagKillOnImpact |
                                                 efx::kFlagApplyPhysics);
                falling.primitives[0].origin.set = true;
                falling.primitives[0].origin.min = {50.0f, 0.0f, 0.0f};
                falling.primitives[0].origin.max = falling.primitives[0].origin.min;
                falling.primitives[0].velocity.min = {-400.0f, 0.0f, 0.0f};
                falling.primitives[0].velocity.max = falling.primitives[0].velocity.min;
                System impact;
                impact.play(falling, 1, {}, {}, loader,
                            efx::sim::roomPlanes(500.0f, 500.0f, 500.0f));
                check(impact.startedEffects() == 0,
                      "aber nicht beim Tod durch Aufprall");
            }

            // Ein fehlender Todeseffekt wird gezaehlt, nicht verschluckt.
            {
                efx::Effect missing = projectile(efx::kFlagDeathRunsFx);
                missing.primitives[0].deathFx[0] = "gibt/es/nicht";
                System lost;
                lost.play(missing, 1, {}, {}, loader);
                check(lost.missingEffects() == 1,
                      "ein fehlender Todeseffekt wird gezaehlt");
            }

            // Die Achse ist zufaellig, nicht die Flugrichtung. Ravens
            // Kommentar dazu: "Man, this just seems so, like, uncool and
            // stuff..." — aber es ist, was dasteht.
            //
            // Geprueft ueber mehrere Ausgangswerte: zeigt der Kindeffekt
            // immer gleich, waere die Achse fest.
            {
                efx::Effect child2;
                child2.primitives.push_back(efx::Primitive{});
                {
                    efx::Primitive& c = child2.primitives.back();
                    c.type = efx::PrimitiveType::Particle;
                    c.life = efx::Range::single(100.0f);
                    c.size.present = true;
                    c.size.start = efx::Range::single(2.0f);
                    c.velocity.set = true;
                    c.velocity.min = {50.0f, 0.0f, 0.0f};  // entlang der Achse
                    c.velocity.max = c.velocity.min;
                    c.shaders.push_back("gfx/x");
                }
                auto childLoader = [&](const std::string& n) -> const efx::Effect* {
                    return n == "kinder/funke" ? &child2 : nullptr;
                };

                std::set<int> directions;
                for (unsigned seed = 1; seed <= 12; ++seed) {
                    System varied;
                    varied.play(projectile(efx::kFlagDeathRunsFx), seed, {}, {},
                                childLoader);
                    for (const auto& item : varied.live()) {
                        if (item.spawnMs > 1.0f) {
                            // Die Flugrichtung des Kindes grob einordnen.
                            const auto v = efx::camera::normalise(item.velocity);
                            directions.insert(static_cast<int>(v.x * 4) * 100 +
                                              static_cast<int>(v.y * 4) * 10 +
                                              static_cast<int>(v.z * 4));
                        }
                    }
                }
                check(directions.size() > 3,
                      "der Todeseffekt zeigt in wechselnde Richtungen");
                std::printf("  deathFx-Achse: %zu verschiedene Richtungen bei "
                            "12 Ausgangswerten\n", directions.size());
            }
        }

        // --- Ein Effekt, der sich selbst startet ---------------------------
        //
        // In .efx-Dateien nicht verboten. Ohne Tiefenbegrenzung haengt sich
        // die Vorschau daran auf.
        {
            efx::Effect recursive;
            recursive.primitives.push_back(efx::Primitive{});
            recursive.primitives.back().type = efx::PrimitiveType::FxRunner;
            recursive.primitives.back().playFx.push_back("selbst");

            auto selfLoader = [&](const std::string& name) -> const efx::Effect* {
                return name == "selbst" ? &recursive : nullptr;
            };
            System looping;
            looping.play(recursive, 1, {}, {}, selfLoader);
            check(looping.startedEffects() < 10,
                  "ein Effekt, der sich selbst startet, wird begrenzt");
            std::printf("  Selbstaufruf: bei Tiefe %d abgebrochen nach %d Starts\n",
                        efx::particles::kMaxEffectDepth, looping.startedEffects());
        }

        // Ein Emitter mit unsinnig kleiner Dichte darf die Vorschau nicht
        // anhalten.
        {
            efx::Effect flood;
            flood.primitives.push_back(efx::Primitive{});
            efx::Primitive& f = flood.primitives.back();
            f.type = efx::PrimitiveType::Emitter;
            f.life = efx::Range::single(10000.0f);
            f.velocity.set = true;
            f.velocity.min = {1000.0f, 0.0f, 0.0f};
            f.velocity.max = f.velocity.min;
            f.density = efx::Range::single(0.01f);
            f.emitFx.push_back("kinder/funke");
            System flooded;
            flooded.play(flood, 1, {}, {}, loader);
            check(flooded.startedEffects() > 0 && flooded.startedEffects() <= 256,
                  "eine unsinnig kleine Dichte wird gedeckelt");
        }

        check(asked > 0, "der Lader wurde ueberhaupt gefragt");
    }

    // --- Typen ohne Bild --------------------------------------------------
    // Ein Sound-Segment merkt sich, welchen Klang es spielt.
    {
        efx::Effect noisy;
        noisy.primitives.push_back(efx::Primitive{});
        efx::Primitive& n = noisy.primitives.back();
        n.type = efx::PrimitiveType::Sound;
        n.delay.set = true;
        n.delay.min = n.delay.max = 250.0f;
        n.sounds.push_back("sound/weapons/fire.wav");
        System sounding;
        sounding.play(noisy, 1);
        check(sounding.live().size() == 1, "das Sound-Segment lebt");
        check(sounding.live()[0].soundName == "sound/weapons/fire.wav",
              "und kennt seinen Klang");
        check(!sounding.live()[0].soundPlayed,
              "gespielt wurde er noch nicht");
        check(std::fabs(sounding.live()[0].spawnMs - 250.0f) < 0.01f,
              "und startet mit seinem Versatz");

        // Mehrere Klaenge: einer wird ausgewuerfelt, wie bei den Shadern.
        efx::Effect several;
        several.primitives.push_back(efx::Primitive{});
        several.primitives.back().type = efx::PrimitiveType::Sound;
        several.primitives.back().count = efx::Range::single(30.0f);
        several.primitives.back().sounds = {"a.wav", "b.wav", "c.wav"};
        System varied;
        varied.play(several, 5);
        std::set<std::string> used;
        for (const auto& item : varied.live()) used.insert(item.soundName);
        check(used.size() > 1, "bei dreissig Segmenten kommen mehrere Klaenge vor");
        for (const auto& name : used) {
            check(name == "a.wav" || name == "b.wav" || name == "c.wav",
                  "und nur solche aus der Liste");
        }
    }

    // Ein FxRunner steht NICHT mehr in der Liste: er startet einen anderen
    // Effekt und ist damit fertig. Genau so macht es die Engine — der
    // Scheduler ruft fuer Fx_RUNNER direkt PlayEffect und legt keine
    // Primitive an.
    //
    // Der Test erwartete vorher, dass er "lebt". Das war schon damals falsch,
    // fiel aber nicht auf, weil ohne Lader ohnehin nichts passierte.
    for (auto type : {efx::PrimitiveType::Sound,
                      efx::PrimitiveType::CameraShake}) {
        efx::Effect quiet;
        quiet.primitives.push_back(efx::Primitive{});
        quiet.primitives.back().type = type;
        quiet.primitives.back().life = efx::Range::single(500.0f);
        System silent;
        silent.play(quiet, 1);
        const DrawList list = silent.build(100.0f, right, up);
        check(list.alive == 1, std::string(efx::typeName(type)) + " lebt");
        check(list.drawn == 0,
              std::string(efx::typeName(type)) + " wird nicht gezeichnet");
    }

    // Ein Tail bekommt eine Linie, kein Viereck.
    {
        efx::Effect trail;
        trail.primitives.push_back(efx::Primitive{});
        efx::Primitive& t = trail.primitives.back();
        t.type = efx::PrimitiveType::Tail;
        t.life = efx::Range::single(500.0f);
        t.velocity.set = true;
        t.velocity.min = {100.0f, 0.0f, 0.0f};
        t.velocity.max = t.velocity.min;
        t.length.present = true;
        t.length.start = efx::Range::single(30.0f);
        System trailing;
        trailing.play(trail, 1);
        const DrawList list = trailing.build(100.0f, right, up);
        // CTail ist ein RT_LINE: ein Band (DoLine) von der Spitze nach hinten.
        const auto& band = list.byTexture.begin()->second.vertices;
        check(list.byTexture.size() == 1 && band.size() == 4, "der Tail ist ein Band");
        // Die Laenge in drei Dimensionen messen, nicht nur entlang X.
        //
        // Frueher stand hier nur die X-Komponente — das ging, solange die
        // Effektachse die Einheitsachse war. Seit "Orient Up" wirklich dreht,
        // zeigt "vorwaerts" nach oben, und die Messung entlang X ergab null.
        //
        // Der Test war zu eng, nicht die Aenderung falsch: gemessen werden
        // sollte die Laenge, nicht eine Achse davon.
        // Ecke 0 liegt am Anfang, Ecke 2 auf derselben Seite am Ende.
        const efx::camera::Vec3 tail{band[2].pos[0] - band[0].pos[0],
                                     band[2].pos[1] - band[0].pos[1],
                                     band[2].pos[2] - band[0].pos[2]};
        check(std::fabs(efx::camera::length(tail) - 30.0f) < 0.5f,
              "dreissig Einheiten lang");
        // Und entgegen der Flugrichtung — die jetzt nach oben zeigt.
        check(tail.z < -1.0f, "und entgegen der Flugrichtung");
    }

    // Ein leerer Effekt haengt nichts auf.
    {
        System empty;
        empty.play(efx::Effect{}, 1);
        check(!empty.playing(), "ein leerer Effekt laeuft nicht");
        check(empty.build(0.0f, right, up).drawn == 0, "und zeichnet nichts");
    }
}

void testInflate() {
    std::cout << "== Auspacken ==\n";
    namespace fs = std::filesystem;

    // Gegen echte Deflate-Daten, nicht gegen meine Vorstellung davon: die
    // gepackten Bytes unten stammen aus Pythons zlib, in drei Stufen und mit
    // verschiedenen Inhalten, damit alle drei Blockarten vorkommen.
    //
    // **Eingebettet, nicht aus Dateien gelesen.** Der erste Anlauf las sie aus
    // /tmp — dort lagen sie, weil ich sie dort erzeugt hatte. Auf einem
    // anderen Rechner gab es sie nicht, der Test fand null Faelle und wurde
    // rot. Ein Test, der von der Umgebung seines Autors abhaengt, prueft
    // anderswo gar nichts.
    //
    // Die erwarteten Daten stehen als Erzeugungsvorschrift statt als Rohbytes
    // da — sonst waere die Testdatei um ein Vielfaches groesser als der
    // Auspacker.

// Fixed: 9 Byte roh, 7 Byte gepackt
const unsigned char kFixedPacked[] = {
    75, 76, 74, 78, 4, 35, 0,
};

// Dynamic: 2280 Byte roh, 74 Byte gepackt
const unsigned char kDynamicPacked[] = {
    237, 203, 203, 13, 128, 48, 12, 4, 209, 86, 182, 2, 170, 136, 16, 109,
    228, 99, 8, 146, 101, 33, 27, 247, 79, 202, 224, 176, 167, 185, 204, 43,
    226, 136, 62, 77, 84, 5, 205, 107, 154, 96, 207, 62, 3, 241, 248, 109,
    215, 139, 148, 182, 158, 33, 134, 179, 166, 174, 28, 105, 99, 67, 33, 36,
    36, 36, 36, 36, 36, 36, 252, 55, 252, 0,
};

// Runs: 3001 Byte roh, 22 Byte gepackt
const unsigned char kRunsPacked[] = {
    237, 193, 33, 1, 0, 0, 0, 2, 160, 141, 86, 255, 15, 241, 134, 1,
    104, 0, 0, 128, 123, 3,
};

// Binary: 4000 Byte roh, 434 Byte gepackt
const unsigned char kBinaryPacked[] = {
    237, 209, 135, 54, 22, 96, 0, 6, 224, 63, 178, 35, 201, 42, 163, 40,
    100, 134, 178, 26, 100, 166, 223, 104, 144, 36, 163, 172, 138, 172, 16, 26,
    40, 123, 75, 217, 179, 68, 90, 84, 132, 208, 208, 50, 10, 77, 35, 123,
    101, 43, 50, 162, 194, 123, 29, 206, 119, 9, 207, 121, 40, 34, 84, 151,
    235, 79, 59, 86, 138, 27, 186, 39, 61, 235, 97, 148, 62, 228, 149, 86,
    249, 99, 149, 156, 201, 133, 172, 183, 195, 236, 10, 102, 126, 183, 106, 198,
    57, 85, 44, 3, 242, 234, 38, 121, 119, 91, 135, 220, 255, 52, 195, 175,
    110, 31, 241, 240, 219, 252, 70, 109, 135, 152, 162, 150, 133, 205, 251, 156,
    175, 149, 182, 211, 108, 49, 56, 155, 88, 209, 77, 47, 117, 240, 92, 234,
    203, 126, 102, 217, 195, 231, 51, 223, 12, 177, 109, 63, 234, 155, 93, 61,
    198, 161, 108, 113, 229, 246, 135, 9, 238, 93, 39, 130, 239, 125, 156, 94,
    191, 199, 46, 188, 224, 235, 156, 160, 214, 233, 232, 194, 230, 255, 194, 186,
    78, 113, 37, 109, 43, 68, 245, 221, 18, 202, 187, 232, 36, 14, 120, 166,
    188, 232, 99, 146, 49, 246, 201, 120, 61, 200, 42, 111, 122, 233, 102, 213,
    232, 26, 69, 243, 203, 185, 239, 127, 113, 237, 56, 30, 116, 183, 97, 106,
    157, 170, 109, 88, 254, 151, 63, 2, 26, 167, 162, 30, 55, 253, 19, 210,
    57, 115, 181, 184, 149, 34, 66, 117, 141, 47, 235, 4, 192, 35, 249, 121,
    47, 0, 222, 233, 175, 6, 0, 184, 120, 227, 221, 8, 0, 254, 57, 181,
    63, 1, 8, 188, 83, 255, 27, 128, 208, 7, 159, 103, 1, 136, 124, 212,
    248, 23, 128, 216, 39, 223, 23, 1, 0, 159, 22, 0, 240, 25, 0, 0,
    159, 5, 0, 240, 87, 3, 0, 254, 90, 0, 192, 231, 1, 0, 124, 62,
    0, 192, 223, 0, 0, 248, 155, 0, 0, 95, 12, 0, 240, 37, 1, 0,
    127, 43, 0, 224, 111, 3, 0, 124, 37, 0, 192, 223, 9, 0, 248, 106,
    0, 128, 175, 9, 0, 248, 123, 1, 0, 95, 15, 0, 240, 247, 3, 0,
    190, 17, 0, 224, 31, 1, 0, 252, 99, 0, 128, 111, 5, 0, 248, 54,
    0, 128, 127, 18, 0, 240, 29, 1, 160, 144, 63, 242, 71, 254, 200, 31,
    249, 35, 127, 228, 143, 252, 145, 63, 242, 71, 254, 200, 31, 249, 91, 22,
    127, 75,
};

    struct Case {
        const char* name;
        const unsigned char* packed;
        size_t packedSize;
        std::vector<unsigned char> expected;
    };

    auto repeat = [](const std::string& text, int times) {
        std::vector<unsigned char> out;
        for (int i = 0; i < times; ++i) {
            out.insert(out.end(), text.begin(), text.end());
        }
        return out;
    };
    std::vector<unsigned char> runs{'Z'};
    runs.insert(runs.end(), 3000, 'Q');
    std::vector<unsigned char> binary(4000);
    for (int i = 0; i < 4000; ++i) {
        binary[static_cast<size_t>(i)] =
            static_cast<unsigned char>((i * 37 + i / 7) % 256);
    }

    const Case cases[] = {
        {"fixed", kFixedPacked, sizeof(kFixedPacked), repeat("abcabcabc", 1)},
        {"dynamic", kDynamicPacked, sizeof(kDynamicPacked),
         repeat("Der schnelle braune Fuchs springt ueber den faulen Hund. ", 40)},
        {"runs", kRunsPacked, sizeof(kRunsPacked), runs},
        {"binary", kBinaryPacked, sizeof(kBinaryPacked), binary},
    };

    size_t totalOut = 0;
    for (const auto& entry : cases) {
        const auto result = efx::inflate::raw(entry.packed, entry.packedSize,
                                              entry.expected.size());
        check(result.ok, std::string(entry.name) + " laesst sich auspacken");
        check(result.data == entry.expected,
              std::string(entry.name) + " ergibt byteweise dasselbe");
        totalOut += entry.expected.size();
    }

    // Und ein gespeicherter Block, hier von Hand gebaut: Kopf, Laenge,
    // Einerkomplement, dann die Bytes.
    {
        std::vector<unsigned char> stored{0x01, 0x0A, 0x00, 0xF5, 0xFF};
        for (int i = 0; i < 10; ++i) {
            stored.push_back(static_cast<unsigned char>('a' + i));
        }
        const auto result = efx::inflate::raw(stored.data(), stored.size(), 10);
        check(result.ok, "gespeicherter Block laesst sich lesen");
        check(result.data.size() == 10 && result.data[0] == 'a' &&
                  result.data[9] == 'j',
              "und ergibt genau die zehn Bytes");
        totalOut += 10;
    }

    std::printf("  %zu Faelle von zlib, zusammen %zu Byte ausgepackt\n",
                sizeof(cases) / sizeof(cases[0]) + 1, totalOut);

    // --- Boesartige Eingaben ---------------------------------------------
    //
    // Ein .pk3 liegt im Spielordner, und da kommt alles Moegliche her. Keiner
    // dieser Faelle darf abstuerzen oder unbegrenzt Speicher nehmen.
    using efx::inflate::raw;

    check(!raw(nullptr, 0, 100).ok, "Nullzeiger wird abgewiesen");
    const unsigned char empty[1] = {0};
    check(!raw(empty, 0, 100).ok, "Laenge null ebenso");

    // Abgeschnitten mitten im Block.
    {
        const unsigned char truncated[] = {0x78, 0x9C, 0x4B};
        const auto result = raw(truncated, sizeof(truncated), 1000);
        check(std::string(result.error).size() > 0 || result.ok,
              "abgeschnittene Eingabe liefert eine Begruendung oder nichts");
    }

    // Reservierte Blockart 3.
    {
        const unsigned char reserved[] = {0x07};  // last=1, type=3
        check(!raw(reserved, sizeof(reserved), 100).ok,
              "reservierte Blockart wird abgewiesen");
    }

    // Gespeicherter Block mit falscher Laengenpruefung.
    {
        const unsigned char bad[] = {0x01, 0x05, 0x00, 0x00, 0x00,
                                     'a', 'b', 'c', 'd', 'e'};
        const auto result = raw(bad, sizeof(bad), 100);
        check(!result.ok, "falsche Laengenpruefung wird abgewiesen");
        check(result.error.find("check") != std::string::npos,
              "und genau so benannt");
    }

    // Unsinnig grosse angekuendigte Groesse.
    {
        const unsigned char small[] = {0x03, 0x00};
        const auto result = raw(small, sizeof(small), 4ull * 1024 * 1024 * 1024);
        check(!result.ok, "eine unsinnige Groesse bekommt keinen Speicher");
        check(result.error.find("implausible") != std::string::npos,
              "mit klarer Begruendung");
    }

    // Der wichtigste Fall: ein Rueckverweis vor den Anfang. Genau damit
    // bricht man einen naiven Auspacker auf — er liest dann fremden Speicher.
    //
    // Von Hand gebaut: fester Huffman-Block, sofort ein Laengen-/Abstandspaar
    // ohne vorher ausgegebene Bytes.
    {
        // 1 (last) + 01 (fixed) + Symbol 257 (Laenge 3) + Abstand 1
        // Symbol 257 hat im festen Baum den Code 0000001 (7 Bit).
        const unsigned char attack[] = {0x63, 0x00, 0x00, 0x00, 0x00, 0x00};
        const auto result = raw(attack, sizeof(attack), 1000);
        // Entweder abgewiesen oder leer — auf keinen Fall Fremdspeicher.
        check(!result.ok || result.data.size() <= 1000,
              "Rueckverweis vor den Anfang fuehrt nicht ins Verderben");
    }

    // Zufaellige Bytes: nichts davon darf abstuerzen.
    {
        unsigned state = 12345;
        int survived = 0;
        for (int attempt = 0; attempt < 400; ++attempt) {
            unsigned char noise[64];
            for (unsigned char& byte : noise) {
                state = state * 1664525u + 1013904223u;
                byte = static_cast<unsigned char>(state >> 16);
            }
            const auto result = raw(noise, sizeof(noise), 4096);
            if (result.data.size() <= 4096) ++survived;
        }
        check(survived == 400,
              "vierhundert zufaellige Eingaben, keine ueberschreitet die Grenze");
        std::printf("  400 zufaellige Eingaben ohne Absturz und ohne "
                    "Grenzueberschreitung\n");
    }
}

void testImages() {
    std::cout << "== Bilder ==\n";
    using namespace efx::image;

    // Die Referenzdateien sind von Pillow geschrieben, nicht von mir — sonst
    // pruefte der Test nur, ob mein Leser zu meinem Schreiber passt.
    //
    // Eingebettet statt aus /tmp gelesen: derselbe Fehler wie beim Auspacker,
    // den ich nicht zweimal machen wollte.
    // Von Pillow erzeugt: Typ 2, 24 Bit, Deskriptor 0x00
    const unsigned char kRgb24[] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 3, 0,
        24, 0, 128, 128, 128, 30, 20, 10, 50, 100, 200, 3, 2, 1, 0, 0,
        0, 0, 255, 255, 255, 255, 0, 255, 0, 255, 0, 0, 255, 0, 255, 0,
        255, 0, 0, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 84, 82,
        85, 69, 86, 73, 83, 73, 79, 78, 45, 88, 70, 73, 76, 69, 46, 0,
    };

    // Von Pillow erzeugt: Typ 2, 32 Bit, Deskriptor 0x08
    const unsigned char kRgba32[] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 3, 0,
        32, 8, 128, 128, 128, 255, 30, 20, 10, 255, 50, 100, 200, 0, 3, 2,
        1, 4, 0, 0, 0, 255, 0, 255, 255, 64, 255, 255, 0, 255, 255, 0,
        255, 255, 0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255, 255, 255,
        255, 128, 0, 0, 0, 0, 0, 0, 0, 0, 84, 82, 85, 69, 86, 73,
        83, 73, 79, 78, 45, 88, 70, 73, 76, 69, 46, 0,
    };

    // Von Pillow erzeugt: Typ 10, 32 Bit, Deskriptor 0x08
    const unsigned char kRle32[] = {
        0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 3, 0,
        32, 8, 3, 128, 128, 128, 255, 30, 20, 10, 255, 50, 100, 200, 0, 3,
        2, 1, 4, 3, 0, 0, 0, 255, 0, 255, 255, 64, 255, 255, 0, 255,
        255, 0, 255, 255, 3, 0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0,
        255, 255, 255, 255, 128, 0, 0, 0, 0, 0, 0, 0, 0, 84, 82, 85,
        69, 86, 73, 83, 73, 79, 78, 45, 88, 70, 73, 76, 69, 46, 0,
    };

    // Dieselben zwoelf Bildpunkte in allen drei Dateien.
    struct Pixel { unsigned char r, g, b, a; };
    const Pixel expected[12] = {
        {255, 0, 0, 255},   {0, 255, 0, 255},   {0, 0, 255, 255},
        {255, 255, 255, 128}, {0, 0, 0, 255},   {255, 255, 0, 64},
        {0, 255, 255, 255}, {255, 0, 255, 255}, {128, 128, 128, 255},
        {10, 20, 30, 255},  {200, 100, 50, 0},  {1, 2, 3, 4},
    };

    auto checkPixels = [&](const Image& image, const char* what, bool withAlpha) {
        check(image.ok, std::string(what) + " laesst sich lesen");
        if (!image.ok) return;
        check(image.width == 4 && image.height == 3,
              std::string(what) + ": vier mal drei");
        bool colours = true, alphas = true;
        for (int i = 0; i < 12; ++i) {
            const size_t at = static_cast<size_t>(i) * 4;
            if (image.rgba[at + 0] != expected[i].r ||
                image.rgba[at + 1] != expected[i].g ||
                image.rgba[at + 2] != expected[i].b) colours = false;
            if (withAlpha && image.rgba[at + 3] != expected[i].a) alphas = false;
        }
        check(colours, std::string(what) + ": Farben stimmen (BGR richtig gedreht)");
        if (withAlpha) {
            check(alphas, std::string(what) + ": Alphawerte stimmen");
        }
    };

    checkPixels(decodeTga(kRgba32, sizeof(kRgba32)), "TGA 32 Bit", true);
    checkPixels(decodeTga(kRgb24, sizeof(kRgb24)), "TGA 24 Bit", false);
    checkPixels(decodeTga(kRle32, sizeof(kRle32)), "TGA 32 Bit lauflaengenkodiert",
                true);
    std::printf("  drei Targa-Fassungen von Pillow, alle byteweise richtig\n");

    // Zeilenrichtung: Pillow schreibt von unten nach oben (Deskriptor ohne
    // Bit 5). Wer das uebersieht, bekommt jedes Bild auf dem Kopf — und merkt
    // es bei einem Funkenbild nie.
    check((kRgba32[17] & 0x20) == 0, "die Referenz liegt von unten nach oben");
    {
        // Dieselbe Datei mit gesetztem Bit 5 muss gespiegelt herauskommen.
        std::vector<unsigned char> flipped(kRgba32, kRgba32 + sizeof(kRgba32));
        flipped[17] |= 0x20;
        const Image normal = decodeTga(kRgba32, sizeof(kRgba32));
        const Image other = decodeTga(flipped.data(), flipped.size());
        check(other.ok, "auch von oben nach unten lesbar");
        // Erste Zeile der einen ist letzte Zeile der anderen.
        bool mirrored = true;
        for (int x = 0; x < 4 * 4; ++x) {
            if (normal.rgba[static_cast<size_t>(x)] !=
                other.rgba[static_cast<size_t>(2 * 4 * 4 + x)]) mirrored = false;
        }
        check(mirrored, "und dann steht das Bild auf dem Kopf, wie es soll");
    }

    // --- Formaterkennung am Inhalt ---------------------------------------
    //
    // In Mods liegen regelmaessig JPEG-Dateien mit der Endung .tga, weil
    // jemand umbenannt statt umgewandelt hat.
    check(sniff(kRgba32, sizeof(kRgba32)) == Format::Tga, "TGA erkannt");
    const unsigned char pngHeader[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    check(sniff(pngHeader, sizeof(pngHeader)) == Format::Png, "PNG erkannt");
    const unsigned char jpegHeader[] = {0xFF, 0xD8, 0xFF, 0xE0};
    check(sniff(jpegHeader, sizeof(jpegHeader)) == Format::Jpeg, "JPEG erkannt");
    check(sniff(nullptr, 0) == Format::Unknown, "nichts ist nichts");
    const unsigned char junk[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                  11, 12, 13, 14, 15, 16, 17, 18};
    check(sniff(junk, sizeof(junk)) == Format::Unknown, "Unsinn wird nicht erkannt");

    // decode() sagt bei PNG und JPEG, was fehlt, statt stumm zu scheitern.
    check(!decode(pngHeader, sizeof(pngHeader)).ok, "PNG noch nicht");
    check(decode(pngHeader, sizeof(pngHeader)).error.find("PNG") != std::string::npos,
          "und sagt es beim Namen");
    // JPEG geht jetzt — ein Kopf ohne Bilddaten wird trotzdem abgewiesen,
    // aber mit einer anderen Begruendung als "nicht unterstuetzt".
    check(!decode(jpegHeader, sizeof(jpegHeader)).ok,
          "ein JPEG-Kopf ohne Bilddaten wird abgewiesen");

    // --- JPEG -------------------------------------------------------------
    //
    // Vier Fassungen desselben Bildes von Pillow: die drei Unterabtastungen
    // (4:4:4, 4:2:2, 4:2:0) und eine Graustufenfassung. Das Bild hat einen
    // Farbverlauf UND harte Kanten — beides fordert den Decoder auf andere
    // Weise.
    //
    // Verglichen wird mit Spielraum: zwei JPEG-Decoder liefern nie Byte fuer
    // Byte dasselbe, weil die inverse DCT in Gleitkomma gerechnet wird und
    // jeder anders rundet. Ein Spielraum von 12 Stufen je Kanal trennt
    // "rundet anders" sauber von "dekodiert falsch" — bei einem echten Fehler
    // liegen die Werte um Hunderte daneben, nicht um zehn.
    {
        struct Sample { int x, y, r, g, b; };
    // 444: von Pillow geschrieben, 1017 Byte
    const unsigned char k444Jpeg[] = {
        255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 1, 0, 0, 1,
        0, 1, 0, 0, 255, 219, 0, 67, 0, 2, 1, 1, 1, 1, 1, 2,
        1, 1, 1, 2, 2, 2, 2, 2, 4, 3, 2, 2, 2, 2, 5, 4,
        4, 3, 4, 6, 5, 6, 6, 6, 5, 6, 6, 6, 7, 9, 8, 6,
        7, 9, 7, 6, 6, 8, 11, 8, 9, 10, 10, 10, 10, 10, 6, 8,
        11, 12, 11, 10, 12, 9, 10, 10, 10, 255, 219, 0, 67, 1, 2, 2,
        2, 2, 2, 2, 5, 3, 3, 5, 10, 7, 6, 7, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 255, 192,
        0, 17, 8, 0, 24, 0, 32, 3, 1, 17, 0, 2, 17, 1, 3, 17,
        1, 255, 196, 0, 31, 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 255, 196, 0, 181, 16, 0, 2, 1, 3, 3, 2, 4, 3, 5,
        5, 4, 4, 0, 0, 1, 125, 1, 2, 3, 0, 4, 17, 5, 18, 33,
        49, 65, 6, 19, 81, 97, 7, 34, 113, 20, 50, 129, 145, 161, 8, 35,
        66, 177, 193, 21, 82, 209, 240, 36, 51, 98, 114, 130, 9, 10, 22, 23,
        24, 25, 26, 37, 38, 39, 40, 41, 42, 52, 53, 54, 55, 56, 57, 58,
        67, 68, 69, 70, 71, 72, 73, 74, 83, 84, 85, 86, 87, 88, 89, 90,
        99, 100, 101, 102, 103, 104, 105, 106, 115, 116, 117, 118, 119, 120, 121, 122,
        131, 132, 133, 134, 135, 136, 137, 138, 146, 147, 148, 149, 150, 151, 152, 153,
        154, 162, 163, 164, 165, 166, 167, 168, 169, 170, 178, 179, 180, 181, 182, 183,
        184, 185, 186, 194, 195, 196, 197, 198, 199, 200, 201, 202, 210, 211, 212, 213,
        214, 215, 216, 217, 218, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 241,
        242, 243, 244, 245, 246, 247, 248, 249, 250, 255, 196, 0, 31, 1, 0, 3,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1,
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 255, 196, 0, 181, 17, 0,
        2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119, 0,
        1, 2, 3, 17, 4, 5, 33, 49, 6, 18, 65, 81, 7, 97, 113, 19,
        34, 50, 129, 8, 20, 66, 145, 161, 177, 193, 9, 35, 51, 82, 240, 21,
        98, 114, 209, 10, 22, 36, 52, 225, 37, 241, 23, 24, 25, 26, 38, 39,
        40, 41, 42, 53, 54, 55, 56, 57, 58, 67, 68, 69, 70, 71, 72, 73,
        74, 83, 84, 85, 86, 87, 88, 89, 90, 99, 100, 101, 102, 103, 104, 105,
        106, 115, 116, 117, 118, 119, 120, 121, 122, 130, 131, 132, 133, 134, 135, 136,
        137, 138, 146, 147, 148, 149, 150, 151, 152, 153, 154, 162, 163, 164, 165, 166,
        167, 168, 169, 170, 178, 179, 180, 181, 182, 183, 184, 185, 186, 194, 195, 196,
        197, 198, 199, 200, 201, 202, 210, 211, 212, 213, 214, 215, 216, 217, 218, 226,
        227, 228, 229, 230, 231, 232, 233, 234, 242, 243, 244, 245, 246, 247, 248, 249,
        250, 255, 218, 0, 12, 3, 1, 0, 2, 17, 3, 17, 0, 63, 0, 241,
        250, 254, 111, 63, 218, 195, 195, 62, 39, 252, 42, 62, 36, 248, 179, 170,
        107, 95, 102, 221, 231, 152, 57, 199, 164, 17, 175, 244, 175, 237, 95, 10,
        115, 143, 170, 248, 113, 130, 165, 125, 189, 175, 254, 158, 168, 207, 224, 95,
        22, 243, 239, 169, 120, 185, 152, 209, 190, 222, 199, 241, 161, 73, 254, 167,
        176, 254, 199, 191, 14, 63, 225, 16, 241, 189, 230, 167, 228, 109, 223, 163,
        73, 22, 113, 235, 44, 39, 255, 0, 101, 175, 154, 241, 111, 52, 250, 238,
        67, 74, 157, 246, 170, 159, 254, 73, 63, 243, 63, 145, 190, 153, 153, 207,
        246, 143, 131, 216, 42, 119, 189, 177, 180, 159, 254, 80, 196, 175, 212, 250,
        42, 191, 158, 207, 243, 8, 240, 122, 249, 51, 254, 188, 14, 143, 193, 223,
        10, 143, 137, 17, 117, 175, 179, 110, 243, 201, 231, 111, 161, 219, 253, 43,
        250, 7, 131, 115, 143, 170, 240, 150, 30, 149, 246, 231, 255, 0, 211, 146,
        103, 249, 55, 244, 136, 207, 190, 165, 227, 238, 111, 70, 251, 125, 95, 241,
        194, 208, 127, 169, 233, 26, 39, 195, 143, 248, 67, 237, 127, 180, 188, 141,
        187, 255, 0, 117, 156, 122, 243, 255, 0, 178, 215, 143, 197, 153, 167, 215,
        112, 145, 167, 125, 164, 159, 224, 255, 0, 204, 254, 87, 250, 72, 103, 63,
        218, 62, 26, 225, 105, 223, 108, 85, 55, 255, 0, 148, 107, 175, 212, 187,
        95, 4, 127, 13, 158, 15, 95, 38, 127, 215, 129, 245, 207, 236, 157, 240,
        171, 254, 18, 79, 132, 26, 54, 181, 246, 125, 222, 113, 184, 249, 177, 233,
        113, 42, 255, 0, 74, 250, 204, 14, 113, 245, 92, 178, 20, 175, 181, 255,
        0, 22, 217, 254, 24, 125, 45, 179, 239, 169, 125, 39, 179, 234, 55, 219,
        234, 191, 142, 11, 12, 255, 0, 83, 175, 253, 163, 62, 28, 127, 194, 33,
        240, 206, 207, 83, 242, 54, 121, 154, 204, 113, 103, 30, 176, 204, 127, 246,
        90, 230, 134, 105, 245, 218, 238, 157, 246, 87, 254, 190, 243, 249, 167, 197,
        92, 231, 251, 71, 131, 104, 211, 189, 237, 90, 47, 255, 0, 41, 212, 95,
        169, 225, 245, 210, 127, 58, 31, 255, 217,
    };
    const Sample k444Points[] = {{3, 2, 254, 40, 40}, {16, 2, 125, 22, 129}, {28, 2, 20, 200, 90}, {3, 12, 254, 40, 40}, {16, 12, 127, 129, 128}, {28, 12, 20, 200, 90}, {3, 21, 254, 40, 40}, {16, 21, 128, 222, 126}, {28, 21, 20, 200, 90}};

    // 422: von Pillow geschrieben, 897 Byte
    const unsigned char k422Jpeg[] = {
        255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 1, 0, 0, 1,
        0, 1, 0, 0, 255, 219, 0, 67, 0, 3, 2, 2, 3, 2, 2, 3,
        3, 3, 3, 4, 3, 3, 4, 5, 8, 5, 5, 4, 4, 5, 10, 7,
        7, 6, 8, 12, 10, 12, 12, 11, 10, 11, 11, 13, 14, 18, 16, 13,
        14, 17, 14, 11, 11, 16, 22, 16, 17, 19, 20, 21, 21, 21, 12, 15,
        23, 24, 22, 20, 24, 18, 20, 21, 20, 255, 219, 0, 67, 1, 3, 4,
        4, 5, 4, 5, 9, 5, 5, 9, 20, 13, 11, 13, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 255, 192,
        0, 17, 8, 0, 24, 0, 32, 3, 1, 33, 0, 2, 17, 1, 3, 17,
        1, 255, 196, 0, 31, 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 255, 196, 0, 181, 16, 0, 2, 1, 3, 3, 2, 4, 3, 5,
        5, 4, 4, 0, 0, 1, 125, 1, 2, 3, 0, 4, 17, 5, 18, 33,
        49, 65, 6, 19, 81, 97, 7, 34, 113, 20, 50, 129, 145, 161, 8, 35,
        66, 177, 193, 21, 82, 209, 240, 36, 51, 98, 114, 130, 9, 10, 22, 23,
        24, 25, 26, 37, 38, 39, 40, 41, 42, 52, 53, 54, 55, 56, 57, 58,
        67, 68, 69, 70, 71, 72, 73, 74, 83, 84, 85, 86, 87, 88, 89, 90,
        99, 100, 101, 102, 103, 104, 105, 106, 115, 116, 117, 118, 119, 120, 121, 122,
        131, 132, 133, 134, 135, 136, 137, 138, 146, 147, 148, 149, 150, 151, 152, 153,
        154, 162, 163, 164, 165, 166, 167, 168, 169, 170, 178, 179, 180, 181, 182, 183,
        184, 185, 186, 194, 195, 196, 197, 198, 199, 200, 201, 202, 210, 211, 212, 213,
        214, 215, 216, 217, 218, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 241,
        242, 243, 244, 245, 246, 247, 248, 249, 250, 255, 196, 0, 31, 1, 0, 3,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1,
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 255, 196, 0, 181, 17, 0,
        2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119, 0,
        1, 2, 3, 17, 4, 5, 33, 49, 6, 18, 65, 81, 7, 97, 113, 19,
        34, 50, 129, 8, 20, 66, 145, 161, 177, 193, 9, 35, 51, 82, 240, 21,
        98, 114, 209, 10, 22, 36, 52, 225, 37, 241, 23, 24, 25, 26, 38, 39,
        40, 41, 42, 53, 54, 55, 56, 57, 58, 67, 68, 69, 70, 71, 72, 73,
        74, 83, 84, 85, 86, 87, 88, 89, 90, 99, 100, 101, 102, 103, 104, 105,
        106, 115, 116, 117, 118, 119, 120, 121, 122, 130, 131, 132, 133, 134, 135, 136,
        137, 138, 146, 147, 148, 149, 150, 151, 152, 153, 154, 162, 163, 164, 165, 166,
        167, 168, 169, 170, 178, 179, 180, 181, 182, 183, 184, 185, 186, 194, 195, 196,
        197, 198, 199, 200, 201, 202, 210, 211, 212, 213, 214, 215, 216, 217, 218, 226,
        227, 228, 229, 230, 231, 232, 233, 234, 242, 243, 244, 245, 246, 247, 248, 249,
        250, 255, 218, 0, 12, 3, 1, 0, 2, 17, 3, 17, 0, 63, 0, 243,
        250, 243, 47, 20, 120, 83, 251, 75, 197, 183, 183, 59, 51, 191, 103, 56,
        244, 69, 31, 210, 188, 110, 4, 173, 236, 51, 42, 178, 255, 0, 167, 111,
        255, 0, 74, 129, 253, 149, 226, 46, 35, 234, 249, 101, 9, 255, 0, 211,
        216, 175, 252, 146, 103, 160, 252, 30, 240, 231, 246, 62, 185, 113, 54, 221,
        187, 173, 89, 51, 255, 0, 3, 67, 253, 43, 215, 42, 184, 158, 175, 182,
        204, 101, 63, 36, 127, 152, 254, 46, 214, 246, 252, 76, 231, 255, 0, 78,
        225, 249, 51, 203, 235, 95, 71, 240, 167, 246, 144, 91, 157, 153, 223, 223,
        30, 156, 127, 74, 240, 120, 110, 183, 176, 197, 78, 95, 221, 127, 156, 79,
        244, 87, 198, 44, 71, 213, 178, 60, 52, 255, 0, 233, 252, 87, 254, 83,
        168, 118, 22, 62, 28, 254, 199, 139, 206, 219, 183, 119, 201, 211, 241, 254,
        149, 102, 183, 204, 170, 251, 108, 67, 145, 254, 102, 120, 135, 91, 235, 25,
        219, 159, 247, 35, 249, 30, 95, 94, 247, 240, 159, 194, 159, 218, 94, 15,
        211, 174, 118, 103, 127, 153, 206, 61, 36, 97, 253, 43, 231, 48, 117, 189,
        132, 229, 47, 47, 213, 31, 232, 127, 210, 3, 17, 245, 126, 26, 194, 79,
        254, 162, 96, 191, 242, 157, 99, 160, 248, 141, 225, 207, 236, 127, 12, 219,
        205, 183, 110, 235, 165, 79, 252, 113, 207, 244, 175, 52, 174, 184, 85, 246,
        203, 152, 255, 0, 52, 184, 162, 183, 183, 204, 57, 255, 0, 187, 19, 255,
        217,
    };
    const Sample k422Points[] = {{3, 2, 255, 36, 42}, {16, 2, 128, 20, 132}, {28, 2, 22, 200, 88}, {3, 12, 254, 40, 38}, {16, 12, 127, 127, 129}, {28, 12, 23, 199, 90}, {3, 21, 248, 42, 42}, {16, 21, 126, 224, 121}, {28, 21, 18, 202, 90}};

    // 420: von Pillow geschrieben, 819 Byte
    const unsigned char k420Jpeg[] = {
        255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 1, 0, 0, 1,
        0, 1, 0, 0, 255, 219, 0, 67, 0, 5, 3, 4, 4, 4, 3, 5,
        4, 4, 4, 5, 5, 5, 6, 7, 12, 8, 7, 7, 7, 7, 15, 11,
        11, 9, 12, 17, 15, 18, 18, 17, 15, 17, 17, 19, 22, 28, 23, 19,
        20, 26, 21, 17, 17, 24, 33, 24, 26, 29, 29, 31, 31, 31, 19, 23,
        34, 36, 34, 30, 36, 28, 30, 31, 30, 255, 219, 0, 67, 1, 5, 5,
        5, 7, 6, 7, 14, 8, 8, 14, 30, 20, 17, 20, 30, 30, 30, 30,
        30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
        30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
        30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 255, 192,
        0, 17, 8, 0, 24, 0, 32, 3, 1, 34, 0, 2, 17, 1, 3, 17,
        1, 255, 196, 0, 31, 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 255, 196, 0, 181, 16, 0, 2, 1, 3, 3, 2, 4, 3, 5,
        5, 4, 4, 0, 0, 1, 125, 1, 2, 3, 0, 4, 17, 5, 18, 33,
        49, 65, 6, 19, 81, 97, 7, 34, 113, 20, 50, 129, 145, 161, 8, 35,
        66, 177, 193, 21, 82, 209, 240, 36, 51, 98, 114, 130, 9, 10, 22, 23,
        24, 25, 26, 37, 38, 39, 40, 41, 42, 52, 53, 54, 55, 56, 57, 58,
        67, 68, 69, 70, 71, 72, 73, 74, 83, 84, 85, 86, 87, 88, 89, 90,
        99, 100, 101, 102, 103, 104, 105, 106, 115, 116, 117, 118, 119, 120, 121, 122,
        131, 132, 133, 134, 135, 136, 137, 138, 146, 147, 148, 149, 150, 151, 152, 153,
        154, 162, 163, 164, 165, 166, 167, 168, 169, 170, 178, 179, 180, 181, 182, 183,
        184, 185, 186, 194, 195, 196, 197, 198, 199, 200, 201, 202, 210, 211, 212, 213,
        214, 215, 216, 217, 218, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 241,
        242, 243, 244, 245, 246, 247, 248, 249, 250, 255, 196, 0, 31, 1, 0, 3,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1,
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 255, 196, 0, 181, 17, 0,
        2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119, 0,
        1, 2, 3, 17, 4, 5, 33, 49, 6, 18, 65, 81, 7, 97, 113, 19,
        34, 50, 129, 8, 20, 66, 145, 161, 177, 193, 9, 35, 51, 82, 240, 21,
        98, 114, 209, 10, 22, 36, 52, 225, 37, 241, 23, 24, 25, 26, 38, 39,
        40, 41, 42, 53, 54, 55, 56, 57, 58, 67, 68, 69, 70, 71, 72, 73,
        74, 83, 84, 85, 86, 87, 88, 89, 90, 99, 100, 101, 102, 103, 104, 105,
        106, 115, 116, 117, 118, 119, 120, 121, 122, 130, 131, 132, 133, 134, 135, 136,
        137, 138, 146, 147, 148, 149, 150, 151, 152, 153, 154, 162, 163, 164, 165, 166,
        167, 168, 169, 170, 178, 179, 180, 181, 182, 183, 184, 185, 186, 194, 195, 196,
        197, 198, 199, 200, 201, 202, 210, 211, 212, 213, 214, 215, 216, 217, 218, 226,
        227, 228, 229, 230, 231, 232, 233, 234, 242, 243, 244, 245, 246, 247, 248, 249,
        250, 255, 218, 0, 12, 3, 1, 0, 2, 17, 3, 17, 0, 63, 0, 230,
        107, 145, 214, 180, 175, 180, 235, 215, 19, 237, 206, 237, 191, 250, 8, 21,
        215, 85, 237, 63, 74, 251, 72, 19, 237, 206, 239, 233, 197, 121, 28, 51,
        138, 250, 182, 42, 83, 254, 235, 95, 138, 63, 111, 241, 15, 25, 245, 76,
        186, 148, 255, 0, 233, 226, 95, 249, 44, 140, 175, 135, 218, 111, 216, 245,
        41, 101, 219, 140, 192, 87, 255, 0, 30, 95, 240, 174, 222, 161, 182, 211,
        126, 198, 158, 110, 220, 103, 229, 255, 0, 63, 149, 77, 85, 155, 226, 62,
        177, 137, 115, 242, 71, 242, 47, 136, 24, 175, 173, 102, 238, 167, 247, 98,
        113, 245, 233, 62, 4, 210, 190, 211, 225, 251, 89, 246, 231, 118, 255, 0,
        209, 216, 81, 69, 120, 20, 106, 74, 155, 110, 39, 244, 183, 141, 117, 101,
        79, 35, 160, 227, 255, 0, 63, 163, 255, 0, 164, 84, 52, 252, 95, 166,
        253, 143, 70, 138, 93, 184, 204, 225, 127, 241, 214, 255, 0, 10, 228, 232,
        162, 183, 165, 55, 56, 221, 159, 201, 121, 229, 73, 84, 197, 115, 75, 178,
        63, 255, 217,
    };
    const Sample k420Points[] = {{3, 2, 255, 36, 46}, {16, 2, 131, 19, 127}, {28, 2, 16, 203, 88}, {3, 12, 248, 45, 31}, {16, 12, 124, 130, 120}, {28, 12, 18, 202, 91}, {3, 21, 254, 40, 38}, {16, 21, 134, 220, 129}, {28, 21, 19, 201, 90}};

    // grey: von Pillow geschrieben, 459 Byte
    const unsigned char kGreyJpeg[] = {
        255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 1, 0, 0, 1,
        0, 1, 0, 0, 255, 219, 0, 67, 0, 3, 2, 2, 2, 2, 2, 3,
        2, 2, 2, 3, 3, 3, 3, 4, 6, 4, 4, 4, 4, 4, 8, 6,
        6, 5, 6, 9, 8, 10, 10, 9, 8, 9, 9, 10, 12, 15, 12, 10,
        11, 14, 11, 9, 9, 13, 17, 13, 14, 15, 16, 16, 17, 16, 10, 12,
        18, 19, 18, 16, 19, 15, 16, 16, 16, 255, 192, 0, 11, 8, 0, 24,
        0, 32, 1, 1, 17, 0, 255, 196, 0, 31, 0, 0, 1, 5, 1, 1,
        1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4,
        5, 6, 7, 8, 9, 10, 11, 255, 196, 0, 181, 16, 0, 2, 1, 3,
        3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125, 1, 2, 3, 0,
        4, 17, 5, 18, 33, 49, 65, 6, 19, 81, 97, 7, 34, 113, 20, 50,
        129, 145, 161, 8, 35, 66, 177, 193, 21, 82, 209, 240, 36, 51, 98, 114,
        130, 9, 10, 22, 23, 24, 25, 26, 37, 38, 39, 40, 41, 42, 52, 53,
        54, 55, 56, 57, 58, 67, 68, 69, 70, 71, 72, 73, 74, 83, 84, 85,
        86, 87, 88, 89, 90, 99, 100, 101, 102, 103, 104, 105, 106, 115, 116, 117,
        118, 119, 120, 121, 122, 131, 132, 133, 134, 135, 136, 137, 138, 146, 147, 148,
        149, 150, 151, 152, 153, 154, 162, 163, 164, 165, 166, 167, 168, 169, 170, 178,
        179, 180, 181, 182, 183, 184, 185, 186, 194, 195, 196, 197, 198, 199, 200, 201,
        202, 210, 211, 212, 213, 214, 215, 216, 217, 218, 225, 226, 227, 228, 229, 230,
        231, 232, 233, 234, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 255, 218,
        0, 8, 1, 1, 0, 0, 63, 0, 243, 250, 243, 47, 20, 120, 83, 251,
        75, 197, 183, 183, 190, 94, 124, 195, 23, 56, 244, 141, 71, 244, 175, 65,
        248, 61, 225, 207, 236, 125, 114, 226, 231, 203, 219, 186, 201, 227, 206, 61,
        93, 15, 244, 175, 92, 175, 47, 173, 125, 31, 194, 159, 218, 65, 111, 124,
        188, 249, 135, 174, 61, 56, 254, 149, 216, 88, 248, 115, 251, 30, 47, 180,
        249, 123, 119, 126, 239, 167, 175, 63, 210, 172, 215, 151, 215, 189, 252, 39,
        240, 167, 246, 151, 131, 244, 235, 223, 47, 62, 97, 151, 156, 122, 74, 227,
        250, 87, 65, 241, 27, 195, 159, 216, 254, 25, 183, 185, 242, 246, 238, 188,
        72, 250, 127, 176, 231, 250, 87, 154, 87, 255, 217,
    };
    const Sample kGreyPoints[] = {{3, 2, 104, 104, 104}, {16, 2, 65, 65, 65}, {28, 2, 134, 134, 134}, {3, 12, 104, 104, 104}, {16, 12, 127, 127, 127}, {28, 12, 134, 134, 134}, {3, 21, 104, 104, 104}, {16, 21, 183, 183, 183}, {28, 21, 134, 134, 134}};

        struct JpegCase {
            const char* name;
            const unsigned char* data;
            size_t size;
            const Sample* points;
            size_t pointCount;
            bool colour;
        };
        const JpegCase jpegCases[] = {
            {"4:4:4", k444Jpeg, sizeof(k444Jpeg), k444Points,
             sizeof(k444Points) / sizeof(Sample), true},
            {"4:2:2", k422Jpeg, sizeof(k422Jpeg), k422Points,
             sizeof(k422Points) / sizeof(Sample), true},
            {"4:2:0", k420Jpeg, sizeof(k420Jpeg), k420Points,
             sizeof(k420Points) / sizeof(Sample), true},
            {"Graustufen", kGreyJpeg, sizeof(kGreyJpeg), kGreyPoints,
             sizeof(kGreyPoints) / sizeof(Sample), false},
        };

        for (const auto& entry : jpegCases) {
            const Image picture = decodeJpeg(entry.data, entry.size);
            const std::string name = entry.name;
            check(picture.ok, name + " laesst sich lesen");
            if (!picture.ok) {
                std::printf("    %s: %s\n", entry.name, picture.error.c_str());
                continue;
            }
            check(picture.width == 32 && picture.height == 24,
                  name + ": 32 x 24");
            check(!picture.hasAlpha, name + ": JPEG kennt keinen Alphakanal");

            int worst = 0;
            for (size_t i = 0; i < entry.pointCount; ++i) {
                const Sample& s = entry.points[i];
                const size_t at =
                    (static_cast<size_t>(s.y) * 32 + static_cast<size_t>(s.x)) * 4;
                worst = std::max(worst, std::abs(picture.rgba[at + 0] - s.r));
                worst = std::max(worst, std::abs(picture.rgba[at + 1] - s.g));
                worst = std::max(worst, std::abs(picture.rgba[at + 2] - s.b));
                check(picture.rgba[at + 3] == 255, name + ": voll deckend");
            }
            check(worst <= 12, name + ": stimmt mit Pillow ueberein");
            std::printf("  %-11s groesste Abweichung von Pillow: %d Stufen\n",
                        entry.name, worst);
            (void)entry.colour;
        }

        // decode() erkennt es am Inhalt und leitet weiter.
        check(decode(k444Jpeg, sizeof(k444Jpeg)).ok,
              "decode() erkennt JPEG und dekodiert es");

        // Progressive JPEGs werden abgewiesen und beim Namen genannt, statt
        // Muell zu liefern.
        {
            std::vector<unsigned char> progressive(k444Jpeg,
                                                   k444Jpeg + sizeof(k444Jpeg));
            for (size_t i = 0; i + 1 < progressive.size(); ++i) {
                if (progressive[i] == 0xFF && progressive[i + 1] == 0xC0) {
                    progressive[i + 1] = 0xC2;  // SOF0 -> SOF2
                    break;
                }
            }
            const Image result = decodeJpeg(progressive.data(), progressive.size());
            check(!result.ok, "ein progressives JPEG wird abgewiesen");
            check(result.error.find("progressive") != std::string::npos,
                  "und beim Namen genannt");
        }

        // Kaputte Dateien.
        check(!decodeJpeg(nullptr, 0).ok, "Nullzeiger");
        check(!decodeJpeg(k444Jpeg, 3).ok, "zu kurz");
        {
            const unsigned char notJpeg[] = {1, 2, 3, 4, 5, 6, 7, 8};
            const Image result = decodeJpeg(notJpeg, sizeof(notJpeg));
            check(!result.ok, "ohne Startkennung abgewiesen");
        }
        {
            // Mitten in den Bilddaten abgeschnitten: der Decoder soll
            // aufhoeren, nicht abstuerzen — und liefert, was er hat.
            for (size_t cut : {sizeof(k444Jpeg) / 2, sizeof(k444Jpeg) - 20,
                               sizeof(k444Jpeg) / 4}) {
                const Image result = decodeJpeg(k444Jpeg, cut);
                check(!result.ok || result.rgba.size() == 32u * 24 * 4,
                      "abgeschnitten: entweder abgewiesen oder vollstaendig");
            }
        }
        {
            // Zufaellig verbogene Bytes im Datenteil. Keine darf abstuerzen
            // oder ueber die Bildgroesse hinausschreiben.
            unsigned state = 4242;
            int survived = 0;
            for (int attempt = 0; attempt < 150; ++attempt) {
                std::vector<unsigned char> broken(k444Jpeg,
                                                  k444Jpeg + sizeof(k444Jpeg));
                for (int k = 0; k < 8; ++k) {
                    state = state * 1664525u + 1013904223u;
                    const size_t at = 200 + (state >> 8) % (broken.size() - 200);
                    broken[at] = static_cast<unsigned char>(state >> 20);
                }
                const Image result = decodeJpeg(broken.data(), broken.size());
                if (!result.ok || result.rgba.size() <= 32u * 24 * 4) ++survived;
            }
            check(survived == 150,
                  "hundertfuenfzig verbogene JPEGs, keines bricht aus");
        }
    }

    // --- PNG --------------------------------------------------------------
    //
    // Vier Farbarten von Pillow: RGBA, RGB, Graustufen und Farbtabelle. PNG
    // ist verlustfrei, also muss es hier BYTEWEISE stimmen — anders als bei
    // JPEG gibt es keinen Spielraum zu vergeben.
    {
        struct PngSample { int x, y, r, g, b, a; };
    // rgba.png: Farbart 6, 151 Byte
    const unsigned char kRgbaPng[] = {
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
        0, 0, 0, 16, 0, 0, 0, 12, 8, 6, 0, 0, 0, 107, 231, 61,
        129, 0, 0, 0, 94, 73, 68, 65, 84, 120, 156, 99, 100, 96, 96, 248,
        47, 192, 192, 192, 128, 7, 71, 227, 147, 103, 97, 16, 97, 96, 96, 96,
        96, 38, 27, 35, 25, 192, 70, 22, 70, 51, 128, 147, 100, 140, 197, 0,
        30, 146, 48, 14, 3, 248, 137, 198, 120, 12, 16, 34, 10, 19, 48, 64,
        148, 0, 214, 37, 198, 0, 9, 44, 88, 29, 206, 38, 210, 0, 105, 40,
        86, 68, 98, 67, 48, 9, 6, 200, 97, 197, 68, 26, 160, 8, 213, 160,
        136, 129, 1, 192, 77, 12, 141, 32, 206, 115, 84, 0, 0, 0, 0, 73,
        69, 78, 68, 174, 66, 96, 130,
    };

    const PngSample kRgbaPngPoints[] = {{2, 1, 32, 20, 6, 255}, {8, 1, 128, 20, 24, 90}, {14, 1, 224, 20, 42, 90}, {2, 6, 32, 120, 36, 255}, {8, 6, 128, 120, 144, 90}, {14, 6, 224, 120, 252, 90}, {2, 10, 32, 200, 60, 255}, {8, 10, 128, 200, 240, 90}, {14, 10, 224, 200, 164, 90}};

    // rgb.png: Farbart 2, 144 Byte
    const unsigned char kRgbPng[] = {
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
        0, 0, 0, 16, 0, 0, 0, 12, 8, 2, 0, 0, 0, 228, 133, 170,
        214, 0, 0, 0, 87, 73, 68, 65, 84, 120, 156, 149, 203, 65, 7, 128,
        64, 16, 64, 225, 151, 18, 17, 37, 69, 12, 203, 156, 58, 245, 255, 255,
        94, 135, 148, 109, 107, 103, 39, 190, 195, 187, 188, 10, 24, 255, 104, 152,
        129, 218, 239, 30, 90, 167, 120, 232, 60, 146, 161, 47, 122, 15, 131, 237,
        115, 152, 12, 185, 97, 201, 216, 141, 97, 125, 218, 206, 176, 7, 1, 1,
        189, 66, 64, 138, 67, 72, 216, 131, 66, 0, 141, 29, 77, 118, 11, 51,
        95, 233, 87, 220, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130,
    };

    const PngSample kRgbPngPoints[] = {{2, 1, 32, 20, 6, 255}, {8, 1, 128, 20, 24, 255}, {14, 1, 224, 20, 42, 255}, {2, 6, 32, 120, 36, 255}, {8, 6, 128, 120, 144, 255}, {14, 6, 224, 120, 252, 255}, {2, 10, 32, 200, 60, 255}, {8, 10, 128, 200, 240, 255}, {14, 10, 224, 200, 164, 255}};

    // grey.png: Farbart 0, 157 Byte
    const unsigned char kGreyPng[] = {
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
        0, 0, 0, 16, 0, 0, 0, 12, 8, 0, 0, 0, 0, 78, 140, 98,
        93, 0, 0, 0, 100, 73, 68, 65, 84, 120, 156, 85, 136, 65, 10, 3,
        49, 12, 196, 186, 224, 241, 200, 196, 176, 219, 255, 255, 48, 249, 65, 79,
        61, 132, 64, 139, 24, 9, 230, 122, 73, 161, 61, 133, 164, 171, 37, 73,
        202, 147, 24, 169, 95, 20, 157, 153, 202, 60, 222, 135, 51, 243, 36, 58,
        253, 71, 12, 219, 118, 30, 207, 104, 99, 219, 219, 243, 243, 142, 54, 6,
        99, 60, 121, 32, 26, 3, 128, 215, 3, 56, 70, 1, 20, 172, 155, 2,
        162, 139, 162, 168, 117, 83, 69, 241, 5, 52, 125, 11, 199, 175, 23, 234,
        222, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130,
    };

    const PngSample kGreyPngPoints[] = {{2, 1, 22, 22, 22, 255}, {8, 1, 53, 53, 53, 255}, {14, 1, 84, 84, 84, 255}, {2, 6, 84, 84, 84, 255}, {8, 6, 125, 125, 125, 255}, {14, 6, 166, 166, 166, 255}, {2, 10, 134, 134, 134, 255}, {8, 10, 183, 183, 183, 255}, {14, 10, 203, 203, 203, 255}};

    // pal.png: Farbart 3, 1317 Byte
    const unsigned char kPalPng[] = {
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
        0, 0, 0, 16, 0, 0, 0, 12, 8, 3, 0, 0, 0, 92, 57, 205,
        179, 0, 0, 3, 0, 80, 76, 84, 69, 8, 0, 0, 8, 20, 1, 8,
        40, 3, 8, 60, 4, 8, 80, 6, 8, 100, 7, 8, 120, 9, 8, 140,
        10, 8, 160, 12, 8, 180, 13, 8, 200, 15, 40, 0, 0, 40, 20, 7,
        40, 40, 15, 40, 60, 22, 40, 120, 45, 40, 140, 52, 72, 0, 0, 72,
        20, 13, 72, 40, 27, 72, 60, 40, 72, 80, 54, 72, 120, 81, 72, 160,
        108, 104, 0, 0, 104, 20, 19, 104, 40, 39, 104, 60, 58, 104, 80, 78,
        104, 120, 117, 104, 180, 175, 136, 0, 0, 136, 20, 25, 136, 40, 51, 136,
        60, 76, 136, 80, 102, 136, 140, 178, 136, 160, 204, 168, 0, 0, 200, 0,
        0, 200, 20, 37, 200, 40, 75, 200, 60, 112, 200, 80, 150, 200, 160, 44,
        200, 180, 81, 232, 0, 0, 232, 20, 43, 232, 40, 87, 232, 80, 174, 232,
        140, 48, 0, 220, 0, 16, 220, 33, 32, 80, 24, 48, 80, 36, 32, 100,
        30, 48, 100, 45, 32, 160, 48, 48, 160, 72, 32, 180, 54, 48, 180, 81,
        32, 200, 60, 48, 200, 90, 32, 220, 66, 48, 220, 99, 64, 100, 60, 80,
        100, 75, 64, 140, 84, 80, 140, 105, 64, 180, 108, 80, 180, 135, 64, 200,
        120, 80, 200, 150, 64, 220, 132, 80, 220, 165, 96, 100, 90, 112, 100, 105,
        96, 140, 126, 112, 140, 147, 96, 160, 144, 112, 160, 168, 96, 200, 180, 112,
        200, 210, 96, 220, 198, 112, 220, 231, 128, 100, 120, 144, 100, 135, 128, 120,
        144, 144, 120, 162, 128, 180, 216, 144, 180, 243, 144, 200, 14, 128, 200, 240,
        128, 220, 8, 144, 220, 41, 160, 20, 30, 176, 20, 33, 160, 40, 60, 176,
        40, 66, 160, 60, 90, 176, 60, 99, 160, 80, 120, 176, 80, 132, 160, 100,
        150, 176, 100, 165, 160, 120, 180, 176, 120, 198, 160, 140, 210, 176, 140, 231,
        176, 160, 8, 160, 160, 240, 160, 180, 14, 176, 180, 41, 160, 200, 44, 176,
        200, 74, 160, 220, 74, 176, 220, 107, 192, 100, 180, 208, 100, 195, 192, 120,
        216, 208, 120, 234, 208, 140, 17, 192, 140, 252, 192, 200, 104, 208, 200, 134,
        192, 220, 140, 208, 220, 173, 224, 60, 126, 240, 60, 135, 224, 100, 210, 240,
        100, 225, 240, 120, 14, 224, 120, 252, 224, 160, 80, 240, 160, 104, 224, 180,
        122, 240, 180, 149, 224, 200, 164, 240, 200, 194, 224, 220, 206, 240, 220, 239,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 159, 219, 163, 70, 0, 0, 1,
        0, 116, 82, 78, 83, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90,
        90, 90, 90, 90, 90, 90, 90, 90, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 90, 90, 90, 90, 90, 90,
        90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90,
        90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90,
        90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90,
        90, 90, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 212, 234, 112, 214, 0, 0, 0, 212, 73, 68, 65,
        84, 120, 156, 99, 96, 96, 224, 230, 22, 20, 148, 144, 144, 151, 87, 83,
        83, 87, 215, 211, 99, 96, 100, 228, 225, 17, 18, 146, 148, 84, 80, 136,
        79, 208, 208, 208, 215, 103, 96, 98, 226, 229, 21, 22, 150, 146, 82, 84,
        76, 76, 210, 212, 52, 48, 96, 96, 102, 230, 227, 19, 17, 145, 150, 86,
        82, 74, 78, 209, 210, 170, 111, 96, 96, 97, 49, 53, 19, 21, 149, 145,
        81, 86, 78, 77, 211, 214, 54, 52, 100, 96, 101, 53, 183, 112, 116, 242,
        246, 9, 13, 75, 207, 40, 45, 107, 108, 98, 96, 99, 227, 231, 23, 19,
        147, 149, 13, 143, 200, 204, 42, 175, 104, 105, 102, 96, 103, 23, 16, 112,
        118, 241, 245, 83, 81, 201, 206, 169, 170, 52, 50, 98, 224, 224, 176, 180,
        18, 23, 247, 15, 80, 85, 205, 203, 213, 209, 105, 109, 99, 224, 228, 180,
        182, 113, 117, 147, 147, 139, 140, 202, 47, 208, 213, 109, 239, 96, 224, 226,
        178, 181, 115, 247, 8, 12, 138, 137, 46, 44, 170, 174, 233, 236, 98, 48,
        54, 177, 119, 240, 244, 10, 14, 137, 141, 43, 46, 169, 173, 235, 238, 1,
        0, 185, 254, 43, 138, 112, 249, 121, 32, 0, 0, 0, 0, 73, 69, 78,
        68, 174, 66, 96, 130,
    };

    const PngSample kPalPngPoints[] = {{2, 1, 40, 20, 7, 255}, {8, 1, 136, 20, 25, 90}, {14, 1, 232, 20, 43, 90}, {2, 6, 40, 120, 45, 255}, {8, 6, 128, 120, 144, 90}, {14, 6, 224, 120, 252, 90}, {2, 10, 32, 200, 60, 255}, {8, 10, 128, 200, 240, 90}, {14, 10, 224, 200, 164, 90}};

        struct PngCase {
            const char* name;
            const unsigned char* data;
            size_t size;
            const PngSample* points;
            size_t count;
        };
        const PngCase pngCases[] = {
            {"RGBA", kRgbaPng, sizeof(kRgbaPng), kRgbaPngPoints,
             sizeof(kRgbaPngPoints) / sizeof(PngSample)},
            {"RGB", kRgbPng, sizeof(kRgbPng), kRgbPngPoints,
             sizeof(kRgbPngPoints) / sizeof(PngSample)},
            {"Graustufen", kGreyPng, sizeof(kGreyPng), kGreyPngPoints,
             sizeof(kGreyPngPoints) / sizeof(PngSample)},
            {"Farbtabelle", kPalPng, sizeof(kPalPng), kPalPngPoints,
             sizeof(kPalPngPoints) / sizeof(PngSample)},
        };

        for (const auto& entry : pngCases) {
            const Image picture = decodePng(entry.data, entry.size);
            const std::string name = entry.name;
            check(picture.ok, name + ": laesst sich lesen");
            if (!picture.ok) {
                std::printf("    %s: %s\n", entry.name, picture.error.c_str());
                continue;
            }
            check(picture.width == 16 && picture.height == 12, name + ": 16 x 12");

            int worst = 0;
            for (size_t i = 0; i < entry.count; ++i) {
                const PngSample& s = entry.points[i];
                const size_t at =
                    (static_cast<size_t>(s.y) * 16 + static_cast<size_t>(s.x)) * 4;
                worst = std::max(worst, std::abs(picture.rgba[at + 0] - s.r));
                worst = std::max(worst, std::abs(picture.rgba[at + 1] - s.g));
                worst = std::max(worst, std::abs(picture.rgba[at + 2] - s.b));
                worst = std::max(worst, std::abs(picture.rgba[at + 3] - s.a));
            }
            check(worst == 0, name + ": byteweise gleich wie Pillow");
        }
        std::printf("  vier PNG-Farbarten, alle byteweise gleich\n");

        check(decode(kRgbaPng, sizeof(kRgbaPng)).ok, "decode() leitet PNG weiter");

        // Verschachtelte PNGs werden abgewiesen statt verzerrt dargestellt.
        {
            std::vector<unsigned char> interlaced(kRgbaPng,
                                                  kRgbaPng + sizeof(kRgbaPng));
            interlaced[28] = 1;  // Interlace-Byte im IHDR
            const Image result = decodePng(interlaced.data(), interlaced.size());
            check(!result.ok, "verschachteltes PNG wird abgewiesen");
            check(result.error.find("interlaced") != std::string::npos,
                  "und beim Namen genannt");
        }

        check(!decodePng(nullptr, 0).ok, "Nullzeiger");
        check(!decodePng(kRgbaPng, 6).ok, "zu kurz");
        {
            const unsigned char notPng[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
            check(!decodePng(notPng, sizeof(notPng)).ok, "ohne Kennung abgewiesen");
        }
        {
            unsigned state = 5150;
            int survived = 0;
            for (int attempt = 0; attempt < 150; ++attempt) {
                std::vector<unsigned char> broken(kRgbaPng,
                                                  kRgbaPng + sizeof(kRgbaPng));
                for (int k = 0; k < 5; ++k) {
                    state = state * 1664525u + 1013904223u;
                    broken[8 + (state >> 8) % (broken.size() - 8)] =
                        static_cast<unsigned char>(state >> 20);
                }
                const Image result = decodePng(broken.data(), broken.size());
                if (!result.ok || result.rgba.size() <= 16u * 12 * 4) ++survived;
            }
            check(survived == 150, "hundertfuenfzig verbogene PNGs, keines bricht aus");
        }
    }

    // --- Targa schreiben ---------------------------------------------------
    //
    // Fuer Bildschirmfotos. Geprueft ueber den Rundlauf: schreiben, mit dem
    // eigenen Leser zurueck — und zusaetzlich mit Pillow, damit nicht nur
    // mein Schreiber zu meinem Leser passt.
    {
        std::vector<unsigned char> source(8u * 6 * 4);
        for (size_t i = 0; i < 8u * 6; ++i) {
            source[i * 4 + 0] = static_cast<unsigned char>(i * 5);
            source[i * 4 + 1] = static_cast<unsigned char>(255 - i * 3);
            source[i * 4 + 2] = static_cast<unsigned char>(i * 7 % 256);
            source[i * 4 + 3] = static_cast<unsigned char>(i % 2 ? 255 : 128);
        }
        const auto written = encodeTga(source.data(), 8, 6);
        check(!written.empty(), "es wird etwas geschrieben");
        check(written.size() == 18 + 8u * 6 * 4, "Kopf plus Bildpunkte");

        const Image back = decodeTga(written.data(), written.size());
        check(back.ok, "und laesst sich wieder lesen");
        check(back.width == 8 && back.height == 6, "mit denselben Massen");
        check(back.rgba == source, "und byteweise denselben Bildpunkten");
        std::printf("  Targa-Rundlauf: %zu Byte, byteweise gleich\n",
                    written.size());

        check(encodeTga(nullptr, 8, 6).empty(), "Nullzeiger ergibt nichts");
        check(encodeTga(source.data(), 0, 6).empty(), "Breite null ebenso");
    }

    // --- Kaputte Dateien --------------------------------------------------
    check(!decodeTga(nullptr, 0).ok, "Nullzeiger");
    check(!decodeTga(kRgba32, 5).ok, "zu kurz fuer einen Kopf");
    {
        // Ein Kopf, der eine riesige Groesse behauptet.
        std::vector<unsigned char> huge(kRgba32, kRgba32 + sizeof(kRgba32));
        huge[12] = 0xFF; huge[13] = 0xFF;  // Breite 65535
        huge[14] = 0xFF; huge[15] = 0xFF;  // Hoehe 65535
        const Image result = decodeTga(huge.data(), huge.size());
        check(!result.ok, "eine unsinnige Groesse wird abgewiesen");
        check(result.error.find("implausible") != std::string::npos,
              "mit klarer Begruendung");
    }
    {
        // Abgeschnitten mitten in den Bildpunkten.
        const Image result = decodeTga(kRgba32, 30);
        check(!result.ok, "abgeschnittene Bilddaten werden abgewiesen");
    }
    {
        // Ein RLE-Paket, das ueber das Bildende hinauslaeuft — der Fall, mit
        // dem man einen naiven Leser aus dem Puffer schreiben laesst.
        std::vector<unsigned char> attack(kRle32, kRle32 + sizeof(kRle32));
        attack[18] = 0xFF;  // Wiederholung von 128 Bildpunkten bei 12 Platz
        const Image result = decodeTga(attack.data(), attack.size());
        check(!result.ok, "ein ueberlanges RLE-Paket wird abgewiesen");
        check(result.error.find("overrun") != std::string::npos ||
                  result.error.find("truncated") != std::string::npos,
              "mit Begruendung");
    }
    {
        // Zufaellige Bytes mit gueltig aussehendem Kopf.
        unsigned state = 999;
        int survived = 0;
        for (int attempt = 0; attempt < 200; ++attempt) {
            std::vector<unsigned char> noise(sizeof(kRgba32));
            std::memcpy(noise.data(), kRgba32, 18);  // Kopf behalten
            for (size_t i = 18; i < noise.size(); ++i) {
                state = state * 1664525u + 1013904223u;
                noise[i] = static_cast<unsigned char>(state >> 16);
            }
            const Image result = decodeTga(noise.data(), noise.size());
            if (!result.ok || result.rgba.size() == 48) ++survived;
        }
        check(survived == 200, "zweihundert verbogene Dateien, keine bricht aus");
    }
}

void testBlendModes() {
    std::cout << "== Mischungen ==\n";
    using namespace efx::shader;

    auto mode = [](BlendFactor src, BlendFactor dst) {
        return blendModeOf(src, dst);
    };
    auto isExact = [](BlendFactor src, BlendFactor dst) {
        bool exact = false;
        blendModeOf(src, dst, &exact);
        return exact;
    };

    // Die fuenf, die der Renderer genau kann.
    check(mode(BlendFactor::Unset, BlendFactor::Unset) == BlendMode::Opaque,
          "kein blendFunc heisst undurchsichtig");
    check(isExact(BlendFactor::Unset, BlendFactor::Unset), "und das ist genau");
    check(mode(BlendFactor::One, BlendFactor::Zero) == BlendMode::Opaque,
          "GL_ONE GL_ZERO ist undurchsichtig");
    check(mode(BlendFactor::One, BlendFactor::One) == BlendMode::Additive,
          "GL_ONE GL_ONE ist additiv");
    check(mode(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha) ==
              BlendMode::AlphaBlend,
          "GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA ist Alphamischung");
    check(mode(BlendFactor::DstColor, BlendFactor::Zero) == BlendMode::Modulate,
          "GL_DST_COLOR GL_ZERO moduliert");
    check(mode(BlendFactor::DstColor, BlendFactor::SrcColor) == BlendMode::Filter,
          "GL_DST_COLOR GL_SRC_COLOR filtert");
    for (auto pair : {std::pair{BlendFactor::One, BlendFactor::Zero},
                      {BlendFactor::One, BlendFactor::One},
                      {BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha},
                      {BlendFactor::DstColor, BlendFactor::Zero},
                      {BlendFactor::DstColor, BlendFactor::SrcColor}}) {
        check(isExact(pair.first, pair.second), "die fuenf sind alle genau");
    }

    // Und die haeufigste, die NICHT genau geht: GL_SRC_ALPHA GL_ONE. Die
    // steht in vielen JKA-Effektshadern und ist additiv mit weicher Kante.
    check(mode(BlendFactor::SrcAlpha, BlendFactor::One) == BlendMode::Additive,
          "GL_SRC_ALPHA GL_ONE wird additiv genaehert");
    check(!isExact(BlendFactor::SrcAlpha, BlendFactor::One),
          "und meldet, dass gerundet wurde");
    std::printf("  GL_SRC_ALPHA GL_ONE -> %s (genaehert)\n",
                blendModeName(mode(BlendFactor::SrcAlpha, BlendFactor::One)));

    // Vormultipliziertes Alpha.
    check(mode(BlendFactor::One, BlendFactor::OneMinusSrcAlpha) ==
              BlendMode::AlphaBlend,
          "GL_ONE GL_ONE_MINUS_SRC_ALPHA ist eine Alphamischung");

    // Massgeblich ist der Zielfaktor: bleibt das Vorhandene erhalten, wird
    // aufgehellt.
    for (auto src : {BlendFactor::SrcColor, BlendFactor::SrcAlpha,
                     BlendFactor::DstAlpha, BlendFactor::SrcAlphaSaturate}) {
        check(mode(src, BlendFactor::One) == BlendMode::Additive,
              "Zielfaktor GL_ONE heisst immer aufhellen");
    }

    // Kein Paar darf ohne Ergebnis bleiben — sonst faellt eine Stufe stumm
    // durch und wird gar nicht gezeichnet.
    const BlendFactor all[] = {
        BlendFactor::Unset, BlendFactor::One, BlendFactor::Zero,
        BlendFactor::SrcColor, BlendFactor::OneMinusSrcColor,
        BlendFactor::DstColor, BlendFactor::OneMinusDstColor,
        BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
        BlendFactor::DstAlpha, BlendFactor::OneMinusDstAlpha,
        BlendFactor::SrcAlphaSaturate};
    int exactCount = 0, total = 0;
    for (auto src : all) {
        for (auto dst : all) {
            bool exact = false;
            const BlendMode result = blendModeOf(src, dst, &exact);
            check(std::string(blendModeName(result)) != "?",
                  "jedes Paar ergibt eine gueltige Mischung");
            if (exact) ++exactCount;
            ++total;
        }
    }
    std::printf("  %d von %d Faktorenpaaren genau getroffen, der Rest gerundet\n",
                exactCount, total);
}

void testDiagnosticIdentity() {
    std::cout << "== Meldungskennungen ==\n";
    // Jede Meldung traegt ihre Kennung. Das ist der Unterschied zwischen
    // "die Regel greift" und "der Text lautet so" — und die Tests prueften
    // vorher das Falsche: sie wurden rot, sobald die Prueflist uebersetzt
    // war, obwohl sich an keiner Regel etwas geaendert hatte.
    efx::Effect effect;
    const auto empty = efx::validate(effect, efx::Dialect::Both);
    check(!empty.empty(), "ein leerer Effekt wird gemeldet");
    check(empty[0].id == efx::i18n::Str::VNoPrimitives,
          "und traegt die passende Kennung");

    // Dieselbe Regel, zwei Sprachen: gleiche Kennung, anderer Text.
    //
    // Beide Sprachen werden ausdruecklich gesetzt. Der erste Anlauf nahm an,
    // die Ausgangssprache sei Deutsch — sie ist Englisch, und der Test
    // verglich Englisch mit Englisch. Ein Test, der eine Voreinstellung
    // annimmt statt sie zu setzen, prueft etwas anderes als er behauptet.
    efx::i18n::setLanguage(efx::i18n::Language::German);
    const auto germanRun = efx::validate(effect, efx::Dialect::Both);
    efx::i18n::setLanguage(efx::i18n::Language::English);
    const auto englishRun = efx::validate(effect, efx::Dialect::Both);

    check(englishRun[0].id == germanRun[0].id, "die Kennung bleibt gleich");
    check(englishRun[0].message != germanRun[0].message, "der Text aendert sich");
    std::printf("  dieselbe Regel: \"%s\" / \"%s\"\n",
                germanRun[0].message.substr(0, 30).c_str(),
                englishRun[0].message.substr(0, 30).c_str());

    // Und in allen vier Sprachen ist sie nicht leer.
    for (auto language : {efx::i18n::Language::English, efx::i18n::Language::German,
                          efx::i18n::Language::ChineseSimplified,
                          efx::i18n::Language::Japanese}) {
        efx::i18n::setLanguage(language);
        const auto run = efx::validate(effect, efx::Dialect::Both);
        check(!run[0].message.empty(), "die Meldung ist in jeder Sprache da");
    }
    efx::i18n::setLanguage(efx::i18n::Language::German);
}

void testUndo() {
    std::cout << "== Rueckgaengig ==\n";
    using efx::undo::Stack;

    auto effectWith = [](int count) {
        efx::Effect e;
        for (int i = 0; i < count; ++i) {
            e.primitives.push_back(efx::Primitive{});
            e.primitives.back().name = "p" + std::to_string(i);
        }
        return e;
    };

    Stack stack;
    check(!stack.canUndo() && !stack.canRedo(), "leer geht nichts");
    check(stack.undo() == nullptr, "und Rueckgaengig liefert nichts");

    stack.reset(effectWith(1));
    check(!stack.canUndo(), "nach dem Anfangszustand gibt es nichts zurueckzunehmen");

    stack.record(effectWith(2), "Segment hinzugefuegt");
    stack.record(effectWith(3), "noch eines");
    check(stack.canUndo(), "jetzt schon");
    check(stack.undoLabel() == "noch eines",
          "beschrieben wird die Aenderung, die zurueckgenommen wuerde");

    const efx::Effect* back = stack.undo();
    check(back && back->primitives.size() == 2, "eins zurueck: zwei Primitiven");
    check(stack.canRedo(), "und Wiederherstellen ist moeglich");
    check(stack.redoLabel() == "noch eines", "mit derselben Beschreibung");

    back = stack.undo();
    check(back && back->primitives.size() == 1, "noch eins zurueck: eine");
    check(!stack.canUndo(), "weiter geht es nicht");
    check(stack.undo() == nullptr, "und es kommt nichts");

    const efx::Effect* forward = stack.redo();
    check(forward && forward->primitives.size() == 2, "vorwaerts: wieder zwei");
    forward = stack.redo();
    check(forward && forward->primitives.size() == 3, "und wieder drei");
    check(!stack.canRedo(), "weiter vorwaerts geht es nicht");

    // Wer nach einem Rueckgaengig etwas Neues tut, verwirft das Verworfene.
    stack.undo();
    stack.record(effectWith(9), "etwas anderes");
    check(!stack.canRedo(), "nach einer neuen Aenderung gibt es kein Vorwaerts mehr");
    const efx::Effect* afterNew = stack.undo();
    check(afterNew && afterNew->primitives.size() == 2,
          "und zurueck geht es auf den Stand davor");

    // Die Obergrenze. Was vorne herausfaellt, ist weg — aber die Liste bleibt
    // benutzbar, und die aktuelle Stelle wandert mit.
    {
        Stack small(4);
        small.reset(effectWith(0));
        for (int i = 1; i <= 20; ++i) {
            small.record(effectWith(i), "Schritt " + std::to_string(i));
        }
        check(small.size() == 4, "hoechstens vier Zustaende");
        check(small.canUndo(), "und man kann zurueck");
        int steps = 0;
        while (small.undo() != nullptr) ++steps;
        check(steps == 3, "drei Schritte zurueck bei vier Zustaenden");
        std::printf("  Grenze 4: nach 20 Aenderungen %zu Zustaende, %d Schritte "
                    "zurueck\n", small.size(), steps);
    }

    // Ein Zustand als Grenze darf nicht in eine Endlosschleife oder einen
    // leeren Zugriff laufen.
    {
        Stack tiny(1);
        tiny.reset(effectWith(1));
        tiny.record(effectWith(2), "eins");
        tiny.record(effectWith(3), "zwei");
        check(tiny.size() == 1, "genau ein Zustand");
        check(!tiny.canUndo() && !tiny.canRedo(), "und nichts zu tun");
        check(tiny.undoLabel().empty(), "keine Beschreibung");
    }

    // Der Inhalt wird wirklich kopiert, nicht nur verwiesen.
    {
        Stack copies;
        efx::Effect original = effectWith(1);
        copies.reset(original);
        copies.record(effectWith(2), "zwei");
        original.primitives.clear();      // das Original veraendern
        const efx::Effect* stored = copies.undo();
        check(stored && stored->primitives.size() == 1,
              "der gemerkte Zustand haengt nicht am Original");
    }
}

void testSound() {
    std::cout << "== Klang ==\n";
    using namespace efx::sound;

    // Referenzdaten: ein Sinus mit Pegelwechsel in der Mitte. Daran sieht man
    // Verschiebungen sofort — bei einem gleichmaessigen Ton faellt es nicht
    // auf, wenn die Haelfte fehlt.
    // mono16.wav: 6658 Byte
    const unsigned char kMono16Wav[] = {
        82, 73, 70, 70, 250, 25, 0, 0, 87, 65, 86, 69, 102, 109, 116, 32,
        16, 0, 0, 0, 1, 0, 1, 0, 34, 86, 0, 0, 68, 172, 0, 0,
        2, 0, 16, 0, 100, 97, 116, 97, 214, 25, 0, 0, 0, 0, 185, 11,
        67, 23, 111, 34, 18, 45, 255, 54, 15, 64, 29, 72, 10, 79, 185, 84,
        20, 89, 8, 92, 139, 93, 150, 93, 40, 92, 72, 89, 2, 85, 101, 79,
        138, 72, 139, 64, 137, 55, 167, 45, 14, 35, 232, 23, 98, 12, 170, 0,
        241, 244, 99, 233, 48, 222, 133, 211, 140, 201, 111, 192, 80, 184, 82, 177,
        145, 171, 34, 167, 25, 164, 129, 162, 97, 162, 185, 163, 132, 166, 183, 170,
        64, 176, 10, 183, 249, 190, 238, 199, 196, 209, 83, 220, 115, 231, 244, 242,
        171, 254, 101, 10, 247, 21, 48, 33, 229, 43, 232, 53, 19, 63, 65, 71,
        80, 78, 36, 84, 167, 88, 197, 91, 114, 93, 167, 93, 100, 92, 174, 89,
        144, 85, 25, 80, 97, 73, 129, 65, 155, 56, 209, 46, 74, 36, 50, 25,
        181, 13, 0, 2, 69, 246, 175, 234, 112, 223, 179, 212, 164, 202, 108, 193,
        47, 185, 14, 178, 39, 172, 145, 167, 95, 164, 157, 162, 82, 162, 127, 163,
        32, 166, 43, 170, 143, 175, 53, 182, 5, 190, 221, 198, 155, 208, 24, 219,
        41, 230, 162, 241, 85, 253, 17, 9, 170, 20, 240, 31, 181, 42, 207, 52,
        21, 62, 97, 70, 146, 77, 140, 83, 53, 88, 124, 91, 84, 93, 180, 93,
        156, 92, 16, 90, 25, 86, 201, 80, 52, 74, 116, 66, 170, 57, 248, 47,
        133, 37, 123, 26, 7, 15, 86, 3, 153, 247, 253, 235, 177, 224, 227, 213,
        190, 203, 108, 194, 17, 186, 206, 178, 195, 172, 5, 168, 170, 164, 189, 162,
        71, 162, 74, 163, 194, 165, 164, 169, 225, 174, 100, 181, 20, 189, 208, 197,
        118, 207, 223, 217, 225, 228, 81, 240, 255, 251, 189, 7, 92, 19, 174, 30,
        132, 41, 179, 51, 19, 61, 125, 69, 208, 76, 238, 82, 191, 87, 47, 91,
        49, 93, 188, 93, 207, 92, 108, 90, 158, 86, 116, 81, 3, 75, 100, 67,
        182, 58, 28, 49, 189, 38, 194, 27, 88, 16, 172, 4, 238, 248, 75, 237,
        244, 225, 22, 215, 220, 204, 111, 195, 246, 186, 146, 179, 98, 173, 126, 168,
        249, 164, 226, 162, 66, 162, 26, 163, 103, 165, 33, 169, 56, 174, 151, 180,
        38, 188, 197, 196, 82, 206, 167, 216, 155, 227, 0, 239, 169, 250, 104, 6,
        13, 18, 106, 29, 80, 40, 149, 50, 14, 60, 150, 68, 10, 76, 77, 82,
        68, 87, 222, 90, 9, 93, 191, 93, 253, 92, 196, 90, 31, 87, 27, 82,
        206, 75, 80, 68, 191, 59, 62, 50, 243, 39, 8, 29, 168, 17, 1, 6,
        67, 250, 155, 238, 57, 227, 74, 216, 252, 205, 118, 196, 223, 187, 90, 180,
        6, 174, 251, 168, 77, 165, 12, 163, 65, 162, 238, 162, 18, 165, 163, 168,
        147, 173, 206, 179, 60, 187, 190, 195, 50, 205, 114, 215, 85, 226, 176, 237,
        84, 249, 18, 5, 189, 16, 36, 28, 26, 39, 116, 49, 6, 59, 171, 67,
        64, 75, 167, 81, 197, 86, 135, 90, 221, 92, 190, 93, 38, 93, 23, 91,
        155, 87, 190, 82, 149, 76, 56, 69, 197, 60, 94, 51, 40, 41, 77, 30,
        248, 18, 86, 7, 153, 251, 235, 239, 127, 228, 129, 217, 30, 207, 127, 197,
        204, 188, 38, 181, 174, 174, 124, 169, 166, 165, 59, 163, 69, 162, 200, 162,
        193, 164, 41, 168, 242, 172, 9, 179, 85, 186, 185, 194, 20, 204, 63, 214,
        18, 225, 97, 236, 255, 247, 189, 3, 108, 15, 221, 26, 227, 37, 80, 48,
        251, 57, 189, 66, 114, 74, 253, 80, 66, 86, 44, 90, 172, 92, 183, 93,
        74, 93, 102, 91, 18, 88, 93, 83, 88, 77, 29, 70, 200, 61, 122, 52,
        90, 42, 143, 31, 70, 20, 171, 8, 238, 252, 61, 241, 199, 229, 186, 218,
        67, 208, 140, 198, 188, 189, 246, 181, 90, 175, 2, 170, 3, 166, 111, 163,
        78, 162, 166, 162, 117, 164, 180, 167, 86, 172, 72, 178, 114, 185, 184, 193,
        248, 202, 14, 213, 208, 223, 19, 235, 171, 246, 103, 2, 26, 14, 149, 25,
        169, 36, 41, 47, 236, 56, 203, 65, 160, 73, 78, 80, 185, 85, 204, 89,
        118, 92, 172, 93, 105, 93, 175, 91, 133, 88, 247, 83, 24, 78, 254, 70,
        199, 62, 148, 53, 138, 43, 208, 32, 147, 21, 255, 9, 68, 254, 143, 242,
        16, 231, 245, 219, 107, 209, 156, 199, 176, 190, 202, 182, 10, 176, 140, 170,
        102, 166, 167, 163, 92, 162, 137, 162, 45, 164, 67, 167, 189, 171, 138, 177,
        147, 184, 186, 192, 224, 201, 223, 211, 144, 222, 199, 233, 87, 245, 17, 1,
        200, 12, 75, 24, 109, 35, 1, 46, 219, 55, 213, 64, 203, 72, 156, 79,
        45, 85, 103, 89, 59, 92, 155, 93, 132, 93, 244, 91, 243, 88, 141, 84,
        211, 78, 220, 71, 196, 63, 171, 54, 184, 44, 16, 34, 223, 22, 83, 11,
        154, 255, 226, 243, 90, 232, 49, 221, 149, 210, 174, 200, 167, 191, 161, 183,
        191, 176, 27, 171, 205, 166, 228, 163, 111, 162, 113, 162, 235, 163, 215, 166,
        42, 171, 209, 176, 183, 183, 191, 191, 202, 200, 178, 210, 81, 221, 123, 232,
        3, 244, 188, 255, 117, 11, 1, 23, 48, 34, 214, 44, 199, 54, 221, 63,
        241, 71, 229, 78, 156, 84, 254, 88, 251, 91, 134, 93, 154, 93, 53, 92,
        93, 89, 30, 85, 138, 79, 181, 72, 189, 64, 192, 55, 227, 45, 78, 35,
        42, 24, 166, 12, 239, 0, 53, 245, 165, 233, 112, 222, 193, 211, 196, 201,
        161, 192, 125, 184, 120, 177, 174, 171, 56, 167, 39, 164, 134, 162, 93, 162,
        173, 163, 112, 166, 154, 170, 28, 176, 223, 182, 200, 190, 183, 199, 136, 209,
        20, 220, 49, 231, 177, 242, 102, 254, 33, 10, 180, 21, 240, 32, 168, 43,
        176, 53, 225, 62, 20, 71, 42, 78, 6, 84, 144, 88, 183, 91, 108, 93,
        170, 93, 112, 92, 194, 89, 172, 85, 61, 80, 139, 73, 178, 65, 209, 56,
        12, 47, 137, 36, 116, 25, 249, 13, 69, 2, 137, 246, 242, 234, 176, 223,
        239, 212, 220, 202, 159, 193, 92, 185, 52, 178, 70, 172, 168, 167, 109, 164,
        163, 162, 79, 162, 116, 163, 13, 166, 16, 170, 107, 175, 11, 182, 212, 189,
        167, 198, 97, 208, 217, 218, 231, 229, 95, 241, 16, 253, 205, 8, 103, 20,
        176, 31, 120, 42, 151, 52, 225, 61, 52, 70, 108, 77, 108, 83, 30, 88,
        109, 91, 77, 93, 182, 93, 167, 92, 34, 90, 52, 86, 235, 80, 93, 74,
        164, 66, 224, 57, 50, 48, 195, 37, 189, 26, 74, 15, 155, 3, 221, 247,
        64, 236, 242, 224, 32, 214, 247, 203, 159, 194, 62, 186, 245, 178, 226, 172,
        29, 168, 185, 164, 196, 162, 70, 162, 64, 163, 175, 165, 137, 169, 191, 174,
        59, 181, 228, 188, 154, 197, 59, 207, 160, 217, 160, 228, 13, 240, 187, 251,
        120, 7, 25, 19, 109, 30, 70, 41, 122, 51, 223, 60, 79, 69, 169, 76,
        206, 82, 167, 87, 31, 91, 42, 93, 189, 93, 216, 92, 126, 90, 184, 86,
        150, 81, 44, 75, 147, 67, 235, 58, 86, 49, 251, 38, 4, 28, 155, 16,
        240, 4, 50, 249, 143, 237, 53, 226, 83, 215, 21, 205, 163, 195, 36, 187,
        186, 179, 130, 173, 150, 168, 9, 165, 234, 162, 65, 162, 17, 163, 86, 165,
        7, 169, 22, 174, 111, 180, 247, 187, 144, 196, 24, 206, 105, 216, 89, 227,
        189, 238, 101, 250, 35, 6, 202, 17, 41, 29, 18, 40, 91, 50, 217, 59,
        103, 68, 226, 75, 44, 82, 43, 87, 205, 90, 1, 93, 191, 93, 5, 93,
        213, 90, 56, 87, 60, 82, 246, 75, 126, 68, 244, 59, 120, 50, 49, 40,
        73, 29, 235, 17, 69, 6, 135, 250, 222, 238, 122, 227, 136, 216, 53, 206,
        171, 196, 14, 188, 131, 180, 39, 174, 20, 169, 94, 165, 21, 163, 65, 162,
        230, 162, 1, 165, 138, 168, 114, 173, 166, 179, 13, 187, 137, 195, 248, 204,
        52, 215, 21, 226, 109, 237, 16, 249, 206, 4, 122, 16, 227, 27, 220, 38,
        57, 49, 209, 58, 124, 67, 23, 75, 133, 81, 171, 86, 117, 90, 212, 92,
        189, 93, 45, 93, 39, 91, 179, 87, 222, 82, 188, 76, 102, 69, 249, 60,
        151, 51, 101, 41, 141, 30, 58, 19, 154, 7, 221, 251, 47, 240, 192, 228,
        192, 217, 88, 207, 181, 197, 252, 188, 80, 181, 208, 174, 150, 169, 184, 165,
        69, 163, 70, 162, 192, 162, 177, 164, 17, 168, 210, 172, 226, 178, 39, 186,
        134, 194, 219, 203, 2, 214, 209, 224, 30, 236, 187, 247, 120, 3, 41, 15,
        156, 26, 164, 37, 21, 48, 197, 57, 140, 66, 73, 74, 218, 80, 39, 86,
        25, 90, 161, 92, 181, 93, 81, 93, 117, 91, 42, 88, 124, 83, 127, 77,
        74, 70, 251, 61, 179, 52, 151, 42, 208, 31, 137, 20, 239, 8, 51, 253,
        128, 241, 8, 230, 249, 218, 126, 208, 194, 198, 236, 189, 32, 182, 125, 175,
        29, 170, 23, 166, 122, 163, 80, 162, 160, 162, 102, 164, 157, 167, 55, 172,
        33, 178, 69, 185, 133, 193, 192, 202, 209, 212, 144, 223, 209, 234, 103, 246,
        35, 2, 215, 13, 83, 25, 106, 36, 238, 46, 182, 56, 154, 65, 118, 73,
        43, 80, 158, 85, 184, 89, 106, 92, 169, 93, 111, 93, 190, 91, 156, 88,
        21, 84, 61, 78, 43, 71, 250, 62, 204, 53, 198, 43, 17, 33, 214, 21,
        67, 10, 136, 254, 210, 242, 82, 231, 52, 220, 166, 209, 210, 199, 225, 190,
        245, 182, 46, 176, 169, 170, 122, 166, 179, 163, 95, 162, 132, 162, 32, 164,
        45, 167, 160, 171, 101, 177, 102, 184, 136, 192, 168, 201, 163, 211, 80, 222,
        132, 233, 19, 245, 205, 0, 132, 12, 9, 24, 46, 35, 197, 45, 164, 55,
        164, 64, 160, 72, 120, 79, 16, 85, 83, 89, 46, 92, 152, 93, 137, 93,
        2, 92, 9, 89, 170, 84, 248, 78, 7, 72, 246, 63, 227, 54, 244, 44,
        80, 34, 34, 23, 151, 11, 222, 255, 37, 244, 156, 232, 113, 221, 208, 210,
        230, 200, 216, 191, 205, 183, 228, 176, 56, 171, 226, 166, 241, 163, 115, 162,
        108, 162, 222, 163, 194, 166, 13, 171, 173, 176, 140, 183, 142, 191, 147, 200,
        119, 210, 18, 221, 57, 232, 192, 243, 120, 255, 49, 11, 190, 22, 240, 33,
        153, 44, 144, 54, 171, 63, 198, 71, 192, 78, 126, 84, 233, 88, 238, 91,
        129, 93, 157, 93, 65, 92, 114, 89, 59, 85, 174, 79, 224, 72, 238, 64,
        247, 55, 31, 46, 141, 35, 108, 24, 234, 12, 51, 1, 121, 245, 232, 233,
        176, 222, 253, 211, 252, 201, 211, 192, 169, 184, 157, 177, 204, 171, 78, 167,
        52, 164, 139, 162, 90, 162, 161, 163, 92, 166, 126, 170, 249, 175, 181, 182,
        151, 190, 128, 199, 77, 209, 213, 219, 239, 230, 109, 242, 34, 254, 221, 9,
        114, 21, 176, 32, 108, 43, 120, 53, 174, 62, 232, 70, 5, 78, 232, 83,
        122, 88, 168, 91, 102, 93, 173, 93, 123, 92, 214, 89, 199, 85, 96, 80,
        182, 73, 227, 65, 8, 57, 71, 47, 200, 36, 182, 25, 60, 14, 137, 2,
        205, 246, 53, 235, 240, 223, 44, 213, 21, 203, 210, 193, 137, 185, 91, 178,
        101, 172, 191, 167, 124, 164, 169, 162, 77, 162, 105, 163, 250, 165, 244, 169,
        73, 175, 225, 181, 164, 189, 113, 198, 38, 208, 155, 218, 166, 229, 27, 241,
        204, 252, 137, 8, 36, 20, 111, 31, 59, 42, 94, 52, 174, 61, 6, 70,
        69, 77, 77, 83, 7, 88, 94, 91, 71, 93, 184, 93, 177, 92, 53, 90,
        79, 86, 14, 81, 135, 74, 213, 66, 22, 58, 109, 48, 2, 38, 254, 26,
        142, 15, 223, 3, 33, 248, 131, 236, 50, 225, 94, 214, 48, 204, 211, 194,
        108, 186, 28, 179, 2, 173, 53, 168, 201, 164, 203, 162, 68, 162, 54, 163,
        157, 165, 111, 169, 157, 174, 18, 181, 180, 188, 101, 197, 1, 207, 98, 217,
        94, 228, 202, 239, 118, 251, 52, 7, 214, 18, 44, 30, 9, 41, 65, 51,
        171, 60, 33, 69, 129, 76, 174, 82, 143, 87, 15, 91, 34, 93, 190, 93,
        226, 92, 144, 90, 210, 86, 184, 81, 84, 75, 195, 67, 32, 59, 145, 49,
        58, 39, 69, 28, 223, 16, 52, 5, 118, 249, 210, 237, 118, 226, 145, 215,
        79, 205, 216, 195, 83, 187, 226, 179, 163, 173, 175, 168, 26, 165, 242, 162,
        65, 162, 8, 163, 68, 165, 238, 168, 245, 173, 70, 180, 200, 187, 91, 196,
        223, 205, 43, 216, 24, 227, 121, 238, 33, 250, 223, 5, 135, 17, 232, 28,
        213, 39, 33, 50, 165, 59, 56, 68, 186, 75, 11, 82, 18, 87, 188, 90,
        248, 92, 191, 93, 14, 93, 230, 90, 81, 87, 93, 82, 30, 76, 173, 68,
        40, 60, 177, 50, 111, 40, 138, 29, 46, 18, 138, 6, 204, 250, 33, 239,
        187, 227, 198, 216, 111, 206, 224, 196, 61, 188, 172, 180, 72, 174, 46, 169,
        112, 165, 30, 163, 66, 162, 222, 162, 241, 164, 113, 168, 82, 173, 127, 179,
        223, 186, 85, 195, 191, 204, 247, 214, 212, 225, 42, 237, 204, 248, 138, 4,
        54, 16, 162, 27, 158, 38, 255, 48, 155, 58, 76, 67, 238, 74, 99, 81,
        145, 86, 99, 90, 202, 92, 188, 93, 53, 93, 55, 91, 203, 87, 254, 82,
        228, 76, 148, 69, 45, 61, 208, 51, 162, 41, 206, 30, 125, 19, 223, 7,
        33, 252, 114, 240, 2, 229, 254, 217, 147, 207, 234, 197, 43, 189, 121, 181,
        242, 174, 177, 169, 203, 165, 79, 163, 72, 162, 185, 162, 162, 164, 249, 167,
        179, 172, 187, 178, 250, 185, 82, 194, 162, 203, 197, 213, 145, 224, 220, 235,
        119, 247, 52, 3, 229, 14, 90, 26, 101, 37, 218, 47, 143, 57, 92, 66,
        31, 74, 183, 80, 12, 86, 6, 90, 151, 92, 179, 93, 87, 93, 132, 91,
        65, 88, 155, 83, 165, 77, 119, 70, 46, 62, 235, 52, 212, 42, 16, 32,
        203, 20, 51, 9, 119, 253, 196, 241, 74, 230, 56, 219, 185, 208, 248, 198,
        29, 190, 74, 182, 160, 175, 57, 170, 42, 166, 133, 163, 83, 162, 154, 162,
        88, 164, 134, 167, 24, 172, 251, 177, 24, 185, 82, 193, 136, 202, 148, 212,
        80, 223, 142, 234, 35, 246, 222, 1, 147, 13, 17, 25, 43, 36, 179, 46,
        128, 56, 105, 65, 75, 73, 7, 80, 130, 85, 164, 89, 95, 92, 166, 93,
        117, 93, 204, 91, 178, 88, 52, 84, 99, 78, 87, 71, 45, 63, 4, 54,
        3, 44, 80, 33, 24, 22, 135, 10, 205, 254, 22, 243, 148, 231, 115, 220,
        225, 209, 9, 200, 18, 191, 32, 183, 82, 176, 197, 170, 142, 166, 191, 163,
        99, 162, 127, 162, 18, 164, 23, 167, 130, 171, 64, 177, 58, 184, 85, 192,
        112, 201, 103, 211, 16, 222, 66, 233, 207, 244, 136, 0, 64, 12, 199, 23,
        238, 34, 137, 45, 109, 55, 114, 64, 116, 72, 83, 79, 243, 84, 62, 89,
        34, 92, 148, 93, 141, 93, 15, 92, 30, 89, 200, 84, 28, 79, 51, 72,
        40, 64, 26, 55, 48, 45, 143, 34, 100, 23, 219, 11, 34, 0, 105, 244,
        222, 232, 176, 221, 12, 211, 29, 201, 10, 192, 249, 183, 8, 177, 86, 171,
        247, 166, 254, 163, 119, 162, 104, 162, 210, 163, 173, 166, 240, 170, 136, 176,
        96, 183, 92, 191, 92, 200, 59, 210, 210, 220, 247, 231, 124, 243, 51, 255,
        237, 10, 124, 22, 176, 33, 93, 44, 88, 54, 120, 63, 154, 71, 155, 78,
        96, 84, 211, 88, 224, 91, 124, 93, 161, 93, 77, 92, 134, 89, 87, 85,
        210, 79, 11, 73, 31, 65, 46, 56, 90, 46, 204, 35, 174, 24, 46, 13,
        120, 1, 189, 245, 42, 234, 239, 222, 58, 212, 52, 202, 6, 193, 213, 184,
        195, 177, 235, 171, 100, 167, 66, 164, 145, 162, 87, 162, 150, 163, 72, 166,
        98, 170, 213, 175, 138, 182, 102, 190, 74, 199, 18, 209, 150, 219, 173, 230,
        41, 242, 221, 253, 153, 9, 47, 21, 112, 32, 47, 43, 64, 53, 123, 62,
        187, 70, 223, 77, 201, 83, 99, 88, 154, 91, 96, 93, 176, 93, 134, 92,
        233, 89, 227, 85, 131, 80, 224, 73, 20, 66, 62, 57, 130, 47, 7, 37,
        248, 25, 128, 14, 205, 2, 17, 247, 119, 235, 48, 224, 105, 213, 77, 203,
        5, 194, 182, 185, 129, 178, 132, 172, 214, 167, 139, 164, 175, 162, 75, 162,
        95, 163, 231, 165, 217, 169, 38, 175, 183, 181, 116, 189, 59, 198, 235, 207,
        92, 218, 100, 229, 215, 240, 136, 252, 69, 8, 226, 19, 47, 31, 254, 41,
        37, 52, 122, 61, 217, 69, 30, 77, 46, 83, 239, 87, 79, 91, 64, 93,
        186, 93, 187, 92, 72, 90, 106, 86, 48, 81, 176, 74, 4, 67, 75, 58,
        168, 48, 64, 38, 64, 27, 209, 15, 35, 4, 102, 248, 198, 236, 115, 225,
        155, 214, 105, 204, 7, 195, 154, 186, 68, 179, 34, 173, 77, 168, 217, 164,
        211, 162, 67, 162, 44, 163, 139, 165, 85, 169, 123, 174, 233, 180, 132, 188,
        47, 197, 199, 206, 36, 217, 29, 228, 134, 239, 50, 251, 240, 6, 147, 18,
        235, 29, 204, 40, 8, 51, 119, 60, 243, 68, 90, 76, 142, 82, 118, 87,
        255, 90, 26, 93, 191, 93, 235, 92, 162, 90, 236, 86, 217, 81, 125, 75,
        242, 67, 85, 59, 203, 49, 120, 39, 134, 28, 34, 17, 121, 5, 187, 249,
        21, 238, 183, 226, 207, 215, 136, 205, 12, 196, 130, 187, 10, 180, 196, 173,
        200, 168, 43, 165, 251, 162, 65, 162, 255, 162, 51, 165, 213, 168, 212, 173,
        30, 180, 153, 187, 39, 196, 165, 205, 238, 215, 215, 226, 54, 238, 221, 249,
        155, 5, 67, 17, 167, 28, 151, 39, 232, 49, 112, 59, 9, 68, 145, 75,
        234, 81, 249, 86, 170, 90, 239, 92, 191, 93, 22, 93, 247, 90, 106, 87,
        126, 82, 70, 76, 220, 68, 93, 60, 235, 50, 173, 40, 203, 29, 113, 18,
        206, 6, 16, 251, 101, 239, 252, 227, 5, 217, 170, 206, 21, 197, 109, 188,
        212, 180, 106, 174, 72, 169, 130, 165, 40, 163, 67, 162, 214, 162, 225, 164,
        89, 168, 50, 173, 87, 179, 177, 186, 33, 195, 134, 204, 186, 214, 147, 225,
        231, 236, 136, 248, 69, 4, 243, 15, 96, 27, 96, 38, 197, 48, 102, 58,
        28, 67, 197, 74, 65, 81, 119, 86, 81, 90, 192, 92, 186, 93, 60, 93,
        71, 91, 227, 87, 30, 83, 11, 77, 194, 69, 97, 61, 9, 52, 224, 41,
        14, 31, 192, 19, 35, 8, 101, 252, 182, 240, 67, 229, 61, 218, 206, 207,
        32, 198, 92, 189, 163, 181, 21, 175, 204, 169, 222, 165, 89, 163, 74, 162,
        179, 162, 147, 164, 226, 167, 148, 172, 148, 178, 204, 185, 31, 194, 105, 203,
        136, 213, 80, 224, 153, 235, 64, 253, 235, 0, 146, 4, 39, 8, 156, 11,
        225, 14, 235, 17, 173, 20, 28, 23, 46, 25, 219, 26, 28, 28, 235, 28,
        71, 29, 45, 29, 157, 28, 155, 27, 42, 26, 79, 24, 19, 22, 126, 19,
        155, 16, 117, 13, 25, 10, 148, 6, 245, 2, 75, 255, 163, 251, 12, 248,
        149, 244, 77, 241, 63, 238, 121, 235, 5, 233, 237, 230, 59, 229, 244, 227,
        29, 227, 187, 226, 207, 226, 87, 227, 83, 228, 190, 229, 147, 231, 202, 233,
        90, 236, 57, 239, 92, 242, 181, 245, 56, 249, 214, 252, 128, 0, 40, 4,
        192, 7, 57, 11, 133, 14, 150, 17, 97, 20, 218, 22, 247, 24, 175, 26,
        253, 27, 218, 28, 66, 29, 54, 29, 180, 28, 190, 27, 89, 26, 138, 24,
        89, 22, 205, 19, 242, 16, 211, 13, 125, 10, 252, 6, 95, 3, 182, 255,
        12, 252, 115, 248, 248, 244, 169, 241, 148, 238, 197, 235, 72, 233, 37, 231,
        103, 229, 19, 228, 48, 227, 192, 226, 198, 226, 66, 227, 49, 228, 144, 229,
        89, 231, 133, 233, 11, 236, 226, 238, 254, 241, 81, 245, 208, 248, 108, 252,
        21, 0, 191, 3, 89, 7, 214, 10, 40, 14, 65, 17, 20, 20, 150, 22,
        190, 24, 131, 26, 220, 27, 198, 28, 60, 29, 61, 29, 200, 28, 224, 27,
        135, 26, 196, 24, 157, 22, 28, 20, 73, 17, 49, 14, 224, 10, 100, 7,
        201, 3, 32, 0, 118, 252, 219, 248, 91, 245, 7, 242, 235, 238, 19, 236,
        140, 233, 95, 231, 148, 229, 52, 228, 68, 227, 199, 226, 192, 226, 46, 227,
        16, 228, 98, 229, 32, 231, 65, 233, 190, 235, 140, 238, 160, 241, 238, 244,
        105, 248, 2, 252, 171, 255, 85, 3, 242, 6, 115, 10, 202, 13, 234, 16,
        198, 19, 82, 22, 132, 24, 84, 26, 187, 27, 177, 28, 53, 29, 67, 29,
        219, 28, 0, 28, 180, 26, 252, 24, 225, 22, 105, 20, 159, 17, 142, 14,
        67, 11, 203, 7, 51, 4, 138, 0, 225, 252, 66, 249, 191, 245, 101, 242,
        66, 239, 98, 236, 209, 233, 153, 231, 195, 229, 87, 228, 90, 227, 207, 226,
        187, 226, 28, 227, 241, 227, 54, 229, 232, 230, 254, 232, 113, 235, 54, 238,
        67, 241, 140, 244, 2, 248, 152, 251, 64, 255, 234, 2, 138, 6, 15, 10,
        107, 13, 146, 16, 118, 19, 12, 22, 73, 24, 37, 26, 151, 27, 155, 28,
        44, 29, 71, 29, 237, 28, 31, 28, 223, 26, 52, 25, 35, 23, 181, 20,
        244, 17, 235, 14, 166, 11, 50, 8, 157, 4, 245, 0, 75, 253, 171, 249,
        36, 246, 196, 242, 154, 239, 178, 236, 23, 234, 213, 231, 243, 229, 123, 228,
        113, 227, 217, 226, 183, 226, 11, 227, 211, 227, 12, 229, 177, 230, 189, 232,
        37, 235, 226, 237, 231, 240, 42, 244, 155, 247, 47, 251, 213, 254, 128, 2,
        33, 6, 170, 9, 12, 13, 57, 16, 38, 19, 197, 21, 13, 24, 244, 25,
        115, 27, 131, 28, 33, 29, 74, 29, 253, 28, 60, 28, 9, 27, 105, 25,
        100, 23, 0, 21, 72, 18, 70, 15, 7, 12, 152, 8, 6, 5, 96, 1,
        181, 253, 19, 250, 136, 246, 36, 243, 243, 239, 3, 237, 95, 234, 18, 232,
        37, 230, 160, 228, 137, 227, 229, 226, 181, 226, 251, 226, 182, 227, 227, 228,
        124, 230, 124, 232, 219, 234, 143, 237, 140, 240, 200, 243, 53, 247, 197, 250,
        107, 254, 21, 2, 185, 5, 69, 9, 172, 12, 224, 15, 212, 18, 125, 21,
        207, 23, 194, 25, 77, 27, 106, 28, 21, 29, 75, 29, 12, 29, 88, 28,
        49, 27, 158, 25, 163, 23, 74, 21, 155, 18, 161, 15, 104, 12, 254, 8,
        111, 5, 203, 1, 32, 254, 124, 250, 238, 246, 132, 243, 77, 240, 85, 237,
        167, 234, 80, 232, 88, 230, 199, 228, 163, 227, 241, 226, 181, 226, 237, 226,
        155, 227, 187, 228, 72, 230, 61, 232, 146, 234, 60, 237, 50, 240, 103, 243,
        207, 246, 92, 250, 0, 254, 171, 1, 80, 5, 223, 8, 75, 12, 134, 15,
        130, 18, 52, 21, 144, 23, 142, 25, 37, 27, 79, 28, 8, 29, 75, 29,
        25, 29, 114, 28, 88, 27, 209, 25, 226, 23, 147, 21, 237, 18, 251, 15,
        201, 12, 99, 9, 216, 5, 53, 2, 139, 254, 229, 250, 84, 247, 229, 243,
        168, 240, 167, 237, 241, 234, 143, 232, 140, 230, 239, 228, 191, 227, 0, 227,
        182, 226, 225, 226, 130, 227, 149, 228, 22, 230, 255, 231, 73, 234, 234, 236,
        216, 239, 7, 243, 106, 246, 244, 249, 149, 253, 64, 1, 230, 4, 121, 8,
        234, 11, 43, 15, 47, 18, 234, 20, 80, 23, 89, 25, 252, 26, 51, 28,
        249, 28, 73, 29, 37, 29, 139, 28, 126, 27, 3, 26, 31, 24, 218, 21,
        62, 19, 84, 16, 41, 13, 200, 9, 65, 6, 160, 2, 245, 254, 78, 251,
        186, 247, 71, 244, 3, 241, 251, 237, 60, 235, 208, 232, 194, 230, 24, 229,
        219, 227, 16, 227, 184, 226, 214, 226, 105, 227, 112, 228, 229, 229, 195, 231,
        2, 234, 154, 236, 128, 239, 168, 242, 5, 246, 139, 249, 43, 253, 213, 0,
        125, 4, 19, 8, 136, 11, 207, 14, 219, 17, 158, 20, 15, 23, 35, 25,
        210, 26, 22, 28, 232, 28, 70, 29, 47, 29, 162, 28, 162, 27, 51, 26,
        91, 24, 33, 22, 142, 19, 172, 16, 136, 13, 45, 10, 169, 6, 10, 3,
        96, 255, 184, 251, 33, 248, 169, 244, 95, 241, 80, 238, 136, 235, 18, 233,
        248, 230, 67, 229, 250, 227, 33, 227, 188, 226, 205, 226, 83, 227, 76, 228,
        181, 229, 135, 231, 188, 233, 74, 236, 40, 239, 73, 242, 161, 245, 35, 249,
        193, 252, 106, 0, 19, 4, 172, 7, 38, 11, 114, 14, 133, 17, 82, 20,
        204, 22, 236, 24, 167, 26, 246, 27, 214, 28, 65, 29, 55, 29, 184, 28,
        197, 27, 98, 26, 150, 24, 103, 22, 221, 19, 4, 17, 230, 13, 145, 10,
        17, 7, 116, 3, 203, 255, 34, 252, 136, 248, 12, 245, 188, 241, 166, 238,
        213, 235, 85, 233, 49, 231, 112, 229, 26, 228, 52, 227, 194, 226, 197, 226,
        62, 227, 42, 228, 135, 229, 77, 231, 119, 233, 252, 235, 209, 238, 235, 241,
        62, 245, 187, 248, 87, 252, 0, 0, 169, 3, 69, 7, 194, 10, 21, 14,
        47, 17, 4, 20, 137, 22, 179, 24, 121, 26, 214, 27, 194, 28, 59, 29,
        62, 29, 204, 28, 230, 27, 144, 26, 207, 24, 171, 22, 43, 20, 90, 17,
        68, 14, 244, 10, 120, 7, 222, 3, 53, 0, 140, 252, 239, 248, 111, 245,
        26, 242, 252, 238, 35, 236, 153, 233, 106, 231, 158, 229, 59, 228, 72, 227,
        201, 226, 191, 226, 42, 227, 10, 228, 89, 229, 20, 231, 52, 233, 174, 235,
        123, 238, 142, 241, 218, 244, 84, 248, 237, 251, 150, 255, 63, 3, 221, 6,
        95, 10, 183, 13, 216, 16, 182, 19, 68, 22, 121, 24, 75, 26, 180, 27,
        173, 28, 51, 29, 68, 29, 223, 28, 6, 28, 189, 26, 8, 25, 238, 22,
        120, 20, 176, 17, 161, 14, 87, 11, 223, 7, 72, 4, 160, 0, 246, 252,
        87, 249, 211, 245, 120, 242, 84, 239, 114, 236, 223, 233, 165, 231, 205, 229,
        94, 228, 94, 227, 209, 226, 186, 226, 24, 227, 234, 227, 46, 229, 221, 230,
        241, 232, 98, 235, 37, 238, 49, 241, 120, 244, 237, 247, 131, 251, 43, 255,
        213, 2, 117, 6, 251, 9, 88, 13, 128, 16, 102, 19, 254, 21, 61, 24,
        27, 26, 144, 27, 151, 28, 42, 29, 72, 29, 240, 28, 37, 28, 232, 26,
        62, 25, 48, 23, 196, 20, 5, 18, 253, 14, 185, 11, 70, 8, 178, 4,
        11, 1, 96, 253, 191, 249, 56, 246, 215, 242, 172, 239, 194, 236, 38, 234,
        225, 231, 253, 229, 130, 228, 117, 227, 219, 226, 183, 226, 7, 227, 205, 227,
        4, 229, 167, 230, 176, 232, 22, 235, 209, 237, 213, 240, 22, 244, 135, 247,
        26, 251, 192, 254, 107, 2, 12, 6, 150, 9, 249, 12, 40, 16, 22, 19,
        183, 21, 1, 24, 234, 25, 107, 27, 126, 28, 31, 29, 74, 29, 0, 29,
        65, 28, 17, 27, 116, 25, 113, 23, 15, 21, 89, 18, 88, 15, 27, 12,
        172, 8, 27, 5, 117, 1, 203, 253, 40, 250, 157, 246, 55, 243, 5, 240,
        19, 237, 109, 234, 30, 232, 47, 230, 168, 228, 142, 227, 231, 226, 181, 226,
        248, 226, 177, 227, 219, 228, 114, 230, 112, 232, 204, 234, 126, 237, 122, 240,
        181, 243, 33, 247, 176, 250, 85, 254, 0, 2, 164, 5, 49, 9, 153, 12,
        206, 15, 196, 18, 110, 21, 195, 23, 184, 25, 69, 27, 101, 28, 19, 29,
        75, 29, 15, 29, 93, 28, 57, 27, 168, 25, 176, 23, 89, 21, 171, 18,
        179, 15, 124, 12, 18, 9, 132, 5, 224, 1, 53, 254, 145, 250, 2, 247,
        152, 243, 95, 240, 101, 237, 182, 234, 93, 232, 98, 230, 207, 228, 168, 227,
        244, 226, 181, 226, 235, 226, 150, 227, 179, 228, 62, 230, 49, 232, 131, 234,
        44, 237, 32, 240, 84, 243, 187, 246, 71, 250, 235, 253, 149, 1, 59, 5,
        203, 8, 56, 12, 116, 15, 113, 18, 37, 21, 132, 23, 132, 25, 29, 27,
        74, 28, 5, 29, 75, 29, 27, 29, 119, 28, 96, 27, 219, 25, 238, 23,
        161, 21, 253, 18, 13, 16, 220, 12, 120, 9, 237, 5, 75, 2, 160, 254,
        250, 250, 104, 247, 249, 243, 186, 240, 184, 237, 0, 235, 156, 232, 151, 230,
        247, 228, 196, 227, 3, 227, 182, 226, 223, 226, 125, 227, 141, 228, 12, 230,
        243, 231, 59, 234, 218, 236, 199, 239, 244, 242, 86, 246, 223, 249, 128, 253,
        43, 1, 209, 4, 101, 8, 214, 11, 25, 15, 30, 18, 219, 20, 67, 23,
        79, 25, 244, 26, 45, 28, 245, 28, 73, 29, 39, 29, 143, 28, 133, 27,
        13, 26, 43, 24, 233, 21, 78, 19, 102, 16, 60, 13, 220, 9, 85, 6,
        181, 2, 11, 255, 99, 251, 206, 247, 90, 244, 21, 241, 12, 238, 75, 235,
        221, 232, 204, 230, 33, 229, 225, 227, 19, 227, 185, 226, 212, 226, 101, 227,
        105, 228, 219, 229, 183, 231, 244, 233, 138, 236, 110, 239, 149, 242, 241, 245,
        118, 249, 22, 253, 192, 0, 104, 4, 254, 7, 116, 11, 189, 14, 202, 17,
        143, 20, 2, 23, 24, 25, 202, 26, 15, 28, 228, 28, 69, 29, 49, 29,
        166, 28, 169, 27, 61, 26, 103, 24, 47, 22, 158, 19, 190, 16, 155, 13,
        65, 10, 190, 6, 31, 3, 118, 255, 205, 251, 53, 248, 189, 244, 114, 241,
        97, 238, 151, 235, 31, 233, 4, 231, 76, 229, 0, 228, 37, 227, 189, 226,
        203, 226, 79, 227, 69, 228, 172, 229, 124, 231, 174, 233, 58, 236, 22, 239,
        54, 242, 141, 245, 14, 249, 171, 252, 85, 0, 254, 3, 151, 7, 18, 11,
        96, 14, 116, 17, 66, 20, 191, 22, 224, 24, 158, 26, 240, 27, 210, 28,
        64, 29, 57, 29, 188, 28, 204, 27, 108, 26, 161, 24, 116, 22, 237, 19,
        21, 17, 249, 13, 165, 10, 37, 7, 138, 3, 224, 255, 55, 252, 156, 248,
        32, 245, 207, 241, 183, 238, 228, 235, 99, 233, 60, 231, 121, 229, 32, 228,
        56, 227, 195, 226, 196, 226, 58, 227, 36, 228, 125, 229, 66, 231, 106, 233,
        236, 235, 191, 238, 216, 241, 42, 245, 167, 248, 65, 252, 235, 255, 148, 3,
        48, 7, 175, 10, 2, 14, 30, 17, 245, 19, 123, 22, 167, 24, 112, 26,
        207, 27, 190, 28, 58, 29, 64, 29, 208, 28, 237, 27, 153, 26, 219, 24,
        184, 22, 59, 20, 108, 17, 87, 14, 8, 11, 141, 7, 244, 3, 74, 0,
        161, 252, 4, 249, 131, 245, 45, 242, 14, 239, 51, 236, 167, 233, 118, 231,
        167, 229, 66, 228, 76, 227, 202, 226, 190, 226, 38, 227, 3, 228, 81, 229,
        9, 231, 38, 233, 159, 235, 106, 238, 123, 241, 199, 244, 64, 248, 216, 251,
        128, 255, 42, 3, 200, 6, 75, 10, 164, 13, 199, 16, 166, 19, 54, 22,
        109, 24, 66, 26, 173, 27, 169, 28, 49, 29, 69, 29, 227, 28, 12, 28,
        197, 26, 19, 25, 251, 22, 135, 20, 193, 17, 179, 14, 107, 11, 244, 7,
        93, 4, 181, 0, 11, 253, 108, 249, 231, 245, 139, 242, 101, 239, 130, 236,
        237, 233, 177, 231, 214, 229, 101, 228, 99, 227, 211, 226, 185, 226, 21, 227,
        228, 227, 37, 229, 210, 230, 228, 232, 83, 235, 21, 238, 31, 241, 100, 244,
        217, 247, 110, 251, 21, 255, 192, 2, 96, 6, 231, 9, 69, 13, 111, 16,
        86, 19, 240, 21, 49, 24, 18, 26, 137, 27, 146, 28, 40, 29, 73, 29,
        244, 28, 42, 28, 240, 26, 73, 25, 61, 23, 211, 20, 22, 18, 15, 15,
        205, 11, 91, 8, 199, 4, 32, 1, 117, 253, 212, 249, 76, 246, 234, 242,
        190, 239, 210, 236, 52, 234, 237, 231, 7, 230, 137, 228, 122, 227, 222, 226,
        182, 226, 4, 227, 199, 227, 251, 228, 156, 230, 163, 232, 7, 235, 193, 237,
        195, 240, 2, 244, 114, 247, 4, 251, 171, 254, 85, 2, 247, 5, 130, 9,
        230, 12, 22, 16, 5, 19, 168, 21, 244, 23, 224, 25, 100, 27, 121, 28,
        29, 29, 75, 29, 3, 29, 71, 28, 25, 27, 127, 25, 125, 23, 30, 21,
        105, 18, 107, 15, 46, 12, 193, 8, 48, 5, 139, 1, 224, 253, 61, 250,
        177, 246, 74, 243, 23, 240, 35, 237, 124, 234, 43, 232, 57, 230, 175, 228,
        147, 227, 234, 226, 181, 226, 246, 226, 171, 227, 211, 228, 103, 230, 99, 232,
        189, 234, 109, 237, 104, 240, 161, 243, 12, 247, 155, 250, 64, 254, 235, 1,
        143, 5, 28, 9, 133, 12, 188, 15, 180, 18, 96, 21, 182, 23, 173, 25,
        61, 27, 96, 28, 16, 29, 75, 29, 17, 29, 98, 28, 65, 27, 178, 25,
        189, 23, 103, 21, 188, 18, 197, 15, 143, 12, 39, 9, 153, 5, 245, 1,
        75, 254, 166, 250, 22, 247, 171, 243, 113, 240, 118, 237, 197, 234, 105, 232,
        109, 230, 215, 228, 174, 227, 247, 226, 181, 226, 232, 226, 145, 227, 171, 228,
        52, 230, 36, 232, 116, 234, 27, 237, 14, 240, 65, 243, 167, 246, 50, 250,
        213, 253, 128, 1, 38, 5, 183, 8, 36, 12, 98, 15, 97, 18, 22, 21,
        119, 23, 121, 25, 21, 27, 68, 28, 2, 29, 75, 29, 30, 29, 124, 28,
        104, 27, 229, 25, 251, 23, 176, 21, 13, 19, 31, 16, 239, 12, 140, 9,
        2, 6, 96, 2, 181, 254, 15, 251, 124, 247, 12, 244, 204, 240, 201, 237,
        15, 235, 169, 232, 161, 230, 255, 228, 202, 227, 6, 227, 182, 226, 220, 226,
        120, 227, 134, 228, 2, 230, 231, 231, 45, 234, 202, 236, 181, 239, 225, 242,
        66, 246, 202, 249, 107, 253, 21, 1, 188, 4, 80, 8, 195, 11, 6, 15,
        13, 18, 204, 20, 54, 23, 68, 25, 236, 26, 39, 28, 242, 28, 72, 29,
        41, 29, 148, 28, 141, 27, 22, 26, 55, 24, 247, 21, 94, 19, 120, 16,
        79, 13, 241, 9, 106, 6, 202, 2, 32, 255, 120, 251, 227, 247, 110, 244,
        40, 241, 29, 238, 90, 235, 234, 232, 215, 230, 41, 229, 231, 227, 22, 227,
        185, 226, 210, 226, 96, 227, 97, 228, 209, 229, 171, 231, 230, 233, 122, 236,
        92, 239, 130, 242, 221, 245, 98, 249, 0, 253, 170, 0, 83, 4, 234, 7,
        97, 11, 170, 14, 185, 17, 128, 20, 245, 22, 13, 25, 193, 26, 9, 28,
        225, 28, 68, 29, 50, 29, 171, 28, 176, 27, 70, 26, 115, 24, 61, 22,
        174, 19, 208, 16, 174, 13, 85, 10, 210, 6, 53, 3, 139, 255, 226, 251,
        74, 248, 209, 244, 132, 241, 114, 238, 167, 235, 45, 233, 15, 231, 85, 229,
        6, 228, 40, 227, 190, 226, 201, 226, 74, 227, 62, 228, 162, 229, 112, 231,
        160, 233, 43, 236, 5, 239, 35, 242, 121, 245, 250, 248, 150, 252, 64, 0,
        233, 3, 131, 7, 254, 10, 77, 14, 99, 17, 51, 20, 178, 22, 213, 24,
        149, 26, 234, 27, 206, 28, 63, 29, 58, 29, 192, 28, 210, 27, 117, 26,
        173, 24, 130, 22, 252, 19, 39, 17, 12, 14, 185, 10, 58, 7, 159, 3,
        246, 255, 76, 252, 177, 248, 52, 245, 225, 241, 200, 238, 244, 235, 112, 233,
        71, 231, 130, 229, 39, 228, 60, 227, 196, 226, 194, 226, 54, 227, 29, 228,
        116, 229, 54, 231, 92, 233, 221, 235, 174, 238, 197, 241, 22, 245, 146, 248,
        44, 252, 214, 255, 127, 3, 27, 7, 155, 10, 240, 13, 13, 17, 229, 19,
        109, 22, 156, 24, 103, 26, 200, 27, 186, 28, 56, 29, 65, 29, 212, 28,
        243, 27, 162, 26, 230, 24, 198, 22, 74, 20, 125, 17, 105, 14, 28, 11,
        162, 7, 9, 4, 96, 0, 182, 252, 25, 249, 151, 245, 63, 242, 31, 239,
        66, 236, 181, 233, 129, 231, 176, 229, 73, 228, 81, 227, 204, 226, 189, 226,
        35, 227, 253, 227, 72, 229, 254, 230, 25, 233, 144, 235, 88, 238, 104, 241,
        179, 244, 43, 248, 194, 251, 107, 255, 21, 3, 179, 6, 55, 10, 145, 13,
        181, 16, 150, 19, 40, 22, 97, 24, 56, 26, 166, 27, 164, 28, 48, 29,
        70, 29, 230, 28, 18, 28, 206, 26, 30, 25, 8, 23, 151, 20, 210, 17,
        198, 14, 126, 11, 9, 8, 114, 4, 203, 0, 32, 253, 129, 249, 251, 245,
        158, 242, 119, 239, 146, 236, 251, 233, 189, 231, 224, 229, 108, 228, 103, 227,
        213, 226, 184, 226, 17, 227, 222, 227, 29, 229, 199, 230, 215, 232, 68, 235,
        4, 238, 12, 241, 81, 244, 196, 247, 89, 251, 0, 255, 170, 2, 75, 6,
        210, 9, 50, 13, 93, 16, 70, 19, 226, 21, 37, 24, 8, 26, 130, 27,
        141, 28, 38, 29, 73, 29, 247, 28, 48, 28, 248, 26, 84, 25, 74, 23,
        226, 20, 38, 18, 34, 15, 224, 11, 111, 8, 220, 4, 53, 1, 139, 253,
        233, 249, 96, 246, 254, 242, 207, 239, 226, 236, 66, 234, 249, 231, 17, 230,
        145, 228, 127, 227, 224, 226, 182, 226, 1, 227, 193, 227, 243, 228, 145, 230,
        150, 232, 249, 234, 176, 237, 177, 240, 239, 243, 94, 247, 239, 250, 149, 254,
        64, 2, 227, 5, 109, 9, 210, 12, 4, 16, 245, 18, 154, 21, 232, 23,
        214, 25, 92, 27, 116, 28, 26, 29, 75, 29, 6, 29, 77, 28, 33, 27,
        137, 25, 138, 23, 44, 21, 122, 18, 125, 15, 66, 12, 213, 8, 69, 5,
        160, 1, 245, 253, 82, 250, 197, 246, 94, 243, 41, 240, 52, 237, 138, 234,
        55, 232, 67, 230, 183, 228, 152, 227, 236, 226, 181, 226, 243, 226, 166, 227,
        203, 228, 93, 230, 86, 232, 175, 234, 93, 237, 86, 240, 142, 243, 248, 246,
        134, 250,
    };

    // mono8.wav: 3351 Byte
    const unsigned char kMono8Wav[] = {
        82, 73, 70, 70, 15, 13, 0, 0, 87, 65, 86, 69, 102, 109, 116, 32,
        16, 0, 0, 0, 1, 0, 1, 0, 34, 86, 0, 0, 34, 86, 0, 0,
        1, 0, 8, 0, 100, 97, 116, 97, 235, 12, 0, 0, 128, 139, 151, 162,
        173, 182, 192, 200, 207, 212, 217, 220, 221, 221, 220, 217, 213, 207, 200, 192,
        183, 173, 163, 151, 140, 128, 116, 105, 94, 83, 73, 64, 56, 49, 43, 39,
        36, 34, 34, 35, 38, 42, 48, 55, 62, 71, 81, 92, 103, 114, 126, 138,
        149, 161, 171, 181, 191, 199, 206, 212, 216, 219, 221, 221, 220, 217, 213, 208,
        201, 193, 184, 174, 164, 153, 141, 130, 118, 106, 95, 84, 74, 65, 57, 50,
        44, 39, 36, 34, 34, 35, 38, 42, 47, 54, 62, 70, 80, 91, 102, 113,
        125, 137, 148, 159, 170, 180, 190, 198, 205, 211, 216, 219, 221, 221, 220, 218,
        214, 208, 202, 194, 185, 175, 165, 154, 143, 131, 119, 107, 96, 85, 75, 66,
        58, 50, 44, 40, 36, 34, 34, 35, 37, 41, 46, 53, 61, 69, 79, 89,
        100, 112, 123, 135, 147, 158, 169, 179, 189, 197, 204, 210, 215, 219, 221, 221,
        220, 218, 214, 209, 203, 195, 186, 177, 166, 155, 144, 132, 120, 109, 97, 87,
        76, 67, 58, 51, 45, 40, 36, 34, 34, 35, 37, 41, 46, 52, 60, 68,
        78, 88, 99, 111, 122, 134, 146, 157, 168, 178, 188, 196, 204, 210, 215, 218,
        221, 221, 220, 218, 215, 210, 203, 196, 187, 178, 167, 157, 145, 134, 122, 110,
        99, 88, 77, 68, 59, 52, 46, 40, 37, 35, 34, 34, 37, 40, 45, 51,
        59, 67, 77, 87, 98, 109, 121, 133, 144, 156, 167, 177, 187, 195, 203, 209,
        214, 218, 220, 221, 221, 219, 215, 210, 204, 197, 188, 179, 169, 158, 146, 135,
        123, 111, 100, 89, 79, 69, 60, 53, 46, 41, 37, 35, 34, 34, 36, 40,
        44, 51, 58, 66, 76, 86, 97, 108, 119, 131, 143, 154, 165, 176, 185, 194,
        202, 208, 214, 218, 220, 221, 221, 219, 216, 211, 205, 198, 189, 180, 170, 159,
        148, 136, 124, 113, 101, 90, 80, 70, 61, 53, 47, 42, 38, 35, 34, 34,
        36, 39, 44, 50, 57, 65, 74, 85, 95, 107, 118, 130, 142, 153, 164, 175,
        184, 193, 201, 208, 213, 217, 220, 221, 221, 219, 216, 211, 206, 198, 190, 181,
        171, 160, 149, 137, 126, 114, 103, 91, 81, 71, 62, 54, 48, 42, 38, 35,
        34, 34, 36, 39, 43, 49, 56, 64, 73, 83, 94, 105, 117, 129, 140, 152,
        163, 174, 183, 192, 200, 207, 213, 217, 220, 221, 221, 219, 216, 212, 206, 199,
        191, 182, 172, 162, 150, 139, 127, 115, 104, 93, 82, 72, 63, 55, 48, 43,
        38, 35, 34, 34, 35, 38, 43, 48, 55, 63, 72, 82, 93, 104, 116, 127,
        139, 151, 162, 172, 182, 191, 199, 206, 212, 216, 219, 221, 221, 220, 217, 213,
        207, 200, 192, 183, 173, 163, 152, 140, 128, 117, 105, 94, 83, 73, 64, 56,
        49, 43, 39, 36, 34, 34, 35, 38, 42, 48, 54, 62, 71, 81, 92, 103,
        114, 126, 138, 149, 160, 171, 181, 190, 199, 206, 212, 216, 219, 221, 221, 220,
        217, 213, 208, 201, 193, 184, 175, 164, 153, 141, 130, 118, 106, 95, 84, 74,
        65, 57, 50, 44, 39, 36, 34, 34, 35, 38, 42, 47, 54, 61, 70, 80,
        90, 101, 113, 125, 136, 148, 159, 170, 180, 189, 198, 205, 211, 216, 219, 221,
        221, 220, 218, 214, 208, 202, 194, 185, 176, 165, 154, 143, 131, 119, 108, 96,
        86, 75, 66, 58, 50, 44, 40, 36, 34, 34, 35, 37, 41, 46, 53, 60,
        69, 79, 89, 100, 112, 123, 135, 147, 158, 169, 179, 188, 197, 204, 210, 215,
        219, 221, 221, 220, 218, 214, 209, 203, 195, 186, 177, 166, 156, 144, 132, 121,
        109, 98, 87, 77, 67, 59, 51, 45, 40, 37, 34, 34, 35, 37, 41, 46,
        52, 59, 68, 78, 88, 99, 110, 122, 134, 145, 157, 168, 178, 187, 196, 203,
        210, 215, 218, 221, 221, 221, 218, 215, 210, 203, 196, 187, 178, 168, 157, 145,
        134, 122, 110, 99, 88, 78, 68, 60, 52, 46, 41, 37, 35, 34, 34, 37,
        40, 45, 51, 59, 67, 76, 87, 98, 109, 121, 132, 144, 155, 166, 177, 186,
        195, 203, 209, 214, 218, 220, 221, 221, 219, 215, 210, 204, 197, 188, 179, 169,
        158, 147, 135, 123, 112, 100, 89, 79, 69, 60, 53, 46, 41, 37, 35, 34,
        34, 36, 40, 44, 50, 58, 66, 75, 86, 96, 108, 119, 131, 143, 154, 165,
        176, 185, 194, 202, 208, 214, 218, 220, 221, 221, 219, 216, 211, 205, 198, 189,
        180, 170, 159, 148, 136, 125, 113, 102, 90, 80, 70, 61, 54, 47, 42, 38,
        35, 34, 34, 36, 39, 44, 50, 57, 65, 74, 84, 95, 106, 118, 130, 141,
        153, 164, 174, 184, 193, 201, 208, 213, 217, 220, 221, 221, 219, 216, 212, 206,
        199, 190, 181, 171, 161, 149, 138, 126, 114, 103, 92, 81, 71, 62, 54, 48,
        42, 38, 35, 34, 34, 36, 39, 43, 49, 56, 64, 73, 83, 94, 105, 117,
        128, 140, 152, 163, 173, 183, 192, 200, 207, 213, 217, 220, 221, 221, 220, 217,
        212, 206, 200, 191, 182, 172, 162, 151, 139, 127, 116, 104, 93, 82, 72, 63,
        55, 48, 43, 38, 35, 34, 34, 35, 38, 43, 48, 55, 63, 72, 82, 93,
        104, 115, 127, 139, 150, 161, 172, 182, 191, 199, 206, 212, 216, 219, 221, 221,
        220, 217, 213, 207, 200, 192, 183, 174, 163, 152, 140, 129, 117, 105, 94, 83,
        73, 64, 56, 49, 43, 39, 36, 34, 34, 35, 38, 42, 47, 54, 62, 71,
        81, 91, 102, 114, 126, 137, 149, 160, 171, 181, 190, 198, 206, 211, 216, 219,
        221, 221, 220, 217, 213, 208, 201, 193, 185, 175, 164, 153, 142, 130, 118, 107,
        95, 85, 75, 65, 57, 50, 44, 39, 36, 34, 34, 35, 37, 41, 47, 53,
        61, 70, 80, 90, 101, 113, 124, 136, 148, 159, 170, 180, 189, 198, 205, 211,
        216, 219, 221, 221, 220, 218, 214, 209, 202, 194, 186, 176, 166, 154, 143, 131,
        120, 108, 97, 86, 76, 66, 58, 51, 45, 40, 36, 34, 34, 35, 37, 41,
        46, 53, 60, 69, 79, 89, 100, 111, 123, 135, 146, 158, 169, 179, 188, 197,
        204, 210, 215, 219, 221, 221, 220, 218, 214, 209, 203, 195, 187, 177, 167, 156,
        144, 133, 121, 109, 98, 87, 77, 67, 59, 51, 45, 40, 37, 34, 34, 35,
        37, 40, 45, 52, 59, 68, 77, 88, 99, 110, 122, 133, 145, 156, 167, 178,
        187, 196, 203, 210, 215, 218, 220, 221, 221, 218, 215, 210, 204, 196, 188, 178,
        168, 157, 146, 134, 122, 111, 99, 88, 78, 68, 60, 52, 46, 41, 37, 35,
        34, 34, 36, 40, 45, 51, 58, 67, 76, 86, 97, 109, 120, 132, 144, 155,
        166, 176, 186, 195, 202, 209, 214, 218, 220, 221, 221, 219, 215, 210, 204, 197,
        189, 179, 169, 158, 147, 135, 124, 112, 101, 89, 79, 69, 61, 53, 46, 41,
        37, 35, 34, 34, 36, 39, 44, 50, 57, 66, 75, 85, 96, 107, 119, 131,
        142, 154, 165, 175, 185, 194, 202, 208, 214, 218, 220, 221, 221, 219, 216, 211,
        205, 198, 190, 180, 170, 160, 148, 137, 125, 113, 102, 91, 80, 70, 62, 54,
        47, 42, 38, 35, 34, 34, 36, 39, 44, 49, 57, 65, 74, 84, 95, 106,
        118, 129, 141, 153, 164, 174, 184, 193, 201, 208, 213, 217, 220, 221, 221, 219,
        216, 212, 206, 199, 191, 182, 172, 161, 150, 138, 126, 115, 103, 92, 81, 72,
        63, 55, 48, 42, 38, 35, 34, 34, 36, 39, 43, 49, 56, 64, 73, 83,
        94, 105, 116, 128, 140, 151, 162, 173, 183, 192, 200, 207, 212, 217, 220, 221,
        221, 220, 217, 212, 207, 200, 192, 183, 173, 162, 151, 139, 128, 116, 104, 93,
        83, 73, 64, 55, 49, 43, 38, 35, 34, 34, 35, 38, 42, 48, 55, 63,
        72, 82, 92, 103, 115, 127, 138, 150, 161, 172, 182, 191, 199, 206, 212, 216,
        219, 221, 221, 220, 217, 213, 207, 201, 193, 184, 174, 163, 152, 141, 129, 117,
        106, 94, 84, 74, 65, 56, 49, 43, 39, 36, 34, 34, 35, 38, 42, 47,
        54, 62, 71, 81, 91, 102, 114, 125, 137, 149, 160, 171, 181, 190, 198, 205,
        211, 216, 219, 221, 221, 220, 217, 213, 208, 201, 194, 185, 175, 165, 153, 142,
        130, 119, 107, 96, 85, 75, 66, 57, 50, 44, 39, 36, 34, 34, 35, 37,
        41, 47, 53, 61, 70, 79, 90, 101, 112, 124, 136, 147, 159, 169, 180, 189,
        197, 205, 211, 215, 219, 221, 221, 220, 218, 214, 209, 202, 195, 186, 176, 166,
        155, 143, 132, 120, 108, 97, 86, 76, 67, 58, 51, 45, 40, 36, 34, 34,
        35, 37, 41, 46, 52, 60, 69, 78, 89, 100, 111, 123, 134, 146, 157, 168,
        179, 188, 196, 204, 210, 215, 218, 221, 221, 220, 218, 214, 209, 203, 195, 187,
        177, 167, 156, 145, 133, 121, 110, 98, 87, 77, 68, 59, 52, 45, 40, 37,
        34, 34, 34, 37, 40, 45, 52, 59, 68, 77, 87, 98, 110, 121, 133, 145,
        156, 167, 177, 187, 196, 203, 209, 214, 218, 220, 221, 221, 218, 215, 210, 204,
        196, 188, 178, 168, 157, 146, 134, 123, 111, 99, 89, 78, 69, 60, 52, 46,
        41, 37, 35, 34, 34, 36, 40, 45, 51, 58, 67, 76, 86, 97, 108, 120,
        132, 143, 155, 166, 176, 186, 195, 202, 209, 214, 218, 220, 221, 221, 219, 215,
        211, 205, 197, 189, 180, 169, 159, 147, 136, 124, 112, 101, 90, 79, 70, 61,
        53, 47, 41, 37, 35, 34, 34, 36, 39, 44, 50, 57, 66, 75, 85, 96,
        107, 125, 128, 132, 136, 139, 142, 145, 148, 151, 153, 154, 156, 156, 157, 157,
        156, 155, 154, 152, 150, 147, 144, 141, 138, 134, 130, 127, 123, 120, 116, 113,
        110, 107, 105, 102, 101, 99, 99, 98, 98, 99, 100, 101, 103, 105, 108, 111,
        114, 117, 121, 124, 128, 132, 135, 139, 142, 145, 148, 150, 152, 154, 155, 156,
        157, 157, 156, 155, 154, 152, 150, 147, 144, 141, 138, 134, 131, 127, 124, 120,
        116, 113, 110, 107, 105, 103, 101, 100, 99, 98, 98, 99, 100, 101, 103, 105,
        108, 110, 113, 117, 120, 124, 128, 131, 135, 138, 142, 145, 148, 150, 152, 154,
        155, 156, 157, 157, 156, 155, 154, 152, 150, 148, 145, 142, 138, 135, 131, 128,
        124, 120, 117, 114, 110, 108, 105, 103, 101, 100, 99, 98, 98, 99, 100, 101,
        103, 105, 107, 110, 113, 116, 120, 124, 127, 131, 134, 138, 141, 144, 147, 150,
        152, 154, 155, 156, 157, 157, 156, 156, 154, 152, 150, 148, 145, 142, 139, 135,
        132, 128, 124, 121, 117, 114, 111, 108, 105, 103, 101, 100, 99, 98, 98, 99,
        99, 101, 102, 104, 107, 110, 113, 116, 120, 123, 127, 130, 134, 138, 141, 144,
        147, 150, 152, 154, 155, 156, 157, 157, 156, 156, 154, 153, 151, 148, 145, 142,
        139, 136, 132, 128, 125, 121, 118, 114, 111, 108, 106, 103, 101, 100, 99, 98,
        98, 99, 99, 101, 102, 104, 107, 109, 112, 116, 119, 123, 126, 130, 134, 137,
        141, 144, 147, 149, 152, 153, 155, 156, 157, 157, 156, 156, 155, 153, 151, 149,
        146, 143, 140, 136, 133, 129, 125, 122, 118, 115, 111, 109, 106, 104, 102, 100,
        99, 98, 98, 98, 99, 100, 102, 104, 106, 109, 112, 115, 119, 122, 126, 130,
        133, 137, 140, 143, 146, 149, 151, 153, 155, 156, 157, 157, 157, 156, 155, 153,
        151, 149, 146, 143, 140, 136, 133, 129, 126, 122, 118, 115, 112, 109, 106, 104,
        102, 100, 99, 98, 98, 98, 99, 100, 102, 104, 106, 109, 112, 115, 118, 122,
        126, 129, 133, 136, 140, 143, 146, 149, 151, 153, 155, 156, 157, 157, 157, 156,
        155, 153, 151, 149, 146, 143, 140, 137, 133, 130, 126, 122, 119, 115, 112, 109,
        106, 104, 102, 100, 99, 99, 98, 98, 99, 100, 102, 103, 106, 108, 111, 115,
        118, 121, 125, 129, 132, 136, 139, 143, 146, 148, 151, 153, 154, 156, 156, 157,
        157, 156, 155, 154, 152, 149, 147, 144, 141, 137, 134, 130, 126, 123, 119, 116,
        113, 109, 107, 104, 102, 101, 99, 99, 98, 98, 99, 100, 101, 103, 106, 108,
        111, 114, 118, 121, 125, 128, 132, 136, 139, 142, 145, 148, 151, 153, 154, 156,
        156, 157, 157, 156, 155, 154, 152, 150, 147, 144, 141, 138, 134, 131, 127, 123,
        120, 116, 113, 110, 107, 105, 102, 101, 99, 99, 98, 98, 99, 100, 101, 103,
        105, 108, 111, 114, 117, 121, 124, 128, 132, 135, 139, 142, 145, 148, 150, 152,
        154, 155, 156, 157, 157, 156, 155, 154, 152, 150, 147, 145, 141, 138, 135, 131,
        127, 124, 120, 117, 113, 110, 107, 105, 103, 101, 100, 99, 98, 98, 99, 100,
        101, 103, 105, 107, 110, 113, 117, 120, 124, 128, 131, 135, 138, 142, 145, 148,
        150, 152, 154, 155, 156, 157, 157, 156, 155, 154, 152, 150, 148, 145, 142, 138,
        135, 131, 128, 124, 120, 117, 114, 110, 108, 105, 103, 101, 100, 99, 98, 98,
        99, 100, 101, 103, 105, 107, 110, 113, 116, 120, 123, 127, 131, 134, 138, 141,
        144, 147, 150, 152, 154, 155, 156, 157, 157, 156, 156, 154, 153, 150, 148, 145,
        142, 139, 135, 132, 128, 124, 121, 117, 114, 111, 108, 105, 103, 101, 100, 99,
        98, 98, 99, 99, 101, 102, 104, 107, 110, 113, 116, 119, 123, 127, 130, 134,
        137, 141, 144, 147, 149, 152, 154, 155, 156, 157, 157, 156, 156, 154, 153, 151,
        148, 146, 142, 139, 136, 132, 129, 125, 121, 118, 114, 111, 108, 106, 103, 101,
        100, 99, 98, 98, 99, 99, 101, 102, 104, 107, 109, 112, 116, 119, 123, 126,
        130, 134, 137, 140, 144, 147, 149, 152, 153, 155, 156, 157, 157, 157, 156, 155,
        153, 151, 149, 146, 143, 140, 136, 133, 129, 125, 122, 118, 115, 112, 109, 106,
        104, 102, 100, 99, 98, 98, 98, 99, 100, 102, 104, 106, 109, 112, 115, 119,
        122, 126, 130, 133, 137, 140, 143, 146, 149, 151, 153, 155, 156, 157, 157, 157,
        156, 155, 153, 151, 149, 146, 143, 140, 137, 133, 129, 126, 122, 119, 115, 112,
        109, 106, 104, 102, 100, 99, 98, 98, 98, 99, 100, 102, 104, 106, 109, 112,
        115, 118, 122, 125, 129, 133, 136, 140, 143, 146, 149, 151, 153, 155, 156, 157,
        157, 157, 156, 155, 153, 151, 149, 146, 144, 140, 137, 133, 130, 126, 122, 119,
        115, 112, 109, 107, 104, 102, 100, 99, 99, 98, 98, 99, 100, 102, 103, 106,
        108, 111, 114, 118, 121, 125, 129, 132, 136, 139, 143, 146, 148, 151, 153, 154,
        156, 156, 157, 157, 156, 155, 154, 152, 149, 147, 144, 141, 137, 134, 130, 127,
        123, 119, 116, 113, 110, 107, 104, 102, 101, 99, 99, 98, 98, 99, 100, 101,
        103, 105, 108, 111, 114, 117, 121, 125, 128, 132, 135, 139, 142, 145, 148, 151,
        153, 154, 156, 156, 157, 157, 156, 155, 154, 152, 150, 147, 144, 141, 138, 134,
        131, 127, 123, 120, 116, 113, 110, 107, 105, 103, 101, 100, 99, 98, 98, 99,
        100, 101, 103, 105, 108, 111, 114, 117, 121, 124, 128, 131, 135, 139, 142, 145,
        148, 150, 152, 154, 155, 156, 157, 157, 156, 155, 154, 152, 150, 147, 145, 141,
        138, 135, 131, 127, 124, 120, 117, 113, 110, 107, 105, 103, 101, 100, 99, 98,
        98, 99, 100, 101, 103, 105, 107, 110, 113, 117, 120, 124, 127, 131, 135, 138,
        142, 145, 147, 150, 152, 154, 155, 156, 157, 157, 156, 155, 154, 152, 150, 148,
        145, 142, 139, 135, 131, 128, 124, 121, 117, 114, 111, 108, 105, 103, 101, 100,
        99, 98, 98, 99, 100, 101, 103, 105, 107, 110, 113, 116, 120, 123, 127, 131,
        134, 138, 141, 144, 147, 150, 152, 154, 155, 156, 157, 157, 156, 156, 154, 153,
        150, 148, 145, 142, 139, 135, 132, 128, 125, 121, 117, 114, 111, 108, 105, 103,
        101, 100, 99, 98, 98, 99, 99, 101, 102, 104, 107, 110, 113, 116, 119, 123,
        127, 130, 134, 137, 141, 144, 147, 149, 152, 154, 155, 156, 157, 157, 156, 156,
        154, 153, 151, 148, 146, 143, 139, 136, 132, 129, 125, 121, 118, 114, 111, 108,
        106, 103, 102, 100, 99, 98, 98, 99, 99, 100, 102, 104, 107, 109, 112, 116,
        119, 123, 126, 130, 133, 137, 140, 144, 147, 149, 151, 153, 155, 156, 157, 157,
        157, 156, 155, 153, 151, 149, 146, 143, 140, 136, 133, 129, 125, 122, 118, 115,
        112, 109, 106, 104, 102, 100, 99, 98, 98, 98, 99, 100, 102, 104, 106, 109,
        112, 115, 119, 122, 126, 129, 133, 137, 140, 143, 146, 149, 151, 153, 155, 156,
        157, 157, 157, 156, 155, 153, 151, 149, 146, 143, 140, 137, 133, 129, 126, 122,
        119, 115, 112, 109, 106, 104, 102, 100, 99, 98, 98, 98, 99, 100, 102, 104,
        106, 109, 112, 115, 118, 122, 125, 129, 133, 136, 140, 143, 146, 149, 151, 153,
        155, 156, 157, 157, 157, 156, 155, 153, 151, 149, 147, 144, 140, 137, 134, 130,
        126, 123, 119, 116, 112, 109, 107, 104, 102, 100, 99, 99, 98, 98, 99, 100,
        102, 103, 106, 108, 111, 114, 118, 121, 125, 129, 132, 136, 139, 143, 146, 148,
        151, 153, 154, 156, 156, 157, 157, 156, 155, 154, 152, 149, 147, 144, 141, 137,
        134, 130, 127, 123, 119, 116, 113, 110, 107, 104, 102, 101, 99, 99, 98, 98,
        99, 100, 101, 103, 105, 108, 111, 114, 117, 121, 125, 128, 132, 135, 139, 142,
        145, 148, 150, 153, 154, 156, 156, 157, 157, 156, 155, 154, 152, 150, 147, 144,
        141, 138, 134, 131, 127, 123, 120, 116, 113, 110, 107, 105, 103, 101, 100, 99,
        98, 98, 99, 100, 101, 103, 105, 108, 111, 114, 117, 120, 124, 128, 131, 135,
        138, 142, 145, 148, 150, 152, 154, 155, 156, 157, 157, 156, 155, 154, 152, 150,
        147, 145, 142, 138, 135, 131, 127, 124, 120, 117, 113, 110, 107, 105, 103, 101,
        100, 99, 98, 98, 99, 100, 101, 103, 105, 107, 110, 113, 117, 120, 124, 127,
        131, 135, 138, 141, 145, 147, 150, 152, 154, 155, 156, 157, 157, 156, 155, 154,
        152, 150, 148, 145, 142, 139, 135, 132, 128, 124, 121, 117, 114, 111, 108, 105,
        103, 101, 100, 99, 98, 98, 99, 99, 101, 102, 105, 107, 110, 113, 116, 120,
        123, 127, 131, 134, 138, 141, 144, 147, 150, 152, 154, 155, 156, 157, 157, 156,
        156, 154, 153, 151, 148, 145, 142, 139, 136, 132, 128, 125, 121, 117, 114, 111,
        108, 105, 103, 101, 100, 99, 98, 98, 99, 99, 101, 102, 104, 107, 110, 113,
        116, 119, 123, 127, 130, 134, 137, 141, 144, 147, 149, 152, 154, 155, 156, 157,
        157, 156, 156, 154, 153, 151, 148, 146, 143, 139, 136, 132, 129, 125, 121, 118,
        114, 111, 108, 106, 103, 102, 100, 99, 98, 98, 99, 99, 100, 102, 104, 106,
        109, 112, 115, 119, 122, 126, 130, 133, 137, 140, 144, 146, 149, 151, 153, 155,
        156, 157, 157, 157, 156, 155, 153, 151, 149, 146, 143, 140, 136, 133, 129, 125,
        122, 118, 115, 112, 109, 106, 104, 102, 100, 99, 98, 98, 98, 99, 100, 102,
        104, 106, 109, 112, 115, 118, 122,
    };

    // stereo16.wav: 13272 Byte
    const unsigned char kStereo16Wav[] = {
        82, 73, 70, 70, 208, 51, 0, 0, 87, 65, 86, 69, 102, 109, 116, 32,
        16, 0, 0, 0, 1, 0, 2, 0, 34, 86, 0, 0, 136, 88, 1, 0,
        4, 0, 16, 0, 100, 97, 116, 97, 172, 51, 0, 0, 0, 0, 0, 0,
        185, 11, 71, 244, 67, 23, 189, 232, 111, 34, 145, 221, 18, 45, 238, 210,
        255, 54, 1, 201, 15, 64, 241, 191, 29, 72, 227, 183, 10, 79, 246, 176,
        185, 84, 71, 171, 20, 89, 236, 166, 8, 92, 248, 163, 139, 93, 117, 162,
        150, 93, 106, 162, 40, 92, 216, 163, 72, 89, 184, 166, 2, 85, 254, 170,
        101, 79, 155, 176, 138, 72, 118, 183, 139, 64, 117, 191, 137, 55, 119, 200,
        167, 45, 89, 210, 14, 35, 242, 220, 232, 23, 24, 232, 98, 12, 158, 243,
        170, 0, 86, 255, 241, 244, 15, 11, 99, 233, 157, 22, 48, 222, 208, 33,
        133, 211, 123, 44, 140, 201, 116, 54, 111, 192, 145, 63, 80, 184, 176, 71,
        82, 177, 174, 78, 145, 171, 111, 84, 34, 167, 222, 88, 25, 164, 231, 91,
        129, 162, 127, 93, 97, 162, 159, 93, 185, 163, 71, 92, 132, 166, 124, 89,
        183, 170, 73, 85, 64, 176, 192, 79, 10, 183, 246, 72, 249, 190, 7, 65,
        238, 199, 18, 56, 196, 209, 60, 46, 83, 220, 173, 35, 115, 231, 141, 24,
        244, 242, 12, 13, 171, 254, 85, 1, 101, 10, 155, 245, 247, 21, 9, 234,
        48, 33, 208, 222, 229, 43, 27, 212, 232, 53, 24, 202, 19, 63, 237, 192,
        65, 71, 191, 184, 80, 78, 176, 177, 36, 84, 220, 171, 167, 88, 89, 167,
        197, 91, 59, 164, 114, 93, 142, 162, 167, 93, 89, 162, 100, 92, 156, 163,
        174, 89, 82, 166, 144, 85, 112, 170, 25, 80, 231, 175, 97, 73, 159, 182,
        129, 65, 127, 190, 155, 56, 101, 199, 209, 46, 47, 209, 74, 36, 182, 219,
        50, 25, 206, 230, 181, 13, 75, 242, 0, 2, 0, 254, 69, 246, 187, 9,
        175, 234, 81, 21, 112, 223, 144, 32, 179, 212, 77, 43, 164, 202, 92, 53,
        108, 193, 148, 62, 47, 185, 209, 70, 14, 178, 242, 77, 39, 172, 217, 83,
        145, 167, 111, 88, 95, 164, 161, 91, 157, 162, 99, 93, 82, 162, 174, 93,
        127, 163, 129, 92, 32, 166, 224, 89, 43, 170, 213, 85, 143, 175, 113, 80,
        53, 182, 203, 73, 5, 190, 251, 65, 221, 198, 35, 57, 155, 208, 101, 47,
        24, 219, 232, 36, 41, 230, 215, 25, 162, 241, 94, 14, 85, 253, 171, 2,
        17, 9, 239, 246, 170, 20, 86, 235, 240, 31, 16, 224, 181, 42, 75, 213,
        207, 52, 49, 203, 21, 62, 235, 193, 97, 70, 159, 185, 146, 77, 110, 178,
        140, 83, 116, 172, 53, 88, 203, 167, 124, 91, 132, 164, 84, 93, 172, 162,
        180, 93, 76, 162, 156, 92, 100, 163, 16, 90, 240, 165, 25, 86, 231, 169,
        201, 80, 55, 175, 52, 74, 204, 181, 116, 66, 140, 189, 170, 57, 86, 198,
        248, 47, 8, 208, 133, 37, 123, 218, 123, 26, 133, 229, 7, 15, 249, 240,
        86, 3, 170, 252, 153, 247, 103, 8, 253, 235, 3, 20, 177, 224, 79, 31,
        227, 213, 29, 42, 190, 203, 66, 52, 108, 194, 148, 61, 17, 186, 239, 69,
        206, 178, 50, 77, 195, 172, 61, 83, 5, 168, 251, 87, 170, 164, 86, 91,
        189, 162, 67, 93, 71, 162, 185, 93, 74, 163, 182, 92, 194, 165, 62, 90,
        164, 169, 92, 86, 225, 174, 31, 81, 100, 181, 156, 74, 20, 189, 236, 66,
        208, 197, 48, 58, 118, 207, 138, 48, 223, 217, 33, 38, 225, 228, 31, 27,
        81, 240, 175, 15, 255, 251, 1, 4, 189, 7, 67, 248, 92, 19, 164, 236,
        174, 30, 82, 225, 132, 41, 124, 214, 179, 51, 77, 204, 19, 61, 237, 194,
        125, 69, 131, 186, 208, 76, 48, 179, 238, 82, 18, 173, 191, 87, 65, 168,
        47, 91, 209, 164, 49, 93, 207, 162, 188, 93, 68, 162, 207, 92, 49, 163,
        108, 90, 148, 165, 158, 86, 98, 169, 116, 81, 140, 174, 3, 75, 253, 180,
        100, 67, 156, 188, 182, 58, 74, 197, 28, 49, 228, 206, 189, 38, 67, 217,
        194, 27, 62, 228, 88, 16, 168, 239, 172, 4, 84, 251, 238, 248, 18, 7,
        75, 237, 181, 18, 244, 225, 12, 30, 22, 215, 234, 40, 220, 204, 36, 51,
        111, 195, 145, 60, 246, 186, 10, 69, 146, 179, 110, 76, 98, 173, 158, 82,
        126, 168, 130, 87, 249, 164, 7, 91, 226, 162, 30, 93, 66, 162, 190, 93,
        26, 163, 230, 92, 103, 165, 153, 90, 33, 169, 223, 86, 56, 174, 200, 81,
        151, 180, 105, 75, 38, 188, 218, 67, 197, 196, 59, 59, 82, 206, 174, 49,
        167, 216, 89, 39, 155, 227, 101, 28, 0, 239, 0, 17, 169, 250, 87, 5,
        104, 6, 152, 249, 13, 18, 243, 237, 106, 29, 150, 226, 80, 40, 176, 215,
        149, 50, 107, 205, 14, 60, 242, 195, 150, 68, 106, 187, 10, 76, 246, 179,
        77, 82, 179, 173, 68, 87, 188, 168, 222, 90, 34, 165, 9, 93, 247, 162,
        191, 93, 65, 162, 253, 92, 3, 163, 196, 90, 60, 165, 31, 87, 225, 168,
        27, 82, 229, 173, 206, 75, 50, 180, 80, 68, 176, 187, 191, 59, 65, 196,
        62, 50, 194, 205, 243, 39, 13, 216, 8, 29, 248, 226, 168, 17, 88, 238,
        1, 6, 255, 249, 67, 250, 189, 5, 155, 238, 101, 17, 57, 227, 199, 28,
        74, 216, 182, 39, 252, 205, 4, 50, 118, 196, 138, 59, 223, 187, 33, 68,
        90, 180, 166, 75, 6, 174, 250, 81, 251, 168, 5, 87, 77, 165, 179, 90,
        12, 163, 244, 92, 65, 162, 191, 93, 238, 162, 18, 93, 18, 165, 238, 90,
        163, 168, 93, 87, 147, 173, 109, 82, 206, 179, 50, 76, 60, 187, 196, 68,
        190, 195, 66, 60, 50, 205, 206, 50, 114, 215, 142, 40, 85, 226, 171, 29,
        176, 237, 80, 18, 84, 249, 172, 6, 18, 5, 238, 250, 189, 16, 67, 239,
        36, 28, 220, 227, 26, 39, 230, 216, 116, 49, 140, 206, 6, 59, 250, 196,
        171, 67, 85, 188, 64, 75, 192, 180, 167, 81, 89, 174, 197, 86, 59, 169,
        135, 90, 121, 165, 221, 92, 35, 163, 190, 93, 66, 162, 38, 93, 218, 162,
        23, 91, 233, 164, 155, 87, 101, 168, 190, 82, 66, 173, 149, 76, 107, 179,
        56, 69, 200, 186, 197, 60, 59, 195, 94, 51, 162, 204, 40, 41, 216, 214,
        77, 30, 179, 225, 248, 18, 8, 237, 86, 7, 170, 248, 153, 251, 103, 4,
        235, 239, 21, 16, 127, 228, 129, 27, 129, 217, 127, 38, 30, 207, 226, 48,
        127, 197, 129, 58, 204, 188, 52, 67, 38, 181, 218, 74, 174, 174, 82, 81,
        124, 169, 132, 86, 166, 165, 90, 90, 59, 163, 197, 92, 69, 162, 187, 93,
        200, 162, 56, 93, 193, 164, 63, 91, 41, 168, 215, 87, 242, 172, 14, 83,
        9, 179, 247, 76, 85, 186, 171, 69, 185, 194, 71, 61, 20, 204, 236, 51,
        63, 214, 193, 41, 18, 225, 238, 30, 97, 236, 159, 19, 255, 247, 1, 8,
        189, 3, 67, 252, 108, 15, 148, 240, 221, 26, 35, 229, 227, 37, 29, 218,
        80, 48, 176, 207, 251, 57, 5, 198, 189, 66, 67, 189, 114, 74, 142, 181,
        253, 80, 3, 175, 66, 86, 190, 169, 44, 90, 212, 165, 172, 92, 84, 163,
        183, 93, 73, 162, 74, 93, 182, 162, 102, 91, 154, 164, 18, 88, 238, 167,
        93, 83, 163, 172, 88, 77, 168, 178, 29, 70, 227, 185, 200, 61, 56, 194,
        122, 52, 134, 203, 90, 42, 166, 213, 143, 31, 113, 224, 70, 20, 186, 235,
        171, 8, 85, 247, 238, 252, 18, 3, 61, 241, 195, 14, 199, 229, 57, 26,
        186, 218, 70, 37, 67, 208, 189, 47, 140, 198, 116, 57, 188, 189, 68, 66,
        246, 181, 10, 74, 90, 175, 166, 80, 2, 170, 254, 85, 3, 166, 253, 89,
        111, 163, 145, 92, 78, 162, 178, 93, 166, 162, 90, 93, 117, 164, 139, 91,
        180, 167, 76, 88, 86, 172, 170, 83, 72, 178, 184, 77, 114, 185, 142, 70,
        184, 193, 72, 62, 248, 202, 8, 53, 14, 213, 242, 42, 208, 223, 48, 32,
        19, 235, 237, 20, 171, 246, 85, 9, 103, 2, 153, 253, 26, 14, 230, 241,
        149, 25, 107, 230, 169, 36, 87, 219, 41, 47, 215, 208, 236, 56, 20, 199,
        203, 65, 53, 190, 160, 73, 96, 182, 78, 80, 178, 175, 185, 85, 71, 170,
        204, 89, 52, 166, 118, 92, 138, 163, 172, 93, 84, 162, 105, 93, 151, 162,
        175, 91, 81, 164, 133, 88, 123, 167, 247, 83, 9, 172, 24, 78, 232, 177,
        254, 70, 2, 185, 199, 62, 57, 193, 148, 53, 108, 202, 138, 43, 118, 212,
        208, 32, 48, 223, 147, 21, 109, 234, 255, 9, 1, 246, 68, 254, 188, 1,
        143, 242, 113, 13, 16, 231, 240, 24, 245, 219, 11, 36, 107, 209, 149, 46,
        156, 199, 100, 56, 176, 190, 80, 65, 202, 182, 54, 73, 10, 176, 246, 79,
        140, 170, 116, 85, 102, 166, 154, 89, 167, 163, 89, 92, 92, 162, 164, 93,
        137, 162, 119, 93, 45, 164, 211, 91, 67, 167, 189, 88, 189, 171, 67, 84,
        138, 177, 118, 78, 147, 184, 109, 71, 186, 192, 70, 63, 224, 201, 32, 54,
        223, 211, 33, 44, 144, 222, 112, 33, 199, 233, 57, 22, 87, 245, 169, 10,
        17, 1, 239, 254, 200, 12, 56, 243, 75, 24, 181, 231, 109, 35, 147, 220,
        1, 46, 255, 209, 219, 55, 37, 200, 213, 64, 43, 191, 203, 72, 53, 183,
        156, 79, 100, 176, 45, 85, 211, 170, 103, 89, 153, 166, 59, 92, 197, 163,
        155, 93, 101, 162, 132, 93, 124, 162, 244, 91, 12, 164, 243, 88, 13, 167,
        141, 84, 115, 171, 211, 78, 45, 177, 220, 71, 36, 184, 196, 63, 60, 192,
        171, 54, 85, 201, 184, 44, 72, 211, 16, 34, 240, 221, 223, 22, 33, 233,
        83, 11, 173, 244, 154, 255, 102, 0, 226, 243, 30, 12, 90, 232, 166, 23,
        49, 221, 207, 34, 149, 210, 107, 45, 174, 200, 82, 55, 167, 191, 89, 64,
        161, 183, 95, 72, 191, 176, 65, 79, 27, 171, 229, 84, 205, 166, 51, 89,
        228, 163, 28, 92, 111, 162, 145, 93, 113, 162, 143, 93, 235, 163, 21, 92,
        215, 166, 41, 89, 42, 171, 214, 84, 209, 176, 47, 79, 183, 183, 73, 72,
        191, 191, 65, 64, 202, 200, 54, 55, 178, 210, 78, 45, 81, 221, 175, 34,
        123, 232, 133, 23, 3, 244, 253, 11, 188, 255, 68, 0, 117, 11, 139, 244,
        1, 23, 255, 232, 48, 34, 208, 221, 214, 44, 42, 211, 199, 54, 57, 201,
        221, 63, 35, 192, 241, 71, 15, 184, 229, 78, 27, 177, 156, 84, 100, 171,
        254, 88, 2, 167, 251, 91, 5, 164, 134, 93, 122, 162, 154, 93, 102, 162,
        53, 92, 203, 163, 93, 89, 163, 166, 30, 85, 226, 170, 138, 79, 118, 176,
        181, 72, 75, 183, 189, 64, 67, 191, 192, 55, 64, 200, 227, 45, 29, 210,
        78, 35, 178, 220, 42, 24, 214, 231, 166, 12, 90, 243, 239, 0, 17, 255,
        53, 245, 203, 10, 165, 233, 91, 22, 112, 222, 144, 33, 193, 211, 63, 44,
        196, 201, 60, 54, 161, 192, 95, 63, 125, 184, 131, 71, 120, 177, 136, 78,
        174, 171, 82, 84, 56, 167, 200, 88, 39, 164, 217, 91, 134, 162, 122, 93,
        93, 162, 163, 93, 173, 163, 83, 92, 112, 166, 144, 89, 154, 170, 102, 85,
        28, 176, 228, 79, 223, 182, 33, 73, 200, 190, 56, 65, 183, 199, 73, 56,
        136, 209, 120, 46, 20, 220, 236, 35, 49, 231, 207, 24, 177, 242, 79, 13,
        102, 254, 154, 1, 33, 10, 223, 245, 180, 21, 76, 234, 240, 32, 16, 223,
        168, 43, 88, 212, 176, 53, 80, 202, 225, 62, 31, 193, 20, 71, 236, 184,
        42, 78, 214, 177, 6, 84, 250, 171, 144, 88, 112, 167, 183, 91, 73, 164,
        108, 93, 148, 162, 170, 93, 86, 162, 112, 92, 144, 163, 194, 89, 62, 166,
        172, 85, 84, 170, 61, 80, 195, 175, 139, 73, 117, 182, 178, 65, 78, 190,
        209, 56, 47, 199, 12, 47, 244, 208, 137, 36, 119, 219, 116, 25, 140, 230,
        249, 13, 7, 242, 69, 2, 187, 253, 137, 246, 119, 9, 242, 234, 14, 21,
        176, 223, 80, 32, 239, 212, 17, 43, 220, 202, 36, 53, 159, 193, 97, 62,
        92, 185, 164, 70, 52, 178, 204, 77, 70, 172, 186, 83, 168, 167, 88, 88,
        109, 164, 147, 91, 163, 162, 93, 93, 79, 162, 177, 93, 116, 163, 140, 92,
        13, 166, 243, 89, 16, 170, 240, 85, 107, 175, 149, 80, 11, 182, 245, 73,
        212, 189, 44, 66, 167, 198, 89, 57, 97, 208, 159, 47, 217, 218, 39, 37,
        231, 229, 25, 26, 95, 241, 161, 14, 16, 253, 240, 2, 205, 8, 51, 247,
        103, 20, 153, 235, 176, 31, 80, 224, 120, 42, 136, 213, 151, 52, 105, 203,
        225, 61, 31, 194, 52, 70, 204, 185, 108, 77, 148, 178, 108, 83, 148, 172,
        30, 88, 226, 167, 109, 91, 147, 164, 77, 93, 179, 162, 182, 93, 74, 162,
        167, 92, 89, 163, 34, 90, 222, 165, 52, 86, 204, 169, 235, 80, 21, 175,
        93, 74, 163, 181, 164, 66, 92, 189, 224, 57, 32, 198, 50, 48, 206, 207,
        195, 37, 61, 218, 189, 26, 67, 229, 74, 15, 182, 240, 155, 3, 101, 252,
        221, 247, 35, 8, 64, 236, 192, 19, 242, 224, 14, 31, 32, 214, 224, 41,
        247, 203, 9, 52, 159, 194, 97, 61, 62, 186, 194, 69, 245, 178, 11, 77,
        226, 172, 30, 83, 29, 168, 227, 87, 185, 164, 71, 91, 196, 162, 60, 93,
        70, 162, 186, 93, 64, 163, 192, 92, 175, 165, 81, 90, 137, 169, 119, 86,
        191, 174, 65, 81, 59, 181, 197, 74, 228, 188, 28, 67, 154, 197, 102, 58,
        59, 207, 197, 48, 160, 217, 96, 38, 160, 228, 96, 27, 13, 240, 243, 15,
        187, 251, 69, 4, 120, 7, 136, 248, 25, 19, 231, 236, 109, 30, 147, 225,
        70, 41, 186, 214, 122, 51, 134, 204, 223, 60, 33, 195, 79, 69, 177, 186,
        169, 76, 87, 179, 206, 82, 50, 173, 167, 87, 89, 168, 31, 91, 225, 164,
        42, 93, 214, 162, 189, 93, 67, 162, 216, 92, 40, 163, 126, 90, 130, 165,
        184, 86, 72, 169, 150, 81, 106, 174, 44, 75, 212, 180, 147, 67, 109, 188,
        235, 58, 21, 197, 86, 49, 170, 206, 251, 38, 5, 217, 4, 28, 252, 227,
        155, 16, 101, 239, 240, 4, 16, 251, 50, 249, 206, 6, 143, 237, 113, 18,
        53, 226, 203, 29, 83, 215, 173, 40, 21, 205, 235, 50, 163, 195, 93, 60,
        36, 187, 220, 68, 186, 179, 70, 76, 130, 173, 126, 82, 150, 168, 106, 87,
        9, 165, 247, 90, 234, 162, 22, 93, 65, 162, 191, 93, 17, 163, 239, 92,
        86, 165, 170, 90, 7, 169, 249, 86, 22, 174, 234, 81, 111, 180, 145, 75,
        247, 187, 9, 68, 144, 196, 112, 59, 24, 206, 232, 49, 105, 216, 151, 39,
        89, 227, 167, 28, 189, 238, 67, 17, 101, 250, 155, 5, 35, 6, 221, 249,
        202, 17, 54, 238, 41, 29, 215, 226, 18, 40, 238, 215, 91, 50, 165, 205,
        217, 59, 39, 196, 103, 68, 153, 187, 226, 75, 30, 180, 44, 82, 212, 173,
        43, 87, 213, 168, 205, 90, 51, 165, 1, 93, 255, 162, 191, 93, 65, 162,
        5, 93, 251, 162, 213, 90, 43, 165, 56, 87, 200, 168, 60, 82, 196, 173,
        246, 75, 10, 180, 126, 68, 130, 187, 244, 59, 12, 196, 120, 50, 136, 205,
        49, 40, 207, 215, 73, 29, 183, 226, 235, 17, 21, 238, 69, 6, 187, 249,
        135, 250, 121, 5, 222, 238, 34, 17, 122, 227, 134, 28, 136, 216, 120, 39,
        53, 206, 203, 49, 171, 196, 85, 59, 14, 188, 242, 67, 131, 180, 125, 75,
        39, 174, 217, 81, 20, 169, 236, 86, 94, 165, 162, 90, 21, 163, 235, 92,
        65, 162, 191, 93, 230, 162, 26, 93, 1, 165, 255, 90, 138, 168, 118, 87,
        114, 173, 142, 82, 166, 179, 90, 76, 13, 187, 243, 68, 137, 195, 119, 60,
        248, 204, 8, 51, 52, 215, 204, 40, 21, 226, 235, 29, 109, 237, 147, 18,
        16, 249, 240, 6, 206, 4, 50, 251, 122, 16, 134, 239, 227, 27, 29, 228,
        220, 38, 36, 217, 57, 49, 199, 206, 209, 58, 47, 197, 124, 67, 132, 188,
        23, 75, 233, 180, 133, 81, 123, 174, 171, 86, 85, 169, 117, 90, 139, 165,
        212, 92, 44, 163, 189, 93, 67, 162, 45, 93, 211, 162, 39, 91, 217, 164,
        179, 87, 77, 168, 222, 82, 34, 173, 188, 76, 68, 179, 102, 69, 154, 186,
        249, 60, 7, 195, 151, 51, 105, 204, 101, 41, 155, 214, 141, 30, 115, 225,
        58, 19, 198, 236, 154, 7, 102, 248, 221, 251, 35, 4, 47, 240, 209, 15,
        192, 228, 64, 27, 192, 217, 64, 38, 88, 207, 168, 48, 181, 197, 75, 58,
        252, 188, 4, 67, 80, 181, 176, 74, 208, 174, 48, 81, 150, 169, 106, 86,
        184, 165, 72, 90, 69, 163, 187, 92, 70, 162, 186, 93, 192, 162, 64, 93,
        177, 164, 79, 91, 17, 168, 239, 87, 210, 172, 46, 83, 226, 178, 30, 77,
        39, 186, 217, 69, 134, 194, 122, 61, 219, 203, 37, 52, 2, 214, 254, 41,
        209, 224, 47, 31, 30, 236, 226, 19, 187, 247, 69, 8, 120, 3, 136, 252,
        41, 15, 215, 240, 156, 26, 100, 229, 164, 37, 92, 218, 21, 48, 235, 207,
        197, 57, 59, 198, 140, 66, 116, 189, 73, 74, 183, 181, 218, 80, 38, 175,
        39, 86, 217, 169, 25, 90, 231, 165, 161, 92, 95, 163, 181, 93, 75, 162,
        81, 93, 175, 162, 117, 91, 139, 164, 42, 88, 214, 167, 124, 83, 132, 172,
        127, 77, 129, 178, 74, 70, 182, 185, 251, 61, 5, 194, 179, 52, 77, 203,
        151, 42, 105, 213, 208, 31, 48, 224, 137, 20, 119, 235, 239, 8, 17, 247,
        51, 253, 205, 2, 128, 241, 128, 14, 8, 230, 248, 25, 249, 218, 7, 37,
        126, 208, 130, 47, 194, 198, 62, 57, 236, 189, 20, 66, 32, 182, 224, 73,
        125, 175, 131, 80, 29, 170, 227, 85, 23, 166, 233, 89, 122, 163, 134, 92,
        80, 162, 176, 93, 160, 162, 96, 93, 102, 164, 154, 91, 157, 167, 99, 88,
        55, 172, 201, 83, 33, 178, 223, 77, 69, 185, 187, 70, 133, 193, 123, 62,
        192, 202, 64, 53, 209, 212, 47, 43, 144, 223, 112, 32, 209, 234, 47, 21,
        103, 246, 153, 9, 35, 2, 221, 253, 215, 13, 41, 242, 83, 25, 173, 230,
        106, 36, 150, 219, 238, 46, 18, 209, 182, 56, 74, 199, 154, 65, 102, 190,
        118, 73, 138, 182, 43, 80, 213, 175, 158, 85, 98, 170, 184, 89, 72, 166,
        106, 92, 150, 163, 169, 93, 87, 162, 111, 93, 145, 162, 190, 91, 66, 164,
        156, 88, 100, 167, 21, 84, 235, 171, 61, 78, 195, 177, 43, 71, 213, 184,
        250, 62, 6, 193, 204, 53, 52, 202, 198, 43, 58, 212, 17, 33, 239, 222,
        214, 21, 42, 234, 67, 10, 189, 245, 136, 254, 120, 1, 210, 242, 46, 13,
        82, 231, 174, 24, 52, 220, 204, 35, 166, 209, 90, 46, 210, 199, 46, 56,
        225, 190, 31, 65, 245, 182, 11, 73, 46, 176, 210, 79, 169, 170, 87, 85,
        122, 166, 134, 89, 179, 163, 77, 92, 95, 162, 161, 93, 132, 162, 124, 93,
        32, 164, 224, 91, 45, 167, 211, 88, 160, 171, 96, 84, 101, 177, 155, 78,
        102, 184, 154, 71, 136, 192, 120, 63, 168, 201, 88, 54, 163, 211, 93, 44,
        80, 222, 176, 33, 132, 233, 124, 22, 19, 245, 237, 10, 205, 0, 51, 255,
        132, 12, 124, 243, 9, 24, 247, 231, 46, 35, 210, 220, 197, 45, 59, 210,
        164, 55, 92, 200, 164, 64, 92, 191, 160, 72, 96, 183, 120, 79, 136, 176,
        16, 85, 240, 170, 83, 89, 173, 166, 46, 92, 210, 163, 152, 93, 104, 162,
        137, 93, 119, 162, 2, 92, 254, 163, 9, 89, 247, 166, 170, 84, 86, 171,
        248, 78, 8, 177, 7, 72, 249, 183, 246, 63, 10, 192, 227, 54, 29, 201,
        244, 44, 12, 211, 80, 34, 176, 221, 34, 23, 222, 232, 151, 11, 105, 244,
        222, 255, 34, 0, 37, 244, 219, 11, 156, 232, 100, 23, 113, 221, 143, 34,
        208, 210, 48, 45, 230, 200, 26, 55, 216, 191, 40, 64, 205, 183, 51, 72,
        228, 176, 28, 79, 56, 171, 200, 84, 226, 166, 30, 89, 241, 163, 15, 92,
        115, 162, 141, 93, 108, 162, 148, 93, 222, 163, 34, 92, 194, 166, 62, 89,
        13, 171, 243, 84, 173, 176, 83, 79, 140, 183, 116, 72, 142, 191, 114, 64,
        147, 200, 109, 55, 119, 210, 137, 45, 18, 221, 238, 34, 57, 232, 199, 23,
        192, 243, 64, 12, 120, 255, 136, 0, 49, 11, 207, 244, 190, 22, 66, 233,
        240, 33, 16, 222, 153, 44, 103, 211, 144, 54, 112, 201, 171, 63, 85, 192,
        198, 71, 58, 184, 192, 78, 64, 177, 126, 84, 130, 171, 233, 88, 23, 167,
        238, 91, 18, 164, 129, 93, 127, 162, 157, 93, 99, 162, 65, 92, 191, 163,
        114, 89, 142, 166, 59, 85, 197, 170, 174, 79, 82, 176, 224, 72, 32, 183,
        238, 64, 18, 191, 247, 55, 9, 200, 31, 46, 225, 209, 141, 35, 115, 220,
        108, 24, 148, 231, 234, 12, 22, 243, 51, 1, 205, 254, 121, 245, 135, 10,
        232, 233, 24, 22, 176, 222, 80, 33, 253, 211, 3, 44, 252, 201, 4, 54,
        211, 192, 45, 63, 169, 184, 87, 71, 157, 177, 99, 78, 204, 171, 52, 84,
        78, 167, 178, 88, 52, 164, 204, 91, 139, 162, 117, 93, 90, 162, 166, 93,
        161, 163, 95, 92, 92, 166, 164, 89, 126, 170, 130, 85, 249, 175, 7, 80,
        181, 182, 75, 73, 151, 190, 105, 65, 128, 199, 128, 56, 77, 209, 179, 46,
        213, 219, 43, 36, 239, 230, 17, 25, 109, 242, 147, 13, 34, 254, 222, 1,
        221, 9, 35, 246, 114, 21, 142, 234, 176, 32, 80, 223, 108, 43, 148, 212,
        120, 53, 136, 202, 174, 62, 82, 193, 232, 70, 24, 185, 5, 78, 251, 177,
        232, 83, 24, 172, 122, 88, 134, 167, 168, 91, 88, 164, 102, 93, 154, 162,
        173, 93, 83, 162, 123, 92, 133, 163, 214, 89, 42, 166, 199, 85, 57, 170,
        96, 80, 160, 175, 182, 73, 74, 182, 227, 65, 29, 190, 8, 57, 248, 198,
        71, 47, 185, 208, 200, 36, 56, 219, 182, 25, 74, 230, 60, 14, 196, 241,
        137, 2, 119, 253, 205, 246, 51, 9, 53, 235, 203, 20, 240, 223, 16, 32,
        44, 213, 212, 42, 21, 203, 235, 52, 210, 193, 46, 62, 137, 185, 119, 70,
        91, 178, 165, 77, 101, 172, 155, 83, 191, 167, 65, 88, 124, 164, 132, 91,
        169, 162, 87, 93, 77, 162, 179, 93, 105, 163, 151, 92, 250, 165, 6, 90,
        244, 169, 12, 86, 73, 175, 183, 80, 225, 181, 31, 74, 164, 189, 92, 66,
        113, 198, 143, 57, 38, 208, 218, 47, 155, 218, 101, 37, 166, 229, 90, 26,
        27, 241, 229, 14, 204, 252, 52, 3, 137, 8, 119, 247, 36, 20, 220, 235,
        111, 31, 145, 224, 59, 42, 197, 213, 94, 52, 162, 203, 174, 61, 82, 194,
        6, 70, 250, 185, 69, 77, 187, 178, 77, 83, 179, 172, 7, 88, 249, 167,
        94, 91, 162, 164, 71, 93, 185, 162, 184, 93, 72, 162, 177, 92, 79, 163,
        53, 90, 203, 165, 79, 86, 177, 169, 14, 81, 242, 174, 135, 74, 121, 181,
        213, 66, 43, 189, 22, 58, 234, 197, 109, 48, 147, 207, 2, 38, 254, 217,
        254, 26, 2, 229, 142, 15, 114, 240, 223, 3, 33, 252, 33, 248, 223, 7,
        131, 236, 125, 19, 50, 225, 206, 30, 94, 214, 162, 41, 48, 204, 208, 51,
        211, 194, 45, 61, 108, 186, 148, 69, 28, 179, 228, 76, 2, 173, 254, 82,
        53, 168, 203, 87, 201, 164, 55, 91, 203, 162, 53, 93, 68, 162, 188, 93,
        54, 163, 202, 92, 157, 165, 99, 90, 111, 169, 145, 86, 157, 174, 99, 81,
        18, 181, 238, 74, 180, 188, 76, 67, 101, 197, 155, 58, 1, 207, 255, 48,
        98, 217, 158, 38, 94, 228, 162, 27, 202, 239, 54, 16, 118, 251, 138, 4,
        52, 7, 204, 248, 214, 18, 42, 237, 44, 30, 212, 225, 9, 41, 247, 214,
        65, 51, 191, 204, 171, 60, 85, 195, 33, 69, 223, 186, 129, 76, 127, 179,
        174, 82, 82, 173, 143, 87, 113, 168, 15, 91, 241, 164, 34, 93, 222, 162,
        190, 93, 66, 162, 226, 92, 30, 163, 144, 90, 112, 165, 210, 86, 46, 169,
        184, 81, 72, 174, 84, 75, 172, 180, 195, 67, 61, 188, 32, 59, 224, 196,
        145, 49, 111, 206, 58, 39, 198, 216, 69, 28, 187, 227, 223, 16, 33, 239,
        52, 5, 204, 250, 118, 249, 138, 6, 210, 237, 46, 18, 118, 226, 138, 29,
        145, 215, 111, 40, 79, 205, 177, 50, 216, 195, 40, 60, 83, 187, 173, 68,
        226, 179, 30, 76, 163, 173, 93, 82, 175, 168, 81, 87, 26, 165, 230, 90,
        242, 162, 14, 93, 65, 162, 191, 93, 8, 163, 248, 92, 68, 165, 188, 90,
        238, 168, 18, 87, 245, 173, 11, 82, 70, 180, 186, 75, 200, 187, 56, 68,
        91, 196, 165, 59, 223, 205, 33, 50, 43, 216, 213, 39, 24, 227, 232, 28,
        121, 238, 135, 17, 33, 250, 223, 5, 223, 5, 33, 250, 135, 17, 121, 238,
        232, 28, 24, 227, 213, 39, 43, 216, 33, 50, 223, 205, 165, 59, 91, 196,
        56, 68, 200, 187, 186, 75, 70, 180, 11, 82, 245, 173, 18, 87, 238, 168,
        188, 90, 68, 165, 248, 92, 8, 163, 191, 93, 65, 162, 14, 93, 242, 162,
        230, 90, 26, 165, 81, 87, 175, 168, 93, 82, 163, 173, 30, 76, 226, 179,
        173, 68, 83, 187, 40, 60, 216, 195, 177, 50, 79, 205, 111, 40, 145, 215,
        138, 29, 118, 226, 46, 18, 210, 237, 138, 6, 118, 249, 204, 250, 52, 5,
        33, 239, 223, 16, 187, 227, 69, 28, 198, 216, 58, 39, 111, 206, 145, 49,
        224, 196, 32, 59, 61, 188, 195, 67, 172, 180, 84, 75, 72, 174, 184, 81,
        46, 169, 210, 86, 112, 165, 144, 90, 30, 163, 226, 92, 66, 162, 190, 93,
        222, 162, 34, 93, 241, 164, 15, 91, 113, 168, 143, 87, 82, 173, 174, 82,
        127, 179, 129, 76, 223, 186, 33, 69, 85, 195, 171, 60, 191, 204, 65, 51,
        247, 214, 9, 41, 212, 225, 44, 30, 42, 237, 214, 18, 204, 248, 52, 7,
        138, 4, 118, 251, 54, 16, 202, 239, 162, 27, 94, 228, 158, 38, 98, 217,
        255, 48, 1, 207, 155, 58, 101, 197, 76, 67, 180, 188, 238, 74, 18, 181,
        99, 81, 157, 174, 145, 86, 111, 169, 99, 90, 157, 165, 202, 92, 54, 163,
        188, 93, 68, 162, 53, 93, 203, 162, 55, 91, 201, 164, 203, 87, 53, 168,
        254, 82, 2, 173, 228, 76, 28, 179, 148, 69, 108, 186, 45, 61, 211, 194,
        208, 51, 48, 204, 162, 41, 94, 214, 206, 30, 50, 225, 125, 19, 131, 236,
        223, 7, 33, 248, 33, 252, 223, 3, 114, 240, 142, 15, 2, 229, 254, 26,
        254, 217, 2, 38, 147, 207, 109, 48, 234, 197, 22, 58, 43, 189, 213, 66,
        121, 181, 135, 74, 242, 174, 14, 81, 177, 169, 79, 86, 203, 165, 53, 90,
        79, 163, 177, 92, 72, 162, 184, 93, 185, 162, 71, 93, 162, 164, 94, 91,
        249, 167, 7, 88, 179, 172, 77, 83, 187, 178, 69, 77, 250, 185, 6, 70,
        82, 194, 174, 61, 162, 203, 94, 52, 197, 213, 59, 42, 145, 224, 111, 31,
        220, 235, 36, 20, 119, 247, 137, 8, 52, 3, 204, 252, 229, 14, 27, 241,
        90, 26, 166, 229, 101, 37, 155, 218, 218, 47, 38, 208, 143, 57, 113, 198,
        92, 66, 164, 189, 31, 74, 225, 181, 183, 80, 73, 175, 12, 86, 244, 169,
        6, 90, 250, 165, 151, 92, 105, 163, 179, 93, 77, 162, 87, 93, 169, 162,
        132, 91, 124, 164, 65, 88, 191, 167, 155, 83, 101, 172, 165, 77, 91, 178,
        119, 70, 137, 185, 46, 62, 210, 193, 235, 52, 21, 203, 212, 42, 44, 213,
        16, 32, 240, 223, 203, 20, 53, 235, 51, 9, 205, 246, 119, 253, 137, 2,
        196, 241, 60, 14, 74, 230, 182, 25, 56, 219, 200, 36, 185, 208, 71, 47,
        248, 198, 8, 57, 29, 190, 227, 65, 74, 182, 182, 73, 160, 175, 96, 80,
        57, 170, 199, 85, 42, 166, 214, 89, 133, 163, 123, 92, 83, 162, 173, 93,
        154, 162, 102, 93, 88, 164, 168, 91, 134, 167, 122, 88, 24, 172, 232, 83,
        251, 177, 5, 78, 24, 185, 232, 70, 82, 193, 174, 62, 136, 202, 120, 53,
        148, 212, 108, 43, 80, 223, 176, 32, 142, 234, 114, 21, 35, 246, 221, 9,
        222, 1, 34, 254, 147, 13, 109, 242, 17, 25, 239, 230, 43, 36, 213, 219,
        179, 46, 77, 209, 128, 56, 128, 199, 105, 65, 151, 190, 75, 73, 181, 182,
        7, 80, 249, 175, 130, 85, 126, 170, 164, 89, 92, 166, 95, 92, 161, 163,
        166, 93, 90, 162, 117, 93, 139, 162, 204, 91, 52, 164, 178, 88, 78, 167,
        52, 84, 204, 171, 99, 78, 157, 177, 87, 71, 169, 184, 45, 63, 211, 192,
        4, 54, 252, 201, 3, 44, 253, 211, 80, 33, 176, 222, 24, 22, 232, 233,
        135, 10, 121, 245, 205, 254, 51, 1, 22, 243, 234, 12, 148, 231, 108, 24,
        115, 220, 141, 35, 225, 209, 31, 46, 9, 200, 247, 55, 18, 191, 238, 64,
        32, 183, 224, 72, 82, 176, 174, 79, 197, 170, 59, 85, 142, 166, 114, 89,
        191, 163, 65, 92, 99, 162, 157, 93, 127, 162, 129, 93, 18, 164, 238, 91,
        23, 167, 233, 88, 130, 171, 126, 84, 64, 177, 192, 78, 58, 184, 198, 71,
        85, 192, 171, 63, 112, 201, 144, 54, 103, 211, 153, 44, 16, 222, 240, 33,
        66, 233, 190, 22, 207, 244, 49, 11, 136, 0, 120, 255, 64, 12, 192, 243,
        199, 23, 57, 232, 238, 34, 18, 221, 137, 45, 119, 210, 109, 55, 147, 200,
        114, 64, 142, 191, 116, 72, 140, 183, 83, 79, 173, 176, 243, 84, 13, 171,
        62, 89, 194, 166, 34, 92, 222, 163, 148, 93, 108, 162, 141, 93, 115, 162,
        15, 92, 241, 163, 30, 89, 226, 166, 200, 84, 56, 171, 28, 79, 228, 176,
        51, 72, 205, 183, 40, 64, 216, 191, 26, 55, 230, 200, 48, 45, 208, 210,
        143, 34, 113, 221, 100, 23, 156, 232, 219, 11, 37, 244, 34, 0, 222, 255,
        105, 244, 151, 11, 222, 232, 34, 23, 176, 221, 80, 34, 12, 211, 244, 44,
        29, 201, 227, 54, 10, 192, 246, 63, 249, 183, 7, 72, 8, 177, 248, 78,
        86, 171, 170, 84, 247, 166, 9, 89, 254, 163, 2, 92, 119, 162, 137, 93,
        104, 162, 152, 93, 210, 163, 46, 92, 173, 166, 83, 89, 240, 170, 16, 85,
        136, 176, 120, 79, 96, 183, 160, 72, 92, 191, 164, 64, 92, 200, 164, 55,
        59, 210, 197, 45, 210, 220, 46, 35, 247, 231, 9, 24, 124, 243, 132, 12,
        51, 255, 205, 0, 237, 10, 19, 245, 124, 22, 132, 233, 176, 33, 80, 222,
        93, 44, 163, 211, 88, 54, 168, 201, 120, 63, 136, 192, 154, 71, 102, 184,
        155, 78, 101, 177, 96, 84, 160, 171, 211, 88, 45, 167, 224, 91, 32, 164,
        124, 93, 132, 162, 161, 93, 95, 162, 77, 92, 179, 163, 134, 89, 122, 166,
        87, 85, 169, 170, 210, 79, 46, 176, 11, 73, 245, 182, 31, 65, 225, 190,
        46, 56, 210, 199, 90, 46, 166, 209, 204, 35, 52, 220, 174, 24, 82, 231,
        46, 13, 210, 242, 120, 1, 136, 254, 189, 245, 67, 10, 42, 234, 214, 21,
        239, 222, 17, 33, 58, 212, 198, 43, 52, 202, 204, 53, 6, 193, 250, 62,
        213, 184, 43, 71, 195, 177, 61, 78, 235, 171, 21, 84, 100, 167, 156, 88,
        66, 164, 190, 91, 145, 162, 111, 93, 87, 162, 169, 93, 150, 163, 106, 92,
        72, 166, 184, 89, 98, 170, 158, 85, 213, 175, 43, 80, 138, 182, 118, 73,
        102, 190, 154, 65, 74, 199, 182, 56, 18, 209, 238, 46, 150, 219, 106, 36,
        173, 230, 83, 25, 41, 242, 215, 13, 221, 253, 35, 2, 153, 9, 103, 246,
        47, 21, 209, 234, 112, 32, 144, 223, 47, 43, 209, 212, 64, 53, 192, 202,
        123, 62, 133, 193, 187, 70, 69, 185, 223, 77, 33, 178, 201, 83, 55, 172,
        99, 88, 157, 167, 154, 91, 102, 164, 96, 93, 160, 162, 176, 93, 80, 162,
        134, 92, 122, 163, 233, 89, 23, 166, 227, 85, 29, 170, 131, 80, 125, 175,
        224, 73, 32, 182, 20, 66, 236, 189, 62, 57, 194, 198, 130, 47, 126, 208,
        7, 37, 249, 218, 248, 25, 8, 230, 128, 14, 128, 241, 205, 2, 51, 253,
        17, 247, 239, 8, 119, 235, 137, 20, 48, 224, 208, 31, 105, 213, 151, 42,
        77, 203, 179, 52, 5, 194, 251, 61, 182, 185, 74, 70, 129, 178, 127, 77,
        132, 172, 124, 83, 214, 167, 42, 88, 139, 164, 117, 91, 175, 162, 81, 93,
        75, 162, 181, 93, 95, 163, 161, 92, 231, 165, 25, 90, 217, 169, 39, 86,
        38, 175, 218, 80, 183, 181, 73, 74, 116, 189, 140, 66, 59, 198, 197, 57,
        235, 207, 21, 48, 92, 218, 164, 37, 100, 229, 156, 26, 215, 240, 41, 15,
        136, 252, 120, 3, 69, 8, 187, 247, 226, 19, 30, 236, 47, 31, 209, 224,
        254, 41, 2, 214, 37, 52, 219, 203, 122, 61, 134, 194, 217, 69, 39, 186,
        30, 77, 226, 178, 46, 83, 210, 172, 239, 87, 17, 168, 79, 91, 177, 164,
        64, 93, 192, 162, 186, 93, 70, 162, 187, 92, 69, 163, 72, 90, 184, 165,
        106, 86, 150, 169, 48, 81, 208, 174, 176, 74, 80, 181, 4, 67, 252, 188,
        75, 58, 181, 197, 168, 48, 88, 207, 64, 38, 192, 217, 64, 27, 192, 228,
        209, 15, 47, 240, 35, 4, 221, 251, 102, 248, 154, 7, 198, 236, 58, 19,
        115, 225, 141, 30, 155, 214, 101, 41, 105, 204, 151, 51, 7, 195, 249, 60,
        154, 186, 102, 69, 68, 179, 188, 76, 34, 173, 222, 82, 77, 168, 179, 87,
        217, 164, 39, 91, 211, 162, 45, 93, 67, 162, 189, 93, 44, 163, 212, 92,
        139, 165, 117, 90, 85, 169, 171, 86, 123, 174, 133, 81, 233, 180, 23, 75,
        132, 188, 124, 67, 47, 197, 209, 58, 199, 206, 57, 49, 36, 217, 220, 38,
        29, 228, 227, 27, 134, 239, 122, 16, 50, 251, 206, 4, 240, 6, 16, 249,
        147, 18, 109, 237, 235, 29, 21, 226, 204, 40, 52, 215, 8, 51, 248, 204,
        119, 60, 137, 195, 243, 68, 13, 187, 90, 76, 166, 179, 142, 82, 114, 173,
        118, 87, 138, 168, 255, 90, 1, 165, 26, 93, 230, 162, 191, 93, 65, 162,
        235, 92, 21, 163, 162, 90, 94, 165, 236, 86, 20, 169, 217, 81, 39, 174,
        125, 75, 131, 180, 242, 67, 14, 188, 85, 59, 171, 196, 203, 49, 53, 206,
        120, 39, 136, 216, 134, 28, 122, 227, 34, 17, 222, 238, 121, 5, 135, 250,
        187, 249, 69, 6, 21, 238, 235, 17, 183, 226, 73, 29, 207, 215, 49, 40,
        136, 205, 120, 50, 12, 196, 244, 59, 130, 187, 126, 68, 10, 180, 246, 75,
        196, 173, 60, 82, 200, 168, 56, 87, 43, 165, 213, 90, 251, 162, 5, 93,
        65, 162, 191, 93, 255, 162, 1, 93, 51, 165, 205, 90, 213, 168, 43, 87,
        212, 173, 44, 82, 30, 180, 226, 75, 153, 187, 103, 68, 39, 196, 217, 59,
        165, 205, 91, 50, 238, 215, 18, 40, 215, 226, 41, 29, 54, 238, 202, 17,
        221, 249, 35, 6, 155, 5, 101, 250, 67, 17, 189, 238, 167, 28, 89, 227,
        151, 39, 105, 216, 232, 49, 24, 206, 112, 59, 144, 196, 9, 68, 247, 187,
        145, 75, 111, 180, 234, 81, 22, 174, 249, 86, 7, 169, 170, 90, 86, 165,
        239, 92, 17, 163, 191, 93, 65, 162, 22, 93, 234, 162, 247, 90, 9, 165,
        106, 87, 150, 168, 126, 82, 130, 173, 70, 76, 186, 179, 220, 68, 36, 187,
        93, 60, 163, 195, 235, 50, 21, 205, 173, 40, 83, 215, 203, 29, 53, 226,
        113, 18, 143, 237, 206, 6, 50, 249, 16, 251, 240, 4, 101, 239, 155, 16,
        252, 227, 4, 28, 5, 217, 251, 38, 170, 206, 86, 49, 21, 197, 235, 58,
        109, 188, 147, 67, 212, 180, 44, 75, 106, 174, 150, 81, 72, 169, 184, 86,
        130, 165, 126, 90, 40, 163, 216, 92, 67, 162, 189, 93, 214, 162, 42, 93,
        225, 164, 31, 91, 89, 168, 167, 87, 50, 173, 206, 82, 87, 179, 169, 76,
        177, 186, 79, 69, 33, 195, 223, 60, 134, 204, 122, 51, 186, 214, 70, 41,
        147, 225, 109, 30, 231, 236, 25, 19, 136, 248, 120, 7, 69, 4, 187, 251,
        243, 15, 13, 240, 96, 27, 160, 228, 96, 38, 160, 217, 197, 48, 59, 207,
        102, 58, 154, 197, 28, 67, 228, 188, 197, 74, 59, 181, 65, 81, 191, 174,
        119, 86, 137, 169, 81, 90, 175, 165, 192, 92, 64, 163, 186, 93, 70, 162,
        60, 93, 196, 162, 71, 91, 185, 164, 227, 87, 29, 168, 30, 83, 226, 172,
        11, 77, 245, 178, 194, 69, 62, 186, 97, 61, 159, 194, 9, 52, 247, 203,
        224, 41, 32, 214, 14, 31, 242, 224, 192, 19, 64, 236, 35, 8, 221, 247,
        101, 252, 155, 3, 182, 240, 74, 15, 67, 229, 189, 26, 61, 218, 195, 37,
        206, 207, 50, 48, 32, 198, 224, 57, 92, 189, 164, 66, 163, 181, 93, 74,
        21, 175, 235, 80, 204, 169, 52, 86, 222, 165, 34, 90, 89, 163, 167, 92,
        74, 162, 182, 93, 179, 162, 77, 93, 147, 164, 109, 91, 226, 167, 30, 88,
        148, 172, 108, 83, 148, 178, 108, 77, 204, 185, 52, 70, 31, 194, 225, 61,
        105, 203, 151, 52, 136, 213, 120, 42, 80, 224, 176, 31, 153, 235, 103, 20,
        64, 253, 192, 2, 235, 0, 21, 255, 146, 4, 110, 251, 39, 8, 217, 247,
        156, 11, 100, 244, 225, 14, 31, 241, 235, 17, 21, 238, 173, 20, 83, 235,
        28, 23, 228, 232, 46, 25, 210, 230, 219, 26, 37, 229, 28, 28, 228, 227,
        235, 28, 21, 227, 71, 29, 185, 226, 45, 29, 211, 226, 157, 28, 99, 227,
        155, 27, 101, 228, 42, 26, 214, 229, 79, 24, 177, 231, 19, 22, 237, 233,
        126, 19, 130, 236, 155, 16, 101, 239, 117, 13, 139, 242, 25, 10, 231, 245,
        148, 6, 108, 249, 245, 2, 11, 253, 75, 255, 181, 0, 163, 251, 93, 4,
        12, 248, 244, 7, 149, 244, 107, 11, 77, 241, 179, 14, 63, 238, 193, 17,
        121, 235, 135, 20, 5, 233, 251, 22, 237, 230, 19, 25, 59, 229, 197, 26,
        244, 227, 12, 28, 29, 227, 227, 28, 187, 226, 69, 29, 207, 226, 49, 29,
        87, 227, 169, 28, 83, 228, 173, 27, 190, 229, 66, 26, 147, 231, 109, 24,
        202, 233, 54, 22, 90, 236, 166, 19, 57, 239, 199, 16, 92, 242, 164, 13,
        181, 245, 75, 10, 56, 249, 200, 6, 214, 252, 42, 3, 128, 0, 128, 255,
        40, 4, 216, 251, 192, 7, 64, 248, 57, 11, 199, 244, 133, 14, 123, 241,
        150, 17, 106, 238, 97, 20, 159, 235, 218, 22, 38, 233, 247, 24, 9, 231,
        175, 26, 81, 229, 253, 27, 3, 228, 218, 28, 38, 227, 66, 29, 190, 226,
        54, 29, 202, 226, 180, 28, 76, 227, 190, 27, 66, 228, 89, 26, 167, 229,
        138, 24, 118, 231, 89, 22, 167, 233, 205, 19, 51, 236, 242, 16, 14, 239,
        211, 13, 45, 242, 125, 10, 131, 245, 252, 6, 4, 249, 95, 3, 161, 252,
        182, 255, 74, 0, 12, 252, 244, 3, 115, 248, 141, 7, 248, 244, 8, 11,
        169, 241, 87, 14, 148, 238, 108, 17, 197, 235, 59, 20, 72, 233, 184, 22,
        37, 231, 219, 24, 103, 229, 153, 26, 19, 228, 237, 27, 48, 227, 208, 28,
        192, 226, 64, 29, 198, 226, 58, 29, 66, 227, 190, 28, 49, 228, 207, 27,
        144, 229, 112, 26, 89, 231, 167, 24, 133, 233, 123, 22, 11, 236, 245, 19,
        226, 238, 30, 17, 254, 241, 2, 14, 81, 245, 175, 10, 208, 248, 48, 7,
        108, 252, 148, 3, 21, 0, 235, 255, 191, 3, 65, 252, 89, 7, 167, 248,
        214, 10, 42, 245, 40, 14, 216, 241, 65, 17, 191, 238, 20, 20, 236, 235,
        150, 22, 106, 233, 190, 24, 66, 231, 131, 26, 125, 229, 220, 27, 36, 228,
        198, 28, 58, 227, 60, 29, 196, 226, 61, 29, 195, 226, 200, 28, 56, 227,
        224, 27, 32, 228, 135, 26, 121, 229, 196, 24, 60, 231, 157, 22, 99, 233,
        28, 20, 228, 235, 73, 17, 183, 238, 49, 14, 207, 241, 224, 10, 32, 245,
        100, 7, 156, 248, 201, 3, 55, 252, 32, 0, 224, 255, 118, 252, 138, 3,
        219, 248, 37, 7, 91, 245, 165, 10, 7, 242, 249, 13, 235, 238, 21, 17,
        19, 236, 237, 19, 140, 233, 116, 22, 95, 231, 161, 24, 148, 229, 108, 26,
        52, 228, 204, 27, 68, 227, 188, 28, 199, 226, 57, 29, 192, 226, 64, 29,
        46, 227, 210, 28, 16, 228, 240, 27, 98, 229, 158, 26, 32, 231, 224, 24,
        65, 233, 191, 22, 190, 235, 66, 20, 140, 238, 116, 17, 160, 241, 96, 14,
        238, 244, 18, 11, 105, 248, 151, 7, 2, 252, 254, 3, 171, 255, 85, 0,
        85, 3, 171, 252, 242, 6, 14, 249, 115, 10, 141, 245, 202, 13, 54, 242,
        234, 16, 22, 239, 198, 19, 58, 236, 82, 22, 174, 233, 132, 24, 124, 231,
        84, 26, 172, 229, 187, 27, 69, 228, 177, 28, 79, 227, 53, 29, 203, 226,
        67, 29, 189, 226, 219, 28, 37, 227, 0, 28, 0, 228, 180, 26, 76, 229,
        252, 24, 4, 231, 225, 22, 31, 233, 105, 20, 151, 235, 159, 17, 97, 238,
        142, 14, 114, 241, 67, 11, 189, 244, 203, 7, 53, 248, 51, 4, 205, 251,
        138, 0, 118, 255, 225, 252, 31, 3, 66, 249, 190, 6, 191, 245, 65, 10,
        101, 242, 155, 13, 66, 239, 190, 16, 98, 236, 158, 19, 209, 233, 47, 22,
        153, 231, 103, 24, 195, 229, 61, 26, 87, 228, 169, 27, 90, 227, 166, 28,
        207, 226, 49, 29, 187, 226, 69, 29, 28, 227, 228, 28, 241, 227, 15, 28,
        54, 229, 202, 26, 232, 230, 24, 25, 254, 232, 2, 23, 113, 235, 143, 20,
        54, 238, 202, 17, 67, 241, 189, 14, 140, 244, 116, 11, 2, 248, 254, 7,
        152, 251, 104, 4, 64, 255, 192, 0, 234, 2, 22, 253, 138, 6, 118, 249,
        15, 10, 241, 245, 107, 13, 149, 242, 146, 16, 110, 239, 118, 19, 138, 236,
        12, 22, 244, 233, 73, 24, 183, 231, 37, 26, 219, 229, 151, 27, 105, 228,
        155, 28, 101, 227, 44, 29, 212, 226, 71, 29, 185, 226, 237, 28, 19, 227,
        31, 28, 225, 227, 223, 26, 33, 229, 52, 25, 204, 230, 35, 23, 221, 232,
        181, 20, 75, 235, 244, 17, 12, 238, 235, 14, 21, 241, 166, 11, 90, 244,
        50, 8, 206, 247, 157, 4, 99, 251, 245, 0, 11, 255, 75, 253, 181, 2,
        171, 249, 85, 6, 36, 246, 220, 9, 196, 242, 60, 13, 154, 239, 102, 16,
        178, 236, 78, 19, 23, 234, 233, 21, 213, 231, 43, 24, 243, 229, 13, 26,
        123, 228, 133, 27, 113, 227, 143, 28, 217, 226, 39, 29, 183, 226, 73, 29,
        11, 227, 245, 28, 211, 227, 45, 28, 12, 229, 244, 26, 177, 230, 79, 25,
        189, 232, 67, 23, 37, 235, 219, 20, 226, 237, 30, 18, 231, 240, 25, 15,
        42, 244, 214, 11, 155, 247, 101, 8, 47, 251, 209, 4, 213, 254, 43, 1,
        128, 2, 128, 253, 33, 6, 223, 249, 170, 9, 86, 246, 12, 13, 244, 242,
        57, 16, 199, 239, 38, 19, 218, 236, 197, 21, 59, 234, 13, 24, 243, 231,
        244, 25, 12, 230, 115, 27, 141, 228, 131, 28, 125, 227, 33, 29, 223, 226,
        74, 29, 182, 226, 253, 28, 3, 227, 60, 28, 196, 227, 9, 27, 247, 228,
        105, 25, 151, 230, 100, 23, 156, 232, 0, 21, 0, 235, 72, 18, 184, 237,
        70, 15, 186, 240, 7, 12, 249, 243, 152, 8, 104, 247, 6, 5, 250, 250,
        96, 1, 160, 254, 181, 253, 75, 2, 19, 250, 237, 5, 136, 246, 120, 9,
        36, 243, 220, 12, 243, 239, 13, 16, 3, 237, 253, 18, 95, 234, 161, 21,
        18, 232, 238, 23, 37, 230, 219, 25, 160, 228, 96, 27, 137, 227, 119, 28,
        229, 226, 27, 29, 181, 226, 75, 29, 251, 226, 5, 29, 182, 227, 74, 28,
        227, 228, 29, 27, 124, 230, 132, 25, 124, 232, 132, 23, 219, 234, 37, 21,
        143, 237, 113, 18, 140, 240, 116, 15, 200, 243, 56, 12, 53, 247, 203, 8,
        197, 250, 59, 5, 107, 254, 149, 1, 21, 2, 235, 253, 185, 5, 71, 250,
        69, 9, 187, 246, 172, 12, 84, 243, 224, 15, 32, 240, 212, 18, 44, 237,
        125, 21, 131, 234, 207, 23, 49, 232, 194, 25, 62, 230, 77, 27, 179, 228,
        106, 28, 150, 227, 21, 29, 235, 226, 75, 29, 181, 226, 12, 29, 244, 226,
        88, 28, 168, 227, 49, 27, 207, 228, 158, 25, 98, 230, 163, 23, 93, 232,
        74, 21, 182, 234, 155, 18, 101, 237, 161, 15, 95, 240, 104, 12, 152, 243,
        254, 8, 2, 247, 111, 5, 145, 250, 203, 1, 53, 254, 32, 254, 224, 1,
        124, 250, 132, 5, 238, 246, 18, 9, 132, 243, 124, 12, 77, 240, 179, 15,
        85, 237, 171, 18, 167, 234, 89, 21, 80, 232, 176, 23, 88, 230, 168, 25,
        199, 228, 57, 27, 163, 227, 93, 28, 241, 226, 15, 29, 181, 226, 75, 29,
        237, 226, 19, 29, 155, 227, 101, 28, 187, 228, 69, 27, 72, 230, 184, 25,
        61, 232, 195, 23, 146, 234, 110, 21, 60, 237, 196, 18, 50, 240, 206, 15,
        103, 243, 153, 12, 207, 246, 49, 9, 92, 250, 164, 5, 0, 254, 0, 2,
        171, 1, 85, 254, 80, 5, 176, 250, 223, 8, 33, 247, 75, 12, 181, 243,
        134, 15, 122, 240, 130, 18, 126, 237, 52, 21, 204, 234, 144, 23, 112, 232,
        142, 25, 114, 230, 37, 27, 219, 228, 79, 28, 177, 227, 8, 29, 248, 226,
        75, 29, 181, 226, 25, 29, 231, 226, 114, 28, 142, 227, 88, 27, 168, 228,
        209, 25, 47, 230, 226, 23, 30, 232, 147, 21, 109, 234, 237, 18, 19, 237,
        251, 15, 5, 240, 201, 12, 55, 243, 99, 9, 157, 246, 216, 5, 40, 250,
        53, 2, 203, 253, 139, 254, 117, 1, 229, 250, 27, 5, 84, 247, 172, 8,
        229, 243, 27, 12, 168, 240, 88, 15, 167, 237, 89, 18, 241, 234, 15, 21,
        143, 232, 113, 23, 140, 230, 116, 25, 239, 228, 17, 27, 191, 227, 65, 28,
        0, 227, 0, 29, 182, 226, 74, 29, 225, 226, 31, 29, 130, 227, 126, 28,
        149, 228, 107, 27, 22, 230, 234, 25, 255, 231, 1, 24, 73, 234, 183, 21,
        234, 236, 22, 19, 216, 239, 40, 16, 7, 243, 249, 12, 106, 246, 150, 9,
        244, 249, 12, 6, 149, 253, 107, 2, 64, 1, 192, 254, 230, 4, 26, 251,
        121, 8, 135, 247, 234, 11, 22, 244, 43, 15, 213, 240, 47, 18, 209, 237,
        234, 20, 22, 235, 80, 23, 176, 232, 89, 25, 167, 230, 252, 26, 4, 229,
        51, 28, 205, 227, 249, 28, 7, 227, 73, 29, 183, 226, 37, 29, 219, 226,
        139, 28, 117, 227, 126, 27, 130, 228, 3, 26, 253, 229, 31, 24, 225, 231,
        218, 21, 38, 234, 62, 19, 194, 236, 84, 16, 172, 239, 41, 13, 215, 242,
        200, 9, 56, 246, 65, 6, 191, 249, 160, 2, 96, 253, 245, 254, 11, 1,
        78, 251, 178, 4, 186, 247, 70, 8, 71, 244, 185, 11, 3, 241, 253, 14,
        251, 237, 5, 18, 60, 235, 196, 20, 208, 232, 48, 23, 194, 230, 62, 25,
        24, 229, 232, 26, 219, 227, 37, 28, 16, 227, 240, 28, 184, 226, 72, 29,
        214, 226, 42, 29, 105, 227, 151, 28, 112, 228, 144, 27, 229, 229, 27, 26,
        195, 231, 61, 24, 2, 234, 254, 21, 154, 236, 102, 19, 128, 239, 128, 16,
        168, 242, 88, 13, 5, 246, 251, 9, 139, 249, 117, 6, 43, 253, 213, 2,
        213, 0, 43, 255, 125, 4, 131, 251, 19, 8, 237, 247, 136, 11, 120, 244,
        207, 14, 49, 241, 219, 17, 37, 238, 158, 20, 98, 235, 15, 23, 241, 232,
        35, 25, 221, 230, 210, 26, 46, 229, 22, 28, 234, 227, 232, 28, 24, 227,
        70, 29, 186, 226, 47, 29, 209, 226, 162, 28, 94, 227, 162, 27, 94, 228,
        51, 26, 205, 229, 91, 24, 165, 231, 33, 22, 223, 233, 142, 19, 114, 236,
        172, 16, 84, 239, 136, 13, 120, 242, 45, 10, 211, 245, 169, 6, 87, 249,
        10, 3, 246, 252, 96, 255, 160, 0, 184, 251, 72, 4, 33, 248, 223, 7,
        169, 244, 87, 11, 95, 241, 161, 14, 80, 238, 176, 17, 136, 235, 120, 20,
        18, 233, 238, 22, 248, 230, 8, 25, 67, 229, 189, 26, 250, 227, 6, 28,
        33, 227, 223, 28, 188, 226, 68, 29, 205, 226, 51, 29, 83, 227, 173, 28,
        76, 228, 180, 27, 181, 229, 75, 26, 135, 231, 121, 24, 188, 233, 68, 22,
        74, 236, 182, 19, 40, 239, 216, 16, 73, 242, 183, 13, 161, 245, 95, 10,
        35, 249, 221, 6, 193, 252, 63, 3, 106, 0, 150, 255, 19, 4, 237, 251,
        172, 7, 84, 248, 38, 11, 218, 244, 114, 14, 142, 241, 133, 17, 123, 238,
        82, 20, 174, 235, 204, 22, 52, 233, 236, 24, 20, 231, 167, 26, 89, 229,
        246, 27, 10, 228, 214, 28, 42, 227, 65, 29, 191, 226, 55, 29, 201, 226,
        184, 28, 72, 227, 197, 27, 59, 228, 98, 26, 158, 229, 150, 24, 106, 231,
        103, 22, 153, 233, 221, 19, 35, 236, 4, 17, 252, 238, 230, 13, 26, 242,
        145, 10, 111, 245, 17, 7, 239, 248, 116, 3, 140, 252, 203, 255, 53, 0,
        34, 252, 222, 3, 136, 248, 120, 7, 12, 245, 244, 10, 188, 241, 68, 14,
        166, 238, 90, 17, 213, 235, 43, 20, 85, 233, 171, 22, 49, 231, 207, 24,
        112, 229, 144, 26, 26, 228, 230, 27, 52, 227, 204, 28, 194, 226, 62, 29,
        197, 226, 59, 29, 62, 227, 194, 28, 42, 228, 214, 27, 135, 229, 121, 26,
        77, 231, 179, 24, 119, 233, 137, 22, 252, 235, 4, 20, 209, 238, 47, 17,
        235, 241, 21, 14, 62, 245, 194, 10, 187, 248, 69, 7, 87, 252, 169, 3,
        0, 0, 0, 0, 169, 3, 87, 252, 69, 7, 187, 248, 194, 10, 62, 245,
        21, 14, 235, 241, 47, 17, 209, 238, 4, 20, 252, 235, 137, 22, 119, 233,
        179, 24, 77, 231, 121, 26, 135, 229, 214, 27, 42, 228, 194, 28, 62, 227,
        59, 29, 197, 226, 62, 29, 194, 226, 204, 28, 52, 227, 230, 27, 26, 228,
        144, 26, 112, 229, 207, 24, 49, 231, 171, 22, 85, 233, 43, 20, 213, 235,
        90, 17, 166, 238, 68, 14, 188, 241, 244, 10, 12, 245, 120, 7, 136, 248,
        222, 3, 34, 252, 53, 0, 203, 255, 140, 252, 116, 3, 239, 248, 17, 7,
        111, 245, 145, 10, 26, 242, 230, 13, 252, 238, 4, 17, 35, 236, 221, 19,
        153, 233, 103, 22, 106, 231, 150, 24, 158, 229, 98, 26, 59, 228, 197, 27,
        72, 227, 184, 28, 201, 226, 55, 29, 191, 226, 65, 29, 42, 227, 214, 28,
        10, 228, 246, 27, 89, 229, 167, 26, 20, 231, 236, 24, 52, 233, 204, 22,
        174, 235, 82, 20, 123, 238, 133, 17, 142, 241, 114, 14, 218, 244, 38, 11,
        84, 248, 172, 7, 237, 251, 19, 4, 150, 255, 106, 0, 63, 3, 193, 252,
        221, 6, 35, 249, 95, 10, 161, 245, 183, 13, 73, 242, 216, 16, 40, 239,
        182, 19, 74, 236, 68, 22, 188, 233, 121, 24, 135, 231, 75, 26, 181, 229,
        180, 27, 76, 228, 173, 28, 83, 227, 51, 29, 205, 226, 68, 29, 188, 226,
        223, 28, 33, 227, 6, 28, 250, 227, 189, 26, 67, 229, 8, 25, 248, 230,
        238, 22, 18, 233, 120, 20, 136, 235, 176, 17, 80, 238, 161, 14, 95, 241,
        87, 11, 169, 244, 223, 7, 33, 248, 72, 4, 184, 251, 160, 0, 96, 255,
        246, 252, 10, 3, 87, 249, 169, 6, 211, 245, 45, 10, 120, 242, 136, 13,
        84, 239, 172, 16, 114, 236, 142, 19, 223, 233, 33, 22, 165, 231, 91, 24,
        205, 229, 51, 26, 94, 228, 162, 27, 94, 227, 162, 28, 209, 226, 47, 29,
        186, 226, 70, 29, 24, 227, 232, 28, 234, 227, 22, 28, 46, 229, 210, 26,
        221, 230, 35, 25, 241, 232, 15, 23, 98, 235, 158, 20, 37, 238, 219, 17,
        49, 241, 207, 14, 120, 244, 136, 11, 237, 247, 19, 8, 131, 251, 125, 4,
        43, 255, 213, 0, 213, 2, 43, 253, 117, 6, 139, 249, 251, 9, 5, 246,
        88, 13, 168, 242, 128, 16, 128, 239, 102, 19, 154, 236, 254, 21, 2, 234,
        61, 24, 195, 231, 27, 26, 229, 229, 144, 27, 112, 228, 151, 28, 105, 227,
        42, 29, 214, 226, 72, 29, 184, 226, 240, 28, 16, 227, 37, 28, 219, 227,
        232, 26, 24, 229, 62, 25, 194, 230, 48, 23, 208, 232, 196, 20, 60, 235,
        5, 18, 251, 237, 253, 14, 3, 241, 185, 11, 71, 244, 70, 8, 186, 247,
        178, 4, 78, 251, 11, 1, 245, 254, 96, 253, 160, 2, 191, 249, 65, 6,
        56, 246, 200, 9, 215, 242, 41, 13, 172, 239, 84, 16, 194, 236, 62, 19,
        38, 234, 218, 21, 225, 231, 31, 24, 253, 229, 3, 26, 130, 228, 126, 27,
        117, 227, 139, 28, 219, 226, 37, 29, 183, 226, 73, 29, 7, 227, 249, 28,
        205, 227, 51, 28, 4, 229, 252, 26, 167, 230, 89, 25, 176, 232, 80, 23,
        22, 235, 234, 20, 209, 237, 47, 18, 213, 240, 43, 15, 22, 244, 234, 11,
        135, 247, 121, 8, 26, 251, 230, 4, 192, 254, 64, 1, 107, 2, 149, 253,
        12, 6, 244, 249, 150, 9, 106, 246, 249, 12, 7, 243, 40, 16, 216, 239,
        22, 19, 234, 236, 183, 21, 73, 234, 1, 24, 255, 231, 234, 25, 22, 230,
        107, 27, 149, 228, 126, 28, 130, 227, 31, 29, 225, 226, 74, 29, 182, 226,
        0, 29, 0, 227, 65, 28, 191, 227, 17, 27, 239, 228, 116, 25, 140, 230,
        113, 23, 143, 232, 15, 21, 241, 234, 89, 18, 167, 237, 88, 15, 168, 240,
        27, 12, 229, 243, 172, 8, 84, 247, 27, 5, 229, 250, 117, 1, 139, 254,
        203, 253, 53, 2, 40, 250, 216, 5, 157, 246, 99, 9, 55, 243, 201, 12,
        5, 240, 251, 15, 19, 237, 237, 18, 109, 234, 147, 21, 30, 232, 226, 23,
        47, 230, 209, 25, 168, 228, 88, 27, 142, 227, 114, 28, 231, 226, 25, 29,
        181, 226, 75, 29, 248, 226, 8, 29, 177, 227, 79, 28, 219, 228, 37, 27,
        114, 230, 142, 25, 112, 232, 144, 23, 204, 234, 52, 21, 126, 237, 130, 18,
        122, 240, 134, 15, 181, 243, 75, 12, 33, 247, 223, 8, 176, 250, 80, 5,
        85, 254, 171, 1, 0, 2, 0, 254, 164, 5, 92, 250, 49, 9, 207, 246,
        153, 12, 103, 243, 206, 15, 50, 240, 196, 18, 60, 237, 110, 21, 146, 234,
        195, 23, 61, 232, 184, 25, 72, 230, 69, 27, 187, 228, 101, 28, 155, 227,
        19, 29, 237, 226, 75, 29, 181, 226, 15, 29, 241, 226, 93, 28, 163, 227,
        57, 27, 199, 228, 168, 25, 88, 230, 176, 23, 80, 232, 89, 21, 167, 234,
        171, 18, 85, 237, 179, 15, 77, 240, 124, 12, 132, 243, 18, 9, 238, 246,
        132, 5, 124, 250, 224, 1, 32, 254, 53, 254, 203, 1, 145, 250, 111, 5,
        2, 247, 254, 8, 152, 243, 104, 12, 95, 240, 161, 15, 101, 237, 155, 18,
        182, 234, 74, 21, 93, 232, 163, 23, 98, 230, 158, 25, 207, 228, 49, 27,
        168, 227, 88, 28, 244, 226, 12, 29, 181, 226, 75, 29, 235, 226, 21, 29,
        150, 227, 106, 28, 179, 228, 77, 27, 62, 230, 194, 25, 49, 232, 207, 23,
        131, 234, 125, 21, 44, 237, 212, 18, 32, 240, 224, 15, 84, 243, 172, 12,
        187, 246, 69, 9, 71, 250, 185, 5, 235, 253, 21, 2, 149, 1, 107, 254,
        59, 5, 197, 250, 203, 8, 53, 247, 56, 12, 200, 243, 116, 15, 140, 240,
        113, 18, 143, 237, 37, 21, 219, 234, 132, 23, 124, 232, 132, 25, 124, 230,
        29, 27, 227, 228, 74, 28, 182, 227, 5, 29, 251, 226, 75, 29, 181, 226,
        27, 29, 229, 226, 119, 28, 137, 227, 96, 27, 160, 228, 219, 25, 37, 230,
        238, 23, 18, 232, 161, 21, 95, 234, 253, 18, 3, 237, 13, 16, 243, 239,
        220, 12, 36, 243, 120, 9, 136, 246, 237, 5, 19, 250, 75, 2, 181, 253,
        160, 254, 96, 1, 250, 250, 6, 5, 104, 247, 152, 8, 249, 243, 7, 12,
        186, 240, 70, 15, 184, 237, 72, 18, 0, 235, 0, 21, 156, 232, 100, 23,
        151, 230, 105, 25, 247, 228, 9, 27, 196, 227, 60, 28, 3, 227, 253, 28,
        182, 226, 74, 29, 223, 226, 33, 29, 125, 227, 131, 28, 141, 228, 115, 27,
        12, 230, 244, 25, 243, 231, 13, 24, 59, 234, 197, 21, 218, 236, 38, 19,
        199, 239, 57, 16, 244, 242, 12, 13, 86, 246, 170, 9, 223, 249, 33, 6,
        128, 253, 128, 2, 43, 1, 213, 254, 209, 4, 47, 251, 101, 8, 155, 247,
        214, 11, 42, 244, 25, 15, 231, 240, 30, 18, 226, 237, 219, 20, 37, 235,
        67, 23, 189, 232, 79, 25, 177, 230, 244, 26, 12, 229, 45, 28, 211, 227,
        245, 28, 11, 227, 73, 29, 183, 226, 39, 29, 217, 226, 143, 28, 113, 227,
        133, 27, 123, 228, 13, 26, 243, 229, 43, 24, 213, 231, 233, 21, 23, 234,
        78, 19, 178, 236, 102, 16, 154, 239, 60, 13, 196, 242, 220, 9, 36, 246,
        85, 6, 171, 249, 181, 2, 75, 253, 11, 255, 245, 0, 99, 251, 157, 4,
        206, 247, 50, 8, 90, 244, 166, 11, 21, 241, 235, 14, 12, 238, 244, 17,
        75, 235, 181, 20, 221, 232, 35, 23, 204, 230, 52, 25, 33, 229, 223, 26,
        225, 227, 31, 28, 19, 227, 237, 28, 185, 226, 71, 29, 212, 226, 44, 29,
        101, 227, 155, 28, 105, 228, 151, 27, 219, 229, 37, 26, 183, 231, 73, 24,
        244, 233, 12, 22, 138, 236, 118, 19, 110, 239, 146, 16, 149, 242, 107, 13,
        241, 245, 15, 10, 118, 249, 138, 6, 22, 253, 234, 2, 192, 0, 64, 255,
        104, 4, 152, 251, 254, 7, 2, 248, 116, 11, 140, 244, 189, 14, 67, 241,
        202, 17, 54, 238, 143, 20, 113, 235, 2, 23, 254, 232, 24, 25, 232, 230,
        202, 26, 54, 229, 15, 28, 241, 227, 228, 28, 28, 227, 69, 29, 187, 226,
        49, 29, 207, 226, 166, 28, 90, 227, 169, 27, 87, 228, 61, 26, 195, 229,
        103, 24, 153, 231, 47, 22, 209, 233, 158, 19, 98, 236, 190, 16, 66, 239,
        155, 13, 101, 242, 65, 10, 191, 245, 190, 6, 66, 249, 31, 3, 225, 252,
        118, 255, 138, 0, 205, 251, 51, 4, 53, 248, 203, 7, 189, 244, 67, 11,
        114, 241, 142, 14, 97, 238, 159, 17, 151, 235, 105, 20, 31, 233, 225, 22,
        4, 231, 252, 24, 76, 229, 180, 26, 0, 228, 0, 28, 37, 227, 219, 28,
        189, 226, 67, 29, 203, 226, 53, 29, 79, 227, 177, 28, 69, 228, 187, 27,
        172, 229, 84, 26, 124, 231, 132, 24, 174, 233, 82, 22, 58, 236, 198, 19,
        22, 239, 234, 16, 54, 242, 202, 13, 141, 245, 115, 10, 14, 249, 242, 6,
        171, 252, 85, 3, 85, 0, 171, 255, 254, 3, 2, 252, 151, 7, 105, 248,
        18, 11, 238, 244, 96, 14, 160, 241, 116, 17, 140, 238, 66, 20, 190, 235,
        191, 22, 65, 233, 224, 24, 32, 231, 158, 26, 98, 229, 240, 27, 16, 228,
        210, 28, 46, 227, 64, 29, 192, 226, 57, 29, 199, 226, 188, 28, 68, 227,
        204, 27, 52, 228, 108, 26, 148, 229, 161, 24, 95, 231, 116, 22, 140, 233,
        237, 19, 19, 236, 21, 17, 235, 238, 249, 13, 7, 242, 165, 10, 91, 245,
        37, 7, 219, 248, 138, 3, 118, 252, 224, 255, 32, 0, 55, 252, 201, 3,
        156, 248, 100, 7, 32, 245, 224, 10, 207, 241, 49, 14, 183, 238, 73, 17,
        228, 235, 28, 20, 99, 233, 157, 22, 60, 231, 196, 24, 121, 229, 135, 26,
        32, 228, 224, 27, 56, 227, 200, 28, 195, 226, 61, 29, 196, 226, 60, 29,
        58, 227, 198, 28, 36, 228, 220, 27, 125, 229, 131, 26, 66, 231, 190, 24,
        106, 233, 150, 22, 236, 235, 20, 20, 191, 238, 65, 17, 216, 241, 40, 14,
        42, 245, 214, 10, 167, 248, 89, 7, 65, 252, 191, 3, 235, 255, 21, 0,
        148, 3, 108, 252, 48, 7, 208, 248, 175, 10, 81, 245, 2, 14, 254, 241,
        30, 17, 226, 238, 245, 19, 11, 236, 123, 22, 133, 233, 167, 24, 89, 231,
        112, 26, 144, 229, 207, 27, 49, 228, 190, 28, 66, 227, 58, 29, 198, 226,
        64, 29, 192, 226, 208, 28, 48, 227, 237, 27, 19, 228, 153, 26, 103, 229,
        219, 24, 37, 231, 184, 22, 72, 233, 59, 20, 197, 235, 108, 17, 148, 238,
        87, 14, 169, 241, 8, 11, 248, 244, 141, 7, 115, 248, 244, 3, 12, 252,
        74, 0, 182, 255, 161, 252, 95, 3, 4, 249, 252, 6, 131, 245, 125, 10,
        45, 242, 211, 13, 14, 239, 242, 16, 51, 236, 205, 19, 167, 233, 89, 22,
        118, 231, 138, 24, 167, 229, 89, 26, 66, 228, 190, 27, 76, 227, 180, 28,
        202, 226, 54, 29, 190, 226, 66, 29, 38, 227, 218, 28, 3, 228, 253, 27,
        81, 229, 175, 26, 9, 231, 247, 24, 38, 233, 218, 22, 159, 235, 97, 20,
        106, 238, 150, 17, 123, 241, 133, 14, 199, 244, 57, 11, 64, 248, 192, 7,
        216, 251, 40, 4, 128, 255, 128, 0, 42, 3, 214, 252, 200, 6, 56, 249,
        75, 10, 181, 245, 164, 13, 92, 242, 199, 16, 57, 239, 166, 19, 90, 236,
        54, 22, 202, 233, 109, 24, 147, 231, 66, 26, 190, 229, 173, 27, 83, 228,
        169, 28, 87, 227, 49, 29, 207, 226, 69, 29, 187, 226, 227, 28, 29, 227,
        12, 28, 244, 227, 197, 26, 59, 229, 19, 25, 237, 230, 251, 22, 5, 233,
        135, 20, 121, 235, 193, 17, 63, 238, 179, 14, 77, 241, 107, 11, 149, 244,
        244, 7, 12, 248, 93, 4, 163, 251, 181, 0, 75, 255, 11, 253, 245, 2,
        108, 249, 148, 6, 231, 245, 25, 10, 139, 242, 117, 13, 101, 239, 155, 16,
        130, 236, 126, 19, 237, 233, 19, 22, 177, 231, 79, 24, 214, 229, 42, 26,
        101, 228, 155, 27, 99, 227, 157, 28, 211, 226, 45, 29, 185, 226, 71, 29,
        21, 227, 235, 28, 228, 227, 28, 28, 37, 229, 219, 26, 210, 230, 46, 25,
        228, 232, 28, 23, 83, 235, 173, 20, 21, 238, 235, 17, 31, 241, 225, 14,
        100, 244, 156, 11, 217, 247, 39, 8, 110, 251, 146, 4, 21, 255, 235, 0,
        192, 2, 64, 253, 96, 6, 160, 249, 231, 9, 25, 246, 69, 13, 187, 242,
        111, 16, 145, 239, 86, 19, 170, 236, 240, 21, 16, 234, 49, 24, 207, 231,
        18, 26, 238, 229, 137, 27, 119, 228, 146, 28, 110, 227, 40, 29, 216, 226,
        73, 29, 183, 226, 244, 28, 12, 227, 42, 28, 214, 227, 240, 26, 16, 229,
        73, 25, 183, 230, 61, 23, 195, 232, 211, 20, 45, 235, 22, 18, 234, 237,
        15, 15, 241, 240, 205, 11, 51, 244, 91, 8, 165, 247, 199, 4, 57, 251,
        32, 1, 224, 254, 117, 253, 139, 2, 212, 249, 44, 6, 76, 246, 180, 9,
        234, 242, 22, 13, 190, 239, 66, 16, 210, 236, 46, 19, 52, 234, 204, 21,
        237, 231, 19, 24, 7, 230, 249, 25, 137, 228, 119, 27, 122, 227, 134, 28,
        222, 226, 34, 29, 182, 226, 74, 29, 4, 227, 252, 28, 199, 227, 57, 28,
        251, 228, 5, 27, 156, 230, 100, 25, 163, 232, 93, 23, 7, 235, 249, 20,
        193, 237, 63, 18, 195, 240, 61, 15, 2, 244, 254, 11, 114, 247, 142, 8,
        4, 251, 252, 4, 171, 254, 85, 1, 85, 2, 171, 253, 247, 5, 9, 250,
        130, 9, 126, 246, 230, 12, 26, 243, 22, 16, 234, 239, 5, 19, 251, 236,
        168, 21, 88, 234, 244, 23, 12, 232, 224, 25, 32, 230, 100, 27, 156, 228,
        121, 28, 135, 227, 29, 29, 227, 226, 75, 29, 181, 226, 3, 29, 253, 226,
        71, 28, 185, 227, 25, 27, 231, 228, 127, 25, 129, 230, 125, 23, 131, 232,
        30, 21, 226, 234, 105, 18, 151, 237, 107, 15, 149, 240, 46, 12, 210, 243,
        193, 8, 63, 247, 48, 5, 208, 250, 139, 1, 117, 254, 224, 253, 32, 2,
        61, 250, 195, 5, 177, 246, 79, 9, 74, 243, 182, 12, 23, 240, 233, 15,
        35, 237, 221, 18, 124, 234, 132, 21, 43, 232, 213, 23, 57, 230, 199, 25,
        175, 228, 81, 27, 147, 227, 109, 28, 234, 226, 22, 29, 181, 226, 75, 29,
        246, 226, 10, 29, 171, 227, 85, 28, 211, 228, 45, 27, 103, 230, 153, 25,
        99, 232, 157, 23, 189, 234, 67, 21, 109, 237, 147, 18, 104, 240, 152, 15,
        161, 243, 95, 12, 12, 247, 244, 8, 155, 250, 101, 5, 64, 254, 192, 1,
        235, 1, 21, 254, 143, 5, 113, 250, 28, 9, 228, 246, 133, 12, 123, 243,
        188, 15, 68, 240, 180, 18, 76, 237, 96, 21, 160, 234, 182, 23, 74, 232,
        173, 25, 83, 230, 61, 27, 195, 228, 96, 28, 160, 227, 16, 29, 240, 226,
        75, 29, 181, 226, 17, 29, 239, 226, 98, 28, 158, 227, 65, 27, 191, 228,
        178, 25, 78, 230, 189, 23, 67, 232, 103, 21, 153, 234, 188, 18, 68, 237,
        197, 15, 59, 240, 143, 12, 113, 243, 39, 9, 217, 246, 153, 5, 103, 250,
        245, 1, 11, 254, 75, 254, 181, 1, 166, 250, 90, 5, 22, 247, 234, 8,
        171, 243, 85, 12, 113, 240, 143, 15, 118, 237, 138, 18, 197, 234, 59, 21,
        105, 232, 151, 23, 109, 230, 147, 25, 215, 228, 41, 27, 174, 227, 82, 28,
        247, 226, 9, 29, 181, 226, 75, 29, 232, 226, 24, 29, 145, 227, 111, 28,
        171, 228, 85, 27, 52, 230, 204, 25, 36, 232, 220, 23, 116, 234, 140, 21,
        27, 237, 229, 18, 14, 240, 242, 15, 65, 243, 191, 12, 167, 246, 89, 9,
        50, 250, 206, 5, 213, 253, 43, 2, 128, 1, 128, 254, 38, 5, 218, 250,
        183, 8, 73, 247, 36, 12, 220, 243, 98, 15, 158, 240, 97, 18, 159, 237,
        22, 21, 234, 234, 119, 23, 137, 232, 121, 25, 135, 230, 21, 27, 235, 228,
        68, 28, 188, 227, 2, 29, 254, 226, 75, 29, 181, 226, 30, 29, 226, 226,
        124, 28, 132, 227, 104, 27, 152, 228, 229, 25, 27, 230, 251, 23, 5, 232,
        176, 21, 80, 234, 13, 19, 243, 236, 31, 16, 225, 239, 239, 12, 17, 243,
        140, 9, 116, 246, 2, 6, 254, 249, 96, 2, 160, 253, 181, 254, 75, 1,
        15, 251, 241, 4, 124, 247, 132, 8, 12, 244, 244, 11, 204, 240, 52, 15,
        201, 237, 55, 18, 15, 235, 241, 20, 169, 232, 87, 23, 161, 230, 95, 25,
        255, 228, 1, 27, 202, 227, 54, 28, 6, 227, 250, 28, 182, 226, 74, 29,
        220, 226, 36, 29, 120, 227, 136, 28, 134, 228, 122, 27, 2, 230, 254, 25,
        231, 231, 25, 24, 45, 234, 211, 21, 202, 236, 54, 19, 181, 239, 75, 16,
        225, 242, 31, 13, 66, 246, 190, 9, 202, 249, 54, 6, 107, 253, 149, 2,
        21, 1, 235, 254, 188, 4, 68, 251, 80, 8, 176, 247, 195, 11, 61, 244,
        6, 15, 250, 240, 13, 18, 243, 237, 204, 20, 52, 235, 54, 23, 202, 232,
        68, 25, 188, 230, 236, 26, 20, 229, 39, 28, 217, 227, 242, 28, 14, 227,
        72, 29, 184, 226, 41, 29, 215, 226, 148, 28, 108, 227, 141, 27, 115, 228,
        22, 26, 234, 229, 55, 24, 201, 231, 247, 21, 9, 234, 94, 19, 162, 236,
        120, 16, 136, 239, 79, 13, 177, 242, 241, 9, 15, 246, 106, 6, 150, 249,
        202, 2, 54, 253, 32, 255, 224, 0, 120, 251, 136, 4, 227, 247, 29, 8,
        110, 244, 146, 11, 40, 241, 216, 14, 29, 238, 227, 17, 90, 235, 166, 20,
        234, 232, 22, 23, 215, 230, 41, 25, 41, 229, 215, 26, 231, 227, 25, 28,
        22, 227, 234, 28, 185, 226, 71, 29, 210, 226, 46, 29, 96, 227, 160, 28,
        97, 228, 159, 27, 209, 229, 47, 26, 171, 231, 85, 24, 230, 233, 26, 22,
        122, 236, 134, 19, 92, 239, 164, 16, 130, 242, 126, 13, 221, 245, 35, 10,
        98, 249, 158, 6, 0, 253, 0, 3, 170, 0, 86, 255, 83, 4, 173, 251,
        234, 7, 22, 248, 97, 11, 159, 244, 170, 14, 86, 241, 185, 17, 71, 238,
        128, 20, 128, 235, 245, 22, 11, 233, 13, 25, 243, 230, 193, 26, 63, 229,
        9, 28, 247, 227, 225, 28, 31, 227, 68, 29, 188, 226, 50, 29, 206, 226,
        171, 28, 85, 227, 176, 27, 80, 228, 70, 26, 186, 229, 115, 24, 141, 231,
        61, 22, 195, 233, 174, 19, 82, 236, 208, 16, 48, 239, 174, 13, 82, 242,
        85, 10, 171, 245, 210, 6, 46, 249, 53, 3, 203, 252, 139, 255, 117, 0,
        226, 251, 30, 4, 74, 248, 182, 7, 209, 244, 47, 11, 132, 241, 124, 14,
        114, 238, 142, 17, 167, 235, 89, 20, 45, 233, 211, 22, 15, 231, 241, 24,
        85, 229, 171, 26, 6, 228, 250, 27, 40, 227, 216, 28, 190, 226, 66, 29,
        201, 226, 55, 29, 74, 227, 182, 28, 62, 228, 194, 27, 162, 229, 94, 26,
        112, 231, 144, 24, 160, 233, 96, 22, 43, 236, 213, 19, 5, 239, 251, 16,
        35, 242, 221, 13, 121, 245, 135, 10, 250, 248, 6, 7, 150, 252, 106, 3,
        64, 0, 192, 255, 233, 3, 23, 252, 131, 7, 125, 248, 254, 10, 2, 245,
        77, 14, 179, 241, 99, 17, 157, 238, 51, 20, 205, 235, 178, 22, 78, 233,
        213, 24, 43, 231, 149, 26, 107, 229, 234, 27, 22, 228, 206, 28, 50, 227,
        63, 29, 193, 226, 58, 29, 198, 226, 192, 28, 64, 227, 210, 27, 46, 228,
        117, 26, 139, 229, 173, 24, 83, 231, 130, 22, 126, 233, 252, 19, 4, 236,
        39, 17, 217, 238, 12, 14, 244, 241, 185, 10, 71, 245, 58, 7, 198, 248,
        159, 3, 97, 252, 246, 255, 10, 0, 76, 252, 180, 3, 177, 248, 79, 7,
        52, 245, 204, 10, 225, 241, 31, 14, 200, 238, 56, 17, 244, 235, 12, 20,
        112, 233, 144, 22, 71, 231, 185, 24, 130, 229, 126, 26, 39, 228, 217, 27,
        60, 227, 196, 28, 196, 226, 60, 29, 194, 226, 62, 29, 54, 227, 202, 28,
        29, 228, 227, 27, 116, 229, 140, 26, 54, 231, 202, 24, 92, 233, 164, 22,
        221, 235, 35, 20, 174, 238, 82, 17, 197, 241, 59, 14, 22, 245, 234, 10,
        146, 248, 110, 7, 44, 252, 212, 3, 214, 255, 42, 0, 127, 3, 129, 252,
        27, 7, 229, 248, 155, 10, 101, 245, 240, 13, 16, 242, 13, 17, 243, 238,
        229, 19, 27, 236, 109, 22, 147, 233, 156, 24, 100, 231, 103, 26, 153, 229,
        200, 27, 56, 228, 186, 28, 70, 227, 56, 29, 200, 226, 65, 29, 191, 226,
        212, 28, 44, 227, 243, 27, 13, 228, 162, 26, 94, 229, 230, 24, 26, 231,
        198, 22, 58, 233, 74, 20, 182, 235, 125, 17, 131, 238, 105, 14, 151, 241,
        28, 11, 228, 244, 162, 7, 94, 248, 9, 4, 247, 251, 96, 0, 160, 255,
        182, 252, 74, 3, 25, 249, 231, 6, 151, 245, 105, 10, 63, 242, 193, 13,
        31, 239, 225, 16, 66, 236, 190, 19, 181, 233, 75, 22, 129, 231, 127, 24,
        176, 229, 80, 26, 73, 228, 183, 27, 81, 227, 175, 28, 204, 226, 52, 29,
        189, 226, 67, 29, 35, 227, 221, 28, 253, 227, 3, 28, 72, 229, 184, 26,
        254, 230, 2, 25, 25, 233, 231, 22, 144, 235, 112, 20, 88, 238, 168, 17,
        104, 241, 152, 14, 179, 244, 77, 11, 43, 248, 213, 7, 194, 251, 62, 4,
        107, 255, 149, 0, 21, 3, 235, 252, 179, 6, 77, 249, 55, 10, 201, 245,
        145, 13, 111, 242, 181, 16, 75, 239, 150, 19, 106, 236, 40, 22, 216, 233,
        97, 24, 159, 231, 56, 26, 200, 229, 166, 27, 90, 228, 164, 28, 92, 227,
        48, 29, 208, 226, 70, 29, 186, 226, 230, 28, 26, 227, 18, 28, 238, 227,
        206, 26, 50, 229, 30, 25, 226, 230, 8, 23, 248, 232, 151, 20, 105, 235,
        210, 17, 46, 238, 198, 14, 58, 241, 126, 11, 130, 244, 9, 8, 247, 247,
        114, 4, 142, 251, 203, 0, 53, 255, 32, 253, 224, 2, 129, 249, 127, 6,
        251, 245, 5, 10, 158, 242, 98, 13, 119, 239, 137, 16, 146, 236, 110, 19,
        251, 233, 5, 22, 189, 231, 67, 24, 224, 229, 32, 26, 108, 228, 148, 27,
        103, 227, 153, 28, 213, 226, 43, 29, 184, 226, 72, 29, 17, 227, 239, 28,
        222, 227, 34, 28, 29, 229, 227, 26, 199, 230, 57, 25, 215, 232, 41, 23,
        68, 235, 188, 20, 4, 238, 252, 17, 12, 241, 244, 14, 81, 244, 175, 11,
        196, 247, 60, 8, 89, 251, 167, 4, 0, 255, 0, 1, 170, 2, 86, 253,
        75, 6, 181, 249, 210, 9, 46, 246, 50, 13, 206, 242, 93, 16, 163, 239,
        70, 19, 186, 236, 226, 21, 30, 234, 37, 24, 219, 231, 8, 26, 248, 229,
        130, 27, 126, 228, 141, 28, 115, 227, 38, 29, 218, 226, 73, 29, 183, 226,
        247, 28, 9, 227, 48, 28, 208, 227, 248, 26, 8, 229, 84, 25, 172, 230,
        74, 23, 182, 232, 226, 20, 30, 235, 38, 18, 218, 237, 34, 15, 222, 240,
        224, 11, 32, 244, 111, 8, 145, 247, 220, 4, 36, 251, 53, 1, 203, 254,
        139, 253, 117, 2, 233, 249, 23, 6, 96, 246, 160, 9, 254, 242, 2, 13,
        207, 239, 49, 16, 226, 236, 30, 19, 66, 234, 190, 21, 249, 231, 7, 24,
        17, 230, 239, 25, 145, 228, 111, 27, 127, 227, 129, 28, 224, 226, 32, 29,
        182, 226, 74, 29, 1, 227, 255, 28, 193, 227, 63, 28, 243, 228, 13, 27,
        145, 230, 111, 25, 150, 232, 106, 23, 249, 234, 7, 21, 176, 237, 80, 18,
        177, 240, 79, 15, 239, 243, 17, 12, 94, 247, 162, 8, 239, 250, 17, 5,
        149, 254, 107, 1, 64, 2, 192, 253, 227, 5, 29, 250, 109, 9, 147, 246,
        210, 12, 46, 243, 4, 16, 252, 239, 245, 18, 11, 237, 154, 21, 102, 234,
        232, 23, 24, 232, 214, 25, 42, 230, 92, 27, 164, 228, 116, 28, 140, 227,
        26, 29, 230, 226, 75, 29, 181, 226, 6, 29, 250, 226, 77, 28, 179, 227,
        33, 27, 223, 228, 137, 25, 119, 230, 138, 23, 118, 232, 44, 21, 212, 234,
        122, 18, 134, 237, 125, 15, 131, 240, 66, 12, 190, 243, 213, 8, 43, 247,
        69, 5, 187, 250, 160, 1, 96, 254, 245, 253, 11, 2, 82, 250, 174, 5,
        197, 246, 59, 9, 94, 243, 162, 12, 41, 240, 215, 15, 52, 237, 204, 18,
        138, 234, 118, 21, 55, 232, 201, 23, 67, 230, 189, 25, 183, 228, 73, 27,
        152, 227, 104, 28, 236, 226, 20, 29, 181, 226, 75, 29, 243, 226, 13, 29,
        166, 227, 90, 28, 203, 228, 53, 27, 93, 230, 163, 25, 86, 232, 170, 23,
        175, 234, 81, 21, 93, 237, 163, 18, 86, 240, 170, 15, 142, 243, 114, 12,
        248, 246, 8, 9, 134, 250, 122, 5,
    };

    // mono.mp3: 1924 Byte
    const unsigned char kMonoMp3[] = {
        73, 68, 51, 4, 0, 0, 0, 0, 0, 35, 84, 83, 83, 69, 0, 0,
        0, 15, 0, 0, 3, 76, 97, 118, 102, 54, 48, 46, 49, 54, 46, 49,
        48, 48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 243, 128,
        192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 73, 110, 102, 111, 0, 0,
        0, 15, 0, 0, 0, 8, 0, 0, 7, 87, 0, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 85, 85, 85, 85, 85, 85, 85, 85, 85,
        85, 85, 85, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
        142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 170, 170, 170, 170,
        170, 170, 170, 170, 170, 170, 170, 170, 170, 199, 199, 199, 199, 199, 199, 199,
        199, 199, 199, 199, 199, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
        227, 227, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0,
        0, 0, 76, 97, 118, 99, 54, 48, 46, 51, 49, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 36, 2, 213, 0, 0, 0, 0, 0, 0,
        7, 87, 123, 36, 101, 143, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 243, 128,
        196, 0, 40, 192, 174, 124, 5, 90, 72, 1, 1, 140, 120, 243, 30, 60,
        200, 147, 50, 100, 204, 153, 19, 30, 29, 26, 196, 0, 140, 96, 195, 62,
        148, 216, 191, 57, 117, 206, 254, 51, 195, 108, 232, 195, 54, 40, 76, 152,
        114, 215, 160, 162, 164, 80, 4, 84, 72, 133, 4, 93, 141, 114, 28, 13,
        133, 193, 48, 76, 19, 4, 197, 100, 237, 193, 68, 8, 16, 32, 64, 35,
        161, 225, 231, 255, 128, 59, 204, 255, 240, 55, 255, 199, 0, 255, 192, 12,
        127, 248, 120, 1, 159, 0, 12, 61, 249, 135, 128, 0, 112, 0, 12, 60,
        60, 60, 60, 0, 0, 0, 0, 12, 60, 60, 60, 60, 0, 0, 0, 1,
        24, 120, 120, 120, 240, 0, 7, 113, 24, 123, 255, 192, 63, 255, 48, 240,
        240, 240, 240, 0, 0, 0, 4, 97, 225, 225, 227, 192, 0, 0, 0, 0,
        195, 195, 195, 195, 192, 0, 0, 0, 17, 135, 135, 135, 143, 18, 24, 50,
        134, 81, 135, 104, 99, 51, 33, 0, 87, 178, 227, 0, 224, 61, 50, 93,
        38, 131, 18, 200, 219, 48, 58, 4, 163, 54, 66, 82, 5, 255, 243, 130,
        196, 32, 49, 3, 110, 100, 3, 158, 160, 0, 18, 225, 145, 117, 102, 26,
        240, 137, 193, 137, 120, 158, 27, 81, 130, 161, 132, 240, 8, 25, 145, 139,
        1, 129, 160, 29, 129, 184, 234, 7, 178, 232, 29, 108, 100, 209, 185, 175,
        3, 26, 168, 13, 90, 96, 52, 235, 145, 100, 184, 2, 19, 3, 36, 68,
        12, 145, 144, 18, 17, 78, 138, 252, 12, 56, 64, 48, 226, 129, 188, 160,
        217, 32, 0, 15, 255, 14, 72, 44, 136, 27, 240, 126, 161, 138, 67, 0,
        138, 75, 255, 240, 213, 193, 145, 134, 52, 65, 81, 5, 71, 40, 80, 66,
        130, 33, 191, 255, 248, 185, 69, 202, 67, 135, 56, 115, 137, 145, 205, 28,
        210, 145, 22, 34, 198, 36, 87, 255, 255, 255, 34, 165, 34, 44, 79, 24,
        145, 82, 233, 145, 120, 217, 101, 214, 82, 73, 255, 255, 255, 255, 209, 86,
        98, 148, 201, 20, 76, 82, 72, 188, 138, 36, 201, 145, 120, 154, 140, 167,
        233, 128, 24, 0, 72, 16, 0, 112, 176, 11, 6, 6, 248, 31, 134, 7,
        232, 162, 198, 15, 88, 124, 134, 32, 88, 51, 134, 24, 88, 238, 255, 243,
        130, 196, 32, 22, 208, 90, 44, 1, 223, 0, 0, 70, 85, 201, 35, 199,
        225, 26, 203, 38, 89, 33, 15, 224, 161, 219, 12, 9, 240, 32, 140, 17,
        80, 80, 12, 10, 80, 11, 204, 6, 224, 22, 204, 3, 80, 14, 140, 0,
        80, 6, 64, 64, 30, 206, 254, 115, 191, 245, 255, 246, 255, 255, 255, 255,
        255, 247, 255, 255, 255, 250, 213, 128, 3, 182, 219, 109, 182, 214, 150, 209,
        0, 12, 210, 86, 229, 181, 246, 126, 214, 22, 17, 20, 194, 160, 225, 39,
        6, 38, 10, 165, 244, 111, 163, 185, 14, 79, 67, 14, 68, 56, 14, 68,
        221, 196, 249, 212, 235, 93, 26, 140, 214, 206, 56, 77, 146, 83, 152, 200,
        80, 216, 80, 4, 194, 138, 152, 163, 154, 69, 4, 8, 163, 102, 8, 230,
        72, 229, 235, 97, 0, 1, 0, 68, 32, 122, 248, 2, 0, 6, 1, 88,
        23, 153, 108, 203, 110, 189, 217, 153, 119, 208, 6, 223, 176, 4, 36, 38,
        4, 28, 202, 209, 81, 77, 36, 44, 13, 33, 212, 222, 92, 185, 20, 17,
        93, 202, 151, 34, 164, 101, 146, 150, 30, 169, 218, 124, 165, 135, 172, 255,
        243, 130, 196, 136, 76, 236, 14, 177, 190, 222, 88, 223, 119, 94, 56, 195,
        24, 131, 241, 54, 206, 24, 132, 82, 38, 195, 218, 252, 141, 244, 103, 12,
        189, 24, 224, 4, 2, 133, 35, 152, 19, 10, 212, 136, 96, 76, 71, 104,
        64, 3, 132, 198, 68, 0, 104, 88, 67, 16, 193, 185, 109, 32, 144, 21,
        171, 56, 16, 4, 69, 5, 112, 110, 35, 162, 43, 131, 114, 123, 196, 129,
        0, 153, 17, 32, 72, 38, 50, 75, 17, 203, 112, 18, 4, 115, 246, 4,
        129, 32, 241, 56, 150, 37, 168, 59, 18, 201, 241, 156, 9, 4, 200, 142,
        4, 130, 99, 167, 98, 89, 253, 140, 4, 179, 248, 9, 2, 65, 228, 36,
        178, 122, 134, 201, 100, 247, 216, 36, 19, 28, 162, 197, 156, 218, 245, 254,
        194, 247, 241, 133, 142, 106, 245, 238, 106, 245, 239, 226, 197, 148, 197, 139,
        58, 235, 215, 253, 23, 191, 236, 44, 114, 173, 175, 115, 91, 94, 254, 44,
        88, 230, 44, 89, 77, 94, 189, 34, 10, 219, 109, 182, 201, 36, 73, 115,
        34, 178, 69, 55, 202, 218, 201, 1, 128, 40, 90, 12, 21, 50, 97, 179,
        255, 243, 130, 196, 24, 49, 2, 54, 144, 126, 219, 13, 45, 223, 5, 51,
        49, 33, 128, 48, 80, 10, 0, 97, 14, 19, 89, 119, 108, 67, 74, 153,
        151, 79, 184, 44, 229, 77, 161, 181, 49, 69, 85, 211, 40, 92, 202, 153,
        137, 82, 50, 165, 220, 253, 33, 130, 32, 116, 154, 132, 56, 136, 167, 169,
        132, 33, 25, 81, 200, 130, 78, 140, 228, 73, 38, 196, 116, 37, 19, 162,
        58, 50, 62, 243, 147, 19, 222, 100, 197, 109, 23, 25, 61, 167, 43, 158,
        210, 36, 81, 96, 96, 169, 56, 145, 45, 146, 36, 102, 72, 145, 195, 137,
        37, 166, 146, 73, 205, 34, 139, 28, 74, 88, 226, 84, 230, 145, 151, 53,
        28, 99, 146, 217, 170, 217, 35, 57, 4, 228, 21, 136, 46, 34, 161, 13,
        140, 54, 41, 161, 70, 133, 28, 10, 104, 43, 130, 158, 20, 216, 47, 2,
        120, 21, 208, 94, 138, 200, 111, 6, 226, 42, 17, 176, 142, 138, 108, 83,
        97, 62, 19, 193, 92, 19, 225, 93, 5, 252, 177, 173, 29, 37, 0, 0,
        193, 4, 20, 108, 192, 4, 0, 108, 192, 5, 0, 40, 192, 35, 0, 96,
        192, 255, 243, 130, 196, 24, 26, 72, 238, 16, 0, 255, 4, 104, 120, 1,
        228, 253, 40, 99, 0, 230, 66, 48, 49, 216, 72, 58, 147, 77, 90, 47,
        67, 90, 206, 100, 116, 239, 217, 128, 212, 251, 190, 154, 93, 173, 240, 88,
        6, 241, 131, 117, 184, 5, 111, 177, 110, 85, 212, 86, 238, 146, 105, 123,
        166, 197, 78, 70, 57, 177, 245, 192, 137, 79, 88, 145, 219, 208, 234, 213,
        136, 131, 71, 215, 226, 46, 193, 172, 178, 234, 203, 44, 1, 101, 117, 213,
        96, 5, 109, 182, 219, 109, 177, 163, 227, 69, 47, 26, 2, 140, 112, 161,
        0, 21, 38, 195, 139, 136, 153, 70, 81, 144, 191, 99, 88, 25, 25, 211,
        93, 184, 52, 199, 220, 88, 2, 105, 167, 218, 148, 27, 96, 134, 77, 136,
        42, 121, 142, 72, 14, 104, 104, 224, 217, 13, 121, 40, 177, 162, 67, 201,
        51, 189, 3, 140, 200, 47, 209, 144, 201, 130, 99, 26, 166, 1, 1, 114,
        150, 109, 149, 22, 189, 125, 23, 97, 132, 160, 186, 157, 32, 163, 27, 71,
        246, 54, 157, 107, 225, 88, 23, 154, 215, 90, 12, 145, 129, 175, 197, 206,
        223, 174, 255, 243, 130, 196, 114, 70, 243, 162, 149, 190, 214, 24, 247, 71,
        49, 115, 190, 108, 50, 16, 195, 32, 86, 31, 22, 97, 144, 235, 59, 141,
        51, 136, 105, 156, 68, 64, 249, 120, 14, 20, 194, 178, 224, 86, 64, 17,
        7, 177, 220, 120, 29, 7, 177, 220, 123, 31, 199, 3, 65, 204, 220, 112,
        64, 18, 205, 68, 53, 66, 65, 232, 150, 184, 72, 90, 36, 45, 18, 211,
        19, 18, 147, 208, 137, 136, 68, 196, 229, 180, 133, 130, 185, 108, 224, 220,
        168, 160, 174, 172, 144, 136, 150, 140, 205, 227, 7, 76, 222, 48, 90, 102,
        180, 205, 163, 6, 76, 208, 143, 33, 60, 78, 126, 144, 241, 58, 179, 133,
        7, 10, 14, 213, 156, 68, 119, 25, 131, 198, 14, 153, 188, 97, 83, 59,
        31, 180, 121, 9, 252, 7, 144, 159, 235, 248, 227, 111, 177, 215, 137, 191,
        99, 155, 246, 57, 142, 189, 169, 91, 227, 150, 115, 95, 199, 53, 166, 153,
        105, 150, 210, 35, 73, 154, 60, 51, 193, 10, 253, 0, 73, 67, 58, 72,
        66, 20, 203, 19, 11, 10, 52, 101, 129, 139, 197, 170, 153, 184, 64, 237,
        167, 111, 177, 255, 243, 130, 196, 26, 47, 113, 118, 88, 80, 211, 204, 213,
        137, 206, 15, 194, 116, 102, 3, 81, 154, 64, 227, 160, 140, 104, 98, 192,
        0, 81, 36, 221, 66, 144, 206, 20, 161, 29, 41, 67, 136, 136, 1, 40,
        107, 3, 100, 142, 11, 227, 92, 91, 79, 2, 82, 140, 32, 202, 48, 212,
        161, 130, 28, 115, 134, 165, 22, 38, 40, 130, 20, 147, 37, 169, 131, 40,
        237, 66, 143, 212, 37, 20, 133, 34, 80, 148, 82, 20, 166, 71, 31, 171,
        39, 243, 73, 250, 160, 104, 6, 76, 140, 18, 178, 48, 70, 9, 58, 44,
        147, 162, 232, 177, 80, 100, 169, 81, 37, 68, 153, 42, 36, 160, 80, 162,
        194, 139, 36, 40, 178, 197, 154, 81, 67, 77, 20, 113, 167, 28, 105, 82,
        69, 10, 176, 218, 44, 155, 97, 190, 229, 142, 88, 227, 150, 165, 188, 210,
        205, 44, 183, 177, 167, 28, 105, 206, 247, 185, 238, 97, 182, 27, 199, 157,
        255, 238, 88, 237, 76, 65, 77, 69, 51, 46, 49, 48, 48, 85, 85, 85,
        85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
        85, 85, 85, 85,
    };

    // --- WAV --------------------------------------------------------------
    const Sound mono = decodeWav(kMono16Wav, sizeof(kMono16Wav));
    check(mono.ok, "16-Bit-Mono laesst sich lesen");
    check(mono.sampleRate == 22050, "22050 Hz");
    check(mono.channels == 1, "ein Kanal");
    check(mono.frameCount() == 3307, "3307 Abtastwerte");
    check(std::fabs(mono.durationMs() - 150.0f) < 1.0f, "etwa 150 ms");
    std::printf("  WAV mono16: %d Hz, %d Kanal, %.0f ms\n", mono.sampleRate,
                mono.channels, static_cast<double>(mono.durationMs()));

    // Der Pegelwechsel muss an der richtigen Stelle sitzen.
    auto peakBetween = [](const Sound& s, size_t from, size_t to) {
        int worst = 0;
        for (size_t i = from; i < to && i < s.samples.size(); ++i) {
            worst = std::max(worst, std::abs(static_cast<int>(s.samples[i])));
        }
        return worst;
    };
    const int firstHalf = peakBetween(mono, 100, 1500);
    const int secondHalf = peakBetween(mono, 1800, 3200);
    check(firstHalf > 20000, "die erste Haelfte ist laut");
    check(secondHalf < firstHalf / 2, "die zweite deutlich leiser");
    std::printf("  Pegel: erste Haelfte %d, zweite %d\n", firstHalf, secondHalf);

    const Sound stereo = decodeWav(kStereo16Wav, sizeof(kStereo16Wav));
    check(stereo.ok && stereo.channels == 2, "Stereo laesst sich lesen");
    check(stereo.frameCount() == mono.frameCount(), "gleich viele Abtastwerte");
    // Der rechte Kanal ist der gespiegelte linke — so wurde die Datei gebaut.
    bool mirrored = true;
    for (size_t i = 100; i < 2000; ++i) {
        if (stereo.samples[i * 2] != -stereo.samples[i * 2 + 1]) mirrored = false;
    }
    check(mirrored, "die Kanaele stehen verschraenkt und richtig herum");

    // Acht Bit sind vorzeichenlos mit 128 als Mitte — als einzige Breite.
    // Wer das uebersieht, bekommt ein Knacken und nur die halbe Aussteuerung.
    const Sound eightBit = decodeWav(kMono8Wav, sizeof(kMono8Wav));
    check(eightBit.ok, "8-Bit-WAV laesst sich lesen");
    check(eightBit.frameCount() == mono.frameCount(), "gleich lang");
    int nearZero = 0;
    for (size_t i = 0; i < 60; ++i) {
        if (std::abs(static_cast<int>(eightBit.samples[i])) < 4000) ++nearZero;
    }
    check(nearZero > 5, "der Sinus beginnt bei null, nicht bei Vollausschlag");
    check(peakBetween(eightBit, 100, 1500) > 15000, "und steuert richtig aus");

    // --- MP3 --------------------------------------------------------------
    const Sound mp3 = decodeMp3(kMonoMp3, sizeof(kMonoMp3));
    check(mp3.ok, "MP3 laesst sich lesen");
    check(mp3.sampleRate == 22050, "MP3: 22050 Hz");
    check(mp3.channels == 1, "MP3: ein Kanal");
    // MP3 fuegt vorn und hinten Stille an — die Laenge stimmt nie genau.
    check(mp3.frameCount() > mono.frameCount() / 2, "MP3 hat brauchbare Laenge");
    std::printf("  MP3: %d Hz, %d Kanal, %.0f ms (WAV: %.0f ms)\n",
                mp3.sampleRate, mp3.channels,
                static_cast<double>(mp3.durationMs()),
                static_cast<double>(mono.durationMs()));

    // Der Pegelwechsel muss auch im MP3 zu finden sein.
    //
    // Aber NICHT bei der Haelfte: der Kodierer haengt vorn und hinten Stille
    // an (Encoder-Delay und Auffuellen auf ganze Rahmen), hier 235 ms fuer
    // 150 ms Inhalt. Wer die Datei einfach halbiert, misst Stille gegen
    // Signal und bekommt Unsinn — mein erster Anlauf tat genau das.
    //
    // Also erst den hoerbaren Bereich suchen, dann DEN halbieren.
    {
        size_t contentStart = 0, contentEnd = mp3.frameCount();
        while (contentStart < mp3.frameCount() &&
               std::abs(static_cast<int>(mp3.samples[contentStart])) < 2000) {
            ++contentStart;
        }
        while (contentEnd > contentStart &&
               std::abs(static_cast<int>(mp3.samples[contentEnd - 1])) < 2000) {
            --contentEnd;
        }
        check(contentEnd > contentStart, "das MP3 hat einen hoerbaren Bereich");

        const size_t middle = contentStart + (contentEnd - contentStart) / 2;
        // Mit etwas Abstand zur Mitte messen: der Uebergang selbst ist beim
        // MP3 verschmiert, weil ein Rahmen 1152 Abtastwerte umfasst und der
        // Pegelwechsel mitten in einem liegt.
        const size_t margin = (contentEnd - contentStart) / 8;
        const int loud = peakBetween(mp3, contentStart + margin, middle - margin);
        const int quiet = peakBetween(mp3, middle + margin, contentEnd - margin);
        check(loud > 15000, "MP3: die erste Haelfte ist laut");
        check(quiet < loud * 3 / 4, "MP3: die zweite ist leiser");
        std::printf("  MP3: %zu ms Vorlauf, hoerbar von %zu bis %zu, "
                    "Pegel %d dann %d\n",
                    contentStart * 1000 / 22050, contentStart, contentEnd, loud,
                    quiet);
    }

    // --- Formaterkennung am Inhalt ----------------------------------------
    check(sniff(kMono16Wav, sizeof(kMono16Wav)) == Format::Wav, "WAV erkannt");
    check(sniff(kMonoMp3, sizeof(kMonoMp3)) == Format::Mp3, "MP3 erkannt");
    check(sniff(nullptr, 0) == Format::Unknown, "nichts ist nichts");
    check(decode(kMonoMp3, sizeof(kMonoMp3)).ok, "decode() leitet MP3 weiter");
    check(decode(kMono16Wav, sizeof(kMono16Wav)).ok, "und WAV ebenso");

    // --- Kaputte Dateien --------------------------------------------------
    check(!decodeWav(nullptr, 0).ok, "Nullzeiger");
    check(!decodeWav(kMono16Wav, 10).ok, "zu kurz");
    {
        // Ein WAV mit unbekannter Kompression wird beim Namen genannt statt
        // als Rauschen ausgegeben.
        std::vector<unsigned char> adpcm(kMono16Wav, kMono16Wav + sizeof(kMono16Wav));
        adpcm[20] = 0x11;  // WAVE_FORMAT_DVI_ADPCM
        const Sound result = decodeWav(adpcm.data(), adpcm.size());
        check(!result.ok, "ein komprimiertes WAV wird abgewiesen");
        check(result.error.find("compressed") != std::string::npos,
              "und beim Namen genannt");
    }
    {
        // Mitten in den Daten abgeschnitten: nehmen, was da ist.
        const Sound result = decodeWav(kMono16Wav, sizeof(kMono16Wav) / 2);
        check(result.ok, "ein abgeschnittenes WAV liefert, was da ist");
        check(result.frameCount() < mono.frameCount(), "und ist kuerzer");
    }
    {
        unsigned state = 77;
        int survived = 0;
        for (int attempt = 0; attempt < 200; ++attempt) {
            std::vector<unsigned char> broken(kMono16Wav,
                                              kMono16Wav + sizeof(kMono16Wav));
            for (int k = 0; k < 6; ++k) {
                state = state * 1664525u + 1013904223u;
                broken[(state >> 8) % broken.size()] =
                    static_cast<unsigned char>(state >> 20);
            }
            const Sound result = decodeWav(broken.data(), broken.size());
            if (!result.ok || result.samples.size() <= sizeof(kMono16Wav)) ++survived;
        }
        check(survived == 200, "zweihundert verbogene WAVs, keines bricht aus");
    }
    {
        unsigned state = 31337;
        int survived = 0;
        for (int attempt = 0; attempt < 100; ++attempt) {
            std::vector<unsigned char> broken(kMonoMp3, kMonoMp3 + sizeof(kMonoMp3));
            for (int k = 0; k < 6; ++k) {
                state = state * 1664525u + 1013904223u;
                broken[(state >> 8) % broken.size()] =
                    static_cast<unsigned char>(state >> 20);
            }
            const Sound result = decodeMp3(broken.data(), broken.size());
            if (result.samples.size() < 48000u * 600) ++survived;
        }
        check(survived == 100, "hundert verbogene MP3s, keines bricht aus");
    }
}

void testFinishingTouches() {
    std::cout << "== Restliches ==\n";

    // --- Wandtexturen, gerechnet statt geladen ----------------------------
    //
    // Das Original liefert brick.jpg, dirt.jpg und stucco.jpg mit; wir haben
    // sie nicht. Ein Menuepunkt, der nichts tut, ist schlechter als ein
    // gerechnetes Muster.
    {
        using efx::scene::WallTexture;
        using efx::scene::buildWallTexture;
        const uint32_t base = efx::scene::rgba(180, 170, 160);

        check(buildWallTexture(WallTexture::None, 64, base).empty(),
              "keine Textur ergibt nichts");
        check(buildWallTexture(WallTexture::Brick, 0, base).empty(),
              "Groesse null ebenso");
        check(buildWallTexture(WallTexture::Brick, 4096, base).empty(),
              "und eine unsinnige Groesse auch");

        for (auto kind : {WallTexture::Brick, WallTexture::Dirt,
                          WallTexture::Stucco}) {
            const auto texture = buildWallTexture(kind, 64, base);
            check(texture.size() == 64u * 64 * 4, "64 x 64 in RGBA");

            // Sie muss Struktur haben — eine gleichmaessige Flaeche waere
            // nutzlos, und genau das kommt heraus, wenn die Rechnung
            // schiefgeht.
            int lowest = 255, highest = 0;
            for (size_t i = 0; i < texture.size(); i += 4) {
                lowest = std::min(lowest, static_cast<int>(texture[i]));
                highest = std::max(highest, static_cast<int>(texture[i]));
            }
            check(highest - lowest > 20, "sie hat sichtbare Struktur");
            // Voll deckend, sonst sieht man durch die Wand.
            bool opaque = true;
            for (size_t i = 3; i < texture.size(); i += 4) {
                if (texture[i] != 255) opaque = false;
            }
            check(opaque, "und ist voll deckend");
        }

        // Ziegel muessen kontrastreicher sein als Putz — sonst stimmt die
        // Zuordnung nicht, und man merkt es nur beim Hinsehen.
        auto contrastOf = [&](WallTexture kind) {
            const auto texture = buildWallTexture(kind, 64, base);
            int lowest = 255, highest = 0;
            for (size_t i = 0; i < texture.size(); i += 4) {
                lowest = std::min(lowest, static_cast<int>(texture[i]));
                highest = std::max(highest, static_cast<int>(texture[i]));
            }
            return highest - lowest;
        };
        check(contrastOf(WallTexture::Brick) > contrastOf(WallTexture::Stucco),
              "Ziegel sind kontrastreicher als Putz");
        std::printf("  Wandtexturen: Ziegel %d, Erde %d, Putz %d Stufen Kontrast\n",
                    contrastOf(WallTexture::Brick), contrastOf(WallTexture::Dirt),
                    contrastOf(WallTexture::Stucco));

        // Nahtlos kachelbar: das Muster darf nur von x und y abhaengen,
        // sonst sieht man an jeder Kachelgrenze eine Kante.
        const auto once = buildWallTexture(WallTexture::Brick, 64, base);
        const auto again = buildWallTexture(WallTexture::Brick, 64, base);
        check(once == again, "zweimal gerechnet ergibt dasselbe");
    }

    // --- Zuletzt geoeffnete Dateien ---------------------------------------
    {
        efx::layout::Settings settings;
        check(settings.recentFiles.empty(), "am Anfang leer");

        settings.addRecentFile("C:/a.efx");
        settings.addRecentFile("C:/b.efx");
        check(settings.recentFiles.size() == 2, "zwei Eintraege");
        check(settings.recentFiles[0] == "C:/b.efx", "der neueste steht vorn");

        // Eine schon bekannte Datei wandert nach vorn statt sich zu
        // verdoppeln — sonst steht dieselbe achtmal da, sobald jemand daran
        // arbeitet.
        settings.addRecentFile("C:/a.efx");
        check(settings.recentFiles.size() == 2, "keine Verdopplung");
        check(settings.recentFiles[0] == "C:/a.efx", "sie steht jetzt vorn");

        settings.addRecentFile("");
        check(settings.recentFiles.size() == 2, "ein leerer Pfad wird ignoriert");

        for (int i = 0; i < 20; ++i) {
            settings.addRecentFile("C:/datei" + std::to_string(i) + ".efx");
        }
        check(settings.recentFiles.size() ==
                  efx::layout::Settings::kMaxRecentFiles,
              "hoechstens acht");
        check(settings.recentFiles[0] == "C:/datei19.efx", "der letzte steht vorn");

        // Und sie ueberleben das Speichern.
        const std::string text = settings.toIni();
        const auto back = efx::layout::Settings::fromIni(text);
        check(back.recentFiles == settings.recentFiles,
              "die Liste ueberlebt einen Rundlauf");
        std::printf("  Zuletzt geoeffnet: %zu Eintraege, Rundlauf gleich\n",
                    back.recentFiles.size());
    }
}

void testTimeline() {
    std::cout << "== Zeitleiste ==\n";
    using namespace efx::timeline;

    Clock clock;
    clock.setDuration(1000.0f);
    check(clock.state() == State::Stopped, "sie steht am Anfang");
    check(clock.timeMs() == 0.0f, "bei null");

    // Angehalten bewegt sie sich nicht.
    clock.advance(100.0f);
    check(clock.timeMs() == 0.0f, "angehalten laeuft nichts");

    // In kleinen Schritten, wie es die Oberflaeche auch tut: ein Aufruf je
    // Bild. Grosse Einzelschritte werden absichtlich gedeckelt (siehe unten),
    // ein Test mit advance(250) prueft also den Deckel und nicht das Laufen.
    clock.play();
    for (int i = 0; i < 5; ++i) clock.advance(50.0f);
    check(std::fabs(clock.timeMs() - 250.0f) < 0.01f, "abspielend laeuft sie mit");
    check(std::fabs(clock.progress() - 0.25f) < 0.001f, "der Anteil stimmt");

    // Pause haelt an, ohne die Stelle zu verlieren.
    clock.pause();
    for (int i = 0; i < 5; ++i) clock.advance(50.0f);
    check(std::fabs(clock.timeMs() - 250.0f) < 0.01f, "Pause haelt die Stelle");
    clock.togglePause();
    for (int i = 0; i < 5; ++i) clock.advance(50.0f);
    check(std::fabs(clock.timeMs() - 500.0f) < 0.01f, "und laeuft danach weiter");

    // Geschwindigkeit.
    clock.setSpeed(0.5f);
    clock.advance(100.0f);
    check(std::fabs(clock.timeMs() - 550.0f) < 0.01f,
          "halbe Geschwindigkeit, halber Fortschritt");
    clock.setSpeed(2.0f);
    clock.advance(100.0f);
    check(std::fabs(clock.timeMs() - 750.0f) < 0.01f, "doppelte ebenso");
    // Der Deckel greift auf die WANDUHRZEIT, vor der Geschwindigkeit — sonst
    // koennte man ihn mit hoher Geschwindigkeit umgehen.
    clock.setSpeed(8.0f);
    const float beforeCap = clock.timeMs();
    clock.advance(5000.0f);
    check(clock.timeMs() - beforeCap <= 100.0f * 8.0f + 0.01f,
          "auch bei hoher Geschwindigkeit bleibt der Deckel wirksam");
    clock.setSpeed(1.0f);

    // Grenzen: die Uhr darf nicht stehenbleiben und nicht davonrennen.
    clock.setSpeed(0.0f);
    check(clock.speed() > 0.0f, "Geschwindigkeit null wird abgefangen");
    clock.setSpeed(-5.0f);
    check(clock.speed() > 0.0f, "negative auch");
    clock.setSpeed(1000.0f);
    check(clock.speed() <= 8.0f, "und nach oben gedeckelt");
    clock.setSpeed(1.0f);

    // Am Ende: wiederholen, halten oder stehenbleiben.
    {
        Clock repeating;
        repeating.setDuration(1000.0f);
        repeating.setEndMode(EndMode::Repeat);
        repeating.play();
        for (int i = 0; i < 11; ++i) repeating.advance(100.0f);
        check(repeating.playing(), "wiederholend laeuft sie weiter");
        check(repeating.timeMs() < 1000.0f, "und ist wieder am Anfang");
        check(repeating.consumeWrapped(), "der Durchlauf wird gemeldet");
        check(!repeating.consumeWrapped(), "aber nur einmal");
        // Der Ueberhang wird mitgenommen: sonst ruckelt jede Wiederholung.
        check(repeating.timeMs() > 0.0f, "der Ueberhang geht nicht verloren");
        std::printf("  nach 1100 ms bei 1000 ms Dauer: %.0f ms\n",
                    static_cast<double>(repeating.timeMs()));
    }
    {
        Clock holding;
        holding.setDuration(500.0f);
        holding.setEndMode(EndMode::Hold);
        holding.play();
        for (int i = 0; i < 20; ++i) holding.advance(100.0f);
        check(std::fabs(holding.timeMs() - 500.0f) < 0.01f,
              "haltend bleibt sie am Ende stehen");
        check(holding.playing(), "und gilt weiter als laufend");
    }
    {
        Clock stopping;
        stopping.setDuration(500.0f);
        stopping.setEndMode(EndMode::Stop);
        stopping.play();
        for (int i = 0; i < 20; ++i) stopping.advance(100.0f);
        check(!stopping.playing(), "anhaltend hoert sie auf");
    }

    // Spulen haelt an — wer eine Stelle sucht, will sie ansehen.
    {
        Clock scrubbing;
        scrubbing.setDuration(1000.0f);
        scrubbing.play();
        scrubbing.scrubTo(400.0f);
        check(!scrubbing.playing(), "Spulen haelt die Wiedergabe an");
        check(std::fabs(scrubbing.timeMs() - 400.0f) < 0.01f, "an der Stelle");
        scrubbing.scrubTo(-100.0f);
        check(scrubbing.timeMs() == 0.0f, "vor den Anfang geht nicht");
        scrubbing.scrubTo(99999.0f);
        check(std::fabs(scrubbing.timeMs() - 1000.0f) < 0.01f,
              "hinter das Ende auch nicht");
        scrubbing.setProgress(0.5f);
        check(std::fabs(scrubbing.timeMs() - 500.0f) < 0.01f,
              "ueber den Anteil ebenso");
    }

    // Bild fuer Bild.
    {
        Clock stepping;
        stepping.setDuration(1000.0f);
        stepping.setFrameRate(30.0f);
        check(stepping.totalFrames() == 30, "dreissig Bilder bei einer Sekunde");
        stepping.play();
        stepping.stepFrames(1);
        check(!stepping.playing(), "Bild fuer Bild haelt an");
        check(std::fabs(stepping.timeMs() - 1000.0f / 30.0f) < 0.01f,
              "ein Bild weiter");
        check(stepping.currentFrame() == 1, "und die Anzeige zaehlt mit");
        stepping.stepFrames(9);
        check(stepping.currentFrame() == 10, "zehn Bilder");
        stepping.stepFrames(-4);
        check(stepping.currentFrame() == 6, "und wieder zurueck");
        stepping.stepFrames(-100);
        check(stepping.timeMs() == 0.0f, "vor den Anfang geht nicht");
        stepping.stepFrames(1000);
        check(std::fabs(stepping.timeMs() - 1000.0f) < 0.01f,
              "hinter das Ende auch nicht");
        std::printf("  Bild fuer Bild: %d Bilder bei %.0f Hz\n",
                    stepping.totalFrames(), static_cast<double>(stepping.frameRate()));
    }

    // Abspielen vom Ende beginnt von vorn — sonst passiert auf Klick nichts.
    {
        Clock finished;
        finished.setDuration(500.0f);
        finished.setEndMode(EndMode::Stop);
        finished.play();
        for (int i = 0; i < 10; ++i) finished.advance(100.0f);
        finished.play();
        check(finished.timeMs() == 0.0f, "Abspielen vom Ende beginnt von vorn");
        check(finished.playing(), "und laeuft");
    }

    // Ein Haenger darf nicht durchschlagen.
    {
        Clock hitching;
        hitching.setDuration(10000.0f);
        hitching.play();
        hitching.advance(3000.0f);   // drei Sekunden Standbild
        check(hitching.timeMs() <= 100.0f,
              "ein langer Haenger wird gedeckelt statt uebersprungen");
        std::printf("  nach 3000 ms Haenger: %.0f ms weiter (gedeckelt)\n",
                    static_cast<double>(hitching.timeMs()));
    }

    // Unsinnige Werte.
    {
        Clock odd;
        odd.setDuration(0.0f);
        check(odd.durationMs() > 0.0f, "Dauer null wird abgefangen");
        check(std::isfinite(odd.progress()), "und der Anteil bleibt brauchbar");
        odd.setFrameRate(0.0f);
        check(odd.frameRate() > 0.0f, "Bildrate null ebenso");
        odd.play();
        odd.advance(std::nanf(""));
        check(std::isfinite(odd.timeMs()), "und NaN kommt nicht durch");
    }
}

// Die Auswahl, welche Kacheln in einem Bild neu gezeichnet werden.
//
// Steht hier und nicht in der Oberflaeche, weil sie die einzige Stelle ist,
// an der etwas schiefgehen kann, ohne dass man es sofort sieht: eine Kachel,
// die NIE drankommt, faellt erst nach Sekunden auf.
namespace tilebudget {

struct Choice {
    std::vector<size_t> due;
    size_t cursor = 0;
};

Choice pick(const std::vector<bool>& fresh, size_t budget, size_t cursor) {
    Choice out;
    const size_t count = fresh.size();
    for (size_t i = 0; i < count; ++i) {
        if (fresh[i]) out.due.push_back(i);
    }
    for (size_t n = 0; n < count && out.due.size() < budget; ++n) {
        const size_t i = (cursor + n) % count;
        if (!fresh[i]) out.due.push_back(i);
    }
    out.cursor = count == 0 ? 0 : (cursor + budget) % count;
    return out;
}

}  // namespace tilebudget

void testTileBudget() {
    std::cout << "== Welche Kachel wird neu gezeichnet ==\n";

    // Erst war es \"alle, jedes Bild\" — 9 Bilder je Sekunde bei achtzehn
    // Kacheln. Dann ein Budget von 24 je Bild, reihum. Auch das war noch
    // falsch gedacht: bei achtzehn sichtbaren Kacheln greift ein Deckel von 24
    // gar nicht, es liefen weiter alle.
    //
    // Die Auswahl unten ist die dritte Fassung und die richtige: Standbild als
    // Vorgabe, Bewegung nur unter dem Zeiger. Die Reihum-Rechnung bleibt
    // geprueft, weil sie mit \"Alle animieren\" weiterhin greift.

    constexpr size_t kTiles = 112;   // so viele waren beim Anwender sichtbar
    constexpr size_t kBudget = 24;

    // 1. Der Aufwand je Bild ist gedeckelt — genau darum geht es.
    {
        std::vector<bool> fresh(kTiles, false);
        const auto choice = tilebudget::pick(fresh, kBudget, 0);
        check(choice.due.size() == kBudget, "je Bild hoechstens das Budget");
        std::printf("  %zu Kacheln sichtbar, %zu je Bild -> %.0f%% des Aufwands\n",
                    kTiles, choice.due.size(),
                    100.0 * static_cast<double>(choice.due.size()) /
                        static_cast<double>(kTiles));
    }

    // 2. Und trotzdem kommt JEDE dran. Das ist die Gefahr: eine Kachel, die
    //    durch die Reihum-Rechnung faellt, bleibt fuer immer eingefroren.
    {
        std::vector<bool> fresh(kTiles, false);
        std::vector<int> seen(kTiles, 0);
        size_t cursor = 0;
        const int frames = 60;
        for (int f = 0; f < frames; ++f) {
            const auto choice = tilebudget::pick(fresh, kBudget, cursor);
            for (size_t i : choice.due) ++seen[i];
            cursor = choice.cursor;
        }
        int never = 0, least = frames + 1, most = 0;
        for (int n : seen) {
            if (n == 0) ++never;
            if (n < least) least = n;
            if (n > most) most = n;
        }
        check(never == 0, "keine Kachel bleibt fuer immer stehen");
        check(most - least <= 1, "und alle kommen gleich oft dran");
        std::printf("  nach %d Bildern: seltenste %dx, haeufigste %dx\n",
                    frames, least, most);
    }

    // 3. Neu aufgetauchte gehen VOR. Ihr Platz im Blatt zeigt sonst noch den
    //    Inhalt des Vorgaengers — sichtbar als falsches Bild unter dem Namen.
    {
        std::vector<bool> fresh(kTiles, false);
        fresh[100] = true;
        fresh[101] = true;
        const auto choice = tilebudget::pick(fresh, kBudget, 0);
        const bool has100 = std::find(choice.due.begin(), choice.due.end(),
                                      size_t{100}) != choice.due.end();
        const bool has101 = std::find(choice.due.begin(), choice.due.end(),
                                      size_t{101}) != choice.due.end();
        check(has100 && has101, "neu aufgetauchte Kacheln kommen sofort dran");
        check(choice.due.size() == kBudget,
              "und sprengen das Budget nicht (sie zaehlen mit)");
    }

    // 4. Keine Kachel doppelt in einem Bild — sonst waere es verschwendete
    //    Arbeit an der Stelle, an der gerade gespart werden soll.
    {
        std::vector<bool> fresh(kTiles, false);
        fresh[5] = true;
        auto choice = tilebudget::pick(fresh, kBudget, 3);
        std::sort(choice.due.begin(), choice.due.end());
        const bool unique = std::adjacent_find(choice.due.begin(),
                                               choice.due.end()) ==
                            choice.due.end();
        check(unique, "keine Kachel zweimal im selben Bild");
    }
}

void testTilePlacement() {
    std::cout << "== Plaetze im Vorschaublatt ==\n";

    // Der Fehler, den das hier festhaelt, sieht von aussen so aus: man zeigt
    // auf \"sparks\" und \"flamejet\" faengt an zu laufen. Innen heisst das:
    // zwei Kacheln teilen sich einen Platz im Blatt, oder eine hat gar keinen
    // und zeichnet deshalb auf Platz null.
    using efx::tiles::assign;

    const auto noDuplicates = [](const std::vector<efx::tiles::Placement>& p) {
        std::vector<int> cells;
        for (const auto& one : p) cells.push_back(one.cell);
        std::sort(cells.begin(), cells.end());
        return std::adjacent_find(cells.begin(), cells.end()) == cells.end();
    };
    const auto allPlaced = [](const std::vector<efx::tiles::Placement>& p) {
        for (const auto& one : p) {
            if (one.cell < 0) return false;
        }
        return true;
    };

    // 1. Aus dem Nichts heraus: jeder bekommt einen eigenen Platz.
    std::vector<int> owner;
    {
        const auto placed = assign(owner, {10, 11, 12, 13});
        check(allPlaced(placed), "jeder Sichtbare bekommt einen Platz");
        check(noDuplicates(placed), "und keine zwei teilen sich einen");
        for (const auto& one : placed) {
            check(one.fresh, "beim ersten Mal ist jeder neu");
        }
    }

    // 2. Dasselbe noch einmal: niemand ist neu, alle behalten ihren Platz.
    {
        const auto before = assign(owner, {10, 11, 12, 13});
        const auto again = assign(owner, {10, 11, 12, 13});
        bool same = true, anyFresh = false;
        for (size_t i = 0; i < again.size(); ++i) {
            if (again[i].cell != before[i].cell) same = false;
            if (again[i].fresh) anyFresh = true;
        }
        check(same, "wer sichtbar bleibt, behaelt seinen Platz");
        check(!anyFresh, "und gilt nicht als neu");
    }

    // 3. Rollen: einer faellt weg, einer kommt dazu.
    //
    // Der Bleibende darf seinen Platz NICHT verlieren — sonst muesste er neu
    // gezeichnet werden, und beim Rollen waere das jedes Bild.
    {
        const auto before = assign(owner, {10, 11, 12, 13});
        const auto after = assign(owner, {11, 12, 13, 14});
        check(after[0].cell == before[1].cell, "11 behaelt seinen Platz");
        check(after[1].cell == before[2].cell, "12 auch");
        check(after[2].cell == before[3].cell, "13 auch");
        check(after[3].fresh, "nur der neue 14 gilt als neu");
        check(after[3].cell == before[0].cell,
              "und bekommt den Platz des weggefallenen 10");
        check(noDuplicates(after), "weiterhin teilt sich niemand einen Platz");
    }

    // 4. Mehr Sichtbare als Plaetze. Das war der Fall, der schiefging: die
    //    Platzliste wuchs nicht mit, und wer keinen bekam, landete auf null.
    {
        std::vector<int> small(2, -1);
        const auto placed = assign(small, {1, 2, 3, 4, 5});
        check(allPlaced(placed), "auch bei zu wenigen Plaetzen bekommt jeder einen");
        check(noDuplicates(placed), "und immer noch teilt sich niemand einen");
        check(small.size() >= 5, "die Platzliste waechst mit");
        std::printf("  2 Plaetze, 5 Sichtbare -> %zu Plaetze, alle verschieden\n",
                    small.size());
    }

    // 5. Alles ausgetauscht: jeder ist neu, keiner erbt einen belegten Platz.
    {
        std::vector<int> owner2;
        assign(owner2, {1, 2, 3});
        const auto placed = assign(owner2, {7, 8, 9});
        check(allPlaced(placed) && noDuplicates(placed),
              "nach vollstaendigem Wechsel bleibt die Vergabe eindeutig");
        int freshCount = 0;
        for (const auto& one : placed) {
            if (one.fresh) ++freshCount;
        }
        check(freshCount == 3, "und alle drei gelten als neu");
    }
}

void testSheetLayout() {
    std::cout << "== Wo ein Platz im Blatt liegt ==\n";

    // Der Fehler dahinter sah aus wie ein Zuordnungsfehler und war keiner:
    // die PLATZNUMMER blieb richtig, nur ihre Lage im Blatt wanderte.
    //
    // Das Blatt ist moeglichst quadratisch gepackt, seine Spaltenzahl haengt
    // also an der Zahl der Plaetze — und die waechst beim Rollen. Eine Kachel,
    // die stehenbleibt, liest danach an einer anderen Stelle und zeigt das
    // Bild eines fremden Effekts.
    using efx::tiles::sheetColumns;

    const auto whereIs = [](size_t cell, size_t cells) {
        const size_t columns = sheetColumns(cells);
        return std::pair<size_t, size_t>{cell % columns, cell / columns};
    };

    check(sheetColumns(0) >= 1, "auch ohne Plaetze mindestens eine Spalte");
    check(sheetColumns(1) == 1, "ein Platz, eine Spalte");
    check(sheetColumns(4) == 2, "vier Plaetze, zwei Spalten");
    check(sheetColumns(50) * sheetColumns(50) >= 50,
          "die Aufteilung fasst alle Plaetze");

    // Und der Kern der Sache: dieselbe Nummer, andere Lage.
    const auto at30 = whereIs(7, 30);
    const auto at50 = whereIs(7, 50);
    check(at30 != at50,
          "derselbe Platz liegt bei anderer Platzzahl woanders im Blatt");
    std::printf("  Platz 7: bei 30 Plaetzen Spalte %zu/Zeile %zu, "
                "bei 50 Plaetzen Spalte %zu/Zeile %zu\n",
                at30.first, at30.second, at50.first, at50.second);
    std::printf("  -> aendert sich die Spaltenzahl, muss alles neu gezeichnet "
                "werden\n");

    // Die Aufteilung darf nicht zappeln: gleiche Zahl, gleiche Aufteilung.
    check(sheetColumns(50) == sheetColumns(50), "gleiche Zahl, gleiche Aufteilung");
    // Und sie darf nicht schrumpfen, wenn Plaetze dazukommen — sonst wandern
    // Kacheln auch beim blossen Wachsen.
    bool monotone = true;
    for (size_t n = 1; n < 500; ++n) {
        if (sheetColumns(n + 1) < sheetColumns(n)) monotone = false;
    }
    check(monotone, "mehr Plaetze ergeben nie weniger Spalten");
}

void testSpawnPlacementFlags() {
    std::cout << "== Platzierungs-Flags ==\n";

    // Fuenf Flags aus `CFxScheduler::CreateEffect`, alle in dem Block VOR der
    // Verzweigung nach Typ. Sie wurden bei uns gelesen und nie ausgewertet —
    // eine Datei mit `orgOnSphere` sah aus wie eine ohne.
    using namespace efx::particles;
    namespace camera = efx::camera;

    // 1. `orgOnSphere`: ein Punkt auf einem Ellipsoid, in WELTkoordinaten.
    //
    //        x = DEG2RAD( flrand(0,1) * 360 );
    //        y = DEG2RAD( flrand(0,1) * 180 );
    //        temp = ( sin(x)*w*sin(y), cos(x)*w*sin(y), cos(y)*h );
    {
        // Am Pol (y=0) liegt der Punkt genau auf der Hochachse, in Hoehe h.
        const auto pole = pointOnSphere(0.0f, 0.0f, 50.0f, 80.0f);
        check(std::fabs(pole.z - 80.0f) < 0.01f, "am Pol liegt er auf der Hoehe");
        check(std::fabs(pole.x) < 0.01f && std::fabs(pole.y) < 0.01f,
              "und mittig darueber");

        // Am Aequator (y = 90 Grad, also pitch 0.5) liegt er auf dem Radius.
        const auto equator = pointOnSphere(0.0f, 0.5f, 50.0f, 80.0f);
        check(std::fabs(equator.z) < 0.01f, "am Aequator auf halber Hoehe null");
        check(std::fabs(camera::length(equator) - 50.0f) < 0.01f,
              "und im Abstand des Radius");
        std::printf("  Kugel: Pol z=%.1f, Aequator r=%.1f\n", pole.z,
                    camera::length(equator));

        // Alle Punkte liegen auf dem Ellipsoid — das ist die eigentliche
        // Zusicherung, und sie faellt auf, wenn jemand die Winkel vertauscht.
        bool onSurface = true;
        for (int i = 0; i < 200; ++i) {
            const float u = static_cast<float>(i) / 200.0f;
            const auto point = pointOnSphere(u, 1.0f - u, 50.0f, 80.0f);
            const float value = (point.x * point.x + point.y * point.y) / (50.0f * 50.0f) +
                                (point.z * point.z) / (80.0f * 80.0f);
            if (std::fabs(value - 1.0f) > 0.01f) onSurface = false;
        }
        check(onSurface, "jeder Punkt liegt auf der Flaeche des Ellipsoids");
    }

    // 2. `orgOnCylinder`: auf dem Mantel, um die Vorwaertsachse gedreht.
    //
    //        pt = ax[1]*radius;
    //        pt += ax[0] * ( flrand(-1,1) * 0.5 * height );
    //        temp = RotatePointAroundVector( ax[0], pt, flrand(0,1)*360 );
    {
        const Axis axis = axisFor(0);   // Orient Up: forward = +Z
        // Ohne Drehung und in der Mitte: genau auf der Seitenachse.
        const auto side = pointOnCylinder(axis, 30.0f, 100.0f, 0.0f, 0.0f);
        check(std::fabs(camera::length(side) - 30.0f) < 0.01f,
              "ohne Drehung liegt er im Radius");

        // Der Abstand zur Achse bleibt der Radius, egal wie weit gedreht.
        bool constantRadius = true;
        for (int i = 0; i < 90; ++i) {
            const auto point = pointOnCylinder(axis, 30.0f, 100.0f, 0.0f,
                                               static_cast<float>(i) * 4.0f);
            // Abstand zur Vorwaertsachse (+Z), also in der XY-Ebene.
            const float r = std::sqrt(point.x * point.x + point.y * point.y);
            if (std::fabs(r - 30.0f) > 0.05f) constantRadius = false;
        }
        check(constantRadius, "beim Drehen bleibt der Abstand zur Achse gleich");

        // Und der Anteil laengs der Achse ist hoechstens die halbe Hoehe.
        const auto top = pointOnCylinder(axis, 30.0f, 100.0f, 1.0f, 0.0f);
        check(std::fabs(top.z - 50.0f) < 0.01f,
              "ganz oben ist die halbe Hoehe, nicht die ganze");
        std::printf("  Zylinder: Radius %.1f, halbe Hoehe %.1f\n",
                    camera::length(side), top.z);
    }

    // 3. `axisFromSphere`: die Achsen zeigen vom Mittelpunkt nach aussen und
    //    stehen senkrecht aufeinander (MakeNormalVectors in q_math.c).
    {
        const auto axis = axisFromDirection({0.0f, 0.0f, 7.0f});
        check(std::fabs(axis.forward.z - 1.0f) < 0.01f,
              "die Vorwaertsachse zeigt in die gegebene Richtung");
        check(std::fabs(camera::dot(axis.forward, axis.right)) < 0.01f,
              "rechts steht senkrecht auf vorwaerts");
        check(std::fabs(camera::dot(axis.right, axis.up)) < 0.01f,
              "und oben senkrecht auf rechts");
        check(std::fabs(camera::length(axis.right) - 1.0f) < 0.01f,
              "alle sind Einheitsvektoren");
    }

    // 4. `randRotAroundFwd`: die Vorwaertsachse bleibt, die anderen drehen.
    {
        const Axis before = axisFor(0);
        const Axis after = rotatedAroundForward(before, 90.0f);
        check(std::fabs(camera::dot(after.forward, before.forward) - 1.0f) < 0.01f,
              "die Vorwaertsachse bleibt unveraendert");
        check(std::fabs(camera::dot(after.right, before.right)) < 0.01f,
              "nach 90 Grad steht die Seitenachse quer zur alten");
        check(std::fabs(camera::dot(after.forward, after.right)) < 0.01f,
              "und das Dreibein bleibt rechtwinklig");
    }

    // 5. Und das Ganze im Zusammenspiel: mit `orgOnSphere` streuen die
    //    Teilchen ueber eine Kugel, ohne das Flag sitzen sie alle im Ursprung.
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(80.0f);
        p.count.set = true;
        p.life = efx::Range::single(1000.0f);
        p.radius = efx::Range::single(60.0f);
        p.radius.set = true;
        p.height = efx::Range::single(60.0f);
        p.height.set = true;
        p.size.present = true;
        p.size.start = efx::Range::single(3.0f);
        p.shaders.push_back("gfx/x");

        efx::particles::System tight;
        tight.play(effect, 7u);
        float farthest = 0.0f;
        for (const auto& item : tight.live()) {
            farthest = std::max(farthest, camera::length(item.origin));
        }
        check(farthest < 1.0f, "ohne das Flag sitzen alle im Ursprung");

        p.spawnFlags |= efx::kSpawnOrgOnSphere;
        efx::particles::System spread;
        spread.play(effect, 7u);
        float nearest = 1e9f, outermost = 0.0f;
        for (const auto& item : spread.live()) {
            const float r = camera::length(item.origin);
            nearest = std::min(nearest, r);
            outermost = std::max(outermost, r);
        }
        check(outermost > 55.0f, "mit dem Flag liegen sie auf der Kugelschale");
        check(nearest > 55.0f, "und zwar ALLE — keiner bleibt in der Mitte");
        std::printf("  ohne Flag: alle im Ursprung; mit Flag: %.1f bis %.1f\n",
                    nearest, outermost);
    }
}

void testTexMods() {
    std::cout << "== tcMod gegen tr_shade_calc ==\n";

    // `tcMod` steht 238-mal in einer gewoehnlichen Installation, davon
    // 103-mal als `scroll`. Wir haben es GELESEN und nie angewendet —
    // scrollende Texturen standen still: Energiestrahlen, Kraftfelder,
    // fliessende Lava.
    using efx::shader::TexCoord;
    using efx::shader::TexMod;

    // 1. scroll: RB_CalcScrollTexCoords.
    //
    //        adjustedScrollS = scrollSpeed[0] * timeScale;
    //        adjustedScrollS -= floor( adjustedScrollS );
    //        st[0] += adjustedScrollS;
    {
        const std::vector<TexMod> mods{{"scroll", {0.5f, 0.0f}, {}}};
        const auto atZero = efx::shader::applyTexMods(mods, {0.0f, 0.0f}, 0.0f);
        check(std::fabs(atZero.u) < 1e-5f, "bei t=0 unveraendert");

        const auto atOne = efx::shader::applyTexMods(mods, {0.0f, 0.0f}, 1.0f);
        check(std::fabs(atOne.u - 0.5f) < 1e-5f, "nach einer Sekunde um 0.5");

        // Der Bruch wird abgeschnitten: bei 4 Sekunden waeren es 2.0, und
        // 2.0 - floor(2.0) ist null. \"clamp so coordinates don't
        // continuously get larger\" steht im Quelltext.
        const auto atFour = efx::shader::applyTexMods(mods, {0.0f, 0.0f}, 4.0f);
        check(std::fabs(atFour.u) < 1e-5f,
              "der ganzzahlige Anteil wird abgeschnitten");
        std::printf("  scroll 0.5: t=1 -> %.2f, t=4 -> %.2f\n", atOne.u, atFour.u);
    }

    // 2. scale: einfache Multiplikation, zeitunabhaengig.
    {
        const std::vector<TexMod> mods{{"scale", {2.0f, 4.0f}, {}}};
        const auto out = efx::shader::applyTexMods(mods, {0.5f, 0.25f}, 3.0f);
        check(std::fabs(out.u - 1.0f) < 1e-5f && std::fabs(out.v - 1.0f) < 1e-5f,
              "scale multipliziert und haengt nicht an der Zeit");
    }

    // 3. rotate: um (0.5, 0.5), und mit MINUS vor der Geschwindigkeit.
    //    Nach einer vollen Umdrehung muss wieder derselbe Punkt herauskommen.
    {
        const std::vector<TexMod> mods{{"rotate", {90.0f}, {}}};
        const auto full = efx::shader::applyTexMods(mods, {0.75f, 0.5f}, 4.0f);
        check(std::fabs(full.u - 0.75f) < 1e-3f &&
                  std::fabs(full.v - 0.5f) < 1e-3f,
              "nach 360 Grad steht es wieder am Ausgangspunkt");

        // Der Mittelpunkt bleibt immer liegen.
        const auto centre = efx::shader::applyTexMods(mods, {0.5f, 0.5f}, 1.3f);
        check(std::fabs(centre.u - 0.5f) < 1e-4f &&
                  std::fabs(centre.v - 0.5f) < 1e-4f,
              "die Drehung geht um (0.5, 0.5)");

        // Und sie dreht wirklich.
        const auto quarter = efx::shader::applyTexMods(mods, {1.0f, 0.5f}, 1.0f);
        check(std::fabs(quarter.u - 1.0f) > 0.1f ||
                  std::fabs(quarter.v - 0.5f) > 0.1f,
              "nach 90 Grad liegt der Punkt woanders");
        std::printf("  rotate 90/s: (1.00, 0.50) -> (%.2f, %.2f)\n", quarter.u,
                    quarter.v);
    }

    // 4. Mehrere hintereinander, in der Reihenfolge der Datei.
    {
        const std::vector<TexMod> mods{{"scale", {2.0f, 2.0f}, {}},
                                       {"scroll", {0.25f, 0.0f}, {}}};
        const auto out = efx::shader::applyTexMods(mods, {0.5f, 0.5f}, 1.0f);
        check(std::fabs(out.u - 1.25f) < 1e-5f,
              "erst skalieren, dann schieben — die Reihenfolge zaehlt");
    }

    // 5. `transform` koennen wir weiterhin nicht — und was wir nicht koennen,
    //    darf nichts kaputt machen.
    //
    //    Hier standen einmal auch `turb` und `stretch`, mit derselben
    //    Begruendung. Beide wirken inzwischen; die Pruefungen dafuer stehen in
    //    testStretchTurbAndAlphaWave.
    {
        TexMod unknown;
        unknown.type = "transform";
        unknown.args = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
        const std::vector<TexMod> mods{unknown};
        const auto out = efx::shader::applyTexMods(mods, {0.3f, 0.7f}, 2.0f);
        check(std::fabs(out.u - 0.3f) < 1e-5f && std::fabs(out.v - 0.7f) < 1e-5f,
              "was wir nicht kennen, laesst die Koordinaten unveraendert");
    }

    // Und eine Zusicherung, die bleibt: eine Welle ohne Namen tut nichts.
    // `stretch` ohne Wellenform kaeme sonst auf p = 1/0.
    {
        TexMod broken;
        broken.type = "stretch";
        const std::vector<TexMod> mods{broken};
        const auto out = efx::shader::applyTexMods(mods, {0.3f, 0.7f}, 2.0f);
        check(std::fabs(out.u - 0.3f) < 1e-5f,
              "stretch ohne Wellenform bleibt wirkungslos");
    }
}

void testStretchTurbAndAlphaWave() {
    std::cout << "== stretch, turb und alphaGen wave ==\n";

    // Die drei letzten Shader-Eigenschaften mit nennenswerter Verbreitung.
    // Alle drei wurden gelesen und nie angewandt.

    // 1. `tcMod stretch`: der KEHRWERT der Welle skaliert um die Mitte.
    //
    //        RB_CalcStretchTexCoords:
    //            p = 1.0f / EvalWaveForm( wf );
    //            matrix = { p, 0, 0, p };
    //            translate = { 0.5 - 0.5p, 0.5 - 0.5p };
    //
    //    Der Kehrwert ist der Grund, warum die Welle um 1.0 herum gemeint ist
    //    und nicht um 0 — bei base 0 waere p unendlich.
    {
        std::vector<efx::shader::TexMod> mods;
        efx::shader::TexMod stretch;
        stretch.type = "stretch";
        stretch.wave.func = "sin";
        stretch.wave.base = 1.0f;
        stretch.wave.amplitude = 0.5f;
        stretch.wave.frequency = 1.0f;
        mods.push_back(stretch);

        // Bei t=0 ist der Sinus null, also Welle 1.0 und p = 1: nichts
        // aendert sich.
        const auto plain = efx::shader::applyTexMods(mods, {1.0f, 1.0f}, 0.0f);
        check(std::fabs(plain.u - 1.0f) < 0.001f,
              "bei Wellenwert 1.0 bleibt alles, wie es ist");

        // Bei einem Viertel steht der Sinus oben: Welle 1.5, p = 2/3. Die
        // Ecke rueckt zur Mitte.
        const auto shrunk = efx::shader::applyTexMods(mods, {1.0f, 1.0f}, 0.25f);
        check(shrunk.u < 0.9f && shrunk.u > 0.7f,
              "bei groesserer Welle rueckt die Ecke zur Mitte");
        // Die Mitte bleibt die Mitte — dafuer ist die Verschiebung da.
        const auto centre = efx::shader::applyTexMods(mods, {0.5f, 0.5f}, 0.25f);
        check(std::fabs(centre.u - 0.5f) < 0.001f,
              "und die Mitte bleibt stehen");
        std::printf("  stretch: Ecke 1.00 -> %.3f, Mitte bleibt %.3f\n",
                    shrunk.u, centre.u);
    }

    // 2. `tcMod turb`: die Verwirbelung haengt an der LAGE des Eckpunkts.
    //
    //        now  = phase + floatTime * frequency;
    //        s   += sin( (x + z)/128 * 0.125 + now ) * amplitude;
    //        t   += sin(  y     /128 * 0.125 + now ) * amplitude;
    //
    //    Deshalb wirbelt eine grosse Flaeche anders als eine kleine — mit
    //    einem gemeinsamen Versatz je Viereck waere das nicht abzubilden.
    {
        std::vector<efx::shader::TexMod> mods;
        efx::shader::TexMod turb;
        turb.type = "turb";
        turb.wave.func = "sin";
        turb.wave.amplitude = 0.1f;
        turb.wave.frequency = 1.0f;
        mods.push_back(turb);

        const float here[3] = {0.0f, 0.0f, 0.0f};
        const float there[3] = {500.0f, 0.0f, 0.0f};
        const auto atHere = efx::shader::applyTexMods(mods, {0.5f, 0.5f}, 0.0f, here);
        const auto atThere =
            efx::shader::applyTexMods(mods, {0.5f, 0.5f}, 0.0f, there);
        check(std::fabs(atHere.u - atThere.u) > 0.001f,
              "zwei Eckpunkte an verschiedenen Orten wirbeln verschieden");

        // Ohne Lage passiert nichts — lieber gar nichts als etwas Falsches.
        const auto without = efx::shader::applyTexMods(mods, {0.5f, 0.5f}, 0.0f);
        check(std::fabs(without.u - 0.5f) < 0.001f,
              "ohne Lage bleibt turb wirkungslos");
        std::printf("  turb: bei x=0 %.3f, bei x=500 %.3f\n", atHere.u,
                    atThere.u);
    }

    // 3. `alphaGen wave` ersetzt die Aussteuerung, wie `rgbGen wave` die Farbe.
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.life = efx::Range::single(4000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(10.0f);
        p.shaders.push_back("gfx/blinkend");
        p.alpha.present = true;
        p.alpha.start = efx::Range::single(1.0f);
        p.alpha.start.set = true;

        efx::particles::System system;
        system.play(effect, 7u);

        efx::shader::WaveForm wave;
        wave.func = "sin";
        wave.base = 0.5f;
        wave.amplitude = 0.5f;
        wave.frequency = 1.0f;
        const auto lookup = [&](const std::string&) {
            return efx::particles::System::ShaderDraw{nullptr, nullptr, &wave};
        };

        const efx::camera::Vec3 right{1, 0, 0}, up{0, 0, 1};
        const auto bright = system.build(250.0f, right, up, lookup);
        const auto dark = system.build(750.0f, right, up, lookup);

        auto alphaOf = [](const efx::particles::DrawList& list) {
            for (const auto& group : list.byTexture) {
                if (!group.second.vertices.empty()) {
                    return (group.second.vertices[0].colour >> 24) & 0xFFu;
                }
            }
            return 0u;
        };
        check(alphaOf(bright) > 240u, "bei einem Viertel der Periode voll da");
        check(alphaOf(dark) < 15u, "bei drei Vierteln unsichtbar");
        std::printf("  alphaGen wave: %u bei 250 ms, %u bei 750 ms\n",
                    alphaOf(bright), alphaOf(dark));
    }
}

void testEditorOnlyKeys() {
    std::cout << "== Schluesselwoerter des alten Editors ==\n";

    // Aus EffectsEd.exe herausgelesen: der Bereich ab Byte 620500 enthaelt
    // die vollstaendige Schluesselworttabelle des Originals. Ein Abgleich
    // gegen unseren Zerleger ergab:
    //
    //   59 von 60 unserer Woerter stehen auch dort. Das fehlende ist
    //   `shellsound` — ein Mehrspieler-Merkmal, das es 2002 noch nicht gab.
    //
    //   Umgekehrt kennt das Original `force` und `forces`, samt einer
    //   Funktion `ParseForces`. Im ausgelieferten Quelltext von Jedi Academy
    //   kommt beides an KEINER Stelle vor — der Block wird gelesen und
    //   verworfen.
    //
    // \"Unbekannter Schluessel\" waere dafuer irrefuehrend: er ist bekannt,
    // nur wirkungslos.
    const char* text =
        "Particle\n{\n\tlife\t100\n\tforces\n\t[\n\t\twind\n\t]\n}\n";
    const auto result = efx::read(text);

    check(result.effect.primitives.size() == 1,
          "die Datei laedt trotzdem, das Segment bleibt");
    check(!result.hasErrors(), "und es ist kein Fehler, nur ein Hinweis");

    bool explained = false;
    for (const auto& d : result.diagnostics) {
        if (d.message.find("alte Editor") != std::string::npos) explained = true;
    }
    check(explained, "der Hinweis sagt, dass die Engine es nicht auswertet");
    for (const auto& d : result.diagnostics) {
        std::printf("  Zeile %d: %s\n", d.line, d.message.c_str());
    }

    // Ein wirklich unbekanntes Wort bleibt unbekannt — die Ausnahme darf
    // nicht zum Freibrief werden.
    {
        const auto other = efx::read("Particle\n{\n\tblubb\t3\n}\n");
        bool generic = false;
        for (const auto& d : other.diagnostics) {
            if (d.message.find("Unbekannter") != std::string::npos) generic = true;
        }
        check(generic, "was wirklich niemand kennt, heisst weiterhin unbekannt");
    }
}

void testMaxComponents() {
    std::cout << "== Hoechstens 24 Segmente ==\n";

    // FxScheduler.h:
    //
    //     #define FX_MAX_EFFECT_COMPONENTS 24
    //
    // und AddPrimitiveToEffect:
    //
    //     if ( ct >= FX_MAX_EFFECT_COMPONENTS ) {
    //         theFxHelper.Print( "Error--too many primitives in an effect" );
    //     } else { ... }
    //
    // Die ueberzaehligen werden beim Laden STILL verworfen — der Effekt
    // startet trotzdem, nur ohne sie.
    //
    // Wir haben alle simuliert. Die Vorschau zeigte damit mehr als das Spiel,
    // und zwar ohne sichtbaren Hinweis: die Pruefregeln melden es zwar, aber
    // gemeldet heisst nicht gezeigt.
    efx::Effect effect;
    for (int i = 0; i < 30; ++i) {
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.life = efx::Range::single(1000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(5.0f);
        p.shaders.push_back("gfx/x");
    }

    efx::particles::System system;
    system.play(effect, 7u);
    check(system.live().size() == 24,
          "von dreissig Segmenten werden vierundzwanzig gespielt");
    std::printf("  30 Segmente in der Datei -> %zu simuliert\n",
                system.live().size());

    // Genau 24 muessen vollstaendig durchkommen — die Grenze ist `>=`, nicht
    // `>`, also ist das 24. noch dabei.
    {
        efx::Effect exact = effect;
        exact.primitives.resize(24);
        efx::particles::System full;
        full.play(exact, 7u);
        check(full.live().size() == 24, "genau vierundzwanzig gehen vollstaendig durch");
    }

    // Und die Pruefregeln melden es weiterhin — beides gehoert zusammen:
    // die Simulation zeigt, was das Spiel zeigt, die Meldung sagt warum.
    {
        const auto found = efx::validate(effect, efx::Dialect::SP);
        bool complained = false;
        for (const auto& d : found) {
            if (d.id == efx::i18n::Str::VTooMany) complained = true;
        }
        check(complained, "und die Pruefregeln sagen, dass es zu viele sind");
    }
}

void testTurbulenceUsesPosition() {
    std::cout << "== tcMod turb bekommt die Position ==\n";

    // `turb` war umgesetzt und geprueft — aber der Aufrufer gab die Lage des
    // Eckpunkts nicht weiter, und ohne sie lief der ganze Zweig nicht.
    //
    //     RB_CalcTurbulentTexCoords:
    //         now = phase + floatTime * frequency;
    //         st[0] += sin( ((xyz[0]+xyz[2]) * 1/128 * 0.125 + now) ) * amp;
    //         st[1] += sin( ( xyz[1]         * 1/128 * 0.125 + now) ) * amp;
    //
    // Die Lage ist der Unterschied zwischen einer Verzerrung und einer
    // gleichmaessigen Verschiebung: ohne sie saehe `turb` aus wie ein
    // langsames `scroll`.
    std::vector<efx::shader::TexMod> mods;
    efx::shader::TexMod turb;
    turb.type = "turb";
    turb.wave.func = "sin";
    turb.wave.base = 0.0f;
    turb.wave.amplitude = 0.2f;
    turb.wave.phase = 0.0f;
    turb.wave.frequency = 1.0f;
    mods.push_back(turb);

    // Zwei Eckpunkte, weit auseinander: sie muessen VERSCHIEDEN verzerrt
    // werden. Genau das leistet nur die Position.
    const float here[3] = {0.0f, 0.0f, 0.0f};
    // 256 und nicht 512: bei 512 liegt der Sinus auf genau einer halben
    // Periode und ist null — dann waeren beide Punkte zufaellig gleich, und
    // der Test bewiese nichts. 256 ist ein Viertel, also der Hoechstwert.
    const float faraway[3] = {256.0f, 0.0f, 0.0f};
    const auto a = efx::shader::applyTexMods(mods, {0.5f, 0.5f}, 0.0f, here);
    const auto b = efx::shader::applyTexMods(mods, {0.5f, 0.5f}, 0.0f, faraway);
    check(std::fabs(a.u - b.u) > 0.01f,
          "zwei entfernte Eckpunkte werden verschieden verzerrt");
    std::printf("  bei x=0: u=%.3f, bei x=256: u=%.3f\n", a.u, b.u);

    // Ohne Position bleibt alles, wie es war — kein halber Effekt.
    const auto none = efx::shader::applyTexMods(mods, {0.5f, 0.5f}, 0.0f);
    check(std::fabs(none.u - 0.5f) < 1e-6f,
          "ohne Position wird gar nicht verzerrt");

    // Und der Weg bis zum Eckpunkt: kommt es beim Zeichnen an?
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(2.0f);
        p.count.set = true;
        p.life = efx::Range::single(2000.0f);
        // Weit auseinander, damit die Verzerrung sichtbar verschieden ist.
        p.origin.min = {0.0f, -300.0f, 0.0f};
        p.origin.max = {0.0f, 300.0f, 0.0f};
        p.origin.ranged = true;
        p.origin.set = true;
        p.size.present = true;
        p.size.start = efx::Range::single(10.0f);
        p.shaders.push_back("gfx/wirbelnd");

        efx::particles::System system;
        system.play(effect, 7u);

        const auto lookup = [&](const std::string&) {
            return efx::particles::System::ShaderDraw{&mods, nullptr};
        };
        const auto list =
            system.build(100.0f, {1, 0, 0}, {0, 0, 1}, lookup);

        float lowest = 9.0f, highest = -9.0f;
        for (const auto& group : list.byTexture) {
            for (const auto& vertex : group.second.vertices) {
                lowest = std::min(lowest, vertex.uv[0]);
                highest = std::max(highest, vertex.uv[0]);
            }
        }
        // Ohne Verzerrung waeren es genau 0 und 1.
        check(lowest < -0.001f || highest > 1.001f,
              "die Texturkoordinaten sind verzerrt, nicht bei 0 und 1");
        std::printf("  gezeichnet: u von %.3f bis %.3f\n", lowest, highest);
    }
}

void testRgbGenWave() {
    std::cout << "== rgbGen wave ==\n";

    // 135-mal in einer gewoehnlichen Installation — der groesste Posten unter
    // den Shader-Eigenschaften, die wir gelesen und nie angewandt haben.
    //
    // `RB_CalcWaveColor` in tr_shade_calc.cpp:
    //
    //     glow = EvalWaveForm( wf ) * tr.identityLight;
    //     if (glow < 0) glow = 0; else if (glow > 1) glow = 1;
    //     v = Q_ftol( 255 * glow );
    //     color[0] = color[1] = color[2] = v;
    //
    // Es ERSETZT die Farbe durch einen Grauwert, es daempft sie nicht. Damit
    // hat die Farbe aus der .efx fuer diese Stufe keine Wirkung mehr — das ist
    // der Sinn der Sache: das Pulsieren gibt der Shader vor.
    using efx::shader::evaluateWave;

    // Die Wellenformen selbst. Ravens Tabellen sind nicht alle so, wie der
    // Name vermuten laesst.
    {
        efx::shader::WaveForm sine;
        sine.func = "sin";
        sine.base = 0.5f;
        sine.amplitude = 0.5f;
        sine.frequency = 1.0f;
        check(std::fabs(evaluateWave(sine, 0.0f) - 0.5f) < 0.001f,
              "sin faengt in der Mitte an");
        check(std::fabs(evaluateWave(sine, 0.25f) - 1.0f) < 0.001f,
              "nach einem Viertel oben");
        check(std::fabs(evaluateWave(sine, 0.75f) - 0.0f) < 0.001f,
              "nach drei Vierteln unten");

        efx::shader::WaveForm tri = sine;
        tri.func = "triangle";
        tri.base = 0.0f;
        tri.amplitude = 1.0f;
        // triangleTable (R_Init in tr_init.cpp) ist ZWEISEITIG: 0 -> 1 im
        // ersten Viertel, zurueck auf 0 bei der Haelfte, -1 bei drei
        // Vierteln. Hier stand "laeuft 0..1..0, NICHT -1..1" — umgekehrt
        // richtig.
        check(std::fabs(evaluateWave(tri, 0.0f)) < 0.001f, "triangle bei null");
        check(std::fabs(evaluateWave(tri, 0.25f) - 1.0f) < 0.001f,
              "nach einem Viertel ganz oben");
        check(std::fabs(evaluateWave(tri, 0.5f)) < 0.001f, "bei der Haelfte null");
        check(std::fabs(evaluateWave(tri, 0.75f) + 1.0f) < 0.001f,
              "nach drei Vierteln ganz unten, -1");

        efx::shader::WaveForm saw = tri;
        saw.func = "sawtooth";
        check(evaluateWave(saw, 0.9f) > evaluateWave(saw, 0.1f),
              "sawtooth steigt");
        saw.func = "inversesawtooth";
        check(evaluateWave(saw, 0.9f) < evaluateWave(saw, 0.1f),
              "inversesawtooth faellt");

        efx::shader::WaveForm sq = tri;
        sq.func = "square";
        check(evaluateWave(sq, 0.25f) > evaluateWave(sq, 0.75f),
              "square springt bei der Haelfte");

        // Rauschen muss wiederholbar sein, sonst flackert ein angehaltenes Bild.
        efx::shader::WaveForm noise = tri;
        noise.func = "noise";
        check(evaluateWave(noise, 1.23f) == evaluateWave(noise, 1.23f),
              "gleiche Zeit, gleicher Wert");
    }

    // Und jetzt: kommt es beim Zeichnen an?
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.life = efx::Range::single(4000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(10.0f);
        p.shaders.push_back("gfx/pulsierend");
        // Eine kraeftige Farbe, die der Shader ueberschreiben MUSS.
        p.rgb.present = true;
        p.rgb.start.min = {1.0f, 0.0f, 0.0f};
        p.rgb.start.max = {1.0f, 0.0f, 0.0f};
        p.rgb.start.set = true;

        efx::particles::System system;
        system.play(effect, 7u);

        efx::shader::WaveForm wave;
        wave.func = "sin";
        wave.base = 0.5f;
        wave.amplitude = 0.5f;
        wave.frequency = 1.0f;
        const auto lookup = [&](const std::string&) {
            return efx::particles::System::ShaderDraw{nullptr, &wave};
        };

        const efx::camera::Vec3 right{1, 0, 0}, up{0, 0, 1};

        // Ohne Rueckruf bleibt die Farbe der .efx: rot.
        {
            const auto plain = system.build(250.0f, right, up);
            uint32_t colour = 0;
            for (const auto& group : plain.byTexture) {
                if (!group.second.vertices.empty()) {
                    colour = group.second.vertices[0].colour;
                }
            }
            check((colour & 0xFFu) > 200u, "ohne Welle bleibt es rot");
            check(((colour >> 8) & 0xFFu) < 50u, "und gruen bleibt aus");
        }

        // Mit Welle: bei 250 ms steht der Sinus oben, also weiss.
        {
            const auto lit = system.build(250.0f, right, up, lookup);
            uint32_t colour = 0;
            for (const auto& group : lit.byTexture) {
                if (!group.second.vertices.empty()) {
                    colour = group.second.vertices[0].colour;
                }
            }
            const uint32_t r = colour & 0xFFu;
            const uint32_t g = (colour >> 8) & 0xFFu;
            const uint32_t b = (colour >> 16) & 0xFFu;
            check(r == g && g == b, "mit Welle ist es ein Grauwert");
            check(r > 240u, "und bei einem Viertel der Periode ganz hell");
            std::printf("  bei 250 ms: (%u, %u, %u)\n", r, g, b);
        }

        // Bei 750 ms steht der Sinus unten: schwarz.
        {
            const auto dark = system.build(750.0f, right, up, lookup);
            uint32_t colour = 0xFFFFFFFFu;
            for (const auto& group : dark.byTexture) {
                if (!group.second.vertices.empty()) {
                    colour = group.second.vertices[0].colour;
                }
            }
            check((colour & 0xFFu) < 15u, "bei drei Vierteln ganz dunkel");
            std::printf("  bei 750 ms: Helligkeit %u\n", colour & 0xFFu);
        }
    }
}

void testTexModsReachTheQuad() {
    std::cout << "== tcMod erreicht die Eckpunkte ==\n";

    // `applyTexMods` war umgesetzt, geprueft — und wurde von NIEMANDEM
    // aufgerufen ausser von den Tests selbst.
    //
    // Dieselbe Form wie bei der Kameraerschuetterung: die Regel war richtig
    // nachgebaut, die Leitung dorthin fehlte. `tcMod scroll` steht 103-mal in
    // einer gewoehnlichen Installation, `scale` 64-, `rotate` 21-mal; alles
    // davon stand still.
    //
    // Dieser Test prueft nicht die Formel — das tun die Tests darueber —
    // sondern dass sie beim Zeichnen ANKOMMT.
    efx::Effect effect;
    effect.primitives.push_back(efx::Primitive{});
    auto& p = effect.primitives.back();
    p.type = efx::PrimitiveType::Particle;
    p.life = efx::Range::single(4000.0f);
    p.size.present = true;
    p.size.start = efx::Range::single(10.0f);
    p.shaders.push_back("gfx/wandernd");

    efx::particles::System system;
    system.play(effect, 7u);

    // Ein Shader, dessen Textur waagerecht wandert.
    std::vector<efx::shader::TexMod> mods;
    efx::shader::TexMod scroll;
    scroll.type = "scroll";
    scroll.args = {1.0f, 0.0f};
    mods.push_back(scroll);
    const auto lookup = [&](const std::string&) {
        return efx::particles::System::ShaderDraw{&mods, nullptr};
    };

    const efx::camera::Vec3 right{1, 0, 0}, up{0, 0, 1};

    // Ohne Rueckruf bleiben die Ecken bei 0 und 1.
    {
        const auto plain = system.build(500.0f, right, up);
        float lowest = 9.0f, highest = -9.0f;
        for (const auto& group : plain.byTexture) {
            for (const auto& vertex : group.second.vertices) {
                lowest = std::min(lowest, vertex.uv[0]);
                highest = std::max(highest, vertex.uv[0]);
            }
        }
        check(std::fabs(lowest) < 0.001f && std::fabs(highest - 1.0f) < 0.001f,
              "ohne Rueckruf bleiben die Texturkoordinaten unveraendert");
    }

    // Mit Rueckruf wandern sie — und zwar um den Anteil der verstrichenen Zeit.
    {
        const auto moved = system.build(500.0f, right, up, lookup);
        float lowest = 9.0f;
        for (const auto& group : moved.byTexture) {
            for (const auto& vertex : group.second.vertices) {
                lowest = std::min(lowest, vertex.uv[0]);
            }
        }
        // 1.0 je Sekunde, nach 500 ms also ein halber Durchlauf.
        check(std::fabs(lowest - 0.5f) < 0.01f,
              "mit Rueckruf sind sie um eine halbe Breite verschoben");
        std::printf("  nach 500 ms bei scroll 1.0: linke Kante bei %.3f\n",
                    lowest);
    }

    // Und der abgeschnittene Anteil: nach 2.5 s ist es wieder eine halbe
    // Breite, nicht zweieinhalb.
    {
        const auto later = system.build(2500.0f, right, up, lookup);
        float lowest = 9.0f;
        for (const auto& group : later.byTexture) {
            for (const auto& vertex : group.second.vertices) {
                lowest = std::min(lowest, vertex.uv[0]);
            }
        }
        check(std::fabs(lowest - 0.5f) < 0.01f,
              "der ganzzahlige Anteil wird abgeschnitten, wie im Renderer");
        std::printf("  nach 2500 ms immer noch %.3f statt 2.5\n", lowest);
    }
}

void testSpawnFlags() {
    std::cout << "== Spawn-Flags gegen FxScheduler ==\n";

    // Der Block in `CFxScheduler::CreateEffect` vor der Typverzweigung. Vier
    // Regeln, alle mit Eigenheiten.
    auto makeEffect = [](uint32_t flags) {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(400.0f);
        p.count.set = true;
        p.life = efx::Range::single(1000.0f);
        p.radius = efx::Range::single(50.0f);
        p.radius.set = true;
        p.height = efx::Range::single(50.0f);
        p.height.set = true;
        p.size.present = true;
        p.size.start = efx::Range::single(3.0f);
        p.shaders.push_back("gfx/x");
        p.spawnFlags = flags;
        return effect;
    };

    // 1. `orgOnSphere`: Punkte auf einem Ellipsoid.
    //
    //        x = DEG2RAD( flrand(0,1) * 360 );
    //        y = DEG2RAD( flrand(0,1) * 180 );
    //        temp = ( sin(x)*w*sin(y), cos(x)*w*sin(y), cos(y)*h );
    //
    //    Wichtig: `temp` wird NICHT ueber die Achsen gedreht, es steht in
    //    Weltkoordinaten. Und y laeuft nur ueber 180 Grad — das ist die
    //    uebliche Kugelparametrisierung, kein Versehen.
    {
        efx::particles::System system;
        system.play(makeEffect(efx::kSpawnOrgOnSphere), 7u);
        float smallest = 1e9f, largest = 0.0f;
        for (const auto& item : system.live()) {
            const float r = efx::camera::length(item.origin);
            smallest = std::min(smallest, r);
            largest = std::max(largest, r);
        }
        check(largest <= 50.5f, "kein Punkt liegt ausserhalb der Kugel");
        check(largest > 45.0f, "aber sie reichen bis an den Rand");
        std::printf("  Kugel r=50: Abstaende %.1f bis %.1f\n", smallest, largest);
    }

    // 2. `orgOnCylinder`: auf dem Mantel, nicht im Inneren.
    //
    //        pt = ax[1]*radius;
    //        pt += ax[0] * ( flrand(-1,1) * 0.5 * height );
    //        temp = RotatePointAroundVector( ax[0], pt, flrand(0,1)*360 );
    {
        efx::particles::System system;
        system.play(makeEffect(efx::kSpawnOrgOnCylinder), 7u);
        float smallestSide = 1e9f, tallest = 0.0f;
        for (const auto& item : system.live()) {
            // \"Orient Up\": die Achse ist +Z, der Abstand also in der xy-Ebene.
            const float side = std::sqrt(item.origin.x * item.origin.x +
                                         item.origin.y * item.origin.y);
            smallestSide = std::min(smallestSide, side);
            tallest = std::max(tallest, std::fabs(item.origin.z));
        }
        check(smallestSide > 49.0f,
              "alle liegen auf dem Mantel, keiner im Inneren");
        check(tallest <= 25.5f, "und hoechstens eine halbe Hoehe weit oben");
        std::printf("  Zylinder r=50 h=50: Seitenabstand ab %.1f, Hoehe bis %.1f\n",
                    smallestSide, tallest);
    }

    // 3. `axisFromSphere` baut die Achsen aus der Richtung — und zwar bei
    //    Kugel und Zylinder VERSCHIEDEN.
    //
    //    Kugel:    MakeNormalVectors( ax[0], ax[1], ax[2] )
    //    Zylinder: up = {0,0,1}; if (ax[0][2] == 1) up = {0,1,0};
    //              CrossProduct( up, ax[0], ax[1] );
    //              CrossProduct( ax[0], ax[1], ax[2] );
    //
    //    Wir hatten fuer beides die Kugelfassung. Sichtbar wird das bei einem
    //    Kindeffekt, der nicht rund um seine Achse gleich aussieht.
    {
        efx::particles::System system;
        system.play(makeEffect(efx::kSpawnOrgOnCylinder | efx::kSpawnAxisFromSphere),
                    7u);
        bool sane = true;
        for (const auto& item : system.live()) {
            // Die Achse muss vom Mittelpunkt nach aussen zeigen, also in der
            // Ebene liegen, in der auch der Punkt liegt.
            if (std::fabs(efx::camera::length(item.forward) - 1.0f) > 0.01f) {
                sane = false;
            }
        }
        check(sane, "die uebernommene Achse ist ein Einheitsvektor");
    }

    // 4. `randRotAroundFwd` dreht die Seitenachse um die Vorwaertsachse:
    //
    //        RotatePointAroundVector( ax[1], ax[0], axis[1], flrand(0,1)*360 );
    //        CrossProduct( ax[0], ax[1], ax[2] );
    //
    //    Die VORWAERTSachse bleibt dabei stehen — nur die Drehung um sie
    //    aendert sich.
    {
        efx::Effect effect = makeEffect(efx::kSpawnRandRotAroundFwd);
        effect.primitives[0].origin.min = {0.0f, 10.0f, 0.0f};
        effect.primitives[0].origin.max = {0.0f, 10.0f, 0.0f};
        effect.primitives[0].origin.set = true;

        efx::particles::System system;
        system.play(effect, 7u);
        float highest = -1e9f, lowest = 1e9f;
        bool spread = false;
        for (const auto& item : system.live()) {
            highest = std::max(highest, item.origin.z);
            lowest = std::min(lowest, item.origin.z);
            if (std::fabs(item.origin.x) > 1.0f) spread = true;
        }
        check(spread, "die Seitenachse wird zufaellig um die Vorwaertsachse gedreht");
        check(std::fabs(highest) < 1.0f && std::fabs(lowest) < 1.0f,
              "die Vorwaertsachse selbst bleibt stehen");
        std::printf("  randRotAroundFwd: entlang der Achse %.2f bis %.2f\n",
                    lowest, highest);
    }
}

void testTailAndCylinderGeometry() {
    std::cout << "== Schweif und Zylinder ==\n";

    const efx::camera::Vec3 right{1, 0, 0}, up{0, 0, 1};

    // 1. Der Schweif zeigt entgegen der TATSAECHLICHEN Bewegung.
    //
    //    `CTail::CalcNewEndpoint`:
    //        VectorSubtract( mOldOrigin, mOrigin1, temp );
    //        VectorNormalize( temp );
    //        VectorMA( mOrigin1, mLength, temp, mRefEnt.oldorigin );
    //
    //    `mOldOrigin` ist die Stelle von kurz zuvor — der Schweif zeigt also
    //    dorthin zurueck, wo das Teilchen herkam. Mit der ANFANGS-
    //    geschwindigkeit zu rechnen waere falsch: bei Schwerkraft kruemmt sich
    //    die Bahn, und nach einem Abprall zeigte der Schweif in die voellig
    //    falsche Richtung.
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Tail;
        p.life = efx::Range::single(2000.0f);
        // Waagerecht los, Schwerkraft zieht nach unten: die Bahn kruemmt sich.
        p.velocity.min = {0.0f, 100.0f, 0.0f};
        p.velocity.max = {0.0f, 100.0f, 0.0f};
        p.velocity.set = true;
        p.gravity = efx::Range::single(-800.0f);
        p.gravity.set = true;
        p.length.present = true;
        p.length.start = efx::Range::single(20.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(2.0f);
        p.shaders.push_back("gfx/x");

        efx::particles::System system;
        system.play(effect, 7u);
        const auto early = system.build(50.0f, right, up);
        const auto late = system.build(1000.0f, right, up);
        const auto bandOf = [](const efx::particles::DrawList& list)
            -> const std::vector<efx::scene::Vertex>* {
            if (list.byTexture.empty()) return nullptr;
            const auto& v = list.byTexture.begin()->second.vertices;
            return v.size() >= 4 ? &v : nullptr;
        };
        check(bandOf(early) && bandOf(late), "der Schweif wird als Band gezeichnet");

        // Frueh fliegt es fast waagerecht, spaet faellt es. Der Schweif zeigt
        // beide Male nach HINTEN — spaet also staerker nach oben. Ecke 0 am
        // Anfang, Ecke 2 auf derselben Seite am Ende (DoLine).
        if (bandOf(early) && bandOf(late)) {
            const float earlyZ = (*bandOf(early))[2].pos[2] - (*bandOf(early))[0].pos[2];
            const float lateZ = (*bandOf(late))[2].pos[2] - (*bandOf(late))[0].pos[2];
            check(earlyZ < lateZ,
                  "spaet zeigt der Schweif staerker nach oben als frueh");
            std::printf("  Schweifrichtung z: frueh %.2f, spaet %.2f\n",
                        static_cast<double>(earlyZ), static_cast<double>(lateZ));
        }
    }

    // 2. Der Zylinder geht VOM Ursprung aus, nicht mittig:
    //        VectorMA( mOrigin1, mLength, mRefEnt.axis[0], mRefEnt.oldorigin );
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Cylinder;
        p.life = efx::Range::single(1000.0f);
        p.length.present = true;
        p.length.start = efx::Range::single(100.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(10.0f);
        p.shaders.push_back("gfx/x");

        efx::particles::System system;
        system.play(effect, 7u);
        const auto list = system.build(10.0f, right, up);
        float lowest = 1e9f, highest = -1e9f;
        for (const auto& group : list.byTexture) {
            for (const auto& vertex : group.second.vertices) {
                lowest = std::min(lowest, vertex.pos[2]);
                highest = std::max(highest, vertex.pos[2]);
            }
        }
        check(lowest > -1.0f, "er faengt am Ursprung an, nicht darunter");
        check(highest > 90.0f, "und reicht die volle Laenge nach oben");
        std::printf("  Zylinder von z=%.1f bis z=%.1f (Laenge 100)\n", lowest,
                    highest);
    }
}

void testLineAndColourRules() {
    std::cout << "== Linien, Zylinder und Farbmischung ==\n";

    // 1. `origin2` ist bei Line und Electricity ein PUNKT, kein Versatz zum
    //    Teilchen — und mit `cheapOrigin2Calculation` sogar ein absoluter.
    //
    //    FxScheduler.cpp, Block "Line type primitives work with an origin2":
    //
    //        if ( mSpawnFlags & FX_CHEAP_ORG2_CALC || flags & FX_RELATIVE ) {
    //            VectorSet( org2, mOrigin2X.GetVal(), ... );      // roh
    //        } else {
    //            org2 = ax[0]*x + ax[1]*y + ax[2]*z;
    //            VectorAdd( org2, origin, org2 );                 // EFFEKT-Ursprung
    //        }
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Line;
        p.life = efx::Range::single(1000.0f);
        // Der Startpunkt wandert, der Endpunkt darf NICHT mitwandern.
        p.origin.min = {50.0f, 0.0f, 0.0f};
        p.origin.max = {50.0f, 0.0f, 0.0f};
        p.origin.set = true;
        // Wie beim Ursprung: die erste Zahl laeuft entlang der
        // Vorwaertsachse, bei \"Orient Up\" also nach oben.
        p.origin2.min = {100.0f, 0.0f, 0.0f};
        p.origin2.max = {100.0f, 0.0f, 0.0f};
        p.origin2.set = true;
        p.size.present = true;
        p.size.start = efx::Range::single(2.0f);
        p.shaders.push_back("gfx/x");

        efx::particles::System system;
        system.play(effect, 7u);
        check(!system.live().empty(), "die Linie entsteht");
        if (!system.live().empty()) {
            const auto& item = system.live()[0];
            // "Orient Up": forward = +Z, also traegt origin.x nach oben.
            check(std::fabs(item.origin.z - 50.0f) < 0.01f,
                  "der Startpunkt liegt bei 50 auf der Hochachse");
            check(std::fabs(item.origin2.z - 100.0f) < 0.01f,
                  "der Endpunkt bei 100 — vom EFFEKT aus, nicht vom Teilchen");
            std::printf("  Linie von z=%.0f nach z=%.0f\n", item.origin.z,
                        item.origin2.z);
        }
    }

    // 2. Cylinder und Decal richten sich an der VORWAERTSACHSE aus, nicht an
    //    `origin2`:
    //        FX_AddCylinder( clientID, org, ax[0], ... );
    //        CG_ImpactMark( handle, org, ax[0], rotation, ... );
    //
    //    Wir hatten `origin2` genommen. Eine Datei ohne `origin2` ergab damit
    //    einen Zylinder, der immer senkrecht stand, statt in Effektrichtung.
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Cylinder;
        p.life = efx::Range::single(1000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(10.0f);
        p.length.present = true;
        p.length.start = efx::Range::single(100.0f);
        p.shaders.push_back("gfx/x");

        // Ausrichtung "seitwaerts": die Vorwaertsachse ist +X.
        efx::particles::System sideways;
        sideways.play(effect, 7u, {}, efx::particles::axisFor(1));
        check(!sideways.live().empty(), "der Zylinder entsteht");
        if (!sideways.live().empty()) {
            check(std::fabs(sideways.live()[0].forward.x - 1.0f) < 0.01f,
                  "er folgt der Vorwaertsachse des Effekts");
        }
    }

    // 3. `rgbComponentInterpolation`: EIN Zufallswert fuer alle drei Kanaele.
    //
    //        float perc = Q_flrand(0.0f, 1.0f);
    //        sRGB = ( mRedStart.GetVal(perc), mGreenStart.GetVal(perc), ... )
    //
    //    Ohne das Flag wuerfelt jeder Kanal fuer sich, und aus einer Spanne
    //    von Rot nach Weiss werden auch gruenliche und rosa Toene. Der Flag
    //    wurde bei uns gelesen, aber nie benutzt — in t_small_fire.efx steht
    //    er bei drei von vier Segmenten.
    {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(200.0f);
        p.count.set = true;
        p.life = efx::Range::single(1000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(5.0f);
        p.shaders.push_back("gfx/x");
        p.rgb.present = true;
        p.rgb.start.min = {1.0f, 0.0f, 0.0f};   // Rot
        p.rgb.start.max = {1.0f, 1.0f, 1.0f};   // Weiss
        p.rgb.start.ranged = true;
        p.rgb.start.set = true;
        p.spawnFlags |= efx::kSpawnRgbComponentInterp;

        efx::particles::System system;
        system.play(effect, 7u);
        bool onTheLine = true;
        for (const auto& item : system.live()) {
            // Auf der Geraden von Rot nach Weiss gilt gruen == blau, und rot
            // bleibt eins.
            if (std::fabs(item.rgb[1].start - item.rgb[2].start) > 0.001f) {
                onTheLine = false;
            }
            if (std::fabs(item.rgb[0].start - 1.0f) > 0.001f) onTheLine = false;
        }
        check(onTheLine, "mit dem Flag bleibt die Farbe auf der Geraden");

        // Gegenprobe ohne Flag: dann laufen die Kanaele auseinander.
        efx::Effect independent = effect;
        independent.primitives[0].spawnFlags &= ~efx::kSpawnRgbComponentInterp;
        efx::particles::System scattered;
        scattered.play(independent, 7u);
        bool anyOff = false;
        for (const auto& item : scattered.live()) {
            if (std::fabs(item.rgb[1].start - item.rgb[2].start) > 0.05f) {
                anyOff = true;
            }
        }
        check(anyOff, "ohne das Flag wuerfelt jeder Kanal fuer sich");
        std::printf("  mit Flag: gruen == blau, ohne Flag: verschieden\n");
    }
}

void testBounceMatchesEngine() {
    std::cout << "== Abprall gegen die Engine-Regeln ==\n";

    // `CParticle::UpdateOrigin`, der Block hinter FX_APPLY_PHYSICS:
    //
    //     VectorMA( mVel, mFloatFrameTime * trace.fraction, mAccel, mVel );
    //     dot = DotProduct( mVel, trace.plane.normal );
    //     VectorMA( mVel, -2 * dot, trace.plane.normal, mVel );
    //     VectorScale( mVel, mElasticity, mVel );
    //
    //     if ( trace.plane.normal[2] > 0 && mVel[2] < 4 ) {
    //         VectorClear( mVel );  VectorClear( mAccel );
    //         mFlags &= ~(FX_APPLY_PHYSICS|FX_IMPACT_RUNS_FX);
    //     }
    //     VectorCopy( trace.endpos, mOrigin1 );
    //
    // Vier Regeln, die man alle auch anders bauen koennte.
    const std::vector<efx::sim::Plane> floor{{{0.0f, 0.0f, 1.0f}, 0.0f}};

    // 1. Die Elastizitaet daempft die GANZE Geschwindigkeit, nicht nur den
    //    Anteil senkrecht zur Flaeche. Ein schraeg auftreffender Funke wird
    //    also auch langsamer, nicht nur flacher.
    {
        const auto path = efx::sim::buildPath({0, 0, 50}, {100, 0, -100}, {}, 0.0f,
                                              3000.0f, floor, 0.5f, false);
        check(path.segments.size() > 1, "es gibt ein zweites Bahnstueck");
        if (path.segments.size() > 1) {
            const auto v = path.segments[1].velocity;
            check(std::fabs(v.x - 50.0f) < 1.0f,
                  "auch die Geschwindigkeit LAENGS der Flaeche wird gedaempft");
            check(v.z > 0.0f, "und die senkrechte kehrt sich um");
            std::printf("  schraeg mit e=0.5: (100,0,-100) -> (%.0f, %.0f, %.0f)\n",
                        v.x, v.y, v.z);
        }
    }

    // 2. Spiegeln: v' = v - 2*(v·n)*n. Bei e=1 bleibt der Betrag erhalten.
    {
        const auto path = efx::sim::buildPath({0, 0, 50}, {100, 0, -100}, {}, 0.0f,
                                              3000.0f, floor, 1.0f, false);
        if (path.segments.size() > 1) {
            const auto v = path.segments[1].velocity;
            check(std::fabs(v.x - 100.0f) < 1.0f && std::fabs(v.z - 100.0f) < 1.0f,
                  "ohne Daempfung wird nur gespiegelt");
        }
    }

    // 3. Zur Ruhe kommen: auf einer nach oben zeigenden Flaeche und mit
    //    weniger als 4 Einheiten Aufwaertsbewegung bleibt es liegen. Ohne
    //    diese Regel zittert ein Funke ewig auf dem Boden.
    {
        const auto path = efx::sim::buildPath({0, 0, 10}, {}, {}, -800.0f, 5000.0f,
                                              floor, 0.02f, false);
        check(path.settledMs > 0.0f, "mit kleiner Elastizitaet kommt es zur Ruhe");
        check(path.impactMs.size() <= 3,
              "und zwar sofort, nicht nach dreissig Aufprallen");
        std::printf("  e=0.02: %zu Aufpralle, liegt ab %.0f ms still\n",
                    path.impactMs.size(), path.settledMs);
    }

    // Und die Gegenprobe: mit viel Elastizitaet springt es weiter.
    {
        const auto path = efx::sim::buildPath({0, 0, 100}, {}, {}, -800.0f, 3000.0f,
                                              floor, 1.0f, false);
        check(path.impactMs.size() >= 2, "mit e=1 springt es mehrfach");
        if (path.segments.size() > 1) {
            // Fall aus 100 bei g=-800: v = sqrt(2*800*100) = 400.
            check(std::fabs(path.segments[1].velocity.z - 400.0f) < 5.0f,
                  "und kommt mit der Aufprallgeschwindigkeit zurueck");
        }
    }

    // 4. killOnImpact: die Bahn endet am ersten Aufprall, es prallt nicht ab.
    {
        const auto path = efx::sim::buildPath({0, 0, 50}, {}, {}, -800.0f, 3000.0f,
                                              floor, 1.0f, true);
        check(path.killed, "mit killOnImpact stirbt es beim Aufprall");
        check(path.impactMs.size() == 1, "und es gibt genau einen Aufprall");
        check(path.segments.size() == 1, "kein zweites Bahnstueck");
    }
}

void testCameraViews() {
    std::cout << "== Blickrichtungen und Ziehrichtung ==\n";

    // 1. Die Kamera folgt der Maus, sie laeuft ihr nicht entgegen.
    //
    //    Beide Achsen waren verkehrt herum: nach links ziehen liess sie nach
    //    rechts blicken, nach oben ziehen nach unten. Im alten Editor ist es
    //    umgekehrt, und das ist auch die verbreitete Erwartung — man greift
    //    die SZENE und dreht sie.
    {
        // Geprueft wird ueber die AUGENLAGE, nicht ueber die Winkel: die sind
        // privat, und die Lage ist ohnehin das, was man sieht.
        efx::camera::Orbit cam;
        cam.lookFrom(efx::camera::Orbit::View::Front);   // Blick nach +Y
        const efx::camera::Vec3 before = cam.position();
        cam.orbit(20.0f, 0.0f);          // nach rechts ziehen
        const efx::camera::Vec3 after = cam.position();
        // Blick nach +Y heisst: die Kamera STEHT bei -Y (yaw = 270). Zieht man
        // nach rechts, wandert sie nach -X — von dort sieht man die Szene nach
        // rechts weggedreht.
        //
        // Diese Erwartung hatte ich zweimal falsch aufgeschrieben, weil ich
        // `yaw` fuer die Blickrichtung hielt statt fuer den Standort. Erst
        // eine Messung an `position()` hat es geklaert.
        check(after.x < before.x - 0.01f,
              "nach rechts ziehen dreht die Szene mit, nicht dagegen");

        efx::camera::Orbit up;
        up.lookFrom(efx::camera::Orbit::View::Front);
        const float heightBefore = up.position().z;
        up.orbit(0.0f, 20.0f);           // nach unten ziehen
        // `pitch` steigt (0 -> 8), forward zeigt nach oben, das Auge steht
        // also TIEFER und blickt hinauf. Genau wie im alten Editor.
        check(up.position().z < heightBefore - 0.01f,
              "nach unten ziehen senkt das Auge, hebt also den Blick");
        std::printf("  ziehen nach rechts: Auge x %.1f -> %.1f\n", before.x,
                    after.x);
    }

    // 2. Die festen Blickrichtungen zeigen wirklich entlang der Achsen.
    //
    //    Nachgerechnet aus der Richtungsformel:
    //        forward = (cos p · cos y,  cos p · sin y,  sin p)
    struct Wanted {
        efx::camera::Orbit::View view;
        efx::camera::Vec3 forward;
        const char* name;
    };
    const Wanted kWanted[] = {
        {efx::camera::Orbit::View::Front,  {0, 1, 0},  "vorn  -> +Y"},
        {efx::camera::Orbit::View::Back,   {0, -1, 0}, "hinten -> -Y"},
        {efx::camera::Orbit::View::Left,   {1, 0, 0},  "links -> +X"},
        {efx::camera::Orbit::View::Right,  {-1, 0, 0}, "rechts -> -X"},
        {efx::camera::Orbit::View::Top,    {0, 0, -1}, "oben  -> -Z"},
        {efx::camera::Orbit::View::Bottom, {0, 0, 1},  "unten -> +Z"},
    };
    for (const Wanted& w : kWanted) {
        efx::camera::Orbit cam;
        cam.reset(10.0f);
        cam.lookFrom(w.view);
        // Blickrichtung = vom Auge zum Ziel.
        const efx::camera::Vec3 dir =
            efx::camera::normalise(cam.target() - cam.position());
        const float dot = dir.x * w.forward.x + dir.y * w.forward.y +
                          dir.z * w.forward.z;
        // Bei oben und unten sind es 89 statt 90 Grad — genau senkrecht ist
        // die Aufwaertsrichtung unbestimmt und das Bild kippt.
        check(dot > 0.999f, std::string("Blickrichtung ") + w.name);
    }
    std::printf("  alle sechs Ansichten zeigen entlang ihrer Achse\n");

    // 3. Abstand und Ziel bleiben, damit ein Umschalten nicht aus der Szene
    //    springt.
    {
        efx::camera::Orbit cam;
        cam.reset(10.0f);
        cam.dolly(120.0f);
        const float before = cam.distance();
        cam.lookFrom(efx::camera::Orbit::View::Top);
        check(std::fabs(cam.distance() - before) < 0.01f,
              "der Abstand bleibt beim Umschalten erhalten");
    }
}

void testGridOnWalls() {
    std::cout << "== Gitter auf Waenden und Decke ==\n";

    efx::scene::RoomSize room;
    const efx::scene::LineSet floorOnly =
        efx::scene::buildGrid(room, 10.0f, 0xFF808080u, 0xFFC0C0C0u, false);
    const efx::scene::LineSet everywhere =
        efx::scene::buildGrid(room, 10.0f, 0xFF808080u, 0xFFC0C0C0u, true);

    // floorOnly ist die grosse Ebene (ohne Raum), everywhere der Raum mit
    // allen sechs Flaechen — die Linienzahlen sind darum nicht vergleichbar.
    check(!everywhere.vertices.empty(), "mit Waenden und Decke gibt es Linien");

    // Der Boden allein liegt fast in einer Ebene — mit Waenden reicht es bis
    // zur Decke hinauf.
    float highestFloor = -1e9f, highestAll = -1e9f;
    for (const auto& v : floorOnly.vertices) highestFloor = std::max(highestFloor, v.pos[2]);
    for (const auto& v : everywhere.vertices) highestAll = std::max(highestAll, v.pos[2]);
    check(highestFloor < room.floorZ + 1.0f, "das Bodengitter liegt am Boden");
    check(highestAll > room.ceilingZ - 1.0f, "das volle Gitter reicht bis zur Decke");
    std::printf("  nur Boden: %zu Linien bis z=%.1f | alles: %zu bis z=%.1f\n",
                floorOnly.vertices.size() / 2, highestFloor,
                everywhere.vertices.size() / 2, highestAll);
}

void testLegacyRepeatMode() {
    std::cout << "== Wiederholen wie im alten Editor ==\n";

    // Zwei Arten, einen Effekt mit `repeatDelay` zu zeigen — beide richtig,
    // je nachdem, was man beurteilen will.
    //
    // Die Engine (CFxScheduler::AddLoopedEffects) legt alle `repeatDelay`
    // Millisekunden nach, ohne den laufenden Durchlauf abzubrechen: ein Feuer
    // brennt durchgehend.
    //
    // Der alte Editor laesst ihn dagegen auslaufen und faengt von vorn an.
    // Das ist gemessen, nicht vermutet: in einer Aufnahme von 75 Bildern
    // faellt seine Helligkeit ueber rund zehn Bilder auf nahezu null und
    // kommt dann zurueck. Unsere durchgehende Fassung fiel nie unter zwei
    // Drittel.
    //
    // Der Unterschied im Kern ist der VORLAUF: mit ihm steht der Bestand von
    // Anfang an voll, ohne ihn faengt er bei null an.
    efx::Effect effect;
    effect.repeatDelay = 300;
    effect.repeatDelaySet = true;
    effect.primitives.push_back(efx::Primitive{});
    auto& p = effect.primitives.back();
    p.type = efx::PrimitiveType::Particle;
    p.count = efx::Range::single(5.0f);
    p.count.set = true;
    p.life = efx::Range::single(900.0f);
    p.size.present = true;
    p.size.start = efx::Range::single(5.0f);
    p.shaders.push_back("gfx/x");

    // Engine-Fassung: mit Vorlauf.
    efx::particles::System continuous;
    continuous.play(effect, 7u, {}, {}, {}, {}, true);

    // Fassung des alten Editors: ohne Vorlauf.
    efx::particles::System restarting;
    restarting.play(effect, 7u, {}, {}, {}, {}, false);

    check(continuous.live().size() > restarting.live().size(),
          "die Engine-Fassung bringt Generationen aus der Vergangenheit mit");
    check(restarting.live().size() == 5,
          "die alte Fassung faengt mit genau einer Generation an");

    // Und der sichtbare Unterschied: am Ende ist bei der alten Fassung
    // NICHTS mehr da, bei der Engine-Fassung schon.
    const efx::camera::Vec3 right{1, 0, 0}, up{0, 0, 1};
    const float lifeEnd = restarting.durationMs();
    check(restarting.build(lifeEnd, right, up).alive == 0,
          "die alte Fassung laeuft aus — genau das ist die Luecke");
    check(continuous.build(continuous.durationMs() * 0.5f, right, up).alive > 0,
          "die Engine-Fassung brennt durch");
    std::printf("  durchgehend: %zu Live | wie frueher: %zu Live, am Ende %d\n",
                continuous.live().size(), restarting.live().size(),
                restarting.build(lifeEnd, right, up).alive);
}

void testSeamlessRepeat() {
    std::cout << "== Nahtlose Wiederholung ==\n";

    // Drei Forderungen aus dem Betrieb, alle drei gleichzeitig:
    //
    //   1. Die Bildzahl darf nicht wachsen.
    //   2. Zurueckspulen muss den Effekt zeigen, nicht ein leeres Bild.
    //   3. Es darf nicht schwaecher werden, je laenger es laeuft.
    //
    // Zwei Anlaeufe davor haben je eine davon verletzt: das Neuplanen bei
    // jedem Umlauf liess alles springen, das Nachlegen liess die Dauer auf
    // 56 Sekunden wachsen und raeumte die Vergangenheit weg.
    //
    // Die Loesung: alle Generationen mit DEMSELBEN Ausgangswert, von -K*d bis
    // +d. Dann sieht die Generation bei 0 im Alter d genau so aus wie die bei
    // -d im Alter d — nach einer Wiederholung ist das Bild identisch.
    efx::Effect effect;
    effect.repeatDelay = 300;
    effect.repeatDelaySet = true;
    effect.primitives.push_back(efx::Primitive{});
    auto& p = effect.primitives.back();
    p.type = efx::PrimitiveType::Particle;
    p.count = efx::Range::single(6.0f);
    p.count.set = true;
    p.life.min = 500.0f;
    p.life.max = 900.0f;
    p.life.ranged = true;
    p.life.set = true;
    p.velocity.min = {5.0f, -2.0f, -2.0f};
    p.velocity.max = {9.0f, 2.0f, 2.0f};
    p.velocity.ranged = true;
    p.velocity.set = true;
    p.size.present = true;
    p.size.start = efx::Range::single(5.0f);
    p.shaders.push_back("gfx/x");

    efx::particles::System system;
    system.play(effect, 7u, {}, {}, {}, {}, true);

    // 1. Die Schleife dauert MEHRERE Wiederholungen.
    //
    // Sie dauerte einmal genau eine — mit lauter gleichen Generationen. Das
    // war nahtlos und sah furchtbar aus: alle 300 ms exakt dasselbe Bild.
    // Der Anwender beschrieb es als \"als wuerde es immer dieselbe
    // Reihenfolge spielen\".
    //
    // Die Bedingung fuer Nahtlosigkeit ist nicht, dass alle Generationen
    // gleich sind, sondern dass sich ihre Ausgangswerte nach N Generationen
    // WIEDERHOLEN. Dann ist die Schleife N * repeatDelay lang und enthaelt N
    // verschiedene Generationen.
    check(system.durationMs() > 300.0f * 3.0f,
          "die Schleife dauert mehrere Wiederholungen");
    check(std::fmod(system.durationMs() + 0.5f, 300.0f) < 1.0f,
          "und zwar ein ganzes Vielfaches von repeatDelay");
    std::printf("  repeatDelay 300 ms -> Schleife %.0f ms\n",
                system.durationMs());

    const efx::camera::Vec3 right{1, 0, 0}, up{0, 0, 1};

    // 2. Bei t=0 und t=d ist das Bild IDENTISCH — nicht nur gleich gross.
    const auto atStart = system.build(0.0f, right, up);
    const auto atEnd = system.build(system.durationMs(), right, up);
    check(atStart.alive == atEnd.alive,
          "gleich viele Teilchen am Anfang und am Ende der Wiederholung");
    // Verglichen wird die MENGE der Eckpunkte, nicht ihre Reihenfolge.
    //
    // Die Teilchen stehen bei t=d in einer anderen Listenposition als bei t=0
    // — es ist ja eine andere Generation, die dieselbe Rolle spielt. Auf dem
    // Bildschirm ist das nicht zu unterscheiden, in der Liste schon.
    const auto positionsAt = [&](float t) {
        std::vector<float> all;
        for (const auto& group : system.build(t, right, up).byTexture) {
            for (const auto& vertex : group.second.vertices) {
                for (int axis = 0; axis < 3; ++axis) {
                    all.push_back(std::round(vertex.pos[axis] * 1000.0f) / 1000.0f);
                }
            }
        }
        std::sort(all.begin(), all.end());
        return all;
    };
    const bool samePicture = positionsAt(0.0f) == positionsAt(system.durationMs());
    check(samePicture, "und es sind dieselben Eckpunkte, Wert fuer Wert");

    // Und die Gegenprobe, die den dritten Anlauf verhindert haette: nach EINER
    // Wiederholung darf es NICHT dasselbe sein, sonst sieht man die
    // Wiederholung.
    check(positionsAt(0.0f) != positionsAt(300.0f),
          "nach einer einzelnen Wiederholung ist das Bild ein anderes");
    std::printf("  t=0: %d lebend, t=300: %d lebend, Bild %s\n", atStart.alive,
                atEnd.alive, samePicture ? "identisch" : "verschieden");

    // 3. Dazwischen wird es nicht duenner. Ohne die Generation bei +d fiel es
    //    gegen Ende ab — genau das war zu sehen.
    int fewest = atStart.alive, most = atStart.alive;
    for (float t = 0.0f; t <= system.durationMs(); t += 15.0f) {
        const int alive = system.build(t, right, up).alive;
        fewest = std::min(fewest, alive);
        most = std::max(most, alive);
    }
    // Die Schwelle ist 40 Prozent, nicht 50: seit sim::Random seinen
    // Ausgangswert verruehrt, sind die Generationen wirklich verschieden
    // (vorher lieferten benachbarte Ausgangswerte fast dieselben ersten
    // Zufallswerte, und alle Generationen sahen gleich aus). Mit sechs
    // Teilchen von 500 bis 900 ms schwankt der Bestand dann natuerlich
    // staerker — ein EINBRUCH waere weit darunter.
    check(fewest * 5 >= most * 2,
          "der Bestand bricht ueber die Wiederholung nicht ein");
    std::printf("  ueber die Wiederholung: %d bis %d lebend\n", fewest, most);

    // 4. Jeder Zeitpunkt laesst sich anfahren — auch rueckwaerts, auch
    //    mehrfach. Nichts wird weggeraeumt.
    const int again = system.build(0.0f, right, up).alive;
    check(again == atStart.alive,
          "zurueckspulen zeigt wieder dasselbe, nichts wurde verbraucht");
}

void testMotionMatchesEngine() {
    std::cout << "== Bewegung gegen die Engine-Formel ==\n";

    // Nicht nur \"der Quelltext sieht gleich aus\", sondern nachgerechnet.
    //
    // `CParticle::UpdateOrigin` in FxPrimitives.cpp:
    //
    //     const float time = (theFxHelper.mTime - mTimeStart) * 0.001f;
    //     VectorScale( ax[0], mVel[0], realVel );
    //     VectorMA( realVel, mVel[1], ax[1], realVel );
    //     VectorMA( realVel, mVel[2], ax[2], realVel );
    //     realVel[2] += 0.5f * mGravity * time;
    //     ... realAccel aus mAccel ...
    //     VectorMA( realVel, time, realAccel, realVel );
    //     VectorMA( org, time, realVel, mOrigin1 );
    //
    // Ausmultipliziert:
    //
    //     pos = org + vel*t + 0.5*g*t²*ẑ + accel*t²
    //
    // Das letzte Glied ist NICHT 0.5*a*t², wie es die Physik verlangt — die
    // Beschleunigung wirkt doppelt. Raven hat den Zweifel selbst notiert:
    // \"NOTE: not sure if this is even 100% correct math-wise\". Wir bauen es
    // trotzdem so nach; die Vorschau soll fliegen wie das Spiel, nicht wie
    // ein Lehrbuch.
    //
    // Die Schwerkraft wirkt nach OBEN, wenn sie positiv ist:
    //     accel[2] += fx->mGravity.GetVal();
    //     // Gravity is completely decoupled from acceleration since it is
    //     // __always__ absolute. NOTE: I only effect Z
    efx::Effect effect;
    effect.primitives.push_back(efx::Primitive{});
    auto& p = effect.primitives.back();
    p.type = efx::PrimitiveType::Particle;
    p.life = efx::Range::single(1000.0f);
    p.velocity.min = {5.0f, 0.0f, 0.0f};
    p.velocity.max = {5.0f, 0.0f, 0.0f};
    p.velocity.set = true;
    p.acceleration.min = {2.0f, 0.0f, 0.0f};
    p.acceleration.max = {2.0f, 0.0f, 0.0f};
    p.acceleration.set = true;
    p.gravity = efx::Range::single(10.0f);
    p.gravity.set = true;
    p.size.present = true;
    p.size.start = efx::Range::single(5.0f);
    p.shaders.push_back("gfx/x");

    efx::particles::System system;
    system.play(effect, 7u);
    check(!system.live().empty(), "es gibt ein Teilchen");
    if (system.live().empty()) return;

    for (const float ms : {0.0f, 250.0f, 500.0f, 1000.0f}) {
        const auto got = system.live()[0].positionAt(ms);
        const float t = ms * 0.001f;
        // \"Orient Up\": die Vorwaertsachse ist +Z, also traegt mVel[0] nach oben.
        // Der Weg, den ein in der Welt gespielter Effekt nimmt:
        //   CreateEffect: accel[2] += mGravity
        //   UpdateOrigin: schrittweise, Grenzwert 0.5*a*t²
        const float totalAccelZ = 2.0f + 10.0f;   // accel + Schwerkraft
        const float wantZ = 5.0f * t + 0.5f * totalAccelZ * t * t;
        check(std::fabs(got.z - wantZ) < 1e-4f,
              "die Hoehe stimmt mit der Engine-Formel ueberein");
        std::printf("  t=%4.0f ms: unser %8.4f, Engine %8.4f\n", ms, got.z, wantZ);
    }

    // Und die Richtung der Schwerkraft: positiv heisst nach oben.
    {
        efx::Effect falling = effect;
        falling.primitives[0].gravity = efx::Range::single(-10.0f);
        efx::particles::System down;
        down.play(falling, 7u);
        check(down.live()[0].positionAt(1000.0f).z <
                  system.live()[0].positionAt(1000.0f).z,
              "negative Schwerkraft zieht nach unten, positive nach oben");
    }
}

void testReplayDoesNotAccumulate() {
    std::cout << "== Ausloesen raeumt auf ==\n";

    // Die schlimmste Regression dieser Sitzung, und eine selbstgemachte.
    //
    // `System::stop()` wurde getrennt in \"anhalten\" (Geometrie bleibt, damit
    // man nach dem Stoppen weiter anfahren kann) und `clear()` (wegwerfen).
    // Richtig so — nur stand `stop()` auch am Anfang von `play()`.
    //
    // Damit raeumte das Ausloesen nicht mehr auf: jeder Durchlauf legte NEUE
    // Teilchen zu den alten. Bei einer Wiederholung alle 300 ms wuchs der
    // Bestand ohne Grenze. Der Anwender sah ein Feuer mit der zehnfachen
    // Dichte des Originals — und genau das war es.
    //
    // Kein Test hat das gemerkt, weil alle nur EINMAL abspielen. Dieser
    // spielt mehrfach.
    efx::Effect effect;
    effect.primitives.push_back(efx::Primitive{});
    auto& p = effect.primitives.back();
    p.type = efx::PrimitiveType::Particle;
    p.count = efx::Range::single(7.0f);
    p.count.set = true;
    p.life = efx::Range::single(1000.0f);
    p.size.present = true;
    p.size.start = efx::Range::single(5.0f);
    p.shaders.push_back("gfx/x");

    efx::particles::System system;
    system.play(effect, 7u);
    const size_t afterFirst = system.live().size();
    check(afterFirst == 7, "sieben Teilchen beim ersten Mal");

    for (int round = 0; round < 20; ++round) system.play(effect, 7u);
    check(system.live().size() == afterFirst,
          "und nach zwanzig weiteren Malen immer noch sieben");
    std::printf("  nach 21 Durchlaeufen: %zu Live (erwartet %zu)\n",
                system.live().size(), afterFirst);

    // Auch mit Vorlauf: der legt Generationen an, aber jedes Ausloesen faengt
    // von vorn an.
    {
        efx::Effect looping = effect;
        looping.repeatDelay = 300;
        looping.repeatDelaySet = true;

        efx::particles::System repeated;
        repeated.play(looping, 7u, {}, {}, {}, {}, true);
        const size_t once = repeated.live().size();
        check(once > afterFirst, "mit Vorlauf sind es mehr");
        for (int round = 0; round < 20; ++round) {
            repeated.play(looping, 7u, {}, {}, {}, {}, true);
        }
        check(repeated.live().size() == once,
              "aber auch die wachsen nicht ueber die Durchlaeufe");
        std::printf("  mit Vorlauf: %zu, nach 21 Durchlaeufen %zu\n",
                    once, repeated.live().size());
    }

    // Und die Trennung selbst: anhalten behaelt, wegwerfen nicht.
    {
        efx::particles::System keeping;
        keeping.play(effect, 7u);
        keeping.stop();
        check(!keeping.live().empty(),
              "anhalten behaelt die Geometrie (zum Anfahren)");
        keeping.clear();
        check(keeping.live().empty(), "wegwerfen raeumt sie weg");
    }
}

void testUndefinedShaderBlend() {
    std::cout << "== Mischung fuer ein Bild ohne Shaderskript ==\n";

    // Der Fall aus dem Betrieb: in einem Feuereffekt erschienen die Rauch-
    // und Funkenteilchen als sichtbare KAESTEN statt als weiche Flecken.
    //
    // Der Grund: `gfx/effects/whiteFlare` und `gfx/effects/alpha_smoke`
    // stehen in KEINER .shader-Datei einer gewoehnlichen Installation. Fuer
    // solche Namen haben wir additiv geraten — mit der Begruendung \"das ist
    // bei Effekten die haeufigste Mischung\".
    //
    // Die Engine raet nicht, sie baut einen Ersatzshader. R_FindShader in
    // tr_shader.cpp:
    //
    //     } else if ( shader.lightmapIndex[0] == LIGHTMAP_2D ) {
    //         stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
    //               GLS_SRCBLEND_SRC_ALPHA |
    //               GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    //
    // Das ist Alphamischung.
    efx::assets::Index index;
    check(index.blendOf("gfx/effects/gibtsnicht") ==
              efx::shader::BlendMode::AlphaBlend,
          "ein unbekannter Name mischt alphagemischt, nicht additiv");

    // Steht der Name dagegen in einer Shaderdatei, gilt was dort steht — die
    // Regel darf den bekannten Fall nicht ueberschreiben.
    index.shaderBlends.emplace_back("gfx/exp/feuer",
                                    efx::shader::BlendMode::Additive);
    check(index.blendOf("gfx/exp/feuer") == efx::shader::BlendMode::Additive,
          "ein bekannter Shader behaelt seine eigene Mischung");
    check(index.blendOf("GFX/EXP/Feuer") == efx::shader::BlendMode::Additive,
          "und zwar unabhaengig von Gross- und Kleinschreibung");
    std::printf("  unbekannt -> Alphamischung, bekannt -> was dasteht\n");
}

void testCameraShake() {
    std::cout << "== Kameraerschuetterung ==\n";

    // Die Kette aus der Engine, freundlicherweise von einem Mitleser
    // herausgesucht:
    //
    //   FxScheduler.cpp:  CameraShake( origin, mElasticity, mRadius, mLife )
    //   cg_effects.cpp:   if (dist > radius) return;
    //                     intensityScale = 1 - dist/radius
    //   cg_camera.cpp:    intensity > 16.0f -> 16.0f
    //                     intensity_scale = 1 - elapsed/duration
    //
    // Die Umsetzung stand laengst in camera::Shake — sie wurde nur nie
    // AUSGELOEST. Diese Pruefungen halten fest, was sie tun muss, damit die
    // Leitung, die jetzt daran haengt, auch etwas Richtiges bekommt.
    using efx::camera::Shake;

    // 1. Innerhalb der Reichweite zittert es, ausserhalb nicht.
    {
        Shake shake;
        shake.seed(7u);
        shake.trigger(8.0f, 500, 1000, 0.0f);
        shake.update(0, 90.0f);
        check(shake.active(), "nah dran zittert es");

        Shake distant;
        distant.seed(7u);
        distant.trigger(8.0f, 500, 1000, 600.0f);
        check(!distant.active(), "jenseits des Radius passiert gar nichts");
    }

    // 2. Die Staerke faellt mit dem Abstand: 1 - dist/radius.
    {
        Shake close, middle;
        close.seed(11u);
        middle.seed(11u);
        close.trigger(8.0f, 500, 1000, 0.0f);
        middle.trigger(8.0f, 500, 1000, 250.0f);
        close.update(0, 90.0f);
        middle.update(0, 90.0f);
        check(middle.currentIntensity() < close.currentIntensity() * 0.6f,
              "auf halber Entfernung etwa halb so stark");
        std::printf("  Abstand 0: %.2f,  Abstand 250 von 500: %.2f\n",
                    close.currentIntensity(), middle.currentIntensity());
    }

    // 3. Der Deckel bei 16.0 — MAX_SHAKE_INTENSITY.
    {
        Shake loud;
        loud.seed(3u);
        loud.trigger(1000.0f, 500, 1000, 0.0f);
        loud.update(0, 90.0f);
        check(loud.currentIntensity() <= Shake::kMaxIntensity + 0.01f,
              "gedeckelt auf MAX_SHAKE_INTENSITY (16.0)");
        std::printf("  Staerke 1000 angefordert -> %.2f\n",
                    loud.currentIntensity());
    }

    // 4. Ueber die Zeit auslaufend, und danach vorbei.
    {
        Shake fading;
        fading.seed(5u);
        fading.trigger(8.0f, 500, 1000, 0.0f);
        fading.update(0, 90.0f);
        const float atStart = fading.currentIntensity();
        fading.update(900, 90.0f);
        const float atEnd = fading.currentIntensity();
        check(atEnd < atStart * 0.4f, "gegen Ende deutlich schwaecher");
        fading.update(1500, 90.0f);
        check(!fading.active(), "nach der Dauer ist Schluss");
        std::printf("  bei 0 ms: %.2f, bei 900 ms: %.2f\n", atStart, atEnd);
    }

    // 5. Ohne Reichweite (radius 0) darf nichts durch Null geteilt werden.
    {
        Shake odd;
        odd.seed(2u);
        odd.trigger(8.0f, 0, 1000, 10.0f);
        odd.update(0, 90.0f);
        const float value = odd.currentIntensity();
        check(value == value, "radius 0 ergibt keine ungueltige Zahl");
    }
}

void testParallelForStress() {
    std::cout << "== parallelFor unter Dauerlast ==\n";

    // Der Fall aus dem Betrieb: beim Umschalten von \"Alle animieren\" lief in
    // jedem Bild ein parallelFor ueber die sichtbaren Kacheln. Die Fassung
    // von damals legte Sperre und Signal als LOKALE Variablen an; der
    // wartende Faden zerstoerte sie, waehrend der letzte Arbeitsfaden noch in
    // `notify_all` steckte.
    //
    // Beim Anwender:  CRASH code 0xC0000008  (STATUS_INVALID_HANDLE)
    //
    // Nur in der MinGW-Fassung, weil deren condition_variable ein
    // Betriebssystem-Handle mitfuehrt. Die mit Visual Studio gebaute benutzt
    // SRWLOCK und CONDITION_VARIABLE, also gar keins — dort blieb derselbe
    // Fehler unsichtbar.
    //
    // Dieser Test beweist die Zerstoerung nicht (dafuer braeuchte es die
    // richtige Umsetzung und viel Glueck). Er prueft, was pruefbar ist: dass
    // bei vielen kurzen Durchlaeufen JEDE Portion genau einmal abgearbeitet
    // wird — und er ist die Stelle, an der ein Fadenwaechter zuschlagen kann.
    efx::jobs::Pool pool(8);
    std::atomic<long long> total{0};

    constexpr int kRounds = 2000;
    constexpr size_t kItems = 64;
    std::vector<std::atomic<int>> seen(kItems);
    for (auto& counter : seen) counter.store(0);

    for (int round = 0; round < kRounds; ++round) {
        pool.parallelFor(kItems, 1, [&](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                seen[i].fetch_add(1, std::memory_order_relaxed);
                total.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    check(total.load() == static_cast<long long>(kRounds) * kItems,
          "jede Portion genau einmal, ueber alle Durchlaeufe");
    bool everyItem = true;
    for (auto& counter : seen) {
        if (counter.load() != kRounds) everyItem = false;
    }
    check(everyItem, "und keine Portion ausgelassen oder doppelt");
    std::printf("  %d Durchlaeufe x %zu Portionen = %lld\n",
                kRounds, kItems, total.load());

    // Verschachtelt: ein parallelFor aus einem parallelFor heraus. Mit einer
    // gemeinsamen Sperre im Verteiler muss das gehen, sonst waere die
    // Korrektur schlimmer als der Fehler.
    {
        std::atomic<int> inner{0};
        pool.parallelFor(4, 1, [&](size_t, size_t) {
            pool.parallelFor(8, 1, [&](size_t b, size_t e) {
                inner.fetch_add(static_cast<int>(e - b), std::memory_order_relaxed);
            });
        });
        check(inner.load() == 32, "verschachtelte Aufrufe blockieren einander nicht");
    }
}

void testIndexOverflow() {
    std::cout << "== Die Geometrie laeuft nicht ueber ==\n";

    // Die Indizes sind 16 Bit. Ueber 65535 Eckpunkten lief
    // `static_cast<uint16_t>(vertices.size())` still ueber, und die Indizes
    // zeigten auf FREMDE Eckpunkte — das Bild wird zu Fetzen, ohne dass
    // irgendwo etwas gemeldet wird.
    //
    // Die vorhandene Grenze von 4096 Teilchen je Segment schuetzt davor NICHT:
    // fuenf Segmente mit demselben Shader landen in DERSELBEN Geometrie und
    // ergeben 81920 Eckpunkte. Genau das war der Fall.
    efx::Effect effect;
    for (int i = 0; i < 5; ++i) {
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(4096.0f);
        p.life = efx::Range::single(1000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(5.0f);
        p.shaders.push_back("gfx/derselbe");   // derselbe Shader!
    }

    efx::particles::System system;
    system.play(effect, 7u);
    const auto list = system.build(100.0f, {1, 0, 0}, {0, 0, 1});

    check(!list.byTexture.empty(), "es entsteht ueberhaupt Geometrie");
    for (const auto& group : list.byTexture) {
        const auto& mesh = group.second;
        check(mesh.vertices.size() <= 65535,
              "nicht mehr Eckpunkte, als ein 16-Bit-Index ansprechen kann");
        bool pointsOutside = false;
        for (const auto index : mesh.indices) {
            if (index >= mesh.vertices.size()) pointsOutside = true;
        }
        check(!pointsOutside, "und jeder Index zeigt auf einen eigenen Eckpunkt");
        std::printf("  %zu Eckpunkte, %zu Indizes, %d Vierecke weggelassen\n",
                    mesh.vertices.size(), mesh.indices.size(), list.skipped);
    }
    check(list.skipped > 0,
          "die weggelassenen werden gezaehlt, statt still zu verschwinden");

    // Der uebliche Fall darf davon nichts merken.
    {
        efx::Effect small;
        small.primitives.push_back(efx::Primitive{});
        auto& p = small.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(50.0f);
        p.life = efx::Range::single(1000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(5.0f);
        p.shaders.push_back("gfx/klein");

        efx::particles::System tiny;
        tiny.play(small, 7u);
        const auto plain = tiny.build(100.0f, {1, 0, 0}, {0, 0, 1});
        check(plain.skipped == 0, "ein normaler Effekt verliert nichts");
    }
}

void testWallTextures() {
    std::cout << "== Wandtexturen gegen Ravens Dateien ==\n";

    // Wir duerfen Ravens brick.jpg, dirt.jpg und stucco.jpg nicht beilegen,
    // also rechnen wir Muster. Damit sie nicht beliebig aussehen, sind
    // Helligkeit und Kontrast an den Originalen GEMESSEN:
    //
    //     stucco  mittlere Helligkeit 138.5  Streuung 21.7  R144 G140 B115
    //     dirt                         93.7           10.7  R128 G84  B55
    //     brick                       103.0           21.4  R129 G96  B73
    //
    // Der erste Entwurf lag weit daneben: neutrales Hellgrau mit einer
    // mittleren Helligkeit von 236 statt 138, und Putz war mit einer Streuung
    // von 9.7 fast glatt. Eingefaerbt wurde er dann vom Thema — bei dunklem
    // Thema ergab das einen gruenlichen Raum.
    struct Want {
        efx::scene::WallTexture kind;
        const char* name;
        double brightness;
        double spread;
    };
    const Want wanted[] = {
        {efx::scene::WallTexture::Stucco, "Putz",   138.5, 21.7},
        {efx::scene::WallTexture::Dirt,   "Erde",    93.7, 10.7},
        {efx::scene::WallTexture::Brick,  "Ziegel", 103.0, 21.4},
    };

    for (const auto& want : wanted) {
        const auto pixels = efx::scene::buildWallTexture(
            want.kind, 256, efx::scene::wallColourOf(want.kind));
        check(!pixels.empty(), "die Textur entsteht ueberhaupt");
        if (pixels.empty()) continue;

        double sum = 0.0, sumSquares = 0.0;
        size_t count = 0;
        for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
            const double grey = 0.299 * pixels[i] + 0.587 * pixels[i + 1] +
                                0.114 * pixels[i + 2];
            sum += grey;
            sumSquares += grey * grey;
            ++count;
        }
        const double mean = sum / static_cast<double>(count);
        const double spread =
            std::sqrt(sumSquares / static_cast<double>(count) - mean * mean);

        // Grosszuegige Schranken: es geht um \"sieht aus wie\", nicht um
        // Gleichheit. Eine enge Schranke waere hier eine feste Zahl, an der
        // jede Verbesserung des Musters scheitern wuerde.
        check(std::fabs(mean - want.brightness) < 15.0,
              "die Helligkeit liegt bei Ravens Wert");
        check(std::fabs(spread - want.spread) < 8.0,
              "und die Streuung ebenfalls");
        std::printf("  %-7s unser %5.1f / %4.1f     Raven %5.1f / %4.1f\n",
                    want.name, mean, spread, want.brightness, want.spread);
    }

    // Die Farbe muss aus der Textur kommen, nicht aus dem Thema. Ein
    // neutralgraues Muster waere genau der alte Zustand.
    {
        const uint32_t colour =
            efx::scene::wallColourOf(efx::scene::WallTexture::Dirt);
        const int r = static_cast<int>(colour & 0xFF);
        const int b = static_cast<int>((colour >> 16) & 0xFF);
        check(r > b + 40, "Erde ist deutlich roetlich, nicht grau");
    }
    {
        const uint32_t colour =
            efx::scene::wallColourOf(efx::scene::WallTexture::None);
        check((colour & 0xFFFFFF) == 0xFFFFFF,
              "ohne Textur bleibt es weiss - dann faerbt das Thema");
    }

    // Nahtlos kachelbar: sonst sieht man auf einer langen Wand ein Raster.
    // Geprueft am Sprung zwischen letzter und erster Spalte.
    {
        constexpr int kSize = 128;
        const auto pixels = efx::scene::buildWallTexture(
            efx::scene::WallTexture::Stucco, kSize,
            efx::scene::wallColourOf(efx::scene::WallTexture::Stucco));
        double seam = 0.0, inside = 0.0;
        for (int y = 0; y < kSize; ++y) {
            const size_t last = (static_cast<size_t>(y) * kSize + kSize - 1) * 4;
            const size_t first = (static_cast<size_t>(y) * kSize) * 4;
            const size_t middle = (static_cast<size_t>(y) * kSize + kSize / 2) * 4;
            seam += std::fabs(static_cast<double>(pixels[last]) - pixels[first]);
            inside += std::fabs(static_cast<double>(pixels[middle]) -
                                pixels[middle + 4]);
        }
        check(seam < inside * 3.0 + 10.0,
              "der Uebergang an der Kachelkante faellt nicht auf");
        std::printf("  Kachelkante: Sprung %.1f, im Muster %.1f\n",
                    seam / kSize, inside / kSize);
    }
}

void testAnimFrames() {
    std::cout << "== Bildfolgen: welches Bild wann ==\n";

    // Fundstelle `RB_ComputeAnimatedImage` in tr_shade.cpp:
    //
    //     index = Q_ftol( floatTime * imageAnimationSpeed * FUNCTABLE_SIZE );
    //     index >>= FUNCTABLE_SIZE2;
    //     if ( oneShotAnimMap ) { if (index >= count) index = count - 1; }
    //     else                    index %= count;
    //
    // Eine Explosion IST eine Bildfolge. Wir haben bisher ihr erstes Bild
    // gezeigt und es dabei belassen.
    using efx::shader::animFrameAt;

    // Acht Bilder mit 6 je Sekunde, einmalig — genau wie
    // gfx/exp/rocket_explosion.
    check(animFrameAt(8, 6.0f, true, 0.00f) == 0, "bei null das erste Bild");
    check(animFrameAt(8, 6.0f, true, 0.10f) == 0, "kurz danach immer noch");
    check(animFrameAt(8, 6.0f, true, 0.20f) == 1, "nach einer Sechstelsekunde das zweite");
    check(animFrameAt(8, 6.0f, true, 1.00f) == 6, "nach einer Sekunde das siebte");
    check(animFrameAt(8, 6.0f, true, 5.00f) == 7,
          "einmalig: danach bleibt es auf dem letzten stehen");
    std::printf("  8 Bilder, 6/s, einmalig: t=1.0 s -> Bild %d, t=5.0 s -> Bild %d\n",
                animFrameAt(8, 6.0f, true, 1.0f), animFrameAt(8, 6.0f, true, 5.0f));

    // Dasselbe in Schleife: da faengt es wieder von vorn an.
    check(animFrameAt(8, 6.0f, false, 5.00f) == (30 % 8),
          "in Schleife laeuft es rundherum");
    check(animFrameAt(8, 6.0f, false, 8.0f / 6.0f) == 0,
          "nach genau einem Durchlauf wieder das erste");

    // Abschneiden, nicht runden — die Engine wirft die Nachkommastellen weg.
    // Wer rundet, ist ein halbes Bild voraus.
    check(animFrameAt(8, 6.0f, true, 0.99f / 6.0f) == 0,
          "knapp vor dem Wechsel noch das alte Bild");

    // Unsinn darf nicht in eine Division oder einen Zugriff daneben laufen.
    check(animFrameAt(1, 6.0f, false, 99.0f) == 0, "ein Bild bleibt ein Bild");
    check(animFrameAt(0, 6.0f, false, 1.0f) == 0, "keine Bilder ergibt null");
    check(animFrameAt(8, 0.0f, false, 1.0f) == 0, "Bildrate null ergibt das erste");
    check(animFrameAt(8, 6.0f, false, -3.0f) == 0,
          "zurueckgespult wird nicht negativ (\"shader time offsets\")");
    for (int n = 1; n < 12; ++n) {
        for (float t = 0.0f; t < 4.0f; t += 0.13f) {
            const int f = animFrameAt(n, 6.0f, t > 2.0f, t);
            if (f < 0 || f >= n) {
                check(false, "der Bildindex bleibt immer im gueltigen Bereich");
                return;
            }
        }
    }
    check(true, "der Bildindex bleibt immer im gueltigen Bereich");
}

void testDiscoverRoots() {
    std::cout << "== Spielordner unter einem Ordner finden ==\n";

    // Der Anwender zeigt auf `GameData` und will base, seine Mod und den
    // eigenen Arbeitsordner auf einmal.
    //
    // Einfach rekursiv zu durchsuchen genuegt dafuer NICHT: die ausgepackten
    // Dateien bekaemen Pfade wie `base/gfx/...` statt `gfx/...`, und nichts
    // wuerde mehr aufgeloest. Es muessen die WURZELN gefunden und einzeln
    // durchsucht werden.
    namespace fs = std::filesystem;
    const fs::path game = fs::temp_directory_path() / "efxed_gamedata";
    fs::remove_all(game);
    fs::create_directories(game / "base" / "effects" / "blaster");
    fs::create_directories(game / "base" / "shaders");
    fs::create_directories(game / "MeineMod" / "effects" / "eigen");
    fs::create_directories(game / "Arbeitsordner" / "effects");
    // Ein Ordner, der KEIN Spielordner ist — er darf nicht dabei sein.
    fs::create_directories(game / "Screenshots" / "2026");

    { std::ofstream(game / "base" / "effects" / "blaster" / "shot.efx")
          << "Particle\n{\n\tlife\t100\n}\n"; }
    { std::ofstream(game / "MeineMod" / "effects" / "eigen" / "meiner.efx")
          << "Particle\n{\n\tlife\t100\n}\n"; }
    { std::ofstream(game / "Arbeitsordner" / "effects" / "entwurf.efx")
          << "Particle\n{\n\tlife\t100\n}\n"; }
    { std::ofstream(game / "Screenshots" / "2026" / "bild.jpg") << "x"; }

    const auto roots = efx::assets::discoverRoots(game.string());
    check(roots.size() == 3, "drei Spielordner, nicht vier");
    bool hasScreenshots = false;
    for (const auto& r : roots) {
        if (r.find("Screenshots") != std::string::npos) hasScreenshots = true;
    }
    check(!hasScreenshots, "ein Bilderordner ist kein Spielordner");
    std::printf("  %zu Wurzeln gefunden\n", roots.size());

    // Immer dieselbe Reihenfolge — sonst wechselt die Ueberschreibregel von
    // Lauf zu Lauf, und das waere schlimmer als gar keine.
    const auto again = efx::assets::discoverRoots(game.string());
    check(roots == again, "die Reihenfolge ist bei jedem Lauf dieselbe");

    // Zeigt man direkt auf einen Spielordner, ist er selbst das Ergebnis —
    // und nicht seine Unterordner.
    const auto direct = efx::assets::discoverRoots((game / "base").string());
    check(direct.size() == 1, "ein Spielordner findet sich selbst");

    // Und jetzt der eigentliche Zweck: alles zusammen lesen und sagen
    // koennen, woher jeder Effekt kommt.
    const auto index = efx::assets::scanAll(roots);
    check(index.effects.size() == 3, "alle drei Effekte sind da");
    check(index.sourceOf("blaster/shot").find("base") != std::string::npos,
          "shot kommt aus base");
    check(index.sourceOf("eigen/meiner").find("MeineMod") != std::string::npos,
          "meiner kommt aus der Mod");
    check(index.sourceOf("entwurf").find("Arbeitsordner") != std::string::npos,
          "entwurf aus dem Arbeitsordner");
    check(index.sourceOf("gibt/es/nicht").empty(),
          "und ein unbekannter Name hat keine Herkunft");
    std::printf("  blaster/shot <- %s\n",
                index.sourceOf("blaster/shot").c_str());

    fs::remove_all(game);
}

void testRepeatBuildUp() {
    std::cout << "== repeatDelay: Generationen ueberlagern sich ==\n";

    // Fundstelle `CFxScheduler::AddLoopedEffects` in FxScheduler.cpp:
    //
    //     if (mLoopedEffectArray[i].mNextTime < theFxHelper.mTime) {
    //         PlayEffect( ... );   // noch einmal, ZUSAETZLICH
    //         mLoopedEffectArray[i].mNextTime = theFxHelper.mTime + mRepeatDelay;
    //
    // Der laufende Effekt wird NICHT abgebrochen. Bei t_small_fire.efx leben
    // Teilchen 500..1400 ms und werden alle 300 ms nachgelegt — daraus wird
    // ein stehendes Feuer statt einzelner Flaemmchen.
    efx::Effect effect;
    effect.repeatDelay = 300;
    effect.repeatDelaySet = true;
    effect.primitives.push_back(efx::Primitive{});
    auto& p = effect.primitives.back();
    p.type = efx::PrimitiveType::Particle;
    p.count = efx::Range::single(4.0f);
    p.life = efx::Range::single(1200.0f);
    p.size.present = true;
    p.size.start = efx::Range::single(5.0f);
    p.shaders.push_back("gfx/flamme");

    const efx::camera::Vec3 right{1, 0, 0}, up{0, 1, 0};

    efx::particles::System single;
    single.play(effect, 7u, {}, {}, {}, {}, false);

    efx::particles::System stacked;
    stacked.play(effect, 7u, {}, {}, {}, {}, true);

    const int aliveSingle = single.build(0.0f, right, up).alive;
    const int aliveStacked = stacked.build(0.0f, right, up).alive;
    check(aliveStacked > aliveSingle,
          "mit Vorlauf leben mehr Teilchen als bei einem Durchlauf");
    // 1200 ms Leben bei 300 ms Abstand -> vier Generationen.
    check(aliveStacked >= aliveSingle * 3,
          "und zwar ungefaehr so viele Generationen, wie hineinpassen");
    std::printf("  Leben 1200 ms, Wiederholung 300 ms: %d -> %d lebend\n",
                aliveSingle, aliveStacked);

    // Die Dauer bleibt die volle Lebensdauer — auch mit Vorlauf.
    //
    // Hier stand einmal das Gegenteil: die Dauer wurde auf `repeatDelay`
    // gekuerzt. Beim Anwender sprang die Zeitleiste dadurch beim Umschalten
    // auf Wiederholen von 62 Sekunden auf 300 Millisekunden — neun Bilder
    // statt zweitausend, und der Rest war nicht mehr anzufahren.
    //
    // Die gekuerzte Dauer ist eine Frage der ANZEIGE: nur die Browserkacheln
    // wollen ueber genau eine Wiederholung laufen, und die stellen ihre Uhr
    // selbst darauf ein.
    // Mit Vorlauf ist die Dauer EINE Wiederholung.
    //
    // Diese Zusicherung hat dreimal die Seite gewechselt, deshalb hier die
    // Begruendung, die bleibt: nach `repeatDelay` ist das Bild identisch,
    // weil alle Generationen denselben Ausgangswert haben. Eine laengere
    // Zeitleiste zeigte danach nur noch Wiederholung; eine kuerzere gibt es
    // nicht.
    //
    // Die beiden Alternativen sind ausprobiert und verworfen: die volle
    // Lebensdauer laufen zu lassen und am Ende neu zu planen liess alles
    // springen (dreimal je Sekunde), und waehrend der Wiedergabe nachzulegen
    // liess die Dauer auf 56 Sekunden wachsen und raeumte die Vergangenheit
    // weg, sodass Zurueckspulen ein leeres Bild zeigte.
    // Die Schleife ist ein ganzes Vielfaches von `repeatDelay` — so viele
    // Wiederholungen, wie es verschiedene Generationen gibt.
    check(stacked.durationMs() >= 300.0f * 3.0f,
          "mit Vorlauf dauert die Vorschau mehrere Wiederholungen");
    check(std::fmod(stacked.durationMs() + 0.5f, 300.0f) < 1.0f,
          "und zwar ein ganzes Vielfaches davon");

    // Und die Ausnahme: reicht der Vorlauf nicht, bleibt die volle Dauer.
    //
    // Die kurze Schleife setzt einen eingeschwungenen Bestand voraus. Bei
    // einem Rauch, der 52 Sekunden lebt und alle 300 ms nachgelegt wird,
    // waeren das 206 Generationen — der Deckel bei 16 verhindert das. Dann
    // ist die Schleife nicht nahtlos, und eine Zeitleiste ueber 300 ms waere
    // eine Luege: sie zeigte einen Sprung als Wiederholung.
    {
        efx::Effect longLived = effect;
        longLived.primitives[0].life = efx::Range::single(60000.0f);
        longLived.primitives[0].life.set = true;

        efx::particles::System slow;
        slow.play(longLived, 7u, {}, {}, {}, {}, true);
        check(slow.durationMs() > 1000.0f,
              "reicht der Vorlauf nicht, bleibt die volle Dauer stehen");
        std::printf("  Leben 60 s bei 300 ms Wiederholung -> Dauer %.0f ms\n",
                    slow.durationMs());
    }

    // Eingeschwungen von Anfang an: der Vorlauf liegt in der VERGANGENHEIT.
    // Legte man vorwaerts nach, waere t=0 leer und man saehe den Aufbau.
    const int aliveLate = stacked.build(290.0f, right, up).alive;
    check(aliveLate > aliveSingle,
          "auch am Ende der Wiederholung ist der Bestand voll");

    // Der Vorlauf loest nichts Einmaliges aus.
    //
    // Der Fall aus dem Betrieb: smoke_explosion.efx hat ein Sound-Segment und
    // `repeatDelay 300`. Mit sechzehn Vorlaufgenerationen trug JEDE ihren
    // Klang, und weil ihr Zeitpunkt laengst erreicht war, gingen beim Start
    // alle gleichzeitig los — beim Anwender ein Klang, der sich acht- bis
    // neunmal ueberlagerte und erst aufhoerte, als die Puffergrenze des
    // Tongeraets erreicht war.
    //
    // Ein Klang, der vor 4.8 Sekunden begonnen hat, ist vorbei.
    {
        efx::Effect noisy = effect;
        noisy.primitives.push_back(efx::Primitive{});
        auto& sound = noisy.primitives.back();
        sound.type = efx::PrimitiveType::Sound;
        sound.sounds.push_back("sound/weapons/rocket/hit_wall.wav");

        efx::particles::System withPreroll;
        withPreroll.play(noisy, 7u, {}, {}, {}, {}, true);

        int sounds = 0, fromPast = 0;
        for (const auto& item : withPreroll.live()) {
            if (item.soundName.empty()) continue;
            ++sounds;
            if (item.spawnMs < 0.0f) ++fromPast;
        }
        check(fromPast == 0, "kein Klang aus dem Vorlauf");
        check(sounds == 1, "genau ein Klang, nicht einer je Generation");
        std::printf("  mit Sound-Segment: %d Klang, %d aus dem Vorlauf\n",
                    sounds, fromPast);
    }

    // Ohne repeatDelay aendert das Flag nichts — sonst wuerde man beim
    // Beurteilen eines einzelnen Segments hereingelegt.
    {
        efx::Effect once = effect;
        once.repeatDelay = 0;
        once.repeatDelaySet = false;
        efx::particles::System a, b;
        a.play(once, 7u, {}, {}, {}, {}, false);
        b.play(once, 7u, {}, {}, {}, {}, true);
        check(a.live().size() == b.live().size(),
              "ohne repeatDelay bleibt alles wie zuvor");
    }

    // Ein unsinnig kleiner Abstand darf nicht in zehntausende Teilchen laufen.
    {
        efx::Effect fast = effect;
        fast.repeatDelay = 1;
        efx::particles::System many;
        many.play(fast, 7u, {}, {}, {}, {}, true);
        // Die Obergrenze ist jetzt hoeher: zu den Generationen aus der
        // Vergangenheit (gedeckelt auf 16) kommen die aus der Zukunft, die
        // das Fenster fuellen — hoechstens 16 weitere. Dass es ueberhaupt
        // gedeckelt ist, bleibt der Punkt: `repeatDelay 1` bei langer
        // Lebensdauer wuerde sonst Zehntausende erzeugen.
        check(many.live().size() <= single.live().size() * 40,
              "die Zahl der Generationen ist gedeckelt");
        std::printf("  repeatDelay 1 ms: %zu Live bei %zu je Generation\n",
                    many.live().size(), single.live().size());
        std::printf("  repeatDelay 1 ms: %zu Live (gedeckelt)\n",
                    many.live().size());
    }
}

void testFindSound() {
    std::cout << "== Klangdateien finden ==\n";

    // Der Fall aus dem Betrieb:
    //
    //     WARNING sound not found: sound/weapons/rocket/hit_wall.wav
    //
    // bei 29839 Klaengen im Bestand. Der Grund steht in der Engine,
    // S_LoadSound_Actual in snd_mem.cpp:
    //
    //     *piSize = FS_ReadFile( psFilename, ... );      // try WAV
    //     if ( !*pData ) {
    //         psFilename[len-3] = 'm'; ... = 'p'; ... = '3';
    //         *piSize = FS_ReadFile( psFilename, ... );  // try MP3
    //
    // In fast jeder Raven-.efx steht `.wav`, im `.pk3` liegt die `.mp3`.
    namespace fs = std::filesystem;
    const fs::path base = fs::temp_directory_path() / "efxed_findsound";
    fs::remove_all(base);
    fs::create_directories(base / "sound" / "weapons" / "rocket");
    fs::create_directories(base / "sound" / "weapons" / "blaster");

    // Nur als .mp3 vorhanden — die .efx wird trotzdem .wav sagen.
    { std::ofstream(base / "sound" / "weapons" / "rocket" / "hit_wall.mp3") << "x"; }
    // Und einer, den es wirklich als .wav gibt.
    { std::ofstream(base / "sound" / "weapons" / "blaster" / "fire.wav") << "x"; }

    const auto index = efx::assets::scan(base.string());

    {
        const auto found = efx::assets::findSound(
            index, base.string(), "sound/weapons/rocket/hit_wall.wav");
        check(found.found, ".wav angefragt, .mp3 vorhanden -> gefunden");
        check(found.path == "sound/weapons/rocket/hit_wall.mp3",
              "und zwar als .mp3, wie die Engine es tut");
        std::printf("  %s -> %s\n", "sound/weapons/rocket/hit_wall.wav",
                    found.path.c_str());
    }

    {
        const auto found = efx::assets::findSound(
            index, base.string(), "sound/weapons/blaster/fire.wav");
        check(found.found && found.path == "sound/weapons/blaster/fire.wav",
              "eine wirklich vorhandene .wav gewinnt vor der Ersatzendung");
    }

    {
        const auto found = efx::assets::findSound(
            index, base.string(), "sound/gibt/es/nicht.wav");
        check(!found.found, "was es nicht gibt, wird auch nicht gefunden");
    }

    // Rueckwaerts: .mp3 angefragt, nur .wav vorhanden. Kommt seltener vor,
    // aber die Regel soll nicht nur in eine Richtung gelten.
    {
        const auto found = efx::assets::findSound(
            index, base.string(), "sound/weapons/blaster/fire.mp3");
        check(found.found && found.path == "sound/weapons/blaster/fire.wav",
              "und umgekehrt ebenso");
    }

    fs::remove_all(base);
}

void testSoundConvert() {
    std::cout << "== Klaenge auf das Geraeteformat bringen ==\n";

    // Anlass: eine MP3 mit 44100 Hz wurde abgespielt, eine WAV mit 11025 Hz
    // nicht. Beide wurden fehlerfrei gelesen. Der Unterschied lag darin, dass
    // wir das Tongeraet mit der GENAUEN Rate der Datei geoeffnet haben —
    // 44100 ist die Rate des Geraets, 11025 lehnt Windows je nach Treiber ab.
    //
    // Jetzt wird alles auf ein festes Format gebracht. Diese Umrechnung ist
    // die Stelle, an der etwas leise kaputtgehen kann: falsche Laenge, halbe
    // Lautstaerke, verlorene Kanaele.
    using efx::sound::convert;

    // Ein Ton, den man wiedererkennt: eine Sekunde bei 11025 Hz, mono.
    std::vector<int16_t> mono;
    for (int i = 0; i < 11025; ++i) {
        mono.push_back(static_cast<int16_t>(
            20000.0 * std::sin(2.0 * 3.14159265 * 440.0 * i / 11025.0)));
    }

    // 1. Hochrechnen auf 44100: viermal so viele Werte, gleiche Lautstaerke.
    {
        const auto up = convert(mono, 11025, 1, 44100, 1);
        check(up.size() >= mono.size() * 4 - 4 && up.size() <= mono.size() * 4 + 4,
              "viermal die Rate ergibt viermal so viele Werte");
        int peak = 0;
        for (int16_t v : up) peak = std::max(peak, std::abs(static_cast<int>(v)));
        check(peak > 19000 && peak <= 20500,
              "und die Lautstaerke bleibt, was sie war");
        std::printf("  11025 -> 44100 Hz: %zu -> %zu Werte, Spitze %d\n",
                    mono.size(), up.size(), peak);
    }

    // 2. Mono auf zwei Kanaele: beide bekommen dasselbe, nichts wird leiser.
    {
        const auto stereo = convert(mono, 11025, 1, 11025, 2);
        check(stereo.size() == mono.size() * 2, "zwei Kanaele, doppelt so viele Werte");
        bool sameBoth = true;
        for (size_t i = 0; i + 1 < stereo.size(); i += 2) {
            if (stereo[i] != stereo[i + 1]) sameBoth = false;
        }
        check(sameBoth, "Mono liegt auf beiden Lautsprechern gleich (also mittig)");
    }

    // 3. Stereo auf Mono: gemittelt, nicht halbiert und nicht abgeschnitten.
    {
        std::vector<int16_t> stereo;
        for (int i = 0; i < 100; ++i) {
            stereo.push_back(1000);   // links
            stereo.push_back(3000);   // rechts
        }
        const auto down = convert(stereo, 22050, 2, 22050, 1);
        check(down.size() == 100, "aus 100 Bildern Stereo werden 100 Werte Mono");
        check(!down.empty() && down[0] > 1900 && down[0] < 2100,
              "und der Wert ist der Mittelwert beider Seiten, nicht nur einer");
        std::printf("  Stereo 1000/3000 -> Mono %d\n", down.empty() ? 0 : down[0]);
    }

    // 4. Gleiches Format ist ein Durchreicher — kein stiller Qualitaetsverlust
    //    bei Klaengen, die ohnehin passen.
    {
        const auto same = convert(mono, 11025, 1, 11025, 1);
        check(same.size() == mono.size(), "gleiches Format, gleiche Laenge");
        bool identical = same.size() == mono.size();
        for (size_t i = 0; identical && i < same.size(); ++i) {
            if (same[i] != mono[i]) identical = false;
        }
        check(identical, "und Wert fuer Wert unveraendert");
    }

    // 5. Unsinn darf nicht abstuerzen.
    {
        check(convert({}, 11025, 1, 44100, 1).empty(), "leer bleibt leer");
        check(convert(mono, 0, 1, 44100, 1).empty(), "Rate null ergibt nichts");
        check(convert(mono, 11025, 0, 44100, 1).empty(), "null Kanaele ebenso");
    }
}

void testLightDrawsNothing() {
    std::cout << "== Ein Licht zeichnet nichts ==\n";

    // Fundstelle: CLight::Draw in FxPrimitives.cpp. Der ganze Rumpf ist
    //
    //     theFxHelper.AddLightToScene( mOrigin1, mRefEnt.radius, ... );
    //
    // Ein Licht erhellt die Umgebung; eine eigene Flaeche hat es nicht.
    //
    // Bei uns fiel der Typ in den Standardzweig und wurde ein Billboard —
    // ohne Shader also ein weisses Viereck in Groesse des Lichtradius. In
    // side_alt_explosion.efx hat das Segment \"Flash\" den Radius 350; das war
    // der grosse helle Kasten mitten in der Explosion.
    efx::Effect effect;
    effect.primitives.push_back(efx::Primitive{});
    auto& light = effect.primitives.back();
    light.type = efx::PrimitiveType::Light;
    light.count = efx::Range::single(1.0f);
    light.life = efx::Range::single(500.0f);
    light.size.present = true;
    light.size.start = efx::Range::single(350.0f);

    efx::particles::System system;
    system.play(effect, 1u);
    check(!system.live().empty(), "das Licht wird ausgeloest");

    const auto list = system.build(100.0f, efx::camera::Vec3{1, 0, 0},
                                   efx::camera::Vec3{0, 1, 0});
    check(list.alive >= 1, "es lebt");
    check(list.drawn == 0, "aber es wird nichts gezeichnet");
    size_t vertices = 0;
    for (const auto& group : list.byTexture) vertices += group.second.vertices.size();
    check(vertices == 0, "und es entstehen keine Eckpunkte");
    std::printf("  Licht Radius 350: %d lebend, %d gezeichnet, %zu Eckpunkte\n",
                list.alive, list.drawn, vertices);
}

void testAnimMapIndexing() {
    std::cout << "== Bildfolgen im Materialbestand ==\n";

    // Der Fall aus dem Betrieb: side_alt_explosion wurde als weisser Klotz
    // gezeichnet. Die Ursache lag nicht im Effekt und nicht im Zerleger,
    // sondern im Bestandsverzeichnis.
    //
    // Ein Shader wie
    //
    //     gfx/exp/rocket_explosion
    //     {
    //         { oneshotanimmap 6 gfx/exp/rocket_1.tga ... }
    //     }
    //
    // hat KEIN `map` — die erste Stufe ist eine Bildfolge. Der Bestand nahm
    // nur `map`, liess den Shader also aus, und findTexture fiel danach auf
    // \"der Shadername ist der Bildname\" zurueck. `gfx/exp/rocket_explosion.tga`
    // gibt es nicht, die Textur galt als fehlend, und der Ersatzfleck saettigt
    // bei additiver Mischung zu Weiss.
    namespace fs = std::filesystem;
    const fs::path base = fs::temp_directory_path() / "efxed_animmap";
    fs::remove_all(base);
    fs::create_directories(base / "shaders");
    {
        std::ofstream out(base / "shaders" / "explosions.shader");
        out << "gfx/exp/rocket_explosion\n{\n\tcull disable\n\t{\n"
               "\t\toneshotanimmap 6 gfx/exp/rocket_1.tga gfx/exp/rocket_2.tga\n"
               "\t\tblendFunc GL_ONE GL_ONE\n\t}\n}\n"
               "gfx/geklemmt\n{\n\t{\n\t\tclampmap gfx/misc/rand.tga\n"
               "\t\tblendFunc GL_ONE GL_ONE\n\t}\n}\n"
               "gfx/gewoehnlich\n{\n\t{\n\t\tmap gfx/misc/schlicht.tga\n"
               "\t\tblendFunc GL_ONE GL_ONE\n\t}\n}\n";
    }

    const auto index = efx::assets::scan(base.string());

    check(index.mapOf("gfx/exp/rocket_explosion") == "gfx/exp/rocket_1.tga",
          "Bildfolge: das erste Bild der Folge zaehlt");
    check(index.mapOf("gfx/geklemmt") == "gfx/misc/rand.tga",
          "geklemmtes Bild zaehlt ebenfalls");
    check(index.mapOf("gfx/gewoehnlich") == "gfx/misc/schlicht.tga",
          "und ein gewoehnliches map weiterhin auch");
    std::printf("  oneshotanimmap -> %s\n",
                index.mapOf("gfx/exp/rocket_explosion").c_str());

    // Die Mischart muss mitkommen: sie stand im selben Zweig und fiel mit ihm
    // aus. Ohne sie raet die Vorschau, und additiv geratener Rauch ist
    // unsichtbar.
    check(index.blendOf("gfx/exp/rocket_explosion") ==
              efx::shader::BlendMode::Additive,
          "und die Mischart der Bildfolge kommt mit");

    fs::remove_all(base);
}

void testSheetUv() {
    std::cout << "== Ausschnitt aus dem Blatt (gedrehte Textur) ==\n";

    // Der echte Fall aus dem Betrieb, mit den Zahlen aus dem Bildschirmfoto:
    // Blatt 1792 x 1568 (8 x 7 Kacheln zu 224), OpenGL.
    //
    // Beruehrt wurde \"sparks\" auf Platz 35 bei y 896. Gezeigt wurde
    // \"flamejet\" auf Platz 19 bei y 448 — weil oben und unten getauscht
    // statt gespiegelt wurden.
    using efx::tiles::uvFor;
    constexpr int kSheetW = 1792, kSheetH = 1568, kSize = 224;

    // Wieder zurueckrechnen: welche Zeilen des Blattes werden gelesen?
    const auto rowsRead = [](const efx::tiles::UvRect& uv, bool flipped) {
        const float top = flipped ? (1.0f - uv.v0) : uv.v0;
        const float bottom = flipped ? (1.0f - uv.v1) : uv.v1;
        return std::pair<int, int>{static_cast<int>(top * kSheetH + 0.5f),
                                   static_cast<int>(bottom * kSheetH + 0.5f)};
    };

    {
        const auto uv = uvFor(672, 896, kSize, kSheetW, kSheetH, true);
        const auto rows = rowsRead(uv, true);
        check(rows.first == 896 && rows.second == 1120,
              "gedrehte Textur: gelesen wird der eigene Ausschnitt");
        std::printf("  Platz bei y 896: gelesen y %d..%d\n", rows.first,
                    rows.second);
        // Und ausdruecklich NICHT der Ausschnitt, der vorher kam.
        check(rows.first != 448, "und nicht der Ausschnitt einer anderen Kachel");
    }

    // Ohne Drehung (Direct3D) unveraendert — dort war nie etwas kaputt, und
    // das soll so bleiben.
    {
        const auto uv = uvFor(672, 896, kSize, kSheetW, kSheetH, false);
        const auto rows = rowsRead(uv, false);
        check(rows.first == 896 && rows.second == 1120,
              "ungedrehte Textur: ebenfalls der eigene Ausschnitt");
    }

    // Die waagerechte Richtung wird nie gedreht.
    {
        const auto a = uvFor(672, 896, kSize, kSheetW, kSheetH, true);
        const auto b = uvFor(672, 896, kSize, kSheetW, kSheetH, false);
        check(a.u0 == b.u0 && a.u1 == b.u1, "u haengt nicht an der Drehung");
    }

    // Jede Kachel liest genau ihren eigenen Ausschnitt — keine zwei lesen
    // denselben. Das ist die Regel, deren Bruch man als \"falscher Effekt\"
    // sieht.
    {
        bool allDistinct = true;
        for (int cell = 0; cell < 50; ++cell) {
            const int x = (cell % 8) * kSize;
            const int y = (cell / 8) * kSize;
            const auto uv = uvFor(x, y, kSize, kSheetW, kSheetH, true);
            const auto rows = rowsRead(uv, true);
            if (rows.first != y || rows.second != y + kSize) allDistinct = false;
        }
        check(allDistinct, "alle 50 Plaetze lesen ihren eigenen Ausschnitt");
    }

    // Unsinnige Groessen duerfen nicht in eine Division durch null laufen.
    {
        const auto uv = uvFor(0, 0, 0, 0, 0, true);
        check(std::isfinite(uv.v0) && std::isfinite(uv.v1),
              "leeres Blatt ergibt brauchbare Werte");
    }
}

void testCurveDefaults() {
    std::cout << "== Fehlende Kurvenwerte sind EINS ==\n";

    // Fundstelle: der Erzeuger von CPrimitiveTemplate in FxTemplate.cpp.
    //
    //     mAlphaStart.SetRange( 1.0f, 1.0f );
    //     mAlphaEnd.SetRange( 1.0f, 1.0f );
    //     ... dasselbe fuer size, size2, length, red, green, blue
    //
    // Das ist kein Randfall. Fast jede Raven-Datei enthaelt Bloecke wie
    //
    //     alpha { end 0   parm 80   flags linear nonlinear }
    //
    // ohne `start`. Gemeint ist: von voll auf null ausblenden. Mit null als
    // Voreinstellung waere gemeint: von null auf null — unsichtbar.
    //
    // Genau so ist es passiert: t_small_fire.efx zeigte gar nichts mehr,
    // sobald das Ausblenden richtig nach rgb wanderte. Vorher landete die
    // Null im Alphakanal, den additive Mischung nicht ausliest — zwei Fehler,
    // die sich gegenseitig verdeckt haben.
    // Geschrieben wie in einer echten Datei: Bloecke mit Klammern auf eigenen
    // Zeilen. Der erste Anlauf schrieb `alpha { end 0 }` in eine Zeile — GP2
    // kennt das nicht, und der Zerleger hat es auch brav gemeldet
    // (\"Unbekannter Schluessel alpha\"). Der Test haette sonst die
    // Voreinstellung geprueft statt des gelesenen Werts.
    const char* text =
        "Particle\n"
        "{\n"
        "\tlife\t1000\n"
        "\tcount\t1\n"
        "\tsize\n"
        "\t{\n"
        "\t\tstart\t10\n"
        "\t}\n"
        "\talpha\n"
        "\t{\n"
        "\t\tend\t0\n"
        "\t}\n"
        "\tshaders\n"
        "\t[\n"
        "\t\tgfx/test\n"
        "\t]\n"
        "}\n";

    const auto effect = efx::read(text).effect;
    efx::particles::System system;
    system.play(effect, 1u);
    check(!system.live().empty(), "die Primitive wird ueberhaupt ausgeloest");
    if (system.live().empty()) return;

    const auto& item = system.live().front();
    check(item.alpha.start > 0.9f,
          "alpha ohne start beginnt bei 1.0, nicht bei 0.0");
    check(item.alpha.end < 0.1f, "und endet bei dem, was in der Datei steht");
    std::printf("  alpha { end 0 }  ->  start %.2f, end %.2f\n",
                static_cast<double>(item.alpha.start),
                static_cast<double>(item.alpha.end));

    // Und die Wirkung, die daran haengt: am Anfang muss etwas zu sehen sein.
    const auto list = system.build(item.spawnMs + 1.0f,
                                   efx::camera::Vec3{1, 0, 0},
                                   efx::camera::Vec3{0, 1, 0});
    int brightest = 0;
    for (const auto& group : list.byTexture) {
        for (const auto& v : group.second.vertices) {
            const int r = static_cast<int>(v.colour & 0xFF);
            if (r > brightest) brightest = r;
        }
    }
    check(brightest > 200, "und zu Beginn ist die Primitive hell, nicht schwarz");
    std::printf("  hellster Kanal am Anfang: %d von 255\n", brightest);
}

void testUseAlpha() {
    std::cout << "== useAlpha: wohin das Ausblenden wirkt ==\n";

    // Fundstelle: CParticle::UpdateAlpha in FxPrimitives.cpp.
    //
    //     if ( mFlags & FX_USE_ALPHA ) {
    //         ClampVec( mRefEnt.angles, ... );          // rgb bleibt
    //         mRefEnt.shaderRGBA[3] = perc1 * 0xff;     // alpha traegt
    //     } else {
    //         VectorScale( mRefEnt.angles, perc1, ... ) // rgb traegt
    //     }
    //
    // Ravens eigener Kommentar zum zweiten Zweig: \"works fine for additive
    // blending\" — und genau darum geht es. Additiv wertet die Grafikkarte den
    // Alphakanal nicht aus. Steht das Ausblenden dort, blendet nichts aus.
    const auto build = [](bool useAlpha) {
        efx::Effect effect;
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        if (useAlpha) p.flags |= efx::kFlagUseAlpha;
        p.count = efx::Range::single(1.0f);
        p.life = efx::Range::single(1000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(10.0f);
        // Weiss, das ueber die Lebensdauer auf null ausblendet.
        p.rgb.present = true;
        p.rgb.start.min = efx::Vec3{1.0f, 1.0f, 1.0f};
        p.rgb.start.max = efx::Vec3{1.0f, 1.0f, 1.0f};
        p.rgb.start.set = true;
        p.alpha.present = true;
        p.alpha.start = efx::Range::single(1.0f);
        p.alpha.end = efx::Range::single(0.0f);
        p.alpha.curveFlags = efx::kCurveLinear;
        p.shaders.push_back("gfx/rauch");

        efx::particles::System system;
        system.play(effect, 1u);
        // Bei halber Lebensdauer ist das Ausblenden zur Haelfte fortgeschritten.
        return system.build(500.0f, efx::camera::Vec3{1, 0, 0},
                            efx::camera::Vec3{0, 1, 0});
    };

    const auto readVertex = [](const efx::particles::DrawList& list) {
        struct Rgba { int r, g, b, a; };
        Rgba out{-1, -1, -1, -1};
        for (const auto& group : list.byTexture) {
            if (group.second.vertices.empty()) continue;
            const uint32_t c = group.second.vertices.front().colour;
            out = Rgba{static_cast<int>(c & 0xFF),
                       static_cast<int>((c >> 8) & 0xFF),
                       static_cast<int>((c >> 16) & 0xFF),
                       static_cast<int>((c >> 24) & 0xFF)};
            break;
        }
        return out;
    };

    // Ohne das Flag: rgb traegt das Ausblenden, und der Alphakanal bleibt,
    // was er war — NULL. CEffect() setzt mRefEnt mit memset auf 0, und
    // UpdateAlpha schreibt shaderRGBA[3] nur mit useAlpha. Ein alphagemischter
    // Shader mit alphaGen vertex zeichnet ohne useAlpha also nichts — Ravens
    // Abschnitt "If You Don't See Anything". Hier stand "der Alphakanal bleibt
    // voll" (255).
    {
        const auto list = build(false);
        const auto v = readVertex(list);
        check(v.a == 0, "ohne useAlpha bleibt der Alphakanal bei null (memset)");
        check(v.r < 200 && v.r > 50, "ohne useAlpha wird rgb heruntergezogen");
        std::printf("  ohne useAlpha: rgb %d %d %d, alpha %d\n", v.r, v.g, v.b, v.a);
        check(list.alphaShaders.empty(), "und der Shader gilt nicht als Alphabild");
    }

    // Mit dem Flag: genau umgekehrt.
    {
        const auto list = build(true);
        const auto v = readVertex(list);
        check(v.r >= 250, "mit useAlpha bleibt rgb voll");
        check(v.a < 200 && v.a > 50, "mit useAlpha traegt der Alphakanal");
        std::printf("  mit  useAlpha: rgb %d %d %d, alpha %d\n", v.r, v.g, v.b, v.a);
        check(list.alphaShaders.count("gfx/rauch") == 1,
              "und der Shader ist als Alphabild vermerkt");
    }
}

void testVisualReach() {
    std::cout << "== Sichtweite fuer Vorschaukameras ==\n";

    // Der Fehler, den dieser Test festhaelt: der Browser zaehlte nur BAHNEN.
    // Ein Decal oder ein Muendungsfeuer bewegt sich nicht — die Bahn ist ein
    // Punkt. Die Reichweite kam als 1.0 heraus, die Kamera stand danach
    // anderthalb Einheiten vor einer Flaeche von sieben, und in der Kachel war
    // ein einfarbiges Rechteck zu sehen.
    //
    // Deshalb: ein unbewegtes Segment mit Groesse muss eine Reichweite
    // ergeben, die zu seiner Groesse passt — nicht 1.0.
    {
        efx::Effect still;
        still.primitives.push_back(efx::Primitive{});
        auto& d = still.primitives.back();
        d.type = efx::PrimitiveType::Decal;
        d.count = efx::Range::single(1.0f);
        d.life = efx::Range::single(500.0f);
        d.size.present = true;
        d.size.start = efx::Range::single(7.0f);
        d.shaders.push_back("gfx/damage/burnmark4");

        efx::particles::System system;
        system.play(still, 1u);
        const float reach = efx::particles::visualReach(system);
        check(reach > 1.0f, "unbewegtes Segment ergibt nicht die Notreichweite 1.0");
        check(reach >= 3.0f, "die halbe Ausdehnung zaehlt mit (Groesse 7 -> >= 3.5)");
        std::printf("  unbewegtes Decal Groesse 7: Reichweite %.2f\n",
                    static_cast<double>(reach));
    }

    // Umgekehrt: was fliegt, muss weiter reichen als was steht — sonst zaehlt
    // die Bahn nicht mehr mit, und der Fehler waere nur umgedreht.
    {
        efx::Effect flying;
        flying.primitives.push_back(efx::Primitive{});
        auto& p = flying.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(8.0f);
        p.life = efx::Range::single(1000.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(2.0f);
        p.velocity.set = true;
        p.velocity.min = {200.0f, 200.0f, 200.0f};
        p.velocity.max = {200.0f, 200.0f, 200.0f};
        p.shaders.push_back("gfx/funke");

        efx::particles::System system;
        system.play(flying, 1u);
        const float reach = efx::particles::visualReach(system);
        // 200 Einheiten je Sekunde, eine Sekunde Leben -> rund 200 weit.
        check(reach > 100.0f, "eine Bahn ueber 200 Einheiten zaehlt weiterhin");
        std::printf("  fliegende Funken: Reichweite %.2f\n",
                    static_cast<double>(reach));
    }

    // Ein leeres System darf keine unbrauchbare Zahl liefern — sonst rechnet
    // distanceToFit mit null und die Kamera steht im Ursprung.
    {
        efx::particles::System empty;
        const float reach = efx::particles::visualReach(empty);
        check(reach > 0.0f && std::isfinite(reach),
              "leeres System ergibt eine brauchbare Reichweite");
    }
}

void testParallelBuild() {
    std::cout << "== Kacheln parallel aufbauen ==\n";

    // Der Effektbrowser baut die Geometrie vieler Kacheln gleichzeitig auf.
    // Ob das schneller ist, haengt am Rechner — hier laeuft nur ein Kern, also
    // laesst sich der Gewinn NICHT messen. Was sich messen laesst, ist das
    // Wichtigere: dass verteilt genau dasselbe herauskommt wie der Reihe nach.
    //
    // Genau das ist die Gefahr beim Verteilen: eine Kachel doppelt, eine gar
    // nicht, oder zwei Faeden im selben System.
    efx::Effect effect;
    for (int k = 0; k < 3; ++k) {
        effect.primitives.push_back(efx::Primitive{});
        auto& p = effect.primitives.back();
        p.type = efx::PrimitiveType::Particle;
        p.count = efx::Range::single(40.0f);
        p.life = efx::Range::single(900.0f);
        p.size.present = true;
        p.size.start = efx::Range::single(5.0f);
        p.velocity.set = true;
        p.velocity.min = {-150.0f, -150.0f, -150.0f};
        p.velocity.max = {150.0f, 150.0f, 150.0f};
        p.shaders.push_back("gfx/kachel");
    }

    constexpr size_t kTiles = 24;
    auto makeSystems = [&] {
        std::vector<efx::particles::System> systems(kTiles);
        for (size_t i = 0; i < kTiles; ++i) {
            systems[i].play(effect, static_cast<unsigned>(i) | 1u);
        }
        return systems;
    };

    const efx::camera::Vec3 right{1.0f, 0.0f, 0.0f};
    const efx::camera::Vec3 up{0.0f, 0.0f, 1.0f};

    auto serial = makeSystems();
    std::vector<efx::particles::DrawList> oneByOne(kTiles);
    for (size_t i = 0; i < kTiles; ++i) {
        oneByOne[i] = serial[i].build(450.0f, right, up);
    }

    auto shared = makeSystems();
    std::vector<efx::particles::DrawList> distributed(kTiles);
    std::atomic<int> touched{0};
    efx::jobs::pool().parallelFor(kTiles, 1u, [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            distributed[i] = shared[i].build(450.0f, right, up);
            touched.fetch_add(1, std::memory_order_relaxed);
        }
    });

    check(touched.load() == static_cast<int>(kTiles),
          "jede Kachel genau einmal bearbeitet");

    size_t vertices = 0;
    bool identical = true;
    for (size_t i = 0; i < kTiles; ++i) {
        if (oneByOne[i].byTexture.size() != distributed[i].byTexture.size()) {
            identical = false;
            continue;
        }
        for (const auto& group : oneByOne[i].byTexture) {
            const auto found = distributed[i].byTexture.find(group.first);
            if (found == distributed[i].byTexture.end()) {
                identical = false;
                continue;
            }
            if (found->second.vertices.size() != group.second.vertices.size()) {
                identical = false;
                continue;
            }
            vertices += group.second.vertices.size();
            for (size_t v = 0; v < group.second.vertices.size(); ++v) {
                const auto& a = group.second.vertices[v];
                const auto& b = found->second.vertices[v];
                for (int c = 0; c < 3; ++c) {
                    if (std::fabs(a.pos[c] - b.pos[c]) > 0.001f) identical = false;
                }
                if (a.colour != b.colour) identical = false;
            }
        }
    }
    check(identical, "verteilt ergibt Punkt fuer Punkt dasselbe wie seriell");
    check(vertices > 0, "und es kam ueberhaupt Geometrie heraus");
    std::printf("  %zu Kacheln, %zu Eckpunkte verglichen, %u Arbeitsfaeden\n",
                kTiles, vertices, efx::jobs::pool().threadCount());

    // Dasselbe mit ERZWUNGENEN Faeden.
    //
    // Der Pool nimmt so viele Faeden wie Kerne. Auf einem Rechner mit einem
    // Kern sind das null, und dann laeuft alles der Reihe nach — der Test
    // wuerde also gar nichts ueber das Verteilen aussagen.
    //
    // Mit acht erzwungenen Faeden laufen die Aufgaben wirklich nebeneinander,
    // auch wenn sie sich einen Kern teilen. Genau darum geht es: ob zwei
    // Faeden im selben System einander ins Gehege kommen.
    {
        efx::jobs::Pool forced(8);
        auto systems = makeSystems();
        std::vector<efx::particles::DrawList> out(kTiles);
        std::atomic<int> count{0};
        forced.parallelFor(kTiles, 1u, [&](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                out[i] = systems[i].build(450.0f, right, up);
                count.fetch_add(1, std::memory_order_relaxed);
            }
        });
        check(count.load() == static_cast<int>(kTiles),
              "mit acht Faeden ebenfalls jede genau einmal");

        bool same = true;
        for (size_t i = 0; i < kTiles; ++i) {
            if (out[i].byTexture.size() != oneByOne[i].byTexture.size()) same = false;
            for (const auto& group : oneByOne[i].byTexture) {
                const auto found = out[i].byTexture.find(group.first);
                if (found == out[i].byTexture.end() ||
                    found->second.vertices.size() != group.second.vertices.size()) {
                    same = false;
                }
            }
        }
        check(same, "und dasselbe Ergebnis");
        std::printf("  mit 8 erzwungenen Faeden: %d Kacheln, Ergebnis gleich\n",
                    count.load());
    }

    // Leere Liste haengt sich nicht auf.
    efx::jobs::pool().parallelFor(0, 1u, [](size_t, size_t) {});
    check(true, "null Kacheln sind kein Sonderfall");
}

void testValidator() {
    std::cout << "== Pruefung ==\n";
    // Ein CameraShake ohne bounce ist genau der Fall, den man im alten Editor
    // nicht sieht und erst im Spiel bemerkt.
    efx::ReadResult r = efx::read(
        "CameraShake\n{\n\tname\tShake\n\tlife\t500\n\tradius\t300\n}\n");
    auto diags = efx::validate(r.effect);
    bool foundBounce = false;
    for (const auto& d : diags) {
        if (d.message.find("bounce") != std::string::npos) foundBounce = true;
    }
    check(foundBounce, "fehlendes bounce am CameraShake wird gemeldet");

    efx::ReadResult clamped = efx::read(
        "CameraShake\n{\n\tbounce\t40\n\tradius\t300\n\tlife\t500\n}\n");
    bool foundLimit = false;
    for (const auto& d : efx::validate(clamped.effect)) {
        if (d.message.find("16") != std::string::npos) foundLimit = true;
    }
    check(foundLimit, "bounce ueber 16 wird als wirkungslos gemeldet");
}

// ===========================================================================
// Befunde aus dem Massentest ueber alle 634 ausgelieferten .efx-Dateien
// (Movie Duels und Jedi Outcast). Jede Pruefung nennt die Fundstelle in der
// Engine (OpenJK: SP code/, MP codemp/); jede wurde gegengeprueft, indem die
// Korrektur voruebergehend zurueckgenommen wurde — dann wird sie rot.
// ===========================================================================

// Ein unkomprimiertes Zip von Hand, fuer Pruefungen mit mehreren Archiven.
void writeStoredZip(const std::filesystem::path& path,
                    const std::vector<std::pair<std::string, std::string>>& entries) {
    auto put16 = [](std::string& out, std::uint16_t v) {
        out += static_cast<char>(v & 0xFF);
        out += static_cast<char>((v >> 8) & 0xFF);
    };
    auto put32 = [](std::string& out, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) out += static_cast<char>((v >> (i * 8)) & 0xFF);
    };
    std::string local, directory;
    for (const auto& [name, content] : entries) {
        const auto offset = static_cast<std::uint32_t>(local.size());
        const auto size = static_cast<std::uint32_t>(content.size());
        const auto nameSize = static_cast<std::uint16_t>(name.size());
        local += "PK";
        local += static_cast<char>(3);
        local += static_cast<char>(4);
        put16(local, 20); put16(local, 0); put16(local, 0); put16(local, 0); put16(local, 0);
        put32(local, 0); put32(local, size); put32(local, size);
        put16(local, nameSize); put16(local, 0);
        local += name;
        local += content;

        directory += "PK";
        directory += static_cast<char>(1);
        directory += static_cast<char>(2);
        put16(directory, 20); put16(directory, 20); put16(directory, 0); put16(directory, 0);
        put16(directory, 0); put16(directory, 0);
        put32(directory, 0); put32(directory, size); put32(directory, size);
        put16(directory, nameSize); put16(directory, 0); put16(directory, 0);
        put16(directory, 0); put16(directory, 0);
        put32(directory, 0); put32(directory, offset);
        directory += name;
    }
    const auto count = static_cast<std::uint16_t>(entries.size());
    std::string end = "PK";
    end += static_cast<char>(5);
    end += static_cast<char>(6);
    put16(end, 0); put16(end, 0); put16(end, count); put16(end, count);
    put32(end, static_cast<std::uint32_t>(directory.size()));
    put32(end, static_cast<std::uint32_t>(local.size()));
    put16(end, 0);
    std::ofstream file(path, std::ios::binary);
    file << local << directory << end;
}

// Ein Lader fuer untergeordnete Effekte aus einer kleinen Tabelle.
struct ChildLibrary {
    std::vector<std::pair<std::string, efx::Effect>> effects;
    void add(const std::string& name, const char* text) {
        effects.emplace_back(name, efx::read(text).effect);
    }
    efx::particles::EffectLoader loader() {
        return [this](const std::string& name) -> const efx::Effect* {
            for (const auto& entry : effects) {
                if (entry.first == name) return &entry.second;
            }
            return nullptr;
        };
    }
};

// --- 1. Emitter senden `emitfx` aus, nicht `playfx` --------------------------
void testEmitterEmitsEmitFx() {
    std::cout << "== Emitter: emitfx, density 10, variance 1 ==\n";
    // FxScheduler.cpp (SP), Fall Emitter: FX_AddEmitter( ...,
    // fx->mEmitterFxHandles.GetHandle(), fx->mDensity.GetVal(),
    // fx->mVariance.GetVal(), ... ). CEmitter::UpdateEmitter sendet nur bei
    // `mFlags & FX_EMIT_FX` — das Bit setzt ParseEmitterFxStrings selbst.
    // `playfx` liest ausschliesslich der FxRunner.
    ChildLibrary library;
    library.add("kind", "Particle\n{\n\tlife\t100\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n");

    // So schreibt Raven es (47 Emitter in 36 Dateien, keiner mit playfx).
    const auto raven = efx::read(
        "Emitter\n{\n\tlife\t1000\n\tvelocity\t0 0 200\n\tdensity\t10\n"
        "\tvariance\t1\n\tflags\temitFx\n\temitfx\n\t[\n\t\tkind\n\t]\n}\n");
    efx::particles::System emitting;
    emitting.play(raven.effect, 1, {}, {}, library.loader());
    check(emitting.startedEffects() >= 15 && emitting.startedEffects() <= 25,
          "200 Einheiten Weg bei density 10: rund zwanzig Aussendungen der emitfx-Liste");
    std::printf("  Emitter mit emitfx: %d Aussendungen\n", emitting.startedEffects());

    // Ohne das Flag in der Datei: die Liste setzt es (ParseEmitterFxStrings).
    const auto noFlag = efx::read(
        "Emitter\n{\n\tlife\t1000\n\tvelocity\t0 0 200\n\temitfx\n\t[\n\t\tkind\n\t]\n}\n");
    efx::particles::System implied;
    implied.play(noFlag.effect, 1, {}, {}, library.loader());
    check(implied.startedEffects() >= 15,
          "ohne flags emitFx und ohne density sendet er trotzdem aus (Voreinstellung 10)");

    // playfx an einem Emitter wertet die Engine nicht aus.
    const auto playOnly = efx::read(
        "Emitter\n{\n\tlife\t1000\n\tvelocity\t0 0 200\n\tdensity\t10\n"
        "\tplayfx\n\t[\n\t\tkind\n\t]\n}\n");
    efx::particles::System silent;
    silent.play(playOnly.effect, 1, {}, {}, library.loader());
    check(silent.startedEffects() == 0, "playfx an einem Emitter sendet nichts aus");

    // Die Voreinstellungen selbst, aus dem Erzeuger von CPrimitiveTemplate.
    check(efx::kDefaultDensity == 10.0f && efx::kDefaultVariance == 1.0f,
          "density 10 und variance 1 wie mDensity/mVariance im Erzeuger");
}

// --- 2. Kurvenwoerter ueberstehen das Speichern ------------------------------
void testCurveWordsSurviveSaving() {
    std::cout << "== Kurvenwoerter beim Speichern ==\n";
    // FX_CLAMP ist 0x0C = FX_NONLINEAR|FX_WAVE (FxPrimitives.h); random (0x02)
    // ist ein eigenes Bit. Raven schreibt "linear clamp" 118-mal und "random
    // linear clamp" dreimal (scepter/beam_warmup.efx).
    const auto readBack = [](const char* flags) {
        std::string text = "Particle\n{\n\tlife\t1000\n\talpha\n\t{\n\t\tstart\t0\n"
                           "\t\tend\t1\n\t\tparm\t30\n\t\tflags\t";
        text += flags;
        text += "\n\t}\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n";
        return efx::read(text).effect;
    };

    {
        const efx::Effect original = readBack("random linear clamp");
        const std::string written = efx::write(original);
        const efx::Effect back = efx::read(written).effect;
        const auto& a = original.primitives[0].alpha;
        const auto& b = back.primitives[0].alpha;
        check(a.curveFlags == (efx::kCurveRandom | efx::kCurveLinear | efx::kCurveClamp),
              "random linear clamp sind die Bits 0xF");
        check(b.curveFlags == a.curveFlags, "random bleibt nach dem Speichern erhalten");
        check(b.curveWords == a.curveWords, "die Woerter kommen unveraendert zurueck");
        check(written.find("random linear clamp") != std::string::npos &&
                  written.find("wave") == std::string::npos,
              "geschrieben steht \"random linear clamp\", kein wave");
    }

    // "linear clamp" darf nach dem Speichern keinen Pruefungsfehler bekommen,
    // den es vorher nicht hatte (126 Raven-Dateien waren betroffen).
    {
        const efx::Effect original = readBack("linear clamp");
        const efx::Effect back = efx::read(efx::write(original)).effect;
        auto errors = [](const efx::Effect& e) {
            int n = 0;
            for (const auto& d : efx::validate(e)) n += d.severity == efx::Severity::Error;
            return n;
        };
        check(errors(original) == 0 && errors(back) == 0,
              "linear clamp: vor und nach dem Speichern ohne Fehler");
    }

    // Geaendert in der Oberflaeche: die Woerter sind dann leer (editCurveFlags
    // leert sie), und der Schreiber formuliert neu — ohne Kollision.
    {
        efx::Effect edited = readBack("linear");
        auto& alpha = edited.primitives[0].alpha;
        alpha.curveFlags = efx::kCurveRandom | efx::kCurveLinear | efx::kCurveClamp;
        alpha.curveWords.clear();
        const std::string written = efx::write(edited);
        check(written.find("random linear clamp") != std::string::npos,
              "neu formuliert in Ravens Reihenfolge: random linear clamp");
        const efx::Effect back = efx::read(written).effect;
        check(back.primitives[0].alpha.curveFlags == alpha.curveFlags,
              "und die Bits laufen zurueck");
        bool collision = false;
        for (const auto& d : efx::validate(back)) {
            if (d.id == efx::i18n::Str::VCurveCollision) collision = true;
        }
        check(!collision, "ohne Kollisionsmeldung");

        alpha.curveFlags = efx::kCurveLinear | efx::kCurveNonLinear;
        alpha.curveWords.clear();
        check(efx::write(edited).find("linear nonlinear") != std::string::npos,
              "linear|nonlinear wird \"linear nonlinear\"");
        // Veraltete Woerter (passen nicht mehr zu den Bits) werden nicht
        // geschrieben, sondern neu formuliert.
        alpha.curveFlags = efx::kCurveWave;
        alpha.curveWords = {"linear"};
        const std::string waveText = efx::write(edited);
        check(waveText.find("\twave") != std::string::npos &&
                  waveText.find("linear") == std::string::npos,
              "Woerter, die nicht mehr zu den Bits passen, werden ersetzt");
    }
}

// --- 3. Unbekannte Bloecke (ForceFeedback) ----------------------------------
void testForeignGroupsKept() {
    std::cout << "== ForceFeedback und andere fremde Bloecke ==\n";
    // CFxScheduler::ParseEffect (FxScheduler.cpp) sucht den Gruppennamen in der
    // Tabelle der Primitivtypen und uebergeht alles andere stumm. 51
    // ausgelieferte Dateien haben einen forcefeedback-Block, z.B.
    // blaster/muzzle_flash.efx — genau in dieser Lage, mitten drin:
    const char* text =
        "Sound\r\n{\r\n\tsounds\r\n\t[\r\n\t\tsound/weapons/blaster/fire.wav\r\n\t]\r\n}\r\n"
        "\r\n"
        "forcefeedback\r\n{\r\n\tforces\r\n\t[\r\n\t\tfffx/weapons/blaster/fire\r\n\t]\r\n}\r\n"
        "\r\n"
        "CameraShake\r\n{\r\n\tlife\t\t\t\t50\r\n\tintensity\t\t\t0.3 0.4\r\n\tradius\t60\r\n}";
    const efx::ReadResult result = efx::read(text);
    check(!result.hasErrors(), "forcefeedback ist kein Lesefehler");
    bool onlyInfo = true;
    for (const auto& d : result.diagnostics) {
        if (d.severity != efx::Severity::Info) onlyInfo = false;
    }
    check(onlyInfo && !result.diagnostics.empty(), "sondern ein Hinweis");
    check(result.effect.primitives.size() == 2, "beide Primitive gelesen");
    check(result.effect.foreignGroups.size() == 1 &&
              result.effect.foreignGroups[0].beforePrimitive == 1 &&
              result.effect.foreignGroups[0].text.find('\r') == std::string::npos &&
              result.effect.foreignGroups[0].text.rfind("forcefeedback", 0) == 0 &&
              result.effect.foreignGroups[0].text.back() == '}',
          "der Block wird wortwoertlich mit seiner Lage gemerkt");

    const std::string written = efx::write(result.effect);
    const size_t sound = written.find("Sound");
    const size_t feedback = written.find("forcefeedback\r\n{\r\n\tforces\r\n\t[\r\n"
                                         "\t\tfffx/weapons/blaster/fire\r\n\t]\r\n}");
    const size_t shake = written.find("CameraShake");
    check(feedback != std::string::npos, "der Block steht nach dem Speichern noch da");
    check(sound < feedback && feedback < shake, "und an derselben Stelle");
    std::string why;
    check(same(result.effect, efx::read(written).effect, why),
          "verlustfreier Umlauf mit fremdem Block (" + why + ")");

    // Wird die Primitive davor geloescht, geht der Block nicht verloren.
    efx::Effect fewer = result.effect;
    fewer.primitives.clear();
    check(efx::write(fewer).find("fffx/weapons/blaster/fire") != std::string::npos,
          "auch ohne Primitive bleibt der Block erhalten");

    // Ein anderer unbekannter Name ist wahrscheinlich ein Tippfehler: Warnung,
    // aber kein Fehler, und ebenfalls erhalten.
    const efx::ReadResult typo = efx::read("Partcle\n{\n\tlife\t100\n}\n");
    bool warned = false;
    for (const auto& d : typo.diagnostics) warned |= d.severity == efx::Severity::Warning;
    check(!typo.hasErrors() && warned, "ein Tippfehler im Typ ist eine Warnung");
    check(efx::write(typo.effect).find("Partcle") != std::string::npos,
          "und bleibt beim Speichern stehen");
}

// --- 4. Fehlendes `end` ist 1.0 ---------------------------------------------
void testMissingEndIsOne() {
    std::cout << "== Fehlendes end ist 1.0 ==\n";
    // Erzeuger von CPrimitiveTemplate (FxTemplate.cpp): mSizeEnd, mSize2End,
    // mLengthEnd, mAlphaEnd, mRedEnd/mGreenEnd/mBlueEnd stehen auf 1.0;
    // FxScheduler.cpp reicht `mSizeEnd.GetVal()` unveraendert weiter.
    const auto effect = efx::read(
        "Particle\n{\n\tlife\t1000\n\tsize\n\t{\n\t\tstart\t10\n\t\tflags\tlinear\n\t}\n"
        "\trgb\n\t{\n\t\tstart\t1 0 0\n\t\tflags\tlinear\n\t}\n"
        "\talpha\n\t{\n\t\tstart\t0.2\n\t\tflags\tlinear\n\t}\n"
        "\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n").effect;
    efx::particles::System system;
    system.play(effect, 1u);
    check(system.live().size() == 1, "eine Primitive");
    if (system.live().empty()) return;
    const auto& item = system.live()[0];
    check(item.size.end == 1.0f, "size ohne end endet bei 1, nicht bei start");
    check(item.alpha.end == 1.0f, "alpha ohne end endet bei 1");
    check(item.rgb[0].end == 1.0f && item.rgb[1].end == 1.0f && item.rgb[2].end == 1.0f,
          "rgb ohne end endet bei Weiss, nicht bei der Startfarbe");
    const float half = efx::curve::evaluate(item.size, item.spawnMs + 500.0f, item.spawnMs,
                                            item.deathMs, item.sizeParm, 1.0f);
    check(std::fabs(half - 5.5f) < 0.01f,
          "size { start 10 flags linear } ist bei halber Zeit 5.5 wie im Spiel");
    std::printf("  size bei 50 %%: %.2f (Engine 5.5)\n", static_cast<double>(half));
}

// --- 5. Kindeffekte und Wiederholungen: die Uhr der Kurven ------------------
void testChildCurvesUseTheirOwnClock() {
    std::cout << "== Kurvenparameter von Kindeffekten ==\n";
    // CParticle::Init (FxPrimitives.cpp) bzw. FxScheduler: bei nonlinear und
    // clamp ist der Parameter ein ABSOLUTER Zeitpunkt,
    //     mAlphaParm = alphaParm * 0.01f * killTime + theFxHelper.mTime
    // also ab der Entstehung DIESES Teilchens.
    const char* fading =
        "Particle\n{\n\tlife\t1000\n\talpha\n\t{\n\t\tstart\t1\n\t\tend\t0\n"
        "\t\tparm\t50\n\t\tflags\tnonlinear\n\t}\n\tsize\n\t{\n\t\tstart\t10\n\t}\n"
        "\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n";
    ChildLibrary library;
    library.add("kind", fading);
    const efx::Effect root = efx::read(fading).effect;
    const efx::Effect runner =
        efx::read("FxRunner\n{\n\tdelay\t2000\n\tplayfx\n\t[\n\t\tkind\n\t]\n}\n").effect;

    efx::particles::System direct, delayed;
    direct.play(root, 1, {}, {}, library.loader());
    delayed.play(runner, 1, {}, {}, library.loader());
    check(!direct.live().empty() && !delayed.live().empty(), "beide spielen");
    if (direct.live().empty() || delayed.live().empty()) return;
    const auto& child = delayed.live()[0];
    check(std::fabs(child.spawnMs - 2000.0f) < 0.01f, "das Kind startet bei 2000 ms");
    check(std::fabs(child.alphaParm - (child.spawnMs + 500.0f)) < 0.5f,
          "sein Ausblendpunkt liegt 500 ms nach SEINEM Start, nicht nach dem des Elternteils");

    auto colourAt = [](const efx::particles::System& s, float t) {
        const auto list = s.build(t, efx::camera::Vec3{1, 0, 0}, efx::camera::Vec3{0, 0, 1});
        for (const auto& group : list.byTexture) {
            if (!group.second.vertices.empty()) {
                return static_cast<int>(group.second.vertices[0].colour & 0xFF);
            }
        }
        return -1;
    };
    const int atRoot = colourAt(direct, 250.0f);
    const int atChild = colourAt(delayed, 2250.0f);
    check(atRoot == atChild && atRoot > 250,
          "250 ms nach dem Start sieht das Kind aus wie derselbe Effekt allein: voll hell");
    std::printf("  250 ms nach dem Start: allein %d, als Kind %d\n", atRoot, atChild);

    // Dieselbe Regel fuer jede Wiederholungsgeneration (repeatDelay).
    const efx::Effect repeating =
        efx::read((std::string("repeatDelay\t300\n") + fading).c_str()).effect;
    efx::particles::System generations;
    generations.play(repeating, 1, {}, {}, {}, {}, true);
    int total = 0, wrong = 0;
    for (const auto& item : generations.live()) {
        ++total;
        const float expected = efx::curve::resolveParm(
            item.alpha, item.spawnMs, item.deathMs - item.spawnMs);
        if (std::fabs(expected - item.alphaParm) > 0.5f) ++wrong;
    }
    check(total > 1 && wrong == 0,
          "jede Wiederholungsgeneration blendet ab ihrem eigenen Start aus");
    std::printf("  Wiederholung: %d Teilchen, %d mit verschobenem Ausblendpunkt\n", total,
                wrong);
}

// --- 6. Die Listen setzen ihre Flags selbst ----------------------------------
void testListsSetTheirOwnFlags() {
    std::cout << "== deathfx/impactfx/emitfx/models setzen ihre Flags ==\n";
    // FxTemplate.cpp, SP ParseFX bzw. MP am Ende jeder Funktion:
    //   ParseImpactFxStrings   mFlags |= FX_IMPACT_RUNS_FX | FX_APPLY_PHYSICS
    //   ParseDeathFxStrings    mFlags |= FX_DEATH_RUNS_FX
    //   ParseEmitterFxStrings  mFlags |= FX_EMIT_FX
    //   ParseModels            mFlags |= FX_ATTACHED_MODEL
    efx::Primitive p;
    check(efx::effectiveFlags(p) == 0, "ohne Listen keine zusaetzlichen Bits");
    p.deathFx = {"a"};
    p.impactFx = {"b"};
    p.emitFx = {"c"};
    p.models = {"d.md3"};
    const uint32_t flags = efx::effectiveFlags(p);
    check((flags & efx::kFlagDeathRunsFx) && (flags & efx::kFlagImpactRunsFx) &&
              (flags & efx::kFlagApplyPhysics) && (flags & efx::kFlagEmitFx) &&
              (flags & efx::kFlagAttachedModel),
          "jede Liste setzt ihr Bit wie im Parser der Engine");

    // Und die Vorschau richtet sich danach: deathfx ohne Flag startet.
    ChildLibrary library;
    library.add("boom", "Particle\n{\n\tlife\t100\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n");
    const auto dying = efx::read(
        "Particle\n{\n\tlife\t200\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n"
        "\tdeathfx\n\t[\n\t\tboom\n\t]\n}\n");
    efx::particles::System system;
    system.play(dying.effect, 1, {}, {}, library.loader());
    check(system.startedEffects() == 1, "deathfx ohne flags deathFx startet beim Tod");
    bool falseAlarm = false;
    for (const auto& d : efx::validate(dying.effect)) {
        if (d.id == efx::i18n::Str::VDeathFxNoFlag || d.id == efx::i18n::Str::VImpactFxNoFlag ||
            d.id == efx::i18n::Str::VImpactFxNoPhysics) {
            falseAlarm = true;
        }
    }
    check(!falseAlarm, "und die Pruefung meldet nichts dazu");

    // impactfx ohne jedes Flag: Physik und Aufpralleffekt (huge_lightning.efx,
    // tripmine/laserMP.efx haben es so).
    const auto falling = efx::read(
        "Particle\n{\n\tlife\t3000\n\torigin\t100 0 0\n\tvelocity\t-400 0 0\n"
        "\tshaders\n\t[\n\t\tgfx/x\n\t]\n"
        "\timpactfx\n\t[\n\t\tboom\n\t]\n}\n");
    efx::particles::System hitting;
    hitting.play(falling.effect, 1, {}, {}, library.loader(),
                 efx::sim::roomPlanes(256.0f, 384.0f, 320.0f));
    // Das Elternteil steht hinter seinen Kindern in live() — deshalb suchen.
    bool anyPath = false;
    for (const auto& item : hitting.live()) anyPath |= item.hasPath;
    check(anyPath, "impactfx allein schaltet die Physik ein");
    check(hitting.startedEffects() >= 1, "und startet beim Aufprall seinen Effekt");
}

// --- 7. orgOnSphere/orgOnCylinder ohne radius/height -------------------------
void testSphereAndCylinderDefaults() {
    std::cout << "== orgOnSphere/orgOnCylinder ohne radius ==\n";
    // Erzeuger von CPrimitiveTemplate: mRadius und mHeight stehen auf 10.
    // FxScheduler.cpp liest im Zweig FX_ORG_ON_SPHERE/CYLINDER
    // `fx->mRadius.GetVal()` und `fx->mHeight.GetVal()` ohne Pruefung.
    auto spread = [](const char* spawnFlags, float& minDistance, float& maxDistance) {
        std::string text = "Particle\n{\n\tcount\t40\n\tlife\t1000\n\tspawnFlags\t";
        text += spawnFlags;
        text += "\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n";
        efx::particles::System system;
        system.play(efx::read(text).effect, 3u);
        minDistance = 1e9f;
        maxDistance = 0.0f;
        for (const auto& item : system.live()) {
            const float d = efx::camera::length(item.origin);
            minDistance = std::min(minDistance, d);
            maxDistance = std::max(maxDistance, d);
        }
    };
    float lo = 0.0f, hi = 0.0f;
    spread("orgOnSphere", lo, hi);
    check(lo > 9.9f && hi < 10.1f, "orgOnSphere ohne radius: alle Punkte 10 vom Ursprung");
    std::printf("  orgOnSphere: Abstand %.2f bis %.2f (Engine 10)\n",
                static_cast<double>(lo), static_cast<double>(hi));
    spread("orgOnCylinder", lo, hi);
    // Mantel mit Radius 10, Hoehe 10: Abstand zwischen 10 und sqrt(10^2+5^2).
    check(lo > 9.9f && hi < 11.2f, "orgOnCylinder ohne radius/height: auf dem Mantel, Radius 10");
}

// --- 8. Suchreihenfolge der Archive -----------------------------------------
void testPk3SearchOrder() {
    std::cout << "== Suchreihenfolge der .pk3 ==\n";
    // FS_AddGameDirectory (code/qcommon/files.cpp): erst der Ordner, dann jedes
    // Archiv (paksort = FS_PathCmp) VOR die bisherigen gestellt. Ergebnis:
    // letztes Archiv zuerst, ausgepackte Dateien zuletzt (fs_dirbeforepak 0).
    namespace fs = std::filesystem;
    using namespace efx::assets;
    const fs::path base = fs::temp_directory_path() / "efxed_pk3order";
    fs::remove_all(base);
    fs::create_directories(base / "effects" / "blaster");
    fs::create_directories(base / "gfx" / "fx");
    fs::create_directories(base / "sound" / "fx");

    writeStoredZip(base / "assets0.pk3",
                   {{"effects/blaster/muzzle_flash.efx", "ALT"},
                    {"gfx/fx/glow.tga", "ALT"},
                    {"sound/fx/hit.wav", "ALT"},
                    {"shaders/fx.shader", "gfx/fx/s\n{\n\t{\n\t\tmap gfx/fx/alt.tga\n\t}\n}\n"}});
    writeStoredZip(base / "assets2.pk3",
                   {{"effects/blaster/muzzle_flash.efx", "NEU"},
                    {"gfx/fx/glow.tga", "NEU"},
                    {"sound/fx/hit.wav", "NEU"},
                    {"shaders/fx.shader", "gfx/fx/s\n{\n\t{\n\t\tmap gfx/fx/neu.tga\n\t}\n}\n"}});
    // FS_PathCmp vergleicht in GROSSbuchstaben: `_` liegt hinter `Z`, also
    // kommt assets_x NACH assetsz und wird zuerst durchsucht.
    writeStoredZip(base / "assetsz.pk3", {{"gfx/fx/under.tga", "Z"}});
    writeStoredZip(base / "assets_x.pk3", {{"gfx/fx/under.tga", "UNTERSTRICH"}});
    { std::ofstream(base / "effects" / "blaster" / "muzzle_flash.efx") << "LOSE"; }
    { std::ofstream(base / "gfx" / "fx" / "glow.tga") << "LOSE"; }
    { std::ofstream(base / "sound" / "fx" / "hit.wav") << "LOSE"; }

    auto content = [&](const ResolvedTexture& where) {
        const auto bytes = readFile(base.string(), where);
        return std::string(bytes.begin(), bytes.end());
    };
    for (int parallel = 0; parallel < 2; ++parallel) {
        efx::jobs::Pool pool;
        const Index index = scan(base.string(), parallel ? &pool : nullptr);
        const std::string label = parallel ? " (verteilt gelesen)" : "";
        check(index.archives.size() == 4 &&
                  fs::path(index.archives[0]).filename().string() == "assets_x.pk3" &&
                  fs::path(index.archives[1]).filename().string() == "assetsz.pk3" &&
                  fs::path(index.archives[3]).filename().string() == "assets0.pk3",
              "Archive in Suchreihenfolge: assets_x, assetsz, assets2, assets0" + label);
        check(content(findEffect(index, base.string(), "blaster/muzzle_flash")) == "NEU",
              "Effekt: das spaetere Archiv gewinnt, auch gegen die ausgepackte Datei" + label);
        check(content(findTexture(index, base.string(), "gfx/fx/glow")) == "NEU",
              "Bild: ebenso" + label);
        check(content(findSound(index, base.string(), "sound/fx/hit.wav")) == "NEU",
              "Klang: ebenso" + label);
        check(content(findTexture(index, base.string(), "gfx/fx/under")) == "UNTERSTRICH",
              "paksort vergleicht in Grossbuchstaben" + label);
        check(index.mapOf("gfx/fx/s") == "gfx/fx/neu.tga",
              "Shaderdatei gleichen Namens: die aus dem spaeteren Archiv" + label);
        check(fs::path(index.sourceOf("blaster/muzzle_flash")).filename().string() ==
                  "assets2.pk3",
              "der Browser nennt dieselbe Quelle" + label);
    }
    fs::remove_all(base);

    // Zwei .shader-Dateien mit demselben Shader: ScanAndLoadShaderFiles haengt
    // die Dateien RUECKWAERTS aneinander, FindShaderInShaderText nimmt den
    // ersten Treffer — die alphabetisch letzte Datei gewinnt.
    const fs::path shaders = fs::temp_directory_path() / "efxed_shaderorder";
    fs::remove_all(shaders);
    fs::create_directories(shaders / "shaders");
    { std::ofstream(shaders / "shaders" / "a.shader")
          << "gfx/doppelt\n{\n\t{\n\t\tmap gfx/aus_a.tga\n\t}\n}\n"; }
    { std::ofstream(shaders / "shaders" / "b.shader")
          << "gfx/doppelt\n{\n\t{\n\t\tmap gfx/aus_b.tga\n\t}\n}\n"; }
    const Index twice = scan(shaders.string());
    check(twice.mapOf("gfx/doppelt") == "gfx/aus_b.tga",
          "doppelter Shader: die alphabetisch letzte .shader-Datei gewinnt");
    fs::remove_all(shaders);
}

// --- 9. Shadername mit Endung -----------------------------------------------
void testShaderNameWithExtension() {
    std::cout << "== Shadername mit Endung ==\n";
    // R_FindShader (tr_shader.cpp): COM_StripExtension( name, strippedName )
    // vor FindShaderInShaderText. cinematics/takeoff.efx schreibt
    // `gfx/effects/wcloud.tga` und meint den Shader gfx/effects/wcloud.
    namespace fs = std::filesystem;
    using namespace efx::assets;
    const fs::path base = fs::temp_directory_path() / "efxed_shaderext";
    fs::remove_all(base);
    fs::create_directories(base / "shaders");
    fs::create_directories(base / "gfx" / "misc");
    { std::ofstream(base / "shaders" / "fx.shader")
          << "gfx/effects/wcloud\n{\n\t{\n\t\tmap gfx/misc/cloud.tga\n"
             "\t\tblendFunc GL_ONE GL_ONE\n\t}\n}\n"; }
    { std::ofstream(base / "gfx" / "misc" / "cloud.tga") << "x"; }
    const Index index = scan(base.string());
    check(index.hasShader("gfx/effects/wcloud.tga"), "der Shader wird auch mit Endung gefunden");
    check(index.mapOf("gfx/effects/wcloud.tga") == "gfx/misc/cloud.tga",
          "mit seiner map-Zeile");
    check(index.blendOf("gfx/effects/wcloud.tga") == efx::shader::BlendMode::Additive,
          "und seiner Mischung statt der Ersatzmischung");
    const auto where = findTexture(index, base.string(), "gfx/effects/wcloud.tga");
    check(where.found && where.path == "gfx/misc/cloud.tga",
          "das Bild kommt aus dem Shader, nicht aus dem Namen");
    fs::remove_all(base);
}

// --- 10. Klang ohne Endung --------------------------------------------------
void testSoundWithoutExtension() {
    std::cout << "== Klang ohne Endung ==\n";
    // S_LoadSound_Actual (snd_mem.cpp): ohne Endung COM_DefaultExtension
    // ".wav", dann der Rueckfall auf ".mp3" (S_LoadSound_FileLoadAndNameAdjuster).
    // disruptor/alt_miss.efx: `sound/weapons/disruptor/hit_wall` -> .mp3.
    namespace fs = std::filesystem;
    using namespace efx::assets;
    const fs::path base = fs::temp_directory_path() / "efxed_soundnoext";
    fs::remove_all(base);
    fs::create_directories(base / "sound" / "fx");
    { std::ofstream(base / "sound" / "fx" / "hit_wall.mp3") << "m"; }
    { std::ofstream(base / "sound" / "fx" / "glass.wav") << "w"; }
    { std::ofstream(base / "sound" / "fx" / "both.wav") << "w"; }
    { std::ofstream(base / "sound" / "fx" / "both.mp3") << "m"; }
    const Index index = scan(base.string());
    const auto mp3 = findSound(index, base.string(), "sound/fx/hit_wall");
    check(mp3.found && mp3.path == "sound/fx/hit_wall.mp3", "ohne Endung: .wav, dann .mp3");
    const auto wav = findSound(index, base.string(), "sound/fx/glass");
    check(wav.found && wav.path == "sound/fx/glass.wav", "ohne Endung: .wav gefunden");
    const auto both = findSound(index, base.string(), "sound/fx/both");
    check(both.found && both.path == "sound/fx/both.wav", ".wav kommt vor .mp3");
    fs::remove_all(base);
}

// --- 11. Zylinder im 16-Bit-Indexbereich ------------------------------------
void testCylinderIndexBudget() {
    std::cout << "== Zylinder: 16-Bit-Indizes ==\n";
    // Dieselbe Grenze wie hasRoomForQuad; ein Zylinder braucht 64 Eckpunkte.
    const auto effect = efx::read(
        "Cylinder\n{\n\tcount\t2000\n\tlife\t1000\n\tsize\n\t{\n\t\tstart\t10\n\t}\n"
        "\tlength\n\t{\n\t\tstart\t20\n\t}\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n").effect;
    efx::particles::System system;
    system.play(effect, 1u);
    const auto list = system.build(10.0f, efx::camera::Vec3{1, 0, 0},
                                   efx::camera::Vec3{0, 0, 1});
    bool inRange = true;
    for (const auto& group : list.byTexture) {
        if (group.second.vertices.size() > 65535) inRange = false;
        for (auto index : group.second.indices) {
            if (index >= group.second.vertices.size()) inRange = false;
        }
    }
    check(inRange, "2000 Zylinder: kein Index zeigt ueber das Netz hinaus");
    check(list.skipped > 0, "was nicht passt, wird gezaehlt statt still zerstoert");
    std::printf("  2000 Zylinder: %d gezeichnet, %d weggelassen\n", list.drawn, list.skipped);

    efx::scene::Mesh almostFull;
    almostFull.vertices.resize(65535 - 10);
    check(!efx::particles::addCylinder(almostFull, {}, {0, 0, 1}, 10.0f, 5.0f, 5.0f, 0) &&
              almostFull.vertices.size() == 65535 - 10 && almostFull.indices.empty(),
          "ein fast volles Netz bekommt keinen halben Zylinder");
}

// --- 12. Pruefregeln: doppelt und falsch ------------------------------------
void testValidatorBulkFindings() {
    std::cout << "== Pruefregeln aus dem Massentest ==\n";
    // rgb-Ende ohne Kurvenart stand an zwei Stellen (VNoCurve "rgb" und
    // VRgbEndNoCurve) — 67-mal je zweimal gemeldet.
    const auto colour = efx::read(
        "Particle\n{\n\tlife\t100\n\trgb\n\t{\n\t\tstart\t1 0 0\n\t\tend\t0 0 1\n\t}\n"
        "\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n").effect;
    int rgbWarnings = 0;
    for (const auto& d : efx::validate(colour)) {
        if (d.id == efx::i18n::Str::VNoCurve || d.id == efx::i18n::Str::VRgbEndNoCurve) {
            ++rgbWarnings;
        }
    }
    check(rgbWarnings == 1, "rgb-Ende ohne Kurvenart wird genau einmal gemeldet");

    // Ohne life lebt die Primitive 50 ms: mLife.SetRange( 50.0f, 50.0f ) im
    // Erzeuger von CPrimitiveTemplate — nicht "genau ein Bild".
    const auto noLife =
        efx::read("Particle\n{\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n").effect;
    bool says50 = false;
    for (const auto& d : efx::validate(noLife)) {
        if (d.id == efx::i18n::Str::VNoLife && d.message.find("50") != std::string::npos) {
            says50 = true;
        }
    }
    check(says50, "der Hinweis zu fehlendem life nennt die 50 ms der Engine");
    efx::particles::System system;
    system.play(noLife, 1u);
    check(!system.live().empty() &&
              std::fabs(system.live()[0].deathMs - system.live()[0].spawnMs - 50.0f) < 0.01f,
          "und die Vorschau rechnet ebenso mit 50 ms");
}

// --- 13. map $whiteimage ----------------------------------------------------
void testWhiteImageShader() {
    std::cout << "== map $whiteimage ==\n";
    // ParseStage (tr_shader.cpp): `$whiteimage` -> tr.whiteImage, angelegt in
    // R_CreateBuiltinImages (tr_image.cpp) als 8x8, alles 255.
    // gfx/effects/whiteFlash in gfx2.shader, benutzt von atst/side_alt_explosion.efx.
    namespace fs = std::filesystem;
    using namespace efx::assets;
    const fs::path base = fs::temp_directory_path() / "efxed_whiteimage";
    fs::remove_all(base);
    fs::create_directories(base / "shaders");
    { std::ofstream(base / "shaders" / "gfx2.shader")
          << "gfx/effects/whiteFlash\n{\n\t{\n\t\tmap $whiteimage\n"
             "\t\tblendFunc GL_ONE GL_ONE\n\t}\n}\n"; }
    const Index index = scan(base.string());
    check(index.blendOf("gfx/effects/whiteFlash") == efx::shader::BlendMode::Additive,
          "die Mischung des Shaders wird gemerkt");
    const auto where = findTexture(index, base.string(), "gfx/effects/whiteFlash");
    check(where.found && where.white, "der Shader fuehrt zum eingebauten weissen Bild");
    const auto bytes = readFile(base.string(), where);
    const auto picture = efx::image::decode(bytes.data(), bytes.size());
    bool allWhite = picture.width == 8 && picture.rgba.size() == 8u * 8u * 4u;
    for (unsigned char value : picture.rgba) allWhite = allWhite && value == 255;
    check(allWhite, "readFile liefert ein weisses 8x8-Bild wie tr.whiteImage");
    fs::remove_all(base);
}

// --- 14. scanAll uebernimmt alphaGen-Wellen einmal --------------------------
void testScanAllCopiesAlphaWavesOnce() {
    std::cout << "== scanAll: alphaGen wave einmal ==\n";
    namespace fs = std::filesystem;
    using namespace efx::assets;
    const fs::path base = fs::temp_directory_path() / "efxed_alphawave";
    fs::remove_all(base);
    fs::create_directories(base / "shaders");
    { std::ofstream(base / "shaders" / "w.shader")
          << "gfx/pulse\n{\n\t{\n\t\tmap gfx/pulse.tga\n\t\talphaGen wave sin 0.5 0.5 0 1\n"
             "\t}\n}\n"; }
    const Index one = scan(base.string());
    const Index all = scanAll({base.string()});
    bool emptyName = false;
    for (const auto& entry : all.shaderAlphaWaves) emptyName |= entry.first.empty();
    check(one.shaderAlphaWaves.size() == 1 && all.shaderAlphaWaves.size() == 1 && !emptyName,
          "scanAll uebernimmt jede Welle genau einmal, ohne leere Namen");
    fs::remove_all(base);
}

// --- 15. Kleine Abweichungen des Lesers -------------------------------------
void testParserMatchesEngineLimits() {
    std::cout << "== Leser: Grenzen wie in der Engine ==\n";
    // rgb mit einer Zahl: CPrimitiveTemplate::ParseVector verlangt 3 oder 6
    // (`if ( v < 3 || v == 4 || v == 5 ) return false`) — die Farbe bleibt 1 1 1.
    const efx::ReadResult grey = efx::read(
        "Particle\n{\n\trgb\n\t{\n\t\tstart\t0.25\n\t}\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n");
    check(!grey.effect.primitives[0].rgb.start.set, "rgb { start 0.25 } wird ueberlesen");
    bool warned = false;
    for (const auto& d : grey.diagnostics) warned |= d.severity == efx::Severity::Warning;
    check(warned, "mit einer Warnung");

    // Kurvenflags: ParseGroupFlags hat vier Plaetze. Das fuenfte Wort setzt
    // nichts mehr — bleibt aber beim Speichern stehen.
    const efx::ReadResult five = efx::read(
        "Particle\n{\n\tsize\n\t{\n\t\tstart\t1\n\t\tend\t2\n"
        "\t\tflags\trandom random random random linear\n\t}\n}\n");
    const auto& size = five.effect.primitives[0].size;
    check(size.curveFlags == efx::kCurveRandom, "das fuenfte Kurvenwort liest die Engine nicht");
    check(size.curveWords.size() == 5 &&
              efx::write(five.effect).find("random random random random linear") !=
                  std::string::npos,
          "die Zeile bleibt beim Speichern unveraendert");

    // flags/spawnFlags: ParseFlags und ParseSpawnFlags haben sieben Plaetze.
    const efx::ReadResult eight = efx::read(
        "Particle\n{\n\tflags\tuseAlpha useAlpha useAlpha useAlpha useAlpha useAlpha "
        "useAlpha depthHack\n}\n");
    check((eight.effect.primitives[0].flags & efx::kFlagDepthHack) == 0 &&
              (eight.effect.primitives[0].flags & efx::kFlagUseAlpha) != 0,
          "das achte Flag-Wort liest die Engine nicht");
}

// ===========================================================================
// Darstellung gegen die Engine (Render-Audit). Jede Pruefung nennt die
// Fundstelle in OpenJK (SP code/cgame, code/rd-vanilla).
// ===========================================================================

// Ein Effekt aus Text, fuer kurze Pruefungen.
efx::Effect effectFrom(const std::string& text) { return efx::read(text).effect; }

// Eine Shaderbibliothek aus Text und der passende Rueckruf fuer build().
struct ShaderBook {
    efx::shader::Library library;
    explicit ShaderBook(const std::string& text) { efx::shader::parseInto(library, text, "t"); }
    efx::particles::System::ShaderLookup lookup() const {
        return [this](const std::string& name) {
            efx::particles::System::ShaderDraw draw;
            draw.definition = library.find(name);
            return draw;
        };
    }
};

const efx::particles::DrawGroup* groupOf(const efx::particles::DrawList& list,
                                         const std::string& shader, int stage = 0) {
    for (const auto& g : list.groups) {
        if (g.shader == shader && g.stage == stage) return &g;
    }
    return nullptr;
}

// --- 1. Linie, Schweif, Blitz als Baender ------------------------------------
void testRenderLinesAndBolts() {
    std::cout << "== Linien, Schweife, Blitze als Baender ==\n";
    using namespace efx::particles;
    // RB_SurfaceLine: right = normalize(cross(start-eye, end-eye)); DoLine mit
    // spanWidth = e->radius = size. Linie von (0,0,0) nach (0,0,100), Auge
    // bei (100,0,50): Querrichtung y, Ecken bei y = +-4, t = 0 am Anfang.
    efx::scene::Mesh band;
    const efx::camera::Vec3 a{0, 0, 0}, b{0, 0, 100}, eye{100, 0, 50};
    addLineQuad(band, a, b, lineSide(a, b, eye), 4.0f, 0);
    bool ok = band.vertices.size() == 4;
    for (const auto& v : band.vertices) ok = ok && std::fabs(v.pos[0]) < 1e-4f;
    ok = ok && std::fabs(std::fabs(band.vertices[0].pos[1]) - 4.0f) < 1e-4f &&
         band.vertices[0].uv[1] == 0.0f && band.vertices[2].uv[1] == 1.0f &&
         std::fabs(band.vertices[2].pos[2] - 100.0f) < 1e-4f;
    check(ok, "Line: Band quer zur Sichtlinie, halbe Breite = size, t von 0 nach 1");

    // Ueber das System: eine Line mit size 4 ist 8 Einheiten breit, nicht ein Pixel.
    const auto line = effectFrom(
        "Line\n{\n\tlife\t1000\n\torigin2\t100 0 0\n\tsize\n\t{\n\t\tstart\t4\n\t}\n"
        "\tshaders\n\t[\n\t\tgfx/beam\n\t]\n}\n");
    System lines;
    lines.play(line, 1u);
    System::View view;
    view.eye = {100.0f, 0.0f, 50.0f};
    const auto built = lines.build(100.0f, {1, 0, 0}, {0, 1, 0}, {}, &view);
    const auto* group = groupOf(built, "gfx/beam");
    check(group && group->mesh.vertices.size() == 4,
          "die Line kommt als texturiertes Band in die Zeichengruppen");

    // Blitz: grobe Form fest fuer die Lebensdauer (mRefEnt.frame, einmal in
    // CElectricity::Initialize), Feinzacken je Bild (CreateShape, Q_flrand).
    // Die Enden jedes 16er-Schritts (cur, old) haengen nur an boltSeed.
    BoltShape shape;
    shape.radius = 2.0f;
    shape.chaos = 1.0f;
    efx::scene::Mesh one, two, other;
    addElectricity(one, {0, 0, 0}, {160, 0, 0}, {80, -400, 0}, shape, 777, 1u, 0);
    addElectricity(two, {0, 0, 0}, {160, 0, 0}, {80, -400, 0}, shape, 777, 99u, 0);
    addElectricity(other, {0, 0, 0}, {160, 0, 0}, {80, -400, 0}, shape, 778, 1u, 0);
    auto macro = [](const efx::scene::Mesh& m, size_t step) {
        // Band 9*step beginnt bei `cur` dieses Schritts (ApplyShape(cur, old)).
        const auto& v0 = m.vertices[step * 9 * 4];
        const auto& v1 = m.vertices[step * 9 * 4 + 1];
        return efx::camera::Vec3{(v0.pos[0] + v1.pos[0]) * 0.5f, (v0.pos[1] + v1.pos[1]) * 0.5f,
                                 (v0.pos[2] + v1.pos[2]) * 0.5f};
    };
    bool sameMacro = one.vertices.size() == two.vertices.size();
    bool microDiffers = false;
    for (size_t s = 0; sameMacro && s < one.vertices.size() / 36; ++s) {
        sameMacro = efx::camera::length(macro(one, s) - macro(two, s)) < 1e-3f;
    }
    for (size_t i = 0; i < std::min(one.vertices.size(), two.vertices.size()); ++i) {
        if (std::fabs(one.vertices[i].pos[1] - two.vertices[i].pos[1]) > 1e-3f) microDiffers = true;
    }
    check(sameMacro, "Blitz: die grobe Form haengt nur an boltSeed — fest fuer die Lebensdauer");
    check(microDiffers, "und die Feinzacken zittern von Bild zu Bild");
    check(efx::camera::length(macro(one, 3) - macro(other, 3)) > 1e-3f,
          "ein anderer boltSeed ist ein anderer Blitz");

    // taper: radius * (1 - perc^2) — an der Spitze null.
    shape.taper = true;
    efx::scene::Mesh tapered;
    addElectricity(tapered, {0, 0, 0}, {160, 0, 0}, {80, -400, 0}, shape, 777, 1u, 0);
    // Erstes Band jedes Schritts: Ecken 0/1 bei cur mit newRadius.
    const size_t lastStep = tapered.vertices.size() / 36 - 1;
    const auto& tipA = tapered.vertices[lastStep * 36];
    const auto& tipB = tapered.vertices[lastStep * 36 + 1];
    const float tipWidth = std::sqrt((tipA.pos[0] - tipB.pos[0]) * (tipA.pos[0] - tipB.pos[0]) +
                                     (tipA.pos[1] - tipB.pos[1]) * (tipA.pos[1] - tipB.pos[1]) +
                                     (tipA.pos[2] - tipB.pos[2]) * (tipA.pos[2] - tipB.pos[2]));
    check(tipWidth < 1e-3f, "taper: an der Spitze ist der Blitz null breit");

    // grow: bei halber Lebensdauer reicht er bis zur Haelfte (RB_SurfaceElectricity).
    shape.taper = false;
    shape.growPerc = 0.5f;
    efx::scene::Mesh half;
    addElectricity(half, {0, 0, 0}, {320, 0, 0}, {160, -400, 0}, shape, 777, 1u, 0);
    float farthest = 0.0f;
    for (const auto& v : half.vertices) farthest = std::max(farthest, v.pos[0]);
    check(farthest < 175.0f && farthest > 140.0f, "grow: bei 50 % reicht der Blitz bis zur Mitte");

    // branch: bis zu drei Abzweige — mehr Baender als ohne.
    shape.growPerc = 1.0f;
    shape.branch = true;
    int moreWithBranch = 0;
    for (int seed = 1; seed <= 20; ++seed) {
        efx::scene::Mesh plain, forked;
        BoltShape noBranch = shape;
        noBranch.branch = false;
        addElectricity(plain, {0, 0, 0}, {600, 0, 0}, {300, -900, 0}, noBranch, seed, 1u, 0);
        addElectricity(forked, {0, 0, 0}, {600, 0, 0}, {300, -900, 0}, shape, seed, 1u, 0);
        if (forked.vertices.size() > plain.vertices.size()) ++moreWithBranch;
    }
    check(moreWithBranch > 0, "branch: der Blitz bekommt Abzweige");

    // Unruhe ohne Angabe: 0 — mElasticity steht nicht im Erzeuger (FxTemplate.cpp).
    System bolt;
    bolt.play(effectFrom("Electricity\n{\n\tlife\t100\n\torigin2\t100 0 0\n}\n"), 1u);
    check(!bolt.live().empty() && bolt.live()[0].chaos == 0.0f, "Electricity ohne bounce: chaos 0");
}

// --- 2. size ist ein Radius ---------------------------------------------------
void testRenderSizeIsRadius() {
    std::cout << "== size ist ein Radius ==\n";
    // CParticle::UpdateSize: mRefEnt.radius = size; RB_SurfaceSprite spannt mit
    // left = axis[1]*radius, up = axis[2]*radius auf. Size 10 -> Ecke 10*sqrt(2).
    using namespace efx::particles;
    System system;
    system.play(effectFrom("Particle\n{\n\tlife\t1000\n\tsize\n\t{\n\t\tstart\t10\n\t}\n"
                           "\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n"),
                1u);
    const auto list = system.build(100.0f, {1, 0, 0}, {0, 1, 0});
    const auto& v = list.byTexture.begin()->second.vertices[0];
    const float d = std::sqrt(v.pos[0] * v.pos[0] + v.pos[1] * v.pos[1] + v.pos[2] * v.pos[2]);
    check(std::fabs(d - 10.0f * std::sqrt(2.0f)) < 0.01f, "Particle size 10: Ecke 10*sqrt(2) von der Mitte");

    // OrientedParticle ebenso (RB_SurfaceOrientedQuad, radius).
    System oriented;
    oriented.play(effectFrom("OrientedParticle\n{\n\tlife\t1000\n\tsize\n\t{\n\t\tstart\t10\n\t}\n}\n"),
                  1u);
    const auto olist = oriented.build(100.0f, {1, 0, 0}, {0, 1, 0});
    const auto& o = olist.byTexture.begin()->second.vertices[0];
    const float od = std::sqrt(o.pos[0] * o.pos[0] + o.pos[1] * o.pos[1] + o.pos[2] * o.pos[2]);
    check(std::fabs(od - 10.0f * std::sqrt(2.0f)) < 0.01f, "OrientedParticle size 10: ebenso");

    // Ausrichtung: MakeNormalVectors. Normale (1,0,0): (0,0) liegt bei (0,-1,1)*r.
    efx::scene::Mesh quad;
    addOrientedQuad(quad, {}, {1, 0, 0}, 1.0f, 0.0f, 0);
    bool found = false;
    for (const auto& vertex : quad.vertices) {
        if (vertex.uv[0] == 0.0f && vertex.uv[1] == 0.0f) {
            found = std::fabs(vertex.pos[1] + 1.0f) < 1e-4f && std::fabs(vertex.pos[2] - 1.0f) < 1e-4f;
        }
    }
    check(found, "OrientedQuad: Ecke (0,0) wie RB_AddQuadStamp mit MakeNormalVectors");
}

// --- 3. Zylinder ---------------------------------------------------------------
void testRenderCylinderEnds() {
    std::cout << "== Zylinder: Radien und Enden ==\n";
    // CCylinder::Draw: oldorigin = origin + length*axis; RB_SurfaceCylinder:
    // size2 (backlerp) am Ursprung, size (radius) am fernen Ende.
    using namespace efx::particles;
    System system;
    system.play(effectFrom("Cylinder\n{\n\tlife\t1000\n\tsize\n\t{\n\t\tstart\t4\n\t}\n"
                           "\tsize2\n\t{\n\t\tstart\t20\n\t}\n\tlength\n\t{\n\t\tstart\t50\n\t}\n}\n"),
                1u);
    const auto list = system.build(100.0f, {1, 0, 0}, {0, 1, 0});
    float atOrigin = 0.0f, atEnd = 0.0f;
    for (const auto& v : list.byTexture.begin()->second.vertices) {
        const float r = std::sqrt(v.pos[0] * v.pos[0] + v.pos[1] * v.pos[1]);
        if (std::fabs(v.pos[2]) < 0.01f) atOrigin = std::max(atOrigin, r);
        if (std::fabs(v.pos[2] - 50.0f) < 0.01f) atEnd = std::max(atEnd, r);
    }
    check(std::fabs(atOrigin - 20.0f) < 0.01f && std::fabs(atEnd - 4.0f) < 0.01f,
          "size2 20 am Ursprung, size 4 am Ende — volle Radien");
    check(cylinderSegments(0.0f) == 40 && cylinderSegments(5000.0f) == 8,
          "Segmente wie RB_SurfaceCylinder: 40 nah, 8 fern");
}

// --- 4. random je Bild -----------------------------------------------------------
void testRenderRandomPerFrame() {
    std::cout << "== random je Bild ==\n";
    // UpdateAlpha: mischen, auf 0..1 schneiden, DANN `Q_flrand(0,1) * perc1`
    // — in jedem Bild neu. alpha 0.5 -> 1 mit random (ohne linear): Wert ist
    // start = 0.5, mal Zufall: Mittel 0.25, nicht 0.75.
    using namespace efx::particles;
    System system;
    system.play(effectFrom("Particle\n{\n\tlife\t100000\n\tflags\tuseAlpha\n"
                           "\talpha\n\t{\n\t\tstart\t0.5\n\t\tend\t1\n\t\tflags\trandom\n\t}\n"
                           "\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n"),
                1u);
    double sum = 0.0, sumSq = 0.0;
    const int frames = 400;
    for (int f = 0; f < frames; ++f) {
        const auto list = system.build(100.0f + f * 16.7f, {1, 0, 0}, {0, 1, 0});
        const double a = static_cast<double>((list.byTexture.begin()->second.vertices[0].colour >> 24) & 0xFF) / 255.0;
        sum += a;
        sumSq += a * a;
    }
    const double mean = sum / frames;
    const double stdev = std::sqrt(std::max(0.0, sumSq / frames - mean * mean));
    check(std::fabs(mean - 0.25) < 0.04, "alpha random: Mittel 0.25 (Zufall nach dem Mischen)");
    check(stdev > 0.08, "und es flackert von Bild zu Bild");
    std::printf("  alpha 0.5 random: Mittel %.3f, Streuung %.3f\n", mean, stdev);
    // Dasselbe Bild zweimal angefahren: derselbe Wert (Zurueckspulen).
    const auto x = system.build(5000.0f, {1, 0, 0}, {0, 1, 0});
    const auto y = system.build(5000.0f, {1, 0, 0}, {0, 1, 0});
    check(x.byTexture.begin()->second.vertices[0].colour ==
              y.byTexture.begin()->second.vertices[0].colour,
          "derselbe Zeitpunkt ergibt denselben Wert");
}

// --- 5. absoluteVel / absoluteAccel ------------------------------------------
void testRenderAbsoluteVelocity() {
    std::cout << "== absoluteVel / absoluteAccel ==\n";
    // FxScheduler.cpp: FX_VEL_IS_ABSOLUTE -> VectorSet( vel, mVelX, mVelY, mVelZ ).
    using namespace efx::particles;
    const auto effect = effectFrom(
        "Particle\n{\n\tlife\t1000\n\tspawnFlags\tabsoluteVel absoluteAccel\n"
        "\tvelocity\t0 0 100\n\tacceleration\t0 0 10\n}\n");
    bool all = true;
    for (int orientation = 0; orientation < 3; ++orientation) {
        System system;
        system.play(effect, 1u, {}, axisFor(orientation));
        const auto& item = system.live()[0];
        all = all && std::fabs(item.velocity.z - 100.0f) < 1e-3f &&
              std::fabs(item.velocity.x) < 1e-3f && std::fabs(item.velocity.y) < 1e-3f &&
              std::fabs(item.acceleration.z - 10.0f) < 1e-3f;
    }
    check(all, "absolute Geschwindigkeit und Beschleunigung in Weltkoordinaten, jede Ausrichtung");

    // Nur Particle/OrientedParticle/Tail/Emitter bewegen sich (FxScheduler.cpp).
    System still;
    still.play(effectFrom("Line\n{\n\tlife\t1000\n\tvelocity\t100 0 0\n\torigin2\t0 0 50\n}\n"), 1u);
    check(efx::camera::length(still.live()[0].positionAt(900.0f)) < 1e-3f,
          "eine Line mit velocity bewegt sich nicht");
}

// --- 6. Bildfolge je Teilchen ---------------------------------------------------
void testRenderAnimPerParticle() {
    std::cout << "== Bildfolgen je Teilchen (setShaderTime) ==\n";
    // tr_backend.cpp: shaderTime = floatTime - e.shaderTime; mit setShaderTime
    // ist e.shaderTime der Entstehungszeitpunkt (CEffect::SetTimeStart).
    using namespace efx::particles;
    const ShaderBook book(
        "gfx/boom\n{\n\t{\n\t\toneshotanimmap 6 f0.tga f1.tga f2.tga f3.tga f4.tga f5.tga f6.tga f7.tga\n"
        "\t\tblendFunc GL_ONE GL_ONE\n\t}\n}\n");
    const auto effect = effectFrom(
        "Particle\n{\n\tlife\t5000\n\tflags\tsetShaderTime\n\tshaders\n\t[\n\t\tgfx/boom\n\t]\n}\n"
        "Particle\n{\n\tlife\t5000\n\tdelay\t500\n\tflags\tsetShaderTime\n\tshaders\n\t[\n\t\tgfx/boom\n\t]\n}\n");
    System system;
    system.play(effect, 1u);
    const auto list = system.build(600.0f, {1, 0, 0}, {0, 1, 0}, book.lookup());
    bool frame3 = false, frame0 = false;
    for (const auto& g : list.groups) {
        if (g.image == std::string(efx::assets::kImagePrefix) + "f3.tga") frame3 = true;
        if (g.image == std::string(efx::assets::kImagePrefix) + "f0.tga") frame0 = true;
    }
    check(frame3 && frame0, "bei 600 ms: das erste Teilchen zeigt Bild 3, das spaete Bild 0");
}

// --- 7./8./9. Shaderstufen, Mischung, Reihenfolge -----------------------------
void testRenderShaderStages() {
    std::cout << "== Shaderstufen, Faktoren, Reihenfolge ==\n";
    using namespace efx::particles;
    const ShaderBook book(
        "a_add\n{\n\t{\n\t\tmap a.tga\n\t\tblendFunc GL_ONE GL_ONE\n\t}\n}\n"
        "z_smoke\n{\n\t{\n\t\tmap z.tga\n\t\tblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n"
        "\t\trgbGen vertex\n\t\talphaGen vertex\n\t}\n}\n"
        "dark\n{\n\t{\n\t\tmap d.tga\n\t\tblendFunc GL_ZERO GL_ONE_MINUS_SRC_COLOR\n\t}\n}\n"
        "soft\n{\n\t{\n\t\tmap s.tga\n\t\tblendFunc GL_SRC_ALPHA GL_ONE\n\t\talphaFunc GE128\n\t}\n}\n"
        "two\n{\n\t{\n\t\tmap t1.tga\n\t\tblendFunc GL_ONE GL_ONE\n\t}\n"
        "\t{\n\t\tmap t2.tga\n\t\tblendFunc GL_ONE GL_ONE\n\t\trgbGen vertex\n\t}\n}\n"
        "near\n{\n\tsort nearest\n\t{\n\t\tmap n.tga\n\t\tblendFunc GL_ONE GL_ONE\n\t}\n}\n");
    std::string text;
    for (const char* name : {"near", "a_add", "z_smoke", "dark", "soft", "two"}) {
        text += std::string("Particle\n{\n\tlife\t1000\n\trgb\n\t{\n\t\tstart\t1 0 0\n\t}\n"
                            "\tshaders\n\t[\n\t\t") + name + "\n\t]\n}\n";
    }
    text += "Particle\n{\n\tlife\t1000\n\tflags\tuseAlpha\n\tshaders\n\t[\n\t\tgfx/ohneblock\n\t]\n}\n";
    System system;
    system.play(effectFrom(text), 1u);
    const auto list = system.build(100.0f, {1, 0, 0}, {0, 1, 0}, book.lookup());

    // Faktoren eins zu eins.
    const auto* dark = groupOf(list, "dark");
    check(dark && dark->blended && dark->src == efx::shader::BlendFactor::Zero &&
              dark->dst == efx::shader::BlendFactor::OneMinusSrcColor,
          "GL_ZERO GL_ONE_MINUS_SRC_COLOR bleibt, wie es ist (dunkelt ab)");
    const auto* soft = groupOf(list, "soft");
    check(soft && soft->src == efx::shader::BlendFactor::SrcAlpha &&
              soft->dst == efx::shader::BlendFactor::One && soft->alphaTest == 3,
          "GL_SRC_ALPHA GL_ONE und alphaFunc GE128 kommen bei der Zeichnung an");

    // Alle Stufen, und rgbGen ohne Angabe: identityLighting = Weiss — die rote
    // Farbe der Primitive wirkt nicht (ParseStage, ComputeColors).
    const auto* first = groupOf(list, "two", 0);
    const auto* second = groupOf(list, "two", 1);
    check(first && second, "ein Shader mit zwei Stufen ergibt zwei Zeichengruppen");
    if (first && second) {
        const uint32_t c0 = first->mesh.vertices[0].colour;
        const uint32_t c1 = second->mesh.vertices[0].colour;
        check((c0 & 0xFFFFFFu) == 0xFFFFFFu, "Stufe ohne rgbGen: Weiss statt Rot");
        check((c1 & 0xFFu) == 255u && ((c1 >> 8) & 0xFFu) == 0u, "Stufe mit rgbGen vertex: Rot");
    }

    // Reihenfolge: SS_BLEND0 (z_smoke, dark, soft, Ersatzshader) vor SS_BLEND1
    // (a_add, two), `sort nearest` zuletzt — nicht alphabetisch.
    auto position = [&](const std::string& name) {
        for (size_t i = 0; i < list.groups.size(); ++i) {
            if (list.groups[i].shader == name) return static_cast<int>(i);
        }
        return -1;
    };
    check(position("z_smoke") < position("a_add"), "alphagemischter Rauch vor dem additiven Glimmen");
    check(position("near") == static_cast<int>(list.groups.size()) - 1, "sort nearest zuletzt");

    // Kein Shaderblock: Ersatzshader (R_FindShader, LIGHTMAP_2D) — Alphamischung
    // ohne Tiefentest, Farbe und Alpha aus dem Eckpunkt.
    const auto* plain = groupOf(list, "gfx/ohneblock");
    check(plain && plain->blended && !plain->depthTest &&
              plain->src == efx::shader::BlendFactor::SrcAlpha,
          "Bild ohne Shaderblock: Alphamischung ohne Tiefentest");

    // Weder Block noch Bild: das graue Kaestchen der Engine.
    System missing;
    missing.play(effectFrom("Particle\n{\n\tlife\t1000\n\tshaders\n\t[\n\t\tgibt/es/nicht\n\t]\n}\n"),
                 1u);
    const auto none = missing.build(100.0f, {1, 0, 0}, {0, 1, 0}, [](const std::string&) {
        System::ShaderDraw draw;
        draw.missing = true;
        return draw;
    });
    check(!none.groups.empty() && none.groups[0].image == "$default" && !none.groups[0].blended,
          "fehlender Shader: tr.defaultShader (undurchsichtiges graues Kaestchen)");
}

// --- 10. Anzahl -----------------------------------------------------------------
void testRenderCountRounding() {
    std::cout << "== count rundet wie GetRoundedVal ==\n";
    // CFxRange::GetRoundedVal: (int)(flrand(min,max) + 0.5), ohne Untergrenze.
    int zero = 0, total = 0;
    const auto effect = effectFrom("Particle\n{\n\tcount\t0 2\n\tlife\t100\n}\n");
    for (unsigned seed = 1; seed <= 2000; ++seed) {
        efx::sim::Random random(seed);
        const auto spawns = efx::sim::schedule(effect, random);
        if (spawns.empty()) ++zero;
        total += static_cast<int>(spawns.size());
    }
    check(zero > 400 && zero < 600, "count 0 2: in rund einem Viertel der Faelle gar nichts");
    check(std::fabs(total / 2000.0 - 1.0) < 0.06, "und im Mittel 1.0");
    std::printf("  count 0 2: %d von 2000 ohne Teilchen, Mittel %.3f\n", zero, total / 2000.0);
    efx::sim::Random random(1u);
    check(efx::sim::schedule(effectFrom("Particle\n{\n\tcount\t2.7\n}\n"), random).size() == 2,
          "count 2.7 ohne Spanne: abgeschnitten auf 2");
    // Der Ausgangswert wird verruehrt: aufeinanderfolgende Werte geben nicht
    // fast denselben ersten Zufallswert.
    float lowest = 1.0f, highest = 0.0f;
    for (unsigned seed = 1; seed <= 400; ++seed) {
        efx::sim::Random r(seed);
        const float v = r.next();
        lowest = std::min(lowest, v);
        highest = std::max(highest, v);
    }
    check(lowest < 0.05f && highest > 0.95f, "Ausgangswerte 1..400 streuen ueber 0..1");
}

// --- 11. Decal ----------------------------------------------------------------------
void testRenderDecal() {
    std::cout << "== Decal wie CG_ImpactMark ==\n";
    // FxScheduler.cpp ruft CG_ImpactMark mit den STARTwerten; cg_marks.cpp:
    // MARK_TOTAL_TIME 10000, die letzte Sekunde blendet das Alpha aus.
    using namespace efx::particles;
    const auto effect = effectFrom(
        "Decal\n{\n\tsize\n\t{\n\t\tstart\t8\n\t\tend\t30\n\t\tflags\tlinear\n\t}\n"
        "\trgb\n\t{\n\t\tstart\t1 0 0\n\t\tend\t0 0 1\n\t\tflags\tlinear\n\t}\n"
        "\talpha\n\t{\n\t\tstart\t0.5\n\t}\n\tshaders\n\t[\n\t\tgfx/scorch\n\t]\n}\n");
    System decal;
    decal.play(effect, 1u);
    check(std::fabs(decal.live()[0].deathMs - 10000.0f) < 0.01f, "ein Abdruck lebt 10 s, ohne life");
    const auto mid = decal.build(5000.0f, {1, 0, 0}, {0, 1, 0});
    check(!mid.byTexture.empty(), "nach 5 s ist der Abdruck noch da");
    if (mid.byTexture.empty()) return;
    const auto& v = mid.byTexture.begin()->second.vertices;
    const float half = std::sqrt(v[0].pos[0] * v[0].pos[0] + v[0].pos[1] * v[0].pos[1] +
                                 v[0].pos[2] * v[0].pos[2]) / std::sqrt(2.0f);
    check(std::fabs(half - 8.0f) < 0.01f, "Groesse = size start als Radius, keine Kurve");
    check((v[0].colour & 0xFFu) == 255u && ((v[0].colour >> 16) & 0xFFu) == 0u &&
              ((v[0].colour >> 24) & 0xFFu) == 127u,
          "Farbe = rgb start, Alpha = alpha start");
    const auto late = decal.build(9750.0f, {1, 0, 0}, {0, 1, 0});
    const uint32_t fading = late.byTexture.empty()
                                ? 0u
                                : (late.byTexture.begin()->second.vertices[0].colour >> 24) & 0xFFu;
    check(fading > 55 && fading < 70, "in der letzten Sekunde blendet das Alpha aus (255*t/1000)");
    check(mid.marks == 1, "die Statuszeile zaehlt einen Abdruck");

    // Mit Raum: auf die Flaeche, die hoechstens 20 Einheiten hinter ihm liegt.
    System onFloor;
    onFloor.play(effect, 1u, {}, {}, {}, efx::sim::roomPlanes(500.0f, 500.0f, 500.0f));
    const auto floorList = onFloor.build(100.0f, {1, 0, 0}, {0, 1, 0});
    bool flat = !floorList.byTexture.empty();
    for (const auto& vertex : floorList.byTexture.begin()->second.vertices) {
        flat = flat && std::fabs(vertex.pos[2]) < 1e-3f;
    }
    check(flat, "mit Raum liegt der Abdruck auf dem Boden");
    const auto high = effectFrom("Decal\n{\n\torigin\t100 0 0\n\tsize\n\t{\n\t\tstart\t8\n\t}\n}\n");
    System floating;
    floating.play(high, 1u, {}, {}, {}, efx::sim::roomPlanes(500.0f, 500.0f, 500.0f));
    check(floating.build(100.0f, {1, 0, 0}, {0, 1, 0}).drawn == 0,
          "ohne Flaeche in 20 Einheiten kein Abdruck (CM_MarkFragments)");
}

// --- 12. ScreenFlash ----------------------------------------------------------------
void testRenderScreenFlash() {
    std::cout << "== ScreenFlash ==\n";
    // CFlash::Draw: Sprite 8 Einheiten vor dem Auge, radius = 8*tan(fov_x/2);
    // CFlash::Init: mod = dot * (1 - dis^2/600^2).
    using namespace efx::particles;
    System flash;
    flash.play(effectFrom("Flash\n{\n\tlife\t500\n\trgb\n\t{\n\t\tstart\t1 0.5 0\n\t}\n"
                          "\tshaders\n\t[\n\t\tgfx/flash\n\t]\n}\n"),
               1u);
    System::View view;
    view.eye = {0.0f, 0.0f, 100.0f};
    view.fovXDegrees = 90.0f;
    // right x, up y — Blick entlang -z, also auf den Ursprung.
    const auto list = flash.build(100.0f, {1, 0, 0}, {0, 1, 0}, {}, &view);
    check(!list.byTexture.empty(), "der Flash wird gezeichnet");
    if (!list.byTexture.empty()) {
        const auto& v = list.byTexture.begin()->second.vertices;
        const float expected = 1.0f - (100.0f * 100.0f) / (600.0f * 600.0f);
        check(std::fabs(v[0].pos[2] - 92.0f) < 1e-3f, "8 Einheiten vor dem Auge");
        check(std::fabs(std::fabs(v[0].pos[0]) - 8.0f) < 1e-3f, "so gross wie das Sichtfeld");
        check(std::abs(static_cast<int>(v[0].colour & 0xFFu) - static_cast<int>(expected * 255.0f + 0.5f)) <= 1,
              "Farbe nach CFlash::Init gedaempft (0.97 bei 100 Einheiten)");
    }
}

// --- 13. org2FromTrace ----------------------------------------------------------------
void testRenderOrg2FromTrace() {
    std::cout << "== org2fromTrace ==\n";
    // FxScheduler.cpp: Endpunkt = Treffer eines Strahls entlang ax[0];
    // traceImpactFx startet den Einschlag dort.
    using namespace efx::particles;
    efx::Effect child = effectFrom("Particle\n{\n\tlife\t100\n}\n");
    const auto effect = effectFrom(
        "Line\n{\n\tlife\t500\n\tspawnFlags\torg2fromTrace traceImpactFx\n"
        "\timpactfx\n\t[\n\t\tkind\n\t]\n}\n");
    System system;
    system.play(effect, 1u, {}, {},
                [&](const std::string& name) -> const efx::Effect* {
                    return name == "kind" ? &child : nullptr;
                },
                efx::sim::roomPlanes(200.0f, 200.0f, 300.0f));
    const Live* line = nullptr;
    for (const auto& item : system.live()) {
        if (item.type == efx::PrimitiveType::Line) line = &item;
    }
    check(line && std::fabs(line->origin2.z - 300.0f) < 1e-2f,
          "der Endpunkt liegt an der Decke, wo der Strahl auftrifft");
    check(system.startedEffects() == 1, "und traceImpactFx startet dort den Einschlag");
}

// --- 14. Licht -------------------------------------------------------------------------
void testRenderLights() {
    std::cout << "== Lichter ==\n";
    // CLight::Draw: AddLightToScene( origin, radius = size, r, g, b ).
    using namespace efx::particles;
    System system;
    system.play(effectFrom("Light\n{\n\tlife\t1000\n\tsize\n\t{\n\t\tstart\t300\n\t}\n"
                           "\trgb\n\t{\n\t\tstart\t1 0.5 0.25\n\t}\n}\n"),
                1u);
    const auto list = system.build(100.0f, {1, 0, 0}, {0, 1, 0});
    check(list.lights.size() == 1 && std::fabs(list.lights[0].radius - 300.0f) < 1e-3f &&
              std::fabs(list.lights[0].rgb[1] - 0.5f) < 1e-3f && list.drawn == 0,
          "ein Light zeichnet nichts, kommt aber mit Radius und Farbe in die Lichterliste");
}

// --- 16. Wellen -----------------------------------------------------------------------
void testRenderWaves() {
    std::cout << "== noise und random ==\n";
    // EvalWaveForm: GF_NOISE -> R_NoiseGet4f, GF_RAND -> GetNoiseTime <= frequency.
    using efx::shader::evaluateWave;
    efx::shader::WaveForm noise;
    noise.func = "noise";
    noise.amplitude = 1.0f;
    noise.frequency = 1.0f;
    bool inRange = true, smooth = true;
    float previous = evaluateWave(noise, 0.0f);
    for (int i = 1; i <= 400; ++i) {
        const float v = evaluateWave(noise, i * 0.01f);
        inRange = inRange && v >= -1.0f && v <= 1.0f;
        smooth = smooth && std::fabs(v - previous) <= 0.021f;  // hoechstens 2*0.01 je Schritt
        previous = v;
    }
    check(inRange && smooth, "noise: weich (linear zwischen ganzen Zeitschritten) und in -1..1");
    efx::shader::WaveForm random;
    random.func = "random";
    random.base = 0.0f;
    random.amplitude = 1.0f;
    random.frequency = 1.0f;
    int on = 0;
    for (int ms = 0; ms < 256; ++ms) {
        const float v = evaluateWave(random, ms * 0.001f);
        if (v == 1.0f) ++on;
        else if (v != 0.0f) on = -1000;
    }
    check(on > 50 && on < 206, "random: an (base+amplitude) oder aus (base), je nach Rauschtabelle");
}

// --- 17. Kleinigkeiten und Statuszeile -------------------------------------------------
void testRenderSmallSimulationPoints() {
    std::cout << "== Kleinigkeiten der Simulation ==\n";
    using namespace efx::particles;
    // alpha { start 0 } ohne Kurvenart: unsichtbar (kein "dann eben 1").
    System zero;
    zero.play(effectFrom("Particle\n{\n\tlife\t1000\n\tflags\tuseAlpha\n"
                         "\talpha\n\t{\n\t\tstart\t0\n\t}\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n"),
              1u);
    const auto list = zero.build(100.0f, {1, 0, 0}, {0, 1, 0});
    check(((list.byTexture.begin()->second.vertices[0].colour >> 24) & 0xFFu) == 0u,
          "alpha start 0 bleibt 0 (UpdateAlpha)");

    // Emitter zeichnen kein Sprite (CEmitter::Draw: nur ein Modell, mit useModel).
    System emitter;
    emitter.play(effectFrom("Emitter\n{\n\tlife\t1000\n\tshaders\n\t[\n\t\tgfx/x\n\t]\n}\n"), 1u);
    check(emitter.build(100.0f, {1, 0, 0}, {0, 1, 0}).drawn == 0, "ein Emitter zeichnet kein Sprite");

    // Statuszeile: noch wartende Ausloesungen.
    System waiting;
    waiting.play(effectFrom("Particle\n{\n\tcount\t3\n\tlife\t100\n\tdelay\t500\n}\n"
                            "Particle\n{\n\tlife\t100\n}\n"),
                 1u);
    const auto early = waiting.build(10.0f, {1, 0, 0}, {0, 1, 0});
    const auto later = waiting.build(600.0f, {1, 0, 0}, {0, 1, 0});
    check(early.scheduled == 3 && later.scheduled == 0, "Scheduled zaehlt, was noch auf seine Verzoegerung wartet");

    // FxRunner: das Kind bekommt die Achse der Primitive (`ax`), nicht die des
    // Elternteils — hier mit axisFromSphere.
    efx::Effect child = effectFrom("Particle\n{\n\tlife\t100\n\tvelocity\t10 0 0\n}\n");
    System runner;
    runner.play(effectFrom("FxRunner\n{\n\tspawnFlags\torgOnSphere axisFromSphere\n"
                           "\tradius\t10\n\theight\t10\n\tplayfx\n\t[\n\t\tkind\n\t]\n}\n"),
                3u, {}, {},
                [&](const std::string& name) -> const efx::Effect* {
                    return name == "kind" ? &child : nullptr;
                });
    bool radial = !runner.live().empty();
    for (const auto& item : runner.live()) {
        const auto out = efx::camera::normalise(item.origin);
        const auto dir = efx::camera::normalise(item.velocity);
        radial = radial && efx::camera::dot(out, dir) > 0.99f;
    }
    check(radial, "FxRunner mit axisFromSphere: das Kind fliegt radial nach aussen");
}

}  // namespace

int main(int argc, char** argv) {
    // Kindmodus fuer den Absturztest: Protokoll oeffnen, in einen Schritt
    // hineinlaufen und mittendrin abbrechen — kein Abflauf, keine
    // Destruktoren, keine Pufferleerung. Genau wie bei einem echten Absturz.
    if (argc >= 3 && std::string(argv[1]) == "--crash-child") {
        efx::diag::open(argv[2]);
        { efx::diag::Step a("Einstellungen lesen"); }
        { efx::diag::Step b("Grafik anlegen"); }
        efx::diag::Step doomed("Symbolschrift laden");
        std::abort();
    }
    g_selfPath = argv[0];

    std::filesystem::path dataDir = argc > 1 ? argv[1] : "data";
    std::cout << "Testlauf efxcore\n\n";

    if (std::filesystem::exists(dataDir)) {
        testRealFiles(dataDir);
    } else {
        std::cout << "(Kein Datenverzeichnis " << dataDir << ", uebersprungen)\n";
    }
    testPrecision();
    testNumberText();
    testDiagnosticSegment();
    testFlags();
    testTolerance();
    testKnownRavenBugs();
    testTransitionConflicts();
    testDialects();
    testWindIsDead();
    if (std::filesystem::exists(dataDir)) testShaders(dataDir);
    testThemes();
    testLayout();
    testLanguages();
    testRenderers();
    testPaths();
    testGamePath();
    testCamera();
    testPlayback();
    testSpawnOrigin();
    testFields();
    testAssets();
    testCurves();
    testScheduling();
    testMotion();
    testPhysics();
    testParticles();
    testUndo();
    testTimeline();
    testSound();
    testFinishingTouches();
    testInflate();
    testImages();
    testBlendModes();
    testShake();
    testPicking();
    testBillboardFacing();
    testFitDistance();
    testScene();
    testDiagLog();
    testJobs();
    testDiagnosticIdentity();
    testTilePlacement();
    testSheetLayout();
    testSpawnPlacementFlags();
    testTexMods();
    testStretchTurbAndAlphaWave();
    testEditorOnlyKeys();
    testMaxComponents();
    testTurbulenceUsesPosition();
    testRgbGenWave();
    testTexModsReachTheQuad();
    testSpawnFlags();
    testTailAndCylinderGeometry();
    testLineAndColourRules();
    testBounceMatchesEngine();
    testCameraViews();
    testGridOnWalls();
    testLegacyRepeatMode();
    testSeamlessRepeat();
    testMotionMatchesEngine();
    testReplayDoesNotAccumulate();
    testUndefinedShaderBlend();
    testCameraShake();
    testParallelForStress();
    testIndexOverflow();
    testWallTextures();
    testAnimFrames();
    testDiscoverRoots();
    testRepeatBuildUp();
    testFindSound();
    testSoundConvert();
    testLightDrawsNothing();
    testAnimMapIndexing();
    testSheetUv();
    testCurveDefaults();
    testUseAlpha();
    testTileBudget();
    testVisualReach();
    testParallelBuild();
    testValidator();
    // Befunde aus dem Massentest ueber alle 634 ausgelieferten .efx-Dateien.
    testEmitterEmitsEmitFx();
    testCurveWordsSurviveSaving();
    testForeignGroupsKept();
    testMissingEndIsOne();
    testChildCurvesUseTheirOwnClock();
    testListsSetTheirOwnFlags();
    testSphereAndCylinderDefaults();
    testPk3SearchOrder();
    testShaderNameWithExtension();
    testSoundWithoutExtension();
    testCylinderIndexBudget();
    testValidatorBulkFindings();
    testWhiteImageShader();
    testScanAllCopiesAlphaWavesOnce();
    testParserMatchesEngineLimits();
    // Darstellung gegen die Engine (Render-Audit).
    testRenderLinesAndBolts();
    testRenderSizeIsRadius();
    testRenderCylinderEnds();
    testRenderRandomPerFrame();
    testRenderAbsoluteVelocity();
    testRenderAnimPerParticle();
    testRenderShaderStages();
    testRenderCountRounding();
    testRenderDecal();
    testRenderScreenFlash();
    testRenderOrg2FromTrace();
    testRenderLights();
    testRenderWaves();
    testRenderSmallSimulationPoints();

    std::cout << "\n" << g_checks << " Pruefungen, " << g_failures << " Fehler";
    if (g_dataFiles > 0) {
        // Siehe die Begruendung bei "Echte Raven-Dateien": ohne diese Angabe
        // laesst sich die Gesamtzahl zwischen zwei Laeufen nicht vergleichen.
        std::cout << "  (davon " << g_dataFiles * 5 << " aus " << g_dataFiles
                  << " Dateien in data/)";
    }
    std::cout << "\n";
    return g_failures == 0 ? 0 : 1;
}
