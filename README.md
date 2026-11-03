# Kabeltester mit ESP32-S3

Lokales ESP-IDF-Projekt für einen Kabeltester mit acht Anschlüssen je Kabelseite.
Der aktuelle Code prüft das Profil eines aktiven Auftrags. Ein entprellter
Wechsel des externen Kippschalters startet eine Messung. Grün bedeutet OK, Rot
bedeutet FEHLER. Nach dem Abziehen endet die dauerhafte Ergebnisanzeige, sobald
zwei Prüfungen keine Verbindung mehr erkennen. Danach zeigen die LEDs den Zustand.

Die Webspalte **Gemessenes Profil** zeigt die tatsächlichen Verbindungen als
Array. **Zusatzdaten** enthält die bisherigen Angaben und `gemessen` mit
demselben Array. Auch der Meldeempfänger bekommt diese Daten und bei FEHLER
zusätzlich die Abweichungen; siehe API-Entwurf für die Diagnosefelder.

## Unterlagen

- [Projektstand und lokale Abgabeübersicht](docs/Projektstand.md) – Einstieg und offene Punkte
- [Ursprüngliche Netzwerkaufgabe](docs/Aufgabe-Netzwerk.md)
- [Pinbelegung](docs/Pinbelegung.md)
- [Schaltskizze](docs/Schaltskizze.svg) – im Browser öffnen
- [Vorhandene Pinplanung](docs/Pinplanung.png)
- [Neues Profil anlegen](docs/Neues-Profil.md)
- [Test und Abnahme](docs/Abnahme.md)
- [Abschluss-Checkliste für die restlichen praktischen Tests](docs/Abschluss-Checkliste.md)
- [Automatische Softwaretests und ihre Nachweise](docs/Softwaretests.md)
- [GitHub-Uebergabe und Zugangsdatenpruefung](docs/GitHub-Uebergabe.md)
- [API-Entwurf mit JSON- und curl-Beispielen](docs/API-Entwurf.md)
- [Bedienungsanleitung – Vorlage für BookStack](docs/Bedienungsanleitung.md)
- [Historischer Abnahmetest für Projekt 1](docs/Abnahme-Teil1.md)

Alle Unterlagen liegen lokal in diesem Repo. Die Bedienungsanleitung ist
für BookStack vorbereitet; die Veröffentlichung und der Kapitel-Link stehen
noch aus. `auftrag-test.json` ist für acht gerade Adern, `auftrag-abnahme.json`
für die vorgeschriebenen drei Vieradermessungen. Die Testdateien legen erst
nach einem bewusst ausgeführten POST einen Auftrag auf dem Gerät an.

## Bauen und übertragen

In VS Code diesen Ordner öffnen. Über die Befehlspalette ein
**ESP-IDF Terminal** öffnen; ein gewöhnliches Terminal mit Python-venv reicht
ohne aktivierte ESP-IDF-Umgebung nicht aus.

Zuerst die Konfigurationsvorlage kopieren und die Platzhalter in der lokalen
`config/flash_config.h` ersetzen. Diese Datei bleibt durch `.gitignore` ausgeschlossen.
`REPORT_URL` kann leer bleiben, solange kein Ergebnisempfänger eingerichtet ist.

```powershell
Copy-Item config/flash_config.example.h config/flash_config.h
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

COM5 durch den tatsächlichen Board-Port ersetzen. `set-target` ist bei der
ersten Einrichtung oder einem Zielwechsel erforderlich und setzt die
Projektkonfiguration zurück. Den Monitor mit `Ctrl+]` beenden.
Die ESP-IDF-Erweiterung muss mit der eigenen lokalen Installation eingerichtet werden.

## Aktueller Stand

`main/main.c` ist die Quelle für GPIO-Zuordnung, Standardprofile und API.
Das verwendete Profil kommt aus dem Auftrag; beide Kabelrichtungen sind erlaubt.
Die Firmware verwendet GPIO11 und externe LEDs an GPIO9/10. Das unterscheidet
sich von der ursprünglichen Aufgabenbeschreibung mit BOOT/GPIO0 und Onboard-LED;
diese Abweichung muss bei der Abnahme abgestimmt sein.

Die IP-Adresse erscheint im Monitor; die Webbedienung ist unter
`http://<IP-Adresse>/` erreichbar. mDNS macht sie im gleichen lokalen Netzwerk
auch unter `http://<DEVICE_HOSTNAME>.local/` erreichbar, bei Hostname
`kabeltester` also unter `http://kabeltester.local/`. Der HTTP-Dienst wird auf
Port 80 bekannt gemacht. Zum Test nach dem Flashen auf die WLAN-Verbindung warten
und die Namensadresse im Browser öffnen. Der Zugriff über die IP bleibt erhalten.
Die Seite liegt in `main/index.html` und wird
beim Build in die Firmware eingebettet. Sie bietet Standardprofile und eigene
Profile, Anzahl und optionale JSON-Zusatzdaten, Anlegen und Abbrechen eines
Auftrags sowie Fortschritt und die letzten 20 Ergebnisse mit Meldestatus.
Für ein gerades Achtaderkabel `gerade8` wählen; `gerade4` erwartet nur vier Adern.
Nach dem Anlegen über die Webseite die Messung am externen Schalter starten.
Vor jeder weiteren Messung das Kabel abziehen und warten, bis die Ergebnis-LED
nicht mehr dauerhaft leuchtet.
Ohne aktiven Auftrag wird nicht gemessen.

Die API bietet `GET /api/profiles`, `POST /api/jobs`, `GET /api/jobs/current`,
`DELETE /api/jobs/current`, `GET /api/runs` und `GET /api/result`.
Ergebnisse werden im Hintergrund einmal pro Lauf an `REPORT_URL` gesendet.
Ein nicht erreichbarer Empfänger blockiert die Messung nicht.
Aufträge und Ergebnisse liegen im RAM und gehen beim Neustart verloren.
Die selbst angelegte WLAN-Konfiguration liegt in
`config/flash_config.h` und wird von Git ausgeschlossen; die Vorlage ist
`config/flash_config.example.h`. Die Webbedienung muss nach dem
Flashen am Board geprüft werden.

## LED-Anzeigen

| Zustand | Anzeige |
|---|---|
| Kein WLAN | Rot blinkt langsam (etwa 1 Hz). |
| WLAN da, kein Auftrag | Grün blitzt kurz alle 3 Sekunden. |
| Auftrag aktiv | Grün blinkt langsam (etwa 1 Hz). |
| Ergebnis OK | Grün leuchtet bis zum Abziehen. |
| Ergebnis FEHLER | Rot leuchtet bis zum Abziehen. |
| Schalter ohne Auftrag | Rot blinkt schnell (etwa 5 Hz) für 3 Sekunden bei bestehendem WLAN. |

Ein angezeigtes Messergebnis hat Vorrang, auch bei fehlendem WLAN.
Ohne Ergebnis hat die WLAN-Warnung Vorrang vor den übrigen Zuständen.
Nach dem Abziehen wechselt die Anzeige wieder zum aktuellen Zustand.
Das letzte Ergebnis bleibt trotzdem auf der Webseite und in der Laufliste erhalten.

## Git bei der Abnahme zeigen

```powershell
git status
git log --oneline -5
git ls-files
```

Damit werden Arbeitsstand, gespeicherte Commits und versionierte Dateien sichtbar.
