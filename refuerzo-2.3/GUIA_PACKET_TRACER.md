# Guía de Packet Tracer — Actividad de refuerzo 2.3

Guía para reconstruir y validar en **Cisco Packet Tracer 9.0.0.810** la red de monitoreo hídrico de la actividad. Describe la topología que se construyó y se validó (archivo `red-sabana-centro.pkt`); los resultados están en [`PAGINA_WIKI_REFUERZO.md`](PAGINA_WIKI_REFUERZO.md), sección 8.

> **Simplificación frente al diseño.** El diseño tiene una SBC gateway por municipio. En la simulación, el router inalámbrico de cada municipio cumple la función de gateway: enruta hacia el proveedor de Internet y aplica NAT. El gateway SBC con broker local queda en el diseño real, no simulado (ver la sección 5 de esta guía).

## 0. Qué se construyó

* **Chía:** router inalámbrico WRT300N con dos nodos (`chia-01`, `chia-02`) y el teléfono del usuario de la alcaldía.
* **Cajicá:** router inalámbrico WRT300N con un nodo (`cajica-01`).
* **ISP:** router 2911 que une los dos municipios con la red de la nube.
* **Nube:** switch 2960 con un punto de acceso, el broker MQTT (SBC-PT), la Plataforma (SBC-PT) y el servidor web del tablero (Server-PT).

## 1. Dispositivos

| Rol | Nombre en el `.pkt` | Dispositivo de Packet Tracer |
| :--- | :--- | :--- |
| Router inalámbrico de Chía (gateway del municipio) | Router-Chia | WRT300N |
| Router inalámbrico de Cajicá (gateway del municipio) | Router-Cajica | WRT300N |
| Router del proveedor de Internet | ISP | Router 2911 |
| Switch de la nube | SW-Nube | Switch 2960 |
| Punto de acceso de la nube | AP-Nube | AccessPoint-PT |
| Nodos de monitoreo | chia-01, chia-02, cajica-01 | SBC-PT |
| Broker MQTT | broker | SBC-PT |
| Cliente de la plataforma | Plataforma | SBC-PT |
| Servidor web del tablero | Servidor-Web | Server-PT |
| Usuario final | Usuario-Alcaldia | Smart Phone |

**Por qué hay un punto de acceso en la nube:** en Packet Tracer 9, la SBC-PT solo trae una interfaz inalámbrica (Wireless3) y Bluetooth; no se puede cablear al switch. El broker y la Plataforma se asocian por Wi-Fi al AP-Nube, que está cableado al SW-Nube.

## 2. Direccionamiento

| Dispositivo | Interfaz | Dirección | Puerta de enlace |
| :--- | :--- | :--- | :--- |
| Router-Chia | Internet (WAN) | 10.0.0.1/30 | 10.0.0.2 |
| Router-Chia | LAN | 192.168.10.1/24, SSID `WLAN-Chia` | — |
| chia-01, chia-02 | Wireless | DHCP (rango desde 192.168.10.100, 50 usuarios) | 192.168.10.1 |
| Usuario-Alcaldia | Wireless | DHCP en `WLAN-Chia` | 192.168.10.1 |
| Router-Cajica | Internet (WAN) | 10.0.0.5/30 | 10.0.0.6 |
| Router-Cajica | LAN | 192.168.20.1/24, SSID `WLAN-Cajica` | — |
| cajica-01 | Wireless | 192.168.20.10/24 (estática) | 192.168.20.1 |
| ISP | g0/0 (hacia Chía) | 10.0.0.2/30 | — |
| ISP | g0/1 (hacia Cajicá) | 10.0.0.6/30 | — |
| ISP | g0/2 (hacia la nube) | 200.10.10.1/24 | — |
| broker | Wireless (AP-Nube) | 200.10.10.10/24 | 200.10.10.1 |
| Plataforma | Wireless (AP-Nube) | 200.10.10.20/24 | 200.10.10.1 |
| Servidor-Web | FastEthernet (SW-Nube) | 200.10.10.30/24 | 200.10.10.1 |

## 3. Configuración paso a paso

### 3.1. Router ISP (CLI)

```text
enable
configure terminal
hostname ISP
interface g0/0
 ip address 10.0.0.2 255.255.255.252
 no shutdown
interface g0/1
 ip address 10.0.0.6 255.255.255.252
 no shutdown
interface g0/2
 ip address 200.10.10.1 255.255.255.0
 no shutdown
exit
ip route 192.168.10.0 255.255.255.0 10.0.0.1
ip route 192.168.20.0 255.255.255.0 10.0.0.5
end
write memory
```

Como los WRT300N hacen NAT en su WAN, el ISP ve el tráfico de cada municipio con la dirección 10.0.0.1 o 10.0.0.5. MQTT funciona igual, porque los clientes inician la conexión hacia el broker. Verificar con `show ip interface brief` (g0/0, g0/1 y g0/2 en *up/up*) y `show ip route` (rutas `S` a las dos LAN).

### 3.2. Routers inalámbricos de Chía y Cajicá (interfaz gráfica)

1. *Config* o *GUI* → *Internet Setup*: IP estática 10.0.0.1/30 con puerta de enlace 10.0.0.2 (Cajicá: 10.0.0.5/30 y 10.0.0.6).
2. *Network Setup*: IP de la LAN 192.168.10.1/24 (Cajicá: 192.168.20.1/24).
3. **DHCP de Chía:** al cambiar la IP de la LAN, revisar el rango: dirección inicial .100 y 50 usuarios como máximo (ver la falla 2 de la sección 6).
4. *Wireless*: SSID `WLAN-Chia` (Cajicá: `WLAN-Cajica`) y la seguridad elegida; los nodos deben usar exactamente el mismo SSID, tipo de seguridad y clave (falla 1).
5. Cablear el puerto *Internet* de cada router a la interfaz correspondiente del ISP.

En Packet Tracer 9, la interfaz gráfica del WRT300N muestra algunas etiquetas en blanco sobre blanco: seleccionar el texto o revisar cada campo con cuidado.

### 3.3. Red de la nube

1. Cablear el SW-Nube a g0/2 del ISP, y el AP-Nube y el Servidor-Web al SW-Nube.
2. AP-Nube: SSID `Nube`, sin autenticación.
3. broker y Plataforma: *Config* → interfaz inalámbrica asociada a `Nube`, IP y puerta de enlace estáticas según la tabla del paso 2.
4. Servidor-Web: IP y puerta de enlace estáticas según la tabla del paso 2.

### 3.4. Nodos

1. *Config* → interfaz inalámbrica: SSID y seguridad del municipio.
2. chia-01 y chia-02 por DHCP; cajica-01 con IP estática 192.168.20.10/24 y puerta de enlace 192.168.20.1.
3. Verificar con `ping 200.10.10.10`. El primer `ping` de cada nodo pierde 1 de 4 paquetes por la resolución ARP inicial.

### 3.5. Broker MQTT

1. En el broker: *Programming* → proyecto **«MQTT Broker/Client - (Python)»** → *Install to Desktop*.
2. *Desktop* → aplicación del broker: crear los usuarios autorizados `nodo` / `clave-nodo` y `plataforma` / `clave-plataforma` y encenderlo.
3. Las secciones *Clients*, *Subscriptions* y *Event Log* del broker muestran los clientes conectados, sus suscripciones y los paquetes recibidos.

### 3.6. Clientes MQTT (aplicación de escritorio)

En cada SBC cliente (nodos y Plataforma): *Programming* → proyecto «MQTT Broker/Client - (Python)» → *Install to Desktop* → *Desktop* → aplicación *MQTT Client*. Conectar al broker 200.10.10.10 con el usuario que corresponda y luego:

| Cliente | Acción | Tópico | QoS | Carga |
| :--- | :--- | :--- | :--- | :--- |
| Plataforma | Suscribirse | `sabana/#` | 0 | — |
| chia-01, chia-02, cajica-01 | Publicar | `sabana/{municipio}/{nodo}/telemetria` | 0 | `{"nivel_cm":…,"t_c":…,"hr":…,"p_hpa":…,"uv":…,"vpd_kpa":…,"origen":"simulado"}` |
| chia-01 | Publicar | `sabana/chia/chia-01/alerta` | 1 | `{"estado":"ALERTA","causa":"descenso+VPD","origen":"simulado"}` |
| chia-01 | Suscribirse | `sabana/chia/chia-01/cmd/desactivar_alarma` | — | — |
| Plataforma | Publicar | `sabana/chia/chia-01/cmd/desactivar_alarma` | 1 | `{"orden":"desactivar_alarma"}` |

La aplicación *MQTT Client* de Packet Tracer 9 **no permite configurar la última voluntad (LWT)**: el CONNECT se registra con `"will":{}`. Por eso el tópico `.../estado` no se valida en la simulación.

Los scripts de `scripts/` son una alternativa a la aplicación de escritorio para enviar telemetría periódica. No se usaron en la validación, y para usarlos hay que adaptar sus llamadas MQTT a la API de Packet Tracer (ver `scripts/README.md`).

### 3.7. Tablero del usuario (HTTP)

En el Servidor-Web: *Services* → *HTTP* activado; `index.html` con una página estática que describe el tablero y los tópicos (no muestra datos en vivo). Desde el Usuario-Alcaldia: *Desktop* → *Web Browser* → `http://200.10.10.30`.

## 4. Pruebas

| # | Prueba | Qué debe verse | Captura |
| :--- | :--- | :--- | :--- |
| P1 | `ping 200.10.10.10` desde un nodo | Respuestas ICMP | `03-ping-nodo-broker.png` |
| P2 | Conexión de la Plataforma | CONNECT y CONNACK con `returnCode 0` | `05-connect-connack.png`, `05b-simulacion-mqtt.png` |
| P3 | Suscripción de la Plataforma | SUBSCRIBE y SUBACK a `sabana/#` | `06-subscribe.png` |
| P4 | Telemetría de chia-01 | PUBLISH del nodo y mensaje en la Plataforma | `07-publish-telemetria-a.png`, `07-publish-telemetria-b.png` |
| P5 | Telemetría de los dos municipios | Mensajes de Chía y Cajicá en la Plataforma | `07b-telemetria-dos-municipios-a.png`, `-b.png`, `05c-plataforma-event-log.png` |
| P6 | Alerta con QoS 1 | PUBLISH y PUBACK | `08-alerta-qos1-a.png`, `-b.png`, `-c.png` |
| P7 | Orden de desactivar la alarma | PUBLISH de la Plataforma, PUBACK y recepción en chia-01 | `10-orden-desactivar-a.png`, `-b.png` |
| P8 | Estado y LWT | No aplica en Packet Tracer 9 (sin LWT) | — |
| P9 | Acceso del usuario | Página del Servidor-Web en el navegador | `11-http-usuario.png` |

En el modo *Simulation*, filtrar por los protocolos de interés (ICMP, TCP, DHCP, HTTP) y avanzar con *Capture/Forward*.

## 5. Diseño real frente a la simulación

| Elemento | Diseño real | Simulación |
| :--- | :--- | :--- |
| Gateway del municipio | SBC con broker local que reenvía `sabana/{municipio}/#` al broker de la nube | Router inalámbrico WRT300N con NAT |
| Estado de conexión | LWT `offline` y `online` retenidos en `.../estado` | No simulado: la aplicación no permite LWT |
| Seguridad de MQTT | TLS en el puerto 8883 y credenciales por nodo | Puerto 1883; credenciales visibles en texto plano en el CONNECT |
| Red de la nube | Servidor cableado o servicio en la nube | Punto de acceso sin autenticación, por la limitación de la SBC-PT |
| Datos | Mediciones de los sensores | Valores escritos a mano, marcados `"origen":"simulado"` |

## 6. Fallas encontradas (troubleshooting)

| # | Síntoma | Causa | Solución |
| :--- | :--- | :--- | :--- |
| 1 | Los nodos de Chía no se asociaban al router. | Seguridad Wi-Fi distinta entre el router y las SBC. | Igualar el SSID y la seguridad. |
| 2 | Nodos con IPv4 0.0.0.0. | Tras cambiar la IP de la LAN, el DHCP del WRT300N quedó con inicio en 1 y 1 usuario como máximo. | Rango desde .100 con 50 usuarios y renovar el DHCP. |
| 3 | La SBC gateway de Chía (IP estática) no respondía y la interfaz gráfica del router de Cajicá no dejaba configurar el DHCP. | No determinada dentro del simulador. | Router como gateway e IP estática en cajica-01. |

## 7. Capturas

En `capturas/`:

| Archivo | Contenido | Estado |
| :--- | :--- | :--- |
| `01-topologia.png` | Topología completa en modo lógico | No tomada |
| `02-direccionamiento-router-isp.png` | `show ip interface brief` y `show ip route` del ISP | Tomada antes de conectar los enlaces WAN (g0/0 y g0/1 *down*, sin rutas estáticas) |
| `03-ping-nodo-broker.png` | P1 | No tomada |
| `04-broker-config.png` | Aplicación del broker con los usuarios | No tomada |
| `05-connect-connack.png` | P2, registro de la Plataforma | Tomada |
| `05b-simulacion-mqtt.png` | P2 en el modo Simulation | No tomada |
| `05c-plataforma-event-log.png` | Registro completo de la Plataforma (P3, P5, P6, P7 y `"will":{}`) | Tomada |
| `06-subscribe.png` | P3 | Tomada |
| `07-publish-telemetria-a.png` / `-b.png` | P4: publicación en chia-01 / mensaje en la Plataforma | Tomadas |
| `07b-telemetria-dos-municipios-a.png` / `-b.png` | P5: registro de la Plataforma / publicación en cajica-01 | Tomadas |
| `08-alerta-qos1-a.png` / `-b.png` / `-c.png` | P6: publicación / registro de chia-01 con PUBACK / mensaje en la Plataforma | Tomadas |
| `10-orden-desactivar-a.png` / `-b.png` | P7: publicación en la Plataforma / suscripción de chia-01 | Tomadas |
| `11-http-usuario.png` | P9 | Tomada |
| Fallas 1 a 3, antes y después | Troubleshooting | No tomadas |
