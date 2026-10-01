// Die Dialoge: Spielpfad, Wiedergabe, neues Segment, Ursprung, Farben,
// Wind, Auswahllisten, Sonne, Treiberinfo.
//
// Alle nach demselben Muster gebaut — aufgehen, Werte bearbeiten, mit OK
// oder Abbrechen schliessen. Zusammen an einer Stelle sieht man das Muster;
// verstreut in einer 3500-Zeilen-Datei sieht man es nicht.
//
// Teil der Klasse App aus app.h.
#include "app.h"
#include <algorithm>
#include <cctype>

#include "efx/help.h"
#include "app_shared.h"

#include <cfloat>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include "efx/diag.h"
#include "efx/gamepath.h"
#include "efx/paths.h"
#include "efx/version.h"
#include "efx/i18n.h"

namespace efx::gui {

using i18n::Str;
using i18n::tr;

void App::drawGamePathDialog() {
    if (showGamePathDialog_) {
        ImGui::OpenPopup("###gamepath");
        showGamePathDialog_ = false;
        // Fuer Abbrechen: Zusatzpfade werden im Dialog direkt bearbeitet.
        extraPathsBackup_ = settings_.extraGamePaths;
    }

    // Dieselbe Falle wie beim Wiedergabefenster: automatische Breite plus
    // Eingabefelder mit Breite -1. Ein langer Pfad macht das Fenster sonst so
    // breit wie der Pfad.
    const float pathDialogWidth = 620.0f;
    ImGui::SetNextWindowSize(ImVec2(pathDialogWidth, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(pathDialogWidth, 0.0f),
                                        ImVec2(pathDialogWidth, FLT_MAX));
    if (!ImGui::BeginPopupModal(
            (std::string(tr(Str::DialogGamePath)) + "###gamepath").c_str(), nullptr,
            ImGuiWindowFlags_NoResize)) {
        return;
    }

    ImGui::TextWrapped("%s", tr(Str::GamePathIntro));
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##gamePath", gamePathBuffer_, sizeof(gamePathBuffer_));

    // Sofort prüfen, während getippt wird. Der alte Dialog nahm jeden Pfad
    // wortlos an; dass er falsch war, merkte man erst, wenn Shader fehlten —
    // und dann sucht man den Fehler beim Effekt statt beim Pfad.
    const auto inspection = gamepath::inspect(gamePathBuffer_);
    const theme::Palette& palette = activeTheme(settings_).palette;

    switch (inspection.status) {
        case gamepath::Status::Ok: {
            ImGui::TextColored(toImGui(palette.ok), "%s", tr(Str::GamePathValid));
            ImGui::SameLine();
            ImGui::TextDisabled(tr(Str::DialogAssetCounts),
                                tr(Str::GamePathContents), inspection.pk3Count,
                                inspection.shaderCount, inspection.efxCount);
            break;
        }
        case gamepath::Status::Missing:
            ImGui::TextColored(toImGui(palette.error), "%s",
                               tr(Str::GamePathMissing));
            break;
        case gamepath::Status::NotBaseLike:
            ImGui::TextColored(toImGui(palette.warning), "%s",
                               tr(Str::GamePathNoBase));
            for (const auto& note : inspection.notes) {
                ImGui::TextDisabled("  %s", note.c_str());
            }
            break;
        default:
            ImGui::TextDisabled(" ");
            break;
    }

    // Durchsuchen — dieselbe Dateiauswahl wie beim Oeffnen, nur auf einen
    // Ordner. Wer einen Pfad von Hand tippt, vertippt sich.
    ImGui::SameLine();
    if (ImGui::Button(tr(Str::PathsBrowse)) && folderDialog_) {
        // Einen ORDNER waehlen. Vorher musste man im Dateidialog irgendeine
        // Datei anklicken, damit das Programm den Ordner davon nimmt — das
        // funktioniert, aber niemand kommt von selbst darauf, und in einem
        // base-Ordner voller .pk3-Dateien sieht es aus, als solle man ein
        // Archiv auswaehlen.
        //
        // Gemeint ist immer der Ordner: das Programm liest ALLE .pk3-Dateien
        // darin auf einmal, nicht eine bestimmte.
        const std::string picked = folderDialog_(tr(Str::GamePathIntro),
                                                 gamePathBuffer_);
        if (!picked.empty()) {
            std::snprintf(gamePathBuffer_, sizeof(gamePathBuffer_), "%s",
                          picked.c_str());
        }
    }

    // --- Weitere Ordner ---------------------------------------------------
    ImGui::Separator();
    ImGui::TextDisabled("%s", tr(Str::PathsSearched));
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + pathDialogWidth - 40.0f);
    ImGui::TextDisabled("%s", tr(Str::PathsHint));
    ImGui::PopTextWrapPos();

    // Der aktive Pfad steht immer an erster Stelle und laesst sich nicht
    // verschieben — er ist der, der oben im Feld steht.
    ImGui::BeginDisabled();
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::TextUnformatted(gamePathBuffer_[0] ? gamePathBuffer_ : "—");
    ImGui::EndDisabled();

    // Eine Tabelle mit fester Knopfspalte.
    //
    // Vorher standen die Knoepfe ueber SameLine(breite - 150) an einer festen
    // Stelle — bei einem langen Pfad lagen sie mitten auf dem Text. Eine
    // Tabelle mit fester Spaltenbreite kann das nicht passieren: der Text
    // bekommt, was uebrig bleibt, und wird abgeschnitten statt ueberdeckt.
    int removeAt = -1, moveAt = 0, moveTo = 0;
    if (!settings_.extraGamePaths.empty() &&
        ImGui::BeginTable("##paths", 2,
                          ImGuiTableFlags_SizingFixedFit |
                          ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableSetupColumn("##path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##buttons", ImGuiTableColumnFlags_WidthFixed,
                                96.0f);

        for (size_t i = 0; i < settings_.extraGamePaths.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            const auto state = gamepath::inspect(settings_.extraGamePaths[i]);
            const char* path = settings_.extraGamePaths[i].c_str();
            if (state.status == gamepath::Status::Missing) {
                ImGui::TextColored(toImGui(palette.error), "%s", path);
            } else {
                ImGui::TextUnformatted(path);
            }
            // Der volle Pfad im Hinweis — die Spalte kann ihn abschneiden.
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path);

            ImGui::TableSetColumnIndex(1);
            if (ImGui::SmallButton("^") && i > 0) {
                moveAt = static_cast<int>(i);
                moveTo = static_cast<int>(i) - 1;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::PathsUp));
            ImGui::SameLine();
            if (ImGui::SmallButton("v") &&
                i + 1 < settings_.extraGamePaths.size()) {
                moveAt = static_cast<int>(i);
                moveTo = static_cast<int>(i) + 1;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::PathsDown));
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) removeAt = static_cast<int>(i);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tr(Str::PathsRemove));
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // Erst NACH der Schleife aendern — sonst laeuft der Zeiger ins Leere.
    if (moveAt != moveTo) {
        std::swap(settings_.extraGamePaths[static_cast<size_t>(moveAt)],
                  settings_.extraGamePaths[static_cast<size_t>(moveTo)]);
    }
    if (removeAt >= 0) {
        settings_.extraGamePaths.erase(settings_.extraGamePaths.begin() + removeAt);
    }

    if (ImGui::Button(tr(Str::PathsAdd)) && folderDialog_) {
        const std::string picked =
            folderDialog_(tr(Str::PathsAdd),
                          settings_.extraGamePaths.empty()
                              ? gamePathBuffer_
                              : settings_.extraGamePaths.back().c_str());
        if (!picked.empty()) {
            settings_.extraGamePaths.push_back(gamepath::normalise(picked));
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(tr(Str::GamePathScanSubfolders)) && folderDialog_) {
        // Einen Ordner waehlen und ALLE Spielordner darunter uebernehmen.
        //
        // Der uebliche Fall: man zeigt auf `GameData` und moechte base, seine
        // Mod und den eigenen Arbeitsordner auf einmal haben.
        //
        // Warum das nicht einfach ein rekursives Durchsuchen ist: die
        // ausgepackten Dateien bekaemen dann Pfade wie `base/gfx/...` statt
        // `gfx/...`, und nichts wuerde mehr aufgeloest. Es muessen die
        // WURZELN gefunden und einzeln durchsucht werden.
        const std::string picked =
            folderDialog_(tr(Str::GamePathScanSubfolders), gamePathBuffer_);
        if (!picked.empty()) {
            const auto roots = assets::discoverRoots(picked);
            int added = 0;
            for (const auto& root : roots) {
                const std::string tidy = gamepath::normalise(root);
                bool known = tidy == gamepath::normalise(gamePathBuffer_);
                for (const auto& seen : settings_.extraGamePaths) {
                    if (seen == tidy) known = true;
                }
                if (known) continue;
                settings_.extraGamePaths.push_back(tidy);
                ++added;
            }
            scanMessage_ = added;
        }
    }
    if (scanMessage_ > 0) {
        ImGui::TextDisabled(tr(Str::GamePathFoundRoots), scanMessage_);
    }

    ImGui::Separator();
    ImGui::TextDisabled("%s", tr(Str::GamePathPresets));
    // Zusaetzlich das, was in der Registrierung steht.
    //
    // Die Ladenfassungen auf CD liegen dort, wohin man sie installiert hat —
    // eine Vorlage mit festem Pfad trifft das nur zufaellig. Der Installer von
    // LucasArts traegt den echten Ordner aber ein, und Steam wie GOG tun das
    // ebenfalls.
    //
    // Best effort: was nicht da ist, wird nicht angezeigt. Lieber ein Knopf
    // weniger als einer, der ins Leere fuehrt.
    for (const auto& found : registryGamePaths()) {
        const bool exists =
            gamepath::inspect(found.second).status != gamepath::Status::Missing;
        if (!exists) continue;
        if (ImGui::Button(found.first.c_str())) {
            std::snprintf(gamePathBuffer_, sizeof(gamePathBuffer_), "%s",
                          found.second.c_str());
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", found.second.c_str());
        ImGui::SameLine();
    }

    for (const auto& preset : gamepath::presets()) {
        // Nur Vorlagen anbieten, deren Ordner es auf diesem Rechner gibt —
        // eine Schaltfläche, die einen toten Pfad einträgt, hilft niemandem.
        const bool exists =
            gamepath::inspect(preset.path).status != gamepath::Status::Missing;
        if (!exists) ImGui::BeginDisabled();
        if (ImGui::Button(preset.label)) {
            std::snprintf(gamePathBuffer_, sizeof(gamePathBuffer_), "%s",
                          preset.path);
        }
        if (!exists) ImGui::EndDisabled();
        // Mit der rechten Maustaste als ZUSAETZLICHEN Ordner. So kommt man
        // mit zwei Klicks zu "Grundspiel plus Mod", ohne Pfade zu tippen.
        if (exists && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            settings_.extraGamePaths.push_back(gamepath::normalise(preset.path));
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\n%s", preset.path, tr(Str::PathsAdd));
        }
    }

    ImGui::Separator();
    if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) {
        settings_.gamePath = gamepath::normalise(gamePathBuffer_);
        diag::info("game paths: " +
                   std::to_string(settings_.allGamePaths().size()));
        rescanAssets();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(Str::MsgCancel), ImVec2(120, 0))) {
        settings_.extraGamePaths = extraPathsBackup_;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void App::drawPlaybackDialog() {
    if (showPlaybackDialog_) {
        // Im Dialog wird ein ENTWURF bearbeitet; erst Ok uebernimmt ihn.
        // Vorher wirkte jede Aenderung sofort, und Abbrechen tat nichts.
        playbackDraft_ = playback_;
        ImGui::OpenPopup("###playback");
        showPlaybackDialog_ = false;
    }
    // AlwaysAutoResize UND Regler mit Breite -1 schaukeln sich auf: das
    // Fenster waechst nach dem breitesten Element, und die Regler nehmen sich
    // die neue Breite. Bei einem breiten Bildschirm wird der Dialog dann
    // meterbreit — genau das war zu sehen.
    //
    // Also feste Breite statt automatischer, und die Regler bekommen sie
    // vorgegeben. Die Hoehe darf sich weiter nach dem Inhalt richten; nur die
    // Breite ist das Problem.
    const float dialogWidth = 460.0f;
    ImGui::SetNextWindowSize(ImVec2(dialogWidth, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(dialogWidth, 0.0f),
                                        ImVec2(dialogWidth, FLT_MAX));
    if (!ImGui::BeginPopupModal(
            (std::string(tr(Str::DialogPlayback)) + "###playback").c_str(), nullptr,
            ImGuiWindowFlags_NoResize)) {
        return;
    }

    ImGui::SeparatorText(tr(Str::PlaybackRepeatMode));
    int mode = static_cast<int>(playbackDraft_.mode);
    ImGui::RadioButton(tr(Str::PlaybackOnce), &mode, 0);
    ImGui::RadioButton(tr(Str::PlaybackUntilStopped), &mode, 1);
    ImGui::RadioButton(tr(Str::PlaybackForSeconds), &mode, 2);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::BeginDisabled(mode != 2);
    ImGui::DragFloat("##repeatFor", &playbackDraft_.repeatForSeconds, 0.1f, 0.0f, 600.0f,
                     "%.1f");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted(tr(Str::PlaybackSeconds));
    playbackDraft_.mode = static_cast<playback::RepeatMode>(mode);
    // In die Uhr durchreichen: sie ist die einzige Wahrheit ueber das, was am
    // Ende geschieht.
    doc().clock.setEndMode(playbackDraft_.mode == playback::RepeatMode::Once
                          ? timeline::EndMode::Stop
                          : timeline::EndMode::Repeat);

    // Der ganze Block gilt nur beim Wiederholen — im Original ist er dann
    // ausgegraut, und das ist die richtige Anzeige: die Felder existieren
    // weiter, sie wirken nur nicht.
    const bool repeating = playbackDraft_.mode != playback::RepeatMode::Once;
    ImGui::BeginDisabled(!repeating);

    ImGui::SeparatorText(tr(Str::PlaybackRateGroup));
    ImGui::Checkbox(tr(Str::PlaybackEveryFrame), &playbackDraft_.respawnEveryFrame);

    ImGui::BeginDisabled(playbackDraft_.respawnEveryFrame);
    // Zwei Felder, ein Wert. Wer eines aendert, fuehrt das andere nach —
    // gespeichert wird nur die Rate, die Frequenz ist ihr Kehrwert.
    ImGui::TextUnformatted(tr(Str::PlaybackRate));
    float rate = playbackDraft_.repeatRateSeconds;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##rate", &rate, playback::Settings::kMinRate, 2.0f,
                           "%.3f", ImGuiSliderFlags_Logarithmic)) {
        playbackDraft_.setRate(rate);
    }

    ImGui::TextUnformatted(tr(Str::PlaybackFrequency));
    float frequency = playbackDraft_.frequency();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##frequency", &frequency, 0.5f, 200.0f, "%.3f",
                           ImGuiSliderFlags_Logarithmic)) {
        playbackDraft_.setFrequency(frequency);
    }

    const int total = playbackDraft_.totalRepetitions();
    ImGui::Text("%s: %s", tr(Str::PlaybackTotal),
                total < 0 ? tr(Str::PlaybackNotApplicable)
                          : std::to_string(total).c_str());
    ImGui::EndDisabled();

    ImGui::SeparatorText(tr(Str::PlaybackMoveGroup));
    ImGui::Checkbox(tr(Str::PlaybackAnimate), &playbackDraft_.animateSpawnLocation);
    ImGui::BeginDisabled(!playbackDraft_.animateSpawnLocation);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::DragFloat3(tr(Str::PlaybackVelocity), &playbackDraft_.spawnVelocity.x, 1.0f,
                      -4000.0f, 4000.0f, "%.1f");
    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat(tr(Str::PlaybackResetAfter), &playbackDraft_.resetLocationAfter,
                     0.1f, 0.0f, 600.0f, "%.2f");
    ImGui::EndDisabled();

    ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) {
        playback_ = playbackDraft_;
        doc().clock.setEndMode(playback_.mode == playback::RepeatMode::Once
                                   ? timeline::EndMode::Stop
                                   : timeline::EndMode::Repeat);
        settings_.repeatRate = playback_.repeatRateSeconds;
        settings_.repeat = repeating;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(Str::MsgCancel), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void App::drawNewSegmentDialog() {
    if (showNewSegmentDialog_) {
        ImGui::OpenPopup("###newsegment");
        showNewSegmentDialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            (std::string(tr(Str::DialogNewSegment)) + "###newsegment").c_str(),
            nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    // Dieselben dreizehn Typen wie im Original, alphabetisch wie dort.
    static const PrimitiveType kTypes[] = {
        PrimitiveType::CameraShake, PrimitiveType::Cylinder, PrimitiveType::Decal,
        PrimitiveType::Electricity, PrimitiveType::Emitter,
        PrimitiveType::ScreenFlash, PrimitiveType::FxRunner, PrimitiveType::Light,
        PrimitiveType::Line, PrimitiveType::OrientedParticle,
        PrimitiveType::Particle, PrimitiveType::Sound, PrimitiveType::Tail,
    };
    constexpr int kCount = static_cast<int>(sizeof(kTypes) / sizeof(kTypes[0]));
    if (newSegmentType_ < 0 || newSegmentType_ >= kCount) newSegmentType_ = 0;

    ImGui::TextUnformatted(tr(Str::NewSegmentType));
    // So hoch, dass alle dreizehn Typen ohne Rollen passen. Fest 280 Punkte
    // schnitten bei 150 % Skalierung die letzten fuenf ab — der Selbsttest
    // fand "OrientedParticle", "Particle", "Sound" und "Tail" nicht.
    const float listHeight = ImGui::GetTextLineHeightWithSpacing() * (kCount + 0.5f) +
                             ImGui::GetStyle().WindowPadding.y * 2.0f;
    ImGui::BeginChild("typelist", ImVec2(ImGui::GetFontSize() * 16.0f, listHeight),
                      ImGuiChildFlags_Borders);
    for (int i = 0; i < kCount; ++i) {
        if (ImGui::Selectable(typeName(kTypes[i]), newSegmentType_ == i)) {
            newSegmentType_ = i;
        }
        // Doppelklick uebernimmt sofort — im Original genauso.
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            // Doppelklick = Ok. Vorher legte der Doppelklick ein Primitive{}
            // mit anderen Vorgaben an als Ok (freshPrimitive) und trug den
            // Rueckgaengig-Schritt ein, BEVOR der Typ gesetzt war.
            newSegmentType_ = i;
            cmdAddSegment(kTypes[i]);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) {
        // Mit brauchbaren Anfangswerten: ein neues CameraShake ohne Staerke
        // und Reichweite tut gar nichts, und das sieht aus wie ein Fehler im
        // Programm.
        cmdAddSegment(kTypes[newSegmentType_]);
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::Button(tr(Str::MsgCancel), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
    ImGui::Spacing();
    // Die Beschreibung des gewaehlten Typs — im Original steht sie rechts
    // neben der Liste, und sie ist das einzige, was einem beim Auswaehlen
    // hilft.
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 200.0f);
    ImGui::TextUnformatted(typeDescription(kTypes[newSegmentType_]));
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();

    ImGui::EndPopup();
}

void App::drawSpawnOriginDialog() {
    if (showSpawnOriginDialog_) {
        spawnOriginDraft_ = spawnOrigin_;  // erst Ok uebernimmt
        ImGui::OpenPopup("###spawnorigin");
        showSpawnOriginDialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            (std::string(tr(Str::DialogSpawnOrigin)) + "###spawnorigin").c_str(),
            nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::SeparatorText(tr(Str::OriginPosition));
    int mode = static_cast<int>(spawnOriginDraft_.mode);
    const Str labels[] = {Str::OriginDefault, Str::OriginRoomCentre,
                          Str::OriginOnFloor, Str::OriginOnCeiling,
                          Str::OriginOnWall, Str::OriginCustom};
    for (int i = 0; i < 6; ++i) ImGui::RadioButton(tr(labels[i]), &mode, i);
    spawnOriginDraft_.mode = static_cast<playback::OriginMode>(mode);

    ImGui::BeginDisabled(spawnOriginDraft_.mode != playback::OriginMode::Custom);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::DragFloat3("##customOrigin", &spawnOriginDraft_.custom.x, 1.0f, -100000.0f,
                      100000.0f, "%.1f");
    ImGui::EndDisabled();

    // Was dabei herauskommt, sofort anzeigen. Der alte Dialog liess einen
    // raten, wo "an der Wand" liegt.
    const auto resolved = spawnOriginDraft_.resolve(roomSize_, settings_.worldScale);
    ImGui::TextDisabled("-> %.1f  %.1f  %.1f", static_cast<double>(resolved.x),
                        static_cast<double>(resolved.y),
                        static_cast<double>(resolved.z));

    ImGui::Separator();
    if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) {
        spawnOrigin_ = spawnOriginDraft_;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(Str::MsgCancel), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void App::drawColourDialogs() {
    struct ColourEntry {
        bool* show;
        bool* overridden;
        float* value;
        Str title;
        const theme::Color* themeColour;
    };
    const theme::Palette& palette = activeTheme(settings_).palette;
    const ColourEntry entries[] = {
        {&showWallColourDialog_, &wallColourOverridden_, wallColour_,
         Str::ColorWall, &palette.roomWall},
        {&showBackgroundColourDialog_, &backgroundColourOverridden_,
         backgroundColour_, Str::ColorBackground, &palette.viewportBg},
    };

    for (const auto& entry : entries) {
        const std::string id = std::string("###colour") +
                               std::to_string(static_cast<int>(entry.title));
        if (*entry.show) {
            // Beim Oeffnen mit der aktuellen Farbe vorbelegen — sonst springt
            // die Ansicht beim ersten Anfassen auf einen fremden Wert.
            if (!*entry.overridden) {
                entry.value[0] = entry.themeColour->r;
                entry.value[1] = entry.themeColour->g;
                entry.value[2] = entry.themeColour->b;
            }
            // Fuer Abbrechen: den Stand beim Oeffnen merken. Vorher gab es
            // nur "Zuruecksetzen" und "Ok" — wer sich verklickt hatte, kam
            // nicht mehr zu seiner Farbe zurueck.
            colourBackup_[0] = entry.value[0];
            colourBackup_[1] = entry.value[1];
            colourBackup_[2] = entry.value[2];
            colourBackupOverridden_ = *entry.overridden;
            ImGui::OpenPopup(id.c_str());
            *entry.show = false;
        }
        if (ImGui::BeginPopupModal((std::string(tr(entry.title)) + id).c_str(),
                                   nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::ColorPicker3("##picker", entry.value,
                                    ImGuiColorEditFlags_NoSidePreview |
                                        ImGuiColorEditFlags_NoSmallPreview)) {
                *entry.overridden = true;
                geometryDirty_ = true;
            }
            ImGui::Separator();
            if (ImGui::Button(tr(Str::ColorReset), ImVec2(160, 0))) {
                *entry.overridden = false;
                geometryDirty_ = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::SameLine();
            if (ImGui::Button(tr(Str::MsgCancel), ImVec2(120, 0))) {
                entry.value[0] = colourBackup_[0];
                entry.value[1] = colourBackup_[1];
                entry.value[2] = colourBackup_[2];
                *entry.overridden = colourBackupOverridden_;
                geometryDirty_ = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void App::drawWindDialog() {
    if (showWindDialog_) {
        ImGui::OpenPopup("###wind");
        showWindDialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            (std::string(tr(Str::DialogWind)) + "###wind").c_str(), nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::DragFloat3(tr(Str::WindDirection), settings_.windDirection, 0.02f,
                          -1.0f, 1.0f, "%.2f")) {
        geometryDirty_ = true;
    }
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::SliderFloat(tr(Str::WindSpeed), &settings_.windSpeed, 0.2f, 5.0f,
                           "%.2f")) {
        geometryDirty_ = true;
    }
    ImGui::Checkbox(tr(Str::ViewWindVector), &settings_.drawWindVector);

    ImGui::Separator();
    // Der Pfeil ist eine Anzeigehilfe und sonst nichts. Das gehoert hierher,
    // sonst dreht jemand daran und wundert sich, warum die Partikel nicht
    // reagieren.
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 380.0f);
    ImGui::TextDisabled("%s", tr(Str::WindDeadNote));
    ImGui::PopTextWrapPos();

    ImGui::Separator();
    if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void App::drawPickerDialog() {
    // Auswahl aus dem Bestand: Shader, Modelle, Klaenge, Effekte.
    //
    // Wie "Choose Shaders" im Original (Dialog 184): Mehrfachauswahl (Strg-
    // und Umschalt-Klick), "Textures..." waehlt ein Bild direkt aus gfx/, und
    // "Preview" zeigt das Bild des gewaehlten Shaders. Beide Knoepfe waren
    // bei uns vorher nur gesperrte Attrappen. Doppelklick uebernimmt einen
    // Eintrag sofort.
    if (showPicker_) {
        ImGui::OpenPopup("###picker");
        showPicker_ = false;
        pickerChosen_.clear();
        pickerAnchor_ = -1;
        pickerPreview_ = false;
    }

    const Str title = pickerKind_ == PickerKind::Shaders ? Str::DialogChooseShaders
                                                         : Str::FieldAddEntry;
    const float em = ImGui::GetFontSize();
    ImGui::SetNextWindowSize(ImVec2(em * 40.0f, em * 30.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(em * 26.0f, em * 18.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (!ImGui::BeginPopupModal((std::string(tr(title)) + "###picker").c_str(), nullptr,
                                ImGuiWindowFlags_None)) {
        return;
    }
    if (!pickerTarget_) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    const std::vector<std::string>& entries =
        pickerKind_ == PickerKind::Models    ? assets_.models
        : pickerKind_ == PickerKind::Sounds  ? assets_.sounds
        : pickerKind_ == PickerKind::Effects ? assets_.effects
                                             : assets_.shaders;
    if (settings_.gamePath.empty()) ImGui::TextDisabled("%s", tr(Str::ChooseNoGamePath));

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##filter", tr(Str::ChooseFilter), pickerFilter_,
                                 sizeof(pickerFilter_))) {
        pickerChosen_.clear();
        pickerAnchor_ = -1;
    }

    // Suche ohne Gross-/Kleinschreibung: Shadernamen sind in der Engine
    // unabhaengig davon (R_FindShader vergleicht ohne).
    const auto lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string needle = lower(pickerFilter_);
    std::vector<const std::string*> shown;
    for (const auto& entry : entries) {
        if (needle.empty() || lower(entry).find(needle) != std::string::npos) shown.push_back(&entry);
        if (shown.size() >= 5000) break;
    }

    const auto take = [this](const std::string& name) {
        // Doppelte vermeiden: zweimal derselbe Shader verdoppelt nur die
        // Wahrscheinlichkeit, mit der die Engine ihn waehlt.
        if (std::find(pickerTarget_->begin(), pickerTarget_->end(), name) == pickerTarget_->end()) {
            pickerTarget_->push_back(name);
        }
    };
    bool close = false;
    bool added = false;

    const float sideWidth = em * 9.0f;
    ImGui::BeginChild("pickerlist", ImVec2(-sideWidth, -ImGui::GetFrameHeightWithSpacing() * 1.2f),
                      ImGuiChildFlags_Borders);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(shown.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const bool chosen = pickerChosen_.count(i) != 0;
            ImGui::PushID(i);
            if (ImGui::Selectable(shown[static_cast<size_t>(i)]->c_str(), chosen,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                const ImGuiIO& io = ImGui::GetIO();
                if (io.KeyShift && pickerAnchor_ >= 0) {
                    pickerChosen_.clear();
                    for (int k = std::min(pickerAnchor_, i); k <= std::max(pickerAnchor_, i); ++k) {
                        pickerChosen_.insert(k);
                    }
                } else if (io.KeyCtrl) {
                    if (chosen) pickerChosen_.erase(i); else pickerChosen_.insert(i);
                    pickerAnchor_ = i;
                } else {
                    pickerChosen_.clear();
                    pickerChosen_.insert(i);
                    pickerAnchor_ = i;
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    take(*shown[static_cast<size_t>(i)]);
                    added = true;
                    close = true;
                }
            }
            ImGui::PopID();
        }
    }
    if (shown.empty() && !settings_.gamePath.empty()) ImGui::TextDisabled("%s", tr(Str::ChooseNoneFound));
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginGroup();
    const float buttonWidth = sideWidth - ImGui::GetStyle().ItemSpacing.x;
    ImGui::BeginDisabled(pickerChosen_.empty());
    if (ImGui::Button(tr(Str::MsgOk), ImVec2(buttonWidth, 0))) {
        // In der Reihenfolge der Liste, wie das Original (alphabetisch).
        for (int i : pickerChosen_) {
            if (i >= 0 && i < static_cast<int>(shown.size())) take(*shown[static_cast<size_t>(i)]);
        }
        added = true;
        close = true;
    }
    ImGui::EndDisabled();
    if (ImGui::Button(tr(Str::MsgCancel), ImVec2(buttonWidth, 0))) close = true;

    if (pickerKind_ == PickerKind::Shaders) {
        ImGui::Spacing();
        // Ein Bild direkt statt eines Shaders: die Engine nimmt auch einen
        // Bildpfad als Shadernamen (R_FindShader baut dann einen Ersatz-
        // shader daraus).
        if (ImGui::Button(tr(Str::ChooseTextures), ImVec2(buttonWidth, 0)) && fileDialog_) {
            const std::string start = settings_.gamePath.empty() ? std::string()
                                                                 : settings_.gamePath + "/gfx";
            const std::string picked = fileDialog_(
                false, "Image Files (*.tga; *.png; *.jpg)\0*.tga;*.png;*.jpg;*.jpeg\0All files\0*.*\0",
                start.c_str());
            if (!picked.empty()) {
                std::string relative = toGameRelative(picked);
                if (relative.empty()) {
                    // Ausserhalb des Spielpfads findet die Engine das Bild nie
                    // (das Original speicherte dann einen absoluten Pfad).
                    diag::warn("texture outside game path: " + picked);
                } else {
                    const size_t dot = relative.find_last_of('.');
                    if (dot != std::string::npos && relative.find('/', dot) == std::string::npos) {
                        relative.resize(dot);
                    }
                    take(relative);
                    added = true;
                    close = true;
                }
            }
        }
        const bool canPreview = !pickerChosen_.empty();
        ImGui::BeginDisabled(!canPreview);
        if (ImGui::Button(tr(Str::ChoosePreview), ImVec2(buttonWidth, 0))) pickerPreview_ = !pickerPreview_;
        ImGui::EndDisabled();
        if (pickerPreview_ && canPreview && renderer_) {
            const int first = *pickerChosen_.begin();
            if (first < static_cast<int>(shown.size())) {
                const render::TextureId texture =
                    textureFor(renderer_, *shown[static_cast<size_t>(first)], 0.0f);
                if (texture != render::kNoTexture) {
                    ImGui::Image(toImTexture(texture), ImVec2(buttonWidth, buttonWidth));
                }
            }
        }
    }
    ImGui::EndGroup();

    ImGui::TextDisabled(tr(Str::ChooseCount), static_cast<int>(shown.size()),
                        static_cast<int>(entries.size()));

    if (added) {
        doc().dirty = true;
        refreshDiagnostics();
        fieldEditOpen_ = true;
        previewDirty_ = true;
    }
    if (close) {
        pickerTarget_ = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::drawSunDialog() {
    if (showSunDialog_) {
        ImGui::OpenPopup("###sun");
        showSunDialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            (std::string(tr(Str::DialogSun)) + "###sun").c_str(), nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::SeparatorText(tr(Str::ViewRoomStyle));
    const Str styles[] = {Str::RoomEnclosed, Str::RoomOpenSky, Str::RoomNone};
    for (int i = 0; i < 3; ++i) {
        if (ImGui::RadioButton(tr(styles[i]), &settings_.roomStyle, i)) {
            geometryDirty_ = true;
        }
    }

    ImGui::SeparatorText(tr(Str::DialogSun));
    if (ImGui::Checkbox(tr(Str::SunEnabled), &settings_.sunEnabled)) {
        geometryDirty_ = true;
    }
    ImGui::BeginDisabled(!settings_.sunEnabled);

    // Hoehe und Himmelsrichtung statt dreier Zahlen.
    //
    // Ein Richtungsvektor ist genau, aber niemand denkt in Vektoren, wenn er
    // die Sonne tiefer stellen will. Zwei Winkel treffen die Vorstellung —
    // und ein Vektor, den man aus Winkeln baut, ist immer normiert.
    camera::Vec3 direction{settings_.sunDirection[0], settings_.sunDirection[1],
                           settings_.sunDirection[2]};
    direction = camera::normalise(direction);
    float elevation = std::asin(direction.z) * 180.0f / 3.14159265f;
    float azimuth = std::atan2(direction.y, direction.x) * 180.0f / 3.14159265f;

    bool changed = false;
    ImGui::SetNextItemWidth(240.0f);
    changed |= ImGui::SliderFloat(tr(Str::SunElevation), &elevation, 1.0f, 89.0f,
                                  "%.0f\u00B0");
    ImGui::SetNextItemWidth(240.0f);
    changed |= ImGui::SliderFloat(tr(Str::SunAzimuth), &azimuth, -180.0f, 180.0f,
                                  "%.0f\u00B0");
    if (changed) {
        const float e = elevation * 3.14159265f / 180.0f;
        const float a = azimuth * 3.14159265f / 180.0f;
        settings_.sunDirection[0] = std::cos(e) * std::cos(a);
        settings_.sunDirection[1] = std::cos(e) * std::sin(a);
        settings_.sunDirection[2] = std::sin(e);
        geometryDirty_ = true;
    }

    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::SliderFloat(tr(Str::SunAmbient), &settings_.sunAmbient, 0.0f, 1.0f,
                           "%.2f")) {
        geometryDirty_ = true;
    }
    if (ImGui::Checkbox(tr(Str::SunDisc), &settings_.drawSunDisc)) {
        geometryDirty_ = true;
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 380.0f);
    ImGui::TextDisabled("%s", tr(Str::SunNote));
    ImGui::PopTextWrapPos();

    ImGui::Separator();
    if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void App::drawDriverInfoDialog() {
    if (showDriverInfoDialog_) {
        ImGui::OpenPopup("###driverinfo");
        showDriverInfoDialog_ = false;
    }
    // Eine Mindestbreite. AlwaysAutoResize richtet sich rein nach dem Inhalt,
    // und bei kurzen Angaben ergibt das ein Fenster, das schmaler ist als sein
    // eigener Titel.
    ImGui::SetNextWindowSizeConstraints(ImVec2(520.0f, 0.0f),
                                        ImVec2(FLT_MAX, FLT_MAX));
    if (!ImGui::BeginPopupModal(
            (std::string(tr(Str::DialogDriverInfo)) + "###driverinfo").c_str(),
            nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    // Die Angaben stehen schon im Ablaufprotokoll — hier nur sichtbar
    // gemacht. Bei einem Fehlerbericht ist das die erste Frage.
    if (ImGui::BeginTable("driver", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        auto row = [](const char* name, const std::string& value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value.empty() ? "-" : value.c_str());
        };
        // Die Angaben stammen aus der Erkundung beim Start; sie stehen auch
        // im Ablaufprotokoll. Beide Schnittstellen werden gezeigt, nicht nur
        // die benutzte — bei einem Fehlerbericht ist gerade interessant,
        // warum die andere nicht ging.
        if (probes_.empty()) {
            // Sagen, dass nichts da ist, statt eine leere Tabelle zu zeigen.
            // Ein leeres Fenster liest sich als Fehler des Programms; eine
            // Zeile mit Begruendung nicht.
            row(tr(Str::DialogDriverInfo), tr(Str::DriverInfoNone));
        }
        for (const auto& probe : probes_) {
            const bool active = probe.backend == activeBackend_;
            row(render::backendName(probe.backend),
                probe.available
                    ? probe.adapter + " (" + probe.version + ")" +
                          (active ? "  <-" : "")
                    : probe.failure);
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void App::drawDialogs() {
    drawSaveChangesDialog();
    drawConfirmDialog();

    drawDriverInfoDialog();
    drawSunDialog();
    drawWindDialog();
    drawPickerDialog();
    drawGamePathDialog();
    drawPlaybackDialog();
    drawNewSegmentDialog();
    drawSpawnOriginDialog();
    drawColourDialogs();

    if (showRendererDialog_) {
        ImGui::OpenPopup("###renderer");
        showRendererDialog_ = false;
    }
    // "Sichtbarer Titel###feste Kennung": ImGui leitet die Kennung eines
    // Fensters sonst aus dem Titel ab. Uebersetzt man den, gilt das Fenster
    // als ein anderes und verliert Groesse, Lage und Bildlauf.
    if (ImGui::BeginPopupModal(
            (std::string(tr(Str::DialogRenderer)) + "###renderer").c_str(),
            nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(tr(Str::MsgRendererRestart));
        ImGui::Separator();
        if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) {
            settings_.rendererCode = render::backendCode(rendererTarget_);
            rendererChangePending_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr(Str::MsgCancel), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (showAboutDialog_) {
        ImGui::OpenPopup("###about");
        showAboutDialog_ = false;
    }
    if (ImGui::BeginPopupModal(
            (std::string(tr(Str::DialogAbout)) + "###about").c_str(), nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(
            (std::string("EffectsEd ") + versionWithRevision()).c_str());
        ImGui::TextDisabled("%s", tr(Str::AboutTagline));
        ImGui::Separator();
        ImGui::TextDisabled("%s: %s", tr(Str::AboutSettings),
                            paths::configDir().c_str());
        ImGui::Separator();
        if (ImGui::Button(tr(Str::MsgOk), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (showMessagesWindow_) {
        // Was die Pruefregeln beanstanden — zum Lesen, nicht nur zum Zaehlen.
        //
        // Ein Klick auf eine Zeile waehlt das Segment aus, um das es geht.
        // Ohne das muesste man die Meldung lesen, das Fenster schliessen und
        // die Liste danach absuchen; bei einem Effekt mit zehn Segmenten ist
        // das jedes Mal dieselbe Sucherei.
        ImGui::SetNextWindowSize(ImVec2(720, 320), ImGuiCond_FirstUseEver);
        // Eine Untergrenze. Ohne sie laesst sich das Fenster auf
        // Fingernagelgroesse ziehen, und dann ist es nicht mehr zu bedienen —
        // man sieht weder Text noch Bedienelemente, nur die Rollbalken.
        ImGui::SetNextWindowSizeConstraints(ImVec2(380.0f, 200.0f),
                                            ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::Begin((std::string(tr(Str::WindowMessages)) + "###messages").c_str(),
                         &showMessagesWindow_)) {
            const theme::Palette& palette = activeTheme(settings_).palette;
            if (doc().diagnostics.empty()) {
                ImGui::TextDisabled("%s", tr(Str::MessagesNone));
            }
            for (size_t i = 0; i < doc().diagnostics.size(); ++i) {
                const auto& d = doc().diagnostics[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::TextColored(severityColor(d.severity, palette), "%s",
                                   d.severity == Severity::Error
                                       ? tr(Str::DiagError)
                                       : tr(Str::DiagWarning));
                ImGui::SameLine();
                if (d.line > 0) {
                    ImGui::TextDisabled(tr(Str::MessagesLine), d.line);
                    ImGui::SameLine();
                }
                if (ImGui::Selectable(d.message.c_str())) {
                    // Die Meldungen der Pruefregeln tragen die Segmentnummer
                    // in `line` — so entsteht die Verbindung zur Liste.
                    const int at = d.line - 1;
                    if (at >= 0 &&
                        at < static_cast<int>(doc().effect.primitives.size())) {
                        doc().selectedPrimitive = at;
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
    }

    if (showLogWindow_) {
        // Das Protokollfenster gab es schon — es konnte nur nichts als
        // vorbeiziehende Zeilen. Wer wissen will, WARUM es ruckelt, muss die
        // Zeilen mit Zahlen aus demselben Bild zusammenbringen; wer einen
        // Fehler sucht, will die Warnungen allein sehen; und wer ihn melden
        // will, braucht ihn in der Zwischenablage.
        ImGui::SetNextWindowSize(ImVec2(780, 480), ImGuiCond_FirstUseEver);
        // Siehe oben: eine Untergrenze, sonst ist es kleinziehbar bis zur
        // Unbenutzbarkeit. Beim Protokoll faellt das besonders auf, weil die
        // Zeilen lang sind — bei 200 Punkten Breite steht in jeder Zeile nur
        // noch der Zeitstempel.
        ImGui::SetNextWindowSizeConstraints(ImVec2(460.0f, 240.0f),
                                            ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::Begin((std::string(tr(Str::WindowLog)) + "###log").c_str(),
                         &showLogWindow_)) {
            const theme::Palette& palette = activeTheme(settings_).palette;

            // Die Zahlen zuerst: sie beantworten \"warum ruckelt es\", und
            // darunter steht, was dabei schiefging.
            ImGui::TextDisabled(tr(Str::LogCounters), browserRedrawn_,
                                static_cast<int>(browserSlots_.size()),
                                browserDrawCalls_, browserBuildMs_,
                                browserDrawMs_);
            ImGui::TextDisabled(tr(Str::LogFile),
                                paths::startupLogPath().c_str());
            ImGui::Separator();

            ImGui::Checkbox(tr(Str::LogFollow), &logFollow_);
            ImGui::SameLine();
            ImGui::Checkbox(tr(Str::LogWarningsOnly), &logWarningsOnly_);
            ImGui::SameLine();
            if (ImGui::Button(tr(Str::LogCopy))) {
                std::string all;
                for (const auto& line : diag::lines()) {
                    all += line;
                    all += '\n';
                }
                ImGui::SetClipboardText(all.c_str());
            }
            ImGui::Separator();

            ImGui::BeginChild("##logLines", ImVec2(0, 0), false,
                              ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& line : diag::lines()) {
                // Die Stufe steht am Zeilenanfang, der Schreiber setzt sie
                // dorthin. Hier wird nur eingefaerbt, nicht neu zerlegt.
                const bool isError = line.find("ERROR") != std::string::npos;
                const bool isWarning = line.find("WARN") != std::string::npos;
                if (logWarningsOnly_ && !isError && !isWarning) continue;
                if (isError) {
                    ImGui::TextColored(toImGui(palette.error), "%s", line.c_str());
                } else if (isWarning) {
                    ImGui::TextColored(toImGui(palette.warning), "%s", line.c_str());
                } else {
                    ImGui::TextUnformatted(line.c_str());
                }
            }
            if (logFollow_ &&
                ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

void App::takeScreenshot(render::Renderer* renderer, bool toClipboard) {
    std::vector<unsigned char> pixels;
    int width = 0, height = 0;
    if (!renderer || !renderer->readViewport(pixels, width, height)) {
        screenshotMessage_ = tr(Str::MsgScreenshotFailed);
        screenshotMessageUntil_ = static_cast<float>(ImGui::GetTime()) + 4.0f;
        diag::warn("screenshot: viewport read-back not available");
        return;
    }

    if (toClipboard) {
        // Die Zwischenablage braucht Win32 — der Rahmen reicht sie als
        // Rueckruf herein. Vorher schrieb dieser Menuepunkt stattdessen eine
        // Datei, obwohl er "in die Zwischenablage" hiess.
        if (clipboardImage_ && clipboardImage_(pixels, width, height)) {
            screenshotMessage_ = tr(Str::MsgScreenshotClipboard);
            screenshotMessageUntil_ = static_cast<float>(ImGui::GetTime()) + 4.0f;
            diag::info("screenshot copied to clipboard (" + std::to_string(width) + "x" +
                       std::to_string(height) + ")");
            return;
        }
        diag::warn("screenshot: clipboard not available, saving instead");
    }

    // Neben die Einstellungen, mit Zeitstempel im Namen. Ein fester Name
    // wuerde das vorige Bild ueberschreiben, und das merkt man erst, wenn es
    // zu spaet ist.
    const std::string folder = paths::configDir();
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
#ifdef _WIN32
    localtime_s(&parts, &now);
#else
    parts = *std::localtime(&now);
#endif
    char stamp[64] = {};
    std::strftime(stamp, sizeof(stamp), "efxed_%Y%m%d_%H%M%S.tga", &parts);
    const std::string path = folder + "/" + stamp;

    const auto tga = image::encodeTga(pixels.data(), width, height);
    std::ofstream file(path, std::ios::binary);
    if (file) {
        file.write(reinterpret_cast<const char*>(tga.data()),
                   static_cast<std::streamsize>(tga.size()));
    }
    screenshotMessage_ = std::string(tr(Str::MsgScreenshotSaved)) + ": " + stamp;
    screenshotMessageUntil_ = static_cast<float>(ImGui::GetTime()) + 4.0f;
    diag::info("screenshot saved: " + path + " (" + std::to_string(width) + "x" +
               std::to_string(height) + ")");
}


void App::openUsersGuide() {
    // Das Handbuch steckt im Programm. Zum Anzeigen muss es als Datei
    // vorliegen — ein Browser kann nichts aus unserem Speicher lesen.
    //
    // Geschrieben wird es neben die Einstellungen, nicht in den
    // Programmordner: der liegt oft unter Programme und ist nicht
    // beschreibbar. Und nicht in den Temp-Ordner, weil Windows den
    // aufraeumt, waehrend der Browser die Datei noch offen hat.
    //
    // Jedes Mal neu schreiben, statt zu pruefen, ob sie schon da ist: bei
    // 270 KB ist das nicht messbar, und so ist nach einer neuen Fassung
    // auch das Handbuch aktuell.
    const std::string path = paths::configDir() + "/Using_EffectsEd.html";
    {
        std::ofstream out(path, std::ios::binary);
        if (!out.good()) {
            diag::warn("could not write the manual to " + path);
            return;
        }
        out.write(reinterpret_cast<const char*>(help::kUsersGuide),
                  static_cast<std::streamsize>(help::kUsersGuideSize));
    }
    diag::info("manual written: " + path);
    if (shellOpen_) shellOpen_(path);
}

}  // namespace efx::gui
