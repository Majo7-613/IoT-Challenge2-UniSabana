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
 */
#pragma once

#include <cstdint>

/**
 * @brief Atiende la conexión con el broker (llamar cada MQTT_TICK_MS).
 *
 * Crea e inicia el cliente la primera vez que hay Wi-Fi; desde ahí, la
 * reconexión la hace el cliente de ESP-IDF.
 *
 * @param wifiConectado true si el ESP32 tiene dirección IP en la WLAN.
 */
void mqttCiclo(bool wifiConectado);

/** true si la sesión con el broker está abierta. */
bool mqttConectado();
