/* ============================================================================
 *  curva.h  -  La pantalla del barrido de CC (P_BARRIDO): lo que se ve de
 *              barrido.c (Etapa 4, §4.8)
 *
 *  Como osc.h y trazador.h, dos partes:
 *   - funciones puras: la columna de un x, la fila de un y, el valor de la
 *     malla interpolado, la autoescala y los cruces de un nivel. No dibujan:
 *     las prueba el arnés;
 *   - la pantalla (cv_dibuja) y sus teclas (cv_tecla), sobre una Curva que
 *     guarda la ventana, las escalas de cada traza, los cursores y el nivel.
 *
 *  Retícula de 10 × 8 divisiones: x de 34 a 314 (28 px; con la ventana
 *  entera, una columna por intervalo de la malla de 281) e y de 17 a 161
 *  (18 px). En y(x) el eje x es lo barrido; en X-Y, el canal 1 (y el canal 2
 *  en el eje y), y el cursor sigue estando en lo barrido.
 *
 *  Escalas (por traza; las trazas del mismo canal van siempre igual):
 *   - lineal: idiv es el índice de ESCALA (osc.h) y pos la posición del
 *     cero en medias divisiones, -8..8 (en el eje x de X-Y, -10..10);
 *   - logarítmica ([L], |y|): una década por división y pos es el exponente
 *     de la línea de arriba, de -4 a 3 (se ve de 1 pA a 1 kA como mucho);
 *     el 0 y los negativos van al pie.
 *
 *  Las lecturas del panel son exactas (barr_en: un Newton en ese x), no
 *  interpoladas; la derivada también (dy/dx con la LU del jacobiano).
 * ========================================================================= */
#ifndef CURVA_H
#define CURVA_H

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
    /* Añadido al §2.11: cv_x y cv_autoescala no reciben el BarrRes */
    unsigned char log_x;                       /* la malla es logarítmica (lo copia cv_nuevo) */
    /* La capa de entrada: mientras corre una ráfaga (una flecha mantenida o
     * el dedo), la lectura del panel sale de la malla (cv_valor) con «≈»; la
     * exacta (barr_en), con la primera tecla o al soltar. La pone ui.c */
    unsigned char rafaga;
} Curva;

/* Empieza a mirar un barrido: la ventana entera, autoescala, los cursores que
 * caen dentro se quedan (los demás, y los de una Curva nueva, al centro y a
 * tres cuartos; en uno cortado, dentro de lo hecho); X-Y si r->xy (o si ya
 * lo estaba y hay dos canales). Con r->tr_dato >= 0, el nivel del dato
 * (barr_nivel_dato: +m y, si no cruza, -m) y el cursor en su cruce, con el
 * aviso; si no, el aviso del barrido: «Cortado en R1 = 147,0 Ω.», los
 * huecos, el salto, «Sin sondas: …» o la vista de CC (ui.c no los pone).
 * Una Curva nueva va a cero (memset) antes del primer cv_nuevo. */
void   cv_nuevo(Curva *z, const BarrReg *g, const BarrRes *r);
/* La pantalla entera. Con r->fin == FIN_ERROR, el texto de sim_mensaje_circ
 * (con barr_circuito()) y el pie «[esc] volver». */
void   cv_dibuja(Curva *z, const BarrReg *g, const BarrRes *r);
/* Una tecla (con T_CON_SHIFT si va con shift). Devuelve CV_*. En la curva de
 * un componente (r->modo != BARR_CIRCUITO), [=], [tab] y [menu] dan CV_NADA
 * con un aviso; con FIN_ERROR, solo [esc] (y [menu] en un circuito). */
int    cv_tecla(Curva *z, const BarrReg *g, const BarrRes *r, int t);   /* con T_CON_SHIFT */
/* Columna de x (lo barrido) en la ventana, limitada a [CV_X0 - 1, CV_X0 + 281] */
int    cv_x(const Curva *z, double x);
/* Fila de y con la escala de la traza tr, limitada a [CV_Y0 - 1, CV_Y0 + 145];
 * en logarítmica, el 0, los negativos y lo de debajo, en CV_Y0 + 144 (al pie) */
int    cv_y(const Curva *z, int tr, double y);
double cv_valor(const BarrReg *g, int tr, double x);  /* interpolado; NaN en un hueco    */
/* Las escalas de todas las trazas con lo que se ve de la ventana */
void   cv_autoescala(Curva *z, const BarrReg *g);
/* Los cruces de nivel en la malla (interpolados, de menor a mayor x; los
 * huecos se saltan, como barr_nivel). Devuelve cuántos hay (guarda max). */
int    cv_cruces(const BarrReg *g, int tr, double nivel, double *xs, int max);

/* ---- Añadido al §2.11, para ui.c ---- */
/* La ventana en unidades de lo barrido (10^x0 y 10^x1 si es logarítmica):
 * lo que barre de nuevo CV_BARRER */
void   cv_ventana(const Curva *z, double *xa, double *xb);
/* Un valor con sus cifras fijas y su prefijo, como el panel: "360,0 Ω" (unidad " Ω") */
char  *cv_fija(char *buf, size_t cap, double v, int cifras, const char *unidad);
/* Lleva el cursor al cruce de nivel de la traza tr (dato = 1: una magnitud,
 * +nivel y, si no cruza, -nivel), con el aviso «I(D1) = 20,00 mA en R1 =
 * 360,0 Ω» o «... no llega a ...» (C_ERROR). Lo usa cv_nuevo con tr_dato;
 * ui.c, con un modelo de medida 'x'. 1 si hay cruce. */
int    cv_ir_nivel(Curva *z, const BarrReg *g, const BarrRes *r, int tr, double nivel, int dato);

#endif /* CURVA_H */
