# FIS-13: operación de 24 h

Archivos de la prueba de 24 h con el ecosistema completo (tests/protocolo-fisico.md):

- `serie.log`: registro serie del ESP32 durante las 24 h (memoria libre de `tRed` cada minuto).
- `telemetria.csv`, `alertas.csv`, `ordenes.csv`, `agregados.csv`: exportados de `gateway.db` con `sqlite3 -header -csv`.
- `resumen.md`: inicio y fin de la prueba, reinicios observados, memoria libre mínima y ET0 calculada.
