# -*- coding: utf-8 -*-
"""
Cliente de la plataforma para Cisco Packet Tracer (SBC-PT, 200.10.10.20).

Se suscribe a telemetría, alertas y estado de todos los nodos de la región,
lleva la cuenta de mensajes por nodo y, cuando un nodo entra en CRÍTICO,
publica la orden de desactivar su alarma física para demostrar el canal de
órdenes (en la operación real, la orden la da un usuario autorizado).
"""

# ---------------------------------------------------------------------------
# Configuración
# ---------------------------------------------------------------------------
BROKER = "200.10.10.10"
USUARIO = "plataforma"
CLAVE = "clave-plataforma"
SUSCRIPCIONES = [
    ("sabana/+/+/telemetria", 0),
    ("sabana/+/+/alerta", 1),
    ("sabana/+/+/estado", 1),
]


# ===========================================================================
# ADAPTAR: API MQTT de Packet Tracer.
# ===========================================================================
def mqtt_conectar(broker, usuario, clave, id_cliente):
    raise NotImplementedError("Adaptar mqtt_conectar a la API MQTT de Packet Tracer")


def mqtt_publicar(topico, mensaje, qos, retenido):
    raise NotImplementedError("Adaptar mqtt_publicar a la API MQTT de Packet Tracer")


def mqtt_suscribir(topico, qos):
    raise NotImplementedError("Adaptar mqtt_suscribir a la API MQTT de Packet Tracer")


def esperar_ms(ms):
    raise NotImplementedError("Adaptar esperar_ms a la función de espera de Packet Tracer")
# ===========================================================================

contadores = {}


def partes(topico):
    """sabana/{municipio}/{nodo}/{tipo} -> (municipio, nodo, tipo)."""
    p = topico.split("/")
    if len(p) < 4 or p[0] != "sabana":
        return None
    return p[1], p[2], "/".join(p[3:])


def al_recibir(topico, mensaje):
    """Conectar con la retrollamada de mensajes de la API de Packet Tracer."""
    datos = partes(topico)
    if datos is None:
        return
    municipio, nodo, tipo = datos
    contadores[nodo] = contadores.get(nodo, 0) + 1
    print("[%s/%s] %s (%d): %s" % (municipio, nodo, tipo, contadores[nodo], mensaje))

    if tipo == "estado" and mensaje == "offline":
        print("AVISO: el nodo " + nodo + " se desconectó (LWT)")
    if tipo == "alerta" and '"CRITICO"' in mensaje:
        orden = "sabana/" + municipio + "/" + nodo + "/cmd/desactivar_alarma"
        mqtt_publicar(orden, '{"orden":"desactivar","origen":"plataforma"}', 1, False)
        print("Orden de desactivar la alarma enviada a " + nodo)


def main():
    mqtt_conectar(BROKER, USUARIO, CLAVE, "plataforma")
    for topico, qos in SUSCRIPCIONES:
        mqtt_suscribir(topico, qos)
    print("Plataforma suscrita a " + str(len(SUSCRIPCIONES)) + " tópicos")
    while True:
        esperar_ms(1000)


if __name__ == "__main__":
    main()
