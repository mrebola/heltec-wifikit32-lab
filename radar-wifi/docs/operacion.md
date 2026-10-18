# Guía de operación — Hackwise Scanner

## Los botones

La placa tiene dos botones junto al conector USB:

| Botón | Qué hace |
|---|---|
| **PRG** | Todo el control del radar |
| **RST** | Reinicia la placa (vuelve al logo y a la lista; la calibración se conserva) |

Con **PRG** hay tres gestos:

| Gesto | Cómo | Para qué |
|---|---|---|
| **Corto** | pulsar y soltar (menos de 0.6 s) | siguiente opción |
| **Largo** | mantener ~1 s y soltar | aceptar / entrar |
| **Muy largo** | mantener 2 s | volver (actúa al llegar a 2 s, sin esperar a soltar) |

Mientras mantienes PRG, una **barra de progreso** crece sobre la cinta de
ayuda:

```
  ███████|███████████░░░░░░░░░░░░░░░
         ↑                         ↑
   aquí empieza "Largo"     llena = "Muy largo"
```

Si sueltas antes de la marca es un **corto**. Si sueltas entre la marca y el
final es un **largo**. Si la barra se llena, es un **muy largo**.

La **última fila** de la pantalla muestra siempre, deslizándose, los controles
de la pantalla en la que estás. Si dudas, léela.

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
│                            │
│Corto: bajar · Largo: rastre│  ← cinta de ayuda
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
│Largo: menú · Muy largo (2 s│
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
│▓Calibrar 1 m (-40)        ▓│
│ Entorno: Interior          │
│ Volver                     │
│Corto: siguiente · Largo: el│
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
| **Volver** | Regresa al rastreo. |

Ambos ajustes se guardan y se conservan al apagar la placa.

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
| La pantalla no responde | Pulsa **RST**. |
