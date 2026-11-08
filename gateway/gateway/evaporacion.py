"""VPD y ET0 de Hargreaves-Samani (FAO-56), con las mismas ecuaciones que el firmware (evaporacion.cpp).

- VPD: ec. 11 (Tetens) y ec. 19 → indicador de demanda evaporativa.
- Ra: ec. 21 a 25 → radiación extraterrestre diaria.
- ET0: ec. 52 (Hargreaves-Samani) → estimación diaria de evaporación potencial de referencia.
"""

import math

GSC_MJ_M2_MIN = 0.0820   # constante solar (FAO-56)
MJ_A_MM = 0.408          # 1 MJ m⁻² d⁻¹ = 0.408 mm/d


def presion_saturacion_kpa(t_c):
    """e°(T) en kPa (FAO-56, ec. 11)."""
    return 0.6108 * math.exp(17.27 * t_c / (t_c + 237.3))


def vpd_kpa(t_c, hr_pct):
    """Déficit de presión de vapor en kPa; None si los datos no son válidos."""
    if t_c is None or hr_pct is None or not 0.0 <= hr_pct <= 100.0:
        return None
    es = presion_saturacion_kpa(t_c)
    return es - es * hr_pct / 100.0


def radiacion_extraterrestre_mj(latitud_grados, dia_del_anio):
    """Ra en MJ m⁻² d⁻¹ (FAO-56, ec. 21 a 25)."""
    phi = math.radians(latitud_grados)
    x = 2.0 * math.pi * dia_del_anio / 365.0
    dr = 1.0 + 0.033 * math.cos(x)                       # ec. 23
    delta = 0.409 * math.sin(x - 1.39)                   # ec. 24
    omega_s = math.acos(-math.tan(phi) * math.tan(delta))  # ec. 25
    return (24.0 * 60.0 / math.pi) * GSC_MJ_M2_MIN * dr * (
        omega_s * math.sin(phi) * math.sin(delta) + math.cos(phi) * math.cos(delta) * math.sin(omega_s))


def et0_hargreaves_mm_dia(t_max_c, t_min_c, ra_mj):
    """ET0 en mm/d (FAO-56, ec. 52); None si los datos no son válidos."""
    if None in (t_max_c, t_min_c, ra_mj) or t_max_c < t_min_c:
        return None
    t_media = (t_max_c + t_min_c) / 2.0
    return 0.0023 * (t_media + 17.8) * math.sqrt(t_max_c - t_min_c) * ra_mj * MJ_A_MM
