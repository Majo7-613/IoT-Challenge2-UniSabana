/**
 * @file red.h
 * @brief Wi-Fi en modo estación con reconexión, y fecha por NTP para la ET0.
 *
 * Solo lo usa la tarea tRed. La medición y la alarma in situ no dependen de
 * este módulo: si la WLAN se pierde, solo deja de estar disponible el tablero.
 */
#pragma once

#include <cstdint>

/** Configura el Wi-Fi en modo estación e inicia la asociación a la WLAN. */
void redIniciar();

/**
 * @brief Supervisa la conexión (llamar cada RED_PERIODO_MS).
 *
 * Si la conexión se perdió, reintenta cada RED_REINTENTO_MS y registra el
 * tiempo de reconexión. Al conectarse por primera vez inicia el servidor web y
 * la sincronización NTP; si la hora es válida, informa el día del año a la
 * tarea de fusión para la ET0.
 *
 * @param ahoraMs Tiempo actual en ms desde el arranque.
 */
void redSupervisar(uint32_t ahoraMs);

/** true si el ESP32 está asociado a la WLAN y tiene dirección IP. */
bool redConectada();
