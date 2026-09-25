# Guion del video — Actividad de refuerzo 2.3 (5 minutos)

Requisitos: máximo 5 minutos, los tres integrantes con la cámara encendida, explicar el diseño, demostrar la validación en Packet Tracer y mostrar el troubleshooting. Grabar la pantalla de Packet Tracer con la cámara de quien habla en una esquina.

| Tiempo | Bloque | Quién | Qué se muestra | Qué se dice (idea central) |
| :--- | :--- | :--- | :--- | :--- |
| 0:00–0:20 | Presentación | Los tres | Cámaras | Equipo 7; la actividad extiende el nodo del Challenge a una red de monitoreo hídrico en Sabana Centro. |
| 0:20–1:10 | Diseño: problema y tipo de red | María José | Wiki: tabla PAN/WLAN/LPWAN y diagrama del ecosistema | Por qué no basta un nodo aislado; comparación de PAN, WLAN y LPWAN; decisión de arquitectura híbrida: Wi-Fi donde hay WLAN municipal, gateway por municipio y LoRaWAN como extensión rural (no simulada). |
| 1:10–2:00 | Diseño: MQTT | Pablo | Wiki: tabla de tópicos | Publicación/suscripción; estructura `sabana/{municipio}/{nodo}/{tipo}`; QoS 0 para telemetría, QoS 1 para alertas y órdenes; estado retenido con LWT; volumen de datos (unos 289 kB/día por nodo). Aclarar que el prototipo del Challenge no usa MQTT. |
| 2:00–2:40 | Demo: topología y red | Simón | Packet Tracer: topología, `show ip route` del ISP, `ping` de un nodo al broker | Dos municipios, ISP y red de la plataforma; direccionamiento y conectividad de extremo a extremo. |
| 2:40–3:30 | Demo: MQTT | Pablo | Modo Simulation: CONNECT/CONNACK, SUBSCRIBE, PUBLISH; mensajes en la plataforma | Los nodos de Chía y Cajicá publican telemetría y la plataforma la recibe; alerta con QoS 1. |
| 3:30–4:00 | Demo: estado, orden y usuario | María José | Desconexión de un nodo (`offline`), orden `desactivar_alarma`, navegador del PC | La LWT detecta el nodo caído; la plataforma envía la orden al nodo; el usuario consulta el tablero por HTTP. |
| 4:00–4:45 | Troubleshooting | Simón | Capturas de antes y después de las 3 fallas | Síntoma, causa y solución de cada falla real. |
| 4:45–5:00 | Cierre | Los tres | Cámaras | Qué aprendimos de conectividad IoT y cómo se conecta con el Challenge. |

## Lista de verificación antes de grabar

- [ ] Archivo `.pkt` abierto y probado; modo Simulation con los filtros listos.
- [ ] Capturas de las 3 fallas a mano (sección 9 de la Wiki).
- [ ] Wiki de la actividad abierta en el navegador.
- [ ] Los tres con cámara y micrófono probados.
- [ ] Cronómetro: ensayar una vez completo y recortar si pasa de 5:00.
- [ ] Subir el video de forma que se reproduzca en Teams sin descargarlo, junto con la URL de la página de Wiki.

> Nota: el reparto de quién habla es una propuesta; ajustarlo si cambia quién construyó cada parte en Packet Tracer.
