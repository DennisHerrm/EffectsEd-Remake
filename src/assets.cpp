#include "efx/assets.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>

#include "efx/inflate.h"
#include "efx/shader.h"

namespace fs = std::filesystem;

namespace efx::assets {
namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string extensionOf(const std::string& path) {
    const size_t dot = path.rfind('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos) return "";
    if (slash != std::string::npos && dot < slash) return "";
    return toLower(path.substr(dot));
}

bool startsWith(const std::string& text, const char* prefix) {
    return text.compare(0, std::strlen(prefix), prefix) == 0;
}

// Kleine Hilfe zum Lesen aus einem Speicherbereich, die nie ueber das Ende
// hinauslaeuft. Ein beschaedigtes oder absichtlich verbogenes .pk3 darf den
// Editor nicht zum Absturz bringen — es liegt im Spielordner, und da kommt
// alles Moegliche her.
class Reader {
public:
    Reader(const unsigned char* data, size_t size) : data_(data), size_(size) {}

    bool u16(size_t offset, uint16_t& out) const {
        if (offset + 2 > size_) return false;
        out = static_cast<uint16_t>(data_[offset] | (data_[offset + 1] << 8));
        return true;
    }
    bool u32(size_t offset, uint32_t& out) const {
        if (offset + 4 > size_) return false;
        out = static_cast<uint32_t>(data_[offset]) |
              (static_cast<uint32_t>(data_[offset + 1]) << 8) |
              (static_cast<uint32_t>(data_[offset + 2]) << 16) |
              (static_cast<uint32_t>(data_[offset + 3]) << 24);
        return true;
    }
    bool text(size_t offset, size_t length, std::string& out) const {
        if (offset + length > size_) return false;
        out.assign(reinterpret_cast<const char*>(data_ + offset), length);
        return true;
    }
    size_t size() const { return size_; }
    const unsigned char* data() const { return data_; }

private:
    const unsigned char* data_;
    size_t size_;
};

}  // namespace

std::vector<std::string> readZipDirectory(const std::string& path,
                                          std::string* error) {
    std::vector<std::string> names;
    auto fail = [&](const char* why) {
        if (error) *error = why;
        return names;
    };

    std::ifstream file(path, std::ios::binary);
    if (!file) return fail("cannot open");

    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    if (length < 22) return fail("too small for a zip file");

    // Das "End of central directory" steht ganz hinten, kann aber bis zu
    // 64 KiB Kommentar hinter sich haben. Deshalb rueckwaerts suchen statt
    // einen festen Abstand anzunehmen.
    const std::streamoff tailSize = std::min<std::streamoff>(length, 66000);
    std::vector<unsigned char> tail(static_cast<size_t>(tailSize));
    file.seekg(length - tailSize);
    file.read(reinterpret_cast<char*>(tail.data()), tailSize);
    const Reader tailReader(tail.data(), tail.size());

    size_t endRecord = std::string::npos;
    for (size_t i = tail.size() >= 22 ? tail.size() - 22 : 0; i + 4 <= tail.size();
         --i) {
        if (tail[i] == 'P' && tail[i + 1] == 'K' && tail[i + 2] == 0x05 &&
            tail[i + 3] == 0x06) {
            endRecord = i;
            break;
        }
        if (i == 0) break;
    }
    if (endRecord == std::string::npos) return fail("no zip end record");

    uint16_t entryCount = 0;
    uint32_t directoryOffset = 0, directorySize = 0;
    if (!tailReader.u16(endRecord + 10, entryCount) ||
        !tailReader.u32(endRecord + 12, directorySize) ||
        !tailReader.u32(endRecord + 16, directoryOffset)) {
        return fail("truncated zip end record");
    }
    if (directoryOffset + static_cast<uint64_t>(directorySize) >
        static_cast<uint64_t>(length)) {
        return fail("zip directory points outside the file");
    }

    std::vector<unsigned char> directory(directorySize);
    file.seekg(directoryOffset);
    file.read(reinterpret_cast<char*>(directory.data()), directorySize);
    if (!file) return fail("cannot read zip directory");
    const Reader dir(directory.data(), directory.size());

    size_t p = 0;
    names.reserve(entryCount);
    for (uint16_t i = 0; i < entryCount; ++i) {
        if (p + 46 > dir.size()) break;
        if (!(dir.data()[p] == 'P' && dir.data()[p + 1] == 'K' &&
              dir.data()[p + 2] == 0x01 && dir.data()[p + 3] == 0x02)) {
            break;  // kein gueltiger Eintrag mehr
        }
        uint16_t nameLength = 0, extraLength = 0, commentLength = 0;
        dir.u16(p + 28, nameLength);
        dir.u16(p + 30, extraLength);
        dir.u16(p + 32, commentLength);

        std::string name;
        if (!dir.text(p + 46, nameLength, name)) break;

        // Ordnereintraege enden auf / und interessieren nicht.
        if (!name.empty() && name.back() != '/') names.push_back(std::move(name));

        p += 46u + nameLength + extraLength + commentLength;
    }
    return names;
}

std::vector<unsigned char> readFromZip(const std::string& archivePath,
                                       const std::string& name,
                                       std::string* error) {
    std::vector<unsigned char> empty;
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return empty;
    };

    std::ifstream file(archivePath, std::ios::binary);
    if (!file) return fail("cannot open archive");

    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    if (length < 22) return fail("too small for a zip file");

    const std::streamoff tailSize = std::min<std::streamoff>(length, 66000);
    std::vector<unsigned char> tail(static_cast<size_t>(tailSize));
    file.seekg(length - tailSize);
    file.read(reinterpret_cast<char*>(tail.data()), tailSize);
    const Reader tailReader(tail.data(), tail.size());

    size_t endRecord = std::string::npos;
    for (size_t i = tail.size() >= 22 ? tail.size() - 22 : 0; i + 4 <= tail.size();
         --i) {
        if (tail[i] == 'P' && tail[i + 1] == 'K' && tail[i + 2] == 0x05 &&
            tail[i + 3] == 0x06) {
            endRecord = i;
            break;
        }
        if (i == 0) break;
    }
    if (endRecord == std::string::npos) return fail("no zip end record");

    uint16_t entryCount = 0;
    uint32_t directoryOffset = 0, directorySize = 0;
    tailReader.u16(endRecord + 10, entryCount);
    tailReader.u32(endRecord + 12, directorySize);
    tailReader.u32(endRecord + 16, directoryOffset);
    if (directoryOffset + static_cast<uint64_t>(directorySize) >
        static_cast<uint64_t>(length)) {
        return fail("zip directory points outside the file");
    }

    std::vector<unsigned char> directory(directorySize);
    file.seekg(directoryOffset);
    file.read(reinterpret_cast<char*>(directory.data()), directorySize);
    const Reader dir(directory.data(), directory.size());

    const std::string wanted = toLower(name);
    size_t p = 0;
    for (uint16_t i = 0; i < entryCount; ++i) {
        if (p + 46 > dir.size()) break;
        uint16_t method = 0, nameLength = 0, extraLength = 0, commentLength = 0;
        uint32_t compressedSize = 0, uncompressedSize = 0, localOffset = 0;
        dir.u16(p + 10, method);
        dir.u32(p + 20, compressedSize);
        dir.u32(p + 24, uncompressedSize);
        dir.u16(p + 28, nameLength);
        dir.u16(p + 30, extraLength);
        dir.u16(p + 32, commentLength);
        dir.u32(p + 42, localOffset);

        std::string entryName;
        if (!dir.text(p + 46, nameLength, entryName)) break;
        p += 46u + nameLength + extraLength + commentLength;

        if (toLower(entryName) != wanted) continue;

        // Gefunden. Jetzt zum lokalen Kopf springen — dort steht, wie lang
        // Name und Zusatzfeld dieses Eintrags sind, und die koennen von den
        // Angaben im Verzeichnis abweichen.
        if (localOffset + 30u > static_cast<uint64_t>(length)) {
            return fail("local header points outside the file");
        }
        unsigned char localHeader[30];
        file.seekg(localOffset);
        file.read(reinterpret_cast<char*>(localHeader), 30);
        if (!file) return fail("cannot read local header");
        const Reader local(localHeader, 30);
        if (!(localHeader[0] == 'P' && localHeader[1] == 'K' &&
              localHeader[2] == 0x03 && localHeader[3] == 0x04)) {
            return fail("bad local header");
        }
        uint16_t localNameLength = 0, localExtraLength = 0;
        local.u16(26, localNameLength);
        local.u16(28, localExtraLength);

        const uint64_t dataStart =
            static_cast<uint64_t>(localOffset) + 30u + localNameLength +
            localExtraLength;
        if (dataStart + compressedSize > static_cast<uint64_t>(length)) {
            return fail("entry data runs past the end of the file");
        }

        std::vector<unsigned char> packed(compressedSize);
        file.seekg(static_cast<std::streamoff>(dataStart));
        file.read(reinterpret_cast<char*>(packed.data()), compressedSize);
        if (!file) return fail("cannot read entry data");

        if (method == 0) return packed;  // gespeichert, nichts auszupacken
        if (method != 8) {
            return fail("unsupported compression method " + std::to_string(method));
        }

        auto result = inflate::raw(packed.data(), packed.size(), uncompressedSize);
        if (!result.ok) return fail("inflate: " + result.error);
        return result.data;
    }
    return fail("not found in archive");
}

ResolvedTexture findTexture(const Index& index, const std::string& basePath,
                            const std::string& name) {
    ResolvedTexture out;
    if (name.empty()) return out;

    // Schritt 1 und 2: kennt der Shaderbestand diesen Namen? Dann die
    // map-Zeile seiner ersten Stufe nehmen.
    std::string candidate = index.mapOf(name);
    if (candidate.empty()) {
        // Schritt 3: kein Shader mit dem Namen — der Name gilt direkt als
        // Bildname. Die Engine baut sich daraus einen Ersatzshader.
        candidate = name;
    }
    // Sonderwerte der Engine, hinter denen keine Datei steht.
    const std::string lower = toLower(candidate);
    if (lower.empty() || lower[0] == '$') return out;

    // Eine Endung, die schon dransteht, nicht doppelt anhaengen.
    std::string stem = candidate;
    const std::string existing = extensionOf(stem);
    for (const auto& ext : imageExtensions()) {
        if (existing == ext) {
            stem = stem.substr(0, stem.size() - ext.size());
            break;
        }
    }
    for (char& c : stem) {
        if (c == '\\') c = '/';
    }
    stem = toLower(stem);

    std::error_code ec;
    const fs::path base(basePath);

    // Schritt 4 und 5: Endungen durchprobieren, erst ausgepackt, dann in den
    // Archiven. Ausgepackte Dateien gewinnen — genauso macht es die Engine,
    // damit man eine gepackte Datei ueberschreiben kann, ohne sie zu ersetzen.
    // Alle Wurzeln in der Suchreihenfolge, ausgepackt zuerst.
    //
    // `basePath` bleibt vorn, damit ein Aufruf mit einem einzelnen Ordner
    // weiter funktioniert; `index.roots` kommt aus scanAll und ist bei einem
    // einzelnen Ordner leer.
    std::vector<std::string> roots;
    if (!basePath.empty()) roots.push_back(basePath);
    for (const auto& root : index.roots) {
        bool known = false;
        for (const auto& seen : roots) {
            if (seen == root) known = true;
        }
        if (!known) roots.push_back(root);
    }

    for (const auto& root : roots) {
        for (const auto& ext : imageExtensions()) {
            const std::string relative = stem + ext;
            if (fs::is_regular_file(fs::path(root) / relative, ec)) {
                out.path = relative;
                out.root = root;
                out.found = true;
                return out;
            }
        }
    }
    for (const auto& ext : imageExtensions()) {
        const std::string relative = stem + ext;
        // Im Bestand steht der Name ohne Endung — deshalb erst hier pruefen,
        // ob das Archiv ihn wirklich fuehrt.
        if (!std::binary_search(index.textures.begin(), index.textures.end(), stem)) {
            break;
        }
        for (const auto& archive : index.archives) {
            std::string error;
            if (!readFromZip(archive, relative, &error).empty()) {
                out.path = relative;
                out.archive = archive;
                out.found = true;
                return out;
            }
        }
    }
    return out;
}

std::vector<std::string> discoverRoots(const std::string& folder, int maxDepth) {
    std::vector<std::string> found;
    if (folder.empty()) return found;

    std::error_code ec;
    const fs::path start(folder);
    if (!fs::is_directory(start, ec)) return found;

    // Sieht der Ordner selbst wie ein Spielordner aus?
    const auto looksLikeRoot = [](const fs::path& dir) {
        std::error_code inner;
        for (fs::directory_iterator it(dir, inner), end; it != end;
             it.increment(inner)) {
            if (inner) break;
            if (it->is_regular_file(inner)) {
                if (toLower(it->path().extension().string()) == ".pk3") return true;
                continue;
            }
            if (!it->is_directory(inner)) continue;
            const std::string name = toLower(it->path().filename().string());
            if (name == "shaders" || name == "effects" || name == "gfx") return true;
        }
        return false;
    };

    if (looksLikeRoot(start)) {
        found.push_back(start.generic_string());
        return found;
    }

    // Sonst die Unterordner absuchen. Ein Deckel, damit ein versehentlich
    // gewaehltes Laufwerk nicht hunderte Wurzeln ergibt.
    constexpr size_t kMaxRoots = 32;
    std::vector<fs::path> queue{start};
    for (int depth = 0; depth < maxDepth && found.size() < kMaxRoots; ++depth) {
        std::vector<fs::path> next;
        for (const auto& dir : queue) {
            for (fs::directory_iterator it(dir, ec), end; it != end;
                 it.increment(ec)) {
                if (ec) break;
                if (!it->is_directory(ec)) continue;
                if (looksLikeRoot(it->path())) {
                    found.push_back(it->path().generic_string());
                    if (found.size() >= kMaxRoots) break;
                } else {
                    next.push_back(it->path());
                }
            }
            if (found.size() >= kMaxRoots) break;
        }
        queue = std::move(next);
    }

    // Immer dieselbe Reihenfolge — sonst wechselt die Ueberschreibregel von
    // Lauf zu Lauf, und das waere schlimmer als gar keine.
    std::sort(found.begin(), found.end());
    return found;
}

ResolvedTexture findSound(const Index& index, const std::string& basePath,
                          const std::string& name) {
    ResolvedTexture out;
    if (name.empty()) return out;

    std::string base = name;
    for (char& c : base) {
        if (c == '\\') c = '/';
    }
    base = toLower(base);

    // Die Endungen, die probiert werden — in der Reihenfolge der Engine.
    //
    // `S_LoadSound_Actual` liest erst den Namen, wie er dasteht, und ersetzt
    // bei Misserfolg die letzten drei Zeichen durch "mp3". In fast jeder
    // Raven-.efx steht deshalb `.wav`, obwohl im `.pk3` eine `.mp3` liegt.
    std::vector<std::string> candidates;
    candidates.push_back(base);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) {
        const std::string stem = base.substr(0, dot);
        for (const char* ext : {".mp3", ".wav"}) {
            const std::string other = stem + ext;
            bool known = false;
            for (const auto& seen : candidates) {
                if (seen == other) known = true;
            }
            if (!known) candidates.push_back(other);
        }
    }

    std::error_code ec;

    // Alle Wurzeln, ausgepackt zuerst — dieselbe Reihenfolge wie bei Bildern.
    // Vorher wurde nur `basePath` durchsucht; wer mehrere Spielpfade hat, fand
    // in allen ausser dem ersten nichts.
    std::vector<std::string> roots;
    if (!basePath.empty()) roots.push_back(basePath);
    for (const auto& root : index.roots) {
        bool known = false;
        for (const auto& seen : roots) {
            if (seen == root) known = true;
        }
        if (!known) roots.push_back(root);
    }

    for (const auto& root : roots) {
        for (const auto& relative : candidates) {
            if (fs::is_regular_file(fs::path(root) / relative, ec)) {
                out.path = relative;
                out.root = root;
                out.found = true;
                return out;
            }
        }
    }
    for (const auto& relative : candidates) {
        for (const auto& archive : index.archives) {
            std::string error;
            if (!readFromZip(archive, relative, &error).empty()) {
                out.path = relative;
                out.archive = archive;
                out.found = true;
                return out;
            }
        }
    }
    return out;
}

Index scanAll(const std::vector<std::string>& basePaths, jobs::Pool* pool,
              jobs::Cancellation* cancel) {
    Index combined;
    if (basePaths.empty()) {
        combined.notes.push_back("no game path set");
        return combined;
    }

    for (const auto& base : basePaths) {
        if (cancel && cancel->cancelled()) break;
        Index one = scan(base, pool, cancel);

        // Die Wurzel je Fund merken, damit man spaeter weiss, wo eine Datei
        // herkommt. Ohne das koennte man sie nicht mehr oeffnen.
        for (const auto& archive : one.archives) combined.archives.push_back(archive);
        for (auto& entry : one.shaderMaps) combined.shaderMaps.push_back(entry);
        for (auto& entry : one.shaderTexMods) {
            combined.shaderTexMods.push_back(std::move(entry));
        }
        for (auto& entry : one.shaderRgbWaves) {
            combined.shaderRgbWaves.push_back(std::move(entry));
        }
        for (auto& entry : one.shaderAlphaWaves) {
            combined.shaderAlphaWaves.push_back(std::move(entry));
        }
        for (auto& entry : one.shaderAlphaWaves) {
            combined.shaderAlphaWaves.push_back(std::move(entry));
        }
        for (auto& entry : one.shaderAnims) {
            combined.shaderAnims.push_back(std::move(entry));
        }
        for (auto& entry : one.effectSources) {
            combined.effectSources.push_back(std::move(entry));
        }
        for (auto& entry : one.shaderBlends) combined.shaderBlends.push_back(entry);
        combined.roots.push_back(base);

        auto merge = [](std::vector<std::string>& into,
                        const std::vector<std::string>& from) {
            into.insert(into.end(), from.begin(), from.end());
        };
        merge(combined.shaders, one.shaders);
        merge(combined.textures, one.textures);
        merge(combined.models, one.models);
        merge(combined.sounds, one.sounds);
        merge(combined.effects, one.effects);

        combined.pk3Count += one.pk3Count;
        combined.filesSeen += one.filesSeen;
        combined.shaderFilesRead += one.shaderFilesRead;
        for (const auto& note : one.notes) combined.notes.push_back(note);
    }

    // Die Listen muessen sortiert und doppelfrei sein — hasShader und
    // findTexture suchen binaer darin.
    //
    // Bei shaderMaps und shaderBlends wird NICHT sortiert: dort gewinnt der
    // erste Eintrag, und der kommt aus dem zuerst durchsuchten Ordner. Genau
    // das ist die Ueberschreibregel.
    auto tidy = [](std::vector<std::string>& list) {
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    };
    tidy(combined.shaders);
    tidy(combined.textures);
    tidy(combined.models);
    tidy(combined.sounds);
    tidy(combined.effects);
    return combined;
}

ResolvedTexture findEffect(const Index& index, const std::string& basePath,
                           const std::string& name) {
    ResolvedTexture out;
    if (name.empty()) return out;

    std::string stem = toLower(name);
    for (char& c : stem) {
        if (c == '\\') c = '/';
    }
    if (extensionOf(stem) == ".efx") stem.erase(stem.size() - 4);
    // In der Datei steht der Name ohne "effects/" — auf der Platte liegt er
    // darunter.
    const std::string relative = "effects/" + stem + ".efx";

    std::error_code ec;
    std::vector<std::string> roots;
    if (!basePath.empty()) roots.push_back(basePath);
    for (const auto& root : index.roots) {
        bool known = false;
        for (const auto& seen : roots) {
            if (seen == root) known = true;
        }
        if (!known) roots.push_back(root);
    }
    for (const auto& root : roots) {
        if (fs::is_regular_file(fs::path(root) / relative, ec)) {
            out.path = relative;
            out.root = root;
            out.found = true;
            return out;
        }
    }
    if (!std::binary_search(index.effects.begin(), index.effects.end(), stem)) {
        return out;
    }
    for (const auto& archive : index.archives) {
        if (!readFromZip(archive, relative).empty()) {
            out.path = relative;
            out.archive = archive;
            out.found = true;
            return out;
        }
    }
    return out;
}

std::vector<unsigned char> readFile(const std::string& basePath,
                                    const ResolvedTexture& where,
                                    std::string* error) {
    if (!where.found) {
        if (error) *error = "not resolved";
        return {};
    }
    if (!where.archive.empty()) {
        return readFromZip(where.archive, where.path, error);
    }
    // Die beim Suchen gemerkte Wurzel benutzen, nicht die uebergebene: bei
    // mehreren Spielpfaden liegt die Datei unter einem davon, und welcher es
    // war, weiss nur der Fund.
    const fs::path root = where.root.empty() ? fs::path(basePath)
                                             : fs::path(where.root);
    std::ifstream file(root / where.path, std::ios::binary);
    if (!file) {
        if (error) *error = "cannot open " + where.path;
        return {};
    }
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
}

Kind classify(const std::string& relativePath) {
    const std::string lower = toLower(relativePath);
    const std::string ext = extensionOf(lower);

    if (ext == ".shader") return Kind::Shader;
    if (ext == ".efx") return Kind::Effect;
    if (ext == ".md3" || ext == ".glm" || ext == ".ase") return Kind::Model;
    if (ext == ".wav" || ext == ".mp3") return Kind::Sound;
    if (ext == ".tga" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
        ext == ".pcx" || ext == ".bmp") {
        return Kind::Texture;
    }
    return Kind::Ignore;
}

std::string normaliseName(const std::string& relativePath, Kind kind) {
    std::string name = toLower(relativePath);
    for (char& c : name) {
        if (c == '\\') c = '/';
    }
    while (!name.empty() && name.front() == '/') name.erase(name.begin());

    // Texturen und Effekte werden ohne Endung geschrieben — die Engine haengt
    // sie selbst an und probiert dabei mehrere.
    if (kind == Kind::Texture || kind == Kind::Effect) {
        const size_t dot = name.rfind('.');
        const size_t slash = name.find_last_of('/');
        if (dot != std::string::npos &&
            (slash == std::string::npos || dot > slash)) {
            name.erase(dot);
        }
    }
    // Effekte zusaetzlich ohne den Ordnervorsatz: in einer .efx-Datei steht
    // "explosions/big", nicht "effects/explosions/big".
    if (kind == Kind::Effect && startsWith(name, "effects/")) {
        name.erase(0, std::strlen("effects/"));
    }
    return name;
}

const std::vector<std::string>& imageExtensions() {
    // Die Reihenfolge, in der die Engine probiert. Sie nimmt die erste, die
    // es gibt — deshalb ist sie nicht beliebig: liegen `spark.tga` und
    // `spark.jpg` nebeneinander, gewinnt tga.
    static const std::vector<std::string> kExtensions = {".tga", ".jpg", ".jpeg",
                                                         ".png", ".pcx", ".bmp"};
    return kExtensions;
}

std::string Index::mapOf(const std::string& shaderName) const {
    const std::string key = toLower(shaderName);
    for (const auto& entry : shaderMaps) {
        if (entry.first == key) return entry.second;
    }
    return {};
}

const shader::WaveForm* Index::alphaWaveOf(const std::string& shaderName) const {
    const std::string key = toLower(shaderName);
    for (const auto& entry : shaderAlphaWaves) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

const shader::WaveForm* Index::rgbWaveOf(const std::string& shaderName) const {
    const std::string key = toLower(shaderName);
    for (const auto& entry : shaderRgbWaves) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

const std::vector<shader::TexMod>& Index::texModsOf(
    const std::string& shaderName) const {
    static const std::vector<shader::TexMod> none;
    const std::string key = toLower(shaderName);
    for (const auto& entry : shaderTexMods) {
        if (entry.first == key) return entry.second;
    }
    return none;
}

const Index::AnimatedShader* Index::animOf(const std::string& shaderName) const {
    const std::string key = toLower(shaderName);
    for (const auto& entry : shaderAnims) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

const std::string& Index::sourceOf(const std::string& effectName) const {
    static const std::string none;
    for (const auto& entry : effectSources) {
        if (entry.first == effectName) return entry.second;
    }
    return none;
}

shader::BlendMode Index::blendOf(const std::string& shaderName) const {
    const std::string key = toLower(shaderName);
    for (const auto& entry : shaderBlends) {
        if (entry.first == key) return entry.second;
    }
    // Kein Shaderblock zu diesem Namen. Hier stand "additiv, das ist bei
    // Effekten die haeufigste Mischung" — geraten, und falsch.
    //
    // Die Engine baut sich einen Ersatzshader, R_FindShader in tr_shader.cpp:
    //
    //     } else if ( shader.lightmapIndex[0] == LIGHTMAP_2D ) {
    //         stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
    //               GLS_SRCBLEND_SRC_ALPHA |
    //               GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    //
    // Das ist Alphamischung. Bei einem Bild mit schwarzem Rand sieht additiv
    // fast gleich aus, deshalb ist es lange nicht aufgefallen — bei einem mit
    // hellem oder komprimiertem Rand erscheint das Viereck als Kasten.
    return shader::BlendMode::AlphaBlend;
}

bool Index::hasShader(const std::string& name) const {
    return std::binary_search(shaders.begin(), shaders.end(), toLower(name));
}

bool Index::hasTexture(const std::string& name) const {
    return std::binary_search(textures.begin(), textures.end(), toLower(name));
}

namespace {

void addName(Index& index, const std::string& relativePath,
             const std::string& source = {}) {
    const Kind kind = classify(relativePath);
    if (kind == Kind::Ignore || kind == Kind::Shader) return;
    const std::string name = normaliseName(relativePath, kind);
    switch (kind) {
        case Kind::Texture: index.textures.push_back(name); break;
        case Kind::Model:   index.models.push_back(name);   break;
        case Kind::Sound:   index.sounds.push_back(name);   break;
        case Kind::Effect:
            index.effects.push_back(name);
            // Woher — fuer den Quellenfilter im Browser. Nur bei Effekten:
            // bei Texturen waeren das zwanzigtausend Eintraege fuer eine
            // Frage, die niemand stellt.
            if (!source.empty()) index.effectSources.emplace_back(name, source);
            break;
        default: break;
    }
}

void sortUnique(std::vector<std::string>& list) {
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
}

}  // namespace

Index scanArchive(const std::string& archivePath) {
    Index index;
    std::string error;
    const auto names = readZipDirectory(archivePath, &error);
    if (names.empty()) {
        index.notes.push_back(error.empty() ? "archive is empty" : error);
        return index;
    }
    index.archives.push_back(archivePath);
    index.pk3Count = 1;
    index.filesSeen = static_cast<int>(names.size());

    shader::Library library;
    for (const auto& name : names) {
        if (classify(name) == Kind::Shader) {
            // Shadernamen stehen im Inhalt, nicht im Dateinamen — also lesen.
            const auto bytes = readFromZip(archivePath, name);
            if (bytes.empty()) continue;
            shader::parseInto(library, std::string(bytes.begin(), bytes.end()), name);
            ++index.shaderFilesRead;
        } else {
            addName(index, name, archivePath);
        }
    }
    for (const auto& entry : library.shaders) {
        index.shaders.push_back(toLower(entry.name));
        for (const auto& stage : entry.stages) {
            // Nicht nur `map`.
            //
            // Hier stand `if (!stage.map.empty() ...)` — und damit fiel jeder
            // Shader durch, dessen erste Stufe eine Bildfolge oder ein
            // geklemmtes Bild benutzt:
            //
            //     gfx/exp/rocket_explosion { oneshotanimmap 6 gfx/exp/rocket_1.tga ... }
            //
            // `map` ist dort leer, es gab also keinen Eintrag, und findTexture
            // fiel auf "der Shadername ist der Bildname" zurueck. Eine Datei
            // `gfx/exp/rocket_explosion.tga` gibt es nicht — die Textur galt
            // als fehlend, und gezeichnet wurde der weiche Ersatzfleck. Bei
            // additiver Mischung saettigt der zu Weiss: aus einer Explosion
            // wurde ein weisser Klotz.
            //
            // Gemessen an einem echten Bestand: 17 von 531 Shadern beginnen
            // mit animMap, 41 mit clampMap. Jeder zwoelfte war betroffen.
            //
            // `Shader::previewImage()` beantwortet genau diese Frage schon —
            // es gab sie hier nur ein zweites Mal, kuerzer und falsch.
            const std::string* image = nullptr;
            if (!stage.map.empty() && stage.map[0] != '$') {
                image = &stage.map;
            } else if (!stage.clampMap.empty()) {
                image = &stage.clampMap;
            } else if (!stage.animMaps.empty()) {
                image = &stage.animMaps.front();
            }
            if (image) {
                index.shaderMaps.emplace_back(toLower(entry.name), *image);
                index.shaderBlends.emplace_back(
                    toLower(entry.name),
                    shader::blendModeOf(stage.srcBlend, stage.dstBlend));
                // Bildfolge? Dann alle Bilder merken, nicht nur das erste.
                // Die tcMod-Zeilen mitnehmen: ohne sie steht eine
                // scrollende Textur still.
                if (!stage.texMods.empty()) {
                    index.shaderTexMods.emplace_back(toLower(entry.name),
                                                     stage.texMods);
                }
                // `rgbGen wave` mitnehmen: es gibt die Helligkeit vor und
                // ersetzt damit die Farbe aus der .efx. Steht 135-mal in einer
                // gewoehnlichen Installation.
                if (stage.rgbGen == shader::ColorGen::Wave &&
                    !stage.rgbWave.func.empty()) {
                    index.shaderRgbWaves.emplace_back(toLower(entry.name),
                                                      stage.rgbWave);
                }
                if (stage.alphaGen == shader::AlphaGen::Wave &&
                    !stage.alphaWave.func.empty()) {
                    index.shaderAlphaWaves.emplace_back(toLower(entry.name),
                                                        stage.alphaWave);
                }
                if (stage.animMaps.size() > 1) {
                    Index::AnimatedShader anim;
                    anim.frames = stage.animMaps;
                    anim.framesPerSecond = stage.animFrequency;
                    anim.oneShot = stage.animOneShot;
                    index.shaderAnims.emplace_back(toLower(entry.name),
                                                   std::move(anim));
                }
                break;
            }
        }
    }
    sortUnique(index.shaders);
    sortUnique(index.textures);
    sortUnique(index.models);
    sortUnique(index.sounds);
    sortUnique(index.effects);
    return index;
}


Index scan(const std::string& basePath, jobs::Pool* pool,
           jobs::Cancellation* cancel) {
    Index index;
    std::error_code ec;
    const fs::path base(basePath);
    if (basePath.empty() || !fs::is_directory(base, ec)) {
        index.notes.push_back("base folder not found");
        return index;
    }

    // Erst die ausgepackten Dateien. Rekursiv, aber mit Obergrenze — ein
    // versehentlich gewaehlter Ordner wie C:\ soll den Editor nicht minutenlang
    // beschaeftigen.
    constexpr int kMaxFiles = 400000;
    std::vector<fs::path> pk3Files;
    std::vector<fs::path> shaderFiles;

    for (fs::recursive_directory_iterator it(base, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (cancel && cancel->cancelled()) {
            index.notes.push_back("cancelled");
            return index;
        }
        if (++index.filesSeen > kMaxFiles) {
            index.notes.push_back("stopped after " + std::to_string(kMaxFiles) +
                                  " files");
            break;
        }
        if (!it->is_regular_file(ec)) continue;

        const std::string relative =
            fs::relative(it->path(), base, ec).generic_string();
        if (ec) continue;

        if (extensionOf(relative) == ".pk3") {
            pk3Files.push_back(it->path());
            continue;
        }
        if (classify(relative) == Kind::Shader) {
            shaderFiles.push_back(it->path());
            continue;
        }
        addName(index, relative, base.string());
    }
    index.pk3Count = static_cast<int>(pk3Files.size());
    for (const auto& archive : pk3Files) index.archives.push_back(archive.string());

    // Dann die .pk3-Dateien. Jede kann ein eigener Faden lesen — das ist der
    // Fall, fuer den der Arbeitsverteiler gebaut wurde: zwanzig Dateien, jede
    // ein paar Megabyte, und nur das Verzeichnis am Ende wird gebraucht.
    std::mutex mutex;
    std::vector<std::string> packedShaderPaths;

    auto readOnePk3 = [&](size_t i) {
        std::string error;
        const auto names = readZipDirectory(pk3Files[i].string(), &error);

        std::lock_guard<std::mutex> lock(mutex);
        if (!error.empty()) {
            index.notes.push_back(pk3Files[i].filename().string() + ": " + error);
            return;
        }
        for (const auto& name : names) {
            if (classify(name) == Kind::Shader) {
                packedShaderPaths.push_back(pk3Files[i].string() + "|" + name);
            } else {
                addName(index, name, pk3Files[i].string());
            }
        }
    };

    if (pool && pk3Files.size() > 1) {
        pool->parallelFor(pk3Files.size(), 1, [&](size_t from, size_t to) {
            for (size_t i = from; i < to; ++i) {
                if (cancel && cancel->cancelled()) return;
                readOnePk3(i);
            }
        });
    } else {
        for (size_t i = 0; i < pk3Files.size(); ++i) readOnePk3(i);
    }

    // Zuletzt die .shader-Dateien. Nur diese muessen tatsaechlich gelesen
    // werden, weil die Shadernamen im Inhalt stehen und nicht im Dateinamen.
    shader::Library library;
    for (const auto& path : shaderFiles) {
        if (cancel && cancel->cancelled()) break;
        std::ifstream file(path);
        if (!file) continue;
        const std::string text((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        shader::parseInto(library, text, path.filename().string());
        ++index.shaderFilesRead;
    }
    // Shader aus den Archiven. Bis eben blieben sie ungelesen, weil ihre
    // Namen im Dateiinhalt stehen und nicht im Dateinamen — jetzt geht es.
    for (const auto& entry : packedShaderPaths) {
        if (cancel && cancel->cancelled()) break;
        const size_t bar = entry.rfind('|');
        if (bar == std::string::npos) continue;
        const std::string archive = entry.substr(0, bar);
        const std::string inner = entry.substr(bar + 1);

        std::string error;
        const auto bytes = readFromZip(archive, inner, &error);
        if (bytes.empty()) {
            if (!error.empty()) index.notes.push_back(inner + ": " + error);
            continue;
        }
        const std::string text(bytes.begin(), bytes.end());
        shader::parseInto(library, text, inner);
        ++index.shaderFilesRead;
    }

    // Erst jetzt einsammeln — vorher fehlten die aus den Archiven.
    for (const auto& entry : library.shaders) {
        index.shaders.push_back(toLower(entry.name));
        // Die erste Stufe mit einer echten Bilddatei. `$whiteimage` und
        // `$lightmap` sind Sonderwerte der Engine und fuehren zu keiner Datei.
        for (const auto& stage : entry.stages) {
            // Nicht nur `map`.
            //
            // Hier stand `if (!stage.map.empty() ...)` — und damit fiel jeder
            // Shader durch, dessen erste Stufe eine Bildfolge oder ein
            // geklemmtes Bild benutzt:
            //
            //     gfx/exp/rocket_explosion { oneshotanimmap 6 gfx/exp/rocket_1.tga ... }
            //
            // `map` ist dort leer, es gab also keinen Eintrag, und findTexture
            // fiel auf "der Shadername ist der Bildname" zurueck. Eine Datei
            // `gfx/exp/rocket_explosion.tga` gibt es nicht — die Textur galt
            // als fehlend, und gezeichnet wurde der weiche Ersatzfleck. Bei
            // additiver Mischung saettigt der zu Weiss: aus einer Explosion
            // wurde ein weisser Klotz.
            //
            // Gemessen an einem echten Bestand: 17 von 531 Shadern beginnen
            // mit animMap, 41 mit clampMap. Jeder zwoelfte war betroffen.
            //
            // `Shader::previewImage()` beantwortet genau diese Frage schon —
            // es gab sie hier nur ein zweites Mal, kuerzer und falsch.
            const std::string* image = nullptr;
            if (!stage.map.empty() && stage.map[0] != '$') {
                image = &stage.map;
            } else if (!stage.clampMap.empty()) {
                image = &stage.clampMap;
            } else if (!stage.animMaps.empty()) {
                image = &stage.animMaps.front();
            }
            if (image) {
                index.shaderMaps.emplace_back(toLower(entry.name), *image);
                index.shaderBlends.emplace_back(
                    toLower(entry.name),
                    shader::blendModeOf(stage.srcBlend, stage.dstBlend));
                // Bildfolge? Dann alle Bilder merken, nicht nur das erste.
                // Die tcMod-Zeilen mitnehmen: ohne sie steht eine
                // scrollende Textur still.
                if (!stage.texMods.empty()) {
                    index.shaderTexMods.emplace_back(toLower(entry.name),
                                                     stage.texMods);
                }
                // `rgbGen wave` mitnehmen: es gibt die Helligkeit vor und
                // ersetzt damit die Farbe aus der .efx. Steht 135-mal in einer
                // gewoehnlichen Installation.
                if (stage.rgbGen == shader::ColorGen::Wave &&
                    !stage.rgbWave.func.empty()) {
                    index.shaderRgbWaves.emplace_back(toLower(entry.name),
                                                      stage.rgbWave);
                }
                if (stage.alphaGen == shader::AlphaGen::Wave &&
                    !stage.alphaWave.func.empty()) {
                    index.shaderAlphaWaves.emplace_back(toLower(entry.name),
                                                        stage.alphaWave);
                }
                if (stage.animMaps.size() > 1) {
                    Index::AnimatedShader anim;
                    anim.frames = stage.animMaps;
                    anim.framesPerSecond = stage.animFrequency;
                    anim.oneShot = stage.animOneShot;
                    index.shaderAnims.emplace_back(toLower(entry.name),
                                                   std::move(anim));
                }
                break;
            }
        }
    }

    sortUnique(index.shaders);
    sortUnique(index.textures);
    sortUnique(index.models);
    sortUnique(index.sounds);
    sortUnique(index.effects);
    return index;
}

}  // namespace efx::assets
