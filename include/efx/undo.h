// Rückgängig und Wiederherstellen.
//
// Als Abzüge des ganzen Effekts, nicht als Liste einzelner Änderungen.
//
// Das ist die unelegantere Bauart, aber die richtige hier: eine .efx-Datei hat
// höchstens 24 Primitiven mit je ein paar Dutzend Zahlen — ein Abzug sind
// wenige Kilobyte. Einzelne Änderungen zu verfolgen hieße, für jedes Feld eine
// eigene Rückgängig-Klasse zu schreiben, und ein vergessenes Feld fiele erst
// auf, wenn jemand es rückgängig machen will und nichts passiert.
//
// Bei einem Programm, in dem man an Zahlen dreht, ist das der falsche Ort für
// Eleganz.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "efx/effect.h"

namespace efx::undo {

// Die Liste hält immer den **aktuellen** Zustand an der aktuellen Stelle.
//
// Der erste Entwurf merkte den Zustand *vor* einer Änderung. Das klingt
// natürlicher, führt aber dazu, dass der jetzige Stand nirgends steht — und
// beim ersten Rückgängig nachträglich angehängt werden muss, damit das
// Wiederherstellen irgendwohin zurückführen kann. Dafür brauchte es ein
// Merkzeichen, und Merkzeichen dieser Art sind fast immer ein Zeichen, dass
// die Bauart nicht stimmt.
//
// So herum ist es geradeaus: `reset` legt den Anfangszustand ab, `record` den
// Zustand *nach* jeder Änderung.
class Stack {
public:
    explicit Stack(size_t limit = 64) : limit_(limit ? limit : 1) {}

    // Legt den Anfangszustand fest und wirft alles Bisherige weg.
    void reset(const Effect& state);

    // Merkt den Zustand **nach** einer Änderung. `what` beschreibt sie, damit
    // das Menü „Rückgängig: Segment gelöscht" anzeigen kann.
    void record(const Effect& state, std::string what);

    bool canUndo() const { return position_ > 0; }
    bool canRedo() const { return position_ + 1 < states_.size(); }

    // Beschreibung dessen, was rückgängig gemacht bzw. wiederholt würde.
    std::string undoLabel() const;
    std::string redoLabel() const;

    // Geben den Zustand zurück, auf den umgeschaltet werden soll, oder
    // nullptr, wenn es nichts gibt.
    const Effect* undo();
    const Effect* redo();

    void clear();
    size_t size() const { return states_.size(); }

private:
    struct Entry {
        Effect state;
        std::string what;
    };
    std::vector<Entry> states_;
    size_t position_ = 0;
    size_t limit_;
};

}  // namespace efx::undo
