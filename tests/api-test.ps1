param(
    [string]$BaseUrl = 'http://kabeltester.local'
)

$ErrorActionPreference = 'Stop'
$BaseUrl = $BaseUrl.TrimEnd('/')

function Api-Aufrufen([string]$Pfad, [string]$Methode = 'GET', [string]$Body = '') {
    $parameter = @{
        Uri = "$BaseUrl$Pfad"
        Method = $Methode
        TimeoutSec = 10
    }
    if ($Body -ne '') {
        $parameter.ContentType = 'application/json'
        $parameter.Body = $Body
    }
    Invoke-RestMethod @parameter
}

function Pruefen([bool]$Bedingung, [string]$Beschreibung) {
    if (-not $Bedingung) {
        throw "FEHLER: $Beschreibung"
    }
    Write-Host "OK: $Beschreibung" -ForegroundColor Green
}

function Laeufe-Lesen {
    $antwort = Api-Aufrufen '/api/runs'
    Pruefen ($null -ne $antwort.PSObject.Properties['runs']) 'Ergebnisliste vorhanden'
    $liste = @($antwort.runs)
    Pruefen ($liste.Count -le 20) 'Hoechstens 20 gespeicherte Ergebnisse'
    foreach ($eintrag in $liste) {
        Pruefen (($eintrag.job_id -gt 0) -and ($eintrag.lauf -gt 0) -and
                 ($eintrag.result -in @('OK', 'FEHLER'))) 'Ergebniseintrag hat gueltige Werte'
        Pruefen (@($eintrag.profile).Count -eq 8 -and
                 $eintrag.args -is [PSCustomObject] -and
                 $eintrag.gemeldet -is [bool]) 'Profil, args und Meldestatus vorhanden'
    }
    # Ein Array auch bei null oder einem Eintrag zurueckgeben.
    return ,$liste
}

try {
    Write-Host 'API-Test: drei Messungen eines geraden Achtaderkabels.'
    Write-Host 'Der ESP muss bereits die Firmware mit /api/runs verwenden.'
    $null = Laeufe-Lesen
    $zustand = Api-Aufrufen '/api/jobs/current'

    if ($null -eq $zustand.job) {
        $datei = Join-Path $PSScriptRoot '../auftrag-test.json'
        $body = Get-Content -Raw -LiteralPath $datei
        $vorlage = $body | ConvertFrom-Json
        Pruefen (($vorlage.profile -join ',') -eq '0,1,2,3,4,5,6,7' -and
                 $vorlage.count -eq 3) 'Testdatei erwartet acht gerade Adern und drei Tests'
        $null = Api-Aufrufen '/api/jobs' 'POST' $body
        $zustand = Api-Aufrufen '/api/jobs/current'
    }

    # Einen bereits gestarteten Testauftrag verwenden, ohne ihn abzubrechen.
    Pruefen ($null -ne $zustand.job -and $zustand.job.status -eq 'active' -and
             $zustand.job.completed -eq 0 -and $zustand.job.count -eq 3 -and
             ($zustand.job.profile -join ',') -eq '0,1,2,3,4,5,6,7') `
             'Aktiver Achtaderauftrag mit Zaehler 0 von 3'
    $jobId = $zustand.job.job_id

    for ($lauf = 1; $lauf -le 3; $lauf++) {
        Write-Host "`nTest $lauf von 3:"
        Write-Host 'Kabel abziehen, warten bis die Ergebnis-LED nicht mehr dauerhaft leuchtet, anschliessen und Schalter betaetigen.'
        $null = Read-Host 'Nach dem Ergebnis im Monitor hier Enter druecken'

        $alle = Laeufe-Lesen
        $laeufe = @($alle | Where-Object { $_.job_id -eq $jobId })
        Pruefen ($laeufe.Count -eq $lauf) "$lauf Ergebnisse fuer diesen Auftrag gespeichert"
        for ($i = 0; $i -lt $laeufe.Count; $i++) {
            Pruefen ($laeufe[$i].lauf -eq ($i + 1)) 'Messungsnummern in richtiger Reihenfolge'
            Pruefen (($laeufe[$i].profile -join ',') -eq '0,1,2,3,4,5,6,7' -and
                     $laeufe[$i].args.charge -eq 'A17') 'Auftragsdaten erhalten'
        }
        Pruefen ($laeufe[-1].result -eq 'OK') 'Gerades Achtaderkabel hat Ergebnis OK'
        $zustand = Api-Aufrufen '/api/jobs/current'

        if ($lauf -lt 3) {
            Pruefen ($null -ne $zustand.job -and $zustand.job.job_id -eq $jobId -and
                     $zustand.job.status -eq 'active' -and
                     $zustand.job.completed -eq $lauf) 'Auftragszaehler entspricht den Messungen'
        } else {
            Pruefen ($null -eq $zustand.job) 'Auftrag nach drei Messungen beendet'
        }

        if ($lauf -eq 1) {
            Write-Host 'Kabel eingesteckt lassen und den Schalter erneut betaetigen.'
            $null = Read-Host 'Nach der Meldung Zuerst das Kabel abziehen hier Enter druecken'
            $zustand = Api-Aufrufen '/api/jobs/current'
            Pruefen ($null -ne $zustand.job -and $zustand.job.job_id -eq $jobId -and
                     $zustand.job.completed -eq 1) 'Ohne Abziehen kein weiterer gezaehlter Test'
            $alle = Laeufe-Lesen
            $laeufe = @($alle | Where-Object { $_.job_id -eq $jobId })
            Pruefen ($laeufe.Count -eq 1) 'Ohne Abziehen kein weiterer Ergebniseintrag'
        }
    }

    Write-Host "`nKabel abziehen, warten bis die Ergebnisanzeige endet und Schalter ohne aktiven Auftrag betaetigen."
    $null = Read-Host 'Nach der Meldung Kein Auftrag aktiv hier Enter druecken'
    $zustand = Api-Aufrufen '/api/jobs/current'
    Pruefen ($null -eq $zustand.job) 'Weiterhin kein aktiver Auftrag'
    $alle = Laeufe-Lesen
    $laeufe = @($alle | Where-Object { $_.job_id -eq $jobId })
    Pruefen ($laeufe.Count -eq 3) 'Drei Ergebnisse bleiben nach Auftragsende erhalten'
    Write-Host "`nAlle Pruefungen dieses Testablaufs erfolgreich." -ForegroundColor Green
} catch {
    Write-Host "`nTest gestoppt: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'Der Auftrag wurde nicht automatisch abgebrochen. Terminalausgabe zur Diagnose aufheben.'
    exit 1
}
