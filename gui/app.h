// Das Fenstergerüst.
//
// Nachbau der Aufteilung des Originals: zwei Teiler, drei Bereiche. Die
// Registrierungsschlüssel `MainFrame SplitTB` und `MainFrame SplitLR` in
// EffectsEd.exe belegen, dass es genau zwei MFC-Splitter waren.
//
//     +-------------------------------+-----------------+
//     |        3D-Ansicht             |  Eigenschaften  |
//     +===============================+  (volle Hoehe)  |  <- waagerechter Teiler
//     |        Segmentliste           |                 |
//     +-------------------------------+-----------------+
//                                     ^ senkrechter Teiler
//
// Kein Docking. Docking erlaubt es, Bereiche abzureissen und zu schliessen —
// und dann findet man sie nicht wieder.
#pragma once

#include <set>
#include <string>
#include <vector>

#include "efx/effect.h"
#include "efx/io.h"
#include "efx/layout.h"
#include "efx/camera.h"
#include "efx/renderer.h"
#include <functional>
#include <map>
#include <memory>

#include "efx/assets.h"
#include "efx/fields.h"
#include "efx/image.h"
#include "audio_win32.h"
#include "icons.h"

#include "efx/particles.h"
#include "efx/playback.h"
#include "efx/scene.h"
#include "efx/shader.h"
#include "efx/timeline.h"
#include "efx/sound.h"
#include "efx/undo.h"

namespace efx::gui {


// Ein geöffneter Effekt mit allem, was zu ihm gehört.
//
// Vorher lag das alles direkt in `App` — ein Effekt, ein Rückgängig-Verlauf,
// eine Uhr. Für mehrere gleichzeitig geöffnete Dateien muss jede davon ihre
// eigene Auswahl, ihren eigenen Verlauf und ihre eigene Wiedergabe haben.
//
// Der Umbau war mechanisch (dreizehn Felder, 243 Fundstellen), aber die
// Trennung ist die eigentliche Aussage: **was zu einer Datei gehört, steht
// hier; was zum Programm gehört, bleibt in App.** Themen, Spielpfade und der
// Texturspeicher gehören dem Programm — sie zu vervielfachen wäre falsch.
struct Document {
    Effect effect;
    std::string filePath;
    bool dirty = false;
    std::vector<Diagnostic> parseDiagnostics;
    std::vector<Diagnostic> diagnostics;
    int selectedPrimitive = -1;
    particles::System particles;
    unsigned playbackSeed = 1u;

    // Wann die naechste Generation nachgelegt wird — nur bei `repeatDelay`.
    // Siehe die Begruendung in app.cpp: die Engine legt nach, statt neu zu
    // planen.
    float nextRepeatMs = 0.0f;
    std::vector<bool> segmentEnabled;
    timeline::Clock clock;
    bool pendingRestart = false;
    undo::Stack undo;
    bool paused = false;

    // Wie der Reiter beschriftet wird: Dateiname, oder „(unbenannt)".
    std::string title() const;
};

class App {
    // Der Selbsttest (gui/selbsttest.cpp) liest und setzt das Innenleben
    // direkt — er prueft, ob ein Knopf das Datenmodell aendert.
    friend class Selbsttest;

public:
    App();
    ~App();

    // Laedt Einstellungen und Sprache. Vor dem ersten Bild aufzurufen.
    void startup();

    // Ein Bild aufbauen. Laeuft ausschliesslich auf dem Hauptfaden — kein
    // ImGui-Aufruf darf von woanders kommen.
    void buildFrame(render::Renderer* renderer, int windowWidth, int windowHeight,
                    float dpiScale);

    // Schreibt die Einstellungen zurueck.
    void shutdown();

    // Der Nutzer hat Beenden gewaehlt oder das Fenster geschlossen.
    bool wantsQuit() const { return wantsQuit_; }
    // Beenden erbitten: fragt bei ungespeicherten Aenderungen erst nach.
    void requestQuit();

    // Ein Wechsel der Grafikschnittstelle wurde bestaetigt. Das Fenster muss
    // dann neu aufgebaut werden — ImGui erlaubt keinen Tausch im Betrieb.
    bool wantsRendererChange(render::Backend& target) const;
    void clearRendererChange();

    layout::Settings& settings() { return settings_; }

    // Datei oeffnen, auch von aussen (Ziehen auf das Fenster, Aufrufparameter).
    bool openFile(const std::string& path);
    // Eine aufs Fenster gezogene Datei: .efx als Effekt, .pk3 als Archiv.
    void openDroppedFile(const std::string& path);

    // Vom Fensterrahmen gesetzt: oeffnet einen Windows-Dateidialog. Die
    // Oberflaeche kennt keine Win32-Aufrufe, deshalb ueber einen Rueckruf.
    //
    // Gibt einen leeren String zurueck, wenn abgebrochen wurde.
    using FileDialog = std::function<std::string(bool save, const char* filter,
                                                 const char* defaultName)>;
    void setFileDialog(FileDialog dialog) { fileDialog_ = std::move(dialog); }

    // Einen ORDNER wählen, keine Datei.
    //
    // Vorher gab es nur die Dateiauswahl, und für einen Spielpfad musste man
    // darin irgendeine Datei anklicken, damit das Programm den Ordner davon
    // nimmt. Das funktioniert, aber niemand kommt von selbst darauf — und in
    // einem `base`-Ordner, der nur `.pk3`-Dateien enthält, sieht es aus, als
    // solle man ein Archiv auswählen.
    //
    // Gibt einen leeren String zurück, wenn abgebrochen wurde.
    using FolderDialog = std::function<std::string(const char* title,
                                                   const char* startAt)>;
    void setFolderDialog(FolderDialog dialog) { folderDialog_ = std::move(dialog); }

    // Vom Fensterrahmen gesetzt: oeffnet eine Datei mit dem Programm, das
    // Windows dafuer vorgesehen hat — fuer das Handbuch im Browser. Die
    // Oberflaeche kennt keine Win32-Aufrufe, deshalb ueber einen Rueckruf,
    // genau wie beim Dateidialog.
    using ShellOpen = std::function<void(const std::string& path)>;
    void setShellOpen(ShellOpen open) { shellOpen_ = std::move(open); }

    // Ein Bild (RGBA, oben links zuerst) in die Zwischenablage legen. Vom
    // Fensterrahmen gesetzt — die Oberflaeche kennt kein Win32.
    using ClipboardImage =
        std::function<bool(const std::vector<unsigned char>& rgba, int width, int height)>;
    void setClipboardImage(ClipboardImage copy) { clipboardImage_ = std::move(copy); }

    // Was die Erkundung beim Start ergeben hat, und welche Schnittstelle
    // benutzt wird. Der Fensterrahmen weiß das; die Oberfläche zeigt es im
    // Fenster „Grafiktreiber-Information".
    void setGraphicsInfo(std::vector<render::Probe> probes,
                         render::Backend active) {
        probes_ = std::move(probes);
        activeBackend_ = active;
    }

    // Materialbestand. Wird beim Setzen des Spielpfads und auf "Aktualisieren"
    // neu gelesen.
    void rescanAssets();

    // Texturen. Ein Zwischenspeicher je Shadername, damit nicht jedes Bild
    // neu von der Platte kommt — bei dreissig Partikeln waeren das dreissig
    // Ladevorgaenge je Bild.
    //
    // Ein fehlgeschlagener Versuch wird ebenfalls gemerkt (mit kNoTexture),
    // sonst versucht die Vorschau sechzigmal je Sekunde vergeblich, dieselbe
    // fehlende Datei zu oeffnen.
    // Weicher runder Fleck fuer fehlende Bilder. Einmal erzeugt, dann behalten.
    shader::BlendMode blendFor(const std::string& shaderName,
                               const particles::DrawList& list) const;
    render::TextureId fallbackTexture(render::Renderer* renderer);
    render::TextureId fallbackTexture_ = render::kNoTexture;

    // --- Texturen im Hintergrund laden ------------------------------------
    //
    // Suchen, Auspacken und Dekodieren einer Textur kosten beim Anwender rund
    // 20 ms. Beim Oeffnen eines Effekts kommen fuenfzehn bis zwanzig davon —
    // das ist die halbe Sekunde, in der das Fenster steht.
    //
    // Also auf Arbeitsfaeden. Was dort NICHT passieren darf, ist das Anlegen
    // der Textur: das gehoert dem Renderer und damit dem Hauptfaden. Der
    // Arbeitsfaden liefert die fertigen Bildpunkte, der Hauptfaden macht
    // daraus eine Textur — genau die Aufteilung, fuer die `postToMain` da ist.
    struct DecodedTexture {
        std::string name;
        std::string path;
        std::string error;
        bool found = false;
        image::Image picture;
    };
    // Fertig dekodiert, wartet auf den Hauptfaden.
    std::vector<DecodedTexture> readyTextures_;
    // Welche Namen gerade unterwegs sind — sonst wird dieselbe Textur
    // sechzigmal je Sekunde erneut in Auftrag gegeben, bis die erste
    // zurueckkommt.
    std::set<std::string> texturesInFlight_;
    // Bilder, die eine Shaderstufe mit clampMap benutzt: sie werden mit
    // Randklemmung angelegt (sonst blutet die gegenueberliegende Kante ein).
    std::set<std::string> clampImages_;
    void collectDecodedTextures(render::Renderer* renderer);
    // Ein Bild anfordern, ohne es sofort zu brauchen.
    void requestTexture(const std::string& imageName);
    // Alle Bilder eines Effekts anfordern, sobald er geladen ist.
    void prefetchTextures(const Effect& effect);
    // Eine Textur ist eingetroffen — die stehengebliebenen Kacheln zeigen
    // vielleicht noch den Ersatzfleck.
    bool browserNeedsRedraw_ = false;
    // Wartet, bis kein Auftrag mehr laeuft. VOR jeder Aenderung am Bestand
    // aufzurufen: die Arbeitsfaeden lesen ihn.
    void settleTextureJobs();
    // Bilder, die nicht gefunden wurden — zum Nennen in der Oberflaeche.
    std::vector<std::string> missingTextures_;
    // `seconds` ist die Zeit seit dem Ausloesen des Effekts. Sie entscheidet
    // bei einer Bildfolge, welches Bild gilt — bei allem anderen wird sie
    // nicht angesehen.
    render::TextureId textureFor(render::Renderer* renderer,
                                 const std::string& shaderName,
                                 float seconds = 0.0f);
    void clearTextures(render::Renderer* renderer);
    // Ist das Bild dieses Shaders noch unterwegs (Arbeitsfaden)? Solange wird
    // die Gruppe nicht gezeichnet — mit Ersatzbild UND ohne die Mischung, die
    // erst das Bild verraet, erschien sonst ein weisses deckendes Rechteck.
    bool textureStillLoading(const std::string& shaderName, float seconds) const;

    // Was der Kern ueber einen Shader wissen muss (particles::System::build):
    // der ganze Shaderblock, oder dass es weder Block noch Bild gibt. Liest
    // `assets_` nur — darf also auf Arbeitsfaeden laufen (Browser).
    particles::System::ShaderLookup particleShaderLookup() const;
    // Die Zeichengruppen eines Bildes zeichnen, wie die Engine: je Shader-
    // stufe Bild, Faktorpaar, Tiefe und Alphatest (particles::DrawGroup).
    // `renderMode`: 0 texturiert, 1 Drahtgitter, 2 Ueberzeichnung.
    // Gibt die Zahl der Zeichenaufrufe zurueck.
    int drawParticleGroups(render::Renderer* renderer, const particles::DrawList& list,
                           float seconds, int renderMode);

    // Alle Grafikressourcen vergessen, OHNE sie freizugeben.
    //
    // Beim Wechsel der Grafikschnittstelle wird das Fenster neu gebaut, das
    // App-Objekt aber überlebt — mitsamt seiner Texturkennungen, die dann auf
    // zerstörte Ressourcen zeigen. Unter Direct3D ist so eine Kennung ein
    // Zeiger: damit zu zeichnen stürzt ab. Unter OpenGL ist sie eine kleine
    // Zahl, die einfach nichts mehr bedeutet — dort verschwindet der Effekt
    // stillschweigend.
    //
    // Freigeben darf man sie nicht: der Renderer, dem sie gehörten, existiert
    // nicht mehr. Also nur vergessen.
    void forgetGraphicsResources();

private:
    void drawMenuBar();
    void drawToolbar(float dpiScale);
    void drawViewport(render::Renderer* renderer, float width, float height);
    void drawSegmentList(float width, float height);
    void drawProperties(float width, float height);
    void drawTab(fields::Tab tab, Primitive& primitive);
    // Welcher Reiter der Eigenschaftsseite zuletzt gezeichnet wurde.
    fields::Tab propertyTab_ = fields::Tab::Generation;

    // Bausteine fuer die Eigenschaftenblaetter. Alle melden, ob sich etwas
    // geaendert hat, damit der Aufrufer die Datei als geaendert markieren kann.
    bool editRange(const char* id, const char* label, Range& range,
                   float speed = 1.0f, float low = 0.0f, float high = 0.0f);
    bool editVec3Range(const char* id, const char* label, Vec3Range& value,
                       float speed = 1.0f);
    bool editChannel(const char* id, const char* label, Channel& channel,
                     float speed = 1.0f);
    bool editColorChannel(ColorChannel& channel);
    bool editStringList(const char* id, const char* label,
                        std::vector<std::string>& list, const char* hint);
    bool editFlags(Primitive& primitive);

    // Ein Gruppenrahmen wie im Original: Titel oben links, Inhalt eingerueckt,
    // Linie drumherum. Wenn enabled gesetzt ist, sitzt im Titel ein Haekchen,
    // das die Gruppe scharf schaltet — dann sind die Felder darin ausgegraut
    // statt versteckt.
    //
    // Frueher stand vor jedem einzelnen Feld ein Ankreuzfeld. Das war
    // ehrlicher gegenueber dem Dateiformat (ein Feld ohne Haekchen wird nicht
    // geschrieben), aber unbedienbar: zwoelf Haekchen auf einer Seite, und
    // keines sagt, wozu es gehoert.
    bool beginGroup(const char* id, const char* label, bool* enabled = nullptr);
    void endGroup();

    // Welche Gruppen gerade ausgegraut sind. Selbst gefuehrt statt aus
    // ImGuis Innenleben gelesen: ImGuiContext::CurrentItemFlags gehoert zur
    // internen Schnittstelle und kann sich zwischen Fassungen aendern.
    std::vector<bool> groupDisabledStack_;
    void drawStatusBar();
    // Hilfezeile: ueber welchem Menuepunkt/Knopf die Maus gerade steht. Wie
    // im Original steht die Beschreibung unten links statt "Ready".
    const char* statusHint_ = nullptr;
    void hint(i18n::Str text) {
        if (ImGui::IsItemHovered()) statusHint_ = i18n::tr(text);
    }
    void drawDialogs();
    void handleShortcuts();
    void drawGamePathDialog();
    void drawPlaybackDialog();
    void drawNewSegmentDialog();
    void drawSpawnOriginDialog();
    void drawColourDialogs();
    void drawWindDialog();
    void drawSunDialog();
    void drawDriverInfoDialog();
    void takeScreenshot(render::Renderer* renderer, bool toClipboard);
    // Auswahlfenster fuer Shader, Modelle, Klaenge und Effekte. Ein Fenster
    // fuer alle vier: sie unterscheiden sich nur in der Quelle der Liste.
    void drawPickerDialog();

    // Ein ziehbarer Teiler. Gibt true zurueck, wenn er bewegt wurde.
    bool splitter(const char* id, bool vertical, float thickness, float* fraction,
                  float totalPx, float minFirstPx, float minSecondPx);

    bool saveFile(const std::string& path);

    // --- Befehle (gui/app_commands.cpp) -----------------------------------
    // Menue, Werkzeugleiste und Tastenkuerzel rufen nur diese.
    bool hasSelection() const;
    bool documentInUse(const Document& document) const;
    void showEditor();
    void cmdNew();
    void cmdOpen();
    void cmdOpenPk3();
    bool cmdSave();
    bool cmdSaveAs();
    void cmdAddSegment(PrimitiveType type);
    bool canCloneSegment() const;
    void cmdCloneSegment();
    void cmdDeleteSegment();
    void cmdToggleSegmentEnabled();
    // Vor dem Verwerfen ungespeicherter Arbeit fragen.
    void requestCloseDocument(int index);
    void drawSaveChangesDialog();
    // Eine Ja/Nein-Frage. `yes` laeuft nur bei Ja.
    void askConfirm(std::string text, std::function<void()> yes);
    void drawConfirmDialog();
    bool showConfirm_ = false;
    std::string confirmText_;
    std::function<void()> confirmYes_;
    bool showSaveChangesDialog_ = false;
    int pendingClose_ = -1;
    bool pendingQuit_ = false;
    // Eine Feldaenderung auf der Eigenschaftsseite ist noch nicht im
    // Rueckgaengig-Verlauf (das geschieht, wenn das Feld losgelassen wird).
    bool fieldEditOpen_ = false;
    // Die Vorschau muss neu aufgebaut werden (einmal je Bild, nicht je Feld).
    bool previewDirty_ = false;
    void refreshDiagnostics();
public:
    // Der Fensterrahmen setzt ihn bei jeder Aenderung (vorher wurde er
    // berechnet, aber nie gesetzt - im Titel stand immer nur "EffectsEd").
    std::string windowTitle() const;

private:

    // Die geöffneten Dateien. Immer mindestens eine — ein Fenster ohne
    // Dokument gäbe es sonst nur beim Schließen des letzten Reiters, und für
    // diesen einen Fall überall auf Leerheit zu prüfen wäre der schlechtere
    // Tausch.
    std::vector<Document> documents_;
    int activeDocument_ = 0;

    Document& doc();
    const Document& doc() const;

    void newDocument();
    // Die Uhr-Endart, die zur eingestellten Wiederholart gehoert.
    timeline::EndMode playbackEndMode() const;
    void closeDocument(int index);
    void activateDocument(int index);
    void drawDocumentTabs();

    // --- Effektbrowser ----------------------------------------------------
    //
    // Eine Kachel je Effekt, alle laufend. Der Zweck ist Suchen, nicht
    // Bearbeiten: wer „den mit den blauen Funken" sucht, kennt den Dateinamen
    // nicht — aber er erkennt ihn, sobald er ihn sieht.
    struct BrowserEntry {
        std::string name;
        Effect effect;
        bool loaded = false;
        // Wie weit der Effekt reicht — einmal beim ersten Bild bestimmt und
        // dann behalten. Bei jedem Bild neu gerechnet zappelte die Groesse mit
        // jedem Teilchen, das auftaucht.
        float reach = 0.0f;
        // Der Zeitpunkt, an dem am meisten zu sehen ist — das Standbild.
        float bestTimeMs = 0.0f;
        // Woher die Datei kommt — fuer den Kurzhinweis. Wer eine Vorschau
        // seltsam findet, will als Erstes die Datei ansehen, und bei 47
        // Archiven ist die Frage "in welchem?" nicht nebensaechlich.
        std::string sourcePath;
        std::string sourceArchive;
        // Schon einmal gezeichnet? Trennt "muss neu gezeichnet werden" von
        // "war noch nie dran". Ohne diese Trennung sprang eine laufende
        // Vorschau auf den Anfang zurueck, sobald irgendeine Textur eintraf.
        bool everDrawn = false;
        // Die Datei ist gelesen und zerlegt, der Rest fehlt noch. Zwischen
        // Auftrag und Fertigstellung liegen ein paar Bilder — die Kachel
        // bleibt so lange leer, statt das Fenster anzuhalten.
        bool parsed = false;
        bool unreadable = false;
        int parseMessages = 0;
        particles::System system;
        timeline::Clock clock;
        bool wrapped = false;
    };
    std::vector<BrowserEntry> browserEntries_;
    char browserFilter_[128] = {};
    // Auf eine Quelle beschraenken — leer heisst alle.
    //
    // Wer Basisspiel, Mod und eigenen Arbeitsordner zugleich angibt, sieht
    // sonst siebenhundert Effekte in einem Topf und kann nicht sagen, welche
    // davon seine sind.
    std::string browserSource_;
    // Wie viele Spielordner die letzte Unterordnersuche ergeben hat.
    int scanMessage_ = 0;
    float browserTileSize_ = 150.0f;
    int browserVisible_ = 0;
    // Der Startreiter.
    //
    // Der Browser gehört nicht in ein Extrafenster, sondern ist die Seite, auf
    // der man landet, bevor irgendetwas offen ist — wie die Startseite eines
    // Browsers. Er ist immer der erste Reiter und lässt sich nicht schließen;
    // so gibt es immer einen Weg zurück.
    // Ob die Bibliothek gerade die Ansicht fuellt. Der Anfangswert kommt aus
    // den Einstellungen — siehe Settings::openLibraryOnStart.
    bool startTabActive_ = false;
    // Einmalig: „bitte den Startreiter auswählen".
    //
    // Getrennt von `startTabActive_`, weil ImGuis `SetSelected` bei JEDEM Bild
    // erneut auswählt, solange man das Kennzeichen mitgibt. Wer es an den
    // Zustand koppelt, hält den Reiter für immer fest — man kann ihn anklicken
    // und landet sofort wieder dort.
    bool wantStartTab_ = true;
    // Einmalig: den Reiter des aktiven Dokuments auswaehlen (showEditor).
    bool wantDocumentTab_ = false;
    std::vector<std::string> archiveSources_;   // per „Archiv öffnen" geladen

    void drawBrowser(render::Renderer* renderer, float dpiScale);
    // Effekt laden und abspielen, falls noch nicht geschehen. Muss vor dem
    // Zeichnen laufen — der Renderer braucht ein System, das schon lebt.
    // Fordert das Lesen und Zerlegen der .efx an — auf einem Arbeitsfaden.
    void requestBrowserEntry(BrowserEntry& entry);
    // Nimmt das Ergebnis entgegen und macht daraus ein laufendes System.
    void ensureBrowserEntry(BrowserEntry& entry);
    // Gelesene und zerlegte Effekte, warten auf den Hauptfaden.
    struct LoadedEffect {
        std::string name;
        std::string path;
        std::string archive;
        Effect effect;
        int parseMessages = 0;
        bool unreadable = true;
    };
    std::vector<LoadedEffect> readyEffects_;
    std::set<std::string> effectsInFlight_;
    void collectLoadedEffects();
    void drawBrowserTile(BrowserEntry& entry, float size);
    // Alle sichtbaren Kacheln in EIN Ansichtsziel zeichnen.
    void renderBrowserTiles(render::Renderer* renderer, float dpiScale);
    // Was in diesem Bild sichtbar ist — Rechteck in Bildpunkten des Ziels.
    struct TileSlot {
        BrowserEntry* entry = nullptr;
        // Wo die Kachel auf dem Bildschirm liegt, im Raster des Fensters.
        float gridX = 0.0f, gridY = 0.0f, gridSize = 0.0f;
        // Ihr fester Platz im Blatt, ueber Bilder hinweg derselbe.
        int cell = -1;
        // Neu aufgetaucht: muss in diesem Bild gezeichnet werden, sonst waere
        // ihr Platz noch der Inhalt des Vorgaengers.
        bool fresh = false;
        // Wo ihr Bild im Blatt liegt. Das Blatt ist DICHT gepackt und in
        // Vorschauaufloesung — nicht in Anzeigegroesse. Deshalb sind das zwei
        // verschiedene Rechtecke und nicht dasselbe.
        int x = 0, y = 0, size = 0;
    };
    std::vector<TileSlot> browserSlots_;
    int browserPreviewSize_ = 0;
    // Die Aufteilung des Blattes. Aendert sie sich, liegen alle Plaetze
    // woanders — dann muss jede Kachel neu gezeichnet werden.
    // Hat die Bearbeitungsansicht das gemeinsame Ansichtsziel benutzt? Dann
    // ist das Vorschaublatt darin ueberschrieben und muss neu gezeichnet
    // werden.
    bool viewportUsedByEditor_ = false;
    int browserSheetColumns_ = 0;
    int browserSheetWidth_ = 0;
    int browserSheetHeight_ = 0;

    // Welcher Effekt sitzt auf welchem Platz im Blatt.
    //
    // Der Platz muss ueber Bilder hinweg DERSELBE bleiben, sonst kann eine
    // Kachel nicht stehenbleiben. Beim Rollen wandern die Kacheln im Raster,
    // aber ihr Platz im Blatt bleibt — nur wer neu auftaucht, bekommt einen
    // freien.
    // Platz -> Kennung des Effekts (sein Index im Bestand), -1 heisst frei.
    // Die Vergabe macht efx::tiles::assign.
    std::vector<int> browserCellOwner_;

    // Zur Anzeige: wie viele Kacheln im letzten Bild neu gezeichnet wurden.
    int browserRedrawn_ = 0;
    // Wo der Mauszeiger im letzten Bild stand. Nur diese eine Kachel laeuft.
    BrowserEntry* browserHovered_ = nullptr;
    // Alles laufen lassen — auf Wunsch, nicht als Vorgabe.
    bool browserAnimateAll_ = false;
    // Diagnose: Platznummer und Lage im Blatt in jede Kachel schreiben.
    //
    // Gebaut, weil ich dreimal geraten habe, wo eine Fehlzuordnung herkommt.
    // Mit den Nummern im Bild ist ein einziges Bildschirmfoto die Antwort:
    // stimmt die Nummer, liegt es an der Umrechnung; stimmt sie nicht, an der
    // Vergabe.
    bool browserShowCells_ = false;
    // Welche Kacheln in diesem Bild neu gezeichnet werden.
    std::vector<size_t> browserDue_;
    // Was ein Bild des Browsers wirklich kostet — fuer das Protokollfenster.
    // Ohne Zahlen ist "die Bildrate ist schlecht" nicht zu beheben, sondern
    // nur zu beklagen.
    int browserDrawCalls_ = 0;
    double browserBuildMs_ = 0.0;
    double browserDrawMs_ = 0.0;
    // Was die Kachel unter dem Zeiger kostet. Nur diese eine laeuft, also ist
    // sie es auch, die die Bildrate bestimmt — und "manche sind langsam" ist
    // ohne Zahlen nicht zu beheben.
    std::string browserHoveredName_;
    int browserHoveredLive_ = 0;
    int browserHoveredDrawn_ = 0;
    int browserHoveredVertices_ = 0;
    // showLogWindow_ steht schon weiter oben — das Fenster gab es bereits.
    // Das Meldungsfenster: was die 40 Pruefregeln zu sagen haben.
    //
    // Die Zahl stand bisher in der Statuszeile, und dabei blieb es — es gab
    // keine Stelle, an der man die Meldungen LESEN konnte. "1 Error" ohne
    // Text ist keine Diagnose, sondern eine Beunruhigung.
    bool showMessagesWindow_ = false;
    bool logFollow_ = true;
    bool logWarningsOnly_ = false;
    void openBrowserEntry(const BrowserEntry& entry);
    void refreshBrowser();
    void drawStartPage(render::Renderer* renderer, float dpiScale);
    bool openArchive(const std::string& path);

    layout::Settings settings_;

    // Getrennt gehalten: die Meldungen beim Lesen der Datei gelten, bis sie
    // neu gelesen wird; die der Pruefung gelten bis zur naechsten Aenderung.
    // Beides in einer Liste zu fuehren hiess, dass sie bei jeder Feldaenderung
    // laenger wurde — nach hundert Aenderungen stand dieselbe Warnung
    // hundertmal da.

    bool wantsQuit_ = false;
    bool rendererChangePending_ = false;
    render::Backend rendererTarget_ = render::Backend::Direct3D11;

    bool showRendererDialog_ = false;
    bool showGamePathDialog_ = false;
    char gamePathBuffer_[512] = {};
    bool showPlaybackDialog_ = false;
    bool showNewSegmentDialog_ = false;
    bool showSpawnOriginDialog_ = false;
    bool showWallColourDialog_ = false;
    bool showWindDialog_ = false;
    bool showSunDialog_ = false;
    bool showDriverInfoDialog_ = false;
    std::vector<render::Probe> probes_;
    render::Backend activeBackend_ = render::Backend::Direct3D11;
    int pendingScreenshot_ = 0;
    std::string screenshotMessage_;
    float screenshotMessageUntil_ = 0.0f;
    render::TextureId wallTexture_ = render::kNoTexture;
    int wallTextureKind_ = -1;

    // Auswahlfenster. target zeigt auf die Liste, die ergaenzt werden soll.
    enum class PickerKind { Shaders, Models, Sounds, Effects };
    bool showPicker_ = false;
    PickerKind pickerKind_ = PickerKind::Shaders;
    std::vector<std::string>* pickerTarget_ = nullptr;
    char pickerFilter_[128] = {};
    int pickerSelected_ = -1;
    std::set<int> pickerChosen_;   // Mehrfachauswahl
    int pickerAnchor_ = -1;        // fuer Umschalt-Klick
    bool pickerPreview_ = false;
    // Der Renderer dieses Bildes (fuer Vorschaubilder in Dialogen).
    render::Renderer* renderer_ = nullptr;
    bool showBackgroundColourDialog_ = false;

    FileDialog fileDialog_;
    FolderDialog folderDialog_;
    assets::Index assets_;
    std::map<std::string, render::TextureId> textureCache_;
    int texturesLoaded_ = 0;
    int texturesMissing_ = 0;
    bool textureCacheDirty_ = false;

    bool assetsScanned_ = false;

    playback::Settings playback_;

    // Die laufende Vorschau.
    int lastDrawn_ = 0;
    int lastAlive_ = 0;
    int lastScheduled_ = 0;
    int lastMarks_ = 0;
    // Wie lange Aufbau und Zeichnen der Teilchen im letzten Bild brauchten.
    float lastBuildMs_ = 0.0f;
    float lastDrawMs_ = 0.0f;

    // Spielordner aus der Windows-Registrierung. Leer auf anderen Systemen.
    std::vector<std::pair<std::string, std::string>> registryGamePaths() const;
    // Einen einzelnen Klang sofort abspielen — fuer den Knopf im
    // Klang-Segment.
    // Einen vollen Dateipfad auf den spielinternen Namen kuerzen
    // ("sound/..."). Leer, wenn er ausserhalb aller Spielpfade liegt.
    std::string toGameRelative(const std::string& path) const;
    // Loest die Kameraerschuetterung aus, wenn ein CameraShake-Segment an
    // der Reihe ist.
    void triggerCameraShakes(float nowMs);
    // Schreibt das eingebettete Handbuch neben die Einstellungen und
    // oeffnet es im Standardbrowser.
    void openUsersGuide();
    void playSoundNow(const std::string& name);
    // Ein Segment an eine andere Stelle schieben. Gibt false zurueck, wenn
    // sich nichts geaendert hat.
    bool moveSegment(int from, int to);
    // Nach einer Spalte der Segmentliste umsortieren (Klick auf den Kopf).
    void sortSegments(int column, bool ascending);
    // Anzeigename eines Segments: sein Name, sonst "Unnamed <Typ> <n>".
    std::string displayName(int index) const;
    // Umbenennen in der Segmentliste (Doppelklick, F2, Kontextmenue).
    void startRename(int index);
    int renamingSegment_ = -1;
    bool renameFocus_ = false;
    char renameBuffer_[64] = {};
    // Ein neues Segment an dieser Stelle einfuegen.
    bool insertSegmentAt(int at);
    // Baut die Vorschau neu und behaelt Zeit und Zustand der Uhr.
    // Holt die Windfahne in den Raum zurueck — auch nach einem Wechsel des
    // Masstabs, nicht nur beim Ziehen.
    void clampWindFlagToRoom();
    void refreshPreview();
    void buildPreviewStopped();

    ShellOpen shellOpen_;
    ClipboardImage clipboardImage_;
    // Entwuerfe der Dialoge: erst Ok uebernimmt sie, Abbrechen verwirft.
    playback::Settings playbackDraft_;
    playback::Origin spawnOriginDraft_;
    float colourBackup_[3] = {};
    std::vector<std::string> extraPathsBackup_;
    bool colourBackupOverridden_ = false;
    void startPlayback();
    // Ungleich 0: jeder Start benutzt diesen Ausgangswert (Selbsttest).
    unsigned fixedSeed_ = 0;
    // Die Zeitleiste zwischen Ansicht und Segmentliste.
    void drawTimeline(float dpiScale);


    // Die Windfahne laesst sich mit der Maus verschieben.
    void updateFlagDrag(float width, float height);
    bool draggingFlag_ = false;
    bool zeroToOneDepth_ = false;
    bool flagHovered_ = false;
    std::vector<sim::Plane> collisionPlanes() const;
    void pressPlay();
    // Wiederholung beenden, ohne abzubrechen: der laufende Durchlauf lebt aus
    // (Play ein zweites Mal, wie im Original).
    bool playOut_ = false;
    timeline::EndMode endModeBeforePlayOut_ = timeline::EndMode::Repeat;
    void finishPlayOut();
    // Die Wiederholrate der Werkzeugleiste. Sie IST das `repeatDelay` der
    // Datei (das Original schreibt den Wert der Leiste hinein); ohne Eintrag
    // in der Datei gilt die zuletzt eingestellte Rate.
    float repeatRateSeconds() const;
    void setRepeatRateSeconds(float seconds);
    void pressStop();
    void togglePause();

    // Merkt den Stand nach einer Aenderung. An jeder Stelle zu rufen, die
    // effect_ veraendert — sonst fehlt genau dieser Schritt im Verlauf.
    void recordChange(const char* what);
    void applyUndo();
    void applyRedo();


    // Untergeordnete Effekte, die Emitter und FxRunner starten.
    //
    // Zwischengespeichert, weil ein Emitter denselben Namen zwanzigmal
    // fragt — und weil die Zeiger, die der Lader zurueckgibt, das ganze
    // Abspielen ueberleben muessen. Eine lokale Variable waere dafuer der
    // falsche Ort.
    std::map<std::string, std::unique_ptr<Effect>> childEffects_;
    const Effect* loadChildEffect(const std::string& name);

    // Klaenge. Wie die Texturen zwischengespeichert — und wie dort wird auch
    // das Scheitern gemerkt, sonst wird bei jedem Durchlauf wieder vergeblich
    // gesucht.
    std::map<std::string, sound::Sound> soundCache_;
    const sound::Sound* loadSound(const std::string& name);
    void triggerSounds(float nowMs);
    // Damit die Meldung "Ton ist aus" einmal kommt und nicht sechzigmal
    // je Sekunde.
    bool soundsOffReported_ = false;
    Audio audio_;
    playback::Origin spawnOrigin_;

    // Fortlaufendes Nachlegen, wie es die Wiedergabe-Einstellungen des
    // Originals verlangen ("Repeat for N seconds", "Respawn effect every
    // frame", "Animate effect spawn location"): der Effekt wird vorwaerts in
    // der Zeit immer wieder ausgeloest, jedes Mal zusaetzlich.
    bool usesSpawnSchedule() const;
    void advanceSpawnSchedule(float nowMs);
    float spawnScheduleDurationMs() const;
    bool scheduleActive_ = false;
    float nextSpawnMs_ = 0.0f;
    float spawnIntervalMs_ = 0.0f;
    float spawnLimitMs_ = -1.0f;   // negativ: bis zum Anhalten
    float lastSpawnMs_ = 0.0f;
    float singleDurationMs_ = 0.0f;
    unsigned spawnCount_ = 0;
    camera::Vec3 scheduleBase_;
    int newSegmentType_ = 0;

    // Eigene Farben fuer die Ansicht. Leer heisst: die des Themas.
    bool wallColourOverridden_ = false;
    bool backgroundColourOverridden_ = false;
    float wallColour_[3] = {0.4f, 0.7f, 0.42f};
    float backgroundColour_[3] = {0.08f, 0.08f, 0.09f};
    bool showAboutDialog_ = false;
    bool showLogWindow_ = false;

    // Zustand der Wiedergabe. Die eigentliche Vorschau kommt spaeter; die
    // Bedienelemente stehen aber schon, weil sie zum Aufbau des Fensters
    // gehoeren.
    // Ob abgespielt wird, sagt die Uhr — nicht ein eigenes Merkzeichen.
    //
    // Vorher gab es beides, und sie liefen auseinander: die Bedingung, ob
    // ueberhaupt gezeichnet wird, haengte an `playing_`, gesetzt wurde es
    // aber nur INNERHALB dieser Bedingung. Nach dem Ende eines Durchlaufs kam
    // man daraus nicht mehr heraus — man musste Play zweimal druecken.
    //
    // Zwei Wahrheiten fuer dieselbe Sache sind fast immer ein Fehler; hier
    // war es eine Verklemmung.
    bool playing() const;
    float playbackTime_ = 0.0f;

    // Vorschau
    camera::Orbit camera_;
    camera::Shake shake_;
    scene::RoomSize roomSize_;
    float lastWorldScale_ = 0.0f;
    bool geometryDirty_ = true;
    scene::Mesh roomMesh_;
    scene::LineSet gridLines_;
    scene::LineSet axisLines_;
    scene::LineSet windLines_;
    scene::Mesh skyMesh_;
    scene::Mesh sunMesh_;
    // Umgefaerbte Eckpunkte fuer die Overdraw-Ansicht. Als Member, damit der
    // Zeiger den Zeichenaufruf ueberlebt und nicht je Bild neu belegt wird.
    std::vector<scene::Vertex> overdrawScratch_;

    void rebuildGeometry();
    void handleViewportInput(float width, float height);
};

}  // namespace efx::gui
