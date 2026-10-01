/**
 * @file historico.cpp
 * @brief Implementación de los búferes circulares del histórico.
 */
#include "historico.h"

#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"

namespace {

/** Búfer circular de capacidad fija. */
template <size_t N>
struct Circular {
    RegistroHistorico registros[N];
    size_t siguiente = 0;
    size_t ocupados = 0;

    void agregar(const RegistroHistorico& r) {
        registros[siguiente] = r;
        siguiente = (siguiente + 1) % N;
        if (ocupados < N) {
            ocupados++;
        }
    }

    size_t copiar(RegistroHistorico* destino) const {
        // El más antiguo está en `siguiente` cuando el búfer está lleno, y en 0 si no.
        const size_t inicio = (ocupados == N) ? siguiente : 0;
        for (size_t i = 0; i < ocupados; i++) {
            destino[i] = registros[(inicio + i) % N];
        }
        return ocupados;
    }
};

Circular<HISTORICO_RAPIDO_REGISTROS> rapido;
Circular<HISTORICO_LENTO_REGISTROS>  lento;
SemaphoreHandle_t mtxHistorico = nullptr;

/** Valor si el sensor está en OK; NAN si no, para que el tablero muestre un hueco. */
float siOk(EstadoSensor estado, float valor) {
    return estado == EstadoSensor::OK ? valor : NAN;
}

}  // namespace

bool historicoIniciar() {
    if (mtxHistorico == nullptr) {
        mtxHistorico = xSemaphoreCreateMutex();
    }
    return mtxHistorico != nullptr;
}

void historicoAgregar(Bufer bufer, const EstadoPublicado& estado) {
    const Lecturas&  l = estado.instantanea.lecturas;
    const Derivados& d = estado.derivados;

    RegistroHistorico r{};
    r.tiempoS = static_cast<uint32_t>(estado.instantanea.marcaTiempoUs / 1000000);
    r.nivelCm = siOk(l.estadoNivel, d.nivelCm);
    r.nivelPct = siOk(l.estadoNivel, d.nivelPct);
    r.temperaturaC = siOk(l.estadoDht, l.temperaturaDhtC);
    r.humedadPct = siOk(l.estadoDht, l.humedadPct);
    r.presionHpa = siOk(l.estadoBmp, l.presionHpa);
    r.uvIndice = siOk(l.estadoUv, l.uvIndice);
    r.vpdKpa = d.vpdValido ? d.vpdKpa : NAN;
    r.estado = d.estado;

    xSemaphoreTake(mtxHistorico, portMAX_DELAY);
    if (bufer == Bufer::RAPIDO) {
        rapido.agregar(r);
    } else {
        lento.agregar(r);
    }
    xSemaphoreGive(mtxHistorico);
}

size_t historicoCopiar(Bufer bufer, RegistroHistorico* destino) {
    xSemaphoreTake(mtxHistorico, portMAX_DELAY);
    const size_t n = (bufer == Bufer::RAPIDO) ? rapido.copiar(destino) : lento.copiar(destino);
    xSemaphoreGive(mtxHistorico);
    return n;
}

size_t historicoCapacidad(Bufer bufer) {
    return bufer == Bufer::RAPIDO ? HISTORICO_RAPIDO_REGISTROS : HISTORICO_LENTO_REGISTROS;
}

uint32_t historicoPeriodoS(Bufer bufer) {
    return (bufer == Bufer::RAPIDO ? HISTORICO_RAPIDO_PERIODO_MS : HISTORICO_LENTO_PERIODO_MS) / 1000;
}
