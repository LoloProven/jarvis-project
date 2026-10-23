// Carga n8n-flujos/jarvis-reloj.json en el n8n del proyecto y lo deja activo.
// Uso (con n8n APAGADO):  node n8n-flujos/importar-jarvis.mjs
// En el repo el calendario es un marcador (TU_CALENDARIO@gmail.com) para no subir el mail: acá se
// reemplaza por el calendario del flujo "My workflow" del usuario, leído de la base de n8n.
import { DatabaseSync } from 'node:sqlite';
import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const userFolder = path.join(repo, 'n8n-servidor', '.n8n');
const db = new DatabaseSync(path.join(userFolder, '.n8n', 'database.sqlite'), { readOnly: true });
const mine = db.prepare("SELECT nodes FROM workflow_entity WHERE name = 'My workflow'").get();
db.close();
const calendar = JSON.parse(mine.nodes).find((n) => n.parameters?.calendar)?.parameters.calendar.value;
if (!calendar) throw new Error('No encontre el calendario en "My workflow"');

const flow = readFileSync(path.join(repo, 'n8n-flujos', 'jarvis-reloj.json'), 'utf8')
    .split('TU_CALENDARIO@gmail.com').join(calendar);
const tmpDir = 'D:\\temp';
mkdirSync(tmpDir, { recursive: true });
const tmp = path.join(tmpDir, 'jarvis-import.json');
writeFileSync(tmp, flow);                                  // sin BOM: n8n no lo acepta

const n8n = path.join(repo, 'node_modules', '.bin', 'n8n.cmd');
const env = { ...process.env, N8N_USER_FOLDER: userFolder, TEMP: tmpDir, TMP: tmpDir };
const run = (args) => {
    try { return execFileSync(n8n, args, { env, encoding: 'utf8', shell: true }); }
    catch (e) { return (e.stdout || '') + (e.stderr || ''); }   // publish sale con código 255 aunque ande
};
console.log(run(['import:workflow', `--input=${tmp}`]).trim().split('\n').pop());
console.log(run(['publish:workflow', '--id=jarvisReloj00001']).trim().split('\n')[0]);
