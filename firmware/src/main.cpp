/**
 * @file main.cpp
 * @brief Punto de entrada del firmware — IoT Challenge #2, Equipo 7.
 *
 * Nodo de monitoreo del nivel de agua y de variables meteorológicas para un
 * punto de almacenamiento en Sabana Centro (ESP32-DevKitC V4).
 *
 * Paso 5 (red y servidor): a la adquisición (paso 2), al nivel, la tendencia,
 * el VPD y la ET0 (paso 3) y a la fusión con la LCD y el buzzer (paso 4) se
 * suman el Wi-Fi en modo estación con reconexión, el servidor web del tablero
 * con control de acceso (subred, token y Digest), el histórico y el
 * WebSocket. Criterio de "hecho": el tablero muestra datos en vivo desde un
 * celular, rechaza a un usuario sin credenciales y la desactivación de la
 * alarma física funciona (PLAN_DE_TRABAJO.md, "Orden de implementación").
 */
#include <Arduino.h>
#include <esp_arduino_version.h>

#include "config.h"
#include "registro.h"
#include "tareas.h"

void setup() {
    Serial.begin(SERIAL_BAUDIOS);
    registroIniciar();

    registrar("main", "Firmware IoT Challenge #2 — paso 5 (red y tablero web)%s",
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
