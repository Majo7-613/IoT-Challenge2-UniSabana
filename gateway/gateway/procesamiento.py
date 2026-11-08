"""Procesamiento local antes de enviar a la nube: promedios, mensajes perdidos y ET0 diaria."""

import logging
from datetime import datetime, timedelta, timezone

from . import db
from .evaporacion import et0_hargreaves_mm_dia, radiacion_extraterrestre_mj
from .validacion import VARIABLES

log = logging.getLogger("gateway.procesamiento")

# Colombia: UTC−5, sin horario de verano. Se fija para no depender de la base de zonas horarias.
ZONA_LOCAL = timezone(timedelta(hours=-5))

# Gravedad de cada estado, para el estado más grave de un periodo. FALLA NIVEL es una falla
# técnica: se ubica por encima de ADVERTENCIA (el nivel no se conoce) y por debajo de ALERTA.
GRAVEDAD = {"INICIANDO": 0, "NORMAL": 1, "ADVERTENCIA": 2, "FALLA NIVEL": 3, "ALERTA": 4, "CRITICO": 5}


def inicio_periodo(momento, periodo_s):
    """Inicio del periodo de periodo_s segundos que contiene el momento (alineado a la época)."""
    segundos = int(momento.timestamp())
    return datetime.fromtimestamp(segundos - segundos % periodo_s, tz=timezone.utc)


def agregar_periodo(con, nodo, inicio, periodo_s, periodo_telemetria_s=5):
    """Promedia la telemetría válida de [inicio, inicio + periodo_s) y la guarda en agregados_5min.

    Un valor marcado como inválido o nulo no entra al promedio. Devuelve la fila guardada, o None
    si no llegó ningún mensaje en el periodo. Si el periodo ya estaba agregado, no lo repite.
    """
    fin = inicio + timedelta(seconds=periodo_s)
    ini_txt, fin_txt = db.a_texto(inicio), db.a_texto(fin)
    filas = con.execute(
        "SELECT * FROM telemetria WHERE nodo = ? AND recibido_utc >= ? AND recibido_utc < ? ORDER BY recibido_utc",
        (nodo, ini_txt, fin_txt)).fetchall()
    if not filas:
        return None

    agregado = {
        "nodo": nodo, "inicio_utc": ini_txt, "fin_utc": fin_txt,
        "n_mensajes": len(filas), "n_esperados": periodo_s // periodo_telemetria_s,
    }
    for variable in VARIABLES:
        valores = [f[variable] for f in filas
                   if f[variable] is not None and variable not in (f["invalidos"] or "").split(",")]
        agregado[variable] = sum(valores) / len(valores) if valores else None
    agregado["estado_peor"] = max((f["estado"] for f in filas), key=lambda e: GRAVEDAD.get(e, -1))

    columnas = ", ".join(agregado)
    marcas = ", ".join(f":{c}" for c in agregado)
    cursor = con.execute(f"INSERT OR IGNORE INTO agregados_5min ({columnas}) VALUES ({marcas})", agregado)
    con.commit()
    if cursor.rowcount == 0:
        return None
    return agregado


def mensajes_perdidos(con, nodo, desde, hasta):
    """Cuenta los mensajes que faltan entre desde y hasta, por los saltos del campo id.

    El nodo usa un solo contador para la telemetría y las alertas, así que se unen los id de
    las dos tablas. Un id menor que el anterior indica un reinicio del nodo (el contador vuelve
    a 1) y no se cuenta como pérdida. Devuelve (recibidos, perdidos).
    """
    ids = [f[0] for f in con.execute(
        "SELECT msg_id FROM (SELECT msg_id, recibido_utc FROM telemetria WHERE nodo = ? AND recibido_utc >= ?"
        " AND recibido_utc < ? UNION ALL SELECT msg_id, recibido_utc FROM alertas WHERE nodo = ?"
        " AND recibido_utc >= ? AND recibido_utc < ?) ORDER BY recibido_utc",
        (nodo, db.a_texto(desde), db.a_texto(hasta), nodo, db.a_texto(desde), db.a_texto(hasta)))]
    perdidos = 0
    for anterior, actual in zip(ids, ids[1:]):
        if actual > anterior + 1:
            perdidos += actual - anterior - 1
    return len(ids), perdidos


def limites_dia_local(fecha):
    """[inicio, fin) en UTC del día local (UTC−5) de la fecha dada."""
    inicio = datetime(fecha.year, fecha.month, fecha.day, tzinfo=ZONA_LOCAL)
    return inicio.astimezone(timezone.utc), (inicio + timedelta(days=1)).astimezone(timezone.utc)


def calcular_et0_dia(con, nodo, fecha, latitud_grados, cobertura_minima=0.75, periodo_telemetria_s=5):
    """ET0 de Hargreaves-Samani del día local con la temperatura máxima y mínima guardadas.

    Solo se calcula si llegaron al menos cobertura_minima de los mensajes esperados del día,
    para no subestimar la amplitud térmica con un día incompleto. Devuelve la fila o None.
    """
    inicio, fin = limites_dia_local(fecha)
    fila = con.execute(
        "SELECT MAX(t_c) AS t_max, MIN(t_c) AS t_min, COUNT(t_c) AS n FROM telemetria WHERE nodo = ?"
        " AND recibido_utc >= ? AND recibido_utc < ? AND t_c IS NOT NULL"
        " AND (invalidos IS NULL OR instr(invalidos, 't_c') = 0)",
        (nodo, db.a_texto(inicio), db.a_texto(fin))).fetchone()
    esperados = 86400 // periodo_telemetria_s
    if fila["n"] == 0 or fila["n"] < cobertura_minima * esperados:
        log.info("ET0 de %s del %s no calculada: %d de %d mensajes", nodo, fecha, fila["n"], esperados)
        return None
    dia_del_anio = fecha.timetuple().tm_yday
    ra = radiacion_extraterrestre_mj(latitud_grados, dia_del_anio)
    et0 = et0_hargreaves_mm_dia(fila["t_max"], fila["t_min"], ra)
    if et0 is None:
        return None
    resultado = {
        "nodo": nodo, "fecha": fecha.isoformat(), "t_max_c": fila["t_max"], "t_min_c": fila["t_min"],
        "ra_mj_m2_d": ra, "et0_mm_dia": et0, "n_muestras": fila["n"],
    }
    con.execute(
        "INSERT OR REPLACE INTO et0_diaria (nodo, fecha, t_max_c, t_min_c, ra_mj_m2_d, et0_mm_dia, n_muestras)"
        " VALUES (:nodo, :fecha, :t_max_c, :t_min_c, :ra_mj_m2_d, :et0_mm_dia, :n_muestras)", resultado)
    con.commit()
    return resultado


def fecha_local(momento):
    """Fecha local (UTC−5) de un momento."""
    return momento.astimezone(ZONA_LOCAL).date()


def ayer_local(momento):
    """Fecha local del día anterior al momento."""
    return fecha_local(momento) - timedelta(days=1)

