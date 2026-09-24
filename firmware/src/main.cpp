/**
 * @file main.cpp
 * @brief Punto de entrada del firmware — IoT Challenge #2, Equipo 7.
 *
 * Nodo de monitoreo del nivel de agua y de variables meteorológicas para un
 * punto de almacenamiento en Sabana Centro (ESP32-DevKitC V4).
 *
 * Paso 2 (sensores): el temporizador de 1 Hz despierta la adquisición, que lee
 * el HC-SR04, el DHT22, el BMP180 y el GUVA-S12SD y asigna a cada sensor un
 * estado OK, VIEJO o FALLA. Criterio de "hecho": cada lectura tiene su estado
 * y el sistema sobrevive a desconectar un sensor (PLAN_DE_TRABAJO.md,
 * "Orden de implementación").
 */
#include <Arduino.h>
#include <esp_arduino_version.h>

#include "config.h"
#include "registro.h"
#include "tareas.h"

void setup() {
    Serial.begin(SERIAL_BAUDIOS);
    registroIniciar();

    registrar("main", "Firmware IoT Challenge #2 — paso 2 (sensores)%s",
#ifdef SIMULACION
              " [SIMULACIÓN]"
#else
              ""
#endif
    );
    registrar("main", "Núcleo Arduino ESP32 %d.%d.%d, ESP-IDF %s",
              ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR,
              ESP_ARDUINO_VERSION_PATCH, ESP.getSdkVersion());
    registrar("main", "Modo de parámetros: %s",
              MODO_DEMOSTRACION ? "DEMOSTRACIÓN" : "CAMPO");

    if (!tareasIniciar()) {
        registrar("main", "ERROR: fallo al iniciar las tareas; revisar el registro");
    }
}

void loop() {
    // Todo el trabajo lo hacen las tareas FreeRTOS; la tarea del bucle de
    // Arduino se elimina para liberar su memoria.
    vTaskDelete(nullptr);
}
