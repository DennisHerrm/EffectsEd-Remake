// Der Materialbestand: was liegt im Spielordner?
//
// Zwei Quellen, und die zweite ist die wichtigere:
//
//   ausgepackt   Dateien direkt im base-Ordner
//   gepackt      .pk3-Dateien — das ist bei einer normalen Installation
//                praktisch alles
//
// Ein .pk3 ist eine Zip-Datei. Wir lesen nur das Inhaltsverzeichnis am Ende,
// nicht die Dateien selbst: für eine Liste der vorhandenen Shader, Modelle und
// Klänge reichen die Namen, und das Verzeichnis zu lesen kostet einen Bruchteil
// der Zeit, die das Entpacken bräuchte.
//
// Ausnahme: `.shader`-Dateien müssen ausgepackt werden, weil die Shadernamen
// *im* Dateiinhalt stehen, nicht im Dateinamen. Das sind ein paar Dutzend
// kleine Textdateien — der Rest bleibt ungelesen.
#pragma once

#include <string>
#include <vector>

#include "efx/jobs.h"
#include "efx/shader.h"

namespace efx::assets {

struct Index {
    // Shadernamen aus den .shader-Dateien. Das sind die Namen, die in einer
    // .efx-Datei stehen — nicht die Dateinamen.
    std::vector<std::string> shaders;

    // Bilddateien, ohne Endung und ohne führendes base/. So werden sie in
    // Shadern und in .efx-Dateien geschrieben.
    std::vector<std::string> textures;

    std::vector<std::string> models;   // models/... mit Endung
    std::vector<std::string> sounds;   // sound/... mit Endung
    std::vector<std::string> effects;  // ohne effects/ und ohne .efx

    // Shadername -> Mischung der ersten Stufe. Ohne die zeichnet die Vorschau
    // alles additiv, und ein alphagemischter Rauch sieht dann völlig falsch
    // aus.
    std::vector<std::pair<std::string, shader::BlendMode>> shaderBlends;

    // Shadername -> erste map-Zeile. Ohne die käme man vom Shader nie zum
    // Bild: der Shadername ist oft ein ganz anderer als der Dateiname.
    std::vector<std::pair<std::string, std::string>> shaderMaps;

    // Effektname -> woher er kommt: ein Archivpfad oder ein Ordner.
    //
    // Zum Filtern im Browser. Wer Basisspiel, Mod und eigenen Arbeitsordner
    // gleichzeitig angibt, sieht sonst 734 Effekte in einem Topf und kann
    // nicht sagen, welche davon seine sind.
    //
    // Der erste Eintrag gewinnt — dieselbe Überschreibregel wie sonst auch.
    std::vector<std::pair<std::string, std::string>> effectSources;

    // Woher dieser Effekt kommt. Leer, wenn unbekannt.
    const std::string& sourceOf(const std::string& effectName) const;

    // Shader mit Bildfolge: alle Bilder, die Bildrate und ob er einmalig ist.
    //
    // Ohne das zeigt eine Explosion ihr erstes Bild und rührt sich nicht —
    // sie IST eine Bildfolge, kein Standbild. Nur für Shader mit mehr als
    // einem Bild; bei über 9000 Shadern sind das in der Praxis ein paar
    // Dutzend.
    struct AnimatedShader {
        std::vector<std::string> frames;
        float framesPerSecond = 0.0f;
        bool oneShot = false;
    };
    std::vector<std::pair<std::string, AnimatedShader>> shaderAnims;

    // Shadername -> die `tcMod`-Zeilen seiner ersten Bildstufe.
    //
    // Nur die erste Stufe: wir zeichnen ein Bild je Shader, nicht mehrere
    // übereinander. Für ein Effektbild ist das fast immer die richtige —
    // die weiteren Stufen sind Glanzlichter und Hüllkurven.
    std::vector<std::pair<std::string, std::vector<shader::TexMod>>> shaderTexMods;

    // Die `tcMod`-Zeilen zu einem Shadernamen. Leer, wenn keine da sind.
    const std::vector<shader::TexMod>& texModsOf(const std::string& name) const;

    // Shader mit `rgbGen wave`. Nur diese — bei über 9000 Shadern lohnt es
    // sich nicht, für jeden einen leeren Eintrag mitzuschleppen.
    std::vector<std::pair<std::string, shader::WaveForm>> shaderRgbWaves;

    // Die Farbwelle zu einem Shadernamen, oder nullptr.
    const shader::WaveForm* rgbWaveOf(const std::string& name) const;

    std::vector<std::pair<std::string, shader::WaveForm>> shaderAlphaWaves;
    const shader::WaveForm* alphaWaveOf(const std::string& name) const;

    // Die Bildfolge zu einem Shadernamen, oder nullptr.
    const AnimatedShader* animOf(const std::string& shaderName) const;

    // Wo die Archive liegen. Für das spätere Herausholen einzelner Dateien.
    std::vector<std::string> archives;

    // Die durchsuchten Ordner, in der Suchreihenfolge. Nötig, um eine
    // gefundene Datei später wieder zu öffnen: sie liegt unter einem davon.
    std::vector<std::string> roots;

    int pk3Count = 0;
    int filesSeen = 0;
    int shaderFilesRead = 0;
    std::vector<std::string> notes;

    // Ist dieser Shader vorhanden? Beantwortet die Prüfung, ohne die Liste
    // jedes Mal zu durchsuchen.
    bool hasShader(const std::string& name) const;
    // Die map-Zeile eines Shaders, oder leer.
    std::string mapOf(const std::string& shaderName) const;
    // Die Mischung eines Shaders. Unbekannte Namen gelten als additiv — das
    // ist bei Effekten die häufigste und fällt am wenigsten auf.
    shader::BlendMode blendOf(const std::string& shaderName) const;
    bool hasTexture(const std::string& name) const;
};

// Liest ausschließlich; verändert nichts im Spielordner.
//
// Der Pool ist optional. Ohne ihn läuft der Suchlauf seriell — bei einer
// vollständigen Installation sind das gut zwanzig .pk3-Dateien, und jede kann
// ein eigener Faden lesen.
Index scan(const std::string& basePath, jobs::Pool* pool = nullptr,
           jobs::Cancellation* cancel = nullptr);

// Ein einzelnes `.pk3` als Bestand.
//
// Für „Archiv öffnen": man hat eine Mod-Datei bekommen und will hineinsehen,
// ohne sie erst irgendwohin zu entpacken oder einen Spielpfad umzustellen.
Index scanArchive(const std::string& archivePath);

// Mehrere Ordner auf einmal.
//
// Die Reihenfolge entscheidet: **der erste Treffer gewinnt.** Wer einen Mod
// bearbeitet, hat das Grundspiel an einer Stelle und den Mod an einer
// zweiten; steht der Mod vorn, überschreiben seine Dateien die des
// Grundspiels — genau wie die Engine es mit ihren `.pk3`-Dateien macht.
Index scanAll(const std::vector<std::string>& basePaths, jobs::Pool* pool = nullptr,
              jobs::Cancellation* cancel = nullptr);

// Holt eine einzelne Datei aus einem Archiv heraus.
//
// Bis eben lasen wir nur die Verzeichnisse — für eine Namensliste reicht das,
// aber nicht für Texturen und nicht für Shader, deren Namen im Dateiinhalt
// stehen.
//
// Der Name muss genau so geschrieben sein, wie er im Verzeichnis steht.
// Gross- und Kleinschreibung wird dabei ignoriert, weil Zip sie unterscheidet
// und die Engine nicht.
std::vector<unsigned char> readFromZip(const std::string& archivePath,
                                       const std::string& name,
                                       std::string* error = nullptr);

// Das Inhaltsverzeichnis einer einzelnen Zip-Datei. Getrennt, damit es sich
// ohne Spielordner prüfen lässt.
//
// Gibt die Namen zurück, die darin stehen — Ordnereinträge (auf `/` endend)
// werden weggelassen.
std::vector<std::string> readZipDirectory(const std::string& path,
                                          std::string* error = nullptr);

// Eine Bilddatei zu einem Namen finden — und sie lesen.
//
// Der Weg vom Shadernamen zum Bild ist länger, als man denkt:
//
//   1. `gfx/effects/spark` steht in der .efx-Datei
//   2. im Shaderbestand nachsehen: hat ein Shaderblock diesen Namen?
//      Wenn ja, die `map`-Zeile seiner ersten Stufe nehmen.
//   3. Wenn nein, den Namen direkt als Bildnamen behandeln — die Engine tut
//      dasselbe und baut sich aus einer nackten Bilddatei einen Ersatzshader.
//   4. Endungen durchprobieren. Die Engine nimmt die erste, die es gibt.
//   5. Die Datei liegt entweder ausgepackt oder in einem .pk3.
//
// Schritt 4 ist der, an dem Nachbauten gern scheitern: in einer .efx-Datei
// steht **nie** eine Endung, und in einer `map`-Zeile fast nie.
struct ResolvedTexture {
    std::string path;      // wie gefunden, mit Endung
    std::string archive;   // leer, wenn ausgepackt
    std::string root;      // unter welchem Spielpfad sie liegt
    bool found = false;
};

// Sucht die Bilddatei zu einem Shader- oder Texturnamen.
//
// shaderMap ist die Zuordnung Shadername -> map-Zeile; leer bedeutet, dass
// der Name direkt als Bildname gilt.
// Sucht die Klangdatei zu einem Namen aus einer .efx-Datei.
//
// Zwei Dinge, die die Engine tut und die man ohne Fundstelle nicht ahnt:
//
//   1. Steht `.wav` und gibt es die Datei nicht, wird `.mp3` versucht.
//      Fundstelle `S_LoadSound_Actual` in snd_mem.cpp:
//
//          *piSize = FS_ReadFile( psFilename, ... );      // try WAV
//          if ( !*pData ) {
//              psFilename[len-3] = 'm'; ... = 'p'; ... = '3';
//              *piSize = FS_ReadFile( psFilename, ... );  // try MP3
//
//      Genau deshalb steht in fast jeder Raven-.efx `.wav`, obwohl im `.pk3`
//      eine `.mp3` liegt. Ohne diesen Rückfall findet man nichts.
//
//   2. Gesucht wird in ALLEN Spielpfaden und Archiven, nicht nur im ersten —
//      wie bei Bildern auch.
// Sucht die Ordner, die wie ein Spielordner aussehen.
//
// Wofür: der Anwender zeigt auf `GameData` und erwartet, dass base, seine Mod
// und der eigene Arbeitsordner mitkommen. Einfach rekursiv zu suchen genügt
// dafür NICHT — die ausgepackten Dateien bekämen dann Pfade wie
// `base/gfx/…` statt `gfx/…`, und nichts würde mehr aufgelöst.
//
// Deshalb werden die Wurzeln gesucht und danach jede für sich durchsucht.
// Wurzelartig heißt: enthält direkt `.pk3`-Dateien, oder einen Unterordner
// `shaders`, `effects` oder `gfx`.
//
// `maxDepth` begrenzt die Suche. Wer versehentlich `C:\` angibt, soll nicht
// minutenlang warten.
std::vector<std::string> discoverRoots(const std::string& folder,
                                       int maxDepth = 2);

ResolvedTexture findSound(const Index& index, const std::string& basePath,
                          const std::string& name);

ResolvedTexture findTexture(const Index& index, const std::string& basePath,
                            const std::string& name);

// Sucht eine .efx-Datei zu einem Namen, wie er in einer anderen .efx-Datei
// steht: ohne Ordnervorsatz und ohne Endung, etwa „explosions/big".
//
// Getrennt von findTexture, weil hier nichts durchprobiert wird — es gibt
// genau eine Endung.
ResolvedTexture findEffect(const Index& index, const std::string& basePath,
                           const std::string& name);

// Liest eine Datei, gleich ob ausgepackt oder im Archiv.
std::vector<unsigned char> readFile(const std::string& basePath,
                                    const ResolvedTexture& where,
                                    std::string* error = nullptr);

// Die Endungen, die JKA durchprobiert, in der Reihenfolge der Engine.
const std::vector<std::string>& imageExtensions();

// Ordnet einen Dateinamen einer der Listen zu. Auch getrennt, weil hier die
// meisten Sonderfälle liegen.
enum class Kind { Ignore, Shader, Texture, Model, Sound, Effect };
Kind classify(const std::string& relativePath);

// Wie ein Name in einer .efx-Datei geschrieben wird: Kleinbuchstaben,
// Vorwärtsschrägstriche, je nach Art ohne Endung oder ohne Ordnervorsatz.
std::string normaliseName(const std::string& relativePath, Kind kind);

}  // namespace efx::assets
