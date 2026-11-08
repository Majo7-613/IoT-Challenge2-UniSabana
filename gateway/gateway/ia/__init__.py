"""Módulo de IA del gateway (docs/diseno/modulo-ia.md).

evaluar() se llama al cerrar cada periodo de 5 min: predice las horas hasta el nivel crítico,
detecta anomalías y, en un cambio de estado o como máximo una vez por hora, genera la
recomendación en texto. Guarda el resultado en ia_resultados y lo devuelve para la nube.
"""

import json
import time

from .. import db
from . import anomalias, prediccion, recomendacion

VENTANA_PREDICCION = 288         # promedios de 5 min = 24 h: promedia el ciclo diario de consumo
VENTANA_PREDICCION_MINIMA = 72   # 6 h: se usa mientras no haya 24 h de datos
VENTANA_ENTRENAMIENTO = 2016     # promedios de 5 min = 7 días
PERIODO_RECOMENDACION_S = 3600

_ultima_recomendacion = {"momento": 0.0, "estado": None, "texto": None, "origen": None}


def _horas(texto_utc, origen):
    return (db.de_texto(texto_utc) - origen).total_seconds() / 3600.0


def evaluar(con, cfg, ahora=None):
    """Ejecuta los tres componentes sobre los datos de SQLite. Devuelve el resultado o None sin datos."""
    agregados = con.execute(
        "SELECT * FROM agregados_5min WHERE nodo = ? ORDER BY inicio_utc DESC LIMIT ?",
        (cfg.nodo, VENTANA_ENTRENAMIENTO)).fetchall()[::-1]
    if not agregados:
        return None
    actual = agregados[-1]

    # 1. Predicción con las últimas 24 h (o 6 h mientras no haya 24 h de datos).
    ventana = agregados[-VENTANA_PREDICCION:] if len(agregados) >= VENTANA_PREDICCION else agregados[-VENTANA_PREDICCION_MINIMA:]
    origen = db.de_texto(ventana[0]["inicio_utc"])
    pred = prediccion.horas_hasta_critico(
        [(_horas(f["inicio_utc"], origen), f["nivel_pct"]) for f in ventana], cfg.umbral_critico_pct)

    # 2. Anomalías: reglas sobre la telemetría reciente y modelo sobre los promedios.
    telemetria = con.execute(
        "SELECT * FROM telemetria WHERE nodo = ? ORDER BY recibido_utc DESC LIMIT ?",
        (cfg.nodo, anomalias.MENSAJES_CONGELADO)).fetchall()[::-1]
    tipos = anomalias.reglas(telemetria)
    modelo = anomalias.puntaje_atipico(agregados[:-1], actual)
    if modelo is not None and modelo[0]:
        tipos.append("combinacion_atipica")

    # 3. Recomendación: en un cambio de estado o como máximo una vez por hora.
    et0 = con.execute("SELECT et0_mm_dia FROM et0_diaria WHERE nodo = ? ORDER BY fecha DESC LIMIT 1",
                      (cfg.nodo,)).fetchone()
    datos = {
        "estado": actual["estado_peor"], "nivel_pct": actual["nivel_pct"],
        "horas_hasta_critico": pred["horas"] if pred else None,
        "horas_min": pred["horas_min"] if pred else None, "horas_max": pred["horas_max"] if pred else None,
        "anomalias": tipos, "et0_mm_dia": et0[0] if et0 else None,
    }
    momento = time.time() if ahora is None else ahora
    if (_ultima_recomendacion["estado"] != datos["estado"]
            or momento - _ultima_recomendacion["momento"] >= PERIODO_RECOMENDACION_S):
        texto, fuente = recomendacion.recomendar(datos, cfg.gemini_api_key, cfg.gemini_modelo)
        _ultima_recomendacion.update(momento=momento, estado=datos["estado"], texto=texto, origen=fuente)

    resultado = {
        "nodo": cfg.nodo, "calculado_utc": db.ahora_utc(),
        "horas_hasta_critico": datos["horas_hasta_critico"],
        "anomalia": 1 if tipos else 0, "tipo_anomalia": ",".join(tipos) or None,
        "detalle": json.dumps({
            "prediccion": pred, "modelo": None if modelo is None else {"atipico": modelo[0], "puntaje": modelo[1],
                                                                       "metodo": modelo[2]},
            "recomendacion": _ultima_recomendacion["texto"], "origen_recomendacion": _ultima_recomendacion["origen"],
        }, ensure_ascii=False),
    }
    con.execute(
        "INSERT INTO ia_resultados (nodo, calculado_utc, horas_hasta_critico, anomalia, tipo_anomalia, detalle)"
        " VALUES (:nodo, :calculado_utc, :horas_hasta_critico, :anomalia, :tipo_anomalia, :detalle)", resultado)
    con.commit()
    resultado["recomendacion"] = _ultima_recomendacion["texto"]
    return resultado


def carga_nube(resultado):
    """Carga para el dispositivo del gateway en Ubidots. horas_hasta_critico = −1 si el nivel no baja."""
    horas = resultado["horas_hasta_critico"]
    return {
        "horas_hasta_critico": {"value": round(horas, 2) if horas is not None else -1,
                                "context": {"texto": "el nivel no baja" if horas is None else "estimación"}},
        "anomalia": {"value": resultado["anomalia"], "context": {"tipos": resultado["tipo_anomalia"] or ""}},
        "recomendacion": {"value": 1, "context": {"texto": (resultado.get("recomendacion") or "")[:400]}},
    }
