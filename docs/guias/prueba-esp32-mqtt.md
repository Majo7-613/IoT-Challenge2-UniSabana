# Guía: prueba del ESP32 real con el broker del computador

Valida la comunicación MQTT del firmware con el prototipo físico antes de tener lista la Raspberry Pi: el Mosquitto del
computador de desarrollo hace de broker del gateway. Tiempo estimado: 45 min.

Requisitos: el ESP32 montado y conectado por USB al computador, el computador y el ESP32 en la misma red Wi-Fi, y
Mosquitto instalado en el computador (servicio de Windows).

## 1. Mosquitto del computador con usuarios y permisos

En una terminal de PowerShell **como administrador**:

```powershell
cd "C:\Program Files\mosquitto"
.\mosquitto_passwd.exe -c passwd nodo-chia-01        # pide la clave del nodo
.\mosquitto_passwd.exe passwd gw-chia
.\mosquitto_passwd.exe passwd monitor
Copy-Item C:\IoT\IoT-Challenge2-UniSabana\Repo\gateway\despliegue\mosquitto\acl acl
```

Agregar al final de `C:\Program Files\mosquitto\mosquitto.conf` (con el Bloc de notas como administrador):

```text
listener 1883
allow_anonymous false
password_file C:/Program Files/mosquitto/passwd
acl_file C:/Program Files/mosquitto/acl
```

Abrir el puerto 1883 solo para redes privadas y reiniciar el servicio:

```powershell
New-NetFirewallRule -DisplayName "Mosquitto 1883 (prueba IoT)" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow -Profile Private
Restart-Service mosquitto
```

La red Wi-Fi del computador debe estar marcada como **privada** en Windows (Configuración → Red → propiedades).
Anotar la IP del computador (`ipconfig`, adaptador Wi-Fi, «Dirección IPv4»).

Al terminar las pruebas, para volver al estado anterior: borrar las líneas agregadas a `mosquitto.conf`, borrar la regla
(`Remove-NetFirewallRule -DisplayName "Mosquitto 1883 (prueba IoT)"`) y reiniciar el servicio.

## 2. Credenciales en el firmware

En `firmware/include/secrets.h` (no se sube al repositorio), con la IP del computador:

```c
#define MQTT_BROKER_URI  "mqtt://192.168.1.23:1883"
#define MQTT_USUARIO     "nodo-chia-01"
#define MQTT_CLAVE       "<clave del nodo>"
```

## 3. Cargar el firmware y guardar el registro

```bash
cd C:/IoT/IoT-Challenge2-UniSabana/Repo
~/.platformio/penv/Scripts/pio.exe run -d firmware -e esp32dev -t upload
~/.platformio/penv/Scripts/pio.exe device monitor -d firmware | tee tests/datos/fisico/esp32-mqtt-AAAAMMDD.log
```

En otra terminal, el monitor del broker y el gateway:

```bash
"C:/Program Files/mosquitto/mosquitto_sub.exe" -u monitor -P "<clave-monitor>" -t "sabana/#" -v -F "%I %t %p"
cd gateway && .venv/Scripts/python -m gateway.principal     # .env con MQTT_USUARIO=gw-chia
```

## 4. Pruebas

| # | Prueba | Cómo | Esperado |
| :--- | :--- | :--- | :--- |
| 1 | Conexión | Encender el ESP32 | Registro serie: `[mqtt] conectado al broker`; monitor: `sabana/chia/chia-01/estado online` |
| 2 | Telemetría | Esperar 1 min | 12 mensajes de telemetría (uno cada 5 s) con los valores de los sensores |
| 3 | Alerta | Bajar el nivel del recipiente hasta ADVERTENCIA y CRÍTICO | Un mensaje `alerta` por cada cambio de estado |
| 4 | Orden en NORMAL | `mosquitto_pub -u gw-chia -P <clave> -q 1 -t sabana/chia/chia-01/cmd/desactivar_alarma -m '{"orden_id":"p-1"}'` | `cmd/respuesta` con `rechazada` y `motivo: estado NORMAL`; el buzzer no cambia |
| 5 | Orden en CRÍTICO | La misma orden con `p-2` en CRÍTICO | `cmd/respuesta` con `aceptada`; el buzzer se silencia y la LCD muestra `(DESACT.)` |
| 6 | Última voluntad | Desconectar la alimentación del ESP32 | Monitor: `estado offline` en ≤ 45 s (1.5 × keepalive de 30 s) |
| 7 | Reconexión | Detener el servicio de Mosquitto 30 s y volver a iniciarlo | Registro serie: `reconectado al broker en … ms`; la telemetría continúa |
| 8 | Medición sin red | Durante la prueba 7, observar la LCD y el buzzer | La medición y la alarma in situ siguen funcionando |
| 9 | Ciclo de 1 s | Registro serie durante 10 min | `desv` ≤ 2 ms y `perdidos=0` con MQTT activo |
| 10 | Base del gateway | `gateway.db` tras 10 min | ≈ 120 telemetrías, las alertas y las órdenes con su resultado |

Guardar el registro serie, la salida del monitor y `gateway.db` en `tests/datos/fisico/`; estos resultados son físicos
(no de simulación) y van al capítulo 4 de la Wiki con fecha y repeticiones.
