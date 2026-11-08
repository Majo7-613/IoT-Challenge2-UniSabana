/**
 * @file test_mensajes_mqtt.cpp
 * @brief Pruebas unitarias del formato JSON de los mensajes MQTT (mensajes_mqtt.cpp).
 *
 * Se ejecutan en el computador: `pio test -d firmware -e native`.
 * Verifican el contenido exacto de la telemetría y de la alerta definido en
 * docs/diseno/comunicacion-mqtt.md (sección 3), los valores null y el manejo
 * de un búfer demasiado pequeño.
 */
#include <cmath>
#include <cstring>
#include <unity.h>

#include "mensajes_mqtt.h"

namespace {

/** Telemetría de referencia: nodo en NORMAL con todos los sensores en OK. */
mensajes_mqtt::Telemetria telemetriaNormal() {
    mensajes_mqtt::Telemetria t{};
    t.id = 1532;
    t.ciclo = 7660;
    t.epocaValida = true;
    t.epoca = 1793284800;
    t.estado = EstadoAlerta::NORMAL;
    t.causas = CAUSA_NINGUNA;
    t.alarmaDesactivada = false;
    t.nivelCm = 22.0f;
    t.nivelPct = 73.3f;
    t.tendCmMin = -0.12f;
    t.tC = 18.4f;
    t.hrPct = 71.2f;
    t.pHpa = 752.4f;
    t.uvIdx = 1.2f;
    t.vpdKpa = 0.61f;
    t.et0MmDia = NAN;
    t.sensorNivel = "OK";
    t.sensorDht = "OK";
    t.sensorBmp = "OK";
    t.sensorUv = "OK";
    t.rssi = -61;
    return t;
}

}  // namespace

/** Telemetría completa: mismo texto que el ejemplo del diseño. */
void test_telemetria_normal() {
    char json[512];
    const size_t n = mensajes_mqtt::telemetria(telemetriaNormal(), json, sizeof(json));
    TEST_ASSERT_EQUAL_STRING(
        "{\"id\":1532,\"ciclo\":7660,\"epoca\":1793284800,\"estado\":\"NORMAL\",\"causas\":[],"
        "\"alarma_desactivada\":false,\"nivel_cm\":22.0,\"nivel_pct\":73,\"tend_cm_min\":-0.12,"
        "\"t_c\":18.4,\"hr_pct\":71,\"p_hpa\":752.4,\"uv_idx\":1.2,\"vpd_kpa\":0.61,\"et0_mm_dia\":null,"
        "\"sensores\":{\"nivel\":\"OK\",\"dht22\":\"OK\",\"bmp180\":\"OK\",\"guva\":\"OK\"},\"rssi\":-61}",
        json);
    TEST_ASSERT_EQUAL_size_t(strlen(json), n);
}

/** Sin hora NTP ni lecturas válidas: epoca y los valores se escriben como null. */
void test_telemetria_valores_null() {
    mensajes_mqtt::Telemetria t = telemetriaNormal();
    t.epocaValida = false;
    t.estado = EstadoAlerta::FALLA_NIVEL;
    t.nivelCm = NAN;
    t.nivelPct = NAN;
    t.tendCmMin = NAN;
    t.vpdKpa = NAN;
    t.sensorNivel = "FALLA";
    char json[512];
    TEST_ASSERT_NOT_EQUAL(0, mensajes_mqtt::telemetria(t, json, sizeof(json)));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"epoca\":null,"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"estado\":\"FALLA NIVEL\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"nivel_cm\":null,\"nivel_pct\":null,\"tend_cm_min\":null,"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"vpd_kpa\":null,"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"nivel\":\"FALLA\""));
}

/** Causas activas y alarma desactivada, en el orden de la tabla de causas. */
void test_telemetria_causas_y_alarma() {
    mensajes_mqtt::Telemetria t = telemetriaNormal();
    t.estado = EstadoAlerta::ALERTA;
    t.causas = CAUSA_DESCENSO | CAUSA_T_ALTA | CAUSA_UV_ALTO;
    t.alarmaDesactivada = true;
    char json[512];
    TEST_ASSERT_NOT_EQUAL(0, mensajes_mqtt::telemetria(t, json, sizeof(json)));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"causas\":[\"descenso\",\"temperatura_alta\",\"uv_alto\"],"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"alarma_desactivada\":true,"));
}

/** Alerta: estado anterior y nuevo con sus causas. */
void test_alerta() {
    mensajes_mqtt::Alerta a{};
    a.id = 1533;
    a.ciclo = 7702;
    a.epocaValida = true;
    a.epoca = 1793284842;
    a.anterior = EstadoAlerta::ADVERTENCIA;
    a.estado = EstadoAlerta::ALERTA;
    a.causas = CAUSA_DESCENSO | CAUSA_T_ALTA;
    a.nivelPct = 38.0f;
    a.tendCmMin = -1.4f;
    char json[256];
    const size_t n = mensajes_mqtt::alerta(a, json, sizeof(json));
    TEST_ASSERT_EQUAL_STRING(
        "{\"id\":1533,\"ciclo\":7702,\"epoca\":1793284842,\"anterior\":\"ADVERTENCIA\",\"estado\":\"ALERTA\","
        "\"causas\":[\"descenso\",\"temperatura_alta\"],\"nivel_pct\":38,\"tend_cm_min\":-1.40}",
        json);
    TEST_ASSERT_EQUAL_size_t(strlen(json), n);
}

/** Búfer demasiado pequeño: devuelve 0 en lugar de un JSON cortado. */
void test_bufer_pequeno() {
    char json[64];
    TEST_ASSERT_EQUAL_size_t(0, mensajes_mqtt::telemetria(telemetriaNormal(), json, sizeof(json)));
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_telemetria_normal);
    RUN_TEST(test_telemetria_valores_null);
    RUN_TEST(test_telemetria_causas_y_alarma);
    RUN_TEST(test_alerta);
    RUN_TEST(test_bufer_pequeno);
    return UNITY_END();
}
