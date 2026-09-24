/**
 * @file sensores.h
 * @brief Adquisición de los sensores: HC-SR04, DHT22, BMP180 y GUVA-S12SD.
 *
 * Solo se llama desde la tarea tSensores. Las lecturas lentas (eco, DHT22,
 * I2C) se hacen aquí, nunca en una rutina de interrupción. El acceso al bus
 * I2C se protege con el mutex que recibe sensoresIniciar().
 */
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "datos.h"

/**
 * @brief Inicializa los pines, el bus I2C y los sensores.
 *
 * También recorre el bus I2C y registra las direcciones encontradas, para
 * confirmar la LCD (0x27) y el BMP180 (0x77) al montar el hardware.
 *
 * @param mtxI2C Mutex que protege el bus I2C compartido con la LCD.
 */
void sensoresIniciar(SemaphoreHandle_t mtxI2C);

/**
 * @brief Lee los sensores que corresponden al ciclo y actualiza sus estados.
 * @param leerDht22 true si en este ciclo corresponde leer el DHT22.
 * @param salida    Lecturas y estados resultantes.
 */
void sensoresLeer(bool leerDht22, Lecturas& salida);
