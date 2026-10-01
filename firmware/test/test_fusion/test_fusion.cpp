/**
 * @file test_fusion.cpp
 * @brief Pruebas unitarias de la clasificación del estado de alerta (fusion.cpp).
 *
 * Se ejecutan en el computador: `pio test -d firmware -e native`.
 * Usan los parámetros de demostración de config.h (Wiki 4.4), copiados aquí
 * porque config.h depende del marco Arduino.
 */
#include <unity.h>

#include "fusion.h"

namespace {

fusion::Parametros parametrosDemo() {
    fusion::Parametros p{};
    p.nivelCriticoPct = 20.0f;
    p.nivelPreventivoPct = 40.0f;
    p.nivelHisteresisPct = 5.0f;
    p.descensoEntradaCmMin = -1.0f;
    p.descensoSalidaCmMin = -0.5f;
    p.descensoSostenidoS = 30.0f;
    p.temperaturaAltaC = 30.0f;
    p.temperaturaHisteresisC = 1.0f;
    p.vpdAltoKpa = 1.5f;
    p.vpdHisteresisKpa = 0.2f;
    p.uvAlto = 6.0f;
    p.uvHisteresis = 1.0f;
    return p;
}

/** Entradas de un ciclo sin ninguna condición activa. */
fusion::Entradas normales(float tiempoS) {
    fusion::Entradas e{};
    e.tiempoS = tiempoS;
    e.nivelIniciado = true;
    e.nivelOk = true;
    e.nivelPct = 80.0f;
    e.tendenciaOk = true;
    e.tendenciaCmMin = 0.0f;
    e.temperaturaOk = true;
    e.temperaturaC = 20.0f;
    e.vpdOk = true;
    e.vpdKpa = 0.8f;
    e.uvOk = true;
    e.uvIndice = 2.0f;
    return e;
}

/** Evalúa ciclos de 1 s con la tendencia dada, de t0 a t1 inclusive. */
EstadoAlerta tendenciaDurante(fusion::Clasificador& c, int t0, int t1, float tendencia) {
    EstadoAlerta estado = EstadoAlerta::INICIANDO;
    for (int t = t0; t <= t1; t++) {
        fusion::Entradas e = normales(static_cast<float>(t));
        e.tendenciaCmMin = tendencia;
        estado = c.evaluar(e, false).estado;
    }
    return estado;
}

void verificarEstado(EstadoAlerta esperado, EstadoAlerta obtenido) {
    TEST_ASSERT_EQUAL_STRING(textoAlerta(esperado), textoAlerta(obtenido));
}

}  // namespace

// ---------------------------------------------------------------------------
// Estados que no son de alerta
// ---------------------------------------------------------------------------

void test_sin_primera_lectura_es_iniciando() {
    fusion::Clasificador c(parametrosDemo());
    fusion::Entradas e = normales(0);
    e.nivelIniciado = false;
    e.nivelOk = false;
    verificarEstado(EstadoAlerta::INICIANDO, c.evaluar(e, false).estado);
}

void test_nivel_en_falla_es_falla_nivel_y_nunca_normal() {
    fusion::Clasificador c(parametrosDemo());
    fusion::Entradas e = normales(0);
    e.nivelOk = false;
    verificarEstado(EstadoAlerta::FALLA_NIVEL, c.evaluar(e, false).estado);
}

void test_sin_condiciones_es_normal() {
    fusion::Clasificador c(parametrosDemo());
    const fusion::Resultado r = c.evaluar(normales(0), false);
    verificarEstado(EstadoAlerta::NORMAL, r.estado);
    TEST_ASSERT_EQUAL_UINT8(CAUSA_NINGUNA, r.causas);
}

// ---------------------------------------------------------------------------
// Histéresis del nivel
// ---------------------------------------------------------------------------

void test_histeresis_del_nivel() {
    fusion::Clasificador c(parametrosDemo());
    fusion::Entradas e = normales(0);

    e.nivelPct = 20.0f;   // entra a crítico en el umbral
    verificarEstado(EstadoAlerta::CRITICO, c.evaluar(e, false).estado);
    e.nivelPct = 24.0f;   // no sale hasta superar 20 + 5
    verificarEstado(EstadoAlerta::CRITICO, c.evaluar(e, false).estado);
    e.nivelPct = 25.5f;   // sale de crítico; sigue bajo el preventivo
    verificarEstado(EstadoAlerta::ADVERTENCIA, c.evaluar(e, false).estado);
    e.nivelPct = 44.0f;   // no sale del preventivo hasta superar 40 + 5
    verificarEstado(EstadoAlerta::ADVERTENCIA, c.evaluar(e, false).estado);
    e.nivelPct = 46.0f;
    verificarEstado(EstadoAlerta::NORMAL, c.evaluar(e, false).estado);
}

// ---------------------------------------------------------------------------
// Descenso sostenido y condiciones atmosféricas
// ---------------------------------------------------------------------------

void test_descenso_sostenido_y_alerta() {
    fusion::Clasificador c(parametrosDemo());

    // Entrada: tendencia ≤ −1.0 cm/min durante 30 s seguidos.
    verificarEstado(EstadoAlerta::NORMAL, tendenciaDurante(c, 0, 29, -1.2f));
    verificarEstado(EstadoAlerta::ADVERTENCIA, tendenciaDurante(c, 30, 30, -1.2f));

    // Descenso + temperatura alta → ALERTA, con histéresis de 1 °C.
    fusion::Entradas e = normales(31);
    e.tendenciaCmMin = -1.2f;
    e.temperaturaC = 31.0f;
    const fusion::Resultado r = c.evaluar(e, false);
    verificarEstado(EstadoAlerta::ALERTA, r.estado);
    TEST_ASSERT_EQUAL_UINT8(CAUSA_DESCENSO | CAUSA_T_ALTA, r.causas);
    e.tiempoS = 32;
    e.temperaturaC = 29.5f;
    verificarEstado(EstadoAlerta::ALERTA, c.evaluar(e, false).estado);

    // DHT22 fuera de OK → la condición de temperatura cuenta como inactiva.
    e.tiempoS = 33;
    e.temperaturaC = 31.0f;
    e.temperaturaOk = false;
    verificarEstado(EstadoAlerta::ADVERTENCIA, c.evaluar(e, false).estado);
}

void test_salida_del_descenso_y_tendencia_no_disponible() {
    fusion::Clasificador c(parametrosDemo());
    verificarEstado(EstadoAlerta::ADVERTENCIA, tendenciaDurante(c, 0, 30, -1.2f));

    // Entre −1.0 y −0.5 cm/min se mantiene el descenso.
    verificarEstado(EstadoAlerta::ADVERTENCIA, tendenciaDurante(c, 31, 60, -0.7f));
    // Sale con tendencia > −0.5 cm/min durante 30 s seguidos.
    verificarEstado(EstadoAlerta::ADVERTENCIA, tendenciaDurante(c, 61, 90, -0.3f));
    verificarEstado(EstadoAlerta::NORMAL, tendenciaDurante(c, 91, 91, -0.3f));

    // Tendencia no disponible → condición de descenso inactiva.
    fusion::Clasificador c2(parametrosDemo());
    verificarEstado(EstadoAlerta::ADVERTENCIA, tendenciaDurante(c2, 0, 30, -1.2f));
    fusion::Entradas e = normales(31);
    e.tendenciaOk = false;
    verificarEstado(EstadoAlerta::NORMAL, c2.evaluar(e, false).estado);
}

// ---------------------------------------------------------------------------
// Desactivación de la alarma física y rearme
// ---------------------------------------------------------------------------

void test_desactivacion_y_rearme() {
    fusion::Clasificador c(parametrosDemo());
    fusion::Entradas e = normales(0);
    for (int t = 0; t <= 30; t++) {
        e = normales(static_cast<float>(t));
        e.tendenciaCmMin = -1.2f;
        e.vpdKpa = 1.6f;
        c.evaluar(e, false);
    }

    e.tiempoS = 31;
    fusion::Resultado r = c.evaluar(e, true);
    verificarEstado(EstadoAlerta::ALERTA, r.estado);
    TEST_ASSERT_TRUE(r.alarmaDesactivada);

    e.tiempoS = 32;
    e.nivelPct = 15.0f;  // empeora a CRÍTICO → se rearma
    r = c.evaluar(e, false);
    verificarEstado(EstadoAlerta::CRITICO, r.estado);
    TEST_ASSERT_FALSE(r.alarmaDesactivada);

    e.tiempoS = 33;
    r = c.evaluar(e, true);
    TEST_ASSERT_TRUE(r.alarmaDesactivada);

    e.tiempoS = 34;
    e.nivelOk = false;   // FALLA NIVEL anula la desactivación
    r = c.evaluar(e, false);
    verificarEstado(EstadoAlerta::FALLA_NIVEL, r.estado);
    TEST_ASSERT_FALSE(r.alarmaDesactivada);

    fusion::Clasificador c2(parametrosDemo());
    r = c2.evaluar(normales(0), true);  // orden sin alarma sonora → ignorada
    verificarEstado(EstadoAlerta::NORMAL, r.estado);
    TEST_ASSERT_FALSE(r.alarmaDesactivada);
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sin_primera_lectura_es_iniciando);
    RUN_TEST(test_nivel_en_falla_es_falla_nivel_y_nunca_normal);
    RUN_TEST(test_sin_condiciones_es_normal);
    RUN_TEST(test_histeresis_del_nivel);
    RUN_TEST(test_descenso_sostenido_y_alerta);
    RUN_TEST(test_salida_del_descenso_y_tendencia_no_disponible);
    RUN_TEST(test_desactivacion_y_rearme);
    return UNITY_END();
}
