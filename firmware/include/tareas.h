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
 * Paso 4: tSensores lee los sensores; tFusion calcula el nivel, la tendencia,
 * el VPD, la ET0 y el estado de alerta; tHMI maneja la LCD y el buzzer.
 * tHistorico y tRed todavía solo registran su actividad (pasos 5 y 6).
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
bool leerEstadoPublicado(EstadoPublicado& destino);

/**
 * @brief Informa el día del año (1–366), necesario para la radiación
 *        extraterrestre de la ET0. Mientras no se conozca, la ET0 no está
 *        disponible. La fuente de la fecha se define en el paso 5 (red).
 */
void tareasFijarDiaDelAnio(int diaDelAnio);

/**
 * @brief Pide desactivar la alarma física (orden del tablero, paso 5).
 *
 * Segura entre tareas: tFusion atiende la orden en su próximo ciclo y solo la
 * acepta en ALERTA o CRÍTICO. La alarma se rearma si el estado empeora.
 */
void tareasSolicitarDesactivarAlarma();
