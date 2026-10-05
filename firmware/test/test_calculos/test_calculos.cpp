/**
 * @file test_calculos.cpp
 * @brief Prueba unitaria del paso 3: nivel, tendencia, VPD, Ra y ET0 contra
 *        5 casos calculados a mano (criterio de «hecho» del paso 3, Wiki 2.5).
 *
 * Se ejecutan en el computador: `pio test -d firmware -e native`.
 * Valores de referencia:
 *  1. Nivel: montaje 35 cm, altura útil 30 cm, distancia 13 cm → 22 cm (73.3 %).
 *  2. Tendencia: recta que baja 1 cm por minuto durante 60 s → −1.00 cm/min.
 *  3. VPD a 25 °C y 60 %: e°(25 °C) = 3.168 kPa (tabla de FAO-56) → 1.267 kPa.
 *  4. Ra a 20° S el 3 de septiembre (día 246) = 32.2 MJ m⁻² d⁻¹ (FAO-56, ejemplo 8).
 *  5. ET0 con Tmáx 30 °C, Tmín 18 °C y Ra 32.2:
 *     0.0023 × (24 + 17.8) × √12 × 0.408 × 32.2 = 4.375 mm/d.
 */
#include <unity.h>

#include "evaporacion.h"
#include "nivel.h"

/** Caso 1: nivel y fracción con la geometría de demostración. */
void test_caso1_nivel() {
    const nivel::Geometria g{35.0f, 30.0f};
    const float n = nivel::nivelCm(13.0f, g);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.0f, n);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 22.0f / 30.0f, nivel::nivelFraccion(n, g));
}

/** Caso 2: recta que baja 1 cm por minuto → −1.00 cm/min. */
void test_caso2_tendencia() {
    nivel::Tendencia t(60);
    for (int s = 0; s < 60; s++) {
        t.agregar(1000.0f + s, 22.0f - s / 60.0f, true);  // −1 cm por minuto
    }
    float pendiente = 0.0f;
    TEST_ASSERT_TRUE(t.pendienteCmMin(pendiente));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, pendiente);
}

/** Caso 3: VPD a 25 °C y 60 %, con e°(25 °C) de la tabla de FAO-56. */
void test_caso3_vpd() {
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 3.168f, evaporacion::presionSaturacionKpa(25.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 1.267f, evaporacion::vpdKpa(25.0f, 60.0f));
}

/** Caso 4: Ra del ejemplo 8 de FAO-56. */
void test_caso4_radiacion_extraterrestre() {
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 32.2f, evaporacion::radiacionExtraterrestreMJ(-20.0f, 246));
}

/** Caso 5: ET0 de Hargreaves-Samani calculada a mano. */
void test_caso5_et0_hargreaves() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.375f, evaporacion::et0HargreavesMmDia(30.0f, 18.0f, 32.2f));
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_caso1_nivel);
    RUN_TEST(test_caso2_tendencia);
    RUN_TEST(test_caso3_vpd);
    RUN_TEST(test_caso4_radiacion_extraterrestre);
    RUN_TEST(test_caso5_et0_hargreaves);
    return UNITY_END();
}
