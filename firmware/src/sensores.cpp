/**
 * @file sensores.cpp
 * @brief Implementación de la adquisición de los sensores.
 *
 * Reglas de estado por sensor (Especificación de firmware, punto 4):
 *  - Intento válido                        → OK.
 *  - Intento inválido (NaN, fuera de rango
 *    o tiempo de espera del eco agotado)   → FALLA.
 *  - Sin intento en el ciclo (por ejemplo,
 *    el DHT22 en ciclos impares o el bus
 *    I2C ocupado)                          → conserva el estado; un OK pasa a
 *                                            VIEJO si la última lectura válida
 *                                            supera SENSOR_CICLOS_PARA_VIEJO ciclos.
 */
#include "sensores.h"

#include <Adafruit_BMP085.h>
#include <Arduino.h>
#include <DHTesp.h>
#include <Wire.h>
#include <algorithm>
#include <cmath>

#include "config.h"
#include "registro.h"

namespace {

// ---------------------------------------------------------------------------
// Seguimiento del estado de cada sensor
// ---------------------------------------------------------------------------

/** Lleva el estado OK / VIEJO / FALLA de un sensor entre ciclos. */
class SeguimientoSensor {
public:
    /** Registra el resultado de un intento de lectura. */
    void intento(bool valido) {
        if (valido) {
            estado_ = EstadoSensor::OK;
            ciclosDesdeValida_ = 0;
        } else {
            estado_ = EstadoSensor::FALLA;
            ciclosDesdeValida_++;
        }
    }

    /** Registra un ciclo en el que no se intentó leer el sensor. */
    void sinIntento() {
        ciclosDesdeValida_++;
        if (estado_ == EstadoSensor::OK && ciclosDesdeValida_ > SENSOR_CICLOS_PARA_VIEJO) {
            estado_ = EstadoSensor::VIEJO;
        }
    }

    EstadoSensor estado() const { return estado_; }

private:
    // Al arrancar no hay ninguna lectura válida: el sensor empieza como VIEJO.
    EstadoSensor estado_ = EstadoSensor::VIEJO;
    uint32_t ciclosDesdeValida_ = SENSOR_CICLOS_PARA_VIEJO + 1;
};

// ---------------------------------------------------------------------------
// Estado del módulo
// ---------------------------------------------------------------------------

SemaphoreHandle_t mtxBus = nullptr;
DHTesp            dht;
Adafruit_BMP085   bmp;
bool              bmpIniciado = false;

SeguimientoSensor segNivel;
SeguimientoSensor segDht;
SeguimientoSensor segBmp;
SeguimientoSensor segUv;

/** Últimas lecturas válidas (NAN si nunca las hubo). */
Lecturas ultimas = {NAN, 0, FuenteCompensacion::POR_DEFECTO, EstadoSensor::VIEJO,
                    NAN, NAN, EstadoSensor::VIEJO,
                    NAN, NAN, EstadoSensor::VIEJO,
                    NAN, NAN, EstadoSensor::VIEJO};

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

bool enRango(float valor, float minimo, float maximo) {
    return !std::isnan(valor) && valor >= minimo && valor <= maximo;
}

/** Recorre el bus I2C y registra los dispositivos que responden. */
void escanearI2C() {
    uint8_t encontrados = 0;
    for (uint8_t direccion = 1; direccion < 127; direccion++) {
        Wire.beginTransmission(direccion);
        if (Wire.endTransmission() == 0) {
            registrar("sensores", "I2C: dispositivo en 0x%02X", direccion);
            encontrados++;
        }
    }
    registrar("sensores", "I2C: %u dispositivo(s); esperados 0x%02X (LCD) y 0x%02X (BMP180)",
              static_cast<unsigned>(encontrados), I2C_DIR_LCD, I2C_DIR_BMP180);
}

/**
 * @brief Temperatura para compensar la velocidad del sonido.
 *
 * Prioridad: DHT22 en OK → BMP180 en OK → T_POR_DEFECTO_C.
 */
float temperaturaCompensacion(FuenteCompensacion& fuente) {
    if (segDht.estado() == EstadoSensor::OK && !std::isnan(ultimas.temperaturaDhtC)) {
        fuente = FuenteCompensacion::DHT22;
        return ultimas.temperaturaDhtC;
    }
    if (segBmp.estado() == EstadoSensor::OK && !std::isnan(ultimas.temperaturaBmpC)) {
        fuente = FuenteCompensacion::BMP180;
        return ultimas.temperaturaBmpC;
    }
    fuente = FuenteCompensacion::POR_DEFECTO;
    return T_POR_DEFECTO_C;
}

// ---------------------------------------------------------------------------
// Lectura de cada sensor
// ---------------------------------------------------------------------------

/**
 * @brief Mide la distancia con el HC-SR04: mediana de los ecos válidos.
 *
 * Cada eco tiene un tiempo de espera de HCSR04_TIMEOUT_ECO_US. Entre ecos se
 * espera HCSR04_PAUSA_ENTRE_ECOS_MS (sin ocupar la CPU) para que el eco
 * anterior se disipe. La lectura es válida si hay al menos
 * HCSR04_ECOS_MINIMOS ecos válidos y la distancia está en el rango del sensor.
 */
void leerNivel() {
    uint32_t tiempos[HCSR04_ECOS_POR_CICLO];
    uint8_t validos = 0;

    for (uint8_t i = 0; i < HCSR04_ECOS_POR_CICLO; i++) {
        // Pulso de disparo de 10 µs.
        digitalWrite(PIN_HCSR04_TRIG, LOW);
        delayMicroseconds(2);
        digitalWrite(PIN_HCSR04_TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(PIN_HCSR04_TRIG, LOW);

        // pulseIn devuelve 0 si se agota el tiempo de espera.
        const uint32_t tiempoUs = pulseIn(PIN_HCSR04_ECHO, HIGH, HCSR04_TIMEOUT_ECO_US);
        if (tiempoUs > 0) {
            tiempos[validos++] = tiempoUs;
        }
        if (i + 1 < HCSR04_ECOS_POR_CICLO) {
            vTaskDelay(pdMS_TO_TICKS(HCSR04_PAUSA_ENTRE_ECOS_MS));
        }
    }

    ultimas.ecosValidos = validos;
    if (validos < HCSR04_ECOS_MINIMOS) {
        segNivel.intento(false);
        return;
    }

    // Mediana de los tiempos válidos.
    std::sort(tiempos, tiempos + validos);
    const uint32_t medianaUs = tiempos[validos / 2];

    // Velocidad del sonido compensada: c = c0 + k·T [m/s].
    FuenteCompensacion fuente;
    const float temperatura = temperaturaCompensacion(fuente);
    const float velocidad = SONIDO_C0_M_S + SONIDO_COEF_M_S_POR_C * temperatura;

    // Distancia = c · t / 2, con t en µs y el resultado en cm.
    const float distanciaCm = velocidad * static_cast<float>(medianaUs) / 20000.0f;

    const bool valido = enRango(distanciaCm, HCSR04_DISTANCIA_MIN_CM, HCSR04_DISTANCIA_MAX_CM);
    segNivel.intento(valido);
    if (valido) {
        ultimas.distanciaCm = distanciaCm;
        ultimas.compensacion = fuente;
    }
}

/** Lee temperatura y humedad del DHT22. */
void leerDht() {
    const TempAndHumidity valores = dht.getTempAndHumidity();
    const bool valido = dht.getStatus() == DHTesp::ERROR_NONE &&
                        enRango(valores.temperature, DHT22_T_MIN_C, DHT22_T_MAX_C) &&
                        enRango(valores.humidity, 0.0f, 100.0f);
    segDht.intento(valido);
    if (valido) {
        ultimas.temperaturaDhtC = valores.temperature;
        ultimas.humedadPct = valores.humidity;
    }
}

/** Lee presión y temperatura del BMP180, con el bus I2C protegido. */
void leerBmp() {
    if (!bmpIniciado) {
        // Se reintenta la inicialización por si el sensor se conectó después.
        if (xSemaphoreTake(mtxBus, pdMS_TO_TICKS(I2C_ESPERA_MUTEX_MS)) != pdTRUE) {
            segBmp.sinIntento();
            return;
        }
        bmpIniciado = bmp.begin(BMP085_ULTRAHIGHRES, &Wire);
        xSemaphoreGive(mtxBus);
        if (!bmpIniciado) {
            segBmp.intento(false);
            return;
        }
    }

    if (xSemaphoreTake(mtxBus, pdMS_TO_TICKS(I2C_ESPERA_MUTEX_MS)) != pdTRUE) {
        segBmp.sinIntento();
        return;
    }
    const float temperatura = bmp.readTemperature();
    const float presionHpa = bmp.readPressure() / 100.0f;
    xSemaphoreGive(mtxBus);

    const bool valido = enRango(presionHpa, BMP180_P_MIN_HPA, BMP180_P_MAX_HPA) &&
                        enRango(temperatura, BMP180_T_MIN_C, BMP180_T_MAX_C);
    segBmp.intento(valido);
    if (valido) {
        ultimas.presionHpa = presionHpa;
        ultimas.temperaturaBmpC = temperatura;
    }
}

/** Lee la salida analógica del GUVA-S12SD y la convierte a índice UV. */
void leerUv() {
    const float milivoltios = static_cast<float>(analogReadMilliVolts(PIN_GUVA_OUT));
    // Una lectura en el tope de la escala indica saturación del ADC.
    const bool valido = enRango(milivoltios, 0.0f, GUVA_MAX_MV) && milivoltios < GUVA_MAX_MV;
    segUv.intento(valido);
    if (valido) {
        ultimas.uvMilivoltios = milivoltios;
        ultimas.uvIndice = (milivoltios / 1000.0f) * UV_INDICE_POR_VOLTIO;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Interfaz pública
// ---------------------------------------------------------------------------

const char* textoEstado(EstadoSensor estado) {
    switch (estado) {
        case EstadoSensor::OK:    return "OK";
        case EstadoSensor::VIEJO: return "VIEJO";
        case EstadoSensor::FALLA: return "FALLA";
    }
    return "?";
}

const char* textoCompensacion(FuenteCompensacion fuente) {
    switch (fuente) {
        case FuenteCompensacion::DHT22:       return "T DHT22";
        case FuenteCompensacion::BMP180:      return "T BMP180";
        case FuenteCompensacion::POR_DEFECTO: return "compensación por defecto";
    }
    return "?";
}

void sensoresIniciar(SemaphoreHandle_t mtxI2C) {
    mtxBus = mtxI2C;

    pinMode(PIN_HCSR04_TRIG, OUTPUT);
    digitalWrite(PIN_HCSR04_TRIG, LOW);
    pinMode(PIN_HCSR04_ECHO, INPUT);

    dht.setup(PIN_DHT22_DATA, DHTesp::DHT22);

    analogSetPinAttenuation(PIN_GUVA_OUT, GUVA_ATENUACION);

    xSemaphoreTake(mtxBus, portMAX_DELAY);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    escanearI2C();
    bmpIniciado = bmp.begin(BMP085_ULTRAHIGHRES, &Wire);
    xSemaphoreGive(mtxBus);

    registrar("sensores", "BMP180 %s", bmpIniciado ? "iniciado" : "no responde (FALLA)");
}

void sensoresLeer(bool leerDht22, Lecturas& salida) {
    // El DHT22 y el BMP180 se leen antes que el nivel para que la
    // compensación térmica use la temperatura más reciente.
    if (leerDht22) {
        leerDht();
    } else {
        segDht.sinIntento();
    }
    leerBmp();
    leerNivel();
    leerUv();

    ultimas.estadoNivel = segNivel.estado();
    ultimas.estadoDht = segDht.estado();
    ultimas.estadoBmp = segBmp.estado();
    ultimas.estadoUv = segUv.estado();
    salida = ultimas;
}
