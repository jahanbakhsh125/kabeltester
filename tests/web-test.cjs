// Bedienlogik ohne angeschlossenen ESP pruefen: node tests/web-test.cjs
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

class Element {
  constructor(tag = 'div') {
    this.tagName = tag;
    this.children = [];
    this.listeners = {};
    this.value = '';
    this.textContent = '';
    this.disabled = false;
  }
  appendChild(child) { this.children.push(child); }
  replaceChildren() { this.children = []; }
  addEventListener(type, callback) { this.listeners[type] = callback; }
  reportValidity() { return true; }
  async emit(type) { await this.listeners[type]({preventDefault() {}}); }
  set innerHTML(value) { throw new Error('Auftragsdaten duerfen nicht als HTML eingefuegt werden.'); }
}

async function main() {
  const html = fs.readFileSync(path.join(__dirname, '../main/index.html'), 'utf8');
  const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
  const elements = new Map([...html.matchAll(/id="([^"]+)"/g)].map(match => [match[1], new Element()]));
  const field = id => {
    assert(elements.has(id), 'HTML-Feld vorhanden: ' + id);
    return elements.get(id);
  };
  field('profil').value = '0,1,2,3,4,5,6,7';
  field('anzahl').value = '3';
  field('args').value = '{"charge":"A17"}';
  const timers = [];
  const requests = [];
  const state = {job: null, runs: [], result: 'Noch kein Test', receiver: 'unknown', offline: false, conflict: false};
  const straight = [0,1,2,3,4,5,6,7];
  const four = [0,1,2,3,-1,-1,-1,-1];
  let nextId = 1;
  const context = vm.createContext({
    document: {getElementById: field, createElement: tag => new Element(tag)},
    AbortSignal, TextEncoder,
    setTimeout: callback => { timers.push(callback); },
    fetch: async (url, options) => {
      if (state.offline) throw new Error('Verbindung unterbrochen');
      const method = options.method;
      requests.push({url, method, body: options.body});
      let status = 200;
      let data;
      if (url === '/api/profiles') data = {profiles: [{name: 'gerade8', profile: straight}, {name: 'gerade4', profile: four}]};
      else if (url === '/api/result') data = {result: state.result};
      else if (url === '/api/runs') data = {runs: state.runs};
      else if (url === '/api/jobs/current' && method === 'GET') data = {job: state.job, receiver_status: state.receiver};
      else if (url === '/api/jobs/current' && method === 'DELETE') {
        data = {job_id: state.job.job_id, status: 'cancelled'};
        state.job = null;
      } else if (url === '/api/jobs' && method === 'POST') {
        const body = JSON.parse(options.body);
        if (state.conflict) {
          state.job = {job_id: 99, status: 'active', completed: 0, count: 1, profile: straight, args: {}};
          status = 409;
          data = {error: 'job_already_active', message: 'Es ist bereits ein Auftrag aktiv.'};
        } else {
          state.job = {job_id: nextId++, status: 'active', completed: 0, ...body};
          status = 201;
          data = {job_id: state.job.job_id, status: 'active', completed: 0, count: body.count};
        }
      } else throw new Error('Unerwarteter API-Aufruf: ' + method + ' ' + url);
      const snapshot = JSON.stringify(data);
      return {ok: status >= 200 && status < 300, status, json: async () => JSON.parse(snapshot)};
    }
  });
  await vm.runInContext(script, context);
  assert.equal(field('anlegen').disabled, false);
  assert.equal(field('abbrechen').disabled, true);
  assert.equal(field('vorlage').children.length, 2);

  field('vorlage').value = '1';
  await field('vorlage').emit('change');
  assert.equal(field('profil').value, four.join(','));
  const posts = () => requests.filter(request => request.method === 'POST');
  field('profil').value = '0,1,2,3,4,5,6,';
  await field('auftrag-form').emit('submit');
  assert.equal(posts().length, 0, 'Ungueltiges Profil sendet keinen Auftrag');
  field('profil').value = four.join(',');
  field('args').value = 'null';
  await field('auftrag-form').emit('submit');
  assert.equal(posts().length, 0, 'Zusatzdaten muessen ein Objekt sein');

  const args = {charge: '<img src=x onerror=alert(1)>', nested: {number: 17}};
  field('args').value = JSON.stringify(args);
  field('anzahl').value = '2';
  await field('auftrag-form').emit('submit');
  assert.deepEqual(JSON.parse(posts()[0].body), {profile: four, count: 2, args});
  assert.equal(field('anlegen').disabled, true);
  assert.equal(field('eingaben').disabled, true);
  assert.equal(field('abbrechen').disabled, false);
  await field('auftrag-form').emit('submit');
  assert.equal(posts().length, 1, 'Aktiver Auftrag verhindert weiteres Anlegen');

  state.job.completed = 1;
  state.result = 'FEHLER';
  state.receiver = 'unreachable';
  state.runs = [{job_id: 1, lauf: 1, result: 'FEHLER', gemeldet: false, profile: four, args}];
  await timers.shift()();
  assert.equal(field('fortschritt').value, 1);
  assert.equal(field('empfaenger').textContent, 'Meldeempfänger nicht erreichbar');
  const cells = field('laeufe').children[0].children;
  assert.equal(cells[2].textContent, 'FEHLER');
  assert.equal(cells[3].textContent, 'Nicht gemeldet');
  assert.equal(cells[5].textContent, '—', 'Alte Ergebnisse ohne Messprofil bleiben lesbar');
  assert.equal(cells[6].textContent, JSON.stringify(args), 'Zusatzdaten werden als Text dargestellt');

  // Fehlermessung: Metadaten behalten und die gelieferten Ist-Verbindungen anzeigen.
  const measured = [1, 2, 3, 4, -1, -1, -1, -1];
  const errors = ['A0: erwartet B0; gemessen B1', 'A1: erwartet B1; gemessen B2',
    'A2: erwartet B2; gemessen B3', 'A3: erwartet B3; gemessen B4'];
  state.runs[0] = {...state.runs[0], measured_profile: measured, args: {...args, gemessen: measured}, errors,
    comparison_direction: 'normal', error_count: 8};
  await timers.shift()();
  assert.equal(field('laeufe').children[0].children[5].textContent, JSON.stringify(measured));
  const diagnosticCell = field('laeufe').children[0].children[6];
  assert.deepEqual(JSON.parse(diagnosticCell.textContent), {...args, gemessen: measured});

  // Mehrere B-Verbindungen muessen sichtbar bleiben; serverseitige Texte sind nur Text.
  state.runs[0].measured_profile = [[0, 1], 1, 2, 3, -1, -1, -1, -1];
  state.runs[0].args.gemessen = state.runs[0].measured_profile;
  state.runs[0].comparison_direction = 'reversed';
  state.runs[0].errors = ['<img src=x onerror=alert(1)>'];
  await timers.shift()();
  const shortCell = field('laeufe').children[0].children[6];
  assert.deepEqual(JSON.parse(shortCell.textContent).gemessen[0], [0, 1]);
  assert(shortCell.textContent.includes('<img src=x onerror=alert(1)>'));
  assert.equal(field('laeufe').children[0].children[5].textContent,
    JSON.stringify(state.runs[0].measured_profile));

  state.result = 'OK';
  state.runs[0].result = 'OK';
  await timers.shift()();
  assert.equal(field('laeufe').children[0].children[6].textContent, JSON.stringify(state.runs[0].args),
    'Auch OK-Ergebnisse zeigen das gemessene Profil in den Zusatzdaten');

  await field('abbrechen').emit('click');
  assert.equal(state.job, null);
  assert.equal(field('anlegen').disabled, false);
  assert.equal(field('abbrechen').disabled, true);
  assert.equal(field('laeufe').children.length, 1, 'Ergebnis bleibt nach Abbruch sichtbar');

  state.conflict = true;
  await field('auftrag-form').emit('submit');
  assert(field('meldung').textContent.includes('bereits ein Auftrag aktiv'));
  assert.equal(field('anlegen').disabled, true, 'Extern angelegter Auftrag wird nach Konflikt angezeigt');
  state.offline = true;
  await timers.shift()();
  assert.equal(field('anlegen').disabled, true);
  assert.equal(field('abbrechen').disabled, true);
  state.offline = false;
  await timers.shift()();
  assert.equal(field('abbrechen').disabled, false, 'Bedienung nach Wiederverbindung verfuegbar');
  console.log('Webtests bestanden: Profile, Eingaben, Anlegen, Fortschritt, Ergebnisliste, Abbruch, Konflikt und Verbindungsausfall.');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
