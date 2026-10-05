# Capturas de la simulación en Wokwi (SIMULACIÓN)

**Todo el contenido de esta carpeta es de SIMULACIÓN en Wokwi, no del hardware.**
Fecha: 05/10/2026. Firmware: commit `52aa808`, entorno `wokwi`. Simulación interactiva en la extensión de Wokwi para VS Code, con el tablero abierto por el reenvío de puertos de `wokwi.toml` (`localhost:8180`).

| Archivo | Contenido |
| :--- | :--- |
| `simulacion-wokwi-diagrama-normal.png`, `simulacion-wokwi-lcd-normal.png` | Diagrama y LCD (acercamiento ×3) en NORMAL, con el HC-SR04 a 13 cm |
| `simulacion-wokwi-diagrama-critico.png`, `simulacion-wokwi-lcd-critico-luz-encendida.png`, `simulacion-wokwi-lcd-critico-luz-apagada.png` | Diagrama y LCD en CRÍTICO (HC-SR04 a 30 cm); dos capturas tomadas con 250 ms de diferencia muestran el parpadeo de la retroiluminación |
| `simulacion-wokwi-lcd-critico-desactivada.png` | LCD en CRÍTICO después de la orden de desactivar: `CRITICO (DESACT.)` |
| `simulacion-wokwi-tablero-*-computador.png`, `simulacion-wokwi-tablero-*-celular.png` | Tablero servido por el ESP32 simulado en NORMAL, CRÍTICO y CRÍTICO con la alarma desactivada, en vista de computador (1366 px) y de celular (390 px) |
| `simulacion-wokwi-pruebas-acceso.txt` | Pruebas de control de acceso, histórico, archivos del tablero, WebSocket y desactivación, con lo esperado y lo observado |

Las capturas de la LCD se tomaron de la pantalla del computador con la simulación corriendo en VS Code y se recortaron a la zona del diagrama; las del tablero, con `tools/tablero_simulado/capturas.mjs --wokwi`.
