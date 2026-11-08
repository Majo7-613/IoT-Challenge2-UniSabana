# Gateway — Challenge #3

Programa en Python que corre en la Raspberry Pi 5: recibe por MQTT los mensajes del nodo ESP32 (broker Mosquitto local),
los guarda en SQLite, los procesa y envía a Ubidots promedios de 5 min, cambios de estado, ET0 diaria y resultados del
módulo de IA. Recibe de Ubidots la orden de desactivar la alarma y la reenvía al nodo.

Diseño: [comunicacion-mqtt.md](../docs/diseno/comunicacion-mqtt.md), [gateway.md](../docs/diseno/gateway.md),
[modulo-ia.md](../docs/diseno/modulo-ia.md). Instalación en la Raspberry Pi: [raspberry-pi.md](../docs/guias/raspberry-pi.md).

## Estructura

```text
gateway/
├── gateway/                 Paquete del gateway
│   ├── config.py            Configuración desde .env
│   ├── db.py                Esquema SQLite y utilidades de tiempo
│   ├── validacion.py        Rangos válidos de cada variable
│   ├── registro.py          Registro de telemetría, alertas, conexión y respuestas
│   ├── procesamiento.py     Promedios, mensajes perdidos y ET0 diaria
│   ├── evaporacion.py       VPD, Ra y ET0 (FAO-56), mismas ecuaciones que el firmware
│   ├── nube.py              Cola persistente y cliente MQTT de Ubidots
│   ├── ordenes.py           Orden de desactivar la alarma
│   ├── ia/                  Módulo de IA (predicción, anomalías, recomendación)
│   └── principal.py         Programa principal
├── herramientas/            Publicador de prueba que imita al nodo
├── despliegue/              Configuración de Mosquitto y servicio systemd para la Raspberry Pi
├── tests/                   Pruebas automáticas (pytest)
├── requirements.txt
└── .env.example             Plantilla de configuración (copiar como .env)
```

## Uso en el computador de desarrollo (Windows)

```bash
cd gateway
python -m venv .venv
.venv/Scripts/python -m pip install -r requirements.txt
cp .env.example .env                 # MQTT_HOST=localhost; UBIDOTS_TOKEN vacío desactiva la nube
.venv/Scripts/python -m pytest       # pruebas automáticas
.venv/Scripts/python -m gateway.principal
```

Con Mosquitto instalado en el computador, el publicador de prueba reemplaza al ESP32:

```bash
.venv/Scripts/python herramientas/publicador_prueba.py --escenario descenso --periodo 1
```

`.env`, la base `gateway.db` y el entorno `.venv` están en `.gitignore`.
