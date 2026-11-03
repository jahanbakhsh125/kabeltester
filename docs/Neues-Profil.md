# Neues Profil anlegen

## 1. Was ein Profil beschreibt

Ein Profil enthält genau acht ganze Zahlen zwischen −1 und 7. Die Position
ist der A-Anschluss, der Wert der erwartete B-Anschluss. Nummeriert wird
von 0 bis 7. `-1` bedeutet: Dieser A-Anschluss darf mit keinem B-Anschluss
verbunden sein.

Beispiel: `[0,1,2,3,-1,-1,-1,-1]` verlangt vier gerade Verbindungen
A0–B0 bis A3–B3; A4–A7 bleiben offen. Zusätzliche Verbindungen führen zu FEHLER.
Für gewöhnliche Einzelader-Kabel jeden B-Anschluss höchstens einmal zuordnen;
die API überprüft Wertebereich und Ganzzahligkeit, aber keine doppelte B-Zuordnung.

Die aktuelle Netzwerkfirmware verwendet das Profil des aktiven Auftrags.
Es gibt zwei Wege: ein eigenes Profil für einen Auftrag eingeben oder ein
neues Standardprofil in die Firmware aufnehmen.

## 2. Eigenes Profil ohne Codeänderung verwenden

1. Webseite des Kabeltesters öffnen und sicherstellen, dass kein Auftrag aktiv ist.
2. Unter „Verbindungen A0 bis A7“ die acht Werte eingeben, beispielsweise:

   ```text
   0,1,2,3,-1,-1,-1,-1
   ```

3. Gewünschte Anzahl einstellen, für den ersten Test `1`.
4. Optionale Zusatzdaten eintragen und „Auftrag anlegen“ wählen.
5. Ein zum Profil passendes Kabel anschließen und den externen Schalter betätigen.

Das Profil gilt für diesen Auftrag. Dieser Weg benötigt keinen Build und
kein Flashen, erzeugt aber keinen dauerhaft gespeicherten Eintrag im
Auswahlfeld „Standardprofil“. Auftrag und Läufe gehen beim Neustart verloren.

Die gleichen Werte können per API im Feld `profile` übergeben werden;
Beispiele stehen in [API-Entwurf.md](API-Entwurf.md).

## 3. Ein neues Standardprofil in der Firmware hinterlegen

In `main/main.c` die Tabelle `static const kabelprofil_t profile[]` suchen.
Vor deren schließender Klammer eine neue Zeile ergänzen:

```c
    {"mein_vieraderprofil", {0, 1, 2, 3, -1, -1, -1, -1}},
```

Diese Zeile legt den Namen für die Webauswahl und die acht erwarteten
Verbindungen fest. Die Werte sind ein Beispiel und müssen zur eigenen
Kabelverdrahtung passen. Einen eindeutigen Namen verwenden.

Die aktuell hinterlegten Standardprofile sind:

| Name | Verbindungen |
|---|---|
| gerade8 | `[0,1,2,3,4,5,6,7]` |
| gerade4 | `[0,1,2,3,-1,-1,-1,-1]` |
| gekreuzt01 | `[1,0,2,3,4,5,6,7]` |
| asymmetrisch | `[1,2,0,3,4,5,6,-1]` |

Die API liest die Standardprofile aus dieser Tabelle. Die Webseite holt sie
über `GET /api/profiles`; eine zusätzliche HTML-Änderung ist nicht erforderlich.

Die frühere Einstellung `AKTIVES_PROFIL` wurde aus dem Netzwerkcode entfernt.
Das tatsächlich gemessene Profil kommt aus dem Auftrag. Daher für diesen
Ablauf keinen Tabellenindex festlegen. Bei `BEIDE_RICHTUNGEN 1` akzeptiert
die Messlogik außerdem vertauschte Kabelenden; diese globale Einstellung
bleibt beim Ergänzen eines Profils bestehen.

## 4. Bauen, übertragen und auswählen

Nach einer Änderung der Profiltabelle die Datei speichern.
Den Monitor zuerst mit `Ctrl+]` schließen, damit COM5 frei ist.
Im eingerichteten ESP-IDF-Terminal im Projektordner:

```powershell
idf.py -p COM5 build flash monitor
```

Den tatsächlichen Port verwenden. Ein Neustart durch Flashen löscht bestehende
Aufträge und Ergebnisse; die Änderung daher außerhalb laufender Prüfungen
übertragen. Bei eingerichtetem ESP32-S3-Projekt ist erneutes `set-target`
für eine reine Profiländerung nicht erforderlich.

Nach der WLAN-Verbindung die Webseite neu laden. Das neue Profil muss im
Auswahlfeld „Standardprofil“ erscheinen. Alternativ prüfen:

```powershell
curl.exe -i "http://kabeltester.local/api/profiles"
```

Jetzt das neue Profil wählen, einen Auftrag mit Anzahl `1` anlegen und
das passende Kabel prüfen. Es gibt keine erforderliche Startmeldung
„Aktives Profil 4“ in der aktuellen Netzwerkfirmware.

## 5. Profil prüfen

- Mit passender Verdrahtung prüfen: erwartet OK, Grün leuchtet dauerhaft
  bis zum Abziehen.
- Kabel abziehen und das Ende der dauerhaften Ergebnisanzeige abwarten.
- Einen neuen Auftrag mit demselben Profil und Anzahl `1` anlegen.
- Einen passenden Testadapter mit einer unterbrochenen belegten Ader verwenden:
  erwartet FEHLER und dauerhaft rote Ergebnis-LED.
- Änderungen am Testadapter vor der Messung durchführen, niemals währenddessen.
  An der fertigen PCB muss dafür nichts umgelötet werden.

Nach dem Abziehen zeigt die LED wieder den aktuellen Zustand: langsames
grünes Blinken bei aktivem Auftrag oder ein kurzer grüner Blitz alle drei
Sekunden ohne Auftrag. Das letzte Ergebnis bleibt in der Laufliste.

Ergebnisse im [Abnahmeprotokoll](Abnahme.md) festhalten. Ein bestandener
Build allein bestätigt noch nicht die reale Kabelverdrahtung.

## 6. Eine getestete Standardprofiländerung in Git speichern

Nach einem tatsächlich erfolgreichen Test:

```powershell
git diff -- main/main.c
git add main/main.c
git commit -m "Neues Kabelprofil ergänzt"
```

Vor `git add` den Diff prüfen: Die gesamte Datei kann auch andere noch
ungespeicherte Änderungen enthalten. Die private Flash-Konfiguration
`config/flash_config.h` bleibt ausgeschlossen.
