// ImGuis DX11-Rücken, aber ohne feste Bindung an d3dcompiler.
//
// Die Fremddatei wird unverändert eingebunden; nur `D3DCompile` zeigt vorher
// auf unsere Fassung, die die DLL zur Laufzeit sucht.
//
// Diese Datei ersetzt `imgui_impl_dx11.cpp` im Bau — die Originaldatei darf
// nicht zusätzlich übersetzt werden, sonst gibt es die Funktionen doppelt.
#include "imgui_dx11_nodll.h"

// Das #pragma comment(lib, "d3dcompiler") in der Fremddatei gilt nur fuer
// MSVC. Dort schadet es nicht, weil /DELAYLOAD den Eintrag verzoegert; unter
// MinGW gibt es das Pragma ohnehin nicht.
#include "backends/imgui_impl_dx11.cpp"
