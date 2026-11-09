# Diseño del módulo de inteligencia artificial — Challenge #3

Diseño del 29/10/2026. Responde al requerimiento del enunciado: un módulo de IA que use los datos del ecosistema IoT para
generar predicción, detección de anomalías o recomendación, como apoyo a las decisiones de las autoridades, con su uso
justificado. El módulo corre en el gateway (Raspberry Pi 5) sobre el histórico guardado en SQLite ([gateway.md](gateway.md)).

## 1. Qué decisión apoya

Las autoridades necesitan saber con anticipación si un tanque llegará al nivel crítico y cuándo, para decidir racionamiento
o abastecimiento alterno. La alarma del nodo avisa cuando el riesgo ya existe; el módulo de IA estima cuánto tiempo queda y
señala lecturas que no son confiables, para no tomar decisiones sobre datos erróneos.

## 2. Componentes

| Componente | Técnica | Entrada | Salida |
| :--- | :--- | :--- | :--- |
| Predicción del tiempo hasta el nivel crítico | Regresión lineal por mínimos cuadrados del nivel sobre las últimas 24 h de promedios de 5 min (6 h mientras no haya 24 h), con el error estándar de la pendiente | Tabla `agregados_5min` | Horas hasta el umbral crítico, con un intervalo; nulo si el nivel no baja de forma significativa |
| Detección de anomalías | Isolation Forest (aprendizaje no supervisado) entrenado con el histórico de operación, con la hora del día como variable, más reglas físicas | Tabla `telemetria` y `agregados_5min` | Indicador de anomalía y su tipo |
| Recomendación en texto | Texto armado con reglas a partir de los resultados anteriores; redacción opcional con la API de IA generativa Gemini (implementada, desactivada sin clave en el prototipo) | Estado, horas hasta el nivel crítico, anomalías, ET0 | Texto corto de recomendación para las autoridades |

### 2.1. Predicción del tiempo hasta el nivel crítico

- Ventana: 288 promedios de 5 min (24 h), que promedian el ciclo diario de consumo; mientras no haya 24 h de datos, 72 promedios (6 h). Se recalcula cada 5 min, con cada promedio nuevo. La evaluación con datos sintéticos justificó el cambio de 6 h a 24 h: con un consumo que varía ±50 % a lo largo del día, el error absoluto medio bajó de 13.3 h a 4.2 h ([evaluación](../../tests/datos/ia/evaluacion-sintetica.md)).
- Modelo: nivel(t) = a + b·t. Si la pendiente b es negativa y su intervalo de confianza del 95 % no incluye el cero, las horas restantes son (nivel actual − umbral crítico) / |b|.
- Intervalo: el mismo cálculo con los extremos del intervalo de confianza de b da el rango de horas.
- Variables adicionales: se evalúa si agregar VPD y temperatura como regresores reduce el error frente a la regresión simple; se adopta solo si lo reduce en los datos de prueba.

### 2.2. Detección de anomalías

| Tipo | Cómo se detecta |
| :--- | :--- |
| Lectura imposible | Fuera del rango del sensor ([gateway.md](gateway.md), sección 2) |
| Salto de nivel | Cambio entre mensajes consecutivos mayor que el físicamente posible para el recipiente |
| Sensor congelado | El mismo valor exacto durante un número de mensajes en que el ruido normal del sensor lo haría variar |
| Combinación atípica | Isolation Forest sobre (tendencia del nivel, temperatura, humedad, VPD, UV y hora del día como seno y coseno): marca combinaciones que no aparecen en la operación normal, por ejemplo UV alto de noche o un descenso rápido sin calor. Umbral: contaminación 0.05, elegida con la evaluación sintética (precisión 0.82, exhaustividad 0.83) y por recalibrar con datos medidos |

Isolation Forest se elige porque no necesita ejemplos etiquetados de anomalías, que el proyecto no tiene, y porque se entrena
y ejecuta en la Raspberry Pi con la librería scikit-learn.

### 2.3. Recomendación en texto

- La API de IA generativa no decide ni detecta: recibe los resultados ya calculados (estado, horas hasta el nivel crítico con su intervalo, anomalías y ET0) y los redacta como una recomendación breve en español para las autoridades.
- Se llama solo en un cambio de estado de alerta o, como máximo, una vez por hora, para limitar el costo y el uso de la cuota.
- Instrucción fija y verificable: no inventar datos, citar los valores recibidos y marcar el texto como sugerencia.
- Sin Internet o sin respuesta de la API: el gateway usa un texto de respaldo armado con reglas a partir de los mismos resultados.
- La clave de la API vive solo en el archivo `.env` del gateway, fuera del repositorio.
- Límites del plan gratuito de la API: por verificar antes de implementar.

## 3. Justificación técnica

- Pertinencia: la pregunta guía pide alertas tempranas; la predicción convierte la tendencia del nivel en un tiempo restante, que es la información que necesita una autoridad para decidir un racionamiento.
- Confiabilidad: las alertas de la fusión dependen de datos válidos; la detección de anomalías evita que una lectura errónea dispare o oculte una alerta en el tablero global.
- Explicabilidad: la predicción es una regresión con intervalo, que se puede verificar a mano; la IA generativa solo redacta y nunca sustituye un cálculo.
- Recursos: los tres componentes corren en el gateway; el ESP32 no cambia y la alarma in situ no depende de ellos.

## 4. Evaluación

| Componente | Métrica | Datos de prueba |
| :--- | :--- | :--- |
| Predicción | Error absoluto medio entre las horas predichas y el momento real en que el nivel cruzó el umbral | Descensos provocados en el recipiente de demostración, registrados en SQLite |
| Anomalías | Precisión y exhaustividad (recall) sobre anomalías inyectadas: lecturas fuera de rango, saltos y sensor congelado | Datos de operación normal con anomalías insertadas a propósito y marcadas |
| Recomendación | Revisión de que cada texto cita solo valores presentes en la entrada | Muestra de textos generados en los escenarios de prueba |

Resultados con datos sintéticos de respuesta conocida (SIMULACIÓN): [evaluacion-sintetica.md](../../tests/datos/ia/evaluacion-sintetica.md), generados con `gateway/herramientas/evaluar_ia.py`.

## 5. Dónde se muestran los resultados

- SQLite: tabla `ia_resultados` ([gateway.md](gateway.md)).
- Ubidots: dispositivo `gw-chia`, variables `horas_hasta_critico` (−1 si el nivel no baja), `anomalia` (0 o 1, con los tipos en el contexto) y `recomendacion` (texto en el contexto del dato, máximo 1 KB). El widget que muestre ese texto se define al armar el tablero.
- Tablero local (opcional): estado de la conexión MQTT y predicción.
