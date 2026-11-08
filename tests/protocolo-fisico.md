# Protocolo de pruebas con el prototipo físico — Challenge #3

Pruebas del ecosistema completo con el hardware real: nodo ESP32 montado, Raspberry Pi 5 como gateway y Ubidots.
Cierra lo que faltó en el Challenge #2 (validación solo en simulación, sin calibración, sin pérdida de WLAN ni filtro de
subred). Cada prueba guarda sus datos en `tests/datos/fisico/` con el nombre indicado; la plantilla de cada CSV está en
esa carpeta. El script [resumen_csv.py](herramientas/resumen_csv.py) convierte cada CSV en la tabla de resultados de la
Wiki (esperado, observado, error, repeticiones, desviación).

Reglas:

- Cada fila de un CSV es una lectura o una repetición, con fecha y hora local (`AAAA-MM-DD HH:MM:SS`).
- Se registra el valor tal como se observó; los cálculos los hace el script.
- Una prueba que no se ejecutó no se reporta; una que falló se reporta con su resultado.
- Antes de empezar: firmware y gateway de la misma versión del repositorio (anotar el commit en la columna `version`).

## Pruebas

| ID | Qué se prueba | Procedimiento | Repeticiones | Criterio de aceptación | Archivo |
| :--- | :--- | :--- | :--- | :--- | :--- |
| FIS-01 | Montaje | Encender el nodo; revisar en el registro serie el escáner I2C (0x27 y 0x77) y el estado de cada sensor | 1 | Los cuatro sensores en OK; LCD con las cuatro filas | `fis01-montaje.csv` |
| FIS-02 | Exactitud del nivel | Agua a 5 niveles del recipiente medidos con regla (cm); 10 lecturas del nivel en la LCD o el registro por nivel | 5 niveles × 10 | Error medio ≤ 1 cm en cada nivel (RNF-06) | `fis02-nivel.csv` |
| FIS-03 | Temperatura y humedad | DHT22 junto a un termohigrómetro de referencia en 3 condiciones (interior, exterior a la sombra, cerca de una fuente de calor); 10 lecturas por condición | 3 × 10 | Diferencia dentro de la exactitud del DHT22 (±0.5 °C, ±2 a 5 % HR) | `fis03-th.csv` |
| FIS-04 | Ciclo de 1 s con MQTT | Registro serie de 10 min con el nodo publicando al gateway | 600 ciclos | Desviación ≤ 2 ms y 0 ciclos perdidos | `fis04-ciclo.log` |
| FIS-05 | Mensajes nodo → gateway | 30 min de operación; contar en `gateway.db` los mensajes recibidos y los perdidos (saltos de `id`) | 1 sesión de 30 min | 0 perdidos en la WLAN del sitio | `fis05-mqtt.csv` |
| FIS-06 | Escenarios de fusión | Descenso real (vaciar el recipiente), calor (fuente de calor cerca del DHT22) y nivel crítico; anotar estado y causas esperados y observados | 3 por escenario | Estado y causas esperados; sin falsos positivos ni negativos | `fis06-fusion.csv` |
| FIS-07 | Latencia hacia la nube | Provocar un cambio de estado; tiempo entre `alertas.recibido_utc` y `cola_nube.enviado_utc` en `gateway.db`, y verificar el dato en Ubidots | 10 | ≤ 5 s desde el gateway hasta Ubidots | `fis07-latencia-nube.csv` |
| FIS-08 | Desactivación desde la nube | En CRÍTICO, activar el interruptor de Ubidots; tiempo entre `ordenes.recibida_utc` y `ordenes.respuesta_utc`; buzzer silenciado y LCD con `(DESACT.)`; repetir en NORMAL (debe rechazarse) | 5 + 5 | Aceptada en CRÍTICO y rechazada en NORMAL; ≤ 5 s | `fis08-orden.csv` |
| FIS-09 | Acceso no autorizado | Tablero local desde fuera de la subred, sin token y con clave errónea; broker sin clave; Ubidots sin sesión | 1 por caso | Todos rechazados | `fis09-acceso.csv` |
| FIS-10 | Pérdida de la WLAN | Apagar el punto de acceso 60 s; observar LCD y buzzer; medir el tiempo de reconexión del Wi-Fi y del cliente MQTT en el registro serie | 3 | Alarma in situ sin interrupción; tablero y MQTT de vuelta en ≤ 60 s (RNF-05) | `fis10-wlan.csv` |
| FIS-11 | Broker o gateway caído | `sudo systemctl stop mosquitto` 60 s (y un reinicio de la Raspberry Pi); observar la reconexión del nodo | 3 | El nodo reconecta solo; las alertas encoladas llegan | `fis11-broker.csv` |
| FIS-12 | Internet caído en el gateway | Desconectar la salida a Internet 10 min; anotar cargas pendientes en `cola_nube` antes y después de reconectar | 2 | Ninguna carga perdida; la cola vuelve a 0 | `fis12-internet.csv` |
| FIS-13 | Operación prolongada | 24 h del ecosistema completo; registro serie, `gateway.db` y memoria libre de `tRed` | 1 | Sin reinicios; memoria libre mínima estable; ET0 diaria calculada | `fis13-24h/` |
| FIS-14 | Patrones del buzzer | Grabar el sonido en ALERTA, CRÍTICO y FALLA NIVEL; medir frecuencia con una aplicación de afinador o espectro | 1 por estado | 440 Hz intermitente, 580 Hz continuo, 2 pitidos de 1 kHz; frecuencia dentro de ±5 % | `fis14-buzzer.csv` |
| FIS-15 | Consumo | Medidor USB en serie con la alimentación del nodo y de la Raspberry Pi, 5 min en NORMAL y 5 min en CRÍTICO | 1 por estado | Valor reportado (base de la autonomía) | `fis15-consumo.csv` |

## Datos que salen de `gateway.db`

Con el gateway funcionando en la Raspberry Pi:

```bash
cd ~/IoT-Challenge2-UniSabana/gateway
sqlite3 -header -csv gateway.db "SELECT * FROM telemetria;" > ../tests/datos/fisico/fis13-24h/telemetria.csv
sqlite3 -header -csv gateway.db "SELECT * FROM alertas;" > ../tests/datos/fisico/fis13-24h/alertas.csv
sqlite3 -header -csv gateway.db "SELECT * FROM ordenes;" > ../tests/datos/fisico/fis13-24h/ordenes.csv
sqlite3 -header -csv gateway.db "SELECT * FROM agregados_5min;" > ../tests/datos/fisico/fis13-24h/agregados.csv
```

Latencias de FIS-07 (gateway → Ubidots) y FIS-08 (orden → respuesta del nodo), en el formato de las plantillas:

```bash
gateway/.venv/bin/python tests/herramientas/latencias_gateway.py gateway/gateway.db --prueba FIS-07 --version <commit> >> tests/datos/fisico/fis07-latencia-nube.csv
gateway/.venv/bin/python tests/herramientas/latencias_gateway.py gateway/gateway.db --prueba FIS-08 --version <commit> >> tests/datos/fisico/fis08-orden.csv
```

## Resumen para la Wiki

```bash
gateway/.venv/Scripts/python tests/herramientas/resumen_csv.py tests/datos/fisico/fis02-nivel.csv
```

El script imprime la tabla Markdown con esperado, observado (media), error medio, repeticiones, desviación estándar y el
enlace al CSV.
