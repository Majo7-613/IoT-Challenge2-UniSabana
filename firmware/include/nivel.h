/**
 * @file nivel.h
 * @brief Conversión de distancia a nivel y tendencia del nivel (cm/min).
 *
 * Archivo sin dependencias de Arduino, para poder probarlo en un computador.
 */
#pragma once

#include <cstdint>

namespace nivel {

/**
 * @brief Geometría del punto de medición.
 *
 * alturaMontajeCm: distancia del HC-SR04 al fondo útil del recipiente.
 * alturaUtilCm:    altura de agua que corresponde al 100 % del nivel.
 */
struct Geometria {
    float alturaMontajeCm;
    float alturaUtilCm;
};

/** Nivel de agua en cm: alturaMontaje − distancia. NAN si la geometría no está definida. */
float nivelCm(float distanciaCm, const Geometria& geometria);

/** Nivel como fracción de la altura útil (0 = vacío, 1 = lleno), sin recortar. */
float nivelFraccion(float nivelCm, const Geometria& geometria);

/**
 * @brief Tendencia del nivel por regresión lineal sobre una ventana deslizante.
 *
 * Guarda una muestra por ciclo. La pendiente se calcula con mínimos cuadrados
 * sobre las muestras válidas de la ventana y se expresa en cm/min. Se exige que
 * al menos la mitad de la ventana tenga muestras válidas.
 */
class Tendencia {
public:
    static constexpr uint16_t MAX_MUESTRAS = 300;  ///< 5 min a 1 muestra por segundo.

    /** @param muestrasVentana Número de muestras de la ventana (≤ MAX_MUESTRAS). */
    explicit Tendencia(uint16_t muestrasVentana);

    /** Agrega la muestra del ciclo; si no es válida, ocupa su lugar sin dato. */
    void agregar(float tiempoS, float nivelCm, bool valida);

    /** @return false si no hay suficientes muestras válidas en la ventana. */
    bool pendienteCmMin(float& pendiente) const;

private:
    uint16_t ventana_;
    uint16_t siguiente_ = 0;
    uint16_t ocupadas_ = 0;
    float    tiempo_[MAX_MUESTRAS] = {};
    float    valor_[MAX_MUESTRAS] = {};
    bool     valida_[MAX_MUESTRAS] = {};
};

}  // namespace nivel
