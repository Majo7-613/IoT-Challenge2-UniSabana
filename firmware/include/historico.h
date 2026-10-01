/**
 * @file historico.h
 * @brief Histórico reciente en RAM (RF-15): búfer rápido de 120 registros cada
 *        5 s (10 min) y búfer lento de 288 registros cada 5 min (24 h).
 *
 * tHistorico agrega los registros; el servidor web los copia para el tablero.
 * Los búferes se protegen con un mutex propio y se pierden al reiniciar.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "datos.h"

/** Un registro del histórico. Un valor cuyo sensor no está en OK se guarda como NAN. */
struct RegistroHistorico {
    uint32_t     tiempoS;      ///< Segundos desde el arranque.
    float        nivelCm;
    float        nivelPct;
    float        temperaturaC;
    float        humedadPct;
    float        presionHpa;
    float        uvIndice;
    float        vpdKpa;
    EstadoAlerta estado;
};

enum class Bufer : uint8_t { RAPIDO, LENTO };

/** Crea el mutex de los búferes. Llamar una vez antes de crear las tareas. */
bool historicoIniciar();

/** Agrega un registro al búfer indicado a partir del estado publicado. */
void historicoAgregar(Bufer bufer, const EstadoPublicado& estado);

/**
 * @brief Copia los registros del búfer, del más antiguo al más reciente.
 * @param destino Arreglo de al menos historicoCapacidad(bufer) registros.
 * @return Número de registros copiados.
 */
size_t historicoCopiar(Bufer bufer, RegistroHistorico* destino);

/** Capacidad del búfer, en registros. */
size_t historicoCapacidad(Bufer bufer);

/** Periodo entre registros del búfer, en segundos. */
uint32_t historicoPeriodoS(Bufer bufer);
