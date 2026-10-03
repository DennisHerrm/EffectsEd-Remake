#include "efx/sim.h"

#include <algorithm>
#include <cmath>

namespace efx::sim {

float Random::next() {
    // Eigener Generator statt std::rand: der ist zwischen Laufzeitbibliotheken
    // verschieden, und eine Vorschau soll auf jedem Rechner gleich aussehen.
    state_ = state_ * 1664525u + 1013904223u;
    return static_cast<float>((state_ >> 8) & 0xFFFFFF) /
           static_cast<float>(0x1000000);
}

float Random::range(float low, float high) { return low + (high - low) * next(); }

float Random::pick(const Range& r) {
    if (!r.set) return 0.0f;
    if (!r.ranged || r.min == r.max) return r.min;
    return range(std::min(r.min, r.max), std::max(r.min, r.max));
}

// Zum `lateTime`-Ausgleich der Engine, den wir BEWUSST nicht nachbauen.
//
// FxScheduler.cpp, in CreateEffect:
//
//     if ( lateTime > 0 ) {
//         float ftime = lateTime * 0.001f;
//         float time2 = ftime * ftime * 0.5f;
//         VectorMA( vel, ftime, accel, vel );
//         for ( int i = 0 ; i < 3 ; i++ )
//             org[i] = org[i] + ftime * vel[i] + time2 * vel[i];
//     }
//
// `lateTime` ist `theFxHelper.mTime - effect->mStartTime`: der Rueckstand
// zum Bildraster. Ein geplanter Effekt startet im Spiel erst im ersten Bild
// NACH seinem Zeitpunkt, also bis zu einer Bilddauer zu spaet, und die Engine
// rueckt ihn entlang seiner Bahn vor.
//
// Bei uns ist der Rueckstand immer null: wir rechnen die Lage zum exakten
// Zeitpunkt, nicht zum naechsten Bild. Den Ausgleich nachzubauen hiesse, erst
// ein Bildraster einzufuehren — und damit waere die Vorschau wieder von der
// Bildrate abhaengig, genau das, was wir an mehreren Stellen vermieden haben.
//
// Nebenbei: die Zeile ist fehlerhaft. `time2 * vel[i]` muesste `time2 *
// accel[i]` heissen — sonst geht die Geschwindigkeit zweimal ein und die
// Beschleunigung gar nicht. Wirksam wird das ueber hoechstens eine Bilddauer,
// also wenige Millimeter. Ein Grund mehr, es zu lassen.
std::vector<Spawn> schedule(const Effect& effect, Random& random,
                            const std::vector<bool>& enabledMask) {
    std::vector<Spawn> spawns;

    // Hoechstens 24 Segmente — mehr nimmt die Engine nicht an.
    //
    //     #define FX_MAX_EFFECT_COMPONENTS 24
    //
    //     void CFxScheduler::AddPrimitiveToEffect( ... ) {
    //         if ( ct >= FX_MAX_EFFECT_COMPONENTS ) {
    //             theFxHelper.Print( "FxScheduler:  Error--too many primitives" );
    //         } else { ... }
    //     }
    //
    // Die ueberzaehligen werden beim Laden STILL verworfen — der Effekt
    // startet trotzdem, nur ohne sie. Wer eine Datei mit dreissig Segmenten
    // schreibt, sieht im Spiel die ersten vierundzwanzig.
    //
    // Wir haben alle dreissig simuliert. Die Vorschau zeigte damit mehr als
    // das Spiel, und zwar ohne Hinweis — genau der Fall, in dem man den
    // Fehler zuletzt beim Editor sucht. (Gemeldet wird er von den
    // Pruefregeln, aber gemeldet heisst nicht gezeigt.)
    constexpr size_t kMaxComponents = 24;
    const size_t componentCount =
        std::min(effect.primitives.size(), kMaxComponents);

    for (size_t i = 0; i < componentCount; ++i) {
        const Primitive& p = effect.primitives[i];
        if (i < enabledMask.size() && !enabledMask[i]) continue;

        // Anzahl. Nicht gesetzt heisst einmal — so verhaelt sich die Engine,
        // wo mSpawnCount mit 1 vorbelegt ist.
        //
        // CFxRange::GetRoundedVal (FxScheduler.h):
        //
        //     if ( mMin == mMax ) return mMin;              // als int: abgeschnitten
        //     return (int)( flrand( mMin, mMax ) + 0.5f );   // gerundet
        //
        // Und KEINE Untergrenze: `count 0 2` ergibt im Spiel in einem Viertel
        // der Faelle gar nichts (flrand < 0.5). Hier stand `if (count < 1)
        // count = 1` — die Vorschau zeigte immer mindestens eines, im Mittel
        // 1.25 statt 1.0.
        // FX_MAX_EFFECT_COMPONENTS begrenzt die Segmente, nicht die Anzahl je
        // Segment — aber eine Datei mit count 100000 wuerde die Vorschau
        // aufhaengen, und das hilft niemandem beim Bearbeiten.
        constexpr int kMaxPerSegment = 4096;
        // Erst als float begrenzen, dann umwandeln: "count 3000000000" passt
        // in keinen int, die Umwandlung war undefiniert und ergab 0 Teilchen.
        const auto toCount = [](float value) {
            if (!(value > 0.0f)) return 0;  // auch NaN
            if (value >= static_cast<float>(kMaxPerSegment)) return kMaxPerSegment;
            return static_cast<int>(value);
        };
        int count = 1;
        if (p.count.set) {
            if (p.count.min == p.count.max || !p.count.ranged) {
                count = toCount(p.count.min);
            } else {
                count = toCount(random.pick(p.count) + 0.5f);
            }
        }

        // Gleichmaessige Verteilung: die Spanne wird in count Schritte geteilt.
        //   factor = |max - min| / count;  delay = t * factor
        // Bemerkenswert: die Engine benutzt dabei NICHT den Minimalwert als
        // Versatz — sie beginnt bei null. Bei delay 200..400 und count 4
        // ergibt das 0, 50, 100, 150, nicht 200, 250, 300, 350.
        const bool even = (p.spawnFlags & kSpawnEvenDistribution) != 0;
        const float span = std::fabs(p.delay.max - p.delay.min);
        const float factor = count > 0 ? span / static_cast<float>(count) : 0.0f;

        for (int t = 0; t < count; ++t) {
            Spawn spawn;
            spawn.primitiveIndex = static_cast<int>(i);
            spawn.timeMs = even ? static_cast<float>(t) * factor
                                : random.pick(p.delay);
            if (spawn.timeMs < 0.0f) spawn.timeMs = 0.0f;
            spawns.push_back(spawn);
        }
    }

    std::stable_sort(spawns.begin(), spawns.end(),
                     [](const Spawn& a, const Spawn& b) {
                         return a.timeMs < b.timeMs;
                     });
    return spawns;
}

camera::Vec3 positionAt(const camera::Vec3& origin, const camera::Vec3& velocity,
                        const camera::Vec3& acceleration, float gravity,
                        float t) {
    // Die Engine hat ZWEI Bewegungsmodelle, und lange stand hier das falsche.
    //
    // 1. Angeheftet (`FX_RELATIVE`, an einem Bolt oder Spieler). CParticle::
    //    Update rechnet dort geschlossen:
    //
    //        realVel = ax*mVel;  realVel[2] += 0.5*mGravity*time;
    //        realVel += time*realAccel;
    //        org += time*realVel;
    //
    //    Ausmultipliziert: org + v*t + 0.5*g*t² + a*t². Die Beschleunigung
    //    wirkt doppelt so stark, wie die Physik verlangt — Raven hat den
    //    Zweifel selbst notiert.
    //
    // 2. In der Welt gespielt (der gewoehnliche Fall, und der, den ein
    //    Editor zeigt). `CFxScheduler::CreateEffect(fx, origin, axis, ...)`
    //    faltet die Schwerkraft in die Beschleunigung:
    //
    //        accel[2] += fx->mGravity.GetVal();
    //
    //    und `CParticle::UpdateOrigin` rechnet dann SCHRITTWEISE:
    //
    //        UpdateVelocity();            // mVel += mAccel * mFloatFrameTime
    //        new_origin = mOrigin1 + mFloatFrameTime * mVel;
    //
    //    Das ergibt im Grenzwert 0.5*a*t², nicht a*t². Nachgerechnet:
    //
    //        Beschleunigung 100, eine Sekunde
    //          bei  30 fps  51.67      bei 125 fps  50.40
    //          bei  60 fps  50.83      Grenzwert    50.00
    //
    // Wir zeigen unbeheftete Effekte, also gilt Modell 2. Fuer die
    // Schwerkraft aendert sich dadurch nichts (beide Modelle geben 0.5*g*t²);
    // eine Datei mit `accel` flog bei uns dagegen doppelt so weit wie im
    // Spiel.
    //
    // Der Grenzwert statt der Schrittrechnung: die Engine haengt damit an der
    // Bildrate — bei 30 fps fliegt dasselbe Teilchen weiter als bei 125. Eine
    // Vorschau, die auf jedem Rechner anders aussieht, waere schlimmer als
    // ein Prozent Abweichung.
    camera::Vec3 total = acceleration;
    total.z += gravity;
    return origin + velocity * t + total * (0.5f * t * t);
}

std::vector<Plane> roomPlanes(float halfWidth, float halfDepth, float height) {
    return roomPlanes(halfWidth, halfDepth, 0.0f, height);
}

std::vector<Plane> roomPlanes(float halfWidth, float halfDepth, float floorZ, float ceilingZ) {
    // Alle Normalen zeigen nach innen — dorthin, wo sich die Partikel
    // aufhalten. Ein Vorzeichen falsch, und sie prallen von aussen ab.
    return {
        {{0.0f, 0.0f, 1.0f}, floorZ},          // Boden
        {{0.0f, 0.0f, -1.0f}, -ceilingZ},      // Decke
        {{1.0f, 0.0f, 0.0f}, -halfWidth},      // links
        {{-1.0f, 0.0f, 0.0f}, -halfWidth},     // rechts
        {{0.0f, 1.0f, 0.0f}, -halfDepth},      // hinten
        {{0.0f, -1.0f, 0.0f}, -halfDepth},     // vorn
    };
}

std::vector<Plane> planesForBox(const std::vector<Plane>& planes, const camera::Vec3& mins,
                                const camera::Vec3& maxs) {
    std::vector<Plane> out = planes;
    for (auto& plane : out) {
        const float closest = (plane.normal.x > 0.0f ? mins.x : maxs.x) * plane.normal.x +
                              (plane.normal.y > 0.0f ? mins.y : maxs.y) * plane.normal.y +
                              (plane.normal.z > 0.0f ? mins.z : maxs.z) * plane.normal.z;
        plane.distance -= closest;
    }
    return out;
}

Hit trace(const camera::Vec3& from, const camera::Vec3& to,
          const std::vector<Plane>& planes) {
    Hit best;
    const camera::Vec3 delta = to - from;

    for (const auto& plane : planes) {
        const float startDistance = camera::dot(plane.normal, from) - plane.distance;
        const float endDistance = camera::dot(plane.normal, to) - plane.distance;

        // Nur von der freien Seite kommend treffen. Wer schon dahinter
        // steckt, soll nicht nach aussen abprallen.
        if (startDistance < 0.0f) continue;
        if (endDistance >= 0.0f) continue;

        const float denominator = startDistance - endDistance;
        if (denominator <= 1e-6f) continue;
        const float fraction = startDistance / denominator;

        // Die fruehste Beruehrung gewinnt. Wer nur die erste gefundene nimmt,
        // bekommt in einer Ecke den falschen Abpraller.
        if (fraction < best.fraction) {
            best.hit = true;
            best.fraction = fraction < 0.0f ? 0.0f : fraction;
            best.normal = plane.normal;
            best.point = from + delta * best.fraction;
        }
    }
    return best;
}

Path buildPath(const camera::Vec3& origin, const camera::Vec3& velocity,
               const camera::Vec3& acceleration, float gravity, float lifeMs,
               const std::vector<Plane>& planes, float elasticity,
               bool stopOnFirst) {
    Path path;
    path.segments.push_back({0.0f, origin, velocity, acceleration, gravity});
    if (planes.empty() || lifeMs <= 0.0f) return path;

    // Feste Schrittweite, nicht die Bildrate: sonst haengt das Ergebnis
    // davon ab, wie schnell der Rechner ist, und die Vorschau sieht auf jedem
    // Rechner anders aus.
    constexpr float kStepMs = 8.0f;
    constexpr int kMaxBounces = 32;

    camera::Vec3 position = origin;
    camera::Vec3 currentVelocity = velocity;
    camera::Vec3 currentAcceleration = acceleration;
    float currentGravity = gravity;
    float segmentStart = 0.0f;

    // Mit einem ganzzahligen Zaehler: "t += 8" kommt als float ab 2^27 ms
    // nicht mehr voran (t + 8 == t), und life 1.4e8 lief endlos.
    for (long long stepIndex = 0;; ++stepIndex) {
        const float t = static_cast<float>(static_cast<double>(stepIndex) * kStepMs);
        if (t >= lifeMs) break;
        // Zur Ruhe gekommen: es bewegt sich nichts mehr, also trifft auch
        // nichts mehr. Vorher lief die Schleife trotzdem ueber das ganze Leben.
        if (path.settledMs > 0.0f) break;
        // Ebenso, wenn sich von vornherein nichts bewegt.
        if (camera::dot(currentVelocity, currentVelocity) == 0.0f &&
            camera::dot(currentAcceleration, currentAcceleration) == 0.0f && currentGravity == 0.0f) {
            break;
        }
        const float step = std::min(kStepMs, lifeMs - t);
        const float from = t - segmentStart;
        const float to = from + step;

        const camera::Vec3 a = positionAt(position, currentVelocity,
                                          currentAcceleration, currentGravity,
                                          from * 0.001f);
        const camera::Vec3 b = positionAt(position, currentVelocity,
                                          currentAcceleration, currentGravity,
                                          to * 0.001f);

        const Hit hit = trace(a, b, planes);
        if (!hit.hit) continue;

        const float hitMs = t + step * hit.fraction;
        path.impactMs.push_back(hitMs);
        path.impactPoint.push_back(hit.point);
        path.impactNormal.push_back(hit.normal);

        if (stopOnFirst) {
            path.killed = true;
            path.killedMs = hitMs;
            return path;
        }

        // Die Geschwindigkeit im Augenblick des Aufpralls. Sie ist nicht die
        // Startgeschwindigkeit — Schwerkraft und Beschleunigung haben bis
        // dahin gewirkt.
        const float seconds = (hitMs - segmentStart) * 0.001f;
        camera::Vec3 impactVelocity = currentVelocity;
        impactVelocity.z += currentGravity * seconds;
        impactVelocity = impactVelocity + currentAcceleration * seconds;

        // Spiegeln:  v' = v - 2*(v·n)*n, dann mit der Elastizitaet daempfen.
        const float dot = camera::dot(impactVelocity, hit.normal);
        impactVelocity = impactVelocity - hit.normal * (2.0f * dot);
        impactVelocity = impactVelocity * elasticity;

        // Zur Ruhe kommen. Genau wie in der Engine: auf einer nach oben
        // zeigenden Flaeche und mit kaum noch Aufwaertsbewegung bleibt es
        // liegen. Ohne das zittert ein Funke ewig auf dem Boden.
        if (hit.normal.z > 0.0f && impactVelocity.z < 4.0f) {
            impactVelocity = {};
            currentAcceleration = {};
            currentGravity = 0.0f;
            // Die Engine loescht hier BEIDE Flags:
            //     mFlags &= ~(FX_APPLY_PHYSICS|FX_IMPACT_RUNS_FX);
            // Ein liegender Funke soll nicht weiter Aufpralleffekte
            // ausloesen. Bei uns hoert die Zerlegung ohnehin auf, weil sich
            // nichts mehr bewegt — aber der letzte Aufprall zaehlt noch, und
            // genau so macht es die Engine auch: die Loeschung steht NACH dem
            // Ausloesen.
            path.settledMs = hitMs;
        }

        // Ein Stueck von der Flaeche wegsetzen, sonst faengt der naechste
        // Schritt schon wieder in ihr an.
        position = hit.point + hit.normal * 0.03f;
        currentVelocity = impactVelocity;
        segmentStart = hitMs;
        path.segments.push_back({hitMs, position, currentVelocity,
                                 currentAcceleration, currentGravity});

        if (static_cast<int>(path.segments.size()) > kMaxBounces) break;
    }
    return path;
}

camera::Vec3 positionOnPath(const Path& path, float msSinceSpawn) {
    if (path.segments.empty()) return {};
    if (path.killed && msSinceSpawn > path.killedMs) msSinceSpawn = path.killedMs;

    // Rueckwaerts suchen: der letzte Abschnitt, der schon begonnen hat.
    size_t index = path.segments.size() - 1;
    while (index > 0 && path.segments[index].startMs > msSinceSpawn) --index;

    const PathSegment& segment = path.segments[index];
    const float seconds = (msSinceSpawn - segment.startMs) * 0.001f;
    return positionAt(segment.origin, segment.velocity, segment.acceleration,
                      segment.gravity, seconds < 0.0f ? 0.0f : seconds);
}

}  // namespace efx::sim
