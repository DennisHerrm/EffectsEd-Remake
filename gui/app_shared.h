// Kleine Helfer, die mehrere Teile der Oberflaeche brauchen.
//
// Beim Aufteilen von app.cpp stellte sich heraus, dass zwei Funktionen aus
// dem anonymen Namensraum von mehreren Stellen gebraucht wurden. In einer
// 3500-Zeilen-Datei faellt so etwas nicht auf — sobald die Datei geteilt ist,
// muss man sich entscheiden, wohin sie gehoeren.
//
// Genau das ist der Gewinn: die Aufteilung macht sichtbar, was gemeinsam
// benutzt wird.
#pragma once

#include <cmath>
#include <type_traits>

#include "efx/theme.h"
#include "efx/effect.h"
#include "efx/layout.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "theme_imgui.h"

namespace efx::gui {
namespace shared {

// Getippte Zahlen gelten, wenn das Tippen endet — nicht bei jeder Ziffer.
//
// ImGui uebernimmt den Text eines Zahlenfelds bei jedem Tastendruck. Wer in
// "Life" (Min 100, Max 500) im Max-Feld 800 eintippte, hatte nach der ersten
// Ziffer Max = 8; "Min <= Max" zog Min auf 8 herunter, und dort blieb es, als
// aus der 8 eine 800 wurde (Video eines Anwenders, 14.11.2026). Genauso
// stellte die "0" von "0.05" im Feld der Wiederholrate fuer einen Augenblick
// 1 ms ein, und jede Ziffer startete die Vorschau neu.
//
// Jetzt gilt der Wert, wenn das Feld verlassen wird: Enter, Tab, Klick
// daneben — wie im alten EffectsEd, das erst mit Enter oder Apply uebernahm.
// Escape verwirft das Getippte. Ziehen mit der Maus und die Pfeile wirken
// weiter sofort.
struct TypedValue {
    ImGuiID id = 0;
    const void* owner = nullptr;
    int frame = -100;
    double value = 0.0;
    bool valid = false;
};
inline TypedValue& typedValue() {
    static TypedValue value;
    return value;
}
// Wem die Felder gerade gehoeren — auf der Eigenschaftsseite das Segment.
// Die Felder zweier Segmente tragen dieselben ImGui-Kennungen; ein fuer
// Segment A getippter Wert darf nie in Segment B landen.
inline const void*& typedOwner() {
    static const void* owner = nullptr;
    return owner;
}
struct TypedOwnerScope {
    const void* saved;
    explicit TypedOwnerScope(const void* owner) : saved(typedOwner()) { typedOwner() = owner; }
    ~TypedOwnerScope() { typedOwner() = saved; }
    TypedOwnerScope(const TypedOwnerScope&) = delete;
    TypedOwnerScope& operator=(const TypedOwnerScope&) = delete;
};

// Ruft `widget(&wert)` auf (DragFloat, SliderFloat, DragInt ...) und gibt
// true zurueck, sobald *v einen neuen Wert hat: beim Ziehen sofort, beim
// Tippen einmal am Ende.
template <class T, class Widget>
bool commitTyped(const char* label, T* v, Widget&& widget) {
    const ImGuiID id = ImGui::GetID(label);
    TypedValue& t = typedValue();
    if (t.valid && t.id == id && t.owner != typedOwner() && ImGui::TempInputIsActive(id)) {
        // Das Feld zeigt inzwischen ein anderes Segment, waehrend noch fuer
        // das alte getippt wird: die Eingabe beenden und verwerfen, statt sie
        // dem neuen zuzuschreiben.
        ImGui::ClearActiveID();
        t.valid = false;
    }
    const bool mine = t.valid && t.id == id && t.owner == typedOwner();
    const bool typingBefore = ImGui::TempInputIsActive(id);
    T x = (typingBefore && mine) ? static_cast<T>(t.value) : *v;
    const bool changed = widget(&x);
    const bool typingNow = ImGui::TempInputIsActive(id);
    const auto commit = [v](T value) {
        // Unveraendert? (NaN gilt als Aenderung; keepFinite weist es ab.)
        bool same = !(value < *v) && !(value > *v);
        if constexpr (std::is_floating_point_v<T>) same = same && !std::isnan(value);
        if (same) return false;
        *v = value;
        return true;
    };
    if (typingNow) {
        // Es wird (noch) getippt: nur merken.
        if (!(typingBefore && mine)) {
            t.id = id;
            t.owner = typedOwner();
            t.value = static_cast<double>(*v);
            t.valid = true;
        }
        if (changed) t.value = static_cast<double>(x);
        t.frame = ImGui::GetFrameCount();
        return false;
    }
    if (typingBefore) {
        // In diesem Aufruf beendet: Enter, Tab oder Klick daneben.
        t.valid = false;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) return false;
        return commit(changed ? x : (mine ? static_cast<T>(t.value) : *v));
    }
    if (mine) {
        // Beendet, weil ein anderes Element in diesem Bild schon vorher aktiv
        // wurde (Klick auf einen Knopf weiter oben). Nur unmittelbar danach:
        // war das Feld zwischendurch gar nicht zu sehen, gilt nichts.
        t.valid = false;
        if (ImGui::GetFrameCount() - t.frame > 1) return false;
        return commit(static_cast<T>(t.value));
    }
    if (changed) *v = x;
    return changed;
}

// Zahlenfelder, die getipptes "nan" und "inf" abweisen.
//
// ImGui liest den Text eines Feldes mit atof — und das nimmt "nan" und "inf"
// an. Ueber die Eigenschaftsseite landete so "life nan 100" in der Datei, und
// eine unendliche Dauer lief in eine Umwandlung nach int (Fehlersuche
// 03.10.2026). Der alte Wert bleibt dann stehen, und die Grenzen gelten auch
// beim Tippen (lo >= hi heisst wie bei ImGui: keine Grenzen).
inline bool keepFinite(float* v, const float* before, int count, float lo, float hi) {
    bool ok = true;
    for (int i = 0; i < count; ++i) ok = ok && std::isfinite(v[i]);
    if (!ok) {
        for (int i = 0; i < count; ++i) v[i] = before[i];
        return false;
    }
    if (lo < hi) {
        for (int i = 0; i < count; ++i) v[i] = v[i] < lo ? lo : (v[i] > hi ? hi : v[i]);
    }
    return true;
}
inline bool dragFloatFinite(const char* label, float* v, float speed, float lo, float hi,
                            const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
    const float before = *v;
    return commitTyped(label, v, [&](float* x) {
               return ImGui::DragFloat(label, x, speed, lo, hi, format, flags);
           }) &&
           keepFinite(v, &before, 1, lo, hi);
}
inline bool dragIntTyped(const char* label, int* v, float speed, int lo, int hi) {
    return commitTyped(label, v, [&](int* x) { return ImGui::DragInt(label, x, speed, lo, hi); });
}
inline bool dragFloat3Finite(const char* label, float* v, float speed, float lo, float hi,
                             const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
    const float before[3] = {v[0], v[1], v[2]};
    return ImGui::DragFloat3(label, v, speed, lo, hi, format, flags) && keepFinite(v, before, 3, lo, hi);
}
inline bool sliderFloatFinite(const char* label, float* v, float lo, float hi,
                              const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
    const float before = *v;
    // Strg+Klick macht auch aus einem Regler ein Textfeld.
    return commitTyped(label, v, [&](float* x) {
               return ImGui::SliderFloat(label, x, lo, hi, format, flags);
           }) &&
           keepFinite(v, &before, 1, lo, hi);
}
inline bool vSliderFloatFinite(const char* label, const ImVec2& size, float* v, float lo, float hi,
                               const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
    const float before = *v;
    return ImGui::VSliderFloat(label, size, v, lo, hi, format, flags) && keepFinite(v, &before, 1, lo, hi);
}

inline const theme::Theme& activeTheme(const layout::Settings& settings) {
    const theme::Theme* found = theme::findTheme(settings.themeId);
    return found ? *found : theme::builtinThemes().front();
}

inline ImVec4 severityColor(Severity severity, const theme::Palette& p) {
    switch (severity) {
        case Severity::Error: return toImGui(p.error);
        case Severity::Warning: return toImGui(p.warning);
        default: return toImGui(p.info);
    }
}

}  // namespace shared

using shared::activeTheme;
using shared::severityColor;

}  // namespace efx::gui
