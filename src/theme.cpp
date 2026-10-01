#include "efx/theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace efx::theme {
namespace {

// Eine Farbe aufhellen oder abdunkeln und dabei zum Blauen ziehen.
//
// Der Himmel soll zum Thema passen, aber nicht dieselbe Farbe haben wie die
// Waende — sonst verschwimmt der Horizont. `towardsBlue` mischt in Richtung
// eines kuehlen Blaus, wie es ein echter Himmel hat.
Color shadeOf(const Color& base, float brightness, float towardsBlue) {
    const Color sky{0.35f, 0.55f, 0.95f, 1.0f};
    auto mix = [&](float from, float to) {
        const float value = (from * brightness) * (1.0f - towardsBlue) +
                            to * towardsBlue;
        return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    };
    return {mix(base.r, sky.r), mix(base.g, sky.g), mix(base.b, sky.b), 1.0f};
}

}  // namespace

namespace {

// Von sRGB in den linearen Raum, wie WCAG es vorschreibt. Der naive Weg —
// einfach die Kanaele mitteln — liefert bei dunklen Farben deutlich zu
// guenstige Werte.
float linearize(float channel) {
    return channel <= 0.03928f ? channel / 12.92f
                               : std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

}  // namespace

std::string Color::toHex() const {
    auto to255 = [](float v) {
        int i = static_cast<int>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
        return i;
    };
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", to255(r), to255(g),
                  to255(b));
    return buffer;
}

float relativeLuminance(const Color& c) {
    return 0.2126f * linearize(c.r) + 0.7152f * linearize(c.g) +
           0.0722f * linearize(c.b);
}

float contrastRatio(const Color& a, const Color& b) {
    float la = relativeLuminance(a);
    float lb = relativeLuminance(b);
    if (la < lb) std::swap(la, lb);
    return (la + 0.05f) / (lb + 0.05f);
}

const std::vector<Theme>& builtinThemes() {
    static const std::vector<Theme> kThemes = [] {
        std::vector<Theme> themes;

        // -------------------------------------------------------------
        // Dunkel — die Voreinstellung.
        //
        // Kein reines Schwarz als Flaeche. Weisse Schrift auf Schwarz
        // erzeugt Halos, die beim Lesen ermueden; deshalb liegt der
        // Hintergrund bei etwa 12 Prozent Helligkeit und die Schrift knapp
        // unter Weiss.
        {
            Theme t;
            t.id = "dark";
            t.nameId = i18n::Str::ThemeDark;
            t.dark = true;
            Palette& p = t.palette;
            p.windowBg = Color::rgb(0x1E1E22);
            p.panelBg = Color::rgb(0x252529);
            p.headerBg = Color::rgb(0x2D2D33);
            p.popupBg = Color::rgb(0x252529);
            p.text = Color::rgb(0xE4E4E7);
            p.textDim = Color::rgb(0x9A9AA3);
            p.textOnAccent = Color::rgb(0xFFFFFF);
            p.control = Color::rgb(0x35353C);
            p.controlHover = Color::rgb(0x42424A);
            p.controlActive = Color::rgb(0x4E4E58);
            p.border = Color::rgb(0x3A3A42);
            p.accent = Color::rgb(0x4A8FD4);
            p.accentHover = Color::rgb(0x5C9FE0);
            p.error = Color::rgb(0xF08080);
            p.warning = Color::rgb(0xE8C060);
            p.info = Color::rgb(0x7FB8E8);
            p.ok = Color::rgb(0x7FC98A);
            p.viewportBg = Color::rgb(0x141417);
            p.roomWall = Color::rgb(0x3A4A3E);
            // Der Himmel folgt der Grundstimmung des Themas.
            p.skyHorizon = shadeOf(p.roomWall, 1.25f, 0.55f);
            p.skyZenith = shadeOf(p.roomWall, 0.55f, 0.85f);
            p.sunGlow = shadeOf(p.roomWall, 2.2f, 0.15f);
            p.grid = Color::rgb(0x50505A);
            p.axisX = Color::rgb(0xE05555);
            p.axisY = Color::rgb(0x55C055);
            p.axisZ = Color::rgb(0x5580E0);
            themes.push_back(t);
        }

        // -------------------------------------------------------------
        // Mitternacht — dunkler und kuehler, fuer abends.
        {
            Theme t;
            t.id = "midnight";
            t.nameId = i18n::Str::ThemeMidnight;
            t.dark = true;
            Palette& p = t.palette;
            p.windowBg = Color::rgb(0x14171F);
            p.panelBg = Color::rgb(0x1A1E28);
            p.headerBg = Color::rgb(0x222733);
            p.popupBg = Color::rgb(0x1A1E28);
            p.text = Color::rgb(0xDCE1EA);
            p.textDim = Color::rgb(0x8B94A6);
            p.textOnAccent = Color::rgb(0x0E1017);
            p.control = Color::rgb(0x2A303D);
            p.controlHover = Color::rgb(0x353D4D);
            p.controlActive = Color::rgb(0x414B5E);
            p.border = Color::rgb(0x2F3644);
            p.accent = Color::rgb(0x6FA8DC);
            p.accentHover = Color::rgb(0x84B8E8);
            p.error = Color::rgb(0xEF8A8A);
            p.warning = Color::rgb(0xE3BE6E);
            p.info = Color::rgb(0x8FC4EC);
            p.ok = Color::rgb(0x86CE92);
            p.viewportBg = Color::rgb(0x0C0E14);
            p.roomWall = Color::rgb(0x2E3B44);
            // Der Himmel folgt der Grundstimmung des Themas.
            p.skyHorizon = shadeOf(p.roomWall, 1.25f, 0.55f);
            p.skyZenith = shadeOf(p.roomWall, 0.55f, 0.85f);
            p.sunGlow = shadeOf(p.roomWall, 2.2f, 0.15f);
            p.grid = Color::rgb(0x3E4757);
            p.axisX = Color::rgb(0xE06060);
            p.axisY = Color::rgb(0x60C860);
            p.axisZ = Color::rgb(0x6088E8);
            themes.push_back(t);
        }

        // -------------------------------------------------------------
        // Raven Klassik — die Farben aus den Bildschirmfotos von 2003.
        //
        // Grauwerte des Windows-Systemthemas jener Zeit, Testraum im
        // gleichen Gruen. Fuer alle, die den Editor seit zwanzig Jahren
        // benutzen und sich nicht umgewoehnen wollen.
        {
            Theme t;
            t.id = "classic";
            t.nameId = i18n::Str::ThemeClassic;
            t.dark = false;
            Palette& p = t.palette;
            p.windowBg = Color::rgb(0xF0F0F0);
            p.panelBg = Color::rgb(0xF0F0F0);
            p.headerBg = Color::rgb(0xE4E4E4);
            p.popupBg = Color::rgb(0xFFFFFF);
            p.text = Color::rgb(0x000000);
            p.textDim = Color::rgb(0x646464);
            p.textOnAccent = Color::rgb(0xFFFFFF);
            p.control = Color::rgb(0xFFFFFF);
            p.controlHover = Color::rgb(0xE8F0FA);
            p.controlActive = Color::rgb(0xCCE4F7);
            p.border = Color::rgb(0xA0A0A0);
            p.accent = Color::rgb(0x0A5FA5);
            p.accentHover = Color::rgb(0x1373C4);
            p.error = Color::rgb(0xB00000);
            p.warning = Color::rgb(0x8A6100);
            p.info = Color::rgb(0x0A5FA5);
            p.ok = Color::rgb(0x1E7A2E);
            // Genau das Gruen aus den Bildschirmfotos.
            p.viewportBg = Color::rgb(0x4CAF50);
            p.roomWall = Color::rgb(0x66BB6A);
            // Der Himmel folgt der Grundstimmung des Themas.
            p.skyHorizon = shadeOf(p.roomWall, 1.25f, 0.55f);
            p.skyZenith = shadeOf(p.roomWall, 0.55f, 0.85f);
            p.sunGlow = shadeOf(p.roomWall, 2.2f, 0.15f);
            p.grid = Color::rgb(0x2E7D32);
            p.axisX = Color::rgb(0xFF0000);
            p.axisY = Color::rgb(0xFFFFFF);
            p.axisZ = Color::rgb(0x0000FF);
            themes.push_back(t);
        }

        // -------------------------------------------------------------
        // Hell — modernes helles Thema, nicht das Grau von 2003.
        {
            Theme t;
            t.id = "light";
            t.nameId = i18n::Str::ThemeLight;
            t.dark = false;
            Palette& p = t.palette;
            p.windowBg = Color::rgb(0xFAFAFA);
            p.panelBg = Color::rgb(0xFFFFFF);
            p.headerBg = Color::rgb(0xEFEFF2);
            p.popupBg = Color::rgb(0xFFFFFF);
            p.text = Color::rgb(0x1A1A1E);
            p.textDim = Color::rgb(0x5F5F68);
            p.textOnAccent = Color::rgb(0xFFFFFF);
            p.control = Color::rgb(0xFFFFFF);
            p.controlHover = Color::rgb(0xEDF3FB);
            p.controlActive = Color::rgb(0xD8E7F8);
            p.border = Color::rgb(0xC8C8CE);
            p.accent = Color::rgb(0x1565A8);
            p.accentHover = Color::rgb(0x1976C4);
            p.error = Color::rgb(0xB3261E);
            p.warning = Color::rgb(0x8A5A00);
            p.info = Color::rgb(0x1565A8);
            p.ok = Color::rgb(0x1B6E2C);
            p.viewportBg = Color::rgb(0xD8DCE0);
            p.roomWall = Color::rgb(0xB8C4BC);
            // Der Himmel folgt der Grundstimmung des Themas.
            p.skyHorizon = shadeOf(p.roomWall, 1.25f, 0.55f);
            p.skyZenith = shadeOf(p.roomWall, 0.55f, 0.85f);
            p.sunGlow = shadeOf(p.roomWall, 2.2f, 0.15f);
            p.grid = Color::rgb(0x8A8A94);
            p.axisX = Color::rgb(0xC62828);
            p.axisY = Color::rgb(0x2E7D32);
            p.axisZ = Color::rgb(0x1565C0);
            themes.push_back(t);
        }

        // -------------------------------------------------------------
        // Solarized Dunkel — Ethan Schoonovers Palette, die festen Werte.
        // base03 als Flaeche, base0 als Schrift.
        {
            Theme t;
            t.id = "solarized-dark";
            t.nameId = i18n::Str::ThemeSolarizedDark;
            t.dark = true;
            Palette& p = t.palette;
            p.windowBg = Color::rgb(0x002B36);   // base03
            p.panelBg = Color::rgb(0x073642);    // base02
            p.headerBg = Color::rgb(0x073642);
            p.popupBg = Color::rgb(0x073642);
            p.text = Color::rgb(0x93A1A1);       // base1, nicht base0:
                                                 // base0 schafft 4.5:1 nicht
            p.textDim = Color::rgb(0x657B83);    // base00
            p.textOnAccent = Color::rgb(0x002B36);
            p.control = Color::rgb(0x0B4451);
            p.controlHover = Color::rgb(0x115362);
            p.controlActive = Color::rgb(0x186273);
            p.border = Color::rgb(0x0F4A58);
            p.accent = Color::rgb(0x268BD2);     // blue
            p.accentHover = Color::rgb(0x389CE3);
            p.error = Color::rgb(0xDC322F);      // red
            p.warning = Color::rgb(0xB58900);    // yellow
            p.info = Color::rgb(0x2AA198);       // cyan
            p.ok = Color::rgb(0x859900);         // green
            p.viewportBg = Color::rgb(0x001F27);
            p.roomWall = Color::rgb(0x0A4A4A);
            // Der Himmel folgt der Grundstimmung des Themas.
            p.skyHorizon = shadeOf(p.roomWall, 1.25f, 0.55f);
            p.skyZenith = shadeOf(p.roomWall, 0.55f, 0.85f);
            p.sunGlow = shadeOf(p.roomWall, 2.2f, 0.15f);
            p.grid = Color::rgb(0x30606A);
            p.axisX = Color::rgb(0xDC322F);
            p.axisY = Color::rgb(0x859900);
            p.axisZ = Color::rgb(0x268BD2);
            themes.push_back(t);
        }

        // -------------------------------------------------------------
        // Hoher Kontrast — fuer helle Raeume und muede Augen. Reines
        // Schwarz und reines Weiss, Bedeutungsfarben deutlich aufgehellt.
        {
            Theme t;
            t.id = "high-contrast";
            t.nameId = i18n::Str::ThemeHighContrast;
            t.dark = true;
            Palette& p = t.palette;
            p.windowBg = Color::rgb(0x000000);
            p.panelBg = Color::rgb(0x0A0A0A);
            p.headerBg = Color::rgb(0x161616);
            p.popupBg = Color::rgb(0x0A0A0A);
            p.text = Color::rgb(0xFFFFFF);
            p.textDim = Color::rgb(0xC0C0C0);
            p.textOnAccent = Color::rgb(0x000000);
            p.control = Color::rgb(0x1E1E1E);
            p.controlHover = Color::rgb(0x2E2E2E);
            p.controlActive = Color::rgb(0x3E3E3E);
            p.border = Color::rgb(0x8C8C8C);
            p.accent = Color::rgb(0x35B4FF);
            p.accentHover = Color::rgb(0x5FC6FF);
            p.error = Color::rgb(0xFF7A7A);
            p.warning = Color::rgb(0xFFD24D);
            p.info = Color::rgb(0x66CCFF);
            p.ok = Color::rgb(0x66E07A);
            p.viewportBg = Color::rgb(0x000000);
            p.roomWall = Color::rgb(0x333333);
            // Der Himmel folgt der Grundstimmung des Themas.
            p.skyHorizon = shadeOf(p.roomWall, 1.25f, 0.55f);
            p.skyZenith = shadeOf(p.roomWall, 0.55f, 0.85f);
            p.sunGlow = shadeOf(p.roomWall, 2.2f, 0.15f);
            p.grid = Color::rgb(0x808080);
            p.axisX = Color::rgb(0xFF4444);
            p.axisY = Color::rgb(0x44FF44);
            p.axisZ = Color::rgb(0x6699FF);
            themes.push_back(t);
        }

        // Jedes mitgelieferte Thema laeuft durch dieselbe Korrektur wie ein
        // selbst zusammengestelltes. Das haelt die Tabelle oben lesbar: dort
        // stehen die gewuenschten Farben, hier wird sichergestellt, dass sie
        // auch tragen.
        for (auto& t : themes) enforceReadability(t.palette);

        return themes;
    }();
    return kThemes;
}

const Theme* findTheme(const std::string& id) {
    for (const auto& t : builtinThemes()) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

namespace {

void toHsl(const Color& c, float& h, float& sat, float& l) {
    float mx = std::max({c.r, c.g, c.b});
    float mn = std::min({c.r, c.g, c.b});
    l = (mx + mn) * 0.5f;
    float d = mx - mn;
    if (d < 1e-6f) {
        h = 0.0f;
        sat = 0.0f;
        return;
    }
    sat = l > 0.5f ? d / (2.0f - mx - mn) : d / (mx + mn);
    if (mx == c.r) h = (c.g - c.b) / d + (c.g < c.b ? 6.0f : 0.0f);
    else if (mx == c.g) h = (c.b - c.r) / d + 2.0f;
    else h = (c.r - c.g) / d + 4.0f;
    h /= 6.0f;
}

float hueToRgb(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

Color fromHsl(float h, float sat, float l, float a) {
    if (sat < 1e-6f) return Color{l, l, l, a};
    float q = l < 0.5f ? l * (1.0f + sat) : l + sat - l * sat;
    float p = 2.0f * l - q;
    return Color{hueToRgb(p, q, h + 1.0f / 3.0f), hueToRgb(p, q, h),
                 hueToRgb(p, q, h - 1.0f / 3.0f), a};
}

}  // namespace

bool ensureContrast(Color& foreground, const Color& background, float required) {
    if (contrastRatio(foreground, background) >= required) return false;

    float h, sat, l;
    toHsl(foreground, h, sat, l);

    // In welche Richtung? Nicht ueber die Helligkeit des Hintergrunds
    // entscheiden — bei mittleren Toenen wie einem Blau liegt die Schwelle
    // falsch und man laeuft gegen Weiss, obwohl Schwarz besser traegt.
    // Stattdessen beide Enden messen und das bessere nehmen.
    const Color white{1.0f, 1.0f, 1.0f, foreground.a};
    const Color black{0.0f, 0.0f, 0.0f, foreground.a};
    bool lighten = contrastRatio(white, background) >=
                   contrastRatio(black, background);

    // Binaere Suche auf der Helligkeit. Sechzehn Schritte reichen fuer eine
    // Genauigkeit weit unter einer Stufe von 255.
    float low = lighten ? l : 0.0f;
    float high = lighten ? 1.0f : l;
    Color best = lighten ? white : black;

    if (contrastRatio(best, background) < required) {
        // Selbst Weiss beziehungsweise Schwarz reicht nicht — dann ist der
        // Hintergrund das Problem, nicht die Schrift. Wir nehmen das
        // Erreichbare und melden es ueber checkContrast weiter.
        foreground = best;
        return true;
    }

    for (int i = 0; i < 16; ++i) {
        float mid = (low + high) * 0.5f;
        Color candidate = fromHsl(h, sat, mid, foreground.a);
        if (contrastRatio(candidate, background) >= required) {
            best = candidate;
            if (lighten) high = mid; else low = mid;
        } else {
            if (lighten) low = mid; else high = mid;
        }
    }

    foreground = best;
    return true;
}

std::vector<Adjustment> enforceReadability(Palette& p) {
    std::vector<Adjustment> changed;

    struct Rule {
        const char* name;
        Color* fg;
        const Color* bg;
        float required;
    };

    const Rule rules[] = {
        {"Text auf Fensterflaeche", &p.text, &p.windowBg, 4.5f},
        {"Text auf Blattflaeche", &p.text, &p.panelBg, 4.5f},
        {"Text auf Leistenflaeche", &p.text, &p.headerBg, 4.5f},
        {"Text im Eingabefeld", &p.text, &p.control, 4.5f},
        {"Fehlerfarbe auf Blattflaeche", &p.error, &p.panelBg, 4.5f},
        {"Warnfarbe auf Blattflaeche", &p.warning, &p.panelBg, 4.5f},
        {"Hinweisfarbe auf Blattflaeche", &p.info, &p.panelBg, 4.5f},
        {"Erfolgsfarbe auf Blattflaeche", &p.ok, &p.panelBg, 4.5f},
        {"Abgeschaltete Schrift", &p.textDim, &p.panelBg, 3.0f},
        {"Rahmen auf Fensterflaeche", &p.border, &p.windowBg, 1.8f},
        {"Betonung auf Fensterflaeche", &p.accent, &p.windowBg, 3.0f},
        // Erst nachdem die Betonungsfarbe endgueltig feststeht, hat es Sinn,
        // die Schrift darauf zu pruefen.
        {"Text auf Betonungsfarbe", &p.textOnAccent, &p.accent, 4.5f},
        {"Gitter im Ansichtsfenster", &p.grid, &p.viewportBg, 1.8f},
    };

    for (const auto& rule : rules) {
        float before = contrastRatio(*rule.fg, *rule.bg);
        if (ensureContrast(*rule.fg, *rule.bg, rule.required)) {
            changed.push_back({rule.name, before, contrastRatio(*rule.fg, *rule.bg)});
        }
    }

    return changed;
}

std::vector<ContrastIssue> checkContrast(const std::vector<Theme>& themes) {
    std::vector<ContrastIssue> issues;

    for (const auto& t : themes) {
        const Palette& p = t.palette;

        struct Pair {
            const char* name;
            const Color* fg;
            const Color* bg;
            float required;
        };

        // 4.5:1 fuer alles, was gelesen wird. 3:1 fuer Umrisse und fuer
        // abgeschaltete Schrift, die absichtlich zuruecktritt.
        const Pair pairs[] = {
            {"Text auf Fensterflaeche", &p.text, &p.windowBg, 4.5f},
            {"Text auf Blattflaeche", &p.text, &p.panelBg, 4.5f},
            {"Text auf Leistenflaeche", &p.text, &p.headerBg, 4.5f},
            {"Text im Eingabefeld", &p.text, &p.control, 4.5f},
            {"Fehlerfarbe auf Blattflaeche", &p.error, &p.panelBg, 4.5f},
            {"Warnfarbe auf Blattflaeche", &p.warning, &p.panelBg, 4.5f},
            {"Hinweisfarbe auf Blattflaeche", &p.info, &p.panelBg, 4.5f},
            {"Erfolgsfarbe auf Blattflaeche", &p.ok, &p.panelBg, 4.5f},
            {"Abgeschaltete Schrift", &p.textDim, &p.panelBg, 3.0f},
            {"Rahmen auf Fensterflaeche", &p.border, &p.windowBg, 1.8f},
            {"Betonung auf Fensterflaeche", &p.accent, &p.windowBg, 3.0f},
            {"Text auf Betonungsfarbe", &p.textOnAccent, &p.accent, 4.5f},
            {"Gitter im Ansichtsfenster", &p.grid, &p.viewportBg, 1.8f},
        };

        for (const auto& pair : pairs) {
            float ratio = contrastRatio(*pair.fg, *pair.bg);
            if (ratio < pair.required) {
                issues.push_back({t.name(), pair.name, ratio, pair.required});
            }
        }
    }

    return issues;
}

}  // namespace efx::theme
