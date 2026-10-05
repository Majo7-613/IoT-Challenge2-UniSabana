/**
 * @file fusion.h
 * @brief Clasificación del estado de alerta con histéresis y desactivación de
 *        la alarma física con rearme.
 *
 * Reglas (Wiki 2.5.2), evaluadas en cada ciclo de la más grave a la menos
 * grave; se aplica la primera que se cumple:
 *  - INICIANDO:   el sensor de nivel aún no tiene su primera lectura válida.
 *  - FALLA_NIVEL: el sensor de nivel no está en OK (nunca se muestra NORMAL).
 *  - CRITICO:     nivel ≤ umbral crítico, sin importar el ambiente.
 *  - ALERTA:      descenso anómalo sostenido Y (VPD alto O T alta O UV alto).
 *  - ADVERTENCIA: descenso anómalo sostenido O nivel ≤ umbral preventivo.
 *  - NORMAL:      ninguna de las anteriores.
 *
 * Cada condición tiene histéresis. Una condición atmosférica cuyo sensor no
 * está en OK, o un descenso con la tendencia no disponible, cuenta como
 * inactiva (decisiones del 25/09).
 *
 * Archivo sin dependencias de Arduino, para poder probarlo en un computador.
 */
#pragma once

#include <cstdint>

#include "estados.h"

namespace fusion {

/** Umbrales y tiempos de la clasificación (ver config.h). */
struct Parametros {
    float nivelCriticoPct;        ///< Entra a "nivel crítico" con nivel ≤ este valor.
    float nivelPreventivoPct;     ///< Entra a "nivel preventivo" con nivel ≤ este valor.
    float nivelHisteresisPct;     ///< Sale con nivel > umbral + histéresis.
    float descensoEntradaCmMin;   ///< Entra con tendencia ≤ este valor…
    float descensoSalidaCmMin;    ///< …y sale con tendencia > este valor…
    float descensoSostenidoS;     ///< …en ambos casos durante este tiempo seguido.
    float temperaturaAltaC;       ///< Entra con T ≥ este valor.
    float temperaturaHisteresisC; ///< Sale con T < umbral − histéresis.
    float vpdAltoKpa;
    float vpdHisteresisKpa;
    float uvAlto;                 ///< Índice UV.
    float uvHisteresis;
};

/** Datos de un ciclo que entran a la clasificación. */
struct Entradas {
    float tiempoS;          ///< Instante del ciclo, en s desde el arranque.
    bool  nivelIniciado;    ///< El sensor de nivel ya tuvo una lectura válida.
    bool  nivelOk;          ///< El nivel de este ciclo es utilizable.
    float nivelPct;         ///< Nivel en % de la altura útil.
    bool  tendenciaOk;      ///< La tendencia está disponible.
    float tendenciaCmMin;
    bool  temperaturaOk;    ///< DHT22 en OK.
    float temperaturaC;
    bool  vpdOk;
    float vpdKpa;
    bool  uvOk;             ///< GUVA-S12SD en OK.
    float uvIndice;
};

/** Resultado de la clasificación de un ciclo. */
struct Resultado {
    EstadoAlerta estado;
    uint8_t      causas;             ///< Combinación de CausaAlerta.
    bool         alarmaDesactivada;  ///< El buzzer no suena aunque el estado lo pida.
};

/** true si el estado tiene alarma hídrica sonora (ALERTA o CRÍTICO). */
bool tieneAlarmaSonora(EstadoAlerta estado);

/**
 * @brief Máquina de clasificación. Guarda entre ciclos el estado de cada
 *        condición (para la histéresis) y la desactivación de la alarma.
 *
 * Solo la usa la tarea de fusión; no es segura entre tareas.
 */
class Clasificador {
public:
    /** @param parametros Umbrales, histéresis y tiempos que usa la clasificación. */
    explicit Clasificador(const Parametros& parametros);

    /**
     * @brief Clasifica un ciclo.
     * @param entradas               Datos del ciclo.
     * @param solicitudDesactivacion true si el tablero pidió desactivar la
     *        alarma física desde el ciclo anterior. Solo se acepta en ALERTA o
     *        CRÍTICO.
     */
    Resultado evaluar(const Entradas& entradas, bool solicitudDesactivacion);

private:
    /** Actualiza la condición de descenso sostenido, con su histéresis temporal. */
    void actualizarDescenso(const Entradas& e);

    Parametros p_;
    bool  nivelCritico_ = false;
    bool  nivelPreventivo_ = false;
    bool  descenso_ = false;
    float descensoDesdeS_;           ///< Inicio del cruce en curso (NAN si no hay).
    bool  temperaturaAlta_ = false;
    bool  vpdAlto_ = false;
    bool  uvAlto_ = false;
    bool  desactivada_ = false;
    EstadoAlerta estadoAlDesactivar_ = EstadoAlerta::NORMAL;
};

}  // namespace fusion
