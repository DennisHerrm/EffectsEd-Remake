// Die Geometrie des Testraums.
//
// Was das Original zeigt: ein Kasten mit Wänden, ein Bodengitter mit einem Fuß
// je Quadrat und drei Achsen im Ursprung, 16 Einheiten lang. Ravens Anleitung
// nennt diese Maße ausdrücklich.
//
// Erzeugt wird alles hier, ohne Grafikschnittstelle — beide Renderer bekommen
// dieselben Punkte und Linien. Damit lässt sich prüfen, ob das Gitter
// tatsächlich einen Fuß Abstand hat, statt es auf dem Bildschirm abzumessen.
#pragma once

#include <cstdint>
#include <vector>

#include "efx/camera.h"

namespace efx::scene {

struct Vertex {
    float pos[3];
    float uv[2];
    uint32_t colour;  // RGBA, gepackt
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

struct LineSet {
    std::vector<Vertex> vertices;  // paarweise: Anfang, Ende
};

// Maße des Raums, in Fuß. Wird mit dem Weltmaßstab in Einheiten umgerechnet.
struct RoomSize {
    float widthFeet = 32.0f;
    float depthFeet = 48.0f;
    float heightFeet = 20.0f;
};

// Die Wandtexturen des Testraums: Ziegel, Erde, Putz.
//
// **Gerechnet, nicht geladen.** Das Original liefert `brick.jpg`, `dirt.jpg`
// und `stucco.jpg` mit; wir haben sie nicht und dürften sie auch nicht
// beilegen. Ein Menüpunkt, der nichts tut, ist aber schlechter als ein
// gerechnetes Muster — und für die Aufgabe, ein Gefühl für Maßstab und
// Entfernung zu geben, reicht ein Muster vollkommen.
//
// Ergebnis ist RGBA, `size` × `size` Bildpunkte, nahtlos kachelbar.
enum class WallTexture { None, Brick, Dirt, Stucco };

std::vector<unsigned char> buildWallTexture(WallTexture kind, int size,
                                            uint32_t baseColour);

// Die Grundfarbe einer Wandart.
//
// An Ravens Dateien GEMESSEN, nicht geschätzt — sie liegen als Vergleich vor,
// beilegen dürfen wir sie nicht:
//
//     stucco.jpg  256×256  mittlere Helligkeit 138.5  Streuung 21.7  R144 G140 B115
//     dirt.jpg    128×128  mittlere Helligkeit  93.7  Streuung 10.7  R128 G84  B55
//     brick.jpg   256×256  mittlere Helligkeit 103.0  Streuung 21.4  R129 G96  B73
//
// Vorher trugen unsere Muster gar keine Farbe: sie waren neutrales Hellgrau
// und wurden mit der Wandfarbe des Themas multipliziert. Bei einem dunklen
// Thema ergab das einen grünlichen Raum — im Vergleich mit dem Original fiel
// das sofort auf.
uint32_t wallColourOf(WallTexture kind);

// Wie der Testraum aussehen soll.
//
// Der geschlossene Kasten ist die Voreinstellung, wie im Original: er gibt
// einem sofort ein Gefühl für Maßstab, weil Wände und Decke in bekanntem
// Abstand stehen.
//
// Draußen ist die zweite Art, und sie ist für manche Effekte die richtige —
// eine Explosion, die im Spiel unter freiem Himmel steht, sieht in einem
// engen Kasten falsch aus, weil die Wände sie beschneiden und die Beleuchtung
// nicht stimmt.
enum class RoomStyle {
    Enclosed,  // Boden, vier Wände, Decke
    OpenSky,   // nur Boden, darüber Himmel
    None,      // gar nichts
};

// Der Himmel: eine große Kuppel mit eingebackenem Verlauf.
//
// Ohne Textur, absichtlich. Ein Himmel aus dem Spielordner wäre schöner, aber
// er hinge am Spielpfad — und die Vorschau soll auch ohne gesetzten Pfad
// brauchbar sein. Der Verlauf reicht, um oben von unten zu unterscheiden, und
// genau darum geht es.
//
// sunDirection zeigt **zur** Sonne. Um sie herum wird der Himmel heller.
Mesh buildSky(float radiusUnits, const camera::Vec3& sunDirection,
              uint32_t horizonColour, uint32_t zenithColour, uint32_t sunColour);

// Die Sonne als Scheibe am Himmel. Klein, aber sie sagt sofort, woher das
// Licht kommt.
Mesh buildSunDisc(const camera::Vec3& sunDirection, float radiusUnits,
                  float discSize, uint32_t colour);

// Wie stark eine Fläche von der Sonne beschienen wird.
//
// Lambert: der Kosinus zwischen Flächennormale und Sonnenrichtung, unten
// abgeschnitten. Dazu ein Grundanteil, damit abgewandte Flächen nicht schwarz
// werden — im Spiel gibt es immer etwas Streulicht, und eine schwarze Wand
// verrät nichts über den Effekt davor.
float sunLambert(const camera::Vec3& normal, const camera::Vec3& sunDirection,
                 float ambient);

// Der Kasten. Die Wände zeigen nach innen, sonst sieht man von außen hinein
// und von innen nichts.
//
// wallColour ist die Grundfarbe; Boden, Wände und Decke bekommen leicht
// verschiedene Helligkeiten, sonst verschwimmen die Kanten und man verliert
// jedes Gefühl für den Raum. Das Original löst das über sein Licht; wir backen
// es in die Eckpunkte, weil die Vorschau keine Beleuchtung braucht.
Mesh buildRoom(const RoomSize& size, float worldScale, uint32_t wallColour);

// Wie oben, aber mit Sonnenlicht in die Eckpunkte gebacken und wählbarer Art.
//
// Bei `OpenSky` entstehen nur Boden und ein niedriger Sockel — die Wände
// fehlen, der Himmel übernimmt.
// includeWalls trennt Wände und Decke vom Boden.
//
// Das entspricht dem Knopf „Draw Room" des Originals — dessen Hilfetext lautet
// *„Draw the walls of the testing room"*. Ausgeschaltet bleibt der Boden
// stehen; nur die Wände verschwinden. Ich hatte den Knopf zuerst als „ganzer
// Raum" gelesen und damit auch den Boden mit abgeschaltet.
Mesh buildRoomLit(const RoomSize& size, float worldScale, uint32_t wallColour,
                  RoomStyle style, const camera::Vec3& sunDirection,
                  float ambient, bool sunEnabled, bool includeWalls = true);

// Bodengitter. Ein Quadrat je Fuß, jede zehnte Linie kräftiger — sonst zählt
// man beim Abschätzen einer Entfernung Linien.
// `walls`: das Gitter auch auf Wände und Decke legen, nicht nur auf den Boden.
//
// Der Boden allein sagt wenig über die Höhe eines Effekts — eine Flamme, die
// bis zur Decke reicht, sieht genauso aus wie eine halb so hohe, solange
// nichts danebensteht, woran man messen kann. Mit Gitter auf allen Flächen
// wird der Raum zum Maßstab.
LineSet buildGrid(const RoomSize& size, float worldScale, uint32_t colour,
                  uint32_t majorColour, bool walls = false);

// Die drei Achsen im Ursprung. 16 Einheiten lang, wie im Original.
// X rot, Y grün, Z blau — die Farben aus dem Bildschirmfoto.
LineSet buildAxes(float lengthUnits, uint32_t x, uint32_t y, uint32_t z);

// Der Windvektor des Originals ist keine starre Linie, sondern eine **Fahne**:
// ein Wimpel am Mast, der weht und dabei die Richtung wechselt.
//
// Das ist mehr als Zierde. Eine feste Linie sagt „der Wind kommt von dort";
// eine wehende Fahne sagt zusätzlich, wie stark er ist und dass er schwankt.
// Genau darum ging es Raven: der Wind sollte sich anfühlen wie Wind.
//
// (Dass die Engine den Wert nie ausliest, steht auf einem anderen Blatt —
// siehe ENTSCHEIDUNGEN.md. Als Anzeigehilfe bleibt die Fahne trotzdem nützlich.)
//
// seconds treibt die Bewegung. Die Richtung schwankt langsam um die
// eingestellte Grundrichtung, das Tuch wellt sich schneller.
//
// `footPosition` ist der Fußpunkt des Mastes. Er gehört **nicht** in den
// Ursprung, und zwar aus zwei Gründen:
//
//   - Dort entsteht der Effekt. Eine Anzeigehilfe, die mitten in dem steht,
//     was man beurteilen will, ist keine Hilfe.
//   - Der Ursprung ist der Zielpunkt der Umlaufkamera. Beim Drehen bleibt er
//     in der Bildmitte stehen — die Fahne sieht dann aus, als hinge sie an
//     der Kamera, obwohl sie stillsteht.
//
// Beides löst dieselbe Änderung: die Fahne gehört an den Rand.
LineSet buildWindFlag(const camera::Vec3& baseDirection, float lengthUnits,
                      float seconds, float strength, uint32_t poleColour,
                      uint32_t clothColour,
                      const camera::Vec3& footPosition = {});

// Wo die Fahne im Testraum steht: an einer Ecke, auf dem Boden.
camera::Vec3 windFlagPosition(const RoomSize& room, float worldScale);

// Die tatsächliche Windrichtung zum Zeitpunkt t — dieselbe Schwankung, die
// auch die Fahne zeigt. Getrennt, damit sie sich prüfen lässt.
camera::Vec3 windDirectionAt(const camera::Vec3& baseDirection, float seconds,
                             float strength);

// Packt eine Farbe. Reihenfolge RGBA, wie beide Renderer sie erwarten.
constexpr uint32_t rgba(int r, int g, int b, int a = 255) {
    return (static_cast<uint32_t>(r & 0xFF)) |
           (static_cast<uint32_t>(g & 0xFF) << 8) |
           (static_cast<uint32_t>(b & 0xFF) << 16) |
           (static_cast<uint32_t>(a & 0xFF) << 24);
}

}  // namespace efx::scene
