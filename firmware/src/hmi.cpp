/**
 * @file hmi.cpp
 * @brief Implementación de la interfaz local: LCD 20x4 y buzzer.
 *
 * Distribución de la LCD (Wiki 2.5.3):
 *   fila 1: N 18.2cm  61% -0.40    nivel, % de la altura útil y tendencia (cm/min)
 *   fila 2: T 21.5C HR 63% P 752   temperatura, humedad relativa y presión
 *   fila 3: UV 3.2 VPD 0.95kPa     índice UV y VPD
 *   fila 4: ALERTA (DESACT.)       estado de alerta, fijo
 * Marcas por sensor: valor en OK; último valor válido con "*" en VIEJO; "ERR"
 * en FALLA; "ini" mientras no haya primera lectura válida. Tendencia no
 * disponible: "--".
 */
#include "hmi.h"

#include <Arduino.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <esp_arduino_version.h>
#include <esp_timer.h>

#include "config.h"
#include "fusion.h"
#include "lcd.h"
#include "registro.h"
#include "texto_lcd.h"

namespace {

// ---------------------------------------------------------------------------
// Buzzer
// ---------------------------------------------------------------------------

/** Patrón sonoro pedido por tHMI y generado por el temporizador del buzzer. */
enum Patron : uint8_t { PATRON_SILENCIO, PATRON_ALERTA, PATRON_CRITICO, PATRON_FALLA_NIVEL };

/** Ciclo común de todos los patrones (el de FALLA NIVEL contiene al de ALERTA). */
constexpr uint32_t CICLO_PATRONES_MS = BUZZER_FALLA_PERIODO_MS;

std::atomic<uint8_t> patronPedido{PATRON_SILENCIO};
esp_timer_handle_t   temporizadorBuzzer = nullptr;

// Estado propio del temporizador del buzzer (solo lo toca su retrollamada).
uint8_t  patronEnCurso = PATRON_SILENCIO;
uint32_t faseMs = 0;
uint16_t frecuenciaActual = 0;

/** Enciende el tono de la frecuencia dada (0 = silencio). */
void tono(uint16_t frecuenciaHz) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWriteTone(PIN_BUZZER, frecuenciaHz);
#else
    ledcWriteTone(BUZZER_LEDC_CANAL, frecuenciaHz);
#endif
}

/** Frecuencia que corresponde a un patrón en un instante de su ciclo. */
uint16_t frecuenciaDelPatron(uint8_t patron, uint32_t fase) {
    switch (patron) {
        case PATRON_ALERTA: {
            const uint32_t periodo = BUZZER_ALERTA_ENCENDIDO_MS + BUZZER_ALERTA_APAGADO_MS;
            return (fase % periodo) < BUZZER_ALERTA_ENCENDIDO_MS ? BUZZER_ALERTA_HZ : 0;
        }
        case PATRON_CRITICO:
            return BUZZER_CRITICO_HZ;
        case PATRON_FALLA_NIVEL: {
            // BUZZER_FALLA_PITIDOS pitidos separados por una pausa, al inicio del periodo.
            const uint32_t paso = BUZZER_FALLA_PITIDO_MS + BUZZER_FALLA_PAUSA_MS;
            const uint32_t enPeriodo = fase % BUZZER_FALLA_PERIODO_MS;
            if (enPeriodo >= paso * BUZZER_FALLA_PITIDOS) {
                return 0;
            }
            return (enPeriodo % paso) < BUZZER_FALLA_PITIDO_MS ? BUZZER_FALLA_HZ : 0;
        }
        default:
            return 0;
    }
}

/**
 * @brief Retrollamada del temporizador del buzzer (cada BUZZER_TICK_MS).
 *
 * Corre en la tarea del servicio esp_timer, no en una interrupción. Cambia el
 * tono solo cuando la frecuencia debe cambiar. Un patrón nuevo empieza desde
 * el inicio de su ciclo, de modo que una ALERTA empieza sonando.
 */
void tickBuzzer(void*) {
    const uint8_t pedido = patronPedido.load();
    if (pedido != patronEnCurso) {
        patronEnCurso = pedido;
        faseMs = 0;
    } else {
        faseMs = (faseMs + BUZZER_TICK_MS) % CICLO_PATRONES_MS;
    }
    const uint16_t frecuencia = frecuenciaDelPatron(patronEnCurso, faseMs);
    if (frecuencia != frecuenciaActual) {
        tono(frecuencia);
        frecuenciaActual = frecuencia;
    }
}

/** Patrón que corresponde al estado publicado. */
uint8_t patronDelEstado(const Derivados& d) {
    switch (d.estado) {
        case EstadoAlerta::ALERTA:      return d.alarmaDesactivada ? PATRON_SILENCIO : PATRON_ALERTA;
        case EstadoAlerta::CRITICO:     return d.alarmaDesactivada ? PATRON_SILENCIO : PATRON_CRITICO;
        case EstadoAlerta::FALLA_NIVEL: return PATRON_FALLA_NIVEL;
        default:                        return PATRON_SILENCIO;
    }
}

// ---------------------------------------------------------------------------
// LCD
// ---------------------------------------------------------------------------

SemaphoreHandle_t mtxBus = nullptr;
bool     lcdOk = false;
bool     luzEncendida = true;
uint32_t ultimoReintentoMs = 0;
uint32_t ultimoRefrescoMs = 0;
bool     primerRefresco = true;

/** Toma el bus I2C con la espera máxima de config.h. */
bool tomarBus() {
    return xSemaphoreTake(mtxBus, pdMS_TO_TICKS(I2C_ESPERA_MUTEX_MS)) == pdTRUE;
}

/** Inicializa la LCD y muestra INICIANDO hasta el primer estado publicado. */
void iniciarLcd() {
    if (!tomarBus()) {
        return;
    }
    lcdOk = lcd::iniciar(I2C_DIR_LCD);
    if (lcdOk) {
        lcd::escribirFila(3, textoAlerta(EstadoAlerta::INICIANDO));
    }
    xSemaphoreGive(mtxBus);
    luzEncendida = true;
    primerRefresco = true;
    registrar("hmi", "LCD %s (0x%02X)", lcdOk ? "iniciada" : "no responde", I2C_DIR_LCD);
}

/**
 * @brief Texto de un valor con la marca del estado de su sensor, alineado a
 *        la derecha en `ancho` caracteres.
 * @return true si se muestra un número (OK o VIEJO).
 */
bool textoCampo(char* salida, size_t tamano, int ancho, int decimales, float valor,
           EstadoSensor estado, bool iniciado) {
    if (!iniciado) {
        snprintf(salida, tamano, "%*s", ancho, "ini");
        return false;
    }
    if (estado == EstadoSensor::FALLA || std::isnan(valor)) {
        snprintf(salida, tamano, "%*s", ancho, "ERR");
        return false;
    }
    if (estado == EstadoSensor::VIEJO) {
        snprintf(salida, tamano, "%*.*f*", ancho - 1, decimales, valor);
    } else {
        snprintf(salida, tamano, "%*.*f", ancho, decimales, valor);
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// Interfaz pública
// ---------------------------------------------------------------------------

void hmiComponerFilas(const EstadoPublicado& estado, char filas[][21]) {
    const Lecturas&  l = estado.instantanea.lecturas;
    const Derivados& d = estado.derivados;
    constexpr size_t TAM = 21;
    char a[12];
    char b[12];
    char c[12];
    // Cada fila se compone en un búfer amplio y se recorta de forma explícita
    // a LCD_COLUMNAS caracteres al copiarla (%.20s).
    char fila[64];

    // Fila 1: nivel, porcentaje y tendencia.
    if (textoCampo(a, sizeof(a), 5, 1, d.nivelCm, l.estadoNivel, l.iniciadoNivel)) {
        textoCampo(b, sizeof(b), 4, 0, d.nivelPct, l.estadoNivel, l.iniciadoNivel);
        if (d.pendienteValida) {
            texto_lcd::tendencia(c, sizeof(c), d.pendienteCmMin);
        } else {
            snprintf(c, sizeof(c), "--");
        }
        texto_lcd::filaNivel(fila, sizeof(fila), a, b, c);
    } else {
        snprintf(fila, sizeof(fila), "N%s", a);
    }
    snprintf(filas[0], TAM, "%.20s", fila);

    // Fila 2: temperatura y humedad (DHT22) y presión (BMP180).
    const bool hayT = textoCampo(a, sizeof(a), 5, 1, l.temperaturaDhtC, l.estadoDht, l.iniciadoDht);
    const bool hayHr = textoCampo(b, sizeof(b), 3, 0, l.humedadPct, l.estadoDht, l.iniciadoDht);
    textoCampo(c, sizeof(c), 4, 0, l.presionHpa, l.estadoBmp, l.iniciadoBmp);
    snprintf(fila, sizeof(fila), "T%s%s HR%s%s P%s", a, hayT ? "C" : " ", b, hayHr ? "%" : " ", c);
    snprintf(filas[1], TAM, "%.20s", fila);

    // Fila 3: índice UV (GUVA-S12SD) y VPD (calculado con el DHT22).
    textoCampo(a, sizeof(a), 4, 1, l.uvIndice, l.estadoUv, l.iniciadoUv);
    const bool hayVpd = textoCampo(b, sizeof(b), 5, 2, d.vpdValido ? d.vpdKpa : NAN,
                              l.estadoDht, l.iniciadoDht);
    snprintf(fila, sizeof(fila), "UV%s VPD%s%s", a, b, hayVpd ? "kPa" : "");
    snprintf(filas[2], TAM, "%.20s", fila);

    // Fila 4: estado de alerta, fijo.
    snprintf(filas[3], TAM, "%s%s", textoAlerta(d.estado),
             d.alarmaDesactivada ? " (DESACT.)" : "");
}

/** Configura el buzzer (LEDC y temporizador del patrón) e inicializa la LCD. */
bool hmiIniciar(SemaphoreHandle_t mtxI2C) {
    mtxBus = mtxI2C;

    // Buzzer en silencio.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_BUZZER, BUZZER_ALERTA_HZ, BUZZER_LEDC_RESOLUCION);
#else
    ledcSetup(BUZZER_LEDC_CANAL, BUZZER_ALERTA_HZ, BUZZER_LEDC_RESOLUCION);
    ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CANAL);
#endif
    tono(0);

    esp_timer_create_args_t argumentos{};
    argumentos.callback = &tickBuzzer;
    argumentos.name = "buzzer";
    argumentos.dispatch_method = ESP_TIMER_TASK;
    bool ok = esp_timer_create(&argumentos, &temporizadorBuzzer) == ESP_OK &&
              esp_timer_start_periodic(temporizadorBuzzer, BUZZER_TICK_MS * 1000ULL) == ESP_OK;
    registrar("hmi", "buzzer %s (GPIO %u, patrón cada %lu ms)", ok ? "listo" : "ERROR",
              static_cast<unsigned>(PIN_BUZZER), static_cast<unsigned long>(BUZZER_TICK_MS));

    iniciarLcd();
    ultimoReintentoMs = millis();
    return ok;
}

/** Actualiza la LCD, la retroiluminación y el patrón del buzzer según el estado publicado. */
void hmiActualizar(const EstadoPublicado& estado, uint32_t ahoraMs) {
    const Derivados& d = estado.derivados;

    // Buzzer: solo se fija el patrón; lo genera el temporizador.
    patronPedido.store(patronDelEstado(d));

    // LCD ausente: reintento periódico.
    if (!lcdOk) {
        if (ahoraMs - ultimoReintentoMs >= LCD_REINTENTO_MS) {
            ultimoReintentoMs = ahoraMs;
            iniciarLcd();
        }
        return;
    }

    // Aviso visual en CRÍTICO: parpadeo de la retroiluminación.
    const bool luz = d.estado != EstadoAlerta::CRITICO || ((ahoraMs / LCD_PARPADEO_MS) % 2 == 0);
    if (luz != luzEncendida && tomarBus()) {
        lcdOk = lcd::retroiluminacion(luz);
        xSemaphoreGive(mtxBus);
        luzEncendida = luz;
    }

    // Contenido, cada LCD_REFRESCO_MS.
    if (lcdOk && (primerRefresco || ahoraMs - ultimoRefrescoMs >= LCD_REFRESCO_MS)) {
        char filas[LCD_FILAS][21];
        hmiComponerFilas(estado, filas);
        if (tomarBus()) {
            for (uint8_t i = 0; i < LCD_FILAS && lcdOk; i++) {
                lcdOk = lcd::escribirFila(i, filas[i]);
            }
            xSemaphoreGive(mtxBus);
            ultimoRefrescoMs = ahoraMs;
            primerRefresco = false;
        }
    }

    if (!lcdOk) {
        registrar("hmi", "ERROR: la LCD dejó de responder; se reintentará");
        ultimoReintentoMs = ahoraMs;
    }
}
