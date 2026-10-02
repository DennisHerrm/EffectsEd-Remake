// Der Auto-Updater: GitHub fragen, laden, installieren. Siehe update.h.
//
// Alles Netz laeuft in einem Hintergrundfaden; die Oberflaeche liest nur eine
// Kopie des Zustands.
//
// ZUGRIFF: Die fertigen Programme liegen in einem OEFFENTLICHEN Repository
// (update::kRepo). Gefragt wird ohne Anmeldung — es wird kein Schluessel
// gelesen, gespeichert oder mitgeschickt. GitHub erlaubt so 60 Anfragen je
// Stunde und Rechner; eine beim Start reicht.
//
// INSTALLATION: Die laufende .exe laesst sich unter Windows nicht
// ueberschreiben, wohl aber umbenennen. Sie wird zu "<name>.exe.alt", die neue
// kommt an ihren Platz; beim naechsten Start wird die alte geloescht. Jede
// Datei wird erst als "<name>.neu" geschrieben und dann ersetzt — ein
// abgebrochener Download hinterlaesst keine halbe Datei.
//
// Was es bewusst NICHT tut: nichts ausfuehren, was heruntergeladen wurde,
// ausser der eigenen .exe nach dem Neustart, den der Anwender ausloest;
// nichts ausserhalb des Programmordners anfassen (update::targetInFolder);
// keine Einstellungen ersetzen.

#include "update.h"

#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "imgui.h"
#include "selbsttest.h"

#include "efx/assets.h"
#include "efx/diag.h"
#include "efx/i18n.h"
#include "efx/version.h"

namespace efx::gui::updater {
namespace {

namespace fs = std::filesystem;
using i18n::Str;
using i18n::tr;

// Die MinGW-Fassung ist die fuer Windows 7 (build_mingw.bat); sie laedt nur
// Pakete mit "-win7" im Namen, siehe update::zipAsset.
#if defined(__MINGW32__)
constexpr bool kWin7Build = true;
#else
constexpr bool kWin7Build = false;
#endif

// Absichtlich nie freigegeben: der Hintergrundfaden kann beim Beenden noch
// laufen, und dann darf sein Zustand nicht schon zerstoert sein.
struct Shared {
    std::mutex mutex;
    UpdateInfo status;
    bool windowOpen = false;
    bool hintDismissed = false;  // "Spaeter": der Hinweis unten verschwindet bis zum naechsten Fund
    std::atomic<bool> busy{false};
};
Shared& shared() {
    static Shared* instance = new Shared();
    return *instance;
}

void modify(const std::function<void(UpdateInfo&)>& change) {
    std::lock_guard<std::mutex> lock(shared().mutex);
    change(shared().status);
}

void fail(const std::string& message) {
    modify([&](UpdateInfo& s) {
        s.state = UpdatePhase::Failed;
        s.message = message;
    });
}

template <typename... Args>
std::string format(Str id, Args... args) {
    char text[1024];
    std::snprintf(text, sizeof(text), tr(id), args...);
    return text;
}

std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                      nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr,
                        nullptr);
    return s;
}

fs::path exePath() {
    std::wstring p(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, p.data(), static_cast<DWORD>(p.size()));
        if (n < p.size()) {
            p.resize(n);
            return fs::path(p);
        }
        p.resize(p.size() * 2);
    }
}

fs::path downloadFolder() {
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    return fs::path(temp) / L"efxed_update";
}

// --- HTTP ------------------------------------------------------------------

struct Answer {
    DWORD status = 0;
    std::string body;
    std::string error;
};

// Im Selbsttest geht nichts ins Netz: EFXED_UPDATE_QUELLE nennt einen Ordner,
// der die Antwort (latest.json) und die Dateien des Releases enthaelt. So
// laeuft der ganze Ablauf — Lesen, Laden, Entpacken — ohne GitHub.
std::string testSource() {
    if (!selbsttestAktiv()) return {};
    const char* dir = std::getenv("EFXED_UPDATE_QUELLE");
    return dir ? std::string(dir) : std::string();
}

Answer fetchFromTestSource(const std::string& dir, const std::wstring& url) {
    Answer a;
    const std::string u = narrow(url);
    const std::string name = u.find("/releases/latest") != std::string::npos
                                 ? std::string("latest.json")
                                 : u.substr(u.rfind('/') + 1);
    std::ifstream in(fs::path(wide(dir)) / fs::path(wide(name)), std::ios::binary);
    if (!in) {
        a.status = 404;
        return a;
    }
    a.body.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    a.status = 200;
    return a;
}

Answer fetch(const std::wstring& url, const std::vector<std::wstring>& headers,
             const std::function<void(std::uint64_t, std::uint64_t)>& progress = {}) {
    if (const std::string dir = testSource(); !dir.empty()) return fetchFromTestSource(dir, url);

    Answer a;
    URL_COMPONENTSW parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256] = {};
    wchar_t path[4096] = {};
    wchar_t extra[4096] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 4096;
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = 4096;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        // Nur HTTPS. Eine Umleitung auf http:// waere ein Angriffsweg.
        a.error = format(Str::UpdErrConnect, narrow(url).c_str());
        return a;
    }
    const std::wstring agent = L"EffectsEd/" + wide(versionWithRevision());
    HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        // Vor Windows 8.1 gibt es AUTOMATIC_PROXY nicht.
        session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (session == nullptr) {
        a.error = format(Str::UpdErrConnect, "WinHTTP");
        return a;
    }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 30000);
    // GitHub spricht nur TLS 1.2 und neuer. Windows 7 bietet das von sich aus
    // nicht an, wohl aber auf ausdrueckliche Bitte (mit KB3140245, auf
    // aktuellen Windows-7-Rechnern vorhanden). TLS 1.3 kennt erst Windows 11;
    // wo das Setzen scheitert, bleibt es bei 1.2 bzw. bei der Voreinstellung.
    DWORD protocols = 0x00000800 /* TLS1_2 */ | 0x00002000 /* TLS1_3 */;
    if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
        protocols = 0x00000800;
        WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
    }
    // Umleitungen (GitHub schickt Downloads auf seinen Speicher) nur auf HTTPS.
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
    HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
    const std::wstring target = std::wstring(path) + extra;
    HINTERNET request = connection == nullptr
                            ? nullptr
                            : WinHttpOpenRequest(connection, L"GET", target.c_str(), nullptr,
                                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 WINHTTP_FLAG_SECURE);
    if (request == nullptr) {
        a.error = format(Str::UpdErrConnect, narrow(host).c_str());
    } else {
        std::wstring lines;
        for (const std::wstring& h : headers) lines += h + L"\r\n";
        const BOOL sent =
            WinHttpSendRequest(request, lines.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : lines.c_str(),
                               lines.empty() ? 0 : static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA,
                               0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr);
        if (!sent) {
            const DWORD code = GetLastError();
            // 12175 ERROR_WINHTTP_SECURE_FAILURE, 12157 ..._SECURE_CHANNEL_ERROR:
            // fast immer ein Windows 7 ohne TLS 1.2. Das sagen, statt nur
            // eine Nummer zu nennen.
            a.error = (code == 12175 || code == 12157)
                          ? std::string(tr(Str::UpdErrTls))
                          : format(Str::UpdErrNoAnswer, narrow(host).c_str(),
                                   static_cast<unsigned long>(code));
        } else {
            DWORD size = sizeof(a.status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &a.status, &size,
                                WINHTTP_NO_HEADER_INDEX);
            DWORD lengthValue = 0;
            DWORD lengthSize = sizeof(lengthValue);
            std::uint64_t total = 0;
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &lengthValue, &lengthSize,
                                    WINHTTP_NO_HEADER_INDEX)) {
                total = lengthValue;
            }
            for (;;) {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(request, &available)) {
                    a.error = format(Str::UpdErrNoAnswer, narrow(host).c_str(),
                                     static_cast<unsigned long>(GetLastError()));
                    break;
                }
                if (available == 0) break;
                // Ein Programm-Update ist ein paar MB; mehr als 256 MB ist
                // kein Update, sondern ein Fehler (oder Schlimmeres).
                if (a.body.size() + available > (256u << 20)) {
                    a.error = format(Str::UpdErrDownload, "> 256 MB");
                    break;
                }
                const size_t before = a.body.size();
                a.body.resize(before + available);
                DWORD read = 0;
                if (!WinHttpReadData(request, a.body.data() + before, available, &read)) {
                    a.error = format(Str::UpdErrNoAnswer, narrow(host).c_str(),
                                     static_cast<unsigned long>(GetLastError()));
                    break;
                }
                a.body.resize(before + read);
                if (progress) progress(a.body.size(), total);
            }
        }
        WinHttpCloseHandle(request);
    }
    if (connection != nullptr) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return a;
}

// --- Die beiden Hintergrundauftraege ----------------------------------------

void checkJob() {
    const std::wstring url =
        L"https://api.github.com/repos/" + wide(update::kRepo) + L"/releases/latest";
    const Answer a = fetch(url, {L"Accept: application/vnd.github+json",
                                 L"X-GitHub-Api-Version: 2022-11-28"});
    diag::info("update: GitHub (" + std::string(update::kRepo) + ") answers " +
               std::to_string(a.status) + (a.error.empty() ? "" : " - " + a.error));
    if (!a.error.empty()) return fail(a.error);
    if (a.status == 404) return fail(tr(Str::UpdNoRelease));
    if (a.status == 401 || a.status == 403 || a.status == 429) return fail(tr(Str::UpdNoAccess));
    update::Release release;
    std::string error;
    if (a.status != 200 || !update::parseRelease(a.body, release, &error)) {
        return fail(format(Str::UpdErrBadAnswer, static_cast<int>(a.status), error.c_str()));
    }
    const bool newer = update::isNewer(release.tag, localVersion());
    diag::info("update: latest release " + release.tag + ", installed " + localVersion() +
               (newer ? " -> NEWER" : " -> current"));
    std::lock_guard<std::mutex> lock(shared().mutex);
    shared().status.release = release;
    shared().status.state = newer ? UpdatePhase::Available : UpdatePhase::Current;
    if (newer) shared().hintDismissed = false;
}

void installJob(const update::Release& release) {
    const update::Asset* asset = update::zipAsset(release, kWin7Build);
    if (asset == nullptr) return fail(tr(Str::UpdNoZip));
    const long long expected = asset->size;
    const auto progress = [expected](std::uint64_t n, std::uint64_t total) {
        const double whole = total > 0 ? static_cast<double>(total) : static_cast<double>(expected);
        const double part = whole > 0.0 ? static_cast<double>(n) / whole : 0.0;
        modify([&](UpdateInfo& s) { s.progress = std::min(1.0, part); });
    };
    const Answer a = fetch(wide(asset->downloadUrl), {}, progress);
    if (!a.error.empty() || a.status != 200 || a.body.size() < 22) {
        return fail(format(Str::UpdErrDownload,
                           a.error.empty() ? ("HTTP " + std::to_string(a.status)).c_str()
                                           : a.error.c_str()));
    }
    modify([](UpdateInfo& s) { s.progress = 1.0; });

    // Das .zip ablegen und mit dem pk3-Leser oeffnen (dasselbe Format).
    std::error_code ec;
    const fs::path folder = downloadFolder();
    fs::create_directories(folder, ec);
    const fs::path zipFile = folder / L"update.zip";
    {
        std::ofstream out(zipFile, std::ios::binary | std::ios::trunc);
        out.write(a.body.data(), static_cast<std::streamsize>(a.body.size()));
        if (!out) return fail(format(Str::UpdErrWrite, narrow(zipFile.wstring()).c_str()));
    }
    std::string error;
    const std::string zipPath = narrow(zipFile.wstring());
    const std::vector<std::string> names = assets::readZipDirectory(zipPath, &error);
    if (names.empty()) return fail(format(Str::UpdErrZip, error.c_str()));

    // Erst alles auspacken und als .neu ablegen, DANN ersetzen: geht beim
    // Auspacken etwas schief, ist am Programm noch nichts veraendert.
    const std::string top = update::commonFolder(names);
    const fs::path exe = exePath();
    const fs::path dir = exe.parent_path();
    struct Pending {
        fs::path target;
        fs::path fresh;
        std::string relative;
    };
    std::vector<Pending> pending;
    for (const std::string& name : names) {
        const std::string relative = update::targetInFolder(name, top);
        if (relative.empty()) continue;
        const std::vector<unsigned char> content = assets::readFromZip(zipPath, name, &error);
        if (content.empty() && !error.empty()) return fail(format(Str::UpdErrZip, error.c_str()));
        // Die Programmdatei heisst im Paket efxed.exe — hier vielleicht
        // anders, wenn der Anwender sie umbenannt hat. Ersetzt wird die, die
        // gerade laeuft.
        std::string lowered = relative;
        for (char& c : lowered) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const fs::path target = lowered == "efxed.exe" ? exe : dir / fs::path(wide(relative));
        fs::create_directories(target.parent_path(), ec);
        fs::path fresh = target;
        fresh += L".neu";
        std::ofstream out(fresh, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(content.data()),
                  static_cast<std::streamsize>(content.size()));
        if (!out) {
            out.close();
            DeleteFileW(fresh.c_str());
            for (const Pending& p : pending) DeleteFileW(p.fresh.c_str());
            return fail(format(Str::UpdErrWrite, relative.c_str()));
        }
        pending.push_back({target, fresh, relative});
    }

    int replaced = 0;
    std::string problem;
    for (const Pending& p : pending) {
        if (p.target == exe) {
            // Die laufende .exe nur umbenennen — ueberschreiben geht nicht.
            fs::path old = exe;
            old += L".alt";
            DeleteFileW(old.c_str());
            if (!MoveFileExW(exe.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING) ||
                !MoveFileExW(p.fresh.c_str(), exe.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                const DWORD code = GetLastError();
                MoveFileExW(old.c_str(), exe.c_str(), MOVEFILE_REPLACE_EXISTING);  // zurueck
                problem = format(Str::UpdErrReplace, p.relative.c_str(), static_cast<unsigned long>(code));
                break;
            }
        } else if (!MoveFileExW(p.fresh.c_str(), p.target.c_str(),
                                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            problem = format(Str::UpdErrReplace, p.relative.c_str(),
                             static_cast<unsigned long>(GetLastError()));
            break;
        }
        ++replaced;
    }
    for (const Pending& p : pending) DeleteFileW(p.fresh.c_str());
    diag::info("update: " + std::to_string(replaced) + " files replaced" +
               (problem.empty() ? "" : " - " + problem));
    modify([&](UpdateInfo& s) {
        s.files = replaced;
        if (!problem.empty()) {
            s.state = UpdatePhase::Failed;
            s.message = problem;
        } else {
            s.state = UpdatePhase::Installed;
        }
    });
}

void inBackground(std::function<void()> job) {
    bool expected = false;
    if (!shared().busy.compare_exchange_strong(expected, true)) return;  // es laeuft schon einer
    std::thread([job = std::move(job)] {
        job();
        shared().busy = false;
    }).detach();
}

}  // namespace

std::string localVersion() {
    // Im Selbsttest laesst sich die eigene Fassung vorgeben — sonst haengt
    // das Ergebnis davon ab, welche Revision gerade gebaut ist.
    if (selbsttestAktiv()) {
        if (const char* forced = std::getenv("EFXED_UPDATE_LOKAL"); forced && forced[0]) return forced;
    }
    return versionWithRevision();
}

UpdateInfo status() {
    std::lock_guard<std::mutex> lock(shared().mutex);
    return shared().status;
}

void check(bool quiet) {
    if (shared().busy) {
        if (!quiet) openWindow();
        return;
    }
    modify([](UpdateInfo& s) {
        s.state = UpdatePhase::Checking;
        s.message.clear();
    });
    if (!quiet) openWindow();
    inBackground(checkJob);
}

void install() {
    const UpdateInfo s = status();
    if (s.state != UpdatePhase::Available && s.state != UpdatePhase::Failed) return;
    if (s.release.tag.empty()) return;
    modify([](UpdateInfo& x) {
        x.state = UpdatePhase::Downloading;
        x.progress = 0.0;
        x.message.clear();
    });
    inBackground([release = s.release] { installJob(release); });
}

void atStartup(bool checkNow) {
    // Reste eines frueheren Updates: die alte .exe und das geladene .zip.
    fs::path old = exePath();
    old += L".alt";
    DeleteFileW(old.c_str());
    std::error_code ec;
    fs::remove_all(downloadFolder(), ec);
    // Im Selbsttest nie von selbst ins Netz.
    if (checkNow && !selbsttestAktiv()) check(true);
}

void launchNewVersion() {
    const fs::path exe = exePath();
    std::wstring line = L"\"" + exe.wstring() + L"\" --nach-update=" +
                        std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                       exe.parent_path().c_str(), &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        diag::info("update: new version started");
    } else {
        diag::info("update: restart failed, error " + std::to_string(GetLastError()));
    }
}

bool waitForPredecessor(const std::string& argument) {
    const std::string prefix = "--nach-update=";
    if (argument.compare(0, prefix.size(), prefix) != 0) return false;
    const DWORD pid = static_cast<DWORD>(std::strtoul(argument.c_str() + prefix.size(), nullptr, 10));
    if (HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid)) {
        WaitForSingleObject(process, 15000);
        CloseHandle(process);
    }
    return true;
}

void openWindow() {
    std::lock_guard<std::mutex> lock(shared().mutex);
    shared().windowOpen = true;
}

bool windowOpen() {
    std::lock_guard<std::mutex> lock(shared().mutex);
    return shared().windowOpen;
}

void drawStatusHint() {
    const UpdateInfo s = status();
    bool dismissed = false;
    {
        std::lock_guard<std::mutex> lock(shared().mutex);
        dismissed = shared().hintDismissed;
    }
    if (s.state != UpdatePhase::Available && s.state != UpdatePhase::Installed) return;
    if (dismissed && s.state == UpdatePhase::Available) return;
    char text[200];
    if (s.state == UpdatePhase::Available) {
        std::snprintf(text, sizeof(text), tr(Str::UpdStatusAvail), s.release.tag.c_str());
    } else {
        std::snprintf(text, sizeof(text), "%s", tr(Str::UpdStatusDone));
    }
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.85f, 0.45f, 1.0f));
    if (ImGui::SmallButton(text)) openWindow();
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
}

bool drawWindow() {
    bool open = windowOpen();
    if (!open) return false;
    bool restart = false;
    const UpdateInfo s = status();
    const float em = ImGui::GetFontSize();
    ImGui::SetNextWindowSize(ImVec2(em * 34.0f, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    if (ImGui::Begin((std::string(tr(Str::UpdTitle)) + "###update").c_str(), &open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled(tr(Str::UpdInstalled), localVersion().c_str());
        switch (s.state) {
            case UpdatePhase::Idle:
            case UpdatePhase::Checking:
                ImGui::TextUnformatted(tr(Str::UpdChecking));
                break;
            case UpdatePhase::Current:
                ImGui::TextUnformatted(tr(Str::UpdCurrent));
                break;
            case UpdatePhase::Available:
            case UpdatePhase::Downloading:
            case UpdatePhase::Installed:
                ImGui::Text(tr(Str::UpdAvailable), s.release.tag.c_str());
                if (!s.release.body.empty()) {
                    ImGui::TextDisabled("%s", tr(Str::UpdNotes));
                    if (ImGui::BeginChild("##notes", ImVec2(em * 32.0f, em * 12.0f),
                                          ImGuiChildFlags_Borders)) {
                        ImGui::PushTextWrapPos(0.0f);
                        ImGui::TextUnformatted(s.release.body.c_str());
                        ImGui::PopTextWrapPos();
                    }
                    ImGui::EndChild();
                }
                if (s.state == UpdatePhase::Downloading) {
                    ImGui::ProgressBar(static_cast<float>(s.progress), ImVec2(-FLT_MIN, 0.0f));
                } else if (s.state == UpdatePhase::Installed) {
                    ImGui::Text(tr(Str::UpdDone), s.files);
                }
                break;
            case UpdatePhase::Failed:
                ImGui::PushTextWrapPos(em * 32.0f);
                ImGui::Text(tr(Str::UpdError), s.message.c_str());
                ImGui::PopTextWrapPos();
                break;
        }
        ImGui::Separator();
        if (s.state == UpdatePhase::Available) {
            if (ImGui::Button(tr(Str::UpdInstall))) install();
            ImGui::SameLine();
        } else if (s.state == UpdatePhase::Installed) {
            if (ImGui::Button(tr(Str::UpdRestart))) restart = true;
            ImGui::SameLine();
        } else if (s.state == UpdatePhase::Failed || s.state == UpdatePhase::Current) {
            if (ImGui::Button(tr(Str::UpdRetry))) check(false);
            ImGui::SameLine();
        }
        if (!s.release.htmlUrl.empty() && !selbsttestAktiv()) {
            if (ImGui::Button(tr(Str::UpdOpenPage))) {
                ShellExecuteW(nullptr, L"open", wide(s.release.htmlUrl).c_str(), nullptr, nullptr,
                              SW_SHOWNORMAL);
            }
            ImGui::SameLine();
        }
        if (ImGui::Button(tr(Str::UpdLater))) {
            open = false;
            std::lock_guard<std::mutex> lock(shared().mutex);
            shared().hintDismissed = s.state == UpdatePhase::Available;
        }
    }
    ImGui::End();
    std::lock_guard<std::mutex> lock(shared().mutex);
    shared().windowOpen = open;
    return restart;
}

}  // namespace efx::gui::updater
