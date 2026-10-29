# Diseño de la comunicación MQTT — Challenge #3

Propuesta del 29/10/2026. Define los tópicos, los mensajes y las frecuencias de publicación entre el nodo (ESP32), el gateway
(Raspberry Pi 5 con Mosquitto) y la plataforma en la nube (Ubidots). Parte del diseño de tópicos de la
[Actividad de refuerzo 2.3](../../refuerzo-2.3/PAGINA_WIKI_REFUERZO.md).

## 1. Identificadores

| Elemento | Identificador | Uso |
| :--- | :--- | :--- |
| Municipio | `chia` | Segundo nivel de los tópicos |
| Nodo (ESP32) | `chia-01` | Tercer nivel de los tópicos y ID de cliente MQTT |
| Gateway (Raspberry Pi) | `gw-chia` | ID de cliente MQTT en el broker local y en Ubidots |

Los tópicos siguen la estructura `sabana/{municipio}/{nodo}/{tipo}`, de modo que el gateway se suscribe a todos los nodos de
un municipio con `sabana/chia/+/#`.

## 2. Tópicos en el broker local (Mosquitto en la Raspberry Pi)

| Tópico | Publica | Se suscribe | QoS | Retenido | Cuándo |
| :--- | :--- | :--- | :---: | :---: | :--- |
| `sabana/chia/chia-01/telemetria` | Nodo | Gateway | 0 | No | Cada 5 s |
| `sabana/chia/chia-01/alerta` | Nodo | Gateway | 1 | No | En cada cambio del estado de alerta |
| `sabana/chia/chia-01/estado` | Nodo (`online`) y broker por la última voluntad (`offline`) | Gateway | 1 | Sí | Al conectarse y al perderse la conexión |
| `sabana/chia/chia-01/cmd/desactivar_alarma` | Gateway, por orden de un usuario autorizado en Ubidots | Nodo | 1 | No | Cuando un usuario autorizado da la orden |
| `sabana/chia/chia-01/cmd/respuesta` | Nodo | Gateway | 1 | No | Tras recibir una orden: aceptada o rechazada, con el motivo |

Justificación:

- Telemetría con QoS 0: llega cada 5 s, así que perder un mensaje no deja al gateway sin datos recientes, y QoS 0 no agrega acuses de recibo en la WLAN.
- Alerta, orden y respuesta con QoS 1: un cambio de estado o una orden no se deben perder; QoS 1 garantiza al menos una entrega. El mensaje lleva un identificador para descartar duplicados.
- Estado retenido con última voluntad: el gateway conoce de inmediato si el nodo está en línea, incluso si se suscribe después de que el nodo se conectó, y el broker publica `offline` si la conexión del nodo se cae sin cerrarse.
- Respuesta a la orden (tópico nuevo frente al refuerzo 2.3): el nodo aplica la desactivación solo en ALERTA o CRÍTICO ([fusion.cpp](../../firmware/src/fusion.cpp)); la respuesta permite que el tablero global muestre si la orden se aplicó o por qué se rechazó.

## 3. Contenido de los mensajes (JSON)

Los nombres de los campos son cortos y planos para reducir el tamaño del mensaje. Un valor sin lectura válida se envía como `null`.

### 3.1. Telemetría

```json
{
  "id": 1532,
  "ciclo": 7660,
  "epoca": 1793284800,
  "estado": "NORMAL",
  "causas": [],
  "alarma_desactivada": false,
  "nivel_cm": 22.0,
  "nivel_pct": 73,
  "tend_cm_min": -0.12,
  "t_c": 18.4,
  "hr_pct": 71,
  "p_hpa": 752.4,
  "uv_idx": 1.2,
  "vpd_kpa": 0.61,
  "et0_mm_dia": null,
  "sensores": {"nivel": "OK", "dht22": "OK", "bmp180": "OK", "guva": "OK"},
  "rssi": -61
}
```

| Campo | Origen en el firmware | Unidad |
| :--- | :--- | :--- |
| `id` | Contador de mensajes del nodo (para detectar pérdidas y duplicados) | — |
| `ciclo` | `Snapshot::ciclo` | ciclos de 1 s |
| `epoca` | Hora por NTP; `null` si no hay hora válida | s desde 1970 (UTC) |
| `estado`, `causas`, `alarma_desactivada` | `Derivados` (mismos textos que el tablero local) | — |
| `nivel_cm`, `nivel_pct`, `tend_cm_min` | `Derivados` | cm, %, cm/min |
| `t_c`, `hr_pct`, `p_hpa`, `uv_idx` | `Lecturas` | °C, %, hPa, índice UV |
| `vpd_kpa`, `et0_mm_dia` | `Derivados` | kPa, mm/día |
| `sensores` | Estado de cada sensor: `OK`, `VIEJO`, `FALLA` o `INICIANDO` | — |
| `rssi` | Intensidad de la señal Wi-Fi | dBm |

Tamaño estimado: unos 330 B de carga útil.

### 3.2. Alerta

```json
{"id": 1533, "ciclo": 7702, "epoca": 1793284842, "anterior": "ADVERTENCIA", "estado": "ALERTA", "causas": ["descenso", "temperatura_alta"], "nivel_pct": 38, "tend_cm_min": -1.4}
```

### 3.3. Estado del nodo (retenido)

`online` al conectarse; `offline` como mensaje de última voluntad. Texto plano, no JSON, para leerlo sin decodificar.

### 3.4. Orden de desactivar la alarma

```json
{"orden_id": "u-20261029-153012", "origen": "ubidots", "epoca": 1793287812}
```

### 3.5. Respuesta del nodo a la orden

```json
{"orden_id": "u-20261029-153012", "resultado": "aceptada", "estado": "CRITICO", "epoca": 1793287813}
```

`resultado` toma los valores `aceptada` o `rechazada`. Si es `rechazada`, el mensaje incluye `"motivo"`, por ejemplo `"estado NORMAL"`.

## 4. Límites de la cuenta gratuita de Ubidots (STEM)

Consultados el 29/10/2026 en la documentación oficial:

| Límite | Valor | Fuente |
| :--- | :--- | :--- |
| Dispositivos | 3 | [Ubidots STEM](https://ubidots.com/stem); [Plans & Billing](https://help.ubidots.com/en/articles/639806-plans-billing-what-is-the-difference-between-ubidots-and-ubidots-stem) |
| Variables por dispositivo | 10 | Plans & Billing |
| Tableros | 1, con hasta 10 widgets | Plans & Billing |
| Datos recibidos («dots») | 4 000 por día, sumando todos los dispositivos | [Dots con Ubidots STEM](https://help.ubidots.com/en/articles/636666-how-many-dots-can-i-send-with-ubidots-stem) |
| Frecuencia de peticiones | 1 por segundo por token | Dots con Ubidots STEM |
| Tamaño máximo de una petición | 10 000 B | [Dots con Ubidots](https://help.ubidots.com/en/articles/2093902-how-many-dots-can-i-send-to-ubidots) |
| Retención de datos | 1 mes | Plans & Billing |
| Correos de alerta | 100 por mes | Plans & Billing |
| SMS | 10 por mes, solo Estados Unidos y Canadá | Plans & Billing |
| Gestión de usuarios con permisos | No incluida en STEM (solo en planes de pago) | Plans & Billing |
| Uso | Solo no comercial (educación, investigación, proyectos personales) | Plans & Billing |

Pendiente de confirmar al crear la cuenta: servidor, puertos (con y sin TLS), forma de autenticación con el token y formato
de los tópicos MQTT de Ubidots. La página de referencia MQTT de la documentación no estaba disponible al consultarla.

## 5. Frecuencias de publicación

| Tramo | Frecuencia | Motivo |
| :--- | :--- | :--- |
| Nodo → gateway, telemetría | Cada 5 s | La WLAN local no tiene límite de mensajes; el gateway guarda en SQLite el detalle completo |
| Nodo → gateway, alerta | En cada cambio de estado | Aviso inmediato |
| Gateway → Ubidots, mediciones | Cada 5 min, con el promedio de los 60 mensajes del periodo | Límite de 4 000 datos por día |
| Gateway → Ubidots, estado de alerta | Además, de inmediato en cada cambio de estado | La alerta global no espera al siguiente promedio |

Cálculo de datos diarios en Ubidots con envíos cada 5 min (288 envíos por día):

| Dispositivo en Ubidots | Variables | Datos por día |
| :--- | :--- | ---: |
| `chia-01` (mediciones) | nivel_pct, nivel_cm, tendencia, temperatura, humedad, presión, UV, VPD, estado | 9 × 288 = 2 592 |
| `gw-chia` (gateway e IA) | horas hasta el nivel crítico, anomalía | 2 × 288 = 576 |
| `gw-chia` | ET0 diaria, nodo en línea, desactivar alarma (orden), cambios de estado inmediatos | ≈ 50 |
| Total | | ≈ 3 220 de 4 000 |

Queda un margen de unos 780 datos por día (≈ 20 %). Con envíos cada 10 min el total baja a unos 1 630.

## 6. Consecuencias para el diseño

- El gateway procesa localmente antes de enviar a la nube (promedios cada 5 min), como pide el enunciado, y además lo exige el límite de datos de la cuenta.
- El tablero global debe caber en 10 widgets.
- Acceso restringido al tablero global: STEM no incluye usuarios con permisos. Se propone que el tablero solo se consulte iniciando sesión en la cuenta del equipo, sin enlaces públicos: un enlace público no pide inicio de sesión y deja operables los widgets de control, por ejemplo el botón de desactivar la alarma ([Ubidots: compartir un tablero](https://help.ubidots.com/en/articles/901285-share-your-dashboard-or-widget-to-the-public)).
- Notificaciones: correo electrónico (100 por mes); los SMS gratuitos no llegan a Colombia.
- La retención de 1 mes en la nube no afecta el histórico completo, que queda en SQLite en el gateway.

## 7. Decisiones que requieren aprobación

1. Tópicos y mensajes de las secciones 2 y 3, incluido el tópico nuevo `cmd/respuesta`.
2. Telemetría cada 5 s hacia el gateway y promedios cada 5 min hacia Ubidots (o cada 10 min, para más margen).
3. Acceso al tablero global solo con inicio de sesión en la cuenta del equipo, sin enlaces públicos.
4. Notificaciones por correo electrónico.
