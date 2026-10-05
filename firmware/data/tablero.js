/*
 * Lógica del tablero de monitoreo hídrico.
 *
 * - Valores actuales: WebSocket /ws (cada 1 s). Si el WebSocket se cae, se
 *   consulta /api/actual cada 5 s mientras se reconecta.
 * - Histórico: /api/historico?b=rapido (cada 30 s) o ?b=lento (cada 5 min).
 * - Desactivación de la alarma física: POST /api/alarma/desactivar.
 * Toda petición lleva el token del dispositivo (X-Token, o ?token= en /ws).
 */
'use strict';

(function () {
  // -------------------------------------------------------------------------
  // Configuración
  // -------------------------------------------------------------------------

  const TOKEN = window.TABLERO_TOKEN || '';
  const ZONA_HORARIA = 'America/Bogota';  // UTC−5, la misma del firmware.

  // Umbrales de nivel en % de la altura útil: valores de DEMOSTRACIÓN de
  // config.h (namespace demo). Si cambian en el firmware, cambiarlos aquí.
  const UMBRAL_PREVENTIVO_PCT = 40;
  const UMBRAL_CRITICO_PCT = 20;

  const SONDEO_MS = 5000;              // Respaldo si el WebSocket se cae.
  const RECONEXION_WS_MS = 3000;
  const SIN_DATOS_MS = 5000;           // Más de esto sin datos: aviso.
  const ESPERA_CONFIRMACION_MS = 10000;
  const PERIODO_HISTORICO_MS = { rapido: 30000, lento: 300000 };

  const NOMBRE_ESTADO = {
    INICIANDO: 'INICIANDO', NORMAL: 'NORMAL', ADVERTENCIA: 'ADVERTENCIA',
    ALERTA: 'ALERTA', CRITICO: 'CRÍTICO', 'FALLA NIVEL': 'FALLA NIVEL',
  };
  const ICONO_ESTADO = {
    INICIANDO: '…', NORMAL: '✓', ADVERTENCIA: '!', ALERTA: '▲', CRITICO: '✕', 'FALLA NIVEL': '?',
  };
  const TEXTO_CAUSA = {
    nivel_critico: 'Nivel en el umbral crítico',
    nivel_preventivo: 'Nivel en el umbral preventivo',
    descenso: 'Descenso sostenido del nivel',
    vpd_alto: 'VPD alto (demanda evaporativa alta)',
    temperatura_alta: 'Temperatura alta',
    uv_alto: 'Índice UV alto',
  };
  const ESTADOS_DESACTIVABLES = ['ALERTA', 'CRITICO'];
  // Texto de la fuente de compensación de la velocidad del sonido (sensores.cpp).
  const TEXTO_COMPENSACION = {
    'T DHT22': 'Velocidad del sonido compensada con la temperatura del DHT22',
    'T BMP180': 'Velocidad del sonido compensada con la temperatura del BMP180',
    'compensación por defecto': 'Velocidad del sonido con 20 °C por defecto (sin temperatura medida)',
  };

  // -------------------------------------------------------------------------
  // Utilidades
  // -------------------------------------------------------------------------

  const $ = (id) => document.getElementById(id);

  /** Número con los decimales dados, o «—» si no hay dato. */
  function numero(valor, decimales) {
    return valor === null || valor === undefined ? '—' : Number(valor).toFixed(decimales);
  }

  /** Duración legible (s, min o h y min) a partir de segundos. */
  function duracion(segundos) {
    const s = Math.max(0, Math.round(segundos));
    if (s < 60) return s + ' s';
    if (s < 3600) return Math.floor(s / 60) + ' min';
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    return h + ' h ' + m + ' min';
  }

  const formatoHora = new Intl.DateTimeFormat('es-CO', {
    hour: '2-digit', minute: '2-digit', hourCycle: 'h23', timeZone: ZONA_HORARIA,
  });
  const formatoHoraSegundos = new Intl.DateTimeFormat('es-CO', {
    hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23', timeZone: ZONA_HORARIA,
  });

  /** Consulta la API del tablero con el token del dispositivo en el encabezado X-Token. */
  function api(ruta, opciones) {
    return fetch(ruta, Object.assign({ cache: 'no-store', headers: { 'X-Token': TOKEN } }, opciones));
  }

  /** Mensaje para el usuario según el código HTTP de una respuesta fallida. */
  function mensajeError(estadoHttp) {
    if (estadoHttp === 401) return 'Se requiere iniciar sesión (usuario y clave del tablero).';
    if (estadoHttp === 403) return 'Dispositivo no autorizado o fuera de la WLAN local.';
    return 'Error del servidor (' + estadoHttp + ').';
  }

  // -------------------------------------------------------------------------
  // Tarjetas de valores actuales
  // -------------------------------------------------------------------------

  // Cada tarjeta indica de qué sensor depende (para la insignia y la antigüedad).
  const TARJETAS = [
    {
      id: 'nivel', titulo: 'Nivel del agua', unidad: 'cm', sensor: (j) => j.nivel,
      valor: (j) => numero(j.nivel.cm, 1),
      detalle: (j) => numero(j.nivel.pct, 0) + ' % de la altura útil · tendencia ' +
        (j.nivel.tendencia_cm_min === null ? 'no disponible' : numero(j.nivel.tendencia_cm_min, 2) + ' cm/min'),
      extra: (j) => TEXTO_COMPENSACION[j.nivel.compensacion] || 'Compensación: ' + j.nivel.compensacion,
    },
    {
      id: 'temperatura', titulo: 'Temperatura', subtitulo: 'DHT22', unidad: '°C', sensor: (j) => j.dht22,
      valor: (j) => numero(j.dht22.temperatura_c, 1),
    },
    {
      id: 'humedad', titulo: 'Humedad relativa', subtitulo: 'DHT22', unidad: '%', sensor: (j) => j.dht22,
      valor: (j) => numero(j.dht22.humedad_pct, 0),
    },
    {
      id: 'presion', titulo: 'Presión atmosférica', subtitulo: 'BMP180', unidad: 'hPa', sensor: (j) => j.bmp180,
      valor: (j) => numero(j.bmp180.presion_hpa, 1),
    },
    {
      id: 'uv', titulo: 'Índice UV', subtitulo: 'GUVA-S12SD', unidad: '', sensor: (j) => j.guva,
      valor: (j) => numero(j.guva.uv_indice, 1),
      detalle: (j) => 'Salida del sensor: ' + numero(j.guva.mv, 0) + ' mV',
    },
    {
      id: 'vpd', titulo: 'VPD', subtitulo: 'Indicador de demanda evaporativa', unidad: 'kPa', sensor: (j) => j.dht22,
      valor: (j) => numero(j.vpd_kpa, 2),
      detalle: () => 'Calculado con la temperatura y la humedad del DHT22',
    },
    {
      id: 'et0', titulo: 'ET0', subtitulo: 'Estimación diaria de evaporación potencial de referencia',
      unidad: 'mm/día', sensor: null,
      valor: (j) => numero(j.et0_mm_dia, 1),
      detalle: (j) => j.et0 === 'disponible'
        ? 'Hargreaves-Samani, con las temperaturas extremas de 24 h'
        : 'No disponible: requiere 24 h de datos y hora válida por NTP',
    },
  ];

  /** Crea una tarjeta por variable en la sección de valores actuales. */
  function crearTarjetas() {
    const contenedor = $('tarjetas');
    for (const t of TARJETAS) {
      const el = document.createElement('article');
      el.className = 'tarjeta tarjeta--' + t.id;
      el.innerHTML =
        '<div class="tarjeta__cabecera"><div>' +
        '<div class="tarjeta__titulo"></div><div class="tarjeta__subtitulo"></div></div>' +
        '<span class="insignia"></span></div>' +
        '<div class="tarjeta__valor"><span class="v">—</span><span class="tarjeta__unidad"></span></div>' +
        '<div class="tarjeta__detalle"></div><div class="tarjeta__detalle extra"></div>' +
        '<div class="tarjeta__edad"></div>';
      el.querySelector('.tarjeta__titulo').textContent = t.titulo;
      el.querySelector('.tarjeta__subtitulo').textContent = t.subtitulo || '';
      el.querySelector('.tarjeta__unidad').textContent = t.unidad;
      t.el = el;
      contenedor.appendChild(el);
    }
  }

  /** Antigüedad del dato: edad_s del equipo más el tiempo desde que llegó. */
  function textoEdad(sensor) {
    if (!sensor || sensor.edad_s === null) return 'Sin lectura válida todavía';
    const transcurrido = (Date.now() - recibidoMs) / 1000;
    return 'Dato válido de hace ' + duracion(sensor.edad_s + transcurrido);
  }

  /** Actualiza el valor, el detalle y la insignia de estado de cada tarjeta. */
  function actualizarTarjetas(j) {
    for (const t of TARJETAS) {
      const el = t.el;
      el.querySelector('.v').textContent = t.valor(j);
      el.querySelector('.tarjeta__detalle').textContent = t.detalle ? t.detalle(j) : '';
      el.querySelector('.extra').textContent = t.extra ? t.extra(j) : '';
      const insignia = el.querySelector('.insignia');
      if (t.sensor) {
        const s = t.sensor(j);
        insignia.textContent = s.sensor;
        insignia.className = 'insignia insignia--' + s.sensor;
      } else {
        insignia.textContent = j.et0 === 'disponible' ? 'DISPONIBLE' : 'NO DISPONIBLE';
        insignia.className = 'insignia insignia--NEUTRA';
      }
    }
    actualizarEdades();
  }

  /** Actualiza la antigüedad mostrada de cada dato; se llama cada segundo. */
  function actualizarEdades() {
    if (!ultimo) return;
    for (const t of TARJETAS) {
      t.el.querySelector('.tarjeta__edad').textContent = t.sensor ? textoEdad(t.sensor(ultimo)) : '';
    }
  }

  // -------------------------------------------------------------------------
  // Estado de alerta y desactivación de la alarma física
  // -------------------------------------------------------------------------

  let orden = null;  // { enviadaMs } mientras se espera la confirmación.

  /** Muestra el estado de alerta, sus causas y el estado del botón de desactivar. */
  function actualizarEstado(j) {
    const seccion = $('estado');
    seccion.className = 'estado estado--' + j.estado.replace(' ', '_');
    $('estado-nombre').textContent = NOMBRE_ESTADO[j.estado] || j.estado;
    $('estado-icono').textContent = ICONO_ESTADO[j.estado] || '?';

    const lista = $('estado-causas');
    lista.textContent = '';
    const causas = j.causas.length ? j.causas.map((c) => TEXTO_CAUSA[c] || c)
      : [j.estado === 'FALLA NIVEL' ? 'Sin lectura válida del nivel: no se evalúa el riesgo'
        : j.estado === 'INICIANDO' ? 'Esperando la primera lectura válida del nivel'
          : 'Sin condiciones de riesgo activas'];
    for (const texto of causas) {
      const li = document.createElement('li');
      li.textContent = texto;
      if (!j.causas.length) li.className = 'sin-causas';
      lista.appendChild(li);
    }

    $('alarma-desactivada').hidden = !j.alarma_desactivada;
    const estadoConAlarma = ESTADOS_DESACTIVABLES.includes(j.estado);
    const desactivable = estadoConAlarma && !j.alarma_desactivada;
    $('boton-desactivar').disabled = !desactivable || orden !== null;
    $('boton-nota').hidden = estadoConAlarma;

    if (orden) {
      if (j.alarma_desactivada) {
        $('orden-mensaje').textContent = 'El equipo confirmó la desactivación de la alarma física.';
        orden = null;
      } else if (Date.now() - orden.enviadaMs > ESPERA_CONFIRMACION_MS) {
        $('orden-mensaje').textContent =
          'El equipo no confirmó la desactivación. Solo se aplica en ALERTA o CRÍTICO.';
        orden = null;
      }
    }
  }

  /** Conecta el botón y el diálogo de confirmación con la orden de desactivar la alarma física. */
  function configurarDesactivacion() {
    const dialogo = $('dialogo-desactivar');
    $('boton-desactivar').addEventListener('click', () => dialogo.showModal());
    dialogo.addEventListener('close', async () => {
      if (dialogo.returnValue !== 'confirmar') return;
      $('boton-desactivar').disabled = true;
      $('orden-mensaje').textContent = 'Enviando la orden…';
      try {
        const r = await api('/api/alarma/desactivar', { method: 'POST' });
        if (r.status === 202) {
          orden = { enviadaMs: Date.now() };
          $('orden-mensaje').textContent = 'Orden recibida; esperando la confirmación del equipo…';
        } else {
          $('orden-mensaje').textContent = mensajeError(r.status);
          $('boton-desactivar').disabled = false;
        }
      } catch (e) {
        $('orden-mensaje').textContent = 'No se pudo enviar la orden: sin conexión con el equipo.';
        $('boton-desactivar').disabled = false;
      }
    });
  }

  // -------------------------------------------------------------------------
  // Recepción de datos: WebSocket y sondeo de respaldo
  // -------------------------------------------------------------------------

  let ultimo = null;
  let recibidoMs = 0;
  let ws = null;
  let wsAbierto = false;
  let sondeo = null;

  /** Procesa un JSON del estado actual recibido por WebSocket o por consulta. */
  function procesar(j) {
    ultimo = j;
    recibidoMs = Date.now();
    $('banda-simulada').hidden = j.fuente !== 'servidor simulado';
    actualizarEstado(j);
    actualizarTarjetas(j);
    $('wifi').textContent = 'Wi-Fi: ' + (j.wifi_rssi === null || j.wifi_rssi === undefined ? '—' : j.wifi_rssi + ' dBm');
    $('pie-ciclo').textContent = 'Ciclo ' + j.ciclo;
    $('pie-arranque').textContent = 'Equipo encendido hace ' + duracion(j.t_s);
    $('pie-hora').textContent = j.epoca === null
      ? 'Hora sin sincronizar por NTP'
      : 'Hora local ' + formatoHoraSegundos.format(new Date(j.epoca * 1000)) + ' (NTP)';
    actualizarConexion();
  }

  /** Consulta /api/actual; es el respaldo mientras no hay WebSocket. */
  async function consultarActual() {
    try {
      const r = await api('/api/actual');
      if (r.ok) {
        procesar(await r.json());
      } else if (r.status !== 503) {
        $('conexion-texto').textContent = mensajeError(r.status);
      }
    } catch (e) {
      // Sin conexión: lo indica actualizarConexion().
    }
  }

  /** Inicia la consulta periódica de respaldo, si no está activa. */
  function iniciarSondeo() {
    if (sondeo === null) sondeo = setInterval(consultarActual, SONDEO_MS);
  }

  /** Detiene la consulta periódica de respaldo. */
  function detenerSondeo() {
    if (sondeo !== null) clearInterval(sondeo);
    sondeo = null;
  }

  /** Abre el WebSocket /ws con el token y se reconecta solo si se cierra. */
  function conectarWs() {
    const protocolo = location.protocol === 'https:' ? 'wss://' : 'ws://';
    ws = new WebSocket(protocolo + location.host + '/ws?token=' + encodeURIComponent(TOKEN));
    ws.onopen = () => { wsAbierto = true; detenerSondeo(); actualizarConexion(); };
    ws.onmessage = (evento) => {
      try { procesar(JSON.parse(evento.data)); } catch (e) { /* mensaje incompleto: se ignora */ }
    };
    ws.onclose = () => {
      wsAbierto = false;
      iniciarSondeo();
      actualizarConexion();
      setTimeout(conectarWs, RECONEXION_WS_MS);
    };
  }

  /** Actualiza el indicador de conexión: en vivo, reconectando o sin datos. */
  function actualizarConexion() {
    const el = $('conexion');
    const texto = $('conexion-texto');
    const sinDatosMs = Date.now() - recibidoMs;
    if (ultimo && sinDatosMs > SIN_DATOS_MS) {
      el.className = 'conexion__estado conexion--sin-datos';
      texto.textContent = 'Sin datos hace ' + duracion(sinDatosMs / 1000);
    } else if (wsAbierto) {
      el.className = 'conexion__estado conexion--en-vivo';
      texto.textContent = 'En vivo';
    } else {
      el.className = 'conexion__estado conexion--reconectando';
      texto.textContent = ultimo ? 'Reconectando… (consulta cada 5 s)' : 'Conectando…';
    }
  }

  // -------------------------------------------------------------------------
  // Histórico
  // -------------------------------------------------------------------------

  let buferActivo = 'rapido';
  let temporizadorHistorico = null;
  let usaHora = false;  // true: eje en hora local; false: "hace X min".
  let pasoEje = 120;    // Separación de las marcas rotuladas del eje x.
  const graficas = {};

  /** Marcas del eje x en los múltiplos exactos de pasoEje dentro del rango. */
  function marcasEje(eje) {
    const marcas = [];
    for (let v = Math.ceil(eje.min / pasoEje) * pasoEje; v <= eje.max; v += pasoEje) {
      marcas.push({ value: v });
    }
    eje.ticks = marcas;
  }

  /** Rótulo del eje x: hora local si hay hora NTP, o tiempo relativo («hace X min»). */
  function formatoEje(valor) {
    if (usaHora) return formatoHora.format(new Date(valor * 1000));
    const minutos = Math.round(-valor);
    if (minutos === 0) return 'ahora';
    if (minutos >= 60 && minutos % 60 === 0) return 'hace ' + minutos / 60 + ' h';
    return 'hace ' + duracion(minutos * 60);
  }

  /** Opciones comunes de Chart.js para las gráficas del histórico. */
  function opcionesGrafica(ejes) {
    const scales = {
      x: {
        type: 'linear',
        afterBuildTicks: marcasEje,
        ticks: { callback: formatoEje, maxRotation: 0, autoSkipPadding: 12 },
        grid: { color: '#eef1f4' },
      },
    };
    for (const [id, eje] of Object.entries(ejes)) {
      scales[id] = Object.assign({ grid: { color: '#eef1f4' } }, eje);
    }
    return {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      spanGaps: false,
      interaction: { mode: 'index', intersect: false },
      plugins: {
        legend: { labels: { boxWidth: 14 } },
        tooltip: { callbacks: { title: (items) => items.length ? formatoEje(items[0].parsed.x) : '' } },
      },
      scales,
    };
  }

  /** Serie de datos de una gráfica de líneas. */
  function serie(etiqueta, color, eje, extra) {
    return Object.assign({
      label: etiqueta, data: [], borderColor: color, backgroundColor: color,
      yAxisID: eje, borderWidth: 2, pointRadius: 0, tension: 0.2,
    }, extra || {});
  }

  /** Serie punteada para dibujar un umbral horizontal. */
  function umbral(etiqueta, color) {
    return serie(etiqueta, color, 'y', { borderDash: [6, 4], borderWidth: 1.5, tension: 0 });
  }

  /** Crea las gráficas de nivel, de temperatura y humedad, y de VPD y UV. */
  function crearGraficas() {
    Chart.defaults.font.family = getComputedStyle(document.body).fontFamily;
    Chart.defaults.color = '#57606a';
    Chart.defaults.locale = 'en-US';  // Punto decimal, como en la Wiki.

    graficas.nivel = new Chart($('grafica-nivel'), {
      type: 'line',
      data: { datasets: [
        serie('Nivel (%)', '#1565c0', 'y', { fill: { target: 'origin' }, backgroundColor: 'rgba(21, 101, 192, 0.12)' }),
        umbral('Umbral preventivo (' + UMBRAL_PREVENTIVO_PCT + ' %)', '#ef6c00'),
        umbral('Umbral crítico (' + UMBRAL_CRITICO_PCT + ' %)', '#c62828'),
      ] },
      options: opcionesGrafica({ y: { min: 0, max: 100, title: { display: true, text: '%' } } }),
    });

    graficas.clima = new Chart($('grafica-clima'), {
      type: 'line',
      data: { datasets: [
        serie('Temperatura (°C)', '#c62828', 'y'),
        serie('Humedad relativa (%)', '#00897b', 'y1'),
      ] },
      options: opcionesGrafica({
        y: { title: { display: true, text: '°C' } },
        y1: { position: 'right', min: 0, max: 100, title: { display: true, text: '%' }, grid: { drawOnChartArea: false } },
      }),
    });

    graficas.vpd = new Chart($('grafica-vpd'), {
      type: 'line',
      data: { datasets: [
        serie('VPD (kPa)', '#6a1b9a', 'y'),
        serie('Índice UV', '#f9a825', 'y1'),
      ] },
      options: opcionesGrafica({
        y: { min: 0, title: { display: true, text: 'kPa' } },
        y1: { position: 'right', min: 0, title: { display: true, text: 'Índice UV' }, grid: { drawOnChartArea: false } },
      }),
    });
  }

  /** Dibuja el histórico recibido en las tres gráficas. */
  function dibujarHistorico(h) {
    usaHora = h.ahora_epoca !== null && h.ahora_epoca !== undefined;
    // x: hora de época si es válida; si no, minutos relativos (negativos) al momento actual.
    const x = (r) => usaHora ? h.ahora_epoca - (h.ahora_s - r.t_s) : -(h.ahora_s - r.t_s) / 60;
    const puntos = (campo) => h.registros.map((r) => ({ x: x(r), y: r[campo] }));

    // Marcas rotuladas en múltiplos redondos: 2 min (rápido) o 3 h (lento).
    pasoEje = h.bufer === 'rapido' ? (usaHora ? 120 : 2) : (usaHora ? 10800 : 180);
    const n = h.registros.length;
    const xMin = n ? x(h.registros[0]) : (usaHora ? h.ahora_epoca - 600 : -10);
    const xMax = n ? x(h.registros[n - 1]) : (usaHora ? h.ahora_epoca : 0);
    const linea = (valor) => [{ x: xMin, y: valor }, { x: xMax, y: valor }];

    graficas.nivel.data.datasets[0].data = puntos('nivel_pct');
    graficas.nivel.data.datasets[1].data = linea(UMBRAL_PREVENTIVO_PCT);
    graficas.nivel.data.datasets[2].data = linea(UMBRAL_CRITICO_PCT);
    graficas.clima.data.datasets[0].data = puntos('temperatura_c');
    graficas.clima.data.datasets[1].data = puntos('humedad_pct');
    graficas.vpd.data.datasets[0].data = puntos('vpd_kpa');
    graficas.vpd.data.datasets[1].data = puntos('uv_indice');

    // Rango mínimo de ±2 °C, para que el ruido de décimas no parezca una variación.
    const temperaturas = h.registros.map((r) => r.temperatura_c).filter((v) => v !== null);
    const ejeT = graficas.clima.options.scales.y;
    ejeT.suggestedMin = temperaturas.length ? Math.floor(Math.min(...temperaturas) - 2) : undefined;
    ejeT.suggestedMax = temperaturas.length ? Math.ceil(Math.max(...temperaturas) + 2) : undefined;
    for (const g of Object.values(graficas)) {
      g.options.scales.x.min = xMin;
      g.options.scales.x.max = xMax;
      g.update();
    }

    const ventana = h.bufer === 'rapido' ? 'últimos 10 min' : 'últimas 24 h';
    $('historico-nota').textContent =
      n + ' registros cada ' + duracion(h.periodo_s) + ' (' + ventana + '). ' +
      (usaHora ? 'Eje en hora local (NTP).' : 'Hora sin sincronizar: eje en minutos antes del momento actual.') +
      ' Los huecos son lecturas sin dato válido. Los umbrales de nivel son los de demostración.';
  }

  /** Pide el búfer del histórico de la pestaña activa y lo dibuja. */
  async function cargarHistorico() {
    try {
      const r = await api('/api/historico?b=' + buferActivo);
      if (!r.ok) {
        $('historico-nota').textContent = 'No se pudo cargar el histórico: ' + mensajeError(r.status);
        return;
      }
      const h = await r.json();
      if (h.bufer === buferActivo) dibujarHistorico(h);
    } catch (e) {
      $('historico-nota').textContent = 'No se pudo cargar el histórico: sin conexión con el equipo.';
    }
  }

  /** Cambia de pestaña (10 min o 24 h) y programa la recarga del histórico. */
  function seleccionarBufer(bufer) {
    buferActivo = bufer;
    for (const b of document.querySelectorAll('.pestanas button')) {
      b.setAttribute('aria-selected', String(b.dataset.bufer === bufer));
    }
    clearInterval(temporizadorHistorico);
    cargarHistorico();
    temporizadorHistorico = setInterval(cargarHistorico, PERIODO_HISTORICO_MS[bufer]);
  }

  // -------------------------------------------------------------------------
  // Arranque
  // -------------------------------------------------------------------------

  crearTarjetas();
  crearGraficas();
  configurarDesactivacion();
  for (const b of document.querySelectorAll('.pestanas button')) {
    b.addEventListener('click', () => seleccionarBufer(b.dataset.bufer));
  }
  consultarActual();
  conectarWs();
  seleccionarBufer('rapido');
  setInterval(() => { actualizarEdades(); actualizarConexion(); }, 1000);
})();
