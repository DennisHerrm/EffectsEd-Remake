// Kurvenauswertung: wie sich ein Wert über die Lebensdauer verändert.
//
// Das ist das Stück, auf dem die ganze Vorschau steht — Größe, Farbe, Alpha,
// Länge laufen alle darüber. Die Formeln stehen Zeile für Zeile in
// `code/cgame/FxPrimitives.cpp` (`CParticle::UpdateSize` und die
// gleichgebauten `UpdateRGB`, `UpdateAlpha`) und in `FxUtil.cpp`, wo der
// Parameter vor dem Abspielen umgerechnet wird.
//
// Zwei Dinge daran sind überraschend genug, dass man sie kennen muss:
//
// **Ohne Flag bleibt der Wert auf `start`.** `perc1` beginnt bei 1.0 und wird
// nur verändert, wenn ein Flag gesetzt ist. Am Ende steht
// `start*perc1 + end*(1-perc1)` — ohne Flag also durchgehend `start`. Wer
// `end` setzt und sich wundert, dass nichts passiert, hat `linear` vergessen.
//
// **`parm` ist keine Zahl zwischen 0 und 1.** Vor dem Abspielen wird sie
// umgerechnet, und zwar je nach Kurvenart verschieden:
//
//     wave:              parm * PI * 0.001          (eine Frequenz)
//     nonlinear, clamp:  parm * 0.01 * life + jetzt (ein Zeitpunkt)
//
// Bei `wave` ist das Ergebnis eine Kreisfrequenz, bei den anderen beiden ein
// absoluter Zeitpunkt in Millisekunden. Dieselbe Zahl in der Datei bedeutet
// also zweierlei.
#pragma once

#include <cstdint>

namespace efx::curve {

// Die Flags, wie sie in der Datei stehen. Die Bitwerte sind die des
// Größenkanals; die anderen Kanäle benutzen dieselbe Reihenfolge auf anderen
// Bits, deshalb rechnet der Auswerter mit dieser normierten Form.
enum Flags : uint32_t {
    kLinear    = 1u << 0,
    kNonLinear = 1u << 1,
    kWave      = 1u << 2,
    kClamp     = kNonLinear | kWave,  // dieselben zwei Bits zusammen
    kRandom    = 1u << 3,
};

// Ein Wert über die Zeit.
struct Curve {
    float start = 0.0f;
    float end = 0.0f;
    float parm = 0.0f;   // wie in der Datei, noch nicht umgerechnet
    uint32_t flags = 0;
};

// Rechnet `parm` in das um, womit die Engine arbeitet.
//
// startMs ist der Zeitpunkt, zu dem die Primitive erscheint, lifeMs ihre
// Lebensdauer. Bei `wave` gehen beide nicht ein.
float resolveParm(const Curve& curve, float startMs, float lifeMs);

// Der Anteil, mit dem `start` gewichtet wird: 1 heißt ganz `start`, 0 heißt
// ganz `end`. `resolvedParm` kommt aus resolveParm.
//
// randomValue ist die Zufallszahl für `random` — hereingereicht statt intern
// gezogen, damit sich das Ergebnis prüfen lässt.
float bias(const Curve& curve, float nowMs, float startMs, float endMs,
           float resolvedParm, float randomValue = 1.0f);

// Der Wert selbst.
float evaluate(const Curve& curve, float nowMs, float startMs, float endMs,
               float resolvedParm, float randomValue = 1.0f);

}  // namespace efx::curve
