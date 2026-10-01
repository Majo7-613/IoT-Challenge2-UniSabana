/**
 * @file web.h
 * @brief Servidor web del tablero (sin MQTT): rutas, control de acceso y WebSocket.
 *
 * Rutas (Wiki 2.5.4):
 *  - GET  /                      tablero (data/index.html en LittleFS)
 *  - GET  /api/actual            JSON con el valor actual, el estado y la antigüedad de cada dato
 *  - GET  /api/historico?b=…     JSON del búfer rapido (10 min) o lento (24 h)
 *  - POST /api/alarma/desactivar orden de desactivar la alarma física
 *  - WS   /ws                    el mismo JSON de /api/actual, cada 1 s
 *  - GET  /<archivo>             archivos del frontend (CSS, JS) desde LittleFS
 *
 * Control de acceso, en este orden: dirección IP de la subred de la WLAN
 * (si no, 403), token de un dispositivo autorizado (si no, 403) y
 * autenticación Digest (si no, 401 con el desafío). El WebSocket exige subred
 * y token, porque los navegadores no repiten la autenticación Digest en él.
 * En el entorno wokwi el filtro de subred está desactivado (WEB_FILTRO_SUBRED).
 */
#pragma once

/**
 * @brief Registra las rutas e inicia el servidor (llamar con la WLAN conectada).
 * @return true si el servidor quedó escuchando.
 */
bool webIniciar();

/** Envía el estado actual a los clientes WebSocket y libera los desconectados. */
void webDifundir();
