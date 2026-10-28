# Compilar y probar Hakaru CES

Hakaru CES se compila de dos maneras con el mismo código:

- **para la calculadora**, con el Ndless SDK: sale `hakaru_ces.tns`;
- **para el PC**, con el `gcc` del sistema: sale el **arnés de pruebas**,
  que conduce la aplicación entera con guiones de teclas, comprueba los
  motores y vuelca cada pantalla a BMP (sin calculadora).

Todo se hace en Linux o en WSL 2 (Ubuntu 24.04). Los scripts están en
`sh` POSIX; el repositorio fuerza saltos de línea LF (`.gitattributes`), así
que funcionan aunque el código viva en una carpeta de Windows.

## 1. Preparar el entorno

```bash
sudo sh herramientas/instala_wsl.sh       # gcc, make, git, python3 y lo que pide el SDK
sudo sh herramientas/instala_ndless.sh    # el Ndless SDK en /opt/Ndless (tarda: compila la toolchain ARM)
```

`instala_ndless.sh` clona <https://github.com/ndless-nspire/Ndless> con sus
submódulos, construye la toolchain `arm-none-eabi` (`build_toolchain.sh`) y
luego el SDK (`genzehn`, `make-prg`, libndls). El `make` de la raíz de
Ndless no es paralelizable. Al terminar, `nspire-gcc` y `genzehn` quedan en
`/opt/Ndless/ndless-sdk/toolchain/install/bin` y `/opt/Ndless/ndless-sdk/bin`.

Para los oráculos y los generadores de `herramientas/` hace falta Python 3
con numpy, SciPy, mpmath y Pillow:

```bash
python3 -m pip install numpy scipy mpmath pillow
```

## 2. El programa para la calculadora

```bash
sh build_tns.sh          # make clean && make, con el SDK de /opt/Ndless en el PATH
```

o, con el SDK ya en el `PATH`:

```bash
make                     # hakaru_ces.tns
make clean               # borra los .o, el .elf y el .tns
```

El resultado, `hakaru_ces.tns` (898 480 bytes), se copia a la calculadora (ver
el [README](../README.md#instalación)).

**Cómo se compila.** `nspire-gcc` con `-Os -marm`, y el texto en UTF-8
convertido a Latin-1 al compilar (`-finput-charset=UTF-8
-fexec-charset=ISO-8859-1`): las tildes y la ñ se escriben tal cual en el
código y llegan a la pantalla en Latin-1, el mapa de la fuente. `genzehn`
hace la cabecera Zehn (`--name "Hakaru CES"`) y `make-prg` el `.tns`.

**La coma flotante rápida.** La CX II no tiene FPU: la coma flotante es por
software. `cf_arm.c` sustituye la división, la raíz cuadrada y las
comparaciones de `double` de libgcc y newlib por versiones propias **exactas**
(los mismos bits IEEE 754) y mucho más rápidas. Se enlazan con `--wrap`
(`CF_WRAP` en el Makefile), así que pasan por ellas todas las llamadas,
también las de la libm. En el PC no se usa.

**Banderas del Makefile.** Al cambiar una, `make clean` primero: make no ve el
cambio de banderas.

| Bandera | Por omisión | Qué hace |
|---|---|---|
| `TURBO=1` | 0 | Compila la fila «Turbo» del panel Controles: subir el reloj de la CPU por encima de 396 MHz. **Experimental y fuera de la versión**: escribe un registro de la PMU sin el protocolo que usa el propio sistema operativo para cambiar de reloj. Bajo la responsabilidad de quien lo use. Ver [DESIGN.md](DESIGN.md#23-la-optimización-rev9). |
| `DEBUG=TRUE` | `FALSE` | `-O0 -g` en lugar de `-Os` |
| `ESPECIAL=1` | 0 | La versión especial del autor, que añade extras privados desde la carpeta `especial/`. Esa carpeta no se publica: sin ella, esta bandera no compila. |

## 3. El arnés de pruebas

```bash
sh build_host.sh          # compila y corre la batería entera (~1,5 min); BMP en out/
sh build_host.sh asan     # igual, con AddressSanitizer, UBSan y float-cast-overflow
sh build_host.sh arm      # igual, con char sin signo, como en el ARM de la calculadora
sh build_host.sh solo     # solo compila: out/hakaru_test
sh build_host.sh capa     # compila out/hakaru_capa: la suite conducida por la capa de entrada
```

Al final dice `352164 comprobaciones, 0 fallos` y `TODO CORRECTO` (2026-10-28),
y deja unas 900 capturas en `out/*.bmp` (cada pantalla de la aplicación, en
el tamaño de la calculadora, 320×240).

**Qué comprueba.** El motor de CC (40 ejemplos con clave, los 9 del
pizarrón, redes al azar, diagnósticos) y el procedimiento contra el motor; la
interfaz por guion de teclas, sin ningún texto fuera de la pantalla; los
archivos rotos; los tiempos. Del emulador: el cruce con la calculadora de CC
en 2000 redes, transitorios contra soluciones exactas, eventos e impulsos,
el arranque tras los flancos, los modos, el osciloscopio y las 12 claves de
la biblioteca. De la CA: casos cerrados, el cruce con el régimen del
trapecio, Thévenin, el Bode con sus medidas, la calculadora de CA contra
mpmath y con 300 problemas inversos, el fasorial, el trazador y las 10 claves
de la lección 11. De la Etapa 4: diodos, AO, controladas, BJT y MOSFET, el
punto de operación, el barrido, la pequeña señal y las claves de las
lecciones 12 y 13. De los controles: la capa de entrada en tiempo virtual
(teclas, repetición, touchpad, reloj y energía). De la optimización: que cada
cambio dé los mismos resultados que antes. De los capacitores equivalentes:
el cálculo contra fuerza bruta, las pantallas y los dibujos. Y el modelo de
tiempos contra el banco de la calculadora real.

**Partes sueltas.** Tras `sh build_host.sh solo`:

```bash
HAKARU_SOLO_E4=1 ./out/hakaru_test out
```

| Variable | Corre |
|---|---|
| `HAKARU_SOLO_E2T` | el transitorio lineal (Etapa 2) y el oráculo [29] |
| `HAKARU_SOLO_E4` | la Etapa 4 y lo de después |
| `HAKARU_SOLO_REV`, `HAKARU_SOLO_REV2` | los hallazgos de las revisiones (las mutaciones) |
| `HAKARU_SOLO_ORA` | las redes del oráculo no lineal [67] |
| `HAKARU_SOLO_FLANCO` | el banco del arranque tras un flanco |
| `HAKARU_SOLO_M_EDITOR`, `HAKARU_SOLO_M_RESULTADOS` | las 13 mejoras de la tanda 1 |
| `HAKARU_SOLO_CAPA` | la capa de entrada [71] |
| `HAKARU_SOLO_FLUJO` | cuentas, copiar y pegar, nombres [72] |
| `HAKARU_SOLO_CIERRE`, `HAKARU_SOLO_PANTALLAS` | el cierre de la Etapa 4 [73] y sus pantallas [74] |
| `HAKARU_SOLO_CAL`, `HAKARU_SOLO_FINAL` | el modelo de tiempos y la integración de la optimización |
| `HAKARU_SOLO_CAPMOTOR` | los capacitores equivalentes [76] |
| `HAKARU_SOLO_TIEMPOS` | los tiempos de reloj, solos (con otras baterías a la vez fallan por la carga) |
| `HAKARU_SOLO_MONO` | el mono, sin lo demás |

Otras: `HAKARU_DEPURA=1` (más detalle al fallar), `HAKARU_HUELLA_LOG=archivo`
(la huella de cada emulación, para comparar dos versiones del motor),
`HAKARU_GRABACION=ces_muestras.tns` (pasa por la capa de entrada lo que grabó
el grabador en la calculadora).

**El mono.** Pulsa teclas al azar por todas las pantallas y, tras las teclas
sueltas, manda ráfagas y pasos del dedo como la capa de entrada; vigila que
el circuito siga sano, el residuo de Kirchhoff de toda la sesión y que ningún
texto se salga.

```bash
sh mono.sh 1000000          # un millón de teclas por cada una de 4 semillas
sh mono.sh 200000 asan      # con AddressSanitizer
sh mono_desbordes.sh        # lista los textos que se salen, si aparece alguno
```

**Por la capa de entrada.** `compara_capa.sh` corre la suite dos veces: con
las teclas entregadas directamente a la interfaz y con una mano virtual que
pulsa cada tecla y hace clic en la cuña de cada flecha (`-DPOR_LA_CAPA`).
Tienen que dar las mismas comprobaciones, los mismos BMP byte a byte y el
mismo texto:

```bash
sh compara_capa.sh          # IGUALES
```

## 4. Los oráculos independientes

El arnés exporta redes al azar con lo que da el programa (`out/oraculo`,
`out/oraculo_ca` y `out/oraculo_nl`: la red en `.net`, en el formato
`cesnet`, y los resultados en `.ces`). Tres simuladores de referencia
escritos aparte, en Python, las resuelven por su cuenta y comparan (en
Windows, con `PYTHONIOENCODING=utf-8`):

```bash
python3 herramientas/compara_oraculo.py   # tiempo, lineal: 98 de 98 iguales (y 16 que el oráculo rechaza)
python3 herramientas/oraculo_ca.py        # CA: 174 de 174 (fasores a 1e-12, Bode a 1e-10)
python3 herramientas/oraculo_nl.py        # no lineal: 260 de 260 (200 de CC y 60 en el tiempo)
```

`mira_caso.py` enseña dónde difieren el emulador y el oráculo en un caso.

## 5. Los generadores

| Script | Genera |
|---|---|
| `genfont.py` (con `fuente_psf.py`) | `font6x12.c`, la fuente Terminus 6×12 desde el PSF de consola |
| `herramientas/gen_kanji.py` | `kanji_img.c`, el kanji 測る de la portada, con Noto Sans JP, ya mezclado con el fondo (si cambia `C_FONDO`, se regenera) |
| `herramientas/banco_flanco.py` | las tablas de `test_flanco.inc`: los casos del banco del arranque tras un flanco y su solución exacta (mpmath, 40 cifras) |
| `herramientas/gen_imagenes_readme.py` | las capturas de `docs/img`, el banner y el GIF de demostración del README, desde las BMP que deja `sh build_host.sh` en `out/` |
| `herramientas/gen_graficas.py` | la gráfica de la optimización del README, en versión clara y oscura (matplotlib) |
| `herramientas/estadisticas_codigo.py` | la sección «Código» del README (pastel de Mermaid y tabla de líneas por lenguaje) y la insignia de líneas de código; lo corre en cada cambio el workflow `.github/workflows/estadisticas.yml` |

## 6. Pruebas en la calculadora real

El arnés estima los tiempos de la calculadora con un modelo calibrado en la
propia CX II con el **banco de tiempos** (Ayuda > `[T]`; ver el
[manual](MANUAL.md#12-el-banco-de-tiempos)). Para medir bien: **sin el cable
USB** (con USB, el reloj baja a 288 MHz), tras apagar y encender la
calculadora y reactivar Ndless. Si una optimización cambia lo que cuesta cada
operación, la segunda página del banco da lo necesario para recalibrar
`SIM_NS_OP`, `SIM_NS_PASO` (`sim.h`), `CA_NS_OP` (`ca.h`), `NL_OPS_EXP` y
`NS_ITER` (`nolin.h`); `test_calibracion` (en [14]) lleva las cargas del banco
y falla si el modelo deja de dar lo medido. Lo que aún falta probar a mano en
la calculadora está en la [hoja de ruta](HOJA_DE_RUTA.md#9-pendiente-en-la-calculadora-real).
