"""Programa principal del gateway.

Une las piezas:
  - cliente MQTT local: recibe todo lo de sabana/{municipio}/+/# y lo guarda en SQLite;
  - cada periodo (5 min): promedios → cola de la nube; módulo de IA → cola de la nube;
  - cada día local: ET0 del día anterior → cola de la nube;
  - cada segundo: envía a Ubidots la carga pendiente más antigua (1 petición por segundo);
  - órdenes de desactivar la alarma desde Ubidots hasta el nodo.

Uso, desde Repo/gateway:
  .venv/Scripts/python -m gateway.principal            (Windows)
  .venv/bin/python -m gateway.principal                (Raspberry Pi)
Opciones: --env RUTA (por omisión .env), --una-vuelta (procesa y sale; para pruebas).
"""

import argparse
import logging
import threading
import time
from datetime import datetime, timedelta, timezone

import paho.mqtt.client as mqtt

from . import config, db, nube, procesamiento
from .ordenes import GestorOrdenes
from .registro import Registro

log = logging.getLogger("gateway")


class Gateway:
    """Estado y ciclo del gateway."""

    def __init__(self, cfg):
        self.cfg = cfg
        self.con = db.conectar(cfg.bd_ruta)
        self.candado = threading.Lock()   # una sola escritura a la vez en SQLite
        self.registro = Registro(self.con)
        # Sesión persistente (clean_session=False) con identificador fijo: si el gateway se reinicia, el broker
        # guarda los mensajes con QoS 1 (alertas, respuestas) y se los entrega al volver.
        self.local = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"gw-{cfg.municipio}",
                                 clean_session=False)
        if cfg.mqtt_usuario:
            self.local.username_pw_set(cfg.mqtt_usuario, cfg.mqtt_clave)
        self.local.on_connect = self._al_conectar_local
        self.local.on_message = self._al_mensaje_local
        self.ordenes = GestorOrdenes(self.con, cfg, self._publicar_local, self._encolar)
        self.ubidots = nube.ClienteUbidots(cfg, self._orden_desde_ubidots) if cfg.nube_activa else None
        self.modulo_ia = self._cargar_ia()
        ahora = datetime.now(timezone.utc)
        self.periodo_actual = procesamiento.inicio_periodo(ahora, cfg.periodo_agregado_s)
        self.fecha_actual = procesamiento.fecha_local(ahora)

    # ------------------------------------------------------------------ utilidades
    def _cargar_ia(self):
        try:
            from . import ia
        except ImportError as error:
            log.warning("módulo de IA no disponible: %s", error)
            return None
        return ia

    def _encolar(self, dispositivo, carga):
        nube.encolar(self.con, dispositivo, carga)

    def _publicar_local(self, topico, texto, qos):
        self.local.publish(topico, texto, qos=qos)

    # ------------------------------------------------------------------ MQTT local
    def _al_conectar_local(self, cliente, _u, _f, codigo, _p):
        if codigo.is_failure:
            log.error("el broker local rechazó la conexión: %s", codigo)
            return
        cliente.subscribe(self.cfg.suscripcion_local, qos=1)
        log.info("conectado al broker local %s:%s; suscrito a %s",
                 self.cfg.mqtt_host, self.cfg.mqtt_puerto, self.cfg.suscripcion_local)

    def _al_mensaje_local(self, _cliente, _u, mensaje):
        with self.candado:
            tipo, datos = self.registro.procesar(mensaje.topic, mensaje.payload)
            if tipo == "alerta" and not datos["duplicada"]:
                log.info("alerta de %s: %s -> %s", datos["nodo"], datos["anterior"], datos["estado"])
                self._encolar(self.cfg.ubidots_dispositivo_nodo, nube.carga_estado(datos))
            elif tipo == "estado":
                log.info("nodo %s %s", datos["nodo"], datos["estado"])
                self._encolar(self.cfg.ubidots_dispositivo_gateway,
                              {"nodo_en_linea": {"value": 1 if datos["estado"] == "online" else 0}})
            elif tipo == "cmd/respuesta":
                self.ordenes.al_responder(datos)

    def _orden_desde_ubidots(self, carga):
        with self.candado:
            self.ordenes.desde_ubidots(carga)

    # ------------------------------------------------------------------ tareas periódicas
    def cerrar_periodo(self, inicio):
        """Promedios del periodo que empieza en inicio, IA y su envío a la cola de la nube."""
        agregado = procesamiento.agregar_periodo(self.con, self.cfg.nodo, inicio, self.cfg.periodo_agregado_s,
                                                 self.cfg.periodo_telemetria_s)
        if agregado is None:
            return None
        fin = inicio + timedelta(seconds=self.cfg.periodo_agregado_s)
        recibidos, perdidos = procesamiento.mensajes_perdidos(self.con, self.cfg.nodo, inicio, fin)
        log.info("periodo %s: %d mensajes (%d perdidos), estado más grave %s",
                 agregado["inicio_utc"], recibidos, perdidos, agregado["estado_peor"])
        self._encolar(self.cfg.ubidots_dispositivo_nodo, nube.carga_agregado(agregado))
        if self.modulo_ia is not None:
            try:
                resultado = self.modulo_ia.evaluar(self.con, self.cfg)
            except Exception:  # la IA nunca debe detener el gateway
                log.exception("error en el módulo de IA")
            else:
                if resultado is not None:
                    self._encolar(self.cfg.ubidots_dispositivo_gateway, self.modulo_ia.carga_nube(resultado))
        return agregado

    def cerrar_dia(self, fecha):
        """ET0 del día local terminado."""
        fila = procesamiento.calcular_et0_dia(self.con, self.cfg.nodo, fecha, self.cfg.latitud_grados,
                                              self.cfg.cobertura_minima_et0, self.cfg.periodo_telemetria_s)
        if fila is not None:
            log.info("ET0 del %s: %.2f mm/d", fecha, fila["et0_mm_dia"])
            self._encolar(self.cfg.ubidots_dispositivo_gateway,
                          {"et0": {"value": round(fila["et0_mm_dia"], 2),
                                   "context": {"fecha": fila["fecha"], "t_max": fila["t_max_c"], "t_min": fila["t_min_c"]}}})

    def tick(self, ahora=None):
        """Un paso del ciclo principal (cada segundo)."""
        ahora = ahora or datetime.now(timezone.utc)
        periodo = procesamiento.inicio_periodo(ahora, self.cfg.periodo_agregado_s)
        with self.candado:
            if periodo > self.periodo_actual:
                self.cerrar_periodo(self.periodo_actual)
                self.periodo_actual = periodo
            fecha = procesamiento.fecha_local(ahora)
            if fecha > self.fecha_actual:
                self.cerrar_dia(self.fecha_actual)
                self.fecha_actual = fecha
            if self.ubidots is not None:
                nube.enviar_siguiente(self.con, self.ubidots.publicar)

    # ------------------------------------------------------------------ ciclo
    def ejecutar(self, una_vuelta=False):
        self.local.connect_async(self.cfg.mqtt_host, self.cfg.mqtt_puerto, keepalive=30)
        self.local.loop_start()
        if self.ubidots is not None:
            self.ubidots.iniciar()
        else:
            log.warning("UBIDOTS_TOKEN vacío: envío a la nube desactivado; las cargas quedan en la cola")
        try:
            while True:
                time.sleep(1)
                self.tick()
                if una_vuelta:
                    break
        except KeyboardInterrupt:
            log.info("detenido por el usuario")
        finally:
            self.local.loop_stop()
            self.local.disconnect()
            if self.ubidots is not None:
                self.ubidots.detener()
            self.con.close()


def main():
    p = argparse.ArgumentParser(description="Gateway del Challenge #3.")
    p.add_argument("--env", default=".env", help="archivo de configuración (por omisión .env)")
    p.add_argument("--una-vuelta", action="store_true", help="un solo ciclo de 1 s (pruebas)")
    p.add_argument("--nivel-registro", default="INFO")
    a = p.parse_args()
    logging.basicConfig(level=a.nivel_registro, format="%(asctime)s %(name)s %(levelname)s %(message)s")
    Gateway(config.cargar(a.env)).ejecutar(a.una_vuelta)


if __name__ == "__main__":
    main()
