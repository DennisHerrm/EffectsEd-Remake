#include "efx/assets.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>

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

// Pfade vergleichen wie die Engine: FS_PathCmp in files.cpp.
//
// Gross-/Kleinschreibung egal, `\` und `:` gelten als `/`. Wichtig ist das
// Detail, dass FS_PathCmp in GROSSbuchstaben vergleicht: `_` (0x5F) liegt
// damit HINTER allen Buchstaben, waehrend es bei Kleinbuchstaben davor
// laege. `assets_x.pk3` sortiert also nach `assetsz.pk3`. Mit einem
// Vergleich in Kleinbuchstaben waere die Reihenfolge der Archive — und damit,
// welches gewinnt — an genau solchen Namen verkehrt.
int enginePathCompare(const std::string& a, const std::string& b) {
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i <= n; ++i) {
        int c1 = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
        int c2 = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
        if (c1 >= 'a' && c1 <= 'z') c1 -= 'a' - 'A';
        if (c2 >= 'a' && c2 <= 'z') c2 -= 'a' - 'A';
        if (c1 == '\\' || c1 == ':') c1 = '/';
        if (c2 == '\\' || c2 == ':') c2 = '/';
        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
        if (c1 == 0) return 0;
    }
    return 0;
}

// Ein Pfad in einer Form, in der man ihn als Vorsatz vergleichen kann:
// Vorwaertsschraegstriche, klein, ohne Schraegstrich am Ende.
std::string comparablePath(const std::string& path) {
    std::string out = toLower(path);
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    while (!out.empty() && out.back() == '/') out.pop_back();
    return out;
}

// Wie R_FindShader in tr_shader.cpp einen Shadernamen nachschlaegt:
//
//     COM_StripExtension( name, strippedName, sizeof(strippedName) );
//     ...
//     shaderText = FindShaderInShaderText( strippedName );
//
// Eine Endung faellt also weg, bevor ueberhaupt gesucht wird. In zwei
// ausgelieferten Dateien steht `gfx/effects/wcloud.tga` — gemeint und im
// Spiel gefunden ist der Shader `gfx/effects/wcloud` mit seiner Mischung.
// Wir verglichen den Namen roh, fanden keinen Shader und zeichneten das
// nackte Bild mit falscher Mischung.
std::string shaderKey(const std::string& name) {
    std::string key = toLower(name);
    for (char& c : key) {
        if (c == '\\') c = '/';
    }
    const size_t dot = key.rfind('.');
    const size_t slash = key.rfind('/');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        key.erase(dot);
    }
    return key;
}

// Wo gesucht wird, in der Reihenfolge der Suche: ein Archiv oder ein Ordner.
struct Place {
    std::string archive;  // gesetzt: in diesem Archiv
    std::string root;     // sonst: ausgepackt unter diesem Ordner
};

// Die Suchreihenfolge, wie FS_FOpenFileRead (files.cpp) sie abläuft.
//
// FS_AddGameDirectory haengt erst den Ordner selbst in die Suchliste und
// stellt dann jedes Archiv — alphabetisch sortiert — VOR alle bisherigen:
//
//     search->next = fs_searchpaths;  fs_searchpaths = search;
//
// Heraus kommt: das alphabetisch LETZTE Archiv zuerst, dann rueckwaerts bis
// zum ersten, und GANZ ZULETZT die ausgepackten Dateien desselben Ordners.
// (`fs_dirbeforepak` dreht das um; es steht ab Werk auf 0.)
//
// Wir hatten es genau andersherum: ausgepackt vor gepackt, und unter den
// Archiven das erste vor dem letzten. In Jedi Outcast stehen 26 Effekte in
// assets0.pk3 UND assets2.pk3 mit verschiedenem Inhalt — wir nahmen die
// veraltete Fassung.
//
// Mehrere Spielpfade (scanAll) bleiben bei unserer eigenen Regel: der zuerst
// genannte gewinnt, so wie in der Engine `fs_game` vor `base` steht. Innerhalb
// eines Pfads gilt die Engine-Reihenfolge. `index.archives` ist schon in
// Suchreihenfolge (scan legt es so an); hier wird nur jedem Archiv sein
// Ordner zugeordnet. Archive ausserhalb jedes Ordners (ein einzeln
// geoeffnetes .pk3) kommen zuletzt.
std::vector<Place> searchPlaces(const Index& index, const std::string& basePath) {
    std::vector<std::string> roots;
    std::vector<std::string> rootKeys;
    auto addRoot = [&](const std::string& root) {
        if (root.empty()) return;
        const std::string key = comparablePath(root);
        for (const auto& known : rootKeys) {
            if (known == key) return;
        }
        roots.push_back(root);
        rootKeys.push_back(key);
    };
    addRoot(basePath);
    for (const auto& root : index.roots) addRoot(root);

    // Jedes Archiv zu dem Ordner, unter dem es liegt — beim laengsten
    // passenden Vorsatz, falls Ordner ineinander liegen.
    const size_t none = roots.size();
    std::vector<size_t> owner(index.archives.size(), none);
    for (size_t a = 0; a < index.archives.size(); ++a) {
        const std::string archive = comparablePath(index.archives[a]);
        size_t best = none;
        for (size_t r = 0; r < rootKeys.size(); ++r) {
            const std::string& key = rootKeys[r];
            if (archive.size() > key.size() && archive.compare(0, key.size(), key) == 0 &&
                archive[key.size()] == '/' &&
                (best == none || key.size() > rootKeys[best].size())) {
                best = r;
            }
        }
        owner[a] = best;
    }

    std::vector<Place> places;
    for (size_t r = 0; r < roots.size(); ++r) {
        for (size_t a = 0; a < index.archives.size(); ++a) {
            if (owner[a] == r) places.push_back({index.archives[a], {}});
        }
        places.push_back({{}, roots[r]});
    }
    for (size_t a = 0; a < index.archives.size(); ++a) {
        if (owner[a] == none) places.push_back({index.archives[a], {}});
    }
    return places;
}

// Das eingebaute weisse Bild der Engine als TGA.
//
// R_CreateBuiltinImages in tr_image.cpp: `tr.whiteImage = R_CreateImage(
// "*white", data, 8, 8, ...)` mit lauter 255. Eine Shaderstufe `map
// $whiteimage` zeichnet genau dieses Bild (ParseStage in tr_shader.cpp). Als
// TGA, damit es denselben Weg geht wie jede Datei: readFile liefert Bytes,
// image::decode macht ein Bild daraus — der Aufrufer muss nichts wissen.
std::vector<unsigned char> whiteImageTga() {
    constexpr int kSide = 8;
    std::vector<unsigned char> out(18, 0);
    out[2] = 2;                       // unkomprimiert, Echtfarbe
    out[12] = kSide;                  // Breite
    out[14] = kSide;                  // Hoehe
    out[16] = 32;                     // Bit je Bildpunkt
    out[17] = 0x28;                   // oben links beginnend, 8 Bit Alpha
    out.insert(out.end(), static_cast<size_t>(kSide * kSide * 4), 255);
    return out;
}

// Das Ersatzbild der Engine, R_CreateDefaultImage in tr_image.cpp: 16x16,
// alles 32 (dunkelgrau, auch das Alpha), der Rand 255 —
// "the default image will be a box, to allow you to see the mapping coordinates".
std::vector<unsigned char> defaultImageTga() {
    constexpr int kSide = 16;
    std::vector<unsigned char> out(18, 0);
    out[2] = 2;
    out[12] = kSide;
    out[14] = kSide;
    out[16] = 32;
    out[17] = 0x28;
    for (int y = 0; y < kSide; ++y) {
        for (int x = 0; x < kSide; ++x) {
            const bool edge = x == 0 || y == 0 || x == kSide - 1 || y == kSide - 1;
            const unsigned char v = edge ? 255 : 32;
            out.insert(out.end(), {v, v, v, v});
        }
    }
    return out;
}

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

    // Ein Bildname aus einer Shaderstufe (kImagePrefix): keinen Shader fragen.
    const std::string prefix = kImagePrefix;
    const bool imageOnly = name.compare(0, prefix.size(), prefix) == 0;

    // Schritt 1 und 2: kennt der Shaderbestand diesen Namen? Dann die
    // map-Zeile seiner ersten Stufe nehmen.
    std::string candidate = imageOnly ? name.substr(prefix.size()) : index.mapOf(name);
    if (candidate.empty()) {
        // Schritt 3: kein Shader mit dem Namen — der Name gilt direkt als
        // Bildname. Die Engine baut sich daraus einen Ersatzshader.
        candidate = name;
    }
    // Sonderwerte der Engine, hinter denen keine Datei steht. `$lightmap`
    // zeigt bei Effekten ebenfalls das weisse Bild: ParseStage nimmt
    // tr.whiteImage, wenn der Shader keine Lichtkarte hat (lightmapIndex < 0).
    const std::string lower = toLower(candidate);
    if (lower == "$whiteimage" || lower == "$lightmap") {
        // Keine Datei, aber ein Bild: das eingebaute weisse (whiteImageTga).
        // Vorher galt der Shader als bildlos, und die Vorschau zeichnete den
        // weichen Ersatzfleck statt des weissen Vierecks — bei
        // gfx/effects/whiteFlash (GL_ONE GL_ONE) ein ganz anderer Blitz.
        out.path = lower;
        out.white = true;
        out.found = true;
        return out;
    }
    if (lower == "$default") {
        out.path = lower;
        out.defaultImage = true;
        out.found = true;
        return out;
    }
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

    // Schritt 4 und 5: Endungen durchprobieren, an jedem Ort der
    // Suchreihenfolge (searchPlaces): gepackt vor ausgepackt, das spaetere
    // Archiv vor dem frueheren — so wie die Engine.
    //
    // Erst die Endung, dann der Ort: R_FindImageFile probiert eine Endung
    // nach der anderen, und jede einzelne Probe laeuft ueber den ganzen
    // Suchpfad (FS_ReadFile).
    //
    // `basePath` bleibt vorn, damit ein Aufruf mit einem einzelnen Ordner
    // weiter funktioniert; `index.roots` kommt aus scanAll und ist bei einem
    // einzelnen Ordner leer.
    const std::vector<Place> places = searchPlaces(index, basePath);
    // Im Bestand steht der Name ohne Endung. Fuehrt er ihn nicht, braucht
    // kein Archiv geoeffnet zu werden.
    const bool inArchives =
        std::binary_search(index.textures.begin(), index.textures.end(), stem);
    for (const auto& ext : imageExtensions()) {
        const std::string relative = stem + ext;
        for (const auto& place : places) {
            if (place.archive.empty()) {
                if (fs::is_regular_file(fs::path(place.root) / relative, ec)) {
                    out.path = relative;
                    out.root = place.root;
                    out.found = true;
                    return out;
                }
            } else if (inArchives && !readFromZip(place.archive, relative).empty()) {
                out.path = relative;
                out.archive = place.archive;
                out.found = true;
                return out;
            }
        }
    }
    return out;
}

std::string gamePathFromFile(const std::string& path) {
    std::string tidy = path;
    for (char& c : tidy) {
        if (c == '\\') c = '/';
    }
    const std::string lower = toLower(tidy);
    const size_t at = lower.rfind("/base/");
    if (at == std::string::npos) return {};
    return tidy.substr(0, at + 5);
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
    //
    // Steht GAR KEINE Endung da, haengt die Engine vorher `.wav` an:
    //
    //     psExt = &sLoadName[strlen(sLoadName)-4];
    //     if (*psExt != '.')
    //         COM_DefaultExtension(sLoadName, sizeof(sLoadName), ".wav");
    //
    // und dann greift derselbe Rueckfall auf `.mp3`. Wir probierten den
    // nackten Namen und fanden nichts — `sound/weapons/disruptor/hit_wall`
    // in alt_miss.efx spielt im Spiel die .mp3, bei uns blieb es stumm.
    const size_t slash = base.find_last_of('/');
    const size_t lastDot = base.find_last_of('.');
    if (lastDot == std::string::npos ||
        (slash != std::string::npos && lastDot < slash)) {
        base += ".wav";
    }

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

    // Alle Orte in der Suchreihenfolge der Engine (searchPlaces) — fuer jede
    // Endung einmal ganz durch, wie FS_ReadFile es je Versuch tut. Vorher
    // wurde nur `basePath` durchsucht; wer mehrere Spielpfade hat, fand in
    // allen ausser dem ersten nichts.
    const std::vector<Place> places = searchPlaces(index, basePath);
    for (const auto& relative : candidates) {
        // Steht der Name in keinem Archiv, keines oeffnen.
        const bool inArchives =
            std::binary_search(index.sounds.begin(), index.sounds.end(), relative);
        for (const auto& place : places) {
            if (place.archive.empty()) {
                if (fs::is_regular_file(fs::path(place.root) / relative, ec)) {
                    out.path = relative;
                    out.root = place.root;
                    out.found = true;
                    return out;
                }
            } else if (inArchives && !readFromZip(place.archive, relative).empty()) {
                out.path = relative;
                out.archive = place.archive;
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
        // Einmal. Hier stand dieselbe Schleife zweimal; die zweite schob die
        // schon verschobenen Eintraege noch einmal hinterher — mit leerem
        // Namen. Fuer die Suche harmlos, aber doppelt so viele Eintraege.
        for (auto& entry : one.shaderAlphaWaves) {
            combined.shaderAlphaWaves.push_back(std::move(entry));
        }
        for (auto& entry : one.shaderAnims) {
            combined.shaderAnims.push_back(std::move(entry));
        }
        // Erster Ordner zuerst: shaderOf nimmt den ersten Treffer.
        for (auto& entry : one.shaderDefs) {
            combined.shaderDefs.push_back(std::move(entry));
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

    // Suchreihenfolge der Engine (searchPlaces): das spaetere Archiv vor dem
    // frueheren, gepackt vor ausgepackt.
    std::error_code ec;
    const bool inArchives =
        std::binary_search(index.effects.begin(), index.effects.end(), stem);
    for (const auto& place : searchPlaces(index, basePath)) {
        if (place.archive.empty()) {
            if (fs::is_regular_file(fs::path(place.root) / relative, ec)) {
                out.path = relative;
                out.root = place.root;
                out.found = true;
                return out;
            }
        } else if (inArchives && !readFromZip(place.archive, relative).empty()) {
            out.path = relative;
            out.archive = place.archive;
            out.found = true;
            return out;
        }
    }
    return out;
}

ResolvedTexture findModel(const Index& index, const std::string& basePath,
                          const std::string& name) {
    ResolvedTexture out;
    std::string relative = normaliseName(name, Kind::Model);
    if (relative.empty()) return out;
    const size_t slash = relative.find_last_of('/');
    const size_t dot = relative.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        relative += ".md3";
    }

    // Suchreihenfolge wie bei findEffect: das spaetere Archiv vor dem
    // frueheren, gepackt vor ausgepackt.
    std::error_code ec;
    const bool inArchives =
        std::binary_search(index.models.begin(), index.models.end(), relative);
    for (const auto& place : searchPlaces(index, basePath)) {
        if (place.archive.empty()) {
            if (fs::is_regular_file(fs::path(place.root) / relative, ec)) {
                out.path = relative;
                out.root = place.root;
                out.found = true;
                return out;
            }
        } else if (inArchives && !readFromZip(place.archive, relative).empty()) {
            out.path = relative;
            out.archive = place.archive;
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
    if (where.white) return whiteImageTga();
    if (where.defaultImage) return defaultImageTga();
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
    const std::string key = shaderKey(shaderName);
    for (const auto& entry : shaderMaps) {
        if (entry.first == key) return entry.second;
    }
    return {};
}

const shader::Shader* Index::shaderOf(const std::string& shaderName) const {
    // Wie die anderen Abfragen: ohne Endung, klein (R_FindShader).
    const std::string key = shaderKey(shaderName);
    for (const auto& entry : shaderDefs) {
        if (entry.name == key) return &entry;
    }
    return nullptr;
}

const shader::WaveForm* Index::alphaWaveOf(const std::string& shaderName) const {
    const std::string key = shaderKey(shaderName);
    for (const auto& entry : shaderAlphaWaves) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

const shader::WaveForm* Index::rgbWaveOf(const std::string& shaderName) const {
    const std::string key = shaderKey(shaderName);
    for (const auto& entry : shaderRgbWaves) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

const std::vector<shader::TexMod>& Index::texModsOf(
    const std::string& shaderName) const {
    static const std::vector<shader::TexMod> none;
    const std::string key = shaderKey(shaderName);
    for (const auto& entry : shaderTexMods) {
        if (entry.first == key) return entry.second;
    }
    return none;
}

const Index::AnimatedShader* Index::animOf(const std::string& shaderName) const {
    const std::string key = shaderKey(shaderName);
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
    const std::string key = shaderKey(shaderName);
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

// Alle Shader-Abfragen gehen ueber shaderKey: ohne Endung, wie R_FindShader.
bool Index::hasShader(const std::string& name) const {
    return std::binary_search(shaders.begin(), shaders.end(), shaderKey(name));
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

// Eine .shader-Datei, die gelesen werden soll — ausgepackt oder im Archiv.
struct ShaderSource {
    std::string key;      // Pfad im Spiel, klein: "shaders/fx.shader"
    size_t rank = 0;      // Platz in der Suchreihenfolge, 0 = zuerst
    std::string archive;  // leer: ausgepackt
    std::string inner;    // Name im Archiv
    fs::path file;        // die ausgepackte Datei
};

// Welche .shader-Dateien in welcher Reihenfolge gelesen werden.
//
// ScanAndLoadShaderFiles in tr_shader.cpp:
//
//     shaderFiles = ri.FS_ListFiles( "shaders", ".shader", &numShaderFiles );
//     for ( i = 0; i < numShaderFiles; i++ )
//         ri.FS_ReadFile( "shaders/" + shaderFiles[i], &buffers[i] );
//     // free in reverse order, so the temp files are all dumped
//     for ( i = numShaderFiles - 1; i >= 0 ; i-- )
//         strcat( textEnd, buffers[i] );
//
// Drei Regeln stecken darin:
//
//   1. Jeder Dateiname zaehlt einmal (FS_ListFiles liefert ihn einmal), und
//      FS_ReadFile liest die Fassung, die in der Suchreihenfolge zuerst kommt.
//   2. Die Liste ist alphabetisch (FS_SortFileList, mit FS_PathCmp).
//   3. Zusammengehaengt wird RUECKWAERTS, und FindShaderInShaderText nimmt
//      den ersten Treffer. Ein Shader, der in zwei Dateien steht, kommt also
//      aus der alphabetisch LETZTEN.
//
// Wir lasen in Verzeichnisreihenfolge, und die Archive je nach Arbeitsfaden
// in wechselnder Reihenfolge — bei doppelten Shadernamen gewann, wer zuerst
// fertig war.
std::vector<ShaderSource> engineShaderOrder(std::vector<ShaderSource> sources) {
    std::sort(sources.begin(), sources.end(),
              [](const ShaderSource& a, const ShaderSource& b) {
                  const int order = enginePathCompare(a.key, b.key);
                  if (order != 0) return order < 0;
                  return a.rank < b.rank;
              });
    std::vector<ShaderSource> unique;
    for (auto& source : sources) {
        if (!unique.empty() && enginePathCompare(unique.back().key, source.key) == 0) {
            continue;  // dieselbe Datei, aber weiter hinten im Suchpfad
        }
        unique.push_back(std::move(source));
    }
    std::reverse(unique.begin(), unique.end());
    return unique;
}

// Aus den gelesenen Shadern die Tabellen des Bestands bauen.
//
// Stand vorher zweimal, wortgleich, in scan und scanArchive.
void collectShaderInfo(Index& index, const shader::Library& library) {
    // Nur das ERSTE Vorkommen eines Namens zaehlt — so findet die Engine
    // ihn (FindShaderInShaderText, erster Treffer). Ein spaeteres Vorkommen
    // darf auch dann nichts beitragen, wenn das erste keine Bildstufe hat.
    std::set<std::string> seen;
    for (const auto& entry : library.shaders) {
        const std::string name = toLower(entry.name);
        index.shaders.push_back(name);
        if (!seen.insert(name).second) continue;
        // Klein abgelegt: shaderOf vergleicht dann ohne Umwandeln.
        index.shaderDefs.push_back(entry);
        index.shaderDefs.back().name = name;
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
            // `$whiteimage` ist keine Datei, aber ein Bild: das eingebaute
            // weisse (ParseStage in tr_shader.cpp setzt tr.whiteImage).
            // findTexture loest es dorthin auf. `$lightmap` und andere
            // Sonderwerte fuehren weiter zu nichts — die Suche geht zur
            // naechsten Stufe.
            const std::string* image = nullptr;
            if (!stage.map.empty() &&
                (stage.map[0] != '$' || toLower(stage.map) == "$whiteimage")) {
                image = &stage.map;
            } else if (!stage.clampMap.empty()) {
                image = &stage.clampMap;
            } else if (!stage.animMaps.empty()) {
                image = &stage.animMaps.front();
            }
            if (image) {
                index.shaderMaps.emplace_back(name, *image);
                index.shaderBlends.emplace_back(
                    name, shader::blendModeOf(stage.srcBlend, stage.dstBlend));
                // Bildfolge? Dann alle Bilder merken, nicht nur das erste.
                // Die tcMod-Zeilen mitnehmen: ohne sie steht eine
                // scrollende Textur still.
                if (!stage.texMods.empty()) {
                    index.shaderTexMods.emplace_back(name, stage.texMods);
                }
                // `rgbGen wave` mitnehmen: es gibt die Helligkeit vor und
                // ersetzt damit die Farbe aus der .efx. Steht 135-mal in einer
                // gewoehnlichen Installation.
                if (stage.rgbGen == shader::ColorGen::Wave &&
                    !stage.rgbWave.func.empty()) {
                    index.shaderRgbWaves.emplace_back(name, stage.rgbWave);
                }
                if (stage.alphaGen == shader::AlphaGen::Wave &&
                    !stage.alphaWave.func.empty()) {
                    index.shaderAlphaWaves.emplace_back(name, stage.alphaWave);
                }
                if (stage.animMaps.size() > 1) {
                    Index::AnimatedShader anim;
                    anim.frames = stage.animMaps;
                    anim.framesPerSecond = stage.animFrequency;
                    anim.oneShot = stage.animOneShot;
                    index.shaderAnims.emplace_back(name, std::move(anim));
                }
                break;
            }
        }
    }
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

    std::vector<ShaderSource> shaderSources;
    for (const auto& name : names) {
        if (classify(name) == Kind::Shader) {
            // Shadernamen stehen im Inhalt, nicht im Dateinamen — also lesen.
            ShaderSource source;
            source.key = comparablePath(name);
            source.archive = archivePath;
            source.inner = name;
            shaderSources.push_back(std::move(source));
        } else {
            addName(index, name, archivePath);
        }
    }
    shader::Library library;
    for (const auto& source : engineShaderOrder(std::move(shaderSources))) {
        const auto bytes = readFromZip(archivePath, source.inner);
        if (bytes.empty()) continue;
        shader::parseInto(library, std::string(bytes.begin(), bytes.end()), source.inner);
        ++index.shaderFilesRead;
    }
    collectShaderInfo(index, library);
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
    std::vector<ShaderSource> shaderSources;

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
            ShaderSource source;
            source.key = comparablePath(relative);
            source.file = it->path();
            shaderSources.push_back(std::move(source));
            continue;
        }
        addName(index, relative, base.string());
    }

    // Die Archive in die Reihenfolge der Engine bringen.
    //
    // FS_AddGameDirectory sortiert die Dateinamen mit paksort (FS_PathCmp)
    // und stellt jedes Archiv VOR die bisherigen — das alphabetisch letzte
    // wird zuerst durchsucht. `index.archives` steht danach in genau dieser
    // Suchreihenfolge: vorn das, was gewinnt (siehe searchPlaces).
    //
    // Vorher stand hier die Reihenfolge des Verzeichnisdurchlaufs, und vorn
    // gewann assets0.pk3 — das Archiv, das die Engine als LETZTES fragt.
    std::sort(pk3Files.begin(), pk3Files.end(),
              [&](const fs::path& a, const fs::path& b) {
                  std::error_code inner;
                  return enginePathCompare(
                             fs::relative(a, base, inner).generic_string(),
                             fs::relative(b, base, inner).generic_string()) > 0;
              });
    index.pk3Count = static_cast<int>(pk3Files.size());
    for (const auto& archive : pk3Files) index.archives.push_back(archive.string());

    // Dann die .pk3-Dateien. Jede kann ein eigener Faden lesen — das ist der
    // Fall, fuer den der Arbeitsverteiler gebaut wurde: zwanzig Dateien, jede
    // ein paar Megabyte, und nur das Verzeichnis am Ende wird gebraucht.
    std::mutex mutex;

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
                ShaderSource source;
                source.key = comparablePath(name);
                source.rank = i;  // pk3Files steht schon in Suchreihenfolge
                source.archive = pk3Files[i].string();
                source.inner = name;
                shaderSources.push_back(std::move(source));
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

    // Ausgepackte Dateien kommen in der Suchreihenfolge NACH allen Archiven
    // desselben Ordners (siehe searchPlaces).
    for (auto& source : shaderSources) {
        if (source.archive.empty()) source.rank = pk3Files.size();
    }

    // Woher ein Effekt kommt: der erste Eintrag gewinnt (sourceOf). Die
    // Archive wurden womoeglich parallel gelesen, ihre Eintraege stehen also
    // in zufaelliger Folge — hier in die Suchreihenfolge bringen, damit der
    // Browser dieselbe Quelle nennt, aus der die Datei dann geladen wird.
    {
        std::vector<std::pair<std::string, size_t>> rankOf;
        for (size_t i = 0; i < pk3Files.size(); ++i) {
            rankOf.emplace_back(pk3Files[i].string(), i);
        }
        auto rank = [&](const std::string& source) {
            for (const auto& entry : rankOf) {
                if (entry.first == source) return entry.second;
            }
            return pk3Files.size();  // ausgepackt: zuletzt
        };
        std::stable_sort(index.effectSources.begin(), index.effectSources.end(),
                         [&](const auto& a, const auto& b) {
                             if (a.first != b.first) return a.first < b.first;
                             return rank(a.second) < rank(b.second);
                         });
    }

    // Zuletzt die .shader-Dateien. Nur diese muessen tatsaechlich gelesen
    // werden, weil die Shadernamen im Inhalt stehen und nicht im Dateinamen.
    // Reihenfolge und Auswahl wie in der Engine (engineShaderOrder).
    shader::Library library;
    for (const auto& source : engineShaderOrder(std::move(shaderSources))) {
        if (cancel && cancel->cancelled()) break;
        std::string text;
        if (source.archive.empty()) {
            std::ifstream file(source.file);
            if (!file) continue;
            text.assign((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
            shader::parseInto(library, text, source.file.filename().string());
        } else {
            // Shader aus den Archiven. Lange blieben sie ungelesen, weil ihre
            // Namen im Dateiinhalt stehen und nicht im Dateinamen.
            std::string error;
            const auto bytes = readFromZip(source.archive, source.inner, &error);
            if (bytes.empty()) {
                if (!error.empty()) index.notes.push_back(source.inner + ": " + error);
                continue;
            }
            text.assign(bytes.begin(), bytes.end());
            shader::parseInto(library, text, source.inner);
        }
        ++index.shaderFilesRead;
    }

    // Erst jetzt einsammeln — vorher fehlten die aus den Archiven.
    collectShaderInfo(index, library);

    sortUnique(index.shaders);
    sortUnique(index.textures);
    sortUnique(index.models);
    sortUnique(index.sounds);
    sortUnique(index.effects);
    return index;
}

}  // namespace efx::assets
