// Uebersetzt eine geprüfte Palette in ImGuis rund sechzig Farbwerte.
//
// Die einzige Datei, die beides kennt. Der Kern in src/theme.cpp weiss nichts
// von ImGui — deshalb liessen sich die Kontraste dort ohne Fenster messen.
#pragma once

#include <type_traits>

#include "efx/renderer.h"
#include "efx/theme.h"
#include "imgui.h"

namespace efx::gui {

// Texturkennung in ImGuis Typ umwandeln.
//
// ImTextureID hat sich geändert: bis 1.91.3 war es `void*`, seit **1.91.4**
// (08.10.2024) ist es `ImU64`. Ravens Zeiten sind lange vorbei — heute muss der
// Typ auch auf 32-Bit-Systemen einen 64-Bit-Deskriptor aufnehmen, deshalb eine
// Zahl statt eines Zeigers. Wir sind auf 1.91.5 festgenagelt, also gilt hier
// `ImU64`.
//
// Warum trotzdem beide Fälle: eine spätere Aktualisierung soll nicht an einer
// Typumwandlung scheitern, die man erst beim Übersetzen bemerkt.
//
// Wichtig — und genau daran ist der erste Versuch gescheitert:
// **`if constexpr` verwirft den nicht genommenen Zweig nur innerhalb einer
// Vorlage.** In einer gewöhnlichen Funktion werden beide Zweige vollständig
// geprüft, und `reinterpret_cast` von `uintptr_t` nach `ImU64` ist nicht
// erlaubt. Die Hilfsfunktion muss deshalb eine Vorlage sein.
template <typename Target>
inline Target textureIdAs(render::TextureId id) {
    if constexpr (std::is_pointer_v<Target>) {
        return reinterpret_cast<Target>(id);
    } else {
        return static_cast<Target>(id);
    }
}

inline ImTextureID toImTexture(render::TextureId id) {
    return textureIdAs<ImTextureID>(id);
}

inline ImVec4 toImGui(const theme::Color& c) { return ImVec4(c.r, c.g, c.b, c.a); }

// Mischt zwei Farben. Fuer die Zwischentoene, die ImGui braucht und fuer die
// eine Palette keinen eigenen Eintrag hat.
inline ImVec4 mix(const theme::Color& a, const theme::Color& b, float t) {
    return ImVec4(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                  a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

inline ImVec4 alpha(const theme::Color& c, float a) {
    return ImVec4(c.r, c.g, c.b, a);
}

// `scale` ist die Bildschirmskalierung. Sie gehoert HIERHER und nicht hinter
// den Aufruf: vorher skalierte nur der Start (ScaleAllSizes), und jeder
// Theme-Wechsel schrieb die festen Groessen ungeskaliert zurueck — bei 150 %
// wurde FramePadding 9x6 zu 6x4 (Fehlersuche 03.10.2026).
inline void applyTheme(const theme::Theme& t, float scale = 1.0f) {
    const theme::Palette& p = t.palette;
    ImGuiStyle& style = ImGui::GetStyle();
    // Frisch anfangen: ScaleAllSizes skaliert ALLE Groessen, auch die, die
    // hier nicht gesetzt werden. Auf einem schon skalierten Stil waeren die
    // beim zweiten Mal doppelt skaliert.
    style = ImGuiStyle();
    ImVec4* col = style.Colors;

    col[ImGuiCol_Text] = toImGui(p.text);
    col[ImGuiCol_TextDisabled] = toImGui(p.textDim);
    col[ImGuiCol_WindowBg] = toImGui(p.windowBg);
    col[ImGuiCol_ChildBg] = toImGui(p.panelBg);
    col[ImGuiCol_PopupBg] = toImGui(p.popupBg);
    col[ImGuiCol_Border] = toImGui(p.border);
    col[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    col[ImGuiCol_FrameBg] = toImGui(p.control);
    col[ImGuiCol_FrameBgHovered] = toImGui(p.controlHover);
    col[ImGuiCol_FrameBgActive] = toImGui(p.controlActive);

    col[ImGuiCol_TitleBg] = toImGui(p.headerBg);
    col[ImGuiCol_TitleBgActive] = toImGui(p.headerBg);
    col[ImGuiCol_TitleBgCollapsed] = toImGui(p.headerBg);
    col[ImGuiCol_MenuBarBg] = toImGui(p.headerBg);

    col[ImGuiCol_ScrollbarBg] = alpha(p.panelBg, 0.0f);
    col[ImGuiCol_ScrollbarGrab] = toImGui(p.control);
    col[ImGuiCol_ScrollbarGrabHovered] = toImGui(p.controlHover);
    col[ImGuiCol_ScrollbarGrabActive] = toImGui(p.controlActive);

    col[ImGuiCol_CheckMark] = toImGui(p.accent);
    col[ImGuiCol_SliderGrab] = toImGui(p.accent);
    col[ImGuiCol_SliderGrabActive] = toImGui(p.accentHover);

    col[ImGuiCol_Button] = toImGui(p.control);
    col[ImGuiCol_ButtonHovered] = toImGui(p.controlHover);
    col[ImGuiCol_ButtonActive] = toImGui(p.controlActive);

    col[ImGuiCol_Header] = alpha(p.accent, 0.35f);
    col[ImGuiCol_HeaderHovered] = alpha(p.accent, 0.5f);
    col[ImGuiCol_HeaderActive] = alpha(p.accent, 0.7f);

    // Die Teiler zwischen den drei Bereichen. In Ruhe zurueckhaltend, unter
    // dem Zeiger deutlich — sonst sucht man die Greifkante.
    col[ImGuiCol_Separator] = toImGui(p.border);
    col[ImGuiCol_SeparatorHovered] = toImGui(p.accent);
    col[ImGuiCol_SeparatorActive] = toImGui(p.accentHover);

    col[ImGuiCol_ResizeGrip] = alpha(p.accent, 0.25f);
    col[ImGuiCol_ResizeGripHovered] = alpha(p.accent, 0.5f);
    col[ImGuiCol_ResizeGripActive] = alpha(p.accent, 0.75f);

    col[ImGuiCol_Tab] = toImGui(p.control);
    col[ImGuiCol_TabHovered] = toImGui(p.controlHover);
    col[ImGuiCol_TabSelected] = toImGui(p.panelBg);
    col[ImGuiCol_TabDimmed] = toImGui(p.headerBg);
    col[ImGuiCol_TabDimmedSelected] = toImGui(p.panelBg);
    col[ImGuiCol_TabSelectedOverline] = toImGui(p.accent);

    col[ImGuiCol_TableHeaderBg] = toImGui(p.headerBg);
    col[ImGuiCol_TableBorderStrong] = toImGui(p.border);
    col[ImGuiCol_TableBorderLight] = mix(p.border, p.panelBg, 0.5f);
    col[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    // Abwechselnde Zeilen in der Segmentliste. Bewusst schwach: Ravens Liste
    // hatte keine, und zu kraeftige Streifen lenken vom Inhalt ab.
    col[ImGuiCol_TableRowBgAlt] = alpha(p.text, t.dark ? 0.03f : 0.04f);

    col[ImGuiCol_TextSelectedBg] = alpha(p.accent, 0.4f);
    col[ImGuiCol_DragDropTarget] = toImGui(p.accent);
    col[ImGuiCol_NavCursor] = toImGui(p.accent);
    col[ImGuiCol_NavWindowingHighlight] = alpha(p.accent, 0.7f);
    col[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.4f);
    col[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.5f);

    col[ImGuiCol_PlotLines] = toImGui(p.accent);
    col[ImGuiCol_PlotLinesHovered] = toImGui(p.accentHover);
    col[ImGuiCol_PlotHistogram] = toImGui(p.accent);
    col[ImGuiCol_PlotHistogramHovered] = toImGui(p.accentHover);

    // Form. Kantig gehalten, damit es dem Original nahekommt — Raven hatte
    // rechteckige MFC-Bedienelemente, keine abgerundeten.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = t.dark ? 0.0f : 1.0f;

    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 5);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.ScrollbarSize = 13.0f;
    style.GrabMinSize = 10.0f;

    if (scale > 0.0f && scale != 1.0f) style.ScaleAllSizes(scale);
}

}  // namespace efx::gui
