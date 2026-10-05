# Registros de simulación (SIMULACIÓN)

**Todos los archivos de esta carpeta son resultados de SIMULACIÓN en Wokwi, no mediciones del hardware.**
Los sensores son modelos del simulador y sus valores se fijan con los controles de cada escenario.

| Archivo | Escenario | Diagrama | Resultado |
| :--- | :--- | :--- | :--- |
| `estados.log` | [estados.yaml](../../../simulation/wokwi/escenarios/estados.yaml): arranque, NORMAL, ADVERTENCIA y ALERTA | `diagram.json` | Escenario completado |
| `critico.log` | [critico.yaml](../../../simulation/wokwi/escenarios/critico.yaml): CRÍTICO y salida por histéresis | `diagram.json` | Escenario completado |
| `falla-nivel.log` | [falla-nivel.yaml](../../../simulation/wokwi/escenarios/falla-nivel.yaml): desconexión y reconexión del ECHO del HC-SR04 | `diagram-falla-nivel.json` | Escenario completado |

- **Fecha de ejecución:** 05/10/2026, con `wokwi-cli` 0.27.1.
- **Firmware:** commit `f4d2c3f`, entorno `wokwi` (parámetros de demostración), imagen completa con LittleFS generada con `simulation/wokwi/preparar_imagen.py`.
- **Reproducir:** ver el encabezado de cada escenario. Requiere un token de Wokwi en la variable `WOKWI_CLI_TOKEN`.

## Fallas y limitaciones observadas

- **DHT22 en FALLA durante toda la simulación:** la librería DHTesp registra `TIMEOUT`. Se reprodujo con un programa mínimo que solo lee el DHT22 desde `loop()`, así que no depende de las tareas del firmware; en el mismo programa mínimo, la librería DHT de Adafruit sí leyó el sensor simulado. Por eso, en estos registros la temperatura, la humedad y el VPD no están disponibles y la compensación del nivel usa la temperatura del BMP180.
- **Sin capturas de la LCD:** las capturas de `wokwi-cli` muestran la LCD encendida pero sin caracteres, también con la librería LiquidCrystal_I2C. El contenido que el firmware envía a la LCD está en las líneas `tHMI LCD |...|` de cada registro.
- **Límite del plan gratuito:** `wokwi-cli` corta la sesión a los 5 minutos de tiempo real (unos 220 s simulados); por eso CRÍTICO va en un escenario aparte.
