# Cola hacia Ubidots con corte de Internet — 09/11/2026

Tarea 21. Gateway en el computador, Mosquitto local y nodo simulado con el publicador de prueba (SIMULACIÓN del nodo; escenario `descenso`, 75 mensajes cada 1 s); cuenta de Ubidots real. Promedios cada 30 s (`PERIODO_AGREGADO_S=30`) para acortar la prueba.

- **Fase 1 (sin Internet):** el gateway arranca con `UBIDOTS_HOST=sin-internet.invalid`, un nombre que no resuelve, que equivale a no tener salida a Internet. El nodo publica normalmente.
- **Fase 2 (Internet restablecido):** el gateway arranca de nuevo, con la misma base de datos y el servidor real de Ubidots.

Guion: `prueba_cola.py` (fuera del repositorio); base de datos de la prueba descartada al terminar.

| Medida | Resultado |
| :--- | ---: |
| Cargas generadas sin Internet (promedios, cambios de estado, IA y conexión) | 12 |
| Cargas perdidas sin Internet | 0 (12 pendientes en `cola_nube`) |
| Intentos de envío fallidos sin Internet (uno por segundo) | 110 |
| Cargas enviadas al volver Internet | 13 de 13 (12 de la fase 1 + 1 nueva) |
| Tiempo para vaciar la cola | 20 s (1 carga por segundo, límite de la cuenta STEM) |
| Orden de envío | Igual al orden de llegada (id 1 a 13) |
| Cargas pendientes al final | 0 |

Límite: el corte se provoca con un nombre de servidor que no resuelve, no desconectando el Wi-Fi; el corte real del Wi-Fi de la Raspberry Pi es la prueba física FIS-12 ([protocolo-fisico.md](../../protocolo-fisico.md)).

## Cola al final de la prueba (tabla `cola_nube`)

| id | creado (UTC) | enviado (UTC) | intentos | dispositivo |
| ---: | :--- | :--- | ---: | :--- |
| 1 | 2026-11-09T13:54:51.922Z | 2026-11-09T13:56:54.784Z | 112 | gw-chia |
| 2 | 2026-11-09T13:55:00.462Z | 2026-11-09T13:56:55.949Z | 1 | chia-01 |
| 3 | 2026-11-09T13:55:00.468Z | 2026-11-09T13:56:57.076Z | 1 | gw-chia |
| 4 | 2026-11-09T13:55:02.941Z | 2026-11-09T13:56:58.174Z | 1 | chia-01 |
| 5 | 2026-11-09T13:55:09.947Z | 2026-11-09T13:56:59.269Z | 1 | chia-01 |
| 6 | 2026-11-09T13:55:30.927Z | 2026-11-09T13:57:00.373Z | 1 | chia-01 |
| 7 | 2026-11-09T13:55:30.931Z | 2026-11-09T13:57:01.477Z | 1 | gw-chia |
| 8 | 2026-11-09T13:56:00.082Z | 2026-11-09T13:57:02.613Z | 1 | chia-01 |
| 9 | 2026-11-09T13:56:00.085Z | 2026-11-09T13:57:03.705Z | 1 | gw-chia |
| 10 | 2026-11-09T13:56:07.043Z | 2026-11-09T13:57:04.813Z | 1 | gw-chia |
| 11 | 2026-11-09T13:56:30.422Z | 2026-11-09T13:57:05.926Z | 1 | chia-01 |
| 12 | 2026-11-09T13:56:30.426Z | 2026-11-09T13:57:07.021Z | 1 | gw-chia |
| 13 | 2026-11-09T13:56:47.656Z | 2026-11-09T13:57:08.133Z | 1 | gw-chia |

## Registro del gateway, fase 1 (sin Internet; se omiten las líneas repetidas «envío a Ubidots fallido»)

```text
2026-11-09 08:54:51,367 gateway INFO conectado al broker local localhost:1883; suscrito a sabana/chia/+/#
2026-11-09 08:54:51,922 gateway INFO nodo chia-01 online
2026-11-09 08:55:00,417 gateway INFO periodo 2026-11-09T13:54:30.000Z: 8 mensajes (0 perdidos), estado más grave NORMAL
2026-11-09 08:55:02,941 gateway INFO alerta de chia-01: NORMAL -> ADVERTENCIA
2026-11-09 08:55:09,946 gateway INFO alerta de chia-01: ADVERTENCIA -> CRITICO
2026-11-09 08:55:30,927 gateway INFO periodo 2026-11-09T13:55:00.000Z: 32 mensajes (0 perdidos), estado más grave CRITICO
2026-11-09 08:56:00,082 gateway INFO periodo 2026-11-09T13:55:30.000Z: 29 mensajes (0 perdidos), estado más grave CRITICO
2026-11-09 08:56:07,043 gateway INFO nodo chia-01 offline
2026-11-09 08:56:30,422 gateway INFO periodo 2026-11-09T13:56:00.000Z: 8 mensajes (0 perdidos), estado más grave CRITICO
```

## Registro del gateway, fase 2 (Internet restablecido)

```text
2026-11-09 08:56:47,631 gateway INFO conectado al broker local localhost:1883; suscrito a sabana/chia/+/#
2026-11-09 08:56:47,656 gateway INFO nodo chia-01 offline
2026-11-09 08:56:48,592 gateway.nube INFO conectado a Ubidots (industrial.api.ubidots.com:8883)
2026-11-09 08:56:53,684 gateway.nube WARNING envío a Ubidots fallido (cola: 13 pendientes)
```
