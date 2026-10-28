# Hakaru CES: hoja de ruta

Del emulador y calculadora de circuitos de CC y CA para la TI-Nspire CX II.

> **Estado (2026-10-28): Hakaru CES 4.0.** Hechas las etapas 0 a 4: la 4
> (semiconductores, AO y barrido de CC) quedó cerrada el 2026-09-27 con un
> oráculo no lineal y tres revisiones adversariales. Después, sin ser de
> ninguna etapa: el nombre, 13 mejoras, el arreglo del arranque tras los
> flancos, la capa de entrada (teclas y touchpad), la **optimización** (fase
> 1), la función **Capacitores equivalentes** y la preparación del código
> para publicarlo (§2, «Después de la Etapa 4»). **Siguiente: la Etapa 5**
> (transformadores, trifásica y emulación en vivo), **sin empezar**. Lo que
> falta probar en la calculadora real está en el §9.

> **Rumbo (2026-09-24).** El programa es un **emulador de circuitos con
> calculadora, al estilo de NI Multisim**, para la TI-Nspire CX II CAS. Lo
> importante es emular y dar resultados; la teoría se consulta en el libro.
> Nada se filtra por temario: entra todo lo que haga falta para circuitos de
> CC y CA. La primera hoja de ruta (orientada a un examen, con reglas de
> pizarra para todo) quedó archivada.

> **Nombre (2026-09-26).** Al cerrar la Etapa 4 el programa, que se llamaba
> ElectroFock, pasó a llamarse **Hakaru CES**: *hakaru* (測る) es «medir» en
> japonés y CES, *Circuit Emulator & Solver*. Lo de abajo con fecha anterior
> conserva el nombre de entonces. Ver [DESIGN.md](DESIGN.md#nombre-y-formato).

## 1. Qué es Hakaru CES

Dos modos sobre el mismo dibujo del Taller:

| Modo | Qué hace | Cuándo se usa |
|---|---|---|
| **Emulador** | Como Multisim: todos los valores son conocidos y la app simula lo que pasa: punto de operación, evolución en el tiempo, régimen de CA, barridos. Resultados en tablas e instrumentos virtuales (osciloscopio, multímetro, trazador de Bode, vatímetro). | Cuando tienes el circuito completo y quieres ver qué hace. |
| **Calculadora** | Lo que ya existe: marcas datos (lecturas de V o I) e incógnitas (R, E, J, r...) y la app despeja. Es lo que Multisim no hace. | Problemas de libro: "¿qué R hace que pasen 2 mA?". |

`=` sirve para los dos: si hay algo marcado con `?!`, calcula; si no, emula.

**Qué pasa con el paso a paso.** Se queda como está para la CC, pero los
componentes nuevos ya no llevan reglas de pizarra. A cambio, cada componente
tiene una página **Modelo** con la ecuación exacta que usa el emulador (por
ejemplo, `i = C·dv/dt` o `I = Is·(e^(V/nVt) − 1)`). Así puedes comparar el
resultado con lo que dice tu libro.

**Qué pasa con Aprende.** Se convierte poco a poco en una **biblioteca de
circuitos modelo**, como los ejemplos de Multisim: RC, RLC, filtros,
rectificadores, amplificadores... Cada uno se abre en el Taller listo para
emular y trae una nota corta de qué observar ("mide τ con los cursores: debe
dar RC = 1 ms").

## 2. Lo que ya está (Etapas 0 a 4)

**Etapa 4 terminada (2026-09-27):** semiconductores y amplificadores.
Diodo (ideal, con Vγ, exponencial, 1N4148, 1N4007), zener, LED que se
enciende con su corriente media, fuentes controladas (E y J por la V o la I
de otro elemento), el amplificador operacional (ideal y real, con la regla
de la realimentación: comparador, Schmitt, astable), el BJT y el MOSFET,
los primeros componentes de 3 patas; el punto de operación de CC por Newton
(`[=]` y `[P]`), el transitorio no lineal, la pequeña señal en el punto Q
(CA y Bode), el barrido de CC con su pantalla de curvas (nivel, dos
cursores, X-Y, escala log, la ida y vuelta de un Schmitt) y el trazador de
un componente (la curva I-V de un diodo y la familia de un transistor);
lecciones 12 («Diodos») y 13 («AO y transistores») con 20 modelos.
Verificado con el arnés (350 056 comprobaciones al acabar de construirla;
hoy, con todo lo de después, 352 164, 0 fallos), casos cerrados, el
residuo de Kirchhoff de cada solución de Newton, un **oráculo no lineal
independiente** (260 de 260 redes al azar iguales: 200 de CC y 60 en el
tiempo) y **tres revisiones adversariales** (9, 26 y 18 fallos reales, todos
arreglados con su prueba y su mutación; quedan dos casos de rótulos que
tapan un símbolo o no caben en su miniatura). El oráculo destapó además un
fallo del arranque tras los flancos en el motor de la Etapa 2, arreglado con
un tramo rígido (SDIRK2). Detalles, desviaciones y límites: DESIGN.md §19.
El banco de la calculadora real (2026-09-27, sin USB) midió una `exp()` (17
operaciones) y una iteración de Newton (214,3 µs fijos), y con él se calibró
el modelo de tiempos (DESIGN.md §23.4). Pendiente en la calculadora real: los
tiempos del puente y del astable, los colores del LED y la legibilidad de
los símbolos de 3 patas (§9).

**Después de la Etapa 4 (2026-09-26 a 2026-10-28):**

- **El nombre**, Hakaru CES, con sus archivos `ces_*` (DESIGN.md, «Nombre y
  formato»).
- **13 mejoras** (DESIGN.md §22): la sesión al salir, deshacer que no pierde
  nada al cambiar de problema y `[ctrl][Y]`, `[tab]` para elegir y abrir,
  `[menu]` en la ficha, el cursor en el culpable de un error, los avisos al
  sustituir o borrar, guardar mejor (`[ctrl][S]`, «Guardar como copia»);
  las letras de los nodos, la P con signo y el balance, la CA en el tiempo,
  el osciloscopio con `[U]` (el último periodo) y medidas entre cursores, y
  el disparo al nivel de lo que se ve.
- **El arranque tras los flancos** (DESIGN.md §17.3): el tramo rígido, con
  su banco de 25 casos contra la solución exacta.
- **La capa de entrada** (DESIGN.md §20): cada tecla actúa al bajarla, la
  repetición, el touchpad (clic en los bordes, las esquinas y el centro;
  deslizar), el panel Controles y un grabador para calibrarla. Y lo que
  esperaba por ella (§21): cuentas en la ficha, copiar, cortar y pegar, y
  el nombre escrito.
- **El cierre**: los arreglos de la verificación final (integración,
  pantallas y el arnés) y la calibración del modelo de tiempos con el banco
  de la calculadora real (DESIGN.md §19.5 y §19.8).
- **La optimización**, fase 1 (2026-09-27 a 29; DESIGN.md §23): un plan de
  19 puntos sacado de un perfil fiel del ARM; los 11 «bit a bit» y la coma
  flotante rápida (`cf_arm.c`), con los mismos resultados hasta el último
  bit: un ejemplo de CC −67 %, un punto de CA −60 %, el osciloscopio −79 %.
  El turbo (subir el reloj de la CPU) se programó pero quedó **fuera de la
  versión**, apagado (`make TURBO=1` para probarlo).
- **Capacitores equivalentes** (2026-10-07 y 08; DESIGN.md §24): la cuarta
  opción del Taller. Sustituye un capacitor con los de la bodega (uno, dos o
  tres; serie, paralelo o mixtos; con repetidos), con su paso a paso, el
  dibujo del cableado, Teoría y Ejemplos; verificada contra un oráculo de
  fuerza bruta.
- **La publicación** (2026-10-28; DESIGN.md §25): el código, en un
  repositorio con licencia MIT; la documentación, nueva, en `docs/`; los
  últimos nombres internos de ElectroFock, cambiados.

**Etapa 3 terminada (2026-09-25):** la corriente alterna. Fuentes alternas
(fasores, con la f de todo el circuito y el convenio de vista), impedancia Z y
vatímetro; análisis de fasores con 6 capas, tabla de 4 páginas (Boucherot y
Tellegen), diagrama fasorial y triángulo de potencias; Thévenin y Norton en CC
y en CA con la carga de máxima potencia; trazador de Bode con autorrango,
cursores exactos y medidas (fc, f0, BW, Q); la **calculadora de CA** (hasta 4
incógnitas, todas las soluciones, diagnósticos con nombre; corrige el fp);
lección 11 con 10 modelos. Verificado con 311 709 comprobaciones (0 fallos),
casos cerrados, el cruce exacto con el régimen del trapecio, la calculadora
contra mpmath y 300 problemas inversos, y una revisión adversarial. Detalles
y desviaciones: DESIGN.md §18. El banco ya midió el punto de CA y el Bode
(2026-09-27: `CA_NS_OP`); pendiente en la calculadora real, el color de la Z
y la legibilidad del ∠.

**Etapa 2 terminada (2026-09-25):** el emulador en el tiempo. C, L,
interruptores, fuentes con forma (senoidal, cuadrada, triangular, pulso),
sondas, osciloscopio de 2 canales con cursores, disparo y medidas, multímetro
en CC/CA/CA+CC, tabla de 5 páginas (con el balance de energía y la respuesta
natural), esquema emulado, ajustes de tiempo y paso, 12 circuitos modelo en la
lección 10 y la calculadora de CC con C, L y S. Verificado con 310 395
comprobaciones (0 fallos), soluciones exactas, el cruce entre los dos motores
y un **oráculo independiente** (92 de 92 redes al azar iguales, modos
incluidos). Detalles y desviaciones: DESIGN.md §17. Pendiente en la
calculadora real: el `[esc]` tras cancelar y los colores de los canales
(el banco ya midió el paso, 2026-09-27: `SIM_NS_PASO`).

### Etapas 0 y 1

- Motor de CC por tabla (cualquier red, no solo serie-paralelo), rápido: 8
  datos medidos con sus 256 combinaciones de signo en una sola eliminación.
- Resistencias, fuentes de tensión ideales y reales (con r), fuentes de
  corriente, voltímetros y amperímetros ideales o con R interna, tierra y
  potenciales de nodo, terminales A-B.
- Entrada de valores con prefijos (p n µ m k M G) y eco, shift/ctrl
  pegajosos, paleta, fichas de varios campos.
- Formato de archivo `EFK3` extensible por bloques (abría también el `EFK1`;
  hoy es el mismo formato con la marca `CESP`, y los viejos ya no se leen).
- Modo práctica, banco de tiempos para la calculadora real (Ayuda > T).
- Arnés de pruebas: 271 356 comprobaciones, 0 fallos.
- **Probado en la calculadora real** (banco de tiempos, 2026-09-25).

## 3. La pieza nueva: el motor de emulación (`sim.c`)

El motor actual (tabla) se queda para la **calculadora**, porque sabe
despejar incógnitas. Para **emular** se añade un segundo motor, el estándar de
los simuladores tipo SPICE:

- **Análisis nodal modificado (MNA)** con matriz dispersa. Incógnitas: los
  potenciales de nodo más la corriente de las fuentes de tensión. Es mucho más
  pequeño que la tabla: 10 a 30 incógnitas en un circuito normal.
- **Factorización LU una vez** mientras la topología no cambia. En cada paso de
  tiempo solo se sustituye hacia delante y hacia atrás, y eso lo hace viable
  sin FPU.
- **Condensadores y bobinas como "modelos compañeros"** (una conductancia más
  una fuente que cambian con el paso). Integración por **trapecio**, con Euler
  implícito en el primer paso y tras cada conmutación, para que no aparezcan
  oscilaciones falsas.
- **Eventos exactos**: si un interruptor conmuta en t = 2 ms, el paso se parte
  justo en ese instante.
- **Newton-Raphson** para los componentes no lineales (a partir de la Etapa 4),
  con limitación de paso, gmin y rampa de fuentes para que converja.
- **Complejos nativos** para la CA (Etapa 3): el mismo MNA con números
  complejos para una frecuencia o para un barrido.

Ventaja para la verificación: hay **dos motores independientes**. El punto de
operación del emulador tiene que coincidir con la solución de la calculadora
en miles de redes al azar. Si uno se equivoca, el otro lo delata.

## 4. Etapas

### Etapa 2 · El emulador en el tiempo · HECHA

**Componentes nuevos**

| Tecla | Componente | Ficha |
|---|---|---|
| `C` | Condensador | C, tensión inicial (vacía = la del punto de operación) |
| `L` | Bobina | L, corriente inicial |
| `S` | Interruptor | abierto/cerrado al empezar, conmuta en t₁ (y opcionalmente vuelve en t₂) |
| `E`, `J` | Generador de funciones: las fuentes ganan una **forma** | continua · senoidal (amplitud pico, f, fase, nivel de CC) · cuadrada (V₁, V₂, f, ciclo de trabajo) · triangular · escalón/pulso (retardo, ancho) |
| `O` | Sonda del osciloscopio | la primera es el canal 1 y la segunda el canal 2; miden respecto a tierra |

**Instrumentos y resultados**

- **Osciloscopio de 2 canales** a pantalla completa: retícula de 10×8
  divisiones; V/div por canal y t/div con las flechas; disparo por flanco;
  **cursores T1 y T2** con lectura de t, V₁, V₂, ΔT y ΔV; medidas automáticas
  de cada canal (máximo, mínimo, pico a pico, media, eficaz y frecuencia).
  Además de un nodo, un canal puede medir la corriente de un elemento.
- **Multímetro**: el voltímetro y el amperímetro ganan el modo CC (media) o
  CA (eficaz) en su ficha.
- **Tabla final**: V, I y P de cada elemento en el último instante, sus
  máximos, y la energía de cada C y cada L.
- Tiempo simulado y paso **automáticos** (a partir de las constantes de tiempo
  y de las frecuencias del circuito), editables. Mientras calcula se ve una
  barra de progreso, y `esc` cancela.

**En la calculadora:** C y L dejan de ser un error: en CC el condensador es un
abierto y la bobina un corto, y la tabla da Q = C·V y la energía almacenada.

**Biblioteca:** carga y descarga RC, RL, RLC serie (sub, crítico y
sobreamortiguado), oscilador LC, RC con onda cuadrada (integrador y
derivador), conmutación con interruptor.

**Verificación:** soluciones exactas (RC, RL, los tres casos del RLC, energía
conservada en un LC); integración de referencia muy fina en el PC sobre redes
RLC al azar; punto de operación del emulador igual al de la calculadora; el
mono con las teclas nuevas.

**Orden de trabajo:** 1) `sim.c` con el punto de operación, comprobado contra
el motor actual y sin cambios visibles; 2) C y L, y el transitorio con sus
pruebas exactas; 3) formas de onda e interruptor; 4) pantalla del
osciloscopio; 5) multímetro y tabla; 6) biblioteca; 7) auditoría.

**Al terminar podrás:** dibujar un RLC, aplicarle una cuadrada de 1 kHz y ver
en el osciloscopio la tensión del condensador sonando, medir con los cursores
el periodo de la oscilación y el sobrepico.

### Etapa 3 · Corriente alterna · HECHA

- **Análisis de CA de una frecuencia:** fasores de V e I de cada elemento en
  polar o rectangular (la tecla punto cambia la forma); P, Q, S y fp por
  elemento y en total; **diagrama fasorial** y **triángulo de potencias**.
- **Trazador de Bode:** barrido logarítmico de frecuencia entre una fuente y un
  nodo (o V(AB)); magnitud en dB y fase; cursores; medidas automáticas de
  frecuencia de corte (−3 dB), resonancia, ancho de banda y Q.
- **Vatímetro** (P y fp) y **ohmímetro / Z entre A y B**.
- **Thévenin y Norton entre A y B**, en CC y en CA, con la carga de máxima
  transferencia de potencia.
- **Calculadora en CA:** datos e incógnitas como fasores o solo módulos ("el
  voltímetro en R marca 72 V: ¿cuánto vale L?"). **Corrección del factor de
  potencia**: "¿qué C lleva la carga a fp 0,95?".
- Componente **impedancia Z** (R + jX) para los problemas de libro que la dan
  así.
- Glifos ∠ y j en la fuente de letra.
- **Biblioteca:** filtros pasa-bajas, pasa-altas y pasa-banda; resonancia serie
  y paralelo; corrección del fp de un motor.
- **Verificación:** el fasor tiene que coincidir con el régimen permanente del
  transitorio (los dos motores, otra vez cruzados); Tellegen (ΣV·I* = 0);
  Boucherot (ΣP y ΣQ por separado); con ω → 0, lo mismo que la CC.

### Etapa 4 · Semiconductores y amplificadores · HECHA

- **Diodo**, con modelo a elegir: ideal, con Vγ o exponencial (Shockley), más
  modelos de catálogo (1N4148, 1N4007). **Zener** (Vz). **LED** (el color fija
  su Vf): **se enciende en el esquema** con un brillo según su corriente.
- **Fuentes controladas** (E o J controladas por la V o la I de otro elemento;
  se dibujan como rombo).
- **Amplificador operacional**: ideal, o real con ganancia y saturación en
  ±Vsat. Es el primer componente de 3 patas y ocupa su propia huella en la
  rejilla.
- **Barrido de CC**: curva I-V de un componente, curva de transferencia
  Vsal(Vent).
- Al final, lo más pesado: **transistores BJT** (NPN y PNP, con β) y
  **MOSFET** (canal N y P).
- **Circuitos que se abren:** rectificadores de media onda, de onda completa y
  de puente, con filtro y rizado; recortadores y fijadores; regulador zener;
  AO inversor, no inversor, sumador, integrador, derivador, comparador y
  astable; transistor como interruptor; amplificador en emisor común con su
  punto Q.
- **Verificación:** casos cerrados del diodo; residuo de Kirchhoff en cada
  solución de Newton; AO ideal frente a uno de ganancia 10⁷.

Se hizo todo lo de arriba (§2). Lo que se apartó del plan y por qué está
en DESIGN.md §19.4; lo que queda, en §19.9.

### Etapa 5 · Transformadores, trifásica y emulación interactiva · SIGUIENTE (sin empezar)

- **Transformador ideal** (relación n) y **bobinas acopladas** (L₁, L₂, k o M,
  con sus puntos), en el tiempo y en CA.
- **Trifásica:** fuente trifásica (Y o Δ, secuencia, tensión de línea o de
  fase), cargas equilibradas y desequilibradas, corriente de neutro y método
  de los dos vatímetros.
- **Emulación en vivo:** el osciloscopio corre continuo, como uno real. Los
  interruptores se accionan con su tecla mientras corre, el **potenciómetro**
  sube y baja con `+`/`−`, y hay **lámpara** que brilla y **fusible** que se
  funde.
- **Barrido de parámetro:** curvas superpuestas variando una R, C o L.
- **Espectro (Fourier)** de un canal del osciloscopio, si cabe en tiempo.

### Continuo · Calculadora real y pulido

- El banco de tiempos (Ayuda > T) en la calculadora real fija el costo por
  operación. De eso dependen el paso máximo del emulador y cuántos puntos
  caben en un barrido. Se corrió el 2026-09-25 (la coma flotante, la CC y
  la pantalla) y el 2026-09-27, con y sin el cable USB (con el de sin USB
  se calibró el modelo de tiempos, DESIGN.md §23.4). Lo que falta medir y
  qué mirar a ojo: §9.
- La optimización: la fase 2 (puntos 12 a 14, dentro de tolerancia), la
  fase 3 (15, 16 y 18) y el punto 19, recalibrar con el banco de la versión
  optimizada (DESIGN.md §23.6).
- Manual de uso ([MANUAL.md](MANUAL.md), hecho) y páginas **Modelo** de cada
  componente (las ecuaciones, con `[catálogo]` en la ficha).

## 5. Tiempos esperados en la calculadora

**Medido en la calculadora real (2026-09-25):** una operación de coma
flotante (multiplicar y restar) cuesta 0,630 µs; el peor caso de CC de hoy,
82 ms; volcar la pantalla, 4,3 ms. **La segunda corrida (2026-09-27), sin el
cable USB** (con él, el reloj baja a 288 MHz y todo sale 1,37 veces más
lento): 0,463 µs por operación, 43,45 µs fijos por paso del emulador, 214,3
µs por iteración de Newton y 1,23 µs por operación de CA. Con eso (DESIGN.md
§23.4; las estimaciones son de antes de la optimización, que las rebaja):

| Operación | Estimado |
|---|---|
| Punto de operación de CC | menos de 50 ms |
| Transitorio lineal, 20 nodos, 2 000 pasos | 0,2-1 s (estimado en el arnés: RC y RLC de la biblioteca, 0,21-0,28 s; el peor caso de 32 elementos se limita a 2 s) |
| Punto de operación con no lineales | el peor caso (32 elementos, 10 diodos, 2 AO): ~16 ms, estimado en el arnés |
| Transitorio con diodos o transistores (Newton) | 1-5 s, con barra y `esc` (estimado en el arnés: el puente con 1N4007, 5 periodos, ~2,4 s; el astable, ~0,87 s) |
| Barrido de CC | 281 puntos de un diodo: ~0,35 s, estimado |
| CA de una frecuencia | menos de 50 ms (el peor caso de 32 elementos: ~9,1 ms, estimado en el arnés) |
| Bode de 100 puntos | alrededor de 0,5 s (el de un RLC con autorrango y 277 puntos: medido, 0,133 s; el arnés estima ~0,19) |
| Calculadora de CA | el motor con dos incógnitas: ~0,35 s (estimado); tope de 3 s |

## 6. Regla de admisión

Un componente o análisis nuevo entra solo con:

- su modelo en el emulador (y su ley en la calculadora, si aplica);
- símbolo, ficha y tecla (o lugar en la paleta);
- una prueba con resultado exacto conocido y una prueba cruzada entre los dos
  motores;
- un circuito modelo en la biblioteca;
- su página Modelo con las ecuaciones que usa.

Ya no se exige la regla de pizarra.

## 7. Decisiones tomadas por omisión (se pueden cambiar)

- **`=` unificado:** calcula si hay algo marcado con `?!`; si no, emula.
- **Generador de funciones en amplitud pico** (como un generador real y como
  Multisim). **Los instrumentos marcan valores eficaces** en CA. En la
  calculadora de CA, los fasores son eficaces, con coseno y grados.
- **Teclas:** la letra de la etiqueta: `C` condensador, `L` bobina, `S`
  interruptor, `O` sonda, `D` diodo, `U` amp. op., `Q` transistor (BJT), `M`
  MOSFET. Lo menos
  usado (transformador, lámpara, fusible...) va en la paleta.
- **Tope de 32 elementos por circuito.** Caben un puente rectificador con
  filtro y carga, o una trifásica con sus vatímetros. Se ampliaría solo si se
  queda corto.

## 8. Lo que no se hará (o se aplaza)

- Rehacer la CC paso a paso para los componentes nuevos (se sustituye por las
  páginas Modelo).
- Laplace simbólico o un CAS propio: la Nspire ya lo hace.
- Más de 2 canales en el osciloscopio: no caben en 320×240 con lecturas.
- Circuitos digitales (compuertas, flip-flops): son otro simulador; quizá
  después de la Etapa 5.

## 9. Pendiente en la calculadora real

Todo se ha probado en el PC (el arnés, el mono y los oráculos). En la CX II
CAS se ha corrido el banco de tiempos (2026-09-25 y 2026-09-27) y se ha
usado el programa. Falta, en orden:

1. **El banco con la versión optimizada**, sin USB (Ayuda > `[T]`, sus dos
   páginas), tras apagar y encender y reactivar Ndless: con eso se
   recalibran `SIM_NS_OP`, `SIM_NS_PASO`, `NS_ITER` y `CA_NS_OP` (el punto 19
   de la optimización).
2. **Los tiempos del modelo 28 y del astable**: abrir «Puente rectificador
   con filtro» (el 28) y «Astable con AO» (el 39), `[=]`, y cronometrar la
   emulación. El arnés estimaba ~2,3 s el puente y ~0,87 s un astable de 10
   ms antes de la optimización. Si tardan mucho más, es Newton, que
   `elige_paso` no prevé entero (DESIGN.md §19.9).
3. **La capa de entrada.** La repetición (mantener las flechas, `[del]` y las
   escalas; Controles > Repetición) y el deslizamiento (el dedo sin hundir
   mueve el cursor; que no dé pasos al llevarlo a una flecha para hundirla;
   «Deslizar: no» los quita). Y el **grabador**: `[ctrl]` `[menu]`
   (Controles) y `[G]`, oculto, graban 10 s de muestras crudas en
   `ces_muestras.tns`. Grabar deslizamientos lentos y rápidos, el dedo
   quieto, clics en los bordes, las esquinas y el centro, buscar una flecha
   desde el centro, las teclas de las cuentas (espacio, ×, ÷, ^, x², ( y ))
   y tres teclas en rectángulo. En el PC,
   `HAKARU_GRABACION=ces_muestras.tns HAKARU_SOLO_CAPA=1 ./out/hakaru_test out`
   las pasa por la capa; con eso se ajustan las `TP_*` y `REP_*` de
   `entrada.h`. Anotar también lo que dice Controles abajo: el touchpad y el
   reloj de la capa (el plan y sus cambios, las cuentas del SP804 por
   segundo y el tick de `idle()`; mejor tras un minuto quieto).
4. **Los colores del LED y los símbolos de 3 patas**: los seis colores
   encendidos, sus 8 niveles de brillo, el apagado (menos de If/100) y «se
   quemaría»; que el AO, el BJT y el MOSFET se lean en la rejilla y en los
   resultados («Vo -15,0V», «satura -» dentro del triángulo) y la línea
   discontinua de una controlada hasta su control.
5. **De antes**: el `[esc]` tras cancelar una emulación y los colores de los
   canales del osciloscopio (Etapa 2); el color de la Z y el ∠ (Etapa 3).

Las constantes están todas con nombre: `entrada.h` (la capa), `sim.h`,
`nolin.h` y `ca.h` (los tiempos). Con los números reales se cambian ahí y se
vuelve a correr la batería (`test_calibracion`, en [14], lleva las cargas del
banco).

## 10. Fuera de esta versión

- **El turbo** (DESIGN.md §23.5): programado y probado en el PC con un
  simulador de la PMU, pero apagado. Antes de activarlo hay que estudiar el
  método nuevo (la transición del propio sistema operativo), verificarlo
  contra la versión del SO de la calculadora y proponerlo como prueba guiada,
  con una medida real de la CPU.
- **Electrostática**: un módulo aparte (simulador de cargas, campo y
  potencial, Gauss, conductores, capacitores), propuesto y empezado por
  separado; no forma parte de esta versión.
