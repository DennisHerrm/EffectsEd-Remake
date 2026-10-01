// selbsttest.cpp — efxed prueft seine eigene Oberflaeche.
//
// Aufruf:  set EFXED_SELBSTTEST=alles  und efxed.exe starten.
//          Andere Werte waehlen einen Teil (siehe `teile()` unten), mehrere
//          mit Komma: EFXED_SELBSTTEST=segmente,speichern
//
// Was der Test tut: er klickt mit simulierter Maus auf Menues, Knoepfe und
// Felder, tippt Werte ein und prueft danach das DATENMODELL — steht im Effekt,
// was der Knopf verspricht? Gefunden wird jedes Element ueber seine
// Beschriftung (oder eine Testmarke, siehe testmarke.h) und an der Stelle
// geklickt, an der ImGui es gezeichnet hat. Ein Knopf, der verdeckt oder
// abgeschnitten ist, faellt damit ebenfalls auf.
//
// Vorbild ist der Selbsttest von behaved. Woher die Elemente kommen: mit
// IMGUI_ENABLE_TEST_ENGINE (gui/imgui_config.h) meldet ImGui jedes Element an
// die Haken ganz unten in dieser Datei — aber nur, solange
// `TestEngineHookItems` gesetzt ist, und das geschieht ausschliesslich hier.
//
// Was NICHT geklickt wird, ohne es vorher umzuleiten: alles, was ein
// Windows-Fenster oeffnet (Dateidialog, Ordnerauswahl, Browser). Der Test
// ersetzt diese Rueckrufe durch eigene, die einen vorbereiteten Pfad liefern —
// so werden Oeffnen und Speichern trotzdem echt durchlaufen.
//
// Ergebnis: Zeilen "Selbsttest OK/FEHLER" im Startprotokoll und eine
// Zusammenfassung in selbsttest_ergebnis.txt im (umgelenkten)
// Einstellungsordner. Danach beendet sich das Programm selbst.
#include "selbsttest.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "app.h"
#include "efx/diag.h"
#include "efx/fields.h"
#include "efx/i18n.h"
#include "efx/image.h"
#include "efx/io.h"
#include "efx/paths.h"
#include "efx/theme.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "testmarke.h"
#include "theme_imgui.h"

namespace fs = std::filesystem;

namespace efx::gui {

using i18n::Str;
using i18n::tr;

// ===========================================================================
// Was ImGui in diesem Bild gezeichnet hat
// ===========================================================================
namespace {

struct Element {
    ImGuiID id = 0;
    std::string label;   // leer, wenn nur ItemAdd kam
    std::string marke;   // Testmarke, falls gesetzt
    ImRect rect;
    ImRect clip;
    std::string fenster;  // oberstes Fenster
    std::string innen;    // Fenster, in dem es steht
    bool gesperrt = false;  // BeginDisabled: ein Klick darf nichts tun
    bool ankreuz = false;   // ein Haekchen (Checkbox)
    bool ohneLage = false;  // Beschriftung kam vor dem Rechteck (Reiter)
};

std::vector<Element> g_elemente;
bool g_an = false;
std::vector<std::string> g_bereiche;

bool g_mausAn = false;
ImVec2 g_maus{};

void setzeMaus(ImVec2 p) {
    g_mausAn = true;
    g_maus = p;
    ImGui::GetIO().AddMousePosEvent(p.x, p.y);
}

std::string sichtbarerText(const std::string& label) {
    const size_t ende = label.find("##");
    return ende == std::string::npos ? label : label.substr(0, ende);
}

}  // namespace

void selbsttestMerkeElement(ImGuiContext* ctx, ImGuiID id, const ImRect& bb,
                            const ImGuiLastItemData* data = nullptr) {
    if (ctx->CurrentWindow == nullptr) return;
    // Reiter (TabItemEx) melden ihre Beschriftung VOR der Lage. Dann steht
    // das Element schon da, nur mit falschem Rechteck — hier nachtragen.
    for (auto it = g_elemente.rbegin(); it != g_elemente.rend() && it - g_elemente.rbegin() < 4;
         ++it) {
        if (it->id == id && it->ohneLage) {
            it->rect = bb;
            it->clip = ctx->CurrentWindow->ClipRect;
            it->ohneLage = false;
            it->gesperrt =
                ((data ? data->ItemFlags : ctx->CurrentItemFlags) & ImGuiItemFlags_Disabled) != 0;
            return;
        }
    }
    Element e;
    e.id = id;
    e.rect = bb;
    e.gesperrt = ((data ? data->ItemFlags : ctx->CurrentItemFlags) & ImGuiItemFlags_Disabled) != 0;
    e.clip = ctx->CurrentWindow->ClipRect;
    e.fenster = ctx->CurrentWindow->RootWindow ? ctx->CurrentWindow->RootWindow->Name
                                               : ctx->CurrentWindow->Name;
    e.innen = ctx->CurrentWindow->Name;
    g_elemente.push_back(std::move(e));
}

void selbsttestMerkeText(ImGuiContext* ctx, ImGuiID id, const char* label,
                         ImGuiItemStatusFlags flags) {
    if (label == nullptr) return;
    const bool ankreuz = (flags & ImGuiItemStatusFlags_Checkable) != 0;
    for (auto it = g_elemente.rbegin(); it != g_elemente.rend(); ++it) {
        if (it->id == id) {
            it->label = label;
            it->ankreuz = ankreuz;
            return;
        }
    }
    selbsttestMerkeElement(ctx, id, ctx->LastItemData.Rect);
    if (!g_elemente.empty()) {
        g_elemente.back().label = label;
        g_elemente.back().ankreuz = ankreuz;
        g_elemente.back().ohneLage = true;
    }
}

// --- Testmarken (testmarke.h) ---------------------------------------------
namespace testmarke {

bool aktiv() { return g_an; }
void bereichBetreten(const char* name) { g_bereiche.emplace_back(name); }
void bereichVerlassen() {
    if (!g_bereiche.empty()) g_bereiche.pop_back();
}

void marke(const char* name) {
    if (!g_an) return;
    std::string pfad;
    for (const auto& b : g_bereiche) {
        pfad += b;
        pfad += '/';
    }
    pfad += name;
    ImGuiContext& g = *GImGui;
    const ImGuiID id = g.LastItemData.ID;
    // Meist ist das Element schon gemeldet — dann nur die Marke nachtragen.
    for (auto it = g_elemente.rbegin(); it != g_elemente.rend(); ++it) {
        if (it->id == id && id != 0) {
            it->marke = pfad;
            return;
        }
    }
    selbsttestMerkeElement(&g, id, g.LastItemData.Rect);
    if (!g_elemente.empty()) g_elemente.back().marke = pfad;
}

}  // namespace testmarke

// ===========================================================================
// Der Test selbst. Eine Klasse, weil App sie als Freund kennt: die Schritte
// lesen und setzen das Innenleben (Dokument, Einstellungen) direkt.
// ===========================================================================
class Selbsttest {
public:
    struct Schritt {
        std::string text;
        std::function<bool(int)> tun;
    };

    static App* app;
    static render::Renderer* renderer;
    static std::vector<Schritt> schritte;
    static size_t schritt;
    static int imSchritt;
    static int bild;
    static int ok;
    static int fehler;
    static std::vector<std::string> fehlerListe;
    static std::string arbeitsOrdner;
    static std::string fotoOrdner;
    static std::deque<std::string> dateiAntworten;
    static std::vector<std::string> geoeffnetPerShell;
    static std::string aktuellerTeil;

    // --- Melden ------------------------------------------------------------
    static void meldeOk(const std::string& text) {
        ++ok;
        diag::info("Selbsttest OK: [" + aktuellerTeil + "] " + text);
    }
    static void meldeFehler(const std::string& text) {
        ++fehler;
        const std::string zeile = "[" + aktuellerTeil + "] " + text;
        fehlerListe.push_back(zeile);
        diag::warn("Selbsttest FEHLER: " + zeile);
    }
    static void pruefe(bool bedingung, const std::string& text) {
        if (bedingung) meldeOk(text); else meldeFehler(text);
    }

    // --- Finden ------------------------------------------------------------
    // `label` mit "*" am Ende: Anfang genuegt ("Undo*" findet "Undo: x").
    static const Element* finde(const std::string& label, const std::string& fenster,
                                int nte = 0) {
        int gezaehlt = 0;
        const bool praefix = !label.empty() && label.back() == '*';
        const std::string anfang = praefix ? label.substr(0, label.size() - 1) : label;
        for (const Element& e : g_elemente) {
            if (e.label.empty()) continue;
            if (!fenster.empty() && e.fenster.find(fenster) == std::string::npos &&
                e.innen.find(fenster) == std::string::npos) {
                continue;
            }
            if (praefix) {
                if (e.label.rfind(anfang, 0) != 0) continue;
            } else if (e.label != label && sichtbarerText(e.label) != label) {
                continue;
            }
            if (gezaehlt == nte) return &e;
            ++gezaehlt;
        }
        return nullptr;
    }
    static const Element* findeMarke(const std::string& marke, int nte = 0) {
        int gezaehlt = 0;
        for (const Element& e : g_elemente) {
            if (e.marke != marke) continue;
            if (gezaehlt == nte) return &e;
            ++gezaehlt;
        }
        return nullptr;
    }
    // Teilelemente innerhalb eines Elements (die drei Felder eines DragFloat3),
    // von links nach rechts.
    static std::vector<const Element*> innerhalb(const Element& gruppe) {
        std::vector<const Element*> teile;
        for (const Element& e : g_elemente) {
            if (&e == &gruppe || e.id == 0 || e.id == gruppe.id) continue;
            if (e.rect.Min.x >= gruppe.rect.Min.x - 0.5f &&
                e.rect.Max.x <= gruppe.rect.Max.x + 0.5f &&
                e.rect.Min.y >= gruppe.rect.Min.y - 0.5f &&
                e.rect.Max.y <= gruppe.rect.Max.y + 0.5f &&
                e.rect.GetWidth() < gruppe.rect.GetWidth() - 1.0f) {
                teile.push_back(&e);
            }
        }
        std::sort(teile.begin(), teile.end(), [](const Element* a, const Element* b) {
            return a->rect.Min.x < b->rect.Min.x;
        });
        return teile;
    }

    static bool ganzSichtbar(const Element& e) {
        const ImVec2 sp = ImGui::GetStyle().ItemSpacing;
        const float sx = sp.x * 0.5f + 0.5f;
        const float sy = sp.y * 0.5f + 0.5f;
        return e.clip.GetWidth() > 0.0f && e.clip.GetHeight() > 0.0f &&
               e.rect.Min.x >= e.clip.Min.x - sx && e.rect.Min.y >= e.clip.Min.y - sy &&
               e.rect.Max.x <= e.clip.Max.x + sx && e.rect.Max.y <= e.clip.Max.y + sy;
    }

    static std::string wasImGuiSieht() {
        ImGuiContext& g = *GImGui;
        auto name = [](ImGuiID id) -> std::string {
            if (id == 0) return "-";
            for (const Element& e : g_elemente) {
                if (e.id == id) {
                    return (e.label.empty() ? (e.marke.empty() ? "(ohne Namen)" : e.marke)
                                            : e.label) +
                           " in " + e.innen;
                }
            }
            return "?";
        };
        return "aktiv: " + name(g.ActiveId) + ", unter der Maus: " + name(g.HoveredId) +
               ", Fenster " + (g.HoveredWindow ? g.HoveredWindow->Name : "-");
    }

    // --- Grundschritte -----------------------------------------------------
    //
    // Ein Schritt laeuft ueber mehrere Bilder: tun(b) bekommt die Nummer des
    // Bildes im Schritt und meldet mit true, dass er fertig ist. Mausereignisse
    // brauchen ein Bild, bis ImGui sie sieht — deshalb Stelle, Druecken und
    // Loslassen in getrennten Bildern.

    // Klick auf ein Element. `suche` liefert es aus dem aktuellen Bild.
    // `griffX`: wie weit von links geklickt wird (Punkte); 0 = Mitte, hoechstens 40.
    static Schritt klickAuf(const std::string& was,
                            std::function<const Element*()> suche, int taste = 0,
                            bool doppelt = false, bool sichtbarPruefen = true,
                            float griffX = 0.0f) {
        const auto greife = [griffX](const ImRect& r) {
            return griffX > 0.0f ? std::min(griffX, r.GetWidth() - 2.0f)
                                 : std::min(r.GetWidth() * 0.5f, 40.0f);
        };
        auto kennung = std::make_shared<ImGuiID>(0);
        // Bilder, die das Hinrollen gekostet hat — danach beginnt der Klick neu.
        auto versatz = std::make_shared<int>(0);
        auto lage = std::make_shared<ImRect>();
        auto unruhig = std::make_shared<int>(0);
        return {was, [=](int bildImSchritt) {
                    ImGuiIO& io = ImGui::GetIO();
                    if (bildImSchritt == 0) {
                        *versatz = 0;
                        *unruhig = 0;
                    }
                    const int b = bildImSchritt - *versatz;
                    if (b < 0) return false;
                    if (b == 0) {
                        const Element* e = suche();
                        if (e == nullptr) {
                            meldeFehler(was + ": nicht gefunden");
                            return true;
                        }
                        // Liegt es ausserhalb des sichtbaren Bereichs eines
                        // rollbaren Fensters, erst hinrollen — wie ein Mensch
                        // es mit dem Mausrad tut. Hoechstens dreimal.
                        if (!ganzSichtbar(*e) && *versatz < 9) {
                            if (ImGuiWindow* w = ImGui::FindWindowByName(e->innen.c_str());
                                w != nullptr && w->ScrollMax.y > 0.0f) {
                                const float oben = w->InnerClipRect.Min.y;
                                const float unten = w->InnerClipRect.Max.y;
                                float ziel = w->Scroll.y;
                                if (e->rect.Min.y < oben) ziel -= (oben - e->rect.Min.y) + 8.0f;
                                if (e->rect.Max.y > unten) ziel += (e->rect.Max.y - unten) + 8.0f;
                                ImGui::SetScrollY(w, std::clamp(ziel, 0.0f, w->ScrollMax.y));
                                *versatz += 3;
                                return false;
                            }
                        }
                        if (sichtbarPruefen && !ganzSichtbar(*e)) {
                            char z[200];
                            std::snprintf(z, sizeof(z),
                                          ": nicht ganz sichtbar (%.0f,%.0f-%.0f,%.0f; "
                                          "sichtbar %.0f,%.0f-%.0f,%.0f)",
                                          double(e->rect.Min.x), double(e->rect.Min.y),
                                          double(e->rect.Max.x), double(e->rect.Max.y),
                                          double(e->clip.Min.x), double(e->clip.Min.y),
                                          double(e->clip.Max.x), double(e->clip.Max.y));
                            meldeFehler(was + z);
                        }
                        *kennung = e->id;
                        *lage = e->rect;
                        setzeMaus(ImVec2(e->rect.Min.x + greife(e->rect),
                                         e->rect.GetCenter().y));
                        return false;
                    }
                    if (b == 1) {
                        // Steht das Ziel noch da, wo es eben war? Reiter einer
                        // zu schmalen Leiste rueckten im ersten Bild nach einer
                        // Aenderung — der Klick traf dann den Nachbarn.
                        const Element* jetzt = suche();
                        if (jetzt != nullptr && *versatz < 30 &&
                            (std::fabs(jetzt->rect.Min.x - lage->Min.x) > 0.5f ||
                             std::fabs(jetzt->rect.Min.y - lage->Min.y) > 0.5f)) {
                            ++*unruhig;
                            *lage = jetzt->rect;
                            *kennung = jetzt->id;
                            setzeMaus(ImVec2(jetzt->rect.Min.x + greife(jetzt->rect),
                                             jetzt->rect.GetCenter().y));
                            *versatz += 1;
                            return false;
                        }
                        if (*unruhig > 0) {
                            diag::info("Selbsttest: " + was + ": Ziel rueckte " +
                                       std::to_string(*unruhig) + "x vor dem Klick");
                        }
                        io.AddMouseButtonEvent(taste, true);
                        return false;
                    }
                    if (b == 2) {
                        ImGuiContext& g = *GImGui;
                        const bool getroffen = taste == 0
                                                   ? (g.ActiveId == *kennung ||
                                                      g.ActiveIdPreviousFrame == *kennung ||
                                                      g.HoveredIdPreviousFrame == *kennung)
                                                   : g.HoveredIdPreviousFrame == *kennung;
                        if (!getroffen && *kennung != 0) {
                            char z[160];
                            std::snprintf(z, sizeof(z), "; Maus %.0f/%.0f, Ziel %.0f..%.0f/%.0f..%.0f",
                                          double(g_maus.x), double(g_maus.y), double(lage->Min.x),
                                          double(lage->Max.x), double(lage->Min.y),
                                          double(lage->Max.y));
                            meldeFehler(was + ": Klick kam nicht an (" + wasImGuiSieht() + ")" + z);
                            for (const Element& x : g_elemente) {
                                if (GImGui->HoveredWindow && x.innen == GImGui->HoveredWindow->Name && !x.label.empty()) {
                                    std::snprintf(z, sizeof(z), "  Element \"%s\" %.0f..%.0f/%.0f..%.0f",
                                                  x.label.c_str(), double(x.rect.Min.x),
                                                  double(x.rect.Max.x), double(x.rect.Min.y),
                                                  double(x.rect.Max.y));
                                    diag::info(z);
                                }
                            }
                        }
                        io.AddMouseButtonEvent(taste, false);
                        return false;
                    }
                    if (doppelt) {
                        if (b == 3) {
                            io.AddMouseButtonEvent(taste, true);
                            return false;
                        }
                        if (b == 4) {
                            io.AddMouseButtonEvent(taste, false);
                            return false;
                        }
                        return b >= 7;
                    }
                    return b >= 5;
                }};
    }

    static Schritt klick(const std::string& label, const std::string& fenster = "",
                         int nte = 0, int taste = 0) {
        return klickAuf("Klick \"" + sichtbarerText(label) + "\"" +
                            (fenster.empty() ? "" : " in " + fenster),
                        [=] { return finde(label, fenster, nte); }, taste);
    }
    // Beschriftung erst beim Klicken uebersetzen - nach einem Sprachwechsel
    // heisst "View" ploetzlich "Ansicht".
    static Schritt klickTr(Str text, const std::string& fenster) {
        return klickAuf(std::string("Klick tr(") + std::to_string(static_cast<int>(text)) + ") in " + fenster,
                        [=] { return finde(tr(text), fenster); });
    }
    static Schritt klickMarke(const std::string& marke, int nte = 0, bool doppelt = false) {
        return klickAuf(std::string(doppelt ? "Doppelklick" : "Klick") + " Marke " + marke,
                        [=] { return findeMarke(marke, nte); }, 0, doppelt);
    }
    // Ein Teilfeld (0..2) eines DragFloat3 doppelt anklicken.
    static Schritt doppelklickTeil(const std::string& marke, int teil) {
        return klickAuf("Doppelklick Marke " + marke + "[" + std::to_string(teil) + "]",
                        [=]() -> const Element* {
                            const Element* g = findeMarke(marke);
                            if (!g) return nullptr;
                            const auto t = innerhalb(*g);
                            return teil < static_cast<int>(t.size()) ? t[size_t(teil)] : nullptr;
                        },
                        0, true);
    }

    // Menue: oben anklicken, dann den Eintrag. Untermenues ueber `zwischen`.
    static void menue(std::vector<Schritt>& s, Str oben, const std::string& eintrag,
                      const std::string& zwischen = "") {
        s.push_back(klick(tr(oben), "##main"));
        if (!zwischen.empty()) s.push_back(klick(zwischen, "##Menu"));
        s.push_back(klick(eintrag, "##Menu"));
        // Ein gesperrter Eintrag laesst das Menue offen — dann aufraeumen,
        // sonst faengt das Menue den naechsten Klick. Ein Dialog, den der
        // Eintrag geoeffnet hat, bleibt offen.
        s.push_back(menuesZu());
    }
    static void menue(std::vector<Schritt>& s, Str oben, Str eintrag) {
        menue(s, oben, tr(eintrag));
    }
    static void menue(std::vector<Schritt>& s, Str oben, Str zwischen, Str eintrag) {
        menue(s, oben, tr(eintrag), tr(zwischen));
    }

    static Schritt taste(ImGuiKey key, bool ctrl = false, bool shift = false,
                         bool alt = false) {
        const std::string name = std::string(ctrl ? "Strg+" : "") + (shift ? "Umschalt+" : "") +
                                 (alt ? "Alt+" : "") + ImGui::GetKeyName(key);
        return {"Taste " + name, [=](int b) {
                    ImGuiIO& io = ImGui::GetIO();
                    if (b == 0) {
                        if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl, true);
                        if (shift) io.AddKeyEvent(ImGuiMod_Shift, true);
                        if (alt) io.AddKeyEvent(ImGuiMod_Alt, true);
                        io.AddKeyEvent(key, true);
                        return false;
                    }
                    if (b == 1) {
                        io.AddKeyEvent(key, false);
                        if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl, false);
                        if (shift) io.AddKeyEvent(ImGuiMod_Shift, false);
                        if (alt) io.AddKeyEvent(ImGuiMod_Alt, false);
                        return false;
                    }
                    return b >= 3;
                }};
    }
    static Schritt tippe(const std::string& text) {
        return {"Tippe \"" + text + "\"", [=](int b) {
                    if (b == 0) {
                        ImGui::GetIO().AddInputCharactersUTF8(text.c_str());
                        return false;
                    }
                    return b >= 2;
                }};
    }
    // Ein Zahlenfeld (DragFloat) ueber Doppelklick in Texteingabe versetzen,
    // alles ersetzen, Enter.
    static void feld(std::vector<Schritt>& s, const std::string& marke, const std::string& text,
                     int teil = -1) {
        if (marke.find("intensity") != std::string::npos) {
            // Senkrechte Regler (VSliderFloat) haben in ImGui keine
            // Texteingabe - ein Klick setzt den Wert an der Mausstelle.
            s.push_back(klickAuf("Klick oben in Regler " + marke, [=]() -> const Element* {
                static Element oben;
                const Element* e = findeMarke(marke);
                if (!e) return nullptr;
                // Nur das obere Fuenftel: die Mitte (8) ist der Vorgabewert.
                oben = *e;
                // Max nach Min: Min hat Max schon auf seinen Wert gezogen
                // (Min <= Max), also fuer Max noch weiter oben.
                const float anteil = marke.find("max") != std::string::npos ? 0.12f : 0.4f;
                oben.rect.Max.y = e->rect.Min.y + e->rect.GetHeight() * anteil;
                return &oben;
            }));
            return;
        }
        s.push_back(teil < 0 ? klickMarke(marke, 0, true) : doppelklickTeil(marke, teil));
        s.push_back(taste(ImGuiKey_A, true));
        s.push_back(tippe(text));
        s.push_back(taste(ImGuiKey_Enter));
    }
    static Schritt warte(int bilder, const std::string& was = "") {
        return {was.empty() ? "Warte " + std::to_string(bilder) + " Bilder" : was,
                [=](int b) { return b >= bilder; }};
    }
    static Schritt warteBis(const std::string& was, std::function<bool()> bedingung,
                            int maxBilder = 600) {
        return {"Warte bis " + was, [=](int b) {
                    if (bedingung()) return true;
                    if (b >= maxBilder) {
                        meldeFehler("Zeitueberschreitung: " + was);
                        return true;
                    }
                    return false;
                }};
    }
    static Schritt pruefSchritt(const std::string& text, std::function<bool()> bedingung) {
        return {"Pruefe " + text, [=](int) {
                    pruefe(bedingung(), text);
                    return true;
                }};
    }
    static Schritt tu(const std::string& text, std::function<void()> f) {
        return {text, [=](int) {
                    f();
                    return true;
                }};
    }
    static Schritt teil(const std::string& name) {
        return {"=== Teil " + name, [=](int) {
                    aktuellerTeil = name;
                    diag::info("Selbsttest: === " + name + " ===");
                    return true;
                }};
    }
    // Kein Popup und kein Menue offen? Sonst faengt das naechste Klicken ins
    // Leere — und der Test meldete zwanzig Folgefehler fuer einen.
    // `melden`: ein offenes Fenster ist ein Fehler (sonst nur aufraeumen).
    // Nur Menues schliessen, offene Dialoge (modal) stehen lassen.
    static Schritt menuesZu() {
        return {"Menues schliessen", [=](int b) {
                    if (b == 0) ImGui::ClosePopupsExceptModals();
                    return b >= 2;
                }};
    }
    static Schritt allesZu(bool melden = false) {
        return {"Alles geschlossen?", [=](int b) {
                    ImGuiContext& g = *GImGui;
                    if (b == 0 && !g.OpenPopupStack.empty()) {
                        std::string offen;
                        for (const auto& p : g.OpenPopupStack) {
                            offen += p.Window ? p.Window->Name : "?";
                            offen += " ";
                        }
                        if (melden) meldeFehler("noch offen: " + offen);
                        ImGui::ClosePopupToLevel(0, true);
                    }
                    return b >= 2;
                }};
    }
    // Das ganze Fenster, in genau diesem Bild (siehe selbsttestNachZeichnen).
    static std::string fensterFotoName;
    // Ein Bildpunkt des fertigen Fensters, gelesen nach dem Zeichnen.
    static bool punktGewuenscht;
    static ImVec2 punktOrt;
    static int punktFarbe[3];
    // Liest die Farbe des Bildpunkts in der oberen linken Ecke der Ansicht
    // (ein Stueck nach innen, ausserhalb von Achsen und Effekt).
    static Schritt leseAnsichtsEcke() {
        return {"Ansichtsecke lesen", [](int b) {
                    if (b == 0) {
                        const Element* e = findeMarke("ansicht");
                        if (!e) {
                            punktFarbe[0] = punktFarbe[1] = punktFarbe[2] = -1;
                            return true;
                        }
                        punktOrt = ImVec2(e->rect.Min.x + 6.0f, e->rect.Min.y + 6.0f);
                        punktGewuenscht = true;
                        return false;
                    }
                    return !punktGewuenscht || b > 3;
                }};
    }
    static Schritt fensterFoto(const std::string& name) {
        return {"Fensterfoto " + name, [=](int b) {
                    if (fotoOrdner.empty()) return true;
                    if (b == 0) {
                        fensterFotoName = name;
                        return false;
                    }
                    return fensterFotoName.empty() || b > 3;
                }};
    }
    static Schritt foto(const std::string& name) {
        return {"Foto " + name, [=](int) {
                    if (fotoOrdner.empty() || renderer == nullptr) return true;
                    std::vector<unsigned char> rgba;
                    int w = 0, h = 0;
                    if (!renderer->readViewport(rgba, w, h)) {
                        meldeFehler("Foto " + name + ": Ansicht nicht lesbar");
                        return true;
                    }
                    const auto tga = image::encodeTga(rgba.data(), w, h);
                    std::ofstream f(fotoOrdner + "/" + name + ".tga", std::ios::binary);
                    f.write(reinterpret_cast<const char*>(tga.data()),
                            static_cast<std::streamsize>(tga.size()));
                    return true;
                }};
    }

    // --- Zugriff auf das Programm --------------------------------------------
    static Document& doc() { return app->doc(); }
    static Effect& effekt() { return app->doc().effect; }
    static int anzahlSegmente() { return static_cast<int>(effekt().primitives.size()); }
    static Primitive* gewaehlt() {
        const int i = doc().selectedPrimitive;
        if (i < 0 || i >= anzahlSegmente()) return nullptr;
        return &effekt().primitives[size_t(i)];
    }
    static std::string text() { return write(effekt()); }

    static std::string leseDatei(const std::string& pfad) {
        std::ifstream f(pfad, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
    static void schreibeDatei(const std::string& pfad, const std::string& inhalt) {
        std::ofstream f(pfad, std::ios::binary);
        f << inhalt;
    }

    // Das Datei-Oeffnen/Speichern des naechsten Klicks beantworten.
    static Schritt dateiAntwort(const std::string& pfad) {
        return tu("Dateidialog antwortet " + pfad, [=] { dateiAntworten.push_back(pfad); });
    }

    // ======================================================================
    // Die Teile
    // ======================================================================

    // Ein neues, leeres Dokument im aktiven Reiter.
    static void frischesDokument(std::vector<Schritt>& s) {
        s.push_back(allesZu());
        s.push_back(tu("frisches Dokument", [] {
            app->documents_.clear();
            app->documents_.emplace_back();
            app->activeDocument_ = 0;
            app->doc().undo.reset(app->doc().effect);
            app->showEditor();
        }));
        s.push_back(warte(3));
    }

    // Ein Segment ueber Effects > New Segment anlegen.
    static void neuesSegment(std::vector<Schritt>& s, PrimitiveType typ) {
        menue(s, Str::MenuEffects, Str::EffectsNewSegment);
        s.push_back(klick(typeName(typ), "###newsegment"));
        s.push_back(klick(tr(Str::MsgOk), "###newsegment"));
        s.push_back(warte(2));
    }

    static std::vector<Schritt> teilStart() {
        std::vector<Schritt> s;
        s.push_back(teil("start"));
        s.push_back(pruefSchritt("Programm laeuft, Hauptfenster gezeichnet",
                                 [] { return finde(tr(Str::MenuFile), "##main") != nullptr; }));
        s.push_back(pruefSchritt("Voreinstellung wie im Original: Repeat until stopped", [] {
            return app->playback_.mode == playback::RepeatMode::UntilStopped &&
                   doc().clock.endMode() == timeline::EndMode::Repeat;
        }));
        // Grundansicht wie nach dem Start, einmal im voreingestellten und
        // einmal im klassischen Thema (zum Vergleich mit dem Original).
        s.push_back(tu("Editor zeigen", [] { app->showEditor(); }));
        s.push_back(warte(5));
        s.push_back(fensterFoto("grundansicht"));
        s.push_back(foto("grundansicht_3d"));
        s.push_back(tu("Thema classic", [] {
            for (const auto& th : theme::builtinThemes()) {
                if (th.id == "classic") {
                    app->settings_.themeId = th.id;
                    applyTheme(th);
                    app->geometryDirty_ = true;
                }
            }
        }));
        s.push_back(warte(5));
        s.push_back(fensterFoto("grundansicht_classic"));
        s.push_back(foto("grundansicht_classic_3d"));
        // Alle fuenf Hauptmenues sind da und sichtbar.
        for (Str m : {Str::MenuFile, Str::MenuEdit, Str::MenuView, Str::MenuEffects,
                      Str::MenuHelp}) {
            s.push_back(klick(tr(m), "##main"));
            s.push_back(allesZu());
        }
        // Hilfezeile: Maus ueber "New" im Menue Datei -> Beschreibung unten links.
        s.push_back(klick(tr(Str::MenuFile), "##main"));
        s.push_back({"Maus ueber New", [](int b) {
                         if (const Element* e = finde(tr(Str::FileNew), "##Menu")) {
                             setzeMaus(e->rect.GetCenter());
                         }
                         return b >= 3;
                     }});
        s.push_back(pruefSchritt("Hilfezeile zeigt die Beschreibung von New", [] {
            return app->statusHint_ != nullptr &&
                   std::string(app->statusHint_) == tr(Str::HintFileNew);
        }));
        s.push_back(allesZu());
        return s;
    }

    static std::vector<Schritt> teilSegmente() {
        std::vector<Schritt> s;
        s.push_back(teil("segmente"));
        frischesDokument(s);
        // Jeden der dreizehn Typen ueber den Dialog anlegen.
        const PrimitiveType typen[] = {
            PrimitiveType::CameraShake, PrimitiveType::Cylinder, PrimitiveType::Decal,
            PrimitiveType::Electricity, PrimitiveType::Emitter, PrimitiveType::ScreenFlash,
            PrimitiveType::FxRunner, PrimitiveType::Light, PrimitiveType::Line,
            PrimitiveType::OrientedParticle, PrimitiveType::Particle, PrimitiveType::Sound,
            PrimitiveType::Tail,
        };
        int n = 0;
        for (PrimitiveType t : typen) {
            ++n;
            neuesSegment(s, t);
            s.push_back(pruefSchritt(std::string("Neues Segment ") + typeName(t) + " angelegt",
                                     [n, t] {
                                         return anzahlSegmente() == n && gewaehlt() &&
                                                gewaehlt()->type == t;
                                     }));
            s.push_back(pruefSchritt(std::string("Neues Segment ") + typeName(t) +
                                         " ist rueckgaengig machbar",
                                     [] { return doc().undo.canUndo(); }));
            s.push_back(pruefSchritt(std::string("Neues Segment ") + typeName(t) +
                                         " hat dieselben Vorgaben wie freshPrimitive",
                                     [t] {
                                         if (!gewaehlt()) return false;
                                         Effect a, b;
                                         a.primitives.push_back(*gewaehlt());
                                         b.primitives.push_back(freshPrimitive(t));
                                         return write(a) == write(b);
                                     }));
        }
        // Doppelklick im Dialog legt ebenfalls an — mit denselben Vorgaben.
        s.push_back(tu("Zaehler merken", [] {}));
        menue(s, Str::MenuEffects, Str::EffectsNewSegment);
        s.push_back(klickAuf("Doppelklick Particle",
                             [] { return finde(typeName(PrimitiveType::Particle), "###newsegment"); },
                             0, true));
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Doppelklick legt Particle an", [] {
            return anzahlSegmente() == 14 && gewaehlt() && gewaehlt()->type == PrimitiveType::Particle;
        }));
        s.push_back(pruefSchritt("Doppelklick: Vorgaben wie freshPrimitive", [] {
            if (!gewaehlt()) return false;
            Effect a, b;
            a.primitives.push_back(*gewaehlt());
            b.primitives.push_back(freshPrimitive(PrimitiveType::Particle));
            return write(a) == write(b);
        }));
        s.push_back(allesZu());

        // Klonen: Menue Edit > Clone.
        s.push_back(tu("Segment 3 waehlen", [] { doc().selectedPrimitive = 2; }));
        menue(s, Str::MenuEdit, Str::EditCloneEffect);
        s.push_back(pruefSchritt("Klonen haengt eine Kopie ans Ende und waehlt sie (wie im Original)", [] {
            return anzahlSegmente() == 15 && doc().selectedPrimitive == 14 &&
                   effekt().primitives[14].type == effekt().primitives[2].type &&
                   effekt().primitives[14].name.rfind(tr(Str::ListCopyOf), 0) == 0;
        }));
        // Loeschen ueber den Werkzeugknopf.
        s.push_back(klick("##delSegment", "##main"));
        s.push_back(pruefSchritt("Werkzeugknopf Loeschen entfernt das Segment",
                                 [] { return anzahlSegmente() == 14; }));
        // Loeschen ueber die Entf-Taste.
        s.push_back(taste(ImGuiKey_Delete));
        s.push_back(pruefSchritt("Entf entfernt das Segment", [] { return anzahlSegmente() == 13; }));
        // Rueckgaengig und Wiederholen.
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("Strg+Z holt es zurueck", [] { return anzahlSegmente() == 14; }));
        s.push_back(taste(ImGuiKey_Y, true));
        s.push_back(pruefSchritt("Strg+Y loescht es wieder", [] { return anzahlSegmente() == 13; }));
        s.push_back(taste(ImGuiKey_Backspace, false, false, true));
        s.push_back(pruefSchritt("Alt+Ruecktaste holt es zurueck (Accelerator des Originals)",
                                 [] { return anzahlSegmente() == 14; }));
        s.push_back(taste(ImGuiKey_Y, true));
        menue(s, Str::MenuEdit, std::string(tr(Str::EditUndo)) + "*");
        s.push_back(pruefSchritt("Edit > Undo holt es zurueck", [] { return anzahlSegmente() == 14; }));

        // Abgeschaltetes Segment bleibt abgeschaltet, wenn ein anderes geloescht wird.
        s.push_back(tu("Segment 5 abschalten, Segment 1 waehlen", [] {
            doc().segmentEnabled.assign(effekt().primitives.size(), true);
            if (doc().segmentEnabled.size() > 4) doc().segmentEnabled[4] = false;
            doc().selectedPrimitive = 0;
        }));
        s.push_back(klick("##delSegment", "##main"));
        s.push_back(pruefSchritt("Nach dem Loeschen von Segment 1 ist das vorher fuenfte "
                                 "(jetzt vierte) noch abgeschaltet",
                                 [] {
                                     return doc().segmentEnabled.size() == effekt().primitives.size() &&
                                            doc().segmentEnabled.size() > 3 &&
                                            !doc().segmentEnabled[3];
                                 }));
        // Strg+D klont wie der Menuepunkt.
        s.push_back(tu("Segment 1 waehlen", [] { doc().selectedPrimitive = 0; }));
        s.push_back(taste(ImGuiKey_D, true));
        s.push_back(pruefSchritt("Strg+D klont", [] { return anzahlSegmente() == 14; }));
        return s;
    }

    static std::vector<Schritt> teilSpeichern() {
        std::vector<Schritt> s;
        s.push_back(teil("speichern"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        neuesSegment(s, PrimitiveType::Line);
        const std::string pfad = arbeitsOrdner + "/speichern_test.efx";
        s.push_back(tu("alte Datei weg", [pfad] { std::error_code ec; fs::remove(pfad, ec); }));
        s.push_back(dateiAntwort(pfad));
        menue(s, Str::MenuFile, Str::FileSaveAs);
        s.push_back(pruefSchritt("Save As schreibt die Datei", [pfad] { return fs::exists(pfad); }));
        s.push_back(pruefSchritt("Save As: Inhalt = write(Effekt)",
                                 [pfad] { return leseDatei(pfad) == text(); }));
        s.push_back(pruefSchritt("Save As: Dokument heisst jetzt wie die Datei",
                                 [pfad] { return doc().filePath == pfad && !doc().dirty; }));
        // Aenderung, dann Strg+S — ohne Dialog in dieselbe Datei.
        s.push_back(tu("Segment 1 umbenennen", [] {
            if (!effekt().primitives.empty()) effekt().primitives[0].name = "umbenannt";
            doc().dirty = true;
        }));
        s.push_back(taste(ImGuiKey_S, true));
        s.push_back(pruefSchritt("Strg+S speichert ohne Dialog",
                                 [pfad] { return leseDatei(pfad).find("umbenannt") != std::string::npos; }));
        // Neu oeffnen ueber File > Open.
        frischesDokument(s);
        s.push_back(dateiAntwort(pfad));
        menue(s, Str::MenuFile, Str::FileOpen);
        s.push_back(pruefSchritt("File > Open laedt die Datei",
                                 [pfad] { return anzahlSegmente() == 2 && doc().filePath == pfad; }));
        s.push_back(pruefSchritt("Geoeffnet = gespeichert", [pfad] { return text() == leseDatei(pfad); }));
        s.push_back(pruefSchritt("Nach dem Oeffnen ist die Bearbeitungsansicht vorn",
                                 [] { return !app->startTabActive_; }));
        // Strg+O und Strg+N.
        s.push_back(dateiAntwort(pfad));
        s.push_back(taste(ImGuiKey_O, true));
        s.push_back(pruefSchritt("Strg+O oeffnet (zweiter Reiter)", [] {
            return app->documents_.size() >= 2 && anzahlSegmente() == 2;
        }));
        s.push_back(taste(ImGuiKey_N, true));
        s.push_back(pruefSchritt("Strg+N: neuer leerer Effekt", [] { return anzahlSegmente() == 0; }));
        // Datei mit der Toolbar oeffnen.
        s.push_back(dateiAntwort(pfad));
        s.push_back(klick("##open", "##main"));
        s.push_back(pruefSchritt("Werkzeugknopf Oeffnen laedt", [] { return anzahlSegmente() == 2; }));
        return s;
    }

    static std::vector<Schritt> teilAnsicht() {
        std::vector<Schritt> s;
        s.push_back(teil("ansicht"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        struct Umschalter { Str eintrag; bool layout::Settings::*wert; };
        const Umschalter liste[] = {
            {Str::ViewMainToolbar, &layout::Settings::showMainToolbar},
            {Str::ViewEffectsToolbar, &layout::Settings::showEffectsToolbar},
            {Str::ViewPlaybackToolbar, &layout::Settings::showPlaybackToolbar},
            {Str::ViewWorldToolbar, &layout::Settings::showWorldToolbar},
            {Str::ViewStatusBar, &layout::Settings::showStatusBar},
            {Str::ViewDrawAxes, &layout::Settings::drawAxes},
            {Str::ViewWindVector, &layout::Settings::drawWindVector},
            {Str::ViewDrawRoom, &layout::Settings::drawRoom},
            {Str::ViewDrawGrid, &layout::Settings::drawGrid},
        };
        for (const auto& u : liste) {
            auto vorher = std::make_shared<bool>(false);
            const std::string name = tr(u.eintrag);
            auto wert = u.wert;
            s.push_back(tu("merken " + name, [=] { *vorher = app->settings_.*wert; }));
            menue(s, Str::MenuView, u.eintrag);
            s.push_back(pruefSchritt("View > " + name + " schaltet um",
                                     [=] { return app->settings_.*wert != *vorher; }));
            menue(s, Str::MenuView, u.eintrag);
            s.push_back(pruefSchritt("View > " + name + " schaltet zurueck",
                                     [=] { return app->settings_.*wert == *vorher; }));
        }
        // Raumtextur und Darstellungsart.
        const Str texturen[] = {Str::TextureNone, Str::TextureBrick, Str::TextureDirt,
                                Str::TextureStucco};
        for (int i = 0; i < 4; ++i) {
            menue(s, Str::MenuView, Str::ViewTexturedRoom, texturen[i]);
            s.push_back(pruefSchritt(std::string("Textured Room ") + tr(texturen[i]),
                                     [i] { return app->settings_.roomTexture == i; }));
            s.push_back(warte(2));
            s.push_back(foto(std::string("raum_textur_") + std::to_string(i)));
        }
        // Render Options: drei unabhaengige Schalter wie im Original.
        struct Art { Str text; bool layout::Settings::*wert; };
        const Art arten[] = {{Str::RenderTextured, &layout::Settings::effectTextured},
                             {Str::RenderWireframe, &layout::Settings::effectWireframe},
                             {Str::RenderOverdraw, &layout::Settings::effectOverdraw}};
        for (const Art& a : arten) {
            auto vorher = std::make_shared<bool>(false);
            auto wert = a.wert;
            s.push_back(tu("merken", [=] { *vorher = app->settings_.*wert; }));
            menue(s, Str::MenuView, Str::ViewRenderOptions, a.text);
            s.push_back(pruefSchritt(std::string("Render Options ") + tr(a.text) + " schaltet um",
                                     [=] { return app->settings_.*wert != *vorher; }));
            s.push_back(warte(2));
            s.push_back(fensterFoto(std::string("render_") + tr(a.text)));
            menue(s, Str::MenuView, Str::ViewRenderOptions, a.text);
        }
        s.push_back(tu("Overdraw an", [] { app->settings_.effectOverdraw = true; }));
        s.push_back(klick(tr(Str::MenuView), "##main"));
        s.push_back(klick(tr(Str::ViewRenderOptions), "##Menu"));
        s.push_back(pruefSchritt("Overdraw sperrt Textured und Wireframe", [] {
            const Element* e = finde(tr(Str::RenderTextured), "##Menu");
            return e != nullptr && e->gesperrt;
        }));
        s.push_back(menuesZu());
        s.push_back(warte(2));
        s.push_back(leseAnsichtsEcke());
        s.push_back(pruefSchritt("Overdraw: schwarzer Hintergrund, kein Raum (wie im Original)", [] {
            return punktFarbe[0] == 0 && punktFarbe[1] == 0 && punktFarbe[2] == 0;
        }));
        s.push_back(tu("Overdraw aus", [] { app->settings_.effectOverdraw = false; }));
        s.push_back(warte(2));
        s.push_back(leseAnsichtsEcke());
        s.push_back(pruefSchritt("ohne Overdraw ist der Raum wieder da", [] {
            return punktFarbe[0] > 0 || punktFarbe[1] > 0 || punktFarbe[2] > 0;
        }));
        // Ausrichtung.
        const Str ausrichtung[] = {Str::EffectsOrientUp, Str::EffectsOrientSide,
                                   Str::EffectsOrientDown};
        for (int i = 2; i >= 0; --i) {
            menue(s, Str::MenuEffects, ausrichtung[i]);
            s.push_back(pruefSchritt(std::string("Effects > ") + tr(ausrichtung[i]),
                                     [i] { return app->settings_.orientation == i; }));
        }
        // Play Sounds umschalten.
        auto tonVorher = std::make_shared<bool>(false);
        s.push_back(tu("Ton merken", [=] { *tonVorher = app->settings_.playSounds; }));
        menue(s, Str::MenuEffects, Str::EffectsPlaySounds);
        s.push_back(pruefSchritt("Play Sounds schaltet um",
                                 [=] { return app->settings_.playSounds != *tonVorher; }));
        menue(s, Str::MenuEffects, Str::EffectsPlaySounds);
        // Ansicht zuruecksetzen.
        s.push_back(tu("Kamera verdrehen", [] { app->camera_.lookFrom(camera::Orbit::View::Top); }));
        menue(s, Str::MenuView, Str::ViewResetCamera);
        s.push_back(pruefSchritt("Reset View stellt die Kamera zurueck", [] {
            camera::Orbit frisch;
            frisch.reset(app->settings_.worldScale);
            return std::fabs(frisch.distance() - app->camera_.distance()) < 0.01f;
        }));
        return s;
    }

    // --- Nachlegen: Wiedergabe-Einstellungen und eigener Ursprung ----------
    static void feldIn(std::vector<Schritt>& s, const std::string& label, const std::string& fenster,
                       const std::string& text, int nte = 0) {
        s.push_back(klickAuf("Doppelklick " + label, [=] { return finde(label, fenster, nte); }, 0, true));
        s.push_back(taste(ImGuiKey_A, true));
        s.push_back(tippe(text));
        s.push_back(taste(ImGuiKey_Enter));
    }

    static void wiedergabeDialog(std::vector<Schritt>& s) {
        menue(s, Str::MenuEffects, Str::EffectsPlaybackSettings);
        s.push_back(warteBis("Wiedergabe offen", [] { return dialogOffen("###playback"); }, 30));
    }

    static std::vector<Schritt> teilNachlegen() {
        std::vector<Schritt> s;
        s.push_back(teil("nachlegen"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        s.push_back(tu("Lebensdauer 400 ms", [] {
            if (gewaehlt()) gewaehlt()->life = Range::single(400.0f);
            app->recordChange("test");
        }));

        // Eigener Ursprung "an der Decke": der Effekt entsteht bei z = 59.
        menue(s, Str::MenuEffects, Str::EffectsCustomOrigin);
        s.push_back(warteBis("Ursprung offen", [] { return dialogOffen("###spawnorigin"); }, 30));
        s.push_back(klick(tr(Str::OriginOnCeiling), "###spawnorigin"));
        s.push_back(klick(tr(Str::MsgOk), "###spawnorigin"));
        menue(s, Str::MenuEffects, Str::EffectsPlay);
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Eigener Ursprung an der Decke: der Effekt entsteht bei z = 59", [] {
            const auto& live = doc().particles.live();
            return !live.empty() && std::fabs(live[0].positionAt(live[0].spawnMs).z - 59.0f) < 0.01f;
        }));
        s.push_back(tu("Ursprung zurueck", [] {
            app->spawnOrigin_ = playback::Origin{};
            app->pressStop();
        }));

        // "Repeat for 1 seconds" mit 0.25 s: Ausloesungen bei 0, 250, 500,
        // 750 und 1000 ms (Total repetitions 5 im Original), danach haelt es an.
        wiedergabeDialog(s);
        s.push_back(klick(tr(Str::PlaybackForSeconds), "###playback"));
        feldIn(s, "##repeatFor", "###playback", "1");
        feldIn(s, "##rateField", "###playback", "0.25");
        s.push_back(fensterFoto("dialog_wiedergabe_fuer_sekunden"));
        s.push_back(klick(tr(Str::MsgOk), "###playback"));
        s.push_back(pruefSchritt("Repeat for: Modus und Rate uebernommen", [] {
            return app->playback_.mode == playback::RepeatMode::ForSeconds &&
                   std::fabs(app->repeatRateSeconds() - 0.25f) < 1e-4f &&
                   doc().clock.endMode() == timeline::EndMode::Stop;
        }));
        menue(s, Str::MenuEffects, Str::EffectsPlay);
        s.push_back(warteBis("Uhr ueber 1.1 s", [] { return doc().clock.timeMs() > 1100.0f; }, 8000));
        s.push_back(pruefSchritt("Repeat for 1 s bei 0.25 s: viermal nachgelegt (fuenf Ausloesungen)",
                                 [] { return app->scheduleActive_ && app->spawnCount_ == 4; }));
        s.push_back(pruefSchritt("Ausloesungen ueberlagern sich wie im Original",
                                 [] { return app->lastAlive_ >= 1; }));
        s.push_back(warteBis("Uhr steht", [] { return doc().clock.state() == timeline::State::Stopped; }, 8000));
        s.push_back(pruefSchritt("Repeat for: haelt nach dem Auslaufen von selbst an",
                                 [] { return doc().clock.timeMs() >= 1399.0f && app->spawnCount_ == 4; }));

        // "Respawn effect every frame": bis zum Anhalten, 60 je Sekunde.
        wiedergabeDialog(s);
        s.push_back(klick(tr(Str::PlaybackUntilStopped), "###playback"));
        s.push_back(klick(tr(Str::PlaybackEveryFrame), "###playback"));
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Respawn every frame sperrt die Rate (wie im Original)", [] {
            const Element* e = finde("##rateField", "###playback");
            return e != nullptr && e->gesperrt;
        }));
        s.push_back(klick(tr(Str::MsgOk), "###playback"));
        menue(s, Str::MenuEffects, Str::EffectsPlay);
        s.push_back(warteBis("Uhr ueber 0.5 s", [] { return doc().clock.timeMs() > 500.0f; }, 8000));
        s.push_back(pruefSchritt("Respawn every frame: rund 30 Ausloesungen in einer halben Sekunde", [] {
            return app->spawnCount_ >= 28 && app->spawnCount_ <= 40;
        }));
        s.push_back(pruefSchritt("und es leben viele gleichzeitig", [] { return app->lastAlive_ >= 20; }));
        s.push_back(foto("nachlegen_jedes_bild"));
        // Play waehrend der Wiederholung: auslaufen lassen, nichts mehr nachlegen.
        menue(s, Str::MenuEffects, Str::EffectsPlay);
        auto anzahl = std::make_shared<unsigned>(0);
        s.push_back(tu("Anzahl merken", [=] { *anzahl = app->spawnCount_; }));
        s.push_back(warte(10));
        s.push_back(pruefSchritt("Play waehrend der Wiederholung: es wird nicht mehr nachgelegt",
                                 [=] { return app->spawnCount_ <= *anzahl + 1; }));
        s.push_back(warteBis("ausgelaufen", [] { return doc().clock.state() == timeline::State::Stopped; }, 8000));
        s.push_back(pruefSchritt("und die Uhr haelt nach dem Auslaufen an",
                                 [] { return doc().clock.state() == timeline::State::Stopped; }));

        // Wandernder Startpunkt: 100 Einheiten je Sekunde entlang x,
        // zuruecksetzen nach 1 s. Rate 0.25 s.
        wiedergabeDialog(s);
        s.push_back(klick(tr(Str::PlaybackEveryFrame), "###playback"));
        feldIn(s, "##rateField", "###playback", "0.25");
        s.push_back(klick(tr(Str::PlaybackAnimate), "###playback"));
        feldIn(s, "##velocity", "###playback", "100", 0);
        feldIn(s, "##resetAfter", "###playback", "1");
        s.push_back(fensterFoto("dialog_wiedergabe_wandernd"));
        s.push_back(klick(tr(Str::MsgOk), "###playback"));
        s.push_back(pruefSchritt("Wandernder Startpunkt uebernommen", [] {
            return app->playback_.animateSpawnLocation && !app->playback_.respawnEveryFrame &&
                   std::fabs(app->playback_.spawnVelocity.x - 100.0f) < 1e-3f;
        }));
        menue(s, Str::MenuEffects, Str::EffectsPlay);
        s.push_back(warteBis("Uhr ueber 0.6 s", [] { return doc().clock.timeMs() > 600.0f; }, 8000));
        s.push_back(pruefSchritt("Die Ausloesung bei 500 ms entsteht bei x = 50", [] {
            for (const auto& item : doc().particles.live()) {
                if (std::fabs(item.spawnMs - 500.0f) < 0.5f) {
                    return std::fabs(item.positionAt(item.spawnMs).x - 50.0f) < 0.01f;
                }
            }
            return false;
        }));
        s.push_back(warteBis("Uhr ueber 1.3 s", [] { return doc().clock.timeMs() > 1300.0f; }, 8000));
        s.push_back(pruefSchritt("Nach 1 s springt der Startpunkt zurueck: bei 1250 ms x = 25", [] {
            for (const auto& item : doc().particles.live()) {
                if (std::fabs(item.spawnMs - 1250.0f) < 0.5f) {
                    return std::fabs(item.positionAt(item.spawnMs).x - 25.0f) < 0.01f;
                }
            }
            return false;
        }));
        // Bis zum Anhalten, Datei ohne repeatDelay: alle "Repeat Rate"
        // Sekunden kommt eine Ausloesung dazu, wie im Original.
        s.push_back(tu("bis zum Anhalten, ohne repeatDelay, 0.2 s", [] {
            app->pressStop();
            app->playback_ = playback::Settings{};
            app->playback_.mode = playback::RepeatMode::UntilStopped;
            effekt().repeatDelay = 0;
            effekt().repeatDelaySet = false;
            app->settings_.repeatRate = 0.2f;
            doc().clock.setEndMode(timeline::EndMode::Repeat);
            app->startPlayback();
        }));
        s.push_back(warteBis("Uhr ueber 1.05 s", [] { return doc().clock.timeMs() > 1050.0f; }, 8000));
        s.push_back(pruefSchritt("Bis zum Anhalten ohne repeatDelay: alle 0.2 s nachgelegt (5 in 1 s)",
                                 [] { return app->scheduleActive_ && app->spawnCount_ == 5; }));
        s.push_back(pruefSchritt("und die Uhr laeuft weiter statt neu zu beginnen",
                                 [] { return doc().clock.state() == timeline::State::Playing; }));
        // Die Wiederholart gilt auch fuer ein neues Dokument (im Original
        // gibt es nur die eine Einstellung).
        menue(s, Str::MenuFile, Str::FileNew);
        s.push_back(pruefSchritt("Neues Dokument erbt 'bis zum Anhalten'",
                                 [] { return doc().clock.endMode() == timeline::EndMode::Repeat; }));
        // Reiterwechsel waehrend des Auslaufens: nichts bleibt haengen.
        auto sofort = std::make_shared<bool>(false);
        s.push_back(tu("Abspielen, Play (Auslaufen), Reiter wechseln", [=] {
            Primitive p;
            p.type = PrimitiveType::Particle;
            p.life = Range::single(2000.0f);
            effekt().primitives.push_back(p);
            doc().segmentEnabled.assign(effekt().primitives.size(), true);
            app->pressPlay();
            app->pressPlay();
            app->activateDocument(0);
            *sofort = !app->playOut_;   // gleich danach, nicht erst ein Bild spaeter
        }));
        s.push_back(pruefSchritt("Reiterwechsel beendet das Auslaufen sauber", [=] {
            return *sofort && app->documents_.size() >= 2 && !app->playOut_ &&
                   app->documents_.back().clock.state() == timeline::State::Stopped;
        }));
        s.push_back(tu("aufraeumen", [] {
            app->pressStop();
            app->playback_ = playback::Settings{};
            doc().clock.setEndMode(timeline::EndMode::Stop);
        }));
        s.push_back(allesZu());
        return s;
    }

    // --- Spinner: Pfeile neben jedem Zahlenfeld wie im Original -----------
    static std::vector<Schritt> teilSpinner() {
        std::vector<Schritt> s;
        s.push_back(teil("spinner"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        s.push_back(klick(tr(Str::TabGeneration), "properties"));
        s.push_back(tu("Life 50", [] {
            gewaehlt()->life = Range::single(50.0f);
            app->recordChange("test");
        }));
        s.push_back(warte(2));
        s.push_back(fensterFoto("spinner_generation"));
        // Gemessen: Life 50 -> 200 -> 300 (Schritt 100, auf Vielfache gerundet).
        s.push_back(klickMarke("life/min+"));
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Life-Pfeil hoch: 50 -> 200 (gerundet wie im Original)", [] {
            return gewaehlt()->life.min == 200.0f && gewaehlt()->life.max == 200.0f;
        }));
        s.push_back(klickMarke("life/min+"));
        s.push_back(warte(2));
        s.push_back(pruefSchritt("noch einmal: 300", [] { return gewaehlt()->life.min == 300.0f; }));
        s.push_back(klickMarke("life/max-"));
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Max-Pfeil runter: 200, Min folgt (Min <= Max)", [] {
            return gewaehlt()->life.max == 200.0f && gewaehlt()->life.min == 200.0f;
        }));
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("Spinner-Klick ist rueckgaengig machbar",
                                 [] { return gewaehlt()->life.max == 300.0f; }));
        // Grenze: Count 0 bleibt beim Pfeil runter 0.
        s.push_back(tu("Count 0", [] {
            gewaehlt()->count = Range::single(0.0f);
            app->recordChange("test");
        }));
        s.push_back(warte(2));
        s.push_back(klickMarke("count/min-"));
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Count 0, Pfeil runter: bleibt 0 (Grenze 0..1000)",
                                 [] { return gewaehlt()->count.min == 0.0f; }));
        // Alpha: Schritt 0.1, 0.55 -> 0.7 -> 0.8 (gemessen).
        s.push_back(klick(tr(Str::TabColor), "properties"));
        s.push_back(tu("Alpha 0.55", [] {
            gewaehlt()->alpha.present = true;
            gewaehlt()->alpha.start = Range::single(0.55f);
            app->recordChange("test");
        }));
        s.push_back(warte(2));
        s.push_back(klickMarke("alpha/start/min+"));
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Alpha-Pfeil hoch: 0.55 -> 0.7", [] {
            return std::fabs(gewaehlt()->alpha.start.min - 0.7f) < 1e-5f;
        }));
        s.push_back(klickMarke("alpha/start/min+"));
        s.push_back(klickMarke("alpha/start/min+"));
        s.push_back(klickMarke("alpha/start/min+"));
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Alpha bleibt bei 1 stehen (Grenze 0..1)", [] {
            return std::fabs(gewaehlt()->alpha.start.min - 1.0f) < 1e-5f;
        }));
        s.push_back(fensterFoto("spinner_color"));
        return s;
    }

    static std::vector<Schritt> teilWiedergabe() {
        std::vector<Schritt> s;
        s.push_back(teil("wiedergabe"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        s.push_back(tu("Lebensdauer 2000 ms", [] {
            if (gewaehlt()) gewaehlt()->life = Range::single(2000.0f);
        }));
        menue(s, Str::MenuEffects, Str::EffectsPlay);
        s.push_back(pruefSchritt("Effects > Play spielt ab",
                                 [] { return doc().clock.state() == timeline::State::Playing; }));
        s.push_back(warte(10));
        s.push_back(pruefSchritt("Beim Abspielen ist ein Teilchen aktiv", [] { return app->lastAlive_ > 0; }));
        s.push_back(foto("wiedergabe_partikel"));
        menue(s, Str::MenuEffects, Str::EffectsPause);
        s.push_back(pruefSchritt("Effects > Pause haelt an",
                                 [] { return doc().clock.state() == timeline::State::Paused; }));
        auto zeit = std::make_shared<float>(0.0f);
        s.push_back(tu("Zeit merken", [=] { *zeit = doc().clock.timeMs(); }));
        s.push_back(warte(10));
        s.push_back(pruefSchritt("In der Pause steht die Zeit", [=] { return doc().clock.timeMs() == *zeit; }));
        menue(s, Str::MenuEffects, Str::EffectsPause);
        s.push_back(pruefSchritt("Pause noch einmal: laeuft weiter",
                                 [] { return doc().clock.state() == timeline::State::Playing; }));
        menue(s, Str::MenuEffects, Str::EffectsStop);
        s.push_back(pruefSchritt("Effects > Stop haelt an",
                                 [] { return doc().clock.state() == timeline::State::Stopped; }));
        // Wie im Original: laeuft eine Wiederholung, beendet ein zweites Play
        // sie - der Durchlauf lebt aus, dann steht alles.
        s.push_back(tu("Wiederholen einstellen", [] {
            doc().clock.setEndMode(timeline::EndMode::Repeat);
        }));
        s.push_back(klick("##play", "##main"));
        s.push_back(pruefSchritt("Play startet die Wiederholung",
                                 [] { return doc().clock.state() == timeline::State::Playing; }));
        s.push_back(klick("##play", "##main"));
        s.push_back(pruefSchritt("Play zum zweiten: Wiederholung endet, Durchlauf laeuft aus", [] {
            return doc().clock.state() == timeline::State::Playing && app->playOut_;
        }));
        s.push_back(warteBis("Durchlauf zu Ende", [] {
            return doc().clock.state() == timeline::State::Stopped;
        }, 5000));
        s.push_back(pruefSchritt("Danach gilt wieder Wiederholen", [] {
            return !app->playOut_ && doc().clock.endMode() == timeline::EndMode::Repeat;
        }));
        // Strg+C klont, Strg+Entf loescht (Tasten des Originals).
        s.push_back(tu("Segment waehlen", [] { doc().selectedPrimitive = 0; }));
        auto n = std::make_shared<int>(0);
        s.push_back(tu("Anzahl merken", [=] { *n = anzahlSegmente(); }));
        s.push_back(taste(ImGuiKey_C, true));
        s.push_back(pruefSchritt("Strg+C klont das Segment", [=] { return anzahlSegmente() == *n + 1; }));
        s.push_back(taste(ImGuiKey_Insert, true));
        s.push_back(pruefSchritt("Strg+Einfg klont das Segment", [=] { return anzahlSegmente() == *n + 2; }));
        s.push_back(taste(ImGuiKey_Delete, true));
        s.push_back(pruefSchritt("Strg+Entf loescht", [=] { return anzahlSegmente() == *n + 1; }));
        s.push_back(taste(ImGuiKey_Delete, false, true));
        s.push_back(pruefSchritt("Umschalt+Entf loescht", [=] { return anzahlSegmente() == *n; }));
        // Leertaste.
        s.push_back(taste(ImGuiKey_Space));
        s.push_back(pruefSchritt("Leertaste spielt ab",
                                 [] { return doc().clock.state() == timeline::State::Playing; }));
        s.push_back(klick("##tlStop", "##main"));
        s.push_back(pruefSchritt("Stop-Knopf haelt an",
                                 [] { return doc().clock.state() == timeline::State::Stopped; }));
        return s;
    }

    static std::vector<Schritt> teilEigenschaften() {
        std::vector<Schritt> s;
        s.push_back(teil("eigenschaften"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        s.push_back(warte(3));
        feld(s, "count/min", "7");
        s.push_back(pruefSchritt("Count eintippen setzt count = 7", [] {
            return gewaehlt() && gewaehlt()->count.set && gewaehlt()->count.min == 7.0f;
        }));
        s.push_back(pruefSchritt("Feldaenderung steht im Rueckgaengig-Verlauf",
                                 [] { return doc().undo.canUndo(); }));
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("Strg+Z nimmt die Feldaenderung zurueck",
                                 [] { return gewaehlt() && gewaehlt()->count.min != 7.0f; }));
        feld(s, "life/min", "1500");
        s.push_back(pruefSchritt("Life eintippen setzt life = 1500",
                                 [] { return gewaehlt() && gewaehlt()->life.min == 1500.0f; }));
        s.push_back(tu("Lage der Life-Felder melden", [] {
            for (const char* m : {"life/min", "life/ranged", "count/min", "count/ranged"}) {
                if (const Element* e = findeMarke(m)) {
                    char z[200];
                    std::snprintf(z, sizeof(z), "%s: %.0f..%.0f (sichtbar %.0f..%.0f) in %s", m,
                                  double(e->rect.Min.x), double(e->rect.Max.x),
                                  double(e->clip.Min.x), double(e->clip.Max.x), e->innen.c_str());
                    diag::info(z);
                }
            }
        }));
        feld(s, "life/max", "2500");
        s.push_back(pruefSchritt("Max eintippen macht Life zu einer Spanne",
                                 [] { return gewaehlt() && gewaehlt()->life.ranged; }));
        s.push_back(pruefSchritt("Life max = 2500", [] {
            return gewaehlt() && gewaehlt()->life.max == 2500.0f && gewaehlt()->life.min == 1500.0f;
        }));
        s.push_back(pruefSchritt("Text der Datei enthaelt life 1500 2500", [] {
            const bool da = text().find("1500 2500") != std::string::npos;
            if (!da) schreibeDatei(arbeitsOrdner + "/eigenschaften_text.efx", text());
            return da;
        }));
        return s;
    }

    // Packen, ueber die Ziehschwelle bewegen, zum Ziel fahren, dort stehen
    // (das Ziel muss den Zug erst annehmen), loslassen.
    static Schritt ziehe(const std::string& was, std::function<const Element*()> von,
                         std::function<const Element*()> nach, ImVec2 versatz = ImVec2(0, 0)) {
        auto start = std::make_shared<ImVec2>();
        return {was, [=](int b) {
                    ImGuiIO& io = ImGui::GetIO();
                    if (b == 0) {
                        const Element* e = von();
                        if (!e) {
                            meldeFehler(was + ": Quelle nicht gefunden");
                            return true;
                        }
                        *start = ImVec2(e->rect.Min.x + std::min(e->rect.GetWidth() * 0.5f, 60.0f),
                                        e->rect.GetCenter().y);
                        setzeMaus(*start);
                        return false;
                    }
                    if (b == 1) {
                        io.AddMouseButtonEvent(0, true);
                        return false;
                    }
                    if (b >= 2 && b <= 6) {
                        setzeMaus(ImVec2(start->x + 3.0f * float(b - 1), start->y + 3.0f * float(b - 1)));
                        return false;
                    }
                    if (b >= 7 && b <= 12) {
                        const Element* z = nach();
                        if (!z) {
                            if (b == 12) {
                                meldeFehler(was + ": Ziel nicht gefunden");
                                io.AddMouseButtonEvent(0, false);
                                return true;
                            }
                            return false;
                        }
                        const ImVec2 m = z->rect.GetCenter();
                        setzeMaus(ImVec2(std::min(m.x, z->rect.Min.x + 60.0f) + versatz.x, m.y + versatz.y));
                        return false;
                    }
                    if (b == 13) {
                        io.AddMouseButtonEvent(0, false);
                        return false;
                    }
                    return b >= 16;
                }};
    }

    // Die Zeile des Segments `i` in der Liste (das Selectable mit dem Namen).
    static const Element* listenZeile(int i) {
        const std::string name = (i >= 0 && i < anzahlSegmente() &&
                                  !effekt().primitives[size_t(i)].name.empty())
                                     ? effekt().primitives[size_t(i)].name
                                     : std::string(tr(Str::ListUnnamed));
        int gesehen = 0;
        // Gleichnamige Zeilen: die i-te mit diesem Namen zaehlt.
        int nte = 0;
        for (int k = 0; k < i; ++k) {
            const auto& p = effekt().primitives[size_t(k)];
            const std::string n = p.name.empty() ? std::string(tr(Str::ListUnnamed)) : p.name;
            if (n == name) ++nte;
        }
        for (const Element& e : g_elemente) {
            if (e.innen.find("segments") == std::string::npos || e.label != name) continue;
            if (gesehen++ == nte) return &e;
        }
        return nullptr;
    }

    static std::vector<Schritt> teilListe() {
        std::vector<Schritt> s;
        s.push_back(teil("liste"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        neuesSegment(s, PrimitiveType::Line);
        neuesSegment(s, PrimitiveType::Sound);
        s.push_back(tu("Namen vergeben", [] {
            effekt().primitives[0].name = "erstes";
            effekt().primitives[1].name = "zweites";
            effekt().primitives[2].name = "drittes";
            app->recordChange("Namen");
        }));
        s.push_back(warte(2));
        // Auswahl per Klick.
        s.push_back(klickAuf("Klick Zeile 'erstes'", [] { return listenZeile(0); }, 0, false, true,
                             120.0f));
        s.push_back(pruefSchritt("Klick auf eine Zeile waehlt das Segment",
                                 [] { return doc().selectedPrimitive == 0; }));
        s.push_back(pruefSchritt("... und die Eigenschaftsseite zeigt es (Reiter Generation)",
                                 [] { return finde(tr(Str::GenUseCulling), "properties") != nullptr; }));
        // Haekchen in der Zeile schaltet das Segment ab und wieder an.
        s.push_back(klickAuf("Haekchen Zeile 2", [] {
            int gesehen = 0;
            for (const Element& e : g_elemente) {
                if (e.innen.find("segments") != std::string::npos && e.label == "##enabled") {
                    if (gesehen++ == 1) return &e;
                }
            }
            return static_cast<const Element*>(nullptr);
        }));
        s.push_back(pruefSchritt("Haekchen in der Liste schaltet Segment 2 ab", [] {
            return doc().segmentEnabled.size() == 3 && !doc().segmentEnabled[1];
        }));
        s.push_back(pruefSchritt("Abschalten aendert die Datei nicht (wie im Original)",
                                 [] { return text().find("zweites") != std::string::npos; }));
        // Umsortieren durch Ziehen: "erstes" auf "drittes".
        s.push_back(ziehe("Ziehe 'erstes' auf 'drittes'", [] { return listenZeile(0); },
                          [] { return listenZeile(2); }));
        s.push_back(pruefSchritt("Ziehen sortiert um: erstes steht jetzt hinten", [] {
            return anzahlSegmente() == 3 && effekt().primitives[2].name == "erstes" &&
                   effekt().primitives[0].name == "zweites";
        }));
        s.push_back(pruefSchritt("Das abgeschaltete Segment bleibt beim Umsortieren abgeschaltet", [] {
            // "zweites" war aus und steht jetzt vorn.
            return doc().segmentEnabled.size() == 3 && !doc().segmentEnabled[0] &&
                   doc().segmentEnabled[1] && doc().segmentEnabled[2];
        }));
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("Strg+Z nimmt das Umsortieren zurueck",
                                 [] { return effekt().primitives[0].name == "erstes"; }));
        // Kontextmenue: darueber einfuegen.
        s.push_back(klickAuf("Rechtsklick Zeile 'zweites'", [] { return listenZeile(1); }, 1, false,
                             true, 120.0f));
        s.push_back(klick(tr(Str::SegmentInsertAbove), "##Popup"));
        s.push_back(pruefSchritt("Kontextmenue: Neues Segment darueber", [] {
            return anzahlSegmente() == 4 && effekt().primitives[2].name == "zweites" &&
                   doc().selectedPrimitive == 1;
        }));
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("Strg+Z nimmt das Einfuegen zurueck", [] { return anzahlSegmente() == 3; }));
        s.push_back(allesZu());
        // Spaltenkopf anklicken: das Original sortiert danach.
        s.push_back(pruefSchritt("Vor dem Kopfklick: Dateireihenfolge", [] {
            return effekt().primitives[0].name == "erstes" && effekt().primitives[1].name == "zweites";
        }));
        s.push_back(tu("Segment 'zweites' waehlen", [] { doc().selectedPrimitive = 1; }));
        s.push_back(klickMarke("liste/kopf0"));
        s.push_back(warte(3));
        s.push_back(fensterFoto("liste_nach_kopfklick"));
        s.push_back(pruefSchritt("Klick auf Name sortiert aufsteigend (drittes, erstes, zweites)", [] {
            return anzahlSegmente() == 3 && effekt().primitives[0].name == "drittes" &&
                   effekt().primitives[1].name == "erstes" && effekt().primitives[2].name == "zweites";
        }));
        s.push_back(pruefSchritt("Die Auswahl wandert mit ihrem Segment",
                                 [] { return gewaehlt() && gewaehlt()->name == "zweites"; }));
        s.push_back(klickMarke("liste/kopf0"));
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Zweiter Klick sortiert absteigend", [] {
            return effekt().primitives[0].name == "zweites" && effekt().primitives[2].name == "drittes";
        }));
        s.push_back(klickMarke("liste/kopf1"));
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Klick auf Type sortiert nach Typ (Line, Particle, Sound)", [] {
            return std::string(typeName(effekt().primitives[0].type)) == "Line" &&
                   std::string(typeName(effekt().primitives[2].type)) == "Sound";
        }));
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("Sortieren ist rueckgaengig machbar",
                                 [] { return effekt().primitives[0].name == "drittes"; }));
        return s;
    }

    // --- Bedienung: Zeitleiste, 3D-Ansicht, Reiter, Sprachen, Beenden ------
    static std::vector<Schritt> teilBedienung() {
        std::vector<Schritt> s;
        s.push_back(teil("bedienung"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        s.push_back(tu("Lebensdauer 2000 ms", [] {
            if (gewaehlt()) gewaehlt()->life = Range::single(2000.0f);
            app->recordChange("life");
            app->buildPreviewStopped();
        }));
        s.push_back(warte(2));

        // Zeitleiste: ein Bild vor, eins zurueck, an den Anfang.
        s.push_back(klick(">", "##main"));
        s.push_back(pruefSchritt("Zeitleiste '>' geht ein Bild vor",
                                 [] { return doc().clock.currentFrame() == 1; }));
        s.push_back(klick(">", "##main"));
        s.push_back(klick("<", "##main"));
        s.push_back(pruefSchritt("Zeitleiste '<' geht ein Bild zurueck",
                                 [] { return doc().clock.currentFrame() == 1; }));
        s.push_back(klick("|<", "##main"));
        s.push_back(pruefSchritt("Zeitleiste '|<' an den Anfang", [] { return doc().clock.timeMs() == 0.0f; }));
        // Pause-Knopf der Zeitleiste.
        s.push_back(klick("##tlPlay", "##main"));
        s.push_back(warte(5));
        s.push_back(klick("##tlPause", "##main"));
        s.push_back(pruefSchritt("Zeitleiste Pause haelt an",
                                 [] { return doc().clock.state() == timeline::State::Paused && doc().paused; }));
        s.push_back(klick("##tlStop", "##main"));

        // 3D-Ansicht: links ziehen dreht, rechts ziehen faehrt, Alt+links verschiebt, Rad zoomt.
        auto vorher = std::make_shared<camera::Matrix>();
        auto abstand = std::make_shared<float>(0.0f);
        const auto ansichtMitte = [] { return findeMarke("ansicht"); };
        s.push_back(tu("Kamera merken", [=] { *vorher = app->camera_.viewMatrix(); }));
        s.push_back(ziehe("Links ziehen in der Ansicht", ansichtMitte, ansichtMitte, ImVec2(80, 30)));
        s.push_back(pruefSchritt("Links ziehen dreht die Kamera", [=] {
            return app->camera_.viewMatrix() != *vorher;
        }));
        s.push_back(tu("Abstand merken", [=] { *abstand = app->camera_.distance(); }));
        s.push_back({"Rechts ziehen in der Ansicht", [=](int b) {
                         ImGuiIO& io = ImGui::GetIO();
                         const Element* e = findeMarke("ansicht");
                         if (!e) {
                             meldeFehler("Ansicht nicht gefunden");
                             return true;
                         }
                         const ImVec2 m = e->rect.GetCenter();
                         if (b == 0) setzeMaus(m);
                         if (b == 1) io.AddMouseButtonEvent(1, true);
                         if (b >= 2 && b <= 8) setzeMaus(ImVec2(m.x, m.y + 10.0f * float(b - 1)));
                         if (b == 9) io.AddMouseButtonEvent(1, false);
                         return b >= 11;
                     }});
        s.push_back(pruefSchritt("Rechts ziehen faehrt vor oder zurueck",
                                 [=] { return app->camera_.distance() != *abstand; }));
        s.push_back(tu("Abstand merken", [=] { *abstand = app->camera_.distance(); }));
        s.push_back({"Mausrad in der Ansicht", [=](int b) {
                         const Element* e = findeMarke("ansicht");
                         if (!e) return true;
                         if (b == 0) setzeMaus(e->rect.GetCenter());
                         if (b == 2) ImGui::GetIO().AddMouseWheelEvent(0.0f, 2.0f);
                         return b >= 4;
                     }});
        s.push_back(pruefSchritt("Mausrad zoomt", [=] { return app->camera_.distance() < *abstand; }));
        s.push_back(tu("Kamera merken", [=] { *vorher = app->camera_.viewMatrix(); }));
        s.push_back(klick("##viewTop", "##main"));
        s.push_back(pruefSchritt("Knopf 'Ansicht von oben' setzt die Kamera",
                                 [=] { return app->camera_.viewMatrix() != *vorher; }));
        s.push_back(klick("##viewReset", "##main"));

        // Reiter: neuer Reiter ueber '+', Wechsel, Schliessen mit Rueckfrage.
        s.push_back(tu("Dokument als geaendert markieren", [] { doc().dirty = true; }));
        s.push_back(klick("+", "##main"));
        s.push_back(pruefSchritt("'+' legt einen zweiten Reiter an", [] {
            return app->documents_.size() == 2 && app->activeDocument_ == 1;
        }));
        s.push_back(taste(ImGuiKey_Tab, true));
        s.push_back(pruefSchritt("Strg+Tab wechselt zum naechsten Reiter",
                                 [] { return app->activeDocument_ == 0; }));
        s.push_back(taste(ImGuiKey_W, true));
        s.push_back(warteBis("Rueckfrage beim Schliessen eines geaenderten Reiters",
                             [] { return dialogOffen("###savechanges"); }, 30));
        s.push_back(fensterFoto("dialog_speichern_frage"));
        s.push_back(klick(tr(Str::MsgCancel), "###savechanges"));
        s.push_back(pruefSchritt("Abbrechen: Reiter bleibt offen", [] { return app->documents_.size() == 2; }));
        s.push_back(taste(ImGuiKey_W, true));
        s.push_back(warteBis("Rueckfrage erneut", [] { return dialogOffen("###savechanges"); }, 30));
        s.push_back(klick(tr(Str::SaveChangesNo), "###savechanges"));
        s.push_back(pruefSchritt("Nicht speichern: Reiter ist zu", [] { return app->documents_.size() == 1; }));
        // Speichern aus der Rueckfrage.
        const std::string pfad = arbeitsOrdner + "/aus_rueckfrage.efx";
        s.push_back(tu("Effekt anlegen und geaendert lassen", [] {
            doc().effect.primitives.push_back(freshPrimitive(PrimitiveType::Light));
            doc().dirty = true;
        }));
        s.push_back(taste(ImGuiKey_W, true));
        s.push_back(warteBis("Rueckfrage", [] { return dialogOffen("###savechanges"); }, 30));
        s.push_back(dateiAntwort(pfad));
        s.push_back(klick(tr(Str::SaveChangesYes), "###savechanges"));
        s.push_back(pruefSchritt("Speichern aus der Rueckfrage schreibt die Datei",
                                 [pfad] { return fs::exists(pfad) && leseDatei(pfad).find("Light") != std::string::npos; }));

        // Sprachen und Themen: jede einmal, kein Absturz, Menue bleibt bedienbar.
        for (const auto& l : i18n::languages()) {
            const std::string name = l.nativeName;
            const i18n::Language sprache = l.language;
            (void)sprache;
            // Ueber das Menue, wie ein Mensch: nur dann wird auch die Schrift
            // neu gebaut (Chinesisch und Japanisch brauchen eigene Zeichen).
            s.push_back(klickTr(Str::MenuView, "##main"));
            s.push_back(klickTr(Str::ViewLanguage, "##Menu"));
            s.push_back(klick(name, "##Menu"));
            s.push_back(menuesZu());
            s.push_back(warte(4));
            s.push_back(pruefSchritt(std::string("Sprache ") + l.code + ": Bearbeitungsansicht bleibt vorn",
                                     [] { return !app->startTabActive_; }));
            s.push_back(pruefSchritt(std::string("Sprache ") + l.code +
                                         ": jedes Zeichen jeder Beschriftung ist in der Schrift",
                                     [] {
                ImFont* font = ImGui::GetFont();
                std::string fehlend;
                for (const Element& e : g_elemente) {
                    const std::string t = sichtbarerText(e.label);
                    for (size_t k = 0; k < t.size();) {
                        unsigned int c = static_cast<unsigned char>(t[k]);
                        int n = 1;
                        if (c >= 0xF0) { c &= 0x07; n = 4; }
                        else if (c >= 0xE0) { c &= 0x0F; n = 3; }
                        else if (c >= 0xC0) { c &= 0x1F; n = 2; }
                        for (int j = 1; j < n && k + j < t.size(); ++j) {
                            c = (c << 6) | (static_cast<unsigned char>(t[k + j]) & 0x3F);
                        }
                        k += static_cast<size_t>(n);
                        if (c >= 0x80 && c <= 0xFFFF && !font->FindGlyphNoFallback(static_cast<ImWchar>(c))) {
                            char z[48];
                            std::snprintf(z, sizeof(z), "U+%04X in \"%s\" ", c, t.substr(0, 20).c_str());
                            if (fehlend.size() < 300) fehlend += z;
                        }
                    }
                }
                if (!fehlend.empty()) diag::info("  fehlende Zeichen: " + fehlend);
                return fehlend.empty();
            }));
            s.push_back(fensterFoto(std::string("sprache_") + l.code));
            s.push_back(pruefSchritt(std::string("Sprache ") + l.code + ": Menue Datei sichtbar",
                                     [] { return finde(tr(Str::MenuFile), "##main") != nullptr; }));
        }
        s.push_back(tu("zurueck auf Englisch", [] {
            i18n::setLanguage(i18n::Language::English);
            app->settings_.languageCode = "en";
        }));
        s.push_back(warte(4));
        for (const auto& th : theme::builtinThemes()) {
            const std::string id = th.id;
            s.push_back(klick(tr(Str::MenuView), "##main"));
            s.push_back(klick(tr(Str::ViewTheme), "##Menu"));
            s.push_back(klick(th.name(), "##Menu"));
            s.push_back(menuesZu());
            s.push_back(pruefSchritt("Thema " + id + " gesetzt", [id] { return app->settings_.themeId == id; }));
            s.push_back(fensterFoto("thema_" + id));
        }
        return s;
    }

    // Beenden mit ungespeicherter Arbeit: fragt; Abbrechen beendet NICHT.
    static std::vector<Schritt> teilBeenden() {
        std::vector<Schritt> s;
        s.push_back(teil("beenden"));
        frischesDokument(s);
        s.push_back(tu("geaendert", [] {
            doc().effect.primitives.push_back(freshPrimitive(PrimitiveType::Particle));
            doc().dirty = true;
        }));
        menue(s, Str::MenuFile, Str::FileExit);
        s.push_back(warteBis("Rueckfrage beim Beenden", [] { return dialogOffen("###savechanges"); }, 30));
        s.push_back(klick(tr(Str::MsgCancel), "###savechanges"));
        s.push_back(pruefSchritt("Abbrechen beim Beenden: Programm laeuft weiter",
                                 [] { return !app->wantsQuit_; }));
        s.push_back(tu("Windows schliesst das Fenster (Alt+F4 / X)", [] {
            PostMessageW(GetActiveWindow() ? GetActiveWindow() : GetForegroundWindow(), WM_NULL, 0, 0);
            app->requestQuit();
        }));
        s.push_back(warteBis("Rueckfrage beim Schliessen", [] { return dialogOffen("###savechanges"); }, 30));
        s.push_back(klick(tr(Str::MsgCancel), "###savechanges"));
        s.push_back(pruefSchritt("Schliessen abgebrochen: Programm laeuft weiter",
                                 [] { return !app->wantsQuit_; }));
        return s;
    }

    // --- Startseite, Bibliothek, Spielpfad -----------------------------------
    static std::string spielpfad() {
        const char* pfad = std::getenv("EFXED_SPIELPFAD");
        return pfad && pfad[0] ? pfad
                               : "C:/Program Files (x86)/Steam/steamapps/common/Jedi Academy/"
                                 "GameData Movie Duels/base";
    }

    static std::vector<Schritt> teilBibliothek() {
        std::vector<Schritt> s;
        s.push_back(teil("bibliothek"));
        frischesDokument(s);
        s.push_back(tu("ohne Spielpfad beginnen", [] {
            app->settings_.gamePath.clear();
            app->settings_.extraGamePaths.clear();
            app->rescanAssets();
        }));
        // Startseite ueber Strg+B.
        s.push_back(taste(ImGuiKey_B, true));
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Strg+B zeigt die Startseite", [] { return app->startTabActive_; }));
        s.push_back(fensterFoto("startseite_ohne_pfad"));
        // "Neuer leerer Effekt" auf der Startseite.
        s.push_back(klick(tr(Str::StartNewEffect), "startPage"));
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Startseite 'Neuer leerer Effekt': Editor ist vorn",
                                 [] { return !app->startTabActive_; }));
        // Spielpfad ueber den Dialog: Durchsuchen (Ordnerauswahl) + Ok.
        s.push_back(taste(ImGuiKey_B, true));
        s.push_back(warte(3));
        s.push_back(klick(tr(Str::StartSetPath), "startPage"));
        s.push_back(warteBis("Spielpfad-Dialog offen", [] { return dialogOffen("###gamepath"); }, 30));
        s.push_back(dateiAntwort(spielpfad()));
        s.push_back(klick(tr(Str::PathsBrowse), "###gamepath"));
        s.push_back(warte(2));
        s.push_back(fensterFoto("dialog_spielpfad"));
        s.push_back(klick(tr(Str::MsgOk), "###gamepath"));
        s.push_back(warteBis("Bestand gelesen", [] { return app->assetsScanned_; }, 300));
        s.push_back(pruefSchritt("Spielpfad gesetzt und Effekte gefunden", [] {
            return !app->settings_.gamePath.empty() && !app->assets_.effects.empty();
        }));
        // Bibliothek: Kacheln erscheinen.
        s.push_back(tu("Bibliothek zeigen", [] {
            app->startTabActive_ = true;
            app->wantStartTab_ = true;
            if (app->browserEntries_.empty()) app->refreshBrowser();
        }));
        s.push_back(warteBis("Kacheln da", [] {
            for (const Element& e : g_elemente) {
                if (e.marke.rfind("kachel:", 0) == 0) return true;
            }
            return false;
        }, 300));
        s.push_back(warte(30));
        s.push_back(fensterFoto("bibliothek"));
        // Filtern.
        s.push_back(klick("##browserFilter", "startPage"));
        s.push_back(tippe("saber"));
        s.push_back(warte(4));
        s.push_back(pruefSchritt("Filter 'saber' zeigt nur passende Kacheln", [] {
            int passend = 0, andere = 0;
            for (const Element& e : g_elemente) {
                if (e.marke.rfind("kachel:", 0) != 0) continue;
                (e.marke.find("saber") != std::string::npos ? passend : andere)++;
            }
            return passend > 0 && andere == 0;
        }));
        s.push_back(fensterFoto("bibliothek_gefiltert"));
        // Doppelklick auf die erste sichtbare Kachel oeffnet den Effekt.
        auto name = std::make_shared<std::string>();
        s.push_back(klickAuf("Doppelklick erste Kachel", [=]() -> const Element* {
            for (const Element& e : g_elemente) {
                if (e.marke.rfind("kachel:", 0) == 0) {
                    *name = e.marke.substr(7);
                    return &e;
                }
            }
            return nullptr;
        }, 0, true));
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Doppelklick auf eine Kachel oeffnet den Effekt im Editor", [=] {
            return !app->startTabActive_ && anzahlSegmente() > 0;
        }));
        s.push_back(fensterFoto("bibliothek_geoeffnet"));
        // Spielpfad-Dialog: Abbrechen laesst Zusatzpfade, wie sie waren.
        menue(s, Str::MenuEdit, Str::EditGamePath);
        s.push_back(warteBis("Spielpfad-Dialog offen", [] { return dialogOffen("###gamepath"); }, 30));
        s.push_back(dateiAntwort("C:/Windows"));
        s.push_back(klick(tr(Str::PathsAdd), "###gamepath"));
        s.push_back(pruefSchritt("Hinzufuegen traegt einen Zusatzpfad ein",
                                 [] { return app->settings_.extraGamePaths.size() == 1; }));
        s.push_back(klick(tr(Str::MsgCancel), "###gamepath"));
        s.push_back(pruefSchritt("Abbrechen nimmt den Zusatzpfad wieder heraus",
                                 [] { return app->settings_.extraGamePaths.empty(); }));
        s.push_back(allesZu());
        return s;
    }

    // --- Auswahldialog und Rueckfrage ------------------------------------------
    static std::vector<Schritt> teilAuswahl() {
        std::vector<Schritt> s;
        s.push_back(teil("auswahl"));
        frischesDokument(s);
        s.push_back(tu("Spielpfad", [] {
            app->settings_.gamePath = spielpfad();
            app->settings_.extraGamePaths.clear();
            app->rescanAssets();
        }));
        neuesSegment(s, PrimitiveType::Particle);
        s.push_back(klick(tr(Str::TabColor), "properties"));
        s.push_back(warteBis("Reiter Color", [] { return app->propertyTab_ == fields::Tab::Color; }, 30));
        s.push_back(klickMarke("shaders/waehlen"));
        s.push_back(warteBis("Shaderauswahl offen", [] { return dialogOffen("###picker"); }, 30));
        s.push_back(klick("##filter", "###picker"));
        s.push_back(tippe("gfx/effects/sabers"));
        s.push_back(warte(3));
        s.push_back(fensterFoto("dialog_shaderauswahl"));
        // Zwei Eintraege: der erste per Klick, der zweite per Strg+Klick.
        auto namen = std::make_shared<std::vector<std::string>>();
        const auto eintrag = [](int nte) {
            return [nte]() -> const Element* {
                int gesehen = 0;
                for (const Element& e : g_elemente) {
                    if (e.innen.find("pickerlist") == std::string::npos || e.label.empty()) continue;
                    // Das Kindfenster selbst meldet sich auch (mit seinem Namen).
                    if (e.label.find("pickerlist") != std::string::npos) continue;
                    if (gesehen++ == nte) return &e;
                }
                return static_cast<const Element*>(nullptr);
            };
        };
        s.push_back(tu("Namen merken", [=] {
            namen->clear();
            for (int k = 0; k < 2; ++k) {
                if (const Element* e = eintrag(k)()) namen->push_back(sichtbarerText(e->label));
            }
        }));
        s.push_back(klickAuf("Klick erster Shader", eintrag(0)));
        s.push_back({"Strg halten", [](int b) {
                         if (b == 0) ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
                         return b >= 1;
                     }});
        s.push_back(klickAuf("Strg+Klick zweiter Shader", eintrag(1)));
        s.push_back({"Strg los", [](int b) {
                         if (b == 0) ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
                         return b >= 1;
                     }});
        s.push_back(klick(tr(Str::ChoosePreview), "###picker"));
        s.push_back(warte(30));
        s.push_back(fensterFoto("dialog_shaderauswahl_vorschau"));
        s.push_back(klick(tr(Str::MsgOk), "###picker"));
        s.push_back(pruefSchritt("Mehrfachauswahl uebernimmt beide Shader", [=] {
            return gewaehlt() && namen->size() == 2 && gewaehlt()->shaders.size() == 2 &&
                   gewaehlt()->shaders[0] == (*namen)[0] && gewaehlt()->shaders[1] == (*namen)[1];
        }));
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("Auswahl ist rueckgaengig machbar",
                                 [] { return gewaehlt() && gewaehlt()->shaders.empty(); }));
        // Farbknopf: die zwanzig Felder des Originals, Rot waehlen. Erst
        // "RGB Color" anhaken — ohne Haken sind die Knoepfe gesperrt.
        s.push_back(klickMarke("rgb/an"));
        s.push_back(pruefSchritt("RGB Color angehakt", [] { return gewaehlt() && gewaehlt()->rgb.present; }));
        s.push_back(klickMarke("rgb/start/farbe_min"));
        s.push_back(warte(2));
        s.push_back(fensterFoto("farbwahl_palette"));
        s.push_back(klickMarke("rgb/start/palette13"));
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Farbfeld Rot (255,0,0) setzt die Startfarbe und schliesst", [] {
            const auto& c = gewaehlt()->rgb.start;
            return gewaehlt() && c.set && c.min[0] == 1.0f && c.min[1] == 0.0f && c.min[2] == 0.0f &&
                   !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        }));
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("Farbwahl ist rueckgaengig machbar",
                                 [] { return gewaehlt() && !gewaehlt()->rgb.start.set; }));
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("und der Haken auch", [] { return gewaehlt() && !gewaehlt()->rgb.present; }));
        // Teure Physik: Rueckfrage, Nein laesst es aus, Ja schaltet ein.
        s.push_back(klick(tr(Str::TabPhysics), "properties"));
        s.push_back(warteBis("Reiter Physics", [] { return app->propertyTab_ == fields::Tab::Physics; }, 30));
        s.push_back(klickMarke("physik/an"));
        s.push_back(klickMarke("physik/teuer"));
        s.push_back(warteBis("Rueckfrage teure Physik", [] { return dialogOffen("###confirm"); }, 30));
        s.push_back(fensterFoto("dialog_teure_physik"));
        s.push_back(klick(tr(Str::MsgNo), "###confirm"));
        s.push_back(pruefSchritt("Nein: teure Physik bleibt aus", [] {
            return gewaehlt() && (gewaehlt()->flags & kFlagExpensivePhysics) == 0;
        }));
        s.push_back(klickMarke("physik/teuer"));
        s.push_back(warteBis("Rueckfrage erneut", [] { return dialogOffen("###confirm"); }, 30));
        s.push_back(klick(tr(Str::MsgYes), "###confirm"));
        s.push_back(pruefSchritt("Ja: teure Physik ist an", [] {
            return gewaehlt() && (gewaehlt()->flags & kFlagExpensivePhysics) != 0;
        }));
        s.push_back(pruefSchritt("... und steht in der Datei", [] {
            return text().find("expensivePhysics") != std::string::npos;
        }));
        return s;
    }

    // --- Mehrere Dokumente --------------------------------------------------------
    static std::vector<Schritt> teilDokumente() {
        std::vector<Schritt> s;
        s.push_back(teil("dokumente"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        s.push_back(taste(ImGuiKey_T, true));
        s.push_back(pruefSchritt("Strg+T: zweiter Reiter", [] { return app->documents_.size() == 2; }));
        neuesSegment(s, PrimitiveType::Sound);
        neuesSegment(s, PrimitiveType::Light);
        s.push_back(pruefSchritt("Reiter 2 hat zwei Segmente, Reiter 1 eines", [] {
            return app->documents_[1].effect.primitives.size() == 2 &&
                   app->documents_[0].effect.primitives.size() == 1;
        }));
        s.push_back(taste(ImGuiKey_Tab, true));
        s.push_back(pruefSchritt("Strg+Tab: Reiter 1 aktiv", [] { return app->activeDocument_ == 0; }));
        s.push_back(taste(ImGuiKey_Z, true));
        s.push_back(pruefSchritt("Strg+Z wirkt nur auf Reiter 1", [] {
            return app->documents_[0].effect.primitives.empty() &&
                   app->documents_[1].effect.primitives.size() == 2;
        }));
        s.push_back(taste(ImGuiKey_Y, true));
        s.push_back(pruefSchritt("Strg+Y stellt Reiter 1 wieder her",
                                 [] { return app->documents_[0].effect.primitives.size() == 1; }));
        // Ein Effekt aus einer pk3 hat keinen Dateipfad: Speichern fragt nach dem Ziel.
        s.push_back(tu("Spielpfad", [] {
            app->settings_.gamePath = spielpfad();
            app->rescanAssets();
        }));
        s.push_back(tu("Effekt aus pk3 oeffnen", [] {
            App::BrowserEntry e;
            e.name = app->assets_.effects.empty() ? std::string() : app->assets_.effects.front();
            app->openBrowserEntry(e);
        }));
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Effekt aus pk3: kein Dateipfad, Editor vorn", [] {
            return doc().filePath.empty() && !app->startTabActive_ && anzahlSegmente() > 0;
        }));
        const std::string pfad = arbeitsOrdner + "/aus_pk3.efx";
        s.push_back(dateiAntwort(pfad));
        s.push_back(taste(ImGuiKey_S, true));
        s.push_back(pruefSchritt("Strg+S bei pk3-Effekt fragt nach dem Ziel und speichert", [pfad] {
            return fs::exists(pfad) && doc().filePath == pfad && !doc().dirty;
        }));
        s.push_back(pruefSchritt("Gespeicherte pk3-Datei laesst sich fehlerfrei lesen",
                                 [pfad] { return !read(leseDatei(pfad)).hasErrors(); }));
        // Datei aufs Fenster ziehen (der Win32-Teil sammelt nur die Pfade).
        s.push_back(tu("gezogene .efx", [pfad] { app->openDroppedFile(pfad); }));
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Gezogene .efx wird geoeffnet und gezeigt", [pfad] {
            return doc().filePath == pfad && !app->startTabActive_;
        }));
        return s;
    }

    // --- Werkzeuge: Meldungen, Protokoll, Teiler, Regler, Leisten ------------------
    static void feldNachLabel(std::vector<Schritt>& s, const std::string& label, const std::string& text) {
        s.push_back(klickAuf("Doppelklick " + label, [=] { return finde(label, "##main"); }, 0, true));
        s.push_back(taste(ImGuiKey_A, true));
        s.push_back(tippe(text));
        s.push_back(taste(ImGuiKey_Enter));
    }

    static std::vector<Schritt> teilWerkzeug() {
        std::vector<Schritt> s;
        s.push_back(teil("werkzeug"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);
        // Meldungen: ein Particle ohne Shader erzeugt einen Hinweis; die Zahl
        // in der Statuszeile oeffnet das Fenster, ein Klick waehlt das Segment.
        neuesSegment(s, PrimitiveType::Sound);
        s.push_back(tu("Meldungen auffrischen", [] { app->refreshDiagnostics(); }));
        s.push_back(warte(2));
        s.push_back(klickAuf("Klick auf die Meldungszahl", [] {
            for (const Element& e : g_elemente) {
                if (e.innen.find("##main") != std::string::npos &&
                    (e.label.find(tr(Str::DiagWarning)) != std::string::npos ||
                     e.label.find(tr(Str::DiagError)) != std::string::npos)) {
                    return &e;
                }
            }
            return static_cast<const Element*>(nullptr);
        }));
        s.push_back(pruefSchritt("Klick auf die Meldungszahl oeffnet das Meldungsfenster",
                                 [] { return app->showMessagesWindow_; }));
        s.push_back(warte(2));
        s.push_back(fensterFoto("meldungen"));
        auto zielSegment = std::make_shared<int>(-1);
        s.push_back(tu("Segment merken", [=] {
            *zielSegment = doc().diagnostics.empty() ? -1 : doc().diagnostics.front().primitive;
            // Ein anderes Segment waehlen als das der ersten Meldung.
            doc().selectedPrimitive = *zielSegment == 0 ? 1 : 0;
        }));
        s.push_back(klickAuf("Klick erste Meldung", [] {
            for (const Element& e : g_elemente) {
                if (e.innen.find("###messages") != std::string::npos && !e.label.empty() &&
                    e.label.find("###") == std::string::npos) {
                    return &e;
                }
            }
            return static_cast<const Element*>(nullptr);
        }));
        s.push_back(pruefSchritt("Klick auf eine Meldung waehlt das betroffene Segment",
                                 [=] { return *zielSegment >= 0 && doc().selectedPrimitive == *zielSegment; }));
        s.push_back(tu("Meldungen zu", [] { app->showMessagesWindow_ = false; }));
        // Protokoll: oeffnen, alles kopieren.
        menue(s, Str::MenuView, Str::WindowLog);
        s.push_back(tu("Zwischenablage leeren", [] {
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                CloseClipboard();
            }
        }));
        s.push_back(klick(tr(Str::LogCopy), "###log"));
        s.push_back(pruefSchritt("Protokoll: 'Alles kopieren' legt Text in die Zwischenablage",
                                 [] { return IsClipboardFormatAvailable(CF_TEXT) != FALSE; }));
        s.push_back(tu("Protokoll zu", [] { app->showLogWindow_ = false; }));
        // Zeitfaktor eintippen.
        feldNachLabel(s, "##timeScale", "2");
        s.push_back(pruefSchritt("Zeitfaktor 2 eingetippt: Uhr laeuft doppelt", [] {
            return std::fabs(app->settings_.timeScale - 2.0f) < 1e-3f &&
                   std::fabs(doc().clock.speed() - 2.0f) < 1e-3f;
        }));
        feldNachLabel(s, "##timeScale", "1");
        // Wiederholrate eintippen: wird zum repeatDelay der Datei.
        feldNachLabel(s, "##repeatRate", "1.5");
        s.push_back(pruefSchritt("Wiederholrate 1.5 s: repeatDelay 1500 in der Datei", [] {
            return effekt().repeatDelaySet && effekt().repeatDelay == 1500 &&
                   text().find("repeatDelay") != std::string::npos;
        }));
        // Weltmassstab: Kamera faehrt mit (wie im Original).
        auto abstand = std::make_shared<float>(0.0f);
        s.push_back(tu("Abstand merken", [=] {
            app->camera_.reset(app->settings_.worldScale);
            *abstand = app->camera_.distance();
        }));
        s.push_back(klickMarke("weltmassstab"));
        s.push_back(klickAuf("16 Einheiten je Fuss waehlen", [] {
            for (int i = 0; i < layout::worldScaleCount(); ++i) {
                if (layout::worldScales()[i].unitsPerFoot == 16.0f) {
                    return finde(layout::worldScales()[i].label(), "##Combo");
                }
            }
            return static_cast<const Element*>(nullptr);
        }));
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Weltmassstab 16: Kameraabstand waechst um 1.6", [=] {
            return app->settings_.worldScale == 16.0f &&
                   std::fabs(app->camera_.distance() - *abstand * 1.6f) < 0.01f;
        }));
        s.push_back(tu("zurueck auf 10", [] { app->settings_.worldScale = 10.0f; }));
        s.push_back(warte(3));
        // Leisten ein- und ausblenden.
        menue(s, Str::MenuView, Str::ViewMainToolbar);
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Main Toolbar aus: 'New' ist weg", [] {
            return !app->settings_.showMainToolbar && finde("##new", "##main") == nullptr;
        }));
        menue(s, Str::MenuView, Str::ViewMainToolbar);
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Main Toolbar wieder an", [] { return finde("##new", "##main") != nullptr; }));
        menue(s, Str::MenuView, Str::ViewPlaybackToolbar);
        s.push_back(warte(2));
        s.push_back(pruefSchritt("Playback Toolbar aus: Wiederholrate ist weg",
                                 [] { return finde("##repeatRate", "##main") == nullptr; }));
        menue(s, Str::MenuView, Str::ViewPlaybackToolbar);
        // Teiler ziehen: die Eigenschaftsspalte wird breiter.
        auto anteil = std::make_shared<float>(0.0f);
        s.push_back(tu("Anteil merken", [=] { *anteil = app->settings_.split.propertiesFraction; }));
        s.push_back({"Teiler links/rechts ziehen", [=](int b) {
                         ImGuiIO& io = ImGui::GetIO();
                         // Der Teiler liegt zwischen der linken Spalte und der
                         // Eigenschaftsspalte. Nur sichtbare Fenster zaehlen: alte
                         // Tabs lassen ihre Kinder in der Liste stehen, und die
                         // Eigenschaftsspalte hat selbst noch Kinder.
                         auto kind = [](const char* praefix) -> ImGuiWindow* {
                             for (ImGuiWindow* c : GImGui->Windows) {
                                 const std::string n = c->Name;
                                 if (c->WasActive && n.find(praefix) == 0 &&
                                     n.find('/', std::strlen("##main/")) == std::string::npos) {
                                     return c;
                                 }
                             }
                             return nullptr;
                         };
                         ImGuiWindow* lw = kind("##main/leftColumn");
                         ImGuiWindow* pw = kind("##main/properties");
                         if (!lw || !pw) return true;
                         const ImVec2 at((lw->Pos.x + lw->Size.x + pw->Pos.x) * 0.5f, pw->Pos.y + pw->Size.y * 0.5f);
                         if (b == 0) setzeMaus(at);
                         if (b == 1) io.AddMouseButtonEvent(0, true);
                         if (b >= 2 && b <= 8) setzeMaus(ImVec2(at.x - 15.0f * float(b - 1), at.y));
                         if (b == 9) io.AddMouseButtonEvent(0, false);
                         return b >= 11;
                     }});
        s.push_back(pruefSchritt("Teiler ziehen macht die Eigenschaftsspalte breiter",
                                 [=] { return app->settings_.split.propertiesFraction > *anteil + 0.01f; }));
        // Segment abschalten: die Vorschau hat keine lebenden Teilchen mehr.
        s.push_back(tu("nur ein Segment, lange Lebensdauer", [] {
            effekt().primitives.resize(1);
            effekt().primitives[0].life = Range::single(3000.0f);
            doc().segmentEnabled.assign(1, true);
            doc().selectedPrimitive = 0;
            app->recordChange("test");
            app->startPlayback();
        }));
        s.push_back(warte(10));
        s.push_back(pruefSchritt("Segment an: Teilchen leben", [] { return app->lastAlive_ > 0; }));
        s.push_back(klickAuf("Haekchen Segment 1", [] {
            for (const Element& e : g_elemente) {
                if (e.innen.find("segments") != std::string::npos && e.label == "##enabled") return &e;
            }
            return static_cast<const Element*>(nullptr);
        }));
        s.push_back(warte(5));
        s.push_back(pruefSchritt("Segment aus: keine Teilchen mehr in der Vorschau",
                                 [] { return app->lastAlive_ == 0; }));
        // Raumarten (efxed-Zugabe: draussen mit Himmel, gar nichts).
        const Str arten[] = {Str::RoomOpenSky, Str::RoomNone, Str::RoomEnclosed};
        for (Str a : arten) {
            menue(s, Str::MenuView, Str::ViewRoomStyle, a);
            s.push_back(warte(3));
            s.push_back(fensterFoto(std::string("raumart_") + std::to_string(static_cast<int>(a))));
        }
        s.push_back(pruefSchritt("Raumart wieder geschlossen", [] { return app->settings_.roomStyle == 0; }));
        // Ohne Spielpfad leitet das Oeffnen ihn aus ".../base/" ab (Original).
        auto alterPfad = std::make_shared<std::string>();
        s.push_back(tu("Datei unter spiel/base oeffnen, ohne Spielpfad", [=] {
            *alterPfad = app->settings_.gamePath;
            const fs::path ordner = fs::path(arbeitsOrdner) / "spiel" / "base" / "effects";
            std::error_code ec;
            fs::create_directories(ordner, ec);
            schreibeDatei((ordner / "pfadtest.efx").string(), "Particle\n{\n\tlife 500\n}\n");
            app->settings_.gamePath.clear();
            app->openFile((ordner / "pfadtest.efx").string());
        }));
        s.push_back(pruefSchritt("Spielpfad aus dem Dateipfad abgeleitet (.../spiel/base)", [] {
            const std::string& p = app->settings_.gamePath;
            return p.size() >= 10 && p.compare(p.size() - 10, 10, "spiel/base") == 0;
        }));
        s.push_back(tu("Spielpfad zurueck", [=] {
            app->settings_.gamePath = *alterPfad;
            app->rescanAssets();
            doc().dirty = false;
            app->closeDocument(app->activeDocument_);
        }));
        return s;
    }

    // --- Dialoge -----------------------------------------------------------
    //
    // Jeder Dialog geht auf, zeigt sich ganz, und schliesst mit Ok wie mit
    // Abbrechen. Abbrechen darf nichts veraendern — im Original sind das
    // Windows-Dialoge, und dort heisst Abbrechen: alles wie vorher.
    static bool dialogOffen(const std::string& kennung) {
        ImGuiContext& g = *GImGui;
        for (const auto& p : g.OpenPopupStack) {
            if (p.Window && std::string(p.Window->Name).find(kennung) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    static void dialogAufZu(std::vector<Schritt>& s, Str menu, Str eintrag,
                            const std::string& kennung, Str knopf) {
        menue(s, menu, eintrag);
        s.push_back(warteBis(std::string(tr(eintrag)) + " offen", [=] { return dialogOffen(kennung); }, 30));
        s.push_back(fensterFoto("dialog" + kennung.substr(3)));
        s.push_back(klick(tr(knopf), kennung));
        s.push_back(warteBis(std::string(tr(eintrag)) + " wieder zu", [=] { return !dialogOffen(kennung); }, 30));
    }

    static std::vector<Schritt> teilDialoge() {
        std::vector<Schritt> s;
        s.push_back(teil("dialoge"));
        frischesDokument(s);
        neuesSegment(s, PrimitiveType::Particle);

        // Ueber, Grafiktreiber, Wind, Sonne: auf und mit Ok zu.
        dialogAufZu(s, Str::MenuHelp, Str::HelpAbout, "###about", Str::MsgOk);
        dialogAufZu(s, Str::MenuView, Str::ViewGraphicsInfo, "###driverinfo", Str::MsgOk);
        dialogAufZu(s, Str::MenuView, Str::DialogWind, "###wind", Str::MsgOk);
        dialogAufZu(s, Str::MenuView, Str::DialogSun, "###sun", Str::MsgOk);

        // Wandfarbe: aendern, Abbrechen -> unveraendert.
        auto farbe = std::make_shared<std::array<float, 4>>();
        s.push_back(tu("Wandfarbe merken", [=] {
            (*farbe)[0] = app->wallColour_[0];
            (*farbe)[1] = app->wallColour_[1];
            (*farbe)[2] = app->wallColour_[2];
            (*farbe)[3] = app->wallColourOverridden_ ? 1.0f : 0.0f;
        }));
        menue(s, Str::MenuEdit, Str::EditWallColor);
        s.push_back(warteBis("Wandfarbe offen", [] { return dialogOffen("###colour"); }, 30));
        s.push_back(fensterFoto("dialog_wandfarbe"));
        s.push_back(tu("Farbe im Dialog verstellen (wie ein Zug im Farbfeld)", [] {
            app->wallColour_[0] = 0.9f;
            app->wallColour_[1] = 0.1f;
            app->wallColour_[2] = 0.1f;
            app->wallColourOverridden_ = true;
            app->geometryDirty_ = true;
        }));
        s.push_back(klick(tr(Str::MsgCancel), "###colour"));
        s.push_back(pruefSchritt("Wandfarbe: Abbrechen stellt die alte Farbe wieder her", [=] {
            // Ohne eigene Farbe gilt die des Themas — dann zaehlt nur, dass
            // keine eigene uebrig bleibt.
            const bool vorherEigen = (*farbe)[3] != 0.0f;
            if (app->wallColourOverridden_ != vorherEigen) return false;
            return !vorherEigen || (app->wallColour_[0] == (*farbe)[0] &&
                                    app->wallColour_[1] == (*farbe)[1] &&
                                    app->wallColour_[2] == (*farbe)[2]);
        }));
        s.push_back(allesZu());

        // Wiedergabe-Einstellungen: Modus aendern, Abbrechen -> unveraendert.
        auto modus = std::make_shared<int>(0);
        s.push_back(tu("Wiedergabemodus merken", [=] {
            *modus = static_cast<int>(app->playback_.mode);
            doc().clock.setEndMode(*modus == 0 ? timeline::EndMode::Stop : timeline::EndMode::Repeat);
        }));
        menue(s, Str::MenuEffects, Str::EffectsPlaybackSettings);
        s.push_back(warteBis("Wiedergabe offen", [] { return dialogOffen("###playback"); }, 30));
        s.push_back(fensterFoto("dialog_wiedergabe"));
        s.push_back(tu("Modus im Dialog umstellen", [] {}));
        s.push_back(klickAuf("Klick anderer Modus", [=]() -> const Element* {
            const Str ziel = *modus == 0 ? Str::PlaybackUntilStopped : Str::PlaybackOnce;
            return finde(tr(ziel), "###playback");
        }));
        s.push_back(klick(tr(Str::MsgCancel), "###playback"));
        s.push_back(pruefSchritt("Wiedergabe: Abbrechen laesst den Modus, wie er war",
                                 [=] { return static_cast<int>(app->playback_.mode) == *modus; }));
        s.push_back(pruefSchritt("Wiedergabe: Abbrechen laesst auch die Uhr, wie sie war", [=] {
            const bool einmal = *modus == 0;
            return (doc().clock.endMode() == timeline::EndMode::Stop) == einmal;
        }));
        s.push_back(allesZu());

        // Eigener Ursprung: Abbrechen -> unveraendert.
        auto ursprung = std::make_shared<int>(0);
        s.push_back(tu("Ursprung merken", [=] { *ursprung = static_cast<int>(app->spawnOrigin_.mode); }));
        menue(s, Str::MenuEffects, Str::EffectsCustomOrigin);
        s.push_back(warteBis("Ursprung offen", [] { return dialogOffen("###spawnorigin"); }, 30));
        s.push_back(fensterFoto("dialog_ursprung"));
        s.push_back(klick(tr(Str::OriginCustom), "###spawnorigin"));
        s.push_back(klick(tr(Str::MsgCancel), "###spawnorigin"));
        s.push_back(pruefSchritt("Eigener Ursprung: Abbrechen laesst alles, wie es war",
                                 [=] { return static_cast<int>(app->spawnOrigin_.mode) == *ursprung; }));
        s.push_back(allesZu());
        // ... und Ok uebernimmt.
        menue(s, Str::MenuEffects, Str::EffectsCustomOrigin);
        s.push_back(warteBis("Ursprung offen", [] { return dialogOffen("###spawnorigin"); }, 30));
        s.push_back(klick(tr(Str::OriginOnFloor), "###spawnorigin"));
        s.push_back(klick(tr(Str::MsgOk), "###spawnorigin"));
        s.push_back(pruefSchritt("Eigener Ursprung: Ok uebernimmt", [] {
            return app->spawnOrigin_.mode == playback::OriginMode::OnFloor;
        }));
        s.push_back(allesZu());

        // Grafikschnittstelle wechseln: Rueckfrage, Abbrechen -> kein Wechsel.
        // Die jeweils ANDERE Schnittstelle waehlen (die aktive fragt nicht).
        s.push_back(klickTr(Str::MenuView, "##main"));
        s.push_back(klickTr(Str::ViewRenderer, "##Menu"));
        s.push_back(klickAuf("Klick andere Grafikschnittstelle", [] {
            const render::Backend andere = app->activeBackend_ == render::Backend::OpenGL3
                                               ? render::Backend::Direct3D11
                                               : render::Backend::OpenGL3;
            return finde(render::backendName(andere), "##Menu");
        }));
        s.push_back(menuesZu());
        s.push_back(warteBis("Rueckfrage Grafikschnittstelle offen",
                             [] { return dialogOffen("###renderer"); }, 30));
        s.push_back(fensterFoto("dialog_grafik"));
        s.push_back(klick(tr(Str::MsgCancel), "###renderer"));
        s.push_back(pruefSchritt("Grafikschnittstelle: Abbrechen wechselt nicht", [] {
            render::Backend ziel{};
            return !app->wantsRendererChange(ziel);
        }));
        s.push_back(allesZu());

        // Handbuch: wird geschrieben und an Windows uebergeben.
        s.push_back(tu("Shell-Liste leeren", [] { geoeffnetPerShell.clear(); }));
        menue(s, Str::MenuHelp, Str::HelpUsersGuide);
        s.push_back(pruefSchritt("Handbuch: Datei an den Browser uebergeben", [] {
            return !geoeffnetPerShell.empty() && fs::exists(geoeffnetPerShell.back());
        }));

        // Bildschirmfoto in Datei.
        auto vorher = std::make_shared<size_t>(0);
        const auto zaehleTga = [] {
            size_t n = 0;
            std::error_code ec;
            for (const auto& e : fs::directory_iterator(paths::configDir(), ec)) {
                if (e.path().extension() == ".tga") ++n;
            }
            return n;
        };
        s.push_back(tu("Bilder zaehlen", [=] { *vorher = zaehleTga(); }));
        menue(s, Str::MenuView, Str::ViewScreenshot);
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Screenshot to File schreibt ein Bild", [=] { return zaehleTga() == *vorher + 1; }));
        // Bildschirmfoto in die Zwischenablage.
        s.push_back(tu("Zwischenablage leeren", [] {
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                CloseClipboard();
            }
        }));
        menue(s, Str::MenuView, Str::ViewScreenshotClip);
        s.push_back(warte(3));
        s.push_back(pruefSchritt("Screenshot to Clipboard legt ein Bild in die Zwischenablage",
                                 [] { return IsClipboardFormatAvailable(CF_DIB) != FALSE; }));
        s.push_back(pruefSchritt("... und schreibt dabei keine Datei", [=] { return zaehleTga() == *vorher + 1; }));
        return s;
    }

    // --- Alle Effekte des Spiels ---------------------------------------------
    //
    // Spielpfad setzen (EFXED_SPIELPFAD, sonst Movie Duels), jeden Effekt aus
    // dem Bestand oeffnen — ueber denselben Weg wie die Bibliothek —, feste
    // Zeitpunkte anfahren, die Ansicht fotografieren und messen: wie viele
    // Teilchen leben, wie viele gezeichnet werden, wie lange ein Bild dauert.
    // Mit festem Zufallswert, damit zwei Laeufe Bild fuer Bild gleich sind
    // (Vergleich vor/nach einer Aenderung am Renderer).
    static std::string bericht;
    static std::vector<Schritt> teilDarstellung() {
        std::vector<Schritt> s;
        s.push_back(teil("darstellung"));
        s.push_back(tu("Spielpfad setzen und Bestand lesen", [] {
            const char* pfad = std::getenv("EFXED_SPIELPFAD");
            app->settings_.gamePath =
                pfad && pfad[0] ? pfad
                                : "C:/Program Files (x86)/Steam/steamapps/common/Jedi Academy/"
                                  "GameData Movie Duels/base";
            app->settings_.extraGamePaths.clear();
            app->rescanAssets();
            app->fixedSeed_ = 4242;
            bericht = "effekt\tzeit_ms\tlebend\tgezeichnet\tbild_ms\taufbau_ms\tzeichnen_ms\n";
        }));
        s.push_back(pruefSchritt("Bestand enthaelt Effekte",
                                 [] { return !app->assets_.effects.empty(); }));
        s.push_back({"Effekte einplanen", [](int) {
                         std::vector<std::string> liste = app->assets_.effects;
                         std::sort(liste.begin(), liste.end());
                         int grenze = static_cast<int>(liste.size());
                         if (const char* n = std::getenv("EFXED_EFFEKTE"); n && std::atoi(n) > 0) {
                             grenze = std::min(grenze, std::atoi(n));
                         }
                         std::vector<Schritt> neu;
                         for (int i = 0; i < grenze; ++i) {
                             einzelnerEffekt(neu, liste[static_cast<size_t>(i)]);
                         }
                         diag::info("Selbsttest: " + std::to_string(grenze) + " Effekte eingeplant");
                         einfuegen(std::move(neu));
                         return true;
                     }});
        s.push_back(tu("Bericht schreiben", [] {
            schreibeDatei(paths::configDir() + "/darstellung.tsv", bericht);
            app->fixedSeed_ = 0;
        }));
        return s;
    }

    static void einzelnerEffekt(std::vector<Schritt>& s, const std::string& name) {
        std::string datei = name;
        for (char& c : datei) {
            if (c == '/' || c == '\\' || c == ':') c = '_';
        }
        s.push_back(tu("Oeffne " + name, [name] {
            App::BrowserEntry eintrag;
            eintrag.name = name;
            const size_t vorher = app->documents_.size();
            app->openBrowserEntry(eintrag);
            if (app->documents_.size() == vorher) {
                meldeFehler(name + ": liess sich nicht oeffnen");
                return;
            }
            app->showEditor();
        }));
        s.push_back(warte(3));
        for (float ms : {20.0f, 150.0f, 600.0f, 1500.0f}) {
            s.push_back(tu("Zeit " + std::to_string(static_cast<int>(ms)), [ms] {
                app->pressStop();
                app->startPlayback();
                app->doc().clock.stop();
                app->doc().clock.scrubTo(ms);
                // Vergleichbar machen: Kamera zurueck, Wackeln aus (laeuft
                // auf Echtzeit), keine Windfahne (ebenfalls Echtzeit).
                app->shake_ = camera::Shake{};
                app->camera_.reset(app->settings_.worldScale);
                app->settings_.drawWindVector = false;
            }));
            s.push_back(warte(3));
            s.push_back(warteBis("Texturen geladen", [] {
                return app->texturesInFlight_.empty() && app->readyTextures_.empty();
            }, 600));
            s.push_back(warte(2));
            s.push_back(foto("e_" + datei + "_" + std::to_string(static_cast<int>(ms))));
            s.push_back(tu("messen", [name, ms] {
                char z[512];
                std::snprintf(z, sizeof(z), "%s\t%.0f\t%d\t%d\t%.2f\t%.2f\t%.2f\n", name.c_str(),
                              double(ms), app->lastAlive_, app->lastDrawn_,
                              double(ImGui::GetIO().DeltaTime * 1000.0f),
                              double(app->lastBuildMs_), double(app->lastDrawMs_));
                bericht += z;
            }));
        }
        // Eine Sekunde echt abspielen — stuerzt nichts ab, wenn die Uhr laeuft?
        s.push_back(tu("abspielen", [] { app->pressPlay(); }));
        s.push_back(warte(20));
        s.push_back(tu("schliessen", [] {
            app->pressStop();
            app->doc().dirty = false;
            app->closeDocument(app->activeDocument_);
        }));
    }

    // --- Jedes Feld jedes Typs -------------------------------------------
    //
    // Fuer jeden der dreizehn Typen: Segment anlegen, jeden Reiter oeffnen,
    // und dort JEDES Bedienelement benutzen — Zahlen eintippen, Haekchen
    // klicken, Listeneintraege anlegen, Kurvenart waehlen. Nach jedem Schritt
    // muss sich der Dateitext geaendert haben: ein Feld, das nichts in die
    // Datei schreibt, ist entweder Schmuck oder kaputt. Am Ende speichern,
    // neu laden und vergleichen.
    //
    // Welche Elemente es gibt, steht erst fest, wenn der Reiter gezeichnet
    // ist. Deshalb schaut ein Schritt nach und haengt die Schritte fuer genau
    // diese Elemente hinter sich ein.
    static size_t einfuegeAn;
    static void einfuegen(std::vector<Schritt> neu) {
        schritte.insert(schritte.begin() + static_cast<std::ptrdiff_t>(einfuegeAn),
                        std::make_move_iterator(neu.begin()), std::make_move_iterator(neu.end()));
        einfuegeAn += neu.size();
    }

    static bool imReiter(const Element& e) {
        return e.innen.find("tabbody") != std::string::npos && !e.gesperrt;
    }

    // Ein Schritt, der den Text vorher merkt, `aktion` ausfuehrt (als
    // Teilschritte) und danach eine Aenderung verlangt.
    static void mitAenderung(std::vector<Schritt>& s, const std::string& was,
                             std::vector<Schritt> aktion, bool pflicht = true) {
        auto vorher = std::make_shared<std::string>();
        s.push_back(tu("merken: " + was, [=] { *vorher = text(); }));
        for (auto& a : aktion) s.push_back(std::move(a));
        s.push_back(warte(2));
        s.push_back({"Pruefe Aenderung: " + was, [=](int) {
                         const bool anders = text() != *vorher;
                         if (anders) {
                             meldeOk(was + " aendert die Datei");
                         } else if (pflicht) {
                             meldeFehler(was + " aendert die Datei NICHT");
                             static int nummer = 0;
                             einfuegen({fensterFoto("fehler_" + std::to_string(++nummer))});
                         } else {
                             diag::info("Selbsttest HINWEIS: [" + aktuellerTeil + "] " + was +
                                        " aendert die Datei nicht");
                         }
                         return true;
                     }});
    }

    static std::string zahl(int i) {
        // Unterschiedliche, gueltige Werte; keine Null (die ist oft Vorgabe).
        const int ganz = 2 + (i * 7) % 23;
        return std::to_string(ganz) + ".5";
    }

    static Schritt reiterScan(const std::string& reiter) {
        return {"Felder im Reiter " + reiter + " suchen", [=](int b) {
                    if (b < 2) return false;  // zwei Bilder, bis alles steht
                    std::vector<Schritt> neu;
                    int nummer = 0;
                    // 1. Zahlenfelder (Testmarken .../min, .../max)
                    std::vector<std::string> felder;
                    for (const Element& e : g_elemente) {
                        if (!imReiter(e) || e.marke.empty()) continue;
                        const auto ende = e.marke.substr(e.marke.find_last_of('/') + 1);
                        if (ende != "min" && ende != "max") continue;
                        if (std::find(felder.begin(), felder.end(), e.marke) == felder.end()) {
                            felder.push_back(e.marke);
                        }
                    }
                    for (const std::string& m : felder) {
                        const Element* e = findeMarke(m);
                        const int teile = e ? static_cast<int>(innerhalb(*e).size()) : 0;
                        if (teile >= 3) {
                            for (int k = 0; k < 3; ++k) {
                                std::vector<Schritt> a;
                                feld(a, m, zahl(++nummer), k);
                                mitAenderung(neu, reiter + ": " + m + "[" + std::to_string(k) + "]",
                                             std::move(a));
                            }
                        } else {
                            std::vector<Schritt> a;
                            feld(a, m, zahl(++nummer));
                            mitAenderung(neu, reiter + ": " + m, std::move(a));
                        }
                    }
                    // 2. Kurvenart: jede Klappliste einmal auf "wave".
                    for (const Element& e : g_elemente) {
                        if (!imReiter(e) || e.marke.empty()) continue;
                        if (e.marke.size() < 5 || e.marke.substr(e.marke.size() - 5) != "kurve") continue;
                        const std::string m = e.marke;
                        std::vector<Schritt> a;
                        a.push_back(klickMarke(m));
                        a.push_back(klick(tr(Str::TransWave), "##Combo"));
                        mitAenderung(neu, reiter + ": " + m + " = wave", std::move(a));
                    }
                    // 3. Listen: einen Eintrag anlegen und beschriften.
                    for (const Element& e : g_elemente) {
                        if (!imReiter(e) || e.marke.empty()) continue;
                        if (e.marke.size() < 5 || e.marke.substr(e.marke.size() - 5) != "/dazu") continue;
                        const std::string liste = e.marke.substr(0, e.marke.size() - 5);
                        std::vector<Schritt> a;
                        a.push_back(klickMarke(e.marke));
                        a.push_back(warte(2));
                        mitAenderung(neu, reiter + ": " + liste + " Eintrag dazu", std::move(a));
                        std::vector<Schritt> b2;
                        auto zielNummer = std::make_shared<int>(0);
                        b2.push_back(klickAuf("Klick letzter Eintrag " + liste, [=]() -> const Element* {
                            const Element* letzter = nullptr;
                            for (int k = 0; k < 64; ++k) {
                                const Element* x = findeMarke(liste + "/eintrag" + std::to_string(k));
                                if (!x) break;
                                letzter = x;
                            }
                            return letzter;
                        }));
                        b2.push_back(taste(ImGuiKey_A, true));
                        b2.push_back(tippe("selbsttest/eintrag"));
                        b2.push_back(taste(ImGuiKey_Enter));
                        mitAenderung(neu, reiter + ": " + liste + " Eintrag beschriften", std::move(b2));
                    }
                    // 4. Haekchen — zuletzt, weil manche andere Felder sperren.
                    std::vector<std::pair<std::string, std::string>> haken;  // Label, Marke
                    for (const Element& e : g_elemente) {
                        if (!imReiter(e) || !e.ankreuz) continue;
                        haken.emplace_back(e.label, e.marke);
                    }
                    // Gruppenschalter (".../an") ganz ans Ende: sie blenden
                    // ihre Felder aus, und die kaemen danach nicht mehr dran.
                    std::stable_partition(haken.begin(), haken.end(), [](const auto& h) {
                        const std::string& m = h.second;
                        return !(m.size() >= 3 && m.substr(m.size() - 3) == "/an");
                    });
                    for (const auto& [label, marke] : haken) {
                        // "~" macht aus einem Wert eine Spanne: aendert die Datei
                        // erst, wenn das zweite Feld etwas anderes sagt.
                        const bool spanne = sichtbarerText(label) == "~";
                        std::vector<Schritt> a;
                        if (!marke.empty()) {
                            a.push_back(klickMarke(marke));
                        } else {
                            const std::string l = label;
                            a.push_back(klickAuf("Klick Haekchen \"" + sichtbarerText(l) + "\"",
                                                 [=] { return finde(l, "tabbody"); }));
                        }
                        mitAenderung(neu, reiter + ": Haekchen " + (marke.empty() ? sichtbarerText(label) : marke),
                                     std::move(a), !spanne);
                    }
                    diag::info("Selbsttest: Reiter " + reiter + ": " + std::to_string(felder.size()) +
                               " Zahlenfelder, " + std::to_string(haken.size()) + " Haekchen");
                    einfuegen(std::move(neu));
                    return true;
                }};
    }

    static std::vector<Schritt> teilFelder() {
        std::vector<Schritt> s;
        s.push_back(teil("felder"));
        const PrimitiveType typen[] = {
            PrimitiveType::Particle, PrimitiveType::Line, PrimitiveType::Tail,
            PrimitiveType::Cylinder, PrimitiveType::Emitter, PrimitiveType::Sound,
            PrimitiveType::Decal, PrimitiveType::OrientedParticle, PrimitiveType::Electricity,
            PrimitiveType::FxRunner, PrimitiveType::Light, PrimitiveType::CameraShake,
            PrimitiveType::ScreenFlash,
        };
        for (PrimitiveType typ : typen) {
            const std::string name = typeName(typ);
            s.push_back(teil(std::string("felder ") + name));
            frischesDokument(s);
            neuesSegment(s, typ);
            for (fields::Tab reiter : fields::tabsFor(typ)) {
                const std::string label = tr(fields::tabLabel(reiter));
                s.push_back(klick(label, "properties"));
                s.push_back(warteBis("Reiter " + label + " offen",
                                     [reiter] { return app->propertyTab_ == reiter; }, 30));
                {
                    std::string datei = "reiter_" + name + "_" + label;
                    std::replace(datei.begin(), datei.end(), '/', '-');
                    s.push_back(fensterFoto(datei));
                }
                s.push_back(reiterScan(name + "/" + label));
            }
            const std::string pfad = arbeitsOrdner + "/felder_" + name + ".efx";
            s.push_back(dateiAntwort(pfad));
            menue(s, Str::MenuFile, Str::FileSaveAs);
            auto gespeichert = std::make_shared<std::string>();
            s.push_back(tu("Text merken", [=] { *gespeichert = text(); }));
            s.push_back(pruefSchritt(name + ": gespeicherte Datei = Effekt",
                                     [=] { return leseDatei(pfad) == *gespeichert; }));
            s.push_back(pruefSchritt(name + ": gespeicherte Datei laesst sich fehlerfrei lesen", [=] {
                const ReadResult r = read(leseDatei(pfad));
                for (const auto& d : r.diagnostics) {
                    if (d.severity == Severity::Error) {
                        diag::info("  Lesefehler Zeile " + std::to_string(d.line) + ": " + d.message);
                    }
                }
                return !r.hasErrors();
            }));
            frischesDokument(s);
            s.push_back(dateiAntwort(pfad));
            menue(s, Str::MenuFile, Str::FileOpen);
            s.push_back(pruefSchritt(name + ": wieder geoeffnet = gespeichert",
                                     [=] { return text() == *gespeichert; }));
        }
        return s;
    }

    static std::vector<Schritt> ende() {
        std::vector<Schritt> s;
        s.push_back(teil("ende"));
        s.push_back(allesZu());
        s.push_back(tu("Ergebnis schreiben und beenden", [] {
            std::ostringstream o;
            o << "efxed Selbsttest: " << ok << " OK, " << fehler << " FEHLER\n";
            for (const auto& f : fehlerListe) o << "FEHLER " << f << "\n";
            schreibeDatei(paths::configDir() + "/selbsttest_ergebnis.txt", o.str());
            diag::info("Selbsttest fertig: " + std::to_string(ok) + " OK, " +
                       std::to_string(fehler) + " FEHLER");
            // Ungespeicherte Testdokumente sollen beim Beenden nicht fragen.
            for (auto& d : app->documents_) d.dirty = false;
            app->wantsQuit_ = true;
        }));
        return s;
    }

    static std::vector<Schritt> alle(const std::string& auswahl) {
        struct Teil { const char* name; std::vector<Schritt> (*f)(); };
        const Teil teile[] = {
            {"start", &teilStart},
            {"segmente", &teilSegmente},
            {"speichern", &teilSpeichern},
            {"ansicht", &teilAnsicht},
            {"wiedergabe", &teilWiedergabe},
            {"eigenschaften", &teilEigenschaften},
            {"dialoge", &teilDialoge},
            {"liste", &teilListe},
            {"bedienung", &teilBedienung},
            {"beenden", &teilBeenden},
            {"bibliothek", &teilBibliothek},
            {"auswahl", &teilAuswahl},
            {"dokumente", &teilDokumente},
            {"werkzeug", &teilWerkzeug},
            {"nachlegen", &teilNachlegen},
            {"spinner", &teilSpinner},
            {"felder", &teilFelder},
        };
        // Nicht in "alles": dauert mit allen Effekten mehrere Minuten.
        const Teil extra[] = {
            {"darstellung", &teilDarstellung},
        };
        std::vector<Schritt> s;
        for (const Teil& t : teile) {
            const bool gewollt = auswahl == "alles" || auswahl == "1" ||
                                 ("," + auswahl + ",").find("," + std::string(t.name) + ",") !=
                                     std::string::npos;
            if (!gewollt) continue;
            for (Schritt& x : t.f()) s.push_back(std::move(x));
        }
        for (const Teil& t2 : extra) {
            if (("," + auswahl + ",").find("," + std::string(t2.name) + ",") == std::string::npos) continue;
            for (Schritt& x : t2.f()) s.push_back(std::move(x));
        }
        for (Schritt& x : ende()) s.push_back(std::move(x));
        return s;
    }
};

App* Selbsttest::app = nullptr;
render::Renderer* Selbsttest::renderer = nullptr;
std::vector<Selbsttest::Schritt> Selbsttest::schritte;
size_t Selbsttest::schritt = 0;
int Selbsttest::imSchritt = 0;
int Selbsttest::bild = 0;
int Selbsttest::ok = 0;
int Selbsttest::fehler = 0;
std::vector<std::string> Selbsttest::fehlerListe;
std::string Selbsttest::arbeitsOrdner;
std::string Selbsttest::fotoOrdner;
std::deque<std::string> Selbsttest::dateiAntworten;
std::vector<std::string> Selbsttest::geoeffnetPerShell;
std::string Selbsttest::aktuellerTeil = "vorlauf";
size_t Selbsttest::einfuegeAn = 0;
std::string Selbsttest::fensterFotoName;
bool Selbsttest::punktGewuenscht = false;
ImVec2 Selbsttest::punktOrt;
int Selbsttest::punktFarbe[3] = {-1, -1, -1};
std::string Selbsttest::bericht;

// ===========================================================================
// Einstieg
// ===========================================================================
namespace {
std::string g_auswahl;
}

bool selbsttestVorbereiten() {
    const char* modus = std::getenv("EFXED_SELBSTTEST");
    if (modus == nullptr || modus[0] == '\0') return false;
    g_auswahl = modus;
    g_an = true;
    // Eigener Einstellungsordner: der Test liest und schreibt NIE die echten
    // Einstellungen. Neben der .exe, damit mehrere Laeufe nebeneinander gehen.
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    fs::path ordner = fs::path(exe).parent_path() / "selbsttest";
    std::error_code ec;
    fs::remove_all(ordner, ec);
    fs::create_directories(ordner / "arbeit", ec);
    paths::setOverrideDirForTesting(ordner.string());
    Selbsttest::arbeitsOrdner = (ordner / "arbeit").string();
    if (const char* f = std::getenv("EFXED_FOTOS"); f && f[0]) {
        Selbsttest::fotoOrdner = f;
        fs::create_directories(Selbsttest::fotoOrdner, ec);
    }
    return true;
}

bool selbsttestAktiv() { return g_an; }

void selbsttestNachZeichnen(render::Renderer* renderer) {
    if (!g_an || renderer == nullptr) return;
    if (Selbsttest::punktGewuenscht) {
        Selbsttest::punktGewuenscht = false;
        std::vector<unsigned char> bild;
        int bw = 0, bh = 0;
        const int x = static_cast<int>(Selbsttest::punktOrt.x), y = static_cast<int>(Selbsttest::punktOrt.y);
        if (renderer->readBackbuffer(bild, bw, bh) && x >= 0 && y >= 0 && x < bw && y < bh) {
            for (int k = 0; k < 3; ++k) Selbsttest::punktFarbe[k] = bild[(static_cast<size_t>(y) * bw + x) * 4 + k];
        } else {
            for (int& k : Selbsttest::punktFarbe) k = -1;
        }
    }
    if (Selbsttest::fensterFotoName.empty()) return;
    std::vector<unsigned char> rgba;
    int w = 0, h = 0;
    if (renderer->readBackbuffer(rgba, w, h)) {
        const auto tga = image::encodeTga(rgba.data(), w, h);
        std::ofstream f(Selbsttest::fotoOrdner + "/" + Selbsttest::fensterFotoName + ".tga",
                        std::ios::binary);
        f.write(reinterpret_cast<const char*>(tga.data()), static_cast<std::streamsize>(tga.size()));
    } else {
        Selbsttest::meldeFehler("Fensterfoto " + Selbsttest::fensterFotoName + ": nicht lesbar");
    }
    Selbsttest::fensterFotoName.clear();
}

void selbsttestVorBild() {
    if (!g_an) return;
    if (g_mausAn) ImGui::GetIO().AddMousePosEvent(g_maus.x, g_maus.y);
}

void selbsttestNachBild(App& app, render::Renderer* renderer) {
    if (!g_an) return;
    using T = Selbsttest;
    T::renderer = renderer;
    ImGuiContext& g = *ImGui::GetCurrentContext();
    g.TestEngineHookItems = true;
    ++T::bild;

    if (T::bild == 1) {
        T::app = &app;
        // Die Fenster von Windows durch eigene Antworten ersetzen.
        app.setFileDialog([](bool save, const char*, const char*) -> std::string {
            if (T::dateiAntworten.empty()) {
                T::meldeFehler(std::string("unerwarteter Dateidialog (") +
                               (save ? "Speichern" : "Oeffnen") + ")");
                return {};
            }
            std::string p = T::dateiAntworten.front();
            T::dateiAntworten.pop_front();
            return p;
        });
        app.setFolderDialog([](const char*, const char*) -> std::string {
            if (T::dateiAntworten.empty()) {
                T::meldeFehler("unerwartete Ordnerauswahl");
                return {};
            }
            std::string p = T::dateiAntworten.front();
            T::dateiAntworten.pop_front();
            return p;
        });
        app.setShellOpen([](const std::string& p) { T::geoeffnetPerShell.push_back(p); });
        // Englisch, wie das Original — die Beschriftungen kommen ohnehin aus tr().
        i18n::setLanguage(i18n::Language::English);
        app.settings().languageCode = "en";
        // Kein Ton aus dem Lautsprecher, solange getestet wird.
        app.settings().playSounds = false;
        T::schritte = T::alle(g_auswahl);
        diag::info("Selbsttest: " + std::to_string(T::schritte.size()) + " Schritte (" +
                   g_auswahl + ")");
    }

    // Ein paar Bilder Vorlauf, bis alles einmal gezeichnet ist.
    if (T::bild > 5 && T::schritt < T::schritte.size()) {
        if (T::imSchritt == 0) diag::info("Selbsttest Schritt: " + T::schritte[T::schritt].text);
        // Kopie: ein Schritt darf weitere einfuegen, dabei zieht der Vektor um.
        const std::function<bool(int)> tun = T::schritte[T::schritt].tun;
        T::einfuegeAn = T::schritt + 1;
        if (tun(T::imSchritt)) {
            ++T::schritt;
            T::imSchritt = 0;
        } else if (++T::imSchritt > 6000) {
            T::meldeFehler(T::schritte[T::schritt].text + ": haengt");
            ++T::schritt;
            T::imSchritt = 0;
        }
    }
    g_elemente.clear();
}

}  // namespace efx::gui

// ===========================================================================
// Die Haken, die ImGui mit IMGUI_ENABLE_TEST_ENGINE aufruft. Im globalen
// Namensraum, so deklariert sie imgui_internal.h.
// ===========================================================================
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx, ImGuiID id, const ImRect& bb,
                                 const ImGuiLastItemData* data) {
    efx::gui::selbsttestMerkeElement(ctx, id, bb, data);
}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext* ctx, ImGuiID id, const char* label,
                                  ImGuiItemStatusFlags flags) {
    efx::gui::selbsttestMerkeText(ctx, id, label, flags);
}

void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}

const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }
