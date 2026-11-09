# Sesión MQTT persistente del gateway, 09/11/2026 (nodo simulado con el publicador de prueba; SIMULACIÓN)

Antes del cambio: el gateway tardó unos 24 s en arrancar (carga de scikit-learn) y las dos alertas publicadas en ese
intervalo no se guardaron (sesión limpia: el broker no retiene mensajes para un cliente desconectado).

Con clean_session=False e identificador fijo (gw-chia):

1. Primer arranque del gateway: se crea la sesión y la suscripción sabana/chia/+/# (QoS 1).
2. Gateway apagado; el publicador envía el descenso con las alertas NORMAL -> ADVERTENCIA (id 11) y ADVERTENCIA -> CRITICO (id 19).
3. Segundo arranque: el broker entrega las dos alertas guardadas en los primeros 54 ms tras la conexión.

Resultado: alertas guardadas en SQLite = [(11, 'ADVERTENCIA'), (19, 'CRITICO')].
