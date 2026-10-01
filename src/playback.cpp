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
    if (worldScale <= 0.0f) worldScale = 16.0f;

    const float halfX = room.widthFeet * worldScale * 0.5f;
    const float height = room.heightFeet * worldScale;

    switch (mode) {
        case OriginMode::RoomCentre:
            return {0.0f, 0.0f, height * 0.5f};
        case OriginMode::OnFloor:
            return {0.0f, 0.0f, 0.0f};
        case OriginMode::OnCeiling:
            return {0.0f, 0.0f, height};
        case OriginMode::OnWall:
            // An der -X-Wand, auf halber Hoehe. Das Original nimmt eine feste
            // Wand; welche, ist gleichgueltig, solange sie im Bild liegt.
            return {-halfX, 0.0f, height * 0.5f};
        case OriginMode::Custom:
            return custom;
        default:
            return {0.0f, 0.0f, 0.0f};
    }
}

}  // namespace efx::playback
