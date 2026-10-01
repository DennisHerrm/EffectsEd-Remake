#include "icons.h"

#include <cmath>
#include <algorithm>

namespace efx::gui {
namespace {

// Alle Symbole sind im Einheitsquadrat 0..1 beschrieben. Der Zeichner
// streckt sie auf den Knopf — damit stimmen die Proportionen bei jeder
// Bildschirmaufloesung, und es gibt nur eine Stelle, an der gerechnet wird.
struct Canvas {
    ImDrawList* list;
    ImVec2 origin;
    float extent;
    ImU32 colour;
    float thickness;

    ImVec2 at(float x, float y) const {
        return ImVec2(origin.x + x * extent, origin.y + y * extent);
    }
    void line(float x0, float y0, float x1, float y1) const {
        list->AddLine(at(x0, y0), at(x1, y1), colour, thickness);
    }
    void rect(float x0, float y0, float x1, float y1) const {
        list->AddRect(at(x0, y0), at(x1, y1), colour, 0.0f, 0, thickness);
    }
    void filled(float x0, float y0, float x1, float y1) const {
        list->AddRectFilled(at(x0, y0), at(x1, y1), colour);
    }
    void triangle(float x0, float y0, float x1, float y1, float x2, float y2) const {
        list->AddTriangleFilled(at(x0, y0), at(x1, y1), at(x2, y2), colour);
    }
    void circle(float x, float y, float r, bool fill) const {
        if (fill) {
            list->AddCircleFilled(at(x, y), r * extent, colour, 16);
        } else {
            list->AddCircle(at(x, y), r * extent, colour, 16, thickness);
        }
    }
};

void draw(const Canvas& c, Icon icon) {
    switch (icon) {
        case Icon::New:
            // Ein Blatt mit umgeknickter Ecke — das Zeichen fuer "neu", seit
            // es Werkzeugleisten gibt.
            c.line(0.25f, 0.12f, 0.62f, 0.12f);
            c.line(0.62f, 0.12f, 0.78f, 0.30f);
            c.line(0.78f, 0.30f, 0.78f, 0.88f);
            c.line(0.78f, 0.88f, 0.25f, 0.88f);
            c.line(0.25f, 0.88f, 0.25f, 0.12f);
            c.line(0.62f, 0.12f, 0.62f, 0.30f);
            c.line(0.62f, 0.30f, 0.78f, 0.30f);
            break;
        case Icon::Open:
            // Ein geoeffneter Ordner: Rueckwand und schraege Vorderklappe.
            c.line(0.14f, 0.30f, 0.44f, 0.30f);
            c.line(0.44f, 0.30f, 0.52f, 0.40f);
            c.line(0.52f, 0.40f, 0.86f, 0.40f);
            c.line(0.14f, 0.30f, 0.14f, 0.80f);
            c.triangle(0.14f, 0.80f, 0.28f, 0.50f, 0.94f, 0.50f);
            c.triangle(0.14f, 0.80f, 0.94f, 0.50f, 0.80f, 0.80f);
            break;
        case Icon::Save:
            // Eine Diskette. Nach zwanzig Jahren hat noch niemand ein
            // besseres Zeichen fuer "speichern" gefunden.
            c.rect(0.16f, 0.16f, 0.84f, 0.84f);
            c.filled(0.32f, 0.16f, 0.68f, 0.40f);   // Schieber
            c.rect(0.28f, 0.56f, 0.72f, 0.84f);     // Etikett
            break;
        case Icon::AddSegment:
            // Zauberstab wie im Original: Stab und Funken.
            c.line(0.18f, 0.84f, 0.62f, 0.40f);
            c.line(0.72f, 0.14f, 0.72f, 0.38f);
            c.line(0.60f, 0.26f, 0.84f, 0.26f);
            c.line(0.80f, 0.42f, 0.88f, 0.50f);
            c.line(0.50f, 0.12f, 0.56f, 0.18f);
            break;
        case Icon::DeleteSegment:
            // Rotes X wie im Original.
            c.line(0.22f, 0.22f, 0.78f, 0.78f);
            c.line(0.78f, 0.22f, 0.22f, 0.78f);
            break;
        case Icon::Clone:
            c.rect(0.14f, 0.12f, 0.58f, 0.66f);
            c.rect(0.40f, 0.34f, 0.86f, 0.88f);
            break;
        case Icon::About:
            c.line(0.34f, 0.32f, 0.40f, 0.20f);
            c.line(0.40f, 0.20f, 0.60f, 0.20f);
            c.line(0.60f, 0.20f, 0.68f, 0.32f);
            c.line(0.68f, 0.32f, 0.50f, 0.50f);
            c.line(0.50f, 0.50f, 0.50f, 0.64f);
            c.filled(0.46f, 0.76f, 0.54f, 0.84f);
            break;
        case Icon::PlaybackSettings:
            // Haekchen-Feld mit Abspielpfeil, wie Knopf 32861 im Original.
            c.rect(0.14f, 0.30f, 0.54f, 0.70f);
            c.line(0.20f, 0.50f, 0.32f, 0.62f);
            c.line(0.32f, 0.62f, 0.50f, 0.36f);
            c.triangle(0.60f, 0.30f, 0.60f, 0.70f, 0.90f, 0.50f);
            break;
        case Icon::OrientUp:
            c.line(0.30f, 0.80f, 0.30f, 0.14f);   // Z, hervorgehoben
            c.line(0.30f, 0.14f, 0.20f, 0.28f);
            c.line(0.30f, 0.14f, 0.40f, 0.28f);
            c.line(0.30f, 0.80f, 0.80f, 0.80f);
            c.line(0.30f, 0.80f, 0.62f, 0.54f);
            break;
        case Icon::OrientSide:
            c.line(0.20f, 0.80f, 0.20f, 0.36f);
            c.line(0.20f, 0.80f, 0.88f, 0.80f);   // X, hervorgehoben
            c.line(0.88f, 0.80f, 0.74f, 0.70f);
            c.line(0.88f, 0.80f, 0.74f, 0.90f);
            c.line(0.20f, 0.80f, 0.52f, 0.54f);
            break;
        case Icon::OrientDown:
            c.line(0.30f, 0.20f, 0.30f, 0.86f);   // -Z, hervorgehoben
            c.line(0.30f, 0.86f, 0.20f, 0.72f);
            c.line(0.30f, 0.86f, 0.40f, 0.72f);
            c.line(0.30f, 0.20f, 0.80f, 0.20f);
            c.line(0.30f, 0.20f, 0.62f, 0.46f);
            break;
        case Icon::SetOrigin:
            c.line(0.22f, 0.80f, 0.22f, 0.30f);
            c.line(0.22f, 0.80f, 0.72f, 0.80f);
            c.line(0.22f, 0.80f, 0.52f, 0.54f);
            c.circle(0.70f, 0.32f, 0.12f, true);
            break;
        case Icon::Play:
            c.triangle(0.30f, 0.18f, 0.30f, 0.82f, 0.82f, 0.50f);
            break;
        case Icon::Pause:
            c.filled(0.28f, 0.20f, 0.44f, 0.80f);
            c.filled(0.56f, 0.20f, 0.72f, 0.80f);
            break;
        case Icon::Stop:
            c.filled(0.26f, 0.26f, 0.74f, 0.74f);
            break;
        case Icon::Axes:
            // Drei Achsen aus einem Punkt. Die Farbe kommt vom Thema, also
            // keine roten und gruenen Striche — die Form allein genuegt.
            c.line(0.22f, 0.78f, 0.22f, 0.24f);
            c.line(0.22f, 0.78f, 0.80f, 0.78f);
            c.line(0.22f, 0.78f, 0.62f, 0.40f);
            break;
        case Icon::Room:
            // Ein Kasten in Schraegsicht: Vorderflaeche plus drei Kanten
            // nach hinten.
            c.rect(0.18f, 0.34f, 0.66f, 0.82f);
            c.line(0.18f, 0.34f, 0.36f, 0.18f);
            c.line(0.66f, 0.34f, 0.84f, 0.18f);
            c.line(0.66f, 0.82f, 0.84f, 0.66f);
            c.line(0.36f, 0.18f, 0.84f, 0.18f);
            c.line(0.84f, 0.18f, 0.84f, 0.66f);
            break;
        // --- Feste Blickrichtungen ------------------------------------
        //
        // Ein Kasten in Schraegansicht, bei dem die Seite HERVORGEHOBEN ist,
        // auf die man blickt. Ohne diese Hervorhebung waeren alle sechs
        // Symbole gleich und man muesste jedes Mal den Kurzhinweis lesen.
        case Icon::ViewFront:
            c.rect(0.20f, 0.28f, 0.72f, 0.80f);
            c.line(0.20f, 0.28f, 0.34f, 0.16f);
            c.line(0.72f, 0.28f, 0.86f, 0.16f);
            c.line(0.34f, 0.16f, 0.86f, 0.16f);
            c.line(0.86f, 0.16f, 0.86f, 0.68f);
            c.line(0.72f, 0.80f, 0.86f, 0.68f);
            c.filled(0.20f, 0.28f, 0.72f, 0.80f);
            break;
        case Icon::ViewBack:
            c.rect(0.20f, 0.28f, 0.72f, 0.80f);
            c.line(0.20f, 0.28f, 0.34f, 0.16f);
            c.line(0.72f, 0.28f, 0.86f, 0.16f);
            c.line(0.34f, 0.16f, 0.86f, 0.16f);
            c.line(0.86f, 0.16f, 0.86f, 0.68f);
            c.line(0.72f, 0.80f, 0.86f, 0.68f);
            c.filled(0.34f, 0.16f, 0.86f, 0.28f);
            break;
        case Icon::ViewLeft:
            c.rect(0.28f, 0.28f, 0.80f, 0.80f);
            c.line(0.28f, 0.28f, 0.14f, 0.16f);
            c.line(0.28f, 0.80f, 0.14f, 0.68f);
            c.line(0.14f, 0.16f, 0.14f, 0.68f);
            c.filled(0.14f, 0.16f, 0.28f, 0.72f);
            break;
        case Icon::ViewRight:
            c.rect(0.20f, 0.28f, 0.72f, 0.80f);
            c.line(0.72f, 0.28f, 0.86f, 0.16f);
            c.line(0.72f, 0.80f, 0.86f, 0.68f);
            c.line(0.86f, 0.16f, 0.86f, 0.68f);
            c.filled(0.72f, 0.16f, 0.86f, 0.72f);
            break;
        case Icon::ViewTop:
            c.rect(0.20f, 0.34f, 0.72f, 0.82f);
            c.line(0.20f, 0.34f, 0.34f, 0.20f);
            c.line(0.72f, 0.34f, 0.86f, 0.20f);
            c.line(0.34f, 0.20f, 0.86f, 0.20f);
            c.line(0.86f, 0.20f, 0.86f, 0.68f);
            c.filled(0.20f, 0.20f, 0.86f, 0.34f);
            break;
        case Icon::ViewBottom:
            c.rect(0.20f, 0.18f, 0.72f, 0.66f);
            c.line(0.20f, 0.66f, 0.34f, 0.80f);
            c.line(0.72f, 0.66f, 0.86f, 0.80f);
            c.line(0.34f, 0.80f, 0.86f, 0.80f);
            c.line(0.86f, 0.32f, 0.86f, 0.80f);
            c.filled(0.20f, 0.66f, 0.86f, 0.80f);
            break;
        case Icon::ViewReset:
            // Ein Pfeil, der im Kreis zurueckfuehrt.
            c.line(0.24f, 0.50f, 0.32f, 0.30f);
            c.line(0.32f, 0.30f, 0.54f, 0.22f);
            c.line(0.54f, 0.22f, 0.74f, 0.34f);
            c.line(0.74f, 0.34f, 0.78f, 0.56f);
            c.line(0.78f, 0.56f, 0.60f, 0.74f);
            c.line(0.60f, 0.74f, 0.38f, 0.72f);
            c.line(0.24f, 0.50f, 0.14f, 0.44f);
            c.line(0.24f, 0.50f, 0.34f, 0.60f);
            break;

        case Icon::Grid:
            c.rect(0.18f, 0.18f, 0.82f, 0.82f);
            c.line(0.39f, 0.18f, 0.39f, 0.82f);
            c.line(0.61f, 0.18f, 0.61f, 0.82f);
            c.line(0.18f, 0.39f, 0.82f, 0.39f);
            c.line(0.18f, 0.61f, 0.82f, 0.61f);
            break;
        case Icon::Textured:
            // Gefuellte Flaeche mit Muster — "mit Textur".
            c.filled(0.18f, 0.18f, 0.82f, 0.82f);
            break;
        case Icon::Wireframe:
            // Dieselbe Flaeche, nur als Gitter — der Gegensatz ist die
            // Aussage.
            c.rect(0.18f, 0.18f, 0.82f, 0.82f);
            c.line(0.18f, 0.18f, 0.82f, 0.82f);
            c.line(0.82f, 0.18f, 0.18f, 0.82f);
            break;
        case Icon::Overdraw:
            // Drei sich ueberlappende Kreise: genau das, was der Modus zeigt.
            c.circle(0.38f, 0.40f, 0.22f, false);
            c.circle(0.62f, 0.40f, 0.22f, false);
            c.circle(0.50f, 0.62f, 0.22f, false);
            break;
        case Icon::Wind:
            // Drei wehende Linien, unten am laengsten.
            c.line(0.16f, 0.34f, 0.62f, 0.34f);
            c.line(0.62f, 0.34f, 0.74f, 0.24f);
            c.line(0.16f, 0.52f, 0.78f, 0.52f);
            c.line(0.78f, 0.52f, 0.88f, 0.42f);
            c.line(0.16f, 0.70f, 0.56f, 0.70f);
            c.line(0.56f, 0.70f, 0.66f, 0.60f);
            break;
    }
}

}  // namespace

// Die Farbe eines Symbols.
//
// Ravens Symbole aus EffectsEd.exe sind farbig, und die Farben tragen
// Bedeutung: gruen heisst abspielen, gelb Pause, rot Stop oder loeschen, blau
// steht fuer den Aufbau der Szene, oliv fuer Dateioperationen. Wer den alten
// Editor kennt, findet die Knoepfe am Farbfleck, noch bevor er die Form
// erkennt — bei einer Leiste mit fuenfzehn gleichfarbigen Symbolen faellt
// genau das weg.
//
// Die Werte sind aus den Streifen im Binary abgelesen (RT_BITMAP 128, 155,
// 158 und 167), nur etwas entsaettigt: Ravens reines Gruen 0,255,0 stammt aus
// einer 16-Farben-Palette und sticht auf dunklem Grund unangenehm heraus.
//
// Symbole ohne eigene Bedeutung behalten die Textfarbe des Themas — sonst
// wird die Leiste bunt, ohne dass die Farbe etwas sagt.
static ImU32 colourFor(Icon icon) {
    switch (icon) {
        case Icon::Play:           return IM_COL32(88, 200, 96, 255);   // gruen
        case Icon::Pause:          return IM_COL32(226, 200, 74, 255);  // gelb
        case Icon::Stop:           return IM_COL32(214, 78, 70, 255);   // rot
        case Icon::DeleteSegment:  return IM_COL32(214, 78, 70, 255);   // rot
        case Icon::AddSegment:     return IM_COL32(226, 200, 74, 255);  // gelb
        case Icon::Open:           return IM_COL32(214, 176, 84, 255);  // oliv
        case Icon::Save:           return IM_COL32(150, 176, 96, 255);  // oliv-gruen
        case Icon::Axes:           return IM_COL32(110, 150, 230, 255); // blau
        case Icon::Room:           return IM_COL32(110, 150, 230, 255);
        case Icon::Grid:           return IM_COL32(226, 200, 74, 255);  // gelb
        case Icon::Wind:           return IM_COL32(226, 200, 74, 255);
        case Icon::Clone:          return IM_COL32(110, 150, 230, 255);
        case Icon::About:          return IM_COL32(226, 200, 74, 255);
        case Icon::PlaybackSettings: return IM_COL32(88, 200, 96, 255);
        case Icon::OrientUp:       return IM_COL32(110, 150, 230, 255); // Z blau
        case Icon::OrientSide:     return IM_COL32(214, 78, 70, 255);   // X rot
        case Icon::OrientDown:     return IM_COL32(200, 90, 200, 255);  // -Z magenta
        case Icon::SetOrigin:      return IM_COL32(110, 150, 230, 255);
        default: break;
    }
    return ImGui::GetColorU32(ImGuiCol_Text);
}

bool iconButton(const char* id, Icon icon, const char* tooltip, bool active,
                float size) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    const bool pressed = ImGui::Button(id, ImVec2(size, size));
    if (active) ImGui::PopStyleColor();

    // Nach dem Knopf zeichnen, damit das Symbol darueber liegt. Die Groesse
    // kommt aus dem tatsaechlichen Rechteck, nicht aus dem Wunsch — bei
    // Themen mit anderem Innenabstand weichen die voneinander ab.
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float side = std::min(max.x - min.x, max.y - min.y);
    const float inset = side * 0.14f;

    Canvas canvas{
        ImGui::GetWindowDrawList(),
        ImVec2(min.x + (max.x - min.x - side) * 0.5f + inset,
               min.y + (max.y - min.y - side) * 0.5f + inset),
        side - inset * 2.0f,
        colourFor(icon),
        // Strichstaerke mit der Groesse, aber nie duenner als ein Bildpunkt:
        // sonst verschwinden die Symbole bei kleiner Leiste.
        std::max(1.0f, side * 0.075f),
    };
    // Ausgegraute Knoepfe bekommen von selbst ein blasses Symbol:
    // BeginDisabled senkt style.Alpha, und GetColorU32 rechnet das ein.
    //
    // Der erste Entwurf fragte dafuer ImGuis Innenleben ab
    // (GetCurrentContext()->CurrentItemFlags). Das gehoert zur internen
    // Schnittstelle und kann sich zwischen Fassungen aendern — dieselbe
    // Entscheidung wie schon bei den Gruppenrahmen.
    draw(canvas, icon);

    if (tooltip && *tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return pressed;
}

}  // namespace efx::gui
