/**
 * @file lcd.cpp
 * @brief Implementación del controlador HD44780 por PCF8574.
 *
 * Cada byte para el HD44780 se envía como dos medios bytes (nibbles); cada
 * nibble se escribe dos veces en el expansor, con EN en alto y luego en bajo,
 * para generar el pulso de habilitación. Los cuatro bytes de un carácter van
 * en una sola transacción I2C: a 100 kHz cada byte tarda unos 90 µs, más que
 * los 37 µs que necesita el HD44780 para ejecutar una instrucción común.
 */
#include "lcd.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"

namespace lcd {

namespace {

// Bits del expansor PCF8574.
constexpr uint8_t BIT_RS  = 0x01;
constexpr uint8_t BIT_EN  = 0x04;
constexpr uint8_t BIT_LUZ = 0x08;

// Instrucciones del HD44780.
constexpr uint8_t CMD_BORRAR          = 0x01;
constexpr uint8_t CMD_MODO_ENTRADA    = 0x06;  ///< Cursor a la derecha, sin desplazar la pantalla.
constexpr uint8_t CMD_PANTALLA_ON     = 0x0C;  ///< Pantalla encendida, sin cursor.
constexpr uint8_t CMD_FUNCION_4BITS   = 0x28;  ///< 4 bits, 2 líneas lógicas, 5x8 puntos.
constexpr uint8_t CMD_DIRECCION_DDRAM = 0x80;

/** Dirección de inicio de cada fila en una pantalla de 20x4. */
constexpr uint8_t INICIO_FILA[LCD_FILAS] = {0x00, 0x40, 0x14, 0x54};

uint8_t dir = 0;
uint8_t luz = BIT_LUZ;

/** Escribe un byte en el expansor. */
bool escribirExpansor(uint8_t valor) {
    Wire.beginTransmission(dir);
    Wire.write(valor | luz);
    return Wire.endTransmission() == 0;
}

/** Envía un byte al HD44780 (instrucción si rs = 0, dato si rs = BIT_RS). */
bool enviar(uint8_t valor, uint8_t rs) {
    const uint8_t alto = (valor & 0xF0) | rs | luz;
    const uint8_t bajo = static_cast<uint8_t>((valor << 4) & 0xF0) | rs | luz;
    Wire.beginTransmission(dir);
    Wire.write(alto | BIT_EN);
    Wire.write(alto);
    Wire.write(bajo | BIT_EN);
    Wire.write(bajo);
    return Wire.endTransmission() == 0;
}

/** Envía solo el nibble alto (usado en la secuencia de inicialización). */
bool enviarNibble(uint8_t nibbleAlto) {
    Wire.beginTransmission(dir);
    Wire.write(nibbleAlto | luz | BIT_EN);
    Wire.write(nibbleAlto | luz);
    return Wire.endTransmission() == 0;
}

}  // namespace

bool iniciar(uint8_t direccion) {
    dir = direccion;
    luz = BIT_LUZ;

    // Secuencia de inicialización por instrucciones del HD44780 para el modo
    // de 4 bits: tres veces 0x3 y luego 0x2, con las esperas de la hoja de datos.
    delay(50);
    if (!escribirExpansor(0)) {
        return false;
    }
    enviarNibble(0x30);
    delayMicroseconds(4500);
    enviarNibble(0x30);
    delayMicroseconds(4500);
    enviarNibble(0x30);
    delayMicroseconds(150);
    enviarNibble(0x20);

    bool ok = enviar(CMD_FUNCION_4BITS, 0);
    ok &= enviar(CMD_PANTALLA_ON, 0);
    ok &= enviar(CMD_BORRAR, 0);
    delay(2);  // Borrar tarda hasta 1.52 ms.
    ok &= enviar(CMD_MODO_ENTRADA, 0);
    return ok;
}

bool escribirFila(uint8_t fila, const char* texto) {
    if (fila >= LCD_FILAS) {
        return false;
    }
    bool ok = enviar(CMD_DIRECCION_DDRAM | INICIO_FILA[fila], 0);
    bool finTexto = false;
    for (uint8_t columna = 0; columna < LCD_COLUMNAS && ok; columna++) {
        if (!finTexto && texto[columna] == '\0') {
            finTexto = true;
        }
        ok = enviar(finTexto ? ' ' : static_cast<uint8_t>(texto[columna]), BIT_RS);
    }
    return ok;
}

bool retroiluminacion(bool encendida) {
    luz = encendida ? BIT_LUZ : 0;
    return escribirExpansor(0);
}

}  // namespace lcd
