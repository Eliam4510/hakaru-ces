/* ============================================================================
 *  trazador.h  -  La pantalla del Bode (P_BODE): lo que se ve de un barrido
 *
 *  Como osc.h, dos partes:
 *   - funciones puras: la columna de una f, la fila de un dB o de un ángulo,
 *     el valor de la malla interpolado en log f, la autoescala, las marcas y
 *     la lectura exacta de un cursor. No dibujan: las prueba el arnés;
 *   - la pantalla (traz_dibuja) y sus teclas (traz_tecla), sobre un
 *     Trazador que guarda la ventana, las escalas y los cursores.
 *
 *  Dos gráficas con el mismo eje de f, logarítmico, de x = 38 a 314 (276 px:
 *  con la ventana entera, una columna por punto de la malla de 277):
 *   - magnitud en dB, 4 divisiones de 21 px (y de 17 a 101);
 *   - fase en grados (desenrollada), 4 divisiones de 14 px (y de 106 a 162).
 *  Las escalas son índices de DBDIV y FDIV; el tope (la línea de arriba) es
 *  un número entero de divisiones: vale db_top · DBDIV[idb] dB.
 *
 *  Las lecturas de los cursores son exactas (bode_en, con la red del último
 *  barrido), no interpoladas; la fase se desenrolla como la curva.
 *
 *  [shift][arr] sube la curva una división y [shift][ab] la baja, como la
 *  posición en el osciloscopio.
 * ========================================================================= */
#ifndef TRAZADOR_H
#define TRAZADOR_H

#include "bode.h"
#include "gfx.h"

#define BD_X0 38
#define BD_W 276
#define BD_MY0 17        /* magnitud: 4 divisiones de 21 px */
#define BD_MDY 21
#define BD_FY0 106       /* fase: 4 divisiones de 14 px     */
#define BD_FDY 14
#define BD_EJE_Y 164
#define BD_PANEL_Y 175
extern const double DBDIV[6];          /* 1 2 5 10 20 40 dB por división */
extern const double FDIV[4];           /* 15 30 45 90 grados            */
enum { TZ_NADA = 0, TZ_SALIR, TZ_FASORES, TZ_BARRER, TZ_AJUSTES, TZ_ENTRADA, TZ_SALIDA };
typedef struct Trazador {
    double lf0, lf1;                   /* ventana en log10 f                         */
    double fcur[2];                    /* cursores (Hz)                               */
    unsigned char dos, activo, ajustado, imarca;
    signed char idb, db_top, ifase, fase_top;   /* escalas (índices) y tope en divisiones */
    char entrada[20], aviso[64]; u16 aviso_col;
    /* La capa de entrada: mientras corre una ráfaga, la lectura sale de la
     * malla (traz_valor) con «≈»; la exacta (bode_en), al soltar. La pone ui.c */
    unsigned char rafaga;
} Trazador;

/* Empieza a mirar un barrido: la ventana entera; los cursores que caen
 * dentro del barrido se quedan (los demás, al centro); autoescala, salvo la
 * de dB si z->ajustado (el usuario la tocó). Un Trazador nuevo va a cero. */
void   traz_nuevo(Trazador *z, const BodeReg *g, const BodeRes *r);
void   traz_dibuja(Trazador *z, const BodeReg *g, const BodeRes *r);
/* Una tecla (con T_CON_SHIFT si va con shift). Devuelve TZ_*. */
int    traz_tecla(Trazador *z, const BodeReg *g, const BodeRes *r, int t);   /* con T_CON_SHIFT */
/* Columna de f en la ventana, limitada a [BD_X0 - 1, BD_X0 + BD_W + 1] */
int    traz_x(const Trazador *z, double f);
int    traz_y_db(const Trazador *z, double db);      /* en [BD_MY0-1, BD_MY0+85] */
int    traz_y_fase(const Trazador *z, double g);     /* en [BD_FY0-1, BD_FY0+57] */
/* dB (fase = 0) o fase en grados (fase = 1) de la malla, interpolado en
 * log f; NaN fuera del barrido o junto a un punto con ok = 0 (hueco) */
double traz_valor(const BodeReg *g, double f, int fase);   /* interpolado en log f */
/* dB y fase por división y topes: todo lo que se ve de la ventana, dentro */
void   traz_autoescala(Trazador *z, const BodeReg *g);

/* ---- Puras, para el arnés ---- */
/* Las marcas del tipo de filtro, de menor a mayor f ("fc", "fc1", "f0",
 * "fc2", "pico", "valle", "fase 0"): las que recorre [M]. Devuelve cuántas. */
int    traz_marcas(const BodeRes *r, double *f, const char **nom, int max);
/* Lectura exacta en f (bode_en; recuerda las cuatro últimas del mismo
 * barrido): dB, |H| y la fase en grados desenrollada como la curva. 0 si H
 * es singular ahí (resonancia ideal). */
int    traz_lee(const BodeReg *g, double f, double *db, double *mod, double *fase);
/* Puntos de la malla dentro de la ventana */
int    traz_en_vista(const Trazador *z, const BodeReg *g);
/* Ancho mínimo de la ventana con [+], en décadas: 4 intervalos de la malla */
double traz_ancho_min(const BodeReg *g);
/* Las veces que se rehicieron las filas de una curva: la caché del dibujo
 * las guarda mientras no cambien el barrido acabado, la ventana ni la
 * escala (el arnés comprueba que un cursor no las rehace) */
int    traz_filas_hechas(void);

#endif /* TRAZADOR_H */
