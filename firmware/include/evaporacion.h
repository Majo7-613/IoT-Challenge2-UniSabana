/**
 * @file evaporacion.h
 * @brief Indicador de demanda evaporativa (VPD) y estimación diaria de la
 *        evaporación potencial de referencia (ET0, Hargreaves-Samani).
 *
 * Ecuaciones de FAO-56 (Allen et al., 1998):
 *  - ec. 11: e°(T) = 0.6108 · exp(17.27·T / (T + 237.3))            [kPa]
 *  - VPD = e°(T) − e_a, con e_a = e°(T) · HR/100                    [kPa]
 *  - ec. 21–25: radiación extraterrestre diaria Ra                  [MJ m⁻² d⁻¹]
 *  - ec. 52: ET0 = 0.0023 · (Tmedia + 17.8) · (Tmax − Tmin)^0.5 · Ra [mm/d],
 *            con Ra en mm/d (Ra[MJ] × 0.408) y Tmedia = (Tmax + Tmin)/2.
 *
 * Archivo sin dependencias de Arduino, para poder probarlo en un computador.
 */
#pragma once

#include <cstdint>

namespace evaporacion {

/** Presión de vapor de saturación e°(T) en kPa (FAO-56, ec. 11). */
float presionSaturacionKpa(float temperaturaC);

/**
 * @brief Déficit de presión de vapor, en kPa.
 * @return NAN si la humedad está fuera de 0–100 % o la temperatura es NAN.
 */
float vpdKpa(float temperaturaC, float humedadPct);

/**
 * @brief Radiación extraterrestre diaria Ra (FAO-56, ec. 21 a 25).
 * @param latitudGrados Latitud en grados (positiva al norte).
 * @param diaDelAnio    Día juliano, de 1 a 365 (o 366).
 * @return Ra en MJ m⁻² d⁻¹.
 */
float radiacionExtraterrestreMJ(float latitudGrados, int diaDelAnio);

/**
 * @brief ET0 diaria por Hargreaves-Samani (FAO-56, ec. 52), en mm/d.
 * @param tMaxC Temperatura máxima de 24 h.
 * @param tMinC Temperatura mínima de 24 h.
 * @param raMJ  Radiación extraterrestre en MJ m⁻² d⁻¹.
 * @return NAN si tMax < tMin o algún dato es NAN.
 */
float et0HargreavesMmDia(float tMaxC, float tMinC, float raMJ);

/**
 * @brief Extremos de temperatura de las últimas 24 h, en 288 intervalos de 5 min.
 *
 * Cada intervalo guarda el mínimo y el máximo de las lecturas recibidas en
 * él, de modo que los extremos no se pierden entre muestras. La ventana está
 * completa cuando se han cubierto 24 h desde el arranque.
 */
class ExtremosDiarios {
public:
    static constexpr uint16_t INTERVALOS = 288;           ///< 24 h / 5 min.
    static constexpr uint32_t DURACION_INTERVALO_S = 300; ///< 5 min.

    /** Registra una temperatura válida en el instante tiempoS (s desde el arranque). */
    void agregar(uint32_t tiempoS, float temperaturaC);

    /** Informa el paso del tiempo aunque no haya lectura válida. */
    void avanzar(uint32_t tiempoS);

    /**
     * @brief Extremos de la ventana de 24 h.
     * @return false si todavía no se han cubierto 24 h o no hay lecturas.
     */
    bool extremos(float& tMaxC, float& tMinC) const;

private:
    float    minimo_[INTERVALOS] = {};
    float    maximo_[INTERVALOS] = {};
    bool     conDato_[INTERVALOS] = {};
    uint32_t intervaloActual_ = 0;   ///< Número absoluto del intervalo en curso.
    bool     iniciado_ = false;
};

}  // namespace evaporacion
