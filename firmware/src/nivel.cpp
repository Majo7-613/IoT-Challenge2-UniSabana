/**
 * @file nivel.cpp
 * @brief Implementación del nivel y de su tendencia.
 */
#include "nivel.h"

#include <cmath>

namespace nivel {

/** Nivel de agua en cm: altura de montaje − distancia; NAN si falta un dato. */
float nivelCm(float distanciaCm, const Geometria& geometria) {
    if (std::isnan(distanciaCm) || std::isnan(geometria.alturaMontajeCm)) {
        return NAN;
    }
    return geometria.alturaMontajeCm - distanciaCm;
}

/** Nivel como fracción de la altura útil, sin recortar; NAN si falta un dato. */
float nivelFraccion(float nivelCm, const Geometria& geometria) {
    if (std::isnan(nivelCm) || std::isnan(geometria.alturaUtilCm) ||
        geometria.alturaUtilCm <= 0.0f) {
        return NAN;
    }
    return nivelCm / geometria.alturaUtilCm;
}

/** Crea la ventana de la regresión, acotada a MAX_MUESTRAS. */
Tendencia::Tendencia(uint16_t muestrasVentana)
    : ventana_(muestrasVentana > MAX_MUESTRAS ? MAX_MUESTRAS : muestrasVentana) {
    if (ventana_ < 2) {
        ventana_ = 2;
    }
}

/** Agrega la muestra del ciclo; si no es válida, ocupa su lugar sin dato. */
void Tendencia::agregar(float tiempoS, float nivelCm, bool valida) {
    tiempo_[siguiente_] = tiempoS;
    valor_[siguiente_] = nivelCm;
    valida_[siguiente_] = valida && !std::isnan(nivelCm);
    siguiente_ = static_cast<uint16_t>((siguiente_ + 1) % ventana_);
    if (ocupadas_ < ventana_) {
        ocupadas_++;
    }
}

/** Pendiente por mínimos cuadrados en cm/min; false si no hay suficientes muestras válidas. */
bool Tendencia::pendienteCmMin(float& pendiente) const {
    // Se centra el tiempo en la primera muestra válida para conservar precisión
    // en float con tiempos grandes desde el arranque.
    uint16_t n = 0;
    double t0 = 0.0;
    double sumT = 0.0, sumV = 0.0, sumTT = 0.0, sumTV = 0.0;
    for (uint16_t i = 0; i < ocupadas_; i++) {
        if (!valida_[i]) {
            continue;
        }
        if (n == 0) {
            t0 = tiempo_[i];
        }
        const double t = tiempo_[i] - t0;
        const double v = valor_[i];
        sumT += t;
        sumV += v;
        sumTT += t * t;
        sumTV += t * v;
        n++;
    }
    if (n < 2 || n * 2 < ventana_) {
        return false;
    }
    const double denominador = n * sumTT - sumT * sumT;
    if (denominador <= 0.0) {
        return false;
    }
    const double pendienteCmS = (n * sumTV - sumT * sumV) / denominador;
    pendiente = static_cast<float>(pendienteCmS * 60.0);
    return true;
}

}  // namespace nivel
