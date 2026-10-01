/**
 * @file hmi.h
 * @brief Interfaz local: LCD 20x4 y buzzer (Wiki 2.5.3).
 *
 * La tarea tHMI llama a hmiActualizar() cada HMI_PERIODO_MS con una copia del
 * estado publicado. hmiActualizar() refresca la LCD cada LCD_REFRESCO_MS,
 * hace parpadear la retroiluminación en CRÍTICO y fija el patrón del buzzer;
 * el patrón lo genera un temporizador de software de BUZZER_TICK_MS.
 */
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "datos.h"

/**
 * @brief Configura el buzzer (LEDC y temporizador del patrón) e inicializa la LCD.
 * @param mtxI2C Mutex del bus I2C compartido con el BMP180.
 * @return true si el buzzer quedó configurado (la LCD puede faltar: se reintenta).
 */
bool hmiIniciar(SemaphoreHandle_t mtxI2C);

/**
 * @brief Actualiza la LCD, la retroiluminación y el patrón del buzzer.
 * @param estado  Copia del último estado publicado.
 * @param ahoraMs Tiempo actual en ms desde el arranque.
 */
void hmiActualizar(const EstadoPublicado& estado, uint32_t ahoraMs);

/**
 * @brief Compone las cuatro filas de la LCD a partir del estado.
 *
 * Separada de la escritura por I2C para poder revisar el texto en el registro.
 * Cada fila tiene como máximo LCD_COLUMNAS caracteres más el terminador.
 */
void hmiComponerFilas(const EstadoPublicado& estado, char filas[][21]);
