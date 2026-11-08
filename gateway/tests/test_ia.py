"""Pruebas del módulo de IA (pytest). Sin red: la recomendación se prueba con el texto de respaldo."""

import json
import random
from datetime import datetime, timedelta, timezone

import pytest

from gateway import config, db
from gateway import ia
from gateway.ia import anomalias, prediccion, recomendacion


# ---------------------------------------------------------------- predicción
def test_descenso_lineal_exacto():
    puntos = [(h / 12, 60.0 - 2.0 * h / 12) for h in range(72)]   # −2 %/h durante 6 h
    r = prediccion.horas_hasta_critico(puntos, 20.0)
    nivel_final = 60.0 - 2.0 * 71 / 12
    assert r["pendiente_pct_h"] == pytest.approx(-2.0)
    assert r["horas"] == pytest.approx((nivel_final - 20.0) / 2.0)
    assert r["horas_min"] == pytest.approx(r["horas"]) and r["horas_max"] == pytest.approx(r["horas"])


def test_descenso_con_ruido_da_intervalo():
    azar = random.Random(1)
    puntos = [(h / 12, 70.0 - 1.5 * h / 12 + azar.gauss(0, 0.5)) for h in range(72)]
    r = prediccion.horas_hasta_critico(puntos, 20.0)
    assert r["horas_min"] < r["horas"] < r["horas_max"]
    assert r["pendiente_pct_h"] == pytest.approx(-1.5, abs=0.1)


def test_nivel_estable_no_predice():
    azar = random.Random(2)
    puntos = [(h / 12, 70.0 + azar.gauss(0, 0.5)) for h in range(72)]
    assert prediccion.horas_hasta_critico(puntos, 20.0)["horas"] is None


def test_nivel_bajo_el_umbral_y_pocos_datos():
    assert prediccion.horas_hasta_critico([(h, 15.0 - h) for h in range(10)], 20.0)["horas"] == 0.0
    assert prediccion.horas_hasta_critico([(0, 50.0), (1, 49.0)], 20.0) is None


# ---------------------------------------------------------------- anomalías
def fila(**valores):
    base = {"invalidos": None, "nivel_pct": 70.0, "t_c": 18.0, "hr_pct": 70.0, "p_hpa": 752.0}
    base.update(valores)
    return base


def test_reglas():
    normal = [fila(t_c=18.0 + i * 0.01) for i in range(20)]
    assert anomalias.reglas(normal) == []
    assert "lectura_fuera_de_rango" in anomalias.reglas(normal + [fila(invalidos="t_c")])
    assert "salto_de_nivel" in anomalias.reglas(normal + [fila(nivel_pct=40.0)])
    congelado = [fila(t_c=18.0 + i * 0.01, hr_pct=55.0) for i in range(anomalias.MENSAJES_CONGELADO)]
    assert anomalias.reglas(congelado) == ["sensor_congelado:hr_pct", "sensor_congelado:p_hpa"]


def test_modelo_detecta_combinacion_atipica():
    azar = random.Random(3)
    historico = [{"tend_cm_min": azar.gauss(-0.05, 0.03), "t_c": azar.gauss(16, 2), "hr_pct": azar.gauss(75, 5),
                  "vpd_kpa": azar.gauss(0.5, 0.1), "uv_idx": azar.gauss(3, 1), "hora": azar.uniform(0, 24)}
                 for _ in range(300)]
    normal = {"tend_cm_min": -0.05, "t_c": 16.2, "hr_pct": 75, "vpd_kpa": 0.5, "uv_idx": 3, "hora": 12.0}
    raro = {"tend_cm_min": -2.5, "t_c": 15.0, "hr_pct": 80, "vpd_kpa": 0.4, "uv_idx": 0.5, "hora": 12.0}  # descenso rápido sin calor
    assert anomalias.puntaje_atipico(historico, normal)[0] is False
    atipico, _, metodo = anomalias.puntaje_atipico(historico, raro)
    assert atipico is True and metodo in ("isolation_forest", "z_robusto")
    assert anomalias.puntaje_atipico(historico[:10], raro) is None   # pocos datos: no opina


# ---------------------------------------------------------------- recomendación
def test_texto_de_respaldo():
    texto = recomendacion.texto_respaldo({"estado": "ALERTA", "horas_hasta_critico": 5.25, "horas_min": 4.5,
                                          "horas_max": 6.0, "anomalias": ["salto_de_nivel"]})
    assert texto.startswith("Sugerencia:") and "5.2 h" in texto and "entre 4.5 y 6.0 h" in texto
    assert "salto_de_nivel" in texto
    assert "nivel crítico" in recomendacion.texto_respaldo({"estado": "CRITICO"})
    assert recomendacion.recomendar({"estado": "NORMAL"}, clave="") == (
        "Sugerencia: sin descenso significativo del nivel; operación normal.", "reglas")


def test_gemini_sin_red_usa_respaldo(monkeypatch):
    def sin_red(*_a, **_k):
        raise recomendacion.urllib.error.URLError("sin red")
    monkeypatch.setattr(recomendacion.urllib.request, "urlopen", sin_red)
    texto, origen = recomendacion.recomendar({"estado": "NORMAL"}, clave="clave-de-prueba")
    assert origen == "reglas" and texto.startswith("Sugerencia:")


# ---------------------------------------------------------------- integración con SQLite
def test_evaluar_de_punta_a_punta():
    con = db.conectar(":memory:")
    cfg = config.Config()
    inicio = datetime(2026, 11, 7, 12, 0, tzinfo=timezone.utc)
    for i in range(80):   # 80 promedios de 5 min con descenso de 3 %/h
        t = inicio + timedelta(minutes=5 * i)
        con.execute(
            "INSERT INTO agregados_5min (nodo, inicio_utc, fin_utc, n_mensajes, n_esperados, nivel_pct, tend_cm_min,"
            " t_c, hr_pct, vpd_kpa, uv_idx, estado_peor) VALUES (?, ?, ?, 60, 60, ?, -0.15, 18, 70, 0.6, 3, ?)",
            ("chia-01", db.a_texto(t), db.a_texto(t + timedelta(minutes=5)), 80.0 - 0.25 * i,
             "ADVERTENCIA" if i > 60 else "NORMAL"))
    con.commit()
    resultado = ia.evaluar(con, cfg, ahora=0)
    nivel_final = 80.0 - 0.25 * 79
    assert resultado["horas_hasta_critico"] == pytest.approx((nivel_final - 20.0) / 3.0, rel=1e-6)
    assert resultado["recomendacion"].startswith("Sugerencia:")
    assert json.loads(con.execute("SELECT detalle FROM ia_resultados").fetchone()[0])["prediccion"]["n"] == 72
    carga = ia.carga_nube(resultado)
    assert carga["horas_hasta_critico"]["value"] == pytest.approx(resultado["horas_hasta_critico"], abs=0.01)
    assert set(carga) == {"horas_hasta_critico", "anomalia", "recomendacion"}
