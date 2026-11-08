# Validación de la configuración de Mosquitto (despliegue/mosquitto) en el computador de desarrollo, 07/11/2026
# Broker de prueba en 127.0.0.1:1884 con claves de prueba (no son las del despliegue).

| Prueba | Esperado | Observado |
| :--- | :--- | :--- |
| Cliente anónimo | Rechazado | Connection Refused: not authorised |
| nodo-chia-01 con clave errónea | Rechazado | Connection Refused: not authorised |
| nodo-chia-01 publica en su telemetría | Entregado | El usuario monitor recibió el mensaje |
| nodo-chia-01 publica en la telemetría de chia-02 | Bloqueado | Denied PUBLISH |
| gw-chia publica en la telemetría de chia-01 | Bloqueado | Denied PUBLISH |
| gw-chia publica la orden a chia-01 | Entregada al nodo | nodo-chia-01 recibió {"orden_id":"g-1"} |
| nodo-chia-01 se suscribe a tópicos de chia-02 | Sin mensajes | No recibió la orden g-2 de chia-02 |

## Registro del broker (extracto)
1794109538: Client auto-453D075C-8875-0683-91BD-9A67362CD4F3 [127.0.0.1:54655] disconnected: not authorised.
1794109540: Client auto-CD490719-091C-C85C-CF9C-926A2957CA9B [127.0.0.1:54661] disconnected: not authorised.
1794109542: 	sabana/chia/chia-01/cmd/desactivar_alarma (QoS 0)
1794109542: auto-4C92C3E6-7301-2340-13ED-650096964E90 0 sabana/chia/chia-01/cmd/desactivar_alarma
1794109546: Denied PUBLISH from auto-8F8ECA38-4D1A-C730-B4DC-5A4229046BBB (d0, q0, r0, m0, 'sabana/chia/chia-02/telemetria', ... (5 bytes))
1794109548: New client connected from 127.0.0.1:51478 as auto-218A9C4A-A130-8908-2C1F-210FF183F05A (p4, c1, k60, u'gw-chia').
1794109548: Received PUBLISH from auto-218A9C4A-A130-8908-2C1F-210FF183F05A (d0, q1, r0, m1, 'sabana/chia/chia-01/cmd/desactivar_alarma', ... (18 bytes))
1794109550: New client connected from 127.0.0.1:51484 as auto-81CBEA3A-8A55-C95D-9465-081021148CA0 (p4, c1, k60, u'gw-chia').
1794109550: Denied PUBLISH from auto-81CBEA3A-8A55-C95D-9465-081021148CA0 (d0, q0, r0, m0, 'sabana/chia/chia-01/telemetria', ... (5 bytes))
