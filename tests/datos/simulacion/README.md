# Registros de simulación (SIMULACIÓN)

**Todos los archivos de esta carpeta son resultados de SIMULACIÓN en Wokwi, no mediciones del hardware.**
Los sensores son modelos del simulador y sus valores se fijan con los controles de cada escenario.

| Archivo | Escenario | Diagrama | Resultado |
| :--- | :--- | :--- | :--- |
| `estados.log` | [estados.yaml](../../../simulation/wokwi/escenarios/estados.yaml): arranque, NORMAL, ADVERTENCIA y ALERTA | `diagram.json` | Escenario completado |
| `critico.log` | [critico.yaml](../../../simulation/wokwi/escenarios/critico.yaml): CRÍTICO y salida por histéresis | `diagram.json` | Escenario completado |
| `falla-nivel.log` | [falla-nivel.yaml](../../../simulation/wokwi/escenarios/falla-nivel.yaml): desconexión y reconexión del ECHO del HC-SR04 | `diagram-falla-nivel.json` | Escenario completado |
| `alerta-temperatura.log` | [alerta-temperatura.yaml](../../../simulation/wokwi/escenarios/alerta-temperatura.yaml): ALERTA por temperatura y VPD altos con el DHT22 a 31 °C y 40 % | `diagram.json` | Escenario completado |

- **Fecha de ejecución:** 05/10/2026, con `wokwi-cli` 0.27.1.
- **Firmware:** `estados.log`, `critico.log` y `falla-nivel.log` con el commit `f4d2c3f` (DHT22 con la librería DHTesp); `alerta-temperatura.log` con el commit `52aa808` (DHT22 con la librería de Adafruit). Entorno `wokwi` (parámetros de demostración), imagen completa con LittleFS generada con `simulation/wokwi/preparar_imagen.py`.
- **Reproducir:** ver el encabezado de cada escenario. Requiere un token de Wokwi en la variable `WOKWI_CLI_TOKEN`.

## Fallas y limitaciones observadas

- **DHT22 en FALLA (solo en los registros del commit `f4d2c3f`):** la librería DHTesp registra `TIMEOUT`. Se reprodujo con un programa mínimo que solo lee el DHT22 desde `loop()`, así que no depende de las tareas del firmware; en el mismo programa mínimo, la librería DHT de Adafruit sí leyó el sensor simulado. Por eso, en esos tres registros la temperatura, la humedad y el VPD no están disponibles y la compensación del nivel usa la temperatura del BMP180. **Resuelto** en el commit `52aa808` con la librería de Adafruit: en `alerta-temperatura.log` el DHT22 está en OK en 108 de 109 ciclos (el primero es anterior a su primera lectura).
- **Velocidad del sonido fija en Wokwi:** el simulador genera el eco sin considerar la temperatura, y el firmware sí la compensa; con el DHT22 a 31 °C, la distancia calculada es 13.3 cm para 13 cm del control (+0.3 cm). Es una diferencia de la simulación, no del hardware.
- **Sin capturas de la LCD:** las capturas de `wokwi-cli` muestran la LCD encendida pero sin caracteres, también con la librería LiquidCrystal_I2C. El contenido que el firmware envía a la LCD está en las líneas `tHMI LCD |...|` de cada registro.
- **Límite del plan gratuito:** `wokwi-cli` corta la sesión a los 5 minutos de tiempo real (unos 220 s simulados); por eso CRÍTICO va en un escenario aparte.
