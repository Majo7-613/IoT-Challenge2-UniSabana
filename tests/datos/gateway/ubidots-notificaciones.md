# Notificaciones por correo de Ubidots, 09/11/2026
# Nodo simulado con el publicador de prueba (SIMULACIÓN del nodo); gateway, Ubidots y eventos reales.
# Eventos: chia-01/estado >= 4 (ALERTA o CRÍTICO) y gw-chia/nodo_en_linea = 0, cada uno con envío de correo.

| Dato enviado por el gateway | Hora de envío | Correo recibido (hora informada, resolución de 1 min) |
| :--- | :--- | :--- |
| nodo_en_linea = 0 (estado offline retenido de la prueba anterior) | 08:05:42 | 08:05 |
| estado = 5 (CRÍTICO) | 08:06:25 | 08:06 |
| nodo_en_linea = 0 (fin del publicador) | 08:06:47 | 08:06 |

Cola hacia Ubidots: 7 cargas enviadas y confirmadas, 0 pendientes.

## Registro del gateway
2026-11-09 08:05:41,684 gateway INFO conectado al broker local localhost:1883; suscrito a sabana/chia/+/#
2026-11-09 08:05:41,709 gateway INFO nodo chia-01 offline
2026-11-09 08:05:42,155 gateway.nube INFO conectado a Ubidots (industrial.api.ubidots.com:8883)
2026-11-09 08:06:07,691 gateway INFO nodo chia-01 online
2026-11-09 08:06:18,706 gateway INFO alerta de chia-01: NORMAL -> ADVERTENCIA
2026-11-09 08:06:25,735 gateway INFO alerta de chia-01: ADVERTENCIA -> CRITICO
2026-11-09 08:06:47,741 gateway INFO nodo chia-01 offline
2026-11-09 08:07:00,264 gateway INFO periodo 2026-11-09T13:06:00.000Z: 42 mensajes (0 perdidos), estado m�s grave CRITICO
