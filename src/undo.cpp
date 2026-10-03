#include "efx/undo.h"

namespace efx::undo {

void Stack::reset(const Effect& state) {
    states_.clear();
    states_.push_back({state, {}, nextId_++});
    position_ = 0;
}

void Stack::record(const Effect& state, std::string what) {
    if (states_.empty()) {
        states_.push_back({state, std::move(what), nextId_++});
        position_ = 0;
        return;
    }

    // Alles hinter der aktuellen Stelle verwerfen: wer nach einem Rueckgaengig
    // etwas Neues tut, kann das Verworfene nicht mehr wiederherstellen. So
    // verhaelt sich jedes Programm, und alles andere waere verwirrend.
    states_.resize(position_ + 1);
    states_.push_back({state, std::move(what), nextId_++});

    // Obergrenze. Faellt vorne etwas weg, wandert die aktuelle Stelle mit.
    while (states_.size() > limit_) {
        states_.erase(states_.begin());
    }
    position_ = states_.size() - 1;
}

std::string Stack::undoLabel() const {
    // Beschrieben wird die Aenderung, die zum jetzigen Zustand gefuehrt hat —
    // die also rueckgaengig gemacht wuerde.
    return canUndo() ? states_[position_].what : std::string{};
}

std::string Stack::redoLabel() const {
    return canRedo() ? states_[position_ + 1].what : std::string{};
}

const Effect* Stack::undo() {
    if (!canUndo()) return nullptr;
    --position_;
    return &states_[position_].state;
}

const Effect* Stack::redo() {
    if (!canRedo()) return nullptr;
    ++position_;
    return &states_[position_].state;
}

void Stack::clear() {
    states_.clear();
    position_ = 0;
}

}  // namespace efx::undo
