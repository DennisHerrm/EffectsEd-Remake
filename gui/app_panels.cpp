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

#include <cstdio>
#include "app_shared.h"

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
        if (ImGui::MenuItem(tr(Str::FileNew), "Ctrl+N")) newEffect();
        if (ImGui::MenuItem(tr(Str::FileOpen), "Ctrl+O") && fileDialog_) {
            const std::string path =
                fileDialog_(false, "Effect files (*.efx)\0*.efx\0All files\0*.*\0",
                            nullptr);
            if (!path.empty()) openFile(path);
        }
        // Ein .pk3 als Quelle oeffnen, ohne den Spielpfad umzustellen. Wer
        // eine Mod-Datei bekommen hat, will hineinsehen — nicht seine
        // Einstellungen aendern.
        if (ImGui::MenuItem(tr(Str::FileOpenPk3)) && fileDialog_) {
            const std::string picked = fileDialog_(
                false, "PK3-Archive (*.pk3)\0*.pk3\0Alle Dateien\0*.*\0",
                nullptr);
            if (!picked.empty() && openArchive(picked)) {
                startTabActive_ = true;
                wantStartTab_ = true;
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::FileSave), "Ctrl+S")) {
            // Ohne bekannten Pfad wird daraus "Speichern unter" — sonst
            // verschwindet die Datei wortlos irgendwohin.
            if (doc().filePath.empty() && fileDialog_) {
                const std::string path = fileDialog_(
                    true, "Effect files (*.efx)\0*.efx\0All files\0*.*\0",
                    "untitled.efx");
                if (!path.empty()) saveFile(path);
            } else if (!doc().filePath.empty()) {
                saveFile(doc().filePath);
            }
        }
        if (ImGui::MenuItem(tr(Str::FileSaveAs)) && fileDialog_) {
            const std::string suggestion =
                doc().filePath.empty() ? "untitled.efx" : doc().filePath;
            const std::string path = fileDialog_(
                true, "Effect files (*.efx)\0*.efx\0All files\0*.*\0",
                suggestion.c_str());
            if (!path.empty()) saveFile(path);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::FileReloadAssets), "F5")) rescanAssets();
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::FileExit), "Alt+F4")) wantsQuit_ = true;
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
        {
            // Klonen: das gewaehlte Segment verdoppeln und die Kopie gleich
            // auswaehlen. Der Name bekommt eine Ziffer, sonst haette man zwei
            // gleichnamige Segmente — und die Pruefung meldet das zu Recht.
            const bool canClone =
                doc().selectedPrimitive >= 0 &&
                doc().selectedPrimitive < static_cast<int>(doc().effect.primitives.size()) &&
                doc().effect.primitives.size() < 24;
            ImGui::BeginDisabled(!canClone);
            if (ImGui::MenuItem(tr(Str::EditCloneEffect), "Ctrl+D") && canClone) {
                Primitive copy = doc().effect.primitives[static_cast<size_t>(
                    doc().selectedPrimitive)];
                if (!copy.name.empty()) {
                    // Einen freien Namen suchen statt blind eine 2 anzuhaengen.
                    for (int n = 2; n < 100; ++n) {
                        const std::string candidate =
                            copy.name + " " + std::to_string(n);
                        bool taken = false;
                        for (const auto& p : doc().effect.primitives) {
                            if (p.name == candidate) taken = true;
                        }
                        if (!taken) {
                            // Der Parser begrenzt Namen auf 31 Zeichen.
                            copy.name = candidate.size() > 31
                                            ? candidate.substr(0, 31)
                                            : candidate;
                            break;
                        }
                    }
                }
                doc().effect.primitives.insert(
                    doc().effect.primitives.begin() + doc().selectedPrimitive + 1,
                    std::move(copy));
                ++doc().selectedPrimitive;
                doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
                recordChange(tr(Str::UndoCloneSegment));
            }
            ImGui::EndDisabled();
        }
        {
            const bool canDelete =
                doc().selectedPrimitive >= 0 &&
                doc().selectedPrimitive < static_cast<int>(doc().effect.primitives.size());
            ImGui::BeginDisabled(!canDelete);
            if (ImGui::MenuItem(tr(Str::EditDelete), "Del") && canDelete) {
                doc().effect.primitives.erase(doc().effect.primitives.begin() +
                                         doc().selectedPrimitive);
                if (doc().selectedPrimitive >=
                    static_cast<int>(doc().effect.primitives.size())) {
                    doc().selectedPrimitive =
                        static_cast<int>(doc().effect.primitives.size()) - 1;
                }
                doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
                recordChange(tr(Str::UndoDeleteSegment));
            }
            ImGui::EndDisabled();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::EditWallColor))) showWallColourDialog_ = true;
        if (ImGui::MenuItem(tr(Str::EditBgColor))) showBackgroundColourDialog_ = true;
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::EditGamePath))) {
            std::snprintf(gamePathBuffer_, sizeof(gamePathBuffer_), "%s",
                          settings_.gamePath.c_str());
            showGamePathDialog_ = true;
        }
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
            lastWorldScale_ = 0.0f;
        }

        // Wie ein Effekt mit `repeatDelay` wiederholt wird.
        //
        // Aus: wie die Engine — durchgehend, es wird nachgelegt.
        // An:  wie der alte Editor — auslaufen lassen, Pause, von vorn.
        //
        // Gemessen: das Original faellt zwischendurch ueber rund zehn Bilder
        // auf nahezu null zurueck. Wer die beiden Programme nebeneinander
        // vergleicht, braucht diese Fassung.
        if (ImGui::MenuItem(tr(Str::MenuLegacyRepeat), nullptr,
                            &settings_.legacyRepeat)) {
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
        ImGui::MenuItem(tr(Str::ViewWindVector), nullptr, &settings_.drawWindVector);
        if (ImGui::MenuItem(tr(Str::DialogWind))) showWindDialog_ = true;
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::ViewDrawRoom), nullptr, &settings_.drawRoom)) {
            geometryDirty_ = true;
        }
        ImGui::MenuItem(tr(Str::ViewDrawGrid), nullptr, &settings_.drawGrid);
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
            const Str labels[] = {Str::RenderTextured, Str::RenderWireframe,
                                  Str::RenderOverdraw};
            for (int i = 0; i < 3; ++i) {
                if (ImGui::MenuItem(tr(labels[i]), nullptr,
                                    settings_.effectRenderMode == i)) {
                    settings_.effectRenderMode = i;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::ViewResetCamera))) {
            camera_.reset(settings_.worldScale);
        }
        if (ImGui::MenuItem(tr(Str::ViewScreenshot), "Shift+C")) {
            pendingScreenshot_ = 1;
        }
        if (ImGui::MenuItem(tr(Str::ViewScreenshotClip), "Ctrl+Shift+C")) {
            pendingScreenshot_ = 2;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::BrowserOpen), "Ctrl+B", startTabActive_)) {
            // Zum Startreiter und zurueck.
            startTabActive_ = !startTabActive_;
            wantStartTab_ = startTabActive_;   // einmalig auswaehlen
            if (startTabActive_ && browserEntries_.empty()) refreshBrowser();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::ViewGraphicsInfo))) showDriverInfoDialog_ = true;
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
        {
            // "Segment Enabled" aus dem Original. Das Haekchen steht in der
            // Segmentliste, aber der Menuepunkt fehlte — und wer im Menue
            // sucht, findet dort sonst nichts.
            const bool hasSelection =
                doc().selectedPrimitive >= 0 &&
                doc().selectedPrimitive < static_cast<int>(doc().segmentEnabled.size());
            const bool enabled =
                hasSelection && doc().segmentEnabled[static_cast<size_t>(
                                    doc().selectedPrimitive)];
            ImGui::BeginDisabled(!hasSelection);
            if (ImGui::MenuItem(tr(Str::EffectsEnabled), nullptr, enabled) &&
                hasSelection) {
                doc().segmentEnabled[static_cast<size_t>(doc().selectedPrimitive)] = !enabled;
                if (playing()) startPlayback();
            }
            ImGui::EndDisabled();
        }
        {
            const bool canDelete =
                doc().selectedPrimitive >= 0 &&
                doc().selectedPrimitive < static_cast<int>(doc().effect.primitives.size());
            ImGui::BeginDisabled(!canDelete);
            if (ImGui::MenuItem(tr(Str::EditDelete), "Del") && canDelete) {
                doc().effect.primitives.erase(doc().effect.primitives.begin() +
                                         doc().selectedPrimitive);
                if (doc().selectedPrimitive >=
                    static_cast<int>(doc().effect.primitives.size())) {
                    doc().selectedPrimitive =
                        static_cast<int>(doc().effect.primitives.size()) - 1;
                }
                doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
                recordChange(tr(Str::UndoDeleteSegment));
            }
            ImGui::EndDisabled();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::EffectsPlay), "Space")) pressPlay();
        if (ImGui::MenuItem(tr(Str::EffectsPause))) togglePause();
        if (ImGui::MenuItem(tr(Str::EffectsStop))) pressStop();
        if (ImGui::MenuItem(tr(Str::EffectsPlaybackSettings))) showPlaybackDialog_ = true;
        ImGui::Separator();
        // Drei Ausrichtungen, nicht zwei — das Original hat auch "Orient Down".
        if (ImGui::MenuItem(tr(Str::EffectsOrientUp), nullptr,
                            settings_.orientation == 0)) settings_.orientation = 0;
        if (ImGui::MenuItem(tr(Str::EffectsOrientSide), nullptr,
                            settings_.orientation == 1)) settings_.orientation = 1;
        if (ImGui::MenuItem(tr(Str::EffectsOrientDown), nullptr,
                            settings_.orientation == 2)) settings_.orientation = 2;
        ImGui::Separator();
        if (ImGui::MenuItem(tr(Str::EffectsCustomOrigin))) showSpawnOriginDialog_ = true;
        ImGui::MenuItem(tr(Str::EffectsPlaySounds), nullptr, &settings_.playSounds);
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
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void App::drawToolbar(float dpiScale) {
    const float wide = 120.0f * dpiScale;
    const float narrow = 70.0f * dpiScale;
    // Quadratische Symbolknoepfe, gross genug zum Treffen. Die Zeilenhoehe
    // als Mass, damit sie zu den Eingabefeldern daneben passen.
    const float iconSize = ImGui::GetFrameHeight();

    // Zeile 1: Datei | Segmente | Wiedergabe
    //
    // Die Gruppen entsprechen den Leisten 128, 155 und 158 des Originals und
    // lassen sich einzeln abschalten. Dort waren es andockbare Fenster; eine
    // feste Leiste mit schaltbaren Gruppen tut dasselbe, ohne dass man
    // Fenster herumziehen muss.
    bool anythingBefore = false;
    if (settings_.showMainToolbar) {
        if (iconButton("##new", Icon::New, tr(Str::FileNew), false, iconSize)) {
            newEffect();
        }
        ImGui::SameLine();
        if (iconButton("##open", Icon::Open, tr(Str::FileOpen), false, iconSize) &&
            fileDialog_) {
            const std::string path = fileDialog_(
                false, "Effect files (*.efx)\0*.efx\0All files\0*.*\0", nullptr);
            if (!path.empty()) openFile(path);
        }
        ImGui::SameLine();
        if (iconButton("##save", Icon::Save, tr(Str::FileSave), false, iconSize)) {
            if (!doc().filePath.empty()) {
                saveFile(doc().filePath);
            } else if (fileDialog_) {
                const std::string path = fileDialog_(
                    true, "Effect files (*.efx)\0*.efx\0All files\0*.*\0",
                    "untitled.efx");
                if (!path.empty()) saveFile(path);
            }
        }
        anythingBefore = true;
    }

    if (settings_.showEffectsToolbar) {
        if (anythingBefore) { ImGui::SameLine(); ImGui::TextUnformatted("|"); }
        ImGui::SameLine();
        if (iconButton("##addSegment", Icon::AddSegment, tr(Str::ToolNewSegment),
                       false, iconSize)) {
            showNewSegmentDialog_ = true;
        }
        ImGui::SameLine();
        const bool canDelete =
            doc().selectedPrimitive >= 0 &&
            doc().selectedPrimitive < static_cast<int>(doc().effect.primitives.size());
        ImGui::BeginDisabled(!canDelete);
        if (iconButton("##delSegment", Icon::DeleteSegment,
                       tr(Str::ToolDeleteSegment), false, iconSize) && canDelete) {
            doc().effect.primitives.erase(doc().effect.primitives.begin() + doc().selectedPrimitive);
            if (doc().selectedPrimitive >= static_cast<int>(doc().effect.primitives.size())) {
                doc().selectedPrimitive = static_cast<int>(doc().effect.primitives.size()) - 1;
            }
            doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
            recordChange(tr(Str::UndoDeleteSegment));
        }
        ImGui::EndDisabled();
        anythingBefore = true;
    }

    if (!settings_.showPlaybackToolbar) {
        // Die zweite Zeile faengt trotzdem an.
        if (!anythingBefore) ImGui::TextUnformatted(" ");
    } else {
    // Abspielen, Pause und Stopp stehen NUR noch unten in der Zeitleiste.
    //
    // Hier standen sie ein zweites Mal, samt Wiederholungshaeckchen — dieselben
    // drei Knoepfe, keine zwei Handbreit von den anderen entfernt. Bei zwei
    // Bedienelementen fuer dieselbe Sache fragt man sich, ob sie dasselbe tun,
    // und probiert es aus. Das Original hat sie oben, aber es hat unten keine
    // Zeitleiste; wir haben eine, und dort gehoeren sie hin.
    //
    // Stattdessen: feste Blickrichtungen. Die sind neu und haben oben Platz.
    if (anythingBefore) { ImGui::SameLine(); ImGui::TextUnformatted("|"); ImGui::SameLine(); }
    struct ViewButton {
        const char* id;
        Icon icon;
        Str tip;
        camera::Orbit::View view;
    };
    static const ViewButton kViews[] = {
        {"##viewFront",  Icon::ViewFront,  Str::ViewFront,  camera::Orbit::View::Front},
        {"##viewBack",   Icon::ViewBack,   Str::ViewBack,   camera::Orbit::View::Back},
        {"##viewLeft",   Icon::ViewLeft,   Str::ViewLeft,   camera::Orbit::View::Left},
        {"##viewRight",  Icon::ViewRight,  Str::ViewRight,  camera::Orbit::View::Right},
        {"##viewTop",    Icon::ViewTop,    Str::ViewTop,    camera::Orbit::View::Top},
        {"##viewBottom", Icon::ViewBottom, Str::ViewBottom, camera::Orbit::View::Bottom},
    };
    bool firstView = true;
    for (const ViewButton& b : kViews) {
        if (!firstView) ImGui::SameLine();
        firstView = false;
        if (iconButton(b.id, b.icon, tr(b.tip), false, iconSize)) {
            camera_.lookFrom(b.view);
        }
    }
    ImGui::SameLine();
    if (iconButton("##viewReset", Icon::ViewReset, tr(Str::ViewReset), false,
                   iconSize)) {
        // Auf die Ausgangsansicht, wie beim Programmstart.
        camera_.reset(settings_.worldScale);
    }
    ImGui::SameLine();
    // Das Feld zeigt `repeatDelay` DER DATEI, in Sekunden.
    //
    // Es hing an einer Einstellung des Editors — einer eigenen
    // Wiederholrate, die mit dem Effekt nichts zu tun hat. Beim Anwender
    // stand dort 1.062, waehrend die Datei `repeatDelay 300` sagte und die
    // Vorschau auch danach lief. Das Original zeigt an derselben Stelle
    // 0.300, also den Wert aus der Datei.
    //
    // Ein Feld, das eine Zahl zeigt, die nirgends wirkt, ist schlimmer als
    // keines: wer daran dreht, glaubt etwas geaendert zu haben.
    float repeatSeconds = static_cast<float>(doc().effect.repeatDelay) * 0.001f;
    ImGui::SetNextItemWidth(narrow);
    const bool byField =
        ImGui::DragFloat("##repeatRate", &repeatSeconds, 0.005f, 0.0f, 10.0f, "%.3f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::ToolRepeatRate));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(wide);
    const bool bySlider =
        ImGui::SliderFloat("##repeatSlider", &repeatSeconds, 0.0f, 2.0f, "");

    if (byField || bySlider) {
        const int millis = static_cast<int>(repeatSeconds * 1000.0f + 0.5f);
        if (millis != doc().effect.repeatDelay) {
            doc().effect.repeatDelay = millis;
            doc().effect.repeatDelaySet = millis > 0;
            doc().dirty = true;
            // Sofort sichtbar: `repeatDelay` bestimmt Vorlauf und
            // Schleifenlaenge, das laesst sich nicht nachtraeglich anpassen.
            refreshPreview();
        }
    }
    }

    // Zeile 2: Massstaebe
    ImGui::SetNextItemWidth(wide * 1.6f);
    int current = 0;
    for (int i = 0; i < layout::worldScaleCount(); ++i) {
        if (layout::worldScales()[i].unitsPerFoot == settings_.worldScale) current = i;
    }
    if (ImGui::BeginCombo("##worldScale", layout::worldScales()[current].label())) {
        for (int i = 0; i < layout::worldScaleCount(); ++i) {
            const auto& scale = layout::worldScales()[i];
            if (ImGui::Selectable(scale.label(), i == current)) {
                settings_.worldScale = scale.unitsPerFoot;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(narrow);
    // Untergrenze 0.10, wie im Original.
    //
    // Wir standen bei 0.01 — Faktor zehn daneben. Aufgefallen ist es beim
    // Nebeneinanderstellen beider Programme: ganz links zeigt das Original
    // 0.10, unseres 0.01.
    //
    // Es ist nicht nur Formsache. Bei 0.01 dauert ein Effekt von vier
    // Sekunden sechseinhalb Minuten, und wer den Regler versehentlich ganz
    // nach links zieht, haelt das Programm fuer eingefroren.
    constexpr float kSlowestPlayback = 0.10f;
    if (ImGui::DragFloat("##timeScale", &settings_.timeScale, 0.01f,
                         kSlowestPlayback, 10.0f,
                         "%.2f")) {
        doc().clock.setSpeed(settings_.timeScale);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::ToolTimeScale));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(wide);
    if (ImGui::SliderFloat("##timeSlider", &settings_.timeScale, kSlowestPlayback,
                           4.0f, "")) {
        doc().clock.setSpeed(settings_.timeScale);
    }
    // Die sieben Ansichtsknoepfe — Leiste 167 des Originals, einzeln
    // abschaltbar wie die anderen drei Gruppen.
    if (settings_.showWorldToolbar) {
    // Die sieben Ansichtsknoepfe, in der Reihenfolge der Werkzeugleiste 167
    // des Originals: Achsen | Raum, Gitter | Texturiert, Drahtgitter,
    // Overdraw | Wind.
    //
    // Als Umschalter mit Hilfetext. Die Hilfetexte sind die des Originals —
    // sie sagen praeziser, was ein Knopf tut, als jeder Name. "Draw Room"
    // etwa schaltet nur die Waende, nicht den Boden, und das steht dort.
    ImGui::SameLine();
    ImGui::TextUnformatted("|");

    auto toggleButton = [&](const char* id, Icon icon, bool* value, Str tip) {
        ImGui::SameLine();
        if (iconButton(id, icon, tr(tip), *value, iconSize)) {
            *value = !*value;
            geometryDirty_ = true;
        }
    };

    toggleButton("##axes", Icon::Axes, &settings_.drawAxes, Str::ToolDrawAxes);
    toggleButton("##room", Icon::Room, &settings_.drawRoom, Str::ToolDrawRoom);
    toggleButton("##grid", Icon::Grid, &settings_.drawGrid, Str::ToolDrawGrid);

    // Die drei Darstellungsarten schliessen sich gegenseitig aus — im
    // Original sind es drei Knoepfe, von denen immer genau einer gedrueckt
    // ist.
    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    const struct { const char* id; Icon icon; int mode; Str tip; } modes[] = {
        {"##textured", Icon::Textured, 0, Str::ToolDrawTextured},
        {"##wireframe", Icon::Wireframe, 1, Str::ToolDrawWireframe},
        {"##overdraw", Icon::Overdraw, 2, Str::ToolDrawOverdraw},
    };
    for (const auto& entry : modes) {
        ImGui::SameLine();
        if (iconButton(entry.id, entry.icon, tr(entry.tip),
                       settings_.effectRenderMode == entry.mode, iconSize)) {
            settings_.effectRenderMode = entry.mode;
        }
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    toggleButton("##wind", Icon::Wind, &settings_.drawWindVector,
                 Str::ToolDrawWind);
    }  // showWorldToolbar

    ImGui::SameLine();
    ImGui::SetNextItemWidth(wide * 1.2f);
    const Str orientLabels[] = {Str::EffectsOrientUp, Str::EffectsOrientSide,
                                Str::EffectsOrientDown};
    if (ImGui::BeginCombo("##orientation", tr(orientLabels[settings_.orientation]))) {
        for (int i = 0; i < 3; ++i) {
            if (ImGui::Selectable(tr(orientLabels[i]), settings_.orientation == i)) {
                settings_.orientation = i;
            }
        }
        ImGui::EndCombo();
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

    if (ImGui::BeginTabItem(tr(Str::TabStart), nullptr, startFlags)) {
        startTabActive_ = true;
        ImGui::EndTabItem();
    }

    int closeAt = -1;
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
        if (ImGui::BeginTabItem(label.c_str(), &open)) {
            startTabActive_ = false;
            if (static_cast<int>(i) != activeDocument_) {
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
    if (closeAt >= 0) closeDocument(closeAt);

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
                   doc().clock.state() == timeline::State::Playing, iconSize)) {
        pressPlay();
    }
    ImGui::SameLine();
    if (iconButton("##tlPause", Icon::Pause, tr(Str::ToolPause),
                   doc().clock.state() == timeline::State::Paused, iconSize)) {
        doc().clock.togglePause();
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
            if (ImGui::MenuItem(tr(Str::MenuLegacyRepeat), nullptr,
                                settings_.legacyRepeat)) {
                settings_.legacyRepeat = !settings_.legacyRepeat;
                if (playing()) startPlayback(); else buildPreviewStopped();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tr(Str::LegacyRepeatTip));
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

void App::drawSegmentList(float width, float height) {
    ImGui::BeginChild("segments", ImVec2(width, height), ImGuiChildFlags_Borders);

    const ImGuiTableFlags flags = ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable |
                                  ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("segmentTable", 5, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(tr(Str::ListName), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(tr(Str::ListType), ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn(tr(Str::ListSegment), ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn(tr(Str::ListDelay), ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn(tr(Str::ListCount), ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

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

            const std::string label = p.name.empty() ? tr(Str::ListUnnamed) : p.name;
            if (ImGui::Selectable(label.c_str(), doc().selectedPrimitive == i,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                doc().selectedPrimitive = i;
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
            if (p.delay.set) ImGui::Text("%g", p.delay.min); else ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(4);
            if (p.count.set) ImGui::Text("%g", p.count.min); else ImGui::TextDisabled("-");

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
    if (errors == 0 && warnings == 0) {
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

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 320.0f);
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
    ImGui::Text("%s: %d   %s: %d   %.1f FPS", tr(Str::StatusActive), lastAlive_,
                tr(Str::StatusDrawn), lastDrawn_,
                static_cast<double>(ImGui::GetIO().Framerate));
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
