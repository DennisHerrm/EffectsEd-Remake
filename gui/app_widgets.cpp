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

bool App::editColorChannel(ColorChannel& channel) {
    ImGui::PushID("rgb");
    testmarke::Bereich bereich("rgb");
    bool changed = false;

    bool present = channel.present;
    if (beginGroup("grp", tr(Str::FieldRgb), &present)) {
        // Zusaetzlich ein Farbfeld: eine Farbe als drei Zahlen zu lesen ist
        // moeglich, aber niemand tut es gern.
        auto colourRow = [&](const char* id, const char* label, Vec3Range& value) {
            ImGui::PushID(id);
            testmarke::Bereich zeile(id);

            // ZWEI Farbfelder, wenn eine Spanne eingestellt ist — eines fuer
            // den kleinsten, eines fuer den groessten Wert.
            //
            // Das Original zeigt genau das: unter "Start Color" stehen zwei
            // Farbknoepfe nebeneinander, unter "End Color" ebenso. Wir hatten
            // nur einen, naemlich fuer den kleinsten Wert — bei einer Spanne
            // von Weiss nach Gelb sah man das Gelb nirgends und musste es aus
            // drei Zahlen zusammenreimen.
            auto swatch = [&](const char* which, Vec3& target) {
                ImGui::PushID(which);
                float colour[3] = {target[0], target[1], target[2]};
                const bool swatchChanged = ImGui::ColorEdit3(
                    "##swatch", colour,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                testmarke::marke(which);
                if (swatchChanged) {
                    for (int i = 0; i < 3; ++i) target[i] = colour[i];
                    value.set = true;
                    changed = true;
                }
                ImGui::PopID();
            };

            swatch("min", value.min);
            if (value.ranged) {
                ImGui::SameLine(0.0f, 2.0f);
                swatch("max", value.max);
            } else {
                // Ohne Spanne bleiben beide gleich, sonst schreibt die Datei
                // spaeter zwei verschiedene Werte, von denen man einen nie
                // gesehen hat.
                value.max = value.min;
            }
            ImGui::SameLine();
            ImGui::PopID();
            return editVec3Range("wert", label, value, 0.01f);
        };
        changed |= colourRow("start", tr(Str::FieldStart), channel.start);
        changed |= colourRow("end", tr(Str::FieldEnd), channel.end);
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
                         std::vector<std::string>& list, const char* hint) {
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    bool changed = false;

    ImGui::TextUnformatted(label);
    ImGui::Indent();

    int removeAt = -1;
    for (size_t i = 0; i < list.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s", list[i].c_str());
        ImGui::SetNextItemWidth(-90.0f);
        const bool entryChanged = ImGui::InputText("##entry", buffer, sizeof(buffer));
        testmarke::marke(("eintrag" + std::to_string(i)).c_str());
        if (entryChanged) {
            list[i] = buffer;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeAt = static_cast<int>(i);
        testmarke::marke(("weg" + std::to_string(i)).c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::FieldRemoveEntry));
        ImGui::PopID();
    }
    if (removeAt >= 0) {
        list.erase(list.begin() + removeAt);
        changed = true;
    }

    const bool addClicked = ImGui::SmallButton(tr(Str::FieldAddEntry));
    testmarke::marke("dazu");
    if (addClicked) {
        list.emplace_back(hint ? hint : "");
        changed = true;
    }
    // Auswahl aus dem Materialbestand — im Original der Ordnerknopf neben der
    // Liste. Die Liste selbst fuellt erst der Materialsuchlauf.
    if (std::string(id) == "shaders" || std::string(id) == "models" ||
        std::string(id) == "sounds") {
        ImGui::SameLine();
        const bool chooseClicked = ImGui::SmallButton("...");
        testmarke::marke("waehlen");
        if (chooseClicked) {
            pickerTarget_ = &list;
            pickerKind_ = std::string(id) == "models"  ? PickerKind::Models
                          : std::string(id) == "sounds" ? PickerKind::Sounds
                                                        : PickerKind::Shaders;
            showPicker_ = true;
        }
    }
    if (list.empty()) {
        ImGui::SameLine();
        // Eine leere Liste ist der Absturzfall des alten Editors und laesst
        // die Primitive im Spiel unsichtbar. Deshalb steht es hier und nicht
        // erst in der Pruefliste.
        ImGui::TextDisabled("%s", tr(Str::FieldEmptyList));
    }

    ImGui::Unindent();
    ImGui::PopID();
    return changed;
}

}  // namespace efx::gui
