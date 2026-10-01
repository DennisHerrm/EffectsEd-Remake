// Die Eigenschaftsseite des gewaehlten Segments.
//
// Aufbau wie im Original, Reiter fuer Reiter und Zeile fuer Zeile. Der Bauplan
// stammt aus den Dialogressourcen von EffectsEd.exe und aus Messungen am
// laufenden Original (scratchpad agentA, 1.10.2026):
//
//   - Oben die Spaltenkoepfe "Min" und "Max".
//   - Jede Zahl als Paar Min/Max nebeneinander — auch wenn beide gleich sind.
//     Vorher gab es je Zeile ein Feld und ein "~"-Haekchen fuer die Spanne,
//     und Vektoren standen als drei Zahlen nebeneinander in einer Zeile.
//   - Vektoren als drei Zeilen Forward/Right/Up (bzw. Pitch/Yaw/Roll, X/Y/Z).
//   - Kurven als Gruppe mit Start, End, "Transition" (acht Eintraege wie im
//     Original), "Parameter" und "Apply random factor".
//   - Gruppenrahmen mit Titel, manche mit Haekchen im Titel.
//
// Was wir anders machen als das Original, und warum:
//   - Keine Apply-Taste: jede Aenderung gilt sofort, die Vorschau laeuft mit,
//     und Rueckgaengig nimmt sie zurueck. Das Original verlangte Apply oder
//     Enter und fragte nach, wenn man es vergass.
//   - Min bleibt immer <= Max. Das Original liess "life 900 100" und
//     "radius 95 60" zu und schrieb es so in die Datei (Bildschirmfotos des
//     Anwenders, "CameraShake ... Bugged").
//   - Nicht gesetzte Werte zeigen den Vorgabewert der Engine (grau) statt 0.
//     Er steht nicht in der Datei, genau wie im Original.
//   - Versteckte Haekchen bleiben nicht haengen: das Original schrieb
//     "depthHack" in ein Sound-Segment, weil das (dort unsichtbare) Haekchen
//     vom vorigen Segment stehen geblieben war.
//   - Der Name steht nicht hier, sondern wird in der Segmentliste umbenannt
//     (Doppelklick oder F2) — wie im Original.
#include "app.h"
#include "efx/diag.h"
#include "efx/i18n.h"
#include "testmarke.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "app_shared.h"

namespace efx::gui {

using i18n::Str;
using i18n::tr;

namespace {

// --- Spalten ----------------------------------------------------------------
//
// Alle Zeilen einer Seite benutzen dieselben drei Spalten — auch innerhalb
// von Gruppenrahmen. Im Original stehen die Min-Felder bei x=102, die
// Max-Felder bei x=174, durch die ganze Seite.
struct Page {
    float x0 = 0.0f;      // linker Rand der Seite (Bildschirm)
    float label = 0.0f;   // Breite der Beschriftungsspalte
    float field = 0.0f;   // Breite eines Zahlenfelds
    float gap = 0.0f;
    float right() const { return x0 + label + field * 2.0f + gap; }
};
Page g_page;

void beginPage() {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float em = ImGui::GetFontSize();
    g_page.x0 = ImGui::GetCursorScreenPos().x;
    g_page.gap = ImGui::GetStyle().ItemSpacing.x;
    g_page.label = std::clamp(avail * 0.40f, em * 6.0f, em * 10.0f);
    g_page.field = std::max(em * 3.5f, (avail - g_page.label - g_page.gap * 2.0f) * 0.5f);
}

float minX() { return g_page.x0 + g_page.label + g_page.gap; }
float maxX() { return minX() + g_page.field + g_page.gap; }

// Beschriftung rechtsbuendig vor der Min-Spalte, wie im Original.
void labelCell(const char* text) {
    const float y = ImGui::GetCursorScreenPos().y;
    const float width = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorScreenPos(ImVec2(std::max(ImGui::GetCursorScreenPos().x,
                                              minX() - g_page.gap - width),
                                     y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    ImGui::SameLine();
}

// Die Spaltenkoepfe "Min" und "Max".
void minMaxHeader() {
    const float y = ImGui::GetCursorScreenPos().y;
    const auto centred = [](const char* text, float x, float width, float top) {
        ImGui::SetCursorScreenPos(ImVec2(x + (width - ImGui::CalcTextSize(text).x) * 0.5f, top));
        ImGui::TextDisabled("%s", text);
    };
    centred(tr(Str::FieldMin), minX(), g_page.field, y);
    ImGui::SameLine();
    centred(tr(Str::FieldMax), maxX(), g_page.field, y);
}

// --- Gruppenrahmen ------------------------------------------------------------
struct Frame {
    ImVec2 start;
    float titleWidth = 0.0f;
};
std::vector<Frame> g_frames;

// Titel oben links im Rahmen; mit `check` als Haekchen (dann gesperrt, wenn aus).
bool frameBegin(const char* id, const char* title, bool* check = nullptr,
                bool* clicked = nullptr) {
    ImGui::PushID(id);
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight() * 0.25f));
    Frame frame;
    frame.start = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(frame.start.x + ImGui::GetFontSize() * 0.6f, frame.start.y));
    bool open = true;
    if (check != nullptr) {
        const bool changed = ImGui::Checkbox(title, check);
        testmarke::marke("an");
        if (clicked) *clicked = changed;
        open = *check;
    } else {
        ImGui::TextUnformatted(title);
    }
    frame.titleWidth = ImGui::GetItemRectSize().x;
    g_frames.push_back(frame);
    ImGui::Indent(ImGui::GetFontSize() * 0.6f);
    if (!open) ImGui::BeginDisabled();
    ImGui::PushID("body");
    return open;
}

void frameEnd(bool wasOpen) {
    ImGui::PopID();
    if (!wasOpen) ImGui::EndDisabled();
    ImGui::Unindent(ImGui::GetFontSize() * 0.6f);
    const Frame frame = g_frames.back();
    g_frames.pop_back();
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
    const float top = frame.start.y + ImGui::GetTextLineHeight() * 0.5f;
    const float bottom = ImGui::GetCursorScreenPos().y - ImGui::GetStyle().ItemSpacing.y * 0.5f;
    const float left = frame.start.x;
    const float right = std::max(g_page.right(), left + frame.titleWidth + ImGui::GetFontSize());
    const ImU32 colour = ImGui::GetColorU32(ImGuiCol_Border);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float titleLeft = left + ImGui::GetFontSize() * 0.45f;
    const float titleRight = titleLeft + frame.titleWidth + ImGui::GetFontSize() * 0.3f;
    draw->AddLine(ImVec2(left, top), ImVec2(titleLeft, top), colour);
    draw->AddLine(ImVec2(titleRight, top), ImVec2(right, top), colour);
    draw->AddLine(ImVec2(left, top), ImVec2(left, bottom), colour);
    draw->AddLine(ImVec2(right, top), ImVec2(right, bottom), colour);
    draw->AddLine(ImVec2(left, bottom), ImVec2(right, bottom), colour);
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y * 0.5f));
}

// --- Zahlenzeilen -----------------------------------------------------------
//
// Ein Wertepaar. `fallback`: was die Engine nimmt, wenn nichts in der Datei
// steht (FxTemplate.cpp, CPrimitiveTemplate::CPrimitiveTemplate).
bool pairFields(float& low, float& high, bool set, float speed, float lo, float hi,
                const char* format) {
    bool changed = false;
    if (!set) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
    ImGui::SetNextItemWidth(g_page.field);
    float a = low;
    const bool minChanged = ImGui::DragFloat("##min", &a, speed, lo, hi, format);
    testmarke::marke("min");
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SetCursorScreenPos(ImVec2(maxX(), ImGui::GetCursorScreenPos().y));
    ImGui::SetNextItemWidth(g_page.field);
    float b = high;
    const bool maxChanged = ImGui::DragFloat("##max", &b, speed, lo, hi, format);
    testmarke::marke("max");
    if (!set) ImGui::PopStyleColor();
    // Min bleibt <= Max: wer Min ueber Max zieht, nimmt Max mit, und
    // umgekehrt. Das Original schrieb "life 900 100" in die Datei.
    if (minChanged) {
        low = a;
        if (high < low) high = low;
        changed = true;
    }
    if (maxChanged) {
        high = b;
        if (low > high) low = high;
        changed = true;
    }
    return changed;
}

bool rowRange(const char* id, const char* label, Range& r, float fallback, float speed = 1.0f,
              float lo = 0.0f, float hi = 0.0f, const char* format = "%.4g") {
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    labelCell(label);
    float low = r.set ? r.min : fallback;
    float high = r.set ? r.max : fallback;
    const bool changed = pairFields(low, high, r.set, speed, lo, hi, format);
    if (changed) {
        r.min = low;
        r.max = high;
        r.set = true;
        r.ranged = low != high;
    }
    ImGui::PopID();
    return changed;
}

// Ein Vektor als drei Zeilen. Die Datei kennt "x y z" (eine Zahl je Achse)
// oder "x y z X Y Z" (Spanne) — welches, folgt aus den Werten.
bool rowsVector(const char* id, Vec3Range& v, Str a, Str b, Str c, float fallback = 0.0f) {
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    bool changed = false;
    const Str labels[3] = {a, b, c};
    const char* axes[3] = {"x", "y", "z"};
    for (int k = 0; k < 3; ++k) {
        ImGui::PushID(k);
        testmarke::Bereich achse(axes[k]);
        labelCell(tr(labels[k]));
        float low = v.set ? v.min[k] : fallback;
        float high = v.set ? (v.ranged ? v.max[k] : v.min[k]) : fallback;
        if (pairFields(low, high, v.set, 1.0f, 0.0f, 0.0f, "%.4g")) {
            if (!v.set) {
                for (int j = 0; j < 3; ++j) v.min[j] = v.max[j] = fallback;
            }
            if (!v.ranged) v.max = v.min;
            v.min[k] = low;
            v.max[k] = high;
            v.set = true;
            v.ranged = v.min[0] != v.max[0] || v.min[1] != v.max[1] || v.min[2] != v.max[2];
            changed = true;
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}

// Eine Bitmaske als Haekchen. `invert`: das Haekchen zeigt das Gegenteil des
// Bits ("Relative to effect axis" = nicht cheapOrgCalc).
bool flagBox(const char* label, uint32_t& bits, uint32_t mask, bool invert = false,
             const char* mark = nullptr) {
    bool on = ((bits & mask) != 0) != invert;
    const bool clicked = ImGui::Checkbox(label, &on);
    if (mark) testmarke::marke(mark);
    if (clicked) {
        const bool setBit = on != invert;
        bits = setBit ? (bits | mask) : (bits & ~mask);
    }
    return clicked;
}

// --- Kurven -------------------------------------------------------------------
//
// Die acht Uebergaenge des Originals (DLGINIT der Combos 1067/1068/1069/1101/
// 1171). Clamp ist in der Datei nonlinear|wave (0xC), random ein eigenes Bit
// — "Apply random factor".
struct Transition {
    Str label;
    int bits;
};
const Transition kTransitions[] = {
    {Str::TransConstant, 0},
    {Str::TransLinear, kCurveLinear},
    {Str::TransNonlinear, kCurveNonLinear},
    {Str::TransNonlinearLinear, kCurveNonLinear | kCurveLinear},
    {Str::TransWave, kCurveWave},
    {Str::TransWaveLinear, kCurveWave | kCurveLinear},
    {Str::TransClamp, kCurveClamp},
    {Str::TransClampLinear, kCurveClamp | kCurveLinear},
};

int transitionIndex(int flags) {
    const int shape = flags & (kCurveClamp | kCurveLinear);
    for (int i = 0; i < 8; ++i) {
        if (kTransitions[i].bits == shape) return i;
    }
    return 0;
}

bool transitionRow(int& flags, std::vector<std::string>& words) {
    bool changed = false;
    labelCell(tr(Str::FieldTransition));
    ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
    ImGui::SetNextItemWidth(g_page.field * 2.0f + g_page.gap);
    const int current = transitionIndex(flags);
    const bool open = ImGui::BeginCombo("##transition", tr(kTransitions[current].label));
    testmarke::marke("kurve");
    if (open) {
        for (int i = 0; i < 8; ++i) {
            if (ImGui::Selectable(tr(kTransitions[i].label), i == current) && i != current) {
                flags = (flags & ~(kCurveClamp | kCurveLinear)) | kTransitions[i].bits;
                // Die Woerter stimmen nicht mehr; der Schreiber leitet sie aus
                // den Bits neu ab.
                words.clear();
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool randomBox(int& flags, std::vector<std::string>& words) {
    ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
    bool on = (flags & kCurveRandom) != 0;
    const bool clicked = ImGui::Checkbox(tr(Str::CurveApplyRandom), &on);
    testmarke::marke("random");
    if (clicked) {
        flags = on ? (flags | kCurveRandom) : (flags & ~kCurveRandom);
        words.clear();
    }
    return clicked;
}

// Eine Kurvengruppe: Haekchen im Titel, Start, End, Transition, Parameter,
// Zufall. End ist nur bei einem Uebergang belegbar, Parameter nur bei
// nonlinear/wave/clamp — wie im Original.
bool channelGroup(const char* id, Str title, Channel& c, Str startLabel, Str endLabel,
                  float fallback, float speed, bool enabled = true) {
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    bool changed = false;
    if (!enabled) ImGui::BeginDisabled();
    bool present = c.present;
    bool toggled = false;
    const bool open = frameBegin("frame", tr(title), &present, &toggled);
    if (toggled) {
        c.present = present;
        changed = true;
    }
    changed |= rowRange("start", tr(startLabel), c.start, fallback, speed);
    const bool hasTransition = (c.curveFlags & (kCurveClamp | kCurveLinear)) != 0;
    ImGui::BeginDisabled(!hasTransition && (c.curveFlags & kCurveRandom) == 0);
    changed |= rowRange("end", tr(endLabel), c.end, fallback, speed);
    ImGui::EndDisabled();
    changed |= transitionRow(c.curveFlags, c.curveWords);
    ImGui::BeginDisabled((c.curveFlags & kCurveClamp) == 0);
    changed |= rowRange("parm", tr(Str::FieldParm), c.parm, 0.0f, 1.0f);
    ImGui::EndDisabled();
    changed |= randomBox(c.curveFlags, c.curveWords);
    frameEnd(open);
    if (!enabled) ImGui::EndDisabled();
    // Wer ein Feld anfasst, will die Gruppe auch in der Datei haben.
    if (changed && !toggled) c.present = true;
    ImGui::PopID();
    return changed;
}

// Engine-Vorgaben (FxTemplate.cpp, CPrimitiveTemplate-Konstruktor).
constexpr float kDefaultLife = 50.0f;
constexpr float kDefaultCount = 1.0f;
constexpr float kDefaultRadius = 10.0f;
constexpr float kDefaultCurve = 1.0f;
constexpr float kDefaultWind = 1.0f;
constexpr float kDefaultDensity = 10.0f;
constexpr float kDefaultVariance = 1.0f;

// Wo das Original "Always draw on top" zeigt (Ressourcen-Dump, sichtbar nur
// bei diesen Typen).
bool showsDepthHack(PrimitiveType t) {
    switch (t) {
        case PrimitiveType::Particle:
        case PrimitiveType::OrientedParticle:
        case PrimitiveType::Line:
        case PrimitiveType::Tail:
        case PrimitiveType::Cylinder:
        case PrimitiveType::Electricity:
            return true;
        default:
            return false;
    }
}

bool hasDeathFx(PrimitiveType t) {
    return fields::applies(t, fields::Field::DeathFx);
}

// Eine Farbe als Feld in Spaltenbreite; Klick oeffnet die Farbwahl. Im
// Original zwei Knoepfe "Choose..." je Zeile.
bool colourButton(const char* id, Vec3& colour, bool set) {
    bool changed = false;
    ImGui::PushID(id);
    ImVec4 shown(colour[0], colour[1], colour[2], 1.0f);
    if (!set) shown = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // Engine-Vorgabe: weiss
    if (ImGui::ColorButton("##swatch", shown, ImGuiColorEditFlags_NoAlpha,
                           ImVec2(g_page.field, ImGui::GetFrameHeight()))) {
        ImGui::OpenPopup("pick");
    }
    testmarke::marke(id);
    if (ImGui::BeginPopup("pick")) {
        float value[3] = {shown.x, shown.y, shown.z};
        if (ImGui::ColorPicker3("##picker", value,
                                ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_InputRGB)) {
            colour[0] = value[0];
            colour[1] = value[1];
            colour[2] = value[2];
            changed = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

bool colourRow(const char* id, Str label, Vec3Range& v) {
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    labelCell(tr(label));
    Vec3 low = v.set ? v.min : Vec3{{1.0f, 1.0f, 1.0f}};
    Vec3 high = v.set ? (v.ranged ? v.max : v.min) : Vec3{{1.0f, 1.0f, 1.0f}};
    ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
    const bool a = colourButton("farbe_min", low, v.set);
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SetCursorScreenPos(ImVec2(maxX(), ImGui::GetCursorScreenPos().y));
    const bool b = colourButton("farbe_max", high, v.set);
    if (a || b) {
        // Ohne Spanne folgt Max dem Min — so wie bei einer einzelnen Farbe
        // in der Datei ("start 1 0.5 0").
        if (a && !v.ranged) high = low;
        v.min = low;
        v.max = high;
        v.set = true;
        v.ranged = low[0] != high[0] || low[1] != high[1] || low[2] != high[2];
    }
    ImGui::PopID();
    return a || b;
}

}  // namespace

bool App::editColorChannel(ColorChannel& c) {
    ImGui::PushID("rgb");
    testmarke::Bereich bereich("rgb");
    bool changed = false;
    bool present = c.present;
    bool toggled = false;
    const bool open = frameBegin("frame", tr(Str::GroupRgbColor), &present, &toggled);
    if (toggled) {
        c.present = present;
        changed = true;
    }
    minMaxHeader();
    changed |= colourRow("start", Str::FieldStartColor, c.start);
    const bool hasTransition = (c.curveFlags & (kCurveClamp | kCurveLinear)) != 0;
    ImGui::BeginDisabled(!hasTransition && (c.curveFlags & kCurveRandom) == 0);
    changed |= colourRow("end", Str::FieldEndColor, c.end);
    ImGui::EndDisabled();
    changed |= transitionRow(c.curveFlags, c.curveWords);
    ImGui::BeginDisabled((c.curveFlags & kCurveClamp) == 0);
    changed |= rowRange("parm", tr(Str::FieldParm), c.parm, 0.0f, 1.0f);
    ImGui::EndDisabled();
    changed |= randomBox(c.curveFlags, c.curveWords);
    frameEnd(open);
    if (changed && !toggled) c.present = true;
    ImGui::PopID();
    return changed;
}

void App::drawProperties(float width, float height) {
    ImGui::BeginChild("properties", ImVec2(width, height), ImGuiChildFlags_Borders);

    if (doc().selectedPrimitive < 0 ||
        doc().selectedPrimitive >= static_cast<int>(doc().effect.primitives.size())) {
        // Wie im Original: mittig "No effect segment selected."
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const char* text = tr(Str::PropNoSelection);
        const ImVec2 size = ImGui::CalcTextSize(text);
        ImGui::SetCursorPos(ImVec2(std::max(0.0f, (avail.x - size.x) * 0.5f),
                                   std::max(0.0f, avail.y * 0.45f)));
        ImGui::TextDisabled("%s", text);
        ImGui::EndChild();
        return;
    }

    Primitive& p = doc().effect.primitives[static_cast<size_t>(doc().selectedPrimitive)];

    ImGui::TextUnformatted(typeName(p.type));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", typeDescription(p.type));
    // Was in der Datei steht, aber fuer diesen Typ nichts bewirkt — das
    // Original hat solche Felder still mitgeschleppt.
    const auto useless = fields::uselessFields(p);
    if (!useless.empty()) {
        const theme::Palette& palette = activeTheme(settings_).palette;
        std::string text = tr(Str::FieldNotUsed);
        text += ":";
        for (auto field : useless) {
            text += " ";
            text += fields::fieldName(field);
        }
        ImGui::TextColored(toImGui(palette.warning), "%s", text.c_str());
    }

    // Die Reiter. Eigene Leiste statt ImGui-TabBar: die bricht nicht um.
    // Tail hat sechs Seiten — in einer schmalen Spalte rollten "Physics" und
    // "Color" aus dem Bild, oder ImGui stauchte die Beschriftungen, und die
    // Reiter rueckten unter der Maus (Selbsttest). Das Original (Win32-
    // Eigenschaftsblatt) zeigt alle Reiter, notfalls in mehreren Reihen.
    const auto tabs = fields::tabsFor(p.type);
    if (std::find(tabs.begin(), tabs.end(), propertyTab_) == tabs.end()) {
        propertyTab_ = tabs.front();
    }
    {
        ImGui::PushID("propertyTabs");
        const ImGuiStyle& style = ImGui::GetStyle();
        const float left = ImGui::GetCursorScreenPos().x;
        const float right = left + ImGui::GetContentRegionAvail().x;
        float x = left;
        bool first = true;
        for (auto tab : tabs) {
            const char* text = tr(fields::tabLabel(tab));
            const float w = ImGui::CalcTextSize(text).x + style.FramePadding.x * 2.0f;
            if (!first) {
                if (x + style.ItemInnerSpacing.x + w <= right) {
                    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                } else {
                    x = left;  // neue Reihe
                }
            }
            first = false;
            const bool active = tab == propertyTab_;
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(
                                                        active ? ImGuiCol_TabSelected : ImGuiCol_Tab));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImGui::GetStyleColorVec4(ImGuiCol_TabHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  ImGui::GetStyleColorVec4(ImGuiCol_TabSelected));
            ImGui::PushID(fields::tabName(tab));
            if (ImGui::Button(text, ImVec2(w, 0.0f))) propertyTab_ = tab;
            ImGui::PopID();
            ImGui::PopStyleColor(3);
            x = ImGui::GetItemRectMax().x;
        }
        ImGui::PopID();
        ImGui::Separator();
    }
    ImGui::BeginChild("tabbody", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    beginPage();
    drawTab(propertyTab_, p);
    ImGui::EndChild();

    ImGui::EndChild();
}

void App::drawTab(fields::Tab tab, Primitive& p) {
    bool changed = false;
    const PrimitiveType type = p.type;
    const auto applies = [type](fields::Field f) { return fields::applies(type, f); };

    switch (tab) {
        // --- Generation ---------------------------------------------------------
        case fields::Tab::Generation: {
            minMaxHeader();
            changed |= rowRange("delay", tr(Str::FieldDelayMs), p.delay, 0.0f, 10.0f, 0.0f, 1.0e7f);
            {
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                ImGui::BeginDisabled(!applies(fields::Field::Count));
                changed |= flagBox(tr(Str::GenEvenDelay), p.spawnFlags, kSpawnEvenDistribution,
                                   false, "evenDelay");
                ImGui::EndDisabled();
            }
            ImGui::BeginDisabled(!applies(fields::Field::Count));
            changed |= rowRange("count", tr(Str::FieldCount), p.count, kDefaultCount, 1.0f, 0.0f, 1000.0f, "%.0f");
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!applies(fields::Field::Life));
            changed |= rowRange("life", tr(Str::FieldLifeMs), p.life, kDefaultLife, 10.0f, 0.0f, 1.0e7f);
            ImGui::EndDisabled();

            // Sichtweite. Das Haekchen schaltet die Zeile; der Wert ist eine
            // einzige Zahl ("cullrange 500").
            {
                bool cull = p.cullRangeSet;
                if (ImGui::Checkbox(tr(Str::GenUseCulling), &cull)) {
                    p.cullRangeSet = cull;
                    changed = true;
                }
                testmarke::marke("cull/an");
                ImGui::BeginDisabled(!p.cullRangeSet);
                labelCell(tr(Str::FieldCullDistance));
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                ImGui::SetNextItemWidth(g_page.field);
                if (ImGui::DragInt("##cull", &p.cullRange, 10.0f, 0, 1000000)) {
                    p.cullRangeSet = true;
                    changed = true;
                }
                testmarke::marke("cull/min");
                ImGui::EndDisabled();
            }

            // Ende-Effekte. Ab heute spiegelt das Haekchen die Engine: eine
            // Liste setzt das Bit selbst (CPrimitiveTemplate::ParseDeathFxStrings).
            ImGui::BeginDisabled(!hasDeathFx(type));
            {
                bool on = (p.flags & kFlagDeathRunsFx) != 0 || !p.deathFx.empty();
                bool clicked = false;
                const bool open = frameBegin("death", tr(Str::FieldDeathFx), nullptr);
                clicked = ImGui::Checkbox(tr(Str::DeathEnable), &on);
                testmarke::marke("deathfx/an");
                if (clicked) {
                    p.flags = on ? (p.flags | kFlagDeathRunsFx) : (p.flags & ~kFlagDeathRunsFx);
                    // Aus heisst aus: eine stehengebliebene Liste wuerde die
                    // Engine trotzdem abspielen. Rueckgaengig holt sie zurueck.
                    if (!on) p.deathFx.clear();
                    changed = true;
                }
                ImGui::BeginDisabled(!on);
                changed |= editStringList("deathfx", "", p.deathFx, "");
                ImGui::EndDisabled();
                frameEnd(open);
            }
            ImGui::EndDisabled();

            // Alles, was das Original nicht als Haekchen kennt (MP-Flags,
            // seltene Bits) — fuer Fortgeschrittene, eingeklappt.
            if (ImGui::CollapsingHeader(tr(Str::PropAdvancedFlags))) {
                changed |= editFlags(p);
            }
            ImGui::TextDisabled("%s", tr(Str::PropDefaultHint));
            break;
        }

        // --- Origin/Size --------------------------------------------------------
        case fields::Tab::OriginSize: {
            minMaxHeader();
            {
                const bool open = frameBegin("originFrame", tr(Str::FieldOrigin));
                changed |= rowsVector("origin", p.origin, Str::OriginForward, Str::OriginRight,
                                      Str::OriginUp);
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                changed |= flagBox(tr(Str::OriginRelativeAxis), p.spawnFlags, kSpawnCheapOrgCalc,
                                   true, "origin/relativ");
                frameEnd(open);
            }
            // Besondere Verteilung: Kugel oder Zylinder.
            const bool canSpecial = applies(fields::Field::Radius) || applies(fields::Field::Height) ||
                                    type == PrimitiveType::Particle;
            ImGui::BeginDisabled(type == PrimitiveType::ScreenFlash || type == PrimitiveType::Light ||
                                 type == PrimitiveType::Sound || type == PrimitiveType::CameraShake ||
                                 !canSpecial);
            {
                const bool onSphere = (p.spawnFlags & kSpawnOrgOnSphere) != 0;
                const bool onCylinder = (p.spawnFlags & kSpawnOrgOnCylinder) != 0;
                bool special = onSphere || onCylinder;
                if (ImGui::Checkbox(tr(Str::OriginEnableSpecial), &special)) {
                    if (!special) {
                        p.spawnFlags &= ~(kSpawnOrgOnSphere | kSpawnOrgOnCylinder | kSpawnAxisFromSphere);
                    } else {
                        p.spawnFlags |= kSpawnOrgOnSphere;
                    }
                    changed = true;
                }
                testmarke::marke("special/an");
                ImGui::BeginDisabled(!special);
                ImGui::Indent();
                int shape = onCylinder ? 1 : 0;
                if (ImGui::RadioButton(tr(Str::OriginSpherical), &shape, 0)) {
                    p.spawnFlags = (p.spawnFlags | kSpawnOrgOnSphere) & ~kSpawnOrgOnCylinder;
                    changed = true;
                }
                if (ImGui::RadioButton(tr(Str::OriginCylindrical), &shape, 1)) {
                    p.spawnFlags = (p.spawnFlags | kSpawnOrgOnCylinder) & ~kSpawnOrgOnSphere;
                    changed = true;
                }
                ImGui::Unindent();
                changed |= rowRange("radius", tr(Str::FieldRadiusWidth), p.radius, kDefaultRadius);
                ImGui::BeginDisabled(!onCylinder);
                changed |= rowRange("height", tr(Str::FieldHeight), p.height, kDefaultRadius);
                ImGui::EndDisabled();
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                changed |= flagBox(tr(Str::OriginAxisFromOffset), p.spawnFlags, kSpawnAxisFromSphere,
                                   false, "special/achse");
                ImGui::EndDisabled();
            }
            ImGui::EndDisabled();

            changed |= channelGroup("size", Str::GroupSizeWidth, p.size, Str::FieldStartSize,
                                    Str::FieldEndSize, kDefaultCurve, 0.5f,
                                    applies(fields::Field::Size));

            // Drehung steht im Original hier nur bei Typen ohne Bewegungsseite
            // (Decal); bei allen anderen auf der Seite Motion.
            const auto tabs = fields::tabsFor(type);
            const bool hasMotion =
                std::find(tabs.begin(), tabs.end(), fields::Tab::Motion) != tabs.end();
            if (!hasMotion && applies(fields::Field::Rotation)) {
                changed |= rowRange("rotation", tr(Str::FieldRotation), p.rotation, 0.0f, 0.5f);
            }
            if (showsDepthHack(type)) {
                changed |= flagBox(tr(Str::OriginDepthHack), p.flags, kFlagDepthHack, false, "depthhack");
            }
            break;
        }

        // --- Motion ---------------------------------------------------------------
        case fields::Tab::Motion: {
            minMaxHeader();
            {
                const bool open = frameBegin("velFrame", tr(Str::FieldVelocity));
                changed |= rowsVector("velocity", p.velocity, Str::OriginForward, Str::OriginRight,
                                      Str::OriginUp);
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                changed |= flagBox(tr(Str::MotionRelativeAxis), p.spawnFlags, kSpawnVelIsAbsolute,
                                   true, "velocity/relativ");
                changed |= flagBox(tr(Str::MotionAffectedByWind), p.spawnFlags, kSpawnAffectedByWind,
                                   false, "wind/an");
                ImGui::BeginDisabled((p.spawnFlags & kSpawnAffectedByWind) == 0);
                changed |= rowRange("wind", tr(Str::MotionPercentage), p.windModifier, kDefaultWind, 0.5f);
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                ImGui::TextDisabled("%s", tr(Str::MotionPercentHint));
                ImGui::EndDisabled();
                frameEnd(open);
            }
            {
                const bool open = frameBegin("accFrame", tr(Str::FieldAcceleration));
                changed |= rowsVector("accel", p.acceleration, Str::OriginForward, Str::OriginRight,
                                      Str::OriginUp);
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                changed |= flagBox(tr(Str::MotionRelativeAxis), p.spawnFlags, kSpawnAccelIsAbsolute,
                                   true, "accel/relativ");
                frameEnd(open);
            }
            changed |= rowRange("gravity", tr(Str::FieldGravity), p.gravity, 0.0f, 10.0f);
            ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
            ImGui::TextDisabled("(%s)", tr(Str::MotionGravityHint));
            ImGui::BeginDisabled(!applies(fields::Field::Rotation));
            changed |= rowRange("rotation", tr(Str::FieldRotation), p.rotation, 0.0f, 0.5f);
            changed |= rowRange("rotationDelta", tr(Str::FieldRotationDelta), p.rotationDelta, 0.0f, 0.5f);
            ImGui::EndDisabled();
            break;
        }

        // --- Physics --------------------------------------------------------------
        case fields::Tab::Physics: {
            minMaxHeader();
            const bool physics = (efx::effectiveFlags(p) & kFlagApplyPhysics) != 0;
            {
                bool on = physics;
                if (ImGui::Checkbox(tr(Str::PhysicsEnable), &on)) {
                    p.flags = on ? (p.flags | kFlagApplyPhysics) : (p.flags & ~kFlagApplyPhysics);
                    changed = true;
                }
                testmarke::marke("physik/an");
            }
            ImGui::BeginDisabled(!physics);
            ImGui::Indent();
            {
                // Wie im Original eine Rueckfrage beim Einschalten (Dialog-
                // text aus der Ressource): diese Physik kostet viel.
                bool expensive = (p.flags & kFlagExpensivePhysics) != 0;
                if (ImGui::Checkbox(tr(Str::PhysicsExpensive), &expensive)) {
                    if (!expensive) {
                        p.flags &= ~kFlagExpensivePhysics;
                        changed = true;
                    } else {
                        askConfirm(tr(Str::PhysicsExpensiveWarning), [this]() {
                            if (!hasSelection()) return;
                            doc().effect.primitives[static_cast<size_t>(doc().selectedPrimitive)].flags |=
                                kFlagExpensivePhysics;
                            doc().dirty = true;
                            refreshDiagnostics();
                            fieldEditOpen_ = true;
                            previewDirty_ = true;
                        });
                    }
                }
                testmarke::marke("physik/teuer");
            }
            ImGui::Unindent();
            changed |= rowRange("bounce", tr(Str::FieldBounceOnly), p.elasticity, 0.0f, 0.05f);
            {
                const bool open = frameBegin("impacts", tr(Str::PhysicsImpacts));
                changed |= flagBox(tr(Str::PhysicsKillOnImpact), p.flags, kFlagKillOnImpact, false,
                                   "impact/kill");
                bool play = (p.flags & kFlagImpactRunsFx) != 0 || !p.impactFx.empty();
                if (ImGui::Checkbox(tr(Str::PhysicsPlayOnImpact), &play)) {
                    p.flags = play ? (p.flags | kFlagImpactRunsFx) : (p.flags & ~kFlagImpactRunsFx);
                    if (!play) p.impactFx.clear();
                    changed = true;
                }
                testmarke::marke("impact/an");
                ImGui::BeginDisabled(!play);
                changed |= editStringList("impactfx", "", p.impactFx, "");
                ImGui::EndDisabled();
                frameEnd(open);
            }
            {
                const bool open = frameBegin("bbox", tr(Str::PhysicsBoundingBox));
                changed |= flagBox(tr(Str::PhysicsEnableBBox), p.flags, kFlagUseBBox, false, "bbox/an");
                ImGui::BeginDisabled((p.flags & kFlagUseBBox) == 0);
                // Spalte 1 = Ecke "min", Spalte 2 = Ecke "max" (Datei: min x y z,
                // max x y z) — so belegt das Original die drei Zeilen X, Y, Z.
                const Str axes[3] = {Str::BoxX, Str::BoxY, Str::BoxZ};
                for (int k = 0; k < 3; ++k) {
                    ImGui::PushID(k);
                    const char* names[3] = {"x", "y", "z"};
                    testmarke::Bereich bereich((std::string("bbox/") + names[k]).c_str());
                    labelCell(tr(axes[k]));
                    float low = p.min.set ? p.min.min[k] : 0.0f;
                    float high = p.max.set ? p.max.min[k] : 0.0f;
                    if (pairFields(low, high, p.min.set || p.max.set, 1.0f, 0.0f, 0.0f, "%.4g")) {
                        if (!p.min.set) p.min = Vec3Range{};
                        if (!p.max.set) p.max = Vec3Range{};
                        p.min.min[k] = low;
                        p.max.min[k] = high;
                        p.min.max = p.min.min;
                        p.max.max = p.max.min;
                        p.min.set = p.max.set = true;
                        p.min.ranged = p.max.ranged = false;
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndDisabled();
                frameEnd(open);
            }
            ImGui::EndDisabled();
            break;
        }

        // --- Color ----------------------------------------------------------------
        case fields::Tab::Color: {
            if (type == PrimitiveType::Emitter) {
                ImGui::TextDisabled("%s", tr(Str::ColorEmitterNote));
                ImGui::Separator();
            }
            if (applies(fields::Field::Shaders)) {
                const bool open = frameBegin("shadersFrame", tr(Str::FieldShaders));
                changed |= editStringList("shaders", "", p.shaders, "gfx/");
                // Welche Mischung jeder Shader hat — das Original sagt es nicht,
                // und ohne das ist "warum ist mein Rauch so hell" nicht zu klaeren.
                for (const std::string& name : p.shaders) {
                    const bool known = assets_.hasShader(name);
                    const shader::BlendMode mode = assets_.blendOf(name);
                    const char* modeText =
                        mode == shader::BlendMode::Additive    ? tr(Str::BlendAdditive)
                        : mode == shader::BlendMode::AlphaBlend ? tr(Str::BlendAlpha)
                                                                : tr(Str::BlendOpaque);
                    const theme::Palette& pal = activeTheme(settings_).palette;
                    ImGui::TextColored(toImGui(known ? pal.textDim : pal.warning), "%s: %s%s%s",
                                       name.c_str(), modeText, known ? "" : " - ",
                                       known ? "" : tr(Str::ShaderFallback));
                }
                frameEnd(open);
                changed |= flagBox(tr(Str::ColorShaderTime), p.flags, kFlagSetShaderTime, false,
                                   "shadertime");
            }
            changed |= editColorChannel(p.rgb);
            changed |= flagBox(tr(Str::ColorPickCube), p.spawnFlags, kSpawnRgbComponentInterp, true,
                               "farbwuerfel");
            ImGui::BeginDisabled(!applies(fields::Field::Alpha));
            changed |= flagBox(tr(Str::ColorModulateAlpha), p.flags, kFlagUseAlpha, true,
                               "modulate");
            minMaxHeader();
            changed |= channelGroup("alpha", Str::GroupAlphaTransparency, p.alpha, Str::FieldStartAlpha,
                                    Str::FieldEndAlpha, kDefaultCurve, 0.01f,
                                    applies(fields::Field::Alpha));
            ImGui::EndDisabled();
            break;
        }

        // --- Line -----------------------------------------------------------------
        case fields::Tab::Line: {
            int endpointMode = (p.spawnFlags & kSpawnOrg2FromTrace) ? 1 : 0;
            if (ImGui::RadioButton(tr(Str::LineEndpointGiven), &endpointMode, 0)) {
                p.spawnFlags &= ~(kSpawnOrg2FromTrace | kSpawnOrg2IsOffset);
                changed = true;
            }
            if (ImGui::RadioButton(tr(Str::LineEndpointTrace), &endpointMode, 1)) {
                p.spawnFlags |= kSpawnOrg2FromTrace;
                changed = true;
            }
            ImGui::Indent();
            ImGui::BeginDisabled(endpointMode != 1);
            changed |= flagBox(tr(Str::LineEndpointOffset), p.spawnFlags, kSpawnOrg2IsOffset, false,
                               "endpunkt/versatz");
            ImGui::EndDisabled();
            ImGui::Unindent();
            minMaxHeader();
            {
                const bool open = frameBegin("endFrame", tr(Str::LineEndpoint));
                changed |= rowsVector("origin2", p.origin2, Str::OriginForward, Str::OriginRight,
                                      Str::OriginUp);
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                changed |= flagBox(tr(Str::OriginRelativeAxis), p.spawnFlags, kSpawnCheapOrg2Calc,
                                   true, "origin2/relativ");
                frameEnd(open);
            }
            {
                const bool open = frameBegin("endFx", tr(Str::LineEndpointEffects));
                bool traceFx = (p.spawnFlags & kSpawnTraceImpactFx) != 0;
                if (ImGui::Checkbox(tr(Str::LinePlayAtEndpoint), &traceFx)) {
                    p.spawnFlags = traceFx ? (p.spawnFlags | kSpawnTraceImpactFx)
                                           : (p.spawnFlags & ~kSpawnTraceImpactFx);
                    changed = true;
                }
                testmarke::marke("endfx/an");
                ImGui::BeginDisabled(!traceFx);
                changed |= editStringList("impactfx", "", p.impactFx, "");
                ImGui::EndDisabled();
                frameEnd(open);
            }
            // Blitz: im Original auf jeder Linienseite, bei Line gesperrt.
            ImGui::BeginDisabled(type != PrimitiveType::Electricity);
            {
                const bool open = frameBegin("electricity", tr(Str::GroupElectricity));
                // Die Unruhe steht in der Datei unter "bounce" (mElasticity) —
                // der Kern liest sie dort (particles.cpp). Vorher schrieb
                // dieses Feld in `variance`, und der Blitz blieb unbeeindruckt.
                changed |= rowRange("chaos", tr(Str::LineChaos), p.elasticity, 0.0f, 0.05f);
                changed |= flagBox(tr(Str::LineTaper), p.flags, kFlagElectricityTaper, false, "taper");
                changed |= flagBox(tr(Str::LineBranch), p.flags, kFlagElectricityBranch, false, "branch");
                changed |= flagBox(tr(Str::LineGrow), p.flags, kFlagElectricityGrow, false, "grow");
                frameEnd(open);
            }
            ImGui::EndDisabled();
            break;
        }

        // --- Length/Size2 ---------------------------------------------------------
        case fields::Tab::Tail:
        case fields::Tab::LengthSize2:
            minMaxHeader();
            changed |= channelGroup("length", Str::FieldLength, p.length, Str::FieldStartLength,
                                    Str::FieldEndLength, kDefaultCurve, 0.5f);
            changed |= channelGroup("size2", Str::GroupSize2Width2, p.size2, Str::FieldStartSize2,
                                    Str::FieldEndSize2, kDefaultCurve, 0.5f,
                                    applies(fields::Field::Size2));
            break;

        // --- Model ----------------------------------------------------------------
        case fields::Tab::Model: {
            minMaxHeader();
            {
                const bool open = frameBegin("angleFrame", tr(Str::GroupAngle));
                changed |= rowsVector("angles", p.angles, Str::AnglePitch, Str::AngleYaw, Str::AngleRoll);
                frameEnd(open);
            }
            {
                const bool open = frameBegin("deltaFrame", tr(Str::GroupAngleDelta));
                changed |= rowsVector("angleDelta", p.anglesDelta, Str::AnglePitch, Str::AngleYaw,
                                      Str::AngleRoll);
                frameEnd(open);
            }
            {
                const bool open = frameBegin("modelsFrame", tr(Str::FieldModels));
                changed |= flagBox(tr(Str::ModelAttach), p.flags, kFlagAttachedModel, false, "attach");
                changed |= editStringList("models", "", p.models, "models/");
                frameEnd(open);
            }
            break;
        }

        // --- Emitter --------------------------------------------------------------
        case fields::Tab::Emitter: {
            {
                const bool open = frameBegin("emitFrame", tr(Str::FieldEmitFx));
                bool emit = (efx::effectiveFlags(p) & kFlagEmitFx) != 0;
                if (ImGui::Checkbox(tr(Str::EmitterEnable), &emit)) {
                    p.flags = emit ? (p.flags | kFlagEmitFx) : (p.flags & ~kFlagEmitFx);
                    if (!emit) p.emitFx.clear();
                    changed = true;
                }
                testmarke::marke("emit/an");
                ImGui::BeginDisabled(!emit);
                changed |= editStringList("emitfx", "", p.emitFx, "");
                ImGui::EndDisabled();
                frameEnd(open);
            }
            minMaxHeader();
            changed |= rowRange("density", tr(Str::FieldDensity), p.density, kDefaultDensity);
            changed |= rowRange("variance", tr(Str::FieldVariance), p.variance, kDefaultVariance, 0.05f);
            break;
        }

        // --- Sound ----------------------------------------------------------------
        case fields::Tab::Sound: {
            const bool open = frameBegin("soundsFrame", tr(Str::FieldSounds));
            changed |= editStringList("sounds", "", p.sounds, "sound/");
            frameEnd(open);
            if (ImGui::Button(tr(Str::SoundBrowse)) && fileDialog_) {
                const std::string picked = fileDialog_(
                    false, "Sound files (*.wav; *.mp3)\0*.wav;*.mp3\0All files\0*.*\0",
                    settings_.gamePath.c_str());
                if (!picked.empty()) {
                    const std::string relative = toGameRelative(picked);
                    if (relative.empty()) {
                        diag::warn("sound outside game path: " + picked);
                    } else {
                        p.sounds.push_back(relative);
                        changed = true;
                    }
                }
            }
            ImGui::SameLine();
            const bool empty = p.sounds.empty();
            ImGui::BeginDisabled(empty);
            if (ImGui::Button(tr(Str::SoundPlay)) && !empty) playSoundNow(p.sounds.front());
            ImGui::EndDisabled();
            if (empty) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", tr(Str::SoundPlayNone));
            } else if (!settings_.playSounds) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", tr(Str::SoundPlayOff));
            }
            break;
        }

        // --- FxRunner -------------------------------------------------------------
        case fields::Tab::FxRunner: {
            changed |= flagBox(tr(Str::FxRunnerRandomRotation), p.spawnFlags, kSpawnRandRotAroundFwd,
                               false, "randrot");
            const bool open = frameBegin("spawned", tr(Str::GroupSpawnedEffects));
            changed |= editStringList("playfx", "", p.playFx, "");
            frameEnd(open);
            break;
        }

        // --- CameraShake ----------------------------------------------------------
        case fields::Tab::CameraShake: {
            minMaxHeader();
            changed |= rowRange("radius", tr(Str::FieldRadius), p.radius, kDefaultRadius, 5.0f, 0.0f, 1.0e6f);
            // Zwei senkrechte Regler wie im Original: links Min, rechts Max,
            // 0 = nichts, 8 = Beben, 16 = Wahnsinn. Min bleibt <= Max — das
            // Original liess "intensity 12 6" zu (Fotos des Anwenders).
            labelCell(tr(Str::ShakeIntensity));
            const float sliderHeight = ImGui::GetFontSize() * 9.0f;
            const float sliderWidth = ImGui::GetFrameHeight();
            const float top = ImGui::GetCursorScreenPos().y;
            float low = p.elasticity.set ? p.elasticity.min : 0.0f;
            float high = p.elasticity.set ? p.elasticity.max : 0.0f;
            ImGui::SetCursorScreenPos(ImVec2(minX() + (g_page.field - sliderWidth) * 0.5f, top));
            const bool lowChanged = ImGui::VSliderFloat("##shakeMin", ImVec2(sliderWidth, sliderHeight),
                                                        &low, 0.0f, camera::Shake::kMaxIntensity, "%.1f");
            testmarke::marke("intensity/min");
            ImGui::SetCursorScreenPos(ImVec2(maxX() + (g_page.field - sliderWidth) * 0.5f, top));
            const bool highChanged = ImGui::VSliderFloat("##shakeMax", ImVec2(sliderWidth, sliderHeight),
                                                         &high, 0.0f, camera::Shake::kMaxIntensity, "%.1f");
            testmarke::marke("intensity/max");
            // Die Beschriftung des Originals zwischen den Reglern.
            const float labelX = minX() + g_page.field + g_page.gap * 0.5f;
            const auto mark = [&](Str text, float fraction) {
                const char* s = tr(text);
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(labelX - ImGui::CalcTextSize(s).x * 0.5f,
                           top + sliderHeight * fraction - ImGui::GetTextLineHeight() * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), s);
            };
            mark(Str::ShakeInsane, 0.04f);
            mark(Str::ShakeQuake, 0.5f);
            mark(Str::ShakeNothing, 0.96f);
            ImGui::SetCursorScreenPos(ImVec2(g_page.x0, top + sliderHeight + ImGui::GetStyle().ItemSpacing.y));
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
            if (lowChanged || highChanged) {
                if (lowChanged && high < low) high = low;
                if (highChanged && low > high) low = high;
                p.elasticity.min = low;
                p.elasticity.max = high;
                p.elasticity.set = true;
                p.elasticity.ranged = low != high;
                p.elasticityAsIntensity = true;
                changed = true;
            }
            break;
        }
    }

    if (changed) {
        doc().dirty = true;
        refreshDiagnostics();
        // In den Rueckgaengig-Verlauf, sobald das Feld losgelassen wird
        // (App::buildFrame) — nicht in jedem Bild eines Ziehens. Vorher kam
        // eine Feldaenderung NIE in den Verlauf.
        fieldEditOpen_ = true;
        // Und die laufende Vorschau zeigt die Aenderung sofort.
        previewDirty_ = true;
    }
}

}  // namespace efx::gui
