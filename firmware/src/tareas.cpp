/**
 * @file tareas.cpp
 * @brief Tareas FreeRTOS, temporizador de adquisición y sincronización.
 *
 * Reglas de concurrencia (Guía de firmware):
 *  - La ISR del temporizador SOLO notifica a tSensores. El I2C, el DHT22 y la
 *    medición del eco nunca se ejecutan dentro de una interrupción.
 *  - qSnapshot es una cola de longitud 1 escrita con xQueueOverwrite: tFusion
 *    siempre recibe la instantánea más reciente.
 *  - mtxDatos protege el estado publicado; quien lo lee copia la estructura y
 *    libera el mutex enseguida.
 *  - mtxI2C protegerá el bus I2C compartido por el BMP180 y la LCD (se usa a
 *    partir del paso 2).
 */
#include "tareas.h"

#include <Arduino.h>
#include <cstdlib>
#include <esp_arduino_version.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <inttypes.h>

#include "config.h"
#include "registro.h"

namespace {

// ---------------------------------------------------------------------------
// Recursos compartidos
// ---------------------------------------------------------------------------

TaskHandle_t hSensores  = nullptr;
TaskHandle_t hFusion    = nullptr;
TaskHandle_t hHmi       = nullptr;
TaskHandle_t hHistorico = nullptr;
TaskHandle_t hRed       = nullptr;

QueueHandle_t     qSnapshot = nullptr;  ///< tSensores → tFusion (longitud 1).
SemaphoreHandle_t mtxDatos  = nullptr;  ///< Protege estadoPublicado.
SemaphoreHandle_t mtxI2C    = nullptr;  ///< Protegerá el bus I2C (paso 2).

hw_timer_t* temporizador = nullptr;

Snapshot estadoPublicado{};    ///< Último estado publicado por tFusion.
bool     hayEstadoPublicado = false;

// ---------------------------------------------------------------------------
// Rutina de servicio de interrupción del temporizador
// ---------------------------------------------------------------------------

/**
 * @brief ISR del temporizador de 1 Hz: despierta a tSensores.
 *
 * Se mantiene lo más corta posible. Si tSensores tiene mayor prioridad que la
 * tarea interrumpida, se solicita el cambio de contexto al salir de la ISR.
 */
void IRAM_ATTR isrTemporizador() {
    BaseType_t despertoTareaPrioritaria = pdFALSE;
    if (hSensores != nullptr) {
        vTaskNotifyGiveFromISR(hSensores, &despertoTareaPrioritaria);
    }
    if (despertoTareaPrioritaria == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

// ---------------------------------------------------------------------------
// Tareas
// ---------------------------------------------------------------------------

/**
 * @brief Tarea de adquisición, despertada por la ISR.
 *
 * Paso 1: mide el periodo real entre ciclos, cuenta los ciclos fuera de
 * tolerancia y las notificaciones acumuladas, y entrega la instantánea.
 */
void tareaSensores(void*) {
    uint32_t ciclo = 0;
    uint32_t ciclosFueraDeTolerancia = 0;
    int64_t  marcaAnteriorUs = 0;
    const int64_t periodoEsperadoUs = 1000000LL / TEMPORIZADOR_FRECUENCIA_HZ;

    for (;;) {
        // Espera la notificación de la ISR. El valor devuelto es el número de
        // notificaciones pendientes: más de 1 indica ciclos no atendidos.
        const uint32_t pendientes = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const int64_t ahoraUs = esp_timer_get_time();
        ciclo++;

        Snapshot instantanea{};
        instantanea.ciclo = ciclo;
        instantanea.marcaTiempoUs = ahoraUs;
        instantanea.ciclosPerdidos = (pendientes > 1) ? (pendientes - 1) : 0;
        instantanea.cicloDht22 = (ciclo % DHT22_CADA_N_CICLOS) == 0;

        // Paso 2: aquí se leen HC-SR04 (mediana de 5 ecos), BMP180 y GUVA en
        // cada ciclo, y el DHT22 cuando instantanea.cicloDht22 es true.

        xQueueOverwrite(qSnapshot, &instantanea);

        if (marcaAnteriorUs == 0) {
            registrar("tSensores", "ciclo %" PRIu32 " (primer ciclo)", ciclo);
        } else {
            const int64_t periodoUs = ahoraUs - marcaAnteriorUs;
            const int64_t desviacionUs = periodoUs - periodoEsperadoUs;
            if (llabs(desviacionUs) > TOLERANCIA_PERIODO_US) {
                ciclosFueraDeTolerancia++;
            }
            registrar("tSensores",
                      "ciclo %" PRIu32 " periodo=%.3f ms desv=%+.3f ms "
                      "perdidos=%" PRIu32 " fuera_tol=%" PRIu32 "%s",
                      ciclo, periodoUs / 1000.0, desviacionUs / 1000.0,
                      instantanea.ciclosPerdidos, ciclosFueraDeTolerancia,
                      instantanea.cicloDht22 ? " [DHT22]" : "");
        }
        marcaAnteriorUs = ahoraUs;
    }
}

/**
 * @brief Tarea de fusión: recibe cada instantánea y publica el estado.
 *
 * Paso 1: solo copia la instantánea como estado publicado. En el paso 4 se
 * agregan la tendencia, el VPD, la ET0 y la clasificación del estado de alerta.
 */
void tareaFusion(void*) {
    Snapshot recibida{};
    for (;;) {
        if (xQueueReceive(qSnapshot, &recibida, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const int64_t latenciaUs = esp_timer_get_time() - recibida.marcaTiempoUs;

        xSemaphoreTake(mtxDatos, portMAX_DELAY);
        estadoPublicado = recibida;
        hayEstadoPublicado = true;
        xSemaphoreGive(mtxDatos);

        registrar("tFusion", "estado publicado del ciclo %" PRIu32
                  " (latencia desde la adquisición: %.3f ms)",
                  recibida.ciclo, latenciaUs / 1000.0);
    }
}

/**
 * @brief Tarea de interfaz local (LCD y buzzer), periodo de 250 ms.
 *
 * Paso 1: lee el estado publicado y registra una línea por segundo.
 */
void tareaHmi(void*) {
    TickType_t ultimoDespertar = xTaskGetTickCount();
    const TickType_t periodo = pdMS_TO_TICKS(HMI_PERIODO_MS);
    const uint32_t iteracionesPorSegundo = 1000 / HMI_PERIODO_MS;
    uint32_t iteracion = 0;
    Snapshot copia{};

    for (;;) {
        vTaskDelayUntil(&ultimoDespertar, periodo);
        iteracion++;
        const bool hayDato = leerEstadoPublicado(copia);
        if (iteracion % iteracionesPorSegundo == 0) {
            if (hayDato) {
                registrar("tHMI", "mostraría el ciclo %" PRIu32, copia.ciclo);
            } else {
                registrar("tHMI", "sin estado publicado todavía");
            }
        }
    }
}

/**
 * @brief Tarea del histórico: registro rápido cada 5 s y lento cada 5 min.
 *
 * Paso 1: registra en el puerto serie el ciclo que guardaría en cada búfer.
 */
void tareaHistorico(void*) {
    TickType_t ultimoDespertar = xTaskGetTickCount();
    const TickType_t periodo = pdMS_TO_TICKS(HISTORICO_RAPIDO_PERIODO_MS);
    const uint32_t rapidosPorLento =
        HISTORICO_LENTO_PERIODO_MS / HISTORICO_RAPIDO_PERIODO_MS;
    uint32_t iteracion = 0;
    Snapshot copia{};

    for (;;) {
        vTaskDelayUntil(&ultimoDespertar, periodo);
        iteracion++;
        if (!leerEstadoPublicado(copia)) {
            continue;
        }
        registrar("tHistorico", "registro rápido del ciclo %" PRIu32, copia.ciclo);
        if (iteracion % rapidosPorLento == 0) {
            registrar("tHistorico", "registro lento del ciclo %" PRIu32, copia.ciclo);
        }
    }
}

/**
 * @brief Tarea de red: supervisa el Wi-Fi cada 5 s.
 *
 * Paso 1: solo informa la memoria libre, útil para detectar fugas durante la
 * prueba de 10 minutos. La conexión en modo estación se agrega en el paso 5.
 */
void tareaRed(void*) {
    TickType_t ultimoDespertar = xTaskGetTickCount();
    const TickType_t periodo = pdMS_TO_TICKS(RED_PERIODO_MS);

    for (;;) {
        vTaskDelayUntil(&ultimoDespertar, periodo);
        registrar("tRed", "Wi-Fi sin configurar (paso 5). Heap libre: %" PRIu32
                  " B, mínimo histórico: %" PRIu32 " B",
                  static_cast<uint32_t>(ESP.getFreeHeap()),
                  static_cast<uint32_t>(ESP.getMinFreeHeap()));
    }
}

/**
 * @brief Crea una tarea fijada a un núcleo y registra el resultado.
 */
bool crearTarea(TaskFunction_t funcion, const char* nombre, uint32_t pila,
                UBaseType_t prioridad, TaskHandle_t* manejador, BaseType_t nucleo) {
    const BaseType_t resultado = xTaskCreatePinnedToCore(
        funcion, nombre, pila, nullptr, prioridad, manejador, nucleo);
    if (resultado != pdPASS) {
        registrar("tareas", "ERROR: no se pudo crear %s", nombre);
        return false;
    }
    registrar("tareas", "%s creada (núcleo %d, prioridad %u, pila %" PRIu32 " B)",
              nombre, static_cast<int>(nucleo), static_cast<unsigned>(prioridad), pila);
    return true;
}

/**
 * @brief Configura el temporizador de hardware para interrumpir a 1 Hz.
 *
 * La API del temporizador cambió entre las versiones 2.x y 3.x del núcleo
 * Arduino para ESP32; se compila la variante que corresponda.
 */
bool iniciarTemporizador() {
    const uint64_t cuentasPorPeriodo =
        TEMPORIZADOR_BASE_HZ / TEMPORIZADOR_FRECUENCIA_HZ;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    temporizador = timerBegin(TEMPORIZADOR_BASE_HZ);
    if (temporizador == nullptr) {
        return false;
    }
    timerAttachInterrupt(temporizador, &isrTemporizador);
    timerAlarm(temporizador, cuentasPorPeriodo, true, 0);
#else
    // Reloj APB de 80 MHz dividido entre 80 → 1 MHz (1 cuenta = 1 µs).
    constexpr uint16_t DIVISOR_APB = 80;
    temporizador = timerBegin(0, DIVISOR_APB, true);
    if (temporizador == nullptr) {
        return false;
    }
    timerAttachInterrupt(temporizador, &isrTemporizador, true);
    timerAlarmWrite(temporizador, cuentasPorPeriodo, true);
    timerAlarmEnable(temporizador);
#endif
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// Interfaz pública
// ---------------------------------------------------------------------------

bool leerEstadoPublicado(Snapshot& destino) {
    if (mtxDatos == nullptr) {
        return false;
    }
    xSemaphoreTake(mtxDatos, portMAX_DELAY);
    const bool hay = hayEstadoPublicado;
    if (hay) {
        destino = estadoPublicado;
    }
    xSemaphoreGive(mtxDatos);
    return hay;
}

bool tareasIniciar() {
    qSnapshot = xQueueCreate(1, sizeof(Snapshot));
    mtxDatos = xSemaphoreCreateMutex();
    mtxI2C = xSemaphoreCreateMutex();
    if (qSnapshot == nullptr || mtxDatos == nullptr || mtxI2C == nullptr) {
        registrar("tareas", "ERROR: no se pudieron crear la cola o los mutex");
        return false;
    }

    // Las tareas se crean antes del temporizador para que la ISR encuentre
    // listo el manejador de tSensores.
    bool ok = true;
    ok &= crearTarea(tareaSensores, "tSensores", PILA_SENSORES,
                     PRIORIDAD_SENSORES, &hSensores, NUCLEO_APLICACION);
    ok &= crearTarea(tareaFusion, "tFusion", PILA_FUSION,
                     PRIORIDAD_FUSION, &hFusion, NUCLEO_APLICACION);
    ok &= crearTarea(tareaHmi, "tHMI", PILA_HMI,
                     PRIORIDAD_HMI, &hHmi, NUCLEO_APLICACION);
    ok &= crearTarea(tareaHistorico, "tHistorico", PILA_HISTORICO,
                     PRIORIDAD_HISTORICO, &hHistorico, NUCLEO_RED);
    ok &= crearTarea(tareaRed, "tRed", PILA_RED,
                     PRIORIDAD_RED, &hRed, NUCLEO_RED);
    if (!ok) {
        return false;
    }

    if (!iniciarTemporizador()) {
        registrar("tareas", "ERROR: no se pudo iniciar el temporizador");
        return false;
    }
    registrar("tareas", "temporizador de adquisición iniciado a %" PRIu32 " Hz",
              TEMPORIZADOR_FRECUENCIA_HZ);
    return true;
}
