/**
 * @file mqtt.h
 * @brief Comunicación MQTT con el gateway (Challenge #3).
 *
 * Usa el cliente MQTT de ESP-IDF (mqtt_client.h), que corre en su propia
 * tarea y se reconecta solo. Solo lo llama la tarea tMqtt: si el broker no
 * responde, la medición, la fusión, la alarma in situ y el tablero local
 * siguen funcionando.
 *
 * Tópicos y mensajes: docs/diseno/comunicacion-mqtt.md.
 *  - sabana/{municipio}/{nodo}/estado: "online" retenido al conectarse;
 *    "offline" retenido como última voluntad, que publica el broker si la
 *    conexión se cae sin cerrarse.
 *  - .../telemetria: cada MQTT_TELEMETRIA_PERIODO_MS, con QoS 0.
 *  - .../alerta: en cada cambio del estado de alerta, con QoS 1. Si el broker
 *    no está disponible, queda en la bandeja de salida del cliente y se envía
 *    al reconectar.
 *  - .../cmd/desactivar_alarma (suscripción, QoS 1): orden de desactivar la
 *    alarma física. Se acepta solo en ALERTA o CRÍTICO, con las mismas reglas
 *    que el tablero local, y se responde en .../cmd/respuesta (QoS 1).
 */
#pragma once

#include <cstdint>

#include "datos.h"

/**
 * @brief Atiende la conexión con el broker (llamar cada MQTT_TICK_MS).
 *
 * Crea e inicia el cliente la primera vez que hay Wi-Fi; desde ahí, la
 * reconexión la hace el cliente de ESP-IDF.
 *
 * También publica la telemetría y las alertas a partir del último estado
 * publicado por tFusion.
 *
 * @param wifiConectado true si el ESP32 tiene dirección IP en la WLAN.
 * @param estado Último estado publicado, o nullptr si todavía no hay uno.
 */
void mqttCiclo(bool wifiConectado, const EstadoPublicado* estado);

/** true si la sesión con el broker está abierta. */
bool mqttConectado();
