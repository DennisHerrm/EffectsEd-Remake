// Der Effektbrowser: viele Effekte nebeneinander, alle laufend.
//
// Der Zweck ist Suchen, nicht Bearbeiten. Wer in einem Mod nach „dem mit den
// blauen Funken" sucht, kennt den Dateinamen nicht — aber er erkennt den
// Effekt, sobald er ihn sieht. Genau dafür laufen die Vorschauen: ein
// Standbild von einer Explosion nach 20 ms ist ein schwarzes Rechteck.
//
// Aufbau: ein Raster von Kacheln. Jede Kachel hat ihr eigenes Partikelsystem
// und ihre eigene Uhr, und alle werden in **eine** Zeichenliste gelegt, die
// einmal ins Bild geht — nicht eine Ansicht je Kachel. Sechzig eigene
// Ansichten wären sechzig Renderdurchgänge je Bild.
#include "app.h"
#include "testmarke.h"
#include "app_shared.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>

#include "efx/assets.h"
#include "efx/camera.h"
#include "efx/diag.h"
#include "efx/i18n.h"
#include "efx/shader.h"
#include "efx/tiles.h"
#include "efx/io.h"
#include "efx/jobs.h"

#include "imgui.h"
#include "icons.h"
#include "theme_imgui.h"

namespace efx::gui {

using i18n::Str;
using i18n::tr;

void App::refreshBrowser() {
    browserEntries_.clear();
    for (const auto& name : assets_.effects) {
        BrowserEntry entry;
        entry.name = name;
        browserEntries_.push_back(std::move(entry));
    }
    diag::info("browser: " + std::to_string(browserEntries_.size()) + " effects");
}

bool App::openArchive(const std::string& path) {
    // Ein einzelnes .pk3 als Quelle. Der Spielpfad bleibt unberuehrt: wer nur
    // in eine Mod-Datei hineinsehen will, soll seine Einstellungen nicht
    // umstellen muessen.
    // Gleich wird in `assets_` eingemischt — erst die Arbeitsfaeden anhalten.
    settleTextureJobs();
    const auto index = assets::scanArchive(path);
    const size_t slash = path.find_last_of("/\\");
    const std::string shortName = slash == std::string::npos ? path : path.substr(slash + 1);
    const bool usable = !index.effects.empty() || !index.shaders.empty() ||
                        !index.textures.empty() || !index.sounds.empty() || !index.models.empty();
    char message[512];
    if (!usable) {
        // Sichtbar sagen, statt nur ins Protokoll zu schreiben.
        diag::warn("archive has nothing usable: " + path);
        std::snprintf(message, sizeof(message), tr(Str::ArchiveEmpty), shortName.c_str());
        screenshotMessage_ = message;
        screenshotMessageUntil_ = static_cast<float>(ImGui::GetTime()) + 6.0f;
        return false;
    }

    // In den Bestand einmischen — mit Vorrang (siehe mergeOpenedArchive).
    assets::mergeOpenedArchive(assets_, index);
    std::snprintf(message, sizeof(message), tr(Str::ArchiveOpened), shortName.c_str(),
                  static_cast<int>(index.effects.size()), static_cast<int>(index.textures.size()),
                  static_cast<int>(index.sounds.size()), static_cast<int>(index.models.size()));
    screenshotMessage_ = message;
    screenshotMessageUntil_ = static_cast<float>(ImGui::GetTime()) + 6.0f;
    // Die Bibliothek zeigt zuerst nur dieses Archiv; "Alle Quellen" schaltet
    // zurueck. Ohne Effekte darin bleibt die Auswahl, wie sie war.
    // Den Suchtext leeren: stand dort noch etwas von vorhin, blendete er die
    // Effekte des Archivs aus, und die Bibliothek wirkte leer.
    if (!index.effects.empty()) {
        browserSource_ = path;
        browserFilter_[0] = '\0';
    }
    // Texturen, die bisher als fehlend galten, koennen jetzt da sein.
    textureCacheDirty_ = true;
    missingTextures_.clear();

    archiveSources_.push_back(path);
    browserEntries_.clear();
    refreshBrowser();
    diag::info("archive opened: " + path + " (" +
               std::to_string(index.effects.size()) + " effects)");
    return true;
}

void App::drawStartPage(render::Renderer* renderer, float dpiScale) {
    if (browserEntries_.empty() && !assets_.effects.empty()) refreshBrowser();

    // Ohne Spielpfad hat der Browser nichts zu zeigen. Statt einer leeren
    // Flaeche die drei Wege, die von hier wegfuehren — das ist der Zweck einer
    // Startseite.
    if (assets_.effects.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 24.0f * dpiScale));
        ImGui::Indent(24.0f * dpiScale);
        ImGui::TextUnformatted(tr(Str::StartHeading));
        ImGui::Spacing();
        ImGui::TextDisabled("%s", tr(Str::StartNoPath));
        ImGui::Spacing();
        ImGui::Spacing();

        const ImVec2 wide(300.0f * dpiScale, 0.0f);
        if (ImGui::Button(tr(Str::StartSetPath), wide)) {
            showGamePathDialog_ = true;
        }
        if (ImGui::Button(tr(Str::StartOpenPk3), wide) && fileDialog_) {
            const std::string picked = fileDialog_(
                false, "PK3-Archive (*.pk3)\0*.pk3\0Alle Dateien\0*.*\0",
                nullptr);
            if (!picked.empty()) openArchive(picked);
        }
        if (ImGui::Button(tr(Str::StartOpenFile), wide) && fileDialog_) {
            const std::string picked = fileDialog_(
                false, "Effektdateien (*.efx)\0*.efx\0Alle Dateien\0*.*\0",
                nullptr);
            if (!picked.empty()) openFile(picked);  // zeigt selbst den Editor
        }
        if (ImGui::Button(tr(Str::StartNewEffect), wide)) {
            // Ueber den Befehl, der auch den Reiter AUSWAEHLT. Vorher wurde nur
            // startTabActive_ geloescht - die Reiterleiste hielt "Start" fest,
            // und im naechsten Bild war die Startseite wieder vorn.
            cmdNew();
        }
        ImGui::Unindent(24.0f * dpiScale);
        return;
    }

    drawBrowser(renderer, dpiScale);
}

void App::drawBrowser(render::Renderer* renderer, float dpiScale) {
    // Wer im letzten Bild unter dem Zeiger stand, gilt fuer dieses; wer in
    // diesem darunter steht, wird gleich beim Zeichnen der Kacheln vermerkt.
    BrowserEntry* const hoveredLastFrame = browserHovered_;
    browserHovered_ = nullptr;

    // Kopfzeile: Filter und Kachelgröße.
    ImGui::SetNextItemWidth(220.0f * dpiScale);
    ImGui::InputTextWithHint("##browserFilter", tr(Str::BrowserFilter),
                             browserFilter_, sizeof(browserFilter_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f * dpiScale);
    ImGui::SliderFloat("##tileSize", &browserTileSize_, 90.0f, 260.0f, "%.0f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::BrowserSize));
    // Quellenauswahl. Nur zeigen, wenn es ueberhaupt mehr als eine gibt —
    // ein Auswahlfeld mit einem Eintrag ist Platzverschwendung.
    {
        std::vector<std::string> sources;
        for (const auto& entry : assets_.effectSources) {
            bool known = false;
            for (const auto& seen : sources) {
                if (seen == entry.second) known = true;
            }
            if (!known) sources.push_back(entry.second);
        }
        std::sort(sources.begin(), sources.end());
        if (sources.size() > 1) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(240.0f * dpiScale);
            const auto shortName = [](const std::string& path) {
                const size_t at = path.find_last_of("/\\");
                return at == std::string::npos ? path : path.substr(at + 1);
            };
            const std::string current = browserSource_.empty()
                                            ? std::string(tr(Str::BrowserSourceAll))
                                            : shortName(browserSource_);
            if (ImGui::BeginCombo("##browserSource", current.c_str())) {
                if (ImGui::Selectable(tr(Str::BrowserSourceAll),
                                      browserSource_.empty())) {
                    browserSource_.clear();
                }
                for (const auto& one : sources) {
                    ImGui::PushID(one.c_str());
                    if (ImGui::Selectable(shortName(one).c_str(),
                                          browserSource_ == one)) {
                        browserSource_ = one;
                    }
                    // Der volle Pfad im Kurzhinweis: zwei Mods koennen
                    // gleich heissen, ihre Pfade nicht.
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", one.c_str());
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tr(Str::BrowserSource));
            }
        }
    }

    ImGui::SameLine();
    ImGui::Checkbox(tr(Str::BrowserAnimateAll), &browserAnimateAll_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tr(Str::BrowserAnimateAllHint));
    }

    if (browserEntries_.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("%s", tr(Str::BrowserEmpty));
        return;
    }

    // Nur was zum Filter passt. Kleinbuchstaben beidseitig, damit die Suche
    // sich nicht an der Schreibweise stört.
    std::string needle = browserFilter_;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::vector<BrowserEntry*> shown;
    for (auto& entry : browserEntries_) {
        if (!needle.empty() && entry.name.find(needle) == std::string::npos) {
            continue;
        }
        if (!browserSource_.empty() &&
            assets_.sourceOf(entry.name) != browserSource_) {
            continue;
        }
        shown.push_back(&entry);
    }

    ImGui::SameLine();
    ImGui::TextDisabled(tr(Str::BrowserCount), static_cast<int>(shown.size()),
                        static_cast<int>(browserEntries_.size()));
    ImGui::SameLine();
    ImGui::TextDisabled("|  %s", tr(Str::BrowserHint));

    // Fehlende Bilder benennen, nicht verstecken.
    //
    // Der weiche Fleck macht eine fehlende Textur ertraeglich anzusehen — er
    // darf sie aber nicht verschweigen, sonst sucht man den Fehler beim Effekt
    // statt beim Spielpfad.
    if (!missingTextures_.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextColored(toImGui(activeTheme(settings_).palette.warning),
                           tr(Str::BrowserMissing), texturesMissing_);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(tr(Str::BrowserMissingHint));
            ImGui::Separator();
            for (const auto& name : missingTextures_) {
                ImGui::TextUnformatted(name.c_str());
            }
            if (texturesMissing_ > static_cast<int>(missingTextures_.size())) {
                ImGui::TextDisabled("...");
            }
            ImGui::EndTooltip();
        }
    }
    ImGui::Separator();

    ImGui::BeginChild("##browserGrid", ImVec2(0, 0), false);

    // Die Breite ERST HIER holen — im Kind, nicht davor.
    //
    // Draussen ist der Rollbalken noch nicht abgezogen, und die Spaltenzahl
    // fiel deshalb gelegentlich um eine zu hoch aus.
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float available = ImGui::GetContentRegionAvail().x;

    // Der Regler gibt die WUNSCHgroesse vor, nicht die endgueltige.
    //
    // Vorher blieb rechts ein Streifen liegen: was nach der letzten ganzen
    // Spalte uebrig war, blieb leer — bei zehn Spalten schnell hundert Punkte.
    // Jetzt wird der Rest auf die Spalten verteilt, und das Raster steht
    // buendig bis zum Rand.
    //
    // Die Vorschau wird davon NICHT feiner gerechnet: dafuer zaehlt weiter
    // die Wunschgroesse (gedeckelt), sonst haengt die Rechenlast an der
    // Fensterbreite.
    const float requested = browserTileSize_ * dpiScale;
    const int columns = std::max(1, static_cast<int>(
        (available + spacing) / (requested + spacing)));
    const float tile =
        (available - static_cast<float>(columns - 1) * spacing) /
        static_cast<float>(columns);

    const float labelHeight = ImGui::GetTextLineHeightWithSpacing();
    const float rowHeight = tile + labelHeight + spacing;
    const int rows = (static_cast<int>(shown.size()) + columns - 1) / columns;

    // Welche Zeilen sind sichtbar?
    //
    // Selbst gerechnet statt ueber ImGuiListClipper: der Clipper verlangt,
    // dass man die Elemente schon einreicht, waehrend er laeuft. Wir brauchen
    // die Liste aber VORHER — die Kacheln werden in einem Zug gezeichnet, und
    // erst danach kommt die Bedienung darueber.
    const float scroll = ImGui::GetScrollY();
    const float visibleHeight = ImGui::GetContentRegionAvail().y;
    const int firstRow = std::max(0, static_cast<int>(scroll / rowHeight));
    const int lastRow = std::min(rows, static_cast<int>(
        (scroll + visibleHeight) / rowHeight) + 2);

    // Die Plaetze im Ansichtsziel. Es ist DASSELBE Ziel wie fuer die
    // Bearbeitungsansicht — beide sind nie gleichzeitig sichtbar, also braucht
    // es kein zweites.
    browserSlots_.clear();
    const float sheetTop = static_cast<float>(firstRow) * rowHeight;

    // Wie fein eine Vorschau gerechnet wird — NICHT wie gross sie erscheint.
    //
    // Vorher war das dasselbe: das Blatt war so gross wie die ganze sichtbare
    // Flaeche, jede Kachel wurde in Anzeigegroesse gerechnet. Bei Reglerwert
    // 260 und DPI 144 sind das 390x390 je Kachel, und dazwischen lagen
    // Beschriftungszeilen und Luecken, die mitgeloescht wurden, ohne je etwas
    // zu zeigen.
    //
    // Eine Vorschau soll erkennen lassen, welcher Effekt es ist. Dafuer
    // reichen 224 Punkte; darueber wird nur die Grafikkarte beschaeftigt. Der
    // Unterschied ist quadratisch: 390 gegen 224 ist Faktor 3.
    constexpr int kPreviewPixels = 224;
    const int previewSize =
        std::min(static_cast<int>(requested), kPreviewPixels);

    // Vorladen, bevor es sichtbar wird.
    //
    // `lastRow` reicht absichtlich zwei Zeilen ueber den Sichtbereich hinaus.
    // Die werden nicht gezeichnet, aber angefordert — bis man hinrollt, sind
    // sie fertig. So macht es jeder Bildbrowser: Chromium laedt Bilder
    // ausserhalb des Sichtfensters frueh genug, damit sie beim Hinrollen
    // schon dastehen.
    //
    // Gedeckelt, damit ein Sprung ans Listenende nicht siebenhundert
    // Auftraege auf einmal ausloest.
    {
        // Auftraege kosten den Hauptfaden fast nichts — sie werden nur in
        // die Schlange gelegt. Der Deckel schuetzt nicht vor Rechenlast,
        // sondern davor, bei einem Sprung ans Listenende siebenhundert
        // Auftraege abzusetzen, von denen keiner mehr gebraucht wird.
        constexpr int kRequestsPerFrame = 24;
        int started = 0;
        for (int row = firstRow; row < lastRow && started < kRequestsPerFrame; ++row) {
            for (int column = 0; column < columns; ++column) {
                const size_t at = static_cast<size_t>(row) * columns + column;
                if (at >= shown.size()) break;
                BrowserEntry& entry = *shown[at];
                if (entry.loaded || entry.parsed) continue;
                requestBrowserEntry(entry);
                if (++started >= kRequestsPerFrame) break;
            }
        }
    }

    // Erst sammeln, was sichtbar ist — die Blattgroesse haengt an der Anzahl.
    for (int row = firstRow; row < lastRow; ++row) {
        for (int column = 0; column < columns; ++column) {
            const size_t at = static_cast<size_t>(row) * columns + column;
            if (at >= shown.size()) break;
            TileSlot slot;
            slot.entry = shown[at];
            slot.gridX = static_cast<float>(column) * (tile + spacing);
            slot.gridY = static_cast<float>(row) * rowHeight;
            slot.gridSize = tile;
            if (slot.gridY + tile < sheetTop ||
                slot.gridY > sheetTop + visibleHeight) {
                continue;
            }
            browserSlots_.push_back(slot);
        }
    }

    // Und jetzt Plaetze im Blatt vergeben. Das Blatt hat keine Luecken und
    // keine Beschriftungszeilen — nur Kacheln, Kante an Kante.
    //
    // Wichtig ist, dass ein Effekt SEINEN Platz behaelt, solange er sichtbar
    // ist. Nur so kann eine Kachel ueber mehrere Bilder stehenbleiben, statt
    // in jedem neu gezeichnet zu werden — und genau das ist der Unterschied
    // zwischen viertausend Zeichenaufrufen je Bild und ein paar hundert.
    // Plaetze vergeben. Die Regeln stehen im Kern (efx/tiles.h), weil sich
    // dort pruefen laesst, was man von aussen nur als \"ich zeige auf A und B
    // laeuft los\" bemerkt.
    if (previewSize != browserPreviewSize_) {
        // Andere Aufloesung heisst anderes Blatt — alles noch einmal zeichnen.
        browserCellOwner_.clear();
        browserPreviewSize_ = previewSize;
    }

    // Der Kern rechnet mit Zahlen, nicht mit Zeigern. Die Kennung ist der
    // Platz des Effekts im Bestand — stabil, solange die Liste steht.
    std::vector<int> visible;
    visible.reserve(browserSlots_.size());
    for (const auto& slot : browserSlots_) {
        visible.push_back(static_cast<int>(slot.entry - browserEntries_.data()));
    }

    const auto placement = tiles::assign(browserCellOwner_, visible);
    for (size_t i = 0; i < browserSlots_.size(); ++i) {
        browserSlots_[i].cell = placement[i].cell;
        browserSlots_[i].fresh = placement[i].fresh;
    }

    const int packColumns =
        static_cast<int>(tiles::sheetColumns(browserCellOwner_.size()));
    for (auto& slot : browserSlots_) {
        const size_t cell = static_cast<size_t>(slot.cell);
        slot.x = static_cast<int>(cell % static_cast<size_t>(packColumns)) * previewSize;
        slot.y = static_cast<int>(cell / static_cast<size_t>(packColumns)) * previewSize;
        slot.size = previewSize;
    }
    const int packRows =
        (static_cast<int>(browserCellOwner_.size()) + packColumns - 1) /
        std::max(1, packColumns);
    const int sheetWidth = std::max(1, packColumns * previewSize);
    const int sheetHeight = std::max(1, packRows * previewSize);

    // Aendert sich die Aufteilung des Blattes, muss ALLES neu gezeichnet
    // werden — auch was seinen Platz behalten hat.
    //
    // Das war der Fehler hinter "ich zeige auf A und B laeuft". Die
    // Platznummer bleibt, aber wo dieser Platz im Blatt LIEGT, haengt an der
    // Spaltenzahl, und die waechst mit der Zahl der Plaetze. Platz 7 sitzt bei
    // 30 Plaetzen in Spalte 1 / Zeile 1 und bei 50 Plaetzen in Spalte 7 /
    // Zeile 0. Eine stehengebliebene Kachel las danach an einer voellig
    // anderen Stelle — und zeigte einen fremden Effekt.
    //
    // Dazu kommt: bei geaenderter Groesse legt der Renderer ein neues
    // Ansichtsziel an. Der alte Inhalt ist dann ohnehin weg, und was in der
    // frischen Textur steht, ist unbestimmt.
    if (packColumns != browserSheetColumns_ || sheetWidth != browserSheetWidth_ ||
        sheetHeight != browserSheetHeight_ || viewportUsedByEditor_ ||
        browserNeedsRedraw_) {
        for (auto& slot : browserSlots_) slot.fresh = true;
        browserSheetColumns_ = packColumns;
        // Das Ziel gehoert wieder uns. Siehe die Begruendung bei
        // viewportUsedByEditor_ in app.cpp.
        viewportUsedByEditor_ = false;
        browserNeedsRedraw_ = false;
    }

    browserSheetWidth_ = sheetWidth;
    browserSheetHeight_ = sheetHeight;

    // Welche Kacheln kommen in diesem Bild ueberhaupt dran?
    //
    // Das ist die Entscheidung, an der die Bildrate haengt — und sie war
    // falsch. Bisher lief JEDE sichtbare Vorschau dauernd: laden, simulieren,
    // Geometrie bauen, zeichnen, jedes Bild, fuer alle. Bei achtzehn Kacheln
    // mit grossen Effekten waren das 9 Bilder je Sekunde.
    //
    // So macht es niemand. In Unreals Content Browser sind laufende Vorschauen
    // nicht die Vorgabe, sondern ein eigener Modus, den man einschaltet
    // ("Thumbnail Edit Mode"). Godot erzeugt Vorschauen einmal im Hintergrund
    // und legt sie ab. Der Grund ist derselbe: ein Raster ist zum SUCHEN da,
    // und dafuer genuegt ein Bild, das den Effekt erkennen laesst.
    //
    // Also:
    //   - neu aufgetaucht  -> einmal zeichnen, auf dem besten Zeitpunkt
    //   - unter dem Zeiger -> laeuft, solange man draufzeigt
    //   - alles andere     -> bleibt stehen und kostet nichts
    //
    // Wer es doch bewegt haben will, schaltet es oben ein.
    const float stepMs = ImGui::GetIO().DeltaTime * 1000.0f;
    browserDue_.clear();
    for (size_t i = 0; i < browserSlots_.size(); ++i) {
        const TileSlot& slot = browserSlots_[i];
        const bool hovered = slot.entry == hoveredLastFrame;
        if (!slot.fresh && !hovered && !browserAnimateAll_) continue;
        browserDue_.push_back(i);
    }

    for (size_t i : browserDue_) {
        BrowserEntry& entry = *browserSlots_[i].entry;
        ensureBrowserEntry(entry);
        // Wie oben: ueber genau eine Wiederholung, sonst ueber das ganze Leben.
        const float repeat = static_cast<float>(entry.effect.repeatDelay);
        entry.clock.setDuration(repeat >= 1.0f ? repeat
                                               : entry.system.durationMs());

        const bool hovered = &entry == hoveredLastFrame;
        if (!entry.everDrawn) {
            // Noch nie gezeichnet: auf den Zeitpunkt stellen, an dem am
            // meisten zu sehen ist. Bei 0 ms hat ein Aufschlag noch nichts
            // ausgeloest — die Kachel bliebe schwarz, und im Raster sieht das
            // aus, als sei der Effekt kaputt.
            entry.clock.scrubTo(entry.bestTimeMs);
            entry.everDrawn = true;
        } else if (hovered || browserAnimateAll_) {
            entry.clock.advance(stepMs);
        }
        // Sonst: die Kachel wird nur neu gezeichnet — etwa weil eine Textur
        // eingetroffen ist. Ihre Zeit bleibt, wo sie war.
    }

    // Einmal alle Kacheln zeichnen — ein Zielwechsel statt sechzig.
    renderBrowserTiles(renderer, dpiScale);

    // Ein einziger Bezugspunkt fuer Bild UND Bedienung.
    //
    // Vorher wurde das Bild ueber GetCursorScreenPos gesetzt (das den Bildlauf
    // bereits enthaelt) und dann noch einmal `- scroll` gerechnet. Der Abzug
    // stand also doppelt drin: gescrollt lagen Bild und Kachel um genau den
    // Bildlauf auseinander. Dazu kam die Fensterfuellung, die in
    // SetCursorPos steckt, aber nicht in GetCursorScreenPos.
    //
    // Deshalb jetzt beides ueber SetCursorScreenPos aus derselben Ecke.
    const ImVec2 gridOrigin = ImGui::GetCursorScreenPos();

    ImGui::Dummy(ImVec2(0.0f, static_cast<float>(rows) * rowHeight));

    // Jede Kachel holt sich ihren AUSSCHNITT aus dem Blatt.
    //
    // Vorher war es ein einziges grosses Bild, das ueber das ganze Raster
    // gelegt wurde. Das ging so lange gut, wie seine Lage auf den Punkt
    // stimmte — und wenn nicht, verschob sich alles gemeinsam, was als
    // Nachziehen sichtbar wurde.
    //
    // Jetzt ist das Bild einer Kachel an ihr Rechteck genagelt. Eine
    // Verschiebung zwischen Bild und Bedienung ist damit nicht mehr moeglich,
    // nicht nur unwahrscheinlich.
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const bool flipped = renderer && renderer->viewportTextureFlipped();

    int wanted = 0;
    for (const auto& slot : browserSlots_) {
        const ImVec2 corner(gridOrigin.x + slot.gridX, gridOrigin.y + slot.gridY);

        if (renderer && sheetWidth > 0 && sheetHeight > 0) {
            // Die Rechnung steht im Kern, weil sie sich dort nachrechnen
            // laesst — und weil sie schon einmal falsch war: ein getauschtes
            // statt gespiegeltes v liess jede Kachel den Ausschnitt einer
            // anderen zeigen, und zwar nur unter OpenGL.
            const auto uv = tiles::uvFor(slot.x, slot.y, slot.size, sheetWidth,
                                         sheetHeight, flipped);
            draw->AddImage(renderer->viewportTexture(), corner,
                           ImVec2(corner.x + slot.gridSize,
                                  corner.y + slot.gridSize),
                           ImVec2(uv.u0, uv.v0), ImVec2(uv.u1, uv.v1));
        }

        // Diagnose: Platznummer und Lage im Blatt hineinschreiben. Damit
        // laesst sich eine Fehlzuordnung an einem Bildschirmfoto ablesen,
        // statt sie zu erraten.
        if (browserShowCells_) {
            char label[64];
            std::snprintf(label, sizeof(label), "#%d @%d,%d%s", slot.cell,
                          slot.x, slot.y, slot.fresh ? " neu" : "");
            draw->AddText(ImVec2(corner.x + 4.0f, corner.y + 4.0f),
                          IM_COL32(255, 220, 60, 255), label);
        }

        // Danach die Bedienung, damit sie ueber dem Bild liegt.
        ImGui::SetCursorScreenPos(corner);
        drawBrowserTile(*slot.entry, tile);
        ++wanted;
    }

    ImGui::EndChild();

    browserVisible_ = wanted;
}

void App::renderBrowserTiles(render::Renderer* renderer, float dpiScale) {
    (void)dpiScale;
    if (!renderer || browserSlots_.empty()) return;
    if (browserSheetWidth_ <= 0 || browserSheetHeight_ <= 0) return;

    // Dasselbe Ansichtsziel wie die Bearbeitungsansicht — die beiden sind nie
    // gleichzeitig sichtbar. Ein zweites Ziel waere Speicher fuer nichts.
    renderer->resizeViewport(browserSheetWidth_, browserSheetHeight_);

    // Welche Kacheln faellig sind, hat drawBrowser entschieden — dort werden
    // sie auch geladen und ihre Uhren gestellt. Hier wird nur gezeichnet.
    const std::vector<size_t>& due = browserDue_;
    browserRedrawn_ = static_cast<int>(due.size());

    const auto buildStart = std::chrono::steady_clock::now();

    // --- Simulation parallel ------------------------------------------------
    //
    // Der teure Teil je Kachel ist das Aufbauen der Geometrie: fuer jedes
    // lebende Teilchen Lage, Groesse und Farbe ausrechnen. Das haengt zwischen
    // den Kacheln an nichts — jede hat ihr eigenes System und ihre eigene Uhr.
    //
    // Also alle auf einmal, ueber den vorhandenen Arbeitsverteiler. Auf einem
    // Rechner mit vielen Kernen ist das der eigentliche Gewinn: die Zeichnung
    // selbst muss ohnehin der Reihe nach passieren, weil die Grafikkarte einen
    // Faden hat.
    std::vector<particles::DrawList> lists(browserSlots_.size());

    // Die Kameras ZUERST — vor dem Aufbau der Geometrie.
    //
    // Der Grund ist der Fehler, den es vorher gab: Billboards werden entlang
    // der Kameraachsen aufgespannt. Baut man sie mit festen Achsen auf und
    // dreht die Kamera danach, stehen sie hochkant zur Kamera — und eine
    // Flaeche ohne Dicke ist von der Seite unsichtbar.
    //
    // Ein Muzzleflash besteht oft aus EINEM solchen Viereck und verschwand
    // deshalb ganz, waehrend eine Funkenwolke nur duenner aussah. Genau das
    // war der Unterschied zum Editor: der holt die Achsen aus der
    // Blickmatrix.
    std::vector<camera::Matrix> views(browserSlots_.size());
    std::vector<camera::Matrix> projections(browserSlots_.size());
    std::vector<camera::Vec3> rights(browserSlots_.size());
    std::vector<camera::Vec3> ups(browserSlots_.size());
    std::vector<camera::Vec3> eyes(browserSlots_.size());

    for (size_t i : due) {
        camera::Orbit view;
        view.reset(settings_.worldScale);
        view.setDistance(view.distanceToFit(browserSlots_[i].entry->reach));
        // Leicht von der Seite und von oben, damit Tiefe erkennbar ist. Bei
        // reiner Frontsicht sieht jede Kugelwolke gleich aus.
        view.orbit(30.0f, 20.0f);
        eyes[i] = view.position();
        views[i] = view.viewMatrix();
        projections[i] = view.projectionMatrix(1.0f,
                                               renderer->wantsZeroToOneDepth());
        // Die ersten beiden Zeilen der Blickmatrix sind rechts und oben in
        // Weltkoordinaten — dieselbe Rechnung wie im Editor.
        rights[i] = camera::Vec3{views[i][0], views[i][4], views[i][8]};
        ups[i] = camera::Vec3{views[i][1], views[i][5], views[i][9]};
    }

    // Koernung 1: eine Kachel je Aufgabe. Der Aufbau einer Kachel mit ein
    // paar hundert Teilchen ist teuer genug, dass sich das Verteilen lohnt —
    // bei groesserer Koernung bliebe auf einem 32-Kern-Rechner die Haelfte
    // ungenutzt, weil selten mehr als ein Dutzend Kacheln sichtbar sind.
    jobs::pool().parallelFor(due.size(), 1u,
                             [&](size_t begin, size_t end) {
        for (size_t n = begin; n < end; ++n) {
            const size_t i = due[n];
            const BrowserEntry& entry = *browserSlots_[i].entry;
            // Die Uhr steht schon — drawBrowser hat sie auf dem Hauptfaden
            // weitergestellt. Hier wird nur noch GELESEN.
            //
            // Das ist Absicht und nicht Geschmack: eine Aufgabe, die nichts
            // schreibt, kann sich mit keiner anderen ins Gehege kommen, egal
            // wie die Kacheln spaeter verteilt werden.
            // Dieselbe Shaderabfrage und dieselben Zeichengruppen wie im
            // Editor (particleShaderLookup, drawParticleGroups) — die Kachel
            // soll aussehen wie die Vorschau.
            particles::System::View eye;
            eye.eye = eyes[i];
            eye.fovXDegrees = 60.0f;
            lists[i] = entry.system.build(entry.clock.timeMs(), rights[i], ups[i],
                                          particleShaderLookup(), &eye);
        }
    });

    // Was kostet die Kachel unter dem Zeiger? Sie ist meist die einzige, die
    // laeuft — also entscheidet sie ueber die Bildrate.
    browserHoveredName_.clear();
    browserHoveredLive_ = 0;
    browserHoveredDrawn_ = 0;
    browserHoveredVertices_ = 0;
    for (size_t i : due) {
        const BrowserEntry& entry = *browserSlots_[i].entry;
        if (&entry != browserHovered_ && !browserAnimateAll_) continue;
        browserHoveredName_ = entry.name;
        browserHoveredLive_ = static_cast<int>(entry.system.live().size());
        browserHoveredDrawn_ = lists[i].drawn;
        for (const auto& group : lists[i].byTexture) {
            browserHoveredVertices_ += static_cast<int>(group.second.vertices.size());
        }
        break;
    }

    browserBuildMs_ = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - buildStart).count();
    const auto drawStart = std::chrono::steady_clock::now();
    browserDrawCalls_ = 0;

    // --- Zeichnen der Reihe nach --------------------------------------------
    const theme::Palette& palette = activeTheme(settings_).palette;
    renderer->beginViewportPreserving();
    renderer->setDepthWrite(false);
    renderer->setCulling(render::Cull::None);

    for (size_t i : due) {
        const TileSlot& slot = browserSlots_[i];
        if (slot.size <= 0) continue;

        // NUR den Ausschnitt umstellen, nicht das Ziel. Das ist der ganze
        // Unterschied: ein Zielwechsel leert die Puffer der Grafikkarte, ein
        // Ausschnitt kostet zwei Zahlen.
        renderer->setViewportRect(slot.x, slot.y, slot.size, slot.size);

        // Nur DIESEN Platz leeren. Das ganze Ziel zu leeren wuerde die
        // Kacheln mitnehmen, die stehenbleiben sollen.
        renderer->clearViewportRect(palette.viewportBg.r, palette.viewportBg.g,
                                    palette.viewportBg.b, 1.0f);

        // Die Kamera steht schon fest — oben berechnet, weil die Geometrie
        // ihre Achsen braucht.
        renderer->setCamera(views[i].data(), projections[i].data());

        // Die Zeichengruppen wie im Editor: je Shaderstufe Bild, Faktorpaar,
        // Tiefe und Reihenfolge der Engine (drawParticleGroups).
        browserDrawCalls_ += drawParticleGroups(
            renderer, lists[i], browserSlots_[i].entry->clock.timeMs() * 0.001f, 0);
    }

    renderer->setViewportRect(0, 0, 0, 0);
    renderer->endViewport();
    browserDrawMs_ = std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - drawStart).count();
}

void App::requestBrowserEntry(BrowserEntry& entry) {
    // Lesen und Zerlegen auf einem Arbeitsfaden.
    //
    // Das war der letzte grosse Brocken auf dem Hauptfaden: eine .efx aus
    // einem .pk3 zu holen heisst auspacken, und das kostet. Beim Rollen kamen
    // zehn neue Kacheln je Zeile — zehnmal auspacken und zerlegen, mitten im
    // Bild. Genau das sah man als Ruckeln.
    //
    // Was hier NICHT passiert: `play`. Das laedt Untereffekte nach und fasst
    // damit gemeinsame Zwischenspeicher an — es gehoert auf den Hauptfaden.
    // Der teure Teil ist ohnehin das Auspacken, und der ist hier drin.
    if (entry.loaded || entry.parsed) return;
    if (!effectsInFlight_.insert(entry.name).second) return;

    const std::string name = entry.name;
    const std::string base = settings_.gamePath;
    jobs::pool().post([this, name, base] {
        auto ready = std::make_shared<LoadedEffect>();
        ready->name = name;

        // `assets_` wird nur gelesen; settleTextureJobs() haelt das ab, wenn
        // sich der Bestand aendert.
        const auto where = assets::findEffect(assets_, base, name);
        if (where.found) {
            ready->path = where.path;
            ready->archive = where.archive;
            std::string error;
            const auto bytes = assets::readFile(base, where, &error);
            if (!bytes.empty()) {
                auto result = read(std::string(bytes.begin(), bytes.end()));
                ready->effect = std::move(result.effect);
                ready->parseMessages = static_cast<int>(result.diagnostics.size());
                ready->unreadable = false;
            }
        }
        jobs::pool().postToMain([this, ready] {
            readyEffects_.push_back(std::move(*ready));
        });
    });
}

void App::collectLoadedEffects() {
    if (readyEffects_.empty()) return;

    // Wie viele Kacheln je Bild zusammengebaut werden.
    //
    // Vier war geraten. Nachgemessen kostet ein Zusammenbau:
    //
    //     play()              0.008 ms
    //     Reichweite messen   0.09  ms
    //     ---------------------------
    //                         0.10  ms
    //
    // Vier sind also 0.4 ms — drei Prozent eines Bildes bei 60 Hz. Zwoelf
    // sind 1.2 ms und damit immer noch unter einem Zehntel; dafuer fuellt
    // sich das Raster dreimal so schnell.
    //
    // Nicht mehr: bei einem Sprung ans Listenende kaemen sonst fuenfzig auf
    // einmal, und dann ist der Ruckler wieder da, nur an anderer Stelle.
    constexpr size_t kAssemblePerFrame = 12;
    const size_t count = std::min(kAssemblePerFrame, readyEffects_.size());

    for (size_t i = 0; i < count; ++i) {
        LoadedEffect& ready = readyEffects_[i];
        for (auto& entry : browserEntries_) {
            if (entry.name != ready.name) continue;
            entry.sourcePath = std::move(ready.path);
            entry.sourceArchive = std::move(ready.archive);
            entry.effect = std::move(ready.effect);
            entry.parseMessages = ready.parseMessages;
            entry.unreadable = ready.unreadable;
            entry.parsed = true;
            // Die Kachel wurde beim Auftauchen schon einmal (leer) gezeichnet
            // und kommt danach nur bei Zeigerkontakt wieder dran. Ohne diese
            // Zeile blieb sie schwarz — in der grossen Bibliothek fiel es
            // nicht auf, weil eintreffende Texturen anderer Kacheln zufaellig
            // alle neu zeichnen liessen; ein einzelnes .pk3 zeigte es.
            browserNeedsRedraw_ = true;
            break;
        }
        effectsInFlight_.erase(ready.name);
    }
    readyEffects_.erase(readyEffects_.begin(),
                        readyEffects_.begin() + static_cast<ptrdiff_t>(count));
}

void App::ensureBrowserEntry(BrowserEntry& entry) {
    // Der Zusammenbau: aus der zerlegten Datei ein laufendes System machen.
    //
    // Erst wenn der Arbeitsfaden geliefert hat. Vorher bleibt die Kachel
    // leer — ein leeres Rechteck fuer zwei Bilder ist ertraeglicher als ein
    // stehendes Fenster.
    if (entry.loaded || !entry.parsed) return;
    entry.loaded = true;

    // Ein eigener Ausgangswert je Kachel, damit nicht alle im Gleichschritt
    // zappeln — nebeneinander faellt das sofort auf.
    entry.system.play(entry.effect,
                      static_cast<unsigned>(std::hash<std::string>{}(entry.name)) | 1u,
                      {}, {},
                      [this](const std::string& name) {
                          return loadChildEffect(name);
                      },
                      {},
                      // Kacheln laufen immer in Schleife, also immer mit
                      // Vorlauf: ein Feuer soll in der Vorschau ein Feuer
                      // sein und nicht ein einzelnes Flaemmchen.
                      true, {}, modelLoader());
    // Ueber genau EINE Wiederholung laufen, wenn der Effekt eine hat: nach
    // `repeatDelay` sieht der Bestand wieder aus wie am Anfang, und die Kachel
    // laeuft nahtlos rund. Ohne das liefe ein Rauch mit 62 Sekunden
    // Lebensdauer eine Minute lang, bevor er sich wiederholt.
    const float repeat = static_cast<float>(entry.effect.repeatDelay);
    entry.clock.setDuration(repeat >= 1.0f ? repeat : entry.system.durationMs());
    entry.clock.setEndMode(timeline::EndMode::Repeat);
    entry.clock.play();

    // Wie weit der Effekt reicht. Steht im Kern, weil es dort pruefbar ist
    // — und weil die hiesige Fassung falsch war: sie zaehlte nur die
    // Bahnen. Ein Decal bewegt sich nicht, seine Bahn ist ein Punkt, die
    // Reichweite kam als 1.0 heraus, und die Kamera landete mitten in der
    // Flaeche. Das waren die einfarbigen Rechtecke im Raster.
    const auto preview = particles::describePreview(entry.system);
    // Die Bilder gleich anfordern, nicht erst beim Zeichnen. Sonst zeigt
    // die Kachel den Ersatzfleck — und sie wird nur EINMAL gezeichnet.
    prefetchTextures(entry.effect);

    entry.reach = preview.reach;
    entry.bestTimeMs = preview.bestTimeMs;
}

void App::drawBrowserTile(BrowserEntry& entry, float size) {
    ImGui::PushID(entry.name.c_str());
    ImGui::BeginGroup();

    // Die Uhr laeuft NICHT hier. Sie wird einmal je Bild in drawBrowser
    // weitergestellt, bevor die Geometrie gebaut wird.
    //
    // Vorher lief sie an beiden Stellen: einmal in renderBrowserTiles und
    // einmal hier. Jede Vorschau lief damit doppelt so schnell wie im Editor,
    // und bei kurzen Effekten sprang sie ueber die Bilder hinweg, in denen
    // ueberhaupt etwas zu sehen war.
    if (entry.clock.consumeWrapped()) entry.wrapped = true;

    // Die Flaeche: ein Rechteck, in das wir selbst zeichnen.
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##tile", ImVec2(size, size));
    testmarke::marke(("kachel:" + entry.name).c_str());
    const bool hovered = ImGui::IsItemHovered();
    // Fuer das naechste Bild merken: nur diese eine Kachel laeuft. Ein Bild
    // Verzoegerung beim Draufzeigen sieht niemand.
    if (hovered) browserHovered_ = &entry;
    const bool opened = ImGui::IsItemHovered() &&
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const theme::Palette& palette = activeTheme(settings_).palette;

    // KEIN gefuellter Kasten mehr.
    //
    // Hier stand ein AddRectFilled mit der Hintergrundfarbe — und eine
    // Zeichenliste arbeitet in der Reihenfolge der Eintraege. Das Bild aller
    // Kacheln wird vorher eingereicht, dieser Kasten lag also **darueber** und
    // hat jede Vorschau zugedeckt. Sichtbar blieb nur, was zufaellig neben dem
    // Kasten herausschaute.
    //
    // Der Hintergrund kommt ohnehin aus dem Renderer: beginViewport loescht
    // das Ziel mit derselben Farbe.
    if (hovered) {
        draw->AddRect(origin, ImVec2(origin.x + size, origin.y + size),
                      ImGui::GetColorU32(toImGui(palette.accent)), 0.0f, 0, 2.0f);
    }

    // Die Kachel selbst ist schon gezeichnet — renderBrowserTiles hat alle
    // sichtbaren in einem Zug in dasselbe Ansichtsziel gelegt. Hier bleibt
    // nur die Bedienung: Rahmen beim Darueberfahren, Beschriftung, Doppelklick.
    //
    // Der Grund fuer die Trennung ist gemessen: den Zielpuffer zu wechseln
    // kostet ein Vielfaches des Zeichnens, und sechzig Kacheln waeren sechzig
    // Wechsel gewesen.

    // Der Name, auf die Kachelbreite gekuerzt.
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + size);
    const size_t cut = entry.name.find_last_of('/');
    ImGui::TextUnformatted(cut == std::string::npos
                               ? entry.name.c_str()
                               : entry.name.c_str() + cut + 1);
    ImGui::PopTextWrapPos();
    if (ImGui::IsItemHovered() || hovered) {
        // Name, Umfang und WOHER. Das Letzte fehlte, und es ist das, was man
        // als Naechstes braucht: wer eine Vorschau seltsam findet, will die
        // Datei ansehen — bei 47 Archiven ist "in welchem?" keine Nebenfrage.
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(entry.name.c_str());
        ImGui::Separator();
        ImGui::TextDisabled(tr(Str::BrowserTipSegments),
                            static_cast<int>(entry.effect.primitives.size()));

        // Nicht noch einmal `palette`: weiter oben in derselben Funktion gibt
        // es schon eine, und MSVC meldet die Verdeckung (C4456).
        const theme::Palette& tipPalette = activeTheme(settings_).palette;
        if (entry.unreadable) {
            ImGui::TextColored(toImGui(tipPalette.error), "%s",
                               tr(Str::BrowserTipUnreadable));
        } else if (entry.effect.primitives.empty()) {
            // "0 Segmente" ohne Begruendung sieht nach einem Fehler des
            // Programms aus. Meist ist die Datei wirklich leer.
            ImGui::TextColored(toImGui(tipPalette.warning), "%s",
                               tr(Str::BrowserTipNoSegments));
        }
        if (entry.parseMessages > 0) {
            ImGui::TextColored(toImGui(tipPalette.warning),
                               tr(Str::BrowserTipProblems), entry.parseMessages);
        }

        if (!entry.sourcePath.empty()) {
            ImGui::TextDisabled(tr(Str::BrowserTipFrom), entry.sourcePath.c_str());
        }
        if (!entry.sourceArchive.empty()) {
            ImGui::TextDisabled(tr(Str::BrowserTipInArchive),
                                entry.sourceArchive.c_str());
        }
        ImGui::EndTooltip();
    }

    ImGui::EndGroup();
    ImGui::PopID();

    if (opened) openBrowserEntry(entry);
}

void App::openBrowserEntry(const BrowserEntry& entry) {
    // In einen neuen Reiter — genau dafuer gibt es sie.
    const auto where = assets::findEffect(assets_, settings_.gamePath, entry.name);
    if (!where.found) return;
    std::string error;
    const auto bytes = assets::readFile(settings_.gamePath, where, &error);
    if (bytes.empty()) return;

    newDocument();
    // Den neuen Reiter auch zeigen (Doppelklick in der Bibliothek).
    showEditor();
    auto result = read(std::string(bytes.begin(), bytes.end()));
    doc().effect = std::move(result.effect);
    doc().parseDiagnostics = std::move(result.diagnostics);
    // Kein Dateipfad: der Effekt kommt aus einem Archiv, und dorthin
    // zurueckschreiben kann man nicht. "Speichern" wird damit zu "Speichern
    // unter", und das ist richtig so.
    doc().filePath.clear();
    doc().dirty = false;
    doc().selectedPrimitive = doc().effect.primitives.empty() ? -1 : 0;
    doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
    doc().undo.reset(doc().effect);
    refreshDiagnostics();
    buildPreviewStopped();
    diag::info("browser: opened " + entry.name);
}

}  // namespace efx::gui
