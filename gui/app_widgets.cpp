// Wiederverwendbare Eingabeelemente fuer die Eigenschaftsseiten.
//
// Sie kennen weder Effekt noch Renderer — sie bearbeiten einen Wert und
// melden, ob er sich geaendert hat. Deshalb stehen sie fuer sich: wer eine
// neue Eigenschaftsseite baut, braucht nur diese Datei zu lesen.
//
// Teil der Klasse App aus app.h — dieselbe Klasse, nach Aufgaben auf
// mehrere Dateien verteilt. app.cpp war mit 3524 Zeilen und einem Dutzend
// Zustaendigkeiten die Stelle, an der ein Leser aufgibt.
#include "app.h"
#include "efx/i18n.h"
#include "testmarke.h"

#include <algorithm>
#include <cstdio>
#include <string>
namespace efx::gui {

using i18n::Str;
using i18n::tr;

namespace {

// Spaltenmasse der Eigenschaftsseite — aus der Schriftgroesse, nicht in
// festen Punkten. Mit festen 140/146/220 Punkten lief eine Zeile bei 150 %
// Windows-Skalierung ueber den rechten Rand hinaus: das "~"-Haekchen war
// abgeschnitten und nicht mehr anzuklicken (gefunden vom Selbsttest).
float labelColumn() { return ImGui::GetFontSize() * 7.5f; }

float tildeWidth() {
    const ImGuiStyle& style = ImGui::GetStyle();
    return ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize("~").x;
}

// Breite fuer die Zahlenfelder rechts der Beschriftung, abzueglich "~".
float fieldsWidth() {
    const float avail = ImGui::GetContentRegionAvail().x - tildeWidth() -
                        ImGui::GetStyle().ItemSpacing.x;
    return std::max(avail, ImGui::GetFontSize() * 4.0f);
}

void labelCell(const char* label) {
    if (label && label[0]) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(labelColumn());
    }
}

// Die Uebergangsart eines Kanals. Steht hier, weil nur die Eingabeelemente
// sie brauchen — vorher lag sie in app.cpp und war von dort aus nicht
// erkennbar, wozu sie gehoert.
bool editCurveFlags(int& flags, std::vector<std::string>& words) {
    bool changed = false;

    int parmType = 0;  // 0 keine, 1 nonlinear, 2 wave, 3 clamp
    const int bits = flags & efx::kCurveClamp;
    if (bits == efx::kCurveClamp) parmType = 3;
    else if (bits == efx::kCurveWave) parmType = 2;
    else if (bits == efx::kCurveNonLinear) parmType = 1;

    const char* names[] = {efx::i18n::tr(Str::CurveNone), "nonlinear", "wave", "clamp"};
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
    const bool comboOffen = ImGui::BeginCombo("##parmType", names[parmType]);
    testmarke::marke("kurve");
    if (comboOffen) {
        for (int i = 0; i < 4; ++i) {
            if (ImGui::Selectable(names[i], parmType == i)) {
                flags &= ~efx::kCurveClamp;
                if (i == 1) flags |= efx::kCurveNonLinear;
                else if (i == 2) flags |= efx::kCurveWave;
                else if (i == 3) flags |= efx::kCurveClamp;
                words.clear();  // die Woerter stimmen nicht mehr
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    bool linear = (flags & efx::kCurveLinear) != 0;
    const bool linearGeklickt = ImGui::Checkbox(tr(Str::CurveLinear), &linear);
    testmarke::marke("linear");
    if (linearGeklickt) {
        flags = linear ? (flags | efx::kCurveLinear) : (flags & ~efx::kCurveLinear);
        words.clear();
        changed = true;
    }
    // "random" nur dann in dieselbe Zeile, wenn es ganz hineinpasst. Bei
    // schmaler Eigenschaftsseite ragte es sonst rechts hinaus und war nicht
    // mehr anzuklicken (Selbsttest, alle Typen mit Size-Kurve).
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float need = style.ItemSpacing.x + ImGui::GetFrameHeight() +
                           style.ItemInnerSpacing.x + ImGui::CalcTextSize(tr(Str::CurveRandom)).x;
        const float right = ImGui::GetItemRectMax().x;
        if (right + need <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x) {
            ImGui::SameLine();
        }
    }
    bool random = (flags & efx::kCurveRandom) != 0 &&
                  (flags & efx::kCurveClamp) != efx::kCurveClamp;
    const bool randomGeklickt = ImGui::Checkbox(tr(Str::CurveRandom), &random);
    testmarke::marke("random");
    if (randomGeklickt) {
        flags = random ? (flags | efx::kCurveRandom) : (flags & ~efx::kCurveRandom);
        words.clear();
        changed = true;
    }
    return changed;
}

}  // namespace

bool App::beginGroup(const char* id, const char* label, bool* enabled) {
    ImGui::PushID(id);

    // Ein Rahmen mit Titel. ImGui hat dafuer nichts Fertiges, also gezeichnet:
    // ein umrandetes Kindfenster, und der Titel wird darueber gesetzt.
    const float titleHeight = ImGui::GetTextLineHeight();
    ImGui::Dummy(ImVec2(0.0f, titleHeight * 0.35f));

    const ImVec2 titlePos = ImGui::GetCursorScreenPos();
    bool open = true;
    if (enabled) {
        ImGui::Checkbox(label, enabled);
        testmarke::marke("an");
        open = *enabled;
    } else {
        ImGui::TextUnformatted(label);
    }
    (void)titlePos;

    ImGui::BeginGroup();
    ImGui::Indent(ImGui::GetStyle().IndentSpacing * 0.6f);
    groupDisabledStack_.push_back(!open);
    if (!open) ImGui::BeginDisabled();
    return open;
}

void App::endGroup() {
    // Der Rahmen wird nachtraeglich um den Bereich gezeichnet, den die Gruppe
    // tatsaechlich eingenommen hat — vorher weiss man ihn nicht.
    if (!groupDisabledStack_.empty()) {
        if (groupDisabledStack_.back()) ImGui::EndDisabled();
        groupDisabledStack_.pop_back();
    }
    ImGui::Unindent(ImGui::GetStyle().IndentSpacing * 0.6f);
    ImGui::EndGroup();

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImU32 colour = ImGui::GetColorU32(ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x - 6.0f, min.y - 2.0f),
                                        ImVec2(min.x - 6.0f, max.y + 2.0f), colour);
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

bool App::editFlags(Primitive& p) {
    bool changed = false;

    auto group = [&](const char* label, uint32_t& bits,
                     const std::vector<FlagName>& table) {
        ImGui::SeparatorText(label);
        for (const auto& entry : table) {
            if (entry.bits == 0) continue;
            bool on = (bits & entry.bits) == entry.bits;
            if (ImGui::Checkbox(entry.name, &on)) {
                bits = on ? (bits | entry.bits) : (bits & ~entry.bits);
                changed = true;
            }
            // Flags, die nur ein Zweig kennt, werden benannt statt versteckt:
            // wer eine fremde Datei oeffnet, soll sehen, was drinsteht.
            if (entry.dialect != Dialect::Both) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", dialectName(entry.dialect));
            }
            if (entry.note && entry.note[0] && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", entry.note);
            }
        }
    };

    group(tr(Str::FlagsGroup), p.flags, flagNames());
    group(tr(Str::SpawnFlagsGroup), p.spawnFlags, spawnFlagNames());
    return changed;
}


bool App::editRange(const char* id, const char* label, Range& range, float speed,
                    float low, float high) {
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    bool changed = false;

    // Kein Ankreuzfeld mehr vor jedem einzelnen Feld.
    //
    // Das Dateiformat kennt gesetzte und nicht gesetzte Felder, und ein nicht
    // gesetztes wird beim Speichern nicht geschrieben — das bleibt so. Aber
    // die Unterscheidung gehoert nicht vor jedes Feld: zwoelf Haekchen auf
    // einer Seite, und keines sagt, wozu es gehoert.
    //
    // Stattdessen: das Feld gilt als gesetzt, sobald jemand es anfasst. Wer es
    // wieder loswerden will, nimmt das Haekchen der Gruppe. Genau so verhaelt
    // sich der alte Editor.
    labelCell(label);
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    const float total = fieldsWidth();
    const float width = range.ranged ? (total - spacing) * 0.5f : total;
    ImGui::SetNextItemWidth(width);
    const bool minChanged = ImGui::DragFloat("##min", &range.min, speed, low, high, "%.4g");
    testmarke::marke("min");
    if (minChanged) {
        if (!range.ranged) range.max = range.min;
        range.set = true;
        changed = true;
    }
    if (range.ranged) {
        ImGui::SameLine(0.0f, spacing);
        ImGui::SetNextItemWidth(width);
        const bool maxChanged = ImGui::DragFloat("##max", &range.max, speed, low, high, "%.4g");
        testmarke::marke("max");
        if (maxChanged) {
            range.set = true;
            changed = true;
        }
    }

    ImGui::SameLine();
    bool ranged = range.ranged;
    const bool rangedClicked = ImGui::Checkbox("~", &ranged);
    testmarke::marke("ranged");
    if (rangedClicked) {
        range.ranged = ranged;
        if (!ranged) range.max = range.min;
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tr(Str::FieldRangedHint));
    }

    ImGui::PopID();
    return changed;
}

bool App::editVec3Range(const char* id, const char* label, Vec3Range& value,
                        float speed) {
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    bool changed = false;

    // Ein Vektor mit Spanne steht auf zwei Zeilen (Min oben, Max darunter).
    // Sechs Zahlen in einer Zeile passten in keine Eigenschaftsseite.
    labelCell(label);
    const float rowStart = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(fieldsWidth());
    const bool minChanged = ImGui::DragFloat3("##min", value.min.v, speed, 0.0f, 0.0f, "%.4g");
    testmarke::marke("min");
    if (minChanged) {
        if (!value.ranged) value.max = value.min;
        value.set = true;
        changed = true;
    }
    // Das "~" gleich hinter die erste Zeile; die zweite Zeile beginnt
    // unter dem ersten Feld.
    ImGui::SameLine();
    bool ranged = value.ranged;
    const bool rangedClicked = ImGui::Checkbox("~", &ranged);
    testmarke::marke("ranged");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::FieldRangedHint));
    if (value.ranged) {
        ImGui::SetCursorPosX(rowStart);
        ImGui::SetNextItemWidth(fieldsWidth());
        const bool maxChanged = ImGui::DragFloat3("##max", value.max.v, speed, 0.0f, 0.0f, "%.4g");
        testmarke::marke("max");
        if (maxChanged) {
            value.set = true;
            changed = true;
        }
    }

    if (rangedClicked) {
        value.ranged = ranged;
        if (!ranged) value.max = value.min;
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

bool App::editChannel(const char* id, const char* label, Channel& channel,
                      float speed) {
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    bool changed = false;

    bool present = channel.present;
    if (beginGroup("grp", label, &present)) {
        changed |= editRange("start", tr(Str::FieldStart), channel.start, speed);
        changed |= editRange("end", tr(Str::FieldEnd), channel.end, speed);
        changed |= editRange("parm", tr(Str::FieldParm), channel.parm, 1.0f);
        changed |= editCurveFlags(channel.curveFlags, channel.curveWords);
    }
    endGroup();
    if (present != channel.present) {
        channel.present = present;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool App::editStringList(const char* id, const char* label,
                         std::vector<std::string>& list, const char* hint, float width,
                         const std::function<std::string(const std::string&)>& describe,
                         const std::function<void(const std::string&)>& play) {
    // Wie im Original (Listenfeld 1009 mit Ordner 1010 und X 1012, bei Sound
    // dazu der Lautsprecher 1013): links die Liste, rechts die Knoepfe.
    // Vorher waren es Textfelder mit "Hinzufuegen" und "..." darunter.
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    bool changed = false;
    if (label && *label) ImGui::TextUnformatted(label);

    const ImGuiStyle& style = ImGui::GetStyle();
    const float button = ImGui::GetFrameHeight();
    if (width <= 0.0f) width = ImGui::GetContentRegionAvail().x;
    const float boxWidth = std::max(button * 3.0f, width - button - style.ItemSpacing.x);
    const float boxHeight = ImGui::GetTextLineHeightWithSpacing() * 5.0f + style.FramePadding.y * 2.0f;

    int& selected = listSelection_[&list];
    if (selected >= static_cast<int>(list.size())) selected = -1;
    const bool editingThis = listEditing_ == &list;

    // Kein eigenes Unterfenster: das liesse sich nicht mit der Seite rollen
    // (Selbsttest, 2.10.2026). Ein gezeichneter Rahmen, der mit den
    // Eintraegen waechst — mindestens fuenf Zeilen hoch wie im Original.
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float inner = style.FramePadding.y;
    const float rowsHeight = line * static_cast<float>(list.size() + 1) + inner * 2.0f;
    const ImVec2 boxMin = ImGui::GetCursorScreenPos();
    const ImVec2 boxMax(boxMin.x + boxWidth, boxMin.y + std::max(boxHeight, rowsHeight));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(boxMin, boxMax, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
    draw->AddRect(boxMin, boxMax, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);
    ImGui::BeginGroup();
    const float rowWidth = boxWidth - style.FramePadding.x * 2.0f;
    ImGui::SetCursorScreenPos(ImVec2(boxMin.x + style.FramePadding.x, boxMin.y + inner));
    int removeAt = -1;
    for (size_t i = 0; i < list.size(); ++i) {
        ImGui::SetCursorScreenPos(ImVec2(boxMin.x + style.FramePadding.x,
                                         boxMin.y + inner + line * static_cast<float>(i)));
        ImGui::PushID(static_cast<int>(i));
        const std::string mark = "eintrag" + std::to_string(i);
        if (editingThis && listEditRow_ == static_cast<int>(i)) {
            // Eintippen (efxed-Zusatz: das Original kennt nur die Auswahl).
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%s", list[i].c_str());
            if (listEditFocus_) {
                ImGui::SetKeyboardFocusHere();
                listEditFocus_ = false;
            }
            ImGui::SetNextItemWidth(rowWidth);
            if (ImGui::InputText("##entry", buffer, sizeof(buffer))) {
                list[i] = buffer;
                changed = true;
            }
            testmarke::marke(mark.c_str());
            if (ImGui::IsItemDeactivated()) {
                listEditing_ = nullptr;
                listEditRow_ = -1;
            }
        } else {
            const bool isSelected = selected == static_cast<int>(i);
            if (ImGui::Selectable(list[i].empty() ? "-" : list[i].c_str(), isSelected,
                                  ImGuiSelectableFlags_AllowDoubleClick, ImVec2(rowWidth, 0.0f))) {
                selected = static_cast<int>(i);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    listEditing_ = &list;
                    listEditRow_ = static_cast<int>(i);
                    listEditFocus_ = true;
                }
            }
            testmarke::marke(mark.c_str());
            if (describe && ImGui::IsItemHovered()) {
                const std::string text = describe(list[i]);
                if (!text.empty()) ImGui::SetTooltip("%s", text.c_str());
            }
            if (isSelected && ImGui::IsItemFocused() &&
                ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                removeAt = static_cast<int>(i);
            }
        }
        ImGui::PopID();
    }
    const float freeTop = boxMin.y + inner + line * static_cast<float>(list.size());
    if (list.empty()) {
        // Eine leere Liste ist der Absturzfall des alten Editors und laesst
        // die Primitive im Spiel unsichtbar — deshalb steht es hier.
        ImGui::SetCursorScreenPos(ImVec2(boxMin.x + style.FramePadding.x, freeTop));
        // Fensterkoordinaten, nicht Bildschirm (PushTextWrapPos).
        ImGui::PushTextWrapPos(boxMin.x + boxWidth - style.FramePadding.x - ImGui::GetWindowPos().x +
                               ImGui::GetScrollX());
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", tr(Str::FieldEmptyList));
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    }
    // Die freie Flaeche darunter: Doppelklick legt einen Eintrag zum
    // Eintippen an.
    {
        ImGui::SetCursorScreenPos(ImVec2(boxMin.x + 1.0f, freeTop));
        const ImVec2 rest(boxWidth - 2.0f, boxMax.y - freeTop - 1.0f);
        ImGui::InvisibleButton("##frei", ImVec2(std::max(1.0f, rest.x), std::max(4.0f, rest.y)));
        testmarke::marke("dazu");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tr(Str::ListTypeHint));
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                list.emplace_back(hint ? hint : "");
                selected = static_cast<int>(list.size()) - 1;
                listEditing_ = &list;
                listEditRow_ = selected;
                listEditFocus_ = true;
                changed = true;
            }
        }
    }
    ImGui::SetCursorScreenPos(boxMin);
    ImGui::Dummy(ImVec2(boxWidth, boxMax.y - boxMin.y));
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::BeginGroup();
    {
        // Ordner: Auswahl aus dem Materialbestand.
        const std::string listId = id;
        const bool effectList = listId == "deathfx" || listId == "impactfx" ||
                                listId == "emitfx" || listId == "playfx";
        if (iconButton("##choose", Icon::Open, tr(Str::ListChoose), false, button)) {
            pickerTarget_ = &list;
            pickerKind_ = listId == "models"   ? PickerKind::Models
                          : listId == "sounds" ? PickerKind::Sounds
                          : effectList         ? PickerKind::Effects
                                               : PickerKind::Shaders;
            showPicker_ = true;
        }
        testmarke::marke("waehlen");
        // X: den markierten Eintrag entfernen, ohne Rueckfrage, gesperrt ohne
        // Markierung — wie im Original.
        ImGui::BeginDisabled(selected < 0);
        if (iconButton("##remove", Icon::DeleteSegment, tr(Str::FieldRemoveEntry), false, button)) {
            removeAt = selected;
        }
        testmarke::marke("weg");
        ImGui::EndDisabled();
        if (play) {
            ImGui::BeginDisabled(list.empty());
            if (iconButton("##play", Icon::Speaker, tr(Str::SoundPlay), false, button) && !list.empty()) {
                play(list[static_cast<size_t>(selected >= 0 ? selected : 0)]);
            }
            testmarke::marke("spielen");
            ImGui::EndDisabled();
        }
    }
    ImGui::EndGroup();

    if (removeAt >= 0 && removeAt < static_cast<int>(list.size())) {
        list.erase(list.begin() + removeAt);
        if (editingThis) {
            listEditing_ = nullptr;
            listEditRow_ = -1;
        }
        selected = list.empty() ? -1 : std::min(removeAt, static_cast<int>(list.size()) - 1);
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

}  // namespace efx::gui
