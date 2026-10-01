// Eigene Einstellungen fuer Dear ImGui, eingebunden ueber IMGUI_USER_CONFIG.
//
// Der einzige Zweck: die Haken der ImGui-Testumgebung einschalten. Jedes
// Element meldet dann Kennung, Beschriftung und Rechteck — damit klickt der
// Selbsttest (gui/selbsttest.cpp) Knoepfe genau dort, wo ImGui sie gezeichnet
// hat. ImGui ruft die Haken nur, solange `TestEngineHookItems` gesetzt ist,
// und das setzt allein der Selbsttest. Im normalen Betrieb kostet es eine
// Abfrage je Element.
//
// Vorbild ist behaved: dort hat derselbe Aufbau in einer Sitzung gefunden,
// was zehn Runden ueber Protokolle des Anwenders nicht gefunden hatten.
#pragma once

#define IMGUI_ENABLE_TEST_ENGINE
