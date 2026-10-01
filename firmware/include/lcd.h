/**
 * @file lcd.h
 * @brief Controlador mínimo de una LCD HD44780 de 20x4 a través de un
 *        expansor I2C PCF8574, en modo de 4 bits.
 *
 * Conexión del expansor (la habitual de estos módulos y la del modelo
 * wokwi-lcd2004 en modo I2C): P0 = RS, P1 = RW, P2 = EN, P3 = retroiluminación,
 * P4–P7 = D4–D7.
 *
 * Estas funciones NO toman el mutex del bus I2C: quien las llama debe tenerlo.
 */
#pragma once

#include <cstdint>

namespace lcd {

/**
 * @brief Inicializa la pantalla (secuencia de 4 bits del HD44780) y la borra.
 * @param direccion Dirección I2C del expansor.
 * @return true si el expansor respondió.
 */
bool iniciar(uint8_t direccion);

/**
 * @brief Escribe una fila completa: el texto se recorta o se rellena con
 *        espacios hasta LCD_COLUMNAS, para no tener que borrar la pantalla.
 * @return true si la escritura por I2C tuvo éxito.
 */
bool escribirFila(uint8_t fila, const char* texto);

/** Enciende o apaga la retroiluminación. @return true si tuvo éxito. */
bool retroiluminacion(bool encendida);

}  // namespace lcd
