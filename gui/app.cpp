#include "app.h"
#include "app_shared.h"

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
    std::ifstream in(path, std::ios::binary);
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
    const float halfWidth = roomSize_.widthFeet * settings_.worldScale * 0.45f;
    const float halfDepth = roomSize_.depthFeet * settings_.worldScale * 0.45f;
    settings_.windFlagPos[0] =
        std::clamp(settings_.windFlagPos[0], -halfWidth, halfWidth);
    settings_.windFlagPos[1] =
        std::clamp(settings_.windFlagPos[1], -halfDepth, halfDepth);
}

bool App::playing() const {
    return doc().clock.state() != timeline::State::Stopped;
}

std::string Document::title() const {
    // Uebersetzt, nicht fest verdrahtet — der Reiter ist Oberflaeche wie
    // jede andere.
    if (filePath.empty()) return i18n::tr(i18n::Str::TabUnnamed);
    const size_t cut = filePath.find_last_of("/\\");
    return cut == std::string::npos ? filePath : filePath.substr(cut + 1);
}

void App::newDocument() {
    documents_.emplace_back();
    activeDocument_ = static_cast<int>(documents_.size()) - 1;
    doc().undo.reset(doc().effect);
}

void App::activateDocument(int index) {
    if (index < 0 || index >= static_cast<int>(documents_.size())) return;
    if (index == activeDocument_) return;
    // Die Wiedergabe des bisherigen Reiters anhalten. Sonst laufen Klaenge
    // eines Effekts weiter, den man gar nicht mehr sieht.
    doc().clock.stop();
    doc().particles.stop();   // Geometrie behalten: der Reiter kommt wieder
    audio_.stopAll();
    activeDocument_ = index;
    // Auch die Reiterleiste umschalten (Strg+Tab, Speichern-Frage).
    wantDocumentTab_ = true;
}

void App::closeDocument(int index) {
    if (index < 0 || index >= static_cast<int>(documents_.size())) return;
    documents_.erase(documents_.begin() + index);
    if (documents_.empty()) documents_.emplace_back();
    if (activeDocument_ >= static_cast<int>(documents_.size())) {
        activeDocument_ = static_cast<int>(documents_.size()) - 1;
    }
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
    gridLines_ = scene::buildGrid(roomSize_, worldScale, packColour(p.grid),
                                  packColour(p.grid, 1.6f),
                                  settings_.gridOnWalls);
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

    diag::Step step("Write settings");
    std::ofstream out(paths::settingsPath(), std::ios::binary);
    if (out.good()) {
        out << settings_.toIni();
    } else {
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
    // In einen neuen Reiter, wenn der aktuelle schon belegt ist.
    //
    // Ein leerer, unveraenderter Reiter wird dagegen wiederverwendet — sonst
    // haette man nach dem Start immer einen leeren Reiter zu viel.
    if (!doc().filePath.empty() || doc().dirty ||
        !doc().effect.primitives.empty()) {
        newDocument();
    }

    diag::Step step("Open file");
    diag::info(path);

    bool ok = false;
    const std::string text = readWholeFile(path, ok);
    if (!ok) {
        step.fail("not readable");
        return false;
    }

    ReadResult result = read(text);
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
    if (back.hasErrors() ||
        back.effect.primitives.size() != doc().effect.primitives.size()) {
        step.fail("round-trip check failed, nothing written");
        return false;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.good()) {
        step.fail("file not writable");
        return false;
    }
    out << text;
    doc().filePath = path;
    // Auch beim Speichern eintragen: wer eine Datei unter neuem Namen
    // ablegt, will sie danach ebenso schnell wiederfinden.
    settings_.addRecentFile(path);
    doc().dirty = false;
    return true;
}

void App::recordChange(const char* what) {
    doc().undo.record(doc().effect, what);
    refreshDiagnostics();
    doc().dirty = true;
}

void App::applyUndo() {
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
        if (playing()) startPlayback();
    }
}

void App::applyRedo() {
    if (const Effect* state = doc().undo.redo()) {
        doc().effect = *state;
        if (doc().selectedPrimitive >= static_cast<int>(doc().effect.primitives.size())) {
            doc().selectedPrimitive = doc().effect.primitives.empty()
                                     ? -1
                                     : static_cast<int>(doc().effect.primitives.size()) - 1;
        }
        doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
        refreshDiagnostics();
        if (playing()) startPlayback();
    }
}













std::string App::windowTitle() const {
    std::string name = doc().filePath.empty() ? "Untitled" : doc().filePath;
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    return (doc().dirty ? "*" : "") + name + " - EffectsEd";
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
                                0.0f};
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
            if (camera::intersectGroundPlane(ray, 0.0f, onGround)) {
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

    // Vorgemerkte Texturen erst hier freigeben, wo der Renderer bekannt ist.
    if (textureCacheDirty_) {
        clearTextures(renderer);
        textureCacheDirty_ = false;
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
    const float bgR = backgroundColourOverridden_ ? backgroundColour_[0] : palette.viewportBg.r;
    const float bgG = backgroundColourOverridden_ ? backgroundColour_[1] : palette.viewportBg.g;
    const float bgB = backgroundColourOverridden_ ? backgroundColour_[2] : palette.viewportBg.b;
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
    if (!skyMesh_.vertices.empty()) {
        renderer->setBlend(render::Blend::Opaque);
        renderer->setCulling(render::Cull::BackFaces);
        renderer->setFill(render::Fill::Solid);
        renderer->setDepthWrite(false);
        renderer->drawTriangles(
            reinterpret_cast<const render::Vertex*>(skyMesh_.vertices.data()),
            static_cast<int>(skyMesh_.vertices.size()), skyMesh_.indices.data(),
            static_cast<int>(skyMesh_.indices.size()), render::kNoTexture);
    }
    if (!sunMesh_.vertices.empty()) {
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
            scene::buildWallTexture(kind, 128, scene::wallColourOf(kind));
        if (!pixels.empty()) {
            wallTexture_ = renderer->createTexture(pixels.data(), 128, 128, false,
                                                   true);
        }
        wallTextureKind_ = settings_.roomTexture;
    }

    if (!roomMesh_.vertices.empty()) {
        renderer->setBlend(render::Blend::Opaque);

        // Der Raum schreibt KEINE Tiefe — er verdeckt den Effekt nicht.
        //
        // Gemessen am Original (Bildschirmfoto mit Raum und laufendem
        // Effekt): dort reichen die Teilchen bis 70 Punkte UNTER das
        // Achsenkreuz und werden trotzdem vollstaendig gezeichnet. Der
        // Testraum ist dort also nur Kulisse, kein Hindernis.
        //
        // Bei uns schrieb der Boden Tiefe, und alles darunter verschwand. Ein
        // Teilchen entsteht bei z=0 und ist auf seiner Lage ZENTRIERT — die
        // untere Haelfte liegt damit zwangslaeufig unter dem Boden
        // (RB_SurfaceSprite spannt das Viereck mit dem vollen Radius nach
        // jeder Seite auf). Beim Anwender sah das aus, als saenke das Feuer
        // ein: „das Feuer ist halb im Boden, und das ist beim Original nicht
        // so".
        //
        // Ohne Tiefenschreiben geht nichts kaputt: der Raum ist ein KONVEXER
        // Kasten, von innen betrachtet und mit Rueckseiten-Aussortierung. Die
        // sichtbaren Flaechen ueberlappen einander nicht, sie brauchen den
        // Tiefenpuffer also gar nicht untereinander.
        renderer->setDepthWrite(false);
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
        renderer->drawTriangles(
            reinterpret_cast<const render::Vertex*>(roomMesh_.vertices.data()),
            static_cast<int>(roomMesh_.vertices.size()), roomMesh_.indices.data(),
            static_cast<int>(roomMesh_.indices.size()), wallTexture_);
    }

    if (settings_.drawGrid && !gridLines_.vertices.empty()) {
        // Linien haben keine Rueckseite.
        renderer->setCulling(render::Cull::None);
        renderer->setFill(render::Fill::Solid);
        renderer->setBlend(render::Blend::AlphaBlend);
        renderer->setDepthWrite(false);
        renderer->drawLines(
            reinterpret_cast<const render::Vertex*>(gridLines_.vertices.data()),
            static_cast<int>(gridLines_.vertices.size()), 1.0f);
    }

    if (settings_.drawAxes && !axisLines_.vertices.empty()) {
        renderer->setBlend(render::Blend::Opaque);
        renderer->setDepthWrite(false);
        renderer->drawLines(
            reinterpret_cast<const render::Vertex*>(axisLines_.vertices.data()),
            static_cast<int>(axisLines_.vertices.size()), 2.0f);
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
                         0.0f});
    }
    if (settings_.drawWindVector && !windLines_.vertices.empty()) {
        renderer->drawLines(
            reinterpret_cast<const render::Vertex*>(windLines_.vertices.data()),
            static_cast<int>(windLines_.vertices.size()), 1.0f);
    }

    // Die Partikel. Nach dem Raum, damit sie davor liegen, und ohne
    // Tiefenschreiben — durchsichtige Flaechen, die sich gegenseitig
    // verdecken, sehen falsch aus.
    // Gezeichnet wird, solange die Uhr nicht steht — oder solange sie
    // irgendwo mitten im Effekt steht, weil man gespult hat.
    if (playing() || doc().clock.timeMs() > 0.0f) {
        // Die eigene Uhr statt der Wanduhr.
        //
        // Bis zur Zeitleiste kam die Zeit aus ImGui::GetTime() — verstrichene
        // Wanduhrzeit seit dem Start. Das reicht zum Abspielen und fuer sonst
        // nichts: springen, zurueckspulen und Bild fuer Bild gehen setzen
        // voraus, dass sich die Uhr verschieben laesst.
        //
        // Die Geschwindigkeit steckt in der Uhr, nicht in der Rechnung hier:
        // so laeuft derselbe Effekt langsamer ab, statt anders geplant zu
        // werden. Zeitlupe soll zeigen, was ohnehin passiert.
        // Wie in startPlayback: beim Wiederholen ueber genau eine
        // Wiederholung. Beide Stellen muessen dasselbe sagen, sonst wandert
        // die Dauer im laufenden Betrieb wieder auf die volle Lebensdauer
        // zurueck und der Sprung ist wieder da.
        {
            // Die Dauer kommt aus der Simulation.
            //
            // Bei einem Effekt mit `repeatDelay` ist das EINE Wiederholung:
            // danach ist das Bild identisch, weil alle Generationen denselben
            // Ausgangswert haben. Die Bildzahl bleibt damit konstant, jeder
            // Zeitpunkt laesst sich anfahren, und nichts wird schwaecher.
            doc().clock.setDuration(doc().particles.durationMs());
        }
        doc().clock.advance(ImGui::GetIO().DeltaTime * 1000.0f);

        if (doc().clock.consumeWrapped()) {
            // Ein Durchlauf ist zu Ende. Neu ausloesen, damit ein Effekt mit
            // Spannen bei jeder Wiederholung anders aussieht.
            //
            // Aber NICHT hier: startPlayback plant den ganzen Effekt neu,
            // laedt untergeordnete Dateien nach und bricht Klaenge ab — mitten
            // im Zeichnen ist das der falsche Ort. Bei einem grossen Effekt
            // stockt das Bild an genau der Stelle, an der die Wiederholung
            // beginnt, und das sieht aus, als liefe nichts mehr.
            //
            // Stattdessen vormerken und im naechsten Bild erledigen, bevor
            // gezeichnet wird.
            //
            // ABER nicht bei einem Effekt mit `repeatDelay`: dort ist das Bild
            // nach einer Wiederholung identisch, und ein Neuplanen wuerde
            // genau den Sprung erzeugen, den die Nahtlosigkeit vermeidet.
            // Dreimal je Sekunde alles neu zu wuerfeln war der Grund, warum es
            // gegenueber dem Original unruhig aussah.
            //
            // In der Fassung des alten Editors dagegen SCHON: dort ist das
            // Neuausloesen der ganze Punkt.
            if (doc().effect.repeatDelay < 1 || settings_.legacyRepeat) {
                doc().pendingRestart = true;
            }
        }

        const float elapsed = doc().clock.timeMs();

        // Die Spannachsen des Billboards kommen aus der Blickmatrix: die
        // ersten beiden Zeilen sind rechts und oben in Weltkoordinaten.
        const camera::Vec3 billboardRight{view[0], view[4], view[8]};
        const camera::Vec3 billboardUp{view[1], view[5], view[9]};

        triggerSounds(elapsed);
        // Und die Kameraerschuetterung — an derselben Stelle, weil beide
        // dasselbe Muster haben: einmal ausloesen, wenn ihre Zeit gekommen
        // ist, und nicht in jedem Bild erneut.
        triggerCameraShakes(elapsed);

        const particles::DrawList list =
            doc().particles.build(
                elapsed, billboardRight, billboardUp,
                // Die `tcMod`-Regeln aus den Shaderdateien. Ohne diesen
                // Rueckruf stehen scrollende Texturen still — und das tun
                // sie, seit die Regeln umgesetzt wurden.
                [this](const std::string& name)
                    -> particles::System::ShaderDraw {
                    return {&assets_.texModsOf(name), assets_.rgbWaveOf(name),
                            assets_.alphaWaveOf(name)};
                });
        lastDrawn_ = list.drawn;
        lastAlive_ = list.alive;

        renderer->setCulling(render::Cull::None);
        renderer->setDepthWrite(false);

        // Nach Textur gruppiert zeichnen. Ein Zeichenaufruf je Shader statt
        // einer je Partikel — bei zweihundert Funken ist das der Unterschied
        // zwischen fluessig und ruckelig.
        // Erst die undurchsichtigen, dann die durchsichtigen.
        //
        // Bei additiver Mischung ist die Reihenfolge gleichgueltig — Addition
        // ist kommutativ. Bei Alphamischung nicht: was zuerst gezeichnet
        // wird, liegt hinten. Eine undurchsichtige Flaeche, die nach einer
        // durchsichtigen kommt, ueberdeckt sie vollstaendig.
        std::vector<std::pair<const std::string*, const scene::Mesh*>> order;
        order.reserve(list.byTexture.size());
        for (const auto& group : list.byTexture) {
            if (!group.second.vertices.empty()) {
                order.emplace_back(&group.first, &group.second);
            }
        }
        std::stable_sort(order.begin(), order.end(),
                         [&](const auto& a, const auto& b) {
                             const bool aOpaque =
                                 blendFor(*a.first, list) ==
                                 shader::BlendMode::Opaque;
                             const bool bOpaque =
                                 blendFor(*b.first, list) ==
                                 shader::BlendMode::Opaque;
                             return aOpaque && !bOpaque;
                         });

        for (const auto& entry : order) {
            const std::string& groupName = *entry.first;
            const scene::Mesh& mesh = *entry.second;
            // Die Wiedergabezeit, nicht die Wanduhr: eine angehaltene
            // Vorschau soll ihre Bildfolge auch anhalten, und beim
            // Zurueckspulen zurueckspulen.
            const render::TextureId texture =
                textureFor(renderer, groupName, elapsed * 0.001f);

            // Die Mischung aus dem Shader. Bis eben stand hier fest "additiv",
            // und ein alphagemischter Rauch sah damit voellig falsch aus.
            //
            // Die beiden Aufzaehlungen haben dieselbe Reihenfolge; der
            // static_assert weiter unten haelt sie zusammen, falls jemand
            // eine Art einfuegt.
            // Die drei Darstellungsarten des Originals.
            //
            // Overdraw faerbt nichts ein, sondern zeichnet jede Flaeche als
            // schwaches, gleichmaessiges Additiv ohne Textur. Wo viele
            // Flaechen uebereinanderliegen, summiert sich das zu Weiss — und
            // genau das ist die Frage, die der Modus beantwortet: wo kostet
            // dieser Effekt Bilder? Der Hilfetext im Original sagt es so:
            // "Shows areas of high polygon overlap".
            const bool overdraw = settings_.effectRenderMode == 2;
            const shader::BlendMode blend =
                overdraw ? shader::BlendMode::Additive : blendFor(groupName, list);
            renderer->setBlend(static_cast<render::Blend>(blend));
            renderer->setFill(settings_.effectRenderMode == 1
                                  ? render::Fill::Wireframe
                                  : render::Fill::Solid);
            // Undurchsichtige Flaechen schreiben in den Tiefenpuffer, die
            // anderen nicht — sonst verdecken sich durchsichtige Partikel
            // gegenseitig, und man sieht Loecher statt Rauch.
            renderer->setDepthWrite(blend == shader::BlendMode::Opaque);

            // Ohne Textur und ohne Farbe im Overdraw-Modus: es soll die
            // Anzahl der Ueberdeckungen zeigen, nicht wie der Effekt aussieht.
            //
            // Die umgefaerbte Fassung liegt in einem Member und nicht in einer
            // lokalen Kopie: der Zeiger muss den Zeichenaufruf ueberleben, und
            // ein Ausdruck wie `kopie(mesh).data()` waere schon vorher
            // ungueltig. Der Member spart nebenbei die Neubelegung je Bild.
            const scene::Vertex* vertices = mesh.vertices.data();
            if (overdraw) {
                overdrawScratch_ = mesh.vertices;
                // Ein schwaches gleichmaessiges Grau. Zwoelf Ueberdeckungen
                // ergeben Weiss — das ist die Schwelle, ab der es im Spiel
                // wehtut.
                constexpr uint32_t kStep = scene::rgba(21, 21, 21, 255);
                for (auto& v : overdrawScratch_) v.colour = kStep;
                vertices = overdrawScratch_.data();
            }

            renderer->drawTriangles(
                reinterpret_cast<const render::Vertex*>(vertices),
                static_cast<int>(mesh.vertices.size()), mesh.indices.data(),
                static_cast<int>(mesh.indices.size()),
                overdraw ? render::kNoTexture : texture);
        }
        renderer->setDepthWrite(false);
        if (!list.lines.vertices.empty()) {
            renderer->setBlend(render::Blend::AlphaBlend);
            renderer->drawLines(
                reinterpret_cast<const render::Vertex*>(list.lines.vertices.data()),
                static_cast<int>(list.lines.vertices.size()), 1.0f);
        }
        renderer->setFill(render::Fill::Solid);
    } else {
        lastDrawn_ = 0;
        lastAlive_ = 0;
    }

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
    // Eine Feldaenderung ist abgeschlossen, sobald kein Feld mehr aktiv ist.
    if (fieldEditOpen_ && !ImGui::IsAnyItemActive()) {
        fieldEditOpen_ = false;
        recordChange(tr(Str::UndoFieldChange));
    }
    if (previewDirty_) {
        previewDirty_ = false;
        refreshPreview();
    }
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

    if (ctrl && pressed(ImGuiKey_Z)) applyUndo();
    if (ctrl && pressed(ImGuiKey_Y)) applyRedo();
    if (ctrl && pressed(ImGuiKey_D)) cmdCloneSegment();
    if (!ctrl && !shift && pressed(ImGuiKey_Space)) pressPlay();
    if (pressed(ImGuiKey_Insert)) showNewSegmentDialog_ = true;
    if (pressed(ImGuiKey_Delete)) cmdDeleteSegment();
    // Shift+C wie im Original; Strg+Umschalt+C in die Zwischenablage.
    if (shift && !ctrl && pressed(ImGuiKey_C)) pendingScreenshot_ = 1;
    if (shift && ctrl && pressed(ImGuiKey_C)) pendingScreenshot_ = 2;
}

}  // namespace efx::gui
