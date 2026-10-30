/**
 * @file mqtt.cpp
 * @brief Comunicación MQTT con el gateway: conexión, última voluntad y estado
 *        del nodo (Challenge #3).
 */
#include "mqtt.h"

#include <Arduino.h>
#include <atomic>
#include <cstdio>
#include <mqtt_client.h>

#include "config.h"
#include "credenciales.h"
#include "registro.h"

namespace {

esp_mqtt_client_handle_t cliente = nullptr;
std::atomic<bool> conectado{false};
std::atomic<uint32_t> conexiones{0};
std::atomic<uint32_t> desconectadoDesdeMs{0};  ///< 0 si nunca se ha desconectado.

char topicoEstado[64];

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

}  // namespace

void mqttCiclo(bool wifiConectado) {
    // El cliente se crea con la primera conexión Wi-Fi; desde ahí reconecta solo.
    if (cliente == nullptr && wifiConectado) {
        iniciarCliente();
    }
}

bool mqttConectado() {
    return conectado.load();
}
