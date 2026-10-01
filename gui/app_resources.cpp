// Grafik- und Materialressourcen: Texturen, Materialbestand, Diagnose.
//
// Getrennt, weil hier die einzige Stelle liegt, an der das Programm
// Ressourcen des Renderers besitzt — und deren Lebensdauer ist die
// heikelste Frage im ganzen Oberflaechenteil.
//
// Teil der Klasse App aus app.h — dieselbe Klasse, nach Aufgaben auf
// mehrere Dateien verteilt. app.cpp war mit 3524 Zeilen und einem Dutzend
// Zustaendigkeiten die Stelle, an der ein Leser aufgibt.
#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>
#include "app.h"
#include "efx/diag.h"
#include "efx/i18n.h"
#include "efx/jobs.h"
namespace efx::gui {

using i18n::Str;
using i18n::tr;

void App::clearTextures(render::Renderer* renderer) {
    if (renderer) {
        for (const auto& entry : textureCache_) {
            if (entry.second != render::kNoTexture) {
                renderer->destroyTexture(entry.second);
            }
        }
    }
    textureCache_.clear();
    texturesLoaded_ = 0;
    texturesMissing_ = 0;
    // MUSS mitgeloescht werden.
    //
    // Genau das fehlte: der Zaehler wurde zurueckgesetzt, die Namensliste
    // nicht. Danach stand in der Kopfzeile "3 Texturen nicht gefunden", und
    // darunter lagen sieben Namen, zwei davon doppelt — weil nach dem Leeren
    // des Zwischenspeichers dieselbe Textur ein zweites Mal gesucht und ein
    // zweites Mal vermerkt wurde.
    missingTextures_.clear();
}

void App::forgetGraphicsResources() {
    // Erst die Arbeitsfaeden zur Ruhe bringen: sie halten `this` und liefern
    // Bildpunkte fuer einen Renderer, den es gleich nicht mehr gibt.
    settleTextureJobs();

    // Nicht freigeben — der Renderer, dem sie gehoerten, ist schon weg.
    textureCache_.clear();
    textureCacheDirty_ = false;
    texturesLoaded_ = 0;
    texturesMissing_ = 0;
    // MUSS mitgeloescht werden.
    //
    // Genau das fehlte: der Zaehler wurde zurueckgesetzt, die Namensliste
    // nicht. Danach stand in der Kopfzeile "3 Texturen nicht gefunden", und
    // darunter lagen sieben Namen, zwei davon doppelt — weil nach dem Leeren
    // des Zwischenspeichers dieselbe Textur ein zweites Mal gesucht und ein
    // zweites Mal vermerkt wurde.
    missingTextures_.clear();
    wallTexture_ = render::kNoTexture;
    wallTextureKind_ = -1;   // erzwingt das Neubauen der Wandtextur

    // Der Ersatzfleck fuer fehlende Bilder — er gehoerte dem alten Renderer.
    //
    // Das war das Loch: unter Direct3D ist eine Texturkennung ein ZEIGER auf
    // eine ID3D11ShaderResourceView. Nach dem Wechsel zeigt sie auf
    // freigegebenen Speicher, und der erste Zeichenaufruf damit stuerzt ab.
    // Genau deshalb gibt es diese Funktion; ich habe den Fleck eingebaut und
    // vergessen, ihn hier einzutragen.
    fallbackTexture_ = render::kNoTexture;

    // Das Vorschaublatt liegt im Ansichtsziel des alten Renderers. Das neue
    // ist leer, also muss jede Kachel neu gezeichnet werden — sonst bleiben
    // sie stehen und zeigen nichts oder Unbestimmtes.
    browserCellOwner_.clear();
    browserSheetColumns_ = 0;
    browserSheetWidth_ = 0;
    browserSheetHeight_ = 0;
    browserHovered_ = nullptr;

    geometryDirty_ = true;
    diag::info("graphics resources forgotten (renderer change)");
}

void App::rescanAssets() {
    // Vom Pruefer gefunden: auch dieser Zweig ersetzt den Bestand. Er sieht
    // harmlos aus — "kein Pfad, also nichts zu tun" — leert aber genau die
    // Datenstruktur, die die Arbeitsfaeden gerade lesen.
    settleTextureJobs();
    if (settings_.allGamePaths().empty()) {
        assets_ = assets::Index{};
        assetsScanned_ = false;
        return;
    }
    diag::Step step("Scan game assets");
    // Alle Pfade in der Suchreihenfolge, nicht nur den aktiven.
    // Der Bestand wird gleich ERSETZT — die Arbeitsfaeden lesen ihn gerade.
    settleTextureJobs();
    assets_ = assets::scanAll(settings_.allGamePaths(), &jobs::pool());
    // Der Bestand hat sich geaendert; was zwischengespeichert ist, kann
    // veraltet sein.
    //
    // Hier nur vormerken, nicht loeschen: zum Freigeben braucht es den
    // Renderer, und der kommt an dieser Stelle nicht her. Die Zuordnung
    // einfach zu leeren waere ein Leck — die Texturen liegen dann weiter im
    // Grafikspeicher, ohne dass jemand sie noch kennt.
    textureCacheDirty_ = true;
    childEffects_.clear();
    // Gelesene Modelle bleiben (laufende Systeme zeigen darauf, siehe
    // modelCache_), nur die Fehlversuche werden vergessen.
    for (auto it = modelCache_.begin(); it != modelCache_.end();) {
        it = it->second ? std::next(it) : modelCache_.erase(it);
    }
    // Der Browser zeigt, was der Bestand hergibt — aendert der sich, muss er
    // neu gefuellt werden. Die laufenden Vorschauen gehen dabei verloren, und
    // das ist richtig: sie zeigten Dateien aus dem alten Bestand.
    browserEntries_.clear();
    assetsScanned_ = true;
    diag::info("assets: " + std::to_string(assets_.shaders.size()) + " shaders, " +
               std::to_string(assets_.textures.size()) + " textures, " +
               std::to_string(assets_.models.size()) + " models, " +
               std::to_string(assets_.sounds.size()) + " sounds, " +
               std::to_string(assets_.effects.size()) + " effects, from " +
               std::to_string(assets_.pk3Count) + " pk3 files");
}

void App::refreshDiagnostics() {
    // Neu aufbauen, nicht anhaengen. Beim Anhaengen wuchs die Liste mit jeder
    // Feldaenderung, und nach hundert Aenderungen stand dieselbe Warnung
    // hundertmal da — bei einem Aufruf je Tastendruck geht das schnell.
    doc().diagnostics = doc().parseDiagnostics;

    const Dialect target = Dialect::Both;  // spaeter aus den Einstellungen
    for (const auto& d : validate(doc().effect, target)) doc().diagnostics.push_back(d);

    // Und die Pruefung gegen den Bestand: verweist der Effekt auf Shader oder
    // Bilder, die es gar nicht gibt?
    //
    // Sie war laengst geschrieben und geprueft, aber nie angeschlossen — die
    // Oberflaeche fuehrt keine `shader::Library`, sondern den Bestand. Bis
    // jetzt fiel ein Tippfehler im Shadernamen erst beim Abspielen auf, als
    // weisser Fleck.
    if (assetsScanned_) {
        const auto known = [this](const std::string& name) {
            if (assets_.hasShader(name)) return true;
            // Kein Shaderblock heisst nicht "fehlt": die Engine baut sich aus
            // einer gleichnamigen Bilddatei einen Ersatzshader.
            return assets::findTexture(assets_, settings_.gamePath, name).found;
        };
        for (const auto& d : validateShaderNames(doc().effect, known)) {
            doc().diagnostics.push_back(d);
        }

        // Der STILLE Fall: das Bild ist da, ein Shaderblock aber nicht.
        //
        // Dann meldet die Pruefung oben nichts — es fehlt ja nichts —, und die
        // Mischung wird geraten. Die Engine tut dasselbe (`RE_RegisterShader`
        // meldet mit `lightmaps2d` an, R_FindShader nimmt Alphamischung), also
        // ist es kein Fehler.
        //
        // Sichtbar ist es trotzdem: additiv addieren sich uebereinander-
        // liegende Flammen zu einem hellen Kern auf, alphagemischt nicht. Ein
        // Feuer, dessen Shader nicht gefunden wird, sieht deshalb flau aus —
        // und man sucht den Fehler beim Effekt statt beim Spielpfad.
        //
        // In `effects.shader` steht fuer gfx/effects/fire2 zum Beispiel
        // `blendFunc GL_ONE GL_ONE`. Fehlt die Datei im Suchpfad, faellt genau
        // dieses Aufaddieren weg.
        std::set<std::string> alreadySaid;
        for (const Primitive& prim : doc().effect.primitives) {
            for (const std::string& name : prim.shaders) {
                if (name.empty() || assets_.hasShader(name)) continue;
                if (!assets::findTexture(assets_, settings_.gamePath, name).found) {
                    continue;   // fehlt ganz — das meldet die Pruefung oben
                }
                if (!alreadySaid.insert(name).second) continue;
                Diagnostic d;
                d.severity = Severity::Info;
                d.message = name + ": " + tr(Str::ShaderNoBlock);
                doc().diagnostics.push_back(d);
            }
        }
    }
}


// Welche Mischart fuer diesen Shader?
//
// Kennt der Bestand einen Shaderblock, gilt der — der weiss es genau. Kennt er
// keinen, muss geraten werden, und dann entscheidet das Segment mit: `useAlpha`
// heisst "Bild mit Alphakanal", und solche Bilder werden alphagemischt.
//
// Ohne diese Unterscheidung wurde unbekannter Rauch additiv gezeichnet — und
// dunkler Rauch addiert fast nichts. Er fehlte einfach.
shader::BlendMode App::blendFor(const std::string& shaderName,
                                const particles::DrawList& list) const {
    // 1. Steht der Name in einer .shader-Datei? Dann gilt, was dort steht.
    if (assets_.hasShader(shaderName)) return assets_.blendOf(shaderName);

    // 2. Traegt die Primitive `useAlpha`, ist Alphamischung gemeint.
    if (list.alphaShaders.count(shaderName)) return shader::BlendMode::AlphaBlend;

    // 3. Sonst: ein Bild ohne Shaderskript.
    //
    // Das kommt haeufiger vor als gedacht — `gfx/effects/whiteFlare` und
    // `gfx/effects/alpha_smoke` stehen in KEINER der Shaderdateien einer
    // gewoehnlichen Installation. Die Engine baut dafuer einen Ersatzshader,
    // R_FindShader in tr_shader.cpp:
    //
    //     } else if ( shader.lightmapIndex[0] == LIGHTMAP_2D ) {
    //         stages[0].rgbGen   = CGEN_VERTEX;
    //         stages[0].alphaGen = AGEN_VERTEX;
    //         stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
    //               GLS_SRCBLEND_SRC_ALPHA |
    //               GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    //
    // Also ALPHAMISCHUNG, nicht additiv — und genau das war unser Fehler:
    // wir haben additiv geraten. Bei einem Bild mit schwarzem Rand faellt das
    // nicht auf, bei einem mit hellem Rand sieht man das Viereck.
    return shader::BlendMode::AlphaBlend;
}

render::TextureId App::fallbackTexture(render::Renderer* renderer) {
    // Was gezeigt wird, wenn ein Bild fehlt.
    //
    // Vorher war das gar nichts — und „gar nichts" heisst beim Zeichnen: die
    // weisse Ersatztextur. Ein Viereck mit weisser Flaeche und der Farbe der
    // Primitive ergibt ein randscharfes, einfarbiges Rechteck, und bei
    // additiver Mischung saettigt es sofort zu reinem Rot, Gruen, Magenta.
    // Genau diese Kacheln waren im Raster zu sehen.
    //
    // Das ist nicht nur haesslich, es ist irrefuehrend: es sieht nach einem
    // kaputten Effekt aus, obwohl nur eine Datei fehlt.
    //
    // Stattdessen ein weicher runder Fleck. Er zeigt Lage, Groesse und Farbe
    // richtig und sagt durch sein Aussehen: hier war ein Teilchen, nur sein
    // Bild kenne ich nicht.
    if (fallbackTexture_ != render::kNoTexture) return fallbackTexture_;
    if (!renderer) return render::kNoTexture;

    constexpr int kSize = 64;
    std::vector<unsigned char> pixels(static_cast<size_t>(kSize) * kSize * 4);
    const float middle = (kSize - 1) * 0.5f;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float dx = (static_cast<float>(x) - middle) / middle;
            const float dy = (static_cast<float>(y) - middle) / middle;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d > 1.0f) d = 1.0f;
            // Quadratischer Abfall: aussen genau null, damit der Rand des
            // Vierecks nicht zu sehen ist.
            const float fade = (1.0f - d) * (1.0f - d);
            const auto value = static_cast<unsigned char>(fade * 255.0f + 0.5f);
            const size_t at = (static_cast<size_t>(y) * kSize + x) * 4;
            pixels[at + 0] = 255;
            pixels[at + 1] = 255;
            pixels[at + 2] = 255;
            pixels[at + 3] = value;
        }
    }
    fallbackTexture_ = renderer->createTexture(pixels.data(), kSize, kSize, true, false);
    return fallbackTexture_;
}

std::string App::toGameRelative(const std::string& path) const {
    // Aus C:/.../GameData/base/sound/chars/atst/crash1.mp3 wird
    // sound/chars/atst/crash1.mp3.
    //
    // Warum das noetig ist: in einer .efx steht der spielinterne Name, kein
    // Laufwerksbuchstabe. Traegt man den vollen Pfad ein, klingt es im Editor
    // und fehlt im Spiel — ein Fehler, der erst beim Testen der Karte
    // auffaellt und dann schwer zuzuordnen ist.
    if (path.empty()) return {};

    std::string tidy = path;
    for (char& c : tidy) {
        if (c == '\\') c = '/';
    }

    // Alle Spielpfade probieren, laengster zuerst: liegt ein Pfad unter einem
    // anderen, gewinnt der genauere.
    std::vector<std::string> roots = settings_.allGamePaths();
    std::sort(roots.begin(), roots.end(),
              [](const std::string& a, const std::string& b) {
                  return a.size() > b.size();
              });
    for (std::string root : roots) {
        for (char& c : root) {
            if (c == '\\') c = '/';
        }
        while (!root.empty() && root.back() == '/') root.pop_back();
        if (root.empty() || tidy.size() <= root.size() + 1) continue;
        // Gross- und Kleinschreibung ist unter Windows egal.
        // Gross- und Kleinschreibung selbst vergleichen: assets:: bietet das
        // nicht nach aussen an, und dafuer einen Kopf zu oeffnen waere zu viel
        // fuer drei Zeilen.
        bool samePrefix = true;
        for (size_t i = 0; i < root.size() && samePrefix; ++i) {
            const char a = static_cast<char>(std::tolower(
                static_cast<unsigned char>(tidy[i])));
            const char b = static_cast<char>(std::tolower(
                static_cast<unsigned char>(root[i])));
            if (a != b) samePrefix = false;
        }
        if (samePrefix && tidy[root.size()] == '/') {
            return tidy.substr(root.size() + 1);
        }
    }
    return {};
}

void App::settleTextureJobs() {
    // Warten, bis kein Texturauftrag mehr laeuft, und die Ergebnisse abholen.
    //
    // Der Grund ist eine Regel, die man einhalten MUSS und die der Uebersetzer
    // nicht durchsetzt: die Arbeitsfaeden LESEN `assets_` und
    // `settings_.gamePath`. Wer den Bestand neu einliest oder ein Archiv
    // dazunimmt, veraendert genau das — waehrend gelesen wird.
    //
    // Ein Wettlauf auf einem `std::vector`, der gerade umzieht, endet nicht
    // mit falschen Daten, sondern mit einem Absturz an unverstaendlicher
    // Stelle. Deshalb vor jeder Aenderung am Bestand hier durch.
    //
    // Das kostet nichts: der Bestand wird geaendert, wenn der Nutzer einen
    // Pfad setzt oder ein Archiv oeffnet. Ein paar Millisekunden Warten sind
    // dort nicht spuerbar.
    jobs::pool().waitIdle();
    jobs::pool().drainMainQueue(-1);
    readyTextures_.clear();
    texturesInFlight_.clear();
    // Die Effektauftraege lesen `assets_` ebenfalls — sie gehoeren mit
    // abgeraeumt, sonst haelt die Regel nur zur Haelfte.
    readyEffects_.clear();
    effectsInFlight_.clear();
}

void App::collectDecodedTextures(render::Renderer* renderer) {
    if (!renderer || readyTextures_.empty()) return;

    // Hoechstens ein paar je Bild hochladen.
    //
    // Beim Oeffnen eines grossen Effekts kommen zwanzig auf einmal zurueck.
    // Alle in einem Bild anzulegen waere derselbe Haenger wie vorher, nur an
    // einer anderen Stelle — der Gewinn laege dann allein im Dekodieren.
    constexpr size_t kUploadsPerFrame = 4;
    const size_t count = std::min(kUploadsPerFrame, readyTextures_.size());

    for (size_t i = 0; i < count; ++i) {
        const DecodedTexture& ready = readyTextures_[i];
        render::TextureId id = render::kNoTexture;

        if (!ready.found) {
            ++texturesMissing_;
        } else if (ready.picture.ok) {
            id = renderer->createTexture(ready.picture.rgba.data(),
                                         ready.picture.width,
                                         ready.picture.height,
                                         clampImages_.count(ready.name) != 0, true);
            ++texturesLoaded_;
            diag::info("texture " + ready.name + " <- " + ready.path + " (" +
                       std::to_string(ready.picture.width) + "x" +
                       std::to_string(ready.picture.height) + ")");
        } else {
            ++texturesMissing_;
            diag::warn("texture " + ready.name + " (" + ready.path + "): " +
                       (ready.picture.error.empty() ? ready.error
                                                    : ready.picture.error));
        }

        textureCache_[ready.name] = id;
        if (id == render::kNoTexture && missingTextures_.size() < 20 &&
            std::find(missingTextures_.begin(), missingTextures_.end(),
                      ready.name) == missingTextures_.end()) {
            missingTextures_.push_back(ready.name);
        }
        texturesInFlight_.erase(ready.name);
        // Die Kacheln stehen — eine von ihnen zeigt womoeglich noch den
        // Ersatzfleck fuer genau diese Textur. Welche das ist, wissen wir
        // nicht; alle einmal neu zu zeichnen kostet ein Bild und ist die
        // ehrlichere Antwort als eine Buchfuehrung, welche Kachel welchen
        // Shader benutzt.
        browserNeedsRedraw_ = true;
    }
    readyTextures_.erase(readyTextures_.begin(),
                         readyTextures_.begin() + static_cast<ptrdiff_t>(count));
}

render::TextureId App::textureFor(render::Renderer* renderer,
                                  const std::string& shaderName, float seconds) {
    if (!renderer || shaderName.empty()) return fallbackTexture(renderer);

    // Bildfolge? Dann entscheidet die Zeit, welches Bild gilt.
    //
    // Eine Explosion IST eine Bildfolge — `oneshotanimmap 6 rocket_1.tga
    // rocket_2.tga ...`. Wir haben bisher das erste Bild gezeigt und es dabei
    // belassen; im Spiel laeuft sie mit sechs Bildern je Sekunde ab und bleibt
    // auf dem letzten stehen.
    //
    // Zwischengespeichert wird nach dem BILDNAMEN, nicht nach dem Shader:
    // zwei Explosionsshader teilen sich oft dieselben Bilder, und die sollen
    // nur einmal im Grafikspeicher liegen.
    std::string key = shaderName;
    if (const auto* anim = assets_.animOf(shaderName)) {
        const int frame = shader::animFrameAt(
            static_cast<int>(anim->frames.size()), anim->framesPerSecond,
            anim->oneShot, seconds);
        key = anim->frames[static_cast<size_t>(frame)];
    }

    const auto found = textureCache_.find(key);
    if (found != textureCache_.end()) {
        return found->second == render::kNoTexture ? fallbackTexture(renderer)
                                                   : found->second;
    }

    // Noch nicht da — in Auftrag geben und diesmal den Ersatzfleck zeigen.
    //
    // Vorher wurde hier gesucht, ausgepackt und dekodiert, mitten im Bild.
    // Beim Oeffnen eines Effekts kamen zwanzig davon nacheinander, jede rund
    // 20 ms; das Fenster stand eine halbe Sekunde.
    //
    // `texturesInFlight_` verhindert, dass derselbe Name sechzigmal je
    // Sekunde erneut beauftragt wird, bis die erste Antwort kommt.
    requestTexture(key);
    return fallbackTexture(renderer);
}

bool App::textureStillLoading(const std::string& shaderName, float seconds) const {
    if (shaderName.empty()) return false;
    std::string key = shaderName;
    if (const auto* anim = assets_.animOf(shaderName)) {
        const int frame = shader::animFrameAt(static_cast<int>(anim->frames.size()),
                                              anim->framesPerSecond, anim->oneShot, seconds);
        key = anim->frames[static_cast<size_t>(frame)];
    }
    return textureCache_.find(key) == textureCache_.end();
}

void App::requestTexture(const std::string& imageName) {
    if (imageName.empty()) return;
    if (textureCache_.find(imageName) != textureCache_.end()) return;
    // Nicht zweimal beauftragen — sonst geht derselbe Name sechzigmal je
    // Sekunde erneut hinaus, bis die erste Antwort kommt.
    if (!texturesInFlight_.insert(imageName).second) return;

    // Den Pfad KOPIEREN, nicht auf settings_ zeigen: der Faden lebt laenger
    // als dieser Aufruf.
    const std::string base = settings_.gamePath;
    jobs::pool().post([this, imageName, base] {
        auto ready = std::make_shared<DecodedTexture>();
        ready->name = imageName;

        // `assets_` wird hier nur GELESEN. Dass daneben niemand schreibt,
        // stellt settleTextureJobs() sicher — siehe dort.
        const auto where = assets::findTexture(assets_, base, imageName);
        ready->found = where.found;
        if (where.found) {
            ready->path = where.path;
            const auto bytes = assets::readFile(base, where, &ready->error);
            if (!bytes.empty()) {
                ready->picture = image::decode(bytes.data(), bytes.size());
            }
        }

        // Zurueck auf den Hauptfaden. Das Anlegen der Textur gehoert dem
        // Renderer, und der gehoert dem Hauptfaden — hier wird nur in eine
        // Liste gelegt.
        jobs::pool().postToMain([this, ready] {
            readyTextures_.push_back(std::move(*ready));
        });
    });
}

void App::prefetchTextures(const Effect& effect) {
    // Alle Bilder eines Effekts anfordern, SOBALD er geladen ist — nicht erst,
    // wenn er zum ersten Mal gezeichnet wird.
    //
    // Das ist die Kehrseite des Hintergrundladens: beim ersten Zeichnen ist
    // die Textur noch unterwegs, also kommt der Ersatzfleck. Im Editor sah
    // deshalb der ERSTE Durchlauf falsch aus und erst der zweite richtig.
    //
    // Vorgeholt hat der Effekt seine Bilder meist schon, bevor das erste Bild
    // steht: das Laden dauert rund 20 ms, ein Effekt laeuft mehrere hundert.
    for (const auto& primitive : effect.primitives) {
        for (const auto& shaderName : primitive.shaders) {
            if (const auto* anim = assets_.animOf(shaderName)) {
                // Bei einer Bildfolge alle Bilder, nicht nur das erste —
                // sonst stockt sie beim ersten Ablauf Bild fuer Bild.
                for (const auto& frame : anim->frames) requestTexture(frame);
            } else {
                requestTexture(shaderName);
            }
        }
    }
}

}  // namespace efx::gui
