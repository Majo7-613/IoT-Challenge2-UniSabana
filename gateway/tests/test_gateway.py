"""Pruebas automáticas del gateway (pytest). Desde Repo/gateway: .venv/Scripts/python -m pytest

No necesitan broker ni Internet: usan una base SQLite en memoria y relojes fijos.
"""

import json
from datetime import date, datetime, timedelta, timezone

import pytest

from gateway import config, db, nube, procesamiento
from gateway.evaporacion import et0_hargreaves_mm_dia, presion_saturacion_kpa, radiacion_extraterrestre_mj, vpd_kpa
from gateway.ordenes import GestorOrdenes, nuevo_id, valor_de_control
from gateway.registro import Registro
from gateway.validacion import campos_invalidos

INICIO = datetime(2026, 11, 7, 15, 0, 0, tzinfo=timezone.utc)


class Reloj:
    """Reloj controlable: devuelve el momento actual como texto y avanza a pedido."""

    def __init__(self, inicio=INICIO):
        self.momento = inicio

    def __call__(self):
        return db.a_texto(self.momento)

    def avanzar(self, segundos):
        self.momento += timedelta(seconds=segundos)


def telemetria(msg_id, **cambios):
    m = {"id": msg_id, "ciclo": msg_id * 5, "epoca": 1793286000, "estado": "NORMAL", "causas": [],
         "alarma_desactivada": False, "nivel_cm": 22.0, "nivel_pct": 73, "tend_cm_min": -0.12, "t_c": 18.4,
         "hr_pct": 71, "p_hpa": 752.4, "uv_idx": 1.2, "vpd_kpa": 0.61, "et0_mm_dia": None,
         "sensores": {"nivel": "OK", "dht22": "OK", "bmp180": "OK", "guva": "OK"}, "rssi": -61}
    m.update(cambios)
    return json.dumps(m).encode()


@pytest.fixture
def con():
    c = db.conectar(":memory:")
    yield c
    c.close()


# ---------------------------------------------------------------- evaporación (casos FAO-56)
def test_presion_saturacion_y_vpd():
    assert presion_saturacion_kpa(25.0) == pytest.approx(3.168, abs=0.002)   # tabla 2.3 de FAO-56
    assert vpd_kpa(25.0, 60.0) == pytest.approx(1.267, abs=0.002)
    assert vpd_kpa(25.0, 120.0) is None


def test_radiacion_extraterrestre_ejemplo_8_fao56():
    assert radiacion_extraterrestre_mj(-20.0, 246) == pytest.approx(32.2, abs=0.05)


def test_et0_hargreaves_caso_a_mano():
    # 0.0023 × (24 + 17.8) × √12 × 0.408 × 32.2 = 4.375 mm/d (mismo caso que test_calculos del firmware)
    assert et0_hargreaves_mm_dia(30.0, 18.0, 32.2) == pytest.approx(4.375, abs=0.01)
    assert et0_hargreaves_mm_dia(18.0, 30.0, 32.2) is None


# ---------------------------------------------------------------- validación
def test_campos_fuera_de_rango():
    assert campos_invalidos({"t_c": 18.4, "hr_pct": 71}) == []
    assert campos_invalidos({"t_c": 95.0, "hr_pct": 101, "p_hpa": 752}) == ["t_c", "hr_pct"]
    assert campos_invalidos({"nivel_pct": "73"}) == ["nivel_pct"]
    assert campos_invalidos({"nivel_pct": None}) == []


# ---------------------------------------------------------------- registro
def test_registro_de_cada_tipo(con):
    r = Registro(con, Reloj())
    tipo, fila = r.procesar("sabana/chia/chia-01/telemetria", telemetria(1, t_c=95.0))
    assert tipo == "telemetria" and fila["invalidos"] == "t_c"
    tipo, fila = r.procesar("sabana/chia/chia-01/alerta",
                            json.dumps({"id": 2, "epoca": 1, "anterior": "NORMAL", "estado": "ALERTA"}))
    assert tipo == "alerta" and not fila["duplicada"]
    _, fila = r.procesar("sabana/chia/chia-01/alerta",
                         json.dumps({"id": 2, "epoca": 1, "anterior": "NORMAL", "estado": "ALERTA"}))
    assert fila["duplicada"]  # QoS 1 puede entregar dos veces
    assert r.procesar("sabana/chia/chia-01/estado", b"offline")[1]["estado"] == "offline"
    assert con.execute("SELECT COUNT(*) FROM alertas").fetchone()[0] == 1


def test_registro_ignora_mensajes_invalidos(con):
    r = Registro(con, Reloj())
    assert r.procesar("sabana/chia/chia-01/telemetria", b"{no es json") == (None, None)
    assert r.procesar("sabana/chia/chia-01/telemetria", b'{"estado": "NORMAL"}') == (None, None)
    assert r.procesar("sabana/chia/chia-01/estado", b"quizas") == (None, None)
    assert r.procesar("otro/topico", b"x") == (None, None)
    assert con.execute("SELECT COUNT(*) FROM telemetria").fetchone()[0] == 0


# ---------------------------------------------------------------- procesamiento
def test_promedio_excluye_invalidos_y_toma_el_peor_estado(con):
    reloj = Reloj()
    r = Registro(con, reloj)
    r.procesar("sabana/chia/chia-01/telemetria", telemetria(1, nivel_pct=70, t_c=18.0))
    reloj.avanzar(5)
    r.procesar("sabana/chia/chia-01/telemetria", telemetria(2, nivel_pct=60, t_c=95.0, estado="ADVERTENCIA"))
    reloj.avanzar(5)
    r.procesar("sabana/chia/chia-01/telemetria", telemetria(3, nivel_pct=50, t_c=20.0, estado="NORMAL"))
    agregado = procesamiento.agregar_periodo(con, "chia-01", INICIO, 300)
    assert agregado["n_mensajes"] == 3 and agregado["n_esperados"] == 60
    assert agregado["nivel_pct"] == pytest.approx(60.0)
    assert agregado["t_c"] == pytest.approx(19.0)  # el 95 °C fuera de rango no entra
    assert agregado["estado_peor"] == "ADVERTENCIA"
    assert procesamiento.agregar_periodo(con, "chia-01", INICIO, 300) is None  # no se repite


def test_mensajes_perdidos_con_reinicio(con):
    reloj = Reloj()
    r = Registro(con, reloj)
    for msg_id in (1, 2, 5, 6, 1, 2):  # faltan 3 y 4; después el nodo se reinicia
        r.procesar("sabana/chia/chia-01/telemetria", telemetria(msg_id))
        reloj.avanzar(5)
    recibidos, perdidos = procesamiento.mensajes_perdidos(con, "chia-01", INICIO, reloj.momento)
    assert (recibidos, perdidos) == (6, 2)


def test_et0_diaria_con_cobertura(con):
    fecha = date(2026, 11, 6)
    inicio, _ = procesamiento.limites_dia_local(fecha)
    reloj = Reloj(inicio)
    r = Registro(con, reloj)
    # Un mensaje cada 60 s con temperatura entre 8 y 20 °C: cobertura baja (1440 de 17 280).
    for i in range(1440):
        r.procesar("sabana/chia/chia-01/telemetria", telemetria(i + 1, t_c=8.0 + 12.0 * (i % 720) / 719))
        reloj.avanzar(60)
    assert procesamiento.calcular_et0_dia(con, "chia-01", fecha, 4.86) is None
    fila = procesamiento.calcular_et0_dia(con, "chia-01", fecha, 4.86, cobertura_minima=0.05)
    assert fila["t_max_c"] == pytest.approx(20.0) and fila["t_min_c"] == pytest.approx(8.0)
    ra = radiacion_extraterrestre_mj(4.86, fecha.timetuple().tm_yday)
    assert fila["et0_mm_dia"] == pytest.approx(et0_hargreaves_mm_dia(20.0, 8.0, ra))


def test_dia_local_es_utc_menos_5():
    inicio, fin = procesamiento.limites_dia_local(date(2026, 11, 7))
    assert db.a_texto(inicio) == "2026-11-07T05:00:00.000Z" and db.a_texto(fin) == "2026-11-08T05:00:00.000Z"


# ---------------------------------------------------------------- nube
def test_carga_de_promedios_para_ubidots():
    agregado = {"inicio_utc": "2026-11-07T15:00:00.000Z", "nivel_pct": 60.0, "t_c": None, "estado_peor": "CRITICO"}
    carga = nube.carga_agregado(agregado)
    ts = int(INICIO.timestamp() * 1000)
    assert carga["nivel_pct"] == {"value": 60.0, "timestamp": ts}
    assert "temperatura" not in carga
    assert carga["estado"]["value"] == 5 and carga["estado"]["context"]["texto"] == "CRITICO"


def test_cola_conserva_el_orden_y_reintenta(con):
    nube.encolar(con, "chia-01", {"a": 1})
    nube.encolar(con, "chia-01", {"b": 2})
    enviados = []
    assert not nube.enviar_siguiente(con, lambda d, t: False)  # sin Internet: no se pierde nada
    assert nube.pendientes(con) == 2
    assert nube.enviar_siguiente(con, lambda d, t: enviados.append(t) or True)
    assert nube.enviar_siguiente(con, lambda d, t: enviados.append(t) or True)
    assert [json.loads(t) for t in enviados] == [{"a": 1}, {"b": 2}]
    assert nube.pendientes(con) == 0
    assert con.execute("SELECT intentos FROM cola_nube ORDER BY id").fetchall()[0][0] == 2


# ---------------------------------------------------------------- órdenes
def test_valor_de_control():
    assert valor_de_control(b"1.0") == 1.0
    assert valor_de_control(b'{"value": 0}') == 0.0
    assert valor_de_control(b"hola") is None


def test_id_de_orden_cumple_reglas_del_firmware():
    oid = nuevo_id("u", 1793287812.25)
    assert oid == "u-20261029-153012-250"
    assert len(oid) <= 40 and all(c.isalnum() or c in "-_." for c in oid)


def test_ciclo_de_una_orden(con):
    cfg = config.Config()
    publicados, nube_cola = [], []
    gestor = GestorOrdenes(con, cfg, lambda t, m, q: publicados.append((t, json.loads(m), q)),
                           lambda d, c: nube_cola.append((d, c)), Reloj())
    assert gestor.desde_ubidots(b"0.0") is None  # el control en 0 no es una orden
    oid = gestor.desde_ubidots(b"1.0")
    assert publicados == [("sabana/chia/chia-01/cmd/desactivar_alarma", publicados[0][1], 1)]
    assert publicados[0][1]["orden_id"] == oid
    assert nube_cola[-1] == ("gw-chia", {"desactivar_alarma": {"value": 0}})
    respuesta = Registro(con, Reloj()).procesar(
        "sabana/chia/chia-01/cmd/respuesta",
        json.dumps({"orden_id": oid, "resultado": "rechazada", "motivo": "estado NORMAL", "estado": "NORMAL"}))[1]
    gestor.al_responder(respuesta)
    fila = con.execute("SELECT * FROM ordenes WHERE orden_id = ?", (oid,)).fetchone()
    assert fila["resultado"] == "rechazada" and fila["motivo"] == "estado NORMAL"
    assert nube_cola[-1][1]["orden_resultado"]["value"] == 0


# ---------------------------------------------------------------- configuración
def test_config_lee_env(tmp_path):
    ruta = tmp_path / ".env"
    ruta.write_text("# comentario\nMQTT_HOST=192.168.1.50\nMQTT_PUERTO=1884\nUBIDOTS_TLS=0\nOTRA=x\n", encoding="utf-8")
    cfg = config.cargar(str(ruta))
    assert (cfg.mqtt_host, cfg.mqtt_puerto, cfg.ubidots_tls, cfg.nube_activa) == ("192.168.1.50", 1884, False, False)
    assert cfg.extras["OTRA"] == "x"
    assert cfg.suscripcion_local == "sabana/chia/+/#"
