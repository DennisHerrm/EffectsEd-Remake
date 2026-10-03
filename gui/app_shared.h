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

#include "efx/theme.h"
#include "efx/effect.h"
#include "efx/layout.h"

#include "imgui.h"
#include "theme_imgui.h"

namespace efx::gui {
namespace shared {

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
    return ImGui::DragFloat(label, v, speed, lo, hi, format, flags) && keepFinite(v, &before, 1, lo, hi);
}
inline bool dragFloat3Finite(const char* label, float* v, float speed, float lo, float hi,
                             const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
    const float before[3] = {v[0], v[1], v[2]};
    return ImGui::DragFloat3(label, v, speed, lo, hi, format, flags) && keepFinite(v, before, 3, lo, hi);
}
inline bool sliderFloatFinite(const char* label, float* v, float lo, float hi,
                              const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
    const float before = *v;
    return ImGui::SliderFloat(label, v, lo, hi, format, flags) && keepFinite(v, &before, 1, lo, hi);
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
