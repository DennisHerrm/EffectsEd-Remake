// Einstiegspunkt.
//
// Diese Datei ist die einzige, die Windows kennt — und damit die einzige, die
// sich hier nicht prüfen liess. Bei g2c war genau das die Datei mit den
// Fehlern. Deshalb ist sie so klein wie möglich gehalten und ruft für jede
// Entscheidung Code auf, der geprüft ist.
//
// Jeder Schritt trägt eine Protokollmarke. Stürzt es beim Start auf einem
// fremden Rechner ab, benennt die letzte Zeile in
// %APPDATA%\efxed\efxed_start.log den Schuldigen.
#include <windows.h>

// ShellExecuteW steht in shellapi.h.
//
// MinGWs windows.h zieht den Kopf mit herein, MSVC nicht — dort blieb es bei
// "C3861: ShellExecuteW: Bezeichner wurde nicht gefunden". Genau die Sorte
// Unterschied, die man nur beim Bauen auf dem anderen Uebersetzer merkt.
#include <shellapi.h>
#include <commdlg.h>
#include <shlobj.h>
// Nur die Typen; die Funktionen holt logCrashStack zur Laufzeit.
#include <dbghelp.h>

#include "resource.h"

#include "efx/version.h"

#include <cstring>

#include <string>
#include <thread>
#include <cstdlib>
#include <vector>
#include <cstdio>

#include "app.h"
#include "selbsttest.h"
#include "efx/diag.h"
#include "efx/i18n.h"
#include "efx/jobs.h"
#include "efx/layout.h"
#include "efx/paths.h"
#include "efx/renderer.h"
#include "efx/theme.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "theme_imgui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM,
                                                             LPARAM);

namespace {

bool g_quit = false;
bool g_closeRequested = false;
int g_width = 1280;
int g_height = 860;
bool g_resized = false;

LRESULT WINAPI wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return 1;

    switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                g_width = LOWORD(lParam);
                g_height = HIWORD(lParam);
                g_resized = true;
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;  // Alt-Menue aus
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_CLOSE:
            // Nicht gleich schliessen: die Oberflaeche fragt erst, ob
            // ungespeicherte Aenderungen gesichert werden sollen.
            g_closeRequested = true;
            return 0;
        case WM_DESTROY:
            g_quit = true;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

std::string toUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr, 0,
                                         nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring toWide(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), size);
    return out;
}

std::string exeDirectory() {
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring path(buffer);
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::string(".")
                                       : toUtf8(path.substr(0, slash));
}

std::string systemLocale() {
    wchar_t buffer[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH) > 0) {
        return toUtf8(buffer);
    }
    return "en";
}

std::string windowsVersion() {
    // GetVersionEx luegt seit Windows 8.1 ohne Manifest. Die Bauversion steht
    // aber in der Registrierung und stimmt dort.
    HKEY key{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                      KEY_READ, &key) != ERROR_SUCCESS) {
        return "unknown";
    }
    wchar_t product[128]{};
    wchar_t build[64]{};
    DWORD size = sizeof(product);
    RegQueryValueExW(key, L"ProductName", nullptr, nullptr,
                     reinterpret_cast<LPBYTE>(product), &size);
    size = sizeof(build);
    RegQueryValueExW(key, L"CurrentBuildNumber", nullptr, nullptr,
                     reinterpret_cast<LPBYTE>(build), &size);
    RegCloseKey(key);

    // ProductName bleibt seit Windows 11 auf "Windows 10" stehen — Microsoft
    // hat den Wert nie umgestellt. Wer sich darauf verlaesst, schiebt spaeter
    // eine Fehlermeldung auf die falsche Fassung. Die Buildnummer stimmt
    // dagegen: ab 22000 ist es Windows 11.
    std::string name = toUtf8(product);
    const int buildNumber = std::atoi(toUtf8(build).c_str());
    if (buildNumber >= 22000) {
        const size_t ten = name.find("Windows 10");
        if (ten != std::string::npos) name.replace(ten, 10, "Windows 11");
    }
    return name + " Build " + toUtf8(build);
}

// Schriften.
//
// Die harte Regel aus g2c: keine Schriftart aus dem Systemverzeichnis wird
// vorausgesetzt. Dort wurde SegoeIcons.ttf geladen, das es nur unter
// Windows 11 gibt — ImGui bricht mit einer Zusicherung ab, ohne Fehlertext,
// und zwar ausschliesslich auf fremden Rechnern.
//
// Deshalb: ImGuis eingebaute Schrift als Grundlage. Systemschriften nur, wenn
// sie nachweislich da sind, und nur als Zugabe.
// Die Auflösung des Fensters — ebenfalls ueber GetProcAddress.
//
// `GetDpiForWindow` gibt es erst seit Windows 10. Direkt aufgerufen ist es
// derselbe harte Ausfall wie oben: das Programm laedt nicht.
//
// Der Rueckfall ueber `GetDeviceCaps(LOGPIXELSX)` gibt es seit Windows 3.1.
// Er kennt nur die Auflösung des Hauptbildschirms, nicht die je Fenster —
// bei einem Bildschirm ist das dasselbe, bei zweien mit verschiedener
// Auflösung eine Naeherung. Besser als gar nicht starten.
UINT windowDpi(HWND hwnd) {
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using GetDpi = UINT(WINAPI*)(HWND);
        if (auto fn = reinterpret_cast<GetDpi>(
                reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForWindow")))) {
            const UINT dpi = fn(hwnd);
            if (dpi > 0) return dpi;
        }
    }
    const HDC dc = GetDC(nullptr);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);
    return dpi > 0 ? static_cast<UINT>(dpi) : 96u;
}

void announceDpiAwareness() {
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using SetContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        if (auto fn = reinterpret_cast<SetContext>(
                reinterpret_cast<void*>(
                    GetProcAddress(user32, "SetProcessDpiAwarenessContext")))) {
            if (fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
                efx::diag::info("DPI: per-monitor v2");
                return;
            }
        }
    }
    if (HMODULE shcore = LoadLibraryW(L"shcore.dll")) {
        using SetAwareness = HRESULT(WINAPI*)(int);
        if (auto fn = reinterpret_cast<SetAwareness>(
                reinterpret_cast<void*>(
                    GetProcAddress(shcore, "SetProcessDpiAwareness")))) {
            fn(2);   // PROCESS_PER_MONITOR_DPI_AWARE
            efx::diag::info("DPI: per-monitor (shcore)");
            FreeLibrary(shcore);
            return;
        }
        FreeLibrary(shcore);
    }
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using SetAware = BOOL(WINAPI*)();
        if (auto fn = reinterpret_cast<SetAware>(
                reinterpret_cast<void*>(
                    GetProcAddress(user32, "SetProcessDPIAware")))) {
            fn();
            efx::diag::info("DPI: system aware (legacy)");
            return;
        }
    }
    efx::diag::warn("DPI: no awareness API available");
}

bool addFontIfPresent(ImGuiIO& io, const char* path, float size,
                      const ImWchar* ranges, bool merge) {
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        efx::diag::info(std::string("not present, skipped: ") + path);
        return false;
    }
    ImFontConfig config;
    config.MergeMode = merge;
    const ImFont* font = io.Fonts->AddFontFromFileTTF(path, size, &config, ranges);
    if (!font) {
        efx::diag::warn(std::string("could not load: ") + path);
        return false;
    }
    efx::diag::info(std::string("loaded: ") + path);
    return true;
}

void buildFonts(float dpiScale, bool fullCjk) {
    efx::diag::Step step("Build fonts");
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const float size = 16.0f * dpiScale;

    {
        efx::diag::Step base("Latin font");
        // Segoe UI ist seit Windows Vista dabei. Fehlt sie trotzdem, faellt
        // der Aufruf still durch und ImGui benutzt seine eingebaute Schrift.
        // Latin-1 PLUS die Satzzeichen, die wir tatsaechlich benutzen.
        //
        // `GetGlyphRangesDefault` reicht nur bis U+00FF. Der Gedankenstrich
        // U+2014, den fast jede unserer Meldungen enthaelt, liegt darueber und
        // wurde als Fragezeichen gezeichnet:
        //
        //     Particle #2: size has a different end value but no transition
        //     type ? the value stays on the start value ...
        //
        // Deutsche Umlaute fielen nicht auf, weil sie in Latin-1 liegen. Der
        // Fehler zeigte sich erst in der englischen Fassung — und dort sah es
        // aus, als fehle ein Wort.
        static const ImWchar kRanges[] = {
            0x0020, 0x00FF,   // Latein und Latin-1
            0x2010, 0x2027,   // Binde-, Halbgeviert-, Geviertstrich,
                              // Anfuehrungszeichen, Auslassungspunkte
            0x20AC, 0x20AC,   // Eurozeichen
            0,
        };
        if (!addFontIfPresent(io, "C:\\Windows\\Fonts\\segoeui.ttf", size,
                              kRanges, false)) {
            io.Fonts->AddFontDefault();
            efx::diag::info("using built-in font");
        }
    }

    // CJK-Zeichen.
    //
    // Der erste Anlauf lud eine CJK-Schrift nur, wenn beim Start eine
    // CJK-Sprache eingestellt war. Ergebnis: bei deutscher Oberflaeche stand
    // im Sprachmenue "??" und "???" statt 中文 und 日本語 — genau so viele
    // Fragezeichen wie Zeichen. Man konnte die Sprache also nicht auswaehlen,
    // weil man nicht lesen konnte, welche es ist.
    //
    // Deshalb zwei Stufen:
    //   immer     — die Zeichen der Sprachnamen selbst, damit das Menue in
    //               jeder Sprache lesbar ist. Das sind fuenf Glyphen.
    //   bei Bedarf — der volle Bereich, wenn die Oberflaeche selbst auf
    //               Chinesisch oder Japanisch steht.
    {
        efx::diag::Step cjk(fullCjk ? "CJK font (full range)"
                                    : "CJK font (language names only)");

        // Der Bereichsvektor muss laenger leben als dieser Block.
        //
        // ImGui kopiert ihn nicht, sondern merkt sich nur den Zeiger und liest
        // ihn erst in io.Fonts->Build() — also lange nach AddFontFromFileTTF.
        // Stand er wie zuerst in einem eigenen { }-Block, war er beim Bauen
        // laengst zerstoert. Ergebnis: keine einzige CJK-Glyphe, und zwar ohne
        // jede Fehlermeldung. Genau dieser Fall steht in ocornut/imgui#2052
        // und in den "Common pitfalls" von imgui.h.
        //
        // static, nicht Member: der Zeiger muss auch dann noch gelten, wenn
        // der Zeichensatz beim Sprachwechsel neu gebaut wird.
        static ImVector<ImWchar> ranges;
        ranges.clear();
        ImFontGlyphRangesBuilder builder;

        // Die Namen aus der Sprachtabelle, nicht fest verdrahtet: kommt eine
        // Sprache dazu, wird ihr Name automatisch mit aufgenommen.
        for (const auto& info : efx::i18n::languages()) {
            builder.AddText(info.nativeName);
        }
        if (fullCjk) {
            const efx::i18n::Language current = efx::i18n::currentLanguage();
            builder.AddRanges(current == efx::i18n::Language::Japanese
                                  ? io.Fonts->GetGlyphRangesJapanese()
                                  : io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        }
        builder.BuildRanges(&ranges);

        const bool ok =
            addFontIfPresent(io, "C:\\Windows\\Fonts\\msyh.ttc", size, ranges.Data, true) ||
            addFontIfPresent(io, "C:\\Windows\\Fonts\\meiryo.ttc", size, ranges.Data, true) ||
            addFontIfPresent(io, "C:\\Windows\\Fonts\\YuGothM.ttc", size, ranges.Data, true) ||
            addFontIfPresent(io, "C:\\Windows\\Fonts\\msgothic.ttc", size, ranges.Data, true) ||
            addFontIfPresent(io, "C:\\Windows\\Fonts\\simsun.ttc", size, ranges.Data, true);
        if (!ok) {
            cjk.fail("no CJK font found; those characters will show as boxes");
        } else {
            efx::diag::info(std::to_string(ranges.Size / 2) +
                            " glyph ranges requested");
        }
    }

    io.Fonts->Build();

    // Nachsehen, ob die Glyphen tatsaechlich da sind.
    //
    // Der Fehler vorher war stumm: der Zeichensatz wurde gebaut, ImGui meldete
    // nichts, und erst auf dem Bildschirm standen Fragezeichen. Ein Aufbau,
    // der nicht das enthaelt, was er enthalten soll, muss sich melden.
    if (ImFont* font = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0]) {
        int missing = 0;
        std::string missingText;
        for (const auto& info : efx::i18n::languages()) {
            // Die Namen liegen als UTF-8 vor; hier reicht eine grobe
            // Dekodierung fuer die drei Byte langen CJK-Zeichen.
            const char* p = info.nativeName;
            while (*p) {
                unsigned int code = static_cast<unsigned char>(*p);
                int length = 1;
                if (code >= 0xF0) { code &= 0x07; length = 4; }
                else if (code >= 0xE0) { code &= 0x0F; length = 3; }
                else if (code >= 0xC0) { code &= 0x1F; length = 2; }
                for (int i = 1; i < length && p[i]; ++i) {
                    code = (code << 6) | (static_cast<unsigned char>(p[i]) & 0x3F);
                }
                p += length;
                if (code < 0x80) continue;  // Latein interessiert hier nicht
                if (!font->FindGlyphNoFallback(static_cast<ImWchar>(code))) {
                    ++missing;
                    if (missingText.size() < 40) {
                        char buffer[16];
                        std::snprintf(buffer, sizeof(buffer), "U+%04X ", code);
                        missingText += buffer;
                    }
                }
            }
        }
        if (missing > 0) {
            efx::diag::error("font atlas is missing " + std::to_string(missing) +
                             " glyphs used by the language menu: " + missingText +
                             "- those entries will show as boxes");
        } else {
            efx::diag::info("all language-menu glyphs present");
        }
    }
}

}  // namespace

// Die eigentliche Sitzung. Getrennt, weil ein Wechsel der Grafikschnittstelle
// sie neu durchlaufen laesst — ImGui erlaubt keinen Tausch im Betrieb
// (ocornut/imgui#4616). Der Zustand liegt in App und ueberlebt das.
static bool runSession(efx::gui::App& app, efx::render::Backend preferred,
                       efx::render::Backend& actuallyUsed) {
    efx::diag::Step session("Start session");

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    // Das Symbol aus den eigenen Ressourcen.
    //
    // Zwei Groessen, und beide sind noetig: `hIcon` nimmt Alt+Tab und der
    // Fensterwechsler (32 Punkte), `hIconSm` die Titelzeile und die
    // Taskleiste (16). Setzt man nur `hIcon`, skaliert Windows das grosse
    // herunter, und bei einem Symbol mit Schrift sieht man das sofort.
    //
    // LR_DEFAULTSIZE waere hier falsch: es nimmt die Systemgroesse, nicht die
    // gewuenschte. Deshalb die Groessen ausdruecklich.
    const HINSTANCE self = GetModuleHandleW(nullptr);
    wc.hIcon = static_cast<HICON>(LoadImageW(self, MAKEINTRESOURCEW(IDI_APPICON),
                                             IMAGE_ICON, 32, 32, 0));
    wc.hIconSm = static_cast<HICON>(LoadImageW(self, MAKEINTRESOURCEW(IDI_APPICON),
                                               IMAGE_ICON, 16, 16, 0));
    wc.lpszClassName = L"EffectsEdWindow";
    RegisterClassExW(&wc);

    HWND hwnd = nullptr;
    {
        efx::diag::Step step("Create window");
        auto& placement = app.settings().window;
        // Beim allerersten Start maximiert, wie das Original (MainFrame Show
        // = 3 in dessen Registry). Vorher kam ein Fenster von 1280 x 860
        // Bildpunkten — bei 150 Prozent Skalierung so klein, dass die
        // Eigenschaftsseite nicht passte.
        if (!placement.valid) placement.maximized = true;
        int x = placement.valid ? placement.x : CW_USEDEFAULT;
        int y = placement.valid ? placement.y : CW_USEDEFAULT;
        int width = placement.valid ? placement.width : 1280;
        int height = placement.valid ? placement.height : 860;
        // Der Selbsttest laeuft in fester Groesse (EFXED_FENSTER=BxH), damit
        // seine Ergebnisse nicht vom letzten Fenster abhaengen.
        if (const char* size = std::getenv("EFXED_FENSTER");
            efx::gui::selbsttestAktiv() && size && std::sscanf(size, "%dx%d", &width, &height) == 2) {
            x = 0;
            y = 0;
            placement.maximized = false;
        }
        hwnd = CreateWindowW(wc.lpszClassName, L"EffectsEd", WS_OVERLAPPEDWINDOW, x, y, width,
                             height, nullptr, nullptr, wc.hInstance, nullptr);
        if (!hwnd) {
            step.fail("CreateWindowW failed");
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }
    }

    // Der Windows-Dateidialog.
    //
    // GetOpenFileNameW statt der A-Fassung: Pfade mit Umlauten oder
    // chinesischen Zeichen sind sonst kaputt, und genau dort liegen bei
    // vielen Leuten die Mods.
    //
    // OFN_NOCHANGEDIR ist wichtig: ohne das setzt der Dialog das
    // Arbeitsverzeichnis des ganzen Programms um, und relative Pfade zeigen
    // danach woanders hin.
    // Ordnerauswahl.
    //
    // Hier stand SHBrowseForFolder, mit der Begruendung: IFileDialog braucht
    // COM und gibt es erst ab Vista. Die zweite Haelfte stimmt, die
    // Schlussfolgerung nicht — das Ziel ist Windows 7, nicht XP.
    //
    // Microsofts eigene Doku zu SHBrowseForFolder sagt es deutlich: fuer Vista
    // und neuer ist IFileDialog mit FOS_PICKFOLDERS die empfohlene Umsetzung.
    // Der alte Dialog ist der XP-Baum ohne Schnellzugriff, ohne Eingabezeile
    // fuer den Pfad, ohne zuletzt benutzte Ordner und nicht groessenveraenderbar
    // — bei jemandem, der zwischen Steam-, GOG- und Mod-Ordnern springt, ist
    // das jedes Mal derselbe Klickweg von vorn.
    //
    // COM wird zur Laufzeit geholt, nicht fest gebunden: schlaegt
    // CoCreateInstance fehl, faellt es auf SHBrowseForFolder zurueck. Ein
    // Rueckfall hinter einem harten Ausfall waere keiner — deshalb ist der
    // alte Weg NICHT geloescht, sondern der zweite Versuch.
    app.setFolderDialog([&](const char* title, const char* startAt) -> std::string {
        std::wstring wideTitle;
        if (title && title[0]) {
            wchar_t buffer[256] = {};
            MultiByteToWideChar(CP_UTF8, 0, title, -1, buffer, 255);
            wideTitle = buffer;
        }
        std::wstring wideStart;
        if (startAt && startAt[0]) {
            wchar_t buffer[1024] = {};
            MultiByteToWideChar(CP_UTF8, 0, startAt, -1, buffer, 1023);
            wideStart = buffer;
        }

        const auto toUtf8 = [](const wchar_t* wide) {
            char utf8[1024] = {};
            WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8) - 1,
                                nullptr, nullptr);
            return std::string(utf8);
        };

        // Erster Versuch: der heutige Dialog.
        {
            IFileOpenDialog* dialog = nullptr;
            const HRESULT created = CoCreateInstance(
                CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&dialog));
            if (SUCCEEDED(created) && dialog) {
                DWORD options = 0;
                if (SUCCEEDED(dialog->GetOptions(&options))) {
                    // FOS_PICKFOLDERS macht aus dem Oeffnen-Dialog eine
                    // Ordnerauswahl. FOS_FORCEFILESYSTEM dazu, weil wir einen
                    // Pfad brauchen und keine Bibliothek.
                    dialog->SetOptions(options | FOS_PICKFOLDERS |
                                       FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
                }
                if (!wideTitle.empty()) dialog->SetTitle(wideTitle.c_str());

                // Der Startordner. Schlaegt das fehl, oeffnet der Dialog
                // dort, wo er zuletzt war — auch brauchbar, deshalb ohne
                // Abbruch.
                if (!wideStart.empty()) {
                    IShellItem* startItem = nullptr;
                    if (SUCCEEDED(SHCreateItemFromParsingName(
                            wideStart.c_str(), nullptr, IID_PPV_ARGS(&startItem))) &&
                        startItem) {
                        dialog->SetFolder(startItem);
                        startItem->Release();
                    }
                }

                std::string result;
                if (SUCCEEDED(dialog->Show(hwnd))) {
                    IShellItem* chosen = nullptr;
                    if (SUCCEEDED(dialog->GetResult(&chosen)) && chosen) {
                        PWSTR path = nullptr;
                        if (SUCCEEDED(chosen->GetDisplayName(SIGDN_FILESYSPATH,
                                                             &path)) && path) {
                            result = toUtf8(path);
                            CoTaskMemFree(path);
                        }
                        chosen->Release();
                    }
                }
                dialog->Release();
                // Auch ein leeres Ergebnis ist eine Antwort: der Nutzer hat
                // abgebrochen. Nur wenn der Dialog gar nicht kam, wird der
                // alte Weg versucht.
                return result;
            }
        }

        // Rueckfall: der alte Baum. Besser als kein Dialog.
        static std::wstring startFolder;
        startFolder = wideStart;

        BROWSEINFOW info{};
        info.hwndOwner = hwnd;
        info.lpszTitle = wideTitle.empty() ? nullptr : wideTitle.c_str();
        info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
        info.lpfn = [](HWND dialog, UINT message, LPARAM, LPARAM) -> int {
            if (message == BFFM_INITIALIZED && !startFolder.empty()) {
                SendMessageW(dialog, BFFM_SETSELECTIONW, TRUE,
                             reinterpret_cast<LPARAM>(startFolder.c_str()));
            }
            return 0;
        };

        LPITEMIDLIST item = SHBrowseForFolderW(&info);
        if (!item) return {};
        wchar_t chosen[MAX_PATH] = {};
        const bool ok = SHGetPathFromIDListW(item, chosen) != FALSE;
        CoTaskMemFree(item);
        return ok ? toUtf8(chosen) : std::string{};
    });

    // Eine Datei mit dem Programm oeffnen, das Windows dafuer vorgesehen hat.
    //
    // `ShellExecuteW` mit "open" und ohne Verb-Angabe: das nimmt fuer .html
    // den eingestellten Browser. Gibt es keinen, passiert nichts — kein
    // Grund, deswegen einen Fehler zu melden.
    app.setShellOpen([](const std::string& path) {
        std::wstring wide(path.size() + 1, L'\0');
        const int written = MultiByteToWideChar(
            CP_UTF8, 0, path.c_str(), -1, wide.data(),
            static_cast<int>(wide.size()));
        if (written <= 0) return;
        wide.resize(static_cast<size_t>(written - 1));
        ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr,
                      SW_SHOWNORMAL);
    });

    app.setFileDialog([&](bool save, const char* filter,
                          const char* defaultName) -> std::string {
        wchar_t buffer[1024] = {};
        if (defaultName && defaultName[0]) {
            MultiByteToWideChar(CP_UTF8, 0, defaultName, -1, buffer, 1023);
        }

        // Der Filter kommt als UTF-8 mit eingebetteten Nullbytes; die Laenge
        // steht deshalb nicht in einem strlen. Er endet mit zwei Nullbytes.
        std::wstring wideFilter;
        {
            const char* p = filter;
            while (true) {
                const size_t length = std::strlen(p);
                wchar_t part[256] = {};
                MultiByteToWideChar(CP_UTF8, 0, p, -1, part, 255);
                wideFilter.append(part);
                wideFilter.push_back(L'\0');
                p += length + 1;
                if (*p == '\0') break;
            }
            wideFilter.push_back(L'\0');
        }

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = wideFilter.c_str();
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = 1023;
        ofn.lpstrDefExt = L"efx";
        ofn.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
        if (save) {
            ofn.Flags |= OFN_OVERWRITEPROMPT;
        } else {
            ofn.Flags |= OFN_FILEMUSTEXIST;
        }

        const BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
        if (!ok) return {};

        char utf8[1024] = {};
        WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8, sizeof(utf8) - 1,
                            nullptr, nullptr);
        return std::string(utf8);
    });

    // Grafikschnittstelle: erst beide durchprobieren, dann entscheiden. Die
    // Entscheidung selbst steckt in src/renderer.cpp und ist geprueft.
    std::unique_ptr<efx::render::Renderer> renderer;
    {
        efx::diag::Step step("Create graphics backend");
        std::vector<efx::render::Probe> probes;
        for (auto backend : efx::render::fallbackOrder(preferred)) {
            efx::diag::Step attempt(std::string("probe: ") +
                                    efx::render::backendName(backend));
            efx::render::Probe probe;
            auto candidate = efx::render::createRenderer(backend, hwnd, probe);
            probes.push_back(probe);
            if (candidate && probe.available) {
                efx::diag::info(probe.adapter + ", " + probe.version);
                if (!renderer) {
                    renderer = std::move(candidate);
                    actuallyUsed = backend;
                }
            } else {
                attempt.fail(probe.failure);
            }
            if (renderer) break;
        }

        efx::render::Backend chosen{};
        std::string note;
        if (!efx::render::chooseBackend(preferred, probes, chosen, note)) {
            step.fail(note);
            MessageBoxW(nullptr, toWide(note).c_str(), L"EffectsEd",
                        MB_OK | MB_ICONERROR);
            DestroyWindow(hwnd);
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }
        if (!note.empty()) efx::diag::warn(note);

        // Der Oberflaeche mitteilen, was die Erkundung ergeben hat.
        //
        // Das fehlte: `probes` war eine oertliche Variable, die am Ende dieses
        // Blocks verfiel. Der Menuepunkt "Grafiktreiber-Information" oeffnete
        // daraufhin ein Fenster mit einer leeren Tabelle — und weil es sich
        // nach dem Inhalt richtet, war es fingernagelgross. Es sah kaputt aus
        // und war nur leer.
        app.setGraphicsInfo(std::move(probes), actuallyUsed);
    }

    efx::i18n::Language lastLanguage = efx::i18n::currentLanguage();
    float dpiScale = 1.0f;
    {
        efx::diag::Step step("Display scaling");
        const UINT dpi = windowDpi(hwnd);
        dpiScale = static_cast<float>(dpi) / 96.0f;
        efx::diag::info(std::to_string(dpi) + " dpi, Faktor " +
                        std::to_string(dpiScale));
    }

    {
        efx::diag::Step step("Set up ImGui");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        // Keine Tastaturnavigation: mit ihr loest die Leertaste das zuletzt
        // angeklickte Element aus (ein Haekchen schaltete um, statt dass
        // der Effekt abspielte). Im Original spielt die Leertaste ab, egal
        // wo der Fokus steht. Tab zwischen Eingabefeldern geht weiterhin.

        // Der Fensterzustand gehoert neben die Einstellungen, nicht ins
        // Arbeitsverzeichnis.
        static std::string iniPath;
        iniPath = efx::paths::imguiIniPath();
        io.IniFilename = iniPath.c_str();

        const auto* language = efx::i18n::findLanguage(app.settings().languageCode);
        buildFonts(dpiScale, language && language->needsCjkFont);
        lastLanguage = efx::i18n::currentLanguage();

        const efx::theme::Theme* selected =
            efx::theme::findTheme(app.settings().themeId);
        efx::gui::applyTheme(selected ? *selected
                                      : efx::theme::builtinThemes().front());
        ImGui::GetStyle().ScaleAllSizes(dpiScale);
    }

    {
        efx::diag::Step step("Start UI backends");
        {
            efx::diag::Step win32("Win32");
            if (!ImGui_ImplWin32_Init(hwnd)) {
                win32.fail("ImGui_ImplWin32_Init failed");
                step.fail("window input not attached");
                ImGui::DestroyContext();
                renderer.reset();
                DestroyWindow(hwnd);
                UnregisterClassW(wc.lpszClassName, wc.hInstance);
                return false;
            }
        }
        {
            // Erst hier, nicht beim Anlegen des Renderers: die Anbindung
            // braucht den ImGui-Kontext, und den gibt es erst seit dem
            // vorigen Schritt.
            efx::diag::Step gfx(efx::render::backendName(actuallyUsed));
            if (!renderer->initImGuiBackend()) {
                gfx.fail("backend init failed");
                step.fail("drawing not attached");
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                renderer.reset();
                DestroyWindow(hwnd);
                UnregisterClassW(wc.lpszClassName, wc.hInstance);
                return false;
            }
        }
    }

    // Erste Groesse setzen, bevor gezeichnet wird.
    {
        RECT client{};
        GetClientRect(hwnd, &client);
        g_width = client.right - client.left;
        g_height = client.bottom - client.top;
        renderer->resizeSwapChain(g_width, g_height);
    }

    // Im Selbsttest ohne Aktivieren: der Anwender arbeitet nebenher weiter,
    // und ein Fenster, das sich nach vorn draengt, nimmt ihm den Fokus.
    if (efx::gui::selbsttestAktiv()) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    } else {
        ShowWindow(hwnd, app.settings().window.maximized ? SW_SHOWMAXIMIZED : SW_SHOWDEFAULT);
    }
    UpdateWindow(hwnd);
    efx::diag::info("Startup complete, showing window.");

    bool restart = false;
    while (!g_quit && !app.wantsQuit()) {
        // Sprachwechsel: der Zeichensatz muss neu gebaut werden, sonst fehlen
        // die Glyphen der neuen Sprache. Das geht nur zwischen zwei Bildern,
        // nie mitten in einem.
        if (efx::i18n::currentLanguage() != lastLanguage) {
            lastLanguage = efx::i18n::currentLanguage();
            const auto* info =
                efx::i18n::findLanguage(app.settings().languageCode);
            efx::diag::Step rebuild("Rebuild font atlas after language change");
            renderer->shutdownImGuiBackend();
            buildFonts(dpiScale, info && info->needsCjkFont);
            renderer->initImGuiBackend();
        }

        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (g_quit) break;
        if (g_closeRequested) {
            g_closeRequested = false;
            app.requestQuit();
        }

        if (g_resized) {
            renderer->resizeSwapChain(g_width, g_height);
            g_resized = false;
        }

        renderer->newFrame();
        ImGui_ImplWin32_NewFrame();
        efx::gui::selbsttestVorBild();
        ImGui::NewFrame();

        app.buildFrame(renderer.get(), g_width, g_height, dpiScale);
        efx::gui::selbsttestNachBild(app, renderer.get());

        ImGui::Render();
        const auto& palette =
            (efx::theme::findTheme(app.settings().themeId)
                 ? *efx::theme::findTheme(app.settings().themeId)
                 : efx::theme::builtinThemes().front()).palette;
        renderer->clear(palette.windowBg.r, palette.windowBg.g, palette.windowBg.b,
                        1.0f);
        renderer->renderImGui();
        // Im Selbsttest ohne Warten auf den Bildschirm: der Test braucht
        // viele Bilder, nicht schoene.
        renderer->present(!efx::gui::selbsttestAktiv());

        efx::render::Backend target{};
        if (app.wantsRendererChange(target)) {
            app.clearRendererChange();
            preferred = target;
            restart = true;
            break;
        }
    }

    // Fensterlage merken, bevor alles verschwindet.
    {
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        if (GetWindowPlacement(hwnd, &wp)) {
            auto& placement = app.settings().window;
            placement.x = wp.rcNormalPosition.left;
            placement.y = wp.rcNormalPosition.top;
            placement.width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
            placement.height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
            placement.maximized = wp.showCmd == SW_SHOWMAXIMIZED;
            placement.valid = true;
        }
    }

    // Reihenfolge ist wichtig: die Anbindungen greifen auf den Kontext zu,
    // also muessen sie vor dessen Zerstoerung enden. Vorher stand
    // renderer.reset() nach DestroyContext — der Renderer haette dort in
    // seinem Erzeuger auf einen bereits freigegebenen Kontext zugegriffen.
    renderer->shutdownImGuiBackend();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    renderer.reset();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    g_quit = false;
    return restart;
}

// Skalierung anmelden — ueber GetProcAddress, nicht direkt.
//
// Das ist kein Uebereifer. `SetProcessDpiAwarenessContext` wird beim Laden des
// Programms aus user32.dll aufgeloest. Fehlt das Symbol, startet das Programm
// **gar nicht** — kein Fenster, keine Fehlermeldung, nichts im Protokoll.
// Genau die Sorte Fehler, die man auf einem fremden Rechner nicht diagnostiziert.
//
// Drei Stufen, von neu nach alt:
//
//   Windows 10 1703   SetProcessDpiAwarenessContext  (je Bildschirm, Fassung 2)
//   Windows 8.1       SetProcessDpiAwareness         (shcore.dll)
//   Windows Vista     SetProcessDPIAware             (nur an/aus)
//
// Auf allem davor passiert nichts, und die Oberflaeche ist auf einem
// hochaufloesenden Bildschirm klein — unschoen, aber lauffaehig.
// Wer einen Absturz aufschreibt.
//
// Anlass: das Programm ist beim Umschalten von "Alle animieren" verschwunden,
// und das Protokoll endete mitten in gewoehnlichen Texturzeilen. Es ging
// nichts verloren — jede Zeile wird einzeln hinausgeschrieben — es hat nur
// NIEMAND etwas aufgeschrieben. Ein Absturz ohne Spur ist nicht zu
// untersuchen; man kann nur raten, und Raten hilft niemandem.
//
// Der Filter schreibt Fehlercode und Adresse und laesst den Absturz dann
// weiterlaufen. Er REPARIERT nichts und soll es auch nicht: ein Programm, das
// nach einem Speicherfehler weitermacht, richtet mehr Schaden an, als es
// verhindert.
//
// `SetUnhandledExceptionFilter` gibt es seit Windows XP.
// Die Aufrufkette zum Absturz, Bild fuer Bild.
//
// Eine nackte Adresse wie 0x00007FF61A0D8597 sagt nach dem naechsten Start
// nichts mehr — Windows laedt das Programm jedesmal woanders hin. Deshalb
// steht je Bild der Abstand zum Modulanfang dabei (der bleibt gleich), und
// wenn die .pdb neben der .exe liegt, auch Funktion, Datei und Zeile.
//
// dbghelp.dll wird zur Laufzeit geholt: sie gibt es auf jedem Windows, aber
// fest gebunden stuende sie in der Importtabelle, die check_win7_exe.py
// absichtlich klein haelt.
static void logCrashStack(EXCEPTION_POINTERS* info) {
    HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
    if (!dbghelp) return;
    using SymInitializeFn = BOOL(WINAPI*)(HANDLE, PCSTR, BOOL);
    using SymSetOptionsFn = DWORD(WINAPI*)(DWORD);
    using StackWalk64Fn = BOOL(WINAPI*)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID,
                                        PREAD_PROCESS_MEMORY_ROUTINE64,
                                        PFUNCTION_TABLE_ACCESS_ROUTINE64,
                                        PGET_MODULE_BASE_ROUTINE64,
                                        PTRANSLATE_ADDRESS_ROUTINE64);
    using SymFromAddrFn = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
    using SymGetLineFn = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
    auto symInit = reinterpret_cast<SymInitializeFn>(
        reinterpret_cast<void*>(GetProcAddress(dbghelp, "SymInitialize")));
    auto symOptions = reinterpret_cast<SymSetOptionsFn>(
        reinterpret_cast<void*>(GetProcAddress(dbghelp, "SymSetOptions")));
    auto walk = reinterpret_cast<StackWalk64Fn>(
        reinterpret_cast<void*>(GetProcAddress(dbghelp, "StackWalk64")));
    auto fromAddr = reinterpret_cast<SymFromAddrFn>(
        reinterpret_cast<void*>(GetProcAddress(dbghelp, "SymFromAddr")));
    auto lineFromAddr = reinterpret_cast<SymGetLineFn>(
        reinterpret_cast<void*>(GetProcAddress(dbghelp, "SymGetLineFromAddr64")));
    auto tableAccess = reinterpret_cast<PFUNCTION_TABLE_ACCESS_ROUTINE64>(
        reinterpret_cast<void*>(GetProcAddress(dbghelp, "SymFunctionTableAccess64")));
    auto moduleBase = reinterpret_cast<PGET_MODULE_BASE_ROUTINE64>(
        reinterpret_cast<void*>(GetProcAddress(dbghelp, "SymGetModuleBase64")));
    if (!symInit || !walk || !tableAccess || !moduleBase) return;

    const HANDLE process = GetCurrentProcess();
    const HANDLE thread = GetCurrentThread();
    if (symOptions) symOptions(0x00000002 /*UNDNAME*/ | 0x00000010 /*LOAD_LINES*/);
    symInit(process, nullptr, TRUE);

    CONTEXT context = *info->ContextRecord;
    STACKFRAME64 frame{};
#if defined(_M_X64) || defined(__x86_64__)
    const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
#else
    const DWORD machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
#endif
    frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + 256];
    for (int depth = 0; depth < 24; ++depth) {
        if (!walk(machine, process, thread, &frame, &context, nullptr, tableAccess,
                  moduleBase, nullptr)) {
            break;
        }
        const DWORD64 pc = frame.AddrPC.Offset;
        if (pc == 0) break;
        const DWORD64 base = moduleBase(process, pc);
        char moduleName[MAX_PATH] = "?";
        if (base) {
            GetModuleFileNameA(reinterpret_cast<HMODULE>(base), moduleName, MAX_PATH);
        }
        const char* shortName = std::strrchr(moduleName, '\\');
        shortName = shortName ? shortName + 1 : moduleName;

        std::string where = "?";
        if (fromAddr) {
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
            std::memset(symbolBuffer, 0, sizeof(symbolBuffer));
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 255;
            DWORD64 displacement = 0;
            if (fromAddr(process, pc, &displacement, symbol)) where = symbol->Name;
        }
        if (lineFromAddr) {
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisplacement = 0;
            if (lineFromAddr(process, pc, &lineDisplacement, &line) && line.FileName) {
                const char* file = std::strrchr(line.FileName, '\\');
                where += std::string("  ") + (file ? file + 1 : line.FileName) + ":" +
                         std::to_string(line.LineNumber);
            }
        }
        char text[512];
        std::snprintf(text, sizeof(text), "  #%-2d %s+0x%llX  %s", depth, shortName,
                      static_cast<unsigned long long>(base ? pc - base : pc),
                      where.c_str());
        efx::diag::warn(text);
    }
}

static LONG WINAPI logCrash(EXCEPTION_POINTERS* info) {
    if (info && info->ExceptionRecord) {
        char text[256];
        std::snprintf(text, sizeof(text),
                      "CRASH code 0x%08lX at 0x%p",
                      static_cast<unsigned long>(
                          info->ExceptionRecord->ExceptionCode),
                      info->ExceptionRecord->ExceptionAddress);
        efx::diag::warn(text);

        // Bei einem Speicherfehler steht in den Angaben, ob gelesen oder
        // geschrieben wurde und an welcher Adresse. Genau das braucht man.
        if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
            info->ExceptionRecord->NumberParameters >= 2) {
            const ULONG_PTR kind = info->ExceptionRecord->ExceptionInformation[0];
            std::snprintf(text, sizeof(text), "  %s address 0x%p",
                          kind == 0 ? "reading" : (kind == 1 ? "writing" : "executing"),
                          reinterpret_cast<void*>(
                              info->ExceptionRecord->ExceptionInformation[1]));
            efx::diag::warn(text);
        }
        logCrashStack(info);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
    // Skalierung anmelden, bevor irgendein Fenster entsteht. Danach ist es zu
    // spaet, und alles erscheint verwaschen.
    announceDpiAwareness();

    // Als Allererstes das Protokoll. Vor allem, was abstuerzen koennte.
    // Im Selbsttest liegen Einstellungen und Protokoll in einem eigenen
    // Ordner — der Test fasst die echten Einstellungen nie an.
    if (!efx::gui::selbsttestVorbereiten()) efx::paths::setOverrideDirForTesting("");
    efx::diag::open(efx::paths::startupLogPath());
    // Gleich nach dem Protokoll: ab hier hinterlaesst ein Absturz eine Spur.
    SetUnhandledExceptionFilter(logCrash);
    efx::diag::writeHeader({
        {"Version", efx::versionWithRevision()},
        {"Built", std::string(__DATE__) + " " + __TIME__},
        {"Program", exeDirectory()},
        {"Settings", efx::paths::configDir()},
        {"System", windowsVersion()},
        {"Cores", std::to_string(std::thread::hardware_concurrency())},
        {"System locale", systemLocale()},
    });

    // COM anmelden — der heutige Ordnerdialog (IFileOpenDialog) braucht es.
    //
    // APARTMENTTHREADED, weil die Shell-Dialoge das erwarten.
    // DISABLE_OLE1DDE spart das Aufsetzen einer Technik von 1990, die hier
    // niemand benutzt, und verkuerzt den Start messbar.
    //
    // Schlaegt es fehl, laeuft das Programm trotzdem: der Ordnerdialog faellt
    // dann auf SHBrowseForFolder zurueck. Deshalb wird das Ergebnis nur
    // vermerkt und nicht geprueft.
    const HRESULT comReady = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    efx::diag::info(SUCCEEDED(comReady) ? "COM ready" : "COM unavailable");

    efx::gui::App app;

    app.startup();

    // Sprache: gespeicherte Wahl, sonst aus den Systemeinstellungen ableiten.
    if (app.settings().languageCode.empty()) {
        const auto language = efx::i18n::fromSystemLocale(systemLocale());
        efx::i18n::setLanguage(language);
        for (const auto& info : efx::i18n::languages()) {
            if (info.language == language) app.settings().languageCode = info.code;
        }
    }

    if (commandLine && *commandLine) {
        std::string path = toUtf8(commandLine);
        if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.size() - 2);
        }
        app.openFile(path);
    }

    efx::render::Backend preferred = efx::render::Backend::Direct3D11;
    efx::render::backendFromCode(app.settings().rendererCode, preferred);

    // Schleife wegen des Wechsels der Grafikschnittstelle: der ist ein
    // Neustart des Fensters, kein Neustart des Programms.
    efx::render::Backend used = preferred;
    while (runSession(app, preferred, used)) {
        efx::diag::info("rebuilding window for renderer change");
        // Das App-Objekt ueberlebt den Wechsel, seine Texturkennungen zeigen
        // aber auf Ressourcen des alten Renderers. Unter Direct3D ist so eine
        // Kennung ein Zeiger — damit zu zeichnen stuerzt ab.
        app.forgetGraphicsResources();
        efx::render::backendFromCode(app.settings().rendererCode, preferred);
    }

    app.shutdown();
    if (SUCCEEDED(comReady)) CoUninitialize();
    efx::diag::close();
    return 0;
}
