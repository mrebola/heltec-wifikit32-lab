# Guía de operación — Hackwise Scanner

## Los botones

La placa tiene dos botones junto al conector USB:

| Botón | Qué hace |
|---|---|
| **PRG** | Todo el control del radar |
| **RST** | Reinicia la placa (vuelve al logo y a la lista; la calibración se conserva) |

Con **PRG** todo funciona igual: **mantienes, miras el panel y sueltas**.

- Un **toque rápido** (menos de 0.6 s) hace la acción principal de la pantalla:
  bajar en la lista, o pasar a la siguiente opción en el menú.
- Si **mantienes**, aparece el panel **"Si sueltas ahora"**. Las acciones de la
  pantalla van **rotando** en él, una cada 1.2 s, y vuelven a empezar. Al
  soltar se ejecuta la que se ve en ese momento.
- Mantener nunca dispara nada por sí solo. Si te pasaste, sigue apretando:
  la acción que querías vuelve a salir.

```
┌────────────────────────────┐
│Si sueltas ahora:           │
│Rastrear red                │  ← lo que pasa si sueltas YA
│+0.8s: Re-escanear          │  ← la siguiente, y en cuánto tiempo
└────────────────────────────┘
███████████░░░░░░░░░░░░░░░░░░░  ← se llena hasta el cambio de acción
```

El orden es siempre el mismo:

| Mantienes | Si sueltas | Nombre en esta guía |
|---|---|---|
| menos de 0.6 s | acción principal | **corto** |
| 0.6 – 1.8 s | aceptar / entrar | **largo** |
| 1.8 – 3.0 s | volver / acción secundaria | **muy largo** |
| 3.0 s en adelante | se repite: corto → largo → muy largo… | |

Las acciones dependen de la pantalla y, en el menú, de la opción seleccionada
(por ejemplo "Calibrar a 1 m" o "Cambiar entorno"). No hace falta memorizar
la tabla: el panel siempre dice qué va a pasar.

---

## Pantallas

### 1. Arranque
Al encender aparece el logo de **hackwise** con "Radar WiFi" durante 2 s.
Después pasa a la lista.

### 2. Lista de redes
```
┌────────────────────────────┐
│Hackwise Scanner (15)       │  ← título y número de redes
│────────────────────────────│  ← un hueco recorre la línea mientras escanea
│▓Craftyverse   ▂▄▆█  -49  6▓│  ← seleccionada (invertida)
│ INFINITUM2A1  ▂▄▆   -63 11 │
│ (oculta)      ▂▄    -71  1 │
│ Totalplay-5   ▂     -84  6 │
│ IZZI-4F2C     ▂     -88  1 │
└────────────────────────────┘
   red           señal  dBm canal
```

- Las redes se ordenan de la más fuerte a la más débil y se actualizan cada
  ~3 s.
- `(oculta)` es una red que no anuncia su nombre.

| Gesto | Acción |
|---|---|
| Corto | bajar a la siguiente red (al final vuelve arriba) |
| Largo | **rastrear** la red seleccionada |
| Muy largo | volver a escanear ahora |

### 3. Rastreo
```
┌────────────────────────────┐
│Craftyverse              c6 │  ← red y canal
│────────────────────────────│
│~3.4 m              -56dBm  │  ← distancia estimada y potencia
│rango 2.7-4.4 m             │  ← margen probable
│▲ ACERCANDO           4 disp│  ← tendencia y equipos activos
│██████████████████░░░░░░░░░│  ← frío ◄──► caliente
└────────────────────────────┘
```

**Cómo leerla:**
- **Distancia** (`~3.4 m`): estimación a partir de la potencia. Tómala como
  orientación: en interiores puede variar ±30–50 %.
- **Rango**: entre qué valores está probablemente la distancia real.
- **Tendencia**: lo más fiable de la pantalla.
  - **▲ ACERCANDO**: la señal sube; vas en buena dirección.
  - **▼ ALEJANDO**: la señal baja; date la vuelta.
  - **▬ ESTABLE**: no hay cambio claro. Muévete 2 o 3 pasos y espera ~2 s.
- **disp**: equipos distintos que hablaron con ese router en el último minuto.
  Empieza en 0 y sube mientras escuchas.
- **Barra**: un "frío / caliente" rápido según la potencia.
- Mensajes posibles:
  - **"Buscando señal..."**: acabas de fijar la red y aún no llega la primera
    lectura.
  - **"Señal perdida"**: no se oye al router desde hace 5 s. La placa lo busca
    en todos los canales y vuelve sola si lo encuentra.

| Gesto | Acción |
|---|---|
| Largo | abrir el **menú** |
| Muy largo | volver a la lista |

### 4. Menú (desde Rastreo)
```
┌────────────────────────────┐
│Menú                        │
│────────────────────────────│
│ Entorno: Interior          │
│▓Deauth (lab)              ▓│
│ Volver                     │
└────────────────────────────┘
```

| Gesto | Acción |
|---|---|
| Corto | siguiente opción |
| Largo | elegir la opción |
| Muy largo | volver al rastreo |

| Opción | Qué hace |
|---|---|
| **Calibrar 1 m (−40)** | Guarda la potencia actual como "1 metro". El número entre paréntesis es el valor guardado. |
| **Entorno: Interior / Exterior** | Cambia el modelo de distancia. Interior supone paredes y reflejos; exterior, campo abierto. |
| **Deauth (lab)** | Abre la pantalla de deautenticación de la red rastreada. Solo para tu propio laboratorio. |
| **Volver** | Regresa al rastreo. |

Ambos ajustes (calibración y entorno) se guardan y se conservan al apagar la
placa.

### 5. Deauth (lab)
```
┌────────────────────────────┐
│Akbаl_lab               c6  │  ← la red rastreada y su canal
│────────────────────────────│
│ENVIANDO DEAUTH             │  ← parpadea mientras envía
│1024 tramas  8s             │  ← tramas enviadas y cuánto lleva
│disp: 3                     │  ← equipos que aún hablan con el router
└────────────────────────────┘
```

- **Qué hace**: envía tramas de deautenticación en nombre del router
  (suplanta su MAC): al broadcast y una dirigida a cada equipo visto en el
  último minuto. Ráfagas cada 100 ms, **sin límite de tiempo: sigue hasta que
  tú la pares**.
- **"tramas"** crece rápido (~40/s): es el indicador de que está activo.
- **"disp"** baja hacia 0 cuando funciona: los equipos dejan de hablar con el
  router. Normalmente se reconectan en segundos al parar el ataque.
- La escucha sigue corriendo por detrás: la potencia y el canal siguen vivos.
- Los routers con PMF (802.11w) y sus clientes ignoran estas tramas.

| Gesto | Acción |
|---|---|
| Largo | **iniciar** / **parar** |
| Muy largo | parar y volver al rastreo |
| Corto | nada |

> **Solo en redes propias o con autorización expresa** (tu laboratorio, como
> `akbal_lab`). Desasociar equipos de redes ajenas es ilegal en la mayoría de
> jurisdicciones.

---

## Recetas

### Encontrar un router
1. En la lista, pulsa **corto** hasta seleccionar la red.
2. Pulsa **largo** para rastrearla.
3. Camina despacio en una dirección y espera ~2 s.
   - **ACERCANDO**: sigue por ahí.
   - **ALEJANDO**: date la vuelta.
   - **ESTABLE**: prueba en un ángulo de 90°.
4. Cerca del router, la distancia baja a ~1 m y la barra se llena.

### Calibrar (mejora mucho la distancia)
Hazlo una vez por router o tipo de router:
1. Rastrea la red y colócate a **1 metro** del router, con línea de vista.
2. Espera ~2 s a que la lectura se estabilice.
3. **Largo** → menú → **Calibrar 1 m** → **largo**.
4. Debe aparecer **"Calibrado a 1 m"**. Si aparece "Espera la señal", espera un
   poco más y repite.

### Desasociar los equipos de tu laboratorio (deauth)
1. Rastrea la red del lab y deja el rastreo activo unos segundos (así ya vio a
   los equipos).
2. **Largo** → menú → **corto** hasta *Deauth (lab)* → **largo** para entrar.
3. **Largo** para iniciar. La línea **ENVIANDO DEAUTH** parpadea y "tramas"
   corre. Sigue hasta que tú la pares — el router sigue emitiendo beacons,
   así que "Señal perdida" no se activa.
4. **Largo** de nuevo para parar, o **muy largo** para parar y volver.
5. Verifica en un equipo del lab: su WiFi se cae y se reconecta al parar.

### Cambiar entre interior y exterior
**Largo** → menú → **corto** hasta *Entorno* → **largo** para alternar →
**muy largo** para volver.

### Volver a la lista
Desde el rastreo: **muy largo**. Desde el menú: **muy largo** dos veces.

---

## Problemas frecuentes

| Síntoma | Causa y solución |
|---|---|
| La lista dice "Sin redes" | No hay redes de 2.4 GHz cerca. Esta placa no ve 5 GHz. Haz un **muy largo** para reintentar. |
| Mi red no aparece | Probablemente es de 5 GHz. Activa la banda de 2.4 GHz en el router. |
| La distancia salta mucho | Normal con cuerpos o paredes en medio. Fíjate en la tendencia, quédate quieto 2 s y calibra a 1 m. |
| La distancia siempre está mal por el mismo factor | Calibra a 1 m, o cambia el entorno (interior/exterior). |
| "Señal perdida" todo el tiempo | Estás fuera de alcance, o la antena está tapada. La antena es el zigzag del extremo de la placa: no la cubras con la mano. |
| "disp" se queda en 0 | Nadie usa la red en este momento, o los equipos están dormidos. Espera un minuto. |
| ENVIANDO pero nadie se desconecta | Puede pasar: routers con PMF (802.11w) hacen caso omiso de tramas falsificadas, y algunos clientes se reconectan en menos de un segundo. La trama se envía igual; cuéntalo como resultado del ejercicio. |
| La pantalla no responde | Pulsa **RST**. |
