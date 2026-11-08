"""Envío a Ubidots por MQTT con TLS, a través de una cola persistente en SQLite.

Todo lo que va a la nube pasa por la tabla cola_nube: si Internet o Ubidots no están
disponibles, los mensajes esperan y se envían al volver, en orden, sin perder ninguno.

El formato de las cargas sigue la documentación de Ubidots ({"variable": {"value": v,
"timestamp": ms, "context": {...}}}). El servidor, el puerto y los tópicos son configurables
(.env) y se confirman al crear la cuenta (docs/diseno/comunicacion-mqtt.md, sección 4).
"""

import json
import logging
import ssl
import threading

import paho.mqtt.client as mqtt

from . import db
from .procesamiento import GRAVEDAD

log = logging.getLogger("gateway.nube")

# Variables del dispositivo del nodo en Ubidots (máximo 10 por dispositivo en la cuenta STEM).
VARIABLES_NODO = {
    "nivel_pct": "nivel_pct", "nivel_cm": "nivel_cm", "tend_cm_min": "tendencia",
    "t_c": "temperatura", "hr_pct": "humedad", "p_hpa": "presion", "uv_idx": "uv", "vpd_kpa": "vpd",
}


def milisegundos(texto_utc):
    """Texto ISO 8601 en UTC → milisegundos de época (formato de timestamp de Ubidots)."""
    return int(db.de_texto(texto_utc).timestamp() * 1000)


def carga_agregado(agregado):
    """Carga para Ubidots con los promedios de un periodo (timestamp = inicio del periodo)."""
    ts = milisegundos(agregado["inicio_utc"])
    carga = {destino: {"value": round(agregado[origen], 3), "timestamp": ts}
             for origen, destino in VARIABLES_NODO.items() if agregado.get(origen) is not None}
    estado = agregado.get("estado_peor")
    if estado in GRAVEDAD:
        carga["estado"] = {"value": GRAVEDAD[estado], "timestamp": ts, "context": {"texto": estado}}
    return carga


def carga_estado(alerta):
    """Carga inmediata de un cambio de estado de alerta (código de gravedad y texto en el contexto)."""
    ts = milisegundos(alerta["recibido_utc"])
    contexto = {"texto": alerta["estado"], "anterior": alerta.get("anterior") or ""}
    return {"estado": {"value": GRAVEDAD.get(alerta["estado"], -1), "timestamp": ts, "context": contexto}}


def encolar(con, dispositivo, carga, reloj=db.ahora_utc):
    """Agrega una carga a la cola de envío. Devuelve el id de la fila."""
    cursor = con.execute(
        "INSERT INTO cola_nube (creado_utc, dispositivo, carga) VALUES (?, ?, ?)",
        (reloj(), dispositivo, json.dumps(carga, ensure_ascii=False)))
    con.commit()
    return cursor.lastrowid


def siguiente_pendiente(con):
    """La carga pendiente más antigua, o None."""
    return con.execute("SELECT * FROM cola_nube WHERE enviado_utc IS NULL ORDER BY id LIMIT 1").fetchone()


def pendientes(con):
    """Número de cargas sin enviar."""
    return con.execute("SELECT COUNT(*) FROM cola_nube WHERE enviado_utc IS NULL").fetchone()[0]


def marcar_resultado(con, fila_id, enviado, reloj=db.ahora_utc):
    """Marca una carga como enviada, o suma un intento fallido."""
    if enviado:
        con.execute("UPDATE cola_nube SET enviado_utc = ?, intentos = intentos + 1 WHERE id = ?", (reloj(), fila_id))
    else:
        con.execute("UPDATE cola_nube SET intentos = intentos + 1 WHERE id = ?", (fila_id,))
    con.commit()


def enviar_siguiente(con, publicar):
    """Envía la carga pendiente más antigua con la función publicar(dispositivo, texto) → bool.

    Se llama a lo sumo una vez por segundo (límite de 1 petición por segundo de la cuenta STEM).
    Devuelve True si envió algo.
    """
    fila = siguiente_pendiente(con)
    if fila is None:
        return False
    enviado = publicar(fila["dispositivo"], fila["carga"])
    marcar_resultado(con, fila["id"], enviado)
    if not enviado:
        log.warning("envío a Ubidots fallido (cola: %d pendientes)", pendientes(con))
    return enviado


class ClienteUbidots:
    """Sesión MQTT con Ubidots: publica cargas y recibe la orden de desactivar la alarma."""

    def __init__(self, cfg, al_recibir_orden):
        self.cfg = cfg
        self.al_recibir_orden = al_recibir_orden
        self.conectado = threading.Event()
        self.cliente = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"{cfg.ubidots_dispositivo_gateway}-gw")
        # Ubidots autentica con el token de la cuenta como usuario (por confirmar al crear la cuenta).
        self.cliente.username_pw_set(cfg.ubidots_token, "")
        if cfg.ubidots_tls:
            self.cliente.tls_set(cert_reqs=ssl.CERT_REQUIRED)
        self.cliente.on_connect = self._al_conectar
        self.cliente.on_disconnect = self._al_desconectar
        self.cliente.on_message = self._al_mensaje
        self.topico_orden = cfg.ubidots_topico_orden.format(dispositivo=cfg.ubidots_dispositivo_gateway)

    def iniciar(self):
        self.cliente.connect_async(self.cfg.ubidots_host, self.cfg.ubidots_puerto, keepalive=60)
        self.cliente.loop_start()

    def detener(self):
        self.cliente.loop_stop()
        self.cliente.disconnect()

    def _al_conectar(self, cliente, _usuario, _banderas, codigo, _propiedades):
        if codigo.is_failure:
            log.error("Ubidots rechazó la conexión: %s", codigo)
            return
        log.info("conectado a Ubidots (%s:%s)", self.cfg.ubidots_host, self.cfg.ubidots_puerto)
        self.conectado.set()
        cliente.subscribe(self.topico_orden, qos=1)

    def _al_desconectar(self, _cliente, _usuario, _banderas, codigo, _propiedades):
        self.conectado.clear()
        log.warning("desconectado de Ubidots (%s); paho reintenta solo", codigo)

    def _al_mensaje(self, _cliente, _usuario, mensaje):
        if mensaje.topic == self.topico_orden:
            self.al_recibir_orden(mensaje.payload)

    def publicar(self, dispositivo, texto):
        """Publica una carga en el dispositivo de Ubidots. Devuelve True si el broker la confirmó."""
        if not self.conectado.is_set():
            return False
        topico = self.cfg.ubidots_topico_publicar.format(dispositivo=dispositivo)
        info = self.cliente.publish(topico, texto, qos=1)
        try:
            info.wait_for_publish(timeout=5)
        except RuntimeError:
            return False
        return info.is_published()
