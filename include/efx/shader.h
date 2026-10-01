// Q3/JKA-Shaderdateien, so weit die Vorschau sie braucht.
//
// Das ist bewusst kein vollstaendiger Shaderuebersetzer. Die Vorschau muss
// wissen: welches Bild, wie wird es ueberblendet, faerbt es sich nach der
// Primitive, laeuft eine Bildfolge. Alles darueber — Weltgeometrie,
// Lichtkarten, deformVertexes — gehoert zum Renderer der Karte, nicht zum
// Effekt.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "efx/io.h"  // Diagnostic

namespace efx::shader {

// Die GL-Faktoren, die in JKA-Shadern vorkommen.
enum class BlendFactor {
    Unset,
    One,
    Zero,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    SrcAlphaSaturate,
};

// Woher die Farbe kommt. Fuer Effekte ist "vertex" der wichtige Fall: dann
// faerbt der rgb-Block der Primitive das Bild ein. Bei "identity" bleibt das
// Bild, wie es ist — wer dort am rgb dreht, sieht nichts.
enum class ColorGen {
    Identity,
    IdentityLighting,
    Vertex,
    ExactVertex,
    Entity,
    OneMinusEntity,
    OneMinusVertex,
    LightingDiffuse,
    LightingSpecular,
    Wave,
    Const,
};

enum class AlphaGen {
    Identity,
    Vertex,
    OneMinusVertex,
    Entity,
    OneMinusEntity,
    LightingSpecular,
    Wave,
    Portal,
    Const,
};

enum class Cull {
    Front,  // Voreinstellung
    Back,
    None,   // "twosided" / "disable"
};

struct WaveForm {
    std::string func;  // sin, triangle, square, sawtooth, inversesawtooth, noise
    float base = 0.0f;
    float amplitude = 0.0f;
    float phase = 0.0f;
    float frequency = 0.0f;
};

struct TexMod {
    std::string type;         // scroll, scale, rotate, stretch, turb, transform
    std::vector<float> args;  // unverändert, Bedeutung hängt am Typ

    // Nur `stretch` und `turb`. Beide beschreiben eine Welle, und die passt
    // nicht in `args`: dort landet alles als Zahl, und der Name der
    // Wellenform („sin", „triangle") wäre damit verloren.
    //
    // Die Engine liest sie verschieden:
    //     tcMod stretch <func> <base> <amp> <phase> <freq>
    //     tcMod turb           <base> <amp> <phase> <freq>
    //
    // `turb` hat keinen Namen — es ist immer ein Sinus.
    WaveForm wave;
};

// Die Texturkoordinaten einer Stufe zu einem Zeitpunkt verschieben.
//
// `tcMod` steht 238-mal in einer gewöhnlichen Installation, davon 103-mal als
// `scroll`. Wir haben es bisher nur GELESEN und nie angewendet — scrollende
// Texturen standen damit still: Energiestrahlen, Kraftfelder, fließende Lava.
//
// Die Formeln aus tr_shade_calc.cpp:
//
//   scroll s t   adjusted = speed * time;  adjusted -= floor(adjusted);
//                st += adjusted
//   scale  s t   st *= (s, t)
//   rotate deg   degs = -degsPerSecond * time, gedreht um (0.5, 0.5):
//                matrix[0][0]= cos; matrix[1][0]=-sin;
//                translate[0]= 0.5 - 0.5*cos + 0.5*sin;
//                matrix[0][1]= sin; matrix[1][1]= cos;
//                translate[1]= 0.5 - 0.5*sin - 0.5*cos;
//
// Das Abschneiden bei `scroll` ist kein Schönheitsfehler, sondern steht so im
// Quelltext: „clamp so coordinates don't continuously get larger, causing
// problems with hardware limits".
//
// `stretch` und `turb` fehlen noch — beide brauchen die Wellenfunktionen
// beziehungsweise eine Verzerrung je Eckpunkt.
struct TexCoord {
    float u = 0.0f;
    float v = 0.0f;
};

// `position` ist die Lage des Eckpunkts in der Welt. Nur `turb` braucht sie —
// die Verwirbelung hängt davon ab, WO der Eckpunkt steht, nicht nur wann.
//
//     RB_CalcTurbulentTexCoords:
//         now  = phase + floatTime * frequency;
//         s   += sin( (x + z)/128 * 0.125 + now ) * amplitude;
//         t   += sin(  y     /128 * 0.125 + now ) * amplitude;
//
// Die 1/128 stammt aus Quakes Texturmaßstab, die 0.125 ist Ravens Zugabe.
TexCoord applyTexMods(const std::vector<TexMod>& mods, TexCoord in,
                      float seconds,
                      const float position[3] = nullptr);

// Eine Wellenform auswerten — für `rgbGen wave` und `alphaGen wave`.
//
// Das Makro WAVEVALUE in tr_shade_calc.cpp:
//
//     ((base) + table[ Q_ftol( ((phase) + floatTime*(freq)) * FUNCTABLE_SIZE )
//                      & FUNCTABLE_MASK ] * (amplitude))
//
// Die Engine schlägt in einer Tabelle mit 1024 Einträgen nach, wir rechnen
// direkt. Der Unterschied ist eine Rundung auf ein Tausendstel der Periode.
//
// Ravens Tabellen sind nicht alle so, wie der Name vermuten lässt:
// `triangle` läuft von 0 auf 1 und zurück auf 0, NICHT von -1 bis 1, und
// `sawtooth` von 0 nach 1. Nur `sin` und `square` sind zweiseitig.
float evaluateWave(const WaveForm& wave, float seconds);

struct Stage {
    // Bildquelle. Genau eine der drei ist gesetzt.
    std::string map;              // auch "$whiteimage" / "$lightmap"
    std::string clampMap;
    std::vector<std::string> animMaps;
    float animFrequency = 0.0f;   // Bilder je Sekunde
    bool animOneShot = false;     // JKA-Zusatz "oneshotanimMap"

    BlendFactor srcBlend = BlendFactor::Unset;
    BlendFactor dstBlend = BlendFactor::Unset;

    ColorGen rgbGen = ColorGen::Identity;
    WaveForm rgbWave;
    float rgbConst[3]{1.0f, 1.0f, 1.0f};

    AlphaGen alphaGen = AlphaGen::Identity;
    WaveForm alphaWave;
    float alphaConst = 1.0f;

    std::string alphaFunc;  // GT0, LT128, GE128
    // base, lightmap, environment, vector.
    //
    // Gelesen, aber beim Zeichnen nicht ausgewertet — und das ist eine
    // Entscheidung, keine Lücke:
    //
    // `tcGen environment` steht in einer gewöhnlichen Installation 21-mal,
    // davon sechs im Bereich gfx/. Alle sechs sind MODELLshader —
    // cloakedShader, personalshield, ion_shield, forceprotect. Die liegen auf
    // Spielerfiguren, nicht auf Partikeln einer .efx. Die übrigen fünfzehn
    // sind Wandtexturen.
    //
    // Dazu kommt: ein Partikel dreht sich immer zur Kamera. Damit zeigt seine
    // Normale stets zum Betrachter, der Spiegelvektor ändert sich über das
    // Viereck kaum, und die Umgebungsabbildung ergäbe eine fast einfarbige
    // Fläche. Der Aufwand ginge in einen Fall, den es nicht gibt.
    std::string tcGen;
    std::vector<TexMod> texMods;

    bool depthWrite = false;
    bool detail = false;
    bool glow = false;      // JKA-Zusatz: geht in den Glow-Puffer
    std::string depthFunc;  // equal, lequal
};

// Wie eine Stufe gemischt wird — auf die fünf Arten heruntergebrochen, die
// eine Vorschau braucht.
//
// JKA-Shader schreiben mehr Kombinationen, als hier stehen. Das ist Absicht:
// eine Vorschau muss nicht jede Kombination exakt treffen, sie muss den
// **Charakter** treffen. Ein additiv gemischter Funke und ein alphagemischter
// Rauch sehen völlig verschieden aus; ob der Funke mit `GL_ONE GL_ONE` oder
// `GL_SRC_ALPHA GL_ONE` gemischt ist, sieht man kaum.
//
// Wo es nicht passt, wird auf die nächstliegende Art gerundet — und `unknown`
// gemeldet, damit man weiß, dass gerundet wurde.
enum class BlendMode {
    Opaque,      // GL_ONE GL_ZERO oder gar kein blendFunc
    Additive,    // Ziel wird aufgehellt: GL_ONE GL_ONE, GL_SRC_ALPHA GL_ONE
    AlphaBlend,  // GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    Modulate,    // GL_DST_COLOR GL_ZERO — dunkelt ab
    Filter,      // GL_DST_COLOR GL_SRC_COLOR
};

// Bricht ein Faktorenpaar auf eine der fünf Arten herunter.
//
// exact sagt, ob die Kombination genau getroffen wurde oder gerundet werden
// musste. Für die Prüfung: ein Shader, dessen Mischung die Vorschau nur
// annähert, sollte das sagen dürfen.
BlendMode blendModeOf(BlendFactor src, BlendFactor dst, bool* exact = nullptr);
const char* blendModeName(BlendMode mode);

struct Shader {
    std::string name;
    std::string sourceFile;
    int line = 0;

    std::vector<Stage> stages;

    Cull cull = Cull::Front;
    bool noPicMip = false;
    bool noMipMaps = false;
    bool polygonOffset = false;
    std::string sort;
    std::string editorImage;              // qer_editorimage
    std::vector<std::string> surfaceParms;
    std::vector<std::string> deforms;     // roh, nur zur Anzeige

    // Was die Vorschau daraus ableitet.
    //
    // Ein Effekt-Shader mit "blendFunc GL_SRC_ALPHA ..." braucht einen
    // Alphawert ungleich null, sonst bleibt er unsichtbar. Genau davor warnt
    // Ravens Handbuch im Abschnitt "If You Don't See Anything".
    bool needsAlpha() const;
    // Faerbt der rgb-Block der Primitive dieses Bild ueberhaupt ein?
    bool usesVertexColor() const;
    // Das Bild, das die Vorschau zeigen soll.
    const std::string& previewImage() const;
};

// Welches Bild einer Bildfolge zu einem Zeitpunkt gilt.
//
// Fundstelle `RB_ComputeAnimatedImage` in tr_shade.cpp:
//
//     index = Q_ftol( floatTime * imageAnimationSpeed * FUNCTABLE_SIZE );
//     index >>= FUNCTABLE_SIZE2;          // Festkomma-Runden, sonst nichts
//     if ( oneShotAnimMap ) { if (index >= count) index = count - 1; }
//     else                    index %= count;
//
// Der Unterschied zwischen den beiden Arten ist der ganze Witz: `animMap`
// läuft in Schleife, `oneshotanimMap` bleibt auf dem letzten Bild stehen.
// Eine Explosion ist das Zweite — sie soll verglühen und nicht von vorn
// anfangen.
//
// `seconds` ist die Zeit seit dem Auslösen, nicht die Wanduhr: in einer
// Vorschau, die man anhalten und zurückspulen kann, wäre die Wanduhr das
// Falsche.
int animFrameAt(int frameCount, float framesPerSecond, bool oneShot,
                float seconds);

struct Library {
    std::vector<Shader> shaders;
    std::vector<Diagnostic> diagnostics;

    // Gross-/Kleinschreibung egal, wie im Spiel.
    const Shader* find(std::string_view name) const;
};

// Liest eine .shader-Datei. Fehler brechen nicht ab; die Engine ueberliest
// unbekannte Direktiven ebenfalls.
void parseInto(Library& library, std::string_view text, std::string_view fileName);

}  // namespace efx::shader
