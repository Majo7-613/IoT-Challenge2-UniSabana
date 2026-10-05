/**
 * @file fusion.cpp
 * @brief Implementación de la clasificación del estado de alerta.
 */
#include "fusion.h"

#include <cmath>

// ---------------------------------------------------------------------------
// Texto de los estados (declarado en estados.h)
// ---------------------------------------------------------------------------

const char* textoAlerta(EstadoAlerta estado) {
    switch (estado) {
        case EstadoAlerta::INICIANDO:   return "INICIANDO";
        case EstadoAlerta::NORMAL:      return "NORMAL";
        case EstadoAlerta::ADVERTENCIA: return "ADVERTENCIA";
        case EstadoAlerta::ALERTA:      return "ALERTA";
        case EstadoAlerta::CRITICO:     return "CRITICO";
        case EstadoAlerta::FALLA_NIVEL: return "FALLA NIVEL";
    }
    return "?";
}

namespace fusion {

namespace {

/**
 * Histéresis de un umbral superior: entra con valor ≥ umbral y sale con
 * valor < umbral − histéresis.
 */
bool histeresisSuperior(bool activa, float valor, float umbral, float histeresis) {
    return activa ? valor >= umbral - histeresis : valor >= umbral;
}

/**
 * Histéresis de un umbral inferior: entra con valor ≤ umbral y sale con
 * valor > umbral + histéresis.
 */
bool histeresisInferior(bool activa, float valor, float umbral, float histeresis) {
    return activa ? valor <= umbral + histeresis : valor <= umbral;
}

/** Gravedad de los estados con alarma sonora, para decidir el rearme. */
int gravedad(EstadoAlerta estado) {
    switch (estado) {
        case EstadoAlerta::ALERTA:  return 1;
        case EstadoAlerta::CRITICO: return 2;
        default:                    return 0;
    }
}

}  // namespace

/** true si el estado tiene alarma hídrica sonora (ALERTA o CRÍTICO). */
bool tieneAlarmaSonora(EstadoAlerta estado) {
    return estado == EstadoAlerta::ALERTA || estado == EstadoAlerta::CRITICO;
}

Clasificador::Clasificador(const Parametros& parametros)
    : p_(parametros), descensoDesdeS_(NAN) {}

/** Actualiza la condición de descenso sostenido, con su histéresis temporal. */
void Clasificador::actualizarDescenso(const Entradas& e) {
    // Tendencia no disponible: la condición cuenta como inactiva.
    if (!e.tendenciaOk || std::isnan(e.tendenciaCmMin)) {
        descenso_ = false;
        descensoDesdeS_ = NAN;
        return;
    }

    // Mientras está inactiva se busca la entrada; mientras está activa, la
    // salida. En ambos casos el cruce debe sostenerse descensoSostenidoS.
    const bool cruce = descenso_ ? e.tendenciaCmMin > p_.descensoSalidaCmMin
                                 : e.tendenciaCmMin <= p_.descensoEntradaCmMin;
    if (!cruce) {
        descensoDesdeS_ = NAN;
        return;
    }
    if (std::isnan(descensoDesdeS_)) {
        descensoDesdeS_ = e.tiempoS;
    }
    if (e.tiempoS - descensoDesdeS_ >= p_.descensoSostenidoS) {
        descenso_ = !descenso_;
        descensoDesdeS_ = NAN;
    }
}

/** Clasifica un ciclo y aplica la orden de desactivar la alarma física, si la hay. */
Resultado Clasificador::evaluar(const Entradas& e, bool solicitudDesactivacion) {
    Resultado r{EstadoAlerta::NORMAL, CAUSA_NINGUNA, false};

    if (!e.nivelIniciado) {
        r.estado = EstadoAlerta::INICIANDO;
    } else if (!e.nivelOk || std::isnan(e.nivelPct)) {
        // Sin nivel no se evalúa el riesgo; las condiciones de nivel se
        // reinician para volver a evaluarse desde cero con la próxima lectura.
        r.estado = EstadoAlerta::FALLA_NIVEL;
        nivelCritico_ = false;
        nivelPreventivo_ = false;
    } else {
        // Condiciones de nivel.
        nivelCritico_ = histeresisInferior(nivelCritico_, e.nivelPct,
                                           p_.nivelCriticoPct, p_.nivelHisteresisPct);
        nivelPreventivo_ = histeresisInferior(nivelPreventivo_, e.nivelPct,
                                              p_.nivelPreventivoPct, p_.nivelHisteresisPct);
        actualizarDescenso(e);

        // Condiciones atmosféricas: inactivas si su sensor no está en OK.
        temperaturaAlta_ = e.temperaturaOk && !std::isnan(e.temperaturaC) &&
                           histeresisSuperior(temperaturaAlta_, e.temperaturaC,
                                              p_.temperaturaAltaC, p_.temperaturaHisteresisC);
        vpdAlto_ = e.vpdOk && !std::isnan(e.vpdKpa) &&
                   histeresisSuperior(vpdAlto_, e.vpdKpa, p_.vpdAltoKpa, p_.vpdHisteresisKpa);
        uvAlto_ = e.uvOk && !std::isnan(e.uvIndice) &&
                  histeresisSuperior(uvAlto_, e.uvIndice, p_.uvAlto, p_.uvHisteresis);

        const bool atmosfericaAlta = vpdAlto_ || temperaturaAlta_ || uvAlto_;

        // Reglas, de la más grave a la menos grave.
        if (nivelCritico_) {
            r.estado = EstadoAlerta::CRITICO;
        } else if (descenso_ && atmosfericaAlta) {
            r.estado = EstadoAlerta::ALERTA;
        } else if (descenso_ || nivelPreventivo_) {
            r.estado = EstadoAlerta::ADVERTENCIA;
        } else {
            r.estado = EstadoAlerta::NORMAL;
        }

        // Causas activas, para mostrarlas en el tablero. El nivel crítico
        // implica el preventivo, así que se informa solo la causa más grave.
        if (nivelCritico_) {
            r.causas |= CAUSA_NIVEL_CRITICO;
        } else if (nivelPreventivo_) {
            r.causas |= CAUSA_NIVEL_PREVENTIVO;
        }
        if (descenso_)        r.causas |= CAUSA_DESCENSO;
        if (vpdAlto_)         r.causas |= CAUSA_VPD_ALTO;
        if (temperaturaAlta_) r.causas |= CAUSA_T_ALTA;
        if (uvAlto_)          r.causas |= CAUSA_UV_ALTO;
    }

    // Desactivación de la alarma física y rearme (Wiki 2.5.3):
    //  - se anula si el estado deja de tener alarma sonora;
    //  - se rearma si el estado empeora (ALERTA → CRÍTICO);
    //  - solo se acepta en ALERTA o CRÍTICO.
    if (!tieneAlarmaSonora(r.estado)) {
        desactivada_ = false;
    } else if (desactivada_ && gravedad(r.estado) > gravedad(estadoAlDesactivar_)) {
        desactivada_ = false;
    }
    if (solicitudDesactivacion && tieneAlarmaSonora(r.estado)) {
        desactivada_ = true;
        estadoAlDesactivar_ = r.estado;
    }
    r.alarmaDesactivada = desactivada_;
    return r;
}

}  // namespace fusion
