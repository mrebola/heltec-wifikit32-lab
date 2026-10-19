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
| Larga | 0.6–1.8 s | aceptar |
| Muy larga | 1.8–3.0 s | volver |
| (ciclo) | ≥ 3.0 s | se repite corta → larga → muy larga, 1.2 s cada una |

Todo gesto se dispara **al soltar**; mantener nunca actúa solo.

Antirrebote de 25 ms. Mientras se mantiene, una barra muestra el progreso y,
desde 0.15 s, un panel dice qué hace soltar en ese instante (`Button::pending`
+ `gestureLabel`) y cuánto falta para el siguiente gesto.

## Pantallas
- **Arranque**: logo de Hackwise durante 2 s.
- **Lista**: escaneo de todos los canales; red, barras, dBm, canal. Ordenada por
  RSSI, 5 filas. Corta = bajar, larga = rastrear, muy larga = re-escanear.
- **Rastreo**: escucha pasiva (modo promiscuo) del canal de la red fijada. Cada
  beacon del AP es una lectura de RSSI (se promedian cada 125 ms, ~8/s).
  Distancia con rango, tendencia, barra frío/caliente y equipos activos.
  Sin lecturas en 5 s → "Señal perdida" y barrido completo para encontrar el AP
  en otro canal. Larga = menú, muy larga = lista.
- **Menú**: Calibrar a 1 m · Entorno (Interior/Exterior) · Deauth (lab) · Volver.
  Se guarda en NVS (`Preferences`) lo ajustable (la calibración y el entorno).
- **Deauth (lab)**: desde el menú del rastreo. Transmite tramas de
  deautenticación suplantando al AP — broadcast y una unicast por cada cliente
  visto en 60 s — en ráfagas cada 100 ms, **hasta que el usuario las detiene**.
  Detalle en "Deauth (lab)" abajo.

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

## Deauth (lab)
Ataque activo de deautenticación. **Alcance: redes propias o con autorización
expresa (el laboratorio del usuario).** Desasociar equipos de redes ajenas es
ilegal en la mayoría de jurisdicciones; esta función existe para la práctica de
certificación del usuario sobre su propia infraestructura.

- **Trama**: gestión, subtipo 12 (0xC0), reason 7, 26 bytes — el hardware añade
  el FCS. La construye `buildDeauth()` en `lib/radar_core/deauth.h` (pura, con
  pruebas); el número de secuencia 802.11 avanza por trama (12 bits con
  envoltura) para que las estaciones no la descarten como duplicada.
- **Transmisión**: `esp_wifi_80211_tx(WIFI_IF_AP, frame, len, false)` — API del
  ESP-IDF que el core de Arduino ya enlaza; con `en_sys_seq = false` el core no
  toca la trama más allá del FCS. **OJO**: la interfaz STA rechaza tramas deauth
  con `ESP_ERR_INVALID_ARG` (verificado en placa: el ESP-IDF 4.4 solo acepta ahí
  beacon/probe/action/datos sin-QoS); por eso el TX va por la **interfaz AP**,
  que no hace ese chequeo (mismo camino que usa ESP32 Marauder). El WiFi corre
  en modo `AP_STA` — el bloque AP existe solo para habilitar esa tubería, nunca
  anuncia SSID. Requiere el sniffer activo (reutiliza su canal) y **se pausa
  durante un barrido**.
- **Ritmo**: ráfaga cada 100 ms (`DeauthRun::kBurstMs`): 4 broadcast a
  ff:ff:ff:ff:ff:ff + 1 unicast por cada cliente vivo de la tabla de 60 s,
  tomadas con `snifferCopyClients()` bajo `portMUX`.
- **Fin**: solo lo decide el usuario. Largo = iniciar/parar; muy largo = parar y
  volver al rastreo (el parado congela el tiempo y el conteo para verlos).
- **Durante el ataque** la escucha sigue activa: el AP sigue mandando beacons
  (por eso no salta "Señal perdida"), RSSI y tendencia siguen vivos, y `disp`
  baja hacia 0 cuando el ataque funciona. Los equipos se reconectan solos en
  segundos al cesar el ataque; routers con PMF (802.11w) ignoran tramas
  falsificadas.

## Radio
El ESP32 hace una cosa a la vez: barrido (lista o búsqueda del AP perdido) o
escucha del canal. El fin de un barrido se detecta con el evento SCAN_DONE,
no con `WiFi.scanComplete()`: el core da un timeout falso a los
`max_ms_per_chan × 20` y desfasa todos los escaneos siguientes.
Durante el deauth se transmite y se escucha a la vez en ese canal (el AP
sigue balizando); el barrido lo pausa automáticamente.

## Código
- `lib/radar_core/` — C++ puro sin Arduino, probado en la Mac (`pio test -e native`).
  Incluye `deauth.h`: trama deauth y estado del run.
- `src/` — glue con Arduino: escáner WiFi, TX de deauth (`deauth_tx.cpp`,
  `esp_wifi_80211_tx`), dibujo U8g2, loop principal.
