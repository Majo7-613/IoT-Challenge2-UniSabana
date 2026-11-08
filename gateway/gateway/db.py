"""Base de datos SQLite del gateway (esquema de docs/diseno/gateway.md, sección 3)."""

import sqlite3
from datetime import datetime, timezone

ESQUEMA = """
CREATE TABLE IF NOT EXISTS telemetria (
    id                 INTEGER PRIMARY KEY,
    nodo               TEXT    NOT NULL,
    msg_id             INTEGER NOT NULL,
    recibido_utc       TEXT    NOT NULL,
    epoca_nodo         INTEGER,
    ciclo              INTEGER,
    estado             TEXT    NOT NULL,
    causas             TEXT,
    alarma_desactivada INTEGER NOT NULL,
    nivel_cm REAL, nivel_pct REAL, tend_cm_min REAL,
    t_c REAL, hr_pct REAL, p_hpa REAL, uv_idx REAL,
    vpd_kpa REAL, et0_mm_dia REAL,
    sensores           TEXT,
    rssi               INTEGER,
    invalidos          TEXT
);
CREATE INDEX IF NOT EXISTS ix_telemetria_nodo_tiempo ON telemetria (nodo, recibido_utc);

CREATE TABLE IF NOT EXISTS alertas (
    id           INTEGER PRIMARY KEY,
    nodo         TEXT    NOT NULL,
    msg_id       INTEGER NOT NULL,
    recibido_utc TEXT    NOT NULL,
    epoca_nodo   INTEGER,
    anterior     TEXT,
    estado       TEXT    NOT NULL,
    causas       TEXT,
    nivel_pct    REAL,
    tend_cm_min  REAL,
    UNIQUE (nodo, msg_id, epoca_nodo)
);

CREATE TABLE IF NOT EXISTS conexion_nodo (
    id           INTEGER PRIMARY KEY,
    nodo         TEXT NOT NULL,
    recibido_utc TEXT NOT NULL,
    estado       TEXT NOT NULL CHECK (estado IN ('online', 'offline'))
);

CREATE TABLE IF NOT EXISTS ordenes (
    orden_id      TEXT PRIMARY KEY,
    nodo          TEXT NOT NULL,
    origen        TEXT NOT NULL,
    recibida_utc  TEXT NOT NULL,
    enviada_utc   TEXT,
    respuesta_utc TEXT,
    resultado     TEXT CHECK (resultado IN ('aceptada', 'rechazada')),
    motivo        TEXT,
    estado_nodo   TEXT
);

CREATE TABLE IF NOT EXISTS agregados_5min (
    id            INTEGER PRIMARY KEY,
    nodo          TEXT    NOT NULL,
    inicio_utc    TEXT    NOT NULL,
    fin_utc       TEXT    NOT NULL,
    n_mensajes    INTEGER NOT NULL,
    n_esperados   INTEGER NOT NULL,
    nivel_cm REAL, nivel_pct REAL, tend_cm_min REAL,
    t_c REAL, hr_pct REAL, p_hpa REAL, uv_idx REAL, vpd_kpa REAL,
    estado_peor   TEXT,
    UNIQUE (nodo, inicio_utc)
);

CREATE TABLE IF NOT EXISTS et0_diaria (
    nodo        TEXT NOT NULL,
    fecha       TEXT NOT NULL,
    t_max_c     REAL NOT NULL,
    t_min_c     REAL NOT NULL,
    ra_mj_m2_d  REAL NOT NULL,
    et0_mm_dia  REAL NOT NULL,
    n_muestras  INTEGER NOT NULL,
    PRIMARY KEY (nodo, fecha)
);

CREATE TABLE IF NOT EXISTS ia_resultados (
    id                   INTEGER PRIMARY KEY,
    nodo                 TEXT NOT NULL,
    calculado_utc        TEXT NOT NULL,
    horas_hasta_critico  REAL,
    anomalia             INTEGER NOT NULL,
    tipo_anomalia        TEXT,
    detalle              TEXT
);

CREATE TABLE IF NOT EXISTS cola_nube (
    id           INTEGER PRIMARY KEY,
    creado_utc   TEXT    NOT NULL,
    dispositivo  TEXT    NOT NULL,
    carga        TEXT    NOT NULL,
    intentos     INTEGER NOT NULL DEFAULT 0,
    enviado_utc  TEXT
);
CREATE INDEX IF NOT EXISTS ix_cola_pendiente ON cola_nube (enviado_utc, id);
"""


def ahora_utc():
    """Hora actual en UTC, ISO 8601 con milisegundos (ordenable como texto)."""
    return a_texto(datetime.now(timezone.utc))


def a_texto(momento):
    """datetime → texto ISO 8601 en UTC con milisegundos y sufijo Z."""
    return momento.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


def de_texto(texto):
    """Texto ISO 8601 (con Z) → datetime en UTC."""
    return datetime.strptime(texto, "%Y-%m-%dT%H:%M:%S.%fZ").replace(tzinfo=timezone.utc)


def conectar(ruta):
    """Abre (o crea) la base, con el esquema y modo WAL para lecturas mientras se escribe."""
    con = sqlite3.connect(ruta, check_same_thread=False)
    con.row_factory = sqlite3.Row
    if ruta != ":memory:":
        con.execute("PRAGMA journal_mode=WAL")
    con.executescript(ESQUEMA)
    con.commit()
    return con
