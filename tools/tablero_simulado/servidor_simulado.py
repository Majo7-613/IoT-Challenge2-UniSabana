#!/usr/bin/env python3
"""
Servidor simulado del tablero de monitoreo hídrico.

DATOS SIMULADOS. Este servidor NO usa el ESP32 ni los sensores: genera
valores falsos, declarados como tales, para probar el frontend del tablero
(firmware/data/) en un computador. Todas sus respuestas llevan el campo
"fuente": "servidor simulado", y el tablero muestra la banda «DATOS SIMULADOS».

Qué reproduce del firmware (firmware/src/web.cpp):
  - las rutas GET /, GET /api/actual, GET /api/historico?b=rapido|lento,
    POST /api/alarma/desactivar, WebSocket /ws y los archivos de data/;
  - la estructura del JSON y el envío por WebSocket cada 1 s;
  - el token por dispositivo (encabezado X-Token o parámetro ?token=);
  - la desactivación de la alarma: solo en ALERTA o CRITICO, y se rearma si
    el estado empeora.

Qué NO reproduce:
  - la autenticación Digest (no se implementa) ni el filtro de subred;
  - la lógica de fusión: el estado de cada escenario es fijo y los valores se
    eligen para que sean coherentes con las reglas de la Wiki 2.5.2.

Uso (solo librería estándar de Python 3.8 o posterior):
  python servidor_simulado.py [--puerto 8080] [--escenario ciclo|NORMAL|ADVERTENCIA|ALERTA|CRITICO|FALLA_NIVEL]
                              [--sin-hora] [--token TOKEN ...]
  Abrir http://localhost:8080/?token=token-dispositivo-1
"""

import argparse
import base64
import hashlib
import json
import math
import mimetypes
import random
import socket
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

DIR_DATA = (Path(__file__).resolve().parent / ".." / ".." / "firmware" / "data").resolve()
FUENTE = "servidor simulado"

# Geometría y umbrales de DEMOSTRACIÓN de firmware/include/config.h.
ALTURA_UTIL_CM = 30.0
HISTORICO = {"rapido": (5, 120), "lento": (300, 288)}  # periodo (s), registros
ARRANQUE_SIMULADO_S = 26 * 3600  # El equipo simulado "lleva" 26 h encendido.

# Valores de cada escenario, elegidos para cumplir las reglas de la fusión.
ESCENARIOS = {
    "NORMAL": dict(estado="NORMAL", causas=[], pct=72, tendencia=-0.05, t=18.4, hr=68,
                   p=752.3, uv=3.1, sensores={}),
    "ADVERTENCIA": dict(estado="ADVERTENCIA", causas=["nivel_preventivo"], pct=37, tendencia=-0.21,
                        t=22.6, hr=56, p=751.8, uv=4.4, sensores={}),
    "ALERTA": dict(estado="ALERTA", causas=["nivel_preventivo", "descenso", "vpd_alto", "temperatura_alta", "uv_alto"],
                   pct=31, tendencia=-1.32, t=30.8, hr=38, p=750.9, uv=7.4, sensores={}),
    "CRITICO": dict(estado="CRITICO", causas=["nivel_critico", "nivel_preventivo", "descenso", "vpd_alto"],
                    pct=16, tendencia=-1.12, t=28.7, hr=35, p=750.6, uv=5.2, sensores={"guva": "VIEJO"}),
    "FALLA_NIVEL": dict(estado="FALLA NIVEL", causas=[], pct=None, tendencia=None, t=20.1, hr=61,
                        p=752.0, uv=2.6, sensores={"nivel": "FALLA"}),
}
ORDEN_CICLO = ["NORMAL", "ADVERTENCIA", "ALERTA", "CRITICO", "FALLA_NIVEL"]
SEGUNDOS_POR_ESCENARIO = 30
GRAVEDAD = {"ALERTA": 1, "CRITICO": 2}


def estado_registro(pct, tendencia, t, vpd, uv):
    """Estado de un registro histórico con las reglas de la fusión (sin histéresis)."""
    if pct is None:
        return "FALLA NIVEL"
    descenso = tendencia is not None and tendencia <= -1.0
    atmosferica = t >= 30 or vpd >= 1.5 or uv >= 6
    if pct <= 20:
        return "CRITICO"
    if descenso and atmosferica:
        return "ALERTA"
    if descenso or pct <= 40:
        return "ADVERTENCIA"
    return "NORMAL"


def vpd_kpa(t_c, hr_pct):
    """VPD con la ecuación de Tetens (FAO-56, ec. 11), como el firmware."""
    es = 0.6108 * math.exp(17.27 * t_c / (t_c + 237.3))
    return es * (1 - hr_pct / 100.0)


class Simulador:
    """Estado del equipo simulado."""

    def __init__(self, escenario, con_hora):
        self.escenario_fijo = escenario
        self.con_hora = con_hora
        self.inicio = time.time()
        self.alarma_desactivada = False
        self.estado_anterior = None
        self.lock = threading.Lock()

    def uptime_s(self):
        return int(ARRANQUE_SIMULADO_S + time.time() - self.inicio)

    def nombre_escenario(self):
        if self.escenario_fijo != "ciclo":
            return self.escenario_fijo
        indice = int((time.time() - self.inicio) // SEGUNDOS_POR_ESCENARIO) % len(ORDEN_CICLO)
        return ORDEN_CICLO[indice]

    def epoca(self):
        return int(time.time()) if self.con_hora else None

    def _actualizar_alarma(self, estado):
        """Desactivación con rearme, como fusion.cpp."""
        anterior = self.estado_anterior
        if estado not in GRAVEDAD:
            self.alarma_desactivada = False
        elif anterior in GRAVEDAD and GRAVEDAD[estado] > GRAVEDAD[anterior]:
            self.alarma_desactivada = False
        self.estado_anterior = estado

    def desactivar(self):
        with self.lock:
            esc = ESCENARIOS[self.nombre_escenario()]
            if esc["estado"] in GRAVEDAD:
                self.alarma_desactivada = True

    def actual(self):
        with self.lock:
            esc = ESCENARIOS[self.nombre_escenario()]
            self._actualizar_alarma(esc["estado"])
            ahora = self.uptime_s()
            ruido = random.Random(ahora)  # Pequeña variación entre segundos.

            def sensor(nombre, edad_normal):
                estado = esc["sensores"].get(nombre, "OK")
                edad = edad_normal if estado == "OK" else 12 + ahora % 60
                return {"sensor": estado, "edad_s": edad}

            t = round(esc["t"] + ruido.uniform(-0.1, 0.1), 1)
            hr = round(esc["hr"] + ruido.uniform(-0.5, 0.5))
            uv = round(esc["uv"] + ruido.uniform(-0.1, 0.1), 1)
            if esc["pct"] is None:
                nivel = {"cm": None, "pct": None, "tendencia_cm_min": None, "tendencia": "tendencia no disponible"}
            else:
                pct = esc["pct"] + ruido.uniform(-0.2, 0.2)
                nivel = {"cm": round(pct * ALTURA_UTIL_CM / 100, 1), "pct": round(pct),
                         "tendencia_cm_min": round(esc["tendencia"] + ruido.uniform(-0.02, 0.02), 2),
                         "tendencia": "disponible"}
            nivel.update({"compensacion": "T DHT22"}, **sensor("nivel", 0))
            guva = {"uv_indice": uv, "mv": round(uv * 100)}
            guva.update(sensor("guva", 0))
            return {
                "fuente": FUENTE,
                "ciclo": ahora,
                "t_s": ahora,
                "epoca": self.epoca(),
                "estado": esc["estado"],
                "causas": list(esc["causas"]),
                "alarma_desactivada": self.alarma_desactivada,
                "nivel": nivel,
                "dht22": dict({"temperatura_c": t, "humedad_pct": hr}, **sensor("dht22", ahora % 2)),
                "bmp180": dict({"presion_hpa": esc["p"]}, **sensor("bmp180", 0)),
                "guva": guva,
                "vpd_kpa": round(vpd_kpa(t, hr), 2),
                "et0_mm_dia": 3.8 if self.con_hora else None,
                "et0": "disponible" if self.con_hora else "no disponible",
                "wifi_rssi": -58,
            }

    def historico(self, bufer):
        """Serie falsa que termina en los valores del escenario actual."""
        periodo, cantidad = HISTORICO[bufer]
        esc = ESCENARIOS[self.nombre_escenario()]
        ahora = self.uptime_s()
        ultimo_t = ahora - ahora % periodo
        azar = random.Random(bufer + self.nombre_escenario())
        registros = []
        for i in range(cantidad):
            atras = cantidad - 1 - i              # 0 = registro más reciente
            t_s = ultimo_t - atras * periodo
            minutos_atras = atras * periodo / 60.0
            if bufer == "lento":
                # Ciclo diario: más calor y UV hacia las 13:00, más humedad de noche.
                hora = (time.localtime(time.time() - atras * periodo).tm_hour
                        + time.localtime(time.time() - atras * periodo).tm_min / 60.0)
                dia = math.cos((hora - 13) / 24 * 2 * math.pi)
                t = esc["t"] - 6 + 6 * dia + azar.uniform(-0.3, 0.3)
                hr = min(95, esc["hr"] + 18 - 18 * dia + azar.uniform(-1, 1))
                uv = max(0.0, esc["uv"] * max(0.0, math.cos((hora - 12.5) / 12 * math.pi)) + azar.uniform(-0.1, 0.1))
                pct = None if esc["pct"] is None else min(100, esc["pct"] + 0.35 * atras * periodo / 3600 * 2)
            else:
                t = esc["t"] + azar.uniform(-0.15, 0.15)
                hr = esc["hr"] + azar.uniform(-0.8, 0.8)
                uv = esc["uv"] + azar.uniform(-0.15, 0.15)
                pct = None if esc["pct"] is None else (
                    esc["pct"] - (esc["tendencia"] or 0) * minutos_atras * 100 / ALTURA_UTIL_CM
                    + azar.uniform(-0.3, 0.3))
            # En FALLA NIVEL, el nivel falta solo en el último minuto y medio.
            if esc["pct"] is None:
                base = ESCENARIOS["NORMAL"]["pct"]
                pct = None if atras * periodo < 90 else base + azar.uniform(-0.3, 0.3)
            # Un sensor VIEJO deja huecos en sus últimos registros.
            uv_valido = not (esc["sensores"].get("guva") == "VIEJO" and atras * periodo < 15)
            registros.append({
                "t_s": t_s,
                "nivel_cm": None if pct is None else round(pct * ALTURA_UTIL_CM / 100, 1),
                "nivel_pct": None if pct is None else round(pct),
                "temperatura_c": round(t, 1),
                "humedad_pct": round(hr),
                "presion_hpa": round(esc["p"] + azar.uniform(-0.4, 0.4), 1),
                "uv_indice": round(uv, 1) if uv_valido else None,
                "vpd_kpa": round(vpd_kpa(t, hr), 2),
                "estado": estado_registro(pct, esc["tendencia"], t, vpd_kpa(t, hr), uv),
            })
        return {"fuente": FUENTE, "bufer": bufer, "periodo_s": periodo, "ahora_s": ahora,
                "ahora_epoca": self.epoca(), "registros": registros}


# ---------------------------------------------------------------------------
# HTTP y WebSocket
# ---------------------------------------------------------------------------

GUID_WS = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"  # RFC 6455


class Manejador(BaseHTTPRequestHandler):
    server_version = "TableroSimulado/1.0"
    protocol_version = "HTTP/1.1"  # El navegador exige HTTP/1.1 para el WebSocket.
    simulador = None   # Se asigna en main().
    tokens = ()

    def log_message(self, formato, *args):
        print("[simulado] " + (formato % args))

    def _token_valido(self, consulta):
        token = self.headers.get("X-Token") or (consulta.get("token") or [""])[0]
        return token in self.tokens

    def _enviar(self, codigo, cuerpo, tipo="application/json; charset=utf-8"):
        datos = cuerpo.encode("utf-8") if isinstance(cuerpo, str) else cuerpo
        self.send_response(codigo)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(datos)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(datos)

    def _json(self, codigo, objeto):
        self._enviar(codigo, json.dumps(objeto, ensure_ascii=False))

    def do_GET(self):
        url = urlparse(self.path)
        consulta = parse_qs(url.query)
        if not self._token_valido(consulta):
            self._enviar(403, "Dispositivo no autorizado", "text/plain; charset=utf-8")
            return
        if url.path == "/ws":
            self._websocket()
        elif url.path == "/api/actual":
            self._json(200, self.simulador.actual())
        elif url.path == "/api/historico":
            bufer = (consulta.get("b") or [""])[0]
            if bufer not in HISTORICO:
                self._json(400, {"error": "b debe ser rapido o lento"})
            else:
                self._json(200, self.simulador.historico(bufer))
        else:
            self._archivo(url.path)

    def do_POST(self):
        url = urlparse(self.path)
        if not self._token_valido(parse_qs(url.query)):
            self._enviar(403, "Dispositivo no autorizado", "text/plain; charset=utf-8")
            return
        if url.path == "/api/alarma/desactivar":
            self.simulador.desactivar()
            self._json(202, {"orden": "recibida",
                             "nota": "se aplica solo en ALERTA o CRITICO; el resultado se ve en /api/actual"})
        else:
            self._enviar(404, "No encontrado", "text/plain; charset=utf-8")

    def _archivo(self, ruta):
        if ruta == "/":
            ruta = "/index.html"
        destino = (DIR_DATA / ruta.lstrip("/")).resolve()
        if DIR_DATA not in destino.parents or not destino.is_file():
            self._enviar(404, "No encontrado", "text/plain; charset=utf-8")
            return
        tipo = mimetypes.guess_type(str(destino))[0] or "application/octet-stream"
        if tipo.startswith("text/") or tipo == "application/javascript":
            tipo += "; charset=utf-8"
        self._enviar(200, destino.read_bytes(), tipo)

    def _websocket(self):
        clave = self.headers.get("Sec-WebSocket-Key")
        if not clave or self.headers.get("Upgrade", "").lower() != "websocket":
            self._enviar(400, "Se esperaba un WebSocket", "text/plain; charset=utf-8")
            return
        aceptar = base64.b64encode(hashlib.sha1((clave + GUID_WS).encode()).digest()).decode()
        self.send_response(101)
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", aceptar)
        self.end_headers()
        self.wfile.flush()
        self.close_connection = True
        try:
            while True:
                texto = json.dumps(self.simulador.actual(), ensure_ascii=False).encode("utf-8")
                cabecera = bytes([0x81])  # FIN + trama de texto
                if len(texto) < 126:
                    cabecera += bytes([len(texto)])
                else:
                    cabecera += bytes([126]) + struct.pack("!H", len(texto))
                self.connection.sendall(cabecera + texto)
                time.sleep(1)
        except (ConnectionError, socket.error):
            pass  # El navegador cerró la conexión.


def main():
    parser = argparse.ArgumentParser(description="Servidor simulado del tablero (DATOS SIMULADOS).")
    parser.add_argument("--puerto", type=int, default=8080)
    parser.add_argument("--escenario", default="ciclo", choices=["ciclo"] + ORDEN_CICLO)
    parser.add_argument("--sin-hora", action="store_true",
                        help="simula un equipo sin hora NTP (epoca null; ET0 no disponible)")
    parser.add_argument("--token", action="append",
                        help="token autorizado (repetible); por defecto, los de secrets.example.h")
    args = parser.parse_args()

    Manejador.simulador = Simulador(args.escenario, not args.sin_hora)
    Manejador.tokens = tuple(args.token or ["token-dispositivo-1", "token-dispositivo-2"])
    servidor = ThreadingHTTPServer(("127.0.0.1", args.puerto), Manejador)
    servidor.daemon_threads = True
    print("DATOS SIMULADOS: servidor de prueba del tablero, sin ESP32 ni sensores.")
    print("Escenario: %s · hora NTP simulada: %s" % (args.escenario, "no" if args.sin_hora else "sí"))
    print("Abrir http://localhost:%d/?token=%s" % (args.puerto, Manejador.tokens[0]))
    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
