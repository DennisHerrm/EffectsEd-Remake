// OpenGL-Umsetzung.
//
// Der Rückfall für Fälle, in denen Direct3D nicht anläuft: Remotedesktop,
// virtuelle Maschinen, Wine, alte Hardware. Der ursprüngliche EffectsEd war
// ohnehin OpenGL.
#include <windows.h>
#include <GL/gl.h>

#include <cstddef>
#include <cstring>

// Die Windows-Header bringen nur OpenGL 1.1 mit. Was darueber hinausgeht, muss
// zur Laufzeit ueber wglGetProcAddress geholt werden — es gibt keine
// Bibliothek, gegen die man linken koennte.
//
// Bewusst von Hand statt mit GLEW oder glad: es sind genau die Funktionen, die
// die Vorschau braucht, und eine Fremdbibliothek fuer zwei Dutzend Zeiger
// waere mehr Abhaengigkeit als Nutzen. Jede wird einzeln geprueft; fehlt eine,
// steht sie namentlich im Protokoll statt als Absturz auf dem Bildschirm.
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STREAM_DRAW                    0x88E0
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_FRAMEBUFFER                    0x8D40
#define GL_RENDERBUFFER                   0x8D41
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_FUNC_ADD                       0x8006
#endif

typedef ptrdiff_t GLsizeiptrARB_;
typedef ptrdiff_t GLintptrARB_;

namespace gl {

#define EFX_GL_FUNCS(X) \
    X(void,   GenBuffers,      (GLsizei, GLuint*)) \
    X(void,   DeleteBuffers,   (GLsizei, const GLuint*)) \
    X(void,   BindBuffer,      (GLenum, GLuint)) \
    X(void,   BufferData,      (GLenum, GLsizeiptrARB_, const void*, GLenum)) \
    X(void,   GenVertexArrays, (GLsizei, GLuint*)) \
    X(void,   DeleteVertexArrays,(GLsizei, const GLuint*)) \
    X(void,   BindVertexArray, (GLuint)) \
    X(void,   EnableVertexAttribArray, (GLuint)) \
    X(void,   VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(GLuint, CreateShader,    (GLenum)) \
    X(void,   ShaderSource,    (GLuint, GLsizei, const char* const*, const GLint*)) \
    X(void,   CompileShader,   (GLuint)) \
    X(void,   GetShaderiv,     (GLuint, GLenum, GLint*)) \
    X(void,   GetShaderInfoLog,(GLuint, GLsizei, GLsizei*, char*)) \
    X(void,   DeleteShader,    (GLuint)) \
    X(GLuint, CreateProgram,   (void)) \
    X(void,   AttachShader,    (GLuint, GLuint)) \
    X(void,   LinkProgram,     (GLuint)) \
    X(void,   GetProgramiv,    (GLuint, GLenum, GLint*)) \
    X(void,   GetProgramInfoLog,(GLuint, GLsizei, GLsizei*, char*)) \
    X(void,   UseProgram,      (GLuint)) \
    X(void,   DeleteProgram,   (GLuint)) \
    X(GLint,  GetUniformLocation,(GLuint, const char*)) \
    X(void,   UniformMatrix4fv,(GLint, GLsizei, GLboolean, const GLfloat*)) \
    X(void,   Uniform1i,       (GLint, GLint)) \
    X(void,   Uniform4fv,      (GLint, GLsizei, const GLfloat*)) \
    X(void,   GenFramebuffers, (GLsizei, GLuint*)) \
    X(void,   DeleteFramebuffers,(GLsizei, const GLuint*)) \
    X(void,   BindFramebuffer, (GLenum, GLuint)) \
    X(void,   GenerateMipmap,  (GLenum)) \
    X(void,   FramebufferTexture2D,(GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(void,   GenRenderbuffers,(GLsizei, GLuint*)) \
    X(void,   DeleteRenderbuffers,(GLsizei, const GLuint*)) \
    X(void,   BindRenderbuffer,(GLenum, GLuint)) \
    X(void,   RenderbufferStorage,(GLenum, GLenum, GLsizei, GLsizei)) \
    X(void,   FramebufferRenderbuffer,(GLenum, GLenum, GLenum, GLuint)) \
    X(GLenum, CheckFramebufferStatus,(GLenum)) \
    X(void,   BlendEquation,   (GLenum)) \
    X(void,   ActiveTexture,   (GLenum))

#define EFX_DECL(ret, name, args) ret (APIENTRY* name) args = nullptr;
EFX_GL_FUNCS(EFX_DECL)
#undef EFX_DECL

// Gibt den Namen der ersten fehlenden Funktion zurueck, sonst nullptr.
const char* load() {
#define EFX_LOAD(ret, name, args) \
    name = reinterpret_cast<ret(APIENTRY*) args>( \
        reinterpret_cast<void (*)()>(wglGetProcAddress("gl" #name))); \
    if (!name) return "gl" #name;
    EFX_GL_FUNCS(EFX_LOAD)
#undef EFX_LOAD
    return nullptr;
}

}  // namespace gl

#include <cstdio>

#include "efx/diag.h"
#include "efx/renderer.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"

namespace efx::render {
namespace {

class GL3Renderer : public Renderer {
public:
    bool create(HWND hwnd, Probe& probe) {
        hwnd_ = hwnd;
        dc_ = GetDC(hwnd);
        if (!dc_) {
            probe.failure = "GetDC returned nothing";
            return false;
        }

        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;

        const int format = ChoosePixelFormat(dc_, &pfd);
        if (format == 0 || !SetPixelFormat(dc_, format, &pfd)) {
            probe.failure = "no suitable pixel format";
            ReleaseDC(hwnd_, dc_);
            dc_ = nullptr;
            return false;
        }

        context_ = wglCreateContext(dc_);
        if (!context_ || !wglMakeCurrent(dc_, context_)) {
            probe.failure = "wglCreateContext failed";
            if (context_) wglDeleteContext(context_);
            context_ = nullptr;
            ReleaseDC(hwnd_, dc_);
            dc_ = nullptr;
            return false;
        }

        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* rendererName =
            reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

        probe.adapter = rendererName ? rendererName : "unknown";
        probe.version = version ? version : "unknown";
        if (vendor) diag::info(std::string("vendor: ") + vendor);

        // Bildsynchronisation. Ohne ausdrueckliches Intervall entscheidet der
        // Treiber — und manche zeichnen dann ungebremst: gemessen 6.4 s
        // Rechenzeit in 5 s bei einem untaetigen, sichtbaren Fenster. Die
        // Erweiterung WGL_EXT_swap_control gibt es auf jedem Treiber seit
        // Windows XP; fehlt sie, bleibt es beim Verhalten des Treibers.
        swapInterval_ = reinterpret_cast<SwapIntervalProc>(
            reinterpret_cast<void (*)()>(wglGetProcAddress("wglSwapIntervalEXT")));

        // Mindestens OpenGL 3.3 verlangen — darunter fehlen die
        // Vertexpuffer-Objekte, auf denen die ImGui-Anbindung aufsetzt.
        int major = 0, minor = 0;
        if (version && std::sscanf(version, "%d.%d", &major, &minor) == 2) {
            if (major < 3 || (major == 3 && minor < 3)) {
                char message[96];
                std::snprintf(message, sizeof(message),
                              "OpenGL %d.%d is too old, 3.3 required",
                              major, minor);
                probe.failure = message;
                destroy();
                return false;
            }
        }

        // Funktionszeiger holen. Fehlt eine, steht ihr Name im Protokoll —
        // und die Auswahllogik faellt sauber auf Direct3D zurueck, statt
        // beim ersten Zeichnen in einen Nullzeiger zu laufen.
        if (const char* missing = gl::load()) {
            probe.failure = std::string("missing OpenGL function: ") + missing;
            destroy();
            return false;
        }
        if (!createViewportResources()) {
            probe.failure = "viewport shader or buffers could not be created";
            destroy();
            return false;
        }

        probe.available = true;
        probe.backend = Backend::OpenGL3;
        probe_ = probe;

        // Kein ImGui_ImplOpenGL3_Init hier — der ImGui-Kontext existiert an
        // dieser Stelle noch nicht. Siehe initImGuiBackend().
        return true;
    }

    ~GL3Renderer() override { destroy(); }

    bool initImGuiBackend() override {
        if (!ImGui::GetCurrentContext()) {
            diag::error("initImGuiBackend called without an ImGui context");
            return false;
        }
        // Die Anbindung laedt die Funktionszeiger selbst.
        if (!ImGui_ImplOpenGL3_Init("#version 130")) {
            diag::error("ImGui_ImplOpenGL3_Init failed");
            return false;
        }
        backendReady_ = true;
        return true;
    }

    void shutdownImGuiBackend() override {
        if (!backendReady_) return;
        ImGui_ImplOpenGL3_Shutdown();
        backendReady_ = false;
    }

    Backend backend() const override { return Backend::OpenGL3; }
    const Probe& probe() const override { return probe_; }

    void newFrame() override { ImGui_ImplOpenGL3_NewFrame(); }

    void clear(float r, float g, float b, float a) override {
        glViewport(0, 0, width_, height_);
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void renderImGui() override {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void present(bool vsync) override {
        const int wanted = vsync ? 1 : 0;
        if (swapInterval_ && wanted != currentInterval_) {
            swapInterval_(wanted);
            currentInterval_ = wanted;
        }
        SwapBuffers(dc_);
    }

    void resizeSwapChain(int width, int height) override {
        width_ = width;
        height_ = height;
    }

    // --- Vorschau ----------------------------------------------------------

    void resizeViewport(int width, int height) override {
        if (width <= 0 || height <= 0) return;
        if (width == viewWidth_ && height == viewHeight_ && fbo_) return;
        viewWidth_ = width;
        viewHeight_ = height;

        if (!fbo_) gl::GenFramebuffers(1, &fbo_);
        if (!viewTexture_) glGenTextures(1, &viewTexture_);
        if (!depthBuffer_) gl::GenRenderbuffers(1, &depthBuffer_);

        glBindTexture(GL_TEXTURE_2D, viewTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gl::BindRenderbuffer(GL_RENDERBUFFER, depthBuffer_);
        gl::RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

        gl::BindFramebuffer(GL_FRAMEBUFFER, fbo_);
        gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                 viewTexture_, 0);
        gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                    GL_RENDERBUFFER, depthBuffer_);
        if (gl::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            diag::error("viewport framebuffer incomplete");
        }
        gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    TextureId viewportTexture() const override {
        return static_cast<TextureId>(viewTexture_);
    }

    // OpenGL legt den Ursprung unten links, ImGui erwartet ihn oben links.
    bool viewportTextureFlipped() const override { return true; }

    void beginViewport(float r, float g, float b, float a) override {
        if (!fbo_) return;
        gl::BindFramebuffer(GL_FRAMEBUFFER, fbo_);
        setViewportRect(0, 0, 0, 0);
        glDisable(GL_SCISSOR_TEST);   // zum Loeschen die ganze Flaeche
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        setCulling(Cull::None);

        gl::UseProgram(program_);
        gl::BindVertexArray(vao_);
        gl::Uniform1i(textureLocation_, 0);
        setBlend(Blend::Opaque);
        setAlphaTest(0);
        setLights(nullptr, 0);
    }

    void beginViewportPreserving() override {
        if (!fbo_) return;
        gl::BindFramebuffer(GL_FRAMEBUFFER, fbo_);
        setViewportRect(0, 0, 0, 0);
        glDisable(GL_SCISSOR_TEST);
        // Nur die Tiefe. Die Farbe der zuletzt gezeichneten Kacheln bleibt
        // stehen — genau darum geht es.
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        setCulling(Cull::None);

        gl::UseProgram(program_);
        gl::BindVertexArray(vao_);
        gl::Uniform1i(textureLocation_, 0);
        setBlend(Blend::Opaque);
        setAlphaTest(0);
        setLights(nullptr, 0);
    }

    void clearViewportRect(float r, float g, float b, float a) override {
        // glClear achtet auf den Ausschnitt — den hat setViewportRect schon
        // gesetzt. Mehr braucht es hier nicht.
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void setViewportRect(int x, int y, int width, int height) override {
        if (width <= 0 || height <= 0) {
            x = 0;
            y = 0;
            width = viewWidth_;
            height = viewHeight_;
            glDisable(GL_SCISSOR_TEST);
        } else {
            glEnable(GL_SCISSOR_TEST);
        }

        // OpenGL zaehlt von UNTEN links, die Oberflaeche von oben. Ohne das
        // Umdrehen landen die Kacheln spiegelverkehrt im Raster — und zwar
        // erst auffallend, wenn das Raster nicht quadratisch ist.
        const int flipped = viewHeight_ - (y + height);
        glViewport(x, flipped, width, height);
        glScissor(x, flipped, width, height);
    }

    void setCamera(const float view[16], const float proj[16]) override {
        // Beide spaltenweise abgelegt, genau wie OpenGL es erwartet — hier ist
        // nichts umzurechnen. Das ist der einzige Unterschied zum
        // Direct3D-Renderer.
        float mvp[16];
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += proj[k * 4 + row] * view[col * 4 + k];
                mvp[col * 4 + row] = sum;
            }
        }
        gl::UniformMatrix4fv(mvpLocation_, 1, GL_FALSE, mvp);
    }

    void setBlend(Blend mode) override {
        struct Setup { GLenum src, dst; };
        static const Setup setups[] = {
            {GL_ONE, GL_ZERO},                    // Opaque
            {GL_ONE, GL_ONE},                     // Additive
            {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA},
            {GL_DST_COLOR, GL_ZERO},              // Modulate
            {GL_DST_COLOR, GL_SRC_COLOR},         // Filter
        };
        const int index = static_cast<int>(mode);
        if (index == 0) {
            glDisable(GL_BLEND);
        } else {
            glEnable(GL_BLEND);
            gl::BlendEquation(GL_FUNC_ADD);
            glBlendFunc(setups[index].src, setups[index].dst);
        }
    }

    void setBlendFactors(BlendFactor src, BlendFactor dst) override {
        // GL_ONE GL_ZERO heisst: keine Mischung (ParseStage tut dasselbe).
        if (src == BlendFactor::One && dst == BlendFactor::Zero) {
            glDisable(GL_BLEND);
            return;
        }
        glEnable(GL_BLEND);
        gl::BlendEquation(GL_FUNC_ADD);
        glBlendFunc(factor(src), factor(dst));
    }

    void setDepthTest(bool enabled) override {
        if (enabled) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
    }

    void setAlphaTest(int mode) override { gl::Uniform1i(alphaTestLocation_, mode); }

    void setLights(const Light* lights, int count) override {
        if (count > kMaxLights) count = kMaxLights;
        if (!lights || count < 0) count = 0;
        float positions[kMaxLights * 4] = {};
        float colours[kMaxLights * 4] = {};
        for (int i = 0; i < count; ++i) {
            for (int k = 0; k < 3; ++k) {
                positions[i * 4 + k] = lights[i].pos[k];
                colours[i * 4 + k] = lights[i].rgb[k];
            }
            positions[i * 4 + 3] = lights[i].radius;
        }
        gl::Uniform1i(lightCountLocation_, count);
        if (count > 0) {
            gl::Uniform4fv(lightPosLocation_, count, positions);
            gl::Uniform4fv(lightColourLocation_, count, colours);
        }
    }

    void setCulling(Cull mode) override {
        if (mode == Cull::None) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            // Gegen den Uhrzeigersinn ist die Vorderseite. Bei OpenGL ist das
            // die Voreinstellung; wir setzen es trotzdem, damit hier steht,
            // worauf sich der Direct3D-Renderer eingestellt hat.
            glFrontFace(GL_CCW);
        }
    }

    void setFill(Fill mode) override {
        // GL_LINE gilt fuer beide Seiten; bei Direct3D ist die Fuellart Teil
        // des Rasterzustands. Beide zeichnen damit dasselbe.
        glPolygonMode(GL_FRONT_AND_BACK, mode == Fill::Wireframe ? GL_LINE : GL_FILL);
    }

    bool wantsZeroToOneDepth() const override { return false; }

    void setDepthWrite(bool enabled) override {
        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
    }

    void drawTriangles(const Vertex* vertices, int vertexCount,
                       const unsigned short* indices, int indexCount,
                       TextureId texture) override {
        if (vertexCount <= 0 || indexCount <= 0) return;
        bindVertices(vertices, vertexCount);
        gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
        gl::BufferData(GL_ELEMENT_ARRAY_BUFFER,
                       static_cast<GLsizeiptrARB_>(indexCount * sizeof(unsigned short)),
                       indices, GL_STREAM_DRAW);

        gl::ActiveTexture(0x84C0 /* GL_TEXTURE0 */);
        glBindTexture(GL_TEXTURE_2D,
                      texture == kNoTexture ? whiteTexture_
                                            : static_cast<GLuint>(texture));
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, nullptr);
    }

    void drawLines(const Vertex* vertices, int vertexCount, float width) override {
        if (vertexCount <= 0) return;
        bindVertices(vertices, vertexCount);
        gl::ActiveTexture(0x84C0);
        glBindTexture(GL_TEXTURE_2D, whiteTexture_);
        // OpenGL kann Linienbreiten, Direct3D nicht. Damit beide gleich
        // aussehen, bleibt es bei einer Einheit.
        glLineWidth(width > 0.0f ? 1.0f : 1.0f);
        glDrawArrays(GL_LINES, 0, vertexCount);
    }

    void endViewport() override {
        // Fuellart zuruecksetzen, sonst zeichnet ImGui seine Flaechen als
        // Drahtgitter — der Zustand gehoert dem Kontext, nicht uns.
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        gl::BindVertexArray(0);
        gl::UseProgram(0);
        gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    bool readViewport(std::vector<unsigned char>& rgba, int& width,
                      int& height) override {
        if (viewWidth_ <= 0 || viewHeight_ <= 0) return false;
        width = viewWidth_;
        height = viewHeight_;
        rgba.assign(static_cast<size_t>(width) * height * 4, 0);

        // gl:: davor. Windows' opengl32 bietet nur OpenGL 1.1 — alles
        // Neuere wird oben ueber wglGetProcAddress nachgeladen und liegt im
        // Namensraum gl. Ohne das Praefix sucht der Uebersetzer eine
        // Systemfunktion, die es dort nicht gibt.
        gl::BindFramebuffer(GL_FRAMEBUFFER, fbo_);
        // Vier Byte je Bildpunkt heisst: keine Zeilenauffuellung. Ohne diese
        // Zeile richtet sich OpenGL nach der Voreinstellung von vier Byte,
        // was hier passt — aber bei einer Breite, die nicht durch vier
        // teilbar ist, kaeme das Bild schraeg heraus.
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        gl::BindFramebuffer(GL_FRAMEBUFFER, 0);

        // OpenGL beginnt unten links, ein Bild oben links. Zeilen tauschen.
        const size_t rowBytes = static_cast<size_t>(width) * 4;
        std::vector<unsigned char> row(rowBytes);
        for (int y = 0; y < height / 2; ++y) {
            unsigned char* upper = rgba.data() + static_cast<size_t>(y) * rowBytes;
            unsigned char* lower =
                rgba.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
            std::memcpy(row.data(), upper, rowBytes);
            std::memcpy(upper, lower, rowBytes);
            std::memcpy(lower, row.data(), rowBytes);
        }
        // Der Puffer hat keinen sinnvollen Alphakanal — voll deckend setzen,
        // sonst ist das Bildschirmfoto durchsichtig.
        for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
        return true;
    }

    bool readBackbuffer(std::vector<unsigned char>& rgba, int& width,
                        int& height) override {
        GLint view[4] = {};
        glGetIntegerv(GL_VIEWPORT, view);
        width = view[2];
        height = view[3];
        if (width <= 0 || height <= 0) return false;
        rgba.assign(static_cast<size_t>(width) * height * 4, 0);
        gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        const size_t rowBytes = static_cast<size_t>(width) * 4;
        std::vector<unsigned char> row(rowBytes);
        for (int y = 0; y < height / 2; ++y) {
            unsigned char* upper = rgba.data() + static_cast<size_t>(y) * rowBytes;
            unsigned char* lower = rgba.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
            std::memcpy(row.data(), upper, rowBytes);
            std::memcpy(upper, lower, rowBytes);
            std::memcpy(lower, row.data(), rowBytes);
        }
        for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
        return true;
    }

    TextureId createTexture(const unsigned char* rgba, int width, int height,
                            bool clamp, bool mipmaps) override {
        if (!rgba || width <= 0 || height <= 0) return kNoTexture;
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, rgba);
        // Mipmaps wie in der Engine (siehe renderer_d3d11.cpp).
        if (mipmaps) gl::GenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        const GLint wrap = clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        return static_cast<TextureId>(texture);
    }

    void destroyTexture(TextureId texture) override {
        if (texture == kNoTexture) return;
        const GLuint name = static_cast<GLuint>(texture);
        glDeleteTextures(1, &name);
    }

private:
    static GLenum factor(BlendFactor f) {
        switch (f) {
            case BlendFactor::Zero: return GL_ZERO;
            case BlendFactor::One: return GL_ONE;
            case BlendFactor::SrcColor: return GL_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
            case BlendFactor::DstColor: return GL_DST_COLOR;
            case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
            case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::DstAlpha: return GL_DST_ALPHA;
            case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
            case BlendFactor::SrcAlphaSaturate: return GL_SRC_ALPHA_SATURATE;
        }
        return GL_ONE;
    }

    void bindVertices(const Vertex* data, int count) {
        gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
        gl::BufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptrARB_>(count * sizeof(Vertex)), data,
                       GL_STREAM_DRAW);
    }

    GLuint compileShader(GLenum type, const char* source) {
        const GLuint shader = gl::CreateShader(type);
        gl::ShaderSource(shader, 1, &source, nullptr);
        gl::CompileShader(shader);
        GLint ok = 0;
        gl::GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024]{};
            gl::GetShaderInfoLog(shader, sizeof(log), nullptr, log);
            diag::error(std::string("shader compile: ") + log);
            gl::DeleteShader(shader);
            return 0;
        }
        return shader;
    }

    bool createViewportResources() {
        // Dasselbe Shaderpaar wie im Direct3D-Renderer, nur in GLSL: Farbe aus
        // dem Eckpunkt mal Textur. Weichen die beiden voneinander ab, sieht
        // derselbe Effekt je nach Schnittstelle anders aus — und dann sucht
        // man den Fehler in der Effektdatei.
        const char* vertexSource =
            "#version 330 core\n"
            "layout(location=0) in vec3 aPos;\n"
            "layout(location=1) in vec2 aUV;\n"
            "layout(location=2) in vec4 aCol;\n"
            "uniform mat4 uMVP;\n"
            "out vec2 vUV;\n"
            "out vec4 vCol;\n"
            "out vec3 vWorld;\n"
            "void main() {\n"
            "    vUV = aUV;\n"
            "    vCol = aCol;\n"
            "    vWorld = aPos;\n"
            "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
            "}\n";
        // Lichter und Alphatest: siehe Light und setAlphaTest in renderer.h.
        // Gleich im Direct3D-Renderer.
        const char* fragmentSource =
            "#version 330 core\n"
            "in vec2 vUV;\n"
            "in vec4 vCol;\n"
            "in vec3 vWorld;\n"
            "uniform sampler2D uTexture;\n"
            "uniform int uAlphaTest;\n"
            "uniform int uLightCount;\n"
            "uniform vec4 uLightPos[16];\n"
            "uniform vec4 uLightColour[16];\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "    vec4 c = vCol * texture(uTexture, vUV);\n"
            "    if (uAlphaTest == 1 && c.a <= 0.0) discard;\n"
            "    if (uAlphaTest == 2 && c.a >= 0.5) discard;\n"
            "    if (uAlphaTest == 3 && c.a < 0.5) discard;\n"
            "    if (uAlphaTest == 4 && c.a < 0.75) discard;\n"
            "    vec3 lit = vec3(0.0);\n"
            "    if (uLightCount > 0) {\n"
            "        vec3 n = abs(cross(dFdx(vWorld), dFdy(vWorld)));\n"
            "        int best = (n.z >= n.x && n.z >= n.y) ? 2 : (n.y >= n.x ? 1 : 0);\n"
            "        for (int i = 0; i < uLightCount; ++i) {\n"
            "            float r = uLightPos[i].w;\n"
            "            if (r <= 0.0) continue;\n"
            "            vec3 dist = uLightPos[i].xyz - vWorld;\n"
            "            float dAbs = abs(best == 2 ? dist.z : (best == 1 ? dist.y : dist.x));\n"
            "            float dUse = clamp(r * 0.5 / max(dAbs, 0.0001), 0.1, best == 2 ? 1.4 : 1.5);\n"
            "            float scale = 1.0 / (r * dUse);\n"
            "            vec2 tc = 0.5 + (best == 2 ? dist.xy : (best == 1 ? dist.xz : dist.yz)) * scale;\n"
            "            float modulate = dAbs > r ? 0.0 : (dAbs < r * 0.5 ? 1.0 : 2.0 * (r - dAbs) * scale);\n"
            "            float t = length(clamp(tc, 0.0, 1.0) - 0.5);\n"
            "            float profile = t <= 0.117 ? 1.0 : 0.5 + 0.5 * cos(3.14159265 * min(1.0, (t - 0.117) / 0.383));\n"
            "            lit += min(uLightColour[i].rgb * modulate, vec3(1.0)) * profile;\n"
            "        }\n"
            "    }\n"
            // Flaeche x (1 + Licht), wie der D3D-Shader.
            "    c.rgb += c.rgb * lit;\n"
            "    fragColor = c;\n"
            "}\n";

        const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (!vertexShader || !fragmentShader) return false;

        program_ = gl::CreateProgram();
        gl::AttachShader(program_, vertexShader);
        gl::AttachShader(program_, fragmentShader);
        gl::LinkProgram(program_);
        gl::DeleteShader(vertexShader);
        gl::DeleteShader(fragmentShader);

        GLint linked = 0;
        gl::GetProgramiv(program_, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[1024]{};
            gl::GetProgramInfoLog(program_, sizeof(log), nullptr, log);
            diag::error(std::string("shader link: ") + log);
            return false;
        }

        mvpLocation_ = gl::GetUniformLocation(program_, "uMVP");
        textureLocation_ = gl::GetUniformLocation(program_, "uTexture");
        alphaTestLocation_ = gl::GetUniformLocation(program_, "uAlphaTest");
        lightCountLocation_ = gl::GetUniformLocation(program_, "uLightCount");
        lightPosLocation_ = gl::GetUniformLocation(program_, "uLightPos");
        lightColourLocation_ = gl::GetUniformLocation(program_, "uLightColour");

        gl::GenVertexArrays(1, &vao_);
        gl::GenBuffers(1, &vbo_);
        gl::GenBuffers(1, &ibo_);

        gl::BindVertexArray(vao_);
        gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
        gl::EnableVertexAttribArray(0);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                reinterpret_cast<const void*>(offsetof(Vertex, pos)));
        gl::EnableVertexAttribArray(1);
        gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                reinterpret_cast<const void*>(offsetof(Vertex, uv)));
        gl::EnableVertexAttribArray(2);
        gl::VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                                reinterpret_cast<const void*>(offsetof(Vertex, color)));
        gl::BindVertexArray(0);

        const unsigned char white[4] = {255, 255, 255, 255};
        whiteTexture_ = static_cast<GLuint>(createTexture(white, 1, 1, true, false));
        return whiteTexture_ != 0;
    }

    void destroy() {
        if (context_) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(context_);
            context_ = nullptr;
        }
        if (dc_) {
            ReleaseDC(hwnd_, dc_);
            dc_ = nullptr;
        }
    }

    HWND hwnd_ = nullptr;
    HDC dc_ = nullptr;
    HGLRC context_ = nullptr;
    using SwapIntervalProc = BOOL(WINAPI*)(int);
    SwapIntervalProc swapInterval_ = nullptr;
    int currentInterval_ = -1;
    int width_ = 1280;
    int height_ = 860;
    Probe probe_;
    bool backendReady_ = false;

    // Vorschau
    GLuint fbo_ = 0;
    GLuint viewTexture_ = 0;
    GLuint depthBuffer_ = 0;
    int viewWidth_ = 0;
    int viewHeight_ = 0;

    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ibo_ = 0;
    GLint mvpLocation_ = -1;
    GLint textureLocation_ = -1;
    GLint alphaTestLocation_ = -1;
    GLint lightCountLocation_ = -1;
    GLint lightPosLocation_ = -1;
    GLint lightColourLocation_ = -1;
    GLuint whiteTexture_ = 0;
};

}  // namespace

std::unique_ptr<Renderer> createGL3(void* windowHandle, Probe& probe) {
    probe.backend = Backend::OpenGL3;
    auto renderer = std::make_unique<GL3Renderer>();
    if (!renderer->create(static_cast<HWND>(windowHandle), probe)) return nullptr;
    return renderer;
}

// Die gemeinsame Weiche. Steht hier, weil src/renderer.cpp ohne Grafikheader
// auskommen soll — nur so liess sich die Auswahllogik ohne Grafikkarte pruefen.
std::unique_ptr<Renderer> createD3D11(void* windowHandle, Probe& probe);

std::unique_ptr<Renderer> createRenderer(Backend backend, void* windowHandle,
                                         Probe& probe) {
    probe = Probe{};
    probe.backend = backend;
    switch (backend) {
        case Backend::Direct3D11: return createD3D11(windowHandle, probe);
        case Backend::OpenGL3: return createGL3(windowHandle, probe);
    }
    probe.failure = "unknown graphics backend";
    return nullptr;
}

}  // namespace efx::render
