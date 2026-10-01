// Zeitgeber und Bewegung: wann erscheint was, und wohin fliegt es.
//
// Zwei Teile, beide ohne Grafikschnittstelle und damit prüfbar:
//
//   Zeitplanung   welches Segment löst wann aus, und wie oft
//   Bahn          wo ist eine Primitive zum Zeitpunkt t
//
// Beides ist aus `FxScheduler.cpp` und `FxPrimitives.cpp` hergeleitet. An zwei
// Stellen weicht die Engine von dem ab, was man erwarten würde — siehe
// `positionAt`.
#pragma once

#include <cstdint>
#include <vector>

#include "efx/camera.h"
#include "efx/effect.h"

namespace efx::sim {

// Ein Zufallsgeber, der hereingereicht wird statt intern zu ziehen. Sonst
// lässt sich nichts davon prüfen: dieselbe Datei müsste zweimal dasselbe
// ergeben, und das tut sie nur mit bekanntem Ausgangswert.
class Random {
public:
    // Der Ausgangswert wird erst verruehrt (splitmix32). Ohne das ist der
    // erste Wert eines LCG fast linear im Ausgangswert: die Ausgangswerte 1
    // bis 400 gaben alle einen ersten Wert zwischen 0.24 und 0.39 — und die
    // Vorschau bekommt ihre Ausgangswerte aus der Uhr, also aufeinander
    // folgende. Ein Effekt mit `count 0 2` hatte damit nie null Teilchen.
    explicit Random(unsigned seed = 1u) : state_(scramble(seed)) {}
    float next();                       // 0 bis 1
    float range(float low, float high);
    float pick(const Range& r);         // würfelt zwischen min und max, wenn ranged

private:
    static unsigned scramble(unsigned seed) {
        unsigned z = seed + 0x9E3779B9u;
        z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
        z = (z ^ (z >> 13)) * 0xC2B2AE35u;
        z ^= z >> 16;
        return z ? z : 1u;
    }
    unsigned state_;
};

// Eine geplante Auslösung.
struct Spawn {
    int primitiveIndex = 0;
    float timeMs = 0.0f;  // relativ zum Auslösen des Effekts
};

// Plant alle Auslösungen eines Effekts.
//
// Je Primitive: `count`-mal, jedes mit eigener `delay`. Ist
// `evenDistribution` gesetzt, werden die Verzögerungen gleichmäßig über die
// Spanne verteilt statt gewürfelt — genau das ist der Sinn des Flags.
// enabledMask: welche Segmente mitspielen. Leer heisst alle.
//
// Das Häkchen in der Segmentliste ist eine reine Vorschau-Einstellung des
// Editors — es steht nicht in der Datei und gehört deshalb nicht in die
// Primitive, sondern hierher als Parameter.
std::vector<Spawn> schedule(const Effect& effect, Random& random,
                            const std::vector<bool>& enabledMask = {});

// Die Bahn einer Primitive.
//
// **Achtung, hier weicht die Engine von der Physik ab.** Sie rechnet die Lage
// geschlossen statt schrittweise:
//
//     realVel[2] += 0.5 * gravity * t
//     realVel    += t * accel
//     org         = start + t * realVel
//
// Ausmultipliziert wirkt die Schwerkraft mit `0.5·g·t²` — richtig — die
// Beschleunigung aber mit `1.0·a·t²`, also **doppelt so weit**, wie eine
// saubere Integration ergäbe.
//
// Raven hat das selbst bemerkt; im Quelltext steht daneben:
// *„NOTE: not sure if this is even 100% correct math-wise"*.
//
// Wir bauen es genau so nach. Das Ziel ist nicht richtige Physik, sondern
// dieselbe Bahn wie im Spiel — eine Vorschau, die anders fliegt als das Spiel,
// ist schlimmer als keine.
// Eine Ebene, an der abgeprallt wird. `normal` zeigt in den freien Raum.
struct Plane {
    camera::Vec3 normal{0.0f, 0.0f, 1.0f};
    float distance = 0.0f;  // normal·x = distance liegt auf der Ebene
};

// Was ein Wegstück getroffen hat.
struct Hit {
    bool hit = false;
    float fraction = 1.0f;   // 0..1 entlang des Wegstücks
    camera::Vec3 point;
    camera::Vec3 normal;
};

// Trifft das Wegstück von `from` nach `to` eine der Ebenen?
//
// Die früheste Berührung gewinnt. Wer nur die erste gefundene nimmt, bekommt
// in einer Ecke den falschen Abpraller.
Hit trace(const camera::Vec3& from, const camera::Vec3& to,
          const std::vector<Plane>& planes);

// Der Testraum als sechs Ebenen, alle nach innen zeigend.
std::vector<Plane> roomPlanes(float halfWidth, float halfDepth, float height);
// Dasselbe mit Boden und Decke an beliebiger Hoehe (der Raum des Originals
// hat den Boden bei -20).
std::vector<Plane> roomPlanes(float halfWidth, float halfDepth, float floorZ, float ceilingZ);

// Ein Abschnitt der Bahn: ab `startMs` gilt diese Geschwindigkeit ab diesem
// Punkt.
//
// Mit Abprallern lässt sich die Bahn nicht mehr in einer Formel ausdrücken —
// nach jedem Aufprall gilt eine neue Geschwindigkeit, und **wann** er kommt,
// hängt vom Weg ab. Also wird sie beim Auslösen einmal in Abschnitte zerlegt;
// jeder einzelne ist wieder geschlossen ausrechenbar.
//
// Das hält die Vorschau bildratenunabhängig: sie läuft nicht Schritt für
// Schritt mit, sondern schlägt nach.
struct PathSegment {
    float startMs = 0.0f;
    camera::Vec3 origin;
    camera::Vec3 velocity;
    camera::Vec3 acceleration;
    float gravity = 0.0f;
};

// Was beim Abprallen herauskommt.
struct Path {
    std::vector<PathSegment> segments;
    // Zeitpunkte und Orte der Aufpralle — für impactfx und killOnImpact.
    std::vector<float> impactMs;
    std::vector<camera::Vec3> impactPoint;
    std::vector<camera::Vec3> impactNormal;
    bool killed = false;      // killOnImpact hat zugeschlagen
    float killedMs = 0.0f;

    // Ab wann es liegen bleibt. Danach kommt nichts mehr — die Engine
    // löscht an dieser Stelle `FX_APPLY_PHYSICS` und `FX_IMPACT_RUNS_FX`.
    // 0 heißt: kam nie zur Ruhe.
    float settledMs = 0.0f;
};

// Zerlegt die Bahn in Abschnitte und sammelt die Aufpralle.
//
// elasticity ist der Anteil der Geschwindigkeit, der einen Aufprall übersteht.
// stopOnFirst entspricht `killOnImpact`.
Path buildPath(const camera::Vec3& origin, const camera::Vec3& velocity,
               const camera::Vec3& acceleration, float gravity, float lifeMs,
               const std::vector<Plane>& planes, float elasticity,
               bool stopOnFirst);

// Die Lage zu einem Zeitpunkt, über alle Abschnitte hinweg.
camera::Vec3 positionOnPath(const Path& path, float msSinceSpawn);

camera::Vec3 positionAt(const camera::Vec3& origin, const camera::Vec3& velocity,
                        const camera::Vec3& acceleration, float gravity,
                        float secondsSinceSpawn);

}  // namespace efx::sim
