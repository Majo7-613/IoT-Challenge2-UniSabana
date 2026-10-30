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

// Credenciales MQTT (Challenge #3). Un secrets.h anterior al Challenge #3 no
// las trae: se usan las de ejemplo y se advierte en el registro.
#ifndef MQTT_BROKER_URI
#define MQTT_BROKER_URI "mqtt://192.168.1.50:1883"
#define MQTT_USUARIO    "nodo-chia-01"
#define MQTT_CLAVE      "cambiar-esta-clave"
constexpr bool MQTT_CREDENCIALES_DE_EJEMPLO = true;
#else
constexpr bool MQTT_CREDENCIALES_DE_EJEMPLO = CREDENCIALES_DE_EJEMPLO;
#endif

// En la simulación, el ESP32 de Wokwi no alcanza el broker de la WLAN: se usa
// el broker público de prueba de Eclipse Mosquitto, sin usuario ni clave. Los
// datos de la simulación son simulados y no llevan credenciales.
#ifdef SIMULACION
constexpr const char* MQTT_URI_BROKER     = "mqtt://test.mosquitto.org:1883";
constexpr const char* MQTT_ID_CLIENTE     = "chia-01-wokwi";
constexpr const char* MQTT_USUARIO_BROKER = nullptr;
constexpr const char* MQTT_CLAVE_BROKER   = nullptr;
#else
constexpr const char* MQTT_URI_BROKER     = MQTT_BROKER_URI;
constexpr const char* MQTT_ID_CLIENTE     = "chia-01";
constexpr const char* MQTT_USUARIO_BROKER = MQTT_USUARIO;
constexpr const char* MQTT_CLAVE_BROKER   = MQTT_CLAVE;
#endif

constexpr const char* TABLERO_USUARIO_DIGEST = TABLERO_USUARIO;
constexpr const char* TABLERO_CLAVE_DIGEST   = TABLERO_CONTRASENA;

/** Tokens de los dispositivos autorizados. */
constexpr const char* TOKENS_AUTORIZADOS[] = TABLERO_TOKENS;
constexpr size_t      NUM_TOKENS_AUTORIZADOS = sizeof(TOKENS_AUTORIZADOS) / sizeof(TOKENS_AUTORIZADOS[0]);
