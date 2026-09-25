/**
 * @file estados.h
 * @brief Estados de alerta del sistema y causas de la clasificación.
 *
 * Archivo sin dependencias de Arduino, para poder compilar la lógica de
 * fusión y sus pruebas también en un computador.
 */
#pragma once

#include <cstdint>

/**
 * @brief Estado de alerta publicado por la lógica de fusión.
 *
 * NORMAL, ADVERTENCIA, ALERTA y CRÍTICO son los cuatro estados de alerta.
 * INICIANDO y FALLA_NIVEL no son estados de alerta: indican que no se puede
 * evaluar el riesgo porque el sensor de nivel aún no tiene su primera lectura
 * válida o porque falló. En ambos casos nunca se muestra NORMAL.
 */
enum class EstadoAlerta : uint8_t {
    INICIANDO,
    NORMAL,
    ADVERTENCIA,
    ALERTA,
    CRITICO,
    FALLA_NIVEL
};

/** Causas que activaron la clasificación (se combinan con OR). */
enum CausaAlerta : uint8_t {
    CAUSA_NINGUNA          = 0,
    CAUSA_NIVEL_CRITICO    = 1 << 0,
    CAUSA_NIVEL_PREVENTIVO = 1 << 1,
    CAUSA_DESCENSO         = 1 << 2,
    CAUSA_VPD_ALTO         = 1 << 3,
    CAUSA_T_ALTA           = 1 << 4,
    CAUSA_UV_ALTO          = 1 << 5
};

/** Texto corto del estado de alerta, sin tildes (juego de caracteres de la LCD). */
const char* textoAlerta(EstadoAlerta estado);
