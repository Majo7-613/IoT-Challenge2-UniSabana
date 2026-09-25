/**
 * @file evaporacion.cpp
 * @brief Implementación del VPD, la radiación extraterrestre y la ET0.
 */
#include "evaporacion.h"

#include <cmath>

namespace evaporacion {

namespace {
constexpr float PI_F = 3.14159265358979f;
constexpr float GSC_MJ_M2_MIN = 0.0820f;  ///< Constante solar (FAO-56).
constexpr float MJ_A_MM = 0.408f;         ///< 1 MJ m⁻² d⁻¹ = 0.408 mm/d (FAO-56).
}  // namespace

float presionSaturacionKpa(float temperaturaC) {
    return 0.6108f * std::exp(17.27f * temperaturaC / (temperaturaC + 237.3f));
}

float vpdKpa(float temperaturaC, float humedadPct) {
    if (std::isnan(temperaturaC) || std::isnan(humedadPct) ||
        humedadPct < 0.0f || humedadPct > 100.0f) {
        return NAN;
    }
    const float es = presionSaturacionKpa(temperaturaC);
    const float ea = es * humedadPct / 100.0f;
    return es - ea;
}

float radiacionExtraterrestreMJ(float latitudGrados, int diaDelAnio) {
    const float phi = latitudGrados * PI_F / 180.0f;
    const float x = 2.0f * PI_F * static_cast<float>(diaDelAnio) / 365.0f;
    const float dr = 1.0f + 0.033f * std::cos(x);               // ec. 23
    const float delta = 0.409f * std::sin(x - 1.39f);           // ec. 24
    const float omegaS = std::acos(-std::tan(phi) * std::tan(delta));  // ec. 25
    return (24.0f * 60.0f / PI_F) * GSC_MJ_M2_MIN * dr *        // ec. 21
           (omegaS * std::sin(phi) * std::sin(delta) +
            std::cos(phi) * std::cos(delta) * std::sin(omegaS));
}

float et0HargreavesMmDia(float tMaxC, float tMinC, float raMJ) {
    if (std::isnan(tMaxC) || std::isnan(tMinC) || std::isnan(raMJ) || tMaxC < tMinC) {
        return NAN;
    }
    const float tMedia = (tMaxC + tMinC) / 2.0f;
    const float raMm = raMJ * MJ_A_MM;
    return 0.0023f * (tMedia + 17.8f) * std::sqrt(tMaxC - tMinC) * raMm;
}

// ---------------------------------------------------------------------------
// ExtremosDiarios
// ---------------------------------------------------------------------------

void ExtremosDiarios::avanzar(uint32_t tiempoS) {
    const uint32_t intervalo = tiempoS / DURACION_INTERVALO_S;
    if (!iniciado_) {
        intervaloActual_ = intervalo;
        iniciado_ = true;
        return;
    }
    // Se vacían los intervalos que quedaron atrás desde la última llamada.
    while (intervaloActual_ < intervalo) {
        intervaloActual_++;
        conDato_[intervaloActual_ % INTERVALOS] = false;
    }
}

void ExtremosDiarios::agregar(uint32_t tiempoS, float temperaturaC) {
    avanzar(tiempoS);
    if (std::isnan(temperaturaC)) {
        return;
    }
    const uint16_t i = intervaloActual_ % INTERVALOS;
    if (!conDato_[i]) {
        minimo_[i] = temperaturaC;
        maximo_[i] = temperaturaC;
        conDato_[i] = true;
    } else {
        minimo_[i] = std::fmin(minimo_[i], temperaturaC);
        maximo_[i] = std::fmax(maximo_[i], temperaturaC);
    }
}

bool ExtremosDiarios::extremos(float& tMaxC, float& tMinC) const {
    // La ventana está completa cuando ya pasaron al menos 24 h desde el arranque.
    if (!iniciado_ || intervaloActual_ + 1 < INTERVALOS) {
        return false;
    }
    bool hay = false;
    for (uint16_t i = 0; i < INTERVALOS; i++) {
        if (!conDato_[i]) {
            continue;
        }
        if (!hay) {
            tMaxC = maximo_[i];
            tMinC = minimo_[i];
            hay = true;
        } else {
            tMaxC = std::fmax(tMaxC, maximo_[i]);
            tMinC = std::fmin(tMinC, minimo_[i]);
        }
    }
    return hay;
}

}  // namespace evaporacion
