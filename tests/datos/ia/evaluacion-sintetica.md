# Evaluación del módulo de IA con datos sintéticos (SIMULACIÓN)

Generado con `gateway/herramientas/evaluar_ia.py` (semilla 2026). Los datos son sintéticos y de respuesta
conocida; la evaluación con datos medidos se hace con el prototipo físico.

## Predicción del tiempo hasta el nivel crítico

Descensos con pendiente uniforme entre 0.5 y 4 %/h, nivel inicial entre 60 y 95 %, ruido de medición σ = 0.5 %,
umbral crítico 20 %; predicción con los promedios de 5 min de la ventana (6 h = 72; 24 h = 288).

| Escenario | Casos | Con predicción | Error absoluto medio (h) | Error relativo medio | Sesgo (h) | Valor real dentro del intervalo | Horas reales (media) |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Consumo constante, ventana de 6 h | 200 | 200 | 0.75 | 1.8 % | -0.04 | 91 % | 29.5 |
| Consumo constante, ventana de 24 h | 200 | 200 | 0.10 | 0.3 % | -0.02 | 70 % | 25.8 |
| Consumo con patrón diario (±50 %), ventana de 6 h | 200 | 200 | 13.31 | 44.3 % | +4.86 | 6 % | 31.5 |
| Consumo con patrón diario (±50 %), ventana de 24 h | 200 | 200 | 4.17 | 18.8 % | +0.23 | 8 % | 29.5 |

## Detección de anomalías (modelo multivariable)

Método: `isolation_forest`. Entrenamiento: 2 016 promedios de 5 min (7 días) de operación normal sintética.
Prueba: 200 promedios normales y 60 anómalos (15 de cada tipo).

| Métrica | Valor |
| :--- | ---: |
| Verdaderos positivos | 50 |
| Falsos positivos | 11 |
| Falsos negativos | 10 |
| Verdaderos negativos | 189 |
| Precisión | 0.82 |
| Exhaustividad (recall) | 0.83 |

| Tipo de anomalía | Detectadas |
| :--- | ---: |
| descenso_rapido_sin_calor | 8 de 15 |
| uv_de_noche | 14 de 15 |
| calor_nocturno | 15 de 15 |
| vpd_incoherente | 13 de 15 |

## Lectura de los resultados

- Con consumo constante, la regresión estima bien el tiempo restante; la ventana de 24 h reduce el error.
- Con un consumo que cambia a lo largo del día, la ventana de 6 h ve solo una parte del ciclo y el error crece;
  la ventana de 24 h promedia el ciclo y reduce el error, por eso el gateway usa 24 h cuando las tiene.
- El intervalo de confianza supone residuos independientes; con un patrón diario los residuos están
  correlacionados y el intervalo resulta demasiado estrecho: se presenta como orientativo, no como garantía.
- El umbral de anomalías (contaminación 0.05) se eligió comparando 0.01, 0.02, 0.03, 0.05 y el valor automático
  sobre este mismo conjunto sintético; debe recalibrarse con datos medidos del prototipo.
- El descenso rápido sin calor es la anomalía menos detectada por el modelo; la fusión del firmware ya alerta
  por descenso sostenido, así que el modelo es una capa adicional, no la única.
