# Test und Abnahme: Kabeltester im Netzwerk

Stand: 08.10.2026. Grundlage ist die Projektaufgabe vom 01.10.2026.
Dieser Plan trennt erwartetes Verhalten von tatsächlich bestätigten Tests.
„Bestätigt“ bezieht sich auf die im Gespräch gezeigten Ausgaben und Rückmeldungen;
es ist kein neu ausgeführter Test und keine formale Abnahme.

Der frühere Testplan für Projekt 1 bleibt in
[Abnahme-Teil1.md](Abnahme-Teil1.md) erhalten. Seine alten Startmeldungen und
LED-Anweisungen gelten nicht für die aktuelle Netzwerkfirmware.

## Vorbereitung

- Aktuelle Netzwerkfirmware verwenden. Die Messtaste ist der externe Schalter
  an GPIO11; Grün liegt an GPIO9, Rot an GPIO10. Diese Ausführung weicht von
  BOOT/Onboard-LED aus dem ursprünglichen Auftrag ab und muss abgestimmt sein.
- Pinbelegung und Verdrahtung anhand von [Pinbelegung.md](Pinbelegung.md) und
  [Schaltskizze.svg](Schaltskizze.svg) prüfen. Nur spannungsfreie Kabel anschließen.
- Für die offizielle Abnahme das Unternehmens-WLAN verwenden. Der PC muss für
  die mDNS-Namenssuche mit dem lokalen Netz des ESP verbunden sein.
- Die konfigurierte Empfänger-URL muss für die Abnahme auf ein Ziel zeigen,
  an dem kein Server läuft. WLAN-Daten und URL stehen ausschließlich in der
  privaten Flash-Konfiguration; keine echten Zugangsdaten ins Protokoll kopieren.
- Zwei zum Vieraderprofil passende Kabel und ein fehlerhaftes Vieraderkabel
  oder einen entsprechend veränderbaren Testadapter vorbereiten.
  A0–B0, A1–B1, A2–B2 und A3–B3 sind die korrekten Verbindungen;
  A4–A7 bleiben ohne Verbindung. Für den Fehlerfall z. B. A2–B2 unterbrechen.
- Abnahme und Ergebniskontrolle ohne Neustart durchführen. Ein Neustart löscht
  Aufträge und Laufliste. Vor jeder weiteren Messung das Kabel abziehen und
  warten, bis die Ergebnis-LED nicht mehr dauerhaft leuchtet.
- Alte Aufträge bewusst beenden oder abbrechen. Keine bereits laufende
  Arbeit allein für einen Test ungefragt überschreiben.

Für curl-Befehle in einem PowerShell-Terminal die Adresse festlegen:

```powershell
$baseUrl = 'http://kabeltester.local'
```

Falls nötig, stattdessen die aktuelle IP-Adresse aus dem Monitor verwenden.
In einem zweiten ESP-IDF-Terminal kann der Monitor geöffnet bleiben:

```powershell
idf.py -p COM5 monitor
```

COM5 ist der bisher verwendete Port. Einen bereits geöffneten Monitor verwenden,
statt denselben Port ein zweites Mal zu öffnen.

## Bisher bestätigter Stand

| Funktion | Bisheriger Nachweis | Grenze des Nachweises |
|---|---|---|
| Aktuelle Firmware startet | Startausgabe zeigt Webserver, mDNS und WLAN-IP. | Kein Nachweis jeder einzelnen Anforderung. |
| Name und IP erreichbar | Nutzer bestätigt Webseite über IP und `kabeltester.local`, zuletzt mit LAN-Kabel und zusätzlich verbundenem WLAN. | PC muss tatsächlich im passenden lokalen Netz verbunden sein. |
| WLAN-Wiederverbindung | Verbindungsabbruch und erneute Verbindung ohne ESP-Neustart gezeigt; LED-Verhalten bestätigt. | Exakte Unterbrechung von einer Minute nicht protokolliert. |
| Auftrag per API anlegen/lesen | 201 und Fortschritt von 0 auf 1 sowie Auftragsende gezeigt. | Konkreter Vieraderablauf OK, OK, FEHLER noch offen. |
| Zweiten Auftrag ablehnen | 409 `job_already_active` gezeigt. | Für den offiziellen Abnahmelauf nochmals mitführen. |
| Aktiven Auftrag abbrechen | 200 `cancelled`, anschließend `job:null`; zweites DELETE mit 404 gezeigt. | Abbruch während der laufenden Messung noch nicht vollständig bestätigt. |
| FEHLER speichern und zählen | Grenztest mit Achtaderkabel gegen Vieraderprofil bestanden. | Ersetzt keine zwei korrekten Vieradermessungen. |
| Vieraderfehler und Pinzuordnung | Pruefperson bestätigt am 08.10.2026 den korrekten Vieradertest nach seiner Fehlerkorrektur; GPIO-Reihenfolge dokumentiert. | Nutzerbestätigung; vollständige Messfolge OK, OK, FEHLER separat protokollieren. |
| Profile und Zusatzdaten in Läufen | Ausgaben zeigen Profil, `args` und `gemeldet:false`. | Annahme durch einen echten Empfänger bisher nicht getestet. |
| Fehlender Empfänger blockiert Messungen nicht | Drei OK-Läufe gespeichert, Auftrag beendet, Empfänger `unreachable`. | Erfolgreicher Versand ist mangels Empfänger offen; für die Abnahme wird Unerreichbarkeit erwartet. |
| Webbedienung | Auftrag, Fortschritt, Ergebnisse, Abbruch und Standardprofil-Auswahl gezeigt/bestätigt. | Vollständiger Vierader-Abnahmeablauf über die Webseite noch offen. |
| LED-Zustände | Bereitschaft, aktiver Auftrag, OK/FEHLER und Warnungen im Gespräch bestätigt. | Ergebnisvorrang bei gleichzeitigem WLAN-Ausfall gesondert prüfen. |
| Speichergrenze | Test wurde begonnen; gezeigter Zwischenstand 6 von 21. | Überschreiben nach Lauf 21 noch nicht bestätigt. |

## Die zehn vorgeschriebenen Abnahmefälle

Die Fälle entsprechen der Nummerierung in der Projektaufgabe.
Nach der tatsächlichen Durchführung Datum, Ergebnis und einen knappen Nachweis
in die Tabelle am Ende eintragen.

### 1. Einschalten, WLAN und Hostname

Gerät mit der vorgesehenen Flash-Konfiguration einschalten. Falls die Firmware
noch übertragen werden muss, den Monitor schließen und bewusst bauen/flashen:

```powershell
idf.py -p COM5 build flash monitor
```

Erwartet sind die Startmeldungen für Webserver und mDNS sowie
`WLAN verbunden! IP-Adresse: ...`. Danach `http://kabeltester.local` im Browser
öffnen und die API über den Namen prüfen:

```powershell
curl.exe -i "$baseUrl/api/jobs/current"
```

Erwartet: HTTP 200 und eine JSON-Antwort. Ein funktionierender IP-Zugriff allein
bestätigt die mDNS-Anforderung noch nicht.

### 2. WLAN für eine Minute unterbrechen

Ohne angezeigtes Messergebnis die WLAN-Verbindung des ESP unterbrechen,
eine Minute warten und das WLAN wieder verfügbar machen. Dazu den Access Point
oder einen dafür vorgesehenen Test-Hotspot verwenden; ein gemeinsam genutztes
Unternehmens-WLAN nur in Abstimmung unterbrechen. WLAN nur am PC abzuschalten
trennt den ESP nicht von seinem Access Point.

Erwartet: Rot blinkt langsam während des Ausfalls. Danach verbindet sich der
ESP ohne Neustart erneut. Webseite/API werden wieder erreichbar; ohne Auftrag
blitzt Grün kurz alle drei Sekunden.

### 3. Drei Vierader-Läufe per API: OK, OK, FEHLER

Eine gesonderte Testdatei anlegen; die vorhandene Achtaderdatei bleibt erhalten:

```powershell
'{"profile":[0,1,2,3,-1,-1,-1,-1],"count":3,"args":{"charge":"A17"}}' | Set-Content -LiteralPath .\auftrag-abnahme.json -Encoding ascii
curl.exe -i -X POST "$baseUrl/api/jobs" -H "Content-Type: application/json" --data-binary "@auftrag-abnahme.json"
```

Erwartet: 201, `status:active`, `completed:0`, `count:3`.
Die zurückgegebene `job_id` für die Zuordnung der Ergebnisse notieren.

Zwei passende Vieraderkabel und dann das fehlerhafte Vieraderkabel prüfen.
Zwischen den Läufen jeweils abziehen und das Ende der dauerhaften
Ergebnisanzeige abwarten. Während der Messung die Verdrahtung nicht verändern.

Nach jedem Lauf den Fortschritt und die Laufliste lesen:

```powershell
curl.exe -i "$baseUrl/api/jobs/current"
curl.exe -i "$baseUrl/api/runs"
```

Erwartet: Nach Lauf 1 bzw. 2 ist `completed` gleich 1 bzw. 2.
Nach Lauf 3 ist `job:null`. Zur notierten Auftragsnummer gehören genau die
Läufe 1, 2, 3 mit OK, OK, FEHLER, jeweils dem Vieraderprofil und
`args.charge:"A17"` und `args.gemessen` mit den tatsächlichen Verbindungen.
`measured_profile` muss dasselbe Array wie `args.gemessen` enthalten.
Bei den beiden OK-Läufen wird `[0,1,2,3,-1,-1,-1,-1]` erwartet;
der FEHLER-Lauf enthält das tatsächlich abweichende Messprofil.
Auch der FEHLER-Lauf zählt zum Auftragsende.

### 4. Zweiten Auftrag ablehnen

Während ein Auftrag noch aktiv ist, den POST aus Fall 3 wiederholen.

Erwartet: HTTP 409 und `job_already_active`. Anschließend aktuellen Auftrag
lesen: Auftragsnummer, Profil und Fortschritt des ersten Auftrags bleiben erhalten.
Der weitere Ablauf des ersten Auftrags wird nicht durch den zweiten POST beendet.

### 5. Auftrag abbrechen

Wenn kein Auftrag aktiv ist, einen neuen anlegen. Einen aktiven Auftrag abbrechen:

```powershell
curl.exe -i -X DELETE "$baseUrl/api/jobs/current"
curl.exe -i "$baseUrl/api/jobs/current"
```

Erwartet: DELETE liefert 200 und `status:cancelled`; GET liefert 200 und
`job:null`. Bereits gespeicherte Läufe bleiben erhalten.

### 6. Vieraderauftrag über die Webseite

Auf der Webseite Standardprofil `gerade4` wählen, Anzahl `1` einstellen und
Auftrag anlegen. Ein korrektes Vieraderkabel anschließen und den externen
Schalter betätigen.

Erwartet: OK, dauerhaft grüne Ergebnis-LED bis zum Abziehen, ein gespeicherter
Lauf und danach kein aktiver Auftrag. Ein gerades Achtaderkabel ist hierfür
kein passender Ersatz.

### 7. Nicht erreichbaren Empfänger anzeigen

Ohne ESP-Neustart die Webseite und die Laufliste aus den Fällen 3 und 6 prüfen.
Den Versandversuchen Zeit geben, abgeschlossen zu werden.

Erwartet: „Meldeempfänger nicht erreichbar“ und die betreffenden Läufe als
„Nicht gemeldet“ bzw. `gemeldet:false`. Kabelergebnis und Fortschritt wurden
trotz des Versandfehlers gespeichert. Direkt nach einer Messung kann der
Versand noch ausstehen; deshalb den Status erneut abfragen.

### 8. Fälle 3 bis 5 über die Webseite

Über die Webseite mit `gerade4`, Anzahl `3` und Zusatzdaten
`{"charge":"A17"}` einen Auftrag anlegen. Wieder OK, OK und FEHLER mit
passender Vieraderverdrahtung erzeugen und die Liste kontrollieren.

Während eines aktiven Auftrags sind die Eingaben zum Anlegen eines zweiten
Auftrags gesperrt. Den Schutz auf API-Ebene zusätzlich mit dem POST aus Fall 4
prüfen, falls ein zweiter Client verwendet wird. Anschließend einen neuen
Webauftrag anlegen und über „Auftrag abbrechen“ beenden.

Erwartet: Derselbe Fortschritt, dieselben gespeicherten Daten und dasselbe
Abbruchverhalten wie bei curl.

### 9. Schalter ohne Auftrag

`GET /api/jobs/current` muss `job:null` zeigen. Vorherige Ergebnisanzeige
durch Abziehen beenden. Laufliste vor dem Versuch notieren. Kabel anschließen
und externen Schalter betätigen; anschließend die Laufliste erneut lesen.

Erwartet: Rot blinkt schnell für drei Sekunden, Monitor zeigt
„Kein Auftrag aktiv - keine Messung.“ Es erscheint kein neuer Lauf.
Danach kehrt die Anzeige bei vorhandenem WLAN zur grünen Bereitschaftsanzeige zurück.

### 10. Bedienung nur mithilfe der Anleitung

Eine Person, die den Tester nicht gebaut hat, verwendet allein die
[Bedienungsanleitung](Bedienungsanleitung.md), um einen Auftrag per curl
anzulegen und die Laufliste abzurufen.

Erwartet: Der Ablauf klappt ohne zusätzliche Erklärung. Rückfragen oder
unklare Stellen notieren und die Anleitung entsprechend verbessern.
Die vorhandene Anleitung allein ist noch kein Nachweis dieses Tests.

## Ergänzende Grenztests

Diese Tests ergänzen die zehn Abnahmefälle. Sie sind keine zusätzlichen
Pflichtfälle aus der Abnahmetabelle der Aufgabe.

Die vorhandenen Skripte brauchen echte Schalterbetätigungen und warten
auf Benutzereingaben. Sie können keine Kabel selbst anschließen.
Die Skripte legen Testaufträge an; nur bewusst starten, wenn kein anderer
Auftrag mehr aktiv ist. Während eines Durchlaufs den ESP nicht neu starten.

### FEHLER mit dem vorhandenen Achtaderkabel

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\api-grenztests.ps1 -BaseUrl "http://kabeltester.local" -Test Fehler
```

Das Skript erwartet einen FEHLER, weil es das Vieraderprofil für ein
Achtaderkabel verwendet. Dieser Test wurde im Gespräch bereits bestanden.
Er bestätigt das Speichern und Zählen eines FEHLER-Ergebnisses.

### Abbruch während einer Messung

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\api-grenztests.ps1 -BaseUrl "http://kabeltester.local" -Test Abbruch
```

Die Anweisungen und den Countdown des Skripts befolgen. Ein gültiger Nachweis
braucht beide Monitorzeilen: „Messung startet“ und
„Messung verworfen - Auftrag nicht mehr aktiv.“ Der Auftrag ist anschließend
inaktiv und der verworfene Lauf fehlt in der Laufliste.

Wurde vor Messbeginn oder erst nach dem Speichern abgebrochen, ist die gewünschte
zeitliche Situation nicht getroffen. Dann den Versuch als nicht bestätigt
notieren und nach Bedarf wiederholen. Ein normaler Auftragsabbruch reicht
für diesen speziellen Nachweis nicht aus.

### 21 Läufe und Überschreiben des ältesten

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\api-grenztests.ps1 -BaseUrl "http://kabeltester.local" -Test Speicher
```

21 echte Messungen durchführen. Erst nach jedem Ergebnis im Monitor im
Skript Enter drücken. Erwartet: höchstens 20 Einträge zu jeder Zeit;
nach Lauf 21 bleiben für den Testauftrag die Laufnummern 2 bis 21 erhalten,
Lauf 1 ist verdrängt, und der Auftrag ist beendet. Ein Zwischenstand
wie „6 von 21“ bestätigt das Überschreiben noch nicht.

### Ergebnis hat Vorrang vor WLAN-Warnung

Ein Ergebnis anzeigen lassen und das Kabel angeschlossen lassen. Danach
die WLAN-Verbindung des ESP unterbrechen. Erwartet: Die Ergebnis-LED bleibt
dauerhaft an. Erst nach dem Abziehen erscheint die WLAN-Warnung, wenn die
Verbindung weiterhin fehlt. WLAN anschließend wiederherstellen.

## Protokoll zum Ausfüllen

Am 08.10.2026 wurden zusätzlich 347 C-Prüfungen und die Webtests erfolgreich
ausgeführt. API-Handler, Abbruchlogik und 20er-Ringpuffer sind damit auf
Softwareebene geprüft. Umgebung und Grenzen stehen in
[Softwaretests.md](Softwaretests.md). Die folgenden praktischen Tests bleiben
separate Nachweise; ihr Status wird durch Emulator-Tests nicht ersetzt.

„Offen“ bedeutet: Der vollständige vorgeschriebene Ablauf ist noch nicht
als aktueller Abnahmelauf protokolliert. Die Teilnachweise stehen weiter oben.

| Fall | Datum | Tatsächliches Ergebnis / Nachweis | Status |
|---|---|---|---|
| 1: WLAN und Hostname | — | Funktion im Gespräch bestätigt; formalen Abnahmelauf eintragen. | Funktion bestätigt |
| 2: WLAN eine Minute aus | — | Wiederverbindung bestätigt; genaue Dauer noch eintragen. | Teilweise bestätigt |
| 3: Vierader OK, OK, FEHLER per API | — | — | Offen |
| 4: Zweiter Auftrag | — | 409 im Gespräch gezeigt; formalen Abnahmelauf eintragen. | Funktion bestätigt |
| 5: Abbruch | — | 200, danach job:null im Gespräch gezeigt. | Funktion bestätigt |
| 6: Vierader OK über Webseite | 08.10.2026 | Korrekter Vieradertest nach Fehlerkorrektur von Pruefperson bestätigt; keine neue Messausgabe beigefügt. | Vieradermessung von Pruefperson als fertig bestätigt |
| 7: Empfänger nicht erreichbar | — | Verhalten bestätigt; Zuordnung zu Fällen 3/6 noch ergänzen. | Teilweise bestätigt |
| 8: Vieraderablauf über Webseite | — | Webfunktionen bestätigt; vollständiger Ablauf fehlt. | Teilweise bestätigt |
| 9: Kein Auftrag | — | Warnung bestätigt; unveränderte Laufliste noch protokollieren. | Teilweise bestätigt |
| 10: Fremde Person nur mit Anleitung | — | — | Offen |
| Zusatz: Abbruch während Messung | — | — | Offen |
| Zusatz: Lauf 21 verdrängt Lauf 1 | — | Nur Zwischenstand gezeigt. | Offen |
| Zusatz: Ergebnisvorrang bei WLAN-Ausfall | — | — | Offen |

## Abgabe und Git-Nachweis

Zur Übergabe gehören Code im gleichen lokalen Repo wie Projekt 1,
die Vorlage der Flash-Konfiguration und der Link zum Anleitungs-Kapitel
im vorgesehenen BookStack-Buch. Die lokale Anleitung ist vorbereitet;
das BookStack-Kapitel ist noch nicht veröffentlicht. Pruefperson hat am 08.10.2026
bestätigt, dass derzeit noch kein BookStack-Zugang vorhanden ist.

Im Repo die Dokumentation zeigen: Pinbelegung, Schaltskizze, Profilanleitung,
[API-Entwurf](API-Entwurf.md), [Bedienungsanleitung](Bedienungsanleitung.md)
und diesen Testplan.

Im Projektterminal:

```powershell
git status
git log --oneline -5
git ls-files
git check-ignore config/flash_config.h
git ls-files -- config/flash_config.h
```

Nach dem abschließenden Commit soll der Arbeitsstand sauber sein. Die echte
Konfigurationsdatei muss ausgeschlossen bleiben: `git check-ignore` nennt
ihren Pfad, `git ls-files -- config/flash_config.h` liefert keinen Eintrag.
Kein Inhalt der privaten Konfiguration wird für den Nachweis benötigt.

API-Abstimmung mit dem Ansprechpartner und Zustimmung zur Hardwareausführung
sind organisatorische Punkte; diese Datei behauptet keine erfolgte Freigabe.
Bei mehr als einer Stunde ohne Fortschritt eine konkrete Frage mit eigenem
Lösungsvorschlag an den Auftraggeber richten.
