#include "efx/particles.h"

#include <algorithm>
#include <cmath>

namespace efx::particles {
namespace {

constexpr float kPi = 3.14159265358979323846f;

uint32_t packRgba(float r, float g, float b, float a) {
    auto clamp255 = [](float v) {
        const int i = static_cast<int>(v * 255.0f + 0.5f);
        return i < 0 ? 0 : (i > 255 ? 255 : i);
    };
    return scene::rgba(clamp255(r), clamp255(g), clamp255(b), clamp255(a));
}

// Die Kurvenflags eines Kanals auf die normierte Form bringen.
//
// Jeder Kanal benutzt andere Bits fuer dieselben fuenf Arten — size hat sie
// auf 0..3, rgb woanders. Der Auswerter rechnet mit einer Form, also wird
// hier umgesetzt. Die Reihenfolge ist bei allen dieselbe, deshalb reicht ein
// Herunterschieben.
uint32_t normaliseCurveFlags(int rawFlags) {
    uint32_t flags = 0;
    if (rawFlags & kCurveLinear) flags |= curve::kLinear;
    if (rawFlags & kCurveNonLinear) flags |= curve::kNonLinear;
    if (rawFlags & kCurveWave) flags |= curve::kWave;
    if (rawFlags & kCurveRandom) flags |= curve::kRandom;
    return flags;
}

curve::Curve makeCurve(const Channel& channel, sim::Random& random) {
    // Ein Wert, der nicht in der Datei steht, ist EINS — nicht null.
    //
    // Fundstelle: der Erzeuger von CPrimitiveTemplate in FxTemplate.cpp setzt
    // ausnahmslos alle Kurvenenden auf 1.0:
    //
    //     mAlphaStart.SetRange( 1.0f, 1.0f );  mAlphaEnd.SetRange( 1.0f, 1.0f );
    //     mSizeStart/mSizeEnd, mSize2..., mLength..., mRed/Green/Blue...
    //
    // Das ist kein Randfall, sondern der Normalfall. Ein Block wie
    //
    //     alpha { end 0   parm 80   flags linear nonlinear }
    //
    // steht so in fast jeder Raven-Datei und heisst: von voll auf null
    // ausblenden. Mit null als Voreinstellung heisst er: von null auf null —
    // der Effekt ist unsichtbar.
    //
    // Aufgefallen ist das erst, als das Ausblenden richtig in rgb wanderte:
    // vorher landete die Null im Alphakanal, den additive Mischung gar nicht
    // ausliest, und blieb deshalb folgenlos. Zwei Fehler, die sich gegenseitig
    // verdeckt haben.
    //
    // Das gilt fuer `end` genauso wie fuer `start`. Hier stand
    //
    //     out.end = channel.end.set ? random.pick(channel.end) : out.start;
    //
    // — ein fehlendes Ende war der Startwert. Bei `size { start 10 flags
    // linear }` blieb das Teilchen damit 10 gross; im Spiel schrumpft es auf
    // 1, weil FxScheduler.cpp `mSizeEnd.GetVal()` unveraendert weitergibt und
    // mSizeEnd im Erzeuger auf 1.0 steht. In den ausgelieferten Dateien steht
    // so ein Block ueber 150-mal (size, rgb, length, alpha).
    curve::Curve out;
    out.start = kDefaultCurveValue;
    out.end = kDefaultCurveValue;
    if (!channel.present) return out;
    out.start = channel.start.set ? random.pick(channel.start) : kDefaultCurveValue;
    out.end = channel.end.set ? random.pick(channel.end) : kDefaultCurveValue;
    out.parm = random.pick(channel.parm);
    out.flags = normaliseCurveFlags(channel.curveFlags);
    return out;
}

// Ein Wert aus der Datei, oder die Voreinstellung der Engine, wenn er fehlt.
float pickOr(const Range& value, float fallback, sim::Random& random) {
    return value.set ? random.pick(value) : fallback;
}

camera::Vec3 pickVec3(const Vec3Range& value, sim::Random& random) {
    if (!value.set) return {};
    if (!value.ranged) return {value.min[0], value.min[1], value.min[2]};
    return {random.range(std::min(value.min[0], value.max[0]),
                         std::max(value.min[0], value.max[0])),
            random.range(std::min(value.min[1], value.max[1]),
                         std::max(value.min[1], value.max[1])),
            random.range(std::min(value.min[2], value.max[2]),
                         std::max(value.min[2], value.max[2]))};
}

}  // namespace

// Passt noch ein Viereck in diese Geometrie?
//
// Vier Eckpunkte je Viereck, und der groesste ansprechbare Index ist 65535.
// Die Grenze von 4096 Teilchen je Segment schuetzt davor NICHT: fuenf
// Segmente mit demselben Shader landen in derselben Geometrie.
static bool hasRoomForQuad(const scene::Mesh& mesh) {
    return mesh.vertices.size() + 4 <= 65535;
}

camera::Vec3 Live::positionAt(float nowMs) const {
    if (hasPath) return sim::positionOnPath(path, nowMs - spawnMs);
    const float seconds = (nowMs - spawnMs) * 0.001f;
    return sim::positionAt(origin, velocity, acceleration, gravity, seconds);
}

void System::stop() {
    // Nur anhalten. Die Geometrie bleibt, damit man sie weiter anfahren kann.
    playing_ = false;
}

void System::clear() {
    live_.clear();
    durationMs_ = 0.0f;
    playing_ = false;
}

Axis rotatedAroundForward(const Axis& axis, float degrees) {
    // RotatePointAroundVector( ax[1], ax[0], axis[1], winkel )
    const float radians = degrees * 3.14159265358979323846f / 180.0f;
    const float c = std::cos(radians);
    const float sn = std::sin(radians);
    const camera::Vec3 n = camera::normalise(axis.forward);
    const camera::Vec3 v = axis.right;

    // Rodrigues: v*cos + (n x v)*sin + n*(n·v)*(1-cos)
    Axis out;
    out.forward = n;
    out.right = v * c + camera::cross(n, v) * sn + n * (camera::dot(n, v) * (1.0f - c));
    out.right = camera::normalise(out.right);
    // CrossProduct( ax[0], ax[1], ax[2] ) — in dieser Reihenfolge.
    out.up = camera::normalise(camera::cross(out.forward, out.right));
    return out;
}

camera::Vec3 pointOnSphere(float turns, float pitch, float radius, float height) {
    // x laeuft ueber 360 Grad, y NUR ueber 180 — das ist kein Versehen,
    // sondern die uebliche Kugelparametrisierung: y ist der Winkel vom Pol.
    const float x = turns * 2.0f * 3.14159265358979323846f;
    const float y = pitch * 3.14159265358979323846f;
    return {std::sin(x) * radius * std::sin(y),
            std::cos(x) * radius * std::sin(y),
            std::cos(y) * height};
}

camera::Vec3 pointOnCylinder(const Axis& axis, float radius, float height,
                             float alongFraction, float degrees) {
    camera::Vec3 pt = axis.right * radius;
    pt = pt + axis.forward * (alongFraction * 0.5f * height);

    const float radians = degrees * 3.14159265358979323846f / 180.0f;
    const float c = std::cos(radians);
    const float sn = std::sin(radians);
    const camera::Vec3 n = camera::normalise(axis.forward);
    return pt * c + camera::cross(n, pt) * sn +
           n * (camera::dot(n, pt) * (1.0f - c));
}

// Achsen aus einer Richtung — die Zylinderfassung.
//
// Die Engine baut sie beim Zylinder ANDERS als bei der Kugel. FxScheduler.cpp,
// im Zweig FX_ORG_ON_CYLINDER:
//
//     vec3_t up = {0,0,1};
//     VectorNormalize2( temp, ax[0] );
//     if ( ax[0][2] == 1.0f ) VectorSet( up, 0, 1, 0 );   // sonst parallel
//     CrossProduct( up, ax[0], ax[1] );
//     CrossProduct( ax[0], ax[1], ax[2] );
//
// Bei der Kugel steht dort `MakeNormalVectors`, eine ganz andere Rechnung.
// Wir haben beides mit derselben gemacht. Sichtbar wird das erst bei einem
// Kindeffekt, der nicht rund um seine Achse gleich aussieht — dann liegt er
// verdreht.
Axis axisFromCylinderDirection(const camera::Vec3& direction) {
    Axis out;
    const camera::Vec3 f = camera::normalise(direction);
    if (camera::length(f) < 1e-5f) return out;
    out.forward = f;

    // Der Vergleich auf genau 1.0 steht so in der Engine. Wir pruefen etwas
    // grosszuegiger: bei 0.9999 waere das Kreuzprodukt schon fast null, und
    // die Achsen wuerden unbrauchbar.
    camera::Vec3 up{0.0f, 0.0f, 1.0f};
    if (std::fabs(f.z) > 0.9999f) up = {0.0f, 1.0f, 0.0f};

    out.right = camera::normalise(camera::cross(up, f));
    out.up = camera::normalise(camera::cross(f, out.right));
    return out;
}

Axis axisFromDirection(const camera::Vec3& direction) {
    Axis out;
    const camera::Vec3 f = camera::normalise(direction);
    if (camera::length(f) < 1e-5f) return out;
    out.forward = f;

    // MakeNormalVectors in q_math.c — die Vertauschung sieht willkuerlich
    // aus, sorgt aber dafuer, dass der Hilfsvektor nie parallel zu `forward`
    // liegt:
    //     right[1] = -forward[0]; right[2] = forward[1]; right[0] = forward[2];
    camera::Vec3 right{f.z, -f.x, f.y};
    const float d = camera::dot(right, f);
    right = right - f * d;
    if (camera::length(right) < 1e-5f) return out;
    out.right = camera::normalise(right);
    // CrossProduct( right, forward, up )
    out.up = camera::normalise(camera::cross(out.right, out.forward));
    return out;
}

Axis axisFor(int orientation) {
    Axis axis;
    switch (orientation) {
        case 1:  // seitwaerts, auf der X-Achse
            axis.forward = {1.0f, 0.0f, 0.0f};
            axis.right = {0.0f, 1.0f, 0.0f};
            axis.up = {0.0f, 0.0f, 1.0f};
            break;
        case 2:  // nach unten
            axis.forward = {0.0f, 0.0f, -1.0f};
            axis.right = {1.0f, 0.0f, 0.0f};
            axis.up = {0.0f, -1.0f, 0.0f};
            break;
        default:  // nach oben
            axis.forward = {0.0f, 0.0f, 1.0f};
            axis.right = {1.0f, 0.0f, 0.0f};
            axis.up = {0.0f, 1.0f, 0.0f};
            break;
    }
    return axis;
}

void System::play(const Effect& effect, unsigned seed,
                  const std::vector<bool>& enabledMask, const Axis& axis,
                  EffectLoader loader, const std::vector<sim::Plane>& planes,
                  bool buildUpRepeats) {
    // clear() und nicht stop().
    //
    // Der Fehler, den das behebt: `stop()` haelt seit der Trennung von
    // "anhalten" und "wegwerfen" nur noch die Uhr an — die Geometrie bleibt
    // absichtlich stehen, damit man sie nach dem Anhalten weiter anfahren
    // kann.
    //
    // Genau diese Zeile stand aber auch hier, am Anfang von play(). Damit
    // raeumte das Ausloesen nicht mehr auf, und jeder Durchlauf legte NEUE
    // Teilchen zu den alten dazu. Bei einer Wiederholung alle 300 ms wuchs
    // der Bestand ohne Grenze: nach zehn Durchlaeufen das Zehnfache, nach
    // hundert das Hundertfache.
    //
    // Beim Anwender sah das so aus, als haette das Feuer die zehnfache
    // Dichte des Originals — und genau das war es auch.
    clear();
    sim::Random random(seed);
    const PlayContext context{&loader, &planes, axis};
    playInto(effect, random, enabledMask, context, 0, 0.0f, {});

    // `repeatDelay`: der Effekt wiederholt sich, OHNE dass der laufende
    // abgebrochen wird.
    //
    // Fundstelle `CFxScheduler::AddLoopedEffects` in FxScheduler.cpp:
    //
    //     if (mLoopedEffectArray[i].mNextTime < theFxHelper.mTime) {
    //         PlayEffect( ... );                 // noch einmal, ZUSAETZLICH
    //         mLoopedEffectArray[i].mNextTime = theFxHelper.mTime + mRepeatDelay;
    //
    // Deshalb wird aus einzelnen Flaemmchen ein stehendes Feuer: bei
    // t_small_fire.efx leben Teilchen 500 bis 1400 ms und werden alle 300 ms
    // nachgelegt — es ueberlagern sich bis zu fuenf Generationen.
    //
    // Nachgelegt wird in die VERGANGENHEIT, nicht in die Zukunft. Der Grund
    // ist die Wiederholung der Vorschau: legt man vorwaerts nach, sieht man
    // erst den Aufbau und beim Zuruecksetzen wieder eine leere Szene — es
    // pulsiert. Mit Generationen bei -d, -2d, ... ist der Bestand schon beim
    // ersten Bild eingeschwungen, und nach genau `repeatDelay` sieht er
    // wieder genauso aus. Damit laeuft die Vorschau nahtlos rund.
    const float delay = static_cast<float>(effect.repeatDelay);
    if (buildUpRepeats && delay >= 1.0f && !live_.empty()) {
        const float longest = durationMs_;
        // So viele Generationen, bis die aelteste gerade ausgestorben ist.
        // Gedeckelt, damit ein Effekt mit `repeatDelay 1` und langem Leben
        // nicht Zehntausende Teilchen erzeugt.
        constexpr int kMaxGenerations = 16;
        const int wanted = static_cast<int>(std::ceil(longest / delay));
        const int generations = wanted < kMaxGenerations ? wanted : kMaxGenerations;

        // Die Ausgangswerte wiederholen sich PERIODISCH — nicht identisch.
        //
        // Das ist der vierte Anlauf, und die drei davor erklaeren, warum es so
        // aussehen muss:
        //
        //   1. Bei jedem Umlauf alles neu planen: sprang dreimal je Sekunde.
        //   2. Waehrend der Wiedergabe nachlegen: die Dauer wuchs auf 56
        //      Sekunden, und Zurueckspulen zeigte ein leeres Bild.
        //   3. Alle Generationen mit demselben Wert: nahtlos, aber es
        //      wiederholte sich alle `repeatDelay` EXAKT. Beim Anwender sah
        //      man das sofort — „als wuerde es immer dieselbe Reihenfolge
        //      spielen". Ein Feuer, das im Sekundentakt dasselbe tut, ist kein
        //      Feuer.
        //
        // Die Bedingung fuer eine nahtlose Schleife ist NICHT, dass alle
        // Generationen gleich sind. Sie lautet:
        //
        //     seed(g) == seed(g - N)   fuer die Schleifenlaenge N * delay
        //
        // Mit anderen Worten: der Ausgangswert darf sich aendern, er muss sich
        // nur nach N Generationen wiederholen. Dann sieht der Bestand bei
        // t = N*delay wieder genau so aus wie bei t = 0 — und dazwischen liegen
        // N verschiedene Generationen.
        //
        // N so gewaehlt, dass die Schleife rund zwei Sekunden dauert: kurz
        // genug, um wenige Teilchen zu brauchen, lang genug, dass das Auge die
        // Wiederholung nicht bemerkt.
        constexpr float kLoopTargetMs = 2000.0f;
        int variety = static_cast<int>(std::ceil(kLoopTargetMs / delay));
        if (variety < 4) variety = 4;
        if (variety > 16) variety = 16;

        // Generationen von -variety (Zukunft) bis +generations (Vergangenheit).
        //
        // Die aus der Zukunft schliessen die Luecke am Ende des Fensters: ohne
        // sie fehlt dort die juengste Kohorte, und es wurde zum Schluss
        // duenner. Es braucht so viele davon, wie das Fenster lang ist.
        const auto seedFor = [seed, variety](int g) {
            const int mod = ((g % variety) + variety) % variety;
            // Generation 0 behaelt den Ausgangswert des Effekts — sie ist die
            // wirklich ausgeloeste, und ihr Strom entspricht dem von
            // `sim::Random(seed)`.
            if (mod == 0) return seed;
            return (seed ^ (static_cast<unsigned>(mod) * 2654435761u)) | 1u;
        };

        // Ab hier beginnen die zusaetzlichen Generationen. Alles davor ist
        // die eigentliche, ausgeloeste — nur die darf Klaenge und
        // Erschuetterungen tragen.
        const size_t firstExtra = live_.size();

        for (int g = -variety; g <= generations; ++g) {
            if (g == 0) continue;   // die steht schon
            sim::Random again(seedFor(g));
            playInto(effect, again, enabledMask, context, 0,
                     -delay * static_cast<float>(g), {});
        }

        // Die Dauer ist EINE Wiederholung.
        //
        // Danach ist das Bild wieder dasselbe — siehe oben. Ohne diese Zeile
        // liefe die Zeitleiste ueber das ganze Leben des Effekts, bei einem
        // Rauch mit 52 Sekunden Lebensdauer also minutenlang, obwohl sich
        // nach 300 ms nichts Neues mehr zeigt.
        // NUR wenn der Vorlauf die ganze Lebensdauer abdeckt.
        //
        // Die kurze Schleife setzt voraus, dass der Bestand eingeschwungen
        // ist — dafuer braucht es eine Generation je `repeatDelay` ueber das
        // gesamte Leben. Bei smoke_explosion.efx leben Teilchen 52 Sekunden
        // bei 300 ms Wiederholung: das waeren 206 Generationen und ueber
        // dreissigtausend Teilchen. Der Deckel bei 16 verhindert das zu Recht.
        //
        // Dann ist die Schleife aber NICHT nahtlos, und eine Zeitleiste ueber
        // 300 ms waere eine Luege — sie zeigte einen Sprung als Wiederholung.
        // In dem Fall bleibt die volle Dauer stehen: ein Rauch, der eine
        // Minute braucht, braucht eben eine Minute.
        // Die Schleife dauert N Wiederholungen, nicht eine.
        //
        // Nach N Generationen wiederholen sich die Ausgangswerte, also ist der
        // Bestand wieder derselbe. Eine kuerzere Zeitleiste waere eine Luege:
        // sie zeigte einen Sprung als Wiederholung.
        if (wanted <= kMaxGenerations) {
            durationMs_ = delay * static_cast<float>(variety);
        }

        // Die alte Begruendung, warum hier NICHT gekuerzt wurde:
        //
        // Das stand hier und war falsch: im Editor sprang die Zeitleiste beim
        // Umschalten auf Wiederholen von 62 Sekunden auf 300 Millisekunden —
        // neun Bilder statt zweitausend. Man sah nur noch den Anfang und
        // konnte den Rest nicht mehr anfahren.
        //
        // Die gekuerzte Dauer ist eine Frage der ANZEIGE, nicht der
        // Simulation: nur die Browserkacheln wollen ueber genau eine
        // Wiederholung laufen, damit sie nahtlos rundlaufen. Sie stellen ihre
        // Uhr selbst darauf ein.
        //
        // Der Vorlauf liegt in der VERGANGENHEIT — er darf nichts ausloesen,
        // was einmalig passiert.
        //
        // Ein Klang, der vor 4.8 Sekunden begonnen hat, ist vorbei. Ohne diese
        // Zeile trugen alle sechzehn Vorlaufgenerationen ihren Klang, und weil
        // ihr Zeitpunkt schon erreicht war, gingen sie beim Start ALLE
        // GLEICHZEITIG los. Beim Anwender war das ein Klang, der sich acht-
        // bis neunmal ueberlagerte — die Obergrenze des Tongeraets liegt bei
        // sechzehn gleichzeitigen Puffern, also hoerte es irgendwann von
        // selbst auf.
        //
        // Fuer die Kameraerschuetterung gilt dasselbe: ein Ruettler von
        // vorhin ist vorbei.
        // Die zusaetzlichen Generationen sind eine ANZEIGEHILFE, keine
        // weiteren Ausloesungen. Sie duerfen nichts hervorrufen, was einmalig
        // geschieht.
        //
        // Frueher stand hier `spawnMs < 0` — das traf die Generationen aus
        // der Vergangenheit, aber nicht die bei +d, die spaeter dazukam. Die
        // haette dann einen zweiten Klang ausgeloest.
        //
        // Der Fall aus dem Betrieb: smoke_explosion.efx hat ein Sound-Segment
        // und `repeatDelay 300`. Ohne diese Regel gingen sechzehn Klaenge
        // gleichzeitig los, bis die Puffergrenze des Tongeraets erreicht war.
        for (size_t i = firstExtra; i < live_.size(); ++i) {
            Live& item = live_[i];
            item.soundName.clear();
            // Die Kameraerschuetterung laesst sich nicht ebenso abschalten —
            // sie steckt im Typ. Ihre Staerke auf null zu setzen genuegt.
            if (item.type == PrimitiveType::CameraShake) item.deathMs = item.spawnMs;
        }
    }

    playing_ = !live_.empty();
}

void System::playInto(const Effect& effect, sim::Random& random,
                      const std::vector<bool>& enabledMask,
                      const PlayContext& context, int depth, float atMs,
                      const camera::Vec3& atPosition) {
    if (depth >= kMaxEffectDepth) return;

    // Bequeme Namen fuer den Rumpf. Die Zeiger im Kontext sind nie null —
    // `play` setzt sie, und `playInto` ist privat.
    const EffectLoader& loader = *context.loader;
    const std::vector<sim::Plane>& planes = *context.planes;
    const Axis& axis = context.axis;

    for (const auto& spawn : sim::schedule(effect, random, enabledMask)) {
        const Primitive& p = effect.primitives[spawn.primitiveIndex];

        // Die Flags, wie die Engine sie sieht: mit den Bits, die der Parser
        // beim Lesen von impactfx, deathfx, emitfx und models selbst setzt
        // (efx::effectiveFlags). `p.flags` allein ist nur, was in der Zeile
        // `flags` steht.
        const uint32_t flags = effectiveFlags(p);

        Live item;
        item.type = p.type;
        item.primitiveIndex = spawn.primitiveIndex;
        item.useAlpha = (p.flags & kFlagUseAlpha) != 0;
        item.spawnMs = spawn.timeMs;

        // Lebensdauer. Ohne Angabe eine Zehntelsekunde — die Engine hat dort
        // 50 ms stehen; ohne irgendeinen Wert waere die Primitive sofort
        // wieder weg und man saehe nie etwas.
        const float life = p.life.set ? random.pick(p.life) : 50.0f;
        item.deathMs = item.spawnMs + std::max(life, 1.0f);

        // Die Achsen dieses Teilchens. Sie koennen von denen des Effekts
        // abweichen — `randRotAroundFwd` dreht sie, `axisFromSphere`
        // ersetzt sie ganz.
        Axis own = axis;
        if ((p.spawnFlags & kSpawnRandRotAroundFwd) != 0) {
            own = rotatedAroundForward(own, random.next() * 360.0f);
        }
        const auto alongOwn = [&](const camera::Vec3& v) {
            return own.forward * v.x + own.right * v.y + own.up * v.z;
        };

        // Ursprung. Mit `cheapOriginCalculation` roh, sonst ueber die Achsen
        // gedreht — und IMMER wird der Effektursprung addiert:
        //
        //     // We always add our calculated offset to the passed in origin...
        //     VectorAdd( org, origin, org );
        const camera::Vec3 rawOrigin = pickVec3(p.origin, random);
        item.origin = (p.spawnFlags & kSpawnCheapOrgCalc) != 0
                          ? rawOrigin + atPosition
                          : alongOwn(rawOrigin) + atPosition;

        // Ein Punkt auf Kugel oder Zylinder kommt oben drauf. Beides
        // schliesst sich aus — die Engine prueft `else if`.
        //
        // Ohne radius/height sind beide 10, nicht 0: der Erzeuger von
        // CPrimitiveTemplate setzt mRadius und mHeight auf 10, und
        // FxScheduler.cpp liest `fx->mRadius.GetVal()` ohne Pruefung. Mit 0
        // fiel jedes `orgOnSphere` ohne radius auf einen Punkt zusammen
        // (70 Primitive in 42 ausgelieferten Dateien).
        if ((p.spawnFlags & kSpawnOrgOnSphere) != 0) {
            const camera::Vec3 onSphere = pointOnSphere(
                random.next(), random.next(),
                pickOr(p.radius, kDefaultRadius, random),
                pickOr(p.height, kDefaultHeight, random));
            item.origin = item.origin + onSphere;
            if ((p.spawnFlags & kSpawnAxisFromSphere) != 0) {
                own = axisFromDirection(onSphere);
            }
        } else if ((p.spawnFlags & kSpawnOrgOnCylinder) != 0) {
            const camera::Vec3 onCylinder = pointOnCylinder(
                own, pickOr(p.radius, kDefaultRadius, random),
                pickOr(p.height, kDefaultHeight, random),
                random.range(-1.0f, 1.0f), random.next() * 360.0f);
            item.origin = item.origin + onCylinder;
            if ((p.spawnFlags & kSpawnAxisFromSphere) != 0) {
                // Beim Zylinder eine andere Konstruktion als bei der Kugel —
                // siehe axisFromCylinderDirection.
                own = axisFromCylinderDirection(onCylinder);
            }
        }
        // `origin2` bezieht sich auf den EFFEKTURSPRUNG, nicht auf das
        // einzelne Teilchen — und mit `cheapOrigin2Calculation` ist es sogar
        // ein absoluter Punkt.
        //
        // FxScheduler.cpp, im Block "Line type primitives work with an
        // origin2":
        //
        //     if ( mSpawnFlags & FX_CHEAP_ORG2_CALC || flags & FX_RELATIVE ) {
        //         VectorSet( org2, mOrigin2X.GetVal(), ... );   // roh
        //     } else {
        //         VectorScale( ax[0], mOrigin2X.GetVal(), org2 );
        //         VectorMA( org2, mOrigin2Y.GetVal(), ax[1], org2 );
        //         VectorMA( org2, mOrigin2Z.GetVal(), ax[2], org2 );
        //         VectorAdd( org2, origin, org2 );              // EFFEKT-Ursprung
        //     }
        //
        // Zweimal falsch bei uns: wir haben immer ueber die Achsen gedreht
        // (auch mit dem Flag), und wir haben den Endpunkt an das TEILCHEN
        // gehaengt statt an den Effekt. Bei einer Linie mit einer Spanne im
        // Ursprung wandert damit auch ihr Endpunkt mit — im Spiel bleibt er
        // stehen.
        // Die Vorwaertsachse merken: Cylinder, Decal und OrientedParticle
        // richten sich daran aus.
        item.forward = own.forward;

        const camera::Vec3 raw2 = pickVec3(p.origin2, random);
        item.origin2 = (p.spawnFlags & kSpawnCheapOrg2Calc) != 0
                           ? raw2
                           : alongOwn(raw2) + atPosition;
        item.velocity = alongOwn(pickVec3(p.velocity, random));
        item.acceleration = alongOwn(pickVec3(p.acceleration, random));
        item.gravity = p.gravity.set ? random.pick(p.gravity) : 0.0f;

        item.size = makeCurve(p.size, random);
        item.size2 = makeCurve(p.size2, random);
        item.length = makeCurve(p.length, random);
        item.alpha = makeCurve(p.alpha, random);

        // Farbe: drei Kanaele mit gemeinsamen Flags und gemeinsamem Parameter.
        for (int c = 0; c < 3; ++c) {
            item.rgb[c].flags = normaliseCurveFlags(p.rgb.curveFlags);
            item.rgb[c].parm = random.pick(p.rgb.parm);
        }
        if (p.rgb.present) {
            // `rgbComponentInterpolation`: EIN Zufallswert fuer alle drei
            // Kanaele, und zwar fuer Anfang UND Ende.
            //
            // FxScheduler.cpp:
            //
            //     if ( fx->mSpawnFlags & FX_RGB_COMPONENT_INTERP ) {
            //         float perc = Q_flrand(0.0f, 1.0f);
            //         VectorSet( sRGB, mRedStart.GetVal(perc),
            //                    mGreenStart.GetVal(perc), mBlueStart.GetVal(perc) );
            //         VectorSet( eRGB, mRedEnd.GetVal(perc), ... );
            //     } else {
            //         VectorSet( sRGB, mRedStart.GetVal(), ... );   // je Kanal
            //     }
            //
            // und GetVal(perc) ist die gerade Mischung: min + (max-min)*perc.
            //
            // Der Unterschied ist sichtbar: ohne das Flag wuerfelt jeder Kanal
            // fuer sich, und aus einer Spanne von Orange nach Gelb werden auch
            // gruenliche und rosa Toene. Mit dem Flag bleibt die Farbe auf der
            // Geraden zwischen den beiden — genau das wollte der Autor.
            //
            // Der Flag wurde bei uns zwar gelesen, aber nie benutzt. In
            // t_small_fire.efx steht er bei drei von vier Segmenten.
            const bool sameForAll =
                (p.spawnFlags & kSpawnRgbComponentInterp) != 0;
            const float shared = sameForAll ? random.next() : 0.0f;

            const auto pickColour = [&](const Vec3Range& range,
                                        const camera::Vec3& fallback) {
                if (!range.set) return fallback;
                if (!sameForAll) return pickVec3(range, random);
                return camera::Vec3{
                    range.min[0] + (range.max[0] - range.min[0]) * shared,
                    range.min[1] + (range.max[1] - range.min[1]) * shared,
                    range.min[2] + (range.max[2] - range.min[2]) * shared};
            };

            // Beide Enden fehlen als 1 1 1, nicht das Ende als Startfarbe —
            // mRedEnd/mGreenEnd/mBlueEnd stehen im Erzeuger auf 1.0 (siehe
            // makeCurve). `rgb { start 1 0 0 flags linear }` blendet im
            // Spiel von Rot nach Weiss.
            const camera::Vec3 white{kDefaultCurveValue, kDefaultCurveValue,
                                     kDefaultCurveValue};
            const camera::Vec3 start = pickColour(p.rgb.start, white);
            const camera::Vec3 end = pickColour(p.rgb.end, white);
            item.rgb[0].start = start.x; item.rgb[0].end = end.x;
            item.rgb[1].start = start.y; item.rgb[1].end = end.y;
            item.rgb[2].start = start.z; item.rgb[2].end = end.z;
        } else {
            // Ohne rgb-Block weiss — so zeichnet die Engine auch.
            for (int c = 0; c < 3; ++c) {
                item.rgb[c].start = 1.0f;
                item.rgb[c].end = 1.0f;
            }
        }

        // Einen Shader auswaehlen. Mehrere in der Liste heisst: die Engine
        // wuerfelt beim Erzeugen einen aus, und der bleibt.
        if (p.type == PrimitiveType::Sound && !p.sounds.empty()) {
            const size_t at = static_cast<size_t>(
                random.next() * static_cast<float>(p.sounds.size()));
            item.soundName = p.sounds[std::min(at, p.sounds.size() - 1)];
        }

        if (!p.shaders.empty()) {
            const size_t pick = static_cast<size_t>(
                random.next() * static_cast<float>(p.shaders.size()));
            item.shader = p.shaders[std::min(pick, p.shaders.size() - 1)];
        }

        // Fuer den Blitz: Unruhe und ein eigener Ausgangswert je Partikel,
        // damit zwei Blitze nebeneinander nicht deckungsgleich zappeln.
        //
        // Die Unruhe steht im Feld `elasticity` — dasselbe Feld, das bei
        // anderen Typen den Abpraller beschreibt und beim Kamerawackeln die
        // Staerke. In der Datei heisst es je nach Typ "bounce", "intensity"
        // oder gar nichts; die Engine liest ueberall `mElasticity.GetVal()`.
        //
        // Das ist keine Schoenheit, aber es ist, was dasteht.
        item.chaos = p.elasticity.set ? random.pick(p.elasticity) : 1.0f;

        // Die Kameraerschuetterung holt sich ihre Werte aus denselben Feldern
        // — `elasticity` als Staerke, `radius` als Reichweite.
        if (p.type == PrimitiveType::CameraShake) {
            item.shakeIntensity = p.elasticity.set ? random.pick(p.elasticity) : 0.0f;
            // Ohne radius 10 wie in der Engine (mRadius im Erzeuger).
            item.shakeRadius = pickOr(p.radius, kDefaultRadius, random);
        }
        item.seed = static_cast<unsigned>(random.next() * 4000000000.0f) | 1u;

        item.rotation = p.rotation.set ? random.pick(p.rotation) : 0.0f;
        item.rotationDelta = p.rotationDelta.set ? random.pick(p.rotationDelta) : 0.0f;

        // Je Kurve ein Zufallswert, einmal gezogen. Bei jedem Bild neu zu
        // wuerfeln ergaebe Flimmern statt einer gedaempften Kurve.
        item.randomSize = random.next();
        item.randomAlpha = random.next();
        item.randomRgb = random.next();
        item.randomLength = random.next();

        // Untergeordnete Effekte starten mit dem Versatz ihres Erzeugers.
        //
        // Nur die ZEIT wird hier verschoben. Der Ort steckt schon im
        // Ursprung: seit die Platzierungs-Flags nachgebaut sind, addiert die
        // Ursprungsrechnung `atPosition` selbst — genau wie die Engine, die
        // es einmal tut und dazuschreibt:
        //
        //     // We always add our calculated offset to the passed in origin...
        //     VectorAdd( org, origin, org );
        //
        // Stand es auch hier, wurde der Ort doppelt gezaehlt: ein Kindeffekt
        // erschien in doppelter Entfernung vom Erzeuger.
        item.spawnMs += atMs;
        item.deathMs += atMs;

        // Die Parameter einmal umrechnen statt in jedem Bild — und erst
        // JETZT, mit der verschobenen Startzeit.
        //
        // Bei nonlinear und clamp ist der Parameter ein ABSOLUTER Zeitpunkt:
        // CParticle::Init rechnet `mSizeParm = sizeParm * 0.01f * killTime +
        // theFxHelper.mTime`, also mit der Uhr, zu der DIESES Teilchen
        // entsteht. curve::bias vergleicht spaeter mit der absoluten Uhr.
        //
        // Stand die Umrechnung vor dem Versatz, rechnete sie mit der lokalen
        // Zeit des Kindeffekts. Ein Kind, das 2000 ms nach dem Start kam,
        // hatte seinen Ausblendpunkt 2000 ms in der Vergangenheit und war vom
        // ersten Bild an dunkel. Dasselbe traf jede Wiederholungsgeneration
        // (`repeatDelay`), die mit einem Versatz von -d, -2d, ... startet.
        item.sizeParm = curve::resolveParm(item.size, item.spawnMs, life);
        item.size2Parm = curve::resolveParm(item.size2, item.spawnMs, life);
        item.lengthParm = curve::resolveParm(item.length, item.spawnMs, life);
        item.alphaParm = curve::resolveParm(item.alpha, item.spawnMs, life);
        item.rgbParm = curve::resolveParm(item.rgb[0], item.spawnMs, life);

        // Physik: die Bahn einmal in Abschnitte zerlegen.
        //
        // NACH dem Versatz. Die Bahn enthaelt absolute Punkte; wuerde
        // sie vorher gebaut, laege ein untergeordneter Effekt mit Physik
        // an der falschen Stelle im Raum — und man saehe es erst bei
        // einem Emitter, der sich bewegt.
        //
        // Nur bei gesetztem Flag — die Engine macht dasselbe, und eine
        // Kollisionsrechnung fuer jeden Funken waere teuer, ohne dass es
        // jemand verlangt hat.
        //
        // `flags`, nicht `p.flags`: eine impactfx-Liste schaltet die Physik
        // im Spiel selbst ein (ParseImpactFxStrings setzt FX_APPLY_PHYSICS).
        if ((flags & kFlagApplyPhysics) != 0 && !planes.empty()) {
            const float elasticity =
                p.elasticity.set ? random.pick(p.elasticity) : 0.0f;
            item.path = sim::buildPath(item.origin, item.velocity,
                                       item.acceleration, item.gravity,
                                       item.deathMs - item.spawnMs, planes,
                                       elasticity,
                                       (flags & kFlagKillOnImpact) != 0);
            item.hasPath = true;
            // killOnImpact verkuerzt die Lebensdauer bis zum Aufprall.
            if (item.path.killed) {
                item.deathMs = item.spawnMs + item.path.killedMs;
            }
        }


        durationMs_ = std::max(durationMs_, item.deathMs);

        // FxRunner startet einen anderen Effekt, sobald er selbst erscheint.
        // Er zeichnet nichts — deshalb wird er hier verarbeitet und nicht in
        // der Liste behalten.
        // playFx ist eine Liste: stehen mehrere darin, wuerfelt die Engine
        // beim Ausloesen einen aus — wie bei den Shadern.
        auto pickEffect = [&](const std::vector<std::string>& names) {
            const size_t at = static_cast<size_t>(
                random.next() * static_cast<float>(names.size()));
            return names[std::min(at, names.size() - 1)];
        };

        if (p.type == PrimitiveType::FxRunner) {
            if (!p.playFx.empty() && loader) {
                if (const Effect* child = loader(pickEffect(p.playFx))) {
                    ++startedEffects_;
                    playInto(*child, random, {}, context, depth + 1, item.spawnMs,
                             item.origin);
                } else {
                    ++missingEffects_;
                }
            }
            continue;
        }

        // Emitter sendet unterwegs aus — nach zurueckgelegter STRECKE, nicht
        // nach Zeit. Die Engine vergleicht in CEmitter::UpdateEmitter das
        // Quadrat des Abstands zum letzten Aussenden mit
        //     step = density + zufall(-1..1) * variance
        // und setzt den Merkpunkt danach neu.
        //
        // Ausgesendet wird die `emitfx`-Liste, nicht `playfx`. FxScheduler.cpp
        // reicht dem Emitter `fx->mEmitterFxHandles.GetHandle()` mit, und
        // CEmitter::UpdateEmitter fragt `mFlags & FX_EMIT_FX` — das Bit setzt
        // ParseEmitterFxStrings selbst, sobald die Liste da ist. `playfx`
        // liest nur der FxRunner. Hier stand `p.playFx`: kein einziger der 47
        // Emitter in den ausgelieferten Dateien sendete in der Vorschau aus,
        // denn Raven schreibt ausnahmslos `emitfx`.
        //
        // Ohne density und variance gelten 10 und 1 (Erzeuger von
        // CPrimitiveTemplate) — vorher 0, und ein Emitter ohne density
        // sendete gar nichts.
        if (p.type == PrimitiveType::Emitter && (flags & kFlagEmitFx) != 0 &&
            !p.emitFx.empty() && loader) {
            const float density = pickOr(p.density, kDefaultDensity, random);
            const float variance = pickOr(p.variance, kDefaultVariance, random);
            if (density > 0.0f) {
                const Effect* child = loader(pickEffect(p.emitFx));
                if (!child) {
                    ++missingEffects_;
                } else {
                    // Die Bahn abschreiten und an jedem Schritt aussenden.
                    // Feste Zeitschritte statt der Bildrate der Engine: sonst
                    // haengt das Ergebnis davon ab, wie schnell der Rechner
                    // ist, und die Vorschau sieht auf jedem Rechner anders aus.
                    constexpr float kStepMs = 10.0f;
                    camera::Vec3 last = item.positionAt(item.spawnMs);
                    float step = density + random.range(-1.0f, 1.0f) * variance;
                    int emitted = 0;
                    for (float t = item.spawnMs; t < item.deathMs; t += kStepMs) {
                        const camera::Vec3 at = item.positionAt(t);
                        if (camera::length(at - last) >= std::fabs(step)) {
                            playInto(*child, random, {}, context, depth + 1, t, at);
                            ++startedEffects_;
                            last = at;
                            step = density + random.range(-1.0f, 1.0f) * variance;
                            // Eine Obergrenze: ein Emitter mit density 0.1 und
                            // langer Lebensdauer wuerde sonst Tausende
                            // Effekte starten und die Vorschau anhalten.
                            if (++emitted >= 256) break;
                        }
                    }
                }
            }
        }

        // impactfx: bei jedem Aufprall einen Effekt starten.
        //
        // Die Achse ist die FLAECHENNORMALE, nicht zufaellig — anders als
        // beim deathFx. Das ist auch sinnvoll: ein Einschlag zeigt von der
        // Wand weg, und die Engine reicht `trace.plane.normal` durch.
        //
        // Das Flag FX_IMPACT_RUNS_FX braucht es — aber es steht schon, wenn
        // die Liste da ist: ParseImpactFxStrings setzt es selbst, zusammen
        // mit FX_APPLY_PHYSICS (siehe efx::effectiveFlags). Hier stand "ein
        // gesetztes impactfx allein tut nichts" — im Spiel tut es sehr wohl.
        if (item.hasPath && !p.impactFx.empty() && loader &&
            (flags & kFlagImpactRunsFx) != 0) {
            const Effect* child = loader(pickEffect(p.impactFx));
            if (!child) {
                ++missingEffects_;
            } else {
                // Eine Obergrenze: ein Funke mit hoher Elastizitaet prallt
                // ein Dutzend Mal, und jeder Aufprall startet einen Effekt.
                constexpr size_t kMaxImpactEffects = 8;
                const size_t count =
                    std::min(item.path.impactMs.size(), kMaxImpactEffects);
                for (size_t k = 0; k < count; ++k) {
                    Axis impactAxis;
                    impactAxis.forward = camera::normalise(item.path.impactNormal[k]);
                    camera::Vec3 side =
                        camera::cross(impactAxis.forward, {0.0f, 0.0f, 1.0f});
                    if (camera::length(side) < 1e-4f) {
                        side = camera::cross(impactAxis.forward, {0.0f, 1.0f, 0.0f});
                    }
                    impactAxis.right = camera::normalise(side);
                    impactAxis.up = camera::normalise(
                        camera::cross(impactAxis.forward, impactAxis.right));

                    ++startedEffects_;
                    PlayContext impactContext = context;
                    impactContext.axis = impactAxis;
                    playInto(*child, random, {}, impactContext, depth + 1,
                             item.spawnMs + item.path.impactMs[k],
                             item.path.impactPoint[k]);
                }
            }
        }

        // deathFx: beim Sterben einen weiteren Effekt starten.
        //
        // Das steht in fast jeder Geschossdatei — das Projektil fliegt, und
        // beim Aufschlag kommt die Explosion. Drei Details aus
        // CParticle::Die, die man alle falsch raten wuerde:
        //
        //   1. Es braucht das Flag FX_DEATH_RUNS_FX — aber das setzt
        //      ParseDeathFxStrings selbst, sobald die Liste da ist (SP und
        //      MP). Hier stand "ein gesetztes deathfx allein tut nichts";
        //      das galt fuer unsere Vorschau, nicht fuer das Spiel.
        //   2. Bei FX_KILL_ON_IMPACT wird es beim natuerlichen Tod NICHT
        //      ausgeloest — dann gehoert es zum Aufschlag.
        //   3. Die Achse ist eine ZUFAELLIGE Richtung, nicht die Flugrichtung.
        //      Ravens Kommentar daneben: "Man, this just seems so, like,
        //      uncool and stuff..."
        //
        // Der dritte Punkt ist der ueberraschendste: ein Todeseffekt zeigt in
        // eine beliebige Richtung, egal wohin die Primitive flog.
        if (!p.deathFx.empty() && loader &&
            (flags & kFlagDeathRunsFx) != 0 &&
            (flags & kFlagKillOnImpact) == 0) {
            if (const Effect* child = loader(pickEffect(p.deathFx))) {
                ++startedEffects_;

                // Die zufaellige Achse. Sie wird gezogen, auch wenn sie hier
                // nicht in die Bahn eingeht — so bleibt die Zufallsfolge
                // dieselbe wie in der Engine.
                Axis deathAxis;
                camera::Vec3 norm{random.range(-1.0f, 1.0f),
                                  random.range(-1.0f, 1.0f),
                                  random.range(-1.0f, 1.0f)};
                if (camera::length(norm) > 1e-4f) {
                    deathAxis.forward = camera::normalise(norm);
                    // Zwei Achsen dazu, damit sie senkrecht aufeinander
                    // stehen — sonst verzerrt sich der Kindeffekt.
                    camera::Vec3 side =
                        camera::cross(deathAxis.forward, {0.0f, 0.0f, 1.0f});
                    if (camera::length(side) < 1e-4f) {
                        side = camera::cross(deathAxis.forward, {0.0f, 1.0f, 0.0f});
                    }
                    deathAxis.right = camera::normalise(side);
                    deathAxis.up = camera::normalise(
                        camera::cross(deathAxis.forward, deathAxis.right));
                }

                PlayContext deathContext = context;
                deathContext.axis = deathAxis;
                playInto(*child, random, {}, deathContext, depth + 1, item.deathMs,
                         item.positionAt(item.deathMs));
            } else {
                ++missingEffects_;
            }
        }

        live_.push_back(item);
    }
}

int System::aliveAt(float nowMs) const {
    int count = 0;
    for (const auto& item : live_) {
        if (item.aliveAt(nowMs)) ++count;
    }
    return count;
}

// Gibt zurueck, ob das Viereck Platz hatte. Der Aufrufer zaehlt die
// Weggelassenen, damit die Grenze nicht STILL zuschlaegt.
bool addBillboard(scene::Mesh& mesh, const camera::Vec3& centre,
                  const camera::Vec3& right, const camera::Vec3& up, float halfSize,
                  float rotationDegrees, uint32_t colour,
                  const std::vector<shader::TexMod>* texMods, float seconds) {
    if (halfSize <= 0.0f) return true;

    // Um die Blickachse drehen: die beiden Spannachsen werden gedreht, nicht
    // die Ecken einzeln. Damit bleibt das Viereck quadratisch.
    camera::Vec3 axisX = right;
    camera::Vec3 axisY = up;
    if (rotationDegrees != 0.0f) {
        const float r = rotationDegrees * kPi / 180.0f;
        const float c = std::cos(r), s = std::sin(r);
        axisX = right * c + up * s;
        axisY = up * c - right * s;
    }
    axisX = axisX * halfSize;
    axisY = axisY * halfSize;

    // Die Indizes sind 16 Bit. Ueber 65535 Eckpunkten laeuft der Zaehler
    // still ueber und zeigt auf FREMDE Eckpunkte — das Bild wird zu Fetzen,
    // ohne dass irgendwo etwas gemeldet wird.
    if (!hasRoomForQuad(mesh)) return false;
    const auto base = static_cast<uint16_t>(mesh.vertices.size());
    const camera::Vec3 corners[4] = {
        centre - axisX - axisY,
        centre + axisX - axisY,
        centre + axisX + axisY,
        centre - axisX + axisY,
    };
    float uvs[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

    // `tcMod` anwenden, falls der Shader welche hat. Je Eckpunkt, weil
    // `rotate` und `scale` von der Koordinate abhaengen — ein gemeinsamer
    // Versatz genuegt nur bei `scroll`.
    if (texMods && !texMods->empty()) {
        for (int i = 0; i < 4; ++i) {
            // Die Lage des Eckpunkts MIT uebergeben: `tcMod turb` braucht
            // sie, alle anderen Regeln sehen sie nicht an.
            //
            //     st[0] += sin( ((xyz[0]+xyz[2]) * 1/128 * 0.125 + now) ) * amp;
            //
            // Ohne die Position waere `turb` eine gleichmaessige Verschiebung
            // statt einer Verzerrung — es saehe aus wie ein langsames
            // `scroll` und nicht wie das Flimmern, das gemeint ist. Genau so
            // war es: die Regel war umgesetzt und geprueft, aber der Aufrufer
            // gab nichts weiter, und der Zweig lief nie.
            const float at[3] = {corners[i].x, corners[i].y, corners[i].z};
            const shader::TexCoord moved =
                shader::applyTexMods(*texMods, {uvs[i][0], uvs[i][1]}, seconds, at);
            uvs[i][0] = moved.u;
            uvs[i][1] = moved.v;
        }
    }

    for (int i = 0; i < 4; ++i) {
        scene::Vertex v{};
        v.pos[0] = corners[i].x;
        v.pos[1] = corners[i].y;
        v.pos[2] = corners[i].z;
        v.uv[0] = uvs[i][0];
        v.uv[1] = uvs[i][1];
        v.colour = colour;
        mesh.vertices.push_back(v);
    }
    // uint16_t statt int in der Liste: MSVC warnt sonst ueber Datenverlust,
    // und die Warnung hat recht — die Werte gehen in einen uint16_t-Puffer.
    for (uint16_t offset : {uint16_t{0}, uint16_t{1}, uint16_t{2},
                            uint16_t{0}, uint16_t{2}, uint16_t{3}}) {
        mesh.indices.push_back(static_cast<uint16_t>(base + offset));
    }
    return true;
}

namespace {
void addLine(scene::LineSet& lines, const camera::Vec3& from,
             const camera::Vec3& to, uint32_t colour) {
    scene::Vertex a{};
    a.pos[0] = from.x; a.pos[1] = from.y; a.pos[2] = from.z;
    a.colour = colour;
    scene::Vertex b{};
    b.pos[0] = to.x; b.pos[1] = to.y; b.pos[2] = to.z;
    b.colour = colour;
    lines.vertices.push_back(a);
    lines.vertices.push_back(b);
}
}  // namespace

bool addOrientedQuad(scene::Mesh& mesh, const camera::Vec3& centre,
                     const camera::Vec3& normal, float halfSize,
                     float rotationDegrees, uint32_t colour) {
    if (halfSize <= 0.0f) return true;
    camera::Vec3 forward = camera::normalise(normal);
    if (camera::length(forward) < 1e-5f) forward = {0.0f, 0.0f, 1.0f};

    // Zwei Achsen senkrecht zur Normalen. Die Wahl der ersten ist beliebig,
    // solange sie nicht parallel zur Normalen liegt — deshalb die Ausweiche.
    camera::Vec3 axisX = camera::cross(forward, {0.0f, 0.0f, 1.0f});
    if (camera::length(axisX) < 1e-5f) axisX = camera::cross(forward, {0.0f, 1.0f, 0.0f});
    axisX = camera::normalise(axisX);
    camera::Vec3 axisY = camera::normalise(camera::cross(forward, axisX));

    if (rotationDegrees != 0.0f) {
        const float r = rotationDegrees * kPi / 180.0f;
        const float c = std::cos(r), s = std::sin(r);
        const camera::Vec3 rotatedX = axisX * c + axisY * s;
        axisY = axisY * c - axisX * s;
        axisX = rotatedX;
    }
    axisX = axisX * halfSize;
    axisY = axisY * halfSize;

    // Die Indizes sind 16 Bit. Ueber 65535 Eckpunkten laeuft der Zaehler
    // still ueber und zeigt auf FREMDE Eckpunkte — das Bild wird zu Fetzen,
    // ohne dass irgendwo etwas gemeldet wird.
    if (!hasRoomForQuad(mesh)) return false;
    const auto base = static_cast<uint16_t>(mesh.vertices.size());
    const camera::Vec3 corners[4] = {centre - axisX - axisY, centre + axisX - axisY,
                                     centre + axisX + axisY, centre - axisX + axisY};
    const float uvs[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    for (int i = 0; i < 4; ++i) {
        scene::Vertex v{};
        v.pos[0] = corners[i].x; v.pos[1] = corners[i].y; v.pos[2] = corners[i].z;
        v.uv[0] = uvs[i][0]; v.uv[1] = uvs[i][1];
        v.colour = colour;
        mesh.vertices.push_back(v);
    }
    for (uint16_t offset : {uint16_t{0}, uint16_t{1}, uint16_t{2},
                            uint16_t{0}, uint16_t{2}, uint16_t{3}}) {
        mesh.indices.push_back(static_cast<uint16_t>(base + offset));
    }
    return true;
}

bool addCylinder(scene::Mesh& mesh, const camera::Vec3& base,
                 const camera::Vec3& axis, float length, float radius,
                 float radius2, uint32_t colour, int segments) {
    if (length <= 0.0f || segments < 3) return true;
    if (radius <= 0.0f && radius2 <= 0.0f) return true;

    // Dieselbe Grenze wie bei den Vierecken (hasRoomForQuad), aber fuer den
    // GANZEN Mantel: vier Eckpunkte je Abschnitt, 16 Abschnitte sind 64
    // Eckpunkte. Ohne diese Pruefung liefen die 16-Bit-Indizes ab etwa 1024
    // Zylindern mit demselben Shader still ueber — `count 2000` ergab ein
    // Netz mit 128000 Eckpunkten, dessen Indizes auf fremde Ecken zeigten.
    // Ganz oder gar nicht: ein halber Zylinder waere schlimmer als keiner.
    if (mesh.vertices.size() + static_cast<size_t>(segments) * 4u > 65535u) {
        return false;
    }

    camera::Vec3 up = camera::normalise(axis);
    if (camera::length(up) < 1e-5f) up = {0.0f, 0.0f, 1.0f};
    camera::Vec3 right = camera::cross(up, {0.0f, 0.0f, 1.0f});
    if (camera::length(right) < 1e-5f) right = camera::cross(up, {0.0f, 1.0f, 0.0f});
    right = camera::normalise(right);
    const camera::Vec3 forward = camera::normalise(camera::cross(up, right));

    const camera::Vec3 top = base + up * length;
    for (int i = 0; i < segments; ++i) {
        const float a0 = static_cast<float>(i) / segments * 2.0f * kPi;
        const float a1 = static_cast<float>(i + 1) / segments * 2.0f * kPi;
        auto ring = [&](float angle, float r) {
            return right * (std::cos(angle) * r) + forward * (std::sin(angle) * r);
        };

        const auto start = static_cast<uint16_t>(mesh.vertices.size());
        const camera::Vec3 corners[4] = {
            base + ring(a0, radius), base + ring(a1, radius),
            top + ring(a1, radius2), top + ring(a0, radius2)};
        // Die Textur laeuft einmal um den Mantel — so macht es FX_AddCylinder.
        const float u0 = static_cast<float>(i) / segments;
        const float u1 = static_cast<float>(i + 1) / segments;
        const float uvs[4][2] = {{u0, 1}, {u1, 1}, {u1, 0}, {u0, 0}};
        for (int k = 0; k < 4; ++k) {
            scene::Vertex v{};
            v.pos[0] = corners[k].x; v.pos[1] = corners[k].y; v.pos[2] = corners[k].z;
            v.uv[0] = uvs[k][0]; v.uv[1] = uvs[k][1];
            v.colour = colour;
            mesh.vertices.push_back(v);
        }
        for (uint16_t offset : {uint16_t{0}, uint16_t{1}, uint16_t{2},
                                uint16_t{0}, uint16_t{2}, uint16_t{3}}) {
            mesh.indices.push_back(static_cast<uint16_t>(start + offset));
        }
    }
    return true;
}

void addLightning(scene::LineSet& lines, const camera::Vec3& from,
                  const camera::Vec3& to, float chaos, unsigned seed,
                  uint32_t colour) {
    camera::Vec3 forward = to - from;
    float distance = camera::length(forward);
    if (distance < 1e-3f) return;
    forward = forward * (1.0f / distance);

    // Bei mehr als 2000 Einheiten deckelt die Engine:
    //
    //     if (dis > 2000)  // freaky long
    //     {
    //         dis = 2000;
    //     }
    //
    // Das ist keine reine Vorsichtsmassnahme: `dis` geht in `perc` ein, der
    // Blitz erreicht sein Ende also schon nach 2000 Einheiten und wird danach
    // nicht weiter gezackt. Ohne den Deckel bekaeme ein sehr langer Blitz bei
    // uns Hunderte zusaetzliche Knicke — und saehe damit anders aus als im
    // Spiel, nicht nur feiner.
    constexpr float kLongestBolt = 2000.0f;
    if (distance > kLongestBolt) distance = kLongestBolt;

    // Die Querachsen kommen aus `MakeNormalVectors`, nicht aus einem Kreuz mit
    // der Hochachse:
    //
    //     MakeNormalVectors( fwd, rt, up );
    //
    // Das ist eine andere Basis, und weil die Abweichungen genau in dieser
    // Ebene liegen, zackt der Blitz sonst in anderer Richtung.
    const Axis basis = axisFromDirection(forward);
    const camera::Vec3 right = basis.right;
    const camera::Vec3 up = basis.up;

    sim::Random random(seed);
    auto crandom = [&] { return random.range(-1.0f, 1.0f); };

    // Der Anfangsversatz ist {10,10,10}, nicht null.
    //
    //     vec3_t cur, off={10,10,10};
    //
    // Sieht nach einem vergessenen Testwert aus, ist aber der Grund, warum ein
    // Blitz gleich am Anfang einen Knick hat statt schnurgerade loszulaufen.
    camera::Vec3 off{10.0f, 10.0f, 10.0f};
    camera::Vec3 previous = from;

    // Schrittweite 16 Einheiten, nicht 20:
    //
    //     for ( i = 16; i <= dis; i += 16 )
    //
    // Die Zahl bestimmt, wie fein der Blitz gezackt ist — bei 20 hat er ein
    // Fuenftel weniger Knicke als im Spiel.
    constexpr float kStep = 16.0f;

    for (float i = kStep; i <= distance; i += kStep) {
        // Bei grossen Schritten kommt man nicht genau ans Ende — dann wird
        // der Anteil auf eins gezogen. Steht so im Renderer.
        const float perc = (i + kStep > distance) ? 1.0f : i / distance;

        // Entlang der Achse weniger, quer dazu mehr — und nur quer wirkt
        // chaos. Ravens Kommentar: "chaos also does not affect this".
        camera::Vec3 step = forward * (crandom() * 3.0f);
        step = step + right * (crandom() * 7.0f * chaos);
        step = step + up * (crandom() * 7.0f * chaos);
        off = off + step;

        // Die Abweichungen summieren sich, aber beide Enden sitzen trotzdem
        // exakt: der Punkt wird auf die Gerade zurueckgezogen.
        camera::Vec3 current = from + off;
        current = current * (1.0f - perc) + to * perc;

        addLine(lines, previous, current, colour);
        previous = current;
    }
    if (camera::length(previous - to) > 1e-3f) addLine(lines, previous, to, colour);
}


DrawList System::build(float nowMs, const camera::Vec3& right,
                       const camera::Vec3& up,
                       const ShaderLookup& shaders) const {
    DrawList out;
    static const std::vector<shader::TexMod> noMods;

    for (const auto& item : live_) {
        if (!item.aliveAt(nowMs)) continue;
        ++out.alive;

        const float endMs = item.deathMs;
        const camera::Vec3 position = item.positionAt(nowMs);

        const float size = curve::evaluate(item.size, nowMs, item.spawnMs, endMs,
                                           item.sizeParm, item.randomSize);
        const float alpha =
            item.alpha.flags || item.alpha.start != 0.0f
                ? curve::evaluate(item.alpha, nowMs, item.spawnMs, endMs,
                                  item.alphaParm, item.randomAlpha)
                : 1.0f;
        const float r = curve::evaluate(item.rgb[0], nowMs, item.spawnMs, endMs,
                                        item.rgbParm, item.randomRgb);
        const float g = curve::evaluate(item.rgb[1], nowMs, item.spawnMs, endMs,
                                        item.rgbParm, item.randomRgb);
        const float b = curve::evaluate(item.rgb[2], nowMs, item.spawnMs, endMs,
                                        item.rgbParm, item.randomRgb);
        // Wohin das Ausblenden wirkt — siehe Live::useAlpha.
        //
        // Hier stand bisher immer `packRgba(r, g, b, alpha)`, also der
        // useAlpha-Fall fuer ALLE Segmente. Das ist der Fall, den die Engine
        // nur bei gesetztem Flag nimmt, und es hat die Mehrheit falsch
        // gemacht: bei additiver Mischung wertet die Grafikkarte den
        // Alphakanal nicht aus, das Ausblenden fiel also aus. Flammen und
        // Funken blieben bis zur letzten Millisekunde gleich hell und
        // verschwanden dann schlagartig.
        const uint32_t colour =
            item.useAlpha ? packRgba(r, g, b, alpha)
                          : packRgba(r * alpha, g * alpha, b * alpha, 1.0f);

        // Die Drehung waechst ueber die Lebensdauer — und zwar ZEHNMAL so
        // schnell, wie "Grad je Sekunde" vermuten laesst.
        //
        // `CParticle::UpdateRotation` in FxPrimitives.h:
        //
        //     mRefEnt.rotation += theFxHelper.mFrameTime * 0.01f * mRotationDelta;
        //
        // `mFrameTime` ist in MILLISEKUNDEN (die Sekundenfassung heisst
        // mFloatFrameTime). Aus `ms * 0.01` wird also `Sekunden * 10`:
        // `rotationDelta 1` sind zehn Grad je Sekunde, nicht eines.
        //
        // Wir haben mit Grad je Sekunde gerechnet — jede Datei drehte damit
        // ein Zehntel so schnell wie im Spiel. Bei einem Funkenwirbel faellt
        // das sofort auf, bei einem runden Rauchball gar nicht, und deshalb
        // ist es lange durchgegangen.
        //
        // Dieselbe 0.01 steht bei der Emitter-Drehung, dort mit Ravens
        // Kommentar: "was 0.001f, but then you really have to jack up the
        // delta to even notice anything".
        constexpr float kRotationScale = 10.0f;
        const float seconds = (nowMs - item.spawnMs) * 0.001f;
        const float rotation =
            item.rotation + item.rotationDelta * seconds * kRotationScale;

        switch (item.type) {
            case PrimitiveType::Line: {
                // origin2 ist ein PUNKT, kein Versatz — siehe oben.
                addLine(out.lines, position, item.origin2, colour);
                ++out.drawn;
                break;
            }
            case PrimitiveType::Electricity: {
                // Der Ausgangswert haengt am Partikel, nicht an der Zeit: der
                // Blitz soll zappeln, aber nicht in jedem Bild voellig anders
                // aussehen. Ein Wechsel etwa alle 50 ms trifft, wie es im
                // Spiel wirkt.
                const unsigned frame = static_cast<unsigned>(nowMs / 50.0f);
                addLightning(out.lines, position, item.origin2,
                             item.chaos, item.seed ^ (frame * 2654435761u), colour);
                ++out.drawn;
                break;
            }
            case PrimitiveType::Cylinder: {
                // Laenge und zweiter Radius sind die beiden Felder, die nur
                // dieser Typ liest — size2 wird ausschliesslich von
                // FX_AddCylinder ausgewertet.
                const float length = curve::evaluate(item.length, nowMs, item.spawnMs,
                                                     endMs, item.lengthParm,
                                                     item.randomLength);
                const float radius2 = item.size2.start != 0.0f || item.size2.flags
                                          ? curve::evaluate(item.size2, nowMs,
                                                            item.spawnMs, endMs,
                                                            item.size2Parm, 1.0f)
                                          : size;
                // Die Achse ist die VORWAERTSACHSE des Effekts, nicht
                // `origin2`. Fundstelle FxScheduler.cpp:
                //
                //     FX_AddCylinder( clientID, org, ax[0], ... );
                //
                // Wir hatten `origin2` genommen — das ist bei Line und
                // Electricity der Endpunkt und hat mit der Ausrichtung eines
                // Zylinders nichts zu tun. Eine Datei ohne `origin2` ergab
                // damit einen Zylinder, der immer senkrecht stand.
                camera::Vec3 axis = camera::normalise(item.forward);
                if (camera::length(axis) < 1e-5f) axis = {0.0f, 0.0f, 1.0f};
                if (!addCylinder(out.byTexture[item.shader], position, axis, length,
                                 size * 0.5f, radius2 * 0.5f, colour)) {
                    ++out.skipped;
                    break;
                }
                ++out.drawn;
                break;
            }
            case PrimitiveType::OrientedParticle:
            case PrimitiveType::Decal: {
                // Beide liegen in einer eigenen Lage statt zur Kamera. Beim
                // Decal ist das die Flaeche, auf die es projiziert wird; beim
                // OrientedParticle die eingestellte Ausrichtung.
                // Ebenfalls die Vorwaertsachse:
                //     CG_ImpactMark( handle, org, ax[0], rotation, ... );
                camera::Vec3 normal = camera::normalise(item.forward);
                if (camera::length(normal) < 1e-5f) normal = {0.0f, 0.0f, 1.0f};
                if (!addOrientedQuad(out.byTexture[item.shader], position, normal,
                                     size * 0.5f, rotation, colour)) {
                    ++out.skipped;
                    break;
                }
                ++out.drawn;
                break;
            }
            case PrimitiveType::Tail: {
                // Eine Linie entgegen der Flugrichtung, so lang wie `length`.
                const float length = curve::evaluate(item.length, nowMs, item.spawnMs,
                                                     endMs, item.lengthParm,
                                                     item.randomLength);
                // Die Richtung kommt aus der TATSAECHLICHEN Bewegung, nicht
                // aus der Anfangsgeschwindigkeit.
                //
                // `CTail::CalcNewEndpoint` in FxPrimitives.cpp:
                //
                //     VectorSubtract( mOldOrigin, mOrigin1, temp );
                //     VectorNormalize( temp );
                //     VectorMA( mOrigin1, mLength, temp, mRefEnt.oldorigin );
                //
                // `mOldOrigin` ist die Stelle, an der das Teilchen kurz zuvor
                // war. Der Schweif zeigt also dorthin zurueck, wo es herkam.
                //
                // Wir haben `item.velocity` genommen — den Startwert. Bei
                // einem Funken mit Schwerkraft zeigt der Schweif damit die
                // ganze Flugzeit ueber in dieselbe Richtung, obwohl die Bahn
                // laengst gekruemmt ist. Nach einem Abprall zeigt er sogar in
                // die voellig falsche.
                //
                // Drei Millisekunden zurueck, wie die Engine es im
                // angehefteten Fall rechnet:
                //     VectorMA( org, (time - 0.003f), realVel, mOldOrigin );
                constexpr float kLookBackMs = 3.0f;
                const float earlier = std::max(item.spawnMs, nowMs - kLookBackMs);
                camera::Vec3 direction = item.positionAt(earlier) - position;
                if (camera::length(direction) < 1e-5f) {
                    // Am Anfang und im Stillstand: die Anfangsgeschwindigkeit,
                    // umgekehrt.
                    direction = item.velocity * -1.0f;
                }
                direction = camera::normalise(direction);
                if (camera::length(direction) < 1e-5f) direction = {0.0f, 0.0f, -1.0f};
                addLine(out.lines, position, position + direction * length, colour);
                ++out.drawn;
                break;
            }
            case PrimitiveType::Sound:
            case PrimitiveType::CameraShake:
            case PrimitiveType::FxRunner:
                // Nichts zu zeichnen — die wirken anders.
                break;
            case PrimitiveType::Light:
                // Ein Licht zeichnet NICHTS. Fundstelle `CLight::Draw` in
                // FxPrimitives.cpp — der ganze Rumpf ist ein Aufruf:
                //
                //     theFxHelper.AddLightToScene( mOrigin1, mRefEnt.radius, ... );
                //
                // Es erhellt die Umgebung, es hat keine eigene Flaeche.
                //
                // Bei uns fiel der Typ in den Standardzweig und wurde ein
                // Billboard. Ohne Shader heisst das: ein weisses Viereck in
                // Groesse des Lichtradius. In side_alt_explosion.efx hat das
                // Segment "Flash" den Radius 350 — daher der grosse helle
                // Kasten mitten in der Explosion.
                //
                // Kein Sonderfall: Licht kommt in vielen Explosionen vor.
                break;
            default: {
                // Alles andere als Billboard. Ausgerichtete Vierecke, Zylinder
                // und Decals brauchen noch eine eigene Behandlung; bis dahin
                // sieht man wenigstens, dass und wo etwas passiert.
                // Die `tcMod`-Regeln des Shaders und die Zeit seit dem
                // Ausloesen — dieselbe Zeit, die auch die Bildfolgen steuert.
                const ShaderDraw info =
                    shaders ? shaders(item.shader) : ShaderDraw{};
                const std::vector<shader::TexMod>& mods =
                    info.texMods ? *info.texMods : noMods;
                const float shaderSeconds = (nowMs - item.spawnMs) * 0.001f;

                // `rgbGen wave`: der Shader gibt die Helligkeit vor und
                // ersetzt damit die Farbe aus der .efx.
                //
                // Die Durchsichtigkeit bleibt, was die .efx sagt. Die Engine
                // setzt an dieser Stelle zwar `color[3] = 255`, aber das ist
                // nur der Ausgangswert der Stufe — `alphaGen` laeuft danach
                // getrennt und holt sie sich in aller Regel wieder. Ihre
                // Aussteuerung hier mitzuloeschen wuerde ein ausblendendes
                // Teilchen hart stehen lassen.
                uint32_t drawColour = colour;
                if (info.alphaWave) {
                    float glow =
                        shader::evaluateWave(*info.alphaWave, shaderSeconds);
                    if (glow < 0.0f) glow = 0.0f;
                    if (glow > 1.0f) glow = 1.0f;
                    const auto level =
                        static_cast<uint32_t>(glow * 255.0f + 0.5f);
                    drawColour = (drawColour & 0x00FFFFFFu) | (level << 24);
                }
                if (info.rgbWave) {
                    float glow = shader::evaluateWave(*info.rgbWave, shaderSeconds);
                    if (glow < 0.0f) glow = 0.0f;
                    if (glow > 1.0f) glow = 1.0f;
                    const auto level =
                        static_cast<uint32_t>(glow * 255.0f + 0.5f);
                    drawColour = (drawColour & 0xFF000000u) | (level << 16) |
                                 (level << 8) | level;
                }

                if (!addBillboard(out.byTexture[item.shader], position, right, up,
                                  size * 0.5f, rotation, drawColour, &mods,
                                  shaderSeconds)) {
                    ++out.skipped;
                    break;
                }
                if (item.useAlpha) out.alphaShaders.insert(item.shader);
                ++out.drawn;
                break;
            }
        }
    }
    return out;
}

PreviewInfo describePreview(const System& system) {
    // Zwei Fragen in einem Durchgang, weil beide dasselbe brauchen: den
    // Effekt zu mehreren Zeitpunkten aufbauen.
    //
    //   1. Wie weit reicht er?  -> wohin die Kamera muss
    //   2. Wann sieht man ihn?  -> welcher Zeitpunkt als Standbild taugt
    //
    // Die zweite Frage war bisher gar nicht gestellt, und das sieht man: eine
    // Kachel begann bei 0 ms. Da hat ein Aufschlag noch nichts ausgeloest und
    // eine Explosion noch nicht gezuendet — die Kachel blieb schwarz, bis der
    // Zufall sie im richtigen Moment erwischte. Im Raster sah das aus, als
    // waeren die Effekte kaputt.
    //
    // Genommen wird der Zeitpunkt mit den MEISTEN Eckpunkten. Das ist nicht
    // "der schoenste", aber es ist messbar, und es ist nie der leere.
    const camera::Vec3 right{1.0f, 0.0f, 0.0f};
    const camera::Vec3 up{0.0f, 1.0f, 0.0f};

    const float duration = system.durationMs() > 0.0f ? system.durationMs() : 1.0f;

    PreviewInfo out;
    float longest = 0.0f;
    size_t mostVertices = 0;

    // Vierundzwanzig Proben. Ein Aufschlag blitzt in den ersten Millisekunden
    // und ist danach weg; eine Explosion wird erst spaet gross. Wer nur den
    // Anfang oder nur die Mitte nimmt, verfehlt eines von beiden.
    constexpr int kSamples = 24;
    for (int i = 0; i <= kSamples; ++i) {
        const float t = duration * (static_cast<float>(i) / kSamples);
        const DrawList list = system.build(t, right, up);

        size_t vertices = 0;
        for (const auto& group : list.byTexture) {
            for (const scene::Vertex& v : group.second.vertices) {
                const float d = v.pos[0] * v.pos[0] + v.pos[1] * v.pos[1] +
                                v.pos[2] * v.pos[2];
                if (d > longest) longest = d;
            }
            vertices += group.second.vertices.size();
        }
        for (const scene::Vertex& v : list.lines.vertices) {
            const float d = v.pos[0] * v.pos[0] + v.pos[1] * v.pos[1] +
                            v.pos[2] * v.pos[2];
            if (d > longest) longest = d;
        }
        vertices += list.lines.vertices.size();

        if (vertices > mostVertices) {
            mostVertices = vertices;
            out.bestTimeMs = t;
        }
    }

    const float reach = std::sqrt(longest);
    // Untergrenze: ein Effekt ohne sichtbare Geometrie (nur Klang, nur
    // CameraShake) darf die Kamera nicht in den Ursprung stellen.
    out.reach = reach > 1.0f ? reach : 1.0f;
    return out;
}

float visualReach(const System& system) { return describePreview(system).reach; }

}  // namespace efx::particles
