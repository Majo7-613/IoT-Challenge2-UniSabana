"""Registro en SQLite de los mensajes que llegan de los nodos."""

import json
import logging

from . import db
from .validacion import campos_invalidos, es_numero

log = logging.getLogger("gateway.registro")


def _numero_o_none(valor):
    return valor if es_numero(valor) else None


class Registro:
    """Guarda cada mensaje del nodo según su tópico: sabana/{municipio}/{nodo}/{tipo}."""

    def __init__(self, con, reloj=db.ahora_utc):
        self.con = con
        self.reloj = reloj

    def procesar(self, topico, carga):
        """Despacha un mensaje por su tópico. Devuelve (tipo, datos) o (None, None) si se ignora."""
        partes = topico.split("/")
        if len(partes) < 4:
            log.warning("tópico ignorado: %s", topico)
            return None, None
        nodo, tipo = partes[2], "/".join(partes[3:])
        texto = carga.decode("utf-8", errors="replace") if isinstance(carga, bytes) else carga
        try:
            if tipo == "telemetria":
                return tipo, self.telemetria(nodo, json.loads(texto))
            if tipo == "alerta":
                return tipo, self.alerta(nodo, json.loads(texto))
            if tipo == "estado":
                return tipo, self.conexion(nodo, texto.strip())
            if tipo == "cmd/respuesta":
                return tipo, self.respuesta(nodo, json.loads(texto))
            if tipo == "cmd/desactivar_alarma":
                return None, None  # la orden la publica el propio gateway
        except (json.JSONDecodeError, ValueError, TypeError) as error:
            log.warning("mensaje inválido en %s: %s", topico, error)
            return None, None
        log.info("tipo de mensaje no reconocido: %s", topico)
        return None, None

    def telemetria(self, nodo, m):
        """Inserta un mensaje de telemetría y marca los campos fuera de rango."""
        if not isinstance(m, dict) or not es_numero(m.get("id")) or not isinstance(m.get("estado"), str):
            raise ValueError("telemetría sin id o sin estado")
        invalidos = campos_invalidos(m)
        fila = {
            "nodo": nodo,
            "msg_id": int(m["id"]),
            "recibido_utc": self.reloj(),
            "epoca_nodo": _numero_o_none(m.get("epoca")),
            "ciclo": _numero_o_none(m.get("ciclo")),
            "estado": m["estado"],
            "causas": json.dumps(m.get("causas", []), ensure_ascii=False),
            "alarma_desactivada": 1 if m.get("alarma_desactivada") else 0,
            "sensores": json.dumps(m.get("sensores", {}), ensure_ascii=False),
            "rssi": _numero_o_none(m.get("rssi")),
            "invalidos": ",".join(invalidos) or None,
        }
        for campo in ("nivel_cm", "nivel_pct", "tend_cm_min", "t_c", "hr_pct", "p_hpa",
                      "uv_idx", "vpd_kpa", "et0_mm_dia"):
            fila[campo] = _numero_o_none(m.get(campo))
        columnas = ", ".join(fila)
        marcas = ", ".join(f":{c}" for c in fila)
        self.con.execute(f"INSERT INTO telemetria ({columnas}) VALUES ({marcas})", fila)
        self.con.commit()
        if invalidos:
            log.warning("telemetría %s de %s con valores fuera de rango: %s", fila["msg_id"], nodo, invalidos)
        return fila

    def alerta(self, nodo, m):
        """Inserta un cambio de estado; un duplicado (QoS 1) se descarta."""
        if not isinstance(m, dict) or not es_numero(m.get("id")) or not isinstance(m.get("estado"), str):
            raise ValueError("alerta sin id o sin estado")
        fila = {
            "nodo": nodo,
            "msg_id": int(m["id"]),
            "recibido_utc": self.reloj(),
            "epoca_nodo": _numero_o_none(m.get("epoca")),
            "anterior": m.get("anterior"),
            "estado": m["estado"],
            "causas": json.dumps(m.get("causas", []), ensure_ascii=False),
            "nivel_pct": _numero_o_none(m.get("nivel_pct")),
            "tend_cm_min": _numero_o_none(m.get("tend_cm_min")),
        }
        cursor = self.con.execute(
            "INSERT OR IGNORE INTO alertas (nodo, msg_id, recibido_utc, epoca_nodo, anterior, estado, causas,"
            " nivel_pct, tend_cm_min) VALUES (:nodo, :msg_id, :recibido_utc, :epoca_nodo, :anterior, :estado,"
            " :causas, :nivel_pct, :tend_cm_min)", fila)
        self.con.commit()
        fila["duplicada"] = cursor.rowcount == 0
        if fila["duplicada"]:
            log.info("alerta %s de %s duplicada: descartada", fila["msg_id"], nodo)
        return fila

    def conexion(self, nodo, texto):
        """Registra online/offline del nodo."""
        if texto not in ("online", "offline"):
            raise ValueError(f"estado de conexión desconocido: {texto!r}")
        fila = {"nodo": nodo, "recibido_utc": self.reloj(), "estado": texto}
        self.con.execute("INSERT INTO conexion_nodo (nodo, recibido_utc, estado) VALUES (:nodo, :recibido_utc, :estado)", fila)
        self.con.commit()
        return fila

    def respuesta(self, nodo, m):
        """Completa la orden con la respuesta del nodo."""
        if not isinstance(m, dict) or not isinstance(m.get("orden_id"), str):
            raise ValueError("respuesta sin orden_id")
        resultado = m.get("resultado")
        if resultado not in ("aceptada", "rechazada"):
            raise ValueError(f"resultado desconocido: {resultado!r}")
        fila = {
            "orden_id": m["orden_id"], "nodo": nodo, "respuesta_utc": self.reloj(),
            "resultado": resultado, "motivo": m.get("motivo"), "estado_nodo": m.get("estado"),
        }
        cursor = self.con.execute(
            "UPDATE ordenes SET respuesta_utc = :respuesta_utc, resultado = :resultado, motivo = :motivo,"
            " estado_nodo = :estado_nodo WHERE orden_id = :orden_id", fila)
        if cursor.rowcount == 0:
            # Orden dada desde otro origen (por ejemplo, una prueba manual): se registra igual.
            self.con.execute(
                "INSERT INTO ordenes (orden_id, nodo, origen, recibida_utc, respuesta_utc, resultado, motivo,"
                " estado_nodo) VALUES (:orden_id, :nodo, 'desconocido', :respuesta_utc, :respuesta_utc,"
                " :resultado, :motivo, :estado_nodo)", fila)
        self.con.commit()
        return fila
