/**
 * @file secrets.example.h
 * @brief Plantilla de credenciales.
 *
 * Copiar este archivo como `secrets.h` (en la misma carpeta) y llenar los
 * valores reales. `secrets.h` está en .gitignore y nunca se sube al repositorio.
 * Si secrets.h no existe, el firmware compila con estos valores de ejemplo y
 * lo advierte en el registro al arrancar.
 */
#pragma once

// Red Wi-Fi (WLAN) del sitio.
#define WIFI_SSID      "nombre-de-la-red"
#define WIFI_PASSWORD  "clave-de-la-red"

// Credenciales del tablero (autenticación Digest).
#define TABLERO_USUARIO     "operador"
#define TABLERO_CONTRASENA  "cambiar-esta-clave"

// Un token por dispositivo autorizado (celular o PC del operador). Se envía
// en el encabezado X-Token o en el parámetro ?token= de la URL.
#define TABLERO_TOKENS {"token-dispositivo-1", "token-dispositivo-2"}
