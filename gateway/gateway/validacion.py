"""Validación por rango de cada variable (docs/diseno/gateway.md, sección 2).

Un valor fuera de rango se guarda, pero se marca como inválido y no entra a los promedios
ni al módulo de IA.
"""

# Campo de la telemetría → (mínimo, máximo), según la hoja de datos de cada sensor.
RANGOS = {
    "nivel_cm": (0.0, 400.0),     # HC-SR04: 2 a 400 cm de distancia
    "nivel_pct": (0.0, 110.0),    # 10 % adicional por oleaje y tolerancia de montaje
    "t_c": (-40.0, 80.0),         # DHT22
    "hr_pct": (0.0, 100.0),       # DHT22
    "p_hpa": (300.0, 1100.0),     # BMP180
    "uv_idx": (0.0, 15.0),        # escala de la OMS: 11 o más es extremo
}

# Variables numéricas que se promedian y se envían a la nube.
VARIABLES = ("nivel_cm", "nivel_pct", "tend_cm_min", "t_c", "hr_pct", "p_hpa", "uv_idx", "vpd_kpa")


def es_numero(valor):
    """True si el valor es un número real (no booleano)."""
    return isinstance(valor, (int, float)) and not isinstance(valor, bool)


def campos_invalidos(telemetria):
    """Lista de campos presentes con un valor no numérico o fuera de su rango."""
    invalidos = []
    for campo in VARIABLES:
        valor = telemetria.get(campo)
        if valor is None:
            continue
        if not es_numero(valor):
            invalidos.append(campo)
            continue
        if campo in RANGOS:
            minimo, maximo = RANGOS[campo]
            if not minimo <= valor <= maximo:
                invalidos.append(campo)
    return invalidos
