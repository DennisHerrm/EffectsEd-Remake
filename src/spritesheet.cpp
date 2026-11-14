// Sprite-Sheet -> Effekt. Begruendung und Engine-Grenzen in spritesheet.h.
#include "efx/spritesheet.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "efx/io.h"

namespace efx::spritesheet {

namespace {

std::string number(float value) {
    // Mit Punkt, wie die Engine liest; hoechstens vier Stellen ("12.5", "20").
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.4g", static_cast<double>(value));
    std::string text = buffer;
    for (char& c : text) {
        if (c == ',') c = '.';
    }
    return text;
}

std::string twoDigits(int i) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d", i);
    return buffer;
}

}  // namespace

std::vector<Cell> gridCells(int sheetWidth, int sheetHeight, const Layout& layout, Error* error) {
    if (error) *error = Error::None;
    std::vector<Cell> out;
    if (layout.columns < 1 || layout.rows < 1 || layout.margin < 0 || layout.spacing < 0 ||
        layout.columns > 256 || layout.rows > 256) {
        if (error) *error = Error::BadGrid;
        return out;
    }
    const int usableW = sheetWidth - 2 * layout.margin - (layout.columns - 1) * layout.spacing;
    const int usableH = sheetHeight - 2 * layout.margin - (layout.rows - 1) * layout.spacing;
    const int cellW = usableW / layout.columns;
    const int cellH = usableH / layout.rows;
    if (cellW < 1 || cellH < 1) {
        if (error) *error = Error::BadGrid;
        return out;
    }
    const int total = layout.columns * layout.rows;
    out.reserve(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i) {
        const int column = layout.columnsFirst ? i / layout.rows : i % layout.columns;
        const int row = layout.columnsFirst ? i % layout.rows : i / layout.columns;
        Cell cell;
        cell.x = layout.margin + column * (cellW + layout.spacing);
        cell.y = layout.margin + row * (cellH + layout.spacing);
        cell.w = cellW;
        cell.h = cellH;
        out.push_back(cell);
    }
    return out;
}

std::vector<int> pickFrames(int available, int maximum) {
    std::vector<int> out;
    if (available <= 0 || maximum <= 0) return out;
    if (available <= maximum) {
        for (int i = 0; i < available; ++i) out.push_back(i);
        return out;
    }
    if (maximum == 1) return {0};
    for (int i = 0; i < maximum; ++i) {
        const double at = static_cast<double>(i) * (available - 1) / (maximum - 1);
        out.push_back(static_cast<int>(std::lround(at)));
    }
    return out;
}

int nearestPowerOfTwo(int side) {
    side = std::clamp(side, 16, 1024);
    int lower = 16;
    while (lower * 2 <= side) lower *= 2;
    const int upper = std::min(lower * 2, 1024);
    return (side - lower <= upper - side) ? lower : upper;
}

std::vector<unsigned char> crop(const image::Image& sheet, const Cell& cell) {
    std::vector<unsigned char> out(static_cast<size_t>(cell.w) * cell.h * 4, 0);
    for (int y = 0; y < cell.h; ++y) {
        const int sy = cell.y + y;
        if (sy < 0 || sy >= sheet.height) continue;
        for (int x = 0; x < cell.w; ++x) {
            const int sx = cell.x + x;
            if (sx < 0 || sx >= sheet.width) continue;
            const size_t from = (static_cast<size_t>(sy) * sheet.width + sx) * 4;
            const size_t to = (static_cast<size_t>(y) * cell.w + x) * 4;
            for (int k = 0; k < 4; ++k) out[to + k] = sheet.rgba[from + k];
        }
    }
    return out;
}

std::vector<unsigned char> padToSquare(const std::vector<unsigned char>& rgba, int w, int h, int* side) {
    const int s = std::max(w, h);
    if (side) *side = s;
    if (w == h) return rgba;
    std::vector<unsigned char> out(static_cast<size_t>(s) * s * 4, 0);
    const int ox = (s - w) / 2;
    const int oy = (s - h) / 2;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t from = (static_cast<size_t>(y) * w + x) * 4;
            const size_t to = (static_cast<size_t>(y + oy) * s + (x + ox)) * 4;
            for (int k = 0; k < 4; ++k) out[to + k] = rgba[from + k];
        }
    }
    return out;
}

std::vector<unsigned char> resize(const std::vector<unsigned char>& rgba, int w, int h, int newW, int newH) {
    if (w == newW && h == newH) return rgba;
    // Vormultipliziert rechnen: sonst zieht die (oft schwarze oder weisse)
    // Farbe durchsichtiger Stellen beim Mitteln einen Rand ins Bild.
    std::vector<float> pre(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < pre.size(); i += 4) {
        const float a = rgba[i + 3] / 255.0f;
        pre[i + 0] = rgba[i + 0] * a;
        pre[i + 1] = rgba[i + 1] * a;
        pre[i + 2] = rgba[i + 2] * a;
        pre[i + 3] = static_cast<float>(rgba[i + 3]);
    }
    std::vector<float> acc(static_cast<size_t>(newW) * newH * 4, 0.0f);
    const bool shrink = newW <= w && newH <= h;
    if (shrink) {
        // Flaechenmittel: jedes Zielpixel bekommt die Quellpixel, die es
        // ueberdeckt, mit ihrem Flaechenanteil.
        const double fx = static_cast<double>(w) / newW;
        const double fy = static_cast<double>(h) / newH;
        for (int oy = 0; oy < newH; ++oy) {
            const double y0 = oy * fy, y1 = (oy + 1) * fy;
            for (int ox = 0; ox < newW; ++ox) {
                const double x0 = ox * fx, x1 = (ox + 1) * fx;
                double sum[4] = {0, 0, 0, 0};
                double area = 0.0;
                for (int sy = static_cast<int>(y0); sy < std::min(h, static_cast<int>(std::ceil(y1))); ++sy) {
                    const double wy = std::min(y1, sy + 1.0) - std::max(y0, static_cast<double>(sy));
                    if (wy <= 0) continue;
                    for (int sx = static_cast<int>(x0); sx < std::min(w, static_cast<int>(std::ceil(x1))); ++sx) {
                        const double wx = std::min(x1, sx + 1.0) - std::max(x0, static_cast<double>(sx));
                        if (wx <= 0) continue;
                        const double weight = wx * wy;
                        const size_t i = (static_cast<size_t>(sy) * w + sx) * 4;
                        for (int k = 0; k < 4; ++k) sum[k] += pre[i + k] * weight;
                        area += weight;
                    }
                }
                const size_t o = (static_cast<size_t>(oy) * newW + ox) * 4;
                for (int k = 0; k < 4; ++k) acc[o + k] = area > 0 ? static_cast<float>(sum[k] / area) : 0.0f;
            }
        }
    } else {
        // Vergroessern: bilinear, Pixelmitten aufeinander abgebildet.
        for (int oy = 0; oy < newH; ++oy) {
            const double sy = std::clamp((oy + 0.5) * h / newH - 0.5, 0.0, h - 1.0);
            const int y0 = static_cast<int>(sy);
            const int y1 = std::min(y0 + 1, h - 1);
            const double ty = sy - y0;
            for (int ox = 0; ox < newW; ++ox) {
                const double sx = std::clamp((ox + 0.5) * w / newW - 0.5, 0.0, w - 1.0);
                const int x0 = static_cast<int>(sx);
                const int x1 = std::min(x0 + 1, w - 1);
                const double tx = sx - x0;
                const size_t a = (static_cast<size_t>(y0) * w + x0) * 4;
                const size_t b = (static_cast<size_t>(y0) * w + x1) * 4;
                const size_t c = (static_cast<size_t>(y1) * w + x0) * 4;
                const size_t d = (static_cast<size_t>(y1) * w + x1) * 4;
                const size_t o = (static_cast<size_t>(oy) * newW + ox) * 4;
                for (int k = 0; k < 4; ++k) {
                    const double top = pre[a + k] * (1 - tx) + pre[b + k] * tx;
                    const double bottom = pre[c + k] * (1 - tx) + pre[d + k] * tx;
                    acc[o + k] = static_cast<float>(top * (1 - ty) + bottom * ty);
                }
            }
        }
    }
    std::vector<unsigned char> out(acc.size());
    for (size_t i = 0; i < acc.size(); i += 4) {
        const float alpha = acc[i + 3];
        const float a = alpha / 255.0f;
        for (int k = 0; k < 3; ++k) {
            const float value = a > 0.0f ? acc[i + k] / a : 0.0f;
            out[i + k] = static_cast<unsigned char>(std::clamp(std::lround(value), 0L, 255L));
        }
        out[i + 3] = static_cast<unsigned char>(std::clamp(std::lround(alpha), 0L, 255L));
    }
    return out;
}

void alphaFromBrightness(std::vector<unsigned char>& rgba) {
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        // Bisheriges Alpha mitnehmen: ein Bild, das schon Alpha hat, wird
        // nicht durchsichtiger als vorher behauptet.
        const int brightest = std::max({rgba[i], rgba[i + 1], rgba[i + 2]});
        const int alpha = brightest * rgba[i + 3] / 255;
        if (brightest > 0) {
            for (int k = 0; k < 3; ++k) rgba[i + k] = static_cast<unsigned char>(rgba[i + k] * 255 / brightest);
        }
        rgba[i + 3] = static_cast<unsigned char>(alpha);
    }
}

bool hasTransparency(const image::Image& sheet) {
    for (size_t i = 3; i < sheet.rgba.size(); i += 4) {
        if (sheet.rgba[i] < 250) return true;
    }
    return false;
}

bool cellEmpty(const std::vector<unsigned char>& rgba, Blend blend) {
    // "Leer" heisst wirklich nichts zu sehen; ein paar Funken sind Inhalt.
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const int a = rgba[i + 3];
        if (blend == Blend::Alpha) {
            if (a >= 8) return false;
        } else {
            const int brightest = std::max({rgba[i], rgba[i + 1], rgba[i + 2]});
            if (brightest * a / 255 >= 8) return false;
        }
    }
    return true;
}

std::string sanitizeName(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
        const bool keep = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        if (keep) {
            out.push_back(static_cast<char>(c));
        } else if (!out.empty() && out.back() != '_') {
            out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.size() > 48) out.resize(48);
    return out;
}

std::string shaderText(const std::string& shaderName, const std::vector<std::string>& frames,
                       const std::string& endFrame, const Options& options, float fps) {
    const std::string keyword = options.loop ? "animMap" : "oneshotanimMap";
    const std::string blend = options.blend == Blend::Additive ? "GL_ONE GL_ONE"
                                                               : "GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA";
    const auto list = [&](size_t from, bool wrap) {
        std::string text;
        for (size_t i = from; i < frames.size(); ++i) text += " " + frames[i];
        if (wrap) {
            for (size_t i = 0; i < from && i < frames.size(); ++i) text += " " + frames[i];
        } else if (!endFrame.empty()) {
            text += " " + endFrame;
        }
        return text;
    };
    std::string s;
    s += "// Created by EffectsEd-Remake (sprite sheet import): " + std::to_string(frames.size()) +
         " frames, " + number(fps) + " per second\n";
    s += shaderName + "\n{\n\tcull disable\n";
    if (!options.crossfade || frames.size() < 2) {
        s += "\t{\n\t\t" + keyword + " " + number(fps) + list(0, false) + "\n";
        s += "\t\tblendFunc " + blend + "\n\t\trgbGen vertex\n";
        if (options.blend == Blend::Alpha) s += "\t\talphaGen vertex\n";
        s += "\t}\n";
    } else {
        // Wie gfx/exp/explosion1: Stufe 1 blendet Bild i aus, Stufe 2 (ein
        // Bild voraus) blendet Bild i+1 ein — im Takt der Folge.
        const std::string f = number(fps);
        const char* waves[2] = {"inversesawtooth", "sawtooth"};
        for (int stage = 0; stage < 2; ++stage) {
            s += "\t{\n\t\t" + keyword + " " + f + list(static_cast<size_t>(stage), options.loop) + "\n";
            s += "\t\tblendFunc " + blend + "\n";
            if (options.blend == Blend::Additive) {
                s += std::string("\t\trgbGen wave ") + waves[stage] + " 0 1 0 " + f + "\n";
            } else {
                s += "\t\trgbGen vertex\n";
                s += std::string("\t\talphaGen wave ") + waves[stage] + " 0 1 0 " + f + "\n";
            }
            s += "\t}\n";
        }
    }
    s += "}\n";
    return s;
}

Package build(const image::Image& sheet, const Layout& layout, const Options& options,
              const std::string& sourceName) {
    Package p;
    if (!sheet.ok || sheet.width <= 0 || sheet.height <= 0 ||
        sheet.rgba.size() < static_cast<size_t>(sheet.width) * sheet.height * 4) {
        p.error = Error::NoImage;
        return p;
    }
    if (!(options.fps > 0.0f) || options.fps > 1000.0f) {
        p.error = Error::BadFps;
        return p;
    }
    const std::string name = sanitizeName(options.name);
    const std::string folder = sanitizeName(options.folder);
    if (name.empty()) {
        p.error = Error::BadName;
        return p;
    }

    Error gridError = Error::None;
    const auto cells = gridCells(sheet.width, sheet.height, layout, &gridError);
    if (gridError != Error::None) {
        p.error = gridError;
        return p;
    }
    const int total = static_cast<int>(cells.size());
    const int first = std::max(0, layout.first);
    const int last = layout.count > 0 ? std::min(total, first + layout.count) : total;
    if (first >= last) {
        p.error = Error::NoFrames;
        return p;
    }
    p.cellW = cells.front().w;
    p.cellH = cells.front().h;

    std::vector<std::vector<unsigned char>> source;
    for (int i = first; i < last; ++i) source.push_back(crop(sheet, cells[static_cast<size_t>(i)]));
    if (layout.skipEmptyAtEnd) {
        while (!source.empty() && cellEmpty(source.back(), options.blend)) source.pop_back();
    }
    if (source.empty()) {
        p.error = Error::NoFrames;
        return p;
    }
    p.sourceFrames = static_cast<int>(source.size());

    // Mit Ueberblenden traegt die erste Stufe ein Endbild mehr.
    const bool endFrame = options.crossfade && !options.loop && source.size() > 1;
    const int maximum = endFrame ? kMaxFrames - 1 : kMaxFrames;
    const auto picked = pickFrames(p.sourceFrames, maximum);
    p.frames = static_cast<int>(picked.size());
    p.thinned = p.frames < p.sourceFrames;
    // Gleiche Dauer: weniger Bilder, entsprechend langsamer.
    p.fps = options.fps * static_cast<float>(p.frames) / static_cast<float>(p.sourceFrames);
    p.frameSize = options.frameSize > 0 ? options.frameSize
                                        : nearestPowerOfTwo(std::max(p.cellW, p.cellH));
    p.noTransparency = options.blend == Blend::Alpha && !options.alphaFromBrightness &&
                       !hasTransparency(sheet);

    const std::string base = folder.empty() ? name : folder + "/" + name;
    const std::string imageDir = "gfx/effects/" + base + "/";
    p.shaderName = "gfx/effects/" + base;
    p.shaderPath = "shaders/" + (folder.empty() ? name : folder + "_" + name) + ".shader";
    p.effectPath = "effects/" + base + ".efx";

    std::vector<std::string> frameNames;
    for (int k = 0; k < p.frames; ++k) {
        std::vector<unsigned char> rgba = source[static_cast<size_t>(picked[static_cast<size_t>(k)])];
        if (options.blend == Blend::Alpha && options.alphaFromBrightness) {
            alphaFromBrightness(rgba);
        } else if (options.blend == Blend::Additive) {
            // Additiv zaehlt nur die Farbe: durchsichtige Stellen muessen
            // SCHWARZ sein, sonst leuchtet z. B. ein weisser, voll
            // durchsichtiger Hintergrund als Quadrat.
            for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
                for (int c = 0; c < 3; ++c) rgba[i + c] = static_cast<unsigned char>(rgba[i + c] * rgba[i + 3] / 255);
                rgba[i + 3] = 255;
            }
        }
        int side = 0;
        const auto square = padToSquare(rgba, p.cellW, p.cellH, &side);
        const auto scaled = resize(square, side, side, p.frameSize, p.frameSize);
        File file;
        file.path = imageDir + name + "_" + twoDigits(k) + ".tga";
        file.data = image::encodeTga(scaled.data(), p.frameSize, p.frameSize);
        frameNames.push_back(file.path);
        p.files.push_back(std::move(file));
    }
    std::string endName;
    if (endFrame) {
        // Schwarz und durchsichtig: additiv wie alphagemischt unsichtbar.
        const std::vector<unsigned char> blank(8 * 8 * 4, 0);
        File file;
        file.path = imageDir + name + "_end.tga";
        file.data = image::encodeTga(blank.data(), 8, 8);
        endName = file.path;
        p.files.push_back(std::move(file));
    }

    std::string shader = shaderText(p.shaderName, frameNames, endName, options, p.fps);
    if (!sourceName.empty()) {
        // Woher es kam, als Kommentar — nur ASCII. In der .efx stuende er
        // falsch: der Leser meldet Kommentare dort beim Oeffnen.
        std::string ascii;
        for (unsigned char c : sourceName) ascii.push_back(c >= 32 && c < 127 ? static_cast<char>(c) : '_');
        shader = "// Sprite sheet: " + ascii + "\n" + shader;
    }
    p.files.push_back(File{p.shaderPath, std::vector<unsigned char>(shader.begin(), shader.end())});

    // Der Effekt: ein Particle, so lang wie die Folge (bei Schleife vier
    // Durchlaeufe), mit setShaderTime wie Ravens Explosionen.
    const float duration = static_cast<float>(p.frames) / p.fps;
    const float lifeMs = std::max(1.0f, std::round(duration * 1000.0f * (options.loop ? 4.0f : 1.0f)));
    const float size = std::max(0.1f, options.size);
    std::string e = "Particle\n{\n";
    e += "\tname\t" + name + "\n";
    e += "\tlife\t" + number(lifeMs) + "\n";
    const bool alphaBlend = options.blend == Blend::Alpha;
    e += std::string("\tflags\t") + (alphaBlend ? "useAlpha setShaderTime" : "setShaderTime") + "\n";
    switch (options.preset) {
        case Preset::Explosion:
            e += "\trotation\t0 360\n";
            e += "\tsize\n\t{\n\t\tstart\t" + number(size) + "\n\t\tend\t" + number(size * 1.3f) +
                 "\n\t\tflags\tlinear\n\t}\n";
            break;
        case Preset::Smoke:
            e += "\tvelocity\t0 0 10 0 0 20\n";
            e += "\trotation\t0 360\n";
            e += "\trotationDelta\t-15 15\n";
            e += "\tsize\n\t{\n\t\tstart\t" + number(size) + "\n\t\tend\t" + number(size * 2.0f) +
                 "\n\t\tflags\tlinear\n\t}\n";
            e += "\talpha\n\t{\n\t\tstart\t1\n\t\tend\t0\n\t\tflags\tlinear\n\t}\n";
            break;
        case Preset::MuzzleFlash:
            e += "\trotation\t0 360\n";
            e += "\tsize\n\t{\n\t\tstart\t" + number(size * 0.5f) + "\n\t}\n";
            break;
        case Preset::Energy:
            e += "\tsize\n\t{\n\t\tstart\t" + number(size) + "\n\t\tend\t" + number(size * 1.1f) +
                 "\n\t\tflags\tlinear\n\t}\n";
            break;
    }
    e += "\tshaders\n\t[\n\t\t" + p.shaderName + "\n\t]\n}\n";
    ReadResult parsed = read(e);
    if (parsed.hasErrors() || parsed.effect.primitives.size() != 1) {
        p.error = Error::NoFrames;  // kann nicht passieren; dann lieber nichts schreiben
        return p;
    }
    p.effect = std::move(parsed.effect);
    WriteOptions writeOptions;
    p.effectText = write(p.effect, writeOptions);
    p.files.push_back(File{p.effectPath, std::vector<unsigned char>(p.effectText.begin(), p.effectText.end())});
    return p;
}

std::vector<unsigned char> zipStored(const std::vector<File>& files) {
    uint32_t table[256];
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[n] = c;
    }
    const auto crc32 = [&](const std::vector<unsigned char>& d) {
        uint32_t c = 0xFFFFFFFFu;
        for (unsigned char b : d) c = table[(c ^ b) & 0xFFu] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    };
    std::vector<unsigned char> out, central;
    const auto u16 = [](std::vector<unsigned char>& v, uint32_t x) {
        v.push_back(static_cast<unsigned char>(x & 0xFF));
        v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
    };
    const auto u32 = [&](std::vector<unsigned char>& v, uint32_t x) {
        u16(v, x & 0xFFFF);
        u16(v, x >> 16);
    };
    // Datum 1.1.2026 im DOS-Format, damit Packprogramme kein 1980 zeigen.
    const uint32_t dosTime = 0;
    const uint32_t dosDate = ((2026 - 1980) << 9) | (1 << 5) | 1;
    for (const File& f : files) {
        const uint32_t crc = crc32(f.data);
        const auto offset = static_cast<uint32_t>(out.size());
        const auto size = static_cast<uint32_t>(f.data.size());
        const auto nameLength = static_cast<uint32_t>(f.path.size());
        u32(out, 0x04034b50u); u16(out, 20); u16(out, 0); u16(out, 0);
        u16(out, dosTime); u16(out, dosDate);
        u32(out, crc); u32(out, size); u32(out, size);
        u16(out, nameLength); u16(out, 0);
        out.insert(out.end(), f.path.begin(), f.path.end());
        out.insert(out.end(), f.data.begin(), f.data.end());
        u32(central, 0x02014b50u); u16(central, 20); u16(central, 20); u16(central, 0); u16(central, 0);
        u16(central, dosTime); u16(central, dosDate);
        u32(central, crc); u32(central, size); u32(central, size);
        u16(central, nameLength); u16(central, 0); u16(central, 0);
        u16(central, 0); u16(central, 0); u32(central, 0); u32(central, offset);
        central.insert(central.end(), f.path.begin(), f.path.end());
    }
    const auto centralAt = static_cast<uint32_t>(out.size());
    const auto centralSize = static_cast<uint32_t>(central.size());
    out.insert(out.end(), central.begin(), central.end());
    u32(out, 0x06054b50u); u16(out, 0); u16(out, 0);
    u16(out, static_cast<uint32_t>(files.size())); u16(out, static_cast<uint32_t>(files.size()));
    u32(out, centralSize); u32(out, centralAt); u16(out, 0);
    return out;
}

}  // namespace efx::spritesheet
