// Sprite-Sheet -> Effekt.
//
// Ein Sprite-Sheet ist EIN Bild mit den Bildern einer Animation in einem
// Raster (etwa 4 x 4 Explosionsphasen). Die Engine kann so ein Raster nicht
// abspielen: Shader kennen keine Atlas-Koordinaten je Bild. Sie kennt aber
// Bildfolgen aus Einzelbildern — `animMap`, und von JKA dazu `oneshotanimMap`
// (einmal durchlaufen, auf dem letzten Bild stehen bleiben).
//
// Genau so macht es Raven selbst: gfx/exp/explosion1, rocket_explosion,
// gfx/exp/wookie (Bowcaster), demp2 ... sind Bildfolgen mit oneshotanimMap,
// und 53 der 54 Effekte, die sie benutzen, setzen `flags setShaderTime`
// (Spieldaten durchsucht, 14.11.2026). Damit beginnt die Folge fuer jedes
// Teilchen bei seinem Entstehen neu (tr_backend: shaderTime = floatTime -
// e.shaderTime) und nicht shaderweit.
//
// Grenzen der Engine (OpenJK, SP und MP):
//   - MAX_IMAGE_ANIMATIONS 32 (tr_shader.cpp, ParseStage): mehr Bilder in
//     einer Folge werden STILL weggelassen. Deshalb wird hier vorher
//     ausgeduennt und die Bildrate angepasst — mit Hinweis.
//   - Bildgroessen werden auf Zweierpotenzen gebracht (Upload32, mit
//     r_roundImagesDown ABgerundet). Wir rechnen selbst und sauber um.
//
// Ergebnis: ein Particle-Segment, ein Shaderblock, die Einzelbilder als
// 32-Bit-TGA — alles gewoehnliche Spieldaten, kein Laufzeit-Zusatz.
#pragma once

#include <string>
#include <vector>

#include "efx/effect.h"
#include "efx/image.h"

namespace efx::spritesheet {

inline constexpr int kMaxFrames = 32;  // MAX_IMAGE_ANIMATIONS

enum class Blend { Additive, Alpha };
enum class Preset { Explosion, Smoke, MuzzleFlash, Energy };

struct Layout {
    int columns = 4;
    int rows = 4;
    int first = 0;      // erstes Feld, ab 0, in Lesereihenfolge
    int count = 0;      // 0: bis zum letzten Feld
    bool columnsFirst = false;  // Spalte fuer Spalte statt Zeile fuer Zeile
    int margin = 0;     // Rand um das ganze Raster, Bildpunkte
    int spacing = 0;    // Abstand zwischen den Feldern, Bildpunkte
    bool skipEmptyAtEnd = true;  // leere Felder am Ende weglassen
};

struct Options {
    std::string name = "spritesheet";  // nur a-z 0-9 _
    std::string folder = "custom";     // effects/<folder>/, gfx/effects/<folder>/
    float fps = 20.0f;
    bool loop = false;                 // animMap statt oneshotanimMap
    Blend blend = Blend::Additive;
    // Zwei Stufen, die zweite ein Bild voraus, im Bildtakt uebergeblendet —
    // wie gfx/exp/explosion1 (rgbGen/alphaGen wave inversesawtooth/sawtooth).
    bool crossfade = false;
    // Fuer Bilder auf schwarzem Grund, die alphagemischt werden sollen:
    // Alpha = hellster Kanal, Farbe entsprechend aufgehellt.
    bool alphaFromBrightness = false;
    int frameSize = 0;                 // 0: naechste Zweierpotenz
    Preset preset = Preset::Explosion;
    float size = 32.0f;                // Radius des Teilchens am Anfang
};

struct Cell {
    int x = 0, y = 0, w = 0, h = 0;
};

enum class Error {
    None,
    NoImage,
    BadGrid,      // Raster passt nicht ins Bild / Felder kleiner als 1 px
    NoFrames,     // im gewaehlten Bereich kein (sichtbares) Feld
    BadName,
    BadFps,
};

// Alle Felder des Rasters in Lesereihenfolge.
std::vector<Cell> gridCells(int sheetWidth, int sheetHeight, const Layout& layout, Error* error);

// Bis zu `maximum` Felder aus `available`, gleichmaessig verteilt, das erste
// und das letzte immer dabei.
std::vector<int> pickFrames(int available, int maximum);

// Naechste Zweierpotenz zu einer Seitenlaenge (16 .. 1024).
int nearestPowerOfTwo(int side);

// Rechteck ausschneiden (RGBA).
std::vector<unsigned char> crop(const image::Image& sheet, const Cell& cell);
// Auf ein Quadrat erweitern (zentriert, durchsichtig schwarz) — das Teilchen
// ist quadratisch, ein schmales Bild wuerde sonst verzerrt.
std::vector<unsigned char> padToSquare(const std::vector<unsigned char>& rgba, int w, int h, int* side);
// Groesse aendern, Farbe mit Alpha vormultipliziert gemittelt (keine dunklen
// Raender um durchsichtige Stellen).
std::vector<unsigned char> resize(const std::vector<unsigned char>& rgba, int w, int h, int newW, int newH);
// Alpha aus der Helligkeit, Farbe zurueckgerechnet.
void alphaFromBrightness(std::vector<unsigned char>& rgba);
// Hat das Bild ueberhaupt durchsichtige Stellen?
bool hasTransparency(const image::Image& sheet);
// Ist ein Feld leer (nichts Sichtbares)? Additiv zaehlt die Helligkeit,
// alphagemischt das Alpha.
bool cellEmpty(const std::vector<unsigned char>& rgba, Blend blend);

// "Mein Feuerball!" -> "mein_feuerball"; leer, wenn nichts uebrig bleibt.
std::string sanitizeName(const std::string& text);

struct File {
    std::string path;  // spielrelativ, mit '/'
    std::vector<unsigned char> data;
};

struct Package {
    Error error = Error::None;
    std::vector<File> files;
    std::string effectPath;   // effects/<folder>/<name>.efx
    std::string shaderPath;   // shaders/<folder>_<name>.shader
    std::string shaderName;   // gfx/effects/<folder>/<name>
    std::string effectText;
    Effect effect;
    int sourceFrames = 0;     // Felder im gewaehlten Bereich (nach dem Weglassen leerer)
    int frames = 0;           // davon benutzt (hoechstens 32, mit Ueberblenden 31)
    float fps = 0.0f;         // nach dem Ausduennen
    int cellW = 0, cellH = 0; // Groesse eines Feldes im Sheet
    int frameSize = 0;        // Seitenlaenge der geschriebenen Bilder
    bool thinned = false;
    bool noTransparency = false;  // alphagemischt, aber das Bild hat kein Alpha
    bool ok() const { return error == Error::None; }
};

// Shadertext fuer `frames` Bildnamen (ohne Endbild), siehe Options.
std::string shaderText(const std::string& shaderName, const std::vector<std::string>& frames,
                       const std::string& endFrame, const Options& options, float fps);

// Alles zusammen: Felder schneiden, umrechnen, Shader, Effekt.
Package build(const image::Image& sheet, const Layout& layout, const Options& options,
              const std::string& sourceName = "");

// Ein Zip ohne Kompression (wie viele Spielarchive) — fuer ein .pk3.
std::vector<unsigned char> zipStored(const std::vector<File>& files);

}  // namespace efx::spritesheet
