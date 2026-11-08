/**
 * @file mqtt.cpp
 * @brief Comunicación MQTT con el gateway: conexión, última voluntad, estado
 *        del nodo, telemetría, alertas y orden de desactivar la alarma
 *        (Challenge #3).
 */
#include "mqtt.h"

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mqtt_client.h>

#include "config.h"
#include "credenciales.h"
#include "mensajes_mqtt.h"
#include "red.h"
#include "registro.h"
#include "tareas.h"

namespace {

esp_mqtt_client_handle_t cliente = nullptr;
std::atomic<bool> conectado{false};
std::atomic<uint32_t> conexiones{0};
std::atomic<uint32_t> desconectadoDesdeMs{0};  ///< 0 si nunca se ha desconectado.

char topicoEstado[64];
char topicoTelemetria[64];
char topicoAlerta[64];
char topicoOrden[64];
char topicoRespuesta[64];

/** Orden recibida: la deja el manejador de eventos y la atiende tMqtt. */
struct OrdenRecibida {
    char id[mensajes_mqtt::MAX_ORDEN_ID + 1];
};
QueueHandle_t qOrdenes = nullptr;

// Solo los usa tMqtt (mqttCiclo).
uint32_t     contadorMensajes = 0;  ///< Campo "id" de cada mensaje.
uint32_t     ultimaTelemetriaMs = 0;
bool         hayEstadoPrevio = false;
EstadoAlerta estadoPrevio = EstadoAlerta::INICIANDO;
uint32_t     fallosEncolado = 0;

/** Orden aceptada en espera de que tFusion confirme la desactivación. */
bool     ordenPendiente = false;
char     ordenPendienteId[mensajes_mqtt::MAX_ORDEN_ID + 1];
uint32_t ordenPendienteDesdeMs = 0;

/** Arma "sabana/{municipio}/{nodo}/{tipo}". */
void armarTopico(char* destino, size_t tamano, const char* tipo) {
    snprintf(destino, tamano, "%s/%s/%s/%s", MQTT_RAIZ, MQTT_MUNICIPIO, MQTT_NODO, tipo);
}

/**
 * @brief Eventos del cliente MQTT. Corre en la tarea del cliente de ESP-IDF,
 *        no en tMqtt: solo actualiza banderas, registra y encola mensajes.
 */
void alEvento(void*, esp_event_base_t, int32_t idEvento, void* datos) {
    auto* evento = static_cast<esp_mqtt_event_handle_t>(datos);
    switch (static_cast<esp_mqtt_event_id_t>(idEvento)) {
        case MQTT_EVENT_CONNECTED: {
            conectado.store(true);
            const uint32_t n = conexiones.fetch_add(1) + 1;
            const uint32_t desde = desconectadoDesdeMs.load();
            if (desde == 0) {
                registrar("mqtt", "conectado al broker (conexión %" PRIu32 ")", n);
            } else {
                registrar("mqtt", "reconectado al broker en %" PRIu32 " ms (conexión %" PRIu32 ")",
                          static_cast<uint32_t>(millis() - desde), n);
            }
            // Estado retenido: el gateway sabe que el nodo está en línea aunque
            // se suscriba después.
            esp_mqtt_client_enqueue(evento->client, topicoEstado, MQTT_ESTADO_EN_LINEA, 0,
                                    MQTT_QOS_ESTADO, 1, true);
            // Con sesión limpia, la suscripción se renueva en cada conexión.
            esp_mqtt_client_subscribe(evento->client, topicoOrden, MQTT_QOS_ORDEN);
            break;
        }
        case MQTT_EVENT_DATA: {
            // Solo la orden de desactivar; un mensaje fragmentado no es una orden válida.
            const bool esOrden = evento->topic_len == static_cast<int>(strlen(topicoOrden)) &&
                                 strncmp(evento->topic, topicoOrden, evento->topic_len) == 0;
            if (!esOrden || evento->data_len != evento->total_data_len) {
                break;
            }
            OrdenRecibida orden;
            if (!mensajes_mqtt::leerOrden(evento->data, evento->data_len, orden.id)) {
                registrar("mqtt", "orden ignorada: sin orden_id válido");
                break;
            }
            if (xQueueSend(qOrdenes, &orden, 0) != pdTRUE) {
                registrar("mqtt", "orden %s descartada: cola de órdenes llena", orden.id);
            }
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            if (conectado.exchange(false)) {
                desconectadoDesdeMs.store(millis());
                registrar("mqtt", "desconectado del broker; el cliente reintenta solo");
            }
            break;
        case MQTT_EVENT_ERROR:
            if (evento->error_handle != nullptr &&
                evento->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                registrar("mqtt", "conexión rechazada por el broker (código %d): revisar usuario y clave",
                          static_cast<int>(evento->error_handle->connect_return_code));
            } else {
                registrar("mqtt", "error de conexión con el broker");
            }
            break;
        default:
            break;
    }
}

/** Crea el cliente con la última voluntad y lo inicia. @return true si arrancó. */
bool iniciarCliente() {
    armarTopico(topicoEstado, sizeof(topicoEstado), "estado");
    armarTopico(topicoTelemetria, sizeof(topicoTelemetria), "telemetria");
    armarTopico(topicoAlerta, sizeof(topicoAlerta), "alerta");
    armarTopico(topicoOrden, sizeof(topicoOrden), "cmd/desactivar_alarma");
    armarTopico(topicoRespuesta, sizeof(topicoRespuesta), "cmd/respuesta");
    if (qOrdenes == nullptr) {
        qOrdenes = xQueueCreate(4, sizeof(OrdenRecibida));
        if (qOrdenes == nullptr) {
            registrar("mqtt", "ERROR: no se pudo crear la cola de órdenes");
            return false;
        }
    }

    esp_mqtt_client_config_t cfg = {};
    cfg.uri         = MQTT_URI_BROKER;
    cfg.client_id   = MQTT_ID_CLIENTE;
    cfg.username    = MQTT_USUARIO_BROKER;
    cfg.password    = MQTT_CLAVE_BROKER;
    cfg.keepalive   = MQTT_KEEPALIVE_S;
    cfg.lwt_topic   = topicoEstado;
    cfg.lwt_msg     = MQTT_ESTADO_FUERA_DE_LINEA;
    cfg.lwt_qos     = MQTT_QOS_ESTADO;
    cfg.lwt_retain  = 1;

    cliente = esp_mqtt_client_init(&cfg);
    if (cliente == nullptr) {
        registrar("mqtt", "ERROR: no se pudo crear el cliente MQTT");
        return false;
    }
    esp_mqtt_client_register_event(cliente, MQTT_EVENT_ANY, alEvento, nullptr);
    if (esp_mqtt_client_start(cliente) != ESP_OK) {
        registrar("mqtt", "ERROR: no se pudo iniciar el cliente MQTT");
        esp_mqtt_client_destroy(cliente);
        cliente = nullptr;
        return false;
    }
    registrar("mqtt", "cliente iniciado: %s como %s%s", MQTT_URI_BROKER, MQTT_ID_CLIENTE,
              MQTT_CREDENCIALES_DE_EJEMPLO ? " (credenciales MQTT de ejemplo: completar secrets.h)" : "");
    return true;
}

/** Estado de un sensor para el mensaje: INICIANDO mientras no haya primera lectura válida. */
const char* textoSensor(EstadoSensor estado, bool iniciado) {
    return iniciado ? textoEstado(estado) : "INICIANDO";
}

/** Encola un mensaje; registra los fallos (bandeja de salida llena o sin memoria). */
void encolar(const char* topico, const char* datos, size_t longitud, int qos) {
    if (esp_mqtt_client_enqueue(cliente, topico, datos, static_cast<int>(longitud), qos, 0, true) < 0) {
        fallosEncolado++;
        registrar("mqtt", "ERROR: no se pudo encolar en %s (fallos acumulados: %" PRIu32 ")",
                  topico, fallosEncolado);
    }
}

/** Publica la telemetría del estado actual (QoS 0). */
void publicarTelemetria(const EstadoPublicado& e) {
    const Lecturas&  l = e.instantanea.lecturas;
    const Derivados& d = e.derivados;
    mensajes_mqtt::Telemetria t{};
    t.id                = ++contadorMensajes;
    t.ciclo             = e.instantanea.ciclo;
    t.epocaValida       = redHoraEpoca(t.epoca);
    t.estado            = d.estado;
    t.causas            = d.causas;
    t.alarmaDesactivada = d.alarmaDesactivada;
    t.nivelCm           = d.nivelValido ? d.nivelCm : NAN;
    t.nivelPct          = d.nivelValido ? d.nivelPct : NAN;
    t.tendCmMin         = d.pendienteValida ? d.pendienteCmMin : NAN;
    t.tC                = l.temperaturaDhtC;
    t.hrPct             = l.humedadPct;
    t.pHpa              = l.presionHpa;
    t.uvIdx             = l.uvIndice;
    t.vpdKpa            = d.vpdValido ? d.vpdKpa : NAN;
    t.et0MmDia          = d.et0Disponible ? d.et0MmDia : NAN;
    t.sensorNivel       = textoSensor(l.estadoNivel, l.iniciadoNivel);
    t.sensorDht         = textoSensor(l.estadoDht, l.iniciadoDht);
    t.sensorBmp         = textoSensor(l.estadoBmp, l.iniciadoBmp);
    t.sensorUv          = textoSensor(l.estadoUv, l.iniciadoUv);
    t.rssi              = WiFi.RSSI();

    char json[MQTT_TAM_MENSAJE];
    const size_t n = mensajes_mqtt::telemetria(t, json, sizeof(json));
    if (n == 0) {
        registrar("mqtt", "ERROR: la telemetría no cupo en %u B", static_cast<unsigned>(sizeof(json)));
        return;
    }
    encolar(topicoTelemetria, json, n, MQTT_QOS_TELEMETRIA);
}

/** Publica una alerta por cambio de estado (QoS 1). */
void publicarAlerta(const EstadoPublicado& e, EstadoAlerta anterior) {
    const Derivados& d = e.derivados;
    mensajes_mqtt::Alerta a{};
    a.id          = ++contadorMensajes;
    a.ciclo       = e.instantanea.ciclo;
    a.epocaValida = redHoraEpoca(a.epoca);
    a.anterior    = anterior;
    a.estado      = d.estado;
    a.causas      = d.causas;
    a.nivelPct    = d.nivelValido ? d.nivelPct : NAN;
    a.tendCmMin   = d.pendienteValida ? d.pendienteCmMin : NAN;

    char json[MQTT_TAM_MENSAJE];
    const size_t n = mensajes_mqtt::alerta(a, json, sizeof(json));
    if (n == 0) {
        registrar("mqtt", "ERROR: la alerta no cupo en %u B", static_cast<unsigned>(sizeof(json)));
        return;
    }
    encolar(topicoAlerta, json, n, MQTT_QOS_ALERTA);
    registrar("mqtt", "alerta %s -> %s encolada (id %" PRIu32 "%s)", textoAlerta(anterior),
              textoAlerta(d.estado), a.id, conectado.load() ? "" : ", se envía al reconectar");
}

/** Publica la respuesta a una orden (QoS 1) y la registra. */
void responderOrden(const char* id, bool aceptada, const char* motivo, EstadoAlerta estado) {
    mensajes_mqtt::Respuesta r{};
    r.ordenId     = id;
    r.aceptada    = aceptada;
    r.motivo      = motivo;
    r.estado      = estado;
    r.epocaValida = redHoraEpoca(r.epoca);
    char json[MQTT_TAM_MENSAJE];
    const size_t n = mensajes_mqtt::respuesta(r, json, sizeof(json));
    if (n > 0) {
        encolar(topicoRespuesta, json, n, MQTT_QOS_ORDEN);
    }
    registrar("mqtt", "orden %s %s%s%s", id, aceptada ? "aceptada" : "rechazada",
              motivo != nullptr ? ": " : "", motivo != nullptr ? motivo : "");
}

/**
 * @brief Atiende las órdenes recibidas con las reglas del tablero local.
 *
 * En un estado distinto de ALERTA o CRÍTICO la orden se rechaza de inmediato.
 * En ALERTA o CRÍTICO se pide la desactivación a tFusion y la respuesta espera
 * a que el estado publicado la confirme (alarma_desactivada), o se rechaza si
 * no se confirma en MQTT_ORDEN_ESPERA_MS (por ejemplo, porque el estado cambió).
 */
void atenderOrdenes(const EstadoPublicado& e) {
    const Derivados& d = e.derivados;
    static char motivo[48];

    if (ordenPendiente) {
        if (d.alarmaDesactivada) {
            ordenPendiente = false;
            responderOrden(ordenPendienteId, true, nullptr, d.estado);
        } else if (millis() - ordenPendienteDesdeMs >= MQTT_ORDEN_ESPERA_MS) {
            ordenPendiente = false;
            snprintf(motivo, sizeof(motivo), "sin confirmar; estado %s", textoAlerta(d.estado));
            responderOrden(ordenPendienteId, false, motivo, d.estado);
        }
        return;  // una orden a la vez; las demás esperan en la cola
    }

    OrdenRecibida orden;
    if (qOrdenes == nullptr || xQueueReceive(qOrdenes, &orden, 0) != pdTRUE) {
        return;
    }
    if (d.estado != EstadoAlerta::ALERTA && d.estado != EstadoAlerta::CRITICO) {
        snprintf(motivo, sizeof(motivo), "estado %s", textoAlerta(d.estado));
        responderOrden(orden.id, false, motivo, d.estado);
        return;
    }
    if (d.alarmaDesactivada) {
        responderOrden(orden.id, true, nullptr, d.estado);  // ya estaba desactivada
        return;
    }
    tareasSolicitarDesactivarAlarma();
    ordenPendiente = true;
    ordenPendienteDesdeMs = millis();
    strncpy(ordenPendienteId, orden.id, sizeof(ordenPendienteId) - 1);
    ordenPendienteId[sizeof(ordenPendienteId) - 1] = '\0';
}

}  // namespace

void mqttCiclo(bool wifiConectado, const EstadoPublicado* estado) {
    // El cliente se crea con la primera conexión Wi-Fi; desde ahí reconecta solo.
    if (cliente == nullptr) {
        if (!wifiConectado || !iniciarCliente()) {
            return;
        }
    }
    if (estado == nullptr) {
        return;
    }

    // Alerta en cada cambio de estado, aunque el broker no esté disponible: el
    // cliente la guarda en su bandeja de salida (QoS 1) y la envía al reconectar.
    const EstadoAlerta actual = estado->derivados.estado;
    if (hayEstadoPrevio && actual != estadoPrevio) {
        publicarAlerta(*estado, estadoPrevio);
    }
    estadoPrevio = actual;
    hayEstadoPrevio = true;

    atenderOrdenes(*estado);

    // Telemetría solo con la sesión abierta: con QoS 0 no tiene sentido acumularla.
    const uint32_t ahora = millis();
    if (conectado.load() && ahora - ultimaTelemetriaMs >= MQTT_TELEMETRIA_PERIODO_MS) {
        ultimaTelemetriaMs = ahora;
        publicarTelemetria(*estado);
    }
}

bool mqttConectado() {
    return conectado.load();
}
