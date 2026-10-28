/* ============================================================================
 *  osc.h  -  El osciloscopio: lo que se ve de una emulación
 *
 *  Dos partes:
 *   - funciones puras sobre el registro (SimReg): el valor en un instante,
 *     el mínimo y el máximo de cada columna de la pantalla, las medidas de
 *     una ventana, los picos y el disparo. No dibujan: las prueba el arnés
 *     con registros sintéticos;
 *   - la pantalla (osc_dibuja) y sus teclas (osc_tecla), sobre un Osc que
 *     guarda las escalas, la ventana, los cursores y lo ya calculado.
 *
 *  Retícula de 10 x 8 divisiones: 30 px por división en el tiempo (300
 *  columnas: una por décima de división) y 21 px en el eje vertical.
 * ========================================================================= */
#ifndef OSC_H
#define OSC_H

#include "sim.h"
#include "gfx.h"

#define OSC_X0      10          /* borde izquierdo de la retícula          */
#define OSC_DX      30          /* px por división de tiempo               */
#define OSC_Y0      18          /* borde de arriba                         */
#define OSC_DY      21          /* px por división vertical                */
#define OSC_XC      160
#define OSC_YC      102         /* el eje central                          */
#define OSC_NCOL    300
#define OSC_PANEL_Y 188
#define N_ESCALA    48
extern const double ESCALA[N_ESCALA];          /* 1-2-5 de 1e-9 a 5e6 */

enum { OSC_MEDIDAS = 0, OSC_CURSORES };
enum { DISP_LIBRE = 0, DISP_SUBIDA, DISP_BAJADA };
enum { OSC_NADA = 0, OSC_SALIR, OSC_TABLA, OSC_EMULAR, OSC_AJUSTES };

typedef struct {
    double max, min, pp, media, eficaz, frec;
    unsigned char hay, hay_frec;
} Medidas;

typedef struct {
    signed char itdiv, ivdiv[2], pos[2];   /* índices de ESCALA; pos en medias divisiones, -8..8 */
    unsigned char ver[2];                  /* el canal se dibuja                */
    unsigned char activo, cursores, cur, panel, disparo, ajustado;
    double tizq, tc[2];                    /* borde izquierdo; cursores T1 y T2 */
    char entrada[20];                      /* el tiempo que se está escribiendo */
    char aviso[64];
    u16  aviso_col;
    unsigned c_gen;                        /* lo calculado vale para este registro, */
    signed char c_itdiv;                   /* esta escala                         */
    double c_tizq;                         /* y esta ventana                      */
    double mn[2][OSC_NCOL], mx[2][OSC_NCOL];
    unsigned char ok[2][OSC_NCOL];
    Medidas med[2];
    int n_vent;                            /* muestras en la ventana              */
    /* Mejoras de resultados */
    double nivel_disp;                     /* el nivel del disparo: el medio del canal activo
                                              en la ventana al pulsar [D] (o [C], o al llegar
                                              otra emulación); fijo hasta otro [D] o [A]       */
    double c_ta, c_tb;                     /* dónde se midió: la ventana o [T1, T2]          */
    int n_med;                             /* muestras entre T1 y T2 (con los cursores)      */
    double u_ta, u_tb;                     /* el periodo que puso [U] en T1 y T2 (su frec es
                                              1/(u_tb - u_ta) mientras no se muevan); 0 = no  */
    unsigned char u_canal;                 /* el canal en el que se buscó                    */
    /* Las filas de cada columna (osc_pix del máximo y del mínimo), para no
     * convertirlas en cada cuadro: valen mientras prepara no rehaga las
     * columnas y no cambien la escala ni la posición del canal            */
    unsigned char f_ok[2];                 /* 0: por hacer (memset y prepara)                */
    signed char f_iv[2], f_pos[2];         /* con esta escala y esta posición                */
    unsigned char f_y0[2][OSC_NCOL], f_y1[2][OSC_NCOL];   /* osc_pix: de OSC_Y0 - 1 a OSC_Y0 + 169 */
} Osc;

/* Empieza a mirar una emulación: con autoescala, o conservando las escalas */
void   osc_nuevo(Osc *o, const SimReg *r, const SimRes *s, int autoescala);
/* ¿Las medidas van entre los cursores? (encendidos y separados; si no, son
 * de la ventana entera) */
int    osc_mide_cursores(const Osc *o);
void   osc_dibuja(Osc *o, const SimReg *r, const SimRes *s);
/* Las columnas y las medidas de la ventana (o de [T1, T2]) y n_vent y
 * n_med, si cambió algo: lo que osc_dibuja calcula antes de pintar. Quien
 * lea n_vent o n_med fuera del dibujo (el [=] de ui.c) lo llama antes: así
 * no depende de si se dibujó tras la última tecla */
void   osc_prepara(Osc *o, const SimReg *r, const SimRes *s);
/* Una tecla (con T_CON_SHIFT si va con shift). Devuelve OSC_*. */
int    osc_tecla(Osc *o, const SimReg *r, const SimRes *s, int t);

/* Valor del canal k en t, interpolado; en un evento, el de después */
double osc_valor(const SimReg *r, int k, double t);
/* Mínimo y máximo de cada columna [tizq + c·dcol, tizq + (c+1)·dcol]; la
 * columna c acaba en el valor en que empieza la c+1 (la traza es continua).
 * ok[c] = 0 fuera del registro. Devuelve las columnas con dato. */
int    osc_columnas(const SimReg *r, int k, double tizq, double dcol, int ncol,
                    double *mn, double *mx, unsigned char *ok);
/* Medidas del canal k en [ta, tb] */
void   osc_medidas(const SimReg *r, int k, double ta, double tb, Medidas *m);
/* El siguiente máximo (o mínimo) después de `desde`, con prominencia del 1 %
 * y refinado parabólico. 1 si lo hay. */
int    osc_pico(const SimReg *r, int k, double desde, int maximo, double *t, double *v);
/* El siguiente (sentido = +1) o el anterior (-1) cruce de `nivel` subiendo
 * (o bajando), con histéresis del 1 %. 1 si lo hay. */
int    osc_evento(const SimReg *r, int k, double nivel, int subida, double desde, int sentido, double *t);
/* El último periodo del canal k ([U]): tb = el final del registro y ta =
 * tb - T, con T el tiempo entre sus dos últimos cruces de subida por el
 * nivel medio de la cola del registro (la mitad final; si ahí no se repite,
 * la cuarta parte, y así; y al final, el registro entero). Vale si el
 * último cruce cae en el último periodo y T casa con la frecuencia media
 * de la cola (5 %). 1 si lo hay; 0 si la señal no se repite (o no llega a
 * dos periodos). */
int    osc_ultimo_periodo(const SimReg *r, int k, double *ta, double *tb);
void   osc_autoescala(Osc *o, const SimReg *r, const SimRes *s);
/* Índice del primer valor de ESCALA >= x */
int    osc_escala_ge(double x);
/* Fila de la pantalla de un valor, limitada a [OSC_Y0 - 1, OSC_Y0 + 169] */
int    osc_pix(double v, double vdiv, int pos);

#endif /* OSC_H */
