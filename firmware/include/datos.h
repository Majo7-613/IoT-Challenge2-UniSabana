/**
 * @file datos.h
 * @brief Estructuras de datos compartidas entre tareas.
 *
 * La tarea de adquisición produce una "instantánea" (snapshot) por ciclo y la
 * entrega a la tarea de fusión por una cola de longitud 1. Las demás tareas
 * (interfaz local, histórico, red) leen una copia del último estado publicado,
 * protegida por exclusión mutua.
 */
#pragma once

#include <Arduino.h>

/**
 * @brief Estado de un sensor, independiente del estado de alerta.
 *
 *  - OK:    la última lectura válida tiene como máximo SENSOR_CICLOS_PARA_VIEJO ciclos.
 *  - VIEJO: no hay lectura válida en más de SENSOR_CICLOS_PARA_VIEJO ciclos
 *           (por ejemplo, no se pudo intentar la lectura).
 *  - FALLA: el último intento dio NaN, un valor fuera de rango o se agotó el
 *           tiempo de espera del eco.
 */
enum class EstadoSensor : uint8_t { OK, VIEJO, FALLA };

/** Origen de la temperatura usada para compensar la velocidad del sonido. */
enum class FuenteCompensacion : uint8_t { DHT22, BMP180, POR_DEFECTO };

/** Texto corto de un estado de sensor, para el registro y la interfaz. */
const char* textoEstado(EstadoSensor estado);

/** Texto corto de la fuente de compensación térmica. */
const char* textoCompensacion(FuenteCompensacion fuente);

/**
 * @brief Lecturas de los sensores de un ciclo.
 *
 * Cada valor conserva la última lectura válida (NAN si nunca la hubo); el
 * campo de estado indica si ese valor es actual (OK), antiguo (VIEJO) o si el
 * último intento falló (FALLA).
 */
struct Lecturas {
    // HC-SR04: distancia del sensor a la superficie del agua.
    float              distanciaCm;    ///< Mediana de los ecos válidos, compensada por temperatura.
    uint8_t            ecosValidos;    ///< Ecos válidos del ciclo (de HCSR04_ECOS_POR_CICLO).
    FuenteCompensacion compensacion;   ///< Temperatura usada para la velocidad del sonido.
    EstadoSensor       estadoNivel;

    // DHT22: temperatura y humedad relativa (cada DHT22_CADA_N_CICLOS ciclos).
    float        temperaturaDhtC;
    float        humedadPct;
    EstadoSensor estadoDht;

    // BMP180: presión (se muestra y se guarda; no entra a la decisión) y temperatura.
    float        presionHpa;
    float        temperaturaBmpC;
    EstadoSensor estadoBmp;

    // GUVA-S12SD: tensión de salida y conversión inicial a índice UV.
    float        uvMilivoltios;
    float        uvIndice;             ///< Aproximación Vout [V] × 10, a caracterizar en la Wiki (4.2).
    EstadoSensor estadoUv;
};

/**
 * @brief Instantánea de un ciclo de adquisición.
 */
struct Snapshot {
    uint32_t ciclo;           ///< Número de ciclo desde el arranque (empieza en 1).
    int64_t  marcaTiempoUs;   ///< Instante del ciclo, en µs desde el arranque.
    uint32_t ciclosPerdidos;  ///< Notificaciones acumuladas que no se atendieron a tiempo.
    bool     cicloDht22;      ///< true si en este ciclo se intentó leer el DHT22.
    Lecturas lecturas;        ///< Lecturas de los sensores del ciclo.
};
