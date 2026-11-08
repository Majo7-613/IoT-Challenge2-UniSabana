# Guía: tablero global en Ubidots (cuenta STEM)

Configuración de la cuenta gratuita Ubidots STEM para el tablero global. Límites de la cuenta y decisiones de diseño:
[comunicacion-mqtt.md](../diseno/comunicacion-mqtt.md), secciones 4 a 7.

## 1. Crear la cuenta

1. Entrar a ubidots.com/stem y crear la cuenta con el correo del equipo. La cuenta STEM es de uso no comercial
   (educación), que corresponde al proyecto.
2. Verificar el correo.

## 2. Token y datos de conexión MQTT

1. Menú del usuario → **API Credentials** (credenciales de la API): copiar el **Default Token**.
2. En la documentación de Ubidots para MQTT, confirmar y anotar:
   - servidor (por omisión en el gateway: `industrial.api.ubidots.com`);
   - puerto con TLS (por omisión: 8883) y sin TLS;
   - cómo se usa el token (por omisión en el gateway: como nombre de usuario, con contraseña vacía);
   - tópico para publicar en un dispositivo (por omisión: `/v1.6/devices/{dispositivo}`);
   - tópico para recibir el último valor de una variable (por omisión: `/v1.6/devices/{dispositivo}/{variable}/lv`).
3. Si algún dato difiere, cambiarlo en el `.env` del gateway (`UBIDOTS_HOST`, `UBIDOTS_PUERTO`,
   `UBIDOTS_TOPICO_PUBLICAR`, `UBIDOTS_TOPICO_ORDEN`); no hace falta tocar el código.
4. Poner el token en el `.env` del gateway: `UBIDOTS_TOKEN=<token>`. El token nunca va al repositorio ni a la Wiki.

## 3. Dispositivos y variables

Los dispositivos y sus variables se crean solos con el primer dato que envía el gateway. Para crearlos antes y probar
el tablero, desde el computador con el gateway configurado:

```bash
cd gateway
.venv/Scripts/python -m gateway.principal      # con el publicador de prueba corriendo en otra terminal
```

| Dispositivo | Variable | Contenido |
| :--- | :--- | :--- |
| `chia-01` | `nivel_pct`, `nivel_cm`, `tendencia`, `temperatura`, `humedad`, `presion`, `uv`, `vpd` | Promedios de 5 min |
| `chia-01` | `estado` | Gravedad del estado: 0 INICIANDO, 1 NORMAL, 2 ADVERTENCIA, 3 FALLA NIVEL, 4 ALERTA, 5 CRÍTICO; el texto va en el contexto |
| `gw-chia` | `nodo_en_linea` | 1 en línea, 0 fuera de línea |
| `gw-chia` | `et0` | ET0 diaria (mm/d) |
| `gw-chia` | `horas_hasta_critico` | Predicción de la IA; −1 si el nivel no baja |
| `gw-chia` | `anomalia` | 1 si la IA detecta una anomalía; los tipos en el contexto |
| `gw-chia` | `recomendacion` | Texto de la recomendación en el contexto |
| `gw-chia` | `desactivar_alarma` | Control del tablero: 1 = orden de desactivar; el gateway lo devuelve a 0 |
| `gw-chia` | `orden_resultado` | 1 aceptada, 0 rechazada; motivo en el contexto |

El dispositivo `chia-01` usa 9 de las 10 variables permitidas y `gw-chia`, 7.

## 4. Tablero (máximo 10 widgets en STEM)

| # | Widget | Variable | Uso |
| :--- | :--- | :--- | :--- |
| 1 | Indicador (gauge) | `chia-01/nivel_pct` | Nivel actual |
| 2 | Gráfica de líneas | `chia-01/nivel_pct` y `chia-01/tendencia` | Histórico del nivel |
| 3 | Gráfica de líneas | `chia-01/temperatura`, `humedad` y `vpd` | Histórico meteorológico |
| 4 | Métrica | `chia-01/estado` | Estado de alerta (con colores por valor) |
| 5 | Métrica | `chia-01/uv` y `presion` | Valores actuales |
| 6 | Métrica | `gw-chia/horas_hasta_critico` | Predicción de la IA |
| 7 | Métrica o tabla | `gw-chia/anomalia` y `recomendacion` | Anomalías y recomendación (mostrar el contexto) |
| 8 | Interruptor (switch) | `gw-chia/desactivar_alarma` | Desactivar la alarma física |
| 9 | Métrica | `gw-chia/orden_resultado` | Resultado de la última orden |
| 10 | Indicador | `gw-chia/nodo_en_linea` y `et0` | Conexión del nodo y ET0 del día |

## 5. Notificaciones (eventos)

1. **Data → Events → Create Event**.
2. Condición: `chia-01/estado` **mayor o igual que 4** (ALERTA o CRÍTICO).
3. Acción: **Send Email** a los correos de las autoridades de prueba, con el valor y el contexto.
4. Período de espera (cooldown): 600 s, para no gastar los 100 correos mensuales de la cuenta.
5. Repetir para `gw-chia/nodo_en_linea` igual a 0 (nodo fuera de línea).

Los SMS gratuitos de Ubidots no llegan a Colombia; por eso las notificaciones son por correo.

## 6. Acceso restringido

- El tablero se consulta solo iniciando sesión con la cuenta del equipo.
- **No crear enlaces públicos**: un enlace público no pide inicio de sesión y deja operable el interruptor de
  desactivar la alarma.
- La clave de la cuenta la conocen solo los integrantes y las autoridades autorizadas.

## 7. Prueba de punta a punta

1. Con el gateway y el publicador de prueba (o el ESP32) funcionando, revisar que lleguen datos a `chia-01`.
2. Esperar un cambio de estado a ALERTA o CRÍTICO y comprobar que llega el correo.
3. Activar el interruptor `desactivar_alarma` en ALERTA o CRÍTICO: el nodo debe responder y `orden_resultado` debe
   pasar a 1; en NORMAL debe pasar a 0 con el motivo «estado NORMAL».
4. Medir el tiempo entre el cambio de estado en el nodo y su aparición en Ubidots (tarea 51 del plan).
