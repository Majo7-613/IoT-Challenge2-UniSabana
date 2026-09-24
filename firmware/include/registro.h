/**
 * @file registro.h
 * @brief Registro por el puerto serie con marca de tiempo, seguro entre tareas.
 *
 * Cada línea tiene el formato:
 *     [   12345.678 ms] [tSensores ] mensaje
 * Un mutex evita que las líneas de tareas distintas se mezclen.
 * No debe llamarse desde una rutina de interrupción.
 */
#pragma once

/** Crea el mutex del registro. Llamar una vez en setup(), después de Serial.begin(). */
void registroIniciar();

/**
 * @brief Escribe una línea en el puerto serie.
 * @param etiqueta Nombre corto de quien registra (por ejemplo, la tarea).
 * @param formato  Cadena de formato estilo printf.
 */
void registrar(const char* etiqueta, const char* formato, ...)
    __attribute__((format(printf, 2, 3)));
