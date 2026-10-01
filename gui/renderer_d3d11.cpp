// Direct3D-11-Umsetzung.
//
// Die einzige Datei, die d3d11.h einbindet. Nichts hier trifft Entscheidungen —
// welcher Renderer benutzt wird, entscheidet src/renderer.cpp, und das ist
// geprüft. Hier wird nur angelegt und gezeichnet.
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <map>
#include <unordered_set>
#include <cstdio>

#include "efx/diag.h"
#include "efx/renderer.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"

using Microsoft::WRL::ComPtr;

namespace efx::render {
namespace {

// Direct3D und den Shaderuebersetzer zur LAUFZEIT laden.
//
// Fest gebunden (`d3d11.lib`, `d3dcompiler.lib`) entstehen Eintraege in der
// Importtabelle. Fehlt eine der beiden DLLs, laedt Windows das Programm gar
// nicht — kein Fenster, keine Meldung, kein Protokoll.
//
// Genau das passiert auf Windows 7 ohne das Platform Update: `d3d11.dll` fehlt,
// und `d3dcompiler_47.dll` gehoert dort ueberhaupt nicht zum System.
//
// Der Rueckfall auf OpenGL hilft nur, wenn das Programm ueberhaupt startet.
// Also beide ueber LoadLibrary — fehlen sie, meldet sich Direct3D als nicht
// verfuegbar, und die Wahl faellt auf OpenGL.
struct D3DEntryPoints {
    using CreateFn = HRESULT(WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
                                      const D3D_FEATURE_LEVEL*, UINT, UINT,
                                      const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
                                      ID3D11Device**, D3D_FEATURE_LEVEL*,
                                      ID3D11DeviceContext**);
    using CompileFn = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*,
                                       ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT,
                                       ID3DBlob**, ID3DBlob**);
    CreateFn create = nullptr;
    CompileFn compile = nullptr;
    const char* missing = nullptr;
};

const D3DEntryPoints& d3dEntryPoints() {
    // Einmal laden, dann behalten. Die Bibliotheken werden absichtlich nicht
    // freigegeben: sie leben so lange wie das Programm, und ein Freigeben
    // waehrend laufender Geraete waere ein Absturz.
    static const D3DEntryPoints points = [] {
        D3DEntryPoints result;
        const HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
        if (!d3d11) {
            result.missing = "d3d11.dll";
            return result;
        }
        result.create = reinterpret_cast<D3DEntryPoints::CreateFn>(
            reinterpret_cast<void*>(
                GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain")));
        if (!result.create) {
            result.missing = "D3D11CreateDeviceAndSwapChain";
            return result;
        }

        // Der Shaderuebersetzer liegt je nach System unter verschiedenen
        // Namen. 47 ist der aktuelle; die aelteren stehen dahinter, damit ein
        // System mit altem DirectX-SDK trotzdem laeuft.
        for (const wchar_t* name : {L"d3dcompiler_47.dll", L"d3dcompiler_46.dll",
                                    L"d3dcompiler_43.dll"}) {
            if (const HMODULE lib = LoadLibraryW(name)) {
                result.compile = reinterpret_cast<D3DEntryPoints::CompileFn>(
                    reinterpret_cast<void*>(GetProcAddress(lib, "D3DCompile")));
                if (result.compile) break;
            }
        }
        if (!result.compile) result.missing = "d3dcompiler";
        return result;
    }();
    return points;
}

}  // namespace


namespace {

class D3D11Renderer : public Renderer {
public:
    bool create(HWND hwnd, Probe& probe) {
        hwnd_ = hwnd;

        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferDesc.RefreshRate.Numerator = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = hwnd;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        // Sind die Bibliotheken ueberhaupt da? Auf Windows 7 ohne Platform
        // Update fehlt d3d11.dll, und d3dcompiler_47.dll gehoert dort gar
        // nicht zum System.
        const D3DEntryPoints& points = d3dEntryPoints();
        if (points.missing) {
            probe.available = false;
            probe.failure = std::string("Direct3D 11 nicht verfuegbar (") +
                            points.missing + " fehlt)";
            diag::info(probe.failure);
            return false;
        }

        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0,
                                            D3D_FEATURE_LEVEL_10_1,
                                            D3D_FEATURE_LEVEL_10_0};
        D3D_FEATURE_LEVEL achieved{};

        HRESULT hr = points.create(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels,
            static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, &desc,
            &swapChain_, &device_, &achieved, &context_);

        if (hr == DXGI_ERROR_UNSUPPORTED) {
            // Zweiter Versuch mit dem Software-Rasterisierer. Ueber
            // Remotedesktop und in virtuellen Maschinen ist das oft der
            // einzige Weg, der funktioniert.
            diag::info("hardware unavailable, trying WARP");
            hr = points.create(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels,
                static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, &desc,
                &swapChain_, &device_, &achieved, &context_);
        }

        if (FAILED(hr)) {
            char message[128];
            std::snprintf(message, sizeof(message),
                          "D3D11CreateDeviceAndSwapChain lieferte 0x%08lX",
                          static_cast<unsigned long>(hr));
            probe.failure = message;
            probe.available = false;
            return false;
        }

        // Name der Grafikkarte fuers Protokoll.
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(device_.As(&dxgiDevice)) &&
            SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
            DXGI_ADAPTER_DESC adapterDesc{};
            if (SUCCEEDED(adapter->GetDesc(&adapterDesc))) {
                char name[256]{};
                WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, name,
                                    sizeof(name), nullptr, nullptr);
                probe.adapter = name;
            }
        }

        // Kann der Treiber ueberhaupt nebenlaeufig anlegen? Ist das FALSE,
        // verhindert grobe Synchronisierung jede Nebenlaeufigkeit, und das
        // Verteilen auf Faeden kostet dann nur.
        D3D11_FEATURE_DATA_THREADING threading{};
        if (SUCCEEDED(device_->CheckFeatureSupport(
                D3D11_FEATURE_THREADING, &threading, sizeof(threading)))) {
            concurrentCreates_ = threading.DriverConcurrentCreates != FALSE;
            diag::info(std::string("DriverConcurrentCreates: ") +
                       (concurrentCreates_ ? "yes" : "no - textures will be created on the main thread"));
        }

        diag::info("device and swap chain created");

        char version[64];
        std::snprintf(version, sizeof(version), "Feature Level %d.%d",
                      (achieved >> 12) & 0xF, (achieved >> 8) & 0xF);
        probe.version = version;
        probe.available = true;
        probe.backend = Backend::Direct3D11;
        probe_ = probe;

        {
            diag::Step step("Create back buffer view");
            createBackBufferView();
            if (!backBufferView_) step.fail("CreateRenderTargetView returned nothing");
        }
        {
            diag::Step step("Create viewport resources");
            if (!createViewportResources()) {
                step.fail("shader or buffer creation failed");
                probe.failure = "viewport resources could not be created";
                probe.available = false;
                return false;
            }
        }

        // Kein ImGui_ImplDX11_Init hier — der ImGui-Kontext existiert an
        // dieser Stelle noch nicht. Siehe initImGuiBackend().
        return true;
    }

    ~D3D11Renderer() override = default;

    bool initImGuiBackend() override {
        if (!ImGui::GetCurrentContext()) {
            diag::error("initImGuiBackend called without an ImGui context");
            return false;
        }
        if (!ImGui_ImplDX11_Init(device_.Get(), context_.Get())) {
            diag::error("ImGui_ImplDX11_Init failed");
            return false;
        }
        backendReady_ = true;
        return true;
    }

    void shutdownImGuiBackend() override {
        if (!backendReady_) return;
        ImGui_ImplDX11_Shutdown();
        backendReady_ = false;
    }

    Backend backend() const override { return Backend::Direct3D11; }
    const Probe& probe() const override { return probe_; }

    void newFrame() override { ImGui_ImplDX11_NewFrame(); }

    void clear(float r, float g, float b, float a) override {
        const float colour[4] = {r, g, b, a};
        ID3D11RenderTargetView* view = backBufferView_.Get();
        context_->OMSetRenderTargets(1, &view, nullptr);
        context_->ClearRenderTargetView(view, colour);
    }

    void renderImGui() override {
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void present(bool vsync) override { swapChain_->Present(vsync ? 1 : 0, 0); }

    void resizeSwapChain(int width, int height) override {
        if (width <= 0 || height <= 0) return;
        backBufferView_.Reset();
        swapChain_->ResizeBuffers(0, static_cast<UINT>(width),
                                  static_cast<UINT>(height),
                                  DXGI_FORMAT_UNKNOWN, 0);
        createBackBufferView();
    }

    // --- Vorschau ----------------------------------------------------------

    void resizeViewport(int width, int height) override {
        if (width <= 0 || height <= 0) return;
        if (width == viewWidth_ && height == viewHeight_ && viewSrv_) return;
        viewWidth_ = width;
        viewHeight_ = height;

        viewSrv_.Reset();
        viewRtv_.Reset();
        viewDsv_.Reset();
        viewTexture_.Reset();

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> colour;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &colour))) return;
        device_->CreateRenderTargetView(colour.Get(), nullptr, &viewRtv_);
        device_->CreateShaderResourceView(colour.Get(), nullptr, &viewSrv_);
        // Auch die Textur selbst behalten: fuer ein Bildschirmfoto muss sie
        // in eine lesbare kopiert werden, und dafuer braucht es sie, nicht
        // nur ihre Sichten.
        viewTexture_ = colour;

        D3D11_TEXTURE2D_DESC depthDesc = desc;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ComPtr<ID3D11Texture2D> depth;
        if (SUCCEEDED(device_->CreateTexture2D(&depthDesc, nullptr, &depth))) {
            device_->CreateDepthStencilView(depth.Get(), nullptr, &viewDsv_);
        }
    }

    TextureId viewportTexture() const override {
        // ImGui erwartet den Zeiger auf die Sicht, nicht eine eigene Nummer.
        return reinterpret_cast<TextureId>(viewSrv_.Get());
    }

    // Direct3D legt den Ursprung oben links — wie ImGui.
    bool viewportTextureFlipped() const override { return false; }

    void beginViewport(float r, float g, float b, float a) override {
        if (!viewRtv_) return;
        const float colour[4] = {r, g, b, a};
        ID3D11RenderTargetView* rtv = viewRtv_.Get();
        context_->OMSetRenderTargets(1, &rtv, viewDsv_.Get());
        context_->ClearRenderTargetView(rtv, colour);
        if (viewDsv_) {
            context_->ClearDepthStencilView(viewDsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        }

        setViewportRect(0, 0, 0, 0);

        context_->IASetInputLayout(layout_.Get());
        context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
        resetPerViewState();
        setBlend(Blend::Opaque);
        setCulling(Cull::None);
        setFill(Fill::Solid);
    }

    void beginViewportPreserving() override {
        if (!viewRtv_) return;
        ID3D11RenderTargetView* rtv = viewRtv_.Get();
        context_->OMSetRenderTargets(1, &rtv, viewDsv_.Get());
        // KEIN ClearRenderTargetView — die Farbe des letzten Bildes bleibt.
        if (viewDsv_) {
            context_->ClearDepthStencilView(viewDsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        }

        setViewportRect(0, 0, 0, 0);

        context_->IASetInputLayout(layout_.Get());
        context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
        resetPerViewState();
        setBlend(Blend::Opaque);
        setCulling(Cull::None);
        setFill(Fill::Solid);
    }

    void clearViewportRect(float r, float g, float b, float a) override {
        // Direct3D 11.0 kann kein Rechteck loeschen: ClearRenderTargetView
        // trifft immer das ganze Ziel, und ClearView gibt es erst in 11.1 —
        // das faellt wegen Windows 7 aus.
        //
        // Also ein Viereck. Der Ausschnitt ist schon gesetzt, und ein Viereck
        // ueber den ganzen Bildbereich (-1..1) fuellt genau ihn. Das ist EIN
        // Zeichenaufruf; das Loeschen des ganzen Ziels waere es auch, wuerde
        // aber die anderen Kacheln mitnehmen.
        // Ohne efx/scene.h: dieser Renderer kennt den Kern absichtlich nicht
        // weiter als bis renderer.h. Die Reihenfolge ist RGBA und steht so im
        // Eingabelayout unten (DXGI_FORMAT_R8G8B8A8_UNORM auf COLOR0).
        const auto to255 = [](float v) -> uint32_t {
            const int i = static_cast<int>(v * 255.0f + 0.5f);
            return static_cast<uint32_t>(i < 0 ? 0 : (i > 255 ? 255 : i));
        };
        const uint32_t colour = to255(r) | (to255(g) << 8) | (to255(b) << 16) |
                                (to255(a) << 24);
        const Vertex quad[4] = {
            {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}, colour},
            {{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}, colour},
            {{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, colour},
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, colour},
        };
        static const unsigned short order[6] = {0, 1, 2, 0, 2, 3};

        // Die Einheitsmatrix: die Eckpunkte stehen schon im Bildbereich.
        static const float identity[16] = {
            1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
        setCamera(identity, identity);

        setBlend(Blend::Opaque);

        // KEIN Tiefenschreiben.
        //
        // Hier stand setDepthWrite(true), und das hat die Vorschau unter
        // Direct3D vollstaendig ausgeloescht — jede Kachel blieb schwarz.
        //
        // Der Grund: das Viereck liegt bei z = 0, und 0 ist bei Direct3D die
        // NAHE Ebene. Mit Tiefenschreiben stand danach in der ganzen Kachel
        // der naechstmoegliche Wert, und jedes Teilchen dahinter fiel beim
        // Vergleich LESS_EQUAL durch. Das Viereck hat sich selbst im Weg
        // gestanden.
        //
        // Unter OpenGL trat das nie auf: dort loescht clearViewportRect mit
        // glClear samt Tiefe, und die steht danach auf FERN. Zwei Wege,
        // dasselbe zu meinen, und nur einer war richtig.
        //
        // Noetig ist das Schreiben auch gar nicht: beginViewportPreserving
        // leert die Tiefe des ganzen Ziels ohnehin bei jedem Bild. Das
        // Viereck soll nur Farbe hinterlassen.
        setDepthWrite(false);
        drawTriangles(quad, 4, order, 6, kNoTexture);
    }

    void setViewportRect(int x, int y, int width, int height) override {
        // Null bedeutet: das ganze Ziel.
        if (width <= 0 || height <= 0) {
            x = 0;
            y = 0;
            width = viewWidth_;
            height = viewHeight_;
        }
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = static_cast<float>(x);
        vp.TopLeftY = static_cast<float>(y);
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &vp);

        // Dazu ein Schnittrechteck. Der Ausschnitt allein wirkt auf die
        // Eckpunkte; bei dicken Linien oder grossen Punkten kann trotzdem
        // etwas darueber hinausragen — und in einem Raster liefe das in die
        // Nachbarkachel.
        D3D11_RECT clip{};
        clip.left = x;
        clip.top = y;
        clip.right = x + width;
        clip.bottom = y + height;
        context_->RSSetScissorRects(1, &clip);
    }

    void setCamera(const float view[16], const float proj[16]) override {
        // Kein Transponieren.
        //
        // HLSL legt eine float4x4 in einem Konstantenpuffer standardmaessig
        // SPALTENWEISE ab. Unsere Matrizen sind bereits spaltenweise, weil sie
        // der OpenGL-Form folgen — es ist also nichts umzurechnen.
        //
        // Der erste Anlauf transponierte hier. Das ist die uebliche Korrektur,
        // wenn man mit DirectXMath rechnet, weil das zeilenweise arbeitet.
        // Bei uns ist sie falsch herum: aus P*V wurde die transponierte Matrix,
        // und die Geometrie erschien verzerrt und an der falschen Stelle.
        //
        // Beide Renderer rechnen damit gleich — was hier steht, steht so auch
        // in renderer_gl3.cpp.
        float mvp[16];
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += proj[k * 4 + row] * view[col * 4 + k];
                mvp[col * 4 + row] = sum;
            }
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context_->Map(constants_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                                    &mapped))) {
            std::memcpy(mapped.pData, mvp, sizeof(mvp));
            context_->Unmap(constants_.Get(), 0);
        }
        ID3D11Buffer* buffer = constants_.Get();
        context_->VSSetConstantBuffers(0, 1, &buffer);
    }

    void setBlend(Blend mode) override {
        const float factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        context_->OMSetBlendState(blendStates_[static_cast<int>(mode)].Get(), factor,
                                  0xFFFFFFFF);
    }

    void setCulling(Cull mode) override {
        cull_ = mode;
        applyRasterState();
    }

    void setFill(Fill mode) override {
        fill_ = mode;
        applyRasterState();
    }

    bool wantsZeroToOneDepth() const override { return true; }

    void setDepthWrite(bool enabled) override {
        depthWrite_ = enabled;
        applyDepthState();
    }

    void setDepthTest(bool enabled) override {
        depthTest_ = enabled;
        applyDepthState();
    }

    void setBlendFactors(BlendFactor src, BlendFactor dst) override {
        const int key = static_cast<int>(src) * 16 + static_cast<int>(dst);
        auto found = factorBlends_.find(key);
        if (found == factorBlends_.end()) {
            // Je Faktorpaar ein Zustand, beim ersten Gebrauch angelegt und
            // behalten — hoechstens elf mal elf.
            D3D11_BLEND_DESC bd{};
            auto& target = bd.RenderTarget[0];
            target.BlendEnable = !(src == BlendFactor::One && dst == BlendFactor::Zero);
            target.SrcBlend = d3dFactor(src);
            target.DestBlend = d3dFactor(dst);
            target.BlendOp = D3D11_BLEND_OP_ADD;
            // Der Alphakanal des Ziels wird wie bisher einfach ueberschrieben.
            target.SrcBlendAlpha = D3D11_BLEND_ONE;
            target.DestBlendAlpha = D3D11_BLEND_ZERO;
            target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            ComPtr<ID3D11BlendState> state;
            if (FAILED(device_->CreateBlendState(&bd, &state))) return;
            found = factorBlends_.emplace(key, state).first;
        }
        const float factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        context_->OMSetBlendState(found->second.Get(), factor, 0xFFFFFFFF);
    }

    void setAlphaTest(int mode) override {
        if (pixelConstants_.alphaTest == mode) return;
        pixelConstants_.alphaTest = mode;
        uploadPixelConstants();
    }

    void setLights(const Light* lights, int count) override {
        if (count > kMaxLights) count = kMaxLights;
        if (!lights || count < 0) count = 0;
        if (count == 0 && pixelConstants_.lightCount == 0) return;
        pixelConstants_.lightCount = count;
        for (int i = 0; i < count; ++i) {
            for (int k = 0; k < 3; ++k) {
                pixelConstants_.lightPos[i][k] = lights[i].pos[k];
                pixelConstants_.lightColour[i][k] = lights[i].rgb[k];
            }
            pixelConstants_.lightPos[i][3] = lights[i].radius;
            pixelConstants_.lightColour[i][3] = 0.0f;
        }
        uploadPixelConstants();
    }

    void drawTriangles(const Vertex* vertices, int vertexCount,
                       const unsigned short* indices, int indexCount,
                       TextureId texture) override {
        if (vertexCount <= 0 || indexCount <= 0) return;
        if (!uploadVertices(vertices, vertexCount)) return;
        if (!uploadIndices(indices, indexCount)) return;

        ID3D11ShaderResourceView* srv =
            texture == kNoTexture ? whiteSrv_.Get()
                                  : reinterpret_cast<ID3D11ShaderResourceView*>(texture);
        context_->PSSetShaderResources(0, 1, &srv);
        ID3D11SamplerState* sampler =
            clampTextures_.count(srv) != 0 ? clampSampler_.Get() : sampler_.Get();
        context_->PSSetSamplers(0, 1, &sampler);

        const UINT stride = sizeof(Vertex);
        const UINT offset = 0;
        ID3D11Buffer* vb = vertexBuffer_.Get();
        context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        context_->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R16_UINT, 0);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // Die Stellen im Ring: Indizes ab indexBase_, und jeder Index zaehlt
        // ab vertexBase_ (BaseVertexLocation).
        context_->DrawIndexed(static_cast<UINT>(indexCount), static_cast<UINT>(indexBase_),
                              static_cast<INT>(vertexBase_));
    }

    void drawLines(const Vertex* vertices, int vertexCount, float) override {
        if (vertexCount <= 0) return;
        if (!uploadVertices(vertices, vertexCount)) return;

        // Direct3D kennt keine Linienbreite. Fuer Achsen und Gitter reicht
        // eine Einheit; wer dickere Linien will, muesste sie als schmale
        // Rechtecke bauen — das ist es fuer ein Gitter nicht wert.
        ID3D11ShaderResourceView* srv = whiteSrv_.Get();
        context_->PSSetShaderResources(0, 1, &srv);
        ID3D11SamplerState* sampler = sampler_.Get();
        context_->PSSetSamplers(0, 1, &sampler);

        const UINT stride = sizeof(Vertex);
        const UINT offset = 0;
        ID3D11Buffer* vb = vertexBuffer_.Get();
        context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        context_->Draw(static_cast<UINT>(vertexCount), static_cast<UINT>(vertexBase_));
    }

    void endViewport() override {
        // Zurueck auf den Rueckpuffer, sonst zeichnet ImGui in die Vorschau.
        ID3D11RenderTargetView* rtv = backBufferView_.Get();
        context_->OMSetRenderTargets(1, &rtv, nullptr);
    }

    bool readViewport(std::vector<unsigned char>& rgba, int& width,
                      int& height) override {
        if (!device_ || !context_ || !viewTexture_ || viewWidth_ <= 0) return false;

        // Die Vorschautextur liegt im Grafikspeicher und ist nicht lesbar.
        // Direct3D verlangt dafuer eine zweite mit D3D11_USAGE_STAGING —
        // eine, die die Grafikkarte nur beschreibt und die CPU nur liest.
        D3D11_TEXTURE2D_DESC desc{};
        viewTexture_->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        ComPtr<ID3D11Texture2D> staging;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &staging))) return false;
        context_->CopyResource(staging.Get(), viewTexture_.Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
            return false;
        }

        width = viewWidth_;
        height = viewHeight_;
        rgba.assign(static_cast<size_t>(width) * height * 4, 255);
        const auto* source = static_cast<const unsigned char*>(mapped.pData);
        for (int y = 0; y < height; ++y) {
            // RowPitch ist NICHT width*4: Direct3D fuellt Zeilen auf. Wer das
            // uebersieht, bekommt ein schraeg verzogenes Bild.
            std::memcpy(rgba.data() + static_cast<size_t>(y) * width * 4,
                        source + static_cast<size_t>(y) * mapped.RowPitch,
                        static_cast<size_t>(width) * 4);
        }
        context_->Unmap(staging.Get(), 0);

        for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
        return true;
    }

    bool readBackbuffer(std::vector<unsigned char>& rgba, int& width,
                        int& height) override {
        if (!device_ || !context_ || !swapChain_) return false;
        ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
        D3D11_TEXTURE2D_DESC desc{};
        backBuffer->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &staging))) return false;
        context_->CopyResource(staging.Get(), backBuffer.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;
        width = static_cast<int>(desc.Width);
        height = static_cast<int>(desc.Height);
        rgba.assign(static_cast<size_t>(width) * height * 4, 255);
        const auto* source = static_cast<const unsigned char*>(mapped.pData);
        for (int y = 0; y < height; ++y) {
            std::memcpy(rgba.data() + static_cast<size_t>(y) * width * 4,
                        source + static_cast<size_t>(y) * mapped.RowPitch,
                        static_cast<size_t>(width) * 4);
        }
        context_->Unmap(staging.Get(), 0);
        for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
        return true;
    }

    TextureId createTexture(const unsigned char* rgba, int width, int height,
                            bool clamp, bool mipmaps) override {
        if (!rgba || width <= 0 || height <= 0) return kNoTexture;

        // Mipmaps: die Engine laedt jede Textur mit Mipmaps (R_CreateImage,
        // ausser "nomipmaps"). Ohne sie flimmert ein kleines oder fernes
        // Teilchen, weil jeder Bildpunkt einen anderen Texel trifft. Vorher
        // wurden beide Angaben hier verschluckt ((void)clamp; (void)mipmaps).
        //
        // Die Kette erzeugt die Grafikkarte selbst (GenerateMips) - dafuer
        // braucht die Textur RENDER_TARGET und darf nicht IMMUTABLE sein.
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = mipmaps ? 0 : 1;  // 0 = volle Kette
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        ComPtr<ID3D11Texture2D> texture;
        if (mipmaps) {
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
            if (FAILED(device_->CreateTexture2D(&desc, nullptr, &texture))) return kNoTexture;
            context_->UpdateSubresource(texture.Get(), 0, nullptr, rgba,
                                        static_cast<UINT>(width * 4), 0);
        } else {
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = rgba;
            data.SysMemPitch = static_cast<UINT>(width * 4);
            if (FAILED(device_->CreateTexture2D(&desc, &data, &texture))) return kNoTexture;
        }

        ID3D11ShaderResourceView* srv = nullptr;
        if (FAILED(device_->CreateShaderResourceView(texture.Get(), nullptr, &srv))) {
            return kNoTexture;
        }
        if (mipmaps) context_->GenerateMips(srv);
        // clampMap: der Rand wird nicht wiederholt. Welcher Sampler gilt,
        // entscheidet drawTriangles nach dieser Liste.
        if (clamp) clampTextures_.insert(srv);
        return reinterpret_cast<TextureId>(srv);
    }

    void destroyTexture(TextureId texture) override {
        if (texture == kNoTexture) return;
        auto* srv = reinterpret_cast<ID3D11ShaderResourceView*>(texture);
        clampTextures_.erase(srv);
        srv->Release();
    }

private:
    static D3D11_BLEND d3dFactor(BlendFactor f) {
        switch (f) {
            case BlendFactor::Zero: return D3D11_BLEND_ZERO;
            case BlendFactor::One: return D3D11_BLEND_ONE;
            case BlendFactor::SrcColor: return D3D11_BLEND_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
            case BlendFactor::DstColor: return D3D11_BLEND_DEST_COLOR;
            case BlendFactor::OneMinusDstColor: return D3D11_BLEND_INV_DEST_COLOR;
            case BlendFactor::SrcAlpha: return D3D11_BLEND_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
            case BlendFactor::DstAlpha: return D3D11_BLEND_DEST_ALPHA;
            case BlendFactor::OneMinusDstAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
            case BlendFactor::SrcAlphaSaturate: return D3D11_BLEND_SRC_ALPHA_SAT;
        }
        return D3D11_BLEND_ONE;
    }

    void applyDepthState() {
        ID3D11DepthStencilState* state = !depthTest_ ? depthOffState_.Get()
                                         : depthWrite_ ? depthWriteState_.Get()
                                                       : depthReadState_.Get();
        context_->OMSetDepthStencilState(state, 0);
    }

    // Zu Beginn jeder Ansicht: Tiefe an, Alphatest und Lichter aus.
    void resetPerViewState() {
        depthTest_ = true;
        depthWrite_ = true;
        applyDepthState();
        pixelConstants_.alphaTest = 0;
        pixelConstants_.lightCount = 0;
        uploadPixelConstants();
        ID3D11Buffer* buffer = pixelConstantBuffer_.Get();
        context_->PSSetConstantBuffers(1, 1, &buffer);
    }

    void uploadPixelConstants() {
        if (!pixelConstantBuffer_) return;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context_->Map(pixelConstantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD,
                                    0, &mapped))) {
            std::memcpy(mapped.pData, &pixelConstants_, sizeof(pixelConstants_));
            context_->Unmap(pixelConstantBuffer_.Get(), 0);
        }
    }

    // Ein einziges Shaderpaar fuer alles: Farbe aus dem Eckpunkt mal Textur,
    // dazu der Alphatest der Shaderstufe (alphaFunc) und die Lichter fuer den
    // Raum (siehe renderer.h). Dasselbe steht in renderer_gl3.cpp.
    static const char* shaderSource() {
        return
            "cbuffer Constants : register(b0) { float4x4 mvp; };\n"
            "cbuffer PixelConstants : register(b1) {\n"
            "    int alphaTest; int lightCount; int pad0; int pad1;\n"
            "    float4 lightPos[16]; float4 lightColour[16]; };\n"
            "struct VSIn  { float3 pos : POSITION; float2 uv : TEXCOORD0;"
            "               float4 col : COLOR0; };\n"
            "struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0;"
            "               float4 col : COLOR0; float3 world : TEXCOORD1; };\n"
            "VSOut VSMain(VSIn input) {\n"
            "    VSOut output;\n"
            "    output.pos = mul(mvp, float4(input.pos, 1.0));\n"
            "    output.uv = input.uv;\n"
            "    output.col = input.col;\n"
            "    output.world = input.pos;\n"
            "    return output;\n"
            "}\n"
            "Texture2D tex : register(t0);\n"
            "SamplerState smp : register(s0);\n"
            "float4 PSMain(VSOut input) : SV_Target {\n"
            "    float4 c = input.col * tex.Sample(smp, input.uv);\n"
            "    if (alphaTest == 1 && c.a <= 0.0) discard;\n"
            "    if (alphaTest == 2 && c.a >= 0.5) discard;\n"
            "    if (alphaTest == 3 && c.a < 0.5) discard;\n"
            "    if (alphaTest == 4 && c.a < 0.75) discard;\n"
            "    float3 lit = float3(0.0, 0.0, 0.0);\n"
            "    for (int i = 0; i < lightCount; ++i) {\n"
            "        float d = length(input.world - lightPos[i].xyz);\n"
            "        float f = lightPos[i].w > 0.0 ? max(0.0, 1.0 - d / lightPos[i].w) : 0.0;\n"
            "        lit += lightColour[i].rgb * f;\n"
            "    }\n"
            "    c.rgb += c.rgb * lit;\n"
            "    return c;\n"
            "}\n";
    }

    bool compile(const char* entry, const char* target, ComPtr<ID3DBlob>& out) {
        ComPtr<ID3DBlob> errors;
        const HRESULT hr = d3dEntryPoints().compile(shaderSource(), std::strlen(shaderSource()),
                                      nullptr, nullptr, nullptr, entry, target,
                                      0, 0, &out, &errors);
        if (FAILED(hr)) {
            if (errors) {
                diag::error(std::string("D3DCompile: ") +
                            static_cast<const char*>(errors->GetBufferPointer()));
            }
            return false;
        }
        return true;
    }

    bool createViewportResources() {
        ComPtr<ID3DBlob> vsBlob, psBlob;
        if (!compile("VSMain", "vs_4_0", vsBlob)) return false;
        if (!compile("PSMain", "ps_4_0", psBlob)) return false;

        if (FAILED(device_->CreateVertexShader(vsBlob->GetBufferPointer(),
                                               vsBlob->GetBufferSize(), nullptr,
                                               &vertexShader_))) return false;
        if (FAILED(device_->CreatePixelShader(psBlob->GetBufferPointer(),
                                              psBlob->GetBufferSize(), nullptr,
                                              &pixelShader_))) return false;

        const D3D11_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
             D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
             D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20,
             D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        if (FAILED(device_->CreateInputLayout(elements, 3, vsBlob->GetBufferPointer(),
                                              vsBlob->GetBufferSize(), &layout_))) {
            return false;
        }

        D3D11_BUFFER_DESC cb{};
        cb.ByteWidth = 64;  // eine 4x4-Matrix
        cb.Usage = D3D11_USAGE_DYNAMIC;
        cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device_->CreateBuffer(&cb, nullptr, &constants_))) return false;
        cb.ByteWidth = sizeof(PixelConstants);
        if (FAILED(device_->CreateBuffer(&cb, nullptr, &pixelConstantBuffer_))) return false;

        // Ein weisses Bildpunktfeld als Ersatztextur. Damit braucht der Shader
        // keinen zweiten Zweig fuer Flaechen ohne Textur.
        const unsigned char white[4] = {255, 255, 255, 255};
        D3D11_TEXTURE2D_DESC wd{};
        wd.Width = wd.Height = 1;
        wd.MipLevels = wd.ArraySize = 1;
        wd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        wd.SampleDesc.Count = 1;
        wd.Usage = D3D11_USAGE_IMMUTABLE;
        wd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA wdata{white, 4, 0};
        ComPtr<ID3D11Texture2D> whiteTexture;
        if (FAILED(device_->CreateTexture2D(&wd, &wdata, &whiteTexture))) return false;
        if (FAILED(device_->CreateShaderResourceView(whiteTexture.Get(), nullptr,
                                                     &whiteSrv_))) return false;

        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device_->CreateSamplerState(&sd, &sampler_))) return false;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        if (FAILED(device_->CreateSamplerState(&sd, &clampSampler_))) return false;

        // Die fuenf Ueberblendungen, die JKA-Shader benutzen.
        struct BlendSetup { D3D11_BLEND src, dst; };
        const BlendSetup setups[] = {
            {D3D11_BLEND_ONE, D3D11_BLEND_ZERO},            // Opaque
            {D3D11_BLEND_ONE, D3D11_BLEND_ONE},             // Additive
            {D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA},  // AlphaBlend
            {D3D11_BLEND_DEST_COLOR, D3D11_BLEND_ZERO},     // Modulate
            {D3D11_BLEND_DEST_COLOR, D3D11_BLEND_SRC_COLOR},// Filter
        };
        for (int i = 0; i < 5; ++i) {
            D3D11_BLEND_DESC bd{};
            auto& target = bd.RenderTarget[0];
            target.BlendEnable = (i != 0);
            target.SrcBlend = setups[i].src;
            target.DestBlend = setups[i].dst;
            target.BlendOp = D3D11_BLEND_OP_ADD;
            target.SrcBlendAlpha = D3D11_BLEND_ONE;
            target.DestBlendAlpha = D3D11_BLEND_ZERO;
            target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            if (FAILED(device_->CreateBlendState(&bd, &blendStates_[i]))) return false;
        }

        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        if (FAILED(device_->CreateDepthStencilState(&dsd, &depthWriteState_))) return false;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        if (FAILED(device_->CreateDepthStencilState(&dsd, &depthReadState_))) return false;
        dsd.DepthEnable = FALSE;
        if (FAILED(device_->CreateDepthStencilState(&dsd, &depthOffState_))) return false;

        // Zwei Rasterzustaende statt eines.
        //
        // FrontCounterClockwise = TRUE gleicht Direct3D an OpenGL an: dort ist
        // gegen den Uhrzeigersinn die Voreinstellung fuer die Vorderseite.
        // Ohne diese Angleichung waere in genau einer der beiden
        // Schnittstellen die falsche Haelfte des Raums sichtbar — und man
        // suchte den Fehler in der Geometrie statt im Rasterzustand.
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.FrontCounterClockwise = TRUE;
        rd.DepthClipEnable = TRUE;

        // Vier Zustaende: gefuellt und Drahtgitter, jeweils mit und ohne
        // Aussortierung. Sie vorab anzulegen ist billiger, als sie beim
        // Zeichnen zu erzeugen — ein Rasterzustand je Bild waere eine der
        // Stellen, an denen unerklaerliche Ruckler herkommen.
        for (int wire = 0; wire < 2; ++wire) {
            rd.FillMode = wire ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
            for (int cull = 0; cull < 2; ++cull) {
                rd.CullMode = cull ? D3D11_CULL_BACK : D3D11_CULL_NONE;
                if (FAILED(device_->CreateRasterizerState(
                        &rd, &rasterStates_[wire * 2 + cull]))) {
                    return false;
                }
            }
        }
        applyRasterState();
        return true;
    }

    // Die dynamischen Puffer wachsen bei Bedarf. Sie jedes Bild neu anzulegen
    // waere die haeufigste Ursache fuer Ruckler bei vielen Primitiven.
    // --- Ringpuffer fuer Eckpunkte und Indizes -----------------------------
    //
    // Vorher wurde vor JEDEM Zeichenaufruf der ganze Puffer mit
    // WRITE_DISCARD verworfen. Das zwingt den Treiber, je Aufruf einen neuen
    // Speicherbereich bereitzustellen ("renaming") — bei hundert Aufrufen je
    // Bild hundertmal. Der Weg, den Microsoft fuer dynamische Daten empfiehlt
    // (learn.microsoft.com, "How to: Use dynamic resources"), ist ein Ring:
    // hinten anhaengen mit WRITE_NO_OVERWRITE — die Grafikkarte darf weiter
    // aus dem vorderen Teil lesen — und nur beim Umlauf einmal verwerfen.
    //
    // Gibt die Stelle im Puffer zurueck (in Elementen), -1 bei Fehler.
    template <typename T>
    static bool ensureRing(ID3D11Device* device, ComPtr<ID3D11Buffer>& buffer, UINT& capacity,
                           UINT needed, UINT bind) {
        if (needed <= capacity && buffer) return true;
        buffer.Reset();
        // Mindestens 1 MB, und reichlich Luft: ein Ring, der zu klein ist,
        // laeuft jedes Bild mehrmals um und verliert seinen Vorteil.
        capacity = std::max<UINT>(needed * 2, 1u << 20);
        capacity -= capacity % static_cast<UINT>(sizeof(T));
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = capacity;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = bind;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(device->CreateBuffer(&bd, nullptr, &buffer));
    }

    template <typename T>
    long long appendRing(ComPtr<ID3D11Buffer>& buffer, UINT& capacity, UINT& used,
                         const T* data, int count, UINT bind) {
        const UINT needed = static_cast<UINT>(count) * static_cast<UINT>(sizeof(T));
        const UINT before = capacity;
        if (!ensureRing<T>(device_.Get(), buffer, capacity, needed, bind)) return -1;
        D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
        if (capacity != before || used + needed > capacity) {
            mode = D3D11_MAP_WRITE_DISCARD;  // neuer Puffer oder Umlauf
            used = 0;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(buffer.Get(), 0, mode, 0, &mapped))) return -1;
        std::memcpy(static_cast<unsigned char*>(mapped.pData) + used, data, needed);
        context_->Unmap(buffer.Get(), 0);
        const long long at = used / static_cast<UINT>(sizeof(T));
        used += needed;
        return at;
    }

    bool uploadVertices(const Vertex* data, int count) {
        vertexBase_ = appendRing(vertexBuffer_, vertexCapacity_, vertexUsed_, data, count,
                                 D3D11_BIND_VERTEX_BUFFER);
        return vertexBase_ >= 0;
    }

    bool uploadIndices(const unsigned short* data, int count) {
        indexBase_ = appendRing(indexBuffer_, indexCapacity_, indexUsed_, data, count,
                                D3D11_BIND_INDEX_BUFFER);
        return indexBase_ >= 0;
    }

    void createBackBufferView() {
        ComPtr<ID3D11Texture2D> backBuffer;
        if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
            device_->CreateRenderTargetView(backBuffer.Get(), nullptr,
                                            &backBufferView_);
        }
    }

    HWND hwnd_ = nullptr;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> backBufferView_;
    Probe probe_;
    bool concurrentCreates_ = false;
    bool backendReady_ = false;

    // Vorschau
    ComPtr<ID3D11Texture2D> viewTexture_;
    ComPtr<ID3D11RenderTargetView> viewRtv_;
    ComPtr<ID3D11ShaderResourceView> viewSrv_;
    ComPtr<ID3D11DepthStencilView> viewDsv_;
    int viewWidth_ = 0;
    int viewHeight_ = 0;

    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11InputLayout> layout_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11Buffer> vertexBuffer_;
    ComPtr<ID3D11Buffer> indexBuffer_;
    UINT vertexCapacity_ = 0;
    UINT indexCapacity_ = 0;
    UINT vertexUsed_ = 0;
    UINT indexUsed_ = 0;
    long long vertexBase_ = 0;
    long long indexBase_ = 0;
    ComPtr<ID3D11ShaderResourceView> whiteSrv_;
    ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11SamplerState> clampSampler_;
    std::unordered_set<ID3D11ShaderResourceView*> clampTextures_;
    ComPtr<ID3D11BlendState> blendStates_[5];
    std::map<int, ComPtr<ID3D11BlendState>> factorBlends_;
    ComPtr<ID3D11DepthStencilState> depthWriteState_;
    ComPtr<ID3D11DepthStencilState> depthReadState_;
    ComPtr<ID3D11DepthStencilState> depthOffState_;
    bool depthTest_ = true;
    bool depthWrite_ = true;

    // Konstanten des Bildpunktshaders (b1), genau in der Anordnung des HLSL-
    // cbuffer: vier int, dann je 16 float4.
    struct PixelConstants {
        int alphaTest = 0;
        int lightCount = 0;
        int pad0 = 0;
        int pad1 = 0;
        float lightPos[kMaxLights][4]{};
        float lightColour[kMaxLights][4]{};
    };
    static_assert(sizeof(PixelConstants) % 16 == 0, "cbuffer muss ein Vielfaches von 16 Byte sein");
    PixelConstants pixelConstants_;
    ComPtr<ID3D11Buffer> pixelConstantBuffer_;
    ComPtr<ID3D11RasterizerState> rasterStates_[4];
    Cull cull_ = Cull::None;
    Fill fill_ = Fill::Solid;

    void applyRasterState() {
        const int index = (fill_ == Fill::Wireframe ? 2 : 0) +
                          (cull_ == Cull::BackFaces ? 1 : 0);
        if (rasterStates_[index]) context_->RSSetState(rasterStates_[index].Get());
    }
};

}  // namespace

std::unique_ptr<Renderer> createD3D11(void* windowHandle, Probe& probe) {
    probe.backend = Backend::Direct3D11;
    auto renderer = std::make_unique<D3D11Renderer>();
    if (!renderer->create(static_cast<HWND>(windowHandle), probe)) return nullptr;
    return renderer;
}

}  // namespace efx::render
