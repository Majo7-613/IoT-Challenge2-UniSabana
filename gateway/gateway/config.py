"""Configuración del gateway, leída de un archivo .env y de las variables de entorno.

Las credenciales (claves MQTT, token de Ubidots, clave de la API de IA) viven solo en el
archivo .env, que está en .gitignore. La plantilla es .env.example.
"""

import os
from dataclasses import dataclass, field


def leer_env(ruta):
    """Lee un archivo KEY=VALOR (líneas vacías y comentarios con # se ignoran)."""
    valores = {}
    if not ruta or not os.path.exists(ruta):
        return valores
    with open(ruta, encoding="utf-8") as archivo:
        for linea in archivo:
            linea = linea.strip()
            if not linea or linea.startswith("#") or "=" not in linea:
                continue
            clave, valor = linea.split("=", 1)
            valores[clave.strip()] = valor.strip().strip('"').strip("'")
    return valores


@dataclass
class Config:
    """Parámetros del gateway. Los valores por omisión sirven para el computador de desarrollo."""

    # Broker local (Mosquitto en la Raspberry Pi o en el computador).
    mqtt_host: str = "localhost"
    mqtt_puerto: int = 1883
    mqtt_usuario: str = ""
    mqtt_clave: str = ""
    raiz: str = "sabana"
    municipio: str = "chia"
    nodo: str = "chia-01"            # nodo al que se envían las órdenes

    # Base de datos.
    bd_ruta: str = "gateway.db"

    # Procesamiento.
    periodo_agregado_s: int = 300    # promedios de 5 min hacia Ubidots
    periodo_telemetria_s: int = 5    # periodo del nodo (para los mensajes esperados)
    latitud_grados: float = 4.86     # Chía (igual que SITIO_LATITUD_GRADOS del firmware)
    cobertura_minima_et0: float = 0.75  # fracción mínima de mensajes del día para calcular la ET0
    umbral_critico_pct: float = 20.0    # umbral crítico de demostración del firmware

    # Ubidots (vacío el token = envío a la nube desactivado; los datos quedan en la cola).
    ubidots_token: str = ""
    ubidots_host: str = "industrial.api.ubidots.com"
    ubidots_puerto: int = 8883
    ubidots_tls: bool = True
    ubidots_topico_publicar: str = "/v1.6/devices/{dispositivo}"
    ubidots_topico_orden: str = "/v1.6/devices/{dispositivo}/desactivar_alarma/lv"
    ubidots_dispositivo_nodo: str = "chia-01"
    ubidots_dispositivo_gateway: str = "gw-chia"

    # Módulo de IA (recomendación en texto; vacío = texto de respaldo sin API).
    gemini_api_key: str = ""
    gemini_modelo: str = "gemini-2.0-flash"

    extras: dict = field(default_factory=dict)

    @property
    def suscripcion_local(self):
        """Todos los tópicos de los nodos del municipio."""
        return f"{self.raiz}/{self.municipio}/+/#"

    def topico_nodo(self, tipo, nodo=None):
        """sabana/{municipio}/{nodo}/{tipo}."""
        return f"{self.raiz}/{self.municipio}/{nodo or self.nodo}/{tipo}"

    @property
    def nube_activa(self):
        return bool(self.ubidots_token)


_CONVERSION = {
    "MQTT_HOST": ("mqtt_host", str), "MQTT_PUERTO": ("mqtt_puerto", int),
    "MQTT_USUARIO": ("mqtt_usuario", str), "MQTT_CLAVE": ("mqtt_clave", str),
    "MQTT_RAIZ": ("raiz", str), "MUNICIPIO": ("municipio", str), "NODO": ("nodo", str),
    "BD_RUTA": ("bd_ruta", str),
    "PERIODO_AGREGADO_S": ("periodo_agregado_s", int), "PERIODO_TELEMETRIA_S": ("periodo_telemetria_s", int),
    "LATITUD_GRADOS": ("latitud_grados", float), "COBERTURA_MINIMA_ET0": ("cobertura_minima_et0", float),
    "UMBRAL_CRITICO_PCT": ("umbral_critico_pct", float),
    "UBIDOTS_TOKEN": ("ubidots_token", str), "UBIDOTS_HOST": ("ubidots_host", str),
    "UBIDOTS_PUERTO": ("ubidots_puerto", int), "UBIDOTS_TLS": ("ubidots_tls", lambda v: v not in ("0", "false", "no")),
    "UBIDOTS_TOPICO_PUBLICAR": ("ubidots_topico_publicar", str),
    "UBIDOTS_TOPICO_ORDEN": ("ubidots_topico_orden", str),
    "UBIDOTS_DISPOSITIVO_NODO": ("ubidots_dispositivo_nodo", str),
    "UBIDOTS_DISPOSITIVO_GATEWAY": ("ubidots_dispositivo_gateway", str),
    "GEMINI_API_KEY": ("gemini_api_key", str), "GEMINI_MODELO": ("gemini_modelo", str),
}


def cargar(ruta_env=".env"):
    """Config con los valores del .env, sobrescritos por las variables de entorno del mismo nombre."""
    valores = leer_env(ruta_env)
    valores.update({k: v for k, v in os.environ.items() if k in _CONVERSION})
    cfg = Config()
    for clave, valor in valores.items():
        if clave in _CONVERSION:
            atributo, tipo = _CONVERSION[clave]
            setattr(cfg, atributo, tipo(valor))
        else:
            cfg.extras[clave] = valor
    return cfg
