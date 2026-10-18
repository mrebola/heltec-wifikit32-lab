# Radar WiFi — diseño

## Objetivo
Encontrar físicamente un punto de acceso WiFi con la Heltec WiFi Kit 32:
lista de redes ordenada por potencia → fijar una → caminar y ver distancia
estimada y tendencia (acercando / alejando / estable).

## Restricciones
- **Un solo botón programable** (PRG, GPIO0). RST reinicia el chip.
- **Sin FTM** (802.11mc) en el ESP32 original: la distancia sale solo de la RSSI.
  Error típico en interior ±30–50 %. La tendencia es la señal fiable.
- Mínimo espacio: sin BLE, sin sistema de archivos, sin servidor web.

## Botón
| Gesto | Duración | Acción |
|---|---|---|
| Corta | < 0.6 s | siguiente |
| Larga | 0.6–2 s | aceptar |
| Muy larga | ≥ 2 s | volver (se dispara al llegar a 2 s, sin esperar a soltar) |

Antirrebote de 25 ms. Mientras se mantiene, una barra muestra el progreso.

## Pantallas
- **Arranque**: logo de Hackwise durante 2 s.
- **Lista**: escaneo de todos los canales; red, barras, dBm, canal. Ordenada por
  RSSI, 4 filas. Corta = bajar, larga = rastrear, muy larga = re-escanear.
- **Rastreo**: escucha pasiva (modo promiscuo) del canal de la red fijada. Cada
  beacon del AP es una lectura de RSSI (se promedian cada 125 ms, ~8/s).
  Distancia con rango, tendencia, barra frío/caliente y equipos activos.
  Sin lecturas en 5 s → "Señal perdida" y barrido completo para encontrar el AP
  en otro canal. Larga = menú, muy larga = lista.
- **Cinta de ayuda**: la fila de abajo muestra siempre, deslizándose, los
  controles de la pantalla actual.
- **Menú**: Calibrar a 1 m · Entorno (Interior/Exterior) · Volver.
  Se guarda en NVS (`Preferences`).

## Algoritmo
1. Mediana de las últimas 5 lecturas (descarta picos).
2. Kalman 1D sobre RSSI (q = 0.8, r = 6 dB²). σ = max(3 dB, √p).
3. Log-distancia: `d = 10^((A − RSSI) / (10·n))`, A = RSSI a 1 m (−40 por
   defecto, calibrable), n = 3.0 interior / 2.2 exterior.
   Rango: misma fórmula con RSSI ± σ.
4. Tendencia: regresión lineal de la RSSI filtrada en los últimos 3 s.
   Entra en ACERCANDO a > +0.5 dB/s y sale a < +0.2 dB/s (simétrico para
   ALEJANDO). Exige ≥ 4 muestras y ≥ 1 s de historia.

## Equipos activos
Sin unirse a la red no se puede saber cuántos equipos están asociados. Se cuentan
las MAC unicast distintas que intercambian tramas de datos o de gestión con el
BSSID en los últimos 60 s. Solo se leen cabeceras 802.11, nunca el contenido.
Un equipo en reposo que no transmite no aparece hasta que lo haga.

## Radio
El ESP32 hace una cosa a la vez: barrido (lista o búsqueda del AP perdido) o
escucha del canal. El fin de un barrido se detecta con el evento SCAN_DONE,
no con `WiFi.scanComplete()`: el core da un timeout falso a los
`max_ms_per_chan × 20` y desfasa todos los escaneos siguientes.

## Código
- `lib/radar_core/` — C++ puro sin Arduino, probado en la Mac (`pio test -e native`).
- `src/` — glue con Arduino: escáner WiFi, dibujo U8g2, loop principal.
