// Prueft einen Effekt gegen das, was die Engine tatsaechlich auswertet.
//
// Jede Regel hier hat eine Fundstelle im Spielcode. Es geht nicht um Geschmack,
// sondern um Faelle, in denen die Datei etwas sagt und das Spiel etwas anderes
// tut — genau die Faelle, die man sonst erst im Spiel bemerkt.
#include <set>

#include "efx/io.h"
#include <cstdio>

#include "efx/fields.h"
#include "efx/shader.h"

namespace efx {
namespace {

// Eine uebersetzte Meldung mit eingesetzten Werten.
//
// snprintf und nicht std::format: die Formate stammen aus unserer eigenen
// Uebersetzungstabelle, kommen also nicht von aussen, und der Uebersetzer auf
// dem Zielrechner soll nicht mehr koennen muessen als noetig.
//
// Der Puffer ist grosszuegig: die laengste Meldung hat etwa 240 Zeichen, und
// Japanisch braucht in UTF-8 bis zu drei Byte je Zeichen.
template <typename... Args>
std::string message(i18n::Str id, Args... args) {
    const char* format = i18n::tr(id);
    if constexpr (sizeof...(Args) == 0) {
        // Ohne Werte gar nicht erst formatieren. Sonst wuerde ein Prozent-
        // zeichen im uebersetzten Text als Platzhalter gelesen — und
        // -Wformat-security waere zu Recht unruhig.
        return format;
    } else {
        char buffer[1024];
        std::snprintf(buffer, sizeof(buffer), format, args...);
        return buffer;
    }
}


void add(std::vector<Diagnostic>& out, Severity severity,
         std::string message, i18n::Str id = i18n::Str::Count) {
    out.push_back({severity, 0, std::move(message), id});
}

// Welche Felder ein Typ auswertet, steht in einer einzigen Tabelle:
// src/fields.cpp. Hier standen frueher zwei eigene Funktionen dafuer — und
// als die Tabelle nach einer Messung genauer wurde, wichen sie ab: die
// Pruefung hielt einen Decal fuer lebensdauerlos, die Tabelle nicht.
//
// Zwei Quellen fuer dieselbe Wahrheit laufen frueher oder spaeter
// auseinander. Deshalb fragt die Pruefung jetzt die Tabelle.
bool usesShaders(PrimitiveType t) {
    return fields::applies(t, fields::Field::Shaders);
}

bool usesLife(PrimitiveType t) {
    return fields::applies(t, fields::Field::Life);
}

}  // namespace

// Passt ein Flag zum Zielzweig?
static bool availableIn(Dialect flagDialect, Dialect target) {
    if (flagDialect == Dialect::Both) return true;
    if (target == Dialect::Both) return false;  // muss ueberall laufen
    return flagDialect == target;
}

std::vector<Diagnostic> validate(const Effect& effect, Dialect target) {
    std::vector<Diagnostic> out;

    if (effect.primitives.empty()) {
        add(out, Severity::Warning, message(i18n::Str::VNoPrimitives), i18n::Str::VNoPrimitives);
    }
    // FX_MAX_EFFECT_COMPONENTS aus FxScheduler.h.
    if (effect.primitives.size() > 24) {
        add(out, Severity::Error, message(i18n::Str::VTooMany, static_cast<int>(effect.primitives.size())), i18n::Str::VTooMany);
    }

    std::set<std::string> seenNames;

    for (size_t i = 0; i < effect.primitives.size(); ++i) {
        const Primitive& p = effect.primitives[i];
        const std::string where =
            std::string(typeName(p.type)) + " #" + std::to_string(i + 1) +
            (p.name.empty() ? std::string{} : " (\"" + p.name + "\")");

        // FX_MAX_PRIM_NAME ist 32 einschliesslich Nullbyte.
        if (p.name.size() > 31) {
            add(out, Severity::Error, where + ": " + message(i18n::Str::VNameTooLong, static_cast<int>(p.name.size())), i18n::Str::VNameTooLong);
        }
        if (!p.name.empty() && !seenNames.insert(p.name).second) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VNameDuplicate), i18n::Str::VNameDuplicate);
        }

        // impactfx braucht zweierlei: das Flag UND Physik.
        //
        // Ohne Kollision gibt es keinen Aufprall — das ist der zweite
        // Stolperstein, und er faellt noch weniger auf als der erste, weil
        // die beiden Felder in verschiedenen Reitern stehen.
        if (!p.impactFx.empty()) {
            if ((p.flags & kFlagImpactRunsFx) == 0) {
                add(out, Severity::Warning,
                    where + ": " + message(i18n::Str::VImpactFxNoFlag),
                    i18n::Str::VImpactFxNoFlag);
            } else if ((p.flags & kFlagApplyPhysics) == 0) {
                add(out, Severity::Warning,
                    where + ": " + message(i18n::Str::VImpactFxNoPhysics),
                    i18n::Str::VImpactFxNoPhysics);
            }
        }

        // deathfx ohne das Flag bleibt wirkungslos.
        //
        // Der haeufigste Irrtum an diesem Feld: es zu setzen reicht nicht.
        // CParticle::Die prueft FX_DEATH_RUNS_FX, und ohne das Flag passiert
        // beim Sterben nichts — die Datei sieht aber vollstaendig aus.
        if (!p.deathFx.empty()) {
            if ((p.flags & kFlagDeathRunsFx) == 0) {
                add(out, Severity::Warning,
                    where + ": " + message(i18n::Str::VDeathFxNoFlag),
                    i18n::Str::VDeathFxNoFlag);
            } else if ((p.flags & kFlagKillOnImpact) != 0) {
                // Kein Fehler, aber ueberraschend genug fuer einen Hinweis.
                add(out, Severity::Info,
                    where + ": " + message(i18n::Str::VDeathFxKillOnImpact),
                    i18n::Str::VDeathFxKillOnImpact);
            }
        }

        // Ein abweichendes Ende ohne Uebergangsart bleibt wirkungslos.
        //
        // Der haeufigste Stolperstein am Format, und man sieht ihn der Datei
        // nicht an: der Anteil beginnt bei 1.0 und wird nur veraendert, wenn
        // eine Kurvenart gesetzt ist. Am Ende steht
        // start*perc + end*(1-perc) — ohne Flag also durchgehend start.
        //
        // Wer einen Farbverlauf schreibt und "linear" vergisst, bekommt die
        // Startfarbe und sucht den Fehler beim Shader.
        {
            const struct { const char* name; const Channel& channel; } channels[] = {
                {"size", p.size}, {"size2", p.size2}, {"length", p.length},
                {"alpha", p.alpha},
            };
            for (const auto& entry : channels) {
                if (!entry.channel.present || entry.channel.curveFlags != 0) continue;
                if (!entry.channel.end.set) continue;
                if (entry.channel.end.min == entry.channel.start.min &&
                    entry.channel.end.max == entry.channel.start.max) {
                    continue;
                }
                add(out, Severity::Warning,
                    where + ": " + message(i18n::Str::VNoCurve, entry.name),
                    i18n::Str::VNoCurve);
            }
            if (p.rgb.present && p.rgb.curveFlags == 0 && p.rgb.end.set &&
                (p.rgb.end.min[0] != p.rgb.start.min[0] ||
                 p.rgb.end.min[1] != p.rgb.start.min[1] ||
                 p.rgb.end.min[2] != p.rgb.start.min[2])) {
                add(out, Severity::Warning,
                    where + ": " + message(i18n::Str::VNoCurve, "rgb"),
                    i18n::Str::VNoCurve);
            }
        }

        if (usesShaders(p.type) && p.shaders.empty() && p.models.empty()) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VNoVisual), i18n::Str::VNoVisual);
        }
        if (p.type == PrimitiveType::Sound && p.sounds.empty()) {
            add(out, Severity::Error, where + ": " + message(i18n::Str::VSoundNoFile), i18n::Str::VSoundNoFile);
        }
        if (p.type == PrimitiveType::FxRunner && p.playFx.empty()) {
            add(out, Severity::Error, where + ": " + message(i18n::Str::VRunnerNoFx), i18n::Str::VRunnerNoFx);
        }
        if (usesLife(p.type) && !p.life.set) {
            // Die Engine raeumt erst auf, wenn mTime *groesser* als killTime ist
            // (FxUtil.cpp, strikt groesser). Bei life 0 heisst das: genau ein
            // Bild lang sichtbar. Fuer einen Schuss, der ohnehin jedes Bild neu
            // ausgeloest wird, ist das gewollt — deshalb nur ein Hinweis.
            add(out, Severity::Info, where + ": " + message(i18n::Str::VNoLife), i18n::Str::VNoLife);
        }

        // CameraShake liest ausschliesslich bounce, radius und life.
        if (p.type == PrimitiveType::CameraShake) {
            if (!p.elasticity.set) {
                add(out, Severity::Error, where + ": " + message(i18n::Str::VShakeNoBounce), i18n::Str::VShakeNoBounce);
            }
            if (!p.radius.set) {
                add(out, Severity::Error, where + ": " + message(i18n::Str::VShakeNoRadius), i18n::Str::VShakeNoRadius);
            }
            if (p.elasticity.set && p.elasticity.max > 16.0f) {
                add(out, Severity::Warning, where + ": " + message(i18n::Str::VShakeTooStrong), i18n::Str::VShakeTooStrong);
            }
            if (!p.shaders.empty() || !p.sounds.empty()) {
                add(out, Severity::Warning, where + ": " + message(i18n::Str::VShakeIgnored), i18n::Str::VShakeIgnored);
            }
        }

        // Uebergangsarten.
        //
        // nonlinear, clamp und wave teilen sich das Parameterfeld — im
        // Bitmuster ist das FX_*_PARM_MASK, zwei Bits fuer drei Zustaende.
        // Wer zwei davon setzt, bekommt nicht beides, sondern etwas Drittes:
        // linear|nonlinear ergibt zusammen genau das Bitmuster von clamp.
        // Genau dieser Fall steht in Ravens eigener explosion.efx.
        auto checkChannel = [&](const char* label, bool present, bool hasEnd,
                                bool hasStart, int curve,
                                const std::vector<std::string>& words) {
            if (!present) return;

            // Wie viele der drei parameterbehafteten Arten wurden geschrieben?
            int named = 0;
            std::string listed;
            for (const auto& word : words) {
                bool isParmType = word == "nonlinear" || word == "wave" ||
                                  word == "clamp";
                if (isParmType) {
                    ++named;
                    if (!listed.empty()) listed += " + ";
                    listed += word;
                }
            }

            if (named > 1) {
                // Was die Engine daraus macht: die zwei Bits der Parametermaske
                // ergeben genau einen Zustand.
                int parmBits = curve & kCurveClamp;
                const char* actual = parmBits == kCurveClamp     ? "clamp"
                                     : parmBits == kCurveWave    ? "wave"
                                     : parmBits == kCurveNonLinear ? "nonlinear"
                                                                   : "keine";
                add(out, Severity::Error,
                    where + ": " + label + " " +
                        message(i18n::Str::VCurveCollision, listed.c_str(),
                                actual), i18n::Str::VCurveCollision);
            }
            if (curve && !hasEnd && !hasStart) {
                add(out, Severity::Warning,
                    where + ": " + label + " " +
                        message(i18n::Str::VCurveNoValues), i18n::Str::VCurveNoValues);
            }
            if ((curve & kCurveWave) && !hasEnd) {
                add(out, Severity::Warning,
                    where + ": " + label + " " + message(i18n::Str::VWaveNoEnd), i18n::Str::VWaveNoEnd);
            }
            // random moduliert nur den vorhandenen Anteil; ohne end wandert der
            // Wert zwischen start und 0, was oft nicht gemeint ist.
            if ((curve & kCurveRandom) && !hasEnd) {
                add(out, Severity::Info,
                    where + ": " + label + " " +
                        message(i18n::Str::VRandomNoEnd), i18n::Str::VRandomNoEnd);
            }
        };
        checkChannel("alpha", p.alpha.present, p.alpha.end.set, p.alpha.start.set,
                     p.alpha.curveFlags, p.alpha.curveWords);
        checkChannel("size", p.size.present, p.size.end.set, p.size.start.set,
                     p.size.curveFlags, p.size.curveWords);
        checkChannel("size2", p.size2.present, p.size2.end.set, p.size2.start.set,
                     p.size2.curveFlags, p.size2.curveWords);
        checkChannel("length", p.length.present, p.length.end.set,
                     p.length.start.set, p.length.curveFlags, p.length.curveWords);
        checkChannel("rgb", p.rgb.present, p.rgb.end.set, p.rgb.start.set,
                     p.rgb.curveFlags, p.rgb.curveWords);

        // Wind: tot in beiden Zweigen.
        //
        // Der Singleplayer prueft FX_AFFECTED_BY_WIND nirgends. Der
        // Multiplayer liest das Wort im Parser, aber der Block, der es
        // auswerten wuerde, ist auskommentiert — in codemp/client/
        // FxScheduler.cpp Zeile 1345, mit Ravens eigenem Kuerzel davor:
        //
        //     if ( fx->mSpawnFlags & FX_AFFECTED_BY_WIND )
        //     {
        //     /*rjr    vec3_t wind;
        //             CL_GetWindVector( wind );
        //             VectorMA( vel, fx->mWindModifier.GetVal() * 0.01f,
        //                       wind, vel );
        //     */
        //     }
        //
        // CL_GetWindVector kommt im ganzen Quelltext genau einmal vor:
        // in dieser auskommentierten Zeile. Die Funktion ist nie geschrieben
        // worden. Es gibt nur R_GetWindVector im Renderer, und das treibt
        // Regen und Grasbueschel, nicht Effekte.
        //
        // mWindModifier wird ebenfalls nur dort gelesen. Wer "wind" in eine
        // .efx-Datei schreibt, bekommt also in keinem Spielmodus etwas — der
        // alte Editor zeigt dafuer trotzdem einen Windpfeil und ein Haekchen.
        if ((p.spawnFlags & kSpawnAffectedByWind) || p.windModifier.set) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VWindDead), i18n::Str::VWindDead);
        }

        // "end" ohne Kurvenart bleibt wirkungslos.
        //
        // Das ist der haeufigste Stolperstein am Format, und man sieht ihn der
        // Datei nicht an: der Anteil beginnt bei 1.0 und wird nur veraendert,
        // wenn eine Kurvenart gesetzt ist. Am Ende steht
        // start*perc + end*(1-perc) — ohne Flag also durchgehend start.
        //
        // Wer also einen Farbverlauf schreibt und "linear" vergisst, bekommt
        // die Startfarbe und sucht den Fehler beim Shader.
        {
            struct ChannelCheck { const char* name; const Channel& channel; };
            const ChannelCheck channels[] = {
                {"size", p.size}, {"size2", p.size2}, {"length", p.length},
                {"alpha", p.alpha},
            };
            for (const auto& entry : channels) {
                if (!entry.channel.present) continue;
                const bool hasCurve = entry.channel.curveFlags != 0;
                const bool endDiffers =
                    entry.channel.end.set &&
                    (entry.channel.end.min != entry.channel.start.min ||
                     entry.channel.end.max != entry.channel.start.max);
                // Diese Bedingung wird weiter oben schon geprueft (VNoCurve).
                //
                // Beide Regeln waren richtig und pruefen genau dasselbe: ein
                // `end`, das von `start` abweicht, ohne Uebergangsart. Beim
                // Anwender stand deshalb JEDE dieser Warnungen doppelt im
                // Meldungsfenster, in zwei verschiedenen Formulierungen — was
                // aussieht, als seien es zwei verschiedene Probleme.
                (void)endDiffers;
                (void)hasCurve;
            }
            const bool rgbEndDiffers =
                p.rgb.end.set &&
                (p.rgb.end.min[0] != p.rgb.start.min[0] ||
                 p.rgb.end.min[1] != p.rgb.start.min[1] ||
                 p.rgb.end.min[2] != p.rgb.start.min[2]);
            if (p.rgb.present && p.rgb.curveFlags == 0 && rgbEndDiffers) {
                add(out, Severity::Warning, where + ": " + message(i18n::Str::VRgbEndNoCurve), i18n::Str::VRgbEndNoCurve);
            }
        }

        // Dialekt: welche gesetzten Flags kennt der Zielzweig nicht?
        auto checkDialect = [&](uint32_t bits, const std::vector<FlagName>& table,
                                const char* kind) {
            for (const auto& entry : table) {
                if (entry.bits == 0 || (bits & entry.bits) != entry.bits) continue;
                if (availableIn(entry.dialect, target)) continue;
                // Zwei Faelle: schreibt man fuer beide Zweige, wird das Flag
                // im anderen stillschweigend ueberlesen; schreibt man fuer
                // einen, ist es schlicht der falsche.
                const std::string text =
                    target == Dialect::Both
                        ? message(i18n::Str::VFlagOnlyIn, kind, entry.name,
                                  dialectName(entry.dialect))
                        : message(i18n::Str::VFlagWrongBranch, kind, entry.name,
                                  dialectName(entry.dialect), dialectName(target));
                add(out, Severity::Warning, where + ": " + text);
            }
        };
        checkDialect(p.flags, flagNames(), "Flag");
        checkDialect(p.spawnFlags, spawnFlagNames(), "spawnFlag");

        if ((p.spawnFlags & kSpawnSoundLessAttenuation) &&
            p.type != PrimitiveType::Sound &&
            availableIn(Dialect::SP, target)) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VLessAttenuation), i18n::Str::VLessAttenuation);
        }

        // materialImpact ist ein reines MP-Feld.
        if (p.materialImpactSet && !availableIn(Dialect::MP, target)) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VMaterialImpact), i18n::Str::VMaterialImpact);
        }

        // Die drei MP-Flags auf Bit 0x00010000 gelten je nur fuer genau einen
        // Primitivtyp. Raven schreibt an der Stelle selbst ins Quelltext, dass
        // ein Cylinder damit Unfug ergibt — dasselbe Bit ist dort size2 linear.
        if (p.flags & kFlagPaperPhysics) {
            bool fits = p.type == PrimitiveType::Emitter ||
                        p.type == PrimitiveType::ScreenFlash ||
                        p.type == PrimitiveType::Particle ||
                        p.type == PrimitiveType::OrientedParticle;
            if (p.type == PrimitiveType::Cylinder) {
                add(out, Severity::Error, where + ": " + message(i18n::Str::VSize2Collision), i18n::Str::VSize2Collision);
            } else if (!fits) {
                add(out, Severity::Warning, where + ": " + message(i18n::Str::VFlagWrongType), i18n::Str::VFlagWrongType);
            }
        }

        // Ghoul2-Flags teilen sich Bits mit den Size2-Kurvenflags.
        if ((p.flags & (kFlagGhoul2Trace | kFlagGhoul2Decals)) &&
            p.type == PrimitiveType::Cylinder) {
            add(out, Severity::Error,
                where + ": " + message(i18n::Str::VGhoul2Collision), i18n::Str::VGhoul2Collision);
        }

        if ((p.flags & kFlagEmitFx) && p.emitFx.empty()) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VEmitFxEmpty), i18n::Str::VEmitFxEmpty);
        }
        if ((p.flags & kFlagImpactRunsFx) && p.impactFx.empty()) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VImpactFxEmpty), i18n::Str::VImpactFxEmpty);
        }
        if ((p.flags & kFlagDeathRunsFx) && p.deathFx.empty()) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VDeathFxEmpty), i18n::Str::VDeathFxEmpty);
        }
        if ((p.flags & kFlagKillOnImpact) && !(p.flags & kFlagApplyPhysics)) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VImpactKills), i18n::Str::VImpactKills);
        }
        if ((p.flags & kFlagAttachedModel) && p.models.empty() &&
            p.type != PrimitiveType::Electricity) {
            add(out, Severity::Warning, where + ": " + message(i18n::Str::VModelEmpty), i18n::Str::VModelEmpty);
        }

        // Beim Electricity bedeuten drei Flags etwas voellig anderes.
        //
        //   FX_TAPER  0x01000000 == FX_ATTACHED_MODEL (useModel)
        //   FX_BRANCH 0x02000000 == FX_APPLY_PHYSICS  (usePhysics)
        //   FX_GROW   0x04000000 == FX_USE_BBOX       (useBBox)
        //
        // Der Parser kennt keine Schluessel namens taper, branch oder grow —
        // in der Datei stehen sie unter den anderen Namen. Wer das nicht
        // weiss, haelt sie fuer versehentlich gesetzt und loescht sie.
        if (p.type == PrimitiveType::Electricity) {
            std::string meaning;
            if (p.flags & kFlagElectricityTaper) meaning += " useModel=taper";
            if (p.flags & kFlagElectricityBranch) meaning += " usePhysics=branch";
            if (p.flags & kFlagElectricityGrow) meaning += " useBBox=grow";
            if (!meaning.empty()) {
                add(out, Severity::Info, where + ": " + message(i18n::Str::VElectricityFlags, meaning.c_str()), i18n::Str::VElectricityFlags);
            }
        }
    }

    return out;
}

}  // namespace efx

namespace efx {

std::vector<Diagnostic> validateShaderNames(
    const Effect& effect, const std::function<bool(const std::string&)>& exists) {
    std::vector<Diagnostic> out;
    if (!exists) return out;

    // Nur die eine Frage: gibt es zu diesem Namen ueberhaupt etwas?
    //
    // Die andere Pruefung — braucht der Shader einen Alphawert — bleibt bei
    // validateAgainstShaders: dafuer muss man die Mischung kennen, und die
    // steht nur in der zerlegten Shaderdatei.
    //
    // Ein fehlender Name ist eine WARNUNG, kein Fehler: die Engine zeichnet
    // dann einen Ersatz, und ein Effekt kann absichtlich auf etwas verweisen,
    // das erst eine andere Mod mitbringt.
    for (size_t i = 0; i < effect.primitives.size(); ++i) {
        const Primitive& p = effect.primitives[i];
        const std::string where =
            std::string(typeName(p.type)) + " #" + std::to_string(i + 1) +
            (p.name.empty() ? std::string{} : " (\"" + p.name + "\")");

        for (const auto& name : p.shaders) {
            if (name.empty() || exists(name)) continue;
            out.push_back({Severity::Warning, static_cast<int>(i + 1),
                           where + ": " +
                               message(i18n::Str::VShaderMissing, name.c_str())});
        }
    }
    return out;
}

std::vector<Diagnostic> validateAgainstShaders(const Effect& effect,
                                               const shader::Library& library) {
    std::vector<Diagnostic> out;

    for (size_t i = 0; i < effect.primitives.size(); ++i) {
        const Primitive& p = effect.primitives[i];
        const std::string where =
            std::string(typeName(p.type)) + " #" + std::to_string(i + 1) +
            (p.name.empty() ? std::string{} : " (\"" + p.name + "\")");

        for (const auto& name : p.shaders) {
            const shader::Shader* s = library.find(name);
            if (!s) {
                // Kein Eintrag heisst nicht zwangslaeufig Fehler: die Engine
                // baut dann einen Ersatzshader aus einer gleichnamigen
                // Bilddatei. Ohne Zugriff auf das Verzeichnis koennen wir das
                // hier nicht unterscheiden.
                out.push_back({Severity::Info, 0,
                               where + ": " +
                                   message(i18n::Str::VShaderMissing, name.c_str())});
                continue;
            }

            // Der Fall aus Ravens Handbuch, Abschnitt "If You Don't See
            // Anything": ein Shader mit GL_SRC_ALPHA braucht einen Alphawert.
            // Ohne useAlpha faellt der auf null und man sieht nichts.
            if (s->needsAlpha() && !(p.flags & kFlagUseAlpha)) {
                bool hasAlpha = p.alpha.present &&
                                (p.alpha.start.set || p.alpha.end.set);
                if (!hasAlpha) {
                    out.push_back(
                        {Severity::Warning, 0,
                         where + ": \"" + name +
                             "\" blendet ueber Alpha, aber die Primitive setzt "
                             "weder useAlpha noch einen alpha-Block. Sie bleibt "
                             "unsichtbar."});
                }
            }

            // Umgekehrt: ein rgb-Block wirkt nur, wenn der Shader die
            // Vertexfarbe ueberhaupt heranzieht.
            if (p.rgb.present && !s->usesVertexColor()) {
                out.push_back({Severity::Info, 0,
                               where + ": " +
                                   message(i18n::Str::VNoRgbGen, name.c_str())});
            }

            if (p.type == PrimitiveType::Decal && s->polygonOffset == false) {
                out.push_back({Severity::Info, 0,
                               where + ": " +
                                   message(i18n::Str::VNoPolygonOffset, name.c_str())});
            }
        }
    }

    return out;
}

}  // namespace efx
