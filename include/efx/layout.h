// Fensteraufteilung.
//
// Der alte Editor benutzt zwei MFC-Splitter. Das ist nicht geraten: in
// EffectsEd.exe stehen die Registrierungsschluessel im Klartext —
//
//     MainFrame SplitTB       (oben/unten)
//     MainFrame SplitLR       (links/rechts)
//     MainFrame Left/Top/Right/Bottom, MinX/MinY/MaxX/MaxY, Flags, Show
//
// Also genau zwei Teiler und eine gespeicherte Fensterlage. Diese Aufteilung
// bauen wir nach, weil sie funktioniert und weil sich sonst niemand
// zurechtfindet, der den Editor kennt:
//
//     +-------------------------------+-----------------+
//     |                               |                 |
//     |        3D-Ansicht             |  Eigenschaften  |
//     |                               |                 |
//     +===============================+  (volle Hoehe)  |   <- SplitTB
//     |        Segmentliste           |                 |
//     +-------------------------------+-----------------+
//                                     ^ SplitLR
//
// Bewusst *kein* ImGui-Docking. Docking erlaubt es, Bereiche abzureissen,
// zu stapeln und zu schliessen — und dann findet man sie nicht wieder. Ein
// fester Aufbau mit zwei ziehbaren Teilern ist genau das, was das Original
// hatte und was hier gebraucht wird.
#pragma once

#include <string>

#include "efx/i18n.h"

namespace efx::layout {

// Die Teiler werden als Anteil gespeichert, nicht in Bildpunkten. Sonst
// rutscht die Aufteilung bei jedem Monitorwechsel oder jeder anderen
// Skalierung, und wer zwischen Laptop und Bildschirm wechselt, richtet sie
// jedes Mal neu ein.
struct Split {
    float propertiesFraction = 0.28f;  // Breite der Eigenschaften, Anteil
    float listFraction = 0.24f;        // Hoehe der Segmentliste, Anteil

    // Untergrenzen in Bildpunkten bei 100 Prozent Skalierung. Ein Bereich
    // darf nicht so klein gezogen werden, dass man ihn nicht mehr fassen kann.
    static constexpr float kMinPanelPx = 180.0f;
    // Die Eigenschaftsseite braucht mehr: Beschriftung, zwei Zahlenfelder
    // und das Spannen-Haekchen nebeneinander. Das Original hat 334 Punkte
    // bei 100 Prozent. Mit 180 lief bei 150 Prozent das Haekchen rechts aus
    // dem Bild (Selbsttest, 1.10.2026).
    static constexpr float kMinPropertiesPx = 330.0f;
    static constexpr float kMinViewPx = 200.0f;
    static constexpr float kSplitterPx = 5.0f;

    // Begrenzt beide Anteile auf das, was bei dieser Fenstergroesse und
    // Skalierung noch bedienbar ist.
    void clampTo(float windowWidthPx, float windowHeightPx, float dpiScale);
};

struct WindowPlacement {
    int x = 0, y = 0, width = 1280, height = 860;
    bool maximized = false;
    bool valid = false;
};

struct Settings {
    Split split;
    WindowPlacement window;
    std::string themeId = "dark";
    std::string languageCode;   // leer = aus den Systemeinstellungen ableiten
    std::string rendererCode = "d3d11";

    // Anzeigeschalter, gleiche Bedeutung wie im Ansichtsmenue des Originals.
    bool drawAxes = true;
    // "Draw Room" im Original schaltet nur die WAENDE, nicht den Boden —
    // sein Hilfetext lautet "Draw the walls of the testing room". Der Boden
    // bleibt immer stehen, sonst schwebt alles.
    bool drawRoom = true;   // die Waende

    // Der Umriss des Testraums — das Gitter auf dem Boden.
    //
    // Im Original ist das EIN Befehl (32870) mit zwei Namen: das Menue nennt
    // ihn "Draw Wireframe", die Werkzeugleiste "Draw Grid". Sein Hilfetext
    // sagt, was er wirklich tut: "Draw outline of testing room".
    //
    // Bei mir waren es zwei getrennte Einstellungen — zwei Schalter fuer
    // dieselbe Sache, und keiner tat, was sein Name versprach.
    //
    // Seit dem Abgleich mit dem Original (1.10.2026, Handbuch und Messung):
    // der Schalter zeichnet den Raum ALS Drahtgitter statt als Flaechen, auf
    // allen sechs Seiten, 20 Einheiten je Feld. Darum aus, wie im Original.
    bool drawGrid = false;
    // Zeichenreihenfolge wie das alte EffectsEd (spaeteres Segment oben) statt
    // wie das Spiel (GL_ONE GL_ONE immer zuletzt, SS_BLEND1). Voreinstellung
    // an — Wunsch des Anwenders, 07.10.2026: es soll aussehen wie im alten.
    bool legacyDrawOrder = true;
    bool drawWindVector = false;
    bool playSounds = true;
    bool showStatusBar = true;

    // Die vier Werkzeugleisten des Originals.
    //
    // Dort waren es vier andockbare Fenster (MFC), bei uns eine feste Leiste
    // in zwei Zeilen. Die Gruppen entsprechen aber genau den vier Leisten
    // 128, 155, 158 und 167 aus dem Binary, und einzeln abschalten lassen
    // sie sich damit auch — ohne dass man Fenster herumziehen muss.
    bool showMainToolbar = true;      // Neu, Öffnen, Speichern, Klonen
    bool showEffectsToolbar = true;   // Neues Segment, Segment löschen
    bool showPlaybackToolbar = true;  // Abspielen, Pause, Stop, Ausrichtung
    bool showWorldToolbar = true;     // Achsen, Raum, Gitter, Darstellung
    bool resetRepeatRateOnStart = true;  // wie im Original voreingestellt
    // Beim Start im Hintergrund bei GitHub nach einer neueren Fassung
    // fragen (gui/update_win32.cpp). Gefragt wird nur; geladen und
    // installiert wird erst auf Knopfdruck.
    bool checkUpdates = true;

    // 0 ohne Textur, 1 Ziegel, 2 Erde, 3 Putz.
    //
    // Putz ist die Voreinstellung, wie im Original: in EffectsEd.exe stehen
    // die Dateinamen ab 603732 in der Folge `stucco.jpg`, `dirt.jpg`,
    // `brick.jpg`, `none.jpg` — Putz an erster Stelle. Ein Vergleichsvideo
    // des Originals zeigt dieselben rauen grauen Wände.
    //
    // Vorher stand hier 0, also gar keine Textur. Der Raum bekam damit die
    // Wandfarbe des Themas, und bei einem dunklen Thema war das ein grünlicher
    // Kasten — im Vergleich mit Ravens Fassung fiel das sofort auf.
    int roomTexture = 3;

    // Mit welcher Ansicht startet das Programm?
    //
    // Das Original oeffnet mit einem leeren Dokument und dem Testraum. Wir
    // haben mit der Effektbibliothek geoeffnet, weil sie das Auffaelligste
    // war — und damit den ersten Eindruck verschoben: wer den Editor kennt,
    // sucht zuerst seinen Arbeitsbereich.
    //
    // Voreinstellung ist deshalb der Editor. Wer die Bibliothek lieber gleich
    // sieht, stellt es hier um; sie bleibt ueber Effekte > Bibliothek und
    // Strg+B immer einen Griff entfernt.
    bool openLibraryOnStart = false;

    // Zeilenhöhe der Segmentliste in Punkten. 0 heißt: an den Inhalt
    // anpassen, so wie ImGui es von sich aus tut.
    //
    // Wofür: bei einem Effekt mit zwanzig Segmenten will man mehr auf einmal
    // sehen, bei einem mit dreien lieber größere Zeilen. Das Rollen bleibt in
    // beiden Fällen möglich — die Höhe ändert nur, wie viel hineinpasst.
    float segmentRowHeight = 0.0f;

    // Beim Wiederholen mit dem Bestand beginnen, den eine Schleife im Spiel
    // schon aufgebaut haette (Vorlauf, CFxScheduler::AddLoopedEffects) —
    // statt, wie das Original, bei der ersten Ausloesung. Nur fuer Effekte
    // mit repeatDelay. Voreinstellung aus: das Original baut auf (gemessen,
    // Active 16 -> 34 -> 44 -> ... -> 90), und ein repeatDelay heisst nicht,
    // dass der Effekt im Spiel schleift — der alte Editor schrieb ihn in
    // jede Datei (Bildvergleich 2.10.2026).
    //
    // Ersetzt "legacyRepeat" (auslaufen, Pause, von vorn): das widersprach
    // der Messung am Original und ist entfallen.
    bool preRoll = false;

    // Gitter auch auf Wänden und Decke, nicht nur auf dem Boden.
    //
    // Der Boden allein sagt wenig über die Höhe eines Effekts: eine Flamme,
    // die bis zur Decke reicht, sieht aus wie eine halb so hohe, solange
    // nichts danebensteht, woran man messen kann.
    bool gridOnWalls = false;
    // Wie der Effekt gezeichnet wird. Im Original drei Schalter: Textured
    // und Wireframe unabhaengig (beide an = Drahtgitter ueber den Flaechen),
    // Overdraw sperrt beide. Vorher eine Auswahl aus dreien — beides
    // zugleich ging nicht.
    bool effectTextured = true;
    bool effectWireframe = false;
    bool effectOverdraw = false;
    // Abgeleitet fuer den Zeichenweg: 2 Ueberzeichnung, 1 nur Drahtgitter,
    // 0 texturiert (Drahtgitter darueber, wenn effectWireframe).
    int effectRenderMode = 0;
    void updateRenderMode() {
        effectRenderMode = effectOverdraw ? 2 : (effectWireframe && !effectTextured) ? 1 : 0;
    }
    // 0 nach oben, 1 seitwaerts, 2 nach unten. Das Original hat drei, nicht zwei.
    int orientation = 0;

    // Raumart: 0 geschlossen, 1 draussen mit Himmel, 2 gar nichts.
    //
    // Geschlossen ist die Voreinstellung, wie im Original — der Kasten gibt
    // sofort ein Gefühl für Maßstab, weil Wände und Decke in bekanntem
    // Abstand stehen.
    int roomStyle = 0;

    // Zuletzt geöffnete Dateien, neueste zuerst.
    std::vector<std::string> recentFiles;
    static constexpr size_t kMaxRecentFiles = 8;

    // Trägt einen Pfad ein. War er schon dabei, wandert er nach vorn statt
    // ein zweites Mal zu erscheinen — sonst steht dieselbe Datei achtmal da,
    // sobald jemand daran arbeitet.
    void addRecentFile(const std::string& path);

    // Die Sonne. Richtung zeigt **zur** Sonne, nicht in Lichtrichtung.
    bool sunEnabled = true;
    float sunDirection[3] = {0.4f, -0.3f, 0.85f};
    float sunAmbient = 0.35f;   // Grundhelligkeit abgewandter Flächen
    bool drawSunDisc = true;

    // Wo die Windfahne steht, in Weltkoordinaten auf dem Boden.
    //
    // Verschiebbar, weil der richtige Platz vom Effekt abhängt: bei einem
    // kleinen Funken will man sie nah haben, bei einer Explosion weiter weg.
    // Ein fester Ort ist für das eine zu weit und für das andere zu nah.
    float windFlagPos[2] = {-32.0f, -32.0f};

    // Der Windpfeil.
    //
    // Reine Anzeigehilfe: das Spiel wertet Wind bei Effekten nirgends aus
    // (der Block dafuer ist in beiden Zweigen tot). Der alte Editor zeigt den
    // Pfeil trotzdem, und er ist nuetzlich, um sich eine Richtung
    // vorzustellen — nur eben nichts, was in der Datei landet.
    float windDirection[3] = {1.0f, 0.0f, 0.0f};
    float windSpeed = 1.0f;

    // Grundpfad zum Spiel. Von hier aus werden Shader, Texturen, Klaenge und
    // Effektdateien gesucht.
    // Der aktive Spielpfad.
    std::string gamePath;

    // Weitere Pfade, die gleichzeitig durchsucht werden.
    //
    // Ein einzelner Pfad reicht nicht: wer an einem Mod arbeitet, hat das
    // Grundspiel an einer Stelle, den Mod an einer zweiten und vielleicht eine
    // Sammlung eigener Dateien an einer dritten. Ein Effekt aus dem Mod
    // verweist auf einen Shader aus dem Grundspiel — mit nur einem Pfad
    // findet man den nie.
    //
    // Die Reihenfolge entscheidet: **der erste Treffer gewinnt.** So kann ein
    // Mod eine Datei des Grundspiels überschreiben, genau wie die Engine es
    // mit ihren .pk3-Dateien macht.
    std::vector<std::string> extraGamePaths;

    // Alle Pfade in der Suchreihenfolge — der aktive zuerst.
    std::vector<std::string> allGamePaths() const;

    // Werkzeugleiste unten
    // Einheiten je Fuß. Voreinstellung 10 — „WARS", also Star Wars.
    //
    // Hier stand 16 mit dem Vermerk „Voreinstellung wie JKA". Das war falsch:
    // in EffectsEd.exe stehen ab Adresse 607772 genau ZWEI Einträge,
    //
    //     10 units/foot (WARS)
    //     16 units/foot (SOF2)
    //
    // und der erste ist der voreingestellte. 16 gehört zu Soldier of Fortune 2,
    // nicht zu Jedi Academy.
    //
    // Das ist nicht nur eine Zahl im Auswahlfeld: der Maßstab skaliert den
    // Testraum und das Gitter, nicht die Teilchen. Bei 16 statt 10 ist der
    // Raum das 1,6-fache, und derselbe Effekt wirkt entsprechend kleiner —
    // beim Vergleich nebeneinander fiel genau das auf.
    float worldScale = 10.0f;
    float timeScale = 1.0f;
    float repeatRate = 0.300f;
    // Voreinstellung wie im Original: "Repeat until stopped" (Dialog 188,
    // gemessen) — Play legt alle Repeat-Rate-Sekunden nach.
    bool repeat = true;
    // Der Rest des Dialogs "Playback Settings" — das Original merkt sich
    // alles davon (Registry EffectPlay PlayDuration, PerframeRespawn,
    // AnimateSpawnPoint, SpawnVelocityX/Y/Z, SpawnResetDuration).
    // playbackMode: 0 einmal, 1 bis zum Anhalten, 2 fuer N Sekunden.
    int playbackMode = 1;
    float playDuration = 2.0f;
    bool perFrameRespawn = false;
    bool animateSpawnPoint = false;
    float spawnVelocity[3] = {0.0f, 0.0f, 0.0f};
    float spawnResetSeconds = 1.0f;


    std::string toIni() const;
    static Settings fromIni(const std::string& text);
};

// Die Weltmasstaebe aus dem Auswahlfeld des Originals.
struct WorldScale {
    i18n::Str labelId;
    float unitsPerFoot;
    const char* label() const { return i18n::tr(labelId); }
};
const WorldScale* worldScales();
int worldScaleCount();

}  // namespace efx::layout
