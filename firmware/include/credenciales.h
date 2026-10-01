/**
 * @file credenciales.h
 * @brief Credenciales de la WLAN y del tablero.
 *
 * Los valores reales van en secrets.h, que está en .gitignore y nunca se sube
 * al repositorio. Si secrets.h no existe, se usan los valores de ejemplo de
 * secrets.example.h y CREDENCIALES_DE_EJEMPLO vale true, para advertirlo en el
 * registro al arrancar.
 *
 * En la compilación de simulación (SIMULACION), la WLAN es la red abierta
 * "Wokwi-GUEST" que ofrece Wokwi.
 */
#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
constexpr bool CREDENCIALES_DE_EJEMPLO = false;
#else
#include "secrets.example.h"
constexpr bool CREDENCIALES_DE_EJEMPLO = true;
#endif

#ifdef SIMULACION
constexpr const char* RED_SSID  = "Wokwi-GUEST";
constexpr const char* RED_CLAVE = "";
#else
constexpr const char* RED_SSID  = WIFI_SSID;
constexpr const char* RED_CLAVE = WIFI_PASSWORD;
#endif

constexpr const char* TABLERO_USUARIO_DIGEST = TABLERO_USUARIO;
constexpr const char* TABLERO_CLAVE_DIGEST   = TABLERO_CONTRASENA;

/** Tokens de los dispositivos autorizados. */
constexpr const char* TOKENS_AUTORIZADOS[] = TABLERO_TOKENS;
constexpr size_t      NUM_TOKENS_AUTORIZADOS = sizeof(TOKENS_AUTORIZADOS) / sizeof(TOKENS_AUTORIZADOS[0]);
