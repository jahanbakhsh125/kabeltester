# Abschluss: praktische Tests und Übergabe

Stand: 08.10.2026. Der Vieraderfehler ist laut Pruefperson korrigiert; die aktuelle
Pinzuordnung steht in der Pinbelegung. Diese Checkliste führt durch die
noch offenen Nachweise. Einen Test erst nach tatsächlicher Durchführung abhaken.

- [x] Korrekter Vieradertest nach Fehlerkorrektur: am 08.10.2026 von Pruefperson
  als fertig bestätigt. Die folgenden vollständigen Abläufe sind separate Nachweise.
- [x] Automatische Softwareprüfung am 08.10.2026: 347 C-Prüfungen und Webtests
  bestanden, siehe [Softwaretests.md](Softwaretests.md). Die praktischen
  Grenztests weiter unten sind weiterhin am Board durchzuführen.

## 1. Vieraderauftrag per API

Im Projektordner ein ESP-IDF-Terminal öffnen, Firmware bei Bedarf mit
`idf.py -p COM5 build flash monitor` übertragen. COM5 durch den aktuellen Port
ersetzen. In einem zweiten Terminal:

```powershell
curl.exe -i "http://kabeltester.local/api/jobs/current"
```

Nur wenn `job:null` zurückkommt, den vorbereiteten Auftrag senden:

```powershell
curl.exe -i -X POST "http://kabeltester.local/api/jobs" -H "Content-Type: application/json" --data-binary "@auftrag-abnahme.json"
```

Bei Namensproblemen statt `kabeltester.local` die aktuelle IP aus dem Monitor
verwenden. Erwartet: 201 und count 3.

1. Passendes Vieraderkabel anschließen, Schalter betätigen: OK.
2. Kabel abziehen, Ende der dauerhaften Ergebnisanzeige abwarten, erneut
   anschließen und Schalter betätigen: OK.
3. Für FEHLER einen steckbaren Fehleradapter oder ein vorbereitetes fehlerhaftes
   Kabel verwenden, beispielsweise eine unterbrochene Ader. Die PCB bleibt
   unverändert. Wieder abziehen, Anzeige abwarten, Fehlerkabel anschließen
   und Schalter betätigen: FEHLER.

Die beiden OK-Messungen dürfen nacheinander mit demselben passenden Kabel
durchgeführt werden. Das absichtlich fehlerhafte Kabel muss noch mindestens
eine Verbindung haben, damit ein angeschlossenes Kabel erkannt werden kann.

```powershell
curl.exe -i "http://kabeltester.local/api/runs"
curl.exe -i "http://kabeltester.local/api/jobs/current"
```

- [ ] Drei Läufe des neuen Auftrags: OK, OK, FEHLER.
- [ ] Sollprofil `[0,1,2,3,-1,-1,-1,-1]`; charge A17 erhalten.
- [ ] `measured_profile` und `args.gemessen` sind identisch; bei OK gleich dem Sollprofil.
- [ ] Nach Lauf 3 ist der Auftrag beendet (`job:null`).
- [ ] Empfängerstatus nicht erreichbar / gemeldet false, solange kein Empfänger vorhanden ist.

Ausgaben im [Abnahmeprotokoll](Abnahme.md) festhalten. Ein Screenshot der
Tabelle ergänzt den Nachweis.

## 2. Webseite und Betriebszustände

- [ ] Über die Webseite gerade4, Anzahl 3 anlegen; OK, OK, FEHLER wiederholen.
- [ ] Neue Spalte Gemessenes Profil und Zusatzdaten kontrollieren.
- [ ] Einen weiteren Webauftrag anlegen und abbrechen; danach kein aktiver Auftrag.
- [ ] Ohne Auftrag Schalter betätigen: keine neue Messung in der Laufliste.
- [ ] WLAN des ESP für eine Minute unterbrechen und wieder einschalten:
  automatische Wiederverbindung ohne ESP-Neustart. Nur PC-WLAN auszuschalten
  trennt den ESP nicht vom Access Point.
- [ ] Ergebnis-LED hat Vorrang bei WLAN-Ausfall, solange das Prüfkabel steckt.
- [ ] Eine andere Person führt die Bedienung allein anhand der Anleitung durch.

Details und erwartete Zustände stehen in [Abnahme.md](Abnahme.md).

## 3. Ergänzende Grenztests

Die vorhandenen Skripte benötigen ein gerades Achtaderkabel. Im Projektterminal
jeweils einzeln starten und die Aufforderungen im Skript befolgen:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\api-grenztests.ps1 -Test Abbruch
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\api-grenztests.ps1 -Test Speicher
```

- [ ] Abbruch tatsächlich während der Messung; verworfenes Ergebnis wird nicht gespeichert.
- [ ] 21 Messungen: höchstens 20 Einträge; Laufnummern 2 bis 21 bleiben erhalten.

Das Skript steuert die API; Kabelwechsel und Schalterbetätigung bleiben echte
Handgriffe. Ein Softwaretest ersetzt diese Hardware-Nachweise nicht.

## 4. Veröffentlichung und Übergabe

- [ ] BookStack-Zugang erhalten (derzeit laut Pruefperson nicht vorhanden).
- [ ] Inhalt von Bedienungsanleitung.md ins vereinbarte Buch übernehmen,
  Darstellung und Kapitel-Link prüfen; Link in README und Projektstand ergänzen.
- [ ] API und abweichende Hardwareausführung mit Ansprechpartner abstimmen.
- [ ] Tatsächliche Ergebnisse in Abnahme.md eintragen, Änderungen committen.

Die öffentliche Vorlage enthält Platzhalter; private WLAN-Konfiguration nicht
weitergeben. Zum Zeigen des lokalen Git-Repos:

```powershell
git status
git log --oneline -5
git check-ignore config/flash_config.h
git ls-files -- config/flash_config.h
```

Der letzte Befehl darf keinen Eintrag liefern. Build-Verzeichnis und private
Konfigurationsdatei gehören nicht in die Abgabe.
