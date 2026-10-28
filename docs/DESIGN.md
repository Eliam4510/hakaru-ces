# Hakaru CES — diseño

Emulador y calculadora de circuitos para la TI-Nspire CX II CAS (Ndless).

Documento de diseño: la arquitectura, los motores, las decisiones y cómo se
verificó cada etapa. Empezó como la **versión 2** del diseño de una
calculadora de CC (con el nombre provisional «Electroestatica»; luego el
programa se llamó **ElectroFock** y, desde el cierre de la Etapa 4, **Hakaru
CES**). Es hermano de EstaticaCX: mismo SDK, mismo esqueleto, misma filosofía
de verificación. Las secciones con fecha son la historia del proyecto y
conservan los nombres de su momento; lo que vale hoy de nombres, archivos y
marcas está en «Nombre y formato», y el estado de hoy, con el mapa de las
secciones, en «Estado».

En los comentarios del código y en este documento aparecen rutas como
`rev9/…`, `capacitores/…` o `scratchpad/…`, y nombres de flujos de trabajo
(«hakaru-cierre», «workflow»…): son los documentos de trabajo y las
revisiones del desarrollo, que no forman parte del repositorio. Lo que de
ellos importa está resumido aquí.

## Nombre y formato

**Nombre.** *Hakaru* (測る, «medir» en japonés) **CES**, sigla descriptiva
de *Circuit Emulator & Solver*: un emulador y calculadora de circuitos de CC
y CA. Nombre completo: «Hakaru CES». Antes, ElectroFock (hasta el
2026-09-26). Nunca lleva número de versión en el nombre (la versión, 4.0,
solo en ACERCA DE). El nombre visible vive en `app.h` (`APP_NOMBRE`,
`APP_TITULO`, `APP_SIGLA`, `APP_SIGLA_L`, `APP_VERSION`): portada, barra del
menú, tarjeta y ACERCA DE de la Ayuda. Los textos didácticos (lecciones) no
nombran el programa («el Taller», «el programa»), para que otro cambio de
nombre no los toque.

| Qué | Hoy | Antes (ElectroFock) |
|---|---|---|
| Programa | `hakaru_ces.tns` (`EXE = hakaru_ces`; `genzehn --name "Hakaru CES"`) | `electrofock.tns` |
| Mis problemas | `ces_01.tns` … `ces_20.tns`, marca **`CESP`** | `efk_mis_NN.tns`, `EFK3` (y `EFK1`) |
| Sesión (el editor al salir) | `ces_sesion.tns`: un **`CESP`** con cola | (no había) |
| Ajustes de los controles (§20.4) | `ces_controles.tns`: **`CESK`**, versión 1 y los tres ajustes (8 bytes) | (no había) |
| Grabador de la capa (§20.4) | `ces_muestras.tns`: **`CESM`**, cabecera de 12 bytes y 27 por muestra | (no había) |
| Arnés → oráculos | `cesnet 2` (CA), `cesnet 3` (no lineal); resultados `.ces` | `efknet 2/3`, `.efk` |
| Arnés: binario y variables | `out/hakaru_test`, `out/hakaru_capa`; `HAKARU_SOLO_*`, `HAKARU_MONO`, `HAKARU_SEMILLA`, `HAKARU_DEPURA`… | `out/electrofock_test`, `EFK_*` |

Los archivos van en la misma carpeta que el `.tns` (`arch_carpeta`, de
`argv[0]`). Los de antes no se leen: con otro nombre ni aparecen; un archivo
con la marca vieja renombrado a `ces_NN.tns` (o un `CESP` de una versión más
nueva, o uno dañado) sale en Mis problemas como «(no se puede abrir)», con
el aviso «Este archivo está dañado o no es de esta versión. [del] lo
borra.», y se borra con `[del]` (lo prueba [19], con un `CESP` de versión 9
y con la marca `EFK3`).

**CESP** (P de problema; `archivo.c`). Little-endian, campo a campo:
`"CESP"`, versión (1 byte), columnas y filas de la rejilla, nombre (24
bytes), segmentos h y v, bloque global y un bloque por elemento, cada bloque
precedido de su longitud (1 byte) y crecido por secciones al final (global:
tierra, terminales, convenio, frecuencia; sección 2, sondas, `t_fin` y paso;
sección 3 (16 B), la cola del nombre, bytes 24 a 39, solo si pasa de 23
(`Circuito.nombre[40]`: los títulos de la biblioteca llegan a 39; lo que se
escribe con `[ctrl][N]` y las copias siguen en 23);
elemento: tipo, patas, etiqueta, val, V, I, aux, variante, ref; sección 2,
los `par` del emulador; sección 3, `xc`, `yc` en los de 3 patas). Es el
mismo cuerpo que tenía `EFK3`: solo cambió la marca. La **versión** dice qué
parte del modelo hace falta conocer para leerlo bien: 1 la CC; 2 lo que
cambia con el tiempo (C, L, S, una E o J que no es continua); 3 la CA
(alterna, Z, vatímetro, ángulos); 4 semiconductores, AO y controladas. El
programa lee hasta `VERSION_MAX` = 4 y rechaza lo más nuevo en vez de
leerlo a medias; lo que no pide versión (sondas, parámetros) va en
secciones que un lector anterior salta. La cola del nombre tampoco sube la
versión: un lector anterior salta la sección 3 y ve los 23 primeros
caracteres. El inversor con AO de la Etapa 4 ocupa 667 B (lo fija [55]).

**Sesión** (`ces_sesion.tns`; `arch_sesion_*`). El circuito del editor en
`CESP` (el mismo escritor y el mismo lector, con `circ_valido`) y detrás una
cola de 8 bytes: `'S'`, hueco + 1 (0 = ninguno), cursor x e y, y la huella
del archivo de ese hueco al escribir la sesión (FNV-1a de 32 bits de todos
sus bytes, little-endian). El lector de `CESP` no mira lo que va detrás del
último elemento, así que la cola no toca el formato; sin ella (o con otra)
el circuito vale igual, con hueco -1 y el cursor en (0, 0). La escribe
`ui_sesion_guardar` al salir del programa (main.c) y al dejar el editor por
el Taller o por «Salir al menú»; con el editor vacío, guarda el último
circuito con algo del historial de deshacer, con su hueco (tras «Problema
nuevo», el de antes); vacío y sin historial, se borra. La lee `ui_sesion_abrir`,
solo desde main.c y nunca en `ui_init` (el arnés empieza cada prueba vacío
y borra el archivo al arrancar): el hueco vale solo si su archivo sigue ahí
y con la misma huella. Si otro problema ocupó ese hueco (se borró y se
guardó otro, o se copió otro con TI Connect), Guardar va a un hueco nuevo
en vez de pisarlo; si el propio problema se guardó después de la sesión y
el programa no llegó a salir bien, también (una copia de más, nunca uno
perdido). Un archivo roto no se carga y se empieza vacío, como antes; si no
se puede escribir, no se dice nada.

Dentro del programa, borrar un problema en Mis problemas lo olvida en el
editor y en sus niveles de deshacer y rehacer (`olvida_hueco`): si otro
problema reutiliza el hueco, deshacer hasta el de antes y Guardar no lo
pisa.

**CESNET** (texto ASCII del arnés para los oráculos de Python del PC, no
son datos del usuario). La primera línea que no es comentario es
`cesnet 2` (red de CA, `herramientas/oraculo_ca.py`, [29b]) o `cesnet 3`
(CESNET 2 con `dc` o `tran` y los no lineales, `herramientas/oraculo_nl.py`,
[67]). Antes va un comentario con el nombre (`# Hakaru CES: red …`) y, en
CESNET 3, otro con lo que hizo el motor, `# motor: …` (lo lee `rechazada()`
del oráculo; no lleva el nombre ni la sigla, para no confundirse con el
primero). Lo que da el programa va al lado con la extensión **`.ces`**:
`caso_NNN.ces`, `caso_NNN_h2.ces` ([29], `compara_oraculo.py`),
`caso_NNN_h4.ces` y `rechazada_NNN.ces`. La red de [29] (`oraculo_sim.py`)
no lleva marca: tras el comentario del nombre, empieza por `tfin … h …`.
Las redes fijas del banco del flanco (`herramientas/c204/`, `test_flanco.inc`)
usan el mismo formato.

**Portada.** «HAKARU» a ×3 en `C_FUENTE` y, a su derecha, el kanji 測る
como imagen: `kanji_img.c` (59×30, RGB565, 3,5 KB), generado por
`herramientas/gen_kanji.py` con Noto Sans JP Bold a 31 px (SIL OFL 1.1;
créditos en ACERCA DE), sobremuestreado ×8 y mezclado contra `C_FONDO` con
la cuenta de `RGB()`: se dibuja copiando filas (`dib_kanji`) y un píxel sin
tinta es exactamente el fondo. Si cambia la paleta hay que regenerarlo (el
arnés lo comprueba: [9] busca la imagen entera en la pantalla y mira que su
fondo sea `C_FONDO` y su tinta `C_TEXTO`). La guarda de `kanji_img.h` es
`KANJI_IMG_API_H`, no `KANJI_H` (el alto de la imagen: una guarda que se
llama como una constante vacía la cabecera en silencio). Debajo, «CES ·
Circuit Emulator & Solver», el lema «Emulador y calculadora de circuitos de
CC y CA», la miniatura del modelo «Emisor común: el punto Q» (la busca por
título en la biblioteca; si no estuviera, no hay miniatura, y [9] lo nota:
mira que su zona no se quede en `C_FONDO`) y «Dibuja, emula y mide como en
el laboratorio; despeja lo que buscas, paso a paso.». La portada no tiene
animación.

**Si venías de ElectroFock.** En la calculadora, los `electrofock.tns` y
`efk_mis_NN.tns` de antes se pueden borrar: el programa ya no los lee. En la
carpeta del proyecto, `make clean` solo borra `hakaru_ces.*` (y los `.o`):
un `electrofock.tns` o un `electrofock.elf` de una compilación con el nombre
viejo se borran a mano.

## Estado

**Hoy (2026-10-28): Hakaru CES 4.0.** Hechas las etapas 0 a 4 de la
[hoja de ruta](HOJA_DE_RUTA.md); la 5 (transformadores, trifásica y
emulación en vivo) es la siguiente y no está empezada. Tras la Etapa 4
vinieron, por este orden: el nombre (arriba), las 13 mejoras de la tanda 1
(§22), el arreglo del arranque tras los flancos (el tramo rígido, §17.3), la
capa de entrada, con lo que esperaba por ella (§20 y §21), y el cierre: los
arreglos de la verificación final (integración [73], pantallas [74] y el
arnés, §19.5) y la calibración del modelo de tiempos con el banco de la
calculadora real (§19.8). Después, la **optimización** (§23), la función
**Capacitores equivalentes** (§24) y la preparación para publicar el código
(§25).

| Qué | Dónde |
|---|---|
| Etapas 0 y 1: la calculadora de CC, el procedimiento y el editor | §0 a §16 y las secciones con fecha |
| Etapa 2, el emulador en el tiempo (y el tramo rígido tras los flancos) | §17 |
| Etapa 3, la corriente alterna | §18 |
| Etapa 4, semiconductores, AO y barrido de CC; el oráculo no lineal y las tres revisiones | §19 |
| La capa de entrada: teclas, repetición y touchpad | §20 |
| Cuentas en la ficha, copiar y pegar, y nombres | §21 |
| Las 13 mejoras de la tanda 1 | §22 |
| La optimización (rev9): coma flotante rápida, motor, dibujo y la recalibración | §23 |
| Capacitores equivalentes | §24 |
| La versión pública y la especial | §25 |
| Lo que falta probar en la calculadora real | [HOJA_DE_RUTA.md](HOJA_DE_RUTA.md#9-pendiente-en-la-calculadora-real) |

Números de hoy: la batería entera, **352 164 comprobaciones, 0 fallos**; el
`.tns` público, **898 480 bytes**, compilado sin avisos. Los tres oráculos de
Python: el lineal del tiempo ([29]) 98 de 98, el de CA ([29b]) 174 de 174 y
el no lineal ([67]) 260 de 260 (más los 4 que ninguno de los dos hace
converger, de siempre).

Números del cierre de la Etapa 4 (2026-09-27), para la historia: la batería
entera, 351 464 comprobaciones, 0 fallos (igual con `arm`, con `asan` y por
la capa, con las mismas BMP y el mismo texto); `HAKARU_SOLO_E4`, 39 531 y 0;
el mono, un millón de teclas por cuatro semillas, sin fallos.

## Estado a 2026-09-24

Estructura pedida por el usuario: **Taller** (dibuja y resuelve), **Aprende**
(teoría y ejemplos fusionados: 8 lecciones + los 9 del pizarrón, 40 ejemplos
con clave verificada) y **Ayuda**. Texto en Terminus 6×12, código en UTF-8
compilado a Latin-1. Editor con barra de herramientas visible, menú de
opciones, deshacer y "Mis problemas" (archivos `efk_mis_NN.tns` junto al
programa; hoy `ces_NN.tns`). Procedimiento general por propagación en el árbol (explica el
99 % de los inversos aleatorios; el resto, Kirchhoff). Arnés: 271 356
comprobaciones, 0 fallos (también con `asan` y `arm`, y el mono con
millones de teclas).

**Primera corrida en la calculadora real (2026-09-25).** El banco de tiempos
(Ayuda > T) en la CX II CAS del usuario dio: multiplicar y restar en coma
flotante 0,630 µs (250 ciclos a 396 MHz; se estimaba 0,600), un ejemplo
resuelto con sus pasos 3,03 ms, el peor caso de CC (31 R, 8 datos, 119 914
operaciones de Gauss) 81,6 ms, volcar la pantalla 4,34 ms, y un cuadro de
las pantallas animadas, de 4,4 a 6,8 ms (sin el volcado). El arnés usa esos
números (`NS_POR_OP`, `F_RESTO`, `F_CUADRO`, `MS_VOLCAR`) y sus estimaciones
quedan un poco por encima de lo medido. Con la espera de 25 ms del bucle,
las animaciones van a unos 28 cuadros por segundo.

**Segunda corrida (2026-09-27, sin la capa nueva).** La coma flotante
0,63369 µs (251 ciclos); un ejemplo 3,33 ms; el peor caso de CC 86,96 ms;
volcar la pantalla 4,3384 ms; un cuadro animado, de 4,35 a 9,22 ms; `sin()` 9,96
µs; `exp()` 10,768 µs, 17 operaciones; un paso del peor emulador 1,0347 ms;
una vuelta del puente con 1N4007 1,4668 ms; un punto de CA del peor caso
8,7146 ms; el Bode de un RLC 133,33 ms. Con ellos se calibró el modelo de
tiempos (§19.8); el arnés usaba `F_CUADRO` 210 y `MS_VOLCAR` 4,3384. Esta
corrida se hizo con el cable USB puesto, y con USB el SO puede bajar el reloj
de 396 a 288 MHz: otra, sin USB y tras apagar y encender, dio 0,46329 µs por
operación (183 ciclos), 1,37 veces menos. Con esa se recalibró todo en la
optimización (§23.4).

### Verificación a fondo (2026-09-24)

Herramientas nuevas del arnés: **mono** (hasta millones de teclas al azar por
toda la app; exige circuito sano, las 15 pantallas visitadas y cero textos
fuera de pantalla; `mono.sh`, `HAKARU_MONO`, `HAKARU_SEMILLA`, `HAKARU_DEPURA`),
**archivos rotos** (3000 archivos mutados: el que se carga tiene que pasar
`circ_valido`), modo **`arm`** (`-funsigned-char`), **tiempos** y
**fracciones**. Lo que encontraron y se arregló:

- `archivo.c` aceptaba circuitos incoherentes → `circ_valido()` en `circuito.c`.
- Selección colgando de un elemento borrado o deshecho → `sel_el = -1`.
- Un número mal escrito se descartaba en silencio → la ficha dice por qué
  (no es un número, más de 10⁹, R ≤ 0).
- Rótulos del esquema y columnas de la tabla que se salían por la derecha →
  `dentro()`/`lado_x()` en `esquema.c`; prefijos p…T en la tabla; la línea
  Req/I/P se parte en dos si no cabe.
- `num_fraccion` podía desbordar el `long` de 32 bits de la calculadora →
  cuenta en `double` y numerador ≤ 999 999.
- Valores de más de 10¹⁵ salían como "inf" → `num_sig` escribe `1E21`; el
  esquema usa prefijos de n a G (`1GV`, `1mΩ`, `43nA`).

### Etapa 0 de la hoja de ruta de CA/CC (2026-09-24)

Plan aprobado por el usuario: `HOJA_DE_RUTA_CA.md`, hoy [HOJA_DE_RUTA.md](HOJA_DE_RUTA.md) (etapas 0-6 hacia
Circuitos I y II; decisiones por omisión: eficaz, coseno y grados; teclas
con la letra de la etiqueta; alcance hasta trifásica equilibrada y
transformador ideal). Hecho en la Etapa 0:

- **Motor**: `algebra.c` elimina UNA vez `[A | b0 b1 .. b8]` (un término
  independiente por dato medido) y `sis_combinar_signos()` da cada una de las
  2^m combinaciones de signo sumando columnas ya reducidas, sin multiplicar y
  sin calcular la tolerancia si el rango es completo. Salto de ceros de la
  fila pivote y un solo `1/p`. `sis_init` ya no borra 100 KB por sistema.
  Resultado: las 439 pantallas de resultados/pasos/tablas idénticas byte a
  byte; el peor caso, ~120 000 operaciones (antes ~2^8 eliminaciones).
- **Diagnóstico nuevo** `SOL_R_CERO`: una R desconocida con corriente y sin
  tensión vale 0 (antes, "negativa" u OK con 1e-15 según el redondeo).
- **Contadores** `sis_ops()` y `sis_fuera()` (el arnés exige 0 índices fuera).
- **Banco de tiempos** (`banco.c`, tecla `T` en la Ayuda): mide en la
  calculadora con el RTC (`gfx_reloj_ms`), `esc` lo cancela; su peor caso
  (`banco_peor_caso`, red densa de 20 nodos) es el mismo que usa el arnés, que
  ahora estima la calculadora por operaciones (`NS_POR_OP`, a calibrar con el
  banco) y falla si el peor caso pasa de 0,5 s. (Desde la calibración del
  cierre, §19.8, el banco cuenta todo por vuelta; las cargas del emulador
  van fijas, las de la versión medida: `BN_SIM` 2 ms con h = 2 µs y `BN_NL`
  1 s con h = T/45; la CA es la media sin el punto de 1 kHz, y la línea
  «CA: N op. a X µs» calibra `CA_NS_OP`.)
- **Modo práctica** (ui.c): respuestas tapadas, corrección al 1 % con máscara
  de soluciones compatibles, paso a paso que se destapa paso a paso, lista de
  ejemplos en modo práctica.
- `main.c` lee todas las letras (lo necesita la Etapa 1).

Una revisión adversarial (3 revisores + verificadores) encontró 12 fallos,
todos confirmados y arreglados; entre ellos, respuestas mezcladas de
soluciones distintas, respuestas visibles en el detalle de los pasos, el
contador que no contaba la combinación de signos y un "peor caso" de 3 nodos.

### Etapa 1 de la hoja de ruta: cimientos y primeros componentes de CC

- **Tabla de tipos** (`tipos.c`): letra, tecla, nombre, color, polaridad,
  fuente, "ideal" (valor vacío = ideal, no incógnita), campos de la ficha
  con nombre, unidad y ayuda, y rango. Lo que es comportamiento (ley en el
  motor, símbolo, regla de pizarra) sigue en su módulo con un caso por tipo.
  Se introdujo sin cambio visible (capturas idénticas byte a byte).
- **Modelo y archivo, diseñados una vez**: `Mag` con ángulo; `Elemento` con
  `aux` (r interna, R de instrumento...), `variante` y `ref` (renumerada al
  borrar); `Circuito` con tierra, terminales A/B, convenio de CA y
  frecuencia. Formato **EFK3** (versión y bloques con longitud, para añadir
  campos sin romper); EFK1 se sigue leyendo (hoy el mismo formato lleva la
  marca `CESP` y ni EFK1 ni EFK3 se leen). Mis problemas enseña los
  archivos que no se pueden abrir, para borrarlos.
- **Entrada**: `shift`/`ctrl` pegajosos (o a la vez), prefijos p n µ m k M G
  con eco del valor, signo inicial donde se admite, ficha de N campos que se
  desplaza, paleta (tecla catálogo), teclas por la letra de la etiqueta
  (`W` cable, `ctrl` `Z` deshacer; `C` y `Z` solas avisan).
- **Fuente de corriente J**: ley `I = J`; `RED_J_ABIERTA`; el procedimiento
  usa una VISTA en la que la J es una fuente cuya "E" es la V de sus bornes y
  cuya "I" es J, con los textos cambiados (`V = J·Req`).
- **Voltímetro y amperímetro**: ideales (I = 0 / V = 0) o con R interna;
  `RED_FUENTE_AMP` (un amperímetro ideal pone una E en corto); explicados por
  Kirchhoff.
- **Fuente real (E, r)**: nodo interno en el motor (`col_int`); r buscada;
  r negativa = sin solución física. El procedimiento pone la r como una
  resistencia virtual `r1` en serie (VISTA con un nodo más) y añade el paso
  "Tensión en bornes: V = E - r·I".
- **Tierra** (`G`, fija la referencia) y **capa de potenciales**; **terminales
  A y B** (`T`) con `V(AB)` en el resultado.
- **Aprende**: lección 8 "Fuentes reales, J e instrumentos" con 7 ejemplos con
  clave (la clave `'r'` es la r interna).
- Arnés: 270 800 comprobaciones, 0 fallos: 300 redes al azar con J, 250 con
  instrumentos ideales (que no deben cambiar nada), 250 con fuente real, más
  casos cerrados (efecto de carga, R interna de un voltímetro, V en bornes).
  Fallo real encontrado por el arnés: con una J y una E el árbol ignoraba la
  J; ahora el árbol exige una sola fuente de cualquier tipo.

### Revisión adversarial de la Etapa 1 (2026-09-25)

Cuatro lentes (motor, interfaz, modelo, pruebas), cada hallazgo reproducido
por un verificador: 48 confirmados (unos 30 fallos distintos), 1 descartado
(«4k7» se rechaza con aviso: es una mejora, no un fallo). Arreglados todos:

- **Motor.** Una r buscada en una fuente sin corriente daba r = 0 como
  solución (ahora: «no lleva corriente: su r interna no se puede hallar»);
  E y r a la vez se dicen inseparables; una pila en corto (cable o
  amperímetro ideal) se resuelve con I = E/r en lugar de rechazarse; una J
  sin camino de vuelta (lazo abierto, voltímetro ideal en serie) se detecta
  con un union-find; sin datos, los mensajes ya no culpan a los datos; la r
  negativa tiene su propio mensaje. `sol_desconocidos()` es la regla única
  de incógnitas para el motor y para la línea de estado del editor, que
  antes no contaba la r buscada.
- **Procedimiento.** Kirchhoff escribe siempre la ley de una fuente real
  (con E si no se sabe), despeja E = V + r·I y da la r buscada; la V en
  bornes de una fuente ideal sirve de dato al árbol; con 32 elementos (no
  cabe la vista con la r) no se usa el árbol, que la daba por 0; la raíz de
  la vista real se llama `Rtot` y la potencia E·I es «de la fem», con la
  entregada y la perdida en r aparte; la J vacía se calcula en los pasos.
- **Interfaz.** La práctica calificaba la r contra la I y la pedía en A
  (ahora `valor_de()` y `unidad_q()` en un solo sitio); el signo de una E o J
  negativa se ve; V(AB) sale con tres cosas pedidas y con las letras de los
  nodos de los pasos; «ideal (∞)» en la tabla; la tierra solo en un punto
  del circuito; letras A/B encima de los rótulos; shift/ctrl no se arman en
  las pantallas animadas; las letras de la paleta funcionan; en la
  calculadora, shift/ctrl **mantenidos** con otra tecla ya no se la comen
  (main.c espera la tecla mientras el modificador sigue abajo).
- **Archivo.** ±Inf y valores fuera de rango (los de la tabla de tipos) no
  pasan `circ_valido`; el voltímetro declara 1e9 como la ficha.
- **Pruebas.** Registro de textos dibujados en el PC (`gfx_texto_visto`)
  para comprobar lo que se ve (eco, signo, V(AB), potenciales, r); EFK3
  fijado byte a byte y con bloques más largos; renumeración de `ref`;
  shift que se gasta y se anula; [25] con un caso por fallo. Cada arreglo
  se comprobó por mutación: al reintroducir el fallo, el arnés lo detecta.

## Estado de la implementación (2026-09-23)

El alcance se fijó con los nueve problemas del pizarrón (`circuitos_pizarron.pdf`):
una fuente, ramas en serie y en paralelo, y la incógnita en V, I o una R
metida en una rama. Hecho:

| Parte del diseño | Estado |
|---|---|
| Rejilla, editor «acción + flecha», formulario con dato / incógnita / sin dato | hecho (rejilla de **10×7**, no 11×7) |
| Motor lineal por tabla, datos en magnitud, diagnósticos (§6.2, §6.4, §6.6) | hecho — `resuelve.c` |
| Árbol serie-paralelo y procedimiento como en la pizarra (§7.1–7.2, solo ramas pasivas) | hecho — `arbol.c`, `proced.c` |
| Kirchhoff con números para lo que no es serie-paralelo | hecho, versión por nodos |
| Capas del esquema, tabla, pasos con resaltado | hecho — `esquema.c`, `ui.c` |
| Los nueve problemas como ejemplos y clave del arnés | hecho — `ejemplos.c`, `host_test.c` |
| Fracción verificada, símbolos R/E, escenarios, instrumentos, Thévenin, no lineal, mallas | **pendiente** |

Verificación: `sh build_host.sh` → 124 112 comprobaciones, 0 fallos (también
con `asan`). `.tns`: 95 KB (`sh build_tns.sh`).

La v1 de este documento partía de «resolver el triángulo V·I·R en un circuito
dibujado». La v2 parte del **propósito** y de ahí deriva hasta dónde se puede
estirar el programa sin salir de su tema.

---

> **Las secciones 0 a 16** son el diseño original de la v2 (la CC,
> 2026-09-23) y conservan sus cifras y sus nombres de entonces (la rejilla
> de 11 × 7, que se hizo de 10 × 7; `MAX_NODOS` 24, hoy 70; el nombre
> «Electroestatica»). Lo que se construyó después, y dónde cambió, está en
> los apartados con fecha de arriba y en las §17 a §25.

## 0. Propósito

> **Cualquier enunciado de corriente continua con fuentes y resistencias que
> pueda aparecer en un examen, dibujado tal cual en la calculadora, resuelto de
> forma exacta, explicado con el método que pida el curso y verificado.**

Quien lo usa es un estudiante con un enunciado delante. Eso fija seis
principios, y cada capacidad del documento sale de uno de ellos:

| # | Principio | Consecuencia de diseño |
|---|---|---|
| P1 | **La forma del circuito no importa.** Serie, paralelo, una fuente en paralelo dentro de una serie mayor, puentes, varias fuentes, ramas en corto… | El motor no puede depender de reconocer patrones: resuelve cualquier grafo. Los patrones solo se usan para **explicar**. |
| P2 | **El enunciado decide qué es dato y qué es incógnita**, no el programa. Lo mismo puede ser «halla I» o «halla la R que da esa I», o «la bombilla es de 60 W», o «al cerrar el interruptor la corriente se duplica». | El problema se modela como **parámetros desconocidos + condiciones** (§6.1), no como «circuito directo». |
| P3 | **La respuesta como la pide el profesor.** `4/3 A`, no `1,3333`; `5E/(3R)` si el enunciado es simbólico. | Resultados exactos (fracción verificada) y unidades simbólicas R y E (§6.5). |
| P4 | **El procedimiento en el método del curso.** Reducción, mallas, nodos, superposición, Thévenin. | Varios generadores de procedimiento sobre el mismo circuito, todos verificados contra el mismo oráculo (§7). |
| P5 | **Entender el circuito, no solo resolverlo.** Lo que más falla en clase es ver qué está en serie con qué. | Radiografía del circuito: nodos coloreados, expresión serie-paralelo, detección de cortos, puentes equilibrados y fuentes que se cargan (§5.3). |
| P6 | **Nunca mentir.** | Si hay dos soluciones se muestran las dos; si no hay, se dice por qué; todo resultado se comprueba con leyes que no se usaron para calcularlo. |

El tema se mantiene cerrado a propósito: **fuentes de tensión, resistencias y
lo que se construye con ellas** (fuente real, bombilla nominal, instrumentos,
interruptores). Quedan fuera los condensadores, la alterna y los elementos no
lineales (§15).

---

## 1. Tipos de problema y qué los resuelve

Esta es la tabla de la que sale el resto del documento: los tipos de enunciado
de un curso de corriente continua (bachillerato, primer año de universidad,
exámenes de ingreso) y la capacidad que cubre cada uno.

| # | Tipo de enunciado | Ejemplo | Capacidad |
|---|---|---|---|
| 1 | Ohm con un elemento | 12 V y 4 Ω → I | núcleo |
| 2 | Resistencia equivalente | serie, paralelo, mixto, **entre distintos pares de puntos** | terminales A-B (§4.4), reducción (§7.2) |
| 3 | Req de un puente o del cubo | no es serie-paralelo | Δ-Y (§7.2), Kirchhoff |
| 4 | Análisis completo con una fuente | todas las I, V, P | núcleo |
| 5 | Divisores | V en R2, I en R3 | núcleo + reglas de divisor (§7.1) |
| 6 | **Fuente dentro de la red** | una batería en paralelo con una R, y el conjunto en serie con otra fuente | reducción con ramas activas: Millman (§7.2) |
| 7 | Varias fuentes, baterías que se cargan | dos baterías en paralelo sobre una carga | mallas, nodos, superposición, Millman |
| 8 | **Inverso** | ¿qué R hace que I = 2 A? ¿qué E da V2 = 5 V? | parámetros desconocidos (§6.2) |
| 9 | Inverso con potencia | ¿qué R disipa 18 W? (dos soluciones) | camino no lineal (§6.3) |
| 10 | Req como condición | ¿qué R da Req = 10 Ω? | condición sobre un óhmetro (§6.3) |
| 11 | Fuente real (E, r) | tensión en bornes, corriente de corto, potencia máxima | fuente con `r` (§4.2), Thévenin |
| 12 | **E y r con dos mediciones** | con 4 Ω da 1,5 A; con 7 Ω da 1 A | escenarios con parámetros compartidos (§4.5) |
| 13 | Instrumentos ideales | ¿qué marca el voltímetro? el amperímetro marca 2 A → E | voltímetro y amperímetro como componentes (§4.2) |
| 14 | Instrumentos reales | error de inserción, shunt, resistencia multiplicadora | instrumento con resistencia interna |
| 15 | Puente de Wheatstone | Rx con el galvanómetro a cero | condición `I = 0` + detección de equilibrio |
| 16 | Bombillas nominales | 60 W/120 V y 100 W/120 V en serie: ¿cuál brilla más? | R desde valores nominales (§9.3) + ranking de potencias |
| 17 | Interruptores, antes y después | al cerrar S, ¿cuántas veces aumenta I? | escenarios + comparación exacta (`×3/2`) |
| 18 | Condición relativa entre estados | al cerrar S la corriente se duplica: halla R | dato «×2 respecto al escenario 1» (§4.5) |
| 19 | Potencial en puntos | V_A, V_AB, tierra en otro punto | capa de potenciales, tierra movible |
| 20 | Thévenin / Norton, máxima transferencia | R_L que maximiza P_L y cuánto vale | terminales A-B + gráfica P_L(R_L) (§8.5) |
| 21 | Enunciado simbólico | R, 2R, 3R y E: halla I en función de E y R | unidades simbólicas (§6.5) |
| 22 | Energía, carga y coste | 3 h encendida: kWh, Q = I·t | extras de resultados con un tiempo `t` |
| 23 | Rendimiento de una fuente | η = P_útil / P_total | extras de resultados |

Los tipos 1, 4 y 5 son la v1 del documento anterior. **Del 6 al 23 es lo que
añade estirar el propósito.**

---

## 2. Mapa de capacidades por niveles

No todo vale lo mismo. Se construye por niveles; **cada nivel deja una app
completa y útil**.

| Nivel | Capacidades | Cubre los tipos |
|---|---|---|
| **N1 · Núcleo** | editor de rejilla (R, E, cable), cualquier topología, datos e incógnitas en V, I, R, E, resultados exactos, diagnóstico, capas | 1, 2, 4, 5, 6, 7, 8, 19 |
| **N2 · Explicar** | procedimiento razonado con resaltado, reducción con fuentes, radiografía del circuito, nodos coloreados | todo lo de N1, explicado |
| **N3 · Enunciados reales** | fuente real, instrumentos, interruptores, escenarios, terminales A-B (Req, Thévenin), potencia como dato, condiciones relativas | 3, 9–18, 20 |
| **N4 · Método del curso** | mallas, nodos, superposición, Thévenin como procedimientos elegibles | presentación de 7, 20 |
| **N5 · Estudio** | símbolos R y E, expresiones en los campos, valores nominales, explorador, gráfica, modo práctica, ejemplos, guardar | 16, 21, 22, 23 |

---

## 3. Cómo se hace software para la Nspire (lo que se hereda)

EstaticaCX dejó resuelto el «cómo». Resumen de lo que se copia y por qué.

- **ARM926EJ-S sin FPU, 396 MHz.** Todo `double` es soft-float: nunca dentro
  de un bucle de rasterizado; primitivas en enteros y seno en punto fijo.
- **320×240 RGB565.** Backbuffer propio de 150 KB en `.bss` → `lcd_blit()`.
  Sin `lcd_deinit()` en este SDK: se guarda `lcd_type()` y se restaura.
- **Sin `printf`** (decenas de KB con float) → `numfmt.c`. **Sin `malloc`**:
  todo estático y acotado, cada tope marca desborde.
- **Doble destino con `-DNDLESS`.** `main.c` es lo único que lee el teclado
  (a teclas abstractas); `gfx.c` lo único que toca el LCD. El resto se conduce
  desde el PC con un guion de teclas y se vuelca a BMP: **todo se depura sin
  calculadora**.
- **Cadena:** `nspire-gcc -Os -marm -DNDLESS` → `nspire-ld` → `genzehn` →
  `make-prg` → `.tns`. SDK en WSL bajo `/opt/Ndless`. Arnés con
  `sh build_host.sh` (`-Wall -Wextra -Wshadow -Wconversion`, variante `asan`).
  `eol=lf` obligatorio.

| De EstaticaCX | Destino | Cambios |
|---|---|---|
| `gfx`, `font6x8`, `font.h` | copia | paleta de circuitos; glifos **Ω ∥ ⏚ ≈ √** si falta alguno |
| `numfmt` | copia | `num_ing()` (prefijos k M m µ), `num_frac()` (fracciones) |
| `algebra` | copia | topes mayores; exponer columnas libres y rango |
| `pasos` | copia | `Paso` gana máscara de elementos, regla y escenario |
| `main.c`, `Makefile`, `build_host.sh`, `.gitattributes` | copia | teclas de letra, `=`, `?!`, `EE`, `ctrl` |
| `grafica` | idea | nuevo `curva.c` para barridos (§8.5) |
| `escena`, `dcl`, `estatica`, `ficha`, `catalogo*`, `viga` | **no** | el modelo es otro |

Se **copia** (no se comparte como biblioteca), anotando en cada cabecera el
commit de origen de EstaticaCX.

---

## 4. Modelo de datos

### 4.1 La rejilla

El circuito se dibuja sobre una rejilla de **11 × 7 puntos** (paso 28 × 26 px,
cabe entera en pantalla sin desplazamiento). Cada componente o cable ocupa un
segmento entre dos puntos vecinos.

```c
#define REJ_COLS  11
#define REJ_FILAS  7
typedef struct {
    unsigned char h[REJ_FILAS][REJ_COLS - 1];   /* 0 vacío · SEG_CABLE · SEG_EL0+k */
    unsigned char v[REJ_FILAS - 1][REJ_COLS];
} Rejilla;                                        /* 136 B: se guarda tal cual */
```

Por qué rejilla (y no una lista de nodos tipo SPICE):

- **Se dibuja como en el enunciado.** Si el libro pone la fuente en paralelo
  dentro de una serie mayor, se dibuja así; no hay que traducirlo a nodos a
  mano, que es justo el paso donde el estudiante se equivoca.
- **El dibujo sale limpio siempre**: no hace falta un algoritmo de disposición
  automática de grafos, que en 320×240 saldría ilegible.
- **Es plano por construcción**: las mallas son las caras del grafo (§7.3).

Convenio: **todo punto donde se juntan segmentos es una unión**. Para cruzar
sin conectar, se rodea.

### 4.2 Componentes

| Tipo | Tecla | Magnitudes | Qué es para el motor |
|---|---|---|---|
| Resistencia | `R` | R, V, I, P | `V = R·I` |
| Fuente | `E` | E, r, V_bornes, I, P | fuente ideal + `r` en serie (r = 0 por defecto) |
| Cable | `C` | — | fusiona puntos |
| Interruptor | `S` | estado por escenario | cable o circuito abierto según escenario |
| Voltímetro | `V` | lectura, R_V (∞ por defecto) | circuito abierto (o R_V) cuya V es la lectura |
| Amperímetro | `A` | lectura, R_A (0 por defecto) | cable (o R_A) cuya I es la lectura |

Los instrumentos **no son componentes nuevos para el motor**: son un cable o un
abierto con una magnitud marcada. Pero son la forma natural de escribir datos
e incógnitas que no son de un elemento («el voltímetro entre A y C marca 6 V»),
y detectan el mal uso:

- amperímetro en paralelo con un elemento → «A1 cortocircuita a R2»,
- voltímetro en serie → «V1 corta el circuito: casi no circula corriente».

Con R_V y R_A finitas se resuelven los problemas de error de inserción, shunt y
resistencia multiplicadora (tipo 14) sin nada más.

### 4.3 Magnitudes con estado

```c
typedef enum {
    M_LIBRE = 0,  /* no se sabe ni se pide: se calcula y va a la tabla         */
    M_DATO,       /* valor conocido                                            */
    M_BUSCAR,     /* incógnita pedida: sale destacada                          */
    M_RELATIVO    /* dato «×k respecto al escenario 1» (solo escenarios ≥ 2)   */
} EstadoMag;

typedef struct {
    double    v;            /* valor en unidades SI                            */
    Racional  q;            /* el mismo valor exacto, si lo es (§6.5)          */
    unsigned char simb;     /* 0, o múltiplo de la unidad simbólica R o E      */
    EstadoMag est;
} Mag;
```

### 4.4 Terminales A-B

Tecla `T` sobre un punto: marca los terminales **A** y **B** (como mucho un
par). Con terminales, los resultados ganan: Req entre A y B (fuentes
anuladas), V_AB, equivalente de Thévenin y Norton, R de carga para potencia
máxima y cuánto vale. Req_AB también puede ser **dato** («la resistencia entre
A y B es 10 Ω»): el tipo 10.

### 4.5 Escenarios

Un circuito tiene hasta **3 escenarios** (teclas `1`–`3` en el editor). Lo que
cambia entre escenarios:

- el estado de cada interruptor,
- los valores que se **sobrescriban** en ese escenario (la R de carga vale 4 Ω
  en el 1 y 7 Ω en el 2),
- los datos e incógnitas de V, I, P y lecturas.

Lo que no se sobrescribe **es el mismo parámetro en todos los escenarios**: si
E y r son desconocidas y no se sobrescriben, el motor busca *una* E y *una* r
que cumplan las condiciones de todos. Así se resuelve el tipo 12:

| | R_L | I | incógnitas compartidas |
|---|---|---|---|
| escenario 1 | 4 Ω | 1,5 A | E, r |
| escenario 2 | 7 Ω | 1 A | E, r |

→ `1,5·(4 + r) = 1·(7 + r)` → **r = 2 Ω, E = 9 V**.

Y el tipo 18: en el escenario 2 la corriente se escribe como **`×2`**
(`M_RELATIVO`), y la R desconocida se busca para que se cumpla.

### 4.6 Convenios de signo

- **Fuente**: el extremo `b` es el borne **+**; `E = φ₊ − φ₋`. Corriente de
  referencia saliendo por el +: `I > 0` ⇒ entrega `P = E·I`; `I < 0` ⇒
  **se está cargando**, y se dice.
- **Resistencia**: criterio pasivo, `V = R·I`, R > 0.
- **Referencia 0 V**: el borne − de la primera fuente, o donde se ponga ⏚.
- **Una I o V resuelta negativa** se dibuja con la flecha invertida y su valor
  absoluto (como las fuerzas en EstaticaCX).
- **Los datos medidos se escriben en magnitud** («por R2 pasan 2 A»); el signo
  lo decide la física (§6.4). La fem lleva el signo de su polaridad dibujada.

---

## 5. Del dibujo a la red — `nodos.c` y `analisis.c`

### 5.1 Extracción

Union-find sobre los puntos: cables (e interruptores cerrados, amperímetros
ideales) unen puntos. Cada conjunto que toca un borne es un **nodo
eléctrico**, nombrado `A, B, C…` en orden de lectura; con terminales, A y B
son los nodos de los terminales y los demás siguen, en orden, con las letras
libres (`red_letra`/`red_puesto`): V(AB) = VA − VB, y ninguna letra de nodo
repite la de un borne. Se extrae **una
`Netlist` por escenario** (un interruptor cambia los nodos).

### 5.2 Validaciones con mensaje

| Situación | Mensaje |
|---|---|
| fuente con los dos bornes en el mismo nodo | «E1 está en cortocircuito» |
| elemento con los dos bornes en el mismo nodo | «R3 está cortocircuitada: no circula corriente por ella» (aviso) |
| borne al aire | «R4 tiene un extremo sin conectar» (aviso) |
| subcircuito sin fuente | «Esta parte no tiene fuente: todas sus corrientes son 0» (aviso) |
| fuentes ideales en paralelo con distinta E | «E1 y E2 imponen tensiones distintas al mismo par de nodos» |
| instrumento mal conectado | §4.2 |

### 5.3 Radiografía del circuito (P5)

Pestaña **ANÁLISIS**, disponible antes de resolver:

- **Nodos coloreados**: cada nodo eléctrico en un color. Es el truco que se
  enseña para ver serie y paralelo en un dibujo retorcido, y aquí se ve de
  golpe.
- **Cuentas**: nodos `n`, ramas `b`, mallas independientes `b − n + 1`,
  ecuaciones de nodo `n − 1`.
- **Expresión serie-paralelo** vista desde la fuente o desde A-B:
  `R1 + R2 ∥ (R3 + R4)`. Si la red no es serie-paralelo, lo dice y nombra el
  puente.
- **Detecciones**: elementos en corto, ramas abiertas, puente equilibrado
  («por R5 no pasa corriente: se puede quitar»), fuente ideal en paralelo con
  una R («R2 no influye en el resto del circuito»), fuentes que se cargan
  (después de resolver).
- **Balance de datos**: «2 parámetros desconocidos, 2 condiciones: el problema
  está bien planteado».

---

## 6. Motor de cálculo — `problema.c`, `lineal.c`, `nolineal.c`

### 6.1 El modelo del problema: parámetros + condiciones

Todo enunciado del §1 se reduce a lo mismo:

- **Parámetros**: los valores que definen el circuito — cada R, E, r, R_V, R_A
  (por escenario si se sobrescribe). Unos son dato y otros desconocidos.
- **Condiciones**: todo lo demás que el enunciado afirma — una V, una I, una
  P, una lectura, una Req_AB, un «×2».

Con **todos los parámetros conocidos, el circuito es lineal** y se resuelve
con un solo sistema. Lo difícil es cuando hay parámetros desconocidos. La
regla de conteo, que la app enseña, es:

```
bien planteado  ⇔  nº de condiciones  =  nº de parámetros desconocidos
```

### 6.2 Camino lineal (exacto)

Si hay **un solo escenario** y las condiciones son **V o I** (no P, no Req, no
relativas), el problema entero es lineal aunque haya R desconocidas:

> Si la R de un elemento es desconocida, **no se escribe su ecuación de Ohm**.
> Su V y su I siguen siendo incógnitas; la condición del enunciado ocupa el
> lugar de esa ecuación. Resuelto el sistema, `R = V/I`.

Análisis por tabla: incógnitas `φ` de cada nodo (salvo la referencia) e `I` de
cada elemento; ecuaciones de nodo, de Ohm (R conocida), de fuente (E conocida)
y una por condición. Gauss-Jordan con pivoteo y **su diagnóstico** (heredado).
Cubre los tipos 1–8, 13, 15, 19.

### 6.3 Camino no lineal

Para todo lo demás —potencia como dato, Req como dato, escenarios con
parámetros compartidos, condiciones relativas— el problema se reduce al
**espacio de parámetros**:

```
buscar p (k parámetros desconocidos) tal que  c(p) = 0      (k condiciones)
donde evaluar c(p) = resolver el circuito LINEAL con esos parámetros
                     (una vez por escenario, más un circuito auxiliar por Req)
```

- **Las R se buscan en `u = ln R`**: R > 0 queda garantizado por construcción,
  y una escala logarítmica recorre de mΩ a MΩ con los mismos pasos.
- **k = 1** (el caso más común): barrido de `u` en [ln 10⁻³, ln 10⁹] con 120
  muestras, se localizan **todos** los cambios de signo y se refina cada uno
  (Brent). Encuentra todas las raíces del rango, no solo una: en «¿qué R
  disipa 18 W?» salen las dos.
- **k ≥ 2**: Newton amortiguado con jacobiano por diferencias (k + 1
  resoluciones por iteración), desde una malla de arranques (hasta 64); se
  agrupan las soluciones repetidas y cada una se verifica.
- Las E desconocidas entran lineales (superposición): Newton las resuelve en
  una iteración.

Coste en la calculadora: una red de 30 incógnitas son ~9000 operaciones de
coma flotante (~2–3 ms en soft-float). El barrido de k = 1 con 2 escenarios
queda por debajo de 1 s. Si hiciera falta, cambiar una sola R es una
modificación de **rango 1** de la matriz (Sherman–Morrison): cada muestra
extra costaría O(n²) en vez de O(n³).

### 6.4 Datos en magnitud: el signo lo decide la física

Con `m` datos en magnitud hay 2^m combinaciones de signo (`m ≤ 6`). En el
camino lineal se resuelven todas; en el no lineal el signo es parte de la
búsqueda. Filtro:

1. **Obligatorio**: toda R desconocida > 0 (en el camino no lineal ya lo es
   por `u = ln R`).
2. **Preferente**: toda E desconocida ≥ 0 con la polaridad dibujada.

| Quedan | Se muestra |
|---|---|
| una | la solución |
| varias | **todas**, numeradas, con su sentido de corriente |
| ninguna | por qué: «R3 saldría negativa: con estos datos no hay circuito posible» |

### 6.5 Exactitud: fracción verificada y unidades simbólicas (P3)

**Fracción verificada.** Hacer Gauss en racionales con enteros de 64 bits
desborda con valores como 4,7 kΩ en redes medianas. En su lugar:

1. se resuelve en `double`;
2. cada valor se convierte en su fracción candidata por fracciones continuas
   (denominador ≤ 10⁶);
3. se **comprueban exactamente**, en racionales, todas las ecuaciones de la
   red: nodo, Ohm, fuente, condiciones.

Cada ecuación tiene pocos términos (Ohm tiene tres, un nodo tantos como
elementos toca), así que los racionales no desbordan. **Si todas se cumplen
exactamente, la fracción está demostrada**, no adivinada. Si no, se muestra
solo el decimal. Resultado: `I2 = 4/3 A ≈ 1,333 A`.

**Unidades simbólicas.** Un enunciado «R, 2R, 3R, E» se escribe tal cual: los
campos admiten `R`, `2R`, `R/2`, `E`, `3E/2`. El motor resuelve con R = 1 y
E = 1, y como un circuito resistivo es homogéneo, cada magnitud lleva su
dimensión:

| Magnitud | Resultado |
|---|---|
| resistencia | `q·R` |
| tensión | `q·E` |
| corriente | `q·E/R` |
| potencia | `q·E²/R` |

con `q` la fracción verificada: `I = 5E/(3R)`. Condición: todas las
resistencias simbólicas o todas numéricas, y lo mismo con las fuentes (si se
mezclan, se avisa y se pide el valor).

**Comparación entre escenarios, exacta**: «al cerrar S, I pasa de E/(3R) a
E/(2R): **×3/2**» (tipo 17).

### 6.6 Diagnóstico

| Situación | Mensaje |
|---|---|
| faltan condiciones | «Faltan datos: R3 y E1 desconocidas, 1 condición» |
| sobran y son coherentes | «Datos redundantes pero coherentes» (se acepta) |
| sobran y se contradicen | «Los datos se contradicen» + la condición con mayor residuo |
| condición que no aporta | «La medición de I2 no determina R3: R1 y R2 están en serie» (columna libre de Gauss o jacobiano de rango incompleto) |
| R desconocida sin corriente | «R4 no lleva corriente: su valor no afecta al circuito» |
| sin solución física | §6.4 |

---

## 7. Procedimientos — el método del curso (P4)

Cada generador toma la `Netlist` y la solución, y escribe `Pasos`. Todos se
verifican contra el motor del §6: **cada número que escribe un paso tiene que
coincidir con la solución** (tolerancia relativa 1e-9, o exacto si hay
fracción). En la pestaña PASOS, la tecla `tab` cambia de método.

```c
typedef struct {
    char titulo[PASO_TIT], detalle[PASO_TXT], resultado[PASO_TXT];
    unsigned int  resalta;   /* bit k = elemento k: el paso ES la demo         */
    unsigned char regla, tapa, escenario;
} Paso;
```

La máscara hace que el procedimiento sea también la demostración animada: el
esquema arriba con los elementos del paso resaltados, el texto abajo. **No se
escribe ninguna demo a mano.**

### 7.1 Razonado (por defecto)

Como resuelve un estudiante: aplica **una regla con una sola incógnita por
paso**, por prioridad, y vuelve a la primera cada vez que avanza.

| Regla | Condición | Paso |
|---|---|---|
| Triángulo VIR | resistencia con 2 de {V, I, R} | `V2 = I2·R2 = 1·6 = 6 V` + triángulo dibujado tapando la buscada |
| Potencia | 2 de {P, V, I, R} | `R = V²/P = 120²/60 = 240 Ω` |
| Serie | nodo con dos elementos | «R1 y E1 en serie: misma corriente» |
| Paralelo | mismo par de nodos | «R2 ∥ R3: misma tensión» |
| Paralelo con fuente ideal | R en paralelo con E | «R2 está a la tensión de E2: V2 = 6 V» |
| Nodo | todas las corrientes menos una | `I3 = I1 − I2 = 3 − 1 = 2 A` |
| Potencial | un extremo con potencial conocido | `V_B = V_A − V1 = 12 − 6 = 6 V` |
| Divisor | serie con V total conocida | `V2 = E·R2/(R1+R2)` |
| Reducción | atascado | §7.2 |
| Kirchhoff | sigue atascado | el sistema de lo que falta, con los números sustituidos |
| Comprobación | al final | una ley **no usada**: nodo o balance de potencia `24 = 8 + 32/3 + 4/3 + 4 ✓` |

### 7.2 Reducción, también con fuentes dentro

La reducción serie-paralelo clásica solo sabe de resistencias. Para cubrir el
tipo 6 —**la fuente dentro de la red**— la reducción trabaja con **ramas
activas** (E, R):

| Combinación | Resultado |
|---|---|
| serie de ramas | `E = Σ ±Ei`, `R = Σ Ri` |
| paralelo de ramas (Millman) | `E = Σ(Ei/Ri) / Σ(1/Ri)`, `R = 1 / Σ(1/Ri)` |
| fuente ideal en paralelo con ramas | fija la tensión; las ramas no influyen fuera |
| triángulo ↔ estrella (Δ-Y) | desbloquea puentes y el cubo cuando son pasivos |

Se reduce hasta un solo lazo (o dos nodos con ramas en paralelo), se resuelve
ahí y **se deshace la reducción** propagando hacia atrás con las reglas del
§7.1.

**Ejemplo: fuente en paralelo dentro de una serie mayor.**
E1 = 12 V en serie con R1 = 2 Ω y con un bloque B-C formado por R2 = 6 Ω en
paralelo con la rama (E2 = 6 V, + hacia B, en serie con R3 = 3 Ω).

| # | Resalta | Paso |
|---|---|---|
| 1 | todo | Estructura: `E1 + R1 + [R2 ∥ (E2 + R3)]` |
| 2 | R2 E2 R3 | Millman en B-C: `V_BC,0 = (0/6 + 6/3)/(1/6 + 1/3) = 4 V` |
| 3 | R2 R3 | `R_BC = 6 ∥ 3 = 2 Ω` |
| 4 | E1 R1 | Un solo lazo: `I = (12 − 4)/(2 + 2) = 2 A` |
| 5 | R2 E2 R3 | `V_BC = 4 + 2·2 = 8 V` |
| 6 | R2 | `I2 = 8/6 = 4/3 A` |
| 7 | R3 E2 | Nodo B: `I3 = 2 − 4/3 = 2/3 A` |
| 8 | E2 | **E2 absorbe** `6·2/3 = 4 W`: se está cargando |
| ✓ | todo | Balance: `E1 entrega 24 W = 8 + 32/3 + 4/3 + 4` |

Resultados exactos (`4/3`, `2/3`), una fuente que se carga detectada, y una
comprobación con una ley que no se usó.

### 7.3 Mallas

Sin fuentes de corriente **no existen supermallas**, y la rejilla es plana:
las mallas son las caras del grafo, que se encuentran recorriendo segmentos con
cuatro direcciones posibles. Pasos: elegir mallas (se dibujan con su flecha
circular), una ecuación por malla con los números sustituidos, sistema,
solución, corrientes de rama como diferencia de corrientes de malla.

### 7.4 Nodos

Tensiones de nodo con referencia en ⏚. Una fuente entre dos nodos que no son
la referencia forma un **supernodo**, y se explica como tal. Pasos: elegir
referencia, ecuación de cada nodo, sistema, corrientes de rama.

### 7.5 Superposición

Una fuente cada vez (las demás en corto, se dibujan así), cada subcircuito
resuelto por reducción, y suma con signo. Es la presentación natural del
tipo 7.

### 7.6 Thévenin / Norton

Con terminales A-B: `V_th` (A-B abierto), `R_th` (fuentes anuladas, por
reducción o Δ-Y), `I_N`, y la carga de máxima potencia `R_L = R_th`,
`P_max = V_th²/(4 R_th)`.

---

## 8. Presentación de resultados

### 8.1 Pestañas (dinámicas, como en EstaticaCX)

| Pestaña | Contenido |
|---|---|
| RESUMEN | las incógnitas pedidas, grandes, exacta + decimal + unidad; soluciones múltiples; avisos |
| ESQUEMA | el circuito con capas (§8.2) |
| TABLA | todas las magnitudes de todos los elementos, por escenario |
| ANÁLISIS | la radiografía del §5.3 |
| PASOS | procedimiento, método elegible con `tab` |
| COMPARAR | solo con escenarios: cada magnitud pedida en cada uno y su razón exacta |
| GRÁFICA | solo con un parámetro seleccionado (§8.5) |

### 8.2 Capas del esquema

En 320×240 no caben R, V, I y P de diez elementos a la vez. Izquierda/derecha
alterna capas: **Valores** (R, E) · **Corrientes** (flechas en el sentido real
+ I) · **Tensiones** · **Potenciales** (cables coloreados de azul a rojo según
φ: la caída en cada resistencia se *ve*) · **Nodos** (un color por nodo) ·
**Potencias** (y ranking: «R1 disipa más»).

### 8.3 Extras de resultados

Con un tiempo `t` opcional: energía `W = P·t` (J y kWh), carga `Q = I·t`,
coste con precio por kWh. Rendimiento de cada fuente `η = P_útil/P_total`.
Aviso si un elemento con potencia nominal la supera.

### 8.4 Explorador

En ESQUEMA, seleccionar un elemento y pulsar `+`/`−` cambia su valor (×1,1 o
serie E12) y **todo se recalcula al instante**. Es la forma de entender un
divisor o por qué la potencia en la carga tiene un máximo.

### 8.5 Gráfica

Elegir un parámetro (una R) y una magnitud (P de esa R, I de otra…): curva en
el rango, con el máximo marcado y cursor de lectura. El caso estrella es
`P_L(R_L)`, con el máximo justo en `R_L = R_th`.

### 8.6 Modo práctica

Los pasos se revelan de uno en uno («pista») y en RESUMEN se puede escribir la
respuesta propia antes de ver la del programa: «correcto», o «no: te sale
2,5 A y es 2 A». Para estudiar, no solo para comprobar.

---

## 9. Interfaz

### 9.1 Pantallas

```
P_INICIO → P_MENU ─┬─ Nuevo ─────► P_EDITOR ⇄ P_ELEMENTO (formulario)
                   ├─ Ejemplos ──►    │ [=]
                   └─ Guardados ─►    ▼
                                   P_RESULTADOS (pestañas §8.1)
```

### 9.2 Editor

```
  0 ┌───────────────────────────────────────────────────────┐
    │ Circuito 1 · esc 1/2            acción: RESISTENCIA    │
 14 ├───────────────────────────────────────────────────────┤
    │  A●──/\/\──●B──────●                                   │
    │   │  R1    │       │                                   │
    │  ═╪═ E1  /\/\ R2  ═╪═ E2                                │
    │   │        │      /\/\ R3                              │
    │   ●────────●C─────●   ⏚                                │
178 ├───────────────────────────────────────────────────────┤
    │ R2   R = 6 Ω    V = ?    I = —    P = —                │
    │ [tab]sig [enter]editar [del]+flecha borrar [=]resolver │
240 └───────────────────────────────────────────────────────┘
```

Gramática: **acción + dirección**. Una flecha sola mueve el cursor; una tecla
de acción seguida de flecha actúa sobre ese segmento y deja el cursor al otro
lado (`C → → → ↓` tira un cable largo).

| Tecla | Acción |
|---|---|
| `R` `E` `C` `S` `V` `A` + flecha | colocar resistencia, fuente (+ hacia la flecha), cable, interruptor, voltímetro, amperímetro |
| `del` + flecha | borrar segmento |
| `T` | marcar terminal A / B |
| `G` | poner la referencia ⏚ |
| `F` | invertir la fuente seleccionada |
| `1` `2` `3` | escenario |
| `enter` en un interruptor | abrir / cerrar en este escenario |
| `tab` / `enter` | seleccionar / editar componente |
| `=` | resolver |
| `menu` | resolver con método…, análisis, ejemplos, guardar, limpiar |

### 9.3 Formulario de un componente

Un campo por magnitud, cada uno con estado: dígitos → **dato**; `?!` →
**incógnita pedida**; `del` en vacío → **libre**; en escenario ≥ 2, `×` →
**relativo**, y `=` → **igual que el escenario 1**.

Lo que acepta un campo:

- **expresiones**: `2*3`, `1/(1/4+1/6)`, `220^2/100`, `√`,
- **prefijos**: `4.7k`, `330m`, `2M`, `15µ`, y `EE`,
- **símbolos**: `R`, `2R`, `R/3`, `E`, `3E/2` (§6.5),
- **R desde valores nominales** (`ctrl`+`N` en el campo R): se piden P y V
  nominales y se escribe `R = V²/P`, conservando la potencia nominal para el
  aviso de sobrecarga (tipo 16).

### 9.4 Guardar y ejemplos

Circuitos en `/documents/Electroestatica/*.cir.tns` (la extensión `.tns` es la
que muestra el explorador de la Nspire). Formato: rejilla + componentes +
escenarios, unos 2 KB. Los **ejemplos precargados** son los casos del arnés
(§10): uno por tipo del §1, con su enunciado. Sirven de plantilla, de
catálogo de lo que la app sabe hacer, y de prueba.

---

## 10. Verificación — `host_test.c`

Principio heredado: no comparar contra números escritos a mano, sino contra
**propiedades que el resultado correcto cumple y que no aparecen en el camino
que lo calculó**.

| Prueba | Qué exige |
|---|---|
| **Residuo físico** | con la solución final: nodo, Ohm, fuente y cada condición, sin mirar el motor |
| **Balance de potencia** | Σ entregada = Σ disipada + Σ absorbida por fuentes |
| **Métodos contra motor** | cada número de cada paso de cada método = la solución |
| **Inverso por construcción** | circuito aleatorio resuelto en directo; se **ocultan** k parámetros y se **revelan** k condiciones (V, I, P, Req, relativas, en varios escenarios); el motor debe recuperar los ocultos. La respuesta la conoce el arnés por construcción |
| **Homogeneidad** | multiplicar todas las R por λ ⇒ I se divide por λ, V no cambia; todas las E por μ ⇒ I, V por μ, P por μ². Prueba la vía simbólica sin CAS |
| **Fracción verificada** | ninguna fracción mostrada que no cumpla exactamente todas las ecuaciones; y los decimales no representables no se muestran como fracción |
| **Todas las raíces** | casos con dos soluciones conocidas (potencia como dato): el barrido encuentra las dos |
| Barrido aleatorio | miles de redes: serie-paralelo generadas como árbol, puentes, mallas arbitrarias, fuentes en cualquier rama |
| Diagnósticos | casos plantados de cada fila del §5.2 y §6.6 |
| Clave del libro | un ejemplo por tipo del §1 con respuesta en forma cerrada |
| Interfaz | guion de teclas → BMP de cada pantalla; **cero** textos desbordados |
| Mutación | errores plantados (signo en un nodo, Millman sin normalizar, Δ-Y mal indexada, polaridad, flecha negativa sin invertir, raíz perdida…): el arnés los caza todos |

---

## 11. Estratos y archivos

```
  5  ui.c ─── bucle.c ─── entrada.c ──────────────── main.c  (solo aquí: libndls)
     │       (el bucle)  (la capa de entrada, §20: sin hardware)
  4  editor.c   resultados.c   ejemplos.c   archivo.c
     │
  3  razonado.c  reduccion.c  mallas.c  nodal.c  superpos.c  thevenin.c   (métodos)
     esquema.c   curva.c   analisis.c
     │
  2  lineal.c   nolineal.c   exacto.c                                     (motor)
     │
  1  circuito.c   nodos.c   problema.c   algebra.c   racional.c
     pasos.c   numfmt.c   expr_campo.c
     │
  0  gfx.c   font6x8.c
```

| Archivo | Responsabilidad |
|---|---|
| `circuito` | rejilla, componentes, escenarios, magnitudes; colocar, borrar, etiquetar |
| `nodos` | union-find → `Netlist` por escenario; validaciones |
| `problema` | parámetros + condiciones; conteo; elige camino lineal o no lineal |
| `lineal` | análisis por tabla + signos + diagnóstico. **El oráculo** |
| `nolineal` | espacio de parámetros: barrido k = 1, Newton multiarranque k ≥ 2 |
| `exacto`, `racional` | fracción verificada, unidades simbólicas |
| `analisis` | radiografía: nodos, cuentas, expresión serie-paralelo, detecciones |
| métodos | un archivo por método; todos escriben `Pasos` con máscara |
| `esquema` | único que dibuja el circuito; `Vista` = resaltado + capa + escenario |
| `curva` | gráfica de barrido |
| `expr_campo` | evaluador de los campos: expresiones, prefijos, símbolos |
| `archivo` | guardar y abrir (solo con `-DNDLESS` toca el sistema de archivos) |
| `entrada`, `bucle` | de la foto del teclado y del pad a las teclas de ui.h (flancos, repetición, deslizar), y el bucle con la plataforma inyectable (§20) |

Los métodos no se incluyen entre sí ni incluyen `lineal`: reciben la solución
ya calculada. Eso hace independiente la comparación del §10.

---

## 12. Topes, huella y rendimiento

| Constante | Valor |
|---|---|
| rejilla | 11 × 7 (136 segmentos) |
| `MAX_EL` | 32 componentes (la máscara `resalta` es de 32 bits) |
| `MAX_NODOS` | 24 |
| `MAX_ESC` | 3 escenarios |
| `MAX_PARAM` | 6 parámetros desconocidos |
| `MAX_UNK` / `MAX_EQ` | 64 / 72 (red de un escenario + condiciones) |
| `PASO_MAX` | 96 |

| Concepto | Estimación |
|---|---|
| `Sistema` | ≈ 38 KB estático |
| `Pasos` | ≈ 45 KB |
| framebuffer | 150 KB |
| `.tns` | 200–300 KB (EstaticaCX: 285 KB) |
| resolver directo | milisegundos |
| inverso no lineal, k = 1 | < 1 s |
| inverso no lineal, k = 2–3 | 1–3 s, con «calculando…» y cancelable con `esc` |

La CX II tiene 64 MB de RAM: la memoria no es el límite, lo es la pantalla.

---

## 13. Plan por bloques

| Bloque | Contenido | Entregable |
|---|---|---|
| 0 | Esqueleto de EstaticaCX, glifos, `num_ing`, teclas | portada en la calculadora |
| 1 | `circuito`, `nodos`, `esquema`, `editor` (R, E, cable) | dibujar circuitos, BMP en PC |
| 2 | `lineal` + signos + diagnóstico + RESUMEN/ESQUEMA/TABLA | **N1: resuelve cualquier topología, directa e inversa** |
| 3 | `exacto` + `racional` + `expr_campo` | fracciones y expresiones |
| 4 | `analisis` + `razonado` + `reduccion` con ramas activas + PASOS | **N2: explica** |
| 5 | fuente real, instrumentos, interruptores, escenarios, terminales, `nolineal` | **N3: enunciados reales** |
| 6 | `mallas`, `nodal`, `superpos`, `thevenin` | **N4: método del curso** |
| 7 | símbolos R y E, nominales, extras, COMPARAR | |
| 8 | explorador, `curva`, modo práctica, ejemplos, `archivo` | **N5** |

Al final del bloque 2 la app ya resuelve cualquier circuito fuente-resistencia
directo o inverso. Lo demás lo hace más útil, pero no es necesario para que
funcione.

---

## 14. Riesgos

| Riesgo | Mitigación |
|---|---|
| El no lineal no encuentra todas las soluciones con k ≥ 2 | k = 1 es exhaustivo; con k ≥ 2 se dice «se encontraron N soluciones» y cada una se verifica; el arnés mide la tasa de acierto con el inverso por construcción |
| Procedimientos poco naturales | prioridad de reglas ajustada con los ejemplos del §1 como prueba de oro |
| Etiquetas encimadas en 28 px | capas; en el editor solo la etiqueta, los valores van en el panel |
| Rejilla corta | 11×7 cubre los circuitos de examen; si no alcanza, rejilla lógica mayor con desplazamiento |
| Alcance | niveles: cada uno es una app completa; el bloque 2 ya cumple el propósito mínimo |
| Divergencia con EstaticaCX | copia con commit de origen; los arreglos de `gfx` se portan a mano |

---

## 15. Fuera de alcance (a propósito)

- **Fuentes de corriente como componente**: casi no aparecen en los cursos de
  este nivel; Norton sí aparece como resultado.
- **Condensadores, RC, alterna**: otra materia (¿otro módulo de la suite?).
- **Elementos no lineales** (diodos, bombillas cuya R cambia con la
  temperatura): rompen el principio de que con los parámetros fijos la red es
  lineal.
- **CAS general**: los símbolos se limitan a una unidad R y una E; eso cubre
  los enunciados simbólicos de examen sin meter `expr.c`.

---

## 16. Decisiones abiertas

1. **Nombre.** Lo que resuelve es electrocinética; «Electroestatica» encaja si
   es el nombre de una suite que luego tendrá Coulomb y condensadores.
2. **Orden de los niveles.** Propuesto N1 → N2 → N3 → N4 → N5. Si los
   exámenes son sobre todo de mallas o de nodos, N4 se adelanta a N3.
3. **Símbolos antes o después.** Si los enunciados que usas son mayormente
   simbólicos (estilo examen japonés: R, 2R, E), el bloque 7 se adelanta.
4. **Repo**: `git init` dentro de `Electroestatica/`, como EstaticaCX.

---

## 17. Etapa 2 — el emulador en el tiempo

La Etapa 2 convierte ElectroFock en un emulador con calculadora, al estilo de
NI Multisim: C, L, interruptores, fuentes con forma de onda, osciloscopio de
dos canales, multímetro y una biblioteca de circuitos modelo. La especificación
de trabajo fue `espec_e2.md` (juez sobre tres propuestas); aquí queda lo que
se construyó y dónde se apartó de ella.

### 17.1 Dos motores

- **Calculadora** (`resuelve.c`): la tabla de CC de siempre, que despeja
  incógnitas. Con los tipos nuevos, un C es un abierto, una L un cable y un S
  lo que es al empezar (`circ_abierto_cc`, `circ_corto_cc`). Una E o J que no
  es continua con un `[?!]` da `SOL_FORMA`.
- **Emulador** (`sim.c`): análisis nodal modificado (MNA) sobre su propia
  `SimRed`. Incógnitas: potenciales de nodo y la corriente de rama de lo que
  impone una tensión o tiene estado (E ideal, A ideal, C, L, S). LU densa
  (`lu.c`) con listas de no nulos: se factoriza una vez por topología y paso,
  y cada paso solo sustituye.
- `[=]` decide (`igual()` en `ui.c`): con `[?!]` calcula; si no y el circuito
  es dinámico (C, L, S, una forma no continua, una sonda, un multímetro en
  CA), emula; si no, calcula.
- Los dos motores se cruzan: el punto de operación del emulador es igual al de
  la tabla en 2000 redes al azar (sección [27] del arnés).

### 17.2 Punto de operación: reglas en vez de matrices singulares

`analiza()` recorre la topología con unión-búsqueda antes de montar la matriz,
y lo que la haría singular lo convierte en una regla física o en un error con
nombre:

| Caso | Qué hace |
|---|---|
| C en serie sin V0 | **CARGA**: la carga del nodo intermedio es cero (Σ C·v = 0) |
| L en paralelo sin I0 | **FLUJO**: el flujo del lazo es cero (Σ L·i = 0) |
| trozo sin referencia | **ANCLA** (FLOTA): su potencial se fija |
| J que carga un C sin V0 | rescate **C_DESC**: el C empieza descargado |
| E sobre una L sin I0 | rescate **L_SIN_I**: la L empieza sin corriente |
| V0 o I0 que no cuadran | **CI_SALTA**: el estado salta en 0⁺ (impulso) |
| lazo de E, A o S cerrados | error `SIM_LAZO_V`, con el lazo (`d.lazo`) y el instante |
| J sin camino de vuelta | error `SIM_CORTE_J`, con el corte |

Antes de correr se revisa la topología de cada instante en que actúa un S: un
S que al cerrarse cortocircuita una E sale como error **antes** del primer
paso, con su instante («En t = 2 ms, S1 cierra un lazo...»).

### 17.3 Transitorio

- **Trapecio** con modelos compañeros (C: `i − g·Δv`, g = 2C/h; L:
  `Δv − z·i`, z = 2L/h). El tiempo nunca se acumula: t = t_ancla + k·h.
- **Eventos exactos**: flancos, vértices, pulsos e interruptores. Si un
  evento cae entre dos puntos, el paso se parte con una LU desechable. La
  malla se alinea con el mcd de los periodos y de los instantes de los S, así
  que una cuadrada de 1 kHz no parte ningún paso. Antes de t = 0 cada fuente
  está en su reposo (el generador se enciende en 0).
- **Arranque tras cada evento.** Si ningún modo de la red (de ninguna de sus
  topologías) cumple h·|λ| > RIG_HL = 0,2, es decir, si todos tienen τ ≥ 5h,
  el arranque es el de siempre, bit a bit: **8 pasos de Euler implícito de
  h/8** (con su propia LU en la caché) y el trapecio desde te + h. Si hay un
  modo con τ < 5h, tras cada evento va un **tramo rígido** con **SDIRK2 de
  Alexander** (γ = 1 − √2/2):
  - es L-estable, de 2.º orden, rígidamente preciso y no necesita la
    derivada en te⁺ (tras un evento no se sabe; por eso no TR-BDF2, cuya
    primera etapa arrastraría la i_C de te⁻);
  - sus dos etapas son Euler implícitos con la misma LU (hh = 2γ·subpaso): la
    segunda parte de la historia v = vn + (1 + √2)(v1 − vn);
  - subpasos de h/16 hasta te + h y de h/4 hasta te + n·h, con
    n = ⌈10/(|λ|·h)⌉ del rígido más lento y 5 como mucho (RIG_SUB 16, RIG_SUB2
    4, RIG_NMAX 5; con 8 subpasos el banco [68] fallaba en 8 a 12 casos);
  - se registran las mismas muestras que en el arranque (y⁺, te + h/2,
    te + h, te + 2h…);
  - después sigue el trapecio con la historia de siempre (abajo), sobre el
    estado en te + (n − 1)h, te + (n − ½)h y te + nh para los algebraicos;
  - con no lineales, un Newton por etapa, y los primeros subpasos partidos
    hasta que z = s/τ ≤ 1 + √2 (s el trozo, τ la del modo más rápido del
    evento; §19.2): con z mayor la segunda etapa parte de más allá de la
    asíntota del modo (R(z) < 0), y un diodo que carga un C lo deja
    sobrecargado al cortarse.
  Por qué: el error de primer orden del Euler de h/8 en un modo de τ ~ h (del
  2,6 % del salto con τ = 0,3h en te + h) lo arrastra el trapecio con
  ρ = (1 − hλ/2)/(1 + hλ/2) → −1, y con τ/h entre 0,3 y 3 el propio trapecio
  desde te + h deja 1e-2 aunque arranque exacto (el hallazgo de [67] en la
  BJT 204: 3,7 % en la y⁺ y 5,7 % después). La historia del
  trapecio (i_C, v_L) depende del elemento:
  - un C o una L **con estado propio** se queda con la i_C (v_L) de la
    solución en te + h: es la derivada exacta de su estado, la que llevaría
    el trapecio, y no arrastra los modos que el paso no resuelve (una L de
    τ ≪ h ya está en su régimen en te + h);
  - un C **algebraico** (en un lazo de E ideales, A, S cerrados y C) o una
    L algebraica (en un corte de J, V, S abiertos y L) toma una **BDF2 con
    separación h/2** sobre el estado en te⁺, te + h/2 y te + h. Esa BDF2
    tiene el mismo sesgo que el trapecio (D − h²D³/12), así que el trapecio
    no arrastra oscilación alterna en la i_C de un C sobre una E ideal.
  Con la BDF2 para todos (lo de antes), una L de τ = h/25 quedaba oscilando
  un 2 % con factor −0,85 por paso (lo destapó la revisión, §17.10).
- **y⁺** (el valor justo después del evento, que se ve en el osciloscopio y
  cuenta en los máximos). En una red rígida es la **solución consistente en
  te⁺**: los C y las L con su estado de te⁻ y lo demás con las fuentes de
  te⁺. Sin impulso salen dos Euler implícitos de ε = 1e-3·τmín; el segundo
  parte de la historia 2s − v₁, lo que deja el error en O(ε²) ≈ 1e-6. Con un
  impulso salen cuatro: el primero reparte la carga (sus corrientes son las
  del impulso, ~ΔQ/ε: no son una muestra) y el segundo parte de lo repartido;
  los dos mueven un ε·f los estados que no saltan, así que el estado de te⁺
  es sJ = 2v₁ − v₂, y los dos últimos son los de antes con s = sJ. Con tres
  (lo de la primera versión) un C que no salta quedaba desplazado ε·i/C,
  1e-3 del salto (en «imp C», 5 mV); con cuatro, 2e-6. Si Newton no
  converge, se usa la extrapolación. En las redes no rígidas, la extrapolación
  cuadrática de los pasos 1-2-3 del arranque, o de los 2-3-4 si hubo impulso
  (el paso 1 lleva la corriente del impulso, que no es una muestra). La
  extrapolación no ve un modo que se apaga en el primer paso: en la RC de
  τ = h/100 daba 0,79 del salto.
- **Integrales en el tramo rígido** (`rq_etapa1`, `rq_suma`). W, la ventana
  (media, eficaz y P media) y las integrales de los canales van subpaso a
  subpaso, no por el trapecio entre las muestras registradas (la y⁺ exacta de
  un modo con τ ≪ h es solo una muestra: con el trapecio desde ella hasta
  te + h/2, una R de 1 Ω aparecía con 31 mJ y disipa 25 nJ):
  - las de V y de I, con los pesos del método, s·[(1 − γ)·f(etapa 1) +
    γ·f(etapa 2)] (un Euler entero, el de un trozo o el de `localiza_nl`,
    s·f(final)): así ∫I de un C es su carga, C·Δv;
  - las de un producto (V·I, V², I², y²), con la media de los dos extremos
    del subpaso por la integral del método: ∫V·I = V̄·∫I, V̄ = (V₀ + V)/2
    (V₀, el principio del subpaso; en el primero, la y⁺); el segundo puerto
    de AO, BJT y MOS, igual.
  Los extremos cumplen Kirchhoff de tensiones y las etapas el de corrientes:
  Tellegen se cumple y la suma de las W es exacta. En un C, W = C·V̄·Δv =
  Δ(½CV²): **sin L el balance es exacto** (6e-7 o menos de lo entregado en
  el banco). Y lo que disipa un modo que se apaga dentro de una etapa lo ve
  la y⁺: en [41] la R de 1 Ω lleva V̄·CΔV = ½CV². Con los pesos de las etapas
  también en los productos (la primera versión), esa R daba 2e-12 J y los
  25 nJ quedaban en «Diferencia: saltos y método», que no es un impulso
  físico con una fuente de Thévenin; con 47 flancos de τ ≈ h/13, la
  entregada salía un 53 % corta. Con L, W_L = L·Ī·Δi no es exactamente
  Δ(½LI²): la diferencia queda en el balance (≤ 2e-5 de lo entregado en las
  RL y RLC del banco y de la revisión, y 7e-4 en un RLC subamortiguado con
  10 pasos por periodo).
- **Impulsos**: un lazo de E, A, S cerrados y C que no cuadra en te⁺
  (capacitivo), o un corte con L, J, S abiertos o V ideales cuyas corrientes
  no suman cero (inductivo). Dan un `SimSuceso` y el aviso IMPULSO. El salto
  de energía queda en el balance.
- **¿Es t = 0 una discontinuidad?** Sí con una forma periódica, un pulso que
  empieza en 0, un S que actúa en 0, CI_SALTA, un C con i = 0 impuesta en un
  lazo de tensiones que cuadra (dos C con V0 en paralelo), **C/L libres
  junto a C/L con condición inicial que ya se mueven** (el punto de
  operación no es un régimen), **un trozo anclado** (FLOTA: la v_L que lo
  cruza es la del ancla, no una historia) o **C/L que se mueven con un modo
  que el paso no resuelve** (h > 2τ: el trapecio lo dejaría con signo
  alterno). Si no, el trapecio arranca del punto de operación: un LC con V0
  e I0 conserva la energía a 1e-12.
- **Con no lineales, el tramo lo decide la clase del evento** (la
  presonda, `presonda`, a priori en `elige_paso`; §19.2). Un modo rígido que
  solo existe después del evento (un diodo que empieza a conducir) no lo ven
  los modos del punto de operación. En una red con uniones, BJT, MOS o
  diodos por tramos y con C o L, los eventos de las fuentes y de los S se
  agrupan en clases (los S antes y después, el signo del salto de cada
  fuente y, en un vértice de una triangular, el de su pendiente nueva): se
  miran los 64 primeros y caben 8 clases. De la primera de cada clase, desde
  el estado del punto de operación y con las fuentes y los S de te⁺, un
  Newton del Euler de ε (sin aceptarlo) da la linealización en te⁺, y sus
  modos deciden el tramo de todos los eventos de la clase y la τ del ε de su
  y⁺. Así el tramo se sabe antes de emular y cuenta en los 2 s y en los topes
  (§17.4), y una red en que ninguna clase lleva tramo emula como antes, sin
  nada por evento. La primera versión miraba los modos en cada evento (la
  sonda): cuadraba mejor con el estado de cada uno, pero su costo (un Newton
  y los modos por evento, también en las redes no rígidas: +7 a +30 % de
  tiempo) y los tramos que decidía iban fuera de la estimación (4,4 s
  reales con 1,95 estimados; FIN_TOPE donde la base acababa).
- **Límites conocidos**:
  - **la banda 5h ≤ τ < 7,5h**: por debajo de RIG_HL no hay tramo y el
    arranque de Euler deja su error de primer orden tras el flanco: 2,0e-3 de
    la escala con τ = 5h, 1,45e-3 con 6h (el caso «RC tau/h=6» de [68]) y
    9,6e-4 con 7,5h, lo mismo que antes. Bajar el umbral a 0,13 la cerraría,
    pero las redes con todos sus modos de τ ≥ 5h tienen que quedar bit a bit.
    El error salta en el umbral (4,7e-4 con 4,9h) y la convergencia al
    refinar h deja de ser monótona cerca de él: en [67], el caso_225 da
    3,8e-4 con su h (tramo) y 5,3e-4 con h/4 (sin tramo);
  - **la clase se decide con el estado del punto de operación**, no con el
    de cada evento: un evento posterior de la misma clase cuyo estado lo
    haga rígido cuando la primera no lo era queda con el arranque de Euler;
    al revés (tramo de más), solo cuesta. Los eventos de una clase que no
    cupo, y los de una red con más de 64 eventos cuya clase no salió entre
    los primeros, van con el tramo de la red;
  - en un **cambio de tramo** de un diodo por tramos o de un AO (no saltan
    las fuentes) no hay presonda: el tramo es el de la red, como antes (no
    se sabe a priori cuántos habrá, y la estimación tampoco los cuenta en el
    arranque). Un rectificador con un C pequeño, diodo por tramos y seno
    sigue igual que antes al encender el diodo: la y⁺ extrapolada mueve la
    tensión del C (11,46 → 9,73 mV) y en te + h/2 la I(C) queda a 1,8e-2 de
    la escala; con cuadrada (flancos de la fuente) sí se arregla (de 0,25 a
    5e-4 o menos);
  - **un punto a mitad del primer h** (un t_fin o un evento que corta el
    tramo) tiene el error de SDIRK2 con h/16, que solo se mide en los puntos
    registrados (te + h/2, te + h…: ≤ 1e-3 del salto): en «sq_1.3h» (τ =
    h/13) el t_fin en te + 1 µs queda a 2,0e-3 de la escala; con τ ≪ h/16
    (z = 6 a 10) R(z) ≈ −0,2 y un corte justo en te + h/16 lleva 0,2 del
    salto, que se apaga a 1e-3 en te + 5h/16. Un salto de dos veces la
    escala (el vértice de una triangular, el astable con un RC en la salida)
    da 1,1e-3 de la escala en te + h/2 (5,6e-4 del salto);
  - **el balance con L** en el tramo no es exacto (arriba).

### 17.4 Modos, paso y ventana

- **Modos** por topología: se toma el mapa de un paso de Euler implícito con
  un paso de prueba h0 (μ = 1/(1 − h0·s/2)), Hessenberg + QR de Francis, y
  s = (2/h0)(1 − 1/μ). Se descartan los algebraicos (|μ| ≈ 0) y los
  constantes (s ≈ 0: la carga entre C en serie, el flujo de un lazo de L).
  Si h0 queda lejos de la escala de los modos se repite con uno a medida.
- **Ventana**: último evento único + 5·τ máx, al menos 3 periodos de las
  fuentes y 5 de las oscilaciones sin amortiguar; redondeada a 1-2-5. Sin nada
  que varíe: 10 ms y el aviso QUIETO.
- **Paso**: t_fin/2000; T/100 de una senoidal; **T/50 de una cuadrada,
  triangular o pulso** (son rectas a trozos y sus flancos caen en eventos);
  τ/10 y T_osc/50 de los modos visibles; la separación entre eventos / 4;
  alineado con la malla de los eventos.
- **Costo**: cada paso cuesta la sustitución más ~15 operaciones por elemento;
  cada evento, 8 pasos de arranque. Con 0,6337 µs por operación y 73,8 µs
  fijos por paso (`SIM_NS_OP`, `SIM_NS_PASO` en sim.h, medidos con
  `BN_SIM`; con solo tramos, cada paso suma también `NS_ITER`, una iteración
  de `newton()`), si el paso es automático y la
  estimación pasa de 2 s, el paso crece (aviso PASO_LIM). Topes duros: 20 000
  pasos y 24 576 muestras.
- **El tramo rígido cuenta** en la estimación de los 2 s y en los topes de
  pasos y muestras (`pasos_caben`), evento a evento con el tramo de su clase
  (§17.3; sin clases, el de la red): 16 + 4(n − 1) subpasos, cada uno un
  paso más su segunda etapa (la sustitución y el término; con suaves, otro
  Newton y su tiempo fijo) y la integral (la etapa 1 ligera y `rq_suma`, con
  la parte de los eventos que cae en la ventana de medida); con no lineales,
  los trozos del principio (los que hacen z ≤ 1 + √2); la y⁺ (2 a 4
  sustituciones de ε; con no lineales, sus Newton) y hasta 4 LU por
  topología (γh/8, γh/2 y ε, en la caché, que pasa a 8 entradas); se
  descuentan los n pasos h que el tramo cubre. La presonda (una vez) cuenta
  si alguna clase lleva tramo; si ninguna, se suma a t_est pero no mueve el
  paso, y la red emula como antes. Lo que la estimación no ve, como antes:
  los subpasos del Newton del trapecio con suaves (nivel), que con un diodo
  crecen con h.
- **El bucle del paso** (`elige_paso`): con el tramo el costo de un evento no
  crece con h (salta al cruzar el umbral de un modo y baja luego, al acortarse
  su tramo de 5 pasos h). Al agrandar h, se para antes en el primer h donde
  un evento cuesta menos (`paso_mas_barato`); si ni con 200 pasos cabe, se
  prueba un h mayor con el tramo más corto y, si el paso llevaba tramo, uno
  menor que el pedido en que ningún evento lo lleva (`paso_sin_tramo`); si
  nada cabe, la ventana baja (también la pedida por el usuario, como siempre:
  «ni con 200 pasos») y la búsqueda vuelve a empezar desde el h de antes, no
  desde el que llevaba tramo. Sin tramo es el bucle de siempre (bit a bit).
  La primera versión solo sabía agrandar h o acortar la ventana: al cruzar
  el umbral el costo saltaba y la ventana bajaba de más (el diodo NR con
  300 ms pedidos, a 20 ms; ahora 100 ms) y un h a mano de 2 µs subía a 50 µs
  por el tope de pasos (ahora 10 µs, y acaba; la base acababa en FIN_TOPE).
  Con muchos eventos el paso crece (PASO_LIM) o la ventana baja: el peor
  caso de [36] pasa de 5 a 2 ms (estimado 2,00 s; lo real por las
  operaciones, 2,17 s, x1,09; con el arranque de Euler, 1,96 s estimados y 2,52
  reales, x1,28) y la BJT 204 de h = 23 a 38 µs.

### 17.5 Medidas, lecturas y balance

- Por elemento: V, I y P al final, sus máximos con signo, la energía W
  (absorbida por los pasivos, entregada por las fuentes) y, en la ventana de
  medida (los últimos periodos enteros, o el último 10 %), la media, el eficaz
  y la P media.
- **W por puntos medios** (V̄·Ī): con el trapecio, el balance entregada =
  disipada + Δalmacenada es exacto a 1e-12 (Tellegen en los puntos medios). La
  **P media** se integra por el trapecio de V·I, así que en una R es Vef²/R
  exacto. Eso vale entre muestras del trapecio; dentro del tramo rígido,
  subpaso a subpaso, la media de los extremos por la integral del método
  (§17.3), también para la ventana y los canales: sin L el balance sigue
  exacto y en una R la P media sigue siendo Ief²·R.
- El **multímetro** marca, según su modo: CC = la media; CA = el eficaz de la
  parte alterna (como Multisim); CA+CC = el eficaz total.

### 17.6 Pantallas

- `P_EMULANDO`: barra de progreso; `sim_avanzar(150 000 operaciones)` por
  tick (~95 ms en la calculadora); `esc` corta y enseña lo hecho.
- `P_OSC` (`osc.c`): retícula 10×8, 300 columnas de mínimo/máximo (la traza es
  continua y un pico de una muestra se ve), cursores con escritura de tiempos,
  picos con refinado parabólico, disparo por flanco con histéresis, autoescala
  y medidas. Los cruces del disparo se afinan con una cúbica de cuatro muestras.
- `P_EMU_TABLA` (5 páginas), `P_EMU_ESQ` (esquema con los valores al final,
  las sondas, las lecturas y el primer aviso; con un error, el lazo resaltado)
  y `P_AJUSTES` (tiempo total y paso máximo).
- Sondas con `[O]`: nodo, I o V de un elemento; se guardan en el archivo.
- Glifos nuevos a mano en las dos fuentes: τ (0x9E) y ζ (0x9F).

### 17.7 Desviaciones de la especificación

| Especificación | Hecho | Por qué |
|---|---|---|
| arranque CDA4: 4 medios pasos de Euler, BDF2 con h/2 | CDA8: 8 pasos de h/8 (su LU); historia: la solución en te+h para los C/L con estado propio y BDF2 con separación h/2 sobre te⁺, te+h/2 y te+h para los algebraicos | el error de primer orden de 4 medios pasos era del 0,5 % en un derivador (τ = 10h); con h/8 baja 16 veces. La BDF2 con h/2 conserva la coherencia con el trapecio en los algebraicos, pero no ve bien un modo rígido |
| y⁺ = 2y(h/2) − y(h) | extrapolación cuadrática | la lineal dejaba un error x² (0,2 % en el derivador); la cuadrática, x³ |
| `factorizaciones` = 1 en la RC con cuadrada | 2 | la LU del trapecio y la del arranque |
| h ≤ T/100 de toda fuente periódica | T/100 senoidal, T/50 el resto | el integrador de la biblioteca pasaba de 0,5 s |
| tope de 2 s solo con t_fin y h automáticos | con h automático | con un t_fin del usuario el emulador podía tardar sin límite |
| `mask_de` exacta | tolerancia 1e-9·h | dos S que actúan «a la vez» con un ulp de diferencia perdían uno (lo encontró el oráculo) |
| tabla del RLC con R = 1000: 6,97e-4 / 1,72e-4 / 4,28e-5 | 1,227e-3 / 3,048e-4 / 7,559e-5 | el trapecio puro calculado aparte (`tr_rlc.py`) da estos; la fila de la especificación estaba mal |
| en t_fin, el límite por la derecha | por la izquierda | un flanco justo en t_fin daría el valor de después sin haberlo emulado |
| Tras cada evento, 8 pasos de Euler de h/8 (CDA8) | Con un modo de τ < 5h (h·\|λ\| > 0,2): tramo SDIRK2, con 16 subpasos hasta te + h y de h/4 hasta te + n·h (n ≤ 5), y luego el trapecio | El error de primer orden del Euler de h/8 en un modo de τ ~ h lo arrastra el trapecio, con ρ = (1 − hλ/2)/(1 + hλ/2) → −1 (hallazgo de [67]: 3,7 % en la BJT 204; en el caso mínimo, 2,6 % en la y⁺ y 6 % tras el flanco). Con el tramo: y⁺ ≤ 1,1e-4 y después ≤ 8,4e-4 ([68]: 16 de 16 rígidos, y los 9 no rígidos bit a bit). Entre 5h y 7,5h queda la banda del arranque (≤ 2,0e-3, como antes) |
| La y⁺ de una red rígida es aproximada (se extrapola) | La consistente en te⁺ (dos Euler de 1e-3·τmín; cuatro con impulso) | La extrapolación no ve un modo que se apaga en el primer paso: en la RC de τ = h/100, 0,79 del salto. Con impulso, con tres Euler un C que no salta quedaba desplazado ε·i/C (1e-3 del salto); con cuatro, 2e-6 |
| W, ventana y canales por el trapecio entre muestras | Dentro del tramo, subpaso a subpaso: V e I con los pesos del método, sus productos con la media de los extremos (el primero, la y⁺) por la integral del método | Con la y⁺ exacta y muestras cada h/2, el trapecio integraba un pico de ancho τ ≪ h como un triángulo de h/2 (una R de 1 Ω con 31 mJ, disipa 25 nJ). Con los pesos de las etapas también en los productos, esa R daba 0 y la entregada salía un 53 % corta con 47 flancos de τ ≈ h/13. Así, sin L, el balance es exacto y la R lleva sus 25 nJ |
| El paso y la ventana dependen solo de las fuentes y los modos | El tramo cuenta en la estimación de 2 s y en los topes, evento a evento con el de su clase; el bucle busca el h más barato antes de acortar la ventana | Sin contarlo, la BJT 204 costaba x1,51 y rompía el tope; el peor caso de [36] tardaría 5 s. Mantener el tope y acortar la ventana es la política de §17.4. Con la sonda de cada evento (la primera versión), sus tramos iban fuera: 4,4 s reales con 1,95 estimados, y FIN_TOPE donde la base acababa |
| Con no lineales, los modos del punto de operación | Los de la linealización en te⁺ de la clase de cada evento de las fuentes o de un S (la presonda, a priori) | El modo rígido de un diodo que empieza a conducir no está en el punto de operación: el «diodo C» de [68] quedaba con la y⁺ a 0,17 y lo que sigue a 0,04. A priori (y no en cada evento) para que cuente en la estimación y no cueste nada en una red sin tramos |
| SDIRK2 con el subpaso que toque | Con no lineales, los primeros subpasos partidos hasta z = s/τ ≤ 1 + √2 (y, si ni con h/16/64, un Euler entero mientras el modo no se apague) | Con z > 1 + √2 la segunda etapa sale de más allá de la asíntota; con un diodo que carga un C, el C quedaba sobrecargado y el diodo cortado: con τ = h/70, 0,24 de la escala en te + h/2 y 4e-2 tras cada flanco («diodo C z4.5» de [68]; ahora 3,0e-4) |
| `factorizaciones` = 2 en la RC con cuadrada | 2 si no es rígida; hasta 4 por topología si lo es | Las LU de h, γh/8, γh/2 y ε, en la caché |

### 17.8 Verificación

- **Arnés**: 310 395 comprobaciones, 0 fallos (normal, ASan y reglas del
  ARM). Secciones nuevas: [26] LU y ondas; [27] cruce de los dos motores;
  [28] y [34] transitorios contra soluciones exactas (el emulador **es** el
  trapecio: v_n = a·v_{n−1} + b·E a 1e-12; balance exacto en 274 redes RLC al
  azar; trozos de `sim_avanzar` iguales bit a bit); [33] reglas y errores del
  punto de operación; [35] eventos e impulsos (reparto de carga
  3,678794 → 0,919699 con 5,075 µJ perdidos; chispa; derivador; orden ≥ 1,8
  con eventos fuera de la malla); [36] modos (RLC en los tres regímenes para
  h0 de 1e-6 a 1) y paso y ventana automáticos; [37] osciloscopio con
  registros sintéticos; [38] interfaz por guion de teclas; [39] biblioteca
  (las 12 claves y < 0,5 s estimados cada una); [40] y [41] las pruebas de la revisión adversarial (§17.10).
- **Oráculo independiente** (`herramientas/oraculo_sim.py`, Python + SciPy:
  espacio de estados por topología, `solve_ivp` con rtol 1e-10). El arnés
  exporta 116 redes al azar (R, C, L, E y J con las 5 formas, S), a h y a h/2,
  con el estado inicial del emulador explícito. `compara_oraculo.py`: **92 de
  92 iguales** con tolerancia 1e-3 de la escala (peor 8,4e-4, en el primer
  instante de un transitorio rápido) y **92 de 92 con los mismos modos** a
  1e-9. El oráculo rechaza 24 (impulsos y condiciones iniciales incompatibles,
  que el emulador resuelve con un salto). Encontró dos defectos reales, ya
  arreglados: los interruptores simultáneos y el arranque desde un punto de
  operación que no era un régimen.
- **Mono**: 150 000 teclas por las 23 pantallas, sin desbordes.
- **El tramo rígido** (§17.3; después de la Etapa 4):
  - **[68], el banco del arranque tras un flanco** (`test_flanco.inc`, en la
    batería, en `HAKARU_SOLO_E4` y solo con `HAKARU_SOLO_FLANCO`): 25 casos con la
    exacta (analítica con mpmath en los lineales; el oráculo no lineal con
    Radau en los diodos y la BJT 204), `herramientas/banco_flanco.py`. Un caso
    es rígido si algún flanco tiene τ < 5h; ahí, la y⁺ y cada muestra de
    después, en todos los flancos, a 1e-3 de la escala, y pasos y operaciones
    ≤ x1,5 de los del arranque de Euler (FL_HOY; x2 en el «diodo C z4.5»,
    abajo); los no rígidos, bit a bit y ≤ x1,05 en operaciones. Con el
    arranque de Euler fallan los 16 rígidos (el peor, la y⁺ de la RC de
    τ = h/100 con 0,79 del salto; tras el flanco, hasta 6,3e-2); con el tramo
    cumplen los 16 (y⁺ ≤ 1,1e-4; después ≤ 8,4e-4; costo ≤ x1,39 en
    operaciones, la BJT 204 con h/2, y x1,81 el diodo de z = 4,5) y los 9 no
    rígidos quedan bit a bit (el diodo NR, x1,003 en operaciones: la
    presonda). Incluye las dos escaleras RC de 20 flancos del juez (14 y 8
    tramos, τ/h 0,06 y 0,58: el criterio, en todos los flancos) y el balance
    del mínimo, la RL, el RC de τ = h/100 y la escalera de r = 1 Ω a 2e-4 de
    lo entregado (7e-9, 4,5e-6, 4,5e-8 y 5,9e-7). Lo que añadió la revisión
    del arreglo: «RC tau/h=6» (no rígido: la banda, 1,5e-3, visible y bit a
    bit), «imp C» (la y⁺ con impulso: 2,0e-6, con una comprobación a 1e-5),
    el «diodo NR» y el «diodo C z4.5» (100 k de carga, h = 1/140000 s a mano:
    la y⁺ 1,1e-4, después 3,0e-4; la primera versión, 0,24 y 4e-2).
  - **Detrás de la tabla** (`test_flanco_rev`, las redes de la revisión, sin
    exacta): el diodo que carga 1 nF por 100 Ω con 100 k a 20 kHz y paso
    automático lleva un tramo en cada subida y ninguno en las bajadas, estima
    1,95 s y «lo real», con lo medido en la calculadora (operaciones·0,6337
    µs + pasos·73,8 µs + iteraciones·250,5 µs; 4,3 iteraciones por paso), es
    2,66 s ≤ 1,4 veces (con las constantes de antes de calibrar, 2,34 s ≤
    1,3; la sonda, 4,44 s; la base, 2,76 s); con
    h = 2 µs a mano y 26 ms acaba con el h pedido (19 760 pasos; la sonda,
    FIN_TOPE); con 10 k de carga, 30 ms y h = 2 µs, h sube a 10 µs (x5; la
    sonda, x25) y acaba; el diodo NR con 300 ms pedidos se queda en 20 ms
    sin tramos (antes de calibrar, 100 ms): 50 ms no caben ni con el mayor h
    sin tramo (31,25 µs a mano: 2,13 s); y un RC de τ = h/13 con 47 flancos entrega
    5,402e-6 J (exacta 5,3985e-6) con una diferencia de 5e-11 J (la primera
    versión, 2,56e-6 J y −53 %).
  - **Balance**: en [41], la red de un modo de 0,5 ns junto a uno de 1 s (C
    de 0,5 nF con R de 1 Ω, h = 2,5 ms): W(R de 1 Ω) = ½CV² = 25 nJ y la
    diferencia del balance, −5,5e-14 J (con el trapecio desde la y⁺ exacta,
    W = 31 mJ y un 19 % descuadrado; con los pesos de las etapas en los
    productos, W(R) = 2e-12 J y los 25 nJ en la diferencia).
  - **Oráculos**: en [29] (las redes de `HAKARU_SOLO_E2T`), 98 iguales y 1
    distinto (el 084, que no es rígido, 1,7e-3 como antes) de 99, y 17 que
    el oráculo rechaza; el 033, el 101 y el 102 bajan de 0,073, 0,012 y
    0,037 a 1,6e-6, 2,1e-6 y 2,0e-6 (10 cocientes h/(h/2) fuera de [3, 5]:
    ahí los dos errores están ya en 1e-6). La base: 95 y 4 distintos. En
    [67], 260 de 260 iguales sin excluir ninguno (peor 6,1e-4, el caso_240;
    la base, 259: el caso_204 con 0,037); con h/4, 60 de 60 (peor 5,3e-4,
    el caso_225: la banda, §17.3). En la batería entera (otras redes de [29],
    porque el azar llega con otro estado): 102 de 102 iguales y 18
    rechazadas, peor 6,2e-4 (13 cocientes fuera); la base, 93 y 5 distintos
    (el 033, el 035, el 047, el 061 y el 070, de 7e-3 a 0,235), que bajan a
    1e-6. [67], igual que en E4.
  - **BMP**: de las 797 de la batería, 796 salen idénticas a las del motor
    de rev4/base; la otra es 617_banco_hecho.bmp, la pantalla del banco de
    tiempos por reloj, que no es determinista.
  - **Bit a bit**: la huella de cada emulación (`HAKARU_HUELLA_LOG`) cambia solo
    en las redes con un tramo, en las que repiten con h/2 o h/4 el paso
    automático de una de ellas y en una cuyo paso de la base era rígido: en
    E4, 101 de 440 (96 con tramo; 4 repeticiones con h/4 de [67] y el diodo
    NR de 300 ms de `test_flanco_rev`); en E2T, 174 de 988 (173 con tramo y
    una repetición con h/2). El balance mejora x2 o más en 80 de E4 y 129 de
    E2T; empeora en 5, y solo se ve en «imp C» (de 2,6e-3 a 0,196 de lo
    entregado: ahora cuenta la pérdida del impulso de verdad, C1 sobre la E
    ideal) y en otras cuatro, con L o no lineales (hasta 8e-5).
  - **La batería entera**: 350 515 comprobaciones, 0 fallos (con el motor de
    rev4/base y este arnés, 24 fallos: los de [68], [41] y [36] que miden lo
    arreglado); `HAKARU_SOLO_E4`, 38 728 y 0 (también con las reglas del ARM);
    `HAKARU_SOLO_E2T`, 39 011 y 0; ASan y UBSan, limpios en [68] y en los casos
    de la revisión (`sim_avanzar` con trozos de ~0, 997 y 1 operaciones da lo
    mismo en los 69).
  - `sim.c` compila sin avisos con gcc y con nspire-gcc -Os (+18 KB de texto
    y +27 KB de bss frente a rev4/base).

### 17.9 Tiempos estimados en la calculadora

| Qué | Estimado |
|---|---|
| RC, RL, RLC de la biblioteca (2000 pasos) | 0,21-0,28 s (antes de calibrar, 0,10-0,17) |
| integrador (100 ms, cuadrada de 1 kHz) | 0,78 s (6400 pasos; antes, 0,43) |
| RLC con cuadrada de 1 kHz, 5 ms | ~0,32 s (antes, ~0,22) |
| peor caso: 32 elementos, 20 nodos, cuadrada de 10 kHz | ≤ 2 s (con PASO_LIM: 2000 pasos, 1,99 s; por sus operaciones y pasos, ~2,1 s) |
| tick de `P_EMULANDO` | ~95 ms + dibujo |

(Con la calibración del cierre, §19.8: 0,6337 µs por operación y 73,8 µs
fijos por paso.)

`.tns`: 438 KB. Memoria estática del emulador: ~1,4 MB de registro, 5 LU de
~70 KB y el osciloscopio: menos de 2 MB de los 64 MB.

### 17.10 Revisión adversarial

Diez agentes buscaron defectos con cinco lentes (motor, memoria, modelo,
interfaz y calculadora de CC). Otros diez intentaron refutar cada hallazgo
reproduciéndolo con un programa propio. **33 confirmados y 7 refutados.**
Todos los confirmados están arreglados, con su prueba en [40] y [41]. Cada
prueba se comprobó por mutación: al deshacer el arreglo, la prueba falla.

| Lente | Confirmados | Los más serios |
|---|---|---|
| motor | 8 | el trapecio arrancaba sin Euler implícito con todo dado (V0/I0) y un modo que el paso no resuelve (oscilación ±85 %); la v_L del ancla como historia; SIM_CORTE_J falso con J y L con la I0 que cuadra; el tope de pasos no contaba los 8 del arranque; un pulso más estrecho que 1e-6 de su escala se perdía |
| interfaz | 16 | [D] no llegaba a la bajada; [=] con pocas muestras ponía un paso más grueso; la tabla quitaba el signo; textos que se salían de los 320 px |
| memoria | 3 | `que_se_vea` avanzaba división a división (minutos con 1 ns/div); `x_de` desbordaba el int |
| modelo | 3 | un [=] que se negaba cambiaba igual el t_fin (hasta > 1e6 s, y el archivo ya no abría); con 32 elementos, poner uno sobre un cable lo borraba |
| calculadora | 3 | la culpa de RED_FUENTE_AMP caía en una L; SOL_INDET culpaba a S1 o R1 según el orden de dibujo; el residuo de una E real en corto |

Al escribir la prueba del primer hallazgo del motor apareció otro: con el
arranque ya bien, la historia BDF2 de una L rígida la dejaba oscilando un
2 %. De ahí la historia mixta de §17.3.

## 18. Etapa 3 — la corriente alterna

La Etapa 3 añade el régimen permanente senoidal: fasores a una frecuencia,
la calculadora de CA, el trazador de Bode, Thévenin y Norton en CC y en CA, el
diagrama fasorial con el triángulo de potencias, la impedancia Z y el
vatímetro. La especificación de trabajo fue `espec_e3.md` (juez sobre tres
propuestas); aquí queda lo que se construyó y dónde se apartó de ella.

### 18.1 Modelo y convenios

- **La fuente alterna** es una forma más de E y J (`FORMA_FASOR`): módulo y
  ángulo en grados. La f es la de **todo el circuito** (`Circuito.frec`, una
  fila más de la ficha: `Q_FCIRC`); al pasar una fuente a alterna vale 60 Hz.
- **Lo guardado es canónico**: eficaz, coseno y grados. `Circuito.conv`
  (`CONV_PICO`, `CONV_SENO`) solo cambia lo que se ve y lo que se escribe en
  la ficha (con pico, 170 se guarda como 170/√2).
- **Dentro de los motores**: fasores de pico, referencia coseno, radianes y
  S = ½·V·I*. En el emulador la alterna es la senoidal de pico √2·E y fase
  θ + 90°, encendida en t = 0: su régimen permanente es el fasor.
- **Impedancia Z** (`EL_IMP`, tecla Z): |Z|∠θ con θ en [−90°, 90°]; se
  escribe también en rectangular (`30 [+] 40`, `[J] 40`). Es la Z a la f del
  circuito; en el Bode la X escala como una L (X > 0) o una C (X < 0), con el
  aviso `Z_ESCALA`. No existe en el tiempo (`SIM_SOLO_CA`).
- **Vatímetro** (`EL_VATI`, en la paleta): la bobina de corriente es un cable
  en serie; la de tensión va de su salida a la tierra (o al − de la primera
  fuente). Marca P = Re(V·I*) y el fp con signo (+ inductivo, − capacitivo);
  en el tiempo, la media de v·i. Sus datos son de la CA.
- Un V o un A nuevo en un circuito de alterna sale en `MED_CA` (eficaz).
- `circ_alterna`: una fuente alterna o una Z. Con alterna, `[?!]` se admite
  también en el valor de C y L (`CF_BUSCA_CA`) y en el ángulo de V, I, E y Z.
  Al dejar de ser alterna, `circ_quita_ca` quita ángulos y esos `[?!]`.
- **Archivo**: versión 3 solo si hay algo de CA (`version_de`); lo demás se
  sigue guardando como la Etapa 2.

### 18.2 Motores

- **`ca.c`**, los fasores a una f: la misma MNA del emulador con g = jωC y
  z = jωL, sobre una LU compleja (`luc.c`, idéntica bit a bit a `lu.c` con
  im = 0). Da V, I y S de cada elemento, lecturas de V, A y W, Σ entregada y
  Σ absorbida (Boucherot), Tellegen, fp y, con una sola fuente, la Z que ve.
  Las demás fuentes de otra f o continuas se apagan con aviso (`CC_FUERA`);
  los interruptores cuentan en su estado final (`S_FINAL`); V0 e I0 no
  cuentan (`CI_NO`); la resonancia exacta de un LC sin R es un error con
  nombre (`RESONANCIA`, detectado a ω·(1 + 1e-6)).
- **El cruce con el trapecio**: el fasor a ω̃ = (2/h)·tan(ωh/2) es el régimen
  permanente del trapecio, exacto ([46]). Dos diferencias conocidas del
  emulador: un elemento algebraico (un C sobre una E ideal) lleva un modo
  parásito (−1)ⁿ de tamaño ~(ωh)⁴, y un C o una L flotantes guardan la
  continua que tenían al empezar.
- **Thévenin y Norton** (`ca_thevenin`, y en CC `sim_thevenin_cc`): Vth,
  Zth, IN, la carga de máxima potencia ZL = Zth* (en CC RL = Rth) y la mejor
  si solo puede ser una R.
- **`bode.c`**: autorrango grueso de 49 puntos (1 mHz a 1 GHz, 4 por
  década) y la malla fina de 277 puntos (139 o 70 si un punto sale caro:
  `CA_NS_OP` = 1,6938 µs por operación de CA, medido con `BN_CA`; el peor
  caso ya baja a 70). Medidas: pico y valle por
  sección áurea; cortes a −3 dB y fase 0 por Brent; f0, BW, Q; clasificación
  en pasa-bajas, pasa-altas, pasa-banda, rechaza-banda, plano u otra. Todo
  por trozos, con el resultado independiente del tamaño de los trozos.
- **`calca.c`**, la calculadora de CA: prueba el circuito directo y corrige
  las incógnitas hasta que los datos cuadran.
  - Con una incógnita, malla exhaustiva (13 décadas, o la vuelta entera de un
    ángulo, más las frecuencias características si la incógnita es la f) y
    todas las raíces con Newton protegido.
  - De 2 a 4, Levenberg-Marquardt desde 25, 27 o 16 arranques con el
    jacobiano exacto (`ca_derivada`, `ca_dvi`: una sustitución más por
    incógnita con la misma LU), dos pasos de Newton de pulido y una prueba de
    condicionamiento. La semilla de una C (o de una Z que no es inductiva) va
    un factor 1,7 al lado: las de L y C resuenan entre sí y, con saltos de
    década, un tercio de los arranques caería en la resonancia; un arranque
    singular se reintenta un pelo al lado.
  - Una raíz doble (el dato toca la curva sin cruzarla) es un valle plano de
    ancho √CAL_TOL: dos candidatos a menos de 1e-3 cuyo punto medio también
    cuadra son la misma solución.
  - Diagnósticos con nombre: faltan datos, demasiadas incógnitas, lo que no
    se despeja en CA, dos incógnitas que los datos no separan, datos que no
    se alcanzan (con el rango que sí), solo en el borde, un valor que no
    cambia nada, cortado.
  - Por trozos, bit a bit igual con 1, 997 u ops ilimitadas; tope de 5 M de
    operaciones (~3,2 s en la calculadora).

### 18.3 Pantallas

- **La ficha en CA**: caja del módulo y caja del ángulo (`sub`, `[tab]`),
  `[+]`/`[-]`/`[J]` para la parte j de una Z, `[?!]` en el módulo (el fasor
  entero) o en el ángulo, eco en la otra forma y pistas. En CC no cambia nada:
  las BMP de las fichas de CC son las de la Etapa 2.
- **`[=]`** (`igual()`): alterna sin `[?!]` → P_CA; alterna con `[?!]` → la
  calculadora de CA, por trozos en P_EMULANDO (`TR_CALCA`) con un primer
  trozo en el acto; sin alterna, como en la Etapa 2.
- **P_CA**: el esquema con los fasores (6 capas) y un panel: lo que halló la
  calculadora y lo que se pedía, f y convenio, la I y la Z que ve la fuente,
  P, Q y S, V(AB) o el fp y el primer aviso. `[arr/ab]` entre soluciones,
  `[.]` polar o rectangular, `[tab]` la tabla, `[enter]` el diagrama, `[T]`,
  `[B]`, `[O]` (emula cres: se ve cómo se llega al fasor) y `[menu]`. La
  calculadora guarda el problema planteado (`ca_pose`); `cres` lleva la
  solución elegida como datos, así que `[T]`, `[B]` y `[O]` trabajan sobre
  ella.
- **P_CA_TABLA** (4 páginas: fasores, potencias, totales con Boucherot y
  Tellegen, potenciales), **P_FASORIAL** (`fasorial.c`), **P_BODE**
  (`trazador.c`) y **P_THEVENIN**.
- P_EMULANDO sirve para tres trabajos (`TR_EMULAR`, `TR_BODE`, `TR_CALCA`);
  P_AJUSTES tiene tres contextos (tiempo, CA, Bode); el menú del editor tiene
  9 opciones (la 6 son los ajustes de CA en un circuito de alterna; la 7, el
  Bode); la paleta es de 4×4 con la Z y el vatímetro.
- Línea de estado en CA: «Listo: [=] analiza en CA a 60 Hz», los errores de
  forma, f o valores, y con `[?!]` la cuenta de la calculadora («Pides L1:
  listo ✓ pulsa [=] (CA)», «falta 1 dato (CA)», «sobra 1 dato (se
  comprueba)»).
- Lección 11, «Corriente alterna», con dos páginas de teoría y 10 modelos
  (filtros, resonancia, tanque, RLC en fasores, carga con vatímetro,
  corrección del fp, el voltímetro que marca 72 V y Thévenin).

### 18.4 Desviaciones de la especificación

| Especificación | Hecho | Por qué |
|---|---|---|
| «3 0 [-] 4 0» da 50∠−36,87° | 50∠−53,13° | atan2(−40, 30): la cifra de la especificación estaba mal |
| potencias con prefijo («1,142 kVA») en P_CA | de 1000 a 9999 sin prefijo («1142 VA»), en P_CA, la tabla y el fasorial (`num_pot`) | así lo escribe la especificación en sus ejemplos, y el fasorial ya lo hacía |
| `CalRes.ecu` era a la vez un índice y la lista | el índice es `iecu`; con INDET, `elem` e `iecu` son las dos incógnitas; con BORDE, la incógnita y la ecuación | no compilaba |
| mínimos de \|r\| por debajo de 0,3 | sección áurea en cada mínimo interior, parando al cambiar de signo; regula falsi en los cruces de X | una resonancia estrecha esconde dos raíces entre dos puntos de la malla con \|r\| lejos de 0,3 |
| la malla tal cual | un punto singular se reintenta a 1e-9 del lado | la semilla de C cae justo en la resonancia de la L conocida |
| una «solución» en la que r llega a 0 | `CAL_BORDE` si la curva es plana hacia el borde | con el dato igual al límite (V1 = 120), r llega a 0 en coma flotante antes del tope |
| P = 0 con el residuo de \|S\| del arranque | residuo con signo P/esc, esc = ½·Vesc²/Z0 | no depender del punto de arranque |
| la referencia de fase también con `EC_RE`/`EC_IM` | no | un fasor nulo girado sigue nulo: no fija la fase |
| cadena `[S]` de todos los visibles | sin las fuentes (se quedan en el origen) | si no, V(E1) contaría dos veces y la cadena no podría acabar en E1 |
| el fasorial lista todas las V e I | sin las nulas (la I de un voltímetro ideal) | no dicen nada |
| el trazador con dos cursores: líneas 2 y 3 del tipo | las dos lecturas (f1 y f2) | se perdían las lecturas de cada cursor |
| sin teclas para mover la curva | `[shift][arr/ab]` una división, como el osciloscopio; `[P]` sin pico lleva al máximo | el tope de dB en divisiones enteras no dejaba centrarla |
| `traz_lee` pide `bode_en` cada vez | caché de las 4 últimas lecturas por barrido | con dos cursores cada tecla costaba 4 factorizaciones |
| el mono al azar | el último 5 % de las teclas empieza cada 150 en un modelo de la lección 11 (y una vez de cada 8 en los créditos) | el azar casi nunca arma un circuito de CA que se resuelva: P_CA_TABLA y P_FASORIAL salían por suerte |
| 9 arranques con 2 incógnitas; soluciones perdidas aceptadas como riesgo | 25 (la malla de 5×5 en medias décadas), semillas de C y Z capacitiva a 1,7 y reintento del arranque singular | un LC serie con L y C buscadas perdía una de sus dos soluciones sin decirlo; en un barrido de 588 casos solo faltan 4 raíces de 1e-10 H |
| el rango de INCOMPAT, de la malla | de todas las evaluaciones; el ∠ desenrollado y el fp en φ | la malla saltaba el pico de una resonancia (decía 10,3 V cuando el pico es 21 V) y un ∠ o un fp daban un rango que contenía el dato |
| sin tipo para una salida nula | `BODE_NULA` (\|H\| ≤ 1e-12) | un puente equilibrado salía pasa-banda con Q inventada |

### 18.5 Verificación

- **Arnés**: 311 709 comprobaciones, 0 fallos (normal, ASan+UBSan y con
  `char` sin signo). Secciones nuevas: [41] la revisión de la Etapa 2; [42]
  complejos, LU compleja y formato; [43] el modelo; [44] la CC ante la CA;
  [45] casos cerrados (serie, paralelo, puentes, J apagadas, fuentes reales,
  instrumentos con R, resonancia); [46] el cruce con el trapecio; [47]
  Thévenin y Norton; [48] el Bode; [49] la calculadora de CA (la tabla de
  §6.9 contra mpmath a 40 cifras, el jacobiano, 300 inversos por
  construcción, los trozos); [50] el fasorial; [51] el trazador; [52] y
  [52b] la interfaz por guion de teclas (la ficha, P_CA, la tabla, el
  diagrama, `[O]`, `[B]`, la calculadora por teclas, la paleta, el menú, las
  páginas Modelo); [54a], [54b] y [54c] los defectos de la revisión (§18.7);
  [53] la biblioteca (las 10 claves, también abriendo cada
  modelo desde la lección 11).
- **Oráculo de CA** (`herramientas/oraculo_ca.py`, numpy): su propio MNA
  complejo, escrito aparte. El arnés ([29b]) exporta 200 redes de CA al azar
  (R, C, L, Z, E con r, J, amperímetros y voltímetros con y sin R,
  interruptores en su estado final, vatímetros; de 10 Hz a 10 kHz) con los
  fasores de ElectroFock y cuatro puntos del Bode de cada una. **175 de 175
  iguales**: fasores y lecturas a 1,6e-13 de la escala (tolerancia 1e-12) y
  el Bode a 1,4e-12 (tolerancia 1e-10). Las otras 25 las rechaza el análisis
  (lazos de tensión, cortes de J). En una red casi muerta (la salida vale
  cero) la escala es la física: la amplitud de las fuentes, V/1 kΩ para las
  corrientes y 1e-4 para un H adimensional.
- **Mono**: 150 000 teclas por las 28 pantallas, sin desbordes.
- **BMP**: las 622 de la Etapa 2 que no debían cambiar son idénticas; las que
  cambian a propósito son el menú de 9 opciones, la lista de lecciones, la
  Ayuda, el banco y la tierra en el resultado (y las pantallas animadas,
  que cambian con el azar).

### 18.6 Tiempos estimados en la calculadora

| Qué | Estimado |
|---|---|
| CA del peor caso (32 elementos, 20 nodos) | ~9,1 ms |
| Bode de un RLC con autorrango (277 puntos) | ~0,19 s (medido en la calculadora: 0,133) |
| calculadora de CA: el motor (dos incógnitas) | ~0,35 s |
| Bode del peor caso | t_est 3,84 s con la malla de 70 puntos; 3,00 s por sus operaciones |

(Estimados con la calibración del cierre, §19.8; antes, con 0,63 µs y 40 µs
fijos por punto, eran ~4,5 ms, ~0,09 s, ~0,36 s y ≤ 3,5 s.) `BN_CA` (un
punto del Bode en el peor caso: calibra `CA_NS_OP`) y `BN_BODE` (el Bode
entero de un RLC) están en el banco. `.tns`: 616 KB.

### 18.7 Revisión adversarial

Cinco agentes buscaron defectos con cinco lentes (motor de CA, calculadora de
CA, Bode, interfaz, modelo y memoria) y otros cinco intentaron refutar cada
hallazgo reproduciéndolo. **34 confirmados (29 distintos) y 4 refutados.**
Todos los confirmados están arreglados, con sus pruebas en [54a], [54b] y
[54c] (369 comprobaciones), y cada arreglo se comprobó por mutación: al
deshacerlo, su prueba falla. Uno de los refutados («se pierde una solución
con L y C buscadas») era una limitación que la especificación aceptaba; se
arregló igual (25 arranques y semillas que no resuenan), y al probarlo
apareció otro: una raíz doble daba varias soluciones casi iguales.

| Lente | Confirmados | Los más serios |
|---|---|---|
| interfaz | 16 | [esc] quedaba en un bucle entre P_CA y P_BODE; la paleta 4×4 leía fuera de su tabla; «2,5+j1,5» se guardaba como 2,5+j15; los rótulos del editor usaban el convenio de la última P_CA; al borrar la última Z quedaban ángulos que en CC no se podían quitar |
| Bode | 7 | una salida nula salía «Pasa-bajas» y un puente equilibrado, pasa-banda con Q inventada; el LC ideal salía «Otra forma»; los modelos de la lección 11 heredaban el rango del editor; los avisos OTRAS y Z_ESCALA no se veían |
| modelo y memoria | 6 | un fp del vatímetro de 5 se guardaba y el problema ya no se abría; la I de un C y la V de una L quedaban como datos invisibles en CC; `(int)Inf` en el formato |
| calculadora de CA | 3 | el rango de «no se alcanza» salía de la malla y era falso (10,3 V con un pico de 21 V); un ∠ o un fp daban un rango que contenía el dato; «R no lleva corriente» de una R que sí la lleva |
| motor de CA | 2 | una Z de ±90° llevaba una R de 6e-15 Ω: un tanque de Z puras daba 10¹⁷ V en vez de un error; la resonancia sin pérdidas culpaba al amperímetro |

El asan del arnés vigila ahora también las conversiones de coma flotante a
entero (`-fsanitize=float-cast-overflow`).

## 19. Etapa 4 — semiconductores, AO y barrido de CC

La Etapa 4 añade los componentes no lineales y los que tienen más de dos
patas: el diodo (ideal, con Vγ, exponencial y de catálogo), el zener, el LED,
las fuentes controladas (E y J por una V o una I), el amplificador
operacional (ideal y real), el BJT y el MOSFET; el punto de operación de CC
por Newton, el transitorio no lineal, la pequeña señal en su punto Q (CA y
Bode) y el barrido de CC con su pantalla de curvas y el trazador de un
componente. La especificación de trabajo fue [`espec_e4.md`](historia/espec_e4.md) (juez sobre
cuatro propuestas; en `docs/`, con los nombres de su momento); aquí queda lo
que se construyó, dónde se apartó de ella, cómo se verificó (el oráculo no
lineal y tres rondas de revisión adversarial) y lo que queda.

### 19.1 Modelo

- **Ocho tipos nuevos** al final de `TipoEl`, cada uno con sus variantes: el
  diodo (ideal, con Vγ, exponencial, 1N4148, 1N4007), el zener (de libro,
  exponencial, 1N4733A), el LED (seis colores), la E y la J controladas (por
  la V o por la I de otro elemento de dos bornes: μ, r, g, β), el AO (ideal,
  real), el BJT (NPN, PNP) y el MOSFET (canal N, canal P). Los vacíos toman
  los valores por omisión de `nolin.h`.
- **Tres patas**: `Elemento.xc/yc` y una huella por tipo (`H_AO`, `H_BJT`,
  `H_MOS`) con su caja; `circ_orienta` recupera la flecha y el espejo con que
  se colocó; `circ_cabe` dice si cabe y qué estorba; los puntos interiores de
  la caja no son bornes. `MAX_NODOS` = 70; `red.c` da a cada AO la referencia
  de su subcircuito (`red_ref_de`: la tierra o el − de su primera fuente).
- **Archivo**: versión 4 solo con algo de la Etapa 4 (la sección 3 del
  elemento lleva la pata c); lo demás se guarda como antes, byte a byte.
- **La tabla (la calculadora de CC)** resuelve las controladas (son
  lineales); con un no lineal da `SOL_NO_LINEAL`, cuyo mensaje lleva al
  barrido.

### 19.2 Motores

- **Todo lo nuevo va detrás de `NL.n`**: un circuito sin no lineales sigue el
  camino de siempre bit a bit ([34], los oráculos, las BMP).
- **Controladas**: la EC con su rama; la JC, un estampado de la ley de
  nodos; el control es Σ c·x + c0 (`ctl_coef_en`). Un lazo con una
  controlada mandada por la I de un miembro del lazo tiene ley (`lazo_con_ley`
  generalizado).
- **No lineales**: los **por tramos** (diodo ideal y con Vγ, zener de libro,
  AO) y los **suaves** (uniones exponenciales, BJT, MOSFET). Newton con
  Katzenelson para los tramos, `pnjlim`/`fetlim` para los suaves, criterio
  doble (incrementos y leyes, o residuo) y una iteración de pulido en el OP;
  rescates por gmin escalonado y por rampa de fuentes (aviso HOMOTOPIA); un
  tope de 1,5 M de operaciones por OP (`NC_TOPE`).
- **AO**: nulador en el ideal, VCVS o Norton con Ro en el real, y los tramos
  LIN, SAT+ y SAT−. **La regla de la realimentación**: el lineal solo vale si
  la realimentación instantánea es negativa (una sustitución más con la
  misma LU); si no, el AO va a una saturación. Así un Schmitt tiene sus dos
  estados (aviso BIESTABLE), un astable arranca solo (aviso ARRANQUE) y un
  comparador sin realimentación no es singular.
- **Transitorio no lineal**: la caché de LU con la clave de los tramos (8
  entradas con no lineales), tramos congelados en el paso y localización de
  los cambios por Illinois (`evento_nl`, con Zenón); para los suaves, Newton
  por paso con predictor lineal, subpasos blandos (h/2ⁿ) controlados por el
  error local de C y L y por el cambio de corriente de cada unión.
- **El tramo rígido con no lineales** (§17.3): cada subpaso SDIRK2 son dos
  Newton, uno por etapa (`paso_nl_rig`), con la misma hh = 2γ·subpaso; los
  subpasos blandos parten el subpaso (nivel, hasta h/8/64 como en el
  arranque) y el reintento de 50 iteraciones de `un_paso_nl` vale también
  aquí. Si la etapa 1 sale de su tramo, `localiza_nl` recibe su instante y
  el cambio se localiza con pasos de Euler partidos, como en el arranque; el
  evento corta el tramo. La y⁺ consistente es el Newton de los dos Euler de ε
  (cuatro con impulso; `rig_ymas_nl`, con Katzenelson si toca). Las
  integrales (W, la ventana y los canales) van subpaso a subpaso como en lo
  lineal (`rq_suma`: la etapa 1 deja sus V e I, con su linealización, antes
  de la etapa 2, y cada trozo de nivel suma lo suyo), con el segundo puerto
  de AO, BJT y MOS igual que el primero.
- **Los primeros subpasos del tramo, partidos**: con z = s/τ > 1 + √2 (τ, la
  del modo más rápido del evento) la historia de la segunda etapa queda más
  allá de la asíntota del modo; en lo lineal es un rebote (R(z) < 0) que los
  subpasos siguientes apagan, pero con un diodo que carga un C el C queda
  sobrecargado, el diodo se corta y el rebote se queda: con τ = h/70
  (100 k de carga, 10 kHz, h = 7,14 µs) 0,24 de la escala en te + h/2 y
  4e-2 tras cada flanco, también en la versión que se revisó. `aplica_evento`
  empieza el tramo con el nivel que deja z ≤ 1 + √2, y `un_paso_nl` lo baja
  (un nivel por cada par de trozos alineado) cuando el modo ya se apagó
  (lo que queda, M.rig_amp, por debajo de 1e-4, con |R(z)| de cada trozo);
  si ni con h/16/64 se llega, cada trozo es un Euler implícito entero
  (monótono) hasta que se apague. Queda en 3,0e-4 (el «diodo C z4.5» de
  [68]). Al acabar el tramo, la historia del error local de los suaves
  (`control_nl`) pasa a te + (n − 1)h, te + (n − ½)h y te + nh: con los tres
  últimos subpasos, a h/16, la tercera diferencia dividida aumenta su error
  256 veces y el trapecio que sigue se partía de más (4200 pasos en vez de
  3900 en el diodo de 100 k a 20 kHz).
- **La presonda** (`presonda`, en `elige_paso`): en una red con uniones, BJT,
  MOS o diodos por tramos, y con C o L, los 64 primeros eventos de las
  fuentes y los S se agrupan en clases (`clase_de`: los S antes y después,
  el signo del salto de cada fuente y el de la pendiente nueva en un vértice
  de una triangular; 8 como mucho). De la primera de cada clase, desde el
  estado del punto de operación, con las fuentes y los S de te⁺, el Newton
  del Euler implícito de ε (con Katzenelson: los tramos nuevos) sin
  aceptarlo, y los modos de esa linealización (`modos_afinados`), que
  deciden el tramo de todos los eventos de la clase (n y la τ de su ε) y
  entran en la estimación de los 2 s y en los topes (§17.4). No copia M:
  repone `M.nl`, el diagnóstico, los avisos, el residuo acumulado y ops_lu;
  sus iteraciones van a `nw.sonda` (no a `nw.iter`). Una red en que ninguna
  clase cambia el tramo de la red emula como antes, bit a bit y sin nada por
  evento (el diodo NR: +0,13 % de operaciones, las de la presonda; la
  escalera de 8 C con un diodo, +3,1 %). En un cambio de tramo (las fuentes
  no saltan) el tramo es el de la red, y con solo AO bastan los modos del
  punto de operación y del AO saturado. Con ella, el «diodo C» de [68] (en
  corte en el punto de operación; al subir la fuente, un modo de 0,1 µs con
  h = 1 µs) pasa de 0,17 en la y⁺ y 0,04 después a 1,9e-6 y 7,8e-4; la
  patada inductiva de [60] no hace el tramo (su modo de 0,56 ns, la L con el
  diodo en corte, es del punto de operación: en te⁺ el diodo lleva la
  corriente de la L y su modo es lento, L/(rd + Rs) ≈ 16 ms) y sigue con
  307 pasos, bit a bit. La primera versión (la sonda) hacía lo mismo en cada
  evento: con el estado de cada uno, pero su costo (un Newton y los modos por
  evento, también en las redes no rígidas: +7 % en el diodo NR, +30 % en la
  escalera) y sus tramos quedaban fuera de la estimación.
- **Pequeña señal**: `sim_punto_q` linealiza en el punto de operación
  (`SimLin`); `ca.c` y `bode.c` estampan esa linealización (aviso PEQ_SENAL;
  un AO saturado en Q da H = 0 y el aviso SATURA).
- **Barrido de CC** (`barrido.c`): barre una E, una J, el valor de una R o la
  ganancia de una controlada en la vista de CC (C abiertos, L cables),
  siguiendo cada punto desde el anterior (continuación): la vuelta de un
  Schmitt sale sola. Un punto que no converge parte el intervalo desde el
  último bueno en 2, 4… 256 trozos; si no, un OP completo (un salto de
  histéresis) o un hueco. Un tope de operaciones por punto. Lecturas exactas
  en cualquier x (`barr_en`, con la derivada exacta dy/dx por una sustitución
  con la LU de la solución, incluidas las controladas mandadas por la I de lo
  barrido), el cruce de un nivel por Brent (`barr_nivel`) y el de un dato con
  su signo (`barr_nivel_dato`). La malla es de 281 puntos (141 o 71 si sale
  caro), por trozos y bit a bit igual con cualquier tamaño de trozo.
- **Paso y ventana con no lineales**: los modos con los tramos del punto de
  operación y, con un AO, la topología con cada AO en V+ (la τ del astable);
  la ventana es 5·T con fuentes periódicas (sin las τ de más de 10·T: la
  descarga del C de un rectificador) y 10·τ de las saturadas sin ellas.

### 19.3 Pantallas

- **Editor**: `[D]` diodo, `[U]` AO (`[Q]` BJT y `[M]` MOSFET, …); zener, LED
  y controladas en la paleta (20 piezas, luego 21 y 22); colocar un tres
  patas con las cuatro flechas (las que caben, en amarillo) y la marca roja
  de lo que estorba; `[F]` espejo; la fila x de una controlada elige su
  control con `[+][-]` y el editor dibuja la línea discontinua hasta él.
- **`[=]`** con no lineales: el **punto de operación** en P_RESULTADO (la
  línea de Newton, el LED encendido con su porcentaje, la región de cada no
  lineal en la tabla, el segundo puerto de los tres patas y el residuo de
  Kirchhoff); con `[?!]`, `SOL_NO_LINEAL` y el pie «[I] barrer R1»; si el
  circuito cambia con el tiempo, lo emula. `[P]` da el punto de operación de
  CC de cualquier circuito.
- **P_BARRIDO** (`curva.c`): las trazas con huecos y saltos, la vuelta
  discontinua, dos cursores, el nivel `[N]` (con un dato, el cursor ya está en
  el cruce: «¿qué R da 20 mA?»), X-Y, escala logarítmica, zoom, `[=]` barre
  lo que se ve, `[tab]` el circuito en el cursor, `[menu]` sus ajustes (qué
  se barre, el rango, la escala y la ida y vuelta). El trazador de curvas
  (`[I]` en la página Modelo) da la curva I-V de un diodo, un zener o un LED
  y la familia de 6 curvas de un BJT (IC(VCE) con IB en pasos) o de un
  MOSFET (ID(VDS) con VGS en pasos).
- **El emulador** con no lineales: el LED brilla con la corriente media; la
  línea de Newton (iteraciones por paso) o la de los cambios de estado; la
  región al final en la tabla.
- **Biblioteca**: la lección 12 «Diodos» (modelos 23 a 32) y la 13 «AO y
  transistores» (33 a 42), con dos páginas de teoría cada una; los modelos
  de punto de operación y de barrido se abren en P_RESULTADO y en P_BARRIDO
  con el cursor en la respuesta. La Ayuda gana sus seis secciones.

### 19.4 Desviaciones de la especificación

| Especificación | Hecho | Por qué |
|---|---|---|
| el diodo por tramos como una G | una variable de rama con la ley del tramo en su fila | con Ron = 1 mΩ la G de 1000 S cancelaba cifras en la ley de nodos |
| residuo de Kirchhoff relativo a la escala del nodo | con un suelo de ruido de redondeo, 2,2e-4·G·(\|va\| + \|vb\|) | en inversa, un nodo con 1 pA daba un residuo relativo enorme hecho solo de redondeo |
| el pivote de siempre | `PIVOTE_NL` = 1e-20 en las factorizaciones con no lineales | un bloque que solo cuelga de diodos en corte es regular gracias a GMIN = 1e-12 S |
| el balance exacto con eventos | Tellegen exacto y `dif_balance` ≤ 5e-3 con eventos | el arranque de Euler tras cada evento no conserva la energía; sin eventos sí es exacto. En el tramo rígido, las integrales subpaso a subpaso (§17.3) lo mejoran (la BJT 204 de 2,8e-7 a 7e-14 de lo entregado) |
| el paso con los modos del punto de operación | con no lineales, el tramo rígido de cada evento lo deciden los modos de la linealización en te⁺ de su clase (la presonda, a priori) | el modo rígido de un diodo que empieza a conducir con el evento no está en el punto de operación (el «diodo C» de [68]); a priori para que cuente en la estimación y en los topes |
| SDIRK2 con el subpaso del tramo | con no lineales, los primeros subpasos partidos hasta z ≤ 1 + √2 | un diodo que carga un C quedaba cortado con el C sobrecargado por la segunda etapa (0,24 de la escala con τ = h/70) |
| la triangular en un vértice | su valor exacto por la izquierda | se evaluaba 1e-6·T antes y un derivador daba ±0,40012 |
| partir el intervalo desde el punto anterior de la malla | desde el último bueno | tras un hueco el anterior no tiene continuación |
| el costo de un paso con suaves (c_ens sin concretar) | c_ens = 10·nel + 8 por suave + el término de cada fuente; el registro, 37·nel | con la fórmula corta, `t_est` era la mitad de lo que cuestan de verdad los pasos |
| las estimaciones de [14] con ×1,3 | el modelo del §8.2 con lo medido (§19.8): 0,6337 µs por operación + `NS_ITER` = 250,5 µs por iteración + `SIM_NS_PASO` = 73,8 µs por paso | ×1,3 era la holgura de las redes lineales; con no lineales, las iteraciones son lo que no se cuenta |
| el banco con cuatro líneas de resumen | tres (13 pruebas no dejan sitio) | `BN_EXP` y `BN_NL` se sumaron a las once |
| V+ ≤ V− solo al resolver | la ficha tampoco lo deja escribir (con los vacíos, ±15 V) y `circ_valido` lo rechaza | el mono dejó un AO con V+ = 0 y V− = 8 |
| la etiqueta del LED emulado, del instante final | la de la corriente media | con una senoidal el LED brillaba y su rótulo decía «apagado» |
| la semilla de las animaciones, de las teclas desde el arranque | el arnés la fija al empezar su prueba (`ui_semilla`) | cada lección nueva cambiaba las BMP de las pantallas animadas |
| modelo 25 con el paso automático | paso T/100 | con ~50 µs ninguna muestra cae en el pico y 'M' salía a 4,7e-5 |
| los modelos de punto de operación y de barrido con sondas | sin sondas | una sonda hace el circuito dinámico y `[=]` emularía |
| en el paso del transitorio, sin iteración de pulido | una más solo si el residuo de Kirchhoff pasa de 1e-10 | los incrementos (1e-6 de un potencial) dejaban la media onda del modelo 25 con 2e-9; ningún otro caso la dispara |
| la solución de la LU tal cual | con un suave de dos puertos (BJT, MOSFET), un paso de refinado iterativo fila a fila (`refina_nl`) | la transconductancia hace de pivote a la fila del colector en la columna de la base: con la base al aire el residuo se quedaba en 7e-10 |
| la familia del PNP (sin decir) | x = VEC y la curva −IC | que se vea igual que la del NPN |
| la familia con 281 puntos | 141 (la malla sale del costo del primer punto, un OP completo con IB = 0) | estimaba 3,5 s; el tope es 3 s |
| la lista de lecciones: «N ejemplos resueltos» | «N circuitos modelo» en las de modelos (10 a 13) | decía «0 ejemplos resueltos» desde la Etapa 2 |
| los límites de `mos1load` tal cual | `nl_mos_lim` cuenta como límite solo un cambio mayor que 2e-15·(\|vGS\| + \|vDS\|) | la vuelta de `mos1load` mueve el último bit y, en óhmica, Newton no acababa de converger |
| la columna «Valor / región» de P_TABLA con `res_nl` | «Valor/reg.» (10 columnas) | 14 columnas pisaban la V de la columna siguiente, y «Valor/región» (12) aún la tocaba |
| [esc] en P_BARRIDO vuelve a `barr_origen` | si el barrido salió de una P_RESULTADO y `[tab]` la pisó con la del cursor, vuelve a la de partida (guardada); `[I]` en la del cursor vuelve a su barrido | se entraba en un bucle de [esc] entre las dos pantallas y el resultado de partida se perdía (revisión) |
| el rango del barrido vive en la sesión | vive en la sesión, pero ligado a su circuito (`firma_circ`: tipos, sitios y etiquetas); con otro circuito, o con un `[?!]` en otro elemento, se olvida | un modelo dejaba su R1 de 10 Ω a 10 kΩ y el editor barría su E1 de 10 V a 10 kV (revisión) |
| `[P]` es de cualquier circuito (sin decir qué pasa con uno lineal) | con una red lineal, «red lineal, sin Newton», `[T]` su Thévenin y `[enter]` el motivo cierto de que no haya paso a paso | decía «? no es lineal» (revisión) |

Las revisiones 2 y 3 (§19.7) añadieron estas:

| Especificación | Hecho | Por qué |
|---|---|---|
| `barr_en` desde la continuación del punto de la malla más cercano | desde el más cercano de los de antes en el sentido del recorrido (x_i ≤ x en la ida y en la familia; x_i ≥ x en la vuelta) | entre los dos puntos de un salto, el de después ya está en la otra rama: un Schmitt leía la otra rama antes de su umbral y `barr_nivel` daba el punto medio de la malla (revisión 2) |
| el rango automático de una ganancia k, [0, 2k] | [2k, 0] con k < 0 | la malla tiene que crecer: con μ = −10 la curva no se dibujaba y los ajustes no aplicaban «auto» (revisión 2) |
| el trazador: E de −1 V (el zener, −(Vz + 1 V)) a V(I_top) + 1 Ω·I_top (§4.10) | arriba, como pide (`v_directa`, la ley del componente al revés); abajo, −2 V (el zener, −Vz0 − 2 V o −BV − 2 V) | antes iba de −2 a 2 V fijos (el LED, hasta 5) y un diodo de Vγ = 3 V no llegaba a conducir; se tocó solo el extremo de arriba: el de abajo no da nada falso (revisión 2) |
| la línea de estado del §4.1 con un no lineal y un `[?!]`: «[I] barre y [N] lee» | no con una fuente alterna, como `igual()` del mismo §4.1, que emula antes | la línea mandaba a `[I]` mientras `[=]` emulaba y fallaba (revisión 2) |
| un AO sin referencia: «pon [G]» | si el circuito ya tiene la tierra en otra parte, «Ux no llega a la tierra ni a una fuente: únelo.» (al colocar, en la línea de estado y en `[=]`); la línea avisa también de AOR_CORTO («La salida de Ux está en su referencia: muévela.») | el consejo llevaba la tierra a la salida del AO y `[=]` fallaba (revisión 2) |
| lo que marca un V, en magnitud (§4.6) | con signo en el punto de operación (`dib_resultado_nl`: también la red lineal de `[P]` y el cursor del barrido); la P_RESULTADO de la calculadora sigue en magnitud | el modelo 35 rotulaba «3V» con V1 = −3 V; en una red lineal con un V al revés, `[=]` enseña «3V» y `[P]`, «-3V» (revisión 2) |
| la familia sin VA: «r0 = ∞ (sin VA)» o «(sin Early)» | solo en la zona plana; fuera (la óhmica del MOS, la saturación del BJT), la pendiente exacta, «r0 = dVDS/dID = …» (`barr_region`, la región de la última lectura) | la curva sube casi vertical ahí: 4 mS con «∞» (revisión 2) |
| ca_aviso(SATURA): «… su salida no sigue (vale 0).» | «… en pequeña señal su salida es 0.», con el AO que satura (`elem_aviso`), también en P_BODE y en P_CA | tenía 61 columnas y nombraba al primer AO del circuito (revisión 2) |
| la regla del AO ideal (§3.F): el lineal vale si S = dVsal/dδ < 0 | si S es nulo frente a lo que se mueven los otros ideales en LIN, decide el primer término no nulo de S(ε) = [(M − εI)⁻¹]kk (M⁻¹ = dVsal/dδ de los ideales en LIN; el AO real con A = 1/ε → ∞), una sustitución más por término (`ideal_vale`); con S no nulo, lo de siempre | con S = 0 (un seguidor dentro del lazo de otro AO, el detector de pico con seguidor) el AO salía del lineal, el otro perdía el lazo y Katzenelson daba vueltas: «no converge» en circuitos de libro (revisión 3) |
| Katzenelson da por bueno un tramo con margen ≥ −1e-9·Vesc, y el paso congelado localiza si algún margen < −1e-9·Vesc (§3.D, §3.G) | además, un diodo por tramos en conducción o en ruptura sale de su tramo si su corriente de rama va del revés más de 1e-15 A + 1e-9·Iesc (`al_reves`), en Katzenelson y al final de un paso congelado | con Ron = 1 mΩ, −1e-9·Vesc son 1e-6·Vesc A del revés: el diodo hacía de pila de Vγ o de Vz0 (40 mV de error en un regulador con divisor) (revisión 3) |
| con la LU singular y un AO en LIN, el AO va a una saturación (el código: siempre a V+) | a la del signo de su u en x (D7; con x = 0, V+, como antes) | siempre a V+, dos AO anidados iban y venían entre V+ y el lineal hasta NLR_CICLA (revisión 3) |
| el residuo de Kirchhoff de cada solución de Newton en `residuo_max` y `residuo_acum` (§3.D) | sin los pasos de gmin con gd > 0: cuenta el del último, con gd = 0; `residuo_acum` (el mono), con el suelo de la escala de la red; el barrido tiene el suyo, `residuo_barr` | tras gmin la tabla daba «Kirchhoff 9,9E-1» en rojo sobre una solución que cumple Kirchhoff a 1e-14 (revisión 3) |
| el suelo de ruido del residuo (arriba) | en la salida de un AO real con Ro > 0, en su lineal, además 2,2e-4·A·G·(\|V+\| + \|V−\|) | A/Ro amplifica el redondeo de V+ y V−: un restador con la solución exacta daba «Kirchhoff 2,1E-8» en rojo (revisión 3) |
| el culpable, el suave de mayor Δv relativo en la última iteración (§3.D) | si la LU sale singular y no hay un AO en su lineal a quien culpar, el suave de mayor G | una unión sin R sobre una E ideal hace singular la LU (su G pasa de 1e20 S): daba «El circuito no tiene una solución única (mira E2)» y, con una segunda etapa, culpaba al otro BJT. Ahora gmin y la rampa lo intentan también y, si no hay manera, «no converge» con su culpable (revisión 3) |
| con suaves, la prueba de Illinois es un Newton congelado de 12 iteraciones (§G.2) | si no converge, otra vez desde M.x con 50 iteraciones antes de dar NC_TR, como en `un_paso_nl` | el superdiodo (AO ideal + 1N4148) moría en 1,5 ms: fallaban 501 de 1152 variantes y 63 de 216 del AO con seguidor de emisor (revisión 3) |
| si en el nivel máximo lo que falla es Newton, NC_TR (§G.3) | antes, 50 iteraciones desde el mismo arranque | tras un flanco sin C ni L no hay Katzenelson y partir el paso no acorta el salto: el modelo 41 con E1 cuadrada moría en t = 4,88 ns (el arreglo B del oráculo, §19.6) |
| NC_TR: «… o usa otro modelo del diodo (con Vγ).» | ese consejo, solo a un diodo exponencial o del catálogo; a los demás, «… Revisa los valores.» | se lo decía también a un LED o a un BJT, que no tienen ese modelo (revisión 3) |
| `dib_emu_esq`: el primer aviso con QUEMA > SATURA > los de antes (§3.M) | QUEMA > BIESTABLE > ARRANQUE > SATURA > los de antes; en P_RESULTADO, (BIESTABLE, SATURA, HOMOTOPIA); `elem_aviso` por rango (BIESTABLE y ARRANQUE, luego SATURA) | BIESTABLE llega siempre con el SATURA del mismo AO y no se veía nunca; el astable decía «U1 satura» en vez de «U1 arranca inestable» (revisión 3) |
| BIESTABLE: «… empieza en V+.»; SATURA con circuito, sin la tensión | «… empieza en V-.» si la primera solución es V−; SATURA dice «en +15 V» (el raíl de la ficha, con signo) | con raíles de 0 y 5 V la regla sale hacia V− y el texto mentía; el §3.M pide la tensión (revisión 3) |
| `modos_de` sin C ni L: sin modos y sin LU | prepara la LU igual (una factorización al empezar) y luego sale | el paso tomaba el costo de la LU de la emulación anterior: el mismo circuito cambiaba de h y daba PASO_LIM (revisión 3) |
| `SOL_NO_LINEAL` y la línea de estado mandan siempre a `[I]` (§3.O, §4.1) | solo con un `[?!]` en el valor de una E, J, R o controlada (o sin `[?!]`); con los `[?!]` solo en una C, una L o una r, «Con D1 no se despeja: quita el [?!] de C1» | `[I]` no halla el valor de una C (revisión 3) |
| `red_extraer`: RED_J_ABIERTA solo con EL_CORR; RED_FUENTE_CORTO y RED_FUENTE_AMP solo con una EL_FUENTE sin r | una J controlada cuenta como una J y una E controlada como una E sin r | la tabla daba «hay fuentes que se contradicen» con una J controlada al aire o una E controlada en corto (revisión 3) |
| `bode_aj.ent`: el índice de la fuente de entrada (Etapa 3) | también su etiqueta: en cada `[B]`, la E o J con esa etiqueta, o la automática si ya no está | al borrar un elemento de delante, el Bode tomaba otra fuente sin avisar (revisión 3) |
| los rótulos de P_EMU_ESQ con los valores en t_fin tal cual | 0 lo que cumple \|x\| ≤ 1e-12·(su máximo en la emulación), y sin «-0» en las tablas | e(t_fin) = 12·sen(12π) salía «-0,000015nV»; con 1e-12 no se pierde la fuga de un diodo en inversa (revisión 3) |

### 19.5 Verificación

- **Arnés al cerrar el paso 14** (el MOSFET): 350 056 comprobaciones, 0
  fallos (normal, ASan+UBSan y con `char` sin signo); 796 BMP de referencia
  (`bmp_ref_e4`). Secciones nuevas: [55] los tres patas; [55n] los modelos
  físicos; [56] y [57] las controladas; [58] el punto de operación de los
  diodos; [59] y [60] el transitorio; [61] el AO; [61b] paso, ventana y
  modos; [62] la pequeña señal; [63] el barrido y [63b] su pantalla; [64] el
  BJT y [64b] el MOSFET (motor, pequeña señal y familia); [65], [65b] y
  [65e] la interfaz, y [65c] y [65d] la del BJT y la del MOSFET; [66] la
  biblioteca. Luego llegaron [67], el oráculo (§19.6), y las pruebas de las
  tres revisiones (§19.7), que se imprimen como [65i] (la primera), [65j] a
  [65o] (la segunda: barr_a, barr_b, ca_res, dibujo_a, dibujo_b y editor) y
  [65p] a [65t] (la tercera: op_a, op_b, tran_a, tran_b y modelo); cada
  etiqueta sale una sola vez, y lo comprueba `compara_capa.sh`.
- **La verificación final** (rev8) dejó 10 hallazgos de integración y 2 de
  documentación ([73], con 21 mutaciones), 13 de pantallas ([74]) y 8 del
  arnés ([12a], [67b], M1-M9), todos arreglados en el cierre. [73]
  (`test_cierre.inc`, `HAKARU_SOLO_CIERRE`): 63 comprobaciones (el ritmo,
  `[ctrl]`, la paleta, la tecla mantenida, el grabador, los textos, la sesión, el
  reloj y, desde la calibración, C9, el tipo de touchpad); [74]
  (`test_r8_pantallas.inc`, `HAKARU_SOLO_PANTALLAS`): 51, al final de la
  batería y fuera de `HAKARU_SOLO_E4`. [14] suma la calibración con el banco
  (`test_calibracion`, §19.8).
- **En el estado final** (el cierre, 2026-09-27, con todo lo que vino
  después): `HAKARU_SOLO_E4` da 39 531 comprobaciones, 0 fallos: 38 633 de la
  Etapa 4, con sus revisiones y el oráculo, y 898 de lo que vino después y
  también corre ahí ([68] 99, [70] 150, las mejoras de resultados 114, [71]
  230, [72] 242 y [73] 63). Las tres revisiones solas (`HAKARU_SOLO_REV2`),
  357; la primera sola (`HAKARU_SOLO_REV`), 45. La batería entera, **351 464
  comprobaciones, 0 fallos** (también con `arm` y con `asan`, sin informes
  del sanitizador, y por la capa, `compara_capa.sh`: las mismas 855 BMP y
  476 líneas de texto, IGUALES)
  (solo cambian [10] y [10b]).
- **Mono**: 150 000 teclas por las 29 pantallas de entonces, sin desbordes;
  además, el residuo de Kirchhoff de todas las soluciones de Newton del mono
  ≤ 1e-9. Hoy recorre las 30 (con el panel Controles). Desde el cierre corre
  en su propia carpeta vacía (`out/guardados/mono`), así que la semilla sola
  repite el recorrido; tras las n teclas de siempre vienen n/4 más
  (`HAKARU_MONO_CAPA`; 0 las quita) con ráfagas de `T_REPE`, pasos del dedo
  `T_TACTIL` y `T_SUELTA`, con las guardas de entrada.c; y el residuo de
  Kirchhoff es el de toda la sesión: el barrido lleva su propio acumulador
  (`sim_residuo_barr`, su `kcl_peor`) y el mono mide con el suelo de
  redondeo de la escala de la red (`residuo_mono`; [12a], con
  `NLP_SIN_PULIDO`). `mono.sh` con un millón de teclas por cuatro semillas:
  0 fallos, 30 de 30 pantallas; `mono_desbordes.sh`, ningún texto fuera.
- **Oráculos**, sobre el árbol del cierre (con la calibración, que agranda h
  en algunas redes por el tope de 2 s): el no lineal [67], 260 de 260
  (§19.6); [29], el del tiempo lineal (`compara_oraculo.py`), 98 de 98
  iguales a 1e-3 de la escala (peor 6,2e-4, el caso_073), 16 que el oráculo
  rechaza y los modos, 98 de 98 a 1e-9 (con la calibración solo cambian el
  caso_059 y el caso_081, por el h); [29b], el de CA (`oraculo_ca.py`), 174
  de 174 (fasores a 1,61e-13, tolerancia 1e-12; el Bode a 1,40e-12,
  tolerancia 1e-10), con las mismas redes y resultados byte a byte.
- **Mutaciones**: cada arreglo de las revisiones se comprobó deshaciéndolo:
  su prueba falla (§19.7).
- **`.tns`**: el primero de la Etapa 4 (con los arreglos de la primera
  revisión), 801 952 bytes; el del estado final, con el Ndless SDK y sin
  avisos, 885 100 bytes sin los extras de la versión especial (§25).

### 19.6 El oráculo no lineal [67]

El paso 15 del plan. [67] (`test_e4_ora.inc`, `HAKARU_SOLO_ORA`) arma redes al
azar con una semilla propia (las secciones de después ven el mismo azar que
sin ella), las resuelve con el motor y las escribe en `out/oraculo_nl`: la red
en CESNET 3 (`caso_NNN.net`) y lo que da el programa (`caso_NNN.ces`).
`herramientas/oraculo_nl.py` (numpy y SciPy) las resuelve con su propio MNA
no lineal, escrito aparte a partir de las ecuaciones de la especificación y
sin mirar el motor, y compara. En el arnés solo se comprueba que el motor
acepta las redes.

- **200 redes de CC**, del todo al azar: R, E (ideal o con r) y J, diodos
  exponenciales y de catálogo, zener exponencial y 1N4733A, LED,
  amperímetros y voltímetros, E y J controladas, AO ideales y reales con
  realimentación negativa, positiva o ninguna, BJT y MOSFET (en la última
  corrida, entre las 200, según el arnés: R 1743, D 263, AO 126, BJT 143 y
  MOS 58).
  El oráculo prueba cada combinación de tramos de los AO (así halla las
  varias soluciones de un Schmitt), con `root` (hybr) desde 0, desde la del
  programa perturbada y con su propia rampa de fuentes, y pule con Newton.
  Pasa si la del programa es una de las suyas a 1e-9 de la escala y su
  residuo en las ecuaciones del oráculo es ≤ 1e-10.
- **60 redes en el tiempo**: un núcleo pasivo (R, C, L, diodos con su R,
  interruptores, E y J con forma) y etapas que lo leen sin realimentarlo (un
  AO inversor o no inversor, un emisor común, una controlada). El oráculo
  integra con Radau (rtol 1e-10) partido en los flancos y en las
  conmutaciones, y compara en los instantes que calculó el motor: el peor
  error ≤ 5e-3 de la escala y el 90 % de los instantes ≤ 1e-3. Con un
  cuarto del paso (`caso_NNN_h4.ces`), lo mismo, y dice cuánto baja el
  error. Se descartan las redes que el motor avisa como degeneradas (el
  estado salta en 0⁺).
- Sin diodos por tramos (ideal, con Vγ, zener de libro): los prueban los
  casos cerrados del arnés.

**La primera corrida** (con el motor del paso 14 y los arreglos de la
primera revisión): CC 200 de 200 y tiempo 59 de 59 (una excluida por
nombre), 259 de 259. Dos hallazgos del motor:

- **B, arreglado**: un LED azul o blanco (Is 1,17e-18, n 3) con un escalón:
  Newton no convergía tras el flanco (12 iteraciones; partir el paso no
  ayuda). `un_paso_nl`, antes de dar el error, reintenta con 50 iteraciones
  (lo que convergía no cambia), y la revisión 3 hizo lo mismo en
  `localiza_nl`. Con él, el generador del tiempo ya no rechaza ninguna red
  por no converger.
- **A**: tras un flanco, con un modo más rápido que el paso, el arranque de
  Euler de h/8 dejaba un error de primer orden que el trapecio arrastraba
  (el caso_204, una BJT: 3,7 % en la y⁺ y 5,7 % después; era la excluida).
  Es del motor lineal de la Etapa 2, y lo arregló el tramo rígido (SDIRK2
  tras cada evento con un modo de τ < 5h: §17.3, con su banco [68]).

**Con el tramo rígido y todo integrado: 260 de 260 iguales, sin excluir
ninguna.** CC, 200 de 200 (peor diferencia 2,78e-11 de la escala; peor
residuo del programa en las ecuaciones del oráculo, 2,76e-13; en 10 redes el
oráculo halla varias soluciones). Tiempo, 60 de 60 (peor error 6,09e-4 de la
escala, el caso_240; el 100 % de los instantes ≤ 1e-3); con un cuarto del
paso, 60 de 60 (peor 5,28e-4, el caso_225, en la banda de §17.3), y el peor
error baja una mediana de 15,7 veces (el trapecio: 16). El generador de CC
hizo 209 intentos: 5 eran lazos de fuentes, amperímetros o interruptores
(no valen) y 4 no convergen, que se exportan aparte y no cuentan. En tres
de ellas el oráculo tampoco halla solución; en una (la 099) sí la halla, y
el motor dice «no converge: Newton se atasca en D3… pon una R en serie con
D3» (§19.9). Las del tiempo: 60 en 63 intentos, 3 descartadas por
degeneradas y ninguna por no converger.

**La causa de la 099** (el arnés, en el cierre). «D3» es el D11 del `.net`:
sin R en serie sobre la E ideal de 16 V, u = 571 > 80 y la e^u sigue por su
recta (1,1e29 A). Su G pasa de 1e20 S, el pivote de la rama de la fuente
cae bajo `PIVOTE_NL` y la LU da singular: en el Newton, en gmin y en la
rampa desde 1,73 V. No se cambia: no es física, el umbral es la guarda de
las singulares de verdad y solo arreglaría `[P]`. [67b] fija la causa con
la red mínima (con 1 Ω en serie converge) y `oraculo_nl.py` lo anota con
`fuera_de_curva`.

**Con la calibración** (el cierre, §19.8), el tope de 2 s agranda h en 44
de las 60 redes del tiempo. CC no cambia: 200 de 200 (peor 2,78e-11).
Tiempo, 60 de 60: el peor error pasa de 6,09e-4 a 1,87e-3 de la escala (el
caso_240, con h de 12,3 a 20 µs; tolerancia 5e-3), y la menor fracción de
instantes ≤ 1e-3, del 100 % al 98,8 % (mínimo 90 %); con un cuarto del
paso, 60 de 60, peor 1,86e-4 (el caso_202), y el peor error baja una
mediana de 15,9 veces. **260 de 260 iguales.** Las 4 que no convergen son
las de siempre (079, 099, 179 y 203).

### 19.7 Revisión adversarial

Tres rondas. Cada hallazgo real se arregló con su prueba en el arnés, y cada
arreglo se comprobó por mutación: al deshacerlo, su prueba falla.

**Primera (2026-09-26).** Ocho lentes que buscan (barrido, CA, dibujo,
editor, resultados, modelo, punto de operación y transitorio) y un
refutador por hallazgo, que intenta reproducirlo con un programa propio. Se
cortó por el límite de la sesión: tres lentes (modelo, punto de operación y
transitorio) no terminaron, y de las otras cinco se rescataron 40 hallazgos:
9 confirmados, todos de la lente de resultados, y 31 que el refutador no
llegó a mirar. Lo más serio (alta): el rango del barrido de la sesión se
aplicaba a otro circuito; tras abrir el modelo 32, `[I]` en el editor barría
E1 de 10 V a 10 kV en escala logarítmica, con cientos de mA en el diodo. Los
otros ocho: `[esc]` en bucle entre P_RESULTADO y P_BARRIDO tras `[I]` y
`[tab]` (y el resultado de partida, perdido); «[I] barrer R1» que barría otro
elemento y acababa en «Para emular falta R en R1»; el error de la curva I-V
que nombraba a otro elemento; la ganancia de una controlada con la unidad
equivocada (β = «50 S»); `[P]` en una red lineal («Sin paso a paso: ? no es
lineal.»); P_RESULTADO en el cursor del barrido sin el aviso de saturación;
un residuo de Kirchhoff en la tabla del cursor que no era el de sus valores;
y la línea «se quemaría» de más de 52 columnas. Los 9, arreglados
(`firma_circ` y `barr_aj_firma`, y el `[?!]` en `barrido_de`; `res_guarda` y
`sal_barrido`; `diag_curva`; `unidad_val`; la red lineal en `res_nl`; SATURA
y `barr_residuo()` en `barr_valores`; el tope de 52 en `led_linea`), con
`test_e4_rev.inc` ([65i], `HAKARU_SOLO_REV`, que lleva también la prueba del
arreglo B del oráculo) y 17 de 17 mutaciones detectadas
(`mutaciones_rev.py`).

**Segunda.** Dos trabajos. Primero, verificar los 31 sin refutar, en seis
grupos (barrido a y b, CA y resultados, dibujo a y b, editor), cada uno en su
copia: reproducir, arreglar, probar y mutar. Tres eran el mismo fallo que un
confirmado (ya arreglado) y uno salía en dos lentes (la «r0 = ∞» de la
familia). De los otros 27, **25 eran reales y quedan arreglados** y **2 se
refutaron**; uno de los reales, el de los rótulos de la Etapa 4, eran cuatro
casos: dos se arreglaron y **dos quedan** (§19.9). Al verificar el trazador
salió otro, también arreglado: un LED de n = 2 con Vf alta (desde unos 4,2 V;
la ficha admite 6) no pasaba por (Vf, If). Lo más serio (alta): en un
Schmitt, la lectura del barrido (`barr_en`, `[N]`, `[tab]`) arrancaba de la
otra rama entre el punto medio del intervalo del salto y el umbral: la ida
leía −15 V en 7,48 V y el umbral salía en 7,4798 V en vez de 7,5 (ahora sale
del punto de antes en el sentido del recorrido). Los medios más visibles: el
trazador de curvas con el rango fijo de −2 a 2 V (un diodo de Vγ = 3 V daba
2 pA: «no conduce»); el Bode y P_CA sin el aviso SATURA (el comparador salía
«Salida nula: no depende de la entrada»); la P media y la P máx del emulador
sin el segundo puerto del BJT y del AO (un BJT saturado con la P máx menor
que la final); el triángulo de Boucherot, que contaba el AO y las
controladas como cargas (la cadena acababa en el doble del total); la tabla
de P_CA con Z = 0 en los no lineales y sin su región; el aviso de P_EMU_ESQ
que nombraba a otro elemento («C1 satura»); y los errores de la emulación
con «?» por nombre. Pruebas: `test_rev2_{barr_a,barr_b,ca_res,dibujo_a,
dibujo_b,editor}.inc` ([65j] a [65o]). Los dos refutados: el
rótulo del AO resuelto que corta los lados del triángulo (lo pide la
especificación, con fondo para que se lea) y el texto del emulador en el
punto de operación y en el barrido (el mismo SIM_FALTA de las otras
pantallas, que nombra bien lo que falta).

Segundo, volver a lanzar las tres lentes que no terminaron: 15 hallazgos
nuevos (punto de operación 6, transitorio 6, modelo 3), que verificó la
tercera.

**Tercera.** Los 15, en cinco grupos (punto de operación a y b, transitorio
a y b, modelo): **20 veredictos, 18 reales y arreglados** (seis de ellos,
cabos sueltos que salieron al verificar) y **2 que ya arreglaba** el
reintento de 50 iteraciones del arreglo B. Otros dos de los 15 eran el LED
de Vf alta, que traían dos lentes y ya había arreglado la segunda. Lo más
serio (altas):

- dos AO ideales con uno dentro del lazo del otro (un seguidor en el lazo,
  el detector de pico con seguidor): la regla de la realimentación veía S = 0
  y echaba al AO del lineal; «no converge», con la culpa en U2, en circuitos
  de libro (27 de 3000 redes al azar con AO ideales). Ahora decide el primer
  término no nulo de S(ε) (`ideal_vale`; §19.4);
- un diodo con Vγ y rd vacía (1 mΩ) se quedaba en conducción con corriente
  inversa, como una pila de 0,7 V: 40 mV de error en un regulador con
  divisor, con la región «corte» en la tabla (53 de 480 combinaciones de una
  plantilla, 13 de 30 000 redes al azar). `al_reves` lo saca del tramo, en
  Katzenelson y al final de un paso congelado;
- el superdiodo (AO ideal y 1N4148): `localiza_nl` abortaba la emulación en
  cuanto un Newton de prueba no convergía; ahora reintenta con 50
  iteraciones, como `un_paso_nl`;
- el interruptor con BJT (o un LED) sin C ni L y una cuadrada moría en el
  primer flanco: ya lo arreglaba el reintento del arreglo B, y queda su
  prueba.

Los medios: el residuo tras gmin («Kirchhoff 9,9E-1» en rojo sobre una
solución exacta); una unión sin R sobre una E ideal («no tiene una solución
única»); el paso sin C ni L, que dependía de la emulación anterior; la página
«Energía», que contaba el AO y las controladas como disipados; la J
controlada al aire («fuentes que se contradicen»); y la entrada del Bode, que
no seguía a su fuente al borrar. Pruebas: `test_rev3_{op_a,op_b,tran_a,
tran_b}.inc` y `test_rev3_v_modelo.inc` ([65p] a [65t]). Las
tres revisiones juntas (`HAKARU_SOLO_REV2`): 357 comprobaciones, 0 fallos.

### 19.8 Tiempos estimados en la calculadora

| Qué | Estimado |
|---|---|
| punto de operación del peor caso (32 elementos, 10 diodos, 2 AO) | ~16 ms |
| puente con 1N4007, 5 periodos | ~2,4 s (t_est 1,99 s: el tope de 2 s agranda h) |
| barrido de 281 puntos de un diodo | ~0,35 s (t_est 0,37) |
| astable (10 ms) | ~0,87 s (t_est 0,79) |
| familia de un BJT (6 curvas de 141 puntos) | ~0,83 s (t_est 2,38) |
| familia de un MOSFET (6 curvas de 281 puntos) | ~1,30 s (t_est 1,03) |

«~» es el modelo del arnés con lo medido en la calculadora
(operaciones·0,6337 µs + pasos·73,8 µs + iteraciones·250,5 µs); t_est, lo
que estima el programa para elegir el paso o la malla. Antes de calibrar
(con 0,63 µs, `NS_ITER` = 15 µs y `NS_PASO` = 20 µs) la tabla decía ~18 ms,
~1,6 s, ~0,1 s y ~0,3 s.

**La calibración** (el cierre, 2026-09-27), con la segunda corrida del banco
(«Estado a 2026-09-24», arriba). En el PC se reprodujeron las cargas del
banco con las mismas vueltas que hizo la calculadora en 4 s (el RTC va a
saltos de 1 s): 3866 en `BN_SIM`, 2727 en `BN_NL` y 459 en `BN_CA`, con las
`exp()` contadas (`nl_exps()` en nolin.c).

- `BN_SIM` sale exacto: 1516,27 operaciones por paso, lo que enseñó el
  banco; con el flop medido quedan 73,8 µs fijos por paso.
- `BN_NL`: las 2727 vueltas dan 2556 pasos (en 171 Newton parte el paso),
  7395 iteraciones, 3 091 392 operaciones y 19 972 `exp` y `log`. Con 17
  operaciones por `exp()` (`BN_EXP`: 10,768 µs) y la cuenta por vuelta,
  (4000 ms − 3 091 392·0,63369 µs − 2556·73,8 µs)/7395 = **250,5 µs por
  iteración**. La pantalla del banco decía 192,3 µs porque `banco.c`
  repartía por paso un tiempo medido por vuelta y contaba las `exp` a 30
  (con las cuentas del PC y esa fórmula salen 194,3: ~1 % de newlib frente a
  glibc en `exp` y `log`, que cambian alguna iteración).
- `BN_CA`: el banco contaba solo el primer punto (4112 operaciones), pero la
  media de los 459 es 5145,1: **1,6938 µs por operación de CA**. Un fijo por
  evaluación no cuadra a la vez con el Bode del RLC (133,33 ms con 401
  evaluaciones de 247,6 operaciones: saldría negativo), así que todo va por
  operación; el RLC queda del lado seguro (168,2 ms por sus operaciones, un
  26 % más; t_est 187,3 ms).
- Las constantes, cada una con su comentario (fecha y medida): en nolin.h,
  `NL_OPS_EXP` 30 → 17 y `NS_ITER` 15 → 250,5 µs (`NL_OPS_OP` no cambia);
  en sim.h, nuevas, `SIM_NS_OP` 633,7 ns y `SIM_NS_PASO` 73,8 µs (las usan
  sim.c, barrido.c y el arnés); en ca.h, `CA_NS_OP` 1693,8 ns (bode.c, t1 =
  operaciones·`CA_NS_OP`; `CA_NS_EVAL` se quitó); en el arnés, `NS_POR_OP` =
  `SIM_NS_OP`, `NS_PASO_E4` = `SIM_NS_PASO`, `MS_VOLCAR` 4,3384 y
  `F_CUADRO` 250 → 210 (lo que cuesta dibujar un cuadro animado en la
  calculadora frente al PC).
- El modelo, donde lo medido obligaba. `elige_paso`: un paso con solo tramos
  también pasa por `newton()` con una iteración, así que `ns_fijo` =
  `NS_PASO` + `NS_ITER`, y el tramo rígido suma `NS_ITER` por etapa y 3 en
  la y⁺ (con 15 µs no importaba). `decide_malla` (el barrido): suma por
  punto las iteraciones del primero, como mucho 4 (`IT_PUNTO`) × `NS_ITER`;
  la familia del BJT sigue en 141 puntos y la del MOSFET en 281. `banco.c`:
  todo por vuelta, las cargas fijas (las de la versión medida, así otra
  versión mide lo mismo), la media de CA sin el punto de 1 kHz y la línea
  «CA: N op. a X µs».
- Efectos. El puente de la biblioteca (el modelo 28) pasa de 2421 pasos con
  h = 49,9 µs (t_est 1,48 s) a 2034 con h = 60,2 µs (t_est 1,99 s; por el
  modelo, 2,30 s); con el paso de antes, el modelo calibrado da 2,69 s, no
  los ~3,5 s que se temían (el 1,4668 ms por vuelta del banco era con
  h = T/45 y 2,9 iteraciones por paso; el puente de la biblioteca hace
  1,83). [36], el peor caso lineal: 2000 pasos (antes 2240), t_est 1,99 s,
  que cuadra con `BN_SIM` (1,0347 ms por paso). El tope `NL_OPS_OP`: la
  huella de las 2120 redes de punto de operación de la batería no cambia
  (3293 bien, 340 cortadas por el tope, 5 singulares y 4 que no convergen,
  antes y después); la más larga que corta, 1224 iteraciones y 1,5 M
  operaciones, 1,26 s. BMP: solo cambian `620_banco_hecho` y
  `e4_res_barriendo` (el barrido llega más lejos con el mismo presupuesto).
- Pruebas que cambian a propósito: `test_nolin` (la `exp()` de 17
  operaciones; «una `exp()`» se cuenta con `nl_exps()`); [39], el límite de
  0,5 s a 1 s (el integrador RC, 0,78 s; el derivador, 0,60); el Bode del
  peor caso, t_est ≤ 4 s y ≤ 3,5 s por sus operaciones (70 puntos: 3,84 y
  3,00 s); [14], el puente ≤ 2,5 s y ≤ 1,3 veces t_est (2,43 s con 1,99), y
  el RLC y la red grande, operaciones + pasos·73,8 µs en vez del ×1,3 a ojo;
  `test_rev3_tran_b` H4, la ventana de 0,5 a 0,35 s (sus 3000 pasos se
  estimaban en ~2,5 s y saltaba PASO_LIM); `test_flanco` («lo real» con las
  constantes medidas, §17.3).
- **`test_calibracion`** ([14]) reproduce las cargas del banco y exige que
  el modelo dé los 4000 ms medidos con ±0,5 % (`BN_SIM` 4000,0; `BN_NL`
  4000,1; `BN_CA` 4000,1), que `exp()` sean 10,768 µs con ±3 % y que el
  Bode del RLC quede entre 1 y 1,5 veces lo medido. El cierre suma 18
  comprobaciones: esta, el tipo de touchpad (C9 de [73]), el diodo NR de
  50 ms, el puente con su t_est y la constante `NS_ITER`.

**OJO: el banco se corrió con USB.** Con el cable puesto el SO puede bajar
el reloj a 288 MHz; una corrida sin USB (tras apagar y encender) dio 0,46329
µs por operación, 1,37 veces menos. El modelo de hoy es, pues, pesimista;
hay que repetir el banco sin USB con el `.tns` de hoy (que ya cuenta por
vuelta y con las cargas fijas) y recalibrar.

### 19.9 Lo que queda

- **Dos casos de rótulos** (revisión 2), reales y sin arreglar:
  - la ley de una E controlada horizontal («2·V(R1)», 42 px) tapa la R
    vertical que cuelga de su borne (se ve en la BMP del arnés
    `e4_esq_todos_valores`; ningún modelo de la biblioteca tiene esa
    disposición). `rotula` pone cada rótulo en un sitio fijo con caja de
    fondo, desde la Etapa 1: evitarlo en un circuito cualquiera pide colocar
    los rótulos mirando los choques;
  - las miniaturas de P_EJEMPLOS de los modelos 39 (el astable) y 42 (el
    emisor común) se salen de su caja: quedan 40 px útiles y a la escala
    mínima (12 px por fila) hacen falta 60 y 72. Se pierden la tierra y la
    fila de abajo, y en el 39 se montan los «10kΩ» de R1 y R2. Arreglarlo es
    rediseñar P_EJEMPLOS (menos texto sobre la miniatura, o una miniatura que
    se desplace).
- **Una red de CC de [67]** (la 099) tiene solución y el motor no converge,
  ni con gmin ni con la rampa: lo dice, con el culpable y el consejo. La
  causa (§19.6): sin R en serie sobre la E ideal, la G de la unión pasa de
  1e20 S y la LU da singular; no se cambia, a propósito ([67b]).
- **El modelo de tiempos** (§19.8): una sola medida por constante
  (`SIM_NS_PASO` sale de la red de 32 elementos, y una pequeña seguramente
  paga menos: por eso el integrador RC se estima en 0,78 s; `CA_NS_OP`, del
  peor caso, y en el RLC se pasa un 26 %), y separar lo que depende del
  tamaño pediría otra línea del banco, que no tiene sitio. `elige_paso`
  subestima las redes suaves difíciles: cuenta 2 iteraciones por paso (la
  mediana del arnés es 2,0) y no prevé que Newton parta el paso (mediana
  +13 %; el puente, +26 %): el puente del arnés, 2,43 s con 1,99 estimados;
  el diodo C, 2,66 con 1,95; el peor de la batería (25 elementos, 4 no
  lineales, 2,85 iteraciones por paso), 5,6 s con 1,83; el puente del banco
  con 1 s pedido y paso automático, ~9,9 s con 1,94 (ya pasaba antes: en la
  calculadora, ~4,5 s con 1,985). El tope `NL_OPS_OP` solo cuenta
  operaciones: un punto de operación que no converge se corta a ~1,26 s, no
  a 0,95. `sin()` cuenta 30 operaciones (sim.c y el `c_ens` de
  `elige_paso`) y lo medido son 15,7; la calculadora de CA mezcla
  operaciones de CA (a 1,69 µs) con las suyas, así que su `CAL_OPS_TOPE` (5
  M, «~3,2 s» a 0,63 µs) podría llegar a ~8 s, y `e_motor` del arnés sigue
  con `NS_POR_OP`·1,3. Todo eso, con un banco sin USB (el de hoy se corrió
  con USB: §19.8).
- **Del tramo rígido**, sus límites (§17.3): la banda 5h ≤ τ < 7,5h, la
  clase decidida con el estado del punto de operación, los cambios de tramo
  sin presonda, un punto a mitad del primer h y el balance con L.
- **A propósito**: el signo del voltímetro distinto en `[=]` y en `[P]` para
  una red lineal (§19.4); «dy/dx = 2,000 kV/S» al barrer una g (la unidad
  compuesta de siempre, sin simplificar a Ω); y el barrido de un circuito con
  una fuente alterna, que da la curva de CC con esa fuente a 0 (el barrido es
  de la vista de CC).
- **En la calculadora real**, el banco se corrió con y sin el USB
  (2026-09-27): con el de con USB, la calibración del §19.8; con el de sin
  USB, la de la optimización (§23.4). Falta ver cuánto tardan de verdad el
  puente (el modelo 28) y el astable, los colores del LED y la legibilidad
  de los símbolos de 3 patas ([HOJA_DE_RUTA.md](HOJA_DE_RUTA.md#9-pendiente-en-la-calculadora-real)).
- **Vistos de paso**, sin tratar: en `mr_osc_pocas_muestras`, «máx
  −0,01pV» para un ruido de 1E-14 V; en `678_emu_sondas`, la banderita «1»
  de la sonda pisa el «1kΩ»; `702_ca_ficha_buscas_modulo` es un circuito de
  prueba viejo muy apretado; las marcas de arriba a la derecha («grabando N
  s», «shift», «ctrl») son cajas encima de la barra que `barra()` no cuenta
  al cortar el título (un título de 39 caracteres sin nada a la derecha
  llega a x ≈ 239 y «grabando N s» empieza en x ≈ 234).

## 20. La capa de entrada: teclas, repetición y touchpad

Antes, la acción ocurría al **soltar** la tecla (`wait_key_pressed`, leer,
`wait_no_key_pressed`): la latencia era lo que duraba la pulsación (80-150
ms) más el cuadro, la segunda tecla de un tecleo solapado se perdía, una
esquina o el centro del pad no daban nada y casi nada se repetía. La
especificación es el estudio `controles.json` (hardware, auditoría
de teclas, costo por pantalla y diseño); aquí queda lo que se construyó.
Del clic al píxel se estima pasar de ~100-170 ms a ~15-50 ms (el Bode, ~45).

### 20.1 Tres piezas

- **`entrada.c` / `entrada.h`, la capa**, sin hardware: se compila igual en
  el `.tns` y en el arnés. Recibe en cada tick una **foto** (las 8 medias
  palabras de la matriz en `0x900E0010` y, si toca, UN `touchpad_scan`) con
  la hora en ms, y devuelve eventos (`ent_saca`).
- **`bucle.c` / `bucle.h`, el bucle**, con la plataforma inyectable
  (`Plataforma`: foto, reloj, espera, volcado). Una vuelta (`bucle_paso`):
  la foto; hasta `BU_EVENTOS` eventos o `BU_MS`, con una foto de la matriz
  entre uno y otro; la animación (`BU_ANIMA_HZ` = 30 ticks por segundo de
  media, a plazo fijo: la hora del siguiente en ms × `BU_ANIMA_HZ`; cada uno
  cae en el primer `idle()` tras su hora, 3 o 4 ticks del SO; tras un
  atasco, sin ráfaga) o un tick de trabajo; UN cuadro si algo cambió (una
  tecla, un tick, la pantalla o `ui_rotulo`: los segundos del grabador; y
  otra foto tras el volcado); y, si no queda nada, `idle()`.
- **`main.c`** solo es el hardware: la foto, el reloj, `idle()` y
  `gfx_flush`. Al salir espera a que se suelte todo (`bucle_suelta`) para
  que el `[esc]` no le llegue al SO. El reloj en ms (el RTC va a saltos de
  1 s): **plan A**, el 2.º contador del SP804 de `0x900D0000` (libre, 32
  bits, sin interrupción; solo si al arrancar está apagado, y se restaura al
  salir si sigue como se dejó); **plan B**, las vueltas de `idle()` por
  `ENT_TICK_US` (el tiempo solo avanza esperando: en una pantalla cara la
  repetición va más despacio). El plan A está **vigilado** (`ent_reloj_*`,
  en entrada.c, sin hardware: main.c le pasa las lecturas y el arnés lo
  prueba con un contador simulado), porque nadie documenta si el SO usa ese
  contador: al abrir, 8 ticks de `idle()` tienen que medir `ENT_TICK_US`
  ± 25 % cada uno (otra frecuencia, o parado: plan B); en marcha, una
  lectura que avanza más de 500 ms (el contador reprogramado o recargado)
  cuenta un tick, y si en 2 `idle()` seguidos no avanza un tick (parado, o
  con otro divisor), plan B hasta salir. El tiempo nunca va hacia atrás ni
  salta al cambiar de plan.
- **El reloj, a la vista** (el cierre): main.c pasa a `ent_reloj_rtc`, tras
  cada `idle()`, los segundos del RTC (`0x90090000`); `ent_reloj_medido` da
  las cuentas del SP804 por segundo de RTC (desde el primer salto, y otra
  vez tras un salto del contador), el tick de `idle()` (la media de los
  deltas de un tick, ± 25 %) y los `idle()` por segundo. main.c lo publica
  con `ent_reloj_a_la_vista(&rel)` y Controles lo enseña en dos líneas
  (§20.4). El arnés no pone reloj (salvo en su prueba).
- **El tipo de touchpad** (el cierre): Ndless no lo publica. En la CX II
  envuelve `touchpad_read` según el pad (`syscalls.c`, `sc_install_compat`):
  el envoltorio del CapTIvate solo acepta las lecturas 0x04-0x07 (el tamaño)
  y las que acaban en 0x0A (el informe). `tipo_pad()` (main.c) lee 0x00-0x01
  de la página 4, tres intentos: si la lectura sale, es Synaptics; si no,
  CapTIvate; fuera de la CX II, siempre Synaptics. Lo publica
  `ent_pad_visto` (entrada.c) y Controles lo enseña.

### 20.2 Las teclas

- Una tabla física `TF_*` → {fila, bit, `T_*`} con los `tpad_row`/`tpad_col`
  de `keys.h` en el orden de prioridad de siempre (esc, tab, menu, enter,
  flechas...). Pulsada: `fila[(tpad_row - 0x10)/2] & tpad_col`.
- **Se actúa al bajar** (el flanco). Varias en la misma foto salen en orden
  de prioridad. La foto del arranque es el estado previo: el `[enter]` que
  lanzó el `.tns` no cuenta. Las teclas sin traducción (π, var, trig...) no
  generan nada (antes mandaban `T_NADA`, que gastaba el shift armado).
- `shift` y `ctrl`: abajo con otra tecla van con ella (`T_CON_*`); soltados
  sin que bajara otra, salen `T_SHIFT`/`T_CTRL` y ui.c los arma como antes;
  pulsados durante la ráfaga de otra tecla no arman nada.
- **Cola** de `ENT_COLA` = 16 flancos: llena, se pierde el nuevo, salvo
  `[esc]`: sale el más nuevo que no sea `[esc]` (otro `[esc]` que espera no
  se pisa). **Fantasmas** de la matriz: tres o más flancos nuevos a la vez
  (sin contar shift ni ctrl), o uno que cierra un rectángulo con otras tres
  teclas abajo (su fila, su columna y la esquina de enfrente: en una matriz
  sin diodos, tres teclas reales encienden la cuarta, y la real y la
  fantasma se ven igual, así que se descartan las dos). No actúan ni dejan
  armar shift y ctrl; `[esc]` sí, que nunca se pierde.
- **Repetición**: ui.c dice QUÉ se repite (`ui_repite`, una lista blanca que
  se consulta antes de CADA repetición: 0 en el borde) y la capa CUÁNDO. Las
  repeticiones no se encolan: se cuentan las vencidas por el reloj (con el
  multiplicador), con un tope de 32 por cuadro, así que soltar no deja
  inercia y un cuadro caro junta pasos. La ráfaga muere al soltar, al bajar
  otra tecla, si cambia la pantalla o si `ui_repite` da 0; si hubo
  repeticiones, manda `T_SUELTA`.

| Perfil | Retardo | Periodo | Dónde |
|---|---|---|---|
| LISTA | 400 ms | 100 ms los primeros 600 ms, luego 50 | menús, Taller, Mis, lecciones, ejemplos, tablas, pasos, ↑↓ de la ficha y de los ajustes, paleta, rejilla del editor |
| CURSOR | 350 ms | 40 ms, ×1/×2/×4/×8 por segundo mantenido (tope 8; Ayuda 3; con shift, 1) | ←→ del Bode, del barrido y de los cursores del osciloscopio; ↑↓ de la Ayuda |
| ESCALA | 500 ms | 180 ms | V/div, t/div, dB/div, zoom, escala del fasorial, la ventana del osciloscopio |
| BORRAR | 450 ms | 70 ms | `[del]` con texto escrito (ficha, entradas del osc., del Bode y del barrido) |
| (especial) | 150 ms | 50 ms | el perfil de la versión especial (`REP_ESP_*`, §25) |

Nunca se repiten `esc`, `enter`, `tab`, `menu`, catálogo, `=`, `?!`, `EE`,
`.`, `(-)`, cifras ni letras; `[del]` en Mis (dos borran) ni en la ficha sin
texto (libera la magnitud); ←→ de la ficha (la variante borra datos); lo
cíclico (capa del resultado y de la CA, páginas de las tablas, vista del
fasorial, opción y campos con vuelta de los ajustes, menú de opciones); ↑↓
de soluciones; nada con `ctrl`. En el editor, las flechas repiten sin menú
abierto y con la acción nada o cable (o con la paleta); `R` y una flecha
ponen la pieza y abren la ficha: la ráfaga muere ahí. El ajuste global
(Controles) multiplica retardos y periodos: lenta ×1,5, rápida ×0,6, o no.

### 20.3 El touchpad

- **Clic**: la flecha sale del único report, al hundir: u, v en [−1, 1]
  (y hacia arriba); centro si u² + v² < 0,30² (≈7 % del pad, frente al 11 %
  de los tercios de libndls), si no el eje dominante, así que las cuatro
  esquinas dan flecha. Con el pad hundido la flecha solo cambia si el dedo
  pasa a otra cuña con 0,10 de margen durante 2 muestras (de bajar a mover sin soltar). Clic
  central = `[enter]` (o nada, en Controles). Un scan que falla conserva la
  muestra anterior hasta 3 veces; a la 4.ª el pad se da por suelto (ninguna
  flecha pegada repitiendo).
- **Deslizar** (un reconocedor por contacto): empieza con 2 muestras con
  contacto y acaba con 3 sin él (un parpadeo no parte el gesto); nada hasta
  alejarse un 8 % del ancla; al arrancar se bloquea el eje que recorrió el
  doble que el otro; un paso por cada «paso» de recorrido, y para volver
  atrás el paso más una histéresis (25 % del paso o 1,5 %); balística en los
  ejes finos (×1, ×2 o ×4 según la velocidad de las 3 últimas muestras;
  tope 8 pasos por muestra); un salto de más del 40 % re-ancla sin pasos;
  con el pad hundido se congela, y al soltar el clic hay 150 ms de bloqueo y
  se re-ancla allí. Los pasos son flechas `T_TACTIL | T_REPE` (con shift o
  ctrl si están ABAJO) que pasan por las teclas de siempre y por
  `ui_repite`; al levantar el dedo, `T_SUELTA`. «Pasar página»
  (`uno_por_contacto`): un 25 % en 350 ms desde que el dedo empezó a
  moverse da un paso y ninguno más en ese contacto.
- **Buscar la flecha**: quien apoya el dedo en el centro y lo lleva a una
  cuña para hundirla recorre un 33 %, que eran 2 o 3 pasos de más (y en la
  ficha, campos confirmados). En los perfiles sin balística (listas, la
  ficha, la rejilla, los pasos, las páginas) los pasos de cada arranque
  esperan hasta `TP_BUSCA_MS` = 200 ms desde que el dedo echó a andar: si
  en ese tiempo se hunde el pad (o baja otra tecla), se tiran y solo cuenta
  la tecla; si no, llegan todos juntos, y al levantar el dedo, en seguida.
  Con balística (Bode, barrido, cursores del osciloscopio, Ayuda) no se
  espera: ahí un paso de más solo mueve un cursor.
- **La deriva**: sin haber arrancado, si el dedo sale de su caja de quietud
  (2 %) tras estar en ella `TP_DERIVA_MS` = 400 ms o más, el ancla pasa a
  donde estuvo quieto. Un pulgar apoyado que resbala despacio (menos de un
  5 %/s) nunca suma el 8 %; un deslizamiento de verdad sale de la caja en
  unas pocas muestras y cuenta desde donde se paró. Y una tecla de la
  matriz, además de cortar los pasos pendientes, re-ancla el dedo apoyado:
  tras teclear hay que volver a recorrer el 8 %.
- **A medio escribir** un valor en la ficha o en los ajustes, el dedo no
  cambia de campo (lo confirmaría): `ui_tactil` da 0 si ya se escribía al
  apoyarlo y `ui_repite` no deja pasar el paso si se empezó a escribir con
  el dedo apoyado.
- **El tamaño del pad**: la x y la y se acotan al pad, y así u y v de la
  cuña no pasan de ±1000 sea cual sea W (antes, una x de más con un W
  pequeño desbordaba u² + v² en el `long` de 32 bits del ARM). Un W o un H
  de `touchpad_getinfo` fuera de 100..16384 no es creíble y se usan los de
  omisión; el mínimo es bajo a propósito, porque el CapTIvate de la CX II
  no publica su tamaño y podría ser pequeño. Con W ≤ 16384 la histéresis
  (paso × 250) cabe en 2^31. Controles enseña el tipo de pad (Synaptics o
  CapTIvate, por el envoltorio de Ndless: §20.1) y su W×H, o «sin tamaño
  (usa …)» con el de omisión que usa la capa.
- `ui_tactil` da el perfil de la pantalla (se pregunta al empezar el
  contacto; si la pantalla cambia, el gesto se anula hasta levantar):

| Pantalla | Deslizar |
|---|---|
| editor sin acción o con `W` | 2D, W/10 × H/7 (la rejilla); con `W`, un gesto es UN nivel de deshacer |
| editor: paleta / menú de opciones | 2D W/4 × H/6 / vertical H/8 |
| editor con poner, editar, sonda o borrar | apagado (una flecha deliberada) |
| osciloscopio | horizontal: con cursores W/40 y balística ×4; sin ellos W/10 (una división); con disparo, uno por contacto; vertical apagado |
| Bode, barrido | horizontal W/40 con balística ×4; vertical apagado |
| menú, Taller, Mis, lecciones, ejemplos, ajustes, tablas | vertical H/8 (los ajustes, no a medio escribir) |
| Ayuda | vertical H/12 con balística ×3 |
| ficha | SOLO vertical, H/6 (de campo en campo); no a medio escribir |
| pasos | horizontal W/6 |
| teoría, resultado, CA | horizontal, uno por contacto |
| tablas de CA y de la emulación, fasorial | vertical H/8; horizontal, uno por contacto |
| portada, banco, emulando, modelo, Thévenin | apagado |

### 20.4 ui.c

- **Marcas** (`ui.h`): `T_REPE` 0x400 (no es el primer evento de su
  ráfaga), `T_TACTIL` 0x800 (del pad) y `T_SUELTA` 0x1000 (fin de ráfaga:
  `ui_tecla` lo atiende arriba del todo y no toca `vueltas` ni `mod_pend`).
  Una repetición lleva los modificadores del primer evento (`mods_rafaga`;
  con shift armado, mantener → en el barrido repite saltos de 28 puntos);
  un paso del dedo, los que están abajo; ninguno gasta ni arma los
  pegajosos.
- **Lecturas «≈»**: en el Bode y el barrido, un `T_REPE`/`T_TACTIL` pone
  `rafaga` y el panel lee de la malla (`traz_valor`, `cv_valor`) con «≈»;
  `T_SUELTA` la apaga y el cuadro siguiente hace la exacta (`bode_en`,
  `barr_en`). Un toque suelto siempre es exacto.
- **Historial**: una ráfaga con `W` (o un gesto: el primer `T_TACTIL` tras
  algo que no lo era empieza otro) es un solo `guarda_hist`: un cable largo
  se deshace de una vez y no gasta los 12 niveles.
- **La precondición**: las cotas del scroll estaban en el dibujo (Ayuda,
  `P_TABLA`, la tabla no lineal, la de la emulación y `emu_lineas`) y
  `ui_tecla` dependía del último `ui_dibuja`. Ahora son de las teclas
  (`ayuda_max`, `tabla_max`, `emu_lineas_de`, `cotas_vista` tras cada
  tecla; los 3 campos que se ven de la ficha, `acota_campos`); el dibujo las
  repite y son idempotentes. `osc_prepara` da `n_med` sin dibujar (el `[=]`
  del osciloscopio). Las pantallas animadas sacaban azar al dibujar: ahora
  sale del cuadro (`ruido`), y dibujar no cambia nada.
- **Saltos** (una regla: shift = paso grande, ctrl = al extremo; ctrl nunca
  repite):

| Dónde | shift | ctrl |
|---|---|---|
| editor | hasta el siguiente punto con borne, cable o caja, o el borde; con `W`, el cable hasta ahí en UN nivel (se para antes de sustituir) | hasta el borde de la rejilla |
| Ayuda, tablas, Mis, lecciones, ejemplos | una página (15 líneas en la Ayuda; ←→ también pasan página) | principio o final |
| osciloscopio | como estaba | la ventana al principio o al final del registro; con cursores, el cursor al borde de la ventana |
| Bode, barrido | como estaba (una década, 28 puntos) | el cursor al borde de la ventana; otra vez, al extremo del barrido |
| pasos, teoría | — | el primero o el último |

- **Panel «Controles»** (`P_CONTROLES`): `[ctrl][menu]` desde cualquier
  pantalla (salvo con trabajo en marcha) con deslizar sí/no, repetición
  no/lenta/normal/rápida y clic central `[enter]`/nada; `[enter]`, `[esc]` o
  `[menu]` guardan y vuelven. Se guarda en `ces_controles.tns` (8 bytes:
  «CESK», versión 1 y los tres ajustes) con `arch_bytes_*` de archivo.c; si
  falta, los valores de `entrada.h`. `main.c` lo abre al arrancar;
  `ui_init` no (el arnés empieza siempre con los de omisión). También con
  `[ctrl]` pegajoso: en la portada y en el banco hecho se arma solo
  `[ctrl]` (no `[shift]`) y lo gasta la tecla
  siguiente. Abajo, lo que se calibra: el touchpad («Touchpad: CapTIvate,
  sin tamaño (usa 2328x1691)», o «Synaptics, W×H») y el reloj de la capa
  («Reloj de la capa: plan A; cambios 0, saltos 0» y «SP804 32768/s; tick
  10250 µs; idle 97,6/s (N s)»).
- **El grabador** (oculto: `[G]` en Controles): 10 s de muestras crudas
  (hora, las 8 filas, el pad) en `ces_muestras.tns` (formato «CESM», 27
  bytes por muestra). `HAKARU_GRABACION=archivo` las reproduce en el arnés por
  la capa y enseña sus eventos: los umbrales se ajustan con datos reales.
  Sigue en cualquier pantalla (a propósito): arriba a la derecha, en rojo,
  «grabando N s» (en el panel, su línea). El bucle repinta cuando cambia
  `ui_rotulo()`, una vez por segundo y al acabar, no por tick, para no
  quitarle muestras.
  El bucle hace 2 o 3 fotos por tick con actividad (al empezar, entre
  eventos y tras el cuadro): las que no cuentan (sin pad, la matriz de la
  anterior y nada que sacar a esa hora) no se graban, porque no cambian el
  estado de la capa ni el bucle entrega nada en ellas; reproducir sin ellas
  da los mismos eventos. Con una ráfaga salen ~125 por segundo: caben en
  `GRAB_MAX` = 2000 (54 KB).

### 20.5 Energía

Toda espera es `idle()` (el mismo WFI de `wait_key_pressed`). La matriz se
lee en cada tick; el pad (I2C), según la actividad: cada tick con algo
abajo, un dedo, una ráfaga o menos de 1 s desde lo último (≈98 por
segundo); de 1 a 30 s, cada 2 (≈49); después, cada 4 (≈24); un dedo quieto
más de 5 s, cada 2. Sin eventos no hay cuadros; en una ráfaga, como mucho
uno por periodo. Emulando, la barra se pinta como mucho cada 200 ms (o cada
6 ticks de trabajo, con el reloj del plan B) y el tick de trabajo baja a
`BU_OPS_TICK` = 50 000 operaciones (~32 ms) para que un `[esc]` corto se
vea; las animaciones van a 30 ticks por segundo de media (3 o 4 ticks del
SO, §20.1) sin `msleep` (que reprograma el temporizador del SO).

### 20.6 Desviaciones del diseño

| Diseño | Hecho | Por qué |
|---|---|---|
| bajar `SIM_OPS_TICK` a 50 000 | `ui_trabajo(BU_OPS_TICK)` desde `main.c`; el arnés sigue con 150 000 y el primer trozo, en la tecla, también | `sim.c` no se toca (otro trabajo en curso); el arnés prueba que el emulador, el Bode, el barrido y la calculadora de CA dan lo mismo con los dos troceos |
| la barra cada ~200 ms | o cada 6 ticks de trabajo | con el plan B el reloj no avanza mientras se trabaja |
| recorrido en milésimas por muestra | en unidades del pad ×1000 | truncar a milésimas en cada muestra perdía 2 de cada 12 pasos en un deslizamiento lento |
| «pasar página» desde el ancla | desde que el dedo empezó a moverse | un dedo apoyado un rato antes de deslizar no pasaba página |
| ↑↓ de los ajustes, LISTA | solo con el dedo en los contextos con vuelta (CA, barrido) | mantener ↓ daba vueltas a los campos |
| un gesto con `W`, un `guarda_hist` | y el primer paso de un gesto empieza ráfaga aunque antes no hubiera `T_SUELTA` | tras un toque suelto con `W`, el gesto se sumaba a su nivel |
| `efk_controles.tns` | `ces_controles.tns` | el prefijo tras el cambio de nombre |
| — | `T_SUELTA` solo si hubo repeticiones o pasos | un toque suelto ya es exacto |
| — | la foto del estado (`ui_foto`) compara los textos hasta su final, el circuito sin huecos y `poner_tipo` solo con `ACC_PONER` | lo de detrás son restos de lo anterior, no estado |
| el ancla, la primera muestra | la última posición en la que el dedo estuvo quieto 400 ms (sin arrancar); y una tecla de la matriz re-ancla | un pulgar que resbala despacio acababa pasando del 8 % (revisión) |
| el 8 % evita los accidentes | y los primeros pasos sin balística esperan 200 ms: un clic en ese tiempo los tira | llevar el dedo del centro a la cuña para hundirla daba 2-3 pasos de más (revisión) |
| la ficha: vertical, H/6 | no a medio escribir (tampoco los ajustes) | cada paso confirmaba lo escrito (revisión) |
| fantasmas: 3 flancos a la vez | y el rectángulo de 3 teclas abajo; `[esc]` pasa siempre | el fantasma típico (2 abajo y 1 nueva) daba 2 flancos; y se tragaba `[esc]` (revisión) |
| el plan A si el contador avanza en 2 `idle()` | calibrado en 8 ticks y vigilado (saltos y paradas) | un salto vaciaba un texto con `[del]`; parado, se congelaban las animaciones y el grabador (revisión) |
| el grabador, todas las fotos | las que cuentan, y 2000 | 1100 se llenaban antes de los 10 s (revisión) |

No se hizo: la confirmación en «Nuevo» con un circuito sin guardar (riesgo
3, aparte; el de antes sí sobrevive a salir con el editor vacío: la sesión),
la ventana de un tick para los acordes (riesgo 9, «si
molesta»), la caché de la miniatura de Mis (riesgo 11: el perfil LISTA ya
va a ≤ 20 cuadros por segundo), el estado de interrupción del pad en
`0x900E0040` y `get_event`.

### 20.7 Verificación

- **[71]** (`test_entrada.inc`, `HAKARU_SOLO_CAPA`): 230 comprobaciones en
  tiempo virtual (una mano con las teclas `TF_*` y bits crudos, un dedo, un
  reloj en µs; la espera salta al tick de 10,25 ms y el volcado suma el costo
  medido de cada pantalla). La independencia del dibujo compara el estado
  tras CADA una de 200 teclas al azar con y sin `ui_dibuja` en 20 pantallas;
  y las pantallas animadas, 600 ticks con uno y con dos cuadros por tick;
  flancos, fantasmas, shift y ctrl, la repetición contra la fórmula del
  perfil (y sin inercia), el Bode a 42 ms por cuadro (168 puntos en 3 s, lo
  mismo que a 5 ms, y 67 cuadros), la lista negra, el cable, el touchpad
  (también con un pad de 1000×700), la energía (98,8, 48,8 y 24,4 lecturas
  por segundo en reposo; 0 cuadros), la cola, los troceos, los
  saltos, el panel y el grabador (grabar 10 s y reproducirlos da los mismos
  eventos). La sección 12, lo de la revisión (§20.9): buscar la flecha en
  el editor, Mis y la ficha (un paso, el del clic), un deslizamiento de
  verdad que espera y llega entero, el Bode sin espera, la ficha a medio
  escribir con el dedo apoyado antes y después; la deriva de 25 % en 30 s
  en la ficha y en Mis, y la tecla que re-ancla; el reloj vigilado con
  lecturas simuladas (32 768 y 32 000 Hz sí; 12 MHz, un 40 % más rápido,
  parado o con un tick raro, no; 1 s por el cero, la recarga y el salto de
  60 s, un tick; parado y a 1/16, el plan B sin volver atrás) y en el bucle
  con la plataforma de main.c (el `[del]` con un salto de 60 s, una
  animación con el contador parado); los fantasmas (`[esc]` pasa, el rectángulo no, una
  tercera sin rectángulo sí), dos `[esc]` con la cola llena, 17 `[esc]`
  seguidos; el grabador (las fotos que no cuentan, 9 s de ráfaga con sitio,
  los 10 s enteros y la reproducción); y el tamaño del pad (1×1, 60000,
  0×50, una x de 65535, un pad de 256×192).
- **La regresión por la capa** (`build_host.sh capa`, `compara_capa.sh`):
  con `-DPOR_LA_CAPA`, `TECLAS(...)` pulsa cada tecla con la mano (las
  flechas, un clic en el centro de su cuña; `T_CON_SHIFT`, shift abajo) y el
  bucle la entrega; cada tecla tiene que dar exactamente ese evento. La
  suite entera: 12 469 teclas (6 543 clics), las mismas 351 165
  comprobaciones sin fallos y las mismas 839 BMP byte a byte (salvo la del
  banco, que mide tiempos del PC). `compara_capa.sh` compara también el
  texto de las dos salidas, sin las líneas con « ms» ni el informe de la
  capa (así cazó el fijador de [69], que leía las medidas sin redibujar), y
  que cada etiqueta de sección salga una sola vez; en el cierre, 351 464
  comprobaciones, 855 BMP y 476 líneas iguales.
- **Mutaciones** (`mut_capa.py`, `mut_corre.sh`): 37 cambios que deshacen
  una decisión (las cotas del dibujo en la Ayuda, la tabla y la emulación, el
  `[esc]` que se pierde, sin la foto del arranque, los modificadores de la
  ráfaga, `[del]` en Mis, la variante, los campos con vuelta, el truncado,
  los fallos del pad, la energía, `T_SUELTA`, los tercios, el margen de la
  cuña, el historial del gesto, la inercia, el re-anclaje, el eje, los
  saltos, los fantasmas, el parpadeo, «pasar página»; y de la revisión: sin
  la espera de buscar la flecha, un clic que no la corta, sin la deriva, la
  tecla que no re-ancla, el dedo a medio escribir, el fantasma que traga
  `[esc]`, sin el rectángulo, el `[esc]` que pisa a otro, grabar todas las
  fotos, el reloj sin saltos, sin lentos y sin calibrar, el pad sin cota):
  las 37 mueren, cada una en la comprobación que se espera.

### 20.8 Lo que se calibra en la calculadora

Todos los números de `entrada.h` son estimados y tienen nombre. En la
calculadora real (el banco, o el grabador y luego el arnés):

- el reloj: ¿está apagado `0x900D0028` al arrancar?, ¿cuánto avanza el 2.º
  contador del SP804 por segundo de RTC (`ENT_SP804_HZ`: 32 000 o 32 768)?,
  y que tras salir el SO siga bien (hora, USB, apagado); si no, plan B
  (`ENT_RELOJ_SP804` 0). La vigilancia: que la calibración de 8 ticks
  (`RELOJ_CALIBRA`, `RELOJ_TOLERANCIA`) no caiga al plan B sin motivo en
  ninguna calculadora (Controles: el plan y sus cambios), y `RELOJ_SALTO_MS`
  frente a lo más largo que bloquea el programa
  sin mirar el reloj (abrir o guardar un archivo, el banco);
- el tick de `idle()` (`ENT_TICK_US`), y lo que cuestan un `touchpad_scan` y
  la foto de la matriz. `ENT_SP804_HZ` y `ENT_TICK_US` se leen en Controles
  (las cuentas por segundo de RTC y el tick medido; mejor tras un minuto
  quieto);
- el pad de esa calculadora: W y H, Synaptics o CapTIvate (Controles lo
  dice: §20.3), informes por
  segundo con el dedo en movimiento (de eso depende la balística), el ruido
  del dedo quieto, la deriva al hundir y al soltar y los clics en las
  esquinas (con el grabador: `TP_ARRANQUE`, `TP_HIST_*`, `TP_VEL_*`,
  `TP_SALTO`, `TP_CENTRO`, `TP_CUNA_MARGEN`); cuánto tarda de verdad un
  alumno en llevar el dedo del centro a una cuña y hundirla (`TP_BUSCA_MS`:
  con 200 ms se cubre quien lo hace en ~170 ms; más, y el primer paso de un
  deslizamiento se nota tarde) y a qué velocidad resbala un pulgar apoyado
  (`TP_DERIVA_MS`);
- la sensación, con el alumno: retardos y periodos (`REP_*`), el 8 %, los
  pasos, las cuñas y si se lee bien el «≈»; y la latencia real con vídeo a
  240 cuadros por segundo, los toques cortos de `[esc]` emulando y en el
  peor punto de CC, y la batería;
- si la matriz tiene fantasmas de verdad (tres teclas en rectángulo con el
  grabador: ¿se enciende la cuarta?). Si no los tiene, el filtro del
  rectángulo solo quita la 4.ª tecla de un rectángulo real, que nadie pulsa.

### 20.9 La revisión de la capa

Un revisor (sondas en `rev_entrada/rv_main.c`, con la mano virtual del
arnés) encontró 2 problemas medios y 8 bajos. Arreglados, con su prueba en
la sección 12 de [71] (o en [72]) y su mutación:

- **Deslizamientos accidentales** al buscar la flecha con el dedo (medio):
  la espera de `TP_BUSCA_MS` (§20.3); la ficha y los ajustes, sin dedo a
  medio escribir. Del centro a la cuña en 150 ms y clic: el editor, Mis y
  la ficha dan 1 (antes 4, 3 y 3, y el 47 confirmado).
- **El reloj del plan A sin vigilancia** (medio): `ent_reloj_*` (§20.1).
  Con `[del]` mantenido y un salto de 60 s: un borrado (antes, el texto
  vacío); con el contador parado, a los 2 `idle()` el plan B y la
  animación sigue.
- **Nombres del usuario sustituidos** (bajo): `guardar(copia, propio)`: lo
  escrito con `[ctrl][N]` se guarda tal cual; y un nombre del programa es
  «Problema nuevo» sin hueco o «Mi problema N» en el hueco N − 1 (§21.4).
- **Deriva lenta del pulgar** (bajo): el ancla sigue al dedo quieto y una
  tecla re-ancla (§20.3).
- **El filtro de fantasmas** tragaba `[esc]` y no veía el rectángulo
  (bajo); **dos `[esc]` con la cola llena** (bajo, teórico); **el grabador**
  se llenaba antes de 10 s (bajo); **`num_eval`** daba infinito con `NE_OK`
  para un número solo y escondía un infinito a medias (bajo, §21.2); **el
  tamaño del pad** sin cota (bajo, §20.3).

No se arregla, y por qué:

- **Dos teclas que bajan en el mismo tick** salen en el orden de prioridad,
  no en el de tecleo (9 y 1 solapados a 4 ms dan «19»). Es del sondeo: las
  dos aparecen en la misma foto y no hay nada que diga cuál bajó antes. Leer
  la matriz más a menudo que el tick del SO costaría batería por un caso de
  4 ms; el diseño ya lo aceptaba (riesgo 9).
- **Buscar la flecha despacio**: si el dedo tarda más de `TP_BUSCA_MS` en
  llegar a la cuña y hundirla, los pasos ya pasaron. Medido con la mano
  virtual (del centro a la cuña en tantos ms y clic): hasta 210 ms, el
  editor, Mis y la ficha dan 1; con 250 ms o más, lo de antes (4, 3 y 2). Esperar más retrasa todos los
  deslizamientos; lo que se hace es calibrarlo con el grabador, y
  «Deslizar: no» en Controles quita el problema de raíz. En la ficha a
  medio escribir no hay daño: el dedo no confirma.
- Las sondas del revisor sobre el reloj (`rv_reloj`) saltan y paran el
  reloj de la plataforma directamente, por debajo de `ent_reloj_*`: siguen
  dando lo mismo, porque el arreglo está en la cuenta que main.c le da al
  bucle. La prueba que cuenta es la de [71] con el contador simulado.

---

## 21. Cuentas en la ficha, copiar y pegar, y nombres

Tres propuestas de la lente «flujo» (`rev2/mejoras.json`) que esperaban a la
capa de entrada: necesitaban teclas que main.c no traducía (× ÷ ^ x² ( ) y el
espacio) y combinaciones con `ctrl` sobre letras que ya eran componentes (C,
V, X, N). Con la tabla `TF_*` de §20 son unas filas más.

### 21.1 Las teclas nuevas

| Tecla | `keys.h` (tpad) | Fila y bit | Código |
|---|---|---|---|
| espacio | `KEY_NSPIRE_SPACE` 0x10 / 0x010 | 0 / 0x010 | `T_ESPACIO` |
| × | `KEY_NSPIRE_MULTIPLY` 0x18 / 0x100 | 4 / 0x100 | `T_POR` |
| ÷ | `KEY_NSPIRE_DIVIDE` 0x16 / 0x100 | 3 / 0x100 | `T_ENTRE` |
| ^ | `KEY_NSPIRE_EXP` 0x18 / 0x200 | 4 / 0x200 | `T_POT` |
| x² | `KEY_NSPIRE_SQU` 0x14 / 0x200 | 2 / 0x200 | `T_CUAD` (escribe `^2`) |
| ( y ) | `KEY_NSPIRE_LP` / `RP` 0x1A / 0x020, 0x008 | 5 / 0x020, 0x008 | `T_PAR_A`, `T_PAR_C` |

Los códigos van DETRÁS de las letras (`T_LETRA0 + 26` a `+ 32`, por debajo de
0x100), no delante de `T_DIG0` como decía la propuesta: así no se corre
ningún código de antes (ni la semilla de las animaciones, `vueltas`, que los suma,
ni las pruebas que los nombran). En la prioridad van tras el catálogo y antes
de shift y ctrl. Ninguna está en la lista blanca de `ui_repite`: mantener ×
escribe uno. La prueba de «las teclas sin uso no mandan nada» de [71] usa
ahora trig en lugar de ×.

### 21.2 Cuentas en la ficha

- **`num_eval`** (numfmt.c): descenso recursivo sobre los números de
  siempre. `suma = producto {± producto}`, `producto = unario {×÷ unario}`,
  `unario = -unario | potencia`, `potencia = primario [^ unario]` (a la
  derecha: 2^3^2 = 512; -2^2 = -4; 2^-1 = 0,5), `primario = número | (suma)`.
  El número lo lee `lee_num`, la misma función que ahora usa `num_lee_ing`:
  con coma o punto, `[EE]` y el prefijo pegado a cada número («4,7k+1k»). Un
  número solo pasa primero por `num_lee_ing` y da el mismo double y el mismo
  «no es un número» que antes. Errores (`NE_*`): paréntesis sin cerrar, `)`
  de más, a medias («2+», «()»), dos números sin signo («(1)(2)»), división
  entre cero (y 0^-1), no real ((-8)^0,5), infinito, y un número mal escrito.
  Como mucho 40 paréntesis anidados. Solo `double`. **Infinito** es
  cualquier paso que no cabe en un double: un número solo («8E505», que
  antes salía con `NE_OK`) o a medias («(1E300×1E300)^0» no es 1, ni
  «1÷(1E300×1E300)» es 0). La ficha, con un número solo que no cabe, hace
  lo de siempre (`valor_edit`): el eco dice «inf» (lo pide R34) y al
  confirmar lo para el rango con su aviso, que dice el máximo.
- **Dónde**: en las cajas de valor de la ficha (`cuentas_aqui`): todas en
  CC y, en CA, la del módulo. No en la del ángulo, ni en la fila x de una
  controlada (el control se elige), ni en las de parte j de la CA (la Z),
  donde `[+]` y `[−]` son +j y −j: ahí × y los paréntesis lo dicen («Aquí [+]
  y [-] son la parte j: sin cuentas.»).
- **Qué entra y dónde**: un signo solo detrás de un número o de «)»; «(» al
  principio o detrás de un signo; «)» si hay uno abierto; x² como `^2`.
  `[−]` (o `(−)`): al principio, el signo del número si el campo lo admite
  (como siempre); tras la E, el del exponente (como siempre); tras un
  número, la resta; tras «(», × ÷ o ^, el signo de lo que sigue. La coma,
  `[EE]` y el prefijo miran el número que se escribe (`num_actual`), no la
  cuenta entera. Así lo escrito siempre es el principio de una cuenta.
- **La caja**: 31 caracteres (las demás cajas, los ajustes y las de parte j,
  siguen con 19); si no cabe, enseña su final por caracteres enteros tras un
  `«` (`texto_campo`, `S_LAQUO`); lo mismo en el módulo y el ángulo de la CA. El eco
  da el resultado («= 484 Ω»); a medias, «...». Mientras se escribe, la línea
  de ayuda dice «Cuentas: + - × ÷ ^ ( ) x²   [enter] la guarda».
- **Al confirmar** se guarda el resultado, con los rangos de siempre y uno
  más: una cuenta que da negativo en un campo sin signo («La cuenta da
  negativo y aquí no lleva signo.»). Si no vale, el aviso rojo y el campo
  como estaba; un número mal escrito se tira (como siempre) y una cuenta se
  queda escrita para corregirla: `[del]` quita el aviso, las flechas no
  cambian de campo, y un segundo `[esc]` la tira y sale.
- El `expr_campo` del plan original (§9.3, §11) queda así, en `num_eval`,
  sin √ ni símbolos; el `ctrl`+`N` de §9.3 (R desde los nominales) no se
  hizo: `[ctrl][N]` es ahora el nombre (§21.4).

### 21.3 Copiar, cortar y pegar

- `[ctrl][C]`, `[ctrl][X]` y `[ctrl][V]` en el editor, antes de
  `tipo_de_tecla` (C y V son el condensador y el voltímetro), como
  `[ctrl][Z]`. Actúan sobre el elegido (`[tab]`, o el de la ficha de la que
  se vuelve) o, sin elegido, sobre el único elemento que toca el cursor; si
  no hay ninguno (o hay dos), «Elige primero un elemento: [tab] o [enter].».
- **El portapapeles** (`clip`) guarda la ficha física: tipo, variante,
  valor, aux y `par[]`, con el espejo de un 3 patas (`circ_orienta`), la f
  del circuito y, en una controlada, la ETIQUETA del que la manda (los
  índices cambian al borrar). No guarda los datos de V e I, los `[?!]`, las
  sondas ni la etiqueta. «Copiado R2 (4,7kΩ): [ctrl][V] y una flecha.»
- **Pegar**: `ACC_PONER` con la marca `pegando` («Pegar R2 (4,7kΩ): una
  flecha. [esc] no»). La flecha pone la pieza como siempre (sustituye y lo
  dice; los de 3 patas, por `pon_3patas`) y `aplica_clip` le copia los
  valores ANTES de `quita_ca_si` (pegar una fuente alterna sobre la otra no
  quita los ángulos); sin el último valor de R (`ultimo_R`) ni el modo CA
  automático de un V o un A. El control se busca por la etiqueta (si ya no
  está, -1 y la ficha va a la fila x); si el circuito pasa a alterna sin f,
  la del circuito de donde vino (o 60 Hz). Se abre su ficha: «La copia, con
  sus valores: revísalos.». Cualquier otra tecla deja de pegar.
- **Desde la ficha**, `[ctrl]` con C, X, V o N confirma lo escrito (como
  `[ctrl][S]`) y vuelve al editor a copiar ese elemento, cortarlo, pegar o
  ponerle nombre; sin esto `[ctrl][X]` marcaría `[?!]` (la X) y `[ctrl][N]`
  escribiría un nano.
- **Cortar** copia y quita en un solo nivel de deshacer (`circ_borra`, nuevo
  en circuito.c: todos los segmentos de su caja, con las referencias y las
  sondas). El portapapeles sobrevive a Problema nuevo y a abrir otro (se
  copia de un ejemplo y se pega en el problema propio); `ui_init` lo vacía.

### 21.4 Nombres

- **Mis problemas > `[N]`** (o `[ctrl][N]`): la línea «Nombre: …_» en lugar
  del resumen; empieza con el nombre que tiene, o vacía si es uno del
  programa («Problema nuevo», «Mi problema 3»). Letras en minúscula (con
  `[shift]`, y la primera del nombre, en mayúscula), cifras, espacio (no al
  principio) y `. - + ( ) /`; hasta 23. `[del]` borra (mantenido se repite:
  perfil BORRAR, hasta el vacío), `[ctrl][del]` todo; las flechas, `[menu]`
  y el dedo no hacen nada. `[enter]` escribe el archivo y, si ese hueco es el
  del editor (o el de un nivel de deshacer o rehacer), también ahí el nombre
  (`renombra_hueco`): si no, deshacer y Guardar volverían a escribir el de
  antes. Vacío, lo pide; `[esc]` lo deja como estaba. El pie de la lista:
  «[enter] abrir [N] nombre [del] borrar [esc] volver».
- **El editor, `[ctrl][N]`**: la misma línea en las dos de abajo del
  editor; `[enter]` le pone el nombre y lo GUARDA (en su hueco o, la primera
  vez, en uno nuevo); si no se puede guardar, vuelve el nombre de antes. Un
  nombre propio no lo cambia Guardar (ya era así con los ejemplos), y lo
  escrito aquí se guarda tal cual aunque se parezca a uno del programa
  (`guardar(copia, propio)`: «Mi problema 9» en un problema nuevo no pasa a
  «Mi problema 1»).
- **¿Del programa?** (`nombre_de_omision(nombre, hueco)`): el programa solo
  pone «Problema nuevo» a uno sin hueco y «Mi problema N» al del hueco N − 1
  (o a uno sin hueco, cuyo archivo se borró). Un «Mi problema 9» en el hueco
  0 o un «Problema nuevo» guardado los escribió el usuario: `[ctrl][N]` y
  `[N]` empiezan con ellos y su copia es «… (2)».
- **Al guardar** con el nombre del programa, el aviso lo recuerda:
  «Guardado: Mi problema 3. [ctrl][N] le pone nombre.» (50 columnas con
  «Mi problema 20»; en una copia no, que no cabe).

### 21.5 Desviaciones de la propuesta

| Propuesta | Hecho | Por qué |
|---|---|---|
| códigos nuevos antes de `T_DIG0` | detrás de las letras | no se corre ningún código (semilla, pruebas) |
| la línea de ayuda de la ficha pasa a «número o cuenta…» | solo mientras se escribe | la fija dice `[?!]` y `[del]`, que siguen valiendo; cambian menos pantallas |
| una cuenta que no vale se avisa como un número | y se queda escrita (el 2.º `[esc]` la tira) | teclear otra vez «1/(1/4k+1/6k)» por un paréntesis |
| — | negativa donde no va signo: aviso propio | con cuentas ya se podía escribir |
| copiar «con un elemento elegido» | o el único junto al cursor | como `[enter]` |
| `[N]` en Mis; `[ctrl][S]` y «Guardar como copia» | `[N]` como se propuso; lo demás ya estaba; «al guardar», `[ctrl][N]` (nombre y guardar) y el aviso | preguntar el nombre al guardar cambiaría `[ctrl][S]` (una pulsación más y las pruebas que lo usan) |
| `[shift]` da mayúscula | y la primera letra, sola | los nombres empiezan en mayúscula |

### 21.6 Verificación

- **[72]** (`test_m_flujo.inc`, `HAKARU_SOLO_FLUJO`, también en `HAKARU_SOLO_E4` y
  al final de la batería): 242 comprobaciones. F1 `num_eval` (17 números
  solos con el mismo double, 8 mal escritos, la precedencia, cada error, lo
  que no cabe en un double, solo o a medias, y 300 000 cuentas al azar sin
  un infinito con `NE_OK`);
  F2 la ficha (el eco, el paralelo, la cuenta que se queda, el segundo
  `[esc]`, ÷0, x², el negativo, «-5» y «2E-3» como siempre, dónde entra cada
  signo, 31 caracteres, CA, la Z y la fila x); F3 copiar y pegar (sin V, I ni
  `[?!]`, un nivel, deshacer desde la ficha, cortar, el de junto al cursor,
  `[esc]` y otra tecla, a otro problema, sustituir, la controlada por su
  etiqueta, un BJT PNP con espejo, la f de 50 Hz, `[ctrl]` con C, X, V y N
  desde la ficha; el eco «inf» de «8E505» y su aviso del rango); F4 los
  nombres (Mis, el editor, `[shift]`, espacio, `[del]`, `[ctrl][del]`, 23,
  vacío, `[esc]`, los niveles de deshacer; «Mi problema 9» y «Problema
  nuevo» escritos con `[ctrl][N]` se respetan, y sus copias); F5 por la capa (la tabla física, × mantenido una vez,
  `[del]` mantenido en un nombre: 10 eventos y para); F6 independencia del
  dibujo con las teclas nuevas en la ficha, en un nombre y pegando; F7 los
  textos en 52 columnas.
- **Mutaciones** (`mut_flujo.py`, `sh mut_corre.sh FLUJO mut_f_`): 37 (sin
  cuentas, la precedencia, el ^ sin signo, ÷0, el paréntesis abierto, el
  texto tirado, el negativo, la resta, el eco de antes, x² a medias, el
  segundo `[esc]`, cuentas en la Z, pegar sin valores, copiar con los datos,
  el control por índice, cortar sin deshacer, sin espejo, pegar que sigue,
  la f de 60, sin el de junto al cursor, el nombre sin archivo, sin el
  editor, sin los niveles, sin mayúsculas, `[ctrl][N]` sin guardar, `[esc]`
  que guarda, `[del]` sin repetir, el espacio en otro bit, × que se repite,
  la ficha sin `[ctrl]` C, X, V y N, un dibujo que cambia el nombre; y de
  la revisión: el nombre escrito que se cambia, el hueco que no cuenta, el
  «Problema nuevo» guardado, el infinito de un número solo, el infinito a
  medias y la ficha que trata «8E505» como una cuenta): las 37 mueren, cada
  una en la comprobación que se espera.
- **El mono** pulsa también × ÷ ^ x² ( ) espacio y `[ctrl]` con C, X, V y
  N: sigue llegando a las 30 pantallas con el circuito sano.
- **La batería**: 351 165 comprobaciones, 0 fallos (también con `arm` y con
  `asan`, sin avisos del sanitizador). Por la capa, 12 469 teclas (6 543
  clics), 0 mal entregadas, las mismas comprobaciones y las mismas 839 BMP.
  Frente a `rev6/base` cambian a propósito 8 BMP (las fichas escribiendo, con
  la línea de las cuentas; el editor tras guardar, con `[ctrl][N]`; los pies
  de Mis problemas) y hay 9 nuevas (`out/f_*.bmp`); los arreglos de la
  revisión (§20.9) no cambian ninguna: las 839 salen idénticas a las de la
  versión revisada.

---

## 22. Las 13 mejoras de la tanda 1

Tras el cambio de nombre, un estudio de mejoras con un juez (workflow
`hakaru-mejoras`, `rev2/mejoras.json`) eligió 13 para hacer ya y dejó para
cuando hubiera capa de entrada las que necesitaban teclas nuevas (las
cuentas en la ficha, copiar y pegar y el nombre escrito a mano: §21). Se
hicieron en dos grupos, el editor y el flujo, y los resultados y los
instrumentos, cada uno con su revisor adversarial y su corrección
(`rev5/mejoras1_resultado.json`). `sim.c` no se tocó. La numeración es la
del estudio.

### 22.1 El editor y el flujo

- **(1) La sesión.** Al salir del programa, y al dejar el editor por el
  Taller o por «Salir al menú», el circuito del editor se guarda en
  `ces_sesion.tns`; al volver, Taller > «Seguir» lo trae con el aviso
  «Recuperado de la última vez: [ctrl][S] lo guarda.». Solo desde main.c,
  nunca en `ui_init`. El formato y la huella del hueco, en «Nombre y
  formato». Vacío tras «Problema nuevo», guarda el de antes (el último del
  historial con algo, con su hueco); si se abrió otro o se trajo un
  ejemplo, el anterior sin guardar solo vuelve con `[ctrl][Z]` en la sesión.
- **(2) Nada se pierde al cambiar de problema.** Cada nivel de deshacer es
  {circuito, hueco, doc}: «Problema nuevo», abrir otro de Mis problemas o
  traer un ejemplo o un modelo con `[tab]` apilan el de antes y `[ctrl][Z]`
  lo recupera, con su hueco (Guardar no pisa el que se abrió). Mis
  problemas lee primero en un circuito aparte: si falla, el del editor sigue
  igual. Cambiar de problema vacía rehacer. Al traer con `[tab]`, el
  enunciado (o lo que hay que ver) ocupa las dos líneas de abajo si no cabe
  en una (`aviso[128]`; la línea de estado vuelve con la tecla siguiente), y
  entonces «Traído al Taller: [ctrl][Z] vuelve al que tenías.» sale con la
  tecla siguiente del editor (`aviso_luego`): solo si esa tecla no deja
  aviso; si deja uno de una línea, va debajo; si se va del editor, ya no. Si
  cabe en una línea, va debajo, como antes. Con la paleta abierta, `[ctrl]`
  con Z, Y, S, C, X, V o N vale como en el editor.
- **(3) `[menu]` en la ficha** confirma el campo (si no vale, se queda con
  su aviso) y abre las Opciones del editor.
- **(4) `[tab]` elige y `[enter]` abre.** `[tab]` en el editor marca el
  siguiente elemento durante una tecla («R1 (4Ω): [enter] su ficha  [tab]
  el siguiente»; `[shift][tab]`, el anterior) y `[enter]` abre su ficha sin
  preguntar la flecha. En la ficha de CC, `[tab]` confirma y pasa a la del
  siguiente, con vuelta al final; en CA sigue siendo la caja del ángulo.
- **(6) El culpable.** Al volver con `[esc]` de un error del editor (la
  calculadora de CC, el punto de operación, la emulación, la CA), el cursor
  queda en el elemento que falla y `[enter]` abre su ficha en la fila que
  falla («C1: falta C. [enter] abre su ficha.»); el pie del error lo avisa
  («[esc] al editor: el cursor irá a C1»). `culpable()` lo recalcula sobre
  el circuito, nunca con un índice de otra red.
- **(8) Sustituir y borrar lo dicen.** Poner una pieza o un cable encima de
  un elemento, o borrarlo con `[del]`, avisa con su valor («Sustituye a R1
  (4,7kΩ): [ctrl][Z] lo deshace.»), y `[ctrl][Z]` también funciona dentro de
  la ficha (tira lo escrito, sale y deshace).
- **(10) Mirar no gasta.** Abrir una ficha ya no apila un nivel: se apila al
  salir, y solo si algo cambió (`sal_ficha`, `circ_igual`). `[ctrl][Y]`
  rehace («Deshecho (quedan N). [ctrl][Y] lo rehace.»); cualquier cambio
  nuevo vacía rehacer.
- **(12) Guardar mejor.** `[ctrl][S]` en el editor y en la ficha; un ejemplo
  o un modelo traído conserva su nombre (solo los de omisión pasan a «Mi
  problema N»); «Guardar como copia (otro hueco)», la opción 11 de las
  Opciones, guarda «Nombre (2)» y desde entonces se edita la copia; si la
  escritura falla, el medio archivo se borra y el nombre y el hueco vuelven
  a los de antes. El nombre escrito a mano quedó para la tanda 2 (§21.4).
- Si un problema se borra en Mis problemas, el editor y sus niveles lo
  olvidan (`olvida_hueco`): si otro reutiliza el hueco, deshacer hasta el de
  antes y Guardar no lo pisa (lo encontró la revisión).

### 22.2 Resultados e instrumentos

- **(5) Letras de nodo.** En la capa Potenciales cada nodo lleva su letra
  delante del valor («B 6V»; en CA, con el fasor), en `C_TENSION`, en un
  sitio donde no pise otro rótulo ni el punto de otro nodo. En los pasos de
  Kirchhoff, las letras de los nodos y la del nodo del paso en naranja, con
  sus puntos resaltados: `letras_pasos` las pone al final, donde no tapan
  nada (mira los píxeles con `gfx_ocupados`). Desde el cierre, en
  Potenciales gana el primer sitio que no pisa y tiene un píxel de aire
  (luego, 2 px más afuera); `letras_pasos` prueba también sus 4 sitios 2 px
  más afuera; y la letra amarilla del terminal busca, entre 4 sitios, el
  que no pisa un rótulo. Con terminales, A y B son los nodos de los bornes
  (§5.1).
- **(7) P con signo.** La tabla de CC da «P abs», la que absorbe cada uno
  (negativa si entrega; en una fuente real, la de bornes), y una fila de
  balance: «Entregan 36 W · absorben 36 W ✓» (en rojo, «no cuadra», si no).
  La fila del vatímetro dice «marca −9 W» en la columna del valor y 0 W en
  «P abs» (no absorbe: la columna suma lo del balance). En la tabla, los
  valores por debajo de 0,1 llevan prefijo (13,251 mW). La tabla del
  punto de operación, también con «P abs».
- **(9) CA en el tiempo.** La tabla de CA gana una quinta página, «En el
  tiempo (ω = 377,0 rad/s)»: v(t) e i(t) de pico de cada elemento con el
  convenio que se ve («i = 13,46 cos(377,0t + 37,5°) A»), sin lo nulo; con
  no lineales, avisa de que falta la parte de CC. Desde 1E6 rad/s, ω va con
  exponente («1E9 rad/s», «cos(1E9t …)»).
- **(11) El osciloscopio en régimen permanente.** `[U]` busca el último
  periodo en la cola del registro (`osc_ultimo_periodo`: dos cruces de
  subida al nivel medio) y pone T1 y T2 en sus bordes, con las medidas y la
  frecuencia 1/T; si no hay periodo, lo dice. Con los cursores encendidos,
  las medidas son entre T1 y T2 (el tramo sombreado); si hay pocas
  muestras, `[=]` vuelve a emular con 300 entre T1 y T2 y los conserva. La
  leyenda y los tiempos de los bordes se pintan después de las líneas de T1
  y T2; con los cursores a menos de 10 px, la cajita «2» sube.
- **(13) El disparo** usa el nivel medio de lo que se ve, fijo hasta otro
  `[D]` o `[A]`; `[C]` lo recalcula para el otro canal, y una emulación nueva
  lo rehace con la señal nueva.
- **El editor y los resultados, en el cierre** (pantallas). El recuadro del
  elegido abarca el símbolo (±10 px; ±13 con el + de la pila, las flechitas
  del LED o la tierra del vatímetro; ±18 con la forma de una J; ±8 la caja
  de un 3 patas) y sus rótulos (±2), y va debajo de todo. Un rótulo de
  arriba que saldría por encima del recorte va debajo (`gfx_clip_arriba`).
  Si el resultado de CC no tiene nada pedido ni Req, el panel da el
  balance. El título de la barra se corta en una palabra con «...».
  `esq_cajas`/`EsqCaja` anotan la caja de cada rótulo (el arnés, [74], mira
  que en 82 circuitos ningún par de rótulos se pise ni tape otro símbolo).
  En la biblioteca, posiciones nuevas para que los rótulos no se pisen: el
  tanque LC (la C en la columna 8), la corrección del fp (W2 en 6-7, Z1 en
  8), el voltímetro de «¿L?» (V1 en la fila 0) y el comparador (E2 en
  (4,6)-(4,5), la tierra en la fila 6).

### 22.3 Desviaciones de la propuesta

| Propuesta | Hecho | Por qué |
|---|---|---|
| (10) quitar el nivel al salir de una ficha sin cambios | no apilarlo hasta salir, y solo si cambió | con los 12 niveles llenos, mirar una ficha tiraba el más viejo |
| (2) al traer un ejemplo, el aviso de `[ctrl][Z]` | el enunciado va entero (si no cabe en una línea, usa también la de estado durante una tecla) y el recordatorio sale con la tecla siguiente; si cabe en una, debajo, durante una tecla | el estudiante perdía el enunciado del ejemplo que acababa de traer (revisión) |
| (8) `[ctrl][Z]` solo en el editor | también en la ficha (y luego `[ctrl][Y]` y `[ctrl][S]`) | para que el aviso «[ctrl][Z] lo deshace» sea cierto sin pasar por `[esc]` |
| (12) «Guardar como copia», aplazado por el juez | hecho; la copia es «(2)» | «(copia)» no cabe en 23 caracteres; la opción del menú pasa a «Guardar [ctrl S]» |
| (4) el pie de la ficha nombra `[tab]` | donde cabe: R, C, L, W, V, A y S en CC | en las fuentes, los semiconductores, las controladas y el AO el pie ya ocupa 44 a 47 de 52 columnas; `[tab]` funciona igual y lo dicen la Ayuda y el aviso |
| (5) la letra del nodo en blanco | en `C_TENSION`, como el potencial | junto a los terminales A y B (amarillos) se confundían (revisión) |
| (11) cruzar la página «En el tiempo» con una emulación larga | no | con ω a 4 cifras, a t ≈ 0,5 s ya hay unos 0,3°; el cruce de la CA con el emulador ya lo hacen otras pruebas |

### 22.4 Las revisiones

- **El editor**: el revisor encontró 7 fallos, todos reales y arreglados.
  El medio: tras borrar un problema en Mis problemas y guardar otro en su
  hueco, deshacer hasta el primero y `[ctrl][S]` pisaba al segundo (y lo
  mismo por la sesión); el arreglo es `olvida_hueco` y la huella del hueco en
  la cola de la sesión. Los bajos: `[ctrl][Y]` que traía un circuito del
  problema anterior con el editor vacío; el culpable de la calculadora de CA
  (la f que falta, la fila de lo que no se despeja); 8 mutaciones que
  sobrevivían a [70]; la caja de 12 opciones que tapaba la línea de avisos;
  el enunciado escondido al traer un ejemplo; `[ctrl][Y]` y `[ctrl][S]` en la
  ficha y el pie con `[tab]`.
- **Los resultados**: 8 hallazgos, todos reales y arreglados. El alto: en
  los pasos de Kirchhoff, 58 de las 111 letras de nodo de la biblioteca
  salían tapadas en parte por los rótulos, y 7 de las 14 resaltadas (en el
  puente se leía «ER1»); ahora 0 tapan o quedan tapadas. Los medios: el
  nivel del disparo que se quedaba del circuito anterior (una regresión);
  `[=][tab]` y `[P][tab]` con la potencia en convenios opuestos; la marca
  «T1-T2: pocas muestras [=]», que `[=]` no cumplía y que borraba los
  cursores; y 16 de las 19 mutaciones del revisor, que sobrevivían. Los
  bajos: la frecuencia vacía tras `[U]`, `[U]` que pedía más de 3 periodos y
  las letras de nodo que se confundían con las de los terminales.

### 22.5 Verificación

- **[70]** (`test_m_editor.inc`, `HAKARU_SOLO_M_EDITOR`): 149 comprobaciones
  (150 tras el cierre).
  Mutaciones: 21 en la primera versión y 41 tras la revisión, todas
  detectadas.
- **Las mejoras de resultados** (`test_m_resultados.inc`,
  `HAKARU_SOLO_M_RESULTADOS`; su cabecera en la salida es «[69] Mejoras de
  resultados e instrumentos»): 114 comprobaciones, también con ASan+UBSan.
  Mutaciones: 52 de 52 atrapadas por esta sección sola
  (las del revisor, las de la corrección y las de la primera versión que
  siguen valiendo).
- La batería entera, en sus copias: 350 581 y 350 545 comprobaciones, 0
  fallos; con las dos en el árbol, 350 695. Las BMP que cambian lo hacen a
  propósito: fichas con el pie de `[tab]`, el menú de 12 opciones, la Ayuda
  más larga, los potenciales con letra, los pasos de Kirchhoff, las tablas
  con «P abs», la paginación «n/5» de la CA y las medidas entre T1 y T2.
- Los textos nuevos caben en 52 columnas; la Ayuda crece (los puntos 4 y 5,
  OTRAS TECLAS, DESHACER, GUARDAR Y SEGUIR OTRO DÍA, el osciloscopio y el
  resultado).

### 22.6 Límites

- Tras `[ctrl][Z]` por encima de un «nuevo», el rango del Bode y el del
  barrido no vuelven.
- Deshacer y rehacer suman unos 108 KB de memoria estática.
- La sesión se escribe en la flash (unos 1 a 5 KB) cada vez que el editor
  pasa al Taller y al salir. Si el propio problema se guardó después de la
  sesión y el programa no salió bien, el siguiente Guardar hace una copia de
  más (nunca se pierde uno).
- La tabla del punto de operación no lleva fila de balance (lleva la suya,
  «Newton · Kirchhoff»).
- `[U]` necesita dos periodos enteros en el registro; con [U] la ventana
  acaba en el final del registro.

## 23. La optimización (rev9)

Con la Etapa 4 cerrada, el objetivo fue que la candidata hiciera **lo mismo
con menos trabajo** en la calculadora: no adaptarse al hardware, sino
encontrar lo que el código hacía de más. Del 2026-09-27 al 2026-09-29.

### 23.1 Cómo se midió

Un **perfil del ARM fiel**: el `.tns` corriendo en QEMU con un plugin que
cuenta, función por función, las instrucciones y las llamadas a la coma
flotante por software, con un modelo de ciclos del ARM926 (la tabla de
instrucciones y las cachés del CP15) ajustado con el banco de la calculadora.
Con él se midieron doce cargas típicas (un ejemplo de CC, el peor caso de
CC, un paso del emulador, un punto de CA, el Bode de un RLC, un paso del
puente con 1N4007, el editor, el osciloscopio, el Bode quieto, la pantalla
de CA…) y salió un plan de **19 puntos**, ordenados por ganancia y riesgo:

| Fase | Puntos | Regla |
|---|---|---|
| 1 | 1-11, 17 y la parte de 19 que se puede hacer sin calculadora | **bit a bit**: los mismos resultados, hasta el último bit |
| 2 | 12 (Newton con inversos guardados), 13 (signo de los cruces del Bode sin log), 14 (trazador sin `pow` por columna) | dentro de tolerancia |
| 3 | 15 (orden de grado mínimo), 16 (volcado propio a la pantalla), 18 (Newton reutilizando la factorización) | cambian resultados o son de riesgo alto |

### 23.2 La fase 1, bit a bit

Hecha e integrada (2026-09-29). Por grupos, cada uno en su copia del árbol y
solo en sus archivos:

- **Motor** (`sim.c`, `lu.c`, `luc.c`, `ondas.c`, `nolin.c`, `barrido.c`):
  la LU dispersa sin comparaciones de coma flotante (1), filas con relleno e
  intercambio de filas por punteros (4), sacar del paso del transitorio y del
  Newton lo que no cambia (2), el planificador del tramo rígido calcula |s|
  una vez por modo (7), una LU por barrido y copias de estado solo de lo
  usado (9) y una máscara de bits por fila en la LU (11).
- **CA y Bode** (`ca.c`, `bode.c`, `calca.c`): cada `g_de_H` una sola vez
  (5); la topología y la base de la matriz una vez por barrido, y la G de
  cada R por topología (10); y tres extras del mismo estilo.
- **Dibujo** (`gfx.c`, `ui.c`, `osc.c`, `trazador.c`, `curva.c`,
  `esquema.c`): una sola limpieza por cuadro (las pantallas que se borran
  solas no se borran dos veces), las primitivas por palabras de 32 bits
  (`gfx_clear`, `gfx_hline`, el texto sin `gfx_px`, `gfx_fila_pares`) y
  cachés de las curvas del Bode y de las filas del osciloscopio (3); y, en
  las pantallas animadas, el Huffman por tabla y sin división entera por
  píxel (17).
- **Otros**: la calculadora de CC prueba las combinaciones de signos por el
  árbol y memoriza `pow(10, k)` (6); y **la coma flotante rápida** (8):
  `cf_arm.c`, que sustituye por `--wrap` la división, la raíz cuadrada y las
  comparaciones de `double` de libgcc y newlib. En la CX II la división de
  libgcc va bit a bit (804 ciclos, el 13 % de un paso del emulador), la raíz
  de newlib también (2129) y cada comparación son tres llamadas anidadas;
  las de `cf_arm.c` estiman con tablas y pasos de Newton y corrigen con el
  **resto exacto**, así que el resultado es el de IEEE 754, **los mismos
  bits** que las de la biblioteca (lo raro, 0, subnormales, Inf y NaN, va a
  las de libgcc). Y la segunda página del banco (§23.4).
- **Cal**: la recalibración del modelo de tiempos con el banco sin USB, el
  control del gasto en `elige_paso`, el tope `NL_OPS_OP` con lo fijo, `sin()`
  medido (`SIM_OPS_SIN` = 16 operaciones), `CAL_OPS_TOPE` y `decide_malla`.

**Bit a bit, comprobado.** La huella de cada emulación (`HAKARU_HUELLA_LOG`,
con el reloj del PC sustituido por uno falso, para que el banco del arnés no
corte por el reloj real) es la misma antes y después: 0 distintas. Las BMP
de todas las pantallas, iguales. En el ARM, las huellas de los mismos
cálculos corridos en el perfil, iguales. Los tres oráculos, idénticos. Cada
grupo tuvo su revisor independiente: de sus 23 hallazgos, 20 se arreglaron,
2 no eran fallo y 1 (el plan del tramo rígido con redes suaves sale corto)
quedó para el punto 19; la revisión final de la integración dio 12 más,
todos arreglados.

### 23.3 Lo que se ganó

El perfil del ARM frente a la candidata de antes:

| Carga | Cambio |
|---|---|
| un ejemplo de CC | −67 % |
| el peor caso de CC | −53 % |
| un paso del emulador | −15 % |
| un punto de CA | −60 % |
| el Bode de un RLC | −48 % |
| un paso del puente con 1N4007 | −26 % |
| el editor | −47 % |
| el osciloscopio | −79 % |
| el Bode quieto | −89 % |
| la pantalla de CA | −56 % |

Ninguna región empeora. El volcado de la pantalla sigue en 4,3 ms por cuadro
(es el punto 16, de la fase 3).

### 23.4 El modelo de tiempos y el banco

El programa estima cuánto tardará cada cálculo (para elegir el paso del
emulador, partir el trabajo en ticks y enseñar cuánto falta) con un modelo:
las operaciones de coma flotante que cuenta, por lo que cuesta cada una, más
lo fijo de cada paso o iteración. Las constantes salen del **banco de tiempos**
de la calculadora (Ayuda > `[T]`) corrido **sin el cable USB** y tras
reiniciar: con el USB el SO baja el reloj de 396 a 288 MHz y todo sale 1,37
veces más lento (el primer banco del cierre, §19.8, se corrió así).

| Constante | Valor | De |
|---|---|---|
| `SIM_NS_OP` (`sim.h`) | 463,29 ns | multiplicar y restar: 183 ciclos a 396 MHz |
| `SIM_NS_PASO` (`sim.h`) | 43,45 µs | lo fijo de un paso del emulador (`BN_SIM`: 1514 operaciones por paso) |
| `SIM_OPS_SIN` (`sim.h`) | 16 | una `sin()` de la libm |
| `CA_NS_OP` (`ca.h`) | 1233,3 ns | una operación de CA (`BN_CA`) |
| `NL_OPS_EXP` (`nolin.h`) | 17 | una `exp()` o un `log()` |
| `NS_ITER` (`nolin.h`) | 214,3 µs | lo fijo de una iteración de Newton (`BN_NL`) |

`test_calibracion` ([14]) reproduce las cargas del banco con el paso y la
ventana de aquella versión y exige que el modelo dé los 4000 ms medidos con
±0,5 %. El banco tiene ahora una **segunda página** (`[izq/der]` al acabar)
con lo necesario para recalibrar tras optimizar: una división, una
comparación y una raíz de `double`, una vuelta de registros, una carga que
acierta y otra que falla en la caché de datos (con el tipo de caché del
CP15), un paso del emulador sin reinicios, el arranque del peor emulador, un
paso de una red pequeña y un punto de CA sin la topología.

Las constantes son las de la candidata de **antes** de optimizar: el código
optimizado hace menos trabajo por operación contada, así que el modelo
queda del lado seguro (estima de más). Falta el punto 19: correr el banco,
sus dos páginas, con la versión optimizada y recalibrar.

### 23.5 El turbo (experimental, fuera de la versión)

`turbo.c` y `turbo.h` suben el reloj de la CX II de 396 a 408-456 MHz desde
el panel Controles, cambiando el multiplicador del PLL en el registro
`0x90140030` de la PMU (el `set_cpu_speed` de Ndless no hace nada en la CX
II). Tiene reglas estrictas: apagado al arrancar; solo actúa a 396 MHz y sin
USB; de 12 en 12 MHz con confirmación de la PMU en cada paso; tope de 432 y
444/456 solo tras un aviso; vuelve a 396 **antes de escribir cualquier
archivo** y al salir por cualquier camino; deja una marca de cuelgue en
`ces_controles.tns` mientras prueba.

**No va en la versión** (decisión del 2026-09-29). La revisión final
encontró que el único programa de referencia que escribía ese registro
directamente (el reproductor de vídeo de GigaZelensky, `performance_clock.c`)
lo acababa de sustituir por una llamada a la transición del propio sistema
operativo, que pone la SDRAM en autorrefresco, confirma el PLL y ajusta los
tiempos de la EMI: escribir solo el multiplicador puede no hacer nada o
aplicarse sin el protocolo de la memoria. El código queda dentro, sin
compilar el acceso al hardware: con `TURBO=0` (por omisión) `turbo_hay()` da
0, la fila no aparece y ningún registro se toca. `make TURBO=1` lo compila,
bajo el propio riesgo. En el PC, un simulador de la PMU lo prueba igual
(`test_opt_turbo.inc`).

### 23.6 Verificación y lo que queda

Al integrar la fase 1 (2026-09-29): la batería, 351 910 comprobaciones y 0
fallos; `asan`, `arm` y por la capa, iguales; los tres oráculos, idénticos;
el mono, cuatro semillas de un millón de teclas por las 30 pantallas, sin
fallos. El `.tns` compilado desde el árbol, byte a byte igual al entregado.
Las pruebas propias de la optimización: `test_opt_motor.inc`,
`test_opt_cabode.inc`, `test_opt_dibujo.inc`, `test_opt_otros.inc`,
`test_opt_cal.inc`, `test_opt_turbo.inc`, `test_opt_fuzz.inc` y
`test_opt_final.inc` (cada una con las mutaciones que la hacen fallar).

Queda: la fase 2 (12, 13 y 14), la fase 3 (15, 16 y 18), el punto 19
(recalibrar con el banco de la versión optimizada) y, aparte, investigar el
método del turbo con la transición del SO para proponerlo como prueba guiada
en la calculadora.

## 24. Capacitores equivalentes

La cuarta opción del Taller (2026-10-07 y 2026-10-08; la v3 es la de hoy).
Nació para un examen de Electromagnetismo: el capacitor de arranque de un
motor monofásico se quemó y en la bodega no hay uno igual. ¿Con cuáles se
arma, con uno, dos o tres, en serie o en paralelo? No es la Etapa 5: es una
herramienta aparte, con su propio menú.

### 24.1 Qué resuelve

Datos: el valor C (en MF, microfaradios, como en las placas), la tolerancia
t y la bodega. **La bodega admite repetidos**: cada vez que un valor aparece
en la lista es un capacitor más, y en cada arreglo cada capacitor se usa una
sola vez (7 ∥ 7 solo si hay dos de 7). La lista se escribe con espacios,
rangos (`15-42`) y exclusiones (`sin 21-24`).

| Arreglo | CT |
|---|---|
| uno | c |
| paralelo | a + b |
| serie | a·b/(a + b) |
| paralelo de 3 | a + b + c |
| serie de 3 | a·b·c/(a·b + b·c + a·c) |
| mixto 1: (a ∥ b) en serie con c | (a + b)·c/(a + b + c) |
| mixto 2: (a en serie con b) ∥ c | a·b/(a + b) + c |

Vale lo que cae en [C − t, C + t]; sin tolerancia, lo exacto con el margen
del redondeo a un decimal, ±0,05 MF (21·42/63 = 14 da «14 ✔»). Las
comparaciones llevan un margen de 1e-9 (`CPM_EPS`): un CT en el borde exacto
vale aunque la coma flotante lo deje un ulp fuera.

**El modo** («Capacitores»): Auto busca con 1, 2 y 3; 2 o 3, solo con ese
número. **El orden** de la lista es total: primero las de **menos
capacitores**; dentro de cada número, |CT − C| (redondeada a 1e-9, para que
dos cuentas iguales empaten aunque difieran en un ulp); luego el tipo (uno <
paralelo < serie < paralelo de 3 < serie de 3 < mixto 1 < mixto 2); luego
los valores ascendentes. Se enseñan hasta 50 de dos y las 20 más cercanas de
tres. Si nada vale, lo dice, da la más cercana y **qué capacitor faltaría**
(«te faltaría…», por cada valor distinto que lo lograría).

### 24.2 Pantallas

- **El menú** (`P_CPM_MENU`): Resolver, Teoría y Ejemplos.
- **Los datos** (`P_CPM`): valor, tolerancia, bodega y modo, con **edición
  con cursor** (`[izq/der]`, `[shift]` salta de valor, `[ctrl]` a un
  extremo); `[A]` y `[B]` cargan las bodegas del pizarrón.
- **El resultado** (`P_CPM_RES`), con dos pestañas: **Opciones** (`[arr/ab]`
  elige, `[enter]` dibuja) y **Paso a paso**, el procedimiento explicado con
  palabras (por qué serie o paralelo, las cuentas y la comprobación).
- **El dibujo** (`P_CPM_DIB`): el cableado de la opción, al estilo de la
  pizarra: la clavija y los bornes C, A y T del motor, con los capacitores
  de esa opción.
- **Teoría** (`P_CPM_TEO`, `P_CPM_PAG`) y **Ejemplos** (`P_CPM_EJ`): el
  contenido de `capeq_texto.c`, con las cuentas verificadas por fuerza bruta.

Los textos van con **punto** decimal, como en el pizarrón (el resto del
programa escribe coma), y en líneas de 52 columnas como mucho.

### 24.3 Código

- `capmotor.c` y `capmotor.h`: el cálculo, **funciones puras** sin gráficos
  ni estado (`cpm_resolver`, los textos del procedimiento, el «te faltaría»).
  El prefijo `cpm_` viene del primer nombre, «Capacitor de motor».
- `capmotor_dib.c`: el dibujo del cableado.
- `capeq_texto.c` y `capeq_texto.h`: la Teoría y los Ejemplos (solo
  contenido, en Latin-1 con las macros `S_*`).
- `ui.c`: las siete pantallas `P_CPM*`, y `AYUDA_CPM`, su parte de la Ayuda.

### 24.4 Verificación

Un **oráculo independiente** en Python (fuerza bruta escrita aparte, en los
documentos de trabajo) contra el volcado del programa
(`HAKARU_CAPMOTOR_VOLCADO`): 15 039 de 15 039 casos iguales, y 6039 más con
otra semilla, 0 diferencias. En el arnés, [76] (`test_capmotor.inc`,
`HAKARU_SOLO_CAPMOTOR`): los casos del pizarrón, 6000 bodegas al azar con sus
dibujos (ningún rótulo pisa a otro), las filas del resultado, los ejemplos y
las pantallas (`out/cm_*.bmp`). Al integrarla: huellas deterministas, 0
distintas frente a la versión anterior; ARM, `asan`, `arm` y por la capa,
iguales; el mono, con dos semillas, por todas las pantallas.

Las pruebas de antes se escribieron para un Taller de tres filas y una
Ayuda que acaba en ACERCA DE: en el arnés, `ui_cpm_oculto(1)` esconde la
cuarta fila y la parte de la Ayuda para no mover sus teclas ni las del mono
(en la calculadora se ve todo); `test_capmotor` y la fase del mono de los
capacitores lo ven todo.

## 25. La versión pública y la especial

Al preparar el código para publicarlo (2026-10-28), el programa quedó en dos
versiones que salen del mismo árbol:

- **La pública** (`ESPECIAL=0`, la de siempre: `make`), la de este
  repositorio.
- **La especial** (`make ESPECIAL=1`), la del autor, que añade extras
  privados. Sus archivos viven en la carpeta `especial/`, que **no se
  publica** (`.gitignore`); sin ella, la bandera no compila.

En el código público, cada punto en que la especial añade algo es una macro
**`ESP_*`**, vacía en la pública: en `ui.c` (pantallas, dibujo, teclas,
ticks, repetición), `ui.h` (las entradas de sus `enum`), `banco.c` y
`banco.h` (sus líneas del banco), `archivo.c` y `archivo.h`, y en el arnés.
`especial/` define esas macros con **los mismos tokens, en el mismo sitio**
que tenían antes de separar las versiones, así que el `.tns` de la especial
salió idéntico, byte a byte, al de antes (comprobado con `cmp`). Después se
añadió en ACERCA DE la línea «Autor: Eliam Mojardin Ibarra. Licencia MIT.»,
en las dos versiones: es lo único que cambió desde entonces. Por eso, por
ejemplo, el perfil de repetición `REP_ESP_*` de `entrada.h` sigue en su
sitio, y las funciones `static` de la especial se definen con una macro en
su posición original: mover una función cambia el orden del código en el
`.tns`.

**Verificación de la separación.** La pública: el arnés, **352 164
comprobaciones, 0 fallos**, sin avisos del compilador; el `.tns`, 898 480
bytes, sin ninguna cadena de los extras. La especial: las mismas
comprobaciones y las mismas BMP que antes de separarla. Las pruebas que
dependían de los extras pasaron a la especial: el ritmo de las animaciones y
la tecla mantenida en una de ellas ([73] C1 y C4), su independencia del
dibujo y el plan B del reloj con una animación ([71]), su parte de D1, D8,
D9 y D10 ([75d]) y de [74] (P11).

**Los nombres.** Con la publicación se terminó de quitar el nombre viejo de
lo interno: las variables del arnés `EFK_*` pasaron a `HAKARU_*`, sus
binarios a `out/hakaru_test` y `out/hakaru_capa`, el formato de red de las
redes fijas (`herramientas/c204`, `test_flanco.inc`) de `efknet` a `cesnet`
y su nota de `# efk:` a `# motor:` (con eso `banco_flanco.py` vuelve a poder
leerlas con el oráculo no lineal, que solo acepta `cesnet`), y los
resultados de ejemplo de `.efk` a `.ces`. Las marcas `EFK1`, `EFK2` y `EFK3`
siguen nombradas donde se comprueba que esos archivos viejos se rechazan. La
documentación pasó a `docs/` y se escribió de nuevo para el repositorio.
