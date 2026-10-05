/**
 * @file web.cpp
 * @brief Implementación del servidor web del tablero.
 *
 * Los manejadores corren en la tarea de AsyncTCP, no en las tareas de
 * medición: solo copian el estado publicado (mtxDatos) o el histórico (mutex
 * propio) y responden. No hay MQTT.
 */
#include "web.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_timer.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#include "config.h"
#include "credenciales.h"
#include "historico.h"
#include "red.h"
#include "registro.h"
#include "tareas.h"

namespace {

AsyncWebServer servidor(WEB_PUERTO);
AsyncWebSocket ws("/ws");
bool sistemaArchivos = false;

// ---------------------------------------------------------------------------
// Construcción de JSON en un búfer fijo
// ---------------------------------------------------------------------------

/** Escribe texto con formato al final de un búfer, sin desbordarlo. */
class Json {
public:
    Json(char* bufer, size_t tamano) : b_(bufer), t_(tamano) { b_[0] = '\0'; }

    /** Agrega texto con formato al final del búfer; marca el desbordamiento si no cabe. */
    void agregar(const char* formato, ...) __attribute__((format(printf, 2, 3))) {
        if (n_ + 1 >= t_) {
            desbordado_ = true;
            return;
        }
        va_list argumentos;
        va_start(argumentos, formato);
        const int escritos = vsnprintf(b_ + n_, t_ - n_, formato, argumentos);
        va_end(argumentos);
        if (escritos < 0 || static_cast<size_t>(escritos) >= t_ - n_) {
            desbordado_ = true;
            n_ = t_ - 1;
        } else {
            n_ += static_cast<size_t>(escritos);
        }
    }

    /** "clave":valor con los decimales dados, o "clave":null si es NAN. */
    void numero(const char* clave, float valor, int decimales) {
        if (std::isnan(valor)) {
            agregar("\"%s\":null", clave);
        } else {
            agregar("\"%s\":%.*f", clave, decimales, static_cast<double>(valor));
        }
    }

    bool desbordado() const { return desbordado_; }

private:
    char*  b_;
    size_t t_;
    size_t n_ = 0;
    bool   desbordado_ = false;
};

/** Estado del sensor para el tablero: INICIANDO mientras no haya primera lectura válida. */
const char* textoSensor(EstadoSensor estado, bool iniciado) {
    return iniciado ? textoEstado(estado) : "INICIANDO";
}

/** "sensor":"…","edad_s":n (null si el sensor no se ha iniciado). */
void estadoSensor(Json& j, EstadoSensor estado, bool iniciado, uint32_t edadS) {
    j.agregar("\"sensor\":\"%s\",", textoSensor(estado, iniciado));
    if (iniciado) {
        j.agregar("\"edad_s\":%lu", static_cast<unsigned long>(edadS));
    } else {
        j.agregar("\"edad_s\":null");
    }
}

/** JSON de /api/actual y del WebSocket. @return false si no cupo en el búfer. */
bool jsonActual(const EstadoPublicado& e, char* bufer, size_t tamano) {
    const Lecturas&  l = e.instantanea.lecturas;
    const Derivados& d = e.derivados;
    Json j(bufer, tamano);

    j.agregar("{\"ciclo\":%lu,\"t_s\":%lu,",
              static_cast<unsigned long>(e.instantanea.ciclo),
              static_cast<unsigned long>(e.instantanea.marcaTiempoUs / 1000000));
    // Hora de época del envío si el NTP ya fijó la hora; si no, null.
    uint32_t epoca = 0;
    if (redHoraEpoca(epoca)) {
        j.agregar("\"epoca\":%lu,", static_cast<unsigned long>(epoca));
    } else {
        j.agregar("\"epoca\":null,");
    }
    j.agregar("\"estado\":\"%s\",\"causas\":[", textoAlerta(d.estado));
    const struct { uint8_t bit; const char* nombre; } causas[] = {
        {CAUSA_NIVEL_CRITICO, "nivel_critico"}, {CAUSA_NIVEL_PREVENTIVO, "nivel_preventivo"},
        {CAUSA_DESCENSO, "descenso"},           {CAUSA_VPD_ALTO, "vpd_alto"},
        {CAUSA_T_ALTA, "temperatura_alta"},     {CAUSA_UV_ALTO, "uv_alto"}};
    bool primera = true;
    for (const auto& c : causas) {
        if (d.causas & c.bit) {
            j.agregar("%s\"%s\"", primera ? "" : ",", c.nombre);
            primera = false;
        }
    }
    j.agregar("],\"alarma_desactivada\":%s,", d.alarmaDesactivada ? "true" : "false");

    // Nivel y tendencia.
    j.agregar("\"nivel\":{");
    j.numero("cm", d.nivelCm, 1);
    j.agregar(",");
    j.numero("pct", d.nivelPct, 0);
    j.agregar(",");
    j.numero("tendencia_cm_min", d.pendienteValida ? d.pendienteCmMin : NAN, 2);
    j.agregar(",\"tendencia\":\"%s\",\"compensacion\":\"%s\",",
              d.pendienteValida ? "disponible" : "tendencia no disponible",
              textoCompensacion(l.compensacion));
    estadoSensor(j, l.estadoNivel, l.iniciadoNivel, l.edadNivelS);

    // DHT22.
    j.agregar("},\"dht22\":{");
    j.numero("temperatura_c", l.temperaturaDhtC, 1);
    j.agregar(",");
    j.numero("humedad_pct", l.humedadPct, 0);
    j.agregar(",");
    estadoSensor(j, l.estadoDht, l.iniciadoDht, l.edadDhtS);

    // BMP180.
    j.agregar("},\"bmp180\":{");
    j.numero("presion_hpa", l.presionHpa, 1);
    j.agregar(",");
    estadoSensor(j, l.estadoBmp, l.iniciadoBmp, l.edadBmpS);

    // GUVA-S12SD.
    j.agregar("},\"guva\":{");
    j.numero("uv_indice", l.uvIndice, 1);
    j.agregar(",");
    j.numero("mv", l.uvMilivoltios, 0);
    j.agregar(",");
    estadoSensor(j, l.estadoUv, l.iniciadoUv, l.edadUvS);

    // Magnitudes derivadas.
    j.agregar("},");
    j.numero("vpd_kpa", d.vpdValido ? d.vpdKpa : NAN, 2);
    j.agregar(",");
    j.numero("et0_mm_dia", d.et0Disponible ? d.et0MmDia : NAN, 1);
    j.agregar(",\"et0\":\"%s\",\"wifi_rssi\":%d}",
              d.et0Disponible ? "disponible" : "no disponible", static_cast<int>(WiFi.RSSI()));
    return !j.desbordado();
}

/** Número con decimales o null, para el histórico. */
void numeroONull(char* salida, size_t tamano, float valor, int decimales) {
    if (std::isnan(valor)) {
        snprintf(salida, tamano, "null");
    } else {
        snprintf(salida, tamano, "%.*f", decimales, static_cast<double>(valor));
    }
}

// ---------------------------------------------------------------------------
// Control de acceso
// ---------------------------------------------------------------------------

/**
 * true si la dirección IP de origen pertenece a la subred de la WLAN, o si el
 * filtro está desactivado (WEB_FILTRO_SUBRED, solo en la simulación).
 */
bool enSubred(AsyncWebServerRequest* request) {
    if (!WEB_FILTRO_SUBRED) {
        return true;
    }
    const uint32_t mascara = static_cast<uint32_t>(WiFi.subnetMask());
    const uint32_t origen = static_cast<uint32_t>(request->client()->remoteIP());
    const uint32_t propia = static_cast<uint32_t>(WiFi.localIP());
    return (origen & mascara) == (propia & mascara);
}

/** true si la petición trae el token de un dispositivo autorizado (encabezado o parámetro). */
bool tokenValido(AsyncWebServerRequest* request) {
    String token;
    if (request->hasHeader(WEB_TOKEN_ENCABEZADO)) {
        token = request->getHeader(WEB_TOKEN_ENCABEZADO)->value();
    } else if (request->hasParam(WEB_TOKEN_PARAMETRO)) {
        token = request->getParam(WEB_TOKEN_PARAMETRO)->value();
    } else {
        return false;
    }
    for (size_t i = 0; i < NUM_TOKENS_AUTORIZADOS; i++) {
        if (token.equals(TOKENS_AUTORIZADOS[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Aplica el control de acceso; si falla, responde y devuelve false.
 *
 * Orden: subred (403), token (403) y autenticación Digest (401 con desafío).
 */
bool autorizar(AsyncWebServerRequest* request) {
    if (!enSubred(request)) {
        request->send(403, "text/plain", "Acceso solo desde la WLAN local");
        return false;
    }
    if (!tokenValido(request)) {
        registrar("web", "petición rechazada: token ausente o no autorizado (%s)",
                  request->client()->remoteIP().toString().c_str());
        request->send(403, "text/plain", "Dispositivo no autorizado");
        return false;
    }
    if (!request->authenticate(TABLERO_USUARIO_DIGEST, TABLERO_CLAVE_DIGEST, WEB_REALM)) {
        request->requestAuthentication(WEB_REALM, true);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Manejadores
// ---------------------------------------------------------------------------

/** Página mínima mientras el frontend del tablero (data/index.html) no esté cargado. */
const char PAGINA_SIN_TABLERO[] =
    "<!DOCTYPE html><html lang=\"es\"><meta charset=\"utf-8\">"
    "<title>Tablero</title><h1>Tablero de monitoreo h&iacute;drico</h1>"
    "<p>El frontend del tablero no est&aacute; cargado en el equipo "
    "(data/index.html en LittleFS).</p>"
    "<p>Datos disponibles: <code>/api/actual</code>, "
    "<code>/api/historico?b=rapido</code>, <code>/api/historico?b=lento</code>.</p></html>";

/** GET /: entrega el frontend del tablero o la página mínima. */
void manejarRaiz(AsyncWebServerRequest* request) {
    if (!autorizar(request)) {
        return;
    }
    if (sistemaArchivos && LittleFS.exists("/index.html")) {
        request->send(LittleFS, "/index.html", "text/html");
    } else {
        request->send(200, "text/html", PAGINA_SIN_TABLERO);
    }
}

/** GET /api/actual: JSON del estado actual. */
void manejarActual(AsyncWebServerRequest* request) {
    if (!autorizar(request)) {
        return;
    }
    EstadoPublicado copia{};
    if (!leerEstadoPublicado(copia)) {
        request->send(503, "application/json", "{\"error\":\"sin estado publicado todavía\"}");
        return;
    }
    char json[1024];
    if (!jsonActual(copia, json, sizeof(json))) {
        request->send(500, "application/json", "{\"error\":\"respuesta demasiado larga\"}");
        return;
    }
    request->send(200, "application/json", json);
}

/** GET /api/historico: JSON del búfer rápido o lento. */
void manejarHistorico(AsyncWebServerRequest* request) {
    if (!autorizar(request)) {
        return;
    }
    if (!request->hasParam("b")) {
        request->send(400, "application/json", "{\"error\":\"falta el parámetro b (rapido o lento)\"}");
        return;
    }
    const String& nombre = request->getParam("b")->value();
    Bufer bufer;
    if (nombre == "rapido") {
        bufer = Bufer::RAPIDO;
    } else if (nombre == "lento") {
        bufer = Bufer::LENTO;
    } else {
        request->send(400, "application/json", "{\"error\":\"b debe ser rapido o lento\"}");
        return;
    }

    // Se copia el búfer para no retener su mutex mientras se arma la respuesta.
    std::unique_ptr<RegistroHistorico[]> copia(
        new (std::nothrow) RegistroHistorico[historicoCapacidad(bufer)]);
    if (!copia) {
        request->send(503, "application/json", "{\"error\":\"memoria insuficiente\"}");
        return;
    }
    const size_t n = historicoCopiar(bufer, copia.get());

    // ahora_s y ahora_epoca son el mismo instante: con ellos el tablero convierte
    // el t_s de cada registro (tiempo desde el arranque) a hora local.
    AsyncResponseStream* respuesta = request->beginResponseStream("application/json");
    respuesta->printf("{\"bufer\":\"%s\",\"periodo_s\":%lu,\"ahora_s\":%lu,",
                      nombre.c_str(), static_cast<unsigned long>(historicoPeriodoS(bufer)),
                      static_cast<unsigned long>(esp_timer_get_time() / 1000000));
    uint32_t epoca = 0;
    if (redHoraEpoca(epoca)) {
        respuesta->printf("\"ahora_epoca\":%lu,", static_cast<unsigned long>(epoca));
    } else {
        respuesta->print("\"ahora_epoca\":null,");
    }
    respuesta->print("\"registros\":[");
    char a[16], b[16], c[16], d[16], e[16], f[16], g[16];
    for (size_t i = 0; i < n; i++) {
        const RegistroHistorico& r = copia[i];
        numeroONull(a, sizeof(a), r.nivelCm, 1);
        numeroONull(b, sizeof(b), r.nivelPct, 0);
        numeroONull(c, sizeof(c), r.temperaturaC, 1);
        numeroONull(d, sizeof(d), r.humedadPct, 0);
        numeroONull(e, sizeof(e), r.presionHpa, 1);
        numeroONull(f, sizeof(f), r.uvIndice, 1);
        numeroONull(g, sizeof(g), r.vpdKpa, 2);
        respuesta->printf("%s{\"t_s\":%lu,\"nivel_cm\":%s,\"nivel_pct\":%s,\"temperatura_c\":%s,"
                          "\"humedad_pct\":%s,\"presion_hpa\":%s,\"uv_indice\":%s,\"vpd_kpa\":%s,"
                          "\"estado\":\"%s\"}",
                          i == 0 ? "" : ",", static_cast<unsigned long>(r.tiempoS),
                          a, b, c, d, e, f, g, textoAlerta(r.estado));
    }
    respuesta->print("]}");
    request->send(respuesta);
}

/** POST /api/alarma/desactivar: envía la orden a la tarea de fusión. */
void manejarDesactivar(AsyncWebServerRequest* request) {
    if (!autorizar(request)) {
        return;
    }
    tareasSolicitarDesactivarAlarma();
    registrar("web", "orden de desactivar la alarma física desde %s",
              request->client()->remoteIP().toString().c_str());
    request->send(202, "application/json",
                  "{\"orden\":\"recibida\",\"nota\":\"se aplica solo en ALERTA o CRITICO; "
                  "el resultado se ve en /api/actual\"}");
}

/**
 * @brief Rutas no registradas: archivos del frontend en LittleFS, o 404.
 *
 * Los archivos del tablero (CSS, JS y Chart.js) pasan por el mismo control de
 * acceso que las demás rutas. Fuera de la subred se responde 403.
 */
void manejarOtraRuta(AsyncWebServerRequest* request) {
    if (!enSubred(request)) {
        request->send(403, "text/plain", "Acceso solo desde la WLAN local");
        return;
    }
    const String& ruta = request->url();
    const bool esArchivo = request->method() == HTTP_GET && sistemaArchivos &&
                           ruta.indexOf("..") < 0 && LittleFS.exists(ruta);
    if (!esArchivo) {
        request->send(404, "text/plain", "No encontrado");
        return;
    }
    if (!autorizar(request)) {
        return;
    }
    // Sin tipo explícito: la librería lo deduce de la extensión del archivo.
    request->send(LittleFS, ruta);
}

void eventoWs(AsyncWebSocket*, AsyncWebSocketClient* cliente, AwsEventType tipo,
              void*, uint8_t*, size_t) {
    if (tipo == WS_EVT_CONNECT) {
        registrar("web", "WebSocket: cliente %lu conectado desde %s",
                  static_cast<unsigned long>(cliente->id()), cliente->remoteIP().toString().c_str());
    } else if (tipo == WS_EVT_DISCONNECT) {
        registrar("web", "WebSocket: cliente %lu desconectado", static_cast<unsigned long>(cliente->id()));
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Interfaz pública
// ---------------------------------------------------------------------------

bool webIniciar() {
    sistemaArchivos = LittleFS.begin(false);
    registrar("web", "LittleFS %s; tablero %s", sistemaArchivos ? "montado" : "no disponible",
              (sistemaArchivos && LittleFS.exists("/index.html")) ? "cargado" : "sin cargar (página mínima)");

    // El WebSocket exige subred y token en el saludo inicial.
    ws.handleHandshake([](AsyncWebServerRequest* request) {
        return enSubred(request) && tokenValido(request);
    });
    ws.onEvent(eventoWs);
    servidor.addHandler(&ws);

    servidor.on("/", HTTP_GET, manejarRaiz);
    servidor.on("/api/actual", HTTP_GET, manejarActual);
    servidor.on("/api/historico", HTTP_GET, manejarHistorico);
    servidor.on("/api/alarma/desactivar", HTTP_POST, manejarDesactivar);
    servidor.onNotFound(manejarOtraRuta);

    servidor.begin();
    registrar("web", "servidor en http://%s:%u", WiFi.localIP().toString().c_str(),
              static_cast<unsigned>(WEB_PUERTO));
    if (!WEB_FILTRO_SUBRED) {
        registrar("web", "AVISO: filtro de subred desactivado (ajuste de simulación)");
    }
    return true;
}

/** Envía el estado actual a los clientes WebSocket y libera los desconectados. */
void webDifundir() {
    ws.cleanupClients(WEB_MAX_CLIENTES_WS);
    if (ws.count() == 0) {
        return;
    }
    EstadoPublicado copia{};
    if (!leerEstadoPublicado(copia)) {
        return;
    }
    char json[1024];
    if (jsonActual(copia, json, sizeof(json))) {
        ws.textAll(json);
    }
}
