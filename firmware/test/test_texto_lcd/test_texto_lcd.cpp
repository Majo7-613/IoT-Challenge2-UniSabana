/**
 * @file test_texto_lcd.cpp
 * @brief Pruebas unitarias del formato de la fila de nivel de la LCD (texto_lcd.cpp).
 *
 * Se ejecutan en el computador: `pio test -d firmware -e native`.
 * Verifican que siempre haya un espacio entre el porcentaje y la tendencia y
 * que la fila no pase de 20 caracteres, también con tendencias de dos o más
 * cifras negativas (caso observado en Wokwi: "16%-19.73").
 */
#include <cstring>
#include <unity.h>

#include "texto_lcd.h"

namespace {

/** Posición del espacio después del "%": "N" + 5 + "cm" + 4 + "%". */
constexpr size_t POS_ESPACIO = 13;

/** Compone la fila con la tendencia dada y verifica ancho y separación. */
void verificarFila(float cmMin, const char* tendenciaEsperada, const char* filaEsperada) {
    char tend[16];
    texto_lcd::tendencia(tend, sizeof(tend), cmMin);
    TEST_ASSERT_EQUAL_STRING(tendenciaEsperada, tend);

    char fila[32];
    texto_lcd::filaNivel(fila, sizeof(fila), "  4.8", "  16", tend);
    TEST_ASSERT_EQUAL_STRING(filaEsperada, fila);
    TEST_ASSERT_EQUAL_size_t(20, strlen(fila));
    TEST_ASSERT_EQUAL_CHAR(' ', fila[POS_ESPACIO]);
}

}  // namespace

/** Tendencias de una cifra: espacio antes de la tendencia. */
void test_tendencia_de_una_cifra() {
    verificarFila(-0.40f, "-0.40", "N  4.8cm  16%  -0.40");
    verificarFila(0.0f, "0.00", "N  4.8cm  16%   0.00");
}

/** Caso observado en Wokwi: −19.73 cm/min. */
void test_tendencia_de_dos_cifras_negativa() {
    verificarFila(-19.73f, "-19.73", "N  4.8cm  16% -19.73");
}

/** Tendencias grandes: se reducen los decimales en lugar de cortar el texto. */
void test_tendencias_grandes_reducen_decimales() {
    verificarFila(-99.999f, "-100.0", "N  4.8cm  16% -100.0");
    verificarFila(-123.46f, "-123.5", "N  4.8cm  16% -123.5");
    verificarFila(-5000.0f, "-5000", "N  4.8cm  16%  -5000");
    verificarFila(-1.0e6f, "-9999", "N  4.8cm  16%  -9999");  // acotada
}

/** Sin tendencia disponible se muestra «--». */
void test_tendencia_no_disponible() {
    char fila[32];
    texto_lcd::filaNivel(fila, sizeof(fila), " 22.0", "  73", "--");
    TEST_ASSERT_EQUAL_STRING("N 22.0cm  73%     --", fila);
    TEST_ASSERT_EQUAL_size_t(20, strlen(fila));
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tendencia_de_una_cifra);
    RUN_TEST(test_tendencia_de_dos_cifras_negativa);
    RUN_TEST(test_tendencias_grandes_reducen_decimales);
    RUN_TEST(test_tendencia_no_disponible);
    return UNITY_END();
}
