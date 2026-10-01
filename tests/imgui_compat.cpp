// Getrennter Test für die ImTextureID-Umwandlung.
//
// Ohne ImGui: der Typ wird beide Male nachgestellt, einmal als Zeiger (bis
// 1.91.3) und einmal als 64-Bit-Zahl (ab 1.91.4). Damit lässt sich hier
// prüfen, was sonst erst auf dem Zielrechner auffällt.
//
// Anlass: der erste Versuch der Hilfsfunktion war keine Vorlage. `if constexpr`
// verwirft den nicht genommenen Zweig aber nur innerhalb einer Vorlage — in
// einer gewöhnlichen Funktion werden beide vollständig geprüft, und der Bau
// scheiterte. Meine eigene Nachrechnung hatte damals versehentlich eine
// Vorlage benutzt und deshalb nichts gemerkt.
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "efx/renderer.h"

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char* what) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FEHLER: %s\n", what);
    }
}

// Wortgleich mit gui/theme_imgui.h. Weicht die dort ab, faellt es hier auf.
template <typename Target>
inline Target textureIdAs(efx::render::TextureId id) {
    if constexpr (std::is_pointer_v<Target>) {
        return reinterpret_cast<Target>(id);
    } else {
        return static_cast<Target>(id);
    }
}

}  // namespace

int main() {
    std::printf("Umwandlung der Texturkennung\n\n");

    check(sizeof(efx::render::TextureId) >= sizeof(void*),
          "TextureId nimmt einen Zeiger auf");
    std::printf("  sizeof(TextureId) = %zu, sizeof(void*) = %zu\n",
                sizeof(efx::render::TextureId), sizeof(void*));

    // Direct3D legt dort einen Zeiger ab.
    int dummy = 7;
    const auto id = reinterpret_cast<efx::render::TextureId>(&dummy);

    // ImGui bis 1.91.3: ImTextureID war void*.
    void* asPointer = textureIdAs<void*>(id);
    check(asPointer == &dummy, "Zeiger ueberlebt den Umweg unveraendert");

    // ImGui ab 1.91.4: ImTextureID ist ImU64.
    using ImU64Like = unsigned long long;
    const ImU64Like asU64 = textureIdAs<ImU64Like>(id);
    check(asU64 == static_cast<ImU64Like>(id), "als 64-Bit-Zahl unveraendert");

    // Der Rueckweg, den der Renderer beim Freigeben braucht.
    void* back = reinterpret_cast<void*>(static_cast<efx::render::TextureId>(asU64));
    check(back == &dummy, "Rueckweg liefert denselben Zeiger");

    // OpenGL legt dort eine Nummer ab — auch die darf nichts verlieren.
    const efx::render::TextureId glName = 4294967295u;  // groesstes GLuint
    check(textureIdAs<ImU64Like>(glName) == 4294967295ull,
          "grosse OpenGL-Nummer bleibt unveraendert");

    check(efx::render::kNoTexture == 0, "keine Textur ist null");

    std::printf("\n%d Pruefungen, %d Fehler\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
