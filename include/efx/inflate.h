// Deflate auspacken (RFC 1951).
//
// Der Schlüssel zu allem Weiteren: die Texturen, Modelle und Klänge einer
// JKA-Installation liegen in `.pk3`-Dateien, und deren Inhalte sind fast immer
// deflate-komprimiert. Ohne Auspacker bleibt der Materialsuchlauf bei den
// Namen stehen.
//
// Selbst geschrieben statt zlib eingebunden: es sind etwa dreihundert Zeilen,
// das Format ist seit 1996 unverändert, und es hält die Abhängigkeitsliste des
// Programms bei null. Der Bau soll auf einem frischen Rechner ohne
// Vorbereitung durchlaufen.
//
// Ausschließlich lesend, und gegen böswillige Eingaben abgesichert: eine
// `.pk3` liegt im Spielordner, und da kommt alles Mögliche her. Jede Grenze
// wird geprüft, und die erwartete Größe ist eine harte Obergrenze — ein
// Archiv, das behauptet, eine Datei sei 3 GB groß, bekommt kein Gigabyte
// Speicher.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace efx::inflate {

struct Result {
    std::vector<unsigned char> data;
    bool ok = false;
    std::string error;
};

// Rohes Deflate ohne zlib-Kopf — so liegt es in einer Zip-Datei.
//
// expectedSize kommt aus dem Zip-Verzeichnis und dient als Obergrenze.
Result raw(const unsigned char* data, size_t size, size_t expectedSize);

}  // namespace efx::inflate
