#include "efx/scene.h"

#include "efx/renderer.h"

// Die Szene wird ohne Kopie an den Renderer gereicht. Weicht das Layout ab,
// zeichnet er Muell — und das sieht man nicht als Absturz, sondern als
// seltsame Farben, die man dann in der Effektdatei sucht.
static_assert(sizeof(efx::scene::Vertex) == sizeof(efx::render::Vertex),
              "scene::Vertex und render::Vertex muessen gleich gross sein");
static_assert(offsetof(efx::scene::Vertex, pos) == offsetof(efx::render::Vertex, pos),
              "Feld pos liegt verschieden");
static_assert(offsetof(efx::scene::Vertex, uv) == offsetof(efx::render::Vertex, uv),
              "Feld uv liegt verschieden");
static_assert(offsetof(efx::scene::Vertex, colour) == offsetof(efx::render::Vertex, color),
              "Farbfeld liegt verschieden");

#include <cmath>

namespace efx::scene {
namespace {

// Helligkeit einer gepackten Farbe aendern, Alpha bleibt.
uint32_t shade(uint32_t colour, float factor) {
    auto channel = [&](int shift) {
        int value = static_cast<int>(((colour >> shift) & 0xFF) * factor);
        return value > 255 ? 255 : (value < 0 ? 0 : value);
    };
    return rgba(channel(0), channel(8), channel(16), (colour >> 24) & 0xFF);
}

void addQuad(Mesh& mesh, const camera::Vec3& a, const camera::Vec3& b,
             const camera::Vec3& c, const camera::Vec3& d, uint32_t colour,
             float uvScale) {
    const auto base = static_cast<uint16_t>(mesh.vertices.size());
    const camera::Vec3 corners[4] = {a, b, c, d};
    const float uvs[4][2] = {{0.0f, 0.0f}, {uvScale, 0.0f},
                             {uvScale, uvScale}, {0.0f, uvScale}};
    for (int i = 0; i < 4; ++i) {
        Vertex v{};
        v.pos[0] = corners[i].x;
        v.pos[1] = corners[i].y;
        v.pos[2] = corners[i].z;
        v.uv[0] = uvs[i][0];
        v.uv[1] = uvs[i][1];
        v.colour = colour;
        mesh.vertices.push_back(v);
    }
    const uint16_t order[6] = {0, 1, 2, 0, 2, 3};
    for (uint16_t offset : order) {
        mesh.indices.push_back(static_cast<uint16_t>(base + offset));
    }
}

void addLine(LineSet& lines, const camera::Vec3& from, const camera::Vec3& to,
             uint32_t colour) {
    Vertex a{};
    a.pos[0] = from.x; a.pos[1] = from.y; a.pos[2] = from.z;
    a.colour = colour;
    Vertex b{};
    b.pos[0] = to.x; b.pos[1] = to.y; b.pos[2] = to.z;
    b.colour = colour;
    lines.vertices.push_back(a);
    lines.vertices.push_back(b);
}

}  // namespace

Mesh buildRoom(const RoomSize& size, float worldScale, uint32_t wallColour) {
    Mesh mesh;
    if (worldScale <= 0.0f) worldScale = 16.0f;

    const float halfX = size.widthFeet * worldScale * 0.5f;
    const float halfY = size.depthFeet * worldScale * 0.5f;
    const float height = size.heightFeet * worldScale;

    // Eine Texturkachel je zwei Fuss. Ohne Bezug zum Massstab waere die
    // Kachelung bei 64 Einheiten je Fuss viermal so fein wie bei 16.
    const float uvX = size.widthFeet * 0.5f;
    const float uvY = size.depthFeet * 0.5f;
    const float uvZ = size.heightFeet * 0.5f;

    // Boden am hellsten, Decke am dunkelsten. Ohne diesen Unterschied
    // verschwimmen die Kanten und man verliert jedes Gefuehl fuer den Raum —
    // im Original macht das die Beleuchtung, hier stecken die Werte in den
    // Eckpunkten.
    const uint32_t floorColour = shade(wallColour, 1.00f);
    const uint32_t wallSide = shade(wallColour, 0.86f);
    const uint32_t wallBack = shade(wallColour, 0.78f);
    const uint32_t ceiling = shade(wallColour, 0.55f);

    const camera::Vec3 f00{-halfX, -halfY, 0.0f};
    const camera::Vec3 f10{ halfX, -halfY, 0.0f};
    const camera::Vec3 f11{ halfX,  halfY, 0.0f};
    const camera::Vec3 f01{-halfX,  halfY, 0.0f};
    const camera::Vec3 c00{-halfX, -halfY, height};
    const camera::Vec3 c10{ halfX, -halfY, height};
    const camera::Vec3 c11{ halfX,  halfY, height};
    const camera::Vec3 c01{-halfX,  halfY, height};

    // Alle Vorderseiten zeigen nach INNEN.
    //
    // Das ist die Bedingung dafuer, dass Rueckseitenaussortierung das Richtige
    // tut: von innen sieht man alle sechs Flaechen, von aussen faellt die
    // naechstgelegene weg und man schaut in den Kasten hinein. Genau so
    // verhaelt sich der alte Editor.
    //
    // Der erste Anlauf hatte die Reihenfolge genau umgekehrt — mein Kommentar
    // behauptete "nach innen", die Nachrechnung ergab bei allen sechs "nach
    // aussen". Deshalb prueft der Test das jetzt, statt dass ein Kommentar es
    // versichert.
    addQuad(mesh, f00, f10, f11, f01, floorColour, uvX);          // Boden
    addQuad(mesh, c00, c01, c11, c10, ceiling, uvX);              // Decke
    addQuad(mesh, f00, c00, c10, f10, wallBack, uvX * 0.5f);      // -Y
    addQuad(mesh, f11, c11, c01, f01, wallBack, uvX * 0.5f);      // +Y
    addQuad(mesh, f01, c01, c00, f00, wallSide, uvY * 0.5f);      // -X
    addQuad(mesh, f10, c10, c11, f11, wallSide, uvY * 0.5f);      // +X
    (void)uvZ;

    return mesh;
}

LineSet buildGrid(const RoomSize& size, float worldScale, uint32_t colour,
                  uint32_t majorColour, bool walls) {
    LineSet lines;
    if (worldScale <= 0.0f) worldScale = 16.0f;

    const int stepsX = static_cast<int>(size.widthFeet);
    const int stepsY = static_cast<int>(size.depthFeet);
    const float halfX = size.widthFeet * worldScale * 0.5f;
    const float halfY = size.depthFeet * worldScale * 0.5f;

    // Ein Quadrat je Fuss — so steht es in Ravens Anleitung.
    // Knapp ueber dem Boden, sonst streiten Gitter und Bodenflaeche um
    // dieselben Tiefenwerte und das Gitter flackert.
    const float z = worldScale * 0.01f;

    for (int i = 0; i <= stepsX; ++i) {
        const float x = -halfX + static_cast<float>(i) * worldScale;
        const bool major = (i % 10) == 0;
        addLine(lines, {x, -halfY, z}, {x, halfY, z},
                major ? majorColour : colour);
    }
    for (int i = 0; i <= stepsY; ++i) {
        const float y = -halfY + static_cast<float>(i) * worldScale;
        const bool major = (i % 10) == 0;
        addLine(lines, {-halfX, y, z}, {halfX, y, z},
                major ? majorColour : colour);
    }

    if (!walls) return lines;

    // Vier Waende und die Decke.
    //
    // Derselbe Abstand von einem Fuss und dieselbe Betonung jeder zehnten
    // Linie. Der kleine Versatz `z` wird hier zum Versatz NACH INNEN: sonst
    // liegt das Gitter genau in der Wandflaeche und flackert.
    const float top = size.heightFeet * worldScale;
    const int stepsZ = static_cast<int>(size.heightFeet);
    const float inset = z;

    // Die beiden Waende bei -Y und +Y: senkrechte Linien ueber x, waagerechte
    // ueber die Hoehe.
    for (const float wallY : {-halfY + inset, halfY - inset}) {
        for (int i = 0; i <= stepsX; ++i) {
            const float x = -halfX + static_cast<float>(i) * worldScale;
            addLine(lines, {x, wallY, 0.0f}, {x, wallY, top},
                    (i % 10) == 0 ? majorColour : colour);
        }
        for (int i = 0; i <= stepsZ; ++i) {
            const float h = static_cast<float>(i) * worldScale;
            addLine(lines, {-halfX, wallY, h}, {halfX, wallY, h},
                    (i % 10) == 0 ? majorColour : colour);
        }
    }

    // Die beiden Waende bei -X und +X.
    for (const float wallX : {-halfX + inset, halfX - inset}) {
        for (int i = 0; i <= stepsY; ++i) {
            const float y = -halfY + static_cast<float>(i) * worldScale;
            addLine(lines, {wallX, y, 0.0f}, {wallX, y, top},
                    (i % 10) == 0 ? majorColour : colour);
        }
        for (int i = 0; i <= stepsZ; ++i) {
            const float h = static_cast<float>(i) * worldScale;
            addLine(lines, {wallX, -halfY, h}, {wallX, halfY, h},
                    (i % 10) == 0 ? majorColour : colour);
        }
    }

    // Die Decke.
    const float ceiling = top - inset;
    for (int i = 0; i <= stepsX; ++i) {
        const float x = -halfX + static_cast<float>(i) * worldScale;
        addLine(lines, {x, -halfY, ceiling}, {x, halfY, ceiling},
                (i % 10) == 0 ? majorColour : colour);
    }
    for (int i = 0; i <= stepsY; ++i) {
        const float y = -halfY + static_cast<float>(i) * worldScale;
        addLine(lines, {-halfX, y, ceiling}, {halfX, y, ceiling},
                (i % 10) == 0 ? majorColour : colour);
    }
    return lines;
}

LineSet buildAxes(float lengthUnits, uint32_t x, uint32_t y, uint32_t z) {
    LineSet lines;
    addLine(lines, {0.0f, 0.0f, 0.0f}, {lengthUnits, 0.0f, 0.0f}, x);
    addLine(lines, {0.0f, 0.0f, 0.0f}, {0.0f, lengthUnits, 0.0f}, y);
    addLine(lines, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, lengthUnits}, z);
    return lines;
}

camera::Vec3 windDirectionAt(const camera::Vec3& baseDirection, float seconds,
                             float strength) {
    const camera::Vec3 base = camera::normalise(baseDirection);
    if (camera::length(base) < 1e-6f) return {1.0f, 0.0f, 0.0f};

    // Die Richtung schwankt um die Grundrichtung. Zwei Schwingungen mit
    // unrunden Frequenzen, damit sich das Muster nicht sichtbar wiederholt —
    // bei ganzzahligem Verhaeltnis sieht man nach wenigen Sekunden dieselbe
    // Bewegung wieder, und dann wirkt es wie eine Maschine, nicht wie Wind.
    const float swing = 0.35f * strength;
    const float yaw = swing * (std::sin(seconds * 0.7f) +
                               0.4f * std::sin(seconds * 1.9f));
    const float pitch = swing * 0.3f * std::sin(seconds * 1.13f);

    // Um die Hochachse drehen, dann etwas neigen.
    const float c = std::cos(yaw), s = std::sin(yaw);
    camera::Vec3 turned{base.x * c - base.y * s, base.x * s + base.y * c, base.z};
    turned.z += pitch;
    return camera::normalise(turned);
}

camera::Vec3 windFlagPosition(const RoomSize& room, float worldScale) {
    (void)room;
    if (worldScale <= 0.0f) worldScale = 16.0f;

    // In der Mitte, aber nicht IM Ursprung.
    //
    // Zwischenzeitlich stand sie am Rand — weil sie im Ursprung genau dort
    // sitzt, wo der Effekt entsteht, und weil der Ursprung der Zielpunkt der
    // Umlaufkamera ist, wodurch sie beim Drehen mitzugehen scheint.
    //
    // Am Rand ist sie aber schlecht zu sehen und steht bei "Boden und Himmel"
    // im Nichts. Der Kompromiss: in der Mitte, um zwei Fuss versetzt und
    // etwas erhoeht — nah genug, um den Wind am Effekt abzulesen, weit genug,
    // um nicht mitten darin zu stehen.
    const float offset = worldScale * 2.0f;
    return {-offset, -offset, 0.0f};
}

LineSet buildWindFlag(const camera::Vec3& baseDirection, float lengthUnits,
                      float seconds, float strength, uint32_t poleColour,
                      uint32_t clothColour, const camera::Vec3& footPosition) {
    LineSet lines;
    if (lengthUnits <= 0.0f) return lines;

    const camera::Vec3 dir = windDirectionAt(baseDirection, seconds, strength);
    const float poleHeight = lengthUnits * 0.6f;
    const camera::Vec3 top = footPosition + camera::Vec3{0.0f, 0.0f, poleHeight};

    // Der Mast.
    addLine(lines, footPosition, top, poleColour);

    // Das Tuch: eine Kette von Punkten, die vom Mast wegzeigt und dabei
    // wellt. Die Welle wandert nach hinten und wird zur Spitze hin staerker —
    // so bewegt sich ein Wimpel wirklich, das Ende schlaegt am weitesten aus.
    camera::Vec3 side = camera::cross(dir, {0.0f, 0.0f, 1.0f});
    if (camera::length(side) < 1e-5f) side = camera::cross(dir, {0.0f, 1.0f, 0.0f});
    side = camera::normalise(side);

    constexpr int kSegments = 12;
    camera::Vec3 previousUpper = top;
    camera::Vec3 previousLower = top;
    for (int i = 1; i <= kSegments; ++i) {
        const float t = static_cast<float>(i) / kSegments;
        // Welle: Phase wandert mit der Zeit nach hinten, Ausschlag waechst
        // quadratisch zur Spitze.
        const float wave = std::sin(t * 9.0f - seconds * 6.0f) * t * t * 0.28f *
                           strength;
        const camera::Vec3 along = dir * (lengthUnits * t);
        const camera::Vec3 offset = side * (wave * lengthUnits);

        // Zwei Kanten, die zur Spitze hin zusammenlaufen — ein Wimpel, kein
        // Rechteck.
        const float halfWidth = (1.0f - t) * lengthUnits * 0.11f;
        const camera::Vec3 upper =
            top + along + offset + camera::Vec3{0.0f, 0.0f, halfWidth};
        const camera::Vec3 lower =
            top + along + offset - camera::Vec3{0.0f, 0.0f, halfWidth};

        addLine(lines, previousUpper, upper, clothColour);
        addLine(lines, previousLower, lower, clothColour);
        previousUpper = upper;
        previousLower = lower;
    }
    return lines;
}

float sunLambert(const camera::Vec3& normal, const camera::Vec3& sunDirection,
                 float ambient) {
    const camera::Vec3 toSun = camera::normalise(sunDirection);
    if (camera::length(toSun) < 1e-6f) return 1.0f;

    // Lambert, unten abgeschnitten. Der Grundanteil verhindert schwarze
    // Flaechen: im Spiel gibt es immer Streulicht, und eine schwarze Wand
    // verraet nichts ueber den Effekt davor.
    const float facing = camera::dot(camera::normalise(normal), toSun);
    const float lit = facing > 0.0f ? facing : 0.0f;
    const float clampedAmbient = ambient < 0.0f ? 0.0f : (ambient > 1.0f ? 1.0f
                                                                         : ambient);
    return clampedAmbient + (1.0f - clampedAmbient) * lit;
}

Mesh buildRoomLit(const RoomSize& size, float worldScale, uint32_t wallColour,
                  RoomStyle style, const camera::Vec3& sunDirection, float ambient,
                  bool sunEnabled, bool includeWalls) {
    Mesh mesh;
    if (style == RoomStyle::None) return mesh;
    if (worldScale <= 0.0f) worldScale = 16.0f;

    const float halfX = size.widthFeet * worldScale * 0.5f;
    const float halfY = size.depthFeet * worldScale * 0.5f;
    const float height = size.heightFeet * worldScale;
    const float uvX = size.widthFeet * 0.5f;
    const float uvY = size.depthFeet * 0.5f;

    // Draussen bleibt ein niedriger Sockel statt der Waende: er faengt den
    // Blick am Rand ab, ohne den Effekt einzusperren. Ohne ihn schwebt der
    // Boden im Nichts, und man verliert das Gefuehl fuer die Hoehe.
    const float wallHeight = style == RoomStyle::OpenSky ? worldScale * 0.5f : height;

    auto shadeFor = [&](const camera::Vec3& normal, float baseFactor) {
        const float lit =
            sunEnabled ? sunLambert(normal, sunDirection, ambient) : 1.0f;
        return shade(wallColour, baseFactor * lit);
    };

    const camera::Vec3 f00{-halfX, -halfY, 0.0f};
    const camera::Vec3 f10{ halfX, -halfY, 0.0f};
    const camera::Vec3 f11{ halfX,  halfY, 0.0f};
    const camera::Vec3 f01{-halfX,  halfY, 0.0f};
    const camera::Vec3 c00{-halfX, -halfY, wallHeight};
    const camera::Vec3 c10{ halfX, -halfY, wallHeight};
    const camera::Vec3 c11{ halfX,  halfY, wallHeight};
    const camera::Vec3 c01{-halfX,  halfY, wallHeight};

    // Alle Vorderseiten zeigen nach innen — siehe buildRoom.
    // Der Boden immer. Er ist der Bezug, an dem man Hoehe und Entfernung
    // ablesen kann — ohne ihn schwebt alles.
    addQuad(mesh, f00, f10, f11, f01, shadeFor({0, 0, 1}, 1.00f), uvX);
    if (!includeWalls) return mesh;

    if (style == RoomStyle::Enclosed) {
        addQuad(mesh, c00, c01, c11, c10, shadeFor({0, 0, -1}, 0.55f), uvX);
    }
    addQuad(mesh, f00, c00, c10, f10, shadeFor({0, 1, 0}, 0.86f), uvX * 0.5f);
    addQuad(mesh, f11, c11, c01, f01, shadeFor({0, -1, 0}, 0.86f), uvX * 0.5f);
    addQuad(mesh, f01, c01, c00, f00, shadeFor({1, 0, 0}, 0.86f), uvY * 0.5f);
    addQuad(mesh, f10, c10, c11, f11, shadeFor({-1, 0, 0}, 0.86f), uvY * 0.5f);
    return mesh;
}

Mesh buildSky(float radiusUnits, const camera::Vec3& sunDirection,
              uint32_t horizonColour, uint32_t zenithColour, uint32_t sunColour) {
    Mesh mesh;
    if (radiusUnits <= 0.0f) return mesh;

    // Eine Kuppel aus Laengen- und Breitenkreisen. Von innen betrachtet,
    // deshalb zeigen die Flaechen nach innen wie beim Raum.
    constexpr int kRings = 8;     // vom Horizont zum Zenit
    constexpr int kSegments = 24; // rundherum
    const camera::Vec3 toSun = camera::normalise(sunDirection);

    auto colourAt = [&](const camera::Vec3& direction) {
        // Hoehe ueber dem Horizont mischt Horizont- und Zenitfarbe.
        const float up = direction.z < 0.0f ? 0.0f : direction.z;
        auto channel = [&](int shift) {
            const float h = static_cast<float>((horizonColour >> shift) & 0xFF);
            const float z = static_cast<float>((zenithColour >> shift) & 0xFF);
            return h + (z - h) * up;
        };
        float r = channel(0), g = channel(8), b = channel(16);

        // Um die Sonne herum aufhellen. Der hohe Exponent macht den Hof eng —
        // sonst leuchtet der halbe Himmel, und man sieht die Richtung nicht.
        if (camera::length(toSun) > 1e-6f) {
            const float towards = camera::dot(direction, toSun);
            if (towards > 0.0f) {
                const float halo = std::pow(towards, 24.0f);
                auto sunChannel = [&](int shift) {
                    return static_cast<float>((sunColour >> shift) & 0xFF);
                };
                r += (sunChannel(0) - r) * halo;
                g += (sunChannel(8) - g) * halo;
                b += (sunChannel(16) - b) * halo;
            }
        }
        auto clamp255 = [](float v) {
            const int i = static_cast<int>(v + 0.5f);
            return i < 0 ? 0 : (i > 255 ? 255 : i);
        };
        return rgba(clamp255(r), clamp255(g), clamp255(b), 255);
    };

    constexpr float kPi = 3.14159265358979323846f;
    for (int ring = 0; ring < kRings; ++ring) {
        const float lower = static_cast<float>(ring) / kRings;
        const float upper = static_cast<float>(ring + 1) / kRings;
        // Quadratisch verteilt: am Horizont dichter, weil dort der Verlauf
        // am staerksten ist und Kanten am ehesten auffallen.
        const float lowerAngle = lower * lower * kPi * 0.5f;
        const float upperAngle = upper * upper * kPi * 0.5f;

        for (int seg = 0; seg < kSegments; ++seg) {
            const float a0 = static_cast<float>(seg) / kSegments * 2.0f * kPi;
            const float a1 = static_cast<float>(seg + 1) / kSegments * 2.0f * kPi;

            auto point = [&](float angle, float elevation) {
                return camera::Vec3{std::cos(elevation) * std::cos(angle),
                                    std::cos(elevation) * std::sin(angle),
                                    std::sin(elevation)};
            };
            const camera::Vec3 d00 = point(a0, lowerAngle);
            const camera::Vec3 d10 = point(a1, lowerAngle);
            const camera::Vec3 d11 = point(a1, upperAngle);
            const camera::Vec3 d01 = point(a0, upperAngle);

            // Nach innen gewickelt, damit die Aussortierung sie stehen laesst.
            const auto base = static_cast<uint16_t>(mesh.vertices.size());
            const camera::Vec3 corners[4] = {d00, d01, d11, d10};
            for (int i = 0; i < 4; ++i) {
                Vertex v{};
                const camera::Vec3 position = corners[i] * radiusUnits;
                v.pos[0] = position.x;
                v.pos[1] = position.y;
                v.pos[2] = position.z;
                v.uv[0] = 0.0f;
                v.uv[1] = 0.0f;
                v.colour = colourAt(corners[i]);
                mesh.vertices.push_back(v);
            }
            for (uint16_t offset : {uint16_t{0}, uint16_t{1}, uint16_t{2},
                                    uint16_t{0}, uint16_t{2}, uint16_t{3}}) {
                mesh.indices.push_back(static_cast<uint16_t>(base + offset));
            }
        }
    }
    return mesh;
}

Mesh buildSunDisc(const camera::Vec3& sunDirection, float radiusUnits,
                  float discSize, uint32_t colour) {
    Mesh mesh;
    const camera::Vec3 toSun = camera::normalise(sunDirection);
    if (camera::length(toSun) < 1e-6f || radiusUnits <= 0.0f || discSize <= 0.0f) {
        return mesh;
    }

    // Etwas innerhalb der Kuppel, sonst streiten Sonne und Himmel um dieselben
    // Tiefenwerte und die Scheibe flackert.
    const camera::Vec3 centre = toSun * (radiusUnits * 0.98f);

    camera::Vec3 right = camera::cross(toSun, {0.0f, 0.0f, 1.0f});
    if (camera::length(right) < 1e-5f) right = camera::cross(toSun, {0.0f, 1.0f, 0.0f});
    right = camera::normalise(right) * discSize;
    const camera::Vec3 up = camera::normalise(camera::cross(right, toSun)) * discSize;

    const auto base = static_cast<uint16_t>(mesh.vertices.size());
    const camera::Vec3 corners[4] = {centre - right - up, centre + right - up,
                                     centre + right + up, centre - right + up};
    for (int i = 0; i < 4; ++i) {
        Vertex v{};
        v.pos[0] = corners[i].x;
        v.pos[1] = corners[i].y;
        v.pos[2] = corners[i].z;
        v.colour = colour;
        mesh.vertices.push_back(v);
    }
    for (uint16_t offset : {uint16_t{0}, uint16_t{2}, uint16_t{1},
                            uint16_t{0}, uint16_t{3}, uint16_t{2}}) {
        mesh.indices.push_back(static_cast<uint16_t>(base + offset));
    }
    return mesh;
}

namespace {

// Ein einfacher, wiederholbarer Zufall aus zwei Koordinaten. Muss aus x und y
// allein folgen, sonst laesst sich das Muster nicht nahtlos kacheln.
float valueNoise(int x, int y, unsigned seed) {
    unsigned h = static_cast<unsigned>(x) * 374761393u +
                 static_cast<unsigned>(y) * 668265263u + seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

}  // namespace

uint32_t wallColourOf(WallTexture kind) {
    switch (kind) {
        case WallTexture::Stucco: return rgba(144, 140, 115);
        case WallTexture::Dirt:   return rgba(128, 84, 55);
        case WallTexture::Brick:  return rgba(129, 96, 73);
        default: break;
    }
    return rgba(255, 255, 255);
}

std::vector<unsigned char> buildWallTexture(WallTexture kind, int size,
                                            uint32_t baseColour) {
    std::vector<unsigned char> out;
    if (kind == WallTexture::None || size <= 0 || size > 1024) return out;

    out.assign(static_cast<size_t>(size) * size * 4, 255);
    const float baseR = static_cast<float>(baseColour & 0xFF);
    const float baseG = static_cast<float>((baseColour >> 8) & 0xFF);
    const float baseB = static_cast<float>((baseColour >> 16) & 0xFF);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float shade = 1.0f;

            switch (kind) {
                // Helligkeit und Kontrast sind an Ravens Dateien angeglichen.
                // Vorher war Putz mit einer Streuung von 9.7 fast glatt; im
                // Original sind es 21.7 bei einer mittleren Helligkeit von
                // 138 — also deutlich rauer und dunkler, als wir es hatten.
                case WallTexture::Brick: {
                    // Acht Reihen, jede zweite um eine halbe Ziegellaenge
                    // versetzt. Der Versatz ist das, was es wie Mauerwerk
                    // aussehen laesst — ohne ihn sieht es aus wie Fliesen.
                    const int rowHeight = size / 8;
                    const int row = y / rowHeight;
                    const int brickWidth = size / 4;
                    const int offset = (row & 1) ? brickWidth / 2 : 0;
                    const int inRow = (x + offset) % brickWidth;
                    const int inColumn = y % rowHeight;

                    const bool mortar = inRow < 2 || inColumn < 2;
                    // Dunkler als vorher: Ravens Ziegel liegen bei einer
                    // mittleren Helligkeit von 103, unsere kamen auf 236.
                    shade = mortar ? 0.48f : 0.84f + valueNoise(x / 3, y / 3, 1) * 0.42f;
                    break;
                }
                case WallTexture::Dirt: {
                    // Grobes Rauschen in zwei Groessen uebereinander. Eine
                    // Groesse allein sieht aus wie Fernsehschnee.
                    const float coarse = valueNoise(x / 8, y / 8, 2);
                    const float fine = valueNoise(x / 2, y / 2, 3);
                    shade = 0.74f + coarse * 0.36f + fine * 0.18f;
                    break;
                }
                case WallTexture::Stucco: {
                    // Feines, gleichmaessiges Korn mit wenig Kontrast.
                    // Deutlich rauer als der erste Entwurf. "Wenig Kontrast"
                    // stand hier als Absicht, gemessen an Raven war es
                    // schlicht zu wenig: Streuung 9.7 gegen 21.7.
                    const float grain = valueNoise(x, y, 4);
                    const float soft = valueNoise(x / 4, y / 4, 5);
                    const float patch = valueNoise(x / 16, y / 16, 6);
                    shade = 0.66f + grain * 0.32f + soft * 0.25f + patch * 0.17f;
                    break;
                }
                default: break;
            }

            auto clamp255 = [](float v) {
                const int i = static_cast<int>(v + 0.5f);
                return static_cast<unsigned char>(i < 0 ? 0 : (i > 255 ? 255 : i));
            };
            const size_t at = (static_cast<size_t>(y) * size + x) * 4;
            out[at + 0] = clamp255(baseR * shade);
            out[at + 1] = clamp255(baseG * shade);
            out[at + 2] = clamp255(baseB * shade);
            out[at + 3] = 255;
        }
    }
    return out;
}

}  // namespace efx::scene
