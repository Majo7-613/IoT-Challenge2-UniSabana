/**
 * @file registro.cpp
 * @brief Implementación del registro serie con marca de tiempo.
 */
#include "registro.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {

/** Longitud máxima del mensaje de una línea (sin la marca de tiempo). */
constexpr size_t LONGITUD_MENSAJE = 160;

/** Mutex que serializa el acceso al puerto serie. */
SemaphoreHandle_t mtxSerial = nullptr;

}  // namespace

void registroIniciar() {
    if (mtxSerial == nullptr) {
        mtxSerial = xSemaphoreCreateMutex();
    }
}

void registrar(const char* etiqueta, const char* formato, ...) {
    // Se formatea antes de tomar el mutex para ocuparlo el menor tiempo posible.
    char mensaje[LONGITUD_MENSAJE];
    va_list argumentos;
    va_start(argumentos, formato);
    vsnprintf(mensaje, sizeof(mensaje), formato, argumentos);
    va_end(argumentos);

    const double marcaMs = esp_timer_get_time() / 1000.0;

    if (mtxSerial != nullptr) {
        xSemaphoreTake(mtxSerial, portMAX_DELAY);
    }
    Serial.printf("[%12.3f ms] [%-10s] %s\n", marcaMs, etiqueta, mensaje);
    if (mtxSerial != nullptr) {
        xSemaphoreGive(mtxSerial);
    }
}
