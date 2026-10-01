#include "efx/playback.h"

#include <algorithm>
#include <cmath>

namespace efx::playback {

float Settings::frequency() const {
    const float rate = std::clamp(repeatRateSeconds, kMinRate, kMaxRate);
    return 1.0f / rate;
}

void Settings::setRate(float secondsPerSpawn) {
    repeatRateSeconds = std::clamp(secondsPerSpawn, kMinRate, kMaxRate);
}

void Settings::setFrequency(float spawnsPerSecond) {
    // Ueber den Kehrwert, nicht ueber ein zweites gespeichertes Feld. Zwei
    // Felder, die dasselbe meinen, laufen frueher oder spaeter auseinander —
    // und dann zeigt der Dialog etwas anderes an, als die Vorschau tut.
    if (spawnsPerSecond <= 0.0f) {
        repeatRateSeconds = kMaxRate;
        return;
    }
    setRate(1.0f / spawnsPerSecond);
}

int Settings::totalRepetitions() const {
    // "n/a" im Original: bei einmaligem Abspielen und bei "bis zum Anhalten"
    // gibt es keine Zahl.
    if (mode == RepeatMode::Once) return 1;
    if (mode == RepeatMode::UntilStopped) return -1;
    if (respawnEveryFrame) return -1;  // haengt an der Bildrate

    const float rate = std::clamp(repeatRateSeconds, kMinRate, kMaxRate);
    if (repeatForSeconds <= 0.0f) return 0;

    // Die erste Ausloesung faellt auf den Zeitpunkt null, deshalb einer mehr.
    return static_cast<int>(std::floor(repeatForSeconds / rate)) + 1;
}

camera::Vec3 Settings::originAt(float seconds, const camera::Vec3& base) const {
    if (!animateSpawnLocation) return base;
    if (seconds < 0.0f) seconds = 0.0f;

    // Nach der eingestellten Zeit springt der Ursprung zurueck. Ohne das
    // wandert er bei laengerer Wiedergabe aus dem Raum heraus, und man sucht
    // den Effekt.
    float elapsed = seconds;
    if (resetLocationAfter > 0.0f) {
        elapsed = std::fmod(seconds, resetLocationAfter);
    }
    return camera::Vec3{base.x + spawnVelocity.x * elapsed,
                        base.y + spawnVelocity.y * elapsed,
                        base.z + spawnVelocity.z * elapsed};
}

camera::Vec3 Origin::resolve(const scene::RoomSize& room, float worldScale) const {
    (void)worldScale;  // der Raum haengt nicht vom Massstab ab
    // Gemessen am Original bei 10 Einheiten je Fuss (Custom Fx Spawn Origin):
    // Raummitte 0/0/20, Boden 0/0/-19, Decke 0/0/59, Wand -99/0/20 — jeweils
    // eine Einheit vor der Flaeche, damit nichts in ihr steckt.
    switch (mode) {
        case OriginMode::RoomCentre:
            return {0.0f, 0.0f, room.centreZ()};
        case OriginMode::OnFloor:
            return {0.0f, 0.0f, room.floorZ + 1.0f};
        case OriginMode::OnCeiling:
            return {0.0f, 0.0f, room.ceilingZ - 1.0f};
        case OriginMode::OnWall:
            return {-room.halfX + 1.0f, 0.0f, room.centreZ()};
        case OriginMode::Custom:
            return custom;
        default:
            return {0.0f, 0.0f, 0.0f};
    }
}

}  // namespace efx::playback
