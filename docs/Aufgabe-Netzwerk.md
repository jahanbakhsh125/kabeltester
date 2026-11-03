# Aufgabe: Kabeltester im Netzwerk

Stand: 2026-10-01 · Version 1 · Ansprechpartner: Projektbetreuung · Frist: Fr 16.10.2026

> **MUSS** = Pflicht, wird abgenommen · **SOLL** = erwünscht, wenn machbar ·
> **OFFEN** = deine Entscheidung – begründe sie kurz

## Zweck

Der Kabeltester soll später von einem lokalen Server gesteuert werden: Der Server sagt, wie viele
Kabel gegen welches Profil zu prüfen sind, und bekommt jedes Ergebnis sofort gemeldet. Diese
Aufgabe baut die Schnittstelle dafür. Den Server gibt es noch nicht.

Baut auf „Kabeltester mit ESP32“ (2026-09-28) auf, im selben Repo. Messablauf (Start per Taste,
Ende beim Abziehen), Profil-Format und OK/FEHLER bleiben wie dort.

## Begriffe

- **Flash-Konfiguration**: Werte, die beim Flashen gesetzt werden: WLAN-Name, WLAN-Passwort,
  Hostname, URL des Meldeempfängers.
- **Auftrag**: ein Profil + Anzahl N + optional `args`. Heißt: „Prüfe die nächsten N Kabel gegen
  dieses Profil.“ Es ist höchstens ein Auftrag gleichzeitig aktiv.
- **Standardprofile**: benannte Profile, die auf dem ESP hinterlegt sind, z. B.
  `gerade4 = {0,1,2,3,-1,-1,-1,-1}`. Die Webseite bietet sie als Vorlage beim Anlegen eines
  Auftrags an.
- **Lauf**: eine einzelne Messung (ein Kabel) innerhalb eines Auftrags.
- **Meldung**: HTTP-Request, den der ESP nach jedem Lauf an den **Meldeempfänger** schickt.
- **args**: beliebiges, optionales JSON-Objekt im Auftrag. Der ESP wertet es nicht aus, sondern gibt
  es unverändert in jeder Meldung und in der Laufliste zurück. Beispiel: `{"charge": "A17"}`.
- **Laufliste**: alle Läufe seit dem letzten Neustart, mit Auftrag, Ergebnis und ob die Meldung
  angekommen ist.

## Anforderungen

**MUSS**
1. Die Flash-Konfiguration steht in einer Datei, die nicht ins git kommt. Eingecheckt ist nur eine
   Vorlage mit Platzhaltern.
2. Nach dem Einschalten verbindet sich der ESP selbstständig mit dem WLAN, auch nach einem
   Verbindungsabbruch. Solange keine Verbindung besteht, blinkt die rote LED.
3. Der ESP ist unter `<hostname>.local` erreichbar.
4. Über eine HTTP-API kann man:
   a. einen Auftrag anlegen (Profil-Array, N, optional `args`),
   b. den aktiven Auftrag und den Fortschritt abfragen (x von N),
   c. den aktiven Auftrag abbrechen,
   d. die Laufliste abfragen.
5. Solange ein Auftrag aktiv ist, wird ein neuer Auftrag mit Fehlermeldung abgelehnt.
6. Nach N Läufen ist der Auftrag beendet.
7. Ohne aktiven Auftrag misst der Tester nicht; ein Tastendruck zeigt „kein Auftrag“ (siehe 12).
8. Nach jedem Lauf geht eine Meldung an den Meldeempfänger. Sie enthält mindestens: Auftrag,
   laufende Nummer, Profil, OK/FEHLER und `args`.
9. Ist der Meldeempfänger nicht erreichbar, misst der Tester normal weiter. Der Lauf steht dann
   als „nicht gemeldet“ in der Laufliste, und die Webseite zeigt „Meldeempfänger nicht erreichbar“.
10. Die Laufliste liegt im RAM. Ist sie voll, fallen die ältesten Läufe heraus.
11. Der ESP liefert eine Webseite aus, über die man alles aus Punkt 4 bedienen kann. Beim Anlegen
    eines Auftrags bietet sie die Standardprofile zur Auswahl an.
12. Die LEDs zeigen genau diese Zustände (langsam ≈ 1 Hz, schnell ≈ 5 Hz):

    | Zustand | LED |
    |---|---|
    | Kein WLAN | rot blinkt langsam |
    | Bereit (WLAN da, kein Auftrag) | grün blitzt kurz alle 3 s |
    | Auftrag aktiv, wartet auf Kabel | grün blinkt langsam |
    | Ergebnis OK | grün leuchtet, bis das Kabel abgezogen wird |
    | Ergebnis FEHLER | rot leuchtet, bis das Kabel abgezogen wird |
    | Taste gedrückt ohne Auftrag | rot blinkt schnell für 3 s |

    Ein angezeigtes Ergebnis hat Vorrang vor allen anderen Zuständen.
13. Es gibt eine deutsche Bedienungsanleitung als Kapitel in einem bestehenden Buch in unserem
    einer Dokumentationsplattform. Sie richtet sich an jemanden, der den Tester nicht gebaut hat, und
    enthält: Bedienung der Webseite, jeden API-Endpunkt mit `curl`-Beispiel und Antwort, die
    LED-Tabelle aus 12.

**SOLL**
1. Die Fehlerdetails pro Ader stehen auch in der Meldung und in der Laufliste.

**OFFEN**
- Standardprofile einkompiliert oder im NVS
- In welches BookStack-Buch die Anleitung kommt
- API-Design: Pfade, JSON-Format, Statuscodes
- Format der Meldung
- Größe der Laufliste
- Webseite: Aussehen und Technik

**Nicht Teil der Aufgabe**
- Login oder Verschlüsselung (nur lokales Netz)
- WLAN ändern ohne neues Flashen
- Laufliste über einen Neustart retten
- Der spätere Server

## Material

- Wie in Teil 1 (Board mit roter und grüner LED, Buchsen, Testadapter)
- Die WLAN-Zugangsdaten für die Abnahme und einen BookStack-Zugang bekommst du von mir.

## Rückmeldung vor dem Start

Vor der Umsetzung, bevor du programmierst, schick mir:
1. Die Aufgabe in 3–5 eigenen Sätzen.
2. Einen API-Entwurf: jeden Endpunkt mit Beispiel-JSON, dazu das Format der Meldung.
   **Erst programmieren, wenn ich ihn freigegeben habe.** Der spätere Server baut darauf auf.
3. Offene Fragen – jede mit deinem Vorschlag.

## Abnahme

Im WLAN des Unternehmens. Die URL des Meldeempfängers zeigt auf eine Adresse, unter der kein Server läuft.

| # | Aufbau | Erwartet |
|---|---|---|
| 1 | Mit den WLAN-Daten flashen, einschalten | verbindet sich, `<hostname>.local` antwortet |
| 2 | WLAN aus, nach 1 Minute wieder an | rote LED blinkt, danach Verbindung ohne Neustart |
| 3 | Per `curl` einen Auftrag anlegen: Profil `{0,1,2,3,-1,-1,-1,-1}`, N = 3, args `{"charge":"A17"}`. Dann 3 Kabel prüfen: 2 korrekt, 1 fehlerhaft | 3 Läufe in der Laufliste (OK, OK, FEHLER), jeweils mit `args`; danach ist kein Auftrag mehr aktiv |
| 4 | Während ein Auftrag aktiv ist, einen zweiten anlegen | abgelehnt, der erste läuft weiter |
| 5 | Aktiven Auftrag abbrechen | Status: kein Auftrag aktiv |
| 6 | Über die Webseite einen Auftrag mit Standardprofil `gerade4` anlegen, N = 1, ein korrektes Kabel prüfen | OK |
| 7 | Webseite öffnen | zeigt „Meldeempfänger nicht erreichbar“; Läufe aus Fall 3 und 6 stehen als „nicht gemeldet“ in der Liste |
| 8 | Fall 3–5 über die Webseite statt `curl` | gleiches Ergebnis |
| 9 | Kein Auftrag aktiv, Kabel anstecken, Taste drücken | rot blinkt schnell, keine Messung, kein Eintrag in der Laufliste |
| 10 | Ich lege nur mithilfe der Anleitung per `curl` einen Auftrag an und frage die Laufliste ab | klappt ohne Rückfrage |

Spiel alle Fälle vorher selbst durch.

## Abgabe

- Code im selben lokalen Git-Repo wie Teil 1; zeig ihn mir bei der Abnahme.
- Link zum Anleitungs-Kapitel in BookStack.
- Im Repo: Vorlage der Flash-Konfiguration – aber nicht deine
  echte Konfiguration.

Hängst du länger als 1 Stunde fest: Frage + dein Vorschlag an mich.

## Änderungen

–
