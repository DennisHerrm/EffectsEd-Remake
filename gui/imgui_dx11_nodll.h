// D3DCompile für ImGuis DX11-Rücken zur Laufzeit auflösen.
//
// ImGuis `imgui_impl_dx11.cpp` ruft `D3DCompile` direkt auf und bindet damit
// `d3dcompiler_47.dll` fest ein — mit einem `#pragma comment(lib, ...)`
// mitten in der Datei. Auf Windows 7 gehört diese DLL nicht zum System, und
// das Programm lädt dann gar nicht.
//
// ImGui nennt die beiden Auswege selbst im Quelltext:
//
//   1) Shader einmal übersetzen und die fertigen Blobs mitliefern
//   2) die DLL zur Laufzeit suchen und den Zeiger auf D3DCompile holen
//
// Hier ist es Weg 2, und zwar ohne die Fremddatei zu ändern: `D3DCompile`
// wird auf unsere eigene Funktion umgebogen, bevor die Datei eingebunden
// wird. Fremden Quelltext zu patchen wäre die schlechtere Wahl — beim
// nächsten ImGui-Wechsel wäre der Patch weg, und niemand merkt es.
//
// Für MSVC gäbe es zusätzlich `/DELAYLOAD`; für MinGW gibt es das nicht.
// Dieser Weg funktioniert bei beiden.
#pragma once

// d3dcompiler.h ZUERST, damit die echte Deklaration von D3DCompile schon
// gelesen ist, bevor das Makro sie umbiegt. Andernfalls trifft das Makro die
// Deklaration selbst — und die steht in einem extern "C"-Block, was mit
// unserer C++-Funktion nicht zusammenpasst.
#include <windows.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>

#include <initializer_list>

namespace efx::gui {

// Dieselbe Signatur wie das Original.
inline HRESULT WINAPI compileShaderRuntime(LPCVOID data, SIZE_T size,
                                           LPCSTR sourceName,
                                           const D3D_SHADER_MACRO* defines,
                                           ID3DInclude* include, LPCSTR entry,
                                           LPCSTR target, UINT flags1, UINT flags2,
                                           ID3DBlob** code, ID3DBlob** errors) {
    using CompileFn = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR,
                                       const D3D_SHADER_MACRO*, ID3DInclude*,
                                       LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**,
                                       ID3DBlob**);
    // Einmal suchen, dann behalten. Die Fassungsnummer wandert mit dem
    // DirectX-SDK; 47 ist die aktuelle, die älteren stehen dahinter, damit
    // ein System mit altem SDK trotzdem läuft.
    static CompileFn compile = [] {
        for (const wchar_t* name : {L"d3dcompiler_47.dll", L"d3dcompiler_46.dll",
                                    L"d3dcompiler_43.dll"}) {
            if (const HMODULE lib = LoadLibraryW(name)) {
                if (auto fn = reinterpret_cast<CompileFn>(
                        reinterpret_cast<void*>(GetProcAddress(lib, "D3DCompile")))) {
                    return fn;
                }
            }
        }
        return static_cast<CompileFn>(nullptr);
    }();

    if (!compile) return E_NOINTERFACE;
    return compile(data, size, sourceName, defines, include, entry, target,
                   flags1, flags2, code, errors);
}

}  // namespace efx::gui

// Ab hier heisst D3DCompile unsere Funktion. Muss VOR imgui_impl_dx11.cpp
// stehen.
#define D3DCompile ::efx::gui::compileShaderRuntime
