// Modelle im md3-Format lesen — fuer Emitter mit `useModel`.
//
// 43 der 46 Emitter im Grundspiel tragen ein Modell: Truemmer, Droidenkoepfe,
// Felsbrocken (chunks/r5d2head, explosions/wedge_explosion1). CEmitter::Draw
// zeichnet NUR dieses Modell; ohne es war ein Brocken unsichtbar.
//
// Das Format steht in qfiles.h der Engine (Quake III, von JKA unveraendert
// uebernommen):
//
//     md3Header_t      "IDP3", Version 15, Name[64], Zaehler und Versaetze
//     md3Frame_t       Huelle, Ursprung, Radius, Name[16]       je Bild
//     md3Tag_t         Name[64], Ursprung, Achsen               je Bild und Anheftpunkt
//     md3Surface_t     "IDP3", Name[64], Zaehler, Versaetze     je Flaeche
//       md3Shader_t    Name[64], Index
//       md3Triangle_t  drei Eckpunktnummern
//       md3St_t        Texturkoordinaten je Eckpunkt
//       md3XyzNormal_t xyz als short (mal MD3_XYZ_SCALE = 1/64), Normale
//                      als Breite/Laenge in je einem Byte — je Bild und Eckpunkt
//
// Alle Versaetze einer Flaeche zaehlen vom Anfang DIESER Flaeche, die
// Flaechen liegen hintereinander (ofsEnd fuehrt zur naechsten).
//
// Eine .md3 kommt aus einem fremden .pk3 — jede Zahl darin kann gelogen sein.
// Der Leser prueft deshalb jeden Versatz und jeden Zaehler gegen die
// Puffergroesse, bevor er liest, und jede Eckpunktnummer gegen die Zahl der
// Eckpunkte. Was nicht passt, wird abgewiesen, nicht geraten.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace efx::md3 {

// Ein Eckpunkt in Modellkoordinaten, schon umgerechnet.
struct Vertex {
    float pos[3]{0.0f, 0.0f, 0.0f};
    // Einheitsvektor. Die Engine braucht ihn fuer die Beleuchtung
    // (RB_CalcDiffuseColor) und fuer `tcGen environment`.
    float normal[3]{0.0f, 0.0f, 1.0f};
    float st[2]{0.0f, 0.0f};
};

struct Surface {
    std::string name;
    // Die Shadernamen, wie sie in der Datei stehen — oft MIT Endung
    // ("models/players/droids/body_r5d2.tga"). Gezeichnet wird mit dem
    // ersten (RB_SurfaceMesh nimmt shaders[0]); shaderName() bringt ihn in
    // die Form, unter der die Engine sucht.
    std::vector<std::string> shaders;
    // Nur das erste Bild (frame 0) — mehr zeigt ein Emitter nie: CEmitter
    // setzt mRefEnt.frame nicht, es bleibt 0 aus dem memset in CEffect().
    std::vector<Vertex> vertices;
    // Je Dreieck drei Nummern, jede kleiner als vertices.size().
    std::vector<uint16_t> indices;
};

struct Model {
    std::string name;
    int frames = 0;
    std::vector<Surface> surfaces;
    // Huelle und Radius von Bild 0, wie in md3Frame_t.
    float mins[3]{0.0f, 0.0f, 0.0f};
    float maxs[3]{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;

    bool ok = false;
    std::string error;
};

// Die Kennungen und Grenzen aus qfiles.h.
inline constexpr uint32_t kIdent = 0x33504449u;   // "IDP3", little endian
inline constexpr int kVersion = 15;
inline constexpr float kXyzScale = 1.0f / 64.0f;  // MD3_XYZ_SCALE
inline constexpr int kMaxSurfaces = 32;           // MD3_MAX_SURFACES
inline constexpr int kMaxFrames = 1024;           // MD3_MAX_FRAMES
inline constexpr int kMaxShaders = 256;           // MD3_MAX_SHADERS
inline constexpr int kMaxVerts = 4096;            // MD3_MAX_VERTS
inline constexpr int kMaxTriangles = 8192;        // MD3_MAX_TRIANGLES

// Liest ein Modell aus einem Puffer. Bei einem Fehler ist `ok` falsch und
// `error` sagt, woran es lag; die Listen sind dann leer.
Model parse(const unsigned char* data, size_t size);

// Der Name, unter dem die Engine den Shader einer Flaeche sucht.
//
// R_LoadMD3 ruft R_FindShader( md3Shader->name, lightmapsNone, ... ), und
// R_FindShader schneidet zuerst die Endung ab (COM_StripExtension). Dazu
// Kleinbuchstaben und Vorwaertsschraegstriche, wie ueberall im Bestand.
std::string shaderName(const std::string& fileName);

}  // namespace efx::md3
