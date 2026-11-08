/**
 * @file mensajes_mqtt.cpp
 * @brief Formato JSON de los mensajes MQTT del nodo (Challenge #3).
 */
#include "mensajes_mqtt.h"

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace mensajes_mqtt {

namespace {

/** Escritor de texto en un búfer fijo; recuerda si algo no cupo. */
class Escritor {
public:
    Escritor(char* bufer, size_t tamano) : b_(bufer), t_(tamano) {
        if (t_ > 0) {
            b_[0] = '\0';
        }
    }

    /** Agrega texto con formato printf. */
    void agregar(const char* formato, ...) {
        if (desbordado_ || t_ == 0) {
            desbordado_ = true;
            return;
        }
        va_list args;
        va_start(args, formato);
        const int n = vsnprintf(b_ + usado_, t_ - usado_, formato, args);
        va_end(args);
        if (n < 0 || static_cast<size_t>(n) >= t_ - usado_) {
            desbordado_ = true;
            return;
        }
        usado_ += static_cast<size_t>(n);
    }

    /** "clave":número con decimales, o "clave":null si el valor es NaN. */
    void numero(const char* clave, float valor, int decimales) {
        if (std::isnan(valor)) {
            agregar("\"%s\":null", clave);
        } else {
            agregar("\"%s\":%.*f", clave, decimales, static_cast<double>(valor));
        }
    }

    /** Arreglo JSON con los nombres de las causas activas. */
    void causas(uint8_t bits) {
        static const struct { uint8_t bit; const char* nombre; } TABLA[] = {
            {CAUSA_NIVEL_CRITICO, "nivel_critico"}, {CAUSA_NIVEL_PREVENTIVO, "nivel_preventivo"},
            {CAUSA_DESCENSO, "descenso"},           {CAUSA_VPD_ALTO, "vpd_alto"},
            {CAUSA_T_ALTA, "temperatura_alta"},     {CAUSA_UV_ALTO, "uv_alto"}};
        agregar("\"causas\":[");
        bool primera = true;
        for (const auto& c : TABLA) {
            if (bits & c.bit) {
                agregar("%s\"%s\"", primera ? "" : ",", c.nombre);
                primera = false;
            }
        }
        agregar("]");
    }

    /** "epoca":n, o "epoca":null si la hora no es válida. */
    void epoca(bool valida, uint32_t segundos) {
        if (valida) {
            agregar("\"epoca\":%lu", static_cast<unsigned long>(segundos));
        } else {
            agregar("\"epoca\":null");
        }
    }

    /** Longitud escrita, o 0 si algo no cupo. */
    size_t resultado() const { return desbordado_ ? 0 : usado_; }

private:
    char*  b_;
    size_t t_;
    size_t usado_ = 0;
    bool   desbordado_ = false;
};

/** true si el carácter puede formar parte de un identificador de orden. */
bool caracterDeId(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.';
}

}  // namespace

bool leerOrden(const char* datos, size_t n, char* ordenId) {
    static const char CLAVE[] = "\"orden_id\"";
    const size_t largoClave = sizeof(CLAVE) - 1;
    for (size_t i = 0; i + largoClave <= n; ++i) {
        if (std::memcmp(datos + i, CLAVE, largoClave) != 0) {
            continue;
        }
        size_t k = i + largoClave;
        while (k < n && (datos[k] == ' ' || datos[k] == '\t')) {
            ++k;
        }
        if (k >= n || datos[k] != ':') {
            return false;
        }
        ++k;
        while (k < n && (datos[k] == ' ' || datos[k] == '\t')) {
            ++k;
        }
        if (k >= n || datos[k] != '"') {
            return false;
        }
        ++k;
        size_t largo = 0;
        while (k + largo < n && datos[k + largo] != '"') {
            if (!caracterDeId(datos[k + largo]) || largo >= MAX_ORDEN_ID) {
                return false;
            }
            ++largo;
        }
        if (k + largo >= n || largo == 0) {
            return false;  // sin comilla de cierre o identificador vacío
        }
        std::memcpy(ordenId, datos + k, largo);
        ordenId[largo] = '\0';
        return true;
    }
    return false;
}

size_t respuesta(const Respuesta& r, char* bufer, size_t tamano) {
    Escritor j(bufer, tamano);
    j.agregar("{\"orden_id\":\"%s\",\"resultado\":\"%s\",", r.ordenId,
              r.aceptada ? "aceptada" : "rechazada");
    if (!r.aceptada && r.motivo != nullptr) {
        j.agregar("\"motivo\":\"%s\",", r.motivo);
    }
    j.agregar("\"estado\":\"%s\",", textoAlerta(r.estado));
    j.epoca(r.epocaValida, r.epoca);
    j.agregar("}");
    return j.resultado();
}

size_t telemetria(const Telemetria& t, char* bufer, size_t tamano) {
    Escritor j(bufer, tamano);
    j.agregar("{\"id\":%lu,\"ciclo\":%lu,", static_cast<unsigned long>(t.id),
              static_cast<unsigned long>(t.ciclo));
    j.epoca(t.epocaValida, t.epoca);
    j.agregar(",\"estado\":\"%s\",", textoAlerta(t.estado));
    j.causas(t.causas);
    j.agregar(",\"alarma_desactivada\":%s,", t.alarmaDesactivada ? "true" : "false");
    j.numero("nivel_cm", t.nivelCm, 1);
    j.agregar(",");
    j.numero("nivel_pct", t.nivelPct, 0);
    j.agregar(",");
    j.numero("tend_cm_min", t.tendCmMin, 2);
    j.agregar(",");
    j.numero("t_c", t.tC, 1);
    j.agregar(",");
    j.numero("hr_pct", t.hrPct, 0);
    j.agregar(",");
    j.numero("p_hpa", t.pHpa, 1);
    j.agregar(",");
    j.numero("uv_idx", t.uvIdx, 1);
    j.agregar(",");
    j.numero("vpd_kpa", t.vpdKpa, 2);
    j.agregar(",");
    j.numero("et0_mm_dia", t.et0MmDia, 1);
    j.agregar(",\"sensores\":{\"nivel\":\"%s\",\"dht22\":\"%s\",\"bmp180\":\"%s\",\"guva\":\"%s\"},",
              t.sensorNivel, t.sensorDht, t.sensorBmp, t.sensorUv);
    j.agregar("\"rssi\":%d}", t.rssi);
    return j.resultado();
}

size_t alerta(const Alerta& a, char* bufer, size_t tamano) {
    Escritor j(bufer, tamano);
    j.agregar("{\"id\":%lu,\"ciclo\":%lu,", static_cast<unsigned long>(a.id),
              static_cast<unsigned long>(a.ciclo));
    j.epoca(a.epocaValida, a.epoca);
    j.agregar(",\"anterior\":\"%s\",\"estado\":\"%s\",", textoAlerta(a.anterior), textoAlerta(a.estado));
    j.causas(a.causas);
    j.agregar(",");
    j.numero("nivel_pct", a.nivelPct, 0);
    j.agregar(",");
    j.numero("tend_cm_min", a.tendCmMin, 2);
    j.agregar("}");
    return j.resultado();
}

}  // namespace mensajes_mqtt
