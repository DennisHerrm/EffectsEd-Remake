// Bilder laden.
//
// JKA liest drei Formate — `R_ImageLoader_Add` in `tr_image_load.cpp` meldet
// `jpg`, `png` und `tga` an. Für Effekttexturen ist TGA das häufigste, weil
// es einen Alphakanal ohne Verluste trägt; JPEG kommt bei großen, undurchsich-
// tigen Flächen vor.
//
// Ergebnis ist immer RGBA mit acht Bit je Kanal, egal was in der Datei stand.
// Der Renderer bekommt damit genau ein Format statt fünf Sonderfälle.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace efx::image {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;  // width*height*4, oben links beginnend
    bool ok = false;
    std::string error;

    bool hasAlpha = false;  // ob die Datei überhaupt einen Alphakanal hatte
};

// Erkennt das Format am Inhalt, nicht an der Endung.
//
// Das ist kein Übereifer: in JKA-Mods liegen regelmäßig JPEG-Dateien mit der
// Endung `.tga`, weil jemand sie umbenannt statt umgewandelt hat. Die Engine
// stolpert darüber, ein Editor sollte es besser können — und wenigstens sagen,
// was wirklich drinsteht.
Image decode(const unsigned char* data, size_t size);

// Groesser kann kein Bild sein, das wir dekodieren: hoechstens 16384 je Seite
// und 64 Millionen Bildpunkte. Fuer alle drei Formate gleich — vorher galt die
// Bildpunktgrenze nur fuer TGA, und ein 82-Byte-JPEG mit 16384x16384 belegte
// 2 GB.
bool implausibleSize(int width, int height);

// Targa. Unterstützt die Fälle, die in JKA vorkommen: 24 und 32 Bit,
// unkomprimiert und lauflängenkodiert, mit und ohne gedrehte Zeilenrichtung.
Image decodeTga(const unsigned char* data, size_t size);

// JPEG, Baseline (sequenziell, Huffman-kodiert).
//
// Selbst geschrieben, wie der Auspacker — das Projekt hat bis hierhin keine
// einzige Abhängigkeit, und ein JPEG-Decoder lässt sich gegen fremde
// Referenzdaten genauso sauber prüfen wie Deflate.
//
// **Nicht unterstützt: progressive JPEGs.** Sie kommen in Spieldaten praktisch
// nicht vor (der Vorteil ist ein früher Vorschau-Aufbau beim Laden über eine
// langsame Leitung, was für eine Textur sinnlos ist), brauchen aber einen
// komplett anderen Dekodierweg. Eine solche Datei wird abgewiesen und beim
// Namen genannt, statt Müll zu liefern.
Image decodeJpeg(const unsigned char* data, size_t size);

// PNG. Nutzt den vorhandenen Deflate-Auspacker.
//
// JKA-Effekte benutzen praktisch nur TGA und JPG — PNG kam erst mit OpenJK
// dazu. Es ist trotzdem drin, weil es fast nichts kostet: der Auspacker war
// schon für die `.pk3`-Dateien nötig, und was bleibt, sind die Zeilenfilter.
Image decodePng(const unsigned char* data, size_t size);

// Ein Bild als Targa schreiben — für Bildschirmfotos.
//
// Targa und nicht PNG, obwohl der Auspacker da ist: **Packen** ist etwas
// anderes als Auspacken, und einen Deflate-Packer zu schreiben, nur um ein
// Bildschirmfoto kleiner zu machen, wäre unverhältnismäßig. Quake und seine
// Nachfolger schreiben ihre Bildschirmfotos ebenfalls als Targa.
std::vector<unsigned char> encodeTga(const unsigned char* rgba, int width,
                                     int height);

// Welches Format steckt drin? Für Meldungen.
enum class Format { Unknown, Tga, Png, Jpeg };
Format sniff(const unsigned char* data, size_t size);
const char* formatName(Format format);

}  // namespace efx::image
