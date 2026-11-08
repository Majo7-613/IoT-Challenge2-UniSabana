"""Detección de anomalías (docs/diseno/modulo-ia.md, sección 2.2).

Dos capas:
  1. Reglas físicas sobre la telemetría reciente: lectura fuera de rango, salto de nivel imposible
     y sensor congelado.
  2. Modelo no supervisado sobre los promedios de 5 min (tendencia, temperatura, humedad, VPD, UV y
     hora del día): Isolation Forest de scikit-learn, entrenado con el histórico de operación. Si scikit-learn no
     está instalado, un puntaje z robusto (mediana y MAD) con las mismas variables.
"""

import math

try:
    from sklearn.ensemble import IsolationForest
except ImportError:  # el gateway funciona sin scikit-learn
    IsolationForest = None

# Variables del modelo multivariable (promedios de 5 min).
VARIABLES_MODELO = ("tend_cm_min", "t_c", "hr_pct", "vpd_kpa", "uv_idx")

# Un nivel que cambia más que esto entre dos mensajes de 5 s no es físicamente posible en el
# recipiente de demostración (sería un vaciado de más de 120 %/min).
SALTO_MAX_PCT = 10.0

# Mensajes seguidos con el mismo valor exacto para considerar un sensor congelado (30 min a 5 s).
MENSAJES_CONGELADO = 360

MINIMO_ENTRENAMIENTO = 48   # 4 h de promedios de 5 min

# Fracción de anomalías esperada en el histórico: fija el umbral de Isolation Forest en el percentil 5
# de los puntajes de entrenamiento. Elegida con datos sintéticos (tests/datos/ia/evaluacion-sintetica.md):
# 0.05 dio el mejor equilibrio (precisión 0.82, exhaustividad 0.83); con el valor automático la
# precisión fue 0.45. Se recalibra con los datos medidos del prototipo.
CONTAMINACION = 0.05
UMBRAL_Z_ROBUSTO = 4.0


def reglas(filas):
    """Anomalías por reglas en la telemetría reciente (filas ordenadas por tiempo). Lista de tipos."""
    tipos = []
    if any(f["invalidos"] for f in filas[-12:]):          # último minuto
        tipos.append("lectura_fuera_de_rango")
    niveles = [f["nivel_pct"] for f in filas[-12:] if f["nivel_pct"] is not None]
    if any(abs(b - a) > SALTO_MAX_PCT for a, b in zip(niveles, niveles[1:])):
        tipos.append("salto_de_nivel")
    if len(filas) >= MENSAJES_CONGELADO:
        ventana = filas[-MENSAJES_CONGELADO:]
        for campo in ("t_c", "hr_pct", "p_hpa"):
            valores = {f[campo] for f in ventana}
            if len(valores) == 1 and None not in valores:
                tipos.append(f"sensor_congelado:{campo}")
    return tipos


def hora_local(fila):
    """Hora local (0–24) de un promedio: campo 'hora' o el inicio del periodo en UTC (Colombia, UTC−5)."""
    if "hora" in fila.keys():
        return fila["hora"]
    inicio = fila["inicio_utc"]
    return (int(inicio[11:13]) + int(inicio[14:16]) / 60.0 - 5.0) % 24.0


def _vector(fila):
    """Variables del modelo más la hora del día como seno y coseno (las 23 h y las 0 h quedan cerca)."""
    valores = [fila[v] for v in VARIABLES_MODELO]
    if any(v is None for v in valores):
        return None
    angulo = 2.0 * math.pi * hora_local(fila) / 24.0
    return [float(v) for v in valores] + [math.sin(angulo), math.cos(angulo)]


def entrenar(historico):
    """Entrena el modelo con el histórico (filas de agregados_5min). None si hay pocos datos.

    Devuelve ("isolation_forest", modelo) o, sin scikit-learn, ("z_robusto", [(mediana, escala), ...]).
    """
    datos = [v for v in (_vector(f) for f in historico) if v is not None]
    if len(datos) < MINIMO_ENTRENAMIENTO:
        return None
    if IsolationForest is not None:
        modelo = IsolationForest(n_estimators=100, contamination=CONTAMINACION, random_state=7)
        modelo.fit(datos)
        return "isolation_forest", modelo
    estadisticos = []
    for i in range(len(datos[0])):
        columna = sorted(d[i] for d in datos)
        mediana = columna[len(columna) // 2]
        mad = sorted(abs(v - mediana) for v in columna)[len(columna) // 2]
        estadisticos.append((mediana, 1.4826 * mad if mad > 0 else 1e-6))
    return "z_robusto", estadisticos


def clasificar(entrenado, actual):
    """(atípico, puntaje, método) de un promedio con un modelo de entrenar(); None si no se puede."""
    x_actual = _vector(actual)
    if entrenado is None or x_actual is None:
        return None
    metodo, modelo = entrenado
    if metodo == "isolation_forest":
        puntaje = float(modelo.score_samples([x_actual])[0])     # más negativo = más atípico
        return int(modelo.predict([x_actual])[0]) == -1, puntaje, metodo
    z_max = max(abs(x - mediana) / escala for x, (mediana, escala) in zip(x_actual, modelo))
    return z_max > UMBRAL_Z_ROBUSTO, -z_max, metodo


def puntaje_atipico(historico, actual):
    """¿El promedio actual es atípico frente al histórico? Devuelve (atípico, puntaje, método) o None.

    historico: filas de agregados_5min (sin la actual); actual: la fila más reciente.
    """
    if _vector(actual) is None:
        return None
    return clasificar(entrenar(historico), actual)


def es_finito(valor):
    return valor is not None and math.isfinite(valor)
