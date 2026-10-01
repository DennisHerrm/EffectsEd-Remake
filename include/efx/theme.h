// Farbgebung.
//
// Bewusst ohne ImGui-Abhaengigkeit: die Farben sind reine Daten, damit sie
// hier ohne Fenster geprueft werden koennen — insbesondere auf Lesbarkeit.
// Die Umsetzung nach ImGuiCol steht in gui/theme_imgui.h und ist die einzige
// Stelle, die ImGui kennt.
//
// Der Aufbau folgt Rollen, nicht Bauteilen: "Text auf Flaeche" statt
// "ImGuiCol_Text". So laesst sich ein neues Thema anlegen, ohne die rund
// sechzig ImGui-Farben einzeln zu treffen, und die Pruefung kann sagen,
// welches Paar zu wenig Kontrast hat.
#pragma once

#include <cstdint>

#include "efx/i18n.h"
#include <string>
#include <vector>

namespace efx::theme {

struct Color {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    static constexpr Color rgb(uint32_t hex, float alpha = 1.0f) {
        return Color{static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
                     static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
                     static_cast<float>(hex & 0xFF) / 255.0f, alpha};
    }
    std::string toHex() const;
};

// Die Rollen. Wer ein Thema baut, fuellt genau diese aus.
struct Palette {
    // Flaechen, von hinten nach vorn
    Color windowBg;      // Hauptflaeche
    Color panelBg;       // Eigenschaftenblatt, Liste
    Color headerBg;      // Menue- und Werkzeugleiste, Spaltenkoepfe
    Color popupBg;

    // Schrift
    Color text;
    Color textDim;       // abgeschaltet, Hinweise
    Color textOnAccent;

    // Bedienelemente
    Color control;       // Eingabefeld, Knopf in Ruhe
    Color controlHover;
    Color controlActive;
    Color border;

    // Betonung
    Color accent;        // Auswahl, Regler, Fokus
    Color accentHover;

    // Bedeutungsfarben fuer die Pruefliste
    Color error;
    Color warning;
    Color info;
    Color ok;

    // Die 3D-Ansicht. Der alte Editor liess Wand- und Hintergrundfarbe frei
    // waehlen (Menue Bearbeiten). Das bleibt so, aber jedes Thema bringt eine
    // passende Voreinstellung mit, sonst leuchtet der Testraum im dunklen
    // Programm wie eine Taschenlampe.
    Color viewportBg;
    Color roomWall;

    // Himmel und Sonne. Sie gehören ins Thema und nicht in feste Werte: ein
    // heller Himmel über der Ansicht von „Hoher Kontrast" wäre so grell, dass
    // man den Effekt davor nicht mehr sieht.
    Color skyHorizon;
    Color skyZenith;
    Color sunGlow;
    Color grid;
    Color axisX, axisY, axisZ;
};

struct Theme {
    std::string id;         // fuer die Einstellungsdatei, unveraenderlich
    i18n::Str nameId;       // fuer das Menue, uebersetzt
    bool dark = true;
    Palette palette;

    // Der angezeigte Name in der aktuellen Sprache.
    //
    // Frueher stand hier eine feste deutsche Zeichenkette. Die
    // Uebersetzungspruefung sah nur gui/ und hat sie deshalb nicht gefunden —
    // im englischen Menue stand trotzdem "Dunkel".
    const char* name() const { return i18n::tr(nameId); }
};

// Alle mitgelieferten Themen. Das erste ist die Voreinstellung.
const std::vector<Theme>& builtinThemes();
const Theme* findTheme(const std::string& id);

// --- Lesbarkeit ----------------------------------------------------------
//
// Kontrastverhaeltnis nach WCAG 2.1. 4.5:1 gilt als Untergrenze fuer
// Fliesstext, 3:1 fuer grosse Schrift und fuer Umrisse von Bedienelementen.
// Das ist kein Geschmack, das ist messbar — und der Grund, warum die meisten
// selbstgebauten dunklen Themen nach einer Stunde in den Augen wehtun.
float relativeLuminance(const Color& c);
float contrastRatio(const Color& a, const Color& b);

struct ContrastIssue {
    std::string theme;
    std::string pair;
    float ratio = 0.0f;
    float required = 0.0f;
};

// Prueft alle Themen auf zu schwache Paarungen. Leer heisst: alles lesbar.
std::vector<ContrastIssue> checkContrast(const std::vector<Theme>& themes);

// Hebt oder senkt die Helligkeit einer Vordergrundfarbe so weit, bis sie vor
// dem Hintergrund das geforderte Verhaeltnis erreicht. Farbton und Saettigung
// bleiben erhalten, es wird also nicht grau, sondern nur heller oder dunkler.
// Gibt zurueck, ob etwas geaendert wurde.
bool ensureContrast(Color& foreground, const Color& background, float required);

// Wendet ensureContrast auf alle Paarungen einer Palette an.
//
// Laeuft beim Laden jedes Themas, auch bei selbst zusammengestellten. Damit
// kann sich niemand — auch ich nicht — eine Farbkombination bauen, die man
// nach einer Stunde nicht mehr lesen kann.
struct Adjustment {
    std::string pair;
    float before = 0.0f;
    float after = 0.0f;
};
std::vector<Adjustment> enforceReadability(Palette& palette);

}  // namespace efx::theme
