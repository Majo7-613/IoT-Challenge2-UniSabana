# -*- coding: utf-8 -*-
"""
Nodo de monitoreo hídrico para Cisco Packet Tracer (SBC-PT o MCU-PT).

Publica telemetría SIMULADA, publica una alerta en cada cambio de estado,
anuncia su estado de conexión y atiende la orden de desactivar la alarma.
Tópicos: sabana/{municipio}/{nodo}/{telemetria|alerta|estado|cmd/desactivar_alarma}
"""
import random

# ---------------------------------------------------------------------------
# Configuración del nodo (cambiar en cada dispositivo)
# ---------------------------------------------------------------------------
MUNICIPIO = "chia"
NODO = "chia-01"
BROKER = "200.10.10.10"      # broker de la nube (o el del gateway local, ver la guía)
USUARIO = "nodo"
CLAVE = "clave-nodo"
PERIODO_S = 10               # 60 s en el diseño; 10 s para la demostración

BASE = "sabana/" + MUNICIPIO + "/" + NODO
TOPICO_TELEMETRIA = BASE + "/telemetria"
TOPICO_ALERTA = BASE + "/alerta"
TOPICO_ESTADO = BASE + "/estado"
TOPICO_ORDEN = BASE + "/cmd/desactivar_alarma"


# ===========================================================================
# ADAPTAR: API MQTT de Packet Tracer.
# Reemplazar el cuerpo de estas funciones por las llamadas del proyecto de
# ejemplo "MQTT Client" de Packet Tracer. Los nombres de este bloque son del
# script, no de Packet Tracer.
# ===========================================================================
def mqtt_conectar(broker, usuario, clave, id_cliente, lwt_topico, lwt_mensaje):
    raise NotImplementedError("Adaptar mqtt_conectar a la API MQTT de Packet Tracer")


def mqtt_publicar(topico, mensaje, qos, retenido):
    raise NotImplementedError("Adaptar mqtt_publicar a la API MQTT de Packet Tracer")


def mqtt_suscribir(topico, qos):
    raise NotImplementedError("Adaptar mqtt_suscribir a la API MQTT de Packet Tracer")


def esperar_ms(ms):
    raise NotImplementedError("Adaptar esperar_ms a la función de espera de Packet Tracer")
# ===========================================================================


# ---------------------------------------------------------------------------
# Estado simulado del nodo
# ---------------------------------------------------------------------------
estado = {
    "ciclo": 0,
    "nivel_cm": 25.0,        # recipiente de demostración de 30 cm de altura útil
    "t_c": 18.0,
    "hr_pct": 70.0,
    "alerta": "NORMAL",
    "alarma_desactivada": False,
}
ALTURA_UTIL_CM = 30.0


def simular_lecturas():
    """Genera lecturas SIMULADAS: el nivel baja poco a poco y el ambiente varía."""
    estado["ciclo"] += 1
    estado["nivel_cm"] = max(0.0, estado["nivel_cm"] - random.uniform(0.0, 0.8))
    estado["t_c"] = min(35.0, max(10.0, estado["t_c"] + random.uniform(-0.5, 0.8)))
    estado["hr_pct"] = min(95.0, max(20.0, estado["hr_pct"] + random.uniform(-2.0, 1.0)))


def clasificar():
    """Clasificación simplificada para la demostración de conectividad
    (la lógica de fusión completa está en el firmware del Challenge)."""
    pct = 100.0 * estado["nivel_cm"] / ALTURA_UTIL_CM
    if pct <= 20.0:
        return "CRITICO"
    if pct <= 40.0:
        return "ADVERTENCIA"
    return "NORMAL"


def mensaje_telemetria():
    """JSON compacto construido a mano (sin el módulo json)."""
    pct = 100.0 * estado["nivel_cm"] / ALTURA_UTIL_CM
    return ('{"nodo":"%s","ciclo":%d,"nivel_cm":%.1f,"nivel_pct":%.0f,'
            '"t_c":%.1f,"hr_pct":%.0f,"estado":"%s","origen":"simulado"}'
            % (NODO, estado["ciclo"], estado["nivel_cm"], pct,
               estado["t_c"], estado["hr_pct"], estado["alerta"]))


def al_recibir(topico, mensaje):
    """Conectar esta función con la retrollamada de mensajes de la API de PT."""
    if topico == TOPICO_ORDEN:
        estado["alarma_desactivada"] = True
        print("Orden recibida: alarma física desactivada (" + mensaje + ")")


def main():
    # Última voluntad: si la conexión se cae, el broker publica "offline" (retenido).
    mqtt_conectar(BROKER, USUARIO, CLAVE, NODO, TOPICO_ESTADO, "offline")
    mqtt_publicar(TOPICO_ESTADO, "online", 1, True)
    mqtt_suscribir(TOPICO_ORDEN, 1)
    print("Nodo " + NODO + " conectado a " + BROKER)

    while True:
        simular_lecturas()
        nueva = clasificar()
        if nueva != estado["alerta"]:
            # Se rearma la alarma si el estado cambia.
            estado["alerta"] = nueva
            estado["alarma_desactivada"] = False
            mqtt_publicar(TOPICO_ALERTA, '{"estado":"%s","origen":"simulado"}' % nueva, 1, False)
            print("Alerta publicada: " + nueva)
        mqtt_publicar(TOPICO_TELEMETRIA, mensaje_telemetria(), 0, False)
        esperar_ms(PERIODO_S * 1000)


if __name__ == "__main__":
    main()
