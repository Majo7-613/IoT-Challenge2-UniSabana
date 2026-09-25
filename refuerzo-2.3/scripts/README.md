# Scripts de Python para Packet Tracer — Actividad de refuerzo 2.3

Scripts para las SBC-PT (o MCU-PT) de la simulación:

| Archivo | Dispositivo | Qué hace |
| :--- | :--- | :--- |
| `nodo_telemetria.py` | Nodo (`chia-01`, `chia-02`, `cajica-01`) | Publica telemetría **simulada** cada `PERIODO_S` segundos, publica una alerta cuando cambia su estado, anuncia `online` (retenido) y atiende la orden `desactivar_alarma`. |
| `gateway_reenvio.py` | Gateway SBC de cada municipio | Se suscribe a los tópicos del municipio en el broker local y los republica en el broker de la nube. |
| `plataforma_suscriptor.py` | Cliente de la plataforma (200.10.10.20) | Se suscribe a telemetría, alertas y estado de todos los nodos, lleva la cuenta de mensajes y publica una orden de desactivar la alarma cuando un nodo entra en CRÍTICO. |

## Antes de usarlos: adaptar la API MQTT de Packet Tracer

Packet Tracer trae un cliente MQTT como proyecto de ejemplo de Python en la pestaña *Programming* de la SBC-PT. **Los nombres de sus funciones no se pudieron verificar en una fuente pública**, así que cada script aísla todo lo específico de Packet Tracer en un bloque marcado `ADAPTAR`, con cuatro funciones:

| Función del script | Qué debe hacer | De dónde sale |
| :--- | :--- | :--- |
| `mqtt_conectar(...)` | Conectar al broker con usuario, clave, identificador y, si la API lo permite, la última voluntad (LWT) | Del proyecto de ejemplo *MQTT Client* de Packet Tracer |
| `mqtt_publicar(topico, mensaje, qos, retenido)` | Publicar un mensaje | Ídem |
| `mqtt_suscribir(topico, qos)` | Suscribirse a un tópico | Ídem |
| `esperar_ms(ms)` | Pausar el script | Función de espera que use el ejemplo de Packet Tracer |

Además, la API de Packet Tracer entrega los mensajes recibidos a una función de retrollamada: conectar esa retrollamada con la función `al_recibir(topico, mensaje)` de cada script.

Mientras no se adapten, las funciones del bloque `ADAPTAR` lanzan un error explícito: los scripts no simulan en silencio un funcionamiento que no existe.

Si en tu versión de Packet Tracer no se pueden usar scripts con MQTT, la guía (`../GUIA_PACKET_TRACER.md`, paso 3.6, opción A) explica cómo hacer las mismas pruebas con la aplicación de escritorio *MQTT Client*.

## Datos simulados

Los nodos de Packet Tracer no tienen los sensores del prototipo. `nodo_telemetria.py` genera valores **simulados** (un nivel que baja poco a poco y un ambiente que varía) y los marca con `"origen":"simulado"` en cada mensaje. No son mediciones.

## Compatibilidad

El código evita construcciones recientes de Python (f-strings, anotaciones de tipo, `json`) para facilitar su ejecución en el intérprete de Packet Tracer. Si alguna instrucción no está disponible en tu versión, anótalo y ajústala.
