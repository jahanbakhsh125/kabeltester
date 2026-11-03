# Projektstand und lokale Abgabeübersicht

Stand: 08.10.2026. Projektordner:
`<PROJEKTORDNER>`.
Die Unterlagen werden zuerst lokal vorbereitet. Die Bedienungsanleitung
ist noch nicht in BookStack veröffentlicht; ein Link zum Kapitel fehlt.
Pruefperson hat am 08.10.2026 mitgeteilt, dass noch kein BookStack-Zugang vorhanden ist.

## Wo liegt was?

| Datei oder Ordner | Inhalt |
|---|---|
| `main/main.c` | Messablauf, LEDs, WLAN, mDNS, API und Ergebnisversand. |
| `main/index.html` | Webseite, beim Build in die Firmware eingebettet. |
| `config/flash_config.example.h` | Öffentliche Vorlage der Flash-Konfiguration mit Platzhaltern. |
| `config/flash_config.h` | Private lokale Konfiguration; darf nicht in Git oder die Abgabe an Dritte. |
| `auftrag-test.json` | Auftrag für drei gerade Achtadermessungen. |
| `auftrag-abnahme.json` | Auftrag für drei Vieradermessungen, Zusatzdaten `{"charge":"A17"}`. |
| `tests/api-test.ps1` | Geführter API-Test mit drei Achtadermessungen. |
| `tests/api-grenztests.ps1` | Geführte Tests für FEHLER, Messungsabbruch und Speichergrenze. |
| `tests/web-test.cjs` | Verhaltenstest der Webseite mit nachgebildeten API-Antworten. |
| `tests/firmware-test.ps1` | Echte C-Funktionen und API-Handler automatisch im Emulator prüfen. |
| [Softwaretests.md](Softwaretests.md) | 347 bestandene C-Prüfungen und Webtests; Grenzen des Software-Nachweises. |
| [Aufgabe-Netzwerk.md](Aufgabe-Netzwerk.md) | Anonymisierte Kopie der ursprünglichen Netzwerkaufgabe. |
| [Pinbelegung.md](Pinbelegung.md) und [Schaltskizze.svg](Schaltskizze.svg) | Anschlussplanung und Verdrahtung. |
| [Neues-Profil.md](Neues-Profil.md) | Eigenes Auftragsprofil verwenden oder Standardprofil ergänzen. |
| [API-Entwurf.md](API-Entwurf.md) | Endpunkte, JSON-Format, Statuscodes und Empfängermeldung. |
| [Bedienungsanleitung.md](Bedienungsanleitung.md) | Deutsche Anleitung, zugleich Vorlage für BookStack. |
| [Abnahme.md](Abnahme.md) | Aktuelle Prüfschritte, bisherige Nachweise und ausfüllbares Protokoll. |
| [Abnahme-Teil1.md](Abnahme-Teil1.md) | Historischer Testplan des ersten Projekts. |

## Funktionsstand

Im Gespräch wurden Kabelmessungen, Auftragsverwaltung, Webbedienung, LEDs,
WLAN-Wiederverbindung, Speicherung der Laufdaten und der Umgang mit einem
unerreichbaren Empfänger gezeigt oder bestätigt. Der ESP startet mDNS;
`kabeltester.local` funktioniert mit dem PC im passenden WLAN, auch bei
zusätzlich eingestecktem LAN-Kabel. Der genaue Umfang der Nachweise steht
in der Abnahmeanleitung. Diese Übersicht ersetzt kein Testprotokoll.

## Noch offen

- [x] Korrekter Vieradertest nach Fehlerkorrektur: von Pruefperson am 08.10.2026 als fertig bestätigt.
- [x] Automatische C-Prüfungen für API, Profile, Messdaten, Abbruchlogik und 20er-Speicher sowie Webtests bestanden.
- [ ] Vorgeschriebener Vieraderablauf: OK, OK, FEHLER per API.
- [ ] Vorgeschriebener Vieraderablauf über die Webseite einschließlich Abbruch.
- [ ] WLAN-Ausfall mit genau einer Minute Dauer protokollieren.
- [ ] Kein Auftrag: bestätigen, dass die Laufliste beim Schalterwechsel unverändert bleibt.
- [ ] Zugehörige Vieraderläufe als nicht gemeldet auf der Webseite nachweisen.
- [ ] Bedienung durch eine andere Person allein anhand der Anleitung prüfen.
- [ ] Ergänzend: Abbruch während einer tatsächlich laufenden Messung nachweisen.
- [ ] Ergänzend: 21 Läufe abschließen und Überschreiben von Lauf 1 nachweisen.
- [ ] Ergänzend: Vorrang einer Ergebnis-LED bei gleichzeitigem WLAN-Ausfall prüfen.
- [ ] API-Abstimmung und Hardwareausführung mit dem Ansprechpartner klären, sofern noch nicht erfolgt.
- [ ] BookStack-Buch auswählen, Anleitung veröffentlichen und Kapitel-Link ergänzen.
- [x] Softwarestand und Unterlagen geprüft und lokal in Git gesichert (08.10.2026).
- [ ] Abschließende Messergebnisse und BookStack-Link nachreichen und als Abgabestand sichern.

Ein echter Meldeempfänger ist noch nicht vorhanden. Ein erfolgreicher Versand
an diesen wurde daher bisher nicht nachgewiesen. Die Abnahme verlangt
ausdrücklich einen nicht erreichbaren Empfänger.
Fehlerdetails je Ader und die gemessenen Verbindungen sind bei FEHLER in
Laufliste/Meldung implementiert. Die Webseite zeigt das Messprofil in einer
eigenen Spalte und zusätzlich als `gemessen` in den Zusatzdaten;
Build und Webtests bestanden. Die neue Messprofilspalte und `args.gemessen`
wurden anschließend in einem Screenshot der laufenden Webseite gezeigt.
Pruefperson hat am 08.10.2026 bestätigt, dass der Vieraderfehler korrigiert ist,
und den korrekten Vieradertest als fertig bestätigen lassen.
Die aktualisierte GPIO-Zuordnung steht in [Pinbelegung.md](Pinbelegung.md)
und [Schaltskizze.svg](Schaltskizze.svg). Der vollständige Ablauf
OK, OK, FEHLER bleibt bis zum zugehörigen Testprotokoll offen.
Firmware-Upload über die Webseite ist derzeit
nicht implementiert und keine Pflicht der ursprünglichen Netzwerkaufgabe.

## Nächster praktischer Schritt: Vierader-Abnahme

Das vorbereitete Vieraderkabel mit der korrigierten Zuordnung verwenden.
Nur A0–B0, A1–B1, A2–B2 und A3–B3 verbinden; A4–A7 bleiben ohne Verbindung.
Die zugehörigen GPIO-Paare stehen in der aktuellen Pinbelegung.
Den geplanten Test zunächst erklären,
dann die Schritte aus [Abnahme.md](Abnahme.md) durchführen.

Die fertige Datei `auftrag-abnahme.json` kann im Projektterminal verwendet werden:

```powershell
curl.exe -i -X POST "http://kabeltester.local/api/jobs" -H "Content-Type: application/json" --data-binary "@auftrag-abnahme.json"
```

Dieser POST legt einen aktiven Auftrag für drei Vieradermessungen an.
Er startet keine Messung. Nur senden, wenn kein anderer Auftrag aktiv ist.
Alle tatsächlichen Ergebnisse anschließend im Abnahmeprotokoll eintragen.

## Was jetzt nicht erforderlich ist

Für diese Dokumentationsvorbereitung ist kein Flashen erforderlich.
Es wurden keine zusätzlichen Standardprofile in der Firmware angelegt.
Die neue Vieraderdatei ist eine Vorlage für den späteren Test; sie wurde
nicht automatisch an den ESP gesendet. Dieser Stand wird als lokaler
Git-Zwischenstand gesichert; die abschließende Abgabe folgt nach den offenen
Tests und der BookStack-Veröffentlichung. Den gespeicherten Zwischenstand
mit `git log --oneline -5` anzeigen.

## Lokale Abschlussprüfung vom 08.10.2026

- Aktuelle GPIO-Zuordnung in Code, Pinbelegung und Schaltskizze abgeglichen.
- Webtests mit nachgebildeten API-Antworten bestanden; reale Kabeltests bleiben separat.
- Öffentliche Flash-Konfigurationsvorlage auf Platzhalter zurückgesetzt.
- Private `config/flash_config.h` bleibt ausgeschlossen und wird nicht abgegeben.
- Anleitung und API-Beispiele vorbereitet; BookStack-Veröffentlichung wartet auf Zugang.
- Die noch offenen Hardwaretests werden nicht als bestanden eingetragen.
- Zusätzlicher Software-Nachweis: 347 C-Prüfungen bestanden, einschließlich
  verworfener Messungen nach Abbruch und Überschreiben nach Lauf 21.
  Siehe [Softwaretests.md](Softwaretests.md); echte Task-Zeitabläufe und GPIOs
  bleiben praktische Nachweise am Board.

Für die restlichen Schritte siehe [Abschluss-Checkliste.md](Abschluss-Checkliste.md).
