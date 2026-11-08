"""Recomendación en texto para las autoridades (docs/diseno/modulo-ia.md, sección 2.3).

La IA generativa solo redacta: recibe resultados ya calculados (estado, horas hasta el nivel
crítico, anomalías, ET0) y devuelve un texto breve. Nunca decide ni calcula. Sin clave de API,
sin Internet o ante cualquier error, se usa el texto de respaldo armado con reglas.
"""

import json
import logging
import urllib.error
import urllib.request

log = logging.getLogger("gateway.ia")

URL_GEMINI = "https://generativelanguage.googleapis.com/v1beta/models/{modelo}:generateContent"
LIMITE_CARACTERES = 400   # el contexto de un dato de Ubidots admite 1 KB

INSTRUCCION = (
    "Eres un asistente técnico para autoridades de un acueducto veredal en Chía (Colombia). "
    "Redacta en español, en máximo 3 frases y sin listas, una recomendación operativa a partir de los "
    "datos que siguen. Reglas: usa solo los valores dados, no inventes cifras ni causas, cita las horas "
    "hasta el nivel crítico si existen, y menciona si hay lecturas anómalas que deban revisarse en sitio. "
    "Empieza con 'Sugerencia:'.")


def resumen(datos):
    """Datos que se envían al modelo (y que el texto de respaldo usa), sin información personal."""
    return {k: datos.get(k) for k in ("estado", "nivel_pct", "horas_hasta_critico", "horas_min", "horas_max",
                                      "anomalias", "et0_mm_dia")}


def texto_respaldo(datos):
    """Recomendación por reglas, sin API."""
    partes = []
    estado = datos.get("estado") or "desconocido"
    horas = datos.get("horas_hasta_critico")
    if estado == "CRITICO" or horas == 0:
        partes.append("Sugerencia: el tanque está en nivel crítico; activar el abastecimiento alterno y el racionamiento.")
    elif horas is not None:
        rango = ""
        if datos.get("horas_min") is not None and datos.get("horas_max") is not None:
            rango = f" (entre {datos['horas_min']:.1f} y {datos['horas_max']:.1f} h)"
        partes.append(f"Sugerencia: al ritmo actual el nivel crítico se alcanzaría en {horas:.1f} h{rango}; "
                      "preparar medidas de racionamiento.")
    elif estado in ("ALERTA", "ADVERTENCIA"):
        partes.append(f"Sugerencia: estado {estado}; vigilar el nivel y las condiciones de calor.")
    else:
        partes.append("Sugerencia: sin descenso significativo del nivel; operación normal.")
    if datos.get("anomalias"):
        partes.append("Hay lecturas anómalas (" + ", ".join(datos["anomalias"]) + "); revisar los sensores en sitio.")
    return " ".join(partes)[:LIMITE_CARACTERES]


def con_gemini(datos, clave, modelo, tiempo_maximo=10):
    """Texto de Gemini, o None si falla. La clave viaja en el encabezado, nunca en la URL ni en el registro."""
    cuerpo = {"contents": [{"parts": [{"text": INSTRUCCION + "\nDatos: " + json.dumps(resumen(datos), ensure_ascii=False)}]}],
              "generationConfig": {"maxOutputTokens": 150, "temperature": 0.2}}
    peticion = urllib.request.Request(
        URL_GEMINI.format(modelo=modelo), data=json.dumps(cuerpo).encode("utf-8"),
        headers={"Content-Type": "application/json", "x-goog-api-key": clave}, method="POST")
    try:
        with urllib.request.urlopen(peticion, timeout=tiempo_maximo) as respuesta:
            datos_respuesta = json.loads(respuesta.read().decode("utf-8"))
        texto = datos_respuesta["candidates"][0]["content"]["parts"][0]["text"].strip()
    except (urllib.error.URLError, TimeoutError, KeyError, IndexError, ValueError) as error:
        log.warning("Gemini no disponible (%s): se usa el texto de respaldo", type(error).__name__)
        return None
    return " ".join(texto.split())[:LIMITE_CARACTERES]


def recomendar(datos, clave="", modelo="gemini-2.0-flash"):
    """(texto, origen): origen es 'gemini' o 'reglas'."""
    if clave:
        texto = con_gemini(datos, clave, modelo)
        if texto:
            return texto, "gemini"
    return texto_respaldo(datos), "reglas"
