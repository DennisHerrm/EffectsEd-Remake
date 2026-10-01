// Die Uhr der Vorschau.
//
// Bisher kam die Zeit direkt von `ImGui::GetTime()` — verstrichene Wanduhrzeit
// seit dem Start. Das reicht zum Abspielen und für sonst nichts: man kann
// nicht springen, nicht zurückspulen und nicht Bild für Bild gehen, weil die
// Wanduhr sich nicht anhalten oder verschieben lässt.
//
// Diese Uhr gehört dem Programm. Sie wird um einen Zeitschritt weitergestellt,
// wenn abgespielt wird, und lässt sich sonst frei setzen. Damit wird die
// Wiedergabe außerdem **vorhersagbar**: dieselbe Datei bei derselben Zeit
// ergibt dasselbe Bild, unabhängig davon, wie schnell der Rechner ist.
#pragma once

namespace efx::timeline {

enum class State { Stopped, Playing, Paused };

// Was am Ende geschieht.
enum class EndMode {
    Stop,    // stehen bleiben
    Repeat,  // von vorn
    Hold,    // auf dem letzten Bild stehen bleiben, aber „läuft" bleiben
};

class Clock {
public:
    // Die Gesamtdauer des Effekts. Kürzer als ein Bild ergibt keinen Sinn,
    // deshalb eine Untergrenze.
    void setDuration(float ms);
    float durationMs() const { return durationMs_; }

    void play();
    void pause();
    void togglePause();
    void stop();

    State state() const { return state_; }
    bool playing() const { return state_ == State::Playing; }

    float timeMs() const { return timeMs_; }

    // Anteil 0..1 für den Schieberegler.
    float progress() const;
    void setProgress(float value);

    // Beim Spulen: die Zeit direkt setzen. Hält die Wiedergabe an, weil man
    // sonst sofort weiterläuft und die Stelle nicht ansehen kann.
    void scrubTo(float ms);

    // Ein Zeitschritt weiter. `deltaMs` ist die vergangene Wanduhrzeit; sie
    // wird mit der Geschwindigkeit multipliziert.
    //
    // Ein Deckel begrenzt große Sprünge: bleibt das Fenster eine Sekunde
    // hängen, soll der Effekt nicht eine Sekunde weiterspringen, sondern
    // dort fortsetzen, wo er war.
    void advance(float deltaMs);

    // Bild für Bild. Vorwärts oder rückwärts, mit der eingestellten
    // Bildrate. Hält die Wiedergabe an — Bild für Bild und Abspielen
    // zugleich ergäbe keinen Sinn.
    void stepFrames(int frames);

    void setSpeed(float value);
    float speed() const { return speed_; }

    void setFrameRate(float fps);
    float frameRate() const { return frameRate_; }

    // Bei welchem Bild wir stehen — für die Anzeige.
    int currentFrame() const;
    int totalFrames() const;

    void setEndMode(EndMode mode) { endMode_ = mode; }
    EndMode endMode() const { return endMode_; }

    // Ist der Effekt gerade durchgelaufen? Wird beim Abfragen zurückgesetzt,
    // damit die Oberfläche einen Neustart genau einmal auslöst.
    bool consumeWrapped();

private:
    float durationMs_ = 1000.0f;
    float timeMs_ = 0.0f;
    float speed_ = 1.0f;
    float frameRate_ = 30.0f;
    State state_ = State::Stopped;
    EndMode endMode_ = EndMode::Repeat;
    bool wrapped_ = false;
};

}  // namespace efx::timeline
