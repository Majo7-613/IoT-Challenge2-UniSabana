"""Orden de desactivar la alarma física: de Ubidots al nodo, con registro en SQLite.

Flujo (docs/diseno/comunicacion-mqtt.md): un usuario autorizado activa el control
desactivar_alarma en el tablero de Ubidots → el gateway crea una orden con un identificador
único, la registra y la publica en sabana/{municipio}/{nodo}/cmd/desactivar_alarma → el nodo
responde en .../cmd/respuesta (registro.py la guarda) → el gateway informa el resultado a
Ubidots y devuelve el control a 0.
"""

import json
import logging
import time

from . import db
from .validacion import es_numero

log = logging.getLogger("gateway.ordenes")


def valor_de_control(carga):
    """Valor numérico de un mensaje de Ubidots: un número suelto o {"value": n}. None si no es válido."""
    texto = carga.decode("utf-8", errors="replace") if isinstance(carga, bytes) else str(carga)
    try:
        dato = json.loads(texto)
    except json.JSONDecodeError:
        return None
    if isinstance(dato, dict):
        dato = dato.get("value")
    return float(dato) if es_numero(dato) else None


def nuevo_id(origen="u", ahora=None):
    """Identificador de orden: u-AAAAMMDD-HHMMSS-mmm (UTC). Cumple las reglas del firmware."""
    ahora = time.time() if ahora is None else ahora
    ms = int((ahora % 1) * 1000)
    return f"{origen}-{time.strftime('%Y%m%d-%H%M%S', time.gmtime(ahora))}-{ms:03d}"


class GestorOrdenes:
    """Crea, envía y cierra las órdenes de desactivar la alarma."""

    def __init__(self, con, cfg, publicar_local, encolar_nube, reloj=db.ahora_utc):
        self.con = con
        self.cfg = cfg
        self.publicar_local = publicar_local    # (topico, texto, qos) → None
        self.encolar_nube = encolar_nube        # (dispositivo, carga) → None
        self.reloj = reloj

    def desde_ubidots(self, carga):
        """Atiende un cambio del control desactivar_alarma; solo un valor ≥ 1 es una orden."""
        valor = valor_de_control(carga)
        if valor is None or valor < 1:
            return None
        return self.emitir("ubidots")

    def emitir(self, origen):
        """Registra una orden nueva y la publica al nodo. Devuelve el identificador."""
        orden_id = nuevo_id("u" if origen == "ubidots" else "g")
        ahora = self.reloj()
        self.con.execute(
            "INSERT INTO ordenes (orden_id, nodo, origen, recibida_utc, enviada_utc) VALUES (?, ?, ?, ?, ?)",
            (orden_id, self.cfg.nodo, origen, ahora, ahora))
        self.con.commit()
        mensaje = {"orden_id": orden_id, "origen": origen, "epoca": int(time.time())}
        self.publicar_local(self.cfg.topico_nodo("cmd/desactivar_alarma"), json.dumps(mensaje), 1)
        # El control del tablero vuelve a 0: la siguiente activación es una orden nueva.
        self.encolar_nube(self.cfg.ubidots_dispositivo_gateway, {"desactivar_alarma": {"value": 0}})
        log.info("orden %s enviada al nodo %s", orden_id, self.cfg.nodo)
        return orden_id

    def al_responder(self, respuesta):
        """Informa a Ubidots el resultado de una orden (1 = aceptada, 0 = rechazada)."""
        contexto = {"orden_id": respuesta["orden_id"], "estado": respuesta.get("estado_nodo") or ""}
        if respuesta.get("motivo"):
            contexto["motivo"] = respuesta["motivo"]
        valor = 1 if respuesta["resultado"] == "aceptada" else 0
        self.encolar_nube(self.cfg.ubidots_dispositivo_gateway,
                          {"orden_resultado": {"value": valor, "context": contexto}})
        log.info("orden %s %s", respuesta["orden_id"], respuesta["resultado"])
