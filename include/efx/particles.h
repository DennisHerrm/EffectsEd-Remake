// Die laufende Vorschau: aus einer Effektdatei werden bewegte Bilder.
//
// Bindet zusammen, was schon geprüft dasteht — `sim::schedule` für das Wann,
// `sim::positionAt` für das Wohin, `curve::evaluate` für Größe, Farbe und
// Alpha — und erzeugt daraus Geometrie.
//
// Die Geometrieerzeugung steht hier und nicht im Renderer, damit sie prüfbar
// bleibt: ob ein Billboard vier Ecken in der richtigen Lage hat, lässt sich
// ausrechnen; ob es dann auf dem Bildschirm erscheint, nicht.
#pragma once

#include <map>
#include <set>
#include <string>
#include <functional>
#include <vector>

#include "efx/camera.h"
#include "efx/curve.h"
#include "efx/effect.h"
#include "efx/scene.h"
#include "efx/shader.h"
#include "efx/sim.h"

namespace efx::particles {

// Eine ausgelöste Primitive, mit allen gewürfelten Werten festgelegt.
//
// Gewürfelt wird **beim Auslösen**, nicht bei jedem Bild — sonst zittert eine
// Größe mit Spanne die ganze Lebensdauer, statt einmal festzustehen. Genau so
// macht es die Engine: `GetVal()` wird beim Erzeugen gerufen.
struct Live {
    PrimitiveType type = PrimitiveType::Particle;
    int primitiveIndex = 0;

    float spawnMs = 0.0f;
    float deathMs = 0.0f;

    camera::Vec3 origin;
    camera::Vec3 origin2;      // Endpunkt bei Line und Electricity
    camera::Vec3 velocity;
    camera::Vec3 acceleration;
    float gravity = 0.0f;

    curve::Curve size;
    curve::Curve size2;
    curve::Curve length;
    curve::Curve alpha;
    curve::Curve rgb[3];       // je Kanal, alle mit denselben Flags

    float rotation = 0.0f;
    float rotationDelta = 0.0f;

    // `useAlpha` aus den Flags der Primitive.
    //
    // Entscheidet, WOHIN die Alphakurve wirkt, und das ist kein Detail:
    //
    //   ohne useAlpha -> rgb *= alpha, Alphakanal bleibt voll
    //   mit  useAlpha -> rgb bleibt,   Alphakanal = alpha
    //
    // Fundstelle `CParticle::UpdateAlpha` in FxPrimitives.cpp, samt Ravens
    // eigenem Kommentar: "Modulate the rgb fields by the alpha value to do the
    // fade, works fine for additive blending".
    //
    // Der Grund, warum das gerade bei additiver Mischung zaehlt: additiv wird
    // der Alphakanal gar nicht ausgewertet. Eine Flamme, deren Ausblenden im
    // Alphakanal steht, blendet also nie aus.
    bool useAlpha = false;

    // Nur für Electricity: wie stark der Blitz vom geraden Weg abweicht, und
    // ein eigener Ausgangswert, damit zwei Blitze nebeneinander nicht
    // deckungsgleich zappeln.
    float chaos = 1.0f;

    // Nur fuer CameraShake. Aus `elasticity` (in der Datei oft `intensity`)
    // und `radius` — so fuettert die Engine es:
    //
    //     theFxHelper.CameraShake( origin, fx->mElasticity.GetVal(),
    //                              fx->mRadius.GetVal(), fx->mLife.GetVal() );
    float shakeIntensity = 0.0f;
    float shakeRadius = 0.0f;

    // Die Vorwärtsachse des Effekts zum Zeitpunkt des Auslösens.
    //
    // Cylinder, Decal und OrientedParticle richten sich daran aus — die
    // Engine reicht `ax[0]` durch, nicht `origin2`:
    //
    //     FX_AddCylinder( clientID, org, ax[0], ... );
    //     CG_ImpactMark( handle, org, ax[0], rotation, ... );
    //
    // Getrennt gespeichert, weil ein Kindeffekt seine eigene Achse bekommt:
    // ein `impactFx` zeigt von der getroffenen Wand weg, nicht in die
    // Richtung des Elternteils.
    camera::Vec3 forward{0.0f, 0.0f, 1.0f};
    // Schon ausgeloest? Wie bei den Klaengen: einmal, nicht in jedem Bild.
    bool shakeTriggered = false;
    unsigned seed = 1u;

    // Der Shadername, unter dem gezeichnet wird. Beim Auslösen gewählt: hat
    // eine Primitive mehrere, würfelt die Engine einen aus — und der bleibt
    // dann für die Lebensdauer derselbe.
    std::string shader;

    // Bei Sound-Segmenten: welcher Klang gespielt wird, und ob es schon
    // geschehen ist. Ohne das Merkzeichen würde er in jedem Bild neu
    // angestoßen, solange die Primitive lebt.
    std::string soundName;
    mutable bool soundPlayed = false;

    // Die Flags, wie die Engine sie sieht (efx::effectiveFlags): useAlpha,
    // setShaderTime, die Blitzformen taper/branch/grow, killOnImpact.
    uint32_t flags = 0;

    // Nur Electricity: der Startwert fuer Q_random, aus dem die GROBE Form
    // des Blitzes entsteht. CElectricity::Initialize (FxPrimitives.cpp):
    //
    //     mRefEnt.frame = Q_flrand(0.0f, 1.0f) * 1265536;
    //
    // Einmal beim Erzeugen gesetzt; der Renderer arbeitet in jedem Bild auf
    // einer Kopie der Entity, also beginnt die Folge jedes Bild gleich — die
    // Form steht fuer die ganze Lebensdauer. Nur das Mikrozittern aus
    // CreateShape (Q_flrand) aendert sich von Bild zu Bild.
    int boltSeed = 0;

    // Der `random`-Anteil einer Kurve.
    //
    // Die Engine wuerfelt ihn in JEDEM Bild neu: `perc1 = Q_flrand(0,1) *
    // perc1` steht in UpdateSize, UpdateRGB, UpdateAlpha, UpdateLength
    // (FxPrimitives.cpp), die jedes Bild laufen. Daher das Flackern von
    // Funken und Flammen. Wir hatten einen Wert je Teilchen fuer die ganze
    // Lebensdauer gezogen — ruhig, wo das Spiel flackert.
    //
    // Abhaengig von Zeit (60 Bilder je Sekunde) statt vom Bildzaehler: so
    // zeigt dasselbe angefahrene Bild beim Zurueckspulen denselben Wert.
    enum RandomChannel { kRandomSize = 0, kRandomSize2, kRandomLength,
                         kRandomAlpha, kRandomRgb };
    float randomAt(RandomChannel channel, float nowMs) const;

    // Vorgerechnet, damit es nicht in jedem Bild neu bestimmt wird.
    float sizeParm = 0.0f, size2Parm = 0.0f, lengthParm = 0.0f;
    float alphaParm = 0.0f, rgbParm = 0.0f;

    // Bei `usePhysics`: die zerlegte Bahn mit Abprallern. Sonst leer, dann
    // gilt die geschlossene Formel.
    sim::Path path;
    bool hasPath = false;

    bool aliveAt(float nowMs) const {
        return nowMs >= spawnMs && nowMs < deathMs;
    }
    camera::Vec3 positionAt(float nowMs) const;
};

// Ein Zeichenaufruf, so wie die Engine ihn absetzt: EINE Shaderstufe eines
// Shaders, mit genau dem Bild, der Mischung und den Farben dieser Stufe.
//
// `byTexture` (unten) ist die rohe Geometrie je Shadername mit den Farben aus
// der .efx. Gezeichnet wird aber `groups`: dort ist jede Stufe des Shaders
// eine eigene Gruppe (RB_StageIteratorGeneric zeichnet alle Stufen
// nacheinander), die Farbe ist nach rgbGen/alphaGen der Stufe berechnet
// (ComputeColors in tr_shade.cpp), und die Reihenfolge ist die der Engine.
struct DrawGroup {
    std::string shader;  // wie in der .efx
    int stage = 0;
    // Das Bild dieser Stufe: Dateiname aus map/clampMap, das gerade gueltige
    // Bild einer Bildfolge, "$whiteimage" fuer das eingebaute weisse — oder
    // der Shadername selbst, wenn es keinen Shaderblock gibt (dann baut die
    // Engine einen Ersatzshader aus dem gleichnamigen Bild).
    std::string image;
    bool clamp = false;  // clampMap

    // Die Mischung als GL-Faktorpaar, genau wie im Shader. `blended` falsch
    // heisst: keine Mischung (kein blendFunc oder GL_ONE GL_ZERO), und dann
    // schreibt die Stufe Tiefe (ParseStage: depthMaskBits = GLS_DEPTHMASK_TRUE).
    shader::BlendFactor src = shader::BlendFactor::One;
    shader::BlendFactor dst = shader::BlendFactor::Zero;
    bool blended = false;
    bool depthWrite = true;
    // Der Ersatzshader fuer ein Bild ohne Shaderblock zeichnet OHNE
    // Tiefentest (R_FindShader, LIGHTMAP_2D: GLS_DEPTHTEST_DISABLE).
    bool depthTest = true;
    // alphaFunc der Stufe: 0 keiner, 1 GT0, 2 LT128, 3 GE128, 4 GE192
    // (NameToAFunc in tr_shader.cpp).
    int alphaTest = 0;

    // Sortierschluessel der Engine (shaderSort_t, oder der Zahlenwert hinter
    // `sort`). Gezeichnet wird aufsteigend.
    float sort = 0.0f;
    int firstSeen = 0;   // wann der Shader in diesem Bild zuerst vorkam

    scene::Mesh mesh;
};

// Ein dynamisches Licht (Light-Primitive). CLight::Draw ruft nur
// `AddLightToScene( origin, radius, r, g, b )` — es erhellt die Flaechen der
// Umgebung, es hat keine eigene Geometrie.
struct DynamicLight {
    camera::Vec3 origin;
    float radius = 0.0f;
    float rgb[3]{1.0f, 1.0f, 1.0f};
};

// Was gezeichnet werden soll, unabhängig von der Grafikschnittstelle.
struct DrawList {
    // Nach Shadername gruppiert. Ein Zeichenaufruf je Shader statt einer je
    // Partikel — bei zweihundert Funken ist das der Unterschied zwischen
    // flüssig und ruckelig, und die Gruppierung kostet nichts, weil ohnehin
    // über alle gelaufen wird.
    //
    // Ein `map` und kein `unordered_map`: die Reihenfolge soll von Bild zu
    // Bild dieselbe sein. Bei additiver Mischung fällt das nicht auf, bei
    // alphagemischten Flächen sehr wohl.
    std::map<std::string, scene::Mesh> byTexture;

    // Was die Grafikschnittstelle zeichnet: je Shaderstufe eine Gruppe, schon
    // in der Reihenfolge der Engine (siehe DrawGroup).
    std::vector<DrawGroup> groups;

    // Die Lichter dieses Bildes — fuer die Raumflaechen.
    std::vector<DynamicLight> lights;

    // Wie viele Vierecke weggelassen wurden, weil die Geometrie voll war.
    //
    // Die Indizes sind 16 Bit — mehr als 65535 Eckpunkte je Shader lassen
    // sich nicht ansprechen. Bei `static_cast<uint16_t>(vertices.size())`
    // lief der Zähler still über und die Indizes zeigten auf FREMDE Eckpunkte:
    // das Bild wurde zu Fetzen, ohne dass irgendwo etwas gemeldet wurde.
    //
    // Die Grenze von 4096 je Segment schützt davor nicht — fünf Segmente mit
    // demselben Shader landen in derselben Geometrie und ergeben 81920
    // Eckpunkte.
    int skipped = 0;
    int drawn = 0;
    int alive = 0;

    // Fuer die Statuszeile des Originals ("Active | Drawn | Scheduled |
    // Marks"): wie viele Ausloesungen noch auf ihre Verzoegerung warten
    // (Kindeffekte eingeschlossen), und wie viele Decal-Abdruecke bis zu
    // diesem Zeitpunkt entstanden sind — ein Abdruck bleibt, bis man anhaelt.
    int scheduled = 0;
    int marks = 0;

    // Shadernamen, die von einem Segment mit `useAlpha` kommen.
    //
    // Wofuer: ist ein Shader in keiner .shader-Datei zu finden, muss die
    // Vorschau die Mischart raten. Die Vorgabe ist additiv, weil das bei
    // Effekten das Haeufigste ist — aber `useAlpha` heisst laut Ravens eigenem
    // Kommentar "art that has an alpha channel", und solche Bilder werden
    // alphagemischt, nicht additiv.
    //
    // Additiv gezeichneter dunkler Rauch addiert fast nichts und ist damit
    // unsichtbar. Genau so fehlte der Rauch ueber einem Feuer.
    std::set<std::string> alphaShaders;
};

// Die Effektachse: wohin „vorwärts" zeigt.
//
// Alles, was in den Eigenschaftsseiten „Forward / Right / Up" heißt, wird
// damit verrechnet — Ursprung, Endpunkt, Geschwindigkeit, Beschleunigung. Das
// ist der Sinn der drei Menüpunkte „Orient Up / Sideways / Down"; ihre
// Hilfetexte im Original lauten *„Orient effect up (on Z axis)"* und so fort.
//
// Ohne sie liegt der Effekt immer so, wie er in der Datei steht, und man
// müsste jede Zahl umrechnen, um ihn von der Seite zu sehen.
struct Axis {
    camera::Vec3 forward{0.0f, 0.0f, 1.0f};
    camera::Vec3 right{1.0f, 0.0f, 0.0f};
    camera::Vec3 up{0.0f, 1.0f, 0.0f};
};

// 0 nach oben, 1 seitwärts, 2 nach unten — wie die Einstellung.
Axis axisFor(int orientation);

// Die drei Platzierungs-Flags aus `CFxScheduler::CreateEffect`.
//
// Sie stehen dort in einem Block VOR der Verzweigung nach Typ und gelten
// deshalb für alle Primitiven gleichermaßen. Bei uns wurden sie gelesen und
// nie ausgewertet — eine Datei mit `orgOnSphere` sah aus wie eine ohne.

// `randRotAroundFwd`: die Seitenachse um einen zufälligen Winkel um die
// Vorwärtsachse drehen.
//
//     RotatePointAroundVector( ax[1], ax[0], axis[1], flrand(0,1)*360 );
//     CrossProduct( ax[0], ax[1], ax[2] );
Axis rotatedAroundForward(const Axis& axis, float degrees);

// `orgOnSphere`: ein Punkt auf einem Ellipsoid, in WELTkoordinaten — die
// Achsen des Effekts spielen hier keine Rolle.
//
//     x = DEG2RAD( flrand(0,1) * 360 );
//     y = DEG2RAD( flrand(0,1) * 180 );
//     temp = ( sin(x)*width*sin(y), cos(x)*width*sin(y), cos(y)*height );
camera::Vec3 pointOnSphere(float turns, float pitch, float radius, float height);

// `orgOnCylinder`: ein Punkt auf einem Zylindermantel, um die Vorwärtsachse
// gedreht.
//
//     pt = ax[1]*radius;
//     pt += ax[0] * ( flrand(-1,1) * 0.5 * height );
//     temp = RotatePointAroundVector( ax[0], pt, flrand(0,1)*360 );
camera::Vec3 pointOnCylinder(const Axis& axis, float radius, float height,
                             float alongFraction, float degrees);

// `axisFromSphere`: die Vorwärtsachse zeigt vom Mittelpunkt zum Punkt, die
// beiden anderen werden dazu senkrecht ergänzt — genau wie MakeNormalVectors
// in q_math.c:
//
//     right[1] = -forward[0]; right[2] = forward[1]; right[0] = forward[2];
//     d = DotProduct(right, forward);  right -= d*forward;  normalize(right);
//     CrossProduct( right, forward, up );
Axis axisFromDirection(const camera::Vec3& direction);



// Woher weitere Effekte kommen.
//
// Emitter und FxRunner starten andere .efx-Dateien. Die Vorschau muss sie
// nachladen können — aber der Kern soll nichts vom Dateisystem wissen, also
// wird ein Rückruf hereingereicht.
//
// Gibt nullptr zurück, wenn es die Datei nicht gibt. Der Aufrufer sollte
// zwischenspeichern: bei einem Emitter, der zwanzigmal aussendet, wird
// derselbe Name zwanzigmal gefragt.
using EffectLoader = std::function<const Effect*(const std::string& name)>;

// Wie tief geschachtelt gestartet wird.
//
// Ein Effekt, der sich selbst startet, ist in .efx-Dateien nicht verboten —
// und ohne Grenze hängt sich die Vorschau daran auf. Vier Ebenen sind mehr,
// als in echten Dateien vorkommt.
inline constexpr int kMaxEffectDepth = 4;

class System {
public:
    // Startet den Effekt. seed macht den Ablauf wiederholbar.
    // `planes` sind die Flächen, an denen Primitiven mit `usePhysics`
    // abprallen. Leer heißt: keine Kollision, alles fliegt durch.
    // `buildUpRepeats`: `repeatDelay` mitspielen.
    //
    // Ein Effekt mit `repeatDelay` wird im Spiel wiederholt, ohne dass der
    // laufende abgebrochen wird — die Generationen überlagern sich, und
    // genau daraus entsteht ein stehendes Feuer statt einzelner Flämmchen.
    // Fundstelle: `CFxScheduler::AddLoopedEffects`.
    //
    // Nicht die Vorgabe: wer ein Segment beurteilen will, will einen
    // Durchlauf sehen und nicht fünf übereinander.
    //
    // Die Dauer bleibt die volle Lebensdauer des Effekts. Wer über genau eine
    // Wiederholung laufen will — die Browserkacheln —, stellt seine Uhr
    // selbst auf `effect.repeatDelay`.
    void play(const Effect& effect, unsigned seed = 1u,
              const std::vector<bool>& enabledMask = {}, const Axis& axis = {},
              EffectLoader loader = {},
              const std::vector<sim::Plane>& planes = {},
              bool buildUpRepeats = false);
    // Anhalten, aber die Geometrie BEHALTEN.
    //
    // Damit bleibt die Zeitleiste bedienbar: nach dem Anhalten kann man den
    // Effekt weiter anfahren und sich jeden Zeitpunkt ansehen. Ohne das war
    // der Bestand leer, die Zeitleiste zeigte „0 / 1 ms", und das Schieben
    // bewirkte nichts — der häufigste Grund, ein Werkzeug für kaputt zu
    // halten.
    void stop();

    // Alles wegwerfen. Für den Wechsel der Datei oder des Reiters, wo die
    // Geometrie zur alten Datei gehört und nicht mehr angesehen werden soll.
    void clear();

    bool playing() const { return playing_; }
    int aliveAt(float nowMs) const;

    // Erzeugt die Geometrie für den Zeitpunkt.
    //
    // right und up kommen aus der Blickmatrix: ein Billboard steht senkrecht
    // zur Blickrichtung, und die beiden Achsen spannen es auf.
    // `texMods`: liefert die `tcMod`-Regeln zu einem Shadernamen.
    //
    // Ein Rückruf statt einer Tabelle, weil der Kern die Shaderdateien nicht
    // kennt — er weiß nur, dass es sie gibt. Leer heißt: keine Verschiebung.
    //
    // Ohne diesen Weg blieb `applyTexMods` das, was es lange war: umgesetzt,
    // geprüft und nie aufgerufen.
    // Was der Kern über einen Shader wissen muss, um ihn zu zeichnen.
    //
    // Ein gemeinsamer Rückruf statt eines je Eigenschaft: `tcMod` war der
    // erste, `rgbGen wave` der zweite, und es werden weitere kommen.
    struct ShaderDraw {
        const std::vector<shader::TexMod>* texMods = nullptr;
        // `rgbGen wave` ERSETZT die Farbe durch einen Grauwert, es dämpft sie
        // nicht. Fundstelle `RB_CalcWaveColor`:
        //
        //     glow = EvalWaveForm( wf ) * tr.identityLight;
        //     if (glow < 0) glow = 0; else if (glow > 1) glow = 1;
        //     v = Q_ftol( 255 * glow );
        //     color[0] = color[1] = color[2] = v;
        //
        // Damit hat die Farbe aus der .efx für diese Stufe keine Wirkung mehr
        // — das ist kein Versehen, sondern der Sinn der Sache: `rgbGen wave`
        // ist ein Pulsieren, das der Shader vorgibt.
        const shader::WaveForm* rgbWave = nullptr;

        // `alphaGen wave` — dasselbe für die Durchsichtigkeit.
        //
        //     RB_CalcWaveAlpha: glow = EvalWaveFormClamped( wf );
        //                       dstColors[3] = 255 * glow;
        //
        // `Clamped` heißt: unter null wird zu null. Nach oben wird NICHT
        // begrenzt — anders als bei der Farbe. Bei `255 * glow` mit glow > 1
        // läuft der Wert über und wird dunkel statt heller. Wir schneiden
        // deshalb bei eins ab: die Engine bekommt hier ein Ergebnis, das kein
        // Autor gemeint haben kann.
        const shader::WaveForm* alphaWave = nullptr;

        // Der ganze Shaderblock, wenn es einen gibt. Daraus entstehen die
        // Stufen in `DrawList::groups` — jede mit Bild, Mischung, rgbGen,
        // alphaGen, tcMod und Bildfolge. Fehlt er, gilt der Ersatzshader der
        // Engine fuer ein nacktes Bild (Alphamischung, Eckpunktfarbe, ohne
        // Tiefentest).
        const shader::Shader* definition = nullptr;

        // Weder Shaderblock noch Bild dieses Namens: RE_RegisterShader gibt 0
        // zurueck, und die Engine zeichnet tr.defaultShader — ein graues
        // Kaestchen mit weissem Rand. Die Gruppe bekommt dann das Bild
        // "$default" (assets::findTexture liefert es).
        bool missing = false;
    };
    using ShaderLookup = std::function<ShaderDraw(const std::string&)>;

    // Wo die Kamera steht und wie weit sie blickt.
    //
    // Linien, Schweife und Blitze stehen in der Engine quer zur Sichtlinie
    // (RB_SurfaceLine: right = cross(start-eye, end-eye)), Zylinder werden mit
    // der Entfernung feiner (RB_SurfaceCylinder), und ein ScreenFlash steht
    // 8 Einheiten vor dem Auge (CFlash::Draw). Ohne Angabe gilt eine
    // unendlich ferne Kamera, die entlang up x right blickt.
    struct View {
        camera::Vec3 eye;
        float fovXDegrees = 90.0f;
    };
    DrawList build(float nowMs, const camera::Vec3& right,
                   const camera::Vec3& up,
                   const ShaderLookup& shaders = {},
                   const View* view = nullptr) const;

    // Wie lange läuft der Effekt insgesamt? Für die Wiederholung.
    float durationMs() const { return durationMs_; }

    // Wie viele weitere Effekte gestartet wurden, und wie viele davon nicht
    // gefunden. Für die Statuszeile und die Prüfung.
    int startedEffects() const { return startedEffects_; }
    int missingEffects() const { return missingEffects_; }

    const std::vector<Live>& live() const { return live_; }

    // Veraenderbar — fuer die Merkzeichen "schon abgespielt" und "schon
    // ausgeloest". Die gehoeren nicht in die Simulation, sondern zur
    // Wiedergabe: sie haengen daran, was der Anwender bereits gehoert oder
    // gesehen hat, nicht daran, was der Effekt tut.
    std::vector<Live>& liveForPlayback() { return live_; }

private:
    std::vector<Live> live_;
    // Die Flaechen aus play(), fuer Decals: CG_ImpactMark projiziert den
    // Abdruck auf die Flaeche, die hoechstens 20 Einheiten hinter ihm liegt.
    std::vector<sim::Plane> planes_;
    int startedEffects_ = 0;
    int missingEffects_ = 0;

    // Was beim Auslösen gilt — für den Effekt und für alle, die er startet.
    //
    // Vorher waren das neun einzelne Argumente an `playInto`, gewachsen mit
    // jeder neuen Fähigkeit: Achse, Lader, Tiefe, Versatz, Ort, Flächen. Bei
    // neun Argumenten vertauscht man zwei, ohne dass es auffällt — und die
    // Regel I.23 der C++-Leitlinien nennt genau den Grund: eine fehlende
    // Abstraktion.
    //
    // Die drei Werte, die sich beim Absteigen ändern (`depth`, `atMs`,
    // `atPosition`), stehen bewusst nicht darin: sie gehören zum Aufruf, nicht
    // zum Zustand.
    struct PlayContext {
        const EffectLoader* loader = nullptr;
        const std::vector<sim::Plane>* planes = nullptr;
        Axis axis;
    };

    // Rekursiv, mit Tiefenbegrenzung. `atMs` ist der Zeitpunkt, zu dem der
    // untergeordnete Effekt startet — er wird auf alle seine Primitiven
    // addiert.
    void playInto(const Effect& effect, sim::Random& random,
                  const std::vector<bool>& enabledMask, const PlayContext& context,
                  int depth, float atMs, const camera::Vec3& atPosition);
    float durationMs_ = 0.0f;
    bool playing_ = false;
};

// Was eine Vorschau ueber einen Effekt wissen muss.
struct PreviewInfo {
    // Wie weit er reicht, in Welteinheiten — daraus folgt der Kameraabstand.
    float reach = 1.0f;
    // Der Zeitpunkt, an dem am meisten zu sehen ist. Fuer ein Standbild.
    float bestTimeMs = 0.0f;
};

// Beides in einem Durchgang. Das System muss `play` gesehen haben.
PreviewInfo describePreview(const System& system);

// Wie weit der Effekt vom Ursprung aus sichtbar reicht, in Welteinheiten.
//
// Fuer die Kamera einer Vorschau gedacht: `distanceToFit(visualReach(system))`
// stellt sie so, dass der Effekt gerade hineinpasst.
//
// Zaehlt Bahn UND Ausdehnung. Nur die Bahn zu zaehlen war der Fehler, den der
// Browser hatte: ein Decal bewegt sich nicht, seine Bahn ist ein Punkt, die
// Reichweite kam als 1.0 heraus — und die Kamera landete mitten darin.
//
// Das System muss `play` gesehen haben; es wird nur gelesen.
float visualReach(const System& system);

// Ein ausgerichtetes Viereck: es steht nicht senkrecht zur Blickrichtung,
// sondern in einer eigenen Lage.
//
// Das ist der Unterschied zwischen Particle und OrientedParticle. Ein
// Billboard dreht sich immer zur Kamera; ein ausgerichtetes Viereck bleibt
// liegen, wo es liegt — deshalb sieht man es von der Seite als Strich.
bool addOrientedQuad(scene::Mesh& mesh, const camera::Vec3& centre,
                     const camera::Vec3& normal, float halfSize,
                     float rotationDegrees, uint32_t colour);

// Ein Zylinder wie RB_SurfaceCylinder (tr_surface.cpp): Mantel aus Segmenten,
// am Ursprung `baseRadius`, am anderen Ende (`base + axis*length`)
// `topRadius`. Ohne Deckel — die Engine zeichnet auch keine.
//
// In der Engine sitzt am URSPRUNG size2 (`backlerp`) und am fernen Ende size
// (`radius`); die Textur hat t = 1 am Ursprung und t = 0 am Ende. Der
// Aufrufer gibt also (size2, size) herein. Ist genau ein Radius kleiner als
// 0.3, wird es wie in der Engine ein Kegel (RB_SurfaceCone).
//
// Der Ring beginnt bei MakeNormalVectors(axis).up und dreht sich um die Achse
// — dieselbe Lage wie im Spiel, sonst stuende die Textur verdreht.
//
// Gibt wie addBillboard zurueck, ob der Mantel Platz hatte: die Indizes sind
// 16 Bit. Ein leerer Zylinder (Laenge null, beide Radien null, weniger als
// drei Segmente) zaehlt als gelungen.
bool addCylinder(scene::Mesh& mesh, const camera::Vec3& base,
                 const camera::Vec3& axis, float length, float baseRadius,
                 float topRadius, uint32_t colour, int segments = 16);

// Wie viele Segmente die Engine einem Zylinder gibt: RB_SurfaceCylinder
// rechnet `40 * (1 - Abstand * fovX/90 / 2048)`, begrenzt auf 8 bis 40.
int cylinderSegments(float distanceToEye, float fovXDegrees = 90.0f);

// Ein Band zwischen zwei Punkten, quer zur Sichtlinie — RB_SurfaceLine /
// DoLine in tr_surface.cpp. `halfWidth` ist der Radius der Primitive (size),
// das Band ist also 2*size breit. Textur: s quer (0..1), t von 0 am Anfang
// bis 1 am Ende. `side` ist die normierte Querrichtung.
bool addLineQuad(scene::Mesh& mesh, const camera::Vec3& start,
                 const camera::Vec3& end, const camera::Vec3& side,
                 float halfWidth, uint32_t colour);

// Die Querrichtung eines Bandes: normalize(cross(start-eye, end-eye)), wie
// in RB_SurfaceLine und RB_SurfaceElectricity.
camera::Vec3 lineSide(const camera::Vec3& start, const camera::Vec3& end,
                      const camera::Vec3& eye);

// Der Blitz, wie RB_SurfaceElectricity / DoBoltSeg / ApplyShape (tr_surface.cpp,
// SP-Fassung) ihn baut — als texturierte Baender, nicht als Linien.
//
//   - alle 16 Einheiten ein Knick, hoechstens 2000 Einheiten weit;
//   - Abweichung zufall*3 entlang, zufall*7*chaos quer, summiert, und auf
//     die Gerade zurueckgezogen (beide Enden sitzen genau);
//   - jedes Stueck bekommt per ApplyShape zwei Stufen Feinzacken;
//   - `taper`: Radius * (1 - perc^2) zur Spitze hin;
//   - `branch`: bis zu drei Abzweige in den ersten 20 % (Q_random > 0.93);
//   - `grow`: der Blitz waechst ueber `growPerc` (0..1) vom Anfang zum Ende.
//
// `boltSeed` ist mRefEnt.frame: aus ihm kommt die GROBE Form, fuer die ganze
// Lebensdauer dieselbe. `jitterSeed` ersetzt Q_flrand in CreateShape — es
// darf sich von Bild zu Bild aendern.
struct BoltShape {
    float radius = 1.0f;   // size
    float chaos = 0.0f;    // elasticity
    bool taper = false;
    bool branch = false;
    float growPerc = 1.0f;
};
bool addElectricity(scene::Mesh& mesh, const camera::Vec3& from,
                    const camera::Vec3& to, const camera::Vec3& eye,
                    const BoltShape& shape, int boltSeed, unsigned jitterSeed,
                    uint32_t colour);

// Ein einzelnes Billboard. Getrennt, weil hier die Rechnung sitzt, die man
// prüfen will: vier Ecken, um `rotation` um die Blickachse gedreht.
// `texMods` und `seconds`: die Texturkoordinaten wandern.
//
// `tcMod scroll` steht 103-mal in einer gewöhnlichen Installation, `scale` 64-
// und `rotate` 21-mal. Wir haben die Regeln gelesen, umgesetzt und geprüft —
// und dann nie angewandt. Energiestrahlen, Kraftfelder und fließende Lava
// standen damit still.
//
// Leerer Zeiger heißt: keine Verschiebung, dann bleiben die Ecken bei 0 und 1.
bool addBillboard(scene::Mesh& mesh, const camera::Vec3& centre,
                  const camera::Vec3& right, const camera::Vec3& up, float halfSize,
                  float rotationDegrees, uint32_t colour,
                  const std::vector<shader::TexMod>* texMods = nullptr,
                  float seconds = 0.0f);

}  // namespace efx::particles
