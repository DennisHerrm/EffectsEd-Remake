#include "efx/tiles.h"

#include <algorithm>
#include <cmath>

namespace efx::tiles {

std::vector<Placement> assign(std::vector<int>& owner,
                              const std::vector<int>& visible) {
    std::vector<Placement> out(visible.size());

    // Es muss mindestens so viele Plaetze wie Sichtbare geben. Fehlte einer,
    // bliebe eine Kachel ohne Platz — und eine Kachel ohne Platz zeichnet
    // nicht ins Nichts, sondern auf Platz null, wo schon jemand anders sitzt.
    if (owner.size() < visible.size()) owner.resize(visible.size(), -1);

    std::vector<bool> taken(owner.size(), false);

    // Erster Durchgang: wer schon einen Platz hat, behaelt ihn.
    //
    // Das ist der Kern der Sache. Beim Rollen wandert eine Kachel im Raster,
    // aber ihr Bild im Blatt bleibt, wo es ist — und muss deshalb nicht neu
    // gezeichnet werden.
    for (size_t i = 0; i < visible.size(); ++i) {
        for (size_t cell = 0; cell < owner.size(); ++cell) {
            if (taken[cell] || owner[cell] != visible[i]) continue;
            out[i].cell = static_cast<int>(cell);
            taken[cell] = true;
            break;
        }
    }

    // Zweiter Durchgang: die uebrigen bekommen einen freien Platz.
    //
    // Frei heisst: in diesem Bild von niemandem beansprucht. Der Vorbesitzer
    // ist entweder weggerollt oder weggefiltert; sein Bild wird ueberschrieben,
    // und deshalb gilt der Neue als `fresh`.
    size_t nextFree = 0;
    for (size_t i = 0; i < visible.size(); ++i) {
        if (out[i].cell >= 0) continue;
        while (nextFree < owner.size() && taken[nextFree]) ++nextFree;
        if (nextFree >= owner.size()) {
            // Kann nach dem resize oben nicht eintreten — aber wenn doch,
            // dann lieber einen Platz anhaengen als still auf null zeichnen.
            owner.push_back(-1);
            taken.push_back(false);
        }
        owner[nextFree] = visible[i];
        out[i].cell = static_cast<int>(nextFree);
        out[i].fresh = true;
        taken[nextFree] = true;
    }

    return out;
}

size_t sheetColumns(size_t cellCount) {
    if (cellCount == 0) return 1;
    // Moeglichst quadratisch: das haelt das Blatt klein, und klein heisst
    // weniger Bildpunkte zu loeschen und zu fuellen.
    const auto columns = static_cast<size_t>(
        std::ceil(std::sqrt(static_cast<double>(cellCount))));
    return columns > 0 ? columns : 1;
}

UvRect uvFor(int x, int y, int size, int sheetWidth, int sheetHeight,
             bool flipped) {
    UvRect out;
    if (sheetWidth <= 0 || sheetHeight <= 0 || size <= 0) return out;

    const float width = static_cast<float>(sheetWidth);
    const float height = static_cast<float>(sheetHeight);

    out.u0 = static_cast<float>(x) / width;
    out.u1 = static_cast<float>(x + size) / width;

    const float top = static_cast<float>(y) / height;
    const float bottom = static_cast<float>(y + size) / height;

    if (flipped) {
        // Die ganze Textur ist gespiegelt, also auch die Lage des
        // Ausschnitts darin. `1 - top` ist die Oberkante von unten gezaehlt.
        out.v0 = 1.0f - top;
        out.v1 = 1.0f - bottom;
    } else {
        out.v0 = top;
        out.v1 = bottom;
    }
    return out;
}

}  // namespace efx::tiles
