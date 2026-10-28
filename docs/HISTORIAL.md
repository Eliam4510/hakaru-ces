# Historial de Hakaru CES

De la calculadora de CC al emulador de CC y CA. Las fechas son de 2026. El
detalle de cada etapa está en [DESIGN.md](DESIGN.md) y lo que viene, en la
[hoja de ruta](HOJA_DE_RUTA.md).

## 4.0 · Publicación (28 de octubre)

- El código, en un repositorio con licencia **MIT**; autor, **Eliam Mojardin
  Ibarra** (también en la Ayuda, en ACERCA DE).
- La documentación, escrita de nuevo para el repositorio: el
  [README](../README.md), el [manual](MANUAL.md), la
  [guía de compilación y pruebas](COMPILAR.md), este historial, la hoja de
  ruta al día y el diseño completo (con la Etapa 4, la optimización y los
  capacitores equivalentes, que faltaban), en `docs/`.
- Los últimos nombres internos de ElectroFock, cambiados: las variables del
  arnés `EFK_*` son `HAKARU_*`; sus binarios, `out/hakaru_test` y
  `out/hakaru_capa`; las redes fijas del banco del flanco usan `cesnet`, como
  las que exporta el arnés (y `banco_flanco.py` vuelve a leerlas con el
  oráculo no lineal).
- La versión pública y la especial del autor salen del mismo árbol con la
  bandera `ESPECIAL` (DESIGN.md §25).
- Arnés: 352 164 comprobaciones, 0 fallos.

## Capacitores equivalentes (7 y 8 de octubre)

- **Capacitor de motor** (v1, 7 de octubre): la cuarta opción del Taller.
  Con el capacitor de un motor monofásico y la bodega (uno de cada valor),
  las combinaciones en serie o paralelo de la más cercana a la más lejana, el
  procedimiento y el dibujo del cableado; si no hay solución, qué capacitor
  faltaría.
- **v2**: la bodega admite repetidos (cada aparición es un capacitor).
- **Capacitores equivalentes** (v3, 8 de octubre): nombre genérico y menú
  propio (Resolver, Teoría, Ejemplos); edición con cursor en los campos;
  resultado en pestañas (Opciones y Paso a paso) con el procedimiento
  explicado con palabras; arreglos de **tres** capacitores (paralelo, serie
  y los dos mixtos), con el modo Auto/2/3. Verificada contra un oráculo de
  fuerza bruta: 15 039 de 15 039 casos iguales.

## Optimización (27 al 29 de septiembre)

- Un perfil fiel del ARM y un plan de 19 puntos. La fase 1, **bit a bit**
  (los mismos resultados hasta el último bit): la LU dispersa, el paso del
  transitorio, el planificador del tramo rígido, el barrido, la CA y el
  Bode, el dibujo por palabras de 32 bits y con cachés, y **la coma flotante
  rápida** (`cf_arm.c`: división, raíz y comparaciones exactas por
  `--wrap`). Un ejemplo de CC −67 %, un punto de CA −60 %, el osciloscopio
  −79 %; nada empeora.
- El modelo de tiempos, recalibrado con el banco de la calculadora corrido
  sin el cable USB; el banco gana una segunda página para recalibrar.
- **El turbo** (subir el reloj a 408-456 MHz) se programó con reglas
  estrictas, pero quedó fuera de la versión, apagado: la única referencia
  que escribía el mismo registro lo cambió por la transición del sistema
  operativo.
- Arnés: 351 910 comprobaciones, 0 fallos.

## Hakaru CES (26 y 27 de septiembre)

- **El nombre**: *Hakaru* (測る, «medir») **CES** (*Circuit Emulator &
  Solver*), con el kanji en la portada. El programa es `hakaru_ces.tns`; los
  problemas, `ces_NN.tns` con la marca `CESP` (antes `efk_mis_NN.tns` con
  `EFK3`: los archivos viejos ya no se leen).
- **13 mejoras**: la sesión al salir, deshacer que no pierde nada al cambiar
  de problema, `[ctrl][Y]`, `[tab]` para elegir, el cursor en el culpable de
  un error, guardar como copia; las letras de los nodos, la P con signo y el
  balance, la CA en el tiempo, el osciloscopio con el último periodo y las
  medidas entre cursores.
- **El arranque tras los flancos**: un tramo rígido (SDIRK2) para los modos
  rígidos tras un salto de una fuente, con un banco de 25 casos contra la
  solución exacta.
- **La capa de entrada**: cada tecla actúa al bajarla, la repetición con
  perfiles, el touchpad (clic en los bordes, las esquinas y el centro;
  deslizar), el panel Controles y un grabador para calibrarla en el PC. Y lo
  que esperaba por ella: cuentas en la ficha, copiar, cortar y pegar, y el
  nombre escrito.
- **El cierre de la Etapa 4**: los arreglos de la verificación final y la
  calibración del modelo de tiempos con el banco de la calculadora real.
  Arnés: 351 464 comprobaciones, 0 fallos.

## Etapa 4 · Semiconductores y amplificadores (25 y 26 de septiembre)

- Diodos (ideal, con Vγ, exponencial, 1N4148, 1N4007), zener, LED, las
  cuatro fuentes controladas, el amplificador operacional (ideal y real,
  con saturación), el BJT y el MOSFET: los primeros componentes de tres
  patas.
- El punto de operación por Newton, el transitorio no lineal, la pequeña
  señal (CA y Bode en el punto Q), el barrido de CC con su pantalla de
  curvas y el trazador de un componente (curva I-V, familia de curvas).
- Lecciones 12 (Diodos) y 13 (AO y transistores), con 20 modelos.
- Un **oráculo no lineal** independiente (260 de 260 redes iguales) y tres
  revisiones adversariales, con todos sus hallazgos arreglados.

## Etapa 3 · Corriente alterna (25 de septiembre)

- Fuentes alternas (fasores eficaces con coseno), impedancia Z y vatímetro.
- El análisis de fasores (6 capas, la tabla con Boucherot y Tellegen), el
  diagrama fasorial y el triángulo de potencias; Thévenin y Norton con la
  carga de máxima potencia.
- El trazador de **Bode** con autorrango, cursores y medidas (fc, f0, BW,
  Q), y la **calculadora de CA**, que despeja hasta 4 valores con todas sus
  soluciones.
- Lección 11 con 10 modelos. Oráculo de CA: 175 de 175. Arnés: 311 709.

## Etapa 2 · El emulador en el tiempo (25 de septiembre)

- El motor MNA (`sim.c`): condensadores, bobinas, interruptores, fuentes
  con forma (senoidal, cuadrada, triangular, pulso).
- El osciloscopio de dos canales, las sondas, el multímetro en CC/CA, la
  tabla con el balance de energía y la respuesta natural.
- Lección 10 con 12 modelos. Oráculo independiente: 92 de 92. Arnés:
  310 395.
- **Primera corrida en la calculadora real** (el banco de tiempos, 25 de
  septiembre).

## ElectroFock · Etapas 0 y 1 (23 y 24 de septiembre)

- El diseño de una calculadora de circuitos de CC para la TI-Nspire CX II
  (con el nombre provisional «Electroestatica»), hermana de EstaticaCX.
- **ElectroFock**: el Taller (dibujar y resolver, con el procedimiento de la
  pizarra), Aprende (8 lecciones y 9 problemas de pizarrón, 40 ejemplos con
  clave) y la Ayuda; Mis problemas, deshacer, el modo práctica y el banco
  de tiempos.
- Fuentes reales (con r), fuentes de corriente, voltímetros y amperímetros,
  tierra y potenciales, terminales A-B, prefijos, la paleta.
- El 24 de septiembre, el rumbo: deja de ser una ayuda para un examen y
  pasa a ser **un emulador con calculadora al estilo de NI Multisim**, con
  la hoja de ruta de CA/CC.
- Arnés: 271 356 comprobaciones, 0 fallos, y el mono, millones de teclas al
  azar.
