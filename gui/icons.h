// Symbole für die Werkzeugleiste — gezeichnet, nicht als Schrift.
//
// Eine Symbolschrift (Font Awesome und Verwandte) wäre weniger Arbeit, hängt
// aber daran, dass die Schrift geladen wird. Fällt sie aus, stehen leere
// Kästen in der Leiste — und genau das ist der Fehler, den man auf einem
// fremden Windows nicht bemerkt, bevor jemand anruft.
//
// Gezeichnete Symbole haben **gar keine Schriftabhängigkeit**, skalieren mit
// der Bildschirmauflösung, ohne unscharf zu werden, und folgen dem Thema,
// weil die Farbe aus der Palette kommt.
//
// Der Preis: jedes Symbol muss von Hand konstruiert werden. Alle sind deshalb
// in einem Einheitsquadrat von 0..1 beschrieben und werden erst beim Zeichnen
// auf die Knopfgröße gestreckt — so stimmen die Proportionen bei jeder DPI.
#pragma once

#include "imgui.h"

namespace efx::gui {

enum class Icon {
    New, Open, Save,
    AddSegment, DeleteSegment,
    Play, Pause, Stop,
    Axes, Room, Grid,
    Textured, Wireframe, Overdraw,
    Wind,
    // Feste Blickrichtungen. Ein Wuerfel, dessen zugewandte Seite gefüllt
    // ist — so sieht man die Richtung, ohne den Kurzhinweis zu lesen.
    ViewFront, ViewBack, ViewLeft, ViewRight, ViewTop, ViewBottom, ViewReset,
};

// Ein Knopf mit Symbol. `active` zeichnet ihn eingedrückt — für Umschalter.
//
// Gibt true zurück, wenn er in diesem Bild gedrückt wurde.
bool iconButton(const char* id, Icon icon, const char* tooltip, bool active,
                float size);

}  // namespace efx::gui
