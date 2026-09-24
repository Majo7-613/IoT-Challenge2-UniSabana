# Monitoreo IoT de Recursos Hídricos en Sabana Centro — Challenge #2

Repositorio de código del **Equipo 7** para el Challenge #2 del curso Internet de las Cosas (2026-2), Universidad de La Sabana.

Nodo de bajo costo basado en un **ESP32-DevKitC V4 (ESP32-WROOM-32E)** que está diseñado para medir el nivel de agua (HC-SR04), la temperatura y la humedad relativa (DHT22), la presión atmosférica (BMP180) y la radiación ultravioleta (GUVA-S12SD) en un punto de almacenamiento de agua, clasificar el riesgo en cuatro estados de alerta y notificarlo en el sitio (LCD 20×4 y buzzer) y en un tablero de control web alojado en el propio ESP32 dentro de la WLAN local, sin MQTT.

La documentación completa (diseño, pruebas y resultados) está en la [Wiki del proyecto](https://github.com/Majo7-613/IoT-Challenge2-UniSabana/wiki).

## Estado

| Componente | Estado |
| :--- | :--- |
| Firmware, paso 1: tareas FreeRTOS, temporizador de 1 Hz y registro serie | En desarrollo |
| Firmware, pasos 2 a 6: sensores, nivel y evaporación, fusión e interfaz local, red y servidor, histórico y robustez | Pendiente |
| Simulación en Wokwi (ESP32) | Pendiente |
| Esquemático, CAD y datos de pruebas | Pendiente |

## Estructura

```text
├── firmware/              Proyecto PlatformIO del firmware
│   ├── platformio.ini
│   ├── include/           config.h (pines, periodos, parámetros), datos.h, secrets.example.h
│   ├── src/               main.cpp y un .cpp por módulo
│   ├── data/              Archivos del tablero web (HTML, CSS, JS) para LittleFS
│   └── test/              Pruebas unitarias (fusión y evaporación)
├── simulation/wokwi/      diagram.json, wokwi.toml y capturas
├── hardware/              Esquemático (fuente y PNG/PDF) y lista de materiales
├── cad/                   Maqueta y carcasa: fuente CAD, STL y renders
├── tests/                 Protocolo, datos CSV y scripts de gráficas
├── docs/actas/            Actas de reunión y coevaluación
├── refuerzo-2.3/          Actividad de refuerzo 2.3 (Packet Tracer)
└── legacy-ch1/            Código y simulación del Challenge #1 (Arduino Uno), solo como referencia
```

## Compilar y cargar el firmware

Requisitos: [PlatformIO](https://platformio.org/) (extensión de VS Code o CLI).

```bash
cd firmware
cp include/secrets.example.h include/secrets.h   # y llenar las credenciales
pio run                        # compilar
pio run -t upload              # cargar en el ESP32
pio device monitor             # ver el registro serie (115200 baudios)
```

`secrets.h` está en `.gitignore`: las credenciales de la WLAN y del tablero nunca se suben al repositorio.

## Equipo

| Integrante | Rol principal |
| :--- | :--- |
| Simón Martínez García | Hardware, alimentación, montaje y banco de pruebas |
| Pablo Andrés Tamayo González | Firmware, FreeRTOS, lógica de fusión, API y autenticación; carcasa y CAD |
| María José Almanza Caviedes | Documentación y Wiki, frontend del tablero, modelo de negocio y datos IDEAM |
