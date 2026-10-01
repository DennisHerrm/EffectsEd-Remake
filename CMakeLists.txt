cmake_minimum_required(VERSION 3.20...4.0)
project(efxed LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Statisch gelinkte Laufzeit: die Exe laeuft ohne nachinstalliertes
    # Redistributable.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /permissive- /utf-8 /MP)
    # Symbole auch im Release-Bau, in eine eigene .pdb. Die .exe bleibt gleich
    # gross und gleich schnell; liegt die .pdb daneben, nennt das
    # Absturzprotokoll Funktion, Datei und Zeile statt einer nackten Adresse.
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_link_options($<$<CONFIG:Release>:/DEBUG> $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
    # Zeigerverkuerzung ist unter x64 nie harmlos, MSVC meldet sie aber nur
    # als Warnung — und eine Warnung unter zwoelf anderen uebersieht man.
    #   C4311/C4302  Zeiger -> zu schmale Zahl
    #   C4312        zu schmale Zahl -> Zeiger
    #   C4826        vorzeichenbehaftete Erweiterung
    add_compile_options(/we4311 /we4302 /we4312 /we4826)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    # -Wshadow entspricht MSVCs C4456. Ohne diese Option faellt eine verdeckte
    # Variable erst beim Bauen auf dem Zielrechner auf.
    add_compile_options(-Wall -Wextra -Wshadow)
endif()

# ---------------------------------------------------------------------------
# Kern — ohne Oberflaechen- oder Betriebssystemabhaengigkeiten
# Ressourcen uebersetzen (nur Windows). Ohne enable_language(RC) reicht CMake
# die .rc-Datei kommentarlos an den C++-Uebersetzer weiter, und der Bau bricht
# mit einer Meldung ab, die von Ressourcen kein Wort sagt.
if(WIN32)
    enable_language(RC)
endif()

add_library(efxcore STATIC
    src/gp2.cpp
    src/help_document.cpp
    src/effect.cpp
    src/read.cpp
    src/write.cpp
    src/validate.cpp
    src/shader.cpp
    src/theme.cpp
    src/layout.cpp
    src/i18n.cpp
    src/renderer.cpp
    src/paths.cpp
    src/diag.cpp
    src/jobs.cpp
    src/gamepath.cpp
    src/camera.cpp
    src/scene.cpp
    src/playback.cpp
    src/fields.cpp
    src/assets.cpp
    src/curve.cpp
    src/sim.cpp
    src/undo.cpp
    src/timeline.cpp
    src/tiles.cpp
    src/sound.cpp
    src/particles.cpp
    src/inflate.cpp
    src/image.cpp
    src/image_jpeg.cpp
    src/image_png.cpp
)
target_include_directories(efxcore PUBLIC include)
# third_party enthaelt minimp3.h — die einzige fremde Datei im Baum.
# SYSTEM: fremde Koepfe sollen keine Warnungen ueber unseren Code streuen.
# minimp3 ist gemeinfreier Fremdcode — dessen Stil ist nicht unsere Sache,
# und seine Meldungen wuerden unsere ueberdecken.
target_include_directories(efxcore SYSTEM PRIVATE third_party)

find_package(Threads REQUIRED)
target_link_libraries(efxcore PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# Kommandozeile und Tests — brauchen keine Oberflaeche
add_executable(efxtool tools/efxtool.cpp)
target_link_libraries(efxtool PRIVATE efxcore)

enable_testing()
add_executable(efxtests tests/tests.cpp)
target_link_libraries(efxtests PRIVATE efxcore)
add_test(NAME efxtests COMMAND efxtests "${CMAKE_CURRENT_SOURCE_DIR}/data")

# Getrennt, weil er ohne ImGui auskommt und deshalb auch dort laeuft, wo die
# Oberflaeche gar nicht gebaut wird.
add_executable(efx_imgui_compat tests/imgui_compat.cpp)
target_link_libraries(efx_imgui_compat PRIVATE efxcore)
add_test(NAME efx_imgui_compat COMMAND efx_imgui_compat)

# ---------------------------------------------------------------------------
# Oberflaeche — nur unter Windows
#
# Dear ImGui wird beim ersten Bauen heruntergeladen. Bewusst auf eine feste
# Fassung festgenagelt: ein Werkzeug, das je nach Tag der Woche anders gebaut
# wird, ist nicht wartbar.
if(WIN32)
    include(FetchContent)
    set(FETCHCONTENT_QUIET FALSE)

    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.5
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        # imgui_impl_dx11.cpp wird ueber gui/imgui_impl_dx11_wrapper.cpp
        # eingebunden — nicht hier, sonst gaebe es die Funktionen doppelt.
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/gui
    )
    # Testhaken fuer den Selbsttest (gui/imgui_config.h, gui/selbsttest.cpp).
    target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG="imgui_config.h")
    if(MSVC)
        # ImGui ist nicht unser Code — dessen Warnungen sollen unsere nicht
        # zudecken.
        target_compile_options(imgui PRIVATE /W0)
    endif()

    add_executable(efxed WIN32
        gui/main_win32.cpp
        gui/app.cpp
    gui/app_widgets.cpp
    gui/app_panels.cpp
    gui/app_playback.cpp
    gui/app_resources.cpp
    gui/app_dialogs.cpp
    gui/app_properties.cpp
    gui/app_browser.cpp
        gui/renderer_d3d11.cpp
        gui/registry_win32.cpp
        # Das Symbol. Windows liest es aus der .exe, bevor eine Zeile laeuft —
        # dafuer muss es in der Ressourcentabelle stehen, nicht im Quelltext.
        gui/efxed.rc
        # Statt backends/imgui_impl_dx11.cpp: die Huelle bindet die Fremddatei
        # ein und biegt D3DCompile auf eine Laufzeitfassung um.
        gui/imgui_impl_dx11_wrapper.cpp
        gui/renderer_gl3.cpp
    gui/audio_win32.cpp
    gui/icons.cpp
    gui/selbsttest.cpp
    gui/app_commands.cpp
    )
    target_link_libraries(efxed PRIVATE
        efxcore imgui
        # d3d11 und d3dcompiler werden zur Laufzeit geladen (siehe
        # renderer_d3d11.cpp) — fest gebunden wuerde das Programm auf einem
        # System ohne diese DLLs gar nicht erst starten.
        # dxgi bleibt fest gebunden: die DLL gibt es seit Vista, und wir
        # benutzen ohnehin nur COM-Schnittstellen daraus, die das Geraet
        # liefert — keine eigene Importfunktion.
        d3d11 d3dcompiler dxgi opengl32
     comdlg32 winmm
     # shell32/ole32 fuer die Ordnerauswahl (SHBrowseForFolder, CoTaskMemFree)
     shell32 ole32 uuid)
    target_include_directories(efxed PRIVATE gui)

# Die aelteste Windows-Fassung, auf die gezielt wird.
#
# 0x0601 ist Windows 7. Der Nutzen liegt nicht darin, etwas zu erlauben,
# sondern darin, dass der Uebersetzer neuere Aufrufe MELDET, statt sie
# stillschweigend anzunehmen — man findet sie also beim Bauen und nicht auf
# dem Rechner eines Benutzers.
#
# Achtung: Visual Studio 2026 kann trotzdem nicht auf Windows 7 zielen, das
# ist eine Entscheidung von Microsoft. Fuer eine lauffaehige Windows-7-Fassung
# braucht es MinGW-w64 oder ein aelteres Toolset — siehe WINDOWS7.md.
target_compile_definitions(efxed PRIVATE
    _WIN32_WINNT=0x0601
    WINVER=0x0601
    NTDDI_VERSION=0x06010000)

# Hier stand einmal /DELAYLOAD fuer d3d11, d3dcompiler_47 und dxgi.
#
# Es ist entfernt, und der Linker selbst hat den Grund geliefert:
#
#   LNK4199: /DELAYLOAD:d3d11.dll wird ignoriert; keine Importe gefunden.
#   LNK4199: /DELAYLOAD:d3dcompiler_47.dll wird ignoriert; keine Importe ...
#   LNK4199: /DELAYLOAD:dxgi.dll wird ignoriert; keine Importe gefunden.
#
# Es GIBT keine Importe mehr. `renderer_d3d11.cpp` holt inzwischen alles ueber
# LoadLibrary und GetProcAddress, und `gui/imgui_dx11_nodll.h` biegt ImGuis
# eingebautes `#pragma comment(lib, "d3dcompiler")` um. Damit steht nichts
# mehr in der Importtabelle, was verzoegert geladen werden koennte.
#
# /DELAYLOAD war also der Guertel zu einem Hosentraeger, der schon haelt — und
# ein Guertel, der drei Warnungen bei jedem Bau erzeugt, ist schlechter als
# keiner: man gewoehnt sich an Warnungen im Bauprotokoll.
#
# Die Absicherung bleibt trotzdem bestehen, nur an der richtigen Stelle:
# `tools/check_win7_exe.py` liest die Importtabelle der FERTIGEN .exe und
# meldet, wenn dort doch etwas auftaucht. Das prueft das Ergebnis statt der
# Absicht — und es haette diesen Zustand auch entdeckt, wenn der Linker
# geschwiegen haette.
endif()
