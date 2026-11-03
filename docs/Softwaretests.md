# Automatische Softwareprüfung

Stand: 08.10.2026. Ergebnis: **347 C-Prüfungen und die Webtests bestanden**.
Die Firmware wurde für diese Tests nicht auf das Board übertragen.
Der vollständige ESP32-S3-Build wurde anschließend erfolgreich geprüft.
Die Firmware passt in die 1-MiB-App-Partition; 5 % bleiben frei.

## Was wird geprüft?

`tests/firmware-test.ps1` liest zwölf echte Funktionen und die zugehörigen
Datenstrukturen direkt aus `main/main.c`. Das Testprogramm verwendet dieselbe
cJSON-Bibliothek wie die Firmware. Die Funktionen werden für ein virtuelles
RISC-V-Board gebaut und mit dem vorhandenen Espressif-QEMU ausgeführt.
Die logischen C-Funktionen sind portabel; die eigentliche Firmware läuft
weiterhin auf dem ESP32-S3 mit Xtensa-Prozessor.

| Bereich | Automatischer Nachweis |
|---|---|
| Profilprüfung | Gerade vier Adern, Unterbrechung, falsches Ziel, Mehrfachverbindung und umgekehrte Richtung. |
| Messprofil | Alle 256 Kombinationen der acht B-Ziele eines A-Anschlusses; Einzelziel als Zahl, offen als -1, mehrere Ziele als Liste. |
| Zusatzdaten | Charge und verschachtelte Daten erhalten; gemessen ergänzt, ursprünglicher Auftrag unverändert. |
| Server-JSON | Dieselben Zusatzdaten wie in der Laufliste, kein gemeldet-Feld im Versand; Fehleranzahl und Vergleichsrichtung geprüft. |
| Auftrag anlegen | Ungültiges JSON, falsche Typen, Wertebereiche, Bruchzahlen, Anfrage über 2048 Bytes und Empfangstimeout abgewiesen. |
| API-Zustände | 201 für neuen Auftrag, 409 bei aktivem Auftrag, GET mit/ohne Auftrag, DELETE mit 200/404, erschöpfte IDs mit 503 und Sperrfehler mit 500. |
| Messungsabbruch | Nach Abbruch oder Änderung der Auftrags-ID wird ein altes Messergebnis verworfen. |
| Speichergrenze | Nach 21 neuen Läufen bleiben genau 20 erhalten, mit den Laufnummern 2 bis 21; auch GET /api/runs liefert diese Reihenfolge. |
| Webbedienung | Profile, Eingaben, Fortschritt, sieben Tabellenspalten, Messprofil und Zusatzdaten, Abbruch, Konflikt sowie Verbindungsverlust/Wiederkehr. |

## Grenzen des Nachweises

FreeRTOS-Sperren, HTTP-Empfang und -Antworten sowie die Versandwarteschlange
werden im C-Test nachgebildet. Die API-Handler und die geprüften Logikfunktionen
stammen unverändert aus dem aktuellen Firmware-Code. Echte parallele Tasks,
HTTP-Transport, WLAN, GPIO-Pegel, LEDs und die Verdrahtung der PCB sind nicht
Bestandteil des Emulator-Tests. Es wurde keine echte Empfängermeldung zugestellt.

Die Webtests verwenden nachgebildete Browser-Elemente und API-Antworten.
Die tatsächlichen Hardware- und Netzwerktests bleiben im Abnahmeprotokoll offen,
soweit sie nicht durch Pruefperson bestätigt wurden. Der Emulator-Nachweis ersetzt
keinen vollständigen PCB-Abnahmelauf.

## Erneut ausführen

Voraussetzungen: Node.js, Espressif RISC-V-GCC und QEMU sowie die nach einem
ESP-IDF-Build vorhandene cJSON-Abhängigkeit. Standardpfad der Espressif-Werkzeuge
ist `C:/Espressif/tools`; bei anderer Installation `-ToolsRoot` angeben.

Im Projektordner:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\firmware-test.ps1
node .\tests\web-test.cjs
```

Erwartet: `Firmware-C-Tests bestanden: 347 Pruefungen` und `Webtests bestanden`.
Ausgaben und erzeugte Dateien liegen unter `build/firmware-test` und werden
nicht in Git gespeichert. Ein hängender Emulator wird nach 20 Sekunden beendet.

Der Hardwaretest bleibt separat in [Abnahme.md](Abnahme.md) und
[Abschluss-Checkliste.md](Abschluss-Checkliste.md) beschrieben.
