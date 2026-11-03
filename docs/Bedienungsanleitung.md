# Kabeltester im Netzwerk – Bedienungsanleitung

Stand: 08.10.2026. Für die aktuelle Ausführung mit externem Schalter und
roter/grüner LED. Dieses Dokument ist die lokale Vorlage für ein Kapitel in
BookStack; es ist noch nicht dort veröffentlicht.

## 1. Was der Kabeltester macht

Der Kabeltester prüft elektrische Verbindungen zwischen den Anschlüssen
A0 bis A7 und B0 bis B7. Ein Profil legt fest, welche Verbindungen vorhanden
sein sollen. Das Ergebnis lautet **OK**, wenn die Verbindungen zum Profil
passen, andernfalls **FEHLER**. Vertauschte Kabelenden werden ebenfalls
akzeptiert, wenn die Verbindungen zur umgekehrten Profilrichtung passen.

Ein **Auftrag** besteht aus einem Profil, der Anzahl zu prüfender Kabel und
optionalen Zusatzdaten. Ein **Lauf** ist eine einzelne gespeicherte Messung.
Es ist immer höchstens ein Auftrag aktiv. Jede gespeicherte Messung zählt,
auch ein FEHLER. Nach der gewünschten Anzahl endet der Auftrag automatisch.

Die Webseite dient zum Anlegen und Abbrechen von Aufträgen und zeigt
Fortschritt und Ergebnisse. Die eigentliche Messung startet am **externen
Schalter** des Geräts. Die RESET-/EN-Taste startet das Gerät neu und ist
nicht die Messtaste.

Nur spannungsfreie, vom übrigen Gerät getrennte Kabel anschließen. Ein OK
bestätigt die geprüften Verbindungen; es ist keine Prüfung von Isolation,
Strombelastbarkeit oder Netzwerkübertragung.

## 2. Einschalten und Webseite öffnen

1. Kabeltester mit Strom versorgen. Für die normale Webbedienung ist kein
   offenes VS-Code-Terminal erforderlich.
2. Warten, bis das Gerät mit dem hinterlegten WLAN verbunden ist. Ohne
   angezeigtes Messergebnis blinkt Rot bei fehlendem WLAN langsam.
3. PC oder Handy mit demselben lokalen WLAN verbinden.
4. Im Browser `http://kabeltester.local` öffnen. Falls der Hostname in der
   Flash-Konfiguration anders lautet, diesen Namen mit `.local` verwenden.
5. Auf der Seite muss **„Mit dem Kabeltester verbunden“** erscheinen.

Alternativ die aktuelle IP-Adresse verwenden, beispielsweise
`http://kabeltester.local`. Sie wird beim Start im ESP-Monitor angezeigt.
Die Adresse kann sich nach einem Wechsel des Netzes ändern.

Bei einem PC mit zusätzlich angeschlossenem LAN-Kabel prüfen, ob WLAN
tatsächlich verbunden bleibt. Im bisherigen Test funktionierte der Name
mit eingestecktem LAN-Kabel, sobald der PC zusätzlich im WLAN des ESP
verbunden war. Das LAN-Symbol allein zeigt den WLAN-Zustand nicht an.

Die Seite wird über HTTP bereitgestellt. Ein Browserhinweis „Nicht sicher“
bezeichnet die fehlende HTTPS-Verschlüsselung und erklärt keinen Kabelfehler.
Es gibt aktuell keine Anmeldung; das Gerät ist für das lokale Netz gedacht.

## 3. Einen Auftrag über die Webseite anlegen

Im Bereich **„Neuer Auftrag“**:

1. Unter **„Standardprofil“** ein zum Kabel passendes Profil wählen.
2. Unter **„Anzahl der Kabel“** die gewünschte Anzahl eintragen, etwa `3`.
3. **„Zusatzdaten (optional)“** bei Bedarf ausfüllen, beispielsweise
   `{"charge":"A17"}`. Das Feld darf auch leer bleiben.
4. Auf **„Auftrag anlegen“** klicken.
5. Im Bereich **„Aktueller Auftrag“** die Auftragsnummer und den Fortschritt
   prüfen, beispielsweise **„Auftrag 1: 0 von 3 geprüft“**.

Solange ein Auftrag aktiv ist, sind die Eingaben für einen neuen Auftrag
gesperrt. Ein anderer API-Client kann ebenfalls keinen zweiten Auftrag
anlegen. Bei fehlender Verbindung werden die Bedienelemente deaktiviert.

### Das passende Profil wählen

| Standardprofil | Erwartete Verbindungen |
|---|---|
| `gerade8` | A0–B0 bis A7–B7: acht gerade verbundene Adern. |
| `gerade4` | A0–B0 bis A3–B3; A4–A7 ohne Verbindung. |
| `gekreuzt01` | A0–B1, A1–B0; A2–B2 bis A7–B7. |
| `asymmetrisch` | A0–B1, A1–B2, A2–B0, A3–B3 bis A6–B6; A7 ohne Verbindung. |

Ein gesundes Achtaderkabel wird mit `gerade4` als FEHLER erkannt, weil vier
zusätzliche Verbindungen vorhanden sind. Für dieses Kabel `gerade8` wählen.
Die vorgesehenen Vierader-Abnahmefälle benötigen ein passendes Vieraderkabel
oder einen entsprechend verdrahteten Testadapter.

Unter **„Verbindungen A0 bis A7“** kann auch ein eigenes Profil eingegeben
werden. Es sind genau acht ganze Zahlen zwischen −1 und 7 erforderlich:

```text
0,1,2,3,-1,-1,-1,-1
```

Die erste Position beschreibt A0, die zweite A1 und so weiter. Der Wert
nennt den erwarteten B-Anschluss; −1 erwartet keine Verbindung. Zusätzliche
Verbindungen werden als Abweichung erkannt. Änderungen in diesem Feld
gelten für den neuen Auftrag und ändern keine gespeicherten Standardprofile.

Zusatzdaten müssen ein JSON-Objekt sein, etwa:

```json
{"charge":"A17","pruefer":"Pruefperson"}
```

Der Kabeltester wertet diese Daten nicht aus. Er gibt sie bei jedem Lauf
und in der Meldung an den Empfänger zurück. Die gesamte Auftragsanfrage
einschließlich Profil, Anzahl und Zusatzdaten darf höchstens 2048 Bytes groß sein.

## 4. Kabel prüfen

1. Sicherstellen, dass ein Auftrag aktiv ist.
2. Beide Kabelenden an Seite A und Seite B anschließen.
3. Den externen Schalter einmal bewusst umschalten. Beide Schaltrichtungen
   können einen Lauf starten.
4. Das Ergebnis abwarten: Grün leuchtet dauerhaft für OK, Rot für FEHLER.
5. Auf der Webseite **„Letzte Messung“** und den Auftragsfortschritt prüfen.
6. Das Kabel abziehen und warten, bis die Ergebnis-LED nicht mehr dauerhaft
   leuchtet. Bei weiter aktivem Auftrag blinkt Grün langsam.
7. Das nächste Kabel anschließen und erneut den Schalter betätigen.

Die Webseite aktualisiert ihren Status automatisch etwa jede Sekunde;
Neuladen ist normalerweise nicht nötig. Ein erneuter Schalterwechsel bei
noch angezeigtem Ergebnis erzeugt keinen weiteren Lauf. Erst das Abziehen
gibt den nächsten Test frei.

Nach dem letzten Lauf erscheint **„Kein Auftrag aktiv“**. Das letzte
Ergebnis bleibt auf der Webseite und in der Laufliste. Die Ergebnis-LED
leuchtet bis zum Abziehen weiter; danach blitzt Grün im Bereitschaftszustand
kurz alle drei Sekunden.

Ohne aktiven Auftrag führt ein Schalterwechsel keine Messung aus und
erzeugt keinen Eintrag in der Laufliste.

## 5. Auftrag abbrechen

Im Bereich **„Aktueller Auftrag“** auf **„Auftrag abbrechen“** klicken.
Danach muss **„Kein Auftrag aktiv“** erscheinen. Bereits gespeicherte
Ergebnisse werden nicht gelöscht.

Wird ein Auftrag während einer Messung abgebrochen, wird deren Ergebnis
am Ende verworfen, sofern der Abbruch vor dem Speichern wirksam wurde.
Bereits gespeicherte Läufe bleiben erhalten. Bereits vorgemerkte Meldungen
können nach dem Abbruch noch versendet werden.

Falls die Ergebnis-LED noch dauerhaft leuchtet, das Kabel vor einer
weiteren Messung abziehen und das Ende dieser Anzeige abwarten.

## 6. Ergebnisse und Empfängermeldungen verstehen

Unter **„Letzte 20 Tests“** steht die neueste Messung oben. Jede Zeile zeigt
Auftragsnummer, Laufnummer, OK/FEHLER, Meldestatus, Profil und Zusatzdaten.

**OK/FEHLER** beschreibt das Kabel. **Gemeldet/Nicht gemeldet** beschreibt
den Versand an einen anderen Server. Ein Kabel kann OK sein, obwohl seine
Meldung nicht angenommen wurde.

Nach jeder gespeicherten Messung versucht der ESP, das Ergebnis im
Hintergrund an die konfigurierte Empfänger-URL zu senden. Der spätere
Empfänger ist ein eigener Server; die Kabeltester-Webadresse ist nicht
automatisch diese Empfänger-URL.

| Anzeige auf der Webseite | Bedeutung |
|---|---|
| Meldeempfänger noch nicht geprüft | Seit dem Neustart liegt noch kein Versandresultat vor. |
| Meldeempfänger erreichbar | Der zuletzt bearbeitete Versandversuch wurde mit HTTP 2xx bestätigt. |
| Meldeempfänger nicht erreichbar | Der zuletzt bearbeitete Versandversuch scheiterte, beispielsweise beim Verbindungsaufbau. |
| Meldeempfänger hat die Meldung nicht angenommen | Der Empfänger antwortete mit einem Status außerhalb von 200–299. |

Der Status ist keine dauernde Erreichbarkeitsprüfung. **„Nicht gemeldet“**
kann direkt nach dem Test auch bedeuten, dass der Versand noch aussteht.
Fehlgeschlagene Meldungen werden derzeit nicht automatisch wiederholt.
Die Messungen und der Auftragszähler funktionieren bei einem unerreichbaren
Empfänger weiter. Für die vorgesehene Abnahme läuft an der Empfänger-URL
absichtlich kein Server; die entsprechende Anzeige ist dabei erwartet.

Es werden höchstens 20 Läufe im RAM gespeichert. Danach wird jeweils der
älteste verdrängt. Beim Neustart gehen Auftrag, Laufliste, letztes Ergebnis
und wartende Meldungen verloren. Das Neuladen des Browsers löscht sie nicht.

## 7. LED-Anzeigen

| Zustand | LED |
|---|---|
| Kein WLAN | Rot blinkt langsam, etwa 1 Hz. |
| Bereit: WLAN verbunden, kein Auftrag | Grün blitzt kurz alle drei Sekunden. |
| Auftrag aktiv, wartet auf Kabel | Grün blinkt langsam, etwa 1 Hz. |
| Ergebnis OK | Grün leuchtet dauerhaft bis zum Abziehen. |
| Ergebnis FEHLER | Rot leuchtet dauerhaft bis zum Abziehen. |
| Schalter ohne Auftrag betätigt | Rot blinkt schnell, etwa 5 Hz, für drei Sekunden. |

Ein angezeigtes Messergebnis hat Vorrang vor allen anderen Zuständen.
Ohne Ergebnis hat die Warnung für fehlendes WLAN Vorrang vor der Warnung
„kein Auftrag“. Die Empfänger-Erreichbarkeit hat keinen eigenen LED-Zustand;
sie wird auf der Webseite angezeigt.

## 8. Bedienung über die API mit curl

Die folgenden Befehle in einem PowerShell-Terminal ausführen. Es muss kein
ESP-IDF-Monitor laufen. Zuerst die Geräteadresse festlegen:

```powershell
$baseUrl = 'http://kabeltester.local'
```

Falls nötig, stattdessen die aktuelle IP aus dem Monitor einsetzen:

```powershell
$baseUrl = 'http://kabeltester.local'
```

`-i` zeigt den HTTP-Status und die Antwortheader. Die unten gezeigten
Antworten sind Beispiele; Nummern, Fortschritt und Empfängerstatus können
abweichen. Alle GET-Befehle lesen Daten, ohne den Auftrag zu verändern.

### Standardprofile: GET /api/profiles

```powershell
curl.exe -i "$baseUrl/api/profiles"
```

**200 OK**

```json
{"profiles":[{"name":"gerade8","profile":[0,1,2,3,4,5,6,7]},{"name":"gerade4","profile":[0,1,2,3,-1,-1,-1,-1]},{"name":"gekreuzt01","profile":[1,0,2,3,4,5,6,7]},{"name":"asymmetrisch","profile":[1,2,0,3,4,5,6,-1]}]}
```

### Auftrag anlegen: POST /api/jobs

Zunächst eine Beispieldatei im aktuellen Terminalordner erstellen. Dieser
Befehl legt `auftrag-api.json` an bzw. ersetzt diese Datei; er verändert
den ESP noch nicht:

```powershell
'{"profile":[0,1,2,3,4,5,6,7],"count":3,"args":{"charge":"A17"}}' | Set-Content -LiteralPath .\auftrag-api.json -Encoding ascii
```

Das Beispiel prüft drei gerade Achtaderkabel. Für vier gerade Adern das
Profil durch `[0,1,2,3,-1,-1,-1,-1]` ersetzen. Danach den Auftrag senden:

```powershell
curl.exe -i -X POST "$baseUrl/api/jobs" -H "Content-Type: application/json" --data-binary "@auftrag-api.json"
```

**201 Created**

```json
{"job_id":1,"status":"active","completed":0,"count":3}
```

Bei einem zweiten POST während eines aktiven Auftrags: **409 Conflict**

```json
{"error":"job_already_active","message":"Es ist bereits ein Auftrag aktiv."}
```

`profile` muss acht ganze Zahlen zwischen −1 und 7 enthalten. `count` muss
eine ganze Zahl von 1 bis 4294967295 sein. `args` ist optional und muss,
falls angegeben, ein JSON-Objekt sein. Ohne `args` wird `{}` verwendet.
Ungültige Angaben führen zu **400 Bad Request**.

### Fortschritt: GET /api/jobs/current

```powershell
curl.exe -i "$baseUrl/api/jobs/current"
```

Beispiel nach einem gespeicherten Lauf: **200 OK**

```json
{"job":{"job_id":1,"status":"active","completed":1,"count":3,"profile":[0,1,2,3,4,5,6,7],"args":{"charge":"A17"}},"receiver_status":"unreachable"}
```

Nach Ende oder Abbruch des Auftrags ebenfalls **200 OK**:

```json
{"job":null,"receiver_status":"unreachable"}
```

Mögliche Werte für `receiver_status` sind `unknown`, `reachable`,
`unreachable` und `error`; sie entsprechen den Webanzeigen aus Abschnitt 6.

### Abbruch: DELETE /api/jobs/current

Dieser Befehl beendet den aktiven Auftrag:

```powershell
curl.exe -i -X DELETE "$baseUrl/api/jobs/current"
```

**200 OK**

```json
{"job_id":1,"status":"cancelled"}
```

Ohne aktiven Auftrag: **404 Not Found**

```json
{"error":"no_active_job","message":"Es ist kein Auftrag aktiv."}
```

### Laufliste: GET /api/runs

```powershell
curl.exe -i "$baseUrl/api/runs"
```

**200 OK**

```json
{"runs":[{"job_id":1,"lauf":1,"result":"OK","gemeldet":false,"profile":[0,1,2,3,4,5,6,7],"args":{"charge":"A17"}}]}
```

Ohne gespeicherte Messungen:

```json
{"runs":[]}
```

Die API liefert den ältesten noch gespeicherten Lauf zuerst. Die Webseite
kehrt die Reihenfolge um. `gemeldet:true` bedeutet, dass der Empfänger
den Versand mit HTTP 2xx bestätigt hat.

### Letztes Ergebnis: GET /api/result

```powershell
curl.exe -i "$baseUrl/api/result"
```

**200 OK**, mit einer der drei Varianten:

```json
{"result":"OK"}
```

```json
{"result":"FEHLER"}
```

```json
{"result":"Noch kein Test"}
```

Dieses Ergebnis enthält keine Auftragsnummer. Für die Zuordnung zu einem
Auftrag die Laufliste verwenden.

### Startseite: GET /

```powershell
curl.exe -i "$baseUrl/"
```

**200 OK**, `Content-Type: text/html; charset=utf-8`, anschließend der
HTML-Inhalt der Webseite. Für die normale Bedienung diese Adresse im
Browser öffnen.

## 9. Format der Meldung an den Empfänger

Die Firmware sendet jeden gespeicherten Lauf im Hintergrund als HTTP-POST
mit `Content-Type: application/json` an die konfigurierte `REPORT_URL`:

```json
{"job_id":1,"lauf":1,"result":"OK","profile":[0,1,2,3,4,5,6,7],"measured_profile":[0,1,2,3,4,5,6,7],"args":{"charge":"A17","gemessen":[0,1,2,3,4,5,6,7]}}
```

Der Empfänger muss mit HTTP 200–299 antworten, damit der Lauf als gemeldet
markiert wird. Ein Antwortinhalt wird nicht ausgewertet. Die privaten
WLAN-Daten, Hostname und Empfänger-URL stehen in `config/flash_config.h`;
Änderungen erfordern derzeit einen neuen Build und das Flashen über USB.
Es gibt noch keinen Firmware-Upload über die Webseite.

Die detaillierte Beschreibung von Anfragegrenzen und Fehlercodes steht im
Repo in `docs/API-Entwurf.md`. Die neue Spalte **Gemessenes Profil** zeigt
das Messprofil als Array, zum Beispiel `[1,2,3,4,-1,-1,-1,-1]`.
Eine Zahl bezeichnet das B-Ziel; `-1` bedeutet offen. Mehrere Ziele werden
als Liste im Array angezeigt, etwa `[[1,2],-1,-1,-1,-1,-1,-1,-1]`.
**Zusatzdaten** enthält deine bisherigen Angaben und zusätzlich `gemessen`
mit demselben Array. Diese Daten gehen auch an den Meldeempfänger.
Bei FEHLER enthält die API zusätzlich `errors`, `comparison_direction`
und `error_count`. `gemessen` ist als Zusatzdatenname für den Tester reserviert.
Die Firmware muss dafür nach dem Build neu übertragen werden.

## 10. Häufige Probleme

Im Normalbetrieb zeigt der Monitor Ergebnis und Fehlerdetails, aber keine
64 Einzelwerte pro Messung. Zur Fehlersuche in `main/main.c` die Einstellung
`MESSDIAGNOSE` von `0` auf `1` setzen, neu bauen und über USB übertragen.
Dann werden zusätzlich alle A–B-Messwerte mit GPIO-Nummern ausgegeben.
Die gespeicherten Messprofile, Webseite und Servermeldung werden durch
diese Einstellung nicht verändert.

| Beobachtung | Was prüfen? |
|---|---|
| `kabeltester.local` wird nicht gefunden | PC/Handy mit dem WLAN des ESP verbinden. Bei angeschlossenem LAN den tatsächlichen WLAN-Zustand prüfen. Alternativ aktuelle ESP-IP verwenden. |
| Webseite zeigt „ESP nicht erreichbar“ | Stromversorgung, WLAN-Verbindung und Geräteadresse prüfen. Die Seite versucht die Abfrage automatisch erneut. |
| Schalter startet keine Messung | Aktiven Auftrag prüfen. Ein noch angeschlossenes Kabel nach einer Messung erst abziehen und das Ende der dauerhaften Ergebnisanzeige abwarten. |
| Rot leuchtet dauerhaft | FEHLER: Profil und tatsächliche Kabelverbindungen vergleichen. Für acht gerade Adern `gerade8` wählen. |
| Rot blinkt schnell | Schalter wurde ohne Auftrag betätigt. Zuerst einen Auftrag anlegen. |
| Rot blinkt langsam | WLAN fehlt, sofern kein Ergebnis angezeigt wird. Nach Rückkehr des WLAN verbindet sich der ESP automatisch erneut. |
| Grün blinkt oder blitzt nach dem Abziehen | Normal: aktiver Auftrag bzw. Bereitschaft. Die LEDs müssen dann nicht vollständig aus bleiben. |
| Auftrag endet trotz FEHLER | Normal: Auch FEHLER zählt als geprüftes Kabel. |
| „Meldeempfänger nicht erreichbar“ bei Ergebnis OK | Kabelprüfung und Versand sind getrennt. Empfänger-URL/Server prüfen; die Messung kann fortgesetzt werden. |
| Ältere Ergebnisse fehlen | Es bleiben höchstens 20 Läufe erhalten. Neustart löscht den gesamten RAM-Zustand. |

Ein vollständig unterbrochenes Kabel ist für die Erkennung des Abziehens
nicht von einem abgezogenen Kabel unterscheidbar. Seine dauerhafte
FEHLER-Anzeige kann deshalb enden; das gespeicherte Ergebnis bleibt erhalten.

## 11. Schneller Funktionstest

1. Gerät einschalten und die Webseite öffnen.
2. `gerade8`, Anzahl `1`, Zusatzdaten nach Bedarf wählen und Auftrag anlegen.
3. Ein bekanntes gerades Achtaderkabel anschließen und den externen Schalter
   betätigen: Erwartet sind OK und eine dauerhaft grüne Ergebnis-LED.
4. Prüfen, dass ein Lauf in „Letzte 20 Tests“ steht und kein Auftrag mehr
   aktiv ist. Bei fehlendem Empfänger ist „Nicht gemeldet“ erwartet.
5. Kabel abziehen: Grün blitzt anschließend kurz alle drei Sekunden.

Dieser Ablauf ist eine Bedienungsprobe. Die vollständigen Abnahmefälle aus
der Projektaufgabe müssen zusätzlich mit den vorgesehenen Kabeln und
Profilen durchgeführt werden.
