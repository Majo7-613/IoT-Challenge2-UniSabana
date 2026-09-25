# -*- coding: utf-8 -*-
"""
Gateway de municipio para Cisco Packet Tracer (SBC-PT).

Se suscribe a todos los tópicos del municipio en el broker local y los
republica en el broker de la nube. Requiere que la API MQTT de Packet Tracer
permita dos conexiones de cliente en el mismo script; si no lo permite, usar
la simplificación de la guía (paso 3.7): los nodos publican directamente en
la nube y el router del municipio cumple la función de gateway.
"""

# ---------------------------------------------------------------------------
# Configuración
# ---------------------------------------------------------------------------
MUNICIPIO = "chia"
BROKER_LOCAL = "192.168.10.2"   # broker en este mismo gateway (ver la guía)
BROKER_NUBE = "200.10.10.10"
USUARIO = "gateway"
CLAVE = "clave-gateway"
FILTRO_LOCAL = "sabana/" + MUNICIPIO + "/#"


# ===========================================================================
# ADAPTAR: API MQTT de Packet Tracer (dos clientes: local y nube).
# ===========================================================================
def mqtt_conectar(nombre, broker, usuario, clave, id_cliente):
    raise NotImplementedError("Adaptar mqtt_conectar a la API MQTT de Packet Tracer")


def mqtt_publicar(nombre, topico, mensaje, qos, retenido):
    raise NotImplementedError("Adaptar mqtt_publicar a la API MQTT de Packet Tracer")


def mqtt_suscribir(nombre, topico, qos):
    raise NotImplementedError("Adaptar mqtt_suscribir a la API MQTT de Packet Tracer")


def esperar_ms(ms):
    raise NotImplementedError("Adaptar esperar_ms a la función de espera de Packet Tracer")
# ===========================================================================

reenviados = {"total": 0}


def qos_de(topico):
    """Conserva la política de QoS y retención del diseño de tópicos."""
    if topico.endswith("/telemetria"):
        return 0, False
    if topico.endswith("/estado"):
        return 1, True
    return 1, False   # alertas y órdenes


def al_recibir(topico, mensaje):
    """Conectar con la retrollamada del cliente LOCAL: reenvía a la nube."""
    qos, retenido = qos_de(topico)
    mqtt_publicar("nube", topico, mensaje, qos, retenido)
    reenviados["total"] += 1
    print("Reenviado (%d): %s" % (reenviados["total"], topico))


def main():
    mqtt_conectar("local", BROKER_LOCAL, USUARIO, CLAVE, "gw-" + MUNICIPIO + "-local")
    mqtt_conectar("nube", BROKER_NUBE, USUARIO, CLAVE, "gw-" + MUNICIPIO + "-nube")
    mqtt_suscribir("local", FILTRO_LOCAL, 1)
    print("Gateway " + MUNICIPIO + ": reenviando " + FILTRO_LOCAL + " a " + BROKER_NUBE)
    while True:
        esperar_ms(1000)


if __name__ == "__main__":
    main()
