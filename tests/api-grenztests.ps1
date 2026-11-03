param(
    [string]$BaseUrl = 'http://kabeltester.local',
    [ValidateSet('Alle', 'Fehler', 'Abbruch', 'Speicher')]
    [string]$Test = 'Alle',
    [ValidateRange(1, 3000)]
    [int]$AbbruchNachMs = 350
)

$ErrorActionPreference = 'Stop'
$BaseUrl = $BaseUrl.TrimEnd('/')

function Api([string]$Pfad, [string]$Methode = 'GET', [string]$Body = '') {
    $parameter = @{ Uri = "$BaseUrl$Pfad"; Method = $Methode; TimeoutSec = 10 }
    if ($Body -ne '') {
        $parameter.ContentType = 'application/json'
        $parameter.Body = $Body
    }
    Invoke-RestMethod @parameter
}

function Pruefen([bool]$Bedingung, [string]$Text) {
    if (-not $Bedingung) { throw $Text }
    Write-Host "OK: $Text" -ForegroundColor Green
}

function Ergebnisse {
    $antwort = Api '/api/runs'
    Pruefen ($null -ne $antwort.PSObject.Properties['runs']) 'Ergebnisliste vorhanden'
    $liste = @($antwort.runs)
    Pruefen ($liste.Count -le 20) 'Ergebnisspeicher bleibt auf 20 Eintraege begrenzt'
    foreach ($eintrag in $liste) {
        Pruefen (@($eintrag.profile).Count -eq 8 -and
                 $eintrag.args -is [PSCustomObject] -and
                 $eintrag.gemeldet -is [bool]) 'Profil, args und Meldestatus vorhanden'
    }
    return ,$liste
}

function Neuer-Auftrag([int[]]$Profil, [int]$Anzahl) {
    $zustand = Api '/api/jobs/current'
    if ($null -ne $zustand.job) {
        throw "Auftrag $($zustand.job.job_id) ist noch aktiv. Zuerst beenden oder bewusst abbrechen."
    }
    $body = @{ profile = $Profil; count = $Anzahl; args = @{ charge = 'Grenztest' } } |
        ConvertTo-Json -Depth 4 -Compress
    $neu = Api '/api/jobs' 'POST' $body
    $zustand = Api '/api/jobs/current'
    Pruefen ($null -ne $zustand.job -and $zustand.job.job_id -eq $neu.job_id -and
             $zustand.job.status -eq 'active' -and $zustand.job.completed -eq 0 -and
             $zustand.job.count -eq $Anzahl -and
             ($zustand.job.profile -join ',') -eq ($Profil -join ',')) 'Testauftrag korrekt angelegt'
    return $neu.job_id
}

function Messung-Abwarten([string]$Hinweis) {
    Write-Host "`n$Hinweis"
    Write-Host 'Kabel abziehen, warten bis die Ergebnis-LED nicht mehr dauerhaft leuchtet, wieder anschliessen und Schalter betaetigen.'
    $null = Read-Host 'Erst nach Ergebnis OK oder FEHLER im Monitor Enter druecken'
}

try {
    Write-Host 'Grenztests am vorhandenen geraden Achtaderkabel. Aktuelle Firmware mit Fehlerdiagnose erforderlich.'
    Write-Host 'ESP waehrend der Tests nicht neu starten. Monitor in einem zweiten Terminal offen lassen.'

    if ($Test -in @('Alle', 'Fehler')) {
        # Bewusster Profilfehler: vier erwartete Adern bei acht vorhandenen Adern.
        $id = Neuer-Auftrag @(0, 1, 2, 3, -1, -1, -1, -1) 1
        Messung-Abwarten 'FEHLER-Test: Das Vieraderprofil muss dein Achtaderkabel ablehnen.'
        $liste = Ergebnisse
        $eigene = @($liste | Where-Object { $_.job_id -eq $id })
        Pruefen ($eigene.Count -eq 1) 'Genau ein Ergebnis fuer den FEHLER-Test'
        Pruefen ($eigene[0].lauf -eq 1 -and $eigene[0].result -eq 'FEHLER') 'FEHLER korrekt gespeichert'
        Pruefen (($eigene[0].profile -join ',') -eq '0,1,2,3,-1,-1,-1,-1' -and
                 $eigene[0].args.charge -eq 'Grenztest') 'Profil und args bleiben nach Auftragsende erhalten'
        # Acht gerade Adern gegen gerade4: vier zusaetzliche Verbindungen.
        $diagnose = $eigene[0]
        Pruefen (@($diagnose.measured_profile).Count -eq 8) 'Gemessenes Profil mit acht A-Anschluessen vorhanden'
        for ($a = 0; $a -lt 8; $a++) {
            Pruefen ($diagnose.measured_profile[$a] -eq $a -and
                     $diagnose.args.gemessen[$a] -eq $a) "Messprofil und Zusatzdaten fuer A$a korrekt"
        }
        Pruefen ($diagnose.error_count -eq 4 -and @($diagnose.errors).Count -eq 4 -and
                 $diagnose.comparison_direction -eq 'normal') 'Vier Abweichungen mit normaler Vergleichsrichtung gespeichert'
        for ($a = 4; $a -lt 8; $a++) {
            Pruefen ($diagnose.errors[$a - 4] -eq "A${a}: erwartet keine; gemessen B$a") "Fehlerbeschreibung fuer A$a korrekt"
        }
        $zustand = Api '/api/jobs/current'
        Pruefen ($null -eq $zustand.job) 'Auch ein FEHLER zaehlt und beendet den Auftrag mit count 1'
    }

    if ($Test -in @('Alle', 'Abbruch')) {
        $id = Neuer-Auftrag @(0, 1, 2, 3, 4, 5, 6, 7) 3
        $vorher = Ergebnisse
        $vorherDaten = @($vorher | Select-Object job_id, lauf, result, profile, args)
        $vorherJson = ConvertTo-Json -InputObject $vorherDaten -Depth 8 -Compress
        Write-Host "`nABBRUCH-Test: Kabel abziehen, warten bis die Ergebnisanzeige endet und wieder anschliessen."
        Write-Host 'Hand an den Schalter legen. Beim Wort JETZT den Schalter EINMAL betaetigen.'
        Write-Host "Das Skript sendet DELETE $AbbruchNachMs ms nach JETZT. Der genaue Zeitpunkt ist wichtig."
        $null = Read-Host 'Bereit? Enter startet den Countdown'
        foreach ($zahl in @(3, 2, 1)) {
            Write-Host $zahl
            Start-Sleep -Seconds 1
        }
        Write-Host 'JETZT - Schalter betaetigen!' -ForegroundColor Yellow
        Start-Sleep -Milliseconds $AbbruchNachMs
        $abbruch = Api '/api/jobs/current' 'DELETE'
        Pruefen ($abbruch.job_id -eq $id -and $abbruch.status -eq 'cancelled') 'Testauftrag abgebrochen'

        # Eine leere Liste allein beweist nicht, dass die Messung schon gestartet war.
        Write-Host 'Im Monitor muessen Messung startet UND Messung verworfen - Auftrag nicht mehr aktiv stehen.'
        $bestaetigung = Read-Host 'Nur wenn beide Zeilen fuer diesen Versuch erschienen sind: verworfen eingeben'
        if ($bestaetigung.Trim().ToLowerInvariant() -ne 'verworfen') {
            throw 'Zeitpunkt nicht bestaetigt. Abbruch-Test mit -Test Abbruch wiederholen; noch kein bestandener Test.'
        }
        $nachher = Ergebnisse
        $nachherDaten = @($nachher | Select-Object job_id, lauf, result, profile, args)
        $nachherJson = ConvertTo-Json -InputObject $nachherDaten -Depth 8 -Compress
        Pruefen ($vorherJson -eq $nachherJson) 'Abgebrochene Messung hat die Ergebnisliste nicht veraendert'
        $zustand = Api '/api/jobs/current'
        Pruefen ($null -eq $zustand.job) 'Nach Abbruch kein aktiver Auftrag'
    }

    if ($Test -in @('Alle', 'Speicher')) {
        $id = Neuer-Auftrag @(0, 1, 2, 3, 4, 5, 6, 7) 21
        for ($lauf = 1; $lauf -le 21; $lauf++) {
            Messung-Abwarten "SPEICHER-Test: Messung $lauf von 21."
            $liste = Ergebnisse
            $eigene = @($liste | Where-Object { $_.job_id -eq $id })
            $erwartet = [Math]::Min($lauf, 20)
            Pruefen ($eigene.Count -eq $erwartet) "$erwartet Ergebnisse dieses Auftrags gespeichert"
            $erster = [Math]::Max(1, $lauf - 19)
            for ($i = 0; $i -lt $eigene.Count; $i++) {
                Pruefen ($eigene[$i].lauf -eq ($erster + $i) -and
                         $eigene[$i].result -eq 'OK') "Lauf $($erster + $i) in richtiger Reihenfolge mit OK"
                Pruefen (($eigene[$i].profile -join ',') -eq '0,1,2,3,4,5,6,7' -and
                         $eigene[$i].args.charge -eq 'Grenztest') 'Auftragsdaten beim Ueberschreiben erhalten'
            }
            $zustand = Api '/api/jobs/current'
            if ($lauf -lt 21) {
                Pruefen ($null -ne $zustand.job -and $zustand.job.job_id -eq $id -and
                         $zustand.job.completed -eq $lauf) 'Zaehler stimmt mit der Messung ueberein'
            } else {
                Pruefen ($null -eq $zustand.job) 'Auftrag nach 21 Messungen beendet'
                Pruefen ($liste.Count -eq 20 -and $eigene.Count -eq 20 -and
                         $eigene[0].lauf -eq 2 -and $eigene[-1].lauf -eq 21) `
                         'Nur Laeufe 2 bis 21 vorhanden: aeltester Lauf korrekt ueberschrieben'
            }
        }
    }
    Write-Host "`nAlle ausgewaehlten Grenztests bestanden." -ForegroundColor Green
} catch {
    Write-Host "`nTest gestoppt: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'Ausgabe aufheben. Ein eventuell aktiver Auftrag wird nicht automatisch abgebrochen.'
    exit 1
}
