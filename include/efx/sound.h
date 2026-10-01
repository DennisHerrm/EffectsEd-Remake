// Klänge lesen.
//
// JKA spielt `.wav` und `.mp3`. Der Fehler, den du am Original gefunden hast,
// liegt genau hier: EffectsEd importiert nur `PlaySoundA`, und das kann kein
// MP3 — deshalb bleibt jeder Effekt mit einem MP3 im Editor stumm, obwohl er
// im Spiel klingt.
//
// Ergebnis ist immer 16-Bit-PCM, egal was in der Datei stand. Die Ausgabe
// bekommt damit ein Format statt fünf Sonderfälle.
//
// **WAV lesen wir selbst** — das Format ist ein paar Dutzend Zeilen und lässt
// sich vollständig prüfen.
//
// **Für MP3 liegt `third_party/minimp3.h` bei.** Das ist die erste fremde
// Datei im Baum, und die Entscheidung fiel nicht leicht: bis hierhin hatte das
// Projekt keine einzige Abhängigkeit, und Deflate und JPEG habe ich selbst
// geschrieben.
//
// Ein MP3-Decoder ist aber eine andere Größenordnung — Huffman, IMDCT,
// Polyphasen-Synthesebank, Bit-Reservoir. Rund zweitausend Zeilen, bei denen
// ein Fehler nicht als falsche Farbe auffällt, sondern als Rauschen, das
// vielleicht nur bei bestimmten Bitraten auftritt. minimp3 ist gemeinfrei
// (CC0, keine Auflagen), eine einzige Datei ohne eigene Abhängigkeiten, und
// ISO-konform geprüft.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace efx::sound {

struct Sound {
    int sampleRate = 0;
    int channels = 0;
    std::vector<int16_t> samples;  // verschränkt: L R L R …
    bool ok = false;
    std::string error;

    // Dauer in Millisekunden. Für die Vorschau: ein Klang, der länger ist als
    // der Effekt, soll nicht mitten im nächsten Durchlauf weiterlaufen.
    float durationMs() const;
    size_t frameCount() const;
};

enum class Format { Unknown, Wav, Mp3 };
Format sniff(const unsigned char* data, size_t size);

// Erkennt das Format am Inhalt, nicht an der Endung — wie bei den Bildern.
Sound decode(const unsigned char* data, size_t size);

// PCM in 8, 16, 24 und 32 Bit sowie 32-Bit-Gleitkomma. Alles andere
// (komprimierte WAVs wie ADPCM) wird abgewiesen und beim Namen genannt.
Sound decodeWav(const unsigned char* data, size_t size);

Sound decodeMp3(const unsigned char* data, size_t size);

// Einen Klang auf das Format des Tongeräts bringen: Abtastrate und Kanäle.
//
// Warum das nötig ist — der Fall aus dem Betrieb: eine MP3 mit 44100 Hz wurde
// abgespielt, eine WAV mit 11025 Hz nicht. Beide wurden fehlerfrei gelesen,
// beide hatten Werte, die Auslösung stimmte bei beiden.
//
// Der Unterschied lag im Tongerät. Wir haben es mit der *genauen* Rate der
// Datei geöffnet, also mal mit 44100, mal mit 11025. Windows mischt heute
// intern mit einer festen Rate; `waveOut` ist nur noch nachgebildet, und
// ungewöhnliche Raten lehnt es je nach Treiber schlicht ab. Bei einer
// 44100er-Datei fällt das nie auf, weil es die Rate des Geräts ist.
//
// Ein Tongerät, das jedes Mal neu geöffnet wird, ist ohnehin die falsche
// Bauform: zwei Klänge mit verschiedenen Raten hätten sich gegenseitig
// abgewürgt. Ein festes Format, in das alles umgerechnet wird, löst beides.
//
// Lineare Zwischenwerte. Für einen Effekteditor genügt das: die Klänge sind
// kurze Aufschläge, keine Musik, und der Unterschied zu einem aufwendigen
// Filter ist an einem Explosionsgeräusch nicht hörbar.
std::vector<int16_t> convert(const std::vector<int16_t>& samples,
                             int sourceRate, int sourceChannels,
                             int targetRate, int targetChannels);

}  // namespace efx::sound
