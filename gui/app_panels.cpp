// Die festen Flaechen des Fensters: Menue, Werkzeugleiste, Zeitleiste,
// Segmentliste, Statuszeile.
//
// Alles, was immer da ist — im Unterschied zu den Dialogen, die aufgehen
// und wieder zugehen, und zur Eigenschaftsseite, die vom gewaehlten
// Segment abhaengt.
//
// Teil der Klasse App aus app.h — dieselbe Klasse, nach Aufgaben auf
// mehrere Dateien verteilt. app.cpp war mit 3524 Zeilen und einem Dutzend
// Zustaendigkeiten die Stelle, an der ein Leser aufgibt.
#include "app.h"
#include "efx/i18n.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include "app_shared.h"
#include "testmarke.h"

// SplitterBehavior gehoert zur internen Schnittstelle von ImGui. Sie hier
// zu benutzen ist eine bewusste Ausnahme: einen Teiler selbst zu bauen
// hiesse, das Ziehen, das Einrasten und die Mauszeigerform nachzubauen.
#include "imgui_internal.h"
namespace efx::gui {

using i18n::Str;
using i18n::tr;

void App::drawMenuBar() {
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu(tr(Str::MenuFile))) {
        if (ImGui::MenuItem(tr(Str::FileNew), "Ctrl+N")) cmdNew();
        hint(Str::HintFileNew);
        if (ImGui::MenuItem(tr(Str::FileOpen), "Ctrl+O")) cmdOpen();
        hint(Str::HintFileOpen);
        if (ImGui::MenuItem(tr(Str::FileOpenPk3))) cmdOpenPk3();
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::FileSave), "Ctrl+S")) cmdSave();
        hint(Str::HintFileSave);
        if (ImGui::MenuItem(tr(Str::FileSaveAs))) cmdSaveAs();
        hint(Str::HintFileSaveAs);
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::FileReloadAssets), "F5")) rescanAssets();
        hint(Str::HintReloadAssets);
        ImGui::Separator();
        // Zuletzt geoeffnet, bis 16 wie im Original. Die Liste wurde schon
        // immer gefuehrt, aber nirgends angezeigt.
        if (ImGui::BeginMenu(tr(Str::FileRecent), !settings_.recentFiles.empty())) {
            int number = 0;
            std::string chosen;
            for (const std::string& path : settings_.recentFiles) {
                if (++number > 16) break;
                std::error_code ec;
                const bool exists = std::filesystem::exists(path, ec);
                const std::string label = std::to_string(number % 10) + "  " + path;
                if (ImGui::MenuItem(label.c_str(), nullptr, false, exists)) chosen = path;
            }
            ImGui::EndMenu();
            if (!chosen.empty()) openFile(chosen);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::FileExit), "Alt+F4")) requestQuit();
        hint(Str::HintFileExit);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(tr(Str::MenuEdit))) {
        {
            const std::string label =
                doc().undo.canUndo()
                    ? std::string(tr(Str::EditUndo)) + ": " + doc().undo.undoLabel()
                    : tr(Str::EditUndo);
            ImGui::BeginDisabled(!doc().undo.canUndo());
            if (ImGui::MenuItem(label.c_str(), "Ctrl+Z")) applyUndo();
            ImGui::EndDisabled();

            const std::string redoLabel =
                doc().undo.canRedo()
                    ? std::string(tr(Str::EditRedo)) + ": " + doc().undo.redoLabel()
                    : tr(Str::EditRedo);
            ImGui::BeginDisabled(!doc().undo.canRedo());
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y")) applyRedo();
            ImGui::EndDisabled();
        }
        ImGui::BeginDisabled(!canCloneSegment());
        if (ImGui::MenuItem(tr(Str::EditCloneEffect), "Ctrl+D")) cmdCloneSegment();
        hint(Str::HintClone);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!hasSelection());
        if (ImGui::MenuItem(tr(Str::EditDelete), "Del")) cmdDeleteSegment();
        hint(Str::HintDelete);
        ImGui::EndDisabled();
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::EditWallColor))) showWallColourDialog_ = true;
        hint(Str::HintWallColor);
        if (ImGui::MenuItem(tr(Str::EditBgColor))) showBackgroundColourDialog_ = true;
        hint(Str::HintBgColor);
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::EditGamePath))) {
            std::snprintf(gamePathBuffer_, sizeof(gamePathBuffer_), "%s",
                          settings_.gamePath.c_str());
            showGamePathDialog_ = true;
        }
        hint(Str::HintGamePath);
        ImGui::MenuItem(tr(Str::EditResetRepeatRate), nullptr,
                        &settings_.resetRepeatRateOnStart);
        // Womit geoeffnet wird. Voreinstellung ist der Editor, wie im
        // Original; die Bibliothek bleibt ueber Effekte > Bibliothek und
        // Strg+B einen Griff entfernt.
        ImGui::MenuItem(tr(Str::SettingsOpenLibrary), nullptr,
                        &settings_.openLibraryOnStart);

        // Gitter auch auf Waenden und Decke.
        //
        // Beim Umschalten muss der Raum neu gebaut werden — das Gitter ist
        // ein Netz, kein Schalter im Zeichenpfad.
        if (ImGui::MenuItem(tr(Str::SettingsGridWalls), nullptr,
                            &settings_.gridOnWalls)) {
            // Neuaufbau anstossen. Der Raum wird gebaut, wenn sich der
            // Masstab geaendert hat — diesen Merker zuruecksetzen ist der
            // vorhandene Weg dorthin, ohne einen zweiten einzufuehren.
            geometryDirty_ = true;
        }

        // Wie ein Effekt mit `repeatDelay` wiederholt wird.
        //
        // Aus: wie das Original — ab der ersten Ausloesung wird alle
        //      Repeat-Rate-Sekunden nachgelegt.
        // An:  wie eine Schleife im Spiel, die schon laeuft — die Vorschau
        //      beginnt eingeschwungen (nur bei repeatDelay in der Datei).
        if (ImGui::MenuItem(tr(Str::MenuPreRoll), nullptr, &settings_.preRoll)) {
            // Sofort wirksam: die Art der Wiederholung entscheidet ueber den
            // Vorlauf, und der wird beim Ausloesen aufgebaut.
            if (playing()) startPlayback(); else buildPreviewStopped();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(tr(Str::MenuView))) {
        if (ImGui::BeginMenu(tr(Str::ViewTheme))) {
            for (const auto& t : theme::builtinThemes()) {
                const bool selected = settings_.themeId == t.id;
                if (ImGui::MenuItem(t.name(), nullptr, selected)) {
                    settings_.themeId = t.id;
                    applyTheme(t);
                    // Raum, Gitter und Achsen bekommen ihre Farben aus dem
                    // Thema — sie muessen mitwechseln.
                    geometryDirty_ = true;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr(Str::ViewLanguage))) {
            for (const auto& l : i18n::languages()) {
                const bool selected = i18n::currentLanguage() == l.language;
                if (ImGui::MenuItem(l.nativeName, nullptr, selected)) {
                    i18n::setLanguage(l.language);
                    settings_.languageCode = l.code;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr(Str::ViewRenderer))) {
            for (auto backend : {render::Backend::Direct3D11, render::Backend::OpenGL3}) {
                const bool selected = settings_.rendererCode == render::backendCode(backend);
                if (ImGui::MenuItem(render::backendName(backend), nullptr, selected) &&
                    !selected) {
                    rendererTarget_ = backend;
                    showRendererDialog_ = true;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::MenuItem(tr(Str::ViewMainToolbar), nullptr,
                        &settings_.showMainToolbar);
        ImGui::MenuItem(tr(Str::ViewEffectsToolbar), nullptr,
                        &settings_.showEffectsToolbar);
        ImGui::MenuItem(tr(Str::ViewPlaybackToolbar), nullptr,
                        &settings_.showPlaybackToolbar);
        ImGui::MenuItem(tr(Str::ViewWorldToolbar), nullptr,
                        &settings_.showWorldToolbar);
        ImGui::MenuItem(tr(Str::ViewStatusBar), nullptr, &settings_.showStatusBar);
        ImGui::Separator();
        ImGui::MenuItem(tr(Str::ViewDrawAxes), nullptr, &settings_.drawAxes);
        hint(Str::HintDrawAxes);
        ImGui::MenuItem(tr(Str::ViewWindVector), nullptr, &settings_.drawWindVector);
        hint(Str::HintWind);
        if (ImGui::MenuItem(tr(Str::DialogWind))) showWindDialog_ = true;
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::ViewDrawRoom), nullptr, &settings_.drawRoom)) {
            geometryDirty_ = true;
        }
        hint(Str::HintDrawRoom);
        ImGui::MenuItem(tr(Str::ViewDrawGrid), nullptr, &settings_.drawGrid);
        hint(Str::HintDrawGrid);
        if (ImGui::BeginMenu(tr(Str::ViewRoomStyle))) {
            const Str labels[] = {Str::RoomEnclosed, Str::RoomOpenSky, Str::RoomNone};
            for (int i = 0; i < 3; ++i) {
                if (ImGui::MenuItem(tr(labels[i]), nullptr,
                                    settings_.roomStyle == i)) {
                    settings_.roomStyle = i;
                    geometryDirty_ = true;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr(Str::DialogSun))) showSunDialog_ = true;
        if (ImGui::BeginMenu(tr(Str::ViewTexturedRoom))) {
            // Reihenfolge wie im Original — aus der MENUE-Ressource, nicht
            // aus der Reihenfolge der Dateinamen:
            //
            //     32883 No Texture   32884 Brick   32885 Dirt   32886 Stucco
            //
            // Ich hatte sie zwischenzeitlich umgedreht, weil die Dateinamen
            // im Binary als `stucco, dirt, brick, none` stehen. Das ist die
            // Ladereihenfolge des Programms, nicht die Anzeige — ein
            // Fehlschluss aus der falschen Fundstelle.
            const Str labels[] = {Str::TextureNone, Str::TextureBrick,
                                  Str::TextureDirt, Str::TextureStucco};
            for (int i = 0; i < 4; ++i) {
                if (ImGui::MenuItem(tr(labels[i]), nullptr,
                                    settings_.roomTexture == i)) {
                    settings_.roomTexture = i;
                    // Die Eckpunktfarbe des Raums haengt daran: mit Textur
                    // weiss, ohne die Wandfarbe des Themas. Ohne dieses
                    // Merkzeichen wechselt die Textur, die Faerbung aber
                    // erst beim naechsten Anlass.
                    geometryDirty_ = true;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr(Str::ViewRenderOptions))) {
            // Drei unabhaengige Schalter wie im Original; Overdraw sperrt die
            // beiden anderen.
            ImGui::BeginDisabled(settings_.effectOverdraw);
            ImGui::MenuItem(tr(Str::RenderTextured), nullptr, &settings_.effectTextured);
            ImGui::MenuItem(tr(Str::RenderWireframe), nullptr, &settings_.effectWireframe);
            ImGui::EndDisabled();
            ImGui::MenuItem(tr(Str::RenderOverdraw), nullptr, &settings_.effectOverdraw);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::ViewResetCamera))) {
            camera_.reset(settings_.worldScale);
        }
        hint(Str::HintResetView);
        if (ImGui::MenuItem(tr(Str::ViewScreenshot), "Shift+C")) {
            pendingScreenshot_ = 1;
        }
        hint(Str::HintScreenshot);
        if (ImGui::MenuItem(tr(Str::ViewScreenshotClip), "Ctrl+Shift+C")) {
            pendingScreenshot_ = 2;
        }
        hint(Str::HintScreenshotClip);
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::BrowserOpen), "Ctrl+B", startTabActive_)) {
            // Zum Startreiter und zurueck.
            startTabActive_ = !startTabActive_;
            wantStartTab_ = startTabActive_;   // einmalig auswaehlen
            if (startTabActive_ && browserEntries_.empty()) refreshBrowser();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::ViewGraphicsInfo))) showDriverInfoDialog_ = true;
        hint(Str::HintGraphicsInfo);
        if (ImGui::MenuItem(tr(Str::ViewResetLayout))) {
            settings_.split = layout::Split{};
        }
        ImGui::MenuItem(tr(Str::WindowMessages), nullptr, &showMessagesWindow_);
        ImGui::MenuItem(tr(Str::WindowLog), nullptr, &showLogWindow_);
        ImGui::MenuItem(tr(Str::ViewTileNumbers), nullptr, &browserShowCells_);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(tr(Str::MenuEffects))) {
        if (ImGui::MenuItem(tr(Str::EffectsNewSegment), "Ins")) showNewSegmentDialog_ = true;
        hint(Str::HintNewSegment);
        {
            // "Segment Enabled" aus dem Original. Das Haekchen steht in der
            // Segmentliste, aber auch hier, wie dort.
            const bool selected = hasSelection();
            const bool enabled =
                selected &&
                (static_cast<size_t>(doc().selectedPrimitive) >= doc().segmentEnabled.size() ||
                 doc().segmentEnabled[static_cast<size_t>(doc().selectedPrimitive)]);
            ImGui::BeginDisabled(!selected);
            if (ImGui::MenuItem(tr(Str::EffectsEnabled), nullptr, enabled)) {
                cmdToggleSegmentEnabled();
            }
            hint(Str::HintEnabled);
            if (ImGui::MenuItem(tr(Str::EditDelete), "Del")) cmdDeleteSegment();
            hint(Str::HintDelete);
            ImGui::EndDisabled();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::EffectsPlay), "Space")) pressPlay();
        hint(Str::HintPlay);
        if (ImGui::MenuItem(tr(Str::EffectsPause))) togglePause();
        hint(Str::HintPause);
        if (ImGui::MenuItem(tr(Str::EffectsStop))) pressStop();
        hint(Str::HintStop);
        if (ImGui::MenuItem(tr(Str::EffectsPlaybackSettings))) showPlaybackDialog_ = true;
        hint(Str::HintPlaybackSettings);
        ImGui::Separator();
        // Drei Ausrichtungen, nicht zwei — das Original hat auch "Orient Down".
        if (ImGui::MenuItem(tr(Str::EffectsOrientUp), nullptr,
                            settings_.orientation == 0)) settings_.orientation = 0;
        if (ImGui::MenuItem(tr(Str::EffectsOrientSide), nullptr,
                            settings_.orientation == 1)) settings_.orientation = 1;
        if (ImGui::MenuItem(tr(Str::EffectsOrientDown), nullptr,
                            settings_.orientation == 2)) settings_.orientation = 2;
        ImGui::Separator();
        hint(Str::HintOrientDown);
        if (ImGui::MenuItem(tr(Str::EffectsCustomOrigin))) showSpawnOriginDialog_ = true;
        hint(Str::HintOrientSide);
        hint(Str::HintCustomOrigin);
        hint(Str::HintOrientUp);
        ImGui::MenuItem(tr(Str::EffectsPlaySounds), nullptr, &settings_.playSounds);
        hint(Str::HintPlaySounds);
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::EffectsValidate))) refreshDiagnostics();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(tr(Str::MenuHelp))) {
        // Ravens Originalhandbuch. Es steckt im Programm und wird beim
        // Oeffnen daneben geschrieben — siehe openUsersGuide().
        if (ImGui::MenuItem(tr(Str::HelpUsersGuide))) openUsersGuide();
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::HelpAbout))) showAboutDialog_ = true;
        hint(Str::HintAbout);
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void App::drawToolbar(float dpiScale) {
    // Zwei Reihen wie im Original (Ressourcen RT_TOOLBAR 128, 155, 158, 167):
    //
    //   Reihe 1: Main     New Open Save | Clone | About
    //            Effects  New Segment, Delete Segment
    //            Playback Play Pause Stop | Settings | Repeat Rate [Feld][Regler]
    //                     Orient Up/Sideways/Down | Set Origin
    //   Reihe 2: World    World Scale [Liste] Time Scale [Feld][Regler] |
    //                     Axes | Room Grid | Textured Wireframe Overdraw | Wind
    //
    // Dazu, ans Ende der zweiten Reihe: die festen Blickrichtungen (gibt es im
    // Original nicht). Jede Gruppe laesst sich ueber Ansicht ein- und
    // ausblenden, wie im Original.
    const float iconSize = ImGui::GetFrameHeight();
    const float em = ImGui::GetFontSize();
    (void)dpiScale;
    const auto separator = [] {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
    };
    bool rowStarted = false;
    const auto next = [&] {
        if (rowStarted) separator();
        rowStarted = true;
    };

    // --- Reihe 1 --------------------------------------------------------------
    if (settings_.showMainToolbar) {
        next();
        if (iconButton("##new", Icon::New, tr(Str::FileNew), false, iconSize)) cmdNew();
        ImGui::SameLine();
        if (iconButton("##open", Icon::Open, tr(Str::FileOpen), false, iconSize)) cmdOpen();
        ImGui::SameLine();
        if (iconButton("##save", Icon::Save, tr(Str::FileSave), false, iconSize)) cmdSave();
        separator();
        ImGui::BeginDisabled(!canCloneSegment());
        if (iconButton("##clone", Icon::Clone, tr(Str::ToolClone), false, iconSize)) {
            cmdCloneSegment();
        }
        ImGui::EndDisabled();
        separator();
        if (iconButton("##about", Icon::About, tr(Str::HelpAbout), false, iconSize)) {
            showAboutDialog_ = true;
        }
    }

    if (settings_.showEffectsToolbar) {
        next();
        if (iconButton("##addSegment", Icon::AddSegment, tr(Str::ToolNewSegment), false,
                       iconSize)) {
            showNewSegmentDialog_ = true;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasSelection());
        if (iconButton("##delSegment", Icon::DeleteSegment, tr(Str::ToolDeleteSegment), false,
                       iconSize)) {
            cmdDeleteSegment();
        }
        ImGui::EndDisabled();
    }

    if (settings_.showPlaybackToolbar) {
        next();
        const bool hasSegments = !doc().effect.primitives.empty();
        const bool running = doc().clock.state() != timeline::State::Stopped;
        ImGui::BeginDisabled(!hasSegments);
        // Play und Pause sind im Original Umschalter (eingedrueckt, solange
        // die Wiederholung laeuft bzw. angehalten ist).
        if (iconButton("##play", Icon::Play, tr(Str::ToolPlay),
                       doc().clock.state() == timeline::State::Playing && !playOut_, iconSize)) {
            pressPlay();
        }
        ImGui::SameLine();
        if (iconButton("##pause", Icon::Pause, tr(Str::ToolPause),
                       doc().clock.state() == timeline::State::Paused, iconSize)) {
            togglePause();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!running);
        if (iconButton("##stop", Icon::Stop, tr(Str::ToolStop), false, iconSize)) pressStop();
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        separator();
        if (iconButton("##playbackSettings", Icon::PlaybackSettings,
                       tr(Str::EffectsPlaybackSettings), false, iconSize)) {
            showPlaybackDialog_ = true;
        }
        ImGui::SameLine();
        // Wiederholrate: Feld (%.3f s) und Regler. Der Regler ist im Original
        // logarithmisch, 0.05 bis 5 s (gemessen: Wert = 0.05 * 100^(pos/1000),
        // Vorgabe 0.300). Der Wert ist zugleich das `repeatDelay` der Datei.
        float repeatSeconds = repeatRateSeconds();
        ImGui::SetNextItemWidth(em * 4.0f);
        const bool byField = ImGui::DragFloat("##repeatRate", &repeatSeconds, 0.005f, 0.001f,
                                              60.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::ToolRepeatRate));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(em * 8.0f);
        float slider = std::clamp(repeatSeconds, 0.05f, 5.0f);
        const bool bySlider = ImGui::SliderFloat("##repeatSlider", &slider, 0.05f, 5.0f, "",
                                                 ImGuiSliderFlags_Logarithmic);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::ToolRepeatRate));
        if (bySlider) repeatSeconds = slider;
        if (byField || bySlider) setRepeatRateSeconds(repeatSeconds);
        separator();
        // Ausrichtung als Gruppe von drei Umschaltern.
        const struct { const char* id; Icon icon; Str tip; int value; } orient[] = {
            {"##orientUp", Icon::OrientUp, Str::EffectsOrientUp, 0},
            {"##orientSide", Icon::OrientSide, Str::EffectsOrientSide, 1},
            {"##orientDown", Icon::OrientDown, Str::EffectsOrientDown, 2},
        };
        bool firstOrient = true;
        for (const auto& o : orient) {
            if (!firstOrient) ImGui::SameLine();
            firstOrient = false;
            if (iconButton(o.id, o.icon, tr(o.tip), settings_.orientation == o.value, iconSize)) {
                settings_.orientation = o.value;
                refreshPreview();
            }
        }
        separator();
        if (iconButton("##setOrigin", Icon::SetOrigin, tr(Str::EffectsCustomOrigin), false,
                       iconSize)) {
            showSpawnOriginDialog_ = true;
        }
    }
    if (!rowStarted) ImGui::Dummy(ImVec2(0.0f, iconSize));

    // --- Reihe 2 --------------------------------------------------------------
    rowStarted = false;
    if (settings_.showWorldToolbar) {
        next();
        ImGui::SetNextItemWidth(em * 11.0f);
        int current = 0;
        for (int i = 0; i < layout::worldScaleCount(); ++i) {
            if (layout::worldScales()[i].unitsPerFoot == settings_.worldScale) current = i;
        }
        const bool scaleOpen = ImGui::BeginCombo("##worldScale", layout::worldScales()[current].label());
        testmarke::marke("weltmassstab");
        if (scaleOpen) {
            for (int i = 0; i < layout::worldScaleCount(); ++i) {
                const auto& scale = layout::worldScales()[i];
                if (ImGui::Selectable(scale.label(), i == current)) {
                    settings_.worldScale = scale.unitsPerFoot;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::ToolWorldScale));
        ImGui::SameLine();
        // Zeitfaktor: Feld (%.2f) und Regler, logarithmisch 0.1 bis 10
        // (gemessen: Wert = 10^((pos-500)/500)). Wirkt nur im Editor.
        ImGui::SetNextItemWidth(em * 3.5f);
        if (ImGui::DragFloat("##timeScale", &settings_.timeScale, 0.01f, 0.1f, 10.0f, "%.2f",
                             ImGuiSliderFlags_AlwaysClamp)) {
            doc().clock.setSpeed(settings_.timeScale);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::ToolTimeScale));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(em * 8.0f);
        if (ImGui::SliderFloat("##timeSlider", &settings_.timeScale, 0.1f, 10.0f, "",
                               ImGuiSliderFlags_Logarithmic)) {
            doc().clock.setSpeed(settings_.timeScale);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::ToolTimeScale));

        const auto toggle = [&](const char* id, Icon icon, bool* value, Str tip) {
            if (iconButton(id, icon, tr(tip), *value, iconSize)) {
                *value = !*value;
                geometryDirty_ = true;
            }
        };
        separator();
        toggle("##axes", Icon::Axes, &settings_.drawAxes, Str::ToolDrawAxes);
        separator();
        toggle("##room", Icon::Room, &settings_.drawRoom, Str::ToolDrawRoom);
        ImGui::SameLine();
        toggle("##grid", Icon::Grid, &settings_.drawGrid, Str::ToolDrawGrid);
        separator();
        // Texturiert und Drahtgitter sind im Original UNABHAENGIGE Schalter
        // (beide an = Drahtgitter ueber den texturierten Flaechen); Overdraw
        // sperrt beide, solange es an ist.
        ImGui::BeginDisabled(settings_.effectOverdraw);
        toggle("##textured", Icon::Textured, &settings_.effectTextured, Str::ToolDrawTextured);
        ImGui::SameLine();
        toggle("##wireframe", Icon::Wireframe, &settings_.effectWireframe, Str::ToolDrawWireframe);
        ImGui::EndDisabled();
        ImGui::SameLine();
        toggle("##overdraw", Icon::Overdraw, &settings_.effectOverdraw, Str::ToolDrawOverdraw);
        separator();
        toggle("##wind", Icon::Wind, &settings_.drawWindVector, Str::ToolDrawWind);
    }
    // Die festen Blickrichtungen — nicht im Original, aber schneller als
    // Ziehen, wenn man einen Effekt von der Seite sehen will.
    {
        next();
        struct ViewButton {
            const char* id;
            Icon icon;
            Str tip;
            camera::Orbit::View view;
        };
        static const ViewButton kViews[] = {
            {"##viewFront", Icon::ViewFront, Str::ViewFront, camera::Orbit::View::Front},
            {"##viewBack", Icon::ViewBack, Str::ViewBack, camera::Orbit::View::Back},
            {"##viewLeft", Icon::ViewLeft, Str::ViewLeft, camera::Orbit::View::Left},
            {"##viewRight", Icon::ViewRight, Str::ViewRight, camera::Orbit::View::Right},
            {"##viewTop", Icon::ViewTop, Str::ViewTop, camera::Orbit::View::Top},
            {"##viewBottom", Icon::ViewBottom, Str::ViewBottom, camera::Orbit::View::Bottom},
        };
        bool first = true;
        for (const ViewButton& b : kViews) {
            if (!first) ImGui::SameLine();
            first = false;
            if (iconButton(b.id, b.icon, tr(b.tip), false, iconSize)) camera_.lookFrom(b.view);
        }
        ImGui::SameLine();
        if (iconButton("##viewReset", Icon::ViewReset, tr(Str::ViewReset), false, iconSize)) {
            camera_.reset(settings_.worldScale);
        }
    }
}

void App::drawDocumentTabs() {
    // Mehrere Effekte gleichzeitig, wie im Browser.
    //
    // Reiter statt Fenster: wer an einer Explosion und ihrem Aufschlageffekt
    // zugleich arbeitet, will zwischen beiden umschalten, ohne die Kamera neu
    // zu stellen — die gehoert dem Fenster, nicht der Datei.
    if (!ImGui::BeginTabBar("##documents",
                            ImGuiTabBarFlags_AutoSelectNewTabs |
                            ImGuiTabBarFlags_Reorderable |
                            ImGuiTabBarFlags_TabListPopupButton)) {
        return;
    }

    // Der Startreiter zuerst, und er laesst sich nicht schliessen. So gibt es
    // immer einen Weg zurueck zu den Kacheln — vorher war der Browser ein
    // Extrafenster, das man zumachen konnte und dann nicht wiederfand.
    // Das Kennzeichen nur EINMAL mitgeben und danach loeschen. Gibt man es
    // bei jedem Bild mit, waehlt ImGui den Reiter bei jedem Bild neu aus — man
    // klickt einen anderen an und ist sofort wieder hier.
    const ImGuiTabItemFlags startFlags =
        wantStartTab_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    wantStartTab_ = false;

    // Feste Kennung "###start": sonst ist der Reiter nach einem Sprachwechsel
    // ein NEUER Reiter (andere Beschriftung = andere Kennung), und
    // AutoSelectNewTabs waehlte ihn aus - die Ansicht sprang auf die
    // Startseite (Selbsttest, Sprachwechsel).
    const std::string startLabel = std::string(tr(Str::TabStart)) + "###start";
    if (ImGui::BeginTabItem(startLabel.c_str(), nullptr, startFlags)) {
        startTabActive_ = true;
        ImGui::EndTabItem();
    }

    int closeAt = -1;
    const bool switchPending = wantDocumentTab_;
    for (size_t i = 0; i < documents_.size(); ++i) {
        Document& document = documents_[i];

        // Ein Stern bei ungespeicherten Aenderungen. Die Kennung dahinter
        // bleibt stabil, damit ImGui den Reiter nicht fuer einen neuen haelt,
        // sobald sich der Titel aendert.
        const std::string label =
            document.title() + (document.dirty ? " *" : "") +
            "###doc" + std::to_string(i);

        // Ungespeicherte Aenderungen: der Reiter laesst sich schliessen,
        // aber der Stern im Titel warnt vorher. Einen Nachfragedialog gibt es
        // bewusst nicht — er unterbricht, und Rueckgaengig ueberlebt das
        // Schliessen ohnehin nicht, egal ob man fragt.
        //
        // Wer das aendern will, faengt hier an: `open` auf false pruefen und
        // statt closeDocument einen Dialog aufmachen.
        bool open = true;
        // Der Reiter muss AUSGEWAEHLT werden, nicht nur intern umgeschaltet:
        // sonst haelt die Leiste "Start" fest, und im naechsten Bild setzt
        // BeginTabItem(Start) die Startseite wieder nach vorn. So blieb nach
        // dem Oeffnen einer Datei die Bibliothek sichtbar.
        const ImGuiTabItemFlags documentFlags =
            (wantDocumentTab_ && static_cast<int>(i) == activeDocument_)
                ? ImGuiTabItemFlags_SetSelected
                : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem(label.c_str(), &open, documentFlags)) {
            startTabActive_ = false;
            // Steht ein Wechsel per Code an (Strg+Tab, Oeffnen), zeigt ImGui in
            // diesem Bild noch den ALTEN Reiter. Der darf sich dann nicht
            // wieder aktiv melden — sonst sprang Strg+Tab nach rechts sofort
            // zurueck (nach links ging es nur dank der Schleifenreihenfolge).
            if (static_cast<int>(i) != activeDocument_ && !switchPending) {
                activateDocument(static_cast<int>(i));
            }
            ImGui::EndTabItem();
        }
        if (ImGui::IsItemHovered() && !document.filePath.empty()) {
            ImGui::SetTooltip("%s", document.filePath.c_str());
        }
        if (!open) closeAt = static_cast<int>(i);
    }

    // Erst nach der Schleife schliessen — sonst zieht man ImGui den Reiter
    // unter den Fuessen weg, ueber den es gerade laeuft.
    wantDocumentTab_ = false;
    if (closeAt >= 0) requestCloseDocument(closeAt);

    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing |
                                  ImGuiTabItemFlags_NoTooltip)) {
        newDocument();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::TabNew));

    ImGui::EndTabBar();
}

void App::drawTimeline(float dpiScale) {
    const float iconSize = ImGui::GetFrameHeight();

    // Abspielen, Pause, Stop — dieselben Symbole wie in der Werkzeugleiste.
    // Doppelt vorhanden zu sein ist kein Fehler: die Werkzeugleiste ist weit
    // weg vom Bild, und wer die Zeitleiste bedient, will die Knoepfe dort.
    if (iconButton("##tlPlay", Icon::Play, tr(Str::ToolPlay),
                   doc().clock.state() == timeline::State::Playing && !playOut_, iconSize)) {
        pressPlay();
    }
    ImGui::SameLine();
    if (iconButton("##tlPause", Icon::Pause, tr(Str::ToolPause),
                   doc().clock.state() == timeline::State::Paused, iconSize)) {
        togglePause();   // derselbe Weg wie der Knopf der Werkzeugleiste
    }
    ImGui::SameLine();
    if (iconButton("##tlStop", Icon::Stop, tr(Str::ToolStop), false, iconSize)) {
        pressStop();
    }

    // Einzelschritt. Die Pfeile sind schlicht Text: ein eigenes Symbol dafuer
    // waere kaum zu unterscheiden von Abspielen.
    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();
    if (ImGui::Button("|<", ImVec2(iconSize * 1.3f, iconSize))) {
        doc().clock.scrubTo(0.0f);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::TimelineToStart));
    ImGui::SameLine();
    if (ImGui::Button("<", ImVec2(iconSize, iconSize))) doc().clock.stepFrames(-1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::TimelineStepBack));
    ImGui::SameLine();
    if (ImGui::Button(">", ImVec2(iconSize, iconSize))) doc().clock.stepFrames(1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::TimelineStepForward));

    // Die Anzeige: Zeit und Bildnummer. Beide, weil man beim Feinarbeiten in
    // Bildern denkt und beim Vergleich mit der Datei in Millisekunden.
    ImGui::SameLine();
    ImGui::Text("%6.0f / %.0f ms   %d / %d", static_cast<double>(doc().clock.timeMs()),
                static_cast<double>(doc().clock.durationMs()), doc().clock.currentFrame(),
                doc().clock.totalFrames());

    // Der Schieberegler. Er nimmt den Rest der Breite, weil er das Werkzeug
    // ist, mit dem man tatsaechlich arbeitet.
    ImGui::SameLine();
    const float tail = 320.0f * dpiScale;
    const float available = ImGui::GetContentRegionAvail().x - tail;
    ImGui::SetNextItemWidth(available > 120.0f ? available : 120.0f);
    float progress = doc().clock.progress();
    if (ImGui::SliderFloat("##scrub", &progress, 0.0f, 1.0f, "")) {
        doc().clock.setProgress(progress);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::TimelineScrub));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f * dpiScale);
    float speed = doc().clock.speed();
    if (ImGui::DragFloat("##speed", &speed, 0.01f, 0.01f, 8.0f, "%.2fx")) {
        doc().clock.setSpeed(speed);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::TimelineSpeed));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f * dpiScale);
    float fps = doc().clock.frameRate();
    if (ImGui::DragFloat("##fps", &fps, 1.0f, 1.0f, 240.0f, "%.0f Hz")) {
        doc().clock.setFrameRate(fps);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::TimelineFrameRate));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f * dpiScale);
    const Str endLabels[] = {Str::EndModeStop, Str::EndModeRepeat, Str::EndModeHold};
    int endIndex = static_cast<int>(doc().clock.endMode());
    if (ImGui::BeginCombo("##endMode", tr(endLabels[endIndex]))) {
        for (int i = 0; i < 3; ++i) {
            if (ImGui::Selectable(tr(endLabels[i]), i == endIndex)) {
                const auto chosen = static_cast<timeline::EndMode>(i);
                doc().clock.setEndMode(chosen);
                // Den Wiedergabe-Dialog mitfuehren, damit beide dasselbe
                // zeigen. "Halten" gilt dort als Wiederholen — es ist die
                // Art, die nicht anhaelt.
                playback_.mode = chosen == timeline::EndMode::Stop
                                     ? playback::RepeatMode::Once
                                     : playback::RepeatMode::UntilStopped;

                // Vorschau NEU BAUEN. Ohne das bleibt der alte Bestand stehen.
                //
                // Bei einem Effekt mit `repeatDelay` entscheidet die
                // Betriebsart, ob ein VORLAUF angelegt wird:
                //
                //   Wiederholen — mit Vorlauf: der Bestand steht von Anfang an
                //                 voll, die Zeitleiste laeuft ueber mehrere
                //                 Wiederholungen (bei t_large_fire 2100 ms,
                //                 243 Teilchen).
                //   Anhalten/Halten — ohne: eine einzelne Ausloesung, die
                //                 ausbrennt (1333 ms, 17 Teilchen).
                //
                // Wurde beim Umschalten nicht neu gebaut, zeigte die Vorschau
                // weiter den Bestand der ALTEN Betriebsart, waehrend Dauer und
                // Bildzahl daneben schon die neue nannten. Der Anwender sah
                // eine Zeitleiste, die nicht zum Bild passte, und eine Flamme,
                // die je nach Vorgeschichte sofort ausging oder gar nicht
                // erst erschien.
                if (playing()) startPlayback(); else buildPreviewStopped();
            }
        }

        // Hinweis, wenn "Anhalten" bei einem Effekt gewaehlt ist, den das
        // Spiel gar nicht anhalten wuerde.
        //
        // Ein Effekt mit `repeatDelay` wird in der Engine als LOOPED EFFECT
        // gefuehrt (CFxScheduler::AddLoopedEffects) und laeuft, bis ihn jemand
        // ausdruecklich entfernt. "Anhalten" zeigt also eine einzelne
        // Ausloesung — richtig, um EINEN Durchlauf zu beurteilen, aber nicht
        // das, was im Spiel zu sehen ist.
        //
        // Der Anwender fragte genau das: "die Flamme geht sofort aus, soll das
        // so sein?" Die Antwort ist ja — und sie gehoert dorthin, wo die Frage
        // entsteht, nicht in eine Anleitung.
        if (doc().effect.repeatDelay >= 1 &&
            doc().clock.endMode() == timeline::EndMode::Stop) {
            ImGui::Separator();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 320.0f);
            ImGui::TextDisabled("%s", tr(Str::StopHintLooped));
            ImGui::PopTextWrapPos();
        }

        // Die Art der Wiederholung gleich mit — hier sucht man sie.
        //
        // Sie stand nur im Menue Bearbeiten, zwischen lauter anderem und
        // direkt neben "Wiederholrate beim Start zuruecksetzen". Zwei aehnlich
        // klingende Eintraege an einer Stelle, an der niemand nach der
        // Wiedergabe sucht — der Anwender fand sie nicht, und das war zu
        // erwarten.
        if (doc().clock.endMode() != timeline::EndMode::Stop) {
            ImGui::Separator();
            if (ImGui::MenuItem(tr(Str::MenuPreRoll), nullptr, settings_.preRoll)) {
                settings_.preRoll = !settings_.preRoll;
                if (playing()) startPlayback(); else buildPreviewStopped();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tr(Str::PreRollTip));
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::TimelineEndMode));
}

bool App::moveSegment(int from, int to) {
    auto& list = doc().effect.primitives;
    const int count = static_cast<int>(list.size());
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) {
        return false;
    }

    Primitive moved = std::move(list[static_cast<size_t>(from)]);
    list.erase(list.begin() + from);
    list.insert(list.begin() + to, std::move(moved));

    // Die Haekchen wandern MIT. Sonst schaltet das Verschieben eines
    // Segments ein anderes stumm — ein Fehler, der wie ein Zufall aussieht.
    if (doc().segmentEnabled.size() == list.size()) {
        const bool wasOn = doc().segmentEnabled[static_cast<size_t>(from)];
        doc().segmentEnabled.erase(doc().segmentEnabled.begin() + from);
        doc().segmentEnabled.insert(doc().segmentEnabled.begin() + to, wasOn);
    } else {
        doc().segmentEnabled.assign(list.size(), true);
    }

    doc().selectedPrimitive = to;
    doc().dirty = true;
    doc().undo.record(doc().effect, "Segment verschoben");
    refreshDiagnostics();
    refreshPreview();
    return true;
}

bool App::insertSegmentAt(int at) {
    auto& list = doc().effect.primitives;
    if (at < 0) at = 0;
    if (at > static_cast<int>(list.size())) at = static_cast<int>(list.size());

    // Denselben Typ wie das Segment, neben dem eingefuegt wurde: wer
    // "darunter einfuegen" waehlt, will meist noch eines von derselben Sorte.
    // Ein leeres Partikel waere die haeufigste Fehlbedienung.
    PrimitiveType type = PrimitiveType::Particle;
    const int neighbour = at > 0 ? at - 1 : 0;
    if (!list.empty() && neighbour < static_cast<int>(list.size())) {
        type = list[static_cast<size_t>(neighbour)].type;
    }

    list.insert(list.begin() + at, freshPrimitive(type));

    if (doc().segmentEnabled.size() + 1 == list.size()) {
        doc().segmentEnabled.insert(doc().segmentEnabled.begin() + at, true);
    } else {
        doc().segmentEnabled.assign(list.size(), true);
    }

    doc().selectedPrimitive = at;
    doc().dirty = true;
    doc().undo.record(doc().effect, "Segment eingefuegt");
    refreshDiagnostics();
    refreshPreview();
    return true;
}

void App::startRename(int index) {
    if (index < 0 || index >= static_cast<int>(doc().effect.primitives.size())) return;
    renamingSegment_ = index;
    renameFocus_ = true;
    std::snprintf(renameBuffer_, sizeof(renameBuffer_), "%s",
                  doc().effect.primitives[static_cast<size_t>(index)].name.c_str());
}

void App::sortSegments(int column, bool ascending) {
    auto& list = doc().effect.primitives;
    if (list.size() < 2) return;
    doc().segmentEnabled.resize(list.size(), true);
    // Die Reihenfolge als Indexliste sortieren, dann alles danach umstellen -
    // so wandern Haekchen und Auswahl mit ihrem Segment.
    std::vector<size_t> order(list.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    const auto lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const auto less = [&](size_t a, size_t b) {
        const Primitive& pa = list[a];
        const Primitive& pb = list[b];
        switch (column) {
            case 0: return lower(pa.name) < lower(pb.name);
            case 1: return std::string(typeName(pa.type)) < std::string(typeName(pb.type));
            case 3: return (pa.delay.set ? pa.delay.min : 0.0f) < (pb.delay.set ? pb.delay.min : 0.0f);
            case 4: return (pa.count.set ? pa.count.min : 0.0f) < (pb.count.set ? pb.count.min : 0.0f);
            default: return a < b;  // "Segment": die Nummer selbst
        }
    };
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return ascending ? less(a, b) : less(b, a);
    });
    bool changed = false;
    for (size_t i = 0; i < order.size(); ++i) changed |= order[i] != i;
    if (!changed) return;
    std::vector<Primitive> sorted;
    std::vector<bool> enabled;
    int selected = -1;
    for (size_t i = 0; i < order.size(); ++i) {
        sorted.push_back(std::move(list[order[i]]));
        enabled.push_back(doc().segmentEnabled[order[i]]);
        if (static_cast<int>(order[i]) == doc().selectedPrimitive) selected = static_cast<int>(i);
    }
    list = std::move(sorted);
    doc().segmentEnabled = std::move(enabled);
    doc().selectedPrimitive = selected;
    recordChange(tr(Str::UndoSortSegments));
    refreshPreview();
}

void App::drawSegmentList(float width, float height) {
    ImGui::BeginChild("segments", ImVec2(width, height), ImGuiChildFlags_Borders);

    const ImGuiTableFlags flags = ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable |
                                  // Drei Zustaende: auf, ab, unsortiert. Ohne
                                  // das sortiert ImGui beim Start die erste
                                  // Spalte - und zeigte einen Pfeil, obwohl
                                  // nichts sortiert war.
                                  ImGuiTableFlags_SortTristate |
                                  ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("segmentTable", 5, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        // Breiten aus der Schriftgroesse: mit festen 70 Punkten stand bei
        // 150 Prozent "Seg..." statt "Segment" im Kopf.
        const float em = ImGui::GetFontSize();
        ImGui::TableSetupColumn(tr(Str::ListName), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(tr(Str::ListType), ImGuiTableColumnFlags_WidthFixed, em * 9.0f);
        ImGui::TableSetupColumn(tr(Str::ListSegment), ImGuiTableColumnFlags_WidthFixed, em * 5.5f);
        ImGui::TableSetupColumn(tr(Str::ListDelay), ImGuiTableColumnFlags_WidthFixed, em * 5.0f);
        ImGui::TableSetupColumn(tr(Str::ListCount), ImGuiTableColumnFlags_WidthFixed, em * 5.0f);
        // Die Kopfzeile von Hand statt TableHeadersRow: so bekommt jeder Kopf
        // eine Testmarke (der Selbsttest klickt ihn zum Sortieren an).
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (int column = 0; column < 5; ++column) {
            if (!ImGui::TableSetColumnIndex(column)) continue;
            ImGui::TableHeader(ImGui::TableGetColumnName(column));
            const std::string mark = "liste/kopf" + std::to_string(column);
            testmarke::marke(mark.c_str());
        }

        // Klick auf einen Spaltenkopf sortiert die SEGMENTE um, wie im
        // Original ("The order of the segments can be changed by clicking on
        // the header of a column"). Vorher zeigte der Kopf einen Pfeil, und
        // nichts geschah.
        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
            if (specs->SpecsDirty) {
                if (specs->SpecsCount > 0) {
                    sortSegments(specs->Specs[0].ColumnIndex,
                                 specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
                }
                specs->SpecsDirty = false;
            }
        }

        // Rechte Maustaste auf die Liste: Zeilenhoehe.
        //
        // Die Spalten haben so ein Menue von ImGui aus ("Size column to
        // fit"), fuer die Zeilen gibt es keines — das muss man selbst
        // aufmachen. Deshalb hier, mit denselben zwei Moeglichkeiten:
        // anpassen oder festlegen.
        if (ImGui::BeginPopupContextWindow("##rowHeight",
                                           ImGuiPopupFlags_MouseButtonRight |
                                               ImGuiPopupFlags_NoOpenOverItems)) {
            ImGui::TextDisabled("%s", tr(Str::RowHeightMenu));
            ImGui::Separator();
            if (ImGui::MenuItem(tr(Str::RowHeightAuto), nullptr,
                                settings_.segmentRowHeight <= 0.0f)) {
                settings_.segmentRowHeight = 0.0f;
            }
            ImGui::TextUnformatted(tr(Str::RowHeightFixed));
            ImGui::SetNextItemWidth(160.0f);
            // Nicht `height`: so heisst schon der Parameter dieser Funktion,
            // und MSVC meldet das zu Recht (C4457).
            float rowHeight = settings_.segmentRowHeight > 0.0f
                                  ? settings_.segmentRowHeight
                                  : ImGui::GetTextLineHeightWithSpacing();
            // Untergrenze eine Textzeile: darunter waere die Zeile nicht mehr
            // lesbar, und ein Wert, bei dem man nichts mehr sieht, ist keine
            // Einstellung, sondern eine Falle.
            const float smallest = ImGui::GetTextLineHeight();
            if (ImGui::DragFloat("##rowHeightValue", &rowHeight, 0.5f, smallest,
                                 smallest * 6.0f, "%.0f px")) {
                settings_.segmentRowHeight = rowHeight;
            }
            ImGui::EndPopup();
        }

        for (int i = 0; i < static_cast<int>(doc().effect.primitives.size()); ++i) {
            const Primitive& p = doc().effect.primitives[i];
            // Feste Zeilenhoehe, wenn eingestellt — sonst wie bisher an den
            // Inhalt angepasst. Der Rollbalken bleibt in beiden Faellen: die
            // Hoehe aendert nur, wie viel gleichzeitig hineinpasst.
            ImGui::TableNextRow(0, settings_.segmentRowHeight);
            ImGui::PushID(i);

            ImGui::TableSetColumnIndex(0);

            // Das Haekchen je Segment. Es gab die Maske schon, sie wurde an
            // die Simulation gereicht — es fehlte nur die Stelle, an der man
            // sie umschalten kann. Im Original steht sie genau hier, links
            // vom Namen.
            //
            // Das Haekchen steht NICHT in der Datei; es ist eine reine
            // Vorschau-Einstellung, um ein Segment kurz stummzuschalten.
            if (doc().segmentEnabled.size() != doc().effect.primitives.size()) {
                doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
            }
            bool segmentOn = doc().segmentEnabled[static_cast<size_t>(i)];
            ImGui::PushID(i);
            if (ImGui::Checkbox("##enabled", &segmentOn)) {
                doc().segmentEnabled[static_cast<size_t>(i)] = segmentOn;
                // Sofort wirksam, AUCH bei stehender Uhr: sonst muesste man
                // neu starten, um zu sehen, was ein Segment beitraegt — und
                // genau dafuer ist das Haekchen da.
                refreshPreview();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tr(Str::EffectsEnabled));
            }
            ImGui::PopID();
            ImGui::SameLine();

            // Ohne Namen wie im Original "Unnamed <Typ> <Nummer>".
            const std::string label = displayName(i);
            // Umbenennen in der Zeile, wie im Original (dort: ein zweiter
            // Klick auf die gewaehlte Zeile). Hier: Doppelklick, F2 oder
            // Kontextmenue. Enter uebernimmt, Esc verwirft.
            if (renamingSegment_ == i) {
                if (renameFocus_) {
                    ImGui::SetKeyboardFocusHere();
                    renameFocus_ = false;
                }
                ImGui::SetNextItemWidth(-1.0f);
                const bool done = ImGui::InputText("##rename", renameBuffer_, sizeof(renameBuffer_),
                                                   ImGuiInputTextFlags_EnterReturnsTrue |
                                                       ImGuiInputTextFlags_AutoSelectAll);
                testmarke::marke("liste/umbenennen");
                if (done || ImGui::IsItemDeactivatedAfterEdit()) {
                    // Der Parser nimmt hoechstens 31 Zeichen (CPrimitiveTemplate::mName[32]).
                    std::string name = renameBuffer_;
                    if (name.size() > 31) name.resize(31);
                    if (name != doc().effect.primitives[static_cast<size_t>(i)].name) {
                        doc().effect.primitives[static_cast<size_t>(i)].name = name;
                        recordChange(tr(Str::ListRename));
                    }
                    renamingSegment_ = -1;
                } else if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                           (!ImGui::IsItemActive() && !ImGui::IsItemFocused() &&
                            ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
                    renamingSegment_ = -1;
                }
            } else {
                if (ImGui::Selectable(label.c_str(), doc().selectedPrimitive == i,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                          ImGuiSelectableFlags_AllowDoubleClick)) {
                    doc().selectedPrimitive = i;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) startRename(i);
                }
            }

            // Zeile ziehen, um die Reihenfolge zu aendern.
            //
            // Das Original kann das nicht — dort muss man ein Segment am Ende
            // anlegen und mit der Anordnung leben. Bei einem Effekt mit zehn
            // Segmenten heisst das im Zweifel: von vorn anfangen.
            //
            // Die Reihenfolge ist nicht nur Kosmetik: sie steht so in der
            // Datei, und die Engine arbeitet die Segmente in dieser Folge ab.
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)) {
                ImGui::SetDragDropPayload("efxSegment", &i, sizeof(int));
                ImGui::TextUnformatted(label.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("efxSegment")) {
                    const int from = *static_cast<const int*>(payload->Data);
                    moveSegment(from, i);
                }
                ImGui::EndDragDropTarget();
            }

            // Rechtsklick auf die Zeile: einfuegen.
            //
            // Auch das fehlt im Original — dort landet ein neues Segment
            // immer am Ende. Wer es woanders braucht, muesste umsortieren,
            // und das ging ja gerade nicht.
            if (ImGui::BeginPopupContextItem("##segmentMenu")) {
                doc().selectedPrimitive = i;
                if (ImGui::MenuItem(tr(Str::ListRename), "F2")) startRename(i);
                ImGui::Separator();
                if (ImGui::MenuItem(tr(Str::SegmentInsertAbove))) {
                    insertSegmentAt(i);
                }
                if (ImGui::MenuItem(tr(Str::SegmentInsertBelow))) {
                    insertSegmentAt(i + 1);
                }
                ImGui::Separator();
                ImGui::TextDisabled("%s", tr(Str::SegmentMoveHint));
                ImGui::EndPopup();
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(typeName(p.type));

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", i + 1);

            ImGui::TableSetColumnIndex(3);
            // Wie im Original: "%4.2f", bei einer Spanne "min - max".
            const auto rangeText = [](const Range& r, float fallback) {
                char text[64];
                if (!r.set) {
                    std::snprintf(text, sizeof(text), "%.2f", static_cast<double>(fallback));
                } else if (r.min != r.max) {
                    std::snprintf(text, sizeof(text), "%.2f - %.2f", static_cast<double>(r.min),
                                  static_cast<double>(r.max));
                } else {
                    std::snprintf(text, sizeof(text), "%.2f", static_cast<double>(r.min));
                }
                return std::string(text);
            };
            if (p.delay.set) {
                ImGui::TextUnformatted(rangeText(p.delay, 0.0f).c_str());
            } else {
                ImGui::TextDisabled("%s", rangeText(p.delay, 0.0f).c_str());
            }

            ImGui::TableSetColumnIndex(4);
            if (p.count.set) {
                ImGui::TextUnformatted(rangeText(p.count, 1.0f).c_str());
            } else {
                ImGui::TextDisabled("%s", rangeText(p.count, 1.0f).c_str());
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (doc().effect.primitives.empty()) {
        ImGui::TextDisabled("%s", tr(Str::ListEmpty));
    }

    ImGui::EndChild();
}

void App::drawStatusBar() {
    const theme::Palette& palette = activeTheme(settings_).palette;

    int errors = 0, warnings = 0;
    for (const auto& d : doc().diagnostics) {
        if (d.severity == Severity::Error) ++errors;
        if (d.severity == Severity::Warning) ++warnings;
    }

    // Die Zaehler sind ANKLICKBAR.
    //
    // Vorher stand da "1 Error" und sonst nichts — keine Stelle im ganzen
    // Programm zeigte, WAS beanstandet wurde. Vierzig Pruefregeln, deren
    // Ergebnis man nur zaehlen, aber nicht lesen konnte.
    const auto clickableCount = [&](Severity severity, int count, const char* label) {
        ImGui::PushStyleColor(ImGuiCol_Text, severityColor(severity, palette));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        char text[64];
        std::snprintf(text, sizeof(text), "%d %s", count, label);
        if (ImGui::SmallButton(text)) showMessagesWindow_ = true;
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tr(Str::MessagesClickHint));
        }
        ImGui::SameLine();
    };

    if (errors > 0) clickableCount(Severity::Error, errors, tr(Str::DiagError));
    if (warnings > 0) {
        clickableCount(Severity::Warning, warnings, tr(Str::DiagWarning));
    }
    if (statusHint_ != nullptr || iconButtonHoveredTip() != nullptr) {
        ImGui::TextUnformatted(statusHint_ ? statusHint_ : iconButtonHoveredTip());
        ImGui::SameLine();
    } else if (errors == 0 && warnings == 0) {
        ImGui::TextUnformatted(tr(Str::StatusReady));
        ImGui::SameLine();
    }

    // Auf der Startseite zaehlen andere Zahlen.
    //
    // Sie standen bisher nur im Protokollfenster — und wer eine schlechte
    // Bildrate meldet, hat es meist nicht offen. Ohne diese Zahlen ist "es
    // ruckelt" nicht zu beheben, sondern nur zu beklagen: sie sagen, ob die
    // Zeit ueberhaupt im Browser sitzt.
    if (startTabActive_ && !browserSlots_.empty()) {
        ImGui::TextDisabled(tr(Str::LogCounters), browserRedrawn_,
                            static_cast<int>(browserSlots_.size()),
                            browserDrawCalls_, browserBuildMs_, browserDrawMs_);
        if (!browserHoveredName_.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled(tr(Str::StatusHoveredTile),
                                browserHoveredName_.c_str(), browserHoveredLive_,
                                browserHoveredDrawn_, browserHoveredVertices_);
        }
        ImGui::SameLine();
    }

    // Rechtsbuendig, so breit wie der Text tatsaechlich ist (DPI, Sprache).
    char counters[256];
    std::snprintf(counters, sizeof(counters), "%s: %d   %s: %d   %s: %d   %s: %d   %.1f FPS",
                  tr(Str::StatusActive), lastAlive_, tr(Str::StatusDrawn), lastDrawn_,
                  tr(Str::StatusScheduled), lastScheduled_, tr(Str::StatusMarks), lastMarks_,
                  static_cast<double>(ImGui::GetIO().Framerate));
    ImGui::SameLine(std::max(0.0f, ImGui::GetContentRegionAvail().x -
                                       ImGui::CalcTextSize(counters).x - ImGui::GetStyle().ItemSpacing.x));
    // Standen bis eben fest auf null — jetzt die tatsaechlichen Zahlen aus
    // der laufenden Vorschau. "Aktiv" ist, was lebt; "Gezeichnet" ist, was
    // davon ein Bild hat (ein Sound lebt, zeichnet aber nichts).
    if (!screenshotMessage_.empty()) {
        if (static_cast<float>(ImGui::GetTime()) > screenshotMessageUntil_) {
            screenshotMessage_.clear();
        } else {
            ImGui::TextUnformatted(screenshotMessage_.c_str());
            ImGui::SameLine();
        }
    }
    ImGui::TextUnformatted(counters);
}


bool App::splitter(const char* id, bool vertical, float thickness,
                   float* fraction, float totalPx, float minFirstPx,
                   float minSecondPx) {
    if (totalPx <= minFirstPx + minSecondPx + thickness) return false;

    float firstPx = *fraction * totalPx;
    float secondPx = totalPx - firstPx - thickness;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImGuiID identifier = window->GetID(id);

    ImRect bounds;
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    if (vertical) {
        bounds.Min = ImVec2(cursor.x, cursor.y);
        bounds.Max = ImVec2(cursor.x + thickness, cursor.y + ImGui::GetContentRegionAvail().y);
    } else {
        bounds.Min = ImVec2(cursor.x, cursor.y);
        bounds.Max = ImVec2(cursor.x + ImGui::GetContentRegionAvail().x, cursor.y + thickness);
    }

    const bool moved = ImGui::SplitterBehavior(
        bounds, identifier, vertical ? ImGuiAxis_X : ImGuiAxis_Y, &firstPx,
        &secondPx, minFirstPx, minSecondPx, 2.0f);

    if (moved) *fraction = firstPx / totalPx;
    return moved;
}

}  // namespace efx::gui
