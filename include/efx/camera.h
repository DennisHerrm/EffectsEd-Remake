// Die Kamera der Vorschau.
//
// Bedienung wie im Original — Ravens Anleitung beschreibt sie so:
//
//     linke Taste           drehen um den Blickpunkt
//     rechte Taste          heran- und wegfahren
//     Alt + linke Taste     verschieben
//     Z + linke Taste       rollen
//
// Eine Umlaufkamera: sie schaut immer auf einen Zielpunkt und bewegt sich auf
// einer Kugel darum. Das passt zum Zweck — man betrachtet einen Effekt, der an
// einer Stelle steht, statt durch eine Welt zu laufen.
//
// Die Rechnungen stehen hier und nicht im Renderer, damit sie ohne Fenster
// geprüft werden können. Beide Grafikschnittstellen bekommen dieselben
// Matrizen.
#pragma once

#include <array>

namespace efx::camera {

// Spaltenweise abgelegt, wie es OpenGL erwartet. Direct3D bekommt beim
// Übergeben die transponierte Form — das ist der einzige Unterschied zwischen
// den beiden, und er steht an genau einer Stelle.
using Matrix = std::array<float, 16>;

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& v, float s);
float dot(const Vec3& a, const Vec3& b);
Vec3 cross(const Vec3& a, const Vec3& b);
float length(const Vec3& v);
Vec3 normalise(const Vec3& v);

Matrix identity();
Matrix multiply(const Matrix& a, const Matrix& b);
Vec3 transformPoint(const Matrix& m, const Vec3& p);

// Ein Strahl von der Kamera durch einen Punkt im Bild.
//
// Der Weg vom Bildschirm in die Welt. Ihn gab es bisher nicht — man konnte
// nichts im Bild anfassen, weil sich zu einem Mauszeiger keine Weltkoordinate
// finden ließ.
struct Ray {
    Vec3 origin;
    Vec3 direction;  // normiert
};

// x und y sind Bildpunkte innerhalb der Ansicht, gezählt von oben links.
//
// Gerechnet wird über die Umkehrung von Blick- und Projektionsmatrix, nicht
// über die Kamerawinkel. Das ist genauer und gilt auch dann noch, wenn die
// Kamera später anders gesteuert wird — die Matrizen sind die Wahrheit, die
// Winkel nur ein Weg dorthin.
Ray rayThroughPixel(const Matrix& view, const Matrix& projection, float x,
                    float y, float viewWidth, float viewHeight);

// Wo trifft der Strahl die waagerechte Ebene auf Höhe `z`?
//
// Gibt false zurück, wenn er parallel dazu läuft oder von ihr wegzeigt — dann
// gibt es keinen sinnvollen Treffer, und ein Aufrufer, der das ignoriert,
// setzt Dinge ins Unendliche.
bool intersectGroundPlane(const Ray& ray, float z, Vec3& out);

// Der kürzeste Abstand eines Punktes zum Strahl. Für das Anfassen: was nah
// genug an der Mauslinie liegt, gilt als getroffen.
float distanceToRay(const Ray& ray, const Vec3& point);

// Die Umkehrung einer Matrix, die aus Drehung und Verschiebung besteht —
// mehr braucht eine Blickmatrix nicht.
Matrix invertRigid(const Matrix& m);


class Orbit {
public:
    // JKA rechnet in Z-oben, wie Quake. Das behalten wir bei — sonst müsste
    // man jede Zahl aus einer .efx-Datei umrechnen, und spätestens beim
    // Vergleich mit dem Spiel stimmt dann nichts mehr.
    void reset(float worldScale);

    // Feste Blickrichtungen.
    //
    // Die Namen sagen, WOHIN die Kamera blickt, nicht wo sie steht:
    // „Vorne" schaut in Richtung der grünen +Y-Achse, „Oben" senkrecht auf
    // den Boden hinunter.
    //
    // Der alte Editor hat das nicht — es ist eine Zugabe, kein Nachbau.
    // Abstand und Blickziel bleiben unverändert, damit ein Umschalten nicht
    // aus der Szene springt.
    enum class View { Front, Back, Left, Right, Top, Bottom };
    void lookFrom(View view);

    // Maus. Die Werte sind Verschiebungen in Bildpunkten.
    void orbit(float dx, float dy);
    void dolly(float dy);
    void pan(float dx, float dy);
    void roll(float dx);
    void zoomWheel(float steps);

    // Sichtfeld in Grad, Seitenverhältnis aus dem Ansichtsfenster.
    Matrix viewMatrix() const;
    // zeroToOneDepth: Direct3D bildet die Tiefe auf 0 bis 1 ab, OpenGL auf
    // -1 bis 1. Gibt man eine OpenGL-Projektion unveraendert an Direct3D,
    // wird die vordere Haelfte des Tiefenbereichs weggeklippt.
    Matrix projectionMatrix(float aspect, bool zeroToOneDepth = false) const;

    Vec3 position() const;
    const Vec3& target() const { return target_; }
    float distance() const { return distance_; }
    float fieldOfView() const { return fov_; }

    // Den Abstand direkt setzen.
    //
    // Für die Kacheln des Effektbrowsers: dort ist bekannt, wie weit der
    // Effekt reicht, und daraus lässt sich der nötige Abstand **ausrechnen**
    // statt über Zoomstufen zu schätzen. Der erste Anlauf tat Letzteres, und
    // die Effekte waren mal zu klein, mal abgeschnitten.
    void setDistance(float value);

    // Wie weit muss die Kamera weg, damit etwas mit Radius `radius` ganz ins
    // Bild passt? `margin` lässt Luft am Rand (1.0 = randvoll).
    float distanceToFit(float radius, float margin = 1.25f) const;
    float fovDegrees() const { return fov_; }

    // Für die Erschütterung: sie verschiebt Position und Blickrichtung, ohne
    // die eigentliche Kamera zu verändern.
    void setShakeOffset(const Vec3& origin, float pitch, float yaw);

private:
    // Vorgaben = Grundstellung des Originals bei 10 Einheiten je Fuss (siehe
    // reset). Vorher standen hier andere Werte als in reset(), und weil das
    // Programm beim Start nicht zuruecksetzt, sah die erste Ansicht anders
    // aus als nach "Reset View".
    Vec3 target_;
    float distance_ = 80.0f;
    float yaw_ = 270.0f;    // Grad, um die Hochachse (wo die Kamera STEHT)
    float pitch_ = 0.0f;    // Grad, über der Waagerechten
    float roll_ = 0.0f;     // Grad, um die Blickachse
    float fov_ = 90.0f;     // senkrecht
    float worldScale_ = 10.0f;

    Vec3 shakeOrigin_;
    float shakePitch_ = 0.0f;
    float shakeYaw_ = 0.0f;
};

// Die Erschütterung des Spiels, Zeile für Zeile aus cg_camera.cpp (SP) und
// cg_view.c (MP) — beide rechnen identisch.
//
//   intensityScale = 1 - verstrichen/dauer * ((FOV+FOV2)/2/90)
//   intensität     = stärke * intensityScale
//   Ursprung += zufall(-1..1) * intensität   je Achse
//   Winkel   += zufall(-1..1) * intensität   nur Nicken und Gieren, kein Rollen
//
// Das ist der Fehler, den du gemeldet hast: der alte Editor hat das gar nicht
// erst eingebaut, obwohl die Engine es kann.
class Shake {
public:
    static constexpr float kMaxIntensity = 16.0f;  // MAX_SHAKE_INTENSITY

    // Löst eine Erschütterung aus. Abstand und Radius entscheiden, wie stark
    // sie ankommt — außerhalb des Radius passiert gar nichts.
    void trigger(float intensity, int radius, int durationMs, float distance);

    // Weiterrechnen. now ist die Zeit in Millisekunden.
    void update(int nowMs, float fovDegrees);

    bool active() const { return durationMs_ > 0; }
    const Vec3& originOffset() const { return originOffset_; }
    float pitchOffset() const { return pitchOffset_; }
    float yawOffset() const { return yawOffset_; }
    float currentIntensity() const { return current_; }

    void stop();

    // Für Tests: setzt den Zufallsgeber auf einen bekannten Wert.
    void seed(unsigned value) { rngState_ = value ? value : 1u; }

private:
    float random(float low, float high);

    float intensity_ = 0.0f;
    int startMs_ = 0;
    int durationMs_ = 0;
    float current_ = 0.0f;

    Vec3 originOffset_;
    float pitchOffset_ = 0.0f;
    float yawOffset_ = 0.0f;

    unsigned rngState_ = 1u;
};

}  // namespace efx::camera
