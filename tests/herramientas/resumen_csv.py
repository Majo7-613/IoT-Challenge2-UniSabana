#!/usr/bin/env python3
"""
Resumen de un CSV del banco de pruebas físico para la Wiki (tests/protocolo-fisico.md).

El CSV debe tener las columnas `grupo`, `esperado` y `observado` (más las que la prueba necesite).
Para cada grupo (por ejemplo, cada nivel de referencia) calcula la media observada, el error medio
(observado − esperado), el número de repeticiones y la desviación estándar, y escribe una tabla Markdown
con las columnas que exige la Wiki.

Uso: python tests/herramientas/resumen_csv.py RUTA.csv [--unidad cm] [--url-base URL]
"""

import argparse
import csv
import statistics
import sys
from collections import OrderedDict
from pathlib import Path

URL_BASE = "https://github.com/Majo7-613/IoT-Challenge2-UniSabana/blob/main/"


def numero(texto):
    return float(texto.replace(",", "."))


def resumir(filas):
    """{grupo: (esperado, media, error medio, n, desviación)} en el orden de aparición."""
    grupos = OrderedDict()
    for f in filas:
        grupos.setdefault(f["grupo"], []).append(f)
    resumen = OrderedDict()
    for grupo, lista in grupos.items():
        esperados = {numero(f["esperado"]) if f.get("esperado", "").strip() else None for f in lista}
        if len(esperados) != 1:
            raise ValueError(f"el grupo {grupo!r} tiene más de un valor esperado: {sorted(esperados, key=str)}")
        esperado = esperados.pop()   # None si la prueba solo reporta un valor (por ejemplo, el consumo)
        observados = [numero(f["observado"]) for f in lista]
        media = statistics.fmean(observados)
        desviacion = statistics.stdev(observados) if len(observados) > 1 else 0.0
        error = None if esperado is None else media - esperado
        resumen[grupo] = (esperado, media, error, len(observados), desviacion)
    return resumen


def tabla(resumen, unidad, enlace):
    lineas = ["| Grupo | Esperado | Observado (media) | Error medio | Repeticiones | Desviación estándar | Datos |",
              "| :--- | ---: | ---: | ---: | ---: | ---: | :--- |"]
    for grupo, (esperado, media, error, n, desv) in resumen.items():
        txt_esperado = "—" if esperado is None else f"{esperado:.2f} {unidad}"
        txt_error = "—" if error is None else f"{error:+.2f} {unidad}"
        lineas.append(f"| {grupo} | {txt_esperado} | {media:.2f} {unidad} | {txt_error} | {n} | "
                      f"{desv:.2f} {unidad} | {enlace} |")
    return "\n".join(lineas)


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    p.add_argument("csv")
    p.add_argument("--unidad", default="")
    p.add_argument("--url-base", default=URL_BASE)
    a = p.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    ruta = Path(a.csv)
    with ruta.open(encoding="utf-8-sig", newline="") as archivo:
        filas = [f for f in csv.DictReader(archivo) if f.get("observado", "").strip()]
    if not filas:
        sys.exit("el CSV no tiene lecturas en la columna observado")
    relativa = ruta.resolve().as_posix()
    if "/tests/" in relativa:
        relativa = "tests/" + relativa.split("/tests/", 1)[1]
    print(tabla(resumir(filas), a.unidad, f"[{ruta.name}]({a.url_base}{relativa})"))


if __name__ == "__main__":
    main()
