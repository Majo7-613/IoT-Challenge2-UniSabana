# Monitoreo IoT de Recursos Hídricos en Sabana Centro — Challenge #2

Repositorio de código del **Equipo 7** para el Challenge #2 del curso Internet de las Cosas (2026-2), Universidad de La Sabana.

Nodo de bajo costo basado en un **ESP32-DevKitC V4 (ESP32-WROOM-32E)** que está diseñado para medir el nivel de agua (HC-SR04), la temperatura y la humedad relativa (DHT22), la presión atmosférica (BMP180) y la radiación ultravioleta (GUVA-S12SD) en un punto de almacenamiento de agua, clasificar el riesgo en cuatro estados de alerta y notificarlo en el sitio (LCD 20×4 y buzzer) y en un tablero de control web alojado en el propio ESP32 dentro de la WLAN local, sin MQTT.

La documentación completa (diseño, pruebas y resultados) está en la [Wiki del proyecto](https://github.com/Majo7-613/IoT-Challenge2-UniSabana/wiki).

## Estado

| Componente | Estado |
| :--- | :--- |
| Firmware, pasos 1 a 5: tareas FreeRTOS, sensores, nivel, tendencia, VPD, ET0, fusión, LCD, buzzer, Wi-Fi, servidor web con control de acceso, histórico y WebSocket | Implementado · (compila sin advertencias) |
| Frontend del tablero (`firmware/data/`) | Implementado · probado con el servidor simulado; pendiente de verificación en el ESP32 |
| Pruebas unitarias (`pio test -e native`): fusión, formato de la LCD y 5 casos calculados a mano del paso 3 | 16 pruebas superadas |
| Simulación en Wokwi (ESP32) | Ejecutada el 05/10/2026 (SIMULACIÓN): estados de alerta, tablero servido por el ESP32 simulado y control de acceso; registros en `tests/datos/simulacion/` y capturas en `docs/capturas/wokwi/` |

## Estructura

```text
├── firmware/              Proyecto PlatformIO del firmware
│   ├── platformio.ini
│   ├── include/           config.h (pines, periodos, parámetros), datos.h, secrets.example.h
│   ├── src/               main.cpp y un .cpp por módulo
│   ├── data/              Archivos del tablero web (HTML, CSS, JS) para LittleFS
│   └── test/              Pruebas unitarias (entorno native)
├── simulation/wokwi/      diagram.json, wokwi.toml, escenarios de wokwi-cli e imagen con LittleFS
├── tools/tablero_simulado/ Servidor simulado del tablero (datos simulados) y script de capturas
├── cad/                   Maqueta y carcasa: fuente CAD, STL y renders
├── tests/                 Protocolo, datos CSV y scripts de gráficas
├── docs/actas/            Actas de reunión y coevaluación
├── docs/capturas/         Capturas del tablero y de la simulación
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
pio test -e native             # pruebas unitarias en el computador (requiere gcc/g++)
pio run -t uploadfs            # cargar el frontend del tablero en LittleFS
```

Probar el tablero sin el ESP32 (datos simulados): `python tools/tablero_simulado/servidor_simulado.py` y abrir `http://localhost:8080/?token=token-dispositivo-1`.

`secrets.h` está en `.gitignore`: las credenciales de la WLAN y del tablero nunca se suben al repositorio.

## Equipo

| Integrante | Rol principal | GitHub |
| :--- | :--- | :--- |
| Simón Martínez García | Todo el hardware | [@simonmartinezunisabana](https://github.com/simonmartinezunisabana) |
| Pablo Andrés Tamayo González | Todo el software; coordina la carcasa y el modelo 3D | [@ItsN3M3515](https://github.com/ItsN3M3515) |
| María José Almanza Caviedes | Documentación y Wiki, frontend del tablero, modelo de negocio y datos IDEAM | [@Majo7-613](https://github.com/Majo7-613) |
