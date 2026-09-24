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
 * @brief Instantánea de un ciclo de adquisición.
 *
 * Paso 1 (esqueleto): solo contiene datos de temporización para verificar
 * que el temporizador despierta la adquisición una vez por segundo.
 * En el paso 2 se agregan las lecturas de los sensores y el estado de cada
 * sensor (OK / VIEJO / FALLA).
 */
struct Snapshot {
    uint32_t ciclo;           ///< Número de ciclo desde el arranque (empieza en 1).
    int64_t  marcaTiempoUs;   ///< Instante del ciclo, en µs desde el arranque.
    uint32_t ciclosPerdidos;  ///< Notificaciones acumuladas que no se atendieron a tiempo.
    bool     cicloDht22;      ///< true si en este ciclo corresponde leer el DHT22.
};
