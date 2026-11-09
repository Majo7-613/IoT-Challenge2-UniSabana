# Guía: Raspberry Pi 5 como gateway

Instalación paso a paso del gateway en la Raspberry Pi 5 (microSD de 128 GB, fuente oficial USB-C). No necesita monitor
ni teclado: todo se hace desde un computador por SSH. Tiempo estimado: 1 a 2 horas.

Diseño: [gateway.md](../diseno/gateway.md). Código: [gateway/](../../gateway).

## 1. Grabar el sistema en la microSD (en el computador)

1. Descargar e instalar **Raspberry Pi Imager** desde raspberrypi.com/software.
2. Abrirlo y elegir:
   - Dispositivo: **Raspberry Pi 5**.
   - Sistema operativo: **Raspberry Pi OS (other) → Raspberry Pi OS Lite (64-bit)**.
   - Almacenamiento: la microSD.
3. En **Editar ajustes** (o «Personalizar»), antes de grabar:
   - Nombre del equipo: `gateway-chia`.
   - Usuario: `equipo7` y una contraseña (guardarla en un lugar seguro; no va al repositorio).
   - Wi-Fi: el nombre y la clave del punto de acceso del celular (el mismo que usará el ESP32). País: CO.
   - Zona horaria: America/Bogota.
   - Pestaña **Servicios**: activar SSH con autenticación por contraseña.
4. Grabar y esperar a que termine la verificación.

## 2. Primer arranque y acceso por SSH

1. Poner la microSD en la Raspberry Pi y conectar la fuente oficial. Esperar unos 2 minutos.
2. Desde el computador, en una terminal:

   ```bash
   ssh equipo7@gateway-chia.local
   ```

   Aceptar la huella la primera vez (`yes`) y escribir la contraseña. Si `gateway-chia.local` no responde, buscar la IP
   de la Raspberry Pi en la lista de dispositivos conectados del punto de acceso del celular y usar `ssh equipo7@<IP>`.
3. Actualizar el sistema:

   ```bash
   sudo apt update && sudo apt full-upgrade -y
   sudo reboot
   ```

## 3. Red: punto de acceso del celular

La Raspberry Pi, el ESP32 y el computador se conectan al **mismo punto de acceso del celular** (el mismo de la
sustentación). Con un punto de acceso la IP de la Raspberry Pi puede cambiar cada vez que se enciende; por eso **no se
fija la IP**: el ESP32 busca la Raspberry Pi por su nombre, `gateway-chia.local`, y la vuelve a buscar sola si la IP
cambia.

Solo hay que comprobar que la Raspberry Pi anuncia su nombre:

```bash
hostname                                         # debe decir gateway-chia
systemctl status avahi-daemon --no-pager         # debe decir active (running)
hostname -I                                      # IP actual, solo como referencia
```

## 4. Instalar Mosquitto, Python y Git

```bash
sudo apt install -y mosquitto mosquitto-clients python3-venv git sqlite3
mosquitto -h | head -1          # versión instalada
```

## 5. Configurar Mosquitto (usuarios y permisos)

1. Descargar el repositorio:

   ```bash
   cd ~
   git clone https://github.com/Majo7-613/IoT-Challenge2-UniSabana.git
   ```

2. Copiar la configuración y los permisos:

   ```bash
   sudo cp ~/IoT-Challenge2-UniSabana/gateway/despliegue/mosquitto/gateway.conf /etc/mosquitto/conf.d/
   sudo cp ~/IoT-Challenge2-UniSabana/gateway/despliegue/mosquitto/acl /etc/mosquitto/acl
   ```

3. Crear los tres usuarios. Cada comando pide la clave dos veces; las claves no quedan en el historial de la terminal.
   Anotarlas en un lugar seguro: la del nodo va en `secrets.h` del ESP32 y la del gateway en el `.env`.

   ```bash
   sudo mosquitto_passwd -c /etc/mosquitto/passwd nodo-chia-01
   sudo mosquitto_passwd /etc/mosquitto/passwd gw-chia
   sudo mosquitto_passwd /etc/mosquitto/passwd monitor
   sudo chown mosquitto:mosquitto /etc/mosquitto/passwd /etc/mosquitto/acl
   sudo chmod 640 /etc/mosquitto/passwd /etc/mosquitto/acl
   ```

4. Reiniciar y comprobar:

   ```bash
   sudo systemctl restart mosquitto
   sudo systemctl status mosquitto --no-pager     # debe decir active (running)
   mosquitto_pub -t prueba -m x                    # debe fallar: no se aceptan clientes anónimos
   ```

5. Prueba con el usuario de diagnóstico (en dos terminales SSH):

   ```bash
   mosquitto_sub -u monitor -P '<clave-monitor>' -t 'sabana/#' -v
   ```

   Con el ESP32 encendido y configurado (sección 8) deben aparecer `online`, la telemetría cada 5 s y las alertas.

## 6. Instalar el gateway

```bash
cd ~/IoT-Challenge2-UniSabana/gateway
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python -m pytest -q                   # las pruebas deben pasar también en la Raspberry Pi
cp .env.example .env
nano .env
```

En `.env`: `MQTT_HOST=localhost`, `MQTT_USUARIO=gw-chia`, `MQTT_CLAVE=<clave del gateway>`, y el token de Ubidots
([guía de Ubidots](ubidots.md)). Guardar con Ctrl+O y salir con Ctrl+X. Proteger el archivo: `chmod 600 .env`.

Prueba manual (Ctrl+C para detener):

```bash
.venv/bin/python -m gateway.principal
```

## 7. Arranque automático

```bash
sudo cp ~/IoT-Challenge2-UniSabana/gateway/despliegue/gateway.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now gateway
sudo systemctl status gateway --no-pager
journalctl -u gateway -f                         # registro en vivo (Ctrl+C para salir)
```

Si el usuario no es `equipo7`, cambiar `User` y las rutas en `/etc/systemd/system/gateway.service` antes del
`daemon-reload`. Comprobar que tras `sudo reboot` el gateway arranca solo (criterio de la tarea 43 del plan).

## 8. Conectar el ESP32

En el computador, en `firmware/include/secrets.h` (no se sube al repositorio):

```c
#define MQTT_BROKER_URI  "mqtt://gateway-chia.local:1883"   // por nombre: la IP puede cambiar
#define MQTT_USUARIO     "nodo-chia-01"
#define MQTT_CLAVE       "<clave del nodo>"
```

Compilar y cargar el firmware (`pio run -d firmware -t upload`), abrir el monitor serie (`pio device monitor`) y
buscar `[mqtt] gateway-chia.local encontrado en ...` y `[mqtt] conectado al broker`.

## 9. Consultas útiles a la base de datos

```bash
sqlite3 ~/IoT-Challenge2-UniSabana/gateway/gateway.db
.headers on
.mode column
SELECT COUNT(*) FROM telemetria;
SELECT recibido_utc, estado, nivel_pct, t_c FROM telemetria ORDER BY id DESC LIMIT 5;
SELECT * FROM alertas ORDER BY id DESC LIMIT 5;
SELECT inicio_utc, n_mensajes, n_esperados, nivel_pct, estado_peor FROM agregados_5min ORDER BY id DESC LIMIT 5;
SELECT COUNT(*) FROM cola_nube WHERE enviado_utc IS NULL;   -- cargas pendientes hacia Ubidots
.quit
```

Exportar a CSV para el banco de pruebas:

```bash
sqlite3 -header -csv gateway.db "SELECT * FROM telemetria;" > telemetria.csv
```

## Problemas frecuentes

| Síntoma | Causa probable | Solución |
| :--- | :--- | :--- |
| `ssh: Could not resolve hostname gateway-chia.local` | El computador no resuelve nombres `.local` o no está en el mismo punto de acceso | Usar la IP de la Raspberry Pi (lista de dispositivos conectados del punto de acceso) |
| Mosquitto no arranca tras copiar la configuración | Error de sintaxis o archivo de claves con permisos incorrectos | `journalctl -u mosquitto -n 30` muestra la línea del error |
| El ESP32 registra «conexión rechazada por el broker» | Usuario o clave de `secrets.h` distintos de los de `mosquitto_passwd` | Volver a crear la clave del nodo y actualizar `secrets.h` |
| El ESP32 registra «no se encontró gateway-chia.local» | La Raspberry Pi no está en la misma red, está apagada o `avahi-daemon` está detenido | Revisar que ambos estén en el mismo punto de acceso; `sudo systemctl restart avahi-daemon`. Último recurso: poner en `secrets.h` la IP de `hostname -I` (`mqtt://<IP>:1883`) y volver a grabar el ESP32; esa IP puede cambiar al reiniciar el punto de acceso |
| El ESP32 registra «error de conexión con el broker» | Mosquitto detenido o la Raspberry Pi en otra red | `sudo systemctl status mosquitto`; revisar que ambos estén en el mismo punto de acceso |
| `gateway` en estado failed | `.env` ausente o con datos incorrectos | `journalctl -u gateway -n 50` |
