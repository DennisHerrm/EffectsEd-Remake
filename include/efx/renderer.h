// Zwei Grafikschnittstellen, umschaltbar.
//
// Warum überhaupt beide?
//
// Direct3D 11 ist die naheliegende Wahl unter Windows und dieselbe, die g2c
// benutzt. Sie ist aber nicht immer da: über Remotedesktop, in virtuellen
// Maschinen, unter Wine und auf sehr alter Hardware schlägt die Erzeugung des
// Geräts fehl. Genau so ein Fall hat bei g2c einen Rechner lahmgelegt, nur mit
// einer Schriftart statt einer Grafikschnittstelle. OpenGL läuft dort, wo
// Direct3D aussteigt — und der alte EffectsEd war ohnehin OpenGL.
//
// Deshalb: beide fest eingebaut, Direct3D 11 als Voreinstellung, OpenGL als
// Rückfall, und ein Menüpunkt zum Umschalten.
//
// Zum Umschalten zur Laufzeit: Dear ImGui erlaubt es nicht, den Renderer im
// laufenden Betrieb auszutauschen — die Schriftartentexturen, die
// Zeichenpuffer und bei mehreren Ansichtsfenstern auch die Fensterverwaltung
// hängen daran (ocornut/imgui#4616 beschreibt genau diesen Versuch und die
// Ausnahmen, die er auslöst). Der einzige verlässliche Weg ist, die
// Oberflächenschicht abzubauen und neu aufzubauen.
//
// Das ist hier unproblematisch, weil der gesamte Zustand — geöffnete Datei,
// Änderungsverlauf, Auswahl, Einstellungen — im Kern liegt und nichts davon
// den Renderer kennt. Der Wechsel ist ein Neustart des Fensters, kein Neustart
// des Programms; man verliert nicht einmal die Cursorposition.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace efx::render {

enum class Backend {
    Direct3D11,
    OpenGL3,
};

const char* backendName(Backend backend);   // "Direct3D 11", "OpenGL 3.3"
const char* backendCode(Backend backend);   // "d3d11", "gl3" — Einstellungsdatei
bool backendFromCode(const std::string& code, Backend& out);

// Was beim Anlauf über eine Schnittstelle herauskam.
struct Probe {
    Backend backend = Backend::Direct3D11;
    bool available = false;
    std::string adapter;   // Name der Grafikkarte
    std::string version;   // Fassung des Treibers oder der Schnittstelle
    std::string failure;   // Grund, falls nicht verfügbar
};

// Die Reihenfolge, in der Schnittstellen probiert werden, wenn die gewünschte
// nicht anläuft. Getrennt von der eigentlichen Erzeugung gehalten, damit sich
// die Regel ohne Grafikkarte prüfen lässt.
std::vector<Backend> fallbackOrder(Backend preferred);

// Wählt aus den Anlaufergebnissen die zu benutzende Schnittstelle.
// Gibt false zurück, wenn keine einzige läuft — dann bleibt nur eine
// Fehlermeldung, und die soll aussagekräftig sein.
bool chooseBackend(Backend preferred, const std::vector<Probe>& probes,
                   Backend& chosen, std::string& note);

// --- Die Schnittstelle selbst ------------------------------------------
//
// Absichtlich schmal. Die Vorschau braucht texturierte Dreiecke mit
// Überblendung, Linien und ein Ziel, in das sie zeichnet — mehr nicht. Was
// darüber hinausgeht, gehört in den Effektteil, nicht in den Renderer.

// Muss Feld fuer Feld zu efx::scene::Vertex passen: die Szene wird ohne
// Kopie an den Renderer gereicht. Ein static_assert in scene.cpp haelt beide
// zusammen — weichen sie ab, zeichnet der Renderer Muell, und das sieht man
// nicht sofort, sondern nur als seltsame Farben.
struct Vertex {
    float pos[3];
    float uv[2];
    unsigned int color;  // RGBA, gepackt
};

// Rueckseitenaussortierung.
//
// Nicht global, sondern je Zeichenaufruf: der Testraum braucht sie — nur so
// faellt beim Herauszoomen die naechstgelegene Wand weg und man schaut in den
// Kasten hinein, wie im alten Editor. Effektpartikel dagegen sind flache
// Vierecke, die man von beiden Seiten sieht; mit Aussortierung waere die
// Haelfte davon unsichtbar.
//
// Beide Schnittstellen sind so eingestellt, dass GEGEN den Uhrzeigersinn die
// Vorderseite ist. Das ist bei OpenGL die Voreinstellung, bei Direct3D nicht —
// dort wird FrontCounterClockwise gesetzt. Ohne diese Angleichung waere in
// einer der beiden Schnittstellen genau die falsche Haelfte sichtbar.
enum class Cull {
    None,      // beide Seiten, fuer Partikel und Linien
    BackFaces, // nur Vorderseiten, fuer den Raum
};

// Fuellart. Das Ansichtsmenue des Originals bietet sie fuer den Raum
// ("Wireframe") und getrennt fuer die Effekte an.
enum class Fill {
    Solid,
    Wireframe,
};

// Muss Wert fuer Wert zu efx::shader::BlendMode passen: die Oberflaeche setzt
// eine Art direkt in die andere um. Der static_assert unter dieser
// Aufzaehlung haelt beide zusammen — weichen sie ab, mischt die Vorschau
// stillschweigend falsch, und das faellt bei einem Funken kaum auf.
enum class Blend {
    Opaque,      // GL_ONE GL_ZERO
    Additive,    // GL_ONE GL_ONE
    AlphaBlend,  // GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    Modulate,    // GL_DST_COLOR GL_ZERO
    Filter,      // GL_DST_COLOR GL_SRC_COLOR
};

// Eine Texturkennung muss beides aufnehmen koennen:
//
//   OpenGL  gibt eine Nummer zurueck (GLuint, 32 Bit)
//   D3D11   gibt einen Zeiger zurueck (ID3D11ShaderResourceView*, 64 Bit)
//
// Der erste Entwurf hatte hier unsigned int — gedacht war an die
// OpenGL-Nummer, vergessen war der Zeiger. Unter x64 schneidet das die oberen
// 32 Bit ab: der Uebersetzer meldet das als C4311, aber eine Warnung uebersieht
// man, und dann zeichnet das Programm mit einer halben Adresse.
//
// uintptr_t ist genau der Typ, der einen Zeiger sicher aufnimmt. Der
// static_assert stellt sicher, dass das auch auf einer Plattform gilt, an die
// heute niemand denkt.
using TextureId = std::uintptr_t;
static_assert(sizeof(TextureId) >= sizeof(void*),
              "TextureId muss einen Zeiger aufnehmen koennen — Direct3D legt "
              "dort eine ID3D11ShaderResourceView* ab.");
constexpr TextureId kNoTexture = 0;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual Backend backend() const = 0;
    virtual const Probe& probe() const = 0;

    // Die ImGui-Anbindung. Getrennt vom Anlegen, und das ist keine
    // Geschmacksfrage:
    //
    // ImGui_ImplDX11_Init und ImGui_ImplOpenGL3_Init greifen auf den
    // ImGui-Kontext zu. Der entsteht aber erst mit ImGui::CreateContext(),
    // und das passiert *nach* dem Anlegen der Grafikschnittstelle — die
    // Schriftgroesse haengt an der Bildschirmskalierung, und die kennt man
    // erst, wenn das Fenster steht.
    //
    // Standen die Init-Aufrufe im Erzeuger, lief das Programm in einen
    // Nullzeiger und starb wortlos. Genau dort endete das Startprotokoll.
    //
    // Reihenfolge beim Start:   Renderer anlegen -> CreateContext ->
    //                           Schriften -> initImGuiBackend
    // Reihenfolge beim Ende:    shutdownImGuiBackend -> DestroyContext ->
    //                           Renderer freigeben
    virtual bool initImGuiBackend() = 0;
    virtual void shutdownImGuiBackend() = 0;

    // Der Bildtakt. newFrame() ruft die ImGui-Anbindung der jeweiligen
    // Schnittstelle auf, present() zeigt das fertige Bild.
    virtual void newFrame() = 0;
    virtual void clear(float r, float g, float b, float a) = 0;
    virtual void renderImGui() = 0;
    virtual void present(bool vsync) = 0;

    // Nach einer Fensteraenderung. Muss vor dem naechsten Bild passieren,
    // sonst zeichnet man in einen Puffer der alten Groesse.
    virtual void resizeSwapChain(int width, int height) = 0;

    // Ansichtsfenster, in das die Vorschau zeichnet. Wird als Textur an die
    // Oberfläche zurückgegeben, damit es in den Bereich zwischen den Teilern
    // passt statt das ganze Fenster zu füllen.
    virtual void resizeViewport(int width, int height) = 0;
    virtual TextureId viewportTexture() const = 0;

    // Steht die Textur auf dem Kopf?
    //
    // OpenGL legt den Ursprung einer Textur unten links, Direct3D oben links.
    // Wer in einen Bildpuffer zeichnet und das Ergebnis anzeigt, bekommt bei
    // OpenGL deshalb ein senkrecht gespiegeltes Bild.
    //
    // Der Renderer sagt es, statt dass die Oberflaeche raet. Sie dreht dann
    // die Texturkoordinaten — das kostet nichts, waehrend ein Spiegeln der
    // Projektionsmatrix zusaetzlich die Umlaufrichtung der Dreiecke umkehren
    // wuerde und damit die Aussortierung durcheinanderbraechte.
    virtual bool viewportTextureFlipped() const = 0;

    virtual void beginViewport(float r, float g, float b, float a) = 0;

    // Nur in einen Ausschnitt des Ansichtsziels zeichnen.
    //
    // Der Grund ist gemessen und nicht geraten: den Zielpuffer zu wechseln ist
    // das Teure, nicht das Zeichnen. Fünfzig einzelne Ziele in einem Bild
    // kosten rund 12 ms, dieselbe Arbeit in einem Ziel gebündelt rund 1,3 ms —
    // Faktor zehn, und zwar für Arbeit, die man gar nicht leisten müsste.
    //
    // Für den Effektbrowser heißt das: **ein** Ziel für alle Kacheln, und je
    // Kachel nur der Ausschnitt umgestellt. Sechzig sichtbare Kacheln kosten
    // dann sechzig Zeichenaufrufe statt sechzig Zielwechsel.
    //
    // `x` und `y` zählen von links oben. Ein Rechteck der Breite oder Höhe
    // null stellt auf das ganze Ziel zurück.
    virtual void setViewportRect(int x, int y, int width, int height) = 0;
    virtual void setCamera(const float viewMatrix[16],
                           const float projMatrix[16]) = 0;

    // Erwartet die Schnittstelle einen Tiefenbereich von 0 bis 1?
    //
    // OpenGL bildet auf -1 bis 1 ab, Direct3D auf 0 bis 1. Wer eine
    // OpenGL-Projektion unveraendert an Direct3D gibt, verliert die vordere
    // Haelfte des Tiefenbereichs — dort wird geklippt. Der Aufrufer holt sich
    // die passende Matrix, statt dass jeder Renderer sie umrechnet.
    virtual bool wantsZeroToOneDepth() const = 0;
    virtual void setBlend(Blend mode) = 0;
    virtual void setCulling(Cull mode) = 0;
    virtual void setFill(Fill mode) = 0;
    virtual void setDepthWrite(bool enabled) = 0;
    virtual void drawTriangles(const Vertex* vertices, int vertexCount,
                               const unsigned short* indices, int indexCount,
                               TextureId texture) = 0;
    virtual void drawLines(const Vertex* vertices, int vertexCount,
                           float width) = 0;
    // Wie beginViewport, aber die FARBE bleibt stehen — nur die Tiefe wird
    // geleert.
    //
    // Dafuer gibt es genau einen Grund, und er ist gemessen: der
    // Effektbrowser zeigt bei kleiner Kachelgroesse ueber hundert Vorschauen
    // gleichzeitig. Alle in jedem Bild neu zu zeichnen sind rund
    // viertausend API-Aufrufe und achthundert Pufferabbildungen — und jede
    // Abbildung ist ein Haltepunkt fuer den Treiber. Gemessen beim Anwender:
    // 6,7 Bilder je Sekunde.
    //
    // Also wird je Bild nur eine Handvoll Kacheln neu gezeichnet und der Rest
    // bleibt stehen. Das geht nur, wenn das Ziel nicht geleert wird.
    //
    // Die Tiefe wird trotzdem geleert: sie ist je Kachel bedeutungslos,
    // sobald die Kachel neu gezeichnet wird, und ein einzelner Aufruf fuer
    // den ganzen Puffer kostet nichts.
    virtual void beginViewportPreserving() = 0;

    // Nur den eingestellten Ausschnitt mit einer Farbe fuellen.
    //
    // Gehoert zu beginViewportPreserving: eine Kachel, die neu gezeichnet
    // wird, muss ihren alten Inhalt loswerden — aber nur ihren.
    virtual void clearViewportRect(float r, float g, float b, float a) = 0;

    virtual void endViewport() = 0;

    // Texturen. Die Bilddaten kommen immer als RGBA8 herein; das Umwandeln von
    // TGA, JPG, PNG, PCX und BMP passiert eine Ebene höher und ist damit für
    // beide Schnittstellen dieselbe Arbeit.
    // Liest die zuletzt gezeichnete Ansicht als RGBA zurück — für
    // Bildschirmfotos.
    //
    // Gibt false zurück, wenn die Schnittstelle es nicht kann; der Aufrufer
    // soll dann etwas sagen und nicht ein leeres Bild schreiben.
    virtual bool readViewport(std::vector<unsigned char>& rgba, int& width,
                              int& height) = 0;

    // Das ganze Fenster, wie es gleich angezeigt wird — nach renderImGui,
    // vor present. Fuer die Fotos des Selbsttests: ein Bildschirmfoto von
    // aussen zeigte, was gerade auf dem Bildschirm des Anwenders liegt, und
    // kaeme nie genau im richtigen Bild.
    virtual bool readBackbuffer(std::vector<unsigned char>& rgba, int& width,
                                int& height) = 0;

    virtual TextureId createTexture(const unsigned char* rgba, int width,
                                    int height, bool clamp, bool mipmaps) = 0;
    virtual void destroyTexture(TextureId texture) = 0;
};

// Legt eine Schnittstelle an. Gibt bei Misserfolg nullptr zurück und füllt
// probe.failure — nie eine Ausnahme, weil der Aufrufer den Rückfall braucht.
//
// Die beiden Umsetzungen liegen in gui/renderer_d3d11.cpp und
// gui/renderer_gl3.cpp und sind die einzigen Dateien, die Direct3D
// beziehungsweise OpenGL einbinden.
std::unique_ptr<Renderer> createRenderer(Backend backend, void* windowHandle,
                                         Probe& probe);

}  // namespace efx::render
