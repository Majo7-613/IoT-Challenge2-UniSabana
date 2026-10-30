# Arquitectura del sistema — Challenge #3

Diagrama de bloques de hardware y de software del ecosistema completo. Diseño del 29/10/2026; los detalles están en
[comunicacion-mqtt.md](comunicacion-mqtt.md), [gateway.md](gateway.md) y [modulo-ia.md](modulo-ia.md).

## 1. Hardware

```mermaid
flowchart LR
    subgraph NODO["Nodo de monitoreo"]
        HC["HC-SR04<br/>nivel"] -->|TRIG GPIO 26 / ECHO GPIO 27| ESP["ESP32-DevKitC V4"]
        DHT["DHT22<br/>T y HR"] -->|GPIO 4| ESP
        BMP["BMP180<br/>presión"] -->|I2C 0x77| ESP
        UV["GUVA-S12SD<br/>UV"] -->|ADC1 GPIO 34| ESP
        ESP -->|I2C 0x27| LCD["LCD 20×4"]
        ESP -->|LEDC GPIO 25| BZ["Buzzer pasivo"]
    end
    subgraph SITIO["WLAN de la zona"]
        AP["Punto de acceso Wi-Fi"]
        RPI["Raspberry Pi 5<br/>gateway, microSD 128 GB"]
        CEL["Computador o celular<br/>autorizado"]
    end
    ESP <-->|Wi-Fi 2.4 GHz| AP
    RPI <-->|Wi-Fi o Ethernet| AP
    CEL <-->|Wi-Fi| AP
    AP <-->|Internet| NUBE["Ubidots<br/>plataforma IoT"]
    REM["Usuario autorizado<br/>en cualquier parte de Colombia"] <-->|Internet| NUBE
```

## 2. Software

```mermaid
flowchart TB
    subgraph FW["Firmware del ESP32 (FreeRTOS)"]
        ISR["ISR del temporizador<br/>1 Hz"] --> TS["tSensores<br/>adquisición"]
        TS -->|cola| TF["tFusion<br/>nivel, tendencia, VPD, fusión"]
        TF --> TH["tHMI<br/>LCD y buzzer"]
        TF --> THI["tHistorico<br/>búferes rápido y lento"]
        TF --> TR["tRed<br/>servidor web, WebSocket"]
        TF --> TM["tMqtt (nuevo)<br/>publicación y órdenes"]
    end
    subgraph GW["Gateway en la Raspberry Pi 5"]
        MQ["Mosquitto<br/>usuarios y ACL"]
        PY["Programa del gateway (Python)<br/>validación, promedios, ET0"]
        DB[("SQLite")]
        IA["Módulo de IA<br/>predicción, anomalías, recomendación"]
        CO["Cola de envío"]
    end
    subgraph CLOUD["Ubidots"]
        DEV["Dispositivos chia-01 y gw-chia"]
        DASH["Tablero global<br/>con inicio de sesión"]
        EV["Eventos<br/>notificación por correo"]
    end
    TR -->|HTTP, Digest + token + subred| LOC["Tablero local<br/>en el navegador"]
    TM -->|MQTT, WLAN| MQ
    MQ -->|orden| TM
    MQ <--> PY
    PY <--> DB
    DB <--> IA
    PY --> CO
    IA --> CO
    CO -->|MQTT con TLS| DEV
    DEV --> DASH
    DEV --> EV
    DASH -->|orden: desactivar alarma| DEV
    DEV -->|MQTT con TLS| PY
    IA -.->|API de IA generativa| GEN["Gemini"]
```

## 3. Qué cambia frente al Challenge #2

| Bloque | Challenge #2 | Challenge #3 |
| :--- | :--- | :--- |
| Firmware | Cinco tareas; tablero local por HTTP y WebSocket | Tarea nueva `tMqtt`; el resto sin cambios |
| Comunicación | Sin MQTT | MQTT nodo → gateway → Ubidots |
| Almacenamiento | Búferes en RAM del ESP32 (se pierden al reiniciar) | Además, SQLite en el gateway |
| Tableros | Local en el ESP32 | Local en el ESP32 y global en Ubidots |
| Inteligencia artificial | — | Predicción, anomalías y recomendación en el gateway |
