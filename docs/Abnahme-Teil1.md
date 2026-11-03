# Test und Abnahme

Historischer Testplan für Projekt 1. Die Angaben zu `AKTIVES_PROFIL`,
Startmeldungen und ausgeschalteten LEDs beschreiben einen früheren Stand
und gelten nicht als Anleitung für die aktuelle Netzwerkfirmware.
Für Projekt 2 siehe [Abnahme.md](Abnahme.md).

## Vorbereitung

Board-Variante, reale Steckerbelegung und Verdrahtung gegen
[Pinbelegung.md](Pinbelegung.md) und [Schaltskizze.svg](Schaltskizze.svg) prüfen.
Testadapter: Stecker → Breadboard/Jumper → Stecker. Nur spannungsfreie Kabel
anschließen. In `main/main.c` für die ursprünglichen Abnahmefälle
`AKTIVES_PROFIL` auf `1` setzen, bauen und flashen. Der aktuelle gespeicherte
Code verwendet noch Profil `0`; es wird hier nicht automatisch umgestellt.

Im Monitor muss `Aktives Profil 1: Abnahme: vier gerade` erscheinen.
Nach dem Einschalten sind beide LEDs aus. Jeder entprellte Wechsel des
Kippschalters an GPIO11 löst einen Test aus, in beiden Schaltrichtungen.
Sehr schnelle Wechsel unterhalb der Entprellzeit von 30 ms zählen nicht.

## Fälle am Testadapter

Für jeden Fall zunächst den korrekten Aufbau A0–B0, A1–B1, A2–B2, A3–B3
herstellen; A4–A7 und B4–B7 bleiben frei. Dann die angegebene Änderung stecken.
Während der Messung den Aufbau nicht verändern.

| Nr. | Aufbau / Änderung | Erwartet | Ergebnis vor Ort |
|---:|---|---|---|
| 1 | Korrekter Aufbau | Grün, OK | noch nicht geprüft |
| 2 | A2–B2 entfernen | Rot, FEHLER | noch nicht geprüft |
| 3 | A0–B1 und A1–B0 statt der geraden Verbindungen | Rot, FEHLER | noch nicht geprüft |
| 4 | Zusätzlicher Jumper zwischen A1 und A2 | Rot, FEHLER | noch nicht geprüft |
| 5 | Zusätzlich A5–B5 verbinden | Rot, FEHLER | noch nicht geprüft |
| 6 | Kein Kabel angeschlossen, Schalter wechseln | Rot, FEHLER im Monitor; LEDs gehen nach Leerscans wieder aus | noch nicht geprüft |
| 7 | Korrekter Aufbau, zehn bewusste Schalterwechsel | Zehn OK-Ergebnisse im Monitor | noch nicht geprüft |
| 8 | Neues Profil gemäß Anleitung ergänzen und passend stecken | OK; Anlegen, Flashen und Test in unter fünf Minuten | noch nicht geprüft |
| 9 | Nach OK oder FEHLER das Kabel abziehen | Beide LEDs aus, sobald zwei Leerscans keine Verbindung zeigen | noch nicht geprüft |

Ein vollständig unterbrochenes Kabel ist für diesen Messaufbau nicht von einem
abgezogenen Kabel unterscheidbar. Daher wird dessen FEHLER-Anzeige ebenfalls
nach zwei Leerscans gelöscht. Das ist eine Grenze des aktuellen Codes.
GPIO11/Kippschalter und externe LEDs weichen vom ursprünglichen BOOT-/Onboard-
LED-Auftrag ab; die Abnahme dieser Ausführung muss abgestimmt werden.

## Nachweis

Diese Tabelle ist ein Testplan, kein bestandener Hardwaretest. Nach jedem
Versuch das tatsächliche Ergebnis mit Datum ergänzen. Build und Flash müssen
in der eingerichteten ESP-IDF-Umgebung geprüft werden.

Bei der Abnahme dieses Repository in VS Code öffnen und zeigen:

```powershell
git status
git log --oneline -5
git ls-files
```

Pinbelegung, Schaltskizze und Profilanleitung liegen gemeinsam mit dem Code
im Repository. Falls die Umsetzung länger als eine Stunde festhängt, eine
konkrete Frage mit eigenem Lösungsvorschlag an den Auftraggeber richten.
