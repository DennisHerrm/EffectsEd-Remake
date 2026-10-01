// Ressourcennummern. Wird von gui/efxed.rc UND von gui/main_win32.cpp
// eingebunden — deshalb ohne C++ darin, der Ressourcenuebersetzer versteht
// nur den Praeprozessor.
//
// Eine gemeinsame Datei statt zweier Zahlen an zwei Stellen: fielen sie
// auseinander, zeigte der Explorer das richtige Symbol und die Titelzeile das
// Windows-Standardsymbol. Ein Fehler, der nur zur Haelfte sichtbar ist und
// deshalb lange unbemerkt bleibt.
#pragma once

#define IDI_APPICON 101
