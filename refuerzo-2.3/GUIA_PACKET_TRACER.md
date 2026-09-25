# Guía de Packet Tracer — Actividad de refuerzo 2.3

Guía paso a paso para construir y validar en Cisco Packet Tracer 8.x la red de monitoreo hídrico de la página de Wiki «Actividad de refuerzo 2.3 — Conectividad IoT».

> **Cómo leer esta guía.** Los comandos de Cisco IOS (configuración de routers) son estándar. Los nombres de menús, módulos y plantillas de Packet Tracer marcados con **(verificar en PT)** no se pudieron confirmar en una fuente pública: compruébalos en tu versión de Packet Tracer y corrige la guía si difieren. Anota la versión exacta de Packet Tracer (menú *Help → About*) para la Wiki.

## 0. Qué se va a construir

* **Chía:** dos nodos (`chia-01`, `chia-02`) y el gateway SBC, conectados por Wi-Fi al router inalámbrico de Chía.
* **Cajicá:** un nodo (`cajica-01`) y el gateway SBC, conectados por Wi-Fi al router inalámbrico de Cajicá.
* **ISP:** un router que une los dos municipios con la red de la plataforma.
* **Nube:** un switch con el broker MQTT (SBC), el cliente de la plataforma (SBC) y el servidor web del tablero (Server-PT).
* **Usuario:** un PC con navegador.

(El diagrama definitivo del ecosistema está en Mermaid en la Wiki; este esquema solo orienta el montaje.)

## 1. Dispositivos

| Rol | Dispositivo de Packet Tracer | Cantidad |
| :--- | :--- | :--- |
| Router inalámbrico de cada municipio | Home Gateway o router inalámbrico WRT300N **(verificar en PT cuál permite IP estática en la WAN)** | 2 |
| Router del proveedor de Internet (ISP) | Router 2911 (o cualquiera con 3 interfaces) | 1 |
| Switch de la nube | Switch 2960 | 1 |
| Nodos de monitoreo | SBC-PT (o MCU-PT) con adaptador inalámbrico | 3 |
| Gateway de cada municipio | SBC-PT | 2 |
| Broker MQTT de la plataforma | SBC-PT | 1 |
| Cliente de la plataforma (suscriptor) | SBC-PT | 1 |
| Servidor web del tablero | Server-PT | 1 |
| Usuario final | PC-PT (o Smartphone) | 1 |

**Adaptador inalámbrico de las SBC/MCU:** en el dispositivo, botón *Advanced* → pestaña *I/O Config* → *Network Adapter* → elegir el módulo inalámbrico (por ejemplo `PT-IOT-NM-1W`) **(verificar en PT)**.

## 2. Direccionamiento

| Dispositivo | Interfaz | Dirección | Puerta de enlace |
| :--- | :--- | :--- | :--- |
| Router inalámbrico Chía | LAN | 192.168.10.1/24 | — |
| Router inalámbrico Chía | Internet (WAN) | 10.0.0.1/30 | 10.0.0.2 |
| Gateway SBC Chía | Wi-Fi | 192.168.10.2/24 (estática) | 192.168.10.1 |
| Nodos de Chía | Wi-Fi | DHCP (192.168.10.100 en adelante) | 192.168.10.1 |
| Router inalámbrico Cajicá | LAN | 192.168.20.1/24 | — |
| Router inalámbrico Cajicá | Internet (WAN) | 10.0.0.5/30 | 10.0.0.6 |
| Gateway SBC Cajicá | Wi-Fi | 192.168.20.2/24 (estática) | 192.168.20.1 |
| Nodo de Cajicá | Wi-Fi | DHCP (192.168.20.100 en adelante) | 192.168.20.1 |
| Router ISP | G0/0 (hacia Chía) | 10.0.0.2/30 | — |
| Router ISP | G0/1 (hacia Cajicá) | 10.0.0.6/30 | — |
| Router ISP | G0/2 (hacia la nube) | 200.10.10.1/24 | — |
| Broker MQTT | Ethernet | 200.10.10.10/24 | 200.10.10.1 |
| Plataforma (suscriptor) | Ethernet | 200.10.10.20/24 | 200.10.10.1 |
| Servidor web | Ethernet | 200.10.10.30/24 | 200.10.10.1 |

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

> Si los routers inalámbricos hacen NAT en su WAN (lo habitual en un Home Gateway), el ISP solo verá las direcciones 10.0.0.1 y 10.0.0.5, y las rutas hacia 192.168.x.0 no se usarán. Funciona igual para MQTT, porque los clientes inician la conexión hacia el broker. Anótalo en la Wiki si es tu caso.

### 3.2. Routers inalámbricos de Chía y Cajicá (interfaz gráfica)

1. Pestaña *Config* → *Internet*: IP estática 10.0.0.1/30, puerta de enlace 10.0.0.2 (Cajicá: 10.0.0.5/30 y 10.0.0.6).
2. *LAN*: 192.168.10.1/24 (Cajicá: 192.168.20.1/24).
3. *DHCP*: activado, desde 192.168.10.100 (Cajicá: 192.168.20.100).
4. *Wireless*: SSID `WLAN-Chia` (Cajicá: `WLAN-Cajica`) y seguridad WPA2-PSK con una clave de prueba.
5. Cablear la WAN de cada router a la interfaz correspondiente del router ISP.

### 3.3. Red de la nube

Conectar el broker, la plataforma y el servidor web al switch, y el switch a G0/2 del router ISP. Configurar IP y puerta de enlace estáticas según la tabla del paso 2 (pestaña *Config* de cada dispositivo).

### 3.4. Nodos y gateways (SBC)

1. Cambiar el adaptador a inalámbrico (paso 1).
2. *Config* → interfaz inalámbrica: SSID y clave del municipio.
3. Nodos: IP por DHCP. Gateways: IP estática según la tabla.
4. Verificar con `ping` desde el escritorio (si la SBC tiene *Desktop* → *Command Prompt*) o desde el PC del usuario **(verificar en PT)**.

### 3.5. Broker MQTT

Packet Tracer trae el broker y el cliente MQTT como proyectos de ejemplo en la pestaña *Programming* de la SBC-PT, y un botón *Install to Desktop* los agrega como aplicaciones del *Desktop*.

1. En la SBC del broker (200.10.10.10): *Programming* → nuevo proyecto a partir de la plantilla **MQTT Broker** **(verificar el nombre exacto de la plantilla en PT)**.
2. Abrir el proyecto y pulsar *Install to Desktop*.
3. *Desktop* → aplicación **MQTT Broker**: puerto 1883, crear el usuario y la clave que usarán los clientes (por ejemplo `nodo` / `clave-nodo`) y encenderlo.

### 3.6. Clientes MQTT

**Opción A (recomendada para la demostración): aplicación de escritorio.** En cada SBC cliente: *Programming* → plantilla **MQTT Client** → *Install to Desktop* → *Desktop* → **MQTT Client**. Llenar la dirección del broker (200.10.10.10), el usuario y la clave, y conectar. Después:

| Cliente | Acción | Tópico |
| :--- | :--- | :--- |
| Plataforma (200.10.10.20) | Suscribirse | `sabana/+/+/telemetria`, `sabana/+/+/alerta`, `sabana/+/+/estado` |
| Nodo chia-01 | Publicar | `sabana/chia/chia-01/telemetria` con el JSON de ejemplo de la Wiki |
| Nodo chia-01 | Publicar | `sabana/chia/chia-01/alerta` con `{"estado":"ALERTA","causa":"descenso+VPD"}` |
| Nodo chia-01 | Suscribirse | `sabana/chia/chia-01/cmd/desactivar_alarma` |
| Plataforma | Publicar | `sabana/chia/chia-01/cmd/desactivar_alarma` con `{"orden":"desactivar"}` |

Si la aplicación permite configurar la última voluntad (LWT), usar tópico `sabana/chia/chia-01/estado`, mensaje `offline`, QoS 1 y retenido **(verificar en PT si la app lo permite; si no, documentarlo como limitación de la simulación)**.

**Opción B: scripts de Python** (`refuerzo-2.3/scripts/`). Envían telemetría periódica y reenvían por el gateway. Requieren adaptar las llamadas MQTT a la API del proyecto de ejemplo *MQTT Client* de Packet Tracer (ver `scripts/README.md`).

### 3.7. Gateway de cada municipio

En el diseño, cada gateway reúne los mensajes de su WLAN y los reenvía a la nube. En Packet Tracer hay dos formas de representarlo:

1. **Reenvío con script** (`scripts/gateway_reenvio.py`): la SBC del gateway se suscribe a `sabana/chia/#` en un broker local y republica en el broker de la nube. Requiere que Packet Tracer permita un broker en el gateway y dos conexiones de cliente en el mismo script **(verificar en PT)**.
2. **Simplificación:** si lo anterior no es posible, los nodos publican directamente en el broker de la nube y el router inalámbrico del municipio cumple la función de gateway hacia Internet. Declararlo en la Wiki como simplificación de la simulación.

### 3.8. Tablero del usuario (HTTP)

En el Server-PT (200.10.10.30): *Services* → *HTTP* → activado; editar `index.html` con una página que describa el tablero de la plataforma (en la simulación no muestra datos en vivo). Desde el PC del usuario: *Desktop* → *Web Browser* → `http://200.10.10.30`.

## 4. Pruebas en el modo Simulation

1. Pasar al modo *Simulation* (esquina inferior derecha).
2. En *Edit Filters*, dejar solo los protocolos de interés: ICMP, TCP, DHCP, HTTP y MQTT si aparece en la lista **(verificar en PT)**.
3. Ejecutar cada prueba y avanzar con *Capture/Forward* hasta ver el paquete llegar.

| # | Prueba | Qué debe verse |
| :--- | :--- | :--- |
| P1 | `ping 200.10.10.10` desde un nodo de Chía | Respuestas ICMP (primero en modo *Realtime*) |
| P2 | Conexión de cada cliente MQTT | Paquetes CONNECT del cliente y CONNACK del broker |
| P3 | Suscripción de la plataforma | SUBSCRIBE y SUBACK |
| P4 | Publicación de telemetría de Chía y de Cajicá | PUBLISH del nodo al broker y del broker a la plataforma |
| P5 | Alerta con QoS 1 | PUBLISH y PUBACK |
| P6 | Estado y LWT | Al desconectar un nodo (por ejemplo, apagando su adaptador), la plataforma recibe `offline` |
| P7 | Orden de desactivar la alarma | PUBLISH de la plataforma al broker y del broker al nodo |
| P8 | Acceso del usuario | Petición HTTP del PC y respuesta del servidor web |

## 5. Resolución de problemas (provocar y corregir 3 fallas)

Provocar al menos tres fallas, capturar el síntoma, corregir y capturar el resultado. Sugerencias:

| Falla provocada | Síntoma esperado | Corrección |
| :--- | :--- | :--- |
| Puerta de enlace errónea en un nodo | `ping` al broker falla; el nodo no conecta | Corregir la puerta de enlace |
| Falta la ruta estática en el router ISP (si no hay NAT) | Sin respuesta desde la nube hacia el municipio | Agregar `ip route` |
| Tópico mal escrito en el nodo (`sabana/chia/chia-01/telemetira`) | La plataforma no recibe la telemetría aunque el nodo publica | Corregir el tópico |
| Usuario o clave MQTT errónea | El broker rechaza la conexión | Usar las credenciales creadas en el broker |
| SSID o clave Wi-Fi errónea | El nodo no se asocia al router | Corregir la configuración inalámbrica |

Registrar cada falla en la tabla de la sección 9 de la página de Wiki: síntoma, causa, solución y capturas de antes y después.

## 6. Lista de capturas

Guardar en `refuerzo-2.3/capturas/` con estos nombres:

- [ ] `01-topologia.png` — topología completa en modo lógico.
- [ ] `02-direccionamiento-router-isp.png` — `show ip interface brief` y `show ip route` del router ISP.
- [ ] `03-ping-nodo-broker.png` — prueba P1.
- [ ] `04-broker-config.png` — aplicación MQTT Broker configurada.
- [ ] `05-connect-connack.png` — prueba P2 en modo Simulation.
- [ ] `06-subscribe.png` — prueba P3.
- [ ] `07-publish-telemetria.png` — prueba P4 (mensaje recibido en la plataforma).
- [ ] `08-alerta-qos1.png` — prueba P5.
- [ ] `09-lwt-offline.png` — prueba P6 (o nota de limitación).
- [ ] `10-orden-desactivar.png` — prueba P7.
- [ ] `11-http-usuario.png` — prueba P8.
- [ ] `12-falla1-antes.png` / `13-falla1-despues.png`
- [ ] `14-falla2-antes.png` / `15-falla2-despues.png`
- [ ] `16-falla3-antes.png` / `17-falla3-despues.png`

Guardar también el archivo de Packet Tracer como `refuerzo-2.3/red-sabana-centro.pkt`.
