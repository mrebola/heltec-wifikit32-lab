# Hackwise Scanner — Radar WiFi

Firmware para la **Heltec WiFi Kit 32** que convierte la placa en un radar WiFi
de bolsillo:

- Lista las redes cercanas ordenadas por potencia, la más fuerte arriba.
- Fija una red y estima a qué distancia está el router.
- Indica si te estás **acercando**, **alejando** o si sigues **estable**.
- Cuenta cuántos **equipos activos** hablan con ese router.
- Se maneja con un solo botón (PRG): mientras lo mantienes, la pantalla rota
  las acciones posibles y dice qué pasará si sueltas en ese momento.

> Cómo usarlo paso a paso: **[docs/operacion.md](docs/operacion.md)**.
> Especificación técnica y decisiones: [docs/design.md](docs/design.md).

## Cómo funciona

### 1. Lista de redes
La placa hace un barrido activo de los 13 canales de 2.4 GHz cada ~3 s, unos
2 s por barrido. Junta las entradas repetidas por BSSID (la MAC del router),
las ordena por RSSI y conserva las 24 más fuertes. El cursor sigue a la misma
red aunque cambie de posición entre barridos.

### 2. Rastreo de una red
Al fijar una red, la placa deja de barrer y **escucha en modo promiscuo** el
canal de esa red:

- Cada trama que envía el router (beacons ~10/s, datos) aporta una lectura de
  potencia. Se promedian en ventanas de 125 ms, lo que da ~8 lecturas por
  segundo.
- Si no se oye al router durante 5 s, la pantalla dice **"Señal perdida"** y la
  placa vuelve a barrer todos los canales. Si el router cambió de canal, lo
  encuentra y sigue escuchando.

### 3. De potencia a distancia
```
RSSI crudo ──► mediana de 5 ──► Kalman 1D ──► modelo log-distancia ──► ~4.2 m (3.1–6.0)
                (quita picos)    (suaviza)            │
                                                      └──► regresión 3 s ──► ACERCANDO / ALEJANDO / ESTABLE
```

1. **Mediana de las últimas 5 lecturas:** descarta los picos sueltos, por
   ejemplo un reflejo o alguien que pasa por delante.
2. **Filtro de Kalman 1D sobre la RSSI**, no sobre la distancia. La distancia es
   exponencial respecto a la RSSI y amplificaría el ruido.
3. **Modelo log-distancia:** `d = 10^((A − RSSI) / (10·n))`.
   - `A` es la potencia medida a 1 m: −40 dBm por defecto, ajustable con
     *Calibrar 1 m*.
   - `n` es el exponente del entorno: 3.0 en interior y 2.2 en exterior.
   - El rango mostrado sale de aplicar la misma fórmula a RSSI ± σ, la
     incertidumbre del filtro (mínimo 3 dB).
4. **Tendencia:** pendiente por regresión lineal de los últimos 3 s.
   - Pasa a ACERCANDO por encima de +0.5 dB/s y sale por debajo de +0.2 dB/s.
     ALEJANDO funciona igual en sentido contrario.
   - Esa histéresis evita que la pantalla parpadee entre estados.

> **Precisión:** el ESP32 original no soporta FTM (802.11mc, medir distancia
> por tiempo de vuelo). La distancia sale solo de la potencia, así que en
> interiores el error típico es de ±30–50 %. Paredes, cuerpos y reflejos
> cambian la señal. **La tendencia es la parte fiable:** sigue la flecha.

### 4. Equipos activos
Mientras escucha, la placa lee la **cabecera** de cada trama 802.11 del canal:
nunca el contenido, que además va cifrado. Cuenta las MAC unicast distintas que
intercambian datos o tramas de gestión con ese router en los últimos 60 s.

- Es una estimación: un equipo dormido no aparece hasta que transmite. Por eso
  el número sube mientras más tiempo escuchas.
- Los celulares con MAC aleatoria usan una MAC fija por red, así que cuentan
  una vez.
- Úsalo en redes propias o con autorización.

## Construcción

### Hardware
| | |
|---|---|
| Placa | Heltec WiFi Kit 32 (ESP32-D0WDQ6, 8 MB flash, sin PSRAM) |
| Pantalla | OLED SSD1306 128×64 por I2C — SDA 4, SCL 15, RST 16 |
| Botón | PRG = GPIO0 (RST reinicia el chip, no se puede usar) |
| Conexión | micro-USB, CP2102 → `/dev/cu.usbserial-0001` en macOS |

No hace falta cablear nada.

### Requisitos
```sh
brew install platformio
```

### Compilar, probar y grabar
```sh
cd radar-wifi
pio test -e native            # 65 pruebas de la lógica, corren en la Mac
pio run -e heltec             # compilar
pio run -e heltec -t upload   # grabar la placa
pio device monitor            # ver el log serie (115200)
```

El log serie muestra cada barrido (`scan: 15 redes`) y cada lectura durante el
rastreo (`track: raw=-56 filt=-56.0 d=3.4m trend=1 disp=4`). `trend` vale
0 = estable, 1 = acercando y 2 = alejando.

### Estructura
```
radar-wifi/
├── platformio.ini          # env heltec (placa) + env native (pruebas)
├── partitions_8mb.csv      # 3 MB por app ×2 (OTA), 1.9 MB datos, 64 KB coredump
├── lib/radar_core/         # lógica pura en C++11, sin Arduino, 100 % probada
│   ├── button.h            #   gestos corta / larga / muy larga, antirrebote, vista previa
│   ├── nav.h               #   pantallas, menú y textos de "si sueltas ahora"
│   ├── net_list.h          #   lista de redes (sin memoria dinámica)
│   ├── rssi_filter.h       #   mediana + Kalman
│   ├── distance.h          #   modelo log-distancia + rango
│   ├── trend.h             #   regresión + histéresis
│   └── frames.h            #   lectura de cabeceras 802.11 + tabla de equipos
├── src/                    # capa de hardware
│   ├── main.cpp            #   loop: botón → navegación → radio → pantalla
│   ├── scanner.cpp         #   barridos WiFi asíncronos con watchdog
│   ├── sniffer.cpp         #   modo promiscuo: RSSI del router + equipos
│   ├── ui.cpp              #   pantallas U8g2, panel "si sueltas ahora", logo
│   └── logo.h              #   logo Hackwise (XBM 124×26)
├── test/test_core/         # pruebas Unity de lib/radar_core
└── docs/
    ├── design.md           # especificación técnica
    └── operacion.md        # guía de uso con los botones
```

La regla del proyecto: **toda decisión vive en `lib/radar_core/` y se prueba
en la Mac.** `src/` solo traduce entre esa lógica y el hardware (radio, botón y
pantalla).

### Uso de recursos
| | |
|---|---|
| Firmware | ~740 KB de 3 MB por partición de app (quedan ~2.3 MB) |
| RAM | ~47 KB estáticos de 320 KB |
| Sin | BLE, sistema de archivos, servidor web, memoria dinámica en la lógica |
| Ajustes | NVS (`Preferences`): calibración y entorno |

### Detalle importante del core de Arduino
El fin de un barrido se detecta con el evento `ARDUINO_EVENT_WIFI_SCAN_DONE`,
**no** con `WiFi.scanComplete()`. En arduino-esp32 2.0.x, `scanComplete()` da
por fallido un barrido asíncrono a los `max_ms_per_chan × 20` ms. Un barrido
completo puede tardar más, y tras ese timeout falso todos los barridos
siguientes quedan desfasados y devuelven 0 redes. Se observó en la placa y está
documentado en `src/scanner.cpp`.

### Logo
`src/logo.h` se generó a partir de
`https://hackwise.mx/wp-content/themes/hackwise-theme/assets/images/logo-dark.svg`:
```sh
rsvg-convert -w 2100 -b black logo-dark.svg -o big.png
# recortar, escalar a 124 px de ancho, umbral → XBM de 1 bit (Pillow)
```
