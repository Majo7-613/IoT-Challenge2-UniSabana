/**
 * @file red.cpp
 * @brief Implementación del Wi-Fi en modo estación, la reconexión y el NTP.
 */
#include "red.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "credenciales.h"
#include "registro.h"
#include "tareas.h"
#include "web.h"

namespace {

bool     conectadaAntes = false;     ///< Estado en la supervisión anterior.
bool     servidorIniciado = false;
bool     ntpConfigurado = false;
bool     fechaInformada = false;
int      ultimoDiaInformado = 0;
uint32_t desconexionMs = 0;          ///< Instante en que se detectó la pérdida de la WLAN.
uint32_t ultimoReintentoMs = 0;

/** Si la hora del sistema es válida, informa el día del año a la tarea de fusión. */
void actualizarFecha() {
    struct tm ahora {};
    // Espera 0 ms: solo consulta la hora actual, sin bloquear la tarea.
    if (!getLocalTime(&ahora, 0) || ahora.tm_year + 1900 < HORA_VALIDA_DESDE_ANIO) {
        return;
    }
    const int dia = ahora.tm_yday + 1;
    if (!fechaInformada || dia != ultimoDiaInformado) {
        tareasFijarDiaDelAnio(dia);
        ultimoDiaInformado = dia;
        if (!fechaInformada) {
            registrar("red", "hora válida por NTP: %04d-%02d-%02d %02d:%02d (día del año %d)",
                      ahora.tm_year + 1900, ahora.tm_mon + 1, ahora.tm_mday,
                      ahora.tm_hour, ahora.tm_min, dia);
        }
        fechaInformada = true;
    }
}

}  // namespace

void redIniciar() {
    if (CREDENCIALES_DE_EJEMPLO) {
        registrar("red", "AVISO: no existe secrets.h; se usan las credenciales de ejemplo");
    }
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // La reconexión la controla redSupervisar().
    WiFi.begin(RED_SSID, RED_CLAVE);
    registrar("red", "asociándose a la WLAN \"%s\"", RED_SSID);
}

bool redConectada() {
    return WiFi.status() == WL_CONNECTED;
}

bool redHoraEpoca(uint32_t& epoca) {
    const time_t ahora = time(nullptr);
    struct tm utc {};
    gmtime_r(&ahora, &utc);
    if (utc.tm_year + 1900 < HORA_VALIDA_DESDE_ANIO) {
        return false;
    }
    epoca = static_cast<uint32_t>(ahora);
    return true;
}

void redSupervisar(uint32_t ahoraMs) {
    const bool conectada = redConectada();

    if (conectada && !conectadaAntes) {
        if (desconexionMs != 0) {
            registrar("red", "WLAN recuperada en %.1f s; IP %s",
                      (ahoraMs - desconexionMs) / 1000.0, WiFi.localIP().toString().c_str());
        } else {
            registrar("red", "conectada a la WLAN; IP %s, máscara %s, RSSI %d dBm",
                      WiFi.localIP().toString().c_str(), WiFi.subnetMask().toString().c_str(),
                      static_cast<int>(WiFi.RSSI()));
        }
        desconexionMs = 0;
        if (!ntpConfigurado) {
            configTime(ZONA_HORARIA_S, 0, NTP_SERVIDOR_1, NTP_SERVIDOR_2);
            ntpConfigurado = true;
        }
        if (!servidorIniciado) {
            servidorIniciado = webIniciar();
        }
    } else if (!conectada && conectadaAntes) {
        desconexionMs = ahoraMs;
        ultimoReintentoMs = ahoraMs;
        registrar("red", "WLAN perdida; la medición y la alarma in situ siguen funcionando");
    }

    if (!conectada && ahoraMs - ultimoReintentoMs >= RED_REINTENTO_MS) {
        ultimoReintentoMs = ahoraMs;
        WiFi.disconnect();
        WiFi.begin(RED_SSID, RED_CLAVE);
    }

    if (conectada && ntpConfigurado) {
        actualizarFecha();
    }
    conectadaAntes = conectada;
}
