#!/usr/bin/env bash
# Den Kern unter dem Fadenwaechter bauen und laufen lassen.
#
# Warum eigens: das Projekt heisst „mit Multithreading" im Titel, und der
# Arbeitsverteiler traegt inzwischen den Effektbrowser, das Durchsuchen der
# Spielpfade und das Laden der Texturen. Ein Wettlauf darin faellt beim
# gewoehnlichen Prueflauf NICHT auf — er kommt einmal unter tausend Laeufen,
# und dann als Absturz an einer voellig anderen Stelle.
#
# ThreadSanitizer sieht ihn dagegen schon beim ersten Mal: er beobachtet jeden
# Zugriff und meldet, wenn zwei Faeden dieselbe Stelle anfassen, ohne sich
# abzusprechen. Auch dann, wenn es zufaellig gutgegangen ist.
#
# Nicht im Prueflauf enthalten, sondern eigens aufzurufen: der Bau dauert
# mehrere Minuten und die Ausfuehrung ist rund zehnmal langsamer. Vor einem
# Paket lohnt es sich, nach jeder Zeile nicht.
#
#   bash tools/check_tsan.sh
#
# Ein Hinweis zur Deutung: die Tests erzwingen ausdruecklich vier und acht
# Arbeitsfaeden. Ohne das saehe der Waechter auf einem Rechner mit einem Kern
# gar nichts — der Verteiler legt dort naemlich keine Faeden an, und ein Lauf
# ohne Faeden beweist nichts.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BIN="${TMPDIR:-/tmp}/efxtests_tsan"

echo "== Kern unter ThreadSanitizer uebersetzen =="
echo "   (dauert ein paar Minuten)"
g++ -std=c++20 -O1 -g -fsanitize=thread -pthread \
    -Iinclude -isystem third_party \
    src/*.cpp tests/tests.cpp -o "$BIN"

echo
echo "== Lauf =="
# halt_on_error=0: alle Meldungen sammeln statt beim ersten aufzuhoeren.
# Eine einzelne sagt selten genug ueber die Ursache.
TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" "$BIN" data

echo
echo "Kein 'WARNING: ThreadSanitizer' oben? Dann ist der Kern rennfrei."
echo "Das heisst NICHT, dass die Oberflaeche es ist — gui/ laesst sich hier"
echo "nicht uebersetzen, und der Waechter sieht nur, was auch laeuft."
