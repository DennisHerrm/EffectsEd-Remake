#include "app.h"
#include "testmarke.h"
#include "app_shared.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "efx/diag.h"
#include "efx/paths.h"
#include "efx/i18n.h"
#include "efx/jobs.h"
#include "imgui_internal.h"
namespace efx::gui {
namespace {

using i18n::Str;
using i18n::tr;

std::string readWholeFile(const std::string& path, bool& ok) {
    std::ifstream in(paths::fromUtf8(path), std::ios::binary);
    ok = in.good();
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}





}  // namespace

App::App() {
    // Immer mindestens ein Dokument. Ein Fenster ohne Dokument gaebe es sonst
    // beim Schliessen des letzten Reiters, und fuer diesen einen Fall ueberall
    // auf Leerheit zu pruefen waere der schlechtere Tausch.
    documents_.emplace_back();
    documents_.back().undo.reset(documents_.back().effect);
}

Document& App::doc() {
    if (documents_.empty()) documents_.emplace_back();
    if (activeDocument_ < 0 ||
        activeDocument_ >= static_cast<int>(documents_.size())) {
        activeDocument_ = 0;
    }
    return documents_[static_cast<size_t>(activeDocument_)];
}

const Document& App::doc() const {
    // Dieselbe Rechnung wie oben, nur ohne die Liste zu veraendern.
    static const Document empty;
    if (documents_.empty()) return empty;
    const int at = (activeDocument_ >= 0 &&
                    activeDocument_ < static_cast<int>(documents_.size()))
                       ? activeDocument_ : 0;
    return documents_[static_cast<size_t>(at)];
}

void App::clampWindFlagToRoom() {
    // Eine Fahne hinter der Wand ist unsichtbar, und man findet sie nicht
    // wieder. 0.45 statt 0.5: ein Stueck Abstand zur Wand, sonst steckt sie
    // halb darin.
    const float halfWidth = roomSize_.halfX * 0.9f;
    const float halfDepth = roomSize_.halfY * 0.9f;
    settings_.windFlagPos[0] =
        std::clamp(settings_.windFlagPos[0], -halfWidth, halfWidth);
    settings_.windFlagPos[1] =
        std::clamp(settings_.windFlagPos[1], -halfDepth, halfDepth);
}

bool App::playing() const {
    return doc().clock.state() != timeline::State::Stopped;
}

bool App::needsContinuousFrames() const {
    if (doc().clock.state() == timeline::State::Playing) return true;
    if (startTabActive_) return true;               // die Kacheln laufen
    if (settings_.drawWindVector) return true;      // die Fahne weht
    if (shake_.active()) return true;
    if (!texturesInFlight_.empty() || !readyTextures_.empty()) return true;
    if (jobs::pool().pendingMainTasks() > 0) return true;
    if (pendingScreenshot_ != 0 || doc().pendingRestart || previewDirty_) return true;
    return false;
}

std::string Document::title() const {
    // Uebersetzt, nicht fest verdrahtet — der Reiter ist Oberflaeche wie
    // jede andere.
    if (filePath.empty()) return i18n::tr(i18n::Str::TabUnnamed);
    const size_t cut = filePath.find_last_of("/\\");
    return cut == std::string::npos ? filePath : filePath.substr(cut + 1);
}

timeline::EndMode App::playbackEndMode() const {
    return playback_.mode == playback::RepeatMode::UntilStopped ? timeline::EndMode::Repeat
                                                                : timeline::EndMode::Stop;
}

void App::flushPendingFieldEdit() {
    // Eine angefangene Feldeingabe wird erst aufgezeichnet, wenn kein Element
    // mehr aktiv ist — beim Klick auf einen Reiter oder "+" ist das erst der
    // Fall, wenn der Reiter schon gewechselt hat. Dann landete die Aenderung
    // im Rueckgaengig-Stapel des FALSCHEN Dokuments. Deshalb vor jedem
    // Wechsel hier abschliessen, solange doc() noch das richtige ist.
    if (!fieldEditOpen_) return;
    fieldEditOpen_ = false;
    recordChange(tr(Str::UndoFieldChange));
}

void App::newDocument() {
    flushPendingFieldEdit();
    // Wie beim Reiterwechsel: die Wiedergabe des bisherigen anhalten.
    pressStop();
    documents_.emplace_back();
    activeDocument_ = static_cast<int>(documents_.size()) - 1;
    doc().undo.reset(doc().effect);
    doc().savedUndoId = doc().undo.currentId();
    // Die Wiederholart gilt fuer alle Dokumente, wie im Original (dort gibt
    // es nur die eine Einstellung).
    doc().clock.setEndMode(playbackEndMode());
}

void App::activateDocument(int index) {
    if (index < 0 || index >= static_cast<int>(documents_.size())) return;
    if (index == activeDocument_) return;
    flushPendingFieldEdit();
    // Die Wiedergabe des bisherigen Reiters anhalten. Sonst laufen Klaenge
    // eines Effekts weiter, den man gar nicht mehr sieht.
    // pressStop und nicht drei Einzelzeilen: sonst blieb ein laufendes
    // Auslaufen (Play waehrend der Wiederholung) fuer den naechsten Reiter
    // haengen. Die Geometrie bleibt, der Reiter kommt wieder.
    pressStop();
    activeDocument_ = index;
    // Auch die Reiterleiste umschalten (Strg+Tab, Speichern-Frage).
    wantDocumentTab_ = true;
}

void App::closeDocument(int index) {
    if (index < 0 || index >= static_cast<int>(documents_.size())) return;
    flushPendingFieldEdit();
    // Der aktive Reiter wird geschlossen: seine Wiedergabe gehoert zu ihm.
    if (index == activeDocument_) pressStop();
    documents_.erase(documents_.begin() + index);
    // Lag der geschlossene Reiter LINKS vom aktiven, rueckt der aktive eins
    // nach vorn. Vorher blieb die Nummer stehen, und man arbeitete ploetzlich
    // im Reiter rechts daneben weiter.
    if (index < activeDocument_) --activeDocument_;
    if (documents_.empty()) {
        documents_.emplace_back();
        documents_.back().clock.setEndMode(playbackEndMode());
        documents_.back().undo.reset(documents_.back().effect);
        documents_.back().savedUndoId = documents_.back().undo.currentId();
    }
    if (activeDocument_ >= static_cast<int>(documents_.size())) {
        activeDocument_ = static_cast<int>(documents_.size()) - 1;
    }
    // Die Reiter heissen "###doc<Nummer>": nach dem Entfernen traegt der
    // rechte Nachbar die alte Nummer, und ImGui hielte IHN fuer ausgewaehlt.
    // Also die Auswahl ausdruecklich setzen — ausser die Bibliothek ist vorn.
    if (!startTabActive_) wantDocumentTab_ = true;
    audio_.stopAll();
}
App::~App() = default;

namespace {

uint32_t packColour(const theme::Color& c, float scale = 1.0f) {
    auto to255 = [](float v) {
        const int i = static_cast<int>(v * 255.0f + 0.5f);
        return i < 0 ? 0 : (i > 255 ? 255 : i);
    };
    return scene::rgba(to255(c.r * scale), to255(c.g * scale), to255(c.b * scale),
                       to255(c.a));
}

}  // namespace

void App::rebuildGeometry() {
    const theme::Palette& p = activeTheme(settings_).palette;
    const float worldScale = settings_.worldScale;

    // Liegt eine Wandtextur an, traegt SIE die Farbe.
    //
    // Vorher wurde das graue Muster mit der Wandfarbe des Themas
    // multipliziert — bei einem dunklen Thema ergab das einen gruenlichen
    // Raum. Ravens Texturen sind selbst gefaerbt (Putz R144 G140 B115), also
    // muessen die Eckpunkte weiss bleiben; die Sonne dunkelt sie ohnehin noch
    // ab.
    //
    // Eine von Hand gesetzte Wandfarbe gewinnt weiterhin: wer sie ausdruecklich
    // waehlt, will sie auch sehen.
    const bool textured = settings_.roomTexture != 0;
    const theme::Color wall =
        wallColourOverridden_
            ? theme::Color{wallColour_[0], wallColour_[1], wallColour_[2], 1.0f}
            : (textured ? theme::Color{1.0f, 1.0f, 1.0f, 1.0f} : p.roomWall);
    const camera::Vec3 sun{settings_.sunDirection[0], settings_.sunDirection[1],
                          settings_.sunDirection[2]};
    const auto style = static_cast<scene::RoomStyle>(settings_.roomStyle);

    roomMesh_ = scene::buildRoomLit(roomSize_, worldScale, packColour(wall), style,
                                    sun, settings_.sunAmbient,
                                    settings_.sunEnabled, settings_.drawRoom);

    // Himmel und Sonne nur draussen. Der Radius liegt weit hinter dem Raum,
    // aber deutlich vor der fernen Ebene — sonst wird die Kuppel weggeklippt.
    if (style == scene::RoomStyle::OpenSky) {
        const float radius = 250.0f * worldScale;
        skyMesh_ = scene::buildSky(radius, sun, packColour(p.skyHorizon),
                                   packColour(p.skyZenith), packColour(p.sunGlow));
        sunMesh_ = settings_.drawSunDisc && settings_.sunEnabled
                       ? scene::buildSunDisc(sun, radius, radius * 0.03f,
                                             packColour(p.sunGlow))
                       : scene::Mesh{};
    } else {
        skyMesh_ = scene::Mesh{};
        sunMesh_ = scene::Mesh{};
    }
    // Im Drahtgittermodus ersetzt das Gitter die Flaechen, in der Wandfarbe
    // (Original: "In wireframe mode, the grid lines will be drawn using this
    // color"). Mit Raum auf allen sechs Flaechen, ohne Raum auf der Ebene.
    {
        const theme::Color gridColour =
            wallColourOverridden_ ? theme::Color{wallColour_[0], wallColour_[1], wallColour_[2], 1.0f}
                                  : p.roomWall;
        gridLines_ = scene::buildGrid(roomSize_, worldScale, packColour(gridColour),
                                      packColour(gridColour, 1.4f),
                                      settings_.drawRoom &&
                                          settings_.roomStyle == static_cast<int>(scene::RoomStyle::Enclosed));
    }
    // Sechzehn Einheiten, wie im Original.
    axisLines_ = scene::buildAxes(16.0f, packColour(p.axisX), packColour(p.axisY),
                                  packColour(p.axisZ));
    // Die Fahne wird jedes Bild neu gebaut, weil sie weht — deshalb steht sie
    // nicht hier, sondern in drawViewport.
    windLines_ = scene::LineSet{};

    // Die Fahne in den Raum zurueckholen.
    //
    // Der Anschlag griff bisher nur beim ZIEHEN. Wechselt man den Masstab von
    // 32 auf 10 Einheiten je Fuss, schrumpft der Raum auf ein Drittel — die
    // Fahne blieb, wo sie war, und stand danach hinter der Wand. Dort ist sie
    // unsichtbar und nicht mehr zu greifen.
    clampWindFlagToRoom();

    lastWorldScale_ = worldScale;
    geometryDirty_ = false;
}

void App::handleViewportInput(float width, float height) {
    ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsItemHovered() && !ImGui::IsItemActive()) return;
    (void)width;
    (void)height;

    const ImVec2 drag = io.MouseDelta;

    // Wird gerade die Fahne verschoben, bleibt die Kamera stehen. Sonst
    // dreht sich das Bild, waehrend man zieht, und die Fahne rutscht einem
    // unter der Maus weg.
    if (draggingFlag_) return;

    // Belegung aus Ravens Anleitung. Die Reihenfolge der Abfragen ist wichtig:
    // Alt und Z aendern die Bedeutung der linken Taste, muessen also zuerst
    // geprueft werden.
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (io.KeyAlt) {
            camera_.pan(drag.x, drag.y);
        } else if (ImGui::IsKeyDown(ImGuiKey_Z)) {
            camera_.roll(drag.x);
        } else {
            camera_.orbit(drag.x, drag.y);
        }
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        camera_.dolly(drag.y);
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        camera_.pan(drag.x, drag.y);
    }
    if (io.MouseWheel != 0.0f) {
        camera_.zoomWheel(io.MouseWheel);
    }
}

void App::startup() {
    diag::Step step("Read settings");

    bool ok = false;
    const std::string path = paths::settingsPath();
    const std::string text = readWholeFile(path, ok);
    if (ok && !text.empty()) {
        settings_ = layout::Settings::fromIni(text);
        diag::info("from " + path);
    } else {
        diag::info("no existing file, using defaults");
    }

    // Mit welcher Ansicht wird geoeffnet — Editor oder Bibliothek.
    //
    // Voreinstellung ist der Editor, wie im Original. Wir haben lange mit der
    // Bibliothek geoeffnet, weil sie das Auffaelligste war; wer den Editor
    // kennt, sucht aber zuerst seinen Arbeitsbereich.
    startTabActive_ = settings_.openLibraryOnStart;
    // Den Reiter auch WAEHLEN. Sonst nimmt ImGui im ersten Bild den ersten
    // Reiter — die Bibliothek —, und efxed oeffnete trotz Voreinstellung
    // dort (und ihre laufenden Kacheln hielten die Leerlaufbremse aus).
    wantStartTab_ = startTabActive_;
    wantDocumentTab_ = !startTabActive_;
    // "Reset Default FX Repeat Rate On Restart" (Voreinstellung an): jeder
    // Start beginnt bei 0.300 s. Die Einstellung wurde bisher gespeichert,
    // aber nie angewendet.
    if (settings_.resetRepeatRateOnStart) settings_.repeatRate = 0.300f;
    // Die Wiedergabe-Einstellungen von gestern. Sie wurden bisher nur
    // teilweise gespeichert ("repeat") und gar nicht angewendet.
    playback_.mode = static_cast<playback::RepeatMode>(settings_.playbackMode);
    playback_.repeatForSeconds = settings_.playDuration;
    playback_.respawnEveryFrame = settings_.perFrameRespawn;
    playback_.repeatRateSeconds = settings_.repeatRate;
    playback_.animateSpawnLocation = settings_.animateSpawnPoint;
    playback_.spawnVelocity = {settings_.spawnVelocity[0], settings_.spawnVelocity[1],
                               settings_.spawnVelocity[2]};
    playback_.resetLocationAfter = settings_.spawnResetSeconds;
    for (Document& d : documents_) d.clock.setEndMode(playbackEndMode());

    // Sprache: gespeicherte Wahl schlaegt Systemeinstellung.
    if (const auto* language = i18n::findLanguage(settings_.languageCode)) {
        i18n::setLanguage(language->language);
    }
    diag::info(std::string("theme ") + settings_.themeId + ", Sprache " +
               (settings_.languageCode.empty() ? "auto"
                                               : settings_.languageCode));

    // Wenn schon ein Spielpfad gespeichert ist, den Bestand gleich einlesen.
    // Das laeuft ueber den Arbeitsverteiler und kostet bei einer vollen
    // Installation den Bruchteil einer Sekunde, weil nur die Verzeichnisse
    // der .pk3-Dateien gelesen werden.
    if (!settings_.gamePath.empty()) rescanAssets();
}

void App::shutdown() {
    // Die Arbeitsfaeden halten `this`. Wer sie beim Beenden weiterlaufen
    // laesst, bekommt einen Zugriff auf ein Objekt, das gerade zerfaellt —
    // und zwar nicht immer, sondern manchmal.
    settleTextureJobs();

    // Die Wiedergabe-Einstellungen zurueck in die Datei.
    settings_.playbackMode = static_cast<int>(playback_.mode);
    settings_.repeat = playback_.mode != playback::RepeatMode::Once;
    settings_.playDuration = playback_.repeatForSeconds;
    settings_.perFrameRespawn = playback_.respawnEveryFrame;
    settings_.animateSpawnPoint = playback_.animateSpawnLocation;
    settings_.spawnVelocity[0] = playback_.spawnVelocity.x;
    settings_.spawnVelocity[1] = playback_.spawnVelocity.y;
    settings_.spawnVelocity[2] = playback_.spawnVelocity.z;
    settings_.spawnResetSeconds = playback_.resetLocationAfter;

    diag::Step step("Write settings");
    // Ganz oder gar nicht: vorher wurde die Datei erst geleert, und ein
    // Absturz mittendrin liess halbe Einstellungen zurueck (Spielpfad weg).
    if (!paths::writeFileReplacing(paths::settingsPath(), settings_.toIni())) {
        step.fail("file not writable");
    }
}

bool App::wantsRendererChange(render::Backend& target) const {
    target = rendererTarget_;
    return rendererChangePending_;
}

void App::clearRendererChange() { rendererChangePending_ = false; }

// ---------------------------------------------------------------------------
// Dateien


bool App::openFile(const std::string& path) {
    diag::Step step("Open file");
    diag::info(path);

    // Erst lesen, dann einen Reiter aufmachen: vorher blieb bei einer Datei,
    // die es nicht (mehr) gibt — etwa aus der Liste der letzten Dateien —
    // ein leerer Reiter zurueck, und der bisherige war nicht mehr vorn.
    bool ok = false;
    const std::string text = readWholeFile(path, ok);
    if (!ok) {
        step.fail("not readable");
        showNotice(std::string(tr(Str::MsgLoadFailed)) + "\n" + path);
        return false;
    }

    // In einen neuen Reiter, wenn der aktuelle schon belegt ist.
    //
    // Ein leerer, unveraenderter Reiter wird dagegen wiederverwendet — sonst
    // haette man nach dem Start immer einen leeren Reiter zu viel.
    if (!doc().filePath.empty() || doc().dirty ||
        !doc().effect.primitives.empty()) {
        newDocument();
    }

    ReadResult result = read(text);
    // Mit Aufbaufehlern (fehlende }, ...) liest das Spiel die Datei gar
    // nicht, und hier kommt ein leerer oder unvollstaendiger Effekt heraus.
    // Den darf "Speichern" nicht ueber die Datei schreiben — sonst ist das
    // Original weg. Speichern fragt dann nach einem neuen Namen.
    doc().unreadableOriginal = result.hasErrors();
    doc().effect = std::move(result.effect);
    doc().parseDiagnostics = std::move(result.diagnostics);
    // Eine laufende Vorschau gehoert zur alten Datei — die wird weggeworfen,
    // nicht nur angehalten.
    doc().particles.clear();
    doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
    doc().undo.reset(doc().effect);
    doc().filePath = path;
    // Auch beim Speichern eintragen: wer eine Datei unter neuem Namen
    // ablegt, will sie danach ebenso schnell wiederfinden.
    settings_.addRecentFile(path);
    doc().selectedPrimitive = doc().effect.primitives.empty() ? -1 : 0;
    doc().dirty = false;
    doc().savedUndoId = doc().undo.currentId();
    // Wie im Original: liegt die Datei unter ".../base/", ist das der
    // Spielpfad — aber nur, wenn noch keiner eingestellt ist. Ein gewaehlter
    // Pfad wird nie still ersetzt.
    if (settings_.gamePath.empty()) {
        const std::string derived = assets::gamePathFromFile(path);
        if (!derived.empty()) {
            settings_.gamePath = derived;
            diag::info("game path from file: " + derived);
            rescanAssets();
        }
    }
    refreshDiagnostics();

    buildPreviewStopped();
    // Wer eine Datei oeffnet, will sie sehen — nicht die Startseite. Beim
    // Doppelklick auf eine .efx blieb vorher die Bibliothek vorn.
    showEditor();

    diag::info(std::to_string(doc().effect.primitives.size()) + " primitives");
    return true;
}

bool App::saveFile(const std::string& path) {
    diag::Step step("Save file");

    WriteOptions options;
    const std::string text = write(doc().effect, options);

    // Nie etwas schreiben, das sich nicht wieder gleich einlesen laesst.
    // Dieselbe Vorsichtsmassnahme wie in efxtool format.
    ReadResult back = read(text);
    //
    // Scheitern wird ANGEZEIGT, nicht nur protokolliert: vorher tat Strg+S
    // dann sichtbar nichts (Fehlersuche 03.10.2026).
    if (back.hasErrors() ||
        back.effect.primitives.size() != doc().effect.primitives.size()) {
        step.fail("round-trip check failed, nothing written");
        std::string reason;
        for (const auto& d : back.diagnostics) {
            if (d.severity == Severity::Error) {
                reason = "\n" + d.message;
                break;
            }
        }
        showNotice(std::string(tr(Str::MsgSaveFailed)) + "\n" + path + reason);
        return false;
    }

    // Ganz oder gar nicht (paths::writeFileReplacing): vorher wurde die
    // Datei zuerst geleert, das Schreiben selbst nie geprueft — bei voller
    // Platte war das Original weg, und das Dokument galt als gespeichert.
    if (!paths::writeFileReplacing(path, text)) {
        step.fail("file not writable");
        showNotice(std::string(tr(Str::MsgSaveFailed)) + "\n" + path);
        return false;
    }
    doc().filePath = path;
    // Auch beim Speichern eintragen: wer eine Datei unter neuem Namen
    // ablegt, will sie danach ebenso schnell wiederfinden.
    settings_.addRecentFile(path);
    doc().dirty = false;
    doc().savedUndoId = doc().undo.currentId();
    // Jetzt liegt unter diesem Pfad ein vollstaendiger, lesbarer Effekt.
    doc().unreadableOriginal = false;
    return true;
}

void App::recordChange(const char* what) {
    doc().undo.record(doc().effect, what);
    refreshDiagnostics();
    doc().dirty = true;
}

void App::applyUndo() {
    flushPendingFieldEdit();
    if (const Effect* state = doc().undo.undo()) {
        doc().effect = *state;
        // Die Auswahl kann jetzt ins Leere zeigen.
        if (doc().selectedPrimitive >= static_cast<int>(doc().effect.primitives.size())) {
            doc().selectedPrimitive = doc().effect.primitives.empty()
                                     ? -1
                                     : static_cast<int>(doc().effect.primitives.size()) - 1;
        }
        doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
        refreshDiagnostics();
        // Geaendert heisst: weicht vom gespeicherten Stand ab. Vorher blieb
        // das Dokument nach Strg+Z "sauber", und Schliessen fragte nicht.
        doc().dirty = doc().undo.currentId() != doc().savedUndoId;
        if (playing()) startPlayback();
    }
}

void App::applyRedo() {
    flushPendingFieldEdit();
    if (const Effect* state = doc().undo.redo()) {
        doc().effect = *state;
        if (doc().selectedPrimitive >= static_cast<int>(doc().effect.primitives.size())) {
            doc().selectedPrimitive = doc().effect.primitives.empty()
                                     ? -1
                                     : static_cast<int>(doc().effect.primitives.size()) - 1;
        }
        doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
        refreshDiagnostics();
        doc().dirty = doc().undo.currentId() != doc().savedUndoId;
        if (playing()) startPlayback();
    }
}













std::string App::windowTitle() const {
    std::string name = doc().filePath.empty() ? "Untitled" : doc().filePath;
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    // Wie im Original (MFC): das " *" haengt am Dokumentnamen.
    return name + (doc().dirty ? " *" : "") + " - EffectsEd";
}

// ---------------------------------------------------------------------------
// Teiler
//
// ImGui hat keinen fertigen Splitter. SplitterBehavior in imgui_internal.h
// macht die Arbeit, erwartet aber Bildpunkte; wir rechnen in Anteilen, damit
// die Aufteilung einen Monitorwechsel uebersteht.


// ---------------------------------------------------------------------------
// Menue


// ---------------------------------------------------------------------------
// Werkzeugleiste — zwei Zeilen, wie im Original



// ---------------------------------------------------------------------------
// Die drei Bereiche

void App::updateFlagDrag(float width, float height) {
    // Die Fahne anfassen und verschieben.
    //
    // Der Strahl von der Maus in die Welt und sein Schnitt mit dem Boden —
    // beides steht seit dieser Runde in efx::camera und ist dort geprueft.
    // Hier bleibt nur die Bedienung.
    if (!settings_.drawWindVector) {
        flagHovered_ = false;
        draggingFlag_ = false;
        return;
    }
    {
        const camera::Vec3 foot{settings_.windFlagPos[0], settings_.windFlagPos[1],
                                roomSize_.floorZ};
        const ImVec2 mouse = ImGui::GetMousePos();
        const ImVec2 viewMin = ImGui::GetItemRectMin();

        const camera::Matrix viewMatrix = camera_.viewMatrix();
        const camera::Matrix projMatrix =
            camera_.projectionMatrix(width / std::max(1.0f, height),
                                     zeroToOneDepth_);
        const camera::Ray ray = camera::rayThroughPixel(
            viewMatrix, projMatrix, mouse.x - viewMin.x, mouse.y - viewMin.y, width,
            height);

        // Getroffen, wenn der Mast nah genug an der Mauslinie liegt. Der
        // Radius waechst mit der Entfernung — sonst waere eine weit entfernte
        // Fahne auf dem Bildschirm nur wenige Bildpunkte breit und praktisch
        // nicht zu treffen.
        const float distance = camera::length(foot - ray.origin);
        const float grabRadius = std::max(settings_.worldScale * 0.5f,
                                          distance * 0.05f);
        const camera::Vec3 grabPoint =
            foot + camera::Vec3{0.0f, 0.0f, settings_.worldScale * 1.8f};
        flagHovered_ = ImGui::IsItemHovered() &&
                       camera::distanceToRay(ray, grabPoint) < grabRadius;

        if (flagHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            draggingFlag_ = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) draggingFlag_ = false;

        if (draggingFlag_) {
            camera::Vec3 onGround;
            if (camera::intersectGroundPlane(ray, roomSize_.floorZ, onGround)) {
                settings_.windFlagPos[0] = onGround.x;
                settings_.windFlagPos[1] = onGround.y;
                clampWindFlagToRoom();
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        } else if (flagHovered_) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    }
}

particles::System::ShaderLookup App::particleShaderLookup() const {
    return [this](const std::string& name) -> particles::System::ShaderDraw {
        particles::System::ShaderDraw draw;
        draw.definition = assets_.shaderOf(name);
        // Eine Definition, deren Stufenbild fehlt, laedt die Engine nicht —
        // sie zeichnet dann das Ersatzkaestchen (assets::engineLoadsShader).
        if (draw.definition && assetsScanned_ && !assets::engineLoadsShader(assets_, *draw.definition)) {
            draw.definition = nullptr;
            draw.missing = true;
            return draw;
        }
        // Weder Shaderblock noch Bild: die Engine zeichnet tr.defaultShader,
        // das graue Kaestchen. Ohne Bestand (kein Spielpfad) wissen wir es
        // nicht — dann nicht als fehlend melden.
        if (!draw.definition && assetsScanned_ && !assets_.hasShader(name) &&
            !assets_.hasTexture(name)) {
            draw.missing = true;
        }
        return draw;
    };
}

int App::drawParticleGroups(render::Renderer* renderer, const particles::DrawList& list,
                            float seconds, int renderMode) {
    // Die Faktoren der Shaderstufe eins zu eins an die Grafikschnittstelle.
    const auto factor = [](shader::BlendFactor f) {
        switch (f) {
            case shader::BlendFactor::Zero: return render::BlendFactor::Zero;
            case shader::BlendFactor::One: return render::BlendFactor::One;
            case shader::BlendFactor::SrcColor: return render::BlendFactor::SrcColor;
            case shader::BlendFactor::OneMinusSrcColor: return render::BlendFactor::OneMinusSrcColor;
            case shader::BlendFactor::DstColor: return render::BlendFactor::DstColor;
            case shader::BlendFactor::OneMinusDstColor: return render::BlendFactor::OneMinusDstColor;
            case shader::BlendFactor::SrcAlpha: return render::BlendFactor::SrcAlpha;
            case shader::BlendFactor::OneMinusSrcAlpha: return render::BlendFactor::OneMinusSrcAlpha;
            case shader::BlendFactor::DstAlpha: return render::BlendFactor::DstAlpha;
            case shader::BlendFactor::OneMinusDstAlpha: return render::BlendFactor::OneMinusDstAlpha;
            case shader::BlendFactor::SrcAlphaSaturate: return render::BlendFactor::SrcAlphaSaturate;
            case shader::BlendFactor::Unset: break;
        }
        return render::BlendFactor::One;
    };

    // Die drei Darstellungsarten des Originals: texturiert, Drahtgitter,
    // Ueberzeichnung ("Shows areas of high polygon overlap" — jede Flaeche
    // als schwaches gleichmaessiges Additiv ohne Textur).
    const bool overdraw = renderMode == 2;
    renderer->setFill(renderMode == 1 ? render::Fill::Wireframe : render::Fill::Solid);
    int calls = 0;
    for (const auto& group : list.groups) {
        const scene::Mesh& mesh = group.mesh;
        if (mesh.vertices.empty()) continue;
        // Erst anfordern (textureFor gibt das Bild in Auftrag), dann fragen:
        // noch unterwegs? Dann diese Gruppe ein, zwei Bilder lang nicht
        // zeichnen — mit Ersatzbild blitzte sonst ein Rechteck auf. Die Bilder
        // der Shaderstufen kennt prefetchTextures nicht; in der anderen
        // Reihenfolge wuerden sie nie angefordert.
        if (group.clamp) clampImages_.insert(group.image);
        if (const shader::Shader* def = assets_.shaderOf(group.shader); def && def->noMipMaps) {
            // Schon mit Kette geladen (etwa vorab fuer eine Kachel)? Dann
            // wegwerfen und neu anfordern — sonst bliebe die Kette.
            if (noMipImages_.insert(group.image).second) {
                const auto cached = textureCache_.find(group.image);
                if (cached != textureCache_.end()) {
                    if (cached->second != render::kNoTexture) renderer->destroyTexture(cached->second);
                    textureCache_.erase(cached);
                }
            }
        }
        const render::TextureId texture = textureFor(renderer, group.image, seconds);
        if (!overdraw && textureStillLoading(group.image, seconds)) continue;

        if (overdraw) {
            renderer->setBlendFactors(render::BlendFactor::One, render::BlendFactor::One);
            renderer->setDepthTest(true);
            renderer->setDepthWrite(false);
            renderer->setAlphaTest(0);
        } else {
            renderer->setBlendFactors(group.blended ? factor(group.src) : render::BlendFactor::One,
                                      group.blended ? factor(group.dst) : render::BlendFactor::Zero);
            // depthHack: vor der Welt (RF_DEPTHHACK) — gegen den Raum ohne
            // Tiefentest und ohne Tiefe zu schreiben.
            renderer->setDepthTest(group.depthTest && !group.depthHack);
            renderer->setDepthWrite(group.depthWrite && !group.depthHack);
            renderer->setAlphaTest(group.alphaTest);
        }
        renderer->setCulling(group.cullBackFaces ? render::Cull::BackFaces : render::Cull::None);

        // Ohne Textur und Farbe im Ueberzeichnungsmodus. Die umgefaerbte
        // Fassung liegt in einem Member: der Zeiger muss den Zeichenaufruf
        // ueberleben.
        const scene::Vertex* vertices = mesh.vertices.data();
        if (overdraw) {
            overdrawScratch_ = mesh.vertices;
            constexpr uint32_t kStep = scene::rgba(21, 21, 21, 255);
            for (auto& v : overdrawScratch_) v.colour = kStep;
            vertices = overdrawScratch_.data();
        }
        renderer->drawTriangles(reinterpret_cast<const render::Vertex*>(vertices),
                                static_cast<int>(mesh.vertices.size()), mesh.indices.data(),
                                static_cast<int>(mesh.indices.size()),
                                overdraw ? render::kNoTexture : texture);
        ++calls;
    }
    renderer->setDepthTest(true);
    renderer->setDepthWrite(false);
    renderer->setAlphaTest(0);
    renderer->setFill(render::Fill::Solid);
    return calls;
}

void App::drawAxesOnTop(render::Renderer* renderer, const camera::Matrix& view, float viewHeight) {
    // Die Achsen wie im Original: 3 Bildpunkte breit (glLineWidth(3)) und
    // NACH dem Effekt, also obenauf. Direct3D 11 zeichnet keine Linien
    // breiter als ein Bildpunkt — darum schmale Baender, die zur Kamera
    // zeigen, mit einer Breite, die an jeder Stelle 3 Bildpunkten entspricht.
    if (!settings_.drawAxes || axisLines_.vertices.size() < 2) return;
    (void)view;
    const camera::Vec3 eye = camera_.position();
    const float perPixel =
        2.0f * std::tan(camera_.fovDegrees() * 0.5f * 3.14159265f / 180.0f) / std::max(1.0f, viewHeight);
    axisQuads_.vertices.clear();
    axisQuads_.indices.clear();
    for (size_t i = 0; i + 1 < axisLines_.vertices.size(); i += 2) {
        const scene::Vertex& a = axisLines_.vertices[i];
        const scene::Vertex& b = axisLines_.vertices[i + 1];
        const camera::Vec3 pa{a.pos[0], a.pos[1], a.pos[2]};
        const camera::Vec3 pb{b.pos[0], b.pos[1], b.pos[2]};
        const camera::Vec3 mid = (pa + pb) * 0.5f;
        const float halfWidth = 1.5f * perPixel * camera::length(eye - mid);
        camera::Vec3 side = camera::cross(pb - pa, eye - mid);
        if (camera::length(side) < 1e-6f) continue;   // Achse zeigt genau auf die Kamera
        side = camera::normalise(side) * halfWidth;
        const auto base = static_cast<uint16_t>(axisQuads_.vertices.size());
        for (const camera::Vec3& p : {pa - side, pa + side, pb + side, pb - side}) {
            scene::Vertex v = a;
            v.pos[0] = p.x;
            v.pos[1] = p.y;
            v.pos[2] = p.z;
            axisQuads_.vertices.push_back(v);
        }
        for (const int k : {0, 1, 2, 0, 2, 3}) axisQuads_.indices.push_back(static_cast<uint16_t>(base + k));
    }
    if (axisQuads_.vertices.empty()) return;
    renderer->setBlend(render::Blend::Opaque);
    renderer->setCulling(render::Cull::None);
    renderer->setFill(render::Fill::Solid);
    renderer->setDepthTest(false);
    renderer->setDepthWrite(false);
    renderer->drawTriangles(reinterpret_cast<const render::Vertex*>(axisQuads_.vertices.data()),
                            static_cast<int>(axisQuads_.vertices.size()), axisQuads_.indices.data(),
                            static_cast<int>(axisQuads_.indices.size()), render::kNoTexture);
    renderer->setDepthTest(true);
}

void App::drawViewport(render::Renderer* renderer, float width, float height) {
    ImGui::BeginChild("viewport", ImVec2(width, height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const float innerW = std::max(1.0f, width - 4.0f);
    const float innerH = std::max(1.0f, height - 4.0f);

    if (!renderer) {
        ImGui::TextDisabled("%s", tr(Str::ViewportComingSoon));
        ImGui::EndChild();
        return;
    }

    // Weltmassstab gewechselt (oder erstes Bild): wie im Original waechst der
    // Kameraabstand mit dem Massstab (10 -> 16 Einheiten je Fuss: 80 -> 128);
    // der Raum selbst bleibt gleich gross.
    if (settings_.worldScale != lastWorldScale_ && settings_.worldScale > 0.0f) {
        if (lastWorldScale_ <= 0.0f) {
            camera_.reset(settings_.worldScale);
        } else {
            camera_.setWorldScale(settings_.worldScale);
        }
    }
    if (geometryDirty_ || settings_.worldScale != lastWorldScale_) {
        rebuildGeometry();
        if (camera_.distance() <= 0.0f) camera_.reset(settings_.worldScale);
    }

    renderer->resizeViewport(static_cast<int>(innerW), static_cast<int>(innerH));

    // Erschuetterung weiterrechnen, bevor die Matrizen gebaut werden.
    const int nowMs = static_cast<int>(ImGui::GetTime() * 1000.0);
    shake_.update(nowMs, camera_.fovDegrees());
    camera_.setShakeOffset(shake_.originOffset(), shake_.pitchOffset(),
                           shake_.yawOffset());

    const theme::Palette& palette = activeTheme(settings_).palette;
    // Ueberzeichnung wie im Original: schwarzer Hintergrund, kein Raum —
    // nur so sieht man, wo sich die grauen Flaechen aufaddieren.
    const bool overdraw = settings_.effectRenderMode == 2;
    const float bgR = overdraw ? 0.0f : backgroundColourOverridden_ ? backgroundColour_[0] : palette.viewportBg.r;
    const float bgG = overdraw ? 0.0f : backgroundColourOverridden_ ? backgroundColour_[1] : palette.viewportBg.g;
    const float bgB = overdraw ? 0.0f : backgroundColourOverridden_ ? backgroundColour_[2] : palette.viewportBg.b;
    zeroToOneDepth_ = renderer->wantsZeroToOneDepth();
    // Vermerken, dass das Ansichtsziel jetzt der Bearbeitungsansicht gehoert.
    //
    // Browser und Editor teilen es sich — das war eine bewusste Entscheidung
    // ("beide sind nie gleichzeitig sichtbar, ein zweites Ziel waere Speicher
    // fuer nichts"), und sie stimmt weiterhin. Aber der Browser laesst seine
    // Kacheln ueber Bilder hinweg STEHEN, und dieser Aufruf leert das Ziel.
    //
    // Wer einen Effekt oeffnete und zur Startseite zurueckging, fand ein
    // leeres Raster: die Kacheln galten als schon gezeichnet, ihr Bild war
    // aber weg. Erst das Darueberfahren holte eine einzelne zurueck.
    viewportUsedByEditor_ = true;
    renderer->beginViewport(bgR, bgG, bgB, 1.0f);

    const camera::Matrix view = camera_.viewMatrix();
    const camera::Matrix proj =
        camera_.projectionMatrix(innerW / innerH, renderer->wantsZeroToOneDepth());
    renderer->setCamera(view.data(), proj.data());

    // Reihenfolge: erst der undurchsichtige Raum, dann die Linien darueber.
    // Andersherum verschwinden Gitter und Achsen hinter den Waenden.
    // Der Himmel zuerst, ohne Tiefenschreiben: er ist der Hintergrund, und
    // alles andere gehoert davor.
    if (!overdraw && !skyMesh_.vertices.empty()) {
        renderer->setBlend(render::Blend::Opaque);
        renderer->setCulling(render::Cull::BackFaces);
        renderer->setFill(render::Fill::Solid);
        renderer->setDepthWrite(false);
        renderer->drawTriangles(
            reinterpret_cast<const render::Vertex*>(skyMesh_.vertices.data()),
            static_cast<int>(skyMesh_.vertices.size()), skyMesh_.indices.data(),
            static_cast<int>(skyMesh_.indices.size()), render::kNoTexture);
    }
    if (!overdraw && !sunMesh_.vertices.empty()) {
        renderer->setCulling(render::Cull::None);
        renderer->drawTriangles(
            reinterpret_cast<const render::Vertex*>(sunMesh_.vertices.data()),
            static_cast<int>(sunMesh_.vertices.size()), sunMesh_.indices.data(),
            static_cast<int>(sunMesh_.indices.size()), render::kNoTexture);
    }

    // Die Wandtextur. Gerechnet statt geladen — das Original liefert
    // brick.jpg und die anderen mit, wir haben sie nicht.
    if (wallTextureKind_ != settings_.roomTexture) {
        if (wallTexture_ != render::kNoTexture) {
            renderer->destroyTexture(wallTexture_);
            wallTexture_ = render::kNoTexture;
        }
        const auto kind = static_cast<scene::WallTexture>(settings_.roomTexture);
        // Die gemessene Grundfarbe der Wandart, nicht Weiss — siehe
        // scene::wallColourOf.
        const auto pixels =
            scene::buildWallTexture(kind, 256, scene::wallColourOf(kind));
        if (!pixels.empty()) {
            // Ohne Mipmaps, wie das Original (GL_LINEAR, keine Mipmaps,
            // ROOM-GEOMETRY 4.1): mit ihnen verschwammen die Ziegel in der
            // Ferne zu einer flachen Flaeche, und Wand und Boden wirkten
            // unterschiedlich hell.
            wallTexture_ = renderer->createTexture(pixels.data(), 256, 256, false,
                                                   false);
        }
        wallTextureKind_ = settings_.roomTexture;
    }

    // Die Partikel VOR dem Raum aufbauen — gezeichnet werden sie danach.
    //
    // Der Grund sind die Lichter: ein Light-Segment erhellt die Waende
    // (CLight::Draw -> AddLightToScene), und dafuer muss der Raum wissen,
    // welche Lichter in diesem Bild leuchten, bevor er gezeichnet wird.
    //
    // Gezeichnet wird, solange die Uhr nicht steht — oder solange sie
    // irgendwo mitten im Effekt steht, weil man gespult hat.
    const bool showParticles = playing() || doc().clock.timeMs() > 0.0f;
    particles::DrawList list;
    float elapsed = 0.0f;
    if (showParticles) {
        // Die eigene Uhr statt der Wanduhr: springen, zurueckspulen und Bild
        // fuer Bild gehen setzen voraus, dass sich die Uhr verschieben laesst.
        // Die Geschwindigkeit steckt in der Uhr, nicht in der Rechnung hier.
        //
        // Die Dauer kommt aus der Simulation. Bei einem Effekt mit
        // `repeatDelay` ist das EINE Wiederholung: danach ist das Bild
        // identisch.
        doc().clock.setDuration(scheduleActive_ ? spawnScheduleDurationMs()
                                                : doc().particles.durationMs());
        doc().clock.advance(ImGui::GetIO().DeltaTime * 1000.0f);

        if (doc().clock.consumeWrapped()) {
            // Ein Durchlauf ist zu Ende. Neu ausloesen — aber nicht hier
            // (startPlayback plant neu, laedt nach, bricht Klaenge ab), sondern
            // vorgemerkt fuer das naechste Bild. Und NICHT bei `repeatDelay`:
            // dort ist das Bild nach einer Wiederholung identisch, ein
            // Neuplanen erzeugte genau den Sprung, den die Nahtlosigkeit
            // vermeidet. In der Fassung des alten Editors dagegen schon.
            if (doc().effect.repeatDelay < 1) {
                doc().pendingRestart = true;
            }
        }

        elapsed = doc().clock.timeMs();
        advanceSpawnSchedule(elapsed);

        // Die Spannachsen des Billboards kommen aus der Blickmatrix: die
        // ersten beiden Zeilen sind rechts und oben in Weltkoordinaten.
        const camera::Vec3 billboardRight{view[0], view[4], view[8]};
        const camera::Vec3 billboardUp{view[1], view[5], view[9]};

        triggerSounds(elapsed);
        // Und die Kameraerschuetterung — an derselben Stelle, weil beide
        // dasselbe Muster haben: einmal ausloesen, wenn ihre Zeit gekommen
        // ist, und nicht in jedem Bild erneut.
        triggerCameraShakes(elapsed);

        // Das Auge und das waagerechte Sichtfeld: Linien und Blitze stehen
        // quer zur Sichtlinie, Zylinder werden mit der Entfernung feiner, ein
        // ScreenFlash steht 8 Einheiten vor dem Auge. camera::Orbit rechnet
        // mit dem SENKRECHTEN Winkel; die Engine (fov_x) mit dem waagerechten.
        particles::System::View eye;
        eye.eye = camera_.position();
        eye.fovXDegrees =
            2.0f * std::atan(std::tan(camera_.fovDegrees() * 0.5f * 3.14159265f / 180.0f) *
                             (innerW / innerH)) *
            180.0f / 3.14159265f;
        const auto buildStart = std::chrono::steady_clock::now();
        list = doc().particles.build(elapsed, billboardRight, billboardUp,
                                     particleShaderLookup(), &eye);
        lastBuildMs_ = std::chrono::duration<float, std::milli>(
                           std::chrono::steady_clock::now() - buildStart).count();
        lastDrawn_ = list.drawn;
        lastAlive_ = list.alive;
        lastScheduled_ = list.scheduled;
        lastMarks_ = list.marks;
        lastDefaultGroups_ = 0;
        for (const auto& group : list.groups) {
            if (group.image == "$default") ++lastDefaultGroups_;
        }
    } else {
        lastDrawn_ = 0;
        lastAlive_ = 0;
        lastScheduled_ = 0;
        lastMarks_ = 0;
    }
    // Mit Gitter zeichnet das Original den Raum nur als Drahtgitter.
    if (!overdraw && !roomMesh_.vertices.empty() && !settings_.drawGrid) {
        renderer->setBlend(render::Blend::Opaque);

        // Der Raum schreibt Tiefe — er verdeckt, was hinter Boden, Decke und
        // Waenden liegt, wie im Original und im Spiel (Bildvergleich vom
        // 2.10.2026: das Original schneidet Teilchen genau an der Boden- und
        // Deckenlinie ab, z. B. concussion/explosion, blaster/shot).
        //
        // Frueher schrieb er keine Tiefe, weil das Feuer "halb im Boden"
        // steckte. Das lag am Boden bei z = 0: ein Teilchen entsteht im
        // Ursprung und ist auf seiner Lage zentriert. Seit dem Abgleich liegt
        // der Boden wie im Original bei z = -20, und die alte Messung ("70
        // Punkte unter dem Achsenkreuz") sind nur rund 14 Einheiten — sie
        // erreicht den Boden gar nicht.
        //
        // Ausnahmen stehen im Shader: Stufen mit "depthFunc disable" und
        // Bilder ohne Shaderblock zeichnen ohne Tiefentest (DrawGroup).
        renderer->setDepthWrite(true);
        // Der Raum mit Aussortierung: seine Flaechen zeigen nach innen, also
        // faellt beim Herauszoomen die naechstgelegene Wand weg und man
        // schaut in den Kasten hinein. Genau so verhaelt sich das Original.
        renderer->setCulling(render::Cull::BackFaces);
        // Drahtgitter gilt hier fuer den Raum. Die drei Knoepfe "Draw
        // Textured / Wireframe / Overdraw" betreffen dagegen die EFFEKTE —
        // ihre Hilfetexte im Original sagen ausdruecklich "effects are
        // rendered in wireframe". Ich hatte sie zuerst auf den Raum gelegt.
        // Der Raum wird immer gefuellt gezeichnet. Sein Umriss ist das
        // Bodengitter, nicht ein Drahtgitter der Waende — das sagt der
        // Hilfetext des Originals: "Draw outline of testing room".
        renderer->setFill(render::Fill::Solid);
        // Die Light-Segmente dieses Bildes erhellen die Raumflaechen — wie im
        // Spiel der Dlight-Durchgang ueber die Welt (CLight::Draw ->
        // AddLightToScene; im Original-Editor ist die Wand im Lichtkreis
        // aufgehellt).
        std::vector<render::Light> lights;
        for (const auto& light : list.lights) {
            if (static_cast<int>(lights.size()) >= render::kMaxLights) break;
            render::Light l{};
            l.pos[0] = light.origin.x;
            l.pos[1] = light.origin.y;
            l.pos[2] = light.origin.z;
            l.radius = light.radius;
            for (int k = 0; k < 3; ++k) l.rgb[k] = light.rgb[k];
            lights.push_back(l);
        }
        renderer->setLights(lights.data(), static_cast<int>(lights.size()));
        renderer->drawTriangles(
            reinterpret_cast<const render::Vertex*>(roomMesh_.vertices.data()),
            static_cast<int>(roomMesh_.vertices.size()), roomMesh_.indices.data(),
            static_cast<int>(roomMesh_.indices.size()), wallTexture_);
        renderer->setLights(nullptr, 0);
    }

    if (!overdraw && settings_.drawGrid && !gridLines_.vertices.empty()) {
        // Linien haben keine Rueckseite.
        renderer->setCulling(render::Cull::None);
        renderer->setFill(render::Fill::Solid);
        renderer->setBlend(render::Blend::AlphaBlend);
        renderer->setDepthWrite(false);
        renderer->drawLines(
            reinterpret_cast<const render::Vertex*>(gridLines_.vertices.data()),
            static_cast<int>(gridLines_.vertices.size()), 1.0f);
    }


    if (settings_.drawWindVector) {
        // Jedes Bild neu: die Fahne weht. Zwoelf Segmente sind nichts, was
        // sich zu zwischenspeichern lohnte.
        const theme::Palette& windPalette = activeTheme(settings_).palette;
        windLines_ = scene::buildWindFlag(
            {settings_.windDirection[0], settings_.windDirection[1],
             settings_.windDirection[2]},
            3.0f * settings_.worldScale,
            // Die Fahne weht nach der Wanduhr — sie gehoert nicht zum
            // Effekt. Aber die Geschwindigkeit der Zeitleiste gilt auch fuer
            // sie: bei Zeitlupe soll alles langsamer werden, sonst wirkt das
            // Bild auseinandergerissen.
            static_cast<float>(ImGui::GetTime()) * doc().clock.speed(),
            settings_.windSpeed,
            // Angefasst oder unter der Maus: heller, damit man sieht, dass
            // sie sich verschieben laesst. Ohne das Zeichen probiert es
            // niemand aus.
            packColour(flagHovered_ || draggingFlag_ ? windPalette.text
                                                     : windPalette.textDim),
            packColour(windPalette.accent),
            camera::Vec3{settings_.windFlagPos[0], settings_.windFlagPos[1],
                         roomSize_.floorZ});
    }
    if (settings_.drawWindVector && !windLines_.vertices.empty()) {
        renderer->drawLines(
            reinterpret_cast<const render::Vertex*>(windLines_.vertices.data()),
            static_cast<int>(windLines_.vertices.size()), 1.0f);
    }

    // Die Partikel. Nach dem Raum, damit sie davor liegen — je Shaderstufe
    // eine Gruppe, in der Reihenfolge der Engine (particles::DrawGroup).
    if (showParticles) {
        renderer->setCulling(render::Cull::None);
        const auto drawStart = std::chrono::steady_clock::now();
        drawParticleGroups(renderer, list, elapsed * 0.001f, settings_.effectRenderMode);
        lastDrawMs_ = std::chrono::duration<float, std::milli>(
                          std::chrono::steady_clock::now() - drawStart).count();
        // Texturiert UND Drahtgitter: im Original liegen die Kanten ueber dem
        // gefuellten Bild (beide Knoepfe sind unabhaengig).
        if (settings_.effectRenderMode == 0 && settings_.effectWireframe && settings_.effectTextured) {
            drawParticleGroups(renderer, list, elapsed * 0.001f, 1);
        }
    }

    // Die Achsen zuletzt, obenauf (wie im Original).
    drawAxesOnTop(renderer, view, innerH);

    renderer->setDepthWrite(true);
    renderer->endViewport();

    // Das Bildschirmfoto NACH endViewport: vorher steht das Bild noch nicht
    // fertig im Puffer, und man bekaeme die halbe Zeichnung.
    if (pendingScreenshot_ != 0) {
        takeScreenshot(renderer, pendingScreenshot_ == 2);
        pendingScreenshot_ = 0;
    }

    const render::TextureId texture = renderer->viewportTexture();
    if (texture != render::kNoTexture) {
        // Texturkoordinaten drehen, wenn der Renderer es meldet. Bei OpenGL
        // liegt der Ursprung unten links, ImGui erwartet ihn oben links —
        // ohne das steht die ganze Ansicht auf dem Kopf.
        const bool flipped = renderer->viewportTextureFlipped();
        const ImVec2 uv0(0.0f, flipped ? 1.0f : 0.0f);
        const ImVec2 uv1(1.0f, flipped ? 0.0f : 1.0f);
        ImGui::Image(toImTexture(texture), ImVec2(innerW, innerH), uv0, uv1);
        testmarke::marke("ansicht");

        // Erst die Fahne, dann die Kamera: beide haengen an der linken
        // Maustaste, und wer die Fahne greift, will nicht drehen.
        //
        // Beides muss NACH ImGui::Image stehen — IsItemHovered() und
        // GetItemRectMin() beziehen sich auf das zuletzt eingereichte
        // Element. Vorher gefragt, antworten sie ueber irgendetwas anderes.
        updateFlagDrag(innerW, innerH);
        handleViewportInput(innerW, innerH);
    } else {
        ImGui::TextDisabled("%s", tr(Str::ViewportComingSoon));
    }

    ImGui::EndChild();
}



// ---------------------------------------------------------------------------
// Bausteine fuer die Eigenschaftenblaetter
//
// Fast jedes Feld einer .efx-Datei ist entweder ein fester Wert oder eine
// Spanne, aus der beim Abspielen gewuerfelt wird. Ravens Blaetter haben dafuer
// zwei Spalten "Min" und "Max" — auch dort, wo nur eine Zahl steht.
//
// Hier stattdessen ein Ankreuzfeld: solange es aus ist, gibt es eine Zahl.
// Damit sieht man auf einen Blick, welche Felder wuerfeln — und genau das ist
// beim Lesen einer fremden Effektdatei die erste Frage.





namespace {

// Die Uebergangsarten. nonlinear, wave und clamp teilen sich zwei Bits, sind
// also gegenseitig ausschliessend — im Original steht das nirgends, und wer
// zwei ankreuzt, bekommt stillschweigend eine dritte Art.


}  // namespace








// ---------------------------------------------------------------------------












// ---------------------------------------------------------------------------

void App::buildFrame(render::Renderer* renderer, int windowWidth, int windowHeight,
                     float dpiScale) {
    renderer_ = renderer;
    dpiScale_ = dpiScale > 0.0f ? dpiScale : 1.0f;
    // Vorgemerkte Texturen erst hier freigeben, wo der Renderer bekannt ist.
    //
    // Am Anfang jedes Bildes, nicht in drawViewport: dort lief es nur in der
    // Editoransicht, und die Kacheln der Startseite zeigten nach einem neuen
    // Spielpfad oder .pk3 weiter die alten Bilder (Fehlersuche 03.10.2026).
    if (textureCacheDirty_ && renderer) {
        clearTextures(renderer);
        textureCacheDirty_ = false;
    }
    openQueuedDrops();
    // Die Hilfezeile gilt nur fuer das Bild, in dem die Maus darueber steht.
    statusHint_ = nullptr;
    // Eine Feldaenderung ist abgeschlossen, sobald kein Feld mehr aktiv ist.
    if (fieldEditOpen_ && !ImGui::IsAnyItemActive()) {
        fieldEditOpen_ = false;
        recordChange(tr(Str::UndoFieldChange));
    }
    if (previewDirty_) {
        previewDirty_ = false;
        refreshPreview();
    }
    finishPlayOut();
    settings_.updateRenderMode();
    // Vorgemerkte Wiederholung erledigen, bevor gezeichnet wird.
    //
    // Der Neustart plant den ganzen Effekt neu und laedt gegebenenfalls
    // Dateien nach. Mitten im Zeichnen war das der falsche Ort.
    if (doc().pendingRestart) {
        doc().pendingRestart = false;
        const float speed = doc().clock.speed();
        startPlayback();
        doc().clock.setSpeed(speed);   // die eingestellte Geschwindigkeit behalten
    }

    // Ergebnisse aus den Arbeitsfaeden abholen. Genau hier und nirgends
    // sonst — der einzige Ort, an dem ein Arbeitsfaden die Oberflaeche
    // beruehren darf.
    jobs::pool().drainMainQueue(4);

    // Fertig dekodierte Texturen anlegen. Muss hier stehen und nicht im
    // Arbeitsfaden: Texturen anzulegen ist Sache des Renderers.
    collectDecodedTextures(renderer);

    // Und die gelesenen Effekte zusammenbauen — ebenfalls hier, weil `play`
    // gemeinsame Zwischenspeicher anfasst.
    collectLoadedEffects();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::Begin("##main", nullptr, flags);
    ImGui::PopStyleVar();

    drawMenuBar();
    drawToolbar(dpiScale);

    // Die Reiter unter der Werkzeugleiste: sie gehoeren zum Dokument, nicht
    // zum Fenster, und stehen deshalb unter allem, was fuer alle Dokumente
    // gilt.
    drawDocumentTabs();
    ImGui::Separator();

    // Die Teiler auf das Fenster begrenzen, bevor gerechnet wird.
    settings_.split.clampTo(static_cast<float>(windowWidth),
                            static_cast<float>(windowHeight), dpiScale);

    const float thickness = layout::Split::kSplitterPx * dpiScale;
    const float minPanel = layout::Split::kMinPanelPx * dpiScale;
    const float minView = layout::Split::kMinViewPx * dpiScale;

    const float statusHeight = ImGui::GetFrameHeightWithSpacing();
    const ImVec2 area = ImGui::GetContentRegionAvail();
    const float bodyHeight = area.y - statusHeight;

    // Auf dem Startreiter die Kacheln statt der Bearbeitungsflaeche.
    //
    // Kein zweites Fenster, kein Umschalten ueber ein Menue: der Startreiter
    // IST die Seite. Wer zurueckwill, klickt ihn an — wie im Browser.
    if (startTabActive_) {
        ImGui::BeginChild("startPage", ImVec2(0.0f, bodyHeight));
        drawStartPage(renderer, dpiScale);
        ImGui::EndChild();
        drawStatusBar();
        // KEIN return hier.
        //
        // Der erste Entwurf sprang hier heraus — und uebersprang damit das
        // ImGui::End() des Hauptfensters weiter unten. ImGui merkt das und
        // haengt eine Fehlermeldung an den Mauszeiger, die einem durchs Bild
        // folgt. Genau die war zu sehen.
        //
        // Ein frueher Ausstieg zwischen Begin und End ist immer falsch; die
        // beiden gehoeren zusammen wie oeffnende und schliessende Klammer.
        // Die Dialoge gehoeren AUCH hierher.
        //
        // Sie standen nur im Bearbeitungszweig — und der Startreiter springt
        // vorher heraus. Ergebnis: auf der Startseite liess sich kein
        // Spielpfad setzen, kein Archiv oeffnen, keine Grafikschnittstelle
        // wechseln. Die Schalter setzten ihr Merkzeichen, aber niemand hat es
        // je gelesen.
        //
        // Ausgerechnet auf der Startseite ist das am schlimmsten: sie ist die
        // Seite, auf der man den Spielpfad ueberhaupt erst einstellt.
        drawDialogs();
        ImGui::End();
        handleShortcuts();
        return;
    }

    // Senkrechter Teiler: links Ansicht + Liste, rechts Eigenschaften.
    float leftFraction = 1.0f - settings_.split.propertiesFraction;
    const float leftWidth = std::max(minView, area.x * leftFraction - thickness);
    const float rightWidth = std::max(minPanel, area.x - leftWidth - thickness);

    ImGui::BeginChild("leftColumn", ImVec2(leftWidth, bodyHeight),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    {
        // Waagerechter Teiler: oben Ansicht, unten Liste.
        const float columnHeight = ImGui::GetContentRegionAvail().y;
        float listFraction = settings_.split.listFraction;
        const float listHeight = std::max(minPanel, columnHeight * listFraction - thickness);
        const float viewHeight = std::max(minView, columnHeight - listHeight - thickness);

        // Die Zeitleiste sitzt zwischen Ansicht und Segmentliste. Ihre Hoehe
        // geht von der Ansicht ab, nicht von der Liste — die Liste ist ohnehin
        // knapp, und wer den Trenner verschiebt, erwartet, dass die Aufteilung
        // bleibt.
        const float timelineHeight = ImGui::GetFrameHeightWithSpacing() +
                                     ImGui::GetStyle().ItemSpacing.y;
        drawViewport(renderer, ImGui::GetContentRegionAvail().x,
                     std::max(minView, viewHeight - timelineHeight));
        drawTimeline(dpiScale);

        float viewPx = viewHeight;
        float listPx = listHeight;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImRect bar;
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        bar.Min = ImVec2(cursor.x, cursor.y);
        bar.Max = ImVec2(cursor.x + ImGui::GetContentRegionAvail().x, cursor.y + thickness);
        if (ImGui::SplitterBehavior(bar, window->GetID("splitTB"), ImGuiAxis_Y,
                                    &viewPx, &listPx, minView, minPanel, 2.0f)) {
            settings_.split.listFraction = listPx / columnHeight;
        }
        ImGui::Dummy(ImVec2(0.0f, thickness));

        drawSegmentList(ImGui::GetContentRegionAvail().x,
                        ImGui::GetContentRegionAvail().y);
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    {
        float leftPx = leftWidth;
        float rightPx = rightWidth;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImRect bar;
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        bar.Min = ImVec2(cursor.x, cursor.y);
        bar.Max = ImVec2(cursor.x + thickness, cursor.y + bodyHeight);
        if (ImGui::SplitterBehavior(bar, window->GetID("splitLR"), ImGuiAxis_X,
                                    &leftPx, &rightPx, minView, minPanel, 2.0f)) {
            settings_.split.propertiesFraction = rightPx / area.x;
        }
        ImGui::Dummy(ImVec2(thickness, 0.0f));
    }

    ImGui::SameLine(0.0f, 0.0f);
    drawProperties(rightWidth, bodyHeight);

    if (settings_.showStatusBar) {
        ImGui::Separator();
        drawStatusBar();
    }

    drawDialogs();
    ImGui::End();

    handleShortcuts();
}

// Tastenkuerzel. Nur wenn kein Eingabefeld den Fokus hat, sonst tippt man in
// einen Namen und loest dabei die Wiedergabe aus.
//
// Eine eigene Funktion, weil es zwei Wege durch buildFrame gibt und beide sie
// brauchen. Als eingebauter Block lief sie nur auf einem davon — auf der
// Startseite tat Strg+B nichts, und das ist genau die Taste, die von dort
// wegfuehrt.
void App::handleShortcuts() {
    // WantTextInput statt WantCaptureKeyboard: nur waehrend getippt wird,
    // gehoeren die Tasten dem Feld. Vorher genuegte ein einziger Klick auf
    // irgendein Element, und die Leertaste kam nie mehr an — im Original
    // gilt sie "application wide", egal wo der Fokus steht.
    const ImGuiIO& io = ImGui::GetIO();
    // Nur fuer dieses Bild (auch wenn gleich ein return kommt).
    const bool listTookDelete = deleteKeyConsumed_;
    deleteKeyConsumed_ = false;
    if (io.WantTextInput) return;
    // Waehrend ein Fenster wie "Neues Segment" offen ist, gehoeren die Tasten
    // dem Fenster.
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) return;

    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;
    const auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };

    if (ctrl && pressed(ImGuiKey_N)) cmdNew();
    if (ctrl && pressed(ImGuiKey_O)) cmdOpen();
    if (ctrl && pressed(ImGuiKey_S)) cmdSave();
    if (ctrl && pressed(ImGuiKey_W)) requestCloseDocument(activeDocument_);
    if (ctrl && pressed(ImGuiKey_T)) newDocument();
    if (pressed(ImGuiKey_F5)) rescanAssets();
    if (ctrl && pressed(ImGuiKey_B)) {
        // Wie der Menuepunkt: ein- und ausschalten.
        startTabActive_ = !startTabActive_;
        wantStartTab_ = startTabActive_;   // einmalig auswaehlen
        if (startTabActive_ && browserEntries_.empty()) refreshBrowser();
    }
    if (ctrl && pressed(ImGuiKey_Tab) && !documents_.empty()) {
        // Zum naechsten Reiter, wie in jedem Browser.
        activateDocument((activeDocument_ + 1) % static_cast<int>(documents_.size()));
    }

    // Was folgt, betrifft den Effekt — auf der Startseite gibt es keinen.
    if (startTabActive_) return;

    // Alt+Ruecktaste steht in der Accelerator-Tabelle des Originals (Undo).
    if ((ctrl && pressed(ImGuiKey_Z)) || (io.KeyAlt && pressed(ImGuiKey_Backspace))) applyUndo();
    if (ctrl && pressed(ImGuiKey_Y)) applyRedo();
    // Klonen: Strg+C und Strg+Einfg wie im Original (Accelerator-Tabelle
    // 128), Strg+D zusaetzlich.
    if (ctrl && !shift && (pressed(ImGuiKey_C) || pressed(ImGuiKey_Insert) || pressed(ImGuiKey_D))) {
        cmdCloneSegment();
    }
    // Loeschen: Strg+Entf und Umschalt+Entf wie im Original; Entf allein
    // zusaetzlich (im Original tat es nichts).
    if ((ctrl || shift) && pressed(ImGuiKey_Delete)) cmdDeleteSegment();
    if (!ctrl && !shift && pressed(ImGuiKey_Space)) pressPlay();
    if (!ctrl && pressed(ImGuiKey_Insert)) showNewSegmentDialog_ = true;
    // Nicht, wenn eine Liste (Shader, Klaenge, Modelle) die Taste in diesem
    // Bild schon fuer ihren Eintrag genommen hat.
    if (!ctrl && !shift && pressed(ImGuiKey_Delete) && !listTookDelete) cmdDeleteSegment();
    if (pressed(ImGuiKey_F2)) startRename(doc().selectedPrimitive);
    // Shift+C wie im Original; Strg+Umschalt+C in die Zwischenablage.
    if (shift && !ctrl && pressed(ImGuiKey_C)) pendingScreenshot_ = 1;
    if (shift && ctrl && pressed(ImGuiKey_C)) pendingScreenshot_ = 2;
}

}  // namespace efx::gui
