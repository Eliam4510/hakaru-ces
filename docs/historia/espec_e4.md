# ElectroFock · Etapa 4 · Especificación única de semiconductores y amplificadores

> **Nota.** Especificación histórica de la Etapa 4 (2026-09-25): conserva los
> nombres de su momento. El programa se llama ahora **Hakaru CES**; el
> archivo «EFK3» es hoy `CESP` (el mismo formato con otra marca), EFKNET es
> CESNET y el `.efk` del arnés, `.ces`. Las rutas `scratchpad/…` son los
> guiones de trabajo con que se calcularon los números, que no forman parte
> del repositorio. Lo que se construyó y en qué se desvió de aquí está en
> [DESIGN.md](../DESIGN.md), §19.

Documento del juez. Junta las cuatro propuestas (motor no lineal; componentes de varias patas; modelos físicos y catálogo; interfaz, instrumentos y verificación) y las comprueba contra el código real, leído el 2026-09-25 (sim.h/sim.c enteros, lu.h/lu.c, ca.h/ca.c, resuelve.h/resuelve.c, tipos.h/tipos.c, circuito.h/circuito.c, red.h/red.c, archivo.c, esquema.h/esquema.c, ui.h y las partes de ui.c del editor, la ficha, la paleta, `igual()`, P_EMU_ESQ y P_MODELO, biblioteca.h/.c, proced.c, el arnés y DESIGN.md §17-18). Si una propuesta contradice al código, manda el código. Si dos propuestas se contradicen, elijo una y digo por qué. Los números del §6 los calculé con Python (mpmath a 40 cifras y scipy Radau con rtol 1e-11); los guiones están en `scratchpad/e4juez/` (`n1.py` diodos, zener, LED, AO y controladas; `n2.py` BJT y MOSFET; `n3.py` rectificadores por tramos, recortador, astable; `n4.py` fijador y rectificadores exponenciales; `n5.py` barrido y pequeña señal; `n6.py` pequeña señal del emisor común, inversor, Schmitt, familia del MOS y Thévenin con una controlada; `n7.py` diez diodos en serie y las Vcrit).

**Revisión tras la crítica (2026-09-25, tarde).** Un crítico encontró 9 problemas graves, 13 medios y 10 menores; los 32 son reales y están cerrados en el texto. El §9 dice qué cambió, dónde y, en los tres puntos en que no tomé el arreglo que proponía, por qué. Los números nuevos salen de `scratchpad/e4juez2/`: `r1_rect.py` (réplica del §3.G para los cuatro rectificadores, con y sin la historia BDF2 del diodo que conduce), `r2_varios.py` (el MOSFET invertido, los límites del LED y del zener, los iconos) y `r3_textos.py` (anchos de los textos nuevos).

---

## 0. Punto de partida (lo he comprobado)

**El arnés hoy.** Copié las fuentes (con los `.inc` de las pruebas) a WSL (`/tmp/efk4`) y corrí `build_host.sh` a las 15 h del 2026-09-25: **311 709 comprobaciones, 0 fallos** («TODO CORRECTO»); el mono visita 28 de 28 pantallas. Los 2 fallos de [49] que vi a las 14:30 los cerró la revisión de la Etapa 3, que además añadió las secciones **[54a], [54b] y [54c]** («Revisión de la Etapa 3: motor, calculadora y Bode», «…: la interfaz», «…: modelo y memoria»). **Regla: cada paso de la Etapa 4 empieza y acaba con el arnés en 0 fallos** (paso 0 del §7).

**El código cambia mientras diseño.** Como en las etapas anteriores: antes de cada paso del §7 hay que volver a leer `sim.c`, `ca.c`, `circuito.c`, `resuelve.c`, `ui.c` y el arnés. Esta especificación fija el rumbo, no números de línea. **Las secciones nuevas del arnés empiezan en [55]** (hoy llegan a [54a]-[54c]; [29] y [29b] son las del oráculo). Si al empezar hay más secciones de la revisión, se corren todas las de la Etapa 4 otra vez.

**Hechos del código que condicionan el diseño.**

| Hecho | Dónde | Consecuencia |
|---|---|---|
| `mapa()` numera primero los potenciales y luego las ramas (`con_rama`: E ideal, A, S, C y L). `typedef char cabe_lu[2 * MAX_EL <= LU_MAX ? 1 : -1]`; `op_interno` da `SIM_GRANDE` si `nincog > LU_MAX`. | sim.c 161-182, 692 | Sin nodos internos, un circuito lineal conserva su mapa bit a bit. Las ramas nuevas (E controlada, AO ideal) van en el mismo bucle de ramas. |
| `rol_de`, `con_rama`, `ensambla`, `termino` y `valores_de` tienen `default`. El de `valores_de` hace `v->I[k] = x[j]` con `j = col_br[k]`, que vale −1 en un tipo sin rama: **leería `x[-1]`**. | sim.c 235-250, 482-546, 561-602, 612-654 | Cada tipo nuevo necesita su `case` en los cinco. |
| `analiza()` solo conoce `ROL_G/V/I/NADA`, recorre los lazos con `ORDEN[5] = {E, AMI, S, L, C}` y usa `e->a`, `e->b` para lazos, trozos y cortes. Un lazo de E, A o S da `SIM_LAZO_V` **aunque cuadre**. | sim.c 302-463 | Los elementos de 3 patas necesitan decir qué pares de nodos unen y cuál es su rama de tensión (§3.B). Una E controlada o la salida de un AO en un lazo de tensiones es `SIM_LAZO_V`. |
| La caché de LU es de 4 entradas con clave `(mask, hh)`; `prepara_lu` vuelve a llamar a `analiza` **en el modo 1** (el C es su compañero 2C/hh) y escribe `res_.d`; `cache_de` suma `res_.factorizaciones`. `M.ic` y `M.ic_arr` son índices de la caché. | sim.c 865-991 | Los tramos de los no lineales por tramos entran en la clave; 8 entradas. **La caché es solo del transitorio:** el punto de operación, la pequeña señal y el barrido no la tocan (usan `lu_op` y `lu_nl`, §3.D), porque su matriz es la del modo 0 y porque desalojar una entrada dejaría a una emulación a medias con una LU ajena. |
| `ensambla` (sim.c) y `ensambla_en` (ca.c) ponen la ley de nodos de toda rama **antes** del `switch`: `if (j >= 0) { suma(ca, j, 1); suma(cb, j, -1); }`, con `e->a` y `e->b`. | sim.c 488, ca.c 144 | La rama del AO está en (nref, c), no en (a, b): esa línea usa el puerto 0 (§3.C) y las filas del AO y de la EC no repiten la ley de nodos. |
| `red_cierra`: la referencia de un subcircuito es la tierra; si no, el − de su primera fuente (`fuente`); si no, **su primer nodo**. Un circuito sin fuentes da `RED_SIN_FUENTE`, que el emulador ignora. | red.c 190-223 | Un astable sin tierra tomaría como referencia de la salida un nodo cualquiera (hasta la salida misma). El subcircuito de un AO necesita tierra o una fuente independiente (§3.A, `SIM_AO_REF`). |
| `circ_elemento` pone a todo C nuevo **V0 = 0 como dato** (y a toda L, I0 = 0). En el OP, un C con V0 es una rama V; `sim_iniciar_red` arranca el C en su V0 aunque el OP diga otra cosa. | circuito.c 157, sim.c 1994 | Un C con V0 que cierra un lazo con la salida de un AO, de una EC o con el nulador de un AO ideal necesita el rescate CI_SALTA, no `SIM_LAZO_V` ni una LU singular (§3.B). |
| `tipo_info()->fuente` decide el convenio de la V en resuelve.c 111 (el dato de V), esquema.c 829 (las flechas de corriente) y ui.c 3440 (la columna Z de P_CA). | esos sitios | Las controladas tienen `fuente = 0`: esos tres sitios pasan a `circ_v_convenio(tipo) == −1` (§3.O, §5). |
| `test_biblioteca_ca` ([53]) toma todo modelo con `ca != 0`; el bucle del fp llama a `ca_analiza` con cualquier modelo de medida 'L' y `ca != 0`; ui.c 1423 (el pie) y 4543 (`[enter]` en P_EJEMPLOS) tratan igual cualquier `ca != 0`. | host_test 7268, 7327; ui.c | Con `ca` 3 y 4 hay que filtrar `ca == 1 \|\| ca == 2` en [53] y despachar 3 y 4 antes en la interfaz (§5, §6.1). |
| Un paso partido (evento entre dos puntos) reancla la malla: `t_ancla = t; k = 0`. El arranque tras un evento son 8 pasos de Euler implícito de h/8 con su propia LU (`hh = h/4`). | un_paso, sim.c 1334-1410 | Un cambio de tramo localizado es un evento más (reancla y arranca). La subdivisión que pide Newton no puede reanclar: va dentro de un paso de la malla. |
| `fuente_en` cuenta un `sin()` como 30 operaciones; en el banco existe `BN_SENO` pero no hay medida de `exp()`. | sim.c 188-192, banco.h | `exp()` y `log()` cuentan 30 hasta medir `BN_EXP`. |
| `lu_factoriza`: pivoteo parcial sin escalar; pivote nulo si es menor que 1e-12 del máximo **de su columna**; listas de no nulos rehechas en cada factorización. | lu.c | Un Newton con uniones factoriza en cada iteración; no se reutiliza el patrón (§1 D11). |
| `op_interno` repite `analiza` hasta que los rescates (C_DESC, L_SIN_I, CI_SALTA, CARGA, FLUJO, ANCLA) se asientan y luego hace **una** resolución lineal. | sim.c 679-718 | El Newton va en el sitio de esa resolución; los rescates no cambian. |
| `busca_impulsos` y `algebraica` solo ven E ideal, AMI, S cerrado y C como fuentes de tensión. `historia_bdf2` da a un C algebraico la i_C de la BDF2 (el mismo sesgo que el trapecio) y a los demás la i_C de la solución en te + h (la del Euler, que no cuadra con el trapecio si el C tiene un modo mucho más rápido que h). | sim.c 1152-1303 | `algebraica` debe contar la salida del AO y la E controlada (el derivador: un C entre una E y la masa virtual) **y el diodo por tramos que conduce con R·C < h** (el C del rectificador: Ron·C = 0,1 µs frente a h = 167 µs; sin eso el trapecio deja la corriente zumbando y el apagado sale 0,19° tarde, §3.B). `busca_impulsos` no (límite conocido, §8). |
| `registra` integra `W` por puntos medios de un solo puerto; `termina` reparte el balance por tipo (E y J entregan, C y L almacenan, el resto menos S y AMI disipa). | sim.c 1064-1088, 1916-1957 | Los de 3 patas necesitan un segundo puerto en `Valores`, en `Punto` y en el registro; el AO y las controladas **entregan**. |
| `ca.c`: `ensambla_en` estampa por tipo con `default` vacío; `ca_apaga` pasa las J apagadas a `SE_VMI` (con `instr = 'j'`); la topología sale de `sim_topo_ca`. | ca.c 104-202 | La CA con no lineales necesita su punto Q **antes** de `ca_apaga` (una J de polarización no puede estar apagada al buscar Q). |
| `igual()`: alterna → CA o su calculadora; con `[?!]` → `calcular()`; si es dinámico → emular; si no → `calcular()`. | ui.c 2319 | La rama nueva va delante de «si es dinámico»; un circuito sin tipos nuevos sigue igual (§4.1). |
| `resolver()` da `SOL_CA` en su primera línea. `arma()` escribe la ley de la fuente en el `default` del `switch`; `extrae()` tiene una rama para «no es fuente» (criterio pasivo). `proced_generar` se llama tras `resolver`. | resuelve.c 42-162, 221-243; ui.c 1737 | Un tipo nuevo caería en el `default` («φ+ − φ− = E») o en la rama pasiva: necesita su `case`. `proced_generar` no debe tocar los tipos nuevos. |
| `avisos` es `unsigned int` y usa hasta `0x8000`. `dib_emu_esq` busca el primer aviso con `for (bit = 1; bit <= SIM_AV_SIN_MODOS; …)`. | sim.h, ui.c 3027 | Los avisos nuevos van de `0x10000` en adelante y el bucle se amplía. |
| `Elemento.ref` ya existe («el que controla una fuente dependiente»), se guarda en EFK3, `borra_elemento` lo renumera o lo deja en −1 y `circ_valido` lo acepta en **cualquier** tipo (lo exige [8]: `ref = 0` en una R). | circuito.h/c, archivo.c | Las fuentes controladas no piden ningún campo nuevo: su control es `ref` y su variante (V o I). |
| red.c une puntos solo con `SEG_CABLE`, tiene `a[k]` y `b[k]`, y `MAX_NODOS = 64` con la guarda silenciosa `r->nn < MAX_NODOS`: un nodo 65 **no se numeraría** y su borne quedaría en −1. | red.h, red.c 132 | Con 3 patas caben 65 nodos (31 elementos de dos bornes aislados y un AO). `MAX_NODOS` sube a `REJ_PUNTOS` = 70. |
| La rejilla es de 10 × 7 puntos; en el editor el punto (gx, gy) cae en (16 + 32·gx, 31 + 24·gy) (el paso **no es cuadrado**). Un segmento vale `SEG_VACIO`, `SEG_CABLE` o `SEG_EL0 + k`. `circ_valido` exige que cada elemento esté en **un** segmento y que el total de segmentos con elemento sea `nel`. | circuito.h/c, esquema.c | Un componente que ocupa varios segmentos cambia esa cuenta, no el formato de la rejilla. |
| `borra_elemento(k)` ya vacía **todos** los segmentos que valen `SEG_EL0 + k`. `circ_elemento` y `circ_cable` **sustituyen** lo que haya en el segmento. | circuito.c 65-110 | Borrar un AO entero funciona sin cambios; un cable tirado dentro de su caja lo borraría si no se prohíbe. |
| `circ_grado` cuenta segmentos > 0; lo usan las uniones (grado ≥ 3), la tierra, `sonda_nodo` y `circ_sondas_limpia`. | circuito.c 187, esquema.c 894, ui.c | Hay que redefinirlo para que los segmentos internos de una caja no cuenten. |
| `tipo_rango(tipo, Q_P*)` toma la **primera** fila con esa q, y `circ_valido` rechaza un `Q_P*` dato si su rango es 0..0. `CampoInfo.vars` es de 8 bits (8 variantes como mucho). El nombre de una fila ocupa 2 columnas (la caja empieza en x = 24). | tipos.c, circuito.c 457-462, ui.c 1143 | **Un hueco `Q_P*`, un significado por tipo** (el Vγ del codo y el Is de Shockley van en huecos distintos). Nombres de 2 caracteres: «Vγ», «Is», «V+». |
| `TipoInfo` se inicializa por posición; `tecla = 0` quiere decir «solo en la paleta» (`tipo_de_tecla` la salta). | tipos.h/c, ui.c 4068 | La huella y la página Modelo por variante entran como punteros al final (NULL en las filas de hoy). |
| En el editor están libres D, H, I, K, M, N, P, Q, U, X e Y (hoy usa R E J V A C L S Z, W, B, G, T, F, O). | ui.c 4137-4263 | [D] diodo, [U] AO, [Q] BJT, [M] MOSFET, [I] barrido de CC, [P] punto de operación. En el arnés, M, U, P y D solo se pulsan en la ficha (prefijos) o en el osciloscopio. |
| La paleta es de 4 columnas y 14 piezas (celdas de 72 × 42 px, de y = 22 a 216) y las pruebas la recorren **por posición** (índices 0 a 13; [52b] pone W1 con `[catálogo]`, 3×`[ab]`, `[der]`, `[enter]`). | ui.c 4008-4065 | Las piezas nuevas van del índice 14 en adelante; las celdas bajan a 30 px. Dos pruebas de la revisión ([54b], [54c]) miran «la última pieza es el vatímetro»: cambian a propósito (§6.1). |
| El menú tiene `N_CTX` = 9; [38] abre P_AJUSTES con 6×`[ab]` y sale con `[menu][arr][enter]`; [52] abre el Bode con 7×`[ab]`. `LIN_H` = 13. | ui.c 845, host_test | 11 opciones (caja de y = 40 a 209); la 6 y la 7 no se mueven y Salir sigue la última. |
| `P_N` = 28 y el mono marca las pantallas en un `unsigned` de 32 bits. | ui.h, host_test 8340 | Una pantalla nueva: `P_BARRIDO` (P_N = 29). |
| `medida_modelo` del arnés: 'M' y 'm' en todo el registro (**'m' incluye t = 0**), 'P' y 'A' en el último periodo. | host_test 4509 | Para el rizado hacen falta 'U' y 'u': máximo y mínimo **del último periodo**. |
| Las fuentes 6×12 y 6×8 tienen β (0x98) y γ (0x99); no hay λ. | font6x8.c, font6x12.c | La modulación de canal del MOSFET se da como VA = 1/λ, igual que el Early. |

**Qué tomo de cada propuesta (resumen).**
- **Motor:** Newton con los límites de SPICE3 (`pnjlim`, `fetlim`, `limvds`), criterio doble (incremento **o** residuo de Kirchhoff: el hallazgo de su prototipo del puente), gmin y rampa de fuentes, el paso de Katzenelson para los elementos por tramos, la regla de la realimentación del AO, los cambios de tramo como eventos localizados (Illinois) con el arranque de siempre, la guarda de Zenón, la subdivisión diádica dentro de la malla para los modelos exponenciales, la caché con clave de tramos, las controladas en la tabla (prueba cruzada entre los dos motores), la pequeña señal en el punto Q y el barrido por continuación.
- **Patas:** un solo `Elemento` con una tercera pata y una caja que ocupa varios segmentos; patas en el borde y puntos interiores bloqueados; las huellas del AO (3×3) y del transistor (2×3); `circ_cabe` con el motivo; `circ_grado` que cuenta conexiones; el espejo con [F]; `MAX_NODOS` = 70; el dibujo con un marco local; la sección 3 del archivo.
- **Modelos:** la unión con **Rs condensada** (sin nodo interno), el zener exponencial que pasa por (Vz, Izt) con pendiente rz, el LED con la Is que da justo Vf a su corriente, el catálogo, Ebers-Moll de transporte con Early, Shichman-Hodges con K·vov², las constantes (VT a 27 °C, GMIN) y la regla «un hueco, un significado».
- **Interfaz:** `[=]` con no lineales (punto de operación en P_RESULTADO o emular), `[P]`, `[I]`, P_BARRIDO con cursores exactos y lectura inversa, el trazador de curvas desde la página Modelo, el LED encendido por niveles, la fila del control de las controladas, los mensajes con nombre, las lecciones 12 y 13 y el oráculo no lineal.

---

## 1. Decisiones finales

**D1. Ocho tipos nuevos, al final del enumerado** (el número se guarda en el archivo):

| Tipo | N.º | Etiqueta | Tecla | Variantes (la 1.ª en negrita es la que sale al colocarlo) |
|---|---|---|---|---|
| `EL_DIODO` | 10 | D | [D] | ideal · **con Vγ** · exponencial · 1N4148 · 1N4007 |
| `EL_ZENER` | 11 | D | paleta | **de libro** · exponencial · 1N4733A |
| `EL_LED` | 12 | D | paleta | **rojo** · naranja · amarillo · verde · azul · blanco |
| `EL_EDEP` | 13 | E | paleta | **por una V** · por una I (rombo con + y −) |
| `EL_JDEP` | 14 | J | paleta | **por una V** · por una I (rombo con flecha) |
| `EL_AO` | 15 | U | [U] | **ideal** · real (3 patas) |
| `EL_BJT` | 16 | Q | [Q] | **NPN** · PNP (3 patas) |
| `EL_MOS` | 17 | M | [M] | **canal N** · canal P (3 patas; acumulación, cuerpo unido a S) |

- Zener y LED son tipos (no variantes del diodo): tienen su símbolo, sus campos y, el LED, su color como variante. Comparten la letra D (D1, D2, D3), porque Z ya es la impedancia.
- La E y la J controladas comparten numeración con las independientes; el rombo las distingue.
- *Descartado:* la propuesta de patas de seis tipos con zener y LED como variantes del diodo (el LED necesita `[izq/der]` para el color); los modelos «de libro» del BJT (estados con VBE = 0,7 fijo) y el catálogo de transistores (2N3904, 2N2222, 2N3906, 2N7000, BS250), que alargan la etapa sin cambiar el motor: **Etapa 5**. El 1N5819, el infrarrojo y los zener 1N4728A/4739A/4742A: fuera (el catálogo se amplía con una fila de tabla cuando haga falta).

**D2. Dos familias de modelo; la variante elegida es EL modelo, sin sustituciones ocultas.**
- **Por tramos** (lineales a trozos, continuos): diodo ideal y con Vγ, zener de libro y los dos AO. En cada tramo el elemento es lineal: la matriz solo depende de la combinación de tramos (va a la caché) y cambiar de tramo es un **evento** localizado en el tiempo, como un interruptor que acciona el propio circuito. Un rectificador con diodo de libro da la forma de onda del libro.
- **Suaves** (exponenciales): diodo exponencial y de catálogo, zener exponencial y de catálogo, LED, BJT y MOSFET. Newton con límites de unión.
- *Descartado:* «la calculadora siempre de libro y el emulador siempre físico» (el mismo diodo daría números distintos en `[=]` y al emular sin que el usuario cambie nada).

**D3. El diodo por tramos tiene resistencia de conducción y fuga, nunca es una rama ideal.**
- Corte: i = GMIN·v. Conducción: i = (v − Vγ)/Ron + GMIN·Vγ. Ruptura (zener): i = (v + Vz0)/Rz − GMIN·Vz0. Los tres tramos se tocan (la curva es continua y creciente).
- **Ron = rd** si rd es dato mayor que 0; si no, **RON = 1 mΩ**. El ideal es Vγ = 0 con RON. **GMIN = 1e-12 S.**
- *Por qué:* con conductancias nunca hay un lazo de ramas de tensión ni un nodo colgado de un diodo en corte (dos diodos ideales en paralelo, o uno sobre una E ideal, serían `SIM_LAZO_V` a mitad de una iteración). El paso de Katzenelson converge en un número finito de pasos con elementos continuos y crecientes. El prototipo del motor (`puente_pwl.py`) dio con RON = 1 mΩ el mínimo del rizado del puente a 1,3e-4 V de la cerrada.
- *Precio:* el ideal de 5 V sobre 1 kΩ da 4,999995 mA (se ve «5,000 mA»). Lo dice la página Modelo.
- *Descartado:* el diodo ideal como rama de tensión (modelos), y sus errores `SIM_IDEALES` e `IDEAL_CORTO` (interfaz): con RON un diodo ideal sobre una E ideal da 5 kA, que es la verdad del modelo, y el aviso QUEMA no hace falta porque no es un LED.

**D4. La unión exponencial lleva Rs condensada: no hay nodos internos.**
- Unión: ij(vd) = Is·(e^(vd/(n·VT)) − 1) − Ibv·e^(−(vd + BV)/VT) (el segundo término solo con BV > 0). Bornes: v = vd + Rs·ij e i = ij + GMIN·v.
- En cada iteración, el compañero en bornes es G = gj/(1 + Rs·gj) + GMIN e Ieq = (ij − gj·vd)/(1 + Rs·gj), con gj = dij/dvd en la vd de la iteración. Tras resolver, la vd nueva sale del modelo lineal, vd = v − Rs·(i − GMIN·v), y se limita con `pnjlim`.
- Es la eliminación exacta del nodo interno: las iteraciones son las de SPICE con el nodo, sin una incógnita más ni cambiar `mapa()`.
- `exp()` sigue en recta por encima de 80 (e^80·(1 + x − 80)).
- *Descartado:* el nodo interno (motor: `col_int` en `mapa`, `SimTopoCA` y la LU compleja) y la W de Lambert en el producto (modelos): la corriente exacta i(v) solo la necesitan el arnés y el oráculo, que la resuelven a su manera.

**D5. Zener.** De libro: tres tramos (D3) con Vγ = 0,7, Rz = rz (o RON) y **Vz0 = Vz − Rz·Iz**, así que pasa por (−Vz, −Iz). Exponencial y catálogo: la directa con Is = 1e-14 y n = 1; la ruptura con **Ibv = Iz, Rs = máx(rz − VT/Iz, 0) y BV = Vz − Rs·Iz**: pasa por (−Vz, −Iz) con pendiente rz. 1N4733A: Vz 5,1 V, Iz 49 mA, rz 7 Ω.

**D6. LED.** Siempre exponencial; la variante es el color, que fija Vf a If (20 mA por omisión), n y Rs:

| Color | Vf (V) | n | Rs (Ω) |
|---|---|---|---|
| rojo | 1,8 | 2 | 5 |
| naranja | 2,0 | 2 | 5 |
| amarillo | 2,1 | 2 | 5 |
| verde | 2,2 | 2 | 5 |
| azul | 3,2 | 3 | 15 |
| blanco | 3,2 | 3 | 15 |

- La Is se calcula para que la curva pase **exactamente** por (Vf, If) con GMIN incluida: ij* = If − GMIN·Vf; vd* = Vf − Rs·ij*; Is = ij*/(e^(vd*/(n·VT)) − 1). Con 5 V y 160 Ω un LED rojo lleva 20,000 mA, como en el libro.
- Vf e If se pueden cambiar en la ficha (vacío = los del color). Im («se quemaría», 30 mA por omisión) solo avisa.
- El brillo sigue a la corriente **media** (lo que ve el ojo) por niveles enteros (D21).

**D7. El AO: tres tramos, raíles V+ y V−, y la regla de la realimentación.**
- **Ideal:** un nulor. Tramo lineal: V⁺ − V⁻ = 0 con la corriente de salida libre (una rama). Saturado: Vsal = V+ o V−. No entra corriente por + ni por −.
- **Real:** u = A·(V⁺ − V⁻) recortado a [V−, V+] (tres tramos continuos), detrás de Ro, con Ri entre + y −. Con Ro = 0 es una rama (VCVS con tope); con Ro > 0, un Norton sin rama. En el tramo lineal es **exactamente** una VCVS de ganancia A: el inversor da −(Rf/R1)/(1 + (1 + Rf/R1)/A) sin error de modelo.
- La salida se mide respecto a la **referencia del subcircuito** (la tierra, o el − de la primera fuente independiente): la alimentación, que no se dibuja, es de ahí. Mismo convenio que la bobina de tensión del vatímetro. **Si el subcircuito del AO no tiene tierra ni fuente independiente** (un astable u otro oscilador dibujado sin [G]), no hay referencia: `SIM_AO_REF` con nombre («pon la tierra»); y si la salida cae en su propia referencia (atada a la tierra), también (§3.A). La biblioteca pone tierra en todos los AO.
- **Regla de la realimentación:** un AO en su tramo lineal solo vale si la realimentación instantánea es negativa (con los C como su compañero y las L como su compañero). Se prueba con **una sustitución más** con la misma LU (§3.F). Si es positiva o no hay lazo (LU singular), el lineal no vale y el AO va a una saturación: desde el lineal, a la del signo de u (u = 0 → V+); desde una saturación, a la otra. Sin la regla, un astable se quedaría quieto en su equilibrio inestable y un comparador ideal sería singular.
- *Descartado:* la saturación suave con tanh (interfaz: comprime ~4 % a media escala y en el astable Newton tendría que «saltar de rama» en un pliegue) y con softplus (modelos: pide Newton en cada paso y no da instantes exactos). Los tramos dan los instantes de conmutación exactos (el periodo del astable se compara con 2RC·ln 3), la LU va a la caché y Katzenelson converge.
- *Fuera (Etapa 5):* el polo dominante (GB), el slew rate y el AO de 5 patas.

**D8. Fuentes controladas: E o J de dos bornes, controladas por la V o la I de otro elemento de dos bornes.**
- El control es `Elemento.ref` (ya existe) y la variante (`DEP_V`, `DEP_I`). La ganancia es `Q_VAL` con signo: μ (V/V) o r (Ω) en la E; g (S) o β en la J.
- **La V y la I del que controla son las de su tabla**, con su convenio: en un pasivo V = φa − φb; en una fuente (E, J, E y J controladas) V = φb − φa; en un vatímetro V = φb; la I, de a hacia b. Lo que ve el usuario es lo que controla. Girar el elemento de control con [F] invierte su V y su I (la página Modelo lo dice).
- **Pueden controlar por su I:** R e instrumentos con R (I = V/R), E ideal y E real (I = (E − V)/r), J (su valor), voltímetro ideal (0), amperímetro ideal y vatímetro, C, L, S y E controlada (su rama). **No:** diodos, Z, J controlada, AO, BJT y MOSFET → `SIM_CTL_MAL` («controla con un amperímetro en serie», que es la fuente de 0 V de SPICE). Por su V puede controlar cualquier elemento de dos bornes.
- **La misma regla en los dos motores:** `circ_ctl_motivo(c, k)` (circuito.c) la aplica sobre el circuito y la usan la tabla (`SOL_CTL` con el motivo), la ficha y la línea de estado; `valida` (sim.c) la aplica sobre la `SimRed` con la misma lista. Así un circuito que la tabla resuelve también se emula (y al revés).
- *Descartado:* el signo aparte del control (`ref_sg`, patas): la ganancia ya lleva signo y el archivo no cambia. El elemento de 4 patas (control + y − propios). La rama forzada en una R o un diodo para leer su corriente (motor): la I de una R, de una E real o de un instrumento con R es una combinación de potenciales y del valor de la fuente, y se estampa sin rama.

**D9. BJT: Ebers-Moll de transporte con Early** (el Gummel-Poon de SPICE con VAF y sin IKF ni ISE), NPN y PNP con `pol` = ±1. Parámetros: β (βF), Is, VA (vacía = ∞) y βR. **Sin capacidades** (Cπ, Cμ: Etapa 5): el Bode de un emisor común enseña los polos de los condensadores de acoplo y de desacoplo, no el de alta frecuencia.

**D10. MOSFET: Shichman-Hodges (nivel 1)** con **ID = K·(VGS − Vt)² en saturación** (el convenio de Boylestad; K = ½·k′·W/L), K·(2·vov·vDS − vDS²) en la zona óhmica, ambas por (1 + vDS/VA); simétrico (con vDS < 0 cambian D y S); Vt se escribe en módulo (positiva también en el canal P). Sin capacidades.

**D11. El motor: un solo Newton, y todo lo lineal bit a bit.**
- Todo lo nuevo va detrás de `if (NL.n)` (`NL.n` cuenta D, AO, BJT y MOS; las controladas son lineales y no cuentan). Con `NL.n == 0` el camino es el de hoy: [34] («trozos iguales bit a bit»), el oráculo 92/92, el cruce fasor = trapecio de [46] y las 622 BMP no cambian.
- **Newton con límites de SPICE3** (`pnjlim` en cada unión, con Vcrit = n·VT·ln(n·VT/(√2·Is)); `fetlim` en VGS y `limvds` en VDS) y el **paso de Katzenelson** para los por tramos: se avanza hasta el primer cambio de tramo (θ) y se cambia ese solo.
- **Criterio doble:** converge si ningún límite actuó en la iteración y además (a) los incrementos son pequeños (|Δφ| ≤ 1e-6·máx(|φ|, |φ_ant|) + 1e-9·Vesc; ramas y corrientes de las uniones igual, con 1e-15 A + 1e-9·Iesc) **o** (b) el **residuo de Kirchhoff** con las corrientes de las uniones en su vd vale 1e-12 relativo fila a fila. *Por qué (b):* con los cuatro diodos del puente en corte el nodo común solo lo sujeta GMIN y su redondeo (eps·I/GMIN ≈ 1e-6 V) no pasa ningún criterio de incremento, mientras el residuo ya está en 1e-18 A (prototipo del motor: sin (b), `NO_CONVERGE` en t = 5,94 ms).
- **Solo por tramos: 0 LU por iteración** (la combinación de tramos va a la caché). **Con suaves: una LU completa por iteración** (la conductancia de una unión cambia 12 órdenes de magnitud y el pivote bueno cambia de fila; en la Nspire lo caro son las operaciones, no recorrer índices).
- *Descartado:* repetir el patrón de `lu.c` (ahorra ~1 %), el complemento de Schur y Newton-cuerda (motor, «P13 si hace falta»): Etapa 5 si el banco lo pide.

**D12. Punto de operación no lineal: la cadena de rescates.** Tras los rescates de `analiza` (los de hoy): (1) Newton con Katzenelson desde x = 0 (uniones en Vcrit, diodos por tramos en corte, AO en su lineal), 100 iteraciones; (2) **gmin en la diagonal** de cada nodo, de 1e-2 S a 0, con factor que se ablanda al fallar; (3) **rampa de fuentes** λ de 0 a 1 con paso adaptativo; (4) si nada converge, `SIM_NO_CONVERGE` con el elemento culpable. **Tope de 1,5 M de operaciones (~0,95 s)**, con `NC_TOPE` si se pasa: el punto de operación es una llamada **bloqueante** (`[=]`, `[P]`, el arranque del emulador, el punto Q, el primer punto del barrido), sin barra ni `[esc]`, así que no puede tardar más de un segundo. Aviso `HOMOTOPIA` si hizo falta (2) o (3). Con un AO, la regla del biestable (§3.F). **El OP no toca el estado de la emulación:** ni la caché del transitorio ni `res_` (sus contadores van a un `SimNewton` que le pasa quien llama, §2.6). *Descartado:* la continuación exacta por tramos (motor): el mismo Newton con la rampa basta y es un solo camino de código. *Descartado:* el OP por trozos con barra (haría falta guardar el estado de las homotopías entre ticks y una pantalla más; con el tope de 1,5 M no hace falta).

**D13. En el tiempo.**
- **Por tramos: los cambios de tramo son eventos.** Cada paso se resuelve con los tramos congelados (una sustitución). Si al final algún margen queda negativo, el instante t* se localiza con **Illinois** (regula falsi modificada, pasos parciales con `lu_suelta`), se acepta el paso hasta t*, se cambian los tramos y se hace el **arranque de siempre** (8 pasos de Euler de h/8). El primer paso tras cualquier evento decide los tramos con Katzenelson, sin localizar (dos diodos que llegan a la vez, como D1 y D4 del puente, no se repiten sin fin). **Guarda de Zenón:** más de 4·MAX_EL eventos en el mismo instante da `SIM_NO_CONVERGE` («conmuta sin parar»). **La regla del AO (§3.F) se evalúa siempre con la matriz de paso h** (la del trapecio con esos tramos), también en el arranque y en los pasos partidos: con la del arranque (h/4) podría decir otra cosa y el AO conmutaría en cada evento hasta la guarda de Zenón. **Tras un evento, un diodo por tramos que conduce con R_τ·C < h hace algebraico a su C** (la historia BDF2): así el modo Ron·C, mucho más rápido que h, no queda zumbando en el trapecio (§3.B).
- **Suaves: Newton en cada paso** con predictor lineal y **subdivisión diádica dentro del paso de la malla** (h/2^m, m ≤ 10), sin reanclar: se parte si Newton no converge en 12 iteraciones, si el error local del trapecio en un C o una L pasa de 1e-4 de su escala, o si la corriente de un no lineal cambia más del 10 % de su máximo. En el prototipo del motor, eso bajó el error de la corriente del condensador del puente de 1,04 A a 0,012 A.
- *Descartado:* el detector de oscilación del trapecio (motor, «red de seguridad»): el arranque de Euler tras cada evento y la subdivisión ya lo cubren; queda como riesgo (§8).

**D14. CA y Bode con no lineales: pequeña señal en el punto Q.** Q es el punto de operación con cada fuente en su nivel de CC (alterna → 0; senoidal → su CC; cuadrada y triangular → su media; pulso → su nivel V1), los C y L sin V0 ni I0 y los S en su estado final (como la CA). Luego cada no lineal entra linealizado (su tramo, o su jacobiano) y las fuentes de CC se apagan. Un AO saturado en Q no pasa señal (aviso `SATURADO`). **`[=]` con una fuente alterna y un no lineal emula en el tiempo** (un rectificador en fasores no tiene sentido); la pequeña señal se ve con **`[B]`** (el Bode) y, desde él, `[tab]` a P_CA en el cursor. Thévenin y la calculadora de CA son de redes lineales (con controladas, sí): con un no lineal dan `SIM_NO_LINEAL` con nombre. *Descartado:* el Thévenin de pequeña señal (motor).

**D15. La calculadora de CC (la tabla).** Con un diodo, zener, LED, AO, BJT o MOSFET da **`SOL_NO_LINEAL`** en su primera línea (tras `SOL_CA`), con el mensaje que lleva a `[I]`. **Las fuentes controladas sí entran en la tabla**: son una ecuación lineal más (V_E − μ·V_k = 0, I_J − β·I_k = 0…), y en la tabla la I de cada elemento ya es incógnita. **Admite los mismos controles que el emulador** (`circ_ctl_motivo`, D8): uno que el emulador rechaza (por la I de una J controlada, un 3 patas, ella misma, ninguno) da `SOL_CTL` con el motivo, aunque la tabla pudiera resolverlo. Así tienen la **prueba cruzada entre los dos motores** de la regla de admisión, y su ganancia se despeja con `[?!]` como una R (su ley no se escribe; al final μ = V_E/V_k). *Descartado:* el método de estados en `resolver()` (modelos, hasta 256 combinaciones de tramos): alarga resuelve.c y el barrido con lectura inversa ya despeja «¿qué R da 20 mA?» con cualquier modelo.

**D16. `[=]`, `[P]` e `[I]`.**
- **`[=]`:** alterna con no lineal → emula; sin alterna y con no lineal: con un `[?!]` en un valor → `SOL_NO_LINEAL`; si es dinámico → emula; si no → **punto de operación de Newton en P_RESULTADO** (`res_nl`), con capas, tabla (con la región de cada no lineal), el LED encendido y la línea de Newton. Un `[?!]` en una V o una I es un **pedido** (se destaca). Un circuito sin tipos nuevos, igual que hoy.
- **`[P]`** (y la opción 9 del menú): el punto de operación de CC de **cualquier** circuito: C abiertos, L cables, sin V0 ni I0, fuentes en su nivel de CC, S al empezar (la vista de la calculadora de CC). Sirve para el punto Q de un emisor común con condensadores de acoplo.
- **`[I]`** (y la opción 8): el barrido de CC (D17).

**D17. Barrido de CC: una pantalla nueva, P_BARRIDO** (`barrido.c`, el motor por trozos como `bode.c`; `curva.c`, la pantalla como `trazador.c`).
- Barre una E, una J, el valor de una R o la ganancia de una controlada; todo en la vista de CC de `[P]`. Cada punto sigue al anterior (continuación): la vuelta de un Schmitt sale sola (histéresis).
- Dibuja las sondas (o canales automáticos), con cursores de lectura **exacta** (un Newton en ese x), la derivada exacta (ganancia, r dinámica), la **lectura inversa `[N]`** («¿dónde vale 20 mA?»), X-Y, |y| logarítmico y ida y vuelta.
- **Trazador de curvas:** `[I]` en la página Modelo de un diodo, zener o LED da su curva I-V (E con 1 Ω en serie, X-Y); en la de un BJT o un MOSFET, la familia de 6 curvas (IC(VCE) con IB en pasos; ID(VDS) con VGS en pasos). Es el «IV Analyzer» de Multisim.
- *Descartado:* reutilizar el osciloscopio (su eje es el tiempo) o el trazador de Bode (eje log f, dos gráficas fijas); las esquinas exactas de los por tramos añadidas a la curva (motor): una esquina cae dentro de un píxel de la malla de 281 y el cursor lee exacto.

**D18. Componentes de 3 patas en el editor: un `Elemento` con una tercera pata y una caja.**
- `Elemento` gana `xc, yc` (la pata c). La caja es la **envolvente de las tres patas**: todos sus segmentos valen `SEG_EL0 + k`. Las patas son puntos del borde; los demás puntos de la caja son **interiores** (bloqueados: ni cables, ni elementos, ni tierra, ni terminal, ni sonda).
- **Huellas** (u a lo largo de la flecha con que se coloca; v hacia abajo si la flecha es horizontal y hacia la derecha si es vertical; el ancla (0,0) es el cursor):

| Tipo | Caja (u × v) | a | b | c | Ancla | El cursor salta a |
|---|---|---|---|---|---|---|
| AO | 0..2 × 0..2 (12 segmentos) | + (0,2) | − (0,0) | salida (2,1) | − | la salida |
| BJT | 0..1 × −1..1 (7 segmentos) | C (1,−1) | E (1,1) | B (0,0) | B | C |
| MOSFET | igual que el BJT | D (1,−1) | S (1,1) | G (0,0) | G | D |

- Con esa asignación V = φa − φb e I de a hacia b son V_CE e I_C (V_DS e I_D), lo que la tabla ya enseña.
- **Colocar es tecla + flecha**, como hoy; si no cabe, no se sustituye nada: se dice el motivo, se marca en rojo el punto que estorba y la acción sigue para probar otra flecha (las que no caben salen en gris). **[F] es el espejo** en su sitio (intercambia a y b: + y − del AO, C y E, D y S); la caja no cambia, así que siempre cabe. Girar uno ya puesto es borrarlo y volverlo a poner (`[ctrl][Z]` protege).
- **No se guarda el giro:** las tres patas bastan para dibujarlo y `circ_valido` comprueba que forman una huella en alguna de las 8 orientaciones.
- **Sin sondas en las patas** en esta etapa: la tensión de una pata es la sonda de nodo de siempre y su corriente, un amperímetro en serie (como en el laboratorio). *Aplazado a la Etapa 5* (patas: `Sonda.pata`, «IC(Q1)», «Vsal(U1)»).
- *Descartado:* el «elemento compuesto» de varios elementos de dos bornes (borrado no atómico, mitades en el archivo) y una tabla aparte de patas.

**D19. Archivo: EFK3 versión 4.** Sección 3 del bloque del elemento (2 bytes: `xc`, `yc`) solo en los de 3 patas; **versión 4 si hay algún tipo ≥ `EL_DIODO`**, comprobado **antes** de la regla `tipo >= EL_IMP → 3`. Los archivos de hoy salen byte a byte iguales; un lector de la Etapa 3 rechaza la versión 4. Las controladas no piden nada (`ref` y la variante ya se guardan).

**D20. Tamaños.** `MAX_NODOS` = `REJ_PUNTOS` = 70 (un nodo tiene al menos un punto: nunca se queda corto). Las variantes de los tipos nuevos se validan (`variante < nvar`) antes de indexar `NOM_*` o `NL_COLOR`. `LU_MAX` sigue en 64: un circuito con 31 elementos aislados y un AO pasa de 64 incógnitas y da `SIM_GRANDE` con su mensaje; subirlo agranda las seis LU de sim.c y la compleja sin ningún caso real que lo pida.

**D21. El LED se enciende** en P_RESULTADO (con el punto de operación) y en P_EMU_ESQ (con la corriente media de la ventana de medida). No en el editor, el osciloscopio, P_CA ni P_EMULANDO. Brillo entero de 0 a 9: 0 si I < If/100; 1 a 8 = redondeo de 8·I/If (entre 1 y 8); 9 si I > Im («se quemaría», aviso `QUEMA`). La mezcla de color es entera.

**D22. Thévenin y Norton** siguen siendo de redes lineales, **ahora con fuentes controladas**: la inyección de 1 A con las fuentes independientes apagadas lleva dentro las controladas (el ejercicio clásico «Thévenin con fuentes dependientes»). Con un no lineal, `SIM_NO_LINEAL`.

**D23. Biblioteca:** lección 12 «Diodos» y lección 13 «AO y transistores», 10 modelos cada una (los modelos 23 a 42; la lección 13 tiene 8 hasta el paso 13 del §7, que añade el 41 y el 42 con el BJT). `CircModelo.ca` gana 3 (punto de operación) y 4 (barrido de CC), con su rango.

**D24. Verificación en siete cruces:**
1. Controladas: el emulador = la tabla en 2000 redes al azar (y fasor(ω̃) = trapecio con ellas).
2. **Residuo de Kirchhoff** de cada solución de Newton, calculado por el arnés aparte (`kcl_de`, `ley_de`).
3. Casos cerrados del diodo, del zener, del LED, del AO y del MOSFET (mpmath a 40 cifras).
4. **AO ideal frente a ganancia 1e7** en 200 redes lineales al azar (orden 1 en 1/A).
5. Rectificadores por tramos frente a su régimen permanente cerrado; exponenciales frente a Radau.
6. Pequeña señal frente a la derivada de la curva del barrido (r_d, ganancias).
7. Oráculo no lineal independiente (Python), en el último paso.

**D25. Qué queda fuera de la Etapa 4:** el BJT y el MOSFET de libro y de catálogo; las capacidades de las uniones (Cj, Cπ, Cμ, Cgs, Cgd) y la recuperación inversa; el polo, el slew rate y la alimentación del AO; las sondas en las patas; girar en su sitio; el método de estados en la calculadora de CC; el paso a paso (`proced.c`) con fuentes controladas (la tabla sí las resuelve); la calculadora de CA y Thévenin con no lineales; el complemento de Schur; el detector de oscilación del trapecio; las esquinas exactas del barrido; biestables de transistores (el punto de operación da el que da la homotopía).

---

## 2. Estructuras C exactas

### 2.1 `tipos.h`: cambios (todo al final de cada enumerado)
```c
typedef enum { EL_RES = 0, EL_FUENTE, EL_CORR, EL_VOLT, EL_AMP,
               EL_COND, EL_BOB, EL_INTER, EL_IMP, EL_VATI,
               EL_DIODO,               /* 10 diodo (tecla D): a = ánodo, b = cátodo         */
               EL_ZENER,               /* 11 zener (paleta): la raya con ganchos           */
               EL_LED,                 /* 12 LED (paleta): se enciende en el resultado     */
               EL_EDEP,                /* 13 E controlada (paleta): rombo, b = +           */
               EL_JDEP,                /* 14 J controlada (paleta): rombo, de a hacia b    */
               EL_AO,                  /* 15 amp. op. (tecla U): a = +, b = -, c = salida  */
               EL_BJT,                 /* 16 transistor bipolar (tecla Q): a = C, b = E, c = B */
               EL_MOS,                 /* 17 MOSFET (tecla M): a = D, b = S, c = G          */
               EL_N } TipoEl;
enum { LEY_VALOR = 0, LEY_ABIERTO, LEY_CORTO, LEY_INTER, LEY_CA,
       LEY_NL,                         /* D, zener, LED, AO, BJT, MOS: SOL_NO_LINEAL        */
       LEY_DEP };                      /* E y J controladas: su ley nombra a otro elemento  */

enum { DIO_IDEAL = 0, DIO_VG, DIO_EXP, DIO_1N4148, DIO_1N4007, DIO_N };
enum { ZEN_LIBRO = 0, ZEN_EXP, ZEN_1N4733A, ZEN_N };
enum { LED_ROJO = 0, LED_NARANJA, LED_AMARILLO, LED_VERDE, LED_AZUL, LED_BLANCO, LED_N };
enum { DEP_V = 0, DEP_I, DEP_N };      /* controlada por la V o por la I de Elemento.ref    */
enum { AO_IDEAL = 0, AO_REAL, AO_N };
enum { BJT_NPN = 0, BJT_PNP, BJT_N };
enum { MOS_CN = 0, MOS_CP, MOS_N };
#define Q_CTRL 0xFD                    /* pseudo-magnitud de la ficha: el control (ref)     */

/* Huecos Q_P* de cada tipo: UNO por significado (tipo_rango y circ_valido toman la
 * primera fila de cada q). Vacío = el valor por omisión de nolin.h. */
#define QD_VG  Q_P1   /* diodo: Vγ (con Vγ)          */
#define QD_RD  Q_P2   /*        rd (con Vγ)          */
#define QD_IS  Q_P3   /*        Is (exponencial)     */
#define QD_N   Q_P4   /*        n  (exponencial)     */
#define QD_RS  Q_P5   /*        Rs (exponencial)     */
#define QZ_VZ  Q_P1   /* zener: Vz a Iz              */
#define QZ_RZ  Q_P2   /*        rz (dinámica en Iz)  */
#define QZ_IZ  Q_P3   /*        Iz (la de prueba)    */
#define QZ_VG  Q_P4   /*        Vγ de la directa (de libro) */
#define QL_VF  Q_P1   /* LED: Vf a If (vacía = la del color) */
#define QL_IF  Q_P2   /*      If (brillo pleno)      */
#define QL_IM  Q_P3   /*      Im (se quemaría)       */
#define QA_A   Q_P1   /* AO: A (real)                */
#define QA_VP  Q_P2   /*     V+ (saturación de arriba) */
#define QA_VN  Q_P3   /*     V- (saturación de abajo)  */
#define QA_RI  Q_P4   /*     Ri (real; vacía = infinita) */
#define QA_RO  Q_P5   /*     Ro (real; vacía = 0)    */
#define QB_BF  Q_P1   /* BJT: β                      */
#define QB_IS  Q_P2   /*      Is                     */
#define QB_VA  Q_P3   /*      VA (vacía = sin Early) */
#define QB_BR  Q_P4   /*      βR                     */
#define QM_VT  Q_P1   /* MOS: Vt (en módulo)         */
#define QM_K   Q_P2   /*      K                      */
#define QM_VA  Q_P3   /*      VA = 1/λ (vacía = infinita) */

/* ---- Huella de un componente de 3 patas ----
 * Coordenadas locales: u a lo largo de la flecha con que se colocó; v hacia abajo
 * si la flecha es horizontal y hacia la derecha si es vertical. El ancla (0,0) es
 * el punto del cursor al colocarlo. El espejo cambia v por v0 + v1 - v: la caja no
 * se mueve, a y b se intercambian y c se queda (pv[c] = (v0 + v1)/2). */
#define NPATAS 3
typedef struct Huella {
    signed char u1, v0, v1;            /* la caja: u de 0 a u1; v de v0 a v1              */
    signed char pu[NPATAS], pv[NPATAS];/* patas a, b, c                                   */
    unsigned char ancla;               /* la pata que está en (0,0)                       */
    unsigned char sigue;               /* a qué pata salta el cursor al colocarlo         */
    const char *nom[NPATAS];           /* "+", "-", "sal" / "C", "E", "B" / "D", "S", "G" */
} Huella;

/* TipoInfo, al final (NULL en las filas de hoy) */
    const struct Huella *huella;       /* NULL: dos bornes                                */
    const char *const *modelo_var;     /* página Modelo de cada variante (tras `modelo`)  */
```

### 2.2 `tipos.c`: las filas nuevas
`NB` = `CF_NO_BUSCAR`, `SG` = `CF_SIGNO`. Las ayudas tienen 51 columnas como mucho (`lineas()`); los nombres, 2. En los **no lineales** (diodo, zener, LED, AO, BJT, MOS) Q_V y Q_I no llevan `FA`: con un no lineal no hay fasores (`[=]` emula; el Bode es de pequeña señal y no lee datos). **En las controladas sí llevan `FA`**, como la V y la I de una E o una J fasor: las controladas funcionan en la CA y en su calculadora, así que un dato de V o de I tiene su caja de ángulo y, con `CONV_PICO`, se guarda el eficaz (ui.c 1268 divide por √2 solo con `CF_AMPL`). La página Modelo (`modelo`) es el texto común; `modelo_var[variante]` va debajo (§4.10).

```c
static const Huella H_AO  = { 2, 0, 2,  { 0, 0, 2 }, { 2, 0, 1 },  1, 2, { "+", "-", "sal" } };
static const Huella H_BJT = { 1, -1, 1, { 1, 1, 0 }, { -1, 1, 0 }, 2, 0, { "C", "E", "B" } };
static const Huella H_MOS = { 1, -1, 1, { 1, 1, 0 }, { -1, 1, 0 }, 2, 0, { "D", "S", "G" } };
const char *const NOM_DIODO[DIO_N] = { "ideal (interruptor)", "con V" S_GAMMA " (codo)",
                                       "exponencial (Shockley)", "1N4148 (se" S_nn "al)", "1N4007 (1 A)" };
const char *const NOM_ZENER[ZEN_N] = { "de libro (Vz, rz)", "exponencial", "1N4733A (5,1 V)" };
const char *const NOM_LED[LED_N]   = { "rojo", "naranja", "amarillo", "verde", "azul", "blanco" };
const char *const NOM_DEP[DEP_N]   = { "por una V", "por una I" };
const char *const NOM_AO[AO_N]     = { "ideal", "real (A, Ri, Ro)" };
const char *const NOM_BJT[BJT_N]   = { "NPN", "PNP" };
const char *const NOM_MOS[MOS_N]   = { "canal N", "canal P" };
```

| Tipo | letra, tecla | nombre | al_poner (≤ 52 col.) | color | polar, fuente, pasivo, ideal | val_min..max | ley_cc | nvar | huella |
|---|---|---|---|---|---|---|---|---|---|
| DIODO | 'D', 'D' | "Diodo" | "Diodo: flecha hacia el c" S_aa "todo (la raya)" | C_DIODO | 1, 0, 0, 0 | 0..0 | LEY_NL | DIO_N | NULL |
| ZENER | 'D', 0 | "Diodo zener" | "Zener: flecha hacia el c" S_aa "todo (rompe en inversa)" | C_DIODO | 1, 0, 0, 0 | 0..0 | LEY_NL | ZEN_N | NULL |
| LED | 'D', 0 | "LED" | "LED: flecha hacia el c" S_aa "todo; se enciende" | C_DIODO | 1, 0, 0, 0 | 0..0 | LEY_NL | LED_N | NULL |
| EDEP | 'E', 0 | "E controlada" | "E controlada: flecha hacia el borne +" | C_FUENTE | 1, **0**, 0, 0 | −1e9..1e9 | LEY_DEP | DEP_N | NULL |
| JDEP | 'J', 0 | "J controlada" | "J controlada: flecha = sentido de J" | C_FUENTE_I | 1, **0**, 0, 0 | −1e9..1e9 | LEY_DEP | DEP_N | NULL |
| AO | 'U', 'U' | "Amp. operacional" | "Amp. op.: flecha hacia la salida (el - aqu" S_ii ")" | C_AO | 1, 0, 0, 0 | 0..0 | LEY_NL | AO_N | &H_AO |
| BJT | 'Q', 'Q' | "Transistor BJT" | "Transistor: flecha de la base a C y E" | C_TRANS | 1, 0, 0, 0 | 0..0 | LEY_NL | BJT_N | &H_BJT |
| MOS | 'M', 'M' | "MOSFET" | "MOSFET: flecha de la puerta G a D y S" | C_TRANS | 1, 0, 0, 0 | 0..0 | LEY_NL | MOS_N | &H_MOS |

- `polar = 1` en todos: [F] gira un dos bornes y hace el espejo de uno de 3 patas.
- `fuente = 0` en las controladas: no son independientes (no cuentan para `RED_SIN_FUENTE`, para la referencia de `red_cierra` ni para «apagar fuentes»).
- `dinamico = 0` en todos.

**Campos** (q, nombre, unidad, ayuda, vars, flags, vacío, mín, máx):
```c
/* EL_DIODO (7) */
{ QD_VG, "V" S_GAMMA, "V", "Tensi" S_oo "n de codo: conduce a partir de ella", VM(DIO_VG), NB, "0,7 V", 0.0, 5.0 },
{ QD_RD, "rd", S_OHM, "Resistencia al conducir; vac" S_ii "a = 1 m" S_OHM, VM(DIO_VG), NB, "1 m" S_OHM, 0.0, 1e6 },
{ QD_IS, "Is", "A",   "Corriente de saturaci" S_oo "n",                  VM(DIO_EXP), NB, "10 fA", 1e-20, 1e-3 },
{ QD_N,  "n",  "",    "Factor de idealidad (de 1 a 2)",                   VM(DIO_EXP), NB, "1", 0.5, 10.0 },
{ QD_RS, "Rs", S_OHM, "Resistencia en serie; vac" S_ii "a = 0",           VM(DIO_EXP), NB, "0", 0.0, 1e3 },
{ Q_V,   "V",  "V",   "Tensi" S_oo "n " S_aa "nodo - c" S_aa "todo",      0, 0, 0, 0, 0 },
{ Q_I,   "I",  "A",   "Corriente de " S_aa "nodo a c" S_aa "todo",        0, 0, 0, 0, 0 },
/* EL_ZENER (6) */
{ QZ_VZ, "Vz", "V",   "Tensi" S_oo "n zener (en inversa, a Iz)",          VM(ZEN_LIBRO) | VM(ZEN_EXP), NB, "5,1 V", 0.5, 1000.0 },
{ QZ_RZ, "rz", S_OHM, "Resistencia en ruptura (a Iz)",                    VM(ZEN_LIBRO) | VM(ZEN_EXP), NB, "0", 0.0, 1e4 },
{ QZ_IZ, "Iz", "A",   "Corriente a la que se da Vz",                      VM(ZEN_LIBRO) | VM(ZEN_EXP), NB, "5 mA", 1e-6, 10.0 },
{ QZ_VG, "V" S_GAMMA, "V", "Tensi" S_oo "n de codo en directa",           VM(ZEN_LIBRO), NB, "0,7 V", 0.0, 5.0 },
{ Q_V, "V", "V", "Tensi" S_oo "n " S_aa "nodo - c" S_aa "todo (-: inversa)", 0, 0, 0, 0, 0 },
{ Q_I, "I", "A", "Corriente de " S_aa "nodo a c" S_aa "todo",             0, 0, 0, 0, 0 },
/* EL_LED (5) */
{ QL_VF, "Vf", "V", "Tensi" S_oo "n a If; vac" S_ii "a = la de su color", 0, NB, "del color", 0.5, 6.0 },
{ QL_IF, "If", "A", "Corriente de Vf y de brillo pleno",                  0, NB, "20 mA", 1e-6, 10.0 },
{ QL_IM, "Im", "A", "M" S_aa "xima: con m" S_aa "s, se quemar" S_ii "a",  0, NB, "30 mA", 1e-6, 100.0 },
{ Q_V, "V", "V", "Tensi" S_oo "n " S_aa "nodo - c" S_aa "todo",           0, 0, 0, 0, 0 },
{ Q_I, "I", "A", "Corriente de " S_aa "nodo a c" S_aa "todo",             0, 0, 0, 0, 0 },
/* EL_EDEP (6): la ganancia cambia de nombre y unidad con la variante (el mismo hueco) */
{ Q_VAL,  S_MU, "",    "E = " S_MU S_CDOT "V(control)",                   VM(DEP_V), SG, "falta", 0, 0 },
{ Q_VAL,  "r",  S_OHM, "E = r" S_CDOT "I(control)",                       VM(DEP_I), SG, "falta", 0, 0 },
{ Q_CTRL, "x",  "",    "Qui" S_ee "n la manda: [+][-] elige el elemento", 0, NB, "falta", 0, 0 },
{ Q_V,    "V",  "V",   "Tensi" S_oo "n en sus bornes (+ en b)",           0, FA, 0, 0, 0 },
{ Q_I,    "I",  "A",   "Corriente que entrega (sale por +)",              0, FA, 0, 0, 0 },
/* EL_JDEP (5): igual, con { Q_VAL, "g", "S", "J = g" S_CDOT "V(control)", VM(DEP_V) } y
 * { Q_VAL, S_BETA, "", "J = " S_BETA S_CDOT "I(control)", VM(DEP_I) }, la fila x, y V e I con FA */
/* EL_AO (7) */
{ QA_A,  "A",  "",    "Ganancia en lazo abierto",                         VM(AO_REAL), NB, "100 000", 1.0, 1e9 },
{ QA_VP, "V+", "V",   "La salida no pasa de V+ (satura)",                 0, NB | SG, "15 V", -1e3, 1e3 },
{ QA_VN, "V-", "V",   "La salida no baja de V- (satura)",                 0, NB | SG, "-15 V", -1e3, 1e3 },
{ QA_RI, "Ri", S_OHM, "Resistencia entre + y -; vac" S_ii "a = infinita", VM(AO_REAL), NB, "infinita", 1.0, 1e15 },
{ QA_RO, "Ro", S_OHM, "Resistencia de salida; vac" S_ii "a = 0",          VM(AO_REAL), NB, "0", 0.0, 1e6 },
{ Q_V,   "V",  "V",   "Vsal: salida respecto a la tierra",                0, 0, 0, 0, 0  },
{ Q_I,   "I",  "A",   "Corriente que entrega la salida",                  0, 0, 0, 0, 0  },
/* EL_BJT (6) */
{ QB_BF, S_BETA, "",  "Ganancia de corriente (IC/IB en activa)",          0, NB, "100", 1.0, 1e5 },
{ QB_IS, "Is", "A",   "Corriente de saturaci" S_oo "n",                   0, NB, "1 fA", 1e-20, 1e-6 },
{ QB_VA, "VA", "V",   "Tensi" S_oo "n de Early; vac" S_ii "a = sin Early", 0, NB, "infinita", 1.0, 1e4 },
{ QB_BR, S_BETA "r", "", "Ganancia inversa (C y E cambiados)",            0, NB, "1", 1e-3, 1e3 },
{ Q_V,   "V",  "V",   "VCE: colector menos emisor",                       0, 0, 0, 0, 0  },
{ Q_I,   "I",  "A",   "IC: la que entra por el colector",                 0, 0, 0, 0, 0  },
/* EL_MOS (5) */
{ QM_VT, "Vt", "V",   "Umbral: conduce con |VGS| > Vt",                   0, NB, "2 V", 0.0, 100.0 },
{ QM_K,  "K",  "A/V" S_SUP2, "ID = K" S_CDOT "(VGS - Vt)" S_SUP2 " saturado", 0, NB, "1 mA/V" S_SUP2, 1e-9, 1e3 },
{ QM_VA, "VA", "V",   "Modulaci" S_oo "n del canal; vac" S_ii "a = sin ella", 0, NB, "infinita", 1.0, 1e4 },
{ Q_V,   "V",  "V",   "VDS: drenador menos surtidor",                     0, 0, 0, 0, 0  },
{ Q_I,   "I",  "A",   "ID: la que entra por el drenador",                 0, 0, 0, 0, 0  },
```
(No hay λ en las fuentes de letra: la modulación del canal se da como VA = 1/λ, igual que el Early. Los datos de V e I son **magnitudes**, como en los tipos de hoy: sin `CF_SIGNO`; con un no lineal no son ecuaciones, sino el nivel de la lectura inversa del barrido y una comprobación en el resultado, §4.5.)

**Vacíos:** los por omisión viven en `nolin.h` (§2.7) y el adaptador los aplica; la ficha los enseña con el rótulo de `vacio`. `QA_VP`/`QA_VN`: `circ_valido` y la ficha exigen V+ > V- (con los vacíos, 15 y −15); al escribir uno que lo rompa: «V+ tiene que ser mayor que V-.» (`AV_FUERA`).

**Páginas Modelo:** §4.10.

### 2.3 `circuito.h` / `circuito.c`
```c
typedef struct {
    ... /* lo de hoy, sin cambios, hasta par[NPAR] */
    /* Etapa 4: la 3.ª pata (AO: salida; BJT: B; MOSFET: G); 0 y 0 en los de dos bornes */
    unsigned char xc, yc;
} Elemento;

int  circ_npatas(const Elemento *e);                         /* 2 o 3 (tipo_info()->huella)     */
void circ_pata(const Elemento *e, int p, int *x, int *y);    /* p: 0 = a, 1 = b, 2 = c          */
int  circ_pata_en(const Elemento *e, int x, int y);          /* la pata que está ahí, o -1      */
/* El punto (u, v) de la huella del tipo, colocada con el ancla en (ox, oy), la flecha d y
 * espejo esp (0 o 1) */
void circ_huella_pt(int tipo, int d, int esp, int ox, int oy, int u, int v, int *x, int *y);
void circ_caja(const Elemento *e, int *x0, int *y0, int *x1, int *y1);  /* envolvente de las patas */
/* La flecha, el espejo y el ancla con que se colocó (de las tres patas); 0 si no forman una huella */
int  circ_orienta(const Elemento *e, int *d, int *esp, int *ox, int *oy);
/* De quién es cada punto de la rejilla: el de 3 patas cuya caja lo tiene (-1 si ninguno) y
 * la pata que está ahí (-1 si es interior). 0 si dos cajas se pisan. */
int  circ_mapa_huellas(const Circuito *c, signed char dueno[REJ_PUNTOS], signed char pata[REJ_PUNTOS]);
int  circ_interior(const Circuito *c, int x, int y);         /* dueño si (x,y) es interior, o -1 */

enum { CM_OK = 0, CM_FUERA, CM_OCUPADO, CM_MARCA, CM_LLENO };
typedef struct {
    int  motivo;                   /* CM_*                                                    */
    int  x, y;                     /* el primer punto que estorba (en orden de lectura)        */
    int  el;                       /* CM_OCUPADO: el elemento que estorba; -1 = un cable       */
    char que;                      /* CM_MARCA: 'G' tierra, 'A'/'B' terminal, '1'/'2' sonda    */
} CabeInfo;
int  circ_cabe(const Circuito *c, int tipo, int x, int y, int d, CabeInfo *ci);
/* ¿Puede ir ahí un cable o un elemento de dos bornes? 0 si el segmento es de una caja o
 * toca un punto interior (en *k, su dueño) */
int  circ_seg_libre(const Circuito *c, int x, int y, int d, int *k);

int  circ_etapa4(const Circuito *c);       /* algún tipo >= EL_DIODO                       */
int  circ_no_lineal(const Circuito *c);    /* D, zener, LED, AO, BJT o MOS                 */
int  circ_busca_valor(const Circuito *c);  /* algún [?!] en Q_VAL o Q_AUX (una incógnita)   */
/* El signo de la V de la tabla: +1 φa - φb (pasivos, instrumentos, 3 patas), -1 φb - φa
 * (E, J y controladas), 2 φb (vatímetro). Los dos motores lo usan para el control. */
int  circ_v_convenio(int tipo);
/* Controladas: el índice del siguiente (dir = +1) o del anterior (-1) elemento de dos
 * bornes que no es j, con vuelta; desde -1, el primero o el último. -1 si no hay ninguno
 * (0 es un índice válido: E1). No cambia nada: la ficha pone ref con lo que devuelve. */
int  circ_control_sig(const Circuito *c, int j, int dir);
/* La regla de admisión del control de la controlada k (D8), la misma en los dos motores:
 * CTL_OK, o por qué no vale. La usan resolver (SOL_CTL), la ficha y la línea de estado;
 * valida (sim.c) aplica la misma lista sobre la SimRed. */
enum { CTL_OK = 0, CTL_FALTA, CTL_SI_MISMA, CTL_3PATAS, CTL_I_NO };
int  circ_ctl_motivo(const Circuito *c, int k);
```
`circ_ctl_motivo`: `ref` fuera de [0, nel) → CTL_FALTA; `ref == k` → CTL_SI_MISMA; el referido es de 3 patas → CTL_3PATAS; variante `DEP_I` y el referido es diodo, zener, LED, Z o J controlada → CTL_I_NO; si no, CTL_OK.
- `circ_elemento(c, t, x, y, d)` no cambia de firma. Con un tipo de 3 patas coloca la huella si `circ_cabe` da `CM_OK` (§3.N) y si no devuelve −1 **sin tocar nada**. Con un tipo de dos bornes devuelve −1 si `!circ_seg_libre` y, si no, sustituye como hoy. Pone la variante por omisión (D1) y, en las controladas, `ref = -1`.
- `circ_cable` devuelve 0 si `!circ_seg_libre`.
- `circ_grado` cuenta **conexiones** (§3.N).
- `circ_voltear` de un 3 patas intercambia (xa, ya) y (xb, yb) (el espejo).
- `circ_valido`: §3.N (con `variante < nvar` en **todos** los tipos nuevos).

### 2.4 `red.h`
```c
#define MAX_NODOS REJ_PUNTOS   /* 70: un nodo tiene al menos un punto (con 3 patas caben 65) */
typedef struct { ...; int c[MAX_EL];          /* nodo de la pata c; -1 en los de dos bornes */ ... } Red;
/* La referencia (0 V) del subcircuito del nodo n: la de red_cierra (ref[m] && comp[m] ==
 * comp[n]); -1 si n < 0. La salida de un AO se mide desde ahí. */
int  red_ref_de(const Red *r, int n);
/* 1 si esa referencia es de verdad: la tierra está en el subcircuito de n, o el - de una
 * fuente independiente (tipo_info()->fuente). 0 si red_cierra tomó «su primer nodo». */
int  red_ref_propia(const Circuito *c, const Red *r, int n);
```

### 2.5 `archivo.c`
```c
#define VERSION3_MAX 4
#define ELEM3_BYTES 2          /* xc, yc: solo en los de 3 patas                       */
typedef char cabe_elem3[ELEM_BYTES + ELEM2_BYTES + ELEM3_BYTES <= 255 ? 1 : -1];   /* 141 */
```
- `version_de`: **lo primero**, `if (algún tipo >= EL_DIODO) return 4;`; luego la regla de la Etapa 3.
- Elemento: longitud = 85 + (elem2 **o** 3 patas ? 54 : 0) + (3 patas ? 2 : 0). Las secciones son acumulativas: un 3 patas escribe también la sección 2 (sus `par`).
- Lectura: tras la sección 2, si quedan ≥ 2 bytes, `xc` e `yc`.
- `elemento_en_rango` comprueba también `xc < REJ_COLS` e `yc < REJ_FILAS`.
- El bloque global no cambia.

### 2.6 `sim.h`: cambios (al final de cada enumerado)
`sim.h` pasa a incluir `lu.h` (usa `LU_MAX` en `SimCc`); `lu.h` no incluye nada del motor, así que no hay ciclo.
```c
#include "lu.h"
enum { SE_R = 0, SE_E, SE_J, SE_C, SE_L, SE_S, SE_AMI, SE_VMI, SE_Z,
       SE_D,       /* diodo, zener, LED: a = ánodo, b = cátodo                          */
       SE_EC,      /* E controlada: b = +; val = μ o r                                  */
       SE_JC,      /* J controlada: de a hacia b; val = g o β                           */
       SE_AO,      /* a = +, b = -, c = salida, nref = referencia de la salida          */
       SE_BJT,     /* a = C, b = E, c = B                                               */
       SE_MOS,     /* a = D, b = S, c = G                                               */
       SE_N };
enum { NL_NO = 0, NL_TRAMOS, NL_UNION, NL_AO_IDEAL, NL_AO_REAL, NL_BJT, NL_MOS };
#define SIM_NM 6

typedef struct {
    /* ... los campos de hoy, sin cambios ... */
    /* Etapa 4. Con memset a 0 los tipos de hoy no cambian: estos campos solo se
     * leen en SE_D, SE_EC, SE_JC, SE_AO, SE_BJT y SE_MOS. */
    short  c;                   /* AO: salida; BJT: B; MOS: G                           */
    short  nref;                /* AO: nodo de la referencia de su salida               */
    signed char ctl;            /* EC, JC: el elemento que la controla                  */
    unsigned char ctl_i;        /* EC, JC: 1 = su corriente; 0 = su tensión             */
    unsigned char modelo;       /* NL_*                                                 */
    signed char pol;            /* BJT, MOS: +1 NPN o canal N; -1 PNP o canal P         */
    double m[SIM_NM];           /* parámetros del modelo, ya resueltos (tabla abajo)   */
} SimEl;
```
Contrato de `m[]` (el adaptador pone los vacíos, el catálogo y las cuentas; el motor solo ve números):

| modelo | m[0] | m[1] | m[2] | m[3] | m[4] | m[5] |
|---|---|---|---|---|---|---|
| NL_TRAMOS (D) | Vγ | Ron (> 0) | Vz0 (0 = sin ruptura) | Rz (> 0) | – | – |
| NL_UNION (D) | Is | n | Rs | BV (0 = sin ruptura) | Ibv | Vcrit |
| NL_AO_IDEAL / NL_AO_REAL | A (ideal: 0) | V+ | V- | Ri (0 = ∞) | Ro | – |
| NL_BJT | Is | βF | βR | VA (0 = ∞) | – | Vcrit |
| NL_MOS | Vt (≥ 0) | K | VA (0 = ∞) | – | – | – |

```c
/* Diagnósticos (al final de SIM_*) */
    SIM_NO_CONVERGE,   /* Newton no converge: elem = el culpable; t = instante (-1 en el OP);
                          campo = NC_*; en NC_BARR, t = el valor del parámetro              */
    SIM_CTL_MAL,       /* una controlada sin control válido: elem = ella; nodo = el que la
                          controla (o -1); campo = CTL_*                                     */
    SIM_NO_LINEAL,     /* Thévenin y la calculadora de CA con un no lineal (elem)            */
    SIM_NADA_BARRER,   /* el barrido: no hay E, J, R ni controlada que barrer                */
    SIM_MODELO,        /* parámetros imposibles del modelo: elem, campo = el hueco (QZ_RZ,
                          QL_IF, QA_VP); no es un dato que falte (SIM_FALTA)                  */
    SIM_AO_REF         /* la salida de un AO sin referencia: elem = el AO; campo = AOR_*     */
enum { NC_OP = 0, NC_TR, NC_BARR, NC_CONMUTA, NC_TOPE };
/* CTL_* (CTL_OK, CTL_FALTA, CTL_SI_MISMA, CTL_3PATAS, CTL_I_NO) viven en circuito.h (§2.3):
 * SIM_CTL_MAL lleva en campo el mismo motivo que circ_ctl_motivo */
enum { AOR_SIN_TIERRA = 0,           /* su subcircuito no tiene tierra ni fuente independiente */
       AOR_CORTO };                  /* la salida es su propia referencia (c == nref)          */

#define SIM_AV_BIESTABLE  0x010000u  /* un AO puede empezar en V+ o en V-: empieza en V+    */
#define SIM_AV_ARRANQUE   0x020000u  /* un AO arranca en su punto inestable (astable)       */
#define SIM_AV_HOMOTOPIA  0x040000u  /* el OP necesitó gmin o la rampa de fuentes           */
#define SIM_AV_PEQ_SENAL  0x080000u  /* CA o Bode de pequeña señal en el punto Q            */
#define SIM_AV_SATURA     0x100000u  /* un AO satura (en el OP, en Q o al final)            */
#define SIM_AV_QUEMA      0x200000u  /* un LED pasa de su Im                                */
#define SIM_AV_CC_BARRIDO 0x400000u  /* el barrido es de CC (C abiertos, L cables)          */
#define SIM_AV_SALTO      0x800000u  /* el barrido salta (histéresis)                       */
#define SIM_AV_HUECOS     0x1000000u /* el barrido tiene huecos (Newton no convergió ahí)   */

/* Regiones (solo para enseñar: tabla, rótulos, esquema) */
enum { REG_NADA = 0, REG_CORTE, REG_CONDUCE, REG_RUPTURA, REG_ACTIVA, REG_SATURACION,
       REG_INVERSA, REG_OHMICA, REG_LINEAL, REG_SAT_MAS, REG_SAT_MENOS, REG_N };

/* Los contadores de Newton. Newton los suma en el SimNewton que le pasa quien llama: el
 * emulador pasa &res_.nw; el punto de operación, el punto Q y el barrido, uno suyo. Así
 * ninguna ruta de OP escribe en res_ (la prueba «Estado intacto» del [58]). */
typedef struct {
    long   iter;                /* iteraciones de Newton                                 */
    int    iter_max;            /* la mayor en un paso (o en el OP)                      */
    long   sub;                 /* subpasos de más (la subdivisión)                      */
    int    eventos;             /* cambios de tramo localizados                          */
    unsigned char homotopia;    /* OP: 0 directo, 1 gmin, 2 rampa de fuentes             */
    int    iter_op;             /* las del OP                                            */
    double residuo_max;         /* el mayor residuo de Kirchhoff relativo de las soluciones */
} SimNewton;
typedef unsigned long long SimTramos;    /* 2 bits por elemento: el tramo de cada D y AO   */

/* SimRes, al final */
    SimNewton nw;                         /* los de la emulación (con su OP)               */
    SimTramos tramos_top[SIM_MAX_TOP];    /* los tramos de cada topología de los modos (§3.K) */

/* ---- Etapa 4: no lineales ---- */
int  sim_hay_nl(const SimRed *s);          /* SE_D, SE_AO, SE_BJT o SE_MOS                    */
/* Punto de operación de CC en la vista de la calculadora ([=] sin dinámica, [P]): C
 * abiertos, L cables, sin V0 ni I0, fuentes en su nivel de CC, S al empezar. Valores con
 * los convenios de la tabla (V2, I2 y reg incluidos). Guarda y repone la última emulación
 * y no toca la caché del transitorio ni res_. nw puede ser NULL. */
int  sim_op_cc_red(const SimRed *s, Valores *v, SimDiag *d, SimNewton *nw);
int  sim_punto_op_cc(const Circuito *c, const Red *r, Valores *v, SimDiag *d, SimNewton *nw);

/* Punto Q y linealización (CA y Bode). Fuentes en su nivel de CC, C y L sin V0 ni I0, S
 * en su estado final. Guarda y repone la última emulación. */
typedef struct {
    unsigned char hay;                    /* 1: hay no lineales linealizados            */
    unsigned char tramo[MAX_EL];          /* D por tramos y AO: el de Q                 */
    double g[MAX_EL][2][2];               /* suaves: di_p/dv_q en Q (puertos de §3.C)   */
    unsigned char reg[MAX_EL];
    Valores q;
} SimLin;
int  sim_punto_q(const SimRed *s, SimLin *L, SimDiag *d, SimNewton *nw);   /* nw: NULL vale */
/* El estampado de las controladas y de los linealizados, el mismo para la LU real y la
 * compleja (ca.c pasa un envoltorio a luc_suma). El mapa es el del que llama: sim.c tiene
 * col_phi y col_br en int y SimTopoCA en signed char, así que se pasa en su propio tipo
 * (ca.c lo rellena una vez tras ca_topologia; sim.c lo arma desde los suyos). */
typedef struct { int phi[MAX_NODOS]; int br[MAX_EL]; } SimMapa;
typedef void (*SimSuma)(void *m, int i, int j, double v);
void sim_estampa_lin(const SimRed *s, const SimLin *L, const SimMapa *mp, SimSuma suma, void *m);

/* Un punto del barrido: el parámetro del elemento k (BX_*) vale v; arranca de io (si
 * io->hay) y lo deja con la solución. Vista de CC de sim_op_cc_red. */
enum { BX_E = 0, BX_J, BX_R, BX_K };      /* valor de una E o J; una R; la ganancia de una controlada */
typedef struct {
    int    hay, n;
    double x[LU_MAX];
    double vlim[MAX_EL][2];
    unsigned char tramo[MAX_EL];
    Valores v;
    int    iter;
    unsigned char salto;                  /* un AO cambió de saturación en este punto   */
} SimCc;
int  sim_cc_punto(const SimRed *s, int k, int bx, double v, SimCc *io, SimDiag *d);
/* dy/dp exacta del canal en la solución de io (una sustitución con la LU del jacobiano) */
int  sim_cc_deriva(const SimRed *s, int k, int bx, const SimCc *io, const SimCanal *ch, double *dy);

double sim_residuo_max(void);              /* el arnés: de la última emulación u OP        */
                                              el de un OP sale en el SimNewton de quien llama */
                                              TODAS las soluciones de Newton (el mono)       */
                                              TODAS las soluciones de Newton (el mono)       */
void   sim_nl_topes(int it_op, int it_tr); /* el arnés: 0 = los de siempre                 */
void   sim_nl_topes(int it_op, int it_tr); /* el arnés: 0 = los de siempre                 */
void   sim_nl_tope_ops(unsigned long ops); /* el arnés: el tope de un OP; 0 = NL_OPS_OP    */
void   sim_nl_prueba(int que, double t);    /* el arnés: ganchos (desde t, o en x = t)      */
void   sim_nl_prueba(int que, double t);    /* el arnés: ganchos (desde t, o en x = t)      */
`nombre_el`: `LETRA` = `"REJCLSAVZDEJUQM"`.
`nombre_el`: `LETRA` = `"REJCLSAVZDEJUQM"`.
**Estado nuevo dentro de sim.c** (no sale en sim.h). Dos familias que no se mezclan:
- **Del transitorio** (solo `un_paso`, `aplica_evento`, `evento_nl`, `localiza`, `elige_paso`): la caché (`lu_cache`, `topo_cache`, `cache_*`, `lu_base`), `lu_suelta` y `M`. Nada de fuera de la emulación en curso los toca.
- **Del punto de operación** (`op_interno` y lo que lo usa: `sim_op_cc_red`, `sim_punto_q`, `sim_cc_punto`, `sim_cc_deriva`, `sim_thevenin_cc`, y el arranque del emulador): `lu_op` (la matriz del modo 0 con los tramos, sin caché: se refactoriza cuando cambian los tramos y se recuerda la clave de la última, `op_clave`), `topo_op`, `x_op` y `lu_nl` (la del Newton con suaves, que es de usar y tirar dentro de una llamada, también en el transitorio).
- **Del punto de operación** (`op_interno` y lo que lo usa: `sim_op_cc_red`, `sim_punto_q`, `sim_cc_punto`, `sim_cc_deriva`, `sim_thevenin_cc`, y el arranque del emulador): `lu_op` (la matriz del modo 0 con los tramos, sin caché: se refactoriza cuando cambian los tramos y se recuerda la clave de la última, `op_clave`), `topo_op`, `x_op` y `lu_nl` (la del Newton con suaves, que es de usar y tirar dentro de una llamada, también en el transitorio).
- El estado de Newton de cada uso (tramos, tensiones limitadas, «frío») va en un `NlEst` que pasa quien llama: el emulador usa `M.nl`; cada ruta de OP, uno suyo. Los contadores, en el `SimNewton` que pasa quien llama.
#define N_CACHE 8                        /* era 4: clave (máscara de S, tramos, hh)          */
#define N_CACHE 8                        /* era 4: clave (máscara de S, tramos, hh)          */
static unsigned  cache_inest[N_CACHE];   /* bit k: el AO k en LIN no vale con esta matriz
                                            (solo se calcula en las entradas de paso h, §3.F) */
                                            (solo se calcula en las entradas de paso h, §3.F) */
static LU        lu_nl;                  /* la del Newton con suaves (de usar y tirar)        */
static LU        lu_nl;                  /* la del Newton con suaves (de usar y tirar)        */
static SimTramos op_clave; static int op_lista;      /* los tramos con que se factorizó lu_op */
static double    lu_base_op[LU_MAX][LU_MAX];         /* con suaves: la del modo 0 sin factorizar */
    double vlim[MAX_EL][2];              /* las tensiones de puerto de la última linealización */
    unsigned char tramo[MAX_EL];         /* D por tramos y AO                                 */
    double vlim[MAX_EL][2];              /* las tensiones de puerto de la última linealización */
    unsigned char frio;                  /* 1: aún no hay linealización (nl_arranque)         */
static struct {                          /* lo fijo de la red no lineal (nl_prepara)          */
static double residuo_acum;              /* sim_residuo_acum: no lo borra ninguna emulación   */
static struct {                          /* lo fijo de la red no lineal (nl_prepara)          */
    int  n; short el[MAX_EL];            /* D, AO, BJT y MOS                                  */
                                            V+ - V-, 1 V); máx(|J|, Vesc/Rmín)                 */
    int  it_op, it_tr;                   /* topes (sim_nl_topes)                              */
                                            V+ - V-, 1 V); máx(|J|, Vesc/Rmín)                 */
    int  it_op, it_tr;                   /* topes (sim_nl_topes)                              */
    int  prueba; double t_prueba;        /* sim_nl_prueba                                     */
} NL;
/* M (la marcha) gana: */
    NlEst    nl;  SimTramos clave;       /* el estado de Newton de la emulación               */
    int      katz;                       /* el próximo medio paso decide los tramos           */
    int      nivel, buenos, pos;         /* subpasos: h/2^nivel; pos en h/1024                */
    double   xa[LU_MAX], ta;             /* el aceptado anterior (predictor)                  */
    int      nh; double ht[3], hx[3][MAX_EL];   /* historia del error local (C y L)           */
    double   imax_nl[MAX_EL];            /* el mayor |i| de cada suave (Δi)                   */
    int      ev_mismo_t; double t_ev_nl; /* Zenón                                             */
    double   pV2[MAX_EL], pI2[MAX_EL];   /* el segundo puerto del último punto registrado     */
```
`Punto` gana `V2[MAX_EL], I2[MAX_EL]`. `guarda_estado`/`repone_estado` guardan también `NL` (sale de `R_`). No hace falta guardar `M`, la caché ni `res_`: ninguna ruta de OP los escribe (la prueba «Estado intacto» del [58] lo vigila parando una emulación a medias).

### 2.7 `nolin.h` (nuevo): los modelos, sin estado
```c
#ifndef NOLIN_H
#define NOLIN_H
#include "sim.h"
#define NL_VT      0.025864925786328750   /* kT/q a 27 °C (300,15 K), CODATA 2018            */
#define NL_GMIN    1e-12                  /* S: fuga de cada unión y del corte por tramos     */
#define NL_RON     1e-3                   /* Ω: conducción por tramos con rd vacía o 0        */
#define NL_XLIM    80.0                   /* e^x sigue en recta por encima                    */
#define NL_OPS_EXP 30                     /* una exp() o un log(): hasta medir BN_EXP         */
#define NL_OPS_OP  1500000UL              /* tope de un OP (y de un punto del barrido): ~0,95 s */
/* Por omisión (vacío): uno por cada hueco con rótulo de vacío en la ficha (§2.2) */
#define NL_VG      0.7
#define NL_IS_D    1e-14
#define NL_N       1.0                    /* QD_N                                             */
#define NL_RS      0.0                    /* QD_RS                                            */
#define NL_VZ      5.1                    /* QZ_VZ                                            */
#define NL_IZ      5e-3
#define NL_IF      20e-3
#define NL_IM      30e-3
#define NL_A       1e5
#define NL_VSAT    15.0
#define NL_BF      100.0
#define NL_IS_Q    1e-15
#define NL_BR      1.0
#define NL_VT_M    2.0
#define NL_K       1e-3

typedef struct { const char *nombre; double Is, n, Rs; } NlDiodo;       /* catálogo */
extern const NlDiodo NL_1N4148, NL_1N4007;                              /* §3.A */
typedef struct { double Vf, n, Rs; unsigned short rgb; } NlColor;
extern const NlColor NL_COLOR[LED_N];

enum { TR_CORTE = 0, TR_CONDUCE, TR_RUPTURA };       /* D por tramos                          */
enum { TR_LIN = 0, TR_SATP, TR_SATN };               /* AO                                    */
double nl_exp(double x, double *d);                  /* e^x con recta por encima de 80; d = derivada */
double nl_pnjlim(double vnew, double vold, double nvt, double vcrit, int *lim);   /* SPICE3 */
double nl_fetlim(double vnew, double vold, double vto);
double nl_limvds(double vnew, double vold);
/* Los límites del MOS como mos1load (§3.D): directo o inverso según el vDS de antes.
 * x = pol·vGS, y = pol·vDS nuevos; xo, yo los de antes. Devuelve 1 si limitó. */
int    nl_mos_lim(double *x, double *y, double xo, double yo, double vt);
double nl_vcrit(double Is, double nvt);              /* nvt·ln(nvt/(√2·Is))                   */
/* Diodo por tramos: i = g·v + i0 en el tramo; margen >= 0 dentro (afín en v) */
void   nl_tr_recta(const SimEl *e, int tramo, double *g, double *i0);
double nl_tr_margen(const SimEl *e, int tramo, double v);
int    nl_tr_de(const SimEl *e, double v);           /* el tramo que contiene v                */
int    nl_tr_vecino(const SimEl *e, int tramo, double v);   /* por dónde sale                 */
/* Unión con Rs condensada: el compañero en vd, y la vd nueva limitada */
void   nl_union_j(const SimEl *e, double vd, double *ij, double *gj);
void   nl_union_comp(const SimEl *e, double vd, double *G, double *Ieq);
double nl_union_vd(const SimEl *e, double vd_ant, double v, double G, double Ieq, int *lim);
/* BJT (NPN o PNP): corrientes que ENTRAN por C y B, y sus derivadas respecto a vBE y vBC */
typedef struct { double ic, ib, dic_be, dic_bc, dib_be, dib_bc; } NlBjt;
void   nl_bjt(const SimEl *e, double vbe, double vbc, NlBjt *o);
/* MOSFET: ID que ENTRA por D, gm = dID/dVGS, gds = dID/dVDS */
typedef struct { double id, gm, gds; } NlMos;
void   nl_mos(const SimEl *e, double vgs, double vds, NlMos *o);
/* Linealización de un suave en dos puertos (§3.C): la unión usa el puerto 0 (a, b); el BJT
 * (B, E) e (C, E); el MOS (G, S) y (D, S). v: tensiones de puerto ya limitadas; i: la
 * corriente que entra por el + de cada puerto; g: di_p/dv_q; vd: la de la unión (con Rs) */
typedef struct { double v[2], i[2], g[2][2], vd; } NlLin;
/* Evalúa el suave en las tensiones de sus puertos, limitándolas respecto a vold (pnjlim,
 * fetlim, limvds); vold pasa a ser las usadas. 1 si alguna se limitó. */
int    nl_evalua(const SimEl *e, const double vpuerto[2], double vold[2], NlLin *L);
void   nl_arranque(const SimEl *e, double vold[2]);  /* uniones en Vcrit; BJT (Vcrit, 0); MOS (Vt, 0) */
/* AO: margen del tramo con u = V+ - V- (ideal: u = Vsal en el lineal) */
double nl_ao_margen(const SimEl *e, int tramo, double vd, double vsal);
int    nl_region(const SimEl *e, const Valores *v, int k);           /* REG_*                  */
double nl_escala_v(const SimEl *e);                  /* Vγ, Vz, V+ - V-: para Vesc             */
unsigned long nl_ops(void); void nl_ops_reset(void); /* se suma a sim_ops()                    */
#endif
```

### 2.8 `resuelve.h`
```c
typedef struct {
    double val[MAX_EL], V[MAX_EL], I[MAX_EL], P[MAX_EL], aux[MAX_EL];
    double phi[MAX_NODOS];
    /* Etapa 4 (0 en la tabla y en los de dos bornes) */
    double V2[MAX_EL], I2[MAX_EL];     /* el 2.º puerto: VBE e IB, VGS, Vd e I(+) del AO */
    unsigned char reg[MAX_EL];         /* REG_* de cada no lineal                        */
} Valores;
enum { ..., SOL_SOLO_CA,
       SOL_NO_LINEAL,                  /* un D, zener, LED, AO, BJT o MOS: la tabla es lineal */
       SOL_CTL,                        /* una fuente controlada sin control                   */
       SOL_SIM };                      /* solo la interfaz: el punto de operación dio un error */
/* Solucion, al final */
    int motivo;                        /* SOL_CTL: CTL_* de circ_ctl_motivo (0 en lo demás)   */
```

| Tipo | V | I | V2 | I2 | P |
|---|---|---|---|---|---|
| D, zener, LED | φa − φb | de a hacia b | 0 | 0 | absorbida V·I |
| E y J controladas | φb − φa | la que sale por + (E: su rama; J: su ley) | 0 | 0 | entregada V·I |
| AO | φc − φnref (Vsal) | la que entrega por c | φa − φb (Vd) | Vd/Ri (0 si ∞) | entregada V·I − V2·I2 |
| BJT | φa − φb (VCE) | IC (entra por C) | φc − φb (VBE) | IB (entra por B) | absorbida V·I + V2·I2 |
| MOSFET | φa − φb (VDS) | ID (entra por D) | φc − φb (VGS) | 0 | absorbida V·I |

En el PNP y el canal P los signos son los mismos (VCE e IC salen negativos en activa; la página Modelo lo dice). *Descartado:* los «signos naturales» del PNP (patas): obligan a otra tabla de signos en cada sitio que lee Valores.

### 2.9 `ca.h`: cambios
```c
/* Pequeña señal: el punto Q de la red SIN apagar (las J de polarización cuentan) y su
 * linealización en L (de quien llama). SIM_OK, o el error del OP. */
int  ca_punto_q(const SimRed *s, SimLin *L, SimDiag *d);
/* La linealización que ensambla_en estampa desde ahora (NULL = ninguna). ca.c no guarda
 * una suya entre llamadas: quien ensambla la pone antes de cada ca_ensambla (como
 * bode.c vuelve a poner la topología), porque otro análisis de CA puede haberla cambiado. */
void ca_pon_q(const SimLin *L);
const SimLin *ca_lin(void);            /* la puesta (o NULL)                                  */
```
`CaRes` gana `unsigned char reg[MAX_EL]` (las regiones en Q, para la tabla). ca.c guarda además el `SimMapa` de su topología (`mapa_`), que rellena `ca_topologia`.

### 2.10 `barrido.h` (nuevo; el motor, por trozos como `bode.h`)
```c
#include "sim.h"
#define BARR_N      281       /* un punto por columna de los 280 px de la gráfica (+1)           */
#define BARR_TRAZAS 6         /* 2 canales de ida + 2 de vuelta, o una familia de 6              */
enum { BARR_CIRCUITO = 0, BARR_CURVA, BARR_FAMILIA };
typedef struct {
    signed char   el;         /* lo que se barre; -1 = automático                                */
    unsigned char log_x;      /* malla logarítmica (por omisión con una R)                       */
    unsigned char vuelta;     /* ida y vuelta                                                    */
    double        x0, x1;     /* 0 y 0 = automático                                              */
} BarrAjustes;
typedef struct {
    int    n, ntr;
    double x[BARR_N];
    double y[BARR_TRAZAS][BARR_N];
    unsigned char ok[BARR_TRAZAS][BARR_N];     /* 0 = hueco                                       */
    unsigned char it[BARR_TRAZAS][BARR_N];     /* iteraciones de Newton                            */
    unsigned char salto[BARR_TRAZAS][BARR_N];  /* 1 = saltó (histéresis) al llegar a este punto   */
} BarrReg;                                     /* ~19 KB                                          */
typedef struct {
    SimDiag  d;               /* SIM_OK con avisos, o el error del primer punto                  */
    int      fin, modo, el, bx;
    double   x0, x1;
    unsigned char log_x, vuelta, xy;           /* xy: la curva I-V (V en x, I en y)               */
    char     nom_x[12], uni_x[4];              /* "E1" y "V"; "R1" y Ω                            */
    SimCanal canal[2];
    char     nom_tr[BARR_TRAZAS][12];          /* "I(D1)"; en la familia, "IB=40µ"                */
    double   nivel_dato; signed char tr_dato;  /* el dato de V o I que lee [N] al acabar; -1 = no */
    int      huecos, it_max;
    double   kcl_peor;
    double   x_fallo;                          /* el primer x sin convergencia                    */
    double   t_est;                            /* segundos estimados en la calculadora            */
    unsigned gen;
} BarrRes;
int  barr_iniciar(const Circuito *c, const Red *r, const BarrAjustes *aj);
int  barr_iniciar_curva(const Circuito *c, int k);        /* el componente k solo (el trazador)  */
int  barr_iniciar_red(const SimRed *s, int el, int bx, const SimCanal ch[2],
                      double x0, double x1, int log_x, int vuelta);          /* el arnés        */
int  barr_avanzar(unsigned long ops);    /* 1 = queda trabajo; solo para entre puntos           */
void barr_cortar(void);
double barr_progreso(void);
const BarrRes *barr_res(void);
const BarrReg *barr_reg(void);
/* Exacto en x (Newton desde el punto de la malla más cercano) y dy/dx exacta. 0 si no
 * converge. Recuerda las 4 últimas lecturas. */
int  barr_en(int tr, double x, double *y, double *dydx);
/* El x de [xa, xb] donde la traza vale nivel: Brent sobre barr_en (1e-14·|x|, 60 como mucho) */
int  barr_nivel(int tr, double nivel, double xa, double xb, double *x);
/* El nivel de un DATO (una magnitud m, §4.8): busca +m y, si no hay cruce, -m. *sg dice cuál */
int  barr_nivel_dato(int tr, double m, double xa, double xb, double *x, int *sg);
/* Todos los valores en x (P_RESULTADO en el cursor) y el circuito con x como dato */
int  barr_valores(int tr, double x, Valores *v, SimDiag *d);
const Circuito *barr_circuito(void);
```
barrido.c guarda además, para `barr_en` y `barr_valores`, **la continuación de cada punto de cada traza** (la ida y la vuelta de un Schmitt están en ramas distintas; en una familia, cada curva tiene su IB o su VGS), así que cambiar de traza con `[C]` no arranca desde la rama equivocada:
```c
typedef struct { double x[LU_MAX]; double vlim[MAX_EL][2]; unsigned char tramo[MAX_EL]; } BarrCont; /* ~1,06 KB */
static BarrCont cont_[BARR_TRAZAS][BARR_N];     /* 6 × 281 × 1,06 KB ≈ 1,8 MB, estático        */
```
Una traza que comparte malla y rama con otra (los dos canales de la misma ida) comparte su fila de `cont_` (se guarda una vez por recorrido: ida, vuelta o curva de la familia; `BarrReg` gana `unsigned char recorrido[BARR_TRAZAS]`).

### 2.11 `curva.h` (nuevo; la pantalla P_BARRIDO, como `osc.h` y `trazador.h`)
```c
#include "barrido.h"
#include "gfx.h"
#define CV_X0 34          /* retícula: x de 34 a 314, 10 divisiones de 28 px          */
#define CV_DX 28
#define CV_Y0 17          /* y de 17 a 161, 8 divisiones de 18 px                     */
#define CV_DY 18
#define CV_YC 89
#define CV_EJE_Y 163      /* rótulos del eje x, en 6×8                                */
#define CV_PANEL_Y 175    /* 4 líneas: 175, 188, 201, 214                            */
enum { CV_NADA = 0, CV_SALIR, CV_BARRER, CV_AJUSTES, CV_CIRCUITO };
typedef struct Curva {
    double x0, x1;                             /* la ventana (en log10 si log_x)          */
    double xc[2];                              /* cursores                                */
    signed char idiv[BARR_TRAZAS], pos[BARR_TRAZAS];
    unsigned char ver[BARR_TRAZAS], log_y[BARR_TRAZAS];
    unsigned char activo, dos, cur, xy, nivel_on;
    double nivel;
    char entrada[20]; unsigned char de_nivel;  /* escribiendo un x (0) o un nivel (1)     */
    char aviso[65]; u16 aviso_col;
} Curva;
void   cv_nuevo(Curva *z, const BarrReg *g, const BarrRes *r);
void   cv_dibuja(Curva *z, const BarrReg *g, const BarrRes *r);
int    cv_tecla(Curva *z, const BarrReg *g, const BarrRes *r, int t);   /* con T_CON_SHIFT */
int    cv_x(const Curva *z, double x);
int    cv_y(const Curva *z, int tr, double y);
double cv_valor(const BarrReg *g, int tr, double x);  /* interpolado; NaN en un hueco    */
void   cv_autoescala(Curva *z, const BarrReg *g);
int    cv_cruces(const BarrReg *g, int tr, double nivel, double *xs, int max);
```

### 2.12 `esquema.h`, `gfx.h`
```c
/* Vista, al final (esq_vista los deja a NULL y -1) */
    const unsigned char *brillo;       /* LED: nivel 0..9 por elemento; NULL = apagados     */
    int marca_x, marca_y;              /* el punto que estorba al colocar (cuadro rojo)     */
int  esq_led_nivel(double I, double If, double Im);   /* D21                                */
u16  esq_mezcla(u16 fondo, u16 col, int t256);        /* (f·(256 - t) + c·t)/256 por canal  */
void esq_flechas_cursor_m(int sx, int sy, unsigned mascara, u16 col, u16 col_no);
enum { ..., ICO_IMP, ICO_VATI, ICO_DIODO, ICO_ZENER, ICO_LED, ICO_EDEP, ICO_JDEP,
       ICO_AO, ICO_BJT, ICO_MOS };
/* gfx.h */
#define C_DIODO RGB(240,120,110)
#define C_AO    RGB(130,220,170)
#define C_TRANS RGB(230,200,120)
```
El color del LED sale de `NL_COLOR[variante].rgb`: rojo (255,50,40), naranja (255,140,30), amarillo (255,230,40), verde (60,235,80), azul (60,120,255), blanco (245,245,255).

### 2.13 `ui.h`, `biblioteca.h`, `banco.h`
```c
enum { ..., P_BODE, P_THEVENIN, P_BARRIDO, P_N };       /* 29 */
const struct Curva *ui_curva(void);
int ui_res_nl(void);                                    /* P_RESULTADO viene del punto de operación */

/* CircModelo, al final (0 en los 22 de hoy) */
    const char   *barre;           /* ca = 4: la etiqueta de lo que se barre ("E1", "R1")          */
    double        x0, x1;          /* su rango                                                      */
    unsigned char log_x;
/* ca: 0 tiempo; 1 CA; 2 Bode; 3 punto de operación de Newton; 4 barrido de CC.
 * Medidas nuevas. Tiempo: 'U' máximo y 'u' mínimo del ÚLTIMO periodo. Punto de
 * operación: 'I' |I| de clave_el, 'V' su V con signo, 'L' la V de un voltímetro
 * clave_el. Barrido: 'x' el x donde el canal 1 vale t; 'y' el canal 1 en x = t;
 * 'g' dy/dx del canal 1 en x = t. */

enum { ..., BN_SENO, BN_EXP /* una exp() */, BN_NL /* un paso del puente exponencial */, BN_N };

/* ondas.h: el nivel de CC de una fuente (punto de operación de [P] y punto Q): continua p0;
 * senoidal p3; cuadrada p3 + p0·(p2/50 - 1); triangular p3; pulso p0 */
double onda_nivel_cc(int forma, const double *p);
```

---

## 3. Algoritmos

### A. Adaptador, catálogo y validación (`el_de` y `valida` en sim.c)

`sim_red_de_ex` pone además `x->c = r->c[k]` (−1 en los de dos bornes) y, en el AO, `x->nref = red_ref_de(r, r->c[k])`. **La referencia del AO tiene que existir:** si el subcircuito de su salida (`r->comp[c]`) no tiene la tierra ni el − de una fuente independiente, `red_cierra` habría tomado «su primer nodo» (que puede ser la salida misma, una entrada o el nodo de un C) y la física saldría falsa: `SIM_AO_REF` con `elem = k` y `campo = AOR_SIN_TIERRA`. Si `c == nref` (la salida atada a la tierra o al − de la primera fuente), la columna de su rama quedaría vacía: `SIM_AO_REF` con `AOR_CORTO`. `red.c` da para eso `int red_ref_propia(const Circuito *c, const Red *r, int n)`: 1 si la referencia del subcircuito de n es la tierra o el − de una fuente independiente (no «su primer nodo»).

`el_de` lee los huecos con `par_o(e, q, omisión)` (dato → su valor; vacío → la omisión de `nolin.h`) y **devuelve 1, 0 o −1**: 0 si falta un dato (`SIM_FALTA` con `*q`, como hoy; `*q == Q_CTRL` en una controlada sin control → `SIM_CTL_MAL`), −1 si los parámetros son imposibles (`SIM_MODELO` con `*q` = el hueco culpable). Antes de mirar la variante comprueba `variante < tipo_info(tipo)->nvar` (si no, `SIM_MODELO` con `*q = Q_VAL`: un archivo roto no indexa `NOM_*` ni `NL_COLOR` fuera de su tabla).

| Tipo y variante | `tipo`, `modelo` | `m[]` |
|---|---|---|
| DIODO ideal | SE_D, NL_TRAMOS | {0, NL_RON, 0, NL_RON} |
| DIODO con Vγ | SE_D, NL_TRAMOS | {Vγ, rd > 0 ? rd : NL_RON, 0, NL_RON} |
| DIODO exponencial | SE_D, NL_UNION | {Is, n, Rs, 0, 0, Vcrit} |
| DIODO 1N4148 / 1N4007 | SE_D, NL_UNION | los de `NL_1N4148` / `NL_1N4007` (abajo), sin mirar los huecos |
| ZENER de libro | SE_D, NL_TRAMOS | {Vγ, NL_RON, Vz0 = Vz − Rz·Iz, Rz = rz > 0 ? rz : NL_RON}; si Vz0 ≤ 0 (rz·Iz ≥ Vz), −1 con `*q = QZ_RZ` |
| ZENER exponencial | SE_D, NL_UNION | {1e-14, 1, Rs = máx(rz − VT/Iz, 0), BV = Vz − Rs·Iz, Ibv = Iz, Vcrit}; si BV ≤ 0 (BV = 0 es la marca de «sin ruptura»: el zener la perdería sin avisar; pasa con rz·Iz ≥ Vz + VT, p. ej. rz ≥ 1025 Ω con 5,1 V y 5 mA), −1 con `*q = QZ_RZ` |
| ZENER 1N4733A | SE_D, NL_UNION | lo mismo con Vz = 5,1 V, Iz = 49 mA, rz = 7 Ω: Rs = 6,4721443717075765 Ω, BV = 4,7828649257863288 V |
| LED (color c) | SE_D, NL_UNION | {Is*, n_c, Rs_c, 0, 0, Vcrit} con Vf = QL_VF o la del color, If = QL_IF o 20 mA: ij* = If − GMIN·Vf; vd* = Vf − Rs_c·ij*; Is* = ij*/(e^(vd*/(n_c·VT)) − 1). **Si vd* ≤ 0** (Rs·If ≥ Vf: el azul con If > 0,213 A, el rojo con If > 0,36 A; el campo admite hasta 10 A), Is* saldría ≤ 0 o infinita: −1 con `*q = QL_IF` |
| EDEP, JDEP | SE_EC, SE_JC | `val` = la ganancia (dato; si no, `SIM_FALTA` con Q_VAL; con `tolera`, de la calculadora de CA, 0 provisional: calca.c ya paró antes, §3.I); `ctl = e->ref` (−1: devuelve 0 con `*q = Q_CTRL`); `ctl_i = variante == DEP_I` |
| AO ideal | SE_AO, NL_AO_IDEAL | {0, V+, V−, 0, 0}; si V+ ≤ V−, −1 con `*q = QA_VP` |
| AO real | SE_AO, NL_AO_REAL | {A, V+, V−, Ri (vacía: 0 = ∞), Ro (vacía: 0)}; lo mismo con V+ ≤ V− |
| BJT | SE_BJT, NL_BJT; `pol` = NPN ? +1 : −1 | {Is, βF, βR, VA (vacía: 0 = ∞), –, Vcrit = VT·ln(VT/(√2·Is))} |
| MOS | SE_MOS, NL_MOS; `pol` = canal N ? +1 : −1 | {Vt, K, VA (vacía: 0 = ∞)} |

- **Catálogo** (`nolin.c`; los modelos de LTspice `standard.dio`, reducidos a Is, n y Rs): `NL_1N4148` = {"1N4148", 2,52e-9 A, 1,752, 0,568 Ω}; `NL_1N4007` = {"1N4007", 7,02767e-9 A, 1,80803, 0,0341512 Ω}. Vcrit = n·VT·ln(n·VT/(√2·Is)).
- `sim_valida` y `sim_red_de_ex`: si `el_de` falla con `*q == Q_CTRL`, el estado es `SIM_CTL_MAL` con `campo = CTL_FALTA`, `elem = k`, `nodo = −1` (no `SIM_FALTA`); si devuelve −1, `SIM_MODELO` con `elem = k` y `campo = *q` (su texto dice qué no cuadra, §3.M; no «falta rz en D2 (un dato)»).
- **`valida(d, ca)`** (sobre `R_`, para las redes armadas a mano del arnés), además de lo de hoy:
  - SE_D por tramos: Ron > 0, Rz > 0; con ruptura, Vz0 > 0 (si no, `SIM_MODELO` con `campo = QZ_RZ`). SE_D unión: Is > 0, n > 0, Rs ≥ 0, BV ≥ 0.
  - SE_AO: V+ > V− (si no, `SIM_MODELO` con `campo = QA_VP`); real: A > 0, Ri ≥ 0, Ro ≥ 0. `c` y `nref` en rango; `c ≠ nref` (si no, `SIM_AO_REF` con `AOR_CORTO`).
  - SE_BJT: Is, βF, βR > 0; VA ≥ 0. SE_MOS: Vt ≥ 0, K > 0, VA ≥ 0. `c` en rango.
  - SE_EC, SE_JC (`SIM_CTL_MAL`, `nodo = ctl`, `campo` = el motivo con los valores de `circ_ctl_motivo`): `ctl` fuera de [0, nel) → `CTL_FALTA`; `ctl == k` → `CTL_SI_MISMA`; el que controla es de 3 patas → `CTL_3PATAS`; con `ctl_i`, el que controla es SE_D, SE_Z, SE_JC, SE_AO, SE_BJT o SE_MOS → `CTL_I_NO`. Es la lista de `circ_ctl_motivo` (§2.3) traducida a la SimRed: el arnés comprueba en [57] que las dos dan lo mismo en 2000 circuitos al azar.
- **La tensión y la corriente de control** (las de la tabla del que controla, D8), como combinación de incógnitas:
  - `V_ctl = x[cp] − x[cq]` con (p, q) = (a, b) en pasivos, SE_D e instrumentos; (b, a) en SE_E, SE_J, SE_EC y SE_JC; (b, referencia) en el vatímetro.
  - `I_ctl = Σ c_i·x_i + c0(t)`: rama (E ideal, AMI, S, C, L, EC): `x[col_br]`; SE_R (también un instrumento con R): (φa − φb)/R; E real: (φa − φb)/r + E(t)/r; J: c0 = J(t); VMI: 0.
  - `static int ctl_coef(int k, int col[3], double c[3], double *c0, double t, int izq)`: hasta 3 términos y la parte conocida. La usan `ensambla` (los c_i), `termino` (c0), `valores_de` y `sim_estampa_lin`.

### B. Mapa, puertos y análisis estructural
- **`con_rama(e)`**: SE_EC → 1; SE_AO → 1 si es ideal o si Ro = 0. El resto como hoy. `mapa()` no cambia: las ramas nuevas se numeran en el mismo bucle (por elemento), así que un circuito sin tipos nuevos conserva su mapa bit a bit.
- **Puertos.** Un elemento se ve en el análisis estructural como uno o dos puertos (par de nodos y papel). Función nueva `static int puertos(int k, int modo, unsigned mask, short pa[2], short pb[2], unsigned char rol[2])`:

| Tipo | Puerto 0 | Puerto 1 |
|---|---|---|
| los de hoy | (a, b, `rol_de`) | – |
| SE_D | (a, b, G) | – |
| SE_EC | (a, b, V) | – |
| SE_JC | (a, b, I) | – |
| SE_AO ideal | (nref, c, V): la salida | (a, b, NADA): las entradas |
| SE_AO real | (nref, c, Ro = 0 ? V : G) | (a, b, Ri > 0 ? G : NADA) |
| SE_BJT | (c, b, G): B–E | (c, a, G): B–C |
| SE_MOS | (a, b, G): D–S (la puerta no conduce: no es un puerto) | – |

- **`analiza`** recorre puertos en vez de elementos (hoy, cada elemento es su puerto 0). `tp->rol[k]` sigue siendo el papel del puerto 0 (lo que leen `ensambla` y `termino`).
  - Lazos de ramas V (**primer recorrido**, el de hoy con puertos): `ORDEN[7] = { SE_E, SE_EC, SE_AO, SE_AMI, SE_S, SE_L, SE_C }`. Son las ramas **de corriente libre** (su corriente la fija el resto del circuito): un lazo de ellas deja una corriente que circula sin ley, y la LU sale singular aunque las tensiones cuadren. La tensión de un puerto V de SE_EC o SE_AO no es conocida antes de resolver, así que un lazo que contiene uno (cerrándolo o en el camino):
    - **lo cierra un C con V0 efectiva (modo 0)** → el rescate CI_SALTA de hoy (`ci_ef = 0`, aviso, se repite): el C va abierto en el OP y arranca en su V0 en 0⁺ (el AO o la EC lo cargarían con un impulso). Si por casualidad cuadraba, el resultado es el mismo (el OP da al C su V0) y solo sobra el aviso;
    - **lo cierra una L en corto (modo 0, sin I0)** → el rescate L_SIN_I de hoy (la L empieza sin corriente);
    - si no → **`SIM_LAZO_V`** sin mirar si cuadra (la salida de un AO atada a una E ideal, dos AO con las salidas juntas, un A de la salida a tierra).
    El C va el último en `ORDEN`, así que es él quien cierra el lazo cuando lo hay; la L va antes que el C y después de E, EC, AO, AMI y S.
  - **Segundo recorrido, solo en el modo 0 y solo si hay un AO ideal y algún C con V0 efectiva** (los lazos de **tensiones** que cierra el nulador; el primero no los ve porque el nulador no lleva corriente): union-find con las ramas que **imponen tensión**: E ideal, EC, la salida de un AO real con Ro = 0, AMI, S cerrado, L en corto y **el nulador de cada AO ideal** (a con b: el OP arranca con los AO en su lineal), y al final los C con V0 efectiva. **La salida de un AO ideal (el norador) no entra**: su tensión es libre (si entrara, el integrador, con el C de la salida a la masa virtual, formaría un lazo con el norador y el nulador y perdería su V0 sin motivo). Un C con V0 que cierra aquí un lazo **que contiene un nulador**:
    - si en el camino hay una EC o la salida de un AO real (tensión desconocida) → CI_SALTA;
    - si no, se comprueba con las tensiones conocidas (E, 0 en AMI, S, L y el nulador, V0 de los otros C) como hoy: cuadra → `SU_I0` (el C no lleva corriente en el OP: sin eso las filas de E, C y nulador serían dependientes); no cuadra → CI_SALTA.
    Es el derivador: la triangular vale −1 V en 0⁻, el C (V0 = 0) va de la fuente a la masa virtual y el nulador fija esta a 0 V; sin este recorrido las tres filas (E, C y nulador) solo tienen φsrc y φ⁻, la LU sale singular y la regla del §F culparía al AO y lo saturaría en el OP. Con él: CI_SALTA, el C abierto en el OP, Vsal = 0 y el AO en su lineal.
  - Trozos: nodos unidos por puertos G y V. Cortes de J: los puertos I y NADA que cruzan (como hoy). Una SE_JC en un corte cuenta como una J de valor desconocido: sin C abiertos en el corte, `SIM_CORTE_J` con ella en `elem`; con C abiertos (en el OP), el rescate C_DESC como si su corriente no fuera 0.
  - Los rescates (C_DESC, L_SIN_I, CI_SALTA, CARGA, FLUJO, ANCLA) no cambian: los no lineales son G, V o NADA, **sea cual sea su tramo**. Por eso el diodo por tramos es siempre una conductancia (D3): el análisis estructural no depende del estado de los diodos.
  - Una puerta de MOS o una entrada de AO ideal que no toca nada más queda en un trozo que flota: la regla de hoy la ancla con el aviso FLOTA.
- **`rol_de`** gana los `case` del puerto 0 (D: G; EC: V; JC: I; AO: V o G; BJT, MOS: G).
- **`algebraica(k)`** (la historia BDF2 tras un evento): para un C une además los nodos de las salidas de SE_EC y de SE_AO con Ro = 0 (nref con c), **en el tramo lineal de un AO ideal, sus entradas a con b** (el nulador: la masa virtual) y **un SE_D por tramos en CONDUCE o en RUPTURA cuya R de tramo cumple R_τ·C_k < h** (Ron o Rz; C_k es el C que se mira). Así el C del derivador, entre una E ideal y la masa virtual, es algebraico, y también el C del rectificador mientras el diodo conduce: su modo Ron·C = 0,1 µs es 1700 veces más rápido que h, el trapecio no lo amortigua y, con la i_C del Euler en te + h (que no cuadra con la del trapecio), la corriente del diodo queda zumbando ±δ. Como el margen del diodo en conducción es Ron·i, ese zumbido corre el apagado: en la réplica (`e4juez2/r1_rect.py`, T/100) el apagado sale a 91,604° en vez de 91,413° (media onda), 91,620° en vez de 91,413° (onda completa) y 91,141° en vez de 91,342° (puente); con la historia BDF2, a 91,415°, 91,416° y 91,347° (el resto es el efecto de Ron frente a la cerrada con Ron = 0). Con R_τ·C ≥ h (un rd de 10 Ω con 100 µF: τ = 1 ms) el modo es lento, el trapecio lo sigue y el C se queda con la i_C de la solución, como hoy. En una L no cambia nada: un diodo en corte (1 pS) solo deja pasar pA.

### C. Estampados
Notación: `ca = cphi(a)` y lo mismo con b, c, nref, p, q; `j = col_br[k]`; `g(a, b, G)` = `estampa_g`; `iny(a, b, I)` = `inyecta` (I de a hacia b por dentro: `rhs[ca] −= I; rhs[cb] += I`). Las filas de KCL suman lo que sale del nodo. `ensambla(f, tp, modo, hh, tr)` gana `const unsigned char *tr` (el tramo de cada elemento por tramos; NULL = sin no lineales) y `termino(rhs, tp, metodo, t, izq, hh, tr, lam)` gana el tramo y la rampa de fuentes λ (E y J por λ; λ = 1 fuera de la rampa, con `if (lam != 1.0)` para no tocar el camino de hoy).

**La ley de nodos de las ramas.** Hoy, antes del `switch`, `ensambla` hace `if (j >= 0) { suma(ca, j, 1); suma(cb, j, −1); }` con `e->a` y `e->b` (y `ensambla_en` de ca.c, lo mismo). Esa línea pasa a usar **los nodos del puerto 0** (`puertos()`): (a, b) en todos los tipos de dos bornes, **(nref, c) en el AO**. Así la corriente de la rama del AO (la que entrega por c) entra en las filas de nref y c, no en las de + y −. En los tipos de hoy el puerto 0 es (a, b): el ensamblado sale bit a bit igual. **Las filas de la tabla de abajo no repiten la ley de nodos** de las ramas (EC, AO ideal, AO real con Ro = 0): la pone esa línea común, una sola vez.

| Elemento | Matriz (sin la ley de nodos de su rama: la pone la línea común) | Término independiente |
|---|---|---|
| **E controlada por V** (VCVS, rama j) | A[j][cb] += 1; A[j][ca] −= 1; A[j][cp] −= μ; A[j][cq] += μ | rhs[j] = 0 |
| **E controlada por I** (CCVS) | A[j][cb] += 1; A[j][ca] −= 1; A[j][col_i] −= r·c_i | rhs[j] = r·c0(t) |
| **J controlada por V** (VCCS), de a hacia b | A[ca][cp] += g; A[ca][cq] −= g; A[cb][cp] −= g; A[cb][cq] += g | – |
| **J controlada por I** (CCCS) | A[ca][col_i] += β·c_i; A[cb][col_i] −= β·c_i | iny(a, b, β·c0(t)) |
| **D por tramos**, tramo τ | g(a, b, G_τ) | iny(a, b, I0_τ) |
| **AO ideal** (rama j: la corriente que entrega por c; la línea común pone A[cnref][j] += 1; A[cc][j] −= 1) | LIN: A[j][ca] += 1; A[j][cb] −= 1. SAT±: A[j][cc] += 1; A[j][cnref] −= 1 | LIN: 0. SAT+: rhs[j] = V+; SAT−: V− |
| **AO real, Ro = 0** (rama j) | SAT como el ideal. LIN: A[j][cc] += 1; A[j][cnref] −= 1; A[j][ca] −= A; A[j][cb] += A | LIN: 0 |
| **AO real, Ro > 0** (Norton, sin rama) | g(c, nref, 1/Ro). LIN: A[cc][ca] −= A/Ro; A[cc][cb] += A/Ro; A[cnref][ca] += A/Ro; A[cnref][cb] −= A/Ro | SAT±: iny(nref, c, V±/Ro) |
| **Ri** del AO real | g(a, b, 1/Ri) | – |
| **Suave de dos puertos** (`NlLin L`, puertos (p⁺, p⁻)) | ∀ p, q: A[p⁺][q⁺] += g_pq; A[p⁺][q⁻] −= g_pq; A[p⁻][q⁺] −= g_pq; A[p⁻][q⁻] += g_pq | ieq_p = i_p − Σ_q g_pq·v_q: rhs[p⁺] −= ieq_p; rhs[p⁻] += ieq_p |

- **Tramos del diodo** (i = G_τ·v + I0_τ, v = φa − φb; márgenes afines en v, ≥ 0 dentro):

| Tramo | G_τ | I0_τ | Margen |
|---|---|---|---|
| CORTE | GMIN | 0 | mín(Vγ − v, v + Vz0) (sin ruptura: Vγ − v) |
| CONDUCE | 1/Ron | GMIN·Vγ − Vγ/Ron | v − Vγ |
| RUPTURA | 1/Rz | Vz0/Rz − GMIN·Vz0 | −Vz0 − v |

  La curva es continua (en v = Vγ los dos tramos dan GMIN·Vγ; en v = −Vz0, −GMIN·Vz0) y creciente: el paso de Katzenelson converge en un número finito de cambios. `nl_tr_vecino`: de CORTE a CONDUCE si v > Vγ, a RUPTURA si v < −Vz0; de los otros dos, a CORTE.
- **Tramos del AO** (Vd = φa − φb; Vsal = φc − φnref; u = A·Vd en el real):

| Tramo | Margen ideal | Margen real | Sale a |
|---|---|---|---|
| LIN | mín(V+ − Vsal, Vsal − V−) | mín(V+ − u, u − V−) | SAT+ por arriba, SAT− por abajo |
| SAT+ | Vd | u − V+ | LIN (si LIN no vale: SAT−; §F) |
| SAT− | −Vd | V− − u | LIN (si LIN no vale: SAT+) |

  En la frontera los dos márgenes valen 0 (continuidad): Vsal = V+ con Vd = 0 en el ideal, u = V+ en el real.
- **Puertos de los suaves** (el mismo estampado para los tres):
  - Unión (D exponencial, catálogo, zener exponencial, LED): puerto (a, b); G = gj/(1 + Rs·gj) + GMIN, Ieq = (ij − gj·vd)/(1 + Rs·gj), con ij, gj en la vd de la iteración (D4).
  - BJT: P0 = (B, E) con i = IB; P1 = (C, E) con i = IC (las dos entran por su pata). Con las derivadas de `nl_bjt` respecto a vBE y vBC (vBC = vBE − vCE): g00 = dib_be + dib_bc; g01 = −dib_bc; g10 = dic_be + dic_bc; g11 = −dic_bc.
  - MOS: P0 = (G, S) con i = 0 (g00 = g01 = 0); P1 = (D, S) con i = ID: g10 = gm, g11 = gds.
- **Modelos** (`nolin.c`; VT = NL_VT; `nl_exp` sigue en recta por encima de 80; x < −40 no llama a `exp`):
  - Unión: ij(vd) = Is·(e^(vd/(n·VT)) − 1) − Ibv·e^(−(vd + BV)/VT) (el segundo término, solo con BV > 0); gj = ij′.
  - BJT (x = pol·vBE, y = pol·vBC): iF = Is·(e^(x/VT) − 1), iR = Is·(e^(y/VT) − 1); q = 1 − y/VA (sin VA, 1; con tope q ≥ 0,05 y dq = 0 en el tope); ic′ = (iF − iR)·q − iR/βR − GMIN·y; ib′ = iF/βF + iR/βR + GMIN·(x + y); IC = pol·ic′, IB = pol·ib′. Derivadas: ∂ic/∂vBE = gF·q; ∂ic/∂vBC = −gR·q − (iF − iR)/VA − gR/βR − GMIN; ∂ib/∂vBE = gF/βF + GMIN; ∂ib/∂vBC = gR/βR + GMIN (gF = Is·e^(x/VT)/VT; iguales en NPN y PNP porque pol² = 1).
  - MOS (x = pol·vGS, y = pol·vDS, λ = 1/VA): si y < 0, D y S cambian: x ← x − y, y ← −y. vov = x − Vt. Corte (vov ≤ 0): id = 0. Óhmica (y < vov): id = K·(2·vov·y − y²)·(1 + λy); gm = 2K·y·(1 + λy); gds = 2K·(vov − y)·(1 + λy) + λK·(2·vov·y − y²). Saturación: id = K·vov²·(1 + λy); gm = 2K·vov·(1 + λy); gds = λK·vov². Más GMIN·y. Si se cambiaron: id ← −id, gds ← gm + gds, gm ← −gm. ID = pol·id.
- **En la CA** (`sim_estampa_lin`, coeficientes reales en la LU compleja, con el `SimMapa` de ca.c; la ley de nodos de las ramas la pone la línea común de `ensambla_en`, con (nref, c) en el AO y (a, b) en los demás, como en sim.c): las mismas filas sin el término independiente; el D por tramos con la G de su tramo en Q; la unión con su G en Q; el AO con su tramo en Q (saturado: salida fija, que en pequeña señal es 0); BJT y MOS con sus g en Q. El c0 de una controlada por la I de una E real o de una J activas va a `ca_b_fuentes` (c0 = U/r o U, con U el fasor de esa fuente).

### D. El Newton (un solo núcleo para el punto de operación, el paso y el barrido)
```c
/* modo: NL_OP, NL_TR o NL_BARR. katz: 1 = los tramos pueden cambiar (paso de
 * Katzenelson y regla del AO); 0 = congelados. x entra (el arranque) y sale.
 * lam: rampa de fuentes (1 = sin rampa); gd: gmin en la diagonal de cada nodo.
 * st: el estado de Newton de quien llama (tramos, vlim, frío); cnt: sus contadores.
 * Devuelve NLR_OK, NLR_NOCONV, NLR_CICLA o NLR_SINGULAR. */
static int newton(int modo, int katz, int metodo, double t, int izq, double hh,
                  double lam, double gd, double *x, int itmax, NlEst *st, SimNewton *cnt);
```
**De dónde sale la matriz** (el paso 1 de abajo):
- **Modo NL_OP y NL_BARR** (el punto de operación, el punto Q, el barrido): la del **modo 0** con `topo_op` (C abiertos o fuentes de su V0, L cortos o de su I0, los rescates ya asentados por `op_interno`) y los tramos de `st`: `ensambla(&lu_op, &topo_op, 0, 1.0, st->tramo)` y `lu_factoriza`, **solo si los tramos cambiaron** desde la última (`op_clave`, `op_lista`). Sin la caché del transitorio: su clave tiene hh, y un paso real de 1 s daría la misma clave que el «hh = 1» del OP; además, `prepara_lu` hace `analiza` en el modo 1 (el C como 2C/hh) y escribe `res_.d`. Con suaves, la base es esa misma matriz del modo 0 sin factorizar (`lu_base_op`, una sola), que se copia a `lu_nl` en cada iteración.
- **Modo NL_TR** (un paso): la de la caché del transitorio (mask, tramos, hh), o `lu_suelta` en un paso partido, como hoy.
```
flips[] = 0; pulido = 0
evalúa los suaves en x (st->frio: la 1.ª vez, nl_arranque: uniones en Vcrit, BJT (Vcrit, 0), MOS (Vt, 0))
para it = 1 .. itmax:
  1. matriz: sin suaves y gd = 0 -> f = la de los tramos de st (arriba): 0 factorizaciones si no cambiaron
             si no -> copia la base (lo lineal y los tramos, sin factorizar) a lu_nl,
                      estampa los suaves (NlLin) y gd en la diagonal de los nodos, lu_factoriza: 1 LU
             singular -> un AO en LIN es el culpable (§F); si no, NLR_SINGULAR (col_mala)
  2. rhs = termino(…, tramos, lam) + ieq de los suaves;  xn = f⁻¹·rhs
  3. si katz:
       a. un AO en LIN con la entrada «inestable» (§F): kq = ese, th = 0
       b. si no, para cada elemento por tramos: m1 = margen(tramo, xn); si m1 < −1e-9·Vesc:
             m0 = margen(tramo, x) (≥ −1e-9·Vesc); tq = m0 > 0 ? m0/(m0 − m1) : 0; el menor tq -> kq, th
       c. si kq ≥ 0: x += th·(xn − x); st->tramo[kq] = sale(kq, x) (§C, §F);
                     si ++flips[kq] > 8 -> NLR_CICLA; reevalúa los suaves en x; sigue (no converge aquí)
  4. evalúa los suaves en xn con límite (pnjlim; en el MOS, nl_mos_lim): lim = alguno limitó
     (esa evaluación es también la linealización de la iteración siguiente: una exp por unión e iteración)
  5. convergencia:
       sin suaves: ok = 1 (la sustitución es exacta en sus tramos)
       con suaves: ok = !lim y ( (incrementos(x, xn) y leyes(xn)) o residuo(xn) ≤ 1e-12 )
     x = xn; cnt->iter++
     si ok: si modo ≠ NL_TR, hay suaves y !pulido: pulido = 1; sigue   (una iteración más)
            r = residuo(x); cnt->residuo_max = máx(cnt->residuo_max, r);
            residuo_acum = máx(residuo_acum, r); NLR_OK
culpable = el suave de mayor |Δv|/(1e-6·|v| + 1e-9·Vesc) en la última iteración, o el último que se limitó
NLR_NOCONV
```
- **incrementos**: |xn_i − x_i| ≤ 1e-6·máx(|xn_i|, |x_i|) + tol_i, con tol = 1e-9·Vesc en los potenciales y 1e-15 A + 1e-9·Iesc en las ramas.
- **leyes**: en cada puerto de un suave, |i_p(v) − i_lin,p(v)| ≤ 1e-6·máx(|i_p|, |i_lin,p|) + 1e-15 A + 1e-9·Iesc (la corriente del modelo en la v nueva frente a la del compañero con el que se resolvió).
- **residuo** (el de Kirchhoff, O(nel)): en cada nodo, la suma de las corrientes que salen por cada elemento con su ley de verdad (los suaves con su evaluación en xn; en una unión con Rs, ij(vd*) + GMIN·v con vd* = v − Rs·(i_lin − GMIN·v)), dividida por la suma de sus módulos (+1e-300); y en cada fila de rama, el error de su ecuación relativo a sus términos. Devuelve el mayor. *Por qué este criterio además del de incrementos:* con los cuatro diodos del puente en corte, el nodo común cuelga de GMIN y su redondeo (eps·I/GMIN ≈ 1e-6 V) no pasa ningún criterio de incremento; el residuo ya está en 1e-18 A.
- **Límites** (SPICE3f5, sin tocar):
```c
double nl_pnjlim(double vn, double vo, double nvt, double vcrit, int *lim)
{   *lim = 0;
    if (vn > vcrit && fabs(vn - vo) > 2.0 * nvt) {
        *lim = 1;
        if (vo > 0.0) { double arg = 1.0 + (vn - vo) / nvt; return arg > 0.0 ? vo + nvt * log(arg) : vcrit; }
        return nvt * log(vn / nvt);
    }
    return vn;
}
/* Ruptura: si vd < -BV + 10·VT se limita la tensión reflejada -(vd + BV) con VT y la misma vcrit */
double nl_fetlim(double vn, double vo, double vto)
{   double hi = fabs(2.0 * (vo - vto)) + 2.0, lo = hi / 2.0 + 2.0, vtox = vto + 3.5, dv = vn - vo;
    if (vo >= vto) {
        if (vo >= vtox) { if (dv <= 0.0) { if (vn >= vtox) { if (-dv > lo) vn = vo - lo; } else if (vn < vto + 2.0) vn = vto + 2.0; }
                          else if (dv >= hi) vn = vo + hi; }
        else { if (dv <= 0.0) { if (vn < vto - 0.5) vn = vto - 0.5; } else if (vn > vto + 4.0) vn = vto + 4.0; }
    } else {
        if (dv <= 0.0) { if (-dv > hi) vn = vo - hi; }
        else { double vt = vto + 0.5; if (vn <= vt) { if (dv > lo) vn = vo + lo; } else vn = vt; }
    }
    return vn;
}
double nl_limvds(double vn, double vo)
{   if (vo >= 3.5) { if (vn > vo) { if (vn > 3.0 * vo + 2.0) vn = 3.0 * vo + 2.0; } else if (vn < 3.5 && vn < 2.0) vn = 2.0; }
    else { if (vn > vo) { if (vn > 4.0) vn = 4.0; } else if (vn < -0.5) vn = -0.5; }
    return vn;
}
```
  La unión limita su vd (la del modelo lineal, `nl_union_vd`), el BJT limita vBE y vBC (con `pol`). **El MOS limita como `mos1load` de SPICE3, con su modo inverso** (sin él, con vDS < −0,5 V `limvds` recorta a −0,5 en cada iteración, `lim` sigue en 1 y Newton no converge nunca, ni con gmin ni con la rampa: un MOSFET dibujado con D y S al revés daría `NO_CONVERGE`, contra el «simétrico» de D10):
```c
/* x = pol·vGS, y = pol·vDS nuevos (de la solución lineal); xo, yo los de la iteración anterior */
int nl_mos_lim(double *x, double *y, double xo, double yo, double vt)
{   double x0 = *x, y0 = *y, xgd = *x - *y;             /* vGD nueva, sin limitar */
    if (yo >= 0.0) {                                     /* modo directo */
        *x = nl_fetlim(*x, xo, vt);
        *y = *x - xgd;                                   /* vDS = vGS(limitada) - vGD */
        *y = nl_limvds(*y, yo);
    } else {                                             /* modo inverso: D y S cambian */
        xgd = nl_fetlim(xgd, xo - yo, vt);
        *y = *x - xgd;
        *y = -nl_limvds(-*y, -yo);
        *x = xgd + *y;
    }
    return *x != x0 || *y != y0;
}
```
- **Cuántas factorizaciones:** solo por tramos, **ninguna por iteración** si la combinación de tramos ya está en la caché (en el transitorio) o es la de la última factorización de `lu_op` (en el OP y el barrido); más una sustitución por cada AO en LIN al crear la entrada (la prueba de estabilidad). Con suaves, **una por iteración**. *Por qué no se reutiliza el patrón de `lu.c`:* la conductancia de una unión cambia 12 órdenes de magnitud entre iteraciones y el pivote bueno cambia de fila; en la calculadora lo caro son las operaciones, no recorrer índices.
- **La caché (solo del transitorio)** pasa a 8 entradas con clave (máscara de S, tramos, hh): `SimTramos` es un `unsigned long long` con 2 bits por elemento. Cada entrada guarda también `cache_inest` (bit k: el AO k en LIN no vale con esta matriz; **solo se calcula en las entradas de paso h**, §3.F) y, si hay suaves, la base sin factorizar (`lu_base[N_CACHE]`). El arranque del puente usa 3 combinaciones de tramos × 2 pasos: 6 entradas. `cache_de` y `prepara_lu` siguen escribiendo `res_.factorizaciones` y `res_.d`, como hoy: solo los llama el transitorio.

### E. Punto de operación y homotopías (`op_interno` con no lineales)
```
op_interno(s, d, st, cnt): como hoy hasta el bucle de los rescates de analiza (con los dos recorridos
                         del §B) y el mapa. st y cnt: los de quien llama (el emulador: &M.nl, &res_.nw)
  si NL.n == 0: como hoy (una LU, una sustitución)                               -> bit a bit
  nl_prepara(): la lista NL, Vesc, Iesc; st: tramos de reposo (D: CORTE; AO: LIN); vlim = 0; frio = 1; x = 0
  op_lista = 0 (la próxima matriz del modo 0 se factoriza sí o sí)
  r = newton(NL_OP, katz 1, metodo 0, t 0, izq 1, hh 1, lam 1, gd 0, x, 100, st, cnt)   (solo tramos: 4·ntramos + 20)
  si r ≠ OK y hay suaves: r = gmin_escalonado(x);  si OK: homotopia = 1
  si r ≠ OK:             r = rampa_fuentes(x);     si OK: homotopia = 2
  si r ≠ OK: SIM_NO_CONVERGE (elem = culpable, t = −1, campo = NC_OP)
  con AO: la regla del biestable (§F)
  x_op = x; lu_op = la última factorización (el jacobiano en la solución, o la LU de sus tramos)
  cnt->iter_op = las iteraciones de este OP; cnt->homotopia = homotopia
  todo el OP gasta como mucho NL_OPS_OP = 1 500 000 operaciones (~0,95 s; es una llamada que bloquea
  la pantalla): si no, SIM_NO_CONVERGE con NC_TOPE
  aviso HOMOTOPIA si homotopia > 0
  (ni la caché del transitorio, ni M, ni res_: la ruta del OP no los toca; el emulador copia
   lo que necesite del OP a su arranque, como hoy)
```
Quién pasa qué: `sim_iniciar_red` pasa `&M.nl` y `&res_.nw` (acaba de borrar los dos, como hoy borra `res_` y `M`); `sim_op_cc_red`, `sim_punto_q` y `sim_cc_punto`, unos locales; las llamadas de hoy que no ven no lineales (`sim_thevenin_cc`, que con un no lineal da antes `SIM_NO_LINEAL`, y `sim_modos_prueba` del arnés) pasan NULL, que con `NL.n == 0` no se mira.
```
gmin_escalonado(x):  g = 1e-2; fac = 10; x_ok = 0 (con sus vlim y tramos); g_ok = −1
  bucle:
    r = newton(NL_OP, 1, 0, 0, 1, 1, 1, g, x, 50, st, cnt)
    si r == OK: x_ok = x; g_ok = g; si g == 0: OK
                g = (g/fac < 1e-12) ? 0 : g/fac; fac = mín(2·fac, 10)
    si no:      x = x_ok; si g_ok < 0: prueba g = 1e-1 y luego 1 (si también fallan: FALLA)
                fac = √fac; si fac < 1,001: FALLA; g = g_ok/fac
rampa_fuentes(x):  lam = 0; dl = 0,1; x = 0; x_ok = 0
  mientras lam < 1:
    l2 = mín(1, lam + dl); r = newton(NL_OP, 1, 0, 0, 1, 1, l2, 0, x, 50, st, cnt)
    OK: lam = l2; x_ok = x; dl = mín(1,5·dl, 0,5)
    no: x = x_ok; dl = dl/4; si dl < 1e-4: FALLA
  (se escalan las E y las J; no las ganancias de las controladas, ni Vγ, Vz o V±)
```
- **`sim_op_cc_red`** (`[=]` sin dinámica y `[P]`): copia la red con C y L sin V0 ni I0, los S con t1 = t2 = −1 (su estado al empezar) y cada E y J pasada a `ONDA_CC` con su nivel de CC (`onda_nivel_cc`, nuevo en ondas.c: continua p0; senoidal p3; cuadrada p3 + p0·(p2/50 − 1); triangular p3; pulso p0), guarda el estado, `op_interno` con un `NlEst` y un `SimNewton` propios (el segundo se copia a `*nw` si no es NULL), `valores_de`, repone. `sim_punto_q` y `sim_cc_punto` hacen lo mismo. Ninguna de las tres escribe en `res_`, `M` ni la caché del transitorio.

### F. La regla de la realimentación del AO
Con la LU de la combinación de tramos en uso y el AO q en LIN, una sustitución con esta perturbación. **Qué LU:** en el OP y el barrido, la del modo 0 (`lu_op`, o la de la iteración con suaves). **En el transitorio, siempre una de paso h** (la del trapecio): la entrada (mask, tramos, h) de la caché, que se crea si no está (es la que usará el trapecio). También en el arranque (cuyos medios pasos usan hh = h/4) y en los pasos partidos (hh = dt): con otra matriz la regla podría decir otra cosa (el compañero del C es 4 veces mayor en el arranque) y el AO iría y vendría entre LIN y una saturación en cada evento hasta la guarda de Zenón. Con suaves, en los pasos del trapecio vale la matriz de la iteración (ya es de paso h); en el arranque y en los partidos se copia la base de la entrada de paso h a `lu_nl`, se estampan los suaves en el x de ese momento y se factoriza (una LU más, solo si hay a la vez un AO en LIN y suaves).

| AO | Perturbación | Se mide | LIN vale si |
|---|---|---|---|
| ideal (nulor) | rhs = e_j (la fila j pasa a V⁺ − V⁻ = δ) | S = ∂Vsal/∂δ = x[cc] − x[cnref] | S < 0 |
| real, Ro = 0 | rhs = e_j (la fila j: Vsal − A·Vd = δ) | ∂Vsal/∂δ = 1/(1 − A·G_lazo) | > 0 |
| real, Ro > 0 | iny(nref, c, 1/Ro) (una δ en la fuente interna) | ∂u/∂δ = 1 + A·ΔVd | > 0 |

- Si la LU con algún AO en LIN es **singular** (un comparador sin realimentación; + y − atados a fuentes), ese AO no puede estar en LIN: el culpable es el AO en LIN cuya columna de rama o de salida es `col_mala`, o el primero en LIN.
- **Dónde se mira:** al crear cada entrada de paso h de la caché (se guarda en `cache_inest`), en cada factorización de `lu_op` y en cada factorización con suaves de paso h (y la de más del párrafo de arriba). En el paso 3.a del Newton, un AO en LIN que no vale sale de LIN sin avanzar (th = 0).
- **A dónde va:** desde LIN, a SAT+ si Vsal ≥ (V+ + V−)/2 (ideal) o u ≥ (V+ + V−)/2 (real), si no a SAT−; al salir de una saturación por su margen, a LIN si LIN vale con la matriz nueva y, si no, a la otra saturación (así el Schmitt salta de +V a −V al cruzar el umbral y el comparador sin realimentación también).
- **Comprobado a mano** (V⁻ = entrada inversora): inversor, S = −(R1 + Rf)/R1 < 0; no inversor, S = −(R1 + R2)/R1 < 0; seguidor, S = −1; Schmitt (V⁺ = β·Vsal), S = 1/β > 0; integrador con V0 (el C, rama V de V⁻ a la salida en el OP), S = −1, y en el transitorio estable; astable **con el C sin V0** (vacía): en CC (C abierto, V⁻ = Vsal) G_lazo = β − 1 < 0, estable; con el compañero del C (2C/h ≫ 1/R), G_lazo ≈ β > 0, inestable. **Astable con V0 = 0 dato** (lo que pone `circ_elemento` a todo C nuevo): en el OP el C fija V⁻ = 0 y solo queda la realimentación positiva: S = 1/β > 0, el lineal no vale ya en el OP y el AO sale a V+ con el aviso `BIESTABLE` (la otra saturación también cuadra con V⁻ = 0). No es un error: con el C descargado, la salida puede arrancar en cualquiera de los dos raíles; el primer cambio llega igual en RC·ln 2.
- **Biestable en el OP:** tras converger, cada AO saturado cuyo LIN no vale se prueba en la otra saturación (un Newton con ese AO fijo allí y los demás como quedaron). Si también converge sin que ese AO salga de ella, aviso `BIESTABLE` («U1 puede empezar en V+ o en V-: empieza en V+.»: vale para el Schmitt, que tiene dos estados estables, y para el astable con su C en V0, que tiene dos de arranque). Se queda la primera solución, que es determinista: el Schmitt con la entrada a 0 empieza en V+.
- **Arranque:** con algún AO, t = 0 es una discontinuidad (`disc0 = 1`): el primer medio paso de Euler decide los tramos con Katzenelson (§G), con la regla evaluada en la matriz de paso h. Si ahí un AO sale de LIN por la regla, aviso `ARRANQUE` («U1 arranca inestable: su salida se va a V+»). Así arranca el astable con el C sin V0; sin la regla se quedaría en Vsal = 0 para siempre (como SPICE sin condición inicial).
- *No adoptado:* «probar sin las V0 antes de culpar al AO» cuando la LU del OP sale singular. Los lazos de V0 que la hacían singular (con la salida de una EC o un AO, o con el nulador) los resuelve ya el análisis estructural (§B, los dos recorridos) antes de factorizar, y un reintento sin las V0 escondería un comparador de verdad con un C en su entrada (le quitaría la V0 sin motivo y seguiría singular).
- *Fuera:* elegir entre soluciones de redes suaves (dos BJT en biestable): el OP da la que da la homotopía.

### G. Transitorio con no lineales

**G.1 El paso** (`un_paso`; con `NL.n == 0`, el de hoy sin tocar):
```
(metodo, t_obj, hh, izq, parcial) como hoy
si hay suaves y M.nivel > 0: t_sub = el siguiente subpaso (G.3); si t_sub < t_obj − tol: t_obj = t_sub, hh = h/2^nivel
    (en el arranque: medio paso de Euler de h/8 partido en 2^nivel; hh = 2·dt)
guarda (x, M.nl, vC, iC, iL, vL) para poder rechazar
katz = M.katz
matriz: sin suaves, la LU de la caché (mask, tramos, hh) o la suelta si es parcial (como hoy);
        con suaves, la base de la caché o la suelta
si !katz y algún AO en LIN no vale con la matriz de PASO h (cache_inest de la entrada (mask, tramos, h), §F):
        evento_nl(M.t) y vuelve                                                    (el astable si disc0 = 0)
x0 = katz ? M.x : predictor(t_obj)   (lineal con los dos últimos puntos aceptados)
r = newton(NL_TR, katz, metodo, t_obj, izq, hh, 1, 0, x0, katz ? 50 : 12, &M.nl, &res_.nw)
    (con katz, la regla del paso 3.a también mira la matriz de paso h, no la de hh)
  CICLA -> SIM_NO_CONVERGE, NC_CONMUTA, t = t_obj
  NOCONV o SINGULAR -> restaura; si hay suaves y el nivel puede subir: M.nivel++, res_.nw.sub++, vuelve sin avanzar
                       si no -> SIM_NO_CONVERGE (NC_TR, t = t_obj, elem = culpable)
si !katz y algún margen de los tramos congelados < −1e-9·Vesc en x:  localiza (G.2); vuelve
M.katz = 0
si hay suaves y metodo == 1: control de error (G.3); si rechaza: restaura, M.nivel++, vuelve sin avanzar
acepta: estados de C y L, valores_de, canales, registro (como hoy, con V2/I2)
si fue un subpaso: M.pos += 1024 >> M.nivel; si M.pos == 1024: M.pos = 0, M.k++   (en vez del M.k++ de hoy)
lo de hoy tras el paso: evento de fuente si t == t_ev; fin
```
- **`aplica_evento`** (fuentes, S) y **`evento_nl`** (cambio de tramo) hacen lo de hoy (anclas, máscara, caché, impulsos, reanclar, arranque de 8 pasos de Euler de h/8) con la clave de tramos, y además `M.katz = 1` si hay elementos por tramos, `M.nh = 0` (la historia del error local) y `M.nivel = 0`. Así el primer medio paso tras **cualquier** evento decide los tramos con Katzenelson, sin localizar: dos diodos que llegan a la vez al codo (D1 y D4 del puente) no repiten el mismo evento.
- **`evento_nl(t*)`**: cambia los que salen (margen ≤ 1e-9·Vesc en t*; el AO con la regla); `res_.nw.eventos++`; Zenón (G.4); `aplica_evento(t*)`.
- **t = 0:** con algún AO, `disc0 = 1` (§F). Con solo diodos, las reglas de hoy.

**G.2 Localización del cambio de tramo** (Illinois):
```
ta = M.t, fa = el menor margen en M.x de los que salen (≥ 0); tb = t_obj, fb = el menor en x (< 0)
si fa ≤ 1e-9·Vesc: t* = ta (el cambio es ya)
si no, hasta 30 veces:
   tc = tb − fb·(tb − ta)/(fb − fa)
   fc = el menor margen en el paso parcial de M.t a tc (metodo 1: hh = tc − M.t; arranque: hh = 2·(tc − M.t));
        sin suaves: prepara_lu(lu_suelta) y una sustitución; con suaves: un Newton congelado
   si |fc| < 1e-12·Vesc o tb − ta < 1e-12·h: t* = tc; fin
   si fc < 0: (tb, fb) = (tc, fc); si el lado se repite: fa /= 2
   si no:     (ta, fa) = (tc, fc); si el lado se repite: fb /= 2
acepta el paso parcial a t* (izq = 1, reancla: M.t_ancla = t*, M.k = 0) y evento_nl(t*)
```
Cada prueba es una factorización suelta: 3 a 6 por cambio de tramo (el puente: 20 cambios en 5 periodos).

**G.3 Los suaves: Newton en cada paso, subdivisión y error local**
- **Subpasos.** `M.nivel` = m (0 a 10) y `M.pos` = dónde va dentro del paso de la malla, en unidades enteras de h/1024 (sin deriva y sin reanclar). El subpaso es de h/2^m y termina en t_ancla + (k + (pos + 1024/2^m)/1024)·h. Si cae más allá de un evento o del final, se parte como hoy (un paso parcial reancla).
- **Se rechaza y se parte** (m + 1, con `res_.nw.sub++`) si:
  1. Newton no converge en 12 iteraciones;
  2. el error local del trapecio en un C o una L pasa de 1e-4 de su escala: LTE = (hh³/12)·|x‴|, con x‴ = 6·(la diferencia dividida tercera de los cuatro últimos puntos aceptados: vC en un C, iL en una L), frente a 1e-4·máx(|x|, Vesc) (C) o 1e-4·máx(|x|, Iesc) (L); hace falta historia de 3 puntos (tras un evento, no se mira);
  3. la corriente de un suave cambia en el subpaso más del 10 % de máx(su mayor |i| hasta ahora, 1e-3·Iesc).
  Con m = 10 y aún rechazado por el error local o por Δi, se acepta (a h/1024 el error ya no mejora partiendo); si lo que falla es Newton, `SIM_NO_CONVERGE` (NC_TR).
- **Sube** (m − 1) tras dos subpasos aceptados seguidos con LTE < tol/16 y |Δi| < 2,5 %, si `M.pos` es múltiplo de 2·(1024 >> m).
- **En el arranque** (medios pasos de Euler de h/8): el mismo mecanismo parte cada medio paso en 2^m (m ≤ 6); solo se registran los puntos de los finales de cada h/8 (los que usa `arranque_salida`), así que la y⁺ y la historia BDF2 no cambian.
- Los subpasos cuentan como pasos para `M.tope` y cada subpaso del trapecio se registra (el balance sigue siendo exacto por puntos medios).
- *En el prototipo del motor* (puente exponencial, 5 periodos a T/100): sin subdivisión, error de 1,04 A en el pulso de corriente del C (el pulso, ~85 µs, es más corto que un paso de 167 µs); con subdivisión, 0,012 A, con 591 subpasos y 1199 iteraciones.

**G.4 Zenón.** Si |t* − M.t_ev_nl| ≤ 1e-9·h, `M.ev_mismo_t++`; si no, vuelve a 0 y `M.t_ev_nl = t*`. Más de 4·MAX_EL = 128 cambios en el mismo instante, o `NLR_CICLA` dentro de un medio paso con Katzenelson: `SIM_NO_CONVERGE` con `campo = NC_CONMUTA` («conmuta sin parar»). El arnés la prueba con un gancho, `sim_nl_prueba(NLP_INESTABLE, t0)`: desde t0, la prueba de estabilidad dice siempre «no vale».

**G.5 Lo que no cambia.** `busca_impulsos` (un C directo sobre la salida de un AO que conmuta da un salto que no se anota como `SimSuceso`: el arranque de Euler lo resuelve igual; límite conocido, §8), `sim_avanzar` (todo el estado nuevo vive en `M`, así que partir la emulación en trozos sigue dando lo mismo bit a bit), `siguiente_evento`, `muestra`, `sim_medir`.

### H. Valores, balance y regiones
- **`valores_de`**: la tabla del §2.8. Unión: I = la corriente del modelo en la solución; tramos: G_τ·V + I0_τ; AO: I = x[j] (rama) o (u_τ − Vsal)/Ro (Norton, con u_τ = A·Vd en LIN o V± saturado), V2 = Vd, I2 = Vd/Ri (0 sin Ri); BJT: V = VCE, I = IC, V2 = VBE, I2 = IB; MOS: V = VDS, I = ID, V2 = VGS; EC: V = φb − φa, I = x[j]; JC: V = φb − φa, I = su ley. `reg[k] = nl_region(...)`.
- **Regiones** (solo para enseñar): D: CONDUCE si I > 1 µA; RUPTURA si I < −1 µA y V < −(Vz/2) (zener); si no, CORTE. En los por tramos, la del tramo. BJT (con pol: pol·IC, pol·vBE, pol·vBC): CORTE si pol·IC < 1 µA; si no, SATURACION si pol·vBE > 0,4 V y pol·vBC > 0,4 V; INVERSA si solo pol·vBC > 0,4 V; si no, ACTIVA. MOS: CORTE si vov ≤ 0; OHMICA si vDS < vov; si no, SATURACION. AO: su tramo (LINEAL, SAT_MAS, SAT_MENOS).
- **`Punto`** gana V2[], I2[]; **`registra`** integra la energía por puntos medios en los dos puertos: W[k] += dt·¼·[(pV + V)(pI + I) + s_k·(pV2 + V2)(pI2 + I2)], con s_k = +1 en BJT y MOS, −1 en el AO y 0 en el resto. Por Tellegen en los puntos medios (KCL y KVL valen en los dos extremos de cada paso, así que valen para las medias), el balance sigue siendo exacto aunque las leyes no sean lineales.
- **`termina`**: entregada += W de E, J, EC, JC y AO; almacenada, C y L; disipada, el resto menos S y AMI (R, instrumentos con R, D, BJT, MOS). `reg_fin[k]` y los máximos de V2 e I2.

### I. Pequeña señal: CA y Bode
```
sim_punto_q(s, L, d, nw):  guarda el estado
   s2 = *s con: cada E y J a su nivel de CC (onda_nivel_cc; una alterna fasor ya es senoidal con p3 = 0);
                C y L sin V0 ni I0; cada S con cerrado0 = su estado final y t1 = t2 = −1
   op_interno(&s2, d, &st, &cnt) (con homotopías; st y cnt locales) -> error: se devuelve (el del OP)
   L->q = valores; por cada no lineal: su tramo (D por tramos, AO) o sus g en Q (suaves, sin límites)
   repone el estado (no toca res_, M ni la caché)
ca_punto_q(s, L, d): si sim_hay_nl(s): sim_punto_q(s, L, d, NULL); aviso PEQ_SENAL; SATURA por cada AO saturado en Q
                     si no: L->hay = 0
ca_pon_q(L): la linealización que estampa ensambla_en desde ahora (lin_act = L; NULL = ninguna)
analiza_con (ca.c): tras sim_red_de_ex y ANTES de ca_excitacion/ca_apaga: ca_punto_q(s, &lin_ca, d) (las fuentes
                    de CC fijan Q); ca_pon_q(&lin_ca); luego lo de hoy; en ensambla_en, tras su bucle, si lin_act:
                    sim_estampa_lin(s, lin_act, &mapa_, suma_cx, &luc_); al acabar: ca_pon_q(NULL)
bode.c: B gana `SimLin lin` (su propia linealización, ~2,5 KB)
   bode_iniciar_red (que es la que apaga; bode_iniciar la llama, y el arnés la llama directamente):
        tras B.s = *s y ANTES de ca_apaga: si sim_hay_nl(&B.s): ca_punto_q(&B.s, &B.lin, &d) (error -> FIN_ERROR);
        luego ca_apaga y lo de hoy
   H_en: antes de cada ca_ensambla, ca_pon_q(B.lin.hay ? &B.lin : NULL), como ya vuelve a poner la topología
        (topo_lista): entre dos trozos, o entre bode_en del cursor, otro análisis de CA (el [tab] a P_CA, que hace
        su propio punto Q y al acabar pone NULL) puede haber cambiado la de ca.c
```
- Con no lineales el aviso `CC_FUERA` no se da (sustituido por `PEQ_SENAL`: «Pequeña señal: no lineales linealizados en Q.»).
- **Cuándo se llega aquí:** `[B]` (el Bode) y, desde él, `[tab]` (P_CA a la f del cursor, `ca_analiza_f`). `[=]` con una fuente alterna y un no lineal **emula** (D14); la calculadora de CA y Thévenin con un no lineal dan `SIM_NO_LINEAL` con su nombre (D22). Con solo controladas (lineales), todo como hoy y los tres motores las estampan.
- **La calculadora de CA (calca.c) con controladas:** en su primer bucle, una EDEP o JDEP con la ganancia en `[?!]` → `con_no_ca(s, k)` («En CA no se despeja la ganancia de E2: ponla como dato (en CC, [=] sí la despeja).»); con la ganancia vacía → `con_sim(s, SIM_FALTA, k)` con `campo = Q_VAL`. Así el `switch` de las incógnitas (que acaba en `default: break`) nunca ve una ganancia buscada, y `el_de` con `tolera` nunca tiene que inventar su valor. Sus datos de V e I (con `FA`, §2.2) van a las ecuaciones como los de una E fasor: con su convenio (φb − φa) y su ángulo.
- `CaRes.reg[k]` = la región en Q (la tabla de P_CA la enseña).

### J. Barrido de CC (`barrido.c`, por trozos como `bode.c`)
```
barr_iniciar(c, r, aj):
   el que se barre: aj.el, o el automático: el que tiene [?!] en su valor (R, E, J o controlada); si no, la primera
      E continua; si no, la primera E o J; si no, la primera R; si no -> SIM_NADA_BARRER
   bx: E -> BX_E; J -> BX_J; R -> BX_R; EDEP, JDEP -> BX_K
   copia del circuito con el valor barrido como dato (x0): barr_circuito(); sim_red_de_ex -> error: BarrRes.d
   rango (si no se dio): una E o J de valor v: [−|v|, |v|] (v = 0: [−1, 1]); una R de valor v: [v/100, 100·v]
      logarítmico (sin valor: 1 Ω a 1 MΩ); una ganancia k: [0, 2k] (k = 0: [0, 1])
   canales: las sondas (sim_canales); sin sondas, los automáticos: 1) los V e I con dato o pedidos; 2) la Vsal
      del primer AO; 3) la I y la V del primer diodo, zener o LED; 4) IC y VCE del primer BJT (ID y VDS del MOS);
      5) V(AB); 6) la I de la primera R. Los dos primeros que haya.
      La Vsal de un AO es CANAL_NODO de su nodo c con el nombre «Vsal(U1)» (se escribe en ch->nombre tras
      nombre_canal): nref es la referencia de su subcircuito y vale 0 V, así que phi[c] es Vsal. CANAL_V no
      vale: canal_val hace φa − φb, que en el AO es Vd.
   malla: n = 281 puntos (141 si la estimación pasa de 3 s; 71 si pasa de 6 s), lineal o logarítmica
   vista de CC: la de sim_op_cc_red (aviso CC_BARRIDO si había algo dinámico)
barr_avanzar(ops): punto a punto (solo se para entre puntos):
   el primero: OP completo (la cadena del §E)
   los demás: sim_cc_punto(&s, el, bx, x_i, &io, d): Newton NL_BARR (katz 1, 30 iteraciones) desde io
      falla -> parte [x_{i−1}, x_i] en 2, 4, … 256 trozos, cada uno desde el anterior
      aún falla -> OP completo en x_i (sin io): OK -> salto = 1 (aviso SALTO); no -> hueco (ok = 0,
               aviso HUECOS, x_fallo = x_i); el siguiente sigue desde el último bueno
      un AO que pasa de una saturación a la otra -> salto = 1
      TOPE: un punto (con sus trozos y su OP de rescate) gasta como mucho NL_OPS_OP; si se pasa -> hueco
            (como si no convergiera). barr_avanzar solo se para entre puntos, así que sin este tope un
            punto con 256 trozos de 30 iteraciones podría bloquear la pantalla un minuto.
   y[tr][i] = canal en los Valores del punto; it[tr][i] = iteraciones; kcl_peor = máx residuo
   cont_[recorrido][i] = la continuación de ese punto (x, vlim, tramos), para barr_en y barr_valores
   ida y vuelta: la vuelta recorre la malla al revés desde el io del final (la histéresis sale sola)
   familia (el trazador de un transistor): 6 curvas; cada una empieza con un OP completo
   progreso = puntos hechos / puntos totales
```
- **`barr_en(tr, x, &y, &dydx)`**: Newton desde la continuación guardada **de esa traza** (`cont_[recorrido de tr]`) en el punto de la malla más cercano (x, vlim y tramos) y, con la LU del jacobiano en la solución, la derivada exacta con **una sustitución** (`sim_cc_deriva`): dx/dp = J⁻¹·r, con r = ∂b/∂p − (∂A/∂p)·x (F(x, p) = A·x − b = 0; r = −∂F/∂p). r es la suma de dos partes:

  **(1) El elemento barrido** (su estampado derivado respecto a su valor p):

| Se barre | su parte de r |
|---|---|
| E ideal (rama j) | e_j |
| E real | iny(a, b, 1/r) |
| J | iny(a, b, 1) |
| R | fila a: +(φa − φb)/R²; fila b: −(φa − φb)/R² |
| ganancia μ, r, g o β de la controlada q | fila j_q: +V_ctl (VCVS), +I_ctl (CCVS); filas a_q, b_q: −V_ctl y +V_ctl (VCCS), −I_ctl y +I_ctl (CCCS) |

  **(2) Cada controlada q controlada por la I del elemento barrido k** (`ctl_q == k` y `ctl_i`): su I_ctl = Σ c_i·x_i + c0 (`ctl_coef`) depende de p. Con D = Σ (∂c_i/∂p)·x_i + ∂c0/∂p: **R** (I_ctl = (φa − φb)/R) → D = −(φa − φb)/R²; **E real** (I_ctl = (φa − φb)/r + E/r, se barre E) → D = 1/r; **J** (c0 = J) → D = 1; **E ideal** (su I es su rama) → D = 0; **una controlada barrida por su ganancia** (su I es su rama, o su ley en una J controlada, que no puede controlar por su I) → 0. Su parte de r: **CCVS** (fila (φb − φa) − r_q·I_ctl = 0) → fila j_q: +r_q·D; **CCCS** (J = β_q·I_ctl de a_q hacia b_q) → iny(a_q, b_q, β_q·D). Las controladas por una **V** no dependen del valor de k (V_ctl es una diferencia de potenciales).

  dy/dp es el canal aplicado a dx (una I de un suave: con su g). Recuerda las 4 últimas lecturas (como `traz_lee`).
- **`barr_nivel(tr, nivel, xa, xb, &x)`**: Brent sobre barr_en − nivel, con tolerancia 1e-14·|x| y 60 evaluaciones como mucho; el cambio de signo se busca antes en la malla. El nivel lleva signo: el que escribe el usuario con `[N]` es el suyo.
- **`barr_nivel_dato(tr, m, …, &sg)`** (el nivel que sale de un **dato** de V o I, que es una magnitud): busca +m y, si en el rango no hay cruce, −m (una I(D1) medida al revés, una Vsal de un inversor). `sg` dice cuál encontró; el aviso lo escribe con su signo («I(D1) = -20,00 mA en R1 = …»).
- **`barr_valores(tr, x, &v, &d)`**: los Valores completos en x por la misma continuación que `barr_en` (sale el mismo punto, no otra solución).

### K. Paso y ventana con no lineales (`elige_paso`)
- **Modos:** con la linealización del OP (los tramos de la solución y las g de los suaves en ella, estampadas en la matriz de `modos_de`). Con algún AO, además una topología con cada AO en SAT+ (dentro del tope de 8 topologías). `modos_de(mask, tramos, h0, t, out, nout)` gana los tramos, y cada topología de `SimRes` guarda los suyos en `tramos_top[i]` (junto a `mascara[i]` y `t_top[i]`; la pantalla de los modos puede decir «U1 en V+»).
- **Ventana** (automática):
  - con fuentes periódicas y no lineales: **5·T_máx** redondeada a 1-2-5, sin mirar las τ del OP mayores que 10·T_máx (la τ = R·C de un rectificador con los diodos en corte es la de la descarga, que no fija cuándo se llega al régimen: el C se recarga en cada ciclo);
  - sin fuentes periódicas y con algún AO: **10·τ_máx de las topologías saturadas** (un astable de β = 0,5 da ~4,5 periodos);
  - si no, la regla de hoy.
- **Paso:** las reglas de hoy. El costo de un paso con suaves es c_paso = 2·(c_ens + c_lu + c_sust + Σ c_eval) + c_reg (dos iteraciones de media) más 2·`NS_ITER`; con solo tramos, el de hoy más los márgenes. El tope de 2 s y `PASO_LIM` siguen igual.
- `NS_ITER` = 15 000 ns (a calibrar con `BN_NL`); `NL_OPS_EXP` = 30 (a calibrar con `BN_EXP`).

### L. Costo (lo que cuentan `nl_ops` y `sim_ops`)
| Qué | Operaciones |
|---|---|
| `nl_exp`, `log` | 30 (NL_OPS_EXP) |
| evaluar una unión | 30 + ~12 (x < −40: ~6) |
| evaluar un BJT | 60 + ~20 |
| evaluar un MOS | ~15 |
| estampar un suave | 4 por coeficiente (4 en la unión; 16 en el BJT y el MOS) |
| margen de un tramo | 3 |
| residuo | 6 por elemento |
| la prueba de estabilidad | una sustitución |

### M. Mensajes (`sim_mensaje_circ`, `sim_aviso`, `sol_mensaje`; 4 líneas de 52 como mucho)
| Código | Texto |
|---|---|
| `SIM_NO_CONVERGE`, NC_OP | «El punto de operación no converge: Newton se atasca en D1, aun subiendo las fuentes poco a poco. Revisa los valores o pon una R en serie con D1.» |
| NC_TR | «En t = 3,21 ms Newton no converge en D1, ni partiendo el paso. Revisa los valores o usa otro modelo del diodo (con Vγ).» |
| NC_BARR | «Con E1 = 4,20 V Newton no converge en D1: la curva queda con un hueco ahí.» |
| NC_CONMUTA | «En t = 2,00 ms, U1 conmuta sin parar (ningún estado de su salida cuadra). Revisa cómo está realimentado.» |
| NC_TOPE | «El punto de operación tarda demasiado (muchos no lineales): simplifica el circuito.» |
| `SIM_CTL_MAL`, CTL_FALTA | «E2 es una fuente controlada sin control: en su ficha, [+][-] en la fila x elige quién la manda.» |
| CTL_SI_MISMA | «E2 no puede controlarse a sí misma: elige otro elemento en la fila x.» |
| CTL_3PATAS | «E2 no puede controlarse con Q1 (tiene 3 patas): pon un voltímetro o un amperímetro donde quieras medir.» |
| CTL_I_NO | «La corriente de D1 no puede mandar a E2: pon un amperímetro en serie con D1 y controla con el suyo.» |
| `SIM_NO_LINEAL` | «D1 no es lineal: aquí (Thévenin, calculadora de CA) solo caben redes lineales. [B] da el Bode en su punto de operación.» |
| `SIM_NADA_BARRER` | «Nada que barrer: el barrido cambia una E, una J, el valor de una R o la ganancia de una fuente controlada.» |
| `SOL_NO_LINEAL` | «D1 es un diodo: la calculadora de CC es lineal y no despeja con él. Sin [?!], [=] da su punto de operación; para hallar un valor, barre con [I] y lee con [N].» |
| `SIM_MODELO`, QZ_RZ | «D2: rz·Iz tiene que ser menor que Vz; si no, la ruptura quedaría en 0 V.» |
| QL_IF | «D3: con If = 250 mA, Rs·If pasa de Vf y la curva no puede pasar por (Vf, If). Baja If o sube Vf.» |
| QA_VP | «U1: V+ tiene que ser mayor que V-.» |
| Q_VAL (variante fuera de su tabla: un archivo roto) | «D1 tiene un modelo que no existe: elígelo otra vez en su ficha.» |
| `SIM_AO_REF`, AOR_SIN_TIERRA | «U1 no tiene referencia: su salida se mide desde la tierra (su alimentación), y aquí no hay tierra ni fuente. Pon la tierra con [G].» |
| AOR_CORTO | «La salida de U1 está unida a su referencia (la tierra o el - de la primera fuente): quedaría en corto. Muévela.» |
| `SOL_CTL` (la tabla), con el motivo de `circ_ctl_motivo` | los mismos cuatro textos de `SIM_CTL_MAL` (arriba): la tabla y el emulador admiten los mismos controles |

(Los de varias líneas los parte `lineas()`; `r3_textos.py` los midió: 3 líneas como mucho.)

| Aviso | Texto |
|---|---|
| `BIESTABLE` | «U1 puede empezar en V+ o en V-: empieza en V+.» |
| `ARRANQUE` | «U1 arranca inestable: su salida se va a V+.» |
| `HOMOTOPIA` | «Hizo falta subir las fuentes poco a poco (vale).» |
| `PEQ_SENAL` | «Pequeña señal: no lineales linealizados en Q.» |
| `SATURA` | «U1 satura en +15 V: la salida ya no sigue.» |
| `QUEMA` | «D3 lleva 45,2 mA (máx. 30 mA): se quemaría.» |
| `CC_BARRIDO` | «Barrido de CC: C abiertos, L cables.» |
| `SALTO` | «La curva salta en E1 = 7,50 V (histéresis).» |
| `HUECOS` | «La curva tiene 3 huecos: ahí Newton no converge.» |

`dib_emu_esq` busca el primer aviso hasta `SIM_AV_HUECOS`, con prioridad QUEMA > SATURA > los de hoy > los demás nuevos.

### N. Componentes de 3 patas en el modelo (circuito.c, red.c, archivo.c)

**N.1 Huella y orientación.** Con el ancla O = (x, y), la flecha d, (ux, uy) = DIR[d] y espejo esp:
`punto(u, v) = (x + u·ux + v′·|uy|, y + u·uy + v′·|ux|)`, con v′ = esp ? (v0 + v1) − v : v.
- `circ_huella_pt(tipo, d, esp, ox, oy, u, v, &x, &y)` aplica esa fórmula con la `Huella` del tipo.
- `circ_orienta(e, &d, &esp, &ox, &oy)` (nueva): prueba las 8 orientaciones y devuelve la que pone las tres patas donde están (es única: la pata c fija la flecha y el orden de a y b fija el espejo). 0 si ninguna (el elemento está roto).
- `circ_caja` es la envolvente de las tres patas. Una caja de W × H celdas tiene (H + 1)·W + (W + 1)·H segmentos: **12 en el AO y 7 en el transistor**.

AO con el ancla (el −) en (x, y) y BJT/MOS con el ancla (B o G) en (x, y), sin espejo:

| Flecha | AO: − / + / salida | caja | BJT: C (D) / E (S) | caja |
|---|---|---|---|---|
| → | (x, y) / (x, y+2) / (x+2, y+1) | x..x+2 × y..y+2 | (x+1, y−1) / (x+1, y+1) | x..x+1 × y−1..y+1 |
| ← | (x, y) / (x, y+2) / (x−2, y+1) | x−2..x × y..y+2 | (x−1, y−1) / (x−1, y+1) | x−1..x × y−1..y+1 |
| ↓ | (x, y) / (x+2, y) / (x+1, y+2) | x..x+2 × y..y+2 | (x−1, y+1) / (x+1, y+1) | x−1..x+1 × y..y+1 |
| ↑ | (x, y) / (x+2, y) / (x+1, y−2) | x..x+2 × y−2..y | (x−1, y−1) / (x+1, y−1) | x−1..x+1 × y−1..y |

**N.2 `circ_cabe(c, tipo, x, y, d, ci)`**, en orden y parando en el primer problema:
1. `nel == MAX_EL` → CM_LLENO.
2. Las esquinas de la caja; si alguna se sale → CM_FUERA.
3. `circ_mapa_huellas` y, en orden de lectura (y, luego x), cada punto P de la caja (es_pata = P es una de las tres patas nuevas):
   - `dueno[P] ≥ 0` → CM_OCUPADO con `el = dueno[P]` (dos cajas no se tocan, ni en una pata);
   - cada segmento s > 0 que sale de P: si su otro extremo está dentro de la caja, o P no es pata → CM_OCUPADO con `el = s ≥ SEG_EL0 ? s − SEG_EL0 : −1`;
   - P no es pata y tiene la tierra, un terminal o una sonda de nodo → CM_MARCA con `que` = 'G', 'A', 'B', '1' o '2'.
4. CM_OK.

**N.3 Colocar** (`circ_elemento` con huella y CM_OK): `k = nel++`, `memset`, `ref = −1`, la variante por omisión, las patas a, b y c con `circ_huella_pt(tipo, d, 0, x, y, …)`, `etiqueta()`, y **todos los segmentos de la caja** (los h y los v de la envolvente) a `SEG_EL0 + k`. En la rejilla vacía caben **160** colocaciones del AO (40 por flecha) y **186** del transistor (45 con → y con ←, 48 con ↓ y con ↑).

**N.4 `circ_seg_libre(c, x, y, d, &k)`**: no, si el segmento se sale de la rejilla; no, si vale `SEG_EL0 + k` con k de 3 patas; no, si uno de sus extremos es interior (en *k, su dueño); si no, sí. `circ_cable` devuelve 0 y `circ_elemento` (dos bornes) −1 cuando no es libre, **sin tocar nada**.

**N.5 Borrar.** `circ_quitar` de un segmento de la caja llama a `borra_elemento(k)`, que ya vacía todos sus segmentos. Sin cambios (el `ref` de las controladas ya se renumera o queda en −1).

**N.6 Espejo.** `circ_voltear` de un 3 patas intercambia (xa, ya) y (xb, yb): la caja no cambia (las tres huellas son simétricas bajo v → v0 + v1 − v, que manda a ↔ b y deja c). Las sondas I y V de ese elemento **no** existen en esta etapa (D18), así que no hay signos que tocar. En uno de dos bornes, como hoy.

**N.7 `circ_grado(c, x, y)`** cuenta **conexiones**:
```c
for (d = 0; d < 4; d++) { s = circ_seg(c, x, y, d);
    if (s == SEG_CABLE || (s >= SEG_EL0 && circ_npatas(&c->el[s - SEG_EL0]) == 2)) n++; }
for (k = 0; k < c->nel; k++) if (circ_npatas(&c->el[k]) == 3 && circ_pata_en(&c->el[k], x, y) >= 0) n++;
```
Sin componentes de 3 patas da lo mismo que hoy. Una pata con dos cables tiene grado 3 (punto de unión, como el nodo − de un inversor en el libro). Un punto interior tiene grado 0: la tierra, las sondas y los terminales lo rechazan solos.

**N.8 `circ_valido`** (lo nuevo):
- Por cada elemento de 3 patas: `xc < REJ_COLS`, `yc < REJ_FILAS`; `circ_orienta` da 1; la caja cabe en la rejilla; todos sus segmentos valen `SEG_EL0 + k`; `nseg += (x1 − x0)·(y1 − y0 + 1) + (x1 − x0 + 1)·(y1 − y0)`. `variante < nvar`.
- Por cada uno de dos bornes: lo de hoy, más `xc == yc == 0`, y `nseg++`.
- Después: `circ_mapa_huellas` da 1 (las cajas no se pisan); ningún punto interior tiene un segmento que no sea de su dueño, ni la tierra, ni un terminal, ni una sonda de nodo; ninguna sonda I o V apunta a un elemento de 3 patas; la cuenta de segmentos con elemento es `Σ nseg` (hoy es `nel`).
- EDEP y JDEP: `variante < DEP_N`; `ref` en [−1, nel), `ref ≠ k` y, si `ref ≥ 0`, el referido es de dos bornes (el resto de los tipos, `ref` como hoy: [8] pone `ref = 0` en una R). Un control por la I de un diodo o de una J controlada **sí** es válido para `circ_valido` (se puede dibujar y guardar); lo rechazan los motores con `CTL_I_NO`.
- **Diodo, zener y LED: `variante < nvar`** (DIO_N, ZEN_N, LED_N): si no, `NOM_DIODO[v]` y `NL_COLOR[v]` leerían fuera de su tabla al dibujar o al abrir la ficha.
- AO: con V+ y V− datos, V+ > V−.

**N.9 red.c.** `red_extraer`: `usado` y `r->c[k]` para la pata c (−1 en los de dos bornes). `red_cierra`: en la propagación de subcircuitos, un elemento de 3 patas iguala `comp` en a, b y c. `agrupa`: si `pasa(k)`, une a, b y c. `red_grado`: cuenta también c. Los puntos interiores quedan con `punto = −1` (no tocan ningún borne ni cable). `red_ref_de(r, n)`: el nodo m con `ref[m]` y `comp[m] == comp[n]`. `MAX_NODOS` = 70.

**N.10 Archivo.** `version_de`: lo primero, `if (algún tipo ≥ EL_DIODO) return 4;`. Elemento: longitud 85 + (elem2 **o** 3 patas ? 54 : 0) + (3 patas ? 2 : 0), con `xc`, `yc` detrás de la sección 2. Lectura: tras la sección 2, si quedan ≥ 2 bytes, `xc` e `yc`. `elemento_en_rango` comprueba `xc` e `yc`. El bloque global no cambia.

### O. La calculadora de CC con fuentes controladas (resuelve.c)
- **`resolver`**: tras `SOL_CA` y antes de mirar la red, si hay un D, zener, LED, AO, BJT o MOS → `SOL_NO_LINEAL` con `elem` = el primero. Con EDEP y JDEP sigue; antes de `arma`, cada una pasa por **`circ_ctl_motivo`**: si no es `CTL_OK` → `SOL_CTL` con `elem` = ella y el motivo en `Solucion.motivo` (campo nuevo, §2.8) (la tabla podría resolver un control por la I de una J controlada, porque en ella toda I es incógnita, pero el emulador no: el mismo circuito saldría con `[=]` y fallaría en cuanto se le pusiera una sonda).
- **El dato de V** (resuelve.c 111): hoy elige φb − φa con `tipo_info()->fuente`; pasa a `circ_v_convenio(tipo) == −1` (E, J, EDEP y JDEP), que es el convenio de D8. Con `fuente = 0` en las controladas, un dato de V(E2) se escribiría φa − φb.
- **`circ_val_en_ley`**: también `LEY_DEP` (la ganancia es una incógnita si no es dato). `circ_abierto_cc` y `circ_corto_cc`: 0 con `LEY_NL` y `LEY_DEP`.
- **`arma`**: la ley de una controlada, solo con su ganancia dato (como la R):
  - EDEP por V: φb − φa − μ·(φp − φq) = 0 (p, q del §A); por I: φb − φa − r·I_ctl = 0, con **I_ctl = la columna `col_i0 + ctl`** (en la tabla toda I es incógnita; los controles que no admite el emulador ya los paró `circ_ctl_motivo`).
  - JDEP por V: I_k − g·(φp − φq) = 0; por I: I_k − β·I_ctl = 0.
  - El mensaje de `SOL_CTL`: el de `SIM_CTL_MAL` con el mismo motivo (§3.M).
  - `arma` escribe la I del control como la columna `col_i0 + ctl` para cualquier elemento que `circ_ctl_motivo` admite (todos los de dos bornes menos diodos, zener, LED, Z y J controladas).
- **`extrae`**: EDEP y JDEP antes de la rama pasiva: V = φb − φa; `val` = dato, o la ganancia despejada: V/V_ctl, V/I_ctl, I/V_ctl o I/I_ctl; P = V·I (entregada).
- **La preferencia** de hoy («fems desconocidas ≥ 0 con la polaridad dibujada, si alguna solución la cumple») se extiende a las ganancias despejadas de las controladas: los datos son magnitudes y μ y −μ suelen cumplir los dos.
- **La comprobación física** de una ganancia despejada: con |V_ctl| (o |I_ctl|) ≤ 1e-12·escala, no se puede hallar (`SOL_SIN_CORRIENTE` con `elem` = la controlada; el mensaje dice «su control vale 0»). Una ganancia negativa **sí** es física (un amplificador inversor).
- **`sol_residuo`**: la ley de la controlada con sus valores.
- **`proced_generar`**: con alguna EDEP o JDEP devuelve `PROC_NADA` (la pantalla de pasos dice «No hay procedimiento para este resultado.»; D25).
- **Thévenin y Norton de CC** (`sim_thevenin_cc`): sin cambios de código: la inyección de 1 A con las fuentes independientes apagadas ya lleva dentro las controladas (sus filas no dependen de las fuentes). Con un no lineal: `SIM_NO_LINEAL`.

---

## 4. Interfaz

### 4.1 `[=]`, `[P]`, `[I]`, editor, menú y línea de estado
- **`igual()`**, lo nuevo delante de lo de hoy:
```c
static void igual(void)
{
    if (circ_no_lineal(&circ)) {
        if (circ_alterna(&circ)) emular(P_EDITOR);          /* un rectificador en fasores no tiene sentido */
        else if (circ_busca_valor(&circ)) calcular();       /* SOL_NO_LINEAL: el mensaje lleva a [I]       */
        else if (circ_dinamico(&circ)) emular(P_EDITOR);
        else punto_op(P_EDITOR, 0);                         /* P_RESULTADO con res_nl                      */
        return;
    }
    /* lo de hoy, carácter por carácter: las controladas son lineales y las
     * resuelven la tabla, la CA, la calculadora de CA y el emulador */
}
```
  - `circ_busca_valor(c)`: algún `[?!]` en Q_VAL o Q_AUX (una incógnita de la tabla). Un `[?!]` en una V o una I es un **pedido**: el punto de operación lo resuelve y lo destaca.
  - Un circuito sin tipos de la Etapa 4 hace lo de hoy bit a bit (mismas pantallas y BMP).
- **`punto_op(desde, modo_cc)`** (nuevo): con `circ_busca_valor`, aviso «El punto de operación no despeja: quita los [?!].»; si no:
```c
cres = circ;  red_extraer(&cres, &red);
memset(&sol, 0, sizeof sol);  sol.elem = sol.fuente = -1;
sol.estado = sim_punto_op_cc(&cres, &red, &sol.s[0], &op_d, &op_nw) == SIM_OK ? SOL_OK : SOL_SIM;
sol.nsol = sol.estado == SOL_OK;  sol.fuente = /* la única fuente independiente, o -1 (como resolver) */;
res_nl = 1;  op_modo_cc = modo_cc;  pasos.n = 0;  isol = 0;  capa = CAPA_VALORES;
origen = desde;  pant = P_RESULTADO;
```
  `resolver_y_mostrar` pone `res_nl = 0`. `SOL_SIM` es un estado nuevo de `Solucion` solo para la interfaz: su mensaje es el de `sim_mensaje_circ(&cres, 0, &op_d, …)`, **sin SimRed** (como ca.c y el arnés): tras «guarda y repone», `sim_red()` es la red de la última emulación, no la del punto de operación, y el nombre del culpable saldría de otro circuito. Los mensajes del OP solo necesitan el circuito (nombres y valores).
- **Editor, teclas nuevas** (salen de `TIPOS[].tecla` salvo I y P): `[D]` diodo, `[U]` amp. op., `[Q]` transistor, `[M]` MOSFET; `[I]` barrido de CC (`barrido_de(P_EDITOR, &circ)`; vacío: «Nada que barrer: pon una E, una J o una R.»); `[P]` punto de operación de CC (`punto_op(P_EDITOR, 1)`). Zener, LED y controladas, solo en la paleta. **`[Q]` y `[M]` se activan en los pasos 13 y 14 del §7**, con sus modelos: hasta entonces su `tecla` es 0.
- **Al colocar:** una controlada abre su ficha en la fila x con el aviso «[+][-] elige quién la manda (en naranja).»; un LED, «Se enciende en el resultado: [=].»; un AO sin tierra: con una fuente independiente en su subcircuito, «Sin [G], la salida de U1 va al - de E1.»; sin ninguna (`red_ref_propia` = 0), «Sin [G] ni fuentes, U1 no tiene referencia: pon [G].» en C_ERROR (52 columnas).
- **Menú de opciones** (`N_CTX` = 11; caja de y = 40 a 209): 0-7 como hoy; 8 «Barrido de CC: curvas        [I]»; 9 «Punto de operación de CC     [P]»; 10 «Salir al menú principal».
- **Línea de estado** (`estado_problema`), lo nuevo delante de lo de hoy:

| Caso | Texto | Color |
|---|---|---|
| no lineal y `circ_busca_valor` | «Con D1 no se despeja: [I] barre y [N] lee» | C_ERROR |
| una controlada con `circ_ctl_motivo` ≠ CTL_OK | «E2 no tiene control: elígelo en su ficha.» (CTL_FALTA); «El control de E2 no vale: mira su ficha.» (los otros tres) | C_ERROR |
| un AO sin referencia (`red_ref_propia` = 0) | «U1 sin referencia: pon la tierra con [G].» | C_ERROR |
| no lineal, sin dinámica, `sim_valida` falla | «Para el punto de operación falta Vz en D2.» (SIM_FALTA); «D2: su modelo no cuadra: [=] dice por qué.» (SIM_MODELO; `[=]` da el texto entero del §3.M) | C_ERROR |
| no lineal, sin dinámica, bien | «Listo: [=] da el punto de operación» | C_OK |
| no lineal y dinámico o alterno | «Listo: pulsa [=] para emular» (el de hoy) | C_OK |

- **`practicar()`** con un no lineal: aviso «La práctica tapada es de CC lineal.» (no se abre). **`resumen()`** (Mis problemas): «Se da el punto de operación.» o «Se emula.».
- **Antes de activar** `[D]`, `[U]`, `[Q]`, `[M]`, `[I]` y `[P]` en el editor, se buscan en el arnés las pulsaciones de esas letras en P_EDITOR (hoy no hacen nada): ninguna prueba de hoy puede cambiar lo que comprueba.

### 4.2 Colocar, borrar y girar un componente de 3 patas
- **`[U]`, `[Q]`, `[M]`** (o su pieza de la paleta): `accion = ACC_PONER`, la línea enseña `al_poner` y el cursor dibuja las 4 flechas con `esq_flechas_cursor_m`: las que caben (`circ_cabe == CM_OK`) en C_CURSOR y las demás en C_TENUE.
- **Flecha:** si cabe, `guarda_hist`, `circ_elemento`, el cursor salta a la pata `sigue` (la salida del AO, el C o el D) y se abre la ficha (`abre_formulario(k, 0)`). Si no cabe, **no se toca nada**, la acción sigue en pie (otra flecha o `[esc]`), el cursor no se mueve, `marca_x/marca_y` señalan el punto que estorba (un cuadro de 7×7 en C_ERROR) y el aviso dice por qué:
  - «No cabe hacia →: se sale de la rejilla.»
  - «No cabe hacia →: estorba R2.» · «…estorba un cable.» · «…estorba la tierra.» · «…estorba el terminal A.» · «…estorba la sonda 1.»
  - «No caben más elementos (máximo 32).»
- **Cable o elemento de dos bornes contra una caja** (`!circ_seg_libre`): «Ahí está U1: los cables van a sus patas.» o «Ese punto está dentro de U1: no es una pata.»; el cursor no se mueve y la acción sigue.
- **`[G]`, `[T]` y `[O][enter]` en un punto interior:** «Dentro de U1 no hay borne: ponla en una pata.». En una pata funcionan (la tierra puede ir en el + de un AO). **`[O]` + flecha hacia dentro de una caja:** «Sin sondas en las patas: [O] en el punto o un A.» (D18).
- **`[del]` + flecha hacia dentro de una caja** borra el componente entero (se deshace con `[ctrl][Z]`).
- **`[enter]`** (`editar_aqui`): cuenta elementos **distintos** alrededor (los 12 segmentos de una caja son uno). En un punto interior abre su componente.
- **`[F]` con un 3 patas elegido:** espejo, con `guarda_hist`. Avisos: «Espejo: + y - de U1 cambian de sitio.», «Espejo: C y E de Q1 cambian de sitio.», «Espejo: D y S de M1 cambian de sitio.».
- **`[tab]`**: como hoy (el cursor va a la pata a).
- **Línea «Aquí»**: en una pata, su nombre en lugar del segmento («U1 (-)»); en un punto interior, «Aquí: dentro de U1 (no es una pata)».
- **`dib_editor`**: si hay aviso, el aviso gana a la instrucción (la instrucción vuelve con la tecla siguiente). Revisar [9], [38] y [52] por si alguna mira la instrucción justo tras un aviso.

### 4.3 Paleta (22 piezas al acabar la etapa, 6 filas de 4)
- Panel en (12, 22) de 296 × 202 (acaba en y = 224); cabecera «Componentes  [flechas] [enter]» como hoy; celda i en (16 + 72·(i % 4), 42 + 30·(i / 4)); relleno de la elegida (cx, cy − 1, 70, 30); icono (20 × 14) en (cx + 4, cy + 1); tecla en (cx + 30, cy + 2); nombre en (cx + 2, cy + 16).
- **Los índices 0 a 13 no se mueven** ([52b] y las demás recorren la paleta por posición). Del 14 al 21: «Diodo» [D], «Zener», «LED», «E control.», «J control.» (sin tecla: `tipo` = EL_ZENER, EL_LED, EL_EDEP, EL_JDEP), «Amp. op.» [U], «Transistor» [Q], «MOSFET» [M]. PNP y canal P se eligen en la ficha con `[izq/der]`.
- **Las piezas entran con su modelo** (§7): 20 en el paso 11 (hasta «Amp. op.», índice 19), la 21.ª («Transistor») en el paso 13 y la 22.ª («MOSFET») en el 14. Una pieza en la paleta sin su modelo la pondría el mono y fallaría «un elemento de cada tipo se emula». `PAL_N` (hoy 14) es la constante que usan el dibujo, la navegación y el arnés.
- La paleta cambia de dibujo (celdas de 30 px): su BMP cambia a propósito (en los pasos 11, 13 y 14).

### 4.4 Ficha
- Todo por la maquinaria de hoy (`calcula_vis` con `vars`, `CF_NO_BUSCAR`, el rótulo del vacío, `tipo_rango`). La variante en la barra como hoy, con `nom_var`.
- **Pie:** «[arr/ab] campo [izq/der] modelo [F] girar [=]» (`que_variante`: «modelo» en diodo, zener y AO; «color» en el LED; «tipo» en controladas y BJT; «canal» en el MOSFET). En un 3 patas, «[F] espejo».
- **Eco (y = 200) sin escribir:** lo que toma el vacío o el catálogo:
  - «1N4148: Is 2,52 nA · n 1,752 · Rs 0,568 Ω»; «1N4007: Is 7,03 nA · n 1,808 · Rs 34 mΩ»;
  - «1N4733A: Vz 5,1 V a 49 mA · rz 7 Ω»;
  - «Rojo: Vf 1,8 V a 20 mA (n 2, Rs 5 Ω)»;
  - «Ideal: sin corriente de entrada; en lineal Vd = 0»;
  - «NPN: β 100 · Is 1 fA · sin Early»; «Canal N: Vt 2 V · K 1 mA/V²».
- **La fila x (`Q_CTRL`, controladas):**
  - `mag_campo` devuelve un `Mag` estático de relleno: `Q_CTRL` nunca pasa por `circ_mag` (escribiría en `aux`, como el riesgo de `Q_FCIRC` de la Etapa 3).
  - La caja (x = 24 a 127) enseña «V(R2)», «I(R1)» o «—»; el estado sale de `circ_ctl_motivo`: «control» (C_OK), «falta» (CTL_FALTA) o «no vale» (los otros tres; el eco de y = 200 da la primera línea del texto de `SIM_CTL_MAL` con ese motivo), en C_ERROR.
  - Teclas en esa fila: `[+]`/`[-]` → `k = circ_control_sig(c, j, ±1)`; si `k ≥ 0`, `ref = k`; si es −1, «No hay otro elemento de dos bornes.» y `ref` no cambia; `[del]` → `ref = −1`; `[izq/der]` → la variante (V o I), como en todas las fichas; cifras → «El control se elige con [+][-].»; `[?!]` → «El control no se despeja: elígelo con [+][-].».
  - Pie en esa fila: «[arr/ab] campo [+][-] control [izq/der] tipo [=]».
  - **El esquema de arriba** (la ficha y el editor con la controlada elegida): `v.resalta = 1u << ref` sin atenuar, y una línea discontinua (2 px de trazo, 2 de hueco) en C_CURSOR del centro del rombo al centro del elemento que la controla.
- **`[catálogo]`** en la ficha: P_MODELO (§4.10).
- **Nombres y unidades de la ganancia:** `nombre_mag` y `unidad_q` (ui.c 333-355) miran hoy la **primera** fila con esa q (`tipo_campo`), así que la ganancia de una CCVS saldría «E2 = 5000» sin Ω. En EDEP y JDEP, `Q_VAL` se nombra con el nombre de **su** fila (`tipo_campo_v(tipo, variante, Q_VAL)`) y la etiqueta: «μ(E2)», «r(E2)», «g(J2)», «β(J2)», y `unidad_q` usa esa misma fila (Ω en r, S en g, nada en μ y β). En los tipos de hoy, `tipo_campo_v` da la misma fila que `tipo_campo` (las BMP no cambian).

### 4.5 El punto de operación en P_RESULTADO (`res_nl`)
Reutiliza P_RESULTADO (sus capas, P_TABLA, V(AB) y los pedidos). *Descartado:* una pantalla propia (duplicaría el panel y gastaría una de las 4 pantallas libres).

**Panel** (líneas en y = 162, 175, 188, 201 y 214):
1. **Pedidos** en C_INCOG, como hoy (`resultado_de` sin fracción). Un **dato** de V o I en un circuito no lineal no es una ecuación: se compara en magnitud (1e-6 relativo) y se escribe «I(D1) = 20,00 mA (dato) ✓» (C_OK) o «I(D1) = 20 mA (dato): da 21,78 mA» (C_CURSOR).
2. La línea de la fuente de hoy (I y P; sin Req).
3. V(AB) de hoy; «[T] Thévenin» solo si `!circ_no_lineal`.
4. **Newton** (C_SUAVE): prefijo + cuerpo. Prefijo: «Punto de operación: » (20 columnas) o, con `[P]`, «CC, C abiertos, L cables: » (26). Cuerpo: «Newton, 6 iter. ✓», «Newton+gmin, 41 iter. ✓», «Newton+rampa, 41 iter. ✓» o «por tramos, 3 iter. ✓» (el número con 3 cifras como mucho; con más, «999+»). El peor caso, «CC, C abiertos, L cables: Newton+rampa, 999+ iter. ✓», tiene 52 columnas (`r3_textos.py`; el de antes, «CC (C abiertos, L cables): Punto de operación: …», pasaba de 70).
5. **LED** o aviso, con la regla de corte de abajo: «D3 rojo 21,8 mA 100 %  D4 verde apagado» (cada nombre en el color de su LED); si alguno quema, solo ese: «D13 amarillo: 145,2 mA (máx. 30 mA): se quemaría» (C_ERROR; 48 columnas, 52 con signo y «máx. 100,0 mA»); sin LED, el primer aviso (SATURA, BIESTABLE, HOMOTOPIA) en C_CURSOR. Los textos de los avisos nuevos caben en una línea de 52 (§3.M).

**Regla de corte de las líneas de LED** (P_RESULTADO y P_EMU_ESQ, una función `led_linea(buf, 52, …)`): cada LED es un trozo «D3 rojo 21,8 mA 100 %», «D4 verde apagado» o «D12 amarillo ¡quema!» (nombre ≤ 3, color ≤ 8, corriente con 4 cifras, porcentaje ≤ 999); los trozos se separan con dos espacios y se añaden mientras quepan en 52 − 3; si sobran, « +N» al final. En P_EMU_ESQ la línea empieza por «Media: ». El arnés la prueba con los peores casos (D32 amarillo, D31 naranja, corrientes de 4 cifras con signo): ≤ 52 siempre.

**Pie:** «[izq/der] capa  [tab] tabla  [I] barrido  [esc]».
**Teclas:** `[enter]` → aviso «Sin paso a paso: D1 no es lineal.»; `[I]` → `barrido_de(P_RESULTADO, &cres)`; `[T]` con un no lineal → «Thévenin es de redes lineales: D1 no lo es.»; `[esc]` → `origen` (P_EDITOR, P_EJEMPLOS o P_BARRIDO).
**Error** (`SOL_SIM`): el texto de `sim_mensaje_circ` en C_ERROR, el elemento y su lazo resaltados (`v.resalta = op_d.lazo | 1u << op_d.elem`, `atenuar`), pie «[esc] volver y corregirlo». `SOL_NO_LINEAL` (desde `calcular()`): pie «[I] barrer R1   [esc] volver y corregirlo» (R1 = el del `[?!]`).

**P_TABLA con `res_nl`:** la columna de x = 40 se titula «Valor / región» y en cada no lineal enseña su región (`NOM_REG`: «conduce» C_OK, «corte» C_SUAVE, «ruptura», «activa», «saturación», «inversa», «óhmica», «lineal», «satura +», «satura -» en C_CURSOR). Tras las filas (y las de carga y flujo de C y L), una fila por cada 3 patas con su segundo puerto: «Q1  VBE 738,0 mV  IB 24,54 µA» («Vd» en el AO, «VGS» en el MOSFET) y, al final, «Newton: 6 iteraciones · Kirchhoff 3,1E-15 ✓» (C_SUAVE; C_ERROR si pasa de 1e-9).

**Desde P_BARRIDO (`[tab]`):** `sol.s[0]` sale de `barr_valores(tr, x)` y `cres` de `barr_circuito()`; la línea 1 dice «En el cursor: R1 = 360,0 Ω» (C_TITULO).

### 4.6 El LED encendido
- **Nivel** (`esq_led_nivel(I, If, Im)`, entero): 0 si I < If/100; 9 si I > Im; si no, `(int)(8·I/If + 0,5)` entre 1 y 8.
- **Dibujo** (`case EL_LED` en esquema.c, todo en enteros, sobre el símbolo del diodo; `col` = `NL_COLOR[variante].rgb`):
  - nivel 0: el contorno del triángulo en `esq_mezcla(C_FONDO, col, 150)` y dos flechitas de 5 px a 45° hacia fuera, del lado contrario al rótulo;
  - 1 a 8: triángulo relleno en `esq_mezcla(C_FONDO, col, 64 + 24·nivel)`, contorno y flechas en `col`; desde el nivel 4, un halo: circunferencia de radio 10 + nivel/2 en `esq_mezcla(C_FONDO, col, 16·nivel)` (en el 8, dos);
  - 9: relleno C_TEXTO, halo en C_ERROR y un «!» en 6×8 junto al cátodo.
- **Dónde:** P_RESULTADO con `res_nl` (`v.brillo` sale de `sol.s[isol].I`) y P_EMU_ESQ (de `sim_res()->Imed`, la media de la ventana de medida: con PWM al 50 %, brilla a medias). En ningún otro sitio (`esq_vista` lo deja a NULL).
- **Rótulo** (capa Valores con solución): línea 1 «21,8mA» (C_CORRIENTE); línea 2 «100 %», «apagado» (C_SUAVE) o «¡quema!» (C_ERROR). El porcentaje es 100·I/If redondeado (hasta 999 %).

### 4.7 Símbolos, rótulos e iconos (`esquema.c`, todo en enteros)
- **Dos bornes** (sobre su segmento, con el macro `AL` de hoy; c = el centro):
  - **diodo**: hilo hasta c − 5, triángulo hueco (base de ±5 px en c − 5, punta en c + 4) y raya de ±5 px en c + 4;
  - **zener**: lo mismo con ganchos de 2 px en los extremos de la raya;
  - **LED**: el diodo con sus flechitas y su brillo (§4.6);
  - **controlada**: un rombo de puntas a ±9 px a lo largo y ±7 px de lado, hilos hasta las puntas; la E con «+» en c + 4 (lado b) y «−» en c − 4; la J con una flecha de c − 5 a c + 5. El color, el de su independiente (C_FUENTE, C_FUENTE_I): el color dice el tipo de fuente y la forma dice que es controlada.
- **Marco local de los 3 patas** (enteros, en dieciseisavos de paso; los símbolos escalan con `m->px` y `m->py`):
```c
typedef struct { int ox, oy, ux, uy, vx, vy, S, esp; } Local;  /* S = 16·(v0 + v1) */
static int d16(int n) { return n >= 0 ? (n + 8) / 16 : -((-n + 8) / 16); }
static void lp(const Local *L, int u, int v, int *x, int *y)
{ if (L->esp) v = L->S - v; *x = L->ox + d16(u * L->ux + v * L->vx); *y = L->oy + d16(u * L->uy + v * L->vy); }
/* ox, oy: la pantalla del ancla; U = (DX[d]·px, DY[d]·py); V = (|DY[d]|·px, |DX[d]|·py); d y esp de circ_orienta */
```
  - **AO** (− en (0,0), + en (0,32), salida en (32,16)): triángulo con relleno C_FONDO y contorno en C_AO, vértices (4, −4), (4, 36) y (28, 16); hilos (0,0)–(4,0), (0,32)–(4,32) y (28,16)–(32,16); «−» en (8, 8) y «+» en (8, 24) (con espejo, `lp` los cambia de sitio). En el editor con → en (4,2): patas en (144,79), (144,127) y (208,103). Vertical sale de 80 × 36 px (el paso no es cuadrado): se acepta.
  - **BJT** (B en (0,0), C en (16,−16), E en (16,16)): base (0,0)–(6,0); barra de 2 px en u = 6 de v = −6 a 6; colector (6,−3)–(16,−10)–(16,−16); emisor (6,3)–(16,10)–(16,16); punta de flecha rellena de 5 px al 80 % del emisor, hacia fuera en NPN y hacia la barra en PNP.
  - **MOSFET** (G en (0,0), D en (16,−16), S en (16,16)): G a (5,0) y placa en u = 5 de v = −7 a 7; canal en u = 8 partido en [−9,−4], [−2,2] y [4,9]; D: (8,−6)–(16,−6)–(16,−16); S: (8,6)–(16,6)–(16,16); cuerpo (8,0)–(16,0)–(16,6), con la punta hacia el canal en N y hacia fuera en P.
- **Rejilla:** los puntos interiores de una caja **no se pintan** (`esq_dibuja` hace `circ_mapa_huellas` una vez al empezar). **Uniones:** con el `circ_grado` nuevo (≥ 3). **Recuadro de la selección:** la caja ± 8 px. **Capa de corrientes:** sin flechas en los de 3 patas.
- **Rótulos:**
  - sin resolver (`esq_valor_c`, con el circuito): diodo «1N4148», «Vγ0,7V», «ideal» o «exp.»; zener «Vz5,1V»; LED su color, en su color; controlada «2·V(R2)», «-2·V(R2)» o «1k·I(R1)»; AO «ideal» o «A100k»; BJT «NPN β100»; MOSFET «N Vt2V»;
  - capa Valores resuelta: diodo y zener, línea 1 la región en su color y línea 2 la V («0,65V», C_TENSION); controlada, su V (E) o su I (J) y la ley; AO «Vo 5,00V» y «lineal» o «satura +»; BJT la región e «Ic 2,45mA»; MOSFET la región e «Id 4,00mA»;
  - posición: el AO, dentro del triángulo centrado en el local (16,16) (la 6×12 si la etiqueta tiene 3 caracteres o menos; si no, la 6×8 con fondo); BJT y MOSFET, al lado +u de la línea C–E a la altura de la base.
- **Iconos** 20 × 14 (columnas x..x + 19, filas y..y + 13): `ICO_DIODO`, `ICO_ZENER`, `ICO_LED`, `ICO_EDEP`, `ICO_JDEP` (el símbolo en horizontal) y `ICO_AO` (`Local` a mano: **ox = x + 2, oy = y + 2, U = (8, 0), V = (0, 5)**; sin los rótulos «+» y «−»), `ICO_BJT` y `ICO_MOS` (**ox = x + 3, oy = y + 7, U = (14, 0), V = (0, 6)**). Con los marcos de antes (AO: oy = y + 1 y V = (0, 6); transistores: V = (0, 7)) el AO ocupaba las filas y − 1 a y + 15 y los transistores y a y + 14: se salían 1-2 px de su celda. Con estos, el AO va de x + 2 a x + 18 y de y + 1 a y + 13, y los transistores de x + 3 a x + 17 y de y + 1 a y + 13 (`r2_varios.py`, con los puntos extremos de cada símbolo). [65] lo comprueba: ningún píxel de un icono fuera de su celda.

### 4.8 P_BARRIDO (`curva.c`)
**Entradas:** `[I]` en el editor o la opción 8; `[I]` en P_RESULTADO; `[I]` en P_MODELO (la curva de un componente, §4.10); un modelo con `ca = 4` en P_EJEMPLOS.

**Trozos, como el Bode:** un primer trozo en el acto (`barr_avanzar(SIM_OPS_TICK)`); si queda trabajo, P_EMULANDO con `trabajo = TR_BARRIDO`: «Barriendo R1 de 10 Ω a 10 kΩ...» (o «Curva I-V de D1...») y «R1 = 147,0 Ω · [esc] corta». `[esc]` con dos puntos o más: P_BARRIDO con el aviso «Cortado en R1 = 147,0 Ω.»; si no, vuelve con «Barrido cancelado.». Error en el primer punto: P_BARRIDO con el texto de `sim_mensaje_circ` y el pie «[esc] volver».

**Geometría:**
- Cabecera, y de 0 a 14 (C_PANEL): en x = 4, «I(D1) y V(D1) frente a E1» (o «Curva I-V de D1 (1N4148)», o «IC frente a VCE de Q1»); en x = 200, «-5 a 5 V» (C_SUAVE); desde x = 268, nada (el aviso del shift).
- Retícula: x de 34 a 314 (10 divisiones de 28 px), y de 17 a 161 (8 de 18 px); líneas punteadas en C_REJILLA; los ejes x = 0 e y = 0, enteros en C_TENUE, si caen dentro. En escala logarítmica, una vertical por década y marcas de 2 a 9, como el Bode.
- Rótulos de y: los del canal activo cada 2 divisiones, a la derecha de x = 31, en 6×8 y en su color («8m», «0», «-4m»). Rótulos de x en y = 163, en las divisiones 0, 2, 4, 6, 8 y 10 (o en las décadas), centrados, en 6×8.
- Trazas: canal 1 en C_CANAL1 y canal 2 en C_CANAL2; la vuelta, discontinua del mismo color; en una familia, todas en C_CANAL1 y la activa en C_CURSOR, con su rótulo («40µ», 6×8) a la izquierda de su final. Un hueco corta la línea; un salto se dibuja como un trazo vertical punteado.
- Marcas: un triángulo de 4 px en x = 315 a la altura del cero de cada canal; el nivel de `[N]`, una horizontal punteada del color del canal con una «N» en x = 36.
- Cursores: vertical discontinua (C_TEXTO el activo, C_SUAVE el otro), un cuadrito de 3 × 3 en cada traza y «1» o «2» en 6×8 arriba.
- Panel (4 líneas: 175, 188, 201, 214):
  1. «E1 = 1,250 V  I(D1) = 3,400 mA  V(D1) = 612,3 mV» (de `barr_en`); con dos cursores, «x1 = 500,0 mV  x2 = 1,500 V  Δ = 1,000 V»; escribiendo, «→ E1 = 2,5_   (= 2,500 V)» o «→ I(D1) = 20m_   (= 20,00 mA)».
  2. La derivada exacta del canal activo en el cursor: misma unidad, «ganancia dy/dx = -9,999»; A frente a V, «g = dI/dV = 38,21 mS   r = 26,17 Ω»; V frente a A, «r = dV/dI = …»; en una familia, «β = IC/IB = 100,0   r0 = dVCE/dIC = …» (o «sin Early»); con dos cursores, «Δ I(D1) = 5,210 mA   Δy/Δx = 5,210 mS».
  3. «C1 2 mA/div   C2 200 mV/div   x 500 mV/div».
  4. El aviso («I(D1) = 20,00 mA en R1 = 360,0 Ω (1 de 2)»; «La curva tiene 3 huecos: ahí Newton no converge.»; «Sin sondas: I(D1) y V(D1). Pon las tuyas con [O].») o, si no hay, «[C] canal [N] nivel [X] 2 cursores [Y] X-Y [L] log» (C_SUAVE).
- Pie (227): «[izq/der] cursor [N] nivel [+-] zoom [tab] circuito»; escribiendo, «[enter] ir ahí  [del] borrar  [esc] dejarlo».

**Teclas, en este orden** (como `osc_tecla`, con `T_CON_SHIFT`):
1. **Escribiendo** (`entrada` no vacía): cifras, `[.]`, `[EE]`, prefijos (P N U M K G; `[shift][M]` es mega), `(-)` o `[-]` al principio o tras la E, `[del]`, `[esc]` (cancela), `[enter]`: un x lleva el cursor ahí (fuera del barrido: «Fuera del barrido: [menu] cambia el rango.»); un nivel hace `barr_nivel` desde el cursor hacia la derecha, dando la vuelta: el cursor queda en el cruce con «I(D1) = 20,00 mA en R1 = 360,0 Ω» o, sin cruce, «I(D1) no llega a 50 mA: va de 0 a 21,8 mA.» (C_ERROR).
2. `[izq/der]`: el cursor, un punto de la malla; con `[shift]`, una división (28 puntos).
3. `[arr/ab]`: la escala del canal activo (la tabla `ESCALA` del osciloscopio); con `[shift]`, su posición media división.
4. `[+]`/`[-]`: la ventana en x a la mitad o al doble alrededor del cursor, dentro del barrido; con menos de 20 puntos a la vista, «Pocas muestras aquí: [=] afina lo que se ve.».
5. `[C]`: el canal (o la traza de la familia) activo. `[del]`: enseña u oculta el activo. `[A]`: autoescala.
6. `[N]`: empieza a escribir un nivel del canal activo; `[shift][N]`: el cruce siguiente del mismo nivel. Una cifra sin `[N]` empieza a escribir un x.
7. `[X]`: segundo cursor; `[enter]`: cambia de cursor.
8. `[P]`: el máximo del canal activo (sección áurea sobre `barr_en` entre los vecinos del mayor punto de la malla); `[shift][P]`: el mínimo.
9. `[Y]`: X-Y (canal 2 frente a canal 1) o y(x); la curva I-V empieza en X-Y.
10. `[L]`: |y| del canal activo en escala logarítmica (décadas de 1 pA a 1 kA; 0 y los negativos, al pie).
11. `[=]` → `CV_BARRER` (la ventana a la vista con 281 puntos, como el `TZ_BARRER` del Bode); `[tab]` → `CV_CIRCUITO` (P_RESULTADO en el cursor, §4.5); `[menu]` → `CV_AJUSTES`; `[esc]` → `CV_SALIR`.

**Tras barrer** (`cv_nuevo`): si `BarrRes.tr_dato ≥ 0` (un dato de V o I en un canal) o el modelo es de medida 'x', se aplica ese nivel y el cursor salta al cruce: es la respuesta a «¿qué R da 20 mA?». Como los datos son magnitudes, el cruce se busca con `barr_nivel_dato`: +m y, si no hay, −m (la Vsal de un inversor con «V(U1) = 12 V» como dato cruza −12 V); el aviso lleva el signo del que encontró. Un nivel escrito con `[N]` lleva el signo que se escribió.

### 4.9 P_AJUSTES del barrido (contexto `AJ_BARRIDO`)
- Título «Barrido de CC: qué se barre y dónde». Filas en y = 28, 54, 80, 106 y 132 (etiqueta en x = 8; caja de x = 120 a 310): «Barre» ◄ R1 (330 Ω) ► (recorre las E, J, R y controladas); «Desde» y «Hasta» (vacío = «auto (-5 V)»); «Escala» ◄ lineal ► / ◄ logarítmica ►; «Sentido» ◄ ida ► / ◄ ida y vuelta ►.
- Desde y = 160, en C_SUAVE: «Canal 1 = I(D1), canal 2 = V(D1): las sondas.» o «Sin sondas: la I del primer diodo y su V.».
- Avisos: «Fuera de rango: entre -1000 y 1000 V.» (J ±100 A; R de 1 µΩ a 1 GΩ; ganancias ±1e9); «Desde tiene que ser menor que Hasta.»; «La escala logarítmica pide Desde > 0.».
- Pie «[arr/ab] campo [izq/der] opción [enter] listo [=]»; `[=]` aplica y barre. El rango vive en la sesión; `nuevo_circuito` y abrir lo vacían (como el del Bode).

### 4.10 Páginas Modelo y trazador de curvas
- `dib_modelo`: `ti->modelo` y, debajo, `ti->modelo_var[variante]` (si no es NULL). Con un diodo, zener, LED, BJT o MOSFET, el pie es «[I] su curva I-V              [esc] a la ficha» y `[I]` lanza `barr_iniciar_curva(&circ, sel_el)` con origen P_MODELO.
- **El trazador** (`barr_iniciar_curva`, el «IV Analyzer» de Multisim), sobre un circuito sintético con una copia del componente:
  - diodo, zener, LED: E en serie con 1 Ω y el componente, X-Y (V del componente en x, su I en y). E va de −Vr a V(I_top) + 1 Ω·I_top, con I_top = 100 mA (1 A en el 1N4007; 1,2·Im en el LED) y Vr = 1 V (en el zener, Vz + 1 V);
  - BJT: una E ideal entre C y E de 0 a 10 V (pol·) y una J en la base con IB_k = k·2 mA/β, k = 0..5 (IC de 0 a 10 mA): 6 trazas;
  - MOSFET: VDS de 0 a 10 V con VGS = Vt + 0,5·k V, k = 0..5.
- **Textos** (13 líneas de 52 como mucho con su variante; el arnés lo comprueba con `lineas()`):

**Diodo** (común): «Ánodo a, cátodo b (la raya): v = v(a) - v(b) e i / de a hacia b. Conduce en directa.»
- ideal: «Ideal: un interruptor. Conduce (v ≈ 0) con i > 0; / si no, abierto. Aquí conduce con 1 mΩ y abierto / fuga 1 pS: 5 V sobre 1 kΩ dan 4,999995 mA.»
- con Vγ: «Con Vγ (el del libro): v = Vγ + rd·i si conduce; / si no, abierto (fuga de 1 pS). Vγ vacía = 0,7 V; / rd vacía = 1 mΩ. Al emular, cambia de estado / en el instante exacto.»
- exponencial: «Shockley: i = Is·(exp(vd/(n·VT)) - 1), con / v = vd + Rs·i y VT = kT/q = 25,86 mV (27 °C). / Newton lo resuelve en cada paso.»
- 1N4148: «1N4148 (de señal): Is 2,52 nA, n 1,752, Rs 0,568 Ω. / 0,65 V a 4,3 mA.»
- 1N4007: «1N4007 (rectificador de 1 A y 1000 V): Is 7,03 nA, / n 1,808, Rs 34 mΩ. 0,77 V a 92 mA.»
- (todas) «Sin capacidades: no hay recuperación inversa.»

**Zener** (común): «Ánodo a, cátodo b (la raya con ganchos). En / directa es un diodo; en inversa conduce desde Vz.»
- de libro: «De libro: directa (Vγ), abierto o ruptura con / v = -(Vz0 + rz·|i|), Vz0 = Vz - rz·Iz: pasa por / (-Vz, -Iz). rz vacía = 1 mΩ.»
- exponencial: «Exponencial: Shockley más la ruptura / -Iz·exp(-(vd + BV)/VT), con Rs = rz - VT/Iz y / BV = Vz - Rs·Iz: pasa por (-Vz, -Iz) con / pendiente rz.»
- 1N4733A: «1N4733A (1 W): Vz 5,1 V a 49 mA, rz 7 Ω.»

**LED**: «Un diodo que da luz: ánodo a, cátodo b. / Su color fija Vf a If (20 mA): rojo 1,8 V, / naranja 2,0, amarillo 2,1, verde 2,2, azul y / blanco 3,2. Es Shockley con la Is que pasa justo / por (Vf, If): con R = (E - Vf)/If lleva If. / Brillo: la corriente media (lo que ve el ojo), en / 8 niveles; apagado por debajo de If/100. Con / más de Im, en la vida real se quemaría.»

**E controlada**: «Rombo: su tensión la manda otro elemento de dos / bornes (el control: fila x de la ficha). / Por una V: e = μ·V(x); por una I: e = r·I(x). / V e I del control, las de su tabla (su criterio). / Es lineal: la calculadora despeja su ganancia.» **J controlada**: lo mismo con «j = g·V(x); j = β·I(x)».

**AO** (común): «Entradas + (a) y - (b); salida respecto a la / tierra (la de su alimentación, que no se dibuja). / La salida no pasa de V+ ni baja de V- (satura). / Con realimentación negativa trabaja en lineal; / con positiva, o sin ella, salta a V+ o a V-.»
- ideal: «Ideal: no entra corriente y, en lineal, v+ = v- / (la masa virtual).»
- real: «Real: u = A·(v+ - v-) recortada a [V-, V+], tras / Ro; Ri entre + y -. Sin polo ni slew (Etapa 5).»

**BJT**: «C, B, E. NPN: la flecha del emisor sale; PNP: / entra. Ebers-Moll: iF = Is·(exp(vBE/VT) - 1), iR / igual con vBC; iC = (iF - iR)·(1 - vBC/VA) - iR/βr; / iB = iF/β + iR/βr. VA vacía: sin Early. / En la ficha, V = VCE e I = IC; la tabla da VBE / e IB. En PNP salen negativas en activa. / En CA: gm = IC/VT, rπ = β/gm; sin capacidades.»

**MOSFET**: «D, G, S; de acumulación, cuerpo unido a S. / Nivel 1, con vov = |vGS| - Vt: / corte (vov ≤ 0): iD = 0; / óhmica (vDS < vov): iD = K·(2·vov·vDS - vDS²); / saturación: iD = K·vov²; las dos por (1 + vDS/VA). / Canal P: signos al revés. Simétrico: con vDS < 0 / cambian D y S. La puerta no lleva corriente.»

(« / » marca el salto de línea.)

### 4.11 P_EMU_ESQ y P_EMU_TABLA con no lineales
- **P_EMU_ESQ:** `v.brillo` de `Imed`; los rótulos llevan la región al final (`reg_fin`). Tras «Emulado de 0 a …»: «Newton: 2,4 iteraciones/paso, máx. 7; 12 partidos» (C_SUAVE; con solo tramos, «20 cambios de estado»); con LED, la línea de `led_linea` (§4.5) con «Media: » delante: «Media: D2 rojo 12,3 mA 62 %  D3 verde apagado»; luego el primer aviso con la prioridad del §3.M.
- **Canales automáticos del emulador** (`sim_canales` sin sondas): hoy miran C, L, E y J, así que un comparador enseñaría I(E1) y V(E1). Con algún AO, **el canal 1 es la Vsal del primer AO** (CANAL_NODO de su nodo c, con el nombre «Vsal(U1)», como en el barrido, §3.J) y el canal 2, la V del primer C (el astable, el integrador) o, sin C, la primera fuente que no es continua (el comparador: su senoidal). Sin AO, lo de hoy (un circuito sin tipos nuevos sale bit a bit igual). Los modelos de la biblioteca que miden un canal llevan sus sondas, así que esto no cambia sus claves.
- **P_EMU_TABLA:** la columna Valor enseña la región al final en diodos, AO y transistores; una fila más por cada 3 patas con V2 e I2 al final.
- **El osciloscopio** no cambia.

### 4.12 Bode y P_CA con no lineales
- `[B]` con no lineales: el Bode de pequeña señal en su punto Q (§3.I), con el aviso PEQ_SENAL en su panel. Un AO saturado en Q da H = 0 y el aviso SATURA.
- `[tab]` desde el Bode lleva a P_CA a la f del cursor (pequeña señal); su tabla enseña la región en Q de cada no lineal.
- La calculadora de CA y `[T]` con un no lineal: `SIM_NO_LINEAL` con su texto.

### 4.13 Biblioteca y Ayuda
- **Lección 12 «Diodos»** (modelos 23 a 32) y **lección 13 «AO y transistores»** (33 a 42), 10 modelos cada una: la tabla con sus claves en el §6.12. Títulos de 44 columnas como mucho, enunciado de 3 líneas y «qué ver» de 2. Ejemplo (32): enunciado «Un LED rojo (Vf = 1,8 V a 20 mA) con E = 9 V. Se barre su resistencia de 10 Ω a 10 kΩ.»; qué ver «El cursor ya está en I = 20 mA: R = 360 Ω = (9 - 1,8)/0,02. [tab]: el LED, a tope.».
- `CircModelo` gana `barre`, `x0`, `x1` y `log_x` (§2.13). En P_EJEMPLOS, `[enter]` con `ca = 3` hace `punto_op`; con `ca = 4`, `barrido_de` con el rango del modelo; el pie dice «[enter] calcular» o «[enter] barrer». **Dos sitios de ui.c tratan hoy igual todo `ca != 0`** y hay que separarlos: el pie de P_EJEMPLOS (ui.c 1423: `md->ca ? "[enter] analizar…" : "[enter] emular…"`) y el `[enter]` de la lista (ui.c 4543: `if (…ca) abre_modelo_ca(ca)`, que con 3 o 4 abriría P_CA): los dos despachan 3 y 4 antes de mirar `ca != 0`.
- **La lección 13 crece por pasos** (§7): 8 modelos (33 a 40) en el paso 12; el 41 y el 42 (BJT) en el paso 13. El arnés comprueba `mod < N_MODELOS`, así que la lección no puede nombrar un modelo que aún no existe.
- **Los AO de la biblioteca llevan tierra** (sin ella el astable no tiene referencia: `SIM_AO_REF`). El derivador (37) y el astable (39) llevan su C **con V0 vacía**: el derivador arranca sin el aviso CI_SALTA (con V0 = 0 dato la triangular, que vale −1 V en 0⁻, no cuadra con la masa virtual) y el astable arranca con el aviso ARRANQUE (con V0 = 0 dato sería BIESTABLE, §3.F); el régimen es el mismo en los dos casos.
- **Teoría** (dos páginas por lección, ≤ 15 líneas de 52): 12.1 el diodo y sus modelos (interruptor, codo, Shockley; la curva I-V; r_d = n·VT/I); 12.2 rectificar (media onda, onda completa, puente; el rizado ≈ Vp/(f·R·C) y por qué el emulador da menos), recortar y fijar, el regulador zener y el LED; 13.1 el AO (masa virtual; inversor −Rf/R1, no inversor 1 + R2/R1, sumador, integrador, derivador, comparador, Schmitt y astable, f = 1/(2RC·ln(1 + 2R2/R1))); 13.2 fuentes controladas, el BJT (corte, activa, saturación; β; el punto Q y la recta de carga) y el MOSFET.
- **Ayuda:** seis secciones nuevas: DIODOS, ZENER Y LED · [=] CON SEMICONDUCTORES Y AO · BARRIDO DE CC · FUENTES CONTROLADAS · AMPLIFICADOR OPERACIONAL · TRANSISTORES. En «Dibujar»: «[D] diodo  [U] amp. op.  [Q] BJT  [M] MOSFET» y «zener, LED y fuentes controladas, en la paleta». En «Otras teclas del Taller»: «[I] barrido de CC  [P] punto de operación». ACERCA DE: «ElectroFock 4.0 … semiconductores, AO y barrido de CC.». (En el paso 12 la Ayuda va sin la sección TRANSISTORES y «Dibujar» sin «[Q] BJT  [M] MOSFET»: los añade el paso 14, cuando las dos teclas existen.)

---

## 5. Cambios, sitio por sitio

| Sitio | Cambio |
|---|---|
| **nolin.h/.c, barrido.h/.c, curva.h/.c** | nuevos (§2.7, §2.10, §2.11; §3.C-D, §3.J, §4.8); al `Makefile` (OBJS y DEPS) y a `build_host.sh` |
| tipos.h/.c | §2.1 y §2.2: 8 tipos, `LEY_NL` y `LEY_DEP`, variantes, huecos, `Huella` (H_AO, H_BJT, H_MOS), `TipoInfo.huella` y `modelo_var`, `Q_CTRL`, `NOM_*`, las páginas Modelo (§4.10) |
| circuito.h/.c | `xc`, `yc`; `circ_npatas`, `circ_pata`, `circ_pata_en`, `circ_huella_pt`, **`circ_orienta`**, `circ_caja`, `circ_mapa_huellas`, `circ_interior`, `circ_cabe`, `circ_seg_libre`, `circ_etapa4`, `circ_no_lineal`, `circ_busca_valor`, `circ_v_convenio`, `circ_control_sig` (−1 si no hay), **`circ_ctl_motivo`** y el enumerado `CTL_*`; `circ_elemento` (3 patas, variante por omisión, `ref = −1`), `circ_cable`, `circ_grado`, `circ_voltear`, `circ_valido` (§3.N, con `variante < nvar` en todos los tipos nuevos); `circ_val_en_ley` con `LEY_DEP`; `circ_abierto_cc`/`circ_corto_cc` dan 0 con `LEY_NL` y `LEY_DEP` |
| red.h/.c | `MAX_NODOS` = 70; `c[]`; `red_extraer`, `red_cierra`, `agrupa`, `red_grado`; `red_ref_de`, **`red_ref_propia`** (§2.4, §3.N.9) |
| archivo.c | versión 4; la sección 3 del elemento; `elemento_en_rango` (§3.N.10) |
| ondas.h/.c | `double onda_nivel_cc(int forma, const double *p)` (§3.E) |
| sim.h/.c | §2.6 y §3.A-M: `#include "lu.h"`; `el_de` (devuelve 1, 0 o −1; variante en rango; LED, zener y AO imposibles → `SIM_MODELO`) y `sim_red_de_ex` (c, nref, Q_CTRL, **`SIM_AO_REF`**); `valida`; `con_rama`; `puertos`, `rol_de`, `analiza` (ORDEN de 7; EC y AO en los lazos con CI_SALTA y L_SIN_I; **el segundo recorrido con el nulador**); `algebraica` (nulador, salidas y **el diodo que conduce con R·C < h**); `ensambla` (**la ley de nodos común con el puerto 0**) y `termino` (tramos, λ, controladas); `valores_de`; `Punto` y `registra` (V2, I2); `op_interno` (Newton y homotopías, con `NlEst` y `SimNewton` de quien llama; `lu_op` con `op_clave`, sin la caché); la caché (solo del transitorio: 8 entradas, clave de tramos, base, `cache_inest` en las de paso h); `prepara_lu`; `un_paso` (la regla con la matriz de paso h), `aplica_evento`, `evento_nl`; `elige_paso` y `modos_de` (tramos por topología, `SimRes.tramos_top`); `termina`; `sim_canales` (la Vsal del primer AO); `guarda_estado`/`repone_estado` también con `NL`; `nombre_el` (`"REJCLSAVZDEJUQM"`); mensajes y avisos. Nuevas: `newton`, `residuo`, `gmin_escalonado`, `rampa_fuentes`, `localiza`, `sim_op_cc_red`, `sim_punto_op_cc`, `sim_punto_q`, `sim_estampa_lin` (con `SimMapa`), `sim_cc_punto`, `sim_cc_deriva` (con las controladas por la I de lo barrido), `sim_hay_nl`, `sim_residuo_max`, `sim_residuo_acum`, `sim_nl_topes`, `sim_nl_prueba` |
| resuelve.h/.c | `Valores.V2`, `I2`, `reg`; `SOL_NO_LINEAL`, `SOL_CTL` (con `Solucion.motivo`), `SOL_SIM`; `resolver` (`circ_ctl_motivo`), `arma`, `extrae`, `sol_residuo`, `sol_mensaje` (§3.O); **resuelve.c 111**: el dato de V con `circ_v_convenio(tipo) == −1` en vez de `tipo_info()->fuente`; resuelve.c 351 (la preferencia de las fems), también con las ganancias de EDEP y JDEP |
| proced.c | `PROC_NADA` con alguna EDEP o JDEP |
| ca.h/.c | `ca_punto_q(s, L, d)`, `ca_pon_q`, `ca_lin` (sin `ca_sin_q`); `mapa_` (un `SimMapa`, lo rellena `ca_topologia`); `analiza_con` (Q antes de apagar, `ca_pon_q` y al acabar NULL); `ensambla_en` (**la ley de nodos común con (nref, c) en el AO**; `sim_estampa_lin` si hay una linealización puesta); `ca_b_fuentes` (c0 de las controladas); `ca_vi` y `ca_valores` con los tipos nuevos (S absorbida en D, BJT y MOS; entregada en EC, JC y AO; `reg`); `ca_thevenin_red` con un no lineal: `SIM_NO_LINEAL`; avisos PEQ_SENAL y SATURA |
| calca.c | con un no lineal: `CAL_SIM` con `SIM_NO_LINEAL`; EDEP o JDEP con la ganancia en `[?!]`: `con_no_ca` con su texto; con la ganancia vacía: `SIM_FALTA` (§3.I) |
| bode.c | `B.lin` (su `SimLin`): `ca_punto_q` en **`bode_iniciar_red`** antes de `ca_apaga`; `H_en` hace `ca_pon_q` antes de cada `ca_ensambla`; los avisos nuevos en `res_.d` |
| esquema.h/.c | `Vista.brillo`, `marca_x/y`; `esq_led_nivel`, `esq_mezcla`, `esq_valor_c`, `esq_flechas_cursor_m`; `Local`/`lp`; los símbolos, rótulos e iconos (§4.7, con los marcos nuevos de los iconos de 3 patas); rejilla sin puntos interiores; uniones con el `circ_grado` nuevo; recuadro de la caja; **esquema.c 829** (las flechas de corriente): `circ_v_convenio(tipo) == −1` en vez de `tipo_info()->fuente` (las controladas, como las fuentes, sin flecha) |
| gfx.h | `C_DIODO`, `C_AO`, `C_TRANS` |
| ui.h/.c | `P_BARRIDO` (P_N = 29), `ui_curva`, `ui_res_nl`; `igual`, `punto_op` (mensaje de `SOL_SIM` sin SimRed), `barrido_de`; teclas D U I P (Q y M en los pasos 13 y 14); colocar 3 patas y la marca; la paleta (20, 21 y 22 piezas por pasos; `PAL_N`); la ficha (Q_CTRL con `circ_ctl_motivo`, eco, pie); `nombre_mag` y `unidad_q` con la fila de la variante (ui.c 333-355); P_RESULTADO (línea 4 y `led_linea`) y P_TABLA con `res_nl`; P_EMU_ESQ y P_EMU_TABLA; `TR_BARRIDO`; `AJ_BARRIDO`; P_MODELO con `modelo_var` y `[I]`; `N_CTX` = 11; `estado_problema` (AO sin referencia, `SIM_MODELO`), `resumen`, `practicar`; P_EJEMPLOS con `ca` 3 y 4 (**ui.c 1423** el pie y **ui.c 4543** el `[enter]`); **ui.c 3440** (la columna Z de P_CA): `circ_v_convenio(tipo) == −1` en vez de `tipo_info()->fuente`; el primer aviso hasta `SIM_AV_HUECOS` (§4) |
| biblioteca.h/.c, lecciones.c | `CircModelo` (`barre`, `x0`, `x1`, `log_x`; `ca` 3 y 4; medidas 'U', 'u', 'I', 'V', 'x', 'y', 'g'); 20 modelos; lecciones 12 y 13 |
| banco.h/.c | `BN_EXP` (una `exp()`: calibra `NL_OPS_EXP`) y `BN_NL` (un paso del puente con 1N4007: calibra `NS_ITER`) |
| host_test.c | §6, con las pruebas nuevas en `test_e4_*.inc` (como las de la revisión de la Etapa 3); `circuito_sano` y `mismos()` con `xc`, `yc` y las cajas; `sr_el` gana c y nref; `kcl_de` y `ley_de`; **`test_biblioteca_ca` ([53])** filtra `ca == 1 \|\| ca == 2` en sus dos bucles (el de las claves y el del fp); **[54b] 13 y [54c] R30** reescritas con `PAL_N` (§6.1) |
| DESIGN.md, README.md, HOJA_DE_RUTA_CA.md | §19 «Etapa 4»: lo construido, las desviaciones y lo aplazado (D25) |

---

## 6. Plan de pruebas

Los números salen de `scratchpad/e4juez/n1.py` a `n7.py` (mpmath a 40 cifras, VT = 0,025864925786328750067 V, GMIN y RON incluidos) y de scipy Radau (rtol 1e-11) en `n4.py`; los de la revisión, de `scratchpad/e4juez2/` (`r1_rect.py` rectificadores con la réplica del §3.G; `r2_varios.py` y `r4_mos_inv.py` el MOSFET invertido). Tolerancia relativa salvo que se diga otra. **Cada comprobación está en la sección del paso del §7 en que existe lo que prueba;** las que dependen de un paso posterior llevan «(paso N)» y se añaden en ese paso.

### 6.1 Pruebas que cambian a propósito (y solo estas)
1. **[12] el mono:** entran `T_LETRA('I')` y `T_LETRA('Y')` (D, Q, U, M, P, N y L ya están); exige **29 de 29** pantallas; en el último 5 % se abren modelos de las lecciones 12 y 13 por turno; antes de empezar, `sim_residuo_acum_reset()`, y al final **`sim_residuo_acum()`** ≤ 1e-9: el máximo de todas las soluciones de Newton del mono, no solo de la última emulación (así el mono hace de *fuzz* del residuo); `circuito_sano` gana las cajas (segmentos de la caja, puntos interiores sin nada, patas en su huella).
2. **[39]** recorre los 12 modelos de tiempo de la Etapa 2 (índice < 12), no todos los de `ca == 0`; los nuevos van en [66].
3. **[29]** (la exportación del oráculo): la tabla de letras gana 'D', 'E', 'J', 'U', 'Q', 'M' (`SE_N` = 15); los tipos nuevos se exportan en el paso 15 (EFKNET 3).
4. **[53]** (`test_biblioteca_ca`): sus dos bucles filtran **`ca == 1 || ca == 2`**. Hoy hacen `if (!md->ca) continue;`, así que desde el paso 12 los modelos de `ca` 3 y 4 entrarían como de alterna: fallarían «circuito válido y de alterna» y «diez modelos de la alterna» (saldrían 20), y el bucle del fp (`medida == 'L' && ca`) llamaría a `ca_analiza` con los modelos 34, 35, 40 y 42, donde no hay W1. (Cambia en el paso 12.)
5. **[54b] (la prueba 13 de `test_rev3_ui.inc`) y [54c] (R30 de `test_rev3_modelo.inc`):** comprueban que `[der]` en la última fila de la paleta no pasa de la última pieza, que hoy es el vatímetro (13). Con 20 piezas, `[ab]`×3 y `[der]`×3 llegan al zener. **Se reescriben una vez, en el paso 11, en función de `PAL_N`** (así valen con 20, 21 y 22 piezas sin tocarlas en los pasos 13 y 14): con el cursor del editor en (1,1) (R30, un editor nuevo) o en (6,3) (la 13, que ya tiene E1 y W1 en la esquina), `[catálogo]`, `[ab]`×6 (más filas de las que hay: se queda en la última, f = (PAL_N − 1)/4), n×`[der]` (n = 1..3), `[enter]` y `[→]` ponen la pieza `mín(4·f + n, PAL_N − 1)` (con 20 piezas, n = 3 da el AO; con 22, n ≥ 1 da el MOSFET), que es válida, se guarda y se abre; `[izq]` desde la última da la anterior. Los tipos de 3 patas caben con `[→]` en (1,1) y en (6,3). Las demás navegaciones de la paleta (`[ab]`×3 y `[der]` al vatímetro; `[ab]`×2 a la C) no cambian.
6. **BMP que cambian a propósito:** la paleta (celdas de 30 px; en los pasos 11, 13 y 14), el menú de opciones (11), la lista de lecciones (13), las páginas de la Ayuda (pasos 12 y 14) y el banco. **Todas las demás BMP de las Etapas 0-3 salen idénticas** (`cmp` antes y después de cada paso del §7), fichas incluidas.
7. **Nada más cambia:** [8] y [43] (sus archivos salen byte a byte iguales: versión ≤ 3), [13], [27] a [38] (el mapa de las redes lineales es el de hoy), [45] (d) (fasor = trapecio), el oráculo 92/92 y 175/175, [34] (trozos bit a bit).

### 6.2 [55] Los 3 patas en el modelo, el editor y el archivo (sin motor)
- **Geometría** (§3.N.1): AO con → en (4,2): − (4,2), + (4,4), salida (6,3); con espejo, + (4,2), − (4,4). BJT con ↓ en (4,3): C (3,4), E (5,4), B (4,3). En las 3 huellas × 4 flechas × espejo: la caja es la envolvente de las patas, tiene 12 o 7 segmentos y `circ_orienta` devuelve la flecha y el espejo con que se colocó.
- **En la rejilla vacía:** `circ_cabe == CM_OK` en **160** colocaciones del AO (40 por flecha) y **186** del transistor (45, 45, 48 y 48).
- **No cabe:** AO → en (8,2) o en (4,5): CM_FUERA; BJT → en (4,0) o en (9,3): CM_FUERA; con U1 puesto, otro AO → en (5,3): CM_OCUPADO en (5,3) con `el` = U1; sin U1 y con el cable (5,1)-(5,2), AO → en (4,2): CM_OCUPADO en (5,2) con `el = −1`; con la tierra en (5,4): CM_MARCA 'G'; con los cables (3,2)-(4,2) y (4,1)-(4,2): CM_OK (llegan a la pata −).
- **El inversor en la rejilla** (el caso de referencia): E1 (1,4)→(1,3) de 1 V; R1 (2,2)→(3,2) de 1 kΩ; U1 AO → en (4,2); Rf (4,1)→(5,1) de 10 kΩ; RL (7,3)→(7,4) de 2 kΩ; cables (1,3)-(1,2)-(2,2), (3,2)-(4,2), (4,2)-(4,1), (5,1)-(6,1)-(7,1)-(7,2)-(7,3), (6,3)-(7,3), (4,4)-(4,5), (1,4)-(1,5), la fila 5 de (1,5) a (7,5) y (7,4)-(7,5); tierra en (4,5).
  - `red_extraer`: **nn = 4**: A (el −: (4,1), (4,2), (3,2)), B (la salida: (5,1)…(7,3), (6,3)), C (la entrada: (1,3), (1,2), (2,2)) y D (tierra). U1: a = D, b = A, c = B; `red_ref_de(B)` = D. `red_grado`: A 3, B 3, C 2, D 3. `punto` = −1 en (5,2), (6,2), (4,3), (5,3), (5,4) y (6,4).
  - `circ_grado`: (4,2) = 3 (punto de unión); (6,3) = 2; (4,4) = 2; (5,3) = 0; (7,3) = 3.
- **Rechazos:** `circ_cable(5,1,↓)` da 0 y el circuito no cambia; `circ_cable(4,2,→)` da 0; `circ_elemento(EL_RES, 6,2, →)` da −1; `circ_elemento(EL_RES, 7,3, ←)` sustituye el cable (6,3)-(7,3) (está fuera).
- **Borrar y espejo:** `circ_quitar(5,2,↓)` deja nel = 4, los 12 segmentos vacíos, los cables de fuera intactos, Rf pasa del índice 3 al 2 y `circ_valido` da 1. `circ_voltear(U1)`: + en (4,2) y − en (4,4); en la red, a = A y b = D.
- **`circ_valido` rechaza:** un segmento de la caja cambiado a cable; `yc` corrido en 1; unas patas que no forman una huella; una R con `xc` = 1; dos cajas que se pisan (armadas a mano); la tierra o un terminal en un punto interior; una sonda I o V sobre un 3 patas; una EDEP con `ref` = ella misma o hacia un AO; variante 2 en un BJT; **variante 5 en un diodo, 3 en un zener, 6 en un LED** (DIO_N, ZEN_N, LED_N).
- **Controladas:** con E1, R1, R2 y E2 (EDEP), desde sin control `circ_control_sig(E2, +1)` devuelve 0 (E1), luego 1, 2 y vuelve a 0; hacia atrás, desde sin control, 2 (R2). Con E2 sola, **−1** (0 sería E1). Borrar R1 con `ref` = R1 deja `ref = −1`; borrar E1 con `ref` = R1 (índice 1) deja `ref = 0`. **`circ_ctl_motivo`**: sin control → CTL_FALTA; `ref` = ella → CTL_SI_MISMA; hacia U1 → CTL_3PATAS; por la I de un diodo o de una J controlada → CTL_I_NO; por la V de un diodo y por la I de una R, de una E, de una J o de un A → CTL_OK.
- **Archivo:** el inversor sale en **versión 4 y 667 bytes** (bloques de elemento de 85, 85, 141, 85 y 85 B; global de 25); con una sonda de nodo, **695 bytes** (global de 53). Guardar y abrir da lo mismo (`mismos`), también con espejo, PNP, canal P y controladas. `escribe_efk3` extendido lo fija byte a byte. Un lector copiado de la Etapa 3 (versión ≤ 3) rechaza la versión 4. En [13] (archivos rotos) entra un circuito con AO y BJT: 3000 casos y ninguno se abre corrupto.
- **Aleatorio del API** (semilla fija, 20 000 operaciones: colocar cualquier tipo en cualquier flecha, cables, quitar, espejo, tierra): tras cada una, `circuito_sano == 0` y `circ_valido`; `circ_cabe == CM_OK` si y solo si `circ_elemento ≥ 0` en los de 3 patas; toda pata tiene nodo, todo punto interior tiene `punto = −1` y `nn ≤ 70`; cada 100, guardar y abrir da lo mismo.

### 6.3 [56] La calculadora de CC con fuentes controladas
- **VCVS:** E1 = 2 V con R1 = R2 = 1 kΩ en serie (V(R2) = 1 V); E2 (μ = 10, control V(R2)) sobre RL = 1 kΩ: V(E2) = 10 V, I(E2) = 10 mA (1e-12). La misma con μ `[?!]` y V(RL) = 10 dato: los datos son magnitudes, así que μ = 10 y μ = −10 cumplen; con la preferencia de las fuentes (§3.O), queda **μ = 10**. Con μ = −10 **dato**: V(E2) = −10 V (una ganancia negativa es válida: un amplificador inversor).
- **CCCS:** E1 = 1 V, R1 = 100 kΩ y A1 ideal en serie (10 µA); J2 (β = 100, control I(A1)) sobre R3 = 1 kΩ: |V(R3)| = 1 V.
- **CCVS:** E1 = 1 V sobre R1 = 1 kΩ (I(R1) = 1 mA, control por la I de una R, sin rama); E2 = r·I(R1) con r = 5 kΩ sobre RL = 1 kΩ: V(E2) = 5 V.
- **VCCS:** E1 = 5 V sobre R1 = 1 kΩ; J2 = g·V(R1) con g = 1 mS sobre R2 = 1 kΩ: |V(R2)| = 5 V.
- **Lazo:** E1 = 1 V, R1 = 1 kΩ de E1+ a x, R2 = 1 kΩ de x a y, V1 ideal de x a tierra, E2 = −10·V(V1) de tierra (−) a y (+): **φx = 1/12 = 0,083333333333333333 y φy = −10/12** (1e-12).
- **Errores:** sin control, `SOL_CTL` con `elem` = E2 y `motivo = CTL_FALTA`; controlada por la I de una J controlada (J2 = 2·V(R1), E3 = 1k·I(J2)): **`SOL_CTL` con `CTL_I_NO`**, igual que el emulador (`SIM_CTL_MAL` con el mismo motivo), aunque la tabla podría resolverlo; con ella misma, CTL_SI_MISMA; con la ganancia `[?!]` y un control que vale 0, `SOL_SIN_CORRIENTE` («su control vale 0»). `proced_generar` con una controlada: `PROC_NADA`. (`SOL_NO_LINEAL` con cada no lineal va en [58] (D, paso 3), [61] (AO, paso 7) y [64] (BJT y MOS, pasos 13 y 14): en el paso 2 esos tipos aún no tienen motor.)
- **Un dato de V de una controlada** (resuelve.c 111 con `circ_v_convenio`): E2 (μ = 10, control V(R2)) con la V(E2) = 10 dato y μ `[?!]`: la ecuación es φb − φa = ±10 (no φa − φb) y la solución preferida es μ = 10 con V(E2) = +10 V.
- **Thévenin con una controlada:** E1 = 12 V, R1 = 4 Ω de E1+ a n1, R2 = 12 Ω de n1 a tierra, E2 = 0,5·V(R2) con − en n1 y + en A; B en tierra: **Vth = 13,5 V, Rth = 4,5 Ω, IN = 3 A, Pmax = 10,125 W** (1e-12), por `sim_thevenin_cc` y por la tabla (V(AB) en vacío y la I de un A ideal entre A y B).

### 6.4 [57] Controladas en el emulador y en la CA (cruces al azar, semillas fijas)
- **(a) Emulador = tabla:** 2000 redes de CC del generador de [27] y [33] con 1 a 3 controladas de los 4 tipos (ganancias de ±0,1 a ±10; control por la V o la I de un elemento de dos bornes al azar, **también los que no lo admiten**): si `circ_ctl_motivo` ≠ CTL_OK, la tabla da `SOL_CTL` y el emulador `SIM_CTL_MAL` **con el mismo motivo** (las dos reglas son la misma); si no, el punto de operación del emulador (`sim_op_red`) y la tabla (`resolver` sin incógnitas) coinciden a 1e-9 de la escala. Las que la tabla da por singulares se cuentan aparte (al menos 1500 comparadas).
- **(b) Fasor = trapecio:** redes RLC con controladas, como [45] (d), **filtradas por sus modos** (`sim_modos_prueba`): con ganancias de ±0,1 a ±10 muchas son inestables o casi y no llegan al régimen permanente. Solo se comparan las que tienen **todos los modos con Re s ≤ −0,02·|s|** (amortiguamiento de al menos el 2 %) y una ventana de al menos 20·τ_máx (τ_máx = 1/mín|Re s|); las demás se cuentan aparte («inestables o lentas») y se generan redes hasta tener **150 comparadas**: las muestras del último periodo son Re{X(ω̃)·e^{jωt}} a 1e-9.
- **(c) Tellegen en CA** con controladas: ≤ 1e-12 (EC y JC entregan).
- **(d) Errores con nombre:** `SIM_CTL_MAL` con CTL_FALTA, CTL_SI_MISMA y CTL_I_NO (por la I de una J controlada, y de una Z en la CA); dos EC en paralelo, o una EC sobre una E ideal: `SIM_LAZO_V` con las dos en `lazo`. (CTL_I_NO por la I de un diodo, en [58], paso 3; CTL_3PATAS, en [61], paso 7.)
- **(d2) Un C con V0 en la salida de una EC** (E2 = 10·V(R2) con su − en tierra, C1 de 1 µF con V0 = 0 dato de su + a tierra, R3 en paralelo): el OP da el aviso CI_SALTA (no `SIM_LAZO_V`), el C va abierto en el OP y arranca en 0 V en 0⁺; con V0 vacía, sin aviso y el C arranca en 10·V(R2). Una L sin I0 en la salida de la EC: aviso L_SIN_I.
- **(d3) La ley de nodos de las ramas, una sola vez:** en el primer caso de [56], el emulador da I(E2) = 10 mA a 1e-12 (con la ley de nodos de la rama repetida, como en la tabla del §3.C de antes, saldría 5 mA); el (a) lo vigila en 2000 redes.
- **(e) Bode** de E1 → VCVS μ = 10 → RL: plano, H = 10 exacto (1e-12).
- **(f) La calculadora de CA con controladas:** la VCVS del (e) con una E fasor y μ `[?!]` → `CAL_NO_CA` con el texto de la ganancia (§3.I); con μ vacía → `SIM_FALTA`; con μ = 10 dato y V(RL) = 10∠0° V dato con E1 `[?!]`: E1 = 1∠0° V. Un dato de V(E2) con `CONV_PICO` en la ficha se guarda eficaz (÷√2), como el de una E fasor.
- **(e) Bode** de E1 → VCVS μ = 10 → RL: plano, H = 10 exacto (1e-12).

### 6.5 [58] Diodos: el punto de operación (pasos 3 y 4)
**Casos cerrados** (E en serie con R y el diodo; por `sim_op_red` y por `sim_op_cc_red`; por `[=]` en [65], paso 11):

| Diodo | E, R | I | V | Tol. |
|---|---|---|---|---|
| exponencial (1e-14, n 1) | 5 V, 1 kΩ | 4,3071121676219442 mA | 0,69288783237805583 V | 1e-12 |
| 1N4148 | 5 V, 1 kΩ | 4,3467715385002356 mA | 0,65322846149976443 V | 1e-12 |
| 1N4007 | 5 V, 1 kΩ | 4,375928266602544 mA | 0,62407173339745597 V | 1e-12 |
| 1N4007 | 10 V, 100 Ω | 92,303421120134247 mA | 0,76965788798657532 V | 1e-12 |
| exponencial | −5 V, 1 kΩ | −5,00999999499 pA | −4,99999999499 V | 1e-9 |
| exponencial | 100 V, 1 Ω | 99,047348503037482 A | 0,95265149696251774 V | 1e-12; ≤ 40 iteraciones |
| exponencial | 1 V, 1 MΩ | 539,51149881271225 nA | 0,46048850118728775 V | 1e-12 |
| exponencial | 0,3 V, 1 kΩ | 1,0898251756675721 nA | 0,29999891017482433 V | 1e-9 |
| ideal | 5 V, 1 kΩ | 4,999995000005 mA | 4,999995000005 µV | 1e-12 |
| ideal | −5 V, 1 kΩ | −4,999999995 pA | −4,999999995 V | 1e-9 |
| con Vγ 0,7 | 5 V, 1 kΩ | 4,2999957000043007 mA | 0,7000042999956993 V | 1e-12 |
| con Vγ 0,7 y rd 10 Ω | 5 V, 1 kΩ | 4,2574257425811881 mA | 0,74257425741881188 V | 1e-12 |
| 2 exponenciales en serie | 5 V, 1 kΩ | 3,6231693828557282 mA | 0,68841530857213592 V cada uno | 1e-12 |
| 10 exponenciales en serie | 10 V, 10 Ω | 206,95689299461996 mA | 0,79304310700538004 V cada uno | 1e-12 |

- **OR de dos ideales** (E1 = 5 V → D1 → n, E2 = 3 V → D2 → n, 1 kΩ de n a tierra): Vn = 4,999995000004998 V; I(D1) = 4,999995002004993 mA; I(D2) = −1,999995000004998 pA; V(D2) = −1,999995000004998 V (1e-9). Regiones: D1 conduce, D2 corte.
- **Zener** (E = 12 V, Rs = 220 Ω, cátodo arriba, RL):

| Zener | RL | Vo | \|I(Dz)\| |
|---|---|---|---|
| de libro, Vz 5,1, rz y Iz vacías | 1 kΩ | 5,1000212635184427 | 26,263518447761363 mA |
| el mismo | 100 Ω (sale de ruptura: corte) | 3,7499999997421875 | 3,7499999997421875 pA |
| el mismo | sin carga | 5,1000263635165244 | 31,363516529470344 mA |
| de libro, rz 7 Ω, Iz 49 mA | 1 kΩ | 4,9467926839619945 | 27,113240570756212 mA |
| 1N4733A | 1 kΩ | 4,9431873823062371 | 27,133233607210867 mA |
| 1N4733A | sin carga | 4,9783457050491179 | 31,916610431594919 mA |
| 1N4733A con E = 10 V / 14 V | 1 kΩ | 4,8767051158608403 / 5,006956672510536 | – |
| exponencial, Vz 5,1, Iz 5 mA, rz vacía (Rs = 0) | 1 kΩ | 5,1426692944195435 | 26,027015730946168 mA |

  y la del 1N4733A en −5,1 V: −49,000000005100754 mA (1e-9).
- **LED:** las Is de los 6 colores (rojo 1,0685493590266111e-16; naranja 2,237216788021277e-18; amarillo 3,2371648073427147e-19; verde 4,6840503101921716e-20; azul y blanco 1,1745197291847082e-18) y i(Vf) = 20 mA en todos (1e-12). Rojo con 5 V y 160 Ω: **20,000 mA** y 1,8 V; con 9 V y 330 Ω: 21,777894098247443 mA y 1,8132949475783439 V (nivel 8, 109 %); con 5 V y 1 kΩ: 3,375166452816769 mA (nivel 1); con 9 V y 360 Ω: 20 mA (1e-12).
- **Newton por dentro:** `nl_pnjlim` en los cuatro casos de SPICE3 (vold > 0 con arg > 0 y con arg ≤ 0; vold ≤ 0; sin límite); `nl_fetlim` y `nl_limvds` contra sus fórmulas en 50 pares; Vcrit = 0,730289720247 (1e-14), 0,741283172253 (1N4148), 0,71850042798 (1N4007); `nl_exp` continua con su derivada en 80; derivadas de la unión (con y sin Rs y ruptura) frente a diferencias centradas en 200 puntos al azar a 1e-6.
- **Criterios:** `residuo` ≤ 1e-12 en cada OP; `kcl_de` y `ley_de` (el arnés, por su cuenta con los Valores) ≤ 1e-10.
- **Homotopías:** los 10 diodos en serie con `sim_nl_topes(5, 0)` salen por gmin (`homotopia = 1`, aviso HOMOTOPIA) con la misma I a 1e-12; con `sim_nl_prueba(NLP_SIN_GMIN)`, por la rampa (`homotopia = 2`), igual; con `NLP_SIN_HOMOTOPIA` y topes de 1 iteración, `SIM_NO_CONVERGE` con `campo = NC_OP` y `elem` en un diodo. **Tope:** el peor caso de [14] (paso 10) gasta menos de `NL_OPS_OP`; con `sim_nl_topes` altos y el tope de operaciones bajado por el gancho `sim_nl_tope_ops`, `NC_TOPE`.
- **Errores del modelo (`SIM_MODELO`, no `SIM_FALTA`):** zener de libro con rz = 2 kΩ e Iz = 5 mA (rz·Iz = 10 V > Vz) → `campo = QZ_RZ`; zener exponencial con Vz 5,1 V, Iz 5 mA y rz = 1100 Ω (BV ≤ 0: el límite es 1025,17 Ω) → `QZ_RZ`, y con rz = 1000 Ω sí rompe (BV > 0); LED azul con If = 250 mA (Rs·If = 3,75 V > 3,2 V) → `QL_IF`, y con If = 200 mA vale; un diodo con variante 7 armado a mano → `campo = Q_VAL`. Los textos, en 4 líneas de 52 y sin «falta».
- **`SOL_NO_LINEAL` con un diodo** (el de [56], que aquí ya tiene motor): `elem` = el primero y un texto de 4 líneas de 52 como mucho que contiene «[I]». **CTL_I_NO** con una E controlada por la I de un diodo: `SIM_CTL_MAL` en el emulador y `SOL_CTL` en la tabla, con el mismo motivo.
- **Estado intacto** (paso 3 con `sim_op_cc_red`; el paso 8 añade `sim_punto_q` y el 9, `sim_cc_punto`): emular **a medias** (`sim_avanzar` con un tope de operaciones, sin acabar) un RLC con un interruptor (desde el paso 3) y un rectificador (desde el paso 5), copiar `*sim_red()` y `*sim_res()`, llamar a esas funciones sobre otras redes (con diodos por tramos y exponenciales, para que su Newton use `lu_op` y `lu_nl` y cambie tramos): `memcmp` = 0 con las copias; y **seguir emulando**: el registro y `*sim_res()` al acabar son bit a bit los de la misma emulación sin interrupciones (si una ruta de OP hubiera desalojado una entrada de la caché del transitorio, `M.ic` apuntaría a otra LU y saldría distinto).

### 6.6 [59] Transitorio por tramos (paso 5)
**Rectificadores con diodos con Vγ = 0,7 V** (60 Hz, C = 100 µF en paralelo con R = 1 kΩ, desde el C descargado, h = T/100 dato, 6 periodos; medidas en el último). La referencia es el régimen cerrado con Ron = 0 y GMIN = 0 (`n3.py`; la tabla de abajo). **Qué se compara y con qué tolerancia** (medido en la réplica del §3.G, `e4juez2/r1_rect.py`, a T/100 y T/200, con la historia BDF2 del diodo que conduce):
- **Cada muestra registrada del último periodo** frente a la cerrada **en su instante** (conducción: Vp·|sen θ| − Vd entre θ_on y θ_off; descarga: v_off·e^{−(θ − θ_off)/(ωRC)}, con v_off = Vp·sen θ_off − Vd): **5e-4 V**. En la réplica, el peor es 1,9e-4 V (media onda), 1,4e-4 (onda completa) y 3,2e-4 (puente: dos Ron en serie con el pico de corriente de carga).
- **La muestra más alta** («Vmax»): por debajo de Vp − Vd y a **5e-3 V** de ella. El pico no es una muestra: tras cada encendido la malla se reancla en θ_on y el máximo cae entre dos puntos. En la réplica: 1,3e-3 V (media onda), 3,5e-3 (media onda ideal a T/100), 7,6e-4 (onda completa), 2,7e-3 (puente). Una tolerancia de 2e-4 V (la de antes) no la cumple el algoritmo; la comparación que dice si el emulador acierta es la de cada muestra.
- **Vmin**: **2e-4 V** (el mínimo cae en θ_on, que es un evento y una muestra; en la réplica, 2e-6 V). **Rizado** (la muestra más alta menos Vmin): 5e-3 V, por lo mismo que Vmax.
- **La media** (la 'A' de `osc_medidas`: el trapecio sobre las muestras en [5T, 6T]): **1e-4 relativo** (en la réplica, ≤ 4,7e-5).
- **θ_on a 0,01°** y **θ_off a 0,02°** (en la réplica: θ_on a 1e-4°; θ_off a 0,002° en la media onda, 0,003° en la onda completa y 0,006° en el puente: el resto es el efecto de Ron frente a la cerrada con Ron = 0). **Sin la historia BDF2 del diodo que conduce (§3.B) el apagado sale a 0,19-0,21°**: el modo Ron·C = 0,1 µs (h/τ ≈ 1700) queda zumbando en el trapecio y corre el cruce por cero de la corriente. Esta comprobación es la que lo vigila.

| Montaje | Vmax | Vmin | Rizado | Media | θ_on / θ_off |
|---|---|---|---|---|---|
| media onda, 10 Vp | 9,3 | 7,98398262081831 | 1,31601737918169 | 8,64575304946355 | 60,2730373768404° / 91,4131115517566° |
| media onda ideal, 10 Vp | 10,0 | 8,58941315611351 | 1,41058684388649 | 9,29961706891035 | 59,1979208795881° / 91,5194614473286° |
| onda completa con dos fuentes en contrafase, 10 Vp | 9,3 | 8,64240954057463 | 0,657590459425368 | 8,98156622274838 | 69,1058598043297° / 91,4131115517566° |
| puente, 12 Vp (2Vγ) | 10,6 | 9,84803287246722 | 0,751967127532781 | 10,2354225047336 | 69,6088916670063° / 91,3422114404318° |

- Cambios de estado: 2 por periodo en la media onda y 4 en el puente (±1 por el primer ciclo); cada t* con |margen| < 1e-9·Vesc. Las 6 entradas de la caché bastan (`factorizaciones` ≤ 6 + las sueltas de Illinois).
- **La historia BDF2 del diodo:** `algebraica` da 1 para el C del rectificador mientras un diodo conduce (Ron·C < h) y 0 con los diodos en corte; con un diodo con Vγ y rd = 10 Ω (rd·C = 1 ms > h), 0 también conduciendo.
- **Recortador** (10 Vp a 1 kHz, R 1 kΩ, diodo con Vγ en serie con E2 = 3 V): máximo de la salida **3,7000062999936993 V** (1e-9).
- **Fijador** (5 Vp a 1 kHz, C 1 µF, diodo con Vγ de la salida a tierra, R 100 kΩ, 20 ms): mínimo del último periodo −9,278550321932634 V, máximo 0,700004099003878 V (Radau; 1e-3).
- **Balance:** `dif_balance` ≤ 1e-9 de la energía entregada; **trozos:** `sim_avanzar` con 1, 997 y sin tope da lo mismo bit a bit.
- (El comparador y la guarda de Zenón necesitan el AO: van en [61], paso 7.)

### 6.7 [60] Transitorio de los suaves (paso 6)
- **Media onda con filtro y 1N4007** (10 Vp, 60 Hz, 100 µF ∥ 1 kΩ, 6 periodos): con h = T/100 dato y con h = T/200 dato, **cada muestra del último periodo a 1e-3·Vp de la salida densa de Radau en su instante** (el error se define así, en las muestras, y no con el máximo muestreado: el pico cae entre muestras y el control del error local parte los pasos donde quiere, así que el máximo muestreado no tiene por qué mejorar al partir h; en la réplica del crítico, de T/100 a T/200 bajó solo 1,14 veces). La media del último periodo, a 1e-3 relativo de 8,66987970057439 (Radau). Como referencia, los extremos de la solución continua: máximo 9,307509246232415, mínimo 8,01694574349012. No se exige que el error baje en una proporción dada al partir h: el orden lo vigilan [45] y el oráculo.
- **Puente con 1N4007** (12 Vp, paso automático): cada muestra del último periodo a 1e-3·Vp de Radau; media 10,245198195320043 (1e-3 relativo); extremos de la continua: 10,584457725700444 y 9,882897896127597.
- **Media onda sin C** (10 Vp, 1N4007, 1 kΩ): pico de V(R) **9,3403009505190993 V** (1e-9: la muestra de t = T/4 es la solución de la ecuación algebraica).
- **Patada inductiva con diodo de rueda libre** (E 10 V, R 10 Ω, L 10 mH y un S que abre en 1 ms; 1N4148 en antiparalelo con la L): tras abrir, la I de la L baja por el diodo sin que V(L) cambie de signo entre dos pasos seguidos (sin oscilación del trapecio) y `res.nw.sub` > 0 (la subdivisión actuó).
- En todos: iteraciones por paso < 3 de media; `residuo` ≤ 1e-9 en cada paso; `dif_balance` ≤ 1e-6 de la entregada; trozos bit a bit.

### 6.8 [61] Amplificador operacional
- **Inversor** (R1 1 kΩ, Rf 10 kΩ, E1 1 V, ±15 V): ideal **−10**; real con A = 1e5: **−9,9989001209866915**; 2e5: −9,9994500302483363; **1e7: −9,9999890000121**; 1e8: −9,999998900000121 (1e-12): la diferencia con el ideal baja 10 veces de 1e7 a 1e8. Con A = 1e5 y E1 = 1,6 V satura: Vsal = −15 y V⁻ = 0,090909090909090909 (1e-12); lineal hasta |E1| = 1,500165.
- **No inversor** (1 kΩ, 9 kΩ): 10 ideal; A = 1e5: 9,999000099990001. **Seguidor** A = 1e5: 0,999990000099999. **Sumador** (10 kΩ ×3; 1 V y 2 V): −3 ideal; A = 1e5: −2,999910002699919.
- **200 redes lineales al azar con AO** (inversor, no inversor, sumador, diferencial, cadenas de dos; solo las que quedan dentro de ±15 V): ‖x(1e7) − x(ideal)‖ ≤ 1e-4 de la escala y ‖x(1e8) − x(ideal)‖ ≤ 0,12·‖x(1e7) − x(ideal)‖ + 1e-12 (orden 1 en 1/A); en el ideal, V⁺ = V⁻ a 1e-15 de la escala.
- **La regla** en 6 montajes: inversor, no inversor y seguidor, LIN vale; Schmitt (entrada en −, V⁺ = β·Vsal), no vale: `BIESTABLE` y empieza en +15 V; comparador sin realimentación: la LU con LIN es singular y va a la saturación del signo de Vd; astable con el C sin V0: vale en CC y no con el compañero del C. **La regla con la matriz de paso h:** en el astable, `cache_inest` solo se calcula en las entradas de paso h, y el arranque tras cada conmutación (hh = h/4) no cambia el veredicto: sin ganchos, 10 ms sin `NC_CONMUTA` y con 2 cambios de tramo por periodo (±1).
- **Integrador ideal** (R 10 kΩ, C 100 nF con V0 = 0 dato, E escalón de 0 a 1 V en t = 0): sin aviso CI_SALTA (el norador no entra en el segundo recorrido del §B: el C conserva su V0) y el AO en LIN en el OP; Vsal = −t/1 ms y satura en **t = 15 ms** (1e-9: la rampa es exacta en el trapecio y el margen es afín), luego −15 V.
- **Derivador ideal** (triangular ±1 V a 1 kHz, C 100 nF, R 1 kΩ): con la V0 del C **vacía** (el modelo 37), sin avisos, el AO en LIN en el OP y cuadrada de ±0,4 V: máximo y mínimo del último periodo **0,4 y −0,4** (1e-9). Con **V0 = 0 dato**: el segundo recorrido del §B da CI_SALTA (la triangular vale −1 V en 0⁻ y el nulador fija la masa virtual a 0) y **el OP sale con el AO en LIN** (sin el segundo recorrido, la LU sería singular y la regla lo saturaría); el último periodo, igual (1e-9). (Sin el nulador en `algebraica`, oscila: la prueba lo vigila.)
- **Astable** (R 10 kΩ, C 100 nF, R1 = R2 = 10 kΩ, ±15 V, **con tierra**, h = 10 µs dato, 10 ms): con la V0 del C **vacía** (el modelo 39), aviso `ARRANQUE`; con **V0 = 0 dato**, aviso `BIESTABLE` («U1 puede empezar en V+ o en V-: empieza en V+.»), la salida en +15 V ya en el OP. En los dos: el primer cambio en **RC·ln 2 = 0,69314718055994531 ms**; **T = 2RC·ln 3 = 2,1972245773362194 ms, f = 455,1196133134187 Hz** (1e-4); vC entre ±7,5 V (1e-4).
- **Comparador ideal** (senoidal 1 Vp a 1 kHz en +, 0,5 V en −, ±15 V; viene de [59]): media del último periodo **−5 V** (1e-9); flancos en t = T/12 y 5T/12 de cada periodo a **1e-11·T** (Illinois para con |fc| < 1e-12·Vesc: con Vesc = 30 V y la pendiente de Vd en el cruce, 5441 V/s, el error en t llega a 5,5e-12·T, así que 1e-12·T no está garantizado); frecuencia 1000 Hz (1e-9). Sin sondas, sus canales automáticos son «Vsal(U1)» y V(E1) (no I(E1)).
- **Zenón** (viene de [59]): con `sim_nl_prueba(NLP_INESTABLE, 1 ms)` un seguidor con 1 V de entrada da `SIM_NO_CONVERGE` con `campo = NC_CONMUTA` y t = 1 ms.
- **Errores:** la salida de un AO atada a una E ideal, `SIM_LAZO_V` con los dos en `lazo`; un A ideal de la salida a tierra, igual. **Un C con V0 = 0 dato de la salida del seguidor a tierra** (con 1 V de entrada): aviso CI_SALTA (no `SIM_LAZO_V`), Vsal = 1 V en el OP y el C arranca en 0 V en 0⁺ (lo carga el AO). **Sin referencia:** el astable sin tierra (no tiene fuentes) → `SIM_AO_REF` con `AOR_SIN_TIERRA` y `elem` = U1, en `[=]`, en el emulador y en la línea de estado («U1 sin referencia…»); con la salida atada a la tierra → `AOR_CORTO`. Un inversor sin tierra pero con E1: vale (la referencia es el − de E1). **CTL_3PATAS**: una E controlada por la V de U1 → `SIM_CTL_MAL` y `SOL_CTL` con ese motivo. **`SOL_NO_LINEAL`** con un AO (la tabla con `[?!]`), `elem` = U1. `SIM_MODELO` con V+ ≤ V− (`campo = QA_VP`).
- **Ley de nodos de la rama del AO** (§3.C): el inversor ideal (sin carga) da I(U1) = Vsal/Rf = −1 mA a 1e-12 y la I de R1 igual a la de Rf (por la entrada − no entra nada). Con la línea común en (a, b), la corriente de la salida iría a las filas de + y −, y ni Vsal ni I(U1) saldrían.

### 6.9 [62] Pequeña señal (CA y Bode) (paso 8; el emisor común, paso 13)
- **Diodo exponencial** (E 5 V + e alterna de 1 mV a 1 kHz, en serie, + R 1 kΩ + D): H = V(D)/e = **0,0059693197616019841** (1e-9); G = 0,16652327567247468 S (r_d = 6,005166520786224 Ω; n·VT/I = 6,0051665198701735: la diferencia es GMIN). **1N4148:** H = 0,010873519830456858; G = 0,090966540328458143 S.
- (El cruce con la derivada del barrido va en [63], paso 9.)
- **La linealización del Bode es suya** (§3.I): con el 1N4148, `bode_iniciar_red` y un trozo de `bode_avanzar`; luego un `ca_analiza` completo de **otro** circuito con un diodo (que pone su punto Q y al acabar `ca_pon_q(NULL)`); luego el resto de `bode_avanzar` y `bode_en(1 kHz)`: el registro es bit a bit el de un Bode sin interrupción y `bode_en(1 kHz)` = 0,010873519830456858 (1e-9). (Sin `B.lin`, las lecturas después del `ca_analiza` saldrían sin el diodo: H = 1.) Y `bode_iniciar_red` llamado directamente (como el arnés) hace el punto Q antes de `ca_apaga`: con una J de polarización en vez de la E, el Q es el mismo.
- **Estado intacto** con `sim_punto_q` (se añade a la prueba de [58]).
- **AO:** el inversor real (A = 1e5) da un Bode plano de −9,9989001209866915 (1e-12); un AO saturado en Q da H = 0 con el aviso SATURA.
- **Emisor común** (VCC 12 V, R1 47 kΩ, R2 10 kΩ, RC 2,2 kΩ, RE 470 Ω con CE 100 µF, Cin 10 µF desde la fuente de señal a la base, NPN por omisión; salida V(C)):

| f | H |
|---|---|
| 10 Hz | −2,2796025175134517 − j12,402598395470426 |
| 100 Hz | −54,471578424495574 − j90,315241479703668 |
| 1 kHz | −202,87069724233534 − j34,390096411599195 |
| 10 kHz | −208,6863050586752 − j3,5383875996049011 (46,391127366805791 dB) |

  (1e-9); en la banda media tiende a −gm·RC = −208,74676437389735. (Es del paso 13, con el BJT.)
- Con no lineales el aviso es PEQ_SENAL y no CC_FUERA.

### 6.10 [63] Barrido de CC (paso 9; las familias, pasos 13 y 14)
- **Divisor** (1 kΩ y 3 kΩ; E de 0 a 10 V): y = 0,75·x (1e-15); dy/dx = 0,75 (1e-12).
- **1N4148 con 1 kΩ** (E de 0 a 5 V): `barr_en(5)` = 4,3467715385002356 mA; **`barr_nivel(1 mA)` = 1,5847396946292637 V** (1e-9); dy/dx exacta frente a diferencias centradas (paso 1e-6) a 1e-7.
- **Cruce con la pequeña señal** (viene de [62]): dV(D)/dE del barrido en E = 5 V = la H del Bode del diodo exponencial (1e-9); dI/dE (1N4148) = 0,00098912648016954314.
- **La derivada con controladas por la I de lo barrido** (§3.J, parte (2)): E1 = 1 V sobre R1 (se barre de 500 Ω a 2 kΩ) y E2 = 5 kΩ·I(R1) sobre RL = 1 kΩ: V(RL) = 5000/R1, así que en R1 = 1 kΩ **dy/dx = −5e-3 V/Ω** (1e-12; sin la parte (2) saldría 0). J1 con R1 = 1 kΩ en paralelo (se barre de 0 a 2 mA) y J2 = 2·I(J1) sobre R2 = 1 kΩ: **dV(R2)/dJ1 = 2000 Ω** (1e-12; sin la (2), 0). Una E real barrida con una CCCS por su I: D = 1/r, frente a diferencias centradas a 1e-9.
- **Nivel de un dato con signo:** el inversor real con E1 de 0 a 2 V y el dato V = 12 V en la ficha de U1 (su Vsal, que es un canal): no hay cruce en +12 y `barr_nivel_dato` da **E1 = 1,200132** (1e-9) con `sg` = −1; el aviso dice «-12,00 V».
- **Una continuación por traza:** el Schmitt de abajo, `[C]` a la traza de la vuelta y `barr_en(0)`: **−15 V** en la vuelta y **+15 V** en la ida (cada uno desde su `cont_`; con una sola continuación, la de la traza activa, la vuelta leería la rama de la ida).
- **Canal automático del AO:** el inversor sin sondas tiene de canal 1 «Vsal(U1)», que vale Vsal (−4,9994500604933457 V en E1 = 0,5), no Vd.
- **Tope por punto:** con `sim_nl_tope_ops` bajado a 5000 operaciones, un punto que necesita partir el intervalo acaba en hueco (aviso HUECOS) y la curva sigue; ninguna llamada a `barr_avanzar(1)` gasta más que ese tope más un punto.
- **Estado intacto** con `sim_cc_punto` (se añade a la prueba de [58]).
- **Trazador del 1N4148:** V a 1 mA = 0,58473969462926372; a 10 mA = 0,69419404121982237; a 100 mA = 0,84965648028731514 (1e-9).
- **LED rojo** con E = 9 V, R de 10 Ω a 10 kΩ (logarítmico): **`barr_nivel(20 mA)` = 360 Ω** (1e-9); en R = 330 Ω, 21,777894098247443 mA.
- **Inversor real** (A = 1e5, E1 de −2 a 2 V): dy/dx(0) = −9,9989001209866915; y(0,5) = −4,9994500604933457; `barr_nivel(−12)` = 1,200132 (1e-9); satura desde |E1| = 1,500165.
- **Schmitt ideal** (±15 V, β = 0,5), ida y vuelta de −10 a 10 V: la ida salta de +15 a −15 entre x = 7,5 y el punto siguiente de la malla; la vuelta, entre −7,5 y el anterior; `salto` = 1 en esos dos puntos y el aviso SALTO.
- **Familia del BJT** (paso 13; NPN por omisión, VCE = 5 V): IB = 20 µA → IC = 2,0000003578488944 mA (VBE 0,73260251552082675); IB = 40 µA → 4,0000003542453266 mA (VBE 0,75053071359549415) (1e-9).
- **Familia del MOSFET** (paso 14; K 1 mA/V², Vt 2 V, VDS = 5 V): ID = 5 pA; 0,250000005; 1,000000005; 2,250000005; 4,000000005 y 6,250000005 mA (1e-9).
- **Trozos:** `barr_avanzar` con 1, 997 e ilimitadas operaciones da un `BarrReg` igual bit a bit; `barr_en` en un punto de la malla es la malla bit a bit; `barr_valores` da los mismos canales que `barr_en`.
- **Huecos:** con `sim_nl_prueba(NLP_FALLA_BARR, x)`, un hueco en x con el aviso HUECOS y la curva sigue.
- **Tope:** con 32 elementos, `t_est` ≤ 3,5 s (con la malla reducida).

### 6.11 [64] BJT y MOSFET (pasos 13 y 14)
- **Puntos del modelo** (NPN, Is 1e-15, β 100, βR 1): (vBE, vBC) = (0,7; −4,3): IC = 5,6702947265407799e-4, IB = 5,6702910825207799e-6; el PNP en (−0,7; 4,3) da los mismos con el signo cambiado, **bit a bit**; (0,7; 0,6): IC = 5,4328572891369173e-4, IB = 1,7542165402713912e-5; con VA = 100 en (0,7; −4,3): IC = 5,9141173979326034e-4. Derivadas del BJT (NPN y PNP) y del MOS (N, P e invertido) frente a diferencias centradas en 200 puntos a 1e-6; el MOS, continuo en vov = 0 y en vDS = vov.
- **Emisor común de Boylestad** (VCC 22 V, 39 kΩ/3,9 kΩ, RC 10 kΩ, RE 1,5 kΩ, β 140): VBE = 0,71015607662840807, IB = 5,9979952449108933 µA, IC = 0,83972087399969434 mA, VCE = 12,334212956136149 (1e-9); con VA = 100: VBE 0,7074386661541133, IC 0,84354456360751576 mA, VCE 12,291137805735711.
- **Emisor común de la biblioteca** (VCC 12 V, 47 kΩ/10 kΩ, RC 2,2 kΩ, RE 470 Ω, β 100): VB = 1,9029000966741065, VBE = 0,73789576485171988, IB = 24,541903169310483 µA, **IC = 2,4541907177293845 mA, VCE = 5,4357760891729673** (1e-9); gm = 0,094884892897226069 S, rπ = 1053,9085500462489 Ω; región ACTIVA.
- **Interruptor** (5 V por RB 4,7 kΩ a la base; LED rojo y 330 Ω a 9 V en el colector): VBE = 0,79559221925032602, VCE = 0,091424804456153236, **I(LED) = IC = 21,506917662571294 mA**, IB = 0,89455484696801574 mA, IC/IB = 24,042033571743936; región SATURACION.
- **PNP:** el emisor común de la biblioteca con VCC = −12 V y un PNP da las mismas tensiones y corrientes cambiadas de signo (1e-12).
- **MOSFET canal N** (K 1 mA/V², Vt 2 V): a) VGS = 4 V (una E), RD 2 kΩ, VDD 12 V: **ID = 4,000000004 mA, VDS = 3,999999992 V** (SATURACION); con VA = 50: ID 4,2758620719381688 mA, VDS 3,4482758561236623. b) VG = 6 V, RS 1 kΩ, RD 2 kΩ: **ID = 2,4384471883273663 mA**, VGS 3,5615528116726337, VDS 4,6846584350179011 (sin GMIN, (9 − √17)/2 mA = 2,4384471871911697). Puntos con VA = 50: (3; 0,5) 0,7575000005 mA; (3; 5) 1,100000005 mA; (4; −1) −5,100000001 mA; (1; 5) 5 pA. Canal P en (−4; −4): −4,000000004 mA.
- **MOSFET dibujado al revés** (el caso a) con D y S cambiados: la pata D a tierra, la E de 4 V entre G y tierra y RD de 12 V a la pata S): **ID = −4,000000004 mA, VDS = −3,999999992 V** (1e-9), por Newton **sin homotopía** (`homotopia = 0`) y en menos de 10 iteraciones. En la réplica (`e4juez2/r4_mos_inv.py`), con los límites de `mos1load` converge en 4; con los de antes (`limvds` siempre en el sentido directo) se queda en vDS = −0,5 V y no converge en 100. `nl_mos_lim` frente a `mos1load` en 50 pares con vDS anterior positivo y negativo.

### 6.12 [65] Interfaz de la Etapa 4 (por guion de teclas) y [66] biblioteca
**[65]:**
- **Ficha del diodo:** `[D][der]` pone D1 y abre su ficha con `ve("con V" S_GAMMA)`; `[der]` → «exponencial»; `[der]` → «1N4148» y `ve("Is 2,52 nA")` en el eco; `[catálogo]` → P_MODELO, `[I]` → P_BARRIDO con `ve("Curva I-V de D1")`; `[esc][esc]` vuelve a la ficha. «0 [.] 6 [enter]» en la fila Vγ (variante con Vγ) guarda 0,6.
- **Controlada:** paleta (`[catálogo]`, 4×`[ab]`, `[der]`, `[enter]`, flecha) pone E2 con la ficha en la fila x: `ve("falta")`; `[+]`: `ve("V(E1)")` y el píxel de E1 en C_RESALTA; `[izq]`: `ve("I(E1)")`; `[del]`: `ref = −1`; «5»: `ve("El control se elige con [+][-].")`; `aux` sigue vacía.
- **AO:** `[U]` con el cursor en (4,2): las 4 flechas (las que caben en C_CURSOR); `[→]` coloca U1 con las patas del §3.N.1, el cursor en (6,3) y la ficha abierta; `[der]`: `ve("real")` y las filas A, Ri, Ro; `[esc]`. `[W][←]` desde (6,3): `ve("Ahí está U1")`, el cursor no se mueve; `[→]` pone el cable. En (5,3): `[G]` y `[T]` no ponen nada; `ve("dentro de U1")`. `[U][←]` en (4,2): `ve("No cabe hacia")`, la marca roja en (4,2), la acción sigue. `[F]` con U1 elegido: espejo y `ve("Espejo")`.
- **`[=]` con E 9 V + R 330 Ω + LED rojo:** P_RESULTADO, `ui_res_nl()` = 1, `ve("Newton")`, `ve("109 %")`; el píxel del centro del triángulo de D1 vale `NL_COLOR[LED_ROJO].rgb` (nivel 8: `esq_mezcla(C_FONDO, col, 256)`); `[enter]`: `ve("Sin paso a paso")`; `[tab]`: `ve("Kirchhoff")` y la región «conduce».
- **«¿Qué R da 20 mA?»:** R1 con `[?!]` e I(D1) = 20 mA dato; `[=]`: `ve("barre con [I]")`; `[I]`: P_BARRIDO con el cursor en 360 Ω: `ve("360,0")`; `[tab]`: `ve("En el cursor: R1 = 360,0")`; `[esc]`; «[N] 2 0 [M] [enter]»: el mismo cruce; `[X]`, `[Y]`, `[L]`, `[+]` y `[-]` respetan sus topes; `[menu]`: `ve("Barre")`, `[der]` pasa a E1, `[=]`: `ve("frente a E1")`.
- **Menú:** `[menu]` 8×`[ab]` `[enter]` → el barrido; 9×`[ab]` → el punto de operación (`[P]` hace lo mismo); `[menu][arr][enter]` → Salir.
- **Paleta:** con 20 piezas (paso 11), `[catálogo]` con 4×`[ab]` y 3×`[der]`: `ve("Amp. op.")`; en el paso 13, 5×`[ab]`: `ve("Transistor")`; en el 14, 5×`[ab]` y `[der]`: `ve("MOSFET")`. La pieza 13 sigue siendo el vatímetro ([52b] pasa sin tocarla). **Iconos:** ningún píxel de un icono fuera de su celda de 20 × 14 (el arnés pinta cada icono sobre un fondo de otro color y mira el marco de alrededor).
- **Línea de estado:** `ve("Listo: [=] da el punto de operación")`; con E2 sin control, `ve("no tiene control")`; con E2 controlada por la I de una J controlada, `ve("no vale")`; con un astable sin tierra, `ve("sin referencia")`.
- **`[=]` con los casos cerrados de [58]** (vienen de allí: por la interfaz hacen falta las teclas del paso 11): el diodo exponencial con 5 V y 1 kΩ, el zener 1N4733A y el OR de dos ideales dan los mismos números que `sim_op_cc_red` (bit a bit).
- **Línea 4 de P_RESULTADO:** con `[P]` y la rampa de fuentes (los 10 diodos en serie con `sim_nl_topes(5, 0)` y `NLP_SIN_GMIN`): `ve("CC, C abiertos, L cables: Newton+rampa")` y la línea en 52 columnas. **Línea de LED** con 4 LED (D10 a D13, amarillo y naranja): `led_linea` deja « +N» y 52 columnas como mucho; en P_EMU_ESQ con «Media: ».
- **`SOL_SIM`** (un OP que no converge por `[=]`): el texto nombra al culpable del circuito del OP aunque la última emulación fuera de otro circuito (se emula antes otro con un D1 distinto).
- **Ganancias en la tabla:** una CCVS con r `[?!]`: `ve("r(E2)")` y la unidad Ω; una VCCS: `ve("g(J2)")` y S.
- **Bode con un diodo:** `[B]` en el circuito del §6.9 → P_BODE con `ve("Peque" S_nn "a se" S_nn "al")`.
- **Textos:** todas las cadenas nuevas en 52 columnas (una tabla de constantes probada con `lineas()`), las páginas Modelo en 13 líneas, los mensajes en 4, y 0 desbordes.
- **Un elemento de cada tipo** (§8.1, riesgo 8): en el paso 11, de cada tipo que está en la paleta (diodo, zener, LED, E y J controladas, AO); en el 13, también el BJT; en el 14, todos: se dibuja, se guarda, se resuelve, se emula, se barre y se analiza en CA.

**[66] Biblioteca (cada modelo, además: válido, se resuelve sin error y en menos de 2 s estimados; los de `ca` 3 y 4, en menos de 0,5 s):**

| # | Título (≤ 44) | ca | Circuito | Clave | Tol. |
|---|---|---|---|---|---|
| 23 | Diodo en directa: su punto de operación | 3 | E 5 V, R 1 kΩ, 1N4148 | 'I' D1 = 4,3467715385002356 mA | 1e-9 |
| 24 | La curva del diodo: ¿qué E da 1 mA? | 4 | el mismo; barre E1 de 0 a 5 V | 'x' (t = 1 mA) = 1,5847396946292637 V | 1e-9 |
| 25 | Rectificador de media onda | 0 | senoidal 10 Vp a 60 Hz, 1N4007, R 1 kΩ | 'M' V(R1) = 9,3403009505190993 V | 1e-9 |
| 26 | Media onda con filtro: el rizado | 0 | + C 100 µF ∥ R; 0,1 s | 'A' = 8,66987970057439 V ('U' − 'u' = 1,2906, 1e-2) | 1e-3 |
| 27 | Onda completa con dos fuentes | 0 | E1 y E2 10 Vp a 0° y 180°, 2 diodos con Vγ, C 100 µF, R 1 kΩ | 'A' = 8,98156622274838 V | 1e-4 |
| 28 | Puente rectificador con filtro | 0 | 12 Vp flotante, 4 × 1N4007, C 100 µF, R 1 kΩ | 'A' = 10,245198195320043 V | 1e-3 |
| 29 | Recortador polarizado | 0 | 10 Vp a 1 kHz, R 1 kΩ, diodo con Vγ y E2 3 V | 'M' = 3,7000062999936993 V | 1e-9 |
| 30 | Fijador (sujetador) negativo | 0 | 5 Vp a 1 kHz, C 1 µF, diodo con Vγ a tierra, R 100 kΩ; 20 ms | 'u' = −9,278550321932634 V | 1e-3 |
| 31 | Regulador zener | 3 | E 12 V, Rs 220 Ω, 1N4733A, RL 1 kΩ | 'I' Dz = 27,133233607210867 mA | 1e-9 |
| 32 | LED: ¿qué R para 20 mA? | 4 | LED rojo, E 9 V; barre R1 de 10 Ω a 10 kΩ (log) | 'x' (t = 20 mA) = 360 Ω | 1e-9 |
| 33 | AO inversor: la transferencia | 4 | AO real A 1e5, R1 1 kΩ, Rf 10 kΩ; barre E1 de −2 a 2 V | 'g' (t = 0) = −9,9989001209866915 | 1e-9 |
| 34 | AO no inversor | 3 | ideal, E1 1 V, R1 1 kΩ, R2 9 kΩ, V1 en la salida | 'L' V1 = 10 V | 1e-12 |
| 35 | Sumador inversor | 3 | ideal, 1 V y 2 V, 10 kΩ ×3, V1 en la salida | 'L' V1 = −3 V | 1e-12 |
| 36 | Integrador con AO | 0 | cuadrada ±1 V a 1 kHz, R 10 kΩ, C 100 nF (V0 = 0), ideal | 'P' = 0,5 V | 1e-9 |
| 37 | Derivador con AO | 0 | triangular ±1 V a 1 kHz, C 100 nF (V0 vacía), R 1 kΩ, ideal, tierra | 'U' = 0,4 V | 1e-9 |
| 38 | Comparador | 0 | ideal; senoidal 1 Vp a 1 kHz en +, 0,5 V en − | 'A' = −5 V ('F' = 1000 Hz, 1e-9) | 1e-9 |
| 39 | Astable con AO | 0 | ideal ±15 V; R1 = R2 = 10 kΩ, R 10 kΩ, C 100 nF (V0 vacía); **tierra**; paso 10 µs, 10 ms | 'F' = 455,1196133134187 Hz | 1e-4 |
| 40 | Amplificador con fuente controlada | 3 | 10 mV, Rs 1 kΩ, Rin 9 kΩ, E2 = 100·V(Rin), Rsal 100 Ω, RL 900 Ω | 'L' V1 = 0,81 V | 1e-12 |
| 41 | El transistor como interruptor | 3 | NPN; 5 V por RB 4,7 kΩ; LED rojo y 330 Ω a 9 V | 'I' LED = 21,506917662571294 mA | 1e-9 |
| 42 | Emisor común: el punto Q | 3 | NPN; VCC 12 V, 47 kΩ/10 kΩ, RC 2,2 kΩ, RE 470 Ω; A1 y V1 | 'L' A1 = 2,4541907177293845 mA (V1 = 5,4357760891729673 V) | 1e-9 |

- Medidas nuevas de `medida_modelo`: 'U' y 'u' (máximo y mínimo del **último** periodo); 'I' (|I| de `clave_el` en el punto de operación), 'V' (su V), 'L' (la lectura); 'x', 'y', 'g' del barrido (§2.13).
- Los 20 enunciados caben en 3 líneas de 52 y los «qué ver» en 2; por teclas, abrir las lecciones 12 y 13 desde Aprende y, en los modelos 24 y 32, `[enter]` lleva a P_BARRIDO con el cursor ya en la respuesta; en los de `ca` 3, `[enter]` lleva a P_RESULTADO (no a P_CA) y el pie dice «[enter] calcular».
- Todos los AO de la biblioteca llevan tierra (`red_ref_propia` = 1). En el paso 12, [66] recorre los modelos 23 a 40 (la lección 13 con 8); en el 13, los 20.

### 6.13 Tiempos, banco y oráculo
- **[14]** (paso 10): se añaden (estimados en la calculadora) el punto de operación del peor caso sin transistores (32 elementos con 10 diodos exponenciales y 2 AO, 20 nodos: ≤ 0,5 s y por debajo de `NL_OPS_OP`), el puente con filtro con 1N4007, 5 periodos (**≤ 2 s**, el presupuesto de la hoja de ruta), el barrido de 281 puntos de un diodo (≤ 1 s) y el astable de la biblioteca (≤ 2 s). **En el paso 13** se añaden el peor caso con 8 diodos, 2 AO y 2 BJT (≤ 0,5 s) y la familia de un BJT (≤ 3 s).
- **Banco:** `BN_EXP` (una `exp()`) y `BN_NL` (un paso del puente con 1N4007: estampados, LU de n = 5, sustitución y 4 `exp`). Se corren en la calculadora real antes de cerrar la etapa y recalibran `NL_OPS_EXP` y `NS_ITER`.
- **[67] Oráculo no lineal** (paso 15; `herramientas/oraculo_nl.py`): la exportación pasa a **EFKNET 3** (EFKNET 2 más `dc` o `tran <tfin> <h>` y las líneas `D <nom> <a> <c> <Is> <n> <Rs>`, `DZ <nom> <a> <c> <Is> <n> <Rs> <BV> <Ibv>`, `DT <nom> <a> <c> <Vg> <Ron> <Vz0> <Rz>`, `AO <nom> <p> <m> <o> <ref> <A> <V+> <V-> <Ri> <Ro>`, `EV|EI|JV|JI <nom> <a> <b> <ctrl> <k>`, `Q <nom> <c> <b> <e> NPN|PNP <Is> <bf> <br> <VA>`, `M <nom> <d> <g> <s> N|P <Vt> <K> <VA>`). El oráculo tiene su propio MNA no lineal: en CC, `scipy.optimize.root` (hybr, xtol 1e-14) desde x = 0, desde la solución de ElectroFock perturbada 1e-3 y con su propia rampa de fuentes; pasa si la de ElectroFock coincide con una de las suyas a 1e-9 de la escala **y** su residuo en las ecuaciones del oráculo es ≤ 1e-10 (así valen también los circuitos con varias soluciones). En el tiempo, Radau (rtol 1e-10) con el algebraico resuelto en cada llamada y partido en los flancos; pasa con el peor error ≤ 5e-3 de la escala y el 90 % de los instantes ≤ 1e-3. 200 redes de CC y 60 en el tiempo, sin diodos ideales ni con Vγ (esos, por los casos cerrados). Resultado «N de N iguales».

---

## 7. Orden de implementación

Cada paso termina con el arnés en 0 fallos, `-Wconversion` sin avisos, `./build_host.sh arm` y `asan` limpios, `sim_fuera()` = 0 y `luc_fuera()` = 0, y **las BMP de las Etapas 0-3 idénticas** salvo las del §6.1 (la paleta en los pasos 11, 13 y 14; el menú en el 11; las lecciones y la Ayuda en el 12 y el 14). Antes de cada paso se vuelve a leer el código (§0). **Regla de este orden:** cada comprobación entra en el paso en que existe todo lo que usa (el tipo, su motor, su tecla, su pantalla, su modelo de la biblioteca); nada se prueba antes ni se activa en la interfaz antes de tener su modelo.

0. **Punto de partida:** el arnés en 0 fallos (hoy lo está: 311 709 comprobaciones, con [54a]-[54c] de la revisión de la Etapa 3), releer el código y guardar las BMP de referencia de las Etapas 0-3 (`bmp_ref_e4`). Nada de la Etapa 4 empieza antes.
1. **Modelo sin nada que se vea:** tipos (los 8, con **tecla 0** y fuera de la paleta), `Elemento.xc/yc`, `MAX_NODOS` = 70, circuito.c (§3.N: huellas, orientación, cabe, seg_libre, grado, válido con `variante < nvar`, `control_sig` con −1, `ctl_motivo`, etapa4, no_lineal, busca_valor), red.c (con `red_ref_propia`), archivo (versión 4); en el arnés, `circuito_sano`, `mismos()` y el lector de la Etapa 3. Prueba **[55]** entera (no necesita el motor). [8] y [43] byte a byte.
2. **Controladas en los tres motores y en la tabla:** sim.c (`#include "lu.h"`, `el_de`, `valida`, `con_rama`, `puertos` y `analiza` por puertos con CI_SALTA y L_SIN_I para los lazos con EC, **la ley de nodos común con el puerto 0** en `ensambla`, `termino`, `valores_de`, `termina`), ca.c (la misma línea común en `ensambla_en`, `SimMapa`, `sim_estampa_lin` con EC y JC, `ca_b_fuentes`, `ca_vi`), calca.c (la ganancia `[?!]` y vacía), resuelve.c (§3.O: `circ_ctl_motivo`, `SOL_CTL` con motivo, resuelve.c 111 con `circ_v_convenio`), proced.c, Thévenin; `nombre_mag`/`unidad_q` con la fila de la variante; esquema.c 829 y ui.c 3440 con `circ_v_convenio`. Pruebas **[56]** (sin `SOL_NO_LINEAL`) y **[57]** (sin CTL_I_NO por un diodo ni CTL_3PATAS).
3. **`nolin.c` y el diodo por tramos:** tramos, `NlEst`, `SimNewton`, `lu_op` con `op_clave` (sin la caché del transitorio), Newton con Katzenelson en el OP, `residuo` y `residuo_acum`, `sim_op_cc_red`, `SOL_NO_LINEAL`, `SIM_MODELO` (el zener de libro; la variante), regiones, `Valores.V2/I2/reg`. Prueba **[58]** (los por tramos, `SOL_NO_LINEAL` y CTL_I_NO con un diodo, «Estado intacto» con `sim_op_cc_red` sobre una emulación lineal a medias).
4. **La unión exponencial:** límites, criterio doble, pulido, gmin, rampa, tope de operaciones (`NL_OPS_OP`, `sim_nl_tope_ops`), diagnósticos; LED y zener (con sus `SIM_MODELO`). **[58]** entera.
5. **Transitorio por tramos:** la caché con clave de tramos (solo del transitorio), congelado, Illinois, `evento_nl`, Katzenelson del primer medio paso, Zenón, **`algebraica` con el diodo que conduce (R·C < h)**. **[59]** (sin el comparador ni Zenón); «Estado intacto» también con un rectificador a medias.
6. **Transitorio de los suaves:** Newton por paso, predictor, subdivisión, error local y Δi, `registra` con dos puertos. **[60]**.
7. **AO:** estampados (la rama en (nref, c)), puertos de salida y entrada, **el segundo recorrido del §B** (el nulador y los C con V0), `SIM_AO_REF`, la regla (con la matriz de paso h en el transitorio), biestable, arranque, `algebraica` con el nulador, `sim_canales` con «Vsal(U1)». **[61]** (con redes armadas a mano en el arnés: `sr_el` gana c y nref), que recibe el comparador y Zenón de [59], `SOL_NO_LINEAL` y `SIM_MODELO` del AO, y CTL_3PATAS.
8. **Pequeña señal:** `sim_punto_q`, `sim_estampa_lin` (D y AO), ca.c (`ca_punto_q`, `ca_pon_q`) y bode.c (`B.lin` en `bode_iniciar_red`). **[62]** (diodo y AO; la linealización propia del Bode) y «Estado intacto» con `sim_punto_q`.
9. **El motor del barrido:** barrido.c (continuación por traza, `barr_nivel_dato`, `sim_cc_deriva` con las dos partes, tope por punto). **[63]** (sin las familias; con el cruce de la pequeña señal que venía de [62]) y «Estado intacto» con `sim_cc_punto`.
10. **Paso y ventana con no lineales; banco:** `elige_paso` y `modos_de` con tramos (`tramos_top`), `BN_EXP`, `BN_NL`. **[14]** (diodos y AO).
11. **Interfaz:** ahora sí, las teclas **D, U, I y P** (tecla ≠ 0 en diodo y AO), **la paleta de 20** (hasta «Amp. op.»), la ficha (Q_CTRL con `circ_ctl_motivo`, eco, pie), colocar y dibujar los 3 patas (esquema.c, `Local`, la caja, la marca) **con el símbolo y el icono del AO**, el LED encendido (`led_linea`), `igual`, `punto_op` (`SOL_SIM` sin SimRed), `[P]`, `[I]`, P_RESULTADO (línea 4 nueva) y P_TABLA con `res_nl`, P_BARRIDO (`curva.c`), `AJ_BARRIDO`, `TR_BARRIDO`, P_EMU_ESQ y P_EMU_TABLA, P_MODELO con el trazador (diodos), menú de 11, línea de estado (AO sin referencia, `SIM_MODELO`) y mensajes; [54b] 13 y [54c] R30 reescritas con `PAL_N`. **[65]** (con los tipos de la paleta; los casos por `[=]` que venían de [58]), el mono de 29 pantallas y las BMP del §6.1.
12. **Biblioteca, Ayuda y páginas Modelo:** modelos 23 a 40, la lección 12 (10 modelos) y la 13 (8: del 33 al 40), su teoría, la Ayuda sin la sección TRANSISTORES; ui.c 1423 y 4543 (`ca` 3 y 4); [53] con `ca == 1 || ca == 2`. **[66]** (sin 41 y 42), [39] filtrado y [53].
13. **BJT:** `nl_bjt`, límites, dos puertos, puertos B–E y B–C, regiones, ficha, **símbolo e icono**, **la tecla [Q] y la pieza 21 de la paleta**, trazador (familia), modelos 41 y 42 (la lección 13 llega a 10), el Bode del emisor común, la teoría 13.2 del BJT. **[64]** (BJT), **[62]** (emisor común), **[63]** (familia del BJT), **[14]** (el peor caso con BJT y la familia), **[65]** (con el BJT) y **[66]** entera.
14. **MOSFET:** `nl_mos`, `nl_mos_lim` (el modo inverso de `mos1load`), simetría, ficha, **símbolo e icono**, **la tecla [M] y la pieza 22**, familia; la sección TRANSISTORES de la Ayuda y la línea «[Q] BJT [M] MOSFET» de «Dibujar». **[64]** entera (con el MOSFET invertido), **[63]** (familia del MOS) y **[65]** con todos los tipos.
15. **Cierre:** el oráculo no lineal **[67]**; DESIGN.md §19, README y HOJA_DE_RUTA_CA.md; el banco en la calculadora real (`BN_EXP`, `BN_NL`, los colores del LED, la legibilidad de los símbolos de 3 patas y del rombo).

---

## 8. Riesgos y presupuesto

### 8.1 Riesgos y mitigación
1. **Nodos que cuelgan de uniones en corte** (el Newton no converge por redondeo): el criterio del residuo (§3.D). Es el hallazgo del prototipo del motor.
2. **Zenón** (conmutaciones simultáneas en el umbral): el primer medio paso tras un evento decide con Katzenelson; la regla del AO se evalúa siempre con la matriz de paso h (con la del arranque podría decir otra cosa y el AO iría y vendría en cada evento); la guarda de 128 eventos en el mismo instante; el gancho del arnés la prueba.
3. **Oscilación del trapecio** con suaves rígidos (un diodo que libera una L): la subdivisión por Δi y el arranque de Euler tras cada evento. Con los por tramos, un C cuyo modo con el diodo que conduce es mucho más rápido que h (Ron·C) toma la historia BDF2 (§3.B); sin eso, el apagado del rectificador se corría 0,2°. *Queda sin el detector del motor:* si la patada inductiva de [60] u otra prueba lo muestra, se añade en la Etapa 5.
4. **Varias soluciones:** resuelto para el AO (§F: regla, BIESTABLE, ARRANQUE). La regla se aplica AO por AO: con varios AO en un lazo cruzado es heurística (aviso y consejo de usar el real). Los biestables de transistores dan la solución de la homotopía (Etapa 5: los modos del punto linealizado).
5. **El costo de `exp()`** en la Nspire sin FPU: `BN_EXP` primero. Con 100 operaciones por `exp` en vez de 30, el puente exponencial sigue por debajo de 1 s. La Rs condensada deja una sola `exp` por unión e iteración; en inversa profunda, ninguna.
6. **Tamaño:** con 31 elementos aislados y un AO se pasa de 64 incógnitas: `SIM_GRANDE` con su mensaje (`LU_MAX` sigue en 64).
7. **Regresión:** todo lo nuevo va detrás de `NL.n` salvo los `case` de las controladas; el mapa de una red sin tipos nuevos es el de hoy; el paso 1 se prueba con las BMP y los archivos byte a byte.
8. **Los `default` de sim.c, ca.c, esquema.c y resuelve.c** tratarían un tipo nuevo como otra cosa (`valores_de` leería `x[−1]`): cada tipo lleva su `case` en todos (§5); un circuito con un elemento de cada tipo tiene que dibujarse, guardarse, resolverse, emularse, barrerse y analizarse en CA en [65].
9. **`Q_CTRL` en `circ_mag`** escribiría en `aux`: `mag_campo` la atiende; [65] comprueba que `aux` queda vacía.
10. **Límite conocido:** un C directo sobre la salida de un AO o de una EC que conmuta da un salto que no se anota como `SimSuceso` (el arranque lo resuelve igual).
11. **Ron = 1 mΩ:** un diodo ideal sobre una E ideal en directa da kA (la verdad del modelo; sin aviso). La página Modelo lo dice; el LED avisa con QUEMA.
12. **Los números del catálogo** (1N4148, 1N4007, 1N4733A, los colores del LED) salen de modelos publicados de LTspice y de tablas de libro (el verde de GaP a 2,2 V; uno moderno de InGaN va a unos 3 V): confirmarlos con el usuario antes de cerrar.
13. **Decisiones que el usuario debería confirmar:** el diodo sale «con Vγ» (el del libro) y no el 1N4148; la salida del AO respecto a la tierra (su alimentación implícita, sin patas de alimentación); ±15 V por omisión; el AO real sin polo ni slew; las sondas no van en las patas; el PNP y el canal P con los signos del NPN (VCE e IC negativas en activa); `[=]` con alterna y un no lineal emula (no fasores); lo que queda fuera (D25).
14. **Pantallas:** con P_N = 29 quedan 3 para la Etapa 5.
15. **Llamadas que bloquean:** el punto de operación (`[=]`, `[P]`, el arranque del emulador, el punto Q, el primer punto del barrido y cada punto del barrido) no va por trozos: el tope de `NL_OPS_OP` = 1,5 M operaciones (~0,95 s) lo acota, con `NC_TOPE` (o un hueco, en el barrido) si se pasa. Si el banco dice que un caso real lo necesita, la Etapa 5 lo parte en trozos.
16. **Estado compartido entre análisis:** las rutas del OP no tocan `res_`, `M` ni la caché del transitorio (§2.6), y la linealización de la CA la pone quien ensambla (`ca_pon_q`, §3.I). «Estado intacto» ([58], [62], [63]) y la del Bode interrumpido lo vigilan.

### 8.2 Presupuesto en la calculadora (0,63 µs por operación; `exp` = 30 operaciones hasta medir `BN_EXP`; `NS_ITER` ≈ 15 µs fijos por iteración y `NS_PASO` = 20 µs por paso)

| Operación | Operaciones | Tiempo |
|---|---|---|
| Punto de operación de un diodo con R (6 iteraciones, n = 2) | ~600 | < 1 ms |
| Punto de operación del emisor común (n = 5, ~8 iteraciones) | ~5 k | ~5 ms |
| Punto de operación del peor caso (32 elementos, 8 uniones, 20 nodos, con homotopía) | tope de 1,5 M | ≤ 0,95 s (bloquea: sin barra ni `[esc]`) |
| Puente por tramos, 5 periodos a T/100 (~750 sustituciones de ~160 op. y ~100 LU pequeñas) | ~0,15 M | ~0,1 s |
| Puente con 1N4007, 5 periodos (~1200 iteraciones de ~330 op.; 600 registros) | ~0,5 M | ~0,35 s (con `exp` de 100 op., ~0,7 s) |
| Emisor común en el tiempo, 5 periodos de 1 kHz (n = 11) | ~0,4 M | ~0,3 s |
| Astable ideal (1000 pasos de 10 µs, ~10 cambios) | ~0,1 M | < 0,1 s |
| Barrido de 281 puntos de un diodo (~3 iteraciones por punto) | ~0,2 M | ~0,15 s |
| Familia de un BJT (6 × 281) | ~1,5 M | ~1 s |
| Bode del emisor común (277 frecuencias + 1 OP) | ≈ el de hoy | ~0,1 s |
| Dibujo de P_BARRIDO (281 × 2 líneas y retícula) | enteros | ~12 ms + 4,3 ms de volcado |

| Memoria | Tamaño |
|---|---|
| Caché de LU de 8 entradas (~70 KB cada una) y sus 8 bases sin factorizar (32 KB) | ~0,8 MB |
| `lu_nl` (la del Newton), `lu_suelta` y `lu_base_op` | ~175 KB |
| `BarrReg` (~19 KB) y `cont_`, la continuación de cada punto de cada traza (6 × 281 × 1,06 KB) | ~1,8 MB |
| `Curva`, `BarrRes`, el `Circuito` barrido | ~12 KB |
| `SimEl` crece 64 B: cada `SimRed` +2 KB (6 copias); `B.lin` del Bode y el `SimLin` de ca.c (~2,5 KB cada uno) | ~17 KB |
| **Total nuevo** | **~2,8 MB** de 64 MB; el `.tns` crece unos 60-80 KB |

---

## 9. Revisión tras la crítica: qué cambió y dónde

Los 32 hallazgos son reales; los comprobé contra el código (sim.c 488 y ca.c 144, la línea común de la ley de nodos; sim.c 865-991, `prepara_lu` y `cache_de`; red.c 190-223, `red_cierra`; circuito.c 157, la V0 de todo C nuevo; resuelve.c 111, esquema.c 829 y ui.c 3440, `->fuente`; ui.c 1268, el ÷√2 solo con `CF_AMPL`; calca.c, el `default: break` de las incógnitas; host_test 7268 y 7327, [53]; bode.c 352-386, `ca_apaga` en `bode_iniciar_red`; sim.c 1012, `canal_val`; ui.c 333-355, `nombre_mag` y `unidad_q`) y con réplicas en Python donde había números. En tres puntos no tomé el arreglo tal como venía (marcados con ≠).

| # | Hallazgo | Arreglo | Dónde |
|---|---|---|---|
| 1 | La ley de nodos común pondría la corriente del AO en + y −; la EC la contaba dos veces | La línea común usa el puerto 0 ((nref, c) en el AO); la tabla ya no repite la ley de nodos | §0, §3.C, §5; pruebas [57] (d3) y [61] |
| 2 ≠ | El OP usaba la caché del transitorio (matriz del modo 1, clave que choca con h = 1 s) y escribía en `res_` | **No** meto el modo en la clave ni enseño a `prepara_lu` el modo 0: la ruta del OP **no usa la caché** (`lu_op` con `op_clave` y `lu_nl`); el estado de Newton va en un `NlEst` y los contadores en un `SimNewton` que pasa quien llama; así tampoco se desaloja una entrada de una emulación a medias | §0, D12, §2.6, §3.D, §3.E, §3.I; «Estado intacto» con una emulación interrumpida y reanudada ([58], [62], [63]) |
| 3 ≠ | Un C con V0 en la salida de un AO o EC daba `SIM_LAZO_V`; el derivador salía singular y el AO saturado | Primer recorrido: si el lazo con una EC o un AO lo cierra un C con V0, CI_SALTA (y una L, L_SIN_I). Segundo recorrido (modo 0) con las ramas que **imponen tensión** y el nulador: cuadra → `SU_I0`, no → CI_SALTA. **Distinto de lo propuesto:** el norador (la salida del AO ideal) no entra en el segundo recorrido, porque si entrara el integrador perdería su V0 sin motivo (C, norador y nulador formarían lazo). **No adopto** «probar sin las V0 antes de culpar al AO»: el análisis estructural ya lo resuelve y el reintento escondería un comparador real | §0, §3.B, §3.F; [57] (d2), [61] |
| 4 | Sin tierra ni fuente, la referencia del AO era «el primer nodo» (hasta la salida) | `SIM_AO_REF` (sin tierra / en corto), `red_ref_propia`, aviso al colocar y línea de estado; la biblioteca pone tierra | D7, §2.4, §2.6, §3.A, §3.M, §4.1, §4.13; [61], [65], [66] |
| 5 | El Bode perdía los no lineales tras otro análisis de CA; `ca_apaga` está en `bode_iniciar_red` | `B.lin` propio calculado en `bode_iniciar_red` antes de `ca_apaga`; `ca_pon_q` antes de cada `ca_ensambla` | §2.9, §3.I, §5; [62] |
| 6 | [53] tomaba los modelos de `ca` 3 y 4 | Filtro `ca == 1 \|\| ca == 2`; ui.c 1423 y 4543 despachan 3 y 4 | §0, §4.13, §5, §6.1 |
| 7 | Las tolerancias del [59] no las podía dar el algoritmo | Cada muestra frente a la cerrada en su instante (5e-4 V); Vmax y rizado a 5e-3 V (el crítico proponía 2e-3; en mi réplica la media onda ideal a T/100 da 3,5e-3); `algebraica` con el diodo que conduce y R·C < h (apagado a 0,002-0,006° en vez de 0,19-0,21°); θ_off a 0,02° | §0, D13, §3.B, §6.6; `e4juez2/r1_rect.py` |
| 8 | El orden de implementación dejaba pruebas antes de lo que prueban | Cada comprobación en su paso; Q y M con tecla 0 y fuera de la paleta hasta los pasos 13 y 14; la paleta crece 20 → 21 → 22; [54b]/[54c] reescritas una vez con `PAL_N`; la lección 13 con 8 modelos hasta el paso 13; los símbolos del BJT y del MOS en sus pasos | §4.1, §4.3, §4.13, §6 (encabezados), §6.1, §7 |
| 9 | El MOS sin el modo inverso de `mos1load` no convergía con vDS < −0,5 V | `nl_mos_lim` como SPICE3; prueba del MOSFET invertido (converge en 4 iteraciones en la réplica; sin el arreglo, no en 100) | §2.7, §3.D; [64]; `e4juez2/r4_mos_inv.py` |
| 10 | `sim_cc_deriva` omitía las controladas por la I de lo barrido | La parte (2) de r: CCVS y CCCS con D = Σ ∂c_i/∂p·x_i + ∂c0/∂p | §3.J; [63] (−5e-3 V/Ω y 2000 Ω, que sin la parte (2) darían 0) |
| 11 | `->fuente` daba el convenio equivocado a las controladas | `circ_v_convenio(tipo) == −1` en resuelve.c 111, esquema.c 829 y ui.c 3440 | §0, §3.O, §5; [56] |
| 12 | La V y la I de las controladas sin `FA` | Con `FA` (en los no lineales sigue sin él) | §2.2; [57] (f) |
| 13 | calca.c con una ganancia `[?!]` | `con_no_ca` con texto propio; vacía → `SIM_FALTA`; `el_de` con `tolera` nunca inventa la ganancia | §3.A, §3.I, §5; [57] (f) |
| 14 | La tabla admitía controles que el emulador rechaza | `circ_ctl_motivo`, la misma regla en los dos; `SOL_CTL` con `Solucion.motivo` | D8, D15, §2.3, §2.8, §3.O; [56], [57] (a) |
| 15 | Una sola continuación, la de la traza activa | `cont_[recorrido][punto]` (~1,8 MB) | §2.10, §3.J, §8.2; [63] |
| 16 | «Vsal del primer AO» salía Vd; `sim_canales` no miraba los tipos nuevos | CANAL_NODO de c con el nombre «Vsal(U1)» en el barrido y en el emulador | §3.J, §4.11; [61], [63] |
| 17 ≠ | «3,2 s con barra y `[esc]`» en llamadas que bloquean | Tope bajado a 1,5 M (~0,95 s) y sin barra (de las dos opciones propuestas, esta); además, **tope por punto del barrido** (un punto con 256 trozos podía bloquear un minuto: no lo había visto el crítico) | D12, §2.7, §3.E, §3.J, §8.1 (15), §8.2 |
| 18 | [57] (b) con controladas al azar inestables | Filtro por los modos (Re s ≤ −0,02·\|s\|, ventana ≥ 20·τ_máx); 150 comparadas | §6.4 |
| 19 | Validación incompleta (variante, LED, zener) y mensajes engañosos | `variante < nvar` en todos los tipos nuevos; `SIM_MODELO` con su texto (LED con Rs·If ≥ Vf, zener con BV ≤ 0 o Vz0 ≤ 0, AO con V+ ≤ V−, variante fuera) | D20, §2.6, §3.A, §3.M, §3.N.8; [55], [58] |
| 20 | `circ_control_sig` devolvía 0 sin ninguno | −1 | §2.3, §4.4; [55] |
| 21 | El mensaje de `SOL_SIM` usaba `sim_red()` | `sim_mensaje_circ(&cres, 0, …)` | §4.1; [65] |
| 22 | La regla del AO con la matriz en uso (h/4 en el arranque) | Siempre con la de paso h (`cache_inest` solo en esas entradas) | D13, §3.F, §3.G.1, §8.1; [61] |
| 23 | `LU_MAX` en sim.h sin `lu.h`; `signed char *` frente a `int` | sim.h incluye lu.h; `SimMapa` con `int` | §2.6, §2.9 |
| 24 | Textos de más de 52 columnas | Línea 4 con prefijo y cuerpo (≤ 52 medido); `led_linea` con « +N»; «D13 amarillo: 145,2 mA (máx. 30 mA): se quemaría» (48) | §4.5, §4.11; [65]; `e4juez2/r3_textos.py` |
| 25 | Flancos del comparador a 1e-12·T | 1e-11·T | §6.8 |
| 26 | «El error baja 2,5 veces» | Error en las muestras frente a Radau, 1e-3·Vp con T/100 y con T/200, sin exigir proporción | §6.7 |
| 27 | Faltaban `NL_VZ`, `NL_N`, `NL_RS` | Añadidos | §2.7 |
| 28 | `sim_residuo_max` solo de la última emulación | `sim_residuo_acum` y su reset; el mono lo usa | §2.6, §3.D, §6.1 |
| 29 | `nombre_mag` y `unidad_q` sin la variante | La fila de la variante: «r(E2)» en Ω, «g(J2)» en S | §4.4, §5; [65] |
| 30 | La lectura inversa buscaba solo +nivel | `barr_nivel_dato`: +m y, si no hay cruce, −m | §2.10, §3.J, §4.8; [63] |
| 31 | `SimRes` sin los tramos de cada topología | `tramos_top[]` y `modos_de` con tramos | §2.6, §3.K |
| 32 | Iconos de 3 patas fuera de su celda | Marcos nuevos (AO: oy = y + 2, V = (0, 5); transistores: V = (0, 6)), medidos | §4.7; [65]; `e4juez2/r2_varios.py` |

**Lo que encontré de más al revisar.**
- **El astable con V0 = 0 dato** (lo que `circ_elemento` pone a todo C nuevo): en el OP el C fija V⁻ = 0 y el lineal no vale ya ahí, así que el aviso es `BIESTABLE` y no `ARRANQUE` como decía el [61]. No es un error del motor (con el C descargado la salida puede arrancar en cualquier raíl), pero el texto «tiene dos estados estables» engañaba: ahora es «U1 puede empezar en V+ o en V-: empieza en V+.»; el [61] prueba las dos variantes y los modelos 37 y 39 llevan la V0 vacía.
- **El derivador con V0 = 0 dato** da CI_SALTA por el segundo recorrido (la triangular vale −1 V en 0⁻); con la V0 vacía, sin aviso. El modelo 37, con la V0 vacía.
- **Cada punto del barrido** tenía trabajo sin tope (hasta 256 trozos de 30 iteraciones): tope `NL_OPS_OP` por punto, con hueco si se pasa.

