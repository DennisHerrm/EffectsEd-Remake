// Wiedergabe: Uhr, Ausloesen, Klang, untergeordnete Effekte.
//
// Der Teil, der mit der Zeit zu tun hat. Er beruehrt die Oberflaeche nur
// dort, wo er Zeit von ImGui braucht.
//
// Teil der Klasse App aus app.h — dieselbe Klasse, nach Aufgaben auf
// mehrere Dateien verteilt. app.cpp war mit 3524 Zeilen und einem Dutzend
// Zustaendigkeiten die Stelle, an der ein Leser aufgibt.
#include "app.h"
#include <chrono>
#include <filesystem>
#include "efx/diag.h"
#include "efx/i18n.h"
#include "efx/sim.h"
namespace efx::gui {

using i18n::Str;
using i18n::tr;

void App::pressPlay() {
    // Ein Druck genuegt, auch vom Ende aus.
    //
    // Vorher brauchte es zwei: einen, um zurueckzuspringen, und einen zum
    // Abspielen. Beide Play-Knoepfe — Werkzeugleiste und Zeitleiste — gehen
    // jetzt durch dieselbe Stelle, damit sie sich nicht wieder auseinander
    // entwickeln.
    if (doc().clock.state() == timeline::State::Playing) {
        doc().clock.pause();
        doc().paused = true;
        return;
    }
    const bool atEnd = doc().clock.timeMs() >= doc().clock.durationMs() - 0.5f;
    if (atEnd || !doc().particles.playing()) {
        startPlayback();   // neu ausloesen und von vorn
    } else {
        doc().clock.play();     // fortsetzen, wo es stand
    }
    doc().paused = false;
}

void App::pressStop() {
    // Stop an EINER Stelle.
    //
    // Es gab drei Knoepfe — Menue, Werkzeugleiste, Zeitleiste — und jeder
    // hatte seine eigene Kopie derselben drei Zeilen. Der in der
    // Werkzeugleiste hatte `audio_.stopAll()` nicht: wer damit anhielt, hoerte
    // den Klang zu Ende.
    //
    // Dasselbe ist der Wiedergabe schon einmal passiert, deshalb gibt es
    // pressPlay(). Stop hat die Behandlung nur nie bekommen.
    doc().clock.stop();
    doc().particles.stop();
    audio_.stopAll();
}

void App::togglePause() {
    // Pause haelt die Uhr an, Stop wirft alles weg.
    //
    // Frueher rechnete das hier mit zwei Zeitstempeln herum, weil die Zeit
    // aus der Wanduhr kam und sich nicht anhalten liess. Die eigene Uhr kann
    // das selbst — die beiden Felder sind mit ihr weggefallen.
    doc().clock.togglePause();
    doc().paused = doc().clock.state() == timeline::State::Paused;
}

void App::triggerCameraShakes(float nowMs) {
    // Die fehlende Leitung.
    //
    // `camera::Shake` gab es von Anfang an: es rechnet die Erschuetterung
    // korrekt weiter, deckelt bei MAX_SHAKE_INTENSITY und wird in jedem Bild
    // an die Kamera gegeben. Nur AUSGELOEST hat es nie jemand — deshalb tat
    // ein CameraShake-Segment gar nichts.
    //
    // So fuettert die Engine es (FxScheduler.cpp, case CameraShake):
    //
    //     theFxHelper.CameraShake( origin, fx->mElasticity.GetVal(),
    //                              fx->mRadius.GetVal(), fx->mLife.GetVal() );
    //
    // Also `elasticity` als Staerke, `radius` als Reichweite, `life` als
    // Dauer. Die Abstandsabschwaechung steckt in CG_ExplosionEffects und ist
    // in Shake::trigger nachgebaut.
    if (doc().clock.state() != timeline::State::Playing) return;

    // Der Abstand, in dem die Vorschaukamera steht — dasselbe, was im Spiel
    // der Abstand des Zuschauers zum Effekt waere.
    const float distance = camera_.distance();

    for (auto& item : doc().particles.liveForPlayback()) {
        if (item.type != PrimitiveType::CameraShake) continue;
        if (item.shakeTriggered) continue;
        if (nowMs < item.spawnMs) continue;

        item.shakeTriggered = true;
        const int duration = static_cast<int>(item.deathMs - item.spawnMs);
        if (duration <= 0) continue;

        shake_.trigger(item.shakeIntensity, static_cast<int>(item.shakeRadius),
                       duration, distance);
        diag::info("camera shake: intensity " +
                   std::to_string(static_cast<int>(item.shakeIntensity)) +
                   ", radius " + std::to_string(static_cast<int>(item.shakeRadius)) +
                   ", " + std::to_string(duration) + " ms");
    }
}

void App::playSoundNow(const std::string& name) {
    // Einen einzelnen Klang anhoeren, ohne den Effekt zu starten.
    //
    // Derselbe Weg wie beim Abspielen: suchen, entschluesseln, auf das
    // Geraeteformat bringen. Nicht abgekuerzt — sonst klaenge der Knopf
    // anders als der Effekt, und das waere schlimmer als kein Knopf.
    if (name.empty()) return;
    const sound::Sound* clip = loadSound(name);
    if (!clip || !clip->ok) return;

    constexpr int kDeviceRate = 44100;
    constexpr int kDeviceChannels = 2;
    if (!audio_.open(kDeviceRate, kDeviceChannels)) {
        diag::warn(std::string("audio device: ") + audio_.lastError());
        return;
    }
    const auto ready = sound::convert(clip->samples, clip->sampleRate,
                                      clip->channels, kDeviceRate,
                                      kDeviceChannels);
    if (ready.empty() || !audio_.play(ready)) return;
    diag::info("sound preview: " + name);
}

void App::refreshPreview() {
    // Neu aufbauen und dabei stehen bleiben, wo man war.
    //
    // Der Fehler, der das noetig machte: die Haekchen zum Stummschalten
    // einzelner Segmente wirkten nicht. Die Maske war richtig, sie wurde auch
    // an die Simulation gereicht — sie kam nur nie dort an, weil niemand neu
    // aufbaute.
    //
    // Zwei meiner eigenen Aenderungen zusammen:
    //
    //   - Nach dem Oeffnen steht die Uhr auf null (buildPreviewStopped), also
    //     ist `playing()` falsch, und das Haekchen loeste kein
    //     startPlayback() aus.
    //   - `particles.stop()` wirft die Geometrie nicht mehr weg, also hielt
    //     pressPlay() den Effekt fuer noch laufend und setzte nur fort,
    //     statt neu auszuloesen.
    //
    // Jede fuer sich war richtig. Zusammen ergaben sie ein Haekchen, das
    // nichts tut.
    const float wasAt = doc().clock.timeMs();
    const timeline::State wasState = doc().clock.state();

    startPlayback();

    doc().clock.scrubTo(wasAt);
    if (wasState != timeline::State::Playing) {
        if (wasState == timeline::State::Paused) {
            doc().clock.pause();
        } else {
            doc().clock.stop();
            doc().clock.scrubTo(wasAt);
        }
    }
}

void App::buildPreviewStopped() {
    // Die Vorschau aufbauen, aber NICHT loslaufen lassen.
    //
    // Sonst ist der Bestand nach dem Oeffnen leer: die Zeitleiste zeigt
    // "0 / 1 ms", und das Schieben bewirkt nichts, bis man einmal abgespielt
    // hat. Wer eine Datei oeffnet und erst einmal hin und her fahren will,
    // haelt das Programm zu Recht fuer kaputt.
    //
    // An EINER Stelle, weil es zwei Wege ins Programm gibt — ueber Datei
    // oeffnen und ueber den Doppelklick im Browser. Beim ersten Anlauf hatte
    // ich nur den ersten bedacht; der Anwender kam ueber den zweiten.
    //
    // Klaenge loesen dabei nicht aus: dafuer muesste die Uhr laufen.
    startPlayback();
    doc().clock.stop();
    doc().paused = false;
}

void App::startPlayback() {
    // Bei jedem Start ein neuer Ausgangswert, sonst sieht ein Effekt mit
    // Spannen jedes Mal identisch aus — und gerade das Wuerfeln will man
    // beurteilen.
    //
    // Die Zeit kommt von der Uhr des Systems, nicht von ImGui::GetTime():
    // beim Oeffnen ueber die Kommandozeile (Doppelklick auf eine .efx) laeuft
    // das hier, bevor es einen ImGui-Kontext gibt — und GetTime las dann
    // durch einen Nullzeiger. Das Programm stuerzte bei jedem Doppelklick ab.
    doc().playbackSeed =
        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count() /
                              1000000) | 1u;
    // Fester Ausgangswert fuer den Selbsttest: nur dann sind zwei Laeufe
    // Bild fuer Bild vergleichbar.
    if (fixedSeed_ != 0) doc().playbackSeed = fixedSeed_;
    doc().segmentEnabled.resize(doc().effect.primitives.size(), true);
    doc().particles.play(doc().effect, doc().playbackSeed, doc().segmentEnabled,
                    particles::axisFor(settings_.orientation),
                    [this](const std::string& name) {
                        return loadChildEffect(name);
                    },
                    // Die Waende des Testraums als Kollisionsflaechen.
                    //
                    // Nur wenn ein Raum da ist: bei "Nichts" soll ein Funke
                    // nicht an einer unsichtbaren Ebene abprallen. Bei
                    // "Boden und Himmel" nur der Boden, weil die Waende
                    // ebenfalls fehlen.
                    collisionPlanes(),
                    // `repeatDelay` nur mitspielen, wenn auch wiederholt wird.
                    //
                    // Beim einmaligen Abspielen will man EINEN Durchlauf
                    // beurteilen, nicht fuenf uebereinander. Beim Wiederholen
                    // ist es umgekehrt: dann soll es aussehen wie im Spiel,
                    // und dort legt der Zeitplaner alle `repeatDelay`
                    // Millisekunden nach, ohne den laufenden abzubrechen.
                    //
                    // In der Fassung des alten Editors NICHT: dort laeuft der
                    // Effekt aus und faengt von vorn an, also gibt es auch
                    // keinen Bestand aus der Vergangenheit.
                    doc().clock.endMode() == timeline::EndMode::Repeat &&
                        !settings_.legacyRepeat);
    // Die Bilder des Effekts anfordern, bevor das erste Bild steht. Ohne das
    // sah der erste Durchlauf falsch aus und erst der zweite richtig.
    prefetchTextures(doc().effect);

    // Beim Wiederholen ueber EINE Wiederholung laufen, nicht ueber das ganze
    // Leben.
    //
    // Ein Effekt mit `repeatDelay` endet in der Engine nie: der Zeitplaner
    // legt alle `repeatDelay` Millisekunden nach, ohne den laufenden
    // abzubrechen. Nach genau einer Wiederholung sieht der Bestand deshalb
    // wieder aus wie am Anfang — dank des Vorlaufs, der die Generationen der
    // Vergangenheit mitbringt.
    //
    // Laesst man stattdessen die volle Lebensdauer durchlaufen und dann
    // zuruecksetzen, sieht man am Ende eine ausduennende Wolke und danach
    // einen Sprung. Genau das war gemeint mit "doesn't loop well".
    //
    // Beim einmaligen Abspielen bleibt die volle Dauer: dort will man den
    // ganzen Verlauf sehen und anfahren koennen.
    const bool repeats = doc().clock.endMode() == timeline::EndMode::Repeat;
    const float repeatDelay = static_cast<float>(doc().effect.repeatDelay);
    // Die Dauer kommt aus der Simulation: bei `repeatDelay` ist das eine
    // Wiederholung, sonst das ganze Leben des Effekts.
    if (settings_.legacyRepeat && repeats && repeatDelay >= 1.0f) {
        // Wie der alte Editor: das ganze Leben PLUS die Wiederholpause.
        //
        // Die Pause gehoert dazu — sie ist der sichtbare Unterschied. Ohne
        // sie faengt der Effekt sofort wieder an und man sieht dieselbe
        // Durchgaengigkeit wie in der Engine-Fassung.
        doc().clock.setDuration(doc().particles.durationMs() + repeatDelay);
    } else {
        doc().clock.setDuration(doc().particles.durationMs());
    }
    // Die Geschwindigkeit NICHT ueberschreiben, wenn sie schon gesetzt ist:
    // wer sie in der Zeitleiste einstellt, verliert sie sonst bei jeder
    // Wiederholung, weil startPlayback sie aus der Werkzeugleiste zurueckholt.
    if (settings_.timeScale != doc().clock.speed() && !doc().pendingRestart) {
        doc().clock.setSpeed(settings_.timeScale);
    }
    // Die Wiederholung NICHT aus playback_.mode zurueckholen.
    //
    // Das war die zweite Stelle fuer dieselbe Sache: die Zeitleiste hat ein
    // eigenes Auswahlfeld dafuer, und startPlayback ueberschrieb es bei jedem
    // Start mit dem Wert aus dem Wiedergabe-Dialog — der auf "einmal" steht.
    // Man stellte also Wiederholen ein und es sprang sofort auf Anhalten
    // zurueck.
    //
    // Der Dialog schreibt jetzt in die Uhr, und die Uhr ist die einzige
    // Wahrheit. Umgekehrt schreibt die Zeitleiste in den Dialog, damit beide
    // dasselbe zeigen.
    doc().clock.stop();
    doc().clock.play();
    doc().paused = false;
    // Beim Neustart alles Laufende abbrechen, sonst ueberlagern sich die
    // Klaenge des vorigen Durchlaufs mit denen des neuen.
    audio_.stopAll();
}

void App::triggerSounds(float nowMs) {
    // Klaenge gehoeren zur Wiedergabe. Laeuft sie nicht, wird nichts
    // angestossen.
    //
    // Das fehlte: die Zeile wird bei JEDEM Bild aufgerufen, auch bei
    // angehaltener Uhr. Ein Klang, dessen Zeitpunkt schon erreicht war, der
    // aber noch nicht dran gewesen ist, ging danach trotzdem los — nach dem
    // Stoppen, ohne dass sich etwas bewegte.
    if (doc().clock.state() != timeline::State::Playing) return;

    if (!settings_.playSounds) {
        // EINMAL sagen, dass der Ton abgeschaltet ist.
        //
        // Bisher kehrte diese Zeile stumm zurueck. Wer keinen Ton hoerte,
        // fand im Protokoll dazu gar nichts — nicht einmal die Auskunft, dass
        // gar nicht erst versucht wurde. Fehlende Datei, kaputtes Format,
        // belegtes Geraet und ein Haken im Menue sahen alle gleich aus.
        //
        // Der Haken steht in den Einstellungen und ueberlebt einen Neustart;
        // man kann ihn also vor Wochen ausgeschaltet haben.
        if (!soundsOffReported_) {
            soundsOffReported_ = true;
            diag::info("sounds are switched off (Effects > Play sounds)");
        }
        return;
    }
    soundsOffReported_ = false;
    audio_.update();

    for (const auto& item : doc().particles.live()) {
        if (item.soundPlayed || item.soundName.empty()) continue;
        if (nowMs < item.spawnMs) continue;

        // Merken, BEVOR gespielt wird: schlaegt das Abspielen fehl, soll es
        // nicht in jedem Bild erneut versucht werden.
        item.soundPlayed = true;

        const sound::Sound* clip = loadSound(item.soundName);
        if (!clip) continue;

        // Das Tongeraet meldet Fehler ueber lastError() — und bis eben hat sie
        // niemand gelesen. Wer keinen Ton hoert, konnte an keiner Stelle
        // nachsehen, woran es liegt: fehlende Datei, unlesbares Format,
        // belegtes Geraet oder gar kein Geraet sahen alle gleich aus, naemlich
        // nach Stille.
        //
        // Nur beim ersten Mal je Klang melden. Ein Fehler, der sechzigmal je
        // Sekunde im Protokoll steht, verdeckt alles andere.
        // EIN festes Geraeteformat, alles wird darauf umgerechnet.
        //
        // Vorher wurde das Geraet mit der genauen Rate der Datei geoeffnet.
        // Eine MP3 mit 44100 Hz lief damit, eine WAV mit 11025 Hz nicht:
        // Windows mischt heute mit einer festen Rate, `waveOut` ist nur noch
        // nachgebildet, und ungewoehnliche Raten lehnt es je nach Treiber ab.
        // Bei 44100 faellt das nie auf, weil es die Rate des Geraets ist.
        //
        // Nebenbei behoben: zwei Klaenge mit verschiedenen Raten haben sich
        // vorher gegenseitig abgewuergt, weil der zweite das Geraet des ersten
        // schloss.
        constexpr int kDeviceRate = 44100;
        constexpr int kDeviceChannels = 2;
        if (!audio_.open(kDeviceRate, kDeviceChannels)) {
            diag::warn(std::string("audio device: ") + audio_.lastError() +
                       " (" + item.soundName + ", " +
                       std::to_string(kDeviceRate) + " Hz, " +
                       std::to_string(kDeviceChannels) + " ch)");
            continue;
        }
        const auto ready = sound::convert(clip->samples, clip->sampleRate,
                                          clip->channels, kDeviceRate,
                                          kDeviceChannels);
        if (ready.empty()) {
            diag::warn("audio conversion produced nothing: " + item.soundName);
            continue;
        }
        if (!audio_.play(ready)) {
            diag::warn(std::string("audio playback: ") + audio_.lastError() +
                       " (" + item.soundName + ")");
            continue;
        }
        diag::info("sound playing: " + item.soundName + " (" +
                   std::to_string(clip->sampleRate) + " Hz, " +
                   std::to_string(clip->channels) + " ch -> Geraet)");
    }
}

const sound::Sound* App::loadSound(const std::string& name) {
    const auto found = soundCache_.find(name);
    if (found != soundCache_.end()) {
        return found->second.ok ? &found->second : nullptr;
    }

    sound::Sound loaded;
    // Klaenge stehen in der .efx-Datei mit Endung und Pfad, etwa
    // "sound/weapons/blaster/fire.wav" — anders als Shader und Effekte.
    //
    // Das Suchen macht assets::findSound. Hier stand vorher eine eigene,
    // kuerzere Fassung, und die hatte zwei Loecher:
    //
    //   - nur `settings_.gamePath`, nicht alle Spielpfade
    //   - keine Ersatzendung
    //
    // Das zweite ist das entscheidende: die Engine liest erst die `.wav` und
    // versucht bei Misserfolg dieselbe Datei als `.mp3` (S_LoadSound_Actual
    // in snd_mem.cpp). In fast jeder Raven-.efx steht `.wav`, im `.pk3` liegt
    // aber die `.mp3`. So kam die Meldung zustande:
    //
    //     WARNING sound not found: sound/weapons/rocket/hit_wall.wav
    //
    // bei 29839 gefundenen Klaengen im Bestand.
    const assets::ResolvedTexture where =
        assets::findSound(assets_, settings_.gamePath, name);

    if (where.found) {
        std::string error;
        const auto bytes = assets::readFile(settings_.gamePath, where, &error);
        if (!bytes.empty()) {
            loaded = sound::decode(bytes.data(), bytes.size());
            if (loaded.ok) {
                // Auch WOHER — bei einer Ersatzendung ist das die halbe Miete.
                diag::info("sound " + name + " <- " + where.path +
                           (where.archive.empty() ? "" : " (" + where.archive + ")") +
                           " (" +
                           std::to_string(loaded.sampleRate) + " Hz, " +
                           std::to_string(loaded.channels) + " ch, " +
                           std::to_string(static_cast<int>(loaded.durationMs())) +
                           " ms)");
            } else {
                diag::warn("sound " + name + ": " + loaded.error);
            }
        }
    } else {
        diag::warn("sound not found: " + name);
    }

    soundCache_[name] = std::move(loaded);
    const sound::Sound& stored = soundCache_[name];
    return stored.ok ? &stored : nullptr;
}

const Effect* App::loadChildEffect(const std::string& name) {
    const auto found = childEffects_.find(name);
    if (found != childEffects_.end()) return found->second.get();

    // Auch das Scheitern merken (als leerer Zeiger), sonst wird bei jedem
    // Start wieder vergeblich gesucht.
    std::unique_ptr<Effect> loaded;

    const auto where = assets::findEffect(assets_, settings_.gamePath, name);
    if (where.found) {
        std::string error;
        const auto bytes = assets::readFile(settings_.gamePath, where, &error);
        if (!bytes.empty()) {
            const std::string text(bytes.begin(), bytes.end());
            auto result = read(text);
            if (!result.effect.primitives.empty()) {
                loaded = std::make_unique<Effect>(std::move(result.effect));
                diag::info("child effect " + name + " <- " + where.path);
            }
        }
    }
    if (!loaded) diag::warn("child effect not found: " + name);

    const Effect* pointer = loaded.get();
    childEffects_[name] = std::move(loaded);
    return pointer;
}

std::vector<sim::Plane> App::collisionPlanes() const {
    const auto style = static_cast<scene::RoomStyle>(settings_.roomStyle);
    if (style == scene::RoomStyle::None) return {};

    const float scale = settings_.worldScale > 0.0f ? settings_.worldScale : 16.0f;
    const float halfWidth = roomSize_.widthFeet * scale * 0.5f;
    const float halfDepth = roomSize_.depthFeet * scale * 0.5f;
    const float height = roomSize_.heightFeet * scale;

    auto planes = sim::roomPlanes(halfWidth, halfDepth, height);
    if (style == scene::RoomStyle::OpenSky) {
        // Draussen gibt es nur den Boden. Alles andere waere eine Wand, die
        // man nicht sieht — und nichts ist verwirrender als ein Funke, der
        // im Nichts abprallt.
        planes.resize(1);
    }
    return planes;
}


}  // namespace efx::gui
