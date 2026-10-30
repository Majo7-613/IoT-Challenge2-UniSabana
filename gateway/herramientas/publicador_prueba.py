#!/usr/bin/env python3
"""
Publicador de prueba que imita al nodo ESP32 (Challenge #3).

Publica con los mismos tópicos, mensajes, QoS y última voluntad que el
firmware (docs/diseno/comunicacion-mqtt.md), para desarrollar y probar el
gateway sin el ESP32. Todos los mensajes llevan "origen": "simulado".

Escenarios:
  normal    nivel estable al 73 %, sin alertas.
  descenso  el nivel baja hasta el umbral crítico y genera las alertas
            NORMAL → ADVERTENCIA → CRITICO (umbrales de demostración del firmware).

Además responde a la orden de desactivar la alarma con las reglas del
firmware: se acepta solo en ALERTA o CRITICO.

Ejemplos, desde Repo/gateway:
  .venv/Scripts/python herramientas/publicador_prueba.py --mensajes 3
  .venv/Scripts/python herramientas/publicador_prueba.py --escenario descenso --periodo 1
  .venv/Scripts/python herramientas/publicador_prueba.py --cortar   # sale sin desconectarse: prueba la última voluntad

Usuario y clave del broker: opciones --usuario y --clave, o variables de
entorno MQTT_USUARIO y MQTT_CLAVE (nunca en el código).
"""

import argparse
import json
import os
import sys
import threading
import time

import paho.mqtt.client as mqtt

RAIZ, MUNICIPIO, NODO = "sabana", "chia", "chia-01"

# Umbrales de demostración del firmware (config.h): % de la altura útil.
UMBRAL_PREVENTIVO_PCT = 40
UMBRAL_CRITICO_PCT = 20


def topico(tipo):
    """Arma sabana/{municipio}/{nodo}/{tipo}."""
    return f"{RAIZ}/{MUNICIPIO}/{NODO}/{tipo}"


def estado_por_nivel(nivel_pct):
    """Clasificación simplificada por nivel (sin histéresis), suficiente para la prueba."""
    if nivel_pct <= UMBRAL_CRITICO_PCT:
        return "CRITICO", ["nivel_critico"]
    if nivel_pct <= UMBRAL_PREVENTIVO_PCT:
        return "ADVERTENCIA", ["nivel_preventivo"]
    return "NORMAL", []


class NodoSimulado:
    """Estado del nodo simulado y armado de sus mensajes."""

    def __init__(self, escenario):
        self.escenario = escenario
        self.msg_id = 0
        self.ciclo = 0
        self.segundos = 0.0
        self.nivel_pct = 73.0
        self.estado, self.causas = estado_por_nivel(self.nivel_pct)
        self.alarma_desactivada = False
        self.candado = threading.Lock()

    def siguiente_id(self):
        self.msg_id += 1
        return self.msg_id

    def avanzar(self, segundos):
        """Avanza el tiempo simulado; devuelve el estado anterior si cambió."""
        self.segundos += segundos
        self.ciclo = int(self.segundos)  # ciclos de 1 s del firmware
        if self.escenario == "descenso":
            self.nivel_pct = max(5.0, self.nivel_pct - 3.0)
        nuevo, causas = estado_por_nivel(self.nivel_pct)
        anterior = None
        if nuevo != self.estado:
            anterior = self.estado
            # Rearme: si el estado empeora, la alarma vuelve a sonar.
            self.alarma_desactivada = False
        self.estado, self.causas = nuevo, causas
        return anterior

    def telemetria(self):
        nivel_cm = round(self.nivel_pct * 0.30, 1)  # altura útil de demostración: 30 cm
        return {
            "id": self.siguiente_id(),
            "ciclo": self.ciclo,
            "epoca": int(time.time()),
            "estado": self.estado,
            "causas": self.causas,
            "alarma_desactivada": self.alarma_desactivada,
            "nivel_cm": nivel_cm,
            "nivel_pct": round(self.nivel_pct),
            "tend_cm_min": -0.9 if self.escenario == "descenso" else -0.01,
            "t_c": 18.4,
            "hr_pct": 71,
            "p_hpa": 752.4,
            "uv_idx": 1.2,
            "vpd_kpa": 0.61,
            "et0_mm_dia": None,
            "sensores": {"nivel": "OK", "dht22": "OK", "bmp180": "OK", "guva": "OK"},
            "rssi": -61,
            "origen": "simulado",
        }

    def alerta(self, anterior):
        return {
            "id": self.siguiente_id(),
            "ciclo": self.ciclo,
            "epoca": int(time.time()),
            "anterior": anterior,
            "estado": self.estado,
            "causas": self.causas,
            "nivel_pct": round(self.nivel_pct),
            "tend_cm_min": -0.9,
            "origen": "simulado",
        }

    def atender_orden(self, orden):
        """Aplica la orden de desactivar con las reglas del firmware."""
        with self.candado:
            respuesta = {"orden_id": orden.get("orden_id"), "estado": self.estado, "epoca": int(time.time())}
            if self.estado in ("ALERTA", "CRITICO"):
                self.alarma_desactivada = True
                respuesta["resultado"] = "aceptada"
            else:
                respuesta["resultado"] = "rechazada"
                respuesta["motivo"] = f"estado {self.estado}"
            return respuesta


def main():
    p = argparse.ArgumentParser(description="Publicador de prueba que imita al nodo ESP32.")
    p.add_argument("--host", default="localhost")
    p.add_argument("--puerto", type=int, default=1883)
    p.add_argument("--usuario", default=os.environ.get("MQTT_USUARIO"))
    p.add_argument("--clave", default=os.environ.get("MQTT_CLAVE"))
    p.add_argument("--escenario", choices=["normal", "descenso"], default="normal")
    p.add_argument("--periodo", type=float, default=5.0, help="segundos entre mensajes de telemetría (firmware: 5)")
    p.add_argument("--mensajes", type=int, default=0, help="número de mensajes de telemetría (0 = sin límite)")
    p.add_argument("--keepalive", type=int, default=30, help="segundos (firmware: 30)")
    p.add_argument("--cortar", action="store_true",
                   help="terminar sin desconectarse, para que el broker publique la última voluntad")
    a = p.parse_args()

    nodo = NodoSimulado(a.escenario)
    cliente = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"{NODO}-sim")
    if a.usuario:
        cliente.username_pw_set(a.usuario, a.clave)
    cliente.will_set(topico("estado"), "offline", qos=1, retain=True)

    def al_conectar(c, _u, _f, codigo, _p):
        if codigo.is_failure:
            print(f"conexión rechazada: {codigo}", file=sys.stderr)
            return
        print(f"conectado a {a.host}:{a.puerto}")
        c.publish(topico("estado"), "online", qos=1, retain=True)
        c.subscribe(topico("cmd/desactivar_alarma"), qos=1)

    def al_mensaje(c, _u, msg):
        try:
            orden = json.loads(msg.payload)
        except json.JSONDecodeError:
            print(f"orden ignorada (JSON inválido): {msg.payload!r}", file=sys.stderr)
            return
        respuesta = nodo.atender_orden(orden)
        c.publish(topico("cmd/respuesta"), json.dumps(respuesta), qos=1)
        print(f"orden {respuesta['orden_id']}: {respuesta['resultado']}")

    cliente.on_connect = al_conectar
    cliente.on_message = al_mensaje
    cliente.connect(a.host, a.puerto, keepalive=a.keepalive)
    cliente.loop_start()

    enviados = 0
    try:
        while a.mensajes == 0 or enviados < a.mensajes:
            time.sleep(a.periodo)
            with nodo.candado:
                anterior = nodo.avanzar(a.periodo)
                if anterior is not None:
                    cliente.publish(topico("alerta"), json.dumps(nodo.alerta(anterior)), qos=1)
                    print(f"alerta: {anterior} -> {nodo.estado}")
                cliente.publish(topico("telemetria"), json.dumps(nodo.telemetria()), qos=0)
            enviados += 1
    except KeyboardInterrupt:
        pass

    if a.cortar:
        # Sale sin DISCONNECT: el broker publica "offline" tras 1.5 × keepalive.
        print("saliendo sin desconectarse (prueba de la última voluntad)", flush=True)
        os._exit(0)
    cliente.publish(topico("estado"), "offline", qos=1, retain=True).wait_for_publish(5)
    cliente.loop_stop()
    cliente.disconnect()


if __name__ == "__main__":
    main()
