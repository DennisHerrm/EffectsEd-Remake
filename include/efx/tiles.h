// Wer bekommt welchen Platz im Vorschaublatt.
//
// Klingt nach Buchhaltung und ist der Grund für einen Fehler, den man von
// außen nur als „ich zeige auf A und B fängt an zu laufen" sieht.
//
// Der Effektbrowser zeichnet alle sichtbaren Vorschauen in EIN Blatt und legt
// je Kachel einen Ausschnitt daraus über ihr Rechteck. Damit eine Kachel über
// mehrere Bilder stehenbleiben kann — und genau das ist der Trick, mit dem
// aus 5 Bildern je Sekunde 300 werden —, muss ihr Platz im Blatt derselbe
// bleiben, auch wenn sie beim Rollen im Raster wandert.
//
// Drei Dinge müssen dabei stimmen, und keines davon sieht man beim Lesen:
//
//   1. Jeder Sichtbare bekommt einen Platz. Wer keinen bekommt, zeichnet
//      irgendwo hin — meist auf Platz null, und dann zeigt seine Kachel den
//      Effekt eines anderen.
//   2. Keine zwei teilen sich einen Platz. Sonst überschreiben sie sich
//      gegenseitig, und beide zeigen denselben Effekt.
//   3. Wer sichtbar bleibt, behält seinen Platz. Sonst gilt er als neu, wird
//      neu gezeichnet, und die Ersparnis ist dahin.
//
// Deshalb steht das hier im Kern und nicht in der Oberfläche: mit Zahlen
// statt Zeigern lässt sich jede der drei Regeln prüfen.
#pragma once

#include <cstddef>
#include <vector>

namespace efx::tiles {

// Das Ergebnis für einen Sichtbaren.
struct Placement {
    int cell = -1;
    // Neu auf diesem Platz: sein Inhalt gehört noch dem Vorgänger, er muss
    // also in diesem Bild gezeichnet werden.
    bool fresh = false;
};

// `owner` bildet Platz -> Kennung ab; -1 heißt frei. Wird angepasst.
// `visible` sind die Kennungen der sichtbaren Kacheln, in Anzeigereihenfolge.
//
// Reicht die Zahl der Plätze nicht, wächst `owner` mit. Das ist der Fall, der
// vorher fehlte: es gab eine Obergrenze, und wer darüber lag, bekam gar
// keinen Platz.
std::vector<Placement> assign(std::vector<int>& owner,
                              const std::vector<int>& visible);

// Wie viele Spalten das Blatt bei so vielen Plätzen hat.
//
// Steht hier, weil daran ein Fehler hängt, den man nicht sieht, wenn die
// Rechnung an zwei Stellen steht: der Platz eines Effekts BLEIBT (das ist der
// ganze Zweck), aber wo dieser Platz im Blatt LIEGT, hängt an der Spaltenzahl
// — und die wächst mit der Zahl der Plätze mit.
//
//     30 Plätze -> 6 Spalten -> Platz 7 liegt bei Spalte 1, Zeile 1
//     40 Plätze -> 7 Spalten -> Platz 7 liegt bei Spalte 0, Zeile 1
//     50 Plätze -> 8 Spalten -> Platz 7 liegt bei Spalte 7, Zeile 0
//
// Eine Kachel, die stehenbleibt, liest danach an einer ganz anderen Stelle
// des Blattes — und zeigt das Bild eines fremden Effekts. Genau so sah es
// aus: „ich zeige auf A und B läuft".
//
// Deshalb gilt: ändert sich die Spaltenzahl, MUSS alles neu gezeichnet
// werden. Die Platznummer allein genügt nicht.
size_t sheetColumns(size_t cellCount);

// Die Texturkoordinaten für einen Ausschnitt des Blattes.
//
// `flipped` sagt, ob die Grafikschnittstelle die Textur von unten nach oben
// ablegt — OpenGL tut das, Direct3D nicht.
//
// Diese fünf Zeilen sind die Stelle, an der ein Effekt das Bild eines anderen
// zeigte. Für das GANZE Blatt genügt es, oben und unten zu vertauschen:
//
//     AddImage(tex, ..., ImVec2(0, 1), ImVec2(1, 0));   // richtig
//
// Für einen AUSSCHNITT ist derselbe Griff falsch. Ein Tausch spiegelt den
// Ausschnitt in sich selbst; gebraucht wird die Spiegelung des ganzen
// Blattes, also `1 - y/höhe`. Nachgerechnet an einem echten Fall:
//
//     Blatt 1792 x 1568, Kachel bei y 896..1120 („sparks")
//     getauscht  -> gelesen wird y 448..672   = eine andere Kachel („flamejet")
//     gespiegelt -> gelesen wird y 896..1120  = richtig
//
// Der Fehler zeigt sich NUR unter OpenGL. Unter Direct3D ist `flipped` falsch,
// beide Rechnungen sind dann gleich, und alles sieht in Ordnung aus.
struct UvRect {
    float u0 = 0.0f, v0 = 0.0f;   // oben links
    float u1 = 1.0f, v1 = 1.0f;   // unten rechts
};

UvRect uvFor(int x, int y, int size, int sheetWidth, int sheetHeight,
             bool flipped);

}  // namespace efx::tiles
