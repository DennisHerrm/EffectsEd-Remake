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
#include "imgui_internal.h"
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
    float field = 0.0f;   // Breite eines Zahlenfelds (mit Pfeilen)
    float gap = 0.0f;
    float pad = 0.0f;     // Luft zwischen Max-Feld und rechter Rahmenlinie
    // Rechte Rahmenkante: hinter dem Max-Feld. Vorher lag sie einen Abstand
    // zu weit links und schnitt durch die Pfeile des Max-Felds.
    float right() const { return x0 + label + gap * 2.0f + field * 2.0f + pad; }
};
Page g_page;

void beginPage() {
    // Wie im Original: Beschriftung rechtsbuendig, zwei schmale Felder mit
    // Pfeilen, alles innerhalb der Rahmen. Die Felder wachsen nicht mehr bis
    // zum Rand (das Original hat feste Breiten); die Bildlaufleiste ist
    // eingerechnet, damit nichts springt, wenn sie erscheint.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float avail = ImGui::GetContentRegionAvail().x - style.ScrollbarSize;
    const float em = ImGui::GetFontSize();
    g_page.x0 = ImGui::GetCursorScreenPos().x;
    g_page.gap = style.ItemSpacing.x;
    g_page.pad = em * 0.6f;
    g_page.label = std::clamp(avail * 0.36f, em * 6.0f, em * 9.0f);
    g_page.field = std::clamp((avail - g_page.label - g_page.gap * 2.0f - g_page.pad) * 0.5f,
                              em * 4.0f, em * 6.5f);
}

// Breite einer Liste im Rahmen: von der Einrueckung bis zur rechten Kante.
float listWidth() {
    return std::max(ImGui::GetFontSize() * 8.0f,
                    g_page.right() - g_page.pad - ImGui::GetCursorScreenPos().x);
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

// --- Spinner ----------------------------------------------------------------
//
// Jedes Zahlenfeld des Originals hat Pfeile (Up-Down-Control). Gemessen
// (ORIGINAL-INVENTAR 4.2): ein Klick geht um einen festen Schritt je Feld und
// RUNDET auf ein Vielfaches davon (Life 50 -> 200 -> 300, Alpha 0.55 -> 0.7),
// an den Bereichsgrenzen wird geklemmt. Gedrueckt halten wiederholt.
//
// Schritt und Grenzen gelten fuer die naechsten Zeilen; SpinScope setzt sie
// und stellt sie danach wieder her. Ohne Angabe: Schritt 1, unbegrenzt.
struct Spin {
    float step = 1.0f;
    float lo = -1.0e30f;
    float hi = 1.0e30f;
};
Spin g_spin;

struct SpinScope {
    Spin saved;
    SpinScope(float step, float lo = -1.0e30f, float hi = 1.0e30f) : saved(g_spin) {
        g_spin = Spin{step, lo, hi};
    }
    ~SpinScope() { g_spin = saved; }
    SpinScope(const SpinScope&) = delete;
    SpinScope& operator=(const SpinScope&) = delete;
};

// Ein Schritt nach oben oder unten, gerundet wie im Original.
float spinStep(float value, int direction, const Spin& spin) {
    const double step = spin.step > 0.0f ? spin.step : 1.0;
    // Der kleine Zuschlag faengt Gleitkommareste ab: 0.65 / 0.1 ist 6.4999...
    const double scaled = (static_cast<double>(value) + direction * step) / step;
    const double rounded = std::floor(scaled + 0.5 + 1.0e-6) * step;
    return static_cast<float>(std::clamp(rounded, static_cast<double>(spin.lo),
                                         static_cast<double>(spin.hi)));
}

// Die beiden Pfeile rechts neben einem Feld. Gibt die Zahl der Schritte
// zurueck (negativ nach unten, 0 ohne Klick, beim Halten beschleunigt).
int spinner(const char* mark) {
    const float h = ImGui::GetFrameHeight();
    const float w = std::round(h * 0.6f);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float half = std::floor(h * 0.5f);
    int direction = 0;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
    for (int k = 0; k < 2; ++k) {
        const ImVec2 pos(at.x, at.y + (k == 0 ? 0.0f : half));
        const ImVec2 size(w, k == 0 ? half : h - half);
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(k);
        if (ImGui::InvisibleButton("##spin", size)) {
            // Beschleunigung wie die Up-Down-Felder des Originals (UDM_SETACCEL,
            // gemessen: 0 s -> 1 Schritt, 2 s -> 5, 5 s -> 20 beim Halten).
            const float held = ImGui::GetIO().MouseDownDuration[ImGuiMouseButton_Left];
            const int accel = held >= 5.0f ? 20 : (held >= 2.0f ? 5 : 1);
            direction = (k == 0 ? 1 : -1) * accel;
        }
        const std::string name = std::string(mark) + (k == 0 ? "+" : "-");
        testmarke::marke(name.c_str());
        const bool hot = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                            ImGui::GetColorU32(held  ? ImGuiCol_FrameBgActive
                                               : hot ? ImGuiCol_FrameBgHovered
                                                     : ImGuiCol_FrameBg));
        // Ein kleines Dreieck, nach oben bzw. unten.
        const float cx = pos.x + size.x * 0.5f;
        const float cy = pos.y + size.y * 0.5f;
        const float r = std::max(2.0f, std::min(size.x, size.y) * 0.28f);
        const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text);
        if (k == 0) {
            draw->AddTriangleFilled(ImVec2(cx - r, cy + r * 0.5f), ImVec2(cx + r, cy + r * 0.5f),
                                    ImVec2(cx, cy - r * 0.6f), ink);
        } else {
            draw->AddTriangleFilled(ImVec2(cx - r, cy - r * 0.5f), ImVec2(cx, cy + r * 0.6f),
                                    ImVec2(cx + r, cy - r * 0.5f), ink);
        }
        ImGui::PopID();
    }
    ImGui::PopItemFlag();
    // Den Platz als ein Element belegen, damit die Zeile weiterlaeuft.
    ImGui::SetCursorScreenPos(at);
    ImGui::Dummy(ImVec2(w, h));
    return direction;
}

float spinnerWidth() { return std::round(ImGui::GetFrameHeight() * 0.6f); }

// --- Zahlenzeilen -----------------------------------------------------------
//
// Ein Wertepaar. `fallback`: was die Engine nimmt, wenn nichts in der Datei
// steht (FxTemplate.cpp, CPrimitiveTemplate::CPrimitiveTemplate).
bool pairFields(float& low, float& high, bool set, float speed, float lo, float hi,
                const char* format) {
    bool changed = false;
    if (!set) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
    ImGui::SetNextItemWidth(g_page.field - spinnerWidth());
    float a = low;
    bool minChanged = shared::dragFloatFinite("##min", &a, speed, lo, hi, format);
    testmarke::marke("min");
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushID("minSpin");
    if (const int d = spinner("min"); d != 0) {
        a = spinStep(low, d, g_spin);
        minChanged = true;
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SetCursorScreenPos(ImVec2(maxX(), ImGui::GetCursorScreenPos().y));
    ImGui::SetNextItemWidth(g_page.field - spinnerWidth());
    float b = high;
    bool maxChanged = shared::dragFloatFinite("##max", &b, speed, lo, hi, format);
    testmarke::marke("max");
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushID("maxSpin");
    if (const int d = spinner("max"); d != 0) {
        b = spinStep(high, d, g_spin);
        maxChanged = true;
    }
    ImGui::PopID();
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
    // Passt die Beschriftung nicht mehr bis zur rechten Rahmenkante, rueckt
    // der Haken nach links, statt ueber den Rahmen zu laufen ("Set effect
    // axis to offset direc..." in schmalen Spalten).
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float needed = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
                             ImGui::CalcTextSize(label, nullptr, true).x;
        const float limit = g_page.right() - g_page.pad;
        if (at.x + needed > limit && at.x > g_page.x0) {
            ImGui::SetCursorScreenPos(ImVec2(std::max(g_page.x0 + ImGui::GetFontSize() * 0.6f, limit - needed), at.y));
        }
    }
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

// Eine Kurvengruppe: Start, End, Transition, Parameter, Zufall. End ist nur
// bei einem Uebergang belegbar, Parameter nur bei nonlinear/wave/clamp —
// wie im Original. Ohne Haken im Titel (das Original hat keinen): wer ein
// Feld anfasst, nimmt die Gruppe in die Datei; unberuehrt zeigt sie grau die
// Vorgaben der Engine.
bool channelGroup(const char* id, Str title, Channel& c, Str startLabel, Str endLabel,
                  float fallback, float speed, bool enabled = true) {
    ImGui::PushID(id);
    testmarke::Bereich bereich(id);
    bool changed = false;
    if (!enabled) ImGui::BeginDisabled();
    const bool open = frameBegin("frame", tr(title));
    changed |= rowRange("start", tr(startLabel), c.start, fallback, speed);
    const bool hasTransition = (c.curveFlags & (kCurveClamp | kCurveLinear)) != 0;
    ImGui::BeginDisabled(!hasTransition && (c.curveFlags & kCurveRandom) == 0);
    changed |= rowRange("end", tr(endLabel), c.end, fallback, speed);
    ImGui::EndDisabled();
    changed |= transitionRow(c.curveFlags, c.curveWords);
    ImGui::BeginDisabled((c.curveFlags & kCurveClamp) == 0);
    {
        const SpinScope spin(1.0f);   // Parameter: Schritt 1, unbegrenzt (gemessen)
        changed |= rowRange("parm", tr(Str::FieldParm), c.parm, 0.0f, 1.0f);
    }
    ImGui::EndDisabled();
    changed |= randomBox(c.curveFlags, c.curveWords);
    frameEnd(open);
    if (!enabled) ImGui::EndDisabled();
    // Wer ein Feld anfasst, will die Gruppe auch in der Datei haben.
    if (changed) c.present = true;
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
        // Die zwanzig Felder des Originals (Popup 5100, Windows-Systempalette),
        // fuenf Reihen zu vier; der freie Farbwaehler darunter ist sein
        // "Other...".
        static constexpr unsigned char kPalette[20][3] = {
            {0, 0, 0},       {128, 0, 0},     {0, 128, 0},     {128, 128, 0},
            {0, 0, 128},     {128, 0, 128},   {0, 128, 128},   {192, 192, 192},
            {192, 220, 192}, {166, 202, 240}, {255, 251, 240}, {160, 160, 164},
            {128, 128, 128}, {255, 0, 0},     {0, 255, 0},     {255, 255, 0},
            {0, 0, 255},     {255, 0, 255},   {0, 255, 255},   {255, 255, 255}};
        const float cell = ImGui::GetFrameHeight();
        for (int k = 0; k < 20; ++k) {
            if (k % 4 != 0) ImGui::SameLine();
            ImGui::PushID(k);
            const ImVec4 swatch(kPalette[k][0] / 255.0f, kPalette[k][1] / 255.0f,
                                kPalette[k][2] / 255.0f, 1.0f);
            if (ImGui::ColorButton("##palette", swatch, ImGuiColorEditFlags_NoAlpha,
                                   ImVec2(cell, cell))) {
                colour[0] = swatch.x;
                colour[1] = swatch.y;
                colour[2] = swatch.z;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            testmarke::marke(("palette" + std::to_string(k)).c_str());
            ImGui::PopID();
        }
        ImGui::Separator();
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

bool App::editColorChannel(ColorChannel& c, Primitive& p) {
    // Aufbau wie die Color-Seite des Originals: Start, End, "Pick start/end
    // in color cube", Transition, Parameter, Zufall, "Modulate RGB value
    // using alpha value" — alles in EINEM Rahmen, ohne Haken im Titel.
    ImGui::PushID("rgb");
    testmarke::Bereich bereich("rgb");
    bool changed = false;
    const bool open = frameBegin("frame", tr(Str::GroupRgbColor));
    changed |= colourRow("start", Str::FieldStartColor, c.start);
    const bool hasTransition = (c.curveFlags & (kCurveClamp | kCurveLinear)) != 0;
    ImGui::BeginDisabled(!hasTransition && (c.curveFlags & kCurveRandom) == 0);
    changed |= colourRow("end", Str::FieldEndColor, c.end);
    ImGui::EndDisabled();
    ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
    // Wie Modulate: ein Schalter der Primitive, keine Aenderung der Farbkurve.
    const bool cube = flagBox(tr(Str::ColorPickCube), p.spawnFlags, kSpawnRgbComponentInterp, true,
                              "farbwuerfel");
    changed |= transitionRow(c.curveFlags, c.curveWords);
    ImGui::BeginDisabled((c.curveFlags & kCurveClamp) == 0);
    {
        const SpinScope spin(1.0f);   // Parameter: Schritt 1, unbegrenzt (gemessen)
        changed |= rowRange("parm", tr(Str::FieldParm), c.parm, 0.0f, 1.0f);
    }
    ImGui::EndDisabled();
    changed |= randomBox(c.curveFlags, c.curveWords);
    // Modulate steht im selben Rahmen, gehoert aber nicht zur Farbkurve: es
    // macht die Gruppe nicht "vorhanden".
    ImGui::BeginDisabled(!fields::applies(p.type, fields::Field::Alpha));
    const bool modulate = flagBox(tr(Str::ColorModulateAlpha), p.flags, kFlagUseAlpha, true,
                                  "modulate");
    ImGui::EndDisabled();
    frameEnd(open);
    if (changed) c.present = true;
    ImGui::PopID();
    return changed || modulate || cube;
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
            // Wie ein Reiter im Eigenschaftsblatt: der gewaehlte hat die
            // Farbe der Seite und keine Unterkante, die anderen liegen etwas
            // tiefer und dunkler.
            const bool active = tab == propertyTab_;
            ImGui::PushID(fields::tabName(tab));
            const float h = ImGui::GetFrameHeight();
            if (ImGui::InvisibleButton(text, ImVec2(w, h))) propertyTab_ = tab;
            const bool hot = ImGui::IsItemHovered();
            const ImVec2 a = ImGui::GetItemRectMin();
            const ImVec2 b = ImGui::GetItemRectMax();
            const float lift = active ? 0.0f : std::round(h * 0.12f);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImU32 fill = ImGui::GetColorU32(active ? ImGuiCol_ChildBg
                                                  : hot  ? ImGuiCol_TabHovered
                                                         : ImGuiCol_Tab);
            dl->AddRectFilled(ImVec2(a.x, a.y + lift), b, active ? ImGui::GetColorU32(ImGuiCol_WindowBg) : fill,
                              style.TabRounding, ImDrawFlags_RoundCornersTop);
            const ImU32 edge = ImGui::GetColorU32(ImGuiCol_Border);
            dl->AddLine(ImVec2(a.x, b.y), ImVec2(a.x, a.y + lift), edge);
            dl->AddLine(ImVec2(a.x, a.y + lift), ImVec2(b.x, a.y + lift), edge);
            dl->AddLine(ImVec2(b.x, a.y + lift), ImVec2(b.x, b.y), edge);
            if (!active) dl->AddLine(ImVec2(a.x, b.y - 1.0f), ImVec2(b.x, b.y - 1.0f), edge);
            const ImVec2 ts = ImGui::CalcTextSize(text);
            dl->AddText(ImVec2(a.x + (w - ts.x) * 0.5f, a.y + lift * 0.5f + (h - ts.y) * 0.5f),
                        ImGui::GetColorU32(active ? ImGuiCol_Text : ImGuiCol_TextDisabled), text);
            ImGui::PopID();
            x = ImGui::GetItemRectMax().x;
        }
        ImGui::PopID();
        ImGui::Separator();
    }
    // Kompakt wie die Win32-Seite des Originals: niedrige Felder, kleine
    // Haken und Optionsknoepfe. Vorher hatten Haken die volle Feldhoehe des
    // Themas und wirkten wie Knoepfe.
    const float em = ImGui::GetFontSize();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(std::round(em * 0.35f), std::round(em * 0.12f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(std::round(em * 0.45f), std::round(em * 0.32f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(std::round(em * 0.35f), std::round(em * 0.25f)));
    ImGui::BeginChild("tabbody", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    beginPage();
    drawTab(propertyTab_, p);
    ImGui::EndChild();
    ImGui::PopStyleVar(3);

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
            {
                const SpinScope spin(10.0f, 0.0f);
                changed |= rowRange("delay", tr(Str::FieldDelayMs), p.delay, 0.0f, 10.0f, 0.0f, 1.0e7f);
            }
            {
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                ImGui::BeginDisabled(!applies(fields::Field::Count));
                changed |= flagBox(tr(Str::GenEvenDelay), p.spawnFlags, kSpawnEvenDistribution,
                                   false, "evenDelay");
                ImGui::EndDisabled();
            }
            ImGui::BeginDisabled(!applies(fields::Field::Count));
            {
                const SpinScope spin(1.0f, 0.0f, 1000.0f);
                changed |= rowRange("count", tr(Str::FieldCount), p.count, kDefaultCount, 1.0f, 0.0f, 1000.0f, "%.0f");
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!applies(fields::Field::Life));
            {
                const SpinScope spin(100.0f, 0.0f);
                changed |= rowRange("life", tr(Str::FieldLifeMs), p.life, kDefaultLife, 10.0f, 0.0f, 1.0e7f);
            }
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
                ImGui::SetNextItemWidth(g_page.field - spinnerWidth());
                if (ImGui::DragInt("##cull", &p.cullRange, 10.0f, 0, 1000000)) {
                    p.cullRangeSet = true;
                    changed = true;
                }
                testmarke::marke("cull/min");
                // Up-Down wie im Original: Schritt 1, ab 0 (gemessen,
                // msctls_updown32 buddy 1055, range 0..2147483647).
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::PushID("cullSpin");
                {
                    testmarke::Bereich spinArea("cull");
                    if (const int d = spinner("min"); d != 0) {
                        p.cullRange = std::max(0, p.cullRange + d);
                        p.cullRangeSet = true;
                        changed = true;
                    }
                }
                ImGui::PopID();
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
                changed |= editStringList("deathfx", "", p.deathFx, "", listWidth());
                ImGui::EndDisabled();
                frameEnd(open);
            }
            ImGui::EndDisabled();

            // Alles, was das Original nicht als Haekchen kennt (MP-Flags,
            // seltene Bits) — fuer Fortgeschrittene, eingeklappt.
            // efxed-Zusaetze am Seitenende, unauffaellig: das Original hat sie
            // nicht. Vorher ein breiter Balken und ein Hinweis, der rechts
            // abgeschnitten wurde und eine waagerechte Bildlaufleiste erzwang.
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight() * 0.5f));
            if (ImGui::TreeNode(tr(Str::PropAdvancedFlags))) {
                changed |= editFlags(p);
                ImGui::TreePop();
            }
            // Fensterkoordinaten, nicht Bildschirm (PushTextWrapPos).
            ImGui::PushTextWrapPos(g_page.right() - ImGui::GetWindowPos().x + ImGui::GetScrollX());
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", tr(Str::PropDefaultHint));
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
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
            // Besondere Verteilung: Kugel oder Zylinder — im Original IM
            // Origin-Rahmen, unter "Relative to effect axis".
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
                {
                    const SpinScope spin(1.0f, 0.0f);
                    changed |= rowRange("radius", tr(Str::FieldRadiusWidth), p.radius, kDefaultRadius);
                }
                ImGui::BeginDisabled(!onCylinder);
                {
                    const SpinScope spin(1.0f, 0.0f);
                    changed |= rowRange("height", tr(Str::FieldHeight), p.height, kDefaultRadius);
                }
                ImGui::EndDisabled();
                ImGui::SetCursorScreenPos(ImVec2(minX(), ImGui::GetCursorScreenPos().y));
                changed |= flagBox(tr(Str::OriginAxisFromOffset), p.spawnFlags, kSpawnAxisFromSphere,
                                   false, "special/achse");
                ImGui::EndDisabled();
            }
            ImGui::EndDisabled();
                frameEnd(open);
            }

            {
                const SpinScope spin(1.0f, 0.0f);
                changed |= channelGroup("size", Str::GroupSizeWidth, p.size, Str::FieldStartSize,
                                        Str::FieldEndSize, kDefaultCurve, 0.5f,
                                        applies(fields::Field::Size));
            }

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
            {
                const SpinScope spin(0.1f, 0.0f);
                changed |= rowRange("bounce", tr(Str::FieldBounceOnly), p.elasticity, 0.0f, 0.05f);
            }
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
                changed |= editStringList("impactfx", "", p.impactFx, "", listWidth());
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
                // Wie im Original: Liste mit Ordner und X, darunter im selben
                // Rahmen "Set Shader Time for animating textures".
                const bool open = frameBegin("shadersFrame", tr(Str::FieldShaders));
                // Welche Mischung jeder Shader hat — das Original sagt es
                // nicht; bei uns als Kurzhinweis am Eintrag.
                const auto describe = [this](const std::string& name) {
                    const bool known = assets_.hasShader(name);
                    const shader::BlendMode mode = assets_.blendOf(name);
                    std::string text = name + ": ";
                    text += mode == shader::BlendMode::Additive    ? tr(Str::BlendAdditive)
                            : mode == shader::BlendMode::AlphaBlend ? tr(Str::BlendAlpha)
                                                                    : tr(Str::BlendOpaque);
                    if (!known) {
                        text += " - ";
                        text += tr(Str::ShaderFallback);
                    }
                    return text;
                };
                changed |= editStringList("shaders", "", p.shaders, "gfx/", listWidth(), describe);
                changed |= flagBox(tr(Str::ColorShaderTime), p.flags, kFlagSetShaderTime, false,
                                   "shadertime");
                frameEnd(open);
            }
            minMaxHeader();
            changed |= editColorChannel(p.rgb, p);
            ImGui::BeginDisabled(!applies(fields::Field::Alpha));
            {
                const SpinScope spin(0.1f, 0.0f, 1.0f);
                changed |= channelGroup("alpha", Str::GroupAlphaTransparency, p.alpha, Str::FieldStartAlpha,
                                        Str::FieldEndAlpha, kDefaultCurve, 0.01f,
                                        applies(fields::Field::Alpha));
            }
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
                changed |= editStringList("impactfx", "", p.impactFx, "", listWidth());
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
                {
                    const SpinScope spin(0.1f, 0.0f);
                    changed |= rowRange("chaos", tr(Str::LineChaos), p.elasticity, 0.0f, 0.05f);
                }
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
            {
                const SpinScope spin(1.0f, 0.0f);
                changed |= channelGroup("length", Str::FieldLength, p.length, Str::FieldStartLength,
                                        Str::FieldEndLength, kDefaultCurve, 0.5f);
            }
            {
                const SpinScope spin(1.0f, 0.0f);
                changed |= channelGroup("size2", Str::GroupSize2Width2, p.size2, Str::FieldStartSize2,
                                        Str::FieldEndSize2, kDefaultCurve, 0.5f,
                                        applies(fields::Field::Size2));
            }
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
                changed |= editStringList("models", "", p.models, "models/", listWidth());
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
                changed |= editStringList("emitfx", "", p.emitFx, "", listWidth());
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
            // Wie im Original: Liste mit Ordner, X und Lautsprecher.
            const bool open = frameBegin("soundsFrame", tr(Str::FieldSounds));
            changed |= editStringList("sounds", "", p.sounds, "sound/", listWidth(), {},
                                      [this](const std::string& name) { playSoundNow(name); });
            frameEnd(open);
            if (!p.sounds.empty() && !settings_.playSounds) {
                ImGui::TextDisabled("%s", tr(Str::SoundPlayOff));
            }
            break;
        }

        // --- FxRunner -------------------------------------------------------------
        case fields::Tab::FxRunner: {
            changed |= flagBox(tr(Str::FxRunnerRandomRotation), p.spawnFlags, kSpawnRandRotAroundFwd,
                               false, "randrot");
            const bool open = frameBegin("spawned", tr(Str::GroupSpawnedEffects));
            changed |= editStringList("playfx", "", p.playFx, "", listWidth());
            frameEnd(open);
            break;
        }

        // --- CameraShake ----------------------------------------------------------
        case fields::Tab::CameraShake: {
            minMaxHeader();
            {
                const SpinScope spin(1.0f, 0.0f);
                changed |= rowRange("radius", tr(Str::FieldRadius), p.radius, kDefaultRadius, 5.0f, 0.0f, 1.0e6f);
            }
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
            const bool lowChanged = shared::vSliderFloatFinite("##shakeMin", ImVec2(sliderWidth, sliderHeight),
                                                        &low, 0.0f, camera::Shake::kMaxIntensity, "%.1f");
            testmarke::marke("intensity/min");
            ImGui::SetCursorScreenPos(ImVec2(maxX() + (g_page.field - sliderWidth) * 0.5f, top));
            const bool highChanged = shared::vSliderFloatFinite("##shakeMax", ImVec2(sliderWidth, sliderHeight),
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
