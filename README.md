# Monitoreo IoT de Recursos Hídricos en Sabana Centro

[![Arduino](https://img.shields.io/badge/Arduino-Uno%20Rev3-blue.svg)](https://www.arduino.cc/)
[![Simulator](https://img.shields.io/badge/Simulator-Wokwi-green.svg)](https://wokwi.com/)
[![License](https://img.shields.io/badge/License-Academic-orange.svg)]()

Sistema embebido de bajo costo para el monitoreo ambiental e hídrico local en la región de Sabana Centro, desarrollado sobre la plataforma Arduino Uno (ATmega328P) con operación 100% *offline*. Proyecto desarrollado para el **Challenge #1** del curso de IoT en la Universidad de La Sabana.

## Descripción General

El sistema realiza la adquisición multivariable en tiempo real de cinco parámetros ambientales: **nivel de agua** (ultrasónico HC-SR04), **temperatura y humedad relativa** (DHT22), **presión atmosférica** (BMP180) e **iluminación/radiación solar** (LDR analógico en A0). 

A través de una lógica de fusión embebida, la información procesada se despliega en una interfaz visual **LCD 20x4 I2C** y emite alarmas sonoras diferenciadas (*Buzzer* en D8) ante escenarios de escasez hídrica o estrés térmico, garantizando la autonomía del sistema sin depender de conectividad a Internet.

## Estructura del Repositorio de Código

```text
├── src/
│   └── main.ino          # Código fuente en C++ (Arduino)
├── wokwi/
│   └── diagram.json      # Configuración del circuito en Wokwi
└── README.md             # Presentación del repositorio
```


## Requisitos de Hardware y Librerías
### Componentes Empleados
* Microcontrolador: Arduino Uno Rev3 (ATmega328P)
* Sensor de Nivel: HC-SR04 (Ultrasonido)
* Sensor Climático: DHT22 (Humedad y Temperatura)
* Sensor Barométrico: BMP180 (Presión y Temperatura por I2C)
* Sensor de Radiación/Luz: Fotorresistencia LDR (Entrada analógica A0)
* Visualización Local: Pantalla LCD 20x4 I2C (0x27)
* Alarma In Situ: Buzzer Pasivo (Pin D8)

## Librerías Requeridas en la IDE / Wokwi
* LiquidCrystal_I2C.h (Frank de Brabander)
* Adafruit_BMP085.h (Adafruit)
* DHT.h / DHT_sensor_library (Adafruit)

## Documentación Técnica Completa (Wiki)
Toda la documentación detallada del proyecto (contextualización, requerimientos, arquitectura, esquemáticos, matrices de pruebas, análisis de resultados, modelo de negocio y declaración de IA) se encuentra organizada en la Wiki del Repositorio.

## Integrantes del Equipo
* Simón Martínez García — Líder de Hardware y Embebidos
* Pablo Andrés Tamayo González — Líder de Software y Lógica de Fusión
* María José Almanza Caviedes — Líder de Documentación, Wiki y Modelo de Negocio
