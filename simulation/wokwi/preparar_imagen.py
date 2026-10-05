#!/usr/bin/env python3
"""
Prepara la imagen de flash completa para la simulación en Wokwi.

Wokwi carga solo el binario indicado en wokwi.toml. Para que el ESP32 simulado
sirva el frontend del tablero (firmware/data/ en LittleFS), ese binario debe
ser la flash completa: cargador de arranque, tabla de particiones,
boot_app0, aplicación y la imagen de LittleFS, cada una en su dirección de la
tabla de particiones por defecto (default.csv del núcleo para ESP32).

Uso, desde la raíz del repositorio:
    python simulation/wokwi/preparar_imagen.py
Resultado: firmware/.pio/build/wokwi/flash-completa.bin
"""

import os
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
FIRMWARE = RAIZ / "firmware"
BUILD = FIRMWARE / ".pio" / "build" / "wokwi"
PIO_HOME = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))

# Direcciones de la tabla de particiones por defecto (default.csv).
SEGMENTOS = [
    ("0x1000", BUILD / "bootloader.bin"),
    ("0x8000", BUILD / "partitions.bin"),
    ("0xe000", PIO_HOME / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"),
    ("0x10000", BUILD / "firmware.bin"),
    ("0x290000", BUILD / "littlefs.bin"),
]


def ejecutar(comando):
    print("$ " + " ".join(str(c) for c in comando))
    subprocess.run([str(c) for c in comando], check=True)


def main():
    pio = PIO_HOME / "penv" / "Scripts" / "pio.exe"
    python = PIO_HOME / "penv" / "Scripts" / "python.exe"
    if not pio.exists():  # Linux o macOS
        pio = PIO_HOME / "penv" / "bin" / "pio"
        python = PIO_HOME / "penv" / "bin" / "python"
    esptool = PIO_HOME / "packages" / "tool-esptoolpy" / "esptool.py"

    ejecutar([pio, "run", "-d", FIRMWARE, "-e", "wokwi"])
    ejecutar([pio, "run", "-d", FIRMWARE, "-e", "wokwi", "-t", "buildfs"])

    faltan = [str(ruta) for _, ruta in SEGMENTOS if not ruta.exists()]
    if faltan:
        sys.exit("Faltan archivos: " + ", ".join(faltan))

    salida = BUILD / "flash-completa.bin"
    comando = [python, esptool, "--chip", "esp32", "merge_bin", "-o", salida, "--flash_size", "4MB"]
    for direccion, ruta in SEGMENTOS:
        comando += [direccion, ruta]
    ejecutar(comando)
    print("Imagen lista: " + str(salida))


if __name__ == "__main__":
    main()
