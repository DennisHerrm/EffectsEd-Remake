// Die Befehle — jeder an genau einer Stelle.
//
// Vorher stand "Segment loeschen" viermal im Quelltext (Menue Bearbeiten,
// Menue Effekte, Werkzeugleiste, Entf-Taste), und jede Kopie tat etwas
// anderes: zwei setzten die Abschalt-Haekchen aller Segmente zurueck, eine
// vergass die Vorschau, die Taste rechnete die Auswahl anders. "Neues
// Segment" legte ueber Ok ein Segment ohne Rueckgaengig-Eintrag an und ueber
// Doppelklick eines mit anderen Vorgaben. Strg+N, Strg+O und Strg+S standen
// im Menue, taten aber nichts. Gefunden hat das der Selbsttest
// (gui/selbsttest.cpp) in seinem ersten Lauf.
//
// Deshalb: Menue, Werkzeugleiste und Tastenkuerzel rufen nur noch diese
// Funktionen. Eine Wahrheit je Befehl.
#include "app.h"

#include <cstdio>

#include "efx/diag.h"
#include "efx/i18n.h"
#include "imgui.h"

namespace efx::gui {

using i18n::Str;
using i18n::tr;

namespace {
constexpr const char kEfxFilter[] = "Effect files (*.efx)\0*.efx\0All files\0*.*\0";

// Die Engine nimmt hoechstens so viele Segmente je Effekt
// (FX_MAX_EFFECT_COMPONENTS in FxScheduler.h).
constexpr size_t kMaxSegments = 24;
}  // namespace

std::string App::displayName(int index) const {
    if (index < 0 || index >= static_cast<int>(doc().effect.primitives.size())) return {};
    const Primitive& p = doc().effect.primitives[static_cast<size_t>(index)];
    if (!p.name.empty()) return p.name;
    return std::string(tr(Str::ListUnnamedPrefix)) + " " + typeName(p.type) + " " +
           std::to_string(index + 1);
}

bool App::hasSelection() const {
    return doc().selectedPrimitive >= 0 &&
           doc().selectedPrimitive < static_cast<int>(doc().effect.primitives.size());
}

bool App::documentInUse(const Document& document) const {
    return !document.filePath.empty() || document.dirty ||
           !document.effect.primitives.empty();
}

void App::showEditor() {
    startTabActive_ = false;
    wantStartTab_ = false;
    wantDocumentTab_ = true;
}

// --- Datei ------------------------------------------------------------------

void App::cmdNew() {
    // Nie einen belegten Effekt ersetzen. Vorher setzte "Neu" den aktuellen
    // Reiter stillschweigend zurueck — ungespeicherte Arbeit war ohne
    // Rueckfrage weg. Jetzt: ein neuer Reiter, wie beim Oeffnen.
    if (documentInUse(doc())) newDocument();
    doc().undo.reset(doc().effect);
    showEditor();
}

void App::cmdOpen() {
    if (!fileDialog_) return;
    const std::string path = fileDialog_(false, kEfxFilter, nullptr);
    if (!path.empty()) openFile(path);
}

bool App::cmdSave() {
    if (doc().filePath.empty()) return cmdSaveAs();
    return saveFile(doc().filePath);
}

bool App::cmdSaveAs() {
    if (!fileDialog_) return false;
    const std::string suggestion =
        doc().filePath.empty() ? std::string("untitled.efx") : doc().filePath;
    const std::string path = fileDialog_(true, kEfxFilter, suggestion.c_str());
    if (path.empty()) return false;
    return saveFile(path);
}

void App::cmdOpenPk3() {
    if (!fileDialog_) return;
    const std::string picked =
        fileDialog_(false, "PK3 (*.pk3)\0*.pk3\0All files\0*.*\0", nullptr);
    if (!picked.empty() && openArchive(picked)) {
        startTabActive_ = true;
        wantStartTab_ = true;
    }
}

// --- Fragen vor dem Verwerfen -------------------------------------------------
//
// Das Original (MFC) fragt vor dem Schliessen einer geaenderten Datei. Bei
// uns fragte nichts: Strg+W, das Kreuz am Reiter, Datei > Beenden und das
// Schliessen des Fensters warfen ungespeicherte Arbeit einfach weg.

void App::requestCloseDocument(int index) {
    if (index < 0 || index >= static_cast<int>(documents_.size())) return;
    if (!documents_[static_cast<size_t>(index)].dirty) {
        closeDocument(index);
        return;
    }
    pendingClose_ = index;
    pendingQuit_ = false;
    showSaveChangesDialog_ = true;
}

void App::requestQuit() {
    for (size_t i = 0; i < documents_.size(); ++i) {
        if (documents_[i].dirty) {
            pendingClose_ = static_cast<int>(i);
            pendingQuit_ = true;
            showSaveChangesDialog_ = true;
            return;
        }
    }
    wantsQuit_ = true;
}

void App::drawSaveChangesDialog() {
    if (showSaveChangesDialog_) {
        ImGui::OpenPopup("###savechanges");
        showSaveChangesDialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            (std::string(tr(Str::SaveChangesTitle)) + "###savechanges").c_str(), nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (pendingClose_ < 0 || pendingClose_ >= static_cast<int>(documents_.size())) {
        pendingClose_ = -1;
        pendingQuit_ = false;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const Document& target = documents_[static_cast<size_t>(pendingClose_)];
    ImGui::Text(tr(Str::SaveChangesText), target.title().c_str());
    ImGui::Separator();

    // Nach dem Speichern oder Verwerfen geht es weiter: beim Beenden mit dem
    // naechsten geaenderten Reiter, sonst ist der Fall erledigt.
    const auto proceed = [this]() {
        const bool quitting = pendingQuit_;
        closeDocument(pendingClose_);
        pendingClose_ = -1;
        pendingQuit_ = false;
        ImGui::CloseCurrentPopup();
        if (quitting) requestQuit();
    };

    if (ImGui::Button(tr(Str::SaveChangesYes), ImVec2(130, 0))) {
        activateDocument(pendingClose_);
        if (cmdSave()) {
            proceed();
        }
        // Abgebrochen oder fehlgeschlagen: nichts schliessen, Frage bleibt.
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(Str::SaveChangesNo), ImVec2(130, 0))) {
        documents_[static_cast<size_t>(pendingClose_)].dirty = false;
        proceed();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(Str::MsgCancel), ImVec2(130, 0)) ||
        ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        pendingClose_ = -1;
        pendingQuit_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// --- Ja/Nein ---------------------------------------------------------------

void App::askConfirm(std::string text, std::function<void()> yes) {
    confirmText_ = std::move(text);
    confirmYes_ = std::move(yes);
    showConfirm_ = true;
}

void App::drawConfirmDialog() {
    if (showConfirm_) {
        ImGui::OpenPopup("###confirm");
        showConfirm_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 28.0f, 0.0f), ImGuiCond_Always);
    if (!ImGui::BeginPopupModal((std::string(tr(Str::AppTitle)) + "###confirm").c_str(), nullptr,
                                ImGuiWindowFlags_NoResize)) {
        return;
    }
    ImGui::TextWrapped("%s", confirmText_.c_str());
    ImGui::Separator();
    if (ImGui::Button(tr(Str::MsgYes), ImVec2(120, 0))) {
        if (confirmYes_) confirmYes_();
        confirmYes_ = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(Str::MsgNo), ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        confirmYes_ = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// --- Segmente ---------------------------------------------------------------

void App::cmdAddSegment(PrimitiveType type) {
    if (doc().effect.primitives.size() >= kMaxSegments) {
        diag::warn("segment limit reached (" + std::to_string(kMaxSegments) + ")");
        return;
    }
    doc().segmentEnabled.resize(doc().effect.primitives.size(), true);
    doc().effect.primitives.push_back(freshPrimitive(type));
    doc().segmentEnabled.push_back(true);
    doc().selectedPrimitive = static_cast<int>(doc().effect.primitives.size()) - 1;
    recordChange(tr(Str::UndoNewSegment));
    refreshPreview();
}

bool App::canCloneSegment() const {
    return hasSelection() && doc().effect.primitives.size() < kMaxSegments;
}

void App::cmdCloneSegment() {
    if (!canCloneSegment()) return;
    const size_t at = static_cast<size_t>(doc().selectedPrimitive);
    Primitive copy = doc().effect.primitives[at];
    // Wie im Original: "Copy of <Name>", ans Ende der Liste, ausgewaehlt.
    // Der Parser nimmt hoechstens 31 Zeichen (CPrimitiveTemplate::mName[32]);
    // das Original schnitt hier ebenfalls ab.
    std::string name = std::string(tr(Str::ListCopyOf)) + " " + displayName(static_cast<int>(at));
    if (name.size() > 31) name.resize(31);
    copy.name = name;
    doc().segmentEnabled.resize(doc().effect.primitives.size(), true);
    const bool wasOn = doc().segmentEnabled[at];
    doc().effect.primitives.push_back(std::move(copy));
    doc().segmentEnabled.push_back(wasOn);
    doc().selectedPrimitive = static_cast<int>(doc().effect.primitives.size()) - 1;
    recordChange(tr(Str::UndoCloneSegment));
    refreshPreview();
}

void App::cmdDeleteSegment() {
    if (!hasSelection()) return;
    const size_t at = static_cast<size_t>(doc().selectedPrimitive);
    doc().effect.primitives.erase(doc().effect.primitives.begin() +
                                  static_cast<std::ptrdiff_t>(at));
    // Nur das eine Haekchen entfernen. Vorher wurden alle zurueck auf "an"
    // gesetzt — wer drei Segmente zum Vergleichen abgeschaltet hatte, sah
    // nach dem Loeschen eines vierten wieder alle.
    if (at < doc().segmentEnabled.size()) {
        doc().segmentEnabled.erase(doc().segmentEnabled.begin() +
                                   static_cast<std::ptrdiff_t>(at));
    }
    doc().segmentEnabled.resize(doc().effect.primitives.size(), true);
    if (doc().selectedPrimitive >= static_cast<int>(doc().effect.primitives.size())) {
        doc().selectedPrimitive = static_cast<int>(doc().effect.primitives.size()) - 1;
    }
    recordChange(tr(Str::UndoDeleteSegment));
    refreshPreview();
}

void App::cmdToggleSegmentEnabled() {
    if (!hasSelection()) return;
    doc().segmentEnabled.resize(doc().effect.primitives.size(), true);
    const size_t at = static_cast<size_t>(doc().selectedPrimitive);
    doc().segmentEnabled[at] = !doc().segmentEnabled[at];
    refreshPreview();
}

}  // namespace efx::gui
