#include "efx/camera.h"

#include <algorithm>
#include <cmath>

namespace efx::camera {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

float clampf(float v, float low, float high) {
    return v < low ? low : (v > high ? high : v);
}

}  // namespace

Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(const Vec3& v, float s) { return {v.x * s, v.y * s, v.z * s}; }
float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float length(const Vec3& v) { return std::sqrt(dot(v, v)); }

Vec3 normalise(const Vec3& v) {
    const float len = length(v);
    return len > 1e-8f ? v * (1.0f / len) : Vec3{};
}

Matrix identity() {
    Matrix m{};
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    return m;
}

Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix out{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[col * 4 + k];
            out[col * 4 + row] = sum;
        }
    }
    return out;
}

Vec3 transformPoint(const Matrix& m, const Vec3& p) {
    return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
            m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
            m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}

// ---------------------------------------------------------------------------

void Orbit::setDistance(float value) {
    // Untergrenze: bei null steckt die Kamera im Zielpunkt, und die
    // Blickmatrix wird unbrauchbar.
    constexpr float kNearest = 1.0f;
    constexpr float kFarthest = 1.0e6f;
    distance_ = value < kNearest ? kNearest : (value > kFarthest ? kFarthest : value);
}

float Orbit::distanceToFit(float radius, float margin) const {
    if (!(radius > 0.0f)) radius = 1.0f;
    // Der halbe Bildwinkel spannt bei Abstand d eine halbe Bildhoehe von
    // d * tan(fov/2) auf. Damit `radius` hineinpasst:
    //
    //     d = radius / tan(fov/2)
    //
    // Das ist die ganze Rechnung — kein Schaetzen ueber Zoomstufen.
    const float halfAngle = std::tan(fov_ * 0.5f * kDegToRad);
    if (!(halfAngle > 0.0001f)) return distance_;
    return radius / halfAngle * (margin > 0.0f ? margin : 1.0f);
}

void Orbit::reset(float worldScale) {
    worldScale_ = worldScale > 0.0f ? worldScale : 16.0f;
    target_ = Vec3{};
    // Grundstellung wie im Original (gemessen, agentA ROOM-GEOMETRY.md): Auge
    // bei (0, -80, 0) fuer 10 Einheiten je Fuss, Blick entlang +Y genau auf
    // den Ursprung, kein Kippen, 90 Grad senkrechtes Sichtfeld. Bei einem
    // anderen Massstab waechst der Abstand mit (16 Einheiten: 128).
    //
    // Vorher: schraeg von oben (Gieren 225, Nicken 15), zehn Fuss, 60 Grad —
    // ein ganz anderes Bild als im Original.
    distance_ = 8.0f * worldScale_;
    yaw_ = 270.0f;  // Auge auf -Y (yaw beschreibt, wo die Kamera STEHT)
    pitch_ = 0.0f;
    roll_ = 0.0f;
    fov_ = 90.0f;
    shakeOrigin_ = Vec3{};
    shakePitch_ = shakeYaw_ = 0.0f;
}

void Orbit::lookFrom(View view) {
    // Ausgerechnet aus der Richtungsformel weiter unten:
    //
    //     forward = (cos p · cos y,  cos p · sin y,  sin p)
    //
    // Umgekehrt aufgeloest ergibt das fuer jede Achsenrichtung genau ein Paar.
    // `pitch` ist bei den senkrechten Ansichten 89 statt 90: bei genau 90 ist
    // die Aufwaertsrichtung unbestimmt, das Kreuzprodukt wird null und das
    // Bild kippt. Dieselbe Grenze wie beim Ziehen mit der Maus.
    // ACHTUNG: `yaw_` und `pitch_` beschreiben, wo die Kamera STEHT, nicht
    // wohin sie blickt. `position()` rechnet
    //
    //     Auge = Ziel + (cos p·cos y, cos p·sin y, sin p) · Abstand
    //
    // also PLUS, nicht minus. Die Blickrichtung ist demnach die Gegenrichtung.
    //
    // Ich hatte zuerst die Winkel der Blickrichtung eingesetzt — alle sechs
    // Ansichten zeigten damit genau verkehrt herum. Erst das Nachmessen an
    // `position()` hat es geklaert, nicht das Nachdenken.
    //
    // Also: um nach +Y zu BLICKEN, muss die Kamera bei -Y STEHEN, das heisst
    // yaw = 270.
    switch (view) {
        case View::Front:  yaw_ = 270.0f; pitch_ =   0.0f; break;  // Blick +Y
        case View::Back:   yaw_ =  90.0f; pitch_ =   0.0f; break;  // Blick -Y
        case View::Left:   yaw_ = 180.0f; pitch_ =   0.0f; break;  // Blick +X
        case View::Right:  yaw_ =   0.0f; pitch_ =   0.0f; break;  // Blick -X
        case View::Top:    yaw_ =   0.0f; pitch_ =  89.0f; break;  // hinunter
        case View::Bottom: yaw_ =   0.0f; pitch_ = -89.0f; break;  // hinauf
    }
    roll_ = 0.0f;
}

void Orbit::orbit(float dx, float dy) {
    // Die Kamera folgt der Maus, sie laeuft ihr nicht entgegen.
    //
    // Beide Achsen waren verkehrt herum: nach links ziehen liess die Kamera
    // nach rechts blicken, nach oben ziehen nach unten. Im alten Editor ist es
    // umgekehrt, und das ist auch die verbreitete Erwartung — man greift die
    // SZENE und dreht sie, statt einen Kamerakopf zu schwenken.
    //
    // Wichtig fuer die Vorzeichen: `yaw_` und `pitch_` sagen, wo die Kamera
    // STEHT, nicht wohin sie blickt (`position()` addiert den Versatz). Zieht
    // man die Szene nach rechts, muss die Kamera also nach LINKS wandern —
    // und um den Blick zu heben, muss sie SINKEN.
    yaw_ -= dx * 0.5f;  // 0.5 Grad je Bildpunkt wie im Original (gemessen)
    // Nicken begrenzen, aber nicht ganz bis zum Pol: genau senkrecht von oben
    // ist die Aufwaertsrichtung unbestimmt und das Bild kippt schlagartig.
    pitch_ = clampf(pitch_ - dy * 0.5f, -89.0f, 89.0f);

    while (yaw_ >= 360.0f) yaw_ -= 360.0f;
    while (yaw_ < 0.0f) yaw_ += 360.0f;
}

void Orbit::dolly(float dy) {
    // Verhaeltnismaessig, nicht in festen Schritten: aus der Ferne will man
    // grosse Spruenge, aus der Naehe kleine. Ein fester Betrag macht das eine
    // zaeh und das andere unbrauchbar.
    distance_ *= std::exp(dy * 0.01f);
    distance_ = clampf(distance_, 0.25f * worldScale_, 400.0f * worldScale_);
}

void Orbit::zoomWheel(float steps) { dolly(-steps * 12.0f); }

void Orbit::pan(float dx, float dy) {
    // Am Abstand ausgerichtet, damit sich das Schieben aus jeder Entfernung
    // gleich anfuehlt.
    const float speed = distance_ * 0.0015f;

    const float yawRad = yaw_ * kDegToRad;
    const float pitchRad = pitch_ * kDegToRad;

    const Vec3 forward{std::cos(pitchRad) * std::cos(yawRad),
                       std::cos(pitchRad) * std::sin(yawRad),
                       std::sin(pitchRad)};
    const Vec3 worldUp{0.0f, 0.0f, 1.0f};
    const Vec3 right = normalise(cross(forward, worldUp));
    const Vec3 up = cross(right, forward);

    target_ = target_ + right * (-dx * speed) + up * (dy * speed);
}

void Orbit::roll(float dx) {
    roll_ += dx * 0.3f;
    while (roll_ >= 360.0f) roll_ -= 360.0f;
    while (roll_ < 0.0f) roll_ += 360.0f;
}

Vec3 Orbit::position() const {
    const float yawRad = (yaw_ + shakeYaw_) * kDegToRad;
    const float pitchRad = clampf(pitch_ + shakePitch_, -89.9f, 89.9f) * kDegToRad;

    const Vec3 offset{std::cos(pitchRad) * std::cos(yawRad) * distance_,
                      std::cos(pitchRad) * std::sin(yawRad) * distance_,
                      std::sin(pitchRad) * distance_};
    return target_ + offset + shakeOrigin_;
}

void Orbit::setShakeOffset(const Vec3& origin, float pitch, float yaw) {
    shakeOrigin_ = origin;
    shakePitch_ = pitch;
    shakeYaw_ = yaw;
}

Matrix Orbit::viewMatrix() const {
    const Vec3 eye = position();
    const Vec3 forward = normalise(target_ + shakeOrigin_ - eye);
    Vec3 worldUp{0.0f, 0.0f, 1.0f};

    Vec3 right = cross(forward, worldUp);
    if (length(right) < 1e-5f) {
        // Genau von oben oder unten: die Aufwaertsrichtung ist unbestimmt.
        // Statt eines Sprungs eine feste Ersatzachse nehmen.
        worldUp = Vec3{0.0f, 1.0f, 0.0f};
        right = cross(forward, worldUp);
    }
    right = normalise(right);
    Vec3 up = cross(right, forward);

    if (roll_ != 0.0f) {
        const float r = roll_ * kDegToRad;
        const float c = std::cos(r);
        const float s = std::sin(r);
        const Vec3 rotatedRight = right * c + up * s;
        const Vec3 rotatedUp = up * c - right * s;
        right = rotatedRight;
        up = rotatedUp;
    }

    Matrix m = identity();
    m[0] = right.x;   m[4] = right.y;   m[8]  = right.z;
    m[1] = up.x;      m[5] = up.y;      m[9]  = up.z;
    m[2] = -forward.x; m[6] = -forward.y; m[10] = -forward.z;
    m[12] = -dot(right, eye);
    m[13] = -dot(up, eye);
    m[14] = dot(forward, eye);
    return m;
}

Matrix Orbit::projectionMatrix(float aspect, bool zeroToOneDepth) const {
    if (aspect <= 0.0f) aspect = 1.0f;

    // Nahe Ebene am Weltmassstab ausrichten: bei 16 Einheiten je Fuss ist ein
    // Zoll etwa 1.3 Einheiten, und naeher will niemand heran. Eine zu kleine
    // nahe Ebene verschenkt Tiefengenauigkeit und laesst weit entfernte
    // Flaechen flackern.
    const float nearPlane = 0.05f * worldScale_;
    const float farPlane = 600.0f * worldScale_;

    const float f = 1.0f / std::tan(fov_ * 0.5f * kDegToRad);
    Matrix m{};
    m[0] = f / aspect;
    m[5] = f;
    m[11] = -1.0f;
    if (zeroToOneDepth) {
        // Direct3D: nahe Ebene auf 0, ferne auf 1.
        m[10] = farPlane / (nearPlane - farPlane);
        m[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
    } else {
        // OpenGL: nahe Ebene auf -1, ferne auf 1.
        m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
        m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    }
    return m;
}

// ---------------------------------------------------------------------------

void Shake::trigger(float intensity, int radius, int durationMs, float distance) {
    if (radius <= 0 || durationMs <= 0) return;
    if (distance > static_cast<float>(radius)) return;  // ausserhalb: nichts

    // CG_ExplosionEffects: die Staerke faellt linear mit dem Abstand.
    const float scale = 1.0f - distance / static_cast<float>(radius);
    float value = intensity * scale;
    if (value > kMaxIntensity) value = kMaxIntensity;
    if (value <= 0.0f) return;

    intensity_ = value;
    durationMs_ = durationMs;
    startMs_ = -1;  // wird beim ersten update gesetzt
}

void Shake::stop() {
    durationMs_ = 0;
    intensity_ = 0.0f;
    current_ = 0.0f;
    originOffset_ = Vec3{};
    pitchOffset_ = yawOffset_ = 0.0f;
}

float Shake::random(float low, float high) {
    // Kleiner eigener Zufallsgeber statt std::rand: der ist zwischen
    // Laufzeitbibliotheken verschieden, und ein Test soll auf jedem System
    // dasselbe ergeben.
    rngState_ = rngState_ * 1664525u + 1013904223u;
    const float unit = static_cast<float>((rngState_ >> 8) & 0xFFFFFF) /
                       static_cast<float>(0x1000000);
    return low + (high - low) * unit;
}

void Shake::update(int nowMs, float fovDegrees) {
    if (durationMs_ <= 0) {
        current_ = 0.0f;
        originOffset_ = Vec3{};
        pitchOffset_ = yawOffset_ = 0.0f;
        return;
    }
    if (startMs_ < 0) startMs_ = nowMs;

    const int elapsed = nowMs - startMs_;
    if (elapsed >= durationMs_) {
        stop();
        return;
    }

    // CGCam_UpdateShake. Das Sichtfeld geht ein, weil eine Erschütterung bei
    // engem Blickwinkel staerker wirkt als bei weitem.
    const float progress = static_cast<float>(elapsed) / static_cast<float>(durationMs_);
    const float scale = 1.0f - progress * (fovDegrees / 90.0f);
    current_ = intensity_ * scale;
    if (current_ < 0.0f) current_ = 0.0f;

    originOffset_ = Vec3{random(-1.0f, 1.0f) * current_,
                         random(-1.0f, 1.0f) * current_,
                         random(-1.0f, 1.0f) * current_};
    // Nur Nicken und Gieren — die Engine laesst Rollen ausdruecklich aus.
    pitchOffset_ = random(-1.0f, 1.0f) * current_;
    yawOffset_ = random(-1.0f, 1.0f) * current_;
}

}  // namespace efx::camera

namespace efx::camera {

Matrix invertRigid(const Matrix& m) {
    // Eine Blickmatrix besteht aus Drehung und Verschiebung. Die Umkehrung
    // davon ist die transponierte Drehung und die zurueckgedrehte
    // Verschiebung — keine allgemeine Matrixinversion noetig.
    //
    // Spaltenweise abgelegt: m[0..2] ist die erste Spalte, m[12..14] die
    // Verschiebung.
    Matrix out = identity();
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out[col * 4 + row] = m[row * 4 + col];   // transponiert
        }
    }
    const Vec3 translation{m[12], m[13], m[14]};
    out[12] = -(out[0] * translation.x + out[4] * translation.y +
                out[8] * translation.z);
    out[13] = -(out[1] * translation.x + out[5] * translation.y +
                out[9] * translation.z);
    out[14] = -(out[2] * translation.x + out[6] * translation.y +
                out[10] * translation.z);
    return out;
}

Ray rayThroughPixel(const Matrix& view, const Matrix& projection, float x, float y,
                    float viewWidth, float viewHeight) {
    Ray ray;
    ray.direction = {0.0f, 0.0f, -1.0f};
    if (viewWidth <= 0.0f || viewHeight <= 0.0f) return ray;

    // Bildpunkte -> Anteil -> genormte Gerätekoordinaten (-1..1).
    // Y wird gedreht: Bildpunkte zaehlen von oben, die Gerätekoordinaten von
    // unten.
    const float ndcX = (x / viewWidth) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (y / viewHeight) * 2.0f;

    // Aus der Projektion die Blickrichtung im Kameraraum. Die beiden Werte
    // auf der Hauptdiagonale sind der Kehrwert der halben Bildwinkel.
    const float tanX = std::fabs(projection[0]) > 1e-9f ? 1.0f / projection[0] : 1.0f;
    const float tanY = std::fabs(projection[5]) > 1e-9f ? 1.0f / projection[5] : 1.0f;
    const Vec3 inCamera{ndcX * tanX, ndcY * tanY, -1.0f};

    // In die Welt drehen. Die Blickmatrix bildet Welt -> Kamera ab, also
    // wird ihre Umkehrung gebraucht.
    const Matrix inverse = invertRigid(view);
    const Vec3 origin{inverse[12], inverse[13], inverse[14]};
    const Vec3 direction{
        inverse[0] * inCamera.x + inverse[4] * inCamera.y + inverse[8] * inCamera.z,
        inverse[1] * inCamera.x + inverse[5] * inCamera.y + inverse[9] * inCamera.z,
        inverse[2] * inCamera.x + inverse[6] * inCamera.y + inverse[10] * inCamera.z};

    ray.origin = origin;
    ray.direction = normalise(direction);
    return ray;
}

bool intersectGroundPlane(const Ray& ray, float z, Vec3& out) {
    // Laeuft der Strahl fast parallel zur Ebene, gibt es keinen brauchbaren
    // Treffer — die Rechnung liefert dann einen Punkt in vielen Kilometern
    // Entfernung, und ein Aufrufer, der das ignoriert, setzt Dinge ins
    // Unendliche.
    if (std::fabs(ray.direction.z) < 1e-4f) return false;

    const float distance = (z - ray.origin.z) / ray.direction.z;
    if (distance <= 0.0f) return false;   // hinter der Kamera

    out = ray.origin + ray.direction * distance;
    return true;
}

float distanceToRay(const Ray& ray, const Vec3& point) {
    const Vec3 toPoint = point - ray.origin;
    const float along = dot(toPoint, ray.direction);
    // Hinter der Kamera zaehlt der Abstand zum Ursprung, sonst faende man
    // Dinge im Ruecken.
    if (along < 0.0f) return length(toPoint);
    const Vec3 nearest = ray.origin + ray.direction * along;
    return length(point - nearest);
}

}  // namespace efx::camera
