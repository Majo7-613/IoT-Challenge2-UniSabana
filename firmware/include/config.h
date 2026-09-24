/**
 * @file config.h
 * @brief Configuración central del firmware: pines, periodos, tareas y
 *        parámetros de la lógica de fusión.
 *
 * Todos los números "mágicos" del sistema se definen aquí y en ningún otro
 * archivo. Los valores de campo y los de demostración van separados.
 *
 * Fuente: PLAN_DE_TRABAJO.md, "Especificación de firmware (decisiones 24/09)"
 * y "Guía de firmware".
 */
#pragma once

#include <Arduino.h>

// ============================================================================
// Puerto serie
// ============================================================================

/** Velocidad del puerto serie para el registro (igual a monitor_speed). */
constexpr uint32_t SERIAL_BAUDIOS = 115200;

// ============================================================================
// Mapa de pines (ESP32-DevKitC V4)
// Mapa propuesto en la guía de montaje; se confirma con el esquemático de
// hardware antes de soldar. No usar GPIO 6–11 (flash), 0, 2, 5, 12, 15
// (arranque) ni el ADC2 (compartido con el Wi-Fi).
// ============================================================================

constexpr uint8_t PIN_HCSR04_TRIG = 26;  ///< Disparo del HC-SR04.
constexpr uint8_t PIN_HCSR04_ECHO = 27;  ///< Eco del HC-SR04, con divisor 1 kΩ / 2 kΩ.
constexpr uint8_t PIN_DHT22_DATA  = 4;   ///< Datos del DHT22, pull-up de 10 kΩ a 3.3 V.
constexpr uint8_t PIN_I2C_SDA     = 21;  ///< Bus I2C compartido: BMP180 y LCD.
constexpr uint8_t PIN_I2C_SCL     = 22;  ///< Bus I2C compartido: BMP180 y LCD.
constexpr uint8_t PIN_GUVA_OUT    = 34;  ///< Salida analógica del GUVA-S12SD (ADC1).
constexpr uint8_t PIN_BUZZER      = 25;  ///< Buzzer pasivo, manejado por LEDC.

/** Direcciones I2C esperadas (se confirman con un escáner del bus). */
constexpr uint8_t I2C_DIR_LCD    = 0x27;
constexpr uint8_t I2C_DIR_BMP180 = 0x77;

// ============================================================================
// Temporización de la adquisición
// ============================================================================

/** Frecuencia del temporizador de hardware que despierta la adquisición. */
constexpr uint32_t TEMPORIZADOR_FRECUENCIA_HZ = 1;

/** Resolución del contador del temporizador (1 MHz → 1 cuenta = 1 µs). */
constexpr uint32_t TEMPORIZADOR_BASE_HZ = 1000000;

/** El DHT22 se lee cada N ciclos (2 s: intervalo mínimo de su hoja de datos). */
constexpr uint32_t DHT22_CADA_N_CICLOS = 2;

/** Número de ecos del HC-SR04 por ciclo; el nivel es su mediana. */
constexpr uint8_t HCSR04_ECOS_POR_CICLO = 5;

/**
 * Tiempo máximo de espera de un eco del HC-SR04: 25 ms equivalen a unos 4.3 m
 * de ida y vuelta y cubren los 400 cm de alcance de la hoja de datos.
 */
constexpr uint32_t HCSR04_TIMEOUT_ECO_US = 25000;

// ============================================================================
// Compensación térmica de la velocidad del sonido: c = 331.3 + 0.606·T [m/s]
// T se toma del DHT22; si está en FALLA, del BMP180; si ambos fallan, se usa
// T_POR_DEFECTO_C y el nivel se marca como "compensación por defecto".
// ============================================================================

constexpr float SONIDO_C0_M_S         = 331.3f;  ///< Velocidad a 0 °C.
constexpr float SONIDO_COEF_M_S_POR_C = 0.606f;  ///< Aumento por cada °C.
constexpr float T_POR_DEFECTO_C       = 20.0f;

/** Ciclos sin lectura válida tras los cuales un sensor pasa a VIEJO. */
constexpr uint8_t SENSOR_CICLOS_PARA_VIEJO = 3;

/** Periodo de publicación del estado. */
constexpr uint32_t PUBLICACION_PERIODO_MS = 1000;

/**
 * Tolerancia para considerar estable el periodo entre ciclos en el registro
 * del paso 1 (criterio de "hecho": 1 ciclo/s estable durante 10 minutos).
 */
constexpr uint32_t TOLERANCIA_PERIODO_US = 2000;

// ============================================================================
// Histórico en RAM (se pierde al reiniciar)
// ============================================================================

constexpr uint16_t HISTORICO_RAPIDO_REGISTROS = 120;   ///< 120 × 5 s = 10 min.
constexpr uint32_t HISTORICO_RAPIDO_PERIODO_MS = 5000;
constexpr uint16_t HISTORICO_LENTO_REGISTROS  = 288;   ///< 288 × 5 min = 24 h.
constexpr uint32_t HISTORICO_LENTO_PERIODO_MS  = 300000;

// ============================================================================
// Tareas FreeRTOS: núcleo, prioridad, pila (bytes) y periodo
// ============================================================================

constexpr BaseType_t NUCLEO_APLICACION = 1;  ///< Adquisición, fusión e interfaz local.
constexpr BaseType_t NUCLEO_RED        = 0;  ///< Histórico y red (núcleo del Wi-Fi).

constexpr UBaseType_t PRIORIDAD_SENSORES  = 4;
constexpr UBaseType_t PRIORIDAD_FUSION    = 3;
constexpr UBaseType_t PRIORIDAD_HMI       = 2;
constexpr UBaseType_t PRIORIDAD_HISTORICO = 1;
constexpr UBaseType_t PRIORIDAD_RED       = 1;

constexpr uint32_t PILA_SENSORES  = 4096;
constexpr uint32_t PILA_FUSION    = 4096;
constexpr uint32_t PILA_HMI       = 4096;
constexpr uint32_t PILA_HISTORICO = 4096;
constexpr uint32_t PILA_RED       = 4096;

constexpr uint32_t HMI_PERIODO_MS = 250;   ///< Refresco de LCD y patrón del buzzer.
constexpr uint32_t RED_PERIODO_MS = 5000;  ///< Supervisión del Wi-Fi.

// ============================================================================
// Alarma física (buzzer)
// ============================================================================

constexpr uint16_t BUZZER_ALERTA_HZ         = 440;  ///< ALERTA: intermitente.
constexpr uint16_t BUZZER_ALERTA_ENCENDIDO_MS = 500;
constexpr uint16_t BUZZER_ALERTA_APAGADO_MS   = 500;
constexpr uint16_t BUZZER_CRITICO_HZ        = 580;  ///< CRÍTICO: continuo.

/**
 * FALLA NIVEL no es una alarma hídrica sino una falla técnica: 2 pitidos
 * cortos de 1 kHz cada 10 s. Duración del pitido y de la pausa entre ambos:
 * valores iniciales, pendientes de confirmar con el equipo.
 */
constexpr uint16_t BUZZER_FALLA_HZ          = 1000;
constexpr uint8_t  BUZZER_FALLA_PITIDOS     = 2;
constexpr uint16_t BUZZER_FALLA_PITIDO_MS   = 100;
constexpr uint16_t BUZZER_FALLA_PAUSA_MS    = 100;
constexpr uint32_t BUZZER_FALLA_PERIODO_MS  = 10000;

// ============================================================================
// Parámetros de la lógica de fusión
// Los valores de DEMOSTRACIÓN se eligen para provocar cada estado en minutos
// con el banco de pruebas; no representan condiciones de campo.
// Valores de CAMPO: nivel y descenso → Pendiente (operación del acueducto);
// temperatura y VPD → Pendiente (IDEAM). No se definen hasta tener la fuente.
// ============================================================================

/** true: usa los parámetros de demostración. */
#ifndef MODO_DEMOSTRACION
#define MODO_DEMOSTRACION 1
#endif

namespace demo {
constexpr uint32_t TENDENCIA_VENTANA_S        = 60;    ///< Ventana de la regresión.
constexpr float    DESCENSO_PENDIENTE_CM_MIN  = -1.0f; ///< Pendiente ≤ este valor…
constexpr uint32_t DESCENSO_SOSTENIDO_S       = 30;    ///< …durante este tiempo seguido.
constexpr float    NIVEL_PREVENTIVO_FRACCION  = 0.40f; ///< De la altura útil.
constexpr float    NIVEL_CRITICO_FRACCION     = 0.20f; ///< De la altura útil.
constexpr float    NIVEL_HISTERESIS_FRACCION  = 0.05f; ///< De la altura útil.
constexpr float    T_ALTA_C                   = 30.0f;
constexpr float    T_HISTERESIS_C             = 1.0f;
constexpr float    VPD_ALTO_KPA               = 1.5f;
constexpr float    VPD_HISTERESIS_KPA         = 0.2f;
}  // namespace demo

namespace campo {
constexpr uint32_t TENDENCIA_VENTANA_S = 300;  ///< 5 min.
// Umbrales de nivel, descenso, temperatura y VPD: Pendiente (ver arriba).
}  // namespace campo

/** Índice UV ≥ 6: categoría "alto" del Índice UV Solar Mundial (OMS). */
constexpr float UV_INDICE_ALTO = 6.0f;

/** Histéresis del umbral de UV alto en demostración (unidades de índice UV). */
constexpr float UV_HISTERESIS_INDICE = 1.0f;

/**
 * Conversión inicial de la salida del módulo GUVA-S12SD a índice UV:
 * índice UV ≈ Vout [V] × 10 (aproximación habitual del módulo).
 * A caracterizar en la sección 4.2 de la Wiki contra el índice UV del día.
 */
constexpr float UV_INDICE_POR_VOLTIO = 10.0f;
