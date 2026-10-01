// Der Selbsttest der Oberflaeche — siehe gui/selbsttest.cpp.
//
// Eingeschaltet ueber die Umgebungsvariable EFXED_SELBSTTEST. Ohne sie tun
// alle Funktionen hier nichts.
#pragma once

namespace efx::render {
class Renderer;
}

namespace efx::gui {

class App;

// Vor allem anderen in wWinMain: ist der Test an? Wenn ja, wird auch der
// Ordner fuer Einstellungen umgelenkt, damit der Test nie die echten
// Einstellungen des Anwenders liest oder schreibt.
bool selbsttestVorbereiten();
bool selbsttestAktiv();

// Unmittelbar nach ImGui_ImplWin32_NewFrame, vor ImGui::NewFrame.
void selbsttestVorBild();
// Nach App::buildFrame, vor ImGui::Render.
void selbsttestNachBild(App& app, render::Renderer* renderer);

}  // namespace efx::gui
