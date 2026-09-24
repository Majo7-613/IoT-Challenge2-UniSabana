/**
 * @file tareas.h
 * @brief Creación de las tareas FreeRTOS, del temporizador de adquisición y de
 *        los recursos de sincronización.
 *
 * Tareas (ver config.h para núcleo, prioridad y periodo):
 *  - tSensores:  despertada por la ISR del temporizador (1 Hz); adquiere y
 *                entrega la instantánea a tFusion por la cola qSnapshot.
 *  - tFusion:    recibe la instantánea, calcula el estado y lo publica.
 *  - tHMI:       cada 250 ms refresca la LCD y el patrón del buzzer.
 *  - tHistorico: cada 5 s y cada 5 min guarda registros en los búferes.
 *  - tRed:       cada 5 s supervisa el Wi-Fi.
 *
 * Paso 2: tSensores lee los sensores y registra sus lecturas y estados; las
 * demás tareas solo registran su actividad por el puerto serie.
 */
#pragma once

#include "datos.h"

/**
 * @brief Crea la cola, los mutex, las tareas y arranca el temporizador.
 * @return true si todos los recursos se crearon correctamente.
 */
bool tareasIniciar();

/**
 * @brief Copia el último estado publicado por tFusion.
 * @param destino Estructura donde se copia el estado.
 * @return true si ya existe un estado publicado.
 */
bool leerEstadoPublicado(Snapshot& destino);
