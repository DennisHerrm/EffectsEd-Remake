// Der Auto-Updater in der Oberflaeche: Netz, Installation, Fenster.
//
// Der pruefbare Kern steht in include/efx/update.h. Hier: GitHub fragen
// (WinHTTP, im Hintergrund), das .zip laden und neben die .exe entpacken,
// neu starten — und was man davon sieht: ein Hinweis in der Statuszeile und
// ein kleines Fenster mit den Notizen des Releases.
#pragma once

#include <string>

#include "efx/update.h"

namespace efx::gui::updater {

enum class UpdatePhase { Idle, Checking, Current, Available, Downloading, Installed, Failed };

struct UpdateInfo {
    UpdatePhase state = UpdatePhase::Idle;
    std::string message;  // bei Failed: was schiefging
    update::Release release;
    double progress = 0.0;  // 0..1 beim Laden
    int files = 0;          // so viele wurden ersetzt
};

// Beim Programmstart: Reste eines frueheren Updates wegraeumen und — wenn
// eingeschaltet und kein Selbsttest laeuft — im Hintergrund nachsehen.
void atStartup(bool checkNow);
// Nachsehen. `quiet`: nur der Hinweis in der Statuszeile, kein Fenster.
void check(bool quiet);
// Das gefundene Release laden und installieren (im Hintergrund).
void install();
// Eine Kopie des Zustands (der Hintergrundfaden schreibt ihn).
[[nodiscard]] UpdateInfo status();
// Die Fassung, gegen die verglichen wird.
[[nodiscard]] std::string localVersion();

// Einmal je Bild. drawWindow gibt true zurueck, wenn "Jetzt neu starten"
// gedrueckt wurde — das Beenden (mit der Frage nach ungespeicherten
// Aenderungen) erledigt die App, den Neustart danach launchNewVersion().
bool drawWindow();
void drawStatusHint();
void openWindow();
[[nodiscard]] bool windowOpen();

// Nach dem Beenden: die neue .exe starten. Sie wartet, bis diese Instanz
// ihre Einstellungen fertig geschrieben hat.
void launchNewVersion();
// Aus main: "--nach-update=<pid>" — auf das Ende der alten Instanz warten.
// true, wenn das Argument dieses war (dann ist es keine Datei zum Oeffnen).
bool waitForPredecessor(const std::string& argument);

}  // namespace efx::gui::updater
