# heltec-wifikit32-lab

Laboratorio de experimentos con la placa **Heltec WiFi Kit 32** (ESP32 + OLED 0.96").

<p>
  <img src="fotos/WhatsApp Image 2026-10-18 at 4.01.17 PM.jpeg" width="220" alt="Caja Heltec WiFi Kit 32">
  <img src="fotos/WhatsApp Image 2026-10-18 at 4.01.17 PM (1).jpeg" width="220" alt="Placa, vista frontal">
  <img src="fotos/WhatsApp Image 2026-10-18 at 4.01.17 PM (2).jpeg" width="220" alt="Placa, vista trasera">
</p>

## La placa

Datos leídos directamente del chip con `esptool` (no son de la caja):

| | |
|---|---|
| Placa | Heltec WiFi Kit 32 (serigrafía `HTIT-WB32`, probablemente V2) |
| Chip | ESP32-D0WDQ6 rev v1.0 — dual core, 240 MHz |
| Cristal | 26 MHz |
| Flash | 8 MB (Winbond, ID `ef/4017`) |
| RAM | ~520 KB SRAM, sin PSRAM |
| USB-serie | Silicon Labs CP2102 (conector micro-USB) |
| WiFi | 802.11 b/g/n, **solo 2.4 GHz**, antena impresa en la PCB |
| Bluetooth | BLE 4.2 + Bluetooth Clásico |
| Pantalla | OLED SSD1306 128×64 |
| Batería | Conector y cargador Li-Po |

## Pines útiles

| Función | GPIO |
|---|---|
| OLED SDA | 4 |
| OLED SCL | 15 |
| OLED RST | 16 |
| LED integrado | 25 |
| Botón PRG | 0 (activo en LOW, usar `INPUT_PULLUP`) |

## Botones

- **RST** — reinicia el ESP32.
- **PRG** — conectado a GPIO0.
  - Modo bootloader manual: mantener PRG → pulsar y soltar RST → soltar PRG.
    Solo hace falta si la subida falla con *"Failed to connect"*.
  - Una vez arrancado el programa, se puede leer como botón de usuario.

## Particiones

La flash es de 8 MB, pero la tabla de fábrica solo usa los primeros 4 MB:

| Partición | Offset | Tamaño |
|---|---|---|
| nvs | 0x009000 | 20 KB |
| otadata | 0x00E000 | 8 KB |
| app0 | 0x010000 | 1.25 MB |
| app1 | 0x150000 | 1.25 MB |
| spiffs | 0x290000 | 1.44 MB |
| *(sin usar)* | 0x400000 | 4 MB |

Para usar los 8 MB en PlatformIO:

```ini
board_build.partitions = default_8MB.csv
```

## Conexión en macOS

- Puerto: `/dev/cu.usbserial-0001`
- Monitor serie: 115200 baudios
- El firmware de fábrica imprime `Serial initial done` / `OLED initial done!` al arrancar.

```sh
pip install esptool
esptool.py --port /dev/cu.usbserial-0001 flash_id
```

## Proyectos

_Próximamente._ Cada experimento vivirá en su propia carpeta.
