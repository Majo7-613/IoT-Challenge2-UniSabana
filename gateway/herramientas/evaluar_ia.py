#!/usr/bin/env python3
"""
Evaluación del módulo de IA con datos sintéticos de respuesta conocida (SIMULACIÓN).

Predicción: descensos con pendiente conocida y ruido de medición; compara las horas predichas a
las 6 h con el momento real en que la recta verdadera cruza el umbral crítico.
  - Escenario lineal: consumo constante.
  - Escenario con patrón diario: el consumo varía ±50 % a lo largo del día (límite del modelo lineal).
Anomalías: 7 días de promedios de 5 min de operación normal (ciclo diario de temperatura, humedad,
UV y VPD por Tetens) y un conjunto de prueba con combinaciones normales y anómalas conocidas.

Uso, desde Repo/gateway:  .venv/Scripts/python herramientas/evaluar_ia.py [--salida RUTA.md]
"""

import argparse
import math
import random
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gateway.evaporacion import vpd_kpa  # noqa: E402
from gateway.ia import anomalias, prediccion  # noqa: E402

UMBRAL = 20.0
SEMILLA = 2026


def escenario_prediccion(azar, patron_diario, ventana_h=6):
    """(puntos de 6 h, horas reales hasta el umbral) o None si ya está bajo el umbral."""
    pendiente = azar.uniform(0.5, 4.0)          # %/h
    inicio = azar.uniform(60.0, 95.0) + (pendiente * (ventana_h - 6) if ventana_h > 6 else 0.0)
    fase = azar.uniform(0, 2 * math.pi)

    def nivel(h):
        if not patron_diario:
            return inicio - pendiente * h
        # Consumo = pendiente × (1 + 0.5 sen(2π h / 24 + fase)), integrado analíticamente.
        return inicio - pendiente * (h - 0.5 * 24 / (2 * math.pi) * (math.cos(2 * math.pi * h / 24 + fase) - math.cos(fase)))

    n = int(ventana_h * 12)
    puntos = [(i / 12, nivel(i / 12) + azar.gauss(0, 0.5)) for i in range(n)]
    actual = (n - 1) / 12
    if nivel(actual) <= UMBRAL:
        return None
    # Momento real del cruce, por búsqueda con paso de 1 min.
    h = actual
    while nivel(h) > UMBRAL:
        h += 1 / 60
    return puntos, h - actual


def evaluar_prediccion(patron_diario, ventana_h=6, casos=200):
    azar = random.Random(SEMILLA + patron_diario)
    errores, dentro, sin_prediccion = [], 0, 0
    reales = []
    while len(errores) + sin_prediccion < casos:
        e = escenario_prediccion(azar, patron_diario, ventana_h)
        if e is None:
            continue
        puntos, real = e
        r = prediccion.horas_hasta_critico(puntos, UMBRAL)
        if r is None or r["horas"] is None:
            sin_prediccion += 1
            continue
        errores.append(r["horas"] - real)
        reales.append(real)
        dentro += r["horas_min"] <= real <= r["horas_max"]
    mae = statistics.fmean(abs(e) for e in errores)
    mape = statistics.fmean(abs(e) / r for e, r in zip(errores, reales)) * 100
    return {"casos": casos, "con_prediccion": len(errores), "mae_h": mae, "mape_pct": mape,
            "sesgo_h": statistics.fmean(errores), "cobertura_pct": 100 * dentro / len(errores),
            "horas_reales_media": statistics.fmean(reales)}


def promedio_normal(azar, hora):
    t = 16 + 4 * math.sin(2 * math.pi * (hora - 9) / 24) + azar.gauss(0, 0.8)
    hr = min(100, max(30, 75 - 2.5 * (t - 16) + azar.gauss(0, 4)))
    uv = max(0.0, 8 * math.sin(math.pi * (hora - 6) / 12)) if 6 <= hora <= 18 else 0.0
    return {"tend_cm_min": azar.gauss(-0.05, 0.03), "t_c": t, "hr_pct": hr, "vpd_kpa": vpd_kpa(t, hr),
            "uv_idx": max(0.0, uv + azar.gauss(0, 0.3)), "hora": hora}


def anomalo(azar, hora, tipo):
    f = promedio_normal(azar, hora)
    if tipo == "descenso_rapido_sin_calor":
        f["tend_cm_min"] = azar.uniform(-3.0, -1.5)
    elif tipo == "uv_de_noche":
        f["uv_idx"] = azar.uniform(6, 10)
    elif tipo == "calor_nocturno":
        f["t_c"] = azar.uniform(28, 32)
        f["vpd_kpa"] = vpd_kpa(f["t_c"], f["hr_pct"])
    elif tipo == "vpd_incoherente":
        f["vpd_kpa"] = azar.uniform(2.5, 4.0)
    return f


def evaluar_anomalias():
    azar = random.Random(SEMILLA)
    historico = [promedio_normal(azar, (i * 5 / 60) % 24) for i in range(2016)]   # 7 días
    prueba = [(promedio_normal(azar, azar.uniform(0, 24)), False) for _ in range(200)]
    tipos = ["descenso_rapido_sin_calor", "uv_de_noche", "calor_nocturno", "vpd_incoherente"]
    por_tipo = {}
    for tipo in tipos:
        for _ in range(15):
            hora = azar.uniform(0, 4) if tipo in ("uv_de_noche", "calor_nocturno") else azar.uniform(0, 24)
            prueba.append((anomalo(azar, hora, tipo), tipo))
    vp = fp = fn = vn = 0
    entrenado = anomalias.entrenar(historico)   # una sola vez: el gateway reentrena cada 5 min con el mismo histórico
    metodo = entrenado[0]
    for fila, etiqueta in prueba:
        atipico, _, _ = anomalias.clasificar(entrenado, fila)
        if etiqueta:
            por_tipo.setdefault(etiqueta, [0, 0])
            por_tipo[etiqueta][1] += 1
            if atipico:
                vp += 1
                por_tipo[etiqueta][0] += 1
            else:
                fn += 1
        else:
            fp += atipico
            vn += not atipico
    return {"metodo": metodo, "normales": vn + fp, "anomalos": vp + fn, "vp": vp, "fp": fp, "fn": fn, "vn": vn,
            "precision": vp / (vp + fp) if vp + fp else float("nan"), "exhaustividad": vp / (vp + fn),
            "por_tipo": por_tipo}


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    p.add_argument("--salida", help="archivo Markdown con los resultados")
    a = p.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    resultados = [(f"{nombre}, ventana de {v} h", evaluar_prediccion(diario, v))
                  for nombre, diario in (("Consumo constante", False), ("Consumo con patrón diario (±50 %)", True))
                  for v in (6, 24)]
    anom = evaluar_anomalias()

    lineas = [
        "# Evaluación del módulo de IA con datos sintéticos (SIMULACIÓN)",
        "",
        f"Generado con `gateway/herramientas/evaluar_ia.py` (semilla {SEMILLA}). Los datos son sintéticos y de respuesta",
        "conocida; la evaluación con datos medidos se hace con el prototipo físico.",
        "",
        "## Predicción del tiempo hasta el nivel crítico",
        "",
        "Descensos con pendiente uniforme entre 0.5 y 4 %/h, nivel inicial entre 60 y 95 %, ruido de medición σ = 0.5 %,",
        "umbral crítico 20 %; predicción con los promedios de 5 min de la ventana (6 h = 72; 24 h = 288).",
        "",
        "| Escenario | Casos | Con predicción | Error absoluto medio (h) | Error relativo medio | Sesgo (h) | Valor real dentro del intervalo | Horas reales (media) |",
        "| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for nombre, r in resultados:
        lineas.append(f"| {nombre} | {r['casos']} | {r['con_prediccion']} | {r['mae_h']:.2f} | {r['mape_pct']:.1f} % | "
                      f"{r['sesgo_h']:+.2f} | {r['cobertura_pct']:.0f} % | {r['horas_reales_media']:.1f} |")
    lineas += [
        "",
        "## Detección de anomalías (modelo multivariable)",
        "",
        f"Método: `{anom['metodo']}`. Entrenamiento: 2 016 promedios de 5 min (7 días) de operación normal sintética.",
        f"Prueba: {anom['normales']} promedios normales y {anom['anomalos']} anómalos (15 de cada tipo).",
        "",
        "| Métrica | Valor |",
        "| :--- | ---: |",
        f"| Verdaderos positivos | {anom['vp']} |",
        f"| Falsos positivos | {anom['fp']} |",
        f"| Falsos negativos | {anom['fn']} |",
        f"| Verdaderos negativos | {anom['vn']} |",
        f"| Precisión | {anom['precision']:.2f} |",
        f"| Exhaustividad (recall) | {anom['exhaustividad']:.2f} |",
        "",
        "| Tipo de anomalía | Detectadas |",
        "| :--- | ---: |",
    ]
    for tipo, (detectadas, total) in anom["por_tipo"].items():
        lineas.append(f"| {tipo} | {detectadas} de {total} |")
    lineas += [
        "",
        "## Lectura de los resultados",
        "",
        "- Con consumo constante, la regresión estima bien el tiempo restante; la ventana de 24 h reduce el error.",
        "- Con un consumo que cambia a lo largo del día, la ventana de 6 h ve solo una parte del ciclo y el error crece;",
        "  la ventana de 24 h promedia el ciclo y reduce el error, por eso el gateway usa 24 h cuando las tiene.",
        "- El intervalo de confianza supone residuos independientes; con un patrón diario los residuos están",
        "  correlacionados y el intervalo resulta demasiado estrecho: se presenta como orientativo, no como garantía.",
        "- El umbral de anomalías (contaminación 0.05) se eligió comparando 0.01, 0.02, 0.03, 0.05 y el valor automático",
        "  sobre este mismo conjunto sintético; debe recalibrarse con datos medidos del prototipo.",
        "- El descenso rápido sin calor es la anomalía menos detectada por el modelo; la fusión del firmware ya alerta",
        "  por descenso sostenido, así que el modelo es una capa adicional, no la única.",
    ]
    texto = "\n".join(lineas) + "\n"
    print(texto)
    if a.salida:
        Path(a.salida).write_text(texto, encoding="utf-8")


if __name__ == "__main__":
    main()
