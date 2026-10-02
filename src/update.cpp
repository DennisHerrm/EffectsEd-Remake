// Siehe include/efx/update.h.
#include "efx/update.h"

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace efx::update {
namespace {

// Ein kleiner JSON-Leser.
//
// Gebraucht werden nur ein paar Felder einer GitHub-Antwort. Statt einer
// Bibliothek ein Leser, der die ganze Grammatik versteht — sonst verliert er
// bei verschachtelten Objekten den Faden —, aber nur Zeichenketten, Zahlen
// und die Struktur festhaelt.
struct Value {
    enum class Kind { Null, Bool, Number, Text, List, Object } kind = Kind::Null;
    std::string text;
    double number = 0.0;
    std::vector<Value> list;
    std::vector<std::pair<std::string, Value>> object;

    [[nodiscard]] const Value* field(const std::string& key) const {
        for (const auto& [name, value] : object) {
            if (name == key) return &value;
        }
        return nullptr;
    }
    [[nodiscard]] std::string textOf(const std::string& key) const {
        const Value* value = field(key);
        return (value != nullptr && value->kind == Kind::Text) ? value->text : std::string();
    }
};

class Reader {
public:
    explicit Reader(const std::string& s) : s_(s) {}

    bool read(Value& v) {
        skipSpace();
        if (i_ >= s_.size()) return false;
        const char c = s_[i_];
        if (c == '{') return readObject(v);
        if (c == '[') return readList(v);
        if (c == '"') {
            v.kind = Value::Kind::Text;
            return readText(v.text);
        }
        if (c == 't' || c == 'f') return readBool(v);
        if (c == 'n') {
            v.kind = Value::Kind::Null;
            return expect("null");
        }
        return readNumber(v);
    }
    bool atEnd() {
        skipSpace();
        return i_ >= s_.size();
    }

private:
    const std::string& s_;
    std::size_t i_ = 0;
    int depth_ = 0;

    void skipSpace() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_])) != 0) ++i_;
    }
    bool expect(const char* word) {
        for (std::size_t k = 0; word[k] != '\0'; ++k, ++i_) {
            if (i_ >= s_.size() || s_[i_] != word[k]) return false;
        }
        return true;
    }
    bool readBool(Value& v) {
        v.kind = Value::Kind::Bool;
        if (s_[i_] == 't') {
            v.number = 1.0;
            return expect("true");
        }
        return expect("false");
    }
    bool readNumber(Value& v) {
        const std::size_t start = i_;
        while (i_ < s_.size() &&
               (std::isdigit(static_cast<unsigned char>(s_[i_])) != 0 || s_[i_] == '-' ||
                s_[i_] == '+' || s_[i_] == '.' || s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
        }
        if (i_ == start) return false;
        v.kind = Value::Kind::Number;
        v.number = std::strtod(s_.substr(start, i_ - start).c_str(), nullptr);
        return true;
    }
    static void appendUtf8(unsigned cp, std::string& out) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    bool readHex4(unsigned& cp) {
        if (i_ + 4 > s_.size()) return false;
        cp = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s_[i_++];
            cp <<= 4;
            if (c >= '0' && c <= '9') cp |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') cp |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') cp |= static_cast<unsigned>(c - 'A' + 10);
            else return false;
        }
        return true;
    }
    bool readText(std::string& out) {
        ++i_;  // "
        while (i_ < s_.size()) {
            const char c = s_[i_++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i_ >= s_.size()) return false;
            const char e = s_[i_++];
            switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!readHex4(cp)) return false;
                    // Ersatzpaar: ein Zeichen ausserhalb der Grundebene
                    // (etwa ein Emoji in den Release-Notizen).
                    if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 1 < s_.size() && s_[i_] == '\\' &&
                        s_[i_ + 1] == 'u') {
                        i_ += 2;
                        unsigned low = 0;
                        if (!readHex4(low)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    }
                    appendUtf8(cp, out);
                    break;
                }
                default: out += e; break;  // \" \\ \/
            }
        }
        return false;
    }
    bool readList(Value& v) {
        if (++depth_ > 64) return false;
        v.kind = Value::Kind::List;
        ++i_;
        skipSpace();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            --depth_;
            return true;
        }
        while (true) {
            Value child;
            if (!read(child)) return false;
            v.list.push_back(std::move(child));
            skipSpace();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                --depth_;
                return true;
            }
            return false;
        }
    }
    bool readObject(Value& v) {
        if (++depth_ > 64) return false;
        v.kind = Value::Kind::Object;
        ++i_;
        skipSpace();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            --depth_;
            return true;
        }
        while (true) {
            skipSpace();
            if (i_ >= s_.size() || s_[i_] != '"') return false;
            std::string key;
            if (!readText(key)) return false;
            skipSpace();
            if (i_ >= s_.size() || s_[i_] != ':') return false;
            ++i_;
            Value child;
            if (!read(child)) return false;
            v.object.emplace_back(std::move(key), std::move(child));
            skipSpace();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                --depth_;
                return true;
            }
            return false;
        }
    }
};

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

bool parseRelease(const std::string& json, Release& out, std::string* error) {
    out = Release{};
    Value root;
    Reader reader(json);
    if (!reader.read(root) || !reader.atEnd() || root.kind != Value::Kind::Object) {
        if (error != nullptr) *error = "not a valid JSON object";
        return false;
    }
    out.tag = root.textOf("tag_name");
    out.name = root.textOf("name");
    out.body = root.textOf("body");
    out.htmlUrl = root.textOf("html_url");
    if (const Value* list = root.field("assets"); list != nullptr && list->kind == Value::Kind::List) {
        for (const Value& entry : list->list) {
            if (entry.kind != Value::Kind::Object) continue;
            Asset asset;
            asset.name = entry.textOf("name");
            asset.downloadUrl = entry.textOf("browser_download_url");
            if (const Value* size = entry.field("size");
                size != nullptr && size->kind == Value::Kind::Number) {
                asset.size = static_cast<long long>(size->number);
            }
            out.assets.push_back(std::move(asset));
        }
    }
    if (out.tag.empty()) {
        if (error != nullptr) {
            const std::string message = root.textOf("message");
            *error = message.empty() ? std::string("answer without tag_name") : message;
        }
        return false;
    }
    return true;
}

int revisionOf(const std::string& text) {
    const std::string s = lower(text);
    const std::size_t at = s.rfind("rev");
    if (at == std::string::npos) return -1;
    std::size_t i = at + 3;
    if (i >= s.size() || std::isdigit(static_cast<unsigned char>(s[i])) == 0) return -1;
    int n = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) != 0 && n < 100000000) {
        n = n * 10 + (s[i] - '0');
        ++i;
    }
    return n;
}

bool isNewer(const std::string& tag, const std::string& local) {
    const int theirs = revisionOf(tag);
    const int ours = revisionOf(local);
    return theirs >= 0 && ours >= 0 && theirs > ours;
}

const Asset* zipAsset(const Release& release) {
    for (const Asset& asset : release.assets) {
        const std::string n = lower(asset.name);
        if (n.size() > 4 && n.compare(n.size() - 4, 4, ".zip") == 0) return &asset;
    }
    return nullptr;
}

std::string commonFolder(const std::vector<std::string>& entries) {
    std::string top;
    for (std::string entry : entries) {
        // Windows PowerShell 5.1 (Compress-Archive) schreibt Rueckstriche in
        // die Eintragsnamen. Ohne das hier gaebe es keinen gemeinsamen Ordner,
        // und das Update landete in einem Unterordner statt auf der .exe.
        for (char& c : entry) {
            if (c == '\\') c = '/';
        }
        const std::size_t slash = entry.find('/');
        // Eine Datei ganz oben: kein gemeinsamer Ordner.
        if (slash == std::string::npos) return {};
        const std::string first = entry.substr(0, slash + 1);
        if (top.empty()) {
            top = first;
        } else if (first != top) {
            return {};
        }
    }
    return top;
}

std::string targetInFolder(const std::string& entry, const std::string& topFolder) {
    std::string p = entry;
    for (char& c : p) {
        if (c == '\\') c = '/';
    }
    if (!topFolder.empty() && p.compare(0, topFolder.size(), topFolder) == 0) {
        p = p.substr(topFolder.size());
    }
    if (p.empty() || p.back() == '/') return {};                           // ein Ordner
    if (p.front() == '/' || (p.size() > 1 && p[1] == ':')) return {};      // absolut
    std::size_t start = 0;
    while (start <= p.size()) {
        const std::size_t slash = p.find('/', start);
        const std::string part = p.substr(start, (slash == std::string::npos ? p.size() : slash) - start);
        if (part == ".." || part == "." || part.empty()) return {};
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    // Was dem Anwender gehoert, bleibt: efxed_settings.txt, efxed_gui.ini,
    // efxed_portable.txt, die Protokolle.
    const std::string name = lower(p.substr(p.rfind('/') == std::string::npos ? 0 : p.rfind('/') + 1));
    if (name.rfind("efxed_", 0) == 0) return {};
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".log") == 0) return {};
    return p;
}

}  // namespace efx::update
