# API-Entwurf: Kabeltester im Netzwerk

Stand: 08.10.2026. Diese Dokumentation beschreibt die aktuell implementierte
Firmware. Eine Freigabe durch den Ansprechpartner ist damit nicht dokumentiert.

## Zweck und Zugriff

Die HTTP-API erlaubt einem lokalen Programm, Aufträge anzulegen und den
Fortschritt sowie Messergebnisse abzufragen. Die Webseite verwendet dieselbe API.
Eine Messung startet weiterhin am externen Schalter des Kabeltesters; es gibt
keinen API-Endpunkt zum Auslösen einer Messung.

Die folgenden Beispiele sind für PowerShell im VS-Code-Terminal gedacht:

```powershell
$baseUrl = 'http://kabeltester.local'
```

Wenn der Name nicht aufgelöst wird, stattdessen die aktuelle IP aus dem
ESP-Monitor verwenden, beispielsweise:

```powershell
$baseUrl = 'http://kabeltester.local'
```

Beide Adressen führen zum selben Gerät. `kabeltester.local` wurde mit einem PC
im WLAN des ESP getestet, auch bei zusätzlich eingestecktem LAN-Kabel. Über
ein anderes Netz kann die IP erreichbar sein, während die mDNS-Namenssuche fehlt.
Hostname und Empfänger-URL stehen in der privaten Flash-Konfiguration
`config/flash_config.h`. Zugangsdaten gehören nicht in diese Dokumentation.

API-Antworten verwenden `Content-Type: application/json` und
`Cache-Control: no-store`. Ausnahme: ein Timeout beim Empfang eines
Auftrags kann eine vom HTTP-Server erzeugte Antwort mit Status 408 liefern.
Die Startseite `GET /` liefert HTML. Login und Verschlüsselung sind derzeit
nicht implementiert; der Betrieb ist für das lokale Netz vorgesehen.

## Endpunkte im Überblick

| Methode | Pfad | Aufgabe | Erfolg |
|---|---|---|---|
| GET | `/api/profiles` | Standardprofile lesen | 200 |
| POST | `/api/jobs` | Neuen Auftrag anlegen | 201 |
| GET | `/api/jobs/current` | Aktiven Auftrag und Empfängerstatus lesen | 200 |
| DELETE | `/api/jobs/current` | Aktiven Auftrag abbrechen | 200 |
| GET | `/api/runs` | Gespeicherte Läufe lesen | 200 |
| GET | `/api/result` | Letztes Gesamtergebnis lesen | 200 |

## Bedeutung der Daten

| Feld | Bedeutung |
|---|---|
| `profile` | Genau acht ganze Zahlen. Position 0–7 bezeichnet A0–A7; Wert 0–7 bezeichnet den erwarteten B-Anschluss. −1 erwartet keine Verbindung für diesen A-Anschluss. |
| `count` | Anzahl zu prüfender Kabel, ganze Zahl von 1 bis 4294967295. |
| `args` | Optionales JSON-Objekt mit Zusatzdaten, etwa einer Charge. Ohne Angabe wird `{}` verwendet. `null`, Text und Arrays sind hier nicht erlaubt. |
| `job_id` | Vom ESP vergebene Auftragsnummer, beginnend bei 1 nach einem Neustart. |
| `completed` | Anzahl gespeicherter Messungen im aktuellen Auftrag. OK und FEHLER zählen jeweils als ein Lauf. |
| `lauf` | Laufnummer innerhalb eines Auftrags, beginnend bei 1. |
| `result` | Gesamtergebnis `OK` oder `FEHLER`; bei `/api/result` zusätzlich `Noch kein Test`. |
| `gemeldet` | `true`, sobald der Empfänger die Meldung mit HTTP 2xx beantwortet hat; andernfalls `false`. |

Ein gerades Achtaderkabel passt zu `[0,1,2,3,4,5,6,7]`. Ein Vieraderprofil
`[0,1,2,3,-1,-1,-1,-1]` erwartet keine zusätzlichen Verbindungen auf A4–A7;
ein vollständig verbundenes Achtaderkabel wird damit als FEHLER bewertet.
Die Firmware akzeptiert auch vertauschte Kabelenden, wenn die gemessenen
Verbindungen der umgekehrten Profilrichtung entsprechen.

Der ESP übernimmt `args` in den Auftrag. In Laufliste und Meldung ergänzt er
`args.gemessen` mit dem Messprofil; dieser Name ist dafür reserviert.
Alle anderen Angaben bleiben erhalten. Leerzeichen und Darstellung können bei der erneuten
JSON-Ausgabe anders aussehen; es ist keine bytegleiche Textkopie.

## 1. Standardprofile lesen

```powershell
curl.exe -i "$baseUrl/api/profiles"
```

Antwort: **200 OK**

```json
{
  "profiles": [
    {"name": "gerade8", "profile": [0,1,2,3,4,5,6,7]},
    {"name": "gerade4", "profile": [0,1,2,3,-1,-1,-1,-1]},
    {"name": "gekreuzt01", "profile": [1,0,2,3,4,5,6,7]},
    {"name": "asymmetrisch", "profile": [1,2,0,3,4,5,6,-1]}
  ]
}
```

Die Namen dienen als Vorlagen für die Webseite. Beim Anlegen eines Auftrags
wird das Profil-Array gesendet, nicht der Profilname. Die Standardprofile sind
in der Firmware einkompiliert.

## 2. Auftrag anlegen

Im Projektordner enthält `auftrag-test.json` dieses Beispiel für drei
Messungen eines geraden Achtaderkabels:

```json
{
  "profile": [0,1,2,3,4,5,6,7],
  "count": 3,
  "args": {"charge": "A17"}
}
```

```powershell
curl.exe -i -X POST "$baseUrl/api/jobs" -H "Content-Type: application/json" --data-binary "@auftrag-test.json"
```

Antwort: **201 Created**; die Auftragsnummer kann abweichen.

```json
{"job_id":1,"status":"active","completed":0,"count":3}
```

Der gesamte Anfrageinhalt darf höchstens 2048 Bytes groß sein. `profile`
und `count` sind Pflichtfelder. Es kann höchstens ein Auftrag aktiv sein.
Das Anlegen startet noch keine Messung: Kabel anschließen und externen
Schalter betätigen. Vor jedem weiteren Lauf das Kabel abziehen und warten,
bis die Ergebnis-LED nicht mehr dauerhaft leuchtet.

Wird derselbe Befehl bei aktivem Auftrag erneut ausgeführt, bleibt der
bisherige Auftrag erhalten. Antwort: **409 Conflict**

```json
{"error":"job_already_active","message":"Es ist bereits ein Auftrag aktiv."}
```

## 3. Aktuellen Auftrag und Fortschritt lesen

```powershell
curl.exe -i "$baseUrl/api/jobs/current"
```

Beispiel nach einer Messung: **200 OK**

```json
{
  "job": {
    "job_id": 1,
    "status": "active",
    "completed": 1,
    "count": 3,
    "profile": [0,1,2,3,4,5,6,7],
    "args": {"charge": "A17"}
  },
  "receiver_status": "unreachable"
}
```

Ohne Auftrag, nach Abbruch oder nach der dritten gespeicherten Messung:

```json
{"job":null,"receiver_status":"unreachable"}
```

Auch ohne Auftrag ist dies **200 OK**. Ein abgeschlossener Auftrag wird
hier nicht als `completed`-Status aufbewahrt; seine Läufe stehen unter
`/api/runs`.

| `receiver_status` | Bedeutung |
|---|---|
| `unknown` | Seit dem Neustart liegt noch kein abgeschlossenes Versandresultat vor. |
| `reachable` | Die zuletzt bearbeitete Meldung wurde mit HTTP 2xx beantwortet. |
| `unreachable` | Der letzte Versandversuch scheiterte, etwa beim Verbindungsaufbau. |
| `error` | Es kam eine HTTP-Antwort, aber mit einem Status außerhalb von 200–299. |

Der Empfängerstatus beschreibt den letzten bearbeiteten Versandversuch,
keine laufende Erreichbarkeitsprüfung. Er gilt geräteweit, auch wenn kein
Auftrag aktiv ist. Direkt nach einer Messung kann er sich noch ändern,
weil der Versand im Hintergrund läuft.

## 4. Auftrag abbrechen

```powershell
curl.exe -i -X DELETE "$baseUrl/api/jobs/current"
```

Antwort bei aktivem Auftrag: **200 OK**

```json
{"job_id":1,"status":"cancelled"}
```

Danach liefert `GET /api/jobs/current` `"job":null`. Bereits gespeicherte
Läufe bleiben erhalten. Bereits eingereihte Meldungen können weiterhin
versendet werden. Eine laufende Messung wird am Ende verworfen, wenn ihr
Auftrag inzwischen abgebrochen wurde; sie wird dann nicht gespeichert
und nicht gemeldet.

Ein weiterer Abbruch ohne aktiven Auftrag liefert **404 Not Found**:

```json
{"error":"no_active_job","message":"Es ist kein Auftrag aktiv."}
```

## 5. Laufliste lesen

```powershell
curl.exe -i "$baseUrl/api/runs"
```

Antwort: **200 OK**

```json
{
  "runs": [
    {
      "job_id": 1,
      "lauf": 1,
      "result": "OK",
      "gemeldet": false,
      "profile": [0,1,2,3,4,5,6,7],
      "args": {"charge": "A17", "gemessen": [0,1,2,3,4,5,6,7]},
      "measured_profile": [0,1,2,3,4,5,6,7]
    },
    {
      "job_id": 1,
      "lauf": 2,
      "result": "FEHLER",
      "gemeldet": false,
      "profile": [0,1,2,3,4,5,6,7],
      "args": {"charge": "A17", "gemessen": [0,1,2,3,4,5,6,-1]},
      "measured_profile": [0,1,2,3,4,5,6,-1],
      "errors": ["A7: erwartet B7; gemessen keine"],
      "comparison_direction": "normal",
      "error_count": 1
    }
  ]
}
```

Ohne gespeicherte Ergebnisse:

```json
{"runs":[]}
```

Die Liste enthält höchstens 20 Läufe über alle Aufträge hinweg. Der API-Endpunkt
liefert den ältesten noch vorhandenen Lauf zuerst; die Webseite zeigt die Liste
umgekehrt, mit dem neuesten zuerst. Der 21. gespeicherte Lauf verdrängt den
ältesten. Aufträge und Läufe liegen im RAM und gehen beim Neustart verloren.

`gemeldet:false` kann bedeuten, dass der Versand noch aussteht oder erfolglos
war. Es gibt aktuell keine automatische Wiederholung fehlgeschlagener
Meldungen. Ein Auftrag kann bereits beendet sein, während sein letzter Lauf
noch versendet wird. Bei FEHLER enthalten Laufliste und Empfängermeldung
zusätzlich die gemessenen Verbindungen und Fehlerdetails.

### Diagnose bei FEHLER

`profile` bleibt das Sollprofil. Bei jedem Lauf (OK und FEHLER) wird das
Messprofil in `measured_profile` und zusätzlich in `args.gemessen` ausgegeben.
Die anderen Zusatzdaten bleiben erhalten. Ein eigener Wert mit dem Namen
`gemessen` wird in der Ergebnis- und Servermeldung durch die Messung ersetzt;
der ursprüngliche Auftrag bleibt erhalten. Die Fehlerfelder stehen nur bei FEHLER:

| Feld | Bedeutung |
|---|---|
| `measured_profile` | Acht Einträge für A0 bis A7: eine Zahl 0–7 für ein B-Ziel, `-1` für offen, eine Liste wie `[0,1]` für mehrere B-Ziele. |
| `errors` | Eine Beschreibung je abweichendem A-Anschluss: erwartet und tatsächlich gemessen. |
| `comparison_direction` | `normal` oder `reversed`: die Vergleichsrichtung mit weniger Abweichungen; bei Gleichstand normal. |
| `error_count` | Anzahl fehlender plus zusätzlicher Verbindungen. Eine falsche Zuordnung kann zwei Abweichungen verursachen. |

Die Webseite zeigt **Profil**, **Gemessenes Profil** und **Zusatzdaten** in
getrennten Spalten. Zusatzdaten sind ein JSON-Objekt, zum Beispiel
`{"charge":"A17","gemessen":[1,2,3,4,-1,-1,-1,-1]}`.
Die gemessenen Verbindungen beschreiben immer A nach B, auch bei umgekehrter
Vergleichsrichtung. Der Server bekommt dieselben Diagnosefelder; nur
`gemeldet` wird ausschließlich in der Laufliste ausgegeben.

## 6. Letztes Ergebnis lesen

```powershell
curl.exe -i "$baseUrl/api/result"
```

Antwort: **200 OK**, jeweils eine der folgenden Varianten:

```json
{"result":"Noch kein Test"}
```

```json
{"result":"OK"}
```

```json
{"result":"FEHLER"}
```

Dieser Endpunkt liefert nur das letzte Gesamtergebnis ohne Auftragsnummer.
Das Abziehen des Kabels löscht es nicht. Für die Zuordnung zu Auftrag,
Profil und Zusatzdaten `/api/runs` verwenden.

## 7. Meldung vom ESP an den Empfänger

Die Empfänger-URL ist ein anderes Ziel als die Adresse der Kabeltester-Webseite.
Sie wird als `REPORT_URL` in der privaten Flash-Konfiguration festgelegt.
Der spätere Server stellt diese URL bereit; aktuell ist kein echter Empfänger
vorhanden. Für die Abnahme ist ausdrücklich eine nicht erreichbare Zieladresse
vorgesehen.

Nach einer gespeicherten Messung sendet der ESP im Hintergrund:

```http
POST <REPORT_URL>
Content-Type: application/json
```

Beispielinhalt:

```json
{
  "job_id": 1,
  "lauf": 1,
  "result": "OK",
  "profile": [0,1,2,3,4,5,6,7],
  "measured_profile": [0,1,2,3,4,5,6,7],
  "args": {"charge": "A17", "gemessen": [0,1,2,3,4,5,6,7]}
}
```

`gemeldet` wird nicht mitgesendet: Erst die Antwort des Empfängers bestimmt
diesen Wert. Jede HTTP-Antwort mit Status 200–299 gilt als Annahme; ihr Inhalt
wird nicht ausgewertet. Weiterleitungen werden nicht automatisch verfolgt.
Die Netzwerkoperationen verwenden einen Timeout von 1500 ms; das ist keine
Garantie für eine Gesamtdauer von höchstens 1500 ms pro Meldung.

Der Versand läuft getrennt von der Messung. Eine erfolglose Meldung verändert
weder OK/FEHLER noch den Fortschrittszähler. Die Versandwarteschlange nimmt
bis zu 20 wartende Meldungen auf. Bei voller Queue oder Speicherfehler kann
eine Meldung nicht vorgemerkt werden; der Lauf bleibt als nicht gemeldet
gespeichert. Beim Neustart gehen auch wartende Meldungen verloren.

## Fehlerantworten

| HTTP-Status | `error` | Ursache |
|---|---|---|
| 400 | `invalid_request` | Leere oder zu große Anfrage, ungültiges JSON oder ungültige Pflichtfelder/Zusatzdaten beim Anlegen. |
| 404 | `no_active_job` | Abbruch angefordert, obwohl kein Auftrag aktiv ist. |
| 409 | `job_already_active` | Ein zweiter Auftrag soll während eines aktiven Auftrags angelegt werden. |
| 500 | `out_of_memory` | Nicht genügend Speicher für Anfrage oder Antwort. |
| 500 | `internal_error` | Interner Zugriff auf den Auftragszustand fehlgeschlagen. |
| 503 | `job_ids_exhausted` | Alle Auftragsnummern seit dem Neustart verbraucht. |

Beispiel für unzulässige Werte: **400 Bad Request**

```json
{"error":"invalid_request","message":"Profil, count oder args ungueltig."}
```

Bei Empfangs-Timeout kann **408 Request Timeout** auftreten; dabei ist kein
JSON-Fehlerobjekt zugesichert. Unbekannte Pfade und unpassende Methoden werden
vom HTTP-Server behandelt und verwenden ebenfalls nicht zwingend dieses Schema.

## Kurzer manueller Test

1. Im Projektordner `$baseUrl` festlegen und `/api/jobs/current` abfragen.
2. Wenn kein Auftrag aktiv ist, mit `auftrag-test.json` einen Auftrag anlegen.
3. Denselben POST wiederholen: Erwartet ist 409; der erste Auftrag bleibt aktiv.
4. Ein gerades Achtaderkabel anschließen und den externen Schalter betätigen.
5. `/api/jobs/current` abfragen: `completed` ist 1. `/api/runs` enthält den Lauf.
6. Kabel abziehen; nach Ende der dauerhaften Ergebnisanzeige erneut anschließen
   und messen. Insgesamt drei Läufe durchführen.
7. Nach Lauf 3 ist `job` gleich `null`; die Ergebnisse bleiben in `/api/runs`.
8. Für den Abbruchtest einen neuen Auftrag anlegen, per DELETE abbrechen und
   den Status wieder abfragen. Ein zweites DELETE muss 404 liefern.

Die Beispielantworten illustrieren das Format und sind keine Behauptung,
dass alle Abnahmefälle bereits geprüft wurden. Eine bestätigte Annahme durch
einen echten Meldeempfänger wurde bisher nicht getestet.
