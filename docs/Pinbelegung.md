# Pinbelegung

Stand: 08.10.2026. Aktualisiert nach Alis korrigierter GPIO-Zuordnung
in `main/main.c`. Die vorhandene `Pinplanung.png` zeigt die ältere Planung;
für die aktuelle logische Zuordnung gilt die folgende Tabelle.
Die Tabelle beschreibt GPIO-Nummern, keine physischen
Stiftpositionen. Die tatsächliche Board- und Modulvariante vor dem Verdrahten
prüfen; die Zeichnung benennt ESP32-S3 DevKitC-1.

| A-Anschluss | ESP-GPIO | B-Anschluss | ESP-GPIO |
|---|---:|---|---:|
| A0 | 6 | B0 | 42 |
| A1 | 16 | B1 | 39 |
| A2 | 4 | B2 | 1 |
| A3 | 7 | B3 | 41 |
| A4 | 15 | B4 | 40 |
| A5 | 5 | B5 | 2 |
| A6 | 17 | B6 | 47 |
| A7 | 18 | B7 | 21 |

| Funktion | GPIO / Anschluss | Verdrahtung |
|---|---|---|
| Messstart | GPIO11 | Kippschalter zwischen GPIO11 und GND |
| OK-Anzeige | GPIO9 | GPIO → 1 kΩ → grüne LED (Anode) → Kathode → GND |
| Fehleranzeige | GPIO10 | GPIO → 1 kΩ → rote LED (Anode) → Kathode → GND |
| Versorgung | Board-USB | Board über USB versorgen |
| Bezugspotential | GND | Gemeinsamer GND für Schalter und beide LEDs |

## Messanschlüsse

Jeder A-Anschluss erhält einen eigenen 1-kΩ-Serienwiderstand zwischen ESP und
Buchse, wie in der vorhandenen Pinplanung. Die B-Anschlüsse gehen direkt an
ihre GPIOs; der Code aktiviert deren interne Pull-down-Widerstände.
Nur der gerade geprüfte A-Pin wird HIGH geschaltet. Alle übrigen A-Pins bleiben
hochohmig. Anschließend werden alle acht B-Eingänge gelesen.

Die Zuordnung übernimmt den aktuellen Code. GPIO0 bleibt für
BOOT, GPIO19/20 für USB und GPIO43/44 für UART frei. Ob alle verwendeten Pins
auf dem tatsächlichen Board frei nutzbar sind, hängt auch von dessen Modul-
und Speicherbestückung ab und muss am konkreten Board geprüft werden.

## Anschlussnummerierung

Für `gerade4` werden mit dieser Zuordnung nur folgende Verbindungen erwartet:
GPIO6–GPIO42, GPIO16–GPIO39, GPIO4–GPIO1 und GPIO7–GPIO41.
Das entspricht A0–B0 bis A3–B3; A4–A7 bleiben offen.
Das Sollprofil bleibt `[0,1,2,3,-1,-1,-1,-1]`.

Pruefperson hat am 08.10.2026 gemeldet, dass der Vieraderfehler korrigiert ist.
Der vollständige Abnahmeablauf OK, OK, FEHLER wird gesondert protokolliert.

A0–A7 und B0–B7 sind logische Nummern des Programms. Eine Zuordnung zu
Kontaktnummern der realen Stecker ist in den vorhandenen Unterlagen nicht
festgelegt. Buchsen und Testadapter entsprechend beschriften und diese
Zuordnung vor der Abnahme am Aufbau prüfen.

Nur spannungsfreie, von anderen Geräten getrennte Kabel anschließen.
Die Zeichnung ist eine logische Verdrahtungsskizze, kein maßstäblicher Boardplan.
