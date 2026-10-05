/**
 * @file texto_lcd.cpp
 * @brief Implementación del formato de la fila de nivel de la LCD.
 */
#include "texto_lcd.h"

#include <cmath>
#include <cstdio>

namespace texto_lcd {

/** Tendencia con a lo sumo ANCHO_TENDENCIA caracteres (ver texto_lcd.h). */
void tendencia(char* salida, size_t tamano, float cmMin) {
    // Se acota a ±9999 cm/min: valores mayores no son físicos en el recipiente.
    const float v = std::fmax(-9999.0f, std::fmin(9999.0f, cmMin));
    const float magnitud = std::fabs(v);
    const int decimales = magnitud < 99.995f ? 2 : (magnitud < 999.95f ? 1 : 0);
    snprintf(salida, tamano, "%.*f", decimales, static_cast<double>(v));
}

/** Fila de nivel con un espacio fijo antes de la tendencia (ver texto_lcd.h). */
void filaNivel(char* salida, size_t tamano, const char* nivel, const char* pct, const char* tend) {
    snprintf(salida, tamano, "N%scm%s%% %*s", nivel, pct, ANCHO_TENDENCIA, tend);
}

}  // namespace texto_lcd
