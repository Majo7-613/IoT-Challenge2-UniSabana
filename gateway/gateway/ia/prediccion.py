"""Predicción del tiempo hasta el nivel crítico (docs/diseno/modulo-ia.md, sección 2.1).

Regresión lineal por mínimos cuadrados del nivel (%) contra el tiempo, sobre los promedios de
5 min de las últimas 6 h. Si la pendiente es negativa y su intervalo de confianza del 95 % no
incluye el cero, las horas restantes son (nivel actual − umbral crítico) / |pendiente|; el
intervalo de la pendiente da el rango de horas.
"""

import math

# Valores críticos de t de Student (dos colas, 95 %) por grados de libertad; ≥ 30 → 2.04 … 1.96.
_T_95 = {1: 12.71, 2: 4.30, 3: 3.18, 4: 2.78, 5: 2.57, 6: 2.45, 7: 2.36, 8: 2.31, 9: 2.26, 10: 2.23,
         12: 2.18, 15: 2.13, 20: 2.09, 25: 2.06, 30: 2.04, 40: 2.02, 60: 2.00, 120: 1.98}


def t_critico_95(gl):
    """t de Student del 95 % (dos colas) para gl grados de libertad; toma el valor tabulado inferior más cercano."""
    if gl <= 0:
        return math.inf
    claves = [k for k in sorted(_T_95) if k <= gl]
    return _T_95[claves[-1]] if gl < 120 else 1.96


def regresion(puntos):
    """Mínimos cuadrados de y = a + b·x. Devuelve (a, b, error estándar de b) o None si no se puede."""
    n = len(puntos)
    if n < 3:
        return None
    mx = sum(x for x, _ in puntos) / n
    my = sum(y for _, y in puntos) / n
    sxx = sum((x - mx) ** 2 for x, _ in puntos)
    if sxx == 0:
        return None
    b = sum((x - mx) * (y - my) for x, y in puntos) / sxx
    a = my - b * mx
    residuos = sum((y - (a + b * x)) ** 2 for x, y in puntos)
    error_b = math.sqrt(residuos / (n - 2) / sxx)
    return a, b, error_b


def horas_hasta_critico(puntos, umbral_pct, minimo_puntos=6):
    """Predicción a partir de puntos (horas, nivel %), ordenados por tiempo.

    Devuelve un dict con horas, horas_min, horas_max, pendiente_pct_h, n y nivel_actual, o None si no
    hay datos suficientes. horas es None si el nivel no baja de forma significativa; es 0 si el
    nivel ya está en el umbral o por debajo.
    """
    puntos = [(x, y) for x, y in puntos if y is not None]
    if len(puntos) < minimo_puntos:
        return None
    ajuste = regresion(puntos)
    if ajuste is None:
        return None
    a, b, error_b = ajuste
    t = t_critico_95(len(puntos) - 2)
    b_alto, b_bajo = b + t * error_b, b - t * error_b   # b_alto es la pendiente menos negativa
    x_actual = puntos[-1][0]
    nivel_actual = a + b * x_actual                       # nivel estimado por la recta, menos ruidoso
    resultado = {"pendiente_pct_h": b, "n": len(puntos), "nivel_actual": nivel_actual,
                 "horas": None, "horas_min": None, "horas_max": None}
    if nivel_actual <= umbral_pct:
        resultado.update(horas=0.0, horas_min=0.0, horas_max=0.0)
        return resultado
    if b_alto >= 0:   # el intervalo incluye el cero: no hay descenso significativo
        return resultado
    margen = nivel_actual - umbral_pct
    resultado.update(horas=margen / -b, horas_min=margen / -b_bajo, horas_max=margen / -b_alto)
    return resultado
