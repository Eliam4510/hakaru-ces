/* ============================================================================
 *  gfx.h  -  Capa grafica de Hakaru CES
 *
 *  Copiada de EstaticaCX (commit eca703d). Cambian la paleta y los glifos
 *  nuevos (ohmio, paralelo, aproximado, visto bueno); las primitivas son las
 *  mismas.
 *
 *  Backbuffer propio de 320x240 en RGB565. Todas las primitivas trabajan en
 *  enteros (el ARM926 de la CX II no tiene FPU: el soft-float jamas debe
 *  entrar en un bucle de rasterizado).
 *
 *  Compila igual en la calculadora (-DNDLESS) y en PC. En PC el volcado se
 *  hace a BMP con gfx_dump_bmp(), lo que permite depurar el 100% del dibujo
 *  sin tocar el hardware.
 * ========================================================================= */
#ifndef GFX_H
#define GFX_H

#include <stddef.h>

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define SCR_W 320
#define SCR_H 240

/* --------------------------------- Color --------------------------------- */
/* RGB565: rrrrrggg gggbbbbb                                                  */
#define RGB(r,g,b) ((u16)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

/* Paleta. El COLOR dice que es cada cosa; una incognita se rotula en su
 * propio color para que salte a la vista en el dibujo.                       */
#define C_FONDO     RGB( 16, 18, 24)   /* gris muy oscuro                     */
#define C_PANEL     RGB( 28, 32, 42)   /* fondo del panel de texto            */
#define C_BARRA     RGB( 40, 70,130)   /* barra superior / seleccion de lista */
#define C_RESIST    RGB(235,235,240)   /* resistencia (blanco)                */
#define C_FUENTE    RGB(250,205, 40)   /* fuente de tension (amarillo)        */
#define C_FUENTE_I  RGB(255,150, 45)   /* fuente de corriente (naranja)       */
#define C_INSTR     RGB(120,220,255)   /* instrumentos (celeste)              */
#define C_COND      RGB(190,150,255)   /* condensador (lila)                  */
#define C_BOB       RGB(220,150,100)   /* bobina (cobre)                      */
#define C_INTER     RGB(210,214,222)   /* interruptor                         */
#define C_CANAL1    RGB(255,230, 60)   /* canal 1 del osciloscopio (amarillo) */
#define C_CANAL2    RGB( 80,200,255)   /* canal 2 (celeste)                   */
#define C_CABLE     RGB(150,158,172)   /* cable                               */
#define C_CORRIENTE RGB( 70,215,235)   /* corriente (cian)                    */
#define C_TENSION   RGB( 90,220,110)   /* tension (verde)                     */
#define C_POTENCIA  RGB(255,150, 45)   /* potencia (naranja)                  */
#define C_IMP       RGB(200,225,140)   /* impedancia Z (verde palido)         */
#define C_DIODO     RGB(240,120,110)   /* diodo, zener, LED (Etapa 4)         */
#define C_AO        RGB(130,220,170)   /* amplificador operacional           */
#define C_TRANS     RGB(230,200,120)   /* BJT y MOSFET                       */
#define C_REACT     C_TITULO           /* potencia reactiva Q                 */
#define C_INCOG     RGB(240,110,210)   /* incognita pedida (magenta)          */
#define C_RESALTA   RGB(255,150, 45)   /* elemento resaltado en un paso       */
#define C_CURSOR    RGB(250,205, 40)
#define C_REJILLA   RGB( 52, 58, 72)   /* puntos de la rejilla del editor     */
#define C_COTA      RGB(175,182,195)   /* nombres de elementos, texto suave   */
#define C_TEXTO     RGB(228,230,236)
#define C_TENUE     RGB( 96,104,118)   /* elementos atenuados en un paso      */
#define C_SUAVE     RGB(150,160,176)   /* texto secundario: se lee, no grita  */
#define C_TITULO    RGB(120,200,255)   /* titulos de seccion                  */
#define C_OK        RGB( 90,220,110)
#define C_ERROR     RGB(240, 80, 80)

/* ------------------------- Glifos fuera de ASCII --------------------------
 * Mapa de caracteres del programa (las dos fuentes lo siguen):
 *   0x20-0x7E  ASCII
 *   0x80-0x9D  simbolos propios: ohmio, paralelo, flechas, subindices...
 *   0xA0-0xFF  Latin-1: tildes, enye, abre-interrogacion, grado, cuadrado,
 *              punto medio, por
 *
 * El codigo fuente esta en UTF-8 y se compila con -fexec-charset=ISO-8859-1:
 * "tensión" se escribe tal cual y llega a la pantalla como Latin-1. Los
 * simbolos que NO son Latin-1 (el ohmio, las flechas) van como macros de
 * CADENA y no como escapes sueltos: en C, "\x801" es el hex 0x801, no 0x80
 * seguido de '1'. Concatenando macros ese problema desaparece.
 * ------------------------------------------------------------------------ */
#define S_OHM    "\x80"   /* Omega mayuscula: ohmio */
#define S_PAR    "\x81"   /* en paralelo */
#define S_APROX  "\x82"   /* aproximadamente igual */
#define S_CHECK  "\x83"   /* visto bueno */
#define S_RAYA   "\x84"   /* raya: magnitud sin dato ni pregunta */
#define S_RARR   "\x85"   /* flechas */
#define S_LARR   "\x86"
#define S_UARR   "\x87"
#define S_DARR   "\x88"
#define S_SQRT   "\x89"
#define S_LE     "\x8A"
#define S_GE     "\x8B"
#define S_NE     "\x8C"
#define S_DELTA  "\x8D"
#define S_SIGMA  "\x8E"
#define S_SUB1   "\x8F"
#define S_SUB2   "\x90"
#define S_SUB3   "\x91"
#define S_SUB4   "\x92"
#define S_SUB0   "\x93"
#define S_INF    "\x94"
#define S_THETA  "\x95"
#define S_PI     "\x96"
#define S_ALFA   "\x97"
#define S_BETA   "\x98"
#define S_GAMMA  "\x99"
#define S_RHO    "\x9A"
#define S_PHI    "\x9B"   /* fi: flujo, fase */
#define S_OMEGA  "\x9C"
#define S_INT    "\x9D"   /* integral */
#define S_TAU    "\x9E"   /* tau: constante de tiempo */
#define S_ZETA   "\x9F"   /* zeta: amortiguamiento */
/* Latin-1: tambien se pueden escribir directamente en el texto */
#define S_DEG    "\xB0"
#define S_ANG    "\x7F"   /* angulo (fasores): libre en las dos fuentes */
#define S_PM     "\xB1"
#define S_SUP2   "\xB2"
#define S_SUP3   "\xB3"
#define S_MU     "\xB5"
#define S_CDOT   "\xB7"   /* punto medio (multiplicacion) */
#define S_TIMES  "\xD7"
#define S_DIVIDE "\xF7"   /* entre (las cuentas de la ficha) */
#define S_aa     "\xE1"
#define S_ee     "\xE9"
#define S_ii     "\xED"
#define S_oo     "\xF3"
#define S_uu     "\xFA"
#define S_nn     "\xF1"
#define S_ud     "\xFC"
#define S_NN     "\xD1"
#define S_IQ     "\xBF"   /* abre interrogacion */
#define S_IE     "\xA1"   /* abre exclamacion */
#define S_LAQUO  "\xAB"   /* comillas «: el texto de un campo sigue a la izquierda */

/* ------------------------------- Ciclo de vida --------------------------- */
void gfx_init(void);      /* prepara fuente y, en calculadora, el LCD        */
void gfx_deinit(void);    /* devuelve el LCD al estado original              */
void gfx_flush(void);     /* vuelca el backbuffer a la pantalla (lcd_blit)   */
u16 *gfx_buffer(void);    /* acceso directo al backbuffer                    */

/* Reloj en milisegundos para el banco de tiempos. En la calculadora es el
 * RTC (0x90090000, el mismo que usa gettimeofday de Ndless): solo cuenta
 * SEGUNDOS, asi que mide bien intervalos de varios segundos. En el PC,
 * clock(). No sirve para la hora del dia. */
unsigned long gfx_reloj_ms(void);

/* ------------------------------- Recorte --------------------------------- */
/* Limita el area de dibujo. Imprescindible para que el diagrama no invada el
 * panel de texto. gfx_clip_reset() vuelve a la pantalla completa.            */
void gfx_clip(int x, int y, int w, int h);
void gfx_clip_reset(void);
/* La primera fila que se dibuja (el borde de arriba del recorte): lo de
 * encima no se ve (el esquema, bajo la barra de título) */
int  gfx_clip_arriba(void);

/* ------------------------------- Primitivas ------------------------------ */
void gfx_clear(u16 c);
void gfx_px(int x, int y, u16 c);
void gfx_hline(int x, int y, int w, u16 c);
/* La fila y desde la columna 0, n palabras (2n píxeles): el byte f[i] pinta
 * los píxeles 2i y 2i + 1 con la palabra pares[f[i]], tal cual va en
 * memoria (hecha con una union { u16 h[2]; u32 w; }: h[0] el de la
 * izquierda). Sin recorte: para una imagen que ocupa filas enteras.
 * Una fila fuera de la pantalla no escribe nada, y n no pasa de SCR_W/2 */
void gfx_fila_pares(int y, const unsigned char *f, int n, const u32 *pares);
void gfx_vline(int x, int y, int h, u16 c);
void gfx_fill(int x, int y, int w, int h, u16 c);
/* Cuántos píxeles del rectángulo no son del color de fondo (lo que cae
 * fuera del recorte cuenta como ocupado): dónde cabe un rótulo sin tapar
 * nada. */
int  gfx_ocupados(int x, int y, int w, int h, u16 fondo);
void gfx_rect(int x, int y, int w, int h, u16 c);
void gfx_line(int x0, int y0, int x1, int y1, u16 c);
/* Linea gruesa (grosor en pixeles, 1..4). Los vectores de fuerza usan 2.     */
void gfx_line_w(int x0, int y0, int x1, int y1, u16 c, int grosor);
/* Linea discontinua: 'on' pixeles pintados, 'off' en blanco. Marca incognita.*/
void gfx_line_dash(int x0, int y0, int x1, int y1, u16 c, int on, int off);
void gfx_circle(int cx, int cy, int r, u16 c, int relleno);
/* Triangulo relleno: puntas de flecha, apoyos fijos.                         */
void gfx_tri(int x0, int y0, int x1, int y1, int x2, int y2, u16 c);
/* Arco de circunferencia entre dos angulos en GRADOS (sentido antihorario,
 * 0 = eje +x, y crece hacia arriba como en el mundo fisico).                 */
void gfx_arc(int cx, int cy, int r, int a0, int a1, u16 c);
/* Rayado a 45 grados: suelo, muros, empotramientos.                          */
void gfx_hatch(int x, int y, int w, int h, u16 c, int paso);

/* --------------------------------- Texto --------------------------------- */
/* Texto normal: Terminus 6x12 (53 columnas, mayusculas de 8 filas y 2 filas
 * de descendentes). escala 2 = 12x24 para titulos. Es la que se lee bien en
 * la pantalla de la calculadora.
 *
 * gfx_text_p: la 6x8 antigua, solo para rotulos del esquema cuando no hay
 * sitio para la grande. Devuelven el ancho en pixeles.                        */
#define FONT_W   6
#define FONT_H   12  /* OJO: por eso el guard de font.h es FONT_API_H */
#define FONT_H_P 8

int  gfx_text(int x, int y, const char *s, u16 fg, int escala);
int  gfx_text_bg(int x, int y, const char *s, u16 fg, u16 bg, int escala);
int  gfx_text_c(int cx, int y, const char *s, u16 fg, int escala); /* centrado */
int  gfx_text_w(const char *s, int escala);   /* ancho sin dibujar           */
int  gfx_text_p(int x, int y, const char *s, u16 fg);
int  gfx_text_p_bg(int x, int y, const char *s, u16 fg, u16 bg);

/* Contador de textos que empiezan dentro de la pantalla y se salen por la
 * derecha. Salirse no da ningun error: el texto simplemente aparece cortado y
 * en una captura pequena no se nota. Con esto el arnes puede exigir cero.   */
int  gfx_desbordes(void);
void gfx_desbordes_reset(void);
/* Silencia el aviso por consola para las pruebas que dibujan fuera a
 * proposito. No afecta al contador. */
void gfx_desbordes_silencio(int s);

/* ------------------------- Trigonometria entera --------------------------- */
/* sen/cos en punto fijo: devuelven el valor multiplicado por 1024.
 * Angulo en grados enteros, cualquier signo. Sin soft-float.                 */
int gfx_isin(int grados);
int gfx_icos(int grados);

/* ------------------------------ Solo en PC -------------------------------- */
#ifndef NDLESS
/* Vuelca el backbuffer a un BMP de 24 bits. Devuelve 0 si todo fue bien.
 * Es lo que permite revisar los diagramas sin la calculadora.                */
int gfx_dump_bmp(const char *ruta);
/* Igual pero ampliado n veces, para inspeccionar el detalle de la fuente.    */
int gfx_dump_bmp_zoom(const char *ruta, int zoom);
/* Los textos dibujados desde el último reset (uno por línea), para que el
 * arnés compruebe lo que se ve y no solo lo que se calcula. */
void gfx_textos_reset(void);
int  gfx_texto_visto(const char *t);
#endif

#endif /* GFX_H */
