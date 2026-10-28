/* ============================================================================
 *  circuito.h  -  El modelo: rejilla + elementos + magnitudes
 *
 *  El circuito se dibuja sobre una rejilla de REJ_COLS x REJ_FILAS puntos.
 *  Cada resistencia, fuente o cable ocupa UN segmento entre dos puntos
 *  vecinos. Se dibuja como en el enunciado y el dibujo sale limpio siempre:
 *  no hace falta un algoritmo que disponga un grafo en pantalla.
 *
 *  Convenio que no se ve en el codigo: todo punto donde se juntan segmentos
 *  es una union. No hay cables que se cruzan sin tocarse.
 *
 *  Signos:
 *    - cada elemento va de su extremo a a su extremo b; I es la corriente que
 *      lo recorre de a hacia b, y V = phi(a) - phi(b);
 *    - en una resistencia eso es el criterio pasivo: V = R*I;
 *    - en una fuente, b es el borne + y E = phi(+) - phi(-); con I > 0 la
 *      corriente sale por el + y la fuente ENTREGA potencia E*I.
 *  Los datos de V e I se escriben en MAGNITUD, como los da un enunciado; el
 *  signo lo decide el motor (resuelve.c).
 * ========================================================================= */
#ifndef CIRCUITO_H
#define CIRCUITO_H

#define REJ_COLS  10
#define REJ_FILAS  7
#define REJ_PUNTOS (REJ_COLS * REJ_FILAS)
#define MAX_EL    32               /* la mascara de un paso es de 32 bits */

/* Contenido de un segmento */
#define SEG_VACIO 0
#define SEG_CABLE 1
#define SEG_EL0   2                /* SEG_EL0 + k = elemento k            */

/* Direcciones de la gramatica "accion + flecha" del editor */
enum { DIR_DER = 0, DIR_ABA, DIR_IZQ, DIR_ARR };

#include "tipos.h"                 /* TipoEl, TIPOS[] */

typedef enum {
    M_LIBRE = 0,   /* ni se sabe ni se pide: se calcula igual           */
    M_DATO,        /* valor conocido                                    */
    M_BUSCAR       /* incognita pedida: sale destacada en resultados    */
} EstadoMag;

/* Una magnitud. En corriente continua solo cuentan v y est; el ángulo (en
 * grados) y su estado son para la alterna: un fasor es v∠ang, y un módulo sin
 * ángulo (est_ang = M_LIBRE) es una lectura de instrumento. */
typedef struct {
    double v;
    double ang;
    unsigned char est;             /* EstadoMag del módulo             */
    unsigned char est_ang;         /* EstadoMag del ángulo             */
} Mag;

/* El modelo se diseñó una vez para toda la hoja de ruta (docs/HOJA_DE_RUTA.md),
 * y el archivo (CESP) lo guarda entero: así no hay que migrar archivos en
 * cada etapa. Lo que un tipo no usa se queda a cero. */
typedef struct {
    unsigned char tipo;            /* TipoEl                           */
    unsigned char xa, ya, xb, yb;  /* extremos en la rejilla (a -> b)  */
    char  etiq[6];                 /* "R1", "E1"                       */
    Mag   val;                     /* R de una resistencia, E de una fuente */
    Mag   V, I;
    Mag   aux;                     /* parámetro extra del tipo: r interna,
                                      R de un instrumento, ganancia...  */
    unsigned char variante;        /* E, J: forma (ONDA_*); V, A: modo (MODO_*);
                                      S: estado al empezar (INTER_*)    */
    signed char ref;               /* otro elemento (el que controla una
                                      fuente dependiente, la pareja de un
                                      transformador); -1 si ninguno     */
    /* Etapa 2: parámetros del emulador, Q_P1..Q_P6 (tipos.h). Vacío = por
     * omisión; nunca M_BUSCAR; su ángulo no se usa (0). */
    Mag   par[NPAR];
    /* Etapa 4: la 3.ª pata (AO: salida; BJT: B; MOSFET: G); 0 y 0 en los de dos bornes */
    unsigned char xc, yc;
} Elemento;

/* Sonda del osciloscopio: la tensión de un punto (respecto a la tierra), o
 * la corriente o la tensión de un elemento. No es un elemento: no gasta el
 * tope de 32 ni aparece en la red. */
enum { SONDA_NO = 0, SONDA_NODO, SONDA_I, SONDA_V };
typedef struct {
    unsigned char tipo;            /* SONDA_*                               */
    signed char x, y;              /* NODO: el punto; si no, -1             */
    signed char el;                /* I, V: el elemento; si no, -1          */
    signed char sg;                /* I, V: +1 = de a hacia b (I) o
                                      phi(a) - phi(b) (V); -1 al revés; 0 en NO y NODO */
} Sonda;                           /* signed char SIEMPRE: en el ARM char no tiene signo */

/* Convenio de la alterna (Circuito.conv): por omisión, valores eficaces y
 * coseno, que es lo que marcan los instrumentos. */
#define CONV_PICO 1                /* amplitudes de pico, no eficaces  */
#define CONV_SENO 2                /* v(t) = Vm sen(wt + fase)         */

typedef struct {
    unsigned char h[REJ_FILAS][REJ_COLS - 1];   /* (x,y)-(x+1,y) */
    unsigned char v[REJ_FILAS - 1][REJ_COLS];   /* (x,y)-(x,y+1) */
    Elemento el[MAX_EL];
    int      nel;
    char     nombre[40];                        /* 39: los títulos de la biblioteca (los
                                                   que se escriben, 23); el archivo guarda
                                                   23 en la cabecera y el resto, si lo hay,
                                                   en la sección 3 del bloque global */
    signed char tierra_x, tierra_y;             /* punto de la tierra, -1  */
    signed char term_x[2], term_y[2];           /* terminales A y B, -1    */
    unsigned char conv;                         /* CONV_*                  */
    Mag      frec;                              /* frecuencia de la alterna */
    /* Etapa 2 */
    Sonda    sonda[2];                          /* canal 1 y canal 2       */
    Mag      t_fin, paso;                       /* del emulador; vacío = automático */
} Circuito;                                     /* ~9 KB: nunca en la pila  */

/* La magnitud q (Q_VAL, Q_V, Q_I, Q_AUX) de un elemento */
Mag       *circ_mag(Elemento *e, int q);
const Mag *circ_mag_c(const Elemento *e, int q);

void circ_init(Circuito *c, const char *nombre);

/* Punto vecino en la direccion d. 0 si se sale de la rejilla. */
int  circ_vecino(int x, int y, int d, int *nx, int *ny);

/* Contenido del segmento que sale de (x,y) en la direccion d; -1 si no hay
 * segmento (borde de la rejilla). */
int  circ_seg(const Circuito *c, int x, int y, int d);

/* Tira un cable de (x,y) en la direccion d. Si el segmento tenia un
 * elemento, lo sustituye. 1 si se pudo. */
int  circ_cable(Circuito *c, int x, int y, int d);

/* Coloca un elemento de (x,y) hacia d: el extremo a queda en (x,y) y el b en
 * el vecino (en una fuente, b es el +). Devuelve su indice o -1. */
int  circ_elemento(Circuito *c, TipoEl t, int x, int y, int d);

/* Vacia el segmento. 1 si habia algo. */
int  circ_quitar(Circuito *c, int x, int y, int d);
/* Quita el elemento k entero (todos sus segmentos; [ctrl][X] del editor),
 * como circ_quitar en uno de ellos: las referencias y las sondas lo siguen */
void circ_borra(Circuito *c, int k);

/* Invierte a y b (en una fuente, cambia la polaridad). */
void circ_voltear(Circuito *c, int k);

/* Cuantos segmentos llegan al punto (para dibujar las uniones). */
int  circ_grado(const Circuito *c, int x, int y);

/* Un double finito (ni NaN ni ±Inf), sin isfinite: Inf - Inf es NaN */
#define FINITO(x) ((x) - (x) == 0.0)

/* Las leyes de CC en UN sitio (la calculadora, red.c, proced.c) */
int  circ_abierto_cc(const Elemento *e);  /* I = 0: voltímetro ideal, C, S abierto al empezar */
int  circ_corto_cc(const Elemento *e);    /* V = 0: amperímetro ideal, L, S cerrado al empezar */
int  circ_val_en_ley(const Elemento *e);  /* su valor entra en su ley (R, E, J, instrumento con R) */
int  circ_pide(const Circuito *c);        /* algún [?!] en Q_VAL..Q_AUX */
int  circ_dinamico(const Circuito *c);    /* C/L/S, E/J no continua, sonda, V/A en modo no CC */

/* Forma de una E o J con los parámetros de ondas.c. 0 si falta un dato (en
 * *q la magnitud que falta). El vacío toma el valor por omisión. */
int  circ_onda(const Elemento *e, int *forma, double p[ONDA_PAR], int *q);

/* Sondas */
int  circ_sonda_nodo(const Circuito *c, int x, int y);    /* canal con NODO ahí, o -1 */
int  circ_sonda_elem(const Circuito *c, int k);           /* canal con I o V de k, o -1 */
int  circ_sonda_pon_nodo(Circuito *c, int x, int y, int *canal);  /* pone o quita: SONDA_NODO / SONDA_NO */
int  circ_sonda_ciclo(Circuito *c, int k, int sg, int *canal);    /* nada -> I -> V -> nada */
void circ_sondas_limpia(Circuito *c);     /* quita las NODO en puntos sin nada */

/* ---- Etapa 3: la alterna ---- */
/* Alguna E o J en FORMA_FASOR, o alguna Z: [=] va a la alterna */
int  circ_alterna(const Circuito *c);
/* El campo que se ve de la magnitud q de ESE elemento (con su variante) */
const CampoInfo *circ_campo(const Elemento *e, int q);
/* Como circ_onda, con el circuito: la forma alterna sale como ONDA_SENO con
 * p = {√2·|E|, frec, φ + 90, 0, 0} (φ = 0 si el ángulo está vacío). Falta el
 * valor: 0 y *q = Q_VAL; falta frec: 0 y *q = Q_FCIRC. Con c = NULL, circ_onda. */
int  circ_onda_c(const Circuito *c, const Elemento *e, int *forma, double p[ONDA_PAR], int *q);
/* Quita lo que solo vale en alterna: los ángulos de los campos con ángulo,
 * el [?!] del valor de C y L, P y fp del W, frec [?!]. 1 si quitó algo. */
int  circ_quita_ca(Circuito *c);

/* Nombre generico de la magnitud principal: "R" o "E". */
const char *circ_nom_val(const Elemento *e);

/* ---- Etapa 4: componentes de 3 patas y fuentes controladas ----
 * Un componente de 3 patas (AO, BJT, MOSFET) ocupa una CAJA: la envolvente de
 * sus patas. Todos sus segmentos valen SEG_EL0 + k; las patas son puntos del
 * borde y los demás puntos de la caja son interiores (bloqueados). */
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
 * bornes que no es j, con vuelta, desde el control que j tiene ahora; desde -1, el primero
 * o el último. -1 si no hay ninguno (0 es un índice válido: E1). No cambia nada: la ficha
 * pone ref con lo que devuelve. */
int  circ_control_sig(const Circuito *c, int j, int dir);
/* La regla de admisión del control de la controlada k (D8), la misma en los dos motores:
 * CTL_OK, o por qué no vale. La usan resolver (SOL_CTL), la ficha y la línea de estado;
 * valida (sim.c) aplica la misma lista sobre la SimRed. */
enum { CTL_OK = 0, CTL_FALTA, CTL_SI_MISMA, CTL_3PATAS, CTL_I_NO };
int  circ_ctl_motivo(const Circuito *c, int k);

/* 1 si el circuito es coherente: cada elemento está en su segmento, cada
 * segmento apunta a un elemento que existe y está en él, los extremos son
 * vecinos y los nombres no se repiten. Lo usa archivo.c antes de aceptar un
 * circuito leído de disco. */
int  circ_valido(const Circuito *c);

/* 1 si los dos circuitos son iguales campo a campo (sin memcmp: el relleno de
 * las estructuras no cuenta), nombre incluido. El editor la usa para no gastar
 * un nivel de deshacer con una ficha que solo se miró. */
int  circ_igual(const Circuito *a, const Circuito *b);

#endif /* CIRCUITO_H */
