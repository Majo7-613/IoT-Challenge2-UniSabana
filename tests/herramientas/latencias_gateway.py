#!/usr/bin/env python3
"""
Latencias del ecosistema a partir de gateway.db (tests/protocolo-fisico.md, FIS-07 y FIS-08).

- FIS-07: para cada alerta, tiempo entre su registro en el gateway (`alertas.recibido_utc`) y la confirmación de
  Ubidots de la carga de estado correspondiente (`cola_nube.enviado_utc`).
- FIS-08: para cada orden de desactivar, tiempo entre su llegada desde Ubidots (`ordenes.recibida_utc`) y la
  respuesta del nodo (`ordenes.respuesta_utc`).

Escribe filas CSV con las columnas de la plantilla de la prueba (sin encabezado, para agregarlas al archivo) y un
resumen en la salida de errores.

Uso: python tests/herramientas/latencias_gateway.py RUTA/gateway.db --prueba FIS-07|FIS-08 [--version COMMIT]
"""

import argparse
import csv
import sqlite3
import statistics
import sys
from datetime import datetime, timezone


def momento(texto):
    return datetime.strptime(texto, "%Y-%m-%dT%H:%M:%S.%fZ").replace(tzinfo=timezone.utc)


def latencias_nube(con):
    """[(recibido_utc, segundos)] de cada alerta con su carga de estado enviada."""
    resultado = []
    cargas = con.execute(
        "SELECT creado_utc, enviado_utc FROM cola_nube WHERE carga LIKE '{\"estado\"%' AND enviado_utc IS NOT NULL"
        " ORDER BY id").fetchall()
    for recibido, in con.execute("SELECT recibido_utc FROM alertas ORDER BY id"):
        # La carga de estado se encola al registrar la alerta: la primera creada en ese instante o después.
        par = next((c for c in cargas if c[0] >= recibido), None)
        if par is not None:
            resultado.append((recibido, (momento(par[1]) - momento(recibido)).total_seconds()))
    return resultado


def latencias_ordenes(con):
    """[(orden_id, resultado, estado_nodo, recibida_utc, segundos)] de las órdenes con respuesta."""
    return [(oid, res, est, rec, (momento(resp) - momento(rec)).total_seconds())
            for oid, res, est, rec, resp in con.execute(
                "SELECT orden_id, resultado, estado_nodo, recibida_utc, respuesta_utc FROM ordenes"
                " WHERE respuesta_utc IS NOT NULL ORDER BY recibida_utc")]


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    p.add_argument("bd")
    p.add_argument("--prueba", choices=["FIS-07", "FIS-08"], required=True)
    p.add_argument("--version", default="")
    a = p.parse_args()
    for flujo in (sys.stdout, sys.stderr):
        if hasattr(flujo, "reconfigure"):
            flujo.reconfigure(encoding="utf-8")
    con = sqlite3.connect(a.bd)
    escritor = csv.writer(sys.stdout, lineterminator="\n")
    if a.prueba == "FIS-07":
        # Columnas de fis07-latencia-nube.csv: fecha_hora, version, grupo, esperado, observado, observaciones.
        datos = latencias_nube(con)
        for recibido, segundos in datos:
            escritor.writerow([recibido, a.version, "alerta → Ubidots", 5, f"{segundos:.3f}", ""])
        valores = [s for _, s in datos]
    else:
        # Columnas de fis08-orden.csv: fecha_hora, version, estado_nodo, orden_id, resultado_esperado,
        # resultado_observado, latencia_s, buzzer_silenciado, observaciones. El resultado esperado y el
        # buzzer los anota quien hace la prueba.
        datos = latencias_ordenes(con)
        for oid, resultado, estado, recibida, segundos in datos:
            escritor.writerow([recibida, a.version, estado or "", oid, "", resultado, f"{segundos:.3f}", "", ""])
        valores = [d[-1] for d in datos]
    if valores:
        print(f"{a.prueba}: n = {len(valores)}, media = {statistics.fmean(valores):.3f} s, máximo = {max(valores):.3f} s",
              file=sys.stderr)
    else:
        print(f"{a.prueba}: sin datos", file=sys.stderr)


if __name__ == "__main__":
    main()
