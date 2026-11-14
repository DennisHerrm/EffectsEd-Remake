// Effects > Import Sprite Sheet...
//
// Ein Bild mit den Bildern einer Animation im Raster wird ein Effekt: die
// Einzelbilder als TGA, ein Shader mit oneshotanimMap (wie Ravens
// gfx/exp/explosion1) und ein Particle-Segment mit setShaderTime. Der Kern
// (Zerlegen, Umrechnen, Shader, Effekt, pk3) steckt in src/spritesheet.cpp
// und ist dort ohne Oberflaeche geprueft; hier nur Dialog, Vorschau und das
// Schreiben.
#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "efx/assets.h"
#include "efx/diag.h"
#include "efx/i18n.h"
#include "efx/paths.h"
#include "efx/spritesheet.h"

#include "imgui.h"
#include "theme_imgui.h"
#include "app_shared.h"

namespace efx::gui {

using i18n::Str;
using i18n::tr;

namespace {

namespace fs = std::filesystem;

std::string fileNameOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string stemOf(const std::string& path) {
    std::string name = fileNameOf(path);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name.resize(dot);
    return name;
}

// Zum Vergleichen von Ordnern: Schraegstriche, klein, ohne Strich am Ende.
std::string comparable(std::string path) {
    for (char& c : path) {
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    while (!path.empty() && path.back() == '/') path.pop_back();
    return path;
}

std::string joined(const std::string& root, const std::string& relative) {
    if (root.empty()) return relative;
    const char last = root.back();
    return (last == '/' || last == '\\') ? root + relative : root + "/" + relative;
}

Str errorText(spritesheet::Error error) {
    switch (error) {
        case spritesheet::Error::BadGrid: return Str::SpriteErrGrid;
        case spritesheet::Error::NoFrames: return Str::SpriteErrFrames;
        case spritesheet::Error::BadName: return Str::SpriteErrName;
        case spritesheet::Error::BadFps: return Str::SpriteErrFps;
        case spritesheet::Error::NoImage:
        case spritesheet::Error::None: break;
    }
    return Str::SpriteNoImage;
}

// Raster erraten: quadratische Felder, die das Bild ohne Rest teilen, die
// Feldgroesse moeglichst nah an 192 px (Bereich typischer Effekt-Sheets
// 128..256). 1024 x 1024 -> 4 x 4, 880 x 704 -> 5 x 4, 2048 x 256 -> 8 x 1.
bool guessGrid(int width, int height, int* columns, int* rows) {
    double best = 1e9;
    bool found = false;
    for (int r = 1; r <= 16; ++r) {
        if (height % r != 0) continue;
        const int cell = height / r;
        if (cell < 16 || width % cell != 0) continue;
        const int c = width / cell;
        if (c < 1 || c > 16 || c * r < 2) continue;
        const double distance = std::fabs(std::log2(cell / 192.0));
        if (distance < best) {
            best = distance;
            *columns = c;
            *rows = r;
            found = true;
        }
    }
    return found;
}

}  // namespace

void App::releaseSpriteTexture() {
    if (sprite_.texture != render::kNoTexture && renderer_) renderer_->destroyTexture(sprite_.texture);
    sprite_.texture = render::kNoTexture;
}

void App::loadSpriteSheet(const std::string& path) {
    releaseSpriteTexture();
    sprite_.path = path;
    sprite_.loadError.clear();
    sprite_.sheet = image::Image{};
    std::ifstream in(paths::fromUtf8(path), std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),
                                           std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        sprite_.loadError = path;
        return;
    }
    sprite_.sheet = image::decode(bytes.data(), bytes.size());
    if (!sprite_.sheet.ok) {
        sprite_.loadError = sprite_.sheet.error.empty() ? path : sprite_.sheet.error;
        return;
    }
    sprite_.transparent = spritesheet::hasTransparency(sprite_.sheet);
    const std::string name = spritesheet::sanitizeName(stemOf(path));
    std::snprintf(sprite_.name, sizeof(sprite_.name), "%s", name.empty() ? "spritesheet" : name.c_str());
    int columns = 0, rows = 0;
    if (guessGrid(sprite_.sheet.width, sprite_.sheet.height, &columns, &rows)) {
        sprite_.layout.columns = columns;
        sprite_.layout.rows = rows;
        sprite_.layout.first = 0;
        sprite_.layout.count = 0;
        sprite_.layout.margin = 0;
        sprite_.layout.spacing = 0;
    }
    sprite_.previewStart = static_cast<float>(ImGui::GetTime());
    diag::info("sprite sheet: " + path + " " + std::to_string(sprite_.sheet.width) + "x" +
               std::to_string(sprite_.sheet.height));
}

void App::drawSpriteSheetDialog() {
    if (showSpriteDialog_) {
        ImGui::OpenPopup("###spritesheet");
        showSpriteDialog_ = false;
        sprite_.previewStart = static_cast<float>(ImGui::GetTime());
    }
    // Hoechstens so gross wie das Fenster, sonst rollen: auf kleinen
    // Bildschirmen (oder grosser Schrift) lagen "Create effect" und
    // "Cancel" sonst unerreichbar unter dem Rand.
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(0.0f, 0.0f), ImVec2(viewport->WorkSize.x * 0.95f, viewport->WorkSize.y * 0.92f));
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                       viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    if (!ImGui::BeginPopupModal((std::string(tr(Str::SpriteTitle)) + "###spritesheet").c_str(), nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    const float em = ImGui::GetFontSize();
    const theme::Palette& palette = activeTheme(settings_).palette;
    auto& s = sprite_;

    if (ImGui::Button(tr(Str::SpriteChoose)) && fileDialog_) {
        std::string filter = tr(Str::SpriteFilterImages);
        filter.push_back('\0');
        filter += "*.png;*.tga;*.jpg;*.jpeg";
        filter.push_back('\0');
        const std::string picked = fileDialog_(false, filter.c_str(), nullptr);
        if (!picked.empty()) loadSpriteSheet(picked);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", s.path.empty() ? "" : fileNameOf(s.path).c_str());
    if (!s.loadError.empty()) {
        ImGui::TextColored(toImGui(palette.error), "%s %s", tr(Str::SpriteLoadFailed), s.loadError.c_str());
    }
    // Die Vorschautextur erst hier: der Renderer ist beim Laden nicht immer
    // da, und nach dem Schliessen wird sie freigegeben.
    if (s.sheet.ok && s.texture == render::kNoTexture && renderer_) {
        s.texture = renderer_->createTexture(s.sheet.rgba.data(), s.sheet.width, s.sheet.height, true, false);
    }

    spritesheet::Error gridError = spritesheet::Error::None;
    std::vector<spritesheet::Cell> cells;
    if (s.sheet.ok) cells = spritesheet::gridCells(s.sheet.width, s.sheet.height, s.layout, &gridError);
    const int total = static_cast<int>(cells.size());
    const int first = std::clamp(s.layout.first, 0, std::max(0, total));
    const int last = s.layout.count > 0 ? std::min(total, first + s.layout.count) : total;
    const int inRange = std::max(0, last - first);

    // Zwei Spalten: links Sheet und Vorschau, rechts die Einstellungen. Eine
    // Spalte war hoeher als ein 1040er Fenster — die Knoepfe unten lagen
    // ausserhalb und waren nicht zu erreichen (Selbsttest).
    const float field = 6.0f * em;
    ImGui::BeginGroup();
    if (!s.sheet.ok) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 18.0f * em);
        ImGui::TextDisabled("%s", tr(Str::SpriteNoImage));
        ImGui::PopTextWrapPos();
    } else {
        // Das ganze Sheet mit dem Raster, daneben die laufende Folge.
        const float box = 13.0f * em;
        const float scale = std::min(box / s.sheet.width, box / s.sheet.height);
        const ImVec2 size(s.sheet.width * scale, s.sheet.height * scale);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(at, ImVec2(at.x + size.x, at.y + size.y), IM_COL32(40, 40, 40, 255));
        if (s.texture != render::kNoTexture) {
            ImGui::Image(toImTexture(s.texture), size);
        } else {
            ImGui::Dummy(size);
        }
        for (int i = 0; i < total; ++i) {
            const auto& c = cells[static_cast<size_t>(i)];
            const ImVec2 a(at.x + c.x * scale, at.y + c.y * scale);
            const ImVec2 b(at.x + (c.x + c.w) * scale, at.y + (c.y + c.h) * scale);
            const bool used = i >= first && i < last;
            if (!used) draw->AddRectFilled(a, b, IM_COL32(0, 0, 0, 140));
            draw->AddRect(a, b, used ? ImGui::GetColorU32(ImGuiCol_PlotHistogram) : IM_COL32(120, 120, 120, 200));
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(tr(Str::SpritePreview));
        const float view = 6.0f * em;
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1(p0.x + view, p0.y + view);
        if (s.options.blend == spritesheet::Blend::Additive) {
            draw->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
        } else {
            // Schachbrett: man sieht, was durchsichtig ist.
            const float step = view / 8.0f;
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    const ImU32 col = ((x + y) & 1) ? IM_COL32(150, 150, 150, 255) : IM_COL32(100, 100, 100, 255);
                    draw->AddRectFilled(ImVec2(p0.x + x * step, p0.y + y * step),
                                        ImVec2(p0.x + (x + 1) * step, p0.y + (y + 1) * step), col);
                }
            }
        }
        int shown = -1;
        if (inRange > 0 && gridError == spritesheet::Error::None && s.texture != render::kNoTexture &&
            s.options.fps > 0.0f) {
            const float t = static_cast<float>(ImGui::GetTime()) - s.previewStart;
            // Einmal abspielen heisst hier: abspielen, kurz Pause, von vorn.
            const int cycle = s.options.loop ? inRange : inRange + std::max(1, static_cast<int>(s.options.fps * 0.5f));
            const int k = static_cast<int>(t * s.options.fps) % std::max(1, cycle);
            shown = std::min(k, inRange - 1);
            const auto& c = cells[static_cast<size_t>(first + shown)];
            // Nicht quadratische Felder unverzerrt in der Mitte.
            const float side = static_cast<float>(std::max(c.w, c.h));
            const ImVec2 inner(view * c.w / side, view * c.h / side);
            const ImVec2 q0(p0.x + (view - inner.x) * 0.5f, p0.y + (view - inner.y) * 0.5f);
            draw->AddImage(toImTexture(s.texture), q0, ImVec2(q0.x + inner.x, q0.y + inner.y),
                           ImVec2(static_cast<float>(c.x) / s.sheet.width, static_cast<float>(c.y) / s.sheet.height),
                           ImVec2(static_cast<float>(c.x + c.w) / s.sheet.width,
                                  static_cast<float>(c.y + c.h) / s.sheet.height));
        }
        ImGui::Dummy(ImVec2(view, view));
        if (shown >= 0) ImGui::TextDisabled("%d / %d", shown + 1, inRange);
        ImGui::EndGroup();
    }
    // Speichern steht links unter der Vorschau: so sind beide Spalten etwa
    // gleich hoch und der Dialog passt ohne Rollen auf den Bildschirm.
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 18.0f * em);
    {
        // Wie SeparatorText, aber nur so breit wie die linke Spalte — die
        // ImGui-Fassung lief quer durch die rechte.
        ImGui::Spacing();
        ImGui::TextUnformatted(tr(Str::SpriteGroupOutput));
        const ImVec2 r0 = ImGui::GetItemRectMin();
        const ImVec2 r1 = ImGui::GetItemRectMax();
        const float y = (r0.y + r1.y) * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(r1.x + em * 0.5f, y), ImVec2(r0.x + 20.0f * em, y),
                                            ImGui::GetColorU32(ImGuiCol_Separator));
    }
    ImGui::SetNextItemWidth(2.0f * field);
    ImGui::InputText(tr(Str::SpriteName), s.name, sizeof(s.name));
    ImGui::SetNextItemWidth(2.0f * field);
    ImGui::InputText(tr(Str::SpriteFolder), s.folder, sizeof(s.folder));
    {
        const bool haveGame = !settings_.gamePath.empty();
        if (!haveGame && s.output == 0) s.output = 1;
        ImGui::BeginDisabled(!haveGame);
        ImGui::RadioButton(tr(Str::SpriteOutGame), &s.output, 0);
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s", haveGame ? settings_.gamePath.c_str() : tr(Str::SpriteNoGamePath));

        ImGui::RadioButton(tr(Str::SpriteOutFolder), &s.output, 1);
        ImGui::SameLine();
        ImGui::PushID("folder");
        if (ImGui::Button(tr(Str::SpriteBrowse)) && folderDialog_) {
            const std::string picked = folderDialog_(tr(Str::SpriteOutFolder), s.folderPath.c_str());
            if (!picked.empty()) {
                s.folderPath = picked;
                s.output = 1;
            }
        }
        ImGui::PopID();
        if (!s.folderPath.empty()) ImGui::TextWrapped("%s", s.folderPath.c_str());

        ImGui::RadioButton(tr(Str::SpriteOutPk3), &s.output, 2);
        ImGui::SameLine();
        ImGui::PushID("pk3");
        if (ImGui::Button(tr(Str::SpriteBrowse)) && fileDialog_) {
            std::string filter = tr(Str::SpriteFilterPk3);
            filter.push_back('\0');
            filter += "*.pk3";
            filter.push_back('\0');
            const std::string suggested = std::string("zz_") + spritesheet::sanitizeName(s.name) + ".pk3";
            std::string picked = fileDialog_(true, filter.c_str(), suggested.c_str());
            if (!picked.empty()) {
                // Der Dialog haengt ohne Endung ".efx" an (seine Vorgabe).
                std::string lower = comparable(picked);
                if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".efx") picked.resize(picked.size() - 4);
                lower = comparable(picked);
                if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".pk3") picked += ".pk3";
                s.pk3Path = picked;
                s.output = 2;
            }
        }
        ImGui::PopID();
        if (!s.pk3Path.empty()) ImGui::TextWrapped("%s", s.pk3Path.c_str());
    }

    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    ImGui::SameLine(0.0f, em);
    ImGui::BeginGroup();

    const auto intField = [&](Str label, int* value, int lo, int hi) {
        ImGui::SetNextItemWidth(field);
        if (ImGui::InputInt(tr(label), value)) *value = std::clamp(*value, lo, hi);
    };

    ImGui::SeparatorText(tr(Str::SpriteGroupGrid));
    intField(Str::SpriteColumns, &s.layout.columns, 1, 64);
    ImGui::SameLine();
    intField(Str::SpriteRows, &s.layout.rows, 1, 64);
    intField(Str::SpriteFirst, &s.layout.first, 0, std::max(0, total - 1));
    ImGui::SameLine();
    intField(Str::SpriteCount, &s.layout.count, 0, std::max(0, total));
    intField(Str::SpriteMargin, &s.layout.margin, 0, 4096);
    ImGui::SameLine();
    intField(Str::SpriteSpacing, &s.layout.spacing, 0, 4096);
    ImGui::Checkbox(tr(Str::SpriteColumnsFirst), &s.layout.columnsFirst);
    ImGui::SameLine();
    ImGui::Checkbox(tr(Str::SpriteSkipEmpty), &s.layout.skipEmptyAtEnd);

    ImGui::SeparatorText(tr(Str::SpriteGroupAnim));
    ImGui::SetNextItemWidth(field);
    shared::dragFloatFinite(tr(Str::SpriteFps), &s.options.fps, 0.25f, 1.0f, 120.0f, "%.4g",
                            ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    {
        const int frames = std::min(inRange, spritesheet::kMaxFrames);
        const float seconds = s.options.fps > 0.0f ? static_cast<float>(inRange) / s.options.fps : 0.0f;
        ImGui::TextDisabled(tr(Str::SpriteDuration), frames, static_cast<double>(seconds));
    }
    ImGui::Checkbox(tr(Str::SpriteLoop), &s.options.loop);
    ImGui::Checkbox(tr(Str::SpriteCrossfade), &s.options.crossfade);

    ImGui::SeparatorText(tr(Str::SpriteGroupLook));
    {
        const Str presets[] = {Str::PresetExplosion, Str::PresetSmoke, Str::PresetMuzzle, Str::PresetEnergy};
        int preset = static_cast<int>(s.options.preset);
        ImGui::SetNextItemWidth(2.0f * field);
        if (ImGui::BeginCombo(tr(Str::SpritePreset), tr(presets[preset]))) {
            for (int i = 0; i < 4; ++i) {
                if (ImGui::Selectable(tr(presets[i]), i == preset) && i != preset) {
                    s.options.preset = static_cast<spritesheet::Preset>(i);
                    // Rauch wird gemischt, Feuer und Licht addieren.
                    s.options.blend = i == 1 ? spritesheet::Blend::Alpha : spritesheet::Blend::Additive;
                }
            }
            ImGui::EndCombo();
        }
        int blend = static_cast<int>(s.options.blend);
        const Str blends[] = {Str::BlendAdditive, Str::BlendAlpha};
        ImGui::SetNextItemWidth(2.0f * field);
        if (ImGui::BeginCombo(tr(Str::SpriteBlendLabel), tr(blends[blend]))) {
            for (int i = 0; i < 2; ++i) {
                if (ImGui::Selectable(tr(blends[i]), i == blend)) s.options.blend = static_cast<spritesheet::Blend>(i);
            }
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(s.options.blend != spritesheet::Blend::Alpha);
        ImGui::Checkbox(tr(Str::SpriteAlphaFromBrightness), &s.options.alphaFromBrightness);
        ImGui::EndDisabled();

        const int cellSide = cells.empty() ? 0 : std::max(cells.front().w, cells.front().h);
        const int automatic = cellSide > 0 ? spritesheet::nearestPowerOfTwo(cellSide) : 0;
        char autoLabel[64];
        std::snprintf(autoLabel, sizeof(autoLabel), tr(Str::SpriteFrameSizeAuto), automatic);
        const int sizes[] = {0, 64, 128, 256, 512, 1024};
        const std::string current = s.options.frameSize == 0 ? std::string(autoLabel)
                                                             : std::to_string(s.options.frameSize);
        ImGui::SetNextItemWidth(2.0f * field);
        if (ImGui::BeginCombo(tr(Str::SpriteFrameSize), current.c_str())) {
            for (int value : sizes) {
                const std::string label = value == 0 ? std::string(autoLabel) : std::to_string(value);
                if (ImGui::Selectable(label.c_str(), value == s.options.frameSize)) s.options.frameSize = value;
            }
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(field);
        shared::dragFloatFinite(tr(Str::SpriteParticleSize), &s.options.size, 0.5f, 0.5f, 2000.0f, "%.4g",
                                ImGuiSliderFlags_AlwaysClamp);
    }

    // Was herauskommt, und was nicht geht.
    ImGui::Separator();
    Str problem = Str::SpriteNoImage;
    bool blocked = !s.sheet.ok;
    if (s.sheet.ok) {
        if (gridError != spritesheet::Error::None) {
            problem = Str::SpriteErrGrid;
            blocked = true;
        } else if (inRange <= 0) {
            problem = Str::SpriteErrFrames;
            blocked = true;
        } else if (spritesheet::sanitizeName(s.name).empty()) {
            problem = Str::SpriteErrName;
            blocked = true;
        } else if (!(s.options.fps > 0.0f)) {
            problem = Str::SpriteErrFps;
            blocked = true;
        } else if ((s.output == 0 && settings_.gamePath.empty()) || (s.output == 1 && s.folderPath.empty()) ||
                   (s.output == 2 && s.pk3Path.empty())) {
            problem = Str::SpriteErrOutput;
            blocked = true;
        }
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 30.0f * em);
    if (s.sheet.ok && !cells.empty()) {
        const int side = std::max(cells.front().w, cells.front().h);
        const int written = s.options.frameSize > 0 ? s.options.frameSize : spritesheet::nearestPowerOfTwo(side);
        ImGui::TextDisabled(tr(Str::SpriteCellInfo), cells.front().w, cells.front().h, written, written);
        ImGui::TextDisabled(tr(Str::SpriteRange), inRange, total);
        const int maximum = s.options.crossfade && !s.options.loop ? spritesheet::kMaxFrames - 1 : spritesheet::kMaxFrames;
        if (inRange > maximum) {
            ImGui::TextColored(toImGui(palette.warning), tr(Str::SpriteTooMany), maximum, maximum);
        }
        if (s.options.blend == spritesheet::Blend::Alpha && !s.options.alphaFromBrightness && !s.transparent) {
            ImGui::TextColored(toImGui(palette.warning), "%s", tr(Str::SpriteNoAlpha));
        }
        if (s.options.crossfade) ImGui::TextDisabled("%s", tr(Str::SpriteCrossfadeNote));
    }
    if (blocked && s.sheet.ok) ImGui::TextColored(toImGui(palette.error), "%s", tr(problem));
    ImGui::PopTextWrapPos();

    ImGui::BeginDisabled(blocked);
    if (ImGui::Button(tr(Str::SpriteCreate))) {
        ImGui::CloseCurrentPopup();
        releaseSpriteTexture();
        createSpriteEffect();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr(Str::MsgCancel))) {
        ImGui::CloseCurrentPopup();
        releaseSpriteTexture();
    }
    ImGui::EndGroup();
    ImGui::EndPopup();
}

void App::createSpriteEffect() {
    auto& s = sprite_;
    s.options.name = s.name;
    s.options.folder = s.folder;
    auto package = std::make_shared<spritesheet::Package>(
        spritesheet::build(s.sheet, s.layout, s.options, fileNameOf(s.path)));
    if (!package->ok()) {
        showNotice(tr(errorText(package->error)));
        return;
    }
    // Was schon da ist, wird nicht still ersetzt.
    std::vector<std::string> existing;
    std::error_code ec;
    if (s.output == 2) {
        if (fs::exists(paths::fromUtf8(s.pk3Path), ec)) existing.push_back(s.pk3Path);
    } else {
        const std::string root = s.output == 0 ? settings_.gamePath : s.folderPath;
        for (const auto& f : package->files) {
            if (fs::exists(paths::fromUtf8(joined(root, f.path)), ec)) existing.push_back(f.path);
        }
    }
    if (!existing.empty()) {
        std::string text = tr(Str::SpriteOverwrite);
        for (size_t i = 0; i < existing.size() && i < 8; ++i) text += "\n  " + existing[i];
        if (existing.size() > 8) text += "\n  ...";
        askConfirm(text, [this, package] { writeSpritePackage(*package); });
        return;
    }
    writeSpritePackage(*package);
}

bool App::writeSpritePackage(const spritesheet::Package& p) {
    auto& s = sprite_;
    std::error_code ec;
    std::string where;
    if (s.output == 2) {
        const auto bytes = spritesheet::zipStored(p.files);
        fs::create_directories(paths::fromUtf8(s.pk3Path).parent_path(), ec);
        if (!paths::writeFileReplacing(s.pk3Path, bytes.data(), bytes.size())) {
            showNotice(std::string(tr(Str::SpriteWriteFailed)) + "\n" + s.pk3Path);
            return false;
        }
        where = s.pk3Path;
        // Wie "PK3-Archiv oeffnen": mit Vorrang in den Bestand, damit die
        // Vorschau Bilder und Shader findet.
        settleTextureJobs();
        assets::mergeOpenedArchive(assets_, assets::scanArchive(s.pk3Path));
        if (std::find(archiveSources_.begin(), archiveSources_.end(), s.pk3Path) == archiveSources_.end()) {
            archiveSources_.push_back(s.pk3Path);
        }
        textureCacheDirty_ = true;
        missingTextures_.clear();
        browserEntries_.clear();
        // Aus dem Archiv geoeffnet: ohne Dateipfad, Speichern fragt nach.
        if (documentInUse(doc())) newDocument();
        showEditor();
        ReadResult result = read(p.effectText);
        doc().effect = std::move(result.effect);
        doc().parseDiagnostics = std::move(result.diagnostics);
        doc().filePath.clear();
        doc().dirty = false;
        doc().selectedPrimitive = doc().effect.primitives.empty() ? -1 : 0;
        doc().segmentEnabled.assign(doc().effect.primitives.size(), true);
        doc().undo.reset(doc().effect);
        doc().savedUndoId = doc().undo.currentId();
        refreshDiagnostics();
        buildPreviewStopped();
    } else {
        const std::string root = s.output == 0 ? settings_.gamePath : s.folderPath;
        for (const auto& f : p.files) {
            const std::string target = joined(root, f.path);
            fs::create_directories(paths::fromUtf8(target).parent_path(), ec);
            if (!paths::writeFileReplacing(target, f.data.data(), f.data.size())) {
                showNotice(std::string(tr(Str::SpriteWriteFailed)) + "\n" + target);
                return false;
            }
        }
        where = root;
        bool isGamePath = false;
        for (const auto& path : settings_.allGamePaths()) {
            if (comparable(path) == comparable(root)) isGamePath = true;
        }
        if (isGamePath) {
            rescanAssets();
        } else {
            // Ein Arbeitsordner ausserhalb der Spielpfade: nur ihn einmischen.
            settleTextureJobs();
            assets::mergeOpenedArchive(assets_, assets::scanAll({root}));
            textureCacheDirty_ = true;
            missingTextures_.clear();
            browserEntries_.clear();
        }
        if (!openFile(joined(root, p.effectPath))) return false;
    }
    // Gleich zeigen, was entstanden ist.
    pressPlay();
    char message[1024];
    std::snprintf(message, sizeof(message), tr(Str::SpriteDone), p.effectPath.c_str(), p.frames,
                  static_cast<int>(p.files.size()), where.c_str());
    screenshotMessage_ = message;
    screenshotMessageUntil_ = static_cast<float>(ImGui::GetTime()) + 8.0f;
    diag::info(std::string("sprite sheet: ") + message);
    return true;
}

}  // namespace efx::gui
