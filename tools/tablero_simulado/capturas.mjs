// Capturas del tablero con el servidor simulado (DATOS SIMULADOS).
//
// Para cada estado arranca servidor_simulado.py con ese escenario, abre el
// tablero en Microsoft Edge sin interfaz (protocolo DevTools) y guarda la
// página completa en vista de computador (1366 px) y de celular (390 px,
// escala 2). Solo usa Node 22 o posterior (WebSocket nativo) y Python 3.
//
// Uso: node capturas.mjs [carpeta de salida]
//      (por defecto, docs/capturas/tablero/ en la raíz del repositorio)
//
// Modo Wokwi (SIMULACIÓN): captura el tablero que sirve el ESP32 simulado en
// la extensión de VS Code, por el reenvío de puertos de wokwi.toml
// (localhost:8180). No arranca el servidor simulado y responde la
// autenticación Digest con TABLERO_USUARIO y TABLERO_CLAVE, y usa el token de
// TABLERO_TOKEN (por defecto, los valores de secrets.example.h).
//      node capturas.mjs --wokwi <nombre> [carpeta de salida]
//      (por defecto, docs/capturas/wokwi/; archivos simulacion-wokwi-tablero-<nombre>-*.png)

import { spawn } from 'node:child_process';
import { mkdirSync, writeFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { tmpdir } from 'node:os';

const AQUI = dirname(fileURLToPath(import.meta.url));
const MODO_WOKWI = process.argv[2] === '--wokwi';
const NOMBRE_WOKWI = MODO_WOKWI ? (process.argv[3] || 'tablero') : null;
const SALIDA = resolve((MODO_WOKWI ? process.argv[4] : process.argv[2]) ||
  join(AQUI, '..', '..', 'docs', 'capturas', MODO_WOKWI ? 'wokwi' : 'tablero'));
const URL_WOKWI = 'http://localhost:8180/?token=' + encodeURIComponent(process.env.TABLERO_TOKEN || 'token-dispositivo-1');
const USUARIO = process.env.TABLERO_USUARIO || 'operador';
const CLAVE = process.env.TABLERO_CLAVE || 'cambiar-esta-clave';
const EDGE = [
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Microsoft/Edge/Application/msedge.exe',
].find(existsSync);
const PUERTO_DEPURACION = 9333;
const PUERTO_SIMULADOR = 8093;
const ESPERA_MS = 4500;  // Datos por WebSocket y gráficas dibujadas.

const ESCENARIOS = [
  ['NORMAL', 'normal'], ['ADVERTENCIA', 'advertencia'], ['ALERTA', 'alerta'],
  ['CRITICO', 'critico'], ['FALLA_NIVEL', 'falla-nivel'],
];
const VISTAS = [
  { nombre: 'computador', width: 1366, height: 900, deviceScaleFactor: 1, mobile: false },
  { nombre: 'celular', width: 390, height: 844, deviceScaleFactor: 2, mobile: true },
];

const esperar = (ms) => new Promise((r) => setTimeout(r, ms));

/** Cliente mínimo del protocolo DevTools sobre WebSocket. */
async function conectarCdp(url) {
  const ws = new WebSocket(url);
  await new Promise((ok, error) => { ws.onopen = ok; ws.onerror = error; });
  let id = 0;
  const pendientes = new Map();
  const oyentes = new Map();
  ws.onmessage = (e) => {
    const m = JSON.parse(e.data);
    if (m.method && oyentes.has(m.method)) oyentes.get(m.method)(m.params);
    if (m.id && pendientes.has(m.id)) {
      const { ok, error } = pendientes.get(m.id);
      pendientes.delete(m.id);
      m.error ? error(new Error(m.error.message)) : ok(m.result);
    }
  };
  return {
    enviar(metodo, params = {}) {
      id += 1;
      ws.send(JSON.stringify({ id, method: metodo, params }));
      return new Promise((ok, error) => pendientes.set(id, { ok, error }));
    },
    alEvento(metodo, funcion) { oyentes.set(metodo, funcion); },
    cerrar() { ws.close(); },
  };
}

/** Abre el tablero en una vista (computador o celular) y guarda la página completa. */
async function capturar(cdp, vista, archivo, url) {
  await cdp.enviar('Emulation.setDeviceMetricsOverride', {
    width: vista.width, height: vista.height, deviceScaleFactor: vista.deviceScaleFactor, mobile: vista.mobile,
  });
  await cdp.enviar('Page.navigate', {
    url: url || `http://127.0.0.1:${PUERTO_SIMULADOR}/?token=token-dispositivo-1`,
  });
  if (MODO_WOKWI) {
    // El ESP32 simulado sirve los archivos despacio (Chart.js tarda unos 10 s):
    // se espera a que el tablero tenga datos, histórico y gráficas, hasta 120 s.
    const listo = "document.querySelectorAll('.tarjeta').length > 0 && " +
      "document.getElementById('estado-nombre').textContent !== 'INICIANDO' && " +
      "document.getElementById('historico-nota').textContent.includes('registros')";
    const inicio = Date.now();
    for (;;) {
      const r = await cdp.enviar('Runtime.evaluate', { expression: listo, returnByValue: true });
      if (r.result && r.result.value === true) break;
      if (Date.now() - inicio > 120000) { console.log('  aviso: el tablero no terminó de cargar en 120 s'); break; }
      await esperar(1000);
    }
    console.log(`  tablero listo en ${((Date.now() - inicio) / 1000).toFixed(1)} s`);
    await esperar(2000);
  } else {
    await esperar(ESPERA_MS);
  }
  // Página completa: se amplía la ventana al alto del contenido.
  const { cssContentSize } = await cdp.enviar('Page.getLayoutMetrics');
  const alto = Math.ceil(cssContentSize.height);
  await cdp.enviar('Emulation.setDeviceMetricsOverride', {
    width: vista.width, height: alto, deviceScaleFactor: vista.deviceScaleFactor, mobile: vista.mobile,
  });
  await esperar(800);
  const { data } = await cdp.enviar('Page.captureScreenshot', { format: 'png' });
  writeFileSync(archivo, Buffer.from(data, 'base64'));
  console.log('  ' + archivo);
}

async function main() {
  if (!EDGE) throw new Error('No se encontró Microsoft Edge');
  mkdirSync(SALIDA, { recursive: true });
  const perfil = join(tmpdir(), 'tablero-capturas-edge');
  const edge = spawn(EDGE, [
    '--headless=new', '--disable-gpu', '--hide-scrollbars', '--no-first-run',
    `--remote-debugging-port=${PUERTO_DEPURACION}`, `--user-data-dir=${perfil}`, 'about:blank',
  ], { stdio: 'ignore' });

  try {
    let destinos = [];
    for (let i = 0; i < 40 && !destinos.some((d) => d.type === 'page'); i++) {
      await esperar(250);
      try { destinos = await (await fetch(`http://127.0.0.1:${PUERTO_DEPURACION}/json/list`)).json(); } catch { /* aún no */ }
    }
    const pagina = destinos.find((d) => d.type === 'page');
    if (!pagina) throw new Error('Edge no abrió la página de depuración');
    const cdp = await conectarCdp(pagina.webSocketDebuggerUrl);
    await cdp.enviar('Page.enable');

    if (MODO_WOKWI) {
      // Responde el desafío Digest del ESP32 simulado.
      cdp.alEvento('Fetch.requestPaused', (p) => cdp.enviar('Fetch.continueRequest', { requestId: p.requestId }));
      cdp.alEvento('Fetch.authRequired', (p) => cdp.enviar('Fetch.continueWithAuth', {
        requestId: p.requestId,
        authChallengeResponse: { response: 'ProvideCredentials', username: USUARIO, password: CLAVE },
      }));
      await cdp.enviar('Fetch.enable', { handleAuthRequests: true, patterns: [{ urlPattern: '*' }] });
      for (const vista of VISTAS) {
        await capturar(cdp, vista, join(SALIDA, `simulacion-wokwi-tablero-${NOMBRE_WOKWI}-${vista.nombre}.png`), URL_WOKWI);
      }
      cdp.cerrar();
      return;
    }

    for (const [escenario, archivo] of ESCENARIOS) {
      console.log(escenario);
      const servidor = spawn('python', [join(AQUI, 'servidor_simulado.py'),
        '--puerto', String(PUERTO_SIMULADOR), '--escenario', escenario], { stdio: 'ignore' });
      await esperar(1500);
      try {
        for (const vista of VISTAS) {
          await capturar(cdp, vista, join(SALIDA, `${archivo}-${vista.nombre}.png`));
        }
      } finally {
        await cdp.enviar('Page.navigate', { url: 'about:blank' });
        servidor.kill();
        await esperar(500);
      }
    }
    cdp.cerrar();
  } finally {
    edge.kill();
  }
}

main().catch((e) => { console.error(e); process.exit(1); });
