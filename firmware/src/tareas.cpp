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
 *  - mtxI2C protege el bus I2C compartido por el BMP180 y la LCD.
 */
#include "tareas.h"

#include <Arduino.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <esp_arduino_version.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <inttypes.h>

#include "config.h"
#include "evaporacion.h"
#include "fusion.h"
#include "hmi.h"
#include "nivel.h"
#include "registro.h"
#include "sensores.h"

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
SemaphoreHandle_t mtxI2C    = nullptr;  ///< Protege el bus I2C (BMP180 y LCD).

hw_timer_t* temporizador = nullptr;

EstadoPublicado estadoPublicado{};  ///< Último estado publicado por tFusion.
bool            hayEstadoPublicado = false;

// Estado propio de tFusion (se declaran globales por su tamaño).
nivel::Tendencia             tendencia(param::TENDENCIA_VENTANA_S);
evaporacion::ExtremosDiarios extremosTemperatura;
volatile int                 diaDelAnio = 0;  ///< 0: todavía desconocido.

/** Parámetros de la clasificación a partir de los valores activos de config.h. */
fusion::Parametros parametrosFusion() {
    fusion::Parametros p{};
    p.nivelCriticoPct = param::NIVEL_CRITICO_FRACCION * 100.0f;
    p.nivelPreventivoPct = param::NIVEL_PREVENTIVO_FRACCION * 100.0f;
    p.nivelHisteresisPct = param::NIVEL_HISTERESIS_FRACCION * 100.0f;
    p.descensoEntradaCmMin = param::DESCENSO_PENDIENTE_CM_MIN;
    p.descensoSalidaCmMin = param::DESCENSO_SALIDA_CM_MIN;
    p.descensoSostenidoS = static_cast<float>(param::DESCENSO_SOSTENIDO_S);
    p.temperaturaAltaC = param::T_ALTA_C;
    p.temperaturaHisteresisC = param::T_HISTERESIS_C;
    p.vpdAltoKpa = param::VPD_ALTO_KPA;
    p.vpdHisteresisKpa = param::VPD_HISTERESIS_KPA;
    p.uvAlto = UV_INDICE_ALTO;
    p.uvHisteresis = UV_HISTERESIS_INDICE;
    return p;
}

fusion::Clasificador clasificador(parametrosFusion());

/** Orden de desactivar la alarma física, pendiente de atender por tFusion. */
std::atomic<bool> solicitudDesactivar{false};

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

/** Registra en una línea las lecturas y el estado de cada sensor. */
void registrarLecturas(const Lecturas& l) {
    registrar("tSensores",
              "d=%.1f cm (%s, %u/%u ecos, %s) | T=%.1f C HR=%.1f %% (%s) | "
              "P=%.1f hPa T=%.1f C (%s) | UV=%.0f mV ~%.1f (%s)",
              l.distanciaCm, textoEstado(l.estadoNivel),
              static_cast<unsigned>(l.ecosValidos), static_cast<unsigned>(HCSR04_ECOS_POR_CICLO),
              textoCompensacion(l.compensacion),
              l.temperaturaDhtC, l.humedadPct, textoEstado(l.estadoDht),
              l.presionHpa, l.temperaturaBmpC, textoEstado(l.estadoBmp),
              l.uvMilivoltios, l.uvIndice, textoEstado(l.estadoUv));
}

/**
 * @brief Tarea de adquisición, despertada por la ISR.
 *
 * Mide el periodo real entre ciclos, cuenta los ciclos fuera de tolerancia y
 * las notificaciones acumuladas, lee los sensores y entrega la instantánea.
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

        // HC-SR04 (mediana de 5 ecos), BMP180 y GUVA en cada ciclo; DHT22
        // cada DHT22_CADA_N_CICLOS ciclos.
        sensoresLeer(instantanea.cicloDht22, instantanea.lecturas);

        xQueueOverwrite(qSnapshot, &instantanea);
        registrarLecturas(instantanea.lecturas);

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
 * @brief Calcula el nivel, la tendencia, el VPD y la ET0 de una instantánea.
 */
Derivados calcularDerivados(const Snapshot& s) {
    static const nivel::Geometria geometria{param::ALTURA_MONTAJE_CM, param::ALTURA_UTIL_CM};
    const Lecturas& l = s.lecturas;
    Derivados d{};

    // Nivel y tendencia: solo con lecturas de nivel en OK.
    d.nivelValido = l.estadoNivel == EstadoSensor::OK;
    d.nivelCm = nivel::nivelCm(l.distanciaCm, geometria);
    d.nivelPct = nivel::nivelFraccion(d.nivelCm, geometria) * 100.0f;
    tendencia.agregar(static_cast<float>(s.marcaTiempoUs / 1e6), d.nivelCm, d.nivelValido);
    d.pendienteValida = tendencia.pendienteCmMin(d.pendienteCmMin);
    if (!d.pendienteValida) {
        d.pendienteCmMin = NAN;
    }

    // VPD: indicador de demanda evaporativa, con temperatura y humedad del DHT22.
    const bool dhtOk = l.estadoDht == EstadoSensor::OK;
    d.vpdKpa = dhtOk ? evaporacion::vpdKpa(l.temperaturaDhtC, l.humedadPct) : NAN;
    d.vpdValido = !std::isnan(d.vpdKpa);

    // ET0: extremos de 24 h de la temperatura del DHT22 y radiación extraterrestre.
    const uint32_t tiempoS = static_cast<uint32_t>(s.marcaTiempoUs / 1000000);
    if (dhtOk && s.cicloDht22) {
        extremosTemperatura.agregar(tiempoS, l.temperaturaDhtC);
    } else {
        extremosTemperatura.avanzar(tiempoS);
    }
    const int dia = diaDelAnio;
    float tMax = NAN, tMin = NAN;
    d.et0Disponible = dia > 0 && extremosTemperatura.extremos(tMax, tMin);
    d.et0MmDia = d.et0Disponible
        ? evaporacion::et0HargreavesMmDia(
              tMax, tMin, evaporacion::radiacionExtraterrestreMJ(SITIO_LATITUD_GRADOS, dia))
        : NAN;
    return d;
}

/**
 * @brief Tarea de fusión: recibe cada instantánea, calcula y publica el estado.
 *
 * Calcula el nivel, la tendencia, el VPD y la ET0 (paso 3) y clasifica el
 * estado de alerta con histéresis (paso 4). Registra cada cambio de estado.
 */
void tareaFusion(void*) {
    Snapshot recibida{};
    EstadoAlerta estadoAnterior = EstadoAlerta::INICIANDO;
    bool desactivadaAnterior = false;
    for (;;) {
        if (xQueueReceive(qSnapshot, &recibida, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        Derivados d = calcularDerivados(recibida);
        const Lecturas& l = recibida.lecturas;

        fusion::Entradas e{};
        e.tiempoS = static_cast<float>(recibida.marcaTiempoUs / 1e6);
        e.nivelIniciado = l.iniciadoNivel;
        e.nivelOk = d.nivelValido;
        e.nivelPct = d.nivelPct;
        e.tendenciaOk = d.pendienteValida;
        e.tendenciaCmMin = d.pendienteCmMin;
        e.temperaturaOk = l.estadoDht == EstadoSensor::OK;
        e.temperaturaC = l.temperaturaDhtC;
        e.vpdOk = d.vpdValido;
        e.vpdKpa = d.vpdKpa;
        e.uvOk = l.estadoUv == EstadoSensor::OK;
        e.uvIndice = l.uvIndice;
        const bool solicitud = solicitudDesactivar.exchange(false);
        const fusion::Resultado r = clasificador.evaluar(e, solicitud);
        d.estado = r.estado;
        d.causas = r.causas;
        d.alarmaDesactivada = r.alarmaDesactivada;
        if (solicitud) {
            registrar("tFusion", "orden de desactivar la alarma física: %s",
                      r.alarmaDesactivada ? "aceptada" : "ignorada (estado sin alarma sonora)");
        }
        if (d.estado != estadoAnterior || d.alarmaDesactivada != desactivadaAnterior) {
            registrar("tFusion", "estado %s -> %s%s (causas 0x%02X)",
                      textoAlerta(estadoAnterior), textoAlerta(d.estado),
                      d.alarmaDesactivada ? " [alarma desactivada]" : "",
                      static_cast<unsigned>(d.causas));
            estadoAnterior = d.estado;
            desactivadaAnterior = d.alarmaDesactivada;
        }
        const int64_t latenciaUs = esp_timer_get_time() - recibida.marcaTiempoUs;

        xSemaphoreTake(mtxDatos, portMAX_DELAY);
        estadoPublicado.instantanea = recibida;
        estadoPublicado.derivados = d;
        hayEstadoPublicado = true;
        xSemaphoreGive(mtxDatos);

        // Una línea cada 10 ciclos para no saturar el registro.
        if (recibida.ciclo % 10 == 0) {
            registrar("tFusion",
                      "ciclo %" PRIu32 ": nivel=%.1f cm (%.0f %%) tendencia=%.2f cm/min "
                      "VPD=%.2f kPa ET0=%s latencia=%.3f ms",
                      recibida.ciclo, d.nivelCm, d.nivelPct, d.pendienteCmMin, d.vpdKpa,
                      d.et0Disponible ? "disponible" : "no disponible (24 h y fecha)",
                      latenciaUs / 1000.0);
        }
    }
}

/**
 * @brief Tarea de interfaz local (LCD y buzzer), periodo de 250 ms.
 *
 * Copia el estado publicado y actualiza la LCD, la retroiluminación y el
 * patrón del buzzer (hmi.cpp). Registra las filas de la LCD cada 10 s.
 */
void tareaHmi(void*) {
    TickType_t ultimoDespertar = xTaskGetTickCount();
    const TickType_t periodo = pdMS_TO_TICKS(HMI_PERIODO_MS);
    const uint32_t iteracionesPorRegistro = 10000 / HMI_PERIODO_MS;
    uint32_t iteracion = 0;
    EstadoPublicado copia{};

    for (;;) {
        vTaskDelayUntil(&ultimoDespertar, periodo);
        iteracion++;
        if (!leerEstadoPublicado(copia)) {
            continue;
        }
        hmiActualizar(copia, millis());
        if (iteracion % iteracionesPorRegistro == 0) {
            char filas[LCD_FILAS][21];
            hmiComponerFilas(copia, filas);
            registrar("tHMI", "LCD |%s|%s|%s|%s|", filas[0], filas[1], filas[2], filas[3]);
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
    EstadoPublicado copia{};

    for (;;) {
        vTaskDelayUntil(&ultimoDespertar, periodo);
        iteracion++;
        if (!leerEstadoPublicado(copia)) {
            continue;
        }
        registrar("tHistorico", "registro rápido del ciclo %" PRIu32, copia.instantanea.ciclo);
        if (iteracion % rapidosPorLento == 0) {
            registrar("tHistorico", "registro lento del ciclo %" PRIu32, copia.instantanea.ciclo);
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

void tareasSolicitarDesactivarAlarma() {
    solicitudDesactivar.store(true);
}

void tareasFijarDiaDelAnio(int dia) {
    if (dia >= 1 && dia <= 366) {
        diaDelAnio = dia;
    }
}

bool leerEstadoPublicado(EstadoPublicado& destino) {
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

    // Los sensores y la interfaz local se inician antes de crear las tareas.
    sensoresIniciar(mtxI2C);
    if (!hmiIniciar(mtxI2C)) {
        registrar("tareas", "ERROR: no se pudo configurar el buzzer");
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
