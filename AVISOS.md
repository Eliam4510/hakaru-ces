# Avisos de terceros

El código de Hakaru CES es de Eliam Mojardin Ibarra, con licencia MIT
([LICENSE](LICENSE)). Además lleva o usa lo siguiente.

## Fuentes tipográficas (SIL Open Font License 1.1)

| Dónde | Qué | Autor y licencia |
|---|---|---|
| `font6x12.c` | El texto del programa: **Terminus 6×12**, convertida a mapa de bits con `genfont.py` (desde el PSF de consola de Ubuntu, `fuente_psf.py`) | © 2010 Dimitar Toshkov Zhekov. SIL Open Font License 1.1 |
| `kanji_img.c` | El kanji 測る de la portada: un glifo de **Noto Sans JP** dibujado como imagen con `herramientas/gen_kanji.py` | © 2014-2021 Adobe (Noto CJK). SIL Open Font License 1.1 |

La SIL Open Font License 1.1 permite usar, copiar, modificar y redistribuir
las fuentes, también dentro de otro programa, siempre que la fuente no se
venda sola y que el aviso de copyright y la licencia la acompañen. El texto
completo: <https://openfontlicense.org/open-font-license-official-text/>. El
programa lo recuerda en la Ayuda, en ACERCA DE.

`font6x8.c` (los rótulos muy apretados del esquema) es una fuente propia,
escrita glifo a glifo en ASCII-art para EstaticaCX, el proyecto hermano del
mismo autor, y traída de allí.

## Ndless y su SDK

El `.tns` se compila con el **Ndless SDK** (<https://github.com/ndless-nspire/Ndless>)
y se ejecuta sobre **Ndless**. Ndless se distribuye con la Mozilla Public
License 1.1; el SDK enlaza en el programa sus bibliotecas (libndls, la
newlib y la libgcc de la cadena `arm-none-eabi`, cada una con su licencia y
sus excepciones de enlace). Hakaru CES no modifica ningún archivo de Ndless;
su código fuente está en el repositorio de arriba.

TI-Nspire y TI-Nspire CX II CAS son marcas de Texas Instruments. Este
proyecto no está afiliado a Texas Instruments ni lo respalda. NI Multisim,
mencionado como referencia de estilo, es una marca de National Instruments.

## Herramientas de desarrollo

Los oráculos y los generadores de `herramientas/` usan Python con numpy,
SciPy, mpmath y Pillow (cada uno con su licencia). No se distribuyen con el
programa: solo sirven para generar código o comprobarlo.
