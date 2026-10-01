// Wiedergabe-Einstellungen und Ursprung des Effekts.
//
// Nachbau der Dialoge 188 („Playback Settings") und 192 („Custom Fx Spawn
// Origin") aus EffectsEd.exe. Die Rechnungen stehen hier und nicht in der
// Oberfläche, damit sie ohne Fenster geprüft werden können — vor allem die
// Kopplung von Rate und Frequenz, die im Original zwei Felder mit zwei
// Schiebereglern sind, die sich gegenseitig nachführen.
#pragma once

#include "efx/camera.h"
#include "efx/scene.h"

namespace efx::playback {

enum class RepeatMode {
    Once,           // einmal abspielen
    UntilStopped,   // bis zum Anhalten
    ForSeconds,     // eine bestimmte Zeit lang
};

struct Settings {
    RepeatMode mode = RepeatMode::Once;
    float repeatForSeconds = 2.0f;

    // Jedes Bild neu auslösen. Übersteuert die Rate — so steht es im
    // Original, wo die Felder darunter dann ausgegraut sind.
    bool respawnEveryFrame = false;

    // Sekunden je Auslösung. Frequenz ist der Kehrwert und wird nicht
    // gespeichert, sondern gerechnet: zwei Felder, die dasselbe meinen,
    // laufen sonst auseinander.
    float repeatRateSeconds = 0.300f;

    // Bewegung des Ursprungs während der Wiedergabe.
    bool animateSpawnLocation = false;
    camera::Vec3 spawnVelocity;      // Einheiten je Sekunde
    float resetLocationAfter = 1.0f; // Sekunden

    // Grenzen. Unter einer Millisekunde je Auslösung wird aus der Vorschau
    // eine Diaschau, und über einer Minute wartet man vergeblich.
    static constexpr float kMinRate = 0.001f;
    static constexpr float kMaxRate = 60.0f;

    // Auslösungen je Sekunde. Kehrwert der Rate.
    float frequency() const;
    void setFrequency(float spawnsPerSecond);
    void setRate(float secondsPerSpawn);

    // Wie oft wird insgesamt ausgelöst? Negativ heißt „unbestimmt" — das ist
    // das „n/a" im Original.
    int totalRepetitions() const;

    // Wo steht der Ursprung zum Zeitpunkt t, wenn er sich bewegt?
    camera::Vec3 originAt(float seconds, const camera::Vec3& base) const;
};

// --- Ursprung ------------------------------------------------------------

enum class OriginMode {
    Default,     // Weltursprung, wie ohne Dialog
    RoomCentre,
    OnFloor,
    OnCeiling,
    OnWall,
    Custom,
};

struct Origin {
    OriginMode mode = OriginMode::Default;
    camera::Vec3 custom;

    // Rechnet die Lage aus. Braucht die Raumgröße, weil „an der Decke" ohne
    // Raum nichts bedeutet.
    camera::Vec3 resolve(const scene::RoomSize& room, float worldScale) const;
};

}  // namespace efx::playback
