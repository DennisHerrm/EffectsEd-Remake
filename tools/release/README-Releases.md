# EffectsEd-Remake – Downloads

Fertige Programme von **EffectsEd-Remake**, einem Nachbau von Ravens Effekteditor
EffectsEd (Jedi Academy SDK) für die `.efx`-Dateien aus Jedi Academy, Jedi Outcast
und Movie Duels. Aufbau und Bedienung wie das Original, die Vorschau rechnet wie
die Engine (OpenJK als Vorlage).

## Installieren

1. Unter [Releases](https://github.com/DennisHerrm/EffectsEd-Remake-Releases/releases/latest)
   das neueste `EffectsEd-Remake-revNN.zip` herunterladen.
2. Entpacken und `EffectsEd-Remake/efxed.exe` starten (Windows 10/11, 64 Bit).
3. Unter **Edit → Set Game Path…** den `base`-Ordner des Spiels eintragen
   (z. B. `…/Jedi Academy/GameData/base`). Für Mods wie Movie Duels den
   Mod-Ordner vorn und `base` als weiteren Ordner. Wer eine `.efx` direkt aus
   einem `base`-Ordner öffnet, bekommt den Pfad automatisch.

Effekte, Shader und Bilder liest das Programm direkt aus den `.pk3`-Archiven
des Spiels; es bringt selbst keine Spieldaten mit.

## Updates

Das Programm sieht beim Start selbst hier nach, ob es eine neuere Fassung gibt,
und zeigt dann unten in der Statuszeile einen grünen Hinweis. Ein Klick lädt und
installiert sie (**Help → Check for Updates…** sucht von Hand). Ein GitHub-Konto
ist dafür nicht nötig. Abschalten: **Help → Check for Updates at Startup**.

## „Der Computer wurde durch Windows geschützt“

Das Programm ist nicht digital signiert (ein Zertifikat dafür kostet jährlich
Geld). Windows SmartScreen warnt deshalb beim ersten Start einer neuen Fassung:
**Weitere Informationen → Trotzdem ausführen**. Die `.exe` ist nicht gepackt
oder verschleiert und verlangt keine Administratorrechte. Ihre Einstellungen
legt sie in `%APPDATA%\efxed` ab (oder neben die `.exe`, wenn dort eine leere
Datei `efxed_portable.txt` liegt).

Dieses Repository enthält nur die fertigen Programme, keinen Quelltext.
Ein Fan-Projekt – nicht verbunden mit Raven Software, Activision oder Lucasfilm.
