/**
 * @file mensajes_mqtt.h
 * @brief Formato JSON de los mensajes MQTT del nodo, sin dependencias del ESP32.
 *
 * Está separado de mqtt.cpp para poder probarlo en el computador
 * (`pio test -d firmware -e native`). Los campos siguen
 * docs/diseno/comunicacion-mqtt.md, sección 3. Un valor NaN se escribe como
 * null (sensor sin lectura válida, tendencia o VPD no disponibles, ET0 sin
 * 24 h de datos).
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "estados.h"

namespace mensajes_mqtt {

/** Datos de un mensaje de telemetría (tópico .../telemetria). */
struct Telemetria {
    uint32_t     id;                 ///< Contador de mensajes del nodo.
    uint32_t     ciclo;              ///< Ciclo de adquisición (1 s).
    bool         epocaValida;        ///< false si el NTP aún no fijó la hora.
    uint32_t     epoca;              ///< s desde 1970 (UTC).
    EstadoAlerta estado;
    uint8_t      causas;             ///< Combinación de CausaAlerta.
    bool         alarmaDesactivada;
    float        nivelCm, nivelPct, tendCmMin;
    float        tC, hrPct, pHpa, uvIdx;
    float        vpdKpa, et0MmDia;
    const char*  sensorNivel;        ///< OK, VIEJO, FALLA o INICIANDO.
    const char*  sensorDht;
    const char*  sensorBmp;
    const char*  sensorUv;
    int          rssi;               ///< dBm.
};

/** Datos de un mensaje de alerta (tópico .../alerta), en cada cambio de estado. */
struct Alerta {
    uint32_t     id;
    uint32_t     ciclo;
    bool         epocaValida;
    uint32_t     epoca;
    EstadoAlerta anterior;
    EstadoAlerta estado;
    uint8_t      causas;
    float        nivelPct;
    float        tendCmMin;
};

/** Longitud máxima del identificador de una orden (sin el terminador). */
constexpr size_t MAX_ORDEN_ID = 40;

/** Resultado de una orden de desactivar la alarma (tópico .../cmd/respuesta). */
struct Respuesta {
    const char*  ordenId;
    bool         aceptada;
    const char*  motivo;        ///< Solo si fue rechazada; nullptr si no aplica.
    EstadoAlerta estado;        ///< Estado de alerta al responder.
    bool         epocaValida;
    uint32_t     epoca;
};

/**
 * @brief Lee el campo "orden_id" de una orden recibida.
 *
 * Acepta solo identificadores de 1 a MAX_ORDEN_ID caracteres alfanuméricos,
 * guion, guion bajo o punto, para no reenviar texto arbitrario en la respuesta.
 *
 * @param datos   Carga del mensaje (no necesariamente terminada en '\0').
 * @param n       Longitud de la carga.
 * @param ordenId Destino, de al menos MAX_ORDEN_ID + 1 bytes.
 * @return true si el campo existe y es válido.
 */
bool leerOrden(const char* datos, size_t n, char* ordenId);

/**
 * @brief Escribe el JSON de la respuesta a una orden.
 * @return Longitud escrita, o 0 si no cupo en el búfer.
 */
size_t respuesta(const Respuesta& r, char* bufer, size_t tamano);

/**
 * @brief Escribe el JSON de la telemetría.
 * @return Longitud escrita, o 0 si no cupo en el búfer.
 */
size_t telemetria(const Telemetria& t, char* bufer, size_t tamano);

/**
 * @brief Escribe el JSON de una alerta.
 * @return Longitud escrita, o 0 si no cupo en el búfer.
 */
size_t alerta(const Alerta& a, char* bufer, size_t tamano);

}  // namespace mensajes_mqtt
