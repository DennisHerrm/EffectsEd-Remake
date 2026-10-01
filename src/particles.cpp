#include "efx/particles.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <tuple>

#include "efx/assets.h"

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

camera::Vec3 Live::anglesAt(float nowMs) const {
    // Ein Bild zu 1/60 s, wie die Zufallswerte (randomAt).
    constexpr float kFrameMs = 1000.0f / 60.0f;
    // Ab hier dreht sich nichts mehr sichtbar: nach dem Liegenbleiben
    // schrumpft die Drehung je Bild auf 60 % — nach dreissig Bildern ist
    // sie ein Millionstel.
    constexpr float kResting = 1e-4f;

    camera::Vec3 out = angles;
    camera::Vec3 delta = angleDelta;
    float remaining = nowMs - spawnMs;
    camera::Vec3 previous = positionAt(spawnMs);
    for (int frame = 0; remaining > 0.0f; ++frame) {
        const float step = std::min(kFrameMs, remaining);
        // Das erste Update laeuft im Bild der Entstehung: `mTimeStart <
        // mTime` ist noch falsch, UpdateOrigin wird uebersprungen, der
        // Ursprung bleibt gleich — und schon das erste Bild daempft auf 60 %.
        // Danach nur, wenn der Brocken sich nicht bewegt hat (VectorCompare;
        // bei uns mit einem Hauch Spiel fuer die Rundung).
        bool resting = frame == 0;
        if (!resting) {
            const camera::Vec3 here = positionAt(spawnMs + frame * kFrameMs);
            const camera::Vec3 moved = here - previous;
            resting = camera::dot(moved, moved) < 1e-8f;
            previous = here;
        }
        if (resting) delta = delta * 0.6f;
        // VectorMA( mAngles, mFrameTime * 0.01f, mAngleDelta, mAngles ).
        out = out + delta * (step * 0.01f);
        remaining -= kFrameMs;
        if (std::max({std::fabs(delta.x), std::fabs(delta.y), std::fabs(delta.z)}) < kResting) {
            break;
        }
    }
    return out;
}

void anglesToAxis(const camera::Vec3& angles, camera::Vec3 axis[3]) {
    // AngleVectors (q_math.c), Winkel in der Reihenfolge PITCH, YAW, ROLL.
    const float toRad = kPi / 180.0f;
    const float sy = std::sin(angles.y * toRad), cy = std::cos(angles.y * toRad);
    const float sp = std::sin(angles.x * toRad), cp = std::cos(angles.x * toRad);
    const float sr = std::sin(angles.z * toRad), cr = std::cos(angles.z * toRad);
    axis[0] = {cp * cy, cp * sy, -sp};
    const camera::Vec3 right{-1.0f * sr * sp * cy + -1.0f * cr * -sy,
                             -1.0f * sr * sp * sy + -1.0f * cr * cy, -1.0f * sr * cp};
    axis[2] = {cr * sp * cy + -sr * -sy, cr * sp * sy + -sr * cy, cr * cp};
    // AnglesToAxis: VectorSubtract( vec3_origin, right, axis[1] ) — links.
    axis[1] = right * -1.0f;
}

camera::Vec3 vectorToAngles(const camera::Vec3& v) {
    // vectoangles (q_math.c), Wort fuer Wort — auch die Sonderfaelle fuer
    // senkrechte Richtungen, die atan2 sonst anders beantworten wuerde.
    float yaw = 0.0f, pitch = 0.0f;
    if (v.y == 0.0f && v.x == 0.0f) {
        pitch = v.z > 0.0f ? 90.0f : 270.0f;
    } else {
        if (v.x != 0.0f) {
            yaw = std::atan2(v.y, v.x) * 180.0f / kPi;
        } else {
            yaw = v.y > 0.0f ? 90.0f : 270.0f;
        }
        if (yaw < 0.0f) yaw += 360.0f;
        const float forward = std::sqrt(v.x * v.x + v.y * v.y);
        pitch = std::atan2(v.z, forward) * 180.0f / kPi;
        if (pitch < 0.0f) pitch += 360.0f;
    }
    return {-pitch, yaw, 0.0f};
}

void System::stop() {
    // Nur anhalten. Die Geometrie bleibt, damit man sie weiter anfahren kann.
    playing_ = false;
}

void System::spawnMore(const Effect& effect, unsigned seed, const std::vector<bool>& enabledMask,
                       float atMs, const camera::Vec3& origin) {
    sim::Random random(seed);
    const PlayContext context{&loader_, &planes_, axis_, &models_};
    playInto(effect, random, enabledMask, context, 0, atMs, origin);
    playing_ = !live_.empty();
}

void System::forgetDeadBefore(float nowMs) {
    live_.erase(std::remove_if(live_.begin(), live_.end(),
                               [nowMs](const Live& item) { return item.deathMs < nowMs; }),
                live_.end());
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
                  bool buildUpRepeats, const camera::Vec3& origin,
                  ModelLoader models) {
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
    planes_ = planes;
    loader_ = loader;
    models_ = std::move(models);
    axis_ = axis;
    sim::Random random(seed);
    const PlayContext context{&loader, &planes, axis, &models_};
    playInto(effect, random, enabledMask, context, 0, 0.0f, origin);

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
                     -delay * static_cast<float>(g), origin);
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
        item.flags = flags;
        item.spawnMs = spawn.timeMs;

        // Lebensdauer. Ohne Angabe 50 ms — mLife im Erzeuger von
        // CPrimitiveTemplate.
        const float life = p.life.set ? random.pick(p.life) : 50.0f;
        item.deathMs = item.spawnMs + std::max(life, 1.0f);

        // Ein Decal lebt nicht nach `life`, sondern als Abdruck: FxScheduler
        // ruft CG_ImpactMark, und cg_marks.cpp haelt jeden Abdruck
        // MARK_TOTAL_TIME = 10000 ms (CG_AddMarks). 38 von 41 Decals der
        // ausgelieferten Dateien haben gar kein `life` — bei uns blitzten sie
        // 50 ms auf, im Spiel bleibt die Brandspur zehn Sekunden.
        constexpr float kMarkTotalMs = 10000.0f;
        if (p.type == PrimitiveType::Decal) item.deathMs = item.spawnMs + kMarkTotalMs;

        // CElectricity::Initialize: mRefEnt.frame = Q_flrand(0,1) * 1265536.
        item.boltSeed = static_cast<int>(random.next() * 1265536.0f);

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

        // `org2fromTrace` (nur Line und Electricity): der Endpunkt ist, wo ein
        // Strahl vom Ursprung entlang der Vorwaertsachse auftrifft.
        // FxScheduler.cpp, "Line type primitives work with an origin2":
        //
        //     VectorMA( org, FX_MAX_TRACE_DIST, ax[0], temp );
        //     if ( FX_ORG2_IS_OFFSET ) temp += org2 (roh oder ueber die Achsen);
        //     Trace( &tr, org, ..., temp, ... );
        //     org2 = tr.startsolid ? org : tr.endpos;
        //     if ( FX_TRACE_IMPACT_FX ) PlayEffect( impactFx, org2, tr.plane.normal );
        //
        // Getroffen werden hier die Waende des Testraums. Das Flag stand nur in
        // der Eigenschaftsseite; der Endpunkt war das rohe origin2 — meist
        // null, also eine Linie der Laenge null (33 Primitive im Grundspiel).
        bool traceHit = false;
        camera::Vec3 traceNormal{0.0f, 0.0f, 1.0f};
        if ((p.type == PrimitiveType::Line || p.type == PrimitiveType::Electricity) &&
            (p.spawnFlags & kSpawnOrg2FromTrace) != 0) {
            // FX_MAX_TRACE_DIST = WORLD_SIZE = 2 * 64 * 1024 (q_shared.h).
            constexpr float kMaxTraceDist = 131072.0f;
            camera::Vec3 far = item.origin + own.forward * kMaxTraceDist;
            if ((p.spawnFlags & kSpawnOrg2IsOffset) != 0) {
                far = far + ((p.spawnFlags & kSpawnCheapOrg2Calc) != 0 ? raw2
                                                                       : alongOwn(raw2));
            }
            const sim::Hit hit = sim::trace(item.origin, far, planes);
            item.origin2 = hit.hit ? hit.point : far;
            traceHit = hit.hit;
            if (hit.hit) traceNormal = hit.normal;
        }
        // Bewegung. Zwei Regeln aus CFxScheduler::CreateEffect (FxScheduler.cpp,
        // "There are only a few types that really use velocity and
        // acceleration"):
        //
        //   1. Nur Particle, OrientedParticle, Tail und Emitter bewegen sich.
        //      Line, Electricity, Cylinder, Decal, Light usw. bekommen weder
        //      Geschwindigkeit noch Beschleunigung noch Schwerkraft — eine
        //      Linie mit `velocity` steht im Spiel still. Bei uns wanderte sie.
        //   2. `absoluteVel` / `absoluteAccel`: der Vektor gilt in
        //      WELTkoordinaten (`VectorSet( vel, mVelX, mVelY, mVelZ )`), sonst
        //      ueber die Achsen gedreht. Die Flags standen nur in der
        //      Eigenschaftsseite; ein aufsteigender Rauch mit `0 0 100` und
        //      absoluteVel flog bei uns seitwaerts.
        //
        // Die Schwerkraft wirkt immer in Welt-z — sie steht im selben Block.
        const bool moves = p.type == PrimitiveType::Particle ||
                           p.type == PrimitiveType::OrientedParticle ||
                           p.type == PrimitiveType::Tail ||
                           p.type == PrimitiveType::Emitter;
        if (moves) {
            const camera::Vec3 rawVel = pickVec3(p.velocity, random);
            const camera::Vec3 rawAccel = pickVec3(p.acceleration, random);
            item.velocity = (p.spawnFlags & kSpawnVelIsAbsolute) != 0 ? rawVel
                                                                      : alongOwn(rawVel);
            item.acceleration = (p.spawnFlags & kSpawnAccelIsAbsolute) != 0
                                    ? rawAccel
                                    : alongOwn(rawAccel);
            item.gravity = p.gravity.set ? random.pick(p.gravity) : 0.0f;
        }

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
        //
        // Ohne Angabe 0, nicht 1: mElasticity steht NICHT im Erzeuger von
        // CPrimitiveTemplate (FxTemplate.cpp), bleibt also beim CFxRange()-
        // Anfangswert 0 — ein Blitz ohne `bounce` ist gerade, bis auf das
        // Mikrozittern von ApplyShape.
        item.chaos = p.elasticity.set ? random.pick(p.elasticity) : 0.0f;

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

        // Emitter: Lage, Drehung und Modell, wie FxScheduler.cpp sie an
        // FX_AddEmitter uebergibt (Fall Emitter in CreateEffect):
        //
        //     VectorSet( ang, mAngle1.GetVal(), mAngle2.GetVal(), mAngle3.GetVal() );
        //     vectoangles( ax[0], temp );
        //     VectorAdd( ang, temp, ang );
        //     VectorSet( angDelta, mAngle1Delta.GetVal(), ... );
        //     emitterModel = fx->mMediaHandles.GetHandle();
        //
        // `ax[0]` ist die Achse DIESER Primitive (`own`) — nach
        // randRotAroundFwd und axisFromSphere. Ohne angle steht 0: mAngle1..3
        // stehen nicht im Erzeuger von CPrimitiveTemplate.
        //
        // Nur hier gewuerfelt, nicht fuer alle Typen: sonst verschoeben sich
        // die Zufallsfolgen jedes anderen Segments.
        if (p.type == PrimitiveType::Emitter) {
            item.angles = pickVec3(p.angles, random) + vectorToAngles(own.forward);
            item.angleDelta = pickVec3(p.anglesDelta, random);
            if ((flags & kFlagAttachedModel) != 0 && !p.models.empty()) {
                // GetHandle: irand( 0, size-1 ) — jeder Eintrag gleich oft.
                const size_t at = static_cast<size_t>(
                    random.next() * static_cast<float>(p.models.size()));
                const std::string& name = p.models[std::min(at, p.models.size() - 1)];
                if (context.models != nullptr && *context.models) {
                    item.model = (*context.models)(name);
                }
            }
        }


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

        // Die Achse eines Kindeffekts ist die dieser Primitive (`own`, nach
        // randRotAroundFwd und axisFromSphere) — FxScheduler.cpp:
        // `PlayEffect( fx->mPlayFxHandles.GetHandle(), org, ax );` mit dem
        // veraenderten ax. Vorher bekam das Kind die Achse des Elternteils.
        PlayContext ownContext = context;
        ownContext.axis = own;

        if (p.type == PrimitiveType::FxRunner) {
            if (!p.playFx.empty() && loader) {
                if (const Effect* child = loader(pickEffect(p.playFx))) {
                    ++startedEffects_;
                    playInto(*child, random, {}, ownContext, depth + 1, item.spawnMs,
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
                            playInto(*child, random, {}, ownContext, depth + 1, t, at);
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
                    // PlayEffect( id, org, forward ) baut die Achsen mit
                    // MakeNormalVectors (FxScheduler.cpp) — dieselbe Rechnung
                    // wie axisFromDirection. Wir hatten cross(forward, z)
                    // genommen: der Kindeffekt lag um die Normale verdreht.
                    const Axis impactAxis = axisFromDirection(item.path.impactNormal[k]);

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
        //   2. Bei FX_KILL_ON_IMPACT wird es nur beim TOD DURCH AUFPRALL nicht
        //      ausgeloest. Laeuft die Lebensdauer ab, loescht FxUtil.cpp das
        //      Flag vorher ("this flag just has to be cleared otherwise death
        //      effects might not happen correctly"):
        //          ef->mEffect->ClearFlags( FX_KILL_ON_IMPACT ); FX_FreeMember( ef );
        //      Wir hatten deathfx mit killOnImpact ganz abgeschaltet.
        //   3. Die Achse ist eine ZUFAELLIGE Richtung, nicht die Flugrichtung.
        //      Ravens Kommentar daneben: "Man, this just seems so, like,
        //      uncool and stuff..."
        //
        // Der dritte Punkt ist der ueberraschendste: ein Todeseffekt zeigt in
        // eine beliebige Richtung, egal wohin die Primitive flog.
        const bool killedByImpact = item.hasPath && item.path.killed;
        if (!p.deathFx.empty() && loader &&
            (flags & kFlagDeathRunsFx) != 0 && !killedByImpact) {
            if (const Effect* child = loader(pickEffect(p.deathFx))) {
                ++startedEffects_;

                // Die zufaellige Achse, wie in CParticle::Die; die beiden
                // anderen baut PlayEffect mit MakeNormalVectors
                // (axisFromDirection).
                Axis deathAxis;
                camera::Vec3 norm{random.range(-1.0f, 1.0f),
                                  random.range(-1.0f, 1.0f),
                                  random.range(-1.0f, 1.0f)};
                if (camera::length(norm) > 1e-4f) deathAxis = axisFromDirection(norm);

                PlayContext deathContext = context;
                deathContext.axis = deathAxis;
                playInto(*child, random, {}, deathContext, depth + 1, item.deathMs,
                         item.positionAt(item.deathMs));
            } else {
                ++missingEffects_;
            }
        }

        // `traceImpactFx`: am Treffpunkt des Endpunktstrahls den
        // Einschlageffekt starten, ausgerichtet an der Flaechennormale
        // (FxScheduler.cpp, siehe org2fromTrace oben). Ohne Treffer gibt es
        // keine Flaeche und keine Normale — dann nichts.
        if (traceHit && (p.spawnFlags & kSpawnTraceImpactFx) != 0 &&
            !p.impactFx.empty() && loader) {
            if (const Effect* child = loader(pickEffect(p.impactFx))) {
                ++startedEffects_;
                PlayContext traceContext = context;
                traceContext.axis = axisFromDirection(traceNormal);
                playInto(*child, random, {}, traceContext, depth + 1, item.spawnMs,
                         item.origin2);
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

namespace {

// --- Zufall ------------------------------------------------------------------

// Ein guter Mischer fuer 32 Bit (Ausgang wie bei "lowbias32").
uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float unitFloat(uint32_t x) { return static_cast<float>(x >> 8) * (1.0f / 16777216.0f); }

// Q_rand / Q_random / Q_crandom aus q_math.cpp — ein LCG auf einem int:
//
//     *seed = (69069 * *seed + 1);
//     Q_random  = ( Q_rand( seed ) & 0xffff ) / (float)0x10000;
//     Q_crandom = 2.0 * ( Q_random( seed ) - 0.5 );
//
// In vorzeichenlosen Zahlen gerechnet: der Ueberlauf eines int waere in C++
// undefiniert, die unteren Bits sind dieselben.
float qRandom(int& seed) {
    seed = static_cast<int>(69069u * static_cast<uint32_t>(seed) + 1u);
    return static_cast<float>(static_cast<uint32_t>(seed) & 0xffffu) / 65536.0f;
}
float qCRandom(int& seed) { return 2.0f * (qRandom(seed) - 0.5f); }

}  // namespace

float Live::randomAt(RandomChannel channel, float nowMs) const {
    // Ein Bild alle 1/60 Sekunde. Gleicher Zeitpunkt, gleicher Wert — eine
    // angehaltene Vorschau flackert nicht, eine laufende schon.
    const auto tick = static_cast<uint32_t>(static_cast<int64_t>(std::floor(nowMs * 0.06f)));
    uint32_t h = mix32(seed * 0x9E3779B9u + static_cast<uint32_t>(channel) * 0x85EBCA6Bu);
    h = mix32(h ^ (tick * 0xC2B2AE35u + 0x27D4EB2Fu));
    return unitFloat(h);
}

// Gibt zurueck, ob das Viereck Platz hatte. Der Aufrufer zaehlt die
// Weggelassenen, damit die Grenze nicht STILL zuschlaegt.
//
// `halfSize` ist der Radius der Primitive: RB_SurfaceSprite spannt das Viereck
// mit `left = viewaxis[1] * radius`, `up = viewaxis[2] * radius` auf, und
// CParticle::UpdateSize schreibt `mRefEnt.radius = size`. Ein negativer Wert
// (eine Welle unter null) ergibt in der Engine dasselbe Viereck gespiegelt —
// deshalb wird nur bei genau null nichts gezeichnet.
bool addBillboard(scene::Mesh& mesh, const camera::Vec3& centre,
                  const camera::Vec3& right, const camera::Vec3& up, float halfSize,
                  float rotationDegrees, uint32_t colour,
                  const std::vector<shader::TexMod>* texMods, float seconds) {
    if (halfSize == 0.0f) return true;

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

    // `tcMod` anwenden, falls welche uebergeben wurden. Je Eckpunkt, weil
    // `rotate` und `scale` von der Koordinate abhaengen und `turb` von der
    // Lage des Eckpunkts.
    if (texMods && !texMods->empty()) {
        for (int i = 0; i < 4; ++i) {
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

void pushVertex(scene::Mesh& mesh, const camera::Vec3& p, float u, float v,
                uint32_t colour) {
    scene::Vertex vertex{};
    vertex.pos[0] = p.x;
    vertex.pos[1] = p.y;
    vertex.pos[2] = p.z;
    vertex.uv[0] = u;
    vertex.uv[1] = v;
    vertex.colour = colour;
    mesh.vertices.push_back(vertex);
}

// DoLine2 aus tr_surface.cpp: ein Band mit zwei Breiten und frei waehlbarem
// t an den beiden Enden. DoLine ist der Sonderfall gleicher Breite, t 0..1.
//
//     start + w*up (0,tcStart)   start - w*up (1,tcStart)
//     end + w2*up  (0,tcEnd)     end - w2*up  (1,tcEnd)
//     Dreiecke 0,1,2 und 2,1,3
bool addBand(scene::Mesh& mesh, const camera::Vec3& start, const camera::Vec3& end,
             const camera::Vec3& side, float startWidth, float endWidth,
             float tcStart, float tcEnd, uint32_t colour) {
    if (!hasRoomForQuad(mesh)) return false;
    const auto base = static_cast<uint16_t>(mesh.vertices.size());
    pushVertex(mesh, start + side * startWidth, 0.0f, tcStart, colour);
    pushVertex(mesh, start - side * startWidth, 1.0f, tcStart, colour);
    pushVertex(mesh, end + side * endWidth, 0.0f, tcEnd, colour);
    pushVertex(mesh, end - side * endWidth, 1.0f, tcEnd, colour);
    for (uint16_t offset : {uint16_t{0}, uint16_t{1}, uint16_t{2},
                            uint16_t{2}, uint16_t{1}, uint16_t{3}}) {
        mesh.indices.push_back(static_cast<uint16_t>(base + offset));
    }
    return true;
}

}  // namespace

camera::Vec3 lineSide(const camera::Vec3& start, const camera::Vec3& end,
                      const camera::Vec3& eye) {
    // RB_SurfaceLine:
    //     VectorSubtract( start, viewParms.ori.origin, v1 );
    //     VectorSubtract( end, viewParms.ori.origin, v2 );
    //     CrossProduct( v1, v2, right );  VectorNormalize( right );
    return camera::normalise(camera::cross(start - eye, end - eye));
}

bool addLineQuad(scene::Mesh& mesh, const camera::Vec3& start,
                 const camera::Vec3& end, const camera::Vec3& side, float halfWidth,
                 uint32_t colour) {
    // DoLine( start, end, right, e->radius ): die halbe Breite IST der Radius.
    return addBand(mesh, start, end, side, halfWidth, halfWidth, 0.0f, 1.0f, colour);
}

bool addOrientedQuad(scene::Mesh& mesh, const camera::Vec3& centre,
                     const camera::Vec3& normal, float halfSize,
                     float rotationDegrees, uint32_t colour) {
    if (halfSize == 0.0f) return true;
    camera::Vec3 forward = camera::normalise(normal);
    if (camera::length(forward) < 1e-5f) forward = {0.0f, 0.0f, 1.0f};

    // RB_SurfaceOrientedQuad: `MakeNormalVectors( axis[0], left, up )` —
    // dieselbe Rechnung wie axisFromDirection. RB_AddQuadStamp legt dann
    // (0,0) auf origin + left + up. In unserer Eckenfolge (axisX nach
    // "rechts") ist axisX = -left.
    //
    // Hier stand `cross(normal, z)` als erste Achse. Fuer eine nach oben
    // zeigende Normale kommt dasselbe heraus, fuer jede andere nicht: an einer
    // Wand stand die Textur auf dem Kopf (um 180 Grad gedreht).
    const Axis basis = axisFromDirection(forward);
    camera::Vec3 axisX = basis.right * -1.0f;
    camera::Vec3 axisY = basis.up;

    if (rotationDegrees != 0.0f) {
        // tempLeft = c*left - s*up; up = c*up + s*left — mit axisX = -left:
        const float r = rotationDegrees * kPi / 180.0f;
        const float c = std::cos(r), s = std::sin(r);
        const camera::Vec3 rotatedX = axisX * c + axisY * s;
        axisY = axisY * c - axisX * s;
        axisX = rotatedX;
    }
    axisX = axisX * halfSize;
    axisY = axisY * halfSize;

    if (!hasRoomForQuad(mesh)) return false;
    const auto base = static_cast<uint16_t>(mesh.vertices.size());
    const camera::Vec3 corners[4] = {centre - axisX - axisY, centre + axisX - axisY,
                                     centre + axisX + axisY, centre - axisX + axisY};
    const float uvs[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    for (int i = 0; i < 4; ++i) pushVertex(mesh, corners[i], uvs[i][0], uvs[i][1], colour);
    for (uint16_t offset : {uint16_t{0}, uint16_t{1}, uint16_t{2},
                            uint16_t{0}, uint16_t{2}, uint16_t{3}}) {
        mesh.indices.push_back(static_cast<uint16_t>(base + offset));
    }
    return true;
}

int cylinderSegments(float distanceToEye, float fovXDegrees) {
    // RB_SurfaceCylinder:
    //     length *= (viewParms.fovX / 90.0f);
    //     detail = 1 - ((float) length / 2048 );
    //     segments = NUM_CYLINDER_SEGMENTS * detail;   // 40
    //     if ( segments < 8 ) segments = 8;  if ( segments > 40 ) segments = 40;
    const float length = distanceToEye * (fovXDegrees / 90.0f);
    const float detail = 1.0f - length / 2048.0f;
    int segments = static_cast<int>(40.0f * detail);
    if (segments < 8) segments = 8;
    if (segments > 40) segments = 40;
    return segments;
}

bool addCylinder(scene::Mesh& mesh, const camera::Vec3& base,
                 const camera::Vec3& axis, float length, float baseRadius,
                 float topRadius, uint32_t colour, int segments) {
    if (length == 0.0f || segments < 3) return true;
    if (baseRadius <= 0.0f && topRadius <= 0.0f) return true;

    camera::Vec3 forward = camera::normalise(axis);
    if (camera::length(forward) < 1e-5f) forward = {0.0f, 0.0f, 1.0f};
    const camera::Vec3 top = base + forward * length;

    // MakeNormalVectors( e->axis[0], vr, vu ); die Ringpunkte sind vu,
    // um die Achse gedreht (RotatePointAroundVector) — vu*cos + vr*sin, weil
    // axis x vu = vr.
    const Axis basis = axisFromDirection(forward);
    const auto ring = [&](float radius, int i) {
        const float a = static_cast<float>(i) * 2.0f * kPi / static_cast<float>(segments);
        return (basis.up * std::cos(a) + basis.right * std::sin(a)) * radius;
    };
    const float detail = 1.0f / static_cast<float>(segments);

    // Ein Ende spitz genug: RB_SurfaceCone.
    //     if ( !( radius < 0.3 && backlerp < 0.3 ) && ( radius < 0.3 || backlerp < 0.3 ))
    // `radius` ist size (oben), `backlerp` size2 (am Ursprung).
    const bool baseThin = baseRadius < 0.3f;
    const bool topThin = topRadius < 0.3f;
    if (baseThin != topThin) {
        const size_t needed = 2u * static_cast<size_t>(segments + 1);
        if (mesh.vertices.size() + needed > 65535u) return false;
        // Nur um den groesseren Radius drehen; das andere Ende ist ein Punkt.
        camera::Vec3 ringCentre = top, tapered = base;
        float radius = topRadius;
        if (topRadius < baseRadius) {
            ringCentre = base;
            tapered = top;
            radius = baseRadius;
        }
        auto vbase = static_cast<uint16_t>(mesh.vertices.size());
        for (int i = 0; i <= segments; ++i) {
            const int k = i == segments ? 0 : i;
            pushVertex(mesh, ringCentre + ring(radius, k), detail * static_cast<float>(i),
                       1.0f, colour);
            pushVertex(mesh, tapered, detail * static_cast<float>(i) + detail * 0.5f, 0.0f,
                       colour);
        }
        for (int i = 0; i < segments; ++i) {
            mesh.indices.push_back(vbase);
            mesh.indices.push_back(static_cast<uint16_t>(vbase + 1));
            mesh.indices.push_back(static_cast<uint16_t>(vbase + 2));
            vbase = static_cast<uint16_t>(vbase + 2);
        }
        return true;
    }

    // Dieselbe Grenze wie bei den Vierecken (hasRoomForQuad), aber fuer den
    // GANZEN Mantel. Ganz oder gar nicht: ein halber Zylinder waere
    // schlimmer als keiner.
    const size_t needed = 2u * static_cast<size_t>(segments + 1);
    if (mesh.vertices.size() + needed > 65535u) return false;

    // Oberer Ring (size2) am URSPRUNG mit t = 1, unterer (size) am fernen Ende
    // mit t = 0 — die Namen "upper/lower" sind Ravens, die Lage ist es auch.
    // Der letzte Punkt wird mit s = 1 wiederholt, damit die Textur einmal
    // ganz herumlaeuft.
    auto vbase = static_cast<uint16_t>(mesh.vertices.size());
    for (int i = 0; i <= segments; ++i) {
        const int k = i == segments ? 0 : i;
        const float s = detail * static_cast<float>(i);
        pushVertex(mesh, base + ring(baseRadius, k), s, 1.0f, colour);
        pushVertex(mesh, top + ring(topRadius, k), s, 0.0f, colour);
    }
    for (int i = 0; i < segments; ++i) {
        for (uint16_t offset : {uint16_t{0}, uint16_t{1}, uint16_t{2},
                                uint16_t{2}, uint16_t{1}, uint16_t{3}}) {
            mesh.indices.push_back(static_cast<uint16_t>(vbase + offset));
        }
        vbase = static_cast<uint16_t>(vbase + 2);
    }
    return true;
}

namespace {

// Der Blitz aus tr_surface.cpp (SP), Funktion fuer Funktion.
class BoltBuilder {
public:
    BoltBuilder(scene::Mesh& mesh, const camera::Vec3& side, const camera::Vec3& end,
                const BoltShape& shape, int boltSeed, unsigned jitterSeed, uint32_t colour)
        : mesh_(mesh), side_(side), end_(end), shape_(shape), frame_(boltSeed),
          jitter_(jitterSeed), colour_(colour) {}

    bool ok() const { return ok_; }

    // DoBoltSeg
    void segment(const camera::Vec3& start, const camera::Vec3& end, float radius) {
        camera::Vec3 fwd = end - start;
        float dis = camera::length(fwd);
        fwd = dis > 0.0f ? fwd * (1.0f / dis) : camera::Vec3{};
        if (dis > 2000.0f) dis = 2000.0f;  // "freaky long"
        const Axis basis = axisFromDirection(fwd);
        const camera::Vec3 rt = basis.right;
        const camera::Vec3 up = basis.up;

        camera::Vec3 old = start;
        camera::Vec3 off{10.0f, 10.0f, 10.0f};
        float oldPerc = 0.0f;
        float oldRadius = radius;
        float newRadius = radius;

        for (int i = 16; static_cast<float>(i) <= dis; i += 16) {
            // "because of our large step size, we may not actually draw to the
            // end. In this case, fudge our percent"
            const float perc = static_cast<float>(i + 16) > dis
                                   ? 1.0f
                                   : static_cast<float>(i) / dis;

            // Abweichung: wenig entlang, viel quer — nur quer wirkt chaos
            // (e->angles[0]). Die Reihenfolge der drei Q_crandom ist die der
            // Engine, weil sie dieselbe Folge aufbraucht.
            camera::Vec3 temp = fwd * (qCRandom(frame_) * 3.0f);
            temp = temp + rt * (qCRandom(frame_) * 7.0f * shape_.chaos);
            temp = temp + up * (qCRandom(frame_) * 7.0f * shape_.chaos);
            off = off + temp;

            // Von genau start nach genau end, plus die summierte Abweichung.
            const camera::Vec3 cur = (start + off) * (1.0f - perc) + end * perc;

            if (shape_.taper) {
                // "by using one minus the square, the radius stays fairly
                // constant, then drops off quickly at the very point"
                oldRadius = radius * (1.0f - oldPerc * oldPerc);
                newRadius = radius * (1.0f - perc * perc);
            }

            // Zwei Stufen Feinzacken (2 - r_lodbias, r_lodbias ab Werk 0).
            applyShape(cur, old, newRadius, oldRadius, 2, 0.0f, 1.0f);

            // Abzweige: hoechstens drei, nur in den ersten 20 Prozent. Die
            // Bedingung wird in der Reihenfolge der Engine geprueft — Q_random
            // laeuft nur, wenn die beiden ersten Teile stimmen.
            if (shape_.branch && forks_ > 0 && qRandom(frame_) > 0.93f &&
                (1.0f - perc) > 0.8f) {
                --forks_;
                camera::Vec3 dest = (cur + end_) * 0.5f;
                dest.x += qCRandom(frame_) * 80.0f;
                dest.y += qCRandom(frame_) * 80.0f;
                dest.z += qCRandom(frame_) * 80.0f;
                segment(cur, dest, newRadius);
            }

            old = cur;
            oldPerc = perc;
        }
    }

private:
    // CreateShape: die Form, die auf jedes Stueck gelegt wird. Wie in der
    // Engine GEMEINSAM fuer alle Rekursionsebenen (dort zwei statische
    // Vektoren) — eine tiefere Ebene ueberschreibt sie, und die hoehere rechnet
    // mit dem ueberschriebenen Wert weiter.
    void createShape() {
        sh1_ = {0.66f, 0.08f + jitter_.range(-1.0f, 1.0f) * 0.02f,
                0.08f + jitter_.range(-1.0f, 1.0f) * 0.02f};
        sh2_ = {0.33f, -sh1_.y + jitter_.range(-1.0f, 1.0f) * 0.02f,
                -sh1_.z + jitter_.range(-1.0f, 1.0f) * 0.02f};
    }

    // ApplyShape
    void applyShape(const camera::Vec3& start, const camera::Vec3& end, float sradius,
                    float eradius, int count, float startPerc, float endPerc) {
        if (count < 1) {
            if (!addBand(mesh_, start, end, side_, sradius, eradius, startPerc, endPerc,
                         colour_)) {
                ok_ = false;
            }
            return;
        }
        createShape();

        camera::Vec3 fwd = end - start;
        const float len = camera::length(fwd);
        const float dis = len * 0.7f;
        fwd = len > 0.0f ? fwd * (1.0f / len) : camera::Vec3{};
        const Axis basis = axisFromDirection(fwd);

        float perc = sh1_.x;
        const camera::Vec3 point1 = start * perc + end * (1.0f - perc) +
                                    basis.right * (dis * sh1_.y) + basis.up * (dis * sh1_.z);
        const float rads1 = sradius * 0.666f + eradius * 0.333f;
        const float rads2 = sradius * 0.333f + eradius * 0.666f;

        applyShape(start, point1, sradius, rads1, count - 1, startPerc,
                   startPerc * 0.666f + endPerc * 0.333f);

        perc = sh2_.x;
        const camera::Vec3 point2 = start * perc + end * (1.0f - perc) +
                                    basis.right * (dis * sh2_.y) + basis.up * (dis * sh2_.z);

        applyShape(point2, point1, rads1, rads2, count - 1,
                   startPerc * 0.333f + endPerc * 0.666f,
                   startPerc * 0.666f + endPerc * 0.333f);
        applyShape(point2, end, rads2, eradius, count - 1,
                   startPerc * 0.333f + endPerc * 0.666f, endPerc);
    }

    scene::Mesh& mesh_;
    camera::Vec3 side_;
    camera::Vec3 end_;
    BoltShape shape_;
    int frame_;
    sim::Random jitter_;
    uint32_t colour_;
    int forks_ = 3;  // "allow now more than three branches"
    camera::Vec3 sh1_, sh2_;
    bool ok_ = true;
};

}  // namespace

bool addElectricity(scene::Mesh& mesh, const camera::Vec3& from,
                    const camera::Vec3& to, const camera::Vec3& eye,
                    const BoltShape& shape, int boltSeed, unsigned jitterSeed,
                    uint32_t colour) {
    // RB_SurfaceElectricity: `grow` kuerzt das Ende auf perc * Laenge.
    camera::Vec3 fwd = to - from;
    const float dis = camera::length(fwd);
    if (dis <= 0.0f) return true;
    fwd = fwd * (1.0f / dis);
    float perc = shape.growPerc;
    if (perc > 1.0f) perc = 1.0f;
    if (perc < 0.0f) perc = 0.0f;
    const camera::Vec3 end = from + fwd * (perc * dis);

    const camera::Vec3 side = lineSide(from, end, eye);
    BoltBuilder builder(mesh, side, end, shape, boltSeed, jitterSeed, colour);
    builder.segment(from, end, shape.radius);
    return builder.ok();
}

namespace {

// Die Farbe einer Shaderstufe — ComputeColors in tr_shade.cpp, fuer die
// Faelle, die bei Effekten vorkommen. `entity` ist die Farbe der Primitive
// (shaderRGBA); sie steht bei Effekten auch in den Eckpunkten
// (RB_AddQuadStamp kopiert sie hinein), Vertex und Entity sind also gleich.
// r_overBrightBits 0 vorausgesetzt (OpenJK-Vorgabe): identityLight ist 1.
uint32_t stageColour(const shader::Stage& stage, uint32_t entity, float seconds) {
    const uint32_t er = entity & 0xFFu, eg = (entity >> 8) & 0xFFu,
                   eb = (entity >> 16) & 0xFFu, ea = (entity >> 24) & 0xFFu;
    uint32_t r = 255, g = 255, b = 255, a = 255;
    const shader::ColorGen colorGen = shader::effectiveColorGen(stage);
    switch (colorGen) {
        case shader::ColorGen::Vertex:
        case shader::ColorGen::ExactVertex:
        case shader::ColorGen::Entity:
            r = er; g = eg; b = eb; a = ea;
            break;
        case shader::ColorGen::OneMinusVertex:
        case shader::ColorGen::OneMinusEntity:
            r = 255 - er; g = 255 - eg; b = 255 - eb;
            break;
        case shader::ColorGen::Const:
            r = static_cast<uint32_t>(std::clamp(stage.rgbConst[0], 0.0f, 1.0f) * 255.0f);
            g = static_cast<uint32_t>(std::clamp(stage.rgbConst[1], 0.0f, 1.0f) * 255.0f);
            b = static_cast<uint32_t>(std::clamp(stage.rgbConst[2], 0.0f, 1.0f) * 255.0f);
            break;
        case shader::ColorGen::Wave: {
            // RB_CalcWaveColor: glow = EvalWaveForm * identityLight, auf 0..1.
            const float glow = std::clamp(shader::evaluateWave(stage.rgbWave, seconds), 0.0f, 1.0f);
            r = g = b = static_cast<uint32_t>(glow * 255.0f);
            break;
        }
        default:  // identity, identityLighting, lightingDiffuse/Specular
            break;
    }

    switch (shader::effectiveAlphaGen(stage)) {
        case shader::AlphaGen::Identity:
            // AGEN_IDENTITY setzt 255 — ausser bei rgbGen vertex (identityLight
            // 1): dann bleibt das Alpha, das mit der Eckpunktfarbe kam.
            if (colorGen != shader::ColorGen::Vertex) a = 255;
            break;
        case shader::AlphaGen::Vertex:
        case shader::AlphaGen::Entity:
            a = ea;
            break;
        case shader::AlphaGen::OneMinusVertex:
        case shader::AlphaGen::OneMinusEntity:
            a = 255 - ea;
            break;
        case shader::AlphaGen::Const:
            a = static_cast<uint32_t>(std::clamp(stage.alphaConst, 0.0f, 1.0f) * 255.0f);
            break;
        case shader::AlphaGen::Wave: {
            // RB_CalcWaveAlpha: EvalWaveFormClamped.
            const float glow = std::clamp(shader::evaluateWave(stage.alphaWave, seconds), 0.0f, 1.0f);
            a = static_cast<uint32_t>(glow * 255.0f);
            break;
        }
        default:
            a = 255;
            break;
    }
    return r | (g << 8) | (b << 16) | (a << 24);
}

// Das Bild einer Stufe zu ihrer Shaderzeit — map, clampMap, das gerade
// gueltige Bild einer Bildfolge (RB_ComputeAnimatedImage), oder das
// eingebaute weisse. Leer: die Stufe hat kein Bild und wird uebergangen.
std::string stageImage(const shader::Stage& stage, float seconds, bool& clamp) {
    clamp = false;
    if (!stage.animMaps.empty()) {
        const int frame = shader::animFrameAt(static_cast<int>(stage.animMaps.size()),
                                              stage.animFrequency, stage.animOneShot,
                                              seconds);
        return stage.animMaps[static_cast<size_t>(frame)];
    }
    if (!stage.clampMap.empty()) {
        clamp = true;
        return stage.clampMap;
    }
    return stage.map;
}

int alphaTestOf(const std::string& func) {
    if (func.empty()) return 0;
    auto lower = func;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "gt0") return 1;
    if (lower == "lt128") return 2;
    if (lower == "ge128") return 3;
    if (lower == "ge192") return 4;  // JKA-Zusatz (NameToAFunc: GLS_ATEST_GE_C0)
    return 0;
}

// Mischung, Tiefe und Alphatest einer Gruppe aus ihrer Shaderstufe.
void configureStageGroup(DrawGroup& group, const shader::Stage& stage, float sort, bool clamp) {
    group.clamp = clamp;
    group.blended = shader::stageBlends(stage);
    group.src = group.blended ? stage.srcBlend : shader::BlendFactor::One;
    group.dst = group.blended ? stage.dstBlend : shader::BlendFactor::Zero;
    group.depthWrite = shader::stageWritesDepth(stage);
    // `depthFunc disable` (JKA, ParseStage): GLS_DEPTHTEST_DISABLE —
    // gfx/effects/whiteFlash zeichnet so ueber alles hinweg.
    std::string depthFunc = stage.depthFunc;
    for (char& c : depthFunc) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    group.depthTest = depthFunc != "disable";
    group.alphaTest = alphaTestOf(stage.alphaFunc);
    group.sort = sort;
}

// Sammelt die Zeichengruppen eines Bildes.
class GroupCollector {
public:
    explicit GroupCollector(std::vector<DrawGroup>& groups) : groups_(groups) {}

    DrawGroup& get(const std::string& shaderName, int stage, const std::string& image) {
        const auto key = std::make_tuple(shaderName, stage, image);
        const auto found = index_.find(key);
        if (found != index_.end()) return groups_[found->second];
        int seen = 0;
        const auto known = firstSeen_.find(shaderName);
        if (known != firstSeen_.end()) {
            seen = known->second;
        } else {
            seen = static_cast<int>(firstSeen_.size());
            firstSeen_.emplace(shaderName, seen);
        }
        index_.emplace(key, groups_.size());
        groups_.emplace_back();
        DrawGroup& group = groups_.back();
        group.shader = shaderName;
        group.stage = stage;
        group.image = image;
        group.firstSeen = seen;
        return group;
    }

private:
    std::vector<DrawGroup>& groups_;
    std::map<std::tuple<std::string, int, std::string>, size_t> index_;
    std::map<std::string, int> firstSeen_;
};

// Haengt die rohe Geometrie eines Teilchens mit Stufenfarbe und Stufen-tcMod
// an eine Gruppe.
void appendStage(DrawGroup& group, const scene::Mesh& raw, const shader::Stage* stage,
                 const std::vector<shader::TexMod>* mods, uint32_t entity, float seconds,
                 const shader::WaveForm* legacyRgbWave = nullptr,
                 const shader::WaveForm* legacyAlphaWave = nullptr) {
    if (group.mesh.vertices.size() + raw.vertices.size() > 65535u) return;
    uint32_t colour = entity;
    if (stage) {
        colour = stageColour(*stage, entity, seconds);
    } else {
        // Ersatzshader (rgbGen vertex, alphaGen vertex) — und die alten
        // Einzelangaben aus ShaderDraw, falls ein Aufrufer nur sie liefert.
        if (legacyAlphaWave) {
            const float glow =
                std::clamp(shader::evaluateWave(*legacyAlphaWave, seconds), 0.0f, 1.0f);
            colour = (colour & 0x00FFFFFFu) | (static_cast<uint32_t>(glow * 255.0f + 0.5f) << 24);
        }
        if (legacyRgbWave) {
            const float glow =
                std::clamp(shader::evaluateWave(*legacyRgbWave, seconds), 0.0f, 1.0f);
            const auto level = static_cast<uint32_t>(glow * 255.0f + 0.5f);
            colour = (colour & 0xFF000000u) | (level << 16) | (level << 8) | level;
        }
    }
    const auto base = static_cast<uint16_t>(group.mesh.vertices.size());
    for (scene::Vertex v : raw.vertices) {
        v.colour = colour;
        if (mods && !mods->empty()) {
            const shader::TexCoord moved =
                shader::applyTexMods(*mods, {v.uv[0], v.uv[1]}, seconds, v.pos);
            v.uv[0] = moved.u;
            v.uv[1] = moved.v;
        }
        group.mesh.vertices.push_back(v);
    }
    for (uint16_t index : raw.indices) {
        group.mesh.indices.push_back(static_cast<uint16_t>(base + index));
    }
}

// --- Modelle (Emitter mit useModel) ------------------------------------------

// Ein Emitter-Modell, das in diesem Bild gezeichnet wird.
//
// Erst gesammelt und nach allen anderen Primitiven gebaut: die Beleuchtung
// braucht die Lichter DIESES Bildes (R_SetupEntityLighting rechnet alle
// dlights der Szene ein), und die stehen erst nach dem Durchlauf fest.
struct PendingModel {
    const Live* item = nullptr;
    camera::Vec3 position;
    float radius = 0.0f;
    float shaderSeconds = 0.0f;
};

// Was R_SetupEntityLighting (tr_light.cpp) fuer die Entity ausrechnet.
struct EntityLight {
    float ambient[3]{0.0f, 0.0f, 0.0f};
    float directed[3]{0.0f, 0.0f, 0.0f};
    // ent->lightDir: in Modellkoordinaten, mit den SKALIERTEN Achsen gerechnet.
    camera::Vec3 dir;
    // backEnd.ori.viewOrigin: das Auge in Modellkoordinaten (R_RotateForEntity).
    camera::Vec3 view;
};

// Die Beleuchtung einer Modell-Entity im Fall ohne Lichtgitter — der Fall
// des Testraums, der keine Karte und damit kein Lichtgitter hat:
//
//     ambientLight = directedLight = identityLight * 150;
//     lightDir = tr.sunDirection;      // RE_LoadWorldMap: (0.45, 0.3, 0.9) normiert
//     ambientLight += identityLight * 32;   // "give everything a minimum light add"
//
// dann die dynamischen Lichter (Light-Primitive), jedes mit
//
//     d = DLIGHT_AT_RADIUS * radius^2 / max( Abstand, DLIGHT_MINIMUM_RADIUS )^2;
//     directedLight += d * color;  lightDir += d * Richtung;
//
// Umgebungslicht auf 255 begrenzt, lightDir normiert und in die Achsen der
// Entity gedreht. Die Achsen sind mit size skaliert (CEmitter::Draw), und
// die Engine normiert sie hier NICHT — ein Brocken mit size 2 wird doppelt so
// stark gerichtet beleuchtet. So steht es im Quelltext, so machen wir es.
// identityLight ist 1 (r_overBrightBits 0, wie ueberall hier).
EntityLight entityLight(const camera::Vec3& origin, const camera::Vec3 axis[3],
                        const camera::Vec3& eye, const std::vector<DynamicLight>& lights) {
    constexpr float kGridless = 150.0f;
    constexpr float kMinimumAdd = 32.0f;
    constexpr float kAtRadius = 16.0f;      // DLIGHT_AT_RADIUS
    constexpr float kMinimumRadius = 16.0f; // DLIGHT_MINIMUM_RADIUS
    EntityLight out;
    for (int k = 0; k < 3; ++k) {
        out.ambient[k] = kGridless + kMinimumAdd;
        out.directed[k] = kGridless;
    }
    const camera::Vec3 sun = camera::normalise({0.45f, 0.3f, 0.9f});
    // VectorScale( ent->lightDir, VectorLength( directedLight ), lightDir ).
    camera::Vec3 lightDir =
        sun * std::sqrt(out.directed[0] * out.directed[0] + out.directed[1] * out.directed[1] +
                        out.directed[2] * out.directed[2]);
    for (const DynamicLight& light : lights) {
        camera::Vec3 dir = light.origin - origin;
        float d = camera::length(dir);
        dir = d > 0.0f ? dir * (1.0f / d) : camera::Vec3{};
        const float power = kAtRadius * light.radius * light.radius;
        if (d < kMinimumRadius) d = kMinimumRadius;
        d = power / (d * d);
        for (int k = 0; k < 3; ++k) out.directed[k] += d * light.rgb[k];
        lightDir = lightDir + dir * d;
    }
    for (float& a : out.ambient) a = std::min(a, 255.0f);

    lightDir = camera::normalise(lightDir);
    out.dir = {camera::dot(lightDir, axis[0]), camera::dot(lightDir, axis[1]),
               camera::dot(lightDir, axis[2])};

    // R_RotateForEntity: viewOrigin = dot( delta, axis ) / |axis[0]| bei
    // nonNormalizedAxes.
    const float axisLength = camera::length(axis[0]);
    const float inverse = axisLength > 0.0f ? 1.0f / axisLength : 0.0f;
    const camera::Vec3 delta = eye - origin;
    out.view = {camera::dot(delta, axis[0]) * inverse, camera::dot(delta, axis[1]) * inverse,
                camera::dot(delta, axis[2]) * inverse};
    return out;
}

// RB_CalcDiffuseColor (tr_shade_calc.cpp) fuer einen Eckpunkt: Umgebungslicht,
// plus gerichtetes Licht, wo die Normale zum Licht zeigt. Q_ftol schneidet ab.
uint32_t diffuseColour(const EntityLight& light, const float normal[3]) {
    const float incoming =
        normal[0] * light.dir.x + normal[1] * light.dir.y + normal[2] * light.dir.z;
    uint32_t rgb[3];
    for (int k = 0; k < 3; ++k) {
        const float value = incoming <= 0.0f ? light.ambient[k]
                                             : light.ambient[k] + incoming * light.directed[k];
        rgb[k] = static_cast<uint32_t>(std::clamp(static_cast<int>(value), 0, 255));
    }
    return rgb[0] | (rgb[1] << 8) | (rgb[2] << 16) | (255u << 24);
}

// RB_CalcSpecularAlpha: bei einem Modell mit ent->lightDir (nicht mit dem
// festen Ersatzlicht), Glanz hoch vier.
uint32_t specularAlpha(const EntityLight& light, const md3::Vertex& v) {
    const camera::Vec3 n{v.normal[0], v.normal[1], v.normal[2]};
    const float d = 2.0f * camera::dot(n, light.dir);
    const camera::Vec3 reflected = n * d - light.dir;
    const camera::Vec3 viewer = light.view - camera::Vec3{v.pos[0], v.pos[1], v.pos[2]};
    const float viewerLength = camera::length(viewer);
    if (viewerLength <= 0.0f) return 0;
    float l = camera::dot(reflected, viewer) / viewerLength;
    if (l < 0.0f) return 0;
    l = l * l;
    l = l * l;
    return static_cast<uint32_t>(std::min(static_cast<int>(l * 255.0f), 255));
}

// RB_CalcEnvironmentTexCoords: Spiegelvektor zum Auge, in Modellkoordinaten.
void environmentUv(const EntityLight& light, const md3::Vertex& v, float uv[2]) {
    const camera::Vec3 n{v.normal[0], v.normal[1], v.normal[2]};
    const camera::Vec3 viewer =
        camera::normalise(light.view - camera::Vec3{v.pos[0], v.pos[1], v.pos[2]});
    const float d = camera::dot(n, viewer);
    const camera::Vec3 reflected = n * (2.0f * d) - viewer;
    uv[0] = 0.5f + reflected.y * 0.5f;
    uv[1] = 0.5f - reflected.z * 0.5f;
}

// Haengt Modellgeometrie an eine Gruppe — wie appendStage, aber die Farbe
// steht schon JE ECKPUNKT darin (Beleuchtung), statt fuer alle gleich.
bool appendVertexColoured(DrawGroup& group, const scene::Mesh& raw,
                          const std::vector<shader::TexMod>* mods, float seconds) {
    if (group.mesh.vertices.size() + raw.vertices.size() > 65535u) return false;
    const auto base = static_cast<uint16_t>(group.mesh.vertices.size());
    for (scene::Vertex v : raw.vertices) {
        if (mods && !mods->empty()) {
            const shader::TexCoord moved =
                shader::applyTexMods(*mods, {v.uv[0], v.uv[1]}, seconds, v.pos);
            v.uv[0] = moved.u;
            v.uv[1] = moved.v;
        }
        group.mesh.vertices.push_back(v);
    }
    for (uint16_t index : raw.indices) {
        group.mesh.indices.push_back(static_cast<uint16_t>(base + index));
    }
    return true;
}

}  // namespace

DrawList System::build(float nowMs, const camera::Vec3& right,
                       const camera::Vec3& up, const ShaderLookup& shaders,
                       const View* view) const {
    DrawList out;
    static const std::vector<shader::TexMod> noMods;

    // Blickrichtung: up x right zeigt in die Szene (rechte Hand, right = x,
    // up = y, Blick entlang -z). Ohne Kamera steht das Auge sehr weit
    // zurueck — dann sind alle Sichtlinien parallel.
    camera::Vec3 viewForward = camera::normalise(camera::cross(up, right));
    if (camera::length(viewForward) < 1e-5f) viewForward = {0.0f, 0.0f, -1.0f};

    // Die Shaderabfragen je Name nur einmal je Bild — bei zweihundert Funken
    // mit demselben Shader waeren es sonst zweihundert Suchen.
    std::map<std::string, ShaderDraw> lookups;
    const auto lookup = [&](const std::string& name) -> const ShaderDraw& {
        const auto found = lookups.find(name);
        if (found != lookups.end()) return found->second;
        return lookups.emplace(name, shaders ? shaders(name) : ShaderDraw{}).first->second;
    };

    GroupCollector groups(out.groups);
    scene::Mesh raw;
    std::vector<PendingModel> pendingModels;

    for (const auto& item : live_) {
        // Fuer die Statuszeile: noch wartend, und Abdruecke bis jetzt.
        if (item.spawnMs > nowMs) ++out.scheduled;
        if (item.type == PrimitiveType::Decal && item.spawnMs <= nowMs) ++out.marks;

        if (!item.aliveAt(nowMs)) continue;
        ++out.alive;

        const float endMs = item.deathMs;
        const camera::Vec3 position = item.positionAt(nowMs);

        // Die Kurven, mit dem `random`-Anteil dieses Bildes.
        const auto value = [&](const curve::Curve& c, float parm,
                               Live::RandomChannel channel) {
            return curve::evaluate(c, nowMs, item.spawnMs, endMs, parm,
                                   item.randomAt(channel, nowMs));
        };
        const float size = value(item.size, item.sizeParm, Live::kRandomSize);
        const float r = value(item.rgb[0], item.rgbParm, Live::kRandomRgb);
        const float g = value(item.rgb[1], item.rgbParm, Live::kRandomRgb);
        const float b = value(item.rgb[2], item.rgbParm, Live::kRandomRgb);

        // Alpha: CParticle::UpdateAlpha mischt OHNE den Zufall, schneidet auf
        // 0..1 und multipliziert ERST DANN mit Q_flrand(0,1):
        //
        //     perc1 = (mAlphaStart * perc1) + (mAlphaEnd * (1.0f - perc1));
        //     if ( perc1 < 0.0f ) perc1 = 0.0f; else if ( perc1 > 1.0f ) perc1 = 1.0f;
        //     if ( (mFlags & FX_ALPHA_RAND) ) perc1 = Q_flrand(0.0f, 1.0f) * perc1;
        //
        // Bei size/rgb/length steht der Zufall dagegen auf dem Anteil. Beim
        // Alpha dort zu wuerfeln heisst: `alpha { flags random }` mit start ==
        // end tut nichts, im Spiel flackert es zwischen 0 und dem Wert.
        //
        // Und: ohne Kurvenart ist Alpha einfach `start` — auch null. Hier
        // stand `alpha.start != 0 ? ... : 1.0`, ein Alpha von null wurde also
        // zu voll. Im Spiel ist so ein Teilchen unsichtbar.
        curve::Curve alphaCurve = item.alpha;
        const bool alphaRandom = (alphaCurve.flags & curve::kRandom) != 0;
        alphaCurve.flags &= ~curve::kRandom;
        float alpha = curve::evaluate(alphaCurve, nowMs, item.spawnMs, endMs, item.alphaParm, 1.0f);
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        if (alphaRandom) alpha *= item.randomAt(Live::kRandomAlpha, nowMs);

        // Die Farbe der Primitive (mRefEnt.shaderRGBA). Wohin das Ausblenden
        // wirkt, entscheidet useAlpha (CParticle::UpdateAlpha):
        //
        //   mit useAlpha:  rgb bleibt, shaderRGBA[3] = alpha * 255
        //   ohne:          rgb *= alpha, und shaderRGBA[3] bleibt, was es war —
        //                  0, denn CEffect() setzt mRefEnt mit memset auf null.
        //
        // Das zweite ist der Grund fuer Ravens Abschnitt "If You Don't See
        // Anything": ein alphagemischter Shader mit alphaGen vertex zeichnet
        // ohne useAlpha NICHTS. Wir hatten dort 255 eingesetzt und zeigten,
        // was das Spiel nicht zeigt.
        const uint32_t colour = item.useAlpha
                                    ? packRgba(r, g, b, alpha)
                                    : packRgba(r * alpha, g * alpha, b * alpha, 0.0f);

        // Die Drehung waechst ueber die Lebensdauer — und zwar ZEHNMAL so
        // schnell, wie "Grad je Sekunde" vermuten laesst.
        //
        // `CParticle::UpdateRotation` in FxPrimitives.h:
        //
        //     mRefEnt.rotation += theFxHelper.mFrameTime * 0.01f * mRotationDelta;
        //
        // `mFrameTime` ist in MILLISEKUNDEN. Aus `ms * 0.01` wird also
        // `Sekunden * 10`: `rotationDelta 1` sind zehn Grad je Sekunde.
        constexpr float kRotationScale = 10.0f;
        const float seconds = (nowMs - item.spawnMs) * 0.001f;
        const float rotation =
            item.rotation + item.rotationDelta * seconds * kRotationScale;

        // Die Shaderzeit: `refdef.floatTime - e.shaderTime` (tr_backend.cpp).
        // shaderTime ist nur mit `setShaderTime` der Entstehungszeitpunkt
        // (CEffect::SetTimeStart), sonst null — dann laeuft die Uhr des
        // Spiels, hier die des Effekts. Davon haengen Bildfolgen, tcMod und
        // Wellen ab. 38 von 39 Bildfolgen im Grundspiel setzen das Flag: jede
        // verzoegerte Explosion beginnt dann bei ihrem ERSTEN Bild.
        const float shaderSeconds = (item.flags & kFlagSetShaderTime) != 0
                                        ? (nowMs - item.spawnMs) * 0.001f
                                        : nowMs * 0.001f;

        raw.vertices.clear();
        raw.indices.clear();
        uint32_t rawColour = colour;
        bool built = false;   // gehoert in die Zaehlung "gezeichnet"
        bool full = false;    // kein Platz mehr

        // Ein Auge fuer die Baender: die Kamera, oder eine sehr ferne in
        // Blickrichtung.
        const camera::Vec3 eye =
            view ? view->eye : position - viewForward * 1.0e6f;

        switch (item.type) {
            case PrimitiveType::Line: {
                // RB_SurfaceLine: ein Band von origin nach origin2, quer zur
                // Sichtlinie, halbe Breite = size. Vorher eine 1-Pixel-Linie
                // ohne Textur — jeder Strahl, jede Blasterspur war ein Haar.
                built = true;
                full = !addLineQuad(raw, position, item.origin2,
                                    lineSide(position, item.origin2, eye), size, colour);
                break;
            }
            case PrimitiveType::Electricity: {
                // RB_SurfaceElectricity. Die Formflags stehen auf den Bits von
                // useModel/usePhysics/useBBox (CElectricity::Initialize).
                BoltShape shape;
                shape.radius = size;
                shape.chaos = item.chaos;
                shape.taper = (item.flags & kFlagElectricityTaper) != 0;
                shape.branch = (item.flags & kFlagElectricityBranch) != 0;
                if ((item.flags & kFlagElectricityGrow) != 0) {
                    // perc = 1 - ( endTime - refdef.time ) / duration
                    const float life = item.deathMs - item.spawnMs;
                    shape.growPerc = life > 0.0f ? (nowMs - item.spawnMs) / life : 1.0f;
                }
                // Die grobe Form aus boltSeed steht; das Mikrozittern aendert
                // sich jedes Bild (60 je Sekunde).
                const auto tick = static_cast<unsigned>(std::floor(nowMs * 0.06f));
                built = true;
                full = !addElectricity(raw, position, item.origin2, eye, shape, item.boltSeed,
                                       mix32(item.seed ^ (tick * 2654435761u)) | 1u, colour);
                break;
            }
            case PrimitiveType::Cylinder: {
                // CCylinder::Draw: oldorigin = origin + length * axis[0].
                // RB_SurfaceCylinder: size2 (`backlerp`) am Ursprung, size
                // (`radius`) am fernen Ende. Vorher halbe Radien und die
                // Enden vertauscht — Kegel standen auf dem Kopf.
                const float length = value(item.length, item.lengthParm, Live::kRandomLength);
                const float size2 = value(item.size2, item.size2Parm, Live::kRandomSize2);
                camera::Vec3 axis = camera::normalise(item.forward);
                if (camera::length(axis) < 1e-5f) axis = {0.0f, 0.0f, 1.0f};
                const camera::Vec3 middle = position + axis * (length * 0.5f);
                const int segments = cylinderSegments(
                    camera::length(middle - eye), view ? view->fovXDegrees : 90.0f);
                built = true;
                full = !addCylinder(raw, position, axis, length, size2, size, colour,
                                    view ? segments : 16);
                break;
            }
            case PrimitiveType::OrientedParticle: {
                camera::Vec3 normal = camera::normalise(item.forward);
                if (camera::length(normal) < 1e-5f) normal = {0.0f, 0.0f, 1.0f};
                built = true;
                full = !addOrientedQuad(raw, position, normal, size, rotation, colour);
                break;
            }
            case PrimitiveType::Decal: {
                // CG_ImpactMark (cg_marks.cpp), aufgerufen in FxScheduler.cpp
                // mit den STARTwerten: rgb start, alpha start, size start, ein
                // rotation-Wert. Keine Kurven, kein rotationDelta, kein useAlpha:
                //     colors[3] = alpha * 255;
                // Die letzte Sekunde blendet das Alpha aus (alphaFade):
                //     fade = 255 * t / MARK_FADE_TIME;  modulate[3] = fade;
                const float radius = item.size.start;
                if (radius <= 0.0f) break;
                uint32_t markAlpha = static_cast<uint32_t>(
                    std::clamp(item.alpha.start, 0.0f, 1.0f) * 255.0f);
                const float remaining = item.deathMs - nowMs;
                constexpr float kMarkFadeMs = 1000.0f;
                if (remaining < kMarkFadeMs) {
                    markAlpha = static_cast<uint32_t>(255.0f * std::max(remaining, 0.0f) /
                                                      kMarkFadeMs);
                }
                rawColour = packRgba(item.rgb[0].start, item.rgb[1].start,
                                     item.rgb[2].start, 0.0f) |
                            (markAlpha << 24);

                // Die Achsen des Abdrucks:
                //     axis[0] = dir; PerpendicularVector( axis[1], axis[0] );
                //     RotatePointAroundVector( axis[2], axis[0], axis[1], orientation );
                //     CrossProduct( axis[0], axis[2], axis[1] );
                // und die Ecken o -/+ r*axis[1] -/+ r*axis[2] mit s, t von 0
                // bis 1 entlang axis[1], axis[2].
                camera::Vec3 dir = camera::normalise(item.forward);
                if (camera::length(dir) < 1e-5f) dir = {0.0f, 0.0f, 1.0f};
                // PerpendicularVector: die kleinste Komponente, auf die
                // Ebene projiziert (q_math.cpp).
                camera::Vec3 tempVec{0.0f, 0.0f, 0.0f};
                {
                    const float ax = std::fabs(dir.x), ay = std::fabs(dir.y),
                                az = std::fabs(dir.z);
                    if (ax <= ay && ax <= az) tempVec.x = 1.0f;
                    else if (ay <= az) tempVec.y = 1.0f;
                    else tempVec.z = 1.0f;
                }
                camera::Vec3 axis1 =
                    camera::normalise(tempVec - dir * camera::dot(tempVec, dir));
                const float rad = item.rotation * kPi / 180.0f;
                camera::Vec3 axis2 = axis1 * std::cos(rad) +
                                     camera::cross(dir, axis1) * std::sin(rad);
                axis1 = camera::cross(dir, axis2);

                // Die Flaeche, auf die er faellt: CM_MarkFragments sucht in
                // 20 Einheiten entgegen `dir` und nimmt nur Flaechen, deren
                // Normale nicht mehr als 60 Grad von `dir` weg zeigt
                // (dot(normal, -dir) <= -0.5). Ohne Raum: dort, wo er ist —
                // im Editor soll man ihn ohne Wand trotzdem sehen.
                camera::Vec3 centre = position;
                const sim::Plane* surface = nullptr;
                float nearest = 1e9f;
                for (const auto& plane : planes_) {
                    const float facing = camera::dot(plane.normal, dir);
                    if (facing < 0.5f) continue;
                    const float distance = camera::dot(plane.normal, position) - plane.distance;
                    if (distance - 20.0f * facing > 0.0f || distance < -1.0f) continue;
                    if (distance < nearest) {
                        nearest = distance;
                        surface = &plane;
                    }
                }
                if (!planes_.empty() && !surface) break;  // im Spiel: kein Abdruck
                const camera::Vec3 corners[4] = {
                    centre - axis1 * radius - axis2 * radius,
                    centre + axis1 * radius - axis2 * radius,
                    centre + axis1 * radius + axis2 * radius,
                    centre - axis1 * radius + axis2 * radius,
                };
                const float uvs[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
                if (!hasRoomForQuad(raw)) { full = true; break; }
                for (int i = 0; i < 4; ++i) {
                    camera::Vec3 p = corners[i];
                    if (surface) {
                        // Auf die Flaeche gelegt.
                        p = p - surface->normal *
                                    (camera::dot(surface->normal, p) - surface->distance);
                    }
                    pushVertex(raw, p, uvs[i][0], uvs[i][1], rawColour);
                }
                for (uint16_t offset : {uint16_t{0}, uint16_t{1}, uint16_t{2},
                                        uint16_t{0}, uint16_t{2}, uint16_t{3}}) {
                    raw.indices.push_back(offset);
                }
                built = true;
                break;
            }
            case PrimitiveType::Tail: {
                // Ein Band entgegen der Flugrichtung, so lang wie `length`,
                // halbe Breite = size — CTail ist ein RT_LINE.
                const float length = value(item.length, item.lengthParm, Live::kRandomLength);
                // Die Richtung kommt aus der TATSAECHLICHEN Bewegung, nicht
                // aus der Anfangsgeschwindigkeit (`CTail::CalcNewEndpoint`:
                // VectorSubtract( mOldOrigin, mOrigin1, temp )). Drei
                // Millisekunden zurueck, wie die Engine es im angehefteten
                // Fall rechnet.
                constexpr float kLookBackMs = 3.0f;
                const float earlier = std::max(item.spawnMs, nowMs - kLookBackMs);
                camera::Vec3 direction = item.positionAt(earlier) - position;
                if (camera::length(direction) < 1e-5f) direction = item.velocity * -1.0f;
                direction = camera::normalise(direction);
                if (camera::length(direction) < 1e-5f) direction = {0.0f, 0.0f, -1.0f};
                const camera::Vec3 end = position + direction * length;
                built = true;
                full = !addLineQuad(raw, position, end, lineSide(position, end, eye), size,
                                    colour);
                break;
            }
            case PrimitiveType::Light: {
                // Ein Licht zeichnet NICHTS. CLight::Draw ist ein einziger
                // Aufruf: AddLightToScene( mOrigin1, mRefEnt.radius, r, g, b ).
                // Es erhellt die Umgebung — die Liste geht an den Raum.
                DynamicLight light;
                light.origin = position;
                light.radius = size;
                light.rgb[0] = r;
                light.rgb[1] = g;
                light.rgb[2] = b;
                out.lights.push_back(light);
                break;
            }
            case PrimitiveType::ScreenFlash: {
                // CFlash::Draw: ein Sprite 8 Einheiten vor dem Auge, so gross,
                // dass es das Sichtfeld fuellt, Farbe aus der rgb-Kurve,
                // Alpha 255. CFlash::Init daempft nach Lage und Abstand:
                //     mod = dot( normalize(origin - vieworg), viewaxis[0] );
                //     if ( dis > 600 || ( mod < 0.5 && dis > 100 )) mod = 0;
                //     else if ( mod < 0.5 && dis <= 100 ) mod += 1.1;
                //     mod *= 1 - dis^2 / 600^2;
                // Die Engine rechnet das einmal beim Ausloesen; die Kamera des
                // Editors bewegt sich, also hier mit der aktuellen.
                if (!view) break;
                const camera::Vec3 toFlash = position - view->eye;
                const float dis = camera::length(toFlash);
                float mod = dis > 0.0f ? camera::dot(toFlash * (1.0f / dis), viewForward) : 1.0f;
                if (dis > 600.0f || (mod < 0.5f && dis > 100.0f)) {
                    mod = 0.0f;
                } else if (mod < 0.5f && dis <= 100.0f) {
                    mod += 1.1f;
                }
                mod *= 1.0f - (dis * dis) / (600.0f * 600.0f);
                rawColour = packRgba(std::clamp(r * mod, 0.0f, 1.0f),
                                     std::clamp(g * mod, 0.0f, 1.0f),
                                     std::clamp(b * mod, 0.0f, 1.0f), 1.0f);
                constexpr float kFlashDistance = 8.0f;
                const float radius =
                    kFlashDistance * std::tan(view->fovXDegrees * 0.5f * kPi / 180.0f);
                built = true;
                full = !addBillboard(raw, view->eye + viewForward * kFlashDistance, right, up,
                                     radius, 0.0f, rawColour);
                break;
            }
            case PrimitiveType::Particle: {
                built = true;
                full = !addBillboard(raw, position, right, up, size, rotation, colour);
                break;
            }
            case PrimitiveType::Emitter:
                // CEmitter::Draw zeichnet nur ein Modell, und nur mit
                // FX_ATTACHED_MODEL (useModel); ein Sprite gibt es nicht.
                // Hier stand der Standardzweig — jeder Brocken war ein
                // weisser Fleck. Gebaut wird nach dem Durchlauf (PendingModel).
                //
                // size ist der Massstab: UpdateSize schreibt mRefEnt.radius,
                // und Draw skaliert damit die Achsen ("ensure that we are
                // sized"). rgb und alpha wirken nicht — UpdateRGB und
                // UpdateAlpha sind in CEmitter::Update auskommentiert.
                if (item.model != nullptr) {
                    pendingModels.push_back({&item, position, size, shaderSeconds});
                }
                break;
            case PrimitiveType::Sound:
            case PrimitiveType::CameraShake:
            case PrimitiveType::FxRunner:
                break;
        }

        if (full) {
            ++out.skipped;
            continue;
        }
        if (!built || raw.vertices.empty()) continue;

        // Die 16-Bit-Grenze gilt fuer die GESAMMELTE Geometrie eines Shaders,
        // nicht fuer das einzelne Teilchen. Was nicht mehr passt, wird
        // gezaehlt statt still weggelassen (siehe DrawList::skipped).
        scene::Mesh& existing = out.byTexture[item.shader];
        if (existing.vertices.size() + raw.vertices.size() > 65535u) {
            ++out.skipped;
            continue;
        }
        ++out.drawn;
        if (item.useAlpha) out.alphaShaders.insert(item.shader);

        // Die rohe Geometrie, mit der Farbe der Primitive. Fuer Abnehmer, die
        // nur nach Shadern gruppieren (Reichweite, Pruefungen); die alten
        // Einzelangaben aus ShaderDraw (tcMod, Wellen) wirken hier wie bisher.
        const ShaderDraw& info = lookup(item.shader);
        {
            scene::Mesh& target = existing;
            DrawGroup legacy;
            legacy.mesh = std::move(target);
            appendStage(legacy, raw, nullptr, info.texMods ? info.texMods : &noMods,
                        rawColour, shaderSeconds, info.rgbWave, info.alphaWave);
            target = std::move(legacy.mesh);
        }

        // Und die Gruppen, wie die Engine sie zeichnet.
        if (info.missing) {
            // RE_RegisterShader gibt fuer einen Shader ohne Bild 0 zurueck,
            // gezeichnet wird tr.defaultShader: das graue Kaestchen
            // (R_CreateDefaultImage), undurchsichtig, Farbe egal.
            DrawGroup& group = groups.get(item.shader, 0, "$default");
            group.sort = shader::kSortOpaque;
            const uint32_t white = 0xFFFFFFFFu;
            appendStage(group, raw, nullptr, nullptr, white, shaderSeconds);
            continue;
        }
        if (info.definition && !info.definition->stages.empty()) {
            const shader::Shader& def = *info.definition;
            const float sort = shader::sortValue(def);
            for (size_t s = 0; s < def.stages.size(); ++s) {
                const shader::Stage& stage = def.stages[s];
                bool clamp = false;
                const std::string image = stageImage(stage, shaderSeconds, clamp);
                if (image.empty()) continue;
                DrawGroup& group = groups.get(item.shader, static_cast<int>(s),
                                              std::string(assets::kImagePrefix) + image);
                configureStageGroup(group, stage, sort, clamp);
                appendStage(group, raw, &stage, &stage.texMods, rawColour, shaderSeconds);
            }
            continue;
        }
        // Kein Shaderblock: der Ersatzshader der Engine fuer ein nacktes Bild
        // (R_FindShader, LIGHTMAP_2D): rgbGen vertex, alphaGen vertex,
        // GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA, ohne Tiefentest.
        DrawGroup& group = groups.get(item.shader, 0, item.shader);
        group.blended = true;
        group.src = shader::BlendFactor::SrcAlpha;
        group.dst = shader::BlendFactor::OneMinusSrcAlpha;
        group.depthWrite = false;
        group.depthTest = false;
        group.sort = shader::kSortBlend0;
        appendStage(group, raw, nullptr, info.texMods, rawColour, shaderSeconds, info.rgbWave,
                    info.alphaWave);
    }

    // Die Modelle der Emitter, jetzt mit allen Lichtern dieses Bildes.
    for (const PendingModel& pending : pendingModels) {
        const Live& item = *pending.item;
        // AnglesToAxis, dann mit size skaliert (CEmitter::Draw,
        // nonNormalizedAxes): Modellpunkt p liegt bei origin + sum(p[k] * axis[k]).
        camera::Vec3 axis[3];
        anglesToAxis(item.anglesAt(nowMs), axis);
        for (camera::Vec3& a : axis) a = a * pending.radius;
        const camera::Vec3 eye =
            view ? view->eye : pending.position - viewForward * 1.0e6f;
        const EntityLight light = entityLight(pending.position, axis, eye, out.lights);

        bool overflow = false;
        bool any = false;
        for (const md3::Surface& surface : item.model->surfaces) {
            if (surface.vertices.empty() || surface.indices.empty()) continue;
            // RB_SurfaceMesh zeichnet mit dem ersten Shader der Flaeche. Ohne
            // einen nimmt R_AddMD3Surfaces tr.defaultShader.
            const std::string name =
                surface.shaders.empty() ? std::string() : md3::shaderName(surface.shaders[0]);

            // Die Flaeche in Weltkoordinaten, gefaerbt mit der Beleuchtung
            // (rgbGen lightingDiffuse — das hat jede Modellflaeche ohne
            // eigenen Shaderblock, und die meisten mit).
            raw.vertices.clear();
            raw.indices.clear();
            for (const md3::Vertex& v : surface.vertices) {
                const camera::Vec3 world = pending.position + axis[0] * v.pos[0] +
                                           axis[1] * v.pos[1] + axis[2] * v.pos[2];
                pushVertex(raw, world, v.st[0], v.st[1], diffuseColour(light, v.normal));
            }
            raw.indices = surface.indices;

            scene::Mesh& existing = out.byTexture[name];
            if (existing.vertices.size() + raw.vertices.size() > 65535u) {
                overflow = true;
                continue;
            }
            {
                DrawGroup legacy;
                legacy.mesh = std::move(existing);
                appendVertexColoured(legacy, raw, nullptr, pending.shaderSeconds);
                existing = std::move(legacy.mesh);
            }
            any = true;

            static const ShaderDraw kNoShader = [] {
                ShaderDraw draw;
                draw.missing = true;
                return draw;
            }();
            const ShaderDraw& info = name.empty() ? kNoShader : lookup(name);
            if (info.missing) {
                // tr.defaultShader: das graue Kaestchen, undurchsichtig.
                DrawGroup& group = groups.get(name, 0, "$default");
                group.sort = shader::kSortOpaque;
                appendStage(group, raw, nullptr, nullptr, 0xFFFFFFFFu, pending.shaderSeconds);
                continue;
            }
            if (info.definition && !info.definition->stages.empty()) {
                const shader::Shader& def = *info.definition;
                const float sort = shader::sortValue(def);
                for (size_t s = 0; s < def.stages.size(); ++s) {
                    const shader::Stage& stage = def.stages[s];
                    bool clamp = false;
                    const std::string image = stageImage(stage, pending.shaderSeconds, clamp);
                    if (image.empty()) continue;
                    DrawGroup& group = groups.get(name, static_cast<int>(s),
                                                  std::string(assets::kImagePrefix) + image);
                    configureStageGroup(group, stage, sort, clamp);

                    // ComputeColors mit der Entity-Farbe der Emitter: null —
                    // CEffect() leert mRefEnt mit memset, und UpdateRGB/
                    // UpdateAlpha laufen beim Emitter nicht.
                    const uint32_t fixed = stageColour(stage, 0u, pending.shaderSeconds);
                    const bool diffuse =
                        shader::effectiveColorGen(stage) == shader::ColorGen::LightingDiffuse;
                    const bool specular =
                        shader::effectiveAlphaGen(stage) == shader::AlphaGen::LightingSpecular;
                    std::string tcGen = stage.tcGen;
                    for (char& c : tcGen) {
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    }
                    const bool environment = tcGen == "environment";

                    scene::Mesh staged = raw;
                    for (size_t i = 0; i < staged.vertices.size(); ++i) {
                        scene::Vertex& target = staged.vertices[i];
                        const md3::Vertex& v = surface.vertices[i];
                        uint32_t colour = diffuse ? (raw.vertices[i].colour & 0x00FFFFFFu) |
                                                        (fixed & 0xFF000000u)
                                                  : fixed;
                        if (specular) {
                            colour = (colour & 0x00FFFFFFu) | (specularAlpha(light, v) << 24);
                        }
                        target.colour = colour;
                        if (environment) environmentUv(light, v, target.uv);
                    }
                    if (!appendVertexColoured(group, staged, &stage.texMods,
                                              pending.shaderSeconds)) {
                        overflow = true;
                    }
                }
                continue;
            }
            // Kein Shaderblock: der Ersatzshader, den R_FindShader fuer ein
            // Modell anlegt (lightmapIndex LIGHTMAP_NONE, "dynamic colors at
            // vertexes"):
            //
            //     stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
            //     stages[0].stateBits = GLS_DEFAULT;    // undurchsichtig, schreibt Tiefe
            //
            // Also NICHT der Ersatz fuer Effektbilder (LIGHTMAP_2D, gemischt,
            // ohne Tiefentest), den die Teilchen bekommen.
            DrawGroup& group = groups.get(name, 0, std::string(assets::kImagePrefix) + name);
            group.blended = false;
            group.depthWrite = true;
            group.depthTest = true;
            group.sort = shader::kSortOpaque;
            if (!appendVertexColoured(group, raw, nullptr, pending.shaderSeconds)) overflow = true;
        }
        if (overflow) ++out.skipped;
        if (any) ++out.drawn;
    }

    // Die Reihenfolge der Engine: nach Sortierstufe, dann nach dem Shader
    // (wer zuerst da war), dann die Stufen eines Shaders nacheinander
    // (R_SortDrawSurfs, RB_StageIteratorGeneric). Vorher: alphabetisch nach
    // Shadername — ein Rauch, dessen Name hinter dem eines additiven Glimmens
    // sortierte, deckte es zu.
    std::stable_sort(out.groups.begin(), out.groups.end(),
                     [](const DrawGroup& a, const DrawGroup& b) {
                         if (a.sort != b.sort) return a.sort < b.sort;
                         if (a.firstSeen != b.firstSeen) return a.firstSeen < b.firstSeen;
                         return a.stage < b.stage;
                     });
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
        // Linien, Schweife und Blitze liegen seit sie Baender sind ebenfalls
        // in byTexture — eine eigene Linienliste gibt es nicht mehr.

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
