# Datos del banco de pruebas físico (Challenge #3)

Plantillas CSV de las pruebas de [tests/protocolo-fisico.md](../../protocolo-fisico.md). Cada archivo trae solo la fila de
encabezados; las filas se agregan al ejecutar cada prueba con el prototipo físico.

Columnas comunes: `fecha_hora` (hora local, `AAAA-MM-DD HH:MM:SS`), `version` (commit del firmware y del gateway) y
`observaciones`. Las pruebas con valor numérico usan `grupo`, `esperado` y `observado`, que lee
[resumen_csv.py](../../herramientas/resumen_csv.py) para la tabla de la Wiki. En `fis14-buzzer.csv` el grupo es el estado y
el valor la frecuencia en Hz; en `fis15-consumo.csv`, el estado y la corriente en mA (con `esperado` vacío si no hay valor
de referencia).
