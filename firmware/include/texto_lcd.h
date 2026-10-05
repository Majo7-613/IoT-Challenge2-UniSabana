/**
 * @file texto_lcd.h
 * @brief Formato de la fila de nivel de la LCD 20x4, sin dependencias del ESP32.
 *
 * Está separado de hmi.cpp para poder probarlo en el computador
 * (`pio test -d firmware -e native`).
 */
#pragma once

#include <cstddef>

namespace texto_lcd {

/** Ancho, en caracteres, del campo de la tendencia en la fila de nivel. */
constexpr int ANCHO_TENDENCIA = 6;

/**
 * @brief Tendencia en cm/min con a lo sumo ANCHO_TENDENCIA caracteres.
 *
 * Usa 2 decimales mientras el número quepa (hasta ±99.99); con valores más
 * grandes reduce los decimales, en lugar de cortar el texto.
 */
void tendencia(char* salida, size_t tamano, float cmMin);

/**
 * @brief Fila de nivel: "N" + nivel (5) + "cm" + porcentaje (4) + "%" +
 *        espacio + tendencia (ANCHO_TENDENCIA, alineada a la derecha).
 *
 * Siempre hay al menos un espacio entre el porcentaje y la tendencia, y la
 * fila ocupa exactamente 20 caracteres si los campos tienen su ancho.
 *
 * @param salida   Búfer de al menos 21 caracteres.
 * @param nivel    Texto del nivel, de 5 caracteres.
 * @param pct      Texto del porcentaje, de 4 caracteres.
 * @param tend     Texto de la tendencia, de hasta ANCHO_TENDENCIA caracteres.
 */
void filaNivel(char* salida, size_t tamano, const char* nivel, const char* pct, const char* tend);

}  // namespace texto_lcd
