"""Prueft Git-Inhalte auf Zugangsdaten, ohne gefundene Werte auszugeben."""
import argparse
import json
import pathlib
import re
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
args = parser.parse_args()
repo = args.repo.resolve()
git_prefix = ['git', '-c', f'safe.directory={repo.as_posix()}', '-C', str(repo)]

def git(*arguments):
    result = subprocess.run(git_prefix + list(arguments), capture_output=True, timeout=60)
    if result.returncode:
        raise RuntimeError('Git-Pruefung fehlgeschlagen; keine Inhaltsausgabe.')
    return result.stdout

# Nur eindeutige Beispielwerte gelten als Platzhalter.
def placeholder(value):
    return not value or bool(re.fullmatch(r'(?:DEIN(?:E|EN)?_[A-Z_]+|YOUR_[A-Z_]+|<[^<>]+>|CHANGE_ME|CHANGEME)', value))

credential = re.compile(
    r'(?im)(?:#define\s+|\b)(?:WIFI_PASSWORD|WLAN_PASSWORD|WIFI_PASS|WLAN_PASS|PASSWORD|PASSWD|API_KEY|API_TOKEN|ACCESS_TOKEN|CLIENT_SECRET|SECRET_KEY)\b\s*(?:=|:)?\s*["\']([^"\'\r\n]*)["\']')
json_credential = re.compile(r'(?i)["\'](?:password|passwd|api_key|api_token|access_token|client_secret|secret_key)["\']\s*:\s*["\']([^"\'\r\n]+)["\']')
tokens = re.compile(r'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[A-Z0-9]{16})\b|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|https?://[^\s/@:]+:[^\s/@]+@')
known_passwords = set()
private_file = repo / 'config/flash_config.h'
if private_file.exists():
    private_text = private_file.read_text(encoding='utf-8-sig')
    for match in credential.finditer(private_text):
        value = match.group(1)
        if not placeholder(value):
            known_passwords.add(value.encode('utf-8'))
            try:
                known_passwords.add(json.loads('"' + value + '"').encode('utf-8'))
            except (ValueError, UnicodeError):
                pass

findings = set()
def inspect(data, location):
    for password in known_passwords:
        if password and password in data:
            findings.add((location, 'bekannter privater Passwort-/Schluesselwert'))
    text = data.decode('utf-8', errors='replace')
    if tokens.search(text):
        findings.add((location, 'Token, privater Schluessel oder Zugangsdaten in URL'))
    for pattern in (credential, json_credential):
        for match in pattern.finditer(text):
            if not placeholder(match.group(1)):
                findings.add((location, 'ausgeschriebene Passwort-/Schluesselzuweisung'))

commits = git('rev-list', '--all', '--reflog').decode().splitlines()
paths = {}
for line in git('rev-list', '--objects', '--all', '--reflog').decode('utf-8', errors='replace').splitlines():
    oid, _, path = line.partition(' ')
    if path:
        paths.setdefault(oid, path)
# Auch alte erreichbare private Konfigurationsdateien erkennen.
for commit in commits:
    tree = git('ls-tree', '-r', '--name-only', commit).decode('utf-8', errors='replace').splitlines()
    for path in tree:
        if pathlib.PurePosixPath(path).name == 'flash_config.h':
            findings.add((f'{commit[:12]}:{path}', 'private Konfiguration in Commit'))

# Alle lokalen Blob-Objekte: auch Reflog, Index und nicht mehr referenzierte Inhalte.
objects = git('cat-file', '--batch-all-objects', '--batch-check=%(objectname) %(objecttype)').decode().splitlines()
blobs = [line.split()[0] for line in objects if line.endswith(' blob')]
process = subprocess.Popen(git_prefix + ['cat-file', '--batch'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
try:
    for oid in [line.split()[0] for line in objects if line.split()[1] in ('blob', 'commit', 'tag')]:
        process.stdin.write((oid + '\n').encode())
        process.stdin.flush()
        header = process.stdout.readline().split()
        size = int(header[2])
        data = process.stdout.read(size)
        if len(data) != size or process.stdout.read(1) != b'\n':
            raise RuntimeError('Git-Objekt konnte nicht vollstaendig gelesen werden.')
        inspect(data, f'{oid[:12]}:{paths.get(oid, "lokales Git-Objekt")}')
finally:
    process.stdin.close()
    process.wait(timeout=10)
if process.returncode:
    raise RuntimeError('Git-Objektpruefung fehlgeschlagen.')

tracked = [path.decode('utf-8') for path in git('ls-files', '-z').split(b'\0') if path]
for path in tracked:
    file = repo / path
    if file.is_file():
        inspect(file.read_bytes(), f'Arbeitsstand:{path}')
if 'config/flash_config.h' in tracked:
    findings.add(('config/flash_config.h', 'private Datei aktuell verfolgt'))
ignored = subprocess.run(git_prefix + ['check-ignore', '-q', 'config/flash_config.h'], capture_output=True).returncode == 0
if not ignored:
    findings.add(('config/flash_config.h', 'private Datei nicht ausgeschlossen'))
print(f'Geprueft: {len(commits)} Commits, {len(blobs)} Git-Blob-Objekte, {len(tracked)} verfolgte Dateien.')
print('Private Konfiguration ausgeschlossen:', 'JA' if ignored else 'NEIN')
if findings:
    print('Funde (Werte werden nicht ausgegeben):')
    for location, kind in sorted(findings):
        print(f'- {location}: {kind}')
    sys.exit(1)
print('Keine Funde fuer bekannte private Werte und gepruefte Zugangsdatenmuster.')
print('Grenze: unbekannte Zugangsdaten ohne erkennbares Muster und Bildinhalte werden nicht erkannt.')