// Die Auswahllogik der Grafikschnittstelle.
//
// Bewusst ohne Direct3D- und OpenGL-Header: das Anlaufen selbst kann hier
// niemand prüfen, die Entscheidung darüber, was bei welchem Ausgang passiert,
// sehr wohl. Und genau dort sitzen die Fehler — nicht im Anlegen des Geräts,
// sondern in dem, was das Programm tut, wenn es schiefgeht.
#include "efx/renderer.h"

#include "efx/i18n.h"
#include "efx/shader.h"

// Die beiden Aufzaehlungen muessen Wert fuer Wert uebereinstimmen: gui/app.cpp
// setzt eine direkt in die andere um. Weichen sie ab, mischt die Vorschau
// stillschweigend falsch — und bei einem Funken faellt das kaum auf.
//
// Steht hier und nicht im Kopf, weil renderer.h sonst shader.h einbinden
// muesste, nur um eine Behauptung aufzustellen.
static_assert(static_cast<int>(efx::shader::BlendMode::Opaque) ==
                  static_cast<int>(efx::render::Blend::Opaque),
              "BlendMode und Blend muessen dieselbe Reihenfolge haben");
static_assert(static_cast<int>(efx::shader::BlendMode::Additive) ==
                  static_cast<int>(efx::render::Blend::Additive), "Additive");
static_assert(static_cast<int>(efx::shader::BlendMode::AlphaBlend) ==
                  static_cast<int>(efx::render::Blend::AlphaBlend), "AlphaBlend");
static_assert(static_cast<int>(efx::shader::BlendMode::Modulate) ==
                  static_cast<int>(efx::render::Blend::Modulate), "Modulate");
static_assert(static_cast<int>(efx::shader::BlendMode::Filter) ==
                  static_cast<int>(efx::render::Blend::Filter), "Filter");

namespace efx::render {

const char* backendName(Backend backend) {
    switch (backend) {
        case Backend::Direct3D11: return "Direct3D 11";
        case Backend::OpenGL3: return "OpenGL 3.3";
    }
    return "unbekannt";
}

const char* backendCode(Backend backend) {
    switch (backend) {
        case Backend::Direct3D11: return "d3d11";
        case Backend::OpenGL3: return "gl3";
    }
    return "d3d11";
}

bool backendFromCode(const std::string& code, Backend& out) {
    if (code == "d3d11") {
        out = Backend::Direct3D11;
        return true;
    }
    if (code == "gl3") {
        out = Backend::OpenGL3;
        return true;
    }
    return false;
}

std::vector<Backend> fallbackOrder(Backend preferred) {
    // Die gewünschte zuerst, dann die andere. Bei zwei Schnittstellen ist das
    // trivial; als Liste geschrieben, damit ein späteres Direct3D 12 oder
    // Vulkan nur einen Eintrag kostet und nicht eine neue Verzweigung.
    std::vector<Backend> order{preferred};
    for (Backend candidate : {Backend::Direct3D11, Backend::OpenGL3}) {
        if (candidate != preferred) order.push_back(candidate);
    }
    return order;
}

bool chooseBackend(Backend preferred, const std::vector<Probe>& probes,
                   Backend& chosen, std::string& note) {
    note.clear();

    auto findProbe = [&](Backend backend) -> const Probe* {
        for (const auto& p : probes) {
            if (p.backend == backend) return &p;
        }
        return nullptr;
    };

    for (Backend candidate : fallbackOrder(preferred)) {
        const Probe* p = findProbe(candidate);
        if (!p || !p->available) continue;

        chosen = candidate;
        if (candidate != preferred) {
            // Der Rückfall wird gemeldet, nicht stillschweigend genommen. Wer
            // Direct3D eingestellt hat und OpenGL bekommt, soll das erfahren
            // und den Grund lesen können — sonst sucht er beim nächsten
            // Fehler an der falschen Stelle.
            const Probe* wanted = findProbe(preferred);
            note = std::string(backendName(preferred)) + " steht nicht zur "
                   "Verfügung";
            if (wanted && !wanted->failure.empty()) {
                note += " (" + wanted->failure + ")";
            }
            note += ", es wird " + std::string(backendName(candidate)) +
                    " benutzt.";
        }
        return true;
    }

    // Keine läuft. Alle Gründe zusammentragen, damit die Meldung brauchbar ist.
    note = i18n::tr(i18n::Str::RendererNone);
    for (const auto& p : probes) {
        note += "\n  " + std::string(backendName(p.backend)) + ": " +
                (p.failure.empty() ? "unbekannter Grund" : p.failure);
    }
    return false;
}

}  // namespace efx::render
