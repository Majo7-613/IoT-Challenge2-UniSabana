# Simulación en Wokwi

Simulación del nodo con la placa `board-esp32-devkit-c-v4` y los mismos pines de `firmware/include/config.h`. El binario simulado es el mismo firmware, compilado en el entorno `wokwi` de PlatformIO.

## Cómo ejecutarla

1. Compilar: `pio run -d firmware -e wokwi` (desde la raíz del repositorio).
2. En VS Code, con la extensión de Wokwi, abrir `simulation/wokwi/diagram.json` e iniciar la simulación.
3. Ver el registro serie en la terminal de Wokwi y variar los sensores con sus controles.

## Conexiones

| Parte en Wokwi | Pin de la parte | GPIO del ESP32 |
| :--- | :--- | :--- |
| `wokwi-hc-sr04` | TRIG / ECHO | 26 / 27 |
| `wokwi-dht22` | SDA (datos) | 4 |
| `board-bmp180` | SDA / SCL | 21 / 22 |
| `wokwi-lcd2004` (modo I2C) | SDA / SCL | 21 / 22 |
| `wokwi-potentiometer` (emula el GUVA-S12SD) | SIG | 34 (ADC1) |
| `wokwi-buzzer` | 2 | 25 |

## Diferencias entre la simulación y el hardware

| Elemento | Hardware | Simulación |
| :--- | :--- | :--- |
| GUVA-S12SD | Módulo real, salida de unos 0 a 1 V; ADC con atenuación de 2.5 dB | Potenciómetro de 0 a 3.3 V; ADC con atenuación de 11 dB (bandera `SIMULACION`) |
| ECHO del HC-SR04 | Divisor 1 kΩ / 2 kΩ (5 V → 3.3 V) | Conexión directa |
| Bus I2C de la LCD | Conversor de nivel BSS138 (pendiente de compra) | Conexión directa |
| Distancia del HC-SR04 | Tiempo de vuelo real | Wokwi genera el eco con una velocidad del sonido fija; el firmware compensa con la temperatura del DHT22, por lo que la distancia calculada puede diferir ligeramente del valor del control deslizante |
| Presión inicial | Presión del sitio | 75 000 Pa (750 hPa) en `diagram.json`, valor aproximado para la altitud de la Sabana; se ajusta con el control del BMP180 |

La LCD y el buzzer ya están conectados, pero el firmware del paso 2 todavía no los usa (paso 4).

## Pendiente

- Publicar el proyecto en wokwi.com y enlazarlo en el README y en la página 3.5 de la Wiki.
- Una captura por escenario de prueba.
