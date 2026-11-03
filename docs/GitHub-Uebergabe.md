# GitHub: bereinigte Quellcode-Kopie

Diese Kopie enthält Quellcode, Tests und anonymisierte Dokumentation.
Die ursprüngliche Git-Historie, Repository-Verbindung, private Flash-Konfiguration,
lokale Build-Dateien, Protokolle und Sicherungsarchive sind nicht enthalten.
Die alte Commit-Historie darf nicht in dieses neue Repository übernommen werden.

## Lokale Einrichtung

1. Den Projektordner in VS Code öffnen und ESP-IDF lokal einrichten.
2. `config/flash_config.example.h` nach `config/flash_config.h` kopieren.
3. Nur in der lokalen Datei WLAN-Zugangsdaten eintragen; die Datei ist ausgeschlossen.
4. Den Build wie in der README beschrieben ausführen.

## Neues Repository vorbereiten

Vor dem ersten Commit den gewünschten öffentlichen Git-Namen und eine
GitHub-No-Reply-E-Mail als lokale Git-Identität konfigurieren. Andernfalls
können die globalen Git-Einstellungen persönliche Angaben in neue Commits schreiben.

```powershell
git init
git config user.name "DEIN_OEFFENTLICHER_NAME"
git config user.email "DEINE_GITHUB_NOREPLY_ADRESSE"
git add .
python tests/git-pruefung.py
git diff --cached --stat
```

Die beiden Platzhalter vor dem Commit durch die gewünschte öffentliche Identität ersetzen.
Die im GitHub-Konto angezeigte No-Reply-Adresse verwenden.
Das Ziel-Repository wird anschließend selbst gewählt; diese Kopie enthält kein Remote.
Es wurde kein Upload durchgeführt.

Auch ignorierte Dateien können versehentlich in eine neue ZIP gelangen.
Später erzeugte Firmware-Dateien können die selbst eingetragenen WLAN-Zugangsdaten enthalten.
Für die Veröffentlichung ausschließlich geprüfte Quellcode-Dateien verwenden.
