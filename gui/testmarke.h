// Testmarken: feste Namen fuer Bedienelemente ohne sichtbare Beschriftung.
//
// Der Selbsttest (gui/selbsttest.cpp) findet Knoepfe ueber ihre Beschriftung.
// Die Zahlenfelder der Eigenschaftsseite heissen aber alle "##min" und
// "##max" — vierzig Stueck auf einer Seite, und welches zu "Count" gehoert,
// steht nur im ID-Stapel von ImGui, den man von aussen nicht lesen kann.
//
// Deshalb melden die Bausteine in app_widgets.cpp ihr Element zusaetzlich
// unter einem Pfad wie "count/min" oder "size/start/max". Der Pfad entsteht
// aus Bereichen (`Scope`), die genau wie ImGuis PushID geschachtelt werden.
//
// Ausserhalb des Selbsttests ist `aktiv()` falsch, und alles hier kostet eine
// Abfrage.
#pragma once

namespace efx::gui::testmarke {

bool aktiv();
void bereichBetreten(const char* name);
void bereichVerlassen();
// Das zuletzt gezeichnete Element unter <Bereichspfad>/<name> melden.
void marke(const char* name);

struct Bereich {
    bool an;
    explicit Bereich(const char* name) : an(aktiv()) {
        if (an) bereichBetreten(name);
    }
    ~Bereich() {
        if (an) bereichVerlassen();
    }
    Bereich(const Bereich&) = delete;
    Bereich& operator=(const Bereich&) = delete;
};

}  // namespace efx::gui::testmarke
