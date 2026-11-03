param(
    [string]$ToolsRoot = 'C:/Espressif/tools'
)

# Fuehrt echte C-Funktionen der Firmware in QEMU aus, ohne das Board zu flashen.
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$outputDir = Join-Path $repo 'build/firmware-test'
$null = New-Item -ItemType Directory -Path $outputDir -Force
function Werkzeug([string]$Ordner, [string]$Datei) {
    $found = @(Get-ChildItem (Join-Path $ToolsRoot $Ordner) -Filter $Datei -Recurse -File)
    if ($found.Count -ne 1) { throw "Werkzeug nicht eindeutig vorhanden: $Datei" }
    return $found[0].FullName
}
$gcc = Werkzeug 'riscv32-esp-elf' 'riscv32-esp-elf-gcc.exe'
$qemu = Werkzeug 'qemu-riscv32' 'qemu-system-riscv32.exe'
$source = [IO.File]::ReadAllText((Join-Path $repo 'main/main.c'))

# Auch Datenstrukturen und Grenzen aus main.c lesen, damit der Test mitzieht.
$declarations = @()
foreach ($name in @('ADER_ANZAHL','ERGEBNIS_ANZAHL','BEIDE_RICHTUNGEN')) {
    $definition = [regex]::Match($source, "(?m)^#define\s+$name\s+\d+")
    if (-not $definition.Success) { throw "Konstante fehlt: $name" }
    $declarations += $definition.Value
}
foreach ($name in @('auftrag_t','testergebnis_t')) {
    $definition = [regex]::Match($source, "(?s)typedef struct\s*\{[^}]*\}\s*$name;")
    if (-not $definition.Success) { throw "Datenstruktur fehlt: $name" }
    $declarations += $definition.Value
}
[IO.File]::WriteAllText((Join-Path $outputDir 'firmware-types.inc'),
    ($declarations -join "`n"), [Text.UTF8Encoding]::new($false))

# Ganze Funktionen unveraendert uebernehmen; keine zweite Implementierung testen.
function Funktion([string]$Name) {
    $match = [regex]::Match($source, "(?m)^static [^`r`n]+\b$Name\([^;{]*\)\s*\{")
    if (-not $match.Success) { throw "C-Funktion fehlt: $Name" }
    $start = $match.Index
    $position = $start + $match.Length
    $depth = 1
    while ($depth -gt 0 -and $position -lt $source.Length) {
        if ($source[$position] -eq '{') { $depth++ }
        if ($source[$position] -eq '}') { $depth-- }
        $position++
    }
    if ($depth -ne 0) { throw "C-Funktion unvollstaendig: $Name" }
    return $source.Substring($start, $position - $start)
}
$names = @('verbindungen_json','messprofil_json','verbindungen_text',
    'diagnose_json_ergaenzen','lauf_json_erstellen','ergebnis_speichern',
    'messung_abschliessen','profil_pruefen','auftrag_anlegen',
    'aktueller_auftrag_senden','auftrag_abbrechen','testergebnisse_senden')
$functions = ($names | ForEach-Object { Funktion $_ }) -join "`n`n"
[IO.File]::WriteAllText((Join-Path $outputDir 'firmware-functions.inc'), $functions,
    [Text.UTF8Encoding]::new($false))
$jsonDir = Join-Path $repo 'managed_components/espressif__cjson/cJSON'
if (-not (Test-Path (Join-Path $jsonDir 'cJSON.c'))) { throw 'cJSON fehlt; zuerst idf.py build ausfuehren.' }
$elf = Join-Path $outputDir 'firmware-test.elf'
$linker = Join-Path $PSScriptRoot 'firmware-test.ld'
& $gcc '-march=rv32imc_zicsr_zifencei' '-mabi=ilp32' '-O1' '-g' '-fno-stack-protector' `
    '-nostartfiles' '--specs=nosys.specs' "-I$outputDir" "-I$jsonDir" `
    (Join-Path $PSScriptRoot 'firmware-harness.c') (Join-Path $jsonDir 'cJSON.c') `
    "-Wl,-T,$linker" '-lm' '-o' $elf
if ($LASTEXITCODE -ne 0) { throw 'C-Testprogramm konnte nicht gebaut werden.' }

# Ausgabe in Dateien halten; haengende Tests nach 20 Sekunden beenden.
$stdout = Join-Path $outputDir 'stdout.txt'
$stderr = Join-Path $outputDir 'stderr.txt'
$arguments = "-machine virt -m 32M -nographic -bios none -kernel `"$elf`" -semihosting-config enable=on,target=native"
$process = Start-Process -FilePath $qemu -ArgumentList $arguments -WindowStyle Hidden `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
if (-not $process.WaitForExit(20000)) {
    Stop-Process -Id $process.Id
    throw 'QEMU-Test hat nach 20 Sekunden nicht geantwortet.'
}
$result = [IO.File]::ReadAllText($stdout)
Write-Output $result
if ($process.ExitCode -ne 0 -or -not $result.Contains('Firmware-C-Tests bestanden')) {
    Write-Output ([IO.File]::ReadAllText($stderr))
    throw 'Firmware-C-Tests fehlgeschlagen.'
}
