# Contexto para continuar el proyecto (handoff para otra IA)

Este documento resume el estado del repo para que otro asistente (Kimi, GLM,
otro Claude, etc.) pueda seguir trabajando con el usuario sin repetir lo ya
hecho. El usuario escribe en **español**; responde en español.

## Qué es esto
Laboratorio para una placa **Heltec WiFi Kit 32 V2** (comprada en Amazon como
"MakerFocus WiFi Kit 32", pero es una Heltec). Repo:
`github.com/mrebola/heltec-wifikit32-lab`. Rama única: `main`.

El proyecto principal es **radar-wifi** ("Hackwise Scanner"): un radar WiFi que
lista redes por potencia, rastrea una y estima distancia/tendencia hacia el
router, más un conteo de equipos activos.

## La placa (datos medidos, no de la caja)
| | |
|---|---|
| Chip | ESP32-D0WDQ6 rev v1.0, dual core 240 MHz, Xtensa LX6 |
| Flash | 8 MB (Winbond `ef/4017`) |
| RAM | ~520 KB SRAM, **sin PSRAM** |
| USB-serie | CP2102, conector **micro-USB**, puerto `/dev/cu.usbserial-0001` (macOS) |
| WiFi | 802.11 b/g/n, **solo 2.4 GHz**, antena en PCB |
| Bluetooth | BLE 4.2 + Classic (el radar no lo usa) |
| OLED | SSD1306 128×64 I2C — SDA 4, SCL 15, RST 16 |
| Botón | PRG = GPIO0 (activo LOW). RST solo reinicia, no se puede leer |
| LED | GPIO25 |
| **No tiene LoRa** | No sirve para Meshtastic sin módulo externo |

## Entorno de desarrollo
- **PlatformIO** (instalado con `brew install platformio`, binario en `pio`).
- No hay `arduino-cli` ni `esptool` global; esptool se usó en un venv temporal.
- Core arduino-esp32 **2.0.17**, que es **C++11** en la placa (no uses
  `std::clamp` ni C++14+ en `lib/radar_core`; el env `native` ya fuerza C++11).

Comandos clave (desde `radar-wifi/`):
```sh
pio test -e native            # 65 pruebas de lógica en la Mac
pio run -e heltec             # compilar firmware
pio run -e heltec -t upload   # grabar la placa
pio device monitor            # log serie (115200)
```

## Arquitectura (regla de oro)
**Toda la lógica vive en `radar-wifi/lib/radar_core/` como C++ puro sin Arduino,
y está cubierta por pruebas que corren en la Mac.** `src/` solo traduce entre esa
lógica y el hardware (radio, botón, pantalla). Mantén esa separación: si agregas
lógica, ponla en un header de `radar_core` con su prueba en
`test/test_core/test_main.cpp` (TDD: prueba primero, en rojo, luego el código).

```
radar-wifi/
├── lib/radar_core/
│   ├── button.h      Gestos del botón. Se disparan AL SOLTAR. Mantener rota
│   │                 acciones: Short (<0.6s) → Long → VeryLong, 1.2s cada una,
│   │                 y se repite. gestureAt()/msToNext()/pending() son puros.
│   ├── nav.h         Pantallas (List/Track/Menu), menú, y gestureLabel() con
│   │                 el texto de "si sueltas ahora".
│   ├── net_list.h    Lista de redes, sin memoria dinámica, merge por BSSID.
│   ├── rssi_filter.h Mediana de 5 + Kalman 1D sobre RSSI. samples()/sigma().
│   ├── distance.h    Modelo log-distancia d=10^((A-RSSI)/(10n)) + rango.
│   ├── trend.h       Regresión lineal 3s + histéresis → Approaching/Receding/Stable.
│   └── frames.h      parseFrame() lee cabeceras 802.11; ClientTable cuenta MACs.
├── src/
│   ├── main.cpp      loop: botón → nav → radio → pantalla. La radio hace UNA
│   │                 cosa: barrido (lista/buscar AP perdido) o escucha de un canal.
│   ├── scanner.cpp   Barridos WiFi async. OJO: el fin se detecta con el evento
│   │                 ARDUINO_EVENT_WIFI_SCAN_DONE, NO con WiFi.scanComplete()
│   │                 (su timeout falso = max_ms_per_chan*20 desfasa los escaneos).
│   │                 Tiene watchdog que reinicia WiFi si no llega SCAN_DONE.
│   ├── sniffer.cpp   Modo promiscuo: promedia RSSI del AP y cuenta equipos.
│   │                 Callback corre en la tarea WiFi; datos protegidos con portMUX.
│   ├── ui.cpp        Dibujo U8g2: lista, rastreo, menú, panel "si sueltas ahora",
│   │                 barra de progreso, logo de arranque. 128×64, 1 bit.
│   └── logo.h        Logo Hackwise como XBM 124×26 (generado de logo-dark.svg).
└── docs/
    ├── design.md     Especificación técnica.
    └── operacion.md  Guía de uso con el botón (lo que ve el usuario final).
```

## Cómo funciona el radar (resumen)
1. **Lista**: barrido de todos los canales 2.4 GHz cada ~3 s, ordenado por RSSI.
2. **Rastreo**: al fijar una red, escucha pasiva (promiscuo) del canal de esa
   red. Cada beacon del AP = una lectura; se promedian cada 125 ms (~8/s).
   Sin señal 5 s → "Señal perdida" + barrido completo para reencontrar el AP.
3. **Distancia**: RSSI → mediana → Kalman → modelo log-distancia. Calibrable a
   1 m (guardado en NVS con Preferences). Precisión absoluta ±30-50% en interior;
   **la tendencia (acercando/alejando) es lo fiable**.
4. **Equipos activos**: cuenta MACs unicast distintas que hablan con el AP en los
   últimos 60 s, leyendo solo cabeceras. Es defensivo/pasivo.

## Interacción con el botón (estado actual, ya grabado)
Un solo botón (PRG). Toque rápido = acción principal. Si mantienes, un panel
muestra "Si sueltas ahora: X" y las acciones **rotan** cada 1.2 s; sueltas en la
que quieras. Mantener nunca dispara nada solo. No hay cinta de ayuda deslizante
(se quitó). La lista muestra 5 filas.

## Estado
- Todo en `main`, limpio, pusheado. Firmware grabado en la placa (~740 KB de
  3 MB por partición de app; tabla `partitions_8mb.csv` usa los 8 MB).
- 65 pruebas en verde, compila sin warnings.
- **Verificado en placa**: arranque, escaneo, estabilidad del escáner (bug de
  desfase corregido y confirmado), lecturas de rastreo por serie.
- **Falta verificar con ojos humanos en la OLED**: logo, panel "si sueltas
  ahora", conteo de equipos, y la prueba física de caminar hacia el router.

## Ideas pendientes / próximos pasos sugeridos
- Indicador de batería en la OLED (la V2 lee el nivel por un pin analógico;
  verificar cuál con batería conectada). Ver sección batería/case abajo.
- Modo ahorro: apagar la OLED tras inactividad del botón.
- **Seguridad WiFi defensiva** (lo que interesa al usuario): detector de ataques
  deauth contra la propia red, detección de "evil twin" (AP clon con mismo SSID),
  mapa de ocupación de canales.

## Hardware opcional
- Batería: Li-Po 1S 3.7 V con conector **JST 1.25mm / SH1.25** (verificar
  polaridad contra la serigrafía antes de conectar). 1000 mAh ≈ 6-7 h estimadas.
  Se carga por USB. No hay interruptor físico.
- Case 3D: usar modelos de "Heltec WiFi LoRa 32 **V2**" (mismas medidas
  50.2×25.5 mm); sobra el hueco de antena LoRa. El del usuario aún sin imprimir.
