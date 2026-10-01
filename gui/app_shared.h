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

#include "efx/theme.h"
#include "efx/effect.h"
#include "efx/layout.h"

#include "imgui.h"
#include "theme_imgui.h"

namespace efx::gui {
namespace shared {

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
