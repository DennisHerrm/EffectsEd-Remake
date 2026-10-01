#include "efx/md3.h"

#include <cctype>
#include <cmath>
#include <cstring>

namespace efx::md3 {
namespace {

constexpr float kPi = 3.14159265358979323846f;

// Groessen der Strukturen aus qfiles.h, in Bytes.
constexpr size_t kHeaderSize = 108;     // md3Header_t
constexpr size_t kFrameSize = 56;       // md3Frame_t
constexpr size_t kSurfaceSize = 108;    // md3Surface_t
constexpr size_t kShaderSize = 68;      // md3Shader_t
constexpr size_t kTriangleSize = 12;    // md3Triangle_t
constexpr size_t kStSize = 8;           // md3St_t
constexpr size_t kXyzNormalSize = 8;    // md3XyzNormal_t
constexpr size_t kNameSize = 64;        // MAX_QPATH

// Liest im Puffer, aber nur, was wirklich darin liegt. Jeder Zugriff geht
// hier hindurch — ein einzelner ungeprüfter Versatz genuegt sonst, und eine
// fremde Datei liest hinter dem Puffer weiter.
class Reader {
public:
    Reader(const unsigned char* data, size_t size) : data_(data), size_(size) {}

    // Liegen `count` Eintraege zu je `each` Bytes ab `offset` im Puffer?
    // Mit Ueberlaufschutz: offset + count*each darf nicht umlaufen.
    bool fits(size_t offset, size_t count, size_t each) const {
        if (offset > size_) return false;
        if (each != 0 && count > (size_ - offset) / each) return false;
        return true;
    }

    int32_t i32(size_t at) const {
        uint32_t v = 0;
        std::memcpy(&v, data_ + at, 4);
        return static_cast<int32_t>(v);
    }
    int16_t i16(size_t at) const {
        uint16_t v = 0;
        std::memcpy(&v, data_ + at, 2);
        return static_cast<int16_t>(v);
    }
    float f32(size_t at) const {
        float v = 0.0f;
        std::memcpy(&v, data_ + at, 4);
        // Ein NaN oder Unendlich aus einer kaputten Datei soll nicht bis in
        // die Geometrie wandern.
        return std::isfinite(v) ? v : 0.0f;
    }
    // Ein Name mit fester Laenge, nicht zwingend nullterminiert.
    std::string name(size_t at, size_t length) const {
        const char* text = reinterpret_cast<const char*>(data_ + at);
        size_t n = 0;
        while (n < length && text[n] != '\0') ++n;
        return std::string(text, n);
    }

private:
    const unsigned char* data_;
    size_t size_;
};

Model fail(const std::string& why) {
    Model out;
    out.error = why;
    return out;
}

// Ein nicht-negativer Zaehler oder Versatz aus der Datei.
bool counted(int32_t value, int limit) { return value >= 0 && value <= limit; }

}  // namespace

Model parse(const unsigned char* data, size_t size) {
    if (data == nullptr || size < kHeaderSize) return fail("too short for an md3 header");
    const Reader in(data, size);

    if (static_cast<uint32_t>(in.i32(0)) != kIdent) return fail("not an md3 (ident is not IDP3)");
    // R_LoadMD3: "has wrong version (%i should be %i)".
    const int32_t version = in.i32(4);
    if (version != kVersion) {
        return fail("wrong md3 version " + std::to_string(version));
    }

    Model out;
    out.name = in.name(8, kNameSize);
    // flags bei 72 wird nicht gebraucht.
    const int32_t numFrames = in.i32(76);
    const int32_t numTags = in.i32(80);
    const int32_t numSurfaces = in.i32(84);
    const int32_t ofsFrames = in.i32(92);
    const int32_t ofsSurfaces = in.i32(100);

    // R_LoadMD3: "has no frames". Ohne Bild 0 gibt es nichts zu zeichnen.
    if (numFrames < 1 || numFrames > kMaxFrames) return fail("bad frame count");
    if (!counted(numTags, 1 << 16)) return fail("bad tag count");
    if (!counted(numSurfaces, kMaxSurfaces)) return fail("bad surface count");
    if (ofsFrames < 0 || !in.fits(static_cast<size_t>(ofsFrames), 1, kFrameSize)) {
        return fail("frames outside the file");
    }
    out.frames = numFrames;

    // Bild 0: Huelle, Ursprung, Radius.
    {
        const size_t f = static_cast<size_t>(ofsFrames);
        for (int k = 0; k < 3; ++k) {
            out.mins[k] = in.f32(f + 4 * k);
            out.maxs[k] = in.f32(f + 12 + 4 * k);
        }
        out.radius = in.f32(f + 36);
    }

    // Die Flaechen liegen hintereinander; jede sagt in ofsEnd, wo die naechste
    // beginnt.
    if (ofsSurfaces < 0) return fail("surfaces outside the file");
    size_t at = static_cast<size_t>(ofsSurfaces);
    for (int32_t s = 0; s < numSurfaces; ++s) {
        if (!in.fits(at, 1, kSurfaceSize)) return fail("surface header outside the file");
        // Auch die Flaeche traegt die Kennung — R_LoadMD3 prueft sie nicht,
        // aber eine Flaeche ohne sie ist mit Sicherheit verschoben.
        if (static_cast<uint32_t>(in.i32(at)) != kIdent) return fail("surface without IDP3");

        Surface surface;
        surface.name = in.name(at + 4, kNameSize);
        const size_t head = at + 4 + kNameSize;  // hinter Kennung und Name
        // head+0 flags
        const int32_t surfFrames = in.i32(head + 4);
        const int32_t numShaders = in.i32(head + 8);
        const int32_t numVerts = in.i32(head + 12);
        const int32_t numTriangles = in.i32(head + 16);
        const int32_t ofsTriangles = in.i32(head + 20);
        const int32_t ofsShaders = in.i32(head + 24);
        const int32_t ofsSt = in.i32(head + 28);
        const int32_t ofsXyz = in.i32(head + 32);
        const int32_t ofsEnd = in.i32(head + 36);

        if (surfFrames < 1 || surfFrames > kMaxFrames) return fail("surface frame count");
        if (!counted(numShaders, kMaxShaders)) return fail("surface shader count");
        if (!counted(numVerts, kMaxVerts)) return fail("surface vertex count");
        if (!counted(numTriangles, kMaxTriangles)) return fail("surface triangle count");
        if (ofsTriangles < 0 || ofsShaders < 0 || ofsSt < 0 || ofsXyz < 0 ||
            ofsEnd < static_cast<int32_t>(kSurfaceSize)) {
            return fail("negative surface offset");
        }
        const auto sub = [&](int32_t offset) { return at + static_cast<size_t>(offset); };
        if (!in.fits(at, 1, static_cast<size_t>(ofsEnd))) return fail("surface end outside the file");
        if (!in.fits(sub(ofsShaders), static_cast<size_t>(numShaders), kShaderSize) ||
            !in.fits(sub(ofsTriangles), static_cast<size_t>(numTriangles), kTriangleSize) ||
            !in.fits(sub(ofsSt), static_cast<size_t>(numVerts), kStSize) ||
            // Nur Bild 0 wird gelesen — aber es muss ganz da sein.
            !in.fits(sub(ofsXyz), static_cast<size_t>(numVerts), kXyzNormalSize)) {
            return fail("surface data outside the file");
        }

        for (int32_t k = 0; k < numShaders; ++k) {
            surface.shaders.push_back(
                in.name(sub(ofsShaders) + static_cast<size_t>(k) * kShaderSize, kNameSize));
        }

        surface.vertices.resize(static_cast<size_t>(numVerts));
        for (int32_t v = 0; v < numVerts; ++v) {
            Vertex& vertex = surface.vertices[static_cast<size_t>(v)];
            const size_t xyz = sub(ofsXyz) + static_cast<size_t>(v) * kXyzNormalSize;
            for (int k = 0; k < 3; ++k) {
                vertex.pos[k] = static_cast<float>(in.i16(xyz + 2 * k)) * kXyzScale;
            }
            // Die Normale wie in LerpMeshVertexes (tr_surface.cpp):
            //
            //     lat = ( normal >> 8 ) & 0xff;   lng = ( normal & 0xff );
            //     outNormal[0] = cos(lat) * sin(lng);
            //     outNormal[1] = sin(lat) * sin(lng);
            //     outNormal[2] = cos(lng);
            //
            // mit 256 Schritten je Vollkreis (die Engine liest sinTable mit
            // FUNCTABLE_SIZE/256 vervielfachten Nummern).
            const int packed = static_cast<uint16_t>(in.i16(xyz + 6));
            const float lat = static_cast<float>((packed >> 8) & 0xFF) * (2.0f * kPi / 256.0f);
            const float lng = static_cast<float>(packed & 0xFF) * (2.0f * kPi / 256.0f);
            vertex.normal[0] = std::cos(lat) * std::sin(lng);
            vertex.normal[1] = std::sin(lat) * std::sin(lng);
            vertex.normal[2] = std::cos(lng);

            const size_t st = sub(ofsSt) + static_cast<size_t>(v) * kStSize;
            vertex.st[0] = in.f32(st);
            vertex.st[1] = in.f32(st + 4);
        }

        surface.indices.reserve(static_cast<size_t>(numTriangles) * 3);
        for (int32_t t = 0; t < numTriangles; ++t) {
            const size_t tri = sub(ofsTriangles) + static_cast<size_t>(t) * kTriangleSize;
            for (int k = 0; k < 3; ++k) {
                const int32_t index = in.i32(tri + 4 * k);
                // Eine Nummer ausserhalb der Eckpunkte zeigte im Zeichenpuffer
                // auf fremde Daten — abweisen statt klemmen.
                if (index < 0 || index >= numVerts) return fail("triangle index out of range");
                surface.indices.push_back(static_cast<uint16_t>(index));
            }
        }

        out.surfaces.push_back(std::move(surface));
        at += static_cast<size_t>(ofsEnd);
    }

    out.ok = true;
    return out;
}

std::string shaderName(const std::string& fileName) {
    std::string name = fileName;
    for (char& c : name) {
        c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    // COM_StripExtension: nur ein Punkt hinter dem letzten Schraegstrich.
    const size_t dot = name.find_last_of('.');
    const size_t slash = name.find_last_of('/');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        name.erase(dot);
    }
    return name;
}

}  // namespace efx::md3
