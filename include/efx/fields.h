// Welche Reiter und Felder gelten für welchen Primitivtyp?
//
// Nicht geraten: hergeleitet aus dem `switch (mType)` in `CreateEffect` und
// `PlayEffect` (code/cgame/FxScheduler.cpp). Dort steht Zeile für Zeile, was
// die Engine je Typ tatsächlich ausliest. Ein `Sound` liest nur
// `mMediaHandles` und `mSpawnFlags`, ein `CameraShake` nur `mElasticity`,
// `mRadius` und `mLife` — alles andere daran ist wirkungslos.
//
// Ravens Anleitung sagt dazu nur: „Generation und Origin/Size immer, der Rest
// je nach Typ". Welcher Rest, steht dort nicht.
//
// Die Tabelle liegt im Kern und nicht in der Oberfläche, damit sie geprüft
// werden kann — ein falscher Eintrag heißt sonst, dass jemand ein Feld
// ausfüllt, das im Spiel nichts tut.
#pragma once

#include <vector>

#include "efx/effect.h"
#include "efx/i18n.h"

namespace efx::fields {

enum class Tab {
    Generation,   // Name, count, life, delay, cullrange, Flags
    OriginSize,   // origin, origin2, radius, height, min, max, size
    Color,        // rgb, alpha
    Motion,       // velocity, acceleration, angles, rotation
    Physics,      // gravity, bounce, density, variance, wind
    Line,         // origin2, spawnflags fuer Trace
    Tail,         // length
    LengthSize2,  // size2, length
    Model,        // models
    Emitter,      // emitfx, density, variance
    Sound,        // sounds
    FxRunner,     // playfx
    CameraShake,  // bounce als Staerke, radius, life
};

const char* tabName(Tab tab);
i18n::Str tabLabel(Tab tab);

// Die Reiter für einen Typ, in der Reihenfolge, in der sie erscheinen sollen.
std::vector<Tab> tabsFor(PrimitiveType type);

// Wertet die Engine dieses Feld für diesen Typ überhaupt aus?
enum class Field {
    Count, Life, Delay, CullRange,
    Origin, Origin2, Radius, Height, MinMax,
    Rgb, Alpha, Size, Size2, Length,
    Velocity, Acceleration, Angles, AngleDelta, Rotation, RotationDelta,
    Gravity, Bounce, Density, Variance, Wind,
    Shaders, Models, Sounds, PlayFx, ImpactFx, DeathFx, EmitFx,
};

bool applies(PrimitiveType type, Field field);

// Für die Prüfung: alle Felder, die gesetzt sind, obwohl der Typ sie nicht
// ausliest. Das ist die häufigste Art, Zeit zu verlieren — man dreht an einer
// Zahl, die niemand liest.
std::vector<Field> uselessFields(const Primitive& primitive);
const char* fieldName(Field field);

}  // namespace efx::fields
