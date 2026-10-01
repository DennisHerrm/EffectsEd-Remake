#include "efx/fields.h"

#include <algorithm>

namespace efx::fields {
namespace {

using i18n::Str;

struct TabInfo {
    Tab tab;
    const char* name;
    Str label;
};

const TabInfo kTabs[] = {
    {Tab::Generation, "Generation", Str::TabGeneration},
    {Tab::OriginSize, "Origin/Size", Str::TabOriginSize},
    {Tab::Color, "Color", Str::TabColor},
    {Tab::Motion, "Motion", Str::TabMotion},
    {Tab::Physics, "Physics", Str::TabPhysics},
    {Tab::Line, "Line", Str::TabLine},
    {Tab::Tail, "Tail", Str::TabTail},
    {Tab::LengthSize2, "Length/Size2", Str::TabLengthSize2},
    {Tab::Model, "Model", Str::TabModel},
    {Tab::Emitter, "Emitter", Str::TabEmitter},
    {Tab::Sound, "Sound", Str::TabSound},
    {Tab::FxRunner, "FxRunner", Str::TabFxRunner},
    {Tab::CameraShake, "CameraShake", Str::TabCameraShake},
};

}  // namespace

const char* tabName(Tab tab) {
    for (const auto& info : kTabs) {
        if (info.tab == tab) return info.name;
    }
    return "";
}

i18n::Str tabLabel(Tab tab) {
    for (const auto& info : kTabs) {
        if (info.tab == tab) return info.label;
    }
    return Str::TabGeneration;
}

// Die feste Seitenreihenfolge des Originals.
//
// Der Editor haengt nicht je Typ eine eigene Liste an, sondern filtert diese
// eine Reihenfolge. Das ist keine Vermutung: sieben gemessene Typen —
// CameraShake, Cylinder, Decal, Line, Electricity, Emitter, Flash — passen
// alle widerspruchsfrei hinein, und keine zwei widersprechen sich.
//
// Der auffaelligste Punkt daran: **Color steht ganz hinten**, nicht in der
// Mitte. Ich hatte es bei Particle an dritter Stelle. Beim Emitter faellt es
// ganz weg, und genau das machte das Muster sichtbar.
const Tab kPageOrder[] = {
    Tab::Generation,  Tab::OriginSize, Tab::LengthSize2, Tab::Line,
    Tab::Motion,      Tab::Physics,    Tab::Emitter,     Tab::Model,
    Tab::Sound,       Tab::FxRunner,   Tab::CameraShake, Tab::Color,
};

// Zur Reihenfolge:
//
// Length/Size2 steht VOR Motion, nicht hinter Physics. Aufgefallen ist das erst
// beim dreizehnten und letzten Typ: der Tail ist der einzige, der beide Seiten
// hat. Zwoelf Typen passten auch zur falschen Reihenfolge, weil sie nie beide
// gleichzeitig zeigen.
//
// Wo genau Line zwischen Length/Size2 und Motion sitzt, ist unbestimmt — kein
// Typ hat beide. Die Stelle ist deshalb gewaehlt, nicht gemessen.

namespace {

// Hat dieser Typ diese Seite? Die Zugehoerigkeit steht getrennt von der
// Reihenfolge — sonst muss man beim Hinzufuegen eines Typs an zwei Stellen
// denken.
bool hasTab(PrimitiveType type, Tab tab) {
    switch (tab) {
        case Tab::Generation:
            return true;

        case Tab::OriginSize:
            // Auch CameraShake und Flash: bei beiden haengt die Staerke vom
            // Abstand zum Ursprung ab. Beide hatte ich zuerst weggelassen.
            return true;

        case Tab::Motion:
            return type == PrimitiveType::Particle ||
                   type == PrimitiveType::OrientedParticle ||
                   type == PrimitiveType::Tail ||
                   type == PrimitiveType::Emitter;

        case Tab::Physics:
            return type == PrimitiveType::Particle ||
                   type == PrimitiveType::OrientedParticle ||
                   type == PrimitiveType::Tail ||
                   type == PrimitiveType::Emitter;

        case Tab::Line:
            return type == PrimitiveType::Line ||
                   type == PrimitiveType::Electricity;

        case Tab::LengthSize2:
            return type == PrimitiveType::Cylinder ||
                   type == PrimitiveType::Tail;

        case Tab::Tail:
            return false;  // in LengthSize2 aufgegangen, wie im Original

        case Tab::Emitter:
            return type == PrimitiveType::Emitter;

        case Tab::Model:
            return type == PrimitiveType::Emitter;

        case Tab::Sound:
            return type == PrimitiveType::Sound;

        case Tab::FxRunner:
            return type == PrimitiveType::FxRunner;

        case Tab::CameraShake:
            return type == PrimitiveType::CameraShake;

        case Tab::Color:
            // Im Original hat der Emitter keine Farbseite. Wir haengen sie
            // trotzdem an: FX_AddEmitter nimmt rgb und alpha entgegen, die
            // Engine kann es also.
            return type != PrimitiveType::Sound &&
                   type != PrimitiveType::CameraShake &&
                   type != PrimitiveType::FxRunner;
    }
    return false;
}

}  // namespace

std::vector<Tab> tabsFor(PrimitiveType type) {
    std::vector<Tab> tabs;
    for (Tab tab : kPageOrder) {
        if (hasTab(type, tab)) tabs.push_back(tab);
    }
    return tabs;
}

bool applies(PrimitiveType type, Field field) {
    const auto tabs = tabsFor(type);
    auto has = [&](Tab tab) {
        return std::find(tabs.begin(), tabs.end(), tab) != tabs.end();
    };

    switch (field) {
        case Field::Delay:
            return true;

        case Field::Count:
            // Bei Light und CameraShake ist "Count" ausgegraut — beide
            // gemessen. Beide erzeugen genau eine Sache an einer Stelle:
            // FX_AddLight setzt ein Licht, CameraShake ruettelt einmal.
            // Mehrere davon uebereinander waeren sinnlos.
            //
            // Deshalb ist dort auch "Use even delay distribution" ausgegraut:
            // das verteilt eine Anzahl ueber die Verzoegerungsspanne, und ohne
            // Anzahl gibt es nichts zu verteilen.
            return type != PrimitiveType::Light &&
                   type != PrimitiveType::CameraShake;

        case Field::Life:
            // Beim FxRunner ist "Life (ms)" ausgegraut — gemessen. Er loest
            // andere Effekte aus und hat selbst keine Lebensdauer;
            // PlayEffect(mPlayFxHandles, org, ax) liest sie auch nicht.
            // Dasselbe gilt fuer den Sound: der Klang bringt seine Laenge
            // selbst mit.
            return type != PrimitiveType::FxRunner &&
                   type != PrimitiveType::Sound;

        // Sichtweite nur bei etwas, das gezeichnet wird.
        case Field::CullRange:
            // Auch beim CameraShake — im Original steht "Use distance
            // culling" dort ebenfalls, und zwar bedienbar. Es passt auch zur
            // Sache: die Erschuetterung wird ohnehin mit dem Abstand
            // schwaecher (CG_ExplosionEffects), eine Abschneidegrenze ist die
            // grobe Variante davon.
            //
            // Beim Sound nicht: der wird ueber die Lautstaerke gedaempft,
            // nicht ueber eine Entfernungsgrenze.
            return type != PrimitiveType::Sound;

        case Field::Origin:
            // Fuer jeden Typ: der Ursprungsblock laeuft vor der Verzweigung
            // nach Typ. Selbst ein Sound braucht ihn fuer die Panoramierung.
            return true;
        case Field::Origin2:
            return has(Tab::Line);
        case Field::MinMax:
            // Die Begrenzungsbox gehoert zur Physik, nicht zur Lage.
            //
            // Im Original steht sie in Dialog 161 unter "Bounding Box" mit
            // "Enable physics bounding box" — also zusammen mit dem Flag
            // useBBox, das sie erst wirksam macht. Ich hatte sie unter
            // Origin/Size, wo sie wie eine Lageangabe aussieht.
            return has(Tab::Physics);

        case Field::Radius:
            // Doppelt belegt: bei CameraShake die Reichweite, sonst der Radius
            // der Kugel- oder Zylinderverteilung (FX_ORG_ON_SPHERE /
            // FX_ORG_ON_CYLINDER). Beides liest dieselbe Zahl.
            //
            // Bei Flash, Light und Sound ist "Enable special offset types"
            // ausgegraut, also gibt es dort weder Radius noch Hoehe. Alle
            // drei gemessen.
            //
            // Beim Sound ist das einleuchtend: eine Kugel- oder
            // Zylinderverteilung streut den Ursprung, und ein Klang wird an
            // genau einer Stelle abgespielt.
            return type != PrimitiveType::ScreenFlash &&
                   type != PrimitiveType::Light &&
                   type != PrimitiveType::Sound;
        case Field::Height:
            // Gehoert zur Kugel- und Zylinderverteilung und gilt damit
            // ueberall — ausser beim CameraShake, wo mRadius allein die
            // Reichweite bestimmt, und beim Flash.
            return type != PrimitiveType::CameraShake &&
                   type != PrimitiveType::ScreenFlash;

        case Field::Rgb:
            return has(Tab::Color);
        case Field::Alpha:
            // Licht mischt ueber die Farbe, nicht ueber Alpha.
            return has(Tab::Color) && type != PrimitiveType::Light;
        case Field::Size:
            // Beim FxRunner ist die ganze Gruppe "Size/Width" ausgegraut —
            // gemessen. Er zeichnet nichts, also gibt es keine Groesse.
            return type != PrimitiveType::FxRunner &&
                   type != PrimitiveType::Sound &&
                   type != PrimitiveType::CameraShake;
        case Field::Size2:
            // Nur der Cylinder. Im Spielcode liest ausschliesslich
            // FX_AddCylinder die drei mSize2-Werte; FX_AddTail nimmt sie nicht
            // entgegen. Im Bild ist die Gruppe "Size2/Width2" beim Tail
            // entsprechend ausgegraut.
            return type == PrimitiveType::Cylinder;
        case Field::Length:
            return has(Tab::Tail) || has(Tab::LengthSize2);

        case Field::Velocity:
        case Field::Acceleration:
            return has(Tab::Motion);
        case Field::Angles:
        case Field::AngleDelta:
            // Im Original in Dialog 179 (Model), beschriftet Pitch/Yaw/Roll —
            // sie richten das angehaengte Modell aus. Ich hatte sie unter
            // Motion, wo sie wie eine Bewegung aussehen.
            return has(Tab::Model) || type == PrimitiveType::OrientedParticle ||
                   type == PrimitiveType::Decal;
        case Field::Rotation:
            // Genau drei Typen lesen mRotation. Der Spielcode hat dafuer vier
            // Aufrufstellen, mehr nicht:
            //
            //   FX_AddParticle(..., mRotation, mRotationDelta, ...)   zweimal
            //   CG_ImpactMark(handle, org, ax[0], mRotation, ...)     Decal
            //   FX_AddOrientedParticle(..., mRotation, mRotationDelta, ...)
            //
            // Das erklaert die Bilder: beim Emitter ist die Drehung im
            // Motion-Reiter ausgegraut, beim Line fehlt sie ganz. Ich hatte
            // sie fuer jeden Typ mit Lage- oder Bewegungsseite freigegeben.
            return type == PrimitiveType::Particle ||
                   type == PrimitiveType::OrientedParticle ||
                   type == PrimitiveType::Decal;

        case Field::RotationDelta:
            // CG_ImpactMark nimmt nur mRotation entgegen, keine Aenderung —
            // ein Decal liegt still auf der Wand. Die beiden Partikelarten
            // bekommen beides.
            return type == PrimitiveType::Particle ||
                   type == PrimitiveType::OrientedParticle;

        case Field::Gravity:
        case Field::Wind:
            // Beide gehoeren zur Bewegung, nicht zur Physik. Und beide sind im
            // alten Editor ueberhaupt nicht zu erreichen: die Engine liest
            // mGravity in FX_AddParticle, aber keine der zwoelf
            // Eigenschaftsseiten hat ein Feld dafuer. Wir zeigen sie.
            return has(Tab::Motion);
        case Field::Density:
        case Field::Variance:
            // Density und Variance stehen im Original in Dialog 176 (Emitter).
            // Variance dient beim Electricity zusaetzlich als "Chaos" — Dialog
            // 172 nennt es so.
            return has(Tab::Emitter) || has(Tab::Line);
        case Field::Bounce:
            // Bei CameraShake ist bounce die Staerke — dasselbe Feld, andere
            // Bedeutung. Genau die Doppelbelegung, die Ravens Editor nirgends
            // erklaert.
            return has(Tab::Physics) || type == PrimitiveType::CameraShake;

        case Field::Shaders:
            return type != PrimitiveType::Sound &&
                   type != PrimitiveType::CameraShake &&
                   type != PrimitiveType::FxRunner &&
                   type != PrimitiveType::Light;
        case Field::Models:
            return has(Tab::Model);
        case Field::Sounds:
            return has(Tab::Sound);
        case Field::PlayFx:
            return has(Tab::FxRunner);
        case Field::EmitFx:
            return has(Tab::Emitter);
        case Field::ImpactFx:
            // Dialog 161 "Play new effect on impact", Dialog 172
            // "Play new effect at endpoint".
            return has(Tab::Physics) || has(Tab::Line);
        case Field::DeathFx:
            // Dialog 152 (Generation) hat den Kasten "Death Effects" mit
            // "Enable Death Effects". Beim Flash ist er ausgegraut — gemessen,
            // nicht abgeleitet: ein Aufblitzen hinterlaesst nichts.
            return type != PrimitiveType::Sound &&
                   type != PrimitiveType::CameraShake &&
                   type != PrimitiveType::ScreenFlash &&
                   type != PrimitiveType::FxRunner &&
                   type != PrimitiveType::Light;
    }
    return false;
}

const char* fieldName(Field field) {
    switch (field) {
        case Field::Count: return "count";
        case Field::Life: return "life";
        case Field::Delay: return "delay";
        case Field::CullRange: return "cullrange";
        case Field::Origin: return "origin";
        case Field::Origin2: return "origin2";
        case Field::Radius: return "radius";
        case Field::Height: return "height";
        case Field::MinMax: return "min/max";
        case Field::Rgb: return "rgb";
        case Field::Alpha: return "alpha";
        case Field::Size: return "size";
        case Field::Size2: return "size2";
        case Field::Length: return "length";
        case Field::Velocity: return "velocity";
        case Field::Acceleration: return "acceleration";
        case Field::Angles: return "angles";
        case Field::AngleDelta: return "angleDelta";
        case Field::Rotation: return "rotation";
        case Field::RotationDelta: return "rotationDelta";
        case Field::Gravity: return "gravity";
        case Field::Bounce: return "bounce";
        case Field::Density: return "density";
        case Field::Variance: return "variance";
        case Field::Wind: return "wind";
        case Field::Shaders: return "shaders";
        case Field::Models: return "models";
        case Field::Sounds: return "sounds";
        case Field::PlayFx: return "playfx";
        case Field::ImpactFx: return "impactfx";
        case Field::DeathFx: return "deathfx";
        case Field::EmitFx: return "emitfx";
    }
    return "";
}

std::vector<Field> uselessFields(const Primitive& p) {
    std::vector<Field> useless;
    auto note = [&](bool isSet, Field field) {
        if (isSet && !applies(p.type, field)) useless.push_back(field);
    };

    note(p.origin2.set, Field::Origin2);
    note(p.rgb.present, Field::Rgb);
    note(p.alpha.present, Field::Alpha);
    note(p.size2.present, Field::Size2);
    note(p.length.present, Field::Length);
    note(p.velocity.set, Field::Velocity);
    note(p.acceleration.set, Field::Acceleration);
    note(p.rotation.set, Field::Rotation);
    note(p.rotationDelta.set, Field::RotationDelta);
    note(p.gravity.set, Field::Gravity);
    note(p.density.set, Field::Density);
    note(p.variance.set, Field::Variance);
    note(p.windModifier.set, Field::Wind);
    note(!p.shaders.empty(), Field::Shaders);
    note(!p.models.empty(), Field::Models);
    note(!p.sounds.empty(), Field::Sounds);
    note(!p.playFx.empty(), Field::PlayFx);
    note(!p.emitFx.empty(), Field::EmitFx);
    return useless;
}

}  // namespace efx::fields
