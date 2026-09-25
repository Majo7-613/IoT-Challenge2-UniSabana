# Guion del video — Actividad de refuerzo 2.3 (5 minutos)

Requisitos: máximo 5 minutos, los tres integrantes con la cámara encendida, explicar el diseño, demostrar la validación en Packet Tracer 9.0.0.810 y mostrar el troubleshooting. Grabar la pantalla de Packet Tracer con la cámara de quien habla en una esquina.

| Tiempo | Bloque | Quién | Qué se muestra | Qué se dice (idea central) |
| :--- | :--- | :--- | :--- | :--- |
| 0:00–0:20 | Presentación | Los tres | Cámaras | Equipo 7; la actividad extiende el nodo del Challenge a una red de monitoreo hídrico en Sabana Centro. |
| 0:20–1:10 | Diseño: problema y tipo de red | María José | Wiki: tabla PAN/WLAN/LPWAN y diagrama del ecosistema | Por qué no basta un nodo aislado; comparación de PAN, WLAN y LPWAN; arquitectura híbrida: Wi-Fi donde hay WLAN municipal, gateway por municipio y LoRaWAN como extensión rural (no simulada). |
| 1:10–2:00 | Diseño: MQTT | Pablo | Wiki: tabla de tópicos | Publicación/suscripción; estructura `sabana/{municipio}/{nodo}/{tipo}`; QoS 0 para telemetría, QoS 1 para alertas y órdenes; estado retenido con LWT en el diseño; volumen de datos. Aclarar que el prototipo del Challenge no usa MQTT. |
| 2:00–2:40 | Demo: topología y red | Simón | Packet Tracer: topología (dos municipios, ISP y nube con AP-Nube), `show ip route` del ISP, `ping` de un nodo al broker | Direccionamiento; por qué el router inalámbrico cumple la función de gateway en la simulación; por qué hay un punto de acceso en la nube (la SBC-PT solo tiene Wi-Fi). |
| 2:40–3:30 | Demo: MQTT | Pablo | Broker con usuarios; Plataforma: CONNECT/CONNACK y suscripción a `sabana/#`; publicación de chia-01 y cajica-01; alerta con QoS 1 y PUBACK | Los nodos de Chía y Cajicá publican y la Plataforma recibe todo con una sola suscripción. |
| 3:30–4:00 | Demo: orden, usuario y limitaciones | María José | Orden `desactivar_alarma` de la Plataforma a chia-01; navegador del Usuario-Alcaldia en `http://200.10.10.30`; `"will":{}` y la contraseña visible en el CONNECT | La plataforma envía órdenes al nodo y el usuario consulta el tablero por HTTP. Limitaciones: sin LWT en la aplicación de Packet Tracer y credenciales en texto plano, por lo que en producción se usa TLS (8883). |
| 4:00–4:45 | Troubleshooting | Simón | Configuración del WRT300N (seguridad Wi-Fi y rango del DHCP) y de cajica-01 | Las 3 fallas reales: seguridad Wi-Fi distinta; nodos con 0.0.0.0 por el rango del DHCP; gateway SBC que no respondía, resuelto con la simplificación. |
| 4:45–5:00 | Cierre | Los tres | Cámaras | Qué aprendimos de conectividad IoT y cómo se conecta con el Challenge. |

## Lista de verificación antes de grabar

- [ ] Archivo `red-sabana-centro.pkt` abierto y probado; aplicaciones MQTT del broker y de los clientes conectadas.
- [ ] Mensajes de ejemplo listos para pegar en las aplicaciones *MQTT Client* (ver `GUIA_PACKET_TRACER.md`, paso 3.6).
- [ ] Para el troubleshooting no hay capturas de antes y después: mostrar en vivo la configuración corregida o reproducir la falla y corregirla.
- [ ] Wiki de la actividad abierta en el navegador.
- [ ] Los tres con cámara y micrófono probados.
- [ ] Cronómetro: ensayar una vez completo y recortar si pasa de 5:00.
- [ ] Subir el video de forma que se reproduzca en Teams sin descargarlo, junto con la URL de la Wiki de la actividad.

> Nota: el reparto de quién habla es una propuesta; ajustarlo si cambia quién construyó cada parte en Packet Tracer.
